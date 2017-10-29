#if !defined(Py_ERRORS_H)
#define Py_ERRORS_H
#if defined(__cplusplus)
extern "C" {
#endif
typedef struct {
PyObject_HEAD
PyObject *dict;
PyObject *args;
PyObject *message;
} PyBaseExceptionObject;
typedef struct {
PyObject_HEAD
PyObject *dict;
PyObject *args;
PyObject *message;
PyObject *msg;
PyObject *filename;
PyObject *lineno;
PyObject *offset;
PyObject *text;
PyObject *print_file_and_line;
} PySyntaxErrorObject;
#if defined(Py_USING_UNICODE)
typedef struct {
PyObject_HEAD
PyObject *dict;
PyObject *args;
PyObject *message;
PyObject *encoding;
PyObject *object;
Py_ssize_t start;
Py_ssize_t end;
PyObject *reason;
} PyUnicodeErrorObject;
#endif
typedef struct {
PyObject_HEAD
PyObject *dict;
PyObject *args;
PyObject *message;
PyObject *code;
} PySystemExitObject;
typedef struct {
PyObject_HEAD
PyObject *dict;
PyObject *args;
PyObject *message;
PyObject *myerrno;
PyObject *strerror;
PyObject *filename;
} PyEnvironmentErrorObject;
#if defined(MS_WINDOWS)
typedef struct {
PyObject_HEAD
PyObject *dict;
PyObject *args;
PyObject *message;
PyObject *myerrno;
PyObject *strerror;
PyObject *filename;
PyObject *winerror;
} PyWindowsErrorObject;
#endif
PyAPI_FUNC(void) PyErr_SetNone(PyObject *);
PyAPI_FUNC(void) PyErr_SetObject(PyObject *, PyObject *);
PyAPI_FUNC(void) PyErr_SetString(PyObject *, const char *);
PyAPI_FUNC(PyObject *) PyErr_Occurred(void);
PyAPI_FUNC(void) PyErr_Clear(void);
PyAPI_FUNC(void) PyErr_Fetch(PyObject **, PyObject **, PyObject **);
PyAPI_FUNC(void) PyErr_Restore(PyObject *, PyObject *, PyObject *);
#if defined(Py_DEBUG)
#define _PyErr_OCCURRED() PyErr_Occurred()
#else
#define _PyErr_OCCURRED() (_PyThreadState_Current->curexc_type)
#endif
PyAPI_FUNC(int) PyErr_GivenExceptionMatches(PyObject *, PyObject *);
PyAPI_FUNC(int) PyErr_ExceptionMatches(PyObject *);
PyAPI_FUNC(void) PyErr_NormalizeException(PyObject**, PyObject**, PyObject**);
#define PyExceptionClass_Check(x) (PyClass_Check((x)) || (PyType_Check((x)) && PyType_FastSubclass((PyTypeObject*)(x), Py_TPFLAGS_BASE_EXC_SUBCLASS)))
#define PyExceptionInstance_Check(x) (PyInstance_Check((x)) || PyType_FastSubclass((x)->ob_type, Py_TPFLAGS_BASE_EXC_SUBCLASS))
#define PyExceptionClass_Name(x) (PyClass_Check((x)) ? PyString_AS_STRING(((PyClassObject*)(x))->cl_name) : (char *)(((PyTypeObject*)(x))->tp_name))
#define PyExceptionInstance_Class(x) ((PyInstance_Check((x)) ? (PyObject*)((PyInstanceObject*)(x))->in_class : (PyObject*)((x)->ob_type)))
PyAPI_DATA(PyObject *) PyExc_BaseException;
PyAPI_DATA(PyObject *) PyExc_Exception;
PyAPI_DATA(PyObject *) PyExc_StopIteration;
PyAPI_DATA(PyObject *) PyExc_GeneratorExit;
PyAPI_DATA(PyObject *) PyExc_StandardError;
PyAPI_DATA(PyObject *) PyExc_ArithmeticError;
PyAPI_DATA(PyObject *) PyExc_LookupError;
PyAPI_DATA(PyObject *) PyExc_AssertionError;
PyAPI_DATA(PyObject *) PyExc_AttributeError;
PyAPI_DATA(PyObject *) PyExc_EOFError;
PyAPI_DATA(PyObject *) PyExc_FloatingPointError;
PyAPI_DATA(PyObject *) PyExc_EnvironmentError;
PyAPI_DATA(PyObject *) PyExc_IOError;
PyAPI_DATA(PyObject *) PyExc_OSError;
PyAPI_DATA(PyObject *) PyExc_ImportError;
PyAPI_DATA(PyObject *) PyExc_IndexError;
PyAPI_DATA(PyObject *) PyExc_KeyError;
PyAPI_DATA(PyObject *) PyExc_KeyboardInterrupt;
PyAPI_DATA(PyObject *) PyExc_MemoryError;
PyAPI_DATA(PyObject *) PyExc_NameError;
PyAPI_DATA(PyObject *) PyExc_OverflowError;
PyAPI_DATA(PyObject *) PyExc_RuntimeError;
PyAPI_DATA(PyObject *) PyExc_NotImplementedError;
PyAPI_DATA(PyObject *) PyExc_SyntaxError;
PyAPI_DATA(PyObject *) PyExc_IndentationError;
PyAPI_DATA(PyObject *) PyExc_TabError;
PyAPI_DATA(PyObject *) PyExc_ReferenceError;
PyAPI_DATA(PyObject *) PyExc_SystemError;
PyAPI_DATA(PyObject *) PyExc_SystemExit;
PyAPI_DATA(PyObject *) PyExc_TypeError;
PyAPI_DATA(PyObject *) PyExc_UnboundLocalError;
PyAPI_DATA(PyObject *) PyExc_UnicodeError;
PyAPI_DATA(PyObject *) PyExc_UnicodeEncodeError;
PyAPI_DATA(PyObject *) PyExc_UnicodeDecodeError;
PyAPI_DATA(PyObject *) PyExc_UnicodeTranslateError;
PyAPI_DATA(PyObject *) PyExc_ValueError;
PyAPI_DATA(PyObject *) PyExc_ZeroDivisionError;
#if defined(MS_WINDOWS)
PyAPI_DATA(PyObject *) PyExc_WindowsError;
#endif
#if defined(__VMS)
PyAPI_DATA(PyObject *) PyExc_VMSError;
#endif
PyAPI_DATA(PyObject *) PyExc_BufferError;
PyAPI_DATA(PyObject *) PyExc_MemoryErrorInst;
PyAPI_DATA(PyObject *) PyExc_RecursionErrorInst;
PyAPI_DATA(PyObject *) PyExc_Warning;
PyAPI_DATA(PyObject *) PyExc_UserWarning;
PyAPI_DATA(PyObject *) PyExc_DeprecationWarning;
PyAPI_DATA(PyObject *) PyExc_PendingDeprecationWarning;
PyAPI_DATA(PyObject *) PyExc_SyntaxWarning;
PyAPI_DATA(PyObject *) PyExc_RuntimeWarning;
PyAPI_DATA(PyObject *) PyExc_FutureWarning;
PyAPI_DATA(PyObject *) PyExc_ImportWarning;
PyAPI_DATA(PyObject *) PyExc_UnicodeWarning;
PyAPI_DATA(PyObject *) PyExc_BytesWarning;
PyAPI_FUNC(int) PyErr_BadArgument(void);
PyAPI_FUNC(PyObject *) PyErr_NoMemory(void);
PyAPI_FUNC(PyObject *) PyErr_SetFromErrno(PyObject *);
PyAPI_FUNC(PyObject *) PyErr_SetFromErrnoWithFilenameObject(
PyObject *, PyObject *);
PyAPI_FUNC(PyObject *) PyErr_SetFromErrnoWithFilename(PyObject *, char *);
#if defined(Py_WIN_WIDE_FILENAMES)
PyAPI_FUNC(PyObject *) PyErr_SetFromErrnoWithUnicodeFilename(
PyObject *, Py_UNICODE *);
#endif
PyAPI_FUNC(PyObject *) PyErr_Format(PyObject *, const char *, ...)
Py_GCC_ATTRIBUTE((format(printf, 2, 3)));
#if defined(MS_WINDOWS)
PyAPI_FUNC(PyObject *) PyErr_SetFromWindowsErrWithFilenameObject(
int, const char *);
PyAPI_FUNC(PyObject *) PyErr_SetFromWindowsErrWithFilename(
int, const char *);
#if defined(Py_WIN_WIDE_FILENAMES)
PyAPI_FUNC(PyObject *) PyErr_SetFromWindowsErrWithUnicodeFilename(
int, const Py_UNICODE *);
#endif
PyAPI_FUNC(PyObject *) PyErr_SetFromWindowsErr(int);
PyAPI_FUNC(PyObject *) PyErr_SetExcFromWindowsErrWithFilenameObject(
PyObject *,int, PyObject *);
PyAPI_FUNC(PyObject *) PyErr_SetExcFromWindowsErrWithFilename(
PyObject *,int, const char *);
#if defined(Py_WIN_WIDE_FILENAMES)
PyAPI_FUNC(PyObject *) PyErr_SetExcFromWindowsErrWithUnicodeFilename(
PyObject *,int, const Py_UNICODE *);
#endif
PyAPI_FUNC(PyObject *) PyErr_SetExcFromWindowsErr(PyObject *, int);
#endif
PyAPI_FUNC(void) PyErr_BadInternalCall(void);
PyAPI_FUNC(void) _PyErr_BadInternalCall(char *filename, int lineno);
#define PyErr_BadInternalCall() _PyErr_BadInternalCall(__FILE__, __LINE__)
PyAPI_FUNC(PyObject *) PyErr_NewException(char *name, PyObject *base,
PyObject *dict);
PyAPI_FUNC(void) PyErr_WriteUnraisable(PyObject *);
PyAPI_FUNC(int) PyErr_CheckSignals(void);
PyAPI_FUNC(void) PyErr_SetInterrupt(void);
int PySignal_SetWakeupFd(int fd);
PyAPI_FUNC(void) PyErr_SyntaxLocation(const char *, int);
PyAPI_FUNC(PyObject *) PyErr_ProgramText(const char *, int);
#if defined(Py_USING_UNICODE)
PyAPI_FUNC(PyObject *) PyUnicodeDecodeError_Create(
const char *, const char *, Py_ssize_t, Py_ssize_t, Py_ssize_t, const char *);
PyAPI_FUNC(PyObject *) PyUnicodeEncodeError_Create(
const char *, const Py_UNICODE *, Py_ssize_t, Py_ssize_t, Py_ssize_t, const char *);
PyAPI_FUNC(PyObject *) PyUnicodeTranslateError_Create(
const Py_UNICODE *, Py_ssize_t, Py_ssize_t, Py_ssize_t, const char *);
PyAPI_FUNC(PyObject *) PyUnicodeEncodeError_GetEncoding(PyObject *);
PyAPI_FUNC(PyObject *) PyUnicodeDecodeError_GetEncoding(PyObject *);
PyAPI_FUNC(PyObject *) PyUnicodeEncodeError_GetObject(PyObject *);
PyAPI_FUNC(PyObject *) PyUnicodeDecodeError_GetObject(PyObject *);
PyAPI_FUNC(PyObject *) PyUnicodeTranslateError_GetObject(PyObject *);
PyAPI_FUNC(int) PyUnicodeEncodeError_GetStart(PyObject *, Py_ssize_t *);
PyAPI_FUNC(int) PyUnicodeDecodeError_GetStart(PyObject *, Py_ssize_t *);
PyAPI_FUNC(int) PyUnicodeTranslateError_GetStart(PyObject *, Py_ssize_t *);
PyAPI_FUNC(int) PyUnicodeEncodeError_SetStart(PyObject *, Py_ssize_t);
PyAPI_FUNC(int) PyUnicodeDecodeError_SetStart(PyObject *, Py_ssize_t);
PyAPI_FUNC(int) PyUnicodeTranslateError_SetStart(PyObject *, Py_ssize_t);
PyAPI_FUNC(int) PyUnicodeEncodeError_GetEnd(PyObject *, Py_ssize_t *);
PyAPI_FUNC(int) PyUnicodeDecodeError_GetEnd(PyObject *, Py_ssize_t *);
PyAPI_FUNC(int) PyUnicodeTranslateError_GetEnd(PyObject *, Py_ssize_t *);
PyAPI_FUNC(int) PyUnicodeEncodeError_SetEnd(PyObject *, Py_ssize_t);
PyAPI_FUNC(int) PyUnicodeDecodeError_SetEnd(PyObject *, Py_ssize_t);
PyAPI_FUNC(int) PyUnicodeTranslateError_SetEnd(PyObject *, Py_ssize_t);
PyAPI_FUNC(PyObject *) PyUnicodeEncodeError_GetReason(PyObject *);
PyAPI_FUNC(PyObject *) PyUnicodeDecodeError_GetReason(PyObject *);
PyAPI_FUNC(PyObject *) PyUnicodeTranslateError_GetReason(PyObject *);
PyAPI_FUNC(int) PyUnicodeEncodeError_SetReason(
PyObject *, const char *);
PyAPI_FUNC(int) PyUnicodeDecodeError_SetReason(
PyObject *, const char *);
PyAPI_FUNC(int) PyUnicodeTranslateError_SetReason(
PyObject *, const char *);
#endif
#if defined(MS_WIN32) && !defined(HAVE_SNPRINTF)
#define HAVE_SNPRINTF
#define snprintf _snprintf
#define vsnprintf _vsnprintf
#endif
#include <stdarg.h>
PyAPI_FUNC(int) PyOS_snprintf(char *str, size_t size, const char *format, ...)
Py_GCC_ATTRIBUTE((format(printf, 3, 4)));
PyAPI_FUNC(int) PyOS_vsnprintf(char *str, size_t size, const char *format, va_list va)
Py_GCC_ATTRIBUTE((format(printf, 3, 0)));
#if defined(__cplusplus)
}
#endif
#endif