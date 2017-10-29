#if !defined(Py_IMPORTDL_H)
#define Py_IMPORTDL_H
#if defined(__cplusplus)
extern "C" {
#endif
enum filetype {
SEARCH_ERROR,
PY_SOURCE,
PY_COMPILED,
C_EXTENSION,
PY_RESOURCE,
PKG_DIRECTORY,
C_BUILTIN,
PY_FROZEN,
PY_CODERESOURCE,
IMP_HOOK
};
struct filedescr {
char *suffix;
char *mode;
enum filetype type;
};
extern struct filedescr * _PyImport_Filetab;
extern const struct filedescr _PyImport_DynLoadFiletab[];
extern PyObject *_PyImport_LoadDynamicModule(char *name, char *pathname,
FILE *);
#define MAXSUFFIXSIZE 12
#if defined(MS_WINDOWS)
#include <windows.h>
typedef FARPROC dl_funcptr;
#else
#if defined(PYOS_OS2) && !defined(PYCC_GCC)
#include <os2def.h>
typedef int (* APIENTRY dl_funcptr)();
#else
typedef void (*dl_funcptr)(void);
#endif
#endif
#if defined(__cplusplus)
}
#endif
#endif
