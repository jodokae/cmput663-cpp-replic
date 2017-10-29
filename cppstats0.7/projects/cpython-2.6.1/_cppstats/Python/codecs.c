#include "Python.h"
#include <ctype.h>
static int _PyCodecRegistry_Init(void);
int PyCodec_Register(PyObject *search_function) {
PyInterpreterState *interp = PyThreadState_GET()->interp;
if (interp->codec_search_path == NULL && _PyCodecRegistry_Init())
goto onError;
if (search_function == NULL) {
PyErr_BadArgument();
goto onError;
}
if (!PyCallable_Check(search_function)) {
PyErr_SetString(PyExc_TypeError, "argument must be callable");
goto onError;
}
return PyList_Append(interp->codec_search_path, search_function);
onError:
return -1;
}
static
PyObject *normalizestring(const char *string) {
register size_t i;
size_t len = strlen(string);
char *p;
PyObject *v;
if (len > PY_SSIZE_T_MAX) {
PyErr_SetString(PyExc_OverflowError, "string is too large");
return NULL;
}
v = PyString_FromStringAndSize(NULL, len);
if (v == NULL)
return NULL;
p = PyString_AS_STRING(v);
for (i = 0; i < len; i++) {
register char ch = string[i];
if (ch == ' ')
ch = '-';
else
ch = tolower(Py_CHARMASK(ch));
p[i] = ch;
}
return v;
}
PyObject *_PyCodec_Lookup(const char *encoding) {
PyInterpreterState *interp;
PyObject *result, *args = NULL, *v;
Py_ssize_t i, len;
if (encoding == NULL) {
PyErr_BadArgument();
goto onError;
}
interp = PyThreadState_GET()->interp;
if (interp->codec_search_path == NULL && _PyCodecRegistry_Init())
goto onError;
v = normalizestring(encoding);
if (v == NULL)
goto onError;
PyString_InternInPlace(&v);
result = PyDict_GetItem(interp->codec_search_cache, v);
if (result != NULL) {
Py_INCREF(result);
Py_DECREF(v);
return result;
}
args = PyTuple_New(1);
if (args == NULL)
goto onError;
PyTuple_SET_ITEM(args,0,v);
len = PyList_Size(interp->codec_search_path);
if (len < 0)
goto onError;
if (len == 0) {
PyErr_SetString(PyExc_LookupError,
"no codec search functions registered: "
"can't find encoding");
goto onError;
}
for (i = 0; i < len; i++) {
PyObject *func;
func = PyList_GetItem(interp->codec_search_path, i);
if (func == NULL)
goto onError;
result = PyEval_CallObject(func, args);
if (result == NULL)
goto onError;
if (result == Py_None) {
Py_DECREF(result);
continue;
}
if (!PyTuple_Check(result) || PyTuple_GET_SIZE(result) != 4) {
PyErr_SetString(PyExc_TypeError,
"codec search functions must return 4-tuples");
Py_DECREF(result);
goto onError;
}
break;
}
if (i == len) {
PyErr_Format(PyExc_LookupError,
"unknown encoding: %s", encoding);
goto onError;
}
PyDict_SetItem(interp->codec_search_cache, v, result);
Py_DECREF(args);
return result;
onError:
Py_XDECREF(args);
return NULL;
}
static
PyObject *args_tuple(PyObject *object,
const char *errors) {
PyObject *args;
args = PyTuple_New(1 + (errors != NULL));
if (args == NULL)
return NULL;
Py_INCREF(object);
PyTuple_SET_ITEM(args,0,object);
if (errors) {
PyObject *v;
v = PyString_FromString(errors);
if (v == NULL) {
Py_DECREF(args);
return NULL;
}
PyTuple_SET_ITEM(args, 1, v);
}
return args;
}
static
PyObject *codec_getitem(const char *encoding, int index) {
PyObject *codecs;
PyObject *v;
codecs = _PyCodec_Lookup(encoding);
if (codecs == NULL)
return NULL;
v = PyTuple_GET_ITEM(codecs, index);
Py_DECREF(codecs);
Py_INCREF(v);
return v;
}
static
PyObject *codec_getincrementalcodec(const char *encoding,
const char *errors,
const char *attrname) {
PyObject *codecs, *ret, *inccodec;
codecs = _PyCodec_Lookup(encoding);
if (codecs == NULL)
return NULL;
inccodec = PyObject_GetAttrString(codecs, attrname);
Py_DECREF(codecs);
if (inccodec == NULL)
return NULL;
if (errors)
ret = PyObject_CallFunction(inccodec, "s", errors);
else
ret = PyObject_CallFunction(inccodec, NULL);
Py_DECREF(inccodec);
return ret;
}
static
PyObject *codec_getstreamcodec(const char *encoding,
PyObject *stream,
const char *errors,
const int index) {
PyObject *codecs, *streamcodec, *codeccls;
codecs = _PyCodec_Lookup(encoding);
if (codecs == NULL)
return NULL;
codeccls = PyTuple_GET_ITEM(codecs, index);
if (errors != NULL)
streamcodec = PyObject_CallFunction(codeccls, "Os", stream, errors);
else
streamcodec = PyObject_CallFunction(codeccls, "O", stream);
Py_DECREF(codecs);
return streamcodec;
}
PyObject *PyCodec_Encoder(const char *encoding) {
return codec_getitem(encoding, 0);
}
PyObject *PyCodec_Decoder(const char *encoding) {
return codec_getitem(encoding, 1);
}
PyObject *PyCodec_IncrementalEncoder(const char *encoding,
const char *errors) {
return codec_getincrementalcodec(encoding, errors, "incrementalencoder");
}
PyObject *PyCodec_IncrementalDecoder(const char *encoding,
const char *errors) {
return codec_getincrementalcodec(encoding, errors, "incrementaldecoder");
}
PyObject *PyCodec_StreamReader(const char *encoding,
PyObject *stream,
const char *errors) {
return codec_getstreamcodec(encoding, stream, errors, 2);
}
PyObject *PyCodec_StreamWriter(const char *encoding,
PyObject *stream,
const char *errors) {
return codec_getstreamcodec(encoding, stream, errors, 3);
}
PyObject *PyCodec_Encode(PyObject *object,
const char *encoding,
const char *errors) {
PyObject *encoder = NULL;
PyObject *args = NULL, *result = NULL;
PyObject *v;
encoder = PyCodec_Encoder(encoding);
if (encoder == NULL)
goto onError;
args = args_tuple(object, errors);
if (args == NULL)
goto onError;
result = PyEval_CallObject(encoder,args);
if (result == NULL)
goto onError;
if (!PyTuple_Check(result) ||
PyTuple_GET_SIZE(result) != 2) {
PyErr_SetString(PyExc_TypeError,
"encoder must return a tuple (object,integer)");
goto onError;
}
v = PyTuple_GET_ITEM(result,0);
Py_INCREF(v);
Py_DECREF(args);
Py_DECREF(encoder);
Py_DECREF(result);
return v;
onError:
Py_XDECREF(result);
Py_XDECREF(args);
Py_XDECREF(encoder);
return NULL;
}
PyObject *PyCodec_Decode(PyObject *object,
const char *encoding,
const char *errors) {
PyObject *decoder = NULL;
PyObject *args = NULL, *result = NULL;
PyObject *v;
decoder = PyCodec_Decoder(encoding);
if (decoder == NULL)
goto onError;
args = args_tuple(object, errors);
if (args == NULL)
goto onError;
result = PyEval_CallObject(decoder,args);
if (result == NULL)
goto onError;
if (!PyTuple_Check(result) ||
PyTuple_GET_SIZE(result) != 2) {
PyErr_SetString(PyExc_TypeError,
"decoder must return a tuple (object,integer)");
goto onError;
}
v = PyTuple_GET_ITEM(result,0);
Py_INCREF(v);
Py_DECREF(args);
Py_DECREF(decoder);
Py_DECREF(result);
return v;
onError:
Py_XDECREF(args);
Py_XDECREF(decoder);
Py_XDECREF(result);
return NULL;
}
int PyCodec_RegisterError(const char *name, PyObject *error) {
PyInterpreterState *interp = PyThreadState_GET()->interp;
if (interp->codec_search_path == NULL && _PyCodecRegistry_Init())
return -1;
if (!PyCallable_Check(error)) {
PyErr_SetString(PyExc_TypeError, "handler must be callable");
return -1;
}
return PyDict_SetItemString(interp->codec_error_registry,
(char *)name, error);
}
PyObject *PyCodec_LookupError(const char *name) {
PyObject *handler = NULL;
PyInterpreterState *interp = PyThreadState_GET()->interp;
if (interp->codec_search_path == NULL && _PyCodecRegistry_Init())
return NULL;
if (name==NULL)
name = "strict";
handler = PyDict_GetItemString(interp->codec_error_registry, (char *)name);
if (!handler)
PyErr_Format(PyExc_LookupError, "unknown error handler name '%.400s'", name);
else
Py_INCREF(handler);
return handler;
}
static void wrong_exception_type(PyObject *exc) {
PyObject *type = PyObject_GetAttrString(exc, "__class__");
if (type != NULL) {
PyObject *name = PyObject_GetAttrString(type, "__name__");
Py_DECREF(type);
if (name != NULL) {
PyObject *string = PyObject_Str(name);
Py_DECREF(name);
if (string != NULL) {
PyErr_Format(PyExc_TypeError,
"don't know how to handle %.400s in error callback",
PyString_AS_STRING(string));
Py_DECREF(string);
}
}
}
}
PyObject *PyCodec_StrictErrors(PyObject *exc) {
if (PyExceptionInstance_Check(exc))
PyErr_SetObject(PyExceptionInstance_Class(exc), exc);
else
PyErr_SetString(PyExc_TypeError, "codec must pass exception instance");
return NULL;
}
#if defined(Py_USING_UNICODE)
PyObject *PyCodec_IgnoreErrors(PyObject *exc) {
Py_ssize_t end;
if (PyObject_IsInstance(exc, PyExc_UnicodeEncodeError)) {
if (PyUnicodeEncodeError_GetEnd(exc, &end))
return NULL;
} else if (PyObject_IsInstance(exc, PyExc_UnicodeDecodeError)) {
if (PyUnicodeDecodeError_GetEnd(exc, &end))
return NULL;
} else if (PyObject_IsInstance(exc, PyExc_UnicodeTranslateError)) {
if (PyUnicodeTranslateError_GetEnd(exc, &end))
return NULL;
} else {
wrong_exception_type(exc);
return NULL;
}
return Py_BuildValue("(u#n)", &end, 0, end);
}
PyObject *PyCodec_ReplaceErrors(PyObject *exc) {
PyObject *restuple;
Py_ssize_t start;
Py_ssize_t end;
Py_ssize_t i;
if (PyObject_IsInstance(exc, PyExc_UnicodeEncodeError)) {
PyObject *res;
Py_UNICODE *p;
if (PyUnicodeEncodeError_GetStart(exc, &start))
return NULL;
if (PyUnicodeEncodeError_GetEnd(exc, &end))
return NULL;
res = PyUnicode_FromUnicode(NULL, end-start);
if (res == NULL)
return NULL;
for (p = PyUnicode_AS_UNICODE(res), i = start;
i<end; ++p, ++i)
*p = '?';
restuple = Py_BuildValue("(On)", res, end);
Py_DECREF(res);
return restuple;
} else if (PyObject_IsInstance(exc, PyExc_UnicodeDecodeError)) {
Py_UNICODE res = Py_UNICODE_REPLACEMENT_CHARACTER;
if (PyUnicodeDecodeError_GetEnd(exc, &end))
return NULL;
return Py_BuildValue("(u#n)", &res, 1, end);
} else if (PyObject_IsInstance(exc, PyExc_UnicodeTranslateError)) {
PyObject *res;
Py_UNICODE *p;
if (PyUnicodeTranslateError_GetStart(exc, &start))
return NULL;
if (PyUnicodeTranslateError_GetEnd(exc, &end))
return NULL;
res = PyUnicode_FromUnicode(NULL, end-start);
if (res == NULL)
return NULL;
for (p = PyUnicode_AS_UNICODE(res), i = start;
i<end; ++p, ++i)
*p = Py_UNICODE_REPLACEMENT_CHARACTER;
restuple = Py_BuildValue("(On)", res, end);
Py_DECREF(res);
return restuple;
} else {
wrong_exception_type(exc);
return NULL;
}
}
PyObject *PyCodec_XMLCharRefReplaceErrors(PyObject *exc) {
if (PyObject_IsInstance(exc, PyExc_UnicodeEncodeError)) {
PyObject *restuple;
PyObject *object;
Py_ssize_t start;
Py_ssize_t end;
PyObject *res;
Py_UNICODE *p;
Py_UNICODE *startp;
Py_UNICODE *outp;
int ressize;
if (PyUnicodeEncodeError_GetStart(exc, &start))
return NULL;
if (PyUnicodeEncodeError_GetEnd(exc, &end))
return NULL;
if (!(object = PyUnicodeEncodeError_GetObject(exc)))
return NULL;
startp = PyUnicode_AS_UNICODE(object);
for (p = startp+start, ressize = 0; p < startp+end; ++p) {
if (*p<10)
ressize += 2+1+1;
else if (*p<100)
ressize += 2+2+1;
else if (*p<1000)
ressize += 2+3+1;
else if (*p<10000)
ressize += 2+4+1;
#if !defined(Py_UNICODE_WIDE)
else
ressize += 2+5+1;
#else
else if (*p<100000)
ressize += 2+5+1;
else if (*p<1000000)
ressize += 2+6+1;
else
ressize += 2+7+1;
#endif
}
res = PyUnicode_FromUnicode(NULL, ressize);
if (res == NULL) {
Py_DECREF(object);
return NULL;
}
for (p = startp+start, outp = PyUnicode_AS_UNICODE(res);
p < startp+end; ++p) {
Py_UNICODE c = *p;
int digits;
int base;
*outp++ = '&';
*outp++ = '#';
if (*p<10) {
digits = 1;
base = 1;
} else if (*p<100) {
digits = 2;
base = 10;
} else if (*p<1000) {
digits = 3;
base = 100;
} else if (*p<10000) {
digits = 4;
base = 1000;
}
#if !defined(Py_UNICODE_WIDE)
else {
digits = 5;
base = 10000;
}
#else
else if (*p<100000) {
digits = 5;
base = 10000;
} else if (*p<1000000) {
digits = 6;
base = 100000;
} else {
digits = 7;
base = 1000000;
}
#endif
while (digits-->0) {
*outp++ = '0' + c/base;
c %= base;
base /= 10;
}
*outp++ = ';';
}
restuple = Py_BuildValue("(On)", res, end);
Py_DECREF(res);
Py_DECREF(object);
return restuple;
} else {
wrong_exception_type(exc);
return NULL;
}
}
static Py_UNICODE hexdigits[] = {
'0', '1', '2', '3', '4', '5', '6', '7',
'8', '9', 'a', 'b', 'c', 'd', 'e', 'f'
};
PyObject *PyCodec_BackslashReplaceErrors(PyObject *exc) {
if (PyObject_IsInstance(exc, PyExc_UnicodeEncodeError)) {
PyObject *restuple;
PyObject *object;
Py_ssize_t start;
Py_ssize_t end;
PyObject *res;
Py_UNICODE *p;
Py_UNICODE *startp;
Py_UNICODE *outp;
int ressize;
if (PyUnicodeEncodeError_GetStart(exc, &start))
return NULL;
if (PyUnicodeEncodeError_GetEnd(exc, &end))
return NULL;
if (!(object = PyUnicodeEncodeError_GetObject(exc)))
return NULL;
startp = PyUnicode_AS_UNICODE(object);
for (p = startp+start, ressize = 0; p < startp+end; ++p) {
#if defined(Py_UNICODE_WIDE)
if (*p >= 0x00010000)
ressize += 1+1+8;
else
#endif
if (*p >= 0x100) {
ressize += 1+1+4;
} else
ressize += 1+1+2;
}
res = PyUnicode_FromUnicode(NULL, ressize);
if (res==NULL)
return NULL;
for (p = startp+start, outp = PyUnicode_AS_UNICODE(res);
p < startp+end; ++p) {
Py_UNICODE c = *p;
*outp++ = '\\';
#if defined(Py_UNICODE_WIDE)
if (c >= 0x00010000) {
*outp++ = 'U';
*outp++ = hexdigits[(c>>28)&0xf];
*outp++ = hexdigits[(c>>24)&0xf];
*outp++ = hexdigits[(c>>20)&0xf];
*outp++ = hexdigits[(c>>16)&0xf];
*outp++ = hexdigits[(c>>12)&0xf];
*outp++ = hexdigits[(c>>8)&0xf];
} else
#endif
if (c >= 0x100) {
*outp++ = 'u';
*outp++ = hexdigits[(c>>12)&0xf];
*outp++ = hexdigits[(c>>8)&0xf];
} else
*outp++ = 'x';
*outp++ = hexdigits[(c>>4)&0xf];
*outp++ = hexdigits[c&0xf];
}
restuple = Py_BuildValue("(On)", res, end);
Py_DECREF(res);
Py_DECREF(object);
return restuple;
} else {
wrong_exception_type(exc);
return NULL;
}
}
#endif
static PyObject *strict_errors(PyObject *self, PyObject *exc) {
return PyCodec_StrictErrors(exc);
}
#if defined(Py_USING_UNICODE)
static PyObject *ignore_errors(PyObject *self, PyObject *exc) {
return PyCodec_IgnoreErrors(exc);
}
static PyObject *replace_errors(PyObject *self, PyObject *exc) {
return PyCodec_ReplaceErrors(exc);
}
static PyObject *xmlcharrefreplace_errors(PyObject *self, PyObject *exc) {
return PyCodec_XMLCharRefReplaceErrors(exc);
}
static PyObject *backslashreplace_errors(PyObject *self, PyObject *exc) {
return PyCodec_BackslashReplaceErrors(exc);
}
#endif
static int _PyCodecRegistry_Init(void) {
static struct {
char *name;
PyMethodDef def;
} methods[] = {
{
"strict",
{
"strict_errors",
strict_errors,
METH_O
}
},
#if defined(Py_USING_UNICODE)
{
"ignore",
{
"ignore_errors",
ignore_errors,
METH_O
}
},
{
"replace",
{
"replace_errors",
replace_errors,
METH_O
}
},
{
"xmlcharrefreplace",
{
"xmlcharrefreplace_errors",
xmlcharrefreplace_errors,
METH_O
}
},
{
"backslashreplace",
{
"backslashreplace_errors",
backslashreplace_errors,
METH_O
}
}
#endif
};
PyInterpreterState *interp = PyThreadState_GET()->interp;
PyObject *mod;
unsigned i;
if (interp->codec_search_path != NULL)
return 0;
interp->codec_search_path = PyList_New(0);
interp->codec_search_cache = PyDict_New();
interp->codec_error_registry = PyDict_New();
if (interp->codec_error_registry) {
for (i = 0; i < sizeof(methods)/sizeof(methods[0]); ++i) {
PyObject *func = PyCFunction_New(&methods[i].def, NULL);
int res;
if (!func)
Py_FatalError("can't initialize codec error registry");
res = PyCodec_RegisterError(methods[i].name, func);
Py_DECREF(func);
if (res)
Py_FatalError("can't initialize codec error registry");
}
}
if (interp->codec_search_path == NULL ||
interp->codec_search_cache == NULL ||
interp->codec_error_registry == NULL)
Py_FatalError("can't initialize codec registry");
mod = PyImport_ImportModuleLevel("encodings", NULL, NULL, NULL, 0);
if (mod == NULL) {
if (PyErr_ExceptionMatches(PyExc_ImportError)) {
PyErr_Clear();
return 0;
}
return -1;
}
Py_DECREF(mod);
return 0;
}