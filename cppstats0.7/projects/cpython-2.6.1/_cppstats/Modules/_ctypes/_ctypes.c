#define PY_SSIZE_T_CLEAN
#include "Python.h"
#include "structmember.h"
#include <ffi.h>
#if defined(MS_WIN32)
#include <windows.h>
#include <malloc.h>
#if !defined(IS_INTRESOURCE)
#define IS_INTRESOURCE(x) (((size_t)(x) >> 16) == 0)
#endif
#if defined(_WIN32_WCE)
#undef GetProcAddress
#define GetProcAddress GetProcAddressA
#endif
#else
#include "ctypes_dlfcn.h"
#endif
#include "ctypes.h"
PyObject *PyExc_ArgError;
PyObject *_pointer_type_cache;
static PyTypeObject Simple_Type;
static PyObject *_unpickle;
char *conversion_mode_encoding = NULL;
char *conversion_mode_errors = NULL;
#if (PY_VERSION_HEX < 0x02040000)
static PyObject *
PyTuple_Pack(int n, ...) {
int i;
PyObject *o;
PyObject *result;
PyObject **items;
va_list vargs;
va_start(vargs, n);
result = PyTuple_New(n);
if (result == NULL)
return NULL;
items = ((PyTupleObject *)result)->ob_item;
for (i = 0; i < n; i++) {
o = va_arg(vargs, PyObject *);
Py_INCREF(o);
items[i] = o;
}
va_end(vargs);
return result;
}
#endif
typedef struct {
PyObject_HEAD
PyObject *key;
PyObject *dict;
} DictRemoverObject;
static void
_DictRemover_dealloc(PyObject *_self) {
DictRemoverObject *self = (DictRemoverObject *)_self;
Py_XDECREF(self->key);
Py_XDECREF(self->dict);
Py_TYPE(self)->tp_free(_self);
}
static PyObject *
_DictRemover_call(PyObject *_self, PyObject *args, PyObject *kw) {
DictRemoverObject *self = (DictRemoverObject *)_self;
if (self->key && self->dict) {
if (-1 == PyDict_DelItem(self->dict, self->key))
PyErr_WriteUnraisable(Py_None);
Py_DECREF(self->key);
self->key = NULL;
Py_DECREF(self->dict);
self->dict = NULL;
}
Py_INCREF(Py_None);
return Py_None;
}
static PyTypeObject DictRemover_Type = {
PyVarObject_HEAD_INIT(NULL, 0)
"_ctypes.DictRemover",
sizeof(DictRemoverObject),
0,
_DictRemover_dealloc,
0,
0,
0,
0,
0,
0,
0,
0,
0,
_DictRemover_call,
0,
0,
0,
0,
Py_TPFLAGS_DEFAULT,
"deletes a key from a dictionary",
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
int
PyDict_SetItemProxy(PyObject *dict, PyObject *key, PyObject *item) {
PyObject *obj;
DictRemoverObject *remover;
PyObject *proxy;
int result;
obj = PyObject_CallObject((PyObject *)&DictRemover_Type, NULL);
if (obj == NULL)
return -1;
remover = (DictRemoverObject *)obj;
assert(remover->key == NULL);
assert(remover->dict == NULL);
Py_INCREF(key);
remover->key = key;
Py_INCREF(dict);
remover->dict = dict;
proxy = PyWeakref_NewProxy(item, obj);
Py_DECREF(obj);
if (proxy == NULL)
return -1;
result = PyDict_SetItem(dict, key, proxy);
Py_DECREF(proxy);
return result;
}
PyObject *
PyDict_GetItemProxy(PyObject *dict, PyObject *key) {
PyObject *result;
PyObject *item = PyDict_GetItem(dict, key);
if (item == NULL)
return NULL;
if (!PyWeakref_CheckProxy(item))
return item;
result = PyWeakref_GET_OBJECT(item);
if (result == Py_None)
return NULL;
return result;
}
char *
alloc_format_string(const char *prefix, const char *suffix) {
size_t len;
char *result;
if (suffix == NULL) {
assert(PyErr_Occurred());
return NULL;
}
len = strlen(suffix);
if (prefix)
len += strlen(prefix);
result = PyMem_Malloc(len + 1);
if (result == NULL)
return NULL;
if (prefix)
strcpy(result, prefix);
else
result[0] = '\0';
strcat(result, suffix);
return result;
}
static PyCArgObject *
StructUnionType_paramfunc(CDataObject *self) {
PyCArgObject *parg;
StgDictObject *stgdict;
parg = new_CArgObject();
if (parg == NULL)
return NULL;
parg->tag = 'V';
stgdict = PyObject_stgdict((PyObject *)self);
assert(stgdict);
parg->pffi_type = &stgdict->ffi_type_pointer;
parg->value.p = self->b_ptr;
parg->size = self->b_size;
Py_INCREF(self);
parg->obj = (PyObject *)self;
return parg;
}
static PyObject *
StructUnionType_new(PyTypeObject *type, PyObject *args, PyObject *kwds, int isStruct) {
PyTypeObject *result;
PyObject *fields;
StgDictObject *dict;
result = (PyTypeObject *)PyType_Type.tp_new(type, args, kwds);
if (!result)
return NULL;
if (PyDict_GetItemString(result->tp_dict, "_abstract_"))
return (PyObject *)result;
dict = (StgDictObject *)PyObject_CallObject((PyObject *)&StgDict_Type, NULL);
if (!dict) {
Py_DECREF(result);
return NULL;
}
if (-1 == PyDict_Update((PyObject *)dict, result->tp_dict)) {
Py_DECREF(result);
Py_DECREF((PyObject *)dict);
return NULL;
}
Py_DECREF(result->tp_dict);
result->tp_dict = (PyObject *)dict;
dict->format = alloc_format_string(NULL, "B");
if (dict->format == NULL) {
Py_DECREF(result);
return NULL;
}
dict->paramfunc = StructUnionType_paramfunc;
fields = PyDict_GetItemString((PyObject *)dict, "_fields_");
if (!fields) {
StgDictObject *basedict = PyType_stgdict((PyObject *)result->tp_base);
if (basedict == NULL)
return (PyObject *)result;
if (-1 == StgDict_clone(dict, basedict)) {
Py_DECREF(result);
return NULL;
}
dict->flags &= ~DICTFLAG_FINAL;
basedict->flags |= DICTFLAG_FINAL;
return (PyObject *)result;
}
if (-1 == PyObject_SetAttrString((PyObject *)result, "_fields_", fields)) {
Py_DECREF(result);
return NULL;
}
return (PyObject *)result;
}
static PyObject *
StructType_new(PyTypeObject *type, PyObject *args, PyObject *kwds) {
return StructUnionType_new(type, args, kwds, 1);
}
static PyObject *
UnionType_new(PyTypeObject *type, PyObject *args, PyObject *kwds) {
return StructUnionType_new(type, args, kwds, 0);
}
static char from_address_doc[] =
"C.from_address(integer) -> C instance\naccess a C instance at the specified address";
static PyObject *
CDataType_from_address(PyObject *type, PyObject *value) {
void *buf;
if (!PyInt_Check(value) && !PyLong_Check(value)) {
PyErr_SetString(PyExc_TypeError,
"integer expected");
return NULL;
}
buf = (void *)PyLong_AsVoidPtr(value);
if (PyErr_Occurred())
return NULL;
return CData_AtAddress(type, buf);
}
static char from_buffer_doc[] =
"C.from_buffer(object, offset=0) -> C instance\ncreate a C instance from a writeable buffer";
static int
KeepRef(CDataObject *target, Py_ssize_t index, PyObject *keep);
static PyObject *
CDataType_from_buffer(PyObject *type, PyObject *args) {
void *buffer;
Py_ssize_t buffer_len;
Py_ssize_t offset = 0;
PyObject *obj, *result;
StgDictObject *dict = PyType_stgdict(type);
assert (dict);
if (!PyArg_ParseTuple(args,
#if (PY_VERSION_HEX < 0x02050000)
"O|i:from_buffer",
#else
"O|n:from_buffer",
#endif
&obj, &offset))
return NULL;
if (-1 == PyObject_AsWriteBuffer(obj, &buffer, &buffer_len))
return NULL;
if (offset < 0) {
PyErr_SetString(PyExc_ValueError,
"offset cannit be negative");
return NULL;
}
if (dict->size > buffer_len - offset) {
PyErr_Format(PyExc_ValueError,
#if (PY_VERSION_HEX < 0x02050000)
"Buffer size too small (%d instead of at least %d bytes)",
#else
"Buffer size too small (%zd instead of at least %zd bytes)",
#endif
buffer_len, dict->size + offset);
return NULL;
}
result = CData_AtAddress(type, (char *)buffer + offset);
if (result == NULL)
return NULL;
Py_INCREF(obj);
if (-1 == KeepRef((CDataObject *)result, -1, obj)) {
Py_DECREF(result);
return NULL;
}
return result;
}
static char from_buffer_copy_doc[] =
"C.from_buffer_copy(object, offset=0) -> C instance\ncreate a C instance from a readable buffer";
static PyObject *
GenericCData_new(PyTypeObject *type, PyObject *args, PyObject *kwds);
static PyObject *
CDataType_from_buffer_copy(PyObject *type, PyObject *args) {
const void *buffer;
Py_ssize_t buffer_len;
Py_ssize_t offset = 0;
PyObject *obj, *result;
StgDictObject *dict = PyType_stgdict(type);
assert (dict);
if (!PyArg_ParseTuple(args,
#if (PY_VERSION_HEX < 0x02050000)
"O|i:from_buffer",
#else
"O|n:from_buffer",
#endif
&obj, &offset))
return NULL;
if (-1 == PyObject_AsReadBuffer(obj, &buffer, &buffer_len))
return NULL;
if (offset < 0) {
PyErr_SetString(PyExc_ValueError,
"offset cannit be negative");
return NULL;
}
if (dict->size > buffer_len - offset) {
PyErr_Format(PyExc_ValueError,
#if (PY_VERSION_HEX < 0x02050000)
"Buffer size too small (%d instead of at least %d bytes)",
#else
"Buffer size too small (%zd instead of at least %zd bytes)",
#endif
buffer_len, dict->size + offset);
return NULL;
}
result = GenericCData_new((PyTypeObject *)type, NULL, NULL);
if (result == NULL)
return NULL;
memcpy(((CDataObject *)result)->b_ptr,
(char *)buffer+offset, dict->size);
return result;
}
static char in_dll_doc[] =
"C.in_dll(dll, name) -> C instance\naccess a C instance in a dll";
static PyObject *
CDataType_in_dll(PyObject *type, PyObject *args) {
PyObject *dll;
char *name;
PyObject *obj;
void *handle;
void *address;
if (!PyArg_ParseTuple(args, "Os:in_dll", &dll, &name))
return NULL;
obj = PyObject_GetAttrString(dll, "_handle");
if (!obj)
return NULL;
if (!PyInt_Check(obj) && !PyLong_Check(obj)) {
PyErr_SetString(PyExc_TypeError,
"the _handle attribute of the second argument must be an integer");
Py_DECREF(obj);
return NULL;
}
handle = (void *)PyLong_AsVoidPtr(obj);
Py_DECREF(obj);
if (PyErr_Occurred()) {
PyErr_SetString(PyExc_ValueError,
"could not convert the _handle attribute to a pointer");
return NULL;
}
#if defined(MS_WIN32)
address = (void *)GetProcAddress(handle, name);
if (!address) {
PyErr_Format(PyExc_ValueError,
"symbol '%s' not found",
name);
return NULL;
}
#else
address = (void *)ctypes_dlsym(handle, name);
if (!address) {
PyErr_Format(PyExc_ValueError,
#if defined(__CYGWIN__)
"symbol '%s' not found (%s) ",
name,
#endif
ctypes_dlerror());
return NULL;
}
#endif
return CData_AtAddress(type, address);
}
static char from_param_doc[] =
"Convert a Python object into a function call parameter.";
static PyObject *
CDataType_from_param(PyObject *type, PyObject *value) {
PyObject *as_parameter;
if (1 == PyObject_IsInstance(value, type)) {
Py_INCREF(value);
return value;
}
if (PyCArg_CheckExact(value)) {
PyCArgObject *p = (PyCArgObject *)value;
PyObject *ob = p->obj;
const char *ob_name;
StgDictObject *dict;
dict = PyType_stgdict(type);
if(dict && ob
&& PyObject_IsInstance(ob, dict->proto)) {
Py_INCREF(value);
return value;
}
ob_name = (ob) ? Py_TYPE(ob)->tp_name : "???";
PyErr_Format(PyExc_TypeError,
"expected %s instance instead of pointer to %s",
((PyTypeObject *)type)->tp_name, ob_name);
return NULL;
}
as_parameter = PyObject_GetAttrString(value, "_as_parameter_");
if (as_parameter) {
value = CDataType_from_param(type, as_parameter);
Py_DECREF(as_parameter);
return value;
}
PyErr_Format(PyExc_TypeError,
"expected %s instance instead of %s",
((PyTypeObject *)type)->tp_name,
Py_TYPE(value)->tp_name);
return NULL;
}
static PyMethodDef CDataType_methods[] = {
{ "from_param", CDataType_from_param, METH_O, from_param_doc },
{ "from_address", CDataType_from_address, METH_O, from_address_doc },
{ "from_buffer", CDataType_from_buffer, METH_VARARGS, from_buffer_doc, },
{ "from_buffer_copy", CDataType_from_buffer_copy, METH_VARARGS, from_buffer_copy_doc, },
{ "in_dll", CDataType_in_dll, METH_VARARGS, in_dll_doc },
{ NULL, NULL },
};
static PyObject *
CDataType_repeat(PyObject *self, Py_ssize_t length) {
if (length < 0)
return PyErr_Format(PyExc_ValueError,
#if (PY_VERSION_HEX < 0x02050000)
"Array length must be >= 0, not %d",
#else
"Array length must be >= 0, not %zd",
#endif
length);
return CreateArrayType(self, length);
}
static PySequenceMethods CDataType_as_sequence = {
0,
0,
CDataType_repeat,
0,
0,
0,
0,
0,
0,
0,
};
static int
CDataType_clear(PyTypeObject *self) {
StgDictObject *dict = PyType_stgdict((PyObject *)self);
if (dict)
Py_CLEAR(dict->proto);
return PyType_Type.tp_clear((PyObject *)self);
}
static int
CDataType_traverse(PyTypeObject *self, visitproc visit, void *arg) {
StgDictObject *dict = PyType_stgdict((PyObject *)self);
if (dict)
Py_VISIT(dict->proto);
return PyType_Type.tp_traverse((PyObject *)self, visit, arg);
}
static int
StructType_setattro(PyObject *self, PyObject *key, PyObject *value) {
if (-1 == PyType_Type.tp_setattro(self, key, value))
return -1;
if (value && PyString_Check(key) &&
0 == strcmp(PyString_AS_STRING(key), "_fields_"))
return StructUnionType_update_stgdict(self, value, 1);
return 0;
}
static int
UnionType_setattro(PyObject *self, PyObject *key, PyObject *value) {
if (-1 == PyObject_GenericSetAttr(self, key, value))
return -1;
if (PyString_Check(key) &&
0 == strcmp(PyString_AS_STRING(key), "_fields_"))
return StructUnionType_update_stgdict(self, value, 0);
return 0;
}
PyTypeObject StructType_Type = {
PyVarObject_HEAD_INIT(NULL, 0)
"_ctypes.StructType",
0,
0,
0,
0,
0,
0,
0,
0,
0,
&CDataType_as_sequence,
0,
0,
0,
0,
0,
StructType_setattro,
0,
Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE | Py_TPFLAGS_HAVE_GC,
"metatype for the CData Objects",
(traverseproc)CDataType_traverse,
(inquiry)CDataType_clear,
0,
0,
0,
0,
CDataType_methods,
0,
0,
0,
0,
0,
0,
0,
0,
0,
StructType_new,
0,
};
static PyTypeObject UnionType_Type = {
PyVarObject_HEAD_INIT(NULL, 0)
"_ctypes.UnionType",
0,
0,
0,
0,
0,
0,
0,
0,
0,
&CDataType_as_sequence,
0,
0,
0,
0,
0,
UnionType_setattro,
0,
Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE | Py_TPFLAGS_HAVE_GC,
"metatype for the CData Objects",
(traverseproc)CDataType_traverse,
(inquiry)CDataType_clear,
0,
0,
0,
0,
CDataType_methods,
0,
0,
0,
0,
0,
0,
0,
0,
0,
UnionType_new,
0,
};
static int
PointerType_SetProto(StgDictObject *stgdict, PyObject *proto) {
if (!proto || !PyType_Check(proto)) {
PyErr_SetString(PyExc_TypeError,
"_type_ must be a type");
return -1;
}
if (!PyType_stgdict(proto)) {
PyErr_SetString(PyExc_TypeError,
"_type_ must have storage info");
return -1;
}
Py_INCREF(proto);
Py_XDECREF(stgdict->proto);
stgdict->proto = proto;
return 0;
}
static PyCArgObject *
PointerType_paramfunc(CDataObject *self) {
PyCArgObject *parg;
parg = new_CArgObject();
if (parg == NULL)
return NULL;
parg->tag = 'P';
parg->pffi_type = &ffi_type_pointer;
Py_INCREF(self);
parg->obj = (PyObject *)self;
parg->value.p = *(void **)self->b_ptr;
return parg;
}
static PyObject *
PointerType_new(PyTypeObject *type, PyObject *args, PyObject *kwds) {
PyTypeObject *result;
StgDictObject *stgdict;
PyObject *proto;
PyObject *typedict;
typedict = PyTuple_GetItem(args, 2);
if (!typedict)
return NULL;
stgdict = (StgDictObject *)PyObject_CallObject(
(PyObject *)&StgDict_Type, NULL);
if (!stgdict)
return NULL;
stgdict->size = sizeof(void *);
stgdict->align = getentry("P")->pffi_type->alignment;
stgdict->length = 1;
stgdict->ffi_type_pointer = ffi_type_pointer;
stgdict->paramfunc = PointerType_paramfunc;
stgdict->flags |= TYPEFLAG_ISPOINTER;
proto = PyDict_GetItemString(typedict, "_type_");
if (proto && -1 == PointerType_SetProto(stgdict, proto)) {
Py_DECREF((PyObject *)stgdict);
return NULL;
}
if (proto) {
StgDictObject *itemdict = PyType_stgdict(proto);
assert(itemdict);
stgdict->format = alloc_format_string("&",
itemdict->format ? itemdict->format : "B");
if (stgdict->format == NULL) {
Py_DECREF((PyObject *)stgdict);
return NULL;
}
}
result = (PyTypeObject *)PyType_Type.tp_new(type, args, kwds);
if (result == NULL) {
Py_DECREF((PyObject *)stgdict);
return NULL;
}
if (-1 == PyDict_Update((PyObject *)stgdict, result->tp_dict)) {
Py_DECREF(result);
Py_DECREF((PyObject *)stgdict);
return NULL;
}
Py_DECREF(result->tp_dict);
result->tp_dict = (PyObject *)stgdict;
return (PyObject *)result;
}
static PyObject *
PointerType_set_type(PyTypeObject *self, PyObject *type) {
StgDictObject *dict;
dict = PyType_stgdict((PyObject *)self);
assert(dict);
if (-1 == PointerType_SetProto(dict, type))
return NULL;
if (-1 == PyDict_SetItemString((PyObject *)dict, "_type_", type))
return NULL;
Py_INCREF(Py_None);
return Py_None;
}
staticforward PyObject *_byref(PyObject *);
static PyObject *
PointerType_from_param(PyObject *type, PyObject *value) {
StgDictObject *typedict;
if (value == Py_None)
return PyInt_FromLong(0);
typedict = PyType_stgdict(type);
assert(typedict);
switch (PyObject_IsInstance(value, typedict->proto)) {
case 1:
Py_INCREF(value);
return _byref(value);
case -1:
PyErr_Clear();
break;
default:
break;
}
if (PointerObject_Check(value) || ArrayObject_Check(value)) {
StgDictObject *v = PyObject_stgdict(value);
assert(v);
if (PyObject_IsSubclass(v->proto, typedict->proto)) {
Py_INCREF(value);
return value;
}
}
return CDataType_from_param(type, value);
}
static PyMethodDef PointerType_methods[] = {
{ "from_address", CDataType_from_address, METH_O, from_address_doc },
{ "from_buffer", CDataType_from_buffer, METH_VARARGS, from_buffer_doc, },
{ "from_buffer_copy", CDataType_from_buffer_copy, METH_VARARGS, from_buffer_copy_doc, },
{ "in_dll", CDataType_in_dll, METH_VARARGS, in_dll_doc},
{ "from_param", (PyCFunction)PointerType_from_param, METH_O, from_param_doc},
{ "set_type", (PyCFunction)PointerType_set_type, METH_O },
{ NULL, NULL },
};
PyTypeObject PointerType_Type = {
PyVarObject_HEAD_INIT(NULL, 0)
"_ctypes.PointerType",
0,
0,
0,
0,
0,
0,
0,
0,
0,
&CDataType_as_sequence,
0,
0,
0,
0,
0,
0,
0,
Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE | Py_TPFLAGS_HAVE_GC,
"metatype for the Pointer Objects",
(traverseproc)CDataType_traverse,
(inquiry)CDataType_clear,
0,
0,
0,
0,
PointerType_methods,
0,
0,
0,
0,
0,
0,
0,
0,
0,
PointerType_new,
0,
};
static int
CharArray_set_raw(CDataObject *self, PyObject *value) {
char *ptr;
Py_ssize_t size;
if (PyBuffer_Check(value)) {
size = Py_TYPE(value)->tp_as_buffer->bf_getreadbuffer(value, 0, (void *)&ptr);
if (size < 0)
return -1;
} else if (-1 == PyString_AsStringAndSize(value, &ptr, &size)) {
return -1;
}
if (size > self->b_size) {
PyErr_SetString(PyExc_ValueError,
"string too long");
return -1;
}
memcpy(self->b_ptr, ptr, size);
return 0;
}
static PyObject *
CharArray_get_raw(CDataObject *self) {
return PyString_FromStringAndSize(self->b_ptr, self->b_size);
}
static PyObject *
CharArray_get_value(CDataObject *self) {
int i;
char *ptr = self->b_ptr;
for (i = 0; i < self->b_size; ++i)
if (*ptr++ == '\0')
break;
return PyString_FromStringAndSize(self->b_ptr, i);
}
static int
CharArray_set_value(CDataObject *self, PyObject *value) {
char *ptr;
Py_ssize_t size;
if (value == NULL) {
PyErr_SetString(PyExc_TypeError,
"can't delete attribute");
return -1;
}
if (PyUnicode_Check(value)) {
value = PyUnicode_AsEncodedString(value,
conversion_mode_encoding,
conversion_mode_errors);
if (!value)
return -1;
} else if (!PyString_Check(value)) {
PyErr_Format(PyExc_TypeError,
"string expected instead of %s instance",
Py_TYPE(value)->tp_name);
return -1;
} else
Py_INCREF(value);
size = PyString_GET_SIZE(value);
if (size > self->b_size) {
PyErr_SetString(PyExc_ValueError,
"string too long");
Py_DECREF(value);
return -1;
}
ptr = PyString_AS_STRING(value);
memcpy(self->b_ptr, ptr, size);
if (size < self->b_size)
self->b_ptr[size] = '\0';
Py_DECREF(value);
return 0;
}
static PyGetSetDef CharArray_getsets[] = {
{
"raw", (getter)CharArray_get_raw, (setter)CharArray_set_raw,
"value", NULL
},
{
"value", (getter)CharArray_get_value, (setter)CharArray_set_value,
"string value"
},
{ NULL, NULL }
};
#if defined(CTYPES_UNICODE)
static PyObject *
WCharArray_get_value(CDataObject *self) {
unsigned int i;
wchar_t *ptr = (wchar_t *)self->b_ptr;
for (i = 0; i < self->b_size/sizeof(wchar_t); ++i)
if (*ptr++ == (wchar_t)0)
break;
return PyUnicode_FromWideChar((wchar_t *)self->b_ptr, i);
}
static int
WCharArray_set_value(CDataObject *self, PyObject *value) {
Py_ssize_t result = 0;
if (value == NULL) {
PyErr_SetString(PyExc_TypeError,
"can't delete attribute");
return -1;
}
if (PyString_Check(value)) {
value = PyUnicode_FromEncodedObject(value,
conversion_mode_encoding,
conversion_mode_errors);
if (!value)
return -1;
} else if (!PyUnicode_Check(value)) {
PyErr_Format(PyExc_TypeError,
"unicode string expected instead of %s instance",
Py_TYPE(value)->tp_name);
return -1;
} else
Py_INCREF(value);
if ((unsigned)PyUnicode_GET_SIZE(value) > self->b_size/sizeof(wchar_t)) {
PyErr_SetString(PyExc_ValueError,
"string too long");
result = -1;
goto done;
}
result = PyUnicode_AsWideChar((PyUnicodeObject *)value,
(wchar_t *)self->b_ptr,
self->b_size/sizeof(wchar_t));
if (result >= 0 && (size_t)result < self->b_size/sizeof(wchar_t))
((wchar_t *)self->b_ptr)[result] = (wchar_t)0;
done:
Py_DECREF(value);
return result >= 0 ? 0 : -1;
}
static PyGetSetDef WCharArray_getsets[] = {
{
"value", (getter)WCharArray_get_value, (setter)WCharArray_set_value,
"string value"
},
{ NULL, NULL }
};
#endif
static int
add_getset(PyTypeObject *type, PyGetSetDef *gsp) {
PyObject *dict = type->tp_dict;
for (; gsp->name != NULL; gsp++) {
PyObject *descr;
descr = PyDescr_NewGetSet(type, gsp);
if (descr == NULL)
return -1;
if (PyDict_SetItemString(dict, gsp->name, descr) < 0)
return -1;
Py_DECREF(descr);
}
return 0;
}
static PyCArgObject *
ArrayType_paramfunc(CDataObject *self) {
PyCArgObject *p = new_CArgObject();
if (p == NULL)
return NULL;
p->tag = 'P';
p->pffi_type = &ffi_type_pointer;
p->value.p = (char *)self->b_ptr;
Py_INCREF(self);
p->obj = (PyObject *)self;
return p;
}
static PyObject *
ArrayType_new(PyTypeObject *type, PyObject *args, PyObject *kwds) {
PyTypeObject *result;
StgDictObject *stgdict;
StgDictObject *itemdict;
PyObject *proto;
PyObject *typedict;
long length;
Py_ssize_t itemsize, itemalign;
char buf[32];
typedict = PyTuple_GetItem(args, 2);
if (!typedict)
return NULL;
proto = PyDict_GetItemString(typedict, "_length_");
if (!proto || !PyInt_Check(proto)) {
PyErr_SetString(PyExc_AttributeError,
"class must define a '_length_' attribute, "
"which must be a positive integer");
return NULL;
}
length = PyInt_AS_LONG(proto);
proto = PyDict_GetItemString(typedict, "_type_");
if (!proto) {
PyErr_SetString(PyExc_AttributeError,
"class must define a '_type_' attribute");
return NULL;
}
stgdict = (StgDictObject *)PyObject_CallObject(
(PyObject *)&StgDict_Type, NULL);
if (!stgdict)
return NULL;
itemdict = PyType_stgdict(proto);
if (!itemdict) {
PyErr_SetString(PyExc_TypeError,
"_type_ must have storage info");
Py_DECREF((PyObject *)stgdict);
return NULL;
}
assert(itemdict->format);
if (itemdict->format[0] == '(') {
sprintf(buf, "(%ld,", length);
stgdict->format = alloc_format_string(buf, itemdict->format+1);
} else {
sprintf(buf, "(%ld)", length);
stgdict->format = alloc_format_string(buf, itemdict->format);
}
if (stgdict->format == NULL) {
Py_DECREF((PyObject *)stgdict);
return NULL;
}
stgdict->ndim = itemdict->ndim + 1;
stgdict->shape = PyMem_Malloc(sizeof(Py_ssize_t *) * stgdict->ndim);
if (stgdict->shape == NULL) {
Py_DECREF((PyObject *)stgdict);
return NULL;
}
stgdict->shape[0] = length;
memmove(&stgdict->shape[1], itemdict->shape,
sizeof(Py_ssize_t) * (stgdict->ndim - 1));
itemsize = itemdict->size;
if (length * itemsize < 0) {
PyErr_SetString(PyExc_OverflowError,
"array too large");
return NULL;
}
itemalign = itemdict->align;
if (itemdict->flags & (TYPEFLAG_ISPOINTER | TYPEFLAG_HASPOINTER))
stgdict->flags |= TYPEFLAG_HASPOINTER;
stgdict->size = itemsize * length;
stgdict->align = itemalign;
stgdict->length = length;
Py_INCREF(proto);
stgdict->proto = proto;
stgdict->paramfunc = &ArrayType_paramfunc;
stgdict->ffi_type_pointer = ffi_type_pointer;
result = (PyTypeObject *)PyType_Type.tp_new(type, args, kwds);
if (result == NULL)
return NULL;
if (-1 == PyDict_Update((PyObject *)stgdict, result->tp_dict)) {
Py_DECREF(result);
Py_DECREF((PyObject *)stgdict);
return NULL;
}
Py_DECREF(result->tp_dict);
result->tp_dict = (PyObject *)stgdict;
if (itemdict->getfunc == getentry("c")->getfunc) {
if (-1 == add_getset(result, CharArray_getsets))
return NULL;
#if defined(CTYPES_UNICODE)
} else if (itemdict->getfunc == getentry("u")->getfunc) {
if (-1 == add_getset(result, WCharArray_getsets))
return NULL;
#endif
}
return (PyObject *)result;
}
PyTypeObject ArrayType_Type = {
PyVarObject_HEAD_INIT(NULL, 0)
"_ctypes.ArrayType",
0,
0,
0,
0,
0,
0,
0,
0,
0,
&CDataType_as_sequence,
0,
0,
0,
0,
0,
0,
0,
Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
"metatype for the Array Objects",
0,
0,
0,
0,
0,
0,
CDataType_methods,
0,
0,
0,
0,
0,
0,
0,
0,
0,
ArrayType_new,
0,
};
static char *SIMPLE_TYPE_CHARS = "cbBhHiIlLdfuzZqQPXOv?g";
static PyObject *
c_wchar_p_from_param(PyObject *type, PyObject *value) {
PyObject *as_parameter;
#if (PYTHON_API_VERSION < 1012)
#error not supported
#endif
if (value == Py_None) {
Py_INCREF(Py_None);
return Py_None;
}
if (PyUnicode_Check(value) || PyString_Check(value)) {
PyCArgObject *parg;
struct fielddesc *fd = getentry("Z");
parg = new_CArgObject();
if (parg == NULL)
return NULL;
parg->pffi_type = &ffi_type_pointer;
parg->tag = 'Z';
parg->obj = fd->setfunc(&parg->value, value, 0);
if (parg->obj == NULL) {
Py_DECREF(parg);
return NULL;
}
return (PyObject *)parg;
}
if (PyObject_IsInstance(value, type)) {
Py_INCREF(value);
return value;
}
if (ArrayObject_Check(value) || PointerObject_Check(value)) {
StgDictObject *dt = PyObject_stgdict(value);
StgDictObject *dict;
assert(dt);
dict = dt && dt->proto ? PyType_stgdict(dt->proto) : NULL;
if (dict && (dict->setfunc == getentry("u")->setfunc)) {
Py_INCREF(value);
return value;
}
}
if (PyCArg_CheckExact(value)) {
PyCArgObject *a = (PyCArgObject *)value;
StgDictObject *dict = PyObject_stgdict(a->obj);
if (dict && (dict->setfunc == getentry("u")->setfunc)) {
Py_INCREF(value);
return value;
}
}
as_parameter = PyObject_GetAttrString(value, "_as_parameter_");
if (as_parameter) {
value = c_wchar_p_from_param(type, as_parameter);
Py_DECREF(as_parameter);
return value;
}
PyErr_SetString(PyExc_TypeError,
"wrong type");
return NULL;
}
static PyObject *
c_char_p_from_param(PyObject *type, PyObject *value) {
PyObject *as_parameter;
#if (PYTHON_API_VERSION < 1012)
#error not supported
#endif
if (value == Py_None) {
Py_INCREF(Py_None);
return Py_None;
}
if (PyString_Check(value) || PyUnicode_Check(value)) {
PyCArgObject *parg;
struct fielddesc *fd = getentry("z");
parg = new_CArgObject();
if (parg == NULL)
return NULL;
parg->pffi_type = &ffi_type_pointer;
parg->tag = 'z';
parg->obj = fd->setfunc(&parg->value, value, 0);
if (parg->obj == NULL) {
Py_DECREF(parg);
return NULL;
}
return (PyObject *)parg;
}
if (PyObject_IsInstance(value, type)) {
Py_INCREF(value);
return value;
}
if (ArrayObject_Check(value) || PointerObject_Check(value)) {
StgDictObject *dt = PyObject_stgdict(value);
StgDictObject *dict;
assert(dt);
dict = dt && dt->proto ? PyType_stgdict(dt->proto) : NULL;
if (dict && (dict->setfunc == getentry("c")->setfunc)) {
Py_INCREF(value);
return value;
}
}
if (PyCArg_CheckExact(value)) {
PyCArgObject *a = (PyCArgObject *)value;
StgDictObject *dict = PyObject_stgdict(a->obj);
if (dict && (dict->setfunc == getentry("c")->setfunc)) {
Py_INCREF(value);
return value;
}
}
as_parameter = PyObject_GetAttrString(value, "_as_parameter_");
if (as_parameter) {
value = c_char_p_from_param(type, as_parameter);
Py_DECREF(as_parameter);
return value;
}
PyErr_SetString(PyExc_TypeError,
"wrong type");
return NULL;
}
static PyObject *
c_void_p_from_param(PyObject *type, PyObject *value) {
StgDictObject *stgd;
PyObject *as_parameter;
#if (PYTHON_API_VERSION < 1012)
#error not supported
#endif
if (value == Py_None) {
Py_INCREF(Py_None);
return Py_None;
}
if (PyInt_Check(value) || PyLong_Check(value)) {
PyCArgObject *parg;
struct fielddesc *fd = getentry("P");
parg = new_CArgObject();
if (parg == NULL)
return NULL;
parg->pffi_type = &ffi_type_pointer;
parg->tag = 'P';
parg->obj = fd->setfunc(&parg->value, value, 0);
if (parg->obj == NULL) {
Py_DECREF(parg);
return NULL;
}
return (PyObject *)parg;
}
if (PyString_Check(value)) {
PyCArgObject *parg;
struct fielddesc *fd = getentry("z");
parg = new_CArgObject();
if (parg == NULL)
return NULL;
parg->pffi_type = &ffi_type_pointer;
parg->tag = 'z';
parg->obj = fd->setfunc(&parg->value, value, 0);
if (parg->obj == NULL) {
Py_DECREF(parg);
return NULL;
}
return (PyObject *)parg;
}
if (PyUnicode_Check(value)) {
PyCArgObject *parg;
struct fielddesc *fd = getentry("Z");
parg = new_CArgObject();
if (parg == NULL)
return NULL;
parg->pffi_type = &ffi_type_pointer;
parg->tag = 'Z';
parg->obj = fd->setfunc(&parg->value, value, 0);
if (parg->obj == NULL) {
Py_DECREF(parg);
return NULL;
}
return (PyObject *)parg;
}
if (PyObject_IsInstance(value, type)) {
Py_INCREF(value);
return value;
}
if (ArrayObject_Check(value) || PointerObject_Check(value)) {
Py_INCREF(value);
return value;
}
if (PyCArg_CheckExact(value)) {
PyCArgObject *a = (PyCArgObject *)value;
if (a->tag == 'P') {
Py_INCREF(value);
return value;
}
}
if (CFuncPtrObject_Check(value)) {
PyCArgObject *parg;
CFuncPtrObject *func;
func = (CFuncPtrObject *)value;
parg = new_CArgObject();
if (parg == NULL)
return NULL;
parg->pffi_type = &ffi_type_pointer;
parg->tag = 'P';
Py_INCREF(value);
parg->value.p = *(void **)func->b_ptr;
parg->obj = value;
return (PyObject *)parg;
}
stgd = PyObject_stgdict(value);
if (stgd && CDataObject_Check(value) && stgd->proto && PyString_Check(stgd->proto)) {
PyCArgObject *parg;
switch (PyString_AS_STRING(stgd->proto)[0]) {
case 'z':
case 'Z':
parg = new_CArgObject();
if (parg == NULL)
return NULL;
parg->pffi_type = &ffi_type_pointer;
parg->tag = 'Z';
Py_INCREF(value);
parg->obj = value;
parg->value.p = *(void **)(((CDataObject *)value)->b_ptr);
return (PyObject *)parg;
}
}
as_parameter = PyObject_GetAttrString(value, "_as_parameter_");
if (as_parameter) {
value = c_void_p_from_param(type, as_parameter);
Py_DECREF(as_parameter);
return value;
}
PyErr_SetString(PyExc_TypeError,
"wrong type");
return NULL;
}
#if (PYTHON_API_VERSION >= 1012)
static PyMethodDef c_void_p_method = { "from_param", c_void_p_from_param, METH_O };
static PyMethodDef c_char_p_method = { "from_param", c_char_p_from_param, METH_O };
static PyMethodDef c_wchar_p_method = { "from_param", c_wchar_p_from_param, METH_O };
#else
#error
static PyMethodDef c_void_p_method = { "from_param", c_void_p_from_param, METH_VARARGS };
static PyMethodDef c_char_p_method = { "from_param", c_char_p_from_param, METH_VARARGS };
static PyMethodDef c_wchar_p_method = { "from_param", c_wchar_p_from_param, METH_VARARGS };
#endif
static PyObject *CreateSwappedType(PyTypeObject *type, PyObject *args, PyObject *kwds,
PyObject *proto, struct fielddesc *fmt) {
PyTypeObject *result;
StgDictObject *stgdict;
PyObject *name = PyTuple_GET_ITEM(args, 0);
PyObject *swapped_args;
static PyObject *suffix;
Py_ssize_t i;
swapped_args = PyTuple_New(PyTuple_GET_SIZE(args));
if (!swapped_args)
return NULL;
if (suffix == NULL)
#if defined(WORDS_BIGENDIAN)
suffix = PyString_InternFromString("_le");
#else
suffix = PyString_InternFromString("_be");
#endif
Py_INCREF(name);
PyString_Concat(&name, suffix);
if (name == NULL)
return NULL;
PyTuple_SET_ITEM(swapped_args, 0, name);
for (i=1; i<PyTuple_GET_SIZE(args); ++i) {
PyObject *v = PyTuple_GET_ITEM(args, i);
Py_INCREF(v);
PyTuple_SET_ITEM(swapped_args, i, v);
}
result = (PyTypeObject *)PyType_Type.tp_new(type, swapped_args, kwds);
Py_DECREF(swapped_args);
if (result == NULL)
return NULL;
stgdict = (StgDictObject *)PyObject_CallObject(
(PyObject *)&StgDict_Type, NULL);
if (!stgdict)
return NULL;
stgdict->ffi_type_pointer = *fmt->pffi_type;
stgdict->align = fmt->pffi_type->alignment;
stgdict->length = 0;
stgdict->size = fmt->pffi_type->size;
stgdict->setfunc = fmt->setfunc_swapped;
stgdict->getfunc = fmt->getfunc_swapped;
Py_INCREF(proto);
stgdict->proto = proto;
if (-1 == PyDict_Update((PyObject *)stgdict, result->tp_dict)) {
Py_DECREF(result);
Py_DECREF((PyObject *)stgdict);
return NULL;
}
Py_DECREF(result->tp_dict);
result->tp_dict = (PyObject *)stgdict;
return (PyObject *)result;
}
static PyCArgObject *
SimpleType_paramfunc(CDataObject *self) {
StgDictObject *dict;
char *fmt;
PyCArgObject *parg;
struct fielddesc *fd;
dict = PyObject_stgdict((PyObject *)self);
assert(dict);
fmt = PyString_AsString(dict->proto);
assert(fmt);
fd = getentry(fmt);
assert(fd);
parg = new_CArgObject();
if (parg == NULL)
return NULL;
parg->tag = fmt[0];
parg->pffi_type = fd->pffi_type;
Py_INCREF(self);
parg->obj = (PyObject *)self;
memcpy(&parg->value, self->b_ptr, self->b_size);
return parg;
}
static PyObject *
SimpleType_new(PyTypeObject *type, PyObject *args, PyObject *kwds) {
PyTypeObject *result;
StgDictObject *stgdict;
PyObject *proto;
const char *proto_str;
Py_ssize_t proto_len;
PyMethodDef *ml;
struct fielddesc *fmt;
result = (PyTypeObject *)PyType_Type.tp_new(type, args, kwds);
if (result == NULL)
return NULL;
proto = PyObject_GetAttrString((PyObject *)result, "_type_");
if (!proto) {
PyErr_SetString(PyExc_AttributeError,
"class must define a '_type_' attribute");
error:
Py_XDECREF(proto);
Py_XDECREF(result);
return NULL;
}
if (PyString_Check(proto)) {
proto_str = PyString_AS_STRING(proto);
proto_len = PyString_GET_SIZE(proto);
} else {
PyErr_SetString(PyExc_TypeError,
"class must define a '_type_' string attribute");
goto error;
}
if (proto_len != 1) {
PyErr_SetString(PyExc_ValueError,
"class must define a '_type_' attribute "
"which must be a string of length 1");
goto error;
}
if (!strchr(SIMPLE_TYPE_CHARS, *proto_str)) {
PyErr_Format(PyExc_AttributeError,
"class must define a '_type_' attribute which must be\n"
"a single character string containing one of '%s'.",
SIMPLE_TYPE_CHARS);
goto error;
}
fmt = getentry(PyString_AS_STRING(proto));
if (fmt == NULL) {
Py_DECREF(result);
PyErr_Format(PyExc_ValueError,
"_type_ '%s' not supported",
PyString_AS_STRING(proto));
return NULL;
}
stgdict = (StgDictObject *)PyObject_CallObject(
(PyObject *)&StgDict_Type, NULL);
if (!stgdict)
return NULL;
stgdict->ffi_type_pointer = *fmt->pffi_type;
stgdict->align = fmt->pffi_type->alignment;
stgdict->length = 0;
stgdict->size = fmt->pffi_type->size;
stgdict->setfunc = fmt->setfunc;
stgdict->getfunc = fmt->getfunc;
#if defined(WORDS_BIGENDIAN)
stgdict->format = alloc_format_string(">", proto_str);
#else
stgdict->format = alloc_format_string("<", proto_str);
#endif
if (stgdict->format == NULL) {
Py_DECREF(result);
Py_DECREF((PyObject *)stgdict);
return NULL;
}
stgdict->paramfunc = SimpleType_paramfunc;
stgdict->proto = proto;
if (-1 == PyDict_Update((PyObject *)stgdict, result->tp_dict)) {
Py_DECREF(result);
Py_DECREF((PyObject *)stgdict);
return NULL;
}
Py_DECREF(result->tp_dict);
result->tp_dict = (PyObject *)stgdict;
if (result->tp_base == &Simple_Type) {
switch (PyString_AS_STRING(proto)[0]) {
case 'z':
ml = &c_char_p_method;
stgdict->flags |= TYPEFLAG_ISPOINTER;
break;
case 'Z':
ml = &c_wchar_p_method;
stgdict->flags |= TYPEFLAG_ISPOINTER;
break;
case 'P':
ml = &c_void_p_method;
stgdict->flags |= TYPEFLAG_ISPOINTER;
break;
case 'u':
case 'X':
case 'O':
ml = NULL;
stgdict->flags |= TYPEFLAG_ISPOINTER;
break;
default:
ml = NULL;
break;
}
if (ml) {
#if (PYTHON_API_VERSION >= 1012)
PyObject *meth;
int x;
meth = PyDescr_NewClassMethod(result, ml);
if (!meth)
return NULL;
#else
#error
PyObject *meth, *func;
int x;
func = PyCFunction_New(ml, NULL);
if (!func)
return NULL;
meth = PyObject_CallFunctionObjArgs(
(PyObject *)&PyClassMethod_Type,
func, NULL);
Py_DECREF(func);
if (!meth) {
return NULL;
}
#endif
x = PyDict_SetItemString(result->tp_dict,
ml->ml_name,
meth);
Py_DECREF(meth);
if (x == -1) {
Py_DECREF(result);
return NULL;
}
}
}
if (type == &SimpleType_Type && fmt->setfunc_swapped && fmt->getfunc_swapped) {
PyObject *swapped = CreateSwappedType(type, args, kwds,
proto, fmt);
StgDictObject *sw_dict;
if (swapped == NULL) {
Py_DECREF(result);
return NULL;
}
sw_dict = PyType_stgdict(swapped);
#if defined(WORDS_BIGENDIAN)
PyObject_SetAttrString((PyObject *)result, "__ctype_le__", swapped);
PyObject_SetAttrString((PyObject *)result, "__ctype_be__", (PyObject *)result);
PyObject_SetAttrString(swapped, "__ctype_be__", (PyObject *)result);
PyObject_SetAttrString(swapped, "__ctype_le__", swapped);
sw_dict->format = alloc_format_string("<", stgdict->format+1);
#else
PyObject_SetAttrString((PyObject *)result, "__ctype_be__", swapped);
PyObject_SetAttrString((PyObject *)result, "__ctype_le__", (PyObject *)result);
PyObject_SetAttrString(swapped, "__ctype_le__", (PyObject *)result);
PyObject_SetAttrString(swapped, "__ctype_be__", swapped);
sw_dict->format = alloc_format_string(">", stgdict->format+1);
#endif
Py_DECREF(swapped);
if (PyErr_Occurred()) {
Py_DECREF(result);
return NULL;
}
};
return (PyObject *)result;
}
static PyObject *
SimpleType_from_param(PyObject *type, PyObject *value) {
StgDictObject *dict;
char *fmt;
PyCArgObject *parg;
struct fielddesc *fd;
PyObject *as_parameter;
if (1 == PyObject_IsInstance(value, type)) {
Py_INCREF(value);
return value;
}
dict = PyType_stgdict(type);
assert(dict);
fmt = PyString_AsString(dict->proto);
assert(fmt);
fd = getentry(fmt);
assert(fd);
parg = new_CArgObject();
if (parg == NULL)
return NULL;
parg->tag = fmt[0];
parg->pffi_type = fd->pffi_type;
parg->obj = fd->setfunc(&parg->value, value, 0);
if (parg->obj)
return (PyObject *)parg;
PyErr_Clear();
Py_DECREF(parg);
as_parameter = PyObject_GetAttrString(value, "_as_parameter_");
if (as_parameter) {
value = SimpleType_from_param(type, as_parameter);
Py_DECREF(as_parameter);
return value;
}
PyErr_SetString(PyExc_TypeError,
"wrong type");
return NULL;
}
static PyMethodDef SimpleType_methods[] = {
{ "from_param", SimpleType_from_param, METH_O, from_param_doc },
{ "from_address", CDataType_from_address, METH_O, from_address_doc },
{ "from_buffer", CDataType_from_buffer, METH_VARARGS, from_buffer_doc, },
{ "from_buffer_copy", CDataType_from_buffer_copy, METH_VARARGS, from_buffer_copy_doc, },
{ "in_dll", CDataType_in_dll, METH_VARARGS, in_dll_doc},
{ NULL, NULL },
};
PyTypeObject SimpleType_Type = {
PyVarObject_HEAD_INIT(NULL, 0)
"_ctypes.SimpleType",
0,
0,
0,
0,
0,
0,
0,
0,
0,
&CDataType_as_sequence,
0,
0,
0,
0,
0,
0,
0,
Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
"metatype for the SimpleType Objects",
0,
0,
0,
0,
0,
0,
SimpleType_methods,
0,
0,
0,
0,
0,
0,
0,
0,
0,
SimpleType_new,
0,
};
static PyObject *
converters_from_argtypes(PyObject *ob) {
PyObject *converters;
Py_ssize_t i;
Py_ssize_t nArgs;
ob = PySequence_Tuple(ob);
if (!ob) {
PyErr_SetString(PyExc_TypeError,
"_argtypes_ must be a sequence of types");
return NULL;
}
nArgs = PyTuple_GET_SIZE(ob);
converters = PyTuple_New(nArgs);
if (!converters)
return NULL;
for (i = 0; i < nArgs; ++i) {
PyObject *tp = PyTuple_GET_ITEM(ob, i);
PyObject *cnv = PyObject_GetAttrString(tp, "from_param");
if (!cnv)
goto argtypes_error_1;
PyTuple_SET_ITEM(converters, i, cnv);
}
Py_DECREF(ob);
return converters;
argtypes_error_1:
Py_XDECREF(converters);
Py_DECREF(ob);
PyErr_Format(PyExc_TypeError,
#if (PY_VERSION_HEX < 0x02050000)
"item %d in _argtypes_ has no from_param method",
#else
"item %zd in _argtypes_ has no from_param method",
#endif
i+1);
return NULL;
}
static int
make_funcptrtype_dict(StgDictObject *stgdict) {
PyObject *ob;
PyObject *converters = NULL;
stgdict->align = getentry("P")->pffi_type->alignment;
stgdict->length = 1;
stgdict->size = sizeof(void *);
stgdict->setfunc = NULL;
stgdict->getfunc = NULL;
stgdict->ffi_type_pointer = ffi_type_pointer;
ob = PyDict_GetItemString((PyObject *)stgdict, "_flags_");
if (!ob || !PyInt_Check(ob)) {
PyErr_SetString(PyExc_TypeError,
"class must define _flags_ which must be an integer");
return -1;
}
stgdict->flags = PyInt_AS_LONG(ob) | TYPEFLAG_ISPOINTER;
ob = PyDict_GetItemString((PyObject *)stgdict, "_argtypes_");
if (ob) {
converters = converters_from_argtypes(ob);
if (!converters)
goto error;
Py_INCREF(ob);
stgdict->argtypes = ob;
stgdict->converters = converters;
}
ob = PyDict_GetItemString((PyObject *)stgdict, "_restype_");
if (ob) {
if (ob != Py_None && !PyType_stgdict(ob) && !PyCallable_Check(ob)) {
PyErr_SetString(PyExc_TypeError,
"_restype_ must be a type, a callable, or None");
return -1;
}
Py_INCREF(ob);
stgdict->restype = ob;
stgdict->checker = PyObject_GetAttrString(ob, "_check_retval_");
if (stgdict->checker == NULL)
PyErr_Clear();
}
return 0;
error:
Py_XDECREF(converters);
return -1;
}
static PyCArgObject *
CFuncPtrType_paramfunc(CDataObject *self) {
PyCArgObject *parg;
parg = new_CArgObject();
if (parg == NULL)
return NULL;
parg->tag = 'P';
parg->pffi_type = &ffi_type_pointer;
Py_INCREF(self);
parg->obj = (PyObject *)self;
parg->value.p = *(void **)self->b_ptr;
return parg;
}
static PyObject *
CFuncPtrType_new(PyTypeObject *type, PyObject *args, PyObject *kwds) {
PyTypeObject *result;
StgDictObject *stgdict;
stgdict = (StgDictObject *)PyObject_CallObject(
(PyObject *)&StgDict_Type, NULL);
if (!stgdict)
return NULL;
stgdict->paramfunc = CFuncPtrType_paramfunc;
stgdict->format = alloc_format_string(NULL, "X{}");
stgdict->flags |= TYPEFLAG_ISPOINTER;
result = (PyTypeObject *)PyType_Type.tp_new(type, args, kwds);
if (result == NULL) {
Py_DECREF((PyObject *)stgdict);
return NULL;
}
if (-1 == PyDict_Update((PyObject *)stgdict, result->tp_dict)) {
Py_DECREF(result);
Py_DECREF((PyObject *)stgdict);
return NULL;
}
Py_DECREF(result->tp_dict);
result->tp_dict = (PyObject *)stgdict;
if (-1 == make_funcptrtype_dict(stgdict)) {
Py_DECREF(result);
return NULL;
}
return (PyObject *)result;
}
PyTypeObject CFuncPtrType_Type = {
PyVarObject_HEAD_INIT(NULL, 0)
"_ctypes.CFuncPtrType",
0,
0,
0,
0,
0,
0,
0,
0,
0,
&CDataType_as_sequence,
0,
0,
0,
0,
0,
0,
0,
Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE | Py_TPFLAGS_HAVE_GC,
"metatype for C function pointers",
(traverseproc)CDataType_traverse,
(inquiry)CDataType_clear,
0,
0,
0,
0,
CDataType_methods,
0,
0,
0,
0,
0,
0,
0,
0,
0,
CFuncPtrType_new,
0,
};
static CDataObject *
CData_GetContainer(CDataObject *self) {
while (self->b_base)
self = self->b_base;
if (self->b_objects == NULL) {
if (self->b_length) {
self->b_objects = PyDict_New();
} else {
Py_INCREF(Py_None);
self->b_objects = Py_None;
}
}
return self;
}
static PyObject *
GetKeepedObjects(CDataObject *target) {
return CData_GetContainer(target)->b_objects;
}
static PyObject *
unique_key(CDataObject *target, Py_ssize_t index) {
char string[256];
char *cp = string;
size_t bytes_left;
assert(sizeof(string) - 1 > sizeof(Py_ssize_t) * 2);
#if (PY_VERSION_HEX < 0x02050000)
cp += sprintf(cp, "%x", index);
#else
cp += sprintf(cp, "%x", Py_SAFE_DOWNCAST(index, Py_ssize_t, int));
#endif
while (target->b_base) {
bytes_left = sizeof(string) - (cp - string) - 1;
if (bytes_left < sizeof(Py_ssize_t) * 2) {
PyErr_SetString(PyExc_ValueError,
"ctypes object structure too deep");
return NULL;
}
#if (PY_VERSION_HEX < 0x02050000)
cp += sprintf(cp, ":%x", (int)target->b_index);
#else
cp += sprintf(cp, ":%x", Py_SAFE_DOWNCAST(target->b_index, Py_ssize_t, int));
#endif
target = target->b_base;
}
return PyString_FromStringAndSize(string, cp-string);
}
static int
KeepRef(CDataObject *target, Py_ssize_t index, PyObject *keep) {
int result;
CDataObject *ob;
PyObject *key;
if (keep == Py_None) {
Py_DECREF(Py_None);
return 0;
}
ob = CData_GetContainer(target);
if (ob->b_objects == NULL || !PyDict_CheckExact(ob->b_objects)) {
Py_XDECREF(ob->b_objects);
ob->b_objects = keep;
return 0;
}
key = unique_key(target, index);
if (key == NULL) {
Py_DECREF(keep);
return -1;
}
result = PyDict_SetItem(ob->b_objects, key, keep);
Py_DECREF(key);
Py_DECREF(keep);
return result;
}
static int
CData_traverse(CDataObject *self, visitproc visit, void *arg) {
Py_VISIT(self->b_objects);
Py_VISIT((PyObject *)self->b_base);
return 0;
}
static int
CData_clear(CDataObject *self) {
StgDictObject *dict = PyObject_stgdict((PyObject *)self);
assert(dict);
Py_CLEAR(self->b_objects);
if ((self->b_needsfree)
&& ((size_t)dict->size > sizeof(self->b_value)))
PyMem_Free(self->b_ptr);
self->b_ptr = NULL;
Py_CLEAR(self->b_base);
return 0;
}
static void
CData_dealloc(PyObject *self) {
CData_clear((CDataObject *)self);
Py_TYPE(self)->tp_free(self);
}
static PyMemberDef CData_members[] = {
{
"_b_base_", T_OBJECT,
offsetof(CDataObject, b_base), READONLY,
"the base object"
},
{
"_b_needsfree_", T_INT,
offsetof(CDataObject, b_needsfree), READONLY,
"whether the object owns the memory or not"
},
{
"_objects", T_OBJECT,
offsetof(CDataObject, b_objects), READONLY,
"internal objects tree (NEVER CHANGE THIS OBJECT!)"
},
{ NULL },
};
#if (PY_VERSION_HEX >= 0x02060000)
static int CData_NewGetBuffer(PyObject *_self, Py_buffer *view, int flags) {
CDataObject *self = (CDataObject *)_self;
StgDictObject *dict = PyObject_stgdict(_self);
Py_ssize_t i;
if (view == NULL) return 0;
view->buf = self->b_ptr;
view->obj = _self;
Py_INCREF(_self);
view->len = self->b_size;
view->readonly = 0;
view->format = dict->format ? dict->format : "B";
view->ndim = dict->ndim;
view->shape = dict->shape;
view->itemsize = self->b_size;
for (i = 0; i < view->ndim; ++i) {
view->itemsize /= dict->shape[i];
}
view->strides = NULL;
view->suboffsets = NULL;
view->internal = NULL;
return 0;
}
#endif
static Py_ssize_t CData_GetSegcount(PyObject *_self, Py_ssize_t *lenp) {
if (lenp)
*lenp = 1;
return 1;
}
static Py_ssize_t CData_GetBuffer(PyObject *_self, Py_ssize_t seg, void **pptr) {
CDataObject *self = (CDataObject *)_self;
if (seg != 0) {
return -1;
}
*pptr = self->b_ptr;
return self->b_size;
}
static PyBufferProcs CData_as_buffer = {
(readbufferproc)CData_GetBuffer,
(writebufferproc)CData_GetBuffer,
(segcountproc)CData_GetSegcount,
(charbufferproc)NULL,
#if (PY_VERSION_HEX >= 0x02060000)
(getbufferproc)CData_NewGetBuffer,
(releasebufferproc)NULL,
#endif
};
static long
CData_nohash(PyObject *self) {
PyErr_SetString(PyExc_TypeError, "unhashable type");
return -1;
}
static PyObject *
CData_reduce(PyObject *_self, PyObject *args) {
CDataObject *self = (CDataObject *)_self;
if (PyObject_stgdict(_self)->flags & (TYPEFLAG_ISPOINTER|TYPEFLAG_HASPOINTER)) {
PyErr_SetString(PyExc_ValueError,
"ctypes objects containing pointers cannot be pickled");
return NULL;
}
return Py_BuildValue("O(O(NN))",
_unpickle,
Py_TYPE(_self),
PyObject_GetAttrString(_self, "__dict__"),
PyString_FromStringAndSize(self->b_ptr, self->b_size));
}
static PyObject *
CData_setstate(PyObject *_self, PyObject *args) {
void *data;
Py_ssize_t len;
int res;
PyObject *dict, *mydict;
CDataObject *self = (CDataObject *)_self;
if (!PyArg_ParseTuple(args, "Os#", &dict, &data, &len))
return NULL;
if (len > self->b_size)
len = self->b_size;
memmove(self->b_ptr, data, len);
mydict = PyObject_GetAttrString(_self, "__dict__");
res = PyDict_Update(mydict, dict);
Py_DECREF(mydict);
if (res == -1)
return NULL;
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
CData_from_outparam(PyObject *self, PyObject *args) {
Py_INCREF(self);
return self;
}
static PyMethodDef CData_methods[] = {
{ "__ctypes_from_outparam__", CData_from_outparam, METH_NOARGS, },
{ "__reduce__", CData_reduce, METH_NOARGS, },
{ "__setstate__", CData_setstate, METH_VARARGS, },
{ NULL, NULL },
};
PyTypeObject CData_Type = {
PyVarObject_HEAD_INIT(NULL, 0)
"_ctypes._CData",
sizeof(CDataObject),
0,
CData_dealloc,
0,
0,
0,
0,
0,
0,
0,
0,
CData_nohash,
0,
0,
0,
0,
&CData_as_buffer,
Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_NEWBUFFER | Py_TPFLAGS_BASETYPE,
"XXX to be provided",
(traverseproc)CData_traverse,
(inquiry)CData_clear,
0,
0,
0,
0,
CData_methods,
CData_members,
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
static int CData_MallocBuffer(CDataObject *obj, StgDictObject *dict) {
if ((size_t)dict->size <= sizeof(obj->b_value)) {
obj->b_ptr = (char *)&obj->b_value;
obj->b_needsfree = 1;
} else {
obj->b_ptr = (char *)PyMem_Malloc(dict->size);
if (obj->b_ptr == NULL) {
PyErr_NoMemory();
return -1;
}
obj->b_needsfree = 1;
memset(obj->b_ptr, 0, dict->size);
}
obj->b_size = dict->size;
return 0;
}
PyObject *
CData_FromBaseObj(PyObject *type, PyObject *base, Py_ssize_t index, char *adr) {
CDataObject *cmem;
StgDictObject *dict;
assert(PyType_Check(type));
dict = PyType_stgdict(type);
if (!dict) {
PyErr_SetString(PyExc_TypeError,
"abstract class");
return NULL;
}
dict->flags |= DICTFLAG_FINAL;
cmem = (CDataObject *)((PyTypeObject *)type)->tp_alloc((PyTypeObject *)type, 0);
if (cmem == NULL)
return NULL;
assert(CDataObject_Check(cmem));
cmem->b_length = dict->length;
cmem->b_size = dict->size;
if (base) {
assert(CDataObject_Check(base));
cmem->b_ptr = adr;
cmem->b_needsfree = 0;
Py_INCREF(base);
cmem->b_base = (CDataObject *)base;
cmem->b_index = index;
} else {
if (-1 == CData_MallocBuffer(cmem, dict)) {
return NULL;
Py_DECREF(cmem);
}
memcpy(cmem->b_ptr, adr, dict->size);
cmem->b_index = index;
}
return (PyObject *)cmem;
}
PyObject *
CData_AtAddress(PyObject *type, void *buf) {
CDataObject *pd;
StgDictObject *dict;
assert(PyType_Check(type));
dict = PyType_stgdict(type);
if (!dict) {
PyErr_SetString(PyExc_TypeError,
"abstract class");
return NULL;
}
dict->flags |= DICTFLAG_FINAL;
pd = (CDataObject *)((PyTypeObject *)type)->tp_alloc((PyTypeObject *)type, 0);
if (!pd)
return NULL;
assert(CDataObject_Check(pd));
pd->b_ptr = (char *)buf;
pd->b_length = dict->length;
pd->b_size = dict->size;
return (PyObject *)pd;
}
int IsSimpleSubType(PyObject *obj) {
PyTypeObject *type = (PyTypeObject *)obj;
if (SimpleTypeObject_Check(type))
return type->tp_base != &Simple_Type;
return 0;
}
PyObject *
CData_get(PyObject *type, GETFUNC getfunc, PyObject *src,
Py_ssize_t index, Py_ssize_t size, char *adr) {
StgDictObject *dict;
if (getfunc)
return getfunc(adr, size);
assert(type);
dict = PyType_stgdict(type);
if (dict && dict->getfunc && !IsSimpleSubType(type))
return dict->getfunc(adr, size);
return CData_FromBaseObj(type, src, index, adr);
}
static PyObject *
_CData_set(CDataObject *dst, PyObject *type, SETFUNC setfunc, PyObject *value,
Py_ssize_t size, char *ptr) {
CDataObject *src;
if (setfunc)
return setfunc(ptr, value, size);
if (!CDataObject_Check(value)) {
StgDictObject *dict = PyType_stgdict(type);
if (dict && dict->setfunc)
return dict->setfunc(ptr, value, size);
assert(PyType_Check(type));
if (PyTuple_Check(value)) {
PyObject *ob;
PyObject *result;
ob = PyObject_CallObject(type, value);
if (ob == NULL) {
Extend_Error_Info(PyExc_RuntimeError, "(%s) ",
((PyTypeObject *)type)->tp_name);
return NULL;
}
result = _CData_set(dst, type, setfunc, ob,
size, ptr);
Py_DECREF(ob);
return result;
} else if (value == Py_None && PointerTypeObject_Check(type)) {
*(void **)ptr = NULL;
Py_INCREF(Py_None);
return Py_None;
} else {
PyErr_Format(PyExc_TypeError,
"expected %s instance, got %s",
((PyTypeObject *)type)->tp_name,
Py_TYPE(value)->tp_name);
return NULL;
}
}
src = (CDataObject *)value;
if (PyObject_IsInstance(value, type)) {
memcpy(ptr,
src->b_ptr,
size);
if (PointerTypeObject_Check(type))
;
value = GetKeepedObjects(src);
Py_INCREF(value);
return value;
}
if (PointerTypeObject_Check(type)
&& ArrayObject_Check(value)) {
StgDictObject *p1, *p2;
PyObject *keep;
p1 = PyObject_stgdict(value);
assert(p1);
p2 = PyType_stgdict(type);
assert(p2);
if (p1->proto != p2->proto) {
PyErr_Format(PyExc_TypeError,
"incompatible types, %s instance instead of %s instance",
Py_TYPE(value)->tp_name,
((PyTypeObject *)type)->tp_name);
return NULL;
}
*(void **)ptr = src->b_ptr;
keep = GetKeepedObjects(src);
return PyTuple_Pack(2, keep, value);
}
PyErr_Format(PyExc_TypeError,
"incompatible types, %s instance instead of %s instance",
Py_TYPE(value)->tp_name,
((PyTypeObject *)type)->tp_name);
return NULL;
}
int
CData_set(PyObject *dst, PyObject *type, SETFUNC setfunc, PyObject *value,
Py_ssize_t index, Py_ssize_t size, char *ptr) {
CDataObject *mem = (CDataObject *)dst;
PyObject *result;
if (!CDataObject_Check(dst)) {
PyErr_SetString(PyExc_TypeError,
"not a ctype instance");
return -1;
}
result = _CData_set(mem, type, setfunc, value,
size, ptr);
if (result == NULL)
return -1;
return KeepRef(mem, index, result);
}
static PyObject *
GenericCData_new(PyTypeObject *type, PyObject *args, PyObject *kwds) {
CDataObject *obj;
StgDictObject *dict;
dict = PyType_stgdict((PyObject *)type);
if (!dict) {
PyErr_SetString(PyExc_TypeError,
"abstract class");
return NULL;
}
dict->flags |= DICTFLAG_FINAL;
obj = (CDataObject *)type->tp_alloc(type, 0);
if (!obj)
return NULL;
obj->b_base = NULL;
obj->b_index = 0;
obj->b_objects = NULL;
obj->b_length = dict->length;
if (-1 == CData_MallocBuffer(obj, dict)) {
Py_DECREF(obj);
return NULL;
}
return (PyObject *)obj;
}
static int
CFuncPtr_set_errcheck(CFuncPtrObject *self, PyObject *ob) {
if (ob && !PyCallable_Check(ob)) {
PyErr_SetString(PyExc_TypeError,
"the errcheck attribute must be callable");
return -1;
}
Py_XDECREF(self->errcheck);
Py_XINCREF(ob);
self->errcheck = ob;
return 0;
}
static PyObject *
CFuncPtr_get_errcheck(CFuncPtrObject *self) {
if (self->errcheck) {
Py_INCREF(self->errcheck);
return self->errcheck;
}
Py_INCREF(Py_None);
return Py_None;
}
static int
CFuncPtr_set_restype(CFuncPtrObject *self, PyObject *ob) {
if (ob == NULL) {
Py_XDECREF(self->restype);
self->restype = NULL;
Py_XDECREF(self->checker);
self->checker = NULL;
return 0;
}
if (ob != Py_None && !PyType_stgdict(ob) && !PyCallable_Check(ob)) {
PyErr_SetString(PyExc_TypeError,
"restype must be a type, a callable, or None");
return -1;
}
Py_XDECREF(self->checker);
Py_XDECREF(self->restype);
Py_INCREF(ob);
self->restype = ob;
self->checker = PyObject_GetAttrString(ob, "_check_retval_");
if (self->checker == NULL)
PyErr_Clear();
return 0;
}
static PyObject *
CFuncPtr_get_restype(CFuncPtrObject *self) {
StgDictObject *dict;
if (self->restype) {
Py_INCREF(self->restype);
return self->restype;
}
dict = PyObject_stgdict((PyObject *)self);
assert(dict);
if (dict->restype) {
Py_INCREF(dict->restype);
return dict->restype;
} else {
Py_INCREF(Py_None);
return Py_None;
}
}
static int
CFuncPtr_set_argtypes(CFuncPtrObject *self, PyObject *ob) {
PyObject *converters;
if (ob == NULL || ob == Py_None) {
Py_XDECREF(self->converters);
self->converters = NULL;
Py_XDECREF(self->argtypes);
self->argtypes = NULL;
} else {
converters = converters_from_argtypes(ob);
if (!converters)
return -1;
Py_XDECREF(self->converters);
self->converters = converters;
Py_XDECREF(self->argtypes);
Py_INCREF(ob);
self->argtypes = ob;
}
return 0;
}
static PyObject *
CFuncPtr_get_argtypes(CFuncPtrObject *self) {
StgDictObject *dict;
if (self->argtypes) {
Py_INCREF(self->argtypes);
return self->argtypes;
}
dict = PyObject_stgdict((PyObject *)self);
assert(dict);
if (dict->argtypes) {
Py_INCREF(dict->argtypes);
return dict->argtypes;
} else {
Py_INCREF(Py_None);
return Py_None;
}
}
static PyGetSetDef CFuncPtr_getsets[] = {
{
"errcheck", (getter)CFuncPtr_get_errcheck, (setter)CFuncPtr_set_errcheck,
"a function to check for errors", NULL
},
{
"restype", (getter)CFuncPtr_get_restype, (setter)CFuncPtr_set_restype,
"specify the result type", NULL
},
{
"argtypes", (getter)CFuncPtr_get_argtypes,
(setter)CFuncPtr_set_argtypes,
"specify the argument types", NULL
},
{ NULL, NULL }
};
#if defined(MS_WIN32)
static PPROC FindAddress(void *handle, char *name, PyObject *type) {
#if defined(MS_WIN64)
return (PPROC)GetProcAddress(handle, name);
#else
PPROC address;
char *mangled_name;
int i;
StgDictObject *dict;
address = (PPROC)GetProcAddress(handle, name);
if (address)
return address;
if (((size_t)name & ~0xFFFF) == 0) {
return NULL;
}
dict = PyType_stgdict((PyObject *)type);
if (dict==NULL || dict->flags & FUNCFLAG_CDECL)
return address;
mangled_name = alloca(strlen(name) + 1 + 1 + 1 + 3);
if (!mangled_name)
return NULL;
for (i = 0; i < 32; ++i) {
sprintf(mangled_name, "_%s@%d", name, i*4);
address = (PPROC)GetProcAddress(handle, mangled_name);
if (address)
return address;
}
return NULL;
#endif
}
#endif
static int
_check_outarg_type(PyObject *arg, Py_ssize_t index) {
StgDictObject *dict;
if (PointerTypeObject_Check(arg))
return 1;
if (ArrayTypeObject_Check(arg))
return 1;
dict = PyType_stgdict(arg);
if (dict
&& PyString_Check(dict->proto)
&& (strchr("PzZ", PyString_AS_STRING(dict->proto)[0]))) {
return 1;
}
PyErr_Format(PyExc_TypeError,
"'out' parameter %d must be a pointer type, not %s",
Py_SAFE_DOWNCAST(index, Py_ssize_t, int),
PyType_Check(arg) ?
((PyTypeObject *)arg)->tp_name :
Py_TYPE(arg)->tp_name);
return 0;
}
static int
_validate_paramflags(PyTypeObject *type, PyObject *paramflags) {
Py_ssize_t i, len;
StgDictObject *dict;
PyObject *argtypes;
dict = PyType_stgdict((PyObject *)type);
assert(dict);
argtypes = dict->argtypes;
if (paramflags == NULL || dict->argtypes == NULL)
return 1;
if (!PyTuple_Check(paramflags)) {
PyErr_SetString(PyExc_TypeError,
"paramflags must be a tuple or None");
return 0;
}
len = PyTuple_GET_SIZE(paramflags);
if (len != PyTuple_GET_SIZE(dict->argtypes)) {
PyErr_SetString(PyExc_ValueError,
"paramflags must have the same length as argtypes");
return 0;
}
for (i = 0; i < len; ++i) {
PyObject *item = PyTuple_GET_ITEM(paramflags, i);
int flag;
char *name;
PyObject *defval;
PyObject *typ;
if (!PyArg_ParseTuple(item, "i|zO", &flag, &name, &defval)) {
PyErr_SetString(PyExc_TypeError,
"paramflags must be a sequence of (int [,string [,value]]) tuples");
return 0;
}
typ = PyTuple_GET_ITEM(argtypes, i);
switch (flag & (PARAMFLAG_FIN | PARAMFLAG_FOUT | PARAMFLAG_FLCID)) {
case 0:
case PARAMFLAG_FIN:
case PARAMFLAG_FIN | PARAMFLAG_FLCID:
case PARAMFLAG_FIN | PARAMFLAG_FOUT:
break;
case PARAMFLAG_FOUT:
if (!_check_outarg_type(typ, i+1))
return 0;
break;
default:
PyErr_Format(PyExc_TypeError,
"paramflag value %d not supported",
flag);
return 0;
}
}
return 1;
}
static int
_get_name(PyObject *obj, char **pname) {
#if defined(MS_WIN32)
if (PyInt_Check(obj) || PyLong_Check(obj)) {
*pname = MAKEINTRESOURCEA(PyInt_AsUnsignedLongMask(obj) & 0xFFFF);
return 1;
}
#endif
if (PyString_Check(obj) || PyUnicode_Check(obj)) {
*pname = PyString_AsString(obj);
return *pname ? 1 : 0;
}
PyErr_SetString(PyExc_TypeError,
"function name must be string or integer");
return 0;
}
static PyObject *
CFuncPtr_FromDll(PyTypeObject *type, PyObject *args, PyObject *kwds) {
char *name;
int (* address)(void);
PyObject *dll;
PyObject *obj;
CFuncPtrObject *self;
void *handle;
PyObject *paramflags = NULL;
if (!PyArg_ParseTuple(args, "(O&O)|O", _get_name, &name, &dll, &paramflags))
return NULL;
if (paramflags == Py_None)
paramflags = NULL;
obj = PyObject_GetAttrString(dll, "_handle");
if (!obj)
return NULL;
if (!PyInt_Check(obj) && !PyLong_Check(obj)) {
PyErr_SetString(PyExc_TypeError,
"the _handle attribute of the second argument must be an integer");
Py_DECREF(obj);
return NULL;
}
handle = (void *)PyLong_AsVoidPtr(obj);
Py_DECREF(obj);
if (PyErr_Occurred()) {
PyErr_SetString(PyExc_ValueError,
"could not convert the _handle attribute to a pointer");
return NULL;
}
#if defined(MS_WIN32)
address = FindAddress(handle, name, (PyObject *)type);
if (!address) {
if (!IS_INTRESOURCE(name))
PyErr_Format(PyExc_AttributeError,
"function '%s' not found",
name);
else
PyErr_Format(PyExc_AttributeError,
"function ordinal %d not found",
(WORD)(size_t)name);
return NULL;
}
#else
address = (PPROC)ctypes_dlsym(handle, name);
if (!address) {
PyErr_Format(PyExc_AttributeError,
#if defined(__CYGWIN__)
"function '%s' not found (%s) ",
name,
#endif
ctypes_dlerror());
return NULL;
}
#endif
if (!_validate_paramflags(type, paramflags))
return NULL;
self = (CFuncPtrObject *)GenericCData_new(type, args, kwds);
if (!self)
return NULL;
Py_XINCREF(paramflags);
self->paramflags = paramflags;
*(void **)self->b_ptr = address;
Py_INCREF((PyObject *)dll);
if (-1 == KeepRef((CDataObject *)self, 0, dll)) {
Py_DECREF((PyObject *)self);
return NULL;
}
Py_INCREF(self);
self->callable = (PyObject *)self;
return (PyObject *)self;
}
#if defined(MS_WIN32)
static PyObject *
CFuncPtr_FromVtblIndex(PyTypeObject *type, PyObject *args, PyObject *kwds) {
CFuncPtrObject *self;
int index;
char *name = NULL;
PyObject *paramflags = NULL;
GUID *iid = NULL;
Py_ssize_t iid_len = 0;
if (!PyArg_ParseTuple(args, "is|Oz#", &index, &name, &paramflags, &iid, &iid_len))
return NULL;
if (paramflags == Py_None)
paramflags = NULL;
if (!_validate_paramflags(type, paramflags))
return NULL;
self = (CFuncPtrObject *)GenericCData_new(type, args, kwds);
self->index = index + 0x1000;
Py_XINCREF(paramflags);
self->paramflags = paramflags;
if (iid_len == sizeof(GUID))
self->iid = iid;
return (PyObject *)self;
}
#endif
static PyObject *
CFuncPtr_new(PyTypeObject *type, PyObject *args, PyObject *kwds) {
CFuncPtrObject *self;
PyObject *callable;
StgDictObject *dict;
CThunkObject *thunk;
if (PyTuple_GET_SIZE(args) == 0)
return GenericCData_new(type, args, kwds);
if (1 <= PyTuple_GET_SIZE(args) && PyTuple_Check(PyTuple_GET_ITEM(args, 0)))
return CFuncPtr_FromDll(type, args, kwds);
#if defined(MS_WIN32)
if (2 <= PyTuple_GET_SIZE(args) && PyInt_Check(PyTuple_GET_ITEM(args, 0)))
return CFuncPtr_FromVtblIndex(type, args, kwds);
#endif
if (1 == PyTuple_GET_SIZE(args)
&& (PyInt_Check(PyTuple_GET_ITEM(args, 0))
|| PyLong_Check(PyTuple_GET_ITEM(args, 0)))) {
CDataObject *ob;
void *ptr = PyLong_AsVoidPtr(PyTuple_GET_ITEM(args, 0));
if (ptr == NULL && PyErr_Occurred())
return NULL;
ob = (CDataObject *)GenericCData_new(type, args, kwds);
if (ob == NULL)
return NULL;
*(void **)ob->b_ptr = ptr;
return (PyObject *)ob;
}
if (!PyArg_ParseTuple(args, "O", &callable))
return NULL;
if (!PyCallable_Check(callable)) {
PyErr_SetString(PyExc_TypeError,
"argument must be callable or integer function address");
return NULL;
}
dict = PyType_stgdict((PyObject *)type);
if (!dict || !dict->argtypes) {
PyErr_SetString(PyExc_TypeError,
"cannot construct instance of this class:"
" no argtypes");
return NULL;
}
thunk = AllocFunctionCallback(callable,
dict->argtypes,
dict->restype,
dict->flags);
if (!thunk)
return NULL;
self = (CFuncPtrObject *)GenericCData_new(type, args, kwds);
if (self == NULL) {
Py_DECREF(thunk);
return NULL;
}
Py_INCREF(callable);
self->callable = callable;
self->thunk = thunk;
*(void **)self->b_ptr = (void *)thunk->pcl;
Py_INCREF((PyObject *)thunk);
if (-1 == KeepRef((CDataObject *)self, 0, (PyObject *)thunk)) {
Py_DECREF((PyObject *)self);
return NULL;
}
return (PyObject *)self;
}
static PyObject *
_byref(PyObject *obj) {
PyCArgObject *parg;
if (!CDataObject_Check(obj)) {
PyErr_SetString(PyExc_TypeError,
"expected CData instance");
return NULL;
}
parg = new_CArgObject();
if (parg == NULL) {
Py_DECREF(obj);
return NULL;
}
parg->tag = 'P';
parg->pffi_type = &ffi_type_pointer;
parg->obj = obj;
parg->value.p = ((CDataObject *)obj)->b_ptr;
return (PyObject *)parg;
}
static PyObject *
_get_arg(int *pindex, char *name, PyObject *defval, PyObject *inargs, PyObject *kwds) {
PyObject *v;
if (*pindex < PyTuple_GET_SIZE(inargs)) {
v = PyTuple_GET_ITEM(inargs, *pindex);
++*pindex;
Py_INCREF(v);
return v;
}
if (kwds && (v = PyDict_GetItemString(kwds, name))) {
++*pindex;
Py_INCREF(v);
return v;
}
if (defval) {
Py_INCREF(defval);
return defval;
}
if (name)
PyErr_Format(PyExc_TypeError,
"required argument '%s' missing", name);
else
PyErr_Format(PyExc_TypeError,
"not enough arguments");
return NULL;
}
static PyObject *
_build_callargs(CFuncPtrObject *self, PyObject *argtypes,
PyObject *inargs, PyObject *kwds,
int *poutmask, int *pinoutmask, unsigned int *pnumretvals) {
PyObject *paramflags = self->paramflags;
PyObject *callargs;
StgDictObject *dict;
Py_ssize_t i, len;
int inargs_index = 0;
Py_ssize_t actual_args;
*poutmask = 0;
*pinoutmask = 0;
*pnumretvals = 0;
if (argtypes == NULL || paramflags == NULL || PyTuple_GET_SIZE(argtypes) == 0) {
#if defined(MS_WIN32)
if (self->index)
return PyTuple_GetSlice(inargs, 1, PyTuple_GET_SIZE(inargs));
#endif
Py_INCREF(inargs);
return inargs;
}
len = PyTuple_GET_SIZE(argtypes);
callargs = PyTuple_New(len);
if (callargs == NULL)
return NULL;
#if defined(MS_WIN32)
if (self->index) {
inargs_index = 1;
}
#endif
for (i = 0; i < len; ++i) {
PyObject *item = PyTuple_GET_ITEM(paramflags, i);
PyObject *ob;
int flag;
char *name = NULL;
PyObject *defval = NULL;
Py_ssize_t tsize = PyTuple_GET_SIZE(item);
flag = PyInt_AS_LONG(PyTuple_GET_ITEM(item, 0));
name = tsize > 1 ? PyString_AS_STRING(PyTuple_GET_ITEM(item, 1)) : NULL;
defval = tsize > 2 ? PyTuple_GET_ITEM(item, 2) : NULL;
switch (flag & (PARAMFLAG_FIN | PARAMFLAG_FOUT | PARAMFLAG_FLCID)) {
case PARAMFLAG_FIN | PARAMFLAG_FLCID:
if (defval == NULL) {
defval = PyInt_FromLong(0);
if (defval == NULL)
goto error;
} else
Py_INCREF(defval);
PyTuple_SET_ITEM(callargs, i, defval);
break;
case (PARAMFLAG_FIN | PARAMFLAG_FOUT):
*pinoutmask |= (1 << i);
(*pnumretvals)++;
case 0:
case PARAMFLAG_FIN:
ob =_get_arg(&inargs_index, name, defval, inargs, kwds);
if (ob == NULL)
goto error;
PyTuple_SET_ITEM(callargs, i, ob);
break;
case PARAMFLAG_FOUT:
if (defval) {
Py_INCREF(defval);
PyTuple_SET_ITEM(callargs, i, defval);
*poutmask |= (1 << i);
(*pnumretvals)++;
break;
}
ob = PyTuple_GET_ITEM(argtypes, i);
dict = PyType_stgdict(ob);
if (dict == NULL) {
PyErr_Format(PyExc_RuntimeError,
"NULL stgdict unexpected");
goto error;
}
if (PyString_Check(dict->proto)) {
PyErr_Format(
PyExc_TypeError,
"%s 'out' parameter must be passed as default value",
((PyTypeObject *)ob)->tp_name);
goto error;
}
if (ArrayTypeObject_Check(ob))
ob = PyObject_CallObject(ob, NULL);
else
ob = PyObject_CallObject(dict->proto, NULL);
if (ob == NULL)
goto error;
PyTuple_SET_ITEM(callargs, i, ob);
*poutmask |= (1 << i);
(*pnumretvals)++;
break;
default:
PyErr_Format(PyExc_ValueError,
"paramflag %d not yet implemented", flag);
goto error;
break;
}
}
actual_args = PyTuple_GET_SIZE(inargs) + (kwds ? PyDict_Size(kwds) : 0);
if (actual_args != inargs_index) {
PyErr_Format(PyExc_TypeError,
#if (PY_VERSION_HEX < 0x02050000)
"call takes exactly %d arguments (%d given)",
#else
"call takes exactly %d arguments (%zd given)",
#endif
inargs_index, actual_args);
goto error;
}
return callargs;
error:
Py_DECREF(callargs);
return NULL;
}
static PyObject *
_build_result(PyObject *result, PyObject *callargs,
int outmask, int inoutmask, unsigned int numretvals) {
unsigned int i, index;
int bit;
PyObject *tup = NULL;
if (callargs == NULL)
return result;
if (result == NULL || numretvals == 0) {
Py_DECREF(callargs);
return result;
}
Py_DECREF(result);
if (numretvals > 1) {
tup = PyTuple_New(numretvals);
if (tup == NULL) {
Py_DECREF(callargs);
return NULL;
}
}
index = 0;
for (bit = 1, i = 0; i < 32; ++i, bit <<= 1) {
PyObject *v;
if (bit & inoutmask) {
v = PyTuple_GET_ITEM(callargs, i);
Py_INCREF(v);
if (numretvals == 1) {
Py_DECREF(callargs);
return v;
}
PyTuple_SET_ITEM(tup, index, v);
index++;
} else if (bit & outmask) {
v = PyTuple_GET_ITEM(callargs, i);
v = PyObject_CallMethod(v, "__ctypes_from_outparam__", NULL);
if (v == NULL || numretvals == 1) {
Py_DECREF(callargs);
return v;
}
PyTuple_SET_ITEM(tup, index, v);
index++;
}
if (index == numretvals)
break;
}
Py_DECREF(callargs);
return tup;
}
static PyObject *
CFuncPtr_call(CFuncPtrObject *self, PyObject *inargs, PyObject *kwds) {
PyObject *restype;
PyObject *converters;
PyObject *checker;
PyObject *argtypes;
StgDictObject *dict = PyObject_stgdict((PyObject *)self);
PyObject *result;
PyObject *callargs;
PyObject *errcheck;
#if defined(MS_WIN32)
IUnknown *piunk = NULL;
#endif
void *pProc = NULL;
int inoutmask;
int outmask;
unsigned int numretvals;
assert(dict);
restype = self->restype ? self->restype : dict->restype;
converters = self->converters ? self->converters : dict->converters;
checker = self->checker ? self->checker : dict->checker;
argtypes = self->argtypes ? self->argtypes : dict->argtypes;
errcheck = self->errcheck ;
pProc = *(void **)self->b_ptr;
#if defined(MS_WIN32)
if (self->index) {
CDataObject *this;
this = (CDataObject *)PyTuple_GetItem(inargs, 0);
if (!this) {
PyErr_SetString(PyExc_ValueError,
"native com method call without 'this' parameter");
return NULL;
}
if (!CDataObject_Check(this)) {
PyErr_SetString(PyExc_TypeError,
"Expected a COM this pointer as first argument");
return NULL;
}
if (!this->b_ptr || *(void **)this->b_ptr == NULL) {
PyErr_SetString(PyExc_ValueError,
"NULL COM pointer access");
return NULL;
}
piunk = *(IUnknown **)this->b_ptr;
if (NULL == piunk->lpVtbl) {
PyErr_SetString(PyExc_ValueError,
"COM method call without VTable");
return NULL;
}
pProc = ((void **)piunk->lpVtbl)[self->index - 0x1000];
}
#endif
callargs = _build_callargs(self, argtypes,
inargs, kwds,
&outmask, &inoutmask, &numretvals);
if (callargs == NULL)
return NULL;
if (converters) {
int required = Py_SAFE_DOWNCAST(PyTuple_GET_SIZE(converters),
Py_ssize_t, int);
int actual = Py_SAFE_DOWNCAST(PyTuple_GET_SIZE(callargs),
Py_ssize_t, int);
if ((dict->flags & FUNCFLAG_CDECL) == FUNCFLAG_CDECL) {
if (required > actual) {
Py_DECREF(callargs);
PyErr_Format(PyExc_TypeError,
"this function takes at least %d argument%s (%d given)",
required,
required == 1 ? "" : "s",
actual);
return NULL;
}
} else if (required != actual) {
Py_DECREF(callargs);
PyErr_Format(PyExc_TypeError,
"this function takes %d argument%s (%d given)",
required,
required == 1 ? "" : "s",
actual);
return NULL;
}
}
result = _CallProc(pProc,
callargs,
#if defined(MS_WIN32)
piunk,
self->iid,
#endif
dict->flags,
converters,
restype,
checker);
if (result != NULL && errcheck) {
PyObject *v = PyObject_CallFunctionObjArgs(errcheck,
result,
self,
callargs,
NULL);
if (v == NULL || v != callargs) {
Py_DECREF(result);
Py_DECREF(callargs);
return v;
}
Py_DECREF(v);
}
return _build_result(result, callargs,
outmask, inoutmask, numretvals);
}
static int
CFuncPtr_traverse(CFuncPtrObject *self, visitproc visit, void *arg) {
Py_VISIT(self->callable);
Py_VISIT(self->restype);
Py_VISIT(self->checker);
Py_VISIT(self->errcheck);
Py_VISIT(self->argtypes);
Py_VISIT(self->converters);
Py_VISIT(self->paramflags);
Py_VISIT(self->thunk);
return CData_traverse((CDataObject *)self, visit, arg);
}
static int
CFuncPtr_clear(CFuncPtrObject *self) {
Py_CLEAR(self->callable);
Py_CLEAR(self->restype);
Py_CLEAR(self->checker);
Py_CLEAR(self->errcheck);
Py_CLEAR(self->argtypes);
Py_CLEAR(self->converters);
Py_CLEAR(self->paramflags);
Py_CLEAR(self->thunk);
return CData_clear((CDataObject *)self);
}
static void
CFuncPtr_dealloc(CFuncPtrObject *self) {
CFuncPtr_clear(self);
Py_TYPE(self)->tp_free((PyObject *)self);
}
static PyObject *
CFuncPtr_repr(CFuncPtrObject *self) {
#if defined(MS_WIN32)
if (self->index)
return PyString_FromFormat("<COM method offset %d: %s at %p>",
self->index - 0x1000,
Py_TYPE(self)->tp_name,
self);
#endif
return PyString_FromFormat("<%s object at %p>",
Py_TYPE(self)->tp_name,
self);
}
static int
CFuncPtr_nonzero(CFuncPtrObject *self) {
return ((*(void **)self->b_ptr != NULL)
#if defined(MS_WIN32)
|| (self->index != 0)
#endif
);
}
static PyNumberMethods CFuncPtr_as_number = {
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
(inquiry)CFuncPtr_nonzero,
};
PyTypeObject CFuncPtr_Type = {
PyVarObject_HEAD_INIT(NULL, 0)
"_ctypes.CFuncPtr",
sizeof(CFuncPtrObject),
0,
(destructor)CFuncPtr_dealloc,
0,
0,
0,
0,
(reprfunc)CFuncPtr_repr,
&CFuncPtr_as_number,
0,
0,
0,
(ternaryfunc)CFuncPtr_call,
0,
0,
0,
&CData_as_buffer,
Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_NEWBUFFER | Py_TPFLAGS_BASETYPE,
"Function Pointer",
(traverseproc)CFuncPtr_traverse,
(inquiry)CFuncPtr_clear,
0,
0,
0,
0,
0,
0,
CFuncPtr_getsets,
0,
0,
0,
0,
0,
0,
0,
CFuncPtr_new,
0,
};
static int
IBUG(char *msg) {
PyErr_Format(PyExc_RuntimeError,
"inconsistent state in CDataObject (%s)", msg);
return -1;
}
static int
Struct_init(PyObject *self, PyObject *args, PyObject *kwds) {
int i;
PyObject *fields;
if (!PyTuple_Check(args)) {
PyErr_SetString(PyExc_TypeError,
"args not a tuple?");
return -1;
}
if (PyTuple_GET_SIZE(args)) {
fields = PyObject_GetAttrString(self, "_fields_");
if (!fields) {
PyErr_Clear();
fields = PyTuple_New(0);
if (!fields)
return -1;
}
if (PyTuple_GET_SIZE(args) > PySequence_Length(fields)) {
Py_DECREF(fields);
PyErr_SetString(PyExc_TypeError,
"too many initializers");
return -1;
}
for (i = 0; i < PyTuple_GET_SIZE(args); ++i) {
PyObject *pair = PySequence_GetItem(fields, i);
PyObject *name;
PyObject *val;
if (!pair) {
Py_DECREF(fields);
return IBUG("_fields_[i] failed");
}
name = PySequence_GetItem(pair, 0);
if (!name) {
Py_DECREF(pair);
Py_DECREF(fields);
return IBUG("_fields_[i][0] failed");
}
if (kwds && PyDict_GetItem(kwds, name)) {
char *field = PyString_AsString(name);
if (field == NULL) {
PyErr_Clear();
field = "???";
}
PyErr_Format(PyExc_TypeError,
"duplicate values for field %s",
field);
Py_DECREF(pair);
Py_DECREF(name);
Py_DECREF(fields);
return -1;
}
val = PyTuple_GET_ITEM(args, i);
if (-1 == PyObject_SetAttr(self, name, val)) {
Py_DECREF(pair);
Py_DECREF(name);
Py_DECREF(fields);
return -1;
}
Py_DECREF(name);
Py_DECREF(pair);
}
Py_DECREF(fields);
}
if (kwds) {
PyObject *key, *value;
Py_ssize_t pos = 0;
while(PyDict_Next(kwds, &pos, &key, &value)) {
if (-1 == PyObject_SetAttr(self, key, value))
return -1;
}
}
return 0;
}
static PyTypeObject Struct_Type = {
PyVarObject_HEAD_INIT(NULL, 0)
"_ctypes.Structure",
sizeof(CDataObject),
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
&CData_as_buffer,
Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_NEWBUFFER | Py_TPFLAGS_BASETYPE,
"Structure base class",
(traverseproc)CData_traverse,
(inquiry)CData_clear,
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
Struct_init,
0,
GenericCData_new,
0,
};
static PyTypeObject Union_Type = {
PyVarObject_HEAD_INIT(NULL, 0)
"_ctypes.Union",
sizeof(CDataObject),
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
&CData_as_buffer,
Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_NEWBUFFER | Py_TPFLAGS_BASETYPE,
"Union base class",
(traverseproc)CData_traverse,
(inquiry)CData_clear,
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
Struct_init,
0,
GenericCData_new,
0,
};
static int
Array_init(CDataObject *self, PyObject *args, PyObject *kw) {
Py_ssize_t i;
Py_ssize_t n;
if (!PyTuple_Check(args)) {
PyErr_SetString(PyExc_TypeError,
"args not a tuple?");
return -1;
}
n = PyTuple_GET_SIZE(args);
for (i = 0; i < n; ++i) {
PyObject *v;
v = PyTuple_GET_ITEM(args, i);
if (-1 == PySequence_SetItem((PyObject *)self, i, v))
return -1;
}
return 0;
}
static PyObject *
Array_item(PyObject *_self, Py_ssize_t index) {
CDataObject *self = (CDataObject *)_self;
Py_ssize_t offset, size;
StgDictObject *stgdict;
if (index < 0 || index >= self->b_length) {
PyErr_SetString(PyExc_IndexError,
"invalid index");
return NULL;
}
stgdict = PyObject_stgdict((PyObject *)self);
assert(stgdict);
size = stgdict->size / stgdict->length;
offset = index * size;
return CData_get(stgdict->proto, stgdict->getfunc, (PyObject *)self,
index, size, self->b_ptr + offset);
}
static PyObject *
Array_slice(PyObject *_self, Py_ssize_t ilow, Py_ssize_t ihigh) {
CDataObject *self = (CDataObject *)_self;
StgDictObject *stgdict, *itemdict;
PyObject *proto;
PyListObject *np;
Py_ssize_t i, len;
if (ilow < 0)
ilow = 0;
else if (ilow > self->b_length)
ilow = self->b_length;
if (ihigh < ilow)
ihigh = ilow;
else if (ihigh > self->b_length)
ihigh = self->b_length;
len = ihigh - ilow;
stgdict = PyObject_stgdict((PyObject *)self);
assert(stgdict);
proto = stgdict->proto;
itemdict = PyType_stgdict(proto);
assert(itemdict);
if (itemdict->getfunc == getentry("c")->getfunc) {
char *ptr = (char *)self->b_ptr;
return PyString_FromStringAndSize(ptr + ilow, len);
#if defined(CTYPES_UNICODE)
} else if (itemdict->getfunc == getentry("u")->getfunc) {
wchar_t *ptr = (wchar_t *)self->b_ptr;
return PyUnicode_FromWideChar(ptr + ilow, len);
#endif
}
np = (PyListObject *) PyList_New(len);
if (np == NULL)
return NULL;
for (i = 0; i < len; i++) {
PyObject *v = Array_item(_self, i+ilow);
PyList_SET_ITEM(np, i, v);
}
return (PyObject *)np;
}
static PyObject *
Array_subscript(PyObject *_self, PyObject *item) {
CDataObject *self = (CDataObject *)_self;
if (PyIndex_Check(item)) {
Py_ssize_t i = PyNumber_AsSsize_t(item, PyExc_IndexError);
if (i == -1 && PyErr_Occurred())
return NULL;
if (i < 0)
i += self->b_length;
return Array_item(_self, i);
} else if PySlice_Check(item) {
StgDictObject *stgdict, *itemdict;
PyObject *proto;
PyObject *np;
Py_ssize_t start, stop, step, slicelen, cur, i;
if (PySlice_GetIndicesEx((PySliceObject *)item,
self->b_length, &start, &stop,
&step, &slicelen) < 0) {
return NULL;
}
stgdict = PyObject_stgdict((PyObject *)self);
assert(stgdict);
proto = stgdict->proto;
itemdict = PyType_stgdict(proto);
assert(itemdict);
if (itemdict->getfunc == getentry("c")->getfunc) {
char *ptr = (char *)self->b_ptr;
char *dest;
if (slicelen <= 0)
return PyString_FromString("");
if (step == 1) {
return PyString_FromStringAndSize(ptr + start,
slicelen);
}
dest = (char *)PyMem_Malloc(slicelen);
if (dest == NULL)
return PyErr_NoMemory();
for (cur = start, i = 0; i < slicelen;
cur += step, i++) {
dest[i] = ptr[cur];
}
np = PyString_FromStringAndSize(dest, slicelen);
PyMem_Free(dest);
return np;
}
#if defined(CTYPES_UNICODE)
if (itemdict->getfunc == getentry("u")->getfunc) {
wchar_t *ptr = (wchar_t *)self->b_ptr;
wchar_t *dest;
if (slicelen <= 0)
return PyUnicode_FromUnicode(NULL, 0);
if (step == 1) {
return PyUnicode_FromWideChar(ptr + start,
slicelen);
}
dest = (wchar_t *)PyMem_Malloc(
slicelen * sizeof(wchar_t));
for (cur = start, i = 0; i < slicelen;
cur += step, i++) {
dest[i] = ptr[cur];
}
np = PyUnicode_FromWideChar(dest, slicelen);
PyMem_Free(dest);
return np;
}
#endif
np = PyList_New(slicelen);
if (np == NULL)
return NULL;
for (cur = start, i = 0; i < slicelen;
cur += step, i++) {
PyObject *v = Array_item(_self, cur);
PyList_SET_ITEM(np, i, v);
}
return np;
} else {
PyErr_SetString(PyExc_TypeError,
"indices must be integers");
return NULL;
}
}
static int
Array_ass_item(PyObject *_self, Py_ssize_t index, PyObject *value) {
CDataObject *self = (CDataObject *)_self;
Py_ssize_t size, offset;
StgDictObject *stgdict;
char *ptr;
if (value == NULL) {
PyErr_SetString(PyExc_TypeError,
"Array does not support item deletion");
return -1;
}
stgdict = PyObject_stgdict((PyObject *)self);
assert(stgdict);
if (index < 0 || index >= stgdict->length) {
PyErr_SetString(PyExc_IndexError,
"invalid index");
return -1;
}
size = stgdict->size / stgdict->length;
offset = index * size;
ptr = self->b_ptr + offset;
return CData_set((PyObject *)self, stgdict->proto, stgdict->setfunc, value,
index, size, ptr);
}
static int
Array_ass_slice(PyObject *_self, Py_ssize_t ilow, Py_ssize_t ihigh, PyObject *value) {
CDataObject *self = (CDataObject *)_self;
Py_ssize_t i, len;
if (value == NULL) {
PyErr_SetString(PyExc_TypeError,
"Array does not support item deletion");
return -1;
}
if (ilow < 0)
ilow = 0;
else if (ilow > self->b_length)
ilow = self->b_length;
if (ihigh < 0)
ihigh = 0;
if (ihigh < ilow)
ihigh = ilow;
else if (ihigh > self->b_length)
ihigh = self->b_length;
len = PySequence_Length(value);
if (len != ihigh - ilow) {
PyErr_SetString(PyExc_ValueError,
"Can only assign sequence of same size");
return -1;
}
for (i = 0; i < len; i++) {
PyObject *item = PySequence_GetItem(value, i);
int result;
if (item == NULL)
return -1;
result = Array_ass_item(_self, i+ilow, item);
Py_DECREF(item);
if (result == -1)
return -1;
}
return 0;
}
static int
Array_ass_subscript(PyObject *_self, PyObject *item, PyObject *value) {
CDataObject *self = (CDataObject *)_self;
if (value == NULL) {
PyErr_SetString(PyExc_TypeError,
"Array does not support item deletion");
return -1;
}
if (PyIndex_Check(item)) {
Py_ssize_t i = PyNumber_AsSsize_t(item, PyExc_IndexError);
if (i == -1 && PyErr_Occurred())
return -1;
if (i < 0)
i += self->b_length;
return Array_ass_item(_self, i, value);
} else if (PySlice_Check(item)) {
Py_ssize_t start, stop, step, slicelen, otherlen, i, cur;
if (PySlice_GetIndicesEx((PySliceObject *)item,
self->b_length, &start, &stop,
&step, &slicelen) < 0) {
return -1;
}
if ((step < 0 && start < stop) ||
(step > 0 && start > stop))
stop = start;
otherlen = PySequence_Length(value);
if (otherlen != slicelen) {
PyErr_SetString(PyExc_ValueError,
"Can only assign sequence of same size");
return -1;
}
for (cur = start, i = 0; i < otherlen; cur += step, i++) {
PyObject *item = PySequence_GetItem(value, i);
int result;
if (item == NULL)
return -1;
result = Array_ass_item(_self, cur, item);
Py_DECREF(item);
if (result == -1)
return -1;
}
return 0;
} else {
PyErr_SetString(PyExc_TypeError,
"indices must be integer");
return -1;
}
}
static Py_ssize_t
Array_length(PyObject *_self) {
CDataObject *self = (CDataObject *)_self;
return self->b_length;
}
static PySequenceMethods Array_as_sequence = {
Array_length,
0,
0,
Array_item,
Array_slice,
Array_ass_item,
Array_ass_slice,
0,
0,
0,
};
static PyMappingMethods Array_as_mapping = {
Array_length,
Array_subscript,
Array_ass_subscript,
};
PyTypeObject Array_Type = {
PyVarObject_HEAD_INIT(NULL, 0)
"_ctypes.Array",
sizeof(CDataObject),
0,
0,
0,
0,
0,
0,
0,
0,
&Array_as_sequence,
&Array_as_mapping,
0,
0,
0,
0,
0,
&CData_as_buffer,
Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_NEWBUFFER | Py_TPFLAGS_BASETYPE,
"XXX to be provided",
(traverseproc)CData_traverse,
(inquiry)CData_clear,
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
(initproc)Array_init,
0,
GenericCData_new,
0,
};
PyObject *
CreateArrayType(PyObject *itemtype, Py_ssize_t length) {
static PyObject *cache;
PyObject *key;
PyObject *result;
char name[256];
PyObject *len;
if (cache == NULL) {
cache = PyDict_New();
if (cache == NULL)
return NULL;
}
len = PyInt_FromSsize_t(length);
if (len == NULL)
return NULL;
key = PyTuple_Pack(2, itemtype, len);
Py_DECREF(len);
if (!key)
return NULL;
result = PyDict_GetItemProxy(cache, key);
if (result) {
Py_INCREF(result);
Py_DECREF(key);
return result;
}
if (!PyType_Check(itemtype)) {
PyErr_SetString(PyExc_TypeError,
"Expected a type object");
return NULL;
}
#if defined(MS_WIN64)
sprintf(name, "%.200s_Array_%Id",
((PyTypeObject *)itemtype)->tp_name, length);
#else
sprintf(name, "%.200s_Array_%ld",
((PyTypeObject *)itemtype)->tp_name, (long)length);
#endif
result = PyObject_CallFunction((PyObject *)&ArrayType_Type,
#if (PY_VERSION_HEX < 0x02050000)
"s(O){s:i,s:O}",
#else
"s(O){s:n,s:O}",
#endif
name,
&Array_Type,
"_length_",
length,
"_type_",
itemtype
);
if (result == NULL) {
Py_DECREF(key);
return NULL;
}
if (-1 == PyDict_SetItemProxy(cache, key, result)) {
Py_DECREF(key);
Py_DECREF(result);
return NULL;
}
Py_DECREF(key);
return result;
}
static int
Simple_set_value(CDataObject *self, PyObject *value) {
PyObject *result;
StgDictObject *dict = PyObject_stgdict((PyObject *)self);
if (value == NULL) {
PyErr_SetString(PyExc_TypeError,
"can't delete attribute");
return -1;
}
assert(dict);
assert(dict->setfunc);
result = dict->setfunc(self->b_ptr, value, dict->size);
if (!result)
return -1;
return KeepRef(self, 0, result);
}
static int
Simple_init(CDataObject *self, PyObject *args, PyObject *kw) {
PyObject *value = NULL;
if (!PyArg_UnpackTuple(args, "__init__", 0, 1, &value))
return -1;
if (value)
return Simple_set_value(self, value);
return 0;
}
static PyObject *
Simple_get_value(CDataObject *self) {
StgDictObject *dict;
dict = PyObject_stgdict((PyObject *)self);
assert(dict);
assert(dict->getfunc);
return dict->getfunc(self->b_ptr, self->b_size);
}
static PyGetSetDef Simple_getsets[] = {
{
"value", (getter)Simple_get_value, (setter)Simple_set_value,
"current value", NULL
},
{ NULL, NULL }
};
static PyObject *
Simple_from_outparm(PyObject *self, PyObject *args) {
if (IsSimpleSubType((PyObject *)Py_TYPE(self))) {
Py_INCREF(self);
return self;
}
return Simple_get_value((CDataObject *)self);
}
static PyMethodDef Simple_methods[] = {
{ "__ctypes_from_outparam__", Simple_from_outparm, METH_NOARGS, },
{ NULL, NULL },
};
static int Simple_nonzero(CDataObject *self) {
return memcmp(self->b_ptr, "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", self->b_size);
}
static PyNumberMethods Simple_as_number = {
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
(inquiry)Simple_nonzero,
};
static PyObject *
Simple_repr(CDataObject *self) {
PyObject *val, *name, *args, *result;
static PyObject *format;
if (Py_TYPE(self)->tp_base != &Simple_Type) {
return PyString_FromFormat("<%s object at %p>",
Py_TYPE(self)->tp_name, self);
}
if (format == NULL) {
format = PyString_InternFromString("%s(%r)");
if (format == NULL)
return NULL;
}
val = Simple_get_value(self);
if (val == NULL)
return NULL;
name = PyString_FromString(Py_TYPE(self)->tp_name);
if (name == NULL) {
Py_DECREF(val);
return NULL;
}
args = PyTuple_Pack(2, name, val);
Py_DECREF(name);
Py_DECREF(val);
if (args == NULL)
return NULL;
result = PyString_Format(format, args);
Py_DECREF(args);
return result;
}
static PyTypeObject Simple_Type = {
PyVarObject_HEAD_INIT(NULL, 0)
"_ctypes._SimpleCData",
sizeof(CDataObject),
0,
0,
0,
0,
0,
0,
(reprfunc)&Simple_repr,
&Simple_as_number,
0,
0,
0,
0,
0,
0,
0,
&CData_as_buffer,
Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_NEWBUFFER | Py_TPFLAGS_BASETYPE,
"XXX to be provided",
(traverseproc)CData_traverse,
(inquiry)CData_clear,
0,
0,
0,
0,
Simple_methods,
0,
Simple_getsets,
0,
0,
0,
0,
0,
(initproc)Simple_init,
0,
GenericCData_new,
0,
};
static PyObject *
Pointer_item(PyObject *_self, Py_ssize_t index) {
CDataObject *self = (CDataObject *)_self;
Py_ssize_t size;
Py_ssize_t offset;
StgDictObject *stgdict, *itemdict;
PyObject *proto;
if (*(void **)self->b_ptr == NULL) {
PyErr_SetString(PyExc_ValueError,
"NULL pointer access");
return NULL;
}
stgdict = PyObject_stgdict((PyObject *)self);
assert(stgdict);
proto = stgdict->proto;
assert(proto);
itemdict = PyType_stgdict(proto);
assert(itemdict);
size = itemdict->size;
offset = index * itemdict->size;
return CData_get(proto, stgdict->getfunc, (PyObject *)self,
index, size, (*(char **)self->b_ptr) + offset);
}
static int
Pointer_ass_item(PyObject *_self, Py_ssize_t index, PyObject *value) {
CDataObject *self = (CDataObject *)_self;
Py_ssize_t size;
Py_ssize_t offset;
StgDictObject *stgdict, *itemdict;
PyObject *proto;
if (value == NULL) {
PyErr_SetString(PyExc_TypeError,
"Pointer does not support item deletion");
return -1;
}
if (*(void **)self->b_ptr == NULL) {
PyErr_SetString(PyExc_ValueError,
"NULL pointer access");
return -1;
}
stgdict = PyObject_stgdict((PyObject *)self);
assert(stgdict);
proto = stgdict->proto;
assert(proto);
itemdict = PyType_stgdict(proto);
assert(itemdict);
size = itemdict->size;
offset = index * itemdict->size;
return CData_set((PyObject *)self, proto, stgdict->setfunc, value,
index, size, (*(char **)self->b_ptr) + offset);
}
static PyObject *
Pointer_get_contents(CDataObject *self, void *closure) {
StgDictObject *stgdict;
if (*(void **)self->b_ptr == NULL) {
PyErr_SetString(PyExc_ValueError,
"NULL pointer access");
return NULL;
}
stgdict = PyObject_stgdict((PyObject *)self);
assert(stgdict);
return CData_FromBaseObj(stgdict->proto,
(PyObject *)self, 0,
*(void **)self->b_ptr);
}
static int
Pointer_set_contents(CDataObject *self, PyObject *value, void *closure) {
StgDictObject *stgdict;
CDataObject *dst;
PyObject *keep;
if (value == NULL) {
PyErr_SetString(PyExc_TypeError,
"Pointer does not support item deletion");
return -1;
}
stgdict = PyObject_stgdict((PyObject *)self);
assert(stgdict);
assert(stgdict->proto);
if (!CDataObject_Check(value)
|| 0 == PyObject_IsInstance(value, stgdict->proto)) {
PyErr_Format(PyExc_TypeError,
"expected %s instead of %s",
((PyTypeObject *)(stgdict->proto))->tp_name,
Py_TYPE(value)->tp_name);
return -1;
}
dst = (CDataObject *)value;
*(void **)self->b_ptr = dst->b_ptr;
Py_INCREF(value);
if (-1 == KeepRef(self, 1, value))
return -1;
keep = GetKeepedObjects(dst);
Py_INCREF(keep);
return KeepRef(self, 0, keep);
}
static PyGetSetDef Pointer_getsets[] = {
{
"contents", (getter)Pointer_get_contents,
(setter)Pointer_set_contents,
"the object this pointer points to (read-write)", NULL
},
{ NULL, NULL }
};
static int
Pointer_init(CDataObject *self, PyObject *args, PyObject *kw) {
PyObject *value = NULL;
if (!PyArg_UnpackTuple(args, "POINTER", 0, 1, &value))
return -1;
if (value == NULL)
return 0;
return Pointer_set_contents(self, value, NULL);
}
static PyObject *
Pointer_new(PyTypeObject *type, PyObject *args, PyObject *kw) {
StgDictObject *dict = PyType_stgdict((PyObject *)type);
if (!dict || !dict->proto) {
PyErr_SetString(PyExc_TypeError,
"Cannot create instance: has no _type_");
return NULL;
}
return GenericCData_new(type, args, kw);
}
static PyObject *
Pointer_slice(PyObject *_self, Py_ssize_t ilow, Py_ssize_t ihigh) {
CDataObject *self = (CDataObject *)_self;
PyListObject *np;
StgDictObject *stgdict, *itemdict;
PyObject *proto;
Py_ssize_t i, len;
if (ilow < 0)
ilow = 0;
if (ihigh < ilow)
ihigh = ilow;
len = ihigh - ilow;
stgdict = PyObject_stgdict((PyObject *)self);
assert(stgdict);
proto = stgdict->proto;
assert(proto);
itemdict = PyType_stgdict(proto);
assert(itemdict);
if (itemdict->getfunc == getentry("c")->getfunc) {
char *ptr = *(char **)self->b_ptr;
return PyString_FromStringAndSize(ptr + ilow, len);
#if defined(CTYPES_UNICODE)
} else if (itemdict->getfunc == getentry("u")->getfunc) {
wchar_t *ptr = *(wchar_t **)self->b_ptr;
return PyUnicode_FromWideChar(ptr + ilow, len);
#endif
}
np = (PyListObject *) PyList_New(len);
if (np == NULL)
return NULL;
for (i = 0; i < len; i++) {
PyObject *v = Pointer_item(_self, i+ilow);
PyList_SET_ITEM(np, i, v);
}
return (PyObject *)np;
}
static PyObject *
Pointer_subscript(PyObject *_self, PyObject *item) {
CDataObject *self = (CDataObject *)_self;
if (PyIndex_Check(item)) {
Py_ssize_t i = PyNumber_AsSsize_t(item, PyExc_IndexError);
if (i == -1 && PyErr_Occurred())
return NULL;
return Pointer_item(_self, i);
} else if (PySlice_Check(item)) {
PySliceObject *slice = (PySliceObject *)item;
Py_ssize_t start, stop, step;
PyObject *np;
StgDictObject *stgdict, *itemdict;
PyObject *proto;
Py_ssize_t i, len, cur;
if (slice->step == Py_None) {
step = 1;
} else {
step = PyNumber_AsSsize_t(slice->step,
PyExc_ValueError);
if (step == -1 && PyErr_Occurred())
return NULL;
if (step == 0) {
PyErr_SetString(PyExc_ValueError,
"slice step cannot be zero");
return NULL;
}
}
if (slice->start == Py_None) {
if (step < 0) {
PyErr_SetString(PyExc_ValueError,
"slice start is required "
"for step < 0");
return NULL;
}
start = 0;
} else {
start = PyNumber_AsSsize_t(slice->start,
PyExc_ValueError);
if (start == -1 && PyErr_Occurred())
return NULL;
}
if (slice->stop == Py_None) {
PyErr_SetString(PyExc_ValueError,
"slice stop is required");
return NULL;
}
stop = PyNumber_AsSsize_t(slice->stop,
PyExc_ValueError);
if (stop == -1 && PyErr_Occurred())
return NULL;
if ((step > 0 && start > stop) ||
(step < 0 && start < stop))
len = 0;
else if (step > 0)
len = (stop - start - 1) / step + 1;
else
len = (stop - start + 1) / step + 1;
stgdict = PyObject_stgdict((PyObject *)self);
assert(stgdict);
proto = stgdict->proto;
assert(proto);
itemdict = PyType_stgdict(proto);
assert(itemdict);
if (itemdict->getfunc == getentry("c")->getfunc) {
char *ptr = *(char **)self->b_ptr;
char *dest;
if (len <= 0)
return PyString_FromString("");
if (step == 1) {
return PyString_FromStringAndSize(ptr + start,
len);
}
dest = (char *)PyMem_Malloc(len);
if (dest == NULL)
return PyErr_NoMemory();
for (cur = start, i = 0; i < len; cur += step, i++) {
dest[i] = ptr[cur];
}
np = PyString_FromStringAndSize(dest, len);
PyMem_Free(dest);
return np;
}
#if defined(CTYPES_UNICODE)
if (itemdict->getfunc == getentry("u")->getfunc) {
wchar_t *ptr = *(wchar_t **)self->b_ptr;
wchar_t *dest;
if (len <= 0)
return PyUnicode_FromUnicode(NULL, 0);
if (step == 1) {
return PyUnicode_FromWideChar(ptr + start,
len);
}
dest = (wchar_t *)PyMem_Malloc(len * sizeof(wchar_t));
if (dest == NULL)
return PyErr_NoMemory();
for (cur = start, i = 0; i < len; cur += step, i++) {
dest[i] = ptr[cur];
}
np = PyUnicode_FromWideChar(dest, len);
PyMem_Free(dest);
return np;
}
#endif
np = PyList_New(len);
if (np == NULL)
return NULL;
for (cur = start, i = 0; i < len; cur += step, i++) {
PyObject *v = Pointer_item(_self, cur);
PyList_SET_ITEM(np, i, v);
}
return np;
} else {
PyErr_SetString(PyExc_TypeError,
"Pointer indices must be integer");
return NULL;
}
}
static PySequenceMethods Pointer_as_sequence = {
0,
0,
0,
Pointer_item,
Pointer_slice,
Pointer_ass_item,
0,
0,
0,
0,
};
static PyMappingMethods Pointer_as_mapping = {
0,
Pointer_subscript,
};
static int
Pointer_nonzero(CDataObject *self) {
return (*(void **)self->b_ptr != NULL);
}
static PyNumberMethods Pointer_as_number = {
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
(inquiry)Pointer_nonzero,
};
PyTypeObject Pointer_Type = {
PyVarObject_HEAD_INIT(NULL, 0)
"_ctypes._Pointer",
sizeof(CDataObject),
0,
0,
0,
0,
0,
0,
0,
&Pointer_as_number,
&Pointer_as_sequence,
&Pointer_as_mapping,
0,
0,
0,
0,
0,
&CData_as_buffer,
Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_NEWBUFFER | Py_TPFLAGS_BASETYPE,
"XXX to be provided",
(traverseproc)CData_traverse,
(inquiry)CData_clear,
0,
0,
0,
0,
0,
0,
Pointer_getsets,
0,
0,
0,
0,
0,
(initproc)Pointer_init,
0,
Pointer_new,
0,
};
static char *module_docs =
"Create and manipulate C compatible data types in Python.";
#if defined(MS_WIN32)
static char comerror_doc[] = "Raised when a COM method call failed.";
static PyObject *
comerror_init(PyObject *self, PyObject *args) {
PyObject *hresult, *text, *details;
PyObject *a;
int status;
if (!PyArg_ParseTuple(args, "OOOO:COMError", &self, &hresult, &text, &details))
return NULL;
a = PySequence_GetSlice(args, 1, PySequence_Size(args));
if (!a)
return NULL;
status = PyObject_SetAttrString(self, "args", a);
Py_DECREF(a);
if (status < 0)
return NULL;
if (PyObject_SetAttrString(self, "hresult", hresult) < 0)
return NULL;
if (PyObject_SetAttrString(self, "text", text) < 0)
return NULL;
if (PyObject_SetAttrString(self, "details", details) < 0)
return NULL;
Py_INCREF(Py_None);
return Py_None;
}
static PyMethodDef comerror_methods[] = {
{ "__init__", comerror_init, METH_VARARGS },
{ NULL, NULL },
};
static int
create_comerror(void) {
PyObject *dict = PyDict_New();
PyMethodDef *methods = comerror_methods;
PyObject *s;
int status;
if (dict == NULL)
return -1;
while (methods->ml_name) {
PyObject *func = PyCFunction_New(methods, NULL);
PyObject *meth;
if (func == NULL)
goto error;
meth = PyMethod_New(func, NULL, ComError);
Py_DECREF(func);
if (meth == NULL)
goto error;
PyDict_SetItemString(dict, methods->ml_name, meth);
Py_DECREF(meth);
++methods;
}
s = PyString_FromString(comerror_doc);
if (s == NULL)
goto error;
status = PyDict_SetItemString(dict, "__doc__", s);
Py_DECREF(s);
if (status == -1)
goto error;
ComError = PyErr_NewException("_ctypes.COMError",
NULL,
dict);
if (ComError == NULL)
goto error;
return 0;
error:
Py_DECREF(dict);
return -1;
}
#endif
static PyObject *
string_at(const char *ptr, int size) {
if (size == -1)
return PyString_FromString(ptr);
return PyString_FromStringAndSize(ptr, size);
}
static int
cast_check_pointertype(PyObject *arg) {
StgDictObject *dict;
if (PointerTypeObject_Check(arg))
return 1;
if (CFuncPtrTypeObject_Check(arg))
return 1;
dict = PyType_stgdict(arg);
if (dict) {
if (PyString_Check(dict->proto)
&& (strchr("sPzUZXO", PyString_AS_STRING(dict->proto)[0]))) {
return 1;
}
}
PyErr_Format(PyExc_TypeError,
"cast() argument 2 must be a pointer type, not %s",
PyType_Check(arg)
? ((PyTypeObject *)arg)->tp_name
: Py_TYPE(arg)->tp_name);
return 0;
}
static PyObject *
cast(void *ptr, PyObject *src, PyObject *ctype) {
CDataObject *result;
if (0 == cast_check_pointertype(ctype))
return NULL;
result = (CDataObject *)PyObject_CallFunctionObjArgs(ctype, NULL);
if (result == NULL)
return NULL;
if (CDataObject_Check(src)) {
CDataObject *obj = (CDataObject *)src;
CData_GetContainer(obj);
if (obj->b_objects == Py_None) {
Py_DECREF(Py_None);
obj->b_objects = PyDict_New();
if (obj->b_objects == NULL)
goto failed;
}
Py_XINCREF(obj->b_objects);
result->b_objects = obj->b_objects;
if (result->b_objects && PyDict_CheckExact(result->b_objects)) {
PyObject *index;
int rc;
index = PyLong_FromVoidPtr((void *)src);
if (index == NULL)
goto failed;
rc = PyDict_SetItem(result->b_objects, index, src);
Py_DECREF(index);
if (rc == -1)
goto failed;
}
}
memcpy(result->b_ptr, &ptr, sizeof(void *));
return (PyObject *)result;
failed:
Py_DECREF(result);
return NULL;
}
#if defined(CTYPES_UNICODE)
static PyObject *
wstring_at(const wchar_t *ptr, int size) {
Py_ssize_t ssize = size;
if (ssize == -1)
ssize = wcslen(ptr);
return PyUnicode_FromWideChar(ptr, ssize);
}
#endif
PyMODINIT_FUNC
init_ctypes(void) {
PyObject *m;
#if defined(WITH_THREAD)
PyEval_InitThreads();
#endif
m = Py_InitModule3("_ctypes", module_methods, module_docs);
if (!m)
return;
_pointer_type_cache = PyDict_New();
if (_pointer_type_cache == NULL)
return;
PyModule_AddObject(m, "_pointer_type_cache", (PyObject *)_pointer_type_cache);
_unpickle = PyObject_GetAttrString(m, "_unpickle");
if (_unpickle == NULL)
return;
if (PyType_Ready(&PyCArg_Type) < 0)
return;
if (PyType_Ready(&CThunk_Type) < 0)
return;
StgDict_Type.tp_base = &PyDict_Type;
if (PyType_Ready(&StgDict_Type) < 0)
return;
StructType_Type.tp_base = &PyType_Type;
if (PyType_Ready(&StructType_Type) < 0)
return;
UnionType_Type.tp_base = &PyType_Type;
if (PyType_Ready(&UnionType_Type) < 0)
return;
PointerType_Type.tp_base = &PyType_Type;
if (PyType_Ready(&PointerType_Type) < 0)
return;
ArrayType_Type.tp_base = &PyType_Type;
if (PyType_Ready(&ArrayType_Type) < 0)
return;
SimpleType_Type.tp_base = &PyType_Type;
if (PyType_Ready(&SimpleType_Type) < 0)
return;
CFuncPtrType_Type.tp_base = &PyType_Type;
if (PyType_Ready(&CFuncPtrType_Type) < 0)
return;
if (PyType_Ready(&CData_Type) < 0)
return;
Py_TYPE(&Struct_Type) = &StructType_Type;
Struct_Type.tp_base = &CData_Type;
if (PyType_Ready(&Struct_Type) < 0)
return;
PyModule_AddObject(m, "Structure", (PyObject *)&Struct_Type);
Py_TYPE(&Union_Type) = &UnionType_Type;
Union_Type.tp_base = &CData_Type;
if (PyType_Ready(&Union_Type) < 0)
return;
PyModule_AddObject(m, "Union", (PyObject *)&Union_Type);
Py_TYPE(&Pointer_Type) = &PointerType_Type;
Pointer_Type.tp_base = &CData_Type;
if (PyType_Ready(&Pointer_Type) < 0)
return;
PyModule_AddObject(m, "_Pointer", (PyObject *)&Pointer_Type);
Py_TYPE(&Array_Type) = &ArrayType_Type;
Array_Type.tp_base = &CData_Type;
if (PyType_Ready(&Array_Type) < 0)
return;
PyModule_AddObject(m, "Array", (PyObject *)&Array_Type);
Py_TYPE(&Simple_Type) = &SimpleType_Type;
Simple_Type.tp_base = &CData_Type;
if (PyType_Ready(&Simple_Type) < 0)
return;
PyModule_AddObject(m, "_SimpleCData", (PyObject *)&Simple_Type);
Py_TYPE(&CFuncPtr_Type) = &CFuncPtrType_Type;
CFuncPtr_Type.tp_base = &CData_Type;
if (PyType_Ready(&CFuncPtr_Type) < 0)
return;
PyModule_AddObject(m, "CFuncPtr", (PyObject *)&CFuncPtr_Type);
if (PyType_Ready(&CField_Type) < 0)
return;
DictRemover_Type.tp_new = PyType_GenericNew;
if (PyType_Ready(&DictRemover_Type) < 0)
return;
#if defined(MS_WIN32)
if (create_comerror() < 0)
return;
PyModule_AddObject(m, "COMError", ComError);
PyModule_AddObject(m, "FUNCFLAG_HRESULT", PyInt_FromLong(FUNCFLAG_HRESULT));
PyModule_AddObject(m, "FUNCFLAG_STDCALL", PyInt_FromLong(FUNCFLAG_STDCALL));
#endif
PyModule_AddObject(m, "FUNCFLAG_CDECL", PyInt_FromLong(FUNCFLAG_CDECL));
PyModule_AddObject(m, "FUNCFLAG_USE_ERRNO", PyInt_FromLong(FUNCFLAG_USE_ERRNO));
PyModule_AddObject(m, "FUNCFLAG_USE_LASTERROR", PyInt_FromLong(FUNCFLAG_USE_LASTERROR));
PyModule_AddObject(m, "FUNCFLAG_PYTHONAPI", PyInt_FromLong(FUNCFLAG_PYTHONAPI));
PyModule_AddStringConstant(m, "__version__", "1.1.0");
PyModule_AddObject(m, "_memmove_addr", PyLong_FromVoidPtr(memmove));
PyModule_AddObject(m, "_memset_addr", PyLong_FromVoidPtr(memset));
PyModule_AddObject(m, "_string_at_addr", PyLong_FromVoidPtr(string_at));
PyModule_AddObject(m, "_cast_addr", PyLong_FromVoidPtr(cast));
#if defined(CTYPES_UNICODE)
PyModule_AddObject(m, "_wstring_at_addr", PyLong_FromVoidPtr(wstring_at));
#endif
#if !defined(RTLD_LOCAL)
#define RTLD_LOCAL 0
#endif
#if !defined(RTLD_GLOBAL)
#define RTLD_GLOBAL RTLD_LOCAL
#endif
PyModule_AddObject(m, "RTLD_LOCAL", PyInt_FromLong(RTLD_LOCAL));
PyModule_AddObject(m, "RTLD_GLOBAL", PyInt_FromLong(RTLD_GLOBAL));
PyExc_ArgError = PyErr_NewException("ctypes.ArgumentError", NULL, NULL);
if (PyExc_ArgError) {
Py_INCREF(PyExc_ArgError);
PyModule_AddObject(m, "ArgumentError", PyExc_ArgError);
}
init_callbacks_in_module(m);
}
#if defined(HAVE_WCHAR_H)
PyObject *My_PyUnicode_FromWideChar(register const wchar_t *w,
Py_ssize_t size) {
PyUnicodeObject *unicode;
if (w == NULL) {
PyErr_BadInternalCall();
return NULL;
}
unicode = (PyUnicodeObject *)PyUnicode_FromUnicode(NULL, size);
if (!unicode)
return NULL;
#if defined(HAVE_USABLE_WCHAR_T)
memcpy(unicode->str, w, size * sizeof(wchar_t));
#else
{
register Py_UNICODE *u;
register int i;
u = PyUnicode_AS_UNICODE(unicode);
for (i = size; i > 0; i--)
*u++ = *w++;
}
#endif
return (PyObject *)unicode;
}
Py_ssize_t My_PyUnicode_AsWideChar(PyUnicodeObject *unicode,
register wchar_t *w,
Py_ssize_t size) {
if (unicode == NULL) {
PyErr_BadInternalCall();
return -1;
}
if (size > PyUnicode_GET_SIZE(unicode))
size = PyUnicode_GET_SIZE(unicode);
#if defined(HAVE_USABLE_WCHAR_T)
memcpy(w, unicode->str, size * sizeof(wchar_t));
#else
{
register Py_UNICODE *u;
register int i;
u = PyUnicode_AS_UNICODE(unicode);
for (i = size; i > 0; i--)
*w++ = *u++;
}
#endif
return size;
}
#endif