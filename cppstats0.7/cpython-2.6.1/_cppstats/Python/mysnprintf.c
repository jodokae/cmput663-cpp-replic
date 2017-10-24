#include "Python.h"
#include <ctype.h>
int
PyOS_snprintf(char *str, size_t size, const char *format, ...) {
int rc;
va_list va;
va_start(va, format);
rc = PyOS_vsnprintf(str, size, format, va);
va_end(va);
return rc;
}
int
PyOS_vsnprintf(char *str, size_t size, const char *format, va_list va) {
int len;
#if defined(HAVE_SNPRINTF)
#define _PyOS_vsnprintf_EXTRA_SPACE 1
#else
#define _PyOS_vsnprintf_EXTRA_SPACE 512
char *buffer;
#endif
assert(str != NULL);
assert(size > 0);
assert(format != NULL);
if (size > INT_MAX - _PyOS_vsnprintf_EXTRA_SPACE) {
len = -666;
goto Done;
}
#if defined(HAVE_SNPRINTF)
len = vsnprintf(str, size, format, va);
#else
buffer = PyMem_MALLOC(size + _PyOS_vsnprintf_EXTRA_SPACE);
if (buffer == NULL) {
len = -666;
goto Done;
}
len = vsprintf(buffer, format, va);
if (len < 0)
;
else if ((size_t)len >= size + _PyOS_vsnprintf_EXTRA_SPACE)
Py_FatalError("Buffer overflow in PyOS_snprintf/PyOS_vsnprintf");
else {
const size_t to_copy = (size_t)len < size ?
(size_t)len : size - 1;
assert(to_copy < size);
memcpy(str, buffer, to_copy);
str[to_copy] = '\0';
}
PyMem_FREE(buffer);
#endif
Done:
if (size > 0)
str[size-1] = '\0';
return len;
#undef _PyOS_vsnprintf_EXTRA_SPACE
}
