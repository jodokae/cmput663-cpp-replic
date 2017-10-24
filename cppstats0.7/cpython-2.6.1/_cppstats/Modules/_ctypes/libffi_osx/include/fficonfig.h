#if !defined(MACOSX)
#error "This file is only supported on Mac OS X"
#endif
#if defined(__i386__)
#define BYTEORDER 1234
#undef HOST_WORDS_BIG_ENDIAN
#undef WORDS_BIGENDIAN
#define SIZEOF_DOUBLE 8
#define HAVE_LONG_DOUBLE 1
#define SIZEOF_LONG_DOUBLE 16
#elif defined(__x86_64__)
#define BYTEORDER 1234
#undef HOST_WORDS_BIG_ENDIAN
#undef WORDS_BIGENDIAN
#define SIZEOF_DOUBLE 8
#define HAVE_LONG_DOUBLE 1
#define SIZEOF_LONG_DOUBLE 16
#elif defined(__ppc__)
#define BYTEORDER 4321
#define HOST_WORDS_BIG_ENDIAN 1
#define WORDS_BIGENDIAN 1
#define SIZEOF_DOUBLE 8
#if __GNUC__ >= 4
#define HAVE_LONG_DOUBLE 1
#define SIZEOF_LONG_DOUBLE 16
#else
#undef HAVE_LONG_DOUBLE
#define SIZEOF_LONG_DOUBLE 8
#endif
#elif defined(__ppc64__)
#define BYTEORDER 4321
#define HOST_WORDS_BIG_ENDIAN 1
#define WORDS_BIGENDIAN 1
#define SIZEOF_DOUBLE 8
#define HAVE_LONG_DOUBLE 1
#define SIZEOF_LONG_DOUBLE 16
#else
#error "Unknown CPU type"
#endif
#undef CRAY_STACKSEG_END
#define EH_FRAME_FLAGS "aw"
#define FFI_NO_RAW_API 1
#define HAVE_ALLOCA 1
#define HAVE_ALLOCA_H 1
#define HAVE_MEMCPY 1
#define HAVE_MMAP_ANON 1
#define HAVE_MMAP_FILE 1
#define PACKAGE "libffi"
#define PACKAGE_BUGREPORT "http://gcc.gnu.org/bugs.html"
#define PACKAGE_NAME "libffi"
#define PACKAGE_STRING "libffi 2.1"
#define PACKAGE_TARNAME "libffi"
#define PACKAGE_VERSION "2.1"
#define STDC_HEADERS 1
#define VERSION "2.1-pyobjc"
#if defined(HAVE_HIDDEN_VISIBILITY_ATTRIBUTE)
#if defined(LIBFFI_ASM)
#define FFI_HIDDEN(name) .hidden name
#else
#define FFI_HIDDEN __attribute__((visibility ("hidden")))
#endif
#else
#if defined(LIBFFI_ASM)
#define FFI_HIDDEN(name)
#else
#define FFI_HIDDEN
#endif
#endif