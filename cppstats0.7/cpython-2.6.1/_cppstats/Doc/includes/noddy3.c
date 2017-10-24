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
if (! PyArg_ParseTupleAndKeywords(args, kwds, "|SSi", kwlist,
&first, &last,
&self->number))
return -1;
if (first) {
tmp = self->first;
Py_INCREF(first);
self->first = first;
Py_DECREF(tmp);
}
if (last) {
tmp = self->last;
Py_INCREF(last);
self->last = last;
Py_DECREF(tmp);
}
return 0;
}
static PyMemberDef Noddy_members[] = {
{
"number", T_INT, offsetof(Noddy, number), 0,
"noddy number"
},
{NULL}
};
static PyObject *
Noddy_getfirst(Noddy *self, void *closure) {
Py_INCREF(self->first);
return self->first;
}
static int
Noddy_setfirst(Noddy *self, PyObject *value, void *closure) {
if (value == NULL) {
PyErr_SetString(PyExc_TypeError, "Cannot delete the first attribute");
return -1;
}
if (! PyString_Check(value)) {
PyErr_SetString(PyExc_TypeError,
"The first attribute value must be a string");
return -1;
}
Py_DECREF(self->first);
Py_INCREF(value);
self->first = value;
return 0;
}
static PyObject *
Noddy_getlast(Noddy *self, void *closure) {
Py_INCREF(self->last);
return self->last;
}
static int
Noddy_setlast(Noddy *self, PyObject *value, void *closure) {
if (value == NULL) {
PyErr_SetString(PyExc_TypeError, "Cannot delete the last attribute");
return -1;
}
if (! PyString_Check(value)) {
PyErr_SetString(PyExc_TypeError,
"The last attribute value must be a string");
return -1;
}
Py_DECREF(self->last);
Py_INCREF(value);
self->last = value;
return 0;
}
static PyGetSetDef Noddy_getseters[] = {
{
"first",
(getter)Noddy_getfirst, (setter)Noddy_setfirst,
"first name",
NULL
},
{
"last",
(getter)Noddy_getlast, (setter)Noddy_setlast,
"last name",
NULL
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
Noddy_getseters,
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
initnoddy3(void) {
PyObject* m;
if (PyType_Ready(&NoddyType) < 0)
return;
m = Py_InitModule3("noddy3", module_methods,
"Example module that creates an extension type.");
if (m == NULL)
return;
Py_INCREF(&NoddyType);
PyModule_AddObject(m, "Noddy", (PyObject *)&NoddyType);
}