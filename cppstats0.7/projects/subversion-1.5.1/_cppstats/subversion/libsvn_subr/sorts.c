#include <apr_pools.h>
#include <apr_hash.h>
#include <apr_tables.h>
#include <stdlib.h>
#include <assert.h>
#include "svn_path.h"
#include "svn_sorts.h"
#include "svn_error.h"
int
svn_sort_compare_items_as_paths(const svn_sort__item_t *a,
const svn_sort__item_t *b) {
const char *astr, *bstr;
astr = a->key;
bstr = b->key;
assert(astr[a->klen] == '\0');
assert(bstr[b->klen] == '\0');
return svn_path_compare_paths(astr, bstr);
}
int
svn_sort_compare_items_lexically(const svn_sort__item_t *a,
const svn_sort__item_t *b) {
int val;
apr_size_t len;
len = (a->klen < b->klen) ? a->klen : b->klen;
val = memcmp(a->key, b->key, len);
if (val != 0)
return val;
return (a->klen < b->klen) ? -1 : (a->klen > b->klen) ? 1 : 0;
}
int
svn_sort_compare_revisions(const void *a, const void *b) {
svn_revnum_t a_rev = *(const svn_revnum_t *)a;
svn_revnum_t b_rev = *(const svn_revnum_t *)b;
if (a_rev == b_rev)
return 0;
return a_rev < b_rev ? 1 : -1;
}
int
svn_sort_compare_paths(const void *a, const void *b) {
const char *item1 = *((const char * const *) a);
const char *item2 = *((const char * const *) b);
return svn_path_compare_paths(item1, item2);
}
int
svn_sort_compare_ranges(const void *a, const void *b) {
const svn_merge_range_t *item1 = *((const svn_merge_range_t * const *) a);
const svn_merge_range_t *item2 = *((const svn_merge_range_t * const *) b);
if (item1->start == item2->start
&& item1->end == item2->end)
return 0;
if (item1->start == item2->start)
return item1->end < item2->end ? -1 : 1;
return item1->start < item2->start ? -1 : 1;
}
apr_array_header_t *
svn_sort__hash(apr_hash_t *ht,
int (*comparison_func)(const svn_sort__item_t *,
const svn_sort__item_t *),
apr_pool_t *pool) {
apr_hash_index_t *hi;
apr_array_header_t *ary;
ary = apr_array_make(pool, apr_hash_count(ht), sizeof(svn_sort__item_t));
for (hi = apr_hash_first(pool, ht); hi; hi = apr_hash_next(hi)) {
svn_sort__item_t *item = apr_array_push(ary);
apr_hash_this(hi, &item->key, &item->klen, &item->value);
}
qsort(ary->elts, ary->nelts, ary->elt_size,
(int (*)(const void *, const void *))comparison_func);
return ary;
}
