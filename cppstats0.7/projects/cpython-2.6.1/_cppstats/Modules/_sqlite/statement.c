#include "statement.h"
#include "cursor.h"
#include "connection.h"
#include "microprotocols.h"
#include "prepare_protocol.h"
#include "sqlitecompat.h"
static int pysqlite_check_remaining_sql(const char* tail);
typedef enum {
LINECOMMENT_1,
IN_LINECOMMENT,
COMMENTSTART_1,
IN_COMMENT,
COMMENTEND_1,
NORMAL
} parse_remaining_sql_state;
typedef enum {
TYPE_INT,
TYPE_LONG,
TYPE_FLOAT,
TYPE_STRING,
TYPE_UNICODE,
TYPE_BUFFER,
TYPE_UNKNOWN
} parameter_type;
int pysqlite_statement_create(pysqlite_Statement* self, pysqlite_Connection* connection, PyObject* sql) {
const char* tail;
int rc;
PyObject* sql_str;
char* sql_cstr;
self->st = NULL;
self->in_use = 0;
if (PyString_Check(sql)) {
sql_str = sql;
Py_INCREF(sql_str);
} else if (PyUnicode_Check(sql)) {
sql_str = PyUnicode_AsUTF8String(sql);
if (!sql_str) {
rc = PYSQLITE_SQL_WRONG_TYPE;
return rc;
}
} else {
rc = PYSQLITE_SQL_WRONG_TYPE;
return rc;
}
self->in_weakreflist = NULL;
self->sql = sql_str;
sql_cstr = PyString_AsString(sql_str);
Py_BEGIN_ALLOW_THREADS
rc = sqlite3_prepare(connection->db,
sql_cstr,
-1,
&self->st,
&tail);
Py_END_ALLOW_THREADS
self->db = connection->db;
if (rc == SQLITE_OK && pysqlite_check_remaining_sql(tail)) {
(void)sqlite3_finalize(self->st);
self->st = NULL;
rc = PYSQLITE_TOO_MUCH_SQL;
}
return rc;
}
int pysqlite_statement_bind_parameter(pysqlite_Statement* self, int pos, PyObject* parameter, int allow_8bit_chars) {
int rc = SQLITE_OK;
long longval;
PY_LONG_LONG longlongval;
const char* buffer;
char* string;
Py_ssize_t buflen;
PyObject* stringval;
parameter_type paramtype;
char* c;
if (parameter == Py_None) {
rc = sqlite3_bind_null(self->st, pos);
goto final;
}
if (PyInt_CheckExact(parameter)) {
paramtype = TYPE_INT;
} else if (PyLong_CheckExact(parameter)) {
paramtype = TYPE_LONG;
} else if (PyFloat_CheckExact(parameter)) {
paramtype = TYPE_FLOAT;
} else if (PyString_CheckExact(parameter)) {
paramtype = TYPE_STRING;
} else if (PyUnicode_CheckExact(parameter)) {
paramtype = TYPE_UNICODE;
} else if (PyBuffer_Check(parameter)) {
paramtype = TYPE_BUFFER;
} else if (PyInt_Check(parameter)) {
paramtype = TYPE_INT;
} else if (PyLong_Check(parameter)) {
paramtype = TYPE_LONG;
} else if (PyFloat_Check(parameter)) {
paramtype = TYPE_FLOAT;
} else if (PyString_Check(parameter)) {
paramtype = TYPE_STRING;
} else if (PyUnicode_Check(parameter)) {
paramtype = TYPE_UNICODE;
} else {
paramtype = TYPE_UNKNOWN;
}
if (paramtype == TYPE_STRING && !allow_8bit_chars) {
string = PyString_AS_STRING(parameter);
for (c = string; *c != 0; c++) {
if (*c & 0x80) {
PyErr_SetString(pysqlite_ProgrammingError, "You must not use 8-bit bytestrings unless you use a text_factory that can interpret 8-bit bytestrings (like text_factory = str). It is highly recommended that you instead just switch your application to Unicode strings.");
rc = -1;
goto final;
}
}
}
switch (paramtype) {
case TYPE_INT:
longval = PyInt_AsLong(parameter);
rc = sqlite3_bind_int64(self->st, pos, (sqlite_int64)longval);
break;
case TYPE_LONG:
longlongval = PyLong_AsLongLong(parameter);
rc = sqlite3_bind_int64(self->st, pos, (sqlite_int64)longlongval);
break;
case TYPE_FLOAT:
rc = sqlite3_bind_double(self->st, pos, PyFloat_AsDouble(parameter));
break;
case TYPE_STRING:
string = PyString_AS_STRING(parameter);
rc = sqlite3_bind_text(self->st, pos, string, -1, SQLITE_TRANSIENT);
break;
case TYPE_UNICODE:
stringval = PyUnicode_AsUTF8String(parameter);
string = PyString_AsString(stringval);
rc = sqlite3_bind_text(self->st, pos, string, -1, SQLITE_TRANSIENT);
Py_DECREF(stringval);
break;
case TYPE_BUFFER:
if (PyObject_AsCharBuffer(parameter, &buffer, &buflen) == 0) {
rc = sqlite3_bind_blob(self->st, pos, buffer, buflen, SQLITE_TRANSIENT);
} else {
PyErr_SetString(PyExc_ValueError, "could not convert BLOB to buffer");
rc = -1;
}
break;
case TYPE_UNKNOWN:
rc = -1;
}
final:
return rc;
}
static int _need_adapt(PyObject* obj) {
if (pysqlite_BaseTypeAdapted) {
return 1;
}
if (PyInt_CheckExact(obj) || PyLong_CheckExact(obj)
|| PyFloat_CheckExact(obj) || PyString_CheckExact(obj)
|| PyUnicode_CheckExact(obj) || PyBuffer_Check(obj)) {
return 0;
} else {
return 1;
}
}
void pysqlite_statement_bind_parameters(pysqlite_Statement* self, PyObject* parameters, int allow_8bit_chars) {
PyObject* current_param;
PyObject* adapted;
const char* binding_name;
int i;
int rc;
int num_params_needed;
int num_params;
Py_BEGIN_ALLOW_THREADS
num_params_needed = sqlite3_bind_parameter_count(self->st);
Py_END_ALLOW_THREADS
if (PyTuple_CheckExact(parameters) || PyList_CheckExact(parameters) || (!PyDict_Check(parameters) && PySequence_Check(parameters))) {
if (PyTuple_CheckExact(parameters)) {
num_params = PyTuple_GET_SIZE(parameters);
} else if (PyList_CheckExact(parameters)) {
num_params = PyList_GET_SIZE(parameters);
} else {
num_params = PySequence_Size(parameters);
}
if (num_params != num_params_needed) {
PyErr_Format(pysqlite_ProgrammingError, "Incorrect number of bindings supplied. The current statement uses %d, and there are %d supplied.",
num_params_needed, num_params);
return;
}
for (i = 0; i < num_params; i++) {
if (PyTuple_CheckExact(parameters)) {
current_param = PyTuple_GET_ITEM(parameters, i);
Py_XINCREF(current_param);
} else if (PyList_CheckExact(parameters)) {
current_param = PyList_GET_ITEM(parameters, i);
Py_XINCREF(current_param);
} else {
current_param = PySequence_GetItem(parameters, i);
}
if (!current_param) {
return;
}
if (!_need_adapt(current_param)) {
adapted = current_param;
} else {
adapted = pysqlite_microprotocols_adapt(current_param, (PyObject*)&pysqlite_PrepareProtocolType, NULL);
if (adapted) {
Py_DECREF(current_param);
} else {
PyErr_Clear();
adapted = current_param;
}
}
rc = pysqlite_statement_bind_parameter(self, i + 1, adapted, allow_8bit_chars);
Py_DECREF(adapted);
if (rc != SQLITE_OK) {
if (!PyErr_Occurred()) {
PyErr_Format(pysqlite_InterfaceError, "Error binding parameter %d - probably unsupported type.", i);
}
return;
}
}
} else if (PyDict_Check(parameters)) {
for (i = 1; i <= num_params_needed; i++) {
Py_BEGIN_ALLOW_THREADS
binding_name = sqlite3_bind_parameter_name(self->st, i);
Py_END_ALLOW_THREADS
if (!binding_name) {
PyErr_Format(pysqlite_ProgrammingError, "Binding %d has no name, but you supplied a dictionary (which has only names).", i);
return;
}
binding_name++;
if (PyDict_CheckExact(parameters)) {
current_param = PyDict_GetItemString(parameters, binding_name);
Py_XINCREF(current_param);
} else {
current_param = PyMapping_GetItemString(parameters, (char*)binding_name);
}
if (!current_param) {
PyErr_Format(pysqlite_ProgrammingError, "You did not supply a value for binding %d.", i);
return;
}
if (!_need_adapt(current_param)) {
adapted = current_param;
} else {
adapted = pysqlite_microprotocols_adapt(current_param, (PyObject*)&pysqlite_PrepareProtocolType, NULL);
if (adapted) {
Py_DECREF(current_param);
} else {
PyErr_Clear();
adapted = current_param;
}
}
rc = pysqlite_statement_bind_parameter(self, i, adapted, allow_8bit_chars);
Py_DECREF(adapted);
if (rc != SQLITE_OK) {
if (!PyErr_Occurred()) {
PyErr_Format(pysqlite_InterfaceError, "Error binding parameter :%s - probably unsupported type.", binding_name);
}
return;
}
}
} else {
PyErr_SetString(PyExc_ValueError, "parameters are of unsupported type");
}
}
int pysqlite_statement_recompile(pysqlite_Statement* self, PyObject* params) {
const char* tail;
int rc;
char* sql_cstr;
sqlite3_stmt* new_st;
sql_cstr = PyString_AsString(self->sql);
Py_BEGIN_ALLOW_THREADS
rc = sqlite3_prepare(self->db,
sql_cstr,
-1,
&new_st,
&tail);
Py_END_ALLOW_THREADS
if (rc == SQLITE_OK) {
#if defined(SQLITE_VERSION_NUMBER)
#if SQLITE_VERSION_NUMBER >= 3002002
if (sqlite3_bind_parameter_count(self->st) > 0) {
(void)sqlite3_transfer_bindings(self->st, new_st);
}
#endif
#else
statement_bind_parameters(self, params);
#endif
(void)sqlite3_finalize(self->st);
self->st = new_st;
}
return rc;
}
int pysqlite_statement_finalize(pysqlite_Statement* self) {
int rc;
rc = SQLITE_OK;
if (self->st) {
Py_BEGIN_ALLOW_THREADS
rc = sqlite3_finalize(self->st);
Py_END_ALLOW_THREADS
self->st = NULL;
}
self->in_use = 0;
return rc;
}
int pysqlite_statement_reset(pysqlite_Statement* self) {
int rc;
rc = SQLITE_OK;
if (self->in_use && self->st) {
Py_BEGIN_ALLOW_THREADS
rc = sqlite3_reset(self->st);
Py_END_ALLOW_THREADS
if (rc == SQLITE_OK) {
self->in_use = 0;
}
}
return rc;
}
void pysqlite_statement_mark_dirty(pysqlite_Statement* self) {
self->in_use = 1;
}
void pysqlite_statement_dealloc(pysqlite_Statement* self) {
int rc;
if (self->st) {
Py_BEGIN_ALLOW_THREADS
rc = sqlite3_finalize(self->st);
Py_END_ALLOW_THREADS
}
self->st = NULL;
Py_XDECREF(self->sql);
if (self->in_weakreflist != NULL) {
PyObject_ClearWeakRefs((PyObject*)self);
}
Py_TYPE(self)->tp_free((PyObject*)self);
}
static int pysqlite_check_remaining_sql(const char* tail) {
const char* pos = tail;
parse_remaining_sql_state state = NORMAL;
for (;;) {
switch (*pos) {
case 0:
return 0;
case '-':
if (state == NORMAL) {
state = LINECOMMENT_1;
} else if (state == LINECOMMENT_1) {
state = IN_LINECOMMENT;
}
break;
case ' ':
case '\t':
break;
case '\n':
case 13:
if (state == IN_LINECOMMENT) {
state = NORMAL;
}
break;
case '/':
if (state == NORMAL) {
state = COMMENTSTART_1;
} else if (state == COMMENTEND_1) {
state = NORMAL;
} else if (state == COMMENTSTART_1) {
return 1;
}
break;
case '*':
if (state == NORMAL) {
return 1;
} else if (state == LINECOMMENT_1) {
return 1;
} else if (state == COMMENTSTART_1) {
state = IN_COMMENT;
} else if (state == IN_COMMENT) {
state = COMMENTEND_1;
}
break;
default:
if (state == COMMENTEND_1) {
state = IN_COMMENT;
} else if (state == IN_LINECOMMENT) {
} else if (state == IN_COMMENT) {
} else {
return 1;
}
}
pos++;
}
return 0;
}
PyTypeObject pysqlite_StatementType = {
PyVarObject_HEAD_INIT(NULL, 0)
MODULE_NAME ".Statement",
sizeof(pysqlite_Statement),
0,
(destructor)pysqlite_statement_dealloc,
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
Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_WEAKREFS,
0,
0,
0,
0,
offsetof(pysqlite_Statement, in_weakreflist),
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
(initproc)0,
0,
0,
0
};
extern int pysqlite_statement_setup_types(void) {
pysqlite_StatementType.tp_new = PyType_GenericNew;
return PyType_Ready(&pysqlite_StatementType);
}