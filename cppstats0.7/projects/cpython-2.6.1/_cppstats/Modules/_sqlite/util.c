#include "module.h"
#include "connection.h"
int pysqlite_step(sqlite3_stmt* statement, pysqlite_Connection* connection) {
int rc;
if (statement == NULL) {
rc = SQLITE_OK;
} else {
Py_BEGIN_ALLOW_THREADS
rc = sqlite3_step(statement);
Py_END_ALLOW_THREADS
}
return rc;
}
int _pysqlite_seterror(sqlite3* db, sqlite3_stmt* st) {
int errorcode;
if (st != NULL) {
(void)sqlite3_reset(st);
}
errorcode = sqlite3_errcode(db);
switch (errorcode) {
case SQLITE_OK:
PyErr_Clear();
break;
case SQLITE_INTERNAL:
case SQLITE_NOTFOUND:
PyErr_SetString(pysqlite_InternalError, sqlite3_errmsg(db));
break;
case SQLITE_NOMEM:
(void)PyErr_NoMemory();
break;
case SQLITE_ERROR:
case SQLITE_PERM:
case SQLITE_ABORT:
case SQLITE_BUSY:
case SQLITE_LOCKED:
case SQLITE_READONLY:
case SQLITE_INTERRUPT:
case SQLITE_IOERR:
case SQLITE_FULL:
case SQLITE_CANTOPEN:
case SQLITE_PROTOCOL:
case SQLITE_EMPTY:
case SQLITE_SCHEMA:
PyErr_SetString(pysqlite_OperationalError, sqlite3_errmsg(db));
break;
case SQLITE_CORRUPT:
PyErr_SetString(pysqlite_DatabaseError, sqlite3_errmsg(db));
break;
case SQLITE_TOOBIG:
PyErr_SetString(pysqlite_DataError, sqlite3_errmsg(db));
break;
case SQLITE_CONSTRAINT:
case SQLITE_MISMATCH:
PyErr_SetString(pysqlite_IntegrityError, sqlite3_errmsg(db));
break;
case SQLITE_MISUSE:
PyErr_SetString(pysqlite_ProgrammingError, sqlite3_errmsg(db));
break;
default:
PyErr_SetString(pysqlite_DatabaseError, sqlite3_errmsg(db));
break;
}
return errorcode;
}
