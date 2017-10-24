#if !defined(LIBFFI_TARGET_H)
#define LIBFFI_TARGET_H
#if (defined(POWERPC) && defined(__powerpc64__)) || (defined(POWERPC_DARWIN) && defined(__ppc64__))
#define POWERPC64
#endif
#if !defined(LIBFFI_ASM)
typedef unsigned long ffi_arg;
typedef signed long ffi_sarg;
typedef enum ffi_abi {
FFI_FIRST_ABI = 0,
#if defined(POWERPC)
FFI_SYSV,
FFI_GCC_SYSV,
FFI_LINUX64,
#if defined(POWERPC64)
FFI_DEFAULT_ABI = FFI_LINUX64,
#else
FFI_DEFAULT_ABI = FFI_GCC_SYSV,
#endif
#endif
#if defined(POWERPC_AIX)
FFI_AIX,
FFI_DARWIN,
FFI_DEFAULT_ABI = FFI_AIX,
#endif
#if defined(POWERPC_DARWIN)
FFI_AIX,
FFI_DARWIN,
FFI_DEFAULT_ABI = FFI_DARWIN,
#endif
#if defined(POWERPC_FREEBSD)
FFI_SYSV,
FFI_GCC_SYSV,
FFI_LINUX64,
FFI_DEFAULT_ABI = FFI_SYSV,
#endif
FFI_LAST_ABI = FFI_DEFAULT_ABI + 1
} ffi_abi;
#endif
#define FFI_CLOSURES 1
#define FFI_NATIVE_RAW_API 0
#define FFI_SYSV_TYPE_SMALL_STRUCT (FFI_TYPE_LAST)
#if defined(POWERPC64)
#define FFI_TRAMPOLINE_SIZE 48
#elif defined(POWERPC_AIX)
#define FFI_TRAMPOLINE_SIZE 24
#else
#define FFI_TRAMPOLINE_SIZE 40
#endif
#if !defined(LIBFFI_ASM)
#if defined(POWERPC_DARWIN) || defined(POWERPC_AIX)
typedef struct ffi_aix_trampoline_struct {
void* code_pointer;
void* toc;
void* static_chain;
} ffi_aix_trampoline_struct;
#endif
#endif
#endif
