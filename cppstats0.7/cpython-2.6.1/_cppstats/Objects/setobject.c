#include "Python.h"
#include "structmember.h"
static void
set_key_error(PyObject *arg) {
PyObject *tup;
tup = PyTuple_Pack(1, arg);
if (!tup)
return;
PyErr_SetObject(PyExc_KeyError, tup);
Py_DECREF(tup);
}
#define PERTURB_SHIFT 5
static PyObject *dummy = NULL;
#if defined(Py_REF_DEBUG)
PyObject *
_PySet_Dummy(void) {
return dummy;
}
#endif
#define INIT_NONZERO_SET_SLOTS(so) do { (so)->table = (so)->smalltable; (so)->mask = PySet_MINSIZE - 1; (so)->hash = -1; } while(0)
#define EMPTY_TO_MINSIZE(so) do { memset((so)->smalltable, 0, sizeof((so)->smalltable)); (so)->used = (so)->fill = 0; INIT_NONZERO_SET_SLOTS(so); } while(0)
#if !defined(PySet_MAXFREELIST)
#define PySet_MAXFREELIST 80
#endif
static PySetObject *free_list[PySet_MAXFREELIST];
static int numfree = 0;
static setentry *
set_lookkey(PySetObject *so, PyObject *key, register long hash) {
register Py_ssize_t i;
register size_t perturb;
register setentry *freeslot;
register size_t mask = so->mask;
setentry *table = so->table;
register setentry *entry;
register int cmp;
PyObject *startkey;
i = hash & mask;
entry = &table[i];
if (entry->key == NULL || entry->key == key)
return entry;
if (entry->key == dummy)
freeslot = entry;
else {
if (entry->hash == hash) {
startkey = entry->key;
Py_INCREF(startkey);
cmp = PyObject_RichCompareBool(startkey, key, Py_EQ);
Py_DECREF(startkey);
if (cmp < 0)
return NULL;
if (table == so->table && entry->key == startkey) {
if (cmp > 0)
return entry;
} else {
return set_lookkey(so, key, hash);
}
}
freeslot = NULL;
}
for (perturb = hash; ; perturb >>= PERTURB_SHIFT) {
i = (i << 2) + i + perturb + 1;
entry = &table[i & mask];
if (entry->key == NULL) {
if (freeslot != NULL)
entry = freeslot;
break;
}
if (entry->key == key)
break;
if (entry->hash == hash && entry->key != dummy) {
startkey = entry->key;
Py_INCREF(startkey);
cmp = PyObject_RichCompareBool(startkey, key, Py_EQ);
Py_DECREF(startkey);
if (cmp < 0)
return NULL;
if (table == so->table && entry->key == startkey) {
if (cmp > 0)
break;
} else {
return set_lookkey(so, key, hash);
}
} else if (entry->key == dummy && freeslot == NULL)
freeslot = entry;
}
return entry;
}
static setentry *
set_lookkey_string(PySetObject *so, PyObject *key, register long hash) {
register Py_ssize_t i;
register size_t perturb;
register setentry *freeslot;
register size_t mask = so->mask;
setentry *table = so->table;
register setentry *entry;
if (!PyString_CheckExact(key)) {
so->lookup = set_lookkey;
return set_lookkey(so, key, hash);
}
i = hash & mask;
entry = &table[i];
if (entry->key == NULL || entry->key == key)
return entry;
if (entry->key == dummy)
freeslot = entry;
else {
if (entry->hash == hash && _PyString_Eq(entry->key, key))
return entry;
freeslot = NULL;
}
for (perturb = hash; ; perturb >>= PERTURB_SHIFT) {
i = (i << 2) + i + perturb + 1;
entry = &table[i & mask];
if (entry->key == NULL)
return freeslot == NULL ? entry : freeslot;
if (entry->key == key
|| (entry->hash == hash
&& entry->key != dummy
&& _PyString_Eq(entry->key, key)))
return entry;
if (entry->key == dummy && freeslot == NULL)
freeslot = entry;
}
assert(0);
return 0;
}
static int
set_insert_key(register PySetObject *so, PyObject *key, long hash) {
register setentry *entry;
typedef setentry *(*lookupfunc)(PySetObject *, PyObject *, long);
assert(so->lookup != NULL);
entry = so->lookup(so, key, hash);
if (entry == NULL)
return -1;
if (entry->key == NULL) {
so->fill++;
entry->key = key;
entry->hash = hash;
so->used++;
} else if (entry->key == dummy) {
entry->key = key;
entry->hash = hash;
so->used++;
Py_DECREF(dummy);
} else {
Py_DECREF(key);
}
return 0;
}
static void
set_insert_clean(register PySetObject *so, PyObject *key, long hash) {
register size_t i;
register size_t perturb;
register size_t mask = (size_t)so->mask;
setentry *table = so->table;
register setentry *entry;
i = hash & mask;
entry = &table[i];
for (perturb = hash; entry->key != NULL; perturb >>= PERTURB_SHIFT) {
i = (i << 2) + i + perturb + 1;
entry = &table[i & mask];
}
so->fill++;
entry->key = key;
entry->hash = hash;
so->used++;
}
static int
set_table_resize(PySetObject *so, Py_ssize_t minused) {
Py_ssize_t newsize;
setentry *oldtable, *newtable, *entry;
Py_ssize_t i;
int is_oldtable_malloced;
setentry small_copy[PySet_MINSIZE];
assert(minused >= 0);
for (newsize = PySet_MINSIZE;
newsize <= minused && newsize > 0;
newsize <<= 1)
;
if (newsize <= 0) {
PyErr_NoMemory();
return -1;
}
oldtable = so->table;
assert(oldtable != NULL);
is_oldtable_malloced = oldtable != so->smalltable;
if (newsize == PySet_MINSIZE) {
newtable = so->smalltable;
if (newtable == oldtable) {
if (so->fill == so->used) {
return 0;
}
assert(so->fill > so->used);
memcpy(small_copy, oldtable, sizeof(small_copy));
oldtable = small_copy;
}
} else {
newtable = PyMem_NEW(setentry, newsize);
if (newtable == NULL) {
PyErr_NoMemory();
return -1;
}
}
assert(newtable != oldtable);
so->table = newtable;
so->mask = newsize - 1;
memset(newtable, 0, sizeof(setentry) * newsize);
so->used = 0;
i = so->fill;
so->fill = 0;
for (entry = oldtable; i > 0; entry++) {
if (entry->key == NULL) {
;
} else if (entry->key == dummy) {
--i;
assert(entry->key == dummy);
Py_DECREF(entry->key);
} else {
--i;
set_insert_clean(so, entry->key, entry->hash);
}
}
if (is_oldtable_malloced)
PyMem_DEL(oldtable);
return 0;
}
static int
set_add_entry(register PySetObject *so, setentry *entry) {
register Py_ssize_t n_used;
assert(so->fill <= so->mask);
n_used = so->used;
Py_INCREF(entry->key);
if (set_insert_key(so, entry->key, entry->hash) == -1) {
Py_DECREF(entry->key);
return -1;
}
if (!(so->used > n_used && so->fill*3 >= (so->mask+1)*2))
return 0;
return set_table_resize(so, so->used>50000 ? so->used*2 : so->used*4);
}
static int
set_add_key(register PySetObject *so, PyObject *key) {
register long hash;
register Py_ssize_t n_used;
if (!PyString_CheckExact(key) ||
(hash = ((PyStringObject *) key)->ob_shash) == -1) {
hash = PyObject_Hash(key);
if (hash == -1)
return -1;
}
assert(so->fill <= so->mask);
n_used = so->used;
Py_INCREF(key);
if (set_insert_key(so, key, hash) == -1) {
Py_DECREF(key);
return -1;
}
if (!(so->used > n_used && so->fill*3 >= (so->mask+1)*2))
return 0;
return set_table_resize(so, so->used>50000 ? so->used*2 : so->used*4);
}
#define DISCARD_NOTFOUND 0
#define DISCARD_FOUND 1
static int
set_discard_entry(PySetObject *so, setentry *oldentry) {
register setentry *entry;
PyObject *old_key;
entry = (so->lookup)(so, oldentry->key, oldentry->hash);
if (entry == NULL)
return -1;
if (entry->key == NULL || entry->key == dummy)
return DISCARD_NOTFOUND;
old_key = entry->key;
Py_INCREF(dummy);
entry->key = dummy;
so->used--;
Py_DECREF(old_key);
return DISCARD_FOUND;
}
static int
set_discard_key(PySetObject *so, PyObject *key) {
register long hash;
register setentry *entry;
PyObject *old_key;
assert (PyAnySet_Check(so));
if (!PyString_CheckExact(key) ||
(hash = ((PyStringObject *) key)->ob_shash) == -1) {
hash = PyObject_Hash(key);
if (hash == -1)
return -1;
}
entry = (so->lookup)(so, key, hash);
if (entry == NULL)
return -1;
if (entry->key == NULL || entry->key == dummy)
return DISCARD_NOTFOUND;
old_key = entry->key;
Py_INCREF(dummy);
entry->key = dummy;
so->used--;
Py_DECREF(old_key);
return DISCARD_FOUND;
}
static int
set_clear_internal(PySetObject *so) {
setentry *entry, *table;
int table_is_malloced;
Py_ssize_t fill;
setentry small_copy[PySet_MINSIZE];
#if defined(Py_DEBUG)
Py_ssize_t i, n;
assert (PyAnySet_Check(so));
n = so->mask + 1;
i = 0;
#endif
table = so->table;
assert(table != NULL);
table_is_malloced = table != so->smalltable;
fill = so->fill;
if (table_is_malloced)
EMPTY_TO_MINSIZE(so);
else if (fill > 0) {
memcpy(small_copy, table, sizeof(small_copy));
table = small_copy;
EMPTY_TO_MINSIZE(so);
}
for (entry = table; fill > 0; ++entry) {
#if defined(Py_DEBUG)
assert(i < n);
++i;
#endif
if (entry->key) {
--fill;
Py_DECREF(entry->key);
}
#if defined(Py_DEBUG)
else
assert(entry->key == NULL);
#endif
}
if (table_is_malloced)
PyMem_DEL(table);
return 0;
}
static int
set_next(PySetObject *so, Py_ssize_t *pos_ptr, setentry **entry_ptr) {
Py_ssize_t i;
Py_ssize_t mask;
register setentry *table;
assert (PyAnySet_Check(so));
i = *pos_ptr;
assert(i >= 0);
table = so->table;
mask = so->mask;
while (i <= mask && (table[i].key == NULL || table[i].key == dummy))
i++;
*pos_ptr = i+1;
if (i > mask)
return 0;
assert(table[i].key != NULL);
*entry_ptr = &table[i];
return 1;
}
static void
set_dealloc(PySetObject *so) {
register setentry *entry;
Py_ssize_t fill = so->fill;
PyObject_GC_UnTrack(so);
Py_TRASHCAN_SAFE_BEGIN(so)
if (so->weakreflist != NULL)
PyObject_ClearWeakRefs((PyObject *) so);
for (entry = so->table; fill > 0; entry++) {
if (entry->key) {
--fill;
Py_DECREF(entry->key);
}
}
if (so->table != so->smalltable)
PyMem_DEL(so->table);
if (numfree < PySet_MAXFREELIST && PyAnySet_CheckExact(so))
free_list[numfree++] = so;
else
Py_TYPE(so)->tp_free(so);
Py_TRASHCAN_SAFE_END(so)
}
static int
set_tp_print(PySetObject *so, FILE *fp, int flags) {
setentry *entry;
Py_ssize_t pos=0;
char *emit = "";
char *separator = ", ";
int status = Py_ReprEnter((PyObject*)so);
if (status != 0) {
if (status < 0)
return status;
Py_BEGIN_ALLOW_THREADS
fprintf(fp, "%s(...)", so->ob_type->tp_name);
Py_END_ALLOW_THREADS
return 0;
}
Py_BEGIN_ALLOW_THREADS
fprintf(fp, "%s([", so->ob_type->tp_name);
Py_END_ALLOW_THREADS
while (set_next(so, &pos, &entry)) {
Py_BEGIN_ALLOW_THREADS
fputs(emit, fp);
Py_END_ALLOW_THREADS
emit = separator;
if (PyObject_Print(entry->key, fp, 0) != 0) {
Py_ReprLeave((PyObject*)so);
return -1;
}
}
Py_BEGIN_ALLOW_THREADS
fputs("])", fp);
Py_END_ALLOW_THREADS
Py_ReprLeave((PyObject*)so);
return 0;
}
static PyObject *
set_repr(PySetObject *so) {
PyObject *keys, *result=NULL, *listrepr;
int status = Py_ReprEnter((PyObject*)so);
if (status != 0) {
if (status < 0)
return NULL;
return PyString_FromFormat("%s(...)", so->ob_type->tp_name);
}
keys = PySequence_List((PyObject *)so);
if (keys == NULL)
goto done;
listrepr = PyObject_Repr(keys);
Py_DECREF(keys);
if (listrepr == NULL)
goto done;
result = PyString_FromFormat("%s(%s)", so->ob_type->tp_name,
PyString_AS_STRING(listrepr));
Py_DECREF(listrepr);
done:
Py_ReprLeave((PyObject*)so);
return result;
}
static Py_ssize_t
set_len(PyObject *so) {
return ((PySetObject *)so)->used;
}
static int
set_merge(PySetObject *so, PyObject *otherset) {
PySetObject *other;
register Py_ssize_t i;
register setentry *entry;
assert (PyAnySet_Check(so));
assert (PyAnySet_Check(otherset));
other = (PySetObject*)otherset;
if (other == so || other->used == 0)
return 0;
if ((so->fill + other->used)*3 >= (so->mask+1)*2) {
if (set_table_resize(so, (so->used + other->used)*2) != 0)
return -1;
}
for (i = 0; i <= other->mask; i++) {
entry = &other->table[i];
if (entry->key != NULL &&
entry->key != dummy) {
Py_INCREF(entry->key);
if (set_insert_key(so, entry->key, entry->hash) == -1) {
Py_DECREF(entry->key);
return -1;
}
}
}
return 0;
}
static int
set_contains_key(PySetObject *so, PyObject *key) {
long hash;
setentry *entry;
if (!PyString_CheckExact(key) ||
(hash = ((PyStringObject *) key)->ob_shash) == -1) {
hash = PyObject_Hash(key);
if (hash == -1)
return -1;
}
entry = (so->lookup)(so, key, hash);
if (entry == NULL)
return -1;
key = entry->key;
return key != NULL && key != dummy;
}
static int
set_contains_entry(PySetObject *so, setentry *entry) {
PyObject *key;
setentry *lu_entry;
lu_entry = (so->lookup)(so, entry->key, entry->hash);
if (lu_entry == NULL)
return -1;
key = lu_entry->key;
return key != NULL && key != dummy;
}
static PyObject *
set_pop(PySetObject *so) {
register Py_ssize_t i = 0;
register setentry *entry;
PyObject *key;
assert (PyAnySet_Check(so));
if (so->used == 0) {
PyErr_SetString(PyExc_KeyError, "pop from an empty set");
return NULL;
}
entry = &so->table[0];
if (entry->key == NULL || entry->key == dummy) {
i = entry->hash;
if (i > so->mask || i < 1)
i = 1;
while ((entry = &so->table[i])->key == NULL || entry->key==dummy) {
i++;
if (i > so->mask)
i = 1;
}
}
key = entry->key;
Py_INCREF(dummy);
entry->key = dummy;
so->used--;
so->table[0].hash = i + 1;
return key;
}
PyDoc_STRVAR(pop_doc, "Remove and return an arbitrary set element.\n\
Raises KeyError if the set is empty.");
static int
set_traverse(PySetObject *so, visitproc visit, void *arg) {
Py_ssize_t pos = 0;
setentry *entry;
while (set_next(so, &pos, &entry))
Py_VISIT(entry->key);
return 0;
}
static long
frozenset_hash(PyObject *self) {
PySetObject *so = (PySetObject *)self;
long h, hash = 1927868237L;
setentry *entry;
Py_ssize_t pos = 0;
if (so->hash != -1)
return so->hash;
hash *= PySet_GET_SIZE(self) + 1;
while (set_next(so, &pos, &entry)) {
h = entry->hash;
hash ^= (h ^ (h << 16) ^ 89869747L) * 3644798167u;
}
hash = hash * 69069L + 907133923L;
if (hash == -1)
hash = 590923713L;
so->hash = hash;
return hash;
}
typedef struct {
PyObject_HEAD
PySetObject *si_set;
Py_ssize_t si_used;
Py_ssize_t si_pos;
Py_ssize_t len;
} setiterobject;
static void
setiter_dealloc(setiterobject *si) {
Py_XDECREF(si->si_set);
PyObject_Del(si);
}
static PyObject *
setiter_len(setiterobject *si) {
Py_ssize_t len = 0;
if (si->si_set != NULL && si->si_used == si->si_set->used)
len = si->len;
return PyInt_FromLong(len);
}
PyDoc_STRVAR(length_hint_doc, "Private method returning an estimate of len(list(it)).");
static PyMethodDef setiter_methods[] = {
{"__length_hint__", (PyCFunction)setiter_len, METH_NOARGS, length_hint_doc},
{NULL, NULL}
};
static PyObject *setiter_iternext(setiterobject *si) {
PyObject *key;
register Py_ssize_t i, mask;
register setentry *entry;
PySetObject *so = si->si_set;
if (so == NULL)
return NULL;
assert (PyAnySet_Check(so));
if (si->si_used != so->used) {
PyErr_SetString(PyExc_RuntimeError,
"Set changed size during iteration");
si->si_used = -1;
return NULL;
}
i = si->si_pos;
assert(i>=0);
entry = so->table;
mask = so->mask;
while (i <= mask && (entry[i].key == NULL || entry[i].key == dummy))
i++;
si->si_pos = i+1;
if (i > mask)
goto fail;
si->len--;
key = entry[i].key;
Py_INCREF(key);
return key;
fail:
Py_DECREF(so);
si->si_set = NULL;
return NULL;
}
static PyTypeObject PySetIter_Type = {
PyVarObject_HEAD_INIT(&PyType_Type, 0)
"setiterator",
sizeof(setiterobject),
0,
(destructor)setiter_dealloc,
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
PyObject_GenericGetAttr,
0,
0,
Py_TPFLAGS_DEFAULT,
0,
0,
0,
0,
0,
PyObject_SelfIter,
(iternextfunc)setiter_iternext,
setiter_methods,
0,
};
static PyObject *
set_iter(PySetObject *so) {
setiterobject *si = PyObject_New(setiterobject, &PySetIter_Type);
if (si == NULL)
return NULL;
Py_INCREF(so);
si->si_set = so;
si->si_used = so->used;
si->si_pos = 0;
si->len = so->used;
return (PyObject *)si;
}
static int
set_update_internal(PySetObject *so, PyObject *other) {
PyObject *key, *it;
if (PyAnySet_Check(other))
return set_merge(so, other);
if (PyDict_CheckExact(other)) {
PyObject *value;
Py_ssize_t pos = 0;
long hash;
Py_ssize_t dictsize = PyDict_Size(other);
if (dictsize == -1)
return -1;
if ((so->fill + dictsize)*3 >= (so->mask+1)*2) {
if (set_table_resize(so, (so->used + dictsize)*2) != 0)
return -1;
}
while (_PyDict_Next(other, &pos, &key, &value, &hash)) {
setentry an_entry;
an_entry.hash = hash;
an_entry.key = key;
if (set_add_entry(so, &an_entry) == -1)
return -1;
}
return 0;
}
it = PyObject_GetIter(other);
if (it == NULL)
return -1;
while ((key = PyIter_Next(it)) != NULL) {
if (set_add_key(so, key) == -1) {
Py_DECREF(it);
Py_DECREF(key);
return -1;
}
Py_DECREF(key);
}
Py_DECREF(it);
if (PyErr_Occurred())
return -1;
return 0;
}
static PyObject *
set_update(PySetObject *so, PyObject *args) {
Py_ssize_t i;
for (i=0 ; i<PyTuple_GET_SIZE(args) ; i++) {
PyObject *other = PyTuple_GET_ITEM(args, i);
if (set_update_internal(so, other) == -1)
return NULL;
}
Py_RETURN_NONE;
}
PyDoc_STRVAR(update_doc,
"Update a set with the union of itself and others.");
static PyObject *
make_new_set(PyTypeObject *type, PyObject *iterable) {
register PySetObject *so = NULL;
if (dummy == NULL) {
dummy = PyString_FromString("<dummy key>");
if (dummy == NULL)
return NULL;
}
if (numfree &&
(type == &PySet_Type || type == &PyFrozenSet_Type)) {
so = free_list[--numfree];
assert (so != NULL && PyAnySet_CheckExact(so));
Py_TYPE(so) = type;
_Py_NewReference((PyObject *)so);
EMPTY_TO_MINSIZE(so);
PyObject_GC_Track(so);
} else {
so = (PySetObject *)type->tp_alloc(type, 0);
if (so == NULL)
return NULL;
assert(so->table == NULL && so->fill == 0 && so->used == 0);
INIT_NONZERO_SET_SLOTS(so);
}
so->lookup = set_lookkey_string;
so->weakreflist = NULL;
if (iterable != NULL) {
if (set_update_internal(so, iterable) == -1) {
Py_DECREF(so);
return NULL;
}
}
return (PyObject *)so;
}
static PyObject *emptyfrozenset = NULL;
static PyObject *
frozenset_new(PyTypeObject *type, PyObject *args, PyObject *kwds) {
PyObject *iterable = NULL, *result;
if (type == &PyFrozenSet_Type && !_PyArg_NoKeywords("frozenset()", kwds))
return NULL;
if (!PyArg_UnpackTuple(args, type->tp_name, 0, 1, &iterable))
return NULL;
if (type != &PyFrozenSet_Type)
return make_new_set(type, iterable);
if (iterable != NULL) {
if (PyFrozenSet_CheckExact(iterable)) {
Py_INCREF(iterable);
return iterable;
}
result = make_new_set(type, iterable);
if (result == NULL || PySet_GET_SIZE(result))
return result;
Py_DECREF(result);
}
if (emptyfrozenset == NULL)
emptyfrozenset = make_new_set(type, NULL);
Py_XINCREF(emptyfrozenset);
return emptyfrozenset;
}
void
PySet_Fini(void) {
PySetObject *so;
while (numfree) {
numfree--;
so = free_list[numfree];
PyObject_GC_Del(so);
}
Py_CLEAR(dummy);
Py_CLEAR(emptyfrozenset);
}
static PyObject *
set_new(PyTypeObject *type, PyObject *args, PyObject *kwds) {
if (type == &PySet_Type && !_PyArg_NoKeywords("set()", kwds))
return NULL;
return make_new_set(type, NULL);
}
static void
set_swap_bodies(PySetObject *a, PySetObject *b) {
Py_ssize_t t;
setentry *u;
setentry *(*f)(PySetObject *so, PyObject *key, long hash);
setentry tab[PySet_MINSIZE];
long h;
t = a->fill;
a->fill = b->fill;
b->fill = t;
t = a->used;
a->used = b->used;
b->used = t;
t = a->mask;
a->mask = b->mask;
b->mask = t;
u = a->table;
if (a->table == a->smalltable)
u = b->smalltable;
a->table = b->table;
if (b->table == b->smalltable)
a->table = a->smalltable;
b->table = u;
f = a->lookup;
a->lookup = b->lookup;
b->lookup = f;
if (a->table == a->smalltable || b->table == b->smalltable) {
memcpy(tab, a->smalltable, sizeof(tab));
memcpy(a->smalltable, b->smalltable, sizeof(tab));
memcpy(b->smalltable, tab, sizeof(tab));
}
if (PyType_IsSubtype(Py_TYPE(a), &PyFrozenSet_Type) &&
PyType_IsSubtype(Py_TYPE(b), &PyFrozenSet_Type)) {
h = a->hash;
a->hash = b->hash;
b->hash = h;
} else {
a->hash = -1;
b->hash = -1;
}
}
static PyObject *
set_copy(PySetObject *so) {
return make_new_set(Py_TYPE(so), (PyObject *)so);
}
static PyObject *
frozenset_copy(PySetObject *so) {
if (PyFrozenSet_CheckExact(so)) {
Py_INCREF(so);
return (PyObject *)so;
}
return set_copy(so);
}
PyDoc_STRVAR(copy_doc, "Return a shallow copy of a set.");
static PyObject *
set_clear(PySetObject *so) {
set_clear_internal(so);
Py_RETURN_NONE;
}
PyDoc_STRVAR(clear_doc, "Remove all elements from this set.");
static PyObject *
set_union(PySetObject *so, PyObject *args) {
PySetObject *result;
PyObject *other;
Py_ssize_t i;
result = (PySetObject *)set_copy(so);
if (result == NULL)
return NULL;
for (i=0 ; i<PyTuple_GET_SIZE(args) ; i++) {
other = PyTuple_GET_ITEM(args, i);
if ((PyObject *)so == other)
return (PyObject *)result;
if (set_update_internal(result, other) == -1) {
Py_DECREF(result);
return NULL;
}
}
return (PyObject *)result;
}
PyDoc_STRVAR(union_doc,
"Return the union of sets as a new set.\n\
\n\
(i.e. all elements that are in either set.)");
static PyObject *
set_or(PySetObject *so, PyObject *other) {
PySetObject *result;
if (!PyAnySet_Check(so) || !PyAnySet_Check(other)) {
Py_INCREF(Py_NotImplemented);
return Py_NotImplemented;
}
result = (PySetObject *)set_copy(so);
if (result == NULL)
return NULL;
if ((PyObject *)so == other)
return (PyObject *)result;
if (set_update_internal(result, other) == -1) {
Py_DECREF(result);
return NULL;
}
return (PyObject *)result;
}
static PyObject *
set_ior(PySetObject *so, PyObject *other) {
if (!PyAnySet_Check(other)) {
Py_INCREF(Py_NotImplemented);
return Py_NotImplemented;
}
if (set_update_internal(so, other) == -1)
return NULL;
Py_INCREF(so);
return (PyObject *)so;
}
static PyObject *
set_intersection(PySetObject *so, PyObject *other) {
PySetObject *result;
PyObject *key, *it, *tmp;
if ((PyObject *)so == other)
return set_copy(so);
result = (PySetObject *)make_new_set(Py_TYPE(so), NULL);
if (result == NULL)
return NULL;
if (PyAnySet_Check(other)) {
Py_ssize_t pos = 0;
setentry *entry;
if (PySet_GET_SIZE(other) > PySet_GET_SIZE(so)) {
tmp = (PyObject *)so;
so = (PySetObject *)other;
other = tmp;
}
while (set_next((PySetObject *)other, &pos, &entry)) {
int rv = set_contains_entry(so, entry);
if (rv == -1) {
Py_DECREF(result);
return NULL;
}
if (rv) {
if (set_add_entry(result, entry) == -1) {
Py_DECREF(result);
return NULL;
}
}
}
return (PyObject *)result;
}
it = PyObject_GetIter(other);
if (it == NULL) {
Py_DECREF(result);
return NULL;
}
while ((key = PyIter_Next(it)) != NULL) {
int rv;
setentry entry;
long hash = PyObject_Hash(key);
if (hash == -1) {
Py_DECREF(it);
Py_DECREF(result);
Py_DECREF(key);
return NULL;
}
entry.hash = hash;
entry.key = key;
rv = set_contains_entry(so, &entry);
if (rv == -1) {
Py_DECREF(it);
Py_DECREF(result);
Py_DECREF(key);
return NULL;
}
if (rv) {
if (set_add_entry(result, &entry) == -1) {
Py_DECREF(it);
Py_DECREF(result);
Py_DECREF(key);
return NULL;
}
}
Py_DECREF(key);
}
Py_DECREF(it);
if (PyErr_Occurred()) {
Py_DECREF(result);
return NULL;
}
return (PyObject *)result;
}
static PyObject *
set_intersection_multi(PySetObject *so, PyObject *args) {
Py_ssize_t i;
PyObject *result = (PyObject *)so;
if (PyTuple_GET_SIZE(args) == 0)
return set_copy(so);
Py_INCREF(so);
for (i=0 ; i<PyTuple_GET_SIZE(args) ; i++) {
PyObject *other = PyTuple_GET_ITEM(args, i);
PyObject *newresult = set_intersection((PySetObject *)result, other);
if (newresult == NULL) {
Py_DECREF(result);
return NULL;
}
Py_DECREF(result);
result = newresult;
}
return result;
}
PyDoc_STRVAR(intersection_doc,
"Return the intersection of two sets as a new set.\n\
\n\
(i.e. all elements that are in both sets.)");
static PyObject *
set_intersection_update(PySetObject *so, PyObject *other) {
PyObject *tmp;
tmp = set_intersection(so, other);
if (tmp == NULL)
return NULL;
set_swap_bodies(so, (PySetObject *)tmp);
Py_DECREF(tmp);
Py_RETURN_NONE;
}
static PyObject *
set_intersection_update_multi(PySetObject *so, PyObject *args) {
PyObject *tmp;
tmp = set_intersection_multi(so, args);
if (tmp == NULL)
return NULL;
set_swap_bodies(so, (PySetObject *)tmp);
Py_DECREF(tmp);
Py_RETURN_NONE;
}
PyDoc_STRVAR(intersection_update_doc,
"Update a set with the intersection of itself and another.");
static PyObject *
set_and(PySetObject *so, PyObject *other) {
if (!PyAnySet_Check(so) || !PyAnySet_Check(other)) {
Py_INCREF(Py_NotImplemented);
return Py_NotImplemented;
}
return set_intersection(so, other);
}
static PyObject *
set_iand(PySetObject *so, PyObject *other) {
PyObject *result;
if (!PyAnySet_Check(other)) {
Py_INCREF(Py_NotImplemented);
return Py_NotImplemented;
}
result = set_intersection_update(so, other);
if (result == NULL)
return NULL;
Py_DECREF(result);
Py_INCREF(so);
return (PyObject *)so;
}
static PyObject *
set_isdisjoint(PySetObject *so, PyObject *other) {
PyObject *key, *it, *tmp;
if ((PyObject *)so == other) {
if (PySet_GET_SIZE(so) == 0)
Py_RETURN_TRUE;
else
Py_RETURN_FALSE;
}
if (PyAnySet_CheckExact(other)) {
Py_ssize_t pos = 0;
setentry *entry;
if (PySet_GET_SIZE(other) > PySet_GET_SIZE(so)) {
tmp = (PyObject *)so;
so = (PySetObject *)other;
other = tmp;
}
while (set_next((PySetObject *)other, &pos, &entry)) {
int rv = set_contains_entry(so, entry);
if (rv == -1)
return NULL;
if (rv)
Py_RETURN_FALSE;
}
Py_RETURN_TRUE;
}
it = PyObject_GetIter(other);
if (it == NULL)
return NULL;
while ((key = PyIter_Next(it)) != NULL) {
int rv;
setentry entry;
long hash = PyObject_Hash(key);
if (hash == -1) {
Py_DECREF(key);
Py_DECREF(it);
return NULL;
}
entry.hash = hash;
entry.key = key;
rv = set_contains_entry(so, &entry);
Py_DECREF(key);
if (rv == -1) {
Py_DECREF(it);
return NULL;
}
if (rv) {
Py_DECREF(it);
Py_RETURN_FALSE;
}
}
Py_DECREF(it);
if (PyErr_Occurred())
return NULL;
Py_RETURN_TRUE;
}
PyDoc_STRVAR(isdisjoint_doc,
"Return True if two sets have a null intersection.");
static int
set_difference_update_internal(PySetObject *so, PyObject *other) {
if ((PyObject *)so == other)
return set_clear_internal(so);
if (PyAnySet_Check(other)) {
setentry *entry;
Py_ssize_t pos = 0;
while (set_next((PySetObject *)other, &pos, &entry))
if (set_discard_entry(so, entry) == -1)
return -1;
} else {
PyObject *key, *it;
it = PyObject_GetIter(other);
if (it == NULL)
return -1;
while ((key = PyIter_Next(it)) != NULL) {
if (set_discard_key(so, key) == -1) {
Py_DECREF(it);
Py_DECREF(key);
return -1;
}
Py_DECREF(key);
}
Py_DECREF(it);
if (PyErr_Occurred())
return -1;
}
if ((so->fill - so->used) * 5 < so->mask)
return 0;
return set_table_resize(so, so->used>50000 ? so->used*2 : so->used*4);
}
static PyObject *
set_difference_update(PySetObject *so, PyObject *args) {
Py_ssize_t i;
for (i=0 ; i<PyTuple_GET_SIZE(args) ; i++) {
PyObject *other = PyTuple_GET_ITEM(args, i);
if (set_difference_update_internal(so, other) == -1)
return NULL;
}
Py_RETURN_NONE;
}
PyDoc_STRVAR(difference_update_doc,
"Remove all elements of another set from this set.");
static PyObject *
set_difference(PySetObject *so, PyObject *other) {
PyObject *result;
setentry *entry;
Py_ssize_t pos = 0;
if (!PyAnySet_Check(other) && !PyDict_CheckExact(other)) {
result = set_copy(so);
if (result == NULL)
return NULL;
if (set_difference_update_internal((PySetObject *)result, other) != -1)
return result;
Py_DECREF(result);
return NULL;
}
result = make_new_set(Py_TYPE(so), NULL);
if (result == NULL)
return NULL;
if (PyDict_CheckExact(other)) {
while (set_next(so, &pos, &entry)) {
setentry entrycopy;
entrycopy.hash = entry->hash;
entrycopy.key = entry->key;
if (!_PyDict_Contains(other, entry->key, entry->hash)) {
if (set_add_entry((PySetObject *)result, &entrycopy) == -1) {
Py_DECREF(result);
return NULL;
}
}
}
return result;
}
while (set_next(so, &pos, &entry)) {
int rv = set_contains_entry((PySetObject *)other, entry);
if (rv == -1) {
Py_DECREF(result);
return NULL;
}
if (!rv) {
if (set_add_entry((PySetObject *)result, entry) == -1) {
Py_DECREF(result);
return NULL;
}
}
}
return result;
}
static PyObject *
set_difference_multi(PySetObject *so, PyObject *args) {
Py_ssize_t i;
PyObject *result, *other;
if (PyTuple_GET_SIZE(args) == 0)
return set_copy(so);
other = PyTuple_GET_ITEM(args, 0);
result = set_difference(so, other);
if (result == NULL)
return NULL;
for (i=1 ; i<PyTuple_GET_SIZE(args) ; i++) {
other = PyTuple_GET_ITEM(args, i);
if (set_difference_update_internal((PySetObject *)result, other) == -1) {
Py_DECREF(result);
return NULL;
}
}
return result;
}
PyDoc_STRVAR(difference_doc,
"Return the difference of two or more sets as a new set.\n\
\n\
(i.e. all elements that are in this set but not the others.)");
static PyObject *
set_sub(PySetObject *so, PyObject *other) {
if (!PyAnySet_Check(so) || !PyAnySet_Check(other)) {
Py_INCREF(Py_NotImplemented);
return Py_NotImplemented;
}
return set_difference(so, other);
}
static PyObject *
set_isub(PySetObject *so, PyObject *other) {
if (!PyAnySet_Check(other)) {
Py_INCREF(Py_NotImplemented);
return Py_NotImplemented;
}
if (set_difference_update_internal(so, other) == -1)
return NULL;
Py_INCREF(so);
return (PyObject *)so;
}
static PyObject *
set_symmetric_difference_update(PySetObject *so, PyObject *other) {
PySetObject *otherset;
PyObject *key;
Py_ssize_t pos = 0;
setentry *entry;
if ((PyObject *)so == other)
return set_clear(so);
if (PyDict_CheckExact(other)) {
PyObject *value;
int rv;
long hash;
while (_PyDict_Next(other, &pos, &key, &value, &hash)) {
setentry an_entry;
an_entry.hash = hash;
an_entry.key = key;
rv = set_discard_entry(so, &an_entry);
if (rv == -1)
return NULL;
if (rv == DISCARD_NOTFOUND) {
if (set_add_entry(so, &an_entry) == -1)
return NULL;
}
}
Py_RETURN_NONE;
}
if (PyAnySet_Check(other)) {
Py_INCREF(other);
otherset = (PySetObject *)other;
} else {
otherset = (PySetObject *)make_new_set(Py_TYPE(so), other);
if (otherset == NULL)
return NULL;
}
while (set_next(otherset, &pos, &entry)) {
int rv = set_discard_entry(so, entry);
if (rv == -1) {
Py_DECREF(otherset);
return NULL;
}
if (rv == DISCARD_NOTFOUND) {
if (set_add_entry(so, entry) == -1) {
Py_DECREF(otherset);
return NULL;
}
}
}
Py_DECREF(otherset);
Py_RETURN_NONE;
}
PyDoc_STRVAR(symmetric_difference_update_doc,
"Update a set with the symmetric difference of itself and another.");
static PyObject *
set_symmetric_difference(PySetObject *so, PyObject *other) {
PyObject *rv;
PySetObject *otherset;
otherset = (PySetObject *)make_new_set(Py_TYPE(so), other);
if (otherset == NULL)
return NULL;
rv = set_symmetric_difference_update(otherset, (PyObject *)so);
if (rv == NULL)
return NULL;
Py_DECREF(rv);
return (PyObject *)otherset;
}
PyDoc_STRVAR(symmetric_difference_doc,
"Return the symmetric difference of two sets as a new set.\n\
\n\
(i.e. all elements that are in exactly one of the sets.)");
static PyObject *
set_xor(PySetObject *so, PyObject *other) {
if (!PyAnySet_Check(so) || !PyAnySet_Check(other)) {
Py_INCREF(Py_NotImplemented);
return Py_NotImplemented;
}
return set_symmetric_difference(so, other);
}
static PyObject *
set_ixor(PySetObject *so, PyObject *other) {
PyObject *result;
if (!PyAnySet_Check(other)) {
Py_INCREF(Py_NotImplemented);
return Py_NotImplemented;
}
result = set_symmetric_difference_update(so, other);
if (result == NULL)
return NULL;
Py_DECREF(result);
Py_INCREF(so);
return (PyObject *)so;
}
static PyObject *
set_issubset(PySetObject *so, PyObject *other) {
setentry *entry;
Py_ssize_t pos = 0;
if (!PyAnySet_Check(other)) {
PyObject *tmp, *result;
tmp = make_new_set(&PySet_Type, other);
if (tmp == NULL)
return NULL;
result = set_issubset(so, tmp);
Py_DECREF(tmp);
return result;
}
if (PySet_GET_SIZE(so) > PySet_GET_SIZE(other))
Py_RETURN_FALSE;
while (set_next(so, &pos, &entry)) {
int rv = set_contains_entry((PySetObject *)other, entry);
if (rv == -1)
return NULL;
if (!rv)
Py_RETURN_FALSE;
}
Py_RETURN_TRUE;
}
PyDoc_STRVAR(issubset_doc, "Report whether another set contains this set.");
static PyObject *
set_issuperset(PySetObject *so, PyObject *other) {
PyObject *tmp, *result;
if (!PyAnySet_Check(other)) {
tmp = make_new_set(&PySet_Type, other);
if (tmp == NULL)
return NULL;
result = set_issuperset(so, tmp);
Py_DECREF(tmp);
return result;
}
return set_issubset((PySetObject *)other, (PyObject *)so);
}
PyDoc_STRVAR(issuperset_doc, "Report whether this set contains another set.");
static PyObject *
set_richcompare(PySetObject *v, PyObject *w, int op) {
PyObject *r1, *r2;
if(!PyAnySet_Check(w)) {
if (op == Py_EQ)
Py_RETURN_FALSE;
if (op == Py_NE)
Py_RETURN_TRUE;
PyErr_SetString(PyExc_TypeError, "can only compare to a set");
return NULL;
}
switch (op) {
case Py_EQ:
if (PySet_GET_SIZE(v) != PySet_GET_SIZE(w))
Py_RETURN_FALSE;
if (v->hash != -1 &&
((PySetObject *)w)->hash != -1 &&
v->hash != ((PySetObject *)w)->hash)
Py_RETURN_FALSE;
return set_issubset(v, w);
case Py_NE:
r1 = set_richcompare(v, w, Py_EQ);
if (r1 == NULL)
return NULL;
r2 = PyBool_FromLong(PyObject_Not(r1));
Py_DECREF(r1);
return r2;
case Py_LE:
return set_issubset(v, w);
case Py_GE:
return set_issuperset(v, w);
case Py_LT:
if (PySet_GET_SIZE(v) >= PySet_GET_SIZE(w))
Py_RETURN_FALSE;
return set_issubset(v, w);
case Py_GT:
if (PySet_GET_SIZE(v) <= PySet_GET_SIZE(w))
Py_RETURN_FALSE;
return set_issuperset(v, w);
}
Py_INCREF(Py_NotImplemented);
return Py_NotImplemented;
}
static int
set_nocmp(PyObject *self, PyObject *other) {
PyErr_SetString(PyExc_TypeError, "cannot compare sets using cmp()");
return -1;
}
static PyObject *
set_add(PySetObject *so, PyObject *key) {
if (set_add_key(so, key) == -1)
return NULL;
Py_RETURN_NONE;
}
PyDoc_STRVAR(add_doc,
"Add an element to a set.\n\
\n\
This has no effect if the element is already present.");
static int
set_contains(PySetObject *so, PyObject *key) {
PyObject *tmpkey;
int rv;
rv = set_contains_key(so, key);
if (rv == -1) {
if (!PySet_Check(key) || !PyErr_ExceptionMatches(PyExc_TypeError))
return -1;
PyErr_Clear();
tmpkey = make_new_set(&PyFrozenSet_Type, NULL);
if (tmpkey == NULL)
return -1;
set_swap_bodies((PySetObject *)tmpkey, (PySetObject *)key);
rv = set_contains(so, tmpkey);
set_swap_bodies((PySetObject *)tmpkey, (PySetObject *)key);
Py_DECREF(tmpkey);
}
return rv;
}
static PyObject *
set_direct_contains(PySetObject *so, PyObject *key) {
long result;
result = set_contains(so, key);
if (result == -1)
return NULL;
return PyBool_FromLong(result);
}
PyDoc_STRVAR(contains_doc, "x.__contains__(y) <==> y in x.");
static PyObject *
set_remove(PySetObject *so, PyObject *key) {
PyObject *tmpkey;
int rv;
rv = set_discard_key(so, key);
if (rv == -1) {
if (!PySet_Check(key) || !PyErr_ExceptionMatches(PyExc_TypeError))
return NULL;
PyErr_Clear();
tmpkey = make_new_set(&PyFrozenSet_Type, NULL);
if (tmpkey == NULL)
return NULL;
set_swap_bodies((PySetObject *)tmpkey, (PySetObject *)key);
rv = set_discard_key(so, tmpkey);
set_swap_bodies((PySetObject *)tmpkey, (PySetObject *)key);
Py_DECREF(tmpkey);
if (rv == -1)
return NULL;
}
if (rv == DISCARD_NOTFOUND) {
set_key_error(key);
return NULL;
}
Py_RETURN_NONE;
}
PyDoc_STRVAR(remove_doc,
"Remove an element from a set; it must be a member.\n\
\n\
If the element is not a member, raise a KeyError.");
static PyObject *
set_discard(PySetObject *so, PyObject *key) {
PyObject *tmpkey, *result;
int rv;
rv = set_discard_key(so, key);
if (rv == -1) {
if (!PySet_Check(key) || !PyErr_ExceptionMatches(PyExc_TypeError))
return NULL;
PyErr_Clear();
tmpkey = make_new_set(&PyFrozenSet_Type, NULL);
if (tmpkey == NULL)
return NULL;
set_swap_bodies((PySetObject *)tmpkey, (PySetObject *)key);
result = set_discard(so, tmpkey);
set_swap_bodies((PySetObject *)tmpkey, (PySetObject *)key);
Py_DECREF(tmpkey);
return result;
}
Py_RETURN_NONE;
}
PyDoc_STRVAR(discard_doc,
"Remove an element from a set if it is a member.\n\
\n\
If the element is not a member, do nothing.");
static PyObject *
set_reduce(PySetObject *so) {
PyObject *keys=NULL, *args=NULL, *result=NULL, *dict=NULL;
keys = PySequence_List((PyObject *)so);
if (keys == NULL)
goto done;
args = PyTuple_Pack(1, keys);
if (args == NULL)
goto done;
dict = PyObject_GetAttrString((PyObject *)so, "__dict__");
if (dict == NULL) {
PyErr_Clear();
dict = Py_None;
Py_INCREF(dict);
}
result = PyTuple_Pack(3, Py_TYPE(so), args, dict);
done:
Py_XDECREF(args);
Py_XDECREF(keys);
Py_XDECREF(dict);
return result;
}
PyDoc_STRVAR(reduce_doc, "Return state information for pickling.");
static PyObject *
set_sizeof(PySetObject *so) {
Py_ssize_t res;
res = sizeof(PySetObject);
if (so->table != so->smalltable)
res = res + (so->mask + 1) * sizeof(setentry);
return PyInt_FromSsize_t(res);
}
PyDoc_STRVAR(sizeof_doc, "S.__sizeof__() -> size of S in memory, in bytes");
static int
set_init(PySetObject *self, PyObject *args, PyObject *kwds) {
PyObject *iterable = NULL;
if (!PyAnySet_Check(self))
return -1;
if (!PyArg_UnpackTuple(args, Py_TYPE(self)->tp_name, 0, 1, &iterable))
return -1;
set_clear_internal(self);
self->hash = -1;
if (iterable == NULL)
return 0;
return set_update_internal(self, iterable);
}
static PySequenceMethods set_as_sequence = {
set_len,
0,
0,
0,
0,
0,
0,
(objobjproc)set_contains,
};
#if defined(Py_DEBUG)
static PyObject *test_c_api(PySetObject *so);
PyDoc_STRVAR(test_c_api_doc, "Exercises C API. Returns True.\n\
All is well if assertions don't fail.");
#endif
static PyMethodDef set_methods[] = {
{
"add", (PyCFunction)set_add, METH_O,
add_doc
},
{
"clear", (PyCFunction)set_clear, METH_NOARGS,
clear_doc
},
{
"__contains__",(PyCFunction)set_direct_contains, METH_O | METH_COEXIST,
contains_doc
},
{
"copy", (PyCFunction)set_copy, METH_NOARGS,
copy_doc
},
{
"discard", (PyCFunction)set_discard, METH_O,
discard_doc
},
{
"difference", (PyCFunction)set_difference_multi, METH_VARARGS,
difference_doc
},
{
"difference_update", (PyCFunction)set_difference_update, METH_VARARGS,
difference_update_doc
},
{
"intersection",(PyCFunction)set_intersection_multi, METH_VARARGS,
intersection_doc
},
{
"intersection_update",(PyCFunction)set_intersection_update_multi, METH_VARARGS,
intersection_update_doc
},
{
"isdisjoint", (PyCFunction)set_isdisjoint, METH_O,
isdisjoint_doc
},
{
"issubset", (PyCFunction)set_issubset, METH_O,
issubset_doc
},
{
"issuperset", (PyCFunction)set_issuperset, METH_O,
issuperset_doc
},
{
"pop", (PyCFunction)set_pop, METH_NOARGS,
pop_doc
},
{
"__reduce__", (PyCFunction)set_reduce, METH_NOARGS,
reduce_doc
},
{
"remove", (PyCFunction)set_remove, METH_O,
remove_doc
},
{
"__sizeof__", (PyCFunction)set_sizeof, METH_NOARGS,
sizeof_doc
},
{
"symmetric_difference",(PyCFunction)set_symmetric_difference, METH_O,
symmetric_difference_doc
},
{
"symmetric_difference_update",(PyCFunction)set_symmetric_difference_update, METH_O,
symmetric_difference_update_doc
},
#if defined(Py_DEBUG)
{
"test_c_api", (PyCFunction)test_c_api, METH_NOARGS,
test_c_api_doc
},
#endif
{
"union", (PyCFunction)set_union, METH_VARARGS,
union_doc
},
{
"update", (PyCFunction)set_update, METH_VARARGS,
update_doc
},
{NULL, NULL}
};
static PyNumberMethods set_as_number = {
0,
(binaryfunc)set_sub,
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
(binaryfunc)set_and,
(binaryfunc)set_xor,
(binaryfunc)set_or,
0,
0,
0,
0,
0,
0,
0,
(binaryfunc)set_isub,
0,
0,
0,
0,
0,
0,
(binaryfunc)set_iand,
(binaryfunc)set_ixor,
(binaryfunc)set_ior,
};
PyDoc_STRVAR(set_doc,
"set(iterable) --> set object\n\
\n\
Build an unordered collection of unique elements.");
PyTypeObject PySet_Type = {
PyVarObject_HEAD_INIT(&PyType_Type, 0)
"set",
sizeof(PySetObject),
0,
(destructor)set_dealloc,
(printfunc)set_tp_print,
0,
0,
set_nocmp,
(reprfunc)set_repr,
&set_as_number,
&set_as_sequence,
0,
(hashfunc)PyObject_HashNotImplemented,
0,
0,
PyObject_GenericGetAttr,
0,
0,
Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC | Py_TPFLAGS_CHECKTYPES |
Py_TPFLAGS_BASETYPE,
set_doc,
(traverseproc)set_traverse,
(inquiry)set_clear_internal,
(richcmpfunc)set_richcompare,
offsetof(PySetObject, weakreflist),
(getiterfunc)set_iter,
0,
set_methods,
0,
0,
0,
0,
0,
0,
0,
(initproc)set_init,
PyType_GenericAlloc,
set_new,
PyObject_GC_Del,
};
static PyMethodDef frozenset_methods[] = {
{
"__contains__",(PyCFunction)set_direct_contains, METH_O | METH_COEXIST,
contains_doc
},
{
"copy", (PyCFunction)frozenset_copy, METH_NOARGS,
copy_doc
},
{
"difference", (PyCFunction)set_difference_multi, METH_VARARGS,
difference_doc
},
{
"intersection",(PyCFunction)set_intersection_multi, METH_VARARGS,
intersection_doc
},
{
"isdisjoint", (PyCFunction)set_isdisjoint, METH_O,
isdisjoint_doc
},
{
"issubset", (PyCFunction)set_issubset, METH_O,
issubset_doc
},
{
"issuperset", (PyCFunction)set_issuperset, METH_O,
issuperset_doc
},
{
"__reduce__", (PyCFunction)set_reduce, METH_NOARGS,
reduce_doc
},
{
"__sizeof__", (PyCFunction)set_sizeof, METH_NOARGS,
sizeof_doc
},
{
"symmetric_difference",(PyCFunction)set_symmetric_difference, METH_O,
symmetric_difference_doc
},
{
"union", (PyCFunction)set_union, METH_VARARGS,
union_doc
},
{NULL, NULL}
};
static PyNumberMethods frozenset_as_number = {
0,
(binaryfunc)set_sub,
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
(binaryfunc)set_and,
(binaryfunc)set_xor,
(binaryfunc)set_or,
};
PyDoc_STRVAR(frozenset_doc,
"frozenset(iterable) --> frozenset object\n\
\n\
Build an immutable unordered collection of unique elements.");
PyTypeObject PyFrozenSet_Type = {
PyVarObject_HEAD_INIT(&PyType_Type, 0)
"frozenset",
sizeof(PySetObject),
0,
(destructor)set_dealloc,
(printfunc)set_tp_print,
0,
0,
set_nocmp,
(reprfunc)set_repr,
&frozenset_as_number,
&set_as_sequence,
0,
frozenset_hash,
0,
0,
PyObject_GenericGetAttr,
0,
0,
Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC | Py_TPFLAGS_CHECKTYPES |
Py_TPFLAGS_BASETYPE,
frozenset_doc,
(traverseproc)set_traverse,
(inquiry)set_clear_internal,
(richcmpfunc)set_richcompare,
offsetof(PySetObject, weakreflist),
(getiterfunc)set_iter,
0,
frozenset_methods,
0,
0,
0,
0,
0,
0,
0,
0,
PyType_GenericAlloc,
frozenset_new,
PyObject_GC_Del,
};
PyObject *
PySet_New(PyObject *iterable) {
return make_new_set(&PySet_Type, iterable);
}
PyObject *
PyFrozenSet_New(PyObject *iterable) {
return make_new_set(&PyFrozenSet_Type, iterable);
}
Py_ssize_t
PySet_Size(PyObject *anyset) {
if (!PyAnySet_Check(anyset)) {
PyErr_BadInternalCall();
return -1;
}
return PySet_GET_SIZE(anyset);
}
int
PySet_Clear(PyObject *set) {
if (!PySet_Check(set)) {
PyErr_BadInternalCall();
return -1;
}
return set_clear_internal((PySetObject *)set);
}
int
PySet_Contains(PyObject *anyset, PyObject *key) {
if (!PyAnySet_Check(anyset)) {
PyErr_BadInternalCall();
return -1;
}
return set_contains_key((PySetObject *)anyset, key);
}
int
PySet_Discard(PyObject *set, PyObject *key) {
if (!PySet_Check(set)) {
PyErr_BadInternalCall();
return -1;
}
return set_discard_key((PySetObject *)set, key);
}
int
PySet_Add(PyObject *anyset, PyObject *key) {
if (!PySet_Check(anyset) &&
(!PyFrozenSet_Check(anyset) || Py_REFCNT(anyset) != 1)) {
PyErr_BadInternalCall();
return -1;
}
return set_add_key((PySetObject *)anyset, key);
}
int
_PySet_Next(PyObject *set, Py_ssize_t *pos, PyObject **key) {
setentry *entry_ptr;
if (!PyAnySet_Check(set)) {
PyErr_BadInternalCall();
return -1;
}
if (set_next((PySetObject *)set, pos, &entry_ptr) == 0)
return 0;
*key = entry_ptr->key;
return 1;
}
int
_PySet_NextEntry(PyObject *set, Py_ssize_t *pos, PyObject **key, long *hash) {
setentry *entry;
if (!PyAnySet_Check(set)) {
PyErr_BadInternalCall();
return -1;
}
if (set_next((PySetObject *)set, pos, &entry) == 0)
return 0;
*key = entry->key;
*hash = entry->hash;
return 1;
}
PyObject *
PySet_Pop(PyObject *set) {
if (!PySet_Check(set)) {
PyErr_BadInternalCall();
return NULL;
}
return set_pop((PySetObject *)set);
}
int
_PySet_Update(PyObject *set, PyObject *iterable) {
if (!PySet_Check(set)) {
PyErr_BadInternalCall();
return -1;
}
return set_update_internal((PySetObject *)set, iterable);
}
#if defined(Py_DEBUG)
#define assertRaises(call_return_value, exception) do { assert(call_return_value); assert(PyErr_ExceptionMatches(exception)); PyErr_Clear(); } while(0)
static PyObject *
test_c_api(PySetObject *so) {
Py_ssize_t count;
char *s;
Py_ssize_t i;
PyObject *elem=NULL, *dup=NULL, *t, *f, *dup2, *x;
PyObject *ob = (PyObject *)so;
assert(PyAnySet_Check(ob));
assert(PyAnySet_CheckExact(ob));
assert(!PyFrozenSet_CheckExact(ob));
assert(PySet_Size(ob) == 3);
assert(PySet_GET_SIZE(ob) == 3);
assertRaises(PySet_New(Py_None) == NULL, PyExc_TypeError);
assertRaises(PyFrozenSet_New(Py_None) == NULL, PyExc_TypeError);
dup = PySet_New(ob);
assertRaises(PySet_Discard(ob, dup) == -1, PyExc_TypeError);
assertRaises(PySet_Contains(ob, dup) == -1, PyExc_TypeError);
assertRaises(PySet_Add(ob, dup) == -1, PyExc_TypeError);
elem = PySet_Pop(ob);
assert(PySet_Contains(ob, elem) == 0);
assert(PySet_GET_SIZE(ob) == 2);
assert(PySet_Add(ob, elem) == 0);
assert(PySet_Contains(ob, elem) == 1);
assert(PySet_GET_SIZE(ob) == 3);
assert(PySet_Discard(ob, elem) == 1);
assert(PySet_GET_SIZE(ob) == 2);
assert(PySet_Discard(ob, elem) == 0);
assert(PySet_GET_SIZE(ob) == 2);
dup2 = PySet_New(dup);
assert(PySet_Clear(dup2) == 0);
assert(PySet_Size(dup2) == 0);
Py_DECREF(dup2);
f = PyFrozenSet_New(dup);
assertRaises(PySet_Clear(f) == -1, PyExc_SystemError);
assertRaises(_PySet_Update(f, dup) == -1, PyExc_SystemError);
assert(PySet_Add(f, elem) == 0);
Py_INCREF(f);
assertRaises(PySet_Add(f, elem) == -1, PyExc_SystemError);
Py_DECREF(f);
Py_DECREF(f);
i = 0, count = 0;
while (_PySet_Next((PyObject *)dup, &i, &x)) {
s = PyString_AsString(x);
assert(s && (s[0] == 'a' || s[0] == 'b' || s[0] == 'c'));
count++;
}
assert(count == 3);
dup2 = PySet_New(NULL);
assert(_PySet_Update(dup2, dup) == 0);
assert(PySet_Size(dup2) == 3);
assert(_PySet_Update(dup2, dup) == 0);
assert(PySet_Size(dup2) == 3);
Py_DECREF(dup2);
t = PyTuple_New(0);
assertRaises(PySet_Size(t) == -1, PyExc_SystemError);
assertRaises(PySet_Contains(t, elem) == -1, PyExc_SystemError);
Py_DECREF(t);
f = PyFrozenSet_New(dup);
assert(PySet_Size(f) == 3);
assert(PyFrozenSet_CheckExact(f));
assertRaises(PySet_Discard(f, elem) == -1, PyExc_SystemError);
assertRaises(PySet_Pop(f) == NULL, PyExc_SystemError);
Py_DECREF(f);
assert(PyNumber_InPlaceSubtract(ob, ob) == ob);
Py_DECREF(ob);
assert(PySet_GET_SIZE(ob) == 0);
assertRaises(PySet_Pop(ob) == NULL, PyExc_KeyError);
assert(PyNumber_InPlaceOr(ob, dup) == ob);
Py_DECREF(ob);
f = PySet_New(NULL);
assert(f != NULL);
assert(PySet_GET_SIZE(f) == 0);
Py_DECREF(f);
f = PyFrozenSet_New(NULL);
assert(f != NULL);
assert(PyFrozenSet_CheckExact(f));
assert(PySet_GET_SIZE(f) == 0);
Py_DECREF(f);
Py_DECREF(elem);
Py_DECREF(dup);
Py_RETURN_TRUE;
}
#undef assertRaises
#endif