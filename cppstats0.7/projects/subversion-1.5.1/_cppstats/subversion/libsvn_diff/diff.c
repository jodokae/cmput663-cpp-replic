#include <apr.h>
#include <apr_pools.h>
#include <apr_general.h>
#include "svn_pools.h"
#include "svn_error.h"
#include "svn_diff.h"
#include "svn_types.h"
#include "diff.h"
svn_diff_t *
svn_diff__diff(svn_diff__lcs_t *lcs,
apr_off_t original_start, apr_off_t modified_start,
svn_boolean_t want_common,
apr_pool_t *pool) {
svn_diff_t *diff;
svn_diff_t **diff_ref = &diff;
while (1) {
if (original_start < lcs->position[0]->offset
|| modified_start < lcs->position[1]->offset) {
(*diff_ref) = apr_palloc(pool, sizeof(**diff_ref));
(*diff_ref)->type = svn_diff__type_diff_modified;
(*diff_ref)->original_start = original_start - 1;
(*diff_ref)->original_length =
lcs->position[0]->offset - original_start;
(*diff_ref)->modified_start = modified_start - 1;
(*diff_ref)->modified_length =
lcs->position[1]->offset - modified_start;
(*diff_ref)->latest_start = 0;
(*diff_ref)->latest_length = 0;
diff_ref = &(*diff_ref)->next;
}
if (lcs->length == 0)
break;
original_start = lcs->position[0]->offset;
modified_start = lcs->position[1]->offset;
if (want_common) {
(*diff_ref) = apr_palloc(pool, sizeof(**diff_ref));
(*diff_ref)->type = svn_diff__type_common;
(*diff_ref)->original_start = original_start - 1;
(*diff_ref)->original_length = lcs->length;
(*diff_ref)->modified_start = modified_start - 1;
(*diff_ref)->modified_length = lcs->length;
(*diff_ref)->latest_start = 0;
(*diff_ref)->latest_length = 0;
diff_ref = &(*diff_ref)->next;
}
original_start += lcs->length;
modified_start += lcs->length;
lcs = lcs->next;
}
*diff_ref = NULL;
return diff;
}
svn_error_t *
svn_diff_diff(svn_diff_t **diff,
void *diff_baton,
const svn_diff_fns_t *vtable,
apr_pool_t *pool) {
svn_diff__tree_t *tree;
svn_diff__position_t *position_list[2];
svn_diff__lcs_t *lcs;
apr_pool_t *subpool;
apr_pool_t *treepool;
*diff = NULL;
subpool = svn_pool_create(pool);
treepool = svn_pool_create(pool);
svn_diff__tree_create(&tree, treepool);
SVN_ERR(svn_diff__get_tokens(&position_list[0],
tree,
diff_baton, vtable,
svn_diff_datasource_original,
subpool));
SVN_ERR(svn_diff__get_tokens(&position_list[1],
tree,
diff_baton, vtable,
svn_diff_datasource_modified,
subpool));
if (vtable->token_discard_all != NULL)
vtable->token_discard_all(diff_baton);
svn_pool_destroy(treepool);
lcs = svn_diff__lcs(position_list[0], position_list[1], subpool);
*diff = svn_diff__diff(lcs, 1, 1, TRUE, pool);
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
