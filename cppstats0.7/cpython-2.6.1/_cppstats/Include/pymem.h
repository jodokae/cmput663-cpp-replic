#if !defined(Py_PYMEM_H)
#define Py_PYMEM_H
#include "pyport.h"
#if defined(__cplusplus)
extern "C" {
#endif
PyAPI_FUNC(void *) PyMem_Malloc(size_t);
PyAPI_FUNC(void *) PyMem_Realloc(void *, size_t);
PyAPI_FUNC(void) PyMem_Free(void *);
#if defined(PYMALLOC_DEBUG)
#define PyMem_MALLOC PyObject_MALLOC
#define PyMem_REALLOC PyObject_REALLOC
#define PyMem_FREE PyObject_FREE
#else
#define PyMem_MALLOC(n) (((n) < 0 || (n) > PY_SSIZE_T_MAX) ? NULL : malloc((n) ? (n) : 1))
#define PyMem_REALLOC(p, n) (((n) < 0 || (n) > PY_SSIZE_T_MAX) ? NULL : realloc((p), (n) ? (n) : 1))
#define PyMem_FREE free
#endif
#define PyMem_New(type, n) ( ((n) > PY_SSIZE_T_MAX / sizeof(type)) ? NULL : ( (type *) PyMem_Malloc((n) * sizeof(type)) ) )
#define PyMem_NEW(type, n) ( ((n) > PY_SSIZE_T_MAX / sizeof(type)) ? NULL : ( (type *) PyMem_MALLOC((n) * sizeof(type)) ) )
#define PyMem_Resize(p, type, n) ( (p) = ((n) > PY_SSIZE_T_MAX / sizeof(type)) ? NULL : (type *) PyMem_Realloc((p), (n) * sizeof(type)) )
#define PyMem_RESIZE(p, type, n) ( (p) = ((n) > PY_SSIZE_T_MAX / sizeof(type)) ? NULL : (type *) PyMem_REALLOC((p), (n) * sizeof(type)) )
#define PyMem_Del PyMem_Free
#define PyMem_DEL PyMem_FREE
#if defined(__cplusplus)
}
#endif
#endif
