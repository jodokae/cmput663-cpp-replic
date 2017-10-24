#include "Python.h"
static void
set_key_error(PyObject *arg) {
PyObject *tup;
tup = PyTuple_Pack(1, arg);
if (!tup)
return;
PyErr_SetObject(PyExc_KeyError, tup);
Py_DECREF(tup);
}
#undef SHOW_CONVERSION_COUNTS
#define PERTURB_SHIFT 5
static PyObject *dummy = NULL;
#if defined(Py_REF_DEBUG)
PyObject *
_PyDict_Dummy(void) {
return dummy;
}
#endif
static PyDictEntry *
lookdict_string(PyDictObject *mp, PyObject *key, long hash);
#if defined(SHOW_CONVERSION_COUNTS)
static long created = 0L;
static long converted = 0L;
static void
show_counts(void) {
fprintf(stderr, "created %ld string dicts\n", created);
fprintf(stderr, "converted %ld to normal dicts\n", converted);
fprintf(stderr, "%.2f%% conversion rate\n", (100.0*converted)/created);
}
#endif
#undef SHOW_ALLOC_COUNT
#if defined(SHOW_ALLOC_COUNT)
static size_t count_alloc = 0;
static size_t count_reuse = 0;
static void
show_alloc(void) {
fprintf(stderr, "Dict allocations: %" PY_FORMAT_SIZE_T "d\n",
count_alloc);
fprintf(stderr, "Dict reuse through freelist: %" PY_FORMAT_SIZE_T
"d\n", count_reuse);
fprintf(stderr, "%.2f%% reuse rate\n\n",
(100.0*count_reuse/(count_alloc+count_reuse)));
}
#endif
#define INIT_NONZERO_DICT_SLOTS(mp) do { (mp)->ma_table = (mp)->ma_smalltable; (mp)->ma_mask = PyDict_MINSIZE - 1; } while(0)
#define EMPTY_TO_MINSIZE(mp) do { memset((mp)->ma_smalltable, 0, sizeof((mp)->ma_smalltable)); (mp)->ma_used = (mp)->ma_fill = 0; INIT_NONZERO_DICT_SLOTS(mp); } while(0)
#if !defined(PyDict_MAXFREELIST)
#define PyDict_MAXFREELIST 80
#endif
static PyDictObject *free_list[PyDict_MAXFREELIST];
static int numfree = 0;
void
PyDict_Fini(void) {
PyDictObject *op;
while (numfree) {
op = free_list[--numfree];
assert(PyDict_CheckExact(op));
PyObject_GC_Del(op);
}
}
PyObject *
PyDict_New(void) {
register PyDictObject *mp;
if (dummy == NULL) {
dummy = PyString_FromString("<dummy key>");
if (dummy == NULL)
return NULL;
#if defined(SHOW_CONVERSION_COUNTS)
Py_AtExit(show_counts);
#endif
#if defined(SHOW_ALLOC_COUNT)
Py_AtExit(show_alloc);
#endif
}
if (numfree) {
mp = free_list[--numfree];
assert (mp != NULL);
assert (Py_TYPE(mp) == &PyDict_Type);
_Py_NewReference((PyObject *)mp);
if (mp->ma_fill) {
EMPTY_TO_MINSIZE(mp);
} else {
INIT_NONZERO_DICT_SLOTS(mp);
}
assert (mp->ma_used == 0);
assert (mp->ma_table == mp->ma_smalltable);
assert (mp->ma_mask == PyDict_MINSIZE - 1);
#if defined(SHOW_ALLOC_COUNT)
count_reuse++;
#endif
} else {
mp = PyObject_GC_New(PyDictObject, &PyDict_Type);
if (mp == NULL)
return NULL;
EMPTY_TO_MINSIZE(mp);
#if defined(SHOW_ALLOC_COUNT)
count_alloc++;
#endif
}
mp->ma_lookup = lookdict_string;
#if defined(SHOW_CONVERSION_COUNTS)
++created;
#endif
_PyObject_GC_TRACK(mp);
return (PyObject *)mp;
}
static PyDictEntry *
lookdict(PyDictObject *mp, PyObject *key, register long hash) {
register size_t i;
register size_t perturb;
register PyDictEntry *freeslot;
register size_t mask = (size_t)mp->ma_mask;
PyDictEntry *ep0 = mp->ma_table;
register PyDictEntry *ep;
register int cmp;
PyObject *startkey;
i = (size_t)hash & mask;
ep = &ep0[i];
if (ep->me_key == NULL || ep->me_key == key)
return ep;
if (ep->me_key == dummy)
freeslot = ep;
else {
if (ep->me_hash == hash) {
startkey = ep->me_key;
Py_INCREF(startkey);
cmp = PyObject_RichCompareBool(startkey, key, Py_EQ);
Py_DECREF(startkey);
if (cmp < 0)
return NULL;
if (ep0 == mp->ma_table && ep->me_key == startkey) {
if (cmp > 0)
return ep;
} else {
return lookdict(mp, key, hash);
}
}
freeslot = NULL;
}
for (perturb = hash; ; perturb >>= PERTURB_SHIFT) {
i = (i << 2) + i + perturb + 1;
ep = &ep0[i & mask];
if (ep->me_key == NULL)
return freeslot == NULL ? ep : freeslot;
if (ep->me_key == key)
return ep;
if (ep->me_hash == hash && ep->me_key != dummy) {
startkey = ep->me_key;
Py_INCREF(startkey);
cmp = PyObject_RichCompareBool(startkey, key, Py_EQ);
Py_DECREF(startkey);
if (cmp < 0)
return NULL;
if (ep0 == mp->ma_table && ep->me_key == startkey) {
if (cmp > 0)
return ep;
} else {
return lookdict(mp, key, hash);
}
} else if (ep->me_key == dummy && freeslot == NULL)
freeslot = ep;
}
assert(0);
return 0;
}
static PyDictEntry *
lookdict_string(PyDictObject *mp, PyObject *key, register long hash) {
register size_t i;
register size_t perturb;
register PyDictEntry *freeslot;
register size_t mask = (size_t)mp->ma_mask;
PyDictEntry *ep0 = mp->ma_table;
register PyDictEntry *ep;
if (!PyString_CheckExact(key)) {
#if defined(SHOW_CONVERSION_COUNTS)
++converted;
#endif
mp->ma_lookup = lookdict;
return lookdict(mp, key, hash);
}
i = hash & mask;
ep = &ep0[i];
if (ep->me_key == NULL || ep->me_key == key)
return ep;
if (ep->me_key == dummy)
freeslot = ep;
else {
if (ep->me_hash == hash && _PyString_Eq(ep->me_key, key))
return ep;
freeslot = NULL;
}
for (perturb = hash; ; perturb >>= PERTURB_SHIFT) {
i = (i << 2) + i + perturb + 1;
ep = &ep0[i & mask];
if (ep->me_key == NULL)
return freeslot == NULL ? ep : freeslot;
if (ep->me_key == key
|| (ep->me_hash == hash
&& ep->me_key != dummy
&& _PyString_Eq(ep->me_key, key)))
return ep;
if (ep->me_key == dummy && freeslot == NULL)
freeslot = ep;
}
assert(0);
return 0;
}
static int
insertdict(register PyDictObject *mp, PyObject *key, long hash, PyObject *value) {
PyObject *old_value;
register PyDictEntry *ep;
typedef PyDictEntry *(*lookupfunc)(PyDictObject *, PyObject *, long);
assert(mp->ma_lookup != NULL);
ep = mp->ma_lookup(mp, key, hash);
if (ep == NULL) {
Py_DECREF(key);
Py_DECREF(value);
return -1;
}
if (ep->me_value != NULL) {
old_value = ep->me_value;
ep->me_value = value;
Py_DECREF(old_value);
Py_DECREF(key);
} else {
if (ep->me_key == NULL)
mp->ma_fill++;
else {
assert(ep->me_key == dummy);
Py_DECREF(dummy);
}
ep->me_key = key;
ep->me_hash = (Py_ssize_t)hash;
ep->me_value = value;
mp->ma_used++;
}
return 0;
}
static void
insertdict_clean(register PyDictObject *mp, PyObject *key, long hash,
PyObject *value) {
register size_t i;
register size_t perturb;
register size_t mask = (size_t)mp->ma_mask;
PyDictEntry *ep0 = mp->ma_table;
register PyDictEntry *ep;
i = hash & mask;
ep = &ep0[i];
for (perturb = hash; ep->me_key != NULL; perturb >>= PERTURB_SHIFT) {
i = (i << 2) + i + perturb + 1;
ep = &ep0[i & mask];
}
assert(ep->me_value == NULL);
mp->ma_fill++;
ep->me_key = key;
ep->me_hash = (Py_ssize_t)hash;
ep->me_value = value;
mp->ma_used++;
}
static int
dictresize(PyDictObject *mp, Py_ssize_t minused) {
Py_ssize_t newsize;
PyDictEntry *oldtable, *newtable, *ep;
Py_ssize_t i;
int is_oldtable_malloced;
PyDictEntry small_copy[PyDict_MINSIZE];
assert(minused >= 0);
for (newsize = PyDict_MINSIZE;
newsize <= minused && newsize > 0;
newsize <<= 1)
;
if (newsize <= 0) {
PyErr_NoMemory();
return -1;
}
oldtable = mp->ma_table;
assert(oldtable != NULL);
is_oldtable_malloced = oldtable != mp->ma_smalltable;
if (newsize == PyDict_MINSIZE) {
newtable = mp->ma_smalltable;
if (newtable == oldtable) {
if (mp->ma_fill == mp->ma_used) {
return 0;
}
assert(mp->ma_fill > mp->ma_used);
memcpy(small_copy, oldtable, sizeof(small_copy));
oldtable = small_copy;
}
} else {
newtable = PyMem_NEW(PyDictEntry, newsize);
if (newtable == NULL) {
PyErr_NoMemory();
return -1;
}
}
assert(newtable != oldtable);
mp->ma_table = newtable;
mp->ma_mask = newsize - 1;
memset(newtable, 0, sizeof(PyDictEntry) * newsize);
mp->ma_used = 0;
i = mp->ma_fill;
mp->ma_fill = 0;
for (ep = oldtable; i > 0; ep++) {
if (ep->me_value != NULL) {
--i;
insertdict_clean(mp, ep->me_key, (long)ep->me_hash,
ep->me_value);
} else if (ep->me_key != NULL) {
--i;
assert(ep->me_key == dummy);
Py_DECREF(ep->me_key);
}
}
if (is_oldtable_malloced)
PyMem_DEL(oldtable);
return 0;
}
PyObject *
_PyDict_NewPresized(Py_ssize_t minused) {
PyObject *op = PyDict_New();
if (minused>5 && op != NULL && dictresize((PyDictObject *)op, minused) == -1) {
Py_DECREF(op);
return NULL;
}
return op;
}
PyObject *
PyDict_GetItem(PyObject *op, PyObject *key) {
long hash;
PyDictObject *mp = (PyDictObject *)op;
PyDictEntry *ep;
PyThreadState *tstate;
if (!PyDict_Check(op))
return NULL;
if (!PyString_CheckExact(key) ||
(hash = ((PyStringObject *) key)->ob_shash) == -1) {
hash = PyObject_Hash(key);
if (hash == -1) {
PyErr_Clear();
return NULL;
}
}
tstate = _PyThreadState_Current;
if (tstate != NULL && tstate->curexc_type != NULL) {
PyObject *err_type, *err_value, *err_tb;
PyErr_Fetch(&err_type, &err_value, &err_tb);
ep = (mp->ma_lookup)(mp, key, hash);
PyErr_Restore(err_type, err_value, err_tb);
if (ep == NULL)
return NULL;
} else {
ep = (mp->ma_lookup)(mp, key, hash);
if (ep == NULL) {
PyErr_Clear();
return NULL;
}
}
return ep->me_value;
}
int
PyDict_SetItem(register PyObject *op, PyObject *key, PyObject *value) {
register PyDictObject *mp;
register long hash;
register Py_ssize_t n_used;
if (!PyDict_Check(op)) {
PyErr_BadInternalCall();
return -1;
}
assert(key);
assert(value);
mp = (PyDictObject *)op;
if (PyString_CheckExact(key)) {
hash = ((PyStringObject *)key)->ob_shash;
if (hash == -1)
hash = PyObject_Hash(key);
} else {
hash = PyObject_Hash(key);
if (hash == -1)
return -1;
}
assert(mp->ma_fill <= mp->ma_mask);
n_used = mp->ma_used;
Py_INCREF(value);
Py_INCREF(key);
if (insertdict(mp, key, hash, value) != 0)
return -1;
if (!(mp->ma_used > n_used && mp->ma_fill*3 >= (mp->ma_mask+1)*2))
return 0;
return dictresize(mp, (mp->ma_used > 50000 ? 2 : 4) * mp->ma_used);
}
int
PyDict_DelItem(PyObject *op, PyObject *key) {
register PyDictObject *mp;
register long hash;
register PyDictEntry *ep;
PyObject *old_value, *old_key;
if (!PyDict_Check(op)) {
PyErr_BadInternalCall();
return -1;
}
assert(key);
if (!PyString_CheckExact(key) ||
(hash = ((PyStringObject *) key)->ob_shash) == -1) {
hash = PyObject_Hash(key);
if (hash == -1)
return -1;
}
mp = (PyDictObject *)op;
ep = (mp->ma_lookup)(mp, key, hash);
if (ep == NULL)
return -1;
if (ep->me_value == NULL) {
set_key_error(key);
return -1;
}
old_key = ep->me_key;
Py_INCREF(dummy);
ep->me_key = dummy;
old_value = ep->me_value;
ep->me_value = NULL;
mp->ma_used--;
Py_DECREF(old_value);
Py_DECREF(old_key);
return 0;
}
void
PyDict_Clear(PyObject *op) {
PyDictObject *mp;
PyDictEntry *ep, *table;
int table_is_malloced;
Py_ssize_t fill;
PyDictEntry small_copy[PyDict_MINSIZE];
#if defined(Py_DEBUG)
Py_ssize_t i, n;
#endif
if (!PyDict_Check(op))
return;
mp = (PyDictObject *)op;
#if defined(Py_DEBUG)
n = mp->ma_mask + 1;
i = 0;
#endif
table = mp->ma_table;
assert(table != NULL);
table_is_malloced = table != mp->ma_smalltable;
fill = mp->ma_fill;
if (table_is_malloced)
EMPTY_TO_MINSIZE(mp);
else if (fill > 0) {
memcpy(small_copy, table, sizeof(small_copy));
table = small_copy;
EMPTY_TO_MINSIZE(mp);
}
for (ep = table; fill > 0; ++ep) {
#if defined(Py_DEBUG)
assert(i < n);
++i;
#endif
if (ep->me_key) {
--fill;
Py_DECREF(ep->me_key);
Py_XDECREF(ep->me_value);
}
#if defined(Py_DEBUG)
else
assert(ep->me_value == NULL);
#endif
}
if (table_is_malloced)
PyMem_DEL(table);
}
int
PyDict_Next(PyObject *op, Py_ssize_t *ppos, PyObject **pkey, PyObject **pvalue) {
register Py_ssize_t i;
register Py_ssize_t mask;
register PyDictEntry *ep;
if (!PyDict_Check(op))
return 0;
i = *ppos;
if (i < 0)
return 0;
ep = ((PyDictObject *)op)->ma_table;
mask = ((PyDictObject *)op)->ma_mask;
while (i <= mask && ep[i].me_value == NULL)
i++;
*ppos = i+1;
if (i > mask)
return 0;
if (pkey)
*pkey = ep[i].me_key;
if (pvalue)
*pvalue = ep[i].me_value;
return 1;
}
int
_PyDict_Next(PyObject *op, Py_ssize_t *ppos, PyObject **pkey, PyObject **pvalue, long *phash) {
register Py_ssize_t i;
register Py_ssize_t mask;
register PyDictEntry *ep;
if (!PyDict_Check(op))
return 0;
i = *ppos;
if (i < 0)
return 0;
ep = ((PyDictObject *)op)->ma_table;
mask = ((PyDictObject *)op)->ma_mask;
while (i <= mask && ep[i].me_value == NULL)
i++;
*ppos = i+1;
if (i > mask)
return 0;
*phash = (long)(ep[i].me_hash);
if (pkey)
*pkey = ep[i].me_key;
if (pvalue)
*pvalue = ep[i].me_value;
return 1;
}
static void
dict_dealloc(register PyDictObject *mp) {
register PyDictEntry *ep;
Py_ssize_t fill = mp->ma_fill;
PyObject_GC_UnTrack(mp);
Py_TRASHCAN_SAFE_BEGIN(mp)
for (ep = mp->ma_table; fill > 0; ep++) {
if (ep->me_key) {
--fill;
Py_DECREF(ep->me_key);
Py_XDECREF(ep->me_value);
}
}
if (mp->ma_table != mp->ma_smalltable)
PyMem_DEL(mp->ma_table);
if (numfree < PyDict_MAXFREELIST && Py_TYPE(mp) == &PyDict_Type)
free_list[numfree++] = mp;
else
Py_TYPE(mp)->tp_free((PyObject *)mp);
Py_TRASHCAN_SAFE_END(mp)
}
static int
dict_print(register PyDictObject *mp, register FILE *fp, register int flags) {
register Py_ssize_t i;
register Py_ssize_t any;
int status;
status = Py_ReprEnter((PyObject*)mp);
if (status != 0) {
if (status < 0)
return status;
Py_BEGIN_ALLOW_THREADS
fprintf(fp, "{...}");
Py_END_ALLOW_THREADS
return 0;
}
Py_BEGIN_ALLOW_THREADS
fprintf(fp, "{");
Py_END_ALLOW_THREADS
any = 0;
for (i = 0; i <= mp->ma_mask; i++) {
PyDictEntry *ep = mp->ma_table + i;
PyObject *pvalue = ep->me_value;
if (pvalue != NULL) {
Py_INCREF(pvalue);
if (any++ > 0) {
Py_BEGIN_ALLOW_THREADS
fprintf(fp, ", ");
Py_END_ALLOW_THREADS
}
if (PyObject_Print((PyObject *)ep->me_key, fp, 0)!=0) {
Py_DECREF(pvalue);
Py_ReprLeave((PyObject*)mp);
return -1;
}
Py_BEGIN_ALLOW_THREADS
fprintf(fp, ": ");
Py_END_ALLOW_THREADS
if (PyObject_Print(pvalue, fp, 0) != 0) {
Py_DECREF(pvalue);
Py_ReprLeave((PyObject*)mp);
return -1;
}
Py_DECREF(pvalue);
}
}
Py_BEGIN_ALLOW_THREADS
fprintf(fp, "}");
Py_END_ALLOW_THREADS
Py_ReprLeave((PyObject*)mp);
return 0;
}
static PyObject *
dict_repr(PyDictObject *mp) {
Py_ssize_t i;
PyObject *s, *temp, *colon = NULL;
PyObject *pieces = NULL, *result = NULL;
PyObject *key, *value;
i = Py_ReprEnter((PyObject *)mp);
if (i != 0) {
return i > 0 ? PyString_FromString("{...}") : NULL;
}
if (mp->ma_used == 0) {
result = PyString_FromString("{}");
goto Done;
}
pieces = PyList_New(0);
if (pieces == NULL)
goto Done;
colon = PyString_FromString(": ");
if (colon == NULL)
goto Done;
i = 0;
while (PyDict_Next((PyObject *)mp, &i, &key, &value)) {
int status;
Py_INCREF(value);
s = PyObject_Repr(key);
PyString_Concat(&s, colon);
PyString_ConcatAndDel(&s, PyObject_Repr(value));
Py_DECREF(value);
if (s == NULL)
goto Done;
status = PyList_Append(pieces, s);
Py_DECREF(s);
if (status < 0)
goto Done;
}
assert(PyList_GET_SIZE(pieces) > 0);
s = PyString_FromString("{");
if (s == NULL)
goto Done;
temp = PyList_GET_ITEM(pieces, 0);
PyString_ConcatAndDel(&s, temp);
PyList_SET_ITEM(pieces, 0, s);
if (s == NULL)
goto Done;
s = PyString_FromString("}");
if (s == NULL)
goto Done;
temp = PyList_GET_ITEM(pieces, PyList_GET_SIZE(pieces) - 1);
PyString_ConcatAndDel(&temp, s);
PyList_SET_ITEM(pieces, PyList_GET_SIZE(pieces) - 1, temp);
if (temp == NULL)
goto Done;
s = PyString_FromString(", ");
if (s == NULL)
goto Done;
result = _PyString_Join(s, pieces);
Py_DECREF(s);
Done:
Py_XDECREF(pieces);
Py_XDECREF(colon);
Py_ReprLeave((PyObject *)mp);
return result;
}
static Py_ssize_t
dict_length(PyDictObject *mp) {
return mp->ma_used;
}
static PyObject *
dict_subscript(PyDictObject *mp, register PyObject *key) {
PyObject *v;
long hash;
PyDictEntry *ep;
assert(mp->ma_table != NULL);
if (!PyString_CheckExact(key) ||
(hash = ((PyStringObject *) key)->ob_shash) == -1) {
hash = PyObject_Hash(key);
if (hash == -1)
return NULL;
}
ep = (mp->ma_lookup)(mp, key, hash);
if (ep == NULL)
return NULL;
v = ep->me_value;
if (v == NULL) {
if (!PyDict_CheckExact(mp)) {
PyObject *missing;
static PyObject *missing_str = NULL;
if (missing_str == NULL)
missing_str =
PyString_InternFromString("__missing__");
missing = _PyType_Lookup(Py_TYPE(mp), missing_str);
if (missing != NULL)
return PyObject_CallFunctionObjArgs(missing,
(PyObject *)mp, key, NULL);
}
set_key_error(key);
return NULL;
} else
Py_INCREF(v);
return v;
}
static int
dict_ass_sub(PyDictObject *mp, PyObject *v, PyObject *w) {
if (w == NULL)
return PyDict_DelItem((PyObject *)mp, v);
else
return PyDict_SetItem((PyObject *)mp, v, w);
}
static PyMappingMethods dict_as_mapping = {
(lenfunc)dict_length,
(binaryfunc)dict_subscript,
(objobjargproc)dict_ass_sub,
};
static PyObject *
dict_keys(register PyDictObject *mp) {
register PyObject *v;
register Py_ssize_t i, j;
PyDictEntry *ep;
Py_ssize_t mask, n;
again:
n = mp->ma_used;
v = PyList_New(n);
if (v == NULL)
return NULL;
if (n != mp->ma_used) {
Py_DECREF(v);
goto again;
}
ep = mp->ma_table;
mask = mp->ma_mask;
for (i = 0, j = 0; i <= mask; i++) {
if (ep[i].me_value != NULL) {
PyObject *key = ep[i].me_key;
Py_INCREF(key);
PyList_SET_ITEM(v, j, key);
j++;
}
}
assert(j == n);
return v;
}
static PyObject *
dict_values(register PyDictObject *mp) {
register PyObject *v;
register Py_ssize_t i, j;
PyDictEntry *ep;
Py_ssize_t mask, n;
again:
n = mp->ma_used;
v = PyList_New(n);
if (v == NULL)
return NULL;
if (n != mp->ma_used) {
Py_DECREF(v);
goto again;
}
ep = mp->ma_table;
mask = mp->ma_mask;
for (i = 0, j = 0; i <= mask; i++) {
if (ep[i].me_value != NULL) {
PyObject *value = ep[i].me_value;
Py_INCREF(value);
PyList_SET_ITEM(v, j, value);
j++;
}
}
assert(j == n);
return v;
}
static PyObject *
dict_items(register PyDictObject *mp) {
register PyObject *v;
register Py_ssize_t i, j, n;
Py_ssize_t mask;
PyObject *item, *key, *value;
PyDictEntry *ep;
again:
n = mp->ma_used;
v = PyList_New(n);
if (v == NULL)
return NULL;
for (i = 0; i < n; i++) {
item = PyTuple_New(2);
if (item == NULL) {
Py_DECREF(v);
return NULL;
}
PyList_SET_ITEM(v, i, item);
}
if (n != mp->ma_used) {
Py_DECREF(v);
goto again;
}
ep = mp->ma_table;
mask = mp->ma_mask;
for (i = 0, j = 0; i <= mask; i++) {
if ((value=ep[i].me_value) != NULL) {
key = ep[i].me_key;
item = PyList_GET_ITEM(v, j);
Py_INCREF(key);
PyTuple_SET_ITEM(item, 0, key);
Py_INCREF(value);
PyTuple_SET_ITEM(item, 1, value);
j++;
}
}
assert(j == n);
return v;
}
static PyObject *
dict_fromkeys(PyObject *cls, PyObject *args) {
PyObject *seq;
PyObject *value = Py_None;
PyObject *it;
PyObject *key;
PyObject *d;
int status;
if (!PyArg_UnpackTuple(args, "fromkeys", 1, 2, &seq, &value))
return NULL;
d = PyObject_CallObject(cls, NULL);
if (d == NULL)
return NULL;
if (PyDict_CheckExact(d) && PyDict_CheckExact(seq)) {
PyDictObject *mp = (PyDictObject *)d;
PyObject *oldvalue;
Py_ssize_t pos = 0;
PyObject *key;
long hash;
if (dictresize(mp, Py_SIZE(seq)))
return NULL;
while (_PyDict_Next(seq, &pos, &key, &oldvalue, &hash)) {
Py_INCREF(key);
Py_INCREF(value);
if (insertdict(mp, key, hash, value))
return NULL;
}
return d;
}
if (PyDict_CheckExact(d) && PyAnySet_CheckExact(seq)) {
PyDictObject *mp = (PyDictObject *)d;
Py_ssize_t pos = 0;
PyObject *key;
long hash;
if (dictresize(mp, PySet_GET_SIZE(seq)))
return NULL;
while (_PySet_NextEntry(seq, &pos, &key, &hash)) {
Py_INCREF(key);
Py_INCREF(value);
if (insertdict(mp, key, hash, value))
return NULL;
}
return d;
}
it = PyObject_GetIter(seq);
if (it == NULL) {
Py_DECREF(d);
return NULL;
}
if (PyDict_CheckExact(d)) {
while ((key = PyIter_Next(it)) != NULL) {
status = PyDict_SetItem(d, key, value);
Py_DECREF(key);
if (status < 0)
goto Fail;
}
} else {
while ((key = PyIter_Next(it)) != NULL) {
status = PyObject_SetItem(d, key, value);
Py_DECREF(key);
if (status < 0)
goto Fail;
}
}
if (PyErr_Occurred())
goto Fail;
Py_DECREF(it);
return d;
Fail:
Py_DECREF(it);
Py_DECREF(d);
return NULL;
}
static int
dict_update_common(PyObject *self, PyObject *args, PyObject *kwds, char *methname) {
PyObject *arg = NULL;
int result = 0;
if (!PyArg_UnpackTuple(args, methname, 0, 1, &arg))
result = -1;
else if (arg != NULL) {
if (PyObject_HasAttrString(arg, "keys"))
result = PyDict_Merge(self, arg, 1);
else
result = PyDict_MergeFromSeq2(self, arg, 1);
}
if (result == 0 && kwds != NULL)
result = PyDict_Merge(self, kwds, 1);
return result;
}
static PyObject *
dict_update(PyObject *self, PyObject *args, PyObject *kwds) {
if (dict_update_common(self, args, kwds, "update") != -1)
Py_RETURN_NONE;
return NULL;
}
int
PyDict_MergeFromSeq2(PyObject *d, PyObject *seq2, int override) {
PyObject *it;
Py_ssize_t i;
PyObject *item;
PyObject *fast;
assert(d != NULL);
assert(PyDict_Check(d));
assert(seq2 != NULL);
it = PyObject_GetIter(seq2);
if (it == NULL)
return -1;
for (i = 0; ; ++i) {
PyObject *key, *value;
Py_ssize_t n;
fast = NULL;
item = PyIter_Next(it);
if (item == NULL) {
if (PyErr_Occurred())
goto Fail;
break;
}
fast = PySequence_Fast(item, "");
if (fast == NULL) {
if (PyErr_ExceptionMatches(PyExc_TypeError))
PyErr_Format(PyExc_TypeError,
"cannot convert dictionary update "
"sequence element #%zd to a sequence",
i);
goto Fail;
}
n = PySequence_Fast_GET_SIZE(fast);
if (n != 2) {
PyErr_Format(PyExc_ValueError,
"dictionary update sequence element #%zd "
"has length %zd; 2 is required",
i, n);
goto Fail;
}
key = PySequence_Fast_GET_ITEM(fast, 0);
value = PySequence_Fast_GET_ITEM(fast, 1);
if (override || PyDict_GetItem(d, key) == NULL) {
int status = PyDict_SetItem(d, key, value);
if (status < 0)
goto Fail;
}
Py_DECREF(fast);
Py_DECREF(item);
}
i = 0;
goto Return;
Fail:
Py_XDECREF(item);
Py_XDECREF(fast);
i = -1;
Return:
Py_DECREF(it);
return Py_SAFE_DOWNCAST(i, Py_ssize_t, int);
}
int
PyDict_Update(PyObject *a, PyObject *b) {
return PyDict_Merge(a, b, 1);
}
int
PyDict_Merge(PyObject *a, PyObject *b, int override) {
register PyDictObject *mp, *other;
register Py_ssize_t i;
PyDictEntry *entry;
if (a == NULL || !PyDict_Check(a) || b == NULL) {
PyErr_BadInternalCall();
return -1;
}
mp = (PyDictObject*)a;
if (PyDict_Check(b)) {
other = (PyDictObject*)b;
if (other == mp || other->ma_used == 0)
return 0;
if (mp->ma_used == 0)
override = 1;
if ((mp->ma_fill + other->ma_used)*3 >= (mp->ma_mask+1)*2) {
if (dictresize(mp, (mp->ma_used + other->ma_used)*2) != 0)
return -1;
}
for (i = 0; i <= other->ma_mask; i++) {
entry = &other->ma_table[i];
if (entry->me_value != NULL &&
(override ||
PyDict_GetItem(a, entry->me_key) == NULL)) {
Py_INCREF(entry->me_key);
Py_INCREF(entry->me_value);
if (insertdict(mp, entry->me_key,
(long)entry->me_hash,
entry->me_value) != 0)
return -1;
}
}
} else {
PyObject *keys = PyMapping_Keys(b);
PyObject *iter;
PyObject *key, *value;
int status;
if (keys == NULL)
return -1;
iter = PyObject_GetIter(keys);
Py_DECREF(keys);
if (iter == NULL)
return -1;
for (key = PyIter_Next(iter); key; key = PyIter_Next(iter)) {
if (!override && PyDict_GetItem(a, key) != NULL) {
Py_DECREF(key);
continue;
}
value = PyObject_GetItem(b, key);
if (value == NULL) {
Py_DECREF(iter);
Py_DECREF(key);
return -1;
}
status = PyDict_SetItem(a, key, value);
Py_DECREF(key);
Py_DECREF(value);
if (status < 0) {
Py_DECREF(iter);
return -1;
}
}
Py_DECREF(iter);
if (PyErr_Occurred())
return -1;
}
return 0;
}
static PyObject *
dict_copy(register PyDictObject *mp) {
return PyDict_Copy((PyObject*)mp);
}
PyObject *
PyDict_Copy(PyObject *o) {
PyObject *copy;
if (o == NULL || !PyDict_Check(o)) {
PyErr_BadInternalCall();
return NULL;
}
copy = PyDict_New();
if (copy == NULL)
return NULL;
if (PyDict_Merge(copy, o, 1) == 0)
return copy;
Py_DECREF(copy);
return NULL;
}
Py_ssize_t
PyDict_Size(PyObject *mp) {
if (mp == NULL || !PyDict_Check(mp)) {
PyErr_BadInternalCall();
return -1;
}
return ((PyDictObject *)mp)->ma_used;
}
PyObject *
PyDict_Keys(PyObject *mp) {
if (mp == NULL || !PyDict_Check(mp)) {
PyErr_BadInternalCall();
return NULL;
}
return dict_keys((PyDictObject *)mp);
}
PyObject *
PyDict_Values(PyObject *mp) {
if (mp == NULL || !PyDict_Check(mp)) {
PyErr_BadInternalCall();
return NULL;
}
return dict_values((PyDictObject *)mp);
}
PyObject *
PyDict_Items(PyObject *mp) {
if (mp == NULL || !PyDict_Check(mp)) {
PyErr_BadInternalCall();
return NULL;
}
return dict_items((PyDictObject *)mp);
}
static PyObject *
characterize(PyDictObject *a, PyDictObject *b, PyObject **pval) {
PyObject *akey = NULL;
PyObject *aval = NULL;
Py_ssize_t i;
int cmp;
for (i = 0; i <= a->ma_mask; i++) {
PyObject *thiskey, *thisaval, *thisbval;
if (a->ma_table[i].me_value == NULL)
continue;
thiskey = a->ma_table[i].me_key;
Py_INCREF(thiskey);
if (akey != NULL) {
cmp = PyObject_RichCompareBool(akey, thiskey, Py_LT);
if (cmp < 0) {
Py_DECREF(thiskey);
goto Fail;
}
if (cmp > 0 ||
i > a->ma_mask ||
a->ma_table[i].me_value == NULL) {
Py_DECREF(thiskey);
continue;
}
}
thisaval = a->ma_table[i].me_value;
assert(thisaval);
Py_INCREF(thisaval);
thisbval = PyDict_GetItem((PyObject *)b, thiskey);
if (thisbval == NULL)
cmp = 0;
else {
cmp = PyObject_RichCompareBool(
thisaval, thisbval, Py_EQ);
if (cmp < 0) {
Py_DECREF(thiskey);
Py_DECREF(thisaval);
goto Fail;
}
}
if (cmp == 0) {
Py_XDECREF(akey);
Py_XDECREF(aval);
akey = thiskey;
aval = thisaval;
} else {
Py_DECREF(thiskey);
Py_DECREF(thisaval);
}
}
*pval = aval;
return akey;
Fail:
Py_XDECREF(akey);
Py_XDECREF(aval);
*pval = NULL;
return NULL;
}
static int
dict_compare(PyDictObject *a, PyDictObject *b) {
PyObject *adiff, *bdiff, *aval, *bval;
int res;
if (a->ma_used < b->ma_used)
return -1;
else if (a->ma_used > b->ma_used)
return 1;
bdiff = bval = NULL;
adiff = characterize(a, b, &aval);
if (adiff == NULL) {
assert(!aval);
res = PyErr_Occurred() ? -1 : 0;
goto Finished;
}
bdiff = characterize(b, a, &bval);
if (bdiff == NULL && PyErr_Occurred()) {
assert(!bval);
res = -1;
goto Finished;
}
res = 0;
if (bdiff) {
res = PyObject_Compare(adiff, bdiff);
}
if (res == 0 && bval != NULL)
res = PyObject_Compare(aval, bval);
Finished:
Py_XDECREF(adiff);
Py_XDECREF(bdiff);
Py_XDECREF(aval);
Py_XDECREF(bval);
return res;
}
static int
dict_equal(PyDictObject *a, PyDictObject *b) {
Py_ssize_t i;
if (a->ma_used != b->ma_used)
return 0;
for (i = 0; i <= a->ma_mask; i++) {
PyObject *aval = a->ma_table[i].me_value;
if (aval != NULL) {
int cmp;
PyObject *bval;
PyObject *key = a->ma_table[i].me_key;
Py_INCREF(aval);
Py_INCREF(key);
bval = PyDict_GetItem((PyObject *)b, key);
Py_DECREF(key);
if (bval == NULL) {
Py_DECREF(aval);
return 0;
}
cmp = PyObject_RichCompareBool(aval, bval, Py_EQ);
Py_DECREF(aval);
if (cmp <= 0)
return cmp;
}
}
return 1;
}
static PyObject *
dict_richcompare(PyObject *v, PyObject *w, int op) {
int cmp;
PyObject *res;
if (!PyDict_Check(v) || !PyDict_Check(w)) {
res = Py_NotImplemented;
} else if (op == Py_EQ || op == Py_NE) {
cmp = dict_equal((PyDictObject *)v, (PyDictObject *)w);
if (cmp < 0)
return NULL;
res = (cmp == (op == Py_EQ)) ? Py_True : Py_False;
} else {
if (PyErr_WarnPy3k("dict inequality comparisons not supported "
"in 3.x", 1) < 0) {
return NULL;
}
res = Py_NotImplemented;
}
Py_INCREF(res);
return res;
}
static PyObject *
dict_contains(register PyDictObject *mp, PyObject *key) {
long hash;
PyDictEntry *ep;
if (!PyString_CheckExact(key) ||
(hash = ((PyStringObject *) key)->ob_shash) == -1) {
hash = PyObject_Hash(key);
if (hash == -1)
return NULL;
}
ep = (mp->ma_lookup)(mp, key, hash);
if (ep == NULL)
return NULL;
return PyBool_FromLong(ep->me_value != NULL);
}
static PyObject *
dict_has_key(register PyDictObject *mp, PyObject *key) {
if (PyErr_WarnPy3k("dict.has_key() not supported in 3.x; "
"use the in operator", 1) < 0)
return NULL;
return dict_contains(mp, key);
}
static PyObject *
dict_get(register PyDictObject *mp, PyObject *args) {
PyObject *key;
PyObject *failobj = Py_None;
PyObject *val = NULL;
long hash;
PyDictEntry *ep;
if (!PyArg_UnpackTuple(args, "get", 1, 2, &key, &failobj))
return NULL;
if (!PyString_CheckExact(key) ||
(hash = ((PyStringObject *) key)->ob_shash) == -1) {
hash = PyObject_Hash(key);
if (hash == -1)
return NULL;
}
ep = (mp->ma_lookup)(mp, key, hash);
if (ep == NULL)
return NULL;
val = ep->me_value;
if (val == NULL)
val = failobj;
Py_INCREF(val);
return val;
}
static PyObject *
dict_setdefault(register PyDictObject *mp, PyObject *args) {
PyObject *key;
PyObject *failobj = Py_None;
PyObject *val = NULL;
long hash;
PyDictEntry *ep;
if (!PyArg_UnpackTuple(args, "setdefault", 1, 2, &key, &failobj))
return NULL;
if (!PyString_CheckExact(key) ||
(hash = ((PyStringObject *) key)->ob_shash) == -1) {
hash = PyObject_Hash(key);
if (hash == -1)
return NULL;
}
ep = (mp->ma_lookup)(mp, key, hash);
if (ep == NULL)
return NULL;
val = ep->me_value;
if (val == NULL) {
val = failobj;
if (PyDict_SetItem((PyObject*)mp, key, failobj))
val = NULL;
}
Py_XINCREF(val);
return val;
}
static PyObject *
dict_clear(register PyDictObject *mp) {
PyDict_Clear((PyObject *)mp);
Py_RETURN_NONE;
}
static PyObject *
dict_pop(PyDictObject *mp, PyObject *args) {
long hash;
PyDictEntry *ep;
PyObject *old_value, *old_key;
PyObject *key, *deflt = NULL;
if(!PyArg_UnpackTuple(args, "pop", 1, 2, &key, &deflt))
return NULL;
if (mp->ma_used == 0) {
if (deflt) {
Py_INCREF(deflt);
return deflt;
}
PyErr_SetString(PyExc_KeyError,
"pop(): dictionary is empty");
return NULL;
}
if (!PyString_CheckExact(key) ||
(hash = ((PyStringObject *) key)->ob_shash) == -1) {
hash = PyObject_Hash(key);
if (hash == -1)
return NULL;
}
ep = (mp->ma_lookup)(mp, key, hash);
if (ep == NULL)
return NULL;
if (ep->me_value == NULL) {
if (deflt) {
Py_INCREF(deflt);
return deflt;
}
set_key_error(key);
return NULL;
}
old_key = ep->me_key;
Py_INCREF(dummy);
ep->me_key = dummy;
old_value = ep->me_value;
ep->me_value = NULL;
mp->ma_used--;
Py_DECREF(old_key);
return old_value;
}
static PyObject *
dict_popitem(PyDictObject *mp) {
Py_ssize_t i = 0;
PyDictEntry *ep;
PyObject *res;
res = PyTuple_New(2);
if (res == NULL)
return NULL;
if (mp->ma_used == 0) {
Py_DECREF(res);
PyErr_SetString(PyExc_KeyError,
"popitem(): dictionary is empty");
return NULL;
}
ep = &mp->ma_table[0];
if (ep->me_value == NULL) {
i = ep->me_hash;
if (i > mp->ma_mask || i < 1)
i = 1;
while ((ep = &mp->ma_table[i])->me_value == NULL) {
i++;
if (i > mp->ma_mask)
i = 1;
}
}
PyTuple_SET_ITEM(res, 0, ep->me_key);
PyTuple_SET_ITEM(res, 1, ep->me_value);
Py_INCREF(dummy);
ep->me_key = dummy;
ep->me_value = NULL;
mp->ma_used--;
assert(mp->ma_table[0].me_value == NULL);
mp->ma_table[0].me_hash = i + 1;
return res;
}
static int
dict_traverse(PyObject *op, visitproc visit, void *arg) {
Py_ssize_t i = 0;
PyObject *pk;
PyObject *pv;
while (PyDict_Next(op, &i, &pk, &pv)) {
Py_VISIT(pk);
Py_VISIT(pv);
}
return 0;
}
static int
dict_tp_clear(PyObject *op) {
PyDict_Clear(op);
return 0;
}
extern PyTypeObject PyDictIterKey_Type;
extern PyTypeObject PyDictIterValue_Type;
extern PyTypeObject PyDictIterItem_Type;
static PyObject *dictiter_new(PyDictObject *, PyTypeObject *);
static PyObject *
dict_iterkeys(PyDictObject *dict) {
return dictiter_new(dict, &PyDictIterKey_Type);
}
static PyObject *
dict_itervalues(PyDictObject *dict) {
return dictiter_new(dict, &PyDictIterValue_Type);
}
static PyObject *
dict_iteritems(PyDictObject *dict) {
return dictiter_new(dict, &PyDictIterItem_Type);
}
static PyObject *
dict_sizeof(PyDictObject *mp) {
Py_ssize_t res;
res = sizeof(PyDictObject);
if (mp->ma_table != mp->ma_smalltable)
res = res + (mp->ma_mask + 1) * sizeof(PyDictEntry);
return PyInt_FromSsize_t(res);
}
PyDoc_STRVAR(has_key__doc__,
"D.has_key(k) -> True if D has a key k, else False");
PyDoc_STRVAR(contains__doc__,
"D.__contains__(k) -> True if D has a key k, else False");
PyDoc_STRVAR(getitem__doc__, "x.__getitem__(y) <==> x[y]");
PyDoc_STRVAR(sizeof__doc__,
"D.__sizeof__() -> size of D in memory, in bytes");
PyDoc_STRVAR(get__doc__,
"D.get(k[,d]) -> D[k] if k in D, else d. d defaults to None.");
PyDoc_STRVAR(setdefault_doc__,
"D.setdefault(k[,d]) -> D.get(k,d), also set D[k]=d if k not in D");
PyDoc_STRVAR(pop__doc__,
"D.pop(k[,d]) -> v, remove specified key and return the corresponding value.\n\
If key is not found, d is returned if given, otherwise KeyError is raised");
PyDoc_STRVAR(popitem__doc__,
"D.popitem() -> (k, v), remove and return some (key, value) pair as a\n\
2-tuple; but raise KeyError if D is empty.");
PyDoc_STRVAR(keys__doc__,
"D.keys() -> list of D's keys");
PyDoc_STRVAR(items__doc__,
"D.items() -> list of D's (key, value) pairs, as 2-tuples");
PyDoc_STRVAR(values__doc__,
"D.values() -> list of D's values");
PyDoc_STRVAR(update__doc__,
"D.update(E, **F) -> None. Update D from dict/iterable E and F.\n"
"If E has a .keys() method, does: for k in E: D[k] = E[k]\n\
If E lacks .keys() method, does: for (k, v) in E: D[k] = v\n\
In either case, this is followed by: for k in F: D[k] = F[k]");
PyDoc_STRVAR(fromkeys__doc__,
"dict.fromkeys(S[,v]) -> New dict with keys from S and values equal to v.\n\
v defaults to None.");
PyDoc_STRVAR(clear__doc__,
"D.clear() -> None. Remove all items from D.");
PyDoc_STRVAR(copy__doc__,
"D.copy() -> a shallow copy of D");
PyDoc_STRVAR(iterkeys__doc__,
"D.iterkeys() -> an iterator over the keys of D");
PyDoc_STRVAR(itervalues__doc__,
"D.itervalues() -> an iterator over the values of D");
PyDoc_STRVAR(iteritems__doc__,
"D.iteritems() -> an iterator over the (key, value) items of D");
static PyMethodDef mapp_methods[] = {
{
"__contains__",(PyCFunction)dict_contains, METH_O | METH_COEXIST,
contains__doc__
},
{
"__getitem__", (PyCFunction)dict_subscript, METH_O | METH_COEXIST,
getitem__doc__
},
{
"__sizeof__", (PyCFunction)dict_sizeof, METH_NOARGS,
sizeof__doc__
},
{
"has_key", (PyCFunction)dict_has_key, METH_O,
has_key__doc__
},
{
"get", (PyCFunction)dict_get, METH_VARARGS,
get__doc__
},
{
"setdefault", (PyCFunction)dict_setdefault, METH_VARARGS,
setdefault_doc__
},
{
"pop", (PyCFunction)dict_pop, METH_VARARGS,
pop__doc__
},
{
"popitem", (PyCFunction)dict_popitem, METH_NOARGS,
popitem__doc__
},
{
"keys", (PyCFunction)dict_keys, METH_NOARGS,
keys__doc__
},
{
"items", (PyCFunction)dict_items, METH_NOARGS,
items__doc__
},
{
"values", (PyCFunction)dict_values, METH_NOARGS,
values__doc__
},
{
"update", (PyCFunction)dict_update, METH_VARARGS | METH_KEYWORDS,
update__doc__
},
{
"fromkeys", (PyCFunction)dict_fromkeys, METH_VARARGS | METH_CLASS,
fromkeys__doc__
},
{
"clear", (PyCFunction)dict_clear, METH_NOARGS,
clear__doc__
},
{
"copy", (PyCFunction)dict_copy, METH_NOARGS,
copy__doc__
},
{
"iterkeys", (PyCFunction)dict_iterkeys, METH_NOARGS,
iterkeys__doc__
},
{
"itervalues", (PyCFunction)dict_itervalues, METH_NOARGS,
itervalues__doc__
},
{
"iteritems", (PyCFunction)dict_iteritems, METH_NOARGS,
iteritems__doc__
},
{NULL, NULL}
};
int
PyDict_Contains(PyObject *op, PyObject *key) {
long hash;
PyDictObject *mp = (PyDictObject *)op;
PyDictEntry *ep;
if (!PyString_CheckExact(key) ||
(hash = ((PyStringObject *) key)->ob_shash) == -1) {
hash = PyObject_Hash(key);
if (hash == -1)
return -1;
}
ep = (mp->ma_lookup)(mp, key, hash);
return ep == NULL ? -1 : (ep->me_value != NULL);
}
int
_PyDict_Contains(PyObject *op, PyObject *key, long hash) {
PyDictObject *mp = (PyDictObject *)op;
PyDictEntry *ep;
ep = (mp->ma_lookup)(mp, key, hash);
return ep == NULL ? -1 : (ep->me_value != NULL);
}
static PySequenceMethods dict_as_sequence = {
0,
0,
0,
0,
0,
0,
0,
PyDict_Contains,
0,
0,
};
static PyObject *
dict_new(PyTypeObject *type, PyObject *args, PyObject *kwds) {
PyObject *self;
assert(type != NULL && type->tp_alloc != NULL);
self = type->tp_alloc(type, 0);
if (self != NULL) {
PyDictObject *d = (PyDictObject *)self;
assert(d->ma_table == NULL && d->ma_fill == 0 && d->ma_used == 0);
INIT_NONZERO_DICT_SLOTS(d);
d->ma_lookup = lookdict_string;
#if defined(SHOW_CONVERSION_COUNTS)
++created;
#endif
}
return self;
}
static int
dict_init(PyObject *self, PyObject *args, PyObject *kwds) {
return dict_update_common(self, args, kwds, "dict");
}
static PyObject *
dict_iter(PyDictObject *dict) {
return dictiter_new(dict, &PyDictIterKey_Type);
}
PyDoc_STRVAR(dictionary_doc,
"dict() -> new empty dictionary.\n"
"dict(mapping) -> new dictionary initialized from a mapping object's\n"
" (key, value) pairs.\n"
"dict(seq) -> new dictionary initialized as if via:\n"
" d = {}\n"
" for k, v in seq:\n"
" d[k] = v\n"
"dict(**kwargs) -> new dictionary initialized with the name=value pairs\n"
" in the keyword argument list. For example: dict(one=1, two=2)");
PyTypeObject PyDict_Type = {
PyVarObject_HEAD_INIT(&PyType_Type, 0)
"dict",
sizeof(PyDictObject),
0,
(destructor)dict_dealloc,
(printfunc)dict_print,
0,
0,
(cmpfunc)dict_compare,
(reprfunc)dict_repr,
0,
&dict_as_sequence,
&dict_as_mapping,
(hashfunc)PyObject_HashNotImplemented,
0,
0,
PyObject_GenericGetAttr,
0,
0,
Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC |
Py_TPFLAGS_BASETYPE | Py_TPFLAGS_DICT_SUBCLASS,
dictionary_doc,
dict_traverse,
dict_tp_clear,
dict_richcompare,
0,
(getiterfunc)dict_iter,
0,
mapp_methods,
0,
0,
0,
0,
0,
0,
0,
dict_init,
PyType_GenericAlloc,
dict_new,
PyObject_GC_Del,
};
PyObject *
PyDict_GetItemString(PyObject *v, const char *key) {
PyObject *kv, *rv;
kv = PyString_FromString(key);
if (kv == NULL)
return NULL;
rv = PyDict_GetItem(v, kv);
Py_DECREF(kv);
return rv;
}
int
PyDict_SetItemString(PyObject *v, const char *key, PyObject *item) {
PyObject *kv;
int err;
kv = PyString_FromString(key);
if (kv == NULL)
return -1;
PyString_InternInPlace(&kv);
err = PyDict_SetItem(v, kv, item);
Py_DECREF(kv);
return err;
}
int
PyDict_DelItemString(PyObject *v, const char *key) {
PyObject *kv;
int err;
kv = PyString_FromString(key);
if (kv == NULL)
return -1;
err = PyDict_DelItem(v, kv);
Py_DECREF(kv);
return err;
}
typedef struct {
PyObject_HEAD
PyDictObject *di_dict;
Py_ssize_t di_used;
Py_ssize_t di_pos;
PyObject* di_result;
Py_ssize_t len;
} dictiterobject;
static PyObject *
dictiter_new(PyDictObject *dict, PyTypeObject *itertype) {
dictiterobject *di;
di = PyObject_New(dictiterobject, itertype);
if (di == NULL)
return NULL;
Py_INCREF(dict);
di->di_dict = dict;
di->di_used = dict->ma_used;
di->di_pos = 0;
di->len = dict->ma_used;
if (itertype == &PyDictIterItem_Type) {
di->di_result = PyTuple_Pack(2, Py_None, Py_None);
if (di->di_result == NULL) {
Py_DECREF(di);
return NULL;
}
} else
di->di_result = NULL;
return (PyObject *)di;
}
static void
dictiter_dealloc(dictiterobject *di) {
Py_XDECREF(di->di_dict);
Py_XDECREF(di->di_result);
PyObject_Del(di);
}
static PyObject *
dictiter_len(dictiterobject *di) {
Py_ssize_t len = 0;
if (di->di_dict != NULL && di->di_used == di->di_dict->ma_used)
len = di->len;
return PyInt_FromSize_t(len);
}
PyDoc_STRVAR(length_hint_doc, "Private method returning an estimate of len(list(it)).");
static PyMethodDef dictiter_methods[] = {
{"__length_hint__", (PyCFunction)dictiter_len, METH_NOARGS, length_hint_doc},
{NULL, NULL}
};
static PyObject *dictiter_iternextkey(dictiterobject *di) {
PyObject *key;
register Py_ssize_t i, mask;
register PyDictEntry *ep;
PyDictObject *d = di->di_dict;
if (d == NULL)
return NULL;
assert (PyDict_Check(d));
if (di->di_used != d->ma_used) {
PyErr_SetString(PyExc_RuntimeError,
"dictionary changed size during iteration");
di->di_used = -1;
return NULL;
}
i = di->di_pos;
if (i < 0)
goto fail;
ep = d->ma_table;
mask = d->ma_mask;
while (i <= mask && ep[i].me_value == NULL)
i++;
di->di_pos = i+1;
if (i > mask)
goto fail;
di->len--;
key = ep[i].me_key;
Py_INCREF(key);
return key;
fail:
Py_DECREF(d);
di->di_dict = NULL;
return NULL;
}
PyTypeObject PyDictIterKey_Type = {
PyVarObject_HEAD_INIT(&PyType_Type, 0)
"dictionary-keyiterator",
sizeof(dictiterobject),
0,
(destructor)dictiter_dealloc,
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
(iternextfunc)dictiter_iternextkey,
dictiter_methods,
0,
};
static PyObject *dictiter_iternextvalue(dictiterobject *di) {
PyObject *value;
register Py_ssize_t i, mask;
register PyDictEntry *ep;
PyDictObject *d = di->di_dict;
if (d == NULL)
return NULL;
assert (PyDict_Check(d));
if (di->di_used != d->ma_used) {
PyErr_SetString(PyExc_RuntimeError,
"dictionary changed size during iteration");
di->di_used = -1;
return NULL;
}
i = di->di_pos;
mask = d->ma_mask;
if (i < 0 || i > mask)
goto fail;
ep = d->ma_table;
while ((value=ep[i].me_value) == NULL) {
i++;
if (i > mask)
goto fail;
}
di->di_pos = i+1;
di->len--;
Py_INCREF(value);
return value;
fail:
Py_DECREF(d);
di->di_dict = NULL;
return NULL;
}
PyTypeObject PyDictIterValue_Type = {
PyVarObject_HEAD_INIT(&PyType_Type, 0)
"dictionary-valueiterator",
sizeof(dictiterobject),
0,
(destructor)dictiter_dealloc,
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
(iternextfunc)dictiter_iternextvalue,
dictiter_methods,
0,
};
static PyObject *dictiter_iternextitem(dictiterobject *di) {
PyObject *key, *value, *result = di->di_result;
register Py_ssize_t i, mask;
register PyDictEntry *ep;
PyDictObject *d = di->di_dict;
if (d == NULL)
return NULL;
assert (PyDict_Check(d));
if (di->di_used != d->ma_used) {
PyErr_SetString(PyExc_RuntimeError,
"dictionary changed size during iteration");
di->di_used = -1;
return NULL;
}
i = di->di_pos;
if (i < 0)
goto fail;
ep = d->ma_table;
mask = d->ma_mask;
while (i <= mask && ep[i].me_value == NULL)
i++;
di->di_pos = i+1;
if (i > mask)
goto fail;
if (result->ob_refcnt == 1) {
Py_INCREF(result);
Py_DECREF(PyTuple_GET_ITEM(result, 0));
Py_DECREF(PyTuple_GET_ITEM(result, 1));
} else {
result = PyTuple_New(2);
if (result == NULL)
return NULL;
}
di->len--;
key = ep[i].me_key;
value = ep[i].me_value;
Py_INCREF(key);
Py_INCREF(value);
PyTuple_SET_ITEM(result, 0, key);
PyTuple_SET_ITEM(result, 1, value);
return result;
fail:
Py_DECREF(d);
di->di_dict = NULL;
return NULL;
}
PyTypeObject PyDictIterItem_Type = {
PyVarObject_HEAD_INIT(&PyType_Type, 0)
"dictionary-itemiterator",
sizeof(dictiterobject),
0,
(destructor)dictiter_dealloc,
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
(iternextfunc)dictiter_iternextitem,
dictiter_methods,
0,
};
