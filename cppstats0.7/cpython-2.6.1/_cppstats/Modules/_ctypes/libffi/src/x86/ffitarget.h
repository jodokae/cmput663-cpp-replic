#if !defined(LIBFFI_TARGET_H)
#define LIBFFI_TARGET_H
#if defined (X86_64) && defined (__i386__)
#undef X86_64
#define X86
#endif
#if !defined(LIBFFI_ASM)
typedef unsigned long ffi_arg;
typedef signed long ffi_sarg;
typedef enum ffi_abi {
FFI_FIRST_ABI = 0,
#if defined(X86_WIN32)
FFI_SYSV,
FFI_STDCALL,
FFI_DEFAULT_ABI = FFI_SYSV,
#endif
#if !defined(X86_WIN32) && (defined(__i386__) || defined(__x86_64__))
FFI_SYSV,
FFI_UNIX64,
#if defined(__i386__)
FFI_DEFAULT_ABI = FFI_SYSV,
#else
FFI_DEFAULT_ABI = FFI_UNIX64,
#endif
#endif
FFI_LAST_ABI = FFI_DEFAULT_ABI + 1
} ffi_abi;
#endif
#define FFI_CLOSURES 1
#define FFI_TYPE_SMALL_STRUCT_1B (FFI_TYPE_LAST + 1)
#define FFI_TYPE_SMALL_STRUCT_2B (FFI_TYPE_LAST + 2)
#if defined (X86_64) || (defined (__x86_64__) && defined (X86_DARWIN))
#define FFI_TRAMPOLINE_SIZE 24
#define FFI_NATIVE_RAW_API 0
#else
#if defined(X86_WIN32)
#define FFI_TRAMPOLINE_SIZE 13
#else
#define FFI_TRAMPOLINE_SIZE 10
#endif
#define FFI_NATIVE_RAW_API 1
#endif
#endif
