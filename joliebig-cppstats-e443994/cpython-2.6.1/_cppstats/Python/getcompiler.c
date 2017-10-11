/* Return the compiler identification, if possible. */
#include "Python.h"
#if !defined(COMPILER)
#if defined(__GNUC__)
#define COMPILER "\n[GCC " __VERSION__ "]"
#endif
#endif /* !COMPILER */
#if !defined(COMPILER)
#if defined(__cplusplus)
#define COMPILER "[C++]"
#else
#define COMPILER "[C]"
#endif
#endif /* !COMPILER */
const char *
Py_GetCompiler(void) {
return COMPILER;
}
