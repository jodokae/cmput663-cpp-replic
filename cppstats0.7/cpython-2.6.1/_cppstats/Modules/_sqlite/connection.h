#if !defined(PYSQLITE_CONNECTION_H)
#define PYSQLITE_CONNECTION_H
#include "Python.h"
#include "pythread.h"
#include "structmember.h"
#include "cache.h"
#include "module.h"
#include "sqlite3.h"
typedef struct {
PyObject_HEAD
sqlite3* db;
int inTransaction;
int detect_types;
double timeout;
double timeout_started;
PyObject* isolation_level;
char* begin_statement;
int check_same_thread;
long thread_ident;
pysqlite_Cache* statement_cache;
PyObject* statements;
int created_statements;
PyObject* row_factory;
PyObject* text_factory;
PyObject* function_pinboard;
PyObject* collations;
PyObject* apsw_connection;
PyObject* Warning;
PyObject* Error;
PyObject* InterfaceError;
PyObject* DatabaseError;
PyObject* DataError;
PyObject* OperationalError;
PyObject* IntegrityError;
PyObject* InternalError;
PyObject* ProgrammingError;
PyObject* NotSupportedError;
} pysqlite_Connection;
extern PyTypeObject pysqlite_ConnectionType;
PyObject* pysqlite_connection_alloc(PyTypeObject* type, int aware);
void pysqlite_connection_dealloc(pysqlite_Connection* self);
PyObject* pysqlite_connection_cursor(pysqlite_Connection* self, PyObject* args, PyObject* kwargs);
PyObject* pysqlite_connection_close(pysqlite_Connection* self, PyObject* args);
PyObject* _pysqlite_connection_begin(pysqlite_Connection* self);
PyObject* pysqlite_connection_commit(pysqlite_Connection* self, PyObject* args);
PyObject* pysqlite_connection_rollback(pysqlite_Connection* self, PyObject* args);
PyObject* pysqlite_connection_new(PyTypeObject* type, PyObject* args, PyObject* kw);
int pysqlite_connection_init(pysqlite_Connection* self, PyObject* args, PyObject* kwargs);
int pysqlite_check_thread(pysqlite_Connection* self);
int pysqlite_check_connection(pysqlite_Connection* con);
int pysqlite_connection_setup_types(void);
#endif
