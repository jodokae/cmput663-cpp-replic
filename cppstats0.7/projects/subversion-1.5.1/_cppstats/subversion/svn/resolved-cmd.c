#define APR_WANT_STDIO
#include <apr_want.h>
#include "svn_client.h"
#include "svn_error.h"
#include "svn_pools.h"
#include "cl.h"
#include "svn_private_config.h"
svn_error_t *
svn_cl__resolved(apr_getopt_t *os,
void *baton,
apr_pool_t *pool) {
svn_cl__opt_state_t *opt_state = ((svn_cl__cmd_baton_t *) baton)->opt_state;
svn_client_ctx_t *ctx = ((svn_cl__cmd_baton_t *) baton)->ctx;
svn_error_t *err;
apr_array_header_t *targets;
int i;
apr_pool_t *subpool;
SVN_ERR(svn_cl__args_to_target_array_print_reserved(&targets, os,
opt_state->targets,
pool));
if (! targets->nelts)
return svn_error_create(SVN_ERR_CL_INSUFFICIENT_ARGS, 0, NULL);
subpool = svn_pool_create(pool);
if (! opt_state->quiet)
svn_cl__get_notifier(&ctx->notify_func2, &ctx->notify_baton2, FALSE,
FALSE, FALSE, pool);
if (opt_state->depth == svn_depth_unknown)
opt_state->depth = svn_depth_empty;
for (i = 0; i < targets->nelts; i++) {
const char *target = APR_ARRAY_IDX(targets, i, const char *);
svn_pool_clear(subpool);
SVN_ERR(svn_cl__check_cancel(ctx->cancel_baton));
err = svn_client_resolve(target,
opt_state->depth,
svn_wc_conflict_choose_merged,
ctx,
subpool);
if (err) {
svn_handle_warning(stderr, err);
svn_error_clear(err);
}
}
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
