#if defined(__GNUC__) && defined(__i386__)
#define FASTCALL __attribute__((regparm(3)))
#define PTRFASTCALL __attribute__((regparm(3)))
#endif
#if !defined(FASTCALL)
#define FASTCALL
#endif
#if !defined(PTRCALL)
#define PTRCALL
#endif
#if !defined(PTRFASTCALL)
#define PTRFASTCALL
#endif
#if !defined(XML_MIN_SIZE)
#if !defined(__cplusplus) && !defined(inline)
#if defined(__GNUC__)
#define inline __inline
#endif
#endif
#endif
#if defined(__cplusplus)
#define inline inline
#else
#if !defined(inline)
#define inline
#endif
#endif