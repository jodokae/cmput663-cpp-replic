#include "Python.h"
#include "osdefs.h"
#include <windows.h>
#include "importdl.h"
#include "malloc.h"
extern const char *PyWin_DLLVersionString;
FILE *PyWin_FindRegisteredModule(const char *moduleName,
struct filedescr **ppFileDesc,
char *pathBuf,
Py_ssize_t pathLen) {
char *moduleKey;
const char keyPrefix[] = "Software\\Python\\PythonCore\\";
const char keySuffix[] = "\\Modules\\";
#if defined(_DEBUG)
const char debugString[] = "\\Debug";
#else
const char debugString[] = "";
#endif
struct filedescr *fdp = NULL;
FILE *fp;
HKEY keyBase = HKEY_CURRENT_USER;
int modNameSize;
long regStat;
size_t bufSize = sizeof(keyPrefix)-1 +
strlen(PyWin_DLLVersionString) +
sizeof(keySuffix) +
strlen(moduleName) +
sizeof(debugString) - 1;
moduleKey = alloca(bufSize);
PyOS_snprintf(moduleKey, bufSize,
"Software\\Python\\PythonCore\\%s\\Modules\\%s%s",
PyWin_DLLVersionString, moduleName, debugString);
assert(pathLen < INT_MAX);
modNameSize = (int)pathLen;
regStat = RegQueryValue(keyBase, moduleKey, pathBuf, &modNameSize);
if (regStat != ERROR_SUCCESS) {
keyBase = HKEY_LOCAL_MACHINE;
modNameSize = (int)pathLen;
regStat = RegQueryValue(keyBase, moduleKey,
pathBuf, &modNameSize);
if (regStat != ERROR_SUCCESS)
return NULL;
}
for (fdp = _PyImport_Filetab; fdp->suffix != NULL; fdp++) {
size_t extLen = strlen(fdp->suffix);
assert(modNameSize >= 0);
if ((size_t)modNameSize > extLen &&
strnicmp(pathBuf + ((size_t)modNameSize-extLen-1),
fdp->suffix,
extLen) == 0)
break;
}
if (fdp->suffix == NULL)
return NULL;
fp = fopen(pathBuf, fdp->mode);
if (fp != NULL)
*ppFileDesc = fdp;
return fp;
}
