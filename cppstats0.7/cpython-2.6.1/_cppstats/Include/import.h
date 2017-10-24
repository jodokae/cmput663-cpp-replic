#if !defined(Py_IMPORT_H)
#define Py_IMPORT_H
#if defined(__cplusplus)
extern "C" {
#endif
PyAPI_FUNC(long) PyImport_GetMagicNumber(void);
PyAPI_FUNC(PyObject *) PyImport_ExecCodeModule(char *name, PyObject *co);
PyAPI_FUNC(PyObject *) PyImport_ExecCodeModuleEx(
char *name, PyObject *co, char *pathname);
PyAPI_FUNC(PyObject *) PyImport_GetModuleDict(void);
PyAPI_FUNC(PyObject *) PyImport_AddModule(const char *name);
PyAPI_FUNC(PyObject *) PyImport_ImportModule(const char *name);
PyAPI_FUNC(PyObject *) PyImport_ImportModuleNoBlock(const char *);
PyAPI_FUNC(PyObject *) PyImport_ImportModuleLevel(char *name,
PyObject *globals, PyObject *locals, PyObject *fromlist, int level);
#define PyImport_ImportModuleEx(n, g, l, f) PyImport_ImportModuleLevel(n, g, l, f, -1)
PyAPI_FUNC(PyObject *) PyImport_GetImporter(PyObject *path);
PyAPI_FUNC(PyObject *) PyImport_Import(PyObject *name);
PyAPI_FUNC(PyObject *) PyImport_ReloadModule(PyObject *m);
PyAPI_FUNC(void) PyImport_Cleanup(void);
PyAPI_FUNC(int) PyImport_ImportFrozenModule(char *);
PyAPI_FUNC(struct filedescr *) _PyImport_FindModule(
const char *, PyObject *, char *, size_t, FILE **, PyObject **);
PyAPI_FUNC(int) _PyImport_IsScript(struct filedescr *);
PyAPI_FUNC(void) _PyImport_ReInitLock(void);
PyAPI_FUNC(PyObject *)_PyImport_FindExtension(char *, char *);
PyAPI_FUNC(PyObject *)_PyImport_FixupExtension(char *, char *);
struct _inittab {
char *name;
void (*initfunc)(void);
};
PyAPI_DATA(PyTypeObject) PyNullImporter_Type;
PyAPI_DATA(struct _inittab *) PyImport_Inittab;
PyAPI_FUNC(int) PyImport_AppendInittab(char *name, void (*initfunc)(void));
PyAPI_FUNC(int) PyImport_ExtendInittab(struct _inittab *newtab);
struct _frozen {
char *name;
unsigned char *code;
int size;
};
PyAPI_DATA(struct _frozen *) PyImport_FrozenModules;
#if defined(__cplusplus)
}
#endif
#endif