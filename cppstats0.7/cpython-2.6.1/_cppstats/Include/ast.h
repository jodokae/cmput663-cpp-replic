#if !defined(Py_AST_H)
#define Py_AST_H
#if defined(__cplusplus)
extern "C" {
#endif
PyAPI_FUNC(mod_ty) PyAST_FromNode(const node *, PyCompilerFlags *flags,
const char *, PyArena *);
#if defined(__cplusplus)
}
#endif
#endif