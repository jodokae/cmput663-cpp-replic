#include "svn_client.h"
#include "svn_path.h"
#include "svn_error.h"
#include "cl.h"
#include "svn_private_config.h"
svn_error_t *
svn_cl__import(apr_getopt_t *os,
void *baton,
apr_pool_t *pool) {
svn_cl__opt_state_t *opt_state = ((svn_cl__cmd_baton_t *) baton)->opt_state;
svn_client_ctx_t *ctx = ((svn_cl__cmd_baton_t *) baton)->ctx;
apr_array_header_t *targets;
const char *path;
const char *url;
svn_commit_info_t *commit_info = NULL;
SVN_ERR(svn_cl__args_to_target_array_print_reserved(&targets, os,
opt_state->targets,
pool));
if (targets->nelts < 1)
return svn_error_create
(SVN_ERR_CL_INSUFFICIENT_ARGS, NULL,
_("Repository URL required when importing"));
else if (targets->nelts > 2)
return svn_error_create
(SVN_ERR_CL_ARG_PARSING_ERROR, NULL,
_("Too many arguments to import command"));
else if (targets->nelts == 1) {
url = APR_ARRAY_IDX(targets, 0, const char *);
path = "";
} else {
path = APR_ARRAY_IDX(targets, 0, const char *);
url = APR_ARRAY_IDX(targets, 1, const char *);
}
if (! svn_path_is_url(url))
return svn_error_createf
(SVN_ERR_CL_ARG_PARSING_ERROR, NULL,
_("Invalid URL '%s'"), url);
if (! opt_state->quiet)
svn_cl__get_notifier(&ctx->notify_func2, &ctx->notify_baton2,
FALSE, FALSE, FALSE, pool);
if (opt_state->depth == svn_depth_unknown)
opt_state->depth = svn_depth_infinity;
SVN_ERR(svn_cl__make_log_msg_baton(&(ctx->log_msg_baton3), opt_state,
NULL, ctx->config, pool));
SVN_ERR(svn_cl__cleanup_log_msg
(ctx->log_msg_baton3,
svn_client_import3(&commit_info,
path,
url,
opt_state->depth,
opt_state->no_ignore,
opt_state->force,
opt_state->revprop_table,
ctx,
pool)));
if (commit_info && ! opt_state->quiet)
SVN_ERR(svn_cl__print_commit_info(commit_info, pool));
return SVN_NO_ERROR;
}
