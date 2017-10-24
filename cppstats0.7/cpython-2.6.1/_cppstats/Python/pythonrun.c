#include "Python.h"
#include "Python-ast.h"
#undef Yield
#include "grammar.h"
#include "node.h"
#include "token.h"
#include "parsetok.h"
#include "errcode.h"
#include "code.h"
#include "compile.h"
#include "symtable.h"
#include "pyarena.h"
#include "ast.h"
#include "eval.h"
#include "marshal.h"
#if defined(HAVE_SIGNAL_H)
#include <signal.h>
#endif
#if defined(HAVE_LANGINFO_H)
#include <locale.h>
#include <langinfo.h>
#endif
#if defined(MS_WINDOWS)
#undef BYTE
#include "windows.h"
#endif
#if !defined(Py_REF_DEBUG)
#define PRINT_TOTAL_REFS()
#else
#define PRINT_TOTAL_REFS() fprintf(stderr, "[%" PY_FORMAT_SIZE_T "d refs]\n", _Py_GetRefTotal())
#endif
#if defined(__cplusplus)
extern "C" {
#endif
extern char *Py_GetPath(void);
extern grammar _PyParser_Grammar;
static void initmain(void);
static void initsite(void);
static PyObject *run_mod(mod_ty, const char *, PyObject *, PyObject *,
PyCompilerFlags *, PyArena *);
static PyObject *run_pyc_file(FILE *, const char *, PyObject *, PyObject *,
PyCompilerFlags *);
static void err_input(perrdetail *);
static void initsigs(void);
static void call_sys_exitfunc(void);
static void call_ll_exitfuncs(void);
extern void _PyUnicode_Init(void);
extern void _PyUnicode_Fini(void);
#if defined(WITH_THREAD)
extern void _PyGILState_Init(PyInterpreterState *, PyThreadState *);
extern void _PyGILState_Fini(void);
#endif
int Py_DebugFlag;
int Py_VerboseFlag;
int Py_InteractiveFlag;
int Py_InspectFlag;
int Py_NoSiteFlag;
int Py_BytesWarningFlag;
int Py_DontWriteBytecodeFlag;
int Py_UseClassExceptionsFlag = 1;
int Py_FrozenFlag;
int Py_UnicodeFlag = 0;
int Py_IgnoreEnvironmentFlag;
int _Py_QnewFlag = 0;
int Py_NoUserSiteDirectory = 0;
PyObject *
PyModule_GetWarningsModule(void) {
return PyImport_ImportModule("warnings");
}
static int initialized = 0;
int
Py_IsInitialized(void) {
return initialized;
}
static int
add_flag(int flag, const char *envs) {
int env = atoi(envs);
if (flag < env)
flag = env;
if (flag < 1)
flag = 1;
return flag;
}
void
Py_InitializeEx(int install_sigs) {
PyInterpreterState *interp;
PyThreadState *tstate;
PyObject *bimod, *sysmod;
char *p;
char *icodeset = NULL;
char *codeset = NULL;
char *errors = NULL;
int free_codeset = 0;
int overridden = 0;
PyObject *sys_stream, *sys_isatty;
#if defined(Py_USING_UNICODE) && defined(HAVE_LANGINFO_H) && defined(CODESET)
char *saved_locale, *loc_codeset;
#endif
#if defined(MS_WINDOWS)
char ibuf[128];
char buf[128];
#endif
extern void _Py_ReadyTypes(void);
if (initialized)
return;
initialized = 1;
if ((p = Py_GETENV("PYTHONDEBUG")) && *p != '\0')
Py_DebugFlag = add_flag(Py_DebugFlag, p);
if ((p = Py_GETENV("PYTHONVERBOSE")) && *p != '\0')
Py_VerboseFlag = add_flag(Py_VerboseFlag, p);
if ((p = Py_GETENV("PYTHONOPTIMIZE")) && *p != '\0')
Py_OptimizeFlag = add_flag(Py_OptimizeFlag, p);
if ((p = Py_GETENV("PYTHONDONTWRITEBYTECODE")) && *p != '\0')
Py_DontWriteBytecodeFlag = add_flag(Py_DontWriteBytecodeFlag, p);
interp = PyInterpreterState_New();
if (interp == NULL)
Py_FatalError("Py_Initialize: can't make first interpreter");
tstate = PyThreadState_New(interp);
if (tstate == NULL)
Py_FatalError("Py_Initialize: can't make first thread");
(void) PyThreadState_Swap(tstate);
_Py_ReadyTypes();
if (!_PyFrame_Init())
Py_FatalError("Py_Initialize: can't init frames");
if (!_PyInt_Init())
Py_FatalError("Py_Initialize: can't init ints");
if (!PyByteArray_Init())
Py_FatalError("Py_Initialize: can't init bytearray");
_PyFloat_Init();
interp->modules = PyDict_New();
if (interp->modules == NULL)
Py_FatalError("Py_Initialize: can't make modules dictionary");
interp->modules_reloading = PyDict_New();
if (interp->modules_reloading == NULL)
Py_FatalError("Py_Initialize: can't make modules_reloading dictionary");
#if defined(Py_USING_UNICODE)
_PyUnicode_Init();
#endif
bimod = _PyBuiltin_Init();
if (bimod == NULL)
Py_FatalError("Py_Initialize: can't initialize __builtin__");
interp->builtins = PyModule_GetDict(bimod);
if (interp->builtins == NULL)
Py_FatalError("Py_Initialize: can't initialize builtins dict");
Py_INCREF(interp->builtins);
sysmod = _PySys_Init();
if (sysmod == NULL)
Py_FatalError("Py_Initialize: can't initialize sys");
interp->sysdict = PyModule_GetDict(sysmod);
if (interp->sysdict == NULL)
Py_FatalError("Py_Initialize: can't initialize sys dict");
Py_INCREF(interp->sysdict);
_PyImport_FixupExtension("sys", "sys");
PySys_SetPath(Py_GetPath());
PyDict_SetItemString(interp->sysdict, "modules",
interp->modules);
_PyImport_Init();
_PyExc_Init();
_PyImport_FixupExtension("exceptions", "exceptions");
_PyImport_FixupExtension("__builtin__", "__builtin__");
_PyImportHooks_Init();
if (install_sigs)
initsigs();
_PyWarnings_Init();
if (PySys_HasWarnOptions()) {
PyObject *warnings_module = PyImport_ImportModule("warnings");
if (!warnings_module)
PyErr_Clear();
Py_XDECREF(warnings_module);
}
initmain();
if (!Py_NoSiteFlag)
initsite();
#if defined(WITH_THREAD)
_PyGILState_Init(interp, tstate);
#endif
if ((p = Py_GETENV("PYTHONIOENCODING")) && *p != '\0') {
p = icodeset = codeset = strdup(p);
free_codeset = 1;
errors = strchr(p, ':');
if (errors) {
*errors = '\0';
errors++;
}
overridden = 1;
}
#if defined(Py_USING_UNICODE) && defined(HAVE_LANGINFO_H) && defined(CODESET)
if (!overridden || !Py_FileSystemDefaultEncoding) {
saved_locale = strdup(setlocale(LC_CTYPE, NULL));
setlocale(LC_CTYPE, "");
loc_codeset = nl_langinfo(CODESET);
if (loc_codeset && *loc_codeset) {
PyObject *enc = PyCodec_Encoder(loc_codeset);
if (enc) {
loc_codeset = strdup(loc_codeset);
Py_DECREF(enc);
} else {
loc_codeset = NULL;
PyErr_Clear();
}
} else
loc_codeset = NULL;
setlocale(LC_CTYPE, saved_locale);
free(saved_locale);
if (!overridden) {
codeset = icodeset = loc_codeset;
free_codeset = 1;
}
if (!Py_FileSystemDefaultEncoding) {
Py_FileSystemDefaultEncoding = loc_codeset;
if (!overridden)
free_codeset = 0;
}
}
#endif
#if defined(MS_WINDOWS)
if (!overridden) {
icodeset = ibuf;
codeset = buf;
sprintf(ibuf, "cp%d", GetConsoleCP());
sprintf(buf, "cp%d", GetConsoleOutputCP());
}
#endif
if (codeset) {
sys_stream = PySys_GetObject("stdin");
sys_isatty = PyObject_CallMethod(sys_stream, "isatty", "");
if (!sys_isatty)
PyErr_Clear();
if ((overridden ||
(sys_isatty && PyObject_IsTrue(sys_isatty))) &&
PyFile_Check(sys_stream)) {
if (!PyFile_SetEncodingAndErrors(sys_stream, icodeset, errors))
Py_FatalError("Cannot set codeset of stdin");
}
Py_XDECREF(sys_isatty);
sys_stream = PySys_GetObject("stdout");
sys_isatty = PyObject_CallMethod(sys_stream, "isatty", "");
if (!sys_isatty)
PyErr_Clear();
if ((overridden ||
(sys_isatty && PyObject_IsTrue(sys_isatty))) &&
PyFile_Check(sys_stream)) {
if (!PyFile_SetEncodingAndErrors(sys_stream, codeset, errors))
Py_FatalError("Cannot set codeset of stdout");
}
Py_XDECREF(sys_isatty);
sys_stream = PySys_GetObject("stderr");
sys_isatty = PyObject_CallMethod(sys_stream, "isatty", "");
if (!sys_isatty)
PyErr_Clear();
if((overridden ||
(sys_isatty && PyObject_IsTrue(sys_isatty))) &&
PyFile_Check(sys_stream)) {
if (!PyFile_SetEncodingAndErrors(sys_stream, codeset, errors))
Py_FatalError("Cannot set codeset of stderr");
}
Py_XDECREF(sys_isatty);
if (free_codeset)
free(codeset);
}
}
void
Py_Initialize(void) {
Py_InitializeEx(1);
}
#if defined(COUNT_ALLOCS)
extern void dump_counts(FILE*);
#endif
void
Py_Finalize(void) {
PyInterpreterState *interp;
PyThreadState *tstate;
if (!initialized)
return;
call_sys_exitfunc();
initialized = 0;
tstate = PyThreadState_GET();
interp = tstate->interp;
PyOS_FiniInterrupts();
PyType_ClearCache();
PyGC_Collect();
#if defined(COUNT_ALLOCS)
while (PyGC_Collect() > 0)
;
#endif
PyImport_Cleanup();
#if 0
PyGC_Collect();
#endif
_PyImport_Fini();
#if defined(COUNT_ALLOCS)
dump_counts(stdout);
#endif
PRINT_TOTAL_REFS();
#if defined(Py_TRACE_REFS)
if (Py_GETENV("PYTHONDUMPREFS"))
_Py_PrintReferences(stderr);
#endif
PyInterpreterState_Clear(interp);
_PyExc_Fini();
#if defined(WITH_THREAD)
_PyGILState_Fini();
#endif
PyThreadState_Swap(NULL);
PyInterpreterState_Delete(interp);
PyMethod_Fini();
PyFrame_Fini();
PyCFunction_Fini();
PyTuple_Fini();
PyList_Fini();
PySet_Fini();
PyString_Fini();
PyByteArray_Fini();
PyInt_Fini();
PyFloat_Fini();
PyDict_Fini();
#if defined(Py_USING_UNICODE)
_PyUnicode_Fini();
#endif
PyGrammar_RemoveAccelerators(&_PyParser_Grammar);
#if defined(Py_TRACE_REFS)
if (Py_GETENV("PYTHONDUMPREFS"))
_Py_PrintReferenceAddresses(stderr);
#endif
#if defined(PYMALLOC_DEBUG)
if (Py_GETENV("PYTHONMALLOCSTATS"))
_PyObject_DebugMallocStats();
#endif
call_ll_exitfuncs();
}
PyThreadState *
Py_NewInterpreter(void) {
PyInterpreterState *interp;
PyThreadState *tstate, *save_tstate;
PyObject *bimod, *sysmod;
if (!initialized)
Py_FatalError("Py_NewInterpreter: call Py_Initialize first");
interp = PyInterpreterState_New();
if (interp == NULL)
return NULL;
tstate = PyThreadState_New(interp);
if (tstate == NULL) {
PyInterpreterState_Delete(interp);
return NULL;
}
save_tstate = PyThreadState_Swap(tstate);
interp->modules = PyDict_New();
interp->modules_reloading = PyDict_New();
bimod = _PyImport_FindExtension("__builtin__", "__builtin__");
if (bimod != NULL) {
interp->builtins = PyModule_GetDict(bimod);
if (interp->builtins == NULL)
goto handle_error;
Py_INCREF(interp->builtins);
}
sysmod = _PyImport_FindExtension("sys", "sys");
if (bimod != NULL && sysmod != NULL) {
interp->sysdict = PyModule_GetDict(sysmod);
if (interp->sysdict == NULL)
goto handle_error;
Py_INCREF(interp->sysdict);
PySys_SetPath(Py_GetPath());
PyDict_SetItemString(interp->sysdict, "modules",
interp->modules);
_PyImportHooks_Init();
initmain();
if (!Py_NoSiteFlag)
initsite();
}
if (!PyErr_Occurred())
return tstate;
handle_error:
PyErr_Print();
PyThreadState_Clear(tstate);
PyThreadState_Swap(save_tstate);
PyThreadState_Delete(tstate);
PyInterpreterState_Delete(interp);
return NULL;
}
void
Py_EndInterpreter(PyThreadState *tstate) {
PyInterpreterState *interp = tstate->interp;
if (tstate != PyThreadState_GET())
Py_FatalError("Py_EndInterpreter: thread is not current");
if (tstate->frame != NULL)
Py_FatalError("Py_EndInterpreter: thread still has a frame");
if (tstate != interp->tstate_head || tstate->next != NULL)
Py_FatalError("Py_EndInterpreter: not the last thread");
PyImport_Cleanup();
PyInterpreterState_Clear(interp);
PyThreadState_Swap(NULL);
PyInterpreterState_Delete(interp);
}
static char *progname = "python";
void
Py_SetProgramName(char *pn) {
if (pn && *pn)
progname = pn;
}
char *
Py_GetProgramName(void) {
return progname;
}
static char *default_home = NULL;
void
Py_SetPythonHome(char *home) {
default_home = home;
}
char *
Py_GetPythonHome(void) {
char *home = default_home;
if (home == NULL && !Py_IgnoreEnvironmentFlag)
home = Py_GETENV("PYTHONHOME");
return home;
}
static void
initmain(void) {
PyObject *m, *d;
m = PyImport_AddModule("__main__");
if (m == NULL)
Py_FatalError("can't create __main__ module");
d = PyModule_GetDict(m);
if (PyDict_GetItemString(d, "__builtins__") == NULL) {
PyObject *bimod = PyImport_ImportModule("__builtin__");
if (bimod == NULL ||
PyDict_SetItemString(d, "__builtins__", bimod) != 0)
Py_FatalError("can't add __builtins__ to __main__");
Py_DECREF(bimod);
}
}
static void
initsite(void) {
PyObject *m, *f;
m = PyImport_ImportModule("site");
if (m == NULL) {
f = PySys_GetObject("stderr");
if (Py_VerboseFlag) {
PyFile_WriteString(
"'import site' failed; traceback:\n", f);
PyErr_Print();
} else {
PyFile_WriteString(
"'import site' failed; use -v for traceback\n", f);
PyErr_Clear();
}
} else {
Py_DECREF(m);
}
}
int
PyRun_AnyFileExFlags(FILE *fp, const char *filename, int closeit,
PyCompilerFlags *flags) {
if (filename == NULL)
filename = "???";
if (Py_FdIsInteractive(fp, filename)) {
int err = PyRun_InteractiveLoopFlags(fp, filename, flags);
if (closeit)
fclose(fp);
return err;
} else
return PyRun_SimpleFileExFlags(fp, filename, closeit, flags);
}
int
PyRun_InteractiveLoopFlags(FILE *fp, const char *filename, PyCompilerFlags *flags) {
PyObject *v;
int ret;
PyCompilerFlags local_flags;
if (flags == NULL) {
flags = &local_flags;
local_flags.cf_flags = 0;
}
v = PySys_GetObject("ps1");
if (v == NULL) {
PySys_SetObject("ps1", v = PyString_FromString(">>> "));
Py_XDECREF(v);
}
v = PySys_GetObject("ps2");
if (v == NULL) {
PySys_SetObject("ps2", v = PyString_FromString("... "));
Py_XDECREF(v);
}
for (;;) {
ret = PyRun_InteractiveOneFlags(fp, filename, flags);
PRINT_TOTAL_REFS();
if (ret == E_EOF)
return 0;
}
}
#if 0
#define PARSER_FLAGS(flags) ((flags) ? ((((flags)->cf_flags & PyCF_DONT_IMPLY_DEDENT) ? PyPARSE_DONT_IMPLY_DEDENT : 0)) : 0)
#endif
#if 1
#define PARSER_FLAGS(flags) ((flags) ? ((((flags)->cf_flags & PyCF_DONT_IMPLY_DEDENT) ? PyPARSE_DONT_IMPLY_DEDENT : 0) | (((flags)->cf_flags & CO_FUTURE_PRINT_FUNCTION) ? PyPARSE_PRINT_IS_FUNCTION : 0) | (((flags)->cf_flags & CO_FUTURE_UNICODE_LITERALS) ? PyPARSE_UNICODE_LITERALS : 0) ) : 0)
#endif
int
PyRun_InteractiveOneFlags(FILE *fp, const char *filename, PyCompilerFlags *flags) {
PyObject *m, *d, *v, *w;
mod_ty mod;
PyArena *arena;
char *ps1 = "", *ps2 = "";
int errcode = 0;
v = PySys_GetObject("ps1");
if (v != NULL) {
v = PyObject_Str(v);
if (v == NULL)
PyErr_Clear();
else if (PyString_Check(v))
ps1 = PyString_AsString(v);
}
w = PySys_GetObject("ps2");
if (w != NULL) {
w = PyObject_Str(w);
if (w == NULL)
PyErr_Clear();
else if (PyString_Check(w))
ps2 = PyString_AsString(w);
}
arena = PyArena_New();
if (arena == NULL) {
Py_XDECREF(v);
Py_XDECREF(w);
return -1;
}
mod = PyParser_ASTFromFile(fp, filename,
Py_single_input, ps1, ps2,
flags, &errcode, arena);
Py_XDECREF(v);
Py_XDECREF(w);
if (mod == NULL) {
PyArena_Free(arena);
if (errcode == E_EOF) {
PyErr_Clear();
return E_EOF;
}
PyErr_Print();
return -1;
}
m = PyImport_AddModule("__main__");
if (m == NULL) {
PyArena_Free(arena);
return -1;
}
d = PyModule_GetDict(m);
v = run_mod(mod, filename, d, d, flags, arena);
PyArena_Free(arena);
if (v == NULL) {
PyErr_Print();
return -1;
}
Py_DECREF(v);
if (Py_FlushLine())
PyErr_Clear();
return 0;
}
static int
maybe_pyc_file(FILE *fp, const char* filename, const char* ext, int closeit) {
if (strcmp(ext, ".pyc") == 0 || strcmp(ext, ".pyo") == 0)
return 1;
if (closeit) {
unsigned int halfmagic = PyImport_GetMagicNumber() & 0xFFFF;
unsigned char buf[2];
int ispyc = 0;
if (ftell(fp) == 0) {
if (fread(buf, 1, 2, fp) == 2 &&
((unsigned int)buf[1]<<8 | buf[0]) == halfmagic)
ispyc = 1;
rewind(fp);
}
return ispyc;
}
return 0;
}
int
PyRun_SimpleFileExFlags(FILE *fp, const char *filename, int closeit,
PyCompilerFlags *flags) {
PyObject *m, *d, *v;
const char *ext;
int set_file_name = 0, ret;
m = PyImport_AddModule("__main__");
if (m == NULL)
return -1;
d = PyModule_GetDict(m);
if (PyDict_GetItemString(d, "__file__") == NULL) {
PyObject *f = PyString_FromString(filename);
if (f == NULL)
return -1;
if (PyDict_SetItemString(d, "__file__", f) < 0) {
Py_DECREF(f);
return -1;
}
set_file_name = 1;
Py_DECREF(f);
}
ext = filename + strlen(filename) - 4;
if (maybe_pyc_file(fp, filename, ext, closeit)) {
if (closeit)
fclose(fp);
if ((fp = fopen(filename, "rb")) == NULL) {
fprintf(stderr, "python: Can't reopen .pyc file\n");
ret = -1;
goto done;
}
if (strcmp(ext, ".pyo") == 0)
Py_OptimizeFlag = 1;
v = run_pyc_file(fp, filename, d, d, flags);
} else {
v = PyRun_FileExFlags(fp, filename, Py_file_input, d, d,
closeit, flags);
}
if (v == NULL) {
PyErr_Print();
ret = -1;
goto done;
}
Py_DECREF(v);
if (Py_FlushLine())
PyErr_Clear();
ret = 0;
done:
if (set_file_name && PyDict_DelItemString(d, "__file__"))
PyErr_Clear();
return ret;
}
int
PyRun_SimpleStringFlags(const char *command, PyCompilerFlags *flags) {
PyObject *m, *d, *v;
m = PyImport_AddModule("__main__");
if (m == NULL)
return -1;
d = PyModule_GetDict(m);
v = PyRun_StringFlags(command, Py_file_input, d, d, flags);
if (v == NULL) {
PyErr_Print();
return -1;
}
Py_DECREF(v);
if (Py_FlushLine())
PyErr_Clear();
return 0;
}
static int
parse_syntax_error(PyObject *err, PyObject **message, const char **filename,
int *lineno, int *offset, const char **text) {
long hold;
PyObject *v;
if (PyTuple_Check(err))
return PyArg_ParseTuple(err, "O(ziiz)", message, filename,
lineno, offset, text);
if (! (v = PyObject_GetAttrString(err, "msg")))
goto finally;
*message = v;
if (!(v = PyObject_GetAttrString(err, "filename")))
goto finally;
if (v == Py_None)
*filename = NULL;
else if (! (*filename = PyString_AsString(v)))
goto finally;
Py_DECREF(v);
if (!(v = PyObject_GetAttrString(err, "lineno")))
goto finally;
hold = PyInt_AsLong(v);
Py_DECREF(v);
v = NULL;
if (hold < 0 && PyErr_Occurred())
goto finally;
*lineno = (int)hold;
if (!(v = PyObject_GetAttrString(err, "offset")))
goto finally;
if (v == Py_None) {
*offset = -1;
Py_DECREF(v);
v = NULL;
} else {
hold = PyInt_AsLong(v);
Py_DECREF(v);
v = NULL;
if (hold < 0 && PyErr_Occurred())
goto finally;
*offset = (int)hold;
}
if (!(v = PyObject_GetAttrString(err, "text")))
goto finally;
if (v == Py_None)
*text = NULL;
else if (! (*text = PyString_AsString(v)))
goto finally;
Py_DECREF(v);
return 1;
finally:
Py_XDECREF(v);
return 0;
}
void
PyErr_Print(void) {
PyErr_PrintEx(1);
}
static void
print_error_text(PyObject *f, int offset, const char *text) {
char *nl;
if (offset >= 0) {
if (offset > 0 && offset == (int)strlen(text))
offset--;
for (;;) {
nl = strchr(text, '\n');
if (nl == NULL || nl-text >= offset)
break;
offset -= (int)(nl+1-text);
text = nl+1;
}
while (*text == ' ' || *text == '\t') {
text++;
offset--;
}
}
PyFile_WriteString(" ", f);
PyFile_WriteString(text, f);
if (*text == '\0' || text[strlen(text)-1] != '\n')
PyFile_WriteString("\n", f);
if (offset == -1)
return;
PyFile_WriteString(" ", f);
offset--;
while (offset > 0) {
PyFile_WriteString(" ", f);
offset--;
}
PyFile_WriteString("^\n", f);
}
static void
handle_system_exit(void) {
PyObject *exception, *value, *tb;
int exitcode = 0;
if (Py_InspectFlag)
return;
PyErr_Fetch(&exception, &value, &tb);
if (Py_FlushLine())
PyErr_Clear();
fflush(stdout);
if (value == NULL || value == Py_None)
goto done;
if (PyExceptionInstance_Check(value)) {
PyObject *code = PyObject_GetAttrString(value, "code");
if (code) {
Py_DECREF(value);
value = code;
if (value == Py_None)
goto done;
}
}
if (PyInt_Check(value))
exitcode = (int)PyInt_AsLong(value);
else {
PyObject_Print(value, stderr, Py_PRINT_RAW);
PySys_WriteStderr("\n");
exitcode = 1;
}
done:
PyErr_Restore(exception, value, tb);
PyErr_Clear();
Py_Exit(exitcode);
}
void
PyErr_PrintEx(int set_sys_last_vars) {
PyObject *exception, *v, *tb, *hook;
if (PyErr_ExceptionMatches(PyExc_SystemExit)) {
handle_system_exit();
}
PyErr_Fetch(&exception, &v, &tb);
if (exception == NULL)
return;
PyErr_NormalizeException(&exception, &v, &tb);
if (exception == NULL)
return;
if (set_sys_last_vars) {
PySys_SetObject("last_type", exception);
PySys_SetObject("last_value", v);
PySys_SetObject("last_traceback", tb);
}
hook = PySys_GetObject("excepthook");
if (hook) {
PyObject *args = PyTuple_Pack(3,
exception, v, tb ? tb : Py_None);
PyObject *result = PyEval_CallObject(hook, args);
if (result == NULL) {
PyObject *exception2, *v2, *tb2;
if (PyErr_ExceptionMatches(PyExc_SystemExit)) {
handle_system_exit();
}
PyErr_Fetch(&exception2, &v2, &tb2);
PyErr_NormalizeException(&exception2, &v2, &tb2);
if (exception2 == NULL) {
exception2 = Py_None;
Py_INCREF(exception2);
}
if (v2 == NULL) {
v2 = Py_None;
Py_INCREF(v2);
}
if (Py_FlushLine())
PyErr_Clear();
fflush(stdout);
PySys_WriteStderr("Error in sys.excepthook:\n");
PyErr_Display(exception2, v2, tb2);
PySys_WriteStderr("\nOriginal exception was:\n");
PyErr_Display(exception, v, tb);
Py_DECREF(exception2);
Py_DECREF(v2);
Py_XDECREF(tb2);
}
Py_XDECREF(result);
Py_XDECREF(args);
} else {
PySys_WriteStderr("sys.excepthook is missing\n");
PyErr_Display(exception, v, tb);
}
Py_XDECREF(exception);
Py_XDECREF(v);
Py_XDECREF(tb);
}
void
PyErr_Display(PyObject *exception, PyObject *value, PyObject *tb) {
int err = 0;
PyObject *f = PySys_GetObject("stderr");
Py_INCREF(value);
if (f == NULL)
fprintf(stderr, "lost sys.stderr\n");
else {
if (Py_FlushLine())
PyErr_Clear();
fflush(stdout);
if (tb && tb != Py_None)
err = PyTraceBack_Print(tb, f);
if (err == 0 &&
PyObject_HasAttrString(value, "print_file_and_line")) {
PyObject *message;
const char *filename, *text;
int lineno, offset;
if (!parse_syntax_error(value, &message, &filename,
&lineno, &offset, &text))
PyErr_Clear();
else {
char buf[10];
PyFile_WriteString(" File \"", f);
if (filename == NULL)
PyFile_WriteString("<string>", f);
else
PyFile_WriteString(filename, f);
PyFile_WriteString("\", line ", f);
PyOS_snprintf(buf, sizeof(buf), "%d", lineno);
PyFile_WriteString(buf, f);
PyFile_WriteString("\n", f);
if (text != NULL)
print_error_text(f, offset, text);
Py_DECREF(value);
value = message;
if (PyErr_Occurred())
err = -1;
}
}
if (err) {
} else if (PyExceptionClass_Check(exception)) {
PyObject* moduleName;
char* className = PyExceptionClass_Name(exception);
if (className != NULL) {
char *dot = strrchr(className, '.');
if (dot != NULL)
className = dot+1;
}
moduleName = PyObject_GetAttrString(exception, "__module__");
if (moduleName == NULL)
err = PyFile_WriteString("<unknown>", f);
else {
char* modstr = PyString_AsString(moduleName);
if (modstr && strcmp(modstr, "exceptions")) {
err = PyFile_WriteString(modstr, f);
err += PyFile_WriteString(".", f);
}
Py_DECREF(moduleName);
}
if (err == 0) {
if (className == NULL)
err = PyFile_WriteString("<unknown>", f);
else
err = PyFile_WriteString(className, f);
}
} else
err = PyFile_WriteObject(exception, f, Py_PRINT_RAW);
if (err == 0 && (value != Py_None)) {
PyObject *s = PyObject_Str(value);
if (s == NULL)
err = -1;
else if (!PyString_Check(s) ||
PyString_GET_SIZE(s) != 0)
err = PyFile_WriteString(": ", f);
if (err == 0)
err = PyFile_WriteObject(s, f, Py_PRINT_RAW);
Py_XDECREF(s);
}
err += PyFile_WriteString("\n", f);
}
Py_DECREF(value);
if (err != 0)
PyErr_Clear();
}
PyObject *
PyRun_StringFlags(const char *str, int start, PyObject *globals,
PyObject *locals, PyCompilerFlags *flags) {
PyObject *ret = NULL;
mod_ty mod;
PyArena *arena = PyArena_New();
if (arena == NULL)
return NULL;
mod = PyParser_ASTFromString(str, "<string>", start, flags, arena);
if (mod != NULL)
ret = run_mod(mod, "<string>", globals, locals, flags, arena);
PyArena_Free(arena);
return ret;
}
PyObject *
PyRun_FileExFlags(FILE *fp, const char *filename, int start, PyObject *globals,
PyObject *locals, int closeit, PyCompilerFlags *flags) {
PyObject *ret;
mod_ty mod;
PyArena *arena = PyArena_New();
if (arena == NULL)
return NULL;
mod = PyParser_ASTFromFile(fp, filename, start, 0, 0,
flags, NULL, arena);
if (closeit)
fclose(fp);
if (mod == NULL) {
PyArena_Free(arena);
return NULL;
}
ret = run_mod(mod, filename, globals, locals, flags, arena);
PyArena_Free(arena);
return ret;
}
static PyObject *
run_mod(mod_ty mod, const char *filename, PyObject *globals, PyObject *locals,
PyCompilerFlags *flags, PyArena *arena) {
PyCodeObject *co;
PyObject *v;
co = PyAST_Compile(mod, filename, flags, arena);
if (co == NULL)
return NULL;
v = PyEval_EvalCode(co, globals, locals);
Py_DECREF(co);
return v;
}
static PyObject *
run_pyc_file(FILE *fp, const char *filename, PyObject *globals,
PyObject *locals, PyCompilerFlags *flags) {
PyCodeObject *co;
PyObject *v;
long magic;
long PyImport_GetMagicNumber(void);
magic = PyMarshal_ReadLongFromFile(fp);
if (magic != PyImport_GetMagicNumber()) {
PyErr_SetString(PyExc_RuntimeError,
"Bad magic number in .pyc file");
return NULL;
}
(void) PyMarshal_ReadLongFromFile(fp);
v = PyMarshal_ReadLastObjectFromFile(fp);
fclose(fp);
if (v == NULL || !PyCode_Check(v)) {
Py_XDECREF(v);
PyErr_SetString(PyExc_RuntimeError,
"Bad code object in .pyc file");
return NULL;
}
co = (PyCodeObject *)v;
v = PyEval_EvalCode(co, globals, locals);
if (v && flags)
flags->cf_flags |= (co->co_flags & PyCF_MASK);
Py_DECREF(co);
return v;
}
PyObject *
Py_CompileStringFlags(const char *str, const char *filename, int start,
PyCompilerFlags *flags) {
PyCodeObject *co;
mod_ty mod;
PyArena *arena = PyArena_New();
if (arena == NULL)
return NULL;
mod = PyParser_ASTFromString(str, filename, start, flags, arena);
if (mod == NULL) {
PyArena_Free(arena);
return NULL;
}
if (flags && (flags->cf_flags & PyCF_ONLY_AST)) {
PyObject *result = PyAST_mod2obj(mod);
PyArena_Free(arena);
return result;
}
co = PyAST_Compile(mod, filename, flags, arena);
PyArena_Free(arena);
return (PyObject *)co;
}
struct symtable *
Py_SymtableString(const char *str, const char *filename, int start) {
struct symtable *st;
mod_ty mod;
PyCompilerFlags flags;
PyArena *arena = PyArena_New();
if (arena == NULL)
return NULL;
flags.cf_flags = 0;
mod = PyParser_ASTFromString(str, filename, start, &flags, arena);
if (mod == NULL) {
PyArena_Free(arena);
return NULL;
}
st = PySymtable_Build(mod, filename, 0);
PyArena_Free(arena);
return st;
}
mod_ty
PyParser_ASTFromString(const char *s, const char *filename, int start,
PyCompilerFlags *flags, PyArena *arena) {
mod_ty mod;
PyCompilerFlags localflags;
perrdetail err;
int iflags = PARSER_FLAGS(flags);
node *n = PyParser_ParseStringFlagsFilenameEx(s, filename,
&_PyParser_Grammar, start, &err,
&iflags);
if (flags == NULL) {
localflags.cf_flags = 0;
flags = &localflags;
}
if (n) {
flags->cf_flags |= iflags & PyCF_MASK;
mod = PyAST_FromNode(n, flags, filename, arena);
PyNode_Free(n);
return mod;
} else {
err_input(&err);
return NULL;
}
}
mod_ty
PyParser_ASTFromFile(FILE *fp, const char *filename, int start, char *ps1,
char *ps2, PyCompilerFlags *flags, int *errcode,
PyArena *arena) {
mod_ty mod;
PyCompilerFlags localflags;
perrdetail err;
int iflags = PARSER_FLAGS(flags);
node *n = PyParser_ParseFileFlagsEx(fp, filename, &_PyParser_Grammar,
start, ps1, ps2, &err, &iflags);
if (flags == NULL) {
localflags.cf_flags = 0;
flags = &localflags;
}
if (n) {
flags->cf_flags |= iflags & PyCF_MASK;
mod = PyAST_FromNode(n, flags, filename, arena);
PyNode_Free(n);
return mod;
} else {
err_input(&err);
if (errcode)
*errcode = err.error;
return NULL;
}
}
node *
PyParser_SimpleParseFileFlags(FILE *fp, const char *filename, int start, int flags) {
perrdetail err;
node *n = PyParser_ParseFileFlags(fp, filename, &_PyParser_Grammar,
start, NULL, NULL, &err, flags);
if (n == NULL)
err_input(&err);
return n;
}
node *
PyParser_SimpleParseStringFlags(const char *str, int start, int flags) {
perrdetail err;
node *n = PyParser_ParseStringFlags(str, &_PyParser_Grammar,
start, &err, flags);
if (n == NULL)
err_input(&err);
return n;
}
node *
PyParser_SimpleParseStringFlagsFilename(const char *str, const char *filename,
int start, int flags) {
perrdetail err;
node *n = PyParser_ParseStringFlagsFilename(str, filename,
&_PyParser_Grammar, start, &err, flags);
if (n == NULL)
err_input(&err);
return n;
}
node *
PyParser_SimpleParseStringFilename(const char *str, const char *filename, int start) {
return PyParser_SimpleParseStringFlagsFilename(str, filename, start, 0);
}
void
PyParser_SetError(perrdetail *err) {
err_input(err);
}
static void
err_input(perrdetail *err) {
PyObject *v, *w, *errtype;
PyObject* u = NULL;
char *msg = NULL;
errtype = PyExc_SyntaxError;
switch (err->error) {
case E_SYNTAX:
errtype = PyExc_IndentationError;
if (err->expected == INDENT)
msg = "expected an indented block";
else if (err->token == INDENT)
msg = "unexpected indent";
else if (err->token == DEDENT)
msg = "unexpected unindent";
else {
errtype = PyExc_SyntaxError;
msg = "invalid syntax";
}
break;
case E_TOKEN:
msg = "invalid token";
break;
case E_EOFS:
msg = "EOF while scanning triple-quoted string literal";
break;
case E_EOLS:
msg = "EOL while scanning string literal";
break;
case E_INTR:
if (!PyErr_Occurred())
PyErr_SetNone(PyExc_KeyboardInterrupt);
goto cleanup;
case E_NOMEM:
PyErr_NoMemory();
goto cleanup;
case E_EOF:
msg = "unexpected EOF while parsing";
break;
case E_TABSPACE:
errtype = PyExc_TabError;
msg = "inconsistent use of tabs and spaces in indentation";
break;
case E_OVERFLOW:
msg = "expression too long";
break;
case E_DEDENT:
errtype = PyExc_IndentationError;
msg = "unindent does not match any outer indentation level";
break;
case E_TOODEEP:
errtype = PyExc_IndentationError;
msg = "too many levels of indentation";
break;
case E_DECODE: {
PyObject *type, *value, *tb;
PyErr_Fetch(&type, &value, &tb);
if (value != NULL) {
u = PyObject_Str(value);
if (u != NULL) {
msg = PyString_AsString(u);
}
}
if (msg == NULL)
msg = "unknown decode error";
Py_XDECREF(type);
Py_XDECREF(value);
Py_XDECREF(tb);
break;
}
case E_LINECONT:
msg = "unexpected character after line continuation character";
break;
default:
fprintf(stderr, "error=%d\n", err->error);
msg = "unknown parsing error";
break;
}
v = Py_BuildValue("(ziiz)", err->filename,
err->lineno, err->offset, err->text);
w = NULL;
if (v != NULL)
w = Py_BuildValue("(sO)", msg, v);
Py_XDECREF(u);
Py_XDECREF(v);
PyErr_SetObject(errtype, w);
Py_XDECREF(w);
cleanup:
if (err->text != NULL) {
PyObject_FREE(err->text);
err->text = NULL;
}
}
void
Py_FatalError(const char *msg) {
fprintf(stderr, "Fatal Python error: %s\n", msg);
#if defined(MS_WINDOWS)
OutputDebugString("Fatal Python error: ");
OutputDebugString(msg);
OutputDebugString("\n");
#if defined(_DEBUG)
DebugBreak();
#endif
#endif
abort();
}
#if defined(WITH_THREAD)
#include "pythread.h"
#endif
#define NEXITFUNCS 32
static void (*exitfuncs[NEXITFUNCS])(void);
static int nexitfuncs = 0;
int Py_AtExit(void (*func)(void)) {
if (nexitfuncs >= NEXITFUNCS)
return -1;
exitfuncs[nexitfuncs++] = func;
return 0;
}
static void
call_sys_exitfunc(void) {
PyObject *exitfunc = PySys_GetObject("exitfunc");
if (exitfunc) {
PyObject *res;
Py_INCREF(exitfunc);
PySys_SetObject("exitfunc", (PyObject *)NULL);
res = PyEval_CallObject(exitfunc, (PyObject *)NULL);
if (res == NULL) {
if (!PyErr_ExceptionMatches(PyExc_SystemExit)) {
PySys_WriteStderr("Error in sys.exitfunc:\n");
}
PyErr_Print();
}
Py_DECREF(exitfunc);
}
if (Py_FlushLine())
PyErr_Clear();
}
static void
call_ll_exitfuncs(void) {
while (nexitfuncs > 0)
(*exitfuncs[--nexitfuncs])();
fflush(stdout);
fflush(stderr);
}
void
Py_Exit(int sts) {
Py_Finalize();
exit(sts);
}
static void
initsigs(void) {
#if defined(SIGPIPE)
PyOS_setsig(SIGPIPE, SIG_IGN);
#endif
#if defined(SIGXFZ)
PyOS_setsig(SIGXFZ, SIG_IGN);
#endif
#if defined(SIGXFSZ)
PyOS_setsig(SIGXFSZ, SIG_IGN);
#endif
PyOS_InitInterrupts();
}
int
Py_FdIsInteractive(FILE *fp, const char *filename) {
if (isatty((int)fileno(fp)))
return 1;
if (!Py_InteractiveFlag)
return 0;
return (filename == NULL) ||
(strcmp(filename, "<stdin>") == 0) ||
(strcmp(filename, "???") == 0);
}
#if defined(USE_STACKCHECK)
#if defined(WIN32) && defined(_MSC_VER)
#include <malloc.h>
#include <excpt.h>
int
PyOS_CheckStack(void) {
__try {
alloca(PYOS_STACK_MARGIN * sizeof(void*));
return 0;
} __except (GetExceptionCode() == STATUS_STACK_OVERFLOW ?
EXCEPTION_EXECUTE_HANDLER :
EXCEPTION_CONTINUE_SEARCH) {
int errcode = _resetstkoflw();
if (errcode == 0) {
Py_FatalError("Could not reset the stack!");
}
}
return 1;
}
#endif
#endif
PyOS_sighandler_t
PyOS_getsig(int sig) {
#if defined(HAVE_SIGACTION)
struct sigaction context;
if (sigaction(sig, NULL, &context) == -1)
return SIG_ERR;
return context.sa_handler;
#else
PyOS_sighandler_t handler;
#if defined(_MSC_VER) && _MSC_VER >= 1400
switch (sig) {
case SIGINT:
case SIGILL:
case SIGFPE:
case SIGSEGV:
case SIGTERM:
case SIGBREAK:
case SIGABRT:
break;
default:
return SIG_ERR;
}
#endif
handler = signal(sig, SIG_IGN);
if (handler != SIG_ERR)
signal(sig, handler);
return handler;
#endif
}
PyOS_sighandler_t
PyOS_setsig(int sig, PyOS_sighandler_t handler) {
#if defined(HAVE_SIGACTION)
struct sigaction context, ocontext;
context.sa_handler = handler;
sigemptyset(&context.sa_mask);
context.sa_flags = 0;
if (sigaction(sig, &context, &ocontext) == -1)
return SIG_ERR;
return ocontext.sa_handler;
#else
PyOS_sighandler_t oldhandler;
oldhandler = signal(sig, handler);
#if defined(HAVE_SIGINTERRUPT)
siginterrupt(sig, 1);
#endif
return oldhandler;
#endif
}
#undef PyParser_SimpleParseFile
PyAPI_FUNC(node *)
PyParser_SimpleParseFile(FILE *fp, const char *filename, int start) {
return PyParser_SimpleParseFileFlags(fp, filename, start, 0);
}
#undef PyParser_SimpleParseString
PyAPI_FUNC(node *)
PyParser_SimpleParseString(const char *str, int start) {
return PyParser_SimpleParseStringFlags(str, start, 0);
}
#undef PyRun_AnyFile
PyAPI_FUNC(int)
PyRun_AnyFile(FILE *fp, const char *name) {
return PyRun_AnyFileExFlags(fp, name, 0, NULL);
}
#undef PyRun_AnyFileEx
PyAPI_FUNC(int)
PyRun_AnyFileEx(FILE *fp, const char *name, int closeit) {
return PyRun_AnyFileExFlags(fp, name, closeit, NULL);
}
#undef PyRun_AnyFileFlags
PyAPI_FUNC(int)
PyRun_AnyFileFlags(FILE *fp, const char *name, PyCompilerFlags *flags) {
return PyRun_AnyFileExFlags(fp, name, 0, flags);
}
#undef PyRun_File
PyAPI_FUNC(PyObject *)
PyRun_File(FILE *fp, const char *p, int s, PyObject *g, PyObject *l) {
return PyRun_FileExFlags(fp, p, s, g, l, 0, NULL);
}
#undef PyRun_FileEx
PyAPI_FUNC(PyObject *)
PyRun_FileEx(FILE *fp, const char *p, int s, PyObject *g, PyObject *l, int c) {
return PyRun_FileExFlags(fp, p, s, g, l, c, NULL);
}
#undef PyRun_FileFlags
PyAPI_FUNC(PyObject *)
PyRun_FileFlags(FILE *fp, const char *p, int s, PyObject *g, PyObject *l,
PyCompilerFlags *flags) {
return PyRun_FileExFlags(fp, p, s, g, l, 0, flags);
}
#undef PyRun_SimpleFile
PyAPI_FUNC(int)
PyRun_SimpleFile(FILE *f, const char *p) {
return PyRun_SimpleFileExFlags(f, p, 0, NULL);
}
#undef PyRun_SimpleFileEx
PyAPI_FUNC(int)
PyRun_SimpleFileEx(FILE *f, const char *p, int c) {
return PyRun_SimpleFileExFlags(f, p, c, NULL);
}
#undef PyRun_String
PyAPI_FUNC(PyObject *)
PyRun_String(const char *str, int s, PyObject *g, PyObject *l) {
return PyRun_StringFlags(str, s, g, l, NULL);
}
#undef PyRun_SimpleString
PyAPI_FUNC(int)
PyRun_SimpleString(const char *s) {
return PyRun_SimpleStringFlags(s, NULL);
}
#undef Py_CompileString
PyAPI_FUNC(PyObject *)
Py_CompileString(const char *str, const char *p, int s) {
return Py_CompileStringFlags(str, p, s, NULL);
}
#undef PyRun_InteractiveOne
PyAPI_FUNC(int)
PyRun_InteractiveOne(FILE *f, const char *p) {
return PyRun_InteractiveOneFlags(f, p, NULL);
}
#undef PyRun_InteractiveLoop
PyAPI_FUNC(int)
PyRun_InteractiveLoop(FILE *f, const char *p) {
return PyRun_InteractiveLoopFlags(f, p, NULL);
}
#if defined(__cplusplus)
}
#endif