#include "svn_client.h"
#include "svn_error_codes.h"
#include "svn_error.h"
#include "cl.h"
#include "svn_private_config.h"
svn_error_t *
svn_cl__changelist(apr_getopt_t *os,
void *baton,
apr_pool_t *pool) {
const char *changelist_name = NULL;
svn_cl__opt_state_t *opt_state = ((svn_cl__cmd_baton_t *) baton)->opt_state;
svn_client_ctx_t *ctx = ((svn_cl__cmd_baton_t *) baton)->ctx;
apr_array_header_t *targets;
svn_depth_t depth = opt_state->depth;
if (! opt_state->remove) {
apr_array_header_t *args;
SVN_ERR(svn_opt_parse_num_args(&args, os, 1, pool));
changelist_name = APR_ARRAY_IDX(args, 0, const char *);
SVN_ERR(svn_utf_cstring_to_utf8(&changelist_name,
changelist_name, pool));
}
SVN_ERR(svn_cl__args_to_target_array_print_reserved(&targets, os,
opt_state->targets,
pool));
if (! targets->nelts)
return svn_error_create(SVN_ERR_CL_INSUFFICIENT_ARGS, 0, NULL);
if (! opt_state->quiet)
svn_cl__get_notifier(&ctx->notify_func2, &ctx->notify_baton2, FALSE,
FALSE, FALSE, pool);
else
ctx->notify_func2 = NULL;
if (depth == svn_depth_unknown)
depth = svn_depth_empty;
if (changelist_name) {
SVN_ERR(svn_cl__try
(svn_client_add_to_changelist(targets, changelist_name,
depth, opt_state->changelists,
ctx, pool),
NULL, opt_state->quiet,
SVN_ERR_UNVERSIONED_RESOURCE,
SVN_ERR_WC_PATH_NOT_FOUND,
SVN_NO_ERROR));
} else {
SVN_ERR(svn_cl__try
(svn_client_remove_from_changelists(targets, depth,
opt_state->changelists,
ctx, pool),
NULL, opt_state->quiet,
SVN_ERR_UNVERSIONED_RESOURCE,
SVN_ERR_WC_PATH_NOT_FOUND,
SVN_NO_ERROR));
}
return SVN_NO_ERROR;
}
