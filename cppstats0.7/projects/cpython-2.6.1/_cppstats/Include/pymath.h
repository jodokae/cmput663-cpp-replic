#if !defined(Py_PYMATH_H)
#define Py_PYMATH_H
#include "pyconfig.h"
#if defined(HAVE_STDINT_H)
#include <stdint.h>
#endif
#if !defined(HAVE_COPYSIGN)
extern double copysign(double, double);
#endif
#if !defined(HAVE_ACOSH)
extern double acosh(double);
#endif
#if !defined(HAVE_ASINH)
extern double asinh(double);
#endif
#if !defined(HAVE_ATANH)
extern double atanh(double);
#endif
#if !defined(HAVE_LOG1P)
extern double log1p(double);
#endif
#if !defined(HAVE_HYPOT)
extern double hypot(double, double);
#endif
#if !defined(_MSC_VER)
#if !defined(__STDC__)
extern double fmod (double, double);
extern double frexp (double, int *);
extern double ldexp (double, int);
extern double modf (double, double *);
extern double pow(double, double);
#endif
#endif
#if defined(_OSF_SOURCE)
extern int finite(double);
extern double copysign(double, double);
#endif
#if !defined(Py_MATH_PIl)
#define Py_MATH_PIl 3.1415926535897932384626433832795029L
#endif
#if !defined(Py_MATH_PI)
#define Py_MATH_PI 3.14159265358979323846
#endif
#if !defined(Py_MATH_El)
#define Py_MATH_El 2.7182818284590452353602874713526625L
#endif
#if !defined(Py_MATH_E)
#define Py_MATH_E 2.7182818284590452354
#endif
#if !defined(Py_IS_NAN)
#if defined(HAVE_ISNAN)
#define Py_IS_NAN(X) isnan(X)
#else
#define Py_IS_NAN(X) ((X) != (X))
#endif
#endif
#if !defined(Py_IS_INFINITY)
#if defined(HAVE_ISINF)
#define Py_IS_INFINITY(X) isinf(X)
#else
#define Py_IS_INFINITY(X) ((X) && (X)*0.5 == (X))
#endif
#endif
#if !defined(Py_IS_FINITE)
#if defined(HAVE_FINITE)
#define Py_IS_FINITE(X) finite(X)
#else
#define Py_IS_FINITE(X) (!Py_IS_INFINITY(X) && !Py_IS_NAN(X))
#endif
#endif
#if !defined(Py_HUGE_VAL)
#define Py_HUGE_VAL HUGE_VAL
#endif
#if !defined(Py_NAN) && !defined(Py_NO_NAN)
#define Py_NAN (Py_HUGE_VAL * 0.)
#endif
#if defined(__FreeBSD__) || defined(__OpenBSD__)
#define Py_OVERFLOWED(X) isinf(X)
#else
#define Py_OVERFLOWED(X) ((X) != 0.0 && (errno == ERANGE || (X) == Py_HUGE_VAL || (X) == -Py_HUGE_VAL))
#endif
#endif