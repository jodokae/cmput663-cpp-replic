#define BYTEORDER 1234
#if defined(DEBUG)
#define FFI_DEBUG
#else
#undef FFI_DEBUG
#endif
#define HAVE_ALLOCA 1
#define HAVE_MEMCPY 1
#define HAVE_STDLIB_H 1
#define HAVE_STRING_H 1
#define SIZEOF_DOUBLE 8
#define SIZEOF_LONG_DOUBLE 8
#define STDC_HEADERS 1
#define alloca _alloca
#define abort() exit(999)
