#include <assert.h>
#include "svn_client.h"
#include "svn_error.h"
#include "svn_time.h"
#include "svn_path.h"
#include "svn_config.h"
#include "svn_pools.h"
#include "client.h"
#include "svn_private_config.h"
#include "private/svn_wc_private.h"
svn_error_t *
svn_client__switch_internal(svn_revnum_t *result_rev,
const char *path,
const char *switch_url,
const svn_opt_revision_t *peg_revision,
const svn_opt_revision_t *revision,
svn_depth_t depth,
svn_boolean_t depth_is_sticky,
svn_boolean_t *timestamp_sleep,
svn_boolean_t ignore_externals,
svn_boolean_t allow_unver_obstructions,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
const svn_ra_reporter3_t *reporter;
void *report_baton;
const svn_wc_entry_t *entry;
const char *URL, *anchor, *target, *source_root, *switch_rev_url;
svn_ra_session_t *ra_session;
svn_revnum_t revnum;
svn_error_t *err = SVN_NO_ERROR;
svn_wc_adm_access_t *adm_access, *dir_access;
const char *diff3_cmd;
svn_boolean_t use_commit_times;
svn_boolean_t sleep_here;
svn_boolean_t *use_sleep = timestamp_sleep ? timestamp_sleep : &sleep_here;
const svn_delta_editor_t *switch_editor;
void *switch_edit_baton;
svn_wc_traversal_info_t *traversal_info = svn_wc_init_traversal_info(pool);
const char *preserved_exts_str;
apr_array_header_t *preserved_exts;
svn_boolean_t server_supports_depth;
svn_config_t *cfg = ctx->config ? apr_hash_get(ctx->config,
SVN_CONFIG_CATEGORY_CONFIG,
APR_HASH_KEY_STRING)
: NULL;
if (depth == svn_depth_unknown)
depth_is_sticky = FALSE;
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
assert(path);
assert(switch_url && (switch_url[0] != '\0'));
SVN_ERR(svn_wc_adm_open_anchor(&adm_access, &dir_access, &target, path,
TRUE, -1, ctx->cancel_func,
ctx->cancel_baton, pool));
anchor = svn_wc_adm_access_path(adm_access);
SVN_ERR(svn_wc__entry_versioned(&entry, anchor, adm_access, FALSE, pool));
if (! entry->url)
return svn_error_createf(SVN_ERR_ENTRY_MISSING_URL, NULL,
_("Directory '%s' has no URL"),
svn_path_local_style(anchor, pool));
URL = apr_pstrdup(pool, entry->url);
SVN_ERR(svn_client__ra_session_from_path(&ra_session, &revnum,
&switch_rev_url,
switch_url, adm_access,
peg_revision, revision,
ctx, pool));
SVN_ERR(svn_ra_get_repos_root2(ra_session, &source_root, pool));
if (! svn_path_is_ancestor(source_root, URL))
return svn_error_createf
(SVN_ERR_WC_INVALID_SWITCH, NULL,
_("'%s'\n"
"is not the same repository as\n"
"'%s'"), URL, source_root);
SVN_ERR(svn_ra_reparent(ra_session, URL, pool));
SVN_ERR(svn_wc_get_switch_editor3(&revnum, adm_access, target,
switch_rev_url, use_commit_times, depth,
depth_is_sticky, allow_unver_obstructions,
ctx->notify_func2, ctx->notify_baton2,
ctx->cancel_func, ctx->cancel_baton,
ctx->conflict_func, ctx->conflict_baton,
diff3_cmd, preserved_exts,
&switch_editor, &switch_edit_baton,
traversal_info, pool));
SVN_ERR(svn_ra_do_switch2(ra_session, &reporter, &report_baton, revnum,
target, depth, switch_rev_url,
switch_editor, switch_edit_baton, pool));
SVN_ERR(svn_ra_has_capability(ra_session, &server_supports_depth,
SVN_RA_CAPABILITY_DEPTH, pool));
err = svn_wc_crawl_revisions3(path, dir_access, reporter, report_baton,
TRUE, depth, (! server_supports_depth),
use_commit_times,
ctx->notify_func2, ctx->notify_baton2,
NULL,
pool);
if (err) {
svn_sleep_for_timestamps();
return err;
}
*use_sleep = TRUE;
if (SVN_DEPTH_IS_RECURSIVE(depth) && (! ignore_externals))
err = svn_client__handle_externals(traversal_info, switch_url, path,
source_root, depth, FALSE, use_sleep,
ctx, pool);
if (sleep_here)
svn_sleep_for_timestamps();
if (err)
return err;
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
svn_client_switch2(svn_revnum_t *result_rev,
const char *path,
const char *switch_url,
const svn_opt_revision_t *peg_revision,
const svn_opt_revision_t *revision,
svn_depth_t depth,
svn_boolean_t depth_is_sticky,
svn_boolean_t ignore_externals,
svn_boolean_t allow_unver_obstructions,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
return svn_client__switch_internal(result_rev, path, switch_url,
peg_revision, revision, depth,
depth_is_sticky, NULL, ignore_externals,
allow_unver_obstructions, ctx, pool);
}
svn_error_t *
svn_client_switch(svn_revnum_t *result_rev,
const char *path,
const char *switch_url,
const svn_opt_revision_t *revision,
svn_boolean_t recurse,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
svn_opt_revision_t peg_revision;
peg_revision.kind = svn_opt_revision_unspecified;
return svn_client__switch_internal(result_rev, path, switch_url,
&peg_revision, revision,
SVN_DEPTH_INFINITY_OR_FILES(recurse),
FALSE, NULL, FALSE, FALSE, ctx, pool);
}
