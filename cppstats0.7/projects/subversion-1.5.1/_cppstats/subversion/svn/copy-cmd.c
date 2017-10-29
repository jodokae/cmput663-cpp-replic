#include "svn_client.h"
#include "svn_path.h"
#include "svn_error.h"
#include "cl.h"
#include "svn_private_config.h"
svn_error_t *
svn_cl__copy(apr_getopt_t *os,
void *baton,
apr_pool_t *pool) {
svn_cl__opt_state_t *opt_state = ((svn_cl__cmd_baton_t *) baton)->opt_state;
svn_client_ctx_t *ctx = ((svn_cl__cmd_baton_t *) baton)->ctx;
apr_array_header_t *targets, *sources;
const char *src_path, *dst_path;
svn_boolean_t srcs_are_urls, dst_is_url;
svn_commit_info_t *commit_info = NULL;
svn_error_t *err;
int i;
SVN_ERR(svn_cl__args_to_target_array_print_reserved(&targets, os,
opt_state->targets,
pool));
if (targets->nelts < 2)
return svn_error_create(SVN_ERR_CL_INSUFFICIENT_ARGS, 0, NULL);
sources = apr_array_make(pool, targets->nelts - 1,
sizeof(svn_client_copy_source_t *));
for (i = 0; i < (targets->nelts - 1); i++) {
const char *target = APR_ARRAY_IDX(targets, i, const char *);
svn_client_copy_source_t *source = apr_palloc(pool, sizeof(*source));
const char *src;
svn_opt_revision_t *peg_revision = apr_palloc(pool,
sizeof(*peg_revision));
SVN_ERR(svn_opt_parse_path(peg_revision, &src, target, pool));
source->path = src;
source->revision = &(opt_state->start_revision);
source->peg_revision = peg_revision;
APR_ARRAY_PUSH(sources, svn_client_copy_source_t *) = source;
}
src_path = APR_ARRAY_IDX(targets, 0, const char *);
srcs_are_urls = svn_path_is_url(src_path);
dst_path = APR_ARRAY_IDX(targets, targets->nelts - 1, const char *);
apr_array_pop(targets);
dst_is_url = svn_path_is_url(dst_path);
if ((! srcs_are_urls) && (! dst_is_url)) {
if (! opt_state->quiet)
svn_cl__get_notifier(&ctx->notify_func2, &ctx->notify_baton2,
FALSE, FALSE, FALSE, pool);
} else if ((! srcs_are_urls) && (dst_is_url)) {
} else if ((srcs_are_urls) && (! dst_is_url)) {
if (! opt_state->quiet)
svn_cl__get_notifier(&ctx->notify_func2, &ctx->notify_baton2, TRUE,
FALSE, FALSE, pool);
}
if (! dst_is_url) {
ctx->log_msg_func3 = NULL;
if (opt_state->message || opt_state->filedata || opt_state->revprop_table)
return svn_error_create
(SVN_ERR_CL_UNNECESSARY_LOG_MESSAGE, NULL,
_("Local, non-commit operations do not take a log message "
"or revision properties"));
}
if (ctx->log_msg_func3)
SVN_ERR(svn_cl__make_log_msg_baton(&(ctx->log_msg_baton3), opt_state,
NULL, ctx->config, pool));
err = svn_client_copy4(&commit_info, sources, dst_path, TRUE,
opt_state->parents, opt_state->revprop_table,
ctx, pool);
if (ctx->log_msg_func3)
SVN_ERR(svn_cl__cleanup_log_msg(ctx->log_msg_baton3, err));
else if (err)
return err;
if (commit_info && ! opt_state->quiet)
SVN_ERR(svn_cl__print_commit_info(commit_info, pool));
return SVN_NO_ERROR;
}