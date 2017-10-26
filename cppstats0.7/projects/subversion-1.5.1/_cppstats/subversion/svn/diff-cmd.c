#include "svn_pools.h"
#include "svn_client.h"
#include "svn_string.h"
#include "svn_path.h"
#include "svn_error_codes.h"
#include "svn_error.h"
#include "svn_types.h"
#include "svn_cmdline.h"
#include "svn_xml.h"
#include "cl.h"
#include "svn_private_config.h"
static char
kind_to_char(svn_client_diff_summarize_kind_t kind) {
switch (kind) {
case svn_client_diff_summarize_kind_modified:
return 'M';
case svn_client_diff_summarize_kind_added:
return 'A';
case svn_client_diff_summarize_kind_deleted:
return 'D';
default:
return ' ';
}
}
static const char *
kind_to_word(svn_client_diff_summarize_kind_t kind) {
switch (kind) {
case svn_client_diff_summarize_kind_modified:
return "modified";
case svn_client_diff_summarize_kind_added:
return "added";
case svn_client_diff_summarize_kind_deleted:
return "deleted";
default:
return "none";
}
}
static svn_error_t *
summarize_xml(const svn_client_diff_summarize_t *summary,
void *baton,
apr_pool_t *pool) {
const char *path = baton;
svn_stringbuf_t *sb = svn_stringbuf_create("", pool);
path = svn_path_join(path, summary->path, pool);
if (! svn_path_is_url(path))
path = svn_path_local_style(path, pool);
svn_xml_make_open_tag(&sb, pool, svn_xml_protect_pcdata, "path",
"kind", svn_cl__node_kind_str(summary->node_kind),
"item", kind_to_word(summary->summarize_kind),
"props", summary->prop_changed ? "modified" : "none",
NULL);
svn_xml_escape_cdata_cstring(&sb, path, pool);
svn_xml_make_close_tag(&sb, pool, "path");
SVN_ERR(svn_cl__error_checked_fputs(sb->data, stdout));
return SVN_NO_ERROR;
}
static svn_error_t *
summarize_regular(const svn_client_diff_summarize_t *summary,
void *baton,
apr_pool_t *pool) {
const char *path = baton;
path = svn_path_join(path, summary->path, pool);
if (! svn_path_is_url(path))
path = svn_path_local_style(path, pool);
SVN_ERR(svn_cmdline_printf(pool,
"%c%c %s\n",
kind_to_char(summary->summarize_kind),
summary->prop_changed ? 'M' : ' ',
path));
SVN_ERR(svn_cmdline_fflush(stdout));
return SVN_NO_ERROR;
}
svn_error_t *
svn_cl__diff(apr_getopt_t *os,
void *baton,
apr_pool_t *pool) {
svn_cl__opt_state_t *opt_state = ((svn_cl__cmd_baton_t *) baton)->opt_state;
apr_array_header_t *options;
apr_array_header_t *targets;
apr_file_t *outfile, *errfile;
apr_status_t status;
const char *old_target, *new_target;
apr_pool_t *iterpool;
svn_boolean_t pegged_diff = FALSE;
int i;
const svn_client_diff_summarize_func_t summarize_func =
(opt_state->xml ? summarize_xml : summarize_regular);
{
const char *optstr = opt_state->extensions ? opt_state->extensions : "";
options = svn_cstring_split(optstr, " \t\n\r", TRUE, pool);
}
if ((status = apr_file_open_stdout(&outfile, pool)))
return svn_error_wrap_apr(status, _("Can't open stdout"));
if ((status = apr_file_open_stderr(&errfile, pool)))
return svn_error_wrap_apr(status, _("Can't open stderr"));
if (opt_state->xml) {
svn_stringbuf_t *sb;
if (!opt_state->summarize)
return svn_error_create(SVN_ERR_CL_ARG_PARSING_ERROR, NULL,
_("'--xml' option only valid with "
"'--summarize' option"));
SVN_ERR(svn_cl__xml_print_header("diff", pool));
sb = svn_stringbuf_create("", pool);
svn_xml_make_open_tag(&sb, pool, svn_xml_normal, "paths", NULL);
SVN_ERR(svn_cl__error_checked_fputs(sb->data, stdout));
}
SVN_ERR(svn_cl__args_to_target_array_print_reserved(&targets, os,
opt_state->targets,
pool));
if (! opt_state->old_target && ! opt_state->new_target
&& (targets->nelts == 2)
&& svn_path_is_url(APR_ARRAY_IDX(targets, 0, const char *))
&& svn_path_is_url(APR_ARRAY_IDX(targets, 1, const char *))
&& opt_state->start_revision.kind == svn_opt_revision_unspecified
&& opt_state->end_revision.kind == svn_opt_revision_unspecified) {
SVN_ERR(svn_opt_parse_path(&opt_state->start_revision, &old_target,
APR_ARRAY_IDX(targets, 0, const char *),
pool));
SVN_ERR(svn_opt_parse_path(&opt_state->end_revision, &new_target,
APR_ARRAY_IDX(targets, 1, const char *),
pool));
targets->nelts = 0;
if (opt_state->start_revision.kind == svn_opt_revision_unspecified)
opt_state->start_revision.kind = svn_opt_revision_head;
if (opt_state->end_revision.kind == svn_opt_revision_unspecified)
opt_state->end_revision.kind = svn_opt_revision_head;
} else if (opt_state->old_target) {
apr_array_header_t *tmp, *tmp2;
svn_opt_revision_t old_rev, new_rev;
tmp = apr_array_make(pool, 2, sizeof(const char *));
APR_ARRAY_PUSH(tmp, const char *) = (opt_state->old_target);
APR_ARRAY_PUSH(tmp, const char *) = (opt_state->new_target
? opt_state->new_target
: APR_ARRAY_IDX(tmp, 0,
const char *));
SVN_ERR(svn_cl__args_to_target_array_print_reserved(&tmp2, os, tmp,
pool));
SVN_ERR(svn_opt_parse_path(&old_rev, &old_target,
APR_ARRAY_IDX(tmp2, 0, const char *),
pool));
if (old_rev.kind != svn_opt_revision_unspecified)
opt_state->start_revision = old_rev;
SVN_ERR(svn_opt_parse_path(&new_rev, &new_target,
APR_ARRAY_IDX(tmp2, 1, const char *),
pool));
if (new_rev.kind != svn_opt_revision_unspecified)
opt_state->end_revision = new_rev;
if (opt_state->start_revision.kind == svn_opt_revision_unspecified)
opt_state->start_revision.kind = svn_path_is_url(old_target)
? svn_opt_revision_head : svn_opt_revision_base;
if (opt_state->end_revision.kind == svn_opt_revision_unspecified)
opt_state->end_revision.kind = svn_path_is_url(new_target)
? svn_opt_revision_head : svn_opt_revision_working;
} else if (opt_state->new_target) {
return svn_error_create(SVN_ERR_CL_ARG_PARSING_ERROR, NULL,
_("'--new' option only valid with "
"'--old' option"));
} else {
svn_boolean_t working_copy_present = FALSE, url_present = FALSE;
svn_opt_push_implicit_dot_target(targets, pool);
old_target = "";
new_target = "";
for (i = 0; i < targets->nelts; ++i) {
const char *path = APR_ARRAY_IDX(targets, i, const char *);
if (! svn_path_is_url(path))
working_copy_present = TRUE;
else
url_present = TRUE;
}
if (url_present && working_copy_present)
return svn_error_createf(SVN_ERR_UNSUPPORTED_FEATURE, NULL,
_("Target lists to diff may not contain "
"both working copy paths and URLs"));
if (opt_state->start_revision.kind == svn_opt_revision_unspecified
&& working_copy_present)
opt_state->start_revision.kind = svn_opt_revision_base;
if (opt_state->end_revision.kind == svn_opt_revision_unspecified)
opt_state->end_revision.kind = working_copy_present
? svn_opt_revision_working : svn_opt_revision_head;
if ((opt_state->start_revision.kind != svn_opt_revision_base
&& opt_state->start_revision.kind != svn_opt_revision_working)
|| (opt_state->end_revision.kind != svn_opt_revision_base
&& opt_state->end_revision.kind != svn_opt_revision_working))
pegged_diff = TRUE;
}
svn_opt_push_implicit_dot_target(targets, pool);
iterpool = svn_pool_create(pool);
for (i = 0; i < targets->nelts; ++i) {
const char *path = APR_ARRAY_IDX(targets, i, const char *);
const char *target1, *target2;
svn_pool_clear(iterpool);
if (! pegged_diff) {
target1 = svn_path_join(old_target, path, iterpool);
target2 = svn_path_join(new_target, path, iterpool);
if (opt_state->summarize)
SVN_ERR(svn_client_diff_summarize2
(target1,
&opt_state->start_revision,
target2,
&opt_state->end_revision,
opt_state->depth,
opt_state->notice_ancestry ? FALSE : TRUE,
opt_state->changelists,
summarize_func,
(void *) target1,
((svn_cl__cmd_baton_t *)baton)->ctx,
iterpool));
else
SVN_ERR(svn_client_diff4
(options,
target1,
&(opt_state->start_revision),
target2,
&(opt_state->end_revision),
NULL,
opt_state->depth,
opt_state->notice_ancestry ? FALSE : TRUE,
opt_state->no_diff_deleted,
opt_state->force,
svn_cmdline_output_encoding(pool),
outfile,
errfile,
opt_state->changelists,
((svn_cl__cmd_baton_t *)baton)->ctx,
iterpool));
} else {
const char *truepath;
svn_opt_revision_t peg_revision;
SVN_ERR(svn_opt_parse_path(&peg_revision, &truepath, path,
iterpool));
if (peg_revision.kind == svn_opt_revision_unspecified)
peg_revision.kind = svn_path_is_url(path)
? svn_opt_revision_head : svn_opt_revision_working;
if (opt_state->summarize)
SVN_ERR(svn_client_diff_summarize_peg2
(truepath,
&peg_revision,
&opt_state->start_revision,
&opt_state->end_revision,
opt_state->depth,
opt_state->notice_ancestry ? FALSE : TRUE,
opt_state->changelists,
summarize_func,
(void *) truepath,
((svn_cl__cmd_baton_t *)baton)->ctx,
iterpool));
else
SVN_ERR(svn_client_diff_peg4
(options,
truepath,
&peg_revision,
&opt_state->start_revision,
&opt_state->end_revision,
NULL,
opt_state->depth,
opt_state->notice_ancestry ? FALSE : TRUE,
opt_state->no_diff_deleted,
opt_state->force,
svn_cmdline_output_encoding(pool),
outfile,
errfile,
opt_state->changelists,
((svn_cl__cmd_baton_t *)baton)->ctx,
iterpool));
}
}
if (opt_state->xml) {
svn_stringbuf_t *sb = svn_stringbuf_create("", pool);
svn_xml_make_close_tag(&sb, pool, "paths");
SVN_ERR(svn_cl__error_checked_fputs(sb->data, stdout));
SVN_ERR(svn_cl__xml_print_footer("diff", pool));
}
svn_pool_destroy(iterpool);
return SVN_NO_ERROR;
}
