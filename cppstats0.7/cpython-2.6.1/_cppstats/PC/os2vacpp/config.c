#include "Python.h"
extern void initarray(void);
extern void initaudioop(void);
extern void initbinascii(void);
extern void initcmath(void);
extern void initerrno(void);
extern void initimageop(void);
extern void initmath(void);
extern void initmd5(void);
extern void initnt(void);
extern void initos2(void);
extern void initoperator(void);
extern void initposix(void);
extern void initrgbimg(void);
extern void initsignal(void);
extern void initselect(void);
extern void init_socket(void);
extern void initstrop(void);
extern void initstruct(void);
extern void inittime(void);
extern void initthread(void);
extern void initcStringIO(void);
extern void initcPickle(void);
extern void initpcre(void);
#if defined(WIN32)
extern void initmsvcrt(void);
#endif
extern void PyMarshal_Init(void);
extern void initimp(void);
struct _inittab _PyImport_Inittab[] = {
{"array", initarray},
#if defined(M_I386)
{"audioop", initaudioop},
#endif
{"binascii", initbinascii},
{"cmath", initcmath},
{"errno", initerrno},
{"math", initmath},
{"md5", initmd5},
#if defined(MS_WINDOWS) || defined(__BORLANDC__) || defined(__WATCOMC__)
{"nt", initnt},
#else
#if defined(PYOS_OS2)
{"os2", initos2},
#else
{"posix", initposix},
#endif
#endif
{"operator", initoperator},
{"signal", initsignal},
#if defined(USE_SOCKET)
{"_socket", init_socket},
{"select", initselect},
#endif
{"strop", initstrop},
{"struct", initstruct},
{"time", inittime},
#if defined(WITH_THREAD)
{"thread", initthread},
#endif
{"cStringIO", initcStringIO},
{"cPickle", initcPickle},
{"pcre", initpcre},
#if defined(WIN32)
{"msvcrt", initmsvcrt},
#endif
{"marshal", PyMarshal_Init},
{"imp", initimp},
{"__main__", NULL},
{"__builtin__", NULL},
{"sys", NULL},
{0, 0}
};
