#include <apr.h>
#include <apr_pools.h>
#include <apr_general.h>
#include "svn_pools.h"
#include "svn_error.h"
#include "svn_diff.h"
#include "svn_types.h"
#include "diff.h"
void
svn_diff__resolve_conflict(svn_diff_t *hunk,
svn_diff__position_t **position_list1,
svn_diff__position_t **position_list2,
apr_pool_t *pool) {
apr_off_t modified_start = hunk->modified_start + 1;
apr_off_t latest_start = hunk->latest_start + 1;
apr_off_t common_length;
apr_off_t modified_length = hunk->modified_length;
apr_off_t latest_length = hunk->latest_length;
svn_diff__position_t *start_position[2];
svn_diff__position_t *position[2];
svn_diff__lcs_t *lcs = NULL;
svn_diff__lcs_t **lcs_ref = &lcs;
svn_diff_t **diff_ref = &hunk->resolved_diff;
apr_pool_t *subpool;
start_position[0] = *position_list1;
start_position[1] = *position_list2;
while (start_position[0]->offset < modified_start)
start_position[0] = start_position[0]->next;
while (start_position[1]->offset < latest_start)
start_position[1] = start_position[1]->next;
position[0] = start_position[0];
position[1] = start_position[1];
common_length = modified_length < latest_length
? modified_length : latest_length;
while (common_length > 0
&& position[0]->node == position[1]->node) {
position[0] = position[0]->next;
position[1] = position[1]->next;
common_length--;
}
if (common_length == 0
&& modified_length == latest_length) {
hunk->type = svn_diff__type_diff_common;
hunk->resolved_diff = NULL;
*position_list1 = position[0];
*position_list2 = position[1];
return;
}
hunk->type = svn_diff__type_conflict;
subpool = svn_pool_create(pool);
common_length = (modified_length < latest_length
? modified_length : latest_length)
- common_length;
if (common_length > 0) {
lcs = apr_palloc(subpool, sizeof(*lcs));
lcs->next = NULL;
lcs->position[0] = start_position[0];
lcs->position[1] = start_position[1];
lcs->length = common_length;
lcs_ref = &lcs->next;
}
modified_length -= common_length;
latest_length -= common_length;
modified_start = start_position[0]->offset;
latest_start = start_position[1]->offset;
start_position[0] = position[0];
start_position[1] = position[1];
if (modified_length == 0) {
*position_list1 = position[0];
position[0] = NULL;
} else {
while (--modified_length)
position[0] = position[0]->next;
*position_list1 = position[0]->next;
position[0]->next = start_position[0];
}
if (latest_length == 0) {
*position_list2 = position[1];
position[1] = NULL;
} else {
while (--latest_length)
position[1] = position[1]->next;
*position_list2 = position[1]->next;
position[1]->next = start_position[1];
}
*lcs_ref = svn_diff__lcs(position[0], position[1],
subpool);
if ((*lcs_ref)->position[0]->offset == 1)
(*lcs_ref)->position[0] = *position_list1;
if ((*lcs_ref)->position[1]->offset == 1)
(*lcs_ref)->position[1] = *position_list2;
modified_length = hunk->modified_length;
latest_length = hunk->latest_length;
while (1) {
if (modified_start < lcs->position[0]->offset
|| latest_start < lcs->position[1]->offset) {
(*diff_ref) = apr_palloc(pool, sizeof(**diff_ref));
(*diff_ref)->type = svn_diff__type_conflict;
(*diff_ref)->original_start = hunk->original_start;
(*diff_ref)->original_length = hunk->original_length;
(*diff_ref)->modified_start = modified_start - 1;
(*diff_ref)->modified_length = lcs->position[0]->offset
- modified_start;
(*diff_ref)->latest_start = latest_start - 1;
(*diff_ref)->latest_length = lcs->position[1]->offset
- latest_start;
(*diff_ref)->resolved_diff = NULL;
diff_ref = &(*diff_ref)->next;
}
if (lcs->length == 0)
break;
modified_start = lcs->position[0]->offset;
latest_start = lcs->position[1]->offset;
(*diff_ref) = apr_palloc(pool, sizeof(**diff_ref));
(*diff_ref)->type = svn_diff__type_diff_common;
(*diff_ref)->original_start = hunk->original_start;
(*diff_ref)->original_length = hunk->original_length;
(*diff_ref)->modified_start = modified_start - 1;
(*diff_ref)->modified_length = lcs->length;
(*diff_ref)->latest_start = latest_start - 1;
(*diff_ref)->latest_length = lcs->length;
(*diff_ref)->resolved_diff = NULL;
diff_ref = &(*diff_ref)->next;
modified_start += lcs->length;
latest_start += lcs->length;
lcs = lcs->next;
}
*diff_ref = NULL;
svn_pool_destroy(subpool);
}
svn_error_t *
svn_diff_diff3(svn_diff_t **diff,
void *diff_baton,
const svn_diff_fns_t *vtable,
apr_pool_t *pool) {
svn_diff__tree_t *tree;
svn_diff__position_t *position_list[3];
svn_diff__lcs_t *lcs_om;
svn_diff__lcs_t *lcs_ol;
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
SVN_ERR(svn_diff__get_tokens(&position_list[2],
tree,
diff_baton, vtable,
svn_diff_datasource_latest,
subpool));
if (vtable->token_discard_all != NULL)
vtable->token_discard_all(diff_baton);
svn_pool_destroy(treepool);
lcs_om = svn_diff__lcs(position_list[0], position_list[1],
subpool);
lcs_ol = svn_diff__lcs(position_list[0], position_list[2],
subpool);
{
svn_diff_t **diff_ref = diff;
apr_off_t original_start = 1;
apr_off_t modified_start = 1;
apr_off_t latest_start = 1;
apr_off_t original_sync;
apr_off_t modified_sync;
apr_off_t latest_sync;
apr_off_t common_length;
apr_off_t original_length;
apr_off_t modified_length;
apr_off_t latest_length;
svn_boolean_t is_modified;
svn_boolean_t is_latest;
svn_diff__position_t sentinel_position[2];
if (position_list[1]) {
sentinel_position[0].next = position_list[1]->next;
sentinel_position[0].offset = position_list[1]->offset + 1;
position_list[1]->next = &sentinel_position[0];
position_list[1] = sentinel_position[0].next;
} else {
sentinel_position[0].offset = 1;
sentinel_position[0].next = NULL;
position_list[1] = &sentinel_position[0];
}
if (position_list[2]) {
sentinel_position[1].next = position_list[2]->next;
sentinel_position[1].offset = position_list[2]->offset + 1;
position_list[2]->next = &sentinel_position[1];
position_list[2] = sentinel_position[1].next;
} else {
sentinel_position[1].offset = 1;
sentinel_position[1].next = NULL;
position_list[2] = &sentinel_position[1];
}
while (1) {
while (1) {
if (lcs_om->position[0]->offset > lcs_ol->position[0]->offset) {
original_sync = lcs_om->position[0]->offset;
while (lcs_ol->position[0]->offset + lcs_ol->length
< original_sync)
lcs_ol = lcs_ol->next;
if (lcs_om->length == 0 && lcs_ol->length > 0
&& lcs_ol->position[0]->offset + lcs_ol->length
== original_sync
&& lcs_ol->position[1]->offset + lcs_ol->length
!= lcs_ol->next->position[1]->offset)
lcs_ol = lcs_ol->next;
if (lcs_ol->position[0]->offset <= original_sync)
break;
} else {
original_sync = lcs_ol->position[0]->offset;
while (lcs_om->position[0]->offset + lcs_om->length
< original_sync)
lcs_om = lcs_om->next;
if (lcs_ol->length == 0 && lcs_om->length > 0
&& lcs_om->position[0]->offset + lcs_om->length
== original_sync
&& lcs_om->position[1]->offset + lcs_om->length
!= lcs_om->next->position[1]->offset)
lcs_om = lcs_om->next;
if (lcs_om->position[0]->offset <= original_sync)
break;
}
}
modified_sync = lcs_om->position[1]->offset
+ (original_sync - lcs_om->position[0]->offset);
latest_sync = lcs_ol->position[1]->offset
+ (original_sync - lcs_ol->position[0]->offset);
is_modified = lcs_om->position[0]->offset - original_start > 0
|| lcs_om->position[1]->offset - modified_start > 0;
is_latest = lcs_ol->position[0]->offset - original_start > 0
|| lcs_ol->position[1]->offset - latest_start > 0;
if (is_modified || is_latest) {
original_length = original_sync - original_start;
modified_length = modified_sync - modified_start;
latest_length = latest_sync - latest_start;
(*diff_ref) = apr_palloc(pool, sizeof(**diff_ref));
(*diff_ref)->original_start = original_start - 1;
(*diff_ref)->original_length = original_sync - original_start;
(*diff_ref)->modified_start = modified_start - 1;
(*diff_ref)->modified_length = modified_length;
(*diff_ref)->latest_start = latest_start - 1;
(*diff_ref)->latest_length = latest_length;
(*diff_ref)->resolved_diff = NULL;
if (is_modified && is_latest) {
svn_diff__resolve_conflict(*diff_ref,
&position_list[1],
&position_list[2],
pool);
} else if (is_modified) {
(*diff_ref)->type = svn_diff__type_diff_modified;
} else {
(*diff_ref)->type = svn_diff__type_diff_latest;
}
diff_ref = &(*diff_ref)->next;
}
if (lcs_om->length == 0 || lcs_ol->length == 0)
break;
modified_length = lcs_om->length
- (original_sync - lcs_om->position[0]->offset);
latest_length = lcs_ol->length
- (original_sync - lcs_ol->position[0]->offset);
common_length = modified_length < latest_length
? modified_length : latest_length;
(*diff_ref) = apr_palloc(pool, sizeof(**diff_ref));
(*diff_ref)->type = svn_diff__type_common;
(*diff_ref)->original_start = original_sync - 1;
(*diff_ref)->original_length = common_length;
(*diff_ref)->modified_start = modified_sync - 1;
(*diff_ref)->modified_length = common_length;
(*diff_ref)->latest_start = latest_sync - 1;
(*diff_ref)->latest_length = common_length;
(*diff_ref)->resolved_diff = NULL;
diff_ref = &(*diff_ref)->next;
original_start = original_sync + common_length;
modified_start = modified_sync + common_length;
latest_start = latest_sync + common_length;
if (position_list[1]->offset < lcs_om->position[1]->offset)
position_list[1] = lcs_om->position[1];
if (position_list[2]->offset < lcs_ol->position[1]->offset)
position_list[2] = lcs_ol->position[1];
while (original_start >= lcs_om->position[0]->offset + lcs_om->length
&& lcs_om->length > 0) {
lcs_om = lcs_om->next;
}
while (original_start >= lcs_ol->position[0]->offset + lcs_ol->length
&& lcs_ol->length > 0) {
lcs_ol = lcs_ol->next;
}
}
*diff_ref = NULL;
}
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
