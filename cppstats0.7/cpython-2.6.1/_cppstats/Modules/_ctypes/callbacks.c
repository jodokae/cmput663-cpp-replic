#include "Python.h"
#include "compile.h"
#include "frameobject.h"
#include <ffi.h>
#if defined(MS_WIN32)
#include <windows.h>
#endif
#include "ctypes.h"
static void
CThunkObject_dealloc(PyObject *_self) {
CThunkObject *self = (CThunkObject *)_self;
Py_XDECREF(self->converters);
Py_XDECREF(self->callable);
Py_XDECREF(self->restype);
if (self->pcl)
FreeClosure(self->pcl);
PyObject_Del(self);
}
static int
CThunkObject_traverse(PyObject *_self, visitproc visit, void *arg) {
CThunkObject *self = (CThunkObject *)_self;
Py_VISIT(self->converters);
Py_VISIT(self->callable);
Py_VISIT(self->restype);
return 0;
}
static int
CThunkObject_clear(PyObject *_self) {
CThunkObject *self = (CThunkObject *)_self;
Py_CLEAR(self->converters);
Py_CLEAR(self->callable);
Py_CLEAR(self->restype);
return 0;
}
PyTypeObject CThunk_Type = {
PyVarObject_HEAD_INIT(NULL, 0)
"_ctypes.CThunkObject",
sizeof(CThunkObject),
sizeof(ffi_type),
CThunkObject_dealloc,
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
Py_TPFLAGS_DEFAULT,
"CThunkObject",
CThunkObject_traverse,
CThunkObject_clear,
0,
0,
0,
0,
0,
0,
};
static void
PrintError(char *msg, ...) {
char buf[512];
PyObject *f = PySys_GetObject("stderr");
va_list marker;
va_start(marker, msg);
vsnprintf(buf, sizeof(buf), msg, marker);
va_end(marker);
if (f)
PyFile_WriteString(buf, f);
PyErr_Print();
}
void _AddTraceback(char *funcname, char *filename, int lineno) {
PyObject *py_srcfile = 0;
PyObject *py_funcname = 0;
PyObject *py_globals = 0;
PyObject *empty_tuple = 0;
PyObject *empty_string = 0;
PyCodeObject *py_code = 0;
PyFrameObject *py_frame = 0;
py_srcfile = PyString_FromString(filename);
if (!py_srcfile) goto bad;
py_funcname = PyString_FromString(funcname);
if (!py_funcname) goto bad;
py_globals = PyDict_New();
if (!py_globals) goto bad;
empty_tuple = PyTuple_New(0);
if (!empty_tuple) goto bad;
empty_string = PyString_FromString("");
if (!empty_string) goto bad;
py_code = PyCode_New(
0,
0,
0,
0,
empty_string,
empty_tuple,
empty_tuple,
empty_tuple,
empty_tuple,
empty_tuple,
py_srcfile,
py_funcname,
lineno,
empty_string
);
if (!py_code) goto bad;
py_frame = PyFrame_New(
PyThreadState_Get(),
py_code,
py_globals,
0
);
if (!py_frame) goto bad;
py_frame->f_lineno = lineno;
PyTraceBack_Here(py_frame);
bad:
Py_XDECREF(py_globals);
Py_XDECREF(py_srcfile);
Py_XDECREF(py_funcname);
Py_XDECREF(empty_tuple);
Py_XDECREF(empty_string);
Py_XDECREF(py_code);
Py_XDECREF(py_frame);
}
#if defined(MS_WIN32)
static void
TryAddRef(StgDictObject *dict, CDataObject *obj) {
IUnknown *punk;
if (NULL == PyDict_GetItemString((PyObject *)dict, "_needs_com_addref_"))
return;
punk = *(IUnknown **)obj->b_ptr;
if (punk)
punk->lpVtbl->AddRef(punk);
return;
}
#endif
static void _CallPythonObject(void *mem,
ffi_type *restype,
SETFUNC setfunc,
PyObject *callable,
PyObject *converters,
int flags,
void **pArgs) {
Py_ssize_t i;
PyObject *result;
PyObject *arglist = NULL;
Py_ssize_t nArgs;
PyObject *error_object = NULL;
int *space;
#if defined(WITH_THREAD)
PyGILState_STATE state = PyGILState_Ensure();
#endif
nArgs = PySequence_Length(converters);
if (nArgs < 0) {
PrintError("BUG: PySequence_Length");
goto Done;
}
arglist = PyTuple_New(nArgs);
if (!arglist) {
PrintError("PyTuple_New()");
goto Done;
}
for (i = 0; i < nArgs; ++i) {
PyObject *cnv = PySequence_GetItem(converters, i);
StgDictObject *dict;
if (cnv)
dict = PyType_stgdict(cnv);
else {
PrintError("Getting argument converter %d\n", i);
goto Done;
}
if (dict && dict->getfunc && !IsSimpleSubType(cnv)) {
PyObject *v = dict->getfunc(*pArgs, dict->size);
if (!v) {
PrintError("create argument %d:\n", i);
Py_DECREF(cnv);
goto Done;
}
PyTuple_SET_ITEM(arglist, i, v);
} else if (dict) {
CDataObject *obj = (CDataObject *)PyObject_CallFunctionObjArgs(cnv, NULL);
if (!obj) {
PrintError("create argument %d:\n", i);
Py_DECREF(cnv);
goto Done;
}
if (!CDataObject_Check(obj)) {
Py_DECREF(obj);
Py_DECREF(cnv);
PrintError("unexpected result of create argument %d:\n", i);
goto Done;
}
memcpy(obj->b_ptr, *pArgs, dict->size);
PyTuple_SET_ITEM(arglist, i, (PyObject *)obj);
#if defined(MS_WIN32)
TryAddRef(dict, obj);
#endif
} else {
PyErr_SetString(PyExc_TypeError,
"cannot build parameter");
PrintError("Parsing argument %d\n", i);
Py_DECREF(cnv);
goto Done;
}
Py_DECREF(cnv);
pArgs++;
}
#define CHECK(what, x) if (x == NULL) _AddTraceback(what, "_ctypes/callbacks.c", __LINE__ - 1), PyErr_Print()
if (flags & (FUNCFLAG_USE_ERRNO | FUNCFLAG_USE_LASTERROR)) {
error_object = get_error_object(&space);
if (error_object == NULL)
goto Done;
if (flags & FUNCFLAG_USE_ERRNO) {
int temp = space[0];
space[0] = errno;
errno = temp;
}
#if defined(MS_WIN32)
if (flags & FUNCFLAG_USE_LASTERROR) {
int temp = space[1];
space[1] = GetLastError();
SetLastError(temp);
}
#endif
}
result = PyObject_CallObject(callable, arglist);
CHECK("'calling callback function'", result);
#if defined(MS_WIN32)
if (flags & FUNCFLAG_USE_LASTERROR) {
int temp = space[1];
space[1] = GetLastError();
SetLastError(temp);
}
#endif
if (flags & FUNCFLAG_USE_ERRNO) {
int temp = space[0];
space[0] = errno;
errno = temp;
}
Py_XDECREF(error_object);
if ((restype != &ffi_type_void) && result) {
PyObject *keep;
assert(setfunc);
#if defined(WORDS_BIGENDIAN)
if (restype->type != FFI_TYPE_FLOAT && restype->size < sizeof(ffi_arg))
mem = (char *)mem + sizeof(ffi_arg) - restype->size;
#endif
keep = setfunc(mem, result, 0);
CHECK("'converting callback result'", keep);
if (keep == NULL)
PyErr_WriteUnraisable(callable);
else if (keep == Py_None)
Py_DECREF(keep);
else if (setfunc != getentry("O")->setfunc) {
if (-1 == PyErr_Warn(PyExc_RuntimeWarning,
"memory leak in callback function."))
PyErr_WriteUnraisable(callable);
}
}
Py_XDECREF(result);
Done:
Py_XDECREF(arglist);
#if defined(WITH_THREAD)
PyGILState_Release(state);
#endif
}
static void closure_fcn(ffi_cif *cif,
void *resp,
void **args,
void *userdata) {
CThunkObject *p = (CThunkObject *)userdata;
_CallPythonObject(resp,
p->ffi_restype,
p->setfunc,
p->callable,
p->converters,
p->flags,
args);
}
static CThunkObject* CThunkObject_new(Py_ssize_t nArgs) {
CThunkObject *p;
int i;
p = PyObject_NewVar(CThunkObject, &CThunk_Type, nArgs);
if (p == NULL) {
PyErr_NoMemory();
return NULL;
}
p->pcl = NULL;
memset(&p->cif, 0, sizeof(p->cif));
p->converters = NULL;
p->callable = NULL;
p->setfunc = NULL;
p->ffi_restype = NULL;
for (i = 0; i < nArgs + 1; ++i)
p->atypes[i] = NULL;
return p;
}
CThunkObject *AllocFunctionCallback(PyObject *callable,
PyObject *converters,
PyObject *restype,
int flags) {
int result;
CThunkObject *p;
Py_ssize_t nArgs, i;
ffi_abi cc;
nArgs = PySequence_Size(converters);
p = CThunkObject_new(nArgs);
if (p == NULL)
return NULL;
assert(CThunk_CheckExact(p));
p->pcl = MallocClosure();
if (p->pcl == NULL) {
PyErr_NoMemory();
goto error;
}
p->flags = flags;
for (i = 0; i < nArgs; ++i) {
PyObject *cnv = PySequence_GetItem(converters, i);
if (cnv == NULL)
goto error;
p->atypes[i] = GetType(cnv);
Py_DECREF(cnv);
}
p->atypes[i] = NULL;
Py_INCREF(restype);
p->restype = restype;
if (restype == Py_None) {
p->setfunc = NULL;
p->ffi_restype = &ffi_type_void;
} else {
StgDictObject *dict = PyType_stgdict(restype);
if (dict == NULL || dict->setfunc == NULL) {
PyErr_SetString(PyExc_TypeError,
"invalid result type for callback function");
goto error;
}
p->setfunc = dict->setfunc;
p->ffi_restype = &dict->ffi_type_pointer;
}
cc = FFI_DEFAULT_ABI;
#if defined(MS_WIN32) && !defined(_WIN32_WCE) && !defined(MS_WIN64)
if ((flags & FUNCFLAG_CDECL) == 0)
cc = FFI_STDCALL;
#endif
result = ffi_prep_cif(&p->cif, cc,
Py_SAFE_DOWNCAST(nArgs, Py_ssize_t, int),
GetType(restype),
&p->atypes[0]);
if (result != FFI_OK) {
PyErr_Format(PyExc_RuntimeError,
"ffi_prep_cif failed with %d", result);
goto error;
}
result = ffi_prep_closure(p->pcl, &p->cif, closure_fcn, p);
if (result != FFI_OK) {
PyErr_Format(PyExc_RuntimeError,
"ffi_prep_closure failed with %d", result);
goto error;
}
Py_INCREF(converters);
p->converters = converters;
Py_INCREF(callable);
p->callable = callable;
return p;
error:
Py_XDECREF(p);
return NULL;
}
void init_callbacks_in_module(PyObject *m) {
if (PyType_Ready((PyTypeObject *)&PyType_Type) < 0)
return;
}
#if defined(MS_WIN32)
static void LoadPython(void) {
if (!Py_IsInitialized()) {
#if defined(WITH_THREAD)
PyEval_InitThreads();
#endif
Py_Initialize();
}
}
long Call_GetClassObject(REFCLSID rclsid, REFIID riid, LPVOID *ppv) {
PyObject *mod, *func, *result;
long retval;
static PyObject *context;
if (context == NULL)
context = PyString_InternFromString("_ctypes.DllGetClassObject");
mod = PyImport_ImportModuleNoBlock("ctypes");
if (!mod) {
PyErr_WriteUnraisable(context ? context : Py_None);
return E_FAIL;
}
func = PyObject_GetAttrString(mod, "DllGetClassObject");
Py_DECREF(mod);
if (!func) {
PyErr_WriteUnraisable(context ? context : Py_None);
return E_FAIL;
}
{
PyObject *py_rclsid = PyLong_FromVoidPtr((void *)rclsid);
PyObject *py_riid = PyLong_FromVoidPtr((void *)riid);
PyObject *py_ppv = PyLong_FromVoidPtr(ppv);
if (!py_rclsid || !py_riid || !py_ppv) {
Py_XDECREF(py_rclsid);
Py_XDECREF(py_riid);
Py_XDECREF(py_ppv);
Py_DECREF(func);
PyErr_WriteUnraisable(context ? context : Py_None);
return E_FAIL;
}
result = PyObject_CallFunctionObjArgs(func,
py_rclsid,
py_riid,
py_ppv,
NULL);
Py_DECREF(py_rclsid);
Py_DECREF(py_riid);
Py_DECREF(py_ppv);
}
Py_DECREF(func);
if (!result) {
PyErr_WriteUnraisable(context ? context : Py_None);
return E_FAIL;
}
retval = PyInt_AsLong(result);
if (PyErr_Occurred()) {
PyErr_WriteUnraisable(context ? context : Py_None);
retval = E_FAIL;
}
Py_DECREF(result);
return retval;
}
STDAPI DllGetClassObject(REFCLSID rclsid,
REFIID riid,
LPVOID *ppv) {
long result;
#if defined(WITH_THREAD)
PyGILState_STATE state;
#endif
LoadPython();
#if defined(WITH_THREAD)
state = PyGILState_Ensure();
#endif
result = Call_GetClassObject(rclsid, riid, ppv);
#if defined(WITH_THREAD)
PyGILState_Release(state);
#endif
return result;
}
long Call_CanUnloadNow(void) {
PyObject *mod, *func, *result;
long retval;
static PyObject *context;
if (context == NULL)
context = PyString_InternFromString("_ctypes.DllCanUnloadNow");
mod = PyImport_ImportModuleNoBlock("ctypes");
if (!mod) {
PyErr_Clear();
return E_FAIL;
}
func = PyObject_GetAttrString(mod, "DllCanUnloadNow");
Py_DECREF(mod);
if (!func) {
PyErr_WriteUnraisable(context ? context : Py_None);
return E_FAIL;
}
result = PyObject_CallFunction(func, NULL);
Py_DECREF(func);
if (!result) {
PyErr_WriteUnraisable(context ? context : Py_None);
return E_FAIL;
}
retval = PyInt_AsLong(result);
if (PyErr_Occurred()) {
PyErr_WriteUnraisable(context ? context : Py_None);
retval = E_FAIL;
}
Py_DECREF(result);
return retval;
}
STDAPI DllCanUnloadNow(void) {
long result;
#if defined(WITH_THREAD)
PyGILState_STATE state = PyGILState_Ensure();
#endif
result = Call_CanUnloadNow();
#if defined(WITH_THREAD)
PyGILState_Release(state);
#endif
return result;
}
#if !defined(Py_NO_ENABLE_SHARED)
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvRes) {
switch(fdwReason) {
case DLL_PROCESS_ATTACH:
DisableThreadLibraryCalls(hinstDLL);
break;
}
return TRUE;
}
#endif
#endif