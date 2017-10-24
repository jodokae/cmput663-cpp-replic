#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <limits.h>
#include <mach-o/dyld.h>
#include <AvailabilityMacros.h>
#include "dlfcn.h"
#if defined(CTYPES_DARWIN_DLFCN)
#define ERR_STR_LEN 256
#if !defined(MAC_OS_X_VERSION_10_3)
#define MAC_OS_X_VERSION_10_3 1030
#endif
#if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_3
#define DARWIN_HAS_DLOPEN
extern void * dlopen(const char *path, int mode) __attribute__((weak_import));
extern void * dlsym(void * handle, const char *symbol) __attribute__((weak_import));
extern const char * dlerror(void) __attribute__((weak_import));
extern int dlclose(void * handle) __attribute__((weak_import));
extern int dladdr(const void *, Dl_info *) __attribute__((weak_import));
#endif
#if !defined(DARWIN_HAS_DLOPEN)
#define dlopen darwin_dlopen
#define dlsym darwin_dlsym
#define dlerror darwin_dlerror
#define dlclose darwin_dlclose
#define dladdr darwin_dladdr
#endif
void * (*ctypes_dlopen)(const char *path, int mode);
void * (*ctypes_dlsym)(void * handle, const char *symbol);
const char * (*ctypes_dlerror)(void);
int (*ctypes_dlclose)(void * handle);
int (*ctypes_dladdr)(const void *, Dl_info *);
#if MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_3
static void *dlsymIntern(void *handle, const char *symbol);
static const char *error(int setget, const char *str, ...);
static const char *error(int setget, const char *str, ...) {
static char errstr[ERR_STR_LEN];
static int err_filled = 0;
const char *retval;
va_list arg;
if (setget == 0) {
va_start(arg, str);
strncpy(errstr, "dlcompat: ", ERR_STR_LEN);
vsnprintf(errstr + 10, ERR_STR_LEN - 10, str, arg);
va_end(arg);
err_filled = 1;
retval = NULL;
} else {
if (!err_filled)
retval = NULL;
else
retval = errstr;
err_filled = 0;
}
return retval;
}
static void *darwin_dlopen(const char *path, int mode) {
void *module = 0;
NSObjectFileImage ofi = 0;
NSObjectFileImageReturnCode ofirc;
if (!path)
return (void *)-1;
ofirc = NSCreateObjectFileImageFromFile(path, &ofi);
switch (ofirc) {
case NSObjectFileImageSuccess:
module = NSLinkModule(ofi, path,
NSLINKMODULE_OPTION_RETURN_ON_ERROR
| (mode & RTLD_GLOBAL) ? 0 : NSLINKMODULE_OPTION_PRIVATE
| (mode & RTLD_LAZY) ? 0 : NSLINKMODULE_OPTION_BINDNOW);
NSDestroyObjectFileImage(ofi);
break;
case NSObjectFileImageInappropriateFile:
module = (void *)NSAddImage(path, NSADDIMAGE_OPTION_RETURN_ON_ERROR);
break;
default:
error(0, "Can not open \"%s\"", path);
return 0;
}
if (!module)
error(0, "Can not open \"%s\"", path);
return module;
}
static void *dlsymIntern(void *handle, const char *symbol) {
NSSymbol nssym = 0;
if (handle == (void *)-1) {
if (NSIsSymbolNameDefined(symbol)) {
nssym = NSLookupAndBindSymbol(symbol);
}
}
else {
if ((((struct mach_header *)handle)->magic == MH_MAGIC) ||
(((struct mach_header *)handle)->magic == MH_CIGAM)) {
if (NSIsSymbolNameDefinedInImage((struct mach_header *)handle, symbol)) {
nssym = NSLookupSymbolInImage((struct mach_header *)handle,
symbol,
NSLOOKUPSYMBOLINIMAGE_OPTION_BIND
| NSLOOKUPSYMBOLINIMAGE_OPTION_RETURN_ON_ERROR);
}
} else {
nssym = NSLookupSymbolInModule(handle, symbol);
}
}
if (!nssym) {
error(0, "Symbol \"%s\" Not found", symbol);
return NULL;
}
return NSAddressOfSymbol(nssym);
}
static const char *darwin_dlerror(void) {
return error(1, (char *)NULL);
}
static int darwin_dlclose(void *handle) {
if ((((struct mach_header *)handle)->magic == MH_MAGIC) ||
(((struct mach_header *)handle)->magic == MH_CIGAM)) {
error(0, "Can't remove dynamic libraries on darwin");
return 0;
}
if (!NSUnLinkModule(handle, 0)) {
error(0, "unable to unlink module %s", NSNameOfModule(handle));
return 1;
}
return 0;
}
static void *darwin_dlsym(void *handle, const char *symbol) {
static char undersym[257];
int sym_len = strlen(symbol);
void *value = NULL;
char *malloc_sym = NULL;
if (sym_len < 256) {
snprintf(undersym, 256, "_%s", symbol);
value = dlsymIntern(handle, undersym);
} else {
malloc_sym = malloc(sym_len + 2);
if (malloc_sym) {
sprintf(malloc_sym, "_%s", symbol);
value = dlsymIntern(handle, malloc_sym);
free(malloc_sym);
} else {
error(0, "Unable to allocate memory");
}
}
return value;
}
static int darwin_dladdr(const void *handle, Dl_info *info) {
return 0;
}
#endif
#if __GNUC__ < 4
#pragma CALL_ON_LOAD ctypes_dlfcn_init
#else
static void __attribute__ ((constructor)) ctypes_dlfcn_init(void);
static
#endif
void ctypes_dlfcn_init(void) {
if (dlopen != NULL) {
ctypes_dlsym = dlsym;
ctypes_dlopen = dlopen;
ctypes_dlerror = dlerror;
ctypes_dlclose = dlclose;
ctypes_dladdr = dladdr;
} else {
#if MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_3
ctypes_dlsym = darwin_dlsym;
ctypes_dlopen = darwin_dlopen;
ctypes_dlerror = darwin_dlerror;
ctypes_dlclose = darwin_dlclose;
ctypes_dladdr = darwin_dladdr;
#endif
}
}
#endif