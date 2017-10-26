#include <string.h>
#include <apr_lib.h>
#include <apr_fnmatch.h>
#include "svn_wc.h"
#include "svn_client.h"
#include "svn_string.h"
#include "svn_pools.h"
#include "svn_error.h"
#include "svn_path.h"
#include "svn_io.h"
#include "svn_config.h"
#include "svn_props.h"
#include "svn_hash.h"
#include "svn_sorts.h"
#include "client.h"
#include "svn_private_config.h"
typedef struct {
const char *filename;
svn_boolean_t have_executable;
const char *mimetype;
apr_hash_t *properties;
apr_pool_t *pool;
} auto_props_baton_t;
static void
trim_string(char **pstr) {
char *str = *pstr;
int i;
while (apr_isspace(*str))
str++;
*pstr = str;
i = strlen(str);
while ((i > 0) && apr_isspace(str[i-1]))
i--;
str[i] = '\0';
}
static svn_boolean_t
auto_props_enumerator(const char *name,
const char *value,
void *baton,
apr_pool_t *pool) {
auto_props_baton_t *autoprops = baton;
char *property;
char *last_token;
if (strlen(value) == 0)
return TRUE;
if (apr_fnmatch(name, autoprops->filename, APR_FNM_CASE_BLIND) == APR_FNM_NOMATCH)
return TRUE;
property = apr_pstrdup(autoprops->pool, value);
property = apr_strtok(property, ";", &last_token);
while (property) {
int len;
const char *this_value;
char *equal_sign = strchr(property, '=');
if (equal_sign) {
*equal_sign = '\0';
equal_sign++;
trim_string(&equal_sign);
this_value = equal_sign;
} else {
this_value = "";
}
trim_string(&property);
len = strlen(property);
if (len > 0) {
svn_string_t *propval = svn_string_create(this_value,
autoprops->pool);
apr_hash_set(autoprops->properties, property, len, propval);
if (strcmp(property, SVN_PROP_MIME_TYPE) == 0)
autoprops->mimetype = this_value;
else if (strcmp(property, SVN_PROP_EXECUTABLE) == 0)
autoprops->have_executable = TRUE;
}
property = apr_strtok(NULL, ";", &last_token);
}
return TRUE;
}
svn_error_t *
svn_client__get_auto_props(apr_hash_t **properties,
const char **mimetype,
const char *path,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
svn_config_t *cfg;
svn_boolean_t use_autoprops;
auto_props_baton_t autoprops;
autoprops.properties = apr_hash_make(pool);
autoprops.filename = svn_path_basename(path, pool);
autoprops.pool = pool;
autoprops.mimetype = NULL;
autoprops.have_executable = FALSE;
*properties = autoprops.properties;
cfg = ctx->config ? apr_hash_get(ctx->config, SVN_CONFIG_CATEGORY_CONFIG,
APR_HASH_KEY_STRING) : NULL;
SVN_ERR(svn_config_get_bool(cfg, &use_autoprops,
SVN_CONFIG_SECTION_MISCELLANY,
SVN_CONFIG_OPTION_ENABLE_AUTO_PROPS, FALSE));
if (use_autoprops)
svn_config_enumerate2(cfg, SVN_CONFIG_SECTION_AUTO_PROPS,
auto_props_enumerator, &autoprops, pool);
if (! autoprops.mimetype) {
SVN_ERR(svn_io_detect_mimetype2(&autoprops.mimetype, path,
ctx->mimetypes_map, pool));
if (autoprops.mimetype)
apr_hash_set(autoprops.properties, SVN_PROP_MIME_TYPE,
strlen(SVN_PROP_MIME_TYPE),
svn_string_create(autoprops.mimetype, pool));
}
#if !defined(AS400)
if (! autoprops.have_executable) {
svn_boolean_t executable = FALSE;
SVN_ERR(svn_io_is_file_executable(&executable, path, pool));
if (executable)
apr_hash_set(autoprops.properties, SVN_PROP_EXECUTABLE,
strlen(SVN_PROP_EXECUTABLE),
svn_string_create("", pool));
}
#endif
*mimetype = autoprops.mimetype;
return SVN_NO_ERROR;
}
static svn_error_t *
add_file(const char *path,
svn_client_ctx_t *ctx,
svn_wc_adm_access_t *adm_access,
apr_pool_t *pool) {
apr_hash_t* properties;
apr_hash_index_t *hi;
const char *mimetype;
svn_node_kind_t kind;
svn_boolean_t is_special;
SVN_ERR(svn_io_check_special_path(path, &kind, &is_special, pool));
if (is_special)
mimetype = NULL;
else
SVN_ERR(svn_client__get_auto_props(&properties, &mimetype, path, ctx,
pool));
SVN_ERR(svn_wc_add2(path, adm_access, NULL, SVN_INVALID_REVNUM,
ctx->cancel_func, ctx->cancel_baton,
NULL, NULL, pool));
if (is_special)
SVN_ERR(svn_wc_prop_set2
(SVN_PROP_SPECIAL,
svn_string_create(SVN_PROP_BOOLEAN_TRUE, pool),
path, adm_access, FALSE, pool));
else if (properties) {
for (hi = apr_hash_first(pool, properties);
hi != NULL; hi = apr_hash_next(hi)) {
const void *pname;
void *pval;
apr_hash_this(hi, &pname, NULL, &pval);
SVN_ERR(svn_wc_prop_set2(pname, pval, path,
adm_access, FALSE, pool));
}
}
if (ctx->notify_func2 != NULL) {
svn_wc_notify_t *notify = svn_wc_create_notify(path, svn_wc_notify_add,
pool);
notify->kind = svn_node_file;
notify->mime_type = mimetype;
(*ctx->notify_func2)(ctx->notify_baton2, notify, pool);
}
return SVN_NO_ERROR;
}
static svn_error_t *
add_dir_recursive(const char *dirname,
svn_wc_adm_access_t *adm_access,
svn_depth_t depth,
svn_boolean_t force,
svn_boolean_t no_ignore,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
apr_dir_t *dir;
apr_finfo_t this_entry;
svn_error_t *err;
apr_pool_t *subpool;
apr_int32_t flags = APR_FINFO_TYPE | APR_FINFO_NAME;
svn_wc_adm_access_t *dir_access;
apr_array_header_t *ignores;
if (ctx->cancel_func)
SVN_ERR(ctx->cancel_func(ctx->cancel_baton));
err = svn_wc_add2(dirname, adm_access,
NULL, SVN_INVALID_REVNUM,
ctx->cancel_func, ctx->cancel_baton,
ctx->notify_func2, ctx->notify_baton2, pool);
if (err && err->apr_err == SVN_ERR_ENTRY_EXISTS && force)
svn_error_clear(err);
else if (err)
return err;
SVN_ERR(svn_wc_adm_retrieve(&dir_access, adm_access, dirname, pool));
if (!no_ignore)
SVN_ERR(svn_wc_get_ignores(&ignores, ctx->config, dir_access, pool));
subpool = svn_pool_create(pool);
SVN_ERR(svn_io_dir_open(&dir, dirname, pool));
while (1) {
const char *fullpath;
svn_pool_clear(subpool);
err = svn_io_dir_read(&this_entry, flags, dir, subpool);
if (err) {
if (APR_STATUS_IS_ENOENT(err->apr_err)) {
apr_status_t apr_err;
svn_error_clear(err);
apr_err = apr_dir_close(dir);
if (apr_err)
return svn_error_wrap_apr
(apr_err, _("Can't close directory '%s'"),
svn_path_local_style(dirname, subpool));
break;
} else {
return svn_error_createf
(err->apr_err, err,
_("Error during add of '%s'"),
svn_path_local_style(dirname, subpool));
}
}
if (this_entry.name[0] == '.'
&& (this_entry.name[1] == '\0'
|| (this_entry.name[1] == '.' && this_entry.name[2] == '\0')))
continue;
if (ctx->cancel_func)
SVN_ERR(ctx->cancel_func(ctx->cancel_baton));
if (svn_wc_is_adm_dir(this_entry.name, subpool))
continue;
if ((!no_ignore) && svn_wc_match_ignore_list(this_entry.name,
ignores, subpool))
continue;
fullpath = svn_path_join(dirname, this_entry.name, subpool);
if (this_entry.filetype == APR_DIR && depth >= svn_depth_immediates) {
svn_depth_t depth_below_here = depth;
if (depth == svn_depth_immediates)
depth_below_here = svn_depth_empty;
SVN_ERR(add_dir_recursive(fullpath, dir_access, depth_below_here,
force, no_ignore, ctx, subpool));
} else if (this_entry.filetype != APR_UNKFILE
&& this_entry.filetype != APR_DIR
&& depth >= svn_depth_files) {
err = add_file(fullpath, ctx, dir_access, subpool);
if (err && err->apr_err == SVN_ERR_ENTRY_EXISTS && force)
svn_error_clear(err);
else if (err)
return err;
}
}
SVN_ERR(svn_wc_adm_close(dir_access));
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
static svn_error_t *
add(const char *path,
svn_depth_t depth,
svn_boolean_t force,
svn_boolean_t no_ignore,
svn_wc_adm_access_t *adm_access,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
svn_node_kind_t kind;
svn_error_t *err;
SVN_ERR(svn_io_check_path(path, &kind, pool));
if (kind == svn_node_dir && depth >= svn_depth_files)
err = add_dir_recursive(path, adm_access, depth,
force, no_ignore, ctx, pool);
else if (kind == svn_node_file)
err = add_file(path, ctx, adm_access, pool);
else
err = svn_wc_add2(path, adm_access, NULL, SVN_INVALID_REVNUM,
ctx->cancel_func, ctx->cancel_baton,
ctx->notify_func2, ctx->notify_baton2, pool);
if (err && err->apr_err == SVN_ERR_ENTRY_EXISTS && force) {
svn_error_clear(err);
err = SVN_NO_ERROR;
}
return err;
}
static svn_error_t *
add_parent_dirs(const char *path,
svn_wc_adm_access_t **parent_access,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
svn_wc_adm_access_t *adm_access;
svn_error_t *err;
err = svn_wc_adm_open3(&adm_access, NULL, path, TRUE, 0,
ctx->cancel_func, ctx->cancel_baton, pool);
if (err && err->apr_err == SVN_ERR_WC_NOT_DIRECTORY) {
if (svn_dirent_is_root(path, strlen(path))) {
svn_error_clear(err);
return svn_error_create
(SVN_ERR_CLIENT_NO_VERSIONED_PARENT, NULL, NULL);
} else {
const char *parent_path = svn_path_dirname(path, pool);
svn_error_clear(err);
SVN_ERR(add_parent_dirs(parent_path, &adm_access, ctx, pool));
SVN_ERR(svn_wc_adm_retrieve(&adm_access, adm_access, parent_path,
pool));
SVN_ERR(svn_wc_add2(path, adm_access, NULL, SVN_INVALID_REVNUM,
ctx->cancel_func, ctx->cancel_baton,
ctx->notify_func2, ctx->notify_baton2, pool));
}
} else if (err) {
return err;
}
if (parent_access)
*parent_access = adm_access;
return SVN_NO_ERROR;
}
svn_error_t *
svn_client_add4(const char *path,
svn_depth_t depth,
svn_boolean_t force,
svn_boolean_t no_ignore,
svn_boolean_t add_parents,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
svn_error_t *err, *err2;
svn_wc_adm_access_t *adm_access;
const char *parent_dir;
if (add_parents) {
apr_pool_t *subpool;
SVN_ERR(svn_path_get_absolute(&path, path, pool));
parent_dir = svn_path_dirname(path, pool);
subpool = svn_pool_create(pool);
SVN_ERR(add_parent_dirs(parent_dir, &adm_access, ctx, subpool));
SVN_ERR(svn_wc_adm_close(adm_access));
svn_pool_destroy(subpool);
SVN_ERR(svn_wc_adm_open3(&adm_access, NULL, parent_dir,
TRUE, 0, ctx->cancel_func, ctx->cancel_baton,
pool));
} else {
parent_dir = svn_path_dirname(path, pool);
SVN_ERR(svn_wc_adm_open3(&adm_access, NULL, parent_dir,
TRUE, 0, ctx->cancel_func, ctx->cancel_baton,
pool));
}
err = add(path, depth, force, no_ignore, adm_access, ctx, pool);
err2 = svn_wc_adm_close(adm_access);
if (err2) {
if (err)
svn_error_clear(err2);
else
err = err2;
}
return err;
}
svn_error_t *
svn_client_add3(const char *path,
svn_boolean_t recursive,
svn_boolean_t force,
svn_boolean_t no_ignore,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
return svn_client_add4(path, SVN_DEPTH_INFINITY_OR_EMPTY(recursive),
force, no_ignore, FALSE, ctx,
pool);
}
svn_error_t *
svn_client_add2(const char *path,
svn_boolean_t recursive,
svn_boolean_t force,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
return svn_client_add3(path, recursive, force, FALSE, ctx, pool);
}
svn_error_t *
svn_client_add(const char *path,
svn_boolean_t recursive,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
return svn_client_add3(path, recursive, FALSE, FALSE, ctx, pool);
}
static svn_error_t *
path_driver_cb_func(void **dir_baton,
void *parent_baton,
void *callback_baton,
const char *path,
apr_pool_t *pool) {
const svn_delta_editor_t *editor = callback_baton;
SVN_ERR(svn_path_check_valid(path, pool));
return editor->add_directory(path, parent_baton, NULL,
SVN_INVALID_REVNUM, pool, dir_baton);
}
static svn_error_t *
add_url_parents(svn_ra_session_t *ra_session,
const char *url,
apr_array_header_t *targets,
apr_pool_t *temppool,
apr_pool_t *pool) {
svn_node_kind_t kind;
const char *parent_url;
svn_path_split(url, &parent_url, NULL, pool);
SVN_ERR(svn_ra_reparent(ra_session, parent_url, temppool));
SVN_ERR(svn_ra_check_path(ra_session, "", SVN_INVALID_REVNUM, &kind,
temppool));
if (kind == svn_node_none)
SVN_ERR(add_url_parents(ra_session, parent_url, targets, temppool, pool));
APR_ARRAY_PUSH(targets, const char *) = url;
return SVN_NO_ERROR;
}
static svn_error_t *
mkdir_urls(svn_commit_info_t **commit_info_p,
const apr_array_header_t *urls,
svn_boolean_t make_parents,
const apr_hash_t *revprop_table,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
svn_ra_session_t *ra_session = NULL;
const svn_delta_editor_t *editor;
void *edit_baton;
void *commit_baton;
const char *log_msg;
apr_array_header_t *targets;
apr_hash_t *targets_hash;
apr_hash_t *commit_revprops;
svn_error_t *err;
const char *common;
int i;
if (make_parents) {
apr_array_header_t *all_urls = apr_array_make(pool, urls->nelts,
sizeof(const char *));
const char *first_url = APR_ARRAY_IDX(urls, 0, const char *);
apr_pool_t *iterpool = svn_pool_create(pool);
SVN_ERR(svn_client__open_ra_session_internal(&ra_session, first_url,
NULL, NULL, NULL, FALSE,
TRUE, ctx, pool));
for (i = 0; i < urls->nelts; i++) {
const char *url = APR_ARRAY_IDX(urls, i, const char *);
svn_pool_clear(iterpool);
SVN_ERR(add_url_parents(ra_session, url, all_urls, iterpool, pool));
}
svn_pool_destroy(iterpool);
urls = all_urls;
}
SVN_ERR(svn_path_condense_targets(&common, &targets, urls, FALSE, pool));
SVN_ERR(svn_hash_from_cstring_keys(&targets_hash, targets, pool));
SVN_ERR(svn_hash_keys(&targets, targets_hash, pool));
if (! targets->nelts) {
const char *bname;
svn_path_split(common, &common, &bname, pool);
APR_ARRAY_PUSH(targets, const char *) = bname;
} else {
svn_boolean_t resplit = FALSE;
for (i = 0; i < targets->nelts; i++) {
const char *path = APR_ARRAY_IDX(targets, i, const char *);
if (! *path) {
resplit = TRUE;
break;
}
}
if (resplit) {
const char *bname;
svn_path_split(common, &common, &bname, pool);
for (i = 0; i < targets->nelts; i++) {
const char *path = APR_ARRAY_IDX(targets, i, const char *);
path = svn_path_join(bname, path, pool);
APR_ARRAY_IDX(targets, i, const char *) = path;
}
}
}
qsort(targets->elts, targets->nelts, targets->elt_size,
svn_sort_compare_paths);
if (SVN_CLIENT__HAS_LOG_MSG_FUNC(ctx)) {
svn_client_commit_item3_t *item;
const char *tmp_file;
apr_array_header_t *commit_items
= apr_array_make(pool, targets->nelts, sizeof(item));
for (i = 0; i < targets->nelts; i++) {
const char *path = APR_ARRAY_IDX(targets, i, const char *);
SVN_ERR(svn_client_commit_item_create
((const svn_client_commit_item3_t **) &item, pool));
item->url = svn_path_join(common, path, pool);
item->state_flags = SVN_CLIENT_COMMIT_ITEM_ADD;
APR_ARRAY_PUSH(commit_items, svn_client_commit_item3_t *) = item;
}
SVN_ERR(svn_client__get_log_msg(&log_msg, &tmp_file, commit_items,
ctx, pool));
if (! log_msg)
return SVN_NO_ERROR;
} else
log_msg = "";
SVN_ERR(svn_client__ensure_revprop_table(&commit_revprops, revprop_table,
log_msg, ctx, pool));
if (!ra_session)
SVN_ERR(svn_client__open_ra_session_internal(&ra_session, common, NULL,
NULL, NULL, FALSE, TRUE,
ctx, pool));
for (i = 0; i < targets->nelts; i++) {
const char *path = APR_ARRAY_IDX(targets, i, const char *);
path = svn_path_uri_decode(path, pool);
APR_ARRAY_IDX(targets, i, const char *) = path;
}
SVN_ERR(svn_client__commit_get_baton(&commit_baton, commit_info_p, pool));
SVN_ERR(svn_ra_get_commit_editor3(ra_session, &editor, &edit_baton,
commit_revprops,
svn_client__commit_callback,
commit_baton,
NULL, TRUE,
pool));
err = svn_delta_path_driver(editor, edit_baton, SVN_INVALID_REVNUM,
targets, path_driver_cb_func,
(void *)editor, pool);
if (err) {
svn_error_clear(editor->abort_edit(edit_baton, pool));
return err;
}
SVN_ERR(editor->close_edit(edit_baton, pool));
return SVN_NO_ERROR;
}
svn_error_t *
svn_client__make_local_parents(const char *path,
svn_boolean_t make_parents,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
svn_error_t *err;
svn_node_kind_t orig_kind;
SVN_ERR(svn_io_check_path(path, &orig_kind, pool));
if (make_parents)
SVN_ERR(svn_io_make_dir_recursively(path, pool));
else
SVN_ERR(svn_io_dir_make(path, APR_OS_DEFAULT, pool));
err = svn_client_add4(path, svn_depth_empty, FALSE, FALSE,
make_parents, ctx, pool);
if (err && (orig_kind == svn_node_none)) {
svn_error_clear(svn_io_remove_dir(path, pool));
}
return err;
}
svn_error_t *
svn_client_mkdir3(svn_commit_info_t **commit_info_p,
const apr_array_header_t *paths,
svn_boolean_t make_parents,
const apr_hash_t *revprop_table,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
if (! paths->nelts)
return SVN_NO_ERROR;
if (svn_path_is_url(APR_ARRAY_IDX(paths, 0, const char *))) {
SVN_ERR(mkdir_urls(commit_info_p, paths, make_parents,
revprop_table, ctx, pool));
} else {
apr_pool_t *subpool = svn_pool_create(pool);
int i;
for (i = 0; i < paths->nelts; i++) {
const char *path = APR_ARRAY_IDX(paths, i, const char *);
svn_pool_clear(subpool);
if (ctx->cancel_func)
SVN_ERR(ctx->cancel_func(ctx->cancel_baton));
SVN_ERR(svn_client__make_local_parents(path, make_parents, ctx,
subpool));
}
svn_pool_destroy(subpool);
}
return SVN_NO_ERROR;
}
svn_error_t *
svn_client_mkdir2(svn_commit_info_t **commit_info_p,
const apr_array_header_t *paths,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
return svn_client_mkdir3(commit_info_p, paths, FALSE, NULL, ctx, pool);
}
svn_error_t *
svn_client_mkdir(svn_client_commit_info_t **commit_info_p,
const apr_array_header_t *paths,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
svn_commit_info_t *commit_info = NULL;
svn_error_t *err;
err = svn_client_mkdir2(&commit_info, paths, ctx, pool);
*commit_info_p = (svn_client_commit_info_t *) commit_info;
return err;
}
