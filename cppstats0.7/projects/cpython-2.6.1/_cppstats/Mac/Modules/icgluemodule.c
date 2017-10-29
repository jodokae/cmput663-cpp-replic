#include "Python.h"
#include "pymactoolbox.h"
extern int ResObj_Convert(PyObject *, Handle *);
#include <Carbon/Carbon.h>
static PyObject *ErrorObject;
typedef struct {
PyObject_HEAD
ICInstance inst;
} iciobject;
static PyTypeObject Icitype;
static char ici_ICGetSeed__doc__[] =
"()->int; Returns int that changes when configuration does"
;
static PyObject *
ici_ICGetSeed(iciobject *self, PyObject *args) {
OSStatus err;
long seed;
if (!PyArg_ParseTuple(args, ""))
return NULL;
if ((err=ICGetSeed(self->inst, &seed)) != 0 )
return PyMac_Error(err);
return Py_BuildValue("i", (int)seed);
}
static char ici_ICBegin__doc__[] =
"(perm)->None; Lock config file for read/write"
;
static PyObject *
ici_ICBegin(iciobject *self, PyObject *args) {
OSStatus err;
int perm;
if (!PyArg_ParseTuple(args, "i", &perm))
return NULL;
if ((err=ICBegin(self->inst, (ICPerm)perm)) != 0 )
return PyMac_Error(err);
Py_INCREF(Py_None);
return Py_None;
}
static char ici_ICFindPrefHandle__doc__[] =
"(key, handle)->attrs; Lookup key, store result in handle, return attributes"
;
static PyObject *
ici_ICFindPrefHandle(iciobject *self, PyObject *args) {
OSStatus err;
Str255 key;
ICAttr attr;
Handle h;
if (!PyArg_ParseTuple(args, "O&O&", PyMac_GetStr255, &key, ResObj_Convert, &h))
return NULL;
if ((err=ICFindPrefHandle(self->inst, key, &attr, h)) != 0 )
return PyMac_Error(err);
return Py_BuildValue("i", (int)attr);
}
static char ici_ICSetPref__doc__[] =
"(key, attr, data)->None; Set preference key to data with attributes"
;
static PyObject *
ici_ICSetPref(iciobject *self, PyObject *args) {
OSStatus err;
Str255 key;
int attr;
char *data;
int datalen;
if (!PyArg_ParseTuple(args, "O&is#", PyMac_GetStr255, &key, &attr,
&data, &datalen))
return NULL;
if ((err=ICSetPref(self->inst, key, (ICAttr)attr, (Ptr)data,
(long)datalen)) != 0)
return PyMac_Error(err);
Py_INCREF(Py_None);
return Py_None;
}
static char ici_ICCountPref__doc__[] =
"()->int; Return number of preferences"
;
static PyObject *
ici_ICCountPref(iciobject *self, PyObject *args) {
OSStatus err;
long count;
if (!PyArg_ParseTuple(args, ""))
return NULL;
if ((err=ICCountPref(self->inst, &count)) != 0 )
return PyMac_Error(err);
return Py_BuildValue("i", (int)count);
}
static char ici_ICGetIndPref__doc__[] =
"(num)->key; Return key of preference with given index"
;
static PyObject *
ici_ICGetIndPref(iciobject *self, PyObject *args) {
OSStatus err;
long num;
Str255 key;
if (!PyArg_ParseTuple(args, "l", &num))
return NULL;
if ((err=ICGetIndPref(self->inst, num, key)) != 0 )
return PyMac_Error(err);
return Py_BuildValue("O&", PyMac_BuildStr255, key);
}
static char ici_ICDeletePref__doc__[] =
"(key)->None; Delete preference"
;
static PyObject *
ici_ICDeletePref(iciobject *self, PyObject *args) {
OSStatus err;
Str255 key;
if (!PyArg_ParseTuple(args, "O&", PyMac_GetStr255, key))
return NULL;
if ((err=ICDeletePref(self->inst, key)) != 0 )
return PyMac_Error(err);
Py_INCREF(Py_None);
return Py_None;
}
static char ici_ICEnd__doc__[] =
"()->None; Unlock file after ICBegin call"
;
static PyObject *
ici_ICEnd(iciobject *self, PyObject *args) {
OSStatus err;
if (!PyArg_ParseTuple(args, ""))
return NULL;
if ((err=ICEnd(self->inst)) != 0 )
return PyMac_Error(err);
Py_INCREF(Py_None);
return Py_None;
}
static char ici_ICEditPreferences__doc__[] =
"(key)->None; Ask user to edit preferences, staring with key"
;
static PyObject *
ici_ICEditPreferences(iciobject *self, PyObject *args) {
OSStatus err;
Str255 key;
if (!PyArg_ParseTuple(args, "O&", PyMac_GetStr255, key))
return NULL;
if ((err=ICEditPreferences(self->inst, key)) != 0 )
return PyMac_Error(err);
Py_INCREF(Py_None);
return Py_None;
}
static char ici_ICParseURL__doc__[] =
"(hint, data, selStart, selEnd, handle)->selStart, selEnd; Find an URL, return in handle"
;
static PyObject *
ici_ICParseURL(iciobject *self, PyObject *args) {
OSStatus err;
Str255 hint;
char *data;
int datalen;
long selStart, selEnd;
Handle h;
if (!PyArg_ParseTuple(args, "O&s#llO&", PyMac_GetStr255, hint, &data, &datalen,
&selStart, &selEnd, ResObj_Convert, &h))
return NULL;
if ((err=ICParseURL(self->inst, hint, (Ptr)data, (long)datalen,
&selStart, &selEnd, h)) != 0 )
return PyMac_Error(err);
return Py_BuildValue("ii", (int)selStart, (int)selEnd);
}
static char ici_ICLaunchURL__doc__[] =
"(hint, data, selStart, selEnd)->None; Find an URL and launch the correct app"
;
static PyObject *
ici_ICLaunchURL(iciobject *self, PyObject *args) {
OSStatus err;
Str255 hint;
char *data;
int datalen;
long selStart, selEnd;
if (!PyArg_ParseTuple(args, "O&s#ll", PyMac_GetStr255, hint, &data, &datalen,
&selStart, &selEnd))
return NULL;
if ((err=ICLaunchURL(self->inst, hint, (Ptr)data, (long)datalen,
&selStart, &selEnd)) != 0 )
return PyMac_Error(err);
return Py_BuildValue("ii", (int)selStart, (int)selEnd);
}
static char ici_ICMapFilename__doc__[] =
"(filename)->mapinfo; Get filemap info for given filename"
;
static PyObject *
ici_ICMapFilename(iciobject *self, PyObject *args) {
OSStatus err;
Str255 filename;
ICMapEntry entry;
if (!PyArg_ParseTuple(args, "O&", PyMac_GetStr255, filename))
return NULL;
if ((err=ICMapFilename(self->inst, filename, &entry)) != 0 )
return PyMac_Error(err);
return Py_BuildValue("hO&O&O&lO&O&O&O&O&", entry.version,
PyMac_BuildOSType, entry.fileType,
PyMac_BuildOSType, entry.fileCreator,
PyMac_BuildOSType, entry.postCreator,
entry.flags,
PyMac_BuildStr255, entry.extension,
PyMac_BuildStr255, entry.creatorAppName,
PyMac_BuildStr255, entry.postAppName,
PyMac_BuildStr255, entry.MIMEType,
PyMac_BuildStr255, entry.entryName);
}
static char ici_ICMapTypeCreator__doc__[] =
"(type, creator, filename)->mapinfo; Get filemap info for given tp/cr/filename"
;
static PyObject *
ici_ICMapTypeCreator(iciobject *self, PyObject *args) {
OSStatus err;
OSType type, creator;
Str255 filename;
ICMapEntry entry;
if (!PyArg_ParseTuple(args, "O&O&O&",
PyMac_GetOSType, &type,
PyMac_GetOSType, &creator,
PyMac_GetStr255, filename))
return NULL;
if ((err=ICMapTypeCreator(self->inst, type, creator, filename, &entry)) != 0 )
return PyMac_Error(err);
return Py_BuildValue("hO&O&O&lO&O&O&O&O&", entry.version,
PyMac_BuildOSType, entry.fileType,
PyMac_BuildOSType, entry.fileCreator,
PyMac_BuildOSType, entry.postCreator,
entry.flags,
PyMac_BuildStr255, entry.extension,
PyMac_BuildStr255, entry.creatorAppName,
PyMac_BuildStr255, entry.postAppName,
PyMac_BuildStr255, entry.MIMEType,
PyMac_BuildStr255, entry.entryName);
}
static struct PyMethodDef ici_methods[] = {
{"ICGetSeed", (PyCFunction)ici_ICGetSeed, METH_VARARGS, ici_ICGetSeed__doc__},
{"ICBegin", (PyCFunction)ici_ICBegin, METH_VARARGS, ici_ICBegin__doc__},
{"ICFindPrefHandle", (PyCFunction)ici_ICFindPrefHandle, METH_VARARGS, ici_ICFindPrefHandle__doc__},
{"ICSetPref", (PyCFunction)ici_ICSetPref, METH_VARARGS, ici_ICSetPref__doc__},
{"ICCountPref", (PyCFunction)ici_ICCountPref, METH_VARARGS, ici_ICCountPref__doc__},
{"ICGetIndPref", (PyCFunction)ici_ICGetIndPref, METH_VARARGS, ici_ICGetIndPref__doc__},
{"ICDeletePref", (PyCFunction)ici_ICDeletePref, METH_VARARGS, ici_ICDeletePref__doc__},
{"ICEnd", (PyCFunction)ici_ICEnd, METH_VARARGS, ici_ICEnd__doc__},
{"ICEditPreferences", (PyCFunction)ici_ICEditPreferences, METH_VARARGS, ici_ICEditPreferences__doc__},
{"ICParseURL", (PyCFunction)ici_ICParseURL, METH_VARARGS, ici_ICParseURL__doc__},
{"ICLaunchURL", (PyCFunction)ici_ICLaunchURL, METH_VARARGS, ici_ICLaunchURL__doc__},
{"ICMapFilename", (PyCFunction)ici_ICMapFilename, METH_VARARGS, ici_ICMapFilename__doc__},
{"ICMapTypeCreator", (PyCFunction)ici_ICMapTypeCreator, METH_VARARGS, ici_ICMapTypeCreator__doc__},
{NULL, NULL}
};
static iciobject *
newiciobject(OSType creator) {
iciobject *self;
OSStatus err;
self = PyObject_NEW(iciobject, &Icitype);
if (self == NULL)
return NULL;
if ((err=ICStart(&self->inst, creator)) != 0 ) {
(void)PyMac_Error(err);
PyObject_DEL(self);
return NULL;
}
return self;
}
static void
ici_dealloc(iciobject *self) {
(void)ICStop(self->inst);
PyObject_DEL(self);
}
static PyObject *
ici_getattr(iciobject *self, char *name) {
return Py_FindMethod(ici_methods, (PyObject *)self, name);
}
static char Icitype__doc__[] =
"Internet Config instance"
;
static PyTypeObject Icitype = {
PyObject_HEAD_INIT(&PyType_Type)
0,
"icglue.ic_instance",
sizeof(iciobject),
0,
(destructor)ici_dealloc,
(printfunc)0,
(getattrfunc)ici_getattr,
(setattrfunc)0,
(cmpfunc)0,
(reprfunc)0,
0,
0,
0,
(hashfunc)0,
(ternaryfunc)0,
(reprfunc)0,
0L,0L,0L,0L,
Icitype__doc__
};
static char ic_ICStart__doc__[] =
"(OSType)->ic_instance; Create an Internet Config instance"
;
static PyObject *
ic_ICStart(PyObject *self, PyObject *args) {
OSType creator;
if (!PyArg_ParseTuple(args, "O&", PyMac_GetOSType, &creator))
return NULL;
return (PyObject *)newiciobject(creator);
}
static struct PyMethodDef ic_methods[] = {
{"ICStart", (PyCFunction)ic_ICStart, METH_VARARGS, ic_ICStart__doc__},
{NULL, (PyCFunction)NULL, 0, NULL}
};
static char icglue_module_documentation[] =
"Implements low-level Internet Config interface"
;
void
initicglue(void) {
PyObject *m, *d;
if (PyErr_WarnPy3k("In 3.x, icglue is removed.", 1))
return;
m = Py_InitModule4("icglue", ic_methods,
icglue_module_documentation,
(PyObject*)NULL,PYTHON_API_VERSION);
d = PyModule_GetDict(m);
ErrorObject = PyMac_GetOSErrException();
if (ErrorObject == NULL ||
PyDict_SetItemString(d, "error", ErrorObject) != 0)
return;
if (PyErr_Occurred())
Py_FatalError("can't initialize module icglue");
}