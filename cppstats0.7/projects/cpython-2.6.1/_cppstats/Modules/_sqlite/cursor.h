#if !defined(PYSQLITE_CURSOR_H)
#define PYSQLITE_CURSOR_H
#include "Python.h"
#include "statement.h"
#include "connection.h"
#include "module.h"
typedef struct {
PyObject_HEAD
pysqlite_Connection* connection;
PyObject* description;
PyObject* row_cast_map;
int arraysize;
PyObject* lastrowid;
long rowcount;
PyObject* row_factory;
pysqlite_Statement* statement;
PyObject* next_row;
} pysqlite_Cursor;
typedef enum {
STATEMENT_INVALID, STATEMENT_INSERT, STATEMENT_DELETE,
STATEMENT_UPDATE, STATEMENT_REPLACE, STATEMENT_SELECT,
STATEMENT_OTHER
} pysqlite_StatementKind;
extern PyTypeObject pysqlite_CursorType;
int pysqlite_cursor_init(pysqlite_Cursor* self, PyObject* args, PyObject* kwargs);
void pysqlite_cursor_dealloc(pysqlite_Cursor* self);
PyObject* pysqlite_cursor_execute(pysqlite_Cursor* self, PyObject* args);
PyObject* pysqlite_cursor_executemany(pysqlite_Cursor* self, PyObject* args);
PyObject* pysqlite_cursor_getiter(pysqlite_Cursor *self);
PyObject* pysqlite_cursor_iternext(pysqlite_Cursor *self);
PyObject* pysqlite_cursor_fetchone(pysqlite_Cursor* self, PyObject* args);
PyObject* pysqlite_cursor_fetchmany(pysqlite_Cursor* self, PyObject* args, PyObject* kwargs);
PyObject* pysqlite_cursor_fetchall(pysqlite_Cursor* self, PyObject* args);
PyObject* pysqlite_noop(pysqlite_Connection* self, PyObject* args);
PyObject* pysqlite_cursor_close(pysqlite_Cursor* self, PyObject* args);
int pysqlite_cursor_setup_types(void);
#define UNKNOWN (-1)
#endif
