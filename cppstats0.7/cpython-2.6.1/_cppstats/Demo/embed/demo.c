#include "Python.h"
void initxyzzy(void);
main(int argc, char **argv) {
Py_SetProgramName(argv[0]);
Py_Initialize();
initxyzzy();
PySys_SetArgv(argc, argv);
printf("Hello, brave new world\n\n");
PyRun_SimpleString("import sys\n");
PyRun_SimpleString("print sys.builtin_module_names\n");
PyRun_SimpleString("print sys.modules.keys()\n");
PyRun_SimpleString("print sys.executable\n");
PyRun_SimpleString("print sys.argv\n");
printf("\nGoodbye, cruel world\n");
Py_Exit(0);
}
static PyObject *
xyzzy_foo(PyObject *self, PyObject* args) {
return PyInt_FromLong(42L);
}
static PyMethodDef xyzzy_methods[] = {
{
"foo", xyzzy_foo, METH_NOARGS,
"Return the meaning of everything."
},
{NULL, NULL}
};
void
initxyzzy(void) {
PyImport_AddModule("xyzzy");
Py_InitModule("xyzzy", xyzzy_methods);
}
