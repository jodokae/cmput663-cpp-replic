#include "svn_client.h"
#include "svn_wc.h"
#include "svn_pools.h"
#include "svn_path.h"
#include "svn_hash.h"
#include "client.h"
#include "private/svn_wc_private.h"
struct set_cl_fe_baton {
svn_wc_adm_access_t *adm_access;
const char *changelist;
apr_hash_t *changelist_hash;
svn_client_ctx_t *ctx;
apr_pool_t *pool;
};
static svn_error_t *
set_entry_changelist(const char *path,
const svn_wc_entry_t *entry,
void *baton,
apr_pool_t *pool) {
struct set_cl_fe_baton *b = (struct set_cl_fe_baton *)baton;
svn_wc_adm_access_t *adm_access;
if (! SVN_WC__CL_MATCH(b->changelist_hash, entry))
return SVN_NO_ERROR;
if (entry->kind != svn_node_file) {
if ((strcmp(SVN_WC_ENTRY_THIS_DIR, entry->name) == 0)
&& (b->ctx->notify_func2))
b->ctx->notify_func2(b->ctx->notify_baton2,
svn_wc_create_notify(path,
svn_wc_notify_skip,
pool),
pool);
return SVN_NO_ERROR;
}
SVN_ERR(svn_wc_adm_retrieve(&adm_access, b->adm_access,
svn_path_dirname(path, pool), pool));
return svn_wc_set_changelist(path, b->changelist, adm_access,
b->ctx->cancel_func, b->ctx->cancel_baton,
b->ctx->notify_func2, b->ctx->notify_baton2,
pool);
}
static const svn_wc_entry_callbacks2_t set_cl_entry_callbacks =
{ set_entry_changelist, svn_client__default_walker_error_handler };
svn_error_t *
svn_client_add_to_changelist(const apr_array_header_t *paths,
const char *changelist,
svn_depth_t depth,
const apr_array_header_t *changelists,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
apr_pool_t *subpool = svn_pool_create(pool);
apr_hash_t *changelist_hash = NULL;
int i;
if (changelists && changelists->nelts)
SVN_ERR(svn_hash_from_cstring_keys(&changelist_hash, changelists, pool));
for (i = 0; i < paths->nelts; i++) {
struct set_cl_fe_baton seb;
svn_wc_adm_access_t *adm_access;
const char *path = APR_ARRAY_IDX(paths, i, const char *);
svn_pool_clear(subpool);
SVN_ERR(svn_wc_adm_probe_open3(&adm_access, NULL, path,
TRUE, -1,
ctx->cancel_func, ctx->cancel_baton,
subpool));
seb.adm_access = adm_access;
seb.changelist = changelist;
seb.changelist_hash = changelist_hash;
seb.ctx = ctx;
seb.pool = subpool;
SVN_ERR(svn_wc_walk_entries3(path, adm_access,
&set_cl_entry_callbacks, &seb,
depth, FALSE,
ctx->cancel_func, ctx->cancel_baton,
subpool));
SVN_ERR(svn_wc_adm_close(adm_access));
}
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
svn_error_t *
svn_client_remove_from_changelists(const apr_array_header_t *paths,
svn_depth_t depth,
const apr_array_header_t *changelists,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
apr_pool_t *subpool = svn_pool_create(pool);
apr_hash_t *changelist_hash = NULL;
int i;
if (changelists && changelists->nelts)
SVN_ERR(svn_hash_from_cstring_keys(&changelist_hash, changelists, pool));
for (i = 0; i < paths->nelts; i++) {
struct set_cl_fe_baton seb;
svn_wc_adm_access_t *adm_access;
const char *path = APR_ARRAY_IDX(paths, i, const char *);
svn_pool_clear(subpool);
SVN_ERR(svn_wc_adm_probe_open3(&adm_access, NULL, path,
TRUE, -1,
ctx->cancel_func, ctx->cancel_baton,
subpool));
seb.adm_access = adm_access;
seb.changelist = NULL;
seb.changelist_hash = changelist_hash;
seb.ctx = ctx;
seb.pool = subpool;
SVN_ERR(svn_wc_walk_entries3(path, adm_access,
&set_cl_entry_callbacks, &seb,
depth, FALSE,
ctx->cancel_func, ctx->cancel_baton,
subpool));
SVN_ERR(svn_wc_adm_close(adm_access));
}
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
struct get_cl_fe_baton {
svn_changelist_receiver_t callback_func;
void *callback_baton;
apr_hash_t *changelists;
apr_pool_t *pool;
};
static svn_error_t *
get_entry_changelist(const char *path,
const svn_wc_entry_t *entry,
void *baton,
apr_pool_t *pool) {
struct get_cl_fe_baton *b = (struct get_cl_fe_baton *)baton;
if (SVN_WC__CL_MATCH(b->changelists, entry)
&& ((entry->kind == svn_node_file)
|| ((entry->kind == svn_node_dir)
&& (strcmp(entry->name, SVN_WC_ENTRY_THIS_DIR) == 0)))) {
SVN_ERR(b->callback_func(b->callback_baton, path,
entry->changelist, pool));
}
return SVN_NO_ERROR;
}
static const svn_wc_entry_callbacks2_t get_cl_entry_callbacks =
{ get_entry_changelist, svn_client__default_walker_error_handler };
svn_error_t *
svn_client_get_changelists(const char *path,
const apr_array_header_t *changelists,
svn_depth_t depth,
svn_changelist_receiver_t callback_func,
void *callback_baton,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
struct get_cl_fe_baton geb;
svn_wc_adm_access_t *adm_access;
geb.callback_func = callback_func;
geb.callback_baton = callback_baton;
geb.pool = pool;
if (changelists)
SVN_ERR(svn_hash_from_cstring_keys(&(geb.changelists), changelists, pool));
else
geb.changelists = NULL;
SVN_ERR(svn_wc_adm_probe_open3(&adm_access, NULL, path,
FALSE,
-1,
ctx->cancel_func, ctx->cancel_baton, pool));
SVN_ERR(svn_wc_walk_entries3(path, adm_access, &get_cl_entry_callbacks, &geb,
depth, FALSE,
ctx->cancel_func, ctx->cancel_baton, pool));
SVN_ERR(svn_wc_adm_close(adm_access));
return SVN_NO_ERROR;
}
