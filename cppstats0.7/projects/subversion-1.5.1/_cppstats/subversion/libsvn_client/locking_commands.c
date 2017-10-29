#include "svn_client.h"
#include "client.h"
#include "svn_path.h"
#include "svn_xml.h"
#include "svn_pools.h"
#include "svn_private_config.h"
#include "private/svn_wc_private.h"
struct lock_baton {
svn_wc_adm_access_t *adm_access;
apr_hash_t *urls_to_paths;
svn_client_ctx_t *ctx;
apr_pool_t *pool;
};
static svn_error_t *
store_locks_callback(void *baton,
const char *rel_url,
svn_boolean_t do_lock,
const svn_lock_t *lock,
svn_error_t *ra_err, apr_pool_t *pool) {
struct lock_baton *lb = baton;
svn_wc_adm_access_t *adm_access;
const char *abs_path;
svn_wc_notify_t *notify;
notify = svn_wc_create_notify(rel_url,
do_lock
? (ra_err
? svn_wc_notify_failed_lock
: svn_wc_notify_locked)
: (ra_err
? svn_wc_notify_failed_unlock
: svn_wc_notify_unlocked),
pool);
notify->lock = lock;
notify->err = ra_err;
if (lb->adm_access) {
char *path = apr_hash_get(lb->urls_to_paths, rel_url,
APR_HASH_KEY_STRING);
abs_path = svn_path_join(svn_wc_adm_access_path(lb->adm_access),
path, lb->pool);
SVN_ERR(svn_wc_adm_probe_retrieve(&adm_access, lb->adm_access,
abs_path, lb->pool));
if (do_lock) {
if (!ra_err) {
SVN_ERR(svn_wc_add_lock(abs_path, lock, adm_access, lb->pool));
notify->lock_state = svn_wc_notify_lock_state_locked;
} else
notify->lock_state = svn_wc_notify_lock_state_unchanged;
} else {
if (!ra_err ||
(ra_err && (ra_err->apr_err != SVN_ERR_FS_LOCK_OWNER_MISMATCH))) {
SVN_ERR(svn_wc_remove_lock(abs_path, adm_access, lb->pool));
notify->lock_state = svn_wc_notify_lock_state_unlocked;
} else
notify->lock_state = svn_wc_notify_lock_state_unchanged;
}
}
if (lb->ctx->notify_func2)
lb->ctx->notify_func2(lb->ctx->notify_baton2, notify, pool);
return SVN_NO_ERROR;
}
static svn_error_t *
organize_lock_targets(const char **common_parent,
svn_wc_adm_access_t **parent_adm_access_p,
apr_hash_t **rel_targets_p,
apr_hash_t **rel_fs_paths_p,
const apr_array_header_t *targets,
svn_boolean_t do_lock,
svn_boolean_t force,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
int i;
apr_array_header_t *rel_targets = apr_array_make(pool, 1,
sizeof(const char *));
apr_hash_t *rel_targets_ret = apr_hash_make(pool);
apr_pool_t *subpool = svn_pool_create(pool);
SVN_ERR(svn_path_condense_targets(common_parent, &rel_targets, targets,
FALSE, pool));
if (apr_is_empty_array(rel_targets)) {
char *base_name = svn_path_basename(*common_parent, pool);
*common_parent = svn_path_dirname(*common_parent, pool);
APR_ARRAY_PUSH(rel_targets, char *) = base_name;
}
if (*common_parent == NULL || (*common_parent)[0] == '\0')
return svn_error_create
(SVN_ERR_UNSUPPORTED_FEATURE, NULL,
_("No common parent found, unable to operate on disjoint arguments"));
if (svn_path_is_url(*common_parent)) {
svn_revnum_t *invalid_revnum;
invalid_revnum = apr_palloc(pool, sizeof(*invalid_revnum));
*invalid_revnum = SVN_INVALID_REVNUM;
*parent_adm_access_p = NULL;
for (i = 0; i < rel_targets->nelts; i++) {
const char *target = APR_ARRAY_IDX(rel_targets, i, const char *);
apr_hash_set(rel_targets_ret, svn_path_uri_decode(target, pool),
APR_HASH_KEY_STRING,
do_lock ? (const void *) invalid_revnum
: (const void *) "");
}
*rel_fs_paths_p = NULL;
} else {
int max_levels_to_lock = 0;
apr_array_header_t *rel_urls;
apr_array_header_t *urls = apr_array_make(pool, 1,
sizeof(const char *));
apr_hash_t *urls_hash = apr_hash_make(pool);
const char *common_url;
for (i = 0; i < rel_targets->nelts; ++i) {
const char *target = APR_ARRAY_IDX(rel_targets, i, const char *);
int n = svn_path_component_count(target);
if (n > max_levels_to_lock)
max_levels_to_lock = n;
}
SVN_ERR(svn_wc_adm_probe_open3(parent_adm_access_p, NULL,
*common_parent,
TRUE, max_levels_to_lock,
ctx->cancel_func, ctx->cancel_baton,
pool));
for (i = 0; i < rel_targets->nelts; i++) {
const svn_wc_entry_t *entry;
const char *target = APR_ARRAY_IDX(rel_targets, i, const char *);
const char *abs_path;
svn_pool_clear(subpool);
abs_path = svn_path_join
(svn_wc_adm_access_path(*parent_adm_access_p), target, subpool);
SVN_ERR(svn_wc__entry_versioned(&entry, abs_path,
*parent_adm_access_p, FALSE, subpool));
if (! entry->url)
return svn_error_createf(SVN_ERR_ENTRY_MISSING_URL, NULL,
_("'%s' has no URL"),
svn_path_local_style(target, pool));
APR_ARRAY_PUSH(urls, const char *) = apr_pstrdup(pool,
entry->url);
}
SVN_ERR(svn_path_condense_targets(&common_url, &rel_urls, urls,
FALSE, pool));
if (apr_is_empty_array(rel_urls)) {
char *base_name = svn_path_basename(common_url, pool);
common_url = svn_path_dirname(common_url, pool);
APR_ARRAY_PUSH(rel_urls, char *) = base_name;
}
if (common_url == NULL || (common_url)[0] == '\0')
return svn_error_create
(SVN_ERR_UNSUPPORTED_FEATURE, NULL,
_("Unable to lock/unlock across multiple repositories"));
for (i = 0; i < rel_targets->nelts; i++) {
const svn_wc_entry_t *entry;
const char *target = APR_ARRAY_IDX(rel_targets, i, const char *);
const char *url = APR_ARRAY_IDX(rel_urls, i, const char *);
const char *abs_path;
const char *decoded_url = svn_path_uri_decode(url, pool);
svn_pool_clear(subpool);
apr_hash_set(urls_hash, decoded_url,
APR_HASH_KEY_STRING,
apr_pstrdup(pool, target));
abs_path = svn_path_join
(svn_wc_adm_access_path(*parent_adm_access_p), target, subpool);
SVN_ERR(svn_wc_entry(&entry, abs_path, *parent_adm_access_p, FALSE,
subpool));
if (do_lock) {
svn_revnum_t *revnum;
revnum = apr_palloc(pool, sizeof(* revnum));
*revnum = entry->revision;
apr_hash_set(rel_targets_ret, decoded_url,
APR_HASH_KEY_STRING, revnum);
} else {
if (! force) {
if (! entry->lock_token)
return svn_error_createf
(SVN_ERR_CLIENT_MISSING_LOCK_TOKEN, NULL,
_("'%s' is not locked in this working copy"), target);
apr_hash_set(rel_targets_ret, decoded_url,
APR_HASH_KEY_STRING,
apr_pstrdup(pool, entry->lock_token));
} else {
apr_hash_set(rel_targets_ret, decoded_url,
APR_HASH_KEY_STRING, "");
}
}
}
*rel_fs_paths_p = urls_hash;
*common_parent = common_url;
}
*rel_targets_p = rel_targets_ret;
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
static svn_error_t *
fetch_tokens(svn_ra_session_t *ra_session, apr_hash_t *path_tokens,
apr_pool_t *pool) {
apr_hash_index_t *hi;
apr_pool_t *iterpool = svn_pool_create(pool);
for (hi = apr_hash_first(pool, path_tokens); hi; hi = apr_hash_next(hi)) {
const void *key;
const char *path;
svn_lock_t *lock;
svn_pool_clear(iterpool);
apr_hash_this(hi, &key, NULL, NULL);
path = key;
SVN_ERR(svn_ra_get_lock(ra_session, &lock, path, iterpool));
if (! lock)
return svn_error_createf
(SVN_ERR_CLIENT_MISSING_LOCK_TOKEN, NULL,
_("'%s' is not locked"), path);
apr_hash_set(path_tokens, path, APR_HASH_KEY_STRING,
apr_pstrdup(pool, lock->token));
}
svn_pool_destroy(iterpool);
return SVN_NO_ERROR;
}
svn_error_t *
svn_client_lock(const apr_array_header_t *targets,
const char *comment,
svn_boolean_t steal_lock,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
svn_wc_adm_access_t *adm_access;
const char *common_parent;
svn_ra_session_t *ra_session;
apr_hash_t *path_revs, *urls_to_paths;
struct lock_baton cb;
if (apr_is_empty_array(targets))
return SVN_NO_ERROR;
if (comment) {
if (! svn_xml_is_xml_safe(comment, strlen(comment)))
return svn_error_create
(SVN_ERR_XML_UNESCAPABLE_DATA, NULL,
_("Lock comment contains illegal characters"));
}
SVN_ERR(organize_lock_targets(&common_parent, &adm_access,
&path_revs, &urls_to_paths, targets, TRUE,
steal_lock, ctx, pool));
SVN_ERR(svn_client__open_ra_session_internal
(&ra_session, common_parent,
adm_access ? svn_wc_adm_access_path(adm_access) : NULL,
adm_access, NULL, FALSE, FALSE, ctx, pool));
cb.pool = pool;
cb.adm_access = adm_access;
cb.urls_to_paths = urls_to_paths;
cb.ctx = ctx;
SVN_ERR(svn_ra_lock(ra_session, path_revs, comment,
steal_lock, store_locks_callback, &cb, pool));
if (adm_access)
SVN_ERR(svn_wc_adm_close(adm_access));
return SVN_NO_ERROR;
}
svn_error_t *
svn_client_unlock(const apr_array_header_t *targets,
svn_boolean_t break_lock,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
svn_wc_adm_access_t *adm_access;
const char *common_parent;
svn_ra_session_t *ra_session;
apr_hash_t *path_tokens, *urls_to_paths;
struct lock_baton cb;
if (apr_is_empty_array(targets))
return SVN_NO_ERROR;
SVN_ERR(organize_lock_targets(&common_parent, &adm_access,
&path_tokens, &urls_to_paths, targets,
FALSE, break_lock, ctx, pool));
SVN_ERR(svn_client__open_ra_session_internal
(&ra_session, common_parent,
adm_access ? svn_wc_adm_access_path(adm_access) : NULL,
adm_access, NULL, FALSE, FALSE, ctx, pool));
if (! adm_access && !break_lock)
SVN_ERR(fetch_tokens(ra_session, path_tokens, pool));
cb.pool = pool;
cb.adm_access = adm_access;
cb.urls_to_paths = urls_to_paths;
cb.ctx = ctx;
SVN_ERR(svn_ra_unlock(ra_session, path_tokens, break_lock,
store_locks_callback, &cb, pool));
if (adm_access)
SVN_ERR(svn_wc_adm_close(adm_access));
return SVN_NO_ERROR;
}