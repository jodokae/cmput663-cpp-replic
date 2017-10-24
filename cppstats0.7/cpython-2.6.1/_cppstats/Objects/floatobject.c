#include "Python.h"
#include "structseq.h"
#include <ctype.h>
#include <float.h>
#undef MAX
#undef MIN
#define MAX(x, y) ((x) < (y) ? (y) : (x))
#define MIN(x, y) ((x) < (y) ? (x) : (y))
#if defined(HAVE_IEEEFP_H)
#include <ieeefp.h>
#endif
#if defined(_OSF_SOURCE)
extern int finite(double);
#endif
#define BLOCK_SIZE 1000
#define BHEAD_SIZE 8
#define N_FLOATOBJECTS ((BLOCK_SIZE - BHEAD_SIZE) / sizeof(PyFloatObject))
struct _floatblock {
struct _floatblock *next;
PyFloatObject objects[N_FLOATOBJECTS];
};
typedef struct _floatblock PyFloatBlock;
static PyFloatBlock *block_list = NULL;
static PyFloatObject *free_list = NULL;
static PyFloatObject *
fill_free_list(void) {
PyFloatObject *p, *q;
p = (PyFloatObject *) PyMem_MALLOC(sizeof(PyFloatBlock));
if (p == NULL)
return (PyFloatObject *) PyErr_NoMemory();
((PyFloatBlock *)p)->next = block_list;
block_list = (PyFloatBlock *)p;
p = &((PyFloatBlock *)p)->objects[0];
q = p + N_FLOATOBJECTS;
while (--q > p)
Py_TYPE(q) = (struct _typeobject *)(q-1);
Py_TYPE(q) = NULL;
return p + N_FLOATOBJECTS - 1;
}
double
PyFloat_GetMax(void) {
return DBL_MAX;
}
double
PyFloat_GetMin(void) {
return DBL_MIN;
}
static PyTypeObject FloatInfoType = {0, 0, 0, 0, 0, 0};
PyDoc_STRVAR(floatinfo__doc__,
"sys.floatinfo\n\
\n\
A structseq holding information about the float type. It contains low level\n\
information about the precision and internal representation. Please study\n\
your system's :file:`float.h` for more information.");
static PyStructSequence_Field floatinfo_fields[] = {
{"max", "DBL_MAX -- maximum representable finite float"},
{
"max_exp", "DBL_MAX_EXP -- maximum int e such that radix**(e-1) "
"is representable"
},
{
"max_10_exp", "DBL_MAX_10_EXP -- maximum int e such that 10**e "
"is representable"
},
{"min", "DBL_MIN -- Minimum positive normalizer float"},
{
"min_exp", "DBL_MIN_EXP -- minimum int e such that radix**(e-1) "
"is a normalized float"
},
{
"min_10_exp", "DBL_MIN_10_EXP -- minimum int e such that 10**e is "
"a normalized"
},
{"dig", "DBL_DIG -- digits"},
{"mant_dig", "DBL_MANT_DIG -- mantissa digits"},
{
"epsilon", "DBL_EPSILON -- Difference between 1 and the next "
"representable float"
},
{"radix", "FLT_RADIX -- radix of exponent"},
{"rounds", "FLT_ROUNDS -- addition rounds"},
{0}
};
static PyStructSequence_Desc floatinfo_desc = {
"sys.floatinfo",
floatinfo__doc__,
floatinfo_fields,
11
};
PyObject *
PyFloat_GetInfo(void) {
PyObject* floatinfo;
int pos = 0;
floatinfo = PyStructSequence_New(&FloatInfoType);
if (floatinfo == NULL) {
return NULL;
}
#define SetIntFlag(flag) PyStructSequence_SET_ITEM(floatinfo, pos++, PyInt_FromLong(flag))
#define SetDblFlag(flag) PyStructSequence_SET_ITEM(floatinfo, pos++, PyFloat_FromDouble(flag))
SetDblFlag(DBL_MAX);
SetIntFlag(DBL_MAX_EXP);
SetIntFlag(DBL_MAX_10_EXP);
SetDblFlag(DBL_MIN);
SetIntFlag(DBL_MIN_EXP);
SetIntFlag(DBL_MIN_10_EXP);
SetIntFlag(DBL_DIG);
SetIntFlag(DBL_MANT_DIG);
SetDblFlag(DBL_EPSILON);
SetIntFlag(FLT_RADIX);
SetIntFlag(FLT_ROUNDS);
#undef SetIntFlag
#undef SetDblFlag
if (PyErr_Occurred()) {
Py_CLEAR(floatinfo);
return NULL;
}
return floatinfo;
}
PyObject *
PyFloat_FromDouble(double fval) {
register PyFloatObject *op;
if (free_list == NULL) {
if ((free_list = fill_free_list()) == NULL)
return NULL;
}
op = free_list;
free_list = (PyFloatObject *)Py_TYPE(op);
PyObject_INIT(op, &PyFloat_Type);
op->ob_fval = fval;
return (PyObject *) op;
}
PyObject *
PyFloat_FromString(PyObject *v, char **pend) {
const char *s, *last, *end, *sp;
double x;
char buffer[256];
#if defined(Py_USING_UNICODE)
char s_buffer[256];
#endif
Py_ssize_t len;
if (pend)
*pend = NULL;
if (PyString_Check(v)) {
s = PyString_AS_STRING(v);
len = PyString_GET_SIZE(v);
}
#if defined(Py_USING_UNICODE)
else if (PyUnicode_Check(v)) {
if (PyUnicode_GET_SIZE(v) >= (Py_ssize_t)sizeof(s_buffer)) {
PyErr_SetString(PyExc_ValueError,
"Unicode float() literal too long to convert");
return NULL;
}
if (PyUnicode_EncodeDecimal(PyUnicode_AS_UNICODE(v),
PyUnicode_GET_SIZE(v),
s_buffer,
NULL))
return NULL;
s = s_buffer;
len = strlen(s);
}
#endif
else if (PyObject_AsCharBuffer(v, &s, &len)) {
PyErr_SetString(PyExc_TypeError,
"float() argument must be a string or a number");
return NULL;
}
last = s + len;
while (*s && isspace(Py_CHARMASK(*s)))
s++;
if (*s == '\0') {
PyErr_SetString(PyExc_ValueError, "empty string for float()");
return NULL;
}
sp = s;
PyFPE_START_PROTECT("strtod", return NULL)
x = PyOS_ascii_strtod(s, (char **)&end);
PyFPE_END_PROTECT(x)
errno = 0;
if (end > last)
end = last;
if (end == s) {
char *p = (char*)sp;
int sign = 1;
if (*p == '-') {
sign = -1;
p++;
}
if (*p == '+') {
p++;
}
if (PyOS_strnicmp(p, "inf", 4) == 0) {
Py_RETURN_INF(sign);
}
if (PyOS_strnicmp(p, "infinity", 9) == 0) {
Py_RETURN_INF(sign);
}
#if defined(Py_NAN)
if(PyOS_strnicmp(p, "nan", 4) == 0) {
Py_RETURN_NAN;
}
#endif
PyOS_snprintf(buffer, sizeof(buffer),
"invalid literal for float(): %.200s", s);
PyErr_SetString(PyExc_ValueError, buffer);
return NULL;
}
while (*end && isspace(Py_CHARMASK(*end)))
end++;
if (*end != '\0') {
PyOS_snprintf(buffer, sizeof(buffer),
"invalid literal for float(): %.200s", s);
PyErr_SetString(PyExc_ValueError, buffer);
return NULL;
} else if (end != last) {
PyErr_SetString(PyExc_ValueError,
"null byte in argument for float()");
return NULL;
}
if (x == 0.0) {
PyFPE_START_PROTECT("atof", return NULL)
x = PyOS_ascii_atof(s);
PyFPE_END_PROTECT(x)
errno = 0;
}
return PyFloat_FromDouble(x);
}
static void
float_dealloc(PyFloatObject *op) {
if (PyFloat_CheckExact(op)) {
Py_TYPE(op) = (struct _typeobject *)free_list;
free_list = op;
} else
Py_TYPE(op)->tp_free((PyObject *)op);
}
double
PyFloat_AsDouble(PyObject *op) {
PyNumberMethods *nb;
PyFloatObject *fo;
double val;
if (op && PyFloat_Check(op))
return PyFloat_AS_DOUBLE((PyFloatObject*) op);
if (op == NULL) {
PyErr_BadArgument();
return -1;
}
if ((nb = Py_TYPE(op)->tp_as_number) == NULL || nb->nb_float == NULL) {
PyErr_SetString(PyExc_TypeError, "a float is required");
return -1;
}
fo = (PyFloatObject*) (*nb->nb_float) (op);
if (fo == NULL)
return -1;
if (!PyFloat_Check(fo)) {
PyErr_SetString(PyExc_TypeError,
"nb_float should return float object");
return -1;
}
val = PyFloat_AS_DOUBLE(fo);
Py_DECREF(fo);
return val;
}
static void
format_float(char *buf, size_t buflen, PyFloatObject *v, int precision) {
register char *cp;
char format[32];
int i;
assert(PyFloat_Check(v));
PyOS_snprintf(format, 32, "%%.%ig", precision);
PyOS_ascii_formatd(buf, buflen, format, v->ob_fval);
cp = buf;
if (*cp == '-')
cp++;
for (; *cp != '\0'; cp++) {
if (!isdigit(Py_CHARMASK(*cp)))
break;
}
if (*cp == '\0') {
*cp++ = '.';
*cp++ = '0';
*cp++ = '\0';
return;
}
for (i=0; *cp != '\0' && i<3; cp++, i++) {
if (isdigit(Py_CHARMASK(*cp)) || *cp == '.')
continue;
#if defined(Py_NAN)
if (Py_IS_NAN(v->ob_fval)) {
strcpy(buf, "nan");
} else
#endif
if (Py_IS_INFINITY(v->ob_fval)) {
cp = buf;
if (*cp == '-')
cp++;
strcpy(cp, "inf");
}
break;
}
}
void
PyFloat_AsStringEx(char *buf, PyFloatObject *v, int precision) {
format_float(buf, 100, v, precision);
}
#define CONVERT_TO_DOUBLE(obj, dbl) if (PyFloat_Check(obj)) dbl = PyFloat_AS_DOUBLE(obj); else if (convert_to_double(&(obj), &(dbl)) < 0) return obj;
static int
convert_to_double(PyObject **v, double *dbl) {
register PyObject *obj = *v;
if (PyInt_Check(obj)) {
*dbl = (double)PyInt_AS_LONG(obj);
} else if (PyLong_Check(obj)) {
*dbl = PyLong_AsDouble(obj);
if (*dbl == -1.0 && PyErr_Occurred()) {
*v = NULL;
return -1;
}
} else {
Py_INCREF(Py_NotImplemented);
*v = Py_NotImplemented;
return -1;
}
return 0;
}
#define PREC_REPR 17
#define PREC_STR 12
void
PyFloat_AsString(char *buf, PyFloatObject *v) {
format_float(buf, 100, v, PREC_STR);
}
void
PyFloat_AsReprString(char *buf, PyFloatObject *v) {
format_float(buf, 100, v, PREC_REPR);
}
static int
float_print(PyFloatObject *v, FILE *fp, int flags) {
char buf[100];
format_float(buf, sizeof(buf), v,
(flags & Py_PRINT_RAW) ? PREC_STR : PREC_REPR);
Py_BEGIN_ALLOW_THREADS
fputs(buf, fp);
Py_END_ALLOW_THREADS
return 0;
}
static PyObject *
float_repr(PyFloatObject *v) {
char buf[100];
format_float(buf, sizeof(buf), v, PREC_REPR);
return PyString_FromString(buf);
}
static PyObject *
float_str(PyFloatObject *v) {
char buf[100];
format_float(buf, sizeof(buf), v, PREC_STR);
return PyString_FromString(buf);
}
static PyObject*
float_richcompare(PyObject *v, PyObject *w, int op) {
double i, j;
int r = 0;
assert(PyFloat_Check(v));
i = PyFloat_AS_DOUBLE(v);
if (PyFloat_Check(w))
j = PyFloat_AS_DOUBLE(w);
else if (!Py_IS_FINITE(i)) {
if (PyInt_Check(w) || PyLong_Check(w))
j = 0.0;
else
goto Unimplemented;
}
else if (PyInt_Check(w)) {
long jj = PyInt_AS_LONG(w);
#if SIZEOF_LONG > 6
unsigned long abs = (unsigned long)(jj < 0 ? -jj : jj);
if (abs >> 48) {
PyObject *result;
PyObject *ww = PyLong_FromLong(jj);
if (ww == NULL)
return NULL;
result = float_richcompare(v, ww, op);
Py_DECREF(ww);
return result;
}
#endif
j = (double)jj;
assert((long)j == jj);
}
else if (PyLong_Check(w)) {
int vsign = i == 0.0 ? 0 : i < 0.0 ? -1 : 1;
int wsign = _PyLong_Sign(w);
size_t nbits;
int exponent;
if (vsign != wsign) {
i = (double)vsign;
j = (double)wsign;
goto Compare;
}
nbits = _PyLong_NumBits(w);
if (nbits == (size_t)-1 && PyErr_Occurred()) {
PyErr_Clear();
i = (double)vsign;
assert(wsign != 0);
j = wsign * 2.0;
goto Compare;
}
if (nbits <= 48) {
j = PyLong_AsDouble(w);
assert(j != -1.0 || ! PyErr_Occurred());
goto Compare;
}
assert(wsign != 0);
assert(vsign != 0);
if (vsign < 0) {
i = -i;
op = _Py_SwappedOp[op];
}
assert(i > 0.0);
(void) frexp(i, &exponent);
if (exponent < 0 || (size_t)exponent < nbits) {
i = 1.0;
j = 2.0;
goto Compare;
}
if ((size_t)exponent > nbits) {
i = 2.0;
j = 1.0;
goto Compare;
}
{
double fracpart;
double intpart;
PyObject *result = NULL;
PyObject *one = NULL;
PyObject *vv = NULL;
PyObject *ww = w;
if (wsign < 0) {
ww = PyNumber_Negative(w);
if (ww == NULL)
goto Error;
} else
Py_INCREF(ww);
fracpart = modf(i, &intpart);
vv = PyLong_FromDouble(intpart);
if (vv == NULL)
goto Error;
if (fracpart != 0.0) {
PyObject *temp;
one = PyInt_FromLong(1);
if (one == NULL)
goto Error;
temp = PyNumber_Lshift(ww, one);
if (temp == NULL)
goto Error;
Py_DECREF(ww);
ww = temp;
temp = PyNumber_Lshift(vv, one);
if (temp == NULL)
goto Error;
Py_DECREF(vv);
vv = temp;
temp = PyNumber_Or(vv, one);
if (temp == NULL)
goto Error;
Py_DECREF(vv);
vv = temp;
}
r = PyObject_RichCompareBool(vv, ww, op);
if (r < 0)
goto Error;
result = PyBool_FromLong(r);
Error:
Py_XDECREF(vv);
Py_XDECREF(ww);
Py_XDECREF(one);
return result;
}
}
else
goto Unimplemented;
Compare:
PyFPE_START_PROTECT("richcompare", return NULL)
switch (op) {
case Py_EQ:
r = i == j;
break;
case Py_NE:
r = i != j;
break;
case Py_LE:
r = i <= j;
break;
case Py_GE:
r = i >= j;
break;
case Py_LT:
r = i < j;
break;
case Py_GT:
r = i > j;
break;
}
PyFPE_END_PROTECT(r)
return PyBool_FromLong(r);
Unimplemented:
Py_INCREF(Py_NotImplemented);
return Py_NotImplemented;
}
static long
float_hash(PyFloatObject *v) {
return _Py_HashDouble(v->ob_fval);
}
static PyObject *
float_add(PyObject *v, PyObject *w) {
double a,b;
CONVERT_TO_DOUBLE(v, a);
CONVERT_TO_DOUBLE(w, b);
PyFPE_START_PROTECT("add", return 0)
a = a + b;
PyFPE_END_PROTECT(a)
return PyFloat_FromDouble(a);
}
static PyObject *
float_sub(PyObject *v, PyObject *w) {
double a,b;
CONVERT_TO_DOUBLE(v, a);
CONVERT_TO_DOUBLE(w, b);
PyFPE_START_PROTECT("subtract", return 0)
a = a - b;
PyFPE_END_PROTECT(a)
return PyFloat_FromDouble(a);
}
static PyObject *
float_mul(PyObject *v, PyObject *w) {
double a,b;
CONVERT_TO_DOUBLE(v, a);
CONVERT_TO_DOUBLE(w, b);
PyFPE_START_PROTECT("multiply", return 0)
a = a * b;
PyFPE_END_PROTECT(a)
return PyFloat_FromDouble(a);
}
static PyObject *
float_div(PyObject *v, PyObject *w) {
double a,b;
CONVERT_TO_DOUBLE(v, a);
CONVERT_TO_DOUBLE(w, b);
#if defined(Py_NAN)
if (b == 0.0) {
PyErr_SetString(PyExc_ZeroDivisionError,
"float division");
return NULL;
}
#endif
PyFPE_START_PROTECT("divide", return 0)
a = a / b;
PyFPE_END_PROTECT(a)
return PyFloat_FromDouble(a);
}
static PyObject *
float_classic_div(PyObject *v, PyObject *w) {
double a,b;
CONVERT_TO_DOUBLE(v, a);
CONVERT_TO_DOUBLE(w, b);
if (Py_DivisionWarningFlag >= 2 &&
PyErr_Warn(PyExc_DeprecationWarning, "classic float division") < 0)
return NULL;
#if defined(Py_NAN)
if (b == 0.0) {
PyErr_SetString(PyExc_ZeroDivisionError,
"float division");
return NULL;
}
#endif
PyFPE_START_PROTECT("divide", return 0)
a = a / b;
PyFPE_END_PROTECT(a)
return PyFloat_FromDouble(a);
}
static PyObject *
float_rem(PyObject *v, PyObject *w) {
double vx, wx;
double mod;
CONVERT_TO_DOUBLE(v, vx);
CONVERT_TO_DOUBLE(w, wx);
#if defined(Py_NAN)
if (wx == 0.0) {
PyErr_SetString(PyExc_ZeroDivisionError,
"float modulo");
return NULL;
}
#endif
PyFPE_START_PROTECT("modulo", return 0)
mod = fmod(vx, wx);
if (mod && ((wx < 0) != (mod < 0))) {
mod += wx;
}
PyFPE_END_PROTECT(mod)
return PyFloat_FromDouble(mod);
}
static PyObject *
float_divmod(PyObject *v, PyObject *w) {
double vx, wx;
double div, mod, floordiv;
CONVERT_TO_DOUBLE(v, vx);
CONVERT_TO_DOUBLE(w, wx);
if (wx == 0.0) {
PyErr_SetString(PyExc_ZeroDivisionError, "float divmod()");
return NULL;
}
PyFPE_START_PROTECT("divmod", return 0)
mod = fmod(vx, wx);
div = (vx - mod) / wx;
if (mod) {
if ((wx < 0) != (mod < 0)) {
mod += wx;
div -= 1.0;
}
} else {
mod *= mod;
if (wx < 0.0)
mod = -mod;
}
if (div) {
floordiv = floor(div);
if (div - floordiv > 0.5)
floordiv += 1.0;
} else {
div *= div;
floordiv = div * vx / wx;
}
PyFPE_END_PROTECT(floordiv)
return Py_BuildValue("(dd)", floordiv, mod);
}
static PyObject *
float_floor_div(PyObject *v, PyObject *w) {
PyObject *t, *r;
t = float_divmod(v, w);
if (t == NULL || t == Py_NotImplemented)
return t;
assert(PyTuple_CheckExact(t));
r = PyTuple_GET_ITEM(t, 0);
Py_INCREF(r);
Py_DECREF(t);
return r;
}
static PyObject *
float_pow(PyObject *v, PyObject *w, PyObject *z) {
double iv, iw, ix;
if ((PyObject *)z != Py_None) {
PyErr_SetString(PyExc_TypeError, "pow() 3rd argument not "
"allowed unless all arguments are integers");
return NULL;
}
CONVERT_TO_DOUBLE(v, iv);
CONVERT_TO_DOUBLE(w, iw);
if (iw == 0) {
return PyFloat_FromDouble(1.0);
}
if (iv == 0.0) {
if (iw < 0.0) {
PyErr_SetString(PyExc_ZeroDivisionError,
"0.0 cannot be raised to a negative power");
return NULL;
}
return PyFloat_FromDouble(0.0);
}
if (iv == 1.0) {
return PyFloat_FromDouble(1.0);
}
if (iv < 0.0) {
if (iw != floor(iw)) {
PyErr_SetString(PyExc_ValueError, "negative number "
"cannot be raised to a fractional power");
return NULL;
}
if (iv == -1.0 && Py_IS_FINITE(iw)) {
ix = floor(iw * 0.5) * 2.0;
return PyFloat_FromDouble(ix == iw ? 1.0 : -1.0);
}
}
errno = 0;
PyFPE_START_PROTECT("pow", return NULL)
ix = pow(iv, iw);
PyFPE_END_PROTECT(ix)
Py_ADJUST_ERANGE1(ix);
if (errno != 0) {
PyErr_SetFromErrno(errno == ERANGE ? PyExc_OverflowError :
PyExc_ValueError);
return NULL;
}
return PyFloat_FromDouble(ix);
}
static PyObject *
float_neg(PyFloatObject *v) {
return PyFloat_FromDouble(-v->ob_fval);
}
static PyObject *
float_abs(PyFloatObject *v) {
return PyFloat_FromDouble(fabs(v->ob_fval));
}
static int
float_nonzero(PyFloatObject *v) {
return v->ob_fval != 0.0;
}
static int
float_coerce(PyObject **pv, PyObject **pw) {
if (PyInt_Check(*pw)) {
long x = PyInt_AsLong(*pw);
*pw = PyFloat_FromDouble((double)x);
Py_INCREF(*pv);
return 0;
} else if (PyLong_Check(*pw)) {
double x = PyLong_AsDouble(*pw);
if (x == -1.0 && PyErr_Occurred())
return -1;
*pw = PyFloat_FromDouble(x);
Py_INCREF(*pv);
return 0;
} else if (PyFloat_Check(*pw)) {
Py_INCREF(*pv);
Py_INCREF(*pw);
return 0;
}
return 1;
}
static PyObject *
float_is_integer(PyObject *v) {
double x = PyFloat_AsDouble(v);
PyObject *o;
if (x == -1.0 && PyErr_Occurred())
return NULL;
if (!Py_IS_FINITE(x))
Py_RETURN_FALSE;
errno = 0;
PyFPE_START_PROTECT("is_integer", return NULL)
o = (floor(x) == x) ? Py_True : Py_False;
PyFPE_END_PROTECT(x)
if (errno != 0) {
PyErr_SetFromErrno(errno == ERANGE ? PyExc_OverflowError :
PyExc_ValueError);
return NULL;
}
Py_INCREF(o);
return o;
}
#if 0
static PyObject *
float_is_inf(PyObject *v) {
double x = PyFloat_AsDouble(v);
if (x == -1.0 && PyErr_Occurred())
return NULL;
return PyBool_FromLong((long)Py_IS_INFINITY(x));
}
static PyObject *
float_is_nan(PyObject *v) {
double x = PyFloat_AsDouble(v);
if (x == -1.0 && PyErr_Occurred())
return NULL;
return PyBool_FromLong((long)Py_IS_NAN(x));
}
static PyObject *
float_is_finite(PyObject *v) {
double x = PyFloat_AsDouble(v);
if (x == -1.0 && PyErr_Occurred())
return NULL;
return PyBool_FromLong((long)Py_IS_FINITE(x));
}
#endif
static PyObject *
float_trunc(PyObject *v) {
double x = PyFloat_AsDouble(v);
double wholepart;
(void)modf(x, &wholepart);
if (LONG_MIN < wholepart && wholepart < LONG_MAX) {
const long aslong = (long)wholepart;
return PyInt_FromLong(aslong);
}
return PyLong_FromDouble(wholepart);
}
static PyObject *
float_long(PyObject *v) {
double x = PyFloat_AsDouble(v);
return PyLong_FromDouble(x);
}
static PyObject *
float_float(PyObject *v) {
if (PyFloat_CheckExact(v))
Py_INCREF(v);
else
v = PyFloat_FromDouble(((PyFloatObject *)v)->ob_fval);
return v;
}
static char
char_from_hex(int x) {
assert(0 <= x && x < 16);
return "0123456789abcdef"[x];
}
static int
hex_from_char(char c) {
int x;
switch(c) {
case '0':
x = 0;
break;
case '1':
x = 1;
break;
case '2':
x = 2;
break;
case '3':
x = 3;
break;
case '4':
x = 4;
break;
case '5':
x = 5;
break;
case '6':
x = 6;
break;
case '7':
x = 7;
break;
case '8':
x = 8;
break;
case '9':
x = 9;
break;
case 'a':
case 'A':
x = 10;
break;
case 'b':
case 'B':
x = 11;
break;
case 'c':
case 'C':
x = 12;
break;
case 'd':
case 'D':
x = 13;
break;
case 'e':
case 'E':
x = 14;
break;
case 'f':
case 'F':
x = 15;
break;
default:
x = -1;
break;
}
return x;
}
#define TOHEX_NBITS DBL_MANT_DIG + 3 - (DBL_MANT_DIG+2)%4
static PyObject *
float_hex(PyObject *v) {
double x, m;
int e, shift, i, si, esign;
char s[(TOHEX_NBITS-1)/4+3];
CONVERT_TO_DOUBLE(v, x);
if (Py_IS_NAN(x) || Py_IS_INFINITY(x))
return float_str((PyFloatObject *)v);
if (x == 0.0) {
if(copysign(1.0, x) == -1.0)
return PyString_FromString("-0x0.0p+0");
else
return PyString_FromString("0x0.0p+0");
}
m = frexp(fabs(x), &e);
shift = 1 - MAX(DBL_MIN_EXP - e, 0);
m = ldexp(m, shift);
e -= shift;
si = 0;
s[si] = char_from_hex((int)m);
si++;
m -= (int)m;
s[si] = '.';
si++;
for (i=0; i < (TOHEX_NBITS-1)/4; i++) {
m *= 16.0;
s[si] = char_from_hex((int)m);
si++;
m -= (int)m;
}
s[si] = '\0';
if (e < 0) {
esign = (int)'-';
e = -e;
} else
esign = (int)'+';
if (x < 0.0)
return PyString_FromFormat("-0x%sp%c%d", s, esign, e);
else
return PyString_FromFormat("0x%sp%c%d", s, esign, e);
}
PyDoc_STRVAR(float_hex_doc,
"float.hex() -> string\n\
\n\
Return a hexadecimal representation of a floating-point number.\n\
>>> (-0.1).hex()\n\
'-0x1.999999999999ap-4'\n\
>>> 3.14159.hex()\n\
'0x1.921f9f01b866ep+1'");
static PyObject *
float_fromhex(PyObject *cls, PyObject *arg) {
PyObject *result_as_float, *result;
double x;
long exp, top_exp, lsb, key_digit;
char *s, *coeff_start, *s_store, *coeff_end, *exp_start, *s_end;
int half_eps, digit, round_up, sign=1;
Py_ssize_t length, ndigits, fdigits, i;
if (PyString_AsStringAndSize(arg, &s, &length))
return NULL;
s_end = s + length;
while (isspace(*s))
s++;
if (*s == '-') {
s++;
sign = -1;
} else if (*s == '+')
s++;
if (PyOS_strnicmp(s, "nan", 4) == 0) {
x = Py_NAN;
goto finished;
}
if (PyOS_strnicmp(s, "inf", 4) == 0 ||
PyOS_strnicmp(s, "infinity", 9) == 0) {
x = sign*Py_HUGE_VAL;
goto finished;
}
s_store = s;
if (*s == '0') {
s++;
if (tolower(*s) == (int)'x')
s++;
else
s = s_store;
}
coeff_start = s;
while (hex_from_char(*s) >= 0)
s++;
s_store = s;
if (*s == '.') {
s++;
while (hex_from_char(*s) >= 0)
s++;
coeff_end = s-1;
} else
coeff_end = s;
ndigits = coeff_end - coeff_start;
fdigits = coeff_end - s_store;
if (ndigits == 0)
goto parse_error;
if (ndigits > MIN(DBL_MIN_EXP - DBL_MANT_DIG - LONG_MIN/2,
LONG_MAX/2 + 1 - DBL_MAX_EXP)/4)
goto insane_length_error;
if (tolower(*s) == (int)'p') {
s++;
exp_start = s;
if (*s == '-' || *s == '+')
s++;
if (!('0' <= *s && *s <= '9'))
goto parse_error;
s++;
while ('0' <= *s && *s <= '9')
s++;
exp = strtol(exp_start, NULL, 10);
} else
exp = 0;
while (isspace(*s))
s++;
if (s != s_end)
goto parse_error;
#define HEX_DIGIT(j) hex_from_char(*((j) < fdigits ? coeff_end-(j) : coeff_end-1-(j)))
while (ndigits > 0 && HEX_DIGIT(ndigits-1) == 0)
ndigits--;
if (ndigits == 0 || exp < LONG_MIN/2) {
x = sign * 0.0;
goto finished;
}
if (exp > LONG_MAX/2)
goto overflow_error;
exp = exp - 4*((long)fdigits);
top_exp = exp + 4*((long)ndigits - 1);
for (digit = HEX_DIGIT(ndigits-1); digit != 0; digit /= 2)
top_exp++;
if (top_exp < DBL_MIN_EXP - DBL_MANT_DIG) {
x = sign * 0.0;
goto finished;
}
if (top_exp > DBL_MAX_EXP)
goto overflow_error;
lsb = MAX(top_exp, (long)DBL_MIN_EXP) - DBL_MANT_DIG;
x = 0.0;
if (exp >= lsb) {
for (i = ndigits-1; i >= 0; i--)
x = 16.0*x + HEX_DIGIT(i);
x = sign * ldexp(x, (int)(exp));
goto finished;
}
half_eps = 1 << (int)((lsb - exp - 1) % 4);
key_digit = (lsb - exp - 1) / 4;
for (i = ndigits-1; i > key_digit; i--)
x = 16.0*x + HEX_DIGIT(i);
digit = HEX_DIGIT(key_digit);
x = 16.0*x + (double)(digit & (16-2*half_eps));
if ((digit & half_eps) != 0) {
round_up = 0;
if ((digit & (3*half_eps-1)) != 0 ||
(half_eps == 8 && (HEX_DIGIT(key_digit+1) & 1) != 0))
round_up = 1;
else
for (i = key_digit-1; i >= 0; i--)
if (HEX_DIGIT(i) != 0) {
round_up = 1;
break;
}
if (round_up == 1) {
x += 2*half_eps;
if (top_exp == DBL_MAX_EXP &&
x == ldexp((double)(2*half_eps), DBL_MANT_DIG))
goto overflow_error;
}
}
x = sign * ldexp(x, (int)(exp+4*key_digit));
finished:
result_as_float = Py_BuildValue("(d)", x);
if (result_as_float == NULL)
return NULL;
result = PyObject_CallObject(cls, result_as_float);
Py_DECREF(result_as_float);
return result;
overflow_error:
PyErr_SetString(PyExc_OverflowError,
"hexadecimal value too large to represent as a float");
return NULL;
parse_error:
PyErr_SetString(PyExc_ValueError,
"invalid hexadecimal floating-point string");
return NULL;
insane_length_error:
PyErr_SetString(PyExc_ValueError,
"hexadecimal string too long to convert");
return NULL;
}
PyDoc_STRVAR(float_fromhex_doc,
"float.fromhex(string) -> float\n\
\n\
Create a floating-point number from a hexadecimal string.\n\
>>> float.fromhex('0x1.ffffp10')\n\
2047.984375\n\
>>> float.fromhex('-0x1p-1074')\n\
-4.9406564584124654e-324");
static PyObject *
float_as_integer_ratio(PyObject *v, PyObject *unused) {
double self;
double float_part;
int exponent;
int i;
PyObject *prev;
PyObject *py_exponent = NULL;
PyObject *numerator = NULL;
PyObject *denominator = NULL;
PyObject *result_pair = NULL;
PyNumberMethods *long_methods = PyLong_Type.tp_as_number;
#define INPLACE_UPDATE(obj, call) prev = obj; obj = call; Py_DECREF(prev);
CONVERT_TO_DOUBLE(v, self);
if (Py_IS_INFINITY(self)) {
PyErr_SetString(PyExc_OverflowError,
"Cannot pass infinity to float.as_integer_ratio.");
return NULL;
}
#if defined(Py_NAN)
if (Py_IS_NAN(self)) {
PyErr_SetString(PyExc_ValueError,
"Cannot pass NaN to float.as_integer_ratio.");
return NULL;
}
#endif
PyFPE_START_PROTECT("as_integer_ratio", goto error);
float_part = frexp(self, &exponent);
PyFPE_END_PROTECT(float_part);
for (i=0; i<300 && float_part != floor(float_part) ; i++) {
float_part *= 2.0;
exponent--;
}
numerator = PyLong_FromDouble(float_part);
if (numerator == NULL) goto error;
denominator = PyLong_FromLong(1);
py_exponent = PyLong_FromLong(labs((long)exponent));
if (py_exponent == NULL) goto error;
INPLACE_UPDATE(py_exponent,
long_methods->nb_lshift(denominator, py_exponent));
if (py_exponent == NULL) goto error;
if (exponent > 0) {
INPLACE_UPDATE(numerator,
long_methods->nb_multiply(numerator, py_exponent));
if (numerator == NULL) goto error;
} else {
Py_DECREF(denominator);
denominator = py_exponent;
py_exponent = NULL;
}
INPLACE_UPDATE(numerator, PyNumber_Int(numerator));
if (numerator == NULL) goto error;
INPLACE_UPDATE(denominator, PyNumber_Int(denominator));
if (denominator == NULL) goto error;
result_pair = PyTuple_Pack(2, numerator, denominator);
#undef INPLACE_UPDATE
error:
Py_XDECREF(py_exponent);
Py_XDECREF(denominator);
Py_XDECREF(numerator);
return result_pair;
}
PyDoc_STRVAR(float_as_integer_ratio_doc,
"float.as_integer_ratio() -> (int, int)\n"
"\n"
"Returns a pair of integers, whose ratio is exactly equal to the original\n"
"float and with a positive denominator.\n"
"Raises OverflowError on infinities and a ValueError on NaNs.\n"
"\n"
">>> (10.0).as_integer_ratio()\n"
"(10, 1)\n"
">>> (0.0).as_integer_ratio()\n"
"(0, 1)\n"
">>> (-.25).as_integer_ratio()\n"
"(-1, 4)");
static PyObject *
float_subtype_new(PyTypeObject *type, PyObject *args, PyObject *kwds);
static PyObject *
float_new(PyTypeObject *type, PyObject *args, PyObject *kwds) {
PyObject *x = Py_False;
static char *kwlist[] = {"x", 0};
if (type != &PyFloat_Type)
return float_subtype_new(type, args, kwds);
if (!PyArg_ParseTupleAndKeywords(args, kwds, "|O:float", kwlist, &x))
return NULL;
if (PyString_Check(x))
return PyFloat_FromString(x, NULL);
return PyNumber_Float(x);
}
static PyObject *
float_subtype_new(PyTypeObject *type, PyObject *args, PyObject *kwds) {
PyObject *tmp, *newobj;
assert(PyType_IsSubtype(type, &PyFloat_Type));
tmp = float_new(&PyFloat_Type, args, kwds);
if (tmp == NULL)
return NULL;
assert(PyFloat_CheckExact(tmp));
newobj = type->tp_alloc(type, 0);
if (newobj == NULL) {
Py_DECREF(tmp);
return NULL;
}
((PyFloatObject *)newobj)->ob_fval = ((PyFloatObject *)tmp)->ob_fval;
Py_DECREF(tmp);
return newobj;
}
static PyObject *
float_getnewargs(PyFloatObject *v) {
return Py_BuildValue("(d)", v->ob_fval);
}
typedef enum {
unknown_format, ieee_big_endian_format, ieee_little_endian_format
} float_format_type;
static float_format_type double_format, float_format;
static float_format_type detected_double_format, detected_float_format;
static PyObject *
float_getformat(PyTypeObject *v, PyObject* arg) {
char* s;
float_format_type r;
if (!PyString_Check(arg)) {
PyErr_Format(PyExc_TypeError,
"__getformat__() argument must be string, not %.500s",
Py_TYPE(arg)->tp_name);
return NULL;
}
s = PyString_AS_STRING(arg);
if (strcmp(s, "double") == 0) {
r = double_format;
} else if (strcmp(s, "float") == 0) {
r = float_format;
} else {
PyErr_SetString(PyExc_ValueError,
"__getformat__() argument 1 must be "
"'double' or 'float'");
return NULL;
}
switch (r) {
case unknown_format:
return PyString_FromString("unknown");
case ieee_little_endian_format:
return PyString_FromString("IEEE, little-endian");
case ieee_big_endian_format:
return PyString_FromString("IEEE, big-endian");
default:
Py_FatalError("insane float_format or double_format");
return NULL;
}
}
PyDoc_STRVAR(float_getformat_doc,
"float.__getformat__(typestr) -> string\n"
"\n"
"You probably don't want to use this function. It exists mainly to be\n"
"used in Python's test suite.\n"
"\n"
"typestr must be 'double' or 'float'. This function returns whichever of\n"
"'unknown', 'IEEE, big-endian' or 'IEEE, little-endian' best describes the\n"
"format of floating point numbers used by the C type named by typestr.");
static PyObject *
float_setformat(PyTypeObject *v, PyObject* args) {
char* typestr;
char* format;
float_format_type f;
float_format_type detected;
float_format_type *p;
if (!PyArg_ParseTuple(args, "ss:__setformat__", &typestr, &format))
return NULL;
if (strcmp(typestr, "double") == 0) {
p = &double_format;
detected = detected_double_format;
} else if (strcmp(typestr, "float") == 0) {
p = &float_format;
detected = detected_float_format;
} else {
PyErr_SetString(PyExc_ValueError,
"__setformat__() argument 1 must "
"be 'double' or 'float'");
return NULL;
}
if (strcmp(format, "unknown") == 0) {
f = unknown_format;
} else if (strcmp(format, "IEEE, little-endian") == 0) {
f = ieee_little_endian_format;
} else if (strcmp(format, "IEEE, big-endian") == 0) {
f = ieee_big_endian_format;
} else {
PyErr_SetString(PyExc_ValueError,
"__setformat__() argument 2 must be "
"'unknown', 'IEEE, little-endian' or "
"'IEEE, big-endian'");
return NULL;
}
if (f != unknown_format && f != detected) {
PyErr_Format(PyExc_ValueError,
"can only set %s format to 'unknown' or the "
"detected platform value", typestr);
return NULL;
}
*p = f;
Py_RETURN_NONE;
}
PyDoc_STRVAR(float_setformat_doc,
"float.__setformat__(typestr, fmt) -> None\n"
"\n"
"You probably don't want to use this function. It exists mainly to be\n"
"used in Python's test suite.\n"
"\n"
"typestr must be 'double' or 'float'. fmt must be one of 'unknown',\n"
"'IEEE, big-endian' or 'IEEE, little-endian', and in addition can only be\n"
"one of the latter two if it appears to match the underlying C reality.\n"
"\n"
"Overrides the automatic determination of C-level floating point type.\n"
"This affects how floats are converted to and from binary strings.");
static PyObject *
float_getzero(PyObject *v, void *closure) {
return PyFloat_FromDouble(0.0);
}
static PyObject *
float__format__(PyObject *self, PyObject *args) {
PyObject *format_spec;
if (!PyArg_ParseTuple(args, "O:__format__", &format_spec))
return NULL;
if (PyBytes_Check(format_spec))
return _PyFloat_FormatAdvanced(self,
PyBytes_AS_STRING(format_spec),
PyBytes_GET_SIZE(format_spec));
if (PyUnicode_Check(format_spec)) {
PyObject *result;
PyObject *str_spec = PyObject_Str(format_spec);
if (str_spec == NULL)
return NULL;
result = _PyFloat_FormatAdvanced(self,
PyBytes_AS_STRING(str_spec),
PyBytes_GET_SIZE(str_spec));
Py_DECREF(str_spec);
return result;
}
PyErr_SetString(PyExc_TypeError, "__format__ requires str or unicode");
return NULL;
}
PyDoc_STRVAR(float__format__doc,
"float.__format__(format_spec) -> string\n"
"\n"
"Formats the float according to format_spec.");
static PyMethodDef float_methods[] = {
{
"conjugate", (PyCFunction)float_float, METH_NOARGS,
"Returns self, the complex conjugate of any float."
},
{
"__trunc__", (PyCFunction)float_trunc, METH_NOARGS,
"Returns the Integral closest to x between 0 and x."
},
{
"as_integer_ratio", (PyCFunction)float_as_integer_ratio, METH_NOARGS,
float_as_integer_ratio_doc
},
{
"fromhex", (PyCFunction)float_fromhex,
METH_O|METH_CLASS, float_fromhex_doc
},
{
"hex", (PyCFunction)float_hex,
METH_NOARGS, float_hex_doc
},
{
"is_integer", (PyCFunction)float_is_integer, METH_NOARGS,
"Returns True if the float is an integer."
},
#if 0
{
"is_inf", (PyCFunction)float_is_inf, METH_NOARGS,
"Returns True if the float is positive or negative infinite."
},
{
"is_finite", (PyCFunction)float_is_finite, METH_NOARGS,
"Returns True if the float is finite, neither infinite nor NaN."
},
{
"is_nan", (PyCFunction)float_is_nan, METH_NOARGS,
"Returns True if the float is not a number (NaN)."
},
#endif
{"__getnewargs__", (PyCFunction)float_getnewargs, METH_NOARGS},
{
"__getformat__", (PyCFunction)float_getformat,
METH_O|METH_CLASS, float_getformat_doc
},
{
"__setformat__", (PyCFunction)float_setformat,
METH_VARARGS|METH_CLASS, float_setformat_doc
},
{
"__format__", (PyCFunction)float__format__,
METH_VARARGS, float__format__doc
},
{NULL, NULL}
};
static PyGetSetDef float_getset[] = {
{
"real",
(getter)float_float, (setter)NULL,
"the real part of a complex number",
NULL
},
{
"imag",
(getter)float_getzero, (setter)NULL,
"the imaginary part of a complex number",
NULL
},
{NULL}
};
PyDoc_STRVAR(float_doc,
"float(x) -> floating point number\n\
\n\
Convert a string or number to a floating point number, if possible.");
static PyNumberMethods float_as_number = {
float_add,
float_sub,
float_mul,
float_classic_div,
float_rem,
float_divmod,
float_pow,
(unaryfunc)float_neg,
(unaryfunc)float_float,
(unaryfunc)float_abs,
(inquiry)float_nonzero,
0,
0,
0,
0,
0,
0,
float_coerce,
float_trunc,
float_long,
float_float,
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
float_floor_div,
float_div,
0,
0,
};
PyTypeObject PyFloat_Type = {
PyVarObject_HEAD_INIT(&PyType_Type, 0)
"float",
sizeof(PyFloatObject),
0,
(destructor)float_dealloc,
(printfunc)float_print,
0,
0,
0,
(reprfunc)float_repr,
&float_as_number,
0,
0,
(hashfunc)float_hash,
0,
(reprfunc)float_str,
PyObject_GenericGetAttr,
0,
0,
Py_TPFLAGS_DEFAULT | Py_TPFLAGS_CHECKTYPES |
Py_TPFLAGS_BASETYPE,
float_doc,
0,
0,
float_richcompare,
0,
0,
0,
float_methods,
0,
float_getset,
0,
0,
0,
0,
0,
0,
0,
float_new,
};
void
_PyFloat_Init(void) {
#if SIZEOF_DOUBLE == 8
{
double x = 9006104071832581.0;
if (memcmp(&x, "\x43\x3f\xff\x01\x02\x03\x04\x05", 8) == 0)
detected_double_format = ieee_big_endian_format;
else if (memcmp(&x, "\x05\x04\x03\x02\x01\xff\x3f\x43", 8) == 0)
detected_double_format = ieee_little_endian_format;
else
detected_double_format = unknown_format;
}
#else
detected_double_format = unknown_format;
#endif
#if SIZEOF_FLOAT == 4
{
float y = 16711938.0;
if (memcmp(&y, "\x4b\x7f\x01\x02", 4) == 0)
detected_float_format = ieee_big_endian_format;
else if (memcmp(&y, "\x02\x01\x7f\x4b", 4) == 0)
detected_float_format = ieee_little_endian_format;
else
detected_float_format = unknown_format;
}
#else
detected_float_format = unknown_format;
#endif
double_format = detected_double_format;
float_format = detected_float_format;
if (FloatInfoType.tp_name == 0)
PyStructSequence_InitType(&FloatInfoType, &floatinfo_desc);
}
int
PyFloat_ClearFreeList(void) {
PyFloatObject *p;
PyFloatBlock *list, *next;
int i;
int u;
int freelist_size = 0;
list = block_list;
block_list = NULL;
free_list = NULL;
while (list != NULL) {
u = 0;
for (i = 0, p = &list->objects[0];
i < N_FLOATOBJECTS;
i++, p++) {
if (PyFloat_CheckExact(p) && Py_REFCNT(p) != 0)
u++;
}
next = list->next;
if (u) {
list->next = block_list;
block_list = list;
for (i = 0, p = &list->objects[0];
i < N_FLOATOBJECTS;
i++, p++) {
if (!PyFloat_CheckExact(p) ||
Py_REFCNT(p) == 0) {
Py_TYPE(p) = (struct _typeobject *)
free_list;
free_list = p;
}
}
} else {
PyMem_FREE(list);
}
freelist_size += u;
list = next;
}
return freelist_size;
}
void
PyFloat_Fini(void) {
PyFloatObject *p;
PyFloatBlock *list;
int i;
int u;
u = PyFloat_ClearFreeList();
if (!Py_VerboseFlag)
return;
fprintf(stderr, "#cleanup floats");
if (!u) {
fprintf(stderr, "\n");
} else {
fprintf(stderr,
": %d unfreed float%s\n",
u, u == 1 ? "" : "s");
}
if (Py_VerboseFlag > 1) {
list = block_list;
while (list != NULL) {
for (i = 0, p = &list->objects[0];
i < N_FLOATOBJECTS;
i++, p++) {
if (PyFloat_CheckExact(p) &&
Py_REFCNT(p) != 0) {
char buf[100];
PyFloat_AsString(buf, p);
fprintf(stderr,
"#<float at %p, refcnt=%ld, val=%s>\n",
p, (long)Py_REFCNT(p), buf);
}
}
list = list->next;
}
}
}
int
_PyFloat_Pack4(double x, unsigned char *p, int le) {
if (float_format == unknown_format) {
unsigned char sign;
int e;
double f;
unsigned int fbits;
int incr = 1;
if (le) {
p += 3;
incr = -1;
}
if (x < 0) {
sign = 1;
x = -x;
} else
sign = 0;
f = frexp(x, &e);
if (0.5 <= f && f < 1.0) {
f *= 2.0;
e--;
} else if (f == 0.0)
e = 0;
else {
PyErr_SetString(PyExc_SystemError,
"frexp() result out of range");
return -1;
}
if (e >= 128)
goto Overflow;
else if (e < -126) {
f = ldexp(f, 126 + e);
e = 0;
} else if (!(e == 0 && f == 0.0)) {
e += 127;
f -= 1.0;
}
f *= 8388608.0;
fbits = (unsigned int)(f + 0.5);
assert(fbits <= 8388608);
if (fbits >> 23) {
fbits = 0;
++e;
if (e >= 255)
goto Overflow;
}
*p = (sign << 7) | (e >> 1);
p += incr;
*p = (char) (((e & 1) << 7) | (fbits >> 16));
p += incr;
*p = (fbits >> 8) & 0xFF;
p += incr;
*p = fbits & 0xFF;
return 0;
} else {
float y = (float)x;
const char *s = (char*)&y;
int i, incr = 1;
if (Py_IS_INFINITY(y) && !Py_IS_INFINITY(x))
goto Overflow;
if ((float_format == ieee_little_endian_format && !le)
|| (float_format == ieee_big_endian_format && le)) {
p += 3;
incr = -1;
}
for (i = 0; i < 4; i++) {
*p = *s++;
p += incr;
}
return 0;
}
Overflow:
PyErr_SetString(PyExc_OverflowError,
"float too large to pack with f format");
return -1;
}
int
_PyFloat_Pack8(double x, unsigned char *p, int le) {
if (double_format == unknown_format) {
unsigned char sign;
int e;
double f;
unsigned int fhi, flo;
int incr = 1;
if (le) {
p += 7;
incr = -1;
}
if (x < 0) {
sign = 1;
x = -x;
} else
sign = 0;
f = frexp(x, &e);
if (0.5 <= f && f < 1.0) {
f *= 2.0;
e--;
} else if (f == 0.0)
e = 0;
else {
PyErr_SetString(PyExc_SystemError,
"frexp() result out of range");
return -1;
}
if (e >= 1024)
goto Overflow;
else if (e < -1022) {
f = ldexp(f, 1022 + e);
e = 0;
} else if (!(e == 0 && f == 0.0)) {
e += 1023;
f -= 1.0;
}
f *= 268435456.0;
fhi = (unsigned int)f;
assert(fhi < 268435456);
f -= (double)fhi;
f *= 16777216.0;
flo = (unsigned int)(f + 0.5);
assert(flo <= 16777216);
if (flo >> 24) {
flo = 0;
++fhi;
if (fhi >> 28) {
fhi = 0;
++e;
if (e >= 2047)
goto Overflow;
}
}
*p = (sign << 7) | (e >> 4);
p += incr;
*p = (unsigned char) (((e & 0xF) << 4) | (fhi >> 24));
p += incr;
*p = (fhi >> 16) & 0xFF;
p += incr;
*p = (fhi >> 8) & 0xFF;
p += incr;
*p = fhi & 0xFF;
p += incr;
*p = (flo >> 16) & 0xFF;
p += incr;
*p = (flo >> 8) & 0xFF;
p += incr;
*p = flo & 0xFF;
p += incr;
return 0;
Overflow:
PyErr_SetString(PyExc_OverflowError,
"float too large to pack with d format");
return -1;
} else {
const char *s = (char*)&x;
int i, incr = 1;
if ((double_format == ieee_little_endian_format && !le)
|| (double_format == ieee_big_endian_format && le)) {
p += 7;
incr = -1;
}
for (i = 0; i < 8; i++) {
*p = *s++;
p += incr;
}
return 0;
}
}
double
_PyFloat_Unpack4(const unsigned char *p, int le) {
if (float_format == unknown_format) {
unsigned char sign;
int e;
unsigned int f;
double x;
int incr = 1;
if (le) {
p += 3;
incr = -1;
}
sign = (*p >> 7) & 1;
e = (*p & 0x7F) << 1;
p += incr;
e |= (*p >> 7) & 1;
f = (*p & 0x7F) << 16;
p += incr;
if (e == 255) {
PyErr_SetString(
PyExc_ValueError,
"can't unpack IEEE 754 special value "
"on non-IEEE platform");
return -1;
}
f |= *p << 8;
p += incr;
f |= *p;
x = (double)f / 8388608.0;
if (e == 0)
e = -126;
else {
x += 1.0;
e -= 127;
}
x = ldexp(x, e);
if (sign)
x = -x;
return x;
} else {
float x;
if ((float_format == ieee_little_endian_format && !le)
|| (float_format == ieee_big_endian_format && le)) {
char buf[4];
char *d = &buf[3];
int i;
for (i = 0; i < 4; i++) {
*d-- = *p++;
}
memcpy(&x, buf, 4);
} else {
memcpy(&x, p, 4);
}
return x;
}
}
double
_PyFloat_Unpack8(const unsigned char *p, int le) {
if (double_format == unknown_format) {
unsigned char sign;
int e;
unsigned int fhi, flo;
double x;
int incr = 1;
if (le) {
p += 7;
incr = -1;
}
sign = (*p >> 7) & 1;
e = (*p & 0x7F) << 4;
p += incr;
e |= (*p >> 4) & 0xF;
fhi = (*p & 0xF) << 24;
p += incr;
if (e == 2047) {
PyErr_SetString(
PyExc_ValueError,
"can't unpack IEEE 754 special value "
"on non-IEEE platform");
return -1.0;
}
fhi |= *p << 16;
p += incr;
fhi |= *p << 8;
p += incr;
fhi |= *p;
p += incr;
flo = *p << 16;
p += incr;
flo |= *p << 8;
p += incr;
flo |= *p;
x = (double)fhi + (double)flo / 16777216.0;
x /= 268435456.0;
if (e == 0)
e = -1022;
else {
x += 1.0;
e -= 1023;
}
x = ldexp(x, e);
if (sign)
x = -x;
return x;
} else {
double x;
if ((double_format == ieee_little_endian_format && !le)
|| (double_format == ieee_big_endian_format && le)) {
char buf[8];
char *d = &buf[7];
int i;
for (i = 0; i < 8; i++) {
*d-- = *p++;
}
memcpy(&x, buf, 8);
} else {
memcpy(&x, p, 8);
}
return x;
}
}
