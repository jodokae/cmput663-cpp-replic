#if !defined(Py_CONFIG_H)
#define Py_CONFIG_H
#if defined(_WIN32_WCE)
#define MS_WINCE
#endif
#if defined(USE_DL_EXPORT)
#define Py_BUILD_CORE
#endif
#if !defined(_CRT_SECURE_NO_DEPRECATE)
#define _CRT_SECURE_NO_DEPRECATE 1
#endif
#if !defined(_CRT_NONSTDC_NO_DEPRECATE)
#define _CRT_NONSTDC_NO_DEPRECATE 1
#endif
#if !defined(MS_WINCE)
#define HAVE_IO_H
#define HAVE_SYS_UTIME_H
#define HAVE_TEMPNAM
#define HAVE_TMPFILE
#define HAVE_TMPNAM
#define HAVE_CLOCK
#define HAVE_STRERROR
#endif
#if defined(HAVE_IO_H)
#include <io.h>
#endif
#define HAVE_HYPOT
#define HAVE_STRFTIME
#define DONT_HAVE_SIG_ALARM
#define DONT_HAVE_SIG_PAUSE
#define LONG_BIT 32
#define WORD_BIT 32
#define PREFIX ""
#define EXEC_PREFIX ""
#define MS_WIN32
#define MS_WINDOWS
#if !defined(PYTHONPATH)
#define PYTHONPATH ".\\DLLs;.\\lib;.\\lib\\plat-win;.\\lib\\lib-tk"
#endif
#define NT_THREADS
#define WITH_THREAD
#if !defined(NETSCAPE_PI)
#define USE_SOCKET
#endif
#if defined(MS_WINCE)
#define GetVersion() (4)
#define getenv(v) (NULL)
#define environ (NULL)
#endif
#if defined(_MSC_VER)
#define _Py_PASTE_VERSION(SUFFIX) ("[MSC v." _Py_STRINGIZE(_MSC_VER) " " SUFFIX "]")
#define _Py_STRINGIZE(X) _Py_STRINGIZE1((X))
#define _Py_STRINGIZE1(X) _Py_STRINGIZE2 ##X
#define _Py_STRINGIZE2(X) #X
#if defined(_WIN64)
#define MS_WIN64
#endif
#if defined(MS_WIN64)
#if defined(_M_IA64)
#define COMPILER _Py_PASTE_VERSION("64 bit (Itanium)")
#define MS_WINI64
#elif defined(_M_X64) || defined(_M_AMD64)
#define COMPILER _Py_PASTE_VERSION("64 bit (AMD64)")
#define MS_WINX64
#else
#define COMPILER _Py_PASTE_VERSION("64 bit (Unknown)")
#endif
#endif
#if defined(MS_WINX64)
#define Py_WINVER _WIN32_WINNT_WINXP
#define Py_NTDDI NTDDI_WINXP
#else
#if defined(_WIN32_WINNT_WIN2K)
#define Py_WINVER _WIN32_WINNT_WIN2K
#else
#define Py_WINVER 0x0500
#endif
#define Py_NTDDI NTDDI_WIN2KSP4
#endif
#if defined(Py_BUILD_CORE) || defined(Py_BUILD_CORE_MODULE)
#if !defined(NTDDI_VERSION)
#define NTDDI_VERSION Py_NTDDI
#endif
#if !defined(WINVER)
#define WINVER Py_WINVER
#endif
#if !defined(_WIN32_WINNT)
#define _WIN32_WINNT Py_WINVER
#endif
#endif
#if !defined(_W64)
#define _W64
#endif
#if defined(MS_WIN64)
typedef __int64 ssize_t;
#else
typedef _W64 int ssize_t;
#endif
#define HAVE_SSIZE_T 1
#if defined(MS_WIN32) && !defined(MS_WIN64)
#if defined(_M_IX86)
#define COMPILER _Py_PASTE_VERSION("32 bit (Intel)")
#else
#define COMPILER _Py_PASTE_VERSION("32 bit (Unknown)")
#endif
#endif
typedef int pid_t;
#include <float.h>
#define Py_IS_NAN _isnan
#define Py_IS_INFINITY(X) (!_finite(X) && !_isnan(X))
#define Py_IS_FINITE(X) _finite(X)
#define copysign _copysign
#define hypot _hypot
#endif
#if defined(_MSC_VER) && _MSC_VER >= 1200
#include <basetsd.h>
#endif
#if defined(__BORLANDC__)
#define COMPILER "[Borland]"
#if defined(_WIN32)
typedef int pid_t;
#undef HAVE_SYS_UTIME_H
#define HAVE_UTIME_H
#define HAVE_DIRENT_H
#include <io.h>
#define _chsize chsize
#define _setmode setmode
#else
#error "Only Win32 and later are supported"
#endif
#endif
#if defined(__GNUC__) && defined(_WIN32)
#if (__GNUC__==2) && (__GNUC_MINOR__<=91)
#warning "Please use an up-to-date version of gcc! (>2.91 recommended)"
#endif
#define COMPILER "[gcc]"
#define hypot _hypot
#define PY_LONG_LONG long long
#define PY_LLONG_MIN LLONG_MIN
#define PY_LLONG_MAX LLONG_MAX
#define PY_ULLONG_MAX ULLONG_MAX
#endif
#if defined(__LCC__)
#define COMPILER "[lcc-win32]"
typedef int pid_t;
#endif
#if !defined(NO_STDIO_H)
#include <stdio.h>
#endif
#define HAVE_LONG_LONG 1
#if !defined(PY_LONG_LONG)
#define PY_LONG_LONG __int64
#define PY_LLONG_MAX _I64_MAX
#define PY_LLONG_MIN _I64_MIN
#define PY_ULLONG_MAX _UI64_MAX
#endif
#if !defined(MS_NO_COREDLL) && !defined(Py_NO_ENABLE_SHARED)
#define Py_ENABLE_SHARED 1
#define MS_COREDLL
#endif
#define HAVE_DECLSPEC_DLL
#if defined(MS_COREDLL)
#if !defined(Py_BUILD_CORE)
#if defined(_MSC_VER)
#if defined(_DEBUG)
#pragma comment(lib,"python26_d.lib")
#else
#pragma comment(lib,"python26.lib")
#endif
#endif
#endif
#endif
#if defined(MS_WIN64)
#define PLATFORM "win32"
#define SIZEOF_VOID_P 8
#define SIZEOF_TIME_T 8
#define SIZEOF_OFF_T 4
#define SIZEOF_FPOS_T 8
#define SIZEOF_HKEY 8
#define SIZEOF_SIZE_T 8
#define HAVE_LARGEFILE_SUPPORT
#elif defined(MS_WIN32)
#define PLATFORM "win32"
#define HAVE_LARGEFILE_SUPPORT
#define SIZEOF_VOID_P 4
#define SIZEOF_OFF_T 4
#define SIZEOF_FPOS_T 8
#define SIZEOF_HKEY 4
#define SIZEOF_SIZE_T 4
#if defined(_MSC_VER) && _MSC_VER >= 1400
#define SIZEOF_TIME_T 8
#else
#define SIZEOF_TIME_T 4
#endif
#endif
#if defined(_DEBUG)
#define Py_DEBUG
#endif
#if defined(MS_WIN32)
#define SIZEOF_SHORT 2
#define SIZEOF_INT 4
#define SIZEOF_LONG 4
#define SIZEOF_LONG_LONG 8
#define SIZEOF_DOUBLE 8
#define SIZEOF_FLOAT 4
#if defined(_MSC_VER)
#if _MSC_VER > 1300
#define HAVE_UINTPTR_T 1
#define HAVE_INTPTR_T 1
#else
#define Py_LL(x) x##I64
#endif
#endif
#endif
#define HAVE_COPYSIGN 1
#define HAVE_ISINF 1
#define HAVE_ISNAN 1
#if !defined(_ALL_SOURCE)
#endif
#if !defined(MS_WINCE)
#define HAVE_CONIO_H 1
#endif
#if !defined(MS_WINCE)
#define HAVE_DIRECT_H 1
#endif
#define HAVE_TZNAME
#define RETSIGTYPE void
#define STDC_HEADERS 1
#if !defined(MS_WINCE)
#define HAVE_PUTENV
#endif
#define HAVE_PROTOTYPES
#define WITH_DOC_STRINGS 1
#define Py_USING_UNICODE
#define Py_UNICODE_SIZE 2
#define Py_WIN_WIDE_FILENAMES
#define WITH_PYMALLOC 1
#define HAVE_DYNAMIC_LOADING
#if !defined(MS_WINCE)
#define HAVE_FTIME
#endif
#define HAVE_GETPEERNAME
#if !defined(MS_WINCE)
#define HAVE_GETPID
#endif
#define HAVE_MKTIME
#define HAVE_SETVBUF
#if !defined(MS_WINCE)
#define HAVE_WCSCOLL 1
#endif
#if !defined(MS_WINCE)
#define HAVE_ERRNO_H 1
#endif
#if !defined(MS_WINCE)
#define HAVE_FCNTL_H 1
#endif
#if !defined(MS_WINCE)
#define HAVE_PROCESS_H 1
#endif
#if !defined(MS_WINCE)
#define HAVE_SIGNAL_H 1
#endif
#define HAVE_STDARG_PROTOTYPES
#define HAVE_STDDEF_H 1
#if !defined(MS_WINCE)
#define HAVE_SYS_STAT_H 1
#endif
#if !defined(MS_WINCE)
#define HAVE_SYS_TYPES_H 1
#endif
#define HAVE_WCHAR_H 1
#define HAVE_LIBNSL 1
#define HAVE_LIBSOCKET 1
#define Py_SOCKET_FD_CAN_BE_GE_FD_SETSIZE
#endif