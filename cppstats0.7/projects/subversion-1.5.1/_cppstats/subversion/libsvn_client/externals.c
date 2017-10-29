#include <assert.h>
#include <apr_uri.h>
#include "svn_wc.h"
#include "svn_pools.h"
#include "svn_client.h"
#include "svn_hash.h"
#include "svn_types.h"
#include "svn_error.h"
#include "svn_path.h"
#include "client.h"
#include "svn_private_config.h"
struct handle_external_item_change_baton {
apr_hash_t *new_desc;
apr_hash_t *old_desc;
const char *parent_dir;
const char *parent_dir_url;
const char *repos_root_url;
svn_client_ctx_t *ctx;
svn_boolean_t update_unchanged;
svn_boolean_t *timestamp_sleep;
svn_boolean_t is_export;
apr_pool_t *pool;
};
static svn_boolean_t
compare_external_items(svn_wc_external_item2_t *new_item,
svn_wc_external_item2_t *old_item) {
if ((strcmp(new_item->target_dir, old_item->target_dir) != 0)
|| (strcmp(new_item->url, old_item->url) != 0)
|| (! svn_client__compare_revisions(&(new_item->revision),
&(old_item->revision)))
|| (! svn_client__compare_revisions(&(new_item->peg_revision),
&(old_item->peg_revision))))
return FALSE;
return TRUE;
}
static svn_error_t *
relegate_external(const char *path,
svn_cancel_func_t cancel_func,
void *cancel_baton,
apr_pool_t *pool) {
svn_error_t *err;
svn_wc_adm_access_t *adm_access;
SVN_ERR(svn_wc_adm_open3(&adm_access, NULL, path, TRUE, -1, cancel_func,
cancel_baton, pool));
err = svn_wc_remove_from_revision_control(adm_access,
SVN_WC_ENTRY_THIS_DIR,
TRUE, FALSE,
cancel_func,
cancel_baton,
pool);
if (!err || err->apr_err == SVN_ERR_WC_LEFT_LOCAL_MOD)
SVN_ERR(svn_wc_adm_close(adm_access));
if (err && (err->apr_err == SVN_ERR_WC_LEFT_LOCAL_MOD)) {
const char *new_path;
svn_error_clear(err);
SVN_ERR(svn_io_open_unique_file2
(NULL, &new_path, path, ".OLD", svn_io_file_del_none, pool));
err = svn_io_remove_file(new_path, pool);
svn_error_clear(err);
SVN_ERR(svn_io_file_rename(path, new_path, pool));
} else if (err)
return err;
return SVN_NO_ERROR;
}
static svn_error_t *
switch_external(const char *path,
const char *url,
const svn_opt_revision_t *revision,
const svn_opt_revision_t *peg_revision,
svn_boolean_t *timestamp_sleep,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
svn_node_kind_t kind;
svn_error_t *err;
apr_pool_t *subpool = svn_pool_create(pool);
if (ctx->notify_func2)
ctx->notify_func2(ctx->notify_baton2,
svn_wc_create_notify(path, svn_wc_notify_update_external,
pool), pool);
SVN_ERR(svn_io_check_path(path, &kind, pool));
if (kind == svn_node_dir) {
svn_wc_adm_access_t *adm_access;
const svn_wc_entry_t *entry;
SVN_ERR(svn_wc_adm_open3(&adm_access, NULL, path, TRUE, 0,
ctx->cancel_func, ctx->cancel_baton, subpool));
SVN_ERR(svn_wc_entry(&entry, path, adm_access,
FALSE, subpool));
SVN_ERR(svn_wc_adm_close(adm_access));
if (entry && entry->url) {
if (strcmp(entry->url, url) == 0) {
SVN_ERR(svn_client__update_internal(NULL, path, revision,
svn_depth_unknown, FALSE,
FALSE, FALSE,
timestamp_sleep, TRUE,
ctx, subpool));
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
} else if (entry->repos) {
if (! svn_path_is_ancestor(entry->repos, url)) {
const char *repos_root;
svn_ra_session_t *ra_session;
SVN_ERR(svn_client__open_ra_session_internal
(&ra_session, url, NULL, NULL, NULL, FALSE, TRUE,
ctx, subpool));
SVN_ERR(svn_ra_get_repos_root2(ra_session, &repos_root,
subpool));
err = svn_client_relocate(path, entry->repos, repos_root,
TRUE, ctx, subpool);
if (err
&& (err->apr_err == SVN_ERR_WC_INVALID_RELOCATION
|| (err->apr_err
== SVN_ERR_CLIENT_INVALID_RELOCATION))) {
svn_error_clear(err);
goto relegate;
} else if (err)
return err;
}
SVN_ERR(svn_client__switch_internal(NULL, path, url,
peg_revision, revision,
svn_depth_infinity, TRUE,
timestamp_sleep,
FALSE, FALSE, ctx, subpool));
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
}
}
relegate:
svn_pool_destroy(subpool);
if (kind == svn_node_dir)
SVN_ERR(relegate_external(path,
ctx->cancel_func,
ctx->cancel_baton,
pool));
else {
const char *parent = svn_path_dirname(path, pool);
SVN_ERR(svn_io_make_dir_recursively(parent, pool));
}
SVN_ERR(svn_client__checkout_internal(NULL, url, path, peg_revision,
revision,
SVN_DEPTH_INFINITY_OR_FILES(TRUE),
FALSE, FALSE, timestamp_sleep,
ctx, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
uri_scheme(const char **scheme, const char *uri, apr_pool_t *pool) {
apr_size_t i;
for (i = 0; uri[i] && uri[i] != ':'; ++i)
if (uri[i] == '/')
goto error;
if (i > 0 && uri[i] == ':' && uri[i+1] == '/' && uri[i+2] == '/') {
*scheme = apr_pstrmemdup(pool, uri, i);
return SVN_NO_ERROR;
}
error:
return svn_error_createf(SVN_ERR_BAD_URL, 0,
_("URL '%s' does not begin with a scheme"),
uri);
}
static svn_error_t *
resolve_relative_external_url(svn_wc_external_item2_t *item,
const char *repos_root_url,
const char *parent_dir_url,
apr_pool_t *pool) {
const char *uncanonicalized_url = item->url;
const char *canonicalized_url;
apr_uri_t parent_dir_parsed_uri;
apr_status_t status;
canonicalized_url = svn_path_canonicalize(uncanonicalized_url, pool);
if (svn_path_is_url(canonicalized_url)) {
item->url = canonicalized_url;
return SVN_NO_ERROR;
}
status = apr_uri_parse(pool, parent_dir_url, &parent_dir_parsed_uri);
if (status)
return svn_error_createf(SVN_ERR_BAD_URL, 0,
_("Illegal parent directory URL '%s'."),
parent_dir_url);
if ((0 == strncmp("../", uncanonicalized_url, 3)) ||
(0 == strncmp("^/", uncanonicalized_url, 2))) {
apr_array_header_t *base_components;
apr_array_header_t *relative_components;
int i;
if (0 == strncmp("../", uncanonicalized_url, 3)) {
base_components = svn_path_decompose(parent_dir_parsed_uri.path,
pool);
relative_components = svn_path_decompose(canonicalized_url, pool);
} else {
apr_uri_t repos_root_parsed_uri;
status = apr_uri_parse(pool, repos_root_url, &repos_root_parsed_uri);
if (status)
return svn_error_createf(SVN_ERR_BAD_URL, 0,
_("Illegal repository root URL '%s'."),
repos_root_url);
base_components = svn_path_decompose(repos_root_parsed_uri.path,
pool);
relative_components = svn_path_decompose(canonicalized_url + 2,
pool);
}
for (i = 0; i < relative_components->nelts; ++i) {
const char *component = APR_ARRAY_IDX(relative_components,
i,
const char *);
if (0 == strcmp("..", component)) {
if (base_components->nelts > 1)
apr_array_pop(base_components);
} else
APR_ARRAY_PUSH(base_components, const char *) = component;
}
parent_dir_parsed_uri.path = (char *)svn_path_compose(base_components,
pool);
parent_dir_parsed_uri.query = NULL;
parent_dir_parsed_uri.fragment = NULL;
item->url = apr_uri_unparse(pool, &parent_dir_parsed_uri, 0);
return SVN_NO_ERROR;
}
if (svn_path_is_backpath_present(canonicalized_url + 2))
return svn_error_createf(SVN_ERR_BAD_URL, 0,
_("The external relative URL '%s' cannot have "
"backpaths, i.e. '..'."),
uncanonicalized_url);
if (0 == strncmp("//", uncanonicalized_url, 2)) {
const char *scheme;
SVN_ERR(uri_scheme(&scheme, repos_root_url, pool));
item->url = svn_path_canonicalize(apr_pstrcat(pool,
scheme,
":",
uncanonicalized_url,
NULL),
pool);
return SVN_NO_ERROR;
}
if (uncanonicalized_url[0] == '/') {
parent_dir_parsed_uri.path = (char *)canonicalized_url;
parent_dir_parsed_uri.query = NULL;
parent_dir_parsed_uri.fragment = NULL;
item->url = apr_uri_unparse(pool, &parent_dir_parsed_uri, 0);
return SVN_NO_ERROR;
}
return svn_error_createf(SVN_ERR_BAD_URL, 0,
_("Unrecognized format for the relative external "
"URL '%s'."),
uncanonicalized_url);
}
static svn_error_t *
handle_external_item_change(const void *key, apr_ssize_t klen,
enum svn_hash_diff_key_status status,
void *baton) {
struct handle_external_item_change_baton *ib = baton;
svn_wc_external_item2_t *old_item, *new_item;
const char *parent;
const char *path = svn_path_join(ib->parent_dir,
(const char *) key, ib->pool);
if ((ib->old_desc) && (! ib->is_export)) {
old_item = apr_hash_get(ib->old_desc, key, klen);
if (old_item)
SVN_ERR(resolve_relative_external_url(old_item, ib->repos_root_url,
ib->parent_dir_url, ib->pool));
} else
old_item = NULL;
if (ib->new_desc) {
new_item = apr_hash_get(ib->new_desc, key, klen);
if (new_item)
SVN_ERR(resolve_relative_external_url(new_item, ib->repos_root_url,
ib->parent_dir_url, ib->pool));
} else
new_item = NULL;
assert(old_item || new_item);
if (! old_item) {
svn_path_split(path, &parent, NULL, ib->pool);
SVN_ERR(svn_io_make_dir_recursively(parent, ib->pool));
if (ib->ctx->notify_func2)
(*ib->ctx->notify_func2)
(ib->ctx->notify_baton2,
svn_wc_create_notify(path, svn_wc_notify_update_external,
ib->pool), ib->pool);
if (ib->is_export)
SVN_ERR(svn_client_export4(NULL, new_item->url, path,
&(new_item->peg_revision),
&(new_item->revision),
TRUE, FALSE, svn_depth_infinity, NULL,
ib->ctx, ib->pool));
else
SVN_ERR(svn_client__checkout_internal
(NULL, new_item->url, path,
&(new_item->peg_revision), &(new_item->revision),
SVN_DEPTH_INFINITY_OR_FILES(TRUE),
FALSE, FALSE, ib->timestamp_sleep, ib->ctx, ib->pool));
} else if (! new_item) {
svn_error_t *err, *err2;
svn_wc_adm_access_t *adm_access;
SVN_ERR(svn_wc_adm_open3(&adm_access, NULL, path, TRUE, -1,
ib->ctx->cancel_func, ib->ctx->cancel_baton,
ib->pool));
err = svn_wc_remove_from_revision_control
(adm_access, SVN_WC_ENTRY_THIS_DIR, TRUE, FALSE,
ib->ctx->cancel_func, ib->ctx->cancel_baton, ib->pool);
if (!err || err->apr_err == SVN_ERR_WC_LEFT_LOCAL_MOD)
if ((err2 = svn_wc_adm_close(adm_access))) {
if (!err)
err = err2;
else
svn_error_clear(err2);
}
if (err && (err->apr_err != SVN_ERR_WC_LEFT_LOCAL_MOD))
return err;
svn_error_clear(err);
} else if (! compare_external_items(new_item, old_item)
|| ib->update_unchanged) {
SVN_ERR(switch_external(path, new_item->url, &(new_item->revision),
&(new_item->peg_revision),
ib->timestamp_sleep, ib->ctx, ib->pool));
}
svn_pool_clear(ib->pool);
return SVN_NO_ERROR;
}
struct handle_externals_desc_change_baton {
apr_hash_t *externals_new;
apr_hash_t *externals_old;
svn_depth_t requested_depth;
apr_hash_t *ambient_depths;
const char *from_url;
const char *to_path;
svn_client_ctx_t *ctx;
const char *repos_root_url;
svn_boolean_t update_unchanged;
svn_boolean_t *timestamp_sleep;
svn_boolean_t is_export;
apr_pool_t *pool;
};
static svn_error_t *
handle_externals_desc_change(const void *key, apr_ssize_t klen,
enum svn_hash_diff_key_status status,
void *baton) {
struct handle_externals_desc_change_baton *cb = baton;
struct handle_external_item_change_baton ib;
const char *old_desc_text, *new_desc_text;
apr_array_header_t *old_desc, *new_desc;
apr_hash_t *old_desc_hash, *new_desc_hash;
apr_size_t len;
int i;
svn_wc_external_item2_t *item;
const char *ambient_depth_w;
svn_depth_t ambient_depth;
if (cb->ambient_depths) {
ambient_depth_w = apr_hash_get(cb->ambient_depths, key, klen);
if (ambient_depth_w == NULL) {
return svn_error_createf
(SVN_ERR_WC_CORRUPT, NULL,
_("Traversal of '%s' found no ambient depth"),
(const char *) key);
} else {
ambient_depth = svn_depth_from_word(ambient_depth_w);
}
} else {
ambient_depth = svn_depth_infinity;
}
if ((cb->requested_depth < svn_depth_infinity
&& cb->requested_depth != svn_depth_unknown)
|| (ambient_depth < svn_depth_infinity
&& cb->requested_depth < svn_depth_infinity))
return SVN_NO_ERROR;
if ((old_desc_text = apr_hash_get(cb->externals_old, key, klen)))
SVN_ERR(svn_wc_parse_externals_description3(&old_desc, key, old_desc_text,
FALSE, cb->pool));
else
old_desc = NULL;
if ((new_desc_text = apr_hash_get(cb->externals_new, key, klen)))
SVN_ERR(svn_wc_parse_externals_description3(&new_desc, key, new_desc_text,
FALSE, cb->pool));
else
new_desc = NULL;
old_desc_hash = apr_hash_make(cb->pool);
new_desc_hash = apr_hash_make(cb->pool);
for (i = 0; old_desc && (i < old_desc->nelts); i++) {
item = APR_ARRAY_IDX(old_desc, i, svn_wc_external_item2_t *);
apr_hash_set(old_desc_hash, item->target_dir,
APR_HASH_KEY_STRING, item);
}
for (i = 0; new_desc && (i < new_desc->nelts); i++) {
item = APR_ARRAY_IDX(new_desc, i, svn_wc_external_item2_t *);
apr_hash_set(new_desc_hash, item->target_dir,
APR_HASH_KEY_STRING, item);
}
ib.old_desc = old_desc_hash;
ib.new_desc = new_desc_hash;
ib.parent_dir = (const char *) key;
ib.repos_root_url = cb->repos_root_url;
ib.ctx = cb->ctx;
ib.update_unchanged = cb->update_unchanged;
ib.is_export = cb->is_export;
ib.timestamp_sleep = cb->timestamp_sleep;
ib.pool = svn_pool_create(cb->pool);
len = strlen(cb->to_path);
if (ib.parent_dir[len] == '/')
++len;
ib.parent_dir_url = svn_path_join(cb->from_url,
ib.parent_dir + len,
cb->pool);
for (i = 0; old_desc && (i < old_desc->nelts); i++) {
item = APR_ARRAY_IDX(old_desc, i, svn_wc_external_item2_t *);
if (apr_hash_get(new_desc_hash, item->target_dir, APR_HASH_KEY_STRING))
SVN_ERR(handle_external_item_change(item->target_dir,
APR_HASH_KEY_STRING,
svn_hash_diff_key_both, &ib));
else
SVN_ERR(handle_external_item_change(item->target_dir,
APR_HASH_KEY_STRING,
svn_hash_diff_key_a, &ib));
}
for (i = 0; new_desc && (i < new_desc->nelts); i++) {
item = APR_ARRAY_IDX(new_desc, i, svn_wc_external_item2_t *);
if (! apr_hash_get(old_desc_hash, item->target_dir, APR_HASH_KEY_STRING))
SVN_ERR(handle_external_item_change(item->target_dir,
APR_HASH_KEY_STRING,
svn_hash_diff_key_b, &ib));
}
svn_pool_destroy(ib.pool);
return SVN_NO_ERROR;
}
svn_error_t *
svn_client__handle_externals(svn_wc_traversal_info_t *traversal_info,
const char *from_url,
const char *to_path,
const char *repos_root_url,
svn_depth_t requested_depth,
svn_boolean_t update_unchanged,
svn_boolean_t *timestamp_sleep,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
apr_hash_t *externals_old, *externals_new, *ambient_depths;
struct handle_externals_desc_change_baton cb;
svn_wc_edited_externals(&externals_old, &externals_new, traversal_info);
svn_wc_traversed_depths(&ambient_depths, traversal_info);
cb.externals_new = externals_new;
cb.externals_old = externals_old;
cb.requested_depth = requested_depth;
cb.ambient_depths = ambient_depths;
cb.from_url = from_url;
cb.to_path = to_path;
cb.repos_root_url = repos_root_url;
cb.ctx = ctx;
cb.update_unchanged = update_unchanged;
cb.timestamp_sleep = timestamp_sleep;
cb.is_export = FALSE;
cb.pool = pool;
SVN_ERR(svn_hash_diff(cb.externals_old, cb.externals_new,
handle_externals_desc_change, &cb, pool));
return SVN_NO_ERROR;
}
svn_error_t *
svn_client__fetch_externals(apr_hash_t *externals,
const char *from_url,
const char *to_path,
const char *repos_root_url,
svn_depth_t requested_depth,
svn_boolean_t is_export,
svn_boolean_t *timestamp_sleep,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
struct handle_externals_desc_change_baton cb;
cb.externals_new = externals;
cb.externals_old = apr_hash_make(pool);
cb.requested_depth = requested_depth;
cb.ambient_depths = NULL;
cb.ctx = ctx;
cb.from_url = from_url;
cb.to_path = to_path;
cb.repos_root_url = repos_root_url;
cb.update_unchanged = TRUE;
cb.timestamp_sleep = timestamp_sleep;
cb.is_export = is_export;
cb.pool = pool;
SVN_ERR(svn_hash_diff(cb.externals_old, cb.externals_new,
handle_externals_desc_change, &cb, pool));
return SVN_NO_ERROR;
}
svn_error_t *
svn_client__do_external_status(svn_wc_traversal_info_t *traversal_info,
svn_wc_status_func2_t status_func,
void *status_baton,
svn_depth_t depth,
svn_boolean_t get_all,
svn_boolean_t update,
svn_boolean_t no_ignore,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
apr_hash_t *externals_old, *externals_new;
apr_hash_index_t *hi;
apr_pool_t *subpool = svn_pool_create(pool);
svn_wc_edited_externals(&externals_old, &externals_new, traversal_info);
for (hi = apr_hash_first(pool, externals_new);
hi;
hi = apr_hash_next(hi)) {
apr_array_header_t *exts;
const void *key;
void *val;
const char *path;
const char *propval;
apr_pool_t *iterpool;
int i;
svn_pool_clear(subpool);
apr_hash_this(hi, &key, NULL, &val);
path = key;
propval = val;
SVN_ERR(svn_wc_parse_externals_description3(&exts, path, propval,
FALSE, subpool));
iterpool = svn_pool_create(subpool);
for (i = 0; exts && (i < exts->nelts); i++) {
const char *fullpath;
svn_wc_external_item2_t *external;
svn_node_kind_t kind;
svn_pool_clear(iterpool);
external = APR_ARRAY_IDX(exts, i, svn_wc_external_item2_t *);
fullpath = svn_path_join(path, external->target_dir, iterpool);
SVN_ERR(svn_io_check_path(fullpath, &kind, iterpool));
if (kind != svn_node_dir)
continue;
if (ctx->notify_func2)
(ctx->notify_func2)
(ctx->notify_baton2,
svn_wc_create_notify(fullpath, svn_wc_notify_status_external,
iterpool), iterpool);
SVN_ERR(svn_client_status3(NULL, fullpath,
&(external->revision),
status_func, status_baton,
depth, get_all, update,
no_ignore, FALSE, NULL, ctx, iterpool));
}
}
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}