static char *PyCursesVersion = "2.1";
#include "Python.h"
#include "py_curses.h"
#include <panel.h>
static PyObject *PyCursesError;
static PyObject *
PyCursesCheckERR(int code, char *fname) {
if (code != ERR) {
Py_INCREF(Py_None);
return Py_None;
} else {
if (fname == NULL) {
PyErr_SetString(PyCursesError, catchall_ERR);
} else {
PyErr_Format(PyCursesError, "%s() returned ERR", fname);
}
return NULL;
}
}
typedef struct {
PyObject_HEAD
PANEL *pan;
PyCursesWindowObject *wo;
} PyCursesPanelObject;
PyTypeObject PyCursesPanel_Type;
#define PyCursesPanel_Check(v) (Py_TYPE(v) == &PyCursesPanel_Type)
typedef struct _list_of_panels {
PyCursesPanelObject *po;
struct _list_of_panels *next;
} list_of_panels;
static list_of_panels *lop;
static int
insert_lop(PyCursesPanelObject *po) {
list_of_panels *new;
if ((new = (list_of_panels *)malloc(sizeof(list_of_panels))) == NULL) {
PyErr_NoMemory();
return -1;
}
new->po = po;
new->next = lop;
lop = new;
return 0;
}
static void
remove_lop(PyCursesPanelObject *po) {
list_of_panels *temp, *n;
temp = lop;
if (temp->po == po) {
lop = temp->next;
free(temp);
return;
}
while (temp->next == NULL || temp->next->po != po) {
if (temp->next == NULL) {
PyErr_SetString(PyExc_RuntimeError,
"remove_lop: can't find Panel Object");
return;
}
temp = temp->next;
}
n = temp->next->next;
free(temp->next);
temp->next = n;
return;
}
static PyCursesPanelObject *
find_po(PANEL *pan) {
list_of_panels *temp;
for (temp = lop; temp->po->pan != pan; temp = temp->next)
if (temp->next == NULL) return NULL;
return temp->po;
}
#define Panel_NoArgNoReturnFunction(X) static PyObject *PyCursesPanel_##X(PyCursesPanelObject *self) { return PyCursesCheckERR(X(self->pan), #X); }
#define Panel_NoArgTrueFalseFunction(X) static PyObject *PyCursesPanel_##X(PyCursesPanelObject *self) { if (X (self->pan) == FALSE) { Py_INCREF(Py_False); return Py_False; } else { Py_INCREF(Py_True); return Py_True; } }
#define Panel_TwoArgNoReturnFunction(X, TYPE, PARSESTR) static PyObject *PyCursesPanel_##X(PyCursesPanelObject *self, PyObject *args) { TYPE arg1, arg2; if (!PyArg_ParseTuple(args, PARSESTR, &arg1, &arg2)) return NULL; return PyCursesCheckERR(X(self->pan, arg1, arg2), #X); }
Panel_NoArgNoReturnFunction(bottom_panel)
Panel_NoArgNoReturnFunction(hide_panel)
Panel_NoArgNoReturnFunction(show_panel)
Panel_NoArgNoReturnFunction(top_panel)
Panel_NoArgTrueFalseFunction(panel_hidden)
Panel_TwoArgNoReturnFunction(move_panel, int, "ii;y,x")
static PyObject *
PyCursesPanel_New(PANEL *pan, PyCursesWindowObject *wo) {
PyCursesPanelObject *po;
po = PyObject_NEW(PyCursesPanelObject, &PyCursesPanel_Type);
if (po == NULL) return NULL;
po->pan = pan;
po->wo = wo;
Py_INCREF(wo);
if (insert_lop(po) < 0) {
PyObject_DEL(po);
return NULL;
}
return (PyObject *)po;
}
static void
PyCursesPanel_Dealloc(PyCursesPanelObject *po) {
(void)del_panel(po->pan);
Py_DECREF(po->wo);
remove_lop(po);
PyObject_DEL(po);
}
static PyObject *
PyCursesPanel_above(PyCursesPanelObject *self) {
PANEL *pan;
PyCursesPanelObject *po;
pan = panel_above(self->pan);
if (pan == NULL) {
Py_INCREF(Py_None);
return Py_None;
}
po = find_po(pan);
if (po == NULL) {
PyErr_SetString(PyExc_RuntimeError,
"panel_above: can't find Panel Object");
return NULL;
}
Py_INCREF(po);
return (PyObject *)po;
}
static PyObject *
PyCursesPanel_below(PyCursesPanelObject *self) {
PANEL *pan;
PyCursesPanelObject *po;
pan = panel_below(self->pan);
if (pan == NULL) {
Py_INCREF(Py_None);
return Py_None;
}
po = find_po(pan);
if (po == NULL) {
PyErr_SetString(PyExc_RuntimeError,
"panel_below: can't find Panel Object");
return NULL;
}
Py_INCREF(po);
return (PyObject *)po;
}
static PyObject *
PyCursesPanel_window(PyCursesPanelObject *self) {
Py_INCREF(self->wo);
return (PyObject *)self->wo;
}
static PyObject *
PyCursesPanel_replace_panel(PyCursesPanelObject *self, PyObject *args) {
PyCursesPanelObject *po;
PyCursesWindowObject *temp;
int rtn;
if (PyTuple_Size(args) != 1) {
PyErr_SetString(PyExc_TypeError, "replace requires one argument");
return NULL;
}
if (!PyArg_ParseTuple(args, "O!;window object",
&PyCursesWindow_Type, &temp))
return NULL;
po = find_po(self->pan);
if (po == NULL) {
PyErr_SetString(PyExc_RuntimeError,
"replace_panel: can't find Panel Object");
return NULL;
}
rtn = replace_panel(self->pan, temp->win);
if (rtn == ERR) {
PyErr_SetString(PyCursesError, "replace_panel() returned ERR");
return NULL;
}
Py_DECREF(po->wo);
po->wo = temp;
Py_INCREF(po->wo);
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
PyCursesPanel_set_panel_userptr(PyCursesPanelObject *self, PyObject *obj) {
Py_INCREF(obj);
return PyCursesCheckERR(set_panel_userptr(self->pan, (void*)obj),
"set_panel_userptr");
}
static PyObject *
PyCursesPanel_userptr(PyCursesPanelObject *self) {
PyObject *obj;
PyCursesInitialised;
obj = (PyObject *) panel_userptr(self->pan);
if (obj == NULL) {
PyErr_SetString(PyCursesError, "no userptr set");
return NULL;
}
Py_INCREF(obj);
return obj;
}
static PyMethodDef PyCursesPanel_Methods[] = {
{"above", (PyCFunction)PyCursesPanel_above, METH_NOARGS},
{"below", (PyCFunction)PyCursesPanel_below, METH_NOARGS},
{"bottom", (PyCFunction)PyCursesPanel_bottom_panel, METH_NOARGS},
{"hidden", (PyCFunction)PyCursesPanel_panel_hidden, METH_NOARGS},
{"hide", (PyCFunction)PyCursesPanel_hide_panel, METH_NOARGS},
{"move", (PyCFunction)PyCursesPanel_move_panel, METH_VARARGS},
{"replace", (PyCFunction)PyCursesPanel_replace_panel, METH_VARARGS},
{"set_userptr", (PyCFunction)PyCursesPanel_set_panel_userptr, METH_O},
{"show", (PyCFunction)PyCursesPanel_show_panel, METH_NOARGS},
{"top", (PyCFunction)PyCursesPanel_top_panel, METH_NOARGS},
{"userptr", (PyCFunction)PyCursesPanel_userptr, METH_NOARGS},
{"window", (PyCFunction)PyCursesPanel_window, METH_NOARGS},
{NULL, NULL}
};
static PyObject *
PyCursesPanel_GetAttr(PyCursesPanelObject *self, char *name) {
return Py_FindMethod(PyCursesPanel_Methods, (PyObject *)self, name);
}
PyTypeObject PyCursesPanel_Type = {
PyVarObject_HEAD_INIT(NULL, 0)
"_curses_panel.curses panel",
sizeof(PyCursesPanelObject),
0,
(destructor)PyCursesPanel_Dealloc,
0,
(getattrfunc)PyCursesPanel_GetAttr,
(setattrfunc)0,
0,
0,
0,
0,
0,
0,
};
static PyObject *
PyCurses_bottom_panel(PyObject *self) {
PANEL *pan;
PyCursesPanelObject *po;
PyCursesInitialised;
pan = panel_above(NULL);
if (pan == NULL) {
Py_INCREF(Py_None);
return Py_None;
}
po = find_po(pan);
if (po == NULL) {
PyErr_SetString(PyExc_RuntimeError,
"panel_above: can't find Panel Object");
return NULL;
}
Py_INCREF(po);
return (PyObject *)po;
}
static PyObject *
PyCurses_new_panel(PyObject *self, PyObject *args) {
PyCursesWindowObject *win;
PANEL *pan;
if (!PyArg_ParseTuple(args, "O!", &PyCursesWindow_Type, &win))
return NULL;
pan = new_panel(win->win);
if (pan == NULL) {
PyErr_SetString(PyCursesError, catchall_NULL);
return NULL;
}
return (PyObject *)PyCursesPanel_New(pan, win);
}
static PyObject *
PyCurses_top_panel(PyObject *self) {
PANEL *pan;
PyCursesPanelObject *po;
PyCursesInitialised;
pan = panel_below(NULL);
if (pan == NULL) {
Py_INCREF(Py_None);
return Py_None;
}
po = find_po(pan);
if (po == NULL) {
PyErr_SetString(PyExc_RuntimeError,
"panel_below: can't find Panel Object");
return NULL;
}
Py_INCREF(po);
return (PyObject *)po;
}
static PyObject *PyCurses_update_panels(PyObject *self) {
PyCursesInitialised;
update_panels();
Py_INCREF(Py_None);
return Py_None;
}
static PyMethodDef PyCurses_methods[] = {
{"bottom_panel", (PyCFunction)PyCurses_bottom_panel, METH_NOARGS},
{"new_panel", (PyCFunction)PyCurses_new_panel, METH_VARARGS},
{"top_panel", (PyCFunction)PyCurses_top_panel, METH_NOARGS},
{"update_panels", (PyCFunction)PyCurses_update_panels, METH_NOARGS},
{NULL, NULL}
};
PyMODINIT_FUNC
init_curses_panel(void) {
PyObject *m, *d, *v;
Py_TYPE(&PyCursesPanel_Type) = &PyType_Type;
import_curses();
m = Py_InitModule("_curses_panel", PyCurses_methods);
if (m == NULL)
return;
d = PyModule_GetDict(m);
PyCursesError = PyErr_NewException("_curses_panel.error", NULL, NULL);
PyDict_SetItemString(d, "error", PyCursesError);
v = PyString_FromString(PyCursesVersion);
PyDict_SetItemString(d, "version", v);
PyDict_SetItemString(d, "__version__", v);
Py_DECREF(v);
}
