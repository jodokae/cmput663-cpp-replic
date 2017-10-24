#include "Python.h"
static PyObject *fpe_error;
PyMODINIT_FUNC initfpetest(void);
static PyObject *test(PyObject *self,PyObject *args);
static double db0(double);
static double overflow(double);
static double nest1(int, double);
static double nest2(int, double);
static double nest3(double);
static void printerr(double);
static PyMethodDef fpetest_methods[] = {
{"test", (PyCFunction) test, METH_VARARGS},
{0,0}
};
static PyObject *test(PyObject *self,PyObject *args) {
double r;
fprintf(stderr,"overflow");
r = overflow(1.e160);
printerr(r);
fprintf(stderr,"\ndiv by 0");
r = db0(0.0);
printerr(r);
fprintf(stderr,"\nnested outer");
r = nest1(0, 0.0);
printerr(r);
fprintf(stderr,"\nnested inner");
r = nest1(1, 1.0);
printerr(r);
fprintf(stderr,"\ntrailing outer");
r = nest1(2, 2.0);
printerr(r);
fprintf(stderr,"\nnested prior");
r = nest2(0, 0.0);
printerr(r);
fprintf(stderr,"\nnested interior");
r = nest2(1, 1.0);
printerr(r);
fprintf(stderr,"\nnested trailing");
r = nest2(2, 2.0);
printerr(r);
Py_INCREF (Py_None);
return Py_None;
}
static void printerr(double r) {
if(r == 3.1416) {
fprintf(stderr,"\tPASS\n");
PyErr_Print();
} else {
fprintf(stderr,"\tFAIL\n");
}
PyErr_Clear();
}
static double nest1(int i, double x) {
double a = 1.0;
PyFPE_START_PROTECT("Division by zero, outer zone", return 3.1416)
if(i == 0) {
a = 1./x;
} else if(i == 1) {
PyFPE_START_PROTECT("Division by zero, inner zone", return 3.1416)
a = 1./(1. - x);
PyFPE_END_PROTECT(a)
} else if(i == 2) {
a = 1./(2. - x);
}
PyFPE_END_PROTECT(a)
return a;
}
static double nest2(int i, double x) {
double a = 1.0;
PyFPE_START_PROTECT("Division by zero, prior error", return 3.1416)
if(i == 0) {
a = 1./x;
} else if(i == 1) {
a = nest3(x);
} else if(i == 2) {
a = 1./(2. - x);
}
PyFPE_END_PROTECT(a)
return a;
}
static double nest3(double x) {
double result;
PyFPE_START_PROTECT("Division by zero, nest3 error", return 3.1416)
result = 1./(1. - x);
PyFPE_END_PROTECT(result)
return result;
}
static double db0(double x) {
double a;
PyFPE_START_PROTECT("Division by zero", return 3.1416)
a = 1./x;
PyFPE_END_PROTECT(a)
return a;
}
static double overflow(double b) {
double a;
PyFPE_START_PROTECT("Overflow", return 3.1416)
a = b*b;
PyFPE_END_PROTECT(a)
return a;
}
PyMODINIT_FUNC initfpetest(void) {
PyObject *m, *d;
m = Py_InitModule("fpetest", fpetest_methods);
if (m == NULL)
return;
d = PyModule_GetDict(m);
fpe_error = PyErr_NewException("fpetest.error", NULL, NULL);
if (fpe_error != NULL)
PyDict_SetItemString(d, "error", fpe_error);
}
