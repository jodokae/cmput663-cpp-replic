#if defined(__i386__)
#include <ffi.h>
#include <ffi_common.h>
#include <stdlib.h>
static inline int
retval_on_stack(
ffi_type* tp) {
if (tp->type == FFI_TYPE_STRUCT) {
if (tp->size > 8)
return 1;
switch (tp->size) {
case 1:
case 2:
case 4:
case 8:
return 0;
default:
return 1;
}
}
return 0;
}
extern void ffi_prep_args(char*, extended_cif*);
void
ffi_prep_args(
char* stack,
extended_cif* ecif)
{
register unsigned int i;
register void** p_argv = ecif->avalue;
register char* argp = stack;
register ffi_type** p_arg;
if (retval_on_stack(ecif->cif->rtype)) {
*(void**)argp = ecif->rvalue;
argp += 4;
}
p_arg = ecif->cif->arg_types;
for (i = ecif->cif->nargs; i > 0; i--, p_arg++, p_argv++) {
size_t z = (*p_arg)->size;
if ((sizeof(int) - 1) & (unsigned)argp)
argp = (char*)ALIGN(argp, sizeof(int));
if (z < sizeof(int)) {
z = sizeof(int);
switch ((*p_arg)->type) {
case FFI_TYPE_SINT8:
*(signed int*)argp = (signed int)*(SINT8*)(*p_argv);
break;
case FFI_TYPE_UINT8:
*(unsigned int*)argp = (unsigned int)*(UINT8*)(*p_argv);
break;
case FFI_TYPE_SINT16:
*(signed int*)argp = (signed int)*(SINT16*)(*p_argv);
break;
case FFI_TYPE_UINT16:
*(unsigned int*)argp = (unsigned int)*(UINT16*)(*p_argv);
break;
case FFI_TYPE_SINT32:
*(signed int*)argp = (signed int)*(SINT32*)(*p_argv);
break;
case FFI_TYPE_UINT32:
*(unsigned int*)argp = (unsigned int)*(UINT32*)(*p_argv);
break;
case FFI_TYPE_STRUCT:
*(unsigned int*)argp = (unsigned int)*(UINT32*)(*p_argv);
break;
default:
FFI_ASSERT(0);
break;
}
} else
memcpy(argp, *p_argv, z);
argp += z;
}
}
ffi_status
ffi_prep_cif_machdep(
ffi_cif* cif) {
switch (cif->rtype->type) {
#if !defined(X86_WIN32) && !defined(X86_DARWIN)
case FFI_TYPE_STRUCT:
#endif
case FFI_TYPE_VOID:
case FFI_TYPE_SINT64:
case FFI_TYPE_FLOAT:
case FFI_TYPE_DOUBLE:
case FFI_TYPE_LONGDOUBLE:
cif->flags = (unsigned)cif->rtype->type;
break;
case FFI_TYPE_UINT64:
cif->flags = FFI_TYPE_SINT64;
break;
#if defined(X86_WIN32) || defined(X86_DARWIN)
case FFI_TYPE_STRUCT:
switch (cif->rtype->size) {
case 1:
cif->flags = FFI_TYPE_SINT8;
break;
case 2:
cif->flags = FFI_TYPE_SINT16;
break;
case 4:
cif->flags = FFI_TYPE_INT;
break;
case 8:
cif->flags = FFI_TYPE_SINT64;
break;
default:
cif->flags = FFI_TYPE_STRUCT;
break;
}
break;
#endif
default:
cif->flags = FFI_TYPE_INT;
break;
}
cif->bytes = (cif->bytes + 15) & ~0xF;
return FFI_OK;
}
extern void
ffi_call_SYSV(
void (*)(char *, extended_cif *),
extended_cif* ,
unsigned ,
unsigned ,
unsigned* ,
void (*fn)(void));
#if defined(X86_WIN32)
extern void
ffi_call_STDCALL(
void (char *, extended_cif *),
extended_cif* ,
unsigned ,
unsigned ,
unsigned* ,
void (*fn)(void));
#endif
void
ffi_call(
ffi_cif* cif,
void (*fn)(void),
void* rvalue,
void** avalue) {
extended_cif ecif;
ecif.cif = cif;
ecif.avalue = avalue;
if ((rvalue == NULL) && retval_on_stack(cif->rtype)) {
ecif.rvalue = alloca(cif->rtype->size);
} else
ecif.rvalue = rvalue;
switch (cif->abi) {
case FFI_SYSV:
ffi_call_SYSV(ffi_prep_args, &ecif, cif->bytes,
cif->flags, ecif.rvalue, fn);
break;
#if defined(X86_WIN32)
case FFI_STDCALL:
ffi_call_STDCALL(ffi_prep_args, &ecif, cif->bytes,
cif->flags, ecif.rvalue, fn);
break;
#endif
default:
FFI_ASSERT(0);
break;
}
}
static void
ffi_closure_SYSV(
ffi_closure* closure) __attribute__((regparm(1)));
#if !FFI_NO_RAW_API
static void
ffi_closure_raw_SYSV(
ffi_raw_closure* closure) __attribute__((regparm(1)));
#endif
static inline
void
ffi_prep_incoming_args_SYSV(
char* stack,
void** rvalue,
void** avalue,
ffi_cif* cif)
{
register unsigned int i;
register void** p_argv = avalue;
register char* argp = stack;
register ffi_type** p_arg;
if (retval_on_stack(cif->rtype)) {
*rvalue = *(void**)argp;
argp += 4;
}
for (i = cif->nargs, p_arg = cif->arg_types; i > 0; i--, p_arg++, p_argv++) {
if ((sizeof(int) - 1) & (unsigned)argp)
argp = (char*)ALIGN(argp, sizeof(int));
*p_argv = (void*)argp;
argp += (*p_arg)->size;
}
}
__attribute__((regparm(1)))
static void
ffi_closure_SYSV(
ffi_closure* closure) {
long double res;
ffi_cif* cif = closure->cif;
void** arg_area = (void**)alloca(cif->nargs * sizeof(void*));
void* resp = (void*)&res;
void* args = __builtin_dwarf_cfa();
ffi_prep_incoming_args_SYSV(args, (void**)&resp, arg_area, cif);
(closure->fun)(cif, resp, arg_area, closure->user_data);
if (cif->flags == FFI_TYPE_INT)
asm("movl (%0),%%eax"
: : "r" (resp) : "eax");
else if (cif->flags == FFI_TYPE_FLOAT)
asm("flds (%0)"
: : "r" (resp) : "st");
else if (cif->flags == FFI_TYPE_DOUBLE)
asm("fldl (%0)"
: : "r" (resp) : "st", "st(1)");
else if (cif->flags == FFI_TYPE_LONGDOUBLE)
asm("fldt (%0)"
: : "r" (resp) : "st", "st(1)");
else if (cif->flags == FFI_TYPE_SINT64)
asm("movl 0(%0),%%eax;"
"movl 4(%0),%%edx"
: : "r" (resp)
: "eax", "edx");
#if defined(X86_WIN32) || defined(X86_DARWIN)
else if (cif->flags == FFI_TYPE_SINT8)
asm("movsbl (%0),%%eax"
: : "r" (resp) : "eax");
else if (cif->flags == FFI_TYPE_SINT16)
asm("movswl (%0),%%eax"
: : "r" (resp) : "eax");
#endif
else if (cif->flags == FFI_TYPE_STRUCT)
asm("lea -8(%ebp),%esp;"
"pop %esi;"
"pop %edi;"
"pop %ebp;"
"ret $4");
}
#define FFI_INIT_TRAMPOLINE(TRAMP, FUN, CTX) ({ unsigned char* __tramp = (unsigned char*)(TRAMP); unsigned int __fun = (unsigned int)(FUN); unsigned int __ctx = (unsigned int)(CTX); unsigned int __dis = __fun - ((unsigned int)__tramp + FFI_TRAMPOLINE_SIZE); *(unsigned char*)&__tramp[0] = 0xb8; *(unsigned int*)&__tramp[1] = __ctx; *(unsigned char*)&__tramp[5] = 0xe9; *(unsigned int*)&__tramp[6] = __dis; })
ffi_status
ffi_prep_closure(
ffi_closure* closure,
ffi_cif* cif,
void (*fun)(ffi_cif*,void*,void**,void*),
void* user_data) {
if (cif->abi != FFI_SYSV)
return FFI_BAD_ABI;
FFI_INIT_TRAMPOLINE(closure->tramp, &ffi_closure_SYSV, (void*)closure);
closure->cif = cif;
closure->user_data = user_data;
closure->fun = fun;
return FFI_OK;
}
#if !FFI_NO_RAW_API
__attribute__((regparm(1)))
static void
ffi_closure_raw_SYSV(
ffi_raw_closure* closure) {
long double res;
ffi_raw* raw_args = (ffi_raw*)__builtin_dwarf_cfa();
ffi_cif* cif = closure->cif;
unsigned short rtype = cif->flags;
void* resp = (void*)&res;
(closure->fun)(cif, resp, raw_args, closure->user_data);
if (rtype == FFI_TYPE_INT)
asm("movl (%0),%%eax"
: : "r" (resp) : "eax");
else if (rtype == FFI_TYPE_FLOAT)
asm("flds (%0)"
: : "r" (resp) : "st");
else if (rtype == FFI_TYPE_DOUBLE)
asm("fldl (%0)"
: : "r" (resp) : "st", "st(1)");
else if (rtype == FFI_TYPE_LONGDOUBLE)
asm("fldt (%0)"
: : "r" (resp) : "st", "st(1)");
else if (rtype == FFI_TYPE_SINT64)
asm("movl 0(%0),%%eax;"
"movl 4(%0),%%edx"
: : "r" (resp) : "eax", "edx");
}
ffi_status
ffi_prep_raw_closure(
ffi_raw_closure* closure,
ffi_cif* cif,
void (*fun)(ffi_cif*,void*,ffi_raw*,void*),
void* user_data) {
if (cif->abi != FFI_SYSV)
return FFI_BAD_ABI;
int i;
for (i = cif->nargs - 1; i >= 0; i--) {
FFI_ASSERT(cif->arg_types[i]->type != FFI_TYPE_STRUCT);
FFI_ASSERT(cif->arg_types[i]->type != FFI_TYPE_LONGDOUBLE);
}
FFI_INIT_TRAMPOLINE(closure->tramp, &ffi_closure_raw_SYSV, (void*)closure);
closure->cif = cif;
closure->user_data = user_data;
closure->fun = fun;
return FFI_OK;
}
static void
ffi_prep_args_raw(
char* stack,
extended_cif* ecif) {
memcpy(stack, ecif->avalue, ecif->cif->bytes);
}
#if defined(X86_WIN32)
extern void
ffi_call_STDCALL(
void (*)(char *, extended_cif *),
extended_cif* ,
unsigned ,
unsigned ,
unsigned* ,
void (*fn)());
#endif
void
ffi_raw_call(
ffi_cif* cif,
void (*fn)(),
void* rvalue,
ffi_raw* fake_avalue) {
extended_cif ecif;
void **avalue = (void **)fake_avalue;
ecif.cif = cif;
ecif.avalue = avalue;
if ((rvalue == NULL) && retval_on_stack(cif->rtype)) {
ecif.rvalue = alloca(cif->rtype->size);
} else
ecif.rvalue = rvalue;
switch (cif->abi) {
case FFI_SYSV:
ffi_call_SYSV(ffi_prep_args_raw, &ecif, cif->bytes,
cif->flags, ecif.rvalue, fn);
break;
#if defined(X86_WIN32)
case FFI_STDCALL:
ffi_call_STDCALL(ffi_prep_args_raw, &ecif, cif->bytes,
cif->flags, ecif.rvalue, fn);
break;
#endif
default:
FFI_ASSERT(0);
break;
}
}
#endif
#endif
