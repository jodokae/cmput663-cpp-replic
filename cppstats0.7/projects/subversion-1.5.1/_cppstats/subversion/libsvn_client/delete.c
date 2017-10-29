#include <apr_file_io.h>
#include "svn_types.h"
#include "svn_pools.h"
#include "svn_wc.h"
#include "svn_client.h"
#include "svn_error.h"
#include "svn_path.h"
#include "client.h"
#include "svn_private_config.h"
struct status_baton {
svn_error_t *err;
apr_pool_t *pool;
};
static void
find_undeletables(void *baton,
const char *path,
svn_wc_status2_t *status) {
struct status_baton *sb = baton;
if (sb->err)
return;
if (status->text_status == svn_wc_status_obstructed)
sb->err = svn_error_createf(SVN_ERR_NODE_UNEXPECTED_KIND, NULL,
_("'%s' is in the way of the resource "
"actually under version control"),
svn_path_local_style(path, sb->pool));
else if (! status->entry)
sb->err = svn_error_createf(SVN_ERR_UNVERSIONED_RESOURCE, NULL,
_("'%s' is not under version control"),
svn_path_local_style(path, sb->pool));
else if ((status->text_status != svn_wc_status_normal
&& status->text_status != svn_wc_status_deleted
&& status->text_status != svn_wc_status_missing)
||
(status->prop_status != svn_wc_status_none
&& status->prop_status != svn_wc_status_normal))
sb->err = svn_error_createf(SVN_ERR_CLIENT_MODIFIED, NULL,
_("'%s' has local modifications"),
svn_path_local_style(path, sb->pool));
}
svn_error_t *
svn_client__can_delete(const char *path,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
struct status_baton sb;
svn_opt_revision_t revision;
revision.kind = svn_opt_revision_unspecified;
sb.err = SVN_NO_ERROR;
sb.pool = pool;
SVN_ERR(svn_client_status3
(NULL, path, &revision, find_undeletables, &sb,
svn_depth_infinity, FALSE, FALSE, FALSE, FALSE, NULL, ctx, pool));
return sb.err;
}
static svn_error_t *
path_driver_cb_func(void **dir_baton,
void *parent_baton,
void *callback_baton,
const char *path,
apr_pool_t *pool) {
const svn_delta_editor_t *editor = callback_baton;
*dir_baton = NULL;
return editor->delete_entry(path, SVN_INVALID_REVNUM, parent_baton, pool);
}
static svn_error_t *
delete_urls(svn_commit_info_t **commit_info_p,
const apr_array_header_t *paths,
const apr_hash_t *revprop_table,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
svn_ra_session_t *ra_session;
const svn_delta_editor_t *editor;
void *edit_baton;
void *commit_baton;
const char *log_msg;
svn_node_kind_t kind;
apr_array_header_t *targets;
apr_hash_t *commit_revprops;
svn_error_t *err;
const char *common;
int i;
apr_pool_t *subpool = svn_pool_create(pool);
SVN_ERR(svn_path_condense_targets(&common, &targets, paths, TRUE, pool));
if (! targets->nelts) {
const char *bname;
svn_path_split(common, &common, &bname, pool);
APR_ARRAY_PUSH(targets, const char *) = bname;
}
if (SVN_CLIENT__HAS_LOG_MSG_FUNC(ctx)) {
svn_client_commit_item3_t *item;
const char *tmp_file;
apr_array_header_t *commit_items
= apr_array_make(pool, targets->nelts, sizeof(item));
for (i = 0; i < targets->nelts; i++) {
const char *path = APR_ARRAY_IDX(targets, i, const char *);
SVN_ERR(svn_client_commit_item_create
((const svn_client_commit_item3_t **) &item, pool));
item->url = svn_path_join(common, path, pool);
item->state_flags = SVN_CLIENT_COMMIT_ITEM_DELETE;
APR_ARRAY_PUSH(commit_items, svn_client_commit_item3_t *) = item;
}
SVN_ERR(svn_client__get_log_msg(&log_msg, &tmp_file, commit_items,
ctx, pool));
if (! log_msg) {
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
} else
log_msg = "";
SVN_ERR(svn_client__ensure_revprop_table(&commit_revprops, revprop_table,
log_msg, ctx, pool));
SVN_ERR(svn_client__open_ra_session_internal(&ra_session, common, NULL,
NULL, NULL, FALSE, TRUE,
ctx, pool));
for (i = 0; i < targets->nelts; i++) {
const char *path = APR_ARRAY_IDX(targets, i, const char *);
svn_pool_clear(subpool);
path = svn_path_uri_decode(path, pool);
APR_ARRAY_IDX(targets, i, const char *) = path;
SVN_ERR(svn_ra_check_path(ra_session, path, SVN_INVALID_REVNUM,
&kind, subpool));
if (kind == svn_node_none)
return svn_error_createf(SVN_ERR_FS_NOT_FOUND, NULL,
"URL '%s' does not exist",
svn_path_local_style(path, pool));
}
svn_pool_destroy(subpool);
SVN_ERR(svn_client__commit_get_baton(&commit_baton, commit_info_p, pool));
SVN_ERR(svn_ra_get_commit_editor3(ra_session, &editor, &edit_baton,
commit_revprops,
svn_client__commit_callback,
commit_baton,
NULL, TRUE,
pool));
err = svn_delta_path_driver(editor, edit_baton, SVN_INVALID_REVNUM,
targets, path_driver_cb_func,
(void *)editor, pool);
if (err) {
svn_error_clear(editor->abort_edit(edit_baton, pool));
return err;
}
SVN_ERR(editor->close_edit(edit_baton, pool));
return SVN_NO_ERROR;
}
svn_error_t *
svn_client__wc_delete(const char *path,
svn_wc_adm_access_t *adm_access,
svn_boolean_t force,
svn_boolean_t dry_run,
svn_boolean_t keep_local,
svn_wc_notify_func2_t notify_func,
void *notify_baton,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
if (!force && !keep_local)
SVN_ERR(svn_client__can_delete(path, ctx, pool));
if (!dry_run)
SVN_ERR(svn_wc_delete3(path, adm_access,
ctx->cancel_func, ctx->cancel_baton,
notify_func, notify_baton, keep_local, pool));
return SVN_NO_ERROR;
}
svn_error_t *
svn_client_delete3(svn_commit_info_t **commit_info_p,
const apr_array_header_t *paths,
svn_boolean_t force,
svn_boolean_t keep_local,
const apr_hash_t *revprop_table,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
if (! paths->nelts)
return SVN_NO_ERROR;
if (svn_path_is_url(APR_ARRAY_IDX(paths, 0, const char *))) {
SVN_ERR(delete_urls(commit_info_p, paths, revprop_table, ctx, pool));
} else {
apr_pool_t *subpool = svn_pool_create(pool);
int i;
for (i = 0; i < paths->nelts; i++) {
svn_wc_adm_access_t *adm_access;
const char *path = APR_ARRAY_IDX(paths, i, const char *);
const char *parent_path;
svn_pool_clear(subpool);
parent_path = svn_path_dirname(path, subpool);
if (ctx->cancel_func)
SVN_ERR(ctx->cancel_func(ctx->cancel_baton));
SVN_ERR(svn_wc_adm_open3(&adm_access, NULL, parent_path,
TRUE, 0, ctx->cancel_func,
ctx->cancel_baton, subpool));
SVN_ERR(svn_client__wc_delete(path, adm_access, force,
FALSE, keep_local,
ctx->notify_func2,
ctx->notify_baton2,
ctx, subpool));
SVN_ERR(svn_wc_adm_close(adm_access));
}
svn_pool_destroy(subpool);
}
return SVN_NO_ERROR;
}
svn_error_t *
svn_client_delete2(svn_commit_info_t **commit_info_p,
const apr_array_header_t *paths,
svn_boolean_t force,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
return svn_client_delete3(commit_info_p, paths, force, FALSE, NULL,
ctx, pool);
}
svn_error_t *
svn_client_delete(svn_client_commit_info_t **commit_info_p,
const apr_array_header_t *paths,
svn_boolean_t force,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
svn_commit_info_t *commit_info = NULL;
svn_error_t *err = NULL;
err = svn_client_delete2(&commit_info, paths, force, ctx, pool);
*commit_info_p = (svn_client_commit_info_t *) commit_info;
return err;
}