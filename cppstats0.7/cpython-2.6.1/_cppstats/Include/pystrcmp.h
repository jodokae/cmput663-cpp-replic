#if !defined(Py_STRCMP_H)
#define Py_STRCMP_H
#if defined(__cplusplus)
extern "C" {
#endif
PyAPI_FUNC(int) PyOS_mystrnicmp(const char *, const char *, Py_ssize_t);
PyAPI_FUNC(int) PyOS_mystricmp(const char *, const char *);
#if defined(MS_WINDOWS) || defined(PYOS_OS2)
#define PyOS_strnicmp strnicmp
#define PyOS_stricmp stricmp
#else
#define PyOS_strnicmp PyOS_mystrnicmp
#define PyOS_stricmp PyOS_mystricmp
#endif
#if defined(__cplusplus)
}
#endif
#endif
