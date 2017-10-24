#include "Python.h"
#if !defined(__LP64__)
#include <Carbon/Carbon.h>
#include "pymactoolbox.h"
#if defined(USE_TOOLBOX_OBJECT_GLUE)
extern int _CFStringRefObj_Convert(PyObject *, CFStringRef *);
#endif
static PyObject *IBCarbon_Error;
PyTypeObject IBNibRef_Type;
#define IBNibRefObj_Check(x) ((x)->ob_type == &IBNibRef_Type || PyObject_TypeCheck((x), &IBNibRef_Type))
typedef struct IBNibRefObject {
PyObject_HEAD
IBNibRef ob_itself;
} IBNibRefObject;
PyObject *IBNibRefObj_New(IBNibRef itself) {
IBNibRefObject *it;
it = PyObject_NEW(IBNibRefObject, &IBNibRef_Type);
if (it == NULL) return NULL;
it->ob_itself = itself;
return (PyObject *)it;
}
int IBNibRefObj_Convert(PyObject *v, IBNibRef *p_itself) {
if (!IBNibRefObj_Check(v)) {
PyErr_SetString(PyExc_TypeError, "IBNibRef required");
return 0;
}
*p_itself = ((IBNibRefObject *)v)->ob_itself;
return 1;
}
static void IBNibRefObj_dealloc(IBNibRefObject *self) {
DisposeNibReference(self->ob_itself);
self->ob_type->tp_free((PyObject *)self);
}
static PyObject *IBNibRefObj_CreateWindowFromNib(IBNibRefObject *_self, PyObject *_args) {
PyObject *_res = NULL;
OSStatus _err;
CFStringRef inName;
WindowPtr outWindow;
if (!PyArg_ParseTuple(_args, "O&",
CFStringRefObj_Convert, &inName))
return NULL;
_err = CreateWindowFromNib(_self->ob_itself,
inName,
&outWindow);
if (_err != noErr) return PyMac_Error(_err);
_res = Py_BuildValue("O&",
WinObj_New, outWindow);
return _res;
}
static PyObject *IBNibRefObj_CreateMenuFromNib(IBNibRefObject *_self, PyObject *_args) {
PyObject *_res = NULL;
OSStatus _err;
CFStringRef inName;
MenuHandle outMenuRef;
if (!PyArg_ParseTuple(_args, "O&",
CFStringRefObj_Convert, &inName))
return NULL;
_err = CreateMenuFromNib(_self->ob_itself,
inName,
&outMenuRef);
if (_err != noErr) return PyMac_Error(_err);
_res = Py_BuildValue("O&",
MenuObj_New, outMenuRef);
return _res;
}
static PyObject *IBNibRefObj_CreateMenuBarFromNib(IBNibRefObject *_self, PyObject *_args) {
PyObject *_res = NULL;
OSStatus _err;
CFStringRef inName;
Handle outMenuBar;
if (!PyArg_ParseTuple(_args, "O&",
CFStringRefObj_Convert, &inName))
return NULL;
_err = CreateMenuBarFromNib(_self->ob_itself,
inName,
&outMenuBar);
if (_err != noErr) return PyMac_Error(_err);
_res = Py_BuildValue("O&",
ResObj_New, outMenuBar);
return _res;
}
static PyObject *IBNibRefObj_SetMenuBarFromNib(IBNibRefObject *_self, PyObject *_args) {
PyObject *_res = NULL;
OSStatus _err;
CFStringRef inName;
if (!PyArg_ParseTuple(_args, "O&",
CFStringRefObj_Convert, &inName))
return NULL;
_err = SetMenuBarFromNib(_self->ob_itself,
inName);
if (_err != noErr) return PyMac_Error(_err);
Py_INCREF(Py_None);
_res = Py_None;
return _res;
}
static PyMethodDef IBNibRefObj_methods[] = {
{
"CreateWindowFromNib", (PyCFunction)IBNibRefObj_CreateWindowFromNib, 1,
PyDoc_STR("(CFStringRef inName) -> (WindowPtr outWindow)")
},
{
"CreateMenuFromNib", (PyCFunction)IBNibRefObj_CreateMenuFromNib, 1,
PyDoc_STR("(CFStringRef inName) -> (MenuHandle outMenuRef)")
},
{
"CreateMenuBarFromNib", (PyCFunction)IBNibRefObj_CreateMenuBarFromNib, 1,
PyDoc_STR("(CFStringRef inName) -> (Handle outMenuBar)")
},
{
"SetMenuBarFromNib", (PyCFunction)IBNibRefObj_SetMenuBarFromNib, 1,
PyDoc_STR("(CFStringRef inName) -> None")
},
{NULL, NULL, 0}
};
#define IBNibRefObj_getsetlist NULL
#define IBNibRefObj_compare NULL
#define IBNibRefObj_repr NULL
#define IBNibRefObj_hash NULL
#define IBNibRefObj_tp_init 0
#define IBNibRefObj_tp_alloc PyType_GenericAlloc
static PyObject *IBNibRefObj_tp_new(PyTypeObject *type, PyObject *_args, PyObject *_kwds) {
PyObject *_self;
IBNibRef itself;
char *kw[] = {"itself", 0};
if (!PyArg_ParseTupleAndKeywords(_args, _kwds, "O&", kw, IBNibRefObj_Convert, &itself)) return NULL;
if ((_self = type->tp_alloc(type, 0)) == NULL) return NULL;
((IBNibRefObject *)_self)->ob_itself = itself;
return _self;
}
#define IBNibRefObj_tp_free PyObject_Del
PyTypeObject IBNibRef_Type = {
PyObject_HEAD_INIT(NULL)
0,
"_IBCarbon.IBNibRef",
sizeof(IBNibRefObject),
0,
(destructor) IBNibRefObj_dealloc,
0,
(getattrfunc)0,
(setattrfunc)0,
(cmpfunc) IBNibRefObj_compare,
(reprfunc) IBNibRefObj_repr,
(PyNumberMethods *)0,
(PySequenceMethods *)0,
(PyMappingMethods *)0,
(hashfunc) IBNibRefObj_hash,
0,
0,
PyObject_GenericGetAttr,
PyObject_GenericSetAttr,
0,
Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE,
0,
0,
0,
0,
0,
0,
0,
IBNibRefObj_methods,
0,
IBNibRefObj_getsetlist,
0,
0,
0,
0,
0,
IBNibRefObj_tp_init,
IBNibRefObj_tp_alloc,
IBNibRefObj_tp_new,
IBNibRefObj_tp_free,
};
static PyObject *IBCarbon_CreateNibReference(PyObject *_self, PyObject *_args) {
PyObject *_res = NULL;
OSStatus _err;
CFStringRef inNibName;
IBNibRef outNibRef;
if (!PyArg_ParseTuple(_args, "O&",
CFStringRefObj_Convert, &inNibName))
return NULL;
_err = CreateNibReference(inNibName,
&outNibRef);
if (_err != noErr) return PyMac_Error(_err);
_res = Py_BuildValue("O&",
IBNibRefObj_New, outNibRef);
return _res;
}
#endif
static PyMethodDef IBCarbon_methods[] = {
#if !defined(__LP64__)
{
"CreateNibReference", (PyCFunction)IBCarbon_CreateNibReference, 1,
PyDoc_STR("(CFStringRef inNibName) -> (IBNibRef outNibRef)")
},
#endif
{NULL, NULL, 0}
};
void init_IBCarbon(void) {
PyObject *m;
#if !defined(__LP64__)
PyObject *d;
#endif
m = Py_InitModule("_IBCarbon", IBCarbon_methods);
#if !defined(__LP64__)
d = PyModule_GetDict(m);
IBCarbon_Error = PyMac_GetOSErrException();
if (IBCarbon_Error == NULL ||
PyDict_SetItemString(d, "Error", IBCarbon_Error) != 0)
return;
IBNibRef_Type.ob_type = &PyType_Type;
if (PyType_Ready(&IBNibRef_Type) < 0) return;
Py_INCREF(&IBNibRef_Type);
PyModule_AddObject(m, "IBNibRef", (PyObject *)&IBNibRef_Type);
Py_INCREF(&IBNibRef_Type);
PyModule_AddObject(m, "IBNibRefType", (PyObject *)&IBNibRef_Type);
#endif
}