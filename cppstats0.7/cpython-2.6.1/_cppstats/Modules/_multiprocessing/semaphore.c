#include "multiprocessing.h"
enum { RECURSIVE_MUTEX, SEMAPHORE };
typedef struct {
PyObject_HEAD
SEM_HANDLE handle;
long last_tid;
int count;
int maxvalue;
int kind;
} SemLockObject;
#define ISMINE(o) (o->count > 0 && PyThread_get_thread_ident() == o->last_tid)
#if defined(MS_WINDOWS)
#define SEM_FAILED NULL
#define SEM_CLEAR_ERROR() SetLastError(0)
#define SEM_GET_LAST_ERROR() GetLastError()
#define SEM_CREATE(name, val, max) CreateSemaphore(NULL, val, max, NULL)
#define SEM_CLOSE(sem) (CloseHandle(sem) ? 0 : -1)
#define SEM_GETVALUE(sem, pval) _GetSemaphoreValue(sem, pval)
#define SEM_UNLINK(name) 0
static int
_GetSemaphoreValue(HANDLE handle, long *value) {
long previous;
switch (WaitForSingleObject(handle, 0)) {
case WAIT_OBJECT_0:
if (!ReleaseSemaphore(handle, 1, &previous))
return MP_STANDARD_ERROR;
*value = previous + 1;
return 0;
case WAIT_TIMEOUT:
*value = 0;
return 0;
default:
return MP_STANDARD_ERROR;
}
}
static PyObject *
semlock_acquire(SemLockObject *self, PyObject *args, PyObject *kwds) {
int blocking = 1;
double timeout;
PyObject *timeout_obj = Py_None;
DWORD res, full_msecs, msecs, start, ticks;
static char *kwlist[] = {"block", "timeout", NULL};
if (!PyArg_ParseTupleAndKeywords(args, kwds, "|iO", kwlist,
&blocking, &timeout_obj))
return NULL;
if (!blocking) {
full_msecs = 0;
} else if (timeout_obj == Py_None) {
full_msecs = INFINITE;
} else {
timeout = PyFloat_AsDouble(timeout_obj);
if (PyErr_Occurred())
return NULL;
timeout *= 1000.0;
if (timeout < 0.0) {
timeout = 0.0;
} else if (timeout >= 0.5 * INFINITE) {
PyErr_SetString(PyExc_OverflowError,
"timeout is too large");
return NULL;
}
full_msecs = (DWORD)(timeout + 0.5);
}
if (self->kind == RECURSIVE_MUTEX && ISMINE(self)) {
++self->count;
Py_RETURN_TRUE;
}
if (WaitForSingleObject(self->handle, 0) == WAIT_OBJECT_0) {
self->last_tid = GetCurrentThreadId();
++self->count;
Py_RETURN_TRUE;
}
msecs = full_msecs;
start = GetTickCount();
for ( ; ; ) {
HANDLE handles[2] = {self->handle, sigint_event};
Py_BEGIN_ALLOW_THREADS
ResetEvent(sigint_event);
res = WaitForMultipleObjects(2, handles, FALSE, msecs);
Py_END_ALLOW_THREADS
if (res != WAIT_OBJECT_0 + 1)
break;
Sleep(1);
if (PyErr_CheckSignals())
return NULL;
if (msecs != INFINITE) {
ticks = GetTickCount();
if ((DWORD)(ticks - start) >= full_msecs)
Py_RETURN_FALSE;
msecs = full_msecs - (ticks - start);
}
}
switch (res) {
case WAIT_TIMEOUT:
Py_RETURN_FALSE;
case WAIT_OBJECT_0:
self->last_tid = GetCurrentThreadId();
++self->count;
Py_RETURN_TRUE;
case WAIT_FAILED:
return PyErr_SetFromWindowsErr(0);
default:
PyErr_Format(PyExc_RuntimeError, "WaitForSingleObject() or "
"WaitForMultipleObjects() gave unrecognized "
"value %d", res);
return NULL;
}
}
static PyObject *
semlock_release(SemLockObject *self, PyObject *args) {
if (self->kind == RECURSIVE_MUTEX) {
if (!ISMINE(self)) {
PyErr_SetString(PyExc_AssertionError, "attempt to "
"release recursive lock not owned "
"by thread");
return NULL;
}
if (self->count > 1) {
--self->count;
Py_RETURN_NONE;
}
assert(self->count == 1);
}
if (!ReleaseSemaphore(self->handle, 1, NULL)) {
if (GetLastError() == ERROR_TOO_MANY_POSTS) {
PyErr_SetString(PyExc_ValueError, "semaphore or lock "
"released too many times");
return NULL;
} else {
return PyErr_SetFromWindowsErr(0);
}
}
--self->count;
Py_RETURN_NONE;
}
#else
#define SEM_CLEAR_ERROR()
#define SEM_GET_LAST_ERROR() 0
#define SEM_CREATE(name, val, max) sem_open(name, O_CREAT | O_EXCL, 0600, val)
#define SEM_CLOSE(sem) sem_close(sem)
#define SEM_GETVALUE(sem, pval) sem_getvalue(sem, pval)
#define SEM_UNLINK(name) sem_unlink(name)
#if HAVE_BROKEN_SEM_UNLINK
#define sem_unlink(name) 0
#endif
#if !HAVE_SEM_TIMEDWAIT
#define sem_timedwait(sem,deadline) sem_timedwait_save(sem,deadline,_save)
int
sem_timedwait_save(sem_t *sem, struct timespec *deadline, PyThreadState *_save) {
int res;
unsigned long delay, difference;
struct timeval now, tvdeadline, tvdelay;
errno = 0;
tvdeadline.tv_sec = deadline->tv_sec;
tvdeadline.tv_usec = deadline->tv_nsec / 1000;
for (delay = 0 ; ; delay += 1000) {
if (sem_trywait(sem) == 0)
return 0;
else if (errno != EAGAIN)
return MP_STANDARD_ERROR;
if (gettimeofday(&now, NULL) < 0)
return MP_STANDARD_ERROR;
if (tvdeadline.tv_sec < now.tv_sec ||
(tvdeadline.tv_sec == now.tv_sec &&
tvdeadline.tv_usec <= now.tv_usec)) {
errno = ETIMEDOUT;
return MP_STANDARD_ERROR;
}
difference = (tvdeadline.tv_sec - now.tv_sec) * 1000000 +
(tvdeadline.tv_usec - now.tv_usec);
if (delay > 20000)
delay = 20000;
if (delay > difference)
delay = difference;
tvdelay.tv_sec = delay / 1000000;
tvdelay.tv_usec = delay % 1000000;
if (select(0, NULL, NULL, NULL, &tvdelay) < 0)
return MP_STANDARD_ERROR;
Py_BLOCK_THREADS
res = PyErr_CheckSignals();
Py_UNBLOCK_THREADS
if (res) {
errno = EINTR;
return MP_EXCEPTION_HAS_BEEN_SET;
}
}
}
#endif
static PyObject *
semlock_acquire(SemLockObject *self, PyObject *args, PyObject *kwds) {
int blocking = 1, res;
double timeout;
PyObject *timeout_obj = Py_None;
struct timespec deadline = {0};
struct timeval now;
long sec, nsec;
static char *kwlist[] = {"block", "timeout", NULL};
if (!PyArg_ParseTupleAndKeywords(args, kwds, "|iO", kwlist,
&blocking, &timeout_obj))
return NULL;
if (self->kind == RECURSIVE_MUTEX && ISMINE(self)) {
++self->count;
Py_RETURN_TRUE;
}
if (timeout_obj != Py_None) {
timeout = PyFloat_AsDouble(timeout_obj);
if (PyErr_Occurred())
return NULL;
if (timeout < 0.0)
timeout = 0.0;
if (gettimeofday(&now, NULL) < 0) {
PyErr_SetFromErrno(PyExc_OSError);
return NULL;
}
sec = (long) timeout;
nsec = (long) (1e9 * (timeout - sec) + 0.5);
deadline.tv_sec = now.tv_sec + sec;
deadline.tv_nsec = now.tv_usec * 1000 + nsec;
deadline.tv_sec += (deadline.tv_nsec / 1000000000);
deadline.tv_nsec %= 1000000000;
}
do {
Py_BEGIN_ALLOW_THREADS
if (blocking && timeout_obj == Py_None)
res = sem_wait(self->handle);
else if (!blocking)
res = sem_trywait(self->handle);
else
res = sem_timedwait(self->handle, &deadline);
Py_END_ALLOW_THREADS
if (res == MP_EXCEPTION_HAS_BEEN_SET)
break;
} while (res < 0 && errno == EINTR && !PyErr_CheckSignals());
if (res < 0) {
if (errno == EAGAIN || errno == ETIMEDOUT)
Py_RETURN_FALSE;
else if (errno == EINTR)
return NULL;
else
return PyErr_SetFromErrno(PyExc_OSError);
}
++self->count;
self->last_tid = PyThread_get_thread_ident();
Py_RETURN_TRUE;
}
static PyObject *
semlock_release(SemLockObject *self, PyObject *args) {
if (self->kind == RECURSIVE_MUTEX) {
if (!ISMINE(self)) {
PyErr_SetString(PyExc_AssertionError, "attempt to "
"release recursive lock not owned "
"by thread");
return NULL;
}
if (self->count > 1) {
--self->count;
Py_RETURN_NONE;
}
assert(self->count == 1);
} else {
#if HAVE_BROKEN_SEM_GETVALUE
if (self->maxvalue == 1) {
if (sem_trywait(self->handle) < 0) {
if (errno != EAGAIN) {
PyErr_SetFromErrno(PyExc_OSError);
return NULL;
}
} else {
if (sem_post(self->handle) < 0) {
PyErr_SetFromErrno(PyExc_OSError);
return NULL;
}
PyErr_SetString(PyExc_ValueError, "semaphore "
"or lock released too many "
"times");
return NULL;
}
}
#else
int sval;
if (sem_getvalue(self->handle, &sval) < 0) {
return PyErr_SetFromErrno(PyExc_OSError);
} else if (sval >= self->maxvalue) {
PyErr_SetString(PyExc_ValueError, "semaphore or lock "
"released too many times");
return NULL;
}
#endif
}
if (sem_post(self->handle) < 0)
return PyErr_SetFromErrno(PyExc_OSError);
--self->count;
Py_RETURN_NONE;
}
#endif
static PyObject *
newsemlockobject(PyTypeObject *type, SEM_HANDLE handle, int kind, int maxvalue) {
SemLockObject *self;
self = PyObject_New(SemLockObject, type);
if (!self)
return NULL;
self->handle = handle;
self->kind = kind;
self->count = 0;
self->last_tid = 0;
self->maxvalue = maxvalue;
return (PyObject*)self;
}
static PyObject *
semlock_new(PyTypeObject *type, PyObject *args, PyObject *kwds) {
char buffer[256];
SEM_HANDLE handle = SEM_FAILED;
int kind, maxvalue, value;
PyObject *result;
static char *kwlist[] = {"kind", "value", "maxvalue", NULL};
static int counter = 0;
if (!PyArg_ParseTupleAndKeywords(args, kwds, "iii", kwlist,
&kind, &value, &maxvalue))
return NULL;
if (kind != RECURSIVE_MUTEX && kind != SEMAPHORE) {
PyErr_SetString(PyExc_ValueError, "unrecognized kind");
return NULL;
}
PyOS_snprintf(buffer, sizeof(buffer), "/mp%d-%d", getpid(), counter++);
SEM_CLEAR_ERROR();
handle = SEM_CREATE(buffer, value, maxvalue);
if (handle == SEM_FAILED || SEM_GET_LAST_ERROR() != 0)
goto failure;
if (SEM_UNLINK(buffer) < 0)
goto failure;
result = newsemlockobject(type, handle, kind, maxvalue);
if (!result)
goto failure;
return result;
failure:
if (handle != SEM_FAILED)
SEM_CLOSE(handle);
mp_SetError(NULL, MP_STANDARD_ERROR);
return NULL;
}
static PyObject *
semlock_rebuild(PyTypeObject *type, PyObject *args) {
SEM_HANDLE handle;
int kind, maxvalue;
if (!PyArg_ParseTuple(args, F_SEM_HANDLE "ii",
&handle, &kind, &maxvalue))
return NULL;
return newsemlockobject(type, handle, kind, maxvalue);
}
static void
semlock_dealloc(SemLockObject* self) {
if (self->handle != SEM_FAILED)
SEM_CLOSE(self->handle);
PyObject_Del(self);
}
static PyObject *
semlock_count(SemLockObject *self) {
return PyInt_FromLong((long)self->count);
}
static PyObject *
semlock_ismine(SemLockObject *self) {
return PyBool_FromLong(ISMINE(self));
}
static PyObject *
semlock_getvalue(SemLockObject *self) {
#if HAVE_BROKEN_SEM_GETVALUE
PyErr_SetNone(PyExc_NotImplementedError);
return NULL;
#else
int sval;
if (SEM_GETVALUE(self->handle, &sval) < 0)
return mp_SetError(NULL, MP_STANDARD_ERROR);
if (sval < 0)
sval = 0;
return PyInt_FromLong((long)sval);
#endif
}
static PyObject *
semlock_iszero(SemLockObject *self) {
int sval;
#if HAVE_BROKEN_SEM_GETVALUE
if (sem_trywait(self->handle) < 0) {
if (errno == EAGAIN)
Py_RETURN_TRUE;
return mp_SetError(NULL, MP_STANDARD_ERROR);
} else {
if (sem_post(self->handle) < 0)
return mp_SetError(NULL, MP_STANDARD_ERROR);
Py_RETURN_FALSE;
}
#else
if (SEM_GETVALUE(self->handle, &sval) < 0)
return mp_SetError(NULL, MP_STANDARD_ERROR);
return PyBool_FromLong((long)sval == 0);
#endif
}
static PyObject *
semlock_afterfork(SemLockObject *self) {
self->count = 0;
Py_RETURN_NONE;
}
static PyMethodDef semlock_methods[] = {
{
"acquire", (PyCFunction)semlock_acquire, METH_VARARGS | METH_KEYWORDS,
"acquire the semaphore/lock"
},
{
"release", (PyCFunction)semlock_release, METH_NOARGS,
"release the semaphore/lock"
},
{
"__enter__", (PyCFunction)semlock_acquire, METH_VARARGS,
"enter the semaphore/lock"
},
{
"__exit__", (PyCFunction)semlock_release, METH_VARARGS,
"exit the semaphore/lock"
},
{
"_count", (PyCFunction)semlock_count, METH_NOARGS,
"num of `acquire()`s minus num of `release()`s for this process"
},
{
"_is_mine", (PyCFunction)semlock_ismine, METH_NOARGS,
"whether the lock is owned by this thread"
},
{
"_get_value", (PyCFunction)semlock_getvalue, METH_NOARGS,
"get the value of the semaphore"
},
{
"_is_zero", (PyCFunction)semlock_iszero, METH_NOARGS,
"returns whether semaphore has value zero"
},
{
"_rebuild", (PyCFunction)semlock_rebuild, METH_VARARGS | METH_CLASS,
""
},
{
"_after_fork", (PyCFunction)semlock_afterfork, METH_NOARGS,
"rezero the net acquisition count after fork()"
},
{NULL}
};
static PyMemberDef semlock_members[] = {
{
"handle", T_SEM_HANDLE, offsetof(SemLockObject, handle), READONLY,
""
},
{
"kind", T_INT, offsetof(SemLockObject, kind), READONLY,
""
},
{
"maxvalue", T_INT, offsetof(SemLockObject, maxvalue), READONLY,
""
},
{NULL}
};
PyTypeObject SemLockType = {
PyVarObject_HEAD_INIT(NULL, 0)
"_multiprocessing.SemLock",
sizeof(SemLockObject),
0,
(destructor)semlock_dealloc,
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
Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
"Semaphore/Mutex type",
0,
0,
0,
0,
0,
0,
semlock_methods,
semlock_members,
0,
0,
0,
0,
0,
0,
0,
0,
semlock_new,
};