#include "Python.h"
#include <structmember.h>
#if defined(__APPLE__)
#undef HAVE_BROKEN_POLL
#endif
#if defined(MS_WINDOWS) && !defined(FD_SETSIZE)
#define FD_SETSIZE 512
#endif
#if defined(HAVE_POLL_H)
#include <poll.h>
#elif defined(HAVE_SYS_POLL_H)
#include <sys/poll.h>
#endif
#if defined(__sgi)
extern void bzero(void *, int);
#endif
#if defined(HAVE_SYS_TYPES_H)
#include <sys/types.h>
#endif
#if defined(PYOS_OS2) && !defined(PYCC_GCC)
#include <sys/time.h>
#include <utils.h>
#endif
#if defined(MS_WINDOWS)
#include <winsock.h>
#else
#define SOCKET int
#if defined(__BEOS__)
#include <net/socket.h>
#elif defined(__VMS)
#include <socket.h>
#endif
#endif
static PyObject *SelectError;
typedef struct {
PyObject *obj;
SOCKET fd;
int sentinel;
} pylist;
static void
reap_obj(pylist fd2obj[FD_SETSIZE + 1]) {
int i;
for (i = 0; i < FD_SETSIZE + 1 && fd2obj[i].sentinel >= 0; i++) {
Py_XDECREF(fd2obj[i].obj);
fd2obj[i].obj = NULL;
}
fd2obj[0].sentinel = -1;
}
static int
seq2set(PyObject *seq, fd_set *set, pylist fd2obj[FD_SETSIZE + 1]) {
int i;
int max = -1;
int index = 0;
int len = -1;
PyObject* fast_seq = NULL;
PyObject* o = NULL;
fd2obj[0].obj = (PyObject*)0;
FD_ZERO(set);
fast_seq=PySequence_Fast(seq, "arguments 1-3 must be sequences");
if (!fast_seq)
return -1;
len = PySequence_Fast_GET_SIZE(fast_seq);
for (i = 0; i < len; i++) {
SOCKET v;
if (!(o = PySequence_Fast_GET_ITEM(fast_seq, i)))
return -1;
Py_INCREF(o);
v = PyObject_AsFileDescriptor( o );
if (v == -1) goto finally;
#if defined(_MSC_VER)
max = 0;
#else
if (v < 0 || v >= FD_SETSIZE) {
PyErr_SetString(PyExc_ValueError,
"filedescriptor out of range in select()");
goto finally;
}
if (v > max)
max = v;
#endif
FD_SET(v, set);
if (index >= FD_SETSIZE) {
PyErr_SetString(PyExc_ValueError,
"too many file descriptors in select()");
goto finally;
}
fd2obj[index].obj = o;
fd2obj[index].fd = v;
fd2obj[index].sentinel = 0;
fd2obj[++index].sentinel = -1;
}
Py_DECREF(fast_seq);
return max+1;
finally:
Py_XDECREF(o);
Py_DECREF(fast_seq);
return -1;
}
static PyObject *
set2list(fd_set *set, pylist fd2obj[FD_SETSIZE + 1]) {
int i, j, count=0;
PyObject *list, *o;
SOCKET fd;
for (j = 0; fd2obj[j].sentinel >= 0; j++) {
if (FD_ISSET(fd2obj[j].fd, set))
count++;
}
list = PyList_New(count);
if (!list)
return NULL;
i = 0;
for (j = 0; fd2obj[j].sentinel >= 0; j++) {
fd = fd2obj[j].fd;
if (FD_ISSET(fd, set)) {
#if !defined(_MSC_VER)
if (fd > FD_SETSIZE) {
PyErr_SetString(PyExc_SystemError,
"filedescriptor out of range returned in select()");
goto finally;
}
#endif
o = fd2obj[j].obj;
fd2obj[j].obj = NULL;
if (PyList_SetItem(list, i, o) < 0)
goto finally;
i++;
}
}
return list;
finally:
Py_DECREF(list);
return NULL;
}
#undef SELECT_USES_HEAP
#if FD_SETSIZE > 1024
#define SELECT_USES_HEAP
#endif
static PyObject *
select_select(PyObject *self, PyObject *args) {
#if defined(SELECT_USES_HEAP)
pylist *rfd2obj, *wfd2obj, *efd2obj;
#else
pylist rfd2obj[FD_SETSIZE + 1];
pylist wfd2obj[FD_SETSIZE + 1];
pylist efd2obj[FD_SETSIZE + 1];
#endif
PyObject *ifdlist, *ofdlist, *efdlist;
PyObject *ret = NULL;
PyObject *tout = Py_None;
fd_set ifdset, ofdset, efdset;
double timeout;
struct timeval tv, *tvp;
long seconds;
int imax, omax, emax, max;
int n;
if (!PyArg_UnpackTuple(args, "select", 3, 4,
&ifdlist, &ofdlist, &efdlist, &tout))
return NULL;
if (tout == Py_None)
tvp = (struct timeval *)0;
else if (!PyNumber_Check(tout)) {
PyErr_SetString(PyExc_TypeError,
"timeout must be a float or None");
return NULL;
} else {
timeout = PyFloat_AsDouble(tout);
if (timeout == -1 && PyErr_Occurred())
return NULL;
if (timeout > (double)LONG_MAX) {
PyErr_SetString(PyExc_OverflowError,
"timeout period too long");
return NULL;
}
seconds = (long)timeout;
timeout = timeout - (double)seconds;
tv.tv_sec = seconds;
tv.tv_usec = (long)(timeout * 1E6);
tvp = &tv;
}
#if defined(SELECT_USES_HEAP)
rfd2obj = PyMem_NEW(pylist, FD_SETSIZE + 1);
wfd2obj = PyMem_NEW(pylist, FD_SETSIZE + 1);
efd2obj = PyMem_NEW(pylist, FD_SETSIZE + 1);
if (rfd2obj == NULL || wfd2obj == NULL || efd2obj == NULL) {
if (rfd2obj) PyMem_DEL(rfd2obj);
if (wfd2obj) PyMem_DEL(wfd2obj);
if (efd2obj) PyMem_DEL(efd2obj);
return PyErr_NoMemory();
}
#endif
rfd2obj[0].sentinel = -1;
wfd2obj[0].sentinel = -1;
efd2obj[0].sentinel = -1;
if ((imax=seq2set(ifdlist, &ifdset, rfd2obj)) < 0)
goto finally;
if ((omax=seq2set(ofdlist, &ofdset, wfd2obj)) < 0)
goto finally;
if ((emax=seq2set(efdlist, &efdset, efd2obj)) < 0)
goto finally;
max = imax;
if (omax > max) max = omax;
if (emax > max) max = emax;
Py_BEGIN_ALLOW_THREADS
n = select(max, &ifdset, &ofdset, &efdset, tvp);
Py_END_ALLOW_THREADS
#if defined(MS_WINDOWS)
if (n == SOCKET_ERROR) {
PyErr_SetExcFromWindowsErr(SelectError, WSAGetLastError());
}
#else
if (n < 0) {
PyErr_SetFromErrno(SelectError);
}
#endif
else if (n == 0) {
ifdlist = PyList_New(0);
if (ifdlist) {
ret = PyTuple_Pack(3, ifdlist, ifdlist, ifdlist);
Py_DECREF(ifdlist);
}
} else {
ifdlist = set2list(&ifdset, rfd2obj);
ofdlist = set2list(&ofdset, wfd2obj);
efdlist = set2list(&efdset, efd2obj);
if (PyErr_Occurred())
ret = NULL;
else
ret = PyTuple_Pack(3, ifdlist, ofdlist, efdlist);
Py_DECREF(ifdlist);
Py_DECREF(ofdlist);
Py_DECREF(efdlist);
}
finally:
reap_obj(rfd2obj);
reap_obj(wfd2obj);
reap_obj(efd2obj);
#if defined(SELECT_USES_HEAP)
PyMem_DEL(rfd2obj);
PyMem_DEL(wfd2obj);
PyMem_DEL(efd2obj);
#endif
return ret;
}
#if defined(HAVE_POLL) && !defined(HAVE_BROKEN_POLL)
typedef struct {
PyObject_HEAD
PyObject *dict;
int ufd_uptodate;
int ufd_len;
struct pollfd *ufds;
} pollObject;
static PyTypeObject poll_Type;
static int
update_ufd_array(pollObject *self) {
Py_ssize_t i, pos;
PyObject *key, *value;
struct pollfd *old_ufds = self->ufds;
self->ufd_len = PyDict_Size(self->dict);
PyMem_RESIZE(self->ufds, struct pollfd, self->ufd_len);
if (self->ufds == NULL) {
self->ufds = old_ufds;
PyErr_NoMemory();
return 0;
}
i = pos = 0;
while (PyDict_Next(self->dict, &pos, &key, &value)) {
self->ufds[i].fd = PyInt_AsLong(key);
self->ufds[i].events = (short)PyInt_AsLong(value);
i++;
}
self->ufd_uptodate = 1;
return 1;
}
PyDoc_STRVAR(poll_register_doc,
"register(fd [, eventmask] ) -> None\n\n\
Register a file descriptor with the polling object.\n\
fd -- either an integer, or an object with a fileno() method returning an\n\
int.\n\
events -- an optional bitmask describing the type of events to check for");
static PyObject *
poll_register(pollObject *self, PyObject *args) {
PyObject *o, *key, *value;
int fd, events = POLLIN | POLLPRI | POLLOUT;
int err;
if (!PyArg_ParseTuple(args, "O|i:register", &o, &events)) {
return NULL;
}
fd = PyObject_AsFileDescriptor(o);
if (fd == -1) return NULL;
key = PyInt_FromLong(fd);
if (key == NULL)
return NULL;
value = PyInt_FromLong(events);
if (value == NULL) {
Py_DECREF(key);
return NULL;
}
err = PyDict_SetItem(self->dict, key, value);
Py_DECREF(key);
Py_DECREF(value);
if (err < 0)
return NULL;
self->ufd_uptodate = 0;
Py_INCREF(Py_None);
return Py_None;
}
PyDoc_STRVAR(poll_modify_doc,
"modify(fd, eventmask) -> None\n\n\
Modify an already registered file descriptor.\n\
fd -- either an integer, or an object with a fileno() method returning an\n\
int.\n\
events -- an optional bitmask describing the type of events to check for");
static PyObject *
poll_modify(pollObject *self, PyObject *args) {
PyObject *o, *key, *value;
int fd, events;
int err;
if (!PyArg_ParseTuple(args, "Oi:modify", &o, &events)) {
return NULL;
}
fd = PyObject_AsFileDescriptor(o);
if (fd == -1) return NULL;
key = PyInt_FromLong(fd);
if (key == NULL)
return NULL;
if (PyDict_GetItem(self->dict, key) == NULL) {
errno = ENOENT;
PyErr_SetFromErrno(PyExc_IOError);
return NULL;
}
value = PyInt_FromLong(events);
if (value == NULL) {
Py_DECREF(key);
return NULL;
}
err = PyDict_SetItem(self->dict, key, value);
Py_DECREF(key);
Py_DECREF(value);
if (err < 0)
return NULL;
self->ufd_uptodate = 0;
Py_INCREF(Py_None);
return Py_None;
}
PyDoc_STRVAR(poll_unregister_doc,
"unregister(fd) -> None\n\n\
Remove a file descriptor being tracked by the polling object.");
static PyObject *
poll_unregister(pollObject *self, PyObject *o) {
PyObject *key;
int fd;
fd = PyObject_AsFileDescriptor( o );
if (fd == -1)
return NULL;
key = PyInt_FromLong(fd);
if (key == NULL)
return NULL;
if (PyDict_DelItem(self->dict, key) == -1) {
Py_DECREF(key);
return NULL;
}
Py_DECREF(key);
self->ufd_uptodate = 0;
Py_INCREF(Py_None);
return Py_None;
}
PyDoc_STRVAR(poll_poll_doc,
"poll( [timeout] ) -> list of (fd, event) 2-tuples\n\n\
Polls the set of registered file descriptors, returning a list containing \n\
any descriptors that have events or errors to report.");
static PyObject *
poll_poll(pollObject *self, PyObject *args) {
PyObject *result_list = NULL, *tout = NULL;
int timeout = 0, poll_result, i, j;
PyObject *value = NULL, *num = NULL;
if (!PyArg_UnpackTuple(args, "poll", 0, 1, &tout)) {
return NULL;
}
if (tout == NULL || tout == Py_None)
timeout = -1;
else if (!PyNumber_Check(tout)) {
PyErr_SetString(PyExc_TypeError,
"timeout must be an integer or None");
return NULL;
} else {
tout = PyNumber_Int(tout);
if (!tout)
return NULL;
timeout = PyInt_AsLong(tout);
Py_DECREF(tout);
if (timeout == -1 && PyErr_Occurred())
return NULL;
}
if (!self->ufd_uptodate)
if (update_ufd_array(self) == 0)
return NULL;
Py_BEGIN_ALLOW_THREADS
poll_result = poll(self->ufds, self->ufd_len, timeout);
Py_END_ALLOW_THREADS
if (poll_result < 0) {
PyErr_SetFromErrno(SelectError);
return NULL;
}
result_list = PyList_New(poll_result);
if (!result_list)
return NULL;
else {
for (i = 0, j = 0; j < poll_result; j++) {
while (!self->ufds[i].revents) {
i++;
}
value = PyTuple_New(2);
if (value == NULL)
goto error;
num = PyInt_FromLong(self->ufds[i].fd);
if (num == NULL) {
Py_DECREF(value);
goto error;
}
PyTuple_SET_ITEM(value, 0, num);
num = PyInt_FromLong(self->ufds[i].revents & 0xffff);
if (num == NULL) {
Py_DECREF(value);
goto error;
}
PyTuple_SET_ITEM(value, 1, num);
if ((PyList_SetItem(result_list, j, value)) == -1) {
Py_DECREF(value);
goto error;
}
i++;
}
}
return result_list;
error:
Py_DECREF(result_list);
return NULL;
}
static PyMethodDef poll_methods[] = {
{
"register", (PyCFunction)poll_register,
METH_VARARGS, poll_register_doc
},
{
"modify", (PyCFunction)poll_modify,
METH_VARARGS, poll_modify_doc
},
{
"unregister", (PyCFunction)poll_unregister,
METH_O, poll_unregister_doc
},
{
"poll", (PyCFunction)poll_poll,
METH_VARARGS, poll_poll_doc
},
{NULL, NULL}
};
static pollObject *
newPollObject(void) {
pollObject *self;
self = PyObject_New(pollObject, &poll_Type);
if (self == NULL)
return NULL;
self->ufd_uptodate = 0;
self->ufds = NULL;
self->dict = PyDict_New();
if (self->dict == NULL) {
Py_DECREF(self);
return NULL;
}
return self;
}
static void
poll_dealloc(pollObject *self) {
if (self->ufds != NULL)
PyMem_DEL(self->ufds);
Py_XDECREF(self->dict);
PyObject_Del(self);
}
static PyObject *
poll_getattr(pollObject *self, char *name) {
return Py_FindMethod(poll_methods, (PyObject *)self, name);
}
static PyTypeObject poll_Type = {
PyVarObject_HEAD_INIT(NULL, 0)
"select.poll",
sizeof(pollObject),
0,
(destructor)poll_dealloc,
0,
(getattrfunc)poll_getattr,
0,
0,
0,
0,
0,
0,
0,
};
PyDoc_STRVAR(poll_doc,
"Returns a polling object, which supports registering and\n\
unregistering file descriptors, and then polling them for I/O events.");
static PyObject *
select_poll(PyObject *self, PyObject *unused) {
return (PyObject *)newPollObject();
}
#if defined(__APPLE__)
static int select_have_broken_poll(void) {
int poll_test;
int filedes[2];
struct pollfd poll_struct = { 0, POLLIN|POLLPRI|POLLOUT, 0 };
if (pipe(filedes) < 0) {
return 1;
}
poll_struct.fd = filedes[0];
close(filedes[0]);
close(filedes[1]);
poll_test = poll(&poll_struct, 1, 0);
if (poll_test < 0) {
return 1;
} else if (poll_test == 0 && poll_struct.revents != POLLNVAL) {
return 1;
}
return 0;
}
#endif
#endif
#if defined(HAVE_EPOLL)
#if defined(HAVE_SYS_EPOLL_H)
#include <sys/epoll.h>
#endif
typedef struct {
PyObject_HEAD
SOCKET epfd;
} pyEpoll_Object;
static PyTypeObject pyEpoll_Type;
#define pyepoll_CHECK(op) (PyObject_TypeCheck((op), &pyEpoll_Type))
static PyObject *
pyepoll_err_closed(void) {
PyErr_SetString(PyExc_ValueError, "I/O operation on closed epoll fd");
return NULL;
}
static int
pyepoll_internal_close(pyEpoll_Object *self) {
int save_errno = 0;
if (self->epfd >= 0) {
int epfd = self->epfd;
self->epfd = -1;
Py_BEGIN_ALLOW_THREADS
if (close(epfd) < 0)
save_errno = errno;
Py_END_ALLOW_THREADS
}
return save_errno;
}
static PyObject *
newPyEpoll_Object(PyTypeObject *type, int sizehint, SOCKET fd) {
pyEpoll_Object *self;
if (sizehint == -1) {
sizehint = FD_SETSIZE-1;
} else if (sizehint < 1) {
PyErr_Format(PyExc_ValueError,
"sizehint must be greater zero, got %d",
sizehint);
return NULL;
}
assert(type != NULL && type->tp_alloc != NULL);
self = (pyEpoll_Object *) type->tp_alloc(type, 0);
if (self == NULL)
return NULL;
if (fd == -1) {
Py_BEGIN_ALLOW_THREADS
self->epfd = epoll_create(sizehint);
Py_END_ALLOW_THREADS
} else {
self->epfd = fd;
}
if (self->epfd < 0) {
Py_DECREF(self);
PyErr_SetFromErrno(PyExc_IOError);
return NULL;
}
return (PyObject *)self;
}
static PyObject *
pyepoll_new(PyTypeObject *type, PyObject *args, PyObject *kwds) {
int sizehint = -1;
static char *kwlist[] = {"sizehint", NULL};
if (!PyArg_ParseTupleAndKeywords(args, kwds, "|i:epoll", kwlist,
&sizehint))
return NULL;
return newPyEpoll_Object(type, sizehint, -1);
}
static void
pyepoll_dealloc(pyEpoll_Object *self) {
(void)pyepoll_internal_close(self);
Py_TYPE(self)->tp_free(self);
}
static PyObject*
pyepoll_close(pyEpoll_Object *self) {
errno = pyepoll_internal_close(self);
if (errno < 0) {
PyErr_SetFromErrno(PyExc_IOError);
return NULL;
}
Py_RETURN_NONE;
}
PyDoc_STRVAR(pyepoll_close_doc,
"close() -> None\n\
\n\
Close the epoll control file descriptor. Further operations on the epoll\n\
object will raise an exception.");
static PyObject*
pyepoll_get_closed(pyEpoll_Object *self) {
if (self->epfd < 0)
Py_RETURN_TRUE;
else
Py_RETURN_FALSE;
}
static PyObject*
pyepoll_fileno(pyEpoll_Object *self) {
if (self->epfd < 0)
return pyepoll_err_closed();
return PyInt_FromLong(self->epfd);
}
PyDoc_STRVAR(pyepoll_fileno_doc,
"fileno() -> int\n\
\n\
Return the epoll control file descriptor.");
static PyObject*
pyepoll_fromfd(PyObject *cls, PyObject *args) {
SOCKET fd;
if (!PyArg_ParseTuple(args, "i:fromfd", &fd))
return NULL;
return newPyEpoll_Object((PyTypeObject*)cls, -1, fd);
}
PyDoc_STRVAR(pyepoll_fromfd_doc,
"fromfd(fd) -> epoll\n\
\n\
Create an epoll object from a given control fd.");
static PyObject *
pyepoll_internal_ctl(int epfd, int op, PyObject *pfd, unsigned int events) {
struct epoll_event ev;
int result;
int fd;
if (epfd < 0)
return pyepoll_err_closed();
fd = PyObject_AsFileDescriptor(pfd);
if (fd == -1) {
return NULL;
}
switch(op) {
case EPOLL_CTL_ADD:
case EPOLL_CTL_MOD:
ev.events = events;
ev.data.fd = fd;
Py_BEGIN_ALLOW_THREADS
result = epoll_ctl(epfd, op, fd, &ev);
Py_END_ALLOW_THREADS
break;
case EPOLL_CTL_DEL:
Py_BEGIN_ALLOW_THREADS
result = epoll_ctl(epfd, op, fd, &ev);
if (errno == EBADF) {
result = 0;
errno = 0;
}
Py_END_ALLOW_THREADS
break;
default:
result = -1;
errno = EINVAL;
}
if (result < 0) {
PyErr_SetFromErrno(PyExc_IOError);
return NULL;
}
Py_RETURN_NONE;
}
static PyObject *
pyepoll_register(pyEpoll_Object *self, PyObject *args, PyObject *kwds) {
PyObject *pfd;
unsigned int events = EPOLLIN | EPOLLOUT | EPOLLPRI;
static char *kwlist[] = {"fd", "eventmask", NULL};
if (!PyArg_ParseTupleAndKeywords(args, kwds, "O|I:register", kwlist,
&pfd, &events)) {
return NULL;
}
return pyepoll_internal_ctl(self->epfd, EPOLL_CTL_ADD, pfd, events);
}
PyDoc_STRVAR(pyepoll_register_doc,
"register(fd[, eventmask]) -> bool\n\
\n\
Registers a new fd or modifies an already registered fd. register() returns\n\
True if a new fd was registered or False if the event mask for fd was modified.\n\
fd is the target file descriptor of the operation.\n\
events is a bit set composed of the various EPOLL constants; the default\n\
is EPOLL_IN | EPOLL_OUT | EPOLL_PRI.\n\
\n\
The epoll interface supports all file descriptors that support poll.");
static PyObject *
pyepoll_modify(pyEpoll_Object *self, PyObject *args, PyObject *kwds) {
PyObject *pfd;
unsigned int events;
static char *kwlist[] = {"fd", "eventmask", NULL};
if (!PyArg_ParseTupleAndKeywords(args, kwds, "OI:modify", kwlist,
&pfd, &events)) {
return NULL;
}
return pyepoll_internal_ctl(self->epfd, EPOLL_CTL_MOD, pfd, events);
}
PyDoc_STRVAR(pyepoll_modify_doc,
"modify(fd, eventmask) -> None\n\
\n\
fd is the target file descriptor of the operation\n\
events is a bit set composed of the various EPOLL constants");
static PyObject *
pyepoll_unregister(pyEpoll_Object *self, PyObject *args, PyObject *kwds) {
PyObject *pfd;
static char *kwlist[] = {"fd", NULL};
if (!PyArg_ParseTupleAndKeywords(args, kwds, "O:unregister", kwlist,
&pfd)) {
return NULL;
}
return pyepoll_internal_ctl(self->epfd, EPOLL_CTL_DEL, pfd, 0);
}
PyDoc_STRVAR(pyepoll_unregister_doc,
"unregister(fd) -> None\n\
\n\
fd is the target file descriptor of the operation.");
static PyObject *
pyepoll_poll(pyEpoll_Object *self, PyObject *args, PyObject *kwds) {
double dtimeout = -1.;
int timeout;
int maxevents = -1;
int nfds, i;
PyObject *elist = NULL, *etuple = NULL;
struct epoll_event *evs = NULL;
static char *kwlist[] = {"timeout", "maxevents", NULL};
if (self->epfd < 0)
return pyepoll_err_closed();
if (!PyArg_ParseTupleAndKeywords(args, kwds, "|di:poll", kwlist,
&dtimeout, &maxevents)) {
return NULL;
}
if (dtimeout < 0) {
timeout = -1;
} else if (dtimeout * 1000.0 > INT_MAX) {
PyErr_SetString(PyExc_OverflowError,
"timeout is too large");
return NULL;
} else {
timeout = (int)(dtimeout * 1000.0);
}
if (maxevents == -1) {
maxevents = FD_SETSIZE-1;
} else if (maxevents < 1) {
PyErr_Format(PyExc_ValueError,
"maxevents must be greater than 0, got %d",
maxevents);
return NULL;
}
evs = PyMem_New(struct epoll_event, maxevents);
if (evs == NULL) {
Py_DECREF(self);
PyErr_NoMemory();
return NULL;
}
Py_BEGIN_ALLOW_THREADS
nfds = epoll_wait(self->epfd, evs, maxevents, timeout);
Py_END_ALLOW_THREADS
if (nfds < 0) {
PyErr_SetFromErrno(PyExc_IOError);
goto error;
}
elist = PyList_New(nfds);
if (elist == NULL) {
goto error;
}
for (i = 0; i < nfds; i++) {
etuple = Py_BuildValue("iI", evs[i].data.fd, evs[i].events);
if (etuple == NULL) {
Py_CLEAR(elist);
goto error;
}
PyList_SET_ITEM(elist, i, etuple);
}
error:
PyMem_Free(evs);
return elist;
}
PyDoc_STRVAR(pyepoll_poll_doc,
"poll([timeout=-1[, maxevents=-1]]) -> [(fd, events), (...)]\n\
\n\
Wait for events on the epoll file descriptor for a maximum time of timeout\n\
in seconds (as float). -1 makes poll wait indefinitely.\n\
Up to maxevents are returned to the caller.");
static PyMethodDef pyepoll_methods[] = {
{
"fromfd", (PyCFunction)pyepoll_fromfd,
METH_VARARGS | METH_CLASS, pyepoll_fromfd_doc
},
{
"close", (PyCFunction)pyepoll_close, METH_NOARGS,
pyepoll_close_doc
},
{
"fileno", (PyCFunction)pyepoll_fileno, METH_NOARGS,
pyepoll_fileno_doc
},
{
"modify", (PyCFunction)pyepoll_modify,
METH_VARARGS | METH_KEYWORDS, pyepoll_modify_doc
},
{
"register", (PyCFunction)pyepoll_register,
METH_VARARGS | METH_KEYWORDS, pyepoll_register_doc
},
{
"unregister", (PyCFunction)pyepoll_unregister,
METH_VARARGS | METH_KEYWORDS, pyepoll_unregister_doc
},
{
"poll", (PyCFunction)pyepoll_poll,
METH_VARARGS | METH_KEYWORDS, pyepoll_poll_doc
},
{NULL, NULL},
};
static PyGetSetDef pyepoll_getsetlist[] = {
{
"closed", (getter)pyepoll_get_closed, NULL,
"True if the epoll handler is closed"
},
{0},
};
PyDoc_STRVAR(pyepoll_doc,
"select.epoll([sizehint=-1])\n\
\n\
Returns an epolling object\n\
\n\
sizehint must be a positive integer or -1 for the default size. The\n\
sizehint is used to optimize internal data structures. It doesn't limit\n\
the maximum number of monitored events.");
static PyTypeObject pyEpoll_Type = {
PyVarObject_HEAD_INIT(NULL, 0)
"select.epoll",
sizeof(pyEpoll_Object),
0,
(destructor)pyepoll_dealloc,
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
0,
0,
Py_TPFLAGS_DEFAULT,
pyepoll_doc,
0,
0,
0,
0,
0,
0,
pyepoll_methods,
0,
pyepoll_getsetlist,
0,
0,
0,
0,
0,
0,
0,
pyepoll_new,
0,
};
#endif
#if defined(HAVE_KQUEUE)
#if defined(HAVE_SYS_EVENT_H)
#include <sys/event.h>
#endif
PyDoc_STRVAR(kqueue_event_doc,
"kevent(ident, filter=KQ_FILTER_READ, flags=KQ_ADD, fflags=0, data=0, udata=0)\n\
\n\
This object is the equivalent of the struct kevent for the C API.\n\
\n\
See the kqueue manpage for more detailed information about the meaning\n\
of the arguments.\n\
\n\
One minor note: while you might hope that udata could store a\n\
reference to a python object, it cannot, because it is impossible to\n\
keep a proper reference count of the object once it's passed into the\n\
kernel. Therefore, I have restricted it to only storing an integer. I\n\
recommend ignoring it and simply using the 'ident' field to key off\n\
of. You could also set up a dictionary on the python side to store a\n\
udata->object mapping.");
typedef struct {
PyObject_HEAD
struct kevent e;
} kqueue_event_Object;
static PyTypeObject kqueue_event_Type;
#define kqueue_event_Check(op) (PyObject_TypeCheck((op), &kqueue_event_Type))
typedef struct {
PyObject_HEAD
SOCKET kqfd;
} kqueue_queue_Object;
static PyTypeObject kqueue_queue_Type;
#define kqueue_queue_Check(op) (PyObject_TypeCheck((op), &kqueue_queue_Type))
#define KQ_OFF(x) offsetof(kqueue_event_Object, x)
static struct PyMemberDef kqueue_event_members[] = {
{"ident", T_UINT, KQ_OFF(e.ident)},
{"filter", T_SHORT, KQ_OFF(e.filter)},
{"flags", T_USHORT, KQ_OFF(e.flags)},
{"fflags", T_UINT, KQ_OFF(e.fflags)},
{"data", T_INT, KQ_OFF(e.data)},
{"udata", T_INT, KQ_OFF(e.udata)},
{NULL}
};
#undef KQ_OFF
static PyObject *
kqueue_event_repr(kqueue_event_Object *s) {
char buf[1024];
PyOS_snprintf(
buf, sizeof(buf),
"<select.kevent ident=%lu filter=%d flags=0x%x fflags=0x%x "
"data=0x%lx udata=%p>",
(unsigned long)(s->e.ident), s->e.filter, s->e.flags,
s->e.fflags, (long)(s->e.data), s->e.udata);
return PyString_FromString(buf);
}
static int
kqueue_event_init(kqueue_event_Object *self, PyObject *args, PyObject *kwds) {
PyObject *pfd;
static char *kwlist[] = {"ident", "filter", "flags", "fflags",
"data", "udata", NULL
};
EV_SET(&(self->e), 0, EVFILT_READ, EV_ADD, 0, 0, 0);
if (!PyArg_ParseTupleAndKeywords(args, kwds, "O|hhiii:kevent", kwlist,
&pfd, &(self->e.filter), &(self->e.flags),
&(self->e.fflags), &(self->e.data), &(self->e.udata))) {
return -1;
}
self->e.ident = PyObject_AsFileDescriptor(pfd);
if (self->e.ident == -1) {
return -1;
}
return 0;
}
static PyObject *
kqueue_event_richcompare(kqueue_event_Object *s, kqueue_event_Object *o,
int op) {
int result = 0;
if (!kqueue_event_Check(o)) {
if (op == Py_EQ || op == Py_NE) {
PyObject *res = op == Py_EQ ? Py_False : Py_True;
Py_INCREF(res);
return res;
}
PyErr_Format(PyExc_TypeError,
"can't compare %.200s to %.200s",
Py_TYPE(s)->tp_name, Py_TYPE(o)->tp_name);
return NULL;
}
if (((result = s->e.ident - o->e.ident) == 0) &&
((result = s->e.filter - o->e.filter) == 0) &&
((result = s->e.flags - o->e.flags) == 0) &&
((result = s->e.fflags - o->e.fflags) == 0) &&
((result = s->e.data - o->e.data) == 0) &&
((result = s->e.udata - o->e.udata) == 0)
) {
result = 0;
}
switch (op) {
case Py_EQ:
result = (result == 0);
break;
case Py_NE:
result = (result != 0);
break;
case Py_LE:
result = (result <= 0);
break;
case Py_GE:
result = (result >= 0);
break;
case Py_LT:
result = (result < 0);
break;
case Py_GT:
result = (result > 0);
break;
}
return PyBool_FromLong(result);
}
static PyTypeObject kqueue_event_Type = {
PyVarObject_HEAD_INIT(NULL, 0)
"select.kevent",
sizeof(kqueue_event_Object),
0,
0,
0,
0,
0,
0,
(reprfunc)kqueue_event_repr,
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
kqueue_event_doc,
0,
0,
(richcmpfunc)kqueue_event_richcompare,
0,
0,
0,
0,
kqueue_event_members,
0,
0,
0,
0,
0,
0,
(initproc)kqueue_event_init,
0,
0,
0,
};
static PyObject *
kqueue_queue_err_closed(void) {
PyErr_SetString(PyExc_ValueError, "I/O operation on closed kqueue fd");
return NULL;
}
static int
kqueue_queue_internal_close(kqueue_queue_Object *self) {
int save_errno = 0;
if (self->kqfd >= 0) {
int kqfd = self->kqfd;
self->kqfd = -1;
Py_BEGIN_ALLOW_THREADS
if (close(kqfd) < 0)
save_errno = errno;
Py_END_ALLOW_THREADS
}
return save_errno;
}
static PyObject *
newKqueue_Object(PyTypeObject *type, SOCKET fd) {
kqueue_queue_Object *self;
assert(type != NULL && type->tp_alloc != NULL);
self = (kqueue_queue_Object *) type->tp_alloc(type, 0);
if (self == NULL) {
return NULL;
}
if (fd == -1) {
Py_BEGIN_ALLOW_THREADS
self->kqfd = kqueue();
Py_END_ALLOW_THREADS
} else {
self->kqfd = fd;
}
if (self->kqfd < 0) {
Py_DECREF(self);
PyErr_SetFromErrno(PyExc_IOError);
return NULL;
}
return (PyObject *)self;
}
static PyObject *
kqueue_queue_new(PyTypeObject *type, PyObject *args, PyObject *kwds) {
if ((args != NULL && PyObject_Size(args)) ||
(kwds != NULL && PyObject_Size(kwds))) {
PyErr_SetString(PyExc_ValueError,
"select.kqueue doesn't accept arguments");
return NULL;
}
return newKqueue_Object(type, -1);
}
static void
kqueue_queue_dealloc(kqueue_queue_Object *self) {
kqueue_queue_internal_close(self);
Py_TYPE(self)->tp_free(self);
}
static PyObject*
kqueue_queue_close(kqueue_queue_Object *self) {
errno = kqueue_queue_internal_close(self);
if (errno < 0) {
PyErr_SetFromErrno(PyExc_IOError);
return NULL;
}
Py_RETURN_NONE;
}
PyDoc_STRVAR(kqueue_queue_close_doc,
"close() -> None\n\
\n\
Close the kqueue control file descriptor. Further operations on the kqueue\n\
object will raise an exception.");
static PyObject*
kqueue_queue_get_closed(kqueue_queue_Object *self) {
if (self->kqfd < 0)
Py_RETURN_TRUE;
else
Py_RETURN_FALSE;
}
static PyObject*
kqueue_queue_fileno(kqueue_queue_Object *self) {
if (self->kqfd < 0)
return kqueue_queue_err_closed();
return PyInt_FromLong(self->kqfd);
}
PyDoc_STRVAR(kqueue_queue_fileno_doc,
"fileno() -> int\n\
\n\
Return the kqueue control file descriptor.");
static PyObject*
kqueue_queue_fromfd(PyObject *cls, PyObject *args) {
SOCKET fd;
if (!PyArg_ParseTuple(args, "i:fromfd", &fd))
return NULL;
return newKqueue_Object((PyTypeObject*)cls, fd);
}
PyDoc_STRVAR(kqueue_queue_fromfd_doc,
"fromfd(fd) -> kqueue\n\
\n\
Create a kqueue object from a given control fd.");
static PyObject *
kqueue_queue_control(kqueue_queue_Object *self, PyObject *args) {
int nevents = 0;
int gotevents = 0;
int nchanges = 0;
int i = 0;
PyObject *otimeout = NULL;
PyObject *ch = NULL;
PyObject *it = NULL, *ei = NULL;
PyObject *result = NULL;
struct kevent *evl = NULL;
struct kevent *chl = NULL;
struct timespec timeoutspec;
struct timespec *ptimeoutspec;
if (self->kqfd < 0)
return kqueue_queue_err_closed();
if (!PyArg_ParseTuple(args, "Oi|O:control", &ch, &nevents, &otimeout))
return NULL;
if (nevents < 0) {
PyErr_Format(PyExc_ValueError,
"Length of eventlist must be 0 or positive, got %d",
nchanges);
return NULL;
}
if (ch != NULL && ch != Py_None) {
it = PyObject_GetIter(ch);
if (it == NULL) {
PyErr_SetString(PyExc_TypeError,
"changelist is not iterable");
return NULL;
}
nchanges = PyObject_Size(ch);
if (nchanges < 0) {
return NULL;
}
}
if (otimeout == Py_None || otimeout == NULL) {
ptimeoutspec = NULL;
} else if (PyNumber_Check(otimeout)) {
double timeout;
long seconds;
timeout = PyFloat_AsDouble(otimeout);
if (timeout == -1 && PyErr_Occurred())
return NULL;
if (timeout > (double)LONG_MAX) {
PyErr_SetString(PyExc_OverflowError,
"timeout period too long");
return NULL;
}
if (timeout < 0) {
PyErr_SetString(PyExc_ValueError,
"timeout must be positive or None");
return NULL;
}
seconds = (long)timeout;
timeout = timeout - (double)seconds;
timeoutspec.tv_sec = seconds;
timeoutspec.tv_nsec = (long)(timeout * 1E9);
ptimeoutspec = &timeoutspec;
} else {
PyErr_Format(PyExc_TypeError,
"timeout argument must be an number "
"or None, got %.200s",
Py_TYPE(otimeout)->tp_name);
return NULL;
}
if (nchanges) {
chl = PyMem_New(struct kevent, nchanges);
if (chl == NULL) {
PyErr_NoMemory();
return NULL;
}
while ((ei = PyIter_Next(it)) != NULL) {
if (!kqueue_event_Check(ei)) {
Py_DECREF(ei);
PyErr_SetString(PyExc_TypeError,
"changelist must be an iterable of "
"select.kevent objects");
goto error;
} else {
chl[i] = ((kqueue_event_Object *)ei)->e;
}
Py_DECREF(ei);
}
}
Py_CLEAR(it);
if (nevents) {
evl = PyMem_New(struct kevent, nevents);
if (evl == NULL) {
PyErr_NoMemory();
return NULL;
}
}
Py_BEGIN_ALLOW_THREADS
gotevents = kevent(self->kqfd, chl, nchanges,
evl, nevents, ptimeoutspec);
Py_END_ALLOW_THREADS
if (gotevents == -1) {
PyErr_SetFromErrno(PyExc_OSError);
goto error;
}
result = PyList_New(gotevents);
if (result == NULL) {
goto error;
}
for (i=0; i < gotevents; i++) {
kqueue_event_Object *ch;
ch = PyObject_New(kqueue_event_Object, &kqueue_event_Type);
if (ch == NULL) {
goto error;
}
ch->e = evl[i];
PyList_SET_ITEM(result, i, (PyObject *)ch);
}
PyMem_Free(chl);
PyMem_Free(evl);
return result;
error:
PyMem_Free(chl);
PyMem_Free(evl);
Py_XDECREF(result);
Py_XDECREF(it);
return NULL;
}
PyDoc_STRVAR(kqueue_queue_control_doc,
"control(changelist, max_events[, timeout=None]) -> eventlist\n\
\n\
Calls the kernel kevent function.\n\
- changelist must be a list of kevent objects describing the changes\n\
to be made to the kernel's watch list or None.\n\
- max_events lets you specify the maximum number of events that the\n\
kernel will return.\n\
- timeout is the maximum time to wait in seconds, or else None,\n\
to wait forever. timeout accepts floats for smaller timeouts, too.");
static PyMethodDef kqueue_queue_methods[] = {
{
"fromfd", (PyCFunction)kqueue_queue_fromfd,
METH_VARARGS | METH_CLASS, kqueue_queue_fromfd_doc
},
{
"close", (PyCFunction)kqueue_queue_close, METH_NOARGS,
kqueue_queue_close_doc
},
{
"fileno", (PyCFunction)kqueue_queue_fileno, METH_NOARGS,
kqueue_queue_fileno_doc
},
{
"control", (PyCFunction)kqueue_queue_control,
METH_VARARGS , kqueue_queue_control_doc
},
{NULL, NULL},
};
static PyGetSetDef kqueue_queue_getsetlist[] = {
{
"closed", (getter)kqueue_queue_get_closed, NULL,
"True if the kqueue handler is closed"
},
{0},
};
PyDoc_STRVAR(kqueue_queue_doc,
"Kqueue syscall wrapper.\n\
\n\
For example, to start watching a socket for input:\n\
>>> kq = kqueue()\n\
>>> sock = socket()\n\
>>> sock.connect((host, port))\n\
>>> kq.control([kevent(sock, KQ_FILTER_WRITE, KQ_EV_ADD)], 0)\n\
\n\
To wait one second for it to become writeable:\n\
>>> kq.control(None, 1, 1000)\n\
\n\
To stop listening:\n\
>>> kq.control([kevent(sock, KQ_FILTER_WRITE, KQ_EV_DELETE)], 0)");
static PyTypeObject kqueue_queue_Type = {
PyVarObject_HEAD_INIT(NULL, 0)
"select.kqueue",
sizeof(kqueue_queue_Object),
0,
(destructor)kqueue_queue_dealloc,
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
kqueue_queue_doc,
0,
0,
0,
0,
0,
0,
kqueue_queue_methods,
0,
kqueue_queue_getsetlist,
0,
0,
0,
0,
0,
0,
0,
kqueue_queue_new,
0,
};
#endif
PyDoc_STRVAR(select_doc,
"select(rlist, wlist, xlist[, timeout]) -> (rlist, wlist, xlist)\n\
\n\
Wait until one or more file descriptors are ready for some kind of I/O.\n\
The first three arguments are sequences of file descriptors to be waited for:\n\
rlist -- wait until ready for reading\n\
wlist -- wait until ready for writing\n\
xlist -- wait for an ``exceptional condition''\n\
If only one kind of condition is required, pass [] for the other lists.\n\
A file descriptor is either a socket or file object, or a small integer\n\
gotten from a fileno() method call on one of those.\n\
\n\
The optional 4th argument specifies a timeout in seconds; it may be\n\
a floating point number to specify fractions of seconds. If it is absent\n\
or None, the call will never time out.\n\
\n\
The return value is a tuple of three lists corresponding to the first three\n\
arguments; each contains the subset of the corresponding file descriptors\n\
that are ready.\n\
\n\
*** IMPORTANT NOTICE ***\n\
On Windows and OpenVMS, only sockets are supported; on Unix, all file\n\
descriptors can be used.");
static PyMethodDef select_methods[] = {
{"select", select_select, METH_VARARGS, select_doc},
#if defined(HAVE_POLL)
{"poll", select_poll, METH_NOARGS, poll_doc},
#endif
{0, 0},
};
PyDoc_STRVAR(module_doc,
"This module supports asynchronous I/O on multiple file descriptors.\n\
\n\
*** IMPORTANT NOTICE ***\n\
On Windows and OpenVMS, only sockets are supported; on Unix, all file descriptors.");
PyMODINIT_FUNC
initselect(void) {
PyObject *m;
m = Py_InitModule3("select", select_methods, module_doc);
if (m == NULL)
return;
SelectError = PyErr_NewException("select.error", NULL, NULL);
Py_INCREF(SelectError);
PyModule_AddObject(m, "error", SelectError);
#if defined(HAVE_POLL)
#if defined(__APPLE__)
if (select_have_broken_poll()) {
if (PyObject_DelAttrString(m, "poll") == -1) {
PyErr_Clear();
}
} else {
#else
{
#endif
Py_TYPE(&poll_Type) = &PyType_Type;
PyModule_AddIntConstant(m, "POLLIN", POLLIN);
PyModule_AddIntConstant(m, "POLLPRI", POLLPRI);
PyModule_AddIntConstant(m, "POLLOUT", POLLOUT);
PyModule_AddIntConstant(m, "POLLERR", POLLERR);
PyModule_AddIntConstant(m, "POLLHUP", POLLHUP);
PyModule_AddIntConstant(m, "POLLNVAL", POLLNVAL);
#if defined(POLLRDNORM)
PyModule_AddIntConstant(m, "POLLRDNORM", POLLRDNORM);
#endif
#if defined(POLLRDBAND)
PyModule_AddIntConstant(m, "POLLRDBAND", POLLRDBAND);
#endif
#if defined(POLLWRNORM)
PyModule_AddIntConstant(m, "POLLWRNORM", POLLWRNORM);
#endif
#if defined(POLLWRBAND)
PyModule_AddIntConstant(m, "POLLWRBAND", POLLWRBAND);
#endif
#if defined(POLLMSG)
PyModule_AddIntConstant(m, "POLLMSG", POLLMSG);
#endif
}
#endif
#if defined(HAVE_EPOLL)
Py_TYPE(&pyEpoll_Type) = &PyType_Type;
if (PyType_Ready(&pyEpoll_Type) < 0)
return;
Py_INCREF(&pyEpoll_Type);
PyModule_AddObject(m, "epoll", (PyObject *) &pyEpoll_Type);
PyModule_AddIntConstant(m, "EPOLLIN", EPOLLIN);
PyModule_AddIntConstant(m, "EPOLLOUT", EPOLLOUT);
PyModule_AddIntConstant(m, "EPOLLPRI", EPOLLPRI);
PyModule_AddIntConstant(m, "EPOLLERR", EPOLLERR);
PyModule_AddIntConstant(m, "EPOLLHUP", EPOLLHUP);
PyModule_AddIntConstant(m, "EPOLLET", EPOLLET);
#if defined(EPOLLONESHOT)
PyModule_AddIntConstant(m, "EPOLLONESHOT", EPOLLONESHOT);
#endif
PyModule_AddIntConstant(m, "EPOLLRDNORM", EPOLLRDNORM);
PyModule_AddIntConstant(m, "EPOLLRDBAND", EPOLLRDBAND);
PyModule_AddIntConstant(m, "EPOLLWRNORM", EPOLLWRNORM);
PyModule_AddIntConstant(m, "EPOLLWRBAND", EPOLLWRBAND);
PyModule_AddIntConstant(m, "EPOLLMSG", EPOLLMSG);
#endif
#if defined(HAVE_KQUEUE)
kqueue_event_Type.tp_new = PyType_GenericNew;
Py_TYPE(&kqueue_event_Type) = &PyType_Type;
if(PyType_Ready(&kqueue_event_Type) < 0)
return;
Py_INCREF(&kqueue_event_Type);
PyModule_AddObject(m, "kevent", (PyObject *)&kqueue_event_Type);
Py_TYPE(&kqueue_queue_Type) = &PyType_Type;
if(PyType_Ready(&kqueue_queue_Type) < 0)
return;
Py_INCREF(&kqueue_queue_Type);
PyModule_AddObject(m, "kqueue", (PyObject *)&kqueue_queue_Type);
PyModule_AddIntConstant(m, "KQ_FILTER_READ", EVFILT_READ);
PyModule_AddIntConstant(m, "KQ_FILTER_WRITE", EVFILT_WRITE);
PyModule_AddIntConstant(m, "KQ_FILTER_AIO", EVFILT_AIO);
PyModule_AddIntConstant(m, "KQ_FILTER_VNODE", EVFILT_VNODE);
PyModule_AddIntConstant(m, "KQ_FILTER_PROC", EVFILT_PROC);
#if defined(EVFILT_NETDEV)
PyModule_AddIntConstant(m, "KQ_FILTER_NETDEV", EVFILT_NETDEV);
#endif
PyModule_AddIntConstant(m, "KQ_FILTER_SIGNAL", EVFILT_SIGNAL);
PyModule_AddIntConstant(m, "KQ_FILTER_TIMER", EVFILT_TIMER);
PyModule_AddIntConstant(m, "KQ_EV_ADD", EV_ADD);
PyModule_AddIntConstant(m, "KQ_EV_DELETE", EV_DELETE);
PyModule_AddIntConstant(m, "KQ_EV_ENABLE", EV_ENABLE);
PyModule_AddIntConstant(m, "KQ_EV_DISABLE", EV_DISABLE);
PyModule_AddIntConstant(m, "KQ_EV_ONESHOT", EV_ONESHOT);
PyModule_AddIntConstant(m, "KQ_EV_CLEAR", EV_CLEAR);
PyModule_AddIntConstant(m, "KQ_EV_SYSFLAGS", EV_SYSFLAGS);
PyModule_AddIntConstant(m, "KQ_EV_FLAG1", EV_FLAG1);
PyModule_AddIntConstant(m, "KQ_EV_EOF", EV_EOF);
PyModule_AddIntConstant(m, "KQ_EV_ERROR", EV_ERROR);
PyModule_AddIntConstant(m, "KQ_NOTE_LOWAT", NOTE_LOWAT);
PyModule_AddIntConstant(m, "KQ_NOTE_DELETE", NOTE_DELETE);
PyModule_AddIntConstant(m, "KQ_NOTE_WRITE", NOTE_WRITE);
PyModule_AddIntConstant(m, "KQ_NOTE_EXTEND", NOTE_EXTEND);
PyModule_AddIntConstant(m, "KQ_NOTE_ATTRIB", NOTE_ATTRIB);
PyModule_AddIntConstant(m, "KQ_NOTE_LINK", NOTE_LINK);
PyModule_AddIntConstant(m, "KQ_NOTE_RENAME", NOTE_RENAME);
PyModule_AddIntConstant(m, "KQ_NOTE_REVOKE", NOTE_REVOKE);
PyModule_AddIntConstant(m, "KQ_NOTE_EXIT", NOTE_EXIT);
PyModule_AddIntConstant(m, "KQ_NOTE_FORK", NOTE_FORK);
PyModule_AddIntConstant(m, "KQ_NOTE_EXEC", NOTE_EXEC);
PyModule_AddIntConstant(m, "KQ_NOTE_PCTRLMASK", NOTE_PCTRLMASK);
PyModule_AddIntConstant(m, "KQ_NOTE_PDATAMASK", NOTE_PDATAMASK);
PyModule_AddIntConstant(m, "KQ_NOTE_TRACK", NOTE_TRACK);
PyModule_AddIntConstant(m, "KQ_NOTE_CHILD", NOTE_CHILD);
PyModule_AddIntConstant(m, "KQ_NOTE_TRACKERR", NOTE_TRACKERR);
#if defined(EVFILT_NETDEV)
PyModule_AddIntConstant(m, "KQ_NOTE_LINKUP", NOTE_LINKUP);
PyModule_AddIntConstant(m, "KQ_NOTE_LINKDOWN", NOTE_LINKDOWN);
PyModule_AddIntConstant(m, "KQ_NOTE_LINKINV", NOTE_LINKINV);
#endif
#endif
}