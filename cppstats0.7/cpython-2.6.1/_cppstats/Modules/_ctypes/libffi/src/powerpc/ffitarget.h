#if !defined(LIBFFI_TARGET_H)
#define LIBFFI_TARGET_H
#if defined (POWERPC) && defined (__powerpc64__)
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
FFI_LINUX,
FFI_LINUX_SOFT_FLOAT,
#if defined(POWERPC64)
FFI_DEFAULT_ABI = FFI_LINUX64,
#else
#if (!defined(__NO_FPRS__) && (__LDBL_MANT_DIG__ == 106))
FFI_DEFAULT_ABI = FFI_LINUX,
#else
#if defined(__NO_FPRS__)
FFI_DEFAULT_ABI = FFI_LINUX_SOFT_FLOAT,
#else
FFI_DEFAULT_ABI = FFI_GCC_SYSV,
#endif
#endif
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
FFI_LAST_ABI
} ffi_abi;
#endif
#define FFI_CLOSURES 1
#define FFI_NATIVE_RAW_API 0
#define FFI_TYPE_UINT128 (FFI_TYPE_LAST + 1)
#define FFI_SYSV_TYPE_SMALL_STRUCT (FFI_TYPE_LAST + 2)
#if defined(POWERPC64) || defined(POWERPC_AIX)
#define FFI_TRAMPOLINE_SIZE 24
#else
#define FFI_TRAMPOLINE_SIZE 40
#endif
#if !defined(LIBFFI_ASM)
#if defined(POWERPC_DARWIN) || defined(POWERPC_AIX)
struct ffi_aix_trampoline_struct {
void * code_pointer;
void * toc;
void * static_chain;
};
#endif
#endif
#endif
