#include "Python.h"
#if !defined(PLATFORM)
#define PLATFORM "unknown"
#endif
const char *
Py_GetPlatform(void) {
return PLATFORM;
}
