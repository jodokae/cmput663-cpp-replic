#if !defined(Py_PYCONFIG_H)
#define Py_PYCONFIG_H
#if !defined(_ALL_SOURCE)
#undef _ALL_SOURCE
#endif
#if !defined(__CHAR_UNSIGNED__)
#undef __CHAR_UNSIGNED__
#endif
#undef const
#undef gid_t
#undef HAVE_TM_ZONE
#undef HAVE_TZNAME
#undef mode_t
#undef off_t
#undef pid_t
#undef _POSIX_1_SOURCE
#undef _POSIX_SOURCE
#define RETSIGTYPE void
#undef size_t
#define STDC_HEADERS 1
#undef TIME_WITH_SYS_TIME
#define TM_IN_SYS_TIME 1
#undef uid_t
#undef WORDS_BIGENDIAN
#undef AIX_GENUINE_CPLUSPLUS
#undef BAD_STATIC_FORWARD
#undef BEOS_THREADS
#undef C_THREADS
#undef clock_t
#undef __EXTENSIONS__
#undef _FILE_OFFSET_BITS
#undef GETPGRP_HAVE_ARG
#undef GETTIMEOFDAY_NO_TZ
#undef HAVE_ALTZONE
#undef ENABLE_IPV6
#undef HAVE_SOCKADDR_SA_LEN
#undef HAVE_ADDRINFO
#undef HAVE_SOCKADDR_STORAGE
#define HAVE_DYNAMIC_LOADING 1
#undef HAVE_GETC_UNLOCKED
#undef HAVE_GETHOSTBYNAME_R
#undef HAVE_GETHOSTBYNAME_R_3_ARG
#undef HAVE_GETHOSTBYNAME_R_5_ARG
#undef HAVE_GETHOSTBYNAME_R_6_ARG
#undef HAVE_LARGEFILE_SUPPORT
#define HAVE_LONG_LONG
#define HAVE_PROTOTYPES 1
#undef HAVE_PTH
#undef HAVE_RL_COMPLETION_MATCHES
#define HAVE_STDARG_PROTOTYPES 1
#undef HAVE_UINTPTR_T
#undef HAVE_USABLE_WCHAR_T
#undef HAVE_WCHAR_H
#undef _LARGEFILE_SOURCE
#define Py_USING_UNICODE 1
#define PY_UNICODE_TYPE unsigned short
#define Py_UNICODE_SIZE 2
#undef HAVE_BROKEN_NICE
#undef _POSIX_THREADS
#undef Py_DEBUG
#undef _REENTRANT
#undef SETPGRP_HAVE_ARG
#undef signed
#undef SIGNED_RIGHT_SHIFT_ZERO_FILLS
#define SIZEOF_OFF_T 4
#define SIZEOF_TIME_T 4
#undef SIZEOF_PTHREAD_T
#define socklen_t int
#undef SYS_SELECT_WITH_SYS_TIME
#define VA_LIST_IS_ARRAY 1
#undef volatile
#undef WANT_SIGFPE_HANDLER
#undef WANT_WCTYPE_FUNCTIONS
#define WITH_DOC_STRINGS 1
#undef WITH_DYLD
#define WITH_PYMALLOC 1
#undef WITH_NEXT_FRAMEWORK
#undef USE_TOOLBOX_OBJECT_GLUE
#undef WITH_THREAD
#define SIZEOF_CHAR 1
#define SIZEOF_DOUBLE 8
#define SIZEOF_FLOAT 4
#undef SIZEOF_FPOS_T
#define SIZEOF_INT 4
#define SIZEOF_LONG 4
#define SIZEOF_LONG_LONG 8
#define SIZEOF_SHORT 2
#undef SIZEOF_UINTPTR_T
#define SIZEOF_VOID_P 4
#undef SIZEOF_WCHAR_T
#undef HAVE__GETPTY
#undef HAVE_ALARM
#undef HAVE_CHOWN
#define HAVE_CLOCK 1
#undef HAVE_CONFSTR
#undef HAVE_CTERMID
#undef HAVE_CTERMID_R
#undef HAVE_DLOPEN
#undef HAVE_DUP2
#undef HAVE_EXECV
#undef HAVE_FDATASYNC
#undef HAVE_FLOCK
#undef HAVE_FORK
#undef HAVE_FORKPTY
#undef HAVE_FPATHCONF
#undef HAVE_FSEEK64
#undef HAVE_FSEEKO
#undef HAVE_FSTATVFS
#undef HAVE_FSYNC
#undef HAVE_FTELL64
#undef HAVE_FTELLO
#undef HAVE_FTIME
#undef HAVE_FTRUNCATE
#undef HAVE_GAI_STRERROR
#undef HAVE_GETADDRINFO
#undef HAVE_GETCWD
#undef HAVE_GETGROUPS
#undef HAVE_GETHOSTBYNAME
#undef HAVE_GETLOGIN
#undef HAVE_GETNAMEINFO
#define HAVE_GETPEERNAME
#undef HAVE_GETPGID
#undef HAVE_GETPGRP
#undef HAVE_GETPID
#undef HAVE_GETPRIORITY
#undef HAVE_GETPWENT
#undef HAVE_GETTIMEOFDAY
#undef HAVE_GETWD
#undef HAVE_HSTRERROR
#define HAVE_HYPOT
#define HAVE_INET_PTON 1
#undef HAVE_KILL
#undef HAVE_LINK
#undef HAVE_LSTAT
#define HAVE_MEMMOVE 1
#undef HAVE_MKFIFO
#define HAVE_MKTIME 1
#undef HAVE_MREMAP
#undef HAVE_NICE
#undef HAVE_OPENPTY
#undef HAVE_PATHCONF
#undef HAVE_PAUSE
#undef HAVE_PLOCK
#undef HAVE_POLL
#undef HAVE_PTHREAD_INIT
#undef HAVE_PUTENV
#undef HAVE_READLINK
#undef HAVE_SELECT
#undef HAVE_SETEGID
#undef HAVE_SETEUID
#undef HAVE_SETGID
#define HAVE_SETLOCALE 1
#undef HAVE_SETPGID
#undef HAVE_SETPGRP
#undef HAVE_SETREGID
#undef HAVE_SETREUID
#undef HAVE_SETSID
#undef HAVE_SETUID
#undef HAVE_SETVBUF
#undef HAVE_SIGACTION
#undef HAVE_SIGINTERRUPT
#undef HAVE_SIGRELSE
#undef HAVE_SNPRINTF
#undef HAVE_STATVFS
#define HAVE_STRDUP 1
#define HAVE_STRERROR 1
#define HAVE_STRFTIME 1
#undef HAVE_SYMLINK
#undef HAVE_SYSCONF
#undef HAVE_TCGETPGRP
#undef HAVE_TCSETPGRP
#undef HAVE_TEMPNAM
#undef HAVE_TIMEGM
#undef HAVE_TIMES
#undef HAVE_TMPFILE
#undef HAVE_TMPNAM
#undef HAVE_TMPNAM_R
#undef HAVE_TRUNCATE
#undef HAVE_UNAME
#undef HAVE_WAITPID
#undef HAVE_CONIO_H
#undef HAVE_DB_H
#undef HAVE_DB1_NDBM_H
#undef HAVE_DB_185_H
#undef HAVE_DIRECT_H
#undef HAVE_DIRENT_H
#undef HAVE_DLFCN_H
#define HAVE_ERRNO_H 1
#undef HAVE_FCNTL_H
#undef HAVE_GDBM_NDBM_H
#undef HAVE_IO_H
#undef HAVE_LANGINFO_H
#undef HAVE_LIBUTIL_H
#undef HAVE_NCURSES_H
#undef HAVE_NDBM_H
#undef HAVE_NDIR_H
#undef HAVE_NETPACKET_PACKET_H
#undef HAVE_POLL_H
#undef HAVE_PROCESS_H
#undef HAVE_PTHREAD_H
#undef HAVE_PTY_H
#define HAVE_SIGNAL_H
#undef HAVE_SYS_AUDIOIO_H
#undef HAVE_SYS_DIR_H
#undef HAVE_SYS_FILE_H
#undef HAVE_SYS_LOCK_H
#undef HAVE_SYS_MODEM_H
#undef HAVE_SYS_NDIR_H
#undef HAVE_SYS_PARAM_H
#undef HAVE_SYS_POLL_H
#undef HAVE_SYS_RESOURCE_H
#undef HAVE_SYS_SELECT_H
#undef HAVE_SYS_SOCKET_H
#define HAVE_SYS_STAT_H 1
#undef HAVE_SYS_TIME_H
#undef HAVE_SYS_TIMES_H
#define HAVE_SYS_TYPES_H 1
#undef HAVE_SYS_UN_H
#undef HAVE_SYS_UTSNAME_H
#undef HAVE_SYS_WAIT_H
#undef HAVE_TERMIOS_H
#undef HAVE_THREAD_H
#define HAVE_UNISTD_H 1
#undef HAVE_UTIME_H
#undef HAVE_LIBDL
#undef HAVE_LIBDLD
#undef HAVE_LIBIEEE
#if defined(__CYGWIN__)
#if defined(USE_DL_IMPORT)
#define DL_IMPORT(RTYPE) __declspec(dllimport) RTYPE
#define DL_EXPORT(RTYPE) __declspec(dllexport) RTYPE
#else
#define DL_IMPORT(RTYPE) __declspec(dllexport) RTYPE
#define DL_EXPORT(RTYPE) __declspec(dllexport) RTYPE
#endif
#endif
#if defined(__USLC__) && defined(__SCO_VERSION__)
#define STRICT_SYSV_CURSES
#endif
#define DONT_HAVE_FSTAT 1
#define DONT_HAVE_STAT 1
#define PLATFORM "riscos"
#endif
