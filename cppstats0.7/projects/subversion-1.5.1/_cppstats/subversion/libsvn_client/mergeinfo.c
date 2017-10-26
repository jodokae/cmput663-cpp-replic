#include <apr_pools.h>
#include <apr_strings.h>
#include <assert.h>
#include "svn_pools.h"
#include "svn_path.h"
#include "svn_string.h"
#include "svn_opt.h"
#include "svn_error.h"
#include "svn_error_codes.h"
#include "svn_props.h"
#include "svn_mergeinfo.h"
#include "svn_sorts.h"
#include "svn_ra.h"
#include "svn_client.h"
#include "svn_hash.h"
#include "private/svn_mergeinfo_private.h"
#include "private/svn_wc_private.h"
#include "private/svn_ra_private.h"
#include "client.h"
#include "mergeinfo.h"
#include "svn_private_config.h"
svn_error_t *
svn_client__parse_mergeinfo(svn_mergeinfo_t *mergeinfo,
const svn_wc_entry_t *entry,
const char *wcpath,
svn_boolean_t pristine,
svn_wc_adm_access_t *adm_access,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
apr_hash_t *props = apr_hash_make(pool);
const svn_string_t *propval;
SVN_ERR(svn_client__get_prop_from_wc(props, SVN_PROP_MERGEINFO,
wcpath, pristine, entry, adm_access,
svn_depth_empty, NULL, ctx, pool));
propval = apr_hash_get(props, wcpath, APR_HASH_KEY_STRING);
if (propval)
SVN_ERR(svn_mergeinfo_parse(mergeinfo, propval->data, pool));
else
*mergeinfo = NULL;
return SVN_NO_ERROR;
}
svn_error_t *
svn_client__record_wc_mergeinfo(const char *wcpath,
svn_mergeinfo_t mergeinfo,
svn_wc_adm_access_t *adm_access,
apr_pool_t *pool) {
svn_string_t *mergeinfo_str;
if (mergeinfo) {
SVN_ERR(svn_mergeinfo_to_string(&mergeinfo_str, mergeinfo, pool));
} else {
mergeinfo_str = NULL;
}
return svn_wc_prop_set2(SVN_PROP_MERGEINFO, mergeinfo_str, wcpath,
adm_access, TRUE , pool);
}
static APR_INLINE void
adjust_mergeinfo_source_paths(svn_mergeinfo_t mergeinfo, const char *walk_path,
svn_mergeinfo_t wc_mergeinfo, apr_pool_t *pool) {
apr_hash_index_t *hi;
const void *merge_source;
void *rangelist;
const char *path;
for (hi = apr_hash_first(NULL, wc_mergeinfo); hi; hi = apr_hash_next(hi)) {
apr_hash_this(hi, &merge_source, NULL, &rangelist);
path = svn_path_join((const char *) merge_source, walk_path,
apr_hash_pool_get(mergeinfo));
apr_hash_set(mergeinfo, path, APR_HASH_KEY_STRING, rangelist);
}
}
svn_error_t *
svn_client__get_wc_mergeinfo(svn_mergeinfo_t *mergeinfo,
svn_boolean_t *inherited,
svn_boolean_t pristine,
svn_mergeinfo_inheritance_t inherit,
const svn_wc_entry_t *entry,
const char *wcpath,
const char *limit_path,
const char **walked_path,
svn_wc_adm_access_t *adm_access,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
const char *walk_path = "";
svn_mergeinfo_t wc_mergeinfo;
svn_boolean_t switched;
svn_revnum_t base_revision = entry->revision;
if (limit_path)
SVN_ERR(svn_path_get_absolute(&limit_path, limit_path, pool));
while (TRUE) {
if (inherit == svn_mergeinfo_nearest_ancestor) {
wc_mergeinfo = NULL;
inherit = svn_mergeinfo_inherited;
} else {
SVN_ERR(svn_client__parse_mergeinfo(&wc_mergeinfo, entry, wcpath,
pristine, adm_access, ctx,
pool));
}
SVN_ERR(svn_wc__path_switched(wcpath, &switched, entry, pool));
if (switched)
break;
#if defined(WIN32) || defined(__CYGWIN__)
if (strlen(wcpath) == 0
|| ((strlen(wcpath) > 0 && wcpath[0] != '/')
&& !(strlen(wcpath) > 2
&& wcpath[1] == ':'
&& wcpath[2] == '/'
&& ((wcpath[0] >= 'A' && wcpath[0] <= 'Z')
|| (wcpath[0] >= 'a' && wcpath[0] <= 'z')))))
#else
if (!(strlen(wcpath) > 0 && wcpath[0] == '/'))
#endif
{
SVN_ERR(svn_path_get_absolute(&wcpath, wcpath, pool));
}
if (wc_mergeinfo == NULL &&
inherit != svn_mergeinfo_explicit &&
!svn_dirent_is_root(wcpath, strlen(wcpath))) {
svn_error_t *err;
if (limit_path && strcmp(limit_path, wcpath) == 0)
break;
walk_path = svn_path_join(svn_path_basename(wcpath, pool),
walk_path, pool);
wcpath = svn_path_dirname(wcpath, pool);
err = svn_wc_adm_open3(&adm_access, NULL, wcpath,
FALSE, 0, NULL, NULL, pool);
if (err) {
if (err->apr_err == SVN_ERR_WC_NOT_DIRECTORY) {
svn_error_clear(err);
err = SVN_NO_ERROR;
*inherited = FALSE;
*mergeinfo = wc_mergeinfo;
}
return err;
}
SVN_ERR(svn_wc_entry(&entry, wcpath, adm_access, FALSE, pool));
if (base_revision < entry->cmt_rev
|| entry->revision < base_revision)
break;
if (entry)
continue;
}
break;
}
if (svn_path_is_empty(walk_path)) {
*inherited = FALSE;
*mergeinfo = wc_mergeinfo;
} else {
if (wc_mergeinfo) {
*inherited = (wc_mergeinfo != NULL);
*mergeinfo = apr_hash_make(pool);
adjust_mergeinfo_source_paths(*mergeinfo, walk_path, wc_mergeinfo,
pool);
} else {
*inherited = FALSE;
*mergeinfo = NULL;
}
}
if (walked_path)
*walked_path = walk_path;
if (*inherited) {
SVN_ERR(svn_mergeinfo_inheritable(mergeinfo, *mergeinfo, NULL,
SVN_INVALID_REVNUM, SVN_INVALID_REVNUM, pool));
svn_mergeinfo__remove_empty_rangelists(*mergeinfo, pool);
}
return SVN_NO_ERROR;
}
svn_error_t *
svn_client__get_repos_mergeinfo(svn_ra_session_t *ra_session,
svn_mergeinfo_t *target_mergeinfo,
const char *rel_path,
svn_revnum_t rev,
svn_mergeinfo_inheritance_t inherit,
svn_boolean_t squelch_incapable,
apr_pool_t *pool) {
svn_error_t *err;
svn_mergeinfo_t repos_mergeinfo;
const char *old_session_url;
apr_array_header_t *rel_paths = apr_array_make(pool, 1, sizeof(rel_path));
APR_ARRAY_PUSH(rel_paths, const char *) = rel_path;
SVN_ERR(svn_client__ensure_ra_session_url(&old_session_url, ra_session,
NULL, pool));
err = svn_ra_get_mergeinfo(ra_session, &repos_mergeinfo, rel_paths, rev,
inherit, FALSE, pool);
if (err) {
if (squelch_incapable && err->apr_err == SVN_ERR_UNSUPPORTED_FEATURE) {
svn_error_clear(err);
repos_mergeinfo = NULL;
} else
return err;
}
if (old_session_url)
SVN_ERR(svn_ra_reparent(ra_session, old_session_url, pool));
if (repos_mergeinfo)
*target_mergeinfo = apr_hash_get(repos_mergeinfo, rel_path,
APR_HASH_KEY_STRING);
else
*target_mergeinfo = NULL;
return SVN_NO_ERROR;
}
svn_error_t *
svn_client__get_wc_or_repos_mergeinfo(svn_mergeinfo_t *target_mergeinfo,
const svn_wc_entry_t *entry,
svn_boolean_t *indirect,
svn_boolean_t repos_only,
svn_mergeinfo_inheritance_t inherit,
svn_ra_session_t *ra_session,
const char *target_wcpath,
svn_wc_adm_access_t *adm_access,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
const char *url;
svn_revnum_t target_rev;
SVN_ERR(svn_client__entry_location(&url, &target_rev, target_wcpath,
svn_opt_revision_working, entry, pool));
if (repos_only)
*target_mergeinfo = NULL;
else
SVN_ERR(svn_client__get_wc_mergeinfo(target_mergeinfo, indirect, FALSE,
inherit, entry, target_wcpath,
NULL, NULL, adm_access, ctx, pool));
if (*target_mergeinfo == NULL) {
svn_mergeinfo_t repos_mergeinfo;
if (entry->schedule != svn_wc_schedule_add) {
apr_hash_t *props = apr_hash_make(pool);
SVN_ERR(svn_client__get_prop_from_wc(props, SVN_PROP_MERGEINFO,
target_wcpath, TRUE, entry,
adm_access, svn_depth_empty,
NULL, ctx, pool));
if (apr_hash_get(props, target_wcpath, APR_HASH_KEY_STRING) == NULL) {
const char *repos_rel_path;
if (ra_session == NULL)
SVN_ERR(svn_client__open_ra_session_internal(&ra_session, url,
NULL, NULL, NULL,
FALSE, TRUE, ctx,
pool));
SVN_ERR(svn_client__path_relative_to_root(&repos_rel_path, url,
entry->repos, FALSE,
ra_session, NULL,
pool));
SVN_ERR(svn_client__get_repos_mergeinfo(ra_session,
&repos_mergeinfo,
repos_rel_path,
target_rev,
inherit,
TRUE,
pool));
if (repos_mergeinfo) {
*target_mergeinfo = repos_mergeinfo;
*indirect = TRUE;
}
}
}
}
return SVN_NO_ERROR;
}
svn_error_t *
svn_client__mergeinfo_from_segments(svn_mergeinfo_t *mergeinfo_p,
apr_array_header_t *segments,
apr_pool_t *pool) {
svn_mergeinfo_t mergeinfo = apr_hash_make(pool);
int i;
for (i = 0; i < segments->nelts; i++) {
svn_location_segment_t *segment =
APR_ARRAY_IDX(segments, i, svn_location_segment_t *);
apr_array_header_t *path_ranges;
svn_merge_range_t *range;
const char *source_path;
if (! segment->path)
continue;
source_path = apr_pstrcat(pool, "/", segment->path, NULL);
path_ranges = apr_hash_get(mergeinfo, source_path, APR_HASH_KEY_STRING);
if (! path_ranges)
path_ranges = apr_array_make(pool, 1, sizeof(range));
range = apr_pcalloc(pool, sizeof(*range));
range->start = MAX(segment->range_start - 1, 0);
range->end = segment->range_end;
range->inheritable = TRUE;
APR_ARRAY_PUSH(path_ranges, svn_merge_range_t *) = range;
apr_hash_set(mergeinfo, source_path, APR_HASH_KEY_STRING, path_ranges);
}
*mergeinfo_p = mergeinfo;
return SVN_NO_ERROR;
}
svn_error_t *
svn_client__get_history_as_mergeinfo(svn_mergeinfo_t *mergeinfo_p,
const char *path_or_url,
const svn_opt_revision_t *peg_revision,
svn_revnum_t range_youngest,
svn_revnum_t range_oldest,
svn_ra_session_t *ra_session,
svn_wc_adm_access_t *adm_access,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
apr_array_header_t *segments;
svn_revnum_t peg_revnum = SVN_INVALID_REVNUM;
const char *url;
apr_pool_t *sesspool = NULL;
svn_ra_session_t *session = ra_session;
SVN_ERR(svn_client__derive_location(&url, &peg_revnum, path_or_url,
peg_revision, session, adm_access,
ctx, pool));
if (session == NULL) {
sesspool = svn_pool_create(pool);
SVN_ERR(svn_client__open_ra_session_internal(&session, url, NULL, NULL,
NULL, FALSE, TRUE, ctx,
sesspool));
}
if (! SVN_IS_VALID_REVNUM(range_youngest))
range_youngest = peg_revnum;
if (! SVN_IS_VALID_REVNUM(range_oldest))
range_oldest = 0;
SVN_ERR(svn_client__repos_location_segments(&segments, session, "",
peg_revnum, range_youngest,
range_oldest, ctx, pool));
SVN_ERR(svn_client__mergeinfo_from_segments(mergeinfo_p, segments, pool));
if (sesspool)
svn_pool_destroy(sesspool);
return SVN_NO_ERROR;
}
static svn_error_t *
should_elide_mergeinfo(svn_boolean_t *elides,
svn_mergeinfo_t parent_mergeinfo,
svn_mergeinfo_t child_mergeinfo,
const char *path_suffix,
apr_pool_t *pool) {
if (child_mergeinfo == NULL) {
*elides = FALSE;
} else if (apr_hash_count(child_mergeinfo) == 0) {
if (!parent_mergeinfo || apr_hash_count(parent_mergeinfo) == 0)
*elides = TRUE;
else
*elides = FALSE;
} else if (!parent_mergeinfo || apr_hash_count(parent_mergeinfo) == 0) {
*elides = FALSE;
} else {
svn_mergeinfo_t path_tweaked_parent_mergeinfo;
apr_pool_t *subpool = svn_pool_create(pool);
path_tweaked_parent_mergeinfo = apr_hash_make(subpool);
if (path_suffix)
adjust_mergeinfo_source_paths(path_tweaked_parent_mergeinfo,
path_suffix, parent_mergeinfo,
subpool);
else
path_tweaked_parent_mergeinfo = parent_mergeinfo;
SVN_ERR(svn_mergeinfo__equals(elides,
path_tweaked_parent_mergeinfo,
child_mergeinfo, TRUE, subpool));
svn_pool_destroy(subpool);
}
return SVN_NO_ERROR;
}
static svn_error_t *
elide_mergeinfo(svn_mergeinfo_t parent_mergeinfo,
svn_mergeinfo_t child_mergeinfo,
const char *path,
const char *path_suffix,
svn_wc_adm_access_t *adm_access,
apr_pool_t *pool) {
svn_boolean_t elides;
SVN_ERR(should_elide_mergeinfo(&elides,
parent_mergeinfo, child_mergeinfo,
path_suffix, pool));
if (elides)
SVN_ERR(svn_wc_prop_set2(SVN_PROP_MERGEINFO, NULL, path, adm_access,
TRUE, pool));
return SVN_NO_ERROR;
}
svn_error_t *
svn_client__elide_children(apr_array_header_t *children_with_mergeinfo,
const char *target_wcpath,
const svn_wc_entry_t *entry,
svn_wc_adm_access_t *adm_access,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
if (children_with_mergeinfo && children_with_mergeinfo->nelts) {
int i;
const char *last_immediate_child;
svn_mergeinfo_t target_mergeinfo;
apr_pool_t *iterpool = svn_pool_create(pool);
SVN_ERR(svn_client__parse_mergeinfo(&target_mergeinfo, entry,
target_wcpath, FALSE,
adm_access, ctx, pool));
for (i = 0; i < children_with_mergeinfo->nelts; i++) {
svn_mergeinfo_t child_mergeinfo;
svn_boolean_t switched;
const svn_wc_entry_t *child_entry;
svn_client__merge_path_t *child =
APR_ARRAY_IDX(children_with_mergeinfo, i,
svn_client__merge_path_t *);
svn_pool_clear(iterpool);
if (!child)
continue;
if (child->absent)
continue;
if (i == 0) {
if (strcmp(target_wcpath, child->path) == 0) {
last_immediate_child = NULL;
continue;
}
last_immediate_child = child->path;
} else if (last_immediate_child
&& svn_path_is_ancestor(last_immediate_child, child->path)) {
continue;
} else {
last_immediate_child = child->path;
}
SVN_ERR(svn_wc__entry_versioned(&child_entry, child->path,
adm_access, FALSE, iterpool));
SVN_ERR(svn_wc__path_switched(child->path, &switched, child_entry,
iterpool));
if (!switched) {
const char *path_prefix = svn_path_dirname(child->path,
iterpool);
const char *path_suffix = svn_path_basename(child->path,
iterpool);
SVN_ERR(svn_client__parse_mergeinfo(&child_mergeinfo, entry,
child->path, FALSE,
adm_access, ctx, iterpool));
while (strcmp(path_prefix, target_wcpath) != 0) {
path_suffix = svn_path_join(svn_path_basename(path_prefix,
iterpool),
path_suffix, iterpool);
path_prefix = svn_path_dirname(path_prefix, iterpool);
}
SVN_ERR(elide_mergeinfo(target_mergeinfo, child_mergeinfo,
child->path, path_suffix, adm_access,
iterpool));
}
}
svn_pool_destroy(iterpool);
}
return SVN_NO_ERROR;
}
svn_error_t *
svn_client__elide_mergeinfo(const char *target_wcpath,
const char *wc_elision_limit_path,
const svn_wc_entry_t *entry,
svn_wc_adm_access_t *adm_access,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
if (!wc_elision_limit_path
|| strcmp(target_wcpath, wc_elision_limit_path) != 0) {
svn_mergeinfo_t target_mergeinfo;
svn_mergeinfo_t mergeinfo = NULL;
svn_boolean_t inherited;
const char *walk_path;
SVN_ERR(svn_client__get_wc_mergeinfo(&target_mergeinfo, &inherited,
FALSE, svn_mergeinfo_inherited,
entry, target_wcpath,
wc_elision_limit_path
? wc_elision_limit_path
: NULL,
&walk_path, adm_access,
ctx, pool));
if (inherited || target_mergeinfo == NULL)
return SVN_NO_ERROR;
SVN_ERR(svn_client__get_wc_mergeinfo(&mergeinfo, &inherited, FALSE,
svn_mergeinfo_nearest_ancestor,
entry, target_wcpath,
wc_elision_limit_path
? wc_elision_limit_path
: NULL,
&walk_path, adm_access,
ctx, pool));
if (!mergeinfo && !wc_elision_limit_path) {
SVN_ERR(svn_client__get_wc_or_repos_mergeinfo
(&mergeinfo, entry, &inherited, TRUE,
svn_mergeinfo_nearest_ancestor,
NULL, target_wcpath, adm_access, ctx, pool));
}
if (!mergeinfo && wc_elision_limit_path)
return SVN_NO_ERROR;
SVN_ERR(elide_mergeinfo(mergeinfo, target_mergeinfo, target_wcpath,
NULL, adm_access, pool));
}
return SVN_NO_ERROR;
}
svn_error_t *
svn_client__elide_mergeinfo_for_tree(apr_hash_t *children_with_mergeinfo,
svn_wc_adm_access_t *adm_access,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
int i;
apr_pool_t *iterpool = svn_pool_create(pool);
apr_array_header_t *sorted_children =
svn_sort__hash(children_with_mergeinfo, svn_sort_compare_items_as_paths,
pool);
for (i = sorted_children->nelts -1; i >= 0; i--) {
const svn_wc_entry_t *child_entry;
const char *child_wcpath;
svn_sort__item_t *item = &APR_ARRAY_IDX(sorted_children, i,
svn_sort__item_t);
svn_pool_clear(iterpool);
child_wcpath = item->key;
SVN_ERR(svn_wc__entry_versioned(&child_entry, child_wcpath, adm_access,
FALSE, iterpool));
SVN_ERR(svn_client__elide_mergeinfo(child_wcpath, NULL, child_entry,
adm_access, ctx, iterpool));
}
svn_pool_destroy(iterpool);
return SVN_NO_ERROR;
}
static svn_error_t *
get_mergeinfo(svn_mergeinfo_t *mergeinfo,
const char **repos_root,
const char *path_or_url,
const svn_opt_revision_t *peg_revision,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
apr_pool_t *subpool = svn_pool_create(pool);
svn_ra_session_t *ra_session;
svn_revnum_t rev;
if (svn_path_is_url(path_or_url)) {
const char *repos_rel_path;
SVN_ERR(svn_client__open_ra_session_internal(&ra_session, path_or_url,
NULL, NULL, NULL, FALSE,
TRUE, ctx, subpool));
SVN_ERR(svn_client__get_revision_number(&rev, NULL, ra_session,
peg_revision, "", subpool));
SVN_ERR(svn_ra_get_repos_root2(ra_session, repos_root, pool));
SVN_ERR(svn_client__path_relative_to_root(&repos_rel_path, path_or_url,
*repos_root, FALSE, NULL,
NULL, subpool));
SVN_ERR(svn_client__get_repos_mergeinfo(ra_session, mergeinfo,
repos_rel_path, rev,
svn_mergeinfo_inherited, FALSE,
pool));
} else {
svn_wc_adm_access_t *adm_access;
const svn_wc_entry_t *entry;
const char *url;
svn_boolean_t indirect;
SVN_ERR(svn_wc_adm_probe_open3(&adm_access, NULL, path_or_url, FALSE,
0, ctx->cancel_func, ctx->cancel_baton,
subpool));
SVN_ERR(svn_wc__entry_versioned(&entry, path_or_url, adm_access, FALSE,
subpool));
SVN_ERR(svn_client__entry_location(&url, &rev, path_or_url,
svn_opt_revision_working, entry,
subpool));
SVN_ERR(svn_client__open_ra_session_internal(&ra_session, url,
NULL, NULL, NULL, FALSE,
TRUE, ctx, subpool));
SVN_ERR(svn_ra__assert_mergeinfo_capable_server(ra_session, path_or_url,
subpool));
SVN_ERR(svn_client__get_repos_root(repos_root, path_or_url, peg_revision,
adm_access, ctx, pool));
SVN_ERR(svn_client__get_wc_or_repos_mergeinfo(mergeinfo, entry,
&indirect, FALSE,
svn_mergeinfo_inherited,
NULL, path_or_url,
adm_access, ctx, pool));
SVN_ERR(svn_wc_adm_close(adm_access));
}
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
struct elide_mergeinfo_catalog_dir_baton {
const char *inherited_mergeinfo_path;
svn_mergeinfo_t mergeinfo_catalog;
};
static svn_error_t *
elide_mergeinfo_catalog_open_root(void *eb,
svn_revnum_t base_revision,
apr_pool_t *dir_pool,
void **root_baton) {
struct elide_mergeinfo_catalog_dir_baton *b = apr_pcalloc(dir_pool,
sizeof(*b));
b->mergeinfo_catalog = eb;
*root_baton = b;
return SVN_NO_ERROR;
}
static svn_error_t *
elide_mergeinfo_catalog_open_directory(const char *path,
void *parent_baton,
svn_revnum_t base_revision,
apr_pool_t *dir_pool,
void **child_baton) {
struct elide_mergeinfo_catalog_dir_baton *b, *pb = parent_baton;
b = apr_pcalloc(dir_pool, sizeof(*b));
b->mergeinfo_catalog = pb->mergeinfo_catalog;
if (apr_hash_get(b->mergeinfo_catalog, path, APR_HASH_KEY_STRING))
b->inherited_mergeinfo_path = apr_pstrdup(dir_pool, path);
else
b->inherited_mergeinfo_path = pb->inherited_mergeinfo_path;
*child_baton = b;
return SVN_NO_ERROR;
}
struct elide_mergeinfo_catalog_cb_baton {
apr_array_header_t *elidable_paths;
svn_mergeinfo_t mergeinfo_catalog;
apr_pool_t *result_pool;
};
static svn_error_t *
elide_mergeinfo_catalog_cb(void **dir_baton,
void *parent_baton,
void *callback_baton,
const char *path,
apr_pool_t *pool) {
struct elide_mergeinfo_catalog_cb_baton *cb = callback_baton;
struct elide_mergeinfo_catalog_dir_baton *pb = parent_baton;
const char *path_suffix;
svn_boolean_t elides;
assert(pb != NULL);
*dir_baton = NULL;
if (!pb->inherited_mergeinfo_path
|| strcmp(path, "/") == 0)
return SVN_NO_ERROR;
path_suffix = svn_path_is_child(pb->inherited_mergeinfo_path,
path, NULL);
assert(path_suffix != NULL);
SVN_ERR(should_elide_mergeinfo(&elides,
apr_hash_get(cb->mergeinfo_catalog,
pb->inherited_mergeinfo_path,
APR_HASH_KEY_STRING),
apr_hash_get(cb->mergeinfo_catalog,
path,
APR_HASH_KEY_STRING),
path_suffix,
pool));
if (elides)
APR_ARRAY_PUSH(cb->elidable_paths, const char *) =
apr_pstrdup(cb->result_pool, path);
return SVN_NO_ERROR;
}
svn_error_t *
svn_client__elide_mergeinfo_catalog(svn_mergeinfo_t mergeinfo_catalog,
apr_pool_t *pool) {
apr_array_header_t *paths;
apr_array_header_t *elidable_paths = apr_array_make(pool, 1,
sizeof(const char *));
svn_delta_editor_t *editor = svn_delta_default_editor(pool);
struct elide_mergeinfo_catalog_cb_baton cb = {elidable_paths,
mergeinfo_catalog,
pool
};
int i;
editor->open_root = elide_mergeinfo_catalog_open_root;
editor->open_directory = elide_mergeinfo_catalog_open_directory;
SVN_ERR(svn_hash_keys(&paths, mergeinfo_catalog, pool));
SVN_ERR(svn_delta_path_driver(editor,
mergeinfo_catalog,
SVN_INVALID_REVNUM,
paths,
elide_mergeinfo_catalog_cb,
&cb,
pool));
for (i = 0; i < elidable_paths->nelts; i++) {
const char *path = APR_ARRAY_IDX(elidable_paths, i, const char *);
apr_hash_set(mergeinfo_catalog, path, APR_HASH_KEY_STRING, NULL);
}
return SVN_NO_ERROR;
}
struct filter_log_entry_baton_t {
apr_array_header_t *rangelist;
svn_log_entry_receiver_t log_receiver;
void *log_receiver_baton;
svn_client_ctx_t *ctx;
};
static svn_error_t *
filter_log_entry_with_rangelist(void *baton,
svn_log_entry_t *log_entry,
apr_pool_t *pool) {
struct filter_log_entry_baton_t *fleb = baton;
svn_merge_range_t *range;
apr_array_header_t *intersection, *this_rangelist;
if (fleb->ctx->cancel_func)
SVN_ERR(fleb->ctx->cancel_func(fleb->ctx->cancel_baton));
this_rangelist = apr_array_make(pool, 1, sizeof(svn_merge_range_t *));
range = apr_pcalloc(pool, sizeof(*range));
range->start = log_entry->revision - 1;
range->end = log_entry->revision;
range->inheritable = TRUE;
APR_ARRAY_PUSH(this_rangelist, svn_merge_range_t *) = range;
SVN_ERR(svn_rangelist_intersect(&intersection, fleb->rangelist,
this_rangelist, TRUE, pool));
if (! (intersection && intersection->nelts))
return SVN_NO_ERROR;
assert (intersection->nelts == 1);
return fleb->log_receiver(fleb->log_receiver_baton, log_entry, pool);
}
static svn_error_t *
logs_for_mergeinfo_rangelist(const char *source_url,
apr_array_header_t *rangelist,
svn_boolean_t discover_changed_paths,
const apr_array_header_t *revprops,
svn_log_entry_receiver_t log_receiver,
void *log_receiver_baton,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
apr_array_header_t *target;
svn_merge_range_t *oldest_range, *youngest_range;
svn_opt_revision_t oldest_rev, youngest_rev;
struct filter_log_entry_baton_t fleb;
if (! rangelist->nelts)
return SVN_NO_ERROR;
qsort(rangelist->elts, rangelist->nelts,
rangelist->elt_size, svn_sort_compare_ranges);
target = apr_array_make(pool, 1, sizeof(const char *));
APR_ARRAY_PUSH(target, const char *) = source_url;
youngest_range = APR_ARRAY_IDX(rangelist, rangelist->nelts - 1,
svn_merge_range_t *);
youngest_rev.kind = svn_opt_revision_number;
youngest_rev.value.number = youngest_range->end;
oldest_range = APR_ARRAY_IDX(rangelist, 0, svn_merge_range_t *);
oldest_rev.kind = svn_opt_revision_number;
oldest_rev.value.number = oldest_range->start;
fleb.rangelist = rangelist;
fleb.log_receiver = log_receiver;
fleb.log_receiver_baton = log_receiver_baton;
fleb.ctx = ctx;
SVN_ERR(svn_client_log4(target, &youngest_rev, &oldest_rev, &youngest_rev,
0, discover_changed_paths, FALSE, FALSE, revprops,
filter_log_entry_with_rangelist, &fleb, ctx, pool));
if (ctx->cancel_func)
SVN_ERR(ctx->cancel_func(ctx->cancel_baton));
return SVN_NO_ERROR;
}
static svn_error_t *
location_from_path_and_rev(const char **url,
svn_opt_revision_t **revision,
const char *path_or_url,
const svn_opt_revision_t *peg_revision,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
svn_wc_adm_access_t *adm_access = NULL;
svn_ra_session_t *ra_session;
apr_pool_t *subpool = svn_pool_create(pool);
svn_revnum_t rev;
if (! svn_path_is_url(path_or_url)
&& (peg_revision->kind == svn_opt_revision_base
|| peg_revision->kind == svn_opt_revision_working
|| peg_revision->kind == svn_opt_revision_committed
|| peg_revision->kind == svn_opt_revision_unspecified)) {
int adm_lock_level = SVN_WC__LEVELS_TO_LOCK_FROM_DEPTH(svn_depth_empty);
SVN_ERR(svn_wc_adm_probe_open3(&adm_access, NULL, path_or_url,
FALSE, adm_lock_level,
ctx->cancel_func, ctx->cancel_baton,
subpool));
}
SVN_ERR(svn_client__ra_session_from_path(&ra_session, &rev, url,
path_or_url, adm_access,
peg_revision, peg_revision,
ctx, subpool));
*url = apr_pstrdup(pool, *url);
*revision = apr_pcalloc(pool, sizeof(**revision));
(*revision)->kind = svn_opt_revision_number;
(*revision)->value.number = rev;
svn_pool_destroy(subpool);
if (adm_access)
SVN_ERR(svn_wc_adm_close(adm_access));
return SVN_NO_ERROR;
}
svn_error_t *
svn_client_mergeinfo_log_merged(const char *path_or_url,
const svn_opt_revision_t *peg_revision,
const char *merge_source_path_or_url,
const svn_opt_revision_t *src_peg_revision,
svn_log_entry_receiver_t log_receiver,
void *log_receiver_baton,
svn_boolean_t discover_changed_paths,
const apr_array_header_t *revprops,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
const char *repos_root, *log_target = NULL, *merge_source_url;
svn_mergeinfo_t tgt_mergeinfo, source_history, mergeinfo;
apr_array_header_t *rangelist;
svn_opt_revision_t *real_src_peg_revision;
apr_hash_index_t *hi;
svn_revnum_t youngest_rev = SVN_INVALID_REVNUM;
SVN_ERR(location_from_path_and_rev(&merge_source_url, &real_src_peg_revision,
merge_source_path_or_url,
src_peg_revision, ctx, pool));
SVN_ERR(get_mergeinfo(&tgt_mergeinfo, &repos_root, path_or_url,
peg_revision, ctx, pool));
if (! tgt_mergeinfo)
return SVN_NO_ERROR;
SVN_ERR(svn_client__get_history_as_mergeinfo(&source_history,
merge_source_url,
real_src_peg_revision,
SVN_INVALID_REVNUM,
SVN_INVALID_REVNUM,
NULL, NULL, ctx, pool));
SVN_ERR(svn_mergeinfo_intersect(&mergeinfo, tgt_mergeinfo,
source_history, pool));
rangelist = apr_array_make(pool, 64, sizeof(svn_merge_range_t *));
for (hi = apr_hash_first(pool, mergeinfo); hi; hi = apr_hash_next(hi)) {
const void *key;
void *val;
svn_merge_range_t *range;
apr_array_header_t *list;
apr_hash_this(hi, &key, NULL, &val);
list = val;
range = APR_ARRAY_IDX(list, list->nelts - 1, svn_merge_range_t *);
if ((! SVN_IS_VALID_REVNUM(youngest_rev))
|| (range->end > youngest_rev)) {
youngest_rev = range->end;
log_target = key;
}
SVN_ERR(svn_rangelist_merge(&rangelist, list, pool));
}
if (! rangelist->nelts)
return SVN_NO_ERROR;
log_target = svn_path_url_add_component(repos_root, log_target + 1, pool);
return logs_for_mergeinfo_rangelist(log_target, rangelist,
discover_changed_paths, revprops,
log_receiver, log_receiver_baton,
ctx, pool);
}
svn_error_t *
svn_client_mergeinfo_get_merged(apr_hash_t **mergeinfo_p,
const char *path_or_url,
const svn_opt_revision_t *peg_revision,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
const char *repos_root;
apr_hash_t *full_path_mergeinfo;
svn_mergeinfo_t mergeinfo;
SVN_ERR(get_mergeinfo(&mergeinfo, &repos_root, path_or_url,
peg_revision, ctx, pool));
*mergeinfo_p = NULL;
if (mergeinfo) {
apr_hash_index_t *hi;
full_path_mergeinfo = apr_hash_make(pool);
for (hi = apr_hash_first(pool, mergeinfo); hi; hi = apr_hash_next(hi)) {
const void *key;
void *val;
const char *source_url;
apr_hash_this(hi, &key, NULL, &val);
source_url = svn_path_uri_encode(key, pool);
source_url = svn_path_join(repos_root, source_url + 1, pool);
apr_hash_set(full_path_mergeinfo, source_url,
APR_HASH_KEY_STRING, val);
}
*mergeinfo_p = full_path_mergeinfo;
}
return SVN_NO_ERROR;
}
svn_error_t *
svn_client_mergeinfo_log_eligible(const char *path_or_url,
const svn_opt_revision_t *peg_revision,
const char *merge_source_path_or_url,
const svn_opt_revision_t *src_peg_revision,
svn_log_entry_receiver_t log_receiver,
void *log_receiver_baton,
svn_boolean_t discover_changed_paths,
const apr_array_header_t *revprops,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
svn_mergeinfo_t mergeinfo, history, source_history, available;
apr_hash_index_t *hi;
svn_ra_session_t *ra_session;
svn_opt_revision_t *real_src_peg_revision;
const char *repos_root, *merge_source_url;
apr_pool_t *sesspool;
svn_revnum_t youngest_rev = SVN_INVALID_REVNUM;
apr_array_header_t *rangelist;
const char *log_target = NULL;
SVN_ERR(location_from_path_and_rev(&merge_source_url, &real_src_peg_revision,
merge_source_path_or_url,
src_peg_revision, ctx, pool));
SVN_ERR(get_mergeinfo(&mergeinfo, &repos_root, path_or_url,
peg_revision, ctx, pool));
SVN_ERR(svn_client__get_history_as_mergeinfo(&history,
path_or_url,
peg_revision,
SVN_INVALID_REVNUM,
SVN_INVALID_REVNUM,
NULL, NULL, ctx, pool));
if (! mergeinfo)
mergeinfo = history;
else
svn_mergeinfo_merge(mergeinfo, history, pool);
sesspool = svn_pool_create(pool);
SVN_ERR(svn_client__open_ra_session_internal(&ra_session, merge_source_url,
NULL, NULL, NULL, FALSE,
TRUE, ctx, sesspool));
SVN_ERR(svn_client__get_history_as_mergeinfo(&source_history,
merge_source_url,
real_src_peg_revision,
SVN_INVALID_REVNUM,
SVN_INVALID_REVNUM,
ra_session, NULL, ctx, pool));
svn_pool_destroy(sesspool);
SVN_ERR(svn_mergeinfo_remove(&available, mergeinfo, source_history, pool));
rangelist = apr_array_make(pool, 64, sizeof(svn_merge_range_t *));
for (hi = apr_hash_first(pool, available); hi; hi = apr_hash_next(hi)) {
const void *key;
void *val;
svn_merge_range_t *range;
apr_array_header_t *list;
apr_hash_this(hi, &key, NULL, &val);
list = val;
range = APR_ARRAY_IDX(list, list->nelts - 1, svn_merge_range_t *);
if ((! SVN_IS_VALID_REVNUM(youngest_rev))
|| (range->end > youngest_rev)) {
youngest_rev = range->end;
log_target = key;
}
SVN_ERR(svn_rangelist_merge(&rangelist, list, pool));
}
if (! rangelist->nelts)
return SVN_NO_ERROR;
log_target = svn_path_url_add_component(repos_root, log_target + 1, pool);
return logs_for_mergeinfo_rangelist(log_target, rangelist,
discover_changed_paths, revprops,
log_receiver, log_receiver_baton,
ctx, pool);
}
svn_error_t *
svn_client_suggest_merge_sources(apr_array_header_t **suggestions,
const char *path_or_url,
const svn_opt_revision_t *peg_revision,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
const char *repos_root;
const char *copyfrom_path;
apr_array_header_t *list;
svn_revnum_t copyfrom_rev;
svn_mergeinfo_t mergeinfo;
apr_hash_index_t *hi;
list = apr_array_make(pool, 1, sizeof(const char *));
SVN_ERR(get_mergeinfo(&mergeinfo, &repos_root, path_or_url,
peg_revision, ctx, pool));
SVN_ERR(svn_client__get_copy_source(path_or_url, peg_revision,
&copyfrom_path, &copyfrom_rev,
ctx, pool));
if (copyfrom_path) {
APR_ARRAY_PUSH(list, const char *) =
svn_path_url_add_component(repos_root, copyfrom_path + 1, pool);
}
if (mergeinfo) {
for (hi = apr_hash_first(NULL, mergeinfo); hi; hi = apr_hash_next(hi)) {
const void *key;
const char *rel_path;
apr_hash_this(hi, &key, NULL, NULL);
rel_path = key;
if (copyfrom_path == NULL || strcmp(rel_path, copyfrom_path) != 0)
APR_ARRAY_PUSH(list, const char *) = \
svn_path_url_add_component(repos_root, rel_path + 1, pool);
}
}
*suggestions = list;
return SVN_NO_ERROR;
}
