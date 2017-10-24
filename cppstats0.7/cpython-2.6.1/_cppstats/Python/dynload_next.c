#include "Python.h"
#include "importdl.h"
#include <mach-o/dyld.h>
const struct filedescr _PyImport_DynLoadFiletab[] = {
{".so", "rb", C_EXTENSION},
{"module.so", "rb", C_EXTENSION},
{0, 0}
};
#if defined(USE_DYLD_GLOBAL_NAMESPACE)
#define LINKOPTIONS NSLINKMODULE_OPTION_BINDNOW|NSLINKMODULE_OPTION_RETURN_ON_ERROR
#else
#define LINKOPTIONS NSLINKMODULE_OPTION_BINDNOW| NSLINKMODULE_OPTION_RETURN_ON_ERROR|NSLINKMODULE_OPTION_PRIVATE
#endif
dl_funcptr _PyImport_GetDynLoadFunc(const char *fqname, const char *shortname,
const char *pathname, FILE *fp) {
dl_funcptr p = NULL;
char funcname[258];
NSObjectFileImageReturnCode rc;
NSObjectFileImage image;
NSModule newModule;
NSSymbol theSym;
const char *errString;
char errBuf[512];
PyOS_snprintf(funcname, sizeof(funcname), "_init%.200s", shortname);
#if defined(USE_DYLD_GLOBAL_NAMESPACE)
if (NSIsSymbolNameDefined(funcname)) {
theSym = NSLookupAndBindSymbol(funcname);
p = (dl_funcptr)NSAddressOfSymbol(theSym);
return p;
}
#endif
rc = NSCreateObjectFileImageFromFile(pathname, &image);
switch(rc) {
default:
case NSObjectFileImageFailure:
case NSObjectFileImageFormat:
errString = "Can't create object file image";
break;
case NSObjectFileImageSuccess:
errString = NULL;
break;
case NSObjectFileImageInappropriateFile:
errString = "Inappropriate file type for dynamic loading";
break;
case NSObjectFileImageArch:
errString = "Wrong CPU type in object file";
break;
case NSObjectFileImageAccess:
errString = "Can't read object file (no access)";
break;
}
if (errString == NULL) {
newModule = NSLinkModule(image, pathname, LINKOPTIONS);
if (newModule == NULL) {
int errNo;
const char *fileName, *moreErrorStr;
NSLinkEditErrors c;
NSLinkEditError( &c, &errNo, &fileName, &moreErrorStr );
PyOS_snprintf(errBuf, 512, "Failure linking new module: %s: %s",
fileName, moreErrorStr);
errString = errBuf;
}
}
if (errString != NULL) {
PyErr_SetString(PyExc_ImportError, errString);
return NULL;
}
#if defined(USE_DYLD_GLOBAL_NAMESPACE)
if (!NSIsSymbolNameDefined(funcname)) {
PyErr_Format(PyExc_ImportError,
"Loaded module does not contain symbol %.200s",
funcname);
return NULL;
}
theSym = NSLookupAndBindSymbol(funcname);
#else
theSym = NSLookupSymbolInModule(newModule, funcname);
if ( theSym == NULL ) {
PyErr_Format(PyExc_ImportError,
"Loaded module does not contain symbol %.200s",
funcname);
return NULL;
}
#endif
p = (dl_funcptr)NSAddressOfSymbol(theSym);
return p;
}
