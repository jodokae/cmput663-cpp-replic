#include "svn_string.h"
#include "svn_cmdline.h"
#include "svn_wc.h"
#include "svn_pools.h"
#include "svn_error_codes.h"
#include "svn_error.h"
#include "svn_path.h"
#include "svn_time.h"
#include "svn_xml.h"
#include "cl.h"
#include "svn_private_config.h"
static svn_error_t *
svn_cl__info_print_time(apr_time_t atime,
const char *desc,
apr_pool_t *pool) {
const char *time_utf8;
time_utf8 = svn_time_to_human_cstring(atime, pool);
SVN_ERR(svn_cmdline_printf(pool, "%s: %s\n", desc, time_utf8));
return SVN_NO_ERROR;
}
static const char *
schedule_str(svn_wc_schedule_t schedule) {
switch (schedule) {
case svn_wc_schedule_normal:
return "normal";
case svn_wc_schedule_add:
return "add";
case svn_wc_schedule_delete:
return "delete";
case svn_wc_schedule_replace:
return "replace";
default:
return "none";
}
}
static svn_error_t *
print_info_xml(void *baton,
const char *target,
const svn_info_t *info,
apr_pool_t *pool) {
svn_stringbuf_t *sb = svn_stringbuf_create("", pool);
const char *rev_str;
if (SVN_IS_VALID_REVNUM(info->rev))
rev_str = apr_psprintf(pool, "%ld", info->rev);
else
return svn_error_createf(SVN_ERR_WC_CORRUPT, NULL,
_("'%s' has invalid revision"),
svn_path_local_style(target, pool));
svn_xml_make_open_tag(&sb, pool, svn_xml_normal, "entry",
"path", svn_path_local_style(target, pool),
"kind", svn_cl__node_kind_str(info->kind),
"revision", rev_str,
NULL);
svn_cl__xml_tagged_cdata(&sb, pool, "url", info->URL);
if (info->repos_root_URL || info->repos_UUID) {
svn_xml_make_open_tag(&sb, pool, svn_xml_normal, "repository", NULL);
svn_cl__xml_tagged_cdata(&sb, pool, "root", info->repos_root_URL);
svn_cl__xml_tagged_cdata(&sb, pool, "uuid", info->repos_UUID);
svn_xml_make_close_tag(&sb, pool, "repository");
}
if (info->has_wc_info) {
svn_xml_make_open_tag(&sb, pool, svn_xml_normal, "wc-info", NULL);
svn_cl__xml_tagged_cdata(&sb, pool, "schedule",
schedule_str(info->schedule));
svn_cl__xml_tagged_cdata(&sb, pool, "depth",
svn_depth_to_word(info->depth));
svn_cl__xml_tagged_cdata(&sb, pool, "copy-from-url",
info->copyfrom_url);
if (SVN_IS_VALID_REVNUM(info->copyfrom_rev))
svn_cl__xml_tagged_cdata(&sb, pool, "copy-from-rev",
apr_psprintf(pool, "%ld",
info->copyfrom_rev));
if (info->text_time)
svn_cl__xml_tagged_cdata(&sb, pool, "text-updated",
svn_time_to_cstring(info->text_time, pool));
if (info->prop_time)
svn_cl__xml_tagged_cdata(&sb, pool, "prop-updated",
svn_time_to_cstring(info->prop_time, pool));
svn_cl__xml_tagged_cdata(&sb, pool, "checksum", info->checksum);
if (info->changelist)
svn_cl__xml_tagged_cdata(&sb, pool, "changelist", info->changelist);
svn_xml_make_close_tag(&sb, pool, "wc-info");
}
if (info->last_changed_author
|| SVN_IS_VALID_REVNUM(info->last_changed_rev)
|| info->last_changed_date) {
svn_cl__print_xml_commit(&sb, info->last_changed_rev,
info->last_changed_author,
svn_time_to_cstring(info->last_changed_date,
pool),
pool);
}
if (info->conflict_old || info->conflict_wrk
|| info->conflict_new || info->prejfile) {
svn_xml_make_open_tag(&sb, pool, svn_xml_normal, "conflict", NULL);
svn_cl__xml_tagged_cdata(&sb, pool, "prev-base-file",
info->conflict_old);
svn_cl__xml_tagged_cdata(&sb, pool, "prev-wc-file",
info->conflict_wrk);
svn_cl__xml_tagged_cdata(&sb, pool, "cur-base-file",
info->conflict_new);
svn_cl__xml_tagged_cdata(&sb, pool, "prop-file", info->prejfile);
svn_xml_make_close_tag(&sb, pool, "conflict");
}
if (info->lock) {
svn_xml_make_open_tag(&sb, pool, svn_xml_normal, "lock", NULL);
svn_cl__xml_tagged_cdata(&sb, pool, "token", info->lock->token);
svn_cl__xml_tagged_cdata(&sb, pool, "owner", info->lock->owner);
svn_cl__xml_tagged_cdata(&sb, pool, "comment", info->lock->comment);
svn_cl__xml_tagged_cdata(&sb, pool, "created",
svn_time_to_cstring
(info->lock->creation_date, pool));
svn_cl__xml_tagged_cdata(&sb, pool, "expires",
svn_time_to_cstring
(info->lock->expiration_date, pool));
svn_xml_make_close_tag(&sb, pool, "lock");
}
svn_xml_make_close_tag(&sb, pool, "entry");
return svn_cl__error_checked_fputs(sb->data, stdout);
}
static svn_error_t *
print_info(void *baton,
const char *target,
const svn_info_t *info,
apr_pool_t *pool) {
SVN_ERR(svn_cmdline_printf(pool, _("Path: %s\n"),
svn_path_local_style(target, pool)));
if (info->kind != svn_node_dir)
SVN_ERR(svn_cmdline_printf(pool, _("Name: %s\n"),
svn_path_basename(target, pool)));
if (info->URL)
SVN_ERR(svn_cmdline_printf(pool, _("URL: %s\n"), info->URL));
if (info->repos_root_URL)
SVN_ERR(svn_cmdline_printf(pool, _("Repository Root: %s\n"),
info->repos_root_URL));
if (info->repos_UUID)
SVN_ERR(svn_cmdline_printf(pool, _("Repository UUID: %s\n"),
info->repos_UUID));
if (SVN_IS_VALID_REVNUM(info->rev))
SVN_ERR(svn_cmdline_printf(pool, _("Revision: %ld\n"), info->rev));
switch (info->kind) {
case svn_node_file:
SVN_ERR(svn_cmdline_printf(pool, _("Node Kind: file\n")));
break;
case svn_node_dir:
SVN_ERR(svn_cmdline_printf(pool, _("Node Kind: directory\n")));
break;
case svn_node_none:
SVN_ERR(svn_cmdline_printf(pool, _("Node Kind: none\n")));
break;
case svn_node_unknown:
default:
SVN_ERR(svn_cmdline_printf(pool, _("Node Kind: unknown\n")));
break;
}
if (info->has_wc_info) {
switch (info->schedule) {
case svn_wc_schedule_normal:
SVN_ERR(svn_cmdline_printf(pool, _("Schedule: normal\n")));
break;
case svn_wc_schedule_add:
SVN_ERR(svn_cmdline_printf(pool, _("Schedule: add\n")));
break;
case svn_wc_schedule_delete:
SVN_ERR(svn_cmdline_printf(pool, _("Schedule: delete\n")));
break;
case svn_wc_schedule_replace:
SVN_ERR(svn_cmdline_printf(pool, _("Schedule: replace\n")));
break;
default:
break;
}
switch (info->depth) {
case svn_depth_unknown:
break;
case svn_depth_empty:
SVN_ERR(svn_cmdline_printf(pool, _("Depth: empty\n")));
break;
case svn_depth_files:
SVN_ERR(svn_cmdline_printf(pool, _("Depth: files\n")));
break;
case svn_depth_immediates:
SVN_ERR(svn_cmdline_printf(pool, _("Depth: immediates\n")));
break;
case svn_depth_infinity:
break;
default:
SVN_ERR(svn_cmdline_printf(pool, _("Depth: INVALID\n")));
}
if (info->copyfrom_url)
SVN_ERR(svn_cmdline_printf(pool, _("Copied From URL: %s\n"),
info->copyfrom_url));
if (SVN_IS_VALID_REVNUM(info->copyfrom_rev))
SVN_ERR(svn_cmdline_printf(pool, _("Copied From Rev: %ld\n"),
info->copyfrom_rev));
}
if (info->last_changed_author)
SVN_ERR(svn_cmdline_printf(pool, _("Last Changed Author: %s\n"),
info->last_changed_author));
if (SVN_IS_VALID_REVNUM(info->last_changed_rev))
SVN_ERR(svn_cmdline_printf(pool, _("Last Changed Rev: %ld\n"),
info->last_changed_rev));
if (info->last_changed_date)
SVN_ERR(svn_cl__info_print_time(info->last_changed_date,
_("Last Changed Date"), pool));
if (info->has_wc_info) {
if (info->text_time)
SVN_ERR(svn_cl__info_print_time(info->text_time,
_("Text Last Updated"), pool));
if (info->prop_time)
SVN_ERR(svn_cl__info_print_time(info->prop_time,
_("Properties Last Updated"), pool));
if (info->checksum)
SVN_ERR(svn_cmdline_printf(pool, _("Checksum: %s\n"),
info->checksum));
if (info->conflict_old)
SVN_ERR(svn_cmdline_printf(pool,
_("Conflict Previous Base File: %s\n"),
svn_path_local_style(info->conflict_old,
pool)));
if (info->conflict_wrk)
SVN_ERR(svn_cmdline_printf
(pool, _("Conflict Previous Working File: %s\n"),
svn_path_local_style(info->conflict_wrk, pool)));
if (info->conflict_new)
SVN_ERR(svn_cmdline_printf(pool,
_("Conflict Current Base File: %s\n"),
svn_path_local_style(info->conflict_new,
pool)));
if (info->prejfile)
SVN_ERR(svn_cmdline_printf(pool, _("Conflict Properties File: %s\n"),
svn_path_local_style(info->prejfile,
pool)));
}
if (info->lock) {
if (info->lock->token)
SVN_ERR(svn_cmdline_printf(pool, _("Lock Token: %s\n"),
info->lock->token));
if (info->lock->owner)
SVN_ERR(svn_cmdline_printf(pool, _("Lock Owner: %s\n"),
info->lock->owner));
if (info->lock->creation_date)
SVN_ERR(svn_cl__info_print_time(info->lock->creation_date,
_("Lock Created"), pool));
if (info->lock->expiration_date)
SVN_ERR(svn_cl__info_print_time(info->lock->expiration_date,
_("Lock Expires"), pool));
if (info->lock->comment) {
int comment_lines;
comment_lines = svn_cstring_count_newlines(info->lock->comment) + 1;
SVN_ERR(svn_cmdline_printf(pool,
(comment_lines != 1)
? _("Lock Comment (%i lines):\n%s\n")
: _("Lock Comment (%i line):\n%s\n"),
comment_lines,
info->lock->comment));
}
}
if (info->changelist)
SVN_ERR(svn_cmdline_printf(pool, _("Changelist: %s\n"),
info->changelist));
SVN_ERR(svn_cmdline_printf(pool, "\n"));
return SVN_NO_ERROR;
}
svn_error_t *
svn_cl__info(apr_getopt_t *os,
void *baton,
apr_pool_t *pool) {
svn_cl__opt_state_t *opt_state = ((svn_cl__cmd_baton_t *) baton)->opt_state;
svn_client_ctx_t *ctx = ((svn_cl__cmd_baton_t *) baton)->ctx;
apr_array_header_t *targets = NULL;
apr_pool_t *subpool = svn_pool_create(pool);
int i;
svn_error_t *err;
svn_opt_revision_t peg_revision;
svn_info_receiver_t receiver;
SVN_ERR(svn_cl__args_to_target_array_print_reserved(&targets, os,
opt_state->targets,
pool));
svn_opt_push_implicit_dot_target(targets, pool);
if (opt_state->xml) {
receiver = print_info_xml;
if (! opt_state->incremental)
SVN_ERR(svn_cl__xml_print_header("info", pool));
} else {
receiver = print_info;
if (opt_state->incremental)
return svn_error_create(SVN_ERR_CL_ARG_PARSING_ERROR, NULL,
_("'incremental' option only valid in XML "
"mode"));
}
if (opt_state->depth == svn_depth_unknown)
opt_state->depth = svn_depth_empty;
for (i = 0; i < targets->nelts; i++) {
const char *truepath;
const char *target = APR_ARRAY_IDX(targets, i, const char *);
svn_pool_clear(subpool);
SVN_ERR(svn_cl__check_cancel(ctx->cancel_baton));
SVN_ERR(svn_opt_parse_path(&peg_revision, &truepath, target, subpool));
if ((svn_path_is_url(target))
&& (peg_revision.kind == svn_opt_revision_unspecified))
peg_revision.kind = svn_opt_revision_head;
err = svn_client_info2(truepath,
&peg_revision, &(opt_state->start_revision),
receiver, NULL, opt_state->depth,
opt_state->changelists, ctx, subpool);
if (err && err->apr_err == SVN_ERR_UNVERSIONED_RESOURCE) {
svn_error_clear(err);
SVN_ERR(svn_cmdline_fprintf
(stderr, subpool,
_("%s: (Not a versioned resource)\n\n"),
svn_path_local_style(target, pool)));
continue;
} else if (err && err->apr_err == SVN_ERR_RA_ILLEGAL_URL) {
svn_error_clear(err);
SVN_ERR(svn_cmdline_fprintf
(stderr, subpool,
_("%s: (Not a valid URL)\n\n"),
svn_path_local_style(target, pool)));
continue;
} else if (err)
return err;
}
svn_pool_destroy(subpool);
if (opt_state->xml && (! opt_state->incremental))
SVN_ERR(svn_cl__xml_print_footer("info", pool));
return SVN_NO_ERROR;
}