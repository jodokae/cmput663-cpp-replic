#if !defined(_BSDDB_H_)
#define _BSDDB_H_
#include <db.h>
#define DBVER (DB_VERSION_MAJOR * 10 + DB_VERSION_MINOR)
#if DB_VERSION_MINOR > 9
#error "eek! DBVER can't handle minor versions > 9"
#endif
#define PY_BSDDB_VERSION "4.7.3"
struct behaviourFlags {
unsigned int getReturnsNone : 1;
unsigned int cursorSetReturnsNone : 1;
};
struct DBObject;
struct DBCursorObject;
struct DBTxnObject;
struct DBSequenceObject;
typedef struct {
PyObject_HEAD
DB_ENV* db_env;
u_int32_t flags;
int closed;
struct behaviourFlags moduleFlags;
PyObject* event_notifyCallback;
struct DBObject *children_dbs;
struct DBTxnObject *children_txns;
PyObject *private_obj;
PyObject *rep_transport;
PyObject *in_weakreflist;
} DBEnvObject;
typedef struct DBObject {
PyObject_HEAD
DB* db;
DBEnvObject* myenvobj;
u_int32_t flags;
u_int32_t setflags;
int haveStat;
struct behaviourFlags moduleFlags;
struct DBTxnObject *txn;
struct DBCursorObject *children_cursors;
#if (DBVER >=43)
struct DBSequenceObject *children_sequences;
#endif
struct DBObject **sibling_prev_p;
struct DBObject *sibling_next;
struct DBObject **sibling_prev_p_txn;
struct DBObject *sibling_next_txn;
PyObject* associateCallback;
PyObject* btCompareCallback;
int primaryDBType;
PyObject *private_obj;
PyObject *in_weakreflist;
} DBObject;
typedef struct DBCursorObject {
PyObject_HEAD
DBC* dbc;
struct DBCursorObject **sibling_prev_p;
struct DBCursorObject *sibling_next;
struct DBCursorObject **sibling_prev_p_txn;
struct DBCursorObject *sibling_next_txn;
DBObject* mydb;
struct DBTxnObject *txn;
PyObject *in_weakreflist;
} DBCursorObject;
typedef struct DBTxnObject {
PyObject_HEAD
DB_TXN* txn;
DBEnvObject* env;
int flag_prepare;
struct DBTxnObject *parent_txn;
struct DBTxnObject **sibling_prev_p;
struct DBTxnObject *sibling_next;
struct DBTxnObject *children_txns;
struct DBObject *children_dbs;
struct DBSequenceObject *children_sequences;
struct DBCursorObject *children_cursors;
PyObject *in_weakreflist;
} DBTxnObject;
typedef struct {
PyObject_HEAD
DB_LOCK lock;
PyObject *in_weakreflist;
} DBLockObject;
#if (DBVER >= 43)
typedef struct DBSequenceObject {
PyObject_HEAD
DB_SEQUENCE* sequence;
DBObject* mydb;
struct DBTxnObject *txn;
struct DBSequenceObject **sibling_prev_p;
struct DBSequenceObject *sibling_next;
struct DBSequenceObject **sibling_prev_p_txn;
struct DBSequenceObject *sibling_next_txn;
PyObject *in_weakreflist;
} DBSequenceObject;
#endif
typedef struct {
PyTypeObject* db_type;
PyTypeObject* dbcursor_type;
PyTypeObject* dbenv_type;
PyTypeObject* dbtxn_type;
PyTypeObject* dblock_type;
#if (DBVER >= 43)
PyTypeObject* dbsequence_type;
#endif
int (*makeDBError)(int err);
} BSDDB_api;
#if !defined(COMPILING_BSDDB_C)
#define DBObject_Check(v) ((v)->ob_type == bsddb_api->db_type)
#define DBCursorObject_Check(v) ((v)->ob_type == bsddb_api->dbcursor_type)
#define DBEnvObject_Check(v) ((v)->ob_type == bsddb_api->dbenv_type)
#define DBTxnObject_Check(v) ((v)->ob_type == bsddb_api->dbtxn_type)
#define DBLockObject_Check(v) ((v)->ob_type == bsddb_api->dblock_type)
#if (DBVER >= 43)
#define DBSequenceObject_Check(v) ((v)->ob_type == bsddb_api->dbsequence_type)
#endif
#endif
#endif