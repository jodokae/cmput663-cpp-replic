#include <Python.h>
#include <structmember.h>
#include "cursor.h"
#include "microprotocols.h"
#include "prepare_protocol.h"
PyObject *psyco_adapters;
int
pysqlite_microprotocols_init(PyObject *dict) {
if ((psyco_adapters = PyDict_New()) == NULL) {
return -1;
}
return PyDict_SetItemString(dict, "adapters", psyco_adapters);
}
int
pysqlite_microprotocols_add(PyTypeObject *type, PyObject *proto, PyObject *cast) {
PyObject* key;
int rc;
if (proto == NULL) proto = (PyObject*)&pysqlite_PrepareProtocolType;
key = Py_BuildValue("(OO)", (PyObject*)type, proto);
if (!key) {
return -1;
}
rc = PyDict_SetItem(psyco_adapters, key, cast);
Py_DECREF(key);
return rc;
}
PyObject *
pysqlite_microprotocols_adapt(PyObject *obj, PyObject *proto, PyObject *alt) {
PyObject *adapter, *key;
key = Py_BuildValue("(OO)", (PyObject*)obj->ob_type, proto);
if (!key) {
return NULL;
}
adapter = PyDict_GetItem(psyco_adapters, key);
Py_DECREF(key);
if (adapter) {
PyObject *adapted = PyObject_CallFunctionObjArgs(adapter, obj, NULL);
return adapted;
}
if (PyObject_HasAttrString(proto, "__adapt__")) {
PyObject *adapted = PyObject_CallMethod(proto, "__adapt__", "O", obj);
if (adapted) {
if (adapted != Py_None) {
return adapted;
} else {
Py_DECREF(adapted);
}
}
if (PyErr_Occurred() && !PyErr_ExceptionMatches(PyExc_TypeError))
return NULL;
}
if (PyObject_HasAttrString(obj, "__conform__")) {
PyObject *adapted = PyObject_CallMethod(obj, "__conform__","O", proto);
if (adapted) {
if (adapted != Py_None) {
return adapted;
} else {
Py_DECREF(adapted);
}
}
if (PyErr_Occurred() && !PyErr_ExceptionMatches(PyExc_TypeError)) {
return NULL;
}
}
PyErr_SetString(pysqlite_ProgrammingError, "can't adapt");
return NULL;
}
PyObject *
pysqlite_adapt(pysqlite_Cursor *self, PyObject *args) {
PyObject *obj, *alt = NULL;
PyObject *proto = (PyObject*)&pysqlite_PrepareProtocolType;
if (!PyArg_ParseTuple(args, "O|OO", &obj, &proto, &alt)) return NULL;
return pysqlite_microprotocols_adapt(obj, proto, alt);
}
