#include "Python.h"
#define WINDOWS_LEAN_AND_MEAN
#include "windows.h"
typedef struct {
PyObject_HEAD
HANDLE handle;
} sp_handle_object;
staticforward PyTypeObject sp_handle_type;
static PyObject*
sp_handle_new(HANDLE handle) {
sp_handle_object* self;
self = PyObject_NEW(sp_handle_object, &sp_handle_type);
if (self == NULL)
return NULL;
self->handle = handle;
return (PyObject*) self;
}
#if defined(MS_WIN32) && !defined(MS_WIN64)
#define HANDLE_TO_PYNUM(handle) PyInt_FromLong((long) handle)
#define PY_HANDLE_PARAM "l"
#else
#define HANDLE_TO_PYNUM(handle) PyLong_FromLongLong((long long) handle)
#define PY_HANDLE_PARAM "L"
#endif
static PyObject*
sp_handle_detach(sp_handle_object* self, PyObject* args) {
HANDLE handle;
if (! PyArg_ParseTuple(args, ":Detach"))
return NULL;
handle = self->handle;
self->handle = NULL;
return HANDLE_TO_PYNUM(handle);
}
static PyObject*
sp_handle_close(sp_handle_object* self, PyObject* args) {
if (! PyArg_ParseTuple(args, ":Close"))
return NULL;
if (self->handle != INVALID_HANDLE_VALUE) {
CloseHandle(self->handle);
self->handle = INVALID_HANDLE_VALUE;
}
Py_INCREF(Py_None);
return Py_None;
}
static void
sp_handle_dealloc(sp_handle_object* self) {
if (self->handle != INVALID_HANDLE_VALUE)
CloseHandle(self->handle);
PyObject_FREE(self);
}
static PyMethodDef sp_handle_methods[] = {
{"Detach", (PyCFunction) sp_handle_detach, METH_VARARGS},
{"Close", (PyCFunction) sp_handle_close, METH_VARARGS},
{NULL, NULL}
};
static PyObject*
sp_handle_getattr(sp_handle_object* self, char* name) {
return Py_FindMethod(sp_handle_methods, (PyObject*) self, name);
}
static PyObject*
sp_handle_as_int(sp_handle_object* self) {
return HANDLE_TO_PYNUM(self->handle);
}
static PyNumberMethods sp_handle_as_number;
statichere PyTypeObject sp_handle_type = {
PyObject_HEAD_INIT(NULL)
0,
"_subprocess_handle", sizeof(sp_handle_object), 0,
(destructor) sp_handle_dealloc,
0,
(getattrfunc) sp_handle_getattr,
0,
0,
0,
&sp_handle_as_number,
0,
0,
0
};
static PyObject *
sp_GetStdHandle(PyObject* self, PyObject* args) {
HANDLE handle;
int std_handle;
if (! PyArg_ParseTuple(args, "i:GetStdHandle", &std_handle))
return NULL;
Py_BEGIN_ALLOW_THREADS
handle = GetStdHandle((DWORD) std_handle);
Py_END_ALLOW_THREADS
if (handle == INVALID_HANDLE_VALUE)
return PyErr_SetFromWindowsErr(GetLastError());
if (! handle) {
Py_INCREF(Py_None);
return Py_None;
}
return HANDLE_TO_PYNUM(handle);
}
static PyObject *
sp_GetCurrentProcess(PyObject* self, PyObject* args) {
if (! PyArg_ParseTuple(args, ":GetCurrentProcess"))
return NULL;
return sp_handle_new(GetCurrentProcess());
}
static PyObject *
sp_DuplicateHandle(PyObject* self, PyObject* args) {
HANDLE target_handle;
BOOL result;
HANDLE source_process_handle;
HANDLE source_handle;
HANDLE target_process_handle;
int desired_access;
int inherit_handle;
int options = 0;
if (! PyArg_ParseTuple(args,
PY_HANDLE_PARAM PY_HANDLE_PARAM PY_HANDLE_PARAM
"ii|i:DuplicateHandle",
&source_process_handle,
&source_handle,
&target_process_handle,
&desired_access,
&inherit_handle,
&options))
return NULL;
Py_BEGIN_ALLOW_THREADS
result = DuplicateHandle(
source_process_handle,
source_handle,
target_process_handle,
&target_handle,
desired_access,
inherit_handle,
options
);
Py_END_ALLOW_THREADS
if (! result)
return PyErr_SetFromWindowsErr(GetLastError());
return sp_handle_new(target_handle);
}
static PyObject *
sp_CreatePipe(PyObject* self, PyObject* args) {
HANDLE read_pipe;
HANDLE write_pipe;
BOOL result;
PyObject* pipe_attributes;
int size;
if (! PyArg_ParseTuple(args, "Oi:CreatePipe", &pipe_attributes, &size))
return NULL;
Py_BEGIN_ALLOW_THREADS
result = CreatePipe(&read_pipe, &write_pipe, NULL, size);
Py_END_ALLOW_THREADS
if (! result)
return PyErr_SetFromWindowsErr(GetLastError());
return Py_BuildValue(
"NN", sp_handle_new(read_pipe), sp_handle_new(write_pipe));
}
static int
getint(PyObject* obj, char* name) {
PyObject* value;
int ret;
value = PyObject_GetAttrString(obj, name);
if (! value) {
PyErr_Clear();
return 0;
}
ret = (int) PyInt_AsLong(value);
Py_DECREF(value);
return ret;
}
static HANDLE
gethandle(PyObject* obj, char* name) {
sp_handle_object* value;
HANDLE ret;
value = (sp_handle_object*) PyObject_GetAttrString(obj, name);
if (! value) {
PyErr_Clear();
return NULL;
}
if (value->ob_type != &sp_handle_type)
ret = NULL;
else
ret = value->handle;
Py_DECREF(value);
return ret;
}
static PyObject*
getenvironment(PyObject* environment) {
int i, envsize;
PyObject* out = NULL;
PyObject* keys;
PyObject* values;
char* p;
if (! PyMapping_Check(environment)) {
PyErr_SetString(
PyExc_TypeError, "environment must be dictionary or None");
return NULL;
}
envsize = PyMapping_Length(environment);
keys = PyMapping_Keys(environment);
values = PyMapping_Values(environment);
if (!keys || !values)
goto error;
out = PyString_FromStringAndSize(NULL, 2048);
if (! out)
goto error;
p = PyString_AS_STRING(out);
for (i = 0; i < envsize; i++) {
int ksize, vsize, totalsize;
PyObject* key = PyList_GET_ITEM(keys, i);
PyObject* value = PyList_GET_ITEM(values, i);
if (! PyString_Check(key) || ! PyString_Check(value)) {
PyErr_SetString(PyExc_TypeError,
"environment can only contain strings");
goto error;
}
ksize = PyString_GET_SIZE(key);
vsize = PyString_GET_SIZE(value);
totalsize = (p - PyString_AS_STRING(out)) + ksize + 1 +
vsize + 1 + 1;
if (totalsize > PyString_GET_SIZE(out)) {
int offset = p - PyString_AS_STRING(out);
_PyString_Resize(&out, totalsize + 1024);
p = PyString_AS_STRING(out) + offset;
}
memcpy(p, PyString_AS_STRING(key), ksize);
p += ksize;
*p++ = '=';
memcpy(p, PyString_AS_STRING(value), vsize);
p += vsize;
*p++ = '\0';
}
*p++ = '\0';
_PyString_Resize(&out, p - PyString_AS_STRING(out));
Py_XDECREF(keys);
Py_XDECREF(values);
return out;
error:
Py_XDECREF(out);
Py_XDECREF(keys);
Py_XDECREF(values);
return NULL;
}
static PyObject *
sp_CreateProcess(PyObject* self, PyObject* args) {
BOOL result;
PROCESS_INFORMATION pi;
STARTUPINFO si;
PyObject* environment;
char* application_name;
char* command_line;
PyObject* process_attributes;
PyObject* thread_attributes;
int inherit_handles;
int creation_flags;
PyObject* env_mapping;
char* current_directory;
PyObject* startup_info;
if (! PyArg_ParseTuple(args, "zzOOiiOzO:CreateProcess",
&application_name,
&command_line,
&process_attributes,
&thread_attributes,
&inherit_handles,
&creation_flags,
&env_mapping,
&current_directory,
&startup_info))
return NULL;
ZeroMemory(&si, sizeof(si));
si.cb = sizeof(si);
si.dwFlags = getint(startup_info, "dwFlags");
si.wShowWindow = getint(startup_info, "wShowWindow");
si.hStdInput = gethandle(startup_info, "hStdInput");
si.hStdOutput = gethandle(startup_info, "hStdOutput");
si.hStdError = gethandle(startup_info, "hStdError");
if (PyErr_Occurred())
return NULL;
if (env_mapping == Py_None)
environment = NULL;
else {
environment = getenvironment(env_mapping);
if (! environment)
return NULL;
}
Py_BEGIN_ALLOW_THREADS
result = CreateProcess(application_name,
command_line,
NULL,
NULL,
inherit_handles,
creation_flags,
environment ? PyString_AS_STRING(environment) : NULL,
current_directory,
&si,
&pi);
Py_END_ALLOW_THREADS
Py_XDECREF(environment);
if (! result)
return PyErr_SetFromWindowsErr(GetLastError());
return Py_BuildValue("NNii",
sp_handle_new(pi.hProcess),
sp_handle_new(pi.hThread),
pi.dwProcessId,
pi.dwThreadId);
}
static PyObject *
sp_TerminateProcess(PyObject* self, PyObject* args) {
BOOL result;
HANDLE process;
int exit_code;
if (! PyArg_ParseTuple(args, PY_HANDLE_PARAM "i:TerminateProcess",
&process, &exit_code))
return NULL;
result = TerminateProcess(process, exit_code);
if (! result)
return PyErr_SetFromWindowsErr(GetLastError());
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
sp_GetExitCodeProcess(PyObject* self, PyObject* args) {
DWORD exit_code;
BOOL result;
HANDLE process;
if (! PyArg_ParseTuple(args, PY_HANDLE_PARAM ":GetExitCodeProcess", &process))
return NULL;
result = GetExitCodeProcess(process, &exit_code);
if (! result)
return PyErr_SetFromWindowsErr(GetLastError());
return PyInt_FromLong(exit_code);
}
static PyObject *
sp_WaitForSingleObject(PyObject* self, PyObject* args) {
DWORD result;
HANDLE handle;
int milliseconds;
if (! PyArg_ParseTuple(args, PY_HANDLE_PARAM "i:WaitForSingleObject",
&handle,
&milliseconds))
return NULL;
Py_BEGIN_ALLOW_THREADS
result = WaitForSingleObject(handle, (DWORD) milliseconds);
Py_END_ALLOW_THREADS
if (result == WAIT_FAILED)
return PyErr_SetFromWindowsErr(GetLastError());
return PyInt_FromLong((int) result);
}
static PyObject *
sp_GetVersion(PyObject* self, PyObject* args) {
if (! PyArg_ParseTuple(args, ":GetVersion"))
return NULL;
return PyInt_FromLong((int) GetVersion());
}
static PyObject *
sp_GetModuleFileName(PyObject* self, PyObject* args) {
BOOL result;
HMODULE module;
TCHAR filename[MAX_PATH];
if (! PyArg_ParseTuple(args, PY_HANDLE_PARAM ":GetModuleFileName",
&module))
return NULL;
result = GetModuleFileName(module, filename, MAX_PATH);
filename[MAX_PATH-1] = '\0';
if (! result)
return PyErr_SetFromWindowsErr(GetLastError());
return PyString_FromString(filename);
}
static PyMethodDef sp_functions[] = {
{"GetStdHandle", sp_GetStdHandle, METH_VARARGS},
{"GetCurrentProcess", sp_GetCurrentProcess, METH_VARARGS},
{"DuplicateHandle", sp_DuplicateHandle, METH_VARARGS},
{"CreatePipe", sp_CreatePipe, METH_VARARGS},
{"CreateProcess", sp_CreateProcess, METH_VARARGS},
{"TerminateProcess", sp_TerminateProcess, METH_VARARGS},
{"GetExitCodeProcess", sp_GetExitCodeProcess, METH_VARARGS},
{"WaitForSingleObject", sp_WaitForSingleObject, METH_VARARGS},
{"GetVersion", sp_GetVersion, METH_VARARGS},
{"GetModuleFileName", sp_GetModuleFileName, METH_VARARGS},
{NULL, NULL}
};
static void
defint(PyObject* d, const char* name, int value) {
PyObject* v = PyInt_FromLong((long) value);
if (v) {
PyDict_SetItemString(d, (char*) name, v);
Py_DECREF(v);
}
}
#if PY_VERSION_HEX >= 0x02030000
PyMODINIT_FUNC
#else
DL_EXPORT(void)
#endif
init_subprocess() {
PyObject *d;
PyObject *m;
sp_handle_type.ob_type = &PyType_Type;
sp_handle_as_number.nb_int = (unaryfunc) sp_handle_as_int;
m = Py_InitModule("_subprocess", sp_functions);
if (m == NULL)
return;
d = PyModule_GetDict(m);
defint(d, "STD_INPUT_HANDLE", STD_INPUT_HANDLE);
defint(d, "STD_OUTPUT_HANDLE", STD_OUTPUT_HANDLE);
defint(d, "STD_ERROR_HANDLE", STD_ERROR_HANDLE);
defint(d, "DUPLICATE_SAME_ACCESS", DUPLICATE_SAME_ACCESS);
defint(d, "STARTF_USESTDHANDLES", STARTF_USESTDHANDLES);
defint(d, "STARTF_USESHOWWINDOW", STARTF_USESHOWWINDOW);
defint(d, "SW_HIDE", SW_HIDE);
defint(d, "INFINITE", INFINITE);
defint(d, "WAIT_OBJECT_0", WAIT_OBJECT_0);
defint(d, "CREATE_NEW_CONSOLE", CREATE_NEW_CONSOLE);
}
