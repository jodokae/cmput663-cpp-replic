#include "svn_time.h"
#include "svn_wc.h"
#include "svn_client.h"
#include "svn_config.h"
#include "client.h"
#include "svn_private_config.h"
svn_error_t *
svn_client_cleanup(const char *dir,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
const char *diff3_cmd;
svn_error_t *err;
svn_config_t *cfg = ctx->config
? apr_hash_get(ctx->config, SVN_CONFIG_CATEGORY_CONFIG,
APR_HASH_KEY_STRING)
: NULL;
svn_config_get(cfg, &diff3_cmd, SVN_CONFIG_SECTION_HELPERS,
SVN_CONFIG_OPTION_DIFF3_CMD, NULL);
err = svn_wc_cleanup2(dir, diff3_cmd, ctx->cancel_func, ctx->cancel_baton,
pool);
svn_sleep_for_timestamps();
return err;
}
