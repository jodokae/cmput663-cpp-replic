#include "Python.h"
#include "pymactoolbox.h"
#define PyMac_PRECHECK(rtn) do { if ( &rtn == NULL ) {PyErr_SetString(PyExc_NotImplementedError, "Not available in this shared library/OS version"); return NULL; }} while(0)
#include <Carbon/Carbon.h>
#if defined(USE_TOOLBOX_OBJECT_GLUE)
extern PyObject *_ResObj_New(Handle);
extern int _ResObj_Convert(PyObject *, Handle *);
extern PyObject *_OptResObj_New(Handle);
extern int _OptResObj_Convert(PyObject *, Handle *);
#define ResObj_New _ResObj_New
#define ResObj_Convert _ResObj_Convert
#define OptResObj_New _OptResObj_New
#define OptResObj_Convert _OptResObj_Convert
#endif
static void
PyMac_AutoDisposeHandle(Handle h) {
DisposeHandle(h);
}
static PyObject *Res_Error;
PyTypeObject Resource_Type;
#define ResObj_Check(x) ((x)->ob_type == &Resource_Type || PyObject_TypeCheck((x), &Resource_Type))
typedef struct ResourceObject {
PyObject_HEAD
Handle ob_itself;
void (*ob_freeit)(Handle ptr);
} ResourceObject;
PyObject *ResObj_New(Handle itself) {
ResourceObject *it;
if (itself == NULL) return PyMac_Error(resNotFound);
it = PyObject_NEW(ResourceObject, &Resource_Type);
if (it == NULL) return NULL;
it->ob_itself = itself;
it->ob_freeit = NULL;
return (PyObject *)it;
}
int ResObj_Convert(PyObject *v, Handle *p_itself) {
if (!ResObj_Check(v)) {
PyObject *tmp;
if ( (tmp=PyObject_CallMethod(v, "as_Resource", "")) ) {
*p_itself = ((ResourceObject *)tmp)->ob_itself;
Py_DECREF(tmp);
return 1;
}
PyErr_Clear();
}
if (!ResObj_Check(v)) {
PyErr_SetString(PyExc_TypeError, "Resource required");
return 0;
}
*p_itself = ((ResourceObject *)v)->ob_itself;
return 1;
}
static void ResObj_dealloc(ResourceObject *self) {
if (self->ob_freeit && self->ob_itself) {
self->ob_freeit(self->ob_itself);
}
self->ob_itself = NULL;
self->ob_type->tp_free((PyObject *)self);
}
static PyObject *ResObj_HomeResFile(ResourceObject *_self, PyObject *_args) {
PyObject *_res = NULL;
short _rv;
#if !defined(HomeResFile)
PyMac_PRECHECK(HomeResFile);
#endif
if (!PyArg_ParseTuple(_args, ""))
return NULL;
_rv = HomeResFile(_self->ob_itself);
{
OSErr _err = ResError();
if (_err != noErr) return PyMac_Error(_err);
}
_res = Py_BuildValue("h",
_rv);
return _res;
}
static PyObject *ResObj_MacLoadResource(ResourceObject *_self, PyObject *_args) {
PyObject *_res = NULL;
#if !defined(MacLoadResource)
PyMac_PRECHECK(MacLoadResource);
#endif
if (!PyArg_ParseTuple(_args, ""))
return NULL;
MacLoadResource(_self->ob_itself);
{
OSErr _err = ResError();
if (_err != noErr) return PyMac_Error(_err);
}
Py_INCREF(Py_None);
_res = Py_None;
return _res;
}
static PyObject *ResObj_ReleaseResource(ResourceObject *_self, PyObject *_args) {
PyObject *_res = NULL;
#if !defined(ReleaseResource)
PyMac_PRECHECK(ReleaseResource);
#endif
if (!PyArg_ParseTuple(_args, ""))
return NULL;
ReleaseResource(_self->ob_itself);
{
OSErr _err = ResError();
if (_err != noErr) return PyMac_Error(_err);
}
Py_INCREF(Py_None);
_res = Py_None;
return _res;
}
static PyObject *ResObj_DetachResource(ResourceObject *_self, PyObject *_args) {
PyObject *_res = NULL;
#if !defined(DetachResource)
PyMac_PRECHECK(DetachResource);
#endif
if (!PyArg_ParseTuple(_args, ""))
return NULL;
DetachResource(_self->ob_itself);
{
OSErr _err = ResError();
if (_err != noErr) return PyMac_Error(_err);
}
Py_INCREF(Py_None);
_res = Py_None;
return _res;
}
static PyObject *ResObj_GetResAttrs(ResourceObject *_self, PyObject *_args) {
PyObject *_res = NULL;
short _rv;
#if !defined(GetResAttrs)
PyMac_PRECHECK(GetResAttrs);
#endif
if (!PyArg_ParseTuple(_args, ""))
return NULL;
_rv = GetResAttrs(_self->ob_itself);
{
OSErr _err = ResError();
if (_err != noErr) return PyMac_Error(_err);
}
_res = Py_BuildValue("h",
_rv);
return _res;
}
static PyObject *ResObj_GetResInfo(ResourceObject *_self, PyObject *_args) {
PyObject *_res = NULL;
short theID;
ResType theType;
Str255 name;
#if !defined(GetResInfo)
PyMac_PRECHECK(GetResInfo);
#endif
if (!PyArg_ParseTuple(_args, ""))
return NULL;
GetResInfo(_self->ob_itself,
&theID,
&theType,
name);
{
OSErr _err = ResError();
if (_err != noErr) return PyMac_Error(_err);
}
_res = Py_BuildValue("hO&O&",
theID,
PyMac_BuildOSType, theType,
PyMac_BuildStr255, name);
return _res;
}
static PyObject *ResObj_SetResInfo(ResourceObject *_self, PyObject *_args) {
PyObject *_res = NULL;
short theID;
Str255 name;
#if !defined(SetResInfo)
PyMac_PRECHECK(SetResInfo);
#endif
if (!PyArg_ParseTuple(_args, "hO&",
&theID,
PyMac_GetStr255, name))
return NULL;
SetResInfo(_self->ob_itself,
theID,
name);
{
OSErr _err = ResError();
if (_err != noErr) return PyMac_Error(_err);
}
Py_INCREF(Py_None);
_res = Py_None;
return _res;
}
static PyObject *ResObj_AddResource(ResourceObject *_self, PyObject *_args) {
PyObject *_res = NULL;
ResType theType;
short theID;
Str255 name;
#if !defined(AddResource)
PyMac_PRECHECK(AddResource);
#endif
if (!PyArg_ParseTuple(_args, "O&hO&",
PyMac_GetOSType, &theType,
&theID,
PyMac_GetStr255, name))
return NULL;
AddResource(_self->ob_itself,
theType,
theID,
name);
{
OSErr _err = ResError();
if (_err != noErr) return PyMac_Error(_err);
}
Py_INCREF(Py_None);
_res = Py_None;
return _res;
}
static PyObject *ResObj_GetResourceSizeOnDisk(ResourceObject *_self, PyObject *_args) {
PyObject *_res = NULL;
long _rv;
#if !defined(GetResourceSizeOnDisk)
PyMac_PRECHECK(GetResourceSizeOnDisk);
#endif
if (!PyArg_ParseTuple(_args, ""))
return NULL;
_rv = GetResourceSizeOnDisk(_self->ob_itself);
{
OSErr _err = ResError();
if (_err != noErr) return PyMac_Error(_err);
}
_res = Py_BuildValue("l",
_rv);
return _res;
}
static PyObject *ResObj_GetMaxResourceSize(ResourceObject *_self, PyObject *_args) {
PyObject *_res = NULL;
long _rv;
#if !defined(GetMaxResourceSize)
PyMac_PRECHECK(GetMaxResourceSize);
#endif
if (!PyArg_ParseTuple(_args, ""))
return NULL;
_rv = GetMaxResourceSize(_self->ob_itself);
{
OSErr _err = ResError();
if (_err != noErr) return PyMac_Error(_err);
}
_res = Py_BuildValue("l",
_rv);
return _res;
}
static PyObject *ResObj_SetResAttrs(ResourceObject *_self, PyObject *_args) {
PyObject *_res = NULL;
short attrs;
#if !defined(SetResAttrs)
PyMac_PRECHECK(SetResAttrs);
#endif
if (!PyArg_ParseTuple(_args, "h",
&attrs))
return NULL;
SetResAttrs(_self->ob_itself,
attrs);
{
OSErr _err = ResError();
if (_err != noErr) return PyMac_Error(_err);
}
Py_INCREF(Py_None);
_res = Py_None;
return _res;
}
static PyObject *ResObj_ChangedResource(ResourceObject *_self, PyObject *_args) {
PyObject *_res = NULL;
#if !defined(ChangedResource)
PyMac_PRECHECK(ChangedResource);
#endif
if (!PyArg_ParseTuple(_args, ""))
return NULL;
ChangedResource(_self->ob_itself);
{
OSErr _err = ResError();
if (_err != noErr) return PyMac_Error(_err);
}
Py_INCREF(Py_None);
_res = Py_None;
return _res;
}
static PyObject *ResObj_RemoveResource(ResourceObject *_self, PyObject *_args) {
PyObject *_res = NULL;
#if !defined(RemoveResource)
PyMac_PRECHECK(RemoveResource);
#endif
if (!PyArg_ParseTuple(_args, ""))
return NULL;
RemoveResource(_self->ob_itself);
{
OSErr _err = ResError();
if (_err != noErr) return PyMac_Error(_err);
}
Py_INCREF(Py_None);
_res = Py_None;
return _res;
}
static PyObject *ResObj_WriteResource(ResourceObject *_self, PyObject *_args) {
PyObject *_res = NULL;
#if !defined(WriteResource)
PyMac_PRECHECK(WriteResource);
#endif
if (!PyArg_ParseTuple(_args, ""))
return NULL;
WriteResource(_self->ob_itself);
{
OSErr _err = ResError();
if (_err != noErr) return PyMac_Error(_err);
}
Py_INCREF(Py_None);
_res = Py_None;
return _res;
}
static PyObject *ResObj_SetResourceSize(ResourceObject *_self, PyObject *_args) {
PyObject *_res = NULL;
long newSize;
#if !defined(SetResourceSize)
PyMac_PRECHECK(SetResourceSize);
#endif
if (!PyArg_ParseTuple(_args, "l",
&newSize))
return NULL;
SetResourceSize(_self->ob_itself,
newSize);
{
OSErr _err = ResError();
if (_err != noErr) return PyMac_Error(_err);
}
Py_INCREF(Py_None);
_res = Py_None;
return _res;
}
static PyObject *ResObj_GetNextFOND(ResourceObject *_self, PyObject *_args) {
PyObject *_res = NULL;
Handle _rv;
#if !defined(GetNextFOND)
PyMac_PRECHECK(GetNextFOND);
#endif
if (!PyArg_ParseTuple(_args, ""))
return NULL;
_rv = GetNextFOND(_self->ob_itself);
{
OSErr _err = ResError();
if (_err != noErr) return PyMac_Error(_err);
}
_res = Py_BuildValue("O&",
ResObj_New, _rv);
return _res;
}
#if !defined(__LP64__)
static PyObject *ResObj_as_Control(ResourceObject *_self, PyObject *_args) {
PyObject *_res = NULL;
_res = CtlObj_New((ControlHandle)_self->ob_itself);
return _res;
}
static PyObject *ResObj_as_Menu(ResourceObject *_self, PyObject *_args) {
PyObject *_res = NULL;
_res = MenuObj_New((MenuHandle)_self->ob_itself);
return _res;
}
#endif
static PyObject *ResObj_LoadResource(ResourceObject *_self, PyObject *_args) {
PyObject *_res = NULL;
#if !defined(LoadResource)
PyMac_PRECHECK(LoadResource);
#endif
if (!PyArg_ParseTuple(_args, ""))
return NULL;
LoadResource(_self->ob_itself);
{
OSErr _err = ResError();
if (_err != noErr) return PyMac_Error(_err);
}
Py_INCREF(Py_None);
_res = Py_None;
return _res;
}
static PyObject *ResObj_AutoDispose(ResourceObject *_self, PyObject *_args) {
PyObject *_res = NULL;
int onoff, old = 0;
if (!PyArg_ParseTuple(_args, "i", &onoff))
return NULL;
if ( _self->ob_freeit )
old = 1;
if ( onoff )
_self->ob_freeit = PyMac_AutoDisposeHandle;
else
_self->ob_freeit = NULL;
_res = Py_BuildValue("i", old);
return _res;
}
static PyMethodDef ResObj_methods[] = {
{
"HomeResFile", (PyCFunction)ResObj_HomeResFile, 1,
PyDoc_STR("() -> (short _rv)")
},
{
"MacLoadResource", (PyCFunction)ResObj_MacLoadResource, 1,
PyDoc_STR("() -> None")
},
{
"ReleaseResource", (PyCFunction)ResObj_ReleaseResource, 1,
PyDoc_STR("() -> None")
},
{
"DetachResource", (PyCFunction)ResObj_DetachResource, 1,
PyDoc_STR("() -> None")
},
{
"GetResAttrs", (PyCFunction)ResObj_GetResAttrs, 1,
PyDoc_STR("() -> (short _rv)")
},
{
"GetResInfo", (PyCFunction)ResObj_GetResInfo, 1,
PyDoc_STR("() -> (short theID, ResType theType, Str255 name)")
},
{
"SetResInfo", (PyCFunction)ResObj_SetResInfo, 1,
PyDoc_STR("(short theID, Str255 name) -> None")
},
{
"AddResource", (PyCFunction)ResObj_AddResource, 1,
PyDoc_STR("(ResType theType, short theID, Str255 name) -> None")
},
{
"GetResourceSizeOnDisk", (PyCFunction)ResObj_GetResourceSizeOnDisk, 1,
PyDoc_STR("() -> (long _rv)")
},
{
"GetMaxResourceSize", (PyCFunction)ResObj_GetMaxResourceSize, 1,
PyDoc_STR("() -> (long _rv)")
},
{
"SetResAttrs", (PyCFunction)ResObj_SetResAttrs, 1,
PyDoc_STR("(short attrs) -> None")
},
{
"ChangedResource", (PyCFunction)ResObj_ChangedResource, 1,
PyDoc_STR("() -> None")
},
{
"RemoveResource", (PyCFunction)ResObj_RemoveResource, 1,
PyDoc_STR("() -> None")
},
{
"WriteResource", (PyCFunction)ResObj_WriteResource, 1,
PyDoc_STR("() -> None")
},
{
"SetResourceSize", (PyCFunction)ResObj_SetResourceSize, 1,
PyDoc_STR("(long newSize) -> None")
},
{
"GetNextFOND", (PyCFunction)ResObj_GetNextFOND, 1,
PyDoc_STR("() -> (Handle _rv)")
},
#if !defined(__LP64__)
{
"as_Control", (PyCFunction)ResObj_as_Control, 1,
PyDoc_STR("Return this resource/handle as a Control")
},
{
"as_Menu", (PyCFunction)ResObj_as_Menu, 1,
PyDoc_STR("Return this resource/handle as a Menu")
},
#endif
{
"LoadResource", (PyCFunction)ResObj_LoadResource, 1,
PyDoc_STR("() -> None")
},
{
"AutoDispose", (PyCFunction)ResObj_AutoDispose, 1,
PyDoc_STR("(int)->int. Automatically DisposeHandle the object on Python object cleanup")
},
{NULL, NULL, 0}
};
static PyObject *ResObj_get_data(ResourceObject *self, void *closure) {
PyObject *res;
char state;
state = HGetState(self->ob_itself);
HLock(self->ob_itself);
res = PyString_FromStringAndSize(
*self->ob_itself,
GetHandleSize(self->ob_itself));
HUnlock(self->ob_itself);
HSetState(self->ob_itself, state);
return res;
}
static int ResObj_set_data(ResourceObject *self, PyObject *v, void *closure) {
char *data;
long size;
if ( v == NULL )
return -1;
if ( !PyString_Check(v) )
return -1;
size = PyString_Size(v);
data = PyString_AsString(v);
SetHandleSize(self->ob_itself, size);
if ( MemError())
return -1;
HLock(self->ob_itself);
memcpy((char *)*self->ob_itself, data, size);
HUnlock(self->ob_itself);
return 0;
return 0;
}
static PyObject *ResObj_get_size(ResourceObject *self, void *closure) {
return PyInt_FromLong(GetHandleSize(self->ob_itself));
}
#define ResObj_set_size NULL
static PyGetSetDef ResObj_getsetlist[] = {
{"data", (getter)ResObj_get_data, (setter)ResObj_set_data, "The resource data"},
{"size", (getter)ResObj_get_size, (setter)ResObj_set_size, "The length of the resource data"},
{NULL, NULL, NULL, NULL},
};
#define ResObj_compare NULL
#define ResObj_repr NULL
#define ResObj_hash NULL
static int ResObj_tp_init(PyObject *_self, PyObject *_args, PyObject *_kwds) {
char *srcdata = NULL;
int srclen = 0;
Handle itself;
char *kw[] = {"itself", 0};
if (PyArg_ParseTupleAndKeywords(_args, _kwds, "O&", kw, ResObj_Convert, &itself)) {
((ResourceObject *)_self)->ob_itself = itself;
return 0;
}
PyErr_Clear();
if (!PyArg_ParseTupleAndKeywords(_args, _kwds, "|s#", kw, &srcdata, &srclen)) return -1;
if ((itself = NewHandle(srclen)) == NULL) {
PyErr_NoMemory();
return 0;
}
((ResourceObject *)_self)->ob_itself = itself;
if (srclen && srcdata) {
HLock(itself);
memcpy(*itself, srcdata, srclen);
HUnlock(itself);
}
return 0;
}
#define ResObj_tp_alloc PyType_GenericAlloc
static PyObject *ResObj_tp_new(PyTypeObject *type, PyObject *_args, PyObject *_kwds) {
PyObject *self;
if ((self = type->tp_alloc(type, 0)) == NULL) return NULL;
((ResourceObject *)self)->ob_itself = NULL;
((ResourceObject *)self)->ob_freeit = NULL;
return self;
}
#define ResObj_tp_free PyObject_Del
PyTypeObject Resource_Type = {
PyObject_HEAD_INIT(NULL)
0,
"_Res.Resource",
sizeof(ResourceObject),
0,
(destructor) ResObj_dealloc,
0,
(getattrfunc)0,
(setattrfunc)0,
(cmpfunc) ResObj_compare,
(reprfunc) ResObj_repr,
(PyNumberMethods *)0,
(PySequenceMethods *)0,
(PyMappingMethods *)0,
(hashfunc) ResObj_hash,
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
ResObj_methods,
0,
ResObj_getsetlist,
0,
0,
0,
0,
0,
ResObj_tp_init,
ResObj_tp_alloc,
ResObj_tp_new,
ResObj_tp_free,
};
static PyObject *Res_CloseResFile(PyObject *_self, PyObject *_args) {
PyObject *_res = NULL;
short refNum;
#if !defined(CloseResFile)
PyMac_PRECHECK(CloseResFile);
#endif
if (!PyArg_ParseTuple(_args, "h",
&refNum))
return NULL;
CloseResFile(refNum);
{
OSErr _err = ResError();
if (_err != noErr) return PyMac_Error(_err);
}
Py_INCREF(Py_None);
_res = Py_None;
return _res;
}
static PyObject *Res_ResError(PyObject *_self, PyObject *_args) {
PyObject *_res = NULL;
OSErr _err;
#if !defined(ResError)
PyMac_PRECHECK(ResError);
#endif
if (!PyArg_ParseTuple(_args, ""))
return NULL;
_err = ResError();
if (_err != noErr) return PyMac_Error(_err);
Py_INCREF(Py_None);
_res = Py_None;
return _res;
}
static PyObject *Res_CurResFile(PyObject *_self, PyObject *_args) {
PyObject *_res = NULL;
short _rv;
#if !defined(CurResFile)
PyMac_PRECHECK(CurResFile);
#endif
if (!PyArg_ParseTuple(_args, ""))
return NULL;
_rv = CurResFile();
{
OSErr _err = ResError();
if (_err != noErr) return PyMac_Error(_err);
}
_res = Py_BuildValue("h",
_rv);
return _res;
}
static PyObject *Res_UseResFile(PyObject *_self, PyObject *_args) {
PyObject *_res = NULL;
short refNum;
#if !defined(UseResFile)
PyMac_PRECHECK(UseResFile);
#endif
if (!PyArg_ParseTuple(_args, "h",
&refNum))
return NULL;
UseResFile(refNum);
{
OSErr _err = ResError();
if (_err != noErr) return PyMac_Error(_err);
}
Py_INCREF(Py_None);
_res = Py_None;
return _res;
}
static PyObject *Res_CountTypes(PyObject *_self, PyObject *_args) {
PyObject *_res = NULL;
short _rv;
#if !defined(CountTypes)
PyMac_PRECHECK(CountTypes);
#endif
if (!PyArg_ParseTuple(_args, ""))
return NULL;
_rv = CountTypes();
{
OSErr _err = ResError();
if (_err != noErr) return PyMac_Error(_err);
}
_res = Py_BuildValue("h",
_rv);
return _res;
}
static PyObject *Res_Count1Types(PyObject *_self, PyObject *_args) {
PyObject *_res = NULL;
short _rv;
#if !defined(Count1Types)
PyMac_PRECHECK(Count1Types);
#endif
if (!PyArg_ParseTuple(_args, ""))
return NULL;
_rv = Count1Types();
{
OSErr _err = ResError();
if (_err != noErr) return PyMac_Error(_err);
}
_res = Py_BuildValue("h",
_rv);
return _res;
}
static PyObject *Res_GetIndType(PyObject *_self, PyObject *_args) {
PyObject *_res = NULL;
ResType theType;
short index;
#if !defined(GetIndType)
PyMac_PRECHECK(GetIndType);
#endif
if (!PyArg_ParseTuple(_args, "h",
&index))
return NULL;
GetIndType(&theType,
index);
{
OSErr _err = ResError();
if (_err != noErr) return PyMac_Error(_err);
}
_res = Py_BuildValue("O&",
PyMac_BuildOSType, theType);
return _res;
}
static PyObject *Res_Get1IndType(PyObject *_self, PyObject *_args) {
PyObject *_res = NULL;
ResType theType;
short index;
#if !defined(Get1IndType)
PyMac_PRECHECK(Get1IndType);
#endif
if (!PyArg_ParseTuple(_args, "h",
&index))
return NULL;
Get1IndType(&theType,
index);
{
OSErr _err = ResError();
if (_err != noErr) return PyMac_Error(_err);
}
_res = Py_BuildValue("O&",
PyMac_BuildOSType, theType);
return _res;
}
static PyObject *Res_SetResLoad(PyObject *_self, PyObject *_args) {
PyObject *_res = NULL;
Boolean load;
#if !defined(SetResLoad)
PyMac_PRECHECK(SetResLoad);
#endif
if (!PyArg_ParseTuple(_args, "b",
&load))
return NULL;
SetResLoad(load);
{
OSErr _err = ResError();
if (_err != noErr) return PyMac_Error(_err);
}
Py_INCREF(Py_None);
_res = Py_None;
return _res;
}
static PyObject *Res_CountResources(PyObject *_self, PyObject *_args) {
PyObject *_res = NULL;
short _rv;
ResType theType;
#if !defined(CountResources)
PyMac_PRECHECK(CountResources);
#endif
if (!PyArg_ParseTuple(_args, "O&",
PyMac_GetOSType, &theType))
return NULL;
_rv = CountResources(theType);
{
OSErr _err = ResError();
if (_err != noErr) return PyMac_Error(_err);
}
_res = Py_BuildValue("h",
_rv);
return _res;
}
static PyObject *Res_Count1Resources(PyObject *_self, PyObject *_args) {
PyObject *_res = NULL;
short _rv;
ResType theType;
#if !defined(Count1Resources)
PyMac_PRECHECK(Count1Resources);
#endif
if (!PyArg_ParseTuple(_args, "O&",
PyMac_GetOSType, &theType))
return NULL;
_rv = Count1Resources(theType);
{
OSErr _err = ResError();
if (_err != noErr) return PyMac_Error(_err);
}
_res = Py_BuildValue("h",
_rv);
return _res;
}
static PyObject *Res_GetIndResource(PyObject *_self, PyObject *_args) {
PyObject *_res = NULL;
Handle _rv;
ResType theType;
short index;
#if !defined(GetIndResource)
PyMac_PRECHECK(GetIndResource);
#endif
if (!PyArg_ParseTuple(_args, "O&h",
PyMac_GetOSType, &theType,
&index))
return NULL;
_rv = GetIndResource(theType,
index);
{
OSErr _err = ResError();
if (_err != noErr) return PyMac_Error(_err);
}
_res = Py_BuildValue("O&",
ResObj_New, _rv);
return _res;
}
static PyObject *Res_Get1IndResource(PyObject *_self, PyObject *_args) {
PyObject *_res = NULL;
Handle _rv;
ResType theType;
short index;
#if !defined(Get1IndResource)
PyMac_PRECHECK(Get1IndResource);
#endif
if (!PyArg_ParseTuple(_args, "O&h",
PyMac_GetOSType, &theType,
&index))
return NULL;
_rv = Get1IndResource(theType,
index);
{
OSErr _err = ResError();
if (_err != noErr) return PyMac_Error(_err);
}
_res = Py_BuildValue("O&",
ResObj_New, _rv);
return _res;
}
static PyObject *Res_GetResource(PyObject *_self, PyObject *_args) {
PyObject *_res = NULL;
Handle _rv;
ResType theType;
short theID;
#if !defined(GetResource)
PyMac_PRECHECK(GetResource);
#endif
if (!PyArg_ParseTuple(_args, "O&h",
PyMac_GetOSType, &theType,
&theID))
return NULL;
_rv = GetResource(theType,
theID);
{
OSErr _err = ResError();
if (_err != noErr) return PyMac_Error(_err);
}
_res = Py_BuildValue("O&",
ResObj_New, _rv);
return _res;
}
static PyObject *Res_Get1Resource(PyObject *_self, PyObject *_args) {
PyObject *_res = NULL;
Handle _rv;
ResType theType;
short theID;
#if !defined(Get1Resource)
PyMac_PRECHECK(Get1Resource);
#endif
if (!PyArg_ParseTuple(_args, "O&h",
PyMac_GetOSType, &theType,
&theID))
return NULL;
_rv = Get1Resource(theType,
theID);
{
OSErr _err = ResError();
if (_err != noErr) return PyMac_Error(_err);
}
_res = Py_BuildValue("O&",
ResObj_New, _rv);
return _res;
}
static PyObject *Res_GetNamedResource(PyObject *_self, PyObject *_args) {
PyObject *_res = NULL;
Handle _rv;
ResType theType;
Str255 name;
#if !defined(GetNamedResource)
PyMac_PRECHECK(GetNamedResource);
#endif
if (!PyArg_ParseTuple(_args, "O&O&",
PyMac_GetOSType, &theType,
PyMac_GetStr255, name))
return NULL;
_rv = GetNamedResource(theType,
name);
{
OSErr _err = ResError();
if (_err != noErr) return PyMac_Error(_err);
}
_res = Py_BuildValue("O&",
ResObj_New, _rv);
return _res;
}
static PyObject *Res_Get1NamedResource(PyObject *_self, PyObject *_args) {
PyObject *_res = NULL;
Handle _rv;
ResType theType;
Str255 name;
#if !defined(Get1NamedResource)
PyMac_PRECHECK(Get1NamedResource);
#endif
if (!PyArg_ParseTuple(_args, "O&O&",
PyMac_GetOSType, &theType,
PyMac_GetStr255, name))
return NULL;
_rv = Get1NamedResource(theType,
name);
{
OSErr _err = ResError();
if (_err != noErr) return PyMac_Error(_err);
}
_res = Py_BuildValue("O&",
ResObj_New, _rv);
return _res;
}
static PyObject *Res_UniqueID(PyObject *_self, PyObject *_args) {
PyObject *_res = NULL;
short _rv;
ResType theType;
#if !defined(UniqueID)
PyMac_PRECHECK(UniqueID);
#endif
if (!PyArg_ParseTuple(_args, "O&",
PyMac_GetOSType, &theType))
return NULL;
_rv = UniqueID(theType);
{
OSErr _err = ResError();
if (_err != noErr) return PyMac_Error(_err);
}
_res = Py_BuildValue("h",
_rv);
return _res;
}
static PyObject *Res_Unique1ID(PyObject *_self, PyObject *_args) {
PyObject *_res = NULL;
short _rv;
ResType theType;
#if !defined(Unique1ID)
PyMac_PRECHECK(Unique1ID);
#endif
if (!PyArg_ParseTuple(_args, "O&",
PyMac_GetOSType, &theType))
return NULL;
_rv = Unique1ID(theType);
{
OSErr _err = ResError();
if (_err != noErr) return PyMac_Error(_err);
}
_res = Py_BuildValue("h",
_rv);
return _res;
}
static PyObject *Res_UpdateResFile(PyObject *_self, PyObject *_args) {
PyObject *_res = NULL;
short refNum;
#if !defined(UpdateResFile)
PyMac_PRECHECK(UpdateResFile);
#endif
if (!PyArg_ParseTuple(_args, "h",
&refNum))
return NULL;
UpdateResFile(refNum);
{
OSErr _err = ResError();
if (_err != noErr) return PyMac_Error(_err);
}
Py_INCREF(Py_None);
_res = Py_None;
return _res;
}
static PyObject *Res_SetResPurge(PyObject *_self, PyObject *_args) {
PyObject *_res = NULL;
Boolean install;
#if !defined(SetResPurge)
PyMac_PRECHECK(SetResPurge);
#endif
if (!PyArg_ParseTuple(_args, "b",
&install))
return NULL;
SetResPurge(install);
{
OSErr _err = ResError();
if (_err != noErr) return PyMac_Error(_err);
}
Py_INCREF(Py_None);
_res = Py_None;
return _res;
}
static PyObject *Res_GetResFileAttrs(PyObject *_self, PyObject *_args) {
PyObject *_res = NULL;
short _rv;
short refNum;
#if !defined(GetResFileAttrs)
PyMac_PRECHECK(GetResFileAttrs);
#endif
if (!PyArg_ParseTuple(_args, "h",
&refNum))
return NULL;
_rv = GetResFileAttrs(refNum);
{
OSErr _err = ResError();
if (_err != noErr) return PyMac_Error(_err);
}
_res = Py_BuildValue("h",
_rv);
return _res;
}
static PyObject *Res_SetResFileAttrs(PyObject *_self, PyObject *_args) {
PyObject *_res = NULL;
short refNum;
short attrs;
#if !defined(SetResFileAttrs)
PyMac_PRECHECK(SetResFileAttrs);
#endif
if (!PyArg_ParseTuple(_args, "hh",
&refNum,
&attrs))
return NULL;
SetResFileAttrs(refNum,
attrs);
{
OSErr _err = ResError();
if (_err != noErr) return PyMac_Error(_err);
}
Py_INCREF(Py_None);
_res = Py_None;
return _res;
}
#if !defined(__LP64__)
static PyObject *Res_OpenRFPerm(PyObject *_self, PyObject *_args) {
PyObject *_res = NULL;
short _rv;
Str255 fileName;
short vRefNum;
SignedByte permission;
#if !defined(OpenRFPerm)
PyMac_PRECHECK(OpenRFPerm);
#endif
if (!PyArg_ParseTuple(_args, "O&hb",
PyMac_GetStr255, fileName,
&vRefNum,
&permission))
return NULL;
_rv = OpenRFPerm(fileName,
vRefNum,
permission);
{
OSErr _err = ResError();
if (_err != noErr) return PyMac_Error(_err);
}
_res = Py_BuildValue("h",
_rv);
return _res;
}
static PyObject *Res_HOpenResFile(PyObject *_self, PyObject *_args) {
PyObject *_res = NULL;
short _rv;
short vRefNum;
long dirID;
Str255 fileName;
SignedByte permission;
#if !defined(HOpenResFile)
PyMac_PRECHECK(HOpenResFile);
#endif
if (!PyArg_ParseTuple(_args, "hlO&b",
&vRefNum,
&dirID,
PyMac_GetStr255, fileName,
&permission))
return NULL;
_rv = HOpenResFile(vRefNum,
dirID,
fileName,
permission);
{
OSErr _err = ResError();
if (_err != noErr) return PyMac_Error(_err);
}
_res = Py_BuildValue("h",
_rv);
return _res;
}
static PyObject *Res_HCreateResFile(PyObject *_self, PyObject *_args) {
PyObject *_res = NULL;
short vRefNum;
long dirID;
Str255 fileName;
#if !defined(HCreateResFile)
PyMac_PRECHECK(HCreateResFile);
#endif
if (!PyArg_ParseTuple(_args, "hlO&",
&vRefNum,
&dirID,
PyMac_GetStr255, fileName))
return NULL;
HCreateResFile(vRefNum,
dirID,
fileName);
{
OSErr _err = ResError();
if (_err != noErr) return PyMac_Error(_err);
}
Py_INCREF(Py_None);
_res = Py_None;
return _res;
}
static PyObject *Res_FSpOpenResFile(PyObject *_self, PyObject *_args) {
PyObject *_res = NULL;
short _rv;
FSSpec spec;
SignedByte permission;
#if !defined(FSpOpenResFile)
PyMac_PRECHECK(FSpOpenResFile);
#endif
if (!PyArg_ParseTuple(_args, "O&b",
PyMac_GetFSSpec, &spec,
&permission))
return NULL;
_rv = FSpOpenResFile(&spec,
permission);
{
OSErr _err = ResError();
if (_err != noErr) return PyMac_Error(_err);
}
_res = Py_BuildValue("h",
_rv);
return _res;
}
static PyObject *Res_FSpCreateResFile(PyObject *_self, PyObject *_args) {
PyObject *_res = NULL;
FSSpec spec;
OSType creator;
OSType fileType;
ScriptCode scriptTag;
#if !defined(FSpCreateResFile)
PyMac_PRECHECK(FSpCreateResFile);
#endif
if (!PyArg_ParseTuple(_args, "O&O&O&h",
PyMac_GetFSSpec, &spec,
PyMac_GetOSType, &creator,
PyMac_GetOSType, &fileType,
&scriptTag))
return NULL;
FSpCreateResFile(&spec,
creator,
fileType,
scriptTag);
{
OSErr _err = ResError();
if (_err != noErr) return PyMac_Error(_err);
}
Py_INCREF(Py_None);
_res = Py_None;
return _res;
}
#endif
static PyObject *Res_InsertResourceFile(PyObject *_self, PyObject *_args) {
PyObject *_res = NULL;
OSErr _err;
SInt16 refNum;
RsrcChainLocation where;
#if !defined(InsertResourceFile)
PyMac_PRECHECK(InsertResourceFile);
#endif
if (!PyArg_ParseTuple(_args, "hh",
&refNum,
&where))
return NULL;
_err = InsertResourceFile(refNum,
where);
if (_err != noErr) return PyMac_Error(_err);
Py_INCREF(Py_None);
_res = Py_None;
return _res;
}
static PyObject *Res_DetachResourceFile(PyObject *_self, PyObject *_args) {
PyObject *_res = NULL;
OSErr _err;
SInt16 refNum;
#if !defined(DetachResourceFile)
PyMac_PRECHECK(DetachResourceFile);
#endif
if (!PyArg_ParseTuple(_args, "h",
&refNum))
return NULL;
_err = DetachResourceFile(refNum);
if (_err != noErr) return PyMac_Error(_err);
Py_INCREF(Py_None);
_res = Py_None;
return _res;
}
#if !defined(__LP64__)
static PyObject *Res_FSpResourceFileAlreadyOpen(PyObject *_self, PyObject *_args) {
PyObject *_res = NULL;
Boolean _rv;
FSSpec resourceFile;
Boolean inChain;
SInt16 refNum;
#if !defined(FSpResourceFileAlreadyOpen)
PyMac_PRECHECK(FSpResourceFileAlreadyOpen);
#endif
if (!PyArg_ParseTuple(_args, "O&",
PyMac_GetFSSpec, &resourceFile))
return NULL;
_rv = FSpResourceFileAlreadyOpen(&resourceFile,
&inChain,
&refNum);
{
OSErr _err = ResError();
if (_err != noErr) return PyMac_Error(_err);
}
_res = Py_BuildValue("bbh",
_rv,
inChain,
refNum);
return _res;
}
static PyObject *Res_FSpOpenOrphanResFile(PyObject *_self, PyObject *_args) {
PyObject *_res = NULL;
OSErr _err;
FSSpec spec;
SignedByte permission;
SInt16 refNum;
#if !defined(FSpOpenOrphanResFile)
PyMac_PRECHECK(FSpOpenOrphanResFile);
#endif
if (!PyArg_ParseTuple(_args, "O&b",
PyMac_GetFSSpec, &spec,
&permission))
return NULL;
_err = FSpOpenOrphanResFile(&spec,
permission,
&refNum);
if (_err != noErr) return PyMac_Error(_err);
_res = Py_BuildValue("h",
refNum);
return _res;
}
static PyObject *Res_GetTopResourceFile(PyObject *_self, PyObject *_args) {
PyObject *_res = NULL;
OSErr _err;
SInt16 refNum;
#if !defined(GetTopResourceFile)
PyMac_PRECHECK(GetTopResourceFile);
#endif
if (!PyArg_ParseTuple(_args, ""))
return NULL;
_err = GetTopResourceFile(&refNum);
if (_err != noErr) return PyMac_Error(_err);
_res = Py_BuildValue("h",
refNum);
return _res;
}
static PyObject *Res_GetNextResourceFile(PyObject *_self, PyObject *_args) {
PyObject *_res = NULL;
OSErr _err;
SInt16 curRefNum;
SInt16 nextRefNum;
#if !defined(GetNextResourceFile)
PyMac_PRECHECK(GetNextResourceFile);
#endif
if (!PyArg_ParseTuple(_args, "h",
&curRefNum))
return NULL;
_err = GetNextResourceFile(curRefNum,
&nextRefNum);
if (_err != noErr) return PyMac_Error(_err);
_res = Py_BuildValue("h",
nextRefNum);
return _res;
}
#endif
static PyObject *Res_FSOpenResFile(PyObject *_self, PyObject *_args) {
PyObject *_res = NULL;
short _rv;
FSRef ref;
SignedByte permission;
#if !defined(FSOpenResFile)
PyMac_PRECHECK(FSOpenResFile);
#endif
if (!PyArg_ParseTuple(_args, "O&b",
PyMac_GetFSRef, &ref,
&permission))
return NULL;
_rv = FSOpenResFile(&ref,
permission);
{
OSErr _err = ResError();
if (_err != noErr) return PyMac_Error(_err);
}
_res = Py_BuildValue("h",
_rv);
return _res;
}
#if !defined(__LP64__)
static PyObject *Res_FSCreateResFile(PyObject *_self, PyObject *_args) {
PyObject *_res = NULL;
FSRef parentRef;
UniChar *nameLength__in__;
UniCharCount nameLength__len__;
int nameLength__in_len__;
FSRef newRef;
FSSpec newSpec;
#if !defined(FSCreateResFile)
PyMac_PRECHECK(FSCreateResFile);
#endif
if (!PyArg_ParseTuple(_args, "O&u#",
PyMac_GetFSRef, &parentRef,
&nameLength__in__, &nameLength__in_len__))
return NULL;
nameLength__len__ = nameLength__in_len__;
FSCreateResFile(&parentRef,
nameLength__len__, nameLength__in__,
0,
(FSCatalogInfo *)0,
&newRef,
&newSpec);
{
OSErr _err = ResError();
if (_err != noErr) return PyMac_Error(_err);
}
_res = Py_BuildValue("O&O&",
PyMac_BuildFSRef, &newRef,
PyMac_BuildFSSpec, &newSpec);
return _res;
}
static PyObject *Res_FSResourceFileAlreadyOpen(PyObject *_self, PyObject *_args) {
PyObject *_res = NULL;
Boolean _rv;
FSRef resourceFileRef;
Boolean inChain;
SInt16 refNum;
#if !defined(FSResourceFileAlreadyOpen)
PyMac_PRECHECK(FSResourceFileAlreadyOpen);
#endif
if (!PyArg_ParseTuple(_args, "O&",
PyMac_GetFSRef, &resourceFileRef))
return NULL;
_rv = FSResourceFileAlreadyOpen(&resourceFileRef,
&inChain,
&refNum);
{
OSErr _err = ResError();
if (_err != noErr) return PyMac_Error(_err);
}
_res = Py_BuildValue("bbh",
_rv,
inChain,
refNum);
return _res;
}
static PyObject *Res_FSCreateResourceFile(PyObject *_self, PyObject *_args) {
PyObject *_res = NULL;
OSErr _err;
FSRef parentRef;
UniChar *nameLength__in__;
UniCharCount nameLength__len__;
int nameLength__in_len__;
UniChar *forkNameLength__in__;
UniCharCount forkNameLength__len__;
int forkNameLength__in_len__;
FSRef newRef;
FSSpec newSpec;
#if !defined(FSCreateResourceFile)
PyMac_PRECHECK(FSCreateResourceFile);
#endif
if (!PyArg_ParseTuple(_args, "O&u#u#",
PyMac_GetFSRef, &parentRef,
&nameLength__in__, &nameLength__in_len__,
&forkNameLength__in__, &forkNameLength__in_len__))
return NULL;
nameLength__len__ = nameLength__in_len__;
forkNameLength__len__ = forkNameLength__in_len__;
_err = FSCreateResourceFile(&parentRef,
nameLength__len__, nameLength__in__,
0,
(FSCatalogInfo *)0,
forkNameLength__len__, forkNameLength__in__,
&newRef,
&newSpec);
if (_err != noErr) return PyMac_Error(_err);
_res = Py_BuildValue("O&O&",
PyMac_BuildFSRef, &newRef,
PyMac_BuildFSSpec, &newSpec);
return _res;
}
#endif
static PyObject *Res_FSOpenResourceFile(PyObject *_self, PyObject *_args) {
PyObject *_res = NULL;
OSErr _err;
FSRef ref;
UniChar *forkNameLength__in__;
UniCharCount forkNameLength__len__;
int forkNameLength__in_len__;
SignedByte permissions;
ResFileRefNum refNum;
#if !defined(FSOpenResourceFile)
PyMac_PRECHECK(FSOpenResourceFile);
#endif
if (!PyArg_ParseTuple(_args, "O&u#b",
PyMac_GetFSRef, &ref,
&forkNameLength__in__, &forkNameLength__in_len__,
&permissions))
return NULL;
forkNameLength__len__ = forkNameLength__in_len__;
_err = FSOpenResourceFile(&ref,
forkNameLength__len__, forkNameLength__in__,
permissions,
&refNum);
if (_err != noErr) return PyMac_Error(_err);
_res = Py_BuildValue("h",
refNum);
return _res;
}
static PyObject *Res_Handle(PyObject *_self, PyObject *_args) {
PyObject *_res = NULL;
char *buf;
int len;
Handle h;
ResourceObject *rv;
if (!PyArg_ParseTuple(_args, "s#", &buf, &len))
return NULL;
h = NewHandle(len);
if ( h == NULL ) {
PyErr_NoMemory();
return NULL;
}
HLock(h);
memcpy(*h, buf, len);
HUnlock(h);
rv = (ResourceObject *)ResObj_New(h);
rv->ob_freeit = PyMac_AutoDisposeHandle;
_res = (PyObject *)rv;
return _res;
}
static PyMethodDef Res_methods[] = {
{
"CloseResFile", (PyCFunction)Res_CloseResFile, 1,
PyDoc_STR("(short refNum) -> None")
},
{
"ResError", (PyCFunction)Res_ResError, 1,
PyDoc_STR("() -> None")
},
{
"CurResFile", (PyCFunction)Res_CurResFile, 1,
PyDoc_STR("() -> (short _rv)")
},
{
"UseResFile", (PyCFunction)Res_UseResFile, 1,
PyDoc_STR("(short refNum) -> None")
},
{
"CountTypes", (PyCFunction)Res_CountTypes, 1,
PyDoc_STR("() -> (short _rv)")
},
{
"Count1Types", (PyCFunction)Res_Count1Types, 1,
PyDoc_STR("() -> (short _rv)")
},
{
"GetIndType", (PyCFunction)Res_GetIndType, 1,
PyDoc_STR("(short index) -> (ResType theType)")
},
{
"Get1IndType", (PyCFunction)Res_Get1IndType, 1,
PyDoc_STR("(short index) -> (ResType theType)")
},
{
"SetResLoad", (PyCFunction)Res_SetResLoad, 1,
PyDoc_STR("(Boolean load) -> None")
},
{
"CountResources", (PyCFunction)Res_CountResources, 1,
PyDoc_STR("(ResType theType) -> (short _rv)")
},
{
"Count1Resources", (PyCFunction)Res_Count1Resources, 1,
PyDoc_STR("(ResType theType) -> (short _rv)")
},
{
"GetIndResource", (PyCFunction)Res_GetIndResource, 1,
PyDoc_STR("(ResType theType, short index) -> (Handle _rv)")
},
{
"Get1IndResource", (PyCFunction)Res_Get1IndResource, 1,
PyDoc_STR("(ResType theType, short index) -> (Handle _rv)")
},
{
"GetResource", (PyCFunction)Res_GetResource, 1,
PyDoc_STR("(ResType theType, short theID) -> (Handle _rv)")
},
{
"Get1Resource", (PyCFunction)Res_Get1Resource, 1,
PyDoc_STR("(ResType theType, short theID) -> (Handle _rv)")
},
{
"GetNamedResource", (PyCFunction)Res_GetNamedResource, 1,
PyDoc_STR("(ResType theType, Str255 name) -> (Handle _rv)")
},
{
"Get1NamedResource", (PyCFunction)Res_Get1NamedResource, 1,
PyDoc_STR("(ResType theType, Str255 name) -> (Handle _rv)")
},
{
"UniqueID", (PyCFunction)Res_UniqueID, 1,
PyDoc_STR("(ResType theType) -> (short _rv)")
},
{
"Unique1ID", (PyCFunction)Res_Unique1ID, 1,
PyDoc_STR("(ResType theType) -> (short _rv)")
},
{
"UpdateResFile", (PyCFunction)Res_UpdateResFile, 1,
PyDoc_STR("(short refNum) -> None")
},
{
"SetResPurge", (PyCFunction)Res_SetResPurge, 1,
PyDoc_STR("(Boolean install) -> None")
},
{
"GetResFileAttrs", (PyCFunction)Res_GetResFileAttrs, 1,
PyDoc_STR("(short refNum) -> (short _rv)")
},
{
"SetResFileAttrs", (PyCFunction)Res_SetResFileAttrs, 1,
PyDoc_STR("(short refNum, short attrs) -> None")
},
#if !defined(__LP64__)
{
"OpenRFPerm", (PyCFunction)Res_OpenRFPerm, 1,
PyDoc_STR("(Str255 fileName, short vRefNum, SignedByte permission) -> (short _rv)")
},
{
"HOpenResFile", (PyCFunction)Res_HOpenResFile, 1,
PyDoc_STR("(short vRefNum, long dirID, Str255 fileName, SignedByte permission) -> (short _rv)")
},
{
"HCreateResFile", (PyCFunction)Res_HCreateResFile, 1,
PyDoc_STR("(short vRefNum, long dirID, Str255 fileName) -> None")
},
{
"FSpOpenResFile", (PyCFunction)Res_FSpOpenResFile, 1,
PyDoc_STR("(FSSpec spec, SignedByte permission) -> (short _rv)")
},
{
"FSpCreateResFile", (PyCFunction)Res_FSpCreateResFile, 1,
PyDoc_STR("(FSSpec spec, OSType creator, OSType fileType, ScriptCode scriptTag) -> None")
},
#endif
{
"InsertResourceFile", (PyCFunction)Res_InsertResourceFile, 1,
PyDoc_STR("(SInt16 refNum, RsrcChainLocation where) -> None")
},
{
"DetachResourceFile", (PyCFunction)Res_DetachResourceFile, 1,
PyDoc_STR("(SInt16 refNum) -> None")
},
#if !defined(__LP64__)
{
"FSpResourceFileAlreadyOpen", (PyCFunction)Res_FSpResourceFileAlreadyOpen, 1,
PyDoc_STR("(FSSpec resourceFile) -> (Boolean _rv, Boolean inChain, SInt16 refNum)")
},
{
"FSpOpenOrphanResFile", (PyCFunction)Res_FSpOpenOrphanResFile, 1,
PyDoc_STR("(FSSpec spec, SignedByte permission) -> (SInt16 refNum)")
},
{
"GetTopResourceFile", (PyCFunction)Res_GetTopResourceFile, 1,
PyDoc_STR("() -> (SInt16 refNum)")
},
{
"GetNextResourceFile", (PyCFunction)Res_GetNextResourceFile, 1,
PyDoc_STR("(SInt16 curRefNum) -> (SInt16 nextRefNum)")
},
#endif
{
"FSOpenResFile", (PyCFunction)Res_FSOpenResFile, 1,
PyDoc_STR("(FSRef ref, SignedByte permission) -> (short _rv)")
},
#if !defined(__LP64__)
{
"FSCreateResFile", (PyCFunction)Res_FSCreateResFile, 1,
PyDoc_STR("(FSRef parentRef, Buffer nameLength) -> (FSRef newRef, FSSpec newSpec)")
},
{
"FSResourceFileAlreadyOpen", (PyCFunction)Res_FSResourceFileAlreadyOpen, 1,
PyDoc_STR("(FSRef resourceFileRef) -> (Boolean _rv, Boolean inChain, SInt16 refNum)")
},
{
"FSCreateResourceFile", (PyCFunction)Res_FSCreateResourceFile, 1,
PyDoc_STR("(FSRef parentRef, Buffer nameLength, Buffer forkNameLength) -> (FSRef newRef, FSSpec newSpec)")
},
#endif
{
"FSOpenResourceFile", (PyCFunction)Res_FSOpenResourceFile, 1,
PyDoc_STR("(FSRef ref, Buffer forkNameLength, SignedByte permissions) -> (SInt16 refNum)")
},
{
"Handle", (PyCFunction)Res_Handle, 1,
PyDoc_STR("Convert a string to a Handle object.\n\nResource() and Handle() are very similar, but objects created with Handle() are\nby default automatically DisposeHandle()d upon object cleanup. Use AutoDispose()\nto change this.\n")
},
{NULL, NULL, 0}
};
PyObject *OptResObj_New(Handle itself) {
if (itself == NULL) {
Py_INCREF(Py_None);
return Py_None;
}
return ResObj_New(itself);
}
int OptResObj_Convert(PyObject *v, Handle *p_itself) {
PyObject *tmp;
if ( v == Py_None ) {
*p_itself = NULL;
return 1;
}
if (ResObj_Check(v)) {
*p_itself = ((ResourceObject *)v)->ob_itself;
return 1;
}
if ( (tmp=PyObject_CallMethod(v, "as_Resource", "")) ) {
*p_itself = ((ResourceObject *)tmp)->ob_itself;
Py_DECREF(tmp);
return 1;
}
PyErr_Clear();
PyErr_SetString(PyExc_TypeError, "Resource required");
return 0;
}
void init_Res(void) {
PyObject *m;
PyObject *d;
PyMac_INIT_TOOLBOX_OBJECT_NEW(Handle, ResObj_New);
PyMac_INIT_TOOLBOX_OBJECT_CONVERT(Handle, ResObj_Convert);
PyMac_INIT_TOOLBOX_OBJECT_NEW(Handle, OptResObj_New);
PyMac_INIT_TOOLBOX_OBJECT_CONVERT(Handle, OptResObj_Convert);
m = Py_InitModule("_Res", Res_methods);
d = PyModule_GetDict(m);
Res_Error = PyMac_GetOSErrException();
if (Res_Error == NULL ||
PyDict_SetItemString(d, "Error", Res_Error) != 0)
return;
Resource_Type.ob_type = &PyType_Type;
if (PyType_Ready(&Resource_Type) < 0) return;
Py_INCREF(&Resource_Type);
PyModule_AddObject(m, "Resource", (PyObject *)&Resource_Type);
Py_INCREF(&Resource_Type);
PyModule_AddObject(m, "ResourceType", (PyObject *)&Resource_Type);
}