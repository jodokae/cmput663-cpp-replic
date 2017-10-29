#define PY_SSIZE_T_CLEAN
#include "Python.h"
#include "structmember.h"
#include <openssl/evp.h>
#define MUNCH_SIZE INT_MAX
#if !defined(HASH_OBJ_CONSTRUCTOR)
#define HASH_OBJ_CONSTRUCTOR 0
#endif
typedef struct {
PyObject_HEAD
PyObject *name;
EVP_MD_CTX ctx;
} EVPobject;
static PyTypeObject EVPtype;
#define DEFINE_CONSTS_FOR_NEW(Name) static PyObject *CONST_ ##Name ##_name_obj; static EVP_MD_CTX CONST_new_ ##Name ##_ctx; static EVP_MD_CTX *CONST_new_ ##Name ##_ctx_p = NULL;
DEFINE_CONSTS_FOR_NEW(md5)
DEFINE_CONSTS_FOR_NEW(sha1)
DEFINE_CONSTS_FOR_NEW(sha224)
DEFINE_CONSTS_FOR_NEW(sha256)
DEFINE_CONSTS_FOR_NEW(sha384)
DEFINE_CONSTS_FOR_NEW(sha512)
static EVPobject *
newEVPobject(PyObject *name) {
EVPobject *retval = (EVPobject *)PyObject_New(EVPobject, &EVPtype);
if (retval != NULL) {
Py_INCREF(name);
retval->name = name;
}
return retval;
}
static void
EVP_dealloc(PyObject *ptr) {
EVP_MD_CTX_cleanup(&((EVPobject *)ptr)->ctx);
Py_XDECREF(((EVPobject *)ptr)->name);
PyObject_Del(ptr);
}
PyDoc_STRVAR(EVP_copy__doc__, "Return a copy of the hash object.");
static PyObject *
EVP_copy(EVPobject *self, PyObject *unused) {
EVPobject *newobj;
if ( (newobj = newEVPobject(self->name))==NULL)
return NULL;
EVP_MD_CTX_copy(&newobj->ctx, &self->ctx);
return (PyObject *)newobj;
}
PyDoc_STRVAR(EVP_digest__doc__,
"Return the digest value as a string of binary data.");
static PyObject *
EVP_digest(EVPobject *self, PyObject *unused) {
unsigned char digest[EVP_MAX_MD_SIZE];
EVP_MD_CTX temp_ctx;
PyObject *retval;
unsigned int digest_size;
EVP_MD_CTX_copy(&temp_ctx, &self->ctx);
digest_size = EVP_MD_CTX_size(&temp_ctx);
EVP_DigestFinal(&temp_ctx, digest, NULL);
retval = PyString_FromStringAndSize((const char *)digest, digest_size);
EVP_MD_CTX_cleanup(&temp_ctx);
return retval;
}
PyDoc_STRVAR(EVP_hexdigest__doc__,
"Return the digest value as a string of hexadecimal digits.");
static PyObject *
EVP_hexdigest(EVPobject *self, PyObject *unused) {
unsigned char digest[EVP_MAX_MD_SIZE];
EVP_MD_CTX temp_ctx;
PyObject *retval;
char *hex_digest;
unsigned int i, j, digest_size;
EVP_MD_CTX_copy(&temp_ctx, &self->ctx);
digest_size = EVP_MD_CTX_size(&temp_ctx);
EVP_DigestFinal(&temp_ctx, digest, NULL);
EVP_MD_CTX_cleanup(&temp_ctx);
retval = PyString_FromStringAndSize(NULL, digest_size * 2);
if (!retval)
return NULL;
hex_digest = PyString_AsString(retval);
if (!hex_digest) {
Py_DECREF(retval);
return NULL;
}
for(i=j=0; i<digest_size; i++) {
char c;
c = (digest[i] >> 4) & 0xf;
c = (c>9) ? c+'a'-10 : c + '0';
hex_digest[j++] = c;
c = (digest[i] & 0xf);
c = (c>9) ? c+'a'-10 : c + '0';
hex_digest[j++] = c;
}
return retval;
}
PyDoc_STRVAR(EVP_update__doc__,
"Update this hash object's state with the provided string.");
static PyObject *
EVP_update(EVPobject *self, PyObject *args) {
unsigned char *cp;
Py_ssize_t len;
if (!PyArg_ParseTuple(args, "s#:update", &cp, &len))
return NULL;
if (len > 0 && len <= MUNCH_SIZE) {
EVP_DigestUpdate(&self->ctx, cp, Py_SAFE_DOWNCAST(len, Py_ssize_t,
unsigned int));
} else {
Py_ssize_t offset = 0;
while (len) {
unsigned int process = len > MUNCH_SIZE ? MUNCH_SIZE : len;
EVP_DigestUpdate(&self->ctx, cp + offset, process);
len -= process;
offset += process;
}
}
Py_INCREF(Py_None);
return Py_None;
}
static PyMethodDef EVP_methods[] = {
{"update", (PyCFunction)EVP_update, METH_VARARGS, EVP_update__doc__},
{"digest", (PyCFunction)EVP_digest, METH_NOARGS, EVP_digest__doc__},
{"hexdigest", (PyCFunction)EVP_hexdigest, METH_NOARGS, EVP_hexdigest__doc__},
{"copy", (PyCFunction)EVP_copy, METH_NOARGS, EVP_copy__doc__},
{NULL, NULL}
};
static PyObject *
EVP_get_block_size(EVPobject *self, void *closure) {
return PyInt_FromLong(EVP_MD_CTX_block_size(&((EVPobject *)self)->ctx));
}
static PyObject *
EVP_get_digest_size(EVPobject *self, void *closure) {
return PyInt_FromLong(EVP_MD_CTX_size(&((EVPobject *)self)->ctx));
}
static PyMemberDef EVP_members[] = {
{"name", T_OBJECT, offsetof(EVPobject, name), READONLY, PyDoc_STR("algorithm name.")},
{NULL}
};
static PyGetSetDef EVP_getseters[] = {
{
"digest_size",
(getter)EVP_get_digest_size, NULL,
NULL,
NULL
},
{
"block_size",
(getter)EVP_get_block_size, NULL,
NULL,
NULL
},
{
"digestsize",
(getter)EVP_get_digest_size, NULL,
NULL,
NULL
},
{NULL}
};
static PyObject *
EVP_repr(PyObject *self) {
char buf[100];
PyOS_snprintf(buf, sizeof(buf), "<%s HASH object @ %p>",
PyString_AsString(((EVPobject *)self)->name), self);
return PyString_FromString(buf);
}
#if HASH_OBJ_CONSTRUCTOR
static int
EVP_tp_init(EVPobject *self, PyObject *args, PyObject *kwds) {
static char *kwlist[] = {"name", "string", NULL};
PyObject *name_obj = NULL;
char *nameStr;
unsigned char *cp = NULL;
Py_ssize_t len = 0;
const EVP_MD *digest;
if (!PyArg_ParseTupleAndKeywords(args, kwds, "O|s#:HASH", kwlist,
&name_obj, &cp, &len)) {
return -1;
}
if (!PyArg_Parse(name_obj, "s", &nameStr)) {
PyErr_SetString(PyExc_TypeError, "name must be a string");
return -1;
}
digest = EVP_get_digestbyname(nameStr);
if (!digest) {
PyErr_SetString(PyExc_ValueError, "unknown hash function");
return -1;
}
EVP_DigestInit(&self->ctx, digest);
self->name = name_obj;
Py_INCREF(self->name);
if (cp && len) {
if (len > 0 && len <= MUNCH_SIZE) {
EVP_DigestUpdate(&self->ctx, cp, Py_SAFE_DOWNCAST(len, Py_ssize_t,
unsigned int));
} else {
Py_ssize_t offset = 0;
while (len) {
unsigned int process = len > MUNCH_SIZE ? MUNCH_SIZE : len;
EVP_DigestUpdate(&self->ctx, cp + offset, process);
len -= process;
offset += process;
}
}
}
return 0;
}
#endif
PyDoc_STRVAR(hashtype_doc,
"A hash represents the object used to calculate a checksum of a\n\
string of information.\n\
\n\
Methods:\n\
\n\
update() -- updates the current digest with an additional string\n\
digest() -- return the current digest value\n\
hexdigest() -- return the current digest as a string of hexadecimal digits\n\
copy() -- return a copy of the current hash object\n\
\n\
Attributes:\n\
\n\
name -- the hash algorithm being used by this object\n\
digest_size -- number of bytes in this hashes output\n");
static PyTypeObject EVPtype = {
PyVarObject_HEAD_INIT(NULL, 0)
"_hashlib.HASH",
sizeof(EVPobject),
0,
EVP_dealloc,
0,
0,
0,
0,
EVP_repr,
0,
0,
0,
0,
0,
0,
0,
0,
0,
Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
hashtype_doc,
0,
0,
0,
0,
0,
0,
EVP_methods,
EVP_members,
EVP_getseters,
#if 1
0,
0,
0,
0,
0,
#endif
#if HASH_OBJ_CONSTRUCTOR
(initproc)EVP_tp_init,
#endif
};
static PyObject *
EVPnew(PyObject *name_obj,
const EVP_MD *digest, const EVP_MD_CTX *initial_ctx,
const unsigned char *cp, Py_ssize_t len) {
EVPobject *self;
if (!digest && !initial_ctx) {
PyErr_SetString(PyExc_ValueError, "unsupported hash type");
return NULL;
}
if ((self = newEVPobject(name_obj)) == NULL)
return NULL;
if (initial_ctx) {
EVP_MD_CTX_copy(&self->ctx, initial_ctx);
} else {
EVP_DigestInit(&self->ctx, digest);
}
if (cp && len) {
if (len > 0 && len <= MUNCH_SIZE) {
EVP_DigestUpdate(&self->ctx, cp, Py_SAFE_DOWNCAST(len, Py_ssize_t,
unsigned int));
} else {
Py_ssize_t offset = 0;
while (len) {
unsigned int process = len > MUNCH_SIZE ? MUNCH_SIZE : len;
EVP_DigestUpdate(&self->ctx, cp + offset, process);
len -= process;
offset += process;
}
}
}
return (PyObject *)self;
}
PyDoc_STRVAR(EVP_new__doc__,
"Return a new hash object using the named algorithm.\n\
An optional string argument may be provided and will be\n\
automatically hashed.\n\
\n\
The MD5 and SHA1 algorithms are always supported.\n");
static PyObject *
EVP_new(PyObject *self, PyObject *args, PyObject *kwdict) {
static char *kwlist[] = {"name", "string", NULL};
PyObject *name_obj = NULL;
char *name;
const EVP_MD *digest;
unsigned char *cp = NULL;
Py_ssize_t len = 0;
if (!PyArg_ParseTupleAndKeywords(args, kwdict, "O|s#:new", kwlist,
&name_obj, &cp, &len)) {
return NULL;
}
if (!PyArg_Parse(name_obj, "s", &name)) {
PyErr_SetString(PyExc_TypeError, "name must be a string");
return NULL;
}
digest = EVP_get_digestbyname(name);
return EVPnew(name_obj, digest, NULL, cp, len);
}
#define GEN_CONSTRUCTOR(NAME) static PyObject * EVP_new_ ##NAME (PyObject *self, PyObject *args) { unsigned char *cp = NULL; Py_ssize_t len = 0; if (!PyArg_ParseTuple(args, "|s#:" #NAME , &cp, &len)) { return NULL; } return EVPnew( CONST_ ##NAME ##_name_obj, NULL, CONST_new_ ##NAME ##_ctx_p, cp, len); }
#define CONSTRUCTOR_METH_DEF(NAME) {"openssl_" #NAME, (PyCFunction)EVP_new_ ##NAME, METH_VARARGS, PyDoc_STR("Returns a " #NAME " hash object; optionally initialized with a string") }
#define INIT_CONSTRUCTOR_CONSTANTS(NAME) do { CONST_ ##NAME ##_name_obj = PyString_FromString(#NAME); if (EVP_get_digestbyname(#NAME)) { CONST_new_ ##NAME ##_ctx_p = &CONST_new_ ##NAME ##_ctx; EVP_DigestInit(CONST_new_ ##NAME ##_ctx_p, EVP_get_digestbyname(#NAME)); } } while (0);
GEN_CONSTRUCTOR(md5)
GEN_CONSTRUCTOR(sha1)
GEN_CONSTRUCTOR(sha224)
GEN_CONSTRUCTOR(sha256)
GEN_CONSTRUCTOR(sha384)
GEN_CONSTRUCTOR(sha512)
static struct PyMethodDef EVP_functions[] = {
{"new", (PyCFunction)EVP_new, METH_VARARGS|METH_KEYWORDS, EVP_new__doc__},
CONSTRUCTOR_METH_DEF(md5),
CONSTRUCTOR_METH_DEF(sha1),
CONSTRUCTOR_METH_DEF(sha224),
CONSTRUCTOR_METH_DEF(sha256),
CONSTRUCTOR_METH_DEF(sha384),
CONSTRUCTOR_METH_DEF(sha512),
{NULL, NULL}
};
PyMODINIT_FUNC
init_hashlib(void) {
PyObject *m;
OpenSSL_add_all_digests();
Py_TYPE(&EVPtype) = &PyType_Type;
if (PyType_Ready(&EVPtype) < 0)
return;
m = Py_InitModule("_hashlib", EVP_functions);
if (m == NULL)
return;
#if HASH_OBJ_CONSTRUCTOR
Py_INCREF(&EVPtype);
PyModule_AddObject(m, "HASH", (PyObject *)&EVPtype);
#endif
INIT_CONSTRUCTOR_CONSTANTS(md5);
INIT_CONSTRUCTOR_CONSTANTS(sha1);
INIT_CONSTRUCTOR_CONSTANTS(sha224);
INIT_CONSTRUCTOR_CONSTANTS(sha256);
INIT_CONSTRUCTOR_CONSTANTS(sha384);
INIT_CONSTRUCTOR_CONSTANTS(sha512);
}
