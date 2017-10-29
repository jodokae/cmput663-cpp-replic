#include "Python.h"
#include <float.h>
#include "structmember.h"
#if defined(WITH_THREAD)
#include "pythread.h"
#endif
static PyObject *TestError;
static PyObject *
raiseTestError(const char* test_name, const char* msg) {
char buf[2048];
if (strlen(test_name) + strlen(msg) > sizeof(buf) - 50)
PyErr_SetString(TestError, "internal error msg too large");
else {
PyOS_snprintf(buf, sizeof(buf), "%s: %s", test_name, msg);
PyErr_SetString(TestError, buf);
}
return NULL;
}
static PyObject*
sizeof_error(const char* fatname, const char* typname,
int expected, int got) {
char buf[1024];
PyOS_snprintf(buf, sizeof(buf),
"%.200s #define == %d but sizeof(%.200s) == %d",
fatname, expected, typname, got);
PyErr_SetString(TestError, buf);
return (PyObject*)NULL;
}
static PyObject*
test_config(PyObject *self) {
#define CHECK_SIZEOF(FATNAME, TYPE) if (FATNAME != sizeof(TYPE)) return sizeof_error(#FATNAME, #TYPE, FATNAME, sizeof(TYPE))
CHECK_SIZEOF(SIZEOF_SHORT, short);
CHECK_SIZEOF(SIZEOF_INT, int);
CHECK_SIZEOF(SIZEOF_LONG, long);
CHECK_SIZEOF(SIZEOF_VOID_P, void*);
CHECK_SIZEOF(SIZEOF_TIME_T, time_t);
#if defined(HAVE_LONG_LONG)
CHECK_SIZEOF(SIZEOF_LONG_LONG, PY_LONG_LONG);
#endif
#undef CHECK_SIZEOF
Py_INCREF(Py_None);
return Py_None;
}
static PyObject*
test_list_api(PyObject *self) {
PyObject* list;
int i;
#define NLIST 30
list = PyList_New(NLIST);
if (list == (PyObject*)NULL)
return (PyObject*)NULL;
for (i = 0; i < NLIST; ++i) {
PyObject* anint = PyInt_FromLong(i);
if (anint == (PyObject*)NULL) {
Py_DECREF(list);
return (PyObject*)NULL;
}
PyList_SET_ITEM(list, i, anint);
}
i = PyList_Reverse(list);
if (i != 0) {
Py_DECREF(list);
return (PyObject*)NULL;
}
for (i = 0; i < NLIST; ++i) {
PyObject* anint = PyList_GET_ITEM(list, i);
if (PyInt_AS_LONG(anint) != NLIST-1-i) {
PyErr_SetString(TestError,
"test_list_api: reverse screwed up");
Py_DECREF(list);
return (PyObject*)NULL;
}
}
Py_DECREF(list);
#undef NLIST
Py_INCREF(Py_None);
return Py_None;
}
static int
test_dict_inner(int count) {
Py_ssize_t pos = 0, iterations = 0;
int i;
PyObject *dict = PyDict_New();
PyObject *v, *k;
if (dict == NULL)
return -1;
for (i = 0; i < count; i++) {
v = PyInt_FromLong(i);
PyDict_SetItem(dict, v, v);
Py_DECREF(v);
}
while (PyDict_Next(dict, &pos, &k, &v)) {
PyObject *o;
iterations++;
i = PyInt_AS_LONG(v) + 1;
o = PyInt_FromLong(i);
if (o == NULL)
return -1;
if (PyDict_SetItem(dict, k, o) < 0) {
Py_DECREF(o);
return -1;
}
Py_DECREF(o);
}
Py_DECREF(dict);
if (iterations != count) {
PyErr_SetString(
TestError,
"test_dict_iteration: dict iteration went wrong ");
return -1;
} else {
return 0;
}
}
static PyObject*
test_dict_iteration(PyObject* self) {
int i;
for (i = 0; i < 200; i++) {
if (test_dict_inner(i) < 0) {
return NULL;
}
}
Py_INCREF(Py_None);
return Py_None;
}
#define UNBIND(X) Py_DECREF(X); (X) = NULL
static PyObject *
raise_test_long_error(const char* msg) {
return raiseTestError("test_long_api", msg);
}
#define TESTNAME test_long_api_inner
#define TYPENAME long
#define F_S_TO_PY PyLong_FromLong
#define F_PY_TO_S PyLong_AsLong
#define F_U_TO_PY PyLong_FromUnsignedLong
#define F_PY_TO_U PyLong_AsUnsignedLong
#include "testcapi_long.h"
static PyObject *
test_long_api(PyObject* self) {
return TESTNAME(raise_test_long_error);
}
#undef TESTNAME
#undef TYPENAME
#undef F_S_TO_PY
#undef F_PY_TO_S
#undef F_U_TO_PY
#undef F_PY_TO_U
#if defined(HAVE_LONG_LONG)
static PyObject *
raise_test_longlong_error(const char* msg) {
return raiseTestError("test_longlong_api", msg);
}
#define TESTNAME test_longlong_api_inner
#define TYPENAME PY_LONG_LONG
#define F_S_TO_PY PyLong_FromLongLong
#define F_PY_TO_S PyLong_AsLongLong
#define F_U_TO_PY PyLong_FromUnsignedLongLong
#define F_PY_TO_U PyLong_AsUnsignedLongLong
#include "testcapi_long.h"
static PyObject *
test_longlong_api(PyObject* self, PyObject *args) {
return TESTNAME(raise_test_longlong_error);
}
#undef TESTNAME
#undef TYPENAME
#undef F_S_TO_PY
#undef F_PY_TO_S
#undef F_U_TO_PY
#undef F_PY_TO_U
static PyObject *
test_L_code(PyObject *self) {
PyObject *tuple, *num;
PY_LONG_LONG value;
tuple = PyTuple_New(1);
if (tuple == NULL)
return NULL;
num = PyLong_FromLong(42);
if (num == NULL)
return NULL;
PyTuple_SET_ITEM(tuple, 0, num);
value = -1;
if (PyArg_ParseTuple(tuple, "L:test_L_code", &value) < 0)
return NULL;
if (value != 42)
return raiseTestError("test_L_code",
"L code returned wrong value for long 42");
Py_DECREF(num);
num = PyInt_FromLong(42);
if (num == NULL)
return NULL;
PyTuple_SET_ITEM(tuple, 0, num);
value = -1;
if (PyArg_ParseTuple(tuple, "L:test_L_code", &value) < 0)
return NULL;
if (value != 42)
return raiseTestError("test_L_code",
"L code returned wrong value for int 42");
Py_DECREF(tuple);
Py_INCREF(Py_None);
return Py_None;
}
#endif
static PyObject *
getargs_tuple(PyObject *self, PyObject *args) {
int a, b, c;
if (!PyArg_ParseTuple(args, "i(ii)", &a, &b, &c))
return NULL;
return Py_BuildValue("iii", a, b, c);
}
static PyObject *getargs_keywords(PyObject *self, PyObject *args, PyObject *kwargs) {
static char *keywords[] = {"arg1","arg2","arg3","arg4","arg5", NULL};
static char *fmt="(ii)i|(i(ii))(iii)i";
int int_args[10]= {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
if (!PyArg_ParseTupleAndKeywords(args, kwargs, fmt, keywords,
&int_args[0], &int_args[1], &int_args[2], &int_args[3], &int_args[4],
&int_args[5], &int_args[6], &int_args[7], &int_args[8], &int_args[9]))
return NULL;
return Py_BuildValue("iiiiiiiiii",
int_args[0], int_args[1], int_args[2], int_args[3], int_args[4],
int_args[5], int_args[6], int_args[7], int_args[8], int_args[9]);
}
static PyObject *
getargs_b(PyObject *self, PyObject *args) {
unsigned char value;
if (!PyArg_ParseTuple(args, "b", &value))
return NULL;
return PyLong_FromUnsignedLong((unsigned long)value);
}
static PyObject *
getargs_B(PyObject *self, PyObject *args) {
unsigned char value;
if (!PyArg_ParseTuple(args, "B", &value))
return NULL;
return PyLong_FromUnsignedLong((unsigned long)value);
}
static PyObject *
getargs_H(PyObject *self, PyObject *args) {
unsigned short value;
if (!PyArg_ParseTuple(args, "H", &value))
return NULL;
return PyLong_FromUnsignedLong((unsigned long)value);
}
static PyObject *
getargs_I(PyObject *self, PyObject *args) {
unsigned int value;
if (!PyArg_ParseTuple(args, "I", &value))
return NULL;
return PyLong_FromUnsignedLong((unsigned long)value);
}
static PyObject *
getargs_k(PyObject *self, PyObject *args) {
unsigned long value;
if (!PyArg_ParseTuple(args, "k", &value))
return NULL;
return PyLong_FromUnsignedLong(value);
}
static PyObject *
getargs_i(PyObject *self, PyObject *args) {
int value;
if (!PyArg_ParseTuple(args, "i", &value))
return NULL;
return PyLong_FromLong((long)value);
}
static PyObject *
getargs_l(PyObject *self, PyObject *args) {
long value;
if (!PyArg_ParseTuple(args, "l", &value))
return NULL;
return PyLong_FromLong(value);
}
static PyObject *
getargs_n(PyObject *self, PyObject *args) {
Py_ssize_t value;
if (!PyArg_ParseTuple(args, "n", &value))
return NULL;
return PyInt_FromSsize_t(value);
}
#if defined(HAVE_LONG_LONG)
static PyObject *
getargs_L(PyObject *self, PyObject *args) {
PY_LONG_LONG value;
if (!PyArg_ParseTuple(args, "L", &value))
return NULL;
return PyLong_FromLongLong(value);
}
static PyObject *
getargs_K(PyObject *self, PyObject *args) {
unsigned PY_LONG_LONG value;
if (!PyArg_ParseTuple(args, "K", &value))
return NULL;
return PyLong_FromUnsignedLongLong(value);
}
#endif
static PyObject *
test_k_code(PyObject *self) {
PyObject *tuple, *num;
unsigned long value;
tuple = PyTuple_New(1);
if (tuple == NULL)
return NULL;
num = PyLong_FromString("FFFFFFFFFFFFFFFFFFFFFFFF", NULL, 16);
if (num == NULL)
return NULL;
value = PyInt_AsUnsignedLongMask(num);
if (value != ULONG_MAX)
return raiseTestError("test_k_code",
"PyInt_AsUnsignedLongMask() returned wrong value for long 0xFFF...FFF");
PyTuple_SET_ITEM(tuple, 0, num);
value = 0;
if (PyArg_ParseTuple(tuple, "k:test_k_code", &value) < 0)
return NULL;
if (value != ULONG_MAX)
return raiseTestError("test_k_code",
"k code returned wrong value for long 0xFFF...FFF");
Py_DECREF(num);
num = PyLong_FromString("-FFFFFFFF000000000000000042", NULL, 16);
if (num == NULL)
return NULL;
value = PyInt_AsUnsignedLongMask(num);
if (value != (unsigned long)-0x42)
return raiseTestError("test_k_code",
"PyInt_AsUnsignedLongMask() returned wrong value for long 0xFFF...FFF");
PyTuple_SET_ITEM(tuple, 0, num);
value = 0;
if (PyArg_ParseTuple(tuple, "k:test_k_code", &value) < 0)
return NULL;
if (value != (unsigned long)-0x42)
return raiseTestError("test_k_code",
"k code returned wrong value for long -0xFFF..000042");
Py_DECREF(tuple);
Py_INCREF(Py_None);
return Py_None;
}
#if defined(Py_USING_UNICODE)
static PyObject *
test_u_code(PyObject *self) {
PyObject *tuple, *obj;
Py_UNICODE *value;
int len;
int x = Py_UNICODE_ISSPACE(25);
tuple = PyTuple_New(1);
if (tuple == NULL)
return NULL;
obj = PyUnicode_Decode("test", strlen("test"),
"ascii", NULL);
if (obj == NULL)
return NULL;
PyTuple_SET_ITEM(tuple, 0, obj);
value = 0;
if (PyArg_ParseTuple(tuple, "u:test_u_code", &value) < 0)
return NULL;
if (value != PyUnicode_AS_UNICODE(obj))
return raiseTestError("test_u_code",
"u code returned wrong value for u'test'");
value = 0;
if (PyArg_ParseTuple(tuple, "u#:test_u_code", &value, &len) < 0)
return NULL;
if (value != PyUnicode_AS_UNICODE(obj) ||
len != PyUnicode_GET_SIZE(obj))
return raiseTestError("test_u_code",
"u#code returned wrong values for u'test'");
Py_DECREF(tuple);
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
codec_incrementalencoder(PyObject *self, PyObject *args) {
const char *encoding, *errors = NULL;
if (!PyArg_ParseTuple(args, "s|s:test_incrementalencoder",
&encoding, &errors))
return NULL;
return PyCodec_IncrementalEncoder(encoding, errors);
}
static PyObject *
codec_incrementaldecoder(PyObject *self, PyObject *args) {
const char *encoding, *errors = NULL;
if (!PyArg_ParseTuple(args, "s|s:test_incrementaldecoder",
&encoding, &errors))
return NULL;
return PyCodec_IncrementalDecoder(encoding, errors);
}
#endif
static PyObject *
test_long_numbits(PyObject *self) {
struct triple {
long input;
size_t nbits;
int sign;
} testcases[] = {{0, 0, 0},
{1L, 1, 1},
{-1L, 1, -1},
{2L, 2, 1},
{-2L, 2, -1},
{3L, 2, 1},
{-3L, 2, -1},
{4L, 3, 1},
{-4L, 3, -1},
{0x7fffL, 15, 1},
{-0x7fffL, 15, -1},
{0xffffL, 16, 1},
{-0xffffL, 16, -1},
{0xfffffffL, 28, 1},
{-0xfffffffL, 28, -1}
};
int i;
for (i = 0; i < sizeof(testcases) / sizeof(struct triple); ++i) {
PyObject *plong = PyLong_FromLong(testcases[i].input);
size_t nbits = _PyLong_NumBits(plong);
int sign = _PyLong_Sign(plong);
Py_DECREF(plong);
if (nbits != testcases[i].nbits)
return raiseTestError("test_long_numbits",
"wrong result for _PyLong_NumBits");
if (sign != testcases[i].sign)
return raiseTestError("test_long_numbits",
"wrong result for _PyLong_Sign");
}
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
test_null_strings(PyObject *self) {
PyObject *o1 = PyObject_Str(NULL), *o2 = PyObject_Unicode(NULL);
PyObject *tuple = PyTuple_Pack(2, o1, o2);
Py_XDECREF(o1);
Py_XDECREF(o2);
return tuple;
}
static PyObject *
raise_exception(PyObject *self, PyObject *args) {
PyObject *exc;
PyObject *exc_args, *v;
int num_args, i;
if (!PyArg_ParseTuple(args, "Oi:raise_exception",
&exc, &num_args))
return NULL;
if (!PyExceptionClass_Check(exc)) {
PyErr_Format(PyExc_TypeError, "an exception class is required");
return NULL;
}
exc_args = PyTuple_New(num_args);
if (exc_args == NULL)
return NULL;
for (i = 0; i < num_args; ++i) {
v = PyInt_FromLong(i);
if (v == NULL) {
Py_DECREF(exc_args);
return NULL;
}
PyTuple_SET_ITEM(exc_args, i, v);
}
PyErr_SetObject(exc, exc_args);
Py_DECREF(exc_args);
return NULL;
}
#if defined(WITH_THREAD)
static PyThread_type_lock thread_done = NULL;
static int
_make_call(void *callable) {
PyObject *rc;
int success;
PyGILState_STATE s = PyGILState_Ensure();
rc = PyObject_CallFunction((PyObject *)callable, "");
success = (rc != NULL);
Py_XDECREF(rc);
PyGILState_Release(s);
return success;
}
static void
_make_call_from_thread(void *callable) {
_make_call(callable);
PyThread_release_lock(thread_done);
}
static PyObject *
test_thread_state(PyObject *self, PyObject *args) {
PyObject *fn;
int success = 1;
if (!PyArg_ParseTuple(args, "O:test_thread_state", &fn))
return NULL;
if (!PyCallable_Check(fn)) {
PyErr_Format(PyExc_TypeError, "'%s' object is not callable",
fn->ob_type->tp_name);
return NULL;
}
PyEval_InitThreads();
thread_done = PyThread_allocate_lock();
if (thread_done == NULL)
return PyErr_NoMemory();
PyThread_acquire_lock(thread_done, 1);
PyThread_start_new_thread(_make_call_from_thread, fn);
success &= _make_call(fn);
Py_BEGIN_ALLOW_THREADS
success &= _make_call(fn);
PyThread_acquire_lock(thread_done, 1);
Py_END_ALLOW_THREADS
Py_BEGIN_ALLOW_THREADS
PyThread_start_new_thread(_make_call_from_thread, fn);
success &= _make_call(fn);
PyThread_acquire_lock(thread_done, 1);
Py_END_ALLOW_THREADS
PyThread_release_lock(thread_done);
PyThread_free_lock(thread_done);
if (!success)
return NULL;
Py_RETURN_NONE;
}
#endif
static PyObject *
test_string_from_format(PyObject *self, PyObject *args) {
PyObject *result;
char *msg;
#define CHECK_1_FORMAT(FORMAT, TYPE) result = PyString_FromFormat(FORMAT, (TYPE)1); if (result == NULL) return NULL; if (strcmp(PyString_AsString(result), "1")) { msg = FORMAT " failed at 1"; goto Fail; } Py_DECREF(result)
CHECK_1_FORMAT("%d", int);
CHECK_1_FORMAT("%ld", long);
CHECK_1_FORMAT("%zd", Py_ssize_t);
CHECK_1_FORMAT("%u", unsigned int);
CHECK_1_FORMAT("%lu", unsigned long);
CHECK_1_FORMAT("%zu", size_t);
Py_RETURN_NONE;
Fail:
Py_XDECREF(result);
return raiseTestError("test_string_from_format", msg);
#undef CHECK_1_FORMAT
}
static PyObject *
test_with_docstring(PyObject *self) {
Py_RETURN_NONE;
}
static PyObject *
traceback_print(PyObject *self, PyObject *args) {
PyObject *file;
PyObject *traceback;
int result;
if (!PyArg_ParseTuple(args, "OO:traceback_print",
&traceback, &file))
return NULL;
result = PyTraceBack_Print(traceback, file);
if (result < 0)
return NULL;
Py_RETURN_NONE;
}
static PyMethodDef TestMethods[] = {
{"raise_exception", raise_exception, METH_VARARGS},
{"test_config", (PyCFunction)test_config, METH_NOARGS},
{"test_list_api", (PyCFunction)test_list_api, METH_NOARGS},
{"test_dict_iteration", (PyCFunction)test_dict_iteration,METH_NOARGS},
{"test_long_api", (PyCFunction)test_long_api, METH_NOARGS},
{"test_long_numbits", (PyCFunction)test_long_numbits, METH_NOARGS},
{"test_k_code", (PyCFunction)test_k_code, METH_NOARGS},
{"test_null_strings", (PyCFunction)test_null_strings, METH_NOARGS},
{"test_string_from_format", (PyCFunction)test_string_from_format, METH_NOARGS},
{
"test_with_docstring", (PyCFunction)test_with_docstring, METH_NOARGS,
PyDoc_STR("This is a pretty normal docstring.")
},
{"getargs_tuple", getargs_tuple, METH_VARARGS},
{
"getargs_keywords", (PyCFunction)getargs_keywords,
METH_VARARGS|METH_KEYWORDS
},
{"getargs_b", getargs_b, METH_VARARGS},
{"getargs_B", getargs_B, METH_VARARGS},
{"getargs_H", getargs_H, METH_VARARGS},
{"getargs_I", getargs_I, METH_VARARGS},
{"getargs_k", getargs_k, METH_VARARGS},
{"getargs_i", getargs_i, METH_VARARGS},
{"getargs_l", getargs_l, METH_VARARGS},
{"getargs_n", getargs_n, METH_VARARGS},
#if defined(HAVE_LONG_LONG)
{"getargs_L", getargs_L, METH_VARARGS},
{"getargs_K", getargs_K, METH_VARARGS},
{"test_longlong_api", test_longlong_api, METH_NOARGS},
{"test_L_code", (PyCFunction)test_L_code, METH_NOARGS},
{
"codec_incrementalencoder",
(PyCFunction)codec_incrementalencoder, METH_VARARGS
},
{
"codec_incrementaldecoder",
(PyCFunction)codec_incrementaldecoder, METH_VARARGS
},
#endif
#if defined(Py_USING_UNICODE)
{"test_u_code", (PyCFunction)test_u_code, METH_NOARGS},
#endif
#if defined(WITH_THREAD)
{"_test_thread_state", test_thread_state, METH_VARARGS},
#endif
{"traceback_print", traceback_print, METH_VARARGS},
{NULL, NULL}
};
#define AddSym(d, n, f, v) {PyObject *o = f(v); PyDict_SetItemString(d, n, o); Py_DECREF(o);}
typedef struct {
char bool_member;
char byte_member;
unsigned char ubyte_member;
short short_member;
unsigned short ushort_member;
int int_member;
unsigned int uint_member;
long long_member;
unsigned long ulong_member;
float float_member;
double double_member;
#if defined(HAVE_LONG_LONG)
PY_LONG_LONG longlong_member;
unsigned PY_LONG_LONG ulonglong_member;
#endif
} all_structmembers;
typedef struct {
PyObject_HEAD
all_structmembers structmembers;
} test_structmembers;
static struct PyMemberDef test_members[] = {
{"T_BOOL", T_BOOL, offsetof(test_structmembers, structmembers.bool_member), 0, NULL},
{"T_BYTE", T_BYTE, offsetof(test_structmembers, structmembers.byte_member), 0, NULL},
{"T_UBYTE", T_UBYTE, offsetof(test_structmembers, structmembers.ubyte_member), 0, NULL},
{"T_SHORT", T_SHORT, offsetof(test_structmembers, structmembers.short_member), 0, NULL},
{"T_USHORT", T_USHORT, offsetof(test_structmembers, structmembers.ushort_member), 0, NULL},
{"T_INT", T_INT, offsetof(test_structmembers, structmembers.int_member), 0, NULL},
{"T_UINT", T_UINT, offsetof(test_structmembers, structmembers.uint_member), 0, NULL},
{"T_LONG", T_LONG, offsetof(test_structmembers, structmembers.long_member), 0, NULL},
{"T_ULONG", T_ULONG, offsetof(test_structmembers, structmembers.ulong_member), 0, NULL},
{"T_FLOAT", T_FLOAT, offsetof(test_structmembers, structmembers.float_member), 0, NULL},
{"T_DOUBLE", T_DOUBLE, offsetof(test_structmembers, structmembers.double_member), 0, NULL},
#if defined(HAVE_LONG_LONG)
{"T_LONGLONG", T_LONGLONG, offsetof(test_structmembers, structmembers.longlong_member), 0, NULL},
{"T_ULONGLONG", T_ULONGLONG, offsetof(test_structmembers, structmembers.ulonglong_member), 0, NULL},
#endif
{NULL}
};
static PyObject *
test_structmembers_new(PyTypeObject *type, PyObject *args, PyObject *kwargs) {
static char *keywords[] = {
"T_BOOL", "T_BYTE", "T_UBYTE", "T_SHORT", "T_USHORT",
"T_INT", "T_UINT", "T_LONG", "T_ULONG",
"T_FLOAT", "T_DOUBLE",
#if defined(HAVE_LONG_LONG)
"T_LONGLONG", "T_ULONGLONG",
#endif
NULL
};
static char *fmt = "|bbBhHiIlkfd"
#if defined(HAVE_LONG_LONG)
"LK"
#endif
;
test_structmembers *ob;
ob = PyObject_New(test_structmembers, type);
if (ob == NULL)
return NULL;
memset(&ob->structmembers, 0, sizeof(all_structmembers));
if (!PyArg_ParseTupleAndKeywords(args, kwargs, fmt, keywords,
&ob->structmembers.bool_member,
&ob->structmembers.byte_member,
&ob->structmembers.ubyte_member,
&ob->structmembers.short_member,
&ob->structmembers.ushort_member,
&ob->structmembers.int_member,
&ob->structmembers.uint_member,
&ob->structmembers.long_member,
&ob->structmembers.ulong_member,
&ob->structmembers.float_member,
&ob->structmembers.double_member
#if defined(HAVE_LONG_LONG)
, &ob->structmembers.longlong_member,
&ob->structmembers.ulonglong_member
#endif
)) {
Py_DECREF(ob);
return NULL;
}
return (PyObject *)ob;
}
static void
test_structmembers_free(PyObject *ob) {
PyObject_FREE(ob);
}
static PyTypeObject test_structmembersType = {
PyVarObject_HEAD_INIT(NULL, 0)
"test_structmembersType",
sizeof(test_structmembers),
0,
test_structmembers_free,
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
PyObject_GenericSetAttr,
0,
0,
"Type containing all structmember types",
0,
0,
0,
0,
0,
0,
0,
test_members,
0,
0,
0,
0,
0,
0,
0,
0,
test_structmembers_new,
};
PyMODINIT_FUNC
init_testcapi(void) {
PyObject *m;
m = Py_InitModule("_testcapi", TestMethods);
if (m == NULL)
return;
Py_TYPE(&test_structmembersType)=&PyType_Type;
Py_INCREF(&test_structmembersType);
PyModule_AddObject(m, "test_structmembersType", (PyObject *)&test_structmembersType);
PyModule_AddObject(m, "CHAR_MAX", PyInt_FromLong(CHAR_MAX));
PyModule_AddObject(m, "CHAR_MIN", PyInt_FromLong(CHAR_MIN));
PyModule_AddObject(m, "UCHAR_MAX", PyInt_FromLong(UCHAR_MAX));
PyModule_AddObject(m, "SHRT_MAX", PyInt_FromLong(SHRT_MAX));
PyModule_AddObject(m, "SHRT_MIN", PyInt_FromLong(SHRT_MIN));
PyModule_AddObject(m, "USHRT_MAX", PyInt_FromLong(USHRT_MAX));
PyModule_AddObject(m, "INT_MAX", PyLong_FromLong(INT_MAX));
PyModule_AddObject(m, "INT_MIN", PyLong_FromLong(INT_MIN));
PyModule_AddObject(m, "UINT_MAX", PyLong_FromUnsignedLong(UINT_MAX));
PyModule_AddObject(m, "LONG_MAX", PyInt_FromLong(LONG_MAX));
PyModule_AddObject(m, "LONG_MIN", PyInt_FromLong(LONG_MIN));
PyModule_AddObject(m, "ULONG_MAX", PyLong_FromUnsignedLong(ULONG_MAX));
PyModule_AddObject(m, "FLT_MAX", PyFloat_FromDouble(FLT_MAX));
PyModule_AddObject(m, "FLT_MIN", PyFloat_FromDouble(FLT_MIN));
PyModule_AddObject(m, "DBL_MAX", PyFloat_FromDouble(DBL_MAX));
PyModule_AddObject(m, "DBL_MIN", PyFloat_FromDouble(DBL_MIN));
PyModule_AddObject(m, "LLONG_MAX", PyLong_FromLongLong(PY_LLONG_MAX));
PyModule_AddObject(m, "LLONG_MIN", PyLong_FromLongLong(PY_LLONG_MIN));
PyModule_AddObject(m, "ULLONG_MAX", PyLong_FromUnsignedLongLong(PY_ULLONG_MAX));
PyModule_AddObject(m, "PY_SSIZE_T_MAX", PyInt_FromSsize_t(PY_SSIZE_T_MAX));
PyModule_AddObject(m, "PY_SSIZE_T_MIN", PyInt_FromSsize_t(PY_SSIZE_T_MIN));
PyModule_AddObject(m, "SIZEOF_PYGC_HEAD", PyInt_FromSsize_t(sizeof(PyGC_Head)));
TestError = PyErr_NewException("_testcapi.error", NULL, NULL);
Py_INCREF(TestError);
PyModule_AddObject(m, "error", TestError);
}