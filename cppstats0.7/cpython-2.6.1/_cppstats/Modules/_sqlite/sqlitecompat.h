#if !defined(PYSQLITE_COMPAT_H)
#define PYSQLITE_COMPAT_H
#if PY_VERSION_HEX < 0x02050000
typedef int Py_ssize_t;
typedef int (*lenfunc)(PyObject*);
#endif
#endif
