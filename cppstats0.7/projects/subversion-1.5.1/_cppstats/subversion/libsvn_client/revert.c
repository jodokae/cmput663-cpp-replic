#include "svn_wc.h"
#include "svn_client.h"
#include "svn_pools.h"
#include "svn_error.h"
#include "svn_time.h"
#include "svn_config.h"
#include "client.h"
#include "private/svn_wc_private.h"
static svn_error_t *
revert(const char *path,
svn_depth_t depth,
svn_boolean_t use_commit_times,
const apr_array_header_t *changelists,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
svn_wc_adm_access_t *adm_access, *target_access;
const char *target;
svn_error_t *err;
int adm_lock_level = SVN_WC__LEVELS_TO_LOCK_FROM_DEPTH(depth);
SVN_ERR(svn_wc_adm_open_anchor(&adm_access, &target_access, &target, path,
TRUE, adm_lock_level,
ctx->cancel_func, ctx->cancel_baton,
pool));
err = svn_wc_revert3(path, adm_access, depth, use_commit_times, changelists,
ctx->cancel_func, ctx->cancel_baton,
ctx->notify_func2, ctx->notify_baton2,
pool);
if (err) {
if (err->apr_err == SVN_ERR_ENTRY_NOT_FOUND
|| err->apr_err == SVN_ERR_UNVERSIONED_RESOURCE) {
if (ctx->notify_func2)
(*ctx->notify_func2)
(ctx->notify_baton2,
svn_wc_create_notify(path, svn_wc_notify_skip, pool),
pool);
svn_error_clear(err);
} else
return err;
}
SVN_ERR(svn_wc_adm_close(adm_access));
return SVN_NO_ERROR;
}
svn_error_t *
svn_client_revert2(const apr_array_header_t *paths,
svn_depth_t depth,
const apr_array_header_t *changelists,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
apr_pool_t *subpool;
svn_error_t *err = SVN_NO_ERROR;
int i;
svn_config_t *cfg;
svn_boolean_t use_commit_times;
cfg = ctx->config ? apr_hash_get(ctx->config, SVN_CONFIG_CATEGORY_CONFIG,
APR_HASH_KEY_STRING) : NULL;
SVN_ERR(svn_config_get_bool(cfg, &use_commit_times,
SVN_CONFIG_SECTION_MISCELLANY,
SVN_CONFIG_OPTION_USE_COMMIT_TIMES,
FALSE));
subpool = svn_pool_create(pool);
for (i = 0; i < paths->nelts; i++) {
const char *path = APR_ARRAY_IDX(paths, i, const char *);
svn_pool_clear(subpool);
if ((ctx->cancel_func)
&& ((err = ctx->cancel_func(ctx->cancel_baton))))
goto errorful;
err = revert(path, depth, use_commit_times, changelists, ctx, subpool);
if (err)
goto errorful;
}
errorful:
svn_pool_destroy(subpool);
svn_sleep_for_timestamps();
return err;
}
svn_error_t *
svn_client_revert(const apr_array_header_t *paths,
svn_boolean_t recursive,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
return svn_client_revert2(paths,
recursive ? svn_depth_infinity : svn_depth_empty,
NULL, ctx, pool);
}