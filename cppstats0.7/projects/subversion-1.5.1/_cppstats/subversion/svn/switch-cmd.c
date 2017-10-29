#include "svn_wc.h"
#include "svn_client.h"
#include "svn_path.h"
#include "svn_error.h"
#include "svn_pools.h"
#include "cl.h"
#include "svn_private_config.h"
static svn_error_t *
rewrite_urls(apr_array_header_t *targets,
svn_boolean_t recurse,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
apr_pool_t *subpool;
const char *from;
const char *to;
int i;
if (targets->nelts < 2)
return svn_error_create(SVN_ERR_CL_INSUFFICIENT_ARGS, 0, NULL);
from = APR_ARRAY_IDX(targets, 0, const char *);
to = APR_ARRAY_IDX(targets, 1, const char *);
if (svn_path_is_url(from) != svn_path_is_url(to))
return svn_error_createf
(SVN_ERR_INCORRECT_PARAMS, NULL,
_("'%s' to '%s' is not a valid relocation"), from, to);
subpool = svn_pool_create(pool);
if (targets->nelts == 2) {
SVN_ERR(svn_client_relocate("", from, to, recurse, ctx, pool));
} else {
for (i = 2; i < targets->nelts; i++) {
const char *target = APR_ARRAY_IDX(targets, i, const char *);
svn_pool_clear(subpool);
SVN_ERR(svn_client_relocate(target, from, to, recurse,
ctx, subpool));
}
}
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
svn_error_t *
svn_cl__switch(apr_getopt_t *os,
void *baton,
apr_pool_t *pool) {
svn_cl__opt_state_t *opt_state = ((svn_cl__cmd_baton_t *) baton)->opt_state;
svn_client_ctx_t *ctx = ((svn_cl__cmd_baton_t *) baton)->ctx;
apr_array_header_t *targets;
const char *target = NULL, *switch_url = NULL;
svn_wc_adm_access_t *adm_access;
const svn_wc_entry_t *entry;
const char *parent_dir, *base_tgt, *true_path;
svn_opt_revision_t peg_revision;
svn_depth_t depth;
svn_boolean_t depth_is_sticky;
SVN_ERR(svn_cl__args_to_target_array_print_reserved(&targets, os,
opt_state->targets,
pool));
if (opt_state->relocate)
return rewrite_urls(targets,
SVN_DEPTH_IS_RECURSIVE(opt_state->depth),
ctx, pool);
if (targets->nelts < 1)
return svn_error_create(SVN_ERR_CL_INSUFFICIENT_ARGS, 0, NULL);
if (targets->nelts > 2)
return svn_error_create(SVN_ERR_CL_ARG_PARSING_ERROR, 0, NULL);
if (targets->nelts == 1) {
switch_url = APR_ARRAY_IDX(targets, 0, const char *);
target = "";
} else {
switch_url = APR_ARRAY_IDX(targets, 0, const char *);
target = APR_ARRAY_IDX(targets, 1, const char *);
}
SVN_ERR(svn_opt_parse_path(&peg_revision, &true_path, switch_url, pool));
APR_ARRAY_IDX(targets, 0, const char *) = true_path;
switch_url = true_path;
if (! svn_path_is_url(switch_url))
return svn_error_createf
(SVN_ERR_BAD_URL, NULL,
_("'%s' does not appear to be a URL"), switch_url);
switch_url = svn_path_canonicalize(switch_url, pool);
SVN_ERR(svn_wc_adm_probe_open3(&adm_access, NULL, target, FALSE, 0,
ctx->cancel_func, ctx->cancel_baton,
pool));
SVN_ERR(svn_wc_entry(&entry, target, adm_access, FALSE, pool));
if (! entry)
return svn_error_createf
(SVN_ERR_ENTRY_NOT_FOUND, NULL,
_("'%s' does not appear to be a working copy path"), target);
if (entry->kind == svn_node_file)
SVN_ERR(svn_wc_get_actual_target(target, &parent_dir, &base_tgt, pool));
else if (entry->kind == svn_node_dir)
parent_dir = target;
if (! opt_state->quiet)
svn_cl__get_notifier(&ctx->notify_func2, &ctx->notify_baton2, FALSE,
FALSE, FALSE, pool);
if (opt_state->set_depth != svn_depth_unknown) {
depth = opt_state->set_depth;
depth_is_sticky = TRUE;
} else {
depth = opt_state->depth;
depth_is_sticky = FALSE;
}
SVN_ERR(svn_client_switch2(NULL, target, switch_url, &peg_revision,
&(opt_state->start_revision), depth,
depth_is_sticky, opt_state->ignore_externals,
opt_state->force, ctx, pool));
return SVN_NO_ERROR;
}