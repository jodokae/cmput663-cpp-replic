#include "Python.h"
#if defined(__sgi) && defined(WITH_THREAD) && !defined(_SGI_MP_SOURCE)
#define _SGI_MP_SOURCE
#endif
#include <ctype.h>
#if defined(HAVE_ERRNO_H)
#include <errno.h>
#endif
static unsigned long smallmax[] = {
0,
0,
ULONG_MAX / 2,
ULONG_MAX / 3,
ULONG_MAX / 4,
ULONG_MAX / 5,
ULONG_MAX / 6,
ULONG_MAX / 7,
ULONG_MAX / 8,
ULONG_MAX / 9,
ULONG_MAX / 10,
ULONG_MAX / 11,
ULONG_MAX / 12,
ULONG_MAX / 13,
ULONG_MAX / 14,
ULONG_MAX / 15,
ULONG_MAX / 16,
ULONG_MAX / 17,
ULONG_MAX / 18,
ULONG_MAX / 19,
ULONG_MAX / 20,
ULONG_MAX / 21,
ULONG_MAX / 22,
ULONG_MAX / 23,
ULONG_MAX / 24,
ULONG_MAX / 25,
ULONG_MAX / 26,
ULONG_MAX / 27,
ULONG_MAX / 28,
ULONG_MAX / 29,
ULONG_MAX / 30,
ULONG_MAX / 31,
ULONG_MAX / 32,
ULONG_MAX / 33,
ULONG_MAX / 34,
ULONG_MAX / 35,
ULONG_MAX / 36,
};
#if SIZEOF_LONG == 4
static int digitlimit[] = {
0, 0, 32, 20, 16, 13, 12, 11, 10, 10,
9, 9, 8, 8, 8, 8, 8, 7, 7, 7,
7, 7, 7, 7, 6, 6, 6, 6, 6, 6,
6, 6, 6, 6, 6, 6, 6
};
#elif SIZEOF_LONG == 8
static int digitlimit[] = {
0, 0, 64, 40, 32, 27, 24, 22, 21, 20,
19, 18, 17, 17, 16, 16, 16, 15, 15, 15,
14, 14, 14, 14, 13, 13, 13, 13, 13, 13,
13, 12, 12, 12, 12, 12, 12
};
#else
#error "Need table for SIZEOF_LONG"
#endif
unsigned long
PyOS_strtoul(register char *str, char **ptr, int base) {
register unsigned long result = 0;
register int c;
register int ovlimit;
while (*str && isspace(Py_CHARMASK(*str)))
++str;
switch (base) {
case 0:
if (*str == '0') {
++str;
if (*str == 'x' || *str == 'X') {
if (_PyLong_DigitValue[Py_CHARMASK(str[1])] >= 16) {
if (ptr)
*ptr = str;
return 0;
}
++str;
base = 16;
} else if (*str == 'o' || *str == 'O') {
if (_PyLong_DigitValue[Py_CHARMASK(str[1])] >= 8) {
if (ptr)
*ptr = str;
return 0;
}
++str;
base = 8;
} else if (*str == 'b' || *str == 'B') {
if (_PyLong_DigitValue[Py_CHARMASK(str[1])] >= 2) {
if (ptr)
*ptr = str;
return 0;
}
++str;
base = 2;
} else {
base = 8;
}
} else
base = 10;
break;
case 2:
if (*str == '0') {
++str;
if (*str == 'b' || *str == 'B') {
if (_PyLong_DigitValue[Py_CHARMASK(str[1])] >= 2) {
if (ptr)
*ptr = str;
return 0;
}
++str;
}
}
break;
case 8:
if (*str == '0') {
++str;
if (*str == 'o' || *str == 'O') {
if (_PyLong_DigitValue[Py_CHARMASK(str[1])] >= 8) {
if (ptr)
*ptr = str;
return 0;
}
++str;
}
}
break;
case 16:
if (*str == '0') {
++str;
if (*str == 'x' || *str == 'X') {
if (_PyLong_DigitValue[Py_CHARMASK(str[1])] >= 16) {
if (ptr)
*ptr = str;
return 0;
}
++str;
}
}
break;
}
if (base < 2 || base > 36) {
if (ptr)
*ptr = str;
return 0;
}
while (*str == '0')
++str;
ovlimit = digitlimit[base];
while ((c = _PyLong_DigitValue[Py_CHARMASK(*str)]) < base) {
if (ovlimit > 0)
result = result * base + c;
else {
register unsigned long temp_result;
if (ovlimit < 0)
goto overflowed;
if (result > smallmax[base])
goto overflowed;
result *= base;
temp_result = result + c;
if (temp_result < result)
goto overflowed;
result = temp_result;
}
++str;
--ovlimit;
}
if (ptr)
*ptr = str;
return result;
overflowed:
if (ptr) {
while (_PyLong_DigitValue[Py_CHARMASK(*str)] < base)
++str;
*ptr = str;
}
errno = ERANGE;
return (unsigned long)-1;
}
#define PY_ABS_LONG_MIN (0-(unsigned long)LONG_MIN)
long
PyOS_strtol(char *str, char **ptr, int base) {
long result;
unsigned long uresult;
char sign;
while (*str && isspace(Py_CHARMASK(*str)))
str++;
sign = *str;
if (sign == '+' || sign == '-')
str++;
uresult = PyOS_strtoul(str, ptr, base);
if (uresult <= (unsigned long)LONG_MAX) {
result = (long)uresult;
if (sign == '-')
result = -result;
} else if (sign == '-' && uresult == PY_ABS_LONG_MIN) {
result = LONG_MIN;
} else {
errno = ERANGE;
result = LONG_MAX;
}
return result;
}