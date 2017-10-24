#if !defined(Py_CSTRINGIO_H)
#define Py_CSTRINGIO_H
#if defined(__cplusplus)
extern "C" {
#endif
#define PycString_IMPORT PycStringIO = (struct PycStringIO_CAPI*)PyCObject_Import("cStringIO", "cStringIO_CAPI")
static struct PycStringIO_CAPI {
int(*cread)(PyObject *, char **, Py_ssize_t);
int(*creadline)(PyObject *, char **);
int(*cwrite)(PyObject *, const char *, Py_ssize_t);
PyObject *(*cgetvalue)(PyObject *);
PyObject *(*NewOutput)(int);
PyObject *(*NewInput)(PyObject *);
PyTypeObject *InputType, *OutputType;
} *PycStringIO;
#define PycStringIO_InputCheck(O) (Py_TYPE(O)==PycStringIO->InputType)
#define PycStringIO_OutputCheck(O) (Py_TYPE(O)==PycStringIO->OutputType)
#if defined(__cplusplus)
}
#endif
#endif
