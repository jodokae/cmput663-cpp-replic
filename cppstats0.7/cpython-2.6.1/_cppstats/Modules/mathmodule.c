#include "Python.h"
#include "longintrepr.h"
#if defined(_OSF_SOURCE)
extern double copysign(double, double);
#endif
static int
is_error(double x) {
int result = 1;
assert(errno);
if (errno == EDOM)
PyErr_SetString(PyExc_ValueError, "math domain error");
else if (errno == ERANGE) {
if (fabs(x) < 1.0)
result = 0;
else
PyErr_SetString(PyExc_OverflowError,
"math range error");
} else
PyErr_SetFromErrno(PyExc_ValueError);
return result;
}
static double
m_atan2(double y, double x) {
if (Py_IS_NAN(x) || Py_IS_NAN(y))
return Py_NAN;
if (Py_IS_INFINITY(y)) {
if (Py_IS_INFINITY(x)) {
if (copysign(1., x) == 1.)
return copysign(0.25*Py_MATH_PI, y);
else
return copysign(0.75*Py_MATH_PI, y);
}
return copysign(0.5*Py_MATH_PI, y);
}
if (Py_IS_INFINITY(x) || y == 0.) {
if (copysign(1., x) == 1.)
return copysign(0., y);
else
return copysign(Py_MATH_PI, y);
}
return atan2(y, x);
}
static PyObject *
math_1(PyObject *arg, double (*func) (double), int can_overflow) {
double x, r;
x = PyFloat_AsDouble(arg);
if (x == -1.0 && PyErr_Occurred())
return NULL;
errno = 0;
PyFPE_START_PROTECT("in math_1", return 0);
r = (*func)(x);
PyFPE_END_PROTECT(r);
if (Py_IS_NAN(r)) {
if (!Py_IS_NAN(x))
errno = EDOM;
else
errno = 0;
} else if (Py_IS_INFINITY(r)) {
if (Py_IS_FINITE(x))
errno = can_overflow ? ERANGE : EDOM;
else
errno = 0;
}
if (errno && is_error(r))
return NULL;
else
return PyFloat_FromDouble(r);
}
static PyObject *
math_2(PyObject *args, double (*func) (double, double), char *funcname) {
PyObject *ox, *oy;
double x, y, r;
if (! PyArg_UnpackTuple(args, funcname, 2, 2, &ox, &oy))
return NULL;
x = PyFloat_AsDouble(ox);
y = PyFloat_AsDouble(oy);
if ((x == -1.0 || y == -1.0) && PyErr_Occurred())
return NULL;
errno = 0;
PyFPE_START_PROTECT("in math_2", return 0);
r = (*func)(x, y);
PyFPE_END_PROTECT(r);
if (Py_IS_NAN(r)) {
if (!Py_IS_NAN(x) && !Py_IS_NAN(y))
errno = EDOM;
else
errno = 0;
} else if (Py_IS_INFINITY(r)) {
if (Py_IS_FINITE(x) && Py_IS_FINITE(y))
errno = ERANGE;
else
errno = 0;
}
if (errno && is_error(r))
return NULL;
else
return PyFloat_FromDouble(r);
}
#define FUNC1(funcname, func, can_overflow, docstring) static PyObject * math_##funcname(PyObject *self, PyObject *args) { return math_1(args, func, can_overflow); }PyDoc_STRVAR(math_##funcname##_doc, docstring);
#define FUNC2(funcname, func, docstring) static PyObject * math_##funcname(PyObject *self, PyObject *args) { return math_2(args, func, #funcname); }PyDoc_STRVAR(math_##funcname##_doc, docstring);
FUNC1(acos, acos, 0,
"acos(x)\n\nReturn the arc cosine (measured in radians) of x.")
FUNC1(acosh, acosh, 0,
"acosh(x)\n\nReturn the hyperbolic arc cosine (measured in radians) of x.")
FUNC1(asin, asin, 0,
"asin(x)\n\nReturn the arc sine (measured in radians) of x.")
FUNC1(asinh, asinh, 0,
"asinh(x)\n\nReturn the hyperbolic arc sine (measured in radians) of x.")
FUNC1(atan, atan, 0,
"atan(x)\n\nReturn the arc tangent (measured in radians) of x.")
FUNC2(atan2, m_atan2,
"atan2(y, x)\n\nReturn the arc tangent (measured in radians) of y/x.\n"
"Unlike atan(y/x), the signs of both x and y are considered.")
FUNC1(atanh, atanh, 0,
"atanh(x)\n\nReturn the hyperbolic arc tangent (measured in radians) of x.")
FUNC1(ceil, ceil, 0,
"ceil(x)\n\nReturn the ceiling of x as a float.\n"
"This is the smallest integral value >= x.")
FUNC2(copysign, copysign,
"copysign(x,y)\n\nReturn x with the sign of y.")
FUNC1(cos, cos, 0,
"cos(x)\n\nReturn the cosine of x (measured in radians).")
FUNC1(cosh, cosh, 1,
"cosh(x)\n\nReturn the hyperbolic cosine of x.")
FUNC1(exp, exp, 1,
"exp(x)\n\nReturn e raised to the power of x.")
FUNC1(fabs, fabs, 0,
"fabs(x)\n\nReturn the absolute value of the float x.")
FUNC1(floor, floor, 0,
"floor(x)\n\nReturn the floor of x as a float.\n"
"This is the largest integral value <= x.")
FUNC1(log1p, log1p, 1,
"log1p(x)\n\nReturn the natural logarithm of 1+x (base e).\n\
The result is computed in a way which is accurate for x near zero.")
FUNC1(sin, sin, 0,
"sin(x)\n\nReturn the sine of x (measured in radians).")
FUNC1(sinh, sinh, 1,
"sinh(x)\n\nReturn the hyperbolic sine of x.")
FUNC1(sqrt, sqrt, 0,
"sqrt(x)\n\nReturn the square root of x.")
FUNC1(tan, tan, 0,
"tan(x)\n\nReturn the tangent of x (measured in radians).")
FUNC1(tanh, tanh, 0,
"tanh(x)\n\nReturn the hyperbolic tangent of x.")
#define NUM_PARTIALS 32
static int
_fsum_realloc(double **p_ptr, Py_ssize_t n,
double *ps, Py_ssize_t *m_ptr) {
void *v = NULL;
Py_ssize_t m = *m_ptr;
m += m;
if (n < m && m < (PY_SSIZE_T_MAX / sizeof(double))) {
double *p = *p_ptr;
if (p == ps) {
v = PyMem_Malloc(sizeof(double) * m);
if (v != NULL)
memcpy(v, ps, sizeof(double) * n);
} else
v = PyMem_Realloc(p, sizeof(double) * m);
}
if (v == NULL) {
PyErr_SetString(PyExc_MemoryError, "math.fsum partials");
return 1;
}
*p_ptr = (double*) v;
*m_ptr = m;
return 0;
}
static PyObject*
math_fsum(PyObject *self, PyObject *seq) {
PyObject *item, *iter, *sum = NULL;
Py_ssize_t i, j, n = 0, m = NUM_PARTIALS;
double x, y, t, ps[NUM_PARTIALS], *p = ps;
double xsave, special_sum = 0.0, inf_sum = 0.0;
volatile double hi, yr, lo;
iter = PyObject_GetIter(seq);
if (iter == NULL)
return NULL;
PyFPE_START_PROTECT("fsum", Py_DECREF(iter); return NULL)
for(;;) {
assert(0 <= n && n <= m);
assert((m == NUM_PARTIALS && p == ps) ||
(m > NUM_PARTIALS && p != NULL));
item = PyIter_Next(iter);
if (item == NULL) {
if (PyErr_Occurred())
goto _fsum_error;
break;
}
x = PyFloat_AsDouble(item);
Py_DECREF(item);
if (PyErr_Occurred())
goto _fsum_error;
xsave = x;
for (i = j = 0; j < n; j++) {
y = p[j];
if (fabs(x) < fabs(y)) {
t = x;
x = y;
y = t;
}
hi = x + y;
yr = hi - x;
lo = y - yr;
if (lo != 0.0)
p[i++] = lo;
x = hi;
}
n = i;
if (x != 0.0) {
if (! Py_IS_FINITE(x)) {
if (Py_IS_FINITE(xsave)) {
PyErr_SetString(PyExc_OverflowError,
"intermediate overflow in fsum");
goto _fsum_error;
}
if (Py_IS_INFINITY(xsave))
inf_sum += xsave;
special_sum += xsave;
n = 0;
} else if (n >= m && _fsum_realloc(&p, n, ps, &m))
goto _fsum_error;
else
p[n++] = x;
}
}
if (special_sum != 0.0) {
if (Py_IS_NAN(inf_sum))
PyErr_SetString(PyExc_ValueError,
"-inf + inf in fsum");
else
sum = PyFloat_FromDouble(special_sum);
goto _fsum_error;
}
hi = 0.0;
if (n > 0) {
hi = p[--n];
while (n > 0) {
x = hi;
y = p[--n];
assert(fabs(y) < fabs(x));
hi = x + y;
yr = hi - x;
lo = y - yr;
if (lo != 0.0)
break;
}
if (n > 0 && ((lo < 0.0 && p[n-1] < 0.0) ||
(lo > 0.0 && p[n-1] > 0.0))) {
y = lo * 2.0;
x = hi + y;
yr = x - hi;
if (y == yr)
hi = x;
}
}
sum = PyFloat_FromDouble(hi);
_fsum_error:
PyFPE_END_PROTECT(hi)
Py_DECREF(iter);
if (p != ps)
PyMem_Free(p);
return sum;
}
#undef NUM_PARTIALS
PyDoc_STRVAR(math_fsum_doc,
"sum(iterable)\n\n\
Return an accurate floating point sum of values in the iterable.\n\
Assumes IEEE-754 floating point arithmetic.");
static PyObject *
math_factorial(PyObject *self, PyObject *arg) {
long i, x;
PyObject *result, *iobj, *newresult;
if (PyFloat_Check(arg)) {
double dx = PyFloat_AS_DOUBLE((PyFloatObject *)arg);
if (dx != floor(dx)) {
PyErr_SetString(PyExc_ValueError,
"factorial() only accepts integral values");
return NULL;
}
}
x = PyInt_AsLong(arg);
if (x == -1 && PyErr_Occurred())
return NULL;
if (x < 0) {
PyErr_SetString(PyExc_ValueError,
"factorial() not defined for negative values");
return NULL;
}
result = (PyObject *)PyInt_FromLong(1);
if (result == NULL)
return NULL;
for (i=1 ; i<=x ; i++) {
iobj = (PyObject *)PyInt_FromLong(i);
if (iobj == NULL)
goto error;
newresult = PyNumber_Multiply(result, iobj);
Py_DECREF(iobj);
if (newresult == NULL)
goto error;
Py_DECREF(result);
result = newresult;
}
return result;
error:
Py_DECREF(result);
return NULL;
}
PyDoc_STRVAR(math_factorial_doc, "Return n!");
static PyObject *
math_trunc(PyObject *self, PyObject *number) {
return PyObject_CallMethod(number, "__trunc__", NULL);
}
PyDoc_STRVAR(math_trunc_doc,
"trunc(x:Real) -> Integral\n"
"\n"
"Truncates x to the nearest Integral toward 0. Uses the __trunc__ magic method.");
static PyObject *
math_frexp(PyObject *self, PyObject *arg) {
int i;
double x = PyFloat_AsDouble(arg);
if (x == -1.0 && PyErr_Occurred())
return NULL;
if (Py_IS_NAN(x) || Py_IS_INFINITY(x) || !x) {
i = 0;
} else {
PyFPE_START_PROTECT("in math_frexp", return 0);
x = frexp(x, &i);
PyFPE_END_PROTECT(x);
}
return Py_BuildValue("(di)", x, i);
}
PyDoc_STRVAR(math_frexp_doc,
"frexp(x)\n"
"\n"
"Return the mantissa and exponent of x, as pair (m, e).\n"
"m is a float and e is an int, such that x = m * 2.**e.\n"
"If x is 0, m and e are both 0. Else 0.5 <= abs(m) < 1.0.");
static PyObject *
math_ldexp(PyObject *self, PyObject *args) {
double x, r;
PyObject *oexp;
long exp;
if (! PyArg_ParseTuple(args, "dO:ldexp", &x, &oexp))
return NULL;
if (PyLong_Check(oexp)) {
exp = PyLong_AsLong(oexp);
if (exp == -1 && PyErr_Occurred()) {
if (PyErr_ExceptionMatches(PyExc_OverflowError)) {
if (Py_SIZE(oexp) < 0) {
exp = LONG_MIN;
} else {
exp = LONG_MAX;
}
PyErr_Clear();
} else {
return NULL;
}
}
} else if (PyInt_Check(oexp)) {
exp = PyInt_AS_LONG(oexp);
} else {
PyErr_SetString(PyExc_TypeError,
"Expected an int or long as second argument "
"to ldexp.");
return NULL;
}
if (x == 0. || !Py_IS_FINITE(x)) {
r = x;
errno = 0;
} else if (exp > INT_MAX) {
r = copysign(Py_HUGE_VAL, x);
errno = ERANGE;
} else if (exp < INT_MIN) {
r = copysign(0., x);
errno = 0;
} else {
errno = 0;
PyFPE_START_PROTECT("in math_ldexp", return 0);
r = ldexp(x, (int)exp);
PyFPE_END_PROTECT(r);
if (Py_IS_INFINITY(r))
errno = ERANGE;
}
if (errno && is_error(r))
return NULL;
return PyFloat_FromDouble(r);
}
PyDoc_STRVAR(math_ldexp_doc,
"ldexp(x, i) -> x * (2**i)");
static PyObject *
math_modf(PyObject *self, PyObject *arg) {
double y, x = PyFloat_AsDouble(arg);
if (x == -1.0 && PyErr_Occurred())
return NULL;
if (!Py_IS_FINITE(x)) {
if (Py_IS_INFINITY(x))
return Py_BuildValue("(dd)", copysign(0., x), x);
else if (Py_IS_NAN(x))
return Py_BuildValue("(dd)", x, x);
}
errno = 0;
PyFPE_START_PROTECT("in math_modf", return 0);
x = modf(x, &y);
PyFPE_END_PROTECT(x);
return Py_BuildValue("(dd)", x, y);
}
PyDoc_STRVAR(math_modf_doc,
"modf(x)\n"
"\n"
"Return the fractional and integer parts of x. Both results carry the sign\n"
"of x. The integer part is returned as a real.");
static PyObject*
loghelper(PyObject* arg, double (*func)(double), char *funcname) {
if (PyLong_Check(arg)) {
double x;
int e;
x = _PyLong_AsScaledDouble(arg, &e);
if (x <= 0.0) {
PyErr_SetString(PyExc_ValueError,
"math domain error");
return NULL;
}
x = func(x) + (e * (double)PyLong_SHIFT) * func(2.0);
return PyFloat_FromDouble(x);
}
return math_1(arg, func, 0);
}
static PyObject *
math_log(PyObject *self, PyObject *args) {
PyObject *arg;
PyObject *base = NULL;
PyObject *num, *den;
PyObject *ans;
if (!PyArg_UnpackTuple(args, "log", 1, 2, &arg, &base))
return NULL;
num = loghelper(arg, log, "log");
if (num == NULL || base == NULL)
return num;
den = loghelper(base, log, "log");
if (den == NULL) {
Py_DECREF(num);
return NULL;
}
ans = PyNumber_Divide(num, den);
Py_DECREF(num);
Py_DECREF(den);
return ans;
}
PyDoc_STRVAR(math_log_doc,
"log(x[, base]) -> the logarithm of x to the given base.\n\
If the base not specified, returns the natural logarithm (base e) of x.");
static PyObject *
math_log10(PyObject *self, PyObject *arg) {
return loghelper(arg, log10, "log10");
}
PyDoc_STRVAR(math_log10_doc,
"log10(x) -> the base 10 logarithm of x.");
static PyObject *
math_fmod(PyObject *self, PyObject *args) {
PyObject *ox, *oy;
double r, x, y;
if (! PyArg_UnpackTuple(args, "fmod", 2, 2, &ox, &oy))
return NULL;
x = PyFloat_AsDouble(ox);
y = PyFloat_AsDouble(oy);
if ((x == -1.0 || y == -1.0) && PyErr_Occurred())
return NULL;
if (Py_IS_INFINITY(y) && Py_IS_FINITE(x))
return PyFloat_FromDouble(x);
errno = 0;
PyFPE_START_PROTECT("in math_fmod", return 0);
r = fmod(x, y);
PyFPE_END_PROTECT(r);
if (Py_IS_NAN(r)) {
if (!Py_IS_NAN(x) && !Py_IS_NAN(y))
errno = EDOM;
else
errno = 0;
}
if (errno && is_error(r))
return NULL;
else
return PyFloat_FromDouble(r);
}
PyDoc_STRVAR(math_fmod_doc,
"fmod(x,y)\n\nReturn fmod(x, y), according to platform C."
" x % y may differ.");
static PyObject *
math_hypot(PyObject *self, PyObject *args) {
PyObject *ox, *oy;
double r, x, y;
if (! PyArg_UnpackTuple(args, "hypot", 2, 2, &ox, &oy))
return NULL;
x = PyFloat_AsDouble(ox);
y = PyFloat_AsDouble(oy);
if ((x == -1.0 || y == -1.0) && PyErr_Occurred())
return NULL;
if (Py_IS_INFINITY(x))
return PyFloat_FromDouble(fabs(x));
if (Py_IS_INFINITY(y))
return PyFloat_FromDouble(fabs(y));
errno = 0;
PyFPE_START_PROTECT("in math_hypot", return 0);
r = hypot(x, y);
PyFPE_END_PROTECT(r);
if (Py_IS_NAN(r)) {
if (!Py_IS_NAN(x) && !Py_IS_NAN(y))
errno = EDOM;
else
errno = 0;
} else if (Py_IS_INFINITY(r)) {
if (Py_IS_FINITE(x) && Py_IS_FINITE(y))
errno = ERANGE;
else
errno = 0;
}
if (errno && is_error(r))
return NULL;
else
return PyFloat_FromDouble(r);
}
PyDoc_STRVAR(math_hypot_doc,
"hypot(x,y)\n\nReturn the Euclidean distance, sqrt(x*x + y*y).");
static PyObject *
math_pow(PyObject *self, PyObject *args) {
PyObject *ox, *oy;
double r, x, y;
int odd_y;
if (! PyArg_UnpackTuple(args, "pow", 2, 2, &ox, &oy))
return NULL;
x = PyFloat_AsDouble(ox);
y = PyFloat_AsDouble(oy);
if ((x == -1.0 || y == -1.0) && PyErr_Occurred())
return NULL;
r = 0.;
if (!Py_IS_FINITE(x) || !Py_IS_FINITE(y)) {
errno = 0;
if (Py_IS_NAN(x))
r = y == 0. ? 1. : x;
else if (Py_IS_NAN(y))
r = x == 1. ? 1. : y;
else if (Py_IS_INFINITY(x)) {
odd_y = Py_IS_FINITE(y) && fmod(fabs(y), 2.0) == 1.0;
if (y > 0.)
r = odd_y ? x : fabs(x);
else if (y == 0.)
r = 1.;
else
r = odd_y ? copysign(0., x) : 0.;
} else if (Py_IS_INFINITY(y)) {
if (fabs(x) == 1.0)
r = 1.;
else if (y > 0. && fabs(x) > 1.0)
r = y;
else if (y < 0. && fabs(x) < 1.0) {
r = -y;
if (x == 0.)
errno = EDOM;
} else
r = 0.;
}
} else {
errno = 0;
PyFPE_START_PROTECT("in math_pow", return 0);
r = pow(x, y);
PyFPE_END_PROTECT(r);
if (!Py_IS_FINITE(r)) {
if (Py_IS_NAN(r)) {
errno = EDOM;
}
else if (Py_IS_INFINITY(r)) {
if (x == 0.)
errno = EDOM;
else
errno = ERANGE;
}
}
}
if (errno && is_error(r))
return NULL;
else
return PyFloat_FromDouble(r);
}
PyDoc_STRVAR(math_pow_doc,
"pow(x,y)\n\nReturn x**y (x to the power of y).");
static const double degToRad = Py_MATH_PI / 180.0;
static const double radToDeg = 180.0 / Py_MATH_PI;
static PyObject *
math_degrees(PyObject *self, PyObject *arg) {
double x = PyFloat_AsDouble(arg);
if (x == -1.0 && PyErr_Occurred())
return NULL;
return PyFloat_FromDouble(x * radToDeg);
}
PyDoc_STRVAR(math_degrees_doc,
"degrees(x) -> converts angle x from radians to degrees");
static PyObject *
math_radians(PyObject *self, PyObject *arg) {
double x = PyFloat_AsDouble(arg);
if (x == -1.0 && PyErr_Occurred())
return NULL;
return PyFloat_FromDouble(x * degToRad);
}
PyDoc_STRVAR(math_radians_doc,
"radians(x) -> converts angle x from degrees to radians");
static PyObject *
math_isnan(PyObject *self, PyObject *arg) {
double x = PyFloat_AsDouble(arg);
if (x == -1.0 && PyErr_Occurred())
return NULL;
return PyBool_FromLong((long)Py_IS_NAN(x));
}
PyDoc_STRVAR(math_isnan_doc,
"isnan(x) -> bool\n\
Checks if float x is not a number (NaN)");
static PyObject *
math_isinf(PyObject *self, PyObject *arg) {
double x = PyFloat_AsDouble(arg);
if (x == -1.0 && PyErr_Occurred())
return NULL;
return PyBool_FromLong((long)Py_IS_INFINITY(x));
}
PyDoc_STRVAR(math_isinf_doc,
"isinf(x) -> bool\n\
Checks if float x is infinite (positive or negative)");
static PyMethodDef math_methods[] = {
{"acos", math_acos, METH_O, math_acos_doc},
{"acosh", math_acosh, METH_O, math_acosh_doc},
{"asin", math_asin, METH_O, math_asin_doc},
{"asinh", math_asinh, METH_O, math_asinh_doc},
{"atan", math_atan, METH_O, math_atan_doc},
{"atan2", math_atan2, METH_VARARGS, math_atan2_doc},
{"atanh", math_atanh, METH_O, math_atanh_doc},
{"ceil", math_ceil, METH_O, math_ceil_doc},
{"copysign", math_copysign, METH_VARARGS, math_copysign_doc},
{"cos", math_cos, METH_O, math_cos_doc},
{"cosh", math_cosh, METH_O, math_cosh_doc},
{"degrees", math_degrees, METH_O, math_degrees_doc},
{"exp", math_exp, METH_O, math_exp_doc},
{"fabs", math_fabs, METH_O, math_fabs_doc},
{"factorial", math_factorial, METH_O, math_factorial_doc},
{"floor", math_floor, METH_O, math_floor_doc},
{"fmod", math_fmod, METH_VARARGS, math_fmod_doc},
{"frexp", math_frexp, METH_O, math_frexp_doc},
{"fsum", math_fsum, METH_O, math_fsum_doc},
{"hypot", math_hypot, METH_VARARGS, math_hypot_doc},
{"isinf", math_isinf, METH_O, math_isinf_doc},
{"isnan", math_isnan, METH_O, math_isnan_doc},
{"ldexp", math_ldexp, METH_VARARGS, math_ldexp_doc},
{"log", math_log, METH_VARARGS, math_log_doc},
{"log1p", math_log1p, METH_O, math_log1p_doc},
{"log10", math_log10, METH_O, math_log10_doc},
{"modf", math_modf, METH_O, math_modf_doc},
{"pow", math_pow, METH_VARARGS, math_pow_doc},
{"radians", math_radians, METH_O, math_radians_doc},
{"sin", math_sin, METH_O, math_sin_doc},
{"sinh", math_sinh, METH_O, math_sinh_doc},
{"sqrt", math_sqrt, METH_O, math_sqrt_doc},
{"tan", math_tan, METH_O, math_tan_doc},
{"tanh", math_tanh, METH_O, math_tanh_doc},
{"trunc", math_trunc, METH_O, math_trunc_doc},
{NULL, NULL}
};
PyDoc_STRVAR(module_doc,
"This module is always available. It provides access to the\n"
"mathematical functions defined by the C standard.");
PyMODINIT_FUNC
initmath(void) {
PyObject *m;
m = Py_InitModule3("math", math_methods, module_doc);
if (m == NULL)
goto finally;
PyModule_AddObject(m, "pi", PyFloat_FromDouble(Py_MATH_PI));
PyModule_AddObject(m, "e", PyFloat_FromDouble(Py_MATH_E));
finally:
return;
}
