#include "Python.h"
extern void PyMarshal_Init(void);
extern void initimp(void);
extern void initgc(void);
extern void initriscos(void);
extern void initswi(void);
struct _inittab _PyImport_Inittab[] = {
{"riscos", initriscos},
{"marshal", PyMarshal_Init},
{"imp", initimp},
{"__main__", NULL},
{"__builtin__", NULL},
{"sys", NULL},
{"exceptions", NULL},
{"gc", initgc},
{0, 0}
};
