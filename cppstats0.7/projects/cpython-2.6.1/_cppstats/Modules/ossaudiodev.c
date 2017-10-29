#include "Python.h"
#include "structmember.h"
#if defined(HAVE_FCNTL_H)
#include <fcntl.h>
#else
#define O_RDONLY 00
#define O_WRONLY 01
#endif
#include <sys/ioctl.h>
#include <sys/soundcard.h>
#if defined(linux)
#if !defined(HAVE_STDINT_H)
typedef unsigned long uint32_t;
#endif
#elif defined(__FreeBSD__)
#if !defined(SNDCTL_DSP_CHANNELS)
#define SNDCTL_DSP_CHANNELS SOUND_PCM_WRITE_CHANNELS
#endif
#endif
typedef struct {
PyObject_HEAD
char *devicename;
int fd;
int mode;
int icount;
int ocount;
uint32_t afmts;
} oss_audio_t;
typedef struct {
PyObject_HEAD
int fd;
} oss_mixer_t;
static PyTypeObject OSSAudioType;
static PyTypeObject OSSMixerType;
static PyObject *OSSAudioError;
static oss_audio_t *
newossobject(PyObject *arg) {
oss_audio_t *self;
int fd, afmts, imode;
char *devicename = NULL;
char *mode = NULL;
if (!PyArg_ParseTuple(arg, "s|s:open", &devicename, &mode))
return NULL;
if (mode == NULL) {
mode = devicename;
devicename = NULL;
}
if (strcmp(mode, "r") == 0)
imode = O_RDONLY;
else if (strcmp(mode, "w") == 0)
imode = O_WRONLY;
else if (strcmp(mode, "rw") == 0)
imode = O_RDWR;
else {
PyErr_SetString(OSSAudioError, "mode must be 'r', 'w', or 'rw'");
return NULL;
}
if (devicename == NULL) {
devicename = getenv("AUDIODEV");
if (devicename == NULL)
devicename = "/dev/dsp";
}
if ((fd = open(devicename, imode|O_NONBLOCK)) == -1) {
PyErr_SetFromErrnoWithFilename(PyExc_IOError, devicename);
return NULL;
}
if (fcntl(fd, F_SETFL, 0) == -1) {
close(fd);
PyErr_SetFromErrnoWithFilename(PyExc_IOError, devicename);
return NULL;
}
if (ioctl(fd, SNDCTL_DSP_GETFMTS, &afmts) == -1) {
PyErr_SetFromErrnoWithFilename(PyExc_IOError, devicename);
return NULL;
}
if ((self = PyObject_New(oss_audio_t, &OSSAudioType)) == NULL) {
close(fd);
return NULL;
}
self->devicename = devicename;
self->fd = fd;
self->mode = imode;
self->icount = self->ocount = 0;
self->afmts = afmts;
return self;
}
static void
oss_dealloc(oss_audio_t *self) {
if (self->fd != -1)
close(self->fd);
PyObject_Del(self);
}
static oss_mixer_t *
newossmixerobject(PyObject *arg) {
char *devicename = NULL;
int fd;
oss_mixer_t *self;
if (!PyArg_ParseTuple(arg, "|s", &devicename)) {
return NULL;
}
if (devicename == NULL) {
devicename = getenv("MIXERDEV");
if (devicename == NULL)
devicename = "/dev/mixer";
}
if ((fd = open(devicename, O_RDWR)) == -1) {
PyErr_SetFromErrnoWithFilename(PyExc_IOError, devicename);
return NULL;
}
if ((self = PyObject_New(oss_mixer_t, &OSSMixerType)) == NULL) {
close(fd);
return NULL;
}
self->fd = fd;
return self;
}
static void
oss_mixer_dealloc(oss_mixer_t *self) {
if (self->fd != -1)
close(self->fd);
PyObject_Del(self);
}
static PyObject *
_do_ioctl_1(int fd, PyObject *args, char *fname, int cmd) {
char argfmt[33] = "i:";
int arg;
assert(strlen(fname) <= 30);
strcat(argfmt, fname);
if (!PyArg_ParseTuple(args, argfmt, &arg))
return NULL;
if (ioctl(fd, cmd, &arg) == -1)
return PyErr_SetFromErrno(PyExc_IOError);
return PyInt_FromLong(arg);
}
static PyObject *
_do_ioctl_1_internal(int fd, PyObject *args, char *fname, int cmd) {
char argfmt[32] = ":";
int arg = 0;
assert(strlen(fname) <= 30);
strcat(argfmt, fname);
if (!PyArg_ParseTuple(args, argfmt, &arg))
return NULL;
if (ioctl(fd, cmd, &arg) == -1)
return PyErr_SetFromErrno(PyExc_IOError);
return PyInt_FromLong(arg);
}
static PyObject *
_do_ioctl_0(int fd, PyObject *args, char *fname, int cmd) {
char argfmt[32] = ":";
int rv;
assert(strlen(fname) <= 30);
strcat(argfmt, fname);
if (!PyArg_ParseTuple(args, argfmt))
return NULL;
Py_BEGIN_ALLOW_THREADS
rv = ioctl(fd, cmd, 0);
Py_END_ALLOW_THREADS
if (rv == -1)
return PyErr_SetFromErrno(PyExc_IOError);
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
oss_nonblock(oss_audio_t *self, PyObject *unused) {
if (ioctl(self->fd, SNDCTL_DSP_NONBLOCK, NULL) == -1)
return PyErr_SetFromErrno(PyExc_IOError);
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
oss_setfmt(oss_audio_t *self, PyObject *args) {
return _do_ioctl_1(self->fd, args, "setfmt", SNDCTL_DSP_SETFMT);
}
static PyObject *
oss_getfmts(oss_audio_t *self, PyObject *unused) {
int mask;
if (ioctl(self->fd, SNDCTL_DSP_GETFMTS, &mask) == -1)
return PyErr_SetFromErrno(PyExc_IOError);
return PyInt_FromLong(mask);
}
static PyObject *
oss_channels(oss_audio_t *self, PyObject *args) {
return _do_ioctl_1(self->fd, args, "channels", SNDCTL_DSP_CHANNELS);
}
static PyObject *
oss_speed(oss_audio_t *self, PyObject *args) {
return _do_ioctl_1(self->fd, args, "speed", SNDCTL_DSP_SPEED);
}
static PyObject *
oss_sync(oss_audio_t *self, PyObject *args) {
return _do_ioctl_0(self->fd, args, "sync", SNDCTL_DSP_SYNC);
}
static PyObject *
oss_reset(oss_audio_t *self, PyObject *args) {
return _do_ioctl_0(self->fd, args, "reset", SNDCTL_DSP_RESET);
}
static PyObject *
oss_post(oss_audio_t *self, PyObject *args) {
return _do_ioctl_0(self->fd, args, "post", SNDCTL_DSP_POST);
}
static PyObject *
oss_read(oss_audio_t *self, PyObject *args) {
int size, count;
char *cp;
PyObject *rv;
if (!PyArg_ParseTuple(args, "i:read", &size))
return NULL;
rv = PyString_FromStringAndSize(NULL, size);
if (rv == NULL)
return NULL;
cp = PyString_AS_STRING(rv);
Py_BEGIN_ALLOW_THREADS
count = read(self->fd, cp, size);
Py_END_ALLOW_THREADS
if (count < 0) {
PyErr_SetFromErrno(PyExc_IOError);
Py_DECREF(rv);
return NULL;
}
self->icount += count;
_PyString_Resize(&rv, count);
return rv;
}
static PyObject *
oss_write(oss_audio_t *self, PyObject *args) {
char *cp;
int rv, size;
if (!PyArg_ParseTuple(args, "s#:write", &cp, &size)) {
return NULL;
}
Py_BEGIN_ALLOW_THREADS
rv = write(self->fd, cp, size);
Py_END_ALLOW_THREADS
if (rv == -1) {
return PyErr_SetFromErrno(PyExc_IOError);
} else {
self->ocount += rv;
}
return PyInt_FromLong(rv);
}
static PyObject *
oss_writeall(oss_audio_t *self, PyObject *args) {
char *cp;
int rv, size;
fd_set write_set_fds;
int select_rv;
if (!PyArg_ParseTuple(args, "s#:write", &cp, &size))
return NULL;
FD_ZERO(&write_set_fds);
FD_SET(self->fd, &write_set_fds);
while (size > 0) {
Py_BEGIN_ALLOW_THREADS
select_rv = select(self->fd+1, NULL, &write_set_fds, NULL, NULL);
Py_END_ALLOW_THREADS
assert(select_rv != 0);
if (select_rv == -1)
return PyErr_SetFromErrno(PyExc_IOError);
Py_BEGIN_ALLOW_THREADS
rv = write(self->fd, cp, size);
Py_END_ALLOW_THREADS
if (rv == -1) {
if (errno == EAGAIN) {
errno = 0;
continue;
} else
return PyErr_SetFromErrno(PyExc_IOError);
} else {
self->ocount += rv;
size -= rv;
cp += rv;
}
}
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
oss_close(oss_audio_t *self, PyObject *unused) {
if (self->fd >= 0) {
Py_BEGIN_ALLOW_THREADS
close(self->fd);
Py_END_ALLOW_THREADS
self->fd = -1;
}
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
oss_fileno(oss_audio_t *self, PyObject *unused) {
return PyInt_FromLong(self->fd);
}
static PyObject *
oss_setparameters(oss_audio_t *self, PyObject *args) {
int wanted_fmt, wanted_channels, wanted_rate, strict=0;
int fmt, channels, rate;
PyObject * rv;
if (!PyArg_ParseTuple(args, "iii|i:setparameters",
&wanted_fmt, &wanted_channels, &wanted_rate,
&strict))
return NULL;
fmt = wanted_fmt;
if (ioctl(self->fd, SNDCTL_DSP_SETFMT, &fmt) == -1) {
return PyErr_SetFromErrno(PyExc_IOError);
}
if (strict && fmt != wanted_fmt) {
return PyErr_Format
(OSSAudioError,
"unable to set requested format (wanted %d, got %d)",
wanted_fmt, fmt);
}
channels = wanted_channels;
if (ioctl(self->fd, SNDCTL_DSP_CHANNELS, &channels) == -1) {
return PyErr_SetFromErrno(PyExc_IOError);
}
if (strict && channels != wanted_channels) {
return PyErr_Format
(OSSAudioError,
"unable to set requested channels (wanted %d, got %d)",
wanted_channels, channels);
}
rate = wanted_rate;
if (ioctl(self->fd, SNDCTL_DSP_SPEED, &rate) == -1) {
return PyErr_SetFromErrno(PyExc_IOError);
}
if (strict && rate != wanted_rate) {
return PyErr_Format
(OSSAudioError,
"unable to set requested rate (wanted %d, got %d)",
wanted_rate, rate);
}
rv = PyTuple_New(3);
if (rv == NULL)
return NULL;
PyTuple_SET_ITEM(rv, 0, PyInt_FromLong(fmt));
PyTuple_SET_ITEM(rv, 1, PyInt_FromLong(channels));
PyTuple_SET_ITEM(rv, 2, PyInt_FromLong(rate));
return rv;
}
static int
_ssize(oss_audio_t *self, int *nchannels, int *ssize) {
int fmt;
fmt = 0;
if (ioctl(self->fd, SNDCTL_DSP_SETFMT, &fmt) < 0)
return -errno;
switch (fmt) {
case AFMT_MU_LAW:
case AFMT_A_LAW:
case AFMT_U8:
case AFMT_S8:
*ssize = 1;
break;
case AFMT_S16_LE:
case AFMT_S16_BE:
case AFMT_U16_LE:
case AFMT_U16_BE:
*ssize = 2;
break;
case AFMT_MPEG:
case AFMT_IMA_ADPCM:
default:
return -EOPNOTSUPP;
}
if (ioctl(self->fd, SNDCTL_DSP_CHANNELS, nchannels) < 0)
return -errno;
return 0;
}
static PyObject *
oss_bufsize(oss_audio_t *self, PyObject *unused) {
audio_buf_info ai;
int nchannels=0, ssize=0;
if (_ssize(self, &nchannels, &ssize) < 0 || !nchannels || !ssize) {
PyErr_SetFromErrno(PyExc_IOError);
return NULL;
}
if (ioctl(self->fd, SNDCTL_DSP_GETOSPACE, &ai) < 0) {
PyErr_SetFromErrno(PyExc_IOError);
return NULL;
}
return PyInt_FromLong((ai.fragstotal * ai.fragsize) / (nchannels * ssize));
}
static PyObject *
oss_obufcount(oss_audio_t *self, PyObject *unused) {
audio_buf_info ai;
int nchannels=0, ssize=0;
if (_ssize(self, &nchannels, &ssize) < 0 || !nchannels || !ssize) {
PyErr_SetFromErrno(PyExc_IOError);
return NULL;
}
if (ioctl(self->fd, SNDCTL_DSP_GETOSPACE, &ai) < 0) {
PyErr_SetFromErrno(PyExc_IOError);
return NULL;
}
return PyInt_FromLong((ai.fragstotal * ai.fragsize - ai.bytes) /
(ssize * nchannels));
}
static PyObject *
oss_obuffree(oss_audio_t *self, PyObject *unused) {
audio_buf_info ai;
int nchannels=0, ssize=0;
if (_ssize(self, &nchannels, &ssize) < 0 || !nchannels || !ssize) {
PyErr_SetFromErrno(PyExc_IOError);
return NULL;
}
if (ioctl(self->fd, SNDCTL_DSP_GETOSPACE, &ai) < 0) {
PyErr_SetFromErrno(PyExc_IOError);
return NULL;
}
return PyInt_FromLong(ai.bytes / (ssize * nchannels));
}
static PyObject *
oss_getptr(oss_audio_t *self, PyObject *unused) {
count_info info;
int req;
if (self->mode == O_RDONLY)
req = SNDCTL_DSP_GETIPTR;
else
req = SNDCTL_DSP_GETOPTR;
if (ioctl(self->fd, req, &info) == -1) {
PyErr_SetFromErrno(PyExc_IOError);
return NULL;
}
return Py_BuildValue("iii", info.bytes, info.blocks, info.ptr);
}
static PyObject *
oss_mixer_close(oss_mixer_t *self, PyObject *unused) {
if (self->fd >= 0) {
close(self->fd);
self->fd = -1;
}
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
oss_mixer_fileno(oss_mixer_t *self, PyObject *unused) {
return PyInt_FromLong(self->fd);
}
static PyObject *
oss_mixer_controls(oss_mixer_t *self, PyObject *args) {
return _do_ioctl_1_internal(self->fd, args, "controls",
SOUND_MIXER_READ_DEVMASK);
}
static PyObject *
oss_mixer_stereocontrols(oss_mixer_t *self, PyObject *args) {
return _do_ioctl_1_internal(self->fd, args, "stereocontrols",
SOUND_MIXER_READ_STEREODEVS);
}
static PyObject *
oss_mixer_reccontrols(oss_mixer_t *self, PyObject *args) {
return _do_ioctl_1_internal(self->fd, args, "reccontrols",
SOUND_MIXER_READ_RECMASK);
}
static PyObject *
oss_mixer_get(oss_mixer_t *self, PyObject *args) {
int channel, volume;
if (!PyArg_ParseTuple(args, "i:get", &channel))
return NULL;
if (channel < 0 || channel > SOUND_MIXER_NRDEVICES) {
PyErr_SetString(OSSAudioError, "Invalid mixer channel specified.");
return NULL;
}
if (ioctl(self->fd, MIXER_READ(channel), &volume) == -1)
return PyErr_SetFromErrno(PyExc_IOError);
return Py_BuildValue("(ii)", volume & 0xff, (volume & 0xff00) >> 8);
}
static PyObject *
oss_mixer_set(oss_mixer_t *self, PyObject *args) {
int channel, volume, leftVol, rightVol;
if (!PyArg_ParseTuple(args, "i(ii):set", &channel, &leftVol, &rightVol))
return NULL;
if (channel < 0 || channel > SOUND_MIXER_NRDEVICES) {
PyErr_SetString(OSSAudioError, "Invalid mixer channel specified.");
return NULL;
}
if (leftVol < 0 || rightVol < 0 || leftVol > 100 || rightVol > 100) {
PyErr_SetString(OSSAudioError, "Volumes must be between 0 and 100.");
return NULL;
}
volume = (rightVol << 8) | leftVol;
if (ioctl(self->fd, MIXER_WRITE(channel), &volume) == -1)
return PyErr_SetFromErrno(PyExc_IOError);
return Py_BuildValue("(ii)", volume & 0xff, (volume & 0xff00) >> 8);
}
static PyObject *
oss_mixer_get_recsrc(oss_mixer_t *self, PyObject *args) {
return _do_ioctl_1_internal(self->fd, args, "get_recsrc",
SOUND_MIXER_READ_RECSRC);
}
static PyObject *
oss_mixer_set_recsrc(oss_mixer_t *self, PyObject *args) {
return _do_ioctl_1(self->fd, args, "set_recsrc",
SOUND_MIXER_WRITE_RECSRC);
}
static PyMethodDef oss_methods[] = {
{ "read", (PyCFunction)oss_read, METH_VARARGS },
{ "write", (PyCFunction)oss_write, METH_VARARGS },
{ "writeall", (PyCFunction)oss_writeall, METH_VARARGS },
{ "close", (PyCFunction)oss_close, METH_NOARGS },
{ "fileno", (PyCFunction)oss_fileno, METH_NOARGS },
{ "nonblock", (PyCFunction)oss_nonblock, METH_NOARGS },
{ "setfmt", (PyCFunction)oss_setfmt, METH_VARARGS },
{ "getfmts", (PyCFunction)oss_getfmts, METH_NOARGS },
{ "channels", (PyCFunction)oss_channels, METH_VARARGS },
{ "speed", (PyCFunction)oss_speed, METH_VARARGS },
{ "sync", (PyCFunction)oss_sync, METH_VARARGS },
{ "reset", (PyCFunction)oss_reset, METH_VARARGS },
{ "post", (PyCFunction)oss_post, METH_VARARGS },
{ "setparameters", (PyCFunction)oss_setparameters, METH_VARARGS },
{ "bufsize", (PyCFunction)oss_bufsize, METH_NOARGS },
{ "obufcount", (PyCFunction)oss_obufcount, METH_NOARGS },
{ "obuffree", (PyCFunction)oss_obuffree, METH_NOARGS },
{ "getptr", (PyCFunction)oss_getptr, METH_NOARGS },
{ "flush", (PyCFunction)oss_sync, METH_VARARGS },
{ NULL, NULL}
};
static PyMethodDef oss_mixer_methods[] = {
{ "close", (PyCFunction)oss_mixer_close, METH_NOARGS },
{ "fileno", (PyCFunction)oss_mixer_fileno, METH_NOARGS },
{ "controls", (PyCFunction)oss_mixer_controls, METH_VARARGS },
{ "stereocontrols", (PyCFunction)oss_mixer_stereocontrols, METH_VARARGS},
{ "reccontrols", (PyCFunction)oss_mixer_reccontrols, METH_VARARGS},
{ "get", (PyCFunction)oss_mixer_get, METH_VARARGS },
{ "set", (PyCFunction)oss_mixer_set, METH_VARARGS },
{ "get_recsrc", (PyCFunction)oss_mixer_get_recsrc, METH_VARARGS },
{ "set_recsrc", (PyCFunction)oss_mixer_set_recsrc, METH_VARARGS },
{ NULL, NULL}
};
static PyObject *
oss_getattr(oss_audio_t *self, char *name) {
PyObject * rval = NULL;
if (strcmp(name, "closed") == 0) {
rval = (self->fd == -1) ? Py_True : Py_False;
Py_INCREF(rval);
} else if (strcmp(name, "name") == 0) {
rval = PyString_FromString(self->devicename);
} else if (strcmp(name, "mode") == 0) {
switch(self->mode) {
case O_RDONLY:
rval = PyString_FromString("r");
break;
case O_RDWR:
rval = PyString_FromString("rw");
break;
case O_WRONLY:
rval = PyString_FromString("w");
break;
}
} else {
rval = Py_FindMethod(oss_methods, (PyObject *)self, name);
}
return rval;
}
static PyObject *
oss_mixer_getattr(oss_mixer_t *self, char *name) {
return Py_FindMethod(oss_mixer_methods, (PyObject *)self, name);
}
static PyTypeObject OSSAudioType = {
PyVarObject_HEAD_INIT(&PyType_Type, 0)
"ossaudiodev.oss_audio_device",
sizeof(oss_audio_t),
0,
(destructor)oss_dealloc,
0,
(getattrfunc)oss_getattr,
0,
0,
0,
};
static PyTypeObject OSSMixerType = {
PyVarObject_HEAD_INIT(&PyType_Type, 0)
"ossaudiodev.oss_mixer_device",
sizeof(oss_mixer_t),
0,
(destructor)oss_mixer_dealloc,
0,
(getattrfunc)oss_mixer_getattr,
0,
0,
0,
};
static PyObject *
ossopen(PyObject *self, PyObject *args) {
return (PyObject *)newossobject(args);
}
static PyObject *
ossopenmixer(PyObject *self, PyObject *args) {
return (PyObject *)newossmixerobject(args);
}
static PyMethodDef ossaudiodev_methods[] = {
{ "open", ossopen, METH_VARARGS },
{ "openmixer", ossopenmixer, METH_VARARGS },
{ 0, 0 },
};
#define _EXPORT_INT(mod, name) if (PyModule_AddIntConstant(mod, #name, (long) (name)) == -1) return;
static char *control_labels[] = SOUND_DEVICE_LABELS;
static char *control_names[] = SOUND_DEVICE_NAMES;
static int
build_namelists (PyObject *module) {
PyObject *labels;
PyObject *names;
PyObject *s;
int num_controls;
int i;
num_controls = sizeof(control_labels) / sizeof(control_labels[0]);
assert(num_controls == sizeof(control_names) / sizeof(control_names[0]));
labels = PyList_New(num_controls);
names = PyList_New(num_controls);
if (labels == NULL || names == NULL)
goto error2;
for (i = 0; i < num_controls; i++) {
s = PyString_FromString(control_labels[i]);
if (s == NULL)
goto error2;
PyList_SET_ITEM(labels, i, s);
s = PyString_FromString(control_names[i]);
if (s == NULL)
goto error2;
PyList_SET_ITEM(names, i, s);
}
if (PyModule_AddObject(module, "control_labels", labels) == -1)
goto error2;
if (PyModule_AddObject(module, "control_names", names) == -1)
goto error1;
return 0;
error2:
Py_XDECREF(labels);
error1:
Py_XDECREF(names);
return -1;
}
void
initossaudiodev(void) {
PyObject *m;
m = Py_InitModule("ossaudiodev", ossaudiodev_methods);
if (m == NULL)
return;
OSSAudioError = PyErr_NewException("ossaudiodev.OSSAudioError",
NULL, NULL);
if (OSSAudioError) {
Py_INCREF(OSSAudioError);
Py_INCREF(OSSAudioError);
PyModule_AddObject(m, "error", OSSAudioError);
PyModule_AddObject(m, "OSSAudioError", OSSAudioError);
}
if (build_namelists(m) == -1)
return;
_EXPORT_INT(m, AFMT_QUERY);
_EXPORT_INT(m, AFMT_MU_LAW);
_EXPORT_INT(m, AFMT_A_LAW);
_EXPORT_INT(m, AFMT_IMA_ADPCM);
_EXPORT_INT(m, AFMT_U8);
_EXPORT_INT(m, AFMT_S16_LE);
_EXPORT_INT(m, AFMT_S16_BE);
_EXPORT_INT(m, AFMT_S8);
_EXPORT_INT(m, AFMT_U16_LE);
_EXPORT_INT(m, AFMT_U16_BE);
_EXPORT_INT(m, AFMT_MPEG);
#if defined(AFMT_AC3)
_EXPORT_INT(m, AFMT_AC3);
#endif
#if defined(AFMT_S16_NE)
_EXPORT_INT(m, AFMT_S16_NE);
#endif
#if defined(AFMT_U16_NE)
_EXPORT_INT(m, AFMT_U16_NE);
#endif
#if defined(AFMT_S32_LE)
_EXPORT_INT(m, AFMT_S32_LE);
#endif
#if defined(AFMT_S32_BE)
_EXPORT_INT(m, AFMT_S32_BE);
#endif
#if defined(AFMT_MPEG)
_EXPORT_INT(m, AFMT_MPEG);
#endif
_EXPORT_INT(m, SOUND_MIXER_NRDEVICES);
_EXPORT_INT(m, SOUND_MIXER_VOLUME);
_EXPORT_INT(m, SOUND_MIXER_BASS);
_EXPORT_INT(m, SOUND_MIXER_TREBLE);
_EXPORT_INT(m, SOUND_MIXER_SYNTH);
_EXPORT_INT(m, SOUND_MIXER_PCM);
_EXPORT_INT(m, SOUND_MIXER_SPEAKER);
_EXPORT_INT(m, SOUND_MIXER_LINE);
_EXPORT_INT(m, SOUND_MIXER_MIC);
_EXPORT_INT(m, SOUND_MIXER_CD);
_EXPORT_INT(m, SOUND_MIXER_IMIX);
_EXPORT_INT(m, SOUND_MIXER_ALTPCM);
_EXPORT_INT(m, SOUND_MIXER_RECLEV);
_EXPORT_INT(m, SOUND_MIXER_IGAIN);
_EXPORT_INT(m, SOUND_MIXER_OGAIN);
_EXPORT_INT(m, SOUND_MIXER_LINE1);
_EXPORT_INT(m, SOUND_MIXER_LINE2);
_EXPORT_INT(m, SOUND_MIXER_LINE3);
#if defined(SOUND_MIXER_DIGITAL1)
_EXPORT_INT(m, SOUND_MIXER_DIGITAL1);
#endif
#if defined(SOUND_MIXER_DIGITAL2)
_EXPORT_INT(m, SOUND_MIXER_DIGITAL2);
#endif
#if defined(SOUND_MIXER_DIGITAL3)
_EXPORT_INT(m, SOUND_MIXER_DIGITAL3);
#endif
#if defined(SOUND_MIXER_PHONEIN)
_EXPORT_INT(m, SOUND_MIXER_PHONEIN);
#endif
#if defined(SOUND_MIXER_PHONEOUT)
_EXPORT_INT(m, SOUND_MIXER_PHONEOUT);
#endif
#if defined(SOUND_MIXER_VIDEO)
_EXPORT_INT(m, SOUND_MIXER_VIDEO);
#endif
#if defined(SOUND_MIXER_RADIO)
_EXPORT_INT(m, SOUND_MIXER_RADIO);
#endif
#if defined(SOUND_MIXER_MONITOR)
_EXPORT_INT(m, SOUND_MIXER_MONITOR);
#endif
_EXPORT_INT(m, SNDCTL_COPR_HALT);
_EXPORT_INT(m, SNDCTL_COPR_LOAD);
_EXPORT_INT(m, SNDCTL_COPR_RCODE);
_EXPORT_INT(m, SNDCTL_COPR_RCVMSG);
_EXPORT_INT(m, SNDCTL_COPR_RDATA);
_EXPORT_INT(m, SNDCTL_COPR_RESET);
_EXPORT_INT(m, SNDCTL_COPR_RUN);
_EXPORT_INT(m, SNDCTL_COPR_SENDMSG);
_EXPORT_INT(m, SNDCTL_COPR_WCODE);
_EXPORT_INT(m, SNDCTL_COPR_WDATA);
#if defined(SNDCTL_DSP_BIND_CHANNEL)
_EXPORT_INT(m, SNDCTL_DSP_BIND_CHANNEL);
#endif
_EXPORT_INT(m, SNDCTL_DSP_CHANNELS);
_EXPORT_INT(m, SNDCTL_DSP_GETBLKSIZE);
_EXPORT_INT(m, SNDCTL_DSP_GETCAPS);
#if defined(SNDCTL_DSP_GETCHANNELMASK)
_EXPORT_INT(m, SNDCTL_DSP_GETCHANNELMASK);
#endif
_EXPORT_INT(m, SNDCTL_DSP_GETFMTS);
_EXPORT_INT(m, SNDCTL_DSP_GETIPTR);
_EXPORT_INT(m, SNDCTL_DSP_GETISPACE);
#if defined(SNDCTL_DSP_GETODELAY)
_EXPORT_INT(m, SNDCTL_DSP_GETODELAY);
#endif
_EXPORT_INT(m, SNDCTL_DSP_GETOPTR);
_EXPORT_INT(m, SNDCTL_DSP_GETOSPACE);
#if defined(SNDCTL_DSP_GETSPDIF)
_EXPORT_INT(m, SNDCTL_DSP_GETSPDIF);
#endif
_EXPORT_INT(m, SNDCTL_DSP_GETTRIGGER);
_EXPORT_INT(m, SNDCTL_DSP_MAPINBUF);
_EXPORT_INT(m, SNDCTL_DSP_MAPOUTBUF);
_EXPORT_INT(m, SNDCTL_DSP_NONBLOCK);
_EXPORT_INT(m, SNDCTL_DSP_POST);
#if defined(SNDCTL_DSP_PROFILE)
_EXPORT_INT(m, SNDCTL_DSP_PROFILE);
#endif
_EXPORT_INT(m, SNDCTL_DSP_RESET);
_EXPORT_INT(m, SNDCTL_DSP_SAMPLESIZE);
_EXPORT_INT(m, SNDCTL_DSP_SETDUPLEX);
_EXPORT_INT(m, SNDCTL_DSP_SETFMT);
_EXPORT_INT(m, SNDCTL_DSP_SETFRAGMENT);
#if defined(SNDCTL_DSP_SETSPDIF)
_EXPORT_INT(m, SNDCTL_DSP_SETSPDIF);
#endif
_EXPORT_INT(m, SNDCTL_DSP_SETSYNCRO);
_EXPORT_INT(m, SNDCTL_DSP_SETTRIGGER);
_EXPORT_INT(m, SNDCTL_DSP_SPEED);
_EXPORT_INT(m, SNDCTL_DSP_STEREO);
_EXPORT_INT(m, SNDCTL_DSP_SUBDIVIDE);
_EXPORT_INT(m, SNDCTL_DSP_SYNC);
_EXPORT_INT(m, SNDCTL_FM_4OP_ENABLE);
_EXPORT_INT(m, SNDCTL_FM_LOAD_INSTR);
_EXPORT_INT(m, SNDCTL_MIDI_INFO);
_EXPORT_INT(m, SNDCTL_MIDI_MPUCMD);
_EXPORT_INT(m, SNDCTL_MIDI_MPUMODE);
_EXPORT_INT(m, SNDCTL_MIDI_PRETIME);
_EXPORT_INT(m, SNDCTL_SEQ_CTRLRATE);
_EXPORT_INT(m, SNDCTL_SEQ_GETINCOUNT);
_EXPORT_INT(m, SNDCTL_SEQ_GETOUTCOUNT);
#if defined(SNDCTL_SEQ_GETTIME)
_EXPORT_INT(m, SNDCTL_SEQ_GETTIME);
#endif
_EXPORT_INT(m, SNDCTL_SEQ_NRMIDIS);
_EXPORT_INT(m, SNDCTL_SEQ_NRSYNTHS);
_EXPORT_INT(m, SNDCTL_SEQ_OUTOFBAND);
_EXPORT_INT(m, SNDCTL_SEQ_PANIC);
_EXPORT_INT(m, SNDCTL_SEQ_PERCMODE);
_EXPORT_INT(m, SNDCTL_SEQ_RESET);
_EXPORT_INT(m, SNDCTL_SEQ_RESETSAMPLES);
_EXPORT_INT(m, SNDCTL_SEQ_SYNC);
_EXPORT_INT(m, SNDCTL_SEQ_TESTMIDI);
_EXPORT_INT(m, SNDCTL_SEQ_THRESHOLD);
#if defined(SNDCTL_SYNTH_CONTROL)
_EXPORT_INT(m, SNDCTL_SYNTH_CONTROL);
#endif
#if defined(SNDCTL_SYNTH_ID)
_EXPORT_INT(m, SNDCTL_SYNTH_ID);
#endif
_EXPORT_INT(m, SNDCTL_SYNTH_INFO);
_EXPORT_INT(m, SNDCTL_SYNTH_MEMAVL);
#if defined(SNDCTL_SYNTH_REMOVESAMPLE)
_EXPORT_INT(m, SNDCTL_SYNTH_REMOVESAMPLE);
#endif
_EXPORT_INT(m, SNDCTL_TMR_CONTINUE);
_EXPORT_INT(m, SNDCTL_TMR_METRONOME);
_EXPORT_INT(m, SNDCTL_TMR_SELECT);
_EXPORT_INT(m, SNDCTL_TMR_SOURCE);
_EXPORT_INT(m, SNDCTL_TMR_START);
_EXPORT_INT(m, SNDCTL_TMR_STOP);
_EXPORT_INT(m, SNDCTL_TMR_TEMPO);
_EXPORT_INT(m, SNDCTL_TMR_TIMEBASE);
}
