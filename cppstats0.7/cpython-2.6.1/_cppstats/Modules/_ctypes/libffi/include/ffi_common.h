#if !defined(FFI_COMMON_H)
#define FFI_COMMON_H
#if defined(__cplusplus)
extern "C" {
#endif
#include <fficonfig.h>
#if defined(__GNUC__)
#define alloca __builtin_alloca
#define MAYBE_UNUSED __attribute__((__unused__))
#else
#define MAYBE_UNUSED
#if HAVE_ALLOCA_H
#include <alloca.h>
#else
#if defined(_AIX)
#pragma alloca
#else
#if !defined(alloca)
char *alloca ();
#endif
#endif
#endif
#endif
#if STDC_HEADERS
#include <string.h>
#else
#if !defined(HAVE_MEMCPY)
#define memcpy(d, s, n) bcopy ((s), (d), (n))
#endif
#endif
#if defined(FFI_DEBUG)
#include <stdio.h>
#endif
#if defined(FFI_DEBUG)
void ffi_assert(char *expr, char *file, int line);
void ffi_stop_here(void);
void ffi_type_test(ffi_type *a, char *file, int line);
#define FFI_ASSERT(x) ((x) ? (void)0 : ffi_assert(#x, __FILE__,__LINE__))
#define FFI_ASSERT_AT(x, f, l) ((x) ? 0 : ffi_assert(#x, (f), (l)))
#define FFI_ASSERT_VALID_TYPE(x) ffi_type_test (x, __FILE__, __LINE__)
#else
#define FFI_ASSERT(x)
#define FFI_ASSERT_AT(x, f, l)
#define FFI_ASSERT_VALID_TYPE(x)
#endif
#define ALIGN(v, a) (((((size_t) (v))-1) | ((a)-1))+1)
#define ALIGN_DOWN(v, a) (((size_t) (v)) & -a)
ffi_status ffi_prep_cif_machdep(ffi_cif *cif);
typedef struct {
ffi_cif *cif;
void *rvalue;
void **avalue;
} extended_cif;
typedef unsigned int UINT8 __attribute__((__mode__(__QI__)));
typedef signed int SINT8 __attribute__((__mode__(__QI__)));
typedef unsigned int UINT16 __attribute__((__mode__(__HI__)));
typedef signed int SINT16 __attribute__((__mode__(__HI__)));
typedef unsigned int UINT32 __attribute__((__mode__(__SI__)));
typedef signed int SINT32 __attribute__((__mode__(__SI__)));
typedef unsigned int UINT64 __attribute__((__mode__(__DI__)));
typedef signed int SINT64 __attribute__((__mode__(__DI__)));
typedef float FLOAT32;
#if defined(__cplusplus)
}
#endif
#endif