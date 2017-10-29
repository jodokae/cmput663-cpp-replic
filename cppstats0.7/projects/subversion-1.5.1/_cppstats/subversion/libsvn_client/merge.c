#include <apr_strings.h>
#include <apr_tables.h>
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
#include "svn_ra.h"
#include "client.h"
#include "mergeinfo.h"
#include <assert.h>
#include "private/svn_wc_private.h"
#include "private/svn_mergeinfo_private.h"
#include "svn_private_config.h"
#define ENSURE_VALID_REVISION_KINDS(rev1_kind, rev2_kind)
static svn_error_t *
check_scheme_match(svn_wc_adm_access_t *adm_access, const char *url) {
const char *path = svn_wc_adm_access_path(adm_access);
apr_pool_t *pool = svn_wc_adm_access_pool(adm_access);
const svn_wc_entry_t *ent;
const char *idx1, *idx2;
SVN_ERR(svn_wc_entry(&ent, path, adm_access, TRUE, pool));
idx1 = strchr(url, ':');
idx2 = strchr(ent->url, ':');
if ((idx1 == NULL) && (idx2 == NULL)) {
return svn_error_createf
(SVN_ERR_BAD_URL, NULL,
_("URLs have no scheme ('%s' and '%s')"), url, ent->url);
} else if (idx1 == NULL) {
return svn_error_createf
(SVN_ERR_BAD_URL, NULL,
_("URL has no scheme: '%s'"), url);
} else if (idx2 == NULL) {
return svn_error_createf
(SVN_ERR_BAD_URL, NULL,
_("URL has no scheme: '%s'"), ent->url);
} else if (((idx1 - url) != (idx2 - ent->url))
|| (strncmp(url, ent->url, idx1 - url) != 0)) {
return svn_error_createf
(SVN_ERR_UNSUPPORTED_FEATURE, NULL,
_("Access scheme mixtures not yet supported ('%s' and '%s')"),
url, ent->url);
}
return SVN_NO_ERROR;
}
typedef struct {
svn_string_t *working_mergeinfo_propval;
} working_mergeinfo_t;
typedef struct merge_cmd_baton_t {
svn_boolean_t force;
svn_boolean_t dry_run;
svn_boolean_t record_only;
svn_boolean_t sources_ancestral;
svn_boolean_t same_repos;
svn_boolean_t mergeinfo_capable;
svn_boolean_t ignore_ancestry;
svn_boolean_t target_missing_child;
const char *added_path;
const char *target;
const char *url;
svn_client_ctx_t *ctx;
svn_boolean_t add_necessitated_merge;
apr_hash_t *dry_run_deletions;
apr_hash_t *conflicted_paths;
const char *diff3_cmd;
const apr_array_header_t *merge_options;
svn_ra_session_t *ra_session1;
svn_ra_session_t *ra_session2;
svn_boolean_t target_has_dummy_merge_range;
apr_pool_t *pool;
} merge_cmd_baton_t;
apr_hash_t *
svn_client__dry_run_deletions(void *merge_cmd_baton) {
merge_cmd_baton_t *merge_b = merge_cmd_baton;
return merge_b->dry_run_deletions;
}
static APR_INLINE svn_boolean_t
dry_run_deleted_p(merge_cmd_baton_t *merge_b, const char *wcpath) {
return (merge_b->dry_run &&
apr_hash_get(merge_b->dry_run_deletions, wcpath,
APR_HASH_KEY_STRING) != NULL);
}
static APR_INLINE svn_boolean_t
is_path_conflicted_by_merge(merge_cmd_baton_t *merge_b) {
return (merge_b->conflicted_paths &&
apr_hash_count(merge_b->conflicted_paths) > 0);
}
static APR_INLINE void
mergeinfo_behavior(svn_boolean_t *honor_mergeinfo,
svn_boolean_t *record_mergeinfo,
merge_cmd_baton_t *merge_b) {
if (honor_mergeinfo)
*honor_mergeinfo = (merge_b->mergeinfo_capable
&& merge_b->sources_ancestral
&& merge_b->same_repos
&& (! merge_b->ignore_ancestry));
if (record_mergeinfo)
*record_mergeinfo = (merge_b->mergeinfo_capable
&& merge_b->sources_ancestral
&& merge_b->same_repos
&& (! merge_b->ignore_ancestry)
&& (! merge_b->dry_run));
}
static svn_error_t*
filter_self_referential_mergeinfo(apr_array_header_t **props,
const char *path,
merge_cmd_baton_t *merge_b,
svn_wc_adm_access_t *adm_access,
apr_pool_t *pool) {
svn_boolean_t honor_mergeinfo;
apr_array_header_t *adjusted_props;
int i;
mergeinfo_behavior(&honor_mergeinfo, NULL, merge_b);
if (! honor_mergeinfo)
return SVN_NO_ERROR;
adjusted_props = apr_array_make(pool, (*props)->nelts, sizeof(svn_prop_t));
for (i = 0; i < (*props)->nelts; ++i) {
svn_prop_t *prop = &APR_ARRAY_IDX((*props), i, svn_prop_t);
if ((strcmp(prop->name, SVN_PROP_MERGEINFO) != 0)
|| (! prop->value)
|| (! prop->value->len)) {
APR_ARRAY_PUSH(adjusted_props, svn_prop_t) = *prop;
} else {
svn_mergeinfo_t mergeinfo, filtered_mergeinfo = NULL;
apr_hash_index_t *hi;
const char *target_url, *merge_source_root_url;
const svn_wc_entry_t *target_entry;
const char *old_url = NULL;
SVN_ERR(svn_ra_get_repos_root2(merge_b->ra_session2,
&merge_source_root_url, pool));
SVN_ERR(svn_wc__entry_versioned(&target_entry, path, adm_access,
FALSE, pool));
SVN_ERR(svn_client_url_from_path(&target_url, path, pool));
SVN_ERR(svn_client__ensure_ra_session_url(&old_url,
merge_b->ra_session2,
target_url, pool));
SVN_ERR(svn_mergeinfo_parse(&mergeinfo, prop->value->data, pool));
for (hi = apr_hash_first(NULL, mergeinfo);
hi; hi = apr_hash_next(hi)) {
int j;
const void *key;
void *value;
const char *source_path;
apr_array_header_t *rangelist;
const char *merge_source_url;
apr_array_header_t *adjusted_rangelist =
apr_array_make(pool, 0, sizeof(svn_merge_range_t *));
apr_hash_this(hi, &key, NULL, &value);
source_path = key;
rangelist = value;
merge_source_url = svn_path_join(merge_source_root_url,
source_path + 1, pool);
for (j = 0; j < rangelist->nelts; j++) {
svn_error_t *err;
svn_opt_revision_t *start_revision, *end_revision;
const char *start_url, *end_url;
svn_opt_revision_t peg_rev, rev1_opt, rev2_opt;
svn_merge_range_t *range =
APR_ARRAY_IDX(rangelist, j, svn_merge_range_t *);
peg_rev.kind = svn_opt_revision_number;
peg_rev.value.number = target_entry->revision;
rev1_opt.kind = svn_opt_revision_number;
rev1_opt.value.number = range->start + 1;
rev2_opt.kind = svn_opt_revision_unspecified;
err = svn_client__repos_locations(&start_url,
&start_revision,
&end_url,
&end_revision,
merge_b->ra_session2,
target_url,
&peg_rev,
&rev1_opt,
&rev2_opt,
merge_b->ctx,
pool);
if (err) {
if (err->apr_err == SVN_ERR_CLIENT_UNRELATED_RESOURCES
|| err->apr_err == SVN_ERR_RA_DAV_PATH_NOT_FOUND
|| err->apr_err == SVN_ERR_FS_NOT_FOUND) {
svn_error_clear(err);
err = NULL;
APR_ARRAY_PUSH(adjusted_rangelist,
svn_merge_range_t *) = range;
} else {
return err;
}
} else {
if (strcmp(start_url, merge_source_url) != 0) {
APR_ARRAY_PUSH(adjusted_rangelist,
svn_merge_range_t *) = range;
}
}
}
if (adjusted_rangelist->nelts) {
if (!filtered_mergeinfo)
filtered_mergeinfo = apr_hash_make(pool);
apr_hash_set(filtered_mergeinfo, source_path,
APR_HASH_KEY_STRING, adjusted_rangelist);
}
}
if (filtered_mergeinfo) {
svn_string_t *filtered_mergeinfo_str;
svn_prop_t *adjusted_prop = apr_pcalloc(pool,
sizeof(*adjusted_prop));
SVN_ERR(svn_mergeinfo_to_string(&filtered_mergeinfo_str,
filtered_mergeinfo,
pool));
adjusted_prop->name = SVN_PROP_MERGEINFO;
adjusted_prop->value = filtered_mergeinfo_str;
APR_ARRAY_PUSH(adjusted_props, svn_prop_t) = *adjusted_prop;
}
if (old_url)
SVN_ERR(svn_ra_reparent(merge_b->ra_session2, old_url, pool));
}
}
*props = adjusted_props;
return SVN_NO_ERROR;
}
static svn_error_t *
merge_props_changed(svn_wc_adm_access_t *adm_access,
svn_wc_notify_state_t *state,
const char *path,
const apr_array_header_t *propchanges,
apr_hash_t *original_props,
void *baton) {
apr_array_header_t *props;
merge_cmd_baton_t *merge_b = baton;
svn_client_ctx_t *ctx = merge_b->ctx;
apr_pool_t *subpool = svn_pool_create(merge_b->pool);
svn_error_t *err;
SVN_ERR(svn_categorize_props(propchanges, NULL, NULL, &props, subpool));
if (props->nelts) {
if (svn_path_compare_paths(svn_wc_adm_access_path(adm_access),
path) != 0)
SVN_ERR(svn_wc_adm_probe_try3(&adm_access, adm_access, path,
TRUE, -1, ctx->cancel_func,
ctx->cancel_baton, subpool));
SVN_ERR(filter_self_referential_mergeinfo(&props, path, merge_b,
adm_access, subpool));
err = svn_wc_merge_props2(state, path, adm_access, original_props, props,
FALSE, merge_b->dry_run, ctx->conflict_func,
ctx->conflict_baton, subpool);
if (err && (err->apr_err == SVN_ERR_ENTRY_NOT_FOUND
|| err->apr_err == SVN_ERR_UNVERSIONED_RESOURCE)) {
if (state)
*state = svn_wc_notify_state_missing;
svn_error_clear(err);
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
} else if (err)
return err;
}
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
typedef struct {
svn_wc_conflict_resolver_func_t wrapped_func;
void *wrapped_baton;
apr_hash_t **conflicted_paths;
apr_pool_t *pool;
} conflict_resolver_baton_t;
static svn_error_t *
conflict_resolver(svn_wc_conflict_result_t **result,
const svn_wc_conflict_description_t *description,
void *baton, apr_pool_t *pool) {
svn_error_t *err;
conflict_resolver_baton_t *conflict_b = baton;
if (conflict_b->wrapped_func)
err = (*conflict_b->wrapped_func)(result, description,
conflict_b->wrapped_baton, pool);
else {
*result = svn_wc_create_conflict_result(svn_wc_conflict_choose_postpone,
NULL, pool);
err = SVN_NO_ERROR;
}
if ((! conflict_b->wrapped_func)
|| (*result && ((*result)->choice == svn_wc_conflict_choose_postpone))) {
const char *conflicted_path = apr_pstrdup(conflict_b->pool,
description->path);
if (*conflict_b->conflicted_paths == NULL)
*conflict_b->conflicted_paths = apr_hash_make(conflict_b->pool);
apr_hash_set(*conflict_b->conflicted_paths, conflicted_path,
APR_HASH_KEY_STRING, conflicted_path);
}
return err;
}
static svn_error_t *
merge_file_changed(svn_wc_adm_access_t *adm_access,
svn_wc_notify_state_t *content_state,
svn_wc_notify_state_t *prop_state,
const char *mine,
const char *older,
const char *yours,
svn_revnum_t older_rev,
svn_revnum_t yours_rev,
const char *mimetype1,
const char *mimetype2,
const apr_array_header_t *prop_changes,
apr_hash_t *original_props,
void *baton) {
merge_cmd_baton_t *merge_b = baton;
apr_pool_t *subpool = svn_pool_create(merge_b->pool);
svn_boolean_t merge_required = TRUE;
enum svn_wc_merge_outcome_t merge_outcome;
if (adm_access == NULL) {
if (content_state)
*content_state = svn_wc_notify_state_missing;
if (prop_state)
*prop_state = svn_wc_notify_state_missing;
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
{
const svn_wc_entry_t *entry;
svn_node_kind_t kind;
SVN_ERR(svn_wc_entry(&entry, mine, adm_access, FALSE, subpool));
SVN_ERR(svn_io_check_path(mine, &kind, subpool));
if ((! entry) || (kind != svn_node_file)) {
if (content_state)
*content_state = svn_wc_notify_state_missing;
if (prop_state)
*prop_state = svn_wc_notify_state_missing;
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
}
if (prop_changes->nelts > 0)
SVN_ERR(merge_props_changed(adm_access, prop_state, mine, prop_changes,
original_props, baton));
else if (prop_state)
*prop_state = svn_wc_notify_state_unchanged;
if (older) {
svn_boolean_t has_local_mods;
SVN_ERR(svn_wc_text_modified_p(&has_local_mods, mine, FALSE,
adm_access, subpool));
if ((mimetype1 && svn_mime_type_is_binary(mimetype1))
|| (mimetype2 && svn_mime_type_is_binary(mimetype2))) {
svn_boolean_t older_revision_exists =
!merge_b->add_necessitated_merge;
svn_boolean_t same_contents;
SVN_ERR(svn_io_files_contents_same_p(&same_contents,
(older_revision_exists ?
older : yours),
mine, subpool));
if (same_contents) {
if (older_revision_exists && !merge_b->dry_run)
SVN_ERR(svn_io_file_rename(yours, mine, subpool));
merge_outcome = svn_wc_merge_merged;
merge_required = FALSE;
}
}
if (merge_required) {
const char *target_label = _(".working");
const char *left_label = apr_psprintf(subpool,
_(".merge-left.r%ld"),
older_rev);
const char *right_label = apr_psprintf(subpool,
_(".merge-right.r%ld"),
yours_rev);
conflict_resolver_baton_t conflict_baton = {
merge_b->ctx->conflict_func, merge_b->ctx->conflict_baton,
&merge_b->conflicted_paths, merge_b->pool
};
SVN_ERR(svn_wc_merge3(&merge_outcome,
older, yours, mine, adm_access,
left_label, right_label, target_label,
merge_b->dry_run, merge_b->diff3_cmd,
merge_b->merge_options, prop_changes,
conflict_resolver, &conflict_baton,
subpool));
}
if (content_state) {
if (merge_outcome == svn_wc_merge_conflict)
*content_state = svn_wc_notify_state_conflicted;
else if (has_local_mods
&& merge_outcome != svn_wc_merge_unchanged)
*content_state = svn_wc_notify_state_merged;
else if (merge_outcome == svn_wc_merge_merged)
*content_state = svn_wc_notify_state_changed;
else if (merge_outcome == svn_wc_merge_no_merge)
*content_state = svn_wc_notify_state_missing;
else
*content_state = svn_wc_notify_state_unchanged;
}
}
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
static svn_error_t *
merge_file_added(svn_wc_adm_access_t *adm_access,
svn_wc_notify_state_t *content_state,
svn_wc_notify_state_t *prop_state,
const char *mine,
const char *older,
const char *yours,
svn_revnum_t rev1,
svn_revnum_t rev2,
const char *mimetype1,
const char *mimetype2,
const apr_array_header_t *prop_changes,
apr_hash_t *original_props,
void *baton) {
merge_cmd_baton_t *merge_b = baton;
apr_pool_t *subpool = svn_pool_create(merge_b->pool);
svn_node_kind_t kind;
int i;
apr_hash_t *new_props;
if (prop_state)
*prop_state = svn_wc_notify_state_unknown;
new_props = apr_hash_copy(subpool, original_props);
for (i = 0; i < prop_changes->nelts; ++i) {
const svn_prop_t *prop = &APR_ARRAY_IDX(prop_changes, i, svn_prop_t);
if (svn_property_kind(NULL, prop->name) == svn_prop_wc_kind)
continue;
apr_hash_set(new_props, prop->name, APR_HASH_KEY_STRING, prop->value);
}
if (! adm_access) {
if (merge_b->dry_run && merge_b->added_path
&& svn_path_is_child(merge_b->added_path, mine, subpool)) {
if (content_state)
*content_state = svn_wc_notify_state_changed;
if (prop_state && apr_hash_count(new_props))
*prop_state = svn_wc_notify_state_changed;
} else
*content_state = svn_wc_notify_state_missing;
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
SVN_ERR(svn_io_check_path(mine, &kind, subpool));
switch (kind) {
case svn_node_none: {
const svn_wc_entry_t *entry;
SVN_ERR(svn_wc_entry(&entry, mine, adm_access, FALSE, subpool));
if (entry && entry->schedule != svn_wc_schedule_delete) {
if (content_state)
*content_state = svn_wc_notify_state_obstructed;
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
if (! merge_b->dry_run) {
const char *copyfrom_url = NULL;
svn_revnum_t copyfrom_rev = SVN_INVALID_REVNUM;
if (merge_b->same_repos) {
const char *child = svn_path_is_child(merge_b->target,
mine, subpool);
if (child != NULL)
copyfrom_url = svn_path_url_add_component(merge_b->url,
child, subpool);
else
copyfrom_url = merge_b->url;
copyfrom_rev = rev2;
SVN_ERR(check_scheme_match(adm_access, copyfrom_url));
}
SVN_ERR(svn_wc_add_repos_file2(mine, adm_access, yours, NULL,
new_props, NULL, copyfrom_url,
copyfrom_rev, subpool));
}
if (content_state)
*content_state = svn_wc_notify_state_changed;
if (prop_state && apr_hash_count(new_props))
*prop_state = svn_wc_notify_state_changed;
}
break;
case svn_node_dir:
if (content_state) {
const svn_wc_entry_t *entry;
SVN_ERR(svn_wc_entry(&entry, mine, adm_access, FALSE, subpool));
if (entry && dry_run_deleted_p(merge_b, mine))
*content_state = svn_wc_notify_state_changed;
else
*content_state = svn_wc_notify_state_obstructed;
}
break;
case svn_node_file: {
const svn_wc_entry_t *entry;
SVN_ERR(svn_wc_entry(&entry, mine, adm_access, FALSE, subpool));
if (!entry || entry->schedule == svn_wc_schedule_delete) {
if (content_state)
*content_state = svn_wc_notify_state_obstructed;
} else {
if (dry_run_deleted_p(merge_b, mine)) {
if (content_state)
*content_state = svn_wc_notify_state_changed;
} else {
merge_b->add_necessitated_merge = TRUE;
SVN_ERR(merge_file_changed(adm_access, content_state,
prop_state, mine, older, yours,
rev1, rev2,
mimetype1, mimetype2,
prop_changes, original_props,
baton));
merge_b->add_necessitated_merge = FALSE;
}
}
break;
}
default:
if (content_state)
*content_state = svn_wc_notify_state_unknown;
break;
}
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
static svn_error_t *
merge_file_deleted(svn_wc_adm_access_t *adm_access,
svn_wc_notify_state_t *state,
const char *mine,
const char *older,
const char *yours,
const char *mimetype1,
const char *mimetype2,
apr_hash_t *original_props,
void *baton) {
merge_cmd_baton_t *merge_b = baton;
apr_pool_t *subpool = svn_pool_create(merge_b->pool);
svn_node_kind_t kind;
svn_wc_adm_access_t *parent_access;
const char *parent_path;
svn_error_t *err;
if (! adm_access) {
if (state)
*state = svn_wc_notify_state_missing;
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
SVN_ERR(svn_io_check_path(mine, &kind, subpool));
switch (kind) {
case svn_node_file:
svn_path_split(mine, &parent_path, NULL, subpool);
SVN_ERR(svn_wc_adm_retrieve(&parent_access, adm_access, parent_path,
subpool));
err = svn_client__wc_delete(mine, parent_access, merge_b->force,
merge_b->dry_run, FALSE, NULL, NULL,
merge_b->ctx, subpool);
if (err && state) {
*state = svn_wc_notify_state_obstructed;
svn_error_clear(err);
} else if (state) {
*state = svn_wc_notify_state_changed;
}
break;
case svn_node_dir:
if (state)
*state = svn_wc_notify_state_obstructed;
break;
case svn_node_none:
if (state)
*state = svn_wc_notify_state_missing;
break;
default:
if (state)
*state = svn_wc_notify_state_unknown;
break;
}
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
static svn_error_t *
merge_dir_added(svn_wc_adm_access_t *adm_access,
svn_wc_notify_state_t *state,
const char *path,
svn_revnum_t rev,
void *baton) {
merge_cmd_baton_t *merge_b = baton;
apr_pool_t *subpool = svn_pool_create(merge_b->pool);
svn_node_kind_t kind;
const svn_wc_entry_t *entry;
const char *copyfrom_url = NULL, *child;
svn_revnum_t copyfrom_rev = SVN_INVALID_REVNUM;
if (! adm_access) {
if (state) {
if (merge_b->dry_run && merge_b->added_path
&& svn_path_is_child(merge_b->added_path, path, subpool))
*state = svn_wc_notify_state_changed;
else
*state = svn_wc_notify_state_missing;
}
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
child = svn_path_is_child(merge_b->target, path, subpool);
assert(child != NULL);
if (merge_b->same_repos) {
copyfrom_url = svn_path_url_add_component(merge_b->url, child, subpool);
copyfrom_rev = rev;
SVN_ERR(check_scheme_match(adm_access, copyfrom_url));
}
SVN_ERR(svn_io_check_path(path, &kind, subpool));
switch (kind) {
case svn_node_none:
SVN_ERR(svn_wc_entry(&entry, path, adm_access, FALSE, subpool));
if (entry && entry->schedule != svn_wc_schedule_delete) {
if (state)
*state = svn_wc_notify_state_obstructed;
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
if (merge_b->dry_run)
merge_b->added_path = apr_pstrdup(merge_b->pool, path);
else {
SVN_ERR(svn_io_make_dir_recursively(path, subpool));
SVN_ERR(svn_wc_add2(path, adm_access,
copyfrom_url, copyfrom_rev,
merge_b->ctx->cancel_func,
merge_b->ctx->cancel_baton,
NULL, NULL,
subpool));
}
if (state)
*state = svn_wc_notify_state_changed;
break;
case svn_node_dir:
SVN_ERR(svn_wc_entry(&entry, path, adm_access, TRUE, subpool));
if (! entry || entry->schedule == svn_wc_schedule_delete) {
if (!merge_b->dry_run)
SVN_ERR(svn_wc_add2(path, adm_access,
copyfrom_url, copyfrom_rev,
merge_b->ctx->cancel_func,
merge_b->ctx->cancel_baton,
NULL, NULL,
subpool));
else
merge_b->added_path = apr_pstrdup(merge_b->pool, path);
if (state)
*state = svn_wc_notify_state_changed;
} else if (state) {
if (dry_run_deleted_p(merge_b, path))
*state = svn_wc_notify_state_changed;
else
*state = svn_wc_notify_state_obstructed;
}
break;
case svn_node_file:
if (merge_b->dry_run)
merge_b->added_path = NULL;
if (state) {
SVN_ERR(svn_wc_entry(&entry, path, adm_access, FALSE, subpool));
if (entry && dry_run_deleted_p(merge_b, path))
*state = svn_wc_notify_state_changed;
else
*state = svn_wc_notify_state_obstructed;
}
break;
default:
if (merge_b->dry_run)
merge_b->added_path = NULL;
if (state)
*state = svn_wc_notify_state_unknown;
break;
}
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
static svn_error_t *
merge_dir_deleted(svn_wc_adm_access_t *adm_access,
svn_wc_notify_state_t *state,
const char *path,
void *baton) {
merge_cmd_baton_t *merge_b = baton;
apr_pool_t *subpool = svn_pool_create(merge_b->pool);
svn_node_kind_t kind;
svn_wc_adm_access_t *parent_access;
const char *parent_path;
svn_error_t *err;
if (! adm_access) {
if (state)
*state = svn_wc_notify_state_missing;
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
SVN_ERR(svn_io_check_path(path, &kind, subpool));
switch (kind) {
case svn_node_dir: {
svn_path_split(path, &parent_path, NULL, subpool);
SVN_ERR(svn_wc_adm_retrieve(&parent_access, adm_access, parent_path,
subpool));
err = svn_client__wc_delete(path, parent_access, merge_b->force,
merge_b->dry_run, FALSE,
NULL, NULL,
merge_b->ctx, subpool);
if (err && state) {
*state = svn_wc_notify_state_obstructed;
svn_error_clear(err);
} else if (state) {
*state = svn_wc_notify_state_changed;
}
}
break;
case svn_node_file:
if (state)
*state = svn_wc_notify_state_obstructed;
break;
case svn_node_none:
if (state)
*state = svn_wc_notify_state_missing;
break;
default:
if (state)
*state = svn_wc_notify_state_unknown;
break;
}
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
static const svn_wc_diff_callbacks2_t
merge_callbacks = {
merge_file_changed,
merge_file_added,
merge_file_deleted,
merge_dir_added,
merge_dir_deleted,
merge_props_changed
};
typedef struct {
svn_wc_notify_func2_t wrapped_func;
void *wrapped_baton;
apr_uint32_t nbr_notifications;
apr_uint32_t nbr_operative_notifications;
apr_hash_t *merged_paths;
apr_hash_t *skipped_paths;
apr_hash_t *added_paths;
svn_boolean_t is_single_file_merge;
apr_array_header_t *children_with_mergeinfo;
int cur_ancestor_index;
merge_cmd_baton_t *merge_b;
apr_pool_t *pool;
} notification_receiver_baton_t;
static int
find_nearest_ancestor(apr_array_header_t *children_with_mergeinfo,
svn_boolean_t path_is_own_ancestor,
const char *path) {
int i;
int ancestor_index = 0;
if (!children_with_mergeinfo)
return 0;
for (i = 0; i < children_with_mergeinfo->nelts; i++) {
svn_client__merge_path_t *child =
APR_ARRAY_IDX(children_with_mergeinfo, i, svn_client__merge_path_t *);
if (svn_path_is_ancestor(child->path, path)
&& (svn_path_compare_paths(child->path, path) != 0
|| path_is_own_ancestor))
ancestor_index = i;
}
return ancestor_index;
}
#define IS_OPERATIVE_NOTIFICATION(notify) (notify->content_state == svn_wc_notify_state_conflicted || notify->content_state == svn_wc_notify_state_merged || notify->content_state == svn_wc_notify_state_changed || notify->prop_state == svn_wc_notify_state_conflicted || notify->prop_state == svn_wc_notify_state_merged || notify->prop_state == svn_wc_notify_state_changed || notify->action == svn_wc_notify_update_add)
static void
notification_receiver(void *baton, const svn_wc_notify_t *notify,
apr_pool_t *pool) {
notification_receiver_baton_t *notify_b = baton;
svn_boolean_t is_operative_notification = FALSE;
if (IS_OPERATIVE_NOTIFICATION(notify)) {
notify_b->nbr_operative_notifications++;
is_operative_notification = TRUE;
}
if (notify_b->merge_b->sources_ancestral) {
notify_b->nbr_notifications++;
if (!(notify_b->is_single_file_merge) && is_operative_notification) {
int new_nearest_ancestor_index =
find_nearest_ancestor(
notify_b->children_with_mergeinfo,
notify->action == svn_wc_notify_update_delete ? FALSE : TRUE,
notify->path);
if (new_nearest_ancestor_index != notify_b->cur_ancestor_index) {
svn_client__merge_path_t *child =
APR_ARRAY_IDX(notify_b->children_with_mergeinfo,
new_nearest_ancestor_index,
svn_client__merge_path_t *);
notify_b->cur_ancestor_index = new_nearest_ancestor_index;
if (!child->absent && child->remaining_ranges->nelts > 0
&& !(new_nearest_ancestor_index == 0
&& notify_b->merge_b->target_has_dummy_merge_range)) {
svn_wc_notify_t *notify_merge_begin;
notify_merge_begin =
svn_wc_create_notify(child->path,
notify_b->merge_b->same_repos
? svn_wc_notify_merge_begin
: svn_wc_notify_foreign_merge_begin,
pool);
notify_merge_begin->merge_range =
APR_ARRAY_IDX(child->remaining_ranges, 0,
svn_merge_range_t *);
if (notify_b->wrapped_func)
(*notify_b->wrapped_func)(notify_b->wrapped_baton,
notify_merge_begin, pool);
}
}
}
if (notify->content_state == svn_wc_notify_state_merged
|| notify->content_state == svn_wc_notify_state_changed
|| notify->prop_state == svn_wc_notify_state_merged
|| notify->prop_state == svn_wc_notify_state_changed
|| notify->action == svn_wc_notify_update_add) {
const char *merged_path = apr_pstrdup(notify_b->pool, notify->path);
if (notify_b->merged_paths == NULL)
notify_b->merged_paths = apr_hash_make(notify_b->pool);
apr_hash_set(notify_b->merged_paths, merged_path,
APR_HASH_KEY_STRING, merged_path);
}
if (notify->action == svn_wc_notify_skip) {
const char *skipped_path = apr_pstrdup(notify_b->pool, notify->path);
if (notify_b->skipped_paths == NULL)
notify_b->skipped_paths = apr_hash_make(notify_b->pool);
apr_hash_set(notify_b->skipped_paths, skipped_path,
APR_HASH_KEY_STRING, skipped_path);
}
if (notify->action == svn_wc_notify_update_add) {
svn_boolean_t is_root_of_added_subtree = FALSE;
const char *added_path = apr_pstrdup(notify_b->pool, notify->path);
const char *added_path_parent = NULL;
if (notify_b->added_paths == NULL) {
notify_b->added_paths = apr_hash_make(notify_b->pool);
is_root_of_added_subtree = TRUE;
} else {
added_path_parent = svn_path_dirname(added_path, pool);
if (!apr_hash_get(notify_b->added_paths, added_path_parent,
APR_HASH_KEY_STRING))
is_root_of_added_subtree = TRUE;
}
if (is_root_of_added_subtree)
apr_hash_set(notify_b->added_paths, added_path,
APR_HASH_KEY_STRING, added_path);
}
}
else if (!(notify_b->is_single_file_merge)
&& notify_b->nbr_operative_notifications == 1
&& is_operative_notification) {
svn_wc_notify_t *notify_merge_begin;
notify_merge_begin =
svn_wc_create_notify(notify_b->merge_b->target,
notify_b->merge_b->same_repos
? svn_wc_notify_merge_begin
: svn_wc_notify_foreign_merge_begin,
pool);
if (notify_b->wrapped_func)
(*notify_b->wrapped_func)(notify_b->wrapped_baton, notify_merge_begin,
pool);
}
if (notify_b->wrapped_func)
(*notify_b->wrapped_func)(notify_b->wrapped_baton, notify, pool);
}
static apr_array_header_t *
init_rangelist(svn_revnum_t start,
svn_revnum_t end,
svn_boolean_t inheritable,
apr_pool_t *pool) {
apr_array_header_t *rangelist =
apr_array_make(pool, 1, sizeof(svn_merge_range_t *));
svn_merge_range_t *range = apr_pcalloc(pool, sizeof(*range));
range->start = start;
range->end = end;
range->inheritable = inheritable;
APR_ARRAY_PUSH(rangelist, svn_merge_range_t *) = range;
return rangelist;
}
static void
push_range(apr_array_header_t *rangelist,
svn_revnum_t start,
svn_revnum_t end,
svn_boolean_t inheritable,
apr_pool_t *pool) {
svn_merge_range_t *range = apr_pcalloc(pool, sizeof(*range));
range->start = start;
range->end = end;
range->inheritable = inheritable;
APR_ARRAY_PUSH(rangelist, svn_merge_range_t *) = range;
}
static svn_error_t *
prepare_subtree_ranges(apr_array_header_t **requested_rangelist,
svn_boolean_t *child_deleted_or_nonexistant,
const char *mergeinfo_path,
svn_client__merge_path_t *parent,
svn_revnum_t revision1,
svn_revnum_t revision2,
const char *primary_url,
svn_ra_session_t *ra_session,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
svn_boolean_t is_rollback = revision2 < revision1;
svn_revnum_t peg_rev = is_rollback ? revision1 : revision2;
svn_revnum_t start_rev = is_rollback ? revision1 : revision2;
svn_revnum_t end_rev = is_rollback ? revision2 : revision1;
apr_array_header_t *segments;
const char *rel_source_path;
const char *session_url;
svn_error_t *err;
SVN_ERR(svn_ra_get_session_url(ra_session, &session_url, pool));
SVN_ERR(svn_client__path_relative_to_root(&rel_source_path,
primary_url,
session_url,
FALSE,
ra_session,
NULL,
pool));
err = svn_client__repos_location_segments(&segments, ra_session,
rel_source_path, peg_rev,
start_rev, end_rev, ctx, pool);
if (err) {
if (err->apr_err == SVN_ERR_FS_NOT_FOUND
|| err->apr_err == SVN_ERR_RA_DAV_REQUEST_FAILED) {
svn_error_clear(err);
if (is_rollback) {
svn_dirent_t *dirent;
SVN_ERR(svn_ra_stat(ra_session, rel_source_path,
revision2, &dirent, pool));
if (dirent)
*child_deleted_or_nonexistant = FALSE;
else
*child_deleted_or_nonexistant = TRUE;
} else {
*child_deleted_or_nonexistant = TRUE;
}
*requested_rangelist = init_rangelist(revision1, revision2,
TRUE, pool);
} else
return err;
} else {
if (segments->nelts) {
svn_location_segment_t *segment =
APR_ARRAY_IDX(segments, (segments->nelts - 1),
svn_location_segment_t *);
if (is_rollback) {
if (segment->range_start == revision2
&& segment->range_end == revision1) {
*requested_rangelist = init_rangelist(revision1, revision2,
TRUE, pool);
*child_deleted_or_nonexistant = FALSE;
} else {
*requested_rangelist = init_rangelist(revision1, revision2,
TRUE, pool);
*child_deleted_or_nonexistant = TRUE;
}
} else {
if (segment->range_start == revision1
&& segment->range_end == revision2) {
*requested_rangelist = init_rangelist(revision1, revision2,
TRUE, pool);
*child_deleted_or_nonexistant = FALSE;
} else
{
int i;
apr_array_header_t *predate_intersection_rangelist;
apr_array_header_t *different_name_rangelist =
apr_array_make(pool, 1, sizeof(svn_merge_range_t *));
apr_array_header_t *predate_rangelist =
init_rangelist(revision1,
segment->range_start,
TRUE, pool);
SVN_ERR(svn_rangelist_intersect(
&predate_intersection_rangelist,
predate_rangelist,
parent->remaining_ranges,
FALSE, pool));
*requested_rangelist =
init_rangelist(segment->range_start,
revision2,
TRUE, pool);
SVN_ERR(svn_rangelist_merge(
requested_rangelist, predate_intersection_rangelist,
pool));
for (i = 0; i < segments->nelts; i++) {
segment =
APR_ARRAY_IDX(segments, i, svn_location_segment_t *);
if (segment->path
&& strcmp(segment->path, mergeinfo_path + 1) != 0)
push_range(different_name_rangelist,
segment->range_start,
segment->range_end, TRUE, pool);
}
if (different_name_rangelist->nelts)
SVN_ERR(svn_rangelist_remove(requested_rangelist,
different_name_rangelist,
*requested_rangelist, FALSE,
pool));
*child_deleted_or_nonexistant = FALSE;
}
}
}
}
return SVN_NO_ERROR;
}
static svn_error_t *
filter_merged_revisions(svn_client__merge_path_t *parent,
svn_client__merge_path_t *child,
const char *mergeinfo_path,
svn_mergeinfo_t target_mergeinfo,
svn_mergeinfo_t implicit_mergeinfo,
svn_revnum_t revision1,
svn_revnum_t revision2,
const char *primary_url,
svn_ra_session_t *ra_session,
svn_boolean_t is_subtree,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
apr_array_header_t *target_rangelist = NULL;
svn_mergeinfo_t mergeinfo = implicit_mergeinfo;
apr_array_header_t *requested_merge;
if (is_subtree) {
svn_boolean_t child_deleted_or_nonexistant;
SVN_ERR(prepare_subtree_ranges(&requested_merge,
&child_deleted_or_nonexistant,
mergeinfo_path, parent,
revision1, revision2,
primary_url, ra_session, ctx, pool));
if (child_deleted_or_nonexistant && parent) {
child->remaining_ranges =
svn_rangelist_dup(parent->remaining_ranges, pool);
return SVN_NO_ERROR;
}
} else {
requested_merge = init_rangelist(revision1, revision2, TRUE, pool);
}
if (revision1 > revision2) {
if (target_mergeinfo) {
mergeinfo = svn_mergeinfo_dup(implicit_mergeinfo, pool);
SVN_ERR(svn_mergeinfo_merge(mergeinfo, target_mergeinfo, pool));
}
target_rangelist = apr_hash_get(mergeinfo,
mergeinfo_path, APR_HASH_KEY_STRING);
if (target_rangelist) {
SVN_ERR(svn_rangelist_reverse(requested_merge, pool));
SVN_ERR(svn_rangelist_intersect(&(child->remaining_ranges),
target_rangelist,
requested_merge, FALSE, pool));
SVN_ERR(svn_rangelist_reverse(child->remaining_ranges, pool));
} else {
child->remaining_ranges =
apr_array_make(pool, 1, sizeof(svn_merge_range_t *));
}
} else {
child->remaining_ranges = requested_merge;
#if defined(SVN_MERGE__ALLOW_ALL_FORWARD_MERGES_FROM_SELF)
if (target_mergeinfo)
target_rangelist = apr_hash_get(target_mergeinfo,
mergeinfo_path, APR_HASH_KEY_STRING);
#else
if (target_mergeinfo) {
mergeinfo = svn_mergeinfo_dup(implicit_mergeinfo, pool);
SVN_ERR(svn_mergeinfo_merge(mergeinfo, target_mergeinfo, pool));
}
target_rangelist = apr_hash_get(mergeinfo,
mergeinfo_path, APR_HASH_KEY_STRING);
#endif
if (target_rangelist)
SVN_ERR(svn_rangelist_remove(&(child->remaining_ranges),
target_rangelist,
requested_merge, FALSE, pool));
}
return SVN_NO_ERROR;
}
static svn_error_t *
calculate_remaining_ranges(svn_client__merge_path_t *parent,
svn_client__merge_path_t *child,
const char *source_root_url,
const char *url1,
svn_revnum_t revision1,
const char *url2,
svn_revnum_t revision2,
svn_mergeinfo_t target_mergeinfo,
svn_mergeinfo_t implicit_mergeinfo,
svn_boolean_t is_subtree,
svn_ra_session_t *ra_session,
const svn_wc_entry_t *entry,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
const char *mergeinfo_path;
const char *primary_url = (revision1 < revision2) ? url2 : url1;
SVN_ERR(svn_client__path_relative_to_root(&mergeinfo_path, primary_url,
source_root_url, TRUE,
ra_session, NULL, pool));
SVN_ERR(filter_merged_revisions(parent, child, mergeinfo_path,
target_mergeinfo, implicit_mergeinfo,
revision1, revision2, primary_url,
ra_session, is_subtree, ctx, pool));
if (((child->remaining_ranges)->nelts == 0)
&& (revision2 < revision1)
&& (entry->revision <= revision2)) {
svn_error_t *err;
const char *start_url;
svn_opt_revision_t requested, unspec, pegrev, *start_revision;
unspec.kind = svn_opt_revision_unspecified;
requested.kind = svn_opt_revision_number;
requested.value.number = entry->revision;
pegrev.kind = svn_opt_revision_number;
pegrev.value.number = revision1;
err = svn_client__repos_locations(&start_url, &start_revision,
NULL, NULL, ra_session, url1,
&pegrev, &requested,
&unspec, ctx, pool);
if (err) {
if (err->apr_err == SVN_ERR_FS_NOT_FOUND
|| err->apr_err == SVN_ERR_RA_DAV_PATH_NOT_FOUND
|| err->apr_err == SVN_ERR_CLIENT_UNRELATED_RESOURCES)
svn_error_clear(err);
else
return err;
} else if (strcmp(start_url, entry->url) == 0) {
return svn_error_create(SVN_ERR_CLIENT_NOT_READY_TO_MERGE, NULL,
_("Cannot reverse-merge a range from a "
"path's own future history; try "
"updating first"));
}
}
return SVN_NO_ERROR;
}
static svn_error_t *
get_full_mergeinfo(svn_mergeinfo_t *recorded_mergeinfo,
svn_mergeinfo_t *implicit_mergeinfo,
const svn_wc_entry_t *entry,
svn_boolean_t *indirect,
svn_mergeinfo_inheritance_t inherit,
svn_ra_session_t *ra_session,
const char *target_wcpath,
svn_revnum_t start,
svn_revnum_t end,
svn_wc_adm_access_t *adm_access,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
const char *session_url = NULL, *url;
svn_revnum_t target_rev;
svn_opt_revision_t peg_revision;
apr_pool_t *sesspool = NULL;
assert(SVN_IS_VALID_REVNUM(start)
&& SVN_IS_VALID_REVNUM(end)
&& (start > end));
SVN_ERR(svn_client__get_wc_or_repos_mergeinfo(recorded_mergeinfo, entry,
indirect, FALSE, inherit,
ra_session, target_wcpath,
adm_access, ctx, pool));
peg_revision.kind = svn_opt_revision_working;
SVN_ERR(svn_client__derive_location(&url, &target_rev, target_wcpath,
&peg_revision, ra_session, adm_access,
ctx, pool));
if (target_rev <= end) {
*implicit_mergeinfo = apr_hash_make(pool);
return SVN_NO_ERROR;
}
if (ra_session) {
SVN_ERR(svn_client__ensure_ra_session_url(&session_url, ra_session,
url, pool));
} else {
sesspool = svn_pool_create(pool);
SVN_ERR(svn_client__open_ra_session_internal(&ra_session, url,
NULL, NULL, NULL, FALSE,
TRUE, ctx, sesspool));
}
if (target_rev < start) {
const char *start_url;
svn_opt_revision_t requested, unspec, pegrev, *start_revision;
unspec.kind = svn_opt_revision_unspecified;
requested.kind = svn_opt_revision_number;
requested.value.number = start;
pegrev.kind = svn_opt_revision_number;
pegrev.value.number = target_rev;
SVN_ERR(svn_client__repos_locations(&start_url, &start_revision,
NULL, NULL, ra_session, url,
&pegrev, &requested,
&unspec, ctx, pool));
target_rev = start;
}
peg_revision.kind = svn_opt_revision_number;
peg_revision.value.number = target_rev;
SVN_ERR(svn_client__get_history_as_mergeinfo(implicit_mergeinfo, url,
&peg_revision, start, end,
ra_session, NULL, ctx, pool));
if (sesspool) {
svn_pool_destroy(sesspool);
} else if (session_url) {
SVN_ERR(svn_ra_reparent(ra_session, session_url, pool));
}
return SVN_NO_ERROR;
}
static svn_error_t *
populate_remaining_ranges(apr_array_header_t *children_with_mergeinfo,
const char *source_root_url,
const char *url1,
svn_revnum_t revision1,
const char *url2,
svn_revnum_t revision2,
svn_boolean_t inheritable,
svn_boolean_t honor_mergeinfo,
svn_ra_session_t *ra_session,
const char *parent_merge_src_canon_path,
svn_wc_adm_access_t *adm_access,
merge_cmd_baton_t *merge_b) {
apr_pool_t *iterpool, *pool;
int merge_target_len = strlen(merge_b->target);
int i;
pool = children_with_mergeinfo->pool;
iterpool = svn_pool_create(pool);
if (! honor_mergeinfo || merge_b->record_only) {
for (i = 0; i < children_with_mergeinfo->nelts; i++) {
svn_client__merge_path_t *child =
APR_ARRAY_IDX(children_with_mergeinfo, i,
svn_client__merge_path_t *);
svn_merge_range_t *range = apr_pcalloc(pool, sizeof(*range));
range->start = revision1;
range->end = revision2;
range->inheritable = inheritable;
child->remaining_ranges =
apr_array_make(pool, 1, sizeof(svn_merge_range_t *));
APR_ARRAY_PUSH(child->remaining_ranges, svn_merge_range_t *) = range;
}
return SVN_NO_ERROR;
}
for (i = 0; i < children_with_mergeinfo->nelts; i++) {
const char *child_repos_path;
const svn_wc_entry_t *child_entry;
const char *child_url1, *child_url2;
svn_client__merge_path_t *child =
APR_ARRAY_IDX(children_with_mergeinfo, i, svn_client__merge_path_t *);
svn_client__merge_path_t *parent = NULL;
if (!child || child->absent)
continue;
svn_pool_clear(iterpool);
if (strlen(child->path) == merge_target_len)
child_repos_path = "";
else
child_repos_path = child->path +
(merge_target_len ? merge_target_len + 1 : 0);
child_url1 = svn_path_join(url1, child_repos_path, iterpool);
child_url2 = svn_path_join(url2, child_repos_path, iterpool);
SVN_ERR(svn_wc__entry_versioned(&child_entry, child->path, adm_access,
FALSE, iterpool));
SVN_ERR(get_full_mergeinfo(&(child->pre_merge_mergeinfo),
&(child->implicit_mergeinfo), child_entry,
&(child->indirect_mergeinfo),
svn_mergeinfo_inherited, NULL, child->path,
MAX(revision1, revision2),
MIN(revision1, revision2),
adm_access, merge_b->ctx, pool));
if (i > 0) {
int parent_index = find_nearest_ancestor(children_with_mergeinfo,
FALSE, child->path);
parent = APR_ARRAY_IDX(children_with_mergeinfo, parent_index,
svn_client__merge_path_t *);
if (!parent) {
abort();
}
}
SVN_ERR(calculate_remaining_ranges(parent, child,
source_root_url,
child_url1, revision1,
child_url2, revision2,
child->pre_merge_mergeinfo,
child->implicit_mergeinfo,
i > 0 ? TRUE : FALSE,
ra_session, child_entry, merge_b->ctx,
pool));
}
if (children_with_mergeinfo->nelts > 1) {
svn_client__merge_path_t *child =
APR_ARRAY_IDX(children_with_mergeinfo, 0, svn_client__merge_path_t *);
if (child->remaining_ranges->nelts == 0) {
svn_merge_range_t *dummy_range =
apr_pcalloc(pool, sizeof(*dummy_range));
dummy_range->start = revision2;
dummy_range->end = revision2;
dummy_range->inheritable = inheritable;
child->remaining_ranges = apr_array_make(pool, 1,
sizeof(dummy_range));
APR_ARRAY_PUSH(child->remaining_ranges, svn_merge_range_t *) =
dummy_range;
merge_b->target_has_dummy_merge_range = TRUE;
}
}
svn_pool_destroy(iterpool);
return SVN_NO_ERROR;
}
static svn_error_t *
determine_merges_performed(apr_hash_t **merges, const char *target_wcpath,
apr_array_header_t *rangelist,
svn_depth_t depth,
svn_wc_adm_access_t *adm_access,
notification_receiver_baton_t *notify_b,
merge_cmd_baton_t *merge_b,
apr_pool_t *pool) {
apr_size_t nbr_skips = (notify_b->skipped_paths != NULL ?
apr_hash_count(notify_b->skipped_paths) : 0);
*merges = apr_hash_make(pool);
apr_hash_set(*merges, target_wcpath, APR_HASH_KEY_STRING, rangelist);
if (nbr_skips > 0) {
apr_hash_index_t *hi;
for (hi = apr_hash_first(NULL, notify_b->skipped_paths); hi;
hi = apr_hash_next(hi)) {
const void *skipped_path;
svn_wc_status2_t *status;
apr_hash_this(hi, &skipped_path, NULL, NULL);
SVN_ERR(svn_wc_status2(&status, (const char *) skipped_path,
adm_access, pool));
if (status->text_status == svn_wc_status_none
|| status->text_status == svn_wc_status_unversioned)
continue;
apr_hash_set(*merges, (const char *) skipped_path,
APR_HASH_KEY_STRING,
apr_array_make(pool, 0, sizeof(svn_merge_range_t)));
if (nbr_skips < notify_b->nbr_notifications)
;
}
}
if ((depth != svn_depth_infinity) && notify_b->merged_paths) {
apr_hash_index_t *hi;
const void *merged_path;
for (hi = apr_hash_first(NULL, notify_b->merged_paths); hi;
hi = apr_hash_next(hi)) {
const svn_wc_entry_t *child_entry;
apr_array_header_t *rangelist_of_child = NULL;
apr_hash_this(hi, &merged_path, NULL, NULL);
SVN_ERR(svn_wc__entry_versioned(&child_entry,
merged_path,
adm_access, FALSE,
pool));
if (((child_entry->kind == svn_node_dir)
&& (strcmp(merge_b->target, merged_path) == 0)
&& (depth == svn_depth_immediates))
|| ((child_entry->kind == svn_node_file)
&& (depth == svn_depth_files))) {
int i;
rangelist_of_child = svn_rangelist_dup(rangelist, pool);
for (i = 0; i < rangelist_of_child->nelts; i++) {
svn_merge_range_t *rng =
APR_ARRAY_IDX(rangelist_of_child, i, svn_merge_range_t *);
rng->inheritable = TRUE;
}
}
if (rangelist_of_child) {
apr_hash_set(*merges, (const char *)merged_path,
APR_HASH_KEY_STRING, rangelist_of_child);
}
}
}
return SVN_NO_ERROR;
}
static svn_error_t *
update_wc_mergeinfo(const char *target_wcpath, const svn_wc_entry_t *entry,
const char *repos_rel_path, apr_hash_t *merges,
svn_boolean_t is_rollback,
svn_wc_adm_access_t *adm_access,
svn_client_ctx_t *ctx, apr_pool_t *pool) {
apr_pool_t *subpool = svn_pool_create(pool);
const char *rel_path;
svn_mergeinfo_catalog_t mergeinfo;
apr_hash_index_t *hi;
for (hi = apr_hash_first(pool, merges); hi; hi = apr_hash_next(hi)) {
const void *key;
void *value;
const char *path;
apr_array_header_t *ranges, *rangelist;
int len;
svn_error_t *err;
svn_pool_clear(subpool);
apr_hash_this(hi, &key, NULL, &value);
path = key;
ranges = value;
err = svn_client__parse_mergeinfo(&mergeinfo, entry, path, FALSE,
adm_access, ctx, subpool);
if (err) {
if (err->apr_err == SVN_ERR_WC_NOT_LOCKED) {
svn_error_clear(err);
continue;
} else {
return err;
}
}
if (mergeinfo == NULL && ranges->nelts == 0) {
svn_boolean_t inherited;
SVN_ERR(svn_client__get_wc_mergeinfo(&mergeinfo, &inherited, TRUE,
svn_mergeinfo_nearest_ancestor,
entry, path, NULL, NULL,
adm_access, ctx, subpool));
}
if (mergeinfo == NULL)
mergeinfo = apr_hash_make(subpool);
len = strlen(target_wcpath);
if (len < strlen(path)) {
const char *path_relative_to_target = len?(path + len + 1):(path);
rel_path = apr_pstrcat(subpool, repos_rel_path, "/",
path_relative_to_target, NULL);
} else
rel_path = repos_rel_path;
rangelist = apr_hash_get(mergeinfo, rel_path, APR_HASH_KEY_STRING);
if (rangelist == NULL)
rangelist = apr_array_make(subpool, 0, sizeof(svn_merge_range_t *));
if (is_rollback) {
ranges = svn_rangelist_dup(ranges, subpool);
SVN_ERR(svn_rangelist_reverse(ranges, subpool));
SVN_ERR(svn_rangelist_remove(&rangelist, ranges, rangelist,
FALSE,
subpool));
} else {
SVN_ERR(svn_rangelist_merge(&rangelist, ranges,
subpool));
}
apr_hash_set(mergeinfo, rel_path, APR_HASH_KEY_STRING, rangelist);
if (is_rollback && apr_hash_count(mergeinfo) == 0)
mergeinfo = NULL;
svn_mergeinfo__remove_empty_rangelists(mergeinfo, pool);
err = svn_client__record_wc_mergeinfo(path, mergeinfo,
adm_access, subpool);
if (err && err->apr_err == SVN_ERR_ENTRY_NOT_FOUND) {
svn_error_clear(err);
} else
SVN_ERR(err);
}
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
static APR_INLINE svn_error_t *
make_merge_conflict_error(const char *target_wcpath,
svn_merge_range_t *r,
apr_pool_t *pool) {
return svn_error_createf
(SVN_ERR_WC_FOUND_CONFLICT, NULL,
_("One or more conflicts were produced while merging r%ld:%ld into\n"
"'%s' --\n"
"resolve all conflicts and rerun the merge to apply the remaining\n"
"unmerged revisions"),
r->start, r->end, svn_path_local_style(target_wcpath, pool));
}
static void
remove_absent_children(const char *target_wcpath,
apr_array_header_t *children_with_mergeinfo,
notification_receiver_baton_t *notify_b) {
int i;
for (i = 0; i < children_with_mergeinfo->nelts; i++) {
svn_client__merge_path_t *child =
APR_ARRAY_IDX(children_with_mergeinfo,
i, svn_client__merge_path_t *);
if (child
&& (child->absent || child->scheduled_for_deletion)
&& svn_path_is_ancestor(target_wcpath, child->path)) {
if (notify_b->skipped_paths)
apr_hash_set(notify_b->skipped_paths, child->path,
APR_HASH_KEY_STRING, NULL);
APR_ARRAY_IDX(children_with_mergeinfo, i,
svn_client__merge_path_t *) = NULL;
}
}
}
static svn_error_t *
drive_merge_report_editor(const char *target_wcpath,
const char *url1,
svn_revnum_t revision1,
const char *url2,
svn_revnum_t revision2,
apr_array_header_t *children_with_mergeinfo,
svn_boolean_t is_rollback,
svn_depth_t depth,
notification_receiver_baton_t *notify_b,
svn_wc_adm_access_t *adm_access,
const svn_wc_diff_callbacks2_t *callbacks,
merge_cmd_baton_t *merge_b,
apr_pool_t *pool) {
const svn_ra_reporter3_t *reporter;
const svn_delta_editor_t *diff_editor;
void *diff_edit_baton;
void *report_baton;
svn_revnum_t default_start, target_start;
svn_boolean_t honor_mergeinfo;
const char *old_sess2_url;
mergeinfo_behavior(&honor_mergeinfo, NULL, merge_b);
default_start = target_start = revision1;
if (honor_mergeinfo) {
if (merge_b->target_has_dummy_merge_range) {
target_start = revision2;
} else if (children_with_mergeinfo && children_with_mergeinfo->nelts) {
svn_client__merge_path_t *child =
APR_ARRAY_IDX(children_with_mergeinfo, 0,
svn_client__merge_path_t *);
if (child->remaining_ranges->nelts) {
svn_merge_range_t *range =
APR_ARRAY_IDX(child->remaining_ranges, 0,
svn_merge_range_t *);
target_start = range->start;
}
}
}
SVN_ERR(svn_client__ensure_ra_session_url(&old_sess2_url,
merge_b->ra_session2,
url1, pool));
SVN_ERR(svn_client__get_diff_editor(target_wcpath, adm_access, callbacks,
merge_b, depth, merge_b->dry_run,
merge_b->ra_session2, default_start,
notification_receiver, notify_b,
merge_b->ctx->cancel_func,
merge_b->ctx->cancel_baton,
&diff_editor, &diff_edit_baton,
pool));
SVN_ERR(svn_ra_do_diff3(merge_b->ra_session1,
&reporter, &report_baton, revision2,
"", depth, merge_b->ignore_ancestry,
TRUE,
url2, diff_editor, diff_edit_baton, pool));
SVN_ERR(reporter->set_path(report_baton, "", target_start, depth,
FALSE, NULL, pool));
if (honor_mergeinfo && children_with_mergeinfo) {
apr_size_t target_wcpath_len = strlen(target_wcpath);
int i;
for (i = 1; i < children_with_mergeinfo->nelts; i++) {
svn_merge_range_t *range;
const char *child_repos_path;
svn_client__merge_path_t *parent;
svn_client__merge_path_t *child =
APR_ARRAY_IDX(children_with_mergeinfo, i,
svn_client__merge_path_t *);
int parent_index;
svn_boolean_t nearest_parent_is_target;
if (!child || child->absent)
continue;
parent_index = find_nearest_ancestor(children_with_mergeinfo,
FALSE, child->path);
parent = APR_ARRAY_IDX(children_with_mergeinfo, parent_index,
svn_client__merge_path_t *);
nearest_parent_is_target =
(strcmp(parent->path, target_wcpath) == 0) ? TRUE : FALSE;
if (child->remaining_ranges->nelts) {
range = APR_ARRAY_IDX(child->remaining_ranges, 0,
svn_merge_range_t *);
if (parent->remaining_ranges->nelts) {
svn_merge_range_t *parent_range =
APR_ARRAY_IDX(parent->remaining_ranges, 0,
svn_merge_range_t *);
svn_merge_range_t *child_range =
APR_ARRAY_IDX(child->remaining_ranges, 0,
svn_merge_range_t *);
if (parent_range->start == child_range->start)
continue;
}
} else {
if (parent->remaining_ranges->nelts == 0
|| (nearest_parent_is_target
&& merge_b->target_has_dummy_merge_range))
continue;
}
child_repos_path = child->path +
(target_wcpath_len ? target_wcpath_len + 1 : 0);
if ((child->remaining_ranges->nelts == 0)
|| (is_rollback && (range->start < revision2))
|| (!is_rollback && (range->start > revision2))) {
SVN_ERR(reporter->set_path(report_baton, child_repos_path,
revision2, depth, FALSE,
NULL, pool));
} else {
SVN_ERR(reporter->set_path(report_baton, child_repos_path,
range->start, depth, FALSE,
NULL, pool));
}
}
}
SVN_ERR(reporter->finish_report(report_baton, pool));
if (old_sess2_url)
SVN_ERR(svn_ra_reparent(merge_b->ra_session2, old_sess2_url, pool));
svn_sleep_for_timestamps();
return SVN_NO_ERROR;
}
static svn_revnum_t
get_most_inclusive_start_rev(apr_array_header_t *children_with_mergeinfo,
svn_boolean_t is_rollback) {
int i;
svn_revnum_t start_rev = SVN_INVALID_REVNUM;
for (i = 0; i < children_with_mergeinfo->nelts; i++) {
svn_client__merge_path_t *child =
APR_ARRAY_IDX(children_with_mergeinfo, i, svn_client__merge_path_t *);
svn_merge_range_t *range;
if ((! child) || child->absent)
continue;
if (! child->remaining_ranges->nelts)
continue;
range = APR_ARRAY_IDX(child->remaining_ranges, 0, svn_merge_range_t *);
if ((i == 0) && (range->start == range->end))
continue;
if ((start_rev == SVN_INVALID_REVNUM)
|| (is_rollback && (range->start > start_rev))
|| ((! is_rollback) && (range->start < start_rev)))
start_rev = range->start;
}
return start_rev;
}
static svn_revnum_t
get_youngest_end_rev(apr_array_header_t *children_with_mergeinfo,
svn_boolean_t is_rollback) {
int i;
svn_revnum_t end_rev = SVN_INVALID_REVNUM;
for (i = 0; i < children_with_mergeinfo->nelts; i++) {
svn_client__merge_path_t *child =
APR_ARRAY_IDX(children_with_mergeinfo, i, svn_client__merge_path_t *);
if (!child || child->absent)
continue;
if (child->remaining_ranges->nelts > 0) {
svn_merge_range_t *range = APR_ARRAY_IDX(child->remaining_ranges, 0,
svn_merge_range_t *);
if ((end_rev == SVN_INVALID_REVNUM)
|| (is_rollback && (range->end > end_rev))
|| ((! is_rollback) && (range->end < end_rev)))
end_rev = range->end;
}
}
return end_rev;
}
static void
slice_remaining_ranges(apr_array_header_t *children_with_mergeinfo,
svn_boolean_t is_rollback, svn_revnum_t end_rev,
apr_pool_t *pool) {
int i;
for (i = 0; i < children_with_mergeinfo->nelts; i++) {
svn_client__merge_path_t *child =
APR_ARRAY_IDX(children_with_mergeinfo, i,
svn_client__merge_path_t *);
if (!child || child->absent)
continue;
if (child->remaining_ranges->nelts > 0) {
svn_merge_range_t *range = APR_ARRAY_IDX(child->remaining_ranges, 0,
svn_merge_range_t *);
if ((is_rollback && (range->start > end_rev)
&& (range->end < end_rev))
|| (!is_rollback && (range->start < end_rev)
&& (range->end > end_rev))) {
int j;
svn_merge_range_t *split_range1, *split_range2;
apr_array_header_t *orig_remaining_ranges =
child->remaining_ranges;
split_range1 = svn_merge_range_dup(range, pool);
split_range2 = svn_merge_range_dup(range, pool);
split_range1->end = end_rev;
split_range2->start = end_rev;
child->remaining_ranges =
apr_array_make(pool, (child->remaining_ranges->nelts + 1),
sizeof(svn_merge_range_t *));
APR_ARRAY_PUSH(child->remaining_ranges,
svn_merge_range_t *) = split_range1;
APR_ARRAY_PUSH(child->remaining_ranges,
svn_merge_range_t *) = split_range2;
for (j = 1; j < orig_remaining_ranges->nelts; j++) {
svn_merge_range_t *orig_range =
APR_ARRAY_IDX(orig_remaining_ranges, j,
svn_merge_range_t *);
APR_ARRAY_PUSH(child->remaining_ranges,
svn_merge_range_t *) = orig_range;
}
}
}
}
}
static void
remove_first_range_from_remaining_ranges(svn_revnum_t end_rev,
apr_array_header_t
*children_with_mergeinfo,
apr_pool_t *pool) {
int i, j;
for (i = 0; i < children_with_mergeinfo->nelts; i++) {
svn_client__merge_path_t *child =
APR_ARRAY_IDX(children_with_mergeinfo, i,
svn_client__merge_path_t *);
if (!child || child->absent)
continue;
if (child->remaining_ranges->nelts > 0) {
svn_merge_range_t *first_range =
APR_ARRAY_IDX(child->remaining_ranges, 0, svn_merge_range_t *);
if (first_range->end == end_rev) {
apr_array_header_t *orig_remaining_ranges =
child->remaining_ranges;
child->remaining_ranges =
apr_array_make(pool, (child->remaining_ranges->nelts - 1),
sizeof(svn_merge_range_t *));
for (j = 1; j < orig_remaining_ranges->nelts; j++) {
svn_merge_range_t *range =
APR_ARRAY_IDX(orig_remaining_ranges,
j,
svn_merge_range_t *);
APR_ARRAY_PUSH(child->remaining_ranges,
svn_merge_range_t *) = range;
}
}
}
}
}
static svn_error_t *
mark_mergeinfo_as_inheritable_for_a_range(
svn_mergeinfo_t target_mergeinfo,
svn_boolean_t same_urls,
svn_merge_range_t *range,
const char *rel_path,
const char *target_wcpath,
svn_wc_adm_access_t *adm_access,
merge_cmd_baton_t *merge_b,
apr_array_header_t *children_with_mergeinfo,
int target_index, apr_pool_t *pool) {
if (target_mergeinfo && same_urls
&& !merge_b->dry_run
&& merge_b->same_repos
&& target_index >= 0) {
svn_client__merge_path_t *merge_path =
APR_ARRAY_IDX(children_with_mergeinfo,
target_index, svn_client__merge_path_t *);
if (merge_path
&& merge_path->has_noninheritable && !merge_path->missing_child) {
svn_boolean_t is_equal;
apr_hash_t *merges;
apr_hash_t *inheritable_merges = apr_hash_make(pool);
apr_array_header_t *inheritable_ranges =
apr_array_make(pool, 1, sizeof(svn_merge_range_t *));
APR_ARRAY_PUSH(inheritable_ranges, svn_merge_range_t *) = range;
apr_hash_set(inheritable_merges, rel_path, APR_HASH_KEY_STRING,
inheritable_ranges);
SVN_ERR(svn_mergeinfo_inheritable(&merges, target_mergeinfo,
rel_path, range->start,
range->end, pool));
SVN_ERR(svn_mergeinfo__equals(&is_equal, merges, target_mergeinfo,
FALSE, pool));
if (!is_equal) {
SVN_ERR(svn_mergeinfo_merge(merges, inheritable_merges, pool));
SVN_ERR(svn_client__record_wc_mergeinfo(target_wcpath, merges,
adm_access, pool));
}
}
}
return SVN_NO_ERROR;
}
static svn_error_t *
record_mergeinfo_on_merged_children(svn_depth_t depth,
svn_wc_adm_access_t *adm_access,
notification_receiver_baton_t *notify_b,
merge_cmd_baton_t *merge_b,
apr_pool_t *pool) {
if ((depth != svn_depth_infinity) && notify_b->merged_paths) {
svn_boolean_t indirect_child_mergeinfo = FALSE;
apr_hash_index_t *hi;
svn_mergeinfo_t child_target_mergeinfo;
const void *merged_path;
for (hi = apr_hash_first(NULL, notify_b->merged_paths); hi;
hi = apr_hash_next(hi)) {
const svn_wc_entry_t *child_entry;
apr_hash_this(hi, &merged_path, NULL, NULL);
SVN_ERR(svn_wc__entry_versioned(&child_entry, merged_path,
adm_access, FALSE, pool));
if (((child_entry->kind == svn_node_dir)
&& (strcmp(merge_b->target, merged_path) == 0)
&& (depth == svn_depth_immediates))
|| ((child_entry->kind == svn_node_file)
&& (depth == svn_depth_files))) {
SVN_ERR(svn_client__get_wc_or_repos_mergeinfo
(&child_target_mergeinfo, child_entry,
&indirect_child_mergeinfo,
FALSE, svn_mergeinfo_inherited,
merge_b->ra_session1, merged_path,
adm_access, merge_b->ctx, pool));
if (indirect_child_mergeinfo)
SVN_ERR(svn_client__record_wc_mergeinfo(merged_path,
child_target_mergeinfo,
adm_access, pool));
}
}
}
return SVN_NO_ERROR;
}
static svn_error_t *
single_file_merge_get_file(const char **filename,
svn_ra_session_t *ra_session,
apr_hash_t **props,
svn_revnum_t rev,
const char *wc_target,
apr_pool_t *pool) {
apr_file_t *fp;
svn_stream_t *stream;
SVN_ERR(svn_io_open_unique_file2(&fp, filename,
wc_target, ".tmp",
svn_io_file_del_none, pool));
stream = svn_stream_from_aprfile2(fp, FALSE, pool);
SVN_ERR(svn_ra_get_file(ra_session, "", rev,
stream, NULL, props, pool));
SVN_ERR(svn_stream_close(stream));
return SVN_NO_ERROR;
}
static APR_INLINE void
single_file_merge_notify(void *notify_baton,
const char *target_wcpath,
svn_wc_notify_action_t action,
svn_wc_notify_state_t text_state,
svn_wc_notify_state_t prop_state,
svn_wc_notify_t *header_notification,
svn_boolean_t *header_sent,
apr_pool_t *pool) {
svn_wc_notify_t *notify = svn_wc_create_notify(target_wcpath, action, pool);
notify->kind = svn_node_file;
notify->content_state = text_state;
notify->prop_state = prop_state;
if (notify->content_state == svn_wc_notify_state_missing)
notify->action = svn_wc_notify_skip;
if (IS_OPERATIVE_NOTIFICATION(notify)
&& header_notification
&& (! *header_sent)) {
notification_receiver(notify_baton, header_notification, pool);
*header_sent = TRUE;
}
notification_receiver(notify_baton, notify, pool);
}
struct get_mergeinfo_walk_baton {
svn_wc_adm_access_t *base_access;
apr_array_header_t *children_with_mergeinfo;
const char* merge_src_canon_path;
const char* merge_target_path;
const char *source_root_url;
const char* url1;
const char* url2;
svn_revnum_t revision1;
svn_revnum_t revision2;
svn_depth_t depth;
svn_ra_session_t *ra_session;
svn_client_ctx_t *ctx;
};
static svn_error_t *
get_mergeinfo_walk_cb(const char *path,
const svn_wc_entry_t *entry,
void *walk_baton,
apr_pool_t *pool) {
struct get_mergeinfo_walk_baton *wb = walk_baton;
const svn_string_t *propval;
svn_mergeinfo_t mergehash;
svn_boolean_t switched = FALSE;
svn_boolean_t has_mergeinfo_from_merge_src = FALSE;
svn_boolean_t path_is_merge_target =
!svn_path_compare_paths(path, wb->merge_target_path);
const char *parent_path = svn_path_dirname(path, pool);
if ((entry->kind == svn_node_dir)
&& (strcmp(entry->name, SVN_WC_ENTRY_THIS_DIR) != 0)
&& !entry->absent)
return SVN_NO_ERROR;
if (entry->deleted)
return SVN_NO_ERROR;
if (entry->absent || entry->schedule == svn_wc_schedule_delete) {
propval = NULL;
switched = FALSE;
} else {
SVN_ERR(svn_wc_prop_get(&propval, SVN_PROP_MERGEINFO, path,
wb->base_access, pool));
if (propval && !path_is_merge_target) {
svn_stringbuf_t *merge_src_child_path =
svn_stringbuf_create(wb->merge_src_canon_path, pool);
if (strlen(wb->merge_target_path))
svn_path_add_component(merge_src_child_path,
path + strlen(wb->merge_target_path) + 1);
else
svn_path_add_component(merge_src_child_path,
path);
SVN_ERR(svn_mergeinfo_parse(&mergehash, propval->data, pool));
if (propval->len == 0
|| apr_hash_get(mergehash, merge_src_child_path->data,
APR_HASH_KEY_STRING)) {
has_mergeinfo_from_merge_src = TRUE;
} else {
svn_error_t *err;
const char *original_ra_url = NULL;
const char *mergeinfo_url =
svn_path_join(wb->source_root_url,
++(merge_src_child_path->data),
pool);
svn_opt_revision_t *start_revision, *end_revision;
const char *start_url, *end_url;
svn_opt_revision_t peg_rev, rev1_opt, rev2_opt;
peg_rev.value.number = wb->revision1 < wb->revision2
? wb->revision2 : wb->revision1;
peg_rev.kind = svn_opt_revision_number;
rev1_opt.kind = svn_opt_revision_number;
rev1_opt.value.number = wb->revision1;
rev2_opt.kind = svn_opt_revision_number;
rev2_opt.value.number = wb->revision2;
SVN_ERR(svn_client__ensure_ra_session_url(&original_ra_url,
wb->ra_session,
mergeinfo_url, pool));
err = svn_client__repos_locations(&start_url, &start_revision,
&end_url, &end_revision,
wb->ra_session, mergeinfo_url,
&peg_rev, &rev1_opt, &rev2_opt,
wb->ctx, pool);
if (err) {
if (err->apr_err == SVN_ERR_FS_NOT_FOUND
|| err->apr_err == SVN_ERR_RA_DAV_PATH_NOT_FOUND
|| err->apr_err == SVN_ERR_CLIENT_UNRELATED_RESOURCES)
svn_error_clear(err);
else
return err;
} else {
has_mergeinfo_from_merge_src = TRUE;
}
if (original_ra_url) {
SVN_ERR(svn_ra_reparent(wb->ra_session,
original_ra_url, pool));
}
}
}
SVN_ERR(svn_wc__path_switched(path, &switched, entry, pool));
}
if (path_is_merge_target
|| has_mergeinfo_from_merge_src
|| entry->schedule == svn_wc_schedule_delete
|| switched
|| entry->depth == svn_depth_empty
|| entry->depth == svn_depth_files
|| entry->absent
|| ((wb->depth == svn_depth_immediates) &&
(entry->kind == svn_node_dir) &&
(strcmp(parent_path, path) != 0) &&
(strcmp(parent_path, wb->merge_target_path) == 0))) {
svn_client__merge_path_t *child =
apr_pcalloc(wb->children_with_mergeinfo->pool, sizeof(*child));
child->path = apr_pstrdup(wb->children_with_mergeinfo->pool, path);
child->missing_child = (entry->depth == svn_depth_empty
|| entry->depth == svn_depth_files
|| ((wb->depth == svn_depth_immediates) &&
(entry->kind == svn_node_dir) &&
(strcmp(parent_path,
wb->merge_target_path) == 0)))
? TRUE : FALSE;
child->switched = switched;
child->absent = entry->absent;
child->scheduled_for_deletion =
entry->schedule == svn_wc_schedule_delete ? TRUE : FALSE;
if (propval
&& strstr(propval->data, SVN_MERGEINFO_NONINHERITABLE_STR))
child->has_noninheritable = TRUE;
if (!child->has_noninheritable
&& (entry->depth == svn_depth_empty
|| entry->depth == svn_depth_files))
child->has_noninheritable = TRUE;
APR_ARRAY_PUSH(wb->children_with_mergeinfo,
svn_client__merge_path_t *) = child;
}
return SVN_NO_ERROR;
}
static svn_error_t *
get_mergeinfo_error_handler(const char *path,
svn_error_t *err,
void *walk_baton,
apr_pool_t *pool) {
svn_error_t *root_err = svn_error_root_cause(err);
if (root_err == SVN_NO_ERROR)
return err;
switch (root_err->apr_err) {
case SVN_ERR_WC_PATH_NOT_FOUND:
case SVN_ERR_WC_NOT_LOCKED:
svn_error_clear(err);
return SVN_NO_ERROR;
default:
return err;
}
}
static int
find_child_or_parent(apr_array_header_t *children_with_mergeinfo,
svn_client__merge_path_t **child_or_parent,
const char *path,
svn_boolean_t looking_for_child,
int start_index,
apr_pool_t *pool) {
int j = 0;
*child_or_parent = NULL;
if (start_index >= 0 && start_index < children_with_mergeinfo->nelts) {
for (j = looking_for_child ? start_index + 1 : start_index;
looking_for_child ? j < children_with_mergeinfo->nelts : j >= 0;
j = looking_for_child ? j + 1 : j - 1) {
svn_client__merge_path_t *potential_child_or_parent =
APR_ARRAY_IDX(children_with_mergeinfo, j,
svn_client__merge_path_t *);
int cmp = svn_path_compare_paths(path,
potential_child_or_parent->path);
if (cmp == 0) {
*child_or_parent = potential_child_or_parent;
break;
} else if ((looking_for_child && cmp < 0)
|| (!looking_for_child && cmp > 0)) {
if (!looking_for_child)
j++;
break;
} else if (!looking_for_child && j == 0) {
break;
}
}
}
return j;
}
static void
insert_child_to_merge(apr_array_header_t *children_with_mergeinfo,
svn_client__merge_path_t *insert_element,
int insert_index) {
if (insert_index == children_with_mergeinfo->nelts) {
APR_ARRAY_PUSH(children_with_mergeinfo,
svn_client__merge_path_t *) = insert_element;
} else {
int j;
svn_client__merge_path_t *curr =
APR_ARRAY_IDX(children_with_mergeinfo,
children_with_mergeinfo->nelts - 1,
svn_client__merge_path_t *);
svn_client__merge_path_t *curr_copy =
apr_palloc(children_with_mergeinfo->pool, sizeof(*curr_copy));
*curr_copy = *curr;
APR_ARRAY_PUSH(children_with_mergeinfo,
svn_client__merge_path_t *) = curr_copy;
for (j = children_with_mergeinfo->nelts - 2; j >= insert_index; j--) {
svn_client__merge_path_t *prev;
curr = APR_ARRAY_IDX(children_with_mergeinfo, j,
svn_client__merge_path_t *);
if (j == insert_index)
*curr = *insert_element;
else {
prev = APR_ARRAY_IDX(children_with_mergeinfo, j - 1,
svn_client__merge_path_t *);
*curr = *prev;
}
}
}
}
static int
compare_merge_path_t_as_paths(const void *a,
const void *b) {
svn_client__merge_path_t *child1 = *((svn_client__merge_path_t * const *) a);
svn_client__merge_path_t *child2 = *((svn_client__merge_path_t * const *) b);
return svn_path_compare_paths(child1->path, child2->path);
}
static svn_error_t *
insert_parent_and_sibs_of_sw_absent_del_entry(
apr_array_header_t *children_with_mergeinfo,
merge_cmd_baton_t *merge_cmd_baton,
int *curr_index,
svn_client__merge_path_t *child,
svn_wc_adm_access_t *adm_access,
apr_pool_t *pool) {
svn_client__merge_path_t *parent;
const char *parent_path = svn_path_dirname(child->path, pool);
apr_hash_t *entries;
apr_hash_index_t *hi;
svn_wc_adm_access_t *parent_access;
int insert_index, parent_index;
if (!(child->absent
|| (child->switched
&& strcmp(merge_cmd_baton->target, child->path) != 0)))
return SVN_NO_ERROR;
parent_index = find_child_or_parent(children_with_mergeinfo, &parent,
parent_path, FALSE, *curr_index, pool);
if (parent) {
parent->missing_child = TRUE;
} else {
parent = apr_pcalloc(children_with_mergeinfo->pool, sizeof(*parent));
parent->path = apr_pstrdup(children_with_mergeinfo->pool, parent_path);
parent->missing_child = TRUE;
insert_child_to_merge(children_with_mergeinfo, parent, parent_index);
(*curr_index)++;
}
SVN_ERR(svn_wc_adm_probe_try3(&parent_access, adm_access, parent->path,
TRUE, -1, merge_cmd_baton->ctx->cancel_func,
merge_cmd_baton->ctx->cancel_baton, pool));
SVN_ERR(svn_wc_entries_read(&entries, parent_access, FALSE, pool));
for (hi = apr_hash_first(pool, entries); hi; hi = apr_hash_next(hi)) {
const void *key;
svn_client__merge_path_t *sibling_of_missing;
const char *child_path;
apr_hash_this(hi, &key, NULL, NULL);
if (strcmp(key, SVN_WC_ENTRY_THIS_DIR) == 0)
continue;
child_path = svn_path_join(parent->path, key, pool);
insert_index = find_child_or_parent(children_with_mergeinfo,
&sibling_of_missing, child_path,
TRUE, parent_index, pool);
if (!sibling_of_missing) {
sibling_of_missing = apr_pcalloc(children_with_mergeinfo->pool,
sizeof(*sibling_of_missing));
sibling_of_missing->path = apr_pstrdup(children_with_mergeinfo->pool,
child_path);
insert_child_to_merge(children_with_mergeinfo, sibling_of_missing,
insert_index);
}
}
return SVN_NO_ERROR;
}
static svn_error_t *
get_mergeinfo_paths(apr_array_header_t *children_with_mergeinfo,
merge_cmd_baton_t *merge_cmd_baton,
const char* merge_src_canon_path,
const svn_wc_entry_t *entry,
const char *source_root_url,
const char *url1,
const char *url2,
svn_revnum_t revision1,
svn_revnum_t revision2,
svn_ra_session_t *ra_session,
svn_wc_adm_access_t *adm_access,
svn_client_ctx_t *ctx,
svn_depth_t depth,
apr_pool_t *pool) {
int i;
apr_pool_t *iterpool;
static const svn_wc_entry_callbacks2_t walk_callbacks =
{ get_mergeinfo_walk_cb, get_mergeinfo_error_handler };
struct get_mergeinfo_walk_baton wb = {
adm_access, children_with_mergeinfo,
merge_src_canon_path, merge_cmd_baton->target, source_root_url,
url1, url2, revision1, revision2,
depth, ra_session, ctx
};
if (entry->kind == svn_node_file)
SVN_ERR(walk_callbacks.found_entry(merge_cmd_baton->target, entry, &wb,
pool));
else
SVN_ERR(svn_wc_walk_entries3(merge_cmd_baton->target, adm_access,
&walk_callbacks, &wb, depth, TRUE,
merge_cmd_baton->ctx->cancel_func,
merge_cmd_baton->ctx->cancel_baton,
pool));
qsort(children_with_mergeinfo->elts,
children_with_mergeinfo->nelts,
children_with_mergeinfo->elt_size,
compare_merge_path_t_as_paths);
iterpool = svn_pool_create(pool);
for (i = 0; i < children_with_mergeinfo->nelts; i++) {
int insert_index;
svn_client__merge_path_t *child =
APR_ARRAY_IDX(children_with_mergeinfo, i, svn_client__merge_path_t *);
svn_pool_clear(iterpool);
if (child->has_noninheritable) {
apr_hash_t *entries;
apr_hash_index_t *hi;
svn_wc_adm_access_t *child_access;
SVN_ERR(svn_wc_adm_probe_try3(&child_access, adm_access,
child->path, TRUE, -1,
merge_cmd_baton->ctx->cancel_func,
merge_cmd_baton->ctx->cancel_baton,
iterpool));
SVN_ERR(svn_wc_entries_read(&entries, child_access, FALSE,
iterpool));
for (hi = apr_hash_first(iterpool, entries); hi;
hi = apr_hash_next(hi)) {
const void *key;
svn_client__merge_path_t *child_of_noninheritable;
const char *child_path;
apr_hash_this(hi, &key, NULL, NULL);
if (strcmp(key, SVN_WC_ENTRY_THIS_DIR) == 0)
continue;
child_path = svn_path_join(child->path, key, iterpool);
insert_index = find_child_or_parent(children_with_mergeinfo,
&child_of_noninheritable,
child_path, TRUE, i,
iterpool);
if (!child_of_noninheritable) {
child_of_noninheritable =
apr_pcalloc(children_with_mergeinfo->pool,
sizeof(*child_of_noninheritable));
child_of_noninheritable->path =
apr_pstrdup(children_with_mergeinfo->pool, child_path);
insert_child_to_merge(children_with_mergeinfo,
child_of_noninheritable,
insert_index);
if (!merge_cmd_baton->dry_run
&& merge_cmd_baton->same_repos) {
svn_boolean_t inherited;
svn_mergeinfo_t mergeinfo;
SVN_ERR(svn_client__get_wc_mergeinfo
(&mergeinfo, &inherited, FALSE,
svn_mergeinfo_nearest_ancestor,
entry, child_of_noninheritable->path,
merge_cmd_baton->target, NULL, adm_access,
merge_cmd_baton->ctx, iterpool));
SVN_ERR(svn_client__record_wc_mergeinfo(
child_of_noninheritable->path, mergeinfo, adm_access,
iterpool));
}
}
}
}
SVN_ERR(insert_parent_and_sibs_of_sw_absent_del_entry(
children_with_mergeinfo, merge_cmd_baton, &i, child,
adm_access, iterpool));
}
svn_pool_destroy(iterpool);
return SVN_NO_ERROR;
}
static svn_error_t *
log_changed_revs(void *baton,
svn_log_entry_t *log_entry,
apr_pool_t *pool) {
apr_array_header_t *revs = baton;
svn_revnum_t *revision = apr_palloc(revs->pool, sizeof(*revision));
*revision = log_entry->revision;
APR_ARRAY_PUSH(revs, svn_revnum_t *) = revision;
return SVN_NO_ERROR;
}
static svn_error_t *
remove_noop_merge_ranges(apr_array_header_t **operative_ranges_p,
svn_ra_session_t *ra_session,
apr_array_header_t *ranges,
apr_pool_t *pool) {
int i;
svn_revnum_t oldest_rev = SVN_INVALID_REVNUM;
svn_revnum_t youngest_rev = SVN_INVALID_REVNUM;
svn_revnum_t oldest_changed_rev, youngest_changed_rev;
apr_array_header_t *changed_revs =
apr_array_make(pool, ranges->nelts, sizeof(svn_revnum_t *));
apr_array_header_t *operative_ranges =
apr_array_make(ranges->pool, ranges->nelts, ranges->elt_size);
apr_array_header_t *log_targets =
apr_array_make(pool, 1, sizeof(const char *));
APR_ARRAY_PUSH(log_targets, const char *) = "";
for (i = 0; i < ranges->nelts; i++) {
svn_merge_range_t *r = APR_ARRAY_IDX(ranges, i, svn_merge_range_t *);
svn_revnum_t max_rev = MAX(r->start, r->end);
svn_revnum_t min_rev = MIN(r->start, r->end) + 1;
if ((! SVN_IS_VALID_REVNUM(youngest_rev)) || (max_rev > youngest_rev))
youngest_rev = max_rev;
if ((! SVN_IS_VALID_REVNUM(oldest_rev)) || (min_rev < oldest_rev))
oldest_rev = min_rev;
}
SVN_ERR(svn_ra_get_log2(ra_session, log_targets, youngest_rev,
oldest_rev, 0, FALSE, FALSE, FALSE,
apr_array_make(pool, 0, sizeof(const char *)),
log_changed_revs, changed_revs, pool));
youngest_changed_rev = *(APR_ARRAY_IDX(changed_revs,
0, svn_revnum_t *));
oldest_changed_rev = *(APR_ARRAY_IDX(changed_revs,
changed_revs->nelts - 1,
svn_revnum_t *));
for (i = 0; i < ranges->nelts; i++) {
svn_merge_range_t *range = APR_ARRAY_IDX(ranges, i, svn_merge_range_t *);
svn_revnum_t range_min = MIN(range->start, range->end) + 1;
svn_revnum_t range_max = MAX(range->start, range->end);
int j;
if ((range_min > youngest_changed_rev)
|| (range_max < oldest_changed_rev))
continue;
for (j = 0; j < changed_revs->nelts; j++) {
svn_revnum_t *changed_rev =
APR_ARRAY_IDX(changed_revs, j, svn_revnum_t *);
if ((*changed_rev >= range_min) && (*changed_rev <= range_max)) {
APR_ARRAY_PUSH(operative_ranges, svn_merge_range_t *) = range;
break;
}
}
}
*operative_ranges_p = operative_ranges;
return SVN_NO_ERROR;
}
typedef struct merge_source_t {
const char *url1;
svn_revnum_t rev1;
const char *url2;
svn_revnum_t rev2;
} merge_source_t;
static int
compare_merge_source_ts(const void *a,
const void *b) {
svn_revnum_t a_rev = ((const merge_source_t *)a)->rev1;
svn_revnum_t b_rev = ((const merge_source_t *)b)->rev1;
if (a_rev == b_rev)
return 0;
return a_rev < b_rev ? 1 : -1;
}
static svn_error_t *
combine_range_with_segments(apr_array_header_t **merge_source_ts_p,
svn_merge_range_t *range,
apr_array_header_t *segments,
const char *source_root_url,
apr_pool_t *pool) {
apr_array_header_t *merge_source_ts =
apr_array_make(pool, 1, sizeof(merge_source_t *));
svn_revnum_t minrev = MIN(range->start, range->end) + 1;
svn_revnum_t maxrev = MAX(range->start, range->end);
svn_boolean_t subtractive = (range->start > range->end);
int i;
for (i = 0; i < segments->nelts; i++) {
svn_location_segment_t *segment =
APR_ARRAY_IDX(segments, i, svn_location_segment_t *);
merge_source_t *merge_source;
const char *path1 = NULL;
svn_revnum_t rev1;
if ((segment->range_end < minrev)
|| (segment->range_start > maxrev)
|| (! segment->path))
continue;
rev1 = MAX(segment->range_start, minrev) - 1;
if (minrev <= segment->range_start) {
if (i > 0) {
path1 = (APR_ARRAY_IDX(segments, i - 1,
svn_location_segment_t *))->path;
}
if ((! path1) && (i > 1)) {
path1 = (APR_ARRAY_IDX(segments, i - 2,
svn_location_segment_t *))->path;
rev1 = (APR_ARRAY_IDX(segments, i - 2,
svn_location_segment_t *))->range_end;
}
} else {
path1 = apr_pstrdup(pool, segment->path);
}
if (! (path1 && segment->path))
continue;
merge_source = apr_pcalloc(pool, sizeof(*merge_source));
merge_source->url1 = svn_path_join(source_root_url,
svn_path_uri_encode(path1,
pool), pool);
merge_source->url2 = svn_path_join(source_root_url,
svn_path_uri_encode(segment->path,
pool), pool);
merge_source->rev1 = rev1;
merge_source->rev2 = MIN(segment->range_end, maxrev);
if (subtractive) {
svn_revnum_t tmprev = merge_source->rev1;
const char *tmpurl = merge_source->url1;
merge_source->rev1 = merge_source->rev2;
merge_source->url1 = merge_source->url2;
merge_source->rev2 = tmprev;
merge_source->url2 = tmpurl;
}
APR_ARRAY_PUSH(merge_source_ts, merge_source_t *) = merge_source;
}
if (subtractive && (merge_source_ts->nelts > 1))
qsort(merge_source_ts->elts, merge_source_ts->nelts,
merge_source_ts->elt_size, compare_merge_source_ts);
*merge_source_ts_p = merge_source_ts;
return SVN_NO_ERROR;
}
static svn_error_t *
normalize_merge_sources(apr_array_header_t **merge_sources_p,
const char *source,
const char *source_url,
const char *source_root_url,
const svn_opt_revision_t *peg_revision,
const apr_array_header_t *ranges_to_merge,
svn_ra_session_t *ra_session,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
svn_revnum_t youngest_rev = SVN_INVALID_REVNUM;
svn_revnum_t peg_revnum;
svn_revnum_t oldest_requested = SVN_INVALID_REVNUM;
svn_revnum_t youngest_requested = SVN_INVALID_REVNUM;
svn_revnum_t trim_revision = SVN_INVALID_REVNUM;
svn_opt_revision_t youngest_opt_rev;
apr_array_header_t *merge_range_ts, *segments;
apr_pool_t *subpool;
int i;
youngest_opt_rev.kind = svn_opt_revision_head;
*merge_sources_p = apr_array_make(pool, 1, sizeof(merge_source_t *));
SVN_ERR(svn_client__get_revision_number(&peg_revnum, &youngest_rev,
ra_session, peg_revision,
source, pool));
if (! SVN_IS_VALID_REVNUM(peg_revnum))
return svn_error_create(SVN_ERR_CLIENT_BAD_REVISION, NULL, NULL);
merge_range_ts = apr_array_make(pool, ranges_to_merge->nelts,
sizeof(svn_merge_range_t *));
subpool = svn_pool_create(pool);
for (i = 0; i < ranges_to_merge->nelts; i++) {
svn_revnum_t range_start_rev, range_end_rev;
svn_opt_revision_t *range_start =
&((APR_ARRAY_IDX(ranges_to_merge, i,
svn_opt_revision_range_t *))->start);
svn_opt_revision_t *range_end =
&((APR_ARRAY_IDX(ranges_to_merge, i,
svn_opt_revision_range_t *))->end);
svn_pool_clear(subpool);
if ((range_start->kind == svn_opt_revision_unspecified)
|| (range_end->kind == svn_opt_revision_unspecified))
return svn_error_create(SVN_ERR_CLIENT_BAD_REVISION, NULL,
_("Not all required revisions are specified"));
SVN_ERR(svn_client__get_revision_number(&range_start_rev, &youngest_rev,
ra_session, range_start,
source, subpool));
SVN_ERR(svn_client__get_revision_number(&range_end_rev, &youngest_rev,
ra_session, range_end,
source, subpool));
if (range_start_rev != range_end_rev) {
svn_merge_range_t *range = apr_pcalloc(pool, sizeof(*range));
range->start = range_start_rev;
range->end = range_end_rev;
range->inheritable = TRUE;
APR_ARRAY_PUSH(merge_range_ts, svn_merge_range_t *) = range;
}
}
if (merge_range_ts->nelts == 0)
return SVN_NO_ERROR;
for (i = 0; i < merge_range_ts->nelts; i++) {
svn_merge_range_t *range =
APR_ARRAY_IDX(merge_range_ts, i, svn_merge_range_t *);
svn_revnum_t minrev = MIN(range->start, range->end);
svn_revnum_t maxrev = MAX(range->start, range->end);
if ((! SVN_IS_VALID_REVNUM(oldest_requested))
|| (minrev < oldest_requested))
oldest_requested = minrev;
if ((! SVN_IS_VALID_REVNUM(youngest_requested))
|| (maxrev > youngest_requested))
youngest_requested = maxrev;
}
if (peg_revnum < youngest_requested) {
const char *start_url;
svn_opt_revision_t requested, unspec, pegrev, *start_revision;
unspec.kind = svn_opt_revision_unspecified;
requested.kind = svn_opt_revision_number;
requested.value.number = youngest_requested;
pegrev.kind = svn_opt_revision_number;
pegrev.value.number = peg_revnum;
SVN_ERR(svn_client__repos_locations(&start_url, &start_revision,
NULL, NULL,
ra_session, source_url,
&pegrev, &requested,
&unspec, ctx, pool));
peg_revnum = youngest_requested;
}
SVN_ERR(svn_client__repos_location_segments(&segments,
ra_session, "",
peg_revnum,
youngest_requested,
oldest_requested,
ctx, pool));
trim_revision = SVN_INVALID_REVNUM;
if (segments->nelts) {
svn_location_segment_t *segment =
APR_ARRAY_IDX(segments, 0, svn_location_segment_t *);
if (segment->range_start != oldest_requested) {
trim_revision = segment->range_start;
}
else if (! segment->path) {
if (segments->nelts > 1) {
svn_location_segment_t *segment2 =
APR_ARRAY_IDX(segments, 1, svn_location_segment_t *);
const char *copyfrom_path, *segment_url;
svn_revnum_t copyfrom_rev;
svn_opt_revision_t range_start_rev;
range_start_rev.kind = svn_opt_revision_number;
range_start_rev.value.number = segment2->range_start;
segment_url = svn_path_url_add_component(source_root_url,
segment2->path, pool);
SVN_ERR(svn_client__get_copy_source(segment_url,
&range_start_rev,
&copyfrom_path,
&copyfrom_rev,
ctx, pool));
if (copyfrom_path && SVN_IS_VALID_REVNUM(copyfrom_rev)) {
svn_location_segment_t *new_segment =
apr_pcalloc(pool, sizeof(*new_segment));
new_segment->path = (*copyfrom_path == '/')
? copyfrom_path + 1 : copyfrom_path;
new_segment->range_start = copyfrom_rev;
new_segment->range_end = copyfrom_rev;
segment->range_start = copyfrom_rev + 1;
APR_ARRAY_PUSH(segments, svn_location_segment_t *) = NULL;
memmove(segments->elts + segments->elt_size,
segments->elts,
segments->elt_size * (segments->nelts - 1));
APR_ARRAY_IDX(segments, 0, svn_location_segment_t *) =
new_segment;
}
}
}
}
for (i = 0; i < merge_range_ts->nelts; i++) {
svn_merge_range_t *range =
APR_ARRAY_IDX(merge_range_ts, i, svn_merge_range_t *);
apr_array_header_t *merge_sources;
int j;
if (SVN_IS_VALID_REVNUM(trim_revision)) {
if (MAX(range->start, range->end) < trim_revision)
continue;
if (range->start < trim_revision)
range->start = trim_revision;
if (range->end < trim_revision)
range->end = trim_revision;
}
SVN_ERR(combine_range_with_segments(&merge_sources, range,
segments, source_root_url, pool));
for (j = 0; j < merge_sources->nelts; j++) {
APR_ARRAY_PUSH(*merge_sources_p, merge_source_t *) =
APR_ARRAY_IDX(merge_sources, j, merge_source_t *);
}
}
return SVN_NO_ERROR;
}
static svn_error_t *
filter_natural_history_from_mergeinfo(apr_array_header_t **filtered_rangelist,
const char *source_rel_path,
svn_mergeinfo_t implicit_mergeinfo,
svn_merge_range_t *requested_range,
apr_pool_t *pool) {
apr_array_header_t *requested_rangelist =
apr_array_make(pool, 0, sizeof(svn_merge_range_t *));
APR_ARRAY_PUSH(requested_rangelist, svn_merge_range_t *) =
svn_merge_range_dup(requested_range, pool);
*filtered_rangelist = NULL;
if (implicit_mergeinfo
&& (requested_range->start < requested_range->end)) {
apr_array_header_t *implied_rangelist =
apr_hash_get(implicit_mergeinfo, source_rel_path,
APR_HASH_KEY_STRING);
if (implied_rangelist)
SVN_ERR(svn_rangelist_remove(filtered_rangelist,
implied_rangelist,
requested_rangelist,
FALSE, pool));
}
if (! (*filtered_rangelist))
*filtered_rangelist = requested_rangelist;
return SVN_NO_ERROR;
}
static svn_error_t *
do_file_merge(const char *url1,
svn_revnum_t revision1,
const char *url2,
svn_revnum_t revision2,
const char *target_wcpath,
svn_boolean_t sources_related,
svn_wc_adm_access_t *adm_access,
notification_receiver_baton_t *notify_b,
merge_cmd_baton_t *merge_b,
apr_pool_t *pool) {
apr_hash_t *props1, *props2;
const char *tmpfile1, *tmpfile2;
const char *mimetype1, *mimetype2;
svn_string_t *pval;
apr_array_header_t *propchanges, *remaining_ranges;
svn_wc_notify_state_t prop_state = svn_wc_notify_state_unknown;
svn_wc_notify_state_t text_state = svn_wc_notify_state_unknown;
svn_client_ctx_t *ctx = merge_b->ctx;
const char *mergeinfo_path;
svn_merge_range_t range;
svn_mergeinfo_t target_mergeinfo;
const svn_wc_entry_t *entry;
svn_merge_range_t *conflicted_range = NULL;
int i;
svn_boolean_t indirect = FALSE;
apr_pool_t *subpool;
svn_boolean_t is_rollback = (revision1 > revision2);
const char *primary_url = is_rollback ? url1 : url2;
svn_boolean_t honor_mergeinfo, record_mergeinfo;
svn_mergeinfo_t implicit_mergeinfo;
mergeinfo_behavior(&honor_mergeinfo, &record_mergeinfo, merge_b);
notify_b->is_single_file_merge = TRUE;
SVN_ERR(svn_wc_adm_probe_try3(&adm_access, adm_access, target_wcpath,
TRUE, -1, merge_b->ctx->cancel_func,
merge_b->ctx->cancel_baton,
pool));
SVN_ERR(svn_wc__entry_versioned(&entry, target_wcpath, adm_access, FALSE,
pool));
range.start = revision1;
range.end = revision2;
range.inheritable = TRUE;
if (honor_mergeinfo) {
const char *source_root_url;
svn_client__merge_path_t *merge_target =
apr_pcalloc(pool, sizeof(*merge_target));
SVN_ERR(svn_ra_get_repos_root2(merge_b->ra_session1,
&source_root_url, pool));
SVN_ERR(svn_client__path_relative_to_root(&mergeinfo_path, primary_url,
source_root_url, TRUE, NULL,
NULL, pool));
SVN_ERR(svn_ra_reparent(merge_b->ra_session1, entry->url, pool));
SVN_ERR(get_full_mergeinfo(&target_mergeinfo, &implicit_mergeinfo,
entry, &indirect, svn_mergeinfo_inherited,
merge_b->ra_session1, target_wcpath,
MAX(revision1, revision2),
MIN(revision1, revision2),
adm_access, ctx, pool));
SVN_ERR(svn_ra_reparent(merge_b->ra_session1, url1, pool));
if (!merge_b->record_only) {
SVN_ERR(calculate_remaining_ranges(NULL, merge_target,
source_root_url,
url1, revision1, url2, revision2,
target_mergeinfo,
implicit_mergeinfo, FALSE,
merge_b->ra_session1,
entry, ctx, pool));
remaining_ranges = merge_target->remaining_ranges;
}
}
if (!honor_mergeinfo || merge_b->record_only) {
remaining_ranges = apr_array_make(pool, 1, sizeof(&range));
APR_ARRAY_PUSH(remaining_ranges, svn_merge_range_t *) = &range;
}
subpool = svn_pool_create(pool);
if (!merge_b->record_only) {
apr_array_header_t *ranges_to_merge = remaining_ranges;
if (merge_b->sources_ancestral && (remaining_ranges->nelts > 1)) {
const char *old_sess_url = NULL;
SVN_ERR(svn_client__ensure_ra_session_url(&old_sess_url,
merge_b->ra_session1,
primary_url, subpool));
SVN_ERR(remove_noop_merge_ranges(&ranges_to_merge,
merge_b->ra_session1,
remaining_ranges, subpool));
if (old_sess_url)
SVN_ERR(svn_ra_reparent(merge_b->ra_session1, old_sess_url,
subpool));
svn_pool_clear(subpool);
}
for (i = 0; i < ranges_to_merge->nelts; i++) {
svn_wc_notify_t *n;
svn_boolean_t header_sent = FALSE;
svn_error_t *err = SVN_NO_ERROR;
svn_ra_session_t *ra_session1, *ra_session2;
svn_merge_range_t *r = APR_ARRAY_IDX(ranges_to_merge, i,
svn_merge_range_t *);
svn_pool_clear(subpool);
n = svn_wc_create_notify(target_wcpath,
merge_b->same_repos
? svn_wc_notify_merge_begin
: svn_wc_notify_foreign_merge_begin,
subpool);
if (merge_b->sources_ancestral)
n->merge_range = r;
ra_session1 = merge_b->ra_session1;
ra_session2 = merge_b->ra_session2;
if (honor_mergeinfo && strcmp(url1, url2) != 0) {
if (!is_rollback && r->start != revision1)
ra_session1 = ra_session2;
else if (is_rollback && r->end != revision2)
ra_session2 = ra_session1;
}
SVN_ERR(single_file_merge_get_file(&tmpfile1, ra_session1,
&props1, r->start, target_wcpath,
subpool));
SVN_ERR(single_file_merge_get_file(&tmpfile2, ra_session2,
&props2, r->end, target_wcpath,
subpool));
pval = apr_hash_get(props1, SVN_PROP_MIME_TYPE,
strlen(SVN_PROP_MIME_TYPE));
mimetype1 = pval ? pval->data : NULL;
pval = apr_hash_get(props2, SVN_PROP_MIME_TYPE,
strlen(SVN_PROP_MIME_TYPE));
mimetype2 = pval ? pval->data : NULL;
SVN_ERR(svn_prop_diffs(&propchanges, props2, props1, subpool));
if (! (merge_b->ignore_ancestry || sources_related)) {
SVN_ERR(merge_file_deleted(adm_access,
&text_state,
target_wcpath,
NULL,
NULL,
mimetype1, mimetype2,
props1,
merge_b));
single_file_merge_notify(notify_b, target_wcpath,
svn_wc_notify_update_delete, text_state,
svn_wc_notify_state_unknown, n,
&header_sent, subpool);
SVN_ERR(merge_file_added(adm_access,
&text_state, &prop_state,
target_wcpath,
tmpfile1,
tmpfile2,
r->start,
r->end,
mimetype1, mimetype2,
propchanges, props1,
merge_b));
single_file_merge_notify(notify_b, target_wcpath,
svn_wc_notify_update_add, text_state,
prop_state, n, &header_sent, subpool);
} else {
SVN_ERR(merge_file_changed(adm_access,
&text_state, &prop_state,
target_wcpath,
tmpfile1,
tmpfile2,
r->start,
r->end,
mimetype1, mimetype2,
propchanges, props1,
merge_b));
single_file_merge_notify(notify_b, target_wcpath,
svn_wc_notify_update_update, text_state,
prop_state, n, &header_sent, subpool);
}
err = svn_io_remove_file(tmpfile1, subpool);
if (err && ! APR_STATUS_IS_ENOENT(err->apr_err))
return err;
svn_error_clear(err);
err = svn_io_remove_file(tmpfile2, subpool);
if (err && ! APR_STATUS_IS_ENOENT(err->apr_err))
return err;
svn_error_clear(err);
if ((i < (ranges_to_merge->nelts - 1))
&& is_path_conflicted_by_merge(merge_b)) {
conflicted_range = r;
break;
}
}
}
if (record_mergeinfo && remaining_ranges->nelts) {
apr_hash_t *merges;
apr_array_header_t *filtered_rangelist;
SVN_ERR(filter_natural_history_from_mergeinfo(&filtered_rangelist,
mergeinfo_path,
implicit_mergeinfo,
&range, subpool));
if (filtered_rangelist->nelts) {
SVN_ERR(determine_merges_performed(&merges, target_wcpath,
filtered_rangelist,
svn_depth_infinity,
adm_access, notify_b,
merge_b, subpool));
if (indirect)
SVN_ERR(svn_client__record_wc_mergeinfo(target_wcpath,
target_mergeinfo,
adm_access, subpool));
SVN_ERR(update_wc_mergeinfo(target_wcpath, entry, mergeinfo_path,
merges, is_rollback, adm_access,
ctx, subpool));
}
}
svn_pool_destroy(subpool);
svn_sleep_for_timestamps();
if (conflicted_range)
return make_merge_conflict_error(target_wcpath, conflicted_range, pool);
return SVN_NO_ERROR;
}
static svn_error_t *
do_directory_merge(const char *url1,
svn_revnum_t revision1,
const char *url2,
svn_revnum_t revision2,
const svn_wc_entry_t *parent_entry,
svn_wc_adm_access_t *adm_access,
svn_depth_t depth,
notification_receiver_baton_t *notify_b,
merge_cmd_baton_t *merge_b,
apr_pool_t *pool) {
svn_error_t *err = SVN_NO_ERROR;
apr_array_header_t *children_with_mergeinfo;
int merge_target_len = strlen(merge_b->target);
int i;
svn_merge_range_t range;
svn_ra_session_t *ra_session;
svn_boolean_t inheritable;
apr_pool_t *iterpool;
const char *target_wcpath = svn_wc_adm_access_path(adm_access);
svn_client__merge_path_t *target_merge_path;
svn_boolean_t is_rollback = (revision1 > revision2);
const char *primary_url = is_rollback ? url1 : url2;
const char *source_root_url, *mergeinfo_path;
svn_boolean_t honor_mergeinfo, record_mergeinfo;
svn_boolean_t same_urls = (strcmp(url1, url2) == 0);
mergeinfo_behavior(&honor_mergeinfo, &record_mergeinfo, merge_b);
children_with_mergeinfo =
apr_array_make(pool, 0, sizeof(svn_client__merge_path_t *));
notify_b->children_with_mergeinfo = children_with_mergeinfo;
if (! (merge_b->sources_ancestral && merge_b->same_repos)) {
if (merge_b->sources_ancestral) {
svn_client__merge_path_t *item = apr_pcalloc(pool, sizeof(*item));
svn_merge_range_t *itemrange = apr_pcalloc(pool, sizeof(*itemrange));
apr_array_header_t *remaining_ranges =
apr_array_make(pool, 1, sizeof(svn_merge_range_t *));
itemrange->start = revision1;
itemrange->end = revision2;
itemrange->inheritable = TRUE;
APR_ARRAY_PUSH(remaining_ranges, svn_merge_range_t *) = itemrange;
item->path = apr_pstrdup(pool, target_wcpath);
item->remaining_ranges = remaining_ranges;
APR_ARRAY_PUSH(children_with_mergeinfo,
svn_client__merge_path_t *) = item;
}
return drive_merge_report_editor(target_wcpath,
url1, revision1, url2, revision2,
NULL, is_rollback, depth, notify_b,
adm_access, &merge_callbacks,
merge_b, pool);
}
ra_session = is_rollback ? merge_b->ra_session1 : merge_b->ra_session2;
SVN_ERR(svn_ra_get_repos_root2(ra_session, &source_root_url, pool));
SVN_ERR(svn_client__path_relative_to_root(&mergeinfo_path, primary_url,
source_root_url, TRUE, NULL,
NULL, pool));
SVN_ERR(get_mergeinfo_paths(children_with_mergeinfo, merge_b,
mergeinfo_path, parent_entry, source_root_url,
url1, url2, revision1, revision2,
ra_session, adm_access,
merge_b->ctx, depth, pool));
target_merge_path = APR_ARRAY_IDX(children_with_mergeinfo, 0,
svn_client__merge_path_t *);
merge_b->target_missing_child = target_merge_path->missing_child;
inheritable = ((! merge_b->target_missing_child)
&& ((depth == svn_depth_infinity)
|| (depth == svn_depth_immediates)));
SVN_ERR(populate_remaining_ranges(children_with_mergeinfo,
source_root_url,
url1, revision1, url2, revision2,
inheritable, honor_mergeinfo,
ra_session, mergeinfo_path,
adm_access, merge_b));
range.start = revision1;
range.end = revision2;
range.inheritable = inheritable;
if (honor_mergeinfo && !merge_b->record_only) {
svn_revnum_t start_rev, end_rev;
start_rev = get_most_inclusive_start_rev(children_with_mergeinfo,
is_rollback);
if (SVN_IS_VALID_REVNUM(start_rev)) {
range.start = start_rev;
end_rev = get_youngest_end_rev(children_with_mergeinfo, is_rollback);
range.start = start_rev;
iterpool = svn_pool_create(pool);
while (end_rev != SVN_INVALID_REVNUM) {
svn_revnum_t next_end_rev;
const char *real_url1 = url1, *real_url2 = url2;
const char *old_sess1_url = NULL, *old_sess2_url = NULL;
svn_pool_clear(iterpool);
slice_remaining_ranges(children_with_mergeinfo, is_rollback,
end_rev, pool);
notify_b->cur_ancestor_index = -1;
if (! same_urls) {
if (is_rollback && (end_rev != revision2)) {
real_url2 = url1;
SVN_ERR(svn_client__ensure_ra_session_url
(&old_sess2_url, merge_b->ra_session2,
real_url2, iterpool));
}
if ((! is_rollback) && (start_rev != revision1)) {
real_url1 = url2;
SVN_ERR(svn_client__ensure_ra_session_url
(&old_sess1_url, merge_b->ra_session1,
real_url1, iterpool));
}
}
SVN_ERR(drive_merge_report_editor(merge_b->target,
real_url1, start_rev,
real_url2, end_rev,
children_with_mergeinfo,
is_rollback,
depth, notify_b, adm_access,
&merge_callbacks, merge_b,
iterpool));
if (old_sess1_url)
SVN_ERR(svn_ra_reparent(merge_b->ra_session1,
old_sess1_url, iterpool));
if (old_sess2_url)
SVN_ERR(svn_ra_reparent(merge_b->ra_session2,
old_sess2_url, iterpool));
remove_first_range_from_remaining_ranges(
end_rev, children_with_mergeinfo, pool);
next_end_rev = get_youngest_end_rev(children_with_mergeinfo,
is_rollback);
if ((next_end_rev != SVN_INVALID_REVNUM)
&& is_path_conflicted_by_merge(merge_b)) {
svn_merge_range_t conflicted_range;
conflicted_range.start = start_rev;
conflicted_range.end = end_rev;
err = make_merge_conflict_error(merge_b->target,
&conflicted_range, pool);
range.end = end_rev;
break;
}
start_rev = end_rev;
end_rev = next_end_rev;
}
svn_pool_destroy(iterpool);
}
} else {
if (!merge_b->record_only) {
notify_b->cur_ancestor_index = -1;
SVN_ERR(drive_merge_report_editor(merge_b->target,
url1, revision1, url2, revision2,
NULL, is_rollback,
depth, notify_b, adm_access,
&merge_callbacks, merge_b,
pool));
}
}
iterpool = svn_pool_create(pool);
if (record_mergeinfo) {
apr_array_header_t *filtered_rangelist;
svn_client__merge_path_t *merge_target =
APR_ARRAY_IDX(children_with_mergeinfo, 0, svn_client__merge_path_t *);
apr_hash_t *merges;
remove_absent_children(merge_b->target,
children_with_mergeinfo, notify_b);
SVN_ERR(filter_natural_history_from_mergeinfo(
&filtered_rangelist, mergeinfo_path, merge_target->implicit_mergeinfo,
&range, iterpool));
if (filtered_rangelist->nelts) {
SVN_ERR(determine_merges_performed(&merges, merge_b->target,
filtered_rangelist, depth,
adm_access, notify_b,
merge_b, iterpool));
SVN_ERR(record_mergeinfo_on_merged_children(depth, adm_access,
notify_b, merge_b,
iterpool));
SVN_ERR(update_wc_mergeinfo(merge_b->target, parent_entry,
mergeinfo_path, merges,
is_rollback, adm_access, merge_b->ctx,
iterpool));
}
for (i = 0; i < children_with_mergeinfo->nelts; i++) {
const char *child_repos_path;
const char *child_merge_src_canon_path;
const svn_wc_entry_t *child_entry;
apr_array_header_t *child_merge_rangelist;
apr_hash_t *child_merges;
svn_client__merge_path_t *child =
APR_ARRAY_IDX(children_with_mergeinfo, i,
svn_client__merge_path_t *);
if (!child || child->absent)
continue;
if (strlen(child->path) == merge_target_len)
child_repos_path = "";
else
child_repos_path = child->path +
(merge_target_len ? merge_target_len + 1 : 0);
child_merge_src_canon_path = svn_path_join(mergeinfo_path,
child_repos_path,
iterpool);
SVN_ERR(svn_wc__entry_versioned(&child_entry, child->path,
adm_access, FALSE, iterpool));
child_merges = apr_hash_make(iterpool);
SVN_ERR(filter_natural_history_from_mergeinfo(
&child_merge_rangelist, child_merge_src_canon_path,
child->implicit_mergeinfo, &range, iterpool));
if (child_merge_rangelist->nelts == 0)
continue;
else {
int j;
for (j = 0; j < child_merge_rangelist->nelts; j++) {
svn_merge_range_t *rng =
APR_ARRAY_IDX(child_merge_rangelist, j,
svn_merge_range_t *);
if (child_entry->kind == svn_node_file)
rng->inheritable = TRUE;
else
rng->inheritable = (!(child->missing_child)
&& (depth == svn_depth_infinity
|| depth == svn_depth_immediates));
}
}
apr_hash_set(child_merges, child->path, APR_HASH_KEY_STRING,
child_merge_rangelist);
if (child->indirect_mergeinfo) {
SVN_ERR(svn_client__record_wc_mergeinfo(
child->path,
child->pre_merge_mergeinfo,
adm_access,
iterpool));
}
SVN_ERR(update_wc_mergeinfo(child->path, child_entry,
child_merge_src_canon_path,
child_merges, is_rollback,
adm_access, merge_b->ctx, iterpool));
SVN_ERR(mark_mergeinfo_as_inheritable_for_a_range(
child->pre_merge_mergeinfo,
TRUE,
&range,
child_merge_src_canon_path,
child->path,
adm_access,
merge_b,
children_with_mergeinfo,
i, iterpool));
if (i > 0) {
svn_boolean_t in_switched_subtree = FALSE;
if (child->switched)
in_switched_subtree = TRUE;
else if (i > 1) {
svn_client__merge_path_t *parent;
int j = i - 1;
for (; j > 0; j--) {
parent = APR_ARRAY_IDX(children_with_mergeinfo, j,
svn_client__merge_path_t *);
if (parent
&& parent->switched
&& svn_path_is_ancestor(parent->path, child->path)) {
in_switched_subtree = TRUE;
break;
}
}
}
SVN_ERR(svn_client__elide_mergeinfo(
child->path,
in_switched_subtree ? NULL : merge_b->target,
child_entry, adm_access, merge_b->ctx, iterpool));
}
}
if (notify_b->added_paths) {
apr_hash_index_t *hi;
for (hi = apr_hash_first(NULL, notify_b->added_paths); hi;
hi = apr_hash_next(hi)) {
const void *key;
const char *added_path;
const svn_string_t *added_path_parent_propval;
apr_hash_this(hi, &key, NULL, NULL);
added_path = key;
apr_pool_clear(iterpool);
SVN_ERR(svn_wc_prop_get(&added_path_parent_propval,
SVN_PROP_MERGEINFO,
svn_path_dirname(added_path, iterpool),
adm_access, iterpool));
if (added_path_parent_propval
&& strstr(added_path_parent_propval->data,
SVN_MERGEINFO_NONINHERITABLE_STR)) {
svn_boolean_t inherited;
svn_merge_range_t *rng;
svn_mergeinfo_t merge_mergeinfo, added_path_mergeinfo;
apr_array_header_t *rangelist;
const svn_wc_entry_t *entry;
const char *common_ancestor_path =
svn_path_get_longest_ancestor(added_path,
target_merge_path->path,
iterpool);
const char *relative_added_path =
added_path + strlen(common_ancestor_path) + 1;
SVN_ERR(svn_wc__entry_versioned(&entry, added_path,
adm_access, FALSE,
iterpool));
merge_mergeinfo = apr_hash_make(iterpool);
rangelist = apr_array_make(iterpool, 1,
sizeof(svn_merge_range_t *));
rng = svn_merge_range_dup(&range, iterpool);
if (entry->kind == svn_node_file)
rng->inheritable = TRUE;
else
rng->inheritable =
(!(depth == svn_depth_infinity
|| depth == svn_depth_immediates));
APR_ARRAY_PUSH(rangelist, svn_merge_range_t *) = rng;
apr_hash_set(merge_mergeinfo,
svn_path_join(mergeinfo_path,
relative_added_path,
iterpool),
APR_HASH_KEY_STRING, rangelist);
SVN_ERR(svn_client__get_wc_mergeinfo(
&added_path_mergeinfo, &inherited, FALSE,
svn_mergeinfo_explicit, entry, added_path,
NULL, NULL, adm_access, merge_b->ctx, iterpool));
if (added_path_mergeinfo)
SVN_ERR(svn_mergeinfo_merge(merge_mergeinfo,
added_path_mergeinfo,
iterpool));
SVN_ERR(svn_client__record_wc_mergeinfo(added_path,
merge_mergeinfo,
adm_access,
iterpool));
}
}
}
}
svn_pool_destroy(iterpool);
return err;
}
static svn_error_t *
do_merge(apr_array_header_t *merge_sources,
const char *target,
const svn_wc_entry_t *target_entry,
svn_wc_adm_access_t *adm_access,
svn_boolean_t sources_ancestral,
svn_boolean_t sources_related,
svn_boolean_t same_repos,
svn_boolean_t ignore_ancestry,
svn_boolean_t force,
svn_boolean_t dry_run,
svn_boolean_t record_only,
svn_depth_t depth,
const apr_array_header_t *merge_options,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
apr_pool_t *subpool = svn_pool_create(pool);
merge_cmd_baton_t merge_cmd_baton;
notification_receiver_baton_t notify_baton;
svn_config_t *cfg;
const char *diff3_cmd;
int i;
svn_boolean_t checked_mergeinfo_capability = FALSE;
if (record_only) {
if (! sources_ancestral)
return svn_error_create(SVN_ERR_INCORRECT_PARAMS, NULL,
_("Use of two URLs is not compatible with "
"mergeinfo modification"));
if (! same_repos)
return svn_error_create(SVN_ERR_INCORRECT_PARAMS, NULL,
_("Merge from foreign repository is not "
"compatible with mergeinfo modification"));
if (dry_run)
return SVN_NO_ERROR;
}
if (depth == svn_depth_unknown)
depth = target_entry->depth;
cfg = ctx->config ? apr_hash_get(ctx->config, SVN_CONFIG_CATEGORY_CONFIG,
APR_HASH_KEY_STRING) : NULL;
svn_config_get(cfg, &diff3_cmd, SVN_CONFIG_SECTION_HELPERS,
SVN_CONFIG_OPTION_DIFF3_CMD, NULL);
merge_cmd_baton.force = force;
merge_cmd_baton.dry_run = dry_run;
merge_cmd_baton.record_only = record_only;
merge_cmd_baton.ignore_ancestry = ignore_ancestry;
merge_cmd_baton.same_repos = same_repos;
merge_cmd_baton.mergeinfo_capable = FALSE;
merge_cmd_baton.sources_ancestral = sources_ancestral;
merge_cmd_baton.ctx = ctx;
merge_cmd_baton.target_missing_child = FALSE;
merge_cmd_baton.target = target;
merge_cmd_baton.pool = subpool;
merge_cmd_baton.merge_options = merge_options;
merge_cmd_baton.diff3_cmd = diff3_cmd;
notify_baton.wrapped_func = ctx->notify_func2;
notify_baton.wrapped_baton = ctx->notify_baton2;
notify_baton.nbr_notifications = 0;
notify_baton.nbr_operative_notifications = 0;
notify_baton.merged_paths = NULL;
notify_baton.skipped_paths = NULL;
notify_baton.added_paths = NULL;
notify_baton.is_single_file_merge = FALSE;
notify_baton.children_with_mergeinfo = NULL;
notify_baton.cur_ancestor_index = -1;
notify_baton.merge_b = &merge_cmd_baton;
notify_baton.pool = pool;
for (i = 0; i < merge_sources->nelts; i++) {
merge_source_t *merge_source =
APR_ARRAY_IDX(merge_sources, i, merge_source_t *);
const char *url1, *url2;
svn_revnum_t rev1, rev2;
svn_ra_session_t *ra_session1, *ra_session2;
svn_pool_clear(subpool);
url1 = merge_source->url1;
url2 = merge_source->url2;
rev1 = merge_source->rev1;
rev2 = merge_source->rev2;
if ((strcmp(url1, url2) == 0) && (rev1 == rev2))
continue;
SVN_ERR(svn_client__open_ra_session_internal(&ra_session1, url1,
NULL, NULL, NULL,
FALSE, TRUE, ctx, subpool));
SVN_ERR(svn_client__open_ra_session_internal(&ra_session2, url2,
NULL, NULL, NULL,
FALSE, TRUE, ctx, subpool));
merge_cmd_baton.url = url2;
merge_cmd_baton.added_path = NULL;
merge_cmd_baton.add_necessitated_merge = FALSE;
merge_cmd_baton.dry_run_deletions =
dry_run ? apr_hash_make(subpool) : NULL;
merge_cmd_baton.conflicted_paths = NULL;
merge_cmd_baton.target_has_dummy_merge_range = FALSE;
merge_cmd_baton.ra_session1 = ra_session1;
merge_cmd_baton.ra_session2 = ra_session2;
if (! checked_mergeinfo_capability) {
SVN_ERR(svn_ra_has_capability(ra_session1,
&merge_cmd_baton.mergeinfo_capable,
SVN_RA_CAPABILITY_MERGEINFO, subpool));
checked_mergeinfo_capability = TRUE;
}
if (target_entry->kind == svn_node_file) {
SVN_ERR(do_file_merge(url1, rev1, url2, rev2, target,
sources_related, adm_access,
&notify_baton, &merge_cmd_baton, subpool));
} else if (target_entry->kind == svn_node_dir) {
SVN_ERR(do_directory_merge(url1, rev1, url2, rev2, target_entry,
adm_access, depth, &notify_baton,
&merge_cmd_baton, subpool));
}
if (! dry_run)
SVN_ERR(svn_client__elide_mergeinfo(target, NULL, target_entry,
adm_access, ctx, subpool));
}
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
static svn_error_t *
merge_cousins_and_supplement_mergeinfo(const char *target_wcpath,
const svn_wc_entry_t *entry,
svn_wc_adm_access_t *adm_access,
svn_ra_session_t *ra_session,
const char *URL1,
svn_revnum_t rev1,
const char *URL2,
svn_revnum_t rev2,
svn_revnum_t yc_rev,
const char *source_repos_root,
const char *wc_repos_root,
svn_depth_t depth,
svn_boolean_t ignore_ancestry,
svn_boolean_t force,
svn_boolean_t record_only,
svn_boolean_t dry_run,
const apr_array_header_t *merge_options,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
svn_opt_revision_range_t *range;
apr_array_header_t *remove_sources, *add_sources, *ranges;
svn_opt_revision_t peg_revision;
const char *old_url;
svn_boolean_t same_repos =
(strcmp(wc_repos_root, source_repos_root) == 0) ? TRUE : FALSE;
peg_revision.kind = svn_opt_revision_number;
SVN_ERR(svn_ra_get_session_url(ra_session, &old_url, pool));
range = apr_pcalloc(pool, sizeof(*range));
range->start.kind = svn_opt_revision_number;
range->start.value.number = rev1;
range->end.kind = svn_opt_revision_number;
range->end.value.number = yc_rev;
ranges = apr_array_make(pool, 2, sizeof(svn_opt_revision_range_t *));
APR_ARRAY_PUSH(ranges, svn_opt_revision_range_t *) = range;
peg_revision.value.number = rev1;
SVN_ERR(svn_ra_reparent(ra_session, URL1, pool));
SVN_ERR(normalize_merge_sources(&remove_sources, URL1, URL1,
source_repos_root, &peg_revision,
ranges, ra_session, ctx, pool));
range = apr_pcalloc(pool, sizeof(*range));
range->start.kind = svn_opt_revision_number;
range->start.value.number = yc_rev;
range->end.kind = svn_opt_revision_number;
range->end.value.number = rev2;
ranges = apr_array_make(pool, 2, sizeof(svn_opt_revision_range_t *));
APR_ARRAY_PUSH(ranges, svn_opt_revision_range_t *) = range;
peg_revision.value.number = rev2;
SVN_ERR(svn_ra_reparent(ra_session, URL2, pool));
SVN_ERR(normalize_merge_sources(&add_sources, URL2, URL2,
source_repos_root, &peg_revision,
ranges, ra_session, ctx, pool));
SVN_ERR(svn_ra_reparent(ra_session, old_url, pool));
if (! record_only) {
merge_source_t *faux_source;
apr_array_header_t *faux_sources =
apr_array_make(pool, 1, sizeof(merge_source_t *));
faux_source = apr_pcalloc(pool, sizeof(*faux_source));
faux_source->url1 = URL1;
faux_source->url2 = URL2;
faux_source->rev1 = rev1;
faux_source->rev2 = rev2;
APR_ARRAY_PUSH(faux_sources, merge_source_t *) = faux_source;
SVN_ERR(do_merge(faux_sources, target_wcpath, entry, adm_access,
FALSE, TRUE, same_repos,
ignore_ancestry, force, dry_run,
FALSE, depth, merge_options, ctx, pool));
} else if (! same_repos) {
return svn_error_create(SVN_ERR_INCORRECT_PARAMS, NULL,
_("Merge from foreign repository is not "
"compatible with mergeinfo modification"));
}
if (same_repos) {
SVN_ERR(do_merge(add_sources, target_wcpath, entry,
adm_access, TRUE, TRUE, same_repos,
ignore_ancestry, force, dry_run,
TRUE, depth, merge_options, ctx, pool));
SVN_ERR(do_merge(remove_sources, target_wcpath, entry,
adm_access, TRUE, TRUE, same_repos,
ignore_ancestry, force, dry_run,
TRUE, depth, merge_options, ctx, pool));
}
return SVN_NO_ERROR;
}
svn_error_t *
svn_client_merge3(const char *source1,
const svn_opt_revision_t *revision1,
const char *source2,
const svn_opt_revision_t *revision2,
const char *target_wcpath,
svn_depth_t depth,
svn_boolean_t ignore_ancestry,
svn_boolean_t force,
svn_boolean_t record_only,
svn_boolean_t dry_run,
const apr_array_header_t *merge_options,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
svn_wc_adm_access_t *adm_access;
const svn_wc_entry_t *entry;
const char *URL1, *URL2;
svn_revnum_t rev1, rev2;
svn_boolean_t related = FALSE, ancestral = FALSE;
const char *wc_repos_root, *source_repos_root;
svn_revnum_t youngest_rev = SVN_INVALID_REVNUM;
svn_ra_session_t *ra_session1, *ra_session2;
apr_array_header_t *merge_sources;
merge_source_t *merge_source;
svn_opt_revision_t working_rev;
const char *yc_path = NULL;
svn_revnum_t yc_rev = SVN_INVALID_REVNUM;
apr_pool_t *sesspool;
svn_boolean_t same_repos;
if ((revision1->kind == svn_opt_revision_unspecified)
|| (revision2->kind == svn_opt_revision_unspecified))
return svn_error_create(SVN_ERR_CLIENT_BAD_REVISION, NULL,
_("Not all required revisions are specified"));
SVN_ERR(svn_client_url_from_path(&URL1, source1, pool));
if (! URL1)
return svn_error_createf(SVN_ERR_ENTRY_MISSING_URL, NULL,
_("'%s' has no URL"),
svn_path_local_style(source1, pool));
SVN_ERR(svn_client_url_from_path(&URL2, source2, pool));
if (! URL2)
return svn_error_createf(SVN_ERR_ENTRY_MISSING_URL, NULL,
_("'%s' has no URL"),
svn_path_local_style(source2, pool));
SVN_ERR(svn_wc_adm_probe_open3(&adm_access, NULL, target_wcpath,
! dry_run, -1, ctx->cancel_func,
ctx->cancel_baton, pool));
SVN_ERR(svn_wc__entry_versioned(&entry, target_wcpath, adm_access,
FALSE, pool));
working_rev.kind = svn_opt_revision_working;
SVN_ERR(svn_client__get_repos_root(&wc_repos_root, target_wcpath,
&working_rev, adm_access, ctx, pool));
sesspool = svn_pool_create(pool);
SVN_ERR(svn_client__open_ra_session_internal(&ra_session1,
URL1, NULL, NULL, NULL,
FALSE, TRUE, ctx, sesspool));
SVN_ERR(svn_client__open_ra_session_internal(&ra_session2,
URL2, NULL, NULL, NULL,
FALSE, TRUE, ctx, sesspool));
SVN_ERR(svn_client__get_revision_number(&rev1, &youngest_rev, ra_session1,
revision1, NULL, sesspool));
SVN_ERR(svn_client__get_revision_number(&rev2, &youngest_rev, ra_session2,
revision2, NULL, sesspool));
SVN_ERR(svn_ra_get_repos_root2(ra_session1, &source_repos_root, sesspool));
same_repos = (strcmp(source_repos_root, wc_repos_root) == 0) ? TRUE : FALSE;
if (! ignore_ancestry)
SVN_ERR(svn_client__get_youngest_common_ancestor(&yc_path, &yc_rev,
URL1, rev1,
URL2, rev2,
ctx, pool));
if (yc_path && SVN_IS_VALID_REVNUM(yc_rev)) {
apr_array_header_t *ranges;
svn_opt_revision_range_t *range;
svn_opt_revision_t peg_revision;
peg_revision.kind = svn_opt_revision_number;
related = TRUE;
yc_path = svn_path_join(source_repos_root,
svn_path_uri_encode(yc_path, pool), pool);
if ((strcmp(yc_path, URL2) == 0) && (yc_rev == rev2)) {
ancestral = TRUE;
range = apr_pcalloc(pool, sizeof(*range));
range->start.kind = svn_opt_revision_number;
range->start.value.number = rev1;
range->end.kind = svn_opt_revision_number;
range->end.value.number = yc_rev;
ranges = apr_array_make(pool, 2, sizeof(svn_opt_revision_range_t *));
APR_ARRAY_PUSH(ranges, svn_opt_revision_range_t *) = range;
peg_revision.value.number = rev1;
SVN_ERR(normalize_merge_sources(&merge_sources, URL1, URL1,
source_repos_root, &peg_revision,
ranges, ra_session1, ctx, pool));
}
else if ((strcmp(yc_path, URL1) == 0) && (yc_rev == rev1)) {
ancestral = TRUE;
range = apr_pcalloc(pool, sizeof(*range));
range->start.kind = svn_opt_revision_number;
range->start.value.number = yc_rev;
range->end.kind = svn_opt_revision_number;
range->end.value.number = rev2;
ranges = apr_array_make(pool, 2, sizeof(svn_opt_revision_range_t *));
APR_ARRAY_PUSH(ranges, svn_opt_revision_range_t *) = range;
peg_revision.value.number = rev2;
SVN_ERR(normalize_merge_sources(&merge_sources, URL2, URL2,
source_repos_root, &peg_revision,
ranges, ra_session2, ctx, pool));
}
else {
SVN_ERR(merge_cousins_and_supplement_mergeinfo(target_wcpath, entry,
adm_access,
ra_session1,
URL1, rev1,
URL2, rev2,
yc_rev,
source_repos_root,
wc_repos_root,
depth,
ignore_ancestry, force,
record_only, dry_run,
merge_options, ctx,
pool));
svn_pool_destroy(sesspool);
SVN_ERR(svn_wc_adm_close(adm_access));
return SVN_NO_ERROR;
}
} else {
merge_sources = apr_array_make(pool, 1, sizeof(merge_source_t *));
merge_source = apr_pcalloc(pool, sizeof(*merge_source));
merge_source->url1 = URL1;
merge_source->url2 = URL2;
merge_source->rev1 = rev1;
merge_source->rev2 = rev2;
APR_ARRAY_PUSH(merge_sources, merge_source_t *) = merge_source;
}
svn_pool_destroy(sesspool);
SVN_ERR(do_merge(merge_sources, target_wcpath, entry, adm_access,
ancestral, related, same_repos,
ignore_ancestry, force, dry_run,
record_only, depth, merge_options, ctx, pool));
SVN_ERR(svn_wc_adm_close(adm_access));
return SVN_NO_ERROR;
}
svn_error_t *
svn_client_merge2(const char *source1,
const svn_opt_revision_t *revision1,
const char *source2,
const svn_opt_revision_t *revision2,
const char *target_wcpath,
svn_boolean_t recurse,
svn_boolean_t ignore_ancestry,
svn_boolean_t force,
svn_boolean_t dry_run,
const apr_array_header_t *merge_options,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
return svn_client_merge3(source1, revision1, source2, revision2,
target_wcpath,
SVN_DEPTH_INFINITY_OR_FILES(recurse),
ignore_ancestry, force, FALSE, dry_run,
merge_options, ctx, pool);
}
svn_error_t *
svn_client_merge(const char *source1,
const svn_opt_revision_t *revision1,
const char *source2,
const svn_opt_revision_t *revision2,
const char *target_wcpath,
svn_boolean_t recurse,
svn_boolean_t ignore_ancestry,
svn_boolean_t force,
svn_boolean_t dry_run,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
return svn_client_merge2(source1, revision1, source2, revision2,
target_wcpath, recurse, ignore_ancestry, force,
dry_run, NULL, ctx, pool);
}
static svn_error_t *
ensure_wc_reflects_repository_subtree(const char *target_wcpath,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
svn_wc_revision_status_t *wc_stat;
SVN_ERR(svn_wc_revision_status(&wc_stat, target_wcpath, NULL, FALSE,
ctx->cancel_func, ctx->cancel_baton, pool));
if (wc_stat->switched)
return svn_error_create(SVN_ERR_CLIENT_NOT_READY_TO_MERGE, NULL,
_("Cannot reintegrate into a working copy "
"with a switched subtree"));
if (wc_stat->sparse_checkout)
return svn_error_create(SVN_ERR_CLIENT_NOT_READY_TO_MERGE, NULL,
_("Cannot reintegrate into a working copy "
"not entirely at infinite depth"));
if (wc_stat->modified)
return svn_error_create(SVN_ERR_CLIENT_NOT_READY_TO_MERGE, NULL,
_("Cannot reintegrate into a working copy "
"that has local modifications"));
if (! (SVN_IS_VALID_REVNUM(wc_stat->min_rev)
&& SVN_IS_VALID_REVNUM(wc_stat->max_rev)))
return svn_error_create(SVN_ERR_CLIENT_NOT_READY_TO_MERGE, NULL,
_("Cannot determine revision of working copy"));
if (wc_stat->min_rev != wc_stat->max_rev)
return svn_error_create(SVN_ERR_CLIENT_NOT_READY_TO_MERGE, NULL,
_("Cannot reintegrate into mixed-revision "
"working copy; try updating first"));
return SVN_NO_ERROR;
}
static svn_error_t *
ensure_all_missing_ranges_are_phantoms(svn_ra_session_t *ra_session,
svn_mergeinfo_t history_as_mergeinfo,
apr_pool_t *pool) {
apr_hash_index_t *hi;
apr_pool_t *iterpool = svn_pool_create(pool);
for (hi = apr_hash_first(pool, history_as_mergeinfo); hi;
hi = apr_hash_next(hi)) {
const void *key;
void *value;
const char *path;
apr_array_header_t *rangelist;
int i;
apr_hash_this(hi, &key, NULL, &value);
path = key;
rangelist = value;
assert(*path);
path++;
for (i = 0; i < rangelist->nelts; i++) {
svn_merge_range_t *range = APR_ARRAY_IDX(rangelist, i,
svn_merge_range_t *);
svn_dirent_t *dirent;
assert(range->start < range->end);
svn_pool_clear(iterpool);
SVN_ERR(svn_ra_stat(ra_session,
path,
range->end,
&dirent,
iterpool));
if (svn_merge_range_contains_rev(range, dirent->created_rev)) {
const char *full_url;
svn_pool_destroy(iterpool);
SVN_ERR(svn_ra_get_session_url(ra_session, &full_url, pool));
full_url = svn_path_url_add_component(full_url, path, pool);
return svn_error_createf(SVN_ERR_CLIENT_NOT_READY_TO_MERGE, NULL,
_("At least one revision (r%ld) "
"not yet merged from '%s'"),
dirent->created_rev, full_url);
}
}
}
svn_pool_destroy(iterpool);
return SVN_NO_ERROR;
}
static svn_error_t *
remove_irrelevant_ranges(svn_mergeinfo_catalog_t *catalog_p,
svn_mergeinfo_catalog_t catalog,
apr_array_header_t *segments,
const char *source_repos_rel_path,
apr_pool_t *pool) {
apr_hash_index_t *hi;
svn_mergeinfo_catalog_t new_catalog = apr_hash_make(pool);
svn_mergeinfo_t history_as_mergeinfo;
SVN_ERR(svn_client__mergeinfo_from_segments(&history_as_mergeinfo,
segments,
pool));
for (hi = apr_hash_first(pool, catalog);
hi;
hi = apr_hash_next(hi)) {
const void *key;
void *val;
const char *path;
svn_mergeinfo_t mergeinfo, filtered_mergeinfo;
apr_hash_this(hi, &key, NULL, &val);
path = key;
mergeinfo = val;
SVN_ERR(svn_mergeinfo_intersect(&filtered_mergeinfo,
mergeinfo,
history_as_mergeinfo,
pool));
if (apr_hash_count(filtered_mergeinfo)
|| strcmp(source_repos_rel_path, path) != 0)
apr_hash_set(new_catalog,
apr_pstrdup(pool, path),
APR_HASH_KEY_STRING,
filtered_mergeinfo);
}
*catalog_p = new_catalog;
return SVN_NO_ERROR;
}
static svn_error_t *
calculate_left_hand_side(const char **url_left,
svn_revnum_t *rev_left,
svn_mergeinfo_t *source_mergeinfo_p,
const char *target_repos_rel_path,
svn_revnum_t target_rev,
const char *source_repos_rel_path,
const char *source_repos_root,
svn_revnum_t source_rev,
svn_ra_session_t *ra_session,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
apr_array_header_t *segments;
svn_boolean_t have_mergeinfo_for_source = FALSE,
have_mergeinfo_for_descendants = FALSE;
svn_mergeinfo_catalog_t mergeinfo_catalog;
apr_array_header_t *source_repos_rel_path_as_array
= apr_array_make(pool, 1, sizeof(const char *));
apr_pool_t *subpool = svn_pool_create(pool);
SVN_ERR(svn_client__repos_location_segments(&segments,
ra_session,
target_repos_rel_path,
target_rev, target_rev,
SVN_INVALID_REVNUM,
ctx, subpool));
APR_ARRAY_PUSH(source_repos_rel_path_as_array, const char *)
= source_repos_rel_path;
SVN_ERR(svn_ra_get_mergeinfo(ra_session, &mergeinfo_catalog,
source_repos_rel_path_as_array, source_rev,
svn_mergeinfo_inherited, TRUE, subpool));
if (!mergeinfo_catalog)
mergeinfo_catalog = apr_hash_make(subpool);
SVN_ERR(remove_irrelevant_ranges(&mergeinfo_catalog,
mergeinfo_catalog,
segments,
source_repos_rel_path,
subpool));
SVN_ERR(svn_client__elide_mergeinfo_catalog(mergeinfo_catalog, subpool));
if (apr_hash_get(mergeinfo_catalog, source_repos_rel_path,
APR_HASH_KEY_STRING))
have_mergeinfo_for_source = TRUE;
if (apr_hash_count(mergeinfo_catalog) > 1 ||
(! have_mergeinfo_for_source && apr_hash_count(mergeinfo_catalog) == 1))
have_mergeinfo_for_descendants = TRUE;
if (! have_mergeinfo_for_source && ! have_mergeinfo_for_descendants) {
const char *yc_ancestor_path,
*source_url = svn_path_join(source_repos_root, source_repos_rel_path,
subpool),
*target_url = svn_path_join(source_repos_root, target_repos_rel_path,
subpool);
SVN_ERR(svn_client__get_youngest_common_ancestor(&yc_ancestor_path,
rev_left,
source_url, source_rev,
target_url, target_rev,
ctx, subpool));
if (!(yc_ancestor_path && SVN_IS_VALID_REVNUM(*rev_left)))
return svn_error_createf(SVN_ERR_CLIENT_NOT_READY_TO_MERGE, NULL,
_("'%s@%ld' must be ancestrally related to "
"'%s@%ld'"), source_url, source_rev,
target_url, target_rev);
*url_left = svn_path_join(source_repos_root, yc_ancestor_path, pool);
*source_mergeinfo_p = apr_hash_make(pool);
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
} else if (! have_mergeinfo_for_descendants) {
svn_mergeinfo_t source_mergeinfo = apr_hash_get(mergeinfo_catalog,
source_repos_rel_path,
APR_HASH_KEY_STRING);
apr_pool_t *iterpool = svn_pool_create(subpool);
int i;
for (i = segments->nelts - 1; i >= 0; i--) {
svn_location_segment_t *segment
= APR_ARRAY_IDX(segments, i, svn_location_segment_t *);
apr_array_header_t *rangelist;
svn_pool_clear(iterpool);
if (!segment->path)
continue;
rangelist = apr_hash_get(source_mergeinfo,
apr_pstrcat(iterpool, "/", segment->path,
NULL),
APR_HASH_KEY_STRING);
if (rangelist != NULL && rangelist->nelts > 0) {
svn_merge_range_t *last_range
= APR_ARRAY_IDX(rangelist, rangelist->nelts - 1,
svn_merge_range_t *);
*rev_left = last_range->end;
*url_left = svn_path_join(source_repos_root, segment->path,
pool);
*source_mergeinfo_p = svn_mergeinfo_dup(source_mergeinfo, pool);
svn_pool_destroy(iterpool);
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
}
abort();
} else {
const char *full_url;
SVN_ERR(svn_ra_get_session_url(ra_session, &full_url, pool));
full_url = svn_path_url_add_component(full_url, source_repos_rel_path,
pool);
return svn_error_createf(SVN_ERR_CLIENT_NOT_READY_TO_MERGE, NULL,
"Cannot reintegrate from '%s' yet:\n"
"Some revisions have been merged under it "
"that have not been merged\n"
"into the reintegration target; "
"merge them first, then retry.", full_url);
}
return SVN_NO_ERROR;
}
svn_error_t *
svn_client_merge_reintegrate(const char *source,
const svn_opt_revision_t *peg_revision,
const char *target_wcpath,
svn_boolean_t dry_run,
const apr_array_header_t *merge_options,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
svn_wc_adm_access_t *adm_access;
const svn_wc_entry_t *entry;
const char *wc_repos_root, *source_repos_root;
svn_opt_revision_t working_revision;
svn_ra_session_t *ra_session;
const char *source_repos_rel_path, *target_repos_rel_path;
const char *yc_ancestor_path;
svn_revnum_t yc_ancestor_rev;
const char *url1, *url2;
svn_revnum_t rev1, rev2;
svn_mergeinfo_t source_mergeinfo;
SVN_ERR(svn_wc_adm_probe_open3(&adm_access, NULL, target_wcpath,
(! dry_run), -1, ctx->cancel_func,
ctx->cancel_baton, pool));
SVN_ERR(svn_wc__entry_versioned(&entry, target_wcpath, adm_access,
FALSE, pool));
SVN_ERR(svn_client_url_from_path(&url2, source, pool));
if (! url2)
return svn_error_createf(SVN_ERR_ENTRY_MISSING_URL, NULL,
_("'%s' has no URL"),
svn_path_local_style(source, pool));
working_revision.kind = svn_opt_revision_working;
SVN_ERR(svn_client__get_repos_root(&wc_repos_root, target_wcpath,
&working_revision, adm_access, ctx, pool));
SVN_ERR(svn_client__open_ra_session_internal(&ra_session, wc_repos_root,
NULL, NULL, NULL,
FALSE, FALSE, ctx, pool));
SVN_ERR(svn_ra_get_repos_root2(ra_session, &source_repos_root, pool));
if (strcmp(source_repos_root, wc_repos_root) != 0)
return svn_error_createf(SVN_ERR_CLIENT_UNRELATED_RESOURCES, NULL,
_("'%s' must be from the same repository as "
"'%s'"), svn_path_local_style(source, pool),
svn_path_local_style(target_wcpath, pool));
SVN_ERR(ensure_wc_reflects_repository_subtree(target_wcpath, ctx, pool));
rev1 = entry->revision;
SVN_ERR(svn_client__path_relative_to_root(&source_repos_rel_path,
url2, NULL, FALSE,
ra_session, NULL, pool));
SVN_ERR(svn_client__path_relative_to_root(&target_repos_rel_path,
target_wcpath, wc_repos_root,
FALSE, ra_session, NULL, pool));
SVN_ERR(svn_client__get_revision_number(&rev2, NULL,
ra_session, peg_revision,
source_repos_rel_path, pool));
SVN_ERR(calculate_left_hand_side(&url1, &rev1, &source_mergeinfo,
target_repos_rel_path,
rev1,
source_repos_rel_path,
source_repos_root,
rev2,
ra_session,
ctx,
pool));
SVN_ERR(svn_client__get_youngest_common_ancestor(&yc_ancestor_path,
&yc_ancestor_rev,
url2, rev2,
url1, rev1,
ctx, pool));
if (!(yc_ancestor_path && SVN_IS_VALID_REVNUM(yc_ancestor_rev)))
return svn_error_createf(SVN_ERR_CLIENT_NOT_READY_TO_MERGE, NULL,
_("'%s@%ld' must be ancestrally related to "
"'%s@%ld'"), url1, rev1, url2, rev2);
if (rev1 > yc_ancestor_rev) {
svn_opt_revision_t opt_rev1;
svn_mergeinfo_t target_mergeinfo, deleted_mergeinfo, added_mergeinfo;
opt_rev1.kind = svn_opt_revision_number;
opt_rev1.value.number = rev1;
SVN_ERR(svn_client__get_history_as_mergeinfo(&target_mergeinfo,
entry->url,
&opt_rev1,
rev1,
yc_ancestor_rev + 1,
NULL, adm_access, ctx,
pool));
SVN_ERR(svn_mergeinfo_diff(&deleted_mergeinfo, &added_mergeinfo,
target_mergeinfo, source_mergeinfo, FALSE,
pool));
SVN_ERR(ensure_all_missing_ranges_are_phantoms(ra_session,
deleted_mergeinfo, pool));
}
SVN_ERR(merge_cousins_and_supplement_mergeinfo(target_wcpath, entry,
adm_access, ra_session,
url1, rev1, url2, rev2,
yc_ancestor_rev,
source_repos_root,
wc_repos_root,
svn_depth_infinity,
FALSE,
FALSE, FALSE, dry_run,
merge_options, ctx, pool));
SVN_ERR(svn_wc_adm_close(adm_access));
return SVN_NO_ERROR;
}
svn_error_t *
svn_client_merge_peg3(const char *source,
const apr_array_header_t *ranges_to_merge,
const svn_opt_revision_t *peg_revision,
const char *target_wcpath,
svn_depth_t depth,
svn_boolean_t ignore_ancestry,
svn_boolean_t force,
svn_boolean_t record_only,
svn_boolean_t dry_run,
const apr_array_header_t *merge_options,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
svn_wc_adm_access_t *adm_access;
const svn_wc_entry_t *entry;
const char *URL;
apr_array_header_t *merge_sources;
const char *wc_repos_root, *source_repos_root;
svn_opt_revision_t working_rev;
svn_ra_session_t *ra_session;
apr_pool_t *sesspool;
if (ranges_to_merge->nelts == 0)
return SVN_NO_ERROR;
SVN_ERR(svn_wc_adm_probe_open3(&adm_access, NULL, target_wcpath,
(! dry_run), -1, ctx->cancel_func,
ctx->cancel_baton, pool));
SVN_ERR(svn_wc__entry_versioned(&entry, target_wcpath, adm_access,
FALSE, pool));
SVN_ERR(svn_client_url_from_path(&URL, source, pool));
if (! URL)
return svn_error_createf(SVN_ERR_ENTRY_MISSING_URL, NULL,
_("'%s' has no URL"),
svn_path_local_style(source, pool));
working_rev.kind = svn_opt_revision_working;
SVN_ERR(svn_client__get_repos_root(&wc_repos_root, target_wcpath,
&working_rev, adm_access, ctx, pool));
sesspool = svn_pool_create(pool);
SVN_ERR(svn_client__open_ra_session_internal(&ra_session,
URL, NULL, NULL, NULL,
FALSE, TRUE, ctx, sesspool));
SVN_ERR(svn_ra_get_repos_root2(ra_session, &source_repos_root, pool));
SVN_ERR(normalize_merge_sources(&merge_sources, source, URL,
source_repos_root, peg_revision,
ranges_to_merge, ra_session, ctx, pool));
svn_pool_destroy(sesspool);
SVN_ERR(do_merge(merge_sources, target_wcpath, entry, adm_access,
TRUE, TRUE,
(strcmp(wc_repos_root, source_repos_root) == 0),
ignore_ancestry, force, dry_run, record_only, depth,
merge_options, ctx, pool));
SVN_ERR(svn_wc_adm_close(adm_access));
return SVN_NO_ERROR;
}
svn_error_t *
svn_client_merge_peg2(const char *source,
const svn_opt_revision_t *revision1,
const svn_opt_revision_t *revision2,
const svn_opt_revision_t *peg_revision,
const char *target_wcpath,
svn_boolean_t recurse,
svn_boolean_t ignore_ancestry,
svn_boolean_t force,
svn_boolean_t dry_run,
const apr_array_header_t *merge_options,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
svn_opt_revision_range_t range;
apr_array_header_t *ranges_to_merge =
apr_array_make(pool, 1, sizeof(svn_opt_revision_range_t *));
range.start = *revision1;
range.end = *revision2;
APR_ARRAY_PUSH(ranges_to_merge, svn_opt_revision_range_t *) = &range;
return svn_client_merge_peg3(source, ranges_to_merge,
peg_revision,
target_wcpath,
SVN_DEPTH_INFINITY_OR_FILES(recurse),
ignore_ancestry, force, FALSE, dry_run,
merge_options, ctx, pool);
}
svn_error_t *
svn_client_merge_peg(const char *source,
const svn_opt_revision_t *revision1,
const svn_opt_revision_t *revision2,
const svn_opt_revision_t *peg_revision,
const char *target_wcpath,
svn_boolean_t recurse,
svn_boolean_t ignore_ancestry,
svn_boolean_t force,
svn_boolean_t dry_run,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
return svn_client_merge_peg2(source, revision1, revision2, peg_revision,
target_wcpath, recurse, ignore_ancestry, force,
dry_run, NULL, ctx, pool);
}