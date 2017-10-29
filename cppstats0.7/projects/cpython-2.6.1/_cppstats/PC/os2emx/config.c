#include "Python.h"
extern void initos2();
extern void initsignal();
#if defined(WITH_THREAD)
extern void initthread();
#endif
extern void init_codecs();
extern void init_csv();
extern void init_locale();
extern void init_random();
extern void init_sre();
extern void init_symtable();
extern void init_weakref();
extern void initarray();
extern void initbinascii();
extern void initcPickle();
extern void initcStringIO();
extern void init_collections();
extern void initcmath();
extern void initdatetime();
extern void initdl();
extern void initerrno();
extern void initfcntl();
extern void init_fileio();
extern void init_functools();
extern void init_heapq();
extern void initimageop();
extern void inititertools();
extern void initmath();
extern void init_md5();
extern void initoperator();
extern void init_sha();
extern void init_sha256();
extern void init_sha512();
extern void initstrop();
extern void init_struct();
extern void inittermios();
extern void inittime();
extern void inittiming();
extern void initxxsubtype();
extern void initzipimport();
#if !HAVE_DYNAMIC_LOADING
extern void init_curses();
extern void init_curses_panel();
extern void init_hotshot();
extern void init_testcapi();
extern void initbsddb185();
extern void initbz2();
extern void initfpectl();
extern void initfpetest();
extern void initparser();
extern void initpwd();
extern void initunicodedata();
extern void initzlib();
#if defined(USE_SOCKET)
extern void init_socket();
extern void initselect();
#endif
#endif
extern void PyMarshal_Init();
extern void initimp();
extern void initgc();
struct _inittab _PyImport_Inittab[] = {
{"os2", initos2},
{"signal", initsignal},
#if defined(WITH_THREAD)
{"thread", initthread},
#endif
{"_codecs", init_codecs},
{"_csv", init_csv},
{"_locale", init_locale},
{"_random", init_random},
{"_sre", init_sre},
{"_symtable", init_symtable},
{"_weakref", init_weakref},
{"array", initarray},
{"binascii", initbinascii},
{"cPickle", initcPickle},
{"cStringIO", initcStringIO},
{"_collections", init_collections},
{"cmath", initcmath},
{"datetime", initdatetime},
{"dl", initdl},
{"errno", initerrno},
{"fcntl", initfcntl},
{"_fileio", init_fileio},
{"_functools", init_functools},
{"_heapq", init_heapq},
{"imageop", initimageop},
{"itertools", inititertools},
{"math", initmath},
{"_md5", init_md5},
{"operator", initoperator},
{"_sha", init_sha},
{"_sha256", init_sha256},
{"_sha512", init_sha512},
{"strop", initstrop},
{"_struct", init_struct},
{"termios", inittermios},
{"time", inittime},
{"timing", inittiming},
{"xxsubtype", initxxsubtype},
{"zipimport", initzipimport},
#if !HAVE_DYNAMIC_LOADING
{"_curses", init_curses},
{"_curses_panel", init_curses_panel},
{"_hotshot", init_hotshot},
{"_testcapi", init_testcapi},
{"bsddb185", initbsddb185},
{"bz2", initbz2},
{"fpectl", initfpectl},
{"fpetest", initfpetest},
{"parser", initparser},
{"pwd", initpwd},
{"unicodedata", initunicodedata},
{"zlib", initzlib},
#if defined(USE_SOCKET)
{"_socket", init_socket},
{"select", initselect},
#endif
#endif
{"marshal", PyMarshal_Init},
{"imp", initimp},
{"__main__", NULL},
{"__builtin__", NULL},
{"sys", NULL},
{"exceptions", NULL},
{"gc", initgc},
{0, 0}
};