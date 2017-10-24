#include <Carbon/Carbon.h>
#include "Python.h"
#include "pymactoolbox.h"
#if !defined(__LP64__)
static char cp_GetColor__doc__[] =
"GetColor(prompt, (r, g, b)) -> (r, g, b), ok"
;
static PyObject *
cp_GetColor(PyObject *self, PyObject *args) {
RGBColor inColor, outColor;
Boolean ok;
Point where = {0, 0};
Str255 prompt;
if (!PyArg_ParseTuple(args, "O&O&", PyMac_GetStr255, prompt, QdRGB_Convert, &inColor))
return NULL;
ok = GetColor(where, prompt, &inColor, &outColor);
return Py_BuildValue("O&h", QdRGB_New, &outColor, ok);
}
#endif
static struct PyMethodDef cp_methods[] = {
#if !defined(__LP64__)
{"GetColor", (PyCFunction)cp_GetColor, METH_VARARGS, cp_GetColor__doc__},
#endif
{NULL, (PyCFunction)NULL, 0, NULL}
};
static char cp_module_documentation[] =
""
;
void initColorPicker(void) {
PyObject *m;
if (PyErr_WarnPy3k("In 3.x, ColorPicker is removed.", 1) < 0)
return;
m = Py_InitModule4("ColorPicker", cp_methods,
cp_module_documentation,
(PyObject*)NULL,PYTHON_API_VERSION);
if (PyErr_Occurred())
Py_FatalError("can't initialize module ColorPicker");
}
