#define APR_WANT_STRFUNC
#define APR_WANT_STDIO
#include <apr_want.h>
#include "svn_client.h"
#include "svn_compat.h"
#include "svn_string.h"
#include "svn_path.h"
#include "svn_error.h"
#include "svn_sorts.h"
#include "svn_xml.h"
#include "svn_time.h"
#include "svn_cmdline.h"
#include "cl.h"
#include "svn_private_config.h"
struct log_receiver_baton {
svn_cancel_func_t cancel_func;
void *cancel_baton;
svn_boolean_t omit_log_message;
apr_array_header_t *merge_stack;
apr_pool_t *pool;
};
#define SEP_STRING "------------------------------------------------------------------------\n"
static svn_error_t *
log_entry_receiver(void *baton,
svn_log_entry_t *log_entry,
apr_pool_t *pool) {
struct log_receiver_baton *lb = baton;
const char *author;
const char *date;
const char *message;
int lines;
if (lb->cancel_func)
SVN_ERR(lb->cancel_func(lb->cancel_baton));
svn_compat_log_revprops_out(&author, &date, &message, log_entry->revprops);
if (log_entry->revision == 0 && message == NULL)
return SVN_NO_ERROR;
if (! SVN_IS_VALID_REVNUM(log_entry->revision)) {
apr_array_pop(lb->merge_stack);
return SVN_NO_ERROR;
}
if (author == NULL)
author = _("(no author)");
if (date && date[0])
SVN_ERR(svn_cl__time_cstring_to_human_cstring(&date, date, pool));
else
date = _("(no date)");
if (! lb->omit_log_message && message == NULL)
message = "";
SVN_ERR(svn_cmdline_printf(pool,
SEP_STRING "r%ld | %s | %s",
log_entry->revision, author, date));
if (message != NULL) {
lines = svn_cstring_count_newlines(message) + 1;
SVN_ERR(svn_cmdline_printf(pool,
(lines != 1)
? " | %d lines"
: " | %d line", lines));
}
SVN_ERR(svn_cmdline_printf(pool, "\n"));
if (log_entry->changed_paths) {
apr_array_header_t *sorted_paths;
int i;
sorted_paths = svn_sort__hash(log_entry->changed_paths,
svn_sort_compare_items_as_paths, pool);
SVN_ERR(svn_cmdline_printf(pool,
_("Changed paths:\n")));
for (i = 0; i < sorted_paths->nelts; i++) {
svn_sort__item_t *item = &(APR_ARRAY_IDX(sorted_paths, i,
svn_sort__item_t));
const char *path = item->key;
svn_log_changed_path_t *log_item
= apr_hash_get(log_entry->changed_paths, item->key, item->klen);
const char *copy_data = "";
if (log_item->copyfrom_path
&& SVN_IS_VALID_REVNUM(log_item->copyfrom_rev)) {
copy_data
= apr_psprintf(pool,
_(" (from %s:%ld)"),
log_item->copyfrom_path,
log_item->copyfrom_rev);
}
SVN_ERR(svn_cmdline_printf(pool, " %c %s%s\n",
log_item->action, path,
copy_data));
}
}
if (lb->merge_stack->nelts > 0) {
int i;
SVN_ERR(svn_cmdline_printf(pool, _("Merged via:")));
for (i = 0; i < lb->merge_stack->nelts; i++) {
svn_revnum_t rev = APR_ARRAY_IDX(lb->merge_stack, i, svn_revnum_t);
SVN_ERR(svn_cmdline_printf(pool, " r%ld%c", rev,
i == lb->merge_stack->nelts - 1 ?
'\n' : ','));
}
}
if (message != NULL) {
SVN_ERR(svn_cmdline_printf(pool, "\n%s\n", message));
}
SVN_ERR(svn_cmdline_fflush(stdout));
if (log_entry->has_children)
APR_ARRAY_PUSH(lb->merge_stack, svn_revnum_t) = log_entry->revision;
return SVN_NO_ERROR;
}
static svn_error_t *
log_entry_receiver_xml(void *baton,
svn_log_entry_t *log_entry,
apr_pool_t *pool) {
struct log_receiver_baton *lb = baton;
svn_stringbuf_t *sb = svn_stringbuf_create("", pool);
char *revstr;
const char *author;
const char *date;
const char *message;
if (lb->cancel_func)
SVN_ERR(lb->cancel_func(lb->cancel_baton));
svn_compat_log_revprops_out(&author, &date, &message, log_entry->revprops);
if (author)
author = svn_xml_fuzzy_escape(author, pool);
if (date)
date = svn_xml_fuzzy_escape(date, pool);
if (message)
message = svn_xml_fuzzy_escape(message, pool);
if (log_entry->revision == 0 && message == NULL)
return SVN_NO_ERROR;
if (! SVN_IS_VALID_REVNUM(log_entry->revision)) {
svn_xml_make_close_tag(&sb, pool, "logentry");
SVN_ERR(svn_cl__error_checked_fputs(sb->data, stdout));
apr_array_pop(lb->merge_stack);
return SVN_NO_ERROR;
}
revstr = apr_psprintf(pool, "%ld", log_entry->revision);
svn_xml_make_open_tag(&sb, pool, svn_xml_normal, "logentry",
"revision", revstr, NULL);
svn_cl__xml_tagged_cdata(&sb, pool, "author", author);
if (date && date[0] == '\0')
date = NULL;
svn_cl__xml_tagged_cdata(&sb, pool, "date", date);
if (log_entry->changed_paths) {
apr_hash_index_t *hi;
char *path;
svn_xml_make_open_tag(&sb, pool, svn_xml_normal, "paths",
NULL);
for (hi = apr_hash_first(pool, log_entry->changed_paths);
hi != NULL;
hi = apr_hash_next(hi)) {
void *val;
char action[2];
svn_log_changed_path_t *log_item;
apr_hash_this(hi, (void *) &path, NULL, &val);
log_item = val;
action[0] = log_item->action;
action[1] = '\0';
if (log_item->copyfrom_path
&& SVN_IS_VALID_REVNUM(log_item->copyfrom_rev)) {
svn_stringbuf_t *escpath = svn_stringbuf_create("", pool);
svn_xml_escape_attr_cstring(&escpath,
log_item->copyfrom_path, pool);
revstr = apr_psprintf(pool, "%ld",
log_item->copyfrom_rev);
svn_xml_make_open_tag(&sb, pool, svn_xml_protect_pcdata, "path",
"action", action,
"copyfrom-path", escpath->data,
"copyfrom-rev", revstr, NULL);
} else {
svn_xml_make_open_tag(&sb, pool, svn_xml_protect_pcdata, "path",
"action", action, NULL);
}
svn_xml_escape_cdata_cstring(&sb, path, pool);
svn_xml_make_close_tag(&sb, pool, "path");
}
svn_xml_make_close_tag(&sb, pool, "paths");
}
if (message != NULL) {
svn_cl__xml_tagged_cdata(&sb, pool, "msg", message);
}
svn_compat_log_revprops_clear(log_entry->revprops);
if (log_entry->revprops && apr_hash_count(log_entry->revprops) > 0) {
svn_xml_make_open_tag(&sb, pool, svn_xml_normal, "revprops", NULL);
SVN_ERR(svn_cl__print_xml_prop_hash(&sb, log_entry->revprops,
FALSE,
pool));
svn_xml_make_close_tag(&sb, pool, "revprops");
}
if (log_entry->has_children)
APR_ARRAY_PUSH(lb->merge_stack, svn_revnum_t) = log_entry->revision;
else
svn_xml_make_close_tag(&sb, pool, "logentry");
SVN_ERR(svn_cl__error_checked_fputs(sb->data, stdout));
return SVN_NO_ERROR;
}
svn_error_t *
svn_cl__log(apr_getopt_t *os,
void *baton,
apr_pool_t *pool) {
svn_cl__opt_state_t *opt_state = ((svn_cl__cmd_baton_t *) baton)->opt_state;
svn_client_ctx_t *ctx = ((svn_cl__cmd_baton_t *) baton)->ctx;
apr_array_header_t *targets;
struct log_receiver_baton lb;
const char *target;
int i;
svn_opt_revision_t peg_revision;
const char *true_path;
apr_array_header_t *revprops;
if (!opt_state->xml) {
if (opt_state->all_revprops)
return svn_error_create(SVN_ERR_CL_ARG_PARSING_ERROR, NULL,
_("'with-all-revprops' option only valid in"
" XML mode"));
if (opt_state->revprop_table != NULL)
return svn_error_create(SVN_ERR_CL_ARG_PARSING_ERROR, NULL,
_("'with-revprop' option only valid in"
" XML mode"));
}
SVN_ERR(svn_cl__args_to_target_array_print_reserved(&targets, os,
opt_state->targets,
pool));
svn_opt_push_implicit_dot_target(targets, pool);
target = APR_ARRAY_IDX(targets, 0, const char *);
if (opt_state->used_change_arg) {
if (opt_state->start_revision.value.number <
opt_state->end_revision.value.number)
opt_state->start_revision = opt_state->end_revision;
else
opt_state->end_revision = opt_state->start_revision;
}
SVN_ERR(svn_opt_parse_path(&peg_revision, &true_path, target, pool));
APR_ARRAY_IDX(targets, 0, const char *) = true_path;
if ((opt_state->start_revision.kind != svn_opt_revision_unspecified)
&& (opt_state->end_revision.kind == svn_opt_revision_unspecified)) {
opt_state->end_revision = opt_state->start_revision;
} else if (opt_state->start_revision.kind == svn_opt_revision_unspecified) {
if (peg_revision.kind == svn_opt_revision_unspecified) {
if (svn_path_is_url(target))
opt_state->start_revision.kind = svn_opt_revision_head;
else
opt_state->start_revision.kind = svn_opt_revision_base;
} else
opt_state->start_revision = peg_revision;
if (opt_state->end_revision.kind == svn_opt_revision_unspecified) {
opt_state->end_revision.kind = svn_opt_revision_number;
opt_state->end_revision.value.number = 0;
}
}
if (svn_path_is_url(target)) {
for (i = 1; i < targets->nelts; i++) {
target = APR_ARRAY_IDX(targets, i, const char *);
if (svn_path_is_url(target))
return svn_error_create(SVN_ERR_UNSUPPORTED_FEATURE, NULL,
_("Only relative paths can be specified "
"after a URL"));
}
}
lb.cancel_func = ctx->cancel_func;
lb.cancel_baton = ctx->cancel_baton;
lb.omit_log_message = opt_state->quiet;
lb.merge_stack = apr_array_make(pool, 0, sizeof(svn_revnum_t));
lb.pool = pool;
if (! opt_state->quiet)
svn_cl__get_notifier(&ctx->notify_func2, &ctx->notify_baton2, FALSE,
FALSE, FALSE, pool);
if (opt_state->xml) {
if (! opt_state->incremental)
SVN_ERR(svn_cl__xml_print_header("log", pool));
if (opt_state->all_revprops)
revprops = NULL;
else if (opt_state->revprop_table != NULL) {
apr_hash_index_t *hi;
revprops = apr_array_make(pool,
apr_hash_count(opt_state->revprop_table),
sizeof(char *));
for (hi = apr_hash_first(pool, opt_state->revprop_table);
hi != NULL;
hi = apr_hash_next(hi)) {
char *property;
svn_string_t *value;
apr_hash_this(hi, (void *)&property, NULL, (void *)&value);
if (value && value->data[0] != '\0')
return svn_error_createf(SVN_ERR_CL_ARG_PARSING_ERROR, NULL,
_("cannot assign with 'with-revprop'"
" option (drop the '=')"));
APR_ARRAY_PUSH(revprops, char *) = property;
}
} else {
revprops = apr_array_make(pool, 3, sizeof(char *));
APR_ARRAY_PUSH(revprops, const char *) = SVN_PROP_REVISION_AUTHOR;
APR_ARRAY_PUSH(revprops, const char *) = SVN_PROP_REVISION_DATE;
if (!opt_state->quiet)
APR_ARRAY_PUSH(revprops, const char *) = SVN_PROP_REVISION_LOG;
}
SVN_ERR(svn_client_log4(targets,
&peg_revision,
&(opt_state->start_revision),
&(opt_state->end_revision),
opt_state->limit,
opt_state->verbose,
opt_state->stop_on_copy,
opt_state->use_merge_history,
revprops,
log_entry_receiver_xml,
&lb,
ctx,
pool));
if (! opt_state->incremental)
SVN_ERR(svn_cl__xml_print_footer("log", pool));
} else {
revprops = apr_array_make(pool, 3, sizeof(char *));
APR_ARRAY_PUSH(revprops, const char *) = SVN_PROP_REVISION_AUTHOR;
APR_ARRAY_PUSH(revprops, const char *) = SVN_PROP_REVISION_DATE;
if (!opt_state->quiet)
APR_ARRAY_PUSH(revprops, const char *) = SVN_PROP_REVISION_LOG;
SVN_ERR(svn_client_log4(targets,
&peg_revision,
&(opt_state->start_revision),
&(opt_state->end_revision),
opt_state->limit,
opt_state->verbose,
opt_state->stop_on_copy,
opt_state->use_merge_history,
revprops,
log_entry_receiver,
&lb,
ctx,
pool));
if (! opt_state->incremental)
SVN_ERR(svn_cmdline_printf(pool, SEP_STRING));
}
return SVN_NO_ERROR;
}
