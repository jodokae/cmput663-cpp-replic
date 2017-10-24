#if !defined(Py_UCNHASH_H)
#define Py_UCNHASH_H
#if defined(__cplusplus)
extern "C" {
#endif
typedef struct {
int size;
int (*getname)(PyObject *self, Py_UCS4 code, char* buffer, int buflen);
int (*getcode)(PyObject *self, const char* name, int namelen, Py_UCS4* code);
} _PyUnicode_Name_CAPI;
#if defined(__cplusplus)
}
#endif
#endif
