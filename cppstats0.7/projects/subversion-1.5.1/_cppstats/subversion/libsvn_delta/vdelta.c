#include <assert.h>
#include <apr_general.h>
#include "svn_delta.h"
#include "delta.h"
#define VD_KEY_SIZE 4
typedef struct hash_slot_t {
struct hash_slot_t *next;
} hash_slot_t;
typedef struct hash_table_t {
int num_buckets;
hash_slot_t **buckets;
hash_slot_t *slots;
} hash_table_t;
static hash_table_t *
create_hash_table(apr_size_t num_slots, apr_pool_t *pool) {
int i;
apr_size_t j;
hash_table_t* table = apr_palloc(pool, sizeof(*table));
table->num_buckets = (num_slots / 3) | 1;
table->buckets = apr_palloc(pool, (table->num_buckets
* sizeof(*table->buckets)));
for (i = 0; i < table->num_buckets; ++i)
table->buckets[i] = NULL;
table->slots = apr_palloc(pool, num_slots * sizeof(*table->slots));
for (j = 0; j < num_slots; ++j)
table->slots[j].next = NULL;
return table;
}
static APR_INLINE apr_uint32_t
get_bucket(const hash_table_t *table, const char *key) {
int i;
apr_uint32_t hash = 0;
for (i = 0; i < VD_KEY_SIZE; ++i)
hash = hash * 127 + *key++;
return hash % table->num_buckets;
}
static APR_INLINE void
store_mapping(hash_table_t *table, const char* key, apr_size_t idx) {
apr_uint32_t bucket = get_bucket(table, key);
assert(table->slots[idx].next == NULL);
table->slots[idx].next = table->buckets[bucket];
table->buckets[bucket] = &table->slots[idx];
}
static APR_INLINE int
find_match_len(const char *match, const char *from, const char *end) {
const char *here = from;
while (here < end && *match == *here) {
++match;
++here;
}
return here - from;
}
static void
vdelta(svn_txdelta__ops_baton_t *build_baton,
const char *data,
const char *start,
const char *end,
svn_boolean_t outputflag,
hash_table_t *table,
apr_pool_t *pool) {
const char *here = start;
const char *insert_from = NULL;
for (;;) {
const char *current_match, *key;
apr_size_t current_match_len = 0;
hash_slot_t *slot;
svn_boolean_t progress;
if (end - here < VD_KEY_SIZE) {
const char *from = ((insert_from != NULL) ? insert_from : here);
if (outputflag && from < end)
svn_txdelta__insert_op(build_baton, svn_txdelta_new, 0,
end - from, from, pool);
return;
}
current_match = NULL;
current_match_len = 0;
key = here;
do {
progress = FALSE;
for (slot = table->buckets[get_bucket(table, key)];
slot != NULL;
slot = slot->next) {
const char *match;
apr_size_t match_len;
if (slot - table->slots < key - here)
continue;
match = data + (slot - table->slots) - (key - here);
match_len = find_match_len(match, here, end);
if (match < start && match + match_len > start)
match_len = start - match;
if (match_len >= VD_KEY_SIZE && match_len > current_match_len) {
current_match = match;
current_match_len = match_len;
progress = TRUE;
}
}
if (progress)
key = here + current_match_len - (VD_KEY_SIZE - 1);
} while (progress && end - key >= VD_KEY_SIZE);
if (current_match_len < VD_KEY_SIZE) {
store_mapping(table, here, here - data);
if (insert_from == NULL)
insert_from = here;
here++;
continue;
} else if (outputflag) {
if (insert_from != NULL) {
svn_txdelta__insert_op(build_baton, svn_txdelta_new,
0, here - insert_from,
insert_from, pool);
insert_from = NULL;
}
if (current_match < start)
svn_txdelta__insert_op(build_baton, svn_txdelta_source,
current_match - data,
current_match_len,
NULL, pool);
else
svn_txdelta__insert_op(build_baton, svn_txdelta_target,
current_match - start,
current_match_len,
NULL, pool);
}
here += current_match_len;
if (end - here >= VD_KEY_SIZE) {
char const *last = here - (VD_KEY_SIZE - 1);
for (; last < here; ++last)
store_mapping(table, last, last - data);
}
}
}
void
svn_txdelta__vdelta(svn_txdelta__ops_baton_t *build_baton,
const char *data,
apr_size_t source_len,
apr_size_t target_len,
apr_pool_t *pool) {
hash_table_t *table = create_hash_table(source_len + target_len, pool);
vdelta(build_baton, data, data, data + source_len, FALSE, table, pool);
vdelta(build_baton, data, data + source_len, data + source_len + target_len,
TRUE, table, pool);
#if 0
{
int i;
int empty = 0;
int collisions = 0;
for (i = 0; i < table->num_buckets; ++i) {
hash_slot_t *slot = table->buckets[i];
if (!slot)
++empty;
else {
slot = slot->next;
while (slot != NULL) {
++collisions;
slot = slot->next;
}
}
}
fprintf(stderr, "Hash stats: load %d, collisions %d\n",
100 - 100 * empty / table->num_buckets, collisions);
}
#endif
}
