#include <string.h>
#include "svn_compat.h"
#include "svn_private_config.h"
#include "svn_pools.h"
#include "svn_error.h"
#include "svn_error_codes.h"
#include "svn_fs.h"
#include "svn_repos.h"
#include "svn_string.h"
#include "svn_time.h"
#include "svn_sorts.h"
#include "svn_path.h"
#include "svn_props.h"
#include "svn_mergeinfo.h"
#include "repos.h"
#include <assert.h>
static svn_error_t *
get_time(apr_time_t *tm,
svn_fs_t *fs,
svn_revnum_t rev,
apr_pool_t *pool) {
svn_string_t *date_str;
SVN_ERR(svn_fs_revision_prop(&date_str, fs, rev, SVN_PROP_REVISION_DATE,
pool));
if (! date_str)
return svn_error_createf
(SVN_ERR_FS_GENERAL, NULL,
_("Failed to find time on revision %ld"), rev);
SVN_ERR(svn_time_from_cstring(tm, date_str->data, pool));
return SVN_NO_ERROR;
}
svn_error_t *
svn_repos_dated_revision(svn_revnum_t *revision,
svn_repos_t *repos,
apr_time_t tm,
apr_pool_t *pool) {
svn_revnum_t rev_mid, rev_top, rev_bot, rev_latest;
apr_time_t this_time;
svn_fs_t *fs = repos->fs;
SVN_ERR(svn_fs_youngest_rev(&rev_latest, fs, pool));
rev_bot = 0;
rev_top = rev_latest;
while (rev_bot <= rev_top) {
rev_mid = (rev_top + rev_bot) / 2;
SVN_ERR(get_time(&this_time, fs, rev_mid, pool));
if (this_time > tm) {
apr_time_t previous_time;
if ((rev_mid - 1) < 0) {
*revision = 0;
break;
}
SVN_ERR(get_time(&previous_time, fs, rev_mid - 1, pool));
if (previous_time <= tm) {
*revision = rev_mid - 1;
break;
}
rev_top = rev_mid - 1;
}
else if (this_time < tm) {
apr_time_t next_time;
if ((rev_mid + 1) > rev_latest) {
*revision = rev_latest;
break;
}
SVN_ERR(get_time(&next_time, fs, rev_mid + 1, pool));
if (next_time > tm) {
*revision = rev_mid;
break;
}
rev_bot = rev_mid + 1;
}
else {
*revision = rev_mid;
break;
}
}
return SVN_NO_ERROR;
}
svn_error_t *
svn_repos_get_committed_info(svn_revnum_t *committed_rev,
const char **committed_date,
const char **last_author,
svn_fs_root_t *root,
const char *path,
apr_pool_t *pool) {
svn_fs_t *fs = svn_fs_root_fs(root);
svn_string_t *committed_date_s, *last_author_s;
SVN_ERR(svn_fs_node_created_rev(committed_rev, root, path, pool));
SVN_ERR(svn_fs_revision_prop(&committed_date_s, fs, *committed_rev,
SVN_PROP_REVISION_DATE, pool));
SVN_ERR(svn_fs_revision_prop(&last_author_s, fs, *committed_rev,
SVN_PROP_REVISION_AUTHOR, pool));
*committed_date = committed_date_s ? committed_date_s->data : NULL;
*last_author = last_author_s ? last_author_s->data : NULL;
return SVN_NO_ERROR;
}
svn_error_t *
svn_repos_history(svn_fs_t *fs,
const char *path,
svn_repos_history_func_t history_func,
void *history_baton,
svn_revnum_t start,
svn_revnum_t end,
svn_boolean_t cross_copies,
apr_pool_t *pool) {
return svn_repos_history2(fs, path, history_func, history_baton,
NULL, NULL,
start, end, cross_copies, pool);
}
svn_error_t *
svn_repos_history2(svn_fs_t *fs,
const char *path,
svn_repos_history_func_t history_func,
void *history_baton,
svn_repos_authz_func_t authz_read_func,
void *authz_read_baton,
svn_revnum_t start,
svn_revnum_t end,
svn_boolean_t cross_copies,
apr_pool_t *pool) {
svn_fs_history_t *history;
apr_pool_t *oldpool = svn_pool_create(pool);
apr_pool_t *newpool = svn_pool_create(pool);
const char *history_path;
svn_revnum_t history_rev;
svn_fs_root_t *root;
if (! SVN_IS_VALID_REVNUM(start))
return svn_error_createf
(SVN_ERR_FS_NO_SUCH_REVISION, 0,
_("Invalid start revision %ld"), start);
if (! SVN_IS_VALID_REVNUM(end))
return svn_error_createf
(SVN_ERR_FS_NO_SUCH_REVISION, 0,
_("Invalid end revision %ld"), end);
if (start > end) {
svn_revnum_t tmprev = start;
start = end;
end = tmprev;
}
SVN_ERR(svn_fs_revision_root(&root, fs, end, pool));
if (authz_read_func) {
svn_boolean_t readable;
SVN_ERR(authz_read_func(&readable, root, path,
authz_read_baton, pool));
if (! readable)
return svn_error_create(SVN_ERR_AUTHZ_UNREADABLE, NULL, NULL);
}
SVN_ERR(svn_fs_node_history(&history, root, path, oldpool));
do {
apr_pool_t *tmppool;
svn_error_t *err;
SVN_ERR(svn_fs_history_prev(&history, history, cross_copies, newpool));
if (! history)
break;
SVN_ERR(svn_fs_history_location(&history_path, &history_rev,
history, newpool));
if (history_rev < start)
break;
if (authz_read_func) {
svn_boolean_t readable;
svn_fs_root_t *history_root;
SVN_ERR(svn_fs_revision_root(&history_root, fs,
history_rev, newpool));
SVN_ERR(authz_read_func(&readable, history_root, history_path,
authz_read_baton, newpool));
if (! readable)
break;
}
err = history_func(history_baton, history_path, history_rev, newpool);
if (err) {
if (err->apr_err == SVN_ERR_CEASE_INVOCATION) {
svn_error_clear(err);
goto cleanup;
} else {
return err;
}
}
svn_pool_clear(oldpool);
tmppool = oldpool;
oldpool = newpool;
newpool = tmppool;
} while (history);
cleanup:
svn_pool_destroy(oldpool);
svn_pool_destroy(newpool);
return SVN_NO_ERROR;
}
svn_error_t *
svn_repos_deleted_rev(svn_fs_t *fs,
const char *path,
svn_revnum_t start,
svn_revnum_t end,
svn_revnum_t *deleted,
apr_pool_t *pool) {
apr_pool_t *subpool;
svn_fs_root_t *root, *copy_root;
const char *copy_path;
svn_revnum_t mid_rev;
const svn_fs_id_t *start_node_id, *curr_node_id;
svn_error_t *err;
if (! SVN_IS_VALID_REVNUM(start))
return svn_error_createf
(SVN_ERR_FS_NO_SUCH_REVISION, 0,
_("Invalid start revision %ld"), start);
if (! SVN_IS_VALID_REVNUM(end))
return svn_error_createf
(SVN_ERR_FS_NO_SUCH_REVISION, 0,
_("Invalid end revision %ld"), end);
if (start > end) {
svn_revnum_t tmprev = start;
start = end;
end = tmprev;
}
SVN_ERR(svn_fs_revision_root(&root, fs, start, pool));
err = svn_fs_node_id(&start_node_id, root, path, pool);
if (err) {
if (err->apr_err == SVN_ERR_FS_NOT_FOUND) {
*deleted = SVN_INVALID_REVNUM;
svn_error_clear(err);
return SVN_NO_ERROR;
}
return err;
}
SVN_ERR(svn_fs_revision_root(&root, fs, end, pool));
err = svn_fs_node_id(&curr_node_id, root, path, pool);
if (err && err->apr_err == SVN_ERR_FS_NOT_FOUND) {
svn_error_clear(err);
} else if (err) {
return err;
} else {
SVN_ERR(svn_fs_node_id(&curr_node_id, root, path, pool));
if (svn_fs_compare_ids(start_node_id, curr_node_id) != -1) {
SVN_ERR(svn_fs_closest_copy(&copy_root, &copy_path, root,
path, pool));
if (!copy_root ||
(svn_fs_revision_root_revision(copy_root) <= start)) {
*deleted = SVN_INVALID_REVNUM;
return SVN_NO_ERROR;
}
}
}
mid_rev = (start + end) / 2;
subpool = svn_pool_create(pool);
while (1) {
svn_pool_clear(subpool);
SVN_ERR(svn_fs_revision_root(&root, fs, mid_rev, subpool));
err = svn_fs_node_id(&curr_node_id, root, path, subpool);
if (err) {
if (err->apr_err == SVN_ERR_FS_NOT_FOUND) {
svn_error_clear(err);
end = mid_rev;
mid_rev = (start + mid_rev) / 2;
} else
return err;
} else {
int cmp = svn_fs_compare_ids(start_node_id, curr_node_id);
SVN_ERR(svn_fs_closest_copy(&copy_root, &copy_path, root,
path, subpool));
if (cmp == -1 ||
(copy_root &&
(svn_fs_revision_root_revision(copy_root) > start))) {
end = mid_rev;
mid_rev = (start + mid_rev) / 2;
} else if (end - mid_rev == 1) {
*deleted = end;
break;
} else {
start = mid_rev;
mid_rev = (start + end) / 2;
}
}
}
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
static svn_error_t *
check_readability(svn_fs_root_t *root,
const char *path,
svn_repos_authz_func_t authz_read_func,
void *authz_read_baton,
apr_pool_t *pool) {
svn_boolean_t readable;
SVN_ERR(authz_read_func(&readable, root, path, authz_read_baton, pool));
if (! readable)
return svn_error_create(SVN_ERR_AUTHZ_UNREADABLE, NULL,
_("Unreadable path encountered; access denied"));
return SVN_NO_ERROR;
}
static svn_error_t *
check_ancestry_of_peg_path(svn_boolean_t *is_ancestor,
svn_fs_t *fs,
const char *fs_path,
svn_revnum_t peg_revision,
svn_revnum_t future_revision,
apr_pool_t *pool) {
svn_fs_root_t *root;
svn_fs_history_t *history;
const char *path;
svn_revnum_t revision;
apr_pool_t *lastpool, *currpool;
lastpool = svn_pool_create(pool);
currpool = svn_pool_create(pool);
SVN_ERR(svn_fs_revision_root(&root, fs, future_revision, pool));
SVN_ERR(svn_fs_node_history(&history, root, fs_path, lastpool));
fs_path = NULL;
while (1) {
apr_pool_t *tmppool;
SVN_ERR(svn_fs_history_prev(&history, history, TRUE, currpool));
if (!history)
break;
SVN_ERR(svn_fs_history_location(&path, &revision, history, currpool));
if (!fs_path)
fs_path = apr_pstrdup(pool, path);
if (revision <= peg_revision)
break;
svn_pool_clear(lastpool);
tmppool = lastpool;
lastpool = currpool;
currpool = tmppool;
}
assert(fs_path != NULL);
*is_ancestor = (history && strcmp(path, fs_path) == 0);
return SVN_NO_ERROR;
}
svn_error_t *
svn_repos__prev_location(svn_revnum_t *appeared_rev,
const char **prev_path,
svn_revnum_t *prev_rev,
svn_fs_t *fs,
svn_revnum_t revision,
const char *path,
apr_pool_t *pool) {
svn_fs_root_t *root, *copy_root;
const char *copy_path, *copy_src_path, *remainder = "";
svn_revnum_t copy_src_rev;
if (appeared_rev)
*appeared_rev = SVN_INVALID_REVNUM;
if (prev_rev)
*prev_rev = SVN_INVALID_REVNUM;
if (prev_path)
*prev_path = NULL;
SVN_ERR(svn_fs_revision_root(&root, fs, revision, pool));
SVN_ERR(svn_fs_closest_copy(&copy_root, &copy_path, root, path, pool));
if (! copy_root)
return SVN_NO_ERROR;
SVN_ERR(svn_fs_copied_from(&copy_src_rev, &copy_src_path,
copy_root, copy_path, pool));
if (! strcmp(copy_path, path) == 0)
remainder = svn_path_is_child(copy_path, path, pool);
if (prev_path)
*prev_path = svn_path_join(copy_src_path, remainder, pool);
if (appeared_rev)
*appeared_rev = svn_fs_revision_root_revision(copy_root);
if (prev_rev)
*prev_rev = copy_src_rev;
return SVN_NO_ERROR;
}
svn_error_t *
svn_repos_trace_node_locations(svn_fs_t *fs,
apr_hash_t **locations,
const char *fs_path,
svn_revnum_t peg_revision,
apr_array_header_t *location_revisions_orig,
svn_repos_authz_func_t authz_read_func,
void *authz_read_baton,
apr_pool_t *pool) {
apr_array_header_t *location_revisions;
svn_revnum_t *revision_ptr, *revision_ptr_end;
svn_fs_root_t *root;
const char *path;
svn_revnum_t revision;
svn_boolean_t is_ancestor;
apr_pool_t *lastpool, *currpool;
const svn_fs_id_t *id;
assert(location_revisions_orig->elt_size == sizeof(svn_revnum_t));
if (*fs_path != '/')
fs_path = apr_pstrcat(pool, "/", fs_path, NULL);
if (authz_read_func) {
svn_fs_root_t *peg_root;
SVN_ERR(svn_fs_revision_root(&peg_root, fs, peg_revision, pool));
SVN_ERR(check_readability(peg_root, fs_path,
authz_read_func, authz_read_baton, pool));
}
*locations = apr_hash_make(pool);
lastpool = svn_pool_create(pool);
currpool = svn_pool_create(pool);
location_revisions = apr_array_copy(pool, location_revisions_orig);
qsort(location_revisions->elts, location_revisions->nelts,
sizeof(*revision_ptr), svn_sort_compare_revisions);
revision_ptr = (svn_revnum_t *)location_revisions->elts;
revision_ptr_end = revision_ptr + location_revisions->nelts;
is_ancestor = FALSE;
while (revision_ptr < revision_ptr_end && *revision_ptr > peg_revision) {
svn_pool_clear(currpool);
SVN_ERR(check_ancestry_of_peg_path(&is_ancestor, fs, fs_path,
peg_revision, *revision_ptr,
currpool));
if (is_ancestor)
break;
++revision_ptr;
}
revision = is_ancestor ? *revision_ptr : peg_revision;
path = fs_path;
if (authz_read_func) {
SVN_ERR(svn_fs_revision_root(&root, fs, revision, pool));
SVN_ERR(check_readability(root, fs_path, authz_read_func,
authz_read_baton, pool));
}
while (revision_ptr < revision_ptr_end) {
apr_pool_t *tmppool;
svn_revnum_t appeared_rev, prev_rev;
const char *prev_path;
SVN_ERR(svn_repos__prev_location(&appeared_rev, &prev_path, &prev_rev,
fs, revision, path, currpool));
if (! prev_path)
break;
if (authz_read_func) {
svn_boolean_t readable;
svn_fs_root_t *tmp_root;
SVN_ERR(svn_fs_revision_root(&tmp_root, fs, revision, currpool));
SVN_ERR(authz_read_func(&readable, tmp_root, path,
authz_read_baton, currpool));
if (! readable) {
return SVN_NO_ERROR;
}
}
while ((revision_ptr < revision_ptr_end)
&& (*revision_ptr >= appeared_rev)) {
apr_hash_set(*locations, revision_ptr, sizeof(*revision_ptr),
apr_pstrdup(pool, path));
revision_ptr++;
}
while ((revision_ptr < revision_ptr_end)
&& (*revision_ptr > prev_rev))
revision_ptr++;
path = prev_path;
revision = prev_rev;
svn_pool_clear(lastpool);
tmppool = lastpool;
lastpool = currpool;
currpool = tmppool;
}
SVN_ERR(svn_fs_revision_root(&root, fs, revision, currpool));
SVN_ERR(svn_fs_node_id(&id, root, path, pool));
while (revision_ptr < revision_ptr_end) {
svn_node_kind_t kind;
const svn_fs_id_t *lrev_id;
svn_pool_clear(currpool);
SVN_ERR(svn_fs_revision_root(&root, fs, *revision_ptr, currpool));
SVN_ERR(svn_fs_check_path(&kind, root, path, currpool));
if (kind == svn_node_none)
break;
SVN_ERR(svn_fs_node_id(&lrev_id, root, path, currpool));
if (! svn_fs_check_related(id, lrev_id))
break;
apr_hash_set(*locations, revision_ptr, sizeof(*revision_ptr),
apr_pstrdup(pool, path));
revision_ptr++;
}
svn_pool_destroy(lastpool);
svn_pool_destroy(currpool);
return SVN_NO_ERROR;
}
static svn_error_t *
maybe_crop_and_send_segment(svn_location_segment_t *segment,
svn_revnum_t start_rev,
svn_revnum_t end_rev,
svn_location_segment_receiver_t receiver,
void *receiver_baton,
apr_pool_t *pool) {
if (! ((segment->range_start > start_rev)
|| (segment->range_end < end_rev))) {
if (segment->range_start < end_rev)
segment->range_start = end_rev;
if (segment->range_end > start_rev)
segment->range_end = start_rev;
SVN_ERR(receiver(segment, receiver_baton, pool));
}
return SVN_NO_ERROR;
}
svn_error_t *
svn_repos_node_location_segments(svn_repos_t *repos,
const char *path,
svn_revnum_t peg_revision,
svn_revnum_t start_rev,
svn_revnum_t end_rev,
svn_location_segment_receiver_t receiver,
void *receiver_baton,
svn_repos_authz_func_t authz_read_func,
void *authz_read_baton,
apr_pool_t *pool) {
svn_fs_t *fs = svn_repos_fs(repos);
svn_stringbuf_t *current_path;
svn_revnum_t youngest_rev = SVN_INVALID_REVNUM, current_rev;
apr_pool_t *subpool;
if (! SVN_IS_VALID_REVNUM(peg_revision)) {
SVN_ERR(svn_fs_youngest_rev(&youngest_rev, fs, pool));
peg_revision = youngest_rev;
}
if (! SVN_IS_VALID_REVNUM(start_rev)) {
if (SVN_IS_VALID_REVNUM(youngest_rev))
start_rev = youngest_rev;
else
SVN_ERR(svn_fs_youngest_rev(&start_rev, fs, pool));
}
end_rev = SVN_IS_VALID_REVNUM(end_rev) ? end_rev : 0;
assert(end_rev <= start_rev);
assert(start_rev <= peg_revision);
if (*path != '/')
path = apr_pstrcat(pool, "/", path, NULL);
if (authz_read_func) {
svn_fs_root_t *peg_root;
SVN_ERR(svn_fs_revision_root(&peg_root, fs, peg_revision, pool));
SVN_ERR(check_readability(peg_root, path,
authz_read_func, authz_read_baton, pool));
}
subpool = svn_pool_create(pool);
current_rev = peg_revision;
current_path = svn_stringbuf_create(path, pool);
while (current_rev >= end_rev) {
svn_revnum_t appeared_rev, prev_rev;
const char *cur_path, *prev_path;
svn_location_segment_t *segment;
svn_pool_clear(subpool);
cur_path = apr_pstrmemdup(subpool, current_path->data,
current_path->len);
segment = apr_pcalloc(subpool, sizeof(*segment));
segment->range_end = current_rev;
segment->range_start = end_rev;
segment->path = cur_path + 1;
SVN_ERR(svn_repos__prev_location(&appeared_rev, &prev_path, &prev_rev,
fs, current_rev, cur_path, subpool));
if (! prev_path) {
svn_fs_root_t *revroot;
SVN_ERR(svn_fs_revision_root(&revroot, fs, current_rev, subpool));
SVN_ERR(svn_fs_node_origin_rev(&(segment->range_start), revroot,
cur_path, subpool));
if (segment->range_start < end_rev)
segment->range_start = end_rev;
current_rev = SVN_INVALID_REVNUM;
} else {
segment->range_start = appeared_rev;
svn_stringbuf_set(current_path, prev_path);
current_rev = prev_rev;
}
if (authz_read_func) {
svn_boolean_t readable;
svn_fs_root_t *cur_rev_root;
SVN_ERR(svn_fs_revision_root(&cur_rev_root, fs,
segment->range_end, subpool));
SVN_ERR(authz_read_func(&readable, cur_rev_root, segment->path,
authz_read_baton, subpool));
if (! readable)
return SVN_NO_ERROR;
}
SVN_ERR(maybe_crop_and_send_segment(segment, start_rev, end_rev,
receiver, receiver_baton, subpool));
if (! SVN_IS_VALID_REVNUM(current_rev))
break;
if (segment->range_start - current_rev > 1) {
svn_location_segment_t *gap_segment;
gap_segment = apr_pcalloc(subpool, sizeof(*gap_segment));
gap_segment->range_end = segment->range_start - 1;
gap_segment->range_start = current_rev + 1;
gap_segment->path = NULL;
SVN_ERR(maybe_crop_and_send_segment(gap_segment, start_rev, end_rev,
receiver, receiver_baton,
subpool));
}
}
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
static svn_error_t *
get_path_mergeinfo(apr_hash_t **mergeinfo,
svn_fs_t *fs,
const char *path,
svn_revnum_t revnum,
apr_pool_t *pool) {
svn_mergeinfo_catalog_t tmp_catalog;
svn_fs_root_t *root;
apr_pool_t *subpool = svn_pool_create(pool);
apr_array_header_t *paths = apr_array_make(subpool, 1,
sizeof(const char *));
APR_ARRAY_PUSH(paths, const char *) = path;
SVN_ERR(svn_fs_revision_root(&root, fs, revnum, subpool));
SVN_ERR(svn_fs_get_mergeinfo(&tmp_catalog, root, paths,
svn_mergeinfo_inherited, FALSE, subpool));
*mergeinfo = apr_hash_get(tmp_catalog, path, APR_HASH_KEY_STRING);
if (*mergeinfo)
*mergeinfo = svn_mergeinfo_dup(*mergeinfo, pool);
else
*mergeinfo = apr_hash_make(pool);
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
static APR_INLINE svn_boolean_t
is_path_in_hash(apr_hash_t *duplicate_path_revs,
const char *path,
svn_revnum_t revision,
apr_pool_t *pool) {
const char *key = apr_psprintf(pool, "%s:%ld", path, revision);
void *ptr;
ptr = apr_hash_get(duplicate_path_revs, key, APR_HASH_KEY_STRING);
return ptr != NULL;
}
struct path_revision {
svn_revnum_t revnum;
const char *path;
apr_hash_t *merged_mergeinfo;
svn_boolean_t merged;
};
static svn_error_t *
get_merged_mergeinfo(apr_hash_t **merged_mergeinfo,
svn_repos_t *repos,
struct path_revision *old_path_rev,
apr_pool_t *pool) {
apr_pool_t *subpool = svn_pool_create(pool);
apr_hash_t *curr_mergeinfo, *prev_mergeinfo, *deleted, *changed;
svn_error_t *err;
SVN_ERR(get_path_mergeinfo(&curr_mergeinfo, repos->fs, old_path_rev->path,
old_path_rev->revnum, subpool));
err = get_path_mergeinfo(&prev_mergeinfo, repos->fs, old_path_rev->path,
old_path_rev->revnum - 1, subpool);
if (err && err->apr_err == SVN_ERR_FS_NOT_FOUND) {
svn_error_clear(err);
prev_mergeinfo = apr_hash_make(subpool);
} else
SVN_ERR(err);
SVN_ERR(svn_mergeinfo_diff(&deleted, &changed, prev_mergeinfo, curr_mergeinfo,
FALSE, subpool));
SVN_ERR(svn_mergeinfo_merge(changed, deleted, subpool));
*merged_mergeinfo = svn_mergeinfo_dup(changed, pool);
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
static svn_error_t *
find_interesting_revisions(apr_array_header_t *path_revisions,
svn_repos_t *repos,
const char *path,
svn_revnum_t start,
svn_revnum_t end,
svn_boolean_t include_merged_revisions,
svn_boolean_t mark_as_merged,
apr_hash_t *duplicate_path_revs,
svn_repos_authz_func_t authz_read_func,
void *authz_read_baton,
apr_pool_t *pool) {
apr_pool_t *iter_pool, *last_pool;
svn_fs_history_t *history;
svn_fs_root_t *root;
svn_node_kind_t kind;
iter_pool = svn_pool_create(pool);
last_pool = svn_pool_create(pool);
SVN_ERR(svn_fs_revision_root(&root, repos->fs, end, last_pool));
SVN_ERR(svn_fs_check_path(&kind, root, path, pool));
if (kind != svn_node_file)
return svn_error_createf
(SVN_ERR_FS_NOT_FILE, NULL, _("'%s' is not a file in revision %ld"),
path, end);
SVN_ERR(svn_fs_node_history(&history, root, path, last_pool));
while (1) {
struct path_revision *path_rev = apr_palloc(pool, sizeof(*path_rev));
apr_pool_t *tmp_pool;
svn_pool_clear(iter_pool);
SVN_ERR(svn_fs_history_prev(&history, history, TRUE, iter_pool));
if (!history)
break;
SVN_ERR(svn_fs_history_location(&path_rev->path, &path_rev->revnum,
history, iter_pool));
if (include_merged_revisions
&& is_path_in_hash(duplicate_path_revs, path_rev->path,
path_rev->revnum, iter_pool))
break;
if (authz_read_func) {
svn_boolean_t readable;
svn_fs_root_t *tmp_root;
SVN_ERR(svn_fs_revision_root(&tmp_root, repos->fs, path_rev->revnum,
iter_pool));
SVN_ERR(authz_read_func(&readable, tmp_root, path_rev->path,
authz_read_baton, iter_pool));
if (! readable)
break;
}
path_rev->path = apr_pstrdup(pool, path_rev->path);
path_rev->merged = mark_as_merged;
APR_ARRAY_PUSH(path_revisions, struct path_revision *) = path_rev;
if (include_merged_revisions)
SVN_ERR(get_merged_mergeinfo(&path_rev->merged_mergeinfo, repos,
path_rev, pool));
else
path_rev->merged_mergeinfo = NULL;
apr_hash_set(duplicate_path_revs,
apr_psprintf(pool, "%s:%ld", path_rev->path,
path_rev->revnum),
APR_HASH_KEY_STRING, (void *)0xdeadbeef);
if (path_rev->revnum <= start)
break;
tmp_pool = iter_pool;
iter_pool = last_pool;
last_pool = tmp_pool;
}
svn_pool_destroy(iter_pool);
return SVN_NO_ERROR;
}
static int
compare_path_revisions(const void *a, const void *b) {
struct path_revision *a_pr = *(struct path_revision **)a;
struct path_revision *b_pr = *(struct path_revision **)b;
if (a_pr->revnum == b_pr->revnum)
return 0;
return a_pr->revnum < b_pr->revnum ? 1 : -1;
}
static svn_error_t *
find_merged_revisions(apr_array_header_t **merged_path_revisions_out,
apr_array_header_t *mainline_path_revisions,
svn_repos_t *repos,
apr_hash_t *duplicate_path_revs,
svn_repos_authz_func_t authz_read_func,
void *authz_read_baton,
apr_pool_t *pool) {
apr_array_header_t *old, *new;
apr_pool_t *iter_pool, *last_pool;
apr_array_header_t *merged_path_revisions = apr_array_make(pool, 0,
sizeof(struct path_revision *));
old = mainline_path_revisions;
iter_pool = svn_pool_create(pool);
last_pool = svn_pool_create(pool);
do {
int i;
apr_pool_t *temp_pool;
svn_pool_clear(iter_pool);
new = apr_array_make(iter_pool, 0, sizeof(struct path_revision *));
for (i = 0; i < old->nelts; i++) {
apr_hash_index_t *hi;
struct path_revision *old_pr = APR_ARRAY_IDX(old, i,
struct path_revision *);
if (!old_pr->merged_mergeinfo)
continue;
for (hi = apr_hash_first(iter_pool, old_pr->merged_mergeinfo); hi;
hi = apr_hash_next(hi)) {
apr_array_header_t *rangelist;
const char *path;
int j;
apr_hash_this(hi, (void *) &path, NULL, (void *) &rangelist);
for (j = 0; j < rangelist->nelts; j++) {
svn_merge_range_t *range = APR_ARRAY_IDX(rangelist, j,
svn_merge_range_t *);
svn_node_kind_t kind;
svn_fs_root_t *root;
SVN_ERR(svn_fs_revision_root(&root, repos->fs, range->end,
iter_pool));
SVN_ERR(svn_fs_check_path(&kind, root, path, iter_pool));
if (kind != svn_node_file)
continue;
SVN_ERR(find_interesting_revisions(new, repos, path,
range->start, range->end,
TRUE, TRUE,
duplicate_path_revs,
authz_read_func,
authz_read_baton, pool));
}
}
}
merged_path_revisions = apr_array_append(iter_pool, merged_path_revisions,
new);
old = new;
temp_pool = last_pool;
last_pool = iter_pool;
iter_pool = temp_pool;
} while (new->nelts > 0);
qsort(merged_path_revisions->elts, merged_path_revisions->nelts,
sizeof(struct path_revision *), compare_path_revisions);
*merged_path_revisions_out = apr_array_copy(pool, merged_path_revisions);
svn_pool_destroy(iter_pool);
svn_pool_destroy(last_pool);
return SVN_NO_ERROR;
}
struct send_baton {
apr_pool_t *iter_pool;
apr_pool_t *last_pool;
apr_hash_t *last_props;
const char *last_path;
svn_fs_root_t *last_root;
};
static svn_error_t *
send_path_revision(struct path_revision *path_rev,
svn_repos_t *repos,
struct send_baton *sb,
svn_file_rev_handler_t handler,
void *handler_baton) {
apr_hash_t *rev_props;
apr_hash_t *props;
apr_array_header_t *prop_diffs;
svn_fs_root_t *root;
svn_txdelta_stream_t *delta_stream;
svn_txdelta_window_handler_t delta_handler = NULL;
void *delta_baton = NULL;
apr_pool_t *tmp_pool;
svn_boolean_t contents_changed;
svn_pool_clear(sb->iter_pool);
SVN_ERR(svn_fs_revision_proplist(&rev_props, repos->fs,
path_rev->revnum, sb->iter_pool));
SVN_ERR(svn_fs_revision_root(&root, repos->fs, path_rev->revnum,
sb->iter_pool));
SVN_ERR(svn_fs_node_proplist(&props, root, path_rev->path,
sb->iter_pool));
SVN_ERR(svn_prop_diffs(&prop_diffs, props, sb->last_props,
sb->iter_pool));
if (sb->last_root)
SVN_ERR(svn_fs_contents_changed(&contents_changed, sb->last_root,
sb->last_path, root, path_rev->path,
sb->iter_pool));
else
contents_changed = TRUE;
SVN_ERR(handler(handler_baton, path_rev->path, path_rev->revnum,
rev_props, path_rev->merged,
contents_changed ? &delta_handler : NULL,
contents_changed ? &delta_baton : NULL,
prop_diffs, sb->iter_pool));
if (delta_handler) {
SVN_ERR(svn_fs_get_file_delta_stream(&delta_stream,
sb->last_root, sb->last_path,
root, path_rev->path,
sb->iter_pool));
SVN_ERR(svn_txdelta_send_txstream(delta_stream,
delta_handler, delta_baton,
sb->iter_pool));
}
sb->last_root = root;
sb->last_path = path_rev->path;
sb->last_props = props;
tmp_pool = sb->iter_pool;
sb->iter_pool = sb->last_pool;
sb->last_pool = tmp_pool;
return SVN_NO_ERROR;
}
svn_error_t *
svn_repos_get_file_revs2(svn_repos_t *repos,
const char *path,
svn_revnum_t start,
svn_revnum_t end,
svn_boolean_t include_merged_revisions,
svn_repos_authz_func_t authz_read_func,
void *authz_read_baton,
svn_file_rev_handler_t handler,
void *handler_baton,
apr_pool_t *pool) {
apr_array_header_t *mainline_path_revisions, *merged_path_revisions;
apr_hash_t *duplicate_path_revs;
struct send_baton sb;
int mainline_pos, merged_pos;
duplicate_path_revs = apr_hash_make(pool);
mainline_path_revisions = apr_array_make(pool, 0,
sizeof(struct path_revision *));
SVN_ERR(find_interesting_revisions(mainline_path_revisions, repos, path,
start, end, include_merged_revisions,
FALSE, duplicate_path_revs,
authz_read_func, authz_read_baton, pool));
if (include_merged_revisions)
SVN_ERR(find_merged_revisions(&merged_path_revisions,
mainline_path_revisions, repos,
duplicate_path_revs, authz_read_func,
authz_read_baton, pool));
else
merged_path_revisions = apr_array_make(pool, 0,
sizeof(struct path_revision *));
assert(mainline_path_revisions->nelts > 0);
sb.iter_pool = svn_pool_create(pool);
sb.last_pool = svn_pool_create(pool);
sb.last_root = NULL;
sb.last_path = NULL;
sb.last_props = apr_hash_make(sb.last_pool);
mainline_pos = mainline_path_revisions->nelts - 1;
merged_pos = merged_path_revisions->nelts - 1;
while (mainline_pos >= 0 && merged_pos >= 0) {
struct path_revision *main_pr = APR_ARRAY_IDX(mainline_path_revisions,
mainline_pos,
struct path_revision *);
struct path_revision *merged_pr = APR_ARRAY_IDX(merged_path_revisions,
merged_pos,
struct path_revision *);
if (main_pr->revnum <= merged_pr->revnum) {
SVN_ERR(send_path_revision(main_pr, repos, &sb, handler,
handler_baton));
mainline_pos -= 1;
} else {
SVN_ERR(send_path_revision(merged_pr, repos, &sb, handler,
handler_baton));
merged_pos -= 1;
}
}
for (; mainline_pos >= 0; mainline_pos -= 1) {
struct path_revision *main_pr = APR_ARRAY_IDX(mainline_path_revisions,
mainline_pos,
struct path_revision *);
SVN_ERR(send_path_revision(main_pr, repos, &sb, handler, handler_baton));
}
for (; merged_pos >= 0; merged_pos -= 1) {
struct path_revision *merged_pr = APR_ARRAY_IDX(merged_path_revisions,
merged_pos,
struct path_revision *);
SVN_ERR(send_path_revision(merged_pr, repos, &sb, handler,
handler_baton));
}
svn_pool_destroy(sb.last_pool);
svn_pool_destroy(sb.iter_pool);
return SVN_NO_ERROR;
}
svn_error_t *
svn_repos_get_file_revs(svn_repos_t *repos,
const char *path,
svn_revnum_t start,
svn_revnum_t end,
svn_repos_authz_func_t authz_read_func,
void *authz_read_baton,
svn_repos_file_rev_handler_t handler,
void *handler_baton,
apr_pool_t *pool) {
svn_file_rev_handler_t handler2;
void *handler2_baton;
svn_compat_wrap_file_rev_handler(&handler2, &handler2_baton, handler,
handler_baton, pool);
return svn_repos_get_file_revs2(repos, path, start, end, FALSE,
authz_read_func, authz_read_baton,
handler2, handler2_baton, pool);
}