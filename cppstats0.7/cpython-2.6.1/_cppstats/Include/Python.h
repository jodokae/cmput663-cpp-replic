#if !defined(Py_PYTHON_H)
#define Py_PYTHON_H
#include "patchlevel.h"
#include "pyconfig.h"
#include "pymacconfig.h"
#if !defined(WITH_CYCLE_GC)
#define WITH_CYCLE_GC 1
#endif
#include <limits.h>
#if !defined(UCHAR_MAX)
#error "Something's broken. UCHAR_MAX should be defined in limits.h."
#endif
#if UCHAR_MAX != 255
#error "Python's source code assumes C's unsigned char is an 8-bit type."
#endif
#if defined(__sgi) && defined(WITH_THREAD) && !defined(_SGI_MP_SOURCE)
#define _SGI_MP_SOURCE
#endif
#include <stdio.h>
#if !defined(NULL)
#error "Python.h requires that stdio.h define NULL."
#endif
#include <string.h>
#if defined(HAVE_ERRNO_H)
#include <errno.h>
#endif
#include <stdlib.h>
#if defined(HAVE_UNISTD_H)
#include <unistd.h>
#endif
#if defined(HAVE_STDDEF_H)
#include <stddef.h>
#endif
#include <assert.h>
#include "pyport.h"
#if !defined(DL_IMPORT)
#define DL_IMPORT(RTYPE) RTYPE
#endif
#if !defined(DL_EXPORT)
#define DL_EXPORT(RTYPE) RTYPE
#endif
#if defined(Py_DEBUG) && defined(WITH_PYMALLOC) && !defined(PYMALLOC_DEBUG)
#define PYMALLOC_DEBUG
#endif
#if defined(PYMALLOC_DEBUG) && !defined(WITH_PYMALLOC)
#error "PYMALLOC_DEBUG requires WITH_PYMALLOC"
#endif
#include "pymath.h"
#include "pymem.h"
#include "object.h"
#include "objimpl.h"
#include "pydebug.h"
#include "unicodeobject.h"
#include "intobject.h"
#include "boolobject.h"
#include "longobject.h"
#include "floatobject.h"
#if !defined(WITHOUT_COMPLEX)
#include "complexobject.h"
#endif
#include "rangeobject.h"
#include "stringobject.h"
#include "bufferobject.h"
#include "bytesobject.h"
#include "bytearrayobject.h"
#include "tupleobject.h"
#include "listobject.h"
#include "dictobject.h"
#include "enumobject.h"
#include "setobject.h"
#include "methodobject.h"
#include "moduleobject.h"
#include "funcobject.h"
#include "classobject.h"
#include "fileobject.h"
#include "cobject.h"
#include "traceback.h"
#include "sliceobject.h"
#include "cellobject.h"
#include "iterobject.h"
#include "genobject.h"
#include "descrobject.h"
#include "warnings.h"
#include "weakrefobject.h"
#include "codecs.h"
#include "pyerrors.h"
#include "pystate.h"
#include "pyarena.h"
#include "modsupport.h"
#include "pythonrun.h"
#include "ceval.h"
#include "sysmodule.h"
#include "intrcheck.h"
#include "import.h"
#include "abstract.h"
#include "compile.h"
#include "eval.h"
#include "pystrtod.h"
#include "pystrcmp.h"
PyAPI_FUNC(PyObject*) _Py_Mangle(PyObject *p, PyObject *name);
#define PyArg_GetInt(v, a) PyArg_Parse((v), "i", (a))
#define PyArg_NoArgs(v) PyArg_Parse(v, "")
#if defined(__CHAR_UNSIGNED__)
#define Py_CHARMASK(c) (c)
#else
#define Py_CHARMASK(c) ((unsigned char)((c) & 0xff))
#endif
#include "pyfpe.h"
#define Py_single_input 256
#define Py_file_input 257
#define Py_eval_input 258
#if defined(HAVE_PTH)
#include <pth.h>
#endif
#define PyDoc_VAR(name) static char name[]
#define PyDoc_STRVAR(name,str) PyDoc_VAR(name) = PyDoc_STR(str)
#if defined(WITH_DOC_STRINGS)
#define PyDoc_STR(str) str
#else
#define PyDoc_STR(str) ""
#endif
#endif
