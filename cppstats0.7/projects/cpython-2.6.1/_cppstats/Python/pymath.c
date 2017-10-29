#include "Python.h"
#if !defined(HAVE_HYPOT)
double hypot(double x, double y) {
double yx;
x = fabs(x);
y = fabs(y);
if (x < y) {
double temp = x;
x = y;
y = temp;
}
if (x == 0.)
return 0.;
else {
yx = y/x;
return x*sqrt(1.+yx*yx);
}
}
#endif
#if !defined(HAVE_COPYSIGN)
static double
copysign(double x, double y) {
if (y > 0. || (y == 0. && atan2(y, -1.) > 0.)) {
return fabs(x);
} else {
return -fabs(x);
}
}
#endif
#if !defined(HAVE_LOG1P)
#include <float.h>
double
log1p(double x) {
double y;
if (fabs(x) < DBL_EPSILON/2.) {
return x;
} else if (-0.5 <= x && x <= 1.) {
y = 1.+x;
return log(y)-((y-1.)-x)/y;
} else {
return log(1.+x);
}
}
#endif
static const double ln2 = 6.93147180559945286227E-01;
static const double two_pow_m28 = 3.7252902984619141E-09;
static const double two_pow_p28 = 268435456.0;
static const double zero = 0.0;
#if !defined(HAVE_ASINH)
double
asinh(double x) {
double w;
double absx = fabs(x);
if (Py_IS_NAN(x) || Py_IS_INFINITY(x)) {
return x+x;
}
if (absx < two_pow_m28) {
return x;
}
if (absx > two_pow_p28) {
w = log(absx)+ln2;
} else if (absx > 2.0) {
w = log(2.0*absx + 1.0 / (sqrt(x*x + 1.0) + absx));
} else {
double t = x*x;
w = log1p(absx + t / (1.0 + sqrt(1.0 + t)));
}
return copysign(w, x);
}
#endif
#if !defined(HAVE_ACOSH)
double
acosh(double x) {
if (Py_IS_NAN(x)) {
return x+x;
}
if (x < 1.) {
errno = EDOM;
#if defined(Py_NAN)
return Py_NAN;
#else
return (x-x)/(x-x);
#endif
} else if (x >= two_pow_p28) {
if (Py_IS_INFINITY(x)) {
return x+x;
} else {
return log(x)+ln2;
}
} else if (x == 1.) {
return 0.0;
} else if (x > 2.) {
double t = x*x;
return log(2.0*x - 1.0 / (x + sqrt(t - 1.0)));
} else {
double t = x - 1.0;
return log1p(t + sqrt(2.0*t + t*t));
}
}
#endif
#if !defined(HAVE_ATANH)
double
atanh(double x) {
double absx;
double t;
if (Py_IS_NAN(x)) {
return x+x;
}
absx = fabs(x);
if (absx >= 1.) {
errno = EDOM;
#if defined(Py_NAN)
return Py_NAN;
#else
return x/zero;
#endif
}
if (absx < two_pow_m28) {
return x;
}
if (absx < 0.5) {
t = absx+absx;
t = 0.5 * log1p(t + t*absx / (1.0 - absx));
} else {
t = 0.5 * log1p((absx + absx) / (1.0 - absx));
}
return copysign(t, x);
}
#endif
