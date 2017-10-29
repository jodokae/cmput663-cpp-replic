#if !defined(LIBFFI_TARGET_H)
#define LIBFFI_TARGET_H
#if !defined(LIBFFI_ASM)
typedef unsigned long ffi_arg;
typedef signed long ffi_sarg;
typedef enum ffi_abi {
FFI_FIRST_ABI = 0,
FFI_SYSV,
FFI_DEFAULT_ABI = FFI_SYSV,
FFI_LAST_ABI = FFI_DEFAULT_ABI + 1
} ffi_abi;
#endif
#define FFI_CLOSURES 1
#define FFI_CRIS_TRAMPOLINE_CODE_PART_SIZE 36
#define FFI_CRIS_TRAMPOLINE_DATA_PART_SIZE (7*4)
#define FFI_TRAMPOLINE_SIZE (FFI_CRIS_TRAMPOLINE_CODE_PART_SIZE + FFI_CRIS_TRAMPOLINE_DATA_PART_SIZE)
#define FFI_NATIVE_RAW_API 0
#endif