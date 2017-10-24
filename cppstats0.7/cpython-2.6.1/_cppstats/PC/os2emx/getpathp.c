#include "Python.h"
#include "osdefs.h"
#if !defined(PYOS_OS2)
#error This file only compilable on OS/2
#endif
#define INCL_DOS
#include <os2.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if !defined(LANDMARK)
#if defined(PYCC_GCC)
#define LANDMARK "lib/os.py"
#else
#define LANDMARK "lib\\os.py"
#endif
#endif
static char prefix[MAXPATHLEN+1];
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
int n, ok;
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
static void
get_progpath(void) {
extern char *Py_GetProgramName(void);
char *path = getenv("PATH");
char *prog = Py_GetProgramName();
PPIB pib;
if ((DosGetInfoBlocks(NULL, &pib) == 0) &&
(DosQueryModuleName(pib->pib_hmte, sizeof(progpath), progpath) == 0))
return;
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
#if !defined(PYCC_GCC)
len = min(MAXPATHLEN,len);
#else
len = MAXPATHLEN < len ? MAXPATHLEN : len;
#endif
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
char *envpath = getenv("PYTHONPATH");
char zip_path[MAXPATHLEN+1];
size_t len;
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
bufsz += strlen(zip_path) + 1;
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
return;
}
if (envpath) {
strcpy(buf, envpath);
buf = strchr(buf, '\0');
*buf++ = DELIM;
}
if (zip_path[0]) {
strcpy(buf, zip_path);
buf = strchr(buf, '\0');
*buf++ = DELIM;
}
if (pythonhome == NULL) {
strcpy(buf, PYTHONPATH);
buf = strchr(buf, '\0');
} else {
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
