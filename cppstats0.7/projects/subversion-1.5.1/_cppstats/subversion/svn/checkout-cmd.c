#include "svn_client.h"
#include "svn_path.h"
#include "svn_error.h"
#include "svn_pools.h"
#include "cl.h"
#include "svn_private_config.h"
svn_error_t *
svn_cl__checkout(apr_getopt_t *os,
void *baton,
apr_pool_t *pool) {
svn_cl__opt_state_t *opt_state = ((svn_cl__cmd_baton_t *) baton)->opt_state;
svn_client_ctx_t *ctx = ((svn_cl__cmd_baton_t *) baton)->ctx;
apr_pool_t *subpool;
apr_array_header_t *targets;
const char *local_dir;
const char *repos_url;
int i;
SVN_ERR(svn_cl__args_to_target_array_print_reserved(&targets, os,
opt_state->targets,
pool));
if (! targets->nelts)
return svn_error_create(SVN_ERR_CL_INSUFFICIENT_ARGS, 0, NULL);
local_dir = APR_ARRAY_IDX(targets, targets->nelts - 1, const char *);
if (svn_path_is_url(local_dir)) {
if (targets->nelts == 1) {
svn_opt_revision_t pegrev;
SVN_ERR(svn_opt_parse_path(&pegrev, &local_dir, local_dir, pool));
if (pegrev.kind != svn_opt_revision_unspecified)
local_dir = svn_path_canonicalize(local_dir, pool);
local_dir = svn_path_basename(local_dir, pool);
local_dir = svn_path_uri_decode(local_dir, pool);
} else {
local_dir = "";
}
APR_ARRAY_PUSH(targets, const char *) = local_dir;
} else {
if (targets->nelts == 1)
return svn_error_create(SVN_ERR_CL_ARG_PARSING_ERROR, 0, NULL);
}
if (! opt_state->quiet)
svn_cl__get_notifier(&ctx->notify_func2, &ctx->notify_baton2, TRUE, FALSE,
FALSE, pool);
subpool = svn_pool_create(pool);
for (i = 0; i < targets->nelts - 1; ++i) {
const char *target_dir;
const char *true_url;
svn_opt_revision_t revision = opt_state->start_revision;
svn_opt_revision_t peg_revision;
svn_pool_clear(subpool);
SVN_ERR(svn_cl__check_cancel(ctx->cancel_baton));
repos_url = APR_ARRAY_IDX(targets, i, const char *);
if (! svn_path_is_url(repos_url))
return svn_error_createf
(SVN_ERR_BAD_URL, NULL,
_("'%s' does not appear to be a URL"), repos_url);
SVN_ERR(svn_opt_parse_path(&peg_revision, &true_url, repos_url,
subpool));
true_url = svn_path_canonicalize(true_url, subpool);
if (targets->nelts == 2) {
target_dir = local_dir;
} else {
target_dir = svn_path_basename(true_url, subpool);
target_dir = svn_path_uri_decode(target_dir, subpool);
target_dir = svn_path_join(local_dir, target_dir, subpool);
}
if (revision.kind == svn_opt_revision_unspecified) {
if (peg_revision.kind != svn_opt_revision_unspecified)
revision = peg_revision;
else
revision.kind = svn_opt_revision_head;
}
SVN_ERR(svn_client_checkout3
(NULL, true_url, target_dir,
&peg_revision,
&revision,
opt_state->depth,
opt_state->ignore_externals,
opt_state->force,
ctx, subpool));
}
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}