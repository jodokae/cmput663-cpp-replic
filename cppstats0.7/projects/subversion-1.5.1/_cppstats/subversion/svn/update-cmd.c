#include "svn_pools.h"
#include "svn_client.h"
#include "svn_path.h"
#include "svn_error_codes.h"
#include "svn_error.h"
#include "cl.h"
#include "svn_private_config.h"
svn_error_t *
svn_cl__update(apr_getopt_t *os,
void *baton,
apr_pool_t *pool) {
svn_cl__opt_state_t *opt_state = ((svn_cl__cmd_baton_t *) baton)->opt_state;
svn_client_ctx_t *ctx = ((svn_cl__cmd_baton_t *) baton)->ctx;
apr_array_header_t *targets;
svn_depth_t depth;
svn_boolean_t depth_is_sticky;
SVN_ERR(svn_opt_args_to_target_array3(&targets, os,
opt_state->targets, pool));
svn_opt_push_implicit_dot_target(targets, pool);
if (opt_state->changelists) {
svn_depth_t cl_depth = opt_state->depth;
if (cl_depth == svn_depth_unknown)
cl_depth = svn_depth_infinity;
SVN_ERR(svn_cl__changelist_paths(&targets,
opt_state->changelists, targets,
cl_depth, ctx, pool));
}
if (! opt_state->quiet)
svn_cl__get_notifier(&ctx->notify_func2, &ctx->notify_baton2,
FALSE, FALSE, FALSE, pool);
if (opt_state->set_depth != svn_depth_unknown) {
depth = opt_state->set_depth;
depth_is_sticky = TRUE;
} else {
depth = opt_state->depth;
depth_is_sticky = FALSE;
}
SVN_ERR(svn_client_update3(NULL, targets,
&(opt_state->start_revision),
depth, depth_is_sticky,
opt_state->ignore_externals,
opt_state->force,
ctx, pool));
return SVN_NO_ERROR;
}
