#include "Python.h"
#include "pymactoolbox.h"
#include <Sound.h>
#pragma options align=mac68k
struct SampleRateAvailable_arg {
short numrates;
Handle rates;
};
struct SampleSizeAvailable_arg {
short numsizes;
Handle sizes;
};
#pragma options align=reset
static PyObject *ErrorObject;
static int
PyMac_GetUFixed(PyObject *v, Fixed *f) {
double d;
unsigned long uns;
if( !PyArg_Parse(v, "d", &d))
return 0;
uns = (unsigned long)(d * 0x10000);
*f = (Fixed)uns;
return 1;
}
static PyObject *
PyMac_BuildUFixed(Fixed f) {
double d;
unsigned long funs;
funs = (unsigned long)f;
d = funs;
d = d / 0x10000;
return Py_BuildValue("d", d);
}
static char sndih_getChannelAvailable__doc__[] =
""
;
static PyObject *
sndih_getChannelAvailable(self, args)
PyObject *self;
PyObject *args;
{
long inRefNum;
short nchannel;
OSErr err;
if (!PyArg_ParseTuple(args, "l", &inRefNum))
return NULL;
if( (err=SPBGetDeviceInfo(inRefNum, siChannelAvailable, (Ptr)&nchannel)) != noErr )
return PyMac_Error(err);
return Py_BuildValue("h", nchannel);
}
static char sndih_getNumberChannels__doc__[] =
""
;
static PyObject *
sndih_getNumberChannels(self, args)
PyObject *self;
PyObject *args;
{
long inRefNum;
short nchannel;
OSErr err;
if (!PyArg_ParseTuple(args, "l", &inRefNum))
return NULL;
if( (err=SPBGetDeviceInfo(inRefNum, siNumberChannels, (Ptr)&nchannel)) != noErr )
return PyMac_Error(err);
return Py_BuildValue("h", nchannel);
}
static char sndih_setNumberChannels__doc__[] =
""
;
static PyObject *
sndih_setNumberChannels(self, args)
PyObject *self;
PyObject *args;
{
long inRefNum;
short nchannel;
OSErr err;
if (!PyArg_ParseTuple(args, "lh", &inRefNum, &nchannel))
return NULL;
if( (err=SPBSetDeviceInfo(inRefNum, siNumberChannels, (Ptr)&nchannel)) != noErr )
return PyMac_Error(err);
Py_INCREF(Py_None);
return Py_None;
}
static char sndih_getContinuous__doc__[] =
""
;
static PyObject *
sndih_getContinuous(self, args)
PyObject *self;
PyObject *args;
{
long inRefNum;
short onoff;
OSErr err;
if (!PyArg_ParseTuple(args, "l", &inRefNum))
return NULL;
if( (err=SPBGetDeviceInfo(inRefNum, siContinuous, (Ptr)&onoff)) != noErr )
return PyMac_Error(err);
return Py_BuildValue("h", onoff);
}
static char sndih_setContinuous__doc__[] =
""
;
static PyObject *
sndih_setContinuous(self, args)
PyObject *self;
PyObject *args;
{
long inRefNum;
short onoff;
OSErr err;
if (!PyArg_ParseTuple(args, "lh", &inRefNum, &onoff))
return NULL;
if( (err=SPBSetDeviceInfo(inRefNum, siContinuous, (Ptr)&onoff)) != noErr )
return PyMac_Error(err);
Py_INCREF(Py_None);
return Py_None;
}
static char sndih_getInputSourceNames__doc__[] =
""
;
static PyObject *
sndih_getInputSourceNames(self, args)
PyObject *self;
PyObject *args;
{
long inRefNum;
Handle names;
OSErr err;
if (!PyArg_ParseTuple(args, "l", &inRefNum))
return NULL;
if( (err=SPBGetDeviceInfo(inRefNum, siInputSourceNames, (Ptr)&names)) != noErr )
return PyMac_Error(err);
return Py_BuildValue("O&", ResObj_New, names);
}
static char sndih_getInputSource__doc__[] =
""
;
static PyObject *
sndih_getInputSource(self, args)
PyObject *self;
PyObject *args;
{
long inRefNum;
short source;
OSErr err;
if (!PyArg_ParseTuple(args, "l", &inRefNum))
return NULL;
if( (err=SPBGetDeviceInfo(inRefNum, siInputSource, (Ptr)&source)) != noErr )
return PyMac_Error(err);
return Py_BuildValue("h", source);
}
static char sndih_setInputSource__doc__[] =
""
;
static PyObject *
sndih_setInputSource(self, args)
PyObject *self;
PyObject *args;
{
long inRefNum;
short source;
OSErr err;
if (!PyArg_ParseTuple(args, "lh", &inRefNum, &source))
return NULL;
if( (err=SPBSetDeviceInfo(inRefNum, siInputSource, (Ptr)&source)) != noErr )
return PyMac_Error(err);
Py_INCREF(Py_None);
return Py_None;
}
static char sndih_getPlayThruOnOff__doc__[] =
""
;
static PyObject *
sndih_getPlayThruOnOff(self, args)
PyObject *self;
PyObject *args;
{
long inRefNum;
short onoff;
OSErr err;
if (!PyArg_ParseTuple(args, "l", &inRefNum))
return NULL;
if( (err=SPBGetDeviceInfo(inRefNum, siPlayThruOnOff, (Ptr)&onoff)) != noErr )
return PyMac_Error(err);
return Py_BuildValue("h", onoff);
}
static char sndih_setPlayThruOnOff__doc__[] =
""
;
static PyObject *
sndih_setPlayThruOnOff(self, args)
PyObject *self;
PyObject *args;
{
long inRefNum;
short onoff;
OSErr err;
if (!PyArg_ParseTuple(args, "lh", &inRefNum, &onoff))
return NULL;
if( (err=SPBSetDeviceInfo(inRefNum, siPlayThruOnOff, (Ptr)&onoff)) != noErr )
return PyMac_Error(err);
Py_INCREF(Py_None);
return Py_None;
}
static char sndih_getSampleRate__doc__[] =
""
;
static PyObject *
sndih_getSampleRate(self, args)
PyObject *self;
PyObject *args;
{
long inRefNum;
Fixed sample_rate;
OSErr err;
if (!PyArg_ParseTuple(args, "l", &inRefNum))
return NULL;
if( (err=SPBGetDeviceInfo(inRefNum, siSampleRate, (Ptr)&sample_rate)) != noErr )
return PyMac_Error(err);
return Py_BuildValue("O&", PyMac_BuildUFixed, sample_rate);
}
static char sndih_setSampleRate__doc__[] =
""
;
static PyObject *
sndih_setSampleRate(self, args)
PyObject *self;
PyObject *args;
{
long inRefNum;
Fixed sample_rate;
OSErr err;
if (!PyArg_ParseTuple(args, "lO&", &inRefNum, PyMac_GetUFixed, &sample_rate))
return NULL;
if( (err=SPBSetDeviceInfo(inRefNum, siSampleRate, (Ptr)&sample_rate)) != noErr )
return PyMac_Error(err);
Py_INCREF(Py_None);
return Py_None;
}
static char sndih_getSampleSize__doc__[] =
""
;
static PyObject *
sndih_getSampleSize(self, args)
PyObject *self;
PyObject *args;
{
long inRefNum;
short bits;
OSErr err;
if (!PyArg_ParseTuple(args, "l", &inRefNum))
return NULL;
if( (err=SPBGetDeviceInfo(inRefNum, siSampleSize, (Ptr)&bits)) != noErr )
return PyMac_Error(err);
return Py_BuildValue("h", bits);
}
static char sndih_setSampleSize__doc__[] =
""
;
static PyObject *
sndih_setSampleSize(self, args)
PyObject *self;
PyObject *args;
{
long inRefNum;
short size;
OSErr err;
if (!PyArg_ParseTuple(args, "lh", &inRefNum, &size))
return NULL;
if( (err=SPBSetDeviceInfo(inRefNum, siSampleSize, (Ptr)&size)) != noErr )
return PyMac_Error(err);
Py_INCREF(Py_None);
return Py_None;
}
static char sndih_getSampleSizeAvailable__doc__[] =
""
;
static PyObject *
sndih_getSampleSizeAvailable(self, args)
PyObject *self;
PyObject *args;
{
long inRefNum;
struct SampleSizeAvailable_arg arg;
OSErr err;
PyObject *rsizes;
short *fsizes;
int i;
arg.sizes = NULL;
rsizes = NULL;
if (!PyArg_ParseTuple(args, "l", &inRefNum))
return NULL;
if( (err=SPBGetDeviceInfo(inRefNum, siSampleSizeAvailable, (Ptr)&arg)) != noErr ) {
return PyMac_Error(err);
}
fsizes = (short *)*(arg.sizes);
if( (rsizes = PyTuple_New(arg.numsizes)) == NULL)
return NULL;
for( i=0; i<arg.numsizes; i++ )
PyTuple_SetItem(rsizes, i, PyInt_FromLong((long)fsizes[i]));
return rsizes;
}
static char sndih_getSampleRateAvailable__doc__[] =
""
;
static PyObject *
sndih_getSampleRateAvailable(self, args)
PyObject *self;
PyObject *args;
{
long inRefNum;
struct SampleRateAvailable_arg arg;
OSErr err;
PyObject *rrates, *obj;
Fixed *frates;
int i;
arg.rates = NULL;
rrates = NULL;
if (!PyArg_ParseTuple(args, "l", &inRefNum))
return NULL;
if( (err=SPBGetDeviceInfo(inRefNum, siSampleRateAvailable, (Ptr)&arg)) != noErr ) {
return PyMac_Error(err);
}
frates = (Fixed *)*(arg.rates);
if( arg.numrates == 0 ) {
rrates = Py_BuildValue("O&O&", frates[0], frates[1]);
if (rrates == NULL) return NULL;
} else {
if( (rrates = PyTuple_New(arg.numrates)) == NULL)
return NULL;
for( i=0; i<arg.numrates; i++ ) {
if( (obj = Py_BuildValue("O&", PyMac_BuildUFixed, frates[i]))==NULL)
goto out;
PyTuple_SetItem(rrates, i, obj);
}
}
return Py_BuildValue("hO", arg.numrates, rrates);
out:
Py_XDECREF(rrates);
return NULL;
}
static struct PyMethodDef sndih_methods[] = {
{"getChannelAvailable", (PyCFunction)sndih_getChannelAvailable, METH_VARARGS, sndih_getChannelAvailable__doc__},
{"getNumberChannels", (PyCFunction)sndih_getNumberChannels, METH_VARARGS, sndih_getNumberChannels__doc__},
{"setNumberChannels", (PyCFunction)sndih_setNumberChannels, METH_VARARGS, sndih_setNumberChannels__doc__},
{"getContinuous", (PyCFunction)sndih_getContinuous, METH_VARARGS, sndih_getContinuous__doc__},
{"setContinuous", (PyCFunction)sndih_setContinuous, METH_VARARGS, sndih_setContinuous__doc__},
{"getInputSourceNames", (PyCFunction)sndih_getInputSourceNames, METH_VARARGS, sndih_getInputSourceNames__doc__},
{"getInputSource", (PyCFunction)sndih_getInputSource, METH_VARARGS, sndih_getInputSource__doc__},
{"setInputSource", (PyCFunction)sndih_setInputSource, METH_VARARGS, sndih_setInputSource__doc__},
{"getPlayThruOnOff", (PyCFunction)sndih_getPlayThruOnOff, METH_VARARGS, sndih_getPlayThruOnOff__doc__},
{"setPlayThruOnOff", (PyCFunction)sndih_setPlayThruOnOff, METH_VARARGS, sndih_setPlayThruOnOff__doc__},
{"getSampleRate", (PyCFunction)sndih_getSampleRate, METH_VARARGS, sndih_getSampleRate__doc__},
{"setSampleRate", (PyCFunction)sndih_setSampleRate, METH_VARARGS, sndih_setSampleRate__doc__},
{"getSampleSize", (PyCFunction)sndih_getSampleSize, METH_VARARGS, sndih_getSampleSize__doc__},
{"setSampleSize", (PyCFunction)sndih_setSampleSize, METH_VARARGS, sndih_setSampleSize__doc__},
{"getSampleSizeAvailable", (PyCFunction)sndih_getSampleSizeAvailable, METH_VARARGS, sndih_getSampleSizeAvailable__doc__},
{"getSampleRateAvailable", (PyCFunction)sndih_getSampleRateAvailable, METH_VARARGS, sndih_getSampleRateAvailable__doc__},
{NULL, (PyCFunction)NULL, 0, NULL}
};
static char Sndihooks_module_documentation[] =
""
;
void
init_Sndihooks() {
PyObject *m, *d;
m = Py_InitModule4("_Sndihooks", sndih_methods,
Sndihooks_module_documentation,
(PyObject*)NULL,PYTHON_API_VERSION);
d = PyModule_GetDict(m);
ErrorObject = PyString_FromString("Sndihooks.error");
PyDict_SetItemString(d, "error", ErrorObject);
if (PyErr_Occurred())
Py_FatalError("can't initialize module Sndihooks");
}