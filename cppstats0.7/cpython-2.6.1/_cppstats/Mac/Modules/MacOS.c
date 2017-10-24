#include "Python.h"
#include "pymactoolbox.h"
#include <Carbon/Carbon.h>
#include <ApplicationServices/ApplicationServices.h>
#if !defined(HAVE_OSX105_SDK)
typedef SInt16 FSIORefNum;
#endif
static PyObject *MacOS_Error;
#define PATHNAMELEN 1024
typedef struct {
PyObject_HEAD
FSIORefNum fRefNum;
int isclosed;
} rfobject;
static PyTypeObject Rftype;
static void
do_close(rfobject *self) {
if (self->isclosed ) return;
(void)FSCloseFork(self->fRefNum);
self->isclosed = 1;
}
static char rf_read__doc__[] =
"Read data from resource fork"
;
static PyObject *
rf_read(rfobject *self, PyObject *args) {
long n;
PyObject *v;
OSErr err;
ByteCount n2;
if (self->isclosed) {
PyErr_SetString(PyExc_ValueError, "Operation on closed file");
return NULL;
}
if (!PyArg_ParseTuple(args, "l", &n))
return NULL;
v = PyBytes_FromStringAndSize((char *)NULL, n);
if (v == NULL)
return NULL;
err = FSReadFork(self->fRefNum, fsAtMark, 0, n, PyString_AsString(v), &n2);
if (err && err != eofErr) {
PyMac_Error(err);
Py_DECREF(v);
return NULL;
}
_PyString_Resize(&v, n2);
return v;
}
static char rf_write__doc__[] =
"Write to resource fork"
;
static PyObject *
rf_write(rfobject *self, PyObject *args) {
char *buffer;
long size;
OSErr err;
if (self->isclosed) {
PyErr_SetString(PyExc_ValueError, "Operation on closed file");
return NULL;
}
if (!PyArg_ParseTuple(args, "s#", &buffer, &size))
return NULL;
err = FSWriteFork(self->fRefNum, fsAtMark, 0, size, buffer, NULL);
if (err) {
PyMac_Error(err);
return NULL;
}
Py_INCREF(Py_None);
return Py_None;
}
static char rf_seek__doc__[] =
"Set file position"
;
static PyObject *
rf_seek(rfobject *self, PyObject *args) {
long amount;
int whence = SEEK_SET;
int mode;
OSErr err;
if (self->isclosed) {
PyErr_SetString(PyExc_ValueError, "Operation on closed file");
return NULL;
}
if (!PyArg_ParseTuple(args, "l|i", &amount, &whence)) {
return NULL;
}
switch (whence) {
case SEEK_CUR:
mode = fsFromMark;
break;
case SEEK_END:
mode = fsFromLEOF;
break;
case SEEK_SET:
mode = fsFromStart;
break;
default:
PyErr_BadArgument();
return NULL;
}
err = FSSetForkPosition(self->fRefNum, mode, amount);
if (err != noErr) {
PyMac_Error(err);
return NULL;
}
Py_INCREF(Py_None);
return Py_None;
}
static char rf_tell__doc__[] =
"Get file position"
;
static PyObject *
rf_tell(rfobject *self, PyObject *args) {
long long where;
OSErr err;
if (self->isclosed) {
PyErr_SetString(PyExc_ValueError, "Operation on closed file");
return NULL;
}
if (!PyArg_ParseTuple(args, ""))
return NULL;
err = FSGetForkPosition(self->fRefNum, &where);
if (err != noErr) {
PyMac_Error(err);
return NULL;
}
return PyLong_FromLongLong(where);
}
static char rf_close__doc__[] =
"Close resource fork"
;
static PyObject *
rf_close(rfobject *self, PyObject *args) {
if (!PyArg_ParseTuple(args, ""))
return NULL;
do_close(self);
Py_INCREF(Py_None);
return Py_None;
}
static struct PyMethodDef rf_methods[] = {
{"read", (PyCFunction)rf_read, 1, rf_read__doc__},
{"write", (PyCFunction)rf_write, 1, rf_write__doc__},
{"seek", (PyCFunction)rf_seek, 1, rf_seek__doc__},
{"tell", (PyCFunction)rf_tell, 1, rf_tell__doc__},
{"close", (PyCFunction)rf_close, 1, rf_close__doc__},
{NULL, NULL}
};
static rfobject *
newrfobject(void) {
rfobject *self;
self = PyObject_NEW(rfobject, &Rftype);
if (self == NULL)
return NULL;
self->isclosed = 1;
return self;
}
static void
rf_dealloc(rfobject *self) {
do_close(self);
PyObject_DEL(self);
}
static PyObject *
rf_getattr(rfobject *self, char *name) {
return Py_FindMethod(rf_methods, (PyObject *)self, name);
}
static char Rftype__doc__[] =
"Resource fork file object"
;
static PyTypeObject Rftype = {
PyObject_HEAD_INIT(&PyType_Type)
0,
"MacOS.ResourceFork",
sizeof(rfobject),
0,
(destructor)rf_dealloc,
(printfunc)0,
(getattrfunc)rf_getattr,
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
Rftype__doc__
};
static char getcrtp_doc[] = "Get MacOS 4-char creator and type for a file";
static PyObject *
MacOS_GetCreatorAndType(PyObject *self, PyObject *args) {
PyObject *creator, *type, *res;
OSErr err;
FSRef ref;
FSCatalogInfo cataloginfo;
FileInfo* finfo;
if (!PyArg_ParseTuple(args, "O&", PyMac_GetFSRef, &ref)) {
#if !defined(__LP64__)
PyErr_Clear();
FSSpec fss;
FInfo info;
if (!PyArg_ParseTuple(args, "O&", PyMac_GetFSSpec, &fss))
return NULL;
if ((err = FSpGetFInfo(&fss, &info)) != noErr) {
return PyErr_Mac(MacOS_Error, err);
}
creator = PyString_FromStringAndSize(
(char *)&info.fdCreator, 4);
type = PyString_FromStringAndSize((char *)&info.fdType, 4);
res = Py_BuildValue("OO", creator, type);
Py_DECREF(creator);
Py_DECREF(type);
return res;
#else
return NULL;
#endif
}
err = FSGetCatalogInfo(&ref,
kFSCatInfoFinderInfo|kFSCatInfoNodeFlags, &cataloginfo,
NULL, NULL, NULL);
if (err != noErr) {
PyErr_Mac(MacOS_Error, err);
return NULL;
}
if ((cataloginfo.nodeFlags & kFSNodeIsDirectoryMask) != 0) {
PyErr_Mac(MacOS_Error, fnfErr);
return NULL;
}
finfo = (FileInfo*)&(cataloginfo.finderInfo);
creator = PyString_FromStringAndSize((char*)&(finfo->fileCreator), 4);
type = PyString_FromStringAndSize((char*)&(finfo->fileType), 4);
res = Py_BuildValue("OO", creator, type);
Py_DECREF(creator);
Py_DECREF(type);
return res;
}
static char setcrtp_doc[] = "Set MacOS 4-char creator and type for a file";
static PyObject *
MacOS_SetCreatorAndType(PyObject *self, PyObject *args) {
ResType creator, type;
FSRef ref;
FileInfo* finfo;
OSErr err;
FSCatalogInfo cataloginfo;
if (!PyArg_ParseTuple(args, "O&O&O&",
PyMac_GetFSRef, &ref, PyMac_GetOSType, &creator, PyMac_GetOSType, &type)) {
#if !defined(__LP64__)
FSSpec fss;
FInfo info;
if (!PyArg_ParseTuple(args, "O&O&O&",
PyMac_GetFSSpec, &fss, PyMac_GetOSType, &creator, PyMac_GetOSType, &type))
return NULL;
if ((err = FSpGetFInfo(&fss, &info)) != noErr)
return PyErr_Mac(MacOS_Error, err);
info.fdCreator = creator;
info.fdType = type;
if ((err = FSpSetFInfo(&fss, &info)) != noErr)
return PyErr_Mac(MacOS_Error, err);
Py_INCREF(Py_None);
return Py_None;
#else
return NULL;
#endif
}
err = FSGetCatalogInfo(&ref,
kFSCatInfoFinderInfo|kFSCatInfoNodeFlags, &cataloginfo,
NULL, NULL, NULL);
if (err != noErr) {
PyErr_Mac(MacOS_Error, err);
return NULL;
}
if ((cataloginfo.nodeFlags & kFSNodeIsDirectoryMask) != 0) {
PyErr_Mac(MacOS_Error, fnfErr);
return NULL;
}
finfo = (FileInfo*)&(cataloginfo.finderInfo);
finfo->fileCreator = creator;
finfo->fileType = type;
err = FSSetCatalogInfo(&ref, kFSCatInfoFinderInfo, &cataloginfo);
if (err != noErr) {
PyErr_Mac(MacOS_Error, fnfErr);
return NULL;
}
Py_INCREF(Py_None);
return Py_None;
}
static char geterr_doc[] = "Convert OSErr number to string";
static PyObject *
MacOS_GetErrorString(PyObject *self, PyObject *args) {
int err;
char buf[256];
Handle h;
char *str;
static int errors_loaded;
if (!PyArg_ParseTuple(args, "i", &err))
return NULL;
h = GetResource('Estr', err);
if (!h && !errors_loaded) {
PyObject *m, *rv;
errors_loaded = 1;
m = PyImport_ImportModuleNoBlock("macresource");
if (!m) {
if (Py_VerboseFlag)
PyErr_Print();
PyErr_Clear();
} else {
rv = PyObject_CallMethod(m, "open_error_resource", "");
if (!rv) {
if (Py_VerboseFlag)
PyErr_Print();
PyErr_Clear();
} else {
Py_DECREF(rv);
h = GetResource('Estr', err);
}
Py_DECREF(m);
}
}
errors_loaded = 1;
if (h) {
HLock(h);
str = (char *)*h;
memcpy(buf, str+1, (unsigned char)str[0]);
buf[(unsigned char)str[0]] = '\0';
HUnlock(h);
ReleaseResource(h);
} else {
PyOS_snprintf(buf, sizeof(buf), "Mac OS error code %d", err);
}
return Py_BuildValue("s", buf);
}
#if !defined(__LP64__)
static char splash_doc[] = "Open a splash-screen dialog by resource-id (0=close)";
static PyObject *
MacOS_splash(PyObject *self, PyObject *args) {
int resid = -1;
static DialogPtr curdialog = NULL;
DialogPtr olddialog;
WindowRef theWindow;
CGrafPtr thePort;
#if 0
short xpos, ypos, width, height, swidth, sheight;
#endif
if (!PyArg_ParseTuple(args, "|i", &resid))
return NULL;
olddialog = curdialog;
curdialog = NULL;
if ( resid != -1 ) {
curdialog = GetNewDialog(resid, NULL, (WindowPtr)-1);
if ( curdialog ) {
theWindow = GetDialogWindow(curdialog);
thePort = GetWindowPort(theWindow);
#if 0
width = thePort->portRect.right - thePort->portRect.left;
height = thePort->portRect.bottom - thePort->portRect.top;
swidth = qd.screenBits.bounds.right - qd.screenBits.bounds.left;
sheight = qd.screenBits.bounds.bottom - qd.screenBits.bounds.top - LMGetMBarHeight();
xpos = (swidth-width)/2;
ypos = (sheight-height)/5 + LMGetMBarHeight();
MoveWindow(theWindow, xpos, ypos, 0);
ShowWindow(theWindow);
#endif
DrawDialog(curdialog);
}
}
if (olddialog)
DisposeDialog(olddialog);
Py_INCREF(Py_None);
return Py_None;
}
static char DebugStr_doc[] = "Switch to low-level debugger with a message";
static PyObject *
MacOS_DebugStr(PyObject *self, PyObject *args) {
Str255 message;
PyObject *object = 0;
if (!PyArg_ParseTuple(args, "O&|O", PyMac_GetStr255, message, &object))
return NULL;
DebugStr(message);
Py_INCREF(Py_None);
return Py_None;
}
static char SysBeep_doc[] = "BEEEEEP!!!";
static PyObject *
MacOS_SysBeep(PyObject *self, PyObject *args) {
int duration = 6;
if (!PyArg_ParseTuple(args, "|i", &duration))
return NULL;
SysBeep(duration);
Py_INCREF(Py_None);
return Py_None;
}
#endif
static char WMAvailable_doc[] =
"True if this process can interact with the display."
"Will foreground the application on the first call as a side-effect."
;
static PyObject *
MacOS_WMAvailable(PyObject *self, PyObject *args) {
static PyObject *rv = NULL;
if (!PyArg_ParseTuple(args, ""))
return NULL;
if (!rv) {
ProcessSerialNumber psn;
#if defined(kCGNullDirectDisplay)
if (CGMainDisplayID() == 0) {
rv = Py_False;
} else {
#else
{
#endif
if (GetCurrentProcess(&psn) < 0 ||
SetFrontProcess(&psn) < 0) {
rv = Py_False;
} else {
rv = Py_True;
}
}
}
Py_INCREF(rv);
return rv;
}
static char GetTicks_doc[] = "Return number of ticks since bootup";
static PyObject *
MacOS_GetTicks(PyObject *self, PyObject *args) {
return Py_BuildValue("i", (int)TickCount());
}
static char openrf_doc[] = "Open resource fork of a file";
static PyObject *
MacOS_openrf(PyObject *self, PyObject *args) {
OSErr err;
char *mode = "r";
FSRef ref;
SInt8 permission = fsRdPerm;
rfobject *fp;
HFSUniStr255 name;
if (!PyArg_ParseTuple(args, "O&|s", PyMac_GetFSRef, &ref, &mode))
return NULL;
while (*mode) {
switch (*mode++) {
case '*':
break;
case 'r':
permission = fsRdPerm;
break;
case 'w':
permission = fsWrPerm;
break;
case 'b':
break;
default:
PyErr_BadArgument();
return NULL;
}
}
err = FSGetResourceForkName(&name);
if (err != noErr) {
PyMac_Error(err);
return NULL;
}
if ( (fp = newrfobject()) == NULL )
return NULL;
err = FSOpenFork(&ref, name.length, name.unicode, permission, &fp->fRefNum);
if (err != noErr) {
Py_DECREF(fp);
PyMac_Error(err);
return NULL;
}
fp->isclosed = 0;
return (PyObject *)fp;
}
static PyMethodDef MacOS_Methods[] = {
{"GetCreatorAndType", MacOS_GetCreatorAndType, 1, getcrtp_doc},
{"SetCreatorAndType", MacOS_SetCreatorAndType, 1, setcrtp_doc},
{"GetErrorString", MacOS_GetErrorString, 1, geterr_doc},
{"openrf", MacOS_openrf, 1, openrf_doc},
#if !defined(__LP64__)
{"splash", MacOS_splash, 1, splash_doc},
{"DebugStr", MacOS_DebugStr, 1, DebugStr_doc},
{"SysBeep", MacOS_SysBeep, 1, SysBeep_doc},
#endif
{"GetTicks", MacOS_GetTicks, 1, GetTicks_doc},
{"WMAvailable", MacOS_WMAvailable, 1, WMAvailable_doc},
{NULL, NULL}
};
void
initMacOS(void) {
PyObject *m, *d;
if (PyErr_WarnPy3k("In 3.x, MacOS is removed.", 1))
return;
m = Py_InitModule("MacOS", MacOS_Methods);
d = PyModule_GetDict(m);
MacOS_Error = PyMac_GetOSErrException();
if (MacOS_Error == NULL || PyDict_SetItemString(d, "Error", MacOS_Error) != 0)
return;
Rftype.ob_type = &PyType_Type;
Py_INCREF(&Rftype);
if (PyDict_SetItemString(d, "ResourceForkType", (PyObject *)&Rftype) != 0)
return;
{
PyStringObject *p = 0;
long off = (long)&(p->ob_sval[0]);
if( PyDict_SetItemString(d, "string_id_to_buffer", Py_BuildValue("i", off)) != 0)
return;
}
#define PY_RUNTIMEMODEL "macho"
if (PyDict_SetItemString(d, "runtimemodel",
Py_BuildValue("s", PY_RUNTIMEMODEL)) != 0)
return;
#if defined(WITH_NEXT_FRAMEWORK)
#define PY_LINKMODEL "framework"
#elif defined(Py_ENABLE_SHARED)
#define PY_LINKMODEL "shared"
#else
#define PY_LINKMODEL "static"
#endif
if (PyDict_SetItemString(d, "linkmodel",
Py_BuildValue("s", PY_LINKMODEL)) != 0)
return;
}