#include "svn_cmdline.h"
#include "svn_wc.h"
#include "svn_pools.h"
#include "svn_client.h"
#include "svn_string.h"
#include "svn_path.h"
#include "svn_error.h"
#include "svn_utf.h"
#include "svn_props.h"
#include "cl.h"
#include "svn_private_config.h"
svn_error_t *
svn_cl__propedit(apr_getopt_t *os,
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
if (! svn_prop_name_is_valid(pname_utf8))
return svn_error_createf(SVN_ERR_CLIENT_PROPERTY_NAME, NULL,
_("'%s' is not a valid Subversion property name"),
pname_utf8);
if (opt_state->encoding && !svn_prop_needs_translation(pname_utf8))
return svn_error_create
(SVN_ERR_UNSUPPORTED_FEATURE, NULL,
_("--encoding option applies only to textual"
" Subversion-controlled properties"));
SVN_ERR(svn_cl__args_to_target_array_print_reserved(&targets, os,
opt_state->targets,
pool));
if (opt_state->revprop) {
svn_revnum_t rev;
const char *URL;
svn_string_t *propval;
const char *temp_dir;
svn_opt_push_implicit_dot_target(targets, pool);
SVN_ERR(svn_cl__revprop_prepare(&opt_state->start_revision, targets,
&URL, pool));
SVN_ERR(svn_client_revprop_get(pname_utf8, &propval,
URL, &(opt_state->start_revision),
&rev, ctx, pool));
if (! propval)
propval = svn_string_create("", pool);
SVN_ERR(svn_io_temp_dir(&temp_dir, pool));
SVN_ERR(svn_cl__edit_string_externally
(&propval, NULL,
opt_state->editor_cmd, temp_dir,
propval, "svn-prop",
ctx->config,
svn_prop_needs_translation(pname_utf8),
opt_state->encoding, pool));
if (propval) {
SVN_ERR(svn_client_revprop_set(pname_utf8, propval,
URL, &(opt_state->start_revision),
&rev, opt_state->force, ctx, pool));
SVN_ERR
(svn_cmdline_printf
(pool,
_("Set new value for property '%s' on revision %ld\n"),
pname_utf8, rev));
} else {
SVN_ERR(svn_cmdline_printf
(pool, _("No changes to property '%s' on revision %ld\n"),
pname_utf8, rev));
}
} else if (opt_state->start_revision.kind != svn_opt_revision_unspecified) {
return svn_error_createf
(SVN_ERR_CL_ARG_PARSING_ERROR, NULL,
_("Cannot specify revision for editing versioned property '%s'"),
pname_utf8);
} else {
apr_pool_t *subpool = svn_pool_create(pool);
if (targets->nelts == 0) {
return svn_error_create
(SVN_ERR_CL_INSUFFICIENT_ARGS, NULL,
_("Explicit target argument required"));
}
for (i = 0; i < targets->nelts; i++) {
apr_hash_t *props;
const char *target = APR_ARRAY_IDX(targets, i, const char *);
svn_string_t *propval, *edited_propval;
const char *base_dir = target;
const char *target_local;
svn_wc_adm_access_t *adm_access;
const svn_wc_entry_t *entry;
svn_opt_revision_t peg_revision;
svn_revnum_t base_rev = SVN_INVALID_REVNUM;
svn_pool_clear(subpool);
SVN_ERR(svn_cl__check_cancel(ctx->cancel_baton));
peg_revision.kind = svn_opt_revision_unspecified;
SVN_ERR(svn_client_propget3(&props, pname_utf8, target,
&peg_revision,
&(opt_state->start_revision),
&base_rev, svn_depth_empty,
NULL, ctx, subpool));
propval = apr_hash_get(props, target, APR_HASH_KEY_STRING);
if (! propval)
propval = svn_string_create("", subpool);
if (svn_path_is_url(target)) {
base_dir = ".";
} else {
if (opt_state->message || opt_state->filedata ||
opt_state->revprop_table) {
return svn_error_create
(SVN_ERR_CL_UNNECESSARY_LOG_MESSAGE, NULL,
_("Local, non-commit operations do not take a log message "
"or revision properties"));
}
SVN_ERR(svn_wc_adm_probe_open3(&adm_access, NULL, target,
FALSE, 0, ctx->cancel_func,
ctx->cancel_baton, subpool));
SVN_ERR(svn_wc_entry(&entry, target, adm_access, FALSE, subpool));
if (! entry)
return svn_error_createf
(SVN_ERR_ENTRY_NOT_FOUND, NULL,
_("'%s' does not appear to be a working copy path"), target);
if (entry->kind == svn_node_file)
svn_path_split(target, &base_dir, NULL, subpool);
}
SVN_ERR(svn_cl__edit_string_externally(&edited_propval, NULL,
opt_state->editor_cmd,
base_dir,
propval,
"svn-prop",
ctx->config,
svn_prop_needs_translation
(pname_utf8),
opt_state->encoding,
subpool));
target_local = svn_path_is_url(target) ? target
: svn_path_local_style(target, subpool);
if (edited_propval && !svn_string_compare(propval, edited_propval)) {
svn_commit_info_t *commit_info = NULL;
svn_error_t *err = SVN_NO_ERROR;
svn_cl__check_boolean_prop_val(pname_utf8, edited_propval->data,
subpool);
if (ctx->log_msg_func3)
SVN_ERR(svn_cl__make_log_msg_baton(&(ctx->log_msg_baton3),
opt_state, NULL, ctx->config,
subpool));
err = svn_client_propset3(&commit_info,
pname_utf8, edited_propval, target,
svn_depth_empty, opt_state->force,
base_rev, NULL, opt_state->revprop_table,
ctx, subpool);
if (ctx->log_msg_func3)
SVN_ERR(svn_cl__cleanup_log_msg(ctx->log_msg_baton2, err));
else if (err)
return err;
if (commit_info || ! svn_path_is_url(target))
SVN_ERR
(svn_cmdline_printf
(subpool, _("Set new value for property '%s' on '%s'\n"),
pname_utf8, target_local));
if (commit_info && ! opt_state->quiet)
SVN_ERR(svn_cl__print_commit_info(commit_info, subpool));
} else {
SVN_ERR
(svn_cmdline_printf
(subpool, _("No changes to property '%s' on '%s'\n"),
pname_utf8, target_local));
}
}
svn_pool_destroy(subpool);
}
return SVN_NO_ERROR;
}