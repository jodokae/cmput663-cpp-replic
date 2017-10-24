#include "Python.h"
#include "osdefs.h"
#if defined(MS_WINDOWS)
#include <windows.h>
#include <tchar.h>
#endif
#if defined(HAVE_SYS_TYPES_H)
#include <sys/types.h>
#endif
#if defined(HAVE_SYS_STAT_H)
#include <sys/stat.h>
#endif
#include <string.h>
#if !defined(LANDMARK)
#define LANDMARK "lib\\os.py"
#endif
static char prefix[MAXPATHLEN+1];
static char progpath[MAXPATHLEN+1];
static char dllpath[MAXPATHLEN+1];
static char *module_search_path = NULL;
static int
is_sep(char ch) {
#if defined(ALTSEP)
return ch == SEP || ch == ALTSEP;
#else
return ch == SEP;
#endif
}
static void
reduce(char *dir) {
size_t i = strlen(dir);
while (i > 0 && !is_sep(dir[i]))
--i;
dir[i] = '\0';
}
static int
exists(char *filename) {
struct stat buf;
return stat(filename, &buf) == 0;
}
static int
ismodule(char *filename) {
if (exists(filename))
return 1;
if (strlen(filename) < MAXPATHLEN) {
strcat(filename, Py_OptimizeFlag ? "o" : "c");
if (exists(filename))
return 1;
}
return 0;
}
static void
join(char *buffer, char *stuff) {
size_t n, k;
if (is_sep(stuff[0]))
n = 0;
else {
n = strlen(buffer);
if (n > 0 && !is_sep(buffer[n-1]) && n < MAXPATHLEN)
buffer[n++] = SEP;
}
if (n > MAXPATHLEN)
Py_FatalError("buffer overflow in getpathp.c's joinpath()");
k = strlen(stuff);
if (n + k > MAXPATHLEN)
k = MAXPATHLEN - n;
strncpy(buffer+n, stuff, k);
buffer[n+k] = '\0';
}
static int
gotlandmark(char *landmark) {
int ok;
Py_ssize_t n;
n = strlen(prefix);
join(prefix, landmark);
ok = ismodule(prefix);
prefix[n] = '\0';
return ok;
}
static int
search_for_prefix(char *argv0_path, char *landmark) {
strcpy(prefix, argv0_path);
do {
if (gotlandmark(landmark))
return 1;
reduce(prefix);
} while (prefix[0]);
return 0;
}
#if defined(MS_WINDOWS)
extern const char *PyWin_DLLVersionString;
static char *
getpythonregpath(HKEY keyBase, int skipcore) {
HKEY newKey = 0;
DWORD dataSize = 0;
DWORD numKeys = 0;
LONG rc;
char *retval = NULL;
TCHAR *dataBuf = NULL;
static const TCHAR keyPrefix[] = _T("Software\\Python\\PythonCore\\");
static const TCHAR keySuffix[] = _T("\\PythonPath");
size_t versionLen;
DWORD index;
TCHAR *keyBuf = NULL;
TCHAR *keyBufPtr;
TCHAR **ppPaths = NULL;
versionLen = _tcslen(PyWin_DLLVersionString);
keyBuf = keyBufPtr = malloc(sizeof(keyPrefix) +
sizeof(TCHAR)*(versionLen-1) +
sizeof(keySuffix));
if (keyBuf==NULL) goto done;
memcpy(keyBufPtr, keyPrefix, sizeof(keyPrefix)-sizeof(TCHAR));
keyBufPtr += sizeof(keyPrefix)/sizeof(TCHAR) - 1;
memcpy(keyBufPtr, PyWin_DLLVersionString, versionLen * sizeof(TCHAR));
keyBufPtr += versionLen;
memcpy(keyBufPtr, keySuffix, sizeof(keySuffix));
rc=RegOpenKeyEx(keyBase,
keyBuf,
0,
KEY_READ,
&newKey);
if (rc!=ERROR_SUCCESS) goto done;
rc = RegQueryInfoKey(newKey, NULL, NULL, NULL, &numKeys, NULL, NULL,
NULL, NULL, &dataSize, NULL, NULL);
if (rc!=ERROR_SUCCESS) goto done;
if (skipcore) dataSize = 0;
ppPaths = malloc( sizeof(TCHAR *) * numKeys );
if (ppPaths==NULL) goto done;
memset(ppPaths, 0, sizeof(TCHAR *) * numKeys);
for(index=0; index<numKeys; index++) {
TCHAR keyBuf[MAX_PATH+1];
HKEY subKey = 0;
DWORD reqdSize = MAX_PATH+1;
DWORD rc = RegEnumKeyEx(newKey, index, keyBuf, &reqdSize,
NULL, NULL, NULL, NULL );
if (rc!=ERROR_SUCCESS) goto done;
rc=RegOpenKeyEx(newKey,
keyBuf,
0,
KEY_READ,
&subKey);
if (rc!=ERROR_SUCCESS) goto done;
RegQueryValueEx(subKey, NULL, 0, NULL, NULL, &reqdSize);
if (reqdSize) {
ppPaths[index] = malloc(reqdSize);
if (ppPaths[index]) {
RegQueryValueEx(subKey, NULL, 0, NULL,
(LPBYTE)ppPaths[index],
&reqdSize);
dataSize += reqdSize + 1;
}
}
RegCloseKey(subKey);
}
if (dataSize == 0) goto done;
dataBuf = malloc((dataSize+1) * sizeof(TCHAR));
if (dataBuf) {
TCHAR *szCur = dataBuf;
DWORD reqdSize = dataSize;
for (index=0; index<numKeys; index++) {
if (index > 0) {
*(szCur++) = _T(';');
dataSize--;
}
if (ppPaths[index]) {
Py_ssize_t len = _tcslen(ppPaths[index]);
_tcsncpy(szCur, ppPaths[index], len);
szCur += len;
assert(dataSize > (DWORD)len);
dataSize -= (DWORD)len;
}
}
if (skipcore)
*szCur = '\0';
else {
if (numKeys) {
*(szCur++) = _T(';');
dataSize--;
}
rc = RegQueryValueEx(newKey, NULL, 0, NULL,
(LPBYTE)szCur, &dataSize);
}
#if defined(UNICODE)
retval = (char *)malloc(reqdSize+1);
if (retval)
WideCharToMultiByte(CP_ACP, 0,
dataBuf, -1,
retval, reqdSize+1,
NULL, NULL);
free(dataBuf);
#else
retval = dataBuf;
#endif
}
done:
if (ppPaths) {
for(index=0; index<numKeys; index++)
if (ppPaths[index]) free(ppPaths[index]);
free(ppPaths);
}
if (newKey)
RegCloseKey(newKey);
if (keyBuf)
free(keyBuf);
return retval;
}
#endif
static void
get_progpath(void) {
extern char *Py_GetProgramName(void);
char *path = getenv("PATH");
char *prog = Py_GetProgramName();
#if defined(MS_WINDOWS)
extern HANDLE PyWin_DLLhModule;
#if defined(UNICODE)
WCHAR wprogpath[MAXPATHLEN+1];
wprogpath[MAXPATHLEN]=_T('\0');
if (PyWin_DLLhModule &&
GetModuleFileName(PyWin_DLLhModule, wprogpath, MAXPATHLEN)) {
WideCharToMultiByte(CP_ACP, 0,
wprogpath, -1,
dllpath, MAXPATHLEN+1,
NULL, NULL);
}
wprogpath[MAXPATHLEN]=_T('\0');
if (GetModuleFileName(NULL, wprogpath, MAXPATHLEN)) {
WideCharToMultiByte(CP_ACP, 0,
wprogpath, -1,
progpath, MAXPATHLEN+1,
NULL, NULL);
return;
}
#else
if (PyWin_DLLhModule)
if (!GetModuleFileName(PyWin_DLLhModule, dllpath, MAXPATHLEN))
dllpath[0] = 0;
if (GetModuleFileName(NULL, progpath, MAXPATHLEN))
return;
#endif
#endif
if (prog == NULL || *prog == '\0')
prog = "python";
#if defined(ALTSEP)
if (strchr(prog, SEP) || strchr(prog, ALTSEP))
#else
if (strchr(prog, SEP))
#endif
strncpy(progpath, prog, MAXPATHLEN);
else if (path) {
while (1) {
char *delim = strchr(path, DELIM);
if (delim) {
size_t len = delim - path;
len = min(MAXPATHLEN,len);
strncpy(progpath, path, len);
*(progpath + len) = '\0';
} else
strncpy(progpath, path, MAXPATHLEN);
join(progpath, prog);
if (exists(progpath))
break;
if (!delim) {
progpath[0] = '\0';
break;
}
path = delim + 1;
}
} else
progpath[0] = '\0';
}
static void
calculate_path(void) {
char argv0_path[MAXPATHLEN+1];
char *buf;
size_t bufsz;
char *pythonhome = Py_GetPythonHome();
char *envpath = Py_GETENV("PYTHONPATH");
#if defined(MS_WINDOWS)
int skiphome, skipdefault;
char *machinepath = NULL;
char *userpath = NULL;
char zip_path[MAXPATHLEN+1];
size_t len;
#endif
get_progpath();
strcpy(argv0_path, progpath);
reduce(argv0_path);
if (pythonhome == NULL || *pythonhome == '\0') {
if (search_for_prefix(argv0_path, LANDMARK))
pythonhome = prefix;
else
pythonhome = NULL;
} else
strncpy(prefix, pythonhome, MAXPATHLEN);
if (envpath && *envpath == '\0')
envpath = NULL;
#if defined(MS_WINDOWS)
if (dllpath[0])
strncpy(zip_path, dllpath, MAXPATHLEN);
else
strncpy(zip_path, progpath, MAXPATHLEN);
zip_path[MAXPATHLEN] = '\0';
len = strlen(zip_path);
if (len > 4) {
zip_path[len-3] = 'z';
zip_path[len-2] = 'i';
zip_path[len-1] = 'p';
} else {
zip_path[0] = 0;
}
skiphome = pythonhome==NULL ? 0 : 1;
machinepath = getpythonregpath(HKEY_LOCAL_MACHINE, skiphome);
userpath = getpythonregpath(HKEY_CURRENT_USER, skiphome);
skipdefault = envpath!=NULL || pythonhome!=NULL || \
machinepath!=NULL || userpath!=NULL;
#endif
if (pythonhome != NULL) {
char *p;
bufsz = 1;
for (p = PYTHONPATH; *p; p++) {
if (*p == DELIM)
bufsz++;
}
bufsz *= strlen(pythonhome);
} else
bufsz = 0;
bufsz += strlen(PYTHONPATH) + 1;
bufsz += strlen(argv0_path) + 1;
#if defined(MS_WINDOWS)
if (userpath)
bufsz += strlen(userpath) + 1;
if (machinepath)
bufsz += strlen(machinepath) + 1;
bufsz += strlen(zip_path) + 1;
#endif
if (envpath != NULL)
bufsz += strlen(envpath) + 1;
module_search_path = buf = malloc(bufsz);
if (buf == NULL) {
fprintf(stderr, "Can't malloc dynamic PYTHONPATH.\n");
if (envpath) {
fprintf(stderr, "Using environment $PYTHONPATH.\n");
module_search_path = envpath;
} else {
fprintf(stderr, "Using default static path.\n");
module_search_path = PYTHONPATH;
}
#if defined(MS_WINDOWS)
if (machinepath)
free(machinepath);
if (userpath)
free(userpath);
#endif
return;
}
if (envpath) {
strcpy(buf, envpath);
buf = strchr(buf, '\0');
*buf++ = DELIM;
}
#if defined(MS_WINDOWS)
if (zip_path[0]) {
strcpy(buf, zip_path);
buf = strchr(buf, '\0');
*buf++ = DELIM;
}
if (userpath) {
strcpy(buf, userpath);
buf = strchr(buf, '\0');
*buf++ = DELIM;
free(userpath);
}
if (machinepath) {
strcpy(buf, machinepath);
buf = strchr(buf, '\0');
*buf++ = DELIM;
free(machinepath);
}
if (pythonhome == NULL) {
if (!skipdefault) {
strcpy(buf, PYTHONPATH);
buf = strchr(buf, '\0');
}
}
#else
if (pythonhome == NULL) {
strcpy(buf, PYTHONPATH);
buf = strchr(buf, '\0');
}
#endif
else {
char *p = PYTHONPATH;
char *q;
size_t n;
for (;;) {
q = strchr(p, DELIM);
if (q == NULL)
n = strlen(p);
else
n = q-p;
if (p[0] == '.' && is_sep(p[1])) {
strcpy(buf, pythonhome);
buf = strchr(buf, '\0');
p++;
n--;
}
strncpy(buf, p, n);
buf += n;
if (q == NULL)
break;
*buf++ = DELIM;
p = q+1;
}
}
if (argv0_path) {
*buf++ = DELIM;
strcpy(buf, argv0_path);
buf = strchr(buf, '\0');
}
*buf = '\0';
if (*prefix=='\0') {
char lookBuf[MAXPATHLEN+1];
char *look = buf - 1;
while (1) {
Py_ssize_t nchars;
char *lookEnd = look;
while (look >= module_search_path && *look != DELIM)
look--;
nchars = lookEnd-look;
strncpy(lookBuf, look+1, nchars);
lookBuf[nchars] = '\0';
reduce(lookBuf);
if (search_for_prefix(lookBuf, LANDMARK)) {
break;
}
if (look < module_search_path)
break;
look--;
}
}
}
char *
Py_GetPath(void) {
if (!module_search_path)
calculate_path();
return module_search_path;
}
char *
Py_GetPrefix(void) {
if (!module_search_path)
calculate_path();
return prefix;
}
char *
Py_GetExecPrefix(void) {
return Py_GetPrefix();
}
char *
Py_GetProgramFullPath(void) {
if (!module_search_path)
calculate_path();
return progpath;
}
