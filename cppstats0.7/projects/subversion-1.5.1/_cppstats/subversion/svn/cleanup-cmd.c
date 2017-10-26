#include "svn_client.h"
#include "svn_pools.h"
#include "svn_error.h"
#include "cl.h"
svn_error_t *
svn_cl__cleanup(apr_getopt_t *os,
void *baton,
apr_pool_t *pool) {
svn_cl__opt_state_t *opt_state = ((svn_cl__cmd_baton_t *) baton)->opt_state;
svn_client_ctx_t *ctx = ((svn_cl__cmd_baton_t *) baton)->ctx;
apr_array_header_t *targets;
apr_pool_t *subpool;
int i;
SVN_ERR(svn_cl__args_to_target_array_print_reserved(&targets, os,
opt_state->targets,
pool));
svn_opt_push_implicit_dot_target(targets, pool);
subpool = svn_pool_create(pool);
for (i = 0; i < targets->nelts; i++) {
const char *target = APR_ARRAY_IDX(targets, i, const char *);
svn_pool_clear(subpool);
SVN_ERR(svn_cl__check_cancel(ctx->cancel_baton));
SVN_ERR(svn_client_cleanup(target, ctx, subpool));
}
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
