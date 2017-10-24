#include "Python.h"
#include "osdefs.h"
#include <sys/types.h>
#include <string.h>
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif
#if defined(__cplusplus)
extern "C" {
#endif
#if !defined(VERSION)
#define VERSION "2.1"
#endif
#if !defined(VPATH)
#define VPATH "."
#endif
#if !defined(PREFIX)
#if defined(__VMS)
#define PREFIX ""
#else
#define PREFIX "/usr/local"
#endif
#endif
#if !defined(EXEC_PREFIX)
#define EXEC_PREFIX PREFIX
#endif
#if !defined(PYTHONPATH)
#define PYTHONPATH PREFIX "/lib/python" VERSION ":" EXEC_PREFIX "/lib/python" VERSION "/lib-dynload"
#endif
#if !defined(LANDMARK)
#define LANDMARK "os.py"
#endif
static char prefix[MAXPATHLEN+1];
static char exec_prefix[MAXPATHLEN+1];
static char progpath[MAXPATHLEN+1];
static char *module_search_path = NULL;
static char lib_python[] = "lib/python" VERSION;
static void
reduce(char *dir) {
size_t i = strlen(dir);
while (i > 0 && dir[i] != SEP)
--i;
dir[i] = '\0';
}
static int
isfile(char *filename) {
struct stat buf;
if (stat(filename, &buf) != 0)
return 0;
if (!S_ISREG(buf.st_mode))
return 0;
return 1;
}
static int
ismodule(char *filename) {
if (isfile(filename))
return 1;
if (strlen(filename) < MAXPATHLEN) {
strcat(filename, Py_OptimizeFlag ? "o" : "c");
if (isfile(filename))
return 1;
}
return 0;
}
static int
isxfile(char *filename) {
struct stat buf;
if (stat(filename, &buf) != 0)
return 0;
if (!S_ISREG(buf.st_mode))
return 0;
if ((buf.st_mode & 0111) == 0)
return 0;
return 1;
}
static int
isdir(char *filename) {
struct stat buf;
if (stat(filename, &buf) != 0)
return 0;
if (!S_ISDIR(buf.st_mode))
return 0;
return 1;
}
static void
joinpath(char *buffer, char *stuff) {
size_t n, k;
if (stuff[0] == SEP)
n = 0;
else {
n = strlen(buffer);
if (n > 0 && buffer[n-1] != SEP && n < MAXPATHLEN)
buffer[n++] = SEP;
}
if (n > MAXPATHLEN)
Py_FatalError("buffer overflow in getpath.c's joinpath()");
k = strlen(stuff);
if (n + k > MAXPATHLEN)
k = MAXPATHLEN - n;
strncpy(buffer+n, stuff, k);
buffer[n+k] = '\0';
}
static void
copy_absolute(char *path, char *p) {
if (p[0] == SEP)
strcpy(path, p);
else {
getcwd(path, MAXPATHLEN);
if (p[0] == '.' && p[1] == SEP)
p += 2;
joinpath(path, p);
}
}
static void
absolutize(char *path) {
char buffer[MAXPATHLEN + 1];
if (path[0] == SEP)
return;
copy_absolute(buffer, path);
strcpy(path, buffer);
}
static int
search_for_prefix(char *argv0_path, char *home) {
size_t n;
char *vpath;
if (home) {
char *delim;
strncpy(prefix, home, MAXPATHLEN);
delim = strchr(prefix, DELIM);
if (delim)
*delim = '\0';
joinpath(prefix, lib_python);
joinpath(prefix, LANDMARK);
return 1;
}
strcpy(prefix, argv0_path);
joinpath(prefix, "Modules/Setup");
if (isfile(prefix)) {
vpath = VPATH;
strcpy(prefix, argv0_path);
joinpath(prefix, vpath);
joinpath(prefix, "Lib");
joinpath(prefix, LANDMARK);
if (ismodule(prefix))
return -1;
}
copy_absolute(prefix, argv0_path);
do {
n = strlen(prefix);
joinpath(prefix, lib_python);
joinpath(prefix, LANDMARK);
if (ismodule(prefix))
return 1;
prefix[n] = '\0';
reduce(prefix);
} while (prefix[0]);
strncpy(prefix, PREFIX, MAXPATHLEN);
joinpath(prefix, lib_python);
joinpath(prefix, LANDMARK);
if (ismodule(prefix))
return 1;
return 0;
}
static int
search_for_exec_prefix(char *argv0_path, char *home) {
size_t n;
if (home) {
char *delim;
delim = strchr(home, DELIM);
if (delim)
strncpy(exec_prefix, delim+1, MAXPATHLEN);
else
strncpy(exec_prefix, home, MAXPATHLEN);
joinpath(exec_prefix, lib_python);
joinpath(exec_prefix, "lib-dynload");
return 1;
}
strcpy(exec_prefix, argv0_path);
joinpath(exec_prefix, "Modules/Setup");
if (isfile(exec_prefix)) {
reduce(exec_prefix);
return -1;
}
copy_absolute(exec_prefix, argv0_path);
do {
n = strlen(exec_prefix);
joinpath(exec_prefix, lib_python);
joinpath(exec_prefix, "lib-dynload");
if (isdir(exec_prefix))
return 1;
exec_prefix[n] = '\0';
reduce(exec_prefix);
} while (exec_prefix[0]);
strncpy(exec_prefix, EXEC_PREFIX, MAXPATHLEN);
joinpath(exec_prefix, lib_python);
joinpath(exec_prefix, "lib-dynload");
if (isdir(exec_prefix))
return 1;
return 0;
}
static void
calculate_path(void) {
extern char *Py_GetProgramName(void);
static char delimiter[2] = {DELIM, '\0'};
static char separator[2] = {SEP, '\0'};
char *pythonpath = PYTHONPATH;
char *rtpypath = Py_GETENV("PYTHONPATH");
char *home = Py_GetPythonHome();
char *path = getenv("PATH");
char *prog = Py_GetProgramName();
char argv0_path[MAXPATHLEN+1];
char zip_path[MAXPATHLEN+1];
int pfound, efound;
char *buf;
size_t bufsz;
size_t prefixsz;
char *defpath = pythonpath;
#if defined(WITH_NEXT_FRAMEWORK)
NSModule pythonModule;
#endif
#if defined(__APPLE__)
#if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_4
uint32_t nsexeclength = MAXPATHLEN;
#else
unsigned long nsexeclength = MAXPATHLEN;
#endif
#endif
if (strchr(prog, SEP))
strncpy(progpath, prog, MAXPATHLEN);
#if defined(__APPLE__)
else if(0 == _NSGetExecutablePath(progpath, &nsexeclength) && progpath[0] == SEP)
;
#endif
else if (path) {
while (1) {
char *delim = strchr(path, DELIM);
if (delim) {
size_t len = delim - path;
if (len > MAXPATHLEN)
len = MAXPATHLEN;
strncpy(progpath, path, len);
*(progpath + len) = '\0';
} else
strncpy(progpath, path, MAXPATHLEN);
joinpath(progpath, prog);
if (isxfile(progpath))
break;
if (!delim) {
progpath[0] = '\0';
break;
}
path = delim + 1;
}
} else
progpath[0] = '\0';
if (progpath[0] != SEP)
absolutize(progpath);
strncpy(argv0_path, progpath, MAXPATHLEN);
argv0_path[MAXPATHLEN] = '\0';
#if defined(WITH_NEXT_FRAMEWORK)
pythonModule = NSModuleForSymbol(NSLookupAndBindSymbol("_Py_Initialize"));
buf = (char *)NSLibraryNameForModule(pythonModule);
if (buf != NULL) {
strncpy(argv0_path, buf, MAXPATHLEN);
reduce(argv0_path);
joinpath(argv0_path, lib_python);
joinpath(argv0_path, LANDMARK);
if (!ismodule(argv0_path)) {
strncpy(argv0_path, prog, MAXPATHLEN);
} else {
strncpy(argv0_path, buf, MAXPATHLEN);
}
}
#endif
#if HAVE_READLINK
{
char tmpbuffer[MAXPATHLEN+1];
int linklen = readlink(progpath, tmpbuffer, MAXPATHLEN);
while (linklen != -1) {
tmpbuffer[linklen] = '\0';
if (tmpbuffer[0] == SEP)
strncpy(argv0_path, tmpbuffer, MAXPATHLEN);
else {
reduce(argv0_path);
joinpath(argv0_path, tmpbuffer);
}
linklen = readlink(argv0_path, tmpbuffer, MAXPATHLEN);
}
}
#endif
reduce(argv0_path);
if (!(pfound = search_for_prefix(argv0_path, home))) {
if (!Py_FrozenFlag)
fprintf(stderr,
"Could not find platform independent libraries <prefix>\n");
strncpy(prefix, PREFIX, MAXPATHLEN);
joinpath(prefix, lib_python);
} else
reduce(prefix);
strncpy(zip_path, prefix, MAXPATHLEN);
zip_path[MAXPATHLEN] = '\0';
if (pfound > 0) {
reduce(zip_path);
reduce(zip_path);
} else
strncpy(zip_path, PREFIX, MAXPATHLEN);
joinpath(zip_path, "lib/python00.zip");
bufsz = strlen(zip_path);
zip_path[bufsz - 6] = VERSION[0];
zip_path[bufsz - 5] = VERSION[2];
if (!(efound = search_for_exec_prefix(argv0_path, home))) {
if (!Py_FrozenFlag)
fprintf(stderr,
"Could not find platform dependent libraries <exec_prefix>\n");
strncpy(exec_prefix, EXEC_PREFIX, MAXPATHLEN);
joinpath(exec_prefix, "lib/lib-dynload");
}
if ((!pfound || !efound) && !Py_FrozenFlag)
fprintf(stderr,
"Consider setting $PYTHONHOME to <prefix>[:<exec_prefix>]\n");
bufsz = 0;
if (rtpypath)
bufsz += strlen(rtpypath) + 1;
prefixsz = strlen(prefix) + 1;
while (1) {
char *delim = strchr(defpath, DELIM);
if (defpath[0] != SEP)
bufsz += prefixsz;
if (delim)
bufsz += delim - defpath + 1;
else {
bufsz += strlen(defpath) + 1;
break;
}
defpath = delim + 1;
}
bufsz += strlen(zip_path) + 1;
bufsz += strlen(exec_prefix) + 1;
buf = (char *)PyMem_Malloc(bufsz);
if (buf == NULL) {
fprintf(stderr, "Not enough memory for dynamic PYTHONPATH.\n");
fprintf(stderr, "Using default static PYTHONPATH.\n");
module_search_path = PYTHONPATH;
} else {
if (rtpypath) {
strcpy(buf, rtpypath);
strcat(buf, delimiter);
} else
buf[0] = '\0';
strcat(buf, zip_path);
strcat(buf, delimiter);
defpath = pythonpath;
while (1) {
char *delim = strchr(defpath, DELIM);
if (defpath[0] != SEP) {
strcat(buf, prefix);
strcat(buf, separator);
}
if (delim) {
size_t len = delim - defpath + 1;
size_t end = strlen(buf) + len;
strncat(buf, defpath, len);
*(buf + end) = '\0';
} else {
strcat(buf, defpath);
break;
}
defpath = delim + 1;
}
strcat(buf, delimiter);
strcat(buf, exec_prefix);
module_search_path = buf;
}
if (pfound > 0) {
reduce(prefix);
reduce(prefix);
if (!prefix[0])
strcpy(prefix, separator);
} else
strncpy(prefix, PREFIX, MAXPATHLEN);
if (efound > 0) {
reduce(exec_prefix);
reduce(exec_prefix);
reduce(exec_prefix);
if (!exec_prefix[0])
strcpy(exec_prefix, separator);
} else
strncpy(exec_prefix, EXEC_PREFIX, MAXPATHLEN);
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
#if defined(__cplusplus)
}
#endif