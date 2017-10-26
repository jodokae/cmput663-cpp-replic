#if !defined(SVN_SORTS_H)
#define SVN_SORTS_H
#include <apr_pools.h>
#include <apr_tables.h>
#include <apr_hash.h>
#if !defined(MAX)
#define MAX(a, b) ((a) < (b) ? (b) : (a))
#endif
#if !defined(MIN)
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif
#if defined(__cplusplus)
extern "C" {
#endif
typedef struct svn_sort__item_t {
const void *key;
apr_ssize_t klen;
void *value;
} svn_sort__item_t;
int svn_sort_compare_items_as_paths(const svn_sort__item_t *a,
const svn_sort__item_t *b);
int svn_sort_compare_items_lexically(const svn_sort__item_t *a,
const svn_sort__item_t *b);
int svn_sort_compare_revisions(const void *a, const void *b);
int svn_sort_compare_paths(const void *a, const void *b);
int svn_sort_compare_ranges(const void *a, const void *b);
apr_array_header_t *
svn_sort__hash(apr_hash_t *ht,
int (*comparison_func)(const svn_sort__item_t *,
const svn_sort__item_t *),
apr_pool_t *pool);
#if defined(__cplusplus)
}
#endif
#endif
