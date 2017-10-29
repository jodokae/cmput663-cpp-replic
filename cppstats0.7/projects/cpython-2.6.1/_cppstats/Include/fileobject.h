#if !defined(Py_FILEOBJECT_H)
#define Py_FILEOBJECT_H
#if defined(__cplusplus)
extern "C" {
#endif
typedef struct {
PyObject_HEAD
FILE *f_fp;
PyObject *f_name;
PyObject *f_mode;
int (*f_close)(FILE *);
int f_softspace;
int f_binary;
char* f_buf;
char* f_bufend;
char* f_bufptr;
char *f_setbuf;
int f_univ_newline;
int f_newlinetypes;
int f_skipnextlf;
PyObject *f_encoding;
PyObject *f_errors;
PyObject *weakreflist;
int unlocked_count;
} PyFileObject;
PyAPI_DATA(PyTypeObject) PyFile_Type;
#define PyFile_Check(op) PyObject_TypeCheck(op, &PyFile_Type)
#define PyFile_CheckExact(op) (Py_TYPE(op) == &PyFile_Type)
PyAPI_FUNC(PyObject *) PyFile_FromString(char *, char *);
PyAPI_FUNC(void) PyFile_SetBufSize(PyObject *, int);
PyAPI_FUNC(int) PyFile_SetEncoding(PyObject *, const char *);
PyAPI_FUNC(int) PyFile_SetEncodingAndErrors(PyObject *, const char *, char *errors);
PyAPI_FUNC(PyObject *) PyFile_FromFile(FILE *, char *, char *,
int (*)(FILE *));
PyAPI_FUNC(FILE *) PyFile_AsFile(PyObject *);
PyAPI_FUNC(void) PyFile_IncUseCount(PyFileObject *);
PyAPI_FUNC(void) PyFile_DecUseCount(PyFileObject *);
PyAPI_FUNC(PyObject *) PyFile_Name(PyObject *);
PyAPI_FUNC(PyObject *) PyFile_GetLine(PyObject *, int);
PyAPI_FUNC(int) PyFile_WriteObject(PyObject *, PyObject *, int);
PyAPI_FUNC(int) PyFile_SoftSpace(PyObject *, int);
PyAPI_FUNC(int) PyFile_WriteString(const char *, PyObject *);
PyAPI_FUNC(int) PyObject_AsFileDescriptor(PyObject *);
PyAPI_DATA(const char *) Py_FileSystemDefaultEncoding;
#define PY_STDIOTEXTMODE "b"
char *Py_UniversalNewlineFgets(char *, int, FILE*, PyObject *);
size_t Py_UniversalNewlineFread(char *, size_t, FILE *, PyObject *);
int _PyFile_SanitizeMode(char *mode);
#if defined(__cplusplus)
}
#endif
#endif
