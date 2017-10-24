#include "Python.h"
#if defined(__FreeBSD__)
#include <floatingpoint.h>
#endif
int
main(int argc, char **argv) {
#if defined(__FreeBSD__)
fp_except_t m;
m = fpgetmask();
fpsetmask(m & ~FP_X_OFL);
#endif
return Py_Main(argc, argv);
}
