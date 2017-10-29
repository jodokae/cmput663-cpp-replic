#include "svn_cmdline.h"
#include "svn_pools.h"
#include "svn_client.h"
#include "svn_error_codes.h"
#include "svn_error.h"
#include "svn_utf.h"
#include "svn_path.h"
#include "cl.h"
#include "svn_private_config.h"
svn_error_t *
svn_cl__propdel(apr_getopt_t *os,
void *baton,
apr_pool_t *pool) {
svn_cl__opt_state_t *opt_state = ((svn_cl__cmd_baton_t *) baton)->opt_state;
svn_client_ctx_t *ctx = ((svn_cl__cmd_baton_t *) baton)->ctx;
const char *pname, *pname_utf8;
apr_array_header_t *args, *targets;
int i;
SVN_ERR(svn_opt_parse_num_args(&args, os, 1, pool));
pname = APR_ARRAY_IDX(args, 0, const char *);
SVN_ERR(svn_utf_cstring_to_utf8(&pname_utf8, pname, pool));
SVN_ERR(svn_cl__args_to_target_array_print_reserved(&targets, os,
opt_state->targets,
pool));
svn_opt_push_implicit_dot_target(targets, pool);
if (opt_state->revprop) {
svn_revnum_t rev;
const char *URL;
SVN_ERR(svn_cl__revprop_prepare(&opt_state->start_revision, targets,
&URL, pool));
SVN_ERR(svn_client_revprop_set(pname_utf8, NULL,
URL, &(opt_state->start_revision),
&rev, FALSE, ctx, pool));
if (! opt_state->quiet) {
SVN_ERR(svn_cmdline_printf(pool,
_("property '%s' deleted from"
" repository revision %ld\n"),
pname_utf8, rev));
}
} else if (opt_state->start_revision.kind != svn_opt_revision_unspecified) {
return svn_error_createf
(SVN_ERR_CL_ARG_PARSING_ERROR, NULL,
_("Cannot specify revision for deleting versioned property '%s'"),
pname);
} else {
apr_pool_t *subpool = svn_pool_create(pool);
if (opt_state->depth == svn_depth_unknown)
opt_state->depth = svn_depth_empty;
for (i = 0; i < targets->nelts; i++) {
const char *target = APR_ARRAY_IDX(targets, i, const char *);
svn_commit_info_t *commit_info;
svn_boolean_t success;
svn_pool_clear(subpool);
SVN_ERR(svn_cl__check_cancel(ctx->cancel_baton));
SVN_ERR(svn_cl__try(svn_client_propset3
(&commit_info, pname_utf8,
NULL, target,
opt_state->depth,
FALSE, SVN_INVALID_REVNUM,
opt_state->changelists, NULL,
ctx, subpool),
&success, opt_state->quiet,
SVN_ERR_UNVERSIONED_RESOURCE,
SVN_ERR_ENTRY_NOT_FOUND,
SVN_NO_ERROR));
if (success && (! opt_state->quiet)) {
SVN_ERR(svn_cmdline_printf
(subpool,
SVN_DEPTH_IS_RECURSIVE(opt_state->depth)
? _("property '%s' deleted (recursively) from '%s'.\n")
: _("property '%s' deleted from '%s'.\n"),
pname_utf8, svn_path_local_style(target, subpool)));
}
}
svn_pool_destroy(subpool);
}
return SVN_NO_ERROR;
}