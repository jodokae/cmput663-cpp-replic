#if defined(__i386__) || defined(__x86_64__)
#include "x86-ffitarget.h"
#elif defined(__ppc__) || defined(__ppc64__)
#include "ppc-ffitarget.h"
#else
#error "Unsupported CPU type"
#endif
