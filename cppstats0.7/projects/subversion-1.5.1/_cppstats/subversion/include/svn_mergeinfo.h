#if !defined(SVN_MERGEINFO_H)
#define SVN_MERGEINFO_H
#include <apr_pools.h>
#include <apr_tables.h>
#include <apr_hash.h>
#include "svn_error.h"
#if defined(__cplusplus)
extern "C" {
#endif
#define SVN_MERGEINFO_NONINHERITABLE_STR "*"
typedef apr_hash_t *svn_mergeinfo_t;
typedef apr_hash_t *svn_mergeinfo_catalog_t;
svn_error_t *
svn_mergeinfo_parse(svn_mergeinfo_t *mergeinfo, const char *input,
apr_pool_t *pool);
svn_error_t *
svn_mergeinfo_diff(svn_mergeinfo_t *deleted, svn_mergeinfo_t *added,
svn_mergeinfo_t mergefrom, svn_mergeinfo_t mergeto,
svn_boolean_t consider_inheritance,
apr_pool_t *pool);
svn_error_t *
svn_mergeinfo_merge(svn_mergeinfo_t mergeinfo, svn_mergeinfo_t changes,
apr_pool_t *pool);
svn_error_t *
svn_mergeinfo_remove(svn_mergeinfo_t *mergeinfo, svn_mergeinfo_t eraser,
svn_mergeinfo_t whiteboard, apr_pool_t *pool);
svn_error_t *
svn_rangelist_diff(apr_array_header_t **deleted, apr_array_header_t **added,
apr_array_header_t *from, apr_array_header_t *to,
svn_boolean_t consider_inheritance,
apr_pool_t *pool);
svn_error_t *
svn_rangelist_merge(apr_array_header_t **rangelist,
apr_array_header_t *changes,
apr_pool_t *pool);
svn_error_t *
svn_rangelist_remove(apr_array_header_t **output, apr_array_header_t *eraser,
apr_array_header_t *whiteboard,
svn_boolean_t consider_inheritance,
apr_pool_t *pool);
svn_error_t *
svn_mergeinfo_intersect(svn_mergeinfo_t *mergeinfo,
svn_mergeinfo_t mergeinfo1,
svn_mergeinfo_t mergeinfo2,
apr_pool_t *pool);
svn_error_t *
svn_rangelist_intersect(apr_array_header_t **rangelist,
apr_array_header_t *rangelist1,
apr_array_header_t *rangelist2,
svn_boolean_t consider_inheritance,
apr_pool_t *pool);
svn_error_t *
svn_rangelist_reverse(apr_array_header_t *rangelist, apr_pool_t *pool);
svn_error_t *
svn_rangelist_to_string(svn_string_t **output,
const apr_array_header_t *rangelist,
apr_pool_t *pool);
svn_error_t *
svn_rangelist_inheritable(apr_array_header_t **inheritable_rangelist,
apr_array_header_t *rangelist,
svn_revnum_t start,
svn_revnum_t end,
apr_pool_t *pool);
svn_error_t *
svn_mergeinfo_inheritable(svn_mergeinfo_t *inheritable_mergeinfo,
svn_mergeinfo_t mergeinfo,
const char *path,
svn_revnum_t start,
svn_revnum_t end,
apr_pool_t *pool);
svn_error_t *
svn_mergeinfo_to_string(svn_string_t **output,
svn_mergeinfo_t mergeinput,
apr_pool_t *pool);
svn_error_t *
svn_mergeinfo_sort(svn_mergeinfo_t mergeinfo, apr_pool_t *pool);
svn_mergeinfo_t
svn_mergeinfo_dup(svn_mergeinfo_t mergeinfo, apr_pool_t *pool);
apr_array_header_t *
svn_rangelist_dup(apr_array_header_t *rangelist, apr_pool_t *pool);
typedef enum {
svn_mergeinfo_explicit,
svn_mergeinfo_inherited,
svn_mergeinfo_nearest_ancestor
} svn_mergeinfo_inheritance_t;
const char *
svn_inheritance_to_word(svn_mergeinfo_inheritance_t inherit);
svn_mergeinfo_inheritance_t
svn_inheritance_from_word(const char *word);
#if defined(__cplusplus)
}
#endif
#endif
