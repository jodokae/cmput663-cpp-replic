#include <apr_pools.h>
#include <assert.h>
#include "svn_error.h"
#include "svn_pools.h"
#include "svn_string.h"
#include "svn_sorts.h"
#include "svn_ra.h"
#include "svn_client.h"
#include "svn_path.h"
#include "svn_props.h"
#include "svn_mergeinfo.h"
#include "client.h"
#include "mergeinfo.h"
#include "svn_private_config.h"
#include "private/svn_wc_private.h"
static svn_error_t *
open_admin_tmp_file(apr_file_t **fp,
void *callback_baton,
apr_pool_t *pool) {
svn_client__callback_baton_t *cb = callback_baton;
SVN_ERR(svn_wc_create_tmp_file2(fp, NULL, cb->base_dir,
svn_io_file_del_on_close, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
open_tmp_file(apr_file_t **fp,
void *callback_baton,
apr_pool_t *pool) {
svn_client__callback_baton_t *cb = callback_baton;
const char *truepath;
if (cb->base_dir && ! cb->read_only_wc)
truepath = apr_pstrdup(pool, cb->base_dir);
else
SVN_ERR(svn_io_temp_dir(&truepath, pool));
truepath = svn_path_join(truepath, "tempfile", pool);
SVN_ERR(svn_io_open_unique_file2(fp, NULL, truepath, ".tmp",
svn_io_file_del_on_close, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
get_wc_prop(void *baton,
const char *relpath,
const char *name,
const svn_string_t **value,
apr_pool_t *pool) {
svn_client__callback_baton_t *cb = baton;
*value = NULL;
if (cb->commit_items) {
int i;
for (i = 0; i < cb->commit_items->nelts; i++) {
svn_client_commit_item3_t *item
= APR_ARRAY_IDX(cb->commit_items, i,
svn_client_commit_item3_t *);
if (! strcmp(relpath,
svn_path_uri_decode(item->url, pool)))
return svn_wc_prop_get(value, name, item->path, cb->base_access,
pool);
}
return SVN_NO_ERROR;
}
else if (cb->base_dir == NULL)
return SVN_NO_ERROR;
return svn_wc_prop_get(value, name,
svn_path_join(cb->base_dir, relpath, pool),
cb->base_access, pool);
}
static svn_error_t *
push_wc_prop(void *baton,
const char *relpath,
const char *name,
const svn_string_t *value,
apr_pool_t *pool) {
svn_client__callback_baton_t *cb = baton;
int i;
if (! cb->commit_items)
return svn_error_createf
(SVN_ERR_UNSUPPORTED_FEATURE, NULL,
_("Attempt to set wc property '%s' on '%s' in a non-commit operation"),
name, svn_path_local_style(relpath, pool));
for (i = 0; i < cb->commit_items->nelts; i++) {
svn_client_commit_item3_t *item
= APR_ARRAY_IDX(cb->commit_items, i, svn_client_commit_item3_t *);
if (strcmp(relpath, svn_path_uri_decode(item->url, pool)) == 0) {
apr_pool_t *cpool = item->incoming_prop_changes->pool;
svn_prop_t *prop = apr_palloc(cpool, sizeof(*prop));
prop->name = apr_pstrdup(cpool, name);
if (value) {
prop->value
= svn_string_ncreate(value->data, value->len, cpool);
} else
prop->value = NULL;
APR_ARRAY_PUSH(item->incoming_prop_changes, svn_prop_t *) = prop;
return SVN_NO_ERROR;
}
}
return SVN_NO_ERROR;
}
static svn_error_t *
set_wc_prop(void *baton,
const char *path,
const char *name,
const svn_string_t *value,
apr_pool_t *pool) {
svn_client__callback_baton_t *cb = baton;
svn_wc_adm_access_t *adm_access;
const svn_wc_entry_t *entry;
const char *full_path = svn_path_join(cb->base_dir, path, pool);
SVN_ERR(svn_wc__entry_versioned(&entry, full_path, cb->base_access, FALSE,
pool));
SVN_ERR(svn_wc_adm_retrieve(&adm_access, cb->base_access,
(entry->kind == svn_node_dir
? full_path
: svn_path_dirname(full_path, pool)),
pool));
return svn_wc_prop_set2(name, value, full_path, adm_access, TRUE, pool);
}
struct invalidate_wcprop_walk_baton {
const char *prop_name;
svn_wc_adm_access_t *base_access;
};
static svn_error_t *
invalidate_wcprop_for_entry(const char *path,
const svn_wc_entry_t *entry,
void *walk_baton,
apr_pool_t *pool) {
struct invalidate_wcprop_walk_baton *wb = walk_baton;
svn_wc_adm_access_t *entry_access;
SVN_ERR(svn_wc_adm_retrieve(&entry_access, wb->base_access,
((entry->kind == svn_node_dir)
? path
: svn_path_dirname(path, pool)),
pool));
return svn_wc_prop_set2(wb->prop_name, NULL, path, entry_access,
FALSE, pool);
}
static svn_error_t *
invalidate_wc_props(void *baton,
const char *path,
const char *prop_name,
apr_pool_t *pool) {
svn_client__callback_baton_t *cb = baton;
svn_wc_entry_callbacks2_t walk_callbacks = { invalidate_wcprop_for_entry,
svn_client__default_walker_error_handler
};
struct invalidate_wcprop_walk_baton wb;
svn_wc_adm_access_t *adm_access;
wb.base_access = cb->base_access;
wb.prop_name = prop_name;
path = svn_path_join(cb->base_dir, path, pool);
SVN_ERR(svn_wc_adm_probe_retrieve(&adm_access, cb->base_access, path,
pool));
SVN_ERR(svn_wc_walk_entries3(path, adm_access, &walk_callbacks, &wb,
svn_depth_infinity, FALSE,
cb->ctx->cancel_func, cb->ctx->cancel_baton,
pool));
return SVN_NO_ERROR;
}
static svn_error_t *
cancel_callback(void *baton) {
svn_client__callback_baton_t *b = baton;
return (b->ctx->cancel_func)(b->ctx->cancel_baton);
}
static svn_error_t *
get_client_string(void *baton,
const char **name,
apr_pool_t *pool) {
svn_client__callback_baton_t *b = baton;
*name = apr_pstrdup(pool, b->ctx->client_name);
return SVN_NO_ERROR;
}
svn_error_t *
svn_client__open_ra_session_internal(svn_ra_session_t **ra_session,
const char *base_url,
const char *base_dir,
svn_wc_adm_access_t *base_access,
apr_array_header_t *commit_items,
svn_boolean_t use_admin,
svn_boolean_t read_only_wc,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
svn_ra_callbacks2_t *cbtable = apr_pcalloc(pool, sizeof(*cbtable));
svn_client__callback_baton_t *cb = apr_pcalloc(pool, sizeof(*cb));
const char *uuid = NULL;
cbtable->open_tmp_file = use_admin ? open_admin_tmp_file : open_tmp_file;
cbtable->get_wc_prop = use_admin ? get_wc_prop : NULL;
cbtable->set_wc_prop = read_only_wc ? NULL : set_wc_prop;
cbtable->push_wc_prop = commit_items ? push_wc_prop : NULL;
cbtable->invalidate_wc_props = read_only_wc ? NULL : invalidate_wc_props;
cbtable->auth_baton = ctx->auth_baton;
cbtable->progress_func = ctx->progress_func;
cbtable->progress_baton = ctx->progress_baton;
cbtable->cancel_func = ctx->cancel_func ? cancel_callback : NULL;
cbtable->get_client_string = get_client_string;
cb->base_dir = base_dir;
cb->base_access = base_access;
cb->read_only_wc = read_only_wc;
cb->pool = pool;
cb->commit_items = commit_items;
cb->ctx = ctx;
if (base_access) {
const svn_wc_entry_t *entry;
SVN_ERR(svn_wc_entry(&entry, base_dir, base_access, FALSE, pool));
if (entry && entry->uuid)
uuid = entry->uuid;
}
SVN_ERR(svn_ra_open3(ra_session, base_url, uuid, cbtable, cb,
ctx->config, pool));
return SVN_NO_ERROR;
}
svn_error_t *
svn_client_open_ra_session(svn_ra_session_t **session,
const char *url,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
return svn_client__open_ra_session_internal(session, url, NULL, NULL, NULL,
FALSE, TRUE, ctx, pool);
}
svn_error_t *
svn_client_uuid_from_url(const char **uuid,
const char *url,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
svn_ra_session_t *ra_session;
apr_pool_t *subpool = svn_pool_create(pool);
SVN_ERR(svn_client__open_ra_session_internal(&ra_session, url,
NULL,
NULL, NULL, FALSE, TRUE,
ctx, subpool));
SVN_ERR(svn_ra_get_uuid2(ra_session, uuid, pool));
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
svn_error_t *
svn_client_uuid_from_path(const char **uuid,
const char *path,
svn_wc_adm_access_t *adm_access,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
const svn_wc_entry_t *entry;
SVN_ERR(svn_wc__entry_versioned(&entry, path, adm_access,
TRUE, pool));
if (entry->uuid) {
*uuid = entry->uuid;
} else if (entry->url) {
SVN_ERR(svn_client_uuid_from_url(uuid, entry->url, ctx, pool));
} else {
svn_boolean_t is_root;
SVN_ERR(svn_wc_is_wc_root(&is_root, path, adm_access, pool));
if (is_root)
return svn_error_createf(SVN_ERR_ENTRY_MISSING_URL, NULL,
_("'%s' has no URL"),
svn_path_local_style(path, pool));
else
return svn_client_uuid_from_path(uuid, svn_path_dirname(path, pool),
adm_access, ctx, pool);
}
return SVN_NO_ERROR;
}
svn_error_t *
svn_client__ra_session_from_path(svn_ra_session_t **ra_session_p,
svn_revnum_t *rev_p,
const char **url_p,
const char *path_or_url,
svn_wc_adm_access_t *base_access,
const svn_opt_revision_t *peg_revision_p,
const svn_opt_revision_t *revision,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
svn_ra_session_t *ra_session;
const char *initial_url, *url, *base_dir = NULL;
svn_opt_revision_t *good_rev;
svn_opt_revision_t peg_revision, start_rev;
svn_opt_revision_t dead_end_rev;
svn_opt_revision_t *ignored_rev, *new_rev;
svn_revnum_t rev;
const char *ignored_url;
SVN_ERR(svn_client_url_from_path(&initial_url, path_or_url, pool));
if (! initial_url)
return svn_error_createf(SVN_ERR_ENTRY_MISSING_URL, NULL,
_("'%s' has no URL"), path_or_url);
start_rev = *revision;
peg_revision = *peg_revision_p;
SVN_ERR(svn_opt_resolve_revisions(&peg_revision, &start_rev,
svn_path_is_url(path_or_url),
TRUE,
pool));
if (base_access)
base_dir = svn_wc_adm_access_path(base_access);
SVN_ERR(svn_client__open_ra_session_internal(&ra_session, initial_url,
base_dir, base_access, NULL,
base_access ? TRUE : FALSE,
FALSE, ctx, pool));
dead_end_rev.kind = svn_opt_revision_unspecified;
SVN_ERR(svn_client__repos_locations(&url, &new_rev,
&ignored_url, &ignored_rev,
ra_session,
path_or_url, &peg_revision,
&start_rev, &dead_end_rev,
ctx, pool));
good_rev = (svn_opt_revision_t *)new_rev;
SVN_ERR(svn_ra_reparent(ra_session, url, pool));
if (good_rev->kind == svn_opt_revision_unspecified)
good_rev->kind = svn_opt_revision_head;
SVN_ERR(svn_client__get_revision_number(&rev, NULL, ra_session,
good_rev, url, pool));
*ra_session_p = ra_session;
*rev_p = rev;
*url_p = url;
return SVN_NO_ERROR;
}
svn_error_t *
svn_client__path_relative_to_session(const char **rel_path,
svn_ra_session_t *ra_session,
const char *url,
apr_pool_t *pool) {
const char *session_url;
SVN_ERR(svn_ra_get_session_url(ra_session, &session_url, pool));
if (strcmp(session_url, url) == 0)
*rel_path = "";
else
*rel_path = svn_path_uri_decode(svn_path_is_child(session_url, url, pool),
pool);
return SVN_NO_ERROR;
}
svn_error_t *
svn_client__ensure_ra_session_url(const char **old_session_url,
svn_ra_session_t *ra_session,
const char *session_url,
apr_pool_t *pool) {
*old_session_url = NULL;
SVN_ERR(svn_ra_get_session_url(ra_session, old_session_url, pool));
if (! session_url)
SVN_ERR(svn_ra_get_repos_root2(ra_session, &session_url, pool));
if (strcmp(*old_session_url, session_url) != 0)
SVN_ERR(svn_ra_reparent(ra_session, session_url, pool));
return SVN_NO_ERROR;
}
struct gls_receiver_baton_t {
apr_array_header_t *segments;
svn_client_ctx_t *ctx;
apr_pool_t *pool;
};
static svn_error_t *
gls_receiver(svn_location_segment_t *segment,
void *baton,
apr_pool_t *pool) {
struct gls_receiver_baton_t *b = baton;
APR_ARRAY_PUSH(b->segments, svn_location_segment_t *) =
svn_location_segment_dup(segment, b->pool);
if (b->ctx->cancel_func)
SVN_ERR((b->ctx->cancel_func)(b->ctx->cancel_baton));
return SVN_NO_ERROR;
}
static int
compare_segments(const void *a, const void *b) {
const svn_location_segment_t *a_seg
= *((const svn_location_segment_t * const *) a);
const svn_location_segment_t *b_seg
= *((const svn_location_segment_t * const *) b);
if (a_seg->range_start == b_seg->range_start)
return 0;
return (a_seg->range_start < b_seg->range_start) ? -1 : 1;
}
svn_error_t *
svn_client__repos_location_segments(apr_array_header_t **segments,
svn_ra_session_t *ra_session,
const char *path,
svn_revnum_t peg_revision,
svn_revnum_t start_revision,
svn_revnum_t end_revision,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
struct gls_receiver_baton_t gls_receiver_baton;
*segments = apr_array_make(pool, 8, sizeof(svn_location_segment_t *));
gls_receiver_baton.segments = *segments;
gls_receiver_baton.ctx = ctx;
gls_receiver_baton.pool = pool;
SVN_ERR(svn_ra_get_location_segments(ra_session, path, peg_revision,
start_revision, end_revision,
gls_receiver, &gls_receiver_baton,
pool));
qsort((*segments)->elts, (*segments)->nelts,
(*segments)->elt_size, compare_segments);
return SVN_NO_ERROR;
}
svn_error_t *
svn_client__repos_locations(const char **start_url,
svn_opt_revision_t **start_revision,
const char **end_url,
svn_opt_revision_t **end_revision,
svn_ra_session_t *ra_session,
const char *path,
const svn_opt_revision_t *revision,
const svn_opt_revision_t *start,
const svn_opt_revision_t *end,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
const char *repos_url;
const char *url;
const char *start_path = NULL;
const char *end_path = NULL;
svn_revnum_t peg_revnum = SVN_INVALID_REVNUM;
svn_revnum_t start_revnum, end_revnum;
svn_revnum_t youngest_rev = SVN_INVALID_REVNUM;
apr_array_header_t *revs;
apr_hash_t *rev_locs;
apr_pool_t *subpool = svn_pool_create(pool);
if (revision->kind == svn_opt_revision_unspecified
|| start->kind == svn_opt_revision_unspecified)
return svn_error_create(SVN_ERR_CLIENT_BAD_REVISION, NULL, NULL);
if (! svn_path_is_url(path)) {
svn_wc_adm_access_t *adm_access;
const svn_wc_entry_t *entry;
SVN_ERR(svn_wc_adm_probe_open3(&adm_access, NULL, path,
FALSE, 0, ctx->cancel_func,
ctx->cancel_baton, pool));
SVN_ERR(svn_wc_entry(&entry, path, adm_access, FALSE, pool));
SVN_ERR(svn_wc_adm_close(adm_access));
if (entry->copyfrom_url && revision->kind == svn_opt_revision_working) {
url = entry->copyfrom_url;
peg_revnum = entry->copyfrom_rev;
if (!entry->url || strcmp(entry->url, entry->copyfrom_url) != 0) {
ra_session = NULL;
}
} else if (entry->url) {
url = entry->url;
} else {
return svn_error_createf(SVN_ERR_ENTRY_MISSING_URL, NULL,
_("'%s' has no URL"),
svn_path_local_style(path, pool));
}
} else {
url = path;
}
if (! ra_session)
SVN_ERR(svn_client__open_ra_session_internal(&ra_session, url, NULL,
NULL, NULL, FALSE, TRUE,
ctx, subpool));
if (peg_revnum == SVN_INVALID_REVNUM)
SVN_ERR(svn_client__get_revision_number(&peg_revnum, &youngest_rev,
ra_session, revision, path,
pool));
SVN_ERR(svn_client__get_revision_number(&start_revnum, &youngest_rev,
ra_session, start, path, pool));
if (end->kind == svn_opt_revision_unspecified)
end_revnum = start_revnum;
else
SVN_ERR(svn_client__get_revision_number(&end_revnum, &youngest_rev,
ra_session, end, path, pool));
*start_revision = apr_pcalloc(pool, sizeof(**start_revision));
(*start_revision)->kind = svn_opt_revision_number;
(*start_revision)->value.number = start_revnum;
if (end->kind != svn_opt_revision_unspecified) {
*end_revision = apr_pcalloc(pool, sizeof(**end_revision));
(*end_revision)->kind = svn_opt_revision_number;
(*end_revision)->value.number = end_revnum;
}
if (start_revnum == peg_revnum && end_revnum == peg_revnum) {
*start_url = url;
if (end->kind != svn_opt_revision_unspecified)
*end_url = url;
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
SVN_ERR(svn_ra_get_repos_root2(ra_session, &repos_url, subpool));
revs = apr_array_make(subpool, 2, sizeof(svn_revnum_t));
APR_ARRAY_PUSH(revs, svn_revnum_t) = start_revnum;
if (end_revnum != start_revnum)
APR_ARRAY_PUSH(revs, svn_revnum_t) = end_revnum;
SVN_ERR(svn_ra_get_locations(ra_session, &rev_locs, "", peg_revnum,
revs, subpool));
start_path = apr_hash_get(rev_locs, &start_revnum, sizeof(svn_revnum_t));
if (! start_path)
return svn_error_createf
(SVN_ERR_CLIENT_UNRELATED_RESOURCES, NULL,
_("Unable to find repository location for '%s' in revision %ld"),
path, start_revnum);
end_path = apr_hash_get(rev_locs, &end_revnum, sizeof(svn_revnum_t));
if (! end_path)
return svn_error_createf
(SVN_ERR_CLIENT_UNRELATED_RESOURCES, NULL,
_("The location for '%s' for revision %ld does not exist in the "
"repository or refers to an unrelated object"),
path, end_revnum);
if (start_path[0] == '/')
start_path = start_path + 1;
if (end_path[0] == '/')
end_path = end_path + 1;
*start_url = svn_path_join(repos_url, svn_path_uri_encode(start_path,
pool), pool);
if (end->kind != svn_opt_revision_unspecified)
*end_url = svn_path_join(repos_url, svn_path_uri_encode(end_path,
pool), pool);
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
svn_error_t *
svn_client__get_youngest_common_ancestor(const char **ancestor_path,
svn_revnum_t *ancestor_revision,
const char *path_or_url1,
svn_revnum_t rev1,
const char *path_or_url2,
svn_revnum_t rev2,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
apr_hash_t *history1, *history2;
apr_hash_index_t *hi;
svn_revnum_t yc_revision = SVN_INVALID_REVNUM;
const char *yc_path = NULL;
svn_opt_revision_t revision1, revision2;
revision1.kind = revision2.kind = svn_opt_revision_number;
revision1.value.number = rev1;
revision2.value.number = rev2;
SVN_ERR(svn_client__get_history_as_mergeinfo(&history1, path_or_url1,
&revision1,
SVN_INVALID_REVNUM,
SVN_INVALID_REVNUM,
NULL, NULL, ctx, pool));
SVN_ERR(svn_client__get_history_as_mergeinfo(&history2, path_or_url2,
&revision2,
SVN_INVALID_REVNUM,
SVN_INVALID_REVNUM,
NULL, NULL, ctx, pool));
for (hi = apr_hash_first(NULL, history1); hi; hi = apr_hash_next(hi)) {
const void *key;
apr_ssize_t klen;
void *val;
const char *path;
apr_array_header_t *ranges1, *ranges2, *common;
apr_hash_this(hi, &key, &klen, &val);
path = key;
ranges1 = val;
ranges2 = apr_hash_get(history2, key, klen);
if (ranges2) {
SVN_ERR(svn_rangelist_intersect(&common, ranges1, ranges2,
TRUE, pool));
if (common->nelts) {
svn_merge_range_t *yc_range =
APR_ARRAY_IDX(common, common->nelts - 1, svn_merge_range_t *);
if ((! SVN_IS_VALID_REVNUM(yc_revision))
|| (yc_range->end > yc_revision)) {
yc_revision = yc_range->end;
yc_path = path + 1;
}
}
}
}
*ancestor_path = yc_path;
*ancestor_revision = yc_revision;
return SVN_NO_ERROR;
}
