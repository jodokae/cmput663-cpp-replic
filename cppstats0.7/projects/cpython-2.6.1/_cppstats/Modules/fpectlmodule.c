#if defined(__cplusplus)
extern "C" {
#endif
#include "Python.h"
#include <signal.h>
#if defined(__FreeBSD__)
#include <ieeefp.h>
#elif defined(__VMS)
#define __NEW_STARLET
#include <starlet.h>
#include <ieeedef.h>
#endif
#if !defined(WANT_SIGFPE_HANDLER)
#include <setjmp.h>
static jmp_buf PyFPE_jbuf;
static int PyFPE_counter = 0;
#endif
typedef void Sigfunc(int);
static Sigfunc sigfpe_handler;
static void fpe_reset(Sigfunc *);
static PyObject *fpe_error;
PyMODINIT_FUNC initfpectl(void);
static PyObject *turnon_sigfpe (PyObject *self,PyObject *args);
static PyObject *turnoff_sigfpe (PyObject *self,PyObject *args);
static PyMethodDef fpectl_methods[] = {
{"turnon_sigfpe", (PyCFunction) turnon_sigfpe, METH_VARARGS},
{"turnoff_sigfpe", (PyCFunction) turnoff_sigfpe, METH_VARARGS},
{0,0}
};
static PyObject *turnon_sigfpe(PyObject *self,PyObject *args) {
fpe_reset(sigfpe_handler);
Py_INCREF (Py_None);
return Py_None;
}
static void fpe_reset(Sigfunc *handler) {
#if defined(sgi)
#include <sigfpe.h>
typedef void user_routine (unsigned[5], int[2]);
typedef void abort_routine (unsigned long);
handle_sigfpes(_OFF, 0,
(user_routine *)0,
_TURN_OFF_HANDLER_ON_ERROR,
NULL);
handle_sigfpes(_ON, _EN_OVERFL | _EN_DIVZERO | _EN_INVALID,
(user_routine *)0,
_ABORT_ON_ERROR,
NULL);
PyOS_setsig(SIGFPE, handler);
#elif defined(sun)
#include <math.h>
#if !defined(_SUNMATH_H)
extern void nonstandard_arithmetic(void);
extern int ieee_flags(const char*, const char*, const char*, char **);
extern long ieee_handler(const char*, const char*, sigfpe_handler_type);
#endif
char *mode="exception", *in="all", *out;
(void) nonstandard_arithmetic();
(void) ieee_flags("clearall",mode,in,&out);
(void) ieee_handler("set","common",(sigfpe_handler_type)handler);
PyOS_setsig(SIGFPE, handler);
#elif defined(__hppa) || defined(hppa)
#include <math.h>
fpsetdefaults();
PyOS_setsig(SIGFPE, handler);
#elif defined(__AIX) || defined(_AIX)
#include <fptrap.h>
fp_trap(FP_TRAP_SYNC);
fp_enable(TRP_INVALID | TRP_DIV_BY_ZERO | TRP_OVERFLOW);
PyOS_setsig(SIGFPE, handler);
#elif defined(__alpha) && defined(__osf__)
#include <machine/fpu.h>
unsigned long fp_control =
IEEE_TRAP_ENABLE_INV | IEEE_TRAP_ENABLE_DZE | IEEE_TRAP_ENABLE_OVF;
ieee_set_fp_control(fp_control);
PyOS_setsig(SIGFPE, handler);
#elif defined(__alpha) && defined(linux)
#include <asm/fpu.h>
unsigned long fp_control =
IEEE_TRAP_ENABLE_INV | IEEE_TRAP_ENABLE_DZE | IEEE_TRAP_ENABLE_OVF;
ieee_set_fp_control(fp_control);
PyOS_setsig(SIGFPE, handler);
#elif defined(__ALPHA) && defined(__VMS)
IEEE clrmsk;
IEEE setmsk;
clrmsk.ieee$q_flags =
IEEE$M_TRAP_ENABLE_UNF | IEEE$M_TRAP_ENABLE_INE |
IEEE$M_MAP_UMZ;
setmsk.ieee$q_flags =
IEEE$M_TRAP_ENABLE_INV | IEEE$M_TRAP_ENABLE_DZE |
IEEE$M_TRAP_ENABLE_OVF;
sys$ieee_set_fp_control(&clrmsk, &setmsk, 0);
PyOS_setsig(SIGFPE, handler);
#elif defined(__ia64) && defined(__VMS)
PyOS_setsig(SIGFPE, handler);
#elif defined(cray)
#if defined(HAS_LIBMSET)
libmset(-1);
#endif
PyOS_setsig(SIGFPE, handler);
#elif defined(__FreeBSD__)
fpresetsticky(fpgetsticky());
fpsetmask(FP_X_INV | FP_X_DZ | FP_X_OFL);
PyOS_setsig(SIGFPE, handler);
#elif defined(linux)
#if defined(__GLIBC__)
#include <fpu_control.h>
#else
#include <i386/fpu_control.h>
#endif
#if defined(_FPU_SETCW)
{
fpu_control_t cw = 0x1372;
_FPU_SETCW(cw);
}
#else
__setfpucw(0x1372);
#endif
PyOS_setsig(SIGFPE, handler);
#elif defined(_MSC_VER)
#include <float.h>
unsigned int cw = _EM_INVALID | _EM_ZERODIVIDE | _EM_OVERFLOW;
(void)_controlfp(0, cw);
PyOS_setsig(SIGFPE, handler);
#else
fputs("Operation not implemented\n", stderr);
#endif
}
static PyObject *turnoff_sigfpe(PyObject *self,PyObject *args) {
#if defined(__FreeBSD__)
fpresetsticky(fpgetsticky());
fpsetmask(0);
#elif defined(__VMS)
IEEE clrmsk;
clrmsk.ieee$q_flags =
IEEE$M_TRAP_ENABLE_UNF | IEEE$M_TRAP_ENABLE_INE |
IEEE$M_MAP_UMZ | IEEE$M_TRAP_ENABLE_INV |
IEEE$M_TRAP_ENABLE_DZE | IEEE$M_TRAP_ENABLE_OVF |
IEEE$M_INHERIT;
sys$ieee_set_fp_control(&clrmsk, 0, 0);
#else
fputs("Operation not implemented\n", stderr);
#endif
Py_INCREF(Py_None);
return Py_None;
}
static void sigfpe_handler(int signo) {
fpe_reset(sigfpe_handler);
if(PyFPE_counter) {
longjmp(PyFPE_jbuf, 1);
} else {
Py_FatalError("Unprotected floating point exception");
}
}
PyMODINIT_FUNC initfpectl(void) {
PyObject *m, *d;
m = Py_InitModule("fpectl", fpectl_methods);
if (m == NULL)
return;
d = PyModule_GetDict(m);
fpe_error = PyErr_NewException("fpectl.error", NULL, NULL);
if (fpe_error != NULL)
PyDict_SetItemString(d, "error", fpe_error);
}
#if defined(__cplusplus)
}
#endif
