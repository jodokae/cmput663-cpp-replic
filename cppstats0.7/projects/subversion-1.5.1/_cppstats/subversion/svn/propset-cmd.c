#include "svn_cmdline.h"
#include "svn_pools.h"
#include "svn_client.h"
#include "svn_string.h"
#include "svn_error.h"
#include "svn_utf.h"
#include "svn_subst.h"
#include "svn_path.h"
#include "svn_props.h"
#include "cl.h"
#include "svn_private_config.h"
svn_error_t *
svn_cl__propset(apr_getopt_t *os,
void *baton,
apr_pool_t *pool) {
svn_cl__opt_state_t *opt_state = ((svn_cl__cmd_baton_t *) baton)->opt_state;
svn_client_ctx_t *ctx = ((svn_cl__cmd_baton_t *) baton)->ctx;
const char *pname, *pname_utf8;
svn_string_t *propval = NULL;
svn_boolean_t propval_came_from_cmdline;
apr_array_header_t *args, *targets;
int i;
SVN_ERR(svn_opt_parse_num_args(&args, os,
opt_state->filedata ? 1 : 2, pool));
pname = APR_ARRAY_IDX(args, 0, const char *);
SVN_ERR(svn_utf_cstring_to_utf8(&pname_utf8, pname, pool));
if (! svn_prop_name_is_valid(pname_utf8))
return svn_error_createf(SVN_ERR_CLIENT_PROPERTY_NAME, NULL,
_("'%s' is not a valid Subversion property name"),
pname_utf8);
if (opt_state->filedata) {
propval = svn_string_create_from_buf(opt_state->filedata, pool);
propval_came_from_cmdline = FALSE;
} else {
propval = svn_string_create(APR_ARRAY_IDX(args, 1, const char *), pool);
propval_came_from_cmdline = TRUE;
}
if (svn_prop_needs_translation(pname_utf8))
SVN_ERR(svn_subst_translate_string(&propval, propval,
opt_state->encoding, pool));
else if (opt_state->encoding)
return svn_error_create
(SVN_ERR_UNSUPPORTED_FEATURE, NULL,
_("--encoding option applies only to textual"
" Subversion-controlled properties"));
SVN_ERR(svn_cl__args_to_target_array_print_reserved(&targets, os,
opt_state->targets,
pool));
if (opt_state->revprop)
svn_opt_push_implicit_dot_target(targets, pool);
if (opt_state->revprop) {
svn_revnum_t rev;
const char *URL;
SVN_ERR(svn_cl__revprop_prepare(&opt_state->start_revision, targets,
&URL, pool));
SVN_ERR(svn_client_revprop_set(pname_utf8, propval,
URL, &(opt_state->start_revision),
&rev, opt_state->force, ctx, pool));
if (! opt_state->quiet) {
SVN_ERR
(svn_cmdline_printf
(pool, _("property '%s' set on repository revision %ld\n"),
pname_utf8, rev));
}
} else if (opt_state->start_revision.kind != svn_opt_revision_unspecified) {
return svn_error_createf
(SVN_ERR_CL_ARG_PARSING_ERROR, NULL,
_("Cannot specify revision for setting versioned property '%s'"),
pname);
} else {
apr_pool_t *subpool = svn_pool_create(pool);
if (opt_state->depth == svn_depth_unknown)
opt_state->depth = svn_depth_empty;
if (targets->nelts == 0) {
if (propval_came_from_cmdline) {
return svn_error_createf
(SVN_ERR_CL_INSUFFICIENT_ARGS, NULL,
_("Explicit target required ('%s' interpreted as prop value)"),
propval->data);
} else {
return svn_error_create
(SVN_ERR_CL_INSUFFICIENT_ARGS, NULL,
_("Explicit target argument required"));
}
}
for (i = 0; i < targets->nelts; i++) {
const char *target = APR_ARRAY_IDX(targets, i, const char *);
svn_commit_info_t *commit_info;
svn_boolean_t success;
svn_pool_clear(subpool);
SVN_ERR(svn_cl__check_cancel(ctx->cancel_baton));
SVN_ERR(svn_cl__try(svn_client_propset3
(&commit_info, pname_utf8, propval, target,
opt_state->depth, opt_state->force,
SVN_INVALID_REVNUM, opt_state->changelists,
NULL, ctx, subpool),
&success, opt_state->quiet,
SVN_ERR_UNVERSIONED_RESOURCE,
SVN_ERR_ENTRY_NOT_FOUND,
SVN_NO_ERROR));
if (! opt_state->quiet)
svn_cl__check_boolean_prop_val(pname_utf8, propval->data, subpool);
if (success && (! opt_state->quiet)) {
SVN_ERR
(svn_cmdline_printf
(pool, SVN_DEPTH_IS_RECURSIVE(opt_state->depth)
? _("property '%s' set (recursively) on '%s'\n")
: _("property '%s' set on '%s'\n"),
pname, svn_path_local_style(target, pool)));
}
}
svn_pool_destroy(subpool);
}
return SVN_NO_ERROR;
}
