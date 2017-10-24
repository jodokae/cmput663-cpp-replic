#include "Python.h"
static PyObject *ErrorObject;
typedef struct {
PyObject_HEAD
PyObject *x_attr;
} XxoObject;
static PyTypeObject Xxo_Type;
#define XxoObject_Check(v) (Py_TYPE(v) == &Xxo_Type)
static XxoObject *
newXxoObject(PyObject *arg) {
XxoObject *self;
self = PyObject_New(XxoObject, &Xxo_Type);
if (self == NULL)
return NULL;
self->x_attr = NULL;
return self;
}
static void
Xxo_dealloc(XxoObject *self) {
Py_XDECREF(self->x_attr);
PyObject_Del(self);
}
static PyObject *
Xxo_demo(XxoObject *self, PyObject *args) {
if (!PyArg_ParseTuple(args, ":demo"))
return NULL;
Py_INCREF(Py_None);
return Py_None;
}
static PyMethodDef Xxo_methods[] = {
{
"demo", (PyCFunction)Xxo_demo, METH_VARARGS,
PyDoc_STR("demo() -> None")
},
{NULL, NULL}
};
static PyObject *
Xxo_getattr(XxoObject *self, char *name) {
if (self->x_attr != NULL) {
PyObject *v = PyDict_GetItemString(self->x_attr, name);
if (v != NULL) {
Py_INCREF(v);
return v;
}
}
return Py_FindMethod(Xxo_methods, (PyObject *)self, name);
}
static int
Xxo_setattr(XxoObject *self, char *name, PyObject *v) {
if (self->x_attr == NULL) {
self->x_attr = PyDict_New();
if (self->x_attr == NULL)
return -1;
}
if (v == NULL) {
int rv = PyDict_DelItemString(self->x_attr, name);
if (rv < 0)
PyErr_SetString(PyExc_AttributeError,
"delete non-existing Xxo attribute");
return rv;
} else
return PyDict_SetItemString(self->x_attr, name, v);
}
static PyTypeObject Xxo_Type = {
PyVarObject_HEAD_INIT(NULL, 0)
"xxmodule.Xxo",
sizeof(XxoObject),
0,
(destructor)Xxo_dealloc,
0,
(getattrfunc)Xxo_getattr,
(setattrfunc)Xxo_setattr,
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
0,
0,
0,
0,
};
PyDoc_STRVAR(xx_foo_doc,
"foo(i,j)\n\
\n\
Return the sum of i and j.");
static PyObject *
xx_foo(PyObject *self, PyObject *args) {
long i, j;
long res;
if (!PyArg_ParseTuple(args, "ll:foo", &i, &j))
return NULL;
res = i+j;
return PyInt_FromLong(res);
}
static PyObject *
xx_new(PyObject *self, PyObject *args) {
XxoObject *rv;
if (!PyArg_ParseTuple(args, ":new"))
return NULL;
rv = newXxoObject(args);
if (rv == NULL)
return NULL;
return (PyObject *)rv;
}
static PyObject *
xx_bug(PyObject *self, PyObject *args) {
PyObject *list, *item;
if (!PyArg_ParseTuple(args, "O:bug", &list))
return NULL;
item = PyList_GetItem(list, 0);
PyList_SetItem(list, 1, PyInt_FromLong(0L));
PyObject_Print(item, stdout, 0);
printf("\n");
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
xx_roj(PyObject *self, PyObject *args) {
PyObject *a;
long b;
if (!PyArg_ParseTuple(args, "O#:roj", &a, &b))
return NULL;
Py_INCREF(Py_None);
return Py_None;
}
static PyTypeObject Str_Type = {
PyVarObject_HEAD_INIT(NULL, 0)
"xxmodule.Str",
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
0,
Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
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
0,
0,
0,
0,
};
static PyObject *
null_richcompare(PyObject *self, PyObject *other, int op) {
Py_INCREF(Py_NotImplemented);
return Py_NotImplemented;
}
static PyTypeObject Null_Type = {
PyVarObject_HEAD_INIT(NULL, 0)
"xxmodule.Null",
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
0,
Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
0,
0,
0,
null_richcompare,
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
};
static PyMethodDef xx_methods[] = {
{
"roj", xx_roj, METH_VARARGS,
PyDoc_STR("roj(a,b) -> None")
},
{
"foo", xx_foo, METH_VARARGS,
xx_foo_doc
},
{
"new", xx_new, METH_VARARGS,
PyDoc_STR("new() -> new Xx object")
},
{
"bug", xx_bug, METH_VARARGS,
PyDoc_STR("bug(o) -> None")
},
{NULL, NULL}
};
PyDoc_STRVAR(module_doc,
"This is a template module just for instruction.");
PyMODINIT_FUNC
initxx(void) {
PyObject *m;
Null_Type.tp_base = &PyBaseObject_Type;
Null_Type.tp_new = PyType_GenericNew;
Str_Type.tp_base = &PyUnicode_Type;
if (PyType_Ready(&Xxo_Type) < 0)
return;
m = Py_InitModule3("xx", xx_methods, module_doc);
if (m == NULL)
return;
if (ErrorObject == NULL) {
ErrorObject = PyErr_NewException("xx.error", NULL, NULL);
if (ErrorObject == NULL)
return;
}
Py_INCREF(ErrorObject);
PyModule_AddObject(m, "error", ErrorObject);
if (PyType_Ready(&Str_Type) < 0)
return;
PyModule_AddObject(m, "Str", (PyObject *)&Str_Type);
if (PyType_Ready(&Null_Type) < 0)
return;
PyModule_AddObject(m, "Null", (PyObject *)&Null_Type);
}
