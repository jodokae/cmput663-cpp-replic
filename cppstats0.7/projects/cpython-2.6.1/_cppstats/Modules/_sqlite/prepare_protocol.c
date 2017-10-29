#include "prepare_protocol.h"
int pysqlite_prepare_protocol_init(pysqlite_PrepareProtocol* self, PyObject* args, PyObject* kwargs) {
return 0;
}
void pysqlite_prepare_protocol_dealloc(pysqlite_PrepareProtocol* self) {
Py_TYPE(self)->tp_free((PyObject*)self);
}
PyTypeObject pysqlite_PrepareProtocolType= {
PyVarObject_HEAD_INIT(NULL, 0)
MODULE_NAME ".PrepareProtocol",
sizeof(pysqlite_PrepareProtocol),
0,
(destructor)pysqlite_prepare_protocol_dealloc,
0,
0,
0,
0,
0,
0,
0,
0,
0,
0,
0,
0,
0,
0,
Py_TPFLAGS_DEFAULT,
0,
0,
0,
0,
0,
0,
0,
0,
0,
0,
0,
0,
0,
0,
0,
(initproc)pysqlite_prepare_protocol_init,
0,
0,
0
};
extern int pysqlite_prepare_protocol_setup_types(void) {
pysqlite_PrepareProtocolType.tp_new = PyType_GenericNew;
Py_TYPE(&pysqlite_PrepareProtocolType)= &PyType_Type;
return PyType_Ready(&pysqlite_PrepareProtocolType);
}
