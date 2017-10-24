#include "Python.h"
#include "importdl.h"
#include "dlk.h"
const struct filedescr _PyImport_DynLoadFiletab[] = {
{"/pyd", "rb", C_EXTENSION},
{0, 0}
};
void dynload_init_dummy() {
}
dl_funcptr _PyImport_GetDynLoadFunc(const char *fqname, const char *shortname,
char *pathname, FILE *fp) {
int err;
char errstr[256];
void (*init_function)(void);
err = dlk_load_no_init(pathname, &init_function);
if (err) {
PyOS_snprintf(errstr, sizeof(errstr), "dlk failure %d", err);
PyErr_SetString(PyExc_ImportError, errstr);
}
return init_function;
}
