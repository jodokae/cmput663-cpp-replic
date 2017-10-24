#if !defined(Py_LONGINTREPR_H)
#define Py_LONGINTREPR_H
#if defined(__cplusplus)
extern "C" {
#endif
typedef unsigned short digit;
typedef unsigned int wdigit;
#define BASE_TWODIGITS_TYPE long
typedef unsigned BASE_TWODIGITS_TYPE twodigits;
typedef BASE_TWODIGITS_TYPE stwodigits;
#define PyLong_SHIFT 15
#define PyLong_BASE ((digit)1 << PyLong_SHIFT)
#define PyLong_MASK ((int)(PyLong_BASE - 1))
#define SHIFT PyLong_SHIFT
#define BASE PyLong_BASE
#define MASK PyLong_MASK
#if PyLong_SHIFT % 5 != 0
#error "longobject.c requires that PyLong_SHIFT be divisible by 5"
#endif
struct _longobject {
PyObject_VAR_HEAD
digit ob_digit[1];
};
PyAPI_FUNC(PyLongObject *) _PyLong_New(Py_ssize_t);
PyAPI_FUNC(PyObject *) _PyLong_Copy(PyLongObject *src);
#if defined(__cplusplus)
}
#endif
#endif
