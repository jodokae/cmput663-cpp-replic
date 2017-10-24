#if defined (__SVR4) && defined (__sun)
#include <alloca.h>
#endif
#if (PY_VERSION_HEX < 0x02040000)
#define PyDict_CheckExact(ob) (Py_TYPE(ob) == &PyDict_Type)
#endif
#if (PY_VERSION_HEX < 0x02050000)
typedef int Py_ssize_t;
#define PyInt_FromSsize_t PyInt_FromLong
#define PyNumber_AsSsize_t(ob, exc) PyInt_AsLong(ob)
#define PyIndex_Check(ob) PyInt_Check(ob)
typedef Py_ssize_t (*readbufferproc)(PyObject *, Py_ssize_t, void **);
typedef Py_ssize_t (*writebufferproc)(PyObject *, Py_ssize_t, void **);
typedef Py_ssize_t (*segcountproc)(PyObject *, Py_ssize_t *);
typedef Py_ssize_t (*charbufferproc)(PyObject *, Py_ssize_t, char **);
#endif
#if (PY_VERSION_HEX < 0x02060000)
#define Py_TYPE(ob) (((PyObject*)(ob))->ob_type)
#define PyVarObject_HEAD_INIT(type, size) PyObject_HEAD_INIT(type) size,
#define PyImport_ImportModuleNoBlock PyImport_ImportModule
#define PyLong_FromSsize_t PyInt_FromLong
#define Py_TPFLAGS_HAVE_NEWBUFFER 0
#endif
#if !defined(MS_WIN32)
#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) < (b) ? (a) : (b))
#define PARAMFLAG_FIN 0x1
#define PARAMFLAG_FOUT 0x2
#define PARAMFLAG_FLCID 0x4
#endif
#if defined(HAVE_LONG_LONG) && !defined(PY_LONG_LONG)
#define PY_LONG_LONG LONG_LONG
#endif
typedef struct tagPyCArgObject PyCArgObject;
typedef struct tagCDataObject CDataObject;
typedef PyObject *(* GETFUNC)(void *, Py_ssize_t size);
typedef PyObject *(* SETFUNC)(void *, PyObject *value, Py_ssize_t size);
typedef PyCArgObject *(* PARAMFUNC)(CDataObject *obj);
union value {
char c[16];
short s;
int i;
long l;
float f;
double d;
#if defined(HAVE_LONG_LONG)
PY_LONG_LONG ll;
#endif
long double D;
};
struct tagCDataObject {
PyObject_HEAD
char *b_ptr;
int b_needsfree;
CDataObject *b_base;
Py_ssize_t b_size;
Py_ssize_t b_length;
Py_ssize_t b_index;
PyObject *b_objects;
union value b_value;
};
typedef struct {
PyObject_VAR_HEAD
ffi_closure *pcl;
ffi_cif cif;
int flags;
PyObject *converters;
PyObject *callable;
PyObject *restype;
SETFUNC setfunc;
ffi_type *ffi_restype;
ffi_type *atypes[1];
} CThunkObject;
extern PyTypeObject CThunk_Type;
#define CThunk_CheckExact(v) ((v)->ob_type == &CThunk_Type)
typedef struct {
PyObject_HEAD
char *b_ptr;
int b_needsfree;
CDataObject *b_base;
Py_ssize_t b_size;
Py_ssize_t b_length;
Py_ssize_t b_index;
PyObject *b_objects;
union value b_value;
CThunkObject *thunk;
PyObject *callable;
PyObject *converters;
PyObject *argtypes;
PyObject *restype;
PyObject *checker;
PyObject *errcheck;
#if defined(MS_WIN32)
int index;
GUID *iid;
#endif
PyObject *paramflags;
} CFuncPtrObject;
extern PyTypeObject StgDict_Type;
#define StgDict_CheckExact(v) ((v)->ob_type == &StgDict_Type)
#define StgDict_Check(v) PyObject_TypeCheck(v, &StgDict_Type)
extern int StructUnionType_update_stgdict(PyObject *fields, PyObject *type, int isStruct);
extern int PyType_stginfo(PyTypeObject *self, Py_ssize_t *psize, Py_ssize_t *palign, Py_ssize_t *plength);
extern int PyObject_stginfo(PyObject *self, Py_ssize_t *psize, Py_ssize_t *palign, Py_ssize_t *plength);
extern PyTypeObject CData_Type;
#define CDataObject_CheckExact(v) ((v)->ob_type == &CData_Type)
#define CDataObject_Check(v) PyObject_TypeCheck(v, &CData_Type)
extern PyTypeObject SimpleType_Type;
#define SimpleTypeObject_CheckExact(v) ((v)->ob_type == &SimpleType_Type)
#define SimpleTypeObject_Check(v) PyObject_TypeCheck(v, &SimpleType_Type)
extern PyTypeObject CField_Type;
extern struct fielddesc *getentry(char *fmt);
extern PyObject *
CField_FromDesc(PyObject *desc, Py_ssize_t index,
Py_ssize_t *pfield_size, int bitsize, int *pbitofs,
Py_ssize_t *psize, Py_ssize_t *poffset, Py_ssize_t *palign,
int pack, int is_big_endian);
extern PyObject *CData_AtAddress(PyObject *type, void *buf);
extern PyObject *CData_FromBytes(PyObject *type, char *data, Py_ssize_t length);
extern PyTypeObject ArrayType_Type;
extern PyTypeObject Array_Type;
extern PyTypeObject PointerType_Type;
extern PyTypeObject Pointer_Type;
extern PyTypeObject CFuncPtr_Type;
extern PyTypeObject CFuncPtrType_Type;
extern PyTypeObject StructType_Type;
#define ArrayTypeObject_Check(v) PyObject_TypeCheck(v, &ArrayType_Type)
#define ArrayObject_Check(v) PyObject_TypeCheck(v, &Array_Type)
#define PointerObject_Check(v) PyObject_TypeCheck(v, &Pointer_Type)
#define PointerTypeObject_Check(v) PyObject_TypeCheck(v, &PointerType_Type)
#define CFuncPtrObject_Check(v) PyObject_TypeCheck(v, &CFuncPtr_Type)
#define CFuncPtrTypeObject_Check(v) PyObject_TypeCheck(v, &CFuncPtrType_Type)
#define StructTypeObject_Check(v) PyObject_TypeCheck(v, &StructType_Type)
extern PyObject *
CreateArrayType(PyObject *itemtype, Py_ssize_t length);
extern void init_callbacks_in_module(PyObject *m);
extern PyMethodDef module_methods[];
extern CThunkObject *AllocFunctionCallback(PyObject *callable,
PyObject *converters,
PyObject *restype,
int flags);
struct fielddesc {
char code;
SETFUNC setfunc;
GETFUNC getfunc;
ffi_type *pffi_type;
SETFUNC setfunc_swapped;
GETFUNC getfunc_swapped;
};
typedef struct {
PyObject_HEAD
Py_ssize_t offset;
Py_ssize_t size;
Py_ssize_t index;
PyObject *proto;
GETFUNC getfunc;
SETFUNC setfunc;
int anonymous;
} CFieldObject;
typedef struct {
PyDictObject dict;
Py_ssize_t size;
Py_ssize_t align;
Py_ssize_t length;
ffi_type ffi_type_pointer;
PyObject *proto;
SETFUNC setfunc;
GETFUNC getfunc;
PARAMFUNC paramfunc;
PyObject *argtypes;
PyObject *converters;
PyObject *restype;
PyObject *checker;
int flags;
char *format;
int ndim;
Py_ssize_t *shape;
} StgDictObject;
extern StgDictObject *PyType_stgdict(PyObject *obj);
extern StgDictObject *PyObject_stgdict(PyObject *self);
extern int StgDict_clone(StgDictObject *src, StgDictObject *dst);
typedef int(* PPROC)(void);
PyObject *_CallProc(PPROC pProc,
PyObject *arguments,
#if defined(MS_WIN32)
IUnknown *pIUnk,
GUID *iid,
#endif
int flags,
PyObject *argtypes,
PyObject *restype,
PyObject *checker);
#define FUNCFLAG_STDCALL 0x0
#define FUNCFLAG_CDECL 0x1
#define FUNCFLAG_HRESULT 0x2
#define FUNCFLAG_PYTHONAPI 0x4
#define FUNCFLAG_USE_ERRNO 0x8
#define FUNCFLAG_USE_LASTERROR 0x10
#define TYPEFLAG_ISPOINTER 0x100
#define TYPEFLAG_HASPOINTER 0x200
#define DICTFLAG_FINAL 0x1000
struct tagPyCArgObject {
PyObject_HEAD
ffi_type *pffi_type;
char tag;
union {
char c;
char b;
short h;
int i;
long l;
#if defined(HAVE_LONG_LONG)
PY_LONG_LONG q;
#endif
long double D;
double d;
float f;
void *p;
} value;
PyObject *obj;
Py_ssize_t size;
};
extern PyTypeObject PyCArg_Type;
extern PyCArgObject *new_CArgObject(void);
#define PyCArg_CheckExact(v) ((v)->ob_type == &PyCArg_Type)
extern PyCArgObject *new_CArgObject(void);
extern PyObject *
CData_get(PyObject *type, GETFUNC getfunc, PyObject *src,
Py_ssize_t index, Py_ssize_t size, char *ptr);
extern int
CData_set(PyObject *dst, PyObject *type, SETFUNC setfunc, PyObject *value,
Py_ssize_t index, Py_ssize_t size, char *ptr);
extern void Extend_Error_Info(PyObject *exc_class, char *fmt, ...);
struct basespec {
CDataObject *base;
Py_ssize_t index;
char *adr;
};
extern char basespec_string[];
extern ffi_type *GetType(PyObject *obj);
extern PyObject *PyExc_ArgError;
extern char *conversion_mode_encoding;
extern char *conversion_mode_errors;
#if !defined(Py_CLEAR)
#define Py_CLEAR(op) do { if (op) { PyObject *tmp = (PyObject *)(op); (op) = NULL; Py_DECREF(tmp); } } while (0)
#endif
#if !defined(Py_VISIT)
#define Py_VISIT(op) do { if (op) { int vret = visit((op), arg); if (vret) return vret; } } while (0)
#endif
#if defined(Py_USING_UNICODE) && defined(HAVE_WCHAR_H)
#define CTYPES_UNICODE
#endif
#if defined(CTYPES_UNICODE)
#undef PyUnicode_FromWideChar
#define PyUnicode_FromWideChar My_PyUnicode_FromWideChar
#undef PyUnicode_AsWideChar
#define PyUnicode_AsWideChar My_PyUnicode_AsWideChar
extern PyObject *My_PyUnicode_FromWideChar(const wchar_t *, Py_ssize_t);
extern Py_ssize_t My_PyUnicode_AsWideChar(PyUnicodeObject *, wchar_t *, Py_ssize_t);
#endif
extern void FreeClosure(void *);
extern void *MallocClosure(void);
extern void _AddTraceback(char *, char *, int);
extern PyObject *CData_FromBaseObj(PyObject *type, PyObject *base, Py_ssize_t index, char *adr);
extern char *alloc_format_string(const char *prefix, const char *suffix);
extern int IsSimpleSubType(PyObject *obj);
extern PyObject *_pointer_type_cache;
PyObject *get_error_object(int **pspace);
#if defined(MS_WIN32)
extern PyObject *ComError;
#endif
