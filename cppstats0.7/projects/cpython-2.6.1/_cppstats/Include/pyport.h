#if !defined(Py_PYPORT_H)
#define Py_PYPORT_H
#include "pyconfig.h"
#if defined(HAVE_STDINT_H)
#include <stdint.h>
#endif
#if defined(HAVE_PROTOTYPES)
#define Py_PROTO(x) x
#else
#define Py_PROTO(x) ()
#endif
#if !defined(Py_FPROTO)
#define Py_FPROTO(x) Py_PROTO(x)
#endif
#if defined(HAVE_LONG_LONG)
#if !defined(PY_LONG_LONG)
#define PY_LONG_LONG long long
#if defined(LLONG_MAX)
#define PY_LLONG_MIN LLONG_MIN
#define PY_LLONG_MAX LLONG_MAX
#define PY_ULLONG_MAX ULLONG_MAX
#elif defined(__LONG_LONG_MAX__)
#define PY_LLONG_MAX __LONG_LONG_MAX__
#define PY_LLONG_MIN (-PY_LLONG_MAX-1)
#define PY_ULLONG_MAX (__LONG_LONG_MAX__*2ULL + 1ULL)
#else
#define PY_ULLONG_MAX (~0ULL)
#define PY_LLONG_MAX ((long long)(PY_ULLONG_MAX>>1))
#define PY_LLONG_MIN (-PY_LLONG_MAX-1)
#endif
#endif
#endif
#if defined(HAVE_UINTPTR_T)
typedef uintptr_t Py_uintptr_t;
typedef intptr_t Py_intptr_t;
#elif SIZEOF_VOID_P <= SIZEOF_INT
typedef unsigned int Py_uintptr_t;
typedef int Py_intptr_t;
#elif SIZEOF_VOID_P <= SIZEOF_LONG
typedef unsigned long Py_uintptr_t;
typedef long Py_intptr_t;
#elif defined(HAVE_LONG_LONG) && (SIZEOF_VOID_P <= SIZEOF_LONG_LONG)
typedef unsigned PY_LONG_LONG Py_uintptr_t;
typedef PY_LONG_LONG Py_intptr_t;
#else
#error "Python needs a typedef for Py_uintptr_t in pyport.h."
#endif
#if defined(HAVE_SSIZE_T)
typedef ssize_t Py_ssize_t;
#elif SIZEOF_VOID_P == SIZEOF_SIZE_T
typedef Py_intptr_t Py_ssize_t;
#else
#error "Python needs a typedef for Py_ssize_t in pyport.h."
#endif
#if defined(SIZE_MAX)
#define PY_SIZE_MAX SIZE_MAX
#else
#define PY_SIZE_MAX ((size_t)-1)
#endif
#define PY_SSIZE_T_MAX ((Py_ssize_t)(((size_t)-1)>>1))
#define PY_SSIZE_T_MIN (-PY_SSIZE_T_MAX-1)
#if SIZEOF_PID_T > SIZEOF_LONG
#error "Python doesn't support sizeof(pid_t) > sizeof(long)"
#endif
#if !defined(PY_FORMAT_SIZE_T)
#if SIZEOF_SIZE_T == SIZEOF_INT && !defined(__APPLE__)
#define PY_FORMAT_SIZE_T ""
#elif SIZEOF_SIZE_T == SIZEOF_LONG
#define PY_FORMAT_SIZE_T "l"
#elif defined(MS_WINDOWS)
#define PY_FORMAT_SIZE_T "I"
#else
#error "This platform's pyconfig.h needs to define PY_FORMAT_SIZE_T"
#endif
#endif
#undef USE_INLINE
#if defined(_MSC_VER)
#if defined(PY_LOCAL_AGGRESSIVE)
#pragma optimize("agtw", on)
#endif
#pragma warning(disable: 4710)
#define Py_LOCAL(type) static type __fastcall
#define Py_LOCAL_INLINE(type) static __inline type __fastcall
#elif defined(USE_INLINE)
#define Py_LOCAL(type) static type
#define Py_LOCAL_INLINE(type) static inline type
#else
#define Py_LOCAL(type) static type
#define Py_LOCAL_INLINE(type) static type
#endif
#if defined(_MSC_VER)
#define Py_MEMCPY(target, source, length) do { size_t i_, n_ = (length); char *t_ = (void*) (target); const char *s_ = (void*) (source); if (n_ >= 16) memcpy(t_, s_, n_); else for (i_ = 0; i_ < n_; i_++) t_[i_] = s_[i_]; } while (0)
#else
#define Py_MEMCPY memcpy
#endif
#include <stdlib.h>
#include <math.h>
#if defined(TIME_WITH_SYS_TIME)
#include <sys/time.h>
#include <time.h>
#else
#if defined(HAVE_SYS_TIME_H)
#include <sys/time.h>
#else
#include <time.h>
#endif
#endif
#if defined(HAVE_SYS_SELECT_H)
#include <sys/select.h>
#endif
#if !defined(DONT_HAVE_STAT)
#define HAVE_STAT
#endif
#if !defined(DONT_HAVE_FSTAT)
#define HAVE_FSTAT
#endif
#if defined(RISCOS)
#include <sys/types.h>
#include "unixstuff.h"
#endif
#if defined(HAVE_SYS_STAT_H)
#if defined(PYOS_OS2) && defined(PYCC_GCC)
#include <sys/types.h>
#endif
#include <sys/stat.h>
#elif defined(HAVE_STAT_H)
#include <stat.h>
#endif
#if defined(PYCC_VACPP)
#define S_IFMT (S_IFDIR|S_IFCHR|S_IFREG)
#endif
#if !defined(S_ISREG)
#define S_ISREG(x) (((x) & S_IFMT) == S_IFREG)
#endif
#if !defined(S_ISDIR)
#define S_ISDIR(x) (((x) & S_IFMT) == S_IFDIR)
#endif
#if defined(__cplusplus)
extern "C" {
#endif
#if defined(SIGNED_RIGHT_SHIFT_ZERO_FILLS)
#define Py_ARITHMETIC_RIGHT_SHIFT(TYPE, I, J) ((I) < 0 ? ~((~(unsigned TYPE)(I)) >> (J)) : (I) >> (J))
#else
#define Py_ARITHMETIC_RIGHT_SHIFT(TYPE, I, J) ((I) >> (J))
#endif
#define Py_FORCE_EXPANSION(X) X
#if defined(Py_DEBUG)
#define Py_SAFE_DOWNCAST(VALUE, WIDE, NARROW) (assert((WIDE)(NARROW)(VALUE) == (VALUE)), (NARROW)(VALUE))
#else
#define Py_SAFE_DOWNCAST(VALUE, WIDE, NARROW) (NARROW)(VALUE)
#endif
#if defined(__FreeBSD__) || defined(__OpenBSD__) || (defined(__hpux) && defined(__ia64))
#define _Py_SET_EDOM_FOR_NAN(X) if (isnan(X)) errno = EDOM;
#else
#define _Py_SET_EDOM_FOR_NAN(X) ;
#endif
#define Py_SET_ERRNO_ON_MATH_ERROR(X) do { if (errno == 0) { if ((X) == Py_HUGE_VAL || (X) == -Py_HUGE_VAL) errno = ERANGE; else _Py_SET_EDOM_FOR_NAN(X) } } while(0)
#define Py_SET_ERANGE_IF_OVERFLOW(X) Py_SET_ERRNO_ON_MATH_ERROR(X)
#define Py_ADJUST_ERANGE1(X) do { if (errno == 0) { if ((X) == Py_HUGE_VAL || (X) == -Py_HUGE_VAL) errno = ERANGE; } else if (errno == ERANGE && (X) == 0.0) errno = 0; } while(0)
#define Py_ADJUST_ERANGE2(X, Y) do { if ((X) == Py_HUGE_VAL || (X) == -Py_HUGE_VAL || (Y) == Py_HUGE_VAL || (Y) == -Py_HUGE_VAL) { if (errno == 0) errno = ERANGE; } else if (errno == ERANGE) errno = 0; } while(0)
#if defined(__GNUC__) && ((__GNUC__ >= 4) || (__GNUC__ == 3) && (__GNUC_MINOR__ >= 1))
#define Py_DEPRECATED(VERSION_UNUSED) __attribute__((__deprecated__))
#else
#define Py_DEPRECATED(VERSION_UNUSED)
#endif
#if defined(SOLARIS)
extern int gethostname(char *, int);
#endif
#if defined(__BEOS__)
int shutdown( int, int );
#endif
#if defined(HAVE__GETPTY)
#include <sys/types.h>
extern char * _getpty(int *, int, mode_t, int);
#endif
#if defined(HAVE_SYS_TERMIO_H)
#include <sys/termio.h>
#endif
#if defined(HAVE_OPENPTY) || defined(HAVE_FORKPTY)
#if !defined(HAVE_PTY_H) && !defined(HAVE_LIBUTIL_H)
#include <termios.h>
extern int openpty(int *, int *, char *, struct termios *, struct winsize *);
extern pid_t forkpty(int *, char *, struct termios *, struct winsize *);
#endif
#endif
#if 0
extern int getrusage();
extern int getpagesize();
extern int fclose(FILE *);
extern int fdatasync(int);
#endif
#if defined(__FreeBSD__)
#include <osreldate.h>
#if __FreeBSD_version > 500039
#include <ctype.h>
#include <wctype.h>
#undef isalnum
#define isalnum(c) iswalnum(btowc(c))
#undef isalpha
#define isalpha(c) iswalpha(btowc(c))
#undef islower
#define islower(c) iswlower(btowc(c))
#undef isspace
#define isspace(c) iswspace(btowc(c))
#undef isupper
#define isupper(c) iswupper(btowc(c))
#undef tolower
#define tolower(c) towlower(btowc(c))
#undef toupper
#define toupper(c) towupper(btowc(c))
#endif
#endif
#if defined(__CYGWIN__) || defined(__BEOS__)
#define HAVE_DECLSPEC_DLL
#endif
#if defined(Py_ENABLE_SHARED) || defined(__CYGWIN__)
#if defined(HAVE_DECLSPEC_DLL)
#if defined(Py_BUILD_CORE)
#define PyAPI_FUNC(RTYPE) __declspec(dllexport) RTYPE
#define PyAPI_DATA(RTYPE) extern __declspec(dllexport) RTYPE
#if defined(__CYGWIN__)
#define PyMODINIT_FUNC __declspec(dllexport) void
#else
#define PyMODINIT_FUNC void
#endif
#else
#if !defined(__CYGWIN__)
#define PyAPI_FUNC(RTYPE) __declspec(dllimport) RTYPE
#endif
#define PyAPI_DATA(RTYPE) extern __declspec(dllimport) RTYPE
#if defined(__cplusplus)
#define PyMODINIT_FUNC extern "C" __declspec(dllexport) void
#else
#define PyMODINIT_FUNC __declspec(dllexport) void
#endif
#endif
#endif
#endif
#if !defined(PyAPI_FUNC)
#define PyAPI_FUNC(RTYPE) RTYPE
#endif
#if !defined(PyAPI_DATA)
#define PyAPI_DATA(RTYPE) extern RTYPE
#endif
#if !defined(PyMODINIT_FUNC)
#if defined(__cplusplus)
#define PyMODINIT_FUNC extern "C" void
#else
#define PyMODINIT_FUNC void
#endif
#endif
#if defined(Py_ENABLE_SHARED) && defined (HAVE_DECLSPEC_DLL)
#if defined(Py_BUILD_CORE)
#define DL_IMPORT(RTYPE) __declspec(dllexport) RTYPE
#define DL_EXPORT(RTYPE) __declspec(dllexport) RTYPE
#else
#define DL_IMPORT(RTYPE) __declspec(dllimport) RTYPE
#define DL_EXPORT(RTYPE) __declspec(dllexport) RTYPE
#endif
#endif
#if !defined(DL_EXPORT)
#define DL_EXPORT(RTYPE) RTYPE
#endif
#if !defined(DL_IMPORT)
#define DL_IMPORT(RTYPE) RTYPE
#endif
#if 0
#if !defined(FD_SETSIZE)
#define FD_SETSIZE 256
#endif
#if !defined(FD_SET)
typedef long fd_mask;
#define NFDBITS (sizeof(fd_mask) * NBBY)
#if !defined(howmany)
#define howmany(x, y) (((x)+((y)-1))/(y))
#endif
typedef struct fd_set {
fd_mask fds_bits[howmany(FD_SETSIZE, NFDBITS)];
} fd_set;
#define FD_SET(n, p) ((p)->fds_bits[(n)/NFDBITS] |= (1 << ((n) % NFDBITS)))
#define FD_CLR(n, p) ((p)->fds_bits[(n)/NFDBITS] &= ~(1 << ((n) % NFDBITS)))
#define FD_ISSET(n, p) ((p)->fds_bits[(n)/NFDBITS] & (1 << ((n) % NFDBITS)))
#define FD_ZERO(p) memset((char *)(p), '\0', sizeof(*(p)))
#endif
#endif
#if !defined(INT_MAX)
#define INT_MAX 2147483647
#endif
#if !defined(LONG_MAX)
#if SIZEOF_LONG == 4
#define LONG_MAX 0X7FFFFFFFL
#elif SIZEOF_LONG == 8
#define LONG_MAX 0X7FFFFFFFFFFFFFFFL
#else
#error "could not set LONG_MAX in pyport.h"
#endif
#endif
#if !defined(LONG_MIN)
#define LONG_MIN (-LONG_MAX-1)
#endif
#if !defined(LONG_BIT)
#define LONG_BIT (8 * SIZEOF_LONG)
#endif
#if LONG_BIT != 8 * SIZEOF_LONG
#error "LONG_BIT definition appears wrong for platform (bad gcc/glibc config?)."
#endif
#if defined(__cplusplus)
}
#endif
#if (!defined(__GNUC__) || __GNUC__ < 2 || (__GNUC__ == 2 && __GNUC_MINOR__ < 7) ) && !defined(RISCOS)
#define Py_GCC_ATTRIBUTE(x)
#else
#define Py_GCC_ATTRIBUTE(x) __attribute__(x)
#endif
#if defined(HAVE_ATTRIBUTE_FORMAT_PARSETUPLE)
#define Py_FORMAT_PARSETUPLE(func,p1,p2) __attribute__((format(func,p1,p2)))
#else
#define Py_FORMAT_PARSETUPLE(func,p1,p2)
#endif
#if defined(__SUNPRO_C)
#pragma error_messages (off,E_END_OF_LOOP_CODE_NOT_REACHED)
#endif
#if !defined(Py_LL)
#define Py_LL(x) x##LL
#endif
#if !defined(Py_ULL)
#define Py_ULL(x) Py_LL(x##U)
#endif
#endif