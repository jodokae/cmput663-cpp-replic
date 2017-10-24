#if !defined(Py_CELLOBJECT_H)
#define Py_CELLOBJECT_H
#if defined(__cplusplus)
extern "C" {
#endif
typedef struct {
PyObject_HEAD
PyObject *ob_ref;
} PyCellObject;
PyAPI_DATA(PyTypeObject) PyCell_Type;
#define PyCell_Check(op) (Py_TYPE(op) == &PyCell_Type)
PyAPI_FUNC(PyObject *) PyCell_New(PyObject *);
PyAPI_FUNC(PyObject *) PyCell_Get(PyObject *);
PyAPI_FUNC(int) PyCell_Set(PyObject *, PyObject *);
#define PyCell_GET(op) (((PyCellObject *)(op))->ob_ref)
#define PyCell_SET(op, v) (((PyCellObject *)(op))->ob_ref = v)
#if defined(__cplusplus)
}
#endif
#endif