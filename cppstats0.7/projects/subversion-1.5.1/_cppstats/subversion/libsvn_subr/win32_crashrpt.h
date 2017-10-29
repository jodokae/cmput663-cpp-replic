#if !defined(SVN_LIBSVN_SUBR_WIN32_CRASHRPT_H)
#define SVN_LIBSVN_SUBR_WIN32_CRASHRPT_H
#if defined(WIN32)
#if defined(SVN_USE_WIN32_CRASHHANDLER)
LONG WINAPI svn__unhandled_exception_filter(PEXCEPTION_POINTERS ptrs);
#endif
#endif
#endif
