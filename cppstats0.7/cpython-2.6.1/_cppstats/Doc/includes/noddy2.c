#include <Python.h>
#include "structmember.h"
typedef struct {
PyObject_HEAD
PyObject *first;
PyObject *last;
int number;
} Noddy;
static void
Noddy_dealloc(Noddy* self) {
Py_XDECREF(self->first);
Py_XDECREF(self->last);
self->ob_type->tp_free((PyObject*)self);
}
static PyObject *
Noddy_new(PyTypeObject *type, PyObject *args, PyObject *kwds) {
Noddy *self;
self = (Noddy *)type->tp_alloc(type, 0);
if (self != NULL) {
self->first = PyString_FromString("");
if (self->first == NULL) {
Py_DECREF(self);
return NULL;
}
self->last = PyString_FromString("");
if (self->last == NULL) {
Py_DECREF(self);
return NULL;
}
self->number = 0;
}
return (PyObject *)self;
}
static int
Noddy_init(Noddy *self, PyObject *args, PyObject *kwds) {
PyObject *first=NULL, *last=NULL, *tmp;
static char *kwlist[] = {"first", "last", "number", NULL};
if (! PyArg_ParseTupleAndKeywords(args, kwds, "|OOi", kwlist,
&first, &last,
&self->number))
return -1;
if (first) {
tmp = self->first;
Py_INCREF(first);
self->first = first;
Py_XDECREF(tmp);
}
if (last) {
tmp = self->last;
Py_INCREF(last);
self->last = last;
Py_XDECREF(tmp);
}
return 0;
}
static PyMemberDef Noddy_members[] = {
{
"first", T_OBJECT_EX, offsetof(Noddy, first), 0,
"first name"
},
{
"last", T_OBJECT_EX, offsetof(Noddy, last), 0,
"last name"
},
{
"number", T_INT, offsetof(Noddy, number), 0,
"noddy number"
},
{NULL}
};
static PyObject *
Noddy_name(Noddy* self) {
static PyObject *format = NULL;
PyObject *args, *result;
if (format == NULL) {
format = PyString_FromString("%s %s");
if (format == NULL)
return NULL;
}
if (self->first == NULL) {
PyErr_SetString(PyExc_AttributeError, "first");
return NULL;
}
if (self->last == NULL) {
PyErr_SetString(PyExc_AttributeError, "last");
return NULL;
}
args = Py_BuildValue("OO", self->first, self->last);
if (args == NULL)
return NULL;
result = PyString_Format(format, args);
Py_DECREF(args);
return result;
}
static PyMethodDef Noddy_methods[] = {
{
"name", (PyCFunction)Noddy_name, METH_NOARGS,
"Return the name, combining the first and last name"
},
{NULL}
};
static PyTypeObject NoddyType = {
PyObject_HEAD_INIT(NULL)
0,
"noddy.Noddy",
sizeof(Noddy),
0,
(destructor)Noddy_dealloc,
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
Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
"Noddy objects",
0,
0,
0,
0,
0,
0,
Noddy_methods,
Noddy_members,
0,
0,
0,
0,
0,
0,
(initproc)Noddy_init,
0,
Noddy_new,
};
static PyMethodDef module_methods[] = {
{NULL}
};
#if !defined(PyMODINIT_FUNC)
#define PyMODINIT_FUNC void
#endif
PyMODINIT_FUNC
initnoddy2(void) {
PyObject* m;
if (PyType_Ready(&NoddyType) < 0)
return;
m = Py_InitModule3("noddy2", module_methods,
"Example module that creates an extension type.");
if (m == NULL)
return;
Py_INCREF(&NoddyType);
PyModule_AddObject(m, "Noddy", (PyObject *)&NoddyType);
}