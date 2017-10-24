#if !defined(PYMACCONFIG_H)
#define PYMACCONFIG_H
#if defined(__APPLE__)
#undef SIZEOF_LONG
#undef SIZEOF_PTHREAD_T
#undef SIZEOF_SIZE_T
#undef SIZEOF_TIME_T
#undef SIZEOF_VOID_P
#undef VA_LIST_IS_ARRAY
#if defined(__LP64__) && defined(__x86_64__)
#define VA_LIST_IS_ARRAY 1
#endif
#undef HAVE_LARGEFILE_SUPPORT
#if !defined(__LP64__)
#define HAVE_LARGEFILE_SUPPORT 1
#endif
#undef SIZEOF_LONG
#if defined(__LP64__)
#define SIZEOF_LONG 8
#define SIZEOF_PTHREAD_T 8
#define SIZEOF_SIZE_T 8
#define SIZEOF_TIME_T 8
#define SIZEOF_VOID_P 8
#else
#define SIZEOF_LONG 4
#define SIZEOF_PTHREAD_T 4
#define SIZEOF_SIZE_T 4
#define SIZEOF_TIME_T 4
#define SIZEOF_VOID_P 4
#endif
#if defined(__LP64__)
#undef SETPGRP_HAVE_ARG
#endif
#endif
#endif
