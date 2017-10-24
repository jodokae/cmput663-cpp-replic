#include "Python.h"
#include <syslog.h>
static PyObject *S_ident_o = NULL;
static PyObject *
syslog_openlog(PyObject * self, PyObject * args) {
long logopt = 0;
long facility = LOG_USER;
PyObject *new_S_ident_o;
if (!PyArg_ParseTuple(args,
"S|ll;ident string [, logoption [, facility]]",
&new_S_ident_o, &logopt, &facility))
return NULL;
Py_XDECREF(S_ident_o);
S_ident_o = new_S_ident_o;
Py_INCREF(S_ident_o);
openlog(PyString_AsString(S_ident_o), logopt, facility);
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
syslog_syslog(PyObject * self, PyObject * args) {
char *message;
int priority = LOG_INFO;
if (!PyArg_ParseTuple(args, "is;[priority,] message string",
&priority, &message)) {
PyErr_Clear();
if (!PyArg_ParseTuple(args, "s;[priority,] message string",
&message))
return NULL;
}
Py_BEGIN_ALLOW_THREADS;
syslog(priority, "%s", message);
Py_END_ALLOW_THREADS;
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
syslog_closelog(PyObject *self, PyObject *unused) {
closelog();
Py_XDECREF(S_ident_o);
S_ident_o = NULL;
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
syslog_setlogmask(PyObject *self, PyObject *args) {
long maskpri, omaskpri;
if (!PyArg_ParseTuple(args, "l;mask for priority", &maskpri))
return NULL;
omaskpri = setlogmask(maskpri);
return PyInt_FromLong(omaskpri);
}
static PyObject *
syslog_log_mask(PyObject *self, PyObject *args) {
long mask;
long pri;
if (!PyArg_ParseTuple(args, "l:LOG_MASK", &pri))
return NULL;
mask = LOG_MASK(pri);
return PyInt_FromLong(mask);
}
static PyObject *
syslog_log_upto(PyObject *self, PyObject *args) {
long mask;
long pri;
if (!PyArg_ParseTuple(args, "l:LOG_UPTO", &pri))
return NULL;
mask = LOG_UPTO(pri);
return PyInt_FromLong(mask);
}
static PyMethodDef syslog_methods[] = {
{"openlog", syslog_openlog, METH_VARARGS},
{"closelog", syslog_closelog, METH_NOARGS},
{"syslog", syslog_syslog, METH_VARARGS},
{"setlogmask", syslog_setlogmask, METH_VARARGS},
{"LOG_MASK", syslog_log_mask, METH_VARARGS},
{"LOG_UPTO", syslog_log_upto, METH_VARARGS},
{NULL, NULL, 0}
};
PyMODINIT_FUNC
initsyslog(void) {
PyObject *m;
m = Py_InitModule("syslog", syslog_methods);
if (m == NULL)
return;
PyModule_AddIntConstant(m, "LOG_EMERG", LOG_EMERG);
PyModule_AddIntConstant(m, "LOG_ALERT", LOG_ALERT);
PyModule_AddIntConstant(m, "LOG_CRIT", LOG_CRIT);
PyModule_AddIntConstant(m, "LOG_ERR", LOG_ERR);
PyModule_AddIntConstant(m, "LOG_WARNING", LOG_WARNING);
PyModule_AddIntConstant(m, "LOG_NOTICE", LOG_NOTICE);
PyModule_AddIntConstant(m, "LOG_INFO", LOG_INFO);
PyModule_AddIntConstant(m, "LOG_DEBUG", LOG_DEBUG);
PyModule_AddIntConstant(m, "LOG_PID", LOG_PID);
PyModule_AddIntConstant(m, "LOG_CONS", LOG_CONS);
PyModule_AddIntConstant(m, "LOG_NDELAY", LOG_NDELAY);
#if defined(LOG_NOWAIT)
PyModule_AddIntConstant(m, "LOG_NOWAIT", LOG_NOWAIT);
#endif
#if defined(LOG_PERROR)
PyModule_AddIntConstant(m, "LOG_PERROR", LOG_PERROR);
#endif
PyModule_AddIntConstant(m, "LOG_KERN", LOG_KERN);
PyModule_AddIntConstant(m, "LOG_USER", LOG_USER);
PyModule_AddIntConstant(m, "LOG_MAIL", LOG_MAIL);
PyModule_AddIntConstant(m, "LOG_DAEMON", LOG_DAEMON);
PyModule_AddIntConstant(m, "LOG_AUTH", LOG_AUTH);
PyModule_AddIntConstant(m, "LOG_LPR", LOG_LPR);
PyModule_AddIntConstant(m, "LOG_LOCAL0", LOG_LOCAL0);
PyModule_AddIntConstant(m, "LOG_LOCAL1", LOG_LOCAL1);
PyModule_AddIntConstant(m, "LOG_LOCAL2", LOG_LOCAL2);
PyModule_AddIntConstant(m, "LOG_LOCAL3", LOG_LOCAL3);
PyModule_AddIntConstant(m, "LOG_LOCAL4", LOG_LOCAL4);
PyModule_AddIntConstant(m, "LOG_LOCAL5", LOG_LOCAL5);
PyModule_AddIntConstant(m, "LOG_LOCAL6", LOG_LOCAL6);
PyModule_AddIntConstant(m, "LOG_LOCAL7", LOG_LOCAL7);
#if !defined(LOG_SYSLOG)
#define LOG_SYSLOG LOG_DAEMON
#endif
#if !defined(LOG_NEWS)
#define LOG_NEWS LOG_MAIL
#endif
#if !defined(LOG_UUCP)
#define LOG_UUCP LOG_MAIL
#endif
#if !defined(LOG_CRON)
#define LOG_CRON LOG_DAEMON
#endif
PyModule_AddIntConstant(m, "LOG_SYSLOG", LOG_SYSLOG);
PyModule_AddIntConstant(m, "LOG_CRON", LOG_CRON);
PyModule_AddIntConstant(m, "LOG_UUCP", LOG_UUCP);
PyModule_AddIntConstant(m, "LOG_NEWS", LOG_NEWS);
}