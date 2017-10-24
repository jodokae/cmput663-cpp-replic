#include "Python.h"
typedef struct {
PyObject_HEAD
long start;
long step;
long len;
} rangeobject;
static long
get_len_of_range(long lo, long hi, long step) {
long n = 0;
if (lo < hi) {
unsigned long uhi = (unsigned long)hi;
unsigned long ulo = (unsigned long)lo;
unsigned long diff = uhi - ulo - 1;
n = (long)(diff / (unsigned long)step + 1);
}
return n;
}
static PyObject *
range_new(PyTypeObject *type, PyObject *args, PyObject *kw) {
rangeobject *obj;
long ilow = 0, ihigh = 0, istep = 1;
long n;
if (!_PyArg_NoKeywords("xrange()", kw))
return NULL;
if (PyTuple_Size(args) <= 1) {
if (!PyArg_ParseTuple(args,
"l;xrange() requires 1-3 int arguments",
&ihigh))
return NULL;
} else {
if (!PyArg_ParseTuple(args,
"ll|l;xrange() requires 1-3 int arguments",
&ilow, &ihigh, &istep))
return NULL;
}
if (istep == 0) {
PyErr_SetString(PyExc_ValueError, "xrange() arg 3 must not be zero");
return NULL;
}
if (istep > 0)
n = get_len_of_range(ilow, ihigh, istep);
else
n = get_len_of_range(ihigh, ilow, -istep);
if (n < 0) {
PyErr_SetString(PyExc_OverflowError,
"xrange() result has too many items");
return NULL;
}
obj = PyObject_New(rangeobject, &PyRange_Type);
if (obj == NULL)
return NULL;
obj->start = ilow;
obj->len = n;
obj->step = istep;
return (PyObject *) obj;
}
PyDoc_STRVAR(range_doc,
"xrange([start,] stop[, step]) -> xrange object\n\
\n\
Like range(), but instead of returning a list, returns an object that\n\
generates the numbers in the range on demand. For looping, this is \n\
slightly faster than range() and more memory efficient.");
static PyObject *
range_item(rangeobject *r, Py_ssize_t i) {
if (i < 0 || i >= r->len) {
PyErr_SetString(PyExc_IndexError,
"xrange object index out of range");
return NULL;
}
return PyInt_FromSsize_t(r->start + i * r->step);
}
static Py_ssize_t
range_length(rangeobject *r) {
return (Py_ssize_t)(r->len);
}
static PyObject *
range_repr(rangeobject *r) {
PyObject *rtn;
if (r->start == 0 && r->step == 1)
rtn = PyString_FromFormat("xrange(%ld)",
r->start + r->len * r->step);
else if (r->step == 1)
rtn = PyString_FromFormat("xrange(%ld, %ld)",
r->start,
r->start + r->len * r->step);
else
rtn = PyString_FromFormat("xrange(%ld, %ld, %ld)",
r->start,
r->start + r->len * r->step,
r->step);
return rtn;
}
static PyObject *
range_reduce(rangeobject *r, PyObject *args) {
return Py_BuildValue("(O(iii))", Py_TYPE(r),
r->start,
r->start + r->len * r->step,
r->step);
}
static PySequenceMethods range_as_sequence = {
(lenfunc)range_length,
0,
0,
(ssizeargfunc)range_item,
0,
};
static PyObject * range_iter(PyObject *seq);
static PyObject * range_reverse(PyObject *seq);
PyDoc_STRVAR(reverse_doc,
"Returns a reverse iterator.");
static PyMethodDef range_methods[] = {
{"__reversed__", (PyCFunction)range_reverse, METH_NOARGS, reverse_doc},
{"__reduce__", (PyCFunction)range_reduce, METH_VARARGS},
{NULL, NULL}
};
PyTypeObject PyRange_Type = {
PyObject_HEAD_INIT(&PyType_Type)
0,
"xrange",
sizeof(rangeobject),
0,
(destructor)PyObject_Del,
0,
0,
0,
0,
(reprfunc)range_repr,
0,
&range_as_sequence,
0,
0,
0,
0,
PyObject_GenericGetAttr,
0,
0,
Py_TPFLAGS_DEFAULT,
range_doc,
0,
0,
0,
0,
range_iter,
0,
range_methods,
0,
0,
0,
0,
0,
0,
0,
0,
0,
range_new,
};
typedef struct {
PyObject_HEAD
long index;
long start;
long step;
long len;
} rangeiterobject;
static PyObject *
rangeiter_next(rangeiterobject *r) {
if (r->index < r->len)
return PyInt_FromLong(r->start + (r->index++) * r->step);
return NULL;
}
static PyObject *
rangeiter_len(rangeiterobject *r) {
return PyInt_FromLong(r->len - r->index);
}
PyDoc_STRVAR(length_hint_doc, "Private method returning an estimate of len(list(it)).");
static PyMethodDef rangeiter_methods[] = {
{"__length_hint__", (PyCFunction)rangeiter_len, METH_NOARGS, length_hint_doc},
{NULL, NULL}
};
static PyTypeObject Pyrangeiter_Type = {
PyObject_HEAD_INIT(&PyType_Type)
0,
"rangeiterator",
sizeof(rangeiterobject),
0,
(destructor)PyObject_Del,
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
PyObject_GenericGetAttr,
0,
0,
Py_TPFLAGS_DEFAULT,
0,
0,
0,
0,
0,
PyObject_SelfIter,
(iternextfunc)rangeiter_next,
rangeiter_methods,
0,
};
static PyObject *
range_iter(PyObject *seq) {
rangeiterobject *it;
if (!PyRange_Check(seq)) {
PyErr_BadInternalCall();
return NULL;
}
it = PyObject_New(rangeiterobject, &Pyrangeiter_Type);
if (it == NULL)
return NULL;
it->index = 0;
it->start = ((rangeobject *)seq)->start;
it->step = ((rangeobject *)seq)->step;
it->len = ((rangeobject *)seq)->len;
return (PyObject *)it;
}
static PyObject *
range_reverse(PyObject *seq) {
rangeiterobject *it;
long start, step, len;
if (!PyRange_Check(seq)) {
PyErr_BadInternalCall();
return NULL;
}
it = PyObject_New(rangeiterobject, &Pyrangeiter_Type);
if (it == NULL)
return NULL;
start = ((rangeobject *)seq)->start;
step = ((rangeobject *)seq)->step;
len = ((rangeobject *)seq)->len;
it->index = 0;
it->start = start + (len-1) * step;
it->step = -step;
it->len = len;
return (PyObject *)it;
}
