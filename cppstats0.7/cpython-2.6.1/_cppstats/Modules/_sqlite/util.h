#if !defined(PYSQLITE_UTIL_H)
#define PYSQLITE_UTIL_H
#include "Python.h"
#include "pythread.h"
#include "sqlite3.h"
#include "connection.h"
int pysqlite_step(sqlite3_stmt* statement, pysqlite_Connection* connection);
int _pysqlite_seterror(sqlite3* db, sqlite3_stmt* st);
#endif
