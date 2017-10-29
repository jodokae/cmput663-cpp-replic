#include <string.h>
#include <assert.h>
#include "svn_client.h"
#include "svn_error.h"
#include "svn_error_codes.h"
#include "svn_path.h"
#include "svn_opt.h"
#include "svn_time.h"
#include "svn_props.h"
#include "svn_mergeinfo.h"
#include "svn_pools.h"
#include "client.h"
#include "mergeinfo.h"
#include "svn_private_config.h"
#include "private/svn_wc_private.h"
#include "private/svn_mergeinfo_private.h"
static svn_error_t *
calculate_target_mergeinfo(svn_ra_session_t *ra_session,
apr_hash_t **target_mergeinfo,
svn_wc_adm_access_t *adm_access,
const char *src_path_or_url,
svn_revnum_t src_revnum,
svn_boolean_t no_repos_access,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
const svn_wc_entry_t *entry = NULL;
svn_boolean_t locally_added = FALSE;
const char *src_url;
apr_hash_t *src_mergeinfo = NULL;
if (adm_access) {
SVN_ERR(svn_wc__entry_versioned(&entry, src_path_or_url, adm_access,
FALSE, pool));
if (entry->schedule == svn_wc_schedule_add && (! entry->copied)) {
locally_added = TRUE;
} else {
SVN_ERR(svn_client__entry_location(&src_url, &src_revnum,
src_path_or_url,
svn_opt_revision_working, entry,
pool));
}
} else {
src_url = src_path_or_url;
}
if (! locally_added) {
const char *mergeinfo_path;
if (! no_repos_access) {
SVN_ERR(svn_client__path_relative_to_root(&mergeinfo_path, src_url,
entry ? entry->repos : NULL,
FALSE, ra_session,
adm_access, pool));
SVN_ERR(svn_client__get_repos_mergeinfo(ra_session, &src_mergeinfo,
mergeinfo_path, src_revnum,
svn_mergeinfo_inherited, TRUE,
pool));
} else {
svn_boolean_t inherited;
SVN_ERR(svn_client__get_wc_mergeinfo(&src_mergeinfo, &inherited,
FALSE, svn_mergeinfo_inherited,
entry, src_path_or_url, NULL,
NULL, adm_access, ctx, pool));
}
}
*target_mergeinfo = src_mergeinfo;
return SVN_NO_ERROR;
}
static svn_error_t *
extend_wc_mergeinfo(const char *target_wcpath, const svn_wc_entry_t *entry,
apr_hash_t *mergeinfo, svn_wc_adm_access_t *adm_access,
svn_client_ctx_t *ctx, apr_pool_t *pool) {
apr_hash_t *wc_mergeinfo;
SVN_ERR(svn_client__parse_mergeinfo(&wc_mergeinfo, entry, target_wcpath,
FALSE, adm_access, ctx, pool));
if (wc_mergeinfo && mergeinfo)
SVN_ERR(svn_mergeinfo_merge(wc_mergeinfo, mergeinfo, pool));
else if (! wc_mergeinfo)
wc_mergeinfo = mergeinfo;
return svn_client__record_wc_mergeinfo(target_wcpath, wc_mergeinfo,
adm_access, pool);
}
static svn_error_t *
propagate_mergeinfo_within_wc(svn_client__copy_pair_t *pair,
svn_wc_adm_access_t *src_access,
svn_wc_adm_access_t *dst_access,
svn_client_ctx_t *ctx, apr_pool_t *pool) {
apr_hash_t *mergeinfo;
const svn_wc_entry_t *entry;
SVN_ERR(svn_wc__entry_versioned(&entry, pair->src, src_access, FALSE, pool));
if (entry->schedule == svn_wc_schedule_normal
|| (entry->schedule == svn_wc_schedule_add && entry->copied)) {
pair->src_revnum = entry->revision;
SVN_ERR(calculate_target_mergeinfo(NULL, &mergeinfo,
src_access, pair->src,
pair->src_revnum, TRUE,
ctx, pool));
if (! mergeinfo)
mergeinfo = apr_hash_make(pool);
SVN_ERR(svn_wc__entry_versioned(&entry, pair->dst, dst_access, FALSE,
pool));
return extend_wc_mergeinfo(pair->dst, entry, mergeinfo, dst_access,
ctx, pool);
}
SVN_ERR(svn_client__parse_mergeinfo(&mergeinfo, entry, pair->src, FALSE,
src_access, ctx, pool));
if (mergeinfo == NULL) {
mergeinfo = apr_hash_make(pool);
return svn_client__record_wc_mergeinfo(pair->dst, mergeinfo, dst_access,
pool);
} else
return SVN_NO_ERROR;
}
static svn_error_t *
get_copy_pair_ancestors(const apr_array_header_t *copy_pairs,
const char **src_ancestor,
const char **dst_ancestor,
const char **common_ancestor,
apr_pool_t *pool) {
apr_pool_t *subpool = svn_pool_create(pool);
const char *top_dst;
char *top_src;
int i;
top_src = apr_pstrdup(subpool, APR_ARRAY_IDX(copy_pairs, 0,
svn_client__copy_pair_t *)->src);
if (copy_pairs->nelts == 1)
top_dst = apr_pstrdup(subpool, APR_ARRAY_IDX(copy_pairs, 0,
svn_client__copy_pair_t *)->dst);
else
top_dst = svn_path_dirname(APR_ARRAY_IDX(copy_pairs, 0,
svn_client__copy_pair_t *)->dst,
subpool);
for (i = 1; i < copy_pairs->nelts; i++) {
const svn_client__copy_pair_t *pair = APR_ARRAY_IDX(copy_pairs, i,
svn_client__copy_pair_t *);
top_src = svn_path_get_longest_ancestor(top_src, pair->src, subpool);
}
if (src_ancestor)
*src_ancestor = apr_pstrdup(pool, top_src);
if (dst_ancestor)
*dst_ancestor = apr_pstrdup(pool, top_dst);
if (common_ancestor)
*common_ancestor = svn_path_get_longest_ancestor(top_src, top_dst, pool);
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
static svn_error_t *
do_wc_to_wc_copies(const apr_array_header_t *copy_pairs,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
int i;
apr_pool_t *iterpool = svn_pool_create(pool);
const char *dst_parent;
svn_wc_adm_access_t *dst_access;
svn_error_t *err = SVN_NO_ERROR;
get_copy_pair_ancestors(copy_pairs, NULL, &dst_parent, NULL, pool);
if (copy_pairs->nelts == 1)
dst_parent = svn_path_dirname(dst_parent, pool);
SVN_ERR(svn_wc_adm_open3(&dst_access, NULL, dst_parent, TRUE, 0,
ctx->cancel_func, ctx->cancel_baton, pool));
for (i = 0; i < copy_pairs->nelts; i++) {
svn_wc_adm_access_t *src_access;
const char *src_parent;
svn_client__copy_pair_t *pair = APR_ARRAY_IDX(copy_pairs, i,
svn_client__copy_pair_t *);
svn_pool_clear(iterpool);
if (ctx->cancel_func)
SVN_ERR(ctx->cancel_func(ctx->cancel_baton));
svn_path_split(pair->src, &src_parent, NULL, pool);
if (strcmp(src_parent, pair->dst_parent) == 0) {
if (pair->src_kind == svn_node_dir)
SVN_ERR(svn_wc_adm_open3(&src_access, NULL, pair->src, FALSE,
-1, ctx->cancel_func, ctx->cancel_baton,
iterpool));
else
src_access = dst_access;
} else {
err = svn_wc_adm_open3(&src_access, NULL, src_parent, FALSE,
pair->src_kind == svn_node_dir ? -1 : 0,
ctx->cancel_func, ctx->cancel_baton,
iterpool);
if (err && err->apr_err == SVN_ERR_WC_NOT_DIRECTORY) {
src_access = NULL;
svn_error_clear(err);
err = NULL;
}
SVN_ERR(err);
}
err = svn_wc_copy2(pair->src, dst_access, pair->base_name,
ctx->cancel_func, ctx->cancel_baton,
ctx->notify_func2, ctx->notify_baton2, iterpool);
if (err)
break;
if (src_access) {
err = propagate_mergeinfo_within_wc(pair, src_access, dst_access,
ctx, iterpool);
if (err)
break;
if (src_access != dst_access)
SVN_ERR(svn_wc_adm_close(src_access));
}
}
svn_sleep_for_timestamps();
SVN_ERR(err);
SVN_ERR(svn_wc_adm_close(dst_access));
svn_pool_destroy(iterpool);
return SVN_NO_ERROR;
}
static svn_error_t *
do_wc_to_wc_moves(const apr_array_header_t *copy_pairs,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
int i;
apr_pool_t *iterpool = svn_pool_create(pool);
svn_error_t *err = SVN_NO_ERROR;
for (i = 0; i < copy_pairs->nelts; i++) {
svn_wc_adm_access_t *src_access, *dst_access;
const char *src_parent;
svn_client__copy_pair_t *pair = APR_ARRAY_IDX(copy_pairs, i,
svn_client__copy_pair_t *);
svn_pool_clear(iterpool);
if (ctx->cancel_func)
SVN_ERR(ctx->cancel_func(ctx->cancel_baton));
svn_path_split(pair->src, &src_parent, NULL, iterpool);
SVN_ERR(svn_wc_adm_open3(&src_access, NULL, src_parent, TRUE,
pair->src_kind == svn_node_dir ? -1 : 0,
ctx->cancel_func, ctx->cancel_baton,
iterpool));
if (strcmp(src_parent, pair->dst_parent) == 0) {
dst_access = src_access;
} else {
const char *src_parent_abs, *dst_parent_abs;
SVN_ERR(svn_path_get_absolute(&src_parent_abs, src_parent,
iterpool));
SVN_ERR(svn_path_get_absolute(&dst_parent_abs, pair->dst_parent,
iterpool));
if ((pair->src_kind == svn_node_dir)
&& (svn_path_is_child(src_parent_abs, dst_parent_abs,
iterpool))) {
SVN_ERR(svn_wc_adm_retrieve(&dst_access, src_access,
pair->dst_parent, iterpool));
} else {
SVN_ERR(svn_wc_adm_open3(&dst_access, NULL, pair->dst_parent,
TRUE, 0, ctx->cancel_func,
ctx->cancel_baton,
iterpool));
}
}
err = svn_wc_copy2(pair->src, dst_access, pair->base_name,
ctx->cancel_func, ctx->cancel_baton,
ctx->notify_func2, ctx->notify_baton2, iterpool);
if (err)
break;
err = propagate_mergeinfo_within_wc(pair, src_access, dst_access,
ctx, iterpool);
if (err)
break;
SVN_ERR(svn_wc_delete3(pair->src, src_access,
ctx->cancel_func, ctx->cancel_baton,
ctx->notify_func2, ctx->notify_baton2, FALSE,
iterpool));
if (dst_access != src_access)
SVN_ERR(svn_wc_adm_close(dst_access));
SVN_ERR(svn_wc_adm_close(src_access));
}
svn_sleep_for_timestamps();
SVN_ERR(err);
svn_pool_destroy(iterpool);
return SVN_NO_ERROR;
}
static svn_error_t *
wc_to_wc_copy(const apr_array_header_t *copy_pairs,
svn_boolean_t is_move,
svn_boolean_t make_parents,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
int i;
apr_pool_t *iterpool = svn_pool_create(pool);
for (i = 0; i < copy_pairs->nelts; i++) {
svn_client__copy_pair_t *pair = APR_ARRAY_IDX(copy_pairs, i,
svn_client__copy_pair_t *);
svn_node_kind_t dst_kind, dst_parent_kind;
svn_pool_clear(iterpool);
SVN_ERR(svn_io_check_path(pair->src, &pair->src_kind, iterpool));
if (pair->src_kind == svn_node_none)
return svn_error_createf(SVN_ERR_NODE_UNKNOWN_KIND, NULL,
_("Path '%s' does not exist"),
svn_path_local_style(pair->src, pool));
SVN_ERR(svn_io_check_path(pair->dst, &dst_kind, iterpool));
if (dst_kind != svn_node_none)
return svn_error_createf(SVN_ERR_ENTRY_EXISTS, NULL,
_("Path '%s' already exists"),
svn_path_local_style(pair->dst, pool));
svn_path_split(pair->dst, &pair->dst_parent, &pair->base_name, pool);
SVN_ERR(svn_io_check_path(pair->dst_parent, &dst_parent_kind, iterpool));
if (make_parents && dst_parent_kind == svn_node_none) {
SVN_ERR(svn_client__make_local_parents(pair->dst_parent, TRUE, ctx,
iterpool));
} else if (dst_parent_kind != svn_node_dir) {
return svn_error_createf(SVN_ERR_WC_NOT_DIRECTORY, NULL,
_("Path '%s' is not a directory"),
svn_path_local_style(pair->dst_parent,
pool));
}
}
svn_pool_destroy(iterpool);
if (is_move)
return do_wc_to_wc_moves(copy_pairs, ctx, pool);
else
return do_wc_to_wc_copies(copy_pairs, ctx, pool);
}
typedef struct {
const char *src_url;
const char *src_path;
const char *dst_path;
svn_node_kind_t src_kind;
svn_revnum_t src_revnum;
svn_boolean_t resurrection;
svn_boolean_t dir_add;
svn_string_t *mergeinfo;
} path_driver_info_t;
struct path_driver_cb_baton {
const svn_delta_editor_t *editor;
void *edit_baton;
apr_hash_t *action_hash;
svn_boolean_t is_move;
};
static svn_error_t *
path_driver_cb_func(void **dir_baton,
void *parent_baton,
void *callback_baton,
const char *path,
apr_pool_t *pool) {
struct path_driver_cb_baton *cb_baton = callback_baton;
svn_boolean_t do_delete = FALSE, do_add = FALSE;
path_driver_info_t *path_info = apr_hash_get(cb_baton->action_hash,
path,
APR_HASH_KEY_STRING);
*dir_baton = NULL;
assert(! svn_path_is_empty(path));
if (path_info->dir_add) {
SVN_ERR(cb_baton->editor->add_directory(path, parent_baton, NULL,
SVN_INVALID_REVNUM, pool,
dir_baton));
return SVN_NO_ERROR;
}
if (path_info->resurrection) {
if (! cb_baton->is_move)
do_add = TRUE;
}
else {
if (cb_baton->is_move) {
if (strcmp(path_info->src_path, path) == 0)
do_delete = TRUE;
else
do_add = TRUE;
}
else {
do_add = TRUE;
}
}
if (do_delete) {
SVN_ERR(cb_baton->editor->delete_entry(path, SVN_INVALID_REVNUM,
parent_baton, pool));
}
if (do_add) {
SVN_ERR(svn_path_check_valid(path, pool));
if (path_info->src_kind == svn_node_file) {
void *file_baton;
SVN_ERR(cb_baton->editor->add_file(path, parent_baton,
path_info->src_url,
path_info->src_revnum,
pool, &file_baton));
if (path_info->mergeinfo)
SVN_ERR(cb_baton->editor->change_file_prop(file_baton,
SVN_PROP_MERGEINFO,
path_info->mergeinfo,
pool));
SVN_ERR(cb_baton->editor->close_file(file_baton, NULL, pool));
} else {
SVN_ERR(cb_baton->editor->add_directory(path, parent_baton,
path_info->src_url,
path_info->src_revnum,
pool, dir_baton));
if (path_info->mergeinfo)
SVN_ERR(cb_baton->editor->change_dir_prop(*dir_baton,
SVN_PROP_MERGEINFO,
path_info->mergeinfo,
pool));
}
}
return SVN_NO_ERROR;
}
static svn_error_t *
repos_to_repos_copy(svn_commit_info_t **commit_info_p,
const apr_array_header_t *copy_pairs,
svn_boolean_t make_parents,
const apr_hash_t *revprop_table,
svn_client_ctx_t *ctx,
svn_boolean_t is_move,
apr_pool_t *pool) {
apr_array_header_t *paths = apr_array_make(pool, 2 * copy_pairs->nelts,
sizeof(const char *));
apr_hash_t *action_hash = apr_hash_make(pool);
apr_array_header_t *path_infos;
const char *top_url, *message, *repos_root;
svn_revnum_t youngest;
svn_ra_session_t *ra_session;
const svn_delta_editor_t *editor;
void *edit_baton;
void *commit_baton;
struct path_driver_cb_baton cb_baton;
apr_array_header_t *new_dirs = NULL;
apr_hash_t *commit_revprops;
apr_pool_t *iterpool;
int i;
svn_error_t *err;
path_infos = apr_array_make(pool, copy_pairs->nelts,
sizeof(path_driver_info_t *));
for (i = 0; i < copy_pairs->nelts; i++) {
path_driver_info_t *info = apr_pcalloc(pool, sizeof(*info));
info->resurrection = FALSE;
APR_ARRAY_PUSH(path_infos, path_driver_info_t *) = info;
}
get_copy_pair_ancestors(copy_pairs, NULL, NULL, &top_url, pool);
for (i = 0; i < copy_pairs->nelts; i++) {
svn_client__copy_pair_t *pair = APR_ARRAY_IDX(copy_pairs, i,
svn_client__copy_pair_t *);
path_driver_info_t *info = APR_ARRAY_IDX(path_infos, i,
path_driver_info_t *);
if (strcmp(pair->src, pair->dst) == 0) {
info->resurrection = TRUE;
if (strcmp(pair->src, top_url) == 0) {
top_url = svn_path_dirname(top_url, pool);
}
}
}
err = svn_client__open_ra_session_internal(&ra_session, top_url,
NULL, NULL, NULL, FALSE, TRUE,
ctx, pool);
if (err) {
if ((err->apr_err == SVN_ERR_RA_ILLEGAL_URL)
&& ((top_url == NULL) || (top_url[0] == '\0'))) {
svn_client__copy_pair_t *first_pair =
APR_ARRAY_IDX(copy_pairs, 0, svn_client__copy_pair_t *);
svn_error_clear(err);
return svn_error_createf
(SVN_ERR_UNSUPPORTED_FEATURE, NULL,
_("Source and dest appear not to be in the same repository "
"(src: '%s'; dst: '%s')"),
first_pair->src, first_pair->dst);
} else
return err;
}
iterpool = svn_pool_create(pool);
if (make_parents) {
svn_client__copy_pair_t *pair = APR_ARRAY_IDX(copy_pairs, 0,
svn_client__copy_pair_t *);
svn_node_kind_t kind;
const char *dir;
new_dirs = apr_array_make(pool, 0, sizeof(const char *));
dir = svn_path_is_child(top_url, svn_path_dirname(pair->dst, pool),
pool);
SVN_ERR(svn_ra_check_path(ra_session, dir, SVN_INVALID_REVNUM, &kind,
iterpool));
while (kind == svn_node_none) {
svn_pool_clear(iterpool);
APR_ARRAY_PUSH(new_dirs, const char *) = dir;
svn_path_split(dir, &dir, NULL, pool);
SVN_ERR(svn_ra_check_path(ra_session, dir, SVN_INVALID_REVNUM, &kind,
iterpool));
}
}
svn_pool_destroy(iterpool);
SVN_ERR(svn_ra_get_repos_root2(ra_session, &repos_root, pool));
for (i = 0; i < copy_pairs->nelts; i++) {
svn_client__copy_pair_t *pair = APR_ARRAY_IDX(copy_pairs, i,
svn_client__copy_pair_t *);
path_driver_info_t *info = APR_ARRAY_IDX(path_infos, i,
path_driver_info_t *);
if (strcmp(pair->dst, repos_root) != 0
&& svn_path_is_child(pair->dst, pair->src, pool) != NULL) {
info->resurrection = TRUE;
top_url = svn_path_dirname(top_url, pool);
SVN_ERR(svn_ra_reparent(ra_session, top_url, pool));
}
}
SVN_ERR(svn_ra_get_latest_revnum(ra_session, &youngest, pool));
for (i = 0; i < copy_pairs->nelts; i++) {
svn_client__copy_pair_t *pair = APR_ARRAY_IDX(copy_pairs, i,
svn_client__copy_pair_t *);
path_driver_info_t *info = APR_ARRAY_IDX(path_infos, i,
path_driver_info_t *);
svn_node_kind_t dst_kind;
const char *src_rel, *dst_rel;
svn_opt_revision_t *new_rev, *ignored_rev, dead_end_rev;
const char *ignored_url;
SVN_ERR(svn_client__get_revision_number
(&pair->src_revnum, NULL, ra_session, &pair->src_op_revision,
NULL, pool));
info->src_revnum = pair->src_revnum;
dead_end_rev.kind = svn_opt_revision_unspecified;
SVN_ERR(svn_client__repos_locations(&pair->src, &new_rev,
&ignored_url, &ignored_rev,
NULL,
pair->src, &pair->src_peg_revision,
&pair->src_op_revision, &dead_end_rev,
ctx, pool));
src_rel = svn_path_is_child(top_url, pair->src, pool);
if (src_rel)
src_rel = svn_path_uri_decode(src_rel, pool);
else
src_rel = "";
dst_rel = svn_path_is_child(top_url, pair->dst, pool);
if (dst_rel)
dst_rel = svn_path_uri_decode(dst_rel, pool);
else
dst_rel = "";
if (svn_path_is_empty(src_rel) && is_move)
return svn_error_createf(SVN_ERR_UNSUPPORTED_FEATURE, NULL,
_("Cannot move URL '%s' into itself"),
pair->src);
SVN_ERR(svn_ra_check_path(ra_session, src_rel, pair->src_revnum,
&info->src_kind, pool));
if (info->src_kind == svn_node_none)
return svn_error_createf
(SVN_ERR_FS_NOT_FOUND, NULL,
_("Path '%s' does not exist in revision %ld"),
pair->src, pair->src_revnum);
SVN_ERR(svn_ra_check_path(ra_session, dst_rel, youngest, &dst_kind,
pool));
if (dst_kind != svn_node_none) {
return svn_error_createf(SVN_ERR_FS_ALREADY_EXISTS, NULL,
_("Path '%s' already exists"), dst_rel);
}
info->src_url = pair->src;
info->src_path = src_rel;
info->dst_path = dst_rel;
}
if (SVN_CLIENT__HAS_LOG_MSG_FUNC(ctx)) {
svn_client_commit_item3_t *item;
const char *tmp_file;
apr_array_header_t *commit_items
= apr_array_make(pool, 2 * copy_pairs->nelts, sizeof(item));
if (make_parents) {
for (i = 0; i < new_dirs->nelts; i++) {
const char *url = APR_ARRAY_IDX(new_dirs, i, const char *);
SVN_ERR(svn_client_commit_item_create
((const svn_client_commit_item3_t **) &item, pool));
item->url = svn_path_join(top_url, url, pool);
item->state_flags = SVN_CLIENT_COMMIT_ITEM_ADD;
APR_ARRAY_PUSH(commit_items, svn_client_commit_item3_t *) = item;
}
}
for (i = 0; i < path_infos->nelts; i++) {
path_driver_info_t *info = APR_ARRAY_IDX(path_infos, i,
path_driver_info_t *);
SVN_ERR(svn_client_commit_item_create
((const svn_client_commit_item3_t **) &item, pool));
item->url = svn_path_join(top_url, info->dst_path, pool);
item->state_flags = SVN_CLIENT_COMMIT_ITEM_ADD;
APR_ARRAY_PUSH(commit_items, svn_client_commit_item3_t *) = item;
apr_hash_set(action_hash, info->dst_path, APR_HASH_KEY_STRING,
info);
if (is_move && (! info->resurrection)) {
item = apr_pcalloc(pool, sizeof(*item));
item->url = svn_path_join(top_url, info->src_path, pool);
item->state_flags = SVN_CLIENT_COMMIT_ITEM_DELETE;
APR_ARRAY_PUSH(commit_items, svn_client_commit_item3_t *) = item;
apr_hash_set(action_hash, info->src_path, APR_HASH_KEY_STRING,
info);
}
}
SVN_ERR(svn_client__get_log_msg(&message, &tmp_file, commit_items,
ctx, pool));
if (! message)
return SVN_NO_ERROR;
} else
message = "";
if (make_parents) {
for (i = 0; i < new_dirs->nelts; i++) {
const char *url = APR_ARRAY_IDX(new_dirs, i, const char *);
path_driver_info_t *info = apr_pcalloc(pool, sizeof(*info));
info->dst_path = url;
info->dir_add = TRUE;
APR_ARRAY_PUSH(paths, const char *) = url;
apr_hash_set(action_hash, url, APR_HASH_KEY_STRING, info);
}
}
for (i = 0; i < path_infos->nelts; i++) {
path_driver_info_t *info = APR_ARRAY_IDX(path_infos, i,
path_driver_info_t *);
apr_hash_t *mergeinfo;
SVN_ERR(calculate_target_mergeinfo(ra_session, &mergeinfo, NULL,
info->src_url, info->src_revnum,
FALSE, ctx, pool));
if (mergeinfo)
SVN_ERR(svn_mergeinfo_to_string(&info->mergeinfo, mergeinfo, pool));
APR_ARRAY_PUSH(paths, const char *) = info->dst_path;
if (is_move && (! info->resurrection))
APR_ARRAY_PUSH(paths, const char *) = info->src_path;
}
SVN_ERR(svn_client__ensure_revprop_table(&commit_revprops, revprop_table,
message, ctx, pool));
SVN_ERR(svn_client__commit_get_baton(&commit_baton, commit_info_p, pool));
SVN_ERR(svn_ra_get_commit_editor3(ra_session, &editor, &edit_baton,
commit_revprops,
svn_client__commit_callback,
commit_baton,
NULL, TRUE,
pool));
cb_baton.editor = editor;
cb_baton.edit_baton = edit_baton;
cb_baton.action_hash = action_hash;
cb_baton.is_move = is_move;
err = svn_delta_path_driver(editor, edit_baton, youngest, paths,
path_driver_cb_func, &cb_baton, pool);
if (err) {
svn_error_clear(editor->abort_edit(edit_baton, pool));
return err;
}
SVN_ERR(editor->close_edit(edit_baton, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
wc_to_repos_copy(svn_commit_info_t **commit_info_p,
const apr_array_header_t *copy_pairs,
svn_boolean_t make_parents,
const apr_hash_t *revprop_table,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
const char *message;
const char *top_src_path, *top_dst_url, *repos_root;
svn_ra_session_t *ra_session;
const svn_delta_editor_t *editor;
void *edit_baton;
svn_node_kind_t base_kind;
void *commit_baton;
apr_hash_t *committables;
svn_wc_adm_access_t *adm_access, *dir_access;
apr_array_header_t *commit_items;
const svn_wc_entry_t *entry;
apr_pool_t *iterpool;
apr_array_header_t *new_dirs = NULL;
apr_hash_t *commit_revprops;
int i;
for (i = 0; i < copy_pairs->nelts; i++) {
svn_client__copy_pair_t *pair = APR_ARRAY_IDX(copy_pairs, i,
svn_client__copy_pair_t *);
SVN_ERR(svn_path_get_absolute(&pair->src_abs, pair->src, pool));
}
get_copy_pair_ancestors(copy_pairs, &top_src_path, NULL, NULL, pool);
SVN_ERR(svn_wc_adm_probe_open3(&adm_access, NULL, top_src_path,
FALSE, -1, ctx->cancel_func,
ctx->cancel_baton, pool));
svn_path_split(APR_ARRAY_IDX(copy_pairs, 0, svn_client__copy_pair_t *)->dst,
&top_dst_url,
NULL, pool);
for (i = 1; i < copy_pairs->nelts; i++) {
svn_client__copy_pair_t *pair = APR_ARRAY_IDX(copy_pairs, i,
svn_client__copy_pair_t *);
top_dst_url = svn_path_get_longest_ancestor(top_dst_url, pair->dst, pool);
}
SVN_ERR(svn_client__open_ra_session_internal(&ra_session, top_dst_url,
svn_wc_adm_access_path
(adm_access),
adm_access, NULL, TRUE, TRUE,
ctx, pool));
if (make_parents) {
const char *root_url = top_dst_url;
svn_node_kind_t kind;
new_dirs = apr_array_make(pool, 0, sizeof(const char *));
SVN_ERR(svn_ra_check_path(ra_session, "", SVN_INVALID_REVNUM, &kind,
pool));
while (kind == svn_node_none) {
APR_ARRAY_PUSH(new_dirs, const char *) = root_url;
svn_path_split(root_url, &root_url, NULL, pool);
SVN_ERR(svn_ra_reparent(ra_session, root_url, pool));
SVN_ERR(svn_ra_check_path(ra_session, "", SVN_INVALID_REVNUM, &kind,
pool));
}
top_dst_url = root_url;
}
iterpool = svn_pool_create(pool);
for (i = 0; i < copy_pairs->nelts; i++) {
svn_node_kind_t dst_kind;
const char *dst_rel;
svn_client__copy_pair_t *pair =
APR_ARRAY_IDX(copy_pairs, i, svn_client__copy_pair_t *);
svn_pool_clear(iterpool);
SVN_ERR(svn_wc_entry(&entry, pair->src, adm_access, FALSE, iterpool));
pair->src_revnum = entry->revision;
dst_rel = svn_path_uri_decode(svn_path_is_child(top_dst_url,
pair->dst,
iterpool),
iterpool);
SVN_ERR(svn_ra_check_path(ra_session, dst_rel, SVN_INVALID_REVNUM,
&dst_kind, iterpool));
if (dst_kind != svn_node_none) {
return svn_error_createf(SVN_ERR_FS_ALREADY_EXISTS, NULL,
_("Path '%s' already exists"), pair->dst);
}
}
svn_pool_destroy(iterpool);
if (SVN_CLIENT__HAS_LOG_MSG_FUNC(ctx)) {
svn_client_commit_item3_t *item;
const char *tmp_file;
commit_items = apr_array_make(pool, copy_pairs->nelts, sizeof(item));
if (make_parents) {
for (i = 0; i < new_dirs->nelts; i++) {
const char *url = APR_ARRAY_IDX(new_dirs, i, const char *);
SVN_ERR(svn_client_commit_item_create
((const svn_client_commit_item3_t **) &item, pool));
item->url = url;
item->state_flags = SVN_CLIENT_COMMIT_ITEM_ADD;
APR_ARRAY_PUSH(commit_items, svn_client_commit_item3_t *) = item;
}
}
for (i = 0; i < copy_pairs->nelts; i++ ) {
svn_client__copy_pair_t *pair = APR_ARRAY_IDX(copy_pairs, i,
svn_client__copy_pair_t *);
SVN_ERR(svn_client_commit_item_create
((const svn_client_commit_item3_t **) &item, pool));
item->url = pair->dst;
item->state_flags = SVN_CLIENT_COMMIT_ITEM_ADD;
APR_ARRAY_PUSH(commit_items, svn_client_commit_item3_t *) = item;
}
SVN_ERR(svn_client__get_log_msg(&message, &tmp_file, commit_items,
ctx, pool));
if (! message) {
SVN_ERR(svn_wc_adm_close(adm_access));
return SVN_NO_ERROR;
}
} else
message = "";
SVN_ERR(svn_client__ensure_revprop_table(&commit_revprops, revprop_table,
message, ctx, pool));
SVN_ERR(svn_io_check_path(top_src_path, &base_kind, pool));
if (base_kind == svn_node_dir)
SVN_ERR(svn_wc_adm_retrieve(&dir_access, adm_access, top_src_path, pool));
else
dir_access = adm_access;
SVN_ERR(svn_client__get_copy_committables(&committables,
copy_pairs, dir_access,
ctx, pool));
if (! (commit_items = apr_hash_get(committables,
SVN_CLIENT__SINGLE_REPOS_NAME,
APR_HASH_KEY_STRING))) {
SVN_ERR(svn_wc_adm_close(adm_access));
return SVN_NO_ERROR;
}
if (make_parents) {
for (i = 0; i < new_dirs->nelts; i++) {
const char *url = APR_ARRAY_IDX(new_dirs, i, const char *);
svn_client_commit_item3_t *item;
SVN_ERR(svn_client_commit_item_create
((const svn_client_commit_item3_t **) &item, pool));
item->url = url;
item->state_flags = SVN_CLIENT_COMMIT_ITEM_ADD;
item->incoming_prop_changes = apr_array_make(pool, 1,
sizeof(svn_prop_t *));
APR_ARRAY_PUSH(commit_items, svn_client_commit_item3_t *) = item;
}
}
SVN_ERR(svn_ra_get_repos_root2(ra_session, &repos_root, pool));
SVN_ERR(svn_ra_reparent(ra_session, repos_root, pool));
for (i = 0; i < copy_pairs->nelts; i++) {
svn_prop_t *mergeinfo_prop;
apr_hash_t *mergeinfo, *wc_mergeinfo;
svn_client__copy_pair_t *pair = APR_ARRAY_IDX(copy_pairs, i,
svn_client__copy_pair_t *);
svn_client_commit_item3_t *item =
APR_ARRAY_IDX(commit_items, i, svn_client_commit_item3_t *);
item->outgoing_prop_changes = apr_array_make(pool, 1,
sizeof(svn_prop_t *));
mergeinfo_prop = apr_palloc(item->outgoing_prop_changes->pool,
sizeof(svn_prop_t));
mergeinfo_prop->name = SVN_PROP_MERGEINFO;
SVN_ERR(calculate_target_mergeinfo(ra_session, &mergeinfo, adm_access,
pair->src, pair->src_revnum,
FALSE, ctx, pool));
SVN_ERR(svn_wc_entry(&entry, pair->src, adm_access, FALSE, pool));
SVN_ERR(svn_client__parse_mergeinfo(&wc_mergeinfo, entry,
pair->src, FALSE, adm_access, ctx,
pool));
if (wc_mergeinfo && mergeinfo)
SVN_ERR(svn_mergeinfo_merge(mergeinfo, wc_mergeinfo, pool));
else if (! mergeinfo)
mergeinfo = wc_mergeinfo;
if (mergeinfo) {
SVN_ERR(svn_mergeinfo_to_string((svn_string_t **)
&mergeinfo_prop->value,
mergeinfo, pool));
APR_ARRAY_PUSH(item->outgoing_prop_changes, svn_prop_t *) =
mergeinfo_prop;
}
}
SVN_ERR(svn_client__condense_commit_items(&top_dst_url,
commit_items, pool));
SVN_ERR(svn_client__open_ra_session_internal(&ra_session, top_dst_url,
NULL, NULL, commit_items,
FALSE, FALSE, ctx, pool));
SVN_ERR(svn_client__commit_get_baton(&commit_baton, commit_info_p, pool));
SVN_ERR(svn_ra_get_commit_editor3(ra_session, &editor, &edit_baton,
commit_revprops,
svn_client__commit_callback,
commit_baton, NULL,
TRUE,
pool));
SVN_ERR_W(svn_client__do_commit(top_dst_url, commit_items, adm_access,
editor, edit_baton,
0,
NULL, NULL, ctx, pool),
_("Commit failed (details follow):"));
svn_sleep_for_timestamps();
SVN_ERR(svn_wc_adm_close(adm_access));
return SVN_NO_ERROR;
}
static svn_error_t *
repos_to_wc_copy_single(svn_client__copy_pair_t *pair,
svn_boolean_t same_repositories,
svn_ra_session_t *ra_session,
svn_wc_adm_access_t *adm_access,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
svn_revnum_t src_revnum = pair->src_revnum;
apr_hash_t *src_mergeinfo;
const svn_wc_entry_t *dst_entry;
if (pair->src_kind == svn_node_dir) {
SVN_ERR(svn_client__checkout_internal
(NULL, pair->src_original, pair->dst, &pair->src_peg_revision,
&pair->src_op_revision,
SVN_DEPTH_INFINITY_OR_FILES(TRUE),
FALSE, FALSE, NULL, ctx, pool));
if (same_repositories) {
svn_wc_adm_access_t *dst_access;
SVN_ERR(svn_wc_adm_open3(&dst_access, adm_access, pair->dst, TRUE,
-1, ctx->cancel_func, ctx->cancel_baton,
pool));
SVN_ERR(svn_wc_entry(&dst_entry, pair->dst, dst_access, FALSE,
pool));
if (pair->src_op_revision.kind == svn_opt_revision_head) {
src_revnum = dst_entry->revision;
}
SVN_ERR(svn_wc_add2(pair->dst, adm_access, pair->src,
src_revnum,
ctx->cancel_func, ctx->cancel_baton,
ctx->notify_func2, ctx->notify_baton2, pool));
SVN_ERR(calculate_target_mergeinfo(ra_session, &src_mergeinfo, NULL,
pair->src, src_revnum,
FALSE, ctx, pool));
SVN_ERR(extend_wc_mergeinfo(pair->dst, dst_entry, src_mergeinfo,
dst_access, ctx, pool));
} else {
return svn_error_createf
(SVN_ERR_UNSUPPORTED_FEATURE, NULL,
_("Source URL '%s' is from foreign repository; "
"leaving it as a disjoint WC"), pair->src);
}
}
else if (pair->src_kind == svn_node_file) {
apr_file_t *fp;
svn_stream_t *fstream;
svn_revnum_t real_rev;
const char *new_text_path;
apr_hash_t *new_props;
svn_error_t *err;
const char *src_rel;
SVN_ERR(svn_io_open_unique_file2(&fp, &new_text_path, pair->dst, ".tmp",
svn_io_file_del_none, pool));
fstream = svn_stream_from_aprfile2(fp, FALSE, pool);
SVN_ERR(svn_client__path_relative_to_session(&src_rel, ra_session,
pair->src, pool));
SVN_ERR(svn_ra_get_file(ra_session, src_rel, src_revnum, fstream,
&real_rev, &new_props, pool));
SVN_ERR(svn_stream_close(fstream));
if (! SVN_IS_VALID_REVNUM(src_revnum))
src_revnum = real_rev;
err = svn_wc_add_repos_file2
(pair->dst, adm_access,
new_text_path, NULL, new_props, NULL,
same_repositories ? pair->src : NULL,
same_repositories ? src_revnum : SVN_INVALID_REVNUM,
pool);
SVN_ERR(svn_wc_entry(&dst_entry, pair->dst, adm_access, FALSE, pool));
SVN_ERR(calculate_target_mergeinfo(ra_session, &src_mergeinfo,
NULL, pair->src, src_revnum,
FALSE, ctx, pool));
SVN_ERR(extend_wc_mergeinfo(pair->dst, dst_entry, src_mergeinfo,
adm_access, ctx, pool));
if (!err && ctx->notify_func2) {
svn_wc_notify_t *notify = svn_wc_create_notify(pair->dst,
svn_wc_notify_add,
pool);
notify->kind = pair->src_kind;
(*ctx->notify_func2)(ctx->notify_baton2, notify, pool);
}
svn_sleep_for_timestamps();
SVN_ERR(err);
}
return SVN_NO_ERROR;
}
static svn_error_t *
repos_to_wc_copy(const apr_array_header_t *copy_pairs,
svn_boolean_t make_parents,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
svn_ra_session_t *ra_session;
svn_wc_adm_access_t *adm_access;
const char *top_src_url, *top_dst_path;
const char *src_uuid = NULL, *dst_uuid = NULL;
svn_boolean_t same_repositories;
apr_pool_t *iterpool = svn_pool_create(pool);
int i;
for (i = 0; i < copy_pairs->nelts; i++) {
svn_client__copy_pair_t *pair = APR_ARRAY_IDX(copy_pairs, i,
svn_client__copy_pair_t *);
const char *src, *ignored_url;
svn_opt_revision_t *new_rev, *ignored_rev, dead_end_rev;
svn_pool_clear(iterpool);
dead_end_rev.kind = svn_opt_revision_unspecified;
SVN_ERR(svn_client__repos_locations(&src, &new_rev,
&ignored_url, &ignored_rev,
NULL,
pair->src,
&pair->src_peg_revision,
&pair->src_op_revision,
&dead_end_rev,
ctx, iterpool));
pair->src_original = pair->src;
pair->src = apr_pstrdup(pool, src);
}
get_copy_pair_ancestors(copy_pairs, &top_src_url, &top_dst_path, NULL, pool);
if (copy_pairs->nelts == 1)
top_src_url = svn_path_dirname(top_src_url, pool);
SVN_ERR(svn_client__open_ra_session_internal(&ra_session, top_src_url, NULL,
NULL, NULL, FALSE, TRUE,
ctx, pool));
for (i = 0; i < copy_pairs->nelts; i++) {
svn_client__copy_pair_t *pair = APR_ARRAY_IDX(copy_pairs, i,
svn_client__copy_pair_t *);
SVN_ERR(svn_client__get_revision_number
(&pair->src_revnum, NULL, ra_session, &pair->src_op_revision,
NULL, pool));
}
for (i = 0; i < copy_pairs->nelts; i++) {
svn_client__copy_pair_t *pair = APR_ARRAY_IDX(copy_pairs, i,
svn_client__copy_pair_t *);
svn_node_kind_t dst_parent_kind, dst_kind;
const char *dst_parent;
const char *src_rel;
svn_pool_clear(iterpool);
SVN_ERR(svn_client__path_relative_to_session(&src_rel, ra_session,
pair->src, iterpool));
SVN_ERR(svn_ra_check_path(ra_session, src_rel, pair->src_revnum,
&pair->src_kind, pool));
if (pair->src_kind == svn_node_none) {
if (SVN_IS_VALID_REVNUM(pair->src_revnum))
return svn_error_createf
(SVN_ERR_FS_NOT_FOUND, NULL,
_("Path '%s' not found in revision %ld"),
pair->src, pair->src_revnum);
else
return svn_error_createf
(SVN_ERR_FS_NOT_FOUND, NULL,
_("Path '%s' not found in head revision"), pair->src);
}
SVN_ERR(svn_io_check_path(pair->dst, &dst_kind, iterpool));
if (dst_kind != svn_node_none) {
return svn_error_createf(SVN_ERR_ENTRY_EXISTS, NULL,
_("Path '%s' already exists"),
svn_path_local_style(pair->dst, pool));
}
dst_parent = svn_path_dirname(pair->dst, iterpool);
SVN_ERR(svn_io_check_path(dst_parent, &dst_parent_kind, iterpool));
if (make_parents && dst_parent_kind == svn_node_none) {
SVN_ERR(svn_client__make_local_parents(dst_parent, TRUE, ctx,
iterpool));
} else if (dst_parent_kind != svn_node_dir) {
return svn_error_createf(SVN_ERR_WC_NOT_DIRECTORY, NULL,
_("Path '%s' is not a directory"),
svn_path_local_style(dst_parent, pool));
}
}
SVN_ERR(svn_wc_adm_probe_open3(&adm_access, NULL, top_dst_path, TRUE,
0, ctx->cancel_func, ctx->cancel_baton,
pool));
for (i = 0; i < copy_pairs->nelts; i++) {
svn_client__copy_pair_t *pair = APR_ARRAY_IDX(copy_pairs, i,
svn_client__copy_pair_t *);
const svn_wc_entry_t *ent;
svn_pool_clear(iterpool);
SVN_ERR(svn_wc_entry(&ent, pair->dst, adm_access, FALSE, iterpool));
if (ent && (ent->kind != svn_node_dir) &&
(ent->schedule != svn_wc_schedule_delete))
return svn_error_createf
(SVN_ERR_WC_OBSTRUCTED_UPDATE, NULL,
_("Entry for '%s' exists (though the working file is missing)"),
svn_path_local_style(pair->dst, pool));
}
{
svn_error_t *src_err, *dst_err;
const char *parent;
src_err = svn_ra_get_uuid2(ra_session, &src_uuid, pool);
if (src_err && src_err->apr_err != SVN_ERR_RA_NO_REPOS_UUID)
return src_err;
if (copy_pairs->nelts == 1)
svn_path_split(top_dst_path, &parent, NULL, pool);
else
parent = top_dst_path;
dst_err = svn_client_uuid_from_path(&dst_uuid, parent, adm_access,
ctx, pool);
if (dst_err && dst_err->apr_err != SVN_ERR_RA_NO_REPOS_UUID)
return dst_err;
if (src_err || dst_err || (! src_uuid) || (! dst_uuid))
same_repositories = FALSE;
else
same_repositories = (strcmp(src_uuid, dst_uuid) == 0) ? TRUE : FALSE;
}
for (i = 0; i < copy_pairs->nelts; i++) {
if (ctx->cancel_func)
SVN_ERR(ctx->cancel_func(ctx->cancel_baton));
svn_pool_clear(iterpool);
SVN_ERR(repos_to_wc_copy_single(APR_ARRAY_IDX(copy_pairs, i,
svn_client__copy_pair_t *),
same_repositories,
ra_session, adm_access,
ctx, iterpool));
}
SVN_ERR(svn_wc_adm_close(adm_access));
svn_pool_destroy(iterpool);
return SVN_NO_ERROR;
}
#define NEED_REPOS_REVNUM(revision) ((revision.kind != svn_opt_revision_unspecified) && (revision.kind != svn_opt_revision_working))
static svn_error_t *
setup_copy(svn_commit_info_t **commit_info_p,
const apr_array_header_t *sources,
const char *dst_path_in,
svn_boolean_t is_move,
svn_boolean_t force,
svn_boolean_t make_parents,
const apr_hash_t *revprop_table,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
apr_array_header_t *copy_pairs = apr_array_make(pool, sources->nelts,
sizeof(struct copy_pair *));
svn_boolean_t srcs_are_urls, dst_is_url;
int i;
for (i = 0; i < sources->nelts; i++) {
svn_client_copy_source_t *source =
((svn_client_copy_source_t **) (sources->elts))[i];
if (svn_path_is_url(source->path)
&& (source->peg_revision->kind == svn_opt_revision_base
|| source->peg_revision->kind == svn_opt_revision_committed
|| source->peg_revision->kind == svn_opt_revision_previous))
return svn_error_create
(SVN_ERR_CLIENT_BAD_REVISION, NULL,
_("Revision type requires a working copy path, not a URL"));
}
srcs_are_urls = svn_path_is_url(APR_ARRAY_IDX(sources, 0,
svn_client_copy_source_t *)->path);
dst_is_url = svn_path_is_url(dst_path_in);
if (sources->nelts > 1) {
apr_pool_t *iterpool = svn_pool_create(pool);
for (i = 0; i < sources->nelts; i++) {
svn_client_copy_source_t *source = APR_ARRAY_IDX(sources, i,
svn_client_copy_source_t *);
svn_client__copy_pair_t *pair = apr_palloc(pool, sizeof(*pair));
const char *src_basename;
svn_boolean_t src_is_url = svn_path_is_url(source->path);
svn_pool_clear(iterpool);
pair->src = apr_pstrdup(pool, source->path);
pair->src_op_revision = *source->revision;
pair->src_peg_revision = *source->peg_revision;
SVN_ERR(svn_opt_resolve_revisions(&pair->src_peg_revision,
&pair->src_op_revision,
src_is_url,
TRUE,
iterpool));
src_basename = svn_path_basename(pair->src, iterpool);
if (srcs_are_urls && ! dst_is_url)
src_basename = svn_path_uri_decode(src_basename, iterpool);
if (src_is_url != srcs_are_urls)
return svn_error_create
(SVN_ERR_UNSUPPORTED_FEATURE, NULL,
_("Cannot mix repository and working copy sources"));
pair->dst = svn_path_join(dst_path_in, src_basename, pool);
APR_ARRAY_PUSH(copy_pairs, svn_client__copy_pair_t *) = pair;
}
svn_pool_destroy(iterpool);
} else {
svn_client__copy_pair_t *pair = apr_palloc(pool, sizeof(*pair));
svn_client_copy_source_t *source =
APR_ARRAY_IDX(sources, 0, svn_client_copy_source_t *);
pair->src = apr_pstrdup(pool, source->path);
pair->src_op_revision = *source->revision;
pair->src_peg_revision = *source->peg_revision;
SVN_ERR(svn_opt_resolve_revisions(&pair->src_peg_revision,
&pair->src_op_revision,
svn_path_is_url(pair->src),
TRUE,
pool));
pair->dst = dst_path_in;
APR_ARRAY_PUSH(copy_pairs, svn_client__copy_pair_t *) = pair;
}
if (!srcs_are_urls && !dst_is_url) {
apr_pool_t *iterpool = svn_pool_create(pool);
for (i = 0; i < copy_pairs->nelts; i++ ) {
svn_client__copy_pair_t *pair = APR_ARRAY_IDX(copy_pairs, i,
svn_client__copy_pair_t *);
svn_pool_clear(iterpool);
if (svn_path_is_child(pair->src, pair->dst, iterpool))
return svn_error_createf
(SVN_ERR_UNSUPPORTED_FEATURE, NULL,
_("Cannot copy path '%s' into its own child '%s'"),
svn_path_local_style(pair->src, pool),
svn_path_local_style(pair->dst, pool));
}
svn_pool_destroy(iterpool);
}
if (is_move) {
if (srcs_are_urls == dst_is_url) {
for (i = 0; i < copy_pairs->nelts; i++) {
svn_client__copy_pair_t *pair = APR_ARRAY_IDX(copy_pairs, i,
svn_client__copy_pair_t *);
if (strcmp(pair->src, pair->dst) == 0)
return svn_error_createf
(SVN_ERR_UNSUPPORTED_FEATURE, NULL,
_("Cannot move path '%s' into itself"),
svn_path_local_style(pair->src, pool));
}
} else {
return svn_error_create
(SVN_ERR_UNSUPPORTED_FEATURE, NULL,
_("Moves between the working copy and the repository are not "
"supported"));
}
} else {
if (!srcs_are_urls) {
svn_boolean_t need_repos_op_rev = FALSE;
svn_boolean_t need_repos_peg_rev = FALSE;
for (i = 0; i < copy_pairs->nelts; i++) {
svn_client__copy_pair_t *pair = APR_ARRAY_IDX(copy_pairs, i,
svn_client__copy_pair_t *);
if (NEED_REPOS_REVNUM(pair->src_op_revision))
need_repos_op_rev = TRUE;
if (NEED_REPOS_REVNUM(pair->src_peg_revision))
need_repos_peg_rev = TRUE;
if (need_repos_op_rev || need_repos_peg_rev)
break;
}
if (need_repos_op_rev || need_repos_peg_rev) {
apr_pool_t *iterpool = svn_pool_create(pool);
for (i = 0; i < copy_pairs->nelts; i++) {
const char *url;
svn_client__copy_pair_t *pair = APR_ARRAY_IDX(copy_pairs, i,
svn_client__copy_pair_t *);
svn_wc_adm_access_t *adm_access;
const svn_wc_entry_t *entry;
svn_pool_clear(iterpool);
SVN_ERR(svn_wc_adm_probe_open3(&adm_access, NULL,
pair->src, FALSE, 0,
ctx->cancel_func,
ctx->cancel_baton,
iterpool));
SVN_ERR(svn_wc__entry_versioned(&entry, pair->src, adm_access,
FALSE, iterpool));
SVN_ERR(svn_wc_adm_close(adm_access));
url = (entry->copied ? entry->copyfrom_url : entry->url);
if (url == NULL)
return svn_error_createf
(SVN_ERR_ENTRY_MISSING_URL, NULL,
_("'%s' does not have a URL associated with it"),
svn_path_local_style(pair->src, pool));
pair->src = apr_pstrdup(pool, url);
if (!need_repos_peg_rev
|| pair->src_peg_revision.kind == svn_opt_revision_base) {
pair->src_peg_revision.kind = svn_opt_revision_number;
pair->src_peg_revision.value.number =
(entry->copied ? entry->copyfrom_rev : entry->revision);
}
if (pair->src_op_revision.kind == svn_opt_revision_base) {
pair->src_op_revision.kind = svn_opt_revision_number;
pair->src_op_revision.value.number =
(entry->copied ? entry->copyfrom_rev : entry->revision);
}
}
svn_pool_destroy(iterpool);
srcs_are_urls = TRUE;
}
}
}
if ((! srcs_are_urls) && (! dst_is_url)) {
*commit_info_p = NULL;
SVN_ERR(wc_to_wc_copy(copy_pairs, is_move, make_parents,
ctx, pool));
} else if ((! srcs_are_urls) && (dst_is_url)) {
SVN_ERR(wc_to_repos_copy(commit_info_p, copy_pairs, make_parents,
revprop_table, ctx, pool));
} else if ((srcs_are_urls) && (! dst_is_url)) {
*commit_info_p = NULL;
SVN_ERR(repos_to_wc_copy(copy_pairs, make_parents, ctx, pool));
} else {
SVN_ERR(repos_to_repos_copy(commit_info_p, copy_pairs, make_parents,
revprop_table, ctx, is_move, pool));
}
return SVN_NO_ERROR;
}
svn_error_t *
svn_client_copy4(svn_commit_info_t **commit_info_p,
apr_array_header_t *sources,
const char *dst_path,
svn_boolean_t copy_as_child,
svn_boolean_t make_parents,
const apr_hash_t *revprop_table,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
svn_error_t *err;
svn_commit_info_t *commit_info = NULL;
apr_pool_t *subpool = svn_pool_create(pool);
if (sources->nelts > 1 && !copy_as_child)
return svn_error_create(SVN_ERR_CLIENT_MULTIPLE_SOURCES_DISALLOWED,
NULL, NULL);
err = setup_copy(&commit_info,
sources, dst_path,
FALSE ,
TRUE ,
make_parents,
revprop_table,
ctx,
subpool);
if (copy_as_child && err && (sources->nelts == 1)
&& (err->apr_err == SVN_ERR_ENTRY_EXISTS
|| err->apr_err == SVN_ERR_FS_ALREADY_EXISTS)) {
const char *src_path = APR_ARRAY_IDX(sources, 0,
svn_client_copy_source_t *)->path;
const char *src_basename;
svn_error_clear(err);
svn_pool_clear(subpool);
src_basename = svn_path_basename(src_path, subpool);
if (svn_path_is_url(src_path) && ! svn_path_is_url(dst_path))
src_basename = svn_path_uri_decode(src_basename, subpool);
err = setup_copy(&commit_info,
sources,
svn_path_join(dst_path, src_basename, subpool),
FALSE ,
TRUE ,
make_parents,
revprop_table,
ctx,
subpool);
}
if (commit_info_p != NULL) {
if (commit_info)
*commit_info_p = svn_commit_info_dup(commit_info, pool);
else
*commit_info_p = NULL;
}
svn_pool_destroy(subpool);
return err;
}
svn_error_t *
svn_client_copy3(svn_commit_info_t **commit_info_p,
const char *src_path,
const svn_opt_revision_t *src_revision,
const char *dst_path,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
apr_array_header_t *sources = apr_array_make(pool, 1,
sizeof(const svn_client_copy_source_t *));
svn_client_copy_source_t copy_source;
copy_source.path = src_path;
copy_source.revision = src_revision;
copy_source.peg_revision = src_revision;
APR_ARRAY_PUSH(sources, const svn_client_copy_source_t *) = &copy_source;
return svn_client_copy4(commit_info_p, sources, dst_path, FALSE, FALSE,
NULL, ctx, pool);
}
svn_error_t *
svn_client_copy2(svn_commit_info_t **commit_info_p,
const char *src_path,
const svn_opt_revision_t *src_revision,
const char *dst_path,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
svn_error_t *err;
err = svn_client_copy3(commit_info_p, src_path, src_revision,
dst_path, ctx, pool);
if (err && (err->apr_err == SVN_ERR_ENTRY_EXISTS
|| err->apr_err == SVN_ERR_FS_ALREADY_EXISTS)) {
const char *src_basename = svn_path_basename(src_path, pool);
svn_error_clear(err);
return svn_client_copy3(commit_info_p, src_path, src_revision,
svn_path_join(dst_path, src_basename, pool),
ctx, pool);
}
return err;
}
svn_error_t *
svn_client_copy(svn_client_commit_info_t **commit_info_p,
const char *src_path,
const svn_opt_revision_t *src_revision,
const char *dst_path,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
svn_commit_info_t *commit_info = NULL;
svn_error_t *err;
err = svn_client_copy2(&commit_info, src_path, src_revision, dst_path,
ctx, pool);
*commit_info_p = (svn_client_commit_info_t *) commit_info;
return err;
}
svn_error_t *
svn_client_move5(svn_commit_info_t **commit_info_p,
apr_array_header_t *src_paths,
const char *dst_path,
svn_boolean_t force,
svn_boolean_t move_as_child,
svn_boolean_t make_parents,
const apr_hash_t *revprop_table,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
svn_commit_info_t *commit_info = NULL;
const svn_opt_revision_t head_revision
= { svn_opt_revision_head, { 0 } };
svn_error_t *err;
int i;
apr_pool_t *subpool = svn_pool_create(pool);
apr_array_header_t *sources = apr_array_make(pool, src_paths->nelts,
sizeof(const svn_client_copy_source_t *));
if (src_paths->nelts > 1 && !move_as_child)
return svn_error_create(SVN_ERR_CLIENT_MULTIPLE_SOURCES_DISALLOWED,
NULL, NULL);
for (i = 0; i < src_paths->nelts; i++) {
const char *src_path = APR_ARRAY_IDX(src_paths, i, const char *);
svn_client_copy_source_t *copy_source = apr_palloc(pool,
sizeof(*copy_source));
copy_source->path = src_path;
copy_source->revision = &head_revision;
copy_source->peg_revision = &head_revision;
APR_ARRAY_PUSH(sources, svn_client_copy_source_t *) = copy_source;
}
err = setup_copy(&commit_info, sources, dst_path,
TRUE ,
force,
make_parents,
revprop_table,
ctx,
subpool);
if (move_as_child && err && (src_paths->nelts == 1)
&& (err->apr_err == SVN_ERR_ENTRY_EXISTS
|| err->apr_err == SVN_ERR_FS_ALREADY_EXISTS)) {
const char *src_path = APR_ARRAY_IDX(src_paths, 0, const char *);
const char *src_basename;
svn_error_clear(err);
svn_pool_clear(subpool);
src_basename = svn_path_basename(src_path, pool);
err = setup_copy(&commit_info, sources,
svn_path_join(dst_path, src_basename, pool),
TRUE ,
force,
make_parents,
revprop_table,
ctx,
subpool);
}
if (commit_info_p != NULL) {
if (commit_info)
*commit_info_p = svn_commit_info_dup(commit_info, pool);
else
*commit_info_p = commit_info;
}
svn_pool_destroy(subpool);
return err;
}
svn_error_t *
svn_client_move4(svn_commit_info_t **commit_info_p,
const char *src_path,
const char *dst_path,
svn_boolean_t force,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
apr_array_header_t *src_paths =
apr_array_make(pool, 1, sizeof(const char *));
APR_ARRAY_PUSH(src_paths, const char *) = src_path;
return svn_client_move5(commit_info_p, src_paths, dst_path, force, FALSE,
FALSE, NULL, ctx, pool);
}
svn_error_t *
svn_client_move3(svn_commit_info_t **commit_info_p,
const char *src_path,
const char *dst_path,
svn_boolean_t force,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
svn_error_t *err;
err = svn_client_move4(commit_info_p, src_path, dst_path, force, ctx, pool);
if (err && (err->apr_err == SVN_ERR_ENTRY_EXISTS
|| err->apr_err == SVN_ERR_FS_ALREADY_EXISTS)) {
const char *src_basename = svn_path_basename(src_path, pool);
svn_error_clear(err);
return svn_client_move4(commit_info_p, src_path,
svn_path_join(dst_path, src_basename, pool),
force, ctx, pool);
}
return err;
}
svn_error_t *
svn_client_move2(svn_client_commit_info_t **commit_info_p,
const char *src_path,
const char *dst_path,
svn_boolean_t force,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
svn_commit_info_t *commit_info = NULL;
svn_error_t *err;
err = svn_client_move3(&commit_info, src_path, dst_path, force, ctx, pool);
*commit_info_p = (svn_client_commit_info_t *) commit_info;
return err;
}
svn_error_t *
svn_client_move(svn_client_commit_info_t **commit_info_p,
const char *src_path,
const svn_opt_revision_t *src_revision,
const char *dst_path,
svn_boolean_t force,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
svn_commit_info_t *commit_info = NULL;
svn_error_t *err;
svn_client_copy_source_t copy_source;
apr_array_header_t *sources = apr_array_make(pool, 1,
sizeof(const svn_client_copy_source_t *));
if (src_revision->kind != svn_opt_revision_unspecified
&& src_revision->kind != svn_opt_revision_head) {
return svn_error_create
(SVN_ERR_UNSUPPORTED_FEATURE, NULL,
_("Cannot specify revisions (except HEAD) with move operations"));
}
copy_source.path = src_path;
copy_source.revision = src_revision;
copy_source.peg_revision = src_revision;
APR_ARRAY_PUSH(sources, const svn_client_copy_source_t *) = &copy_source;
err = setup_copy(&commit_info,
sources, dst_path,
TRUE ,
force,
FALSE ,
NULL,
ctx,
pool);
*commit_info_p = (svn_client_commit_info_t *) commit_info;
return err;
}