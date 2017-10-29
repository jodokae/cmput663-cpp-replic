#if !defined(Py_BYTEARRAYOBJECT_H)
#define Py_BYTEARRAYOBJECT_H
#if defined(__cplusplus)
extern "C" {
#endif
#include <stdarg.h>
typedef struct {
PyObject_VAR_HEAD
int ob_exports;
Py_ssize_t ob_alloc;
char *ob_bytes;
} PyByteArrayObject;
PyAPI_DATA(PyTypeObject) PyByteArray_Type;
PyAPI_DATA(PyTypeObject) PyByteArrayIter_Type;
#define PyByteArray_Check(self) PyObject_TypeCheck(self, &PyByteArray_Type)
#define PyByteArray_CheckExact(self) (Py_TYPE(self) == &PyByteArray_Type)
PyAPI_FUNC(PyObject *) PyByteArray_FromObject(PyObject *);
PyAPI_FUNC(PyObject *) PyByteArray_Concat(PyObject *, PyObject *);
PyAPI_FUNC(PyObject *) PyByteArray_FromStringAndSize(const char *, Py_ssize_t);
PyAPI_FUNC(Py_ssize_t) PyByteArray_Size(PyObject *);
PyAPI_FUNC(char *) PyByteArray_AsString(PyObject *);
PyAPI_FUNC(int) PyByteArray_Resize(PyObject *, Py_ssize_t);
#define PyByteArray_AS_STRING(self) (assert(PyByteArray_Check(self)),((PyByteArrayObject *)(self))->ob_bytes)
#define PyByteArray_GET_SIZE(self) (assert(PyByteArray_Check(self)),Py_SIZE(self))
#if defined(__cplusplus)
}
#endif
#endif