#include <assert.h>
#include <apr_strings.h>
#include <apr_pools.h>
#include "svn_pools.h"
#include "client.h"
#include "svn_path.h"
#include "svn_delta.h"
#include "svn_client.h"
#include "svn_error.h"
#include "svn_hash.h"
#include "svn_private_config.h"
#include "private/svn_wc_private.h"
struct status_baton {
svn_boolean_t deleted_in_repos;
apr_hash_t *changelist_hash;
svn_wc_status_func2_t real_status_func;
void *real_status_baton;
};
static void
tweak_status(void *baton,
const char *path,
svn_wc_status2_t *status) {
struct status_baton *sb = baton;
if (sb->deleted_in_repos)
status->repos_text_status = svn_wc_status_deleted;
if (! SVN_WC__CL_MATCH(sb->changelist_hash, status->entry))
return;
sb->real_status_func(sb->real_status_baton, path, status);
}
typedef struct report_baton_t {
const svn_ra_reporter3_t* wrapped_reporter;
void *wrapped_report_baton;
char *ancestor;
void *set_locks_baton;
svn_client_ctx_t *ctx;
apr_pool_t *pool;
} report_baton_t;
static svn_error_t *
reporter_set_path(void *report_baton, const char *path,
svn_revnum_t revision, svn_depth_t depth,
svn_boolean_t start_empty, const char *lock_token,
apr_pool_t *pool) {
report_baton_t *rb = report_baton;
return rb->wrapped_reporter->set_path(rb->wrapped_report_baton, path,
revision, depth, start_empty,
lock_token, pool);
}
static svn_error_t *
reporter_delete_path(void *report_baton, const char *path, apr_pool_t *pool) {
report_baton_t *rb = report_baton;
return rb->wrapped_reporter->delete_path(rb->wrapped_report_baton, path,
pool);
}
static svn_error_t *
reporter_link_path(void *report_baton, const char *path, const char *url,
svn_revnum_t revision, svn_depth_t depth,
svn_boolean_t start_empty,
const char *lock_token, apr_pool_t *pool) {
report_baton_t *rb = report_baton;
const char *ancestor;
apr_size_t len;
ancestor = svn_path_get_longest_ancestor(url, rb->ancestor, pool);
len = strlen(ancestor);
if (len < strlen(rb->ancestor))
rb->ancestor[len] = '\0';
return rb->wrapped_reporter->link_path(rb->wrapped_report_baton, path, url,
revision, depth, start_empty,
lock_token, pool);
}
static svn_error_t *
reporter_finish_report(void *report_baton, apr_pool_t *pool) {
report_baton_t *rb = report_baton;
svn_ra_session_t *ras;
apr_hash_t *locks;
const char *repos_root;
apr_pool_t *subpool = svn_pool_create(pool);
svn_error_t *err = SVN_NO_ERROR;
SVN_ERR(svn_client__open_ra_session_internal(&ras, rb->ancestor, NULL,
NULL, NULL, FALSE, TRUE,
rb->ctx, subpool));
err = svn_ra_get_locks(ras, &locks, "", rb->pool);
if (err && ((err->apr_err == SVN_ERR_RA_NOT_IMPLEMENTED)
|| (err->apr_err == SVN_ERR_UNSUPPORTED_FEATURE))) {
svn_error_clear(err);
err = SVN_NO_ERROR;
locks = apr_hash_make(rb->pool);
}
SVN_ERR(err);
SVN_ERR(svn_ra_get_repos_root2(ras, &repos_root, rb->pool));
svn_pool_destroy(subpool);
SVN_ERR(svn_wc_status_set_repos_locks(rb->set_locks_baton, locks,
repos_root, rb->pool));
return rb->wrapped_reporter->finish_report(rb->wrapped_report_baton, pool);
}
static svn_error_t *
reporter_abort_report(void *report_baton, apr_pool_t *pool) {
report_baton_t *rb = report_baton;
return rb->wrapped_reporter->abort_report(rb->wrapped_report_baton, pool);
}
static svn_ra_reporter3_t lock_fetch_reporter = {
reporter_set_path,
reporter_delete_path,
reporter_link_path,
reporter_finish_report,
reporter_abort_report
};
svn_error_t *
svn_client_status3(svn_revnum_t *result_rev,
const char *path,
const svn_opt_revision_t *revision,
svn_wc_status_func2_t status_func,
void *status_baton,
svn_depth_t depth,
svn_boolean_t get_all,
svn_boolean_t update,
svn_boolean_t no_ignore,
svn_boolean_t ignore_externals,
const apr_array_header_t *changelists,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
svn_wc_adm_access_t *anchor_access, *target_access;
svn_wc_traversal_info_t *traversal_info = svn_wc_init_traversal_info(pool);
const char *anchor, *target;
const svn_delta_editor_t *editor;
void *edit_baton, *set_locks_baton;
const svn_wc_entry_t *entry = NULL;
struct status_baton sb;
apr_array_header_t *ignores;
svn_error_t *err;
apr_hash_t *changelist_hash = NULL;
svn_revnum_t edit_revision = SVN_INVALID_REVNUM;
if (changelists && changelists->nelts)
SVN_ERR(svn_hash_from_cstring_keys(&changelist_hash, changelists, pool));
sb.real_status_func = status_func;
sb.real_status_baton = status_baton;
sb.deleted_in_repos = FALSE;
sb.changelist_hash = changelist_hash;
err = svn_wc_adm_open3(&anchor_access, NULL, path, FALSE,
SVN_DEPTH_IS_RECURSIVE(depth) ? -1 : 1,
ctx->cancel_func, ctx->cancel_baton,
pool);
if (err && err->apr_err == SVN_ERR_WC_NOT_DIRECTORY) {
svn_error_clear(err);
SVN_ERR(svn_wc_adm_open_anchor(&anchor_access, &target_access, &target,
path, FALSE,
SVN_DEPTH_IS_RECURSIVE(depth) ? -1 : 1,
ctx->cancel_func, ctx->cancel_baton,
pool));
} else if (!err) {
target = "";
target_access = anchor_access;
} else
return err;
anchor = svn_wc_adm_access_path(anchor_access);
SVN_ERR(svn_wc_get_default_ignores(&ignores, ctx->config, pool));
SVN_ERR(svn_wc_get_status_editor3(&editor, &edit_baton, &set_locks_baton,
&edit_revision, anchor_access, target,
depth, get_all, no_ignore, ignores,
tweak_status, &sb, ctx->cancel_func,
ctx->cancel_baton, traversal_info,
pool));
if (update) {
svn_ra_session_t *ra_session;
const char *URL;
svn_node_kind_t kind;
svn_boolean_t server_supports_depth;
if (! entry)
SVN_ERR(svn_wc__entry_versioned(&entry, anchor, anchor_access, FALSE,
pool));
if (! entry->url)
return svn_error_createf
(SVN_ERR_ENTRY_MISSING_URL, NULL,
_("Entry '%s' has no URL"),
svn_path_local_style(anchor, pool));
URL = apr_pstrdup(pool, entry->url);
SVN_ERR(svn_client__open_ra_session_internal(&ra_session, URL, anchor,
anchor_access, NULL,
FALSE, TRUE,
ctx, pool));
SVN_ERR(svn_ra_check_path(ra_session, "", SVN_INVALID_REVNUM,
&kind, pool));
if (kind == svn_node_none) {
if (entry->schedule != svn_wc_schedule_add)
sb.deleted_in_repos = TRUE;
SVN_ERR(editor->close_edit(edit_baton, pool));
} else {
svn_revnum_t revnum;
report_baton_t rb;
if (revision->kind == svn_opt_revision_head) {
revnum = SVN_INVALID_REVNUM;
} else {
SVN_ERR(svn_client__get_revision_number
(&revnum, NULL, ra_session, revision, target, pool));
}
SVN_ERR(svn_ra_do_status2(ra_session, &rb.wrapped_reporter,
&rb.wrapped_report_baton,
target, revnum, depth, editor,
edit_baton, pool));
rb.ancestor = apr_pstrdup(pool, URL);
rb.set_locks_baton = set_locks_baton;
rb.ctx = ctx;
rb.pool = pool;
SVN_ERR(svn_ra_has_capability(ra_session, &server_supports_depth,
SVN_RA_CAPABILITY_DEPTH, pool));
SVN_ERR(svn_wc_crawl_revisions3(path, target_access,
&lock_fetch_reporter, &rb, FALSE,
depth, (! server_supports_depth),
FALSE, NULL, NULL, NULL, pool));
}
} else {
SVN_ERR(editor->close_edit(edit_baton, pool));
}
if (ctx->notify_func2 && update) {
svn_wc_notify_t *notify
= svn_wc_create_notify(path, svn_wc_notify_status_completed, pool);
notify->revision = edit_revision;
(ctx->notify_func2)(ctx->notify_baton2, notify, pool);
}
if (result_rev)
*result_rev = edit_revision;
SVN_ERR(svn_wc_adm_close(anchor_access));
if (SVN_DEPTH_IS_RECURSIVE(depth) && (! ignore_externals))
SVN_ERR(svn_client__do_external_status(traversal_info, status_func,
status_baton, depth, get_all,
update, no_ignore, ctx, pool));
return SVN_NO_ERROR;
}
svn_error_t *
svn_client_status2(svn_revnum_t *result_rev,
const char *path,
const svn_opt_revision_t *revision,
svn_wc_status_func2_t status_func,
void *status_baton,
svn_boolean_t recurse,
svn_boolean_t get_all,
svn_boolean_t update,
svn_boolean_t no_ignore,
svn_boolean_t ignore_externals,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
return svn_client_status3(result_rev, path, revision,
status_func, status_baton,
SVN_DEPTH_INFINITY_OR_IMMEDIATES(recurse),
get_all, update, no_ignore, ignore_externals, NULL,
ctx, pool);
}
struct old_status_func_cb_baton {
svn_wc_status_func_t original_func;
void *original_baton;
};
static void old_status_func_cb(void *baton,
const char *path,
svn_wc_status2_t *status) {
struct old_status_func_cb_baton *b = baton;
svn_wc_status_t *stat = (svn_wc_status_t *) status;
b->original_func(b->original_baton, path, stat);
}
svn_error_t *
svn_client_status(svn_revnum_t *result_rev,
const char *path,
svn_opt_revision_t *revision,
svn_wc_status_func_t status_func,
void *status_baton,
svn_boolean_t recurse,
svn_boolean_t get_all,
svn_boolean_t update,
svn_boolean_t no_ignore,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
struct old_status_func_cb_baton *b = apr_pcalloc(pool, sizeof(*b));
b->original_func = status_func;
b->original_baton = status_baton;
return svn_client_status2(result_rev, path, revision,
old_status_func_cb, b,
recurse, get_all, update, no_ignore, FALSE,
ctx, pool);
}