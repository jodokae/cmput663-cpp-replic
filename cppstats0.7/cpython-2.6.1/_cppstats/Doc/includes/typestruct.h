typedef struct _typeobject {
PyObject_VAR_HEAD
char *tp_name;
int tp_basicsize, tp_itemsize;
destructor tp_dealloc;
printfunc tp_print;
getattrfunc tp_getattr;
setattrfunc tp_setattr;
cmpfunc tp_compare;
reprfunc tp_repr;
PyNumberMethods *tp_as_number;
PySequenceMethods *tp_as_sequence;
PyMappingMethods *tp_as_mapping;
hashfunc tp_hash;
ternaryfunc tp_call;
reprfunc tp_str;
getattrofunc tp_getattro;
setattrofunc tp_setattro;
PyBufferProcs *tp_as_buffer;
long tp_flags;
char *tp_doc;
traverseproc tp_traverse;
inquiry tp_clear;
richcmpfunc tp_richcompare;
long tp_weaklistoffset;
getiterfunc tp_iter;
iternextfunc tp_iternext;
struct PyMethodDef *tp_methods;
struct PyMemberDef *tp_members;
struct PyGetSetDef *tp_getset;
struct _typeobject *tp_base;
PyObject *tp_dict;
descrgetfunc tp_descr_get;
descrsetfunc tp_descr_set;
long tp_dictoffset;
initproc tp_init;
allocfunc tp_alloc;
newfunc tp_new;
freefunc tp_free;
inquiry tp_is_gc;
PyObject *tp_bases;
PyObject *tp_mro;
PyObject *tp_cache;
PyObject *tp_subclasses;
PyObject *tp_weaklist;
} PyTypeObject;