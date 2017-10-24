#if !defined(Py_TRACEBACK_H)
#define Py_TRACEBACK_H
#if defined(__cplusplus)
extern "C" {
#endif
struct _frame;
typedef struct _traceback {
PyObject_HEAD
struct _traceback *tb_next;
struct _frame *tb_frame;
int tb_lasti;
int tb_lineno;
} PyTracebackObject;
PyAPI_FUNC(int) PyTraceBack_Here(struct _frame *);
PyAPI_FUNC(int) PyTraceBack_Print(PyObject *, PyObject *);
PyAPI_FUNC(int) _Py_DisplaySourceLine(PyObject *, const char *, int, int);
PyAPI_DATA(PyTypeObject) PyTraceBack_Type;
#define PyTraceBack_Check(v) (Py_TYPE(v) == &PyTraceBack_Type)
#if defined(__cplusplus)
}
#endif
#endif