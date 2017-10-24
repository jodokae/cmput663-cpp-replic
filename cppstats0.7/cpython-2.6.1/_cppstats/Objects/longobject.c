#include "Python.h"
#include "longintrepr.h"
#include <ctype.h>
#define KARATSUBA_CUTOFF 70
#define KARATSUBA_SQUARE_CUTOFF (2 * KARATSUBA_CUTOFF)
#define FIVEARY_CUTOFF 8
#define ABS(x) ((x) < 0 ? -(x) : (x))
#undef MIN
#undef MAX
#define MAX(x, y) ((x) < (y) ? (y) : (x))
#define MIN(x, y) ((x) > (y) ? (y) : (x))
static PyLongObject *long_normalize(PyLongObject *);
static PyLongObject *mul1(PyLongObject *, wdigit);
static PyLongObject *muladd1(PyLongObject *, wdigit, wdigit);
static PyLongObject *divrem1(PyLongObject *, digit, digit *);
#define SIGCHECK(PyTryBlock) if (--_Py_Ticker < 0) { _Py_Ticker = _Py_CheckInterval; if (PyErr_CheckSignals()) PyTryBlock }
static PyLongObject *
long_normalize(register PyLongObject *v) {
Py_ssize_t j = ABS(Py_SIZE(v));
Py_ssize_t i = j;
while (i > 0 && v->ob_digit[i-1] == 0)
--i;
if (i != j)
Py_SIZE(v) = (Py_SIZE(v) < 0) ? -(i) : i;
return v;
}
PyLongObject *
_PyLong_New(Py_ssize_t size) {
if (size > PY_SSIZE_T_MAX) {
PyErr_NoMemory();
return NULL;
}
return PyObject_NEW_VAR(PyLongObject, &PyLong_Type, size);
}
PyObject *
_PyLong_Copy(PyLongObject *src) {
PyLongObject *result;
Py_ssize_t i;
assert(src != NULL);
i = src->ob_size;
if (i < 0)
i = -(i);
result = _PyLong_New(i);
if (result != NULL) {
result->ob_size = src->ob_size;
while (--i >= 0)
result->ob_digit[i] = src->ob_digit[i];
}
return (PyObject *)result;
}
PyObject *
PyLong_FromLong(long ival) {
PyLongObject *v;
unsigned long abs_ival;
unsigned long t;
int ndigits = 0;
int negative = 0;
if (ival < 0) {
abs_ival = (unsigned long)(-1-ival) + 1;
negative = 1;
} else {
abs_ival = (unsigned long)ival;
}
t = abs_ival;
while (t) {
++ndigits;
t >>= PyLong_SHIFT;
}
v = _PyLong_New(ndigits);
if (v != NULL) {
digit *p = v->ob_digit;
v->ob_size = negative ? -ndigits : ndigits;
t = abs_ival;
while (t) {
*p++ = (digit)(t & PyLong_MASK);
t >>= PyLong_SHIFT;
}
}
return (PyObject *)v;
}
PyObject *
PyLong_FromUnsignedLong(unsigned long ival) {
PyLongObject *v;
unsigned long t;
int ndigits = 0;
t = (unsigned long)ival;
while (t) {
++ndigits;
t >>= PyLong_SHIFT;
}
v = _PyLong_New(ndigits);
if (v != NULL) {
digit *p = v->ob_digit;
Py_SIZE(v) = ndigits;
while (ival) {
*p++ = (digit)(ival & PyLong_MASK);
ival >>= PyLong_SHIFT;
}
}
return (PyObject *)v;
}
PyObject *
PyLong_FromDouble(double dval) {
PyLongObject *v;
double frac;
int i, ndig, expo, neg;
neg = 0;
if (Py_IS_INFINITY(dval)) {
PyErr_SetString(PyExc_OverflowError,
"cannot convert float infinity to integer");
return NULL;
}
if (Py_IS_NAN(dval)) {
PyErr_SetString(PyExc_ValueError,
"cannot convert float NaN to integer");
return NULL;
}
if (dval < 0.0) {
neg = 1;
dval = -dval;
}
frac = frexp(dval, &expo);
if (expo <= 0)
return PyLong_FromLong(0L);
ndig = (expo-1) / PyLong_SHIFT + 1;
v = _PyLong_New(ndig);
if (v == NULL)
return NULL;
frac = ldexp(frac, (expo-1) % PyLong_SHIFT + 1);
for (i = ndig; --i >= 0; ) {
long bits = (long)frac;
v->ob_digit[i] = (digit) bits;
frac = frac - (double)bits;
frac = ldexp(frac, PyLong_SHIFT);
}
if (neg)
Py_SIZE(v) = -(Py_SIZE(v));
return (PyObject *)v;
}
#define PY_ABS_LONG_MIN (0-(unsigned long)LONG_MIN)
#define PY_ABS_SSIZE_T_MIN (0-(size_t)PY_SSIZE_T_MIN)
long
PyLong_AsLong(PyObject *vv) {
register PyLongObject *v;
unsigned long x, prev;
Py_ssize_t i;
int sign;
if (vv == NULL || !PyLong_Check(vv)) {
if (vv != NULL && PyInt_Check(vv))
return PyInt_AsLong(vv);
PyErr_BadInternalCall();
return -1;
}
v = (PyLongObject *)vv;
i = v->ob_size;
sign = 1;
x = 0;
if (i < 0) {
sign = -1;
i = -(i);
}
while (--i >= 0) {
prev = x;
x = (x << PyLong_SHIFT) + v->ob_digit[i];
if ((x >> PyLong_SHIFT) != prev)
goto overflow;
}
if (x <= (unsigned long)LONG_MAX) {
return (long)x * sign;
} else if (sign < 0 && x == PY_ABS_LONG_MIN) {
return LONG_MIN;
}
overflow:
PyErr_SetString(PyExc_OverflowError,
"long int too large to convert to int");
return -1;
}
Py_ssize_t
PyLong_AsSsize_t(PyObject *vv) {
register PyLongObject *v;
size_t x, prev;
Py_ssize_t i;
int sign;
if (vv == NULL || !PyLong_Check(vv)) {
PyErr_BadInternalCall();
return -1;
}
v = (PyLongObject *)vv;
i = v->ob_size;
sign = 1;
x = 0;
if (i < 0) {
sign = -1;
i = -(i);
}
while (--i >= 0) {
prev = x;
x = (x << PyLong_SHIFT) + v->ob_digit[i];
if ((x >> PyLong_SHIFT) != prev)
goto overflow;
}
if (x <= (size_t)PY_SSIZE_T_MAX) {
return (Py_ssize_t)x * sign;
} else if (sign < 0 && x == PY_ABS_SSIZE_T_MIN) {
return PY_SSIZE_T_MIN;
}
overflow:
PyErr_SetString(PyExc_OverflowError,
"long int too large to convert to int");
return -1;
}
unsigned long
PyLong_AsUnsignedLong(PyObject *vv) {
register PyLongObject *v;
unsigned long x, prev;
Py_ssize_t i;
if (vv == NULL || !PyLong_Check(vv)) {
if (vv != NULL && PyInt_Check(vv)) {
long val = PyInt_AsLong(vv);
if (val < 0) {
PyErr_SetString(PyExc_OverflowError,
"can't convert negative value to unsigned long");
return (unsigned long) -1;
}
return val;
}
PyErr_BadInternalCall();
return (unsigned long) -1;
}
v = (PyLongObject *)vv;
i = Py_SIZE(v);
x = 0;
if (i < 0) {
PyErr_SetString(PyExc_OverflowError,
"can't convert negative value to unsigned long");
return (unsigned long) -1;
}
while (--i >= 0) {
prev = x;
x = (x << PyLong_SHIFT) + v->ob_digit[i];
if ((x >> PyLong_SHIFT) != prev) {
PyErr_SetString(PyExc_OverflowError,
"long int too large to convert");
return (unsigned long) -1;
}
}
return x;
}
unsigned long
PyLong_AsUnsignedLongMask(PyObject *vv) {
register PyLongObject *v;
unsigned long x;
Py_ssize_t i;
int sign;
if (vv == NULL || !PyLong_Check(vv)) {
if (vv != NULL && PyInt_Check(vv))
return PyInt_AsUnsignedLongMask(vv);
PyErr_BadInternalCall();
return (unsigned long) -1;
}
v = (PyLongObject *)vv;
i = v->ob_size;
sign = 1;
x = 0;
if (i < 0) {
sign = -1;
i = -i;
}
while (--i >= 0) {
x = (x << PyLong_SHIFT) + v->ob_digit[i];
}
return x * sign;
}
int
_PyLong_Sign(PyObject *vv) {
PyLongObject *v = (PyLongObject *)vv;
assert(v != NULL);
assert(PyLong_Check(v));
return Py_SIZE(v) == 0 ? 0 : (Py_SIZE(v) < 0 ? -1 : 1);
}
size_t
_PyLong_NumBits(PyObject *vv) {
PyLongObject *v = (PyLongObject *)vv;
size_t result = 0;
Py_ssize_t ndigits;
assert(v != NULL);
assert(PyLong_Check(v));
ndigits = ABS(Py_SIZE(v));
assert(ndigits == 0 || v->ob_digit[ndigits - 1] != 0);
if (ndigits > 0) {
digit msd = v->ob_digit[ndigits - 1];
result = (ndigits - 1) * PyLong_SHIFT;
if (result / PyLong_SHIFT != (size_t)(ndigits - 1))
goto Overflow;
do {
++result;
if (result == 0)
goto Overflow;
msd >>= 1;
} while (msd);
}
return result;
Overflow:
PyErr_SetString(PyExc_OverflowError, "long has too many bits "
"to express in a platform size_t");
return (size_t)-1;
}
PyObject *
_PyLong_FromByteArray(const unsigned char* bytes, size_t n,
int little_endian, int is_signed) {
const unsigned char* pstartbyte;
int incr;
const unsigned char* pendbyte;
size_t numsignificantbytes;
size_t ndigits;
PyLongObject* v;
int idigit = 0;
if (n == 0)
return PyLong_FromLong(0L);
if (little_endian) {
pstartbyte = bytes;
pendbyte = bytes + n - 1;
incr = 1;
} else {
pstartbyte = bytes + n - 1;
pendbyte = bytes;
incr = -1;
}
if (is_signed)
is_signed = *pendbyte >= 0x80;
{
size_t i;
const unsigned char* p = pendbyte;
const int pincr = -incr;
const unsigned char insignficant = is_signed ? 0xff : 0x00;
for (i = 0; i < n; ++i, p += pincr) {
if (*p != insignficant)
break;
}
numsignificantbytes = n - i;
if (is_signed && numsignificantbytes < n)
++numsignificantbytes;
}
ndigits = (numsignificantbytes * 8 + PyLong_SHIFT - 1) / PyLong_SHIFT;
if (ndigits > (size_t)INT_MAX)
return PyErr_NoMemory();
v = _PyLong_New((int)ndigits);
if (v == NULL)
return NULL;
{
size_t i;
twodigits carry = 1;
twodigits accum = 0;
unsigned int accumbits = 0;
const unsigned char* p = pstartbyte;
for (i = 0; i < numsignificantbytes; ++i, p += incr) {
twodigits thisbyte = *p;
if (is_signed) {
thisbyte = (0xff ^ thisbyte) + carry;
carry = thisbyte >> 8;
thisbyte &= 0xff;
}
accum |= thisbyte << accumbits;
accumbits += 8;
if (accumbits >= PyLong_SHIFT) {
assert(idigit < (int)ndigits);
v->ob_digit[idigit] = (digit)(accum & PyLong_MASK);
++idigit;
accum >>= PyLong_SHIFT;
accumbits -= PyLong_SHIFT;
assert(accumbits < PyLong_SHIFT);
}
}
assert(accumbits < PyLong_SHIFT);
if (accumbits) {
assert(idigit < (int)ndigits);
v->ob_digit[idigit] = (digit)accum;
++idigit;
}
}
Py_SIZE(v) = is_signed ? -idigit : idigit;
return (PyObject *)long_normalize(v);
}
int
_PyLong_AsByteArray(PyLongObject* v,
unsigned char* bytes, size_t n,
int little_endian, int is_signed) {
int i;
Py_ssize_t ndigits;
twodigits accum;
unsigned int accumbits;
int do_twos_comp;
twodigits carry;
size_t j;
unsigned char* p;
int pincr;
assert(v != NULL && PyLong_Check(v));
if (Py_SIZE(v) < 0) {
ndigits = -(Py_SIZE(v));
if (!is_signed) {
PyErr_SetString(PyExc_TypeError,
"can't convert negative long to unsigned");
return -1;
}
do_twos_comp = 1;
} else {
ndigits = Py_SIZE(v);
do_twos_comp = 0;
}
if (little_endian) {
p = bytes;
pincr = 1;
} else {
p = bytes + n - 1;
pincr = -1;
}
assert(ndigits == 0 || v->ob_digit[ndigits - 1] != 0);
j = 0;
accum = 0;
accumbits = 0;
carry = do_twos_comp ? 1 : 0;
for (i = 0; i < ndigits; ++i) {
twodigits thisdigit = v->ob_digit[i];
if (do_twos_comp) {
thisdigit = (thisdigit ^ PyLong_MASK) + carry;
carry = thisdigit >> PyLong_SHIFT;
thisdigit &= PyLong_MASK;
}
accum |= thisdigit << accumbits;
accumbits += PyLong_SHIFT;
if (i == ndigits - 1) {
stwodigits s = (stwodigits)(thisdigit <<
(8*sizeof(stwodigits) - PyLong_SHIFT));
unsigned int nsignbits = 0;
while ((s < 0) == do_twos_comp && nsignbits < PyLong_SHIFT) {
++nsignbits;
s <<= 1;
}
accumbits -= nsignbits;
}
while (accumbits >= 8) {
if (j >= n)
goto Overflow;
++j;
*p = (unsigned char)(accum & 0xff);
p += pincr;
accumbits -= 8;
accum >>= 8;
}
}
assert(accumbits < 8);
assert(carry == 0);
if (accumbits > 0) {
if (j >= n)
goto Overflow;
++j;
if (do_twos_comp) {
accum |= (~(twodigits)0) << accumbits;
}
*p = (unsigned char)(accum & 0xff);
p += pincr;
} else if (j == n && n > 0 && is_signed) {
unsigned char msb = *(p - pincr);
int sign_bit_set = msb >= 0x80;
assert(accumbits == 0);
if (sign_bit_set == do_twos_comp)
return 0;
else
goto Overflow;
}
{
unsigned char signbyte = do_twos_comp ? 0xffU : 0U;
for ( ; j < n; ++j, p += pincr)
*p = signbyte;
}
return 0;
Overflow:
PyErr_SetString(PyExc_OverflowError, "long too big to convert");
return -1;
}
double
_PyLong_AsScaledDouble(PyObject *vv, int *exponent) {
#define NBITS_WANTED 57
PyLongObject *v;
double x;
const double multiplier = (double)(1L << PyLong_SHIFT);
Py_ssize_t i;
int sign;
int nbitsneeded;
if (vv == NULL || !PyLong_Check(vv)) {
PyErr_BadInternalCall();
return -1;
}
v = (PyLongObject *)vv;
i = Py_SIZE(v);
sign = 1;
if (i < 0) {
sign = -1;
i = -(i);
} else if (i == 0) {
*exponent = 0;
return 0.0;
}
--i;
x = (double)v->ob_digit[i];
nbitsneeded = NBITS_WANTED - 1;
while (i > 0 && nbitsneeded > 0) {
--i;
x = x * multiplier + (double)v->ob_digit[i];
nbitsneeded -= PyLong_SHIFT;
}
*exponent = i;
assert(x > 0.0);
return x * sign;
#undef NBITS_WANTED
}
double
PyLong_AsDouble(PyObject *vv) {
int e = -1;
double x;
if (vv == NULL || !PyLong_Check(vv)) {
PyErr_BadInternalCall();
return -1;
}
x = _PyLong_AsScaledDouble(vv, &e);
if (x == -1.0 && PyErr_Occurred())
return -1.0;
assert(e >= 0);
if (e > INT_MAX / PyLong_SHIFT)
goto overflow;
errno = 0;
x = ldexp(x, e * PyLong_SHIFT);
if (Py_OVERFLOWED(x))
goto overflow;
return x;
overflow:
PyErr_SetString(PyExc_OverflowError,
"long int too large to convert to float");
return -1.0;
}
PyObject *
PyLong_FromVoidPtr(void *p) {
#if SIZEOF_VOID_P <= SIZEOF_LONG
if ((long)p < 0)
return PyLong_FromUnsignedLong((unsigned long)p);
return PyInt_FromLong((long)p);
#else
#if !defined(HAVE_LONG_LONG)
#error "PyLong_FromVoidPtr: sizeof(void*) > sizeof(long), but no long long"
#endif
#if SIZEOF_LONG_LONG < SIZEOF_VOID_P
#error "PyLong_FromVoidPtr: sizeof(PY_LONG_LONG) < sizeof(void*)"
#endif
if (p == NULL)
return PyInt_FromLong(0);
return PyLong_FromUnsignedLongLong((unsigned PY_LONG_LONG)p);
#endif
}
void *
PyLong_AsVoidPtr(PyObject *vv) {
#if SIZEOF_VOID_P <= SIZEOF_LONG
long x;
if (PyInt_Check(vv))
x = PyInt_AS_LONG(vv);
else if (PyLong_Check(vv) && _PyLong_Sign(vv) < 0)
x = PyLong_AsLong(vv);
else
x = PyLong_AsUnsignedLong(vv);
#else
#if !defined(HAVE_LONG_LONG)
#error "PyLong_AsVoidPtr: sizeof(void*) > sizeof(long), but no long long"
#endif
#if SIZEOF_LONG_LONG < SIZEOF_VOID_P
#error "PyLong_AsVoidPtr: sizeof(PY_LONG_LONG) < sizeof(void*)"
#endif
PY_LONG_LONG x;
if (PyInt_Check(vv))
x = PyInt_AS_LONG(vv);
else if (PyLong_Check(vv) && _PyLong_Sign(vv) < 0)
x = PyLong_AsLongLong(vv);
else
x = PyLong_AsUnsignedLongLong(vv);
#endif
if (x == -1 && PyErr_Occurred())
return NULL;
return (void *)x;
}
#if defined(HAVE_LONG_LONG)
#define IS_LITTLE_ENDIAN (int)*(unsigned char*)&one
PyObject *
PyLong_FromLongLong(PY_LONG_LONG ival) {
PyLongObject *v;
unsigned PY_LONG_LONG abs_ival;
unsigned PY_LONG_LONG t;
int ndigits = 0;
int negative = 0;
if (ival < 0) {
abs_ival = (unsigned PY_LONG_LONG)(-1-ival) + 1;
negative = 1;
} else {
abs_ival = (unsigned PY_LONG_LONG)ival;
}
t = abs_ival;
while (t) {
++ndigits;
t >>= PyLong_SHIFT;
}
v = _PyLong_New(ndigits);
if (v != NULL) {
digit *p = v->ob_digit;
Py_SIZE(v) = negative ? -ndigits : ndigits;
t = abs_ival;
while (t) {
*p++ = (digit)(t & PyLong_MASK);
t >>= PyLong_SHIFT;
}
}
return (PyObject *)v;
}
PyObject *
PyLong_FromUnsignedLongLong(unsigned PY_LONG_LONG ival) {
PyLongObject *v;
unsigned PY_LONG_LONG t;
int ndigits = 0;
t = (unsigned PY_LONG_LONG)ival;
while (t) {
++ndigits;
t >>= PyLong_SHIFT;
}
v = _PyLong_New(ndigits);
if (v != NULL) {
digit *p = v->ob_digit;
Py_SIZE(v) = ndigits;
while (ival) {
*p++ = (digit)(ival & PyLong_MASK);
ival >>= PyLong_SHIFT;
}
}
return (PyObject *)v;
}
PyObject *
PyLong_FromSsize_t(Py_ssize_t ival) {
Py_ssize_t bytes = ival;
int one = 1;
return _PyLong_FromByteArray(
(unsigned char *)&bytes,
SIZEOF_SIZE_T, IS_LITTLE_ENDIAN, 1);
}
PyObject *
PyLong_FromSize_t(size_t ival) {
size_t bytes = ival;
int one = 1;
return _PyLong_FromByteArray(
(unsigned char *)&bytes,
SIZEOF_SIZE_T, IS_LITTLE_ENDIAN, 0);
}
PY_LONG_LONG
PyLong_AsLongLong(PyObject *vv) {
PY_LONG_LONG bytes;
int one = 1;
int res;
if (vv == NULL) {
PyErr_BadInternalCall();
return -1;
}
if (!PyLong_Check(vv)) {
PyNumberMethods *nb;
PyObject *io;
if (PyInt_Check(vv))
return (PY_LONG_LONG)PyInt_AsLong(vv);
if ((nb = vv->ob_type->tp_as_number) == NULL ||
nb->nb_int == NULL) {
PyErr_SetString(PyExc_TypeError, "an integer is required");
return -1;
}
io = (*nb->nb_int) (vv);
if (io == NULL)
return -1;
if (PyInt_Check(io)) {
bytes = PyInt_AsLong(io);
Py_DECREF(io);
return bytes;
}
if (PyLong_Check(io)) {
bytes = PyLong_AsLongLong(io);
Py_DECREF(io);
return bytes;
}
Py_DECREF(io);
PyErr_SetString(PyExc_TypeError, "integer conversion failed");
return -1;
}
res = _PyLong_AsByteArray(
(PyLongObject *)vv, (unsigned char *)&bytes,
SIZEOF_LONG_LONG, IS_LITTLE_ENDIAN, 1);
if (res < 0)
return (PY_LONG_LONG)-1;
else
return bytes;
}
unsigned PY_LONG_LONG
PyLong_AsUnsignedLongLong(PyObject *vv) {
unsigned PY_LONG_LONG bytes;
int one = 1;
int res;
if (vv == NULL || !PyLong_Check(vv)) {
PyErr_BadInternalCall();
return (unsigned PY_LONG_LONG)-1;
}
res = _PyLong_AsByteArray(
(PyLongObject *)vv, (unsigned char *)&bytes,
SIZEOF_LONG_LONG, IS_LITTLE_ENDIAN, 0);
if (res < 0)
return (unsigned PY_LONG_LONG)res;
else
return bytes;
}
unsigned PY_LONG_LONG
PyLong_AsUnsignedLongLongMask(PyObject *vv) {
register PyLongObject *v;
unsigned PY_LONG_LONG x;
Py_ssize_t i;
int sign;
if (vv == NULL || !PyLong_Check(vv)) {
PyErr_BadInternalCall();
return (unsigned long) -1;
}
v = (PyLongObject *)vv;
i = v->ob_size;
sign = 1;
x = 0;
if (i < 0) {
sign = -1;
i = -i;
}
while (--i >= 0) {
x = (x << PyLong_SHIFT) + v->ob_digit[i];
}
return x * sign;
}
#undef IS_LITTLE_ENDIAN
#endif
static int
convert_binop(PyObject *v, PyObject *w, PyLongObject **a, PyLongObject **b) {
if (PyLong_Check(v)) {
*a = (PyLongObject *) v;
Py_INCREF(v);
} else if (PyInt_Check(v)) {
*a = (PyLongObject *) PyLong_FromLong(PyInt_AS_LONG(v));
} else {
return 0;
}
if (PyLong_Check(w)) {
*b = (PyLongObject *) w;
Py_INCREF(w);
} else if (PyInt_Check(w)) {
*b = (PyLongObject *) PyLong_FromLong(PyInt_AS_LONG(w));
} else {
Py_DECREF(*a);
return 0;
}
return 1;
}
#define CONVERT_BINOP(v, w, a, b) if (!convert_binop(v, w, a, b)) { Py_INCREF(Py_NotImplemented); return Py_NotImplemented; }
static digit
v_iadd(digit *x, Py_ssize_t m, digit *y, Py_ssize_t n) {
int i;
digit carry = 0;
assert(m >= n);
for (i = 0; i < n; ++i) {
carry += x[i] + y[i];
x[i] = carry & PyLong_MASK;
carry >>= PyLong_SHIFT;
assert((carry & 1) == carry);
}
for (; carry && i < m; ++i) {
carry += x[i];
x[i] = carry & PyLong_MASK;
carry >>= PyLong_SHIFT;
assert((carry & 1) == carry);
}
return carry;
}
static digit
v_isub(digit *x, Py_ssize_t m, digit *y, Py_ssize_t n) {
int i;
digit borrow = 0;
assert(m >= n);
for (i = 0; i < n; ++i) {
borrow = x[i] - y[i] - borrow;
x[i] = borrow & PyLong_MASK;
borrow >>= PyLong_SHIFT;
borrow &= 1;
}
for (; borrow && i < m; ++i) {
borrow = x[i] - borrow;
x[i] = borrow & PyLong_MASK;
borrow >>= PyLong_SHIFT;
borrow &= 1;
}
return borrow;
}
static PyLongObject *
mul1(PyLongObject *a, wdigit n) {
return muladd1(a, n, (digit)0);
}
static PyLongObject *
muladd1(PyLongObject *a, wdigit n, wdigit extra) {
Py_ssize_t size_a = ABS(Py_SIZE(a));
PyLongObject *z = _PyLong_New(size_a+1);
twodigits carry = extra;
Py_ssize_t i;
if (z == NULL)
return NULL;
for (i = 0; i < size_a; ++i) {
carry += (twodigits)a->ob_digit[i] * n;
z->ob_digit[i] = (digit) (carry & PyLong_MASK);
carry >>= PyLong_SHIFT;
}
z->ob_digit[i] = (digit) carry;
return long_normalize(z);
}
static digit
inplace_divrem1(digit *pout, digit *pin, Py_ssize_t size, digit n) {
twodigits rem = 0;
assert(n > 0 && n <= PyLong_MASK);
pin += size;
pout += size;
while (--size >= 0) {
digit hi;
rem = (rem << PyLong_SHIFT) + *--pin;
*--pout = hi = (digit)(rem / n);
rem -= hi * n;
}
return (digit)rem;
}
static PyLongObject *
divrem1(PyLongObject *a, digit n, digit *prem) {
const Py_ssize_t size = ABS(Py_SIZE(a));
PyLongObject *z;
assert(n > 0 && n <= PyLong_MASK);
z = _PyLong_New(size);
if (z == NULL)
return NULL;
*prem = inplace_divrem1(z->ob_digit, a->ob_digit, size, n);
return long_normalize(z);
}
PyAPI_FUNC(PyObject *)
_PyLong_Format(PyObject *aa, int base, int addL, int newstyle) {
register PyLongObject *a = (PyLongObject *)aa;
PyStringObject *str;
Py_ssize_t i, j, sz;
Py_ssize_t size_a;
char *p;
int bits;
char sign = '\0';
if (a == NULL || !PyLong_Check(a)) {
PyErr_BadInternalCall();
return NULL;
}
assert(base >= 2 && base <= 36);
size_a = ABS(Py_SIZE(a));
i = base;
bits = 0;
while (i > 1) {
++bits;
i >>= 1;
}
i = 5 + (addL ? 1 : 0);
j = size_a*PyLong_SHIFT + bits-1;
sz = i + j / bits;
if (j / PyLong_SHIFT < size_a || sz < i) {
PyErr_SetString(PyExc_OverflowError,
"long is too large to format");
return NULL;
}
str = (PyStringObject *) PyString_FromStringAndSize((char *)0, sz);
if (str == NULL)
return NULL;
p = PyString_AS_STRING(str) + sz;
*p = '\0';
if (addL)
*--p = 'L';
if (a->ob_size < 0)
sign = '-';
if (a->ob_size == 0) {
*--p = '0';
} else if ((base & (base - 1)) == 0) {
twodigits accum = 0;
int accumbits = 0;
int basebits = 1;
i = base;
while ((i >>= 1) > 1)
++basebits;
for (i = 0; i < size_a; ++i) {
accum |= (twodigits)a->ob_digit[i] << accumbits;
accumbits += PyLong_SHIFT;
assert(accumbits >= basebits);
do {
char cdigit = (char)(accum & (base - 1));
cdigit += (cdigit < 10) ? '0' : 'a'-10;
assert(p > PyString_AS_STRING(str));
*--p = cdigit;
accumbits -= basebits;
accum >>= basebits;
} while (i < size_a-1 ? accumbits >= basebits :
accum > 0);
}
} else {
Py_ssize_t size = size_a;
digit *pin = a->ob_digit;
PyLongObject *scratch;
digit powbase = base;
int power = 1;
for (;;) {
unsigned long newpow = powbase * (unsigned long)base;
if (newpow >> PyLong_SHIFT)
break;
powbase = (digit)newpow;
++power;
}
scratch = _PyLong_New(size);
if (scratch == NULL) {
Py_DECREF(str);
return NULL;
}
do {
int ntostore = power;
digit rem = inplace_divrem1(scratch->ob_digit,
pin, size, powbase);
pin = scratch->ob_digit;
if (pin[size - 1] == 0)
--size;
SIGCHECK({
Py_DECREF(scratch);
Py_DECREF(str);
return NULL;
})
assert(ntostore > 0);
do {
digit nextrem = (digit)(rem / base);
char c = (char)(rem - nextrem * base);
assert(p > PyString_AS_STRING(str));
c += (c < 10) ? '0' : 'a'-10;
*--p = c;
rem = nextrem;
--ntostore;
} while (ntostore && (size || rem));
} while (size != 0);
Py_DECREF(scratch);
}
if (base == 2) {
*--p = 'b';
*--p = '0';
} else if (base == 8) {
if (newstyle) {
*--p = 'o';
*--p = '0';
} else if (size_a != 0)
*--p = '0';
} else if (base == 16) {
*--p = 'x';
*--p = '0';
} else if (base != 10) {
*--p = '#';
*--p = '0' + base%10;
if (base > 10)
*--p = '0' + base/10;
}
if (sign)
*--p = sign;
if (p != PyString_AS_STRING(str)) {
char *q = PyString_AS_STRING(str);
assert(p > q);
do {
} while ((*q++ = *p++) != '\0');
q--;
_PyString_Resize((PyObject **)&str,
(Py_ssize_t) (q - PyString_AS_STRING(str)));
}
return (PyObject *)str;
}
int _PyLong_DigitValue[256] = {
37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 37, 37, 37, 37, 37, 37,
37, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 37, 37, 37, 37, 37,
37, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 37, 37, 37, 37, 37,
37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
};
static PyLongObject *
long_from_binary_base(char **str, int base) {
char *p = *str;
char *start = p;
int bits_per_char;
Py_ssize_t n;
PyLongObject *z;
twodigits accum;
int bits_in_accum;
digit *pdigit;
assert(base >= 2 && base <= 32 && (base & (base - 1)) == 0);
n = base;
for (bits_per_char = -1; n; ++bits_per_char)
n >>= 1;
n = 0;
while (_PyLong_DigitValue[Py_CHARMASK(*p)] < base)
++p;
*str = p;
n = (p - start) * bits_per_char + PyLong_SHIFT - 1;
if (n / bits_per_char < p - start) {
PyErr_SetString(PyExc_ValueError,
"long string too large to convert");
return NULL;
}
n = n / PyLong_SHIFT;
z = _PyLong_New(n);
if (z == NULL)
return NULL;
accum = 0;
bits_in_accum = 0;
pdigit = z->ob_digit;
while (--p >= start) {
int k = _PyLong_DigitValue[Py_CHARMASK(*p)];
assert(k >= 0 && k < base);
accum |= (twodigits)(k << bits_in_accum);
bits_in_accum += bits_per_char;
if (bits_in_accum >= PyLong_SHIFT) {
*pdigit++ = (digit)(accum & PyLong_MASK);
assert(pdigit - z->ob_digit <= (int)n);
accum >>= PyLong_SHIFT;
bits_in_accum -= PyLong_SHIFT;
assert(bits_in_accum < PyLong_SHIFT);
}
}
if (bits_in_accum) {
assert(bits_in_accum <= PyLong_SHIFT);
*pdigit++ = (digit)accum;
assert(pdigit - z->ob_digit <= (int)n);
}
while (pdigit - z->ob_digit < n)
*pdigit++ = 0;
return long_normalize(z);
}
PyObject *
PyLong_FromString(char *str, char **pend, int base) {
int sign = 1;
char *start, *orig_str = str;
PyLongObject *z;
PyObject *strobj, *strrepr;
Py_ssize_t slen;
if ((base != 0 && base < 2) || base > 36) {
PyErr_SetString(PyExc_ValueError,
"long() arg 2 must be >= 2 and <= 36");
return NULL;
}
while (*str != '\0' && isspace(Py_CHARMASK(*str)))
str++;
if (*str == '+')
++str;
else if (*str == '-') {
++str;
sign = -1;
}
while (*str != '\0' && isspace(Py_CHARMASK(*str)))
str++;
if (base == 0) {
if (str[0] != '0')
base = 10;
else if (str[1] == 'x' || str[1] == 'X')
base = 16;
else if (str[1] == 'o' || str[1] == 'O')
base = 8;
else if (str[1] == 'b' || str[1] == 'B')
base = 2;
else
base = 8;
}
if (str[0] == '0' &&
((base == 16 && (str[1] == 'x' || str[1] == 'X')) ||
(base == 8 && (str[1] == 'o' || str[1] == 'O')) ||
(base == 2 && (str[1] == 'b' || str[1] == 'B'))))
str += 2;
start = str;
if ((base & (base - 1)) == 0)
z = long_from_binary_base(&str, base);
else {
register twodigits c;
Py_ssize_t size_z;
int i;
int convwidth;
twodigits convmultmax, convmult;
digit *pz, *pzstop;
char* scan;
static double log_base_PyLong_BASE[37] = {0.0e0,};
static int convwidth_base[37] = {0,};
static twodigits convmultmax_base[37] = {0,};
if (log_base_PyLong_BASE[base] == 0.0) {
twodigits convmax = base;
int i = 1;
log_base_PyLong_BASE[base] = log((double)base) /
log((double)PyLong_BASE);
for (;;) {
twodigits next = convmax * base;
if (next > PyLong_BASE)
break;
convmax = next;
++i;
}
convmultmax_base[base] = convmax;
assert(i > 0);
convwidth_base[base] = i;
}
scan = str;
while (_PyLong_DigitValue[Py_CHARMASK(*scan)] < base)
++scan;
size_z = (Py_ssize_t)((scan - str) * log_base_PyLong_BASE[base]) + 1;
assert(size_z > 0);
z = _PyLong_New(size_z);
if (z == NULL)
return NULL;
Py_SIZE(z) = 0;
convwidth = convwidth_base[base];
convmultmax = convmultmax_base[base];
while (str < scan) {
c = (digit)_PyLong_DigitValue[Py_CHARMASK(*str++)];
for (i = 1; i < convwidth && str != scan; ++i, ++str) {
c = (twodigits)(c * base +
_PyLong_DigitValue[Py_CHARMASK(*str)]);
assert(c < PyLong_BASE);
}
convmult = convmultmax;
if (i != convwidth) {
convmult = base;
for ( ; i > 1; --i)
convmult *= base;
}
pz = z->ob_digit;
pzstop = pz + Py_SIZE(z);
for (; pz < pzstop; ++pz) {
c += (twodigits)*pz * convmult;
*pz = (digit)(c & PyLong_MASK);
c >>= PyLong_SHIFT;
}
if (c) {
assert(c < PyLong_BASE);
if (Py_SIZE(z) < size_z) {
*pz = (digit)c;
++Py_SIZE(z);
} else {
PyLongObject *tmp;
assert(Py_SIZE(z) == size_z);
tmp = _PyLong_New(size_z + 1);
if (tmp == NULL) {
Py_DECREF(z);
return NULL;
}
memcpy(tmp->ob_digit,
z->ob_digit,
sizeof(digit) * size_z);
Py_DECREF(z);
z = tmp;
z->ob_digit[size_z] = (digit)c;
++size_z;
}
}
}
}
if (z == NULL)
return NULL;
if (str == start)
goto onError;
if (sign < 0)
Py_SIZE(z) = -(Py_SIZE(z));
if (*str == 'L' || *str == 'l')
str++;
while (*str && isspace(Py_CHARMASK(*str)))
str++;
if (*str != '\0')
goto onError;
if (pend)
*pend = str;
return (PyObject *) z;
onError:
Py_XDECREF(z);
slen = strlen(orig_str) < 200 ? strlen(orig_str) : 200;
strobj = PyString_FromStringAndSize(orig_str, slen);
if (strobj == NULL)
return NULL;
strrepr = PyObject_Repr(strobj);
Py_DECREF(strobj);
if (strrepr == NULL)
return NULL;
PyErr_Format(PyExc_ValueError,
"invalid literal for long() with base %d: %s",
base, PyString_AS_STRING(strrepr));
Py_DECREF(strrepr);
return NULL;
}
#if defined(Py_USING_UNICODE)
PyObject *
PyLong_FromUnicode(Py_UNICODE *u, Py_ssize_t length, int base) {
PyObject *result;
char *buffer = (char *)PyMem_MALLOC(length+1);
if (buffer == NULL)
return NULL;
if (PyUnicode_EncodeDecimal(u, length, buffer, NULL)) {
PyMem_FREE(buffer);
return NULL;
}
result = PyLong_FromString(buffer, NULL, base);
PyMem_FREE(buffer);
return result;
}
#endif
static PyLongObject *x_divrem
(PyLongObject *, PyLongObject *, PyLongObject **);
static PyObject *long_long(PyObject *v);
static int long_divrem(PyLongObject *, PyLongObject *,
PyLongObject **, PyLongObject **);
static int
long_divrem(PyLongObject *a, PyLongObject *b,
PyLongObject **pdiv, PyLongObject **prem) {
Py_ssize_t size_a = ABS(Py_SIZE(a)), size_b = ABS(Py_SIZE(b));
PyLongObject *z;
if (size_b == 0) {
PyErr_SetString(PyExc_ZeroDivisionError,
"long division or modulo by zero");
return -1;
}
if (size_a < size_b ||
(size_a == size_b &&
a->ob_digit[size_a-1] < b->ob_digit[size_b-1])) {
*pdiv = _PyLong_New(0);
if (*pdiv == NULL)
return -1;
Py_INCREF(a);
*prem = (PyLongObject *) a;
return 0;
}
if (size_b == 1) {
digit rem = 0;
z = divrem1(a, b->ob_digit[0], &rem);
if (z == NULL)
return -1;
*prem = (PyLongObject *) PyLong_FromLong((long)rem);
if (*prem == NULL) {
Py_DECREF(z);
return -1;
}
} else {
z = x_divrem(a, b, prem);
if (z == NULL)
return -1;
}
if ((a->ob_size < 0) != (b->ob_size < 0))
z->ob_size = -(z->ob_size);
if (a->ob_size < 0 && (*prem)->ob_size != 0)
(*prem)->ob_size = -((*prem)->ob_size);
*pdiv = z;
return 0;
}
static PyLongObject *
x_divrem(PyLongObject *v1, PyLongObject *w1, PyLongObject **prem) {
Py_ssize_t size_v = ABS(Py_SIZE(v1)), size_w = ABS(Py_SIZE(w1));
digit d = (digit) ((twodigits)PyLong_BASE / (w1->ob_digit[size_w-1] + 1));
PyLongObject *v = mul1(v1, d);
PyLongObject *w = mul1(w1, d);
PyLongObject *a;
Py_ssize_t j, k;
if (v == NULL || w == NULL) {
Py_XDECREF(v);
Py_XDECREF(w);
return NULL;
}
assert(size_v >= size_w && size_w > 1);
assert(Py_REFCNT(v) == 1);
assert(size_w == ABS(Py_SIZE(w)));
size_v = ABS(Py_SIZE(v));
k = size_v - size_w;
a = _PyLong_New(k + 1);
for (j = size_v; a != NULL && k >= 0; --j, --k) {
digit vj = (j >= size_v) ? 0 : v->ob_digit[j];
twodigits q;
stwodigits carry = 0;
int i;
SIGCHECK({
Py_DECREF(a);
a = NULL;
break;
})
if (vj == w->ob_digit[size_w-1])
q = PyLong_MASK;
else
q = (((twodigits)vj << PyLong_SHIFT) + v->ob_digit[j-1]) /
w->ob_digit[size_w-1];
while (w->ob_digit[size_w-2]*q >
((
((twodigits)vj << PyLong_SHIFT)
+ v->ob_digit[j-1]
- q*w->ob_digit[size_w-1]
) << PyLong_SHIFT)
+ v->ob_digit[j-2])
--q;
for (i = 0; i < size_w && i+k < size_v; ++i) {
twodigits z = w->ob_digit[i] * q;
digit zz = (digit) (z >> PyLong_SHIFT);
carry += v->ob_digit[i+k] - z
+ ((twodigits)zz << PyLong_SHIFT);
v->ob_digit[i+k] = (digit)(carry & PyLong_MASK);
carry = Py_ARITHMETIC_RIGHT_SHIFT(PyLong_BASE_TWODIGITS_TYPE,
carry, PyLong_SHIFT);
carry -= zz;
}
if (i+k < size_v) {
carry += v->ob_digit[i+k];
v->ob_digit[i+k] = 0;
}
if (carry == 0)
a->ob_digit[k] = (digit) q;
else {
assert(carry == -1);
a->ob_digit[k] = (digit) q-1;
carry = 0;
for (i = 0; i < size_w && i+k < size_v; ++i) {
carry += v->ob_digit[i+k] + w->ob_digit[i];
v->ob_digit[i+k] = (digit)(carry & PyLong_MASK);
carry = Py_ARITHMETIC_RIGHT_SHIFT(
PyLong_BASE_TWODIGITS_TYPE,
carry, PyLong_SHIFT);
}
}
}
if (a == NULL)
*prem = NULL;
else {
a = long_normalize(a);
*prem = divrem1(v, d, &d);
if (*prem == NULL) {
Py_DECREF(a);
a = NULL;
}
}
Py_DECREF(v);
Py_DECREF(w);
return a;
}
static void
long_dealloc(PyObject *v) {
Py_TYPE(v)->tp_free(v);
}
static PyObject *
long_repr(PyObject *v) {
return _PyLong_Format(v, 10, 1, 0);
}
static PyObject *
long_str(PyObject *v) {
return _PyLong_Format(v, 10, 0, 0);
}
static int
long_compare(PyLongObject *a, PyLongObject *b) {
Py_ssize_t sign;
if (Py_SIZE(a) != Py_SIZE(b)) {
if (ABS(Py_SIZE(a)) == 0 && ABS(Py_SIZE(b)) == 0)
sign = 0;
else
sign = Py_SIZE(a) - Py_SIZE(b);
} else {
Py_ssize_t i = ABS(Py_SIZE(a));
while (--i >= 0 && a->ob_digit[i] == b->ob_digit[i])
;
if (i < 0)
sign = 0;
else {
sign = (int)a->ob_digit[i] - (int)b->ob_digit[i];
if (Py_SIZE(a) < 0)
sign = -sign;
}
}
return sign < 0 ? -1 : sign > 0 ? 1 : 0;
}
static long
long_hash(PyLongObject *v) {
long x;
Py_ssize_t i;
int sign;
i = v->ob_size;
sign = 1;
x = 0;
if (i < 0) {
sign = -1;
i = -(i);
}
#define LONG_BIT_PyLong_SHIFT (8*sizeof(long) - PyLong_SHIFT)
while (--i >= 0) {
x = ((x << PyLong_SHIFT) & ~PyLong_MASK) | ((x >> LONG_BIT_PyLong_SHIFT) & PyLong_MASK);
x += v->ob_digit[i];
if ((unsigned long)x < v->ob_digit[i])
x++;
}
#undef LONG_BIT_PyLong_SHIFT
x = x * sign;
if (x == -1)
x = -2;
return x;
}
static PyLongObject *
x_add(PyLongObject *a, PyLongObject *b) {
Py_ssize_t size_a = ABS(Py_SIZE(a)), size_b = ABS(Py_SIZE(b));
PyLongObject *z;
int i;
digit carry = 0;
if (size_a < size_b) {
{
PyLongObject *temp = a;
a = b;
b = temp;
}
{
Py_ssize_t size_temp = size_a;
size_a = size_b;
size_b = size_temp;
}
}
z = _PyLong_New(size_a+1);
if (z == NULL)
return NULL;
for (i = 0; i < size_b; ++i) {
carry += a->ob_digit[i] + b->ob_digit[i];
z->ob_digit[i] = carry & PyLong_MASK;
carry >>= PyLong_SHIFT;
}
for (; i < size_a; ++i) {
carry += a->ob_digit[i];
z->ob_digit[i] = carry & PyLong_MASK;
carry >>= PyLong_SHIFT;
}
z->ob_digit[i] = carry;
return long_normalize(z);
}
static PyLongObject *
x_sub(PyLongObject *a, PyLongObject *b) {
Py_ssize_t size_a = ABS(Py_SIZE(a)), size_b = ABS(Py_SIZE(b));
PyLongObject *z;
Py_ssize_t i;
int sign = 1;
digit borrow = 0;
if (size_a < size_b) {
sign = -1;
{
PyLongObject *temp = a;
a = b;
b = temp;
}
{
Py_ssize_t size_temp = size_a;
size_a = size_b;
size_b = size_temp;
}
} else if (size_a == size_b) {
i = size_a;
while (--i >= 0 && a->ob_digit[i] == b->ob_digit[i])
;
if (i < 0)
return _PyLong_New(0);
if (a->ob_digit[i] < b->ob_digit[i]) {
sign = -1;
{
PyLongObject *temp = a;
a = b;
b = temp;
}
}
size_a = size_b = i+1;
}
z = _PyLong_New(size_a);
if (z == NULL)
return NULL;
for (i = 0; i < size_b; ++i) {
borrow = a->ob_digit[i] - b->ob_digit[i] - borrow;
z->ob_digit[i] = borrow & PyLong_MASK;
borrow >>= PyLong_SHIFT;
borrow &= 1;
}
for (; i < size_a; ++i) {
borrow = a->ob_digit[i] - borrow;
z->ob_digit[i] = borrow & PyLong_MASK;
borrow >>= PyLong_SHIFT;
borrow &= 1;
}
assert(borrow == 0);
if (sign < 0)
z->ob_size = -(z->ob_size);
return long_normalize(z);
}
static PyObject *
long_add(PyLongObject *v, PyLongObject *w) {
PyLongObject *a, *b, *z;
CONVERT_BINOP((PyObject *)v, (PyObject *)w, &a, &b);
if (a->ob_size < 0) {
if (b->ob_size < 0) {
z = x_add(a, b);
if (z != NULL && z->ob_size != 0)
z->ob_size = -(z->ob_size);
} else
z = x_sub(b, a);
} else {
if (b->ob_size < 0)
z = x_sub(a, b);
else
z = x_add(a, b);
}
Py_DECREF(a);
Py_DECREF(b);
return (PyObject *)z;
}
static PyObject *
long_sub(PyLongObject *v, PyLongObject *w) {
PyLongObject *a, *b, *z;
CONVERT_BINOP((PyObject *)v, (PyObject *)w, &a, &b);
if (a->ob_size < 0) {
if (b->ob_size < 0)
z = x_sub(a, b);
else
z = x_add(a, b);
if (z != NULL && z->ob_size != 0)
z->ob_size = -(z->ob_size);
} else {
if (b->ob_size < 0)
z = x_add(a, b);
else
z = x_sub(a, b);
}
Py_DECREF(a);
Py_DECREF(b);
return (PyObject *)z;
}
static PyLongObject *
x_mul(PyLongObject *a, PyLongObject *b) {
PyLongObject *z;
Py_ssize_t size_a = ABS(Py_SIZE(a));
Py_ssize_t size_b = ABS(Py_SIZE(b));
Py_ssize_t i;
z = _PyLong_New(size_a + size_b);
if (z == NULL)
return NULL;
memset(z->ob_digit, 0, Py_SIZE(z) * sizeof(digit));
if (a == b) {
for (i = 0; i < size_a; ++i) {
twodigits carry;
twodigits f = a->ob_digit[i];
digit *pz = z->ob_digit + (i << 1);
digit *pa = a->ob_digit + i + 1;
digit *paend = a->ob_digit + size_a;
SIGCHECK({
Py_DECREF(z);
return NULL;
})
carry = *pz + f * f;
*pz++ = (digit)(carry & PyLong_MASK);
carry >>= PyLong_SHIFT;
assert(carry <= PyLong_MASK);
f <<= 1;
while (pa < paend) {
carry += *pz + *pa++ * f;
*pz++ = (digit)(carry & PyLong_MASK);
carry >>= PyLong_SHIFT;
assert(carry <= (PyLong_MASK << 1));
}
if (carry) {
carry += *pz;
*pz++ = (digit)(carry & PyLong_MASK);
carry >>= PyLong_SHIFT;
}
if (carry)
*pz += (digit)(carry & PyLong_MASK);
assert((carry >> PyLong_SHIFT) == 0);
}
} else {
for (i = 0; i < size_a; ++i) {
twodigits carry = 0;
twodigits f = a->ob_digit[i];
digit *pz = z->ob_digit + i;
digit *pb = b->ob_digit;
digit *pbend = b->ob_digit + size_b;
SIGCHECK({
Py_DECREF(z);
return NULL;
})
while (pb < pbend) {
carry += *pz + *pb++ * f;
*pz++ = (digit)(carry & PyLong_MASK);
carry >>= PyLong_SHIFT;
assert(carry <= PyLong_MASK);
}
if (carry)
*pz += (digit)(carry & PyLong_MASK);
assert((carry >> PyLong_SHIFT) == 0);
}
}
return long_normalize(z);
}
static int
kmul_split(PyLongObject *n, Py_ssize_t size, PyLongObject **high, PyLongObject **low) {
PyLongObject *hi, *lo;
Py_ssize_t size_lo, size_hi;
const Py_ssize_t size_n = ABS(Py_SIZE(n));
size_lo = MIN(size_n, size);
size_hi = size_n - size_lo;
if ((hi = _PyLong_New(size_hi)) == NULL)
return -1;
if ((lo = _PyLong_New(size_lo)) == NULL) {
Py_DECREF(hi);
return -1;
}
memcpy(lo->ob_digit, n->ob_digit, size_lo * sizeof(digit));
memcpy(hi->ob_digit, n->ob_digit + size_lo, size_hi * sizeof(digit));
*high = long_normalize(hi);
*low = long_normalize(lo);
return 0;
}
static PyLongObject *k_lopsided_mul(PyLongObject *a, PyLongObject *b);
static PyLongObject *
k_mul(PyLongObject *a, PyLongObject *b) {
Py_ssize_t asize = ABS(Py_SIZE(a));
Py_ssize_t bsize = ABS(Py_SIZE(b));
PyLongObject *ah = NULL;
PyLongObject *al = NULL;
PyLongObject *bh = NULL;
PyLongObject *bl = NULL;
PyLongObject *ret = NULL;
PyLongObject *t1, *t2, *t3;
Py_ssize_t shift;
Py_ssize_t i;
if (asize > bsize) {
t1 = a;
a = b;
b = t1;
i = asize;
asize = bsize;
bsize = i;
}
i = a == b ? KARATSUBA_SQUARE_CUTOFF : KARATSUBA_CUTOFF;
if (asize <= i) {
if (asize == 0)
return _PyLong_New(0);
else
return x_mul(a, b);
}
if (2 * asize <= bsize)
return k_lopsided_mul(a, b);
shift = bsize >> 1;
if (kmul_split(a, shift, &ah, &al) < 0) goto fail;
assert(Py_SIZE(ah) > 0);
if (a == b) {
bh = ah;
bl = al;
Py_INCREF(bh);
Py_INCREF(bl);
} else if (kmul_split(b, shift, &bh, &bl) < 0) goto fail;
ret = _PyLong_New(asize + bsize);
if (ret == NULL) goto fail;
#if defined(Py_DEBUG)
memset(ret->ob_digit, 0xDF, Py_SIZE(ret) * sizeof(digit));
#endif
if ((t1 = k_mul(ah, bh)) == NULL) goto fail;
assert(Py_SIZE(t1) >= 0);
assert(2*shift + Py_SIZE(t1) <= Py_SIZE(ret));
memcpy(ret->ob_digit + 2*shift, t1->ob_digit,
Py_SIZE(t1) * sizeof(digit));
i = Py_SIZE(ret) - 2*shift - Py_SIZE(t1);
if (i)
memset(ret->ob_digit + 2*shift + Py_SIZE(t1), 0,
i * sizeof(digit));
if ((t2 = k_mul(al, bl)) == NULL) {
Py_DECREF(t1);
goto fail;
}
assert(Py_SIZE(t2) >= 0);
assert(Py_SIZE(t2) <= 2*shift);
memcpy(ret->ob_digit, t2->ob_digit, Py_SIZE(t2) * sizeof(digit));
i = 2*shift - Py_SIZE(t2);
if (i)
memset(ret->ob_digit + Py_SIZE(t2), 0, i * sizeof(digit));
i = Py_SIZE(ret) - shift;
(void)v_isub(ret->ob_digit + shift, i, t2->ob_digit, Py_SIZE(t2));
Py_DECREF(t2);
(void)v_isub(ret->ob_digit + shift, i, t1->ob_digit, Py_SIZE(t1));
Py_DECREF(t1);
if ((t1 = x_add(ah, al)) == NULL) goto fail;
Py_DECREF(ah);
Py_DECREF(al);
ah = al = NULL;
if (a == b) {
t2 = t1;
Py_INCREF(t2);
} else if ((t2 = x_add(bh, bl)) == NULL) {
Py_DECREF(t1);
goto fail;
}
Py_DECREF(bh);
Py_DECREF(bl);
bh = bl = NULL;
t3 = k_mul(t1, t2);
Py_DECREF(t1);
Py_DECREF(t2);
if (t3 == NULL) goto fail;
assert(Py_SIZE(t3) >= 0);
(void)v_iadd(ret->ob_digit + shift, i, t3->ob_digit, Py_SIZE(t3));
Py_DECREF(t3);
return long_normalize(ret);
fail:
Py_XDECREF(ret);
Py_XDECREF(ah);
Py_XDECREF(al);
Py_XDECREF(bh);
Py_XDECREF(bl);
return NULL;
}
static PyLongObject *
k_lopsided_mul(PyLongObject *a, PyLongObject *b) {
const Py_ssize_t asize = ABS(Py_SIZE(a));
Py_ssize_t bsize = ABS(Py_SIZE(b));
Py_ssize_t nbdone;
PyLongObject *ret;
PyLongObject *bslice = NULL;
assert(asize > KARATSUBA_CUTOFF);
assert(2 * asize <= bsize);
ret = _PyLong_New(asize + bsize);
if (ret == NULL)
return NULL;
memset(ret->ob_digit, 0, Py_SIZE(ret) * sizeof(digit));
bslice = _PyLong_New(asize);
if (bslice == NULL)
goto fail;
nbdone = 0;
while (bsize > 0) {
PyLongObject *product;
const Py_ssize_t nbtouse = MIN(bsize, asize);
memcpy(bslice->ob_digit, b->ob_digit + nbdone,
nbtouse * sizeof(digit));
Py_SIZE(bslice) = nbtouse;
product = k_mul(a, bslice);
if (product == NULL)
goto fail;
(void)v_iadd(ret->ob_digit + nbdone, Py_SIZE(ret) - nbdone,
product->ob_digit, Py_SIZE(product));
Py_DECREF(product);
bsize -= nbtouse;
nbdone += nbtouse;
}
Py_DECREF(bslice);
return long_normalize(ret);
fail:
Py_DECREF(ret);
Py_XDECREF(bslice);
return NULL;
}
static PyObject *
long_mul(PyLongObject *v, PyLongObject *w) {
PyLongObject *a, *b, *z;
if (!convert_binop((PyObject *)v, (PyObject *)w, &a, &b)) {
Py_INCREF(Py_NotImplemented);
return Py_NotImplemented;
}
z = k_mul(a, b);
if (((a->ob_size ^ b->ob_size) < 0) && z)
z->ob_size = -(z->ob_size);
Py_DECREF(a);
Py_DECREF(b);
return (PyObject *)z;
}
static int
l_divmod(PyLongObject *v, PyLongObject *w,
PyLongObject **pdiv, PyLongObject **pmod) {
PyLongObject *div, *mod;
if (long_divrem(v, w, &div, &mod) < 0)
return -1;
if ((Py_SIZE(mod) < 0 && Py_SIZE(w) > 0) ||
(Py_SIZE(mod) > 0 && Py_SIZE(w) < 0)) {
PyLongObject *temp;
PyLongObject *one;
temp = (PyLongObject *) long_add(mod, w);
Py_DECREF(mod);
mod = temp;
if (mod == NULL) {
Py_DECREF(div);
return -1;
}
one = (PyLongObject *) PyLong_FromLong(1L);
if (one == NULL ||
(temp = (PyLongObject *) long_sub(div, one)) == NULL) {
Py_DECREF(mod);
Py_DECREF(div);
Py_XDECREF(one);
return -1;
}
Py_DECREF(one);
Py_DECREF(div);
div = temp;
}
if (pdiv != NULL)
*pdiv = div;
else
Py_DECREF(div);
if (pmod != NULL)
*pmod = mod;
else
Py_DECREF(mod);
return 0;
}
static PyObject *
long_div(PyObject *v, PyObject *w) {
PyLongObject *a, *b, *div;
CONVERT_BINOP(v, w, &a, &b);
if (l_divmod(a, b, &div, NULL) < 0)
div = NULL;
Py_DECREF(a);
Py_DECREF(b);
return (PyObject *)div;
}
static PyObject *
long_classic_div(PyObject *v, PyObject *w) {
PyLongObject *a, *b, *div;
CONVERT_BINOP(v, w, &a, &b);
if (Py_DivisionWarningFlag &&
PyErr_Warn(PyExc_DeprecationWarning, "classic long division") < 0)
div = NULL;
else if (l_divmod(a, b, &div, NULL) < 0)
div = NULL;
Py_DECREF(a);
Py_DECREF(b);
return (PyObject *)div;
}
static PyObject *
long_true_divide(PyObject *v, PyObject *w) {
PyLongObject *a, *b;
double ad, bd;
int failed, aexp = -1, bexp = -1;
CONVERT_BINOP(v, w, &a, &b);
ad = _PyLong_AsScaledDouble((PyObject *)a, &aexp);
bd = _PyLong_AsScaledDouble((PyObject *)b, &bexp);
failed = (ad == -1.0 || bd == -1.0) && PyErr_Occurred();
Py_DECREF(a);
Py_DECREF(b);
if (failed)
return NULL;
assert(aexp >= 0 && bexp >= 0);
if (bd == 0.0) {
PyErr_SetString(PyExc_ZeroDivisionError,
"long division or modulo by zero");
return NULL;
}
ad /= bd;
aexp -= bexp;
if (aexp > INT_MAX / PyLong_SHIFT)
goto overflow;
else if (aexp < -(INT_MAX / PyLong_SHIFT))
return PyFloat_FromDouble(0.0);
errno = 0;
ad = ldexp(ad, aexp * PyLong_SHIFT);
if (Py_OVERFLOWED(ad))
goto overflow;
return PyFloat_FromDouble(ad);
overflow:
PyErr_SetString(PyExc_OverflowError,
"long/long too large for a float");
return NULL;
}
static PyObject *
long_mod(PyObject *v, PyObject *w) {
PyLongObject *a, *b, *mod;
CONVERT_BINOP(v, w, &a, &b);
if (l_divmod(a, b, NULL, &mod) < 0)
mod = NULL;
Py_DECREF(a);
Py_DECREF(b);
return (PyObject *)mod;
}
static PyObject *
long_divmod(PyObject *v, PyObject *w) {
PyLongObject *a, *b, *div, *mod;
PyObject *z;
CONVERT_BINOP(v, w, &a, &b);
if (l_divmod(a, b, &div, &mod) < 0) {
Py_DECREF(a);
Py_DECREF(b);
return NULL;
}
z = PyTuple_New(2);
if (z != NULL) {
PyTuple_SetItem(z, 0, (PyObject *) div);
PyTuple_SetItem(z, 1, (PyObject *) mod);
} else {
Py_DECREF(div);
Py_DECREF(mod);
}
Py_DECREF(a);
Py_DECREF(b);
return z;
}
static PyObject *
long_pow(PyObject *v, PyObject *w, PyObject *x) {
PyLongObject *a, *b, *c;
int negativeOutput = 0;
PyLongObject *z = NULL;
Py_ssize_t i, j, k;
PyLongObject *temp = NULL;
PyLongObject *table[32] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};
CONVERT_BINOP(v, w, &a, &b);
if (PyLong_Check(x)) {
c = (PyLongObject *)x;
Py_INCREF(x);
} else if (PyInt_Check(x)) {
c = (PyLongObject *)PyLong_FromLong(PyInt_AS_LONG(x));
if (c == NULL)
goto Error;
} else if (x == Py_None)
c = NULL;
else {
Py_DECREF(a);
Py_DECREF(b);
Py_INCREF(Py_NotImplemented);
return Py_NotImplemented;
}
if (Py_SIZE(b) < 0) {
if (c) {
PyErr_SetString(PyExc_TypeError, "pow() 2nd argument "
"cannot be negative when 3rd argument specified");
goto Error;
} else {
Py_DECREF(a);
Py_DECREF(b);
return PyFloat_Type.tp_as_number->nb_power(v, w, x);
}
}
if (c) {
if (Py_SIZE(c) == 0) {
PyErr_SetString(PyExc_ValueError,
"pow() 3rd argument cannot be 0");
goto Error;
}
if (Py_SIZE(c) < 0) {
negativeOutput = 1;
temp = (PyLongObject *)_PyLong_Copy(c);
if (temp == NULL)
goto Error;
Py_DECREF(c);
c = temp;
temp = NULL;
c->ob_size = - c->ob_size;
}
if ((Py_SIZE(c) == 1) && (c->ob_digit[0] == 1)) {
z = (PyLongObject *)PyLong_FromLong(0L);
goto Done;
}
if (Py_SIZE(a) < 0) {
if (l_divmod(a, c, NULL, &temp) < 0)
goto Error;
Py_DECREF(a);
a = temp;
temp = NULL;
}
}
z = (PyLongObject *)PyLong_FromLong(1L);
if (z == NULL)
goto Error;
#define REDUCE(X) if (c != NULL) { if (l_divmod(X, c, NULL, &temp) < 0) goto Error; Py_XDECREF(X); X = temp; temp = NULL; }
#define MULT(X, Y, result) { temp = (PyLongObject *)long_mul(X, Y); if (temp == NULL) goto Error; Py_XDECREF(result); result = temp; temp = NULL; REDUCE(result) }
if (Py_SIZE(b) <= FIVEARY_CUTOFF) {
for (i = Py_SIZE(b) - 1; i >= 0; --i) {
digit bi = b->ob_digit[i];
for (j = 1 << (PyLong_SHIFT-1); j != 0; j >>= 1) {
MULT(z, z, z)
if (bi & j)
MULT(z, a, z)
}
}
} else {
Py_INCREF(z);
table[0] = z;
for (i = 1; i < 32; ++i)
MULT(table[i-1], a, table[i])
for (i = Py_SIZE(b) - 1; i >= 0; --i) {
const digit bi = b->ob_digit[i];
for (j = PyLong_SHIFT - 5; j >= 0; j -= 5) {
const int index = (bi >> j) & 0x1f;
for (k = 0; k < 5; ++k)
MULT(z, z, z)
if (index)
MULT(z, table[index], z)
}
}
}
if (negativeOutput && (Py_SIZE(z) != 0)) {
temp = (PyLongObject *)long_sub(z, c);
if (temp == NULL)
goto Error;
Py_DECREF(z);
z = temp;
temp = NULL;
}
goto Done;
Error:
if (z != NULL) {
Py_DECREF(z);
z = NULL;
}
Done:
if (Py_SIZE(b) > FIVEARY_CUTOFF) {
for (i = 0; i < 32; ++i)
Py_XDECREF(table[i]);
}
Py_DECREF(a);
Py_DECREF(b);
Py_XDECREF(c);
Py_XDECREF(temp);
return (PyObject *)z;
}
static PyObject *
long_invert(PyLongObject *v) {
PyLongObject *x;
PyLongObject *w;
w = (PyLongObject *)PyLong_FromLong(1L);
if (w == NULL)
return NULL;
x = (PyLongObject *) long_add(v, w);
Py_DECREF(w);
if (x == NULL)
return NULL;
Py_SIZE(x) = -(Py_SIZE(x));
return (PyObject *)x;
}
static PyObject *
long_neg(PyLongObject *v) {
PyLongObject *z;
if (v->ob_size == 0 && PyLong_CheckExact(v)) {
Py_INCREF(v);
return (PyObject *) v;
}
z = (PyLongObject *)_PyLong_Copy(v);
if (z != NULL)
z->ob_size = -(v->ob_size);
return (PyObject *)z;
}
static PyObject *
long_abs(PyLongObject *v) {
if (v->ob_size < 0)
return long_neg(v);
else
return long_long((PyObject *)v);
}
static int
long_nonzero(PyLongObject *v) {
return ABS(Py_SIZE(v)) != 0;
}
static PyObject *
long_rshift(PyLongObject *v, PyLongObject *w) {
PyLongObject *a, *b;
PyLongObject *z = NULL;
long shiftby;
Py_ssize_t newsize, wordshift, loshift, hishift, i, j;
digit lomask, himask;
CONVERT_BINOP((PyObject *)v, (PyObject *)w, &a, &b);
if (Py_SIZE(a) < 0) {
PyLongObject *a1, *a2;
a1 = (PyLongObject *) long_invert(a);
if (a1 == NULL)
goto rshift_error;
a2 = (PyLongObject *) long_rshift(a1, b);
Py_DECREF(a1);
if (a2 == NULL)
goto rshift_error;
z = (PyLongObject *) long_invert(a2);
Py_DECREF(a2);
} else {
shiftby = PyLong_AsLong((PyObject *)b);
if (shiftby == -1L && PyErr_Occurred())
goto rshift_error;
if (shiftby < 0) {
PyErr_SetString(PyExc_ValueError,
"negative shift count");
goto rshift_error;
}
wordshift = shiftby / PyLong_SHIFT;
newsize = ABS(Py_SIZE(a)) - wordshift;
if (newsize <= 0) {
z = _PyLong_New(0);
Py_DECREF(a);
Py_DECREF(b);
return (PyObject *)z;
}
loshift = shiftby % PyLong_SHIFT;
hishift = PyLong_SHIFT - loshift;
lomask = ((digit)1 << hishift) - 1;
himask = PyLong_MASK ^ lomask;
z = _PyLong_New(newsize);
if (z == NULL)
goto rshift_error;
if (Py_SIZE(a) < 0)
Py_SIZE(z) = -(Py_SIZE(z));
for (i = 0, j = wordshift; i < newsize; i++, j++) {
z->ob_digit[i] = (a->ob_digit[j] >> loshift) & lomask;
if (i+1 < newsize)
z->ob_digit[i] |=
(a->ob_digit[j+1] << hishift) & himask;
}
z = long_normalize(z);
}
rshift_error:
Py_DECREF(a);
Py_DECREF(b);
return (PyObject *) z;
}
static PyObject *
long_lshift(PyObject *v, PyObject *w) {
PyLongObject *a, *b;
PyLongObject *z = NULL;
long shiftby;
Py_ssize_t oldsize, newsize, wordshift, remshift, i, j;
twodigits accum;
CONVERT_BINOP(v, w, &a, &b);
shiftby = PyLong_AsLong((PyObject *)b);
if (shiftby == -1L && PyErr_Occurred())
goto lshift_error;
if (shiftby < 0) {
PyErr_SetString(PyExc_ValueError, "negative shift count");
goto lshift_error;
}
if ((long)(int)shiftby != shiftby) {
PyErr_SetString(PyExc_ValueError,
"outrageous left shift count");
goto lshift_error;
}
wordshift = (int)shiftby / PyLong_SHIFT;
remshift = (int)shiftby - wordshift * PyLong_SHIFT;
oldsize = ABS(a->ob_size);
newsize = oldsize + wordshift;
if (remshift)
++newsize;
z = _PyLong_New(newsize);
if (z == NULL)
goto lshift_error;
if (a->ob_size < 0)
z->ob_size = -(z->ob_size);
for (i = 0; i < wordshift; i++)
z->ob_digit[i] = 0;
accum = 0;
for (i = wordshift, j = 0; j < oldsize; i++, j++) {
accum |= (twodigits)a->ob_digit[j] << remshift;
z->ob_digit[i] = (digit)(accum & PyLong_MASK);
accum >>= PyLong_SHIFT;
}
if (remshift)
z->ob_digit[newsize-1] = (digit)accum;
else
assert(!accum);
z = long_normalize(z);
lshift_error:
Py_DECREF(a);
Py_DECREF(b);
return (PyObject *) z;
}
static PyObject *
long_bitwise(PyLongObject *a,
int op,
PyLongObject *b) {
digit maska, maskb;
int negz;
Py_ssize_t size_a, size_b, size_z;
PyLongObject *z;
int i;
digit diga, digb;
PyObject *v;
if (Py_SIZE(a) < 0) {
a = (PyLongObject *) long_invert(a);
if (a == NULL)
return NULL;
maska = PyLong_MASK;
} else {
Py_INCREF(a);
maska = 0;
}
if (Py_SIZE(b) < 0) {
b = (PyLongObject *) long_invert(b);
if (b == NULL) {
Py_DECREF(a);
return NULL;
}
maskb = PyLong_MASK;
} else {
Py_INCREF(b);
maskb = 0;
}
negz = 0;
switch (op) {
case '^':
if (maska != maskb) {
maska ^= PyLong_MASK;
negz = -1;
}
break;
case '&':
if (maska && maskb) {
op = '|';
maska ^= PyLong_MASK;
maskb ^= PyLong_MASK;
negz = -1;
}
break;
case '|':
if (maska || maskb) {
op = '&';
maska ^= PyLong_MASK;
maskb ^= PyLong_MASK;
negz = -1;
}
break;
}
size_a = Py_SIZE(a);
size_b = Py_SIZE(b);
size_z = op == '&'
? (maska
? size_b
: (maskb ? size_a : MIN(size_a, size_b)))
: MAX(size_a, size_b);
z = _PyLong_New(size_z);
if (z == NULL) {
Py_DECREF(a);
Py_DECREF(b);
return NULL;
}
for (i = 0; i < size_z; ++i) {
diga = (i < size_a ? a->ob_digit[i] : 0) ^ maska;
digb = (i < size_b ? b->ob_digit[i] : 0) ^ maskb;
switch (op) {
case '&':
z->ob_digit[i] = diga & digb;
break;
case '|':
z->ob_digit[i] = diga | digb;
break;
case '^':
z->ob_digit[i] = diga ^ digb;
break;
}
}
Py_DECREF(a);
Py_DECREF(b);
z = long_normalize(z);
if (negz == 0)
return (PyObject *) z;
v = long_invert(z);
Py_DECREF(z);
return v;
}
static PyObject *
long_and(PyObject *v, PyObject *w) {
PyLongObject *a, *b;
PyObject *c;
CONVERT_BINOP(v, w, &a, &b);
c = long_bitwise(a, '&', b);
Py_DECREF(a);
Py_DECREF(b);
return c;
}
static PyObject *
long_xor(PyObject *v, PyObject *w) {
PyLongObject *a, *b;
PyObject *c;
CONVERT_BINOP(v, w, &a, &b);
c = long_bitwise(a, '^', b);
Py_DECREF(a);
Py_DECREF(b);
return c;
}
static PyObject *
long_or(PyObject *v, PyObject *w) {
PyLongObject *a, *b;
PyObject *c;
CONVERT_BINOP(v, w, &a, &b);
c = long_bitwise(a, '|', b);
Py_DECREF(a);
Py_DECREF(b);
return c;
}
static int
long_coerce(PyObject **pv, PyObject **pw) {
if (PyInt_Check(*pw)) {
*pw = PyLong_FromLong(PyInt_AS_LONG(*pw));
if (*pw == NULL)
return -1;
Py_INCREF(*pv);
return 0;
} else if (PyLong_Check(*pw)) {
Py_INCREF(*pv);
Py_INCREF(*pw);
return 0;
}
return 1;
}
static PyObject *
long_long(PyObject *v) {
if (PyLong_CheckExact(v))
Py_INCREF(v);
else
v = _PyLong_Copy((PyLongObject *)v);
return v;
}
static PyObject *
long_int(PyObject *v) {
long x;
x = PyLong_AsLong(v);
if (PyErr_Occurred()) {
if (PyErr_ExceptionMatches(PyExc_OverflowError)) {
PyErr_Clear();
if (PyLong_CheckExact(v)) {
Py_INCREF(v);
return v;
} else
return _PyLong_Copy((PyLongObject *)v);
} else
return NULL;
}
return PyInt_FromLong(x);
}
static PyObject *
long_float(PyObject *v) {
double result;
result = PyLong_AsDouble(v);
if (result == -1.0 && PyErr_Occurred())
return NULL;
return PyFloat_FromDouble(result);
}
static PyObject *
long_oct(PyObject *v) {
return _PyLong_Format(v, 8, 1, 0);
}
static PyObject *
long_hex(PyObject *v) {
return _PyLong_Format(v, 16, 1, 0);
}
static PyObject *
long_subtype_new(PyTypeObject *type, PyObject *args, PyObject *kwds);
static PyObject *
long_new(PyTypeObject *type, PyObject *args, PyObject *kwds) {
PyObject *x = NULL;
int base = -909;
static char *kwlist[] = {"x", "base", 0};
if (type != &PyLong_Type)
return long_subtype_new(type, args, kwds);
if (!PyArg_ParseTupleAndKeywords(args, kwds, "|Oi:long", kwlist,
&x, &base))
return NULL;
if (x == NULL)
return PyLong_FromLong(0L);
if (base == -909)
return PyNumber_Long(x);
else if (PyString_Check(x)) {
char *string = PyString_AS_STRING(x);
if (strlen(string) != PyString_Size(x)) {
PyObject *srepr;
srepr = PyObject_Repr(x);
if (srepr == NULL)
return NULL;
PyErr_Format(PyExc_ValueError,
"invalid literal for long() with base %d: %s",
base, PyString_AS_STRING(srepr));
Py_DECREF(srepr);
return NULL;
}
return PyLong_FromString(PyString_AS_STRING(x), NULL, base);
}
#if defined(Py_USING_UNICODE)
else if (PyUnicode_Check(x))
return PyLong_FromUnicode(PyUnicode_AS_UNICODE(x),
PyUnicode_GET_SIZE(x),
base);
#endif
else {
PyErr_SetString(PyExc_TypeError,
"long() can't convert non-string with explicit base");
return NULL;
}
}
static PyObject *
long_subtype_new(PyTypeObject *type, PyObject *args, PyObject *kwds) {
PyLongObject *tmp, *newobj;
Py_ssize_t i, n;
assert(PyType_IsSubtype(type, &PyLong_Type));
tmp = (PyLongObject *)long_new(&PyLong_Type, args, kwds);
if (tmp == NULL)
return NULL;
assert(PyLong_CheckExact(tmp));
n = Py_SIZE(tmp);
if (n < 0)
n = -n;
newobj = (PyLongObject *)type->tp_alloc(type, n);
if (newobj == NULL) {
Py_DECREF(tmp);
return NULL;
}
assert(PyLong_Check(newobj));
Py_SIZE(newobj) = Py_SIZE(tmp);
for (i = 0; i < n; i++)
newobj->ob_digit[i] = tmp->ob_digit[i];
Py_DECREF(tmp);
return (PyObject *)newobj;
}
static PyObject *
long_getnewargs(PyLongObject *v) {
return Py_BuildValue("(N)", _PyLong_Copy(v));
}
static PyObject *
long_getN(PyLongObject *v, void *context) {
return PyLong_FromLong((Py_intptr_t)context);
}
static PyObject *
long__format__(PyObject *self, PyObject *args) {
PyObject *format_spec;
if (!PyArg_ParseTuple(args, "O:__format__", &format_spec))
return NULL;
if (PyBytes_Check(format_spec))
return _PyLong_FormatAdvanced(self,
PyBytes_AS_STRING(format_spec),
PyBytes_GET_SIZE(format_spec));
if (PyUnicode_Check(format_spec)) {
PyObject *result;
PyObject *str_spec = PyObject_Str(format_spec);
if (str_spec == NULL)
return NULL;
result = _PyLong_FormatAdvanced(self,
PyBytes_AS_STRING(str_spec),
PyBytes_GET_SIZE(str_spec));
Py_DECREF(str_spec);
return result;
}
PyErr_SetString(PyExc_TypeError, "__format__ requires str or unicode");
return NULL;
}
static PyObject *
long_sizeof(PyLongObject *v) {
Py_ssize_t res;
res = v->ob_type->tp_basicsize;
if (v->ob_size != 0)
res += abs(v->ob_size) * sizeof(digit);
return PyInt_FromSsize_t(res);
}
#if 0
static PyObject *
long_is_finite(PyObject *v) {
Py_RETURN_TRUE;
}
#endif
static PyMethodDef long_methods[] = {
{
"conjugate", (PyCFunction)long_long, METH_NOARGS,
"Returns self, the complex conjugate of any long."
},
#if 0
{
"is_finite", (PyCFunction)long_is_finite, METH_NOARGS,
"Returns always True."
},
#endif
{
"__trunc__", (PyCFunction)long_long, METH_NOARGS,
"Truncating an Integral returns itself."
},
{"__getnewargs__", (PyCFunction)long_getnewargs, METH_NOARGS},
{"__format__", (PyCFunction)long__format__, METH_VARARGS},
{
"__sizeof__", (PyCFunction)long_sizeof, METH_NOARGS,
"Returns size in memory, in bytes"
},
{NULL, NULL}
};
static PyGetSetDef long_getset[] = {
{
"real",
(getter)long_long, (setter)NULL,
"the real part of a complex number",
NULL
},
{
"imag",
(getter)long_getN, (setter)NULL,
"the imaginary part of a complex number",
(void*)0
},
{
"numerator",
(getter)long_long, (setter)NULL,
"the numerator of a rational number in lowest terms",
NULL
},
{
"denominator",
(getter)long_getN, (setter)NULL,
"the denominator of a rational number in lowest terms",
(void*)1
},
{NULL}
};
PyDoc_STRVAR(long_doc,
"long(x[, base]) -> integer\n\
\n\
Convert a string or number to a long integer, if possible. A floating\n\
point argument will be truncated towards zero (this does not include a\n\
string representation of a floating point number!) When converting a\n\
string, use the optional base. It is an error to supply a base when\n\
converting a non-string.");
static PyNumberMethods long_as_number = {
(binaryfunc) long_add,
(binaryfunc) long_sub,
(binaryfunc) long_mul,
long_classic_div,
long_mod,
long_divmod,
long_pow,
(unaryfunc) long_neg,
(unaryfunc) long_long,
(unaryfunc) long_abs,
(inquiry) long_nonzero,
(unaryfunc) long_invert,
long_lshift,
(binaryfunc) long_rshift,
long_and,
long_xor,
long_or,
long_coerce,
long_int,
long_long,
long_float,
long_oct,
long_hex,
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
long_div,
long_true_divide,
0,
0,
long_long,
};
PyTypeObject PyLong_Type = {
PyObject_HEAD_INIT(&PyType_Type)
0,
"long",
sizeof(PyLongObject) - sizeof(digit),
sizeof(digit),
long_dealloc,
0,
0,
0,
(cmpfunc)long_compare,
long_repr,
&long_as_number,
0,
0,
(hashfunc)long_hash,
0,
long_str,
PyObject_GenericGetAttr,
0,
0,
Py_TPFLAGS_DEFAULT | Py_TPFLAGS_CHECKTYPES |
Py_TPFLAGS_BASETYPE | Py_TPFLAGS_LONG_SUBCLASS,
long_doc,
0,
0,
0,
0,
0,
0,
long_methods,
0,
long_getset,
0,
0,
0,
0,
0,
0,
0,
long_new,
PyObject_Del,
};
