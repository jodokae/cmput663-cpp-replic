#include "Python.h"
#include "pymactoolbox.h"
#include <Carbon/Carbon.h>
static PyObject *
gestalt_gestalt(PyObject *self, PyObject *args) {
OSErr iErr;
OSType selector;
SInt32 response;
if (!PyArg_ParseTuple(args, "O&", PyMac_GetOSType, &selector))
return NULL;
iErr = Gestalt ( selector, &response );
if (iErr != 0)
return PyMac_Error(iErr);
return PyInt_FromLong(response);
}
static struct PyMethodDef gestalt_methods[] = {
{"gestalt", gestalt_gestalt, METH_VARARGS},
{NULL, NULL}
};
void
initgestalt(void) {
Py_InitModule("gestalt", gestalt_methods);
}
