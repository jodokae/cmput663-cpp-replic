#include <string.h>
#include <assert.h>
#include <apr_strings.h>
#include <apr_hash.h>
#include <apr_md5.h>
#include "svn_wc.h"
#include "svn_ra.h"
#include "svn_delta.h"
#include "svn_subst.h"
#include "svn_client.h"
#include "svn_string.h"
#include "svn_pools.h"
#include "svn_error.h"
#include "svn_error_codes.h"
#include "svn_path.h"
#include "svn_io.h"
#include "svn_md5.h"
#include "svn_time.h"
#include "svn_sorts.h"
#include "svn_props.h"
#include "svn_iter.h"
#include "client.h"
#include "svn_private_config.h"
typedef struct import_ctx_t {
svn_boolean_t repos_changed;
} import_ctx_t;
static svn_error_t *
send_file_contents(const char *path,
void *file_baton,
const svn_delta_editor_t *editor,
apr_hash_t *properties,
unsigned char *digest,
apr_pool_t *pool) {
const char *tmpfile_path = NULL;
svn_stream_t *contents;
svn_txdelta_window_handler_t handler;
void *handler_baton;
apr_file_t *f;
const svn_string_t *eol_style_val = NULL, *keywords_val = NULL;
svn_boolean_t special = FALSE;
svn_subst_eol_style_t eol_style;
const char *eol;
apr_hash_t *keywords;
if (properties) {
eol_style_val = apr_hash_get(properties, SVN_PROP_EOL_STYLE,
sizeof(SVN_PROP_EOL_STYLE) - 1);
keywords_val = apr_hash_get(properties, SVN_PROP_KEYWORDS,
sizeof(SVN_PROP_KEYWORDS) - 1);
if (apr_hash_get(properties, SVN_PROP_SPECIAL, APR_HASH_KEY_STRING))
special = TRUE;
}
SVN_ERR(editor->apply_textdelta(file_baton, NULL, pool,
&handler, &handler_baton));
if (eol_style_val)
svn_subst_eol_style_from_value(&eol_style, &eol, eol_style_val->data);
else {
eol = NULL;
eol_style = svn_subst_eol_style_none;
}
if (keywords_val)
SVN_ERR(svn_subst_build_keywords2(&keywords, keywords_val->data,
APR_STRINGIFY(SVN_INVALID_REVNUM),
"", 0, "", pool));
else
keywords = NULL;
if (svn_subst_translation_required(eol_style, eol, keywords, special, TRUE)) {
const char *temp_dir;
SVN_ERR(svn_io_temp_dir(&temp_dir, pool));
SVN_ERR(svn_io_open_unique_file2
(NULL, &tmpfile_path,
svn_path_join(temp_dir, "svn-import", pool),
".tmp", svn_io_file_del_on_pool_cleanup, pool));
SVN_ERR(svn_subst_translate_to_normal_form
(path, tmpfile_path, eol_style, eol, FALSE,
keywords, special, pool));
}
SVN_ERR(svn_io_file_open(&f, tmpfile_path ? tmpfile_path : path,
APR_READ, APR_OS_DEFAULT, pool));
contents = svn_stream_from_aprfile(f, pool);
SVN_ERR(svn_txdelta_send_stream(contents, handler, handler_baton,
digest, pool));
SVN_ERR(svn_io_file_close(f, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
import_file(const svn_delta_editor_t *editor,
void *dir_baton,
const char *path,
const char *edit_path,
import_ctx_t *import_ctx,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
void *file_baton;
const char *mimetype = NULL;
unsigned char digest[APR_MD5_DIGESTSIZE];
const char *text_checksum;
apr_hash_t* properties;
apr_hash_index_t *hi;
svn_node_kind_t kind;
svn_boolean_t is_special;
SVN_ERR(svn_path_check_valid(path, pool));
SVN_ERR(svn_io_check_special_path(path, &kind, &is_special, pool));
SVN_ERR(editor->add_file(edit_path, dir_baton, NULL, SVN_INVALID_REVNUM,
pool, &file_baton));
import_ctx->repos_changed = TRUE;
if (! is_special) {
SVN_ERR(svn_client__get_auto_props(&properties, &mimetype, path, ctx,
pool));
} else
properties = apr_hash_make(pool);
if (properties) {
for (hi = apr_hash_first(pool, properties); hi; hi = apr_hash_next(hi)) {
const void *pname;
void *pval;
apr_hash_this(hi, &pname, NULL, &pval);
SVN_ERR(editor->change_file_prop(file_baton, pname, pval, pool));
}
}
if (ctx->notify_func2) {
svn_wc_notify_t *notify
= svn_wc_create_notify(path, svn_wc_notify_commit_added, pool);
notify->kind = svn_node_file;
notify->mime_type = mimetype;
notify->content_state = notify->prop_state
= svn_wc_notify_state_inapplicable;
notify->lock_state = svn_wc_notify_lock_state_inapplicable;
(*ctx->notify_func2)(ctx->notify_baton2, notify, pool);
}
if (is_special) {
apr_hash_set(properties, SVN_PROP_SPECIAL, APR_HASH_KEY_STRING,
svn_string_create(SVN_PROP_BOOLEAN_TRUE, pool));
SVN_ERR(editor->change_file_prop(file_baton, SVN_PROP_SPECIAL,
apr_hash_get(properties,
SVN_PROP_SPECIAL,
APR_HASH_KEY_STRING),
pool));
}
SVN_ERR(send_file_contents(path, file_baton, editor,
properties, digest, pool));
text_checksum = svn_md5_digest_to_cstring(digest, pool);
SVN_ERR(editor->close_file(file_baton, text_checksum, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
import_dir(const svn_delta_editor_t *editor,
void *dir_baton,
const char *path,
const char *edit_path,
svn_depth_t depth,
apr_hash_t *excludes,
svn_boolean_t no_ignore,
svn_boolean_t ignore_unknown_node_types,
import_ctx_t *import_ctx,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
apr_pool_t *subpool = svn_pool_create(pool);
apr_hash_t *dirents;
apr_hash_index_t *hi;
apr_array_header_t *ignores;
SVN_ERR(svn_path_check_valid(path, pool));
if (!no_ignore)
SVN_ERR(svn_wc_get_default_ignores(&ignores, ctx->config, pool));
SVN_ERR(svn_io_get_dirents2(&dirents, path, pool));
for (hi = apr_hash_first(pool, dirents); hi; hi = apr_hash_next(hi)) {
const char *this_path, *this_edit_path, *abs_path;
const svn_io_dirent_t *dirent;
const char *filename;
const void *key;
void *val;
svn_pool_clear(subpool);
apr_hash_this(hi, &key, NULL, &val);
filename = key;
dirent = val;
if (ctx->cancel_func)
SVN_ERR(ctx->cancel_func(ctx->cancel_baton));
if (svn_wc_is_adm_dir(filename, subpool)) {
if (ctx->notify_func2) {
svn_wc_notify_t *notify
= svn_wc_create_notify(svn_path_join(path, filename,
subpool),
svn_wc_notify_skip, subpool);
notify->kind = svn_node_dir;
notify->content_state = notify->prop_state
= svn_wc_notify_state_inapplicable;
notify->lock_state = svn_wc_notify_lock_state_inapplicable;
(*ctx->notify_func2)(ctx->notify_baton2, notify, subpool);
}
continue;
}
this_path = svn_path_join(path, filename, subpool);
this_edit_path = svn_path_join(edit_path, filename, subpool);
SVN_ERR(svn_path_get_absolute(&abs_path, this_path, subpool));
if (apr_hash_get(excludes, abs_path, APR_HASH_KEY_STRING))
continue;
if ((!no_ignore) && svn_wc_match_ignore_list(filename, ignores,
subpool))
continue;
if (dirent->kind == svn_node_dir && depth >= svn_depth_immediates) {
void *this_dir_baton;
SVN_ERR(editor->add_directory(this_edit_path, dir_baton,
NULL, SVN_INVALID_REVNUM, subpool,
&this_dir_baton));
import_ctx->repos_changed = TRUE;
if (ctx->notify_func2) {
svn_wc_notify_t *notify
= svn_wc_create_notify(this_path, svn_wc_notify_commit_added,
subpool);
notify->kind = svn_node_dir;
notify->content_state = notify->prop_state
= svn_wc_notify_state_inapplicable;
notify->lock_state = svn_wc_notify_lock_state_inapplicable;
(*ctx->notify_func2)(ctx->notify_baton2, notify, subpool);
}
{
svn_depth_t depth_below_here = depth;
if (depth == svn_depth_immediates)
depth_below_here = svn_depth_empty;
SVN_ERR(import_dir(editor, this_dir_baton, this_path,
this_edit_path, depth_below_here, excludes,
no_ignore, ignore_unknown_node_types,
import_ctx, ctx,
subpool));
}
SVN_ERR(editor->close_directory(this_dir_baton, subpool));
} else if (dirent->kind == svn_node_file && depth >= svn_depth_files) {
SVN_ERR(import_file(editor, dir_baton, this_path,
this_edit_path, import_ctx, ctx, subpool));
} else if (dirent->kind != svn_node_dir && dirent->kind != svn_node_file) {
if (ignore_unknown_node_types) {
if (ctx->notify_func2) {
svn_wc_notify_t *notify
= svn_wc_create_notify(this_path,
svn_wc_notify_skip, subpool);
notify->kind = svn_node_dir;
notify->content_state = notify->prop_state
= svn_wc_notify_state_inapplicable;
notify->lock_state = svn_wc_notify_lock_state_inapplicable;
(*ctx->notify_func2)(ctx->notify_baton2, notify, subpool);
}
} else
return svn_error_createf
(SVN_ERR_NODE_UNKNOWN_KIND, NULL,
_("Unknown or unversionable type for '%s'"),
svn_path_local_style(this_path, subpool));
}
}
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
static svn_error_t *
import(const char *path,
apr_array_header_t *new_entries,
const svn_delta_editor_t *editor,
void *edit_baton,
svn_depth_t depth,
apr_hash_t *excludes,
svn_boolean_t no_ignore,
svn_boolean_t ignore_unknown_node_types,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
void *root_baton;
svn_node_kind_t kind;
apr_array_header_t *ignores;
apr_array_header_t *batons = NULL;
const char *edit_path = "";
import_ctx_t *import_ctx = apr_pcalloc(pool, sizeof(*import_ctx));
SVN_ERR(editor->open_root(edit_baton, SVN_INVALID_REVNUM,
pool, &root_baton));
SVN_ERR(svn_io_check_path(path, &kind, pool));
if (new_entries->nelts) {
int i;
batons = apr_array_make(pool, new_entries->nelts, sizeof(void *));
for (i = 0; i < new_entries->nelts; i++) {
const char *component = APR_ARRAY_IDX(new_entries, i, const char *);
edit_path = svn_path_join(edit_path, component, pool);
if ((i == new_entries->nelts - 1) && (kind == svn_node_file))
break;
APR_ARRAY_PUSH(batons, void *) = root_baton;
SVN_ERR(editor->add_directory(edit_path,
root_baton,
NULL, SVN_INVALID_REVNUM,
pool, &root_baton));
import_ctx->repos_changed = TRUE;
}
} else if (kind == svn_node_file) {
return svn_error_create
(SVN_ERR_NODE_UNKNOWN_KIND, NULL,
_("New entry name required when importing a file"));
}
if (kind == svn_node_file) {
svn_boolean_t ignores_match = FALSE;
if (!no_ignore) {
SVN_ERR(svn_wc_get_default_ignores(&ignores, ctx->config, pool));
ignores_match = svn_wc_match_ignore_list(path, ignores, pool);
}
if (!ignores_match)
SVN_ERR(import_file(editor, root_baton, path, edit_path,
import_ctx, ctx, pool));
} else if (kind == svn_node_dir) {
SVN_ERR(import_dir(editor, root_baton, path, edit_path,
depth, excludes, no_ignore,
ignore_unknown_node_types, import_ctx, ctx, pool));
} else if (kind == svn_node_none
|| kind == svn_node_unknown) {
return svn_error_createf(SVN_ERR_NODE_UNKNOWN_KIND, NULL,
_("'%s' does not exist"),
svn_path_local_style(path, pool));
}
SVN_ERR(editor->close_directory(root_baton, pool));
if (batons && batons->nelts) {
void **baton;
while ((baton = (void **) apr_array_pop(batons))) {
SVN_ERR(editor->close_directory(*baton, pool));
}
}
if (import_ctx->repos_changed)
SVN_ERR(editor->close_edit(edit_baton, pool));
else
SVN_ERR(editor->abort_edit(edit_baton, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
get_ra_editor(svn_ra_session_t **ra_session,
svn_revnum_t *latest_rev,
const svn_delta_editor_t **editor,
void **edit_baton,
svn_client_ctx_t *ctx,
const char *base_url,
const char *base_dir,
svn_wc_adm_access_t *base_access,
const char *log_msg,
apr_array_header_t *commit_items,
const apr_hash_t *revprop_table,
svn_commit_info_t **commit_info_p,
svn_boolean_t is_commit,
apr_hash_t *lock_tokens,
svn_boolean_t keep_locks,
apr_pool_t *pool) {
void *commit_baton;
apr_hash_t *commit_revprops;
SVN_ERR(svn_client__open_ra_session_internal(ra_session,
base_url, base_dir,
base_access, commit_items,
is_commit, !is_commit,
ctx, pool));
if (! is_commit) {
svn_node_kind_t kind;
SVN_ERR(svn_ra_check_path(*ra_session, "", SVN_INVALID_REVNUM,
&kind, pool));
if (kind == svn_node_none)
return svn_error_createf(SVN_ERR_FS_NO_SUCH_ENTRY, NULL,
_("Path '%s' does not exist"),
base_url);
}
if (latest_rev)
SVN_ERR(svn_ra_get_latest_revnum(*ra_session, latest_rev, pool));
SVN_ERR(svn_client__ensure_revprop_table(&commit_revprops, revprop_table,
log_msg, ctx, pool));
SVN_ERR(svn_client__commit_get_baton(&commit_baton, commit_info_p, pool));
return svn_ra_get_commit_editor3(*ra_session, editor, edit_baton,
commit_revprops,
svn_client__commit_callback,
commit_baton, lock_tokens, keep_locks,
pool);
}
svn_error_t *
svn_client_import3(svn_commit_info_t **commit_info_p,
const char *path,
const char *url,
svn_depth_t depth,
svn_boolean_t no_ignore,
svn_boolean_t ignore_unknown_node_types,
const apr_hash_t *revprop_table,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
svn_error_t *err = SVN_NO_ERROR;
const char *log_msg = "";
const svn_delta_editor_t *editor;
void *edit_baton;
svn_ra_session_t *ra_session;
apr_hash_t *excludes = apr_hash_make(pool);
svn_node_kind_t kind;
const char *base_dir = path;
apr_array_header_t *new_entries = apr_array_make(pool, 4,
sizeof(const char *));
const char *temp;
const char *dir;
apr_pool_t *subpool;
if (SVN_CLIENT__HAS_LOG_MSG_FUNC(ctx)) {
svn_client_commit_item3_t *item;
const char *tmp_file;
apr_array_header_t *commit_items
= apr_array_make(pool, 1, sizeof(item));
SVN_ERR(svn_client_commit_item_create
((const svn_client_commit_item3_t **) &item, pool));
item->path = apr_pstrdup(pool, path);
item->state_flags = SVN_CLIENT_COMMIT_ITEM_ADD;
APR_ARRAY_PUSH(commit_items, svn_client_commit_item3_t *) = item;
SVN_ERR(svn_client__get_log_msg(&log_msg, &tmp_file, commit_items,
ctx, pool));
if (! log_msg)
return SVN_NO_ERROR;
if (tmp_file) {
const char *abs_path;
SVN_ERR(svn_path_get_absolute(&abs_path, tmp_file, pool));
apr_hash_set(excludes, abs_path, APR_HASH_KEY_STRING, (void *)1);
}
}
SVN_ERR(svn_io_check_path(path, &kind, pool));
if (kind == svn_node_file)
svn_path_split(path, &base_dir, NULL, pool);
subpool = svn_pool_create(pool);
do {
svn_pool_clear(subpool);
if (ctx->cancel_func)
SVN_ERR(ctx->cancel_func(ctx->cancel_baton));
if (err) {
if (err->apr_err != SVN_ERR_FS_NO_SUCH_ENTRY)
return err;
else
svn_error_clear(err);
svn_path_split(url, &temp, &dir, pool);
APR_ARRAY_PUSH(new_entries, const char *) =
svn_path_uri_decode(dir, pool);
url = temp;
}
} while ((err = get_ra_editor(&ra_session, NULL,
&editor, &edit_baton, ctx, url, base_dir,
NULL, log_msg, NULL, revprop_table,
commit_info_p, FALSE, NULL, TRUE, subpool)));
if (new_entries->nelts) {
int i, j;
const char *component;
for (i = 0; i < (new_entries->nelts / 2); i++) {
j = new_entries->nelts - i - 1;
component =
APR_ARRAY_IDX(new_entries, i, const char *);
APR_ARRAY_IDX(new_entries, i, const char *) =
APR_ARRAY_IDX(new_entries, j, const char *);
APR_ARRAY_IDX(new_entries, j, const char *) =
component;
}
}
if (kind == svn_node_file && (! new_entries->nelts))
return svn_error_createf
(SVN_ERR_ENTRY_EXISTS, NULL,
_("Path '%s' already exists"), url);
if (new_entries->nelts
&& svn_wc_is_adm_dir(temp = APR_ARRAY_IDX(new_entries,
new_entries->nelts - 1,
const char *),
pool))
return svn_error_createf
(SVN_ERR_CL_ADM_DIR_RESERVED, NULL,
_("'%s' is a reserved name and cannot be imported"),
svn_path_local_style(temp, pool));
if ((err = import(path, new_entries, editor, edit_baton,
depth, excludes, no_ignore,
ignore_unknown_node_types, ctx, subpool))) {
svn_error_clear(editor->abort_edit(edit_baton, subpool));
return err;
}
if (*commit_info_p) {
svn_commit_info_t *tmp_commit_info;
tmp_commit_info = svn_create_commit_info(pool);
*tmp_commit_info = **commit_info_p;
if (tmp_commit_info->date)
tmp_commit_info->date = apr_pstrdup(pool, tmp_commit_info->date);
if (tmp_commit_info->author)
tmp_commit_info->author = apr_pstrdup(pool, tmp_commit_info->author);
if (tmp_commit_info->post_commit_err)
tmp_commit_info->post_commit_err
= apr_pstrdup(pool, tmp_commit_info->post_commit_err);
*commit_info_p = tmp_commit_info;
}
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
svn_error_t *
svn_client_import2(svn_commit_info_t **commit_info_p,
const char *path,
const char *url,
svn_boolean_t nonrecursive,
svn_boolean_t no_ignore,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
return svn_client_import3(commit_info_p,
path, url,
SVN_DEPTH_INFINITY_OR_FILES(! nonrecursive),
no_ignore, FALSE, NULL, ctx, pool);
}
svn_error_t *
svn_client_import(svn_client_commit_info_t **commit_info_p,
const char *path,
const char *url,
svn_boolean_t nonrecursive,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
svn_commit_info_t *commit_info = NULL;
svn_error_t *err;
err = svn_client_import2(&commit_info,
path, url, nonrecursive,
FALSE, ctx, pool);
*commit_info_p = (svn_client_commit_info_t *) commit_info;
return err;
}
static svn_error_t *
remove_tmpfiles(apr_hash_t *tempfiles,
apr_pool_t *pool) {
apr_hash_index_t *hi;
apr_pool_t *subpool;
if (! tempfiles)
return SVN_NO_ERROR;
subpool = svn_pool_create(pool);
for (hi = apr_hash_first(pool, tempfiles); hi; hi = apr_hash_next(hi)) {
const void *key;
void *val;
svn_error_t *err;
svn_pool_clear(subpool);
apr_hash_this(hi, &key, NULL, &val);
err = svn_io_remove_file((const char *)key, subpool);
if (err) {
if (! APR_STATUS_IS_ENOENT(err->apr_err))
return err;
else
svn_error_clear(err);
}
}
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
static svn_error_t *
reconcile_errors(svn_error_t *commit_err,
svn_error_t *unlock_err,
svn_error_t *bump_err,
svn_error_t *cleanup_err,
apr_pool_t *pool) {
svn_error_t *err;
if (! (commit_err || unlock_err || bump_err || cleanup_err))
return SVN_NO_ERROR;
if (commit_err) {
commit_err = svn_error_quick_wrap
(commit_err, _("Commit failed (details follow):"));
err = commit_err;
}
else
err = svn_error_create(SVN_ERR_BASE, NULL,
_("Commit succeeded, but other errors follow:"));
if (unlock_err) {
unlock_err = svn_error_quick_wrap
(unlock_err, _("Error unlocking locked dirs (details follow):"));
svn_error_compose(err, unlock_err);
}
if (bump_err) {
bump_err = svn_error_quick_wrap
(bump_err, _("Error bumping revisions post-commit (details follow):"));
svn_error_compose(err, bump_err);
}
if (cleanup_err) {
cleanup_err = svn_error_quick_wrap
(cleanup_err, _("Error in post-commit clean-up (details follow):"));
svn_error_compose(err, cleanup_err);
}
return err;
}
static svn_error_t *
remove_redundancies(apr_array_header_t **punique_targets,
const apr_array_header_t *nonrecursive_targets,
const apr_array_header_t *recursive_targets,
apr_pool_t *pool) {
apr_pool_t *temp_pool;
apr_array_header_t *abs_recursive_targets = NULL;
apr_hash_t *abs_targets;
apr_array_header_t *rel_targets;
int i;
if ((nonrecursive_targets->nelts <= 0) || (! punique_targets)) {
if (punique_targets)
*punique_targets = NULL;
return SVN_NO_ERROR;
}
temp_pool = svn_pool_create(pool);
abs_targets = apr_hash_make(temp_pool);
if (recursive_targets) {
abs_recursive_targets = apr_array_make(temp_pool,
recursive_targets->nelts,
sizeof(const char *));
for (i = 0; i < recursive_targets->nelts; i++) {
const char *rel_path =
APR_ARRAY_IDX(recursive_targets, i, const char *);
const char *abs_path;
SVN_ERR(svn_path_get_absolute(&abs_path, rel_path, temp_pool));
APR_ARRAY_PUSH(abs_recursive_targets, const char *) = abs_path;
}
}
rel_targets = apr_array_make(pool, nonrecursive_targets->nelts,
sizeof(const char *));
for (i = 0; i < nonrecursive_targets->nelts; i++) {
const char *rel_path = APR_ARRAY_IDX(nonrecursive_targets, i,
const char *);
const char *abs_path;
int j;
svn_boolean_t keep_me;
SVN_ERR(svn_path_get_absolute(&abs_path, rel_path, temp_pool));
keep_me = TRUE;
if (abs_recursive_targets) {
for (j = 0; j < abs_recursive_targets->nelts; j++) {
const char *keeper = APR_ARRAY_IDX(abs_recursive_targets, j,
const char *);
if (strcmp(keeper, abs_path) == 0) {
keep_me = FALSE;
break;
}
if (svn_path_is_child(keeper, abs_path, temp_pool)) {
keep_me = FALSE;
break;
}
}
}
if (keep_me
&& apr_hash_get(abs_targets, abs_path, APR_HASH_KEY_STRING) == NULL) {
APR_ARRAY_PUSH(rel_targets, const char *) = rel_path;
apr_hash_set(abs_targets, abs_path, APR_HASH_KEY_STRING, abs_path);
}
}
svn_pool_destroy(temp_pool);
*punique_targets = rel_targets;
return SVN_NO_ERROR;
}
static svn_error_t *
adjust_rel_targets(const char **pbase_dir,
apr_array_header_t **prel_targets,
const char *base_dir,
apr_array_header_t *rel_targets,
apr_pool_t *pool) {
const char *target;
int i;
svn_boolean_t anchor_one_up = FALSE;
apr_array_header_t *new_rel_targets;
for (i = 0; i < rel_targets->nelts; i++) {
target = APR_ARRAY_IDX(rel_targets, i, const char *);
if (target[0] == '\0') {
anchor_one_up = TRUE;
break;
}
}
new_rel_targets = rel_targets;
if (anchor_one_up) {
const char *parent_dir, *name;
SVN_ERR(svn_wc_get_actual_target(base_dir, &parent_dir, &name, pool));
if (*name) {
base_dir = apr_pstrdup(pool, parent_dir);
new_rel_targets = apr_array_make(pool, rel_targets->nelts,
sizeof(name));
for (i = 0; i < rel_targets->nelts; i++) {
target = APR_ARRAY_IDX(rel_targets, i, const char *);
target = svn_path_join(name, target, pool);
APR_ARRAY_PUSH(new_rel_targets, const char *) = target;
}
}
}
*pbase_dir = base_dir;
*prel_targets = new_rel_targets;
return SVN_NO_ERROR;
}
static svn_error_t *
collect_lock_tokens(apr_hash_t **result,
apr_hash_t *all_tokens,
const char *base_url,
apr_pool_t *pool) {
apr_hash_index_t *hi;
size_t base_len = strlen(base_url);
*result = apr_hash_make(pool);
for (hi = apr_hash_first(pool, all_tokens); hi; hi = apr_hash_next(hi)) {
const void *key;
void *val;
const char *url;
const char *token;
apr_hash_this(hi, &key, NULL, &val);
url = key;
token = val;
if (strncmp(base_url, url, base_len) == 0
&& (url[base_len] == '\0' || url[base_len] == '/')) {
if (url[base_len] == '\0')
url = "";
else
url = svn_path_uri_decode(url + base_len + 1, pool);
apr_hash_set(*result, url, APR_HASH_KEY_STRING, token);
}
}
return SVN_NO_ERROR;
}
struct post_commit_baton {
svn_wc_committed_queue_t *queue;
apr_pool_t *qpool;
svn_wc_adm_access_t *base_dir_access;
svn_boolean_t keep_changelists;
svn_boolean_t keep_locks;
apr_hash_t *digests;
};
static svn_error_t *
post_process_commit_item(void *baton, void *this_item, apr_pool_t *pool) {
struct post_commit_baton *btn = baton;
apr_pool_t *subpool = btn->qpool;
svn_client_commit_item3_t *item =
*(svn_client_commit_item3_t **)this_item;
svn_boolean_t loop_recurse = FALSE;
const char *adm_access_path;
svn_wc_adm_access_t *adm_access;
svn_boolean_t remove_lock;
svn_error_t *bump_err;
if (item->kind == svn_node_dir)
adm_access_path = item->path;
else
svn_path_split(item->path, &adm_access_path, NULL, pool);
bump_err = svn_wc_adm_retrieve(&adm_access, btn->base_dir_access,
adm_access_path, pool);
if (bump_err
&& bump_err->apr_err == SVN_ERR_WC_NOT_LOCKED) {
if (item->kind == svn_node_dir
&& item->state_flags & SVN_CLIENT_COMMIT_ITEM_DELETE) {
svn_error_clear(bump_err);
return svn_wc_mark_missing_deleted(item->path,
btn->base_dir_access, pool);
}
}
if (bump_err)
return bump_err;
if ((item->state_flags & SVN_CLIENT_COMMIT_ITEM_ADD)
&& (item->kind == svn_node_dir)
&& (item->copyfrom_url))
loop_recurse = TRUE;
remove_lock = (! btn->keep_locks && (item->state_flags
& SVN_CLIENT_COMMIT_ITEM_LOCK_TOKEN));
SVN_ERR(svn_wc_queue_committed
(&(btn->queue),
item->path, adm_access, loop_recurse,
item->incoming_prop_changes,
remove_lock, (! btn->keep_changelists),
apr_hash_get(btn->digests, item->path, APR_HASH_KEY_STRING),
subpool));
return SVN_NO_ERROR;
}
static svn_error_t *
commit_item_is_changed(void *baton, void *this_item, apr_pool_t *pool) {
svn_client_commit_item3_t **item = this_item;
if ((*item)->state_flags != SVN_CLIENT_COMMIT_ITEM_LOCK_TOKEN)
svn_iter_break(pool);
return SVN_NO_ERROR;
}
struct lock_dirs_baton {
svn_client_ctx_t *ctx;
svn_wc_adm_access_t *base_dir_access;
int levels_to_lock;
};
static svn_error_t *
lock_dirs_for_commit(void *baton, void *this_item, apr_pool_t *pool) {
struct lock_dirs_baton *btn = baton;
svn_wc_adm_access_t *adm_access;
return svn_wc_adm_open3(&adm_access, btn->base_dir_access,
*(const char **)this_item,
TRUE,
btn->levels_to_lock,
btn->ctx->cancel_func,
btn->ctx->cancel_baton,
pool);
}
struct check_dir_delete_baton {
svn_wc_adm_access_t *base_dir_access;
svn_depth_t depth;
};
static svn_error_t *
check_nonrecursive_dir_delete(void *baton, void *this_item, apr_pool_t *pool) {
struct check_dir_delete_baton *btn = baton;
svn_wc_adm_access_t *adm_access;
const char *target;
SVN_ERR(svn_path_get_absolute(&target, *(const char **)this_item, pool));
SVN_ERR_W(svn_wc_adm_probe_retrieve(&adm_access, btn->base_dir_access,
target, pool),
_("Are all the targets part of the same working copy?"));
if (btn->depth != svn_depth_infinity) {
svn_wc_status2_t *status;
svn_node_kind_t kind;
SVN_ERR(svn_io_check_path(target, &kind, pool));
if (kind == svn_node_dir) {
SVN_ERR(svn_wc_status2(&status, target, adm_access, pool));
if (status->text_status == svn_wc_status_deleted ||
status->text_status == svn_wc_status_replaced)
return svn_error_create(SVN_ERR_UNSUPPORTED_FEATURE, NULL,
_("Cannot non-recursively commit a "
"directory deletion"));
}
}
return SVN_NO_ERROR;
}
svn_error_t *
svn_client_commit4(svn_commit_info_t **commit_info_p,
const apr_array_header_t *targets,
svn_depth_t depth,
svn_boolean_t keep_locks,
svn_boolean_t keep_changelists,
const apr_array_header_t *changelists,
const apr_hash_t *revprop_table,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
const svn_delta_editor_t *editor;
void *edit_baton;
svn_ra_session_t *ra_session;
const char *log_msg;
const char *base_dir;
const char *base_url;
const char *target;
apr_array_header_t *rel_targets;
apr_array_header_t *dirs_to_lock;
apr_array_header_t *dirs_to_lock_recursive;
svn_boolean_t lock_base_dir_recursive = FALSE;
apr_hash_t *committables, *lock_tokens, *tempfiles = NULL, *digests;
svn_wc_adm_access_t *base_dir_access;
apr_array_header_t *commit_items;
svn_error_t *cmt_err = SVN_NO_ERROR, *unlock_err = SVN_NO_ERROR;
svn_error_t *bump_err = SVN_NO_ERROR, *cleanup_err = SVN_NO_ERROR;
svn_boolean_t commit_in_progress = FALSE;
const char *display_dir = "";
int i;
for (i = 0; i < targets->nelts; i++) {
target = APR_ARRAY_IDX(targets, i, const char *);
if (svn_path_is_url(target))
return svn_error_createf
(SVN_ERR_ILLEGAL_TARGET, NULL,
_("'%s' is a URL, but URLs cannot be commit targets"), target);
}
SVN_ERR(svn_path_condense_targets(&base_dir, &rel_targets, targets,
depth == svn_depth_infinity, pool));
if (depth == svn_depth_files || depth == svn_depth_immediates) {
const char *rel_target;
for (i = 0; i < rel_targets->nelts; ++i) {
rel_target = APR_ARRAY_IDX(rel_targets, i, const char *);
if (rel_target[0] == '\0')
lock_base_dir_recursive = TRUE;
}
}
if (! base_dir)
goto cleanup;
dirs_to_lock = apr_array_make(pool, 1, sizeof(target));
dirs_to_lock_recursive = apr_array_make(pool, 1, sizeof(target));
if ((! rel_targets) || (! rel_targets->nelts)) {
const char *parent_dir, *name;
SVN_ERR(svn_wc_get_actual_target(base_dir, &parent_dir, &name, pool));
if (*name) {
svn_node_kind_t kind;
base_dir = apr_pstrdup(pool, parent_dir);
if (! rel_targets)
rel_targets = apr_array_make(pool, targets->nelts, sizeof(name));
APR_ARRAY_PUSH(rel_targets, const char *) = name;
target = svn_path_join(base_dir, name, pool);
SVN_ERR(svn_io_check_path(target, &kind, pool));
if (kind == svn_node_dir) {
if (depth == svn_depth_infinity || depth == svn_depth_immediates)
APR_ARRAY_PUSH(dirs_to_lock_recursive, const char *) = target;
else
APR_ARRAY_PUSH(dirs_to_lock, const char *) = target;
}
} else {
lock_base_dir_recursive = TRUE;
}
} else if (! lock_base_dir_recursive) {
apr_pool_t *subpool = svn_pool_create(pool);
SVN_ERR(adjust_rel_targets(&base_dir, &rel_targets,
base_dir, rel_targets,
pool));
for (i = 0; i < rel_targets->nelts; i++) {
const char *parent_dir, *name;
svn_node_kind_t kind;
svn_pool_clear(subpool);
target = svn_path_join(base_dir,
APR_ARRAY_IDX(rel_targets, i, const char *),
subpool);
SVN_ERR(svn_io_check_path(target, &kind, subpool));
if (kind == svn_node_dir) {
if (depth == svn_depth_infinity || depth == svn_depth_immediates)
APR_ARRAY_PUSH(dirs_to_lock_recursive,
const char *) = apr_pstrdup(pool, target);
else
if (strcmp(target, base_dir) != 0)
APR_ARRAY_PUSH(dirs_to_lock,
const char *) = apr_pstrdup(pool, target);
}
if (strcmp(target, base_dir) != 0) {
svn_path_split(target, &parent_dir, &name, subpool);
target = parent_dir;
while (strcmp(target, base_dir) != 0) {
if ((target[0] == '\0') ||
svn_dirent_is_root(target, strlen(target))
)
abort();
APR_ARRAY_PUSH(dirs_to_lock,
const char *) = apr_pstrdup(pool, target);
target = svn_path_dirname(target, subpool);
}
}
}
svn_pool_destroy(subpool);
}
SVN_ERR(svn_wc_adm_open3(&base_dir_access, NULL, base_dir,
TRUE,
lock_base_dir_recursive ? -1 : 0,
ctx->cancel_func, ctx->cancel_baton,
pool));
if (!lock_base_dir_recursive) {
apr_array_header_t *unique_dirs_to_lock;
struct lock_dirs_baton btn;
qsort(dirs_to_lock->elts, dirs_to_lock->nelts,
dirs_to_lock->elt_size, svn_sort_compare_paths);
qsort(dirs_to_lock_recursive->elts, dirs_to_lock_recursive->nelts,
dirs_to_lock_recursive->elt_size, svn_sort_compare_paths);
SVN_ERR(svn_path_remove_redundancies(&unique_dirs_to_lock,
dirs_to_lock_recursive,
pool));
dirs_to_lock_recursive = unique_dirs_to_lock;
SVN_ERR(remove_redundancies(&unique_dirs_to_lock,
dirs_to_lock,
dirs_to_lock_recursive,
pool));
dirs_to_lock = unique_dirs_to_lock;
btn.base_dir_access = base_dir_access;
btn.ctx = ctx;
btn.levels_to_lock = 0;
if (dirs_to_lock)
SVN_ERR(svn_iter_apr_array(NULL, dirs_to_lock,
lock_dirs_for_commit, &btn, pool));
btn.levels_to_lock = -1;
if (dirs_to_lock_recursive)
SVN_ERR(svn_iter_apr_array(NULL, dirs_to_lock_recursive,
lock_dirs_for_commit, &btn, pool));
}
{
struct check_dir_delete_baton btn;
btn.base_dir_access = base_dir_access;
btn.depth = depth;
SVN_ERR(svn_iter_apr_array(NULL, targets,
check_nonrecursive_dir_delete, &btn,
pool));
}
if ((cmt_err = svn_client__harvest_committables(&committables,
&lock_tokens,
base_dir_access,
rel_targets,
depth,
! keep_locks,
changelists,
ctx,
pool)))
goto cleanup;
if (! ((commit_items = apr_hash_get(committables,
SVN_CLIENT__SINGLE_REPOS_NAME,
APR_HASH_KEY_STRING))))
goto cleanup;
{
svn_boolean_t not_found_changed_path = TRUE;
cmt_err = svn_iter_apr_array(&not_found_changed_path,
commit_items,
commit_item_is_changed, NULL, pool);
if (not_found_changed_path || cmt_err)
goto cleanup;
}
if (SVN_CLIENT__HAS_LOG_MSG_FUNC(ctx)) {
const char *tmp_file;
cmt_err = svn_client__get_log_msg(&log_msg, &tmp_file, commit_items,
ctx, pool);
if (cmt_err || (! log_msg))
goto cleanup;
} else
log_msg = "";
if ((cmt_err = svn_client__condense_commit_items(&base_url,
commit_items,
pool)))
goto cleanup;
if ((cmt_err = collect_lock_tokens(&lock_tokens, lock_tokens, base_url,
pool)))
goto cleanup;
if ((cmt_err = get_ra_editor(&ra_session, NULL,
&editor, &edit_baton, ctx,
base_url, base_dir, base_dir_access, log_msg,
commit_items, revprop_table, commit_info_p,
TRUE, lock_tokens, keep_locks, pool)))
goto cleanup;
commit_in_progress = TRUE;
if ((cmt_err = svn_path_get_absolute(&display_dir,
display_dir, pool)))
goto cleanup;
display_dir = svn_path_get_longest_ancestor(display_dir, base_dir, pool);
cmt_err = svn_client__do_commit(base_url, commit_items, base_dir_access,
editor, edit_baton,
display_dir,
&tempfiles, &digests, ctx, pool);
if ((! cmt_err)
|| (cmt_err->apr_err == SVN_ERR_REPOS_POST_COMMIT_HOOK_FAILED)) {
svn_wc_committed_queue_t *queue = svn_wc_committed_queue_create(pool);
struct post_commit_baton btn;
btn.queue = queue;
btn.qpool = pool;
btn.base_dir_access = base_dir_access;
btn.keep_changelists = keep_changelists;
btn.keep_locks = keep_locks;
btn.digests = digests;
commit_in_progress = FALSE;
bump_err = svn_iter_apr_array(NULL, commit_items,
post_process_commit_item, &btn,
pool);
if (bump_err)
goto cleanup;
assert(*commit_info_p);
bump_err
= svn_wc_process_committed_queue(queue, base_dir_access,
(*commit_info_p)->revision,
(*commit_info_p)->date,
(*commit_info_p)->author,
pool);
}
svn_sleep_for_timestamps();
cleanup:
if (commit_in_progress)
svn_error_clear(editor->abort_edit(edit_baton, pool));
if (! bump_err) {
unlock_err = svn_wc_adm_close(base_dir_access);
if (! unlock_err)
cleanup_err = remove_tmpfiles(tempfiles, pool);
}
if (! *commit_info_p)
*commit_info_p = svn_create_commit_info(pool);
return reconcile_errors(cmt_err, unlock_err, bump_err, cleanup_err, pool);
}
svn_error_t *
svn_client_commit3(svn_commit_info_t **commit_info_p,
const apr_array_header_t *targets,
svn_boolean_t recurse,
svn_boolean_t keep_locks,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
svn_depth_t depth = SVN_DEPTH_INFINITY_OR_FILES(recurse);
return svn_client_commit4(commit_info_p, targets, depth, keep_locks,
FALSE, NULL, NULL, ctx, pool);
}
svn_error_t *
svn_client_commit2(svn_client_commit_info_t **commit_info_p,
const apr_array_header_t *targets,
svn_boolean_t recurse,
svn_boolean_t keep_locks,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
svn_commit_info_t *commit_info = NULL;
svn_error_t *err;
err = svn_client_commit3(&commit_info, targets, recurse, keep_locks,
ctx, pool);
*commit_info_p = (svn_client_commit_info_t *) commit_info;
return err;
}
svn_error_t *
svn_client_commit(svn_client_commit_info_t **commit_info_p,
const apr_array_header_t *targets,
svn_boolean_t nonrecursive,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
return svn_client_commit2(commit_info_p, targets,
nonrecursive ? FALSE : TRUE,
TRUE,
ctx, pool);
}
