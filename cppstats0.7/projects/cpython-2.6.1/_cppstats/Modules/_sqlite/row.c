#include "row.h"
#include "cursor.h"
#include "sqlitecompat.h"
void pysqlite_row_dealloc(pysqlite_Row* self) {
Py_XDECREF(self->data);
Py_XDECREF(self->description);
Py_TYPE(self)->tp_free((PyObject*)self);
}
int pysqlite_row_init(pysqlite_Row* self, PyObject* args, PyObject* kwargs) {
PyObject* data;
pysqlite_Cursor* cursor;
self->data = 0;
self->description = 0;
if (!PyArg_ParseTuple(args, "OO", &cursor, &data)) {
return -1;
}
if (!PyObject_IsInstance((PyObject*)cursor, (PyObject*)&pysqlite_CursorType)) {
PyErr_SetString(PyExc_TypeError, "instance of cursor required for first argument");
return -1;
}
if (!PyTuple_Check(data)) {
PyErr_SetString(PyExc_TypeError, "tuple required for second argument");
return -1;
}
Py_INCREF(data);
self->data = data;
Py_INCREF(cursor->description);
self->description = cursor->description;
return 0;
}
PyObject* pysqlite_row_subscript(pysqlite_Row* self, PyObject* idx) {
long _idx;
char* key;
int nitems, i;
char* compare_key;
char* p1;
char* p2;
PyObject* item;
if (PyInt_Check(idx)) {
_idx = PyInt_AsLong(idx);
item = PyTuple_GetItem(self->data, _idx);
Py_XINCREF(item);
return item;
} else if (PyLong_Check(idx)) {
_idx = PyLong_AsLong(idx);
item = PyTuple_GetItem(self->data, _idx);
Py_XINCREF(item);
return item;
} else if (PyString_Check(idx)) {
key = PyString_AsString(idx);
nitems = PyTuple_Size(self->description);
for (i = 0; i < nitems; i++) {
compare_key = PyString_AsString(PyTuple_GET_ITEM(PyTuple_GET_ITEM(self->description, i), 0));
if (!compare_key) {
return NULL;
}
p1 = key;
p2 = compare_key;
while (1) {
if ((*p1 == (char)0) || (*p2 == (char)0)) {
break;
}
if ((*p1 | 0x20) != (*p2 | 0x20)) {
break;
}
p1++;
p2++;
}
if ((*p1 == (char)0) && (*p2 == (char)0)) {
item = PyTuple_GetItem(self->data, i);
Py_INCREF(item);
return item;
}
}
PyErr_SetString(PyExc_IndexError, "No item with that key");
return NULL;
} else if (PySlice_Check(idx)) {
PyErr_SetString(PyExc_ValueError, "slices not implemented, yet");
return NULL;
} else {
PyErr_SetString(PyExc_IndexError, "Index must be int or string");
return NULL;
}
}
Py_ssize_t pysqlite_row_length(pysqlite_Row* self, PyObject* args, PyObject* kwargs) {
return PyTuple_GET_SIZE(self->data);
}
PyObject* pysqlite_row_keys(pysqlite_Row* self, PyObject* args, PyObject* kwargs) {
PyObject* list;
int nitems, i;
list = PyList_New(0);
if (!list) {
return NULL;
}
nitems = PyTuple_Size(self->description);
for (i = 0; i < nitems; i++) {
if (PyList_Append(list, PyTuple_GET_ITEM(PyTuple_GET_ITEM(self->description, i), 0)) != 0) {
Py_DECREF(list);
return NULL;
}
}
return list;
}
static int pysqlite_row_print(pysqlite_Row* self, FILE *fp, int flags) {
return (&PyTuple_Type)->tp_print(self->data, fp, flags);
}
static PyObject* pysqlite_iter(pysqlite_Row* self) {
return PyObject_GetIter(self->data);
}
static long pysqlite_row_hash(pysqlite_Row *self) {
return PyObject_Hash(self->description) ^ PyObject_Hash(self->data);
}
static PyObject* pysqlite_row_richcompare(pysqlite_Row *self, PyObject *_other, int opid) {
if (opid != Py_EQ && opid != Py_NE) {
Py_INCREF(Py_NotImplemented);
return Py_NotImplemented;
}
if (PyType_IsSubtype(Py_TYPE(_other), &pysqlite_RowType)) {
pysqlite_Row *other = (pysqlite_Row *)_other;
PyObject *res = PyObject_RichCompare(self->description, other->description, opid);
if ((opid == Py_EQ && res == Py_True)
|| (opid == Py_NE && res == Py_False)) {
Py_DECREF(res);
return PyObject_RichCompare(self->data, other->data, opid);
}
}
Py_INCREF(Py_NotImplemented);
return Py_NotImplemented;
}
PyMappingMethods pysqlite_row_as_mapping = {
(lenfunc)pysqlite_row_length,
(binaryfunc)pysqlite_row_subscript,
(objobjargproc)0,
};
static PyMethodDef pysqlite_row_methods[] = {
{
"keys", (PyCFunction)pysqlite_row_keys, METH_NOARGS,
PyDoc_STR("Returns the keys of the row.")
},
{NULL, NULL}
};
PyTypeObject pysqlite_RowType = {
PyVarObject_HEAD_INIT(NULL, 0)
MODULE_NAME ".Row",
sizeof(pysqlite_Row),
0,
(destructor)pysqlite_row_dealloc,
(printfunc)pysqlite_row_print,
0,
0,
0,
0,
0,
0,
0,
(hashfunc)pysqlite_row_hash,
0,
0,
0,
0,
0,
Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE,
0,
(traverseproc)0,
0,
(richcmpfunc)pysqlite_row_richcompare,
0,
(getiterfunc)pysqlite_iter,
0,
pysqlite_row_methods,
0,
0,
0,
0,
0,
0,
0,
(initproc)pysqlite_row_init,
0,
0,
0
};
extern int pysqlite_row_setup_types(void) {
pysqlite_RowType.tp_new = PyType_GenericNew;
pysqlite_RowType.tp_as_mapping = &pysqlite_row_as_mapping;
return PyType_Ready(&pysqlite_RowType);
}
