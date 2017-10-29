#if defined(__i386__)
#if !defined(X86_DARWIN)
#define X86_DARWIN
#endif
#undef POWERPC_DARWIN
#include "../src/x86/ffitarget.h"
#elif defined(__ppc__)
#if !defined(POWERPC_DARWIN)
#define POWERPC_DARWIN
#endif
#undef X86_DARWIN
#include "../src/powerpc/ffitarget.h"
#endif