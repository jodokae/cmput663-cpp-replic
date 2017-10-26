#if !defined(Py_ITEROBJECT_H)
#define Py_ITEROBJECT_H
#if defined(__cplusplus)
extern "C" {
#endif
PyAPI_DATA(PyTypeObject) PySeqIter_Type;
#define PySeqIter_Check(op) (Py_TYPE(op) == &PySeqIter_Type)
PyAPI_FUNC(PyObject *) PySeqIter_New(PyObject *);
PyAPI_DATA(PyTypeObject) PyCallIter_Type;
#define PyCallIter_Check(op) (Py_TYPE(op) == &PyCallIter_Type)
PyAPI_FUNC(PyObject *) PyCallIter_New(PyObject *, PyObject *);
#if defined(__cplusplus)
}
#endif
#endif
