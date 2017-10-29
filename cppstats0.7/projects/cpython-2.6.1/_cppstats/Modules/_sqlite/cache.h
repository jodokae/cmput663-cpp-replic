#if !defined(PYSQLITE_CACHE_H)
#define PYSQLITE_CACHE_H
#include "Python.h"
typedef struct _pysqlite_Node {
PyObject_HEAD
PyObject* key;
PyObject* data;
long count;
struct _pysqlite_Node* prev;
struct _pysqlite_Node* next;
} pysqlite_Node;
typedef struct {
PyObject_HEAD
int size;
PyObject* mapping;
PyObject* factory;
pysqlite_Node* first;
pysqlite_Node* last;
int decref_factory;
} pysqlite_Cache;
extern PyTypeObject pysqlite_NodeType;
extern PyTypeObject pysqlite_CacheType;
int pysqlite_node_init(pysqlite_Node* self, PyObject* args, PyObject* kwargs);
void pysqlite_node_dealloc(pysqlite_Node* self);
int pysqlite_cache_init(pysqlite_Cache* self, PyObject* args, PyObject* kwargs);
void pysqlite_cache_dealloc(pysqlite_Cache* self);
PyObject* pysqlite_cache_get(pysqlite_Cache* self, PyObject* args);
int pysqlite_cache_setup_types(void);
#endif