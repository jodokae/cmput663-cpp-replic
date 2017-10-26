#include <assert.h>
#include <string.h>
#include <apr_pools.h>
#include <apr_file_io.h>
#include <apr_hash.h>
#include "svn_pools.h"
#include "svn_types.h"
#include "svn_delta.h"
#include "svn_string.h"
#include "svn_error.h"
#include "svn_path.h"
#include "svn_io.h"
#include "svn_config.h"
#include "svn_time.h"
#include "svn_private_config.h"
#include "wc.h"
#include "lock.h"
#include "props.h"
#include "translate.h"
#include "private/svn_wc_private.h"
struct edit_baton {
const char *anchor;
const char *target;
svn_wc_adm_access_t *adm_access;
svn_depth_t default_depth;
svn_boolean_t get_all;
svn_boolean_t no_ignore;
svn_revnum_t *target_revision;
svn_wc_status_func2_t status_func;
void *status_baton;
svn_cancel_func_t cancel_func;
void *cancel_baton;
apr_array_header_t *ignores;
svn_wc_traversal_info_t *traversal_info;
apr_hash_t *externals;
svn_wc_status2_t *anchor_status;
svn_boolean_t root_opened;
const char *repos_root;
apr_hash_t *repos_locks;
};
struct dir_baton {
const char *path;
const char *name;
struct edit_baton *edit_baton;
struct dir_baton *parent_baton;
svn_depth_t depth;
svn_boolean_t excluded;
svn_boolean_t added;
svn_boolean_t prop_changed;
svn_boolean_t text_changed;
apr_hash_t *statii;
apr_pool_t *pool;
const char *url;
svn_revnum_t ood_last_cmt_rev;
apr_time_t ood_last_cmt_date;
svn_node_kind_t ood_kind;
const char *ood_last_cmt_author;
};
struct file_baton {
struct edit_baton *edit_baton;
struct dir_baton *dir_baton;
apr_pool_t *pool;
const char *name;
const char *path;
svn_boolean_t added;
svn_boolean_t text_changed;
svn_boolean_t prop_changed;
const char *url;
svn_revnum_t ood_last_cmt_rev;
apr_time_t ood_last_cmt_date;
svn_node_kind_t ood_kind;
const char *ood_last_cmt_author;
};
static svn_error_t *
assemble_status(svn_wc_status2_t **status,
const char *path,
svn_wc_adm_access_t *adm_access,
const svn_wc_entry_t *entry,
const svn_wc_entry_t *parent_entry,
svn_node_kind_t path_kind, svn_boolean_t path_special,
svn_boolean_t get_all,
svn_boolean_t is_ignored,
apr_hash_t *repos_locks,
const char *repos_root,
apr_pool_t *pool) {
svn_wc_status2_t *stat;
svn_boolean_t has_props;
svn_boolean_t text_modified_p = FALSE;
svn_boolean_t prop_modified_p = FALSE;
svn_boolean_t locked_p = FALSE;
svn_boolean_t switched_p = FALSE;
#if defined(HAVE_SYMLINK)
svn_boolean_t wc_special;
#endif
enum svn_wc_status_kind final_text_status = svn_wc_status_normal;
enum svn_wc_status_kind final_prop_status = svn_wc_status_none;
svn_lock_t *repos_lock = NULL;
if (repos_locks) {
const char *abs_path;
if (entry && entry->url)
abs_path = entry->url + strlen(repos_root);
else if (parent_entry && parent_entry->url)
abs_path = svn_path_join(parent_entry->url + strlen(repos_root),
svn_path_basename(path, pool), pool);
else
abs_path = NULL;
if (abs_path)
repos_lock = apr_hash_get(repos_locks,
svn_path_uri_decode(abs_path, pool),
APR_HASH_KEY_STRING);
}
if (path_kind == svn_node_unknown)
SVN_ERR(svn_io_check_special_path(path, &path_kind, &path_special,
pool));
if (! entry) {
stat = apr_pcalloc(pool, sizeof(*stat));
stat->entry = NULL;
stat->text_status = svn_wc_status_none;
stat->prop_status = svn_wc_status_none;
stat->repos_text_status = svn_wc_status_none;
stat->repos_prop_status = svn_wc_status_none;
stat->locked = FALSE;
stat->copied = FALSE;
stat->switched = FALSE;
if (path_kind != svn_node_none) {
if (is_ignored)
stat->text_status = svn_wc_status_ignored;
else
stat->text_status = svn_wc_status_unversioned;
}
stat->repos_lock = repos_lock;
stat->url = NULL;
stat->ood_last_cmt_rev = SVN_INVALID_REVNUM;
stat->ood_last_cmt_date = 0;
stat->ood_kind = svn_node_none;
stat->ood_last_cmt_author = NULL;
*status = stat;
return SVN_NO_ERROR;
}
if (entry->kind == svn_node_dir) {
if (path_kind == svn_node_dir) {
if (svn_wc__adm_missing(adm_access, path))
final_text_status = svn_wc_status_obstructed;
} else if (path_kind != svn_node_none)
final_text_status = svn_wc_status_obstructed;
}
if (entry->url && parent_entry && parent_entry->url &&
entry != parent_entry) {
if (strcmp(svn_path_uri_encode(svn_path_basename(path, pool), pool),
svn_path_basename(entry->url, pool)))
switched_p = TRUE;
if (! switched_p
&& strcmp(svn_path_dirname(entry->url, pool),
parent_entry->url))
switched_p = TRUE;
}
if (final_text_status != svn_wc_status_obstructed) {
SVN_ERR(svn_wc__has_props(&has_props, path, adm_access, pool));
if (has_props)
final_prop_status = svn_wc_status_normal;
SVN_ERR(svn_wc_props_modified_p(&prop_modified_p, path, adm_access,
pool));
#if defined(HAVE_SYMLINK)
if (has_props)
SVN_ERR(svn_wc__get_special(&wc_special, path, adm_access, pool));
else
wc_special = FALSE;
#endif
if ((entry->kind == svn_node_file)
#if defined(HAVE_SYMLINK)
&& (wc_special == path_special)
#endif
)
SVN_ERR(svn_wc_text_modified_p(&text_modified_p, path, FALSE,
adm_access, pool));
if (text_modified_p)
final_text_status = svn_wc_status_modified;
if (prop_modified_p)
final_prop_status = svn_wc_status_modified;
if (entry->prejfile || entry->conflict_old ||
entry->conflict_new || entry->conflict_wrk) {
svn_boolean_t text_conflict_p, prop_conflict_p;
const char *parent_dir;
if (entry->kind == svn_node_dir)
parent_dir = path;
else
parent_dir = svn_path_dirname(path, pool);
SVN_ERR(svn_wc_conflicted_p(&text_conflict_p, &prop_conflict_p,
parent_dir, entry, pool));
if (text_conflict_p)
final_text_status = svn_wc_status_conflicted;
if (prop_conflict_p)
final_prop_status = svn_wc_status_conflicted;
}
if (entry->schedule == svn_wc_schedule_add
&& final_text_status != svn_wc_status_conflicted) {
final_text_status = svn_wc_status_added;
final_prop_status = svn_wc_status_none;
}
else if (entry->schedule == svn_wc_schedule_replace
&& final_text_status != svn_wc_status_conflicted) {
final_text_status = svn_wc_status_replaced;
final_prop_status = svn_wc_status_none;
}
else if (entry->schedule == svn_wc_schedule_delete
&& final_text_status != svn_wc_status_conflicted) {
final_text_status = svn_wc_status_deleted;
final_prop_status = svn_wc_status_none;
}
if (entry->incomplete
&& (final_text_status != svn_wc_status_deleted)
&& (final_text_status != svn_wc_status_added)) {
final_text_status = svn_wc_status_incomplete;
} else if (path_kind == svn_node_none) {
if (final_text_status != svn_wc_status_deleted)
final_text_status = svn_wc_status_missing;
} else if (path_kind != entry->kind)
final_text_status = svn_wc_status_obstructed;
#if defined(HAVE_SYMLINK)
else if (((! wc_special) && (path_special))
|| (wc_special && (! path_special))
)
final_text_status = svn_wc_status_obstructed;
#endif
if (path_kind == svn_node_dir && entry->kind == svn_node_dir)
SVN_ERR(svn_wc_locked(&locked_p, path, pool));
}
if (! get_all)
if (((final_text_status == svn_wc_status_none)
|| (final_text_status == svn_wc_status_normal))
&& ((final_prop_status == svn_wc_status_none)
|| (final_prop_status == svn_wc_status_normal))
&& (! locked_p) && (! switched_p) && (! entry->lock_token)
&& (! repos_lock) && (! entry->changelist)) {
*status = NULL;
return SVN_NO_ERROR;
}
stat = apr_pcalloc(pool, sizeof(**status));
stat->entry = svn_wc_entry_dup(entry, pool);
stat->text_status = final_text_status;
stat->prop_status = final_prop_status;
stat->repos_text_status = svn_wc_status_none;
stat->repos_prop_status = svn_wc_status_none;
stat->locked = locked_p;
stat->switched = switched_p;
stat->copied = entry->copied;
stat->repos_lock = repos_lock;
stat->url = (entry->url ? entry->url : NULL);
stat->ood_last_cmt_rev = SVN_INVALID_REVNUM;
stat->ood_last_cmt_date = 0;
stat->ood_kind = svn_node_none;
stat->ood_last_cmt_author = NULL;
*status = stat;
return SVN_NO_ERROR;
}
static svn_error_t *
send_status_structure(const char *path,
svn_wc_adm_access_t *adm_access,
const svn_wc_entry_t *entry,
const svn_wc_entry_t *parent_entry,
svn_node_kind_t path_kind,
svn_boolean_t path_special,
svn_boolean_t get_all,
svn_boolean_t is_ignored,
apr_hash_t *repos_locks,
const char *repos_root,
svn_wc_status_func2_t status_func,
void *status_baton,
apr_pool_t *pool) {
svn_wc_status2_t *statstruct;
SVN_ERR(assemble_status(&statstruct, path, adm_access, entry, parent_entry,
path_kind, path_special, get_all, is_ignored,
repos_locks, repos_root, pool));
if (statstruct && (status_func))
(*status_func)(status_baton, path, statstruct);
return SVN_NO_ERROR;
}
static svn_error_t *
collect_ignore_patterns(apr_array_header_t **patterns,
apr_array_header_t *ignores,
svn_wc_adm_access_t *adm_access,
apr_pool_t *pool) {
int i;
const svn_string_t *value;
*patterns = apr_array_make(pool, 1, sizeof(const char *));
for (i = 0; i < ignores->nelts; i++) {
const char *ignore = APR_ARRAY_IDX(ignores, i, const char *);
APR_ARRAY_PUSH(*patterns, const char *) = ignore;
}
SVN_ERR(svn_wc_prop_get(&value, SVN_PROP_IGNORE,
svn_wc_adm_access_path(adm_access), adm_access,
pool));
if (value != NULL)
svn_cstring_split_append(*patterns, value->data, "\n\r", FALSE, pool);
return SVN_NO_ERROR;
}
static svn_boolean_t
is_external_path(apr_hash_t *externals,
const char *path,
apr_pool_t *pool) {
apr_hash_index_t *hi;
if (apr_hash_get(externals, path, APR_HASH_KEY_STRING))
return TRUE;
for (hi = apr_hash_first(pool, externals); hi; hi = apr_hash_next(hi)) {
const void *key;
apr_hash_this(hi, &key, NULL, NULL);
if (svn_path_is_child(path, key, pool))
return TRUE;
}
return FALSE;
}
static svn_error_t *
send_unversioned_item(const char *name,
svn_node_kind_t path_kind, svn_boolean_t path_special,
svn_wc_adm_access_t *adm_access,
apr_array_header_t *patterns,
apr_hash_t *externals,
svn_boolean_t no_ignore,
apr_hash_t *repos_locks,
const char *repos_root,
svn_wc_status_func2_t status_func,
void *status_baton,
apr_pool_t *pool) {
int ignore_me = svn_wc_match_ignore_list(name, patterns, pool);
const char *path = svn_path_join(svn_wc_adm_access_path(adm_access),
name, pool);
int is_external = is_external_path(externals, path, pool);
svn_wc_status2_t *status;
SVN_ERR(assemble_status(&status, path, adm_access, NULL, NULL,
path_kind, path_special, FALSE, ignore_me,
repos_locks, repos_root, pool));
if (is_external)
status->text_status = svn_wc_status_external;
if (no_ignore || (! ignore_me) || is_external || status->repos_lock)
(status_func)(status_baton, path, status);
return SVN_NO_ERROR;
}
static svn_error_t *get_dir_status(struct edit_baton *eb,
const svn_wc_entry_t *parent_entry,
svn_wc_adm_access_t *adm_access,
const char *entry,
apr_array_header_t *ignores,
svn_depth_t depth,
svn_boolean_t get_all,
svn_boolean_t no_ignore,
svn_boolean_t skip_this_dir,
svn_wc_status_func2_t status_func,
void *status_baton,
svn_cancel_func_t cancel_func,
void *cancel_baton,
apr_pool_t *pool);
static svn_error_t *
handle_dir_entry(struct edit_baton *eb,
svn_wc_adm_access_t *adm_access,
const char *name,
const svn_wc_entry_t *dir_entry,
const svn_wc_entry_t *entry,
svn_node_kind_t kind,
svn_boolean_t special,
apr_array_header_t *ignores,
svn_depth_t depth,
svn_boolean_t get_all,
svn_boolean_t no_ignore,
svn_wc_status_func2_t status_func,
void *status_baton,
svn_cancel_func_t cancel_func,
void *cancel_baton,
apr_pool_t *pool) {
const char *dirname = svn_wc_adm_access_path(adm_access);
const char *path = svn_path_join(dirname, name, pool);
if (kind == svn_node_dir) {
const svn_wc_entry_t *full_entry = entry;
if (entry->kind == kind)
SVN_ERR(svn_wc__entry_versioned(&full_entry, path, adm_access, FALSE,
pool));
if (full_entry != entry
&& (depth == svn_depth_unknown
|| depth == svn_depth_immediates
|| depth == svn_depth_infinity)) {
svn_wc_adm_access_t *dir_access;
SVN_ERR(svn_wc_adm_retrieve(&dir_access, adm_access, path, pool));
SVN_ERR(get_dir_status(eb, dir_entry, dir_access, NULL, ignores,
depth, get_all, no_ignore, FALSE,
status_func, status_baton, cancel_func,
cancel_baton, pool));
} else {
SVN_ERR(send_status_structure(path, adm_access, full_entry,
dir_entry, kind, special, get_all,
FALSE, eb->repos_locks,
eb->repos_root,
status_func, status_baton, pool));
}
} else {
SVN_ERR(send_status_structure(path, adm_access, entry, dir_entry,
kind, special, get_all, FALSE,
eb->repos_locks, eb->repos_root,
status_func, status_baton, pool));
}
return SVN_NO_ERROR;
}
static svn_error_t *
get_dir_status(struct edit_baton *eb,
const svn_wc_entry_t *parent_entry,
svn_wc_adm_access_t *adm_access,
const char *entry,
apr_array_header_t *ignore_patterns,
svn_depth_t depth,
svn_boolean_t get_all,
svn_boolean_t no_ignore,
svn_boolean_t skip_this_dir,
svn_wc_status_func2_t status_func,
void *status_baton,
svn_cancel_func_t cancel_func,
void *cancel_baton,
apr_pool_t *pool) {
apr_hash_t *entries;
apr_hash_index_t *hi;
const svn_wc_entry_t *dir_entry;
const char *path = svn_wc_adm_access_path(adm_access);
apr_hash_t *dirents;
apr_array_header_t *patterns = NULL;
apr_pool_t *iterpool, *subpool = svn_pool_create(pool);
if (cancel_func)
SVN_ERR(cancel_func(cancel_baton));
if (depth == svn_depth_unknown)
depth = svn_depth_infinity;
SVN_ERR(svn_wc_entries_read(&entries, adm_access, FALSE, subpool));
SVN_ERR(svn_io_get_dirents2(&dirents, path, subpool));
SVN_ERR(svn_wc_entry(&dir_entry, path, adm_access, FALSE, subpool));
{
const svn_string_t *prop_val;
SVN_ERR(svn_wc_prop_get(&prop_val, SVN_PROP_EXTERNALS, path,
adm_access, subpool));
if (prop_val) {
apr_array_header_t *ext_items;
int i;
if (eb->traversal_info) {
apr_pool_t *dup_pool = eb->traversal_info->pool;
const char *dup_path = apr_pstrdup(dup_pool, path);
const char *dup_val = apr_pstrmemdup(dup_pool, prop_val->data,
prop_val->len);
apr_hash_set(eb->traversal_info->externals_old,
dup_path, APR_HASH_KEY_STRING, dup_val);
apr_hash_set(eb->traversal_info->externals_new,
dup_path, APR_HASH_KEY_STRING, dup_val);
apr_hash_set(eb->traversal_info->depths,
dup_path, APR_HASH_KEY_STRING,
svn_depth_to_word(dir_entry->depth));
}
SVN_ERR(svn_wc_parse_externals_description3(&ext_items, path,
prop_val->data, FALSE,
pool));
for (i = 0; ext_items && i < ext_items->nelts; i++) {
svn_wc_external_item2_t *item;
item = APR_ARRAY_IDX(ext_items, i, svn_wc_external_item2_t *);
apr_hash_set(eb->externals, svn_path_join(path,
item->target_dir,
pool),
APR_HASH_KEY_STRING, item);
}
}
}
if (entry) {
const svn_wc_entry_t *entry_entry;
svn_io_dirent_t* dirent_p = apr_hash_get(dirents, entry,
APR_HASH_KEY_STRING);
entry_entry = apr_hash_get(entries, entry, APR_HASH_KEY_STRING);
if (entry_entry) {
SVN_ERR(handle_dir_entry(eb, adm_access, entry, dir_entry,
entry_entry,
dirent_p ? dirent_p->kind : svn_node_none,
dirent_p ? dirent_p->special : FALSE,
ignore_patterns, depth, get_all,
no_ignore, status_func, status_baton,
cancel_func, cancel_baton, subpool));
}
else if (dirent_p) {
if (ignore_patterns && ! patterns)
SVN_ERR(collect_ignore_patterns(&patterns, ignore_patterns,
adm_access, subpool));
SVN_ERR(send_unversioned_item(entry, dirent_p->kind,
dirent_p->special, adm_access,
patterns, eb->externals, no_ignore,
eb->repos_locks, eb->repos_root,
status_func, status_baton, subpool));
}
return SVN_NO_ERROR;
}
if (! skip_this_dir)
SVN_ERR(send_status_structure(path, adm_access, dir_entry,
parent_entry, svn_node_dir, FALSE,
get_all, FALSE, eb->repos_locks,
eb->repos_root, status_func, status_baton,
subpool));
if (depth == svn_depth_empty)
return SVN_NO_ERROR;
iterpool = svn_pool_create(subpool);
for (hi = apr_hash_first(subpool, dirents); hi; hi = apr_hash_next(hi)) {
const void *key;
apr_ssize_t klen;
void *val;
svn_io_dirent_t *dirent_p;
svn_pool_clear(iterpool);
apr_hash_this(hi, &key, &klen, &val);
if (apr_hash_get(entries, key, klen)
|| svn_wc_is_adm_dir(key, iterpool))
continue;
dirent_p = val;
if (depth == svn_depth_files && dirent_p->kind == svn_node_dir)
continue;
if (ignore_patterns && ! patterns)
SVN_ERR(collect_ignore_patterns(&patterns, ignore_patterns,
adm_access, subpool));
SVN_ERR(send_unversioned_item(key, dirent_p->kind, dirent_p->special,
adm_access,
patterns, eb->externals, no_ignore,
eb->repos_locks, eb->repos_root,
status_func, status_baton, iterpool));
}
for (hi = apr_hash_first(pool, entries); hi; hi = apr_hash_next(hi)) {
const void *key;
void *val;
svn_io_dirent_t *dirent_p;
apr_hash_this(hi, &key, NULL, &val);
dirent_p = apr_hash_get(dirents, key, APR_HASH_KEY_STRING);
if (strcmp(key, SVN_WC_ENTRY_THIS_DIR) == 0)
continue;
if (depth == svn_depth_files
&& dirent_p && dirent_p->kind == svn_node_dir)
continue;
svn_pool_clear(iterpool);
SVN_ERR(handle_dir_entry(eb, adm_access, key, dir_entry, val,
dirent_p ? dirent_p->kind : svn_node_none,
dirent_p ? dirent_p->special : FALSE,
ignore_patterns,
depth == svn_depth_infinity ? depth
: svn_depth_empty,
get_all, no_ignore,
status_func, status_baton, cancel_func,
cancel_baton, iterpool));
}
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
static void
hash_stash(void *baton,
const char *path,
svn_wc_status2_t *status) {
apr_hash_t *stat_hash = baton;
apr_pool_t *hash_pool = apr_hash_pool_get(stat_hash);
assert(! apr_hash_get(stat_hash, path, APR_HASH_KEY_STRING));
apr_hash_set(stat_hash, apr_pstrdup(hash_pool, path),
APR_HASH_KEY_STRING, svn_wc_dup_status2(status, hash_pool));
}
static svn_error_t *
tweak_statushash(void *baton,
void *this_dir_baton,
svn_boolean_t is_dir_baton,
svn_wc_adm_access_t *adm_access,
const char *path,
svn_boolean_t is_dir,
enum svn_wc_status_kind repos_text_status,
enum svn_wc_status_kind repos_prop_status,
svn_revnum_t deleted_rev,
svn_lock_t *repos_lock) {
svn_wc_status2_t *statstruct;
apr_pool_t *pool;
apr_hash_t *statushash;
if (is_dir_baton)
statushash = ((struct dir_baton *) baton)->statii;
else
statushash = ((struct file_baton *) baton)->dir_baton->statii;
pool = apr_hash_pool_get(statushash);
statstruct = apr_hash_get(statushash, path, APR_HASH_KEY_STRING);
if (! statstruct) {
if (repos_text_status != svn_wc_status_added)
return SVN_NO_ERROR;
SVN_ERR(svn_wc_status2(&statstruct, path, adm_access, pool));
statstruct->repos_lock = repos_lock;
apr_hash_set(statushash, apr_pstrdup(pool, path),
APR_HASH_KEY_STRING, statstruct);
}
if ((repos_text_status == svn_wc_status_added)
&& (statstruct->repos_text_status == svn_wc_status_deleted))
repos_text_status = svn_wc_status_replaced;
if (repos_text_status)
statstruct->repos_text_status = repos_text_status;
if (repos_prop_status)
statstruct->repos_prop_status = repos_prop_status;
if (is_dir_baton) {
struct dir_baton *b = this_dir_baton;
if (b->url) {
if (statstruct->repos_text_status == svn_wc_status_deleted) {
statstruct->url =
svn_path_url_add_component(b->url,
svn_path_basename(path, pool),
pool);
} else
statstruct->url = apr_pstrdup(pool, b->url);
}
if (statstruct->repos_text_status == svn_wc_status_deleted) {
statstruct->ood_kind = is_dir ? svn_node_dir : svn_node_file;
if (deleted_rev == SVN_INVALID_REVNUM)
statstruct->ood_last_cmt_rev =
((struct dir_baton *) baton)->ood_last_cmt_rev;
else
statstruct->ood_last_cmt_rev = deleted_rev;
} else {
statstruct->ood_kind = b->ood_kind;
statstruct->ood_last_cmt_rev = b->ood_last_cmt_rev;
statstruct->ood_last_cmt_date = b->ood_last_cmt_date;
if (b->ood_last_cmt_author)
statstruct->ood_last_cmt_author =
apr_pstrdup(pool, b->ood_last_cmt_author);
}
} else {
struct file_baton *b = baton;
if (b->url)
statstruct->url = apr_pstrdup(pool, b->url);
statstruct->ood_last_cmt_rev = b->ood_last_cmt_rev;
statstruct->ood_last_cmt_date = b->ood_last_cmt_date;
statstruct->ood_kind = b->ood_kind;
if (b->ood_last_cmt_author)
statstruct->ood_last_cmt_author =
apr_pstrdup(pool, b->ood_last_cmt_author);
}
return SVN_NO_ERROR;
}
static const char *
find_dir_url(const struct dir_baton *db, apr_pool_t *pool) {
if (! db->name)
return db->edit_baton->anchor_status->entry->url;
else {
const char *url;
struct dir_baton *pb = db->parent_baton;
svn_wc_status2_t *status = apr_hash_get(pb->statii, db->name,
APR_HASH_KEY_STRING);
if (status && status->entry && status->entry->url)
return status->entry->url;
url = find_dir_url(pb, pool);
if (url)
return svn_path_url_add_component(url, db->name, pool);
else
return NULL;
}
}
static svn_error_t *
make_dir_baton(void **dir_baton,
const char *path,
struct edit_baton *edit_baton,
struct dir_baton *parent_baton,
apr_pool_t *pool) {
struct dir_baton *pb = parent_baton;
struct edit_baton *eb = edit_baton;
struct dir_baton *d = apr_pcalloc(pool, sizeof(*d));
const char *full_path;
svn_wc_status2_t *status_in_parent;
if (pb && (! path))
abort();
if (pb)
full_path = svn_path_join(eb->anchor, path, pool);
else
full_path = apr_pstrdup(pool, eb->anchor);
d->path = full_path;
d->name = path ? (svn_path_basename(path, pool)) : NULL;
d->edit_baton = edit_baton;
d->parent_baton = parent_baton;
d->pool = pool;
d->statii = apr_hash_make(pool);
d->url = apr_pstrdup(pool, find_dir_url(d, pool));
d->ood_last_cmt_rev = SVN_INVALID_REVNUM;
d->ood_last_cmt_date = 0;
d->ood_kind = svn_node_dir;
d->ood_last_cmt_author = NULL;
if (pb) {
if (pb->excluded)
d->excluded = TRUE;
else if (pb->depth == svn_depth_immediates)
d->depth = svn_depth_empty;
else if (pb->depth == svn_depth_files || pb->depth == svn_depth_empty)
d->excluded = TRUE;
else if (pb->depth == svn_depth_unknown)
d->depth = svn_depth_unknown;
else
d->depth = svn_depth_infinity;
} else {
d->depth = eb->default_depth;
}
if (pb)
status_in_parent = apr_hash_get(pb->statii, d->path, APR_HASH_KEY_STRING);
else
status_in_parent = eb->anchor_status;
if (status_in_parent
&& (status_in_parent->text_status != svn_wc_status_unversioned)
&& (status_in_parent->text_status != svn_wc_status_missing)
&& (status_in_parent->text_status != svn_wc_status_obstructed)
&& (status_in_parent->text_status != svn_wc_status_external)
&& (status_in_parent->text_status != svn_wc_status_ignored)
&& (status_in_parent->entry->kind == svn_node_dir)
&& (! d->excluded)
&& (d->depth == svn_depth_unknown
|| d->depth == svn_depth_infinity
|| d->depth == svn_depth_files
|| d->depth == svn_depth_immediates)
) {
svn_wc_adm_access_t *dir_access;
svn_wc_status2_t *this_dir_status;
apr_array_header_t *ignores = eb->ignores;
SVN_ERR(svn_wc_adm_retrieve(&dir_access, eb->adm_access,
d->path, pool));
SVN_ERR(get_dir_status(eb, status_in_parent->entry, dir_access, NULL,
ignores, d->depth == svn_depth_files ?
svn_depth_files : svn_depth_immediates,
TRUE, TRUE, TRUE, hash_stash, d->statii, NULL,
NULL, pool));
this_dir_status = apr_hash_get(d->statii, d->path, APR_HASH_KEY_STRING);
if (this_dir_status && this_dir_status->entry
&& (d->depth == svn_depth_unknown
|| d->depth > status_in_parent->entry->depth)) {
d->depth = this_dir_status->entry->depth;
}
}
*dir_baton = d;
return SVN_NO_ERROR;
}
static struct file_baton *
make_file_baton(struct dir_baton *parent_dir_baton,
const char *path,
apr_pool_t *pool) {
struct dir_baton *pb = parent_dir_baton;
struct edit_baton *eb = pb->edit_baton;
struct file_baton *f = apr_pcalloc(pool, sizeof(*f));
const char *full_path;
full_path = svn_path_join(eb->anchor, path, pool);
f->path = full_path;
f->name = svn_path_basename(path, pool);
f->pool = pool;
f->dir_baton = pb;
f->edit_baton = eb;
f->url = svn_path_url_add_component(find_dir_url(pb, pool),
svn_path_basename(full_path, pool),
pool);
f->ood_last_cmt_rev = SVN_INVALID_REVNUM;
f->ood_last_cmt_date = 0;
f->ood_kind = svn_node_file;
f->ood_last_cmt_author = NULL;
return f;
}
static svn_boolean_t
is_sendable_status(svn_wc_status2_t *status,
struct edit_baton *eb) {
if (status->repos_text_status != svn_wc_status_none)
return TRUE;
if (status->repos_prop_status != svn_wc_status_none)
return TRUE;
if (status->repos_lock)
return TRUE;
if ((status->text_status == svn_wc_status_ignored) && (! eb->no_ignore))
return FALSE;
if (eb->get_all)
return TRUE;
if (status->text_status == svn_wc_status_unversioned)
return TRUE;
if ((status->text_status != svn_wc_status_none)
&& (status->text_status != svn_wc_status_normal))
return TRUE;
if ((status->prop_status != svn_wc_status_none)
&& (status->prop_status != svn_wc_status_normal))
return TRUE;
if (status->locked)
return TRUE;
if (status->switched)
return TRUE;
if (status->entry && status->entry->lock_token)
return TRUE;
if (status->entry && status->entry->changelist)
return TRUE;
return FALSE;
}
struct status_baton {
svn_wc_status_func2_t real_status_func;
void *real_status_baton;
};
static void
mark_deleted(void *baton,
const char *path,
svn_wc_status2_t *status) {
struct status_baton *sb = baton;
status->repos_text_status = svn_wc_status_deleted;
sb->real_status_func(sb->real_status_baton, path, status);
}
static svn_error_t *
handle_statii(struct edit_baton *eb,
svn_wc_entry_t *dir_entry,
const char *dir_path,
apr_hash_t *statii,
svn_boolean_t dir_was_deleted,
svn_depth_t depth,
apr_pool_t *pool) {
apr_array_header_t *ignores = eb->ignores;
apr_hash_index_t *hi;
apr_pool_t *subpool = svn_pool_create(pool);
svn_wc_status_func2_t status_func = eb->status_func;
void *status_baton = eb->status_baton;
struct status_baton sb;
if (dir_was_deleted) {
sb.real_status_func = eb->status_func;
sb.real_status_baton = eb->status_baton;
status_func = mark_deleted;
status_baton = &sb;
}
for (hi = apr_hash_first(pool, statii); hi; hi = apr_hash_next(hi)) {
const void *key;
void *val;
svn_wc_status2_t *status;
apr_hash_this(hi, &key, NULL, &val);
status = val;
svn_pool_clear(subpool);
if (status->text_status != svn_wc_status_obstructed
&& status->text_status != svn_wc_status_missing
&& status->entry && status->entry->kind == svn_node_dir
&& (depth == svn_depth_unknown
|| depth == svn_depth_infinity)) {
svn_wc_adm_access_t *dir_access;
SVN_ERR(svn_wc_adm_retrieve(&dir_access, eb->adm_access,
key, subpool));
SVN_ERR(get_dir_status(eb, dir_entry, dir_access, NULL,
ignores, depth, eb->get_all,
eb->no_ignore, TRUE, status_func,
status_baton, eb->cancel_func,
eb->cancel_baton, subpool));
}
if (dir_was_deleted)
status->repos_text_status = svn_wc_status_deleted;
if (is_sendable_status(status, eb))
(eb->status_func)(eb->status_baton, key, status);
}
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
static svn_error_t *
set_target_revision(void *edit_baton,
svn_revnum_t target_revision,
apr_pool_t *pool) {
struct edit_baton *eb = edit_baton;
*(eb->target_revision) = target_revision;
return SVN_NO_ERROR;
}
static svn_error_t *
open_root(void *edit_baton,
svn_revnum_t base_revision,
apr_pool_t *pool,
void **dir_baton) {
struct edit_baton *eb = edit_baton;
eb->root_opened = TRUE;
return make_dir_baton(dir_baton, NULL, eb, NULL, pool);
}
static svn_error_t *
delete_entry(const char *path,
svn_revnum_t revision,
void *parent_baton,
apr_pool_t *pool) {
struct dir_baton *db = parent_baton;
struct edit_baton *eb = db->edit_baton;
apr_hash_t *entries;
const char *name = svn_path_basename(path, pool);
const char *full_path = svn_path_join(eb->anchor, path, pool);
const char *dir_path;
svn_node_kind_t kind;
svn_wc_adm_access_t *adm_access;
const char *hash_key;
const svn_wc_entry_t *entry;
svn_error_t *err;
SVN_ERR(svn_wc__entry_versioned(&entry, full_path, eb->adm_access,
FALSE, pool));
if (entry->kind == svn_node_dir) {
dir_path = full_path;
hash_key = SVN_WC_ENTRY_THIS_DIR;
} else {
dir_path = svn_path_dirname(full_path, pool);
hash_key = name;
}
err = svn_wc_adm_retrieve(&adm_access, eb->adm_access, dir_path, pool);
if (err) {
SVN_ERR(svn_io_check_path(full_path, &kind, pool));
if ((kind == svn_node_none) && (err->apr_err == SVN_ERR_WC_NOT_LOCKED)) {
svn_error_clear(err);
return SVN_NO_ERROR;
} else
return err;
}
SVN_ERR(svn_wc_entries_read(&entries, adm_access, FALSE, pool));
if (apr_hash_get(entries, hash_key, APR_HASH_KEY_STRING))
SVN_ERR(tweak_statushash(db, db, TRUE, eb->adm_access,
full_path, entry->kind == svn_node_dir,
svn_wc_status_deleted, 0, revision, NULL));
if (db->parent_baton && (! *eb->target))
SVN_ERR(tweak_statushash(db->parent_baton, db, TRUE, eb->adm_access,
db->path, entry->kind == svn_node_dir,
svn_wc_status_modified, 0, SVN_INVALID_REVNUM,
NULL));
return SVN_NO_ERROR;
}
static svn_error_t *
add_directory(const char *path,
void *parent_baton,
const char *copyfrom_path,
svn_revnum_t copyfrom_revision,
apr_pool_t *pool,
void **child_baton) {
struct dir_baton *pb = parent_baton;
struct edit_baton *eb = pb->edit_baton;
struct dir_baton *new_db;
SVN_ERR(make_dir_baton(child_baton, path, eb, pb, pool));
new_db = *child_baton;
new_db->added = TRUE;
pb->text_changed = TRUE;
return SVN_NO_ERROR;
}
static svn_error_t *
open_directory(const char *path,
void *parent_baton,
svn_revnum_t base_revision,
apr_pool_t *pool,
void **child_baton) {
struct dir_baton *pb = parent_baton;
return make_dir_baton(child_baton, path, pb->edit_baton, pb, pool);
}
static svn_error_t *
change_dir_prop(void *dir_baton,
const char *name,
const svn_string_t *value,
apr_pool_t *pool) {
struct dir_baton *db = dir_baton;
if (svn_wc_is_normal_prop(name))
db->prop_changed = TRUE;
if (value != NULL) {
if (strcmp(name, SVN_PROP_ENTRY_COMMITTED_REV) == 0)
db->ood_last_cmt_rev = SVN_STR_TO_REV(value->data);
else if (strcmp(name, SVN_PROP_ENTRY_LAST_AUTHOR) == 0)
db->ood_last_cmt_author = apr_pstrdup(db->pool, value->data);
else if (strcmp(name, SVN_PROP_ENTRY_COMMITTED_DATE) == 0) {
apr_time_t tm;
SVN_ERR(svn_time_from_cstring(&tm, value->data, db->pool));
db->ood_last_cmt_date = tm;
}
}
return SVN_NO_ERROR;
}
static svn_error_t *
close_directory(void *dir_baton,
apr_pool_t *pool) {
struct dir_baton *db = dir_baton;
struct dir_baton *pb = db->parent_baton;
struct edit_baton *eb = db->edit_baton;
svn_wc_status2_t *dir_status = NULL;
if (db->added || db->prop_changed || db->text_changed
|| db->ood_last_cmt_rev != SVN_INVALID_REVNUM) {
enum svn_wc_status_kind repos_text_status;
enum svn_wc_status_kind repos_prop_status;
if (db->added) {
repos_text_status = svn_wc_status_added;
repos_prop_status = db->prop_changed ? svn_wc_status_added
: svn_wc_status_none;
} else {
repos_text_status = db->text_changed ? svn_wc_status_modified
: svn_wc_status_none;
repos_prop_status = db->prop_changed ? svn_wc_status_modified
: svn_wc_status_none;
}
if (pb) {
SVN_ERR(tweak_statushash(pb, db, TRUE,
eb->adm_access,
db->path, TRUE,
repos_text_status,
repos_prop_status, SVN_INVALID_REVNUM,
NULL));
} else {
eb->anchor_status->repos_prop_status = repos_prop_status;
eb->anchor_status->repos_text_status = repos_text_status;
if (db->ood_last_cmt_rev != eb->anchor_status->entry->revision) {
eb->anchor_status->ood_last_cmt_rev = db->ood_last_cmt_rev;
eb->anchor_status->ood_last_cmt_date = db->ood_last_cmt_date;
eb->anchor_status->ood_kind = db->ood_kind;
eb->anchor_status->ood_last_cmt_author =
apr_pstrdup(pool, db->ood_last_cmt_author);
}
}
}
if (pb && ! db->excluded) {
svn_boolean_t was_deleted = FALSE;
dir_status = apr_hash_get(pb->statii, db->path, APR_HASH_KEY_STRING);
if (dir_status &&
((dir_status->repos_text_status == svn_wc_status_deleted)
|| (dir_status->repos_text_status == svn_wc_status_replaced)))
was_deleted = TRUE;
SVN_ERR(handle_statii(eb, dir_status ? dir_status->entry : NULL,
db->path, db->statii, was_deleted, db->depth,
pool));
if (dir_status && is_sendable_status(dir_status, eb))
(eb->status_func)(eb->status_baton, db->path, dir_status);
apr_hash_set(pb->statii, db->path, APR_HASH_KEY_STRING, NULL);
} else if (! pb) {
if (*eb->target) {
svn_wc_status2_t *tgt_status;
const char *path = svn_path_join(eb->anchor, eb->target, pool);
dir_status = eb->anchor_status;
tgt_status = apr_hash_get(db->statii, path, APR_HASH_KEY_STRING);
if (tgt_status) {
if (tgt_status->entry
&& tgt_status->entry->kind == svn_node_dir) {
svn_wc_adm_access_t *dir_access;
SVN_ERR(svn_wc_adm_retrieve(&dir_access, eb->adm_access,
path, pool));
SVN_ERR(get_dir_status
(eb, tgt_status->entry, dir_access, NULL,
eb->ignores, eb->default_depth, eb->get_all,
eb->no_ignore, TRUE,
eb->status_func, eb->status_baton,
eb->cancel_func, eb->cancel_baton, pool));
}
if (is_sendable_status(tgt_status, eb))
(eb->status_func)(eb->status_baton, path, tgt_status);
}
} else {
SVN_ERR(handle_statii(eb, eb->anchor_status->entry, db->path,
db->statii, FALSE, eb->default_depth, pool));
if (is_sendable_status(eb->anchor_status, eb))
(eb->status_func)(eb->status_baton, db->path, eb->anchor_status);
eb->anchor_status = NULL;
}
}
return SVN_NO_ERROR;
}
static svn_error_t *
add_file(const char *path,
void *parent_baton,
const char *copyfrom_path,
svn_revnum_t copyfrom_revision,
apr_pool_t *pool,
void **file_baton) {
struct dir_baton *pb = parent_baton;
struct file_baton *new_fb = make_file_baton(pb, path, pool);
pb->text_changed = TRUE;
new_fb->added = TRUE;
*file_baton = new_fb;
return SVN_NO_ERROR;
}
static svn_error_t *
open_file(const char *path,
void *parent_baton,
svn_revnum_t base_revision,
apr_pool_t *pool,
void **file_baton) {
struct dir_baton *pb = parent_baton;
struct file_baton *new_fb = make_file_baton(pb, path, pool);
*file_baton = new_fb;
return SVN_NO_ERROR;
}
static svn_error_t *
apply_textdelta(void *file_baton,
const char *base_checksum,
apr_pool_t *pool,
svn_txdelta_window_handler_t *handler,
void **handler_baton) {
struct file_baton *fb = file_baton;
fb->text_changed = TRUE;
*handler_baton = NULL;
*handler = svn_delta_noop_window_handler;
return SVN_NO_ERROR;
}
static svn_error_t *
change_file_prop(void *file_baton,
const char *name,
const svn_string_t *value,
apr_pool_t *pool) {
struct file_baton *fb = file_baton;
if (svn_wc_is_normal_prop(name))
fb->prop_changed = TRUE;
if (value != NULL) {
if (strcmp(name, SVN_PROP_ENTRY_COMMITTED_REV) == 0)
fb->ood_last_cmt_rev = SVN_STR_TO_REV(value->data);
else if (strcmp(name, SVN_PROP_ENTRY_LAST_AUTHOR) == 0)
fb->ood_last_cmt_author = apr_pstrdup(fb->dir_baton->pool,
value->data);
else if (strcmp(name, SVN_PROP_ENTRY_COMMITTED_DATE) == 0) {
apr_time_t tm;
SVN_ERR(svn_time_from_cstring(&tm, value->data,
fb->dir_baton->pool));
fb->ood_last_cmt_date = tm;
}
}
return SVN_NO_ERROR;
}
static svn_error_t *
close_file(void *file_baton,
const char *text_checksum,
apr_pool_t *pool) {
struct file_baton *fb = file_baton;
enum svn_wc_status_kind repos_text_status;
enum svn_wc_status_kind repos_prop_status;
svn_lock_t *repos_lock = NULL;
if (! (fb->added || fb->prop_changed || fb->text_changed))
return SVN_NO_ERROR;
if (fb->added) {
const char *url;
repos_text_status = svn_wc_status_added;
repos_prop_status = fb->prop_changed ? svn_wc_status_added : 0;
if (fb->edit_baton->repos_locks) {
url = find_dir_url(fb->dir_baton, pool);
if (url) {
url = svn_path_url_add_component(url, fb->name, pool);
repos_lock = apr_hash_get
(fb->edit_baton->repos_locks,
svn_path_uri_decode(url +
strlen(fb->edit_baton->repos_root),
pool), APR_HASH_KEY_STRING);
}
}
} else {
repos_text_status = fb->text_changed ? svn_wc_status_modified : 0;
repos_prop_status = fb->prop_changed ? svn_wc_status_modified : 0;
}
SVN_ERR(tweak_statushash(fb, NULL, FALSE,
fb->edit_baton->adm_access,
fb->path, FALSE,
repos_text_status,
repos_prop_status, SVN_INVALID_REVNUM,
repos_lock));
return SVN_NO_ERROR;
}
static svn_error_t *
close_edit(void *edit_baton,
apr_pool_t *pool) {
struct edit_baton *eb = edit_baton;
apr_array_header_t *ignores = eb->ignores;
svn_error_t *err = NULL;
if (eb->root_opened)
goto cleanup;
if (*eb->target) {
svn_node_kind_t kind;
const char *full_path = svn_path_join(eb->anchor, eb->target, pool);
err = svn_io_check_path(full_path, &kind, pool);
if (err) goto cleanup;
if (kind == svn_node_dir) {
svn_wc_adm_access_t *tgt_access;
const svn_wc_entry_t *tgt_entry;
err = svn_wc_entry(&tgt_entry, full_path, eb->adm_access,
FALSE, pool);
if (err) goto cleanup;
if (! tgt_entry) {
err = get_dir_status(eb, NULL, eb->adm_access, eb->target,
ignores, svn_depth_empty, eb->get_all,
TRUE, TRUE,
eb->status_func, eb->status_baton,
eb->cancel_func, eb->cancel_baton,
pool);
if (err) goto cleanup;
} else {
err = svn_wc_adm_retrieve(&tgt_access, eb->adm_access,
full_path, pool);
if (err) goto cleanup;
err = get_dir_status(eb, NULL, tgt_access, NULL, ignores,
eb->default_depth, eb->get_all,
eb->no_ignore, FALSE,
eb->status_func, eb->status_baton,
eb->cancel_func, eb->cancel_baton,
pool);
if (err) goto cleanup;
}
} else {
err = get_dir_status(eb, NULL, eb->adm_access, eb->target,
ignores, svn_depth_empty, eb->get_all,
TRUE, TRUE, eb->status_func, eb->status_baton,
eb->cancel_func, eb->cancel_baton, pool);
if (err) goto cleanup;
}
} else {
err = get_dir_status(eb, NULL, eb->adm_access, NULL, ignores,
eb->default_depth, eb->get_all, eb->no_ignore,
FALSE, eb->status_func, eb->status_baton,
eb->cancel_func, eb->cancel_baton, pool);
if (err) goto cleanup;
}
cleanup:
if (eb->traversal_info && *eb->target) {
apr_hash_set(eb->traversal_info->externals_old,
eb->anchor, APR_HASH_KEY_STRING, NULL);
apr_hash_set(eb->traversal_info->externals_new,
eb->anchor, APR_HASH_KEY_STRING, NULL);
apr_hash_set(eb->traversal_info->depths,
eb->anchor, APR_HASH_KEY_STRING, NULL);
}
return err;
}
svn_error_t *
svn_wc_get_status_editor3(const svn_delta_editor_t **editor,
void **edit_baton,
void **set_locks_baton,
svn_revnum_t *edit_revision,
svn_wc_adm_access_t *anchor,
const char *target,
svn_depth_t depth,
svn_boolean_t get_all,
svn_boolean_t no_ignore,
apr_array_header_t *ignore_patterns,
svn_wc_status_func2_t status_func,
void *status_baton,
svn_cancel_func_t cancel_func,
void *cancel_baton,
svn_wc_traversal_info_t *traversal_info,
apr_pool_t *pool) {
struct edit_baton *eb;
svn_delta_editor_t *tree_editor = svn_delta_default_editor(pool);
eb = apr_palloc(pool, sizeof(*eb));
eb->default_depth = depth;
eb->target_revision = edit_revision;
eb->adm_access = anchor;
eb->get_all = get_all;
eb->no_ignore = no_ignore;
eb->status_func = status_func;
eb->status_baton = status_baton;
eb->cancel_func = cancel_func;
eb->cancel_baton = cancel_baton;
eb->traversal_info = traversal_info;
eb->externals = apr_hash_make(pool);
eb->anchor = svn_wc_adm_access_path(anchor);
eb->target = target;
eb->root_opened = FALSE;
eb->repos_locks = NULL;
eb->repos_root = NULL;
if (ignore_patterns) {
eb->ignores = ignore_patterns;
} else {
eb->ignores = apr_array_make(pool, 16, sizeof(const char *));
svn_cstring_split_append(eb->ignores, SVN_CONFIG_DEFAULT_GLOBAL_IGNORES,
"\n\r\t\v ", FALSE, pool);
}
SVN_ERR(svn_wc_status2(&(eb->anchor_status), eb->anchor, anchor, pool));
tree_editor->set_target_revision = set_target_revision;
tree_editor->open_root = open_root;
tree_editor->delete_entry = delete_entry;
tree_editor->add_directory = add_directory;
tree_editor->open_directory = open_directory;
tree_editor->change_dir_prop = change_dir_prop;
tree_editor->close_directory = close_directory;
tree_editor->add_file = add_file;
tree_editor->open_file = open_file;
tree_editor->apply_textdelta = apply_textdelta;
tree_editor->change_file_prop = change_file_prop;
tree_editor->close_file = close_file;
tree_editor->close_edit = close_edit;
SVN_ERR(svn_delta_get_cancellation_editor(cancel_func, cancel_baton,
tree_editor, eb, editor,
edit_baton, pool));
if (set_locks_baton)
*set_locks_baton = eb;
return SVN_NO_ERROR;
}
svn_error_t *
svn_wc_get_status_editor2(const svn_delta_editor_t **editor,
void **edit_baton,
void **set_locks_baton,
svn_revnum_t *edit_revision,
svn_wc_adm_access_t *anchor,
const char *target,
apr_hash_t *config,
svn_boolean_t recurse,
svn_boolean_t get_all,
svn_boolean_t no_ignore,
svn_wc_status_func2_t status_func,
void *status_baton,
svn_cancel_func_t cancel_func,
void *cancel_baton,
svn_wc_traversal_info_t *traversal_info,
apr_pool_t *pool) {
apr_array_header_t *ignores;
SVN_ERR(svn_wc_get_default_ignores(&ignores, config, pool));
return svn_wc_get_status_editor3(editor,
edit_baton,
set_locks_baton,
edit_revision,
anchor,
target,
SVN_DEPTH_INFINITY_OR_IMMEDIATES(recurse),
get_all,
no_ignore,
ignores,
status_func,
status_baton,
cancel_func,
cancel_baton,
traversal_info,
pool);
}
struct old_status_func_cb_baton {
svn_wc_status_func_t original_func;
void *original_baton;
};
static void old_status_func_cb(void *baton,
const char *path,
svn_wc_status2_t *status) {
struct old_status_func_cb_baton *b = baton;
svn_wc_status_t *stat = (svn_wc_status_t *) status;
b->original_func(b->original_baton, path, stat);
}
svn_error_t *
svn_wc_get_status_editor(const svn_delta_editor_t **editor,
void **edit_baton,
svn_revnum_t *edit_revision,
svn_wc_adm_access_t *anchor,
const char *target,
apr_hash_t *config,
svn_boolean_t recurse,
svn_boolean_t get_all,
svn_boolean_t no_ignore,
svn_wc_status_func_t status_func,
void *status_baton,
svn_cancel_func_t cancel_func,
void *cancel_baton,
svn_wc_traversal_info_t *traversal_info,
apr_pool_t *pool) {
struct old_status_func_cb_baton *b = apr_pcalloc(pool, sizeof(*b));
apr_array_header_t *ignores;
b->original_func = status_func;
b->original_baton = status_baton;
SVN_ERR(svn_wc_get_default_ignores(&ignores, config, pool));
return svn_wc_get_status_editor3(editor, edit_baton, NULL, edit_revision,
anchor, target,
SVN_DEPTH_INFINITY_OR_IMMEDIATES(recurse),
get_all, no_ignore, ignores,
old_status_func_cb, b,
cancel_func, cancel_baton,
traversal_info, pool);
}
svn_error_t *
svn_wc_status_set_repos_locks(void *edit_baton,
apr_hash_t *locks,
const char *repos_root,
apr_pool_t *pool) {
struct edit_baton *eb = edit_baton;
eb->repos_locks = locks;
eb->repos_root = apr_pstrdup(pool, repos_root);
return SVN_NO_ERROR;
}
svn_error_t *
svn_wc_get_default_ignores(apr_array_header_t **patterns,
apr_hash_t *config,
apr_pool_t *pool) {
svn_config_t *cfg = config ? apr_hash_get(config,
SVN_CONFIG_CATEGORY_CONFIG,
APR_HASH_KEY_STRING) : NULL;
const char *val;
svn_config_get(cfg, &val, SVN_CONFIG_SECTION_MISCELLANY,
SVN_CONFIG_OPTION_GLOBAL_IGNORES,
SVN_CONFIG_DEFAULT_GLOBAL_IGNORES);
*patterns = apr_array_make(pool, 16, sizeof(const char *));
svn_cstring_split_append(*patterns, val, "\n\r\t\v ", FALSE, pool);
return SVN_NO_ERROR;
}
svn_error_t *
svn_wc_status2(svn_wc_status2_t **status,
const char *path,
svn_wc_adm_access_t *adm_access,
apr_pool_t *pool) {
const svn_wc_entry_t *entry = NULL;
const svn_wc_entry_t *parent_entry = NULL;
if (adm_access)
SVN_ERR(svn_wc_entry(&entry, path, adm_access, FALSE, pool));
if (entry && ! svn_path_is_empty(path)) {
const char *parent_path = svn_path_dirname(path, pool);
svn_wc_adm_access_t *parent_access;
SVN_ERR(svn_wc__adm_retrieve_internal(&parent_access, adm_access,
parent_path, pool));
if (parent_access)
SVN_ERR(svn_wc_entry(&parent_entry, parent_path, parent_access,
FALSE, pool));
}
SVN_ERR(assemble_status(status, path, adm_access, entry, parent_entry,
svn_node_unknown, FALSE,
TRUE, FALSE, NULL, NULL, pool));
return SVN_NO_ERROR;
}
svn_error_t *
svn_wc_status(svn_wc_status_t **status,
const char *path,
svn_wc_adm_access_t *adm_access,
apr_pool_t *pool) {
svn_wc_status2_t *stat2;
SVN_ERR(svn_wc_status2(&stat2, path, adm_access, pool));
*status = (svn_wc_status_t *) stat2;
return SVN_NO_ERROR;
}
svn_wc_status2_t *
svn_wc_dup_status2(svn_wc_status2_t *orig_stat,
apr_pool_t *pool) {
svn_wc_status2_t *new_stat = apr_palloc(pool, sizeof(*new_stat));
*new_stat = *orig_stat;
if (orig_stat->entry)
new_stat->entry = svn_wc_entry_dup(orig_stat->entry, pool);
if (orig_stat->repos_lock)
new_stat->repos_lock = svn_lock_dup(orig_stat->repos_lock, pool);
if (orig_stat->url)
new_stat->url = apr_pstrdup(pool, orig_stat->url);
if (orig_stat->ood_last_cmt_author)
new_stat->ood_last_cmt_author
= apr_pstrdup(pool, orig_stat->ood_last_cmt_author);
return new_stat;
}
svn_wc_status_t *
svn_wc_dup_status(svn_wc_status_t *orig_stat,
apr_pool_t *pool) {
svn_wc_status_t *new_stat = apr_palloc(pool, sizeof(*new_stat));
*new_stat = *orig_stat;
if (orig_stat->entry)
new_stat->entry = svn_wc_entry_dup(orig_stat->entry, pool);
return new_stat;
}
svn_error_t *
svn_wc_get_ignores(apr_array_header_t **patterns,
apr_hash_t *config,
svn_wc_adm_access_t *adm_access,
apr_pool_t *pool) {
apr_array_header_t *default_ignores;
SVN_ERR(svn_wc_get_default_ignores(&default_ignores, config, pool));
SVN_ERR(collect_ignore_patterns(patterns, default_ignores, adm_access,
pool));
return SVN_NO_ERROR;
}
