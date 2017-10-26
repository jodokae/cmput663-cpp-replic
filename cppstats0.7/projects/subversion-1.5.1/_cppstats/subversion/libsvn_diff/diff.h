#if !defined(DIFF_H)
#define DIFF_H
#include <apr.h>
#include <apr_pools.h>
#include <apr_general.h>
#include "svn_diff.h"
#include "svn_types.h"
#define SVN_DIFF__UNIFIED_CONTEXT_SIZE 3
typedef struct svn_diff__node_t svn_diff__node_t;
typedef struct svn_diff__tree_t svn_diff__tree_t;
typedef struct svn_diff__position_t svn_diff__position_t;
typedef struct svn_diff__lcs_t svn_diff__lcs_t;
typedef enum svn_diff__type_e {
svn_diff__type_common,
svn_diff__type_diff_modified,
svn_diff__type_diff_latest,
svn_diff__type_diff_common,
svn_diff__type_conflict
} svn_diff__type_e;
struct svn_diff_t {
svn_diff_t *next;
svn_diff__type_e type;
apr_off_t original_start;
apr_off_t original_length;
apr_off_t modified_start;
apr_off_t modified_length;
apr_off_t latest_start;
apr_off_t latest_length;
svn_diff_t *resolved_diff;
};
struct svn_diff__position_t {
svn_diff__position_t *next;
svn_diff__node_t *node;
apr_off_t offset;
};
struct svn_diff__lcs_t {
svn_diff__lcs_t *next;
svn_diff__position_t *position[2];
apr_off_t length;
int refcount;
};
typedef enum svn_diff__normalize_state_t {
svn_diff__normalize_state_normal,
svn_diff__normalize_state_whitespace,
svn_diff__normalize_state_cr
} svn_diff__normalize_state_t;
svn_diff__lcs_t *
svn_diff__lcs(svn_diff__position_t *position_list1,
svn_diff__position_t *position_list2,
apr_pool_t *pool);
void
svn_diff__tree_create(svn_diff__tree_t **tree, apr_pool_t *pool);
svn_error_t *
svn_diff__get_tokens(svn_diff__position_t **position_list,
svn_diff__tree_t *tree,
void *diff_baton,
const svn_diff_fns_t *vtable,
svn_diff_datasource_e datasource,
apr_pool_t *pool);
svn_diff_t *
svn_diff__diff(svn_diff__lcs_t *lcs,
apr_off_t original_start, apr_off_t modified_start,
svn_boolean_t want_common,
apr_pool_t *pool);
void
svn_diff__resolve_conflict(svn_diff_t *hunk,
svn_diff__position_t **position_list1,
svn_diff__position_t **position_list2,
apr_pool_t *pool);
apr_uint32_t
svn_diff__adler32(apr_uint32_t checksum, const char *data, apr_size_t len);
void
svn_diff__normalize_buffer(char **tgt,
apr_off_t *lengthp,
svn_diff__normalize_state_t *statep,
const char *buf,
const svn_diff_file_options_t *opts);
#endif
