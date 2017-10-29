#include "svn_client.h"
#include "svn_error_codes.h"
#include "svn_error.h"
#include "cl.h"
#include "svn_private_config.h"
svn_error_t *
svn_cl__revert(apr_getopt_t *os,
void *baton,
apr_pool_t *pool) {
svn_cl__opt_state_t *opt_state = ((svn_cl__cmd_baton_t *) baton)->opt_state;
svn_client_ctx_t *ctx = ((svn_cl__cmd_baton_t *) baton)->ctx;
apr_array_header_t *targets = NULL;
svn_error_t *err;
SVN_ERR(svn_cl__args_to_target_array_print_reserved(&targets, os,
opt_state->targets,
pool));
if (! targets->nelts)
return svn_error_create(SVN_ERR_CL_INSUFFICIENT_ARGS, 0, NULL);
if (! opt_state->quiet)
svn_cl__get_notifier(&ctx->notify_func2, &ctx->notify_baton2, FALSE,
FALSE, FALSE, pool);
if (opt_state->depth == svn_depth_unknown)
opt_state->depth = svn_depth_empty;
err = svn_client_revert2(targets, opt_state->depth,
opt_state->changelists, ctx, pool);
if (err
&& (err->apr_err == SVN_ERR_WC_NOT_LOCKED)
&& (! SVN_DEPTH_IS_RECURSIVE(opt_state->depth))) {
err = svn_error_quick_wrap
(err, _("Try 'svn revert --depth infinity' instead?"));
}
return err;
}