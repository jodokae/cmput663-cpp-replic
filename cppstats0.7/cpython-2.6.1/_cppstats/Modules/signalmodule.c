#include "Python.h"
#include "intrcheck.h"
#if defined(MS_WINDOWS)
#include <process.h>
#endif
#include <signal.h>
#include <sys/stat.h>
#if defined(HAVE_SYS_TIME_H)
#include <sys/time.h>
#endif
#if !defined(SIG_ERR)
#define SIG_ERR ((PyOS_sighandler_t)(-1))
#endif
#if defined(PYOS_OS2) && !defined(PYCC_GCC)
#define NSIG 12
#include <process.h>
#endif
#if !defined(NSIG)
#if defined(_NSIG)
#define NSIG _NSIG
#elif defined(_SIGMAX)
#define NSIG (_SIGMAX + 1)
#elif defined(SIGMAX)
#define NSIG (SIGMAX + 1)
#else
#define NSIG 64
#endif
#endif
#if defined(WITH_THREAD)
#include <sys/types.h>
#include "pythread.h"
static long main_thread;
static pid_t main_pid;
#endif
static struct {
int tripped;
PyObject *func;
} Handlers[NSIG];
static sig_atomic_t wakeup_fd = -1;
static volatile sig_atomic_t is_tripped = 0;
static PyObject *DefaultHandler;
static PyObject *IgnoreHandler;
static PyObject *IntHandler;
static PyOS_sighandler_t old_siginthandler = SIG_DFL;
#if defined(HAVE_GETITIMER)
static PyObject *ItimerError;
static void
timeval_from_double(double d, struct timeval *tv) {
tv->tv_sec = floor(d);
tv->tv_usec = fmod(d, 1.0) * 1000000.0;
}
Py_LOCAL_INLINE(double)
double_from_timeval(struct timeval *tv) {
return tv->tv_sec + (double)(tv->tv_usec / 1000000.0);
}
static PyObject *
itimer_retval(struct itimerval *iv) {
PyObject *r, *v;
r = PyTuple_New(2);
if (r == NULL)
return NULL;
if(!(v = PyFloat_FromDouble(double_from_timeval(&iv->it_value)))) {
Py_DECREF(r);
return NULL;
}
PyTuple_SET_ITEM(r, 0, v);
if(!(v = PyFloat_FromDouble(double_from_timeval(&iv->it_interval)))) {
Py_DECREF(r);
return NULL;
}
PyTuple_SET_ITEM(r, 1, v);
return r;
}
#endif
static PyObject *
signal_default_int_handler(PyObject *self, PyObject *args) {
PyErr_SetNone(PyExc_KeyboardInterrupt);
return NULL;
}
PyDoc_STRVAR(default_int_handler_doc,
"default_int_handler(...)\n\
\n\
The default handler for SIGINT installed by Python.\n\
It raises KeyboardInterrupt.");
static int
checksignals_witharg(void * unused) {
return PyErr_CheckSignals();
}
static void
signal_handler(int sig_num) {
#if defined(WITH_THREAD)
#if defined(WITH_PTH)
if (PyThread_get_thread_ident() != main_thread) {
pth_raise(*(pth_t *) main_thread, sig_num);
return;
}
#endif
if (getpid() == main_pid) {
#endif
Handlers[sig_num].tripped = 1;
is_tripped = 1;
Py_AddPendingCall(checksignals_witharg, NULL);
if (wakeup_fd != -1)
write(wakeup_fd, "\0", 1);
#if defined(WITH_THREAD)
}
#endif
#if defined(SIGCHLD)
if (sig_num == SIGCHLD) {
return;
}
#endif
PyOS_setsig(sig_num, signal_handler);
}
#if defined(HAVE_ALARM)
static PyObject *
signal_alarm(PyObject *self, PyObject *args) {
int t;
if (!PyArg_ParseTuple(args, "i:alarm", &t))
return NULL;
return PyInt_FromLong((long)alarm(t));
}
PyDoc_STRVAR(alarm_doc,
"alarm(seconds)\n\
\n\
Arrange for SIGALRM to arrive after the given number of seconds.");
#endif
#if defined(HAVE_PAUSE)
static PyObject *
signal_pause(PyObject *self) {
Py_BEGIN_ALLOW_THREADS
(void)pause();
Py_END_ALLOW_THREADS
if (PyErr_CheckSignals())
return NULL;
Py_INCREF(Py_None);
return Py_None;
}
PyDoc_STRVAR(pause_doc,
"pause()\n\
\n\
Wait until a signal arrives.");
#endif
static PyObject *
signal_signal(PyObject *self, PyObject *args) {
PyObject *obj;
int sig_num;
PyObject *old_handler;
void (*func)(int);
if (!PyArg_ParseTuple(args, "iO:signal", &sig_num, &obj))
return NULL;
#if defined(WITH_THREAD)
if (PyThread_get_thread_ident() != main_thread) {
PyErr_SetString(PyExc_ValueError,
"signal only works in main thread");
return NULL;
}
#endif
if (sig_num < 1 || sig_num >= NSIG) {
PyErr_SetString(PyExc_ValueError,
"signal number out of range");
return NULL;
}
if (obj == IgnoreHandler)
func = SIG_IGN;
else if (obj == DefaultHandler)
func = SIG_DFL;
else if (!PyCallable_Check(obj)) {
PyErr_SetString(PyExc_TypeError,
"signal handler must be signal.SIG_IGN, signal.SIG_DFL, or a callable object");
return NULL;
} else
func = signal_handler;
if (PyOS_setsig(sig_num, func) == SIG_ERR) {
PyErr_SetFromErrno(PyExc_RuntimeError);
return NULL;
}
old_handler = Handlers[sig_num].func;
Handlers[sig_num].tripped = 0;
Py_INCREF(obj);
Handlers[sig_num].func = obj;
return old_handler;
}
PyDoc_STRVAR(signal_doc,
"signal(sig, action) -> action\n\
\n\
Set the action for the given signal. The action can be SIG_DFL,\n\
SIG_IGN, or a callable Python object. The previous action is\n\
returned. See getsignal() for possible return values.\n\
\n\
*** IMPORTANT NOTICE ***\n\
A signal handler function is called with two arguments:\n\
the first is the signal number, the second is the interrupted stack frame.");
static PyObject *
signal_getsignal(PyObject *self, PyObject *args) {
int sig_num;
PyObject *old_handler;
if (!PyArg_ParseTuple(args, "i:getsignal", &sig_num))
return NULL;
if (sig_num < 1 || sig_num >= NSIG) {
PyErr_SetString(PyExc_ValueError,
"signal number out of range");
return NULL;
}
old_handler = Handlers[sig_num].func;
Py_INCREF(old_handler);
return old_handler;
}
PyDoc_STRVAR(getsignal_doc,
"getsignal(sig) -> action\n\
\n\
Return the current action for the given signal. The return value can be:\n\
SIG_IGN -- if the signal is being ignored\n\
SIG_DFL -- if the default action for the signal is in effect\n\
None -- if an unknown handler is in effect\n\
anything else -- the callable Python object used as a handler");
#if defined(HAVE_SIGINTERRUPT)
PyDoc_STRVAR(siginterrupt_doc,
"siginterrupt(sig, flag) -> None\n\
change system call restart behaviour: if flag is False, system calls\n\
will be restarted when interrupted by signal sig, else system calls\n\
will be interrupted.");
static PyObject *
signal_siginterrupt(PyObject *self, PyObject *args) {
int sig_num;
int flag;
if (!PyArg_ParseTuple(args, "ii:siginterrupt", &sig_num, &flag))
return NULL;
if (sig_num < 1 || sig_num >= NSIG) {
PyErr_SetString(PyExc_ValueError,
"signal number out of range");
return NULL;
}
if (siginterrupt(sig_num, flag)<0) {
PyErr_SetFromErrno(PyExc_RuntimeError);
return NULL;
}
Py_INCREF(Py_None);
return Py_None;
}
#endif
static PyObject *
signal_set_wakeup_fd(PyObject *self, PyObject *args) {
struct stat buf;
int fd, old_fd;
if (!PyArg_ParseTuple(args, "i:set_wakeup_fd", &fd))
return NULL;
#if defined(WITH_THREAD)
if (PyThread_get_thread_ident() != main_thread) {
PyErr_SetString(PyExc_ValueError,
"set_wakeup_fd only works in main thread");
return NULL;
}
#endif
if (fd != -1 && fstat(fd, &buf) != 0) {
PyErr_SetString(PyExc_ValueError, "invalid fd");
return NULL;
}
old_fd = wakeup_fd;
wakeup_fd = fd;
return PyLong_FromLong(old_fd);
}
PyDoc_STRVAR(set_wakeup_fd_doc,
"set_wakeup_fd(fd) -> fd\n\
\n\
Sets the fd to be written to (with '\\0') when a signal\n\
comes in. A library can use this to wakeup select or poll.\n\
The previous fd is returned.\n\
\n\
The fd must be non-blocking.");
int
PySignal_SetWakeupFd(int fd) {
int old_fd = wakeup_fd;
if (fd < 0)
fd = -1;
wakeup_fd = fd;
return old_fd;
}
#if defined(HAVE_SETITIMER)
static PyObject *
signal_setitimer(PyObject *self, PyObject *args) {
double first;
double interval = 0;
int which;
struct itimerval new, old;
if(!PyArg_ParseTuple(args, "id|d:setitimer", &which, &first, &interval))
return NULL;
timeval_from_double(first, &new.it_value);
timeval_from_double(interval, &new.it_interval);
if (setitimer(which, &new, &old) != 0) {
PyErr_SetFromErrno(ItimerError);
return NULL;
}
return itimer_retval(&old);
}
PyDoc_STRVAR(setitimer_doc,
"setitimer(which, seconds[, interval])\n\
\n\
Sets given itimer (one of ITIMER_REAL, ITIMER_VIRTUAL\n\
or ITIMER_PROF) to fire after value seconds and after\n\
that every interval seconds.\n\
The itimer can be cleared by setting seconds to zero.\n\
\n\
Returns old values as a tuple: (delay, interval).");
#endif
#if defined(HAVE_GETITIMER)
static PyObject *
signal_getitimer(PyObject *self, PyObject *args) {
int which;
struct itimerval old;
if (!PyArg_ParseTuple(args, "i:getitimer", &which))
return NULL;
if (getitimer(which, &old) != 0) {
PyErr_SetFromErrno(ItimerError);
return NULL;
}
return itimer_retval(&old);
}
PyDoc_STRVAR(getitimer_doc,
"getitimer(which)\n\
\n\
Returns current value of given itimer.");
#endif
static PyMethodDef signal_methods[] = {
#if defined(HAVE_ALARM)
{"alarm", signal_alarm, METH_VARARGS, alarm_doc},
#endif
#if defined(HAVE_SETITIMER)
{"setitimer", signal_setitimer, METH_VARARGS, setitimer_doc},
#endif
#if defined(HAVE_GETITIMER)
{"getitimer", signal_getitimer, METH_VARARGS, getitimer_doc},
#endif
{"signal", signal_signal, METH_VARARGS, signal_doc},
{"getsignal", signal_getsignal, METH_VARARGS, getsignal_doc},
{"set_wakeup_fd", signal_set_wakeup_fd, METH_VARARGS, set_wakeup_fd_doc},
#if defined(HAVE_SIGINTERRUPT)
{"siginterrupt", signal_siginterrupt, METH_VARARGS, siginterrupt_doc},
#endif
#if defined(HAVE_PAUSE)
{
"pause", (PyCFunction)signal_pause,
METH_NOARGS,pause_doc
},
#endif
{
"default_int_handler", signal_default_int_handler,
METH_VARARGS, default_int_handler_doc
},
{NULL, NULL}
};
PyDoc_STRVAR(module_doc,
"This module provides mechanisms to use signal handlers in Python.\n\
\n\
Functions:\n\
\n\
alarm() -- cause SIGALRM after a specified time [Unix only]\n\
setitimer() -- cause a signal (described below) after a specified\n\
float time and the timer may restart then [Unix only]\n\
getitimer() -- get current value of timer [Unix only]\n\
signal() -- set the action for a given signal\n\
getsignal() -- get the signal action for a given signal\n\
pause() -- wait until a signal arrives [Unix only]\n\
default_int_handler() -- default SIGINT handler\n\
\n\
signal constants:\n\
SIG_DFL -- used to refer to the system default handler\n\
SIG_IGN -- used to ignore the signal\n\
NSIG -- number of defined signals\n\
SIGINT, SIGTERM, etc. -- signal numbers\n\
\n\
itimer constants:\n\
ITIMER_REAL -- decrements in real time, and delivers SIGALRM upon\n\
expiration\n\
ITIMER_VIRTUAL -- decrements only when the process is executing,\n\
and delivers SIGVTALRM upon expiration\n\
ITIMER_PROF -- decrements both when the process is executing and\n\
when the system is executing on behalf of the process.\n\
Coupled with ITIMER_VIRTUAL, this timer is usually\n\
used to profile the time spent by the application\n\
in user and kernel space. SIGPROF is delivered upon\n\
expiration.\n\
\n\n\
*** IMPORTANT NOTICE ***\n\
A signal handler function is called with two arguments:\n\
the first is the signal number, the second is the interrupted stack frame.");
PyMODINIT_FUNC
initsignal(void) {
PyObject *m, *d, *x;
int i;
#if defined(WITH_THREAD)
main_thread = PyThread_get_thread_ident();
main_pid = getpid();
#endif
m = Py_InitModule3("signal", signal_methods, module_doc);
if (m == NULL)
return;
d = PyModule_GetDict(m);
x = DefaultHandler = PyLong_FromVoidPtr((void *)SIG_DFL);
if (!x || PyDict_SetItemString(d, "SIG_DFL", x) < 0)
goto finally;
x = IgnoreHandler = PyLong_FromVoidPtr((void *)SIG_IGN);
if (!x || PyDict_SetItemString(d, "SIG_IGN", x) < 0)
goto finally;
x = PyInt_FromLong((long)NSIG);
if (!x || PyDict_SetItemString(d, "NSIG", x) < 0)
goto finally;
Py_DECREF(x);
x = IntHandler = PyDict_GetItemString(d, "default_int_handler");
if (!x)
goto finally;
Py_INCREF(IntHandler);
Handlers[0].tripped = 0;
for (i = 1; i < NSIG; i++) {
void (*t)(int);
t = PyOS_getsig(i);
Handlers[i].tripped = 0;
if (t == SIG_DFL)
Handlers[i].func = DefaultHandler;
else if (t == SIG_IGN)
Handlers[i].func = IgnoreHandler;
else
Handlers[i].func = Py_None;
Py_INCREF(Handlers[i].func);
}
if (Handlers[SIGINT].func == DefaultHandler) {
Py_INCREF(IntHandler);
Py_DECREF(Handlers[SIGINT].func);
Handlers[SIGINT].func = IntHandler;
old_siginthandler = PyOS_setsig(SIGINT, signal_handler);
}
#if defined(SIGHUP)
x = PyInt_FromLong(SIGHUP);
PyDict_SetItemString(d, "SIGHUP", x);
Py_XDECREF(x);
#endif
#if defined(SIGINT)
x = PyInt_FromLong(SIGINT);
PyDict_SetItemString(d, "SIGINT", x);
Py_XDECREF(x);
#endif
#if defined(SIGBREAK)
x = PyInt_FromLong(SIGBREAK);
PyDict_SetItemString(d, "SIGBREAK", x);
Py_XDECREF(x);
#endif
#if defined(SIGQUIT)
x = PyInt_FromLong(SIGQUIT);
PyDict_SetItemString(d, "SIGQUIT", x);
Py_XDECREF(x);
#endif
#if defined(SIGILL)
x = PyInt_FromLong(SIGILL);
PyDict_SetItemString(d, "SIGILL", x);
Py_XDECREF(x);
#endif
#if defined(SIGTRAP)
x = PyInt_FromLong(SIGTRAP);
PyDict_SetItemString(d, "SIGTRAP", x);
Py_XDECREF(x);
#endif
#if defined(SIGIOT)
x = PyInt_FromLong(SIGIOT);
PyDict_SetItemString(d, "SIGIOT", x);
Py_XDECREF(x);
#endif
#if defined(SIGABRT)
x = PyInt_FromLong(SIGABRT);
PyDict_SetItemString(d, "SIGABRT", x);
Py_XDECREF(x);
#endif
#if defined(SIGEMT)
x = PyInt_FromLong(SIGEMT);
PyDict_SetItemString(d, "SIGEMT", x);
Py_XDECREF(x);
#endif
#if defined(SIGFPE)
x = PyInt_FromLong(SIGFPE);
PyDict_SetItemString(d, "SIGFPE", x);
Py_XDECREF(x);
#endif
#if defined(SIGKILL)
x = PyInt_FromLong(SIGKILL);
PyDict_SetItemString(d, "SIGKILL", x);
Py_XDECREF(x);
#endif
#if defined(SIGBUS)
x = PyInt_FromLong(SIGBUS);
PyDict_SetItemString(d, "SIGBUS", x);
Py_XDECREF(x);
#endif
#if defined(SIGSEGV)
x = PyInt_FromLong(SIGSEGV);
PyDict_SetItemString(d, "SIGSEGV", x);
Py_XDECREF(x);
#endif
#if defined(SIGSYS)
x = PyInt_FromLong(SIGSYS);
PyDict_SetItemString(d, "SIGSYS", x);
Py_XDECREF(x);
#endif
#if defined(SIGPIPE)
x = PyInt_FromLong(SIGPIPE);
PyDict_SetItemString(d, "SIGPIPE", x);
Py_XDECREF(x);
#endif
#if defined(SIGALRM)
x = PyInt_FromLong(SIGALRM);
PyDict_SetItemString(d, "SIGALRM", x);
Py_XDECREF(x);
#endif
#if defined(SIGTERM)
x = PyInt_FromLong(SIGTERM);
PyDict_SetItemString(d, "SIGTERM", x);
Py_XDECREF(x);
#endif
#if defined(SIGUSR1)
x = PyInt_FromLong(SIGUSR1);
PyDict_SetItemString(d, "SIGUSR1", x);
Py_XDECREF(x);
#endif
#if defined(SIGUSR2)
x = PyInt_FromLong(SIGUSR2);
PyDict_SetItemString(d, "SIGUSR2", x);
Py_XDECREF(x);
#endif
#if defined(SIGCLD)
x = PyInt_FromLong(SIGCLD);
PyDict_SetItemString(d, "SIGCLD", x);
Py_XDECREF(x);
#endif
#if defined(SIGCHLD)
x = PyInt_FromLong(SIGCHLD);
PyDict_SetItemString(d, "SIGCHLD", x);
Py_XDECREF(x);
#endif
#if defined(SIGPWR)
x = PyInt_FromLong(SIGPWR);
PyDict_SetItemString(d, "SIGPWR", x);
Py_XDECREF(x);
#endif
#if defined(SIGIO)
x = PyInt_FromLong(SIGIO);
PyDict_SetItemString(d, "SIGIO", x);
Py_XDECREF(x);
#endif
#if defined(SIGURG)
x = PyInt_FromLong(SIGURG);
PyDict_SetItemString(d, "SIGURG", x);
Py_XDECREF(x);
#endif
#if defined(SIGWINCH)
x = PyInt_FromLong(SIGWINCH);
PyDict_SetItemString(d, "SIGWINCH", x);
Py_XDECREF(x);
#endif
#if defined(SIGPOLL)
x = PyInt_FromLong(SIGPOLL);
PyDict_SetItemString(d, "SIGPOLL", x);
Py_XDECREF(x);
#endif
#if defined(SIGSTOP)
x = PyInt_FromLong(SIGSTOP);
PyDict_SetItemString(d, "SIGSTOP", x);
Py_XDECREF(x);
#endif
#if defined(SIGTSTP)
x = PyInt_FromLong(SIGTSTP);
PyDict_SetItemString(d, "SIGTSTP", x);
Py_XDECREF(x);
#endif
#if defined(SIGCONT)
x = PyInt_FromLong(SIGCONT);
PyDict_SetItemString(d, "SIGCONT", x);
Py_XDECREF(x);
#endif
#if defined(SIGTTIN)
x = PyInt_FromLong(SIGTTIN);
PyDict_SetItemString(d, "SIGTTIN", x);
Py_XDECREF(x);
#endif
#if defined(SIGTTOU)
x = PyInt_FromLong(SIGTTOU);
PyDict_SetItemString(d, "SIGTTOU", x);
Py_XDECREF(x);
#endif
#if defined(SIGVTALRM)
x = PyInt_FromLong(SIGVTALRM);
PyDict_SetItemString(d, "SIGVTALRM", x);
Py_XDECREF(x);
#endif
#if defined(SIGPROF)
x = PyInt_FromLong(SIGPROF);
PyDict_SetItemString(d, "SIGPROF", x);
Py_XDECREF(x);
#endif
#if defined(SIGXCPU)
x = PyInt_FromLong(SIGXCPU);
PyDict_SetItemString(d, "SIGXCPU", x);
Py_XDECREF(x);
#endif
#if defined(SIGXFSZ)
x = PyInt_FromLong(SIGXFSZ);
PyDict_SetItemString(d, "SIGXFSZ", x);
Py_XDECREF(x);
#endif
#if defined(SIGRTMIN)
x = PyInt_FromLong(SIGRTMIN);
PyDict_SetItemString(d, "SIGRTMIN", x);
Py_XDECREF(x);
#endif
#if defined(SIGRTMAX)
x = PyInt_FromLong(SIGRTMAX);
PyDict_SetItemString(d, "SIGRTMAX", x);
Py_XDECREF(x);
#endif
#if defined(SIGINFO)
x = PyInt_FromLong(SIGINFO);
PyDict_SetItemString(d, "SIGINFO", x);
Py_XDECREF(x);
#endif
#if defined(ITIMER_REAL)
x = PyLong_FromLong(ITIMER_REAL);
PyDict_SetItemString(d, "ITIMER_REAL", x);
Py_DECREF(x);
#endif
#if defined(ITIMER_VIRTUAL)
x = PyLong_FromLong(ITIMER_VIRTUAL);
PyDict_SetItemString(d, "ITIMER_VIRTUAL", x);
Py_DECREF(x);
#endif
#if defined(ITIMER_PROF)
x = PyLong_FromLong(ITIMER_PROF);
PyDict_SetItemString(d, "ITIMER_PROF", x);
Py_DECREF(x);
#endif
#if defined (HAVE_SETITIMER) || defined (HAVE_GETITIMER)
ItimerError = PyErr_NewException("signal.ItimerError",
PyExc_IOError, NULL);
if (ItimerError != NULL)
PyDict_SetItemString(d, "ItimerError", ItimerError);
#endif
if (!PyErr_Occurred())
return;
finally:
return;
}
static void
finisignal(void) {
int i;
PyObject *func;
PyOS_setsig(SIGINT, old_siginthandler);
old_siginthandler = SIG_DFL;
for (i = 1; i < NSIG; i++) {
func = Handlers[i].func;
Handlers[i].tripped = 0;
Handlers[i].func = NULL;
if (i != SIGINT && func != NULL && func != Py_None &&
func != DefaultHandler && func != IgnoreHandler)
PyOS_setsig(i, SIG_DFL);
Py_XDECREF(func);
}
Py_XDECREF(IntHandler);
IntHandler = NULL;
Py_XDECREF(DefaultHandler);
DefaultHandler = NULL;
Py_XDECREF(IgnoreHandler);
IgnoreHandler = NULL;
}
int
PyErr_CheckSignals(void) {
int i;
PyObject *f;
if (!is_tripped)
return 0;
#if defined(WITH_THREAD)
if (PyThread_get_thread_ident() != main_thread)
return 0;
#endif
is_tripped = 0;
if (!(f = (PyObject *)PyEval_GetFrame()))
f = Py_None;
for (i = 1; i < NSIG; i++) {
if (Handlers[i].tripped) {
PyObject *result = NULL;
PyObject *arglist = Py_BuildValue("(iO)", i, f);
Handlers[i].tripped = 0;
if (arglist) {
result = PyEval_CallObject(Handlers[i].func,
arglist);
Py_DECREF(arglist);
}
if (!result)
return -1;
Py_DECREF(result);
}
}
return 0;
}
void
PyErr_SetInterrupt(void) {
is_tripped = 1;
Handlers[SIGINT].tripped = 1;
Py_AddPendingCall((int (*)(void *))PyErr_CheckSignals, NULL);
}
void
PyOS_InitInterrupts(void) {
initsignal();
_PyImport_FixupExtension("signal", "signal");
}
void
PyOS_FiniInterrupts(void) {
finisignal();
}
int
PyOS_InterruptOccurred(void) {
if (Handlers[SIGINT].tripped) {
#if defined(WITH_THREAD)
if (PyThread_get_thread_ident() != main_thread)
return 0;
#endif
Handlers[SIGINT].tripped = 0;
return 1;
}
return 0;
}
void
PyOS_AfterFork(void) {
#if defined(WITH_THREAD)
PyEval_ReInitThreads();
main_thread = PyThread_get_thread_ident();
main_pid = getpid();
_PyImport_ReInitLock();
PyThread_ReInitTLS();
#endif
}