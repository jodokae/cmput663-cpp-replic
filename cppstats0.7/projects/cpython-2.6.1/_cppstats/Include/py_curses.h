#if !defined(Py_CURSES_H)
#define Py_CURSES_H
#if defined(__APPLE__)
#if defined(_BSD_WCHAR_T_DEFINED_)
#define _WCHAR_T
#endif
#endif
#if defined(__FreeBSD__)
#if defined(_XOPEN_SOURCE_EXTENDED)
#if !defined(__FreeBSD_version)
#include <osreldate.h>
#endif
#if __FreeBSD_version >= 500000
#if !defined(__wchar_t)
#define __wchar_t
#endif
#if !defined(__wint_t)
#define __wint_t
#endif
#else
#if !defined(_WCHAR_T)
#define _WCHAR_T
#endif
#if !defined(_WINT_T)
#define _WINT_T
#endif
#endif
#endif
#endif
#if defined(HAVE_NCURSES_H)
#include <ncurses.h>
#else
#include <curses.h>
#if defined(HAVE_TERM_H)
#include <term.h>
#endif
#endif
#if defined(HAVE_NCURSES_H)
#if !defined(WINDOW_HAS_FLAGS)
#define WINDOW_HAS_FLAGS 1
#endif
#if !defined(MVWDELCH_IS_EXPRESSION)
#define MVWDELCH_IS_EXPRESSION 1
#endif
#endif
#if defined(__cplusplus)
extern "C" {
#endif
#define PyCurses_API_pointers 4
typedef struct {
PyObject_HEAD
WINDOW *win;
} PyCursesWindowObject;
#define PyCursesWindow_Check(v) (Py_TYPE(v) == &PyCursesWindow_Type)
#if defined(CURSES_MODULE)
#else
static void **PyCurses_API;
#define PyCursesWindow_Type (*(PyTypeObject *) PyCurses_API[0])
#define PyCursesSetupTermCalled {if (! ((int (*)(void))PyCurses_API[1]) () ) return NULL;}
#define PyCursesInitialised {if (! ((int (*)(void))PyCurses_API[2]) () ) return NULL;}
#define PyCursesInitialisedColor {if (! ((int (*)(void))PyCurses_API[3]) () ) return NULL;}
#define import_curses() { PyObject *module = PyImport_ImportModuleNoBlock("_curses"); if (module != NULL) { PyObject *module_dict = PyModule_GetDict(module); PyObject *c_api_object = PyDict_GetItemString(module_dict, "_C_API"); if (PyCObject_Check(c_api_object)) { PyCurses_API = (void **)PyCObject_AsVoidPtr(c_api_object); } } }
#endif
static char *catchall_ERR = "curses function returned ERR";
static char *catchall_NULL = "curses function returned NULL";
#define NoArgNoReturnFunction(X) static PyObject *PyCurses_ ##X (PyObject *self) { PyCursesInitialised return PyCursesCheckERR(X(), #X); }
#define NoArgOrFlagNoReturnFunction(X) static PyObject *PyCurses_ ##X (PyObject *self, PyObject *args) { int flag = 0; PyCursesInitialised switch(PyTuple_Size(args)) { case 0: return PyCursesCheckERR(X(), #X); case 1: if (!PyArg_ParseTuple(args, "i;True(1) or False(0)", &flag)) return NULL; if (flag) return PyCursesCheckERR(X(), #X); else return PyCursesCheckERR(no ##X (), #X); default: PyErr_SetString(PyExc_TypeError, #X " requires 0 or 1 arguments"); return NULL; } }
#define NoArgReturnIntFunction(X) static PyObject *PyCurses_ ##X (PyObject *self) { PyCursesInitialised return PyInt_FromLong((long) X()); }
#define NoArgReturnStringFunction(X) static PyObject *PyCurses_ ##X (PyObject *self) { PyCursesInitialised return PyString_FromString(X()); }
#define NoArgTrueFalseFunction(X) static PyObject *PyCurses_ ##X (PyObject *self) { PyCursesInitialised if (X () == FALSE) { Py_INCREF(Py_False); return Py_False; } Py_INCREF(Py_True); return Py_True; }
#define NoArgNoReturnVoidFunction(X) static PyObject *PyCurses_ ##X (PyObject *self) { PyCursesInitialised X(); Py_INCREF(Py_None); return Py_None; }
#if defined(__cplusplus)
}
#endif
#endif