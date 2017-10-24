#include <Python.h>
typedef struct {
PyObject_HEAD
} noddy_NoddyObject;
static PyTypeObject noddy_NoddyType = {
PyObject_HEAD_INIT(NULL)
0,
"noddy.Noddy",
sizeof(noddy_NoddyObject),
0,
0,
0,
0,
0,
0,
0,
0,
0,
0,
0,
0,
0,
0,
0,
0,
Py_TPFLAGS_DEFAULT,
"Noddy objects",
};
static PyMethodDef noddy_methods[] = {
{NULL}
};
#if !defined(PyMODINIT_FUNC)
#define PyMODINIT_FUNC void
#endif
PyMODINIT_FUNC
initnoddy(void) {
PyObject* m;
noddy_NoddyType.tp_new = PyType_GenericNew;
if (PyType_Ready(&noddy_NoddyType) < 0)
return;
m = Py_InitModule3("noddy", noddy_methods,
"Example module that creates an extension type.");
Py_INCREF(&noddy_NoddyType);
PyModule_AddObject(m, "Noddy", (PyObject *)&noddy_NoddyType);
}