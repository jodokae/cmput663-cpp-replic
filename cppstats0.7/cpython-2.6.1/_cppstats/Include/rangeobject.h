#if !defined(Py_RANGEOBJECT_H)
#define Py_RANGEOBJECT_H
#if defined(__cplusplus)
extern "C" {
#endif
PyAPI_DATA(PyTypeObject) PyRange_Type;
#define PyRange_Check(op) (Py_TYPE(op) == &PyRange_Type)
#if defined(__cplusplus)
}
#endif
#endif
