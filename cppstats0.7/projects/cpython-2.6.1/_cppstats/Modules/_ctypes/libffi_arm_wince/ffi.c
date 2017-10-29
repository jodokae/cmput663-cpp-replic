#include <ffi.h>
#include <ffi_common.h>
#include <stdlib.h>
#if defined(_WIN32_WCE)
#pragma warning (disable : 4142)
#include <windows.h>
#endif
void ffi_prep_args(char *stack, extended_cif *ecif)
{
register unsigned int i;
register void **p_argv;
register char *argp;
register ffi_type **p_arg;
argp = stack;
if ( ecif->cif->rtype->type == FFI_TYPE_STRUCT ) {
*(void **) argp = ecif->rvalue;
argp += 4;
}
p_argv = ecif->avalue;
for (i = ecif->cif->nargs, p_arg = ecif->cif->arg_types;
(i != 0);
i--, p_arg++) {
size_t z;
size_t argalign = (*p_arg)->alignment;
#if defined(_WIN32_WCE)
if (argalign > 4)
argalign = 4;
#endif
if ((argalign - 1) & (unsigned) argp) {
argp = (char *) ALIGN(argp, argalign);
}
z = (*p_arg)->size;
if (z < sizeof(int)) {
z = sizeof(int);
switch ((*p_arg)->type) {
case FFI_TYPE_SINT8:
*(signed int *) argp = (signed int)*(SINT8 *)(* p_argv);
break;
case FFI_TYPE_UINT8:
*(unsigned int *) argp = (unsigned int)*(UINT8 *)(* p_argv);
break;
case FFI_TYPE_SINT16:
*(signed int *) argp = (signed int)*(SINT16 *)(* p_argv);
break;
case FFI_TYPE_UINT16:
*(unsigned int *) argp = (unsigned int)*(UINT16 *)(* p_argv);
break;
case FFI_TYPE_STRUCT:
memcpy(argp, *p_argv, z);
break;
default:
FFI_ASSERT(0);
}
} else if (z == sizeof(int)) {
*(unsigned int *) argp = (unsigned int)*(UINT32 *)(* p_argv);
} else {
memcpy(argp, *p_argv, z);
}
p_argv++;
argp += z;
}
return;
}
ffi_status ffi_prep_cif_machdep(ffi_cif *cif) {
switch (cif->rtype->type) {
case FFI_TYPE_VOID:
case FFI_TYPE_STRUCT:
case FFI_TYPE_FLOAT:
case FFI_TYPE_DOUBLE:
case FFI_TYPE_SINT64:
cif->flags = (unsigned) cif->rtype->type;
break;
case FFI_TYPE_UINT64:
cif->flags = FFI_TYPE_SINT64;
break;
default:
cif->flags = FFI_TYPE_INT;
break;
}
return FFI_OK;
}
extern void ffi_call_SYSV(void (*)(char *, extended_cif *),
extended_cif *,
unsigned, unsigned,
unsigned *,
void (*fn)());
int ffi_call( ffi_cif *cif,
void (*fn)(),
void *rvalue,
void **avalue) {
extended_cif ecif;
ecif.cif = cif;
ecif.avalue = avalue;
if ((rvalue == NULL) &&
(cif->rtype->type == FFI_TYPE_STRUCT)) {
ecif.rvalue = alloca(cif->rtype->size);
} else
ecif.rvalue = rvalue;
switch (cif->abi) {
case FFI_SYSV:
ffi_call_SYSV(ffi_prep_args, &ecif, cif->bytes,
cif->flags, ecif.rvalue, fn);
break;
default:
FFI_ASSERT(0);
break;
}
return 0;
}
static void ffi_prep_incoming_args_SYSV (char *stack, void **ret,
void** args, ffi_cif* cif);
unsigned int
ffi_closure_SYSV_inner (ffi_closure *closure, char *in_args, void *rvalue) {
ffi_cif *cif = closure->cif;
void **out_args;
out_args = (void **) alloca(cif->nargs * sizeof (void *));
ffi_prep_incoming_args_SYSV(in_args, &rvalue, out_args, cif);
(closure->fun)(cif, rvalue, out_args, closure->user_data);
return cif->flags;
}
static void
ffi_prep_incoming_args_SYSV(char *stack, void **rvalue,
void **avalue, ffi_cif *cif)
{
unsigned int i;
void **p_argv;
char *argp;
ffi_type **p_arg;
argp = stack;
if ( cif->rtype->type == FFI_TYPE_STRUCT ) {
*rvalue = *(void **) argp;
argp += 4;
}
p_argv = avalue;
for (i = cif->nargs, p_arg = cif->arg_types; (i != 0); i--, p_arg++) {
size_t z;
size_t argalign = (*p_arg)->alignment;
#if defined(_WIN32_WCE)
if (argalign > 4)
argalign = 4;
#endif
if ((argalign - 1) & (unsigned) argp) {
argp = (char *) ALIGN(argp, argalign);
}
z = (*p_arg)->size;
if (z < sizeof(int))
z = sizeof(int);
*p_argv = (void*) argp;
p_argv++;
argp += z;
}
}
#define FFI_INIT_TRAMPOLINE(TRAMP,FUN) { unsigned int *__tramp = (unsigned int *)(TRAMP); __tramp[0] = 0xe24fc008; __tramp[1] = 0xe51ff004; __tramp[2] = (unsigned int)(FUN); }
void ffi_closure_SYSV(void);
ffi_status
ffi_prep_closure (ffi_closure* closure,
ffi_cif* cif,
void (*fun)(ffi_cif*,void*,void**,void*),
void *user_data) {
FFI_ASSERT (cif->abi == FFI_SYSV);
FFI_INIT_TRAMPOLINE (&closure->tramp[0], &ffi_closure_SYSV);
closure->cif = cif;
closure->user_data = user_data;
closure->fun = fun;
#if defined(_WIN32_WCE)
FlushInstructionCache(GetCurrentProcess(), 0, 0);
#endif
return FFI_OK;
}