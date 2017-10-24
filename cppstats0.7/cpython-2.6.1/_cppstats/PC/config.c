#include "Python.h"
extern void initarray(void);
#if !defined(MS_WINI64)
extern void initaudioop(void);
#endif
extern void initbinascii(void);
extern void initcmath(void);
extern void initerrno(void);
extern void initfuture_builtins(void);
extern void initgc(void);
#if !defined(MS_WINI64)
extern void initimageop(void);
#endif
extern void initmath(void);
extern void init_md5(void);
extern void initnt(void);
extern void initoperator(void);
extern void initsignal(void);
extern void init_sha(void);
extern void init_sha256(void);
extern void init_sha512(void);
extern void initstrop(void);
extern void inittime(void);
extern void initthread(void);
extern void initcStringIO(void);
extern void initcPickle(void);
#if defined(WIN32)
extern void initmsvcrt(void);
extern void init_locale(void);
#endif
extern void init_codecs(void);
extern void init_weakref(void);
extern void init_hotshot(void);
extern void initxxsubtype(void);
extern void initzipimport(void);
extern void init_random(void);
extern void inititertools(void);
extern void init_collections(void);
extern void init_heapq(void);
extern void init_bisect(void);
extern void init_symtable(void);
extern void initmmap(void);
extern void init_csv(void);
extern void init_sre(void);
extern void initparser(void);
extern void init_winreg(void);
extern void init_struct(void);
extern void initdatetime(void);
extern void init_fileio(void);
extern void init_bytesio(void);
extern void init_functools(void);
extern void init_json(void);
extern void initzlib(void);
extern void init_multibytecodec(void);
extern void init_codecs_cn(void);
extern void init_codecs_hk(void);
extern void init_codecs_iso2022(void);
extern void init_codecs_jp(void);
extern void init_codecs_kr(void);
extern void init_codecs_tw(void);
extern void init_subprocess(void);
extern void init_lsprof(void);
extern void init_ast(void);
extern void _PyWarnings_Init(void);
extern void PyMarshal_Init(void);
extern void initimp(void);
struct _inittab _PyImport_Inittab[] = {
{"array", initarray},
{"_ast", init_ast},
#if defined(MS_WINDOWS)
#if !defined(MS_WINI64)
{"audioop", initaudioop},
#endif
#endif
{"binascii", initbinascii},
{"cmath", initcmath},
{"errno", initerrno},
{"future_builtins", initfuture_builtins},
{"gc", initgc},
#if !defined(MS_WINI64)
{"imageop", initimageop},
#endif
{"math", initmath},
{"_md5", init_md5},
{"nt", initnt},
{"operator", initoperator},
{"signal", initsignal},
{"_sha", init_sha},
{"_sha256", init_sha256},
{"_sha512", init_sha512},
{"strop", initstrop},
{"time", inittime},
#if defined(WITH_THREAD)
{"thread", initthread},
#endif
{"cStringIO", initcStringIO},
{"cPickle", initcPickle},
#if defined(WIN32)
{"msvcrt", initmsvcrt},
{"_locale", init_locale},
#endif
{"_subprocess", init_subprocess},
{"_codecs", init_codecs},
{"_weakref", init_weakref},
{"_hotshot", init_hotshot},
{"_random", init_random},
{"_bisect", init_bisect},
{"_heapq", init_heapq},
{"_lsprof", init_lsprof},
{"itertools", inititertools},
{"_collections", init_collections},
{"_symtable", init_symtable},
{"mmap", initmmap},
{"_csv", init_csv},
{"_sre", init_sre},
{"parser", initparser},
{"_winreg", init_winreg},
{"_struct", init_struct},
{"datetime", initdatetime},
{"_fileio", init_fileio},
{"_bytesio", init_bytesio},
{"_functools", init_functools},
{"_json", init_json},
{"xxsubtype", initxxsubtype},
{"zipimport", initzipimport},
{"zlib", initzlib},
{"_multibytecodec", init_multibytecodec},
{"_codecs_cn", init_codecs_cn},
{"_codecs_hk", init_codecs_hk},
{"_codecs_iso2022", init_codecs_iso2022},
{"_codecs_jp", init_codecs_jp},
{"_codecs_kr", init_codecs_kr},
{"_codecs_tw", init_codecs_tw},
{"marshal", PyMarshal_Init},
{"imp", initimp},
{"__main__", NULL},
{"__builtin__", NULL},
{"sys", NULL},
{"exceptions", NULL},
{"_warnings", _PyWarnings_Init},
{0, 0}
};