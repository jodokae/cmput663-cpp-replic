#include <Python.h>
typedef struct {
PyListObject list;
int state;
} Shoddy;
static PyObject *
Shoddy_increment(Shoddy *self, PyObject *unused) {
self->state++;
return PyInt_FromLong(self->state);
}
static PyMethodDef Shoddy_methods[] = {
{
"increment", (PyCFunction)Shoddy_increment, METH_NOARGS,
PyDoc_STR("increment state counter")
},
{NULL, NULL},
};
static int
Shoddy_init(Shoddy *self, PyObject *args, PyObject *kwds) {
if (PyList_Type.tp_init((PyObject *)self, args, kwds) < 0)
return -1;
self->state = 0;
return 0;
}
static PyTypeObject ShoddyType = {
PyObject_HEAD_INIT(NULL)
0,
"shoddy.Shoddy",
sizeof(Shoddy),
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
Py_TPFLAGS_DEFAULT |
Py_TPFLAGS_BASETYPE,
0,
0,
0,
0,
0,
0,
0,
Shoddy_methods,
0,
0,
0,
0,
0,
0,
0,
(initproc)Shoddy_init,
0,
0,
};
PyMODINIT_FUNC
initshoddy(void) {
PyObject *m;
ShoddyType.tp_base = &PyList_Type;
if (PyType_Ready(&ShoddyType) < 0)
return;
m = Py_InitModule3("shoddy", NULL, "Shoddy module");
if (m == NULL)
return;
Py_INCREF(&ShoddyType);
PyModule_AddObject(m, "Shoddy", (PyObject *) &ShoddyType);
}