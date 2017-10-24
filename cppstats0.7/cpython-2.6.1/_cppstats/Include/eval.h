#if !defined(Py_EVAL_H)
#define Py_EVAL_H
#if defined(__cplusplus)
extern "C" {
#endif
PyAPI_FUNC(PyObject *) PyEval_EvalCode(PyCodeObject *, PyObject *, PyObject *);
PyAPI_FUNC(PyObject *) PyEval_EvalCodeEx(PyCodeObject *co,
PyObject *globals,
PyObject *locals,
PyObject **args, int argc,
PyObject **kwds, int kwdc,
PyObject **defs, int defc,
PyObject *closure);
PyAPI_FUNC(PyObject *) _PyEval_CallTracing(PyObject *func, PyObject *args);
#if defined(__cplusplus)
}
#endif
#endif