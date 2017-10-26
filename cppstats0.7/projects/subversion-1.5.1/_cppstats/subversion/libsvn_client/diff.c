#include <apr_strings.h>
#include <apr_pools.h>
#include <apr_hash.h>
#include "svn_types.h"
#include "svn_hash.h"
#include "svn_wc.h"
#include "svn_delta.h"
#include "svn_diff.h"
#include "svn_mergeinfo.h"
#include "svn_client.h"
#include "svn_string.h"
#include "svn_error.h"
#include "svn_path.h"
#include "svn_io.h"
#include "svn_utf.h"
#include "svn_pools.h"
#include "svn_config.h"
#include "svn_props.h"
#include "svn_time.h"
#include "svn_sorts.h"
#include "client.h"
#include <assert.h>
#include "private/svn_wc_private.h"
#include "svn_private_config.h"
static const char equal_string[] =
"===================================================================";
static const char under_string[] =
"___________________________________________________________________";
static svn_error_t *
file_printf_from_utf8(apr_file_t *fptr, const char *encoding,
const char *format, ...)
__attribute__ ((format(printf, 3, 4)));
static svn_error_t *
file_printf_from_utf8(apr_file_t *fptr, const char *encoding,
const char *format, ...) {
va_list ap;
const char *buf, *buf_apr;
va_start(ap, format);
buf = apr_pvsprintf(apr_file_pool_get(fptr), format, ap);
va_end(ap);
SVN_ERR(svn_utf_cstring_from_utf8_ex2(&buf_apr, buf, encoding,
apr_file_pool_get(fptr)));
return svn_io_file_write_full(fptr, buf_apr, strlen(buf_apr),
NULL, apr_file_pool_get(fptr));
}
static svn_error_t *
display_mergeinfo_diff(const char *old_mergeinfo_val,
const char *new_mergeinfo_val,
const char *encoding,
apr_file_t *file,
apr_pool_t *pool) {
apr_hash_t *old_mergeinfo_hash, *new_mergeinfo_hash, *added, *deleted;
apr_hash_index_t *hi;
const char *from_path;
apr_array_header_t *merge_revarray;
if (old_mergeinfo_val)
SVN_ERR(svn_mergeinfo_parse(&old_mergeinfo_hash, old_mergeinfo_val, pool));
else
old_mergeinfo_hash = NULL;
if (new_mergeinfo_val)
SVN_ERR(svn_mergeinfo_parse(&new_mergeinfo_hash, new_mergeinfo_val, pool));
else
new_mergeinfo_hash = NULL;
SVN_ERR(svn_mergeinfo_diff(&deleted, &added, old_mergeinfo_hash,
new_mergeinfo_hash,
TRUE, pool));
for (hi = apr_hash_first(pool, deleted);
hi; hi = apr_hash_next(hi)) {
const void *key;
void *val;
svn_string_t *merge_revstr;
apr_hash_this(hi, &key, NULL, &val);
from_path = key;
merge_revarray = val;
SVN_ERR(svn_rangelist_to_string(&merge_revstr, merge_revarray, pool));
SVN_ERR(file_printf_from_utf8(file, encoding,
_(" Reverse-merged %s:r%s%s"),
from_path, merge_revstr->data,
APR_EOL_STR));
}
for (hi = apr_hash_first(pool, added);
hi; hi = apr_hash_next(hi)) {
const void *key;
void *val;
svn_string_t *merge_revstr;
apr_hash_this(hi, &key, NULL, &val);
from_path = key;
merge_revarray = val;
SVN_ERR(svn_rangelist_to_string(&merge_revstr, merge_revarray, pool));
SVN_ERR(file_printf_from_utf8(file, encoding,
_(" Merged %s:r%s%s"),
from_path, merge_revstr->data,
APR_EOL_STR));
}
return SVN_NO_ERROR;
}
#define MAKE_ERR_BAD_RELATIVE_PATH(path, relative_to_dir) svn_error_createf(SVN_ERR_BAD_RELATIVE_PATH, NULL, _("Path '%s' must be an immediate child of " "the directory '%s'"), path, relative_to_dir)
static svn_error_t *
display_prop_diffs(const apr_array_header_t *propchanges,
apr_hash_t *original_props,
const char *path,
const char *encoding,
apr_file_t *file,
const char *relative_to_dir,
apr_pool_t *pool) {
int i;
if (relative_to_dir) {
const char *child_path = svn_path_is_child(relative_to_dir, path, pool);
if (child_path)
path = child_path;
else if (!svn_path_compare_paths(relative_to_dir, path))
path = ".";
else
return MAKE_ERR_BAD_RELATIVE_PATH(path, relative_to_dir);
}
SVN_ERR(file_printf_from_utf8(file, encoding,
_("%sProperty changes on: %s%s"),
APR_EOL_STR,
svn_path_local_style(path, pool),
APR_EOL_STR));
SVN_ERR(file_printf_from_utf8(file, encoding, "%s" APR_EOL_STR,
under_string));
for (i = 0; i < propchanges->nelts; i++) {
const char *header_fmt;
const svn_string_t *original_value;
const svn_prop_t *propchange =
&APR_ARRAY_IDX(propchanges, i, svn_prop_t);
if (original_props)
original_value = apr_hash_get(original_props,
propchange->name, APR_HASH_KEY_STRING);
else
original_value = NULL;
if ((! (original_value || propchange->value))
|| (original_value && propchange->value
&& svn_string_compare(original_value, propchange->value)))
continue;
if (! original_value)
header_fmt = _("Added: %s%s");
else if (! propchange->value)
header_fmt = _("Deleted: %s%s");
else
header_fmt = _("Modified: %s%s");
SVN_ERR(file_printf_from_utf8(file, encoding, header_fmt,
propchange->name, APR_EOL_STR));
if (strcmp(propchange->name, SVN_PROP_MERGEINFO) == 0) {
const char *orig = original_value ? original_value->data : NULL;
const char *val = propchange->value ? propchange->value->data : NULL;
SVN_ERR(display_mergeinfo_diff(orig, val, encoding, file, pool));
continue;
}
{
svn_boolean_t val_is_utf8 = svn_prop_is_svn_prop(propchange->name);
if (original_value != NULL) {
if (val_is_utf8) {
SVN_ERR(file_printf_from_utf8
(file, encoding,
" - %s" APR_EOL_STR, original_value->data));
} else {
apr_file_printf
(file, " - %s" APR_EOL_STR, original_value->data);
}
}
if (propchange->value != NULL) {
if (val_is_utf8) {
SVN_ERR(file_printf_from_utf8
(file, encoding, " + %s" APR_EOL_STR,
propchange->value->data));
} else {
apr_file_printf(file, " + %s" APR_EOL_STR,
propchange->value->data);
}
}
}
}
apr_file_printf(file, APR_EOL_STR);
return SVN_NO_ERROR;
}
struct diff_cmd_baton {
const char *diff_cmd;
union {
svn_diff_file_options_t *for_internal;
struct {
const char **argv;
int argc;
} for_external;
} options;
apr_pool_t *pool;
apr_file_t *outfile;
apr_file_t *errfile;
const char *header_encoding;
const char *orig_path_1;
const char *orig_path_2;
svn_revnum_t revnum1;
svn_revnum_t revnum2;
svn_boolean_t force_binary;
svn_boolean_t force_empty;
const char *relative_to_dir;
};
static const char *
diff_label(const char *path,
svn_revnum_t revnum,
apr_pool_t *pool) {
const char *label;
if (revnum != SVN_INVALID_REVNUM)
label = apr_psprintf(pool, _("%s\t(revision %ld)"), path, revnum);
else
label = apr_psprintf(pool, _("%s\t(working copy)"), path);
return label;
}
static svn_error_t *
diff_props_changed(svn_wc_adm_access_t *adm_access,
svn_wc_notify_state_t *state,
const char *path,
const apr_array_header_t *propchanges,
apr_hash_t *original_props,
void *diff_baton) {
struct diff_cmd_baton *diff_cmd_baton = diff_baton;
apr_array_header_t *props;
apr_pool_t *subpool = svn_pool_create(diff_cmd_baton->pool);
SVN_ERR(svn_categorize_props(propchanges, NULL, NULL, &props, subpool));
if (props->nelts > 0)
SVN_ERR(display_prop_diffs(props, original_props, path,
diff_cmd_baton->header_encoding,
diff_cmd_baton->outfile,
diff_cmd_baton->relative_to_dir,
subpool));
if (state)
*state = svn_wc_notify_state_unknown;
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
static svn_error_t *
diff_content_changed(const char *path,
const char *tmpfile1,
const char *tmpfile2,
svn_revnum_t rev1,
svn_revnum_t rev2,
const char *mimetype1,
const char *mimetype2,
void *diff_baton) {
struct diff_cmd_baton *diff_cmd_baton = diff_baton;
int exitcode;
apr_pool_t *subpool = svn_pool_create(diff_cmd_baton->pool);
svn_stream_t *os;
const char *rel_to_dir = diff_cmd_baton->relative_to_dir;
apr_file_t *errfile = diff_cmd_baton->errfile;
const char *label1, *label2;
svn_boolean_t mt1_binary = FALSE, mt2_binary = FALSE;
const char *path1, *path2;
int i;
os = svn_stream_from_aprfile(diff_cmd_baton->outfile, subpool);
path1 = diff_cmd_baton->orig_path_1;
path2 = diff_cmd_baton->orig_path_2;
for (i = 0; path1[i] && path2[i] && (path1[i] == path2[i]); i++)
;
if (path1[i] || path2[i]) {
for ( ; (i > 0) && (path1[i] != '/'); i--)
;
}
path1 = path1 + i;
path2 = path2 + i;
if (path1[0] == '\0')
path1 = apr_psprintf(subpool, "%s", path);
else if (path1[0] == '/')
path1 = apr_psprintf(subpool, "%s\t(...%s)", path, path1);
else
path1 = apr_psprintf(subpool, "%s\t(.../%s)", path, path1);
if (path2[0] == '\0')
path2 = apr_psprintf(subpool, "%s", path);
else if (path2[0] == '/')
path2 = apr_psprintf(subpool, "%s\t(...%s)", path, path2);
else
path2 = apr_psprintf(subpool, "%s\t(.../%s)", path, path2);
if (diff_cmd_baton->relative_to_dir) {
const char *child_path = svn_path_is_child(rel_to_dir, path, subpool);
if (child_path)
path = child_path;
else if (!svn_path_compare_paths(rel_to_dir, path))
path = ".";
else
return MAKE_ERR_BAD_RELATIVE_PATH(path, rel_to_dir);
child_path = svn_path_is_child(rel_to_dir, path1, subpool);
if (child_path)
path1 = child_path;
else if (!svn_path_compare_paths(rel_to_dir, path1))
path1 = ".";
else
return MAKE_ERR_BAD_RELATIVE_PATH(path1, rel_to_dir);
child_path = svn_path_is_child(rel_to_dir, path2, subpool);
if (child_path)
path2 = child_path;
else if (!svn_path_compare_paths(rel_to_dir, path2))
path2 = ".";
else
return MAKE_ERR_BAD_RELATIVE_PATH(path2, rel_to_dir);
}
label1 = diff_label(path1, rev1, subpool);
label2 = diff_label(path2, rev2, subpool);
if (mimetype1)
mt1_binary = svn_mime_type_is_binary(mimetype1);
if (mimetype2)
mt2_binary = svn_mime_type_is_binary(mimetype2);
if (! diff_cmd_baton->force_binary && (mt1_binary || mt2_binary)) {
SVN_ERR(svn_stream_printf_from_utf8
(os, diff_cmd_baton->header_encoding, subpool,
"Index: %s" APR_EOL_STR "%s" APR_EOL_STR, path, equal_string));
SVN_ERR(svn_stream_printf_from_utf8
(os, diff_cmd_baton->header_encoding, subpool,
_("Cannot display: file marked as a binary type.%s"),
APR_EOL_STR));
if (mt1_binary && !mt2_binary)
SVN_ERR(svn_stream_printf_from_utf8
(os, diff_cmd_baton->header_encoding, subpool,
"svn:mime-type = %s" APR_EOL_STR, mimetype1));
else if (mt2_binary && !mt1_binary)
SVN_ERR(svn_stream_printf_from_utf8
(os, diff_cmd_baton->header_encoding, subpool,
"svn:mime-type = %s" APR_EOL_STR, mimetype2));
else if (mt1_binary && mt2_binary) {
if (strcmp(mimetype1, mimetype2) == 0)
SVN_ERR(svn_stream_printf_from_utf8
(os, diff_cmd_baton->header_encoding, subpool,
"svn:mime-type = %s" APR_EOL_STR,
mimetype1));
else
SVN_ERR(svn_stream_printf_from_utf8
(os, diff_cmd_baton->header_encoding, subpool,
"svn:mime-type = (%s, %s)" APR_EOL_STR,
mimetype1, mimetype2));
}
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
if (diff_cmd_baton->diff_cmd) {
SVN_ERR(svn_stream_printf_from_utf8
(os, diff_cmd_baton->header_encoding, subpool,
"Index: %s" APR_EOL_STR "%s" APR_EOL_STR, path, equal_string));
SVN_ERR(svn_stream_close(os));
SVN_ERR(svn_io_run_diff(".",
diff_cmd_baton->options.for_external.argv,
diff_cmd_baton->options.for_external.argc,
label1, label2,
tmpfile1, tmpfile2,
&exitcode, diff_cmd_baton->outfile, errfile,
diff_cmd_baton->diff_cmd, subpool));
} else {
svn_diff_t *diff;
SVN_ERR(svn_diff_file_diff_2(&diff, tmpfile1, tmpfile2,
diff_cmd_baton->options.for_internal,
subpool));
if (svn_diff_contains_diffs(diff) || diff_cmd_baton->force_empty) {
SVN_ERR(svn_stream_printf_from_utf8
(os, diff_cmd_baton->header_encoding, subpool,
"Index: %s" APR_EOL_STR "%s" APR_EOL_STR,
path, equal_string));
SVN_ERR(svn_diff_file_output_unified3
(os, diff, tmpfile1, tmpfile2, label1, label2,
diff_cmd_baton->header_encoding, rel_to_dir,
diff_cmd_baton->options.for_internal->show_c_function,
subpool));
}
}
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
static svn_error_t *
diff_file_changed(svn_wc_adm_access_t *adm_access,
svn_wc_notify_state_t *content_state,
svn_wc_notify_state_t *prop_state,
const char *path,
const char *tmpfile1,
const char *tmpfile2,
svn_revnum_t rev1,
svn_revnum_t rev2,
const char *mimetype1,
const char *mimetype2,
const apr_array_header_t *prop_changes,
apr_hash_t *original_props,
void *diff_baton) {
if (tmpfile1)
SVN_ERR(diff_content_changed(path,
tmpfile1, tmpfile2, rev1, rev2,
mimetype1, mimetype2, diff_baton));
if (prop_changes->nelts > 0)
SVN_ERR(diff_props_changed(adm_access, prop_state, path, prop_changes,
original_props, diff_baton));
if (content_state)
*content_state = svn_wc_notify_state_unknown;
if (prop_state)
*prop_state = svn_wc_notify_state_unknown;
return SVN_NO_ERROR;
}
static svn_error_t *
diff_file_added(svn_wc_adm_access_t *adm_access,
svn_wc_notify_state_t *content_state,
svn_wc_notify_state_t *prop_state,
const char *path,
const char *tmpfile1,
const char *tmpfile2,
svn_revnum_t rev1,
svn_revnum_t rev2,
const char *mimetype1,
const char *mimetype2,
const apr_array_header_t *prop_changes,
apr_hash_t *original_props,
void *diff_baton) {
struct diff_cmd_baton *diff_cmd_baton = diff_baton;
diff_cmd_baton->force_empty = TRUE;
SVN_ERR(diff_file_changed(adm_access, content_state, prop_state, path,
tmpfile1, tmpfile2,
rev1, rev2,
mimetype1, mimetype2,
prop_changes, original_props, diff_baton));
diff_cmd_baton->force_empty = FALSE;
return SVN_NO_ERROR;
}
static svn_error_t *
diff_file_deleted_with_diff(svn_wc_adm_access_t *adm_access,
svn_wc_notify_state_t *state,
const char *path,
const char *tmpfile1,
const char *tmpfile2,
const char *mimetype1,
const char *mimetype2,
apr_hash_t *original_props,
void *diff_baton) {
struct diff_cmd_baton *diff_cmd_baton = diff_baton;
return diff_file_changed(adm_access, state, NULL, path,
tmpfile1, tmpfile2,
diff_cmd_baton->revnum1, diff_cmd_baton->revnum2,
mimetype1, mimetype2,
apr_array_make(diff_cmd_baton->pool, 1,
sizeof(svn_prop_t)),
apr_hash_make(diff_cmd_baton->pool), diff_baton);
}
static svn_error_t *
diff_file_deleted_no_diff(svn_wc_adm_access_t *adm_access,
svn_wc_notify_state_t *state,
const char *path,
const char *tmpfile1,
const char *tmpfile2,
const char *mimetype1,
const char *mimetype2,
apr_hash_t *original_props,
void *diff_baton) {
struct diff_cmd_baton *diff_cmd_baton = diff_baton;
if (state)
*state = svn_wc_notify_state_unknown;
SVN_ERR(file_printf_from_utf8
(diff_cmd_baton->outfile,
diff_cmd_baton->header_encoding,
"Index: %s (deleted)" APR_EOL_STR "%s" APR_EOL_STR,
path, equal_string));
return SVN_NO_ERROR;
}
static svn_error_t *
diff_dir_added(svn_wc_adm_access_t *adm_access,
svn_wc_notify_state_t *state,
const char *path,
svn_revnum_t rev,
void *diff_baton) {
if (state)
*state = svn_wc_notify_state_unknown;
return SVN_NO_ERROR;
}
static svn_error_t *
diff_dir_deleted(svn_wc_adm_access_t *adm_access,
svn_wc_notify_state_t *state,
const char *path,
void *diff_baton) {
if (state)
*state = svn_wc_notify_state_unknown;
return SVN_NO_ERROR;
}
static svn_error_t *
convert_to_url(const char **url,
const char *path,
apr_pool_t *pool) {
svn_wc_adm_access_t *adm_access;
const svn_wc_entry_t *entry;
if (svn_path_is_url(path)) {
*url = path;
return SVN_NO_ERROR;
}
SVN_ERR(svn_wc_adm_probe_open3(&adm_access, NULL, path, FALSE,
0, NULL, NULL, pool));
SVN_ERR(svn_wc__entry_versioned(&entry, path, adm_access, FALSE, pool));
SVN_ERR(svn_wc_adm_close(adm_access));
if (entry->url)
*url = apr_pstrdup(pool, entry->url);
else
*url = apr_pstrdup(pool, entry->copyfrom_url);
return SVN_NO_ERROR;
}
struct diff_parameters {
const char *path1;
const svn_opt_revision_t *revision1;
const char *path2;
const svn_opt_revision_t *revision2;
const svn_opt_revision_t *peg_revision;
svn_depth_t depth;
svn_boolean_t ignore_ancestry;
svn_boolean_t no_diff_deleted;
const apr_array_header_t *changelists;
};
struct diff_paths {
svn_boolean_t is_repos1;
svn_boolean_t is_repos2;
};
static svn_error_t *
check_paths(const struct diff_parameters *params,
struct diff_paths *paths) {
svn_boolean_t is_local_rev1, is_local_rev2;
if ((params->revision1->kind == svn_opt_revision_unspecified)
|| (params->revision2->kind == svn_opt_revision_unspecified))
return svn_error_create(SVN_ERR_CLIENT_BAD_REVISION, NULL,
_("Not all required revisions are specified"));
is_local_rev1 =
((params->revision1->kind == svn_opt_revision_base)
|| (params->revision1->kind == svn_opt_revision_working));
is_local_rev2 =
((params->revision2->kind == svn_opt_revision_base)
|| (params->revision2->kind == svn_opt_revision_working));
if (params->peg_revision->kind != svn_opt_revision_unspecified) {
if (is_local_rev1 && is_local_rev2)
return svn_error_create(SVN_ERR_CLIENT_BAD_REVISION, NULL,
_("At least one revision must be non-local "
"for a pegged diff"));
paths->is_repos1 = ! is_local_rev1;
paths->is_repos2 = ! is_local_rev2;
} else {
paths->is_repos1 = ! is_local_rev1 || svn_path_is_url(params->path1);
paths->is_repos2 = ! is_local_rev2 || svn_path_is_url(params->path2);
}
return SVN_NO_ERROR;
}
struct diff_repos_repos_t {
const char *url1;
const char *url2;
const char *base_path;
svn_boolean_t same_urls;
svn_revnum_t rev1;
svn_revnum_t rev2;
const char *anchor1;
const char *anchor2;
const char *target1;
const char *target2;
svn_ra_session_t *ra_session;
};
static svn_error_t *
diff_prepare_repos_repos(const struct diff_parameters *params,
struct diff_repos_repos_t *drr,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
svn_ra_session_t *ra_session;
svn_node_kind_t kind1, kind2;
SVN_ERR(convert_to_url(&drr->url1, params->path1, pool));
SVN_ERR(convert_to_url(&drr->url2, params->path2, pool));
drr->same_urls = (strcmp(drr->url1, drr->url2) == 0);
drr->base_path = NULL;
if (drr->url1 != params->path1)
drr->base_path = params->path1;
if (drr->url2 != params->path2)
drr->base_path = params->path2;
SVN_ERR(svn_client__open_ra_session_internal(&ra_session, drr->url2,
NULL, NULL, NULL, FALSE,
TRUE, ctx, pool));
if (params->peg_revision->kind != svn_opt_revision_unspecified) {
svn_opt_revision_t *start_ignore, *end_ignore;
SVN_ERR(svn_client__repos_locations(&drr->url1, &start_ignore,
&drr->url2, &end_ignore,
ra_session,
params->path2,
params->peg_revision,
params->revision1,
params->revision2,
ctx, pool));
SVN_ERR(svn_ra_reparent(ra_session, drr->url2, pool));
}
SVN_ERR(svn_client__get_revision_number
(&drr->rev2, NULL, ra_session, params->revision2,
(params->path2 == drr->url2) ? NULL : params->path2, pool));
SVN_ERR(svn_ra_check_path(ra_session, "", drr->rev2, &kind2, pool));
if (kind2 == svn_node_none)
return svn_error_createf
(SVN_ERR_FS_NOT_FOUND, NULL,
_("'%s' was not found in the repository at revision %ld"),
drr->url2, drr->rev2);
SVN_ERR(svn_ra_reparent(ra_session, drr->url1, pool));
SVN_ERR(svn_client__get_revision_number
(&drr->rev1, NULL, ra_session, params->revision1,
(params->path1 == drr->url1) ? NULL : params->path1, pool));
SVN_ERR(svn_ra_check_path(ra_session, "", drr->rev1, &kind1, pool));
if (kind1 == svn_node_none)
return svn_error_createf
(SVN_ERR_FS_NOT_FOUND, NULL,
_("'%s' was not found in the repository at revision %ld"),
drr->url1, drr->rev1);
drr->anchor1 = drr->url1;
drr->anchor2 = drr->url2;
drr->target1 = "";
drr->target2 = "";
if ((kind1 == svn_node_file) || (kind2 == svn_node_file)) {
svn_path_split(drr->url1, &drr->anchor1, &drr->target1, pool);
drr->target1 = svn_path_uri_decode(drr->target1, pool);
svn_path_split(drr->url2, &drr->anchor2, &drr->target2, pool);
drr->target2 = svn_path_uri_decode(drr->target2, pool);
if (drr->base_path)
drr->base_path = svn_path_dirname(drr->base_path, pool);
SVN_ERR(svn_ra_reparent(ra_session, drr->anchor1, pool));
}
drr->ra_session = ra_session;
return SVN_NO_ERROR;
}
static svn_error_t *
unsupported_diff_error(svn_error_t *child_err) {
return svn_error_create(SVN_ERR_INCORRECT_PARAMS, child_err,
_("Sorry, svn_client_diff4 was called in a way "
"that is not yet supported"));
}
static svn_error_t *
diff_wc_wc(const char *path1,
const svn_opt_revision_t *revision1,
const char *path2,
const svn_opt_revision_t *revision2,
svn_depth_t depth,
svn_boolean_t ignore_ancestry,
const apr_array_header_t *changelists,
const svn_wc_diff_callbacks2_t *callbacks,
struct diff_cmd_baton *callback_baton,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
svn_wc_adm_access_t *adm_access, *target_access;
const char *target;
int levels_to_lock = SVN_WC__LEVELS_TO_LOCK_FROM_DEPTH(depth);
assert(! svn_path_is_url(path1));
assert(! svn_path_is_url(path2));
if ((strcmp(path1, path2) != 0)
|| (! ((revision1->kind == svn_opt_revision_base)
&& (revision2->kind == svn_opt_revision_working))))
return unsupported_diff_error
(svn_error_create
(SVN_ERR_INCORRECT_PARAMS, NULL,
_("Only diffs between a path's text-base "
"and its working files are supported at this time")));
SVN_ERR(svn_wc_adm_open_anchor(&adm_access, &target_access, &target,
path1, FALSE, levels_to_lock,
ctx->cancel_func, ctx->cancel_baton,
pool));
SVN_ERR(svn_client__get_revision_number
(&callback_baton->revnum1, NULL, NULL, revision1, path1, pool));
callback_baton->revnum2 = SVN_INVALID_REVNUM;
SVN_ERR(svn_wc_diff4(adm_access, target, callbacks, callback_baton,
depth, ignore_ancestry, changelists, pool));
SVN_ERR(svn_wc_adm_close(adm_access));
return SVN_NO_ERROR;
}
static svn_error_t *
diff_repos_repos(const struct diff_parameters *diff_param,
const svn_wc_diff_callbacks2_t *callbacks,
struct diff_cmd_baton *callback_baton,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
svn_ra_session_t *extra_ra_session;
const svn_ra_reporter3_t *reporter;
void *report_baton;
const svn_delta_editor_t *diff_editor;
void *diff_edit_baton;
struct diff_repos_repos_t drr;
SVN_ERR(diff_prepare_repos_repos(diff_param, &drr, ctx, pool));
callback_baton->orig_path_1 = drr.url1;
callback_baton->orig_path_2 = drr.url2;
callback_baton->revnum1 = drr.rev1;
callback_baton->revnum2 = drr.rev2;
SVN_ERR(svn_client__open_ra_session_internal
(&extra_ra_session, drr.anchor1, NULL, NULL, NULL, FALSE, TRUE, ctx,
pool));
SVN_ERR(svn_client__get_diff_editor
(drr.base_path ? drr.base_path : "",
NULL, callbacks, callback_baton, diff_param->depth,
FALSE , extra_ra_session, drr.rev1,
NULL , NULL ,
ctx->cancel_func, ctx->cancel_baton,
&diff_editor, &diff_edit_baton, pool));
SVN_ERR(svn_ra_do_diff3
(drr.ra_session, &reporter, &report_baton, drr.rev2, drr.target1,
diff_param->depth, diff_param->ignore_ancestry, TRUE,
drr.url2, diff_editor, diff_edit_baton, pool));
SVN_ERR(reporter->set_path(report_baton, "", drr.rev1,
svn_depth_infinity,
FALSE, NULL,
pool));
SVN_ERR(reporter->finish_report(report_baton, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
diff_repos_wc(const char *path1,
const svn_opt_revision_t *revision1,
const svn_opt_revision_t *peg_revision,
const char *path2,
const svn_opt_revision_t *revision2,
svn_boolean_t reverse,
svn_depth_t depth,
svn_boolean_t ignore_ancestry,
const apr_array_header_t *changelists,
const svn_wc_diff_callbacks2_t *callbacks,
struct diff_cmd_baton *callback_baton,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
const char *url1, *anchor, *anchor_url, *target;
svn_wc_adm_access_t *adm_access, *dir_access;
const svn_wc_entry_t *entry;
svn_revnum_t rev;
svn_ra_session_t *ra_session;
const svn_ra_reporter3_t *reporter;
void *report_baton;
const svn_delta_editor_t *diff_editor;
void *diff_edit_baton;
svn_boolean_t rev2_is_base = (revision2->kind == svn_opt_revision_base);
int levels_to_lock = SVN_WC__LEVELS_TO_LOCK_FROM_DEPTH(depth);
svn_boolean_t server_supports_depth;
assert(! svn_path_is_url(path2));
SVN_ERR(convert_to_url(&url1, path1, pool));
SVN_ERR(svn_wc_adm_open_anchor(&adm_access, &dir_access, &target,
path2, FALSE, levels_to_lock,
ctx->cancel_func, ctx->cancel_baton,
pool));
anchor = svn_wc_adm_access_path(adm_access);
SVN_ERR(svn_wc__entry_versioned(&entry, anchor, adm_access, FALSE, pool));
if (! entry->url)
return svn_error_createf(SVN_ERR_ENTRY_MISSING_URL, NULL,
_("Directory '%s' has no URL"),
svn_path_local_style(anchor, pool));
anchor_url = apr_pstrdup(pool, entry->url);
if (peg_revision->kind != svn_opt_revision_unspecified) {
svn_opt_revision_t *start_ignore, *end_ignore, end;
const char *url_ignore;
end.kind = svn_opt_revision_unspecified;
SVN_ERR(svn_client__repos_locations(&url1, &start_ignore,
&url_ignore, &end_ignore,
NULL,
path1,
peg_revision,
revision1, &end,
ctx, pool));
if (!reverse) {
callback_baton->orig_path_1 = url1;
callback_baton->orig_path_2 = svn_path_join(anchor_url, target, pool);
} else {
callback_baton->orig_path_1 = svn_path_join(anchor_url, target, pool);
callback_baton->orig_path_2 = url1;
}
}
SVN_ERR(svn_client__open_ra_session_internal(&ra_session, anchor_url,
NULL, NULL, NULL, FALSE, TRUE,
ctx, pool));
SVN_ERR(svn_wc_get_diff_editor4(adm_access, target,
callbacks, callback_baton,
depth,
ignore_ancestry,
rev2_is_base,
reverse,
ctx->cancel_func, ctx->cancel_baton,
changelists,
&diff_editor, &diff_edit_baton,
pool));
SVN_ERR(svn_client__get_revision_number
(&rev, NULL, ra_session, revision1,
(path1 == url1) ? NULL : path1, pool));
if (!reverse)
callback_baton->revnum1 = rev;
else
callback_baton->revnum2 = rev;
SVN_ERR(svn_ra_do_diff3(ra_session,
&reporter, &report_baton,
rev,
target ? svn_path_uri_decode(target, pool) : NULL,
depth,
ignore_ancestry,
TRUE,
url1,
diff_editor, diff_edit_baton, pool));
SVN_ERR(svn_ra_has_capability(ra_session, &server_supports_depth,
SVN_RA_CAPABILITY_DEPTH, pool));
SVN_ERR(svn_wc_crawl_revisions3(path2, dir_access,
reporter, report_baton,
FALSE, depth, (! server_supports_depth),
FALSE, NULL, NULL,
NULL, pool));
SVN_ERR(svn_wc_adm_close(adm_access));
return SVN_NO_ERROR;
}
static svn_error_t *
do_diff(const struct diff_parameters *diff_param,
const svn_wc_diff_callbacks2_t *callbacks,
struct diff_cmd_baton *callback_baton,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
struct diff_paths diff_paths;
SVN_ERR(check_paths(diff_param, &diff_paths));
if (diff_paths.is_repos1) {
if (diff_paths.is_repos2) {
SVN_ERR(diff_repos_repos(diff_param, callbacks, callback_baton,
ctx, pool));
} else {
SVN_ERR(diff_repos_wc(diff_param->path1, diff_param->revision1,
diff_param->peg_revision,
diff_param->path2, diff_param->revision2,
FALSE, diff_param->depth,
diff_param->ignore_ancestry,
diff_param->changelists,
callbacks, callback_baton, ctx, pool));
}
} else {
if (diff_paths.is_repos2) {
SVN_ERR(diff_repos_wc(diff_param->path2, diff_param->revision2,
diff_param->peg_revision,
diff_param->path1, diff_param->revision1,
TRUE, diff_param->depth,
diff_param->ignore_ancestry,
diff_param->changelists,
callbacks, callback_baton, ctx, pool));
} else {
SVN_ERR(diff_wc_wc(diff_param->path1, diff_param->revision1,
diff_param->path2, diff_param->revision2,
diff_param->depth,
diff_param->ignore_ancestry,
diff_param->changelists,
callbacks, callback_baton, ctx, pool));
}
}
return SVN_NO_ERROR;
}
static svn_error_t *
diff_summarize_repos_repos(const struct diff_parameters *diff_param,
svn_client_diff_summarize_func_t summarize_func,
void *summarize_baton,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
svn_ra_session_t *extra_ra_session;
const svn_ra_reporter3_t *reporter;
void *report_baton;
const svn_delta_editor_t *diff_editor;
void *diff_edit_baton;
struct diff_repos_repos_t drr;
SVN_ERR(diff_prepare_repos_repos(diff_param, &drr, ctx, pool));
SVN_ERR(svn_client__open_ra_session_internal
(&extra_ra_session, drr.anchor1, NULL, NULL, NULL, FALSE, TRUE,
ctx, pool));
SVN_ERR(svn_client__get_diff_summarize_editor
(drr.target2, summarize_func,
summarize_baton, extra_ra_session, drr.rev1, ctx->cancel_func,
ctx->cancel_baton, &diff_editor, &diff_edit_baton, pool));
SVN_ERR(svn_ra_do_diff3
(drr.ra_session, &reporter, &report_baton, drr.rev2, drr.target1,
diff_param->depth, diff_param->ignore_ancestry,
FALSE , drr.url2, diff_editor,
diff_edit_baton, pool));
SVN_ERR(reporter->set_path(report_baton, "", drr.rev1,
svn_depth_infinity,
FALSE, NULL, pool));
SVN_ERR(reporter->finish_report(report_baton, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
do_diff_summarize(const struct diff_parameters *diff_param,
svn_client_diff_summarize_func_t summarize_func,
void *summarize_baton,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
struct diff_paths diff_paths;
SVN_ERR(check_paths(diff_param, &diff_paths));
if (diff_paths.is_repos1 && diff_paths.is_repos2) {
SVN_ERR(diff_summarize_repos_repos(diff_param, summarize_func,
summarize_baton, ctx, pool));
} else
return svn_error_create(SVN_ERR_UNSUPPORTED_FEATURE, NULL,
_("Summarizing diff can only compare repository "
"to repository"));
return SVN_NO_ERROR;
}
static svn_error_t *
set_up_diff_cmd_and_options(struct diff_cmd_baton *diff_cmd_baton,
const apr_array_header_t *options,
apr_hash_t *config, apr_pool_t *pool) {
const char *diff_cmd = NULL;
if (config) {
svn_config_t *cfg = apr_hash_get(config, SVN_CONFIG_CATEGORY_CONFIG,
APR_HASH_KEY_STRING);
svn_config_get(cfg, &diff_cmd, SVN_CONFIG_SECTION_HELPERS,
SVN_CONFIG_OPTION_DIFF_CMD, NULL);
}
diff_cmd_baton->diff_cmd = diff_cmd;
if (diff_cmd_baton->diff_cmd) {
const char **argv = NULL;
int argc = options->nelts;
if (argc) {
int i;
argv = apr_palloc(pool, argc * sizeof(char *));
for (i = 0; i < argc; i++)
argv[i] = APR_ARRAY_IDX(options, i, const char *);
}
diff_cmd_baton->options.for_external.argv = argv;
diff_cmd_baton->options.for_external.argc = argc;
} else {
diff_cmd_baton->options.for_internal
= svn_diff_file_options_create(pool);
SVN_ERR(svn_diff_file_options_parse
(diff_cmd_baton->options.for_internal, options, pool));
}
return SVN_NO_ERROR;
}
svn_error_t *
svn_client_diff4(const apr_array_header_t *options,
const char *path1,
const svn_opt_revision_t *revision1,
const char *path2,
const svn_opt_revision_t *revision2,
const char *relative_to_dir,
svn_depth_t depth,
svn_boolean_t ignore_ancestry,
svn_boolean_t no_diff_deleted,
svn_boolean_t ignore_content_type,
const char *header_encoding,
apr_file_t *outfile,
apr_file_t *errfile,
const apr_array_header_t *changelists,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
struct diff_parameters diff_params;
struct diff_cmd_baton diff_cmd_baton;
svn_wc_diff_callbacks2_t diff_callbacks;
svn_opt_revision_t peg_revision;
peg_revision.kind = svn_opt_revision_unspecified;
diff_params.path1 = path1;
diff_params.revision1 = revision1;
diff_params.path2 = path2;
diff_params.revision2 = revision2;
diff_params.peg_revision = &peg_revision;
diff_params.depth = depth;
diff_params.ignore_ancestry = ignore_ancestry;
diff_params.no_diff_deleted = no_diff_deleted;
diff_params.changelists = changelists;
diff_callbacks.file_changed = diff_file_changed;
diff_callbacks.file_added = diff_file_added;
diff_callbacks.file_deleted = no_diff_deleted ? diff_file_deleted_no_diff :
diff_file_deleted_with_diff;
diff_callbacks.dir_added = diff_dir_added;
diff_callbacks.dir_deleted = diff_dir_deleted;
diff_callbacks.dir_props_changed = diff_props_changed;
diff_cmd_baton.orig_path_1 = path1;
diff_cmd_baton.orig_path_2 = path2;
SVN_ERR(set_up_diff_cmd_and_options(&diff_cmd_baton, options,
ctx->config, pool));
diff_cmd_baton.pool = pool;
diff_cmd_baton.outfile = outfile;
diff_cmd_baton.errfile = errfile;
diff_cmd_baton.header_encoding = header_encoding;
diff_cmd_baton.revnum1 = SVN_INVALID_REVNUM;
diff_cmd_baton.revnum2 = SVN_INVALID_REVNUM;
diff_cmd_baton.force_empty = FALSE;
diff_cmd_baton.force_binary = ignore_content_type;
diff_cmd_baton.relative_to_dir = relative_to_dir;
return do_diff(&diff_params, &diff_callbacks, &diff_cmd_baton, ctx, pool);
}
svn_error_t *
svn_client_diff3(const apr_array_header_t *options,
const char *path1,
const svn_opt_revision_t *revision1,
const char *path2,
const svn_opt_revision_t *revision2,
svn_boolean_t recurse,
svn_boolean_t ignore_ancestry,
svn_boolean_t no_diff_deleted,
svn_boolean_t ignore_content_type,
const char *header_encoding,
apr_file_t *outfile,
apr_file_t *errfile,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
return svn_client_diff4(options, path1, revision1, path2,
revision2, NULL,
SVN_DEPTH_INFINITY_OR_FILES(recurse),
ignore_ancestry, no_diff_deleted,
ignore_content_type, header_encoding,
outfile, errfile, NULL, ctx, pool);
}
svn_error_t *
svn_client_diff2(const apr_array_header_t *options,
const char *path1,
const svn_opt_revision_t *revision1,
const char *path2,
const svn_opt_revision_t *revision2,
svn_boolean_t recurse,
svn_boolean_t ignore_ancestry,
svn_boolean_t no_diff_deleted,
svn_boolean_t ignore_content_type,
apr_file_t *outfile,
apr_file_t *errfile,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
return svn_client_diff3(options, path1, revision1, path2, revision2,
recurse, ignore_ancestry, no_diff_deleted,
ignore_content_type, SVN_APR_LOCALE_CHARSET,
outfile, errfile, ctx, pool);
}
svn_error_t *
svn_client_diff(const apr_array_header_t *options,
const char *path1,
const svn_opt_revision_t *revision1,
const char *path2,
const svn_opt_revision_t *revision2,
svn_boolean_t recurse,
svn_boolean_t ignore_ancestry,
svn_boolean_t no_diff_deleted,
apr_file_t *outfile,
apr_file_t *errfile,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
return svn_client_diff2(options, path1, revision1, path2, revision2,
recurse, ignore_ancestry, no_diff_deleted, FALSE,
outfile, errfile, ctx, pool);
}
svn_error_t *
svn_client_diff_peg4(const apr_array_header_t *options,
const char *path,
const svn_opt_revision_t *peg_revision,
const svn_opt_revision_t *start_revision,
const svn_opt_revision_t *end_revision,
const char *relative_to_dir,
svn_depth_t depth,
svn_boolean_t ignore_ancestry,
svn_boolean_t no_diff_deleted,
svn_boolean_t ignore_content_type,
const char *header_encoding,
apr_file_t *outfile,
apr_file_t *errfile,
const apr_array_header_t *changelists,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
struct diff_parameters diff_params;
struct diff_cmd_baton diff_cmd_baton;
svn_wc_diff_callbacks2_t diff_callbacks;
if (svn_path_is_url(path) &&
(start_revision->kind == svn_opt_revision_base
|| end_revision->kind == svn_opt_revision_base) )
return svn_error_create(SVN_ERR_CLIENT_BAD_REVISION, NULL,
_("Revision type requires a working copy "
"path, not a URL"));
diff_params.path1 = path;
diff_params.revision1 = start_revision;
diff_params.path2 = path;
diff_params.revision2 = end_revision;
diff_params.peg_revision = peg_revision;
diff_params.depth = depth;
diff_params.ignore_ancestry = ignore_ancestry;
diff_params.no_diff_deleted = no_diff_deleted;
diff_params.changelists = changelists;
diff_callbacks.file_changed = diff_file_changed;
diff_callbacks.file_added = diff_file_added;
diff_callbacks.file_deleted = no_diff_deleted ? diff_file_deleted_no_diff :
diff_file_deleted_with_diff;
diff_callbacks.dir_added = diff_dir_added;
diff_callbacks.dir_deleted = diff_dir_deleted;
diff_callbacks.dir_props_changed = diff_props_changed;
diff_cmd_baton.orig_path_1 = path;
diff_cmd_baton.orig_path_2 = path;
SVN_ERR(set_up_diff_cmd_and_options(&diff_cmd_baton, options,
ctx->config, pool));
diff_cmd_baton.pool = pool;
diff_cmd_baton.outfile = outfile;
diff_cmd_baton.errfile = errfile;
diff_cmd_baton.header_encoding = header_encoding;
diff_cmd_baton.revnum1 = SVN_INVALID_REVNUM;
diff_cmd_baton.revnum2 = SVN_INVALID_REVNUM;
diff_cmd_baton.force_empty = FALSE;
diff_cmd_baton.force_binary = ignore_content_type;
diff_cmd_baton.relative_to_dir = relative_to_dir;
return do_diff(&diff_params, &diff_callbacks, &diff_cmd_baton, ctx, pool);
}
svn_error_t *
svn_client_diff_peg3(const apr_array_header_t *options,
const char *path,
const svn_opt_revision_t *peg_revision,
const svn_opt_revision_t *start_revision,
const svn_opt_revision_t *end_revision,
svn_boolean_t recurse,
svn_boolean_t ignore_ancestry,
svn_boolean_t no_diff_deleted,
svn_boolean_t ignore_content_type,
const char *header_encoding,
apr_file_t *outfile,
apr_file_t *errfile,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
return svn_client_diff_peg4(options,
path,
peg_revision,
start_revision,
end_revision,
NULL,
SVN_DEPTH_INFINITY_OR_FILES(recurse),
ignore_ancestry,
no_diff_deleted,
ignore_content_type,
header_encoding,
outfile,
errfile,
NULL,
ctx,
pool);
}
svn_error_t *
svn_client_diff_peg2(const apr_array_header_t *options,
const char *path,
const svn_opt_revision_t *peg_revision,
const svn_opt_revision_t *start_revision,
const svn_opt_revision_t *end_revision,
svn_boolean_t recurse,
svn_boolean_t ignore_ancestry,
svn_boolean_t no_diff_deleted,
svn_boolean_t ignore_content_type,
apr_file_t *outfile,
apr_file_t *errfile,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
return svn_client_diff_peg3(options, path, peg_revision, start_revision,
end_revision,
SVN_DEPTH_INFINITY_OR_FILES(recurse),
ignore_ancestry, no_diff_deleted,
ignore_content_type, SVN_APR_LOCALE_CHARSET,
outfile, errfile, ctx, pool);
}
svn_error_t *
svn_client_diff_peg(const apr_array_header_t *options,
const char *path,
const svn_opt_revision_t *peg_revision,
const svn_opt_revision_t *start_revision,
const svn_opt_revision_t *end_revision,
svn_boolean_t recurse,
svn_boolean_t ignore_ancestry,
svn_boolean_t no_diff_deleted,
apr_file_t *outfile,
apr_file_t *errfile,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
return svn_client_diff_peg2(options, path, peg_revision,
start_revision, end_revision, recurse,
ignore_ancestry, no_diff_deleted, FALSE,
outfile, errfile, ctx, pool);
}
svn_error_t *
svn_client_diff_summarize2(const char *path1,
const svn_opt_revision_t *revision1,
const char *path2,
const svn_opt_revision_t *revision2,
svn_depth_t depth,
svn_boolean_t ignore_ancestry,
const apr_array_header_t *changelists,
svn_client_diff_summarize_func_t summarize_func,
void *summarize_baton,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
struct diff_parameters diff_params;
svn_opt_revision_t peg_revision;
peg_revision.kind = svn_opt_revision_unspecified;
diff_params.path1 = path1;
diff_params.revision1 = revision1;
diff_params.path2 = path2;
diff_params.revision2 = revision2;
diff_params.peg_revision = &peg_revision;
diff_params.depth = depth;
diff_params.ignore_ancestry = ignore_ancestry;
diff_params.no_diff_deleted = FALSE;
diff_params.changelists = changelists;
return do_diff_summarize(&diff_params, summarize_func, summarize_baton,
ctx, pool);
}
svn_error_t *
svn_client_diff_summarize(const char *path1,
const svn_opt_revision_t *revision1,
const char *path2,
const svn_opt_revision_t *revision2,
svn_boolean_t recurse,
svn_boolean_t ignore_ancestry,
svn_client_diff_summarize_func_t summarize_func,
void *summarize_baton,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
return svn_client_diff_summarize2(path1, revision1, path2,
revision2,
SVN_DEPTH_INFINITY_OR_FILES(recurse),
ignore_ancestry, NULL, summarize_func,
summarize_baton, ctx, pool);
}
svn_error_t *
svn_client_diff_summarize_peg2(const char *path,
const svn_opt_revision_t *peg_revision,
const svn_opt_revision_t *start_revision,
const svn_opt_revision_t *end_revision,
svn_depth_t depth,
svn_boolean_t ignore_ancestry,
const apr_array_header_t *changelists,
svn_client_diff_summarize_func_t summarize_func,
void *summarize_baton,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
struct diff_parameters diff_params;
diff_params.path1 = path;
diff_params.revision1 = start_revision;
diff_params.path2 = path;
diff_params.revision2 = end_revision;
diff_params.peg_revision = peg_revision;
diff_params.depth = depth;
diff_params.ignore_ancestry = ignore_ancestry;
diff_params.no_diff_deleted = FALSE;
diff_params.changelists = changelists;
return do_diff_summarize(&diff_params, summarize_func, summarize_baton,
ctx, pool);
}
svn_error_t *
svn_client_diff_summarize_peg(const char *path,
const svn_opt_revision_t *peg_revision,
const svn_opt_revision_t *start_revision,
const svn_opt_revision_t *end_revision,
svn_boolean_t recurse,
svn_boolean_t ignore_ancestry,
svn_client_diff_summarize_func_t summarize_func,
void *summarize_baton,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
return svn_client_diff_summarize_peg2(path, peg_revision,
start_revision, end_revision,
SVN_DEPTH_INFINITY_OR_FILES(recurse),
ignore_ancestry, NULL,
summarize_func, summarize_baton,
ctx, pool);
}
svn_client_diff_summarize_t *
svn_client_diff_summarize_dup(const svn_client_diff_summarize_t *diff,
apr_pool_t *pool) {
svn_client_diff_summarize_t *dup_diff = apr_palloc(pool, sizeof(*dup_diff));
*dup_diff = *diff;
if (diff->path)
dup_diff->path = apr_pstrdup(pool, diff->path);
return dup_diff;
}
