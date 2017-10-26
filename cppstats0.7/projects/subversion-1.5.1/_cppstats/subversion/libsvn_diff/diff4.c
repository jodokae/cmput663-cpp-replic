#include <apr.h>
#include <apr_pools.h>
#include <apr_general.h>
#include "svn_pools.h"
#include "svn_error.h"
#include "svn_diff.h"
#include "svn_types.h"
#include "diff.h"
static void
adjust_diff(svn_diff_t *diff, svn_diff_t *adjust) {
svn_diff_t *hunk;
apr_off_t range_start;
apr_off_t range_end;
apr_off_t adjustment;
for (; adjust; adjust = adjust->next) {
range_start = adjust->modified_start;
range_end = range_start + adjust->modified_length;
adjustment = adjust->original_length - adjust->modified_length;
if (adjustment == 0)
continue;
for (hunk = diff; hunk; hunk = hunk->next) {
if (hunk->modified_start >= range_end) {
hunk->modified_start += adjustment;
continue;
}
if (hunk->modified_start + hunk->modified_length <= range_start)
continue;
if (hunk->type == svn_diff__type_diff_modified) {
hunk->modified_length += adjustment;
continue;
}
if (adjustment < 0)
hunk->type = svn_diff__type_conflict;
hunk->modified_length -= adjustment;
}
}
}
svn_error_t *
svn_diff_diff4(svn_diff_t **diff,
void *diff_baton,
const svn_diff_fns_t *vtable,
apr_pool_t *pool) {
svn_diff__tree_t *tree;
svn_diff__position_t *position_list[4];
svn_diff__lcs_t *lcs_ol;
svn_diff__lcs_t *lcs_adjust;
svn_diff_t *diff_ol;
svn_diff_t *diff_adjust;
svn_diff_t *hunk;
apr_pool_t *subpool;
apr_pool_t *subpool2;
apr_pool_t *subpool3;
*diff = NULL;
subpool = svn_pool_create(pool);
subpool2 = svn_pool_create(subpool);
subpool3 = svn_pool_create(subpool2);
svn_diff__tree_create(&tree, subpool3);
SVN_ERR(svn_diff__get_tokens(&position_list[0],
tree,
diff_baton, vtable,
svn_diff_datasource_original,
subpool2));
SVN_ERR(svn_diff__get_tokens(&position_list[1],
tree,
diff_baton, vtable,
svn_diff_datasource_modified,
subpool));
SVN_ERR(svn_diff__get_tokens(&position_list[2],
tree,
diff_baton, vtable,
svn_diff_datasource_latest,
subpool));
SVN_ERR(svn_diff__get_tokens(&position_list[3],
tree,
diff_baton, vtable,
svn_diff_datasource_ancestor,
subpool2));
if (vtable->token_discard_all != NULL)
vtable->token_discard_all(diff_baton);
svn_pool_clear(subpool3);
lcs_ol = svn_diff__lcs(position_list[0], position_list[2], subpool3);
diff_ol = svn_diff__diff(lcs_ol, 1, 1, TRUE, pool);
svn_pool_clear(subpool3);
for (hunk = diff_ol; hunk; hunk = hunk->next) {
hunk->latest_start = hunk->modified_start;
hunk->latest_length = hunk->modified_length;
hunk->modified_start = hunk->original_start;
hunk->modified_length = hunk->original_length;
if (hunk->type == svn_diff__type_diff_modified)
hunk->type = svn_diff__type_diff_latest;
else
hunk->type = svn_diff__type_diff_modified;
}
lcs_adjust = svn_diff__lcs(position_list[3], position_list[2], subpool3);
diff_adjust = svn_diff__diff(lcs_adjust, 1, 1, FALSE, subpool3);
adjust_diff(diff_ol, diff_adjust);
svn_pool_clear(subpool3);
lcs_adjust = svn_diff__lcs(position_list[1], position_list[3], subpool3);
diff_adjust = svn_diff__diff(lcs_adjust, 1, 1, FALSE, subpool3);
adjust_diff(diff_ol, diff_adjust);
svn_pool_destroy(subpool2);
for (hunk = diff_ol; hunk; hunk = hunk->next) {
if (hunk->type == svn_diff__type_conflict) {
svn_diff__resolve_conflict(hunk, &position_list[1],
&position_list[2], pool);
}
}
svn_pool_destroy(subpool);
*diff = diff_ol;
return SVN_NO_ERROR;
}
