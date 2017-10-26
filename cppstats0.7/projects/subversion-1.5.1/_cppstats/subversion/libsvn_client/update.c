#include <assert.h>
#include "svn_wc.h"
#include "svn_client.h"
#include "svn_error.h"
#include "svn_config.h"
#include "svn_time.h"
#include "svn_path.h"
#include "svn_pools.h"
#include "svn_io.h"
#include "client.h"
#include "svn_private_config.h"
#include "private/svn_wc_private.h"
struct ff_baton {
svn_client_ctx_t *ctx;
const char *repos_root;
svn_ra_session_t *session;
apr_pool_t *pool;
};
static svn_error_t *
file_fetcher(void *baton,
const char *path,
svn_revnum_t revision,
svn_stream_t *stream,
svn_revnum_t *fetched_rev,
apr_hash_t **props,
apr_pool_t *pool) {
struct ff_baton *ffb = (struct ff_baton *)baton;
if (! ffb->session)
SVN_ERR(svn_client__open_ra_session_internal(&(ffb->session),
ffb->repos_root,
NULL, NULL, NULL,
FALSE, TRUE,
ffb->ctx, ffb->pool));
SVN_ERR(svn_ra_get_file(ffb->session, path, revision, stream,
fetched_rev, props, pool));
return SVN_NO_ERROR;
}
svn_error_t *
svn_client__update_internal(svn_revnum_t *result_rev,
const char *path,
const svn_opt_revision_t *revision,
svn_depth_t depth,
svn_boolean_t depth_is_sticky,
svn_boolean_t ignore_externals,
svn_boolean_t allow_unver_obstructions,
svn_boolean_t *timestamp_sleep,
svn_boolean_t send_copyfrom_args,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
const svn_delta_editor_t *update_editor;
void *update_edit_baton;
const svn_ra_reporter3_t *reporter;
void *report_baton;
const svn_wc_entry_t *entry;
const char *anchor, *target;
const char *repos_root;
svn_error_t *err;
svn_revnum_t revnum;
int levels_to_lock;
svn_wc_traversal_info_t *traversal_info = svn_wc_init_traversal_info(pool);
svn_wc_adm_access_t *adm_access;
svn_boolean_t use_commit_times;
svn_boolean_t sleep_here = FALSE;
svn_boolean_t *use_sleep = timestamp_sleep ? timestamp_sleep : &sleep_here;
const char *diff3_cmd;
svn_ra_session_t *ra_session;
svn_wc_adm_access_t *dir_access;
const char *preserved_exts_str;
apr_array_header_t *preserved_exts;
struct ff_baton *ffb;
svn_boolean_t server_supports_depth;
svn_config_t *cfg = ctx->config ? apr_hash_get(ctx->config,
SVN_CONFIG_CATEGORY_CONFIG,
APR_HASH_KEY_STRING) : NULL;
if (depth == svn_depth_unknown)
depth_is_sticky = FALSE;
levels_to_lock = SVN_WC__LEVELS_TO_LOCK_FROM_DEPTH(depth);
assert(path);
if (svn_path_is_url(path))
return svn_error_createf(SVN_ERR_WC_NOT_DIRECTORY, NULL,
_("Path '%s' is not a directory"),
path);
SVN_ERR(svn_wc_adm_open_anchor(&adm_access, &dir_access, &target, path,
TRUE, levels_to_lock,
ctx->cancel_func, ctx->cancel_baton,
pool));
anchor = svn_wc_adm_access_path(adm_access);
SVN_ERR(svn_wc_entry(&entry, anchor, adm_access, FALSE, pool));
if (! entry->url)
return svn_error_createf(SVN_ERR_ENTRY_MISSING_URL, NULL,
_("Entry '%s' has no URL"),
svn_path_local_style(anchor, pool));
if (revision->kind == svn_opt_revision_number)
revnum = revision->value.number;
else
revnum = SVN_INVALID_REVNUM;
svn_config_get(cfg, &diff3_cmd, SVN_CONFIG_SECTION_HELPERS,
SVN_CONFIG_OPTION_DIFF3_CMD, NULL);
SVN_ERR(svn_config_get_bool(cfg, &use_commit_times,
SVN_CONFIG_SECTION_MISCELLANY,
SVN_CONFIG_OPTION_USE_COMMIT_TIMES, FALSE));
svn_config_get(cfg, &preserved_exts_str, SVN_CONFIG_SECTION_MISCELLANY,
SVN_CONFIG_OPTION_PRESERVED_CF_EXTS, "");
preserved_exts = *preserved_exts_str
? svn_cstring_split(preserved_exts_str, "\n\r\t\v ", FALSE, pool)
: NULL;
SVN_ERR(svn_client__open_ra_session_internal(&ra_session, entry->url,
anchor, adm_access,
NULL, TRUE, TRUE,
ctx, pool));
SVN_ERR(svn_client__get_revision_number
(&revnum, NULL, ra_session, revision, path, pool));
SVN_ERR(svn_ra_get_repos_root2(ra_session, &repos_root, pool));
SVN_ERR(svn_wc_maybe_set_repos_root(dir_access, path, repos_root, pool));
ffb = apr_pcalloc(pool, sizeof(*ffb));
ffb->ctx = ctx;
ffb->repos_root = repos_root;
ffb->pool = pool;
SVN_ERR(svn_wc_get_update_editor3(&revnum, adm_access, target,
use_commit_times, depth, depth_is_sticky,
allow_unver_obstructions,
ctx->notify_func2, ctx->notify_baton2,
ctx->cancel_func, ctx->cancel_baton,
ctx->conflict_func, ctx->conflict_baton,
file_fetcher, ffb,
diff3_cmd, preserved_exts,
&update_editor, &update_edit_baton,
traversal_info,
pool));
SVN_ERR(svn_ra_do_update2(ra_session,
&reporter, &report_baton,
revnum,
target,
depth,
send_copyfrom_args,
update_editor, update_edit_baton, pool));
SVN_ERR(svn_ra_has_capability(ra_session, &server_supports_depth,
SVN_RA_CAPABILITY_DEPTH, pool));
err = svn_wc_crawl_revisions3(path, dir_access, reporter, report_baton,
TRUE, depth, (! server_supports_depth),
use_commit_times,
ctx->notify_func2, ctx->notify_baton2,
traversal_info, pool);
if (err) {
svn_sleep_for_timestamps();
return err;
}
*use_sleep = TRUE;
if (SVN_DEPTH_IS_RECURSIVE(depth) && (! ignore_externals))
SVN_ERR(svn_client__handle_externals(traversal_info,
entry->url,
anchor,
repos_root,
depth,
TRUE,
use_sleep, ctx, pool));
if (sleep_here)
svn_sleep_for_timestamps();
SVN_ERR(svn_wc_adm_close(adm_access));
if (ctx->notify_func2) {
svn_wc_notify_t *notify
= svn_wc_create_notify(anchor, svn_wc_notify_update_completed, pool);
notify->kind = svn_node_none;
notify->content_state = notify->prop_state
= svn_wc_notify_state_inapplicable;
notify->lock_state = svn_wc_notify_lock_state_inapplicable;
notify->revision = revnum;
(*ctx->notify_func2)(ctx->notify_baton2, notify, pool);
}
if (result_rev)
*result_rev = revnum;
return SVN_NO_ERROR;
}
svn_error_t *
svn_client_update3(apr_array_header_t **result_revs,
const apr_array_header_t *paths,
const svn_opt_revision_t *revision,
svn_depth_t depth,
svn_boolean_t depth_is_sticky,
svn_boolean_t ignore_externals,
svn_boolean_t allow_unver_obstructions,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
int i;
svn_error_t *err = SVN_NO_ERROR;
apr_pool_t *subpool = svn_pool_create(pool);
if (result_revs)
*result_revs = apr_array_make(pool, paths->nelts, sizeof(svn_revnum_t));
for (i = 0; i < paths->nelts; ++i) {
svn_boolean_t sleep;
svn_revnum_t result_rev;
const char *path = APR_ARRAY_IDX(paths, i, const char *);
svn_pool_clear(subpool);
if (ctx->cancel_func && (err = ctx->cancel_func(ctx->cancel_baton)))
break;
err = svn_client__update_internal(&result_rev, path, revision, depth,
depth_is_sticky, ignore_externals,
allow_unver_obstructions,
&sleep, TRUE, ctx, subpool);
if (err && err->apr_err != SVN_ERR_WC_NOT_DIRECTORY) {
return err;
} else if (err) {
svn_error_clear(err);
err = SVN_NO_ERROR;
result_rev = SVN_INVALID_REVNUM;
if (ctx->notify_func2)
(*ctx->notify_func2)(ctx->notify_baton2,
svn_wc_create_notify(path,
svn_wc_notify_skip,
subpool), subpool);
}
if (result_revs)
APR_ARRAY_PUSH(*result_revs, svn_revnum_t) = result_rev;
}
svn_pool_destroy(subpool);
svn_sleep_for_timestamps();
return err;
}
svn_error_t *
svn_client_update2(apr_array_header_t **result_revs,
const apr_array_header_t *paths,
const svn_opt_revision_t *revision,
svn_boolean_t recurse,
svn_boolean_t ignore_externals,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
return svn_client_update3(result_revs, paths, revision,
SVN_DEPTH_INFINITY_OR_FILES(recurse), FALSE,
ignore_externals, FALSE, ctx, pool);
}
svn_error_t *
svn_client_update(svn_revnum_t *result_rev,
const char *path,
const svn_opt_revision_t *revision,
svn_boolean_t recurse,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
return svn_client__update_internal(result_rev, path, revision,
SVN_DEPTH_INFINITY_OR_FILES(recurse),
FALSE, FALSE, FALSE, NULL,
TRUE, ctx, pool);
}
