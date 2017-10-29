#if defined(__x86_64__)
#include <ffi.h>
#include <ffi_common.h>
#include <stdlib.h>
#include <stdarg.h>
#define MAX_GPR_REGS 6
#define MAX_SSE_REGS 8
typedef struct RegisterArgs {
UINT64 gpr[MAX_GPR_REGS];
__int128_t sse[MAX_SSE_REGS];
} RegisterArgs;
extern void
ffi_call_unix64(
void* args,
unsigned long bytes,
unsigned flags,
void* raddr,
void (*fnaddr)(),
unsigned ssecount);
enum x86_64_reg_class {
X86_64_NO_CLASS,
X86_64_INTEGER_CLASS,
X86_64_INTEGERSI_CLASS,
X86_64_SSE_CLASS,
X86_64_SSESF_CLASS,
X86_64_SSEDF_CLASS,
X86_64_SSEUP_CLASS,
X86_64_X87_CLASS,
X86_64_X87UP_CLASS,
X86_64_COMPLEX_X87_CLASS,
X86_64_MEMORY_CLASS
};
#define MAX_CLASSES 4
#define SSE_CLASS_P(X) ((X) >= X86_64_SSE_CLASS && X <= X86_64_SSEUP_CLASS)
static enum x86_64_reg_class
merge_classes(
enum x86_64_reg_class class1,
enum x86_64_reg_class class2) {
if (class1 == class2)
return class1;
if (class1 == X86_64_NO_CLASS)
return class2;
if (class2 == X86_64_NO_CLASS)
return class1;
if (class1 == X86_64_MEMORY_CLASS || class2 == X86_64_MEMORY_CLASS)
return X86_64_MEMORY_CLASS;
if ((class1 == X86_64_INTEGERSI_CLASS && class2 == X86_64_SSESF_CLASS)
|| (class2 == X86_64_INTEGERSI_CLASS && class1 == X86_64_SSESF_CLASS))
return X86_64_INTEGERSI_CLASS;
if (class1 == X86_64_INTEGER_CLASS || class1 == X86_64_INTEGERSI_CLASS
|| class2 == X86_64_INTEGER_CLASS || class2 == X86_64_INTEGERSI_CLASS)
return X86_64_INTEGER_CLASS;
if (class1 == X86_64_X87_CLASS
|| class1 == X86_64_X87UP_CLASS
|| class1 == X86_64_COMPLEX_X87_CLASS
|| class2 == X86_64_X87_CLASS
|| class2 == X86_64_X87UP_CLASS
|| class2 == X86_64_COMPLEX_X87_CLASS)
return X86_64_MEMORY_CLASS;
return X86_64_SSE_CLASS;
}
static int
classify_argument(
ffi_type* type,
enum x86_64_reg_class classes[],
size_t byte_offset) {
switch (type->type) {
case FFI_TYPE_UINT8:
case FFI_TYPE_SINT8:
case FFI_TYPE_UINT16:
case FFI_TYPE_SINT16:
case FFI_TYPE_UINT32:
case FFI_TYPE_SINT32:
case FFI_TYPE_UINT64:
case FFI_TYPE_SINT64:
case FFI_TYPE_POINTER:
if (byte_offset + type->size <= 4)
classes[0] = X86_64_INTEGERSI_CLASS;
else
classes[0] = X86_64_INTEGER_CLASS;
return 1;
case FFI_TYPE_FLOAT:
if (byte_offset == 0)
classes[0] = X86_64_SSESF_CLASS;
else
classes[0] = X86_64_SSE_CLASS;
return 1;
case FFI_TYPE_DOUBLE:
classes[0] = X86_64_SSEDF_CLASS;
return 1;
case FFI_TYPE_LONGDOUBLE:
classes[0] = X86_64_X87_CLASS;
classes[1] = X86_64_X87UP_CLASS;
return 2;
case FFI_TYPE_STRUCT: {
ffi_type** ptr;
int i;
enum x86_64_reg_class subclasses[MAX_CLASSES];
const int UNITS_PER_WORD = 8;
int words =
(type->size + UNITS_PER_WORD - 1) / UNITS_PER_WORD;
if (type->size > 16)
return 0;
for (i = 0; i < words; i++)
classes[i] = X86_64_NO_CLASS;
for (ptr = type->elements; *ptr != NULL; ptr++) {
byte_offset = ALIGN(byte_offset, (*ptr)->alignment);
int num = classify_argument(*ptr, subclasses, byte_offset % 8);
if (num == 0)
return 0;
int pos = byte_offset / 8;
for (i = 0; i < num; i++) {
classes[i + pos] =
merge_classes(subclasses[i], classes[i + pos]);
}
byte_offset += (*ptr)->size;
}
for (i = 0; i < words; i++) {
if (classes[i] == X86_64_MEMORY_CLASS)
return 0;
if (classes[i] == X86_64_SSEUP_CLASS
&& (i == 0 || classes[i - 1] != X86_64_SSE_CLASS))
classes[i] = X86_64_SSE_CLASS;
if (classes[i] == X86_64_X87UP_CLASS
&& (i == 0 || classes[i - 1] != X86_64_X87_CLASS))
classes[i] = X86_64_SSE_CLASS;
}
return words;
}
default:
FFI_ASSERT(0);
}
return 0;
}
static int
examine_argument(
ffi_type* type,
enum x86_64_reg_class classes[MAX_CLASSES],
_Bool in_return,
int* pngpr,
int* pnsse) {
int n = classify_argument(type, classes, 0);
int ngpr = 0;
int nsse = 0;
int i;
if (n == 0)
return 0;
for (i = 0; i < n; ++i) {
switch (classes[i]) {
case X86_64_INTEGER_CLASS:
case X86_64_INTEGERSI_CLASS:
ngpr++;
break;
case X86_64_SSE_CLASS:
case X86_64_SSESF_CLASS:
case X86_64_SSEDF_CLASS:
nsse++;
break;
case X86_64_NO_CLASS:
case X86_64_SSEUP_CLASS:
break;
case X86_64_X87_CLASS:
case X86_64_X87UP_CLASS:
case X86_64_COMPLEX_X87_CLASS:
return in_return != 0;
default:
abort();
}
}
*pngpr = ngpr;
*pnsse = nsse;
return n;
}
ffi_status
ffi_prep_cif_machdep(
ffi_cif* cif) {
int gprcount = 0;
int ssecount = 0;
int flags = cif->rtype->type;
int i, avn, n, ngpr, nsse;
enum x86_64_reg_class classes[MAX_CLASSES];
size_t bytes;
if (flags != FFI_TYPE_VOID) {
n = examine_argument (cif->rtype, classes, 1, &ngpr, &nsse);
if (n == 0) {
gprcount++;
flags = FFI_TYPE_VOID;
} else if (flags == FFI_TYPE_STRUCT) {
_Bool sse0 = SSE_CLASS_P(classes[0]);
_Bool sse1 = n == 2 && SSE_CLASS_P(classes[1]);
if (sse0 && !sse1)
flags |= 1 << 8;
else if (!sse0 && sse1)
flags |= 1 << 9;
else if (sse0 && sse1)
flags |= 1 << 10;
flags |= cif->rtype->size << 12;
}
}
for (bytes = 0, i = 0, avn = cif->nargs; i < avn; i++) {
if (examine_argument(cif->arg_types[i], classes, 0, &ngpr, &nsse) == 0
|| gprcount + ngpr > MAX_GPR_REGS
|| ssecount + nsse > MAX_SSE_REGS) {
long align = cif->arg_types[i]->alignment;
if (align < 8)
align = 8;
bytes = ALIGN(bytes, align);
bytes += cif->arg_types[i]->size;
} else {
gprcount += ngpr;
ssecount += nsse;
}
}
if (ssecount)
flags |= 1 << 11;
cif->flags = flags;
cif->bytes = bytes;
return FFI_OK;
}
void
ffi_call(
ffi_cif* cif,
void (*fn)(),
void* rvalue,
void** avalue) {
enum x86_64_reg_class classes[MAX_CLASSES];
char* stack;
char* argp;
ffi_type** arg_types;
int gprcount, ssecount, ngpr, nsse, i, avn;
_Bool ret_in_memory;
RegisterArgs* reg_args;
FFI_ASSERT(cif->abi == FFI_UNIX64);
ret_in_memory = (cif->rtype->type == FFI_TYPE_STRUCT
&& (cif->flags & 0xff) == FFI_TYPE_VOID);
if (rvalue == NULL && ret_in_memory)
rvalue = alloca (cif->rtype->size);
stack = alloca(sizeof(RegisterArgs) + cif->bytes + 4 * 8);
reg_args = (RegisterArgs*)stack;
argp = stack + sizeof(RegisterArgs);
gprcount = ssecount = 0;
if (ret_in_memory)
reg_args->gpr[gprcount++] = (long) rvalue;
avn = cif->nargs;
arg_types = cif->arg_types;
for (i = 0; i < avn; ++i) {
size_t size = arg_types[i]->size;
int n;
n = examine_argument (arg_types[i], classes, 0, &ngpr, &nsse);
if (n == 0
|| gprcount + ngpr > MAX_GPR_REGS
|| ssecount + nsse > MAX_SSE_REGS) {
long align = arg_types[i]->alignment;
if (align < 8)
align = 8;
argp = (void *) ALIGN (argp, align);
memcpy (argp, avalue[i], size);
argp += size;
} else {
char *a = (char *) avalue[i];
int j;
for (j = 0; j < n; j++, a += 8, size -= 8) {
switch (classes[j]) {
case X86_64_INTEGER_CLASS:
case X86_64_INTEGERSI_CLASS:
reg_args->gpr[gprcount] = 0;
memcpy (&reg_args->gpr[gprcount], a, size < 8 ? size : 8);
gprcount++;
break;
case X86_64_SSE_CLASS:
case X86_64_SSEDF_CLASS:
reg_args->sse[ssecount++] = *(UINT64 *) a;
break;
case X86_64_SSESF_CLASS:
reg_args->sse[ssecount++] = *(UINT32 *) a;
break;
default:
abort();
}
}
}
}
ffi_call_unix64 (stack, cif->bytes + sizeof(RegisterArgs),
cif->flags, rvalue, fn, ssecount);
}
extern void ffi_closure_unix64(void);
ffi_status
ffi_prep_closure(
ffi_closure* closure,
ffi_cif* cif,
void (*fun)(ffi_cif*, void*, void**, void*),
void* user_data) {
if (cif->abi != FFI_UNIX64)
return FFI_BAD_ABI;
volatile unsigned short* tramp =
(volatile unsigned short*)&closure->tramp[0];
tramp[0] = 0xbb49;
*(void* volatile*)&tramp[1] = ffi_closure_unix64;
tramp[5] = 0xba49;
*(void* volatile*)&tramp[6] = closure;
tramp[10] = cif->flags & (1 << 11) ? 0x49f9 : 0x49f8;
tramp[11] = 0xe3ff;
closure->cif = cif;
closure->fun = fun;
closure->user_data = user_data;
return FFI_OK;
}
int
ffi_closure_unix64_inner(
ffi_closure* closure,
void* rvalue,
RegisterArgs* reg_args,
char* argp) {
ffi_cif* cif = closure->cif;
void** avalue = alloca(cif->nargs * sizeof(void *));
ffi_type** arg_types;
long i, avn;
int gprcount = 0;
int ssecount = 0;
int ngpr, nsse;
int ret;
ret = cif->rtype->type;
if (ret != FFI_TYPE_VOID) {
enum x86_64_reg_class classes[MAX_CLASSES];
int n = examine_argument (cif->rtype, classes, 1, &ngpr, &nsse);
if (n == 0) {
rvalue = (void *) reg_args->gpr[gprcount++];
ret = FFI_TYPE_VOID;
} else if (ret == FFI_TYPE_STRUCT && n == 2) {
_Bool sse0 = SSE_CLASS_P (classes[0]);
_Bool sse1 = SSE_CLASS_P (classes[1]);
if (!sse0 && sse1)
ret |= 1 << 8;
else if (sse0 && !sse1)
ret |= 1 << 9;
}
}
avn = cif->nargs;
arg_types = cif->arg_types;
for (i = 0; i < avn; ++i) {
enum x86_64_reg_class classes[MAX_CLASSES];
int n;
n = examine_argument (arg_types[i], classes, 0, &ngpr, &nsse);
if (n == 0
|| gprcount + ngpr > MAX_GPR_REGS
|| ssecount + nsse > MAX_SSE_REGS) {
long align = arg_types[i]->alignment;
if (align < 8)
align = 8;
argp = (void *) ALIGN (argp, align);
avalue[i] = argp;
argp += arg_types[i]->size;
}
#if !defined(X86_DARWIN)
else if (n == 1 || (n == 2 &&
SSE_CLASS_P (classes[0]) == SSE_CLASS_P (classes[1]))) {
if (SSE_CLASS_P (classes[0])) {
avalue[i] = &reg_args->sse[ssecount];
ssecount += n;
} else {
avalue[i] = &reg_args->gpr[gprcount];
gprcount += n;
}
}
#endif
else {
char *a = alloca (16);
int j;
avalue[i] = a;
for (j = 0; j < n; j++, a += 8) {
if (SSE_CLASS_P (classes[j]))
memcpy (a, &reg_args->sse[ssecount++], 8);
else
memcpy (a, &reg_args->gpr[gprcount++], 8);
}
}
}
closure->fun (cif, rvalue, avalue, closure->user_data);
return ret;
}
#endif