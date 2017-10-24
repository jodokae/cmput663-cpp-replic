#if !defined(Py_PYARENA_H)
#define Py_PYARENA_H
#if defined(__cplusplus)
extern "C" {
#endif
typedef struct _arena PyArena;
PyAPI_FUNC(PyArena *) PyArena_New(void);
PyAPI_FUNC(void) PyArena_Free(PyArena *);
PyAPI_FUNC(void *) PyArena_Malloc(PyArena *, size_t size);
PyAPI_FUNC(int) PyArena_AddPyObject(PyArena *, PyObject *);
#if defined(__cplusplus)
}
#endif
#endif