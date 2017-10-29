#if !defined(Py_OSDEFS_H)
#define Py_OSDEFS_H
#if defined(__cplusplus)
extern "C" {
#endif
#if !defined(__QNX__)
#if defined(MS_WINDOWS) || defined(__BORLANDC__) || defined(__WATCOMC__) || defined(__DJGPP__) || defined(PYOS_OS2)
#if defined(PYOS_OS2) && defined(PYCC_GCC)
#define MAXPATHLEN 260
#define SEP '/'
#define ALTSEP '\\'
#else
#define SEP '\\'
#define ALTSEP '/'
#define MAXPATHLEN 256
#endif
#define DELIM ';'
#endif
#endif
#if defined(RISCOS)
#define SEP '.'
#define MAXPATHLEN 256
#define DELIM ','
#endif
#if !defined(SEP)
#define SEP '/'
#endif
#if !defined(MAXPATHLEN)
#if defined(PATH_MAX) && PATH_MAX > 1024
#define MAXPATHLEN PATH_MAX
#else
#define MAXPATHLEN 1024
#endif
#endif
#if !defined(DELIM)
#define DELIM ':'
#endif
#if defined(__cplusplus)
}
#endif
#endif