#include "Python.h"
#include "osdefs.h"
#if defined(MS_WIN32)
#include <windows.h>
extern BOOL PyWin_IsWin32s(void);
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if !defined(LANDMARK)
#define LANDMARK "lib\\os.py"
#endif
static char prefix[MAXPATHLEN+1];
static char exec_prefix[MAXPATHLEN+1];
static char progpath[MAXPATHLEN+1];
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
int i = strlen(dir);
while (i > 0 && !is_sep(dir[i]))
--i;
dir[i] = '\0';
}
static int
exists(char *filename) {
struct stat buf;
return stat(filename, &buf) == 0;
}
static void
join(char *buffer, char *stuff) {
int n, k;
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
search_for_prefix(char *argv0_path, char *landmark) {
int n;
strcpy(prefix, argv0_path);
do {
n = strlen(prefix);
join(prefix, landmark);
if (exists(prefix)) {
prefix[n] = '\0';
return 1;
}
prefix[n] = '\0';
reduce(prefix);
} while (prefix[0]);
return 0;
}
#if defined(MS_WIN32)
#include "malloc.h"
extern const char *PyWin_DLLVersionString;
static char *
getpythonregpath(HKEY keyBase, BOOL bWin32s) {
HKEY newKey = 0;
DWORD nameSize = 0;
DWORD dataSize = 0;
DWORD numEntries = 0;
LONG rc;
char *retval = NULL;
char *dataBuf;
const char keyPrefix[] = "Software\\Python\\PythonCore\\";
const char keySuffix[] = "\\PythonPath";
int versionLen;
char *keyBuf;
versionLen = strlen(PyWin_DLLVersionString);
keyBuf = alloca(sizeof(keyPrefix)-1 + versionLen + sizeof(keySuffix));
memcpy(keyBuf, keyPrefix, sizeof(keyPrefix)-1);
memcpy(keyBuf+sizeof(keyPrefix)-1, PyWin_DLLVersionString, versionLen);
memcpy(keyBuf+sizeof(keyPrefix)-1+versionLen, keySuffix, sizeof(keySuffix));
rc=RegOpenKey(keyBase,
keyBuf,
&newKey);
if (rc==ERROR_SUCCESS) {
RegQueryInfoKey(newKey, NULL, NULL, NULL, NULL, NULL, NULL,
&numEntries, &nameSize, &dataSize, NULL, NULL);
}
if (bWin32s && numEntries==0 && dataSize==0) {
numEntries = 1;
dataSize = 511;
}
if (numEntries) {
char keyBuf[MAX_PATH+1];
int index = 0;
int off = 0;
for(index=0;; index++) {
long reqdSize = 0;
DWORD rc = RegEnumKey(newKey,
index, keyBuf, MAX_PATH+1);
if (rc) break;
rc = RegQueryValue(newKey, keyBuf, NULL, &reqdSize);
if (rc) break;
if (bWin32s && reqdSize==0) reqdSize = 512;
dataSize += reqdSize + 1;
}
dataBuf = malloc(dataSize+1);
if (dataBuf==NULL)
return NULL;
for(index=0;; index++) {
int adjust;
long reqdSize = dataSize;
DWORD rc = RegEnumKey(newKey,
index, keyBuf,MAX_PATH+1);
if (rc) break;
rc = RegQueryValue(newKey,
keyBuf, dataBuf+off, &reqdSize);
if (rc) break;
if (reqdSize>1) {
adjust = strlen(dataBuf+off);
dataSize -= adjust;
off += adjust;
dataBuf[off++] = ';';
dataBuf[off] = '\0';
dataSize--;
}
}
rc = RegQueryValue(newKey, "", dataBuf+off, &dataSize);
if (rc==ERROR_SUCCESS) {
if (strlen(dataBuf)==0)
free(dataBuf);
else
retval = dataBuf;
} else
free(dataBuf);
}
if (newKey)
RegCloseKey(newKey);
return retval;
}
#endif
static void
get_progpath(void) {
extern char *Py_GetProgramName(void);
char *path = getenv("PATH");
char *prog = Py_GetProgramName();
#if defined(MS_WIN32)
if (GetModuleFileName(NULL, progpath, MAXPATHLEN))
return;
#endif
if (prog == NULL || *prog == '\0')
prog = "python";
#if defined(ALTSEP)
if (strchr(prog, SEP) || strchr(prog, ALTSEP))
#else
if (strchr(prog, SEP))
#endif
strcpy(progpath, prog);
else if (path) {
while (1) {
char *delim = strchr(path, DELIM);
if (delim) {
int len = delim - path;
strncpy(progpath, path, len);
*(progpath + len) = '\0';
} else
strcpy(progpath, path);
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
int bufsz;
char *pythonhome = Py_GetPythonHome();
char *envpath = Py_GETENV("PYTHONPATH");
#if defined(MS_WIN32)
char *machinepath, *userpath;
if (PyWin_IsWin32s()) {
machinepath = getpythonregpath(HKEY_CLASSES_ROOT, TRUE);
userpath = NULL;
} else {
machinepath = getpythonregpath(HKEY_LOCAL_MACHINE, FALSE);
userpath = getpythonregpath(HKEY_CURRENT_USER, FALSE);
}
#endif
get_progpath();
strcpy(argv0_path, progpath);
reduce(argv0_path);
if (pythonhome == NULL || *pythonhome == '\0') {
if (search_for_prefix(argv0_path, LANDMARK))
pythonhome = prefix;
else
pythonhome = NULL;
} else {
char *delim;
strcpy(prefix, pythonhome);
delim = strchr(prefix, DELIM);
if (delim) {
*delim = '\0';
strcpy(exec_prefix, delim+1);
} else
strcpy(exec_prefix, EXEC_PREFIX);
}
if (envpath && *envpath == '\0')
envpath = NULL;
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
if (envpath != NULL)
bufsz += strlen(envpath) + 1;
bufsz += strlen(argv0_path) + 1;
#if defined(MS_WIN32)
if (machinepath)
bufsz += strlen(machinepath) + 1;
if (userpath)
bufsz += strlen(userpath) + 1;
#endif
module_search_path = buf = malloc(bufsz);
if (buf == NULL) {
fprintf(stderr, "Can't malloc dynamic PYTHONPATH.\n");
if (envpath) {
fprintf(stderr, "Using default static $PYTHONPATH.\n");
module_search_path = envpath;
} else {
fprintf(stderr, "Using environment $PYTHONPATH.\n");
module_search_path = PYTHONPATH;
}
return;
}
if (envpath) {
strcpy(buf, envpath);
buf = strchr(buf, '\0');
*buf++ = DELIM;
}
#if defined(MS_WIN32)
if (machinepath) {
strcpy(buf, machinepath);
buf = strchr(buf, '\0');
*buf++ = DELIM;
}
if (userpath) {
strcpy(buf, userpath);
buf = strchr(buf, '\0');
*buf++ = DELIM;
}
#endif
if (pythonhome == NULL) {
strcpy(buf, PYTHONPATH);
buf = strchr(buf, '\0');
} else {
char *p = PYTHONPATH;
char *q;
int n;
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
if (!module_search_path)
calculate_path();
return exec_prefix;
}
char *
Py_GetProgramFullPath(void) {
if (!module_search_path)
calculate_path();
return progpath;
}