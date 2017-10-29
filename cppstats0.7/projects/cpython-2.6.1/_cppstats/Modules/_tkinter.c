#include "Python.h"
#include <ctype.h>
#if defined(WITH_THREAD)
#include "pythread.h"
#endif
#if defined(MS_WINDOWS)
#include <windows.h>
#endif
#if !defined(PyDoc_STRVAR)
#define PyDoc_STRVAR(name,str) static char name[] = str
#endif
#if !defined(PyMODINIT_FUNC)
#define PyMODINIT_FUNC void
#endif
#if !defined(PyBool_Check)
#define PyBool_Check(o) 0
#define PyBool_FromLong PyInt_FromLong
#endif
#define USE_COMPAT_CONST
#define TCL_THREADS
#if defined(TK_FRAMEWORK)
#include <Tcl/tcl.h>
#include <Tk/tk.h>
#else
#include <tcl.h>
#include <tk.h>
#endif
#if !defined(CONST84_RETURN)
#define CONST84_RETURN
#undef CONST
#define CONST
#endif
#define TKMAJORMINOR (TK_MAJOR_VERSION*1000 + TK_MINOR_VERSION)
#if TKMAJORMINOR < 8002
#error "Tk older than 8.2 not supported"
#endif
#if TCL_UTF_MAX != 3 && !(defined(Py_UNICODE_WIDE) && TCL_UTF_MAX==6)
#error "unsupported Tcl configuration"
#endif
#if !(defined(MS_WINDOWS) || defined(__CYGWIN__))
#define HAVE_CREATEFILEHANDLER
#endif
#if defined(HAVE_CREATEFILEHANDLER)
#if !defined(TCL_UNIX_FD)
#if defined(TCL_WIN_SOCKET)
#define TCL_UNIX_FD (! TCL_WIN_SOCKET)
#else
#define TCL_UNIX_FD 1
#endif
#endif
#if defined(MS_WINDOWS)
#define FHANDLETYPE TCL_WIN_SOCKET
#else
#define FHANDLETYPE TCL_UNIX_FD
#endif
#if FHANDLETYPE == TCL_UNIX_FD
#define WAIT_FOR_STDIN
#endif
#endif
#if defined(MS_WINDOWS)
#include <conio.h>
#define WAIT_FOR_STDIN
#endif
#if defined(WITH_THREAD)
static PyThread_type_lock tcl_lock = 0;
#if defined(TCL_THREADS)
static Tcl_ThreadDataKey state_key;
typedef PyThreadState *ThreadSpecificData;
#define tcl_tstate (*(PyThreadState**)Tcl_GetThreadData(&state_key, sizeof(PyThreadState*)))
#else
static PyThreadState *tcl_tstate = NULL;
#endif
#define ENTER_TCL { PyThreadState *tstate = PyThreadState_Get(); Py_BEGIN_ALLOW_THREADS if(tcl_lock)PyThread_acquire_lock(tcl_lock, 1); tcl_tstate = tstate;
#define LEAVE_TCL tcl_tstate = NULL; if(tcl_lock)PyThread_release_lock(tcl_lock); Py_END_ALLOW_THREADS}
#define ENTER_OVERLAP Py_END_ALLOW_THREADS
#define LEAVE_OVERLAP_TCL tcl_tstate = NULL; if(tcl_lock)PyThread_release_lock(tcl_lock); }
#define ENTER_PYTHON { PyThreadState *tstate = tcl_tstate; tcl_tstate = NULL; if(tcl_lock)PyThread_release_lock(tcl_lock); PyEval_RestoreThread((tstate)); }
#define LEAVE_PYTHON { PyThreadState *tstate = PyEval_SaveThread(); if(tcl_lock)PyThread_acquire_lock(tcl_lock, 1); tcl_tstate = tstate; }
#define CHECK_TCL_APPARTMENT if (((TkappObject *)self)->threaded && ((TkappObject *)self)->thread_id != Tcl_GetCurrentThread()) { PyErr_SetString(PyExc_RuntimeError, "Calling Tcl from different appartment"); return 0; }
#else
#define ENTER_TCL
#define LEAVE_TCL
#define ENTER_OVERLAP
#define LEAVE_OVERLAP_TCL
#define ENTER_PYTHON
#define LEAVE_PYTHON
#define CHECK_TCL_APPARTMENT
#endif
#if !defined(FREECAST)
#define FREECAST (char *)
#endif
static PyTypeObject Tkapp_Type;
typedef struct {
PyObject_HEAD
Tcl_Interp *interp;
int wantobjects;
int threaded;
Tcl_ThreadId thread_id;
int dispatching;
Tcl_ObjType *BooleanType;
Tcl_ObjType *ByteArrayType;
Tcl_ObjType *DoubleType;
Tcl_ObjType *IntType;
Tcl_ObjType *ListType;
Tcl_ObjType *ProcBodyType;
Tcl_ObjType *StringType;
} TkappObject;
#define Tkapp_Check(v) (Py_TYPE(v) == &Tkapp_Type)
#define Tkapp_Interp(v) (((TkappObject *) (v))->interp)
#define Tkapp_Result(v) Tcl_GetStringResult(Tkapp_Interp(v))
#define DEBUG_REFCNT(v) (printf("DEBUG: id=%p, refcnt=%i\n", (void *) v, Py_REFCNT(v)))
static PyObject *Tkinter_TclError;
static int quitMainLoop = 0;
static int errorInCmd = 0;
static PyObject *excInCmd;
static PyObject *valInCmd;
static PyObject *trbInCmd;
static PyObject *
Tkinter_Error(PyObject *v) {
PyErr_SetString(Tkinter_TclError, Tkapp_Result(v));
return NULL;
}
static int Tkinter_busywaitinterval = 20;
#if defined(WITH_THREAD)
#if !defined(MS_WINDOWS)
static void
Sleep(int milli) {
struct timeval t;
t.tv_sec = milli/1000;
t.tv_usec = (milli%1000) * 1000;
select(0, (fd_set *)0, (fd_set *)0, (fd_set *)0, &t);
}
#endif
static int
WaitForMainloop(TkappObject* self) {
int i;
for (i = 0; i < 10; i++) {
if (self->dispatching)
return 1;
Py_BEGIN_ALLOW_THREADS
Sleep(100);
Py_END_ALLOW_THREADS
}
if (self->dispatching)
return 1;
PyErr_SetString(PyExc_RuntimeError, "main thread is not in main loop");
return 0;
}
#endif
static char *
AsString(PyObject *value, PyObject *tmp) {
if (PyString_Check(value))
return PyString_AsString(value);
#if defined(Py_USING_UNICODE)
else if (PyUnicode_Check(value)) {
PyObject *v = PyUnicode_AsUTF8String(value);
if (v == NULL)
return NULL;
if (PyList_Append(tmp, v) != 0) {
Py_DECREF(v);
return NULL;
}
Py_DECREF(v);
return PyString_AsString(v);
}
#endif
else {
PyObject *v = PyObject_Str(value);
if (v == NULL)
return NULL;
if (PyList_Append(tmp, v) != 0) {
Py_DECREF(v);
return NULL;
}
Py_DECREF(v);
return PyString_AsString(v);
}
}
#define ARGSZ 64
static char *
Merge(PyObject *args) {
PyObject *tmp = NULL;
char *argvStore[ARGSZ];
char **argv = NULL;
int fvStore[ARGSZ];
int *fv = NULL;
int argc = 0, fvc = 0, i;
char *res = NULL;
if (!(tmp = PyList_New(0)))
return NULL;
argv = argvStore;
fv = fvStore;
if (args == NULL)
argc = 0;
else if (!PyTuple_Check(args)) {
argc = 1;
fv[0] = 0;
if (!(argv[0] = AsString(args, tmp)))
goto finally;
} else {
argc = PyTuple_Size(args);
if (argc > ARGSZ) {
argv = (char **)ckalloc(argc * sizeof(char *));
fv = (int *)ckalloc(argc * sizeof(int));
if (argv == NULL || fv == NULL) {
PyErr_NoMemory();
goto finally;
}
}
for (i = 0; i < argc; i++) {
PyObject *v = PyTuple_GetItem(args, i);
if (PyTuple_Check(v)) {
fv[i] = 1;
if (!(argv[i] = Merge(v)))
goto finally;
fvc++;
} else if (v == Py_None) {
argc = i;
break;
} else {
fv[i] = 0;
if (!(argv[i] = AsString(v, tmp)))
goto finally;
fvc++;
}
}
}
res = Tcl_Merge(argc, argv);
if (res == NULL)
PyErr_SetString(Tkinter_TclError, "merge failed");
finally:
for (i = 0; i < fvc; i++)
if (fv[i]) {
ckfree(argv[i]);
}
if (argv != argvStore)
ckfree(FREECAST argv);
if (fv != fvStore)
ckfree(FREECAST fv);
Py_DECREF(tmp);
return res;
}
static PyObject *
Split(char *list) {
int argc;
char **argv;
PyObject *v;
if (list == NULL) {
Py_INCREF(Py_None);
return Py_None;
}
if (Tcl_SplitList((Tcl_Interp *)NULL, list, &argc, &argv) != TCL_OK) {
return PyString_FromString(list);
}
if (argc == 0)
v = PyString_FromString("");
else if (argc == 1)
v = PyString_FromString(argv[0]);
else if ((v = PyTuple_New(argc)) != NULL) {
int i;
PyObject *w;
for (i = 0; i < argc; i++) {
if ((w = Split(argv[i])) == NULL) {
Py_DECREF(v);
v = NULL;
break;
}
PyTuple_SetItem(v, i, w);
}
}
Tcl_Free(FREECAST argv);
return v;
}
static PyObject *
SplitObj(PyObject *arg) {
if (PyTuple_Check(arg)) {
int i, size;
PyObject *elem, *newelem, *result;
size = PyTuple_Size(arg);
result = NULL;
for(i = 0; i < size; i++) {
elem = PyTuple_GetItem(arg, i);
newelem = SplitObj(elem);
if (!newelem) {
Py_XDECREF(result);
return NULL;
}
if (!result) {
int k;
if (newelem == elem) {
Py_DECREF(newelem);
continue;
}
result = PyTuple_New(size);
if (!result)
return NULL;
for(k = 0; k < i; k++) {
elem = PyTuple_GetItem(arg, k);
Py_INCREF(elem);
PyTuple_SetItem(result, k, elem);
}
}
PyTuple_SetItem(result, i, newelem);
}
if (result)
return result;
} else if (PyString_Check(arg)) {
int argc;
char **argv;
char *list = PyString_AsString(arg);
if (Tcl_SplitList((Tcl_Interp *)NULL, list, &argc, &argv) != TCL_OK) {
Py_INCREF(arg);
return arg;
}
Tcl_Free(FREECAST argv);
if (argc > 1)
return Split(PyString_AsString(arg));
}
Py_INCREF(arg);
return arg;
}
#if !defined(WITH_APPINIT)
int
Tcl_AppInit(Tcl_Interp *interp) {
Tk_Window main;
const char * _tkinter_skip_tk_init;
if (Tcl_Init(interp) == TCL_ERROR) {
PySys_WriteStderr("Tcl_Init error: %s\n", Tcl_GetStringResult(interp));
return TCL_ERROR;
}
_tkinter_skip_tk_init = Tcl_GetVar(interp, "_tkinter_skip_tk_init", TCL_GLOBAL_ONLY);
if (_tkinter_skip_tk_init == NULL || strcmp(_tkinter_skip_tk_init, "1") != 0) {
main = Tk_MainWindow(interp);
if (Tk_Init(interp) == TCL_ERROR) {
PySys_WriteStderr("Tk_Init error: %s\n", Tcl_GetStringResult(interp));
return TCL_ERROR;
}
}
return TCL_OK;
}
#endif
static void EnableEventHook(void);
static void DisableEventHook(void);
static TkappObject *
Tkapp_New(char *screenName, char *baseName, char *className,
int interactive, int wantobjects, int wantTk, int sync, char *use) {
TkappObject *v;
char *argv0;
v = PyObject_New(TkappObject, &Tkapp_Type);
if (v == NULL)
return NULL;
v->interp = Tcl_CreateInterp();
v->wantobjects = wantobjects;
v->threaded = Tcl_GetVar2Ex(v->interp, "tcl_platform", "threaded",
TCL_GLOBAL_ONLY) != NULL;
v->thread_id = Tcl_GetCurrentThread();
v->dispatching = 0;
#if !defined(TCL_THREADS)
if (v->threaded) {
PyErr_SetString(PyExc_RuntimeError, "Tcl is threaded but _tkinter is not");
Py_DECREF(v);
return 0;
}
#endif
#if defined(WITH_THREAD)
if (v->threaded && tcl_lock) {
PyThread_free_lock(tcl_lock);
tcl_lock = NULL;
}
#endif
v->BooleanType = Tcl_GetObjType("boolean");
v->ByteArrayType = Tcl_GetObjType("bytearray");
v->DoubleType = Tcl_GetObjType("double");
v->IntType = Tcl_GetObjType("int");
v->ListType = Tcl_GetObjType("list");
v->ProcBodyType = Tcl_GetObjType("procbody");
v->StringType = Tcl_GetObjType("string");
Tcl_DeleteCommand(v->interp, "exit");
if (screenName != NULL)
Tcl_SetVar2(v->interp, "env", "DISPLAY",
screenName, TCL_GLOBAL_ONLY);
if (interactive)
Tcl_SetVar(v->interp, "tcl_interactive", "1", TCL_GLOBAL_ONLY);
else
Tcl_SetVar(v->interp, "tcl_interactive", "0", TCL_GLOBAL_ONLY);
argv0 = (char*)ckalloc(strlen(className) + 1);
if (!argv0) {
PyErr_NoMemory();
Py_DECREF(v);
return NULL;
}
strcpy(argv0, className);
if (isupper(Py_CHARMASK(argv0[0])))
argv0[0] = tolower(Py_CHARMASK(argv0[0]));
Tcl_SetVar(v->interp, "argv0", argv0, TCL_GLOBAL_ONLY);
ckfree(argv0);
if (! wantTk) {
Tcl_SetVar(v->interp, "_tkinter_skip_tk_init", "1", TCL_GLOBAL_ONLY);
}
if (sync || use) {
char *args;
int len = 0;
if (sync)
len += sizeof "-sync";
if (use)
len += strlen(use) + sizeof "-use ";
args = (char*)ckalloc(len);
if (!args) {
PyErr_NoMemory();
Py_DECREF(v);
return NULL;
}
args[0] = '\0';
if (sync)
strcat(args, "-sync");
if (use) {
if (sync)
strcat(args, " ");
strcat(args, "-use ");
strcat(args, use);
}
Tcl_SetVar(v->interp, "argv", args, TCL_GLOBAL_ONLY);
ckfree(args);
}
if (Tcl_AppInit(v->interp) != TCL_OK) {
PyObject *result = Tkinter_Error((PyObject *)v);
Py_DECREF((PyObject *)v);
return (TkappObject *)result;
}
EnableEventHook();
return v;
}
static void
Tkapp_ThreadSend(TkappObject *self, Tcl_Event *ev,
Tcl_Condition *cond, Tcl_Mutex *mutex) {
Py_BEGIN_ALLOW_THREADS;
Tcl_MutexLock(mutex);
Tcl_ThreadQueueEvent(self->thread_id, ev, TCL_QUEUE_TAIL);
Tcl_ThreadAlert(self->thread_id);
Tcl_ConditionWait(cond, mutex, NULL);
Tcl_MutexUnlock(mutex);
Py_END_ALLOW_THREADS
}
typedef struct {
PyObject_HEAD
Tcl_Obj *value;
PyObject *string;
} PyTclObject;
staticforward PyTypeObject PyTclObject_Type;
#define PyTclObject_Check(v) ((v)->ob_type == &PyTclObject_Type)
static PyObject *
newPyTclObject(Tcl_Obj *arg) {
PyTclObject *self;
self = PyObject_New(PyTclObject, &PyTclObject_Type);
if (self == NULL)
return NULL;
Tcl_IncrRefCount(arg);
self->value = arg;
self->string = NULL;
return (PyObject*)self;
}
static void
PyTclObject_dealloc(PyTclObject *self) {
Tcl_DecrRefCount(self->value);
Py_XDECREF(self->string);
PyObject_Del(self);
}
static PyObject *
PyTclObject_str(PyTclObject *self) {
if (self->string && PyString_Check(self->string)) {
Py_INCREF(self->string);
return self->string;
}
return PyString_FromString(Tcl_GetString(self->value));
}
static char*
PyTclObject_TclString(PyObject *self) {
return Tcl_GetString(((PyTclObject*)self)->value);
}
PyDoc_STRVAR(PyTclObject_string__doc__,
"the string representation of this object, either as string or Unicode");
static PyObject *
PyTclObject_string(PyTclObject *self, void *ignored) {
char *s;
int i, len;
if (!self->string) {
s = Tcl_GetStringFromObj(self->value, &len);
for (i = 0; i < len; i++)
if (s[i] & 0x80)
break;
#if defined(Py_USING_UNICODE)
if (i == len)
self->string = PyString_FromStringAndSize(s, len);
else {
self->string = PyUnicode_DecodeUTF8(s, len, "strict");
if (!self->string) {
PyErr_Clear();
self->string = PyString_FromStringAndSize(s, len);
}
}
#else
self->string = PyString_FromStringAndSize(s, len);
#endif
if (!self->string)
return NULL;
}
Py_INCREF(self->string);
return self->string;
}
#if defined(Py_USING_UNICODE)
PyDoc_STRVAR(PyTclObject_unicode__doc__, "convert argument to unicode");
static PyObject *
PyTclObject_unicode(PyTclObject *self, void *ignored) {
char *s;
int len;
if (self->string && PyUnicode_Check(self->string)) {
Py_INCREF(self->string);
return self->string;
}
s = Tcl_GetStringFromObj(self->value, &len);
return PyUnicode_DecodeUTF8(s, len, "strict");
}
#endif
static PyObject *
PyTclObject_repr(PyTclObject *self) {
char buf[50];
PyOS_snprintf(buf, 50, "<%s object at %p>",
self->value->typePtr->name, self->value);
return PyString_FromString(buf);
}
static int
PyTclObject_cmp(PyTclObject *self, PyTclObject *other) {
int res;
res = strcmp(Tcl_GetString(self->value),
Tcl_GetString(other->value));
if (res < 0) return -1;
if (res > 0) return 1;
return 0;
}
PyDoc_STRVAR(get_typename__doc__, "name of the Tcl type");
static PyObject*
get_typename(PyTclObject* obj, void* ignored) {
return PyString_FromString(obj->value->typePtr->name);
}
static PyGetSetDef PyTclObject_getsetlist[] = {
{"typename", (getter)get_typename, NULL, get_typename__doc__},
{
"string", (getter)PyTclObject_string, NULL,
PyTclObject_string__doc__
},
{0},
};
static PyMethodDef PyTclObject_methods[] = {
#if defined(Py_USING_UNICODE)
{
"__unicode__", (PyCFunction)PyTclObject_unicode, METH_NOARGS,
PyTclObject_unicode__doc__
},
#endif
{0}
};
statichere PyTypeObject PyTclObject_Type = {
PyObject_HEAD_INIT(NULL)
0,
"_tkinter.Tcl_Obj",
sizeof(PyTclObject),
0,
(destructor)PyTclObject_dealloc,
0,
0,
0,
(cmpfunc)PyTclObject_cmp,
(reprfunc)PyTclObject_repr,
0,
0,
0,
0,
0,
(reprfunc)PyTclObject_str,
PyObject_GenericGetAttr,
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
PyTclObject_methods,
0,
PyTclObject_getsetlist,
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
static Tcl_Obj*
AsObj(PyObject *value) {
Tcl_Obj *result;
if (PyString_Check(value))
return Tcl_NewStringObj(PyString_AS_STRING(value),
PyString_GET_SIZE(value));
else if (PyBool_Check(value))
return Tcl_NewBooleanObj(PyObject_IsTrue(value));
else if (PyInt_Check(value))
return Tcl_NewLongObj(PyInt_AS_LONG(value));
else if (PyFloat_Check(value))
return Tcl_NewDoubleObj(PyFloat_AS_DOUBLE(value));
else if (PyTuple_Check(value)) {
Tcl_Obj **argv = (Tcl_Obj**)
ckalloc(PyTuple_Size(value)*sizeof(Tcl_Obj*));
int i;
if(!argv)
return 0;
for(i=0; i<PyTuple_Size(value); i++)
argv[i] = AsObj(PyTuple_GetItem(value,i));
result = Tcl_NewListObj(PyTuple_Size(value), argv);
ckfree(FREECAST argv);
return result;
}
#if defined(Py_USING_UNICODE)
else if (PyUnicode_Check(value)) {
Py_UNICODE *inbuf = PyUnicode_AS_UNICODE(value);
Py_ssize_t size = PyUnicode_GET_SIZE(value);
#if defined(Py_UNICODE_WIDE) && TCL_UTF_MAX == 3
Tcl_UniChar *outbuf = NULL;
Py_ssize_t i;
size_t allocsize = ((size_t)size) * sizeof(Tcl_UniChar);
if (allocsize >= size)
outbuf = (Tcl_UniChar*)ckalloc(allocsize);
if (!outbuf) {
PyErr_NoMemory();
return NULL;
}
for (i = 0; i < size; i++) {
if (inbuf[i] >= 0x10000) {
PyErr_SetString(PyExc_ValueError,
"unsupported character");
ckfree(FREECAST outbuf);
return NULL;
}
outbuf[i] = inbuf[i];
}
result = Tcl_NewUnicodeObj(outbuf, size);
ckfree(FREECAST outbuf);
return result;
#else
return Tcl_NewUnicodeObj(inbuf, size);
#endif
}
#endif
else if(PyTclObject_Check(value)) {
Tcl_Obj *v = ((PyTclObject*)value)->value;
Tcl_IncrRefCount(v);
return v;
} else {
PyObject *v = PyObject_Str(value);
if (!v)
return 0;
result = AsObj(v);
Py_DECREF(v);
return result;
}
}
static PyObject*
FromObj(PyObject* tkapp, Tcl_Obj *value) {
PyObject *result = NULL;
TkappObject *app = (TkappObject*)tkapp;
if (value->typePtr == NULL) {
#if defined(Py_USING_UNICODE)
int i;
char *s = value->bytes;
int len = value->length;
for (i = 0; i < len; i++) {
if (value->bytes[i] & 0x80)
break;
}
if (i == value->length)
result = PyString_FromStringAndSize(s, len);
else {
result = PyUnicode_DecodeUTF8(s, len, "strict");
if (result == NULL) {
PyErr_Clear();
result = PyString_FromStringAndSize(s, len);
}
}
#else
result = PyString_FromStringAndSize(value->bytes, value->length);
#endif
return result;
}
if (value->typePtr == app->BooleanType) {
result = value->internalRep.longValue ? Py_True : Py_False;
Py_INCREF(result);
return result;
}
if (value->typePtr == app->ByteArrayType) {
int size;
char *data = (char*)Tcl_GetByteArrayFromObj(value, &size);
return PyString_FromStringAndSize(data, size);
}
if (value->typePtr == app->DoubleType) {
return PyFloat_FromDouble(value->internalRep.doubleValue);
}
if (value->typePtr == app->IntType) {
return PyInt_FromLong(value->internalRep.longValue);
}
if (value->typePtr == app->ListType) {
int size;
int i, status;
PyObject *elem;
Tcl_Obj *tcl_elem;
status = Tcl_ListObjLength(Tkapp_Interp(tkapp), value, &size);
if (status == TCL_ERROR)
return Tkinter_Error(tkapp);
result = PyTuple_New(size);
if (!result)
return NULL;
for (i = 0; i < size; i++) {
status = Tcl_ListObjIndex(Tkapp_Interp(tkapp),
value, i, &tcl_elem);
if (status == TCL_ERROR) {
Py_DECREF(result);
return Tkinter_Error(tkapp);
}
elem = FromObj(tkapp, tcl_elem);
if (!elem) {
Py_DECREF(result);
return NULL;
}
PyTuple_SetItem(result, i, elem);
}
return result;
}
if (value->typePtr == app->ProcBodyType) {
}
if (value->typePtr == app->StringType) {
#if defined(Py_USING_UNICODE)
#if defined(Py_UNICODE_WIDE) && TCL_UTF_MAX==3
PyObject *result;
int size;
Tcl_UniChar *input;
Py_UNICODE *output;
size = Tcl_GetCharLength(value);
result = PyUnicode_FromUnicode(NULL, size);
if (!result)
return NULL;
input = Tcl_GetUnicode(value);
output = PyUnicode_AS_UNICODE(result);
while (size--)
*output++ = *input++;
return result;
#else
return PyUnicode_FromUnicode(Tcl_GetUnicode(value),
Tcl_GetCharLength(value));
#endif
#else
int size;
char *c;
c = Tcl_GetStringFromObj(value, &size);
return PyString_FromStringAndSize(c, size);
#endif
}
return newPyTclObject(value);
}
TCL_DECLARE_MUTEX(call_mutex)
typedef struct Tkapp_CallEvent {
Tcl_Event ev;
TkappObject *self;
PyObject *args;
int flags;
PyObject **res;
PyObject **exc_type, **exc_value, **exc_tb;
Tcl_Condition done;
} Tkapp_CallEvent;
void
Tkapp_CallDeallocArgs(Tcl_Obj** objv, Tcl_Obj** objStore, int objc) {
int i;
for (i = 0; i < objc; i++)
Tcl_DecrRefCount(objv[i]);
if (objv != objStore)
ckfree(FREECAST objv);
}
static Tcl_Obj**
Tkapp_CallArgs(PyObject *args, Tcl_Obj** objStore, int *pobjc) {
Tcl_Obj **objv = objStore;
int objc = 0, i;
if (args == NULL)
;
else if (!PyTuple_Check(args)) {
objv[0] = AsObj(args);
if (objv[0] == 0)
goto finally;
objc = 1;
Tcl_IncrRefCount(objv[0]);
} else {
objc = PyTuple_Size(args);
if (objc > ARGSZ) {
objv = (Tcl_Obj **)ckalloc(objc * sizeof(char *));
if (objv == NULL) {
PyErr_NoMemory();
objc = 0;
goto finally;
}
}
for (i = 0; i < objc; i++) {
PyObject *v = PyTuple_GetItem(args, i);
if (v == Py_None) {
objc = i;
break;
}
objv[i] = AsObj(v);
if (!objv[i]) {
objc = i;
goto finally;
}
Tcl_IncrRefCount(objv[i]);
}
}
*pobjc = objc;
return objv;
finally:
Tkapp_CallDeallocArgs(objv, objStore, objc);
return NULL;
}
static PyObject*
Tkapp_CallResult(TkappObject *self) {
PyObject *res = NULL;
if(self->wantobjects) {
Tcl_Obj *value = Tcl_GetObjResult(self->interp);
Tcl_IncrRefCount(value);
res = FromObj((PyObject*)self, value);
Tcl_DecrRefCount(value);
} else {
const char *s = Tcl_GetStringResult(self->interp);
const char *p = s;
#if defined(Py_USING_UNICODE)
while (*p != '\0') {
if (*p & 0x80)
break;
p++;
}
if (*p == '\0')
res = PyString_FromStringAndSize(s, (int)(p-s));
else {
p = strchr(p, '\0');
res = PyUnicode_DecodeUTF8(s, (int)(p-s), "strict");
if (res == NULL) {
PyErr_Clear();
res = PyString_FromStringAndSize(s, (int)(p-s));
}
}
#else
p = strchr(p, '\0');
res = PyString_FromStringAndSize(s, (int)(p-s));
#endif
}
return res;
}
static int
Tkapp_CallProc(Tkapp_CallEvent *e, int flags) {
Tcl_Obj *objStore[ARGSZ];
Tcl_Obj **objv;
int objc;
int i;
ENTER_PYTHON
objv = Tkapp_CallArgs(e->args, objStore, &objc);
if (!objv) {
PyErr_Fetch(e->exc_type, e->exc_value, e->exc_tb);
*(e->res) = NULL;
}
LEAVE_PYTHON
if (!objv)
goto done;
i = Tcl_EvalObjv(e->self->interp, objc, objv, e->flags);
ENTER_PYTHON
if (i == TCL_ERROR) {
*(e->res) = NULL;
*(e->exc_type) = NULL;
*(e->exc_tb) = NULL;
*(e->exc_value) = PyObject_CallFunction(
Tkinter_TclError, "s",
Tcl_GetStringResult(e->self->interp));
} else {
*(e->res) = Tkapp_CallResult(e->self);
}
LEAVE_PYTHON
done:
Tcl_MutexLock(&call_mutex);
Tcl_ConditionNotify(&e->done);
Tcl_MutexUnlock(&call_mutex);
return 1;
}
static PyObject *
Tkapp_Call(PyObject *selfptr, PyObject *args) {
Tcl_Obj *objStore[ARGSZ];
Tcl_Obj **objv = NULL;
int objc, i;
PyObject *res = NULL;
TkappObject *self = (TkappObject*)selfptr;
int flags = TCL_EVAL_DIRECT;
if (1 == PyTuple_Size(args)) {
PyObject* item = PyTuple_GetItem(args, 0);
if (PyTuple_Check(item))
args = item;
}
#if defined(WITH_THREAD)
if (self->threaded && self->thread_id != Tcl_GetCurrentThread()) {
Tkapp_CallEvent *ev;
PyObject *exc_type, *exc_value, *exc_tb;
if (!WaitForMainloop(self))
return NULL;
ev = (Tkapp_CallEvent*)ckalloc(sizeof(Tkapp_CallEvent));
ev->ev.proc = (Tcl_EventProc*)Tkapp_CallProc;
ev->self = self;
ev->args = args;
ev->res = &res;
ev->exc_type = &exc_type;
ev->exc_value = &exc_value;
ev->exc_tb = &exc_tb;
ev->done = (Tcl_Condition)0;
Tkapp_ThreadSend(self, (Tcl_Event*)ev, &ev->done, &call_mutex);
if (res == NULL) {
if (exc_type)
PyErr_Restore(exc_type, exc_value, exc_tb);
else
PyErr_SetObject(Tkinter_TclError, exc_value);
}
} else
#endif
{
objv = Tkapp_CallArgs(args, objStore, &objc);
if (!objv)
return NULL;
ENTER_TCL
i = Tcl_EvalObjv(self->interp, objc, objv, flags);
ENTER_OVERLAP
if (i == TCL_ERROR)
Tkinter_Error(selfptr);
else
res = Tkapp_CallResult(self);
LEAVE_OVERLAP_TCL
Tkapp_CallDeallocArgs(objv, objStore, objc);
}
return res;
}
static PyObject *
Tkapp_GlobalCall(PyObject *self, PyObject *args) {
char *cmd;
PyObject *res = NULL;
CHECK_TCL_APPARTMENT;
cmd = Merge(args);
if (cmd) {
int err;
ENTER_TCL
err = Tcl_GlobalEval(Tkapp_Interp(self), cmd);
ENTER_OVERLAP
if (err == TCL_ERROR)
res = Tkinter_Error(self);
else
res = PyString_FromString(Tkapp_Result(self));
LEAVE_OVERLAP_TCL
ckfree(cmd);
}
return res;
}
static PyObject *
Tkapp_Eval(PyObject *self, PyObject *args) {
char *script;
PyObject *res = NULL;
int err;
if (!PyArg_ParseTuple(args, "s:eval", &script))
return NULL;
CHECK_TCL_APPARTMENT;
ENTER_TCL
err = Tcl_Eval(Tkapp_Interp(self), script);
ENTER_OVERLAP
if (err == TCL_ERROR)
res = Tkinter_Error(self);
else
res = PyString_FromString(Tkapp_Result(self));
LEAVE_OVERLAP_TCL
return res;
}
static PyObject *
Tkapp_GlobalEval(PyObject *self, PyObject *args) {
char *script;
PyObject *res = NULL;
int err;
if (!PyArg_ParseTuple(args, "s:globaleval", &script))
return NULL;
CHECK_TCL_APPARTMENT;
ENTER_TCL
err = Tcl_GlobalEval(Tkapp_Interp(self), script);
ENTER_OVERLAP
if (err == TCL_ERROR)
res = Tkinter_Error(self);
else
res = PyString_FromString(Tkapp_Result(self));
LEAVE_OVERLAP_TCL
return res;
}
static PyObject *
Tkapp_EvalFile(PyObject *self, PyObject *args) {
char *fileName;
PyObject *res = NULL;
int err;
if (!PyArg_ParseTuple(args, "s:evalfile", &fileName))
return NULL;
CHECK_TCL_APPARTMENT;
ENTER_TCL
err = Tcl_EvalFile(Tkapp_Interp(self), fileName);
ENTER_OVERLAP
if (err == TCL_ERROR)
res = Tkinter_Error(self);
else
res = PyString_FromString(Tkapp_Result(self));
LEAVE_OVERLAP_TCL
return res;
}
static PyObject *
Tkapp_Record(PyObject *self, PyObject *args) {
char *script;
PyObject *res = NULL;
int err;
if (!PyArg_ParseTuple(args, "s", &script))
return NULL;
CHECK_TCL_APPARTMENT;
ENTER_TCL
err = Tcl_RecordAndEval(Tkapp_Interp(self), script, TCL_NO_EVAL);
ENTER_OVERLAP
if (err == TCL_ERROR)
res = Tkinter_Error(self);
else
res = PyString_FromString(Tkapp_Result(self));
LEAVE_OVERLAP_TCL
return res;
}
static PyObject *
Tkapp_AddErrorInfo(PyObject *self, PyObject *args) {
char *msg;
if (!PyArg_ParseTuple(args, "s:adderrorinfo", &msg))
return NULL;
CHECK_TCL_APPARTMENT;
ENTER_TCL
Tcl_AddErrorInfo(Tkapp_Interp(self), msg);
LEAVE_TCL
Py_INCREF(Py_None);
return Py_None;
}
TCL_DECLARE_MUTEX(var_mutex)
typedef PyObject* (*EventFunc)(PyObject*, PyObject *args, int flags);
typedef struct VarEvent {
Tcl_Event ev;
PyObject *self;
PyObject *args;
int flags;
EventFunc func;
PyObject **res;
PyObject **exc_type;
PyObject **exc_val;
Tcl_Condition cond;
} VarEvent;
static int
varname_converter(PyObject *in, void *_out) {
char **out = (char**)_out;
if (PyString_Check(in)) {
*out = PyString_AsString(in);
return 1;
}
if (PyTclObject_Check(in)) {
*out = PyTclObject_TclString(in);
return 1;
}
return 0;
}
static void
var_perform(VarEvent *ev) {
*(ev->res) = ev->func(ev->self, ev->args, ev->flags);
if (!*(ev->res)) {
PyObject *exc, *val, *tb;
PyErr_Fetch(&exc, &val, &tb);
PyErr_NormalizeException(&exc, &val, &tb);
*(ev->exc_type) = exc;
*(ev->exc_val) = val;
Py_DECREF(tb);
}
}
static int
var_proc(VarEvent* ev, int flags) {
ENTER_PYTHON
var_perform(ev);
Tcl_MutexLock(&var_mutex);
Tcl_ConditionNotify(&ev->cond);
Tcl_MutexUnlock(&var_mutex);
LEAVE_PYTHON
return 1;
}
static PyObject*
var_invoke(EventFunc func, PyObject *selfptr, PyObject *args, int flags) {
TkappObject *self = (TkappObject*)selfptr;
#if defined(WITH_THREAD)
if (self->threaded && self->thread_id != Tcl_GetCurrentThread()) {
TkappObject *self = (TkappObject*)selfptr;
VarEvent *ev;
PyObject *res, *exc_type, *exc_val;
if (!WaitForMainloop(self))
return NULL;
ev = (VarEvent*)ckalloc(sizeof(VarEvent));
ev->self = selfptr;
ev->args = args;
ev->flags = flags;
ev->func = func;
ev->res = &res;
ev->exc_type = &exc_type;
ev->exc_val = &exc_val;
ev->cond = NULL;
ev->ev.proc = (Tcl_EventProc*)var_proc;
Tkapp_ThreadSend(self, (Tcl_Event*)ev, &ev->cond, &var_mutex);
if (!res) {
PyErr_SetObject(exc_type, exc_val);
Py_DECREF(exc_type);
Py_DECREF(exc_val);
return NULL;
}
return res;
}
#endif
return func(selfptr, args, flags);
}
static PyObject *
SetVar(PyObject *self, PyObject *args, int flags) {
char *name1, *name2;
PyObject *newValue;
PyObject *res = NULL;
Tcl_Obj *newval, *ok;
if (PyArg_ParseTuple(args, "O&O:setvar",
varname_converter, &name1, &newValue)) {
newval = AsObj(newValue);
if (newval == NULL)
return NULL;
ENTER_TCL
ok = Tcl_SetVar2Ex(Tkapp_Interp(self), name1, NULL,
newval, flags);
ENTER_OVERLAP
if (!ok)
Tkinter_Error(self);
else {
res = Py_None;
Py_INCREF(res);
}
LEAVE_OVERLAP_TCL
} else {
PyErr_Clear();
if (PyArg_ParseTuple(args, "ssO:setvar",
&name1, &name2, &newValue)) {
newval = AsObj(newValue);
ENTER_TCL
ok = Tcl_SetVar2Ex(Tkapp_Interp(self), name1, name2, newval, flags);
ENTER_OVERLAP
if (!ok)
Tkinter_Error(self);
else {
res = Py_None;
Py_INCREF(res);
}
LEAVE_OVERLAP_TCL
} else {
return NULL;
}
}
return res;
}
static PyObject *
Tkapp_SetVar(PyObject *self, PyObject *args) {
return var_invoke(SetVar, self, args, TCL_LEAVE_ERR_MSG);
}
static PyObject *
Tkapp_GlobalSetVar(PyObject *self, PyObject *args) {
return var_invoke(SetVar, self, args, TCL_LEAVE_ERR_MSG | TCL_GLOBAL_ONLY);
}
static PyObject *
GetVar(PyObject *self, PyObject *args, int flags) {
char *name1, *name2=NULL;
PyObject *res = NULL;
Tcl_Obj *tres;
if (!PyArg_ParseTuple(args, "O&|s:getvar",
varname_converter, &name1, &name2))
return NULL;
ENTER_TCL
tres = Tcl_GetVar2Ex(Tkapp_Interp(self), name1, name2, flags);
ENTER_OVERLAP
if (tres == NULL) {
PyErr_SetString(Tkinter_TclError, Tcl_GetStringResult(Tkapp_Interp(self)));
} else {
if (((TkappObject*)self)->wantobjects) {
res = FromObj(self, tres);
} else {
res = PyString_FromString(Tcl_GetString(tres));
}
}
LEAVE_OVERLAP_TCL
return res;
}
static PyObject *
Tkapp_GetVar(PyObject *self, PyObject *args) {
return var_invoke(GetVar, self, args, TCL_LEAVE_ERR_MSG);
}
static PyObject *
Tkapp_GlobalGetVar(PyObject *self, PyObject *args) {
return var_invoke(GetVar, self, args, TCL_LEAVE_ERR_MSG | TCL_GLOBAL_ONLY);
}
static PyObject *
UnsetVar(PyObject *self, PyObject *args, int flags) {
char *name1, *name2=NULL;
int code;
PyObject *res = NULL;
if (!PyArg_ParseTuple(args, "s|s:unsetvar", &name1, &name2))
return NULL;
ENTER_TCL
code = Tcl_UnsetVar2(Tkapp_Interp(self), name1, name2, flags);
ENTER_OVERLAP
if (code == TCL_ERROR)
res = Tkinter_Error(self);
else {
Py_INCREF(Py_None);
res = Py_None;
}
LEAVE_OVERLAP_TCL
return res;
}
static PyObject *
Tkapp_UnsetVar(PyObject *self, PyObject *args) {
return var_invoke(UnsetVar, self, args, TCL_LEAVE_ERR_MSG);
}
static PyObject *
Tkapp_GlobalUnsetVar(PyObject *self, PyObject *args) {
return var_invoke(UnsetVar, self, args, TCL_LEAVE_ERR_MSG | TCL_GLOBAL_ONLY);
}
static PyObject *
Tkapp_GetInt(PyObject *self, PyObject *args) {
char *s;
int v;
if (PyTuple_Size(args) == 1) {
PyObject* o = PyTuple_GetItem(args, 0);
if (PyInt_Check(o)) {
Py_INCREF(o);
return o;
}
}
if (!PyArg_ParseTuple(args, "s:getint", &s))
return NULL;
if (Tcl_GetInt(Tkapp_Interp(self), s, &v) == TCL_ERROR)
return Tkinter_Error(self);
return Py_BuildValue("i", v);
}
static PyObject *
Tkapp_GetDouble(PyObject *self, PyObject *args) {
char *s;
double v;
if (PyTuple_Size(args) == 1) {
PyObject *o = PyTuple_GetItem(args, 0);
if (PyFloat_Check(o)) {
Py_INCREF(o);
return o;
}
}
if (!PyArg_ParseTuple(args, "s:getdouble", &s))
return NULL;
if (Tcl_GetDouble(Tkapp_Interp(self), s, &v) == TCL_ERROR)
return Tkinter_Error(self);
return Py_BuildValue("d", v);
}
static PyObject *
Tkapp_GetBoolean(PyObject *self, PyObject *args) {
char *s;
int v;
if (PyTuple_Size(args) == 1) {
PyObject *o = PyTuple_GetItem(args, 0);
if (PyInt_Check(o)) {
Py_INCREF(o);
return o;
}
}
if (!PyArg_ParseTuple(args, "s:getboolean", &s))
return NULL;
if (Tcl_GetBoolean(Tkapp_Interp(self), s, &v) == TCL_ERROR)
return Tkinter_Error(self);
return PyBool_FromLong(v);
}
static PyObject *
Tkapp_ExprString(PyObject *self, PyObject *args) {
char *s;
PyObject *res = NULL;
int retval;
if (!PyArg_ParseTuple(args, "s:exprstring", &s))
return NULL;
CHECK_TCL_APPARTMENT;
ENTER_TCL
retval = Tcl_ExprString(Tkapp_Interp(self), s);
ENTER_OVERLAP
if (retval == TCL_ERROR)
res = Tkinter_Error(self);
else
res = Py_BuildValue("s", Tkapp_Result(self));
LEAVE_OVERLAP_TCL
return res;
}
static PyObject *
Tkapp_ExprLong(PyObject *self, PyObject *args) {
char *s;
PyObject *res = NULL;
int retval;
long v;
if (!PyArg_ParseTuple(args, "s:exprlong", &s))
return NULL;
CHECK_TCL_APPARTMENT;
ENTER_TCL
retval = Tcl_ExprLong(Tkapp_Interp(self), s, &v);
ENTER_OVERLAP
if (retval == TCL_ERROR)
res = Tkinter_Error(self);
else
res = Py_BuildValue("l", v);
LEAVE_OVERLAP_TCL
return res;
}
static PyObject *
Tkapp_ExprDouble(PyObject *self, PyObject *args) {
char *s;
PyObject *res = NULL;
double v;
int retval;
if (!PyArg_ParseTuple(args, "s:exprdouble", &s))
return NULL;
CHECK_TCL_APPARTMENT;
PyFPE_START_PROTECT("Tkapp_ExprDouble", return 0)
ENTER_TCL
retval = Tcl_ExprDouble(Tkapp_Interp(self), s, &v);
ENTER_OVERLAP
PyFPE_END_PROTECT(retval)
if (retval == TCL_ERROR)
res = Tkinter_Error(self);
else
res = Py_BuildValue("d", v);
LEAVE_OVERLAP_TCL
return res;
}
static PyObject *
Tkapp_ExprBoolean(PyObject *self, PyObject *args) {
char *s;
PyObject *res = NULL;
int retval;
int v;
if (!PyArg_ParseTuple(args, "s:exprboolean", &s))
return NULL;
CHECK_TCL_APPARTMENT;
ENTER_TCL
retval = Tcl_ExprBoolean(Tkapp_Interp(self), s, &v);
ENTER_OVERLAP
if (retval == TCL_ERROR)
res = Tkinter_Error(self);
else
res = Py_BuildValue("i", v);
LEAVE_OVERLAP_TCL
return res;
}
static PyObject *
Tkapp_SplitList(PyObject *self, PyObject *args) {
char *list;
int argc;
char **argv;
PyObject *v;
int i;
if (PyTuple_Size(args) == 1) {
v = PyTuple_GetItem(args, 0);
if (PyTuple_Check(v)) {
Py_INCREF(v);
return v;
}
}
if (!PyArg_ParseTuple(args, "et:splitlist", "utf-8", &list))
return NULL;
if (Tcl_SplitList(Tkapp_Interp(self), list,
&argc, &argv) == TCL_ERROR) {
PyMem_Free(list);
return Tkinter_Error(self);
}
if (!(v = PyTuple_New(argc)))
goto finally;
for (i = 0; i < argc; i++) {
PyObject *s = PyString_FromString(argv[i]);
if (!s || PyTuple_SetItem(v, i, s)) {
Py_DECREF(v);
v = NULL;
goto finally;
}
}
finally:
ckfree(FREECAST argv);
PyMem_Free(list);
return v;
}
static PyObject *
Tkapp_Split(PyObject *self, PyObject *args) {
PyObject *v;
char *list;
if (PyTuple_Size(args) == 1) {
PyObject* o = PyTuple_GetItem(args, 0);
if (PyTuple_Check(o)) {
o = SplitObj(o);
return o;
}
}
if (!PyArg_ParseTuple(args, "et:split", "utf-8", &list))
return NULL;
v = Split(list);
PyMem_Free(list);
return v;
}
static PyObject *
Tkapp_Merge(PyObject *self, PyObject *args) {
char *s = Merge(args);
PyObject *res = NULL;
if (s) {
res = PyString_FromString(s);
ckfree(s);
}
return res;
}
typedef struct {
PyObject *self;
PyObject *func;
} PythonCmd_ClientData;
static int
PythonCmd_Error(Tcl_Interp *interp) {
errorInCmd = 1;
PyErr_Fetch(&excInCmd, &valInCmd, &trbInCmd);
LEAVE_PYTHON
return TCL_ERROR;
}
static int
PythonCmd(ClientData clientData, Tcl_Interp *interp, int argc, char *argv[]) {
PythonCmd_ClientData *data = (PythonCmd_ClientData *)clientData;
PyObject *self, *func, *arg, *res;
int i, rv;
Tcl_Obj *obj_res;
ENTER_PYTHON
self = data->self;
func = data->func;
if (!(arg = PyTuple_New(argc - 1)))
return PythonCmd_Error(interp);
for (i = 0; i < (argc - 1); i++) {
PyObject *s = PyString_FromString(argv[i + 1]);
if (!s || PyTuple_SetItem(arg, i, s)) {
Py_DECREF(arg);
return PythonCmd_Error(interp);
}
}
res = PyEval_CallObject(func, arg);
Py_DECREF(arg);
if (res == NULL)
return PythonCmd_Error(interp);
obj_res = AsObj(res);
if (obj_res == NULL) {
Py_DECREF(res);
return PythonCmd_Error(interp);
} else {
Tcl_SetObjResult(Tkapp_Interp(self), obj_res);
rv = TCL_OK;
}
Py_DECREF(res);
LEAVE_PYTHON
return rv;
}
static void
PythonCmdDelete(ClientData clientData) {
PythonCmd_ClientData *data = (PythonCmd_ClientData *)clientData;
ENTER_PYTHON
Py_XDECREF(data->self);
Py_XDECREF(data->func);
PyMem_DEL(data);
LEAVE_PYTHON
}
TCL_DECLARE_MUTEX(command_mutex)
typedef struct CommandEvent {
Tcl_Event ev;
Tcl_Interp* interp;
char *name;
int create;
int *status;
ClientData *data;
Tcl_Condition done;
} CommandEvent;
static int
Tkapp_CommandProc(CommandEvent *ev, int flags) {
if (ev->create)
*ev->status = Tcl_CreateCommand(
ev->interp, ev->name, PythonCmd,
ev->data, PythonCmdDelete) == NULL;
else
*ev->status = Tcl_DeleteCommand(ev->interp, ev->name);
Tcl_MutexLock(&command_mutex);
Tcl_ConditionNotify(&ev->done);
Tcl_MutexUnlock(&command_mutex);
return 1;
}
static PyObject *
Tkapp_CreateCommand(PyObject *selfptr, PyObject *args) {
TkappObject *self = (TkappObject*)selfptr;
PythonCmd_ClientData *data;
char *cmdName;
PyObject *func;
int err;
if (!PyArg_ParseTuple(args, "sO:createcommand", &cmdName, &func))
return NULL;
if (!PyCallable_Check(func)) {
PyErr_SetString(PyExc_TypeError, "command not callable");
return NULL;
}
#if defined(WITH_THREAD)
if (self->threaded && self->thread_id != Tcl_GetCurrentThread() &&
!WaitForMainloop(self))
return NULL;
#endif
data = PyMem_NEW(PythonCmd_ClientData, 1);
if (!data)
return PyErr_NoMemory();
Py_INCREF(self);
Py_INCREF(func);
data->self = selfptr;
data->func = func;
if (self->threaded && self->thread_id != Tcl_GetCurrentThread()) {
CommandEvent *ev = (CommandEvent*)ckalloc(sizeof(CommandEvent));
ev->ev.proc = (Tcl_EventProc*)Tkapp_CommandProc;
ev->interp = self->interp;
ev->create = 1;
ev->name = cmdName;
ev->data = (ClientData)data;
ev->status = &err;
ev->done = NULL;
Tkapp_ThreadSend(self, (Tcl_Event*)ev, &ev->done, &command_mutex);
} else {
ENTER_TCL
err = Tcl_CreateCommand(
Tkapp_Interp(self), cmdName, PythonCmd,
(ClientData)data, PythonCmdDelete) == NULL;
LEAVE_TCL
}
if (err) {
PyErr_SetString(Tkinter_TclError, "can't create Tcl command");
PyMem_DEL(data);
return NULL;
}
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
Tkapp_DeleteCommand(PyObject *selfptr, PyObject *args) {
TkappObject *self = (TkappObject*)selfptr;
char *cmdName;
int err;
if (!PyArg_ParseTuple(args, "s:deletecommand", &cmdName))
return NULL;
if (self->threaded && self->thread_id != Tcl_GetCurrentThread()) {
CommandEvent *ev;
ev = (CommandEvent*)ckalloc(sizeof(CommandEvent));
ev->ev.proc = (Tcl_EventProc*)Tkapp_CommandProc;
ev->interp = self->interp;
ev->create = 0;
ev->name = cmdName;
ev->status = &err;
ev->done = NULL;
Tkapp_ThreadSend(self, (Tcl_Event*)ev, &ev->done,
&command_mutex);
} else {
ENTER_TCL
err = Tcl_DeleteCommand(self->interp, cmdName);
LEAVE_TCL
}
if (err == -1) {
PyErr_SetString(Tkinter_TclError, "can't delete Tcl command");
return NULL;
}
Py_INCREF(Py_None);
return Py_None;
}
#if defined(HAVE_CREATEFILEHANDLER)
typedef struct _fhcdata {
PyObject *func;
PyObject *file;
int id;
struct _fhcdata *next;
} FileHandler_ClientData;
static FileHandler_ClientData *HeadFHCD;
static FileHandler_ClientData *
NewFHCD(PyObject *func, PyObject *file, int id) {
FileHandler_ClientData *p;
p = PyMem_NEW(FileHandler_ClientData, 1);
if (p != NULL) {
Py_XINCREF(func);
Py_XINCREF(file);
p->func = func;
p->file = file;
p->id = id;
p->next = HeadFHCD;
HeadFHCD = p;
}
return p;
}
static void
DeleteFHCD(int id) {
FileHandler_ClientData *p, **pp;
pp = &HeadFHCD;
while ((p = *pp) != NULL) {
if (p->id == id) {
*pp = p->next;
Py_XDECREF(p->func);
Py_XDECREF(p->file);
PyMem_DEL(p);
} else
pp = &p->next;
}
}
static void
FileHandler(ClientData clientData, int mask) {
FileHandler_ClientData *data = (FileHandler_ClientData *)clientData;
PyObject *func, *file, *arg, *res;
ENTER_PYTHON
func = data->func;
file = data->file;
arg = Py_BuildValue("(Oi)", file, (long) mask);
res = PyEval_CallObject(func, arg);
Py_DECREF(arg);
if (res == NULL) {
errorInCmd = 1;
PyErr_Fetch(&excInCmd, &valInCmd, &trbInCmd);
}
Py_XDECREF(res);
LEAVE_PYTHON
}
static PyObject *
Tkapp_CreateFileHandler(PyObject *self, PyObject *args)
{
FileHandler_ClientData *data;
PyObject *file, *func;
int mask, tfile;
if (!PyArg_ParseTuple(args, "OiO:createfilehandler",
&file, &mask, &func))
return NULL;
#if defined(WITH_THREAD)
if (!self && !tcl_lock) {
PyErr_SetString(PyExc_RuntimeError,
"_tkinter.createfilehandler not supported "
"for threaded Tcl");
return NULL;
}
#endif
if (self) {
CHECK_TCL_APPARTMENT;
}
tfile = PyObject_AsFileDescriptor(file);
if (tfile < 0)
return NULL;
if (!PyCallable_Check(func)) {
PyErr_SetString(PyExc_TypeError, "bad argument list");
return NULL;
}
data = NewFHCD(func, file, tfile);
if (data == NULL)
return NULL;
ENTER_TCL
Tcl_CreateFileHandler(tfile, mask, FileHandler, (ClientData) data);
LEAVE_TCL
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
Tkapp_DeleteFileHandler(PyObject *self, PyObject *args) {
PyObject *file;
int tfile;
if (!PyArg_ParseTuple(args, "O:deletefilehandler", &file))
return NULL;
#if defined(WITH_THREAD)
if (!self && !tcl_lock) {
PyErr_SetString(PyExc_RuntimeError,
"_tkinter.deletefilehandler not supported "
"for threaded Tcl");
return NULL;
}
#endif
if (self) {
CHECK_TCL_APPARTMENT;
}
tfile = PyObject_AsFileDescriptor(file);
if (tfile < 0)
return NULL;
DeleteFHCD(tfile);
ENTER_TCL
Tcl_DeleteFileHandler(tfile);
LEAVE_TCL
Py_INCREF(Py_None);
return Py_None;
}
#endif
static PyTypeObject Tktt_Type;
typedef struct {
PyObject_HEAD
Tcl_TimerToken token;
PyObject *func;
} TkttObject;
static PyObject *
Tktt_DeleteTimerHandler(PyObject *self, PyObject *args) {
TkttObject *v = (TkttObject *)self;
PyObject *func = v->func;
if (!PyArg_ParseTuple(args, ":deletetimerhandler"))
return NULL;
if (v->token != NULL) {
Tcl_DeleteTimerHandler(v->token);
v->token = NULL;
}
if (func != NULL) {
v->func = NULL;
Py_DECREF(func);
Py_DECREF(v);
}
Py_INCREF(Py_None);
return Py_None;
}
static PyMethodDef Tktt_methods[] = {
{"deletetimerhandler", Tktt_DeleteTimerHandler, METH_VARARGS},
{NULL, NULL}
};
static TkttObject *
Tktt_New(PyObject *func) {
TkttObject *v;
v = PyObject_New(TkttObject, &Tktt_Type);
if (v == NULL)
return NULL;
Py_INCREF(func);
v->token = NULL;
v->func = func;
Py_INCREF(v);
return v;
}
static void
Tktt_Dealloc(PyObject *self) {
TkttObject *v = (TkttObject *)self;
PyObject *func = v->func;
Py_XDECREF(func);
PyObject_Del(self);
}
static PyObject *
Tktt_Repr(PyObject *self) {
TkttObject *v = (TkttObject *)self;
char buf[100];
PyOS_snprintf(buf, sizeof(buf), "<tktimertoken at %p%s>", v,
v->func == NULL ? ", handler deleted" : "");
return PyString_FromString(buf);
}
static PyObject *
Tktt_GetAttr(PyObject *self, char *name) {
return Py_FindMethod(Tktt_methods, self, name);
}
static PyTypeObject Tktt_Type = {
PyVarObject_HEAD_INIT(NULL, 0)
"tktimertoken",
sizeof(TkttObject),
0,
Tktt_Dealloc,
0,
Tktt_GetAttr,
0,
0,
Tktt_Repr,
0,
0,
0,
0,
};
static void
TimerHandler(ClientData clientData) {
TkttObject *v = (TkttObject *)clientData;
PyObject *func = v->func;
PyObject *res;
if (func == NULL)
return;
v->func = NULL;
ENTER_PYTHON
res = PyEval_CallObject(func, NULL);
Py_DECREF(func);
Py_DECREF(v);
if (res == NULL) {
errorInCmd = 1;
PyErr_Fetch(&excInCmd, &valInCmd, &trbInCmd);
} else
Py_DECREF(res);
LEAVE_PYTHON
}
static PyObject *
Tkapp_CreateTimerHandler(PyObject *self, PyObject *args) {
int milliseconds;
PyObject *func;
TkttObject *v;
if (!PyArg_ParseTuple(args, "iO:createtimerhandler",
&milliseconds, &func))
return NULL;
if (!PyCallable_Check(func)) {
PyErr_SetString(PyExc_TypeError, "bad argument list");
return NULL;
}
#if defined(WITH_THREAD)
if (!self && !tcl_lock) {
PyErr_SetString(PyExc_RuntimeError,
"_tkinter.createtimerhandler not supported "
"for threaded Tcl");
return NULL;
}
#endif
if (self) {
CHECK_TCL_APPARTMENT;
}
v = Tktt_New(func);
if (v) {
v->token = Tcl_CreateTimerHandler(milliseconds, TimerHandler,
(ClientData)v);
}
return (PyObject *) v;
}
static PyObject *
Tkapp_MainLoop(PyObject *selfptr, PyObject *args) {
int threshold = 0;
TkappObject *self = (TkappObject*)selfptr;
#if defined(WITH_THREAD)
PyThreadState *tstate = PyThreadState_Get();
#endif
if (!PyArg_ParseTuple(args, "|i:mainloop", &threshold))
return NULL;
#if defined(WITH_THREAD)
if (!self && !tcl_lock) {
PyErr_SetString(PyExc_RuntimeError,
"_tkinter.mainloop not supported "
"for threaded Tcl");
return NULL;
}
#endif
if (self) {
CHECK_TCL_APPARTMENT;
self->dispatching = 1;
}
quitMainLoop = 0;
while (Tk_GetNumMainWindows() > threshold &&
!quitMainLoop &&
!errorInCmd) {
int result;
#if defined(WITH_THREAD)
if (self && self->threaded) {
ENTER_TCL
result = Tcl_DoOneEvent(0);
LEAVE_TCL
} else {
Py_BEGIN_ALLOW_THREADS
if(tcl_lock)PyThread_acquire_lock(tcl_lock, 1);
tcl_tstate = tstate;
result = Tcl_DoOneEvent(TCL_DONT_WAIT);
tcl_tstate = NULL;
if(tcl_lock)PyThread_release_lock(tcl_lock);
if (result == 0)
Sleep(Tkinter_busywaitinterval);
Py_END_ALLOW_THREADS
}
#else
result = Tcl_DoOneEvent(0);
#endif
if (PyErr_CheckSignals() != 0) {
if (self)
self->dispatching = 0;
return NULL;
}
if (result < 0)
break;
}
if (self)
self->dispatching = 0;
quitMainLoop = 0;
if (errorInCmd) {
errorInCmd = 0;
PyErr_Restore(excInCmd, valInCmd, trbInCmd);
excInCmd = valInCmd = trbInCmd = NULL;
return NULL;
}
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
Tkapp_DoOneEvent(PyObject *self, PyObject *args) {
int flags = 0;
int rv;
if (!PyArg_ParseTuple(args, "|i:dooneevent", &flags))
return NULL;
ENTER_TCL
rv = Tcl_DoOneEvent(flags);
LEAVE_TCL
return Py_BuildValue("i", rv);
}
static PyObject *
Tkapp_Quit(PyObject *self, PyObject *args) {
if (!PyArg_ParseTuple(args, ":quit"))
return NULL;
quitMainLoop = 1;
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
Tkapp_InterpAddr(PyObject *self, PyObject *args) {
if (!PyArg_ParseTuple(args, ":interpaddr"))
return NULL;
return PyInt_FromLong((long)Tkapp_Interp(self));
}
static PyObject *
Tkapp_TkInit(PyObject *self, PyObject *args) {
static int has_failed;
Tcl_Interp *interp = Tkapp_Interp(self);
Tk_Window main_window;
const char * _tk_exists = NULL;
int err;
main_window = Tk_MainWindow(interp);
if (has_failed) {
PyErr_SetString(Tkinter_TclError,
"Calling Tk_Init again after a previous call failed might deadlock");
return NULL;
}
CHECK_TCL_APPARTMENT;
ENTER_TCL
err = Tcl_Eval(Tkapp_Interp(self), "info exists tk_version");
ENTER_OVERLAP
if (err == TCL_ERROR) {
Tkinter_Error(self);
} else {
_tk_exists = Tkapp_Result(self);
}
LEAVE_OVERLAP_TCL
if (err == TCL_ERROR) {
return NULL;
}
if (_tk_exists == NULL || strcmp(_tk_exists, "1") != 0) {
if (Tk_Init(interp) == TCL_ERROR) {
PyErr_SetString(Tkinter_TclError, Tcl_GetStringResult(Tkapp_Interp(self)));
has_failed = 1;
return NULL;
}
}
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
Tkapp_WantObjects(PyObject *self, PyObject *args) {
int wantobjects = -1;
if (!PyArg_ParseTuple(args, "|i:wantobjects", &wantobjects))
return NULL;
if (wantobjects == -1)
return PyBool_FromLong(((TkappObject*)self)->wantobjects);
((TkappObject*)self)->wantobjects = wantobjects;
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
Tkapp_WillDispatch(PyObject *self, PyObject *args) {
((TkappObject*)self)->dispatching = 1;
Py_INCREF(Py_None);
return Py_None;
}
static PyMethodDef Tkapp_methods[] = {
{"willdispatch", Tkapp_WillDispatch, METH_NOARGS},
{"wantobjects", Tkapp_WantObjects, METH_VARARGS},
{"call", Tkapp_Call, METH_VARARGS},
{"globalcall", Tkapp_GlobalCall, METH_VARARGS},
{"eval", Tkapp_Eval, METH_VARARGS},
{"globaleval", Tkapp_GlobalEval, METH_VARARGS},
{"evalfile", Tkapp_EvalFile, METH_VARARGS},
{"record", Tkapp_Record, METH_VARARGS},
{"adderrorinfo", Tkapp_AddErrorInfo, METH_VARARGS},
{"setvar", Tkapp_SetVar, METH_VARARGS},
{"globalsetvar", Tkapp_GlobalSetVar, METH_VARARGS},
{"getvar", Tkapp_GetVar, METH_VARARGS},
{"globalgetvar", Tkapp_GlobalGetVar, METH_VARARGS},
{"unsetvar", Tkapp_UnsetVar, METH_VARARGS},
{"globalunsetvar", Tkapp_GlobalUnsetVar, METH_VARARGS},
{"getint", Tkapp_GetInt, METH_VARARGS},
{"getdouble", Tkapp_GetDouble, METH_VARARGS},
{"getboolean", Tkapp_GetBoolean, METH_VARARGS},
{"exprstring", Tkapp_ExprString, METH_VARARGS},
{"exprlong", Tkapp_ExprLong, METH_VARARGS},
{"exprdouble", Tkapp_ExprDouble, METH_VARARGS},
{"exprboolean", Tkapp_ExprBoolean, METH_VARARGS},
{"splitlist", Tkapp_SplitList, METH_VARARGS},
{"split", Tkapp_Split, METH_VARARGS},
{"merge", Tkapp_Merge, METH_VARARGS},
{"createcommand", Tkapp_CreateCommand, METH_VARARGS},
{"deletecommand", Tkapp_DeleteCommand, METH_VARARGS},
#if defined(HAVE_CREATEFILEHANDLER)
{"createfilehandler", Tkapp_CreateFileHandler, METH_VARARGS},
{"deletefilehandler", Tkapp_DeleteFileHandler, METH_VARARGS},
#endif
{"createtimerhandler", Tkapp_CreateTimerHandler, METH_VARARGS},
{"mainloop", Tkapp_MainLoop, METH_VARARGS},
{"dooneevent", Tkapp_DoOneEvent, METH_VARARGS},
{"quit", Tkapp_Quit, METH_VARARGS},
{"interpaddr", Tkapp_InterpAddr, METH_VARARGS},
{"loadtk", Tkapp_TkInit, METH_NOARGS},
{NULL, NULL}
};
static void
Tkapp_Dealloc(PyObject *self) {
ENTER_TCL
Tcl_DeleteInterp(Tkapp_Interp(self));
LEAVE_TCL
PyObject_Del(self);
DisableEventHook();
}
static PyObject *
Tkapp_GetAttr(PyObject *self, char *name) {
return Py_FindMethod(Tkapp_methods, self, name);
}
static PyTypeObject Tkapp_Type = {
PyVarObject_HEAD_INIT(NULL, 0)
"tkapp",
sizeof(TkappObject),
0,
Tkapp_Dealloc,
0,
Tkapp_GetAttr,
0,
0,
0,
0,
0,
0,
0,
};
typedef struct {
PyObject* tuple;
int size;
int maxsize;
} FlattenContext;
static int
_bump(FlattenContext* context, int size) {
int maxsize = context->maxsize * 2;
if (maxsize < context->size + size)
maxsize = context->size + size;
context->maxsize = maxsize;
return _PyTuple_Resize(&context->tuple, maxsize) >= 0;
}
static int
_flatten1(FlattenContext* context, PyObject* item, int depth) {
int i, size;
if (depth > 1000) {
PyErr_SetString(PyExc_ValueError,
"nesting too deep in _flatten");
return 0;
} else if (PyList_Check(item)) {
size = PyList_GET_SIZE(item);
if (context->size + size > context->maxsize &&
!_bump(context, size))
return 0;
for (i = 0; i < size; i++) {
PyObject *o = PyList_GET_ITEM(item, i);
if (PyList_Check(o) || PyTuple_Check(o)) {
if (!_flatten1(context, o, depth + 1))
return 0;
} else if (o != Py_None) {
if (context->size + 1 > context->maxsize &&
!_bump(context, 1))
return 0;
Py_INCREF(o);
PyTuple_SET_ITEM(context->tuple,
context->size++, o);
}
}
} else if (PyTuple_Check(item)) {
size = PyTuple_GET_SIZE(item);
if (context->size + size > context->maxsize &&
!_bump(context, size))
return 0;
for (i = 0; i < size; i++) {
PyObject *o = PyTuple_GET_ITEM(item, i);
if (PyList_Check(o) || PyTuple_Check(o)) {
if (!_flatten1(context, o, depth + 1))
return 0;
} else if (o != Py_None) {
if (context->size + 1 > context->maxsize &&
!_bump(context, 1))
return 0;
Py_INCREF(o);
PyTuple_SET_ITEM(context->tuple,
context->size++, o);
}
}
} else {
PyErr_SetString(PyExc_TypeError, "argument must be sequence");
return 0;
}
return 1;
}
static PyObject *
Tkinter_Flatten(PyObject* self, PyObject* args) {
FlattenContext context;
PyObject* item;
if (!PyArg_ParseTuple(args, "O:_flatten", &item))
return NULL;
context.maxsize = PySequence_Size(item);
if (context.maxsize <= 0)
return PyTuple_New(0);
context.tuple = PyTuple_New(context.maxsize);
if (!context.tuple)
return NULL;
context.size = 0;
if (!_flatten1(&context, item,0))
return NULL;
if (_PyTuple_Resize(&context.tuple, context.size))
return NULL;
return context.tuple;
}
static PyObject *
Tkinter_Create(PyObject *self, PyObject *args) {
char *screenName = NULL;
char *baseName = NULL;
char *className = NULL;
int interactive = 0;
int wantobjects = 0;
int wantTk = 1;
int sync = 0;
char *use = NULL;
baseName = strrchr(Py_GetProgramName(), '/');
if (baseName != NULL)
baseName++;
else
baseName = Py_GetProgramName();
className = "Tk";
if (!PyArg_ParseTuple(args, "|zssiiiiz:create",
&screenName, &baseName, &className,
&interactive, &wantobjects, &wantTk,
&sync, &use))
return NULL;
return (PyObject *) Tkapp_New(screenName, baseName, className,
interactive, wantobjects, wantTk,
sync, use);
}
static PyObject *
Tkinter_setbusywaitinterval(PyObject *self, PyObject *args) {
int new_val;
if (!PyArg_ParseTuple(args, "i:setbusywaitinterval", &new_val))
return NULL;
if (new_val < 0) {
PyErr_SetString(PyExc_ValueError,
"busywaitinterval must be >= 0");
return NULL;
}
Tkinter_busywaitinterval = new_val;
Py_INCREF(Py_None);
return Py_None;
}
static char setbusywaitinterval_doc[] =
"setbusywaitinterval(n) -> None\n\
\n\
Set the busy-wait interval in milliseconds between successive\n\
calls to Tcl_DoOneEvent in a threaded Python interpreter.\n\
It should be set to a divisor of the maximum time between\n\
frames in an animation.";
static PyObject *
Tkinter_getbusywaitinterval(PyObject *self, PyObject *args) {
return PyInt_FromLong(Tkinter_busywaitinterval);
}
static char getbusywaitinterval_doc[] =
"getbusywaitinterval() -> int\n\
\n\
Return the current busy-wait interval between successive\n\
calls to Tcl_DoOneEvent in a threaded Python interpreter.";
static PyMethodDef moduleMethods[] = {
{"_flatten", Tkinter_Flatten, METH_VARARGS},
{"create", Tkinter_Create, METH_VARARGS},
#if defined(HAVE_CREATEFILEHANDLER)
{"createfilehandler", Tkapp_CreateFileHandler, METH_VARARGS},
{"deletefilehandler", Tkapp_DeleteFileHandler, METH_VARARGS},
#endif
{"createtimerhandler", Tkapp_CreateTimerHandler, METH_VARARGS},
{"mainloop", Tkapp_MainLoop, METH_VARARGS},
{"dooneevent", Tkapp_DoOneEvent, METH_VARARGS},
{"quit", Tkapp_Quit, METH_VARARGS},
{
"setbusywaitinterval",Tkinter_setbusywaitinterval, METH_VARARGS,
setbusywaitinterval_doc
},
{
"getbusywaitinterval",(PyCFunction)Tkinter_getbusywaitinterval,
METH_NOARGS, getbusywaitinterval_doc
},
{NULL, NULL}
};
#if defined(WAIT_FOR_STDIN)
static int stdin_ready = 0;
#if !defined(MS_WINDOWS)
static void
MyFileProc(void *clientData, int mask) {
stdin_ready = 1;
}
#endif
#if defined(WITH_THREAD)
static PyThreadState *event_tstate = NULL;
#endif
static int
EventHook(void) {
#if !defined(MS_WINDOWS)
int tfile;
#endif
#if defined(WITH_THREAD)
PyEval_RestoreThread(event_tstate);
#endif
stdin_ready = 0;
errorInCmd = 0;
#if !defined(MS_WINDOWS)
tfile = fileno(stdin);
Tcl_CreateFileHandler(tfile, TCL_READABLE, MyFileProc, NULL);
#endif
while (!errorInCmd && !stdin_ready) {
int result;
#if defined(MS_WINDOWS)
if (_kbhit()) {
stdin_ready = 1;
break;
}
#endif
#if defined(WITH_THREAD) || defined(MS_WINDOWS)
Py_BEGIN_ALLOW_THREADS
if(tcl_lock)PyThread_acquire_lock(tcl_lock, 1);
tcl_tstate = event_tstate;
result = Tcl_DoOneEvent(TCL_DONT_WAIT);
tcl_tstate = NULL;
if(tcl_lock)PyThread_release_lock(tcl_lock);
if (result == 0)
Sleep(Tkinter_busywaitinterval);
Py_END_ALLOW_THREADS
#else
result = Tcl_DoOneEvent(0);
#endif
if (result < 0)
break;
}
#if !defined(MS_WINDOWS)
Tcl_DeleteFileHandler(tfile);
#endif
if (errorInCmd) {
errorInCmd = 0;
PyErr_Restore(excInCmd, valInCmd, trbInCmd);
excInCmd = valInCmd = trbInCmd = NULL;
PyErr_Print();
}
#if defined(WITH_THREAD)
PyEval_SaveThread();
#endif
return 0;
}
#endif
static void
EnableEventHook(void) {
#if defined(WAIT_FOR_STDIN)
if (PyOS_InputHook == NULL) {
#if defined(WITH_THREAD)
event_tstate = PyThreadState_Get();
#endif
PyOS_InputHook = EventHook;
}
#endif
}
static void
DisableEventHook(void) {
#if defined(WAIT_FOR_STDIN)
if (Tk_GetNumMainWindows() == 0 && PyOS_InputHook == EventHook) {
PyOS_InputHook = NULL;
}
#endif
}
static void
ins_long(PyObject *d, char *name, long val) {
PyObject *v = PyInt_FromLong(val);
if (v) {
PyDict_SetItemString(d, name, v);
Py_DECREF(v);
}
}
static void
ins_string(PyObject *d, char *name, char *val) {
PyObject *v = PyString_FromString(val);
if (v) {
PyDict_SetItemString(d, name, v);
Py_DECREF(v);
}
}
PyMODINIT_FUNC
init_tkinter(void) {
PyObject *m, *d;
Py_TYPE(&Tkapp_Type) = &PyType_Type;
#if defined(WITH_THREAD)
tcl_lock = PyThread_allocate_lock();
#endif
m = Py_InitModule("_tkinter", moduleMethods);
if (m == NULL)
return;
d = PyModule_GetDict(m);
Tkinter_TclError = PyErr_NewException("_tkinter.TclError", NULL, NULL);
PyDict_SetItemString(d, "TclError", Tkinter_TclError);
ins_long(d, "READABLE", TCL_READABLE);
ins_long(d, "WRITABLE", TCL_WRITABLE);
ins_long(d, "EXCEPTION", TCL_EXCEPTION);
ins_long(d, "WINDOW_EVENTS", TCL_WINDOW_EVENTS);
ins_long(d, "FILE_EVENTS", TCL_FILE_EVENTS);
ins_long(d, "TIMER_EVENTS", TCL_TIMER_EVENTS);
ins_long(d, "IDLE_EVENTS", TCL_IDLE_EVENTS);
ins_long(d, "ALL_EVENTS", TCL_ALL_EVENTS);
ins_long(d, "DONT_WAIT", TCL_DONT_WAIT);
ins_string(d, "TK_VERSION", TK_VERSION);
ins_string(d, "TCL_VERSION", TCL_VERSION);
PyDict_SetItemString(d, "TkappType", (PyObject *)&Tkapp_Type);
Py_TYPE(&Tktt_Type) = &PyType_Type;
PyDict_SetItemString(d, "TkttType", (PyObject *)&Tktt_Type);
Py_TYPE(&PyTclObject_Type) = &PyType_Type;
PyDict_SetItemString(d, "Tcl_Obj", (PyObject *)&PyTclObject_Type);
#if defined(TK_AQUA)
Tk_MacOSXSetupTkNotifier();
#endif
Tcl_FindExecutable(Py_GetProgramName());
if (PyErr_Occurred())
return;
#if 0
Py_AtExit(Tcl_Finalize);
#endif
}