#include <assert.h>
#include <apr_pools.h>
#include <apr_strings.h>
#include "svn_pools.h"
#include "svn_string.h"
#include "svn_error.h"
#include "svn_types.h"
#include "svn_opt.h"
#include "svn_props.h"
#include "svn_path.h"
#include "svn_wc.h"
#include "svn_client.h"
#include "private/svn_wc_private.h"
#include "client.h"
#include "svn_private_config.h"
static apr_hash_t *
string_hash_dup(apr_hash_t *hash, apr_pool_t *pool) {
apr_hash_index_t *hi;
const void *key;
apr_ssize_t klen;
void *val;
apr_hash_t *new_hash = apr_hash_make(pool);
for (hi = apr_hash_first(pool, hash); hi; hi = apr_hash_next(hi)) {
apr_hash_this(hi, &key, &klen, &val);
key = apr_pstrdup(pool, key);
val = svn_string_dup(val, pool);
apr_hash_set(new_hash, key, klen, val);
}
return new_hash;
}
svn_error_t *
svn_client_commit_item_create(const svn_client_commit_item3_t **item,
apr_pool_t *pool) {
*item = apr_pcalloc(pool, sizeof(svn_client_commit_item3_t));
return SVN_NO_ERROR;
}
svn_client_commit_item3_t *
svn_client_commit_item3_dup(const svn_client_commit_item3_t *item,
apr_pool_t *pool) {
svn_client_commit_item3_t *new_item = apr_palloc(pool, sizeof(*new_item));
*new_item = *item;
if (new_item->path)
new_item->path = apr_pstrdup(pool, new_item->path);
if (new_item->url)
new_item->url = apr_pstrdup(pool, new_item->url);
if (new_item->copyfrom_url)
new_item->copyfrom_url = apr_pstrdup(pool, new_item->copyfrom_url);
if (new_item->incoming_prop_changes)
new_item->incoming_prop_changes =
svn_prop_array_dup(new_item->incoming_prop_changes, pool);
if (new_item->outgoing_prop_changes)
new_item->outgoing_prop_changes =
svn_prop_array_dup(new_item->outgoing_prop_changes, pool);
return new_item;
}
svn_client_commit_item2_t *
svn_client_commit_item2_dup(const svn_client_commit_item2_t *item,
apr_pool_t *pool) {
svn_client_commit_item2_t *new_item = apr_palloc(pool, sizeof(*new_item));
*new_item = *item;
if (new_item->path)
new_item->path = apr_pstrdup(pool, new_item->path);
if (new_item->url)
new_item->url = apr_pstrdup(pool, new_item->url);
if (new_item->copyfrom_url)
new_item->copyfrom_url = apr_pstrdup(pool, new_item->copyfrom_url);
if (new_item->wcprop_changes)
new_item->wcprop_changes = svn_prop_array_dup(new_item->wcprop_changes,
pool);
return new_item;
}
svn_client_proplist_item_t *
svn_client_proplist_item_dup(const svn_client_proplist_item_t *item,
apr_pool_t * pool) {
svn_client_proplist_item_t *new_item = apr_pcalloc(pool, sizeof(*new_item));
if (item->node_name)
new_item->node_name = svn_stringbuf_dup(item->node_name, pool);
if (item->prop_hash)
new_item->prop_hash = string_hash_dup(item->prop_hash, pool);
return new_item;
}
static svn_error_t *
wc_path_to_repos_urls(const char **url, const char **repos_root,
svn_boolean_t *need_wc_cleanup,
svn_wc_adm_access_t **adm_access, const char *wc_path,
apr_pool_t *pool) {
const svn_wc_entry_t *entry;
if (! *adm_access) {
SVN_ERR(svn_wc_adm_probe_open3(adm_access, NULL, wc_path,
FALSE, 0, NULL, NULL, pool));
*need_wc_cleanup = TRUE;
}
SVN_ERR(svn_wc__entry_versioned(&entry, wc_path, *adm_access, FALSE, pool));
SVN_ERR(svn_client__entry_location(url, NULL, wc_path,
svn_opt_revision_unspecified, entry,
pool));
if (*repos_root == NULL)
*repos_root = apr_pstrdup(pool, entry->repos);
return SVN_NO_ERROR;
}
svn_error_t *
svn_client__path_relative_to_root(const char **rel_path,
const char *path_or_url,
const char *repos_root,
svn_boolean_t include_leading_slash,
svn_ra_session_t *ra_session,
svn_wc_adm_access_t *adm_access,
apr_pool_t *pool) {
svn_error_t *err = SVN_NO_ERROR;
svn_boolean_t need_wc_cleanup = FALSE;
assert(repos_root != NULL || ra_session != NULL);
if (! svn_path_is_url(path_or_url)) {
err = wc_path_to_repos_urls(&path_or_url, &repos_root, &need_wc_cleanup,
&adm_access, path_or_url, pool);
if (err)
goto cleanup;
}
if (repos_root == NULL) {
if ((err = svn_ra_get_repos_root2(ra_session, &repos_root, pool)))
goto cleanup;
}
if (strcmp(repos_root, path_or_url) == 0) {
*rel_path = include_leading_slash ? "/" : "";
} else {
const char *rel_url = svn_path_is_child(repos_root, path_or_url, pool);
if (! rel_url) {
err = svn_error_createf(SVN_ERR_CLIENT_UNRELATED_RESOURCES, NULL,
_("URL '%s' is not a child of repository "
"root URL '%s'"),
path_or_url, repos_root);
goto cleanup;
}
rel_url = svn_path_uri_decode(rel_url, pool);
*rel_path = include_leading_slash
? apr_pstrcat(pool, "/", rel_url, NULL) : rel_url;
}
cleanup:
if (need_wc_cleanup) {
svn_error_t *err2 = svn_wc_adm_close(adm_access);
if (! err)
err = err2;
else
svn_error_clear(err2);
}
return err;
}
svn_error_t *
svn_client__get_repos_root(const char **repos_root,
const char *path_or_url,
const svn_opt_revision_t *peg_revision,
svn_wc_adm_access_t *adm_access,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
svn_revnum_t rev;
const char *target_url;
svn_boolean_t need_wc_cleanup = FALSE;
svn_error_t *err = SVN_NO_ERROR;
apr_pool_t *sesspool = NULL;
if (!svn_path_is_url(path_or_url)
&& (peg_revision->kind == svn_opt_revision_working
|| peg_revision->kind == svn_opt_revision_base)) {
*repos_root = NULL;
err = wc_path_to_repos_urls(&path_or_url, repos_root, &need_wc_cleanup,
&adm_access, path_or_url, pool);
if (err)
goto cleanup;
} else {
*repos_root = NULL;
}
if (*repos_root == NULL) {
svn_ra_session_t *ra_session;
sesspool = svn_pool_create(pool);
if ((err = svn_client__ra_session_from_path(&ra_session,
&rev,
&target_url,
path_or_url,
NULL,
peg_revision,
peg_revision,
ctx,
sesspool)))
goto cleanup;
if ((err = svn_ra_get_repos_root2(ra_session, repos_root, pool)))
goto cleanup;
}
cleanup:
if (sesspool)
svn_pool_destroy(sesspool);
if (need_wc_cleanup) {
svn_error_t *err2 = svn_wc_adm_close(adm_access);
if (! err)
err = err2;
else
svn_error_clear(err2);
}
return err;
}
svn_error_t *
svn_client__default_walker_error_handler(const char *path,
svn_error_t *err,
void *walk_baton,
apr_pool_t *pool) {
return err;
}
