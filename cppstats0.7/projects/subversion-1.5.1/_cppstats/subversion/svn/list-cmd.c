#include "svn_cmdline.h"
#include "svn_client.h"
#include "svn_error.h"
#include "svn_pools.h"
#include "svn_time.h"
#include "svn_xml.h"
#include "svn_path.h"
#include "cl.h"
#include "svn_private_config.h"
struct print_baton {
svn_boolean_t verbose;
svn_client_ctx_t *ctx;
};
static svn_error_t *
print_dirent(void *baton,
const char *path,
const svn_dirent_t *dirent,
const svn_lock_t *lock,
const char *abs_path,
apr_pool_t *pool) {
struct print_baton *pb = baton;
const char *entryname;
if (pb->ctx->cancel_func)
SVN_ERR(pb->ctx->cancel_func(pb->ctx->cancel_baton));
if (strcmp(path, "") == 0) {
if (dirent->kind == svn_node_file)
entryname = svn_path_basename(abs_path, pool);
else if (pb->verbose)
entryname = ".";
else
return SVN_NO_ERROR;
} else
entryname = path;
if (pb->verbose) {
apr_time_t now = apr_time_now();
apr_time_exp_t exp_time;
apr_status_t apr_err;
apr_size_t size;
char timestr[20];
const char *sizestr, *utf8_timestr;
apr_time_exp_lt(&exp_time, dirent->time);
if (apr_time_sec(now - dirent->time) < (365 * 86400 / 2)
&& apr_time_sec(dirent->time - now) < (365 * 86400 / 2)) {
apr_err = apr_strftime(timestr, &size, sizeof(timestr),
_("%b %d %H:%M"), &exp_time);
} else {
apr_err = apr_strftime(timestr, &size, sizeof(timestr),
_("%b %d %Y"), &exp_time);
}
if (apr_err)
timestr[0] = '\0';
SVN_ERR(svn_utf_cstring_to_utf8(&utf8_timestr, timestr, pool));
sizestr = apr_psprintf(pool, "%" SVN_FILESIZE_T_FMT, dirent->size);
SVN_ERR(svn_cmdline_printf
(pool, "%7ld %-8.8s %c %10s %12s %s%s\n",
dirent->created_rev,
dirent->last_author ? dirent->last_author : " ? ",
lock ? 'O' : ' ',
(dirent->kind == svn_node_file) ? sizestr : "",
utf8_timestr,
entryname,
(dirent->kind == svn_node_dir) ? "/" : ""));
} else {
SVN_ERR(svn_cmdline_printf(pool, "%s%s\n", entryname,
(dirent->kind == svn_node_dir)
? "/" : ""));
}
return SVN_NO_ERROR;
}
static svn_error_t *
print_dirent_xml(void *baton,
const char *path,
const svn_dirent_t *dirent,
const svn_lock_t *lock,
const char *abs_path,
apr_pool_t *pool) {
struct print_baton *pb = baton;
const char *entryname;
svn_stringbuf_t *sb;
if (strcmp(path, "") == 0) {
if (dirent->kind == svn_node_file)
entryname = svn_path_basename(abs_path, pool);
else if (pb->verbose)
entryname = ".";
else
return SVN_NO_ERROR;
} else
entryname = path;
if (pb->ctx->cancel_func)
SVN_ERR(pb->ctx->cancel_func(pb->ctx->cancel_baton));
sb = svn_stringbuf_create("", pool);
svn_xml_make_open_tag(&sb, pool, svn_xml_normal, "entry",
"kind", svn_cl__node_kind_str(dirent->kind),
NULL);
svn_cl__xml_tagged_cdata(&sb, pool, "name", entryname);
if (dirent->kind == svn_node_file) {
svn_cl__xml_tagged_cdata
(&sb, pool, "size",
apr_psprintf(pool, "%" SVN_FILESIZE_T_FMT, dirent->size));
}
svn_xml_make_open_tag(&sb, pool, svn_xml_normal, "commit",
"revision",
apr_psprintf(pool, "%ld", dirent->created_rev),
NULL);
svn_cl__xml_tagged_cdata(&sb, pool, "author", dirent->last_author);
if (dirent->time)
svn_cl__xml_tagged_cdata(&sb, pool, "date",
svn_time_to_cstring(dirent->time, pool));
svn_xml_make_close_tag(&sb, pool, "commit");
if (lock) {
svn_xml_make_open_tag(&sb, pool, svn_xml_normal, "lock", NULL);
svn_cl__xml_tagged_cdata(&sb, pool, "token", lock->token);
svn_cl__xml_tagged_cdata(&sb, pool, "owner", lock->owner);
svn_cl__xml_tagged_cdata(&sb, pool, "comment", lock->comment);
svn_cl__xml_tagged_cdata(&sb, pool, "created",
svn_time_to_cstring(lock->creation_date,
pool));
if (lock->expiration_date != 0)
svn_cl__xml_tagged_cdata(&sb, pool, "expires",
svn_time_to_cstring
(lock->expiration_date, pool));
svn_xml_make_close_tag(&sb, pool, "lock");
}
svn_xml_make_close_tag(&sb, pool, "entry");
SVN_ERR(svn_cl__error_checked_fputs(sb->data, stdout));
return SVN_NO_ERROR;
}
svn_error_t *
svn_cl__list(apr_getopt_t *os,
void *baton,
apr_pool_t *pool) {
svn_cl__opt_state_t *opt_state = ((svn_cl__cmd_baton_t *) baton)->opt_state;
svn_client_ctx_t *ctx = ((svn_cl__cmd_baton_t *) baton)->ctx;
apr_array_header_t *targets;
int i;
apr_pool_t *subpool = svn_pool_create(pool);
apr_uint32_t dirent_fields;
struct print_baton pb;
SVN_ERR(svn_cl__args_to_target_array_print_reserved(&targets, os,
opt_state->targets,
pool));
svn_opt_push_implicit_dot_target(targets, pool);
if (opt_state->xml) {
if (opt_state->verbose)
return svn_error_create(SVN_ERR_CL_ARG_PARSING_ERROR, NULL,
_("'verbose' option invalid in XML mode"));
if (! opt_state->incremental)
SVN_ERR(svn_cl__xml_print_header("lists", pool));
} else {
if (opt_state->incremental)
return svn_error_create(SVN_ERR_CL_ARG_PARSING_ERROR, NULL,
_("'incremental' option only valid in XML "
"mode"));
}
if (opt_state->verbose || opt_state->xml)
dirent_fields = SVN_DIRENT_ALL;
else
dirent_fields = SVN_DIRENT_KIND;
pb.ctx = ctx;
pb.verbose = opt_state->verbose;
if (opt_state->depth == svn_depth_unknown)
opt_state->depth = svn_depth_immediates;
for (i = 0; i < targets->nelts; i++) {
const char *target = APR_ARRAY_IDX(targets, i, const char *);
const char *truepath;
svn_opt_revision_t peg_revision;
svn_pool_clear(subpool);
SVN_ERR(svn_cl__check_cancel(ctx->cancel_baton));
SVN_ERR(svn_opt_parse_path(&peg_revision, &truepath, target,
subpool));
if (opt_state->xml) {
svn_stringbuf_t *sb = svn_stringbuf_create("", pool);
svn_xml_make_open_tag(&sb, pool, svn_xml_normal, "list",
"path", truepath[0] == '\0' ? "." : truepath,
NULL);
SVN_ERR(svn_cl__error_checked_fputs(sb->data, stdout));
}
SVN_ERR(svn_client_list2(truepath, &peg_revision,
&(opt_state->start_revision),
opt_state->depth,
dirent_fields,
(opt_state->xml || opt_state->verbose),
opt_state->xml ? print_dirent_xml : print_dirent,
&pb, ctx, subpool));
if (opt_state->xml) {
svn_stringbuf_t *sb = svn_stringbuf_create("", pool);
svn_xml_make_close_tag(&sb, pool, "list");
SVN_ERR(svn_cl__error_checked_fputs(sb->data, stdout));
}
}
svn_pool_destroy(subpool);
if (opt_state->xml && ! opt_state->incremental)
SVN_ERR(svn_cl__xml_print_footer("lists", pool));
return SVN_NO_ERROR;
}