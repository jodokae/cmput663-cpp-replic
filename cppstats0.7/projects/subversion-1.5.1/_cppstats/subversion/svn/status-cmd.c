#include "svn_string.h"
#include "svn_wc.h"
#include "svn_client.h"
#include "svn_error_codes.h"
#include "svn_error.h"
#include "svn_pools.h"
#include "svn_xml.h"
#include "svn_path.h"
#include "svn_cmdline.h"
#include "cl.h"
#include "svn_private_config.h"
struct status_baton {
svn_boolean_t detailed;
svn_boolean_t show_last_committed;
svn_boolean_t skip_unrecognized;
svn_boolean_t repos_locks;
apr_pool_t *pool;
apr_hash_t *cached_changelists;
apr_pool_t *cl_pool;
svn_boolean_t had_print_error;
svn_boolean_t xml_mode;
};
struct status_cache {
const char *path;
svn_wc_status2_t *status;
};
static svn_error_t *
print_start_target_xml(const char *target, apr_pool_t *pool) {
svn_stringbuf_t *sb = svn_stringbuf_create("", pool);
svn_xml_make_open_tag(&sb, pool, svn_xml_normal, "target",
"path", target, NULL);
return svn_cl__error_checked_fputs(sb->data, stdout);
}
static svn_error_t *
print_finish_target_xml(svn_revnum_t repos_rev,
apr_pool_t *pool) {
svn_stringbuf_t *sb = svn_stringbuf_create("", pool);
if (SVN_IS_VALID_REVNUM(repos_rev)) {
const char *repos_rev_str;
repos_rev_str = apr_psprintf(pool, "%ld", repos_rev);
svn_xml_make_open_tag(&sb, pool, svn_xml_self_closing, "against",
"revision", repos_rev_str, NULL);
}
svn_xml_make_close_tag(&sb, pool, "target");
return svn_cl__error_checked_fputs(sb->data, stdout);
}
static void
print_status_normal_or_xml(void *baton,
const char *path,
svn_wc_status2_t *status) {
struct status_baton *sb = baton;
svn_error_t *err;
if (sb->xml_mode)
err = svn_cl__print_status_xml(path, status, sb->pool);
else
err = svn_cl__print_status(path, status, sb->detailed,
sb->show_last_committed,
sb->skip_unrecognized,
sb->repos_locks,
sb->pool);
if (err) {
if (!sb->had_print_error) {
sb->had_print_error = TRUE;
svn_handle_error2(err, stderr, FALSE, "svn: ");
}
svn_error_clear(err);
}
}
static void
print_status(void *baton,
const char *path,
svn_wc_status2_t *status) {
struct status_baton *sb = baton;
if (status->entry && status->entry->changelist) {
apr_array_header_t *path_array;
const char *cl_key = apr_pstrdup(sb->cl_pool, status->entry->changelist);
struct status_cache *scache = apr_pcalloc(sb->cl_pool, sizeof(*scache));
scache->path = apr_pstrdup(sb->cl_pool, path);
scache->status = svn_wc_dup_status2(status, sb->cl_pool);
path_array = (apr_array_header_t *)
apr_hash_get(sb->cached_changelists, cl_key, APR_HASH_KEY_STRING);
if (path_array == NULL) {
path_array = apr_array_make(sb->cl_pool, 1,
sizeof(struct status_cache *));
apr_hash_set(sb->cached_changelists, cl_key,
APR_HASH_KEY_STRING, path_array);
}
APR_ARRAY_PUSH(path_array, struct status_cache *) = scache;
return;
}
print_status_normal_or_xml(baton, path, status);
}
svn_error_t *
svn_cl__status(apr_getopt_t *os,
void *baton,
apr_pool_t *pool) {
svn_cl__opt_state_t *opt_state = ((svn_cl__cmd_baton_t *) baton)->opt_state;
svn_client_ctx_t *ctx = ((svn_cl__cmd_baton_t *) baton)->ctx;
apr_array_header_t *targets;
apr_pool_t *subpool;
apr_hash_t *master_cl_hash = apr_hash_make(pool);
int i;
svn_opt_revision_t rev;
struct status_baton sb;
SVN_ERR(svn_cl__args_to_target_array_print_reserved(&targets, os,
opt_state->targets,
pool));
svn_opt_push_implicit_dot_target(targets, pool);
rev.kind = svn_opt_revision_head;
if (! opt_state->xml)
svn_cl__get_notifier(&ctx->notify_func2, &ctx->notify_baton2, FALSE,
FALSE, FALSE, pool);
subpool = svn_pool_create(pool);
sb.had_print_error = FALSE;
if (opt_state->xml) {
if (! opt_state->incremental)
SVN_ERR(svn_cl__xml_print_header("status", pool));
} else {
if (opt_state->incremental)
return svn_error_create(SVN_ERR_CL_ARG_PARSING_ERROR, NULL,
_("'incremental' option only valid in XML "
"mode"));
}
sb.detailed = (opt_state->verbose || opt_state->update);
sb.show_last_committed = opt_state->verbose;
sb.skip_unrecognized = opt_state->quiet;
sb.repos_locks = opt_state->update;
sb.xml_mode = opt_state->xml;
sb.pool = subpool;
sb.cached_changelists = master_cl_hash;
sb.cl_pool = pool;
for (i = 0; i < targets->nelts; i++) {
const char *target = APR_ARRAY_IDX(targets, i, const char *);
svn_revnum_t repos_rev = SVN_INVALID_REVNUM;
svn_pool_clear(subpool);
SVN_ERR(svn_cl__check_cancel(ctx->cancel_baton));
if (opt_state->xml)
SVN_ERR(print_start_target_xml(svn_path_local_style(target, subpool),
subpool));
SVN_ERR(svn_cl__try(svn_client_status3(&repos_rev, target, &rev,
print_status, &sb,
opt_state->depth,
opt_state->verbose,
opt_state->update,
opt_state->no_ignore,
opt_state->ignore_externals,
opt_state->changelists,
ctx, subpool),
NULL, opt_state->quiet,
SVN_ERR_WC_NOT_DIRECTORY,
SVN_NO_ERROR));
if (opt_state->xml)
SVN_ERR(print_finish_target_xml(repos_rev, subpool));
}
if (apr_hash_count(master_cl_hash) > 0) {
apr_hash_index_t *hi;
svn_stringbuf_t *buf;
if (opt_state->xml)
buf = svn_stringbuf_create("", pool);
for (hi = apr_hash_first(pool, master_cl_hash); hi;
hi = apr_hash_next(hi)) {
const char *changelist_name;
apr_array_header_t *path_array;
const void *key;
void *val;
int j;
apr_hash_this(hi, &key, NULL, &val);
changelist_name = key;
path_array = val;
if (opt_state->xml) {
svn_stringbuf_set(buf, "");
svn_xml_make_open_tag(&buf, pool, svn_xml_normal, "changelist",
"name", changelist_name, NULL);
SVN_ERR(svn_cl__error_checked_fputs(buf->data, stdout));
} else
SVN_ERR(svn_cmdline_printf(pool, _("\n--- Changelist '%s':\n"),
changelist_name));
for (j = 0; j < path_array->nelts; j++) {
struct status_cache *scache =
APR_ARRAY_IDX(path_array, j, struct status_cache *);
print_status_normal_or_xml(&sb, scache->path, scache->status);
}
if (opt_state->xml) {
svn_stringbuf_set(buf, "");
svn_xml_make_close_tag(&buf, pool, "changelist");
SVN_ERR(svn_cl__error_checked_fputs(buf->data, stdout));
}
}
}
svn_pool_destroy(subpool);
if (opt_state->xml && (! opt_state->incremental))
SVN_ERR(svn_cl__xml_print_footer("status", pool));
return SVN_NO_ERROR;
}