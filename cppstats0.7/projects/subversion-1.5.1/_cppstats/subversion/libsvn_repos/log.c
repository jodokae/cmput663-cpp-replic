#include <stdlib.h>
#define APR_WANT_STRFUNC
#include <apr_want.h>
#include "svn_compat.h"
#include "svn_private_config.h"
#include "svn_pools.h"
#include "svn_error.h"
#include "svn_path.h"
#include "svn_fs.h"
#include "svn_repos.h"
#include "svn_string.h"
#include "svn_sorts.h"
#include "svn_props.h"
#include "svn_mergeinfo.h"
#include "repos.h"
svn_error_t *
svn_repos_check_revision_access(svn_repos_revision_access_level_t *access_level,
svn_repos_t *repos,
svn_revnum_t revision,
svn_repos_authz_func_t authz_read_func,
void *authz_read_baton,
apr_pool_t *pool) {
svn_fs_t *fs = svn_repos_fs(repos);
svn_fs_root_t *rev_root;
apr_hash_t *changes;
apr_hash_index_t *hi;
svn_boolean_t found_readable = FALSE;
svn_boolean_t found_unreadable = FALSE;
apr_pool_t *subpool;
*access_level = svn_repos_revision_access_full;
if (! authz_read_func)
return SVN_NO_ERROR;
SVN_ERR(svn_fs_revision_root(&rev_root, fs, revision, pool));
SVN_ERR(svn_fs_paths_changed(&changes, rev_root, pool));
if (apr_hash_count(changes) == 0)
return SVN_NO_ERROR;
subpool = svn_pool_create(pool);
for (hi = apr_hash_first(NULL, changes); hi; hi = apr_hash_next(hi)) {
const void *key;
void *val;
svn_fs_path_change_t *change;
svn_boolean_t readable;
svn_pool_clear(subpool);
apr_hash_this(hi, &key, NULL, &val);
change = val;
SVN_ERR(authz_read_func(&readable, rev_root, key,
authz_read_baton, subpool));
if (! readable)
found_unreadable = TRUE;
else
found_readable = TRUE;
if (found_readable && found_unreadable)
goto decision;
switch (change->change_kind) {
case svn_fs_path_change_add:
case svn_fs_path_change_replace: {
const char *copyfrom_path;
svn_revnum_t copyfrom_rev;
SVN_ERR(svn_fs_copied_from(&copyfrom_rev, &copyfrom_path,
rev_root, key, subpool));
if (copyfrom_path && SVN_IS_VALID_REVNUM(copyfrom_rev)) {
svn_fs_root_t *copyfrom_root;
SVN_ERR(svn_fs_revision_root(&copyfrom_root, fs,
copyfrom_rev, subpool));
SVN_ERR(authz_read_func(&readable,
copyfrom_root, copyfrom_path,
authz_read_baton, subpool));
if (! readable)
found_unreadable = TRUE;
if (found_readable && found_unreadable)
goto decision;
}
}
break;
case svn_fs_path_change_delete:
case svn_fs_path_change_modify:
default:
break;
}
}
decision:
svn_pool_destroy(subpool);
if (! found_readable)
*access_level = svn_repos_revision_access_none;
else if (found_unreadable)
*access_level = svn_repos_revision_access_partial;
return SVN_NO_ERROR;
}
static svn_error_t *
detect_changed(apr_hash_t **changed,
svn_fs_root_t *root,
svn_fs_t *fs,
svn_repos_authz_func_t authz_read_func,
void *authz_read_baton,
apr_pool_t *pool) {
apr_hash_t *changes;
apr_hash_index_t *hi;
apr_pool_t *subpool = svn_pool_create(pool);
svn_boolean_t found_readable = FALSE;
svn_boolean_t found_unreadable = FALSE;
*changed = apr_hash_make(pool);
SVN_ERR(svn_fs_paths_changed(&changes, root, pool));
if (apr_hash_count(changes) == 0)
return SVN_NO_ERROR;
for (hi = apr_hash_first(pool, changes); hi; hi = apr_hash_next(hi)) {
const void *key;
void *val;
svn_fs_path_change_t *change;
const char *path;
char action;
svn_log_changed_path_t *item;
svn_pool_clear(subpool);
apr_hash_this(hi, &key, NULL, &val);
path = (const char *) key;
change = val;
if (authz_read_func) {
svn_boolean_t readable;
SVN_ERR(authz_read_func(&readable,
root, path,
authz_read_baton, subpool));
if (! readable) {
found_unreadable = TRUE;
continue;
}
}
found_readable = TRUE;
switch (change->change_kind) {
case svn_fs_path_change_reset:
continue;
case svn_fs_path_change_add:
action = 'A';
break;
case svn_fs_path_change_replace:
action = 'R';
break;
case svn_fs_path_change_delete:
action = 'D';
break;
case svn_fs_path_change_modify:
default:
action = 'M';
break;
}
item = apr_pcalloc(pool, sizeof(*item));
item->action = action;
item->copyfrom_rev = SVN_INVALID_REVNUM;
if ((action == 'A') || (action == 'R')) {
const char *copyfrom_path;
svn_revnum_t copyfrom_rev;
SVN_ERR(svn_fs_copied_from(&copyfrom_rev, &copyfrom_path,
root, path, subpool));
if (copyfrom_path && SVN_IS_VALID_REVNUM(copyfrom_rev)) {
svn_boolean_t readable = TRUE;
if (authz_read_func) {
svn_fs_root_t *copyfrom_root;
SVN_ERR(svn_fs_revision_root(&copyfrom_root, fs,
copyfrom_rev, subpool));
SVN_ERR(authz_read_func(&readable,
copyfrom_root, copyfrom_path,
authz_read_baton, subpool));
if (! readable)
found_unreadable = TRUE;
}
if (readable) {
item->copyfrom_path = apr_pstrdup(pool, copyfrom_path);
item->copyfrom_rev = copyfrom_rev;
}
}
}
apr_hash_set(*changed, apr_pstrdup(pool, path),
APR_HASH_KEY_STRING, item);
}
svn_pool_destroy(subpool);
if (! found_readable)
return svn_error_create(SVN_ERR_AUTHZ_UNREADABLE,
NULL, NULL);
if (found_unreadable)
return svn_error_create(SVN_ERR_AUTHZ_PARTIALLY_READABLE,
NULL, NULL);
return SVN_NO_ERROR;
}
struct path_info {
svn_stringbuf_t *path;
svn_revnum_t history_rev;
svn_boolean_t done;
svn_boolean_t first_time;
svn_fs_history_t *hist;
apr_pool_t *newpool;
apr_pool_t *oldpool;
};
static svn_error_t *
get_history(struct path_info *info,
svn_fs_t *fs,
svn_boolean_t strict,
svn_repos_authz_func_t authz_read_func,
void *authz_read_baton,
svn_revnum_t start,
apr_pool_t *pool) {
svn_fs_root_t *history_root = NULL;
svn_fs_history_t *hist;
apr_pool_t *subpool;
const char *path;
if (info->hist) {
subpool = info->newpool;
SVN_ERR(svn_fs_history_prev(&info->hist, info->hist,
strict ? FALSE : TRUE, subpool));
hist = info->hist;
} else {
subpool = svn_pool_create(pool);
SVN_ERR(svn_fs_revision_root(&history_root, fs, info->history_rev,
subpool));
SVN_ERR(svn_fs_node_history(&hist, history_root, info->path->data,
subpool));
SVN_ERR(svn_fs_history_prev(&hist, hist, strict ? FALSE : TRUE,
subpool));
if (info->first_time)
info->first_time = FALSE;
else
SVN_ERR(svn_fs_history_prev(&hist, hist, strict ? FALSE : TRUE,
subpool));
}
if (! hist) {
svn_pool_destroy(subpool);
if (info->oldpool)
svn_pool_destroy(info->oldpool);
info->done = TRUE;
return SVN_NO_ERROR;
}
SVN_ERR(svn_fs_history_location(&path, &info->history_rev,
hist, subpool));
svn_stringbuf_set(info->path, path);
if (info->history_rev < start) {
svn_pool_destroy(subpool);
if (info->oldpool)
svn_pool_destroy(info->oldpool);
info->done = TRUE;
return SVN_NO_ERROR;
}
if (authz_read_func) {
svn_boolean_t readable;
SVN_ERR(svn_fs_revision_root(&history_root, fs,
info->history_rev,
subpool));
SVN_ERR(authz_read_func(&readable, history_root,
info->path->data,
authz_read_baton,
subpool));
if (! readable)
info->done = TRUE;
}
if (! info->hist) {
svn_pool_destroy(subpool);
} else {
apr_pool_t *temppool = info->oldpool;
info->oldpool = info->newpool;
svn_pool_clear(temppool);
info->newpool = temppool;
}
return SVN_NO_ERROR;
}
static svn_error_t *
check_history(svn_boolean_t *changed,
struct path_info *info,
svn_fs_t *fs,
svn_revnum_t current,
svn_boolean_t strict,
svn_repos_authz_func_t authz_read_func,
void *authz_read_baton,
svn_revnum_t start,
apr_pool_t *pool) {
if (info->done)
return SVN_NO_ERROR;
if (info->history_rev < current)
return SVN_NO_ERROR;
*changed = TRUE;
SVN_ERR(get_history(info, fs, strict, authz_read_func,
authz_read_baton, start, pool));
return SVN_NO_ERROR;
}
static svn_revnum_t
next_history_rev(apr_array_header_t *histories) {
svn_revnum_t next_rev = SVN_INVALID_REVNUM;
int i;
for (i = 0; i < histories->nelts; ++i) {
struct path_info *info = APR_ARRAY_IDX(histories, i,
struct path_info *);
if (info->done)
continue;
if (info->history_rev > next_rev)
next_rev = info->history_rev;
}
return next_rev;
}
static svn_error_t *
fs_mergeinfo_changed(svn_mergeinfo_catalog_t *deleted_mergeinfo_catalog,
svn_mergeinfo_catalog_t *added_mergeinfo_catalog,
svn_fs_t *fs,
svn_revnum_t rev,
apr_pool_t *pool)
{
apr_hash_t *changes;
svn_fs_root_t *root;
apr_pool_t *subpool = NULL, *iterpool;
apr_hash_index_t *hi;
*deleted_mergeinfo_catalog = apr_hash_make(pool);
*added_mergeinfo_catalog = apr_hash_make(pool);
if (rev == 0)
return SVN_NO_ERROR;
subpool = svn_pool_create(pool);
SVN_ERR(svn_fs_revision_root(&root, fs, rev, subpool));
SVN_ERR(svn_fs_paths_changed(&changes, root, subpool));
if (apr_hash_count(changes) == 0) {
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
iterpool = svn_pool_create(subpool);
for (hi = apr_hash_first(pool, changes); hi; hi = apr_hash_next(hi)) {
const void *key;
void *val;
svn_fs_path_change_t *change;
const char *changed_path, *base_path = NULL;
svn_revnum_t base_rev = SVN_INVALID_REVNUM;
svn_string_t *prev_mergeinfo_value = NULL, *mergeinfo_value;
svn_pool_clear(iterpool);
apr_hash_this(hi, &key, NULL, &val);
changed_path = key;
change = val;
if (! change->prop_mod)
continue;
switch (change->change_kind) {
case svn_fs_path_change_add:
case svn_fs_path_change_replace: {
const char *copyfrom_path;
svn_revnum_t copyfrom_rev;
SVN_ERR(svn_fs_copied_from(&copyfrom_rev, &copyfrom_path,
root, changed_path, iterpool));
if (copyfrom_path && SVN_IS_VALID_REVNUM(copyfrom_rev)) {
base_path = apr_pstrdup(subpool, copyfrom_path);
base_rev = copyfrom_rev;
}
break;
}
case svn_fs_path_change_modify: {
svn_revnum_t appeared_rev;
SVN_ERR(svn_repos__prev_location(&appeared_rev, &base_path,
&base_rev, fs, rev,
changed_path, iterpool));
if (! (base_path && SVN_IS_VALID_REVNUM(base_rev)
&& (appeared_rev == rev))) {
base_path = changed_path;
base_rev = rev - 1;
}
break;
}
case svn_fs_path_change_delete:
case svn_fs_path_change_reset:
default:
continue;
}
if (base_path && SVN_IS_VALID_REVNUM(base_rev)) {
svn_fs_root_t *base_root;
apr_array_header_t *query_paths =
apr_array_make(iterpool, 1, sizeof(const char *));
svn_mergeinfo_t base_mergeinfo;
svn_mergeinfo_catalog_t base_catalog;
SVN_ERR(svn_fs_revision_root(&base_root, fs, base_rev, iterpool));
APR_ARRAY_PUSH(query_paths, const char *) = base_path;
SVN_ERR(svn_fs_get_mergeinfo(&base_catalog, base_root, query_paths,
svn_mergeinfo_inherited, FALSE,
iterpool));
base_mergeinfo = apr_hash_get(base_catalog, base_path,
APR_HASH_KEY_STRING);
if (base_mergeinfo)
SVN_ERR(svn_mergeinfo_to_string(&prev_mergeinfo_value,
base_mergeinfo,
iterpool));
}
SVN_ERR(svn_fs_node_prop(&mergeinfo_value, root, changed_path,
SVN_PROP_MERGEINFO, iterpool));
if ((prev_mergeinfo_value && (! mergeinfo_value))
|| ((! prev_mergeinfo_value) && mergeinfo_value)
|| (prev_mergeinfo_value && mergeinfo_value
&& (! svn_string_compare(mergeinfo_value,
prev_mergeinfo_value)))) {
svn_mergeinfo_t prev_mergeinfo = NULL, mergeinfo = NULL;
svn_mergeinfo_t deleted, added;
const char *hash_path;
if (mergeinfo_value)
SVN_ERR(svn_mergeinfo_parse(&mergeinfo,
mergeinfo_value->data, iterpool));
if (prev_mergeinfo_value)
SVN_ERR(svn_mergeinfo_parse(&prev_mergeinfo,
prev_mergeinfo_value->data, iterpool));
SVN_ERR(svn_mergeinfo_diff(&deleted, &added, prev_mergeinfo,
mergeinfo, FALSE, iterpool));
hash_path = apr_pstrdup(pool, changed_path);
apr_hash_set(*deleted_mergeinfo_catalog, hash_path,
APR_HASH_KEY_STRING, svn_mergeinfo_dup(deleted, pool));
apr_hash_set(*added_mergeinfo_catalog, hash_path,
APR_HASH_KEY_STRING, svn_mergeinfo_dup(added, pool));
}
}
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
static svn_error_t *
get_combined_mergeinfo_changes(svn_mergeinfo_t *combined_mergeinfo,
svn_fs_t *fs,
const apr_array_header_t *paths,
svn_revnum_t rev,
apr_pool_t *pool) {
svn_mergeinfo_catalog_t added_mergeinfo_catalog, deleted_mergeinfo_catalog;
apr_hash_index_t *hi;
svn_fs_root_t *root;
apr_pool_t *subpool, *iterpool;
int i;
*combined_mergeinfo = apr_hash_make(pool);
if (rev == 0)
return SVN_NO_ERROR;
if (! paths->nelts)
return SVN_NO_ERROR;
subpool = svn_pool_create(pool);
SVN_ERR(svn_fs_revision_root(&root, fs, rev, subpool));
SVN_ERR(fs_mergeinfo_changed(&deleted_mergeinfo_catalog,
&added_mergeinfo_catalog,
fs, rev, subpool));
iterpool = svn_pool_create(subpool);
for (i = 0; i < paths->nelts; i++) {
const char *path = APR_ARRAY_IDX(paths, i, const char *);
const char *prev_path;
svn_revnum_t appeared_rev, prev_rev;
svn_fs_root_t *prev_root;
svn_mergeinfo_catalog_t catalog;
svn_mergeinfo_t prev_mergeinfo, mergeinfo, deleted, added;
apr_array_header_t *query_paths;
svn_error_t *err;
svn_pool_clear(iterpool);
if (apr_hash_get(deleted_mergeinfo_catalog, path, APR_HASH_KEY_STRING))
continue;
err = svn_repos__prev_location(&appeared_rev, &prev_path, &prev_rev,
fs, rev, path, iterpool);
if (err && (err->apr_err == SVN_ERR_FS_NOT_FOUND)) {
svn_error_clear(err);
err = SVN_NO_ERROR;
continue;
}
SVN_ERR(err);
if (! (prev_path && SVN_IS_VALID_REVNUM(prev_rev)
&& (appeared_rev == rev))) {
prev_path = path;
prev_rev = rev - 1;
}
SVN_ERR(svn_fs_revision_root(&prev_root, fs, prev_rev, iterpool));
query_paths = apr_array_make(iterpool, 1, sizeof(const char *));
APR_ARRAY_PUSH(query_paths, const char *) = prev_path;
err = svn_fs_get_mergeinfo(&catalog, prev_root, query_paths,
svn_mergeinfo_inherited, FALSE, iterpool);
if (err && (err->apr_err == SVN_ERR_FS_NOT_FOUND)) {
svn_error_clear(err);
err = SVN_NO_ERROR;
continue;
}
SVN_ERR(err);
prev_mergeinfo = apr_hash_get(catalog, prev_path, APR_HASH_KEY_STRING);
APR_ARRAY_IDX(query_paths, 0, const char *) = path;
SVN_ERR(svn_fs_get_mergeinfo(&catalog, root, query_paths,
svn_mergeinfo_inherited, FALSE, iterpool));
mergeinfo = apr_hash_get(catalog, path, APR_HASH_KEY_STRING);
SVN_ERR(svn_mergeinfo_diff(&deleted, &added, prev_mergeinfo,
mergeinfo, FALSE, iterpool));
mergeinfo = deleted;
SVN_ERR(svn_mergeinfo_merge(mergeinfo, added, iterpool));
SVN_ERR(svn_mergeinfo_merge(*combined_mergeinfo,
svn_mergeinfo_dup(mergeinfo, pool),
pool));
}
svn_pool_destroy(iterpool);
for (hi = apr_hash_first(NULL, added_mergeinfo_catalog);
hi; hi = apr_hash_next(hi)) {
const void *key;
apr_ssize_t klen;
void *val;
const char *changed_path;
svn_mergeinfo_t added_mergeinfo, deleted_mergeinfo;
apr_hash_this(hi, &key, &klen, &val);
changed_path = key;
added_mergeinfo = val;
for (i = 0; i < paths->nelts; i++) {
const char *path = APR_ARRAY_IDX(paths, i, const char *);
if (! svn_path_is_ancestor(path, changed_path))
continue;
deleted_mergeinfo =
apr_hash_get(deleted_mergeinfo_catalog, key, klen);
SVN_ERR(svn_mergeinfo_merge(*combined_mergeinfo,
svn_mergeinfo_dup(deleted_mergeinfo,
pool),
pool));
SVN_ERR(svn_mergeinfo_merge(*combined_mergeinfo,
svn_mergeinfo_dup(added_mergeinfo,
pool),
pool));
break;
}
}
svn_pool_clear(subpool);
return SVN_NO_ERROR;
}
static svn_error_t *
fill_log_entry(svn_log_entry_t *log_entry,
svn_revnum_t rev,
svn_fs_t *fs,
svn_boolean_t discover_changed_paths,
const apr_array_header_t *revprops,
svn_repos_authz_func_t authz_read_func,
void *authz_read_baton,
apr_pool_t *pool) {
apr_hash_t *r_props, *changed_paths = NULL;
svn_boolean_t get_revprops = TRUE, censor_revprops = FALSE;
if ((rev > 0)
&& (authz_read_func || discover_changed_paths)) {
svn_fs_root_t *newroot;
svn_error_t *patherr;
SVN_ERR(svn_fs_revision_root(&newroot, fs, rev, pool));
patherr = detect_changed(&changed_paths,
newroot, fs,
authz_read_func, authz_read_baton,
pool);
if (patherr
&& patherr->apr_err == SVN_ERR_AUTHZ_UNREADABLE) {
svn_error_clear(patherr);
changed_paths = NULL;
get_revprops = FALSE;
} else if (patherr
&& patherr->apr_err == SVN_ERR_AUTHZ_PARTIALLY_READABLE) {
svn_error_clear(patherr);
censor_revprops = TRUE;
} else if (patherr)
return patherr;
if (! discover_changed_paths)
changed_paths = NULL;
}
if (get_revprops) {
SVN_ERR(svn_fs_revision_proplist(&r_props, fs, rev, pool));
if (revprops == NULL) {
if (censor_revprops) {
log_entry->revprops = apr_hash_make(pool);
apr_hash_set(log_entry->revprops, SVN_PROP_REVISION_AUTHOR,
APR_HASH_KEY_STRING,
apr_hash_get(r_props, SVN_PROP_REVISION_AUTHOR,
APR_HASH_KEY_STRING));
apr_hash_set(log_entry->revprops, SVN_PROP_REVISION_DATE,
APR_HASH_KEY_STRING,
apr_hash_get(r_props, SVN_PROP_REVISION_DATE,
APR_HASH_KEY_STRING));
} else
log_entry->revprops = r_props;
} else {
int i;
for (i = 0; i < revprops->nelts; i++) {
char *name = APR_ARRAY_IDX(revprops, i, char *);
svn_string_t *value = apr_hash_get(r_props, name,
APR_HASH_KEY_STRING);
if (censor_revprops
&& !(strcmp(name, SVN_PROP_REVISION_AUTHOR) == 0
|| strcmp(name, SVN_PROP_REVISION_DATE) == 0))
continue;
if (log_entry->revprops == NULL)
log_entry->revprops = apr_hash_make(pool);
apr_hash_set(log_entry->revprops, name,
APR_HASH_KEY_STRING, value);
}
}
}
log_entry->changed_paths = changed_paths;
log_entry->revision = rev;
return SVN_NO_ERROR;
}
static svn_error_t *
send_log(svn_revnum_t rev,
svn_fs_t *fs,
svn_boolean_t discover_changed_paths,
const apr_array_header_t *revprops,
svn_boolean_t has_children,
svn_log_entry_receiver_t receiver,
void *receiver_baton,
svn_repos_authz_func_t authz_read_func,
void *authz_read_baton,
apr_pool_t *pool) {
svn_log_entry_t *log_entry;
log_entry = svn_log_entry_create(pool);
SVN_ERR(fill_log_entry(log_entry, rev, fs, discover_changed_paths,
revprops, authz_read_func, authz_read_baton,
pool));
log_entry->has_children = has_children;
SVN_ERR((*receiver)(receiver_baton, log_entry, pool));
return SVN_NO_ERROR;
}
#define MAX_OPEN_HISTORIES 32
static svn_error_t *
get_path_histories(apr_array_header_t **histories,
svn_fs_t *fs,
const apr_array_header_t *paths,
svn_revnum_t hist_start,
svn_revnum_t hist_end,
svn_boolean_t strict_node_history,
svn_repos_authz_func_t authz_read_func,
void *authz_read_baton,
apr_pool_t *pool) {
svn_fs_root_t *root;
apr_pool_t *iterpool;
int i;
*histories = apr_array_make(pool, paths->nelts,
sizeof(struct path_info *));
SVN_ERR(svn_fs_revision_root(&root, fs, hist_end, pool));
iterpool = svn_pool_create(pool);
for (i = 0; i < paths->nelts; i++) {
const char *this_path = APR_ARRAY_IDX(paths, i, const char *);
struct path_info *info = apr_palloc(pool,
sizeof(struct path_info));
if (authz_read_func) {
svn_boolean_t readable;
svn_pool_clear(iterpool);
SVN_ERR(authz_read_func(&readable, root, this_path,
authz_read_baton, iterpool));
if (! readable)
return svn_error_create(SVN_ERR_AUTHZ_UNREADABLE, NULL, NULL);
}
info->path = svn_stringbuf_create(this_path, pool);
info->done = FALSE;
info->history_rev = hist_end;
info->first_time = TRUE;
if (i < MAX_OPEN_HISTORIES) {
SVN_ERR(svn_fs_node_history(&info->hist, root, this_path, pool));
info->newpool = svn_pool_create(pool);
info->oldpool = svn_pool_create(pool);
} else {
info->hist = NULL;
info->oldpool = NULL;
info->newpool = NULL;
}
SVN_ERR(get_history(info, fs,
strict_node_history,
authz_read_func, authz_read_baton,
hist_start, pool));
APR_ARRAY_PUSH(*histories, struct path_info *) = info;
}
svn_pool_destroy(iterpool);
return SVN_NO_ERROR;
}
static void *
array_pop_front(apr_array_header_t *arr) {
void *item = arr->elts;
if (apr_is_empty_array(arr))
return NULL;
arr->elts += arr->elt_size;
arr->nelts -= 1;
arr->nalloc -= 1;
return item;
}
struct path_list_range {
apr_array_header_t *paths;
svn_merge_range_t range;
};
struct rangelist_path {
apr_array_header_t *rangelist;
const char *path;
};
static int
compare_rangelist_paths(const void *a, const void *b) {
struct rangelist_path *rpa = *((struct rangelist_path **) a);
struct rangelist_path *rpb = *((struct rangelist_path **) b);
svn_merge_range_t *mra = APR_ARRAY_IDX(rpa->rangelist, 0,
svn_merge_range_t *);
svn_merge_range_t *mrb = APR_ARRAY_IDX(rpb->rangelist, 0,
svn_merge_range_t *);
if (mra->start < mrb->start)
return -1;
if (mra->start > mrb->start)
return 1;
if (mra->end < mrb->end)
return -1;
if (mra->end > mrb->end)
return 1;
return 0;
}
static svn_error_t *
combine_mergeinfo_path_lists(apr_array_header_t **combined_list,
svn_mergeinfo_t mergeinfo,
apr_pool_t *pool) {
apr_hash_index_t *hi;
apr_array_header_t *rangelist_paths;
apr_pool_t *subpool = svn_pool_create(pool);
rangelist_paths = apr_array_make(subpool, apr_hash_count(mergeinfo),
sizeof(struct rangelist_path *));
for (hi = apr_hash_first(subpool, mergeinfo); hi;
hi = apr_hash_next(hi)) {
int i;
struct rangelist_path *rp = apr_palloc(subpool, sizeof(*rp));
apr_hash_this(hi, (void *) &rp->path, NULL,
(void *) &rp->rangelist);
APR_ARRAY_PUSH(rangelist_paths, struct rangelist_path *) = rp;
rp->rangelist = svn_rangelist_dup(rp->rangelist, subpool);
for (i = 0; i < rp->rangelist->nelts; i++)
APR_ARRAY_IDX(rp->rangelist, i, svn_merge_range_t *)->start += 1;
}
*combined_list = apr_array_make(pool, 0, sizeof(struct path_list_range *));
while (rangelist_paths->nelts > 1) {
svn_revnum_t youngest, next_youngest, tail, youngest_end;
struct path_list_range *plr;
struct rangelist_path *rp;
int num_revs;
int i;
qsort(rangelist_paths->elts, rangelist_paths->nelts,
rangelist_paths->elt_size, compare_rangelist_paths);
rp = APR_ARRAY_IDX(rangelist_paths, 0, struct rangelist_path *);
youngest =
APR_ARRAY_IDX(rp->rangelist, 0, struct svn_merge_range_t *)->start;
next_youngest = youngest;
for (num_revs = 1; next_youngest == youngest; num_revs++) {
if (num_revs == rangelist_paths->nelts) {
num_revs += 1;
break;
}
rp = APR_ARRAY_IDX(rangelist_paths, num_revs,
struct rangelist_path *);
next_youngest = APR_ARRAY_IDX(rp->rangelist, 0,
struct svn_merge_range_t *)->start;
}
num_revs -= 1;
youngest_end =
APR_ARRAY_IDX(APR_ARRAY_IDX(rangelist_paths, 0,
struct rangelist_path *)->rangelist,
0, svn_merge_range_t *)->end;
if ( (next_youngest == youngest) || (youngest_end < next_youngest) )
tail = youngest_end;
else
tail = next_youngest - 1;
plr = apr_palloc(pool, sizeof(*plr));
plr->range.start = youngest;
plr->range.end = tail;
plr->paths = apr_array_make(pool, num_revs, sizeof(const char *));
for (i = 0; i < num_revs; i++)
APR_ARRAY_PUSH(plr->paths, const char *) =
APR_ARRAY_IDX(rangelist_paths, i, struct rangelist_path *)->path;
APR_ARRAY_PUSH(*combined_list, struct path_list_range *) = plr;
for (i = 0; i < num_revs; i++) {
svn_merge_range_t *range;
rp = APR_ARRAY_IDX(rangelist_paths, i, struct rangelist_path *);
range = APR_ARRAY_IDX(rp->rangelist, 0, svn_merge_range_t *);
range->start = tail + 1;
if (range->start > range->end) {
if (rp->rangelist->nelts == 1) {
array_pop_front(rangelist_paths);
i--;
num_revs--;
} else {
array_pop_front(rp->rangelist);
}
}
}
}
if (rangelist_paths->nelts > 0) {
struct rangelist_path *first_rp =
APR_ARRAY_IDX(rangelist_paths, 0, struct rangelist_path *);
while (first_rp->rangelist->nelts > 0) {
struct path_list_range *plr = apr_palloc(pool, sizeof(*plr));
plr->paths = apr_array_make(pool, 1, sizeof(const char *));
APR_ARRAY_PUSH(plr->paths, const char *) = first_rp->path;
plr->range = *APR_ARRAY_IDX(first_rp->rangelist, 0,
svn_merge_range_t *);
array_pop_front(first_rp->rangelist);
APR_ARRAY_PUSH(*combined_list, struct path_list_range *) = plr;
}
}
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
static svn_error_t *do_logs(svn_fs_t *fs,
const apr_array_header_t *paths,
svn_revnum_t hist_start,
svn_revnum_t hist_end,
int limit,
svn_boolean_t discover_changed_paths,
svn_boolean_t strict_node_history,
svn_boolean_t include_merged_revisions,
const apr_array_header_t *revprops,
svn_boolean_t descending_order,
svn_log_entry_receiver_t receiver,
void *receiver_baton,
svn_repos_authz_func_t authz_read_func,
void *authz_read_baton,
apr_pool_t *pool);
static svn_error_t *
handle_merged_revisions(svn_revnum_t rev,
svn_fs_t *fs,
svn_mergeinfo_t mergeinfo,
svn_boolean_t discover_changed_paths,
svn_boolean_t strict_node_history,
const apr_array_header_t *revprops,
svn_log_entry_receiver_t receiver,
void *receiver_baton,
svn_repos_authz_func_t authz_read_func,
void *authz_read_baton,
apr_pool_t *pool) {
apr_array_header_t *combined_list;
svn_log_entry_t *empty_log_entry;
apr_pool_t *iterpool;
int i;
if (apr_hash_count(mergeinfo) == 0)
return SVN_NO_ERROR;
SVN_ERR(combine_mergeinfo_path_lists(&combined_list, mergeinfo, pool));
iterpool = svn_pool_create(pool);
for (i = combined_list->nelts - 1; i >= 0; i--) {
svn_error_t *err;
struct path_list_range *pl_range
= APR_ARRAY_IDX(combined_list, i, struct path_list_range *);
svn_pool_clear(iterpool);
err = do_logs(fs, pl_range->paths, pl_range->range.start,
pl_range->range.end, 0, discover_changed_paths,
strict_node_history, TRUE, revprops, TRUE,
receiver, receiver_baton, authz_read_func,
authz_read_baton, iterpool);
if (err && (err->apr_err == SVN_ERR_FS_NOT_FOUND ||
err->apr_err == SVN_ERR_FS_NO_SUCH_REVISION)) {
svn_error_clear(err);
continue;
}
SVN_ERR(err);
}
svn_pool_destroy(iterpool);
empty_log_entry = svn_log_entry_create(pool);
empty_log_entry->revision = SVN_INVALID_REVNUM;
SVN_ERR((*receiver)(receiver_baton, empty_log_entry, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
do_logs(svn_fs_t *fs,
const apr_array_header_t *paths,
svn_revnum_t hist_start,
svn_revnum_t hist_end,
int limit,
svn_boolean_t discover_changed_paths,
svn_boolean_t strict_node_history,
svn_boolean_t include_merged_revisions,
const apr_array_header_t *revprops,
svn_boolean_t descending_order,
svn_log_entry_receiver_t receiver,
void *receiver_baton,
svn_repos_authz_func_t authz_read_func,
void *authz_read_baton,
apr_pool_t *pool) {
apr_pool_t *iterpool;
apr_array_header_t *revs = NULL;
apr_hash_t *rev_mergeinfo = NULL;
svn_revnum_t current;
apr_array_header_t *histories;
svn_boolean_t any_histories_left = TRUE;
int send_count = 0;
int i;
SVN_ERR(get_path_histories(&histories, fs, paths, hist_start, hist_end,
strict_node_history, authz_read_func,
authz_read_baton, pool));
iterpool = svn_pool_create(pool);
for (current = hist_end;
any_histories_left;
current = next_history_rev(histories)) {
svn_boolean_t changed = FALSE;
any_histories_left = FALSE;
svn_pool_clear(iterpool);
for (i = 0; i < histories->nelts; i++) {
struct path_info *info = APR_ARRAY_IDX(histories, i,
struct path_info *);
SVN_ERR(check_history(&changed, info, fs, current,
strict_node_history, authz_read_func,
authz_read_baton, hist_start, pool));
if (! info->done)
any_histories_left = TRUE;
}
if (changed) {
svn_mergeinfo_t mergeinfo = NULL;
svn_boolean_t has_children = FALSE;
if (include_merged_revisions) {
apr_array_header_t *cur_paths =
apr_array_make(iterpool, paths->nelts, sizeof(const char *));
for (i = 0; i < histories->nelts; i++) {
struct path_info *info = APR_ARRAY_IDX(histories, i,
struct path_info *);
APR_ARRAY_PUSH(cur_paths, const char *) = info->path->data;
}
SVN_ERR(get_combined_mergeinfo_changes(&mergeinfo, fs, cur_paths,
current, iterpool));
has_children = (apr_hash_count(mergeinfo) > 0);
}
if (descending_order) {
SVN_ERR(send_log(current, fs, discover_changed_paths,
revprops, has_children, receiver, receiver_baton,
authz_read_func, authz_read_baton, iterpool));
if (has_children) {
SVN_ERR(handle_merged_revisions(current, fs, mergeinfo,
discover_changed_paths,
strict_node_history, revprops,
receiver, receiver_baton,
authz_read_func,
authz_read_baton,
iterpool));
}
if (limit && ++send_count >= limit)
break;
}
else {
if (! revs)
revs = apr_array_make(pool, 64, sizeof(svn_revnum_t));
APR_ARRAY_PUSH(revs, svn_revnum_t) = current;
if (mergeinfo) {
svn_revnum_t *cur_rev = apr_palloc(pool, sizeof(*cur_rev));
*cur_rev = current;
if (! rev_mergeinfo)
rev_mergeinfo = apr_hash_make(pool);
apr_hash_set(rev_mergeinfo, cur_rev, sizeof(*cur_rev),
svn_mergeinfo_dup(mergeinfo, pool));
}
}
}
}
svn_pool_destroy(iterpool);
if (revs) {
iterpool = svn_pool_create(pool);
for (i = 0; i < revs->nelts; ++i) {
svn_mergeinfo_t mergeinfo;
svn_boolean_t has_children = FALSE;
svn_pool_clear(iterpool);
current = APR_ARRAY_IDX(revs, revs->nelts - i - 1, svn_revnum_t);
if (rev_mergeinfo) {
mergeinfo = apr_hash_get(rev_mergeinfo, &current,
sizeof(svn_revnum_t));
has_children = (apr_hash_count(mergeinfo) > 0);
}
SVN_ERR(send_log(current, fs,
discover_changed_paths, revprops, has_children,
receiver, receiver_baton, authz_read_func,
authz_read_baton, iterpool));
if (has_children) {
SVN_ERR(handle_merged_revisions(current, fs, mergeinfo,
discover_changed_paths,
strict_node_history, revprops,
receiver, receiver_baton,
authz_read_func,
authz_read_baton,
iterpool));
}
if (limit && i + 1 >= limit)
break;
}
svn_pool_destroy(iterpool);
}
return SVN_NO_ERROR;
}
svn_error_t *
svn_repos_get_logs4(svn_repos_t *repos,
const apr_array_header_t *paths,
svn_revnum_t start,
svn_revnum_t end,
int limit,
svn_boolean_t discover_changed_paths,
svn_boolean_t strict_node_history,
svn_boolean_t include_merged_revisions,
const apr_array_header_t *revprops,
svn_repos_authz_func_t authz_read_func,
void *authz_read_baton,
svn_log_entry_receiver_t receiver,
void *receiver_baton,
apr_pool_t *pool) {
svn_revnum_t head = SVN_INVALID_REVNUM;
svn_fs_t *fs = repos->fs;
svn_boolean_t descending_order;
svn_revnum_t hist_start = start;
svn_revnum_t hist_end = end;
SVN_ERR(svn_fs_youngest_rev(&head, fs, pool));
if (! SVN_IS_VALID_REVNUM(start))
start = head;
if (! SVN_IS_VALID_REVNUM(end))
end = head;
if (start > head)
return svn_error_createf
(SVN_ERR_FS_NO_SUCH_REVISION, 0,
_("No such revision %ld"), start);
if (end > head)
return svn_error_createf
(SVN_ERR_FS_NO_SUCH_REVISION, 0,
_("No such revision %ld"), end);
descending_order = start >= end;
if (descending_order) {
hist_start = end;
hist_end = start;
}
if (! paths)
paths = apr_array_make(pool, 0, sizeof(const char *));
if ((! include_merged_revisions)
&& ((! paths->nelts)
|| ((paths->nelts == 1)
&& (svn_path_is_empty(APR_ARRAY_IDX(paths, 0, const char *))
|| (strcmp(APR_ARRAY_IDX(paths, 0, const char *),
"/") == 0))))) {
int send_count = 0;
int i;
apr_pool_t *iterpool = svn_pool_create(pool);
send_count = hist_end - hist_start + 1;
if (limit && send_count > limit)
send_count = limit;
for (i = 0; i < send_count; ++i) {
svn_revnum_t rev = hist_start + i;
svn_pool_clear(iterpool);
if (descending_order)
rev = hist_end - i;
SVN_ERR(send_log(rev, fs, discover_changed_paths, revprops, FALSE,
receiver, receiver_baton, authz_read_func,
authz_read_baton, iterpool));
}
svn_pool_destroy(iterpool);
return SVN_NO_ERROR;
}
return do_logs(repos->fs, paths, hist_start, hist_end, limit,
discover_changed_paths, strict_node_history,
include_merged_revisions, revprops, descending_order,
receiver, receiver_baton,
authz_read_func, authz_read_baton, pool);
}
svn_error_t *
svn_repos_get_logs3(svn_repos_t *repos,
const apr_array_header_t *paths,
svn_revnum_t start,
svn_revnum_t end,
int limit,
svn_boolean_t discover_changed_paths,
svn_boolean_t strict_node_history,
svn_repos_authz_func_t authz_read_func,
void *authz_read_baton,
svn_log_message_receiver_t receiver,
void *receiver_baton,
apr_pool_t *pool) {
svn_log_entry_receiver_t receiver2;
void *receiver2_baton;
svn_compat_wrap_log_receiver(&receiver2, &receiver2_baton,
receiver, receiver_baton,
pool);
return svn_repos_get_logs4(repos, paths, start, end, limit,
discover_changed_paths, strict_node_history,
FALSE, svn_compat_log_revprops_in(pool),
authz_read_func, authz_read_baton,
receiver2, receiver2_baton,
pool);
}
svn_error_t *
svn_repos_get_logs2(svn_repos_t *repos,
const apr_array_header_t *paths,
svn_revnum_t start,
svn_revnum_t end,
svn_boolean_t discover_changed_paths,
svn_boolean_t strict_node_history,
svn_repos_authz_func_t authz_read_func,
void *authz_read_baton,
svn_log_message_receiver_t receiver,
void *receiver_baton,
apr_pool_t *pool) {
return svn_repos_get_logs3(repos, paths, start, end, 0,
discover_changed_paths, strict_node_history,
authz_read_func, authz_read_baton, receiver,
receiver_baton, pool);
}
svn_error_t *
svn_repos_get_logs(svn_repos_t *repos,
const apr_array_header_t *paths,
svn_revnum_t start,
svn_revnum_t end,
svn_boolean_t discover_changed_paths,
svn_boolean_t strict_node_history,
svn_log_message_receiver_t receiver,
void *receiver_baton,
apr_pool_t *pool) {
return svn_repos_get_logs3(repos, paths, start, end, 0,
discover_changed_paths, strict_node_history,
NULL, NULL,
receiver, receiver_baton, pool);
}
