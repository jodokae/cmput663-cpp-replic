#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <apr_pools.h>
#include <apr_hash.h>
#include <apr_md5.h>
#include <apr_tables.h>
#include <apr_file_io.h>
#include <apr_strings.h>
#include "svn_types.h"
#include "svn_pools.h"
#include "svn_delta.h"
#include "svn_string.h"
#include "svn_path.h"
#include "svn_xml.h"
#include "svn_error.h"
#include "svn_io.h"
#include "svn_md5.h"
#include "svn_private_config.h"
#include "svn_time.h"
#include "svn_config.h"
#include "wc.h"
#include "questions.h"
#include "log.h"
#include "adm_files.h"
#include "adm_ops.h"
#include "entries.h"
#include "lock.h"
#include "props.h"
#include "translate.h"
#include "private/svn_wc_private.h"
static svn_error_t *add_file_with_history(const char *path,
void *parent_baton,
const char *copyfrom_path,
svn_revnum_t copyfrom_rev,
void **file_baton,
apr_pool_t *pool);
struct edit_baton {
const char *anchor;
const char *target;
svn_wc_adm_access_t *adm_access;
apr_array_header_t *ext_patterns;
svn_revnum_t *target_revision;
svn_depth_t requested_depth;
svn_boolean_t depth_is_sticky;
svn_boolean_t use_commit_times;
svn_boolean_t root_opened;
svn_boolean_t target_deleted;
svn_boolean_t allow_unver_obstructions;
const char *switch_url;
const char *repos;
const char *diff3_cmd;
svn_wc_traversal_info_t *traversal_info;
svn_wc_notify_func2_t notify_func;
void *notify_baton;
svn_cancel_func_t cancel_func;
void *cancel_baton;
svn_wc_conflict_resolver_func_t conflict_func;
void *conflict_baton;
svn_wc_get_file_t fetch_func;
void *fetch_baton;
apr_hash_t *skipped_paths;
apr_pool_t *pool;
};
struct dir_baton {
const char *path;
const char *name;
const char *new_URL;
struct edit_baton *edit_baton;
struct dir_baton *parent_baton;
svn_boolean_t added;
svn_boolean_t existed;
svn_boolean_t add_existed;
apr_array_header_t *propchanges;
struct bump_dir_info *bump_info;
int log_number;
svn_stringbuf_t *log_accum;
svn_depth_t ambient_depth;
apr_pool_t *pool;
};
struct bump_dir_info {
struct bump_dir_info *parent;
int ref_count;
const char *path;
svn_boolean_t skipped;
};
struct handler_baton {
apr_file_t *source;
apr_file_t *dest;
svn_txdelta_window_handler_t apply_handler;
void *apply_baton;
apr_pool_t *pool;
struct file_baton *fb;
};
static const char *
get_entry_url(svn_wc_adm_access_t *associated_access,
const char *dir,
const char *name,
apr_pool_t *pool) {
svn_error_t *err;
const svn_wc_entry_t *entry;
svn_wc_adm_access_t *adm_access;
err = svn_wc_adm_retrieve(&adm_access, associated_access, dir, pool);
if (! err) {
err = svn_wc_entry(&entry, svn_path_join_many(pool, dir, name, NULL),
adm_access, FALSE, pool);
}
if (err || (! entry) || (! entry->url)) {
svn_error_clear(err);
return NULL;
}
return entry->url;
}
static svn_error_t *
flush_log(struct dir_baton *db, apr_pool_t *pool) {
if (! svn_stringbuf_isempty(db->log_accum)) {
svn_wc_adm_access_t *adm_access;
SVN_ERR(svn_wc_adm_retrieve(&adm_access, db->edit_baton->adm_access,
db->path, pool));
SVN_ERR(svn_wc__write_log(adm_access, db->log_number, db->log_accum,
pool));
db->log_number++;
svn_stringbuf_setempty(db->log_accum);
}
return SVN_NO_ERROR;
}
static apr_status_t
cleanup_dir_baton(void *dir_baton) {
struct dir_baton *db = dir_baton;
svn_error_t *err;
apr_status_t apr_err;
svn_wc_adm_access_t *adm_access;
apr_pool_t *pool = apr_pool_parent_get(db->pool);
err = flush_log(db, pool);
if (! err && db->log_number > 0) {
err = svn_wc_adm_retrieve(&adm_access, db->edit_baton->adm_access,
db->path, pool);
if (! err) {
err = svn_wc__run_log(adm_access, NULL, pool);
if (! err)
return APR_SUCCESS;
}
}
if (err)
apr_err = err->apr_err;
else
apr_err = APR_SUCCESS;
svn_error_clear(err);
return apr_err;
}
static apr_status_t
cleanup_dir_baton_child(void *dir_baton) {
struct dir_baton *db = dir_baton;
apr_pool_cleanup_kill(db->pool, db, cleanup_dir_baton);
return APR_SUCCESS;
}
static svn_error_t *
make_dir_baton(struct dir_baton **d_p,
const char *path,
struct edit_baton *eb,
struct dir_baton *pb,
svn_boolean_t added,
apr_pool_t *pool) {
struct dir_baton *d;
struct bump_dir_info *bdi;
if (pb && (! path))
abort();
d = apr_pcalloc(pool, sizeof(*d));
d->path = apr_pstrdup(pool, eb->anchor);
if (path) {
d->path = svn_path_join(d->path, path, pool);
d->name = svn_path_basename(path, pool);
} else {
d->name = NULL;
}
if (eb->switch_url) {
if (! pb) {
if (! *eb->target)
d->new_URL = apr_pstrdup(pool, eb->switch_url);
else
d->new_URL = svn_path_dirname(eb->switch_url, pool);
}
else {
if (*eb->target && (! pb->parent_baton))
d->new_URL = apr_pstrdup(pool, eb->switch_url);
else
d->new_URL = svn_path_url_add_component(pb->new_URL,
d->name, pool);
}
} else {
d->new_URL = get_entry_url(eb->adm_access, d->path, NULL, pool);
if ((! d->new_URL) && pb)
d->new_URL = svn_path_url_add_component(pb->new_URL, d->name, pool);
}
bdi = apr_palloc(eb->pool, sizeof(*bdi));
bdi->parent = pb ? pb->bump_info : NULL;
bdi->ref_count = 1;
bdi->path = apr_pstrdup(eb->pool, d->path);
bdi->skipped = FALSE;
if (pb)
++bdi->parent->ref_count;
d->edit_baton = eb;
d->parent_baton = pb;
d->pool = svn_pool_create(pool);
d->propchanges = apr_array_make(pool, 1, sizeof(svn_prop_t));
d->added = added;
d->existed = FALSE;
d->add_existed = FALSE;
d->bump_info = bdi;
d->log_number = 0;
d->log_accum = svn_stringbuf_create("", pool);
d->ambient_depth = svn_depth_unknown;
apr_pool_cleanup_register(d->pool, d, cleanup_dir_baton,
cleanup_dir_baton_child);
*d_p = d;
return SVN_NO_ERROR;
}
static svn_error_t *
complete_directory(struct edit_baton *eb,
const char *path,
svn_boolean_t is_root_dir,
apr_pool_t *pool) {
svn_wc_adm_access_t *adm_access;
apr_hash_t *entries;
svn_wc_entry_t *entry;
apr_hash_index_t *hi;
apr_pool_t *subpool;
svn_wc_entry_t *current_entry;
const char *name;
if (is_root_dir && *eb->target)
return SVN_NO_ERROR;
SVN_ERR(svn_wc_adm_retrieve(&adm_access, eb->adm_access, path, pool));
SVN_ERR(svn_wc_entries_read(&entries, adm_access, TRUE, pool));
entry = apr_hash_get(entries, SVN_WC_ENTRY_THIS_DIR, APR_HASH_KEY_STRING);
if (! entry)
return svn_error_createf(SVN_ERR_ENTRY_NOT_FOUND, NULL,
_("No '.' entry in: '%s'"),
svn_path_local_style(path, pool));
entry->incomplete = FALSE;
if (eb->depth_is_sticky &&
(eb->requested_depth == svn_depth_infinity
|| (strcmp(path, svn_path_join(eb->anchor, eb->target, pool)) == 0
&& eb->requested_depth > entry->depth)))
entry->depth = eb->requested_depth;
subpool = svn_pool_create(pool);
for (hi = apr_hash_first(pool, entries); hi; hi = apr_hash_next(hi)) {
const void *key;
void *val;
svn_pool_clear(subpool);
apr_hash_this(hi, &key, NULL, &val);
name = key;
current_entry = val;
if (current_entry->deleted) {
if (current_entry->schedule != svn_wc_schedule_add)
svn_wc__entry_remove(entries, name);
else {
svn_wc_entry_t tmpentry;
tmpentry.deleted = FALSE;
SVN_ERR(svn_wc__entry_modify(adm_access, current_entry->name,
&tmpentry,
SVN_WC__ENTRY_MODIFY_DELETED,
FALSE, subpool));
}
}
else if (current_entry->absent
&& (current_entry->revision != *(eb->target_revision))) {
svn_wc__entry_remove(entries, name);
} else if (current_entry->kind == svn_node_dir) {
const char *child_path = svn_path_join(path, name, subpool);
if ((svn_wc__adm_missing(adm_access, child_path))
&& (! current_entry->absent)
&& (current_entry->schedule != svn_wc_schedule_add)) {
svn_wc__entry_remove(entries, name);
if (eb->notify_func) {
svn_wc_notify_t *notify
= svn_wc_create_notify(child_path,
svn_wc_notify_update_delete,
subpool);
notify->kind = current_entry->kind;
(* eb->notify_func)(eb->notify_baton, notify, subpool);
}
}
}
}
svn_pool_destroy(subpool);
SVN_ERR(svn_wc__entries_write(entries, adm_access, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
maybe_bump_dir_info(struct edit_baton *eb,
struct bump_dir_info *bdi,
apr_pool_t *pool) {
for ( ; bdi != NULL; bdi = bdi->parent) {
if (--bdi->ref_count > 0)
return SVN_NO_ERROR;
if (! bdi->skipped)
SVN_ERR(complete_directory(eb, bdi->path,
bdi->parent ? FALSE : TRUE, pool));
}
return SVN_NO_ERROR;
}
struct file_baton {
struct edit_baton *edit_baton;
struct dir_baton *dir_baton;
apr_pool_t *pool;
const char *name;
const char *path;
const char *new_URL;
svn_boolean_t added;
svn_boolean_t added_with_history;
svn_boolean_t skipped;
svn_boolean_t existed;
svn_boolean_t add_existed;
const char *text_base_path;
const char *new_text_base_path;
const char *copied_text_base;
const char *copied_working_text;
apr_hash_t *copied_base_props;
apr_hash_t *copied_working_props;
svn_boolean_t received_textdelta;
apr_array_header_t *propchanges;
const char *last_changed_date;
struct bump_dir_info *bump_info;
unsigned char digest[APR_MD5_DIGESTSIZE];
};
static svn_error_t *
make_file_baton(struct file_baton **f_p,
struct dir_baton *pb,
const char *path,
svn_boolean_t adding,
apr_pool_t *pool) {
struct file_baton *f = apr_pcalloc(pool, sizeof(*f));
if (! path)
abort();
f->path = svn_path_join(pb->edit_baton->anchor, path, pool);
f->name = svn_path_basename(path, pool);
if (pb->edit_baton->switch_url) {
f->new_URL = svn_path_url_add_component(pb->new_URL, f->name, pool);
} else {
f->new_URL = get_entry_url(pb->edit_baton->adm_access,
pb->path, f->name, pool);
}
f->pool = pool;
f->edit_baton = pb->edit_baton;
f->propchanges = apr_array_make(pool, 1, sizeof(svn_prop_t));
f->bump_info = pb->bump_info;
f->added = adding;
f->existed = FALSE;
f->add_existed = FALSE;
f->dir_baton = pb;
++f->bump_info->ref_count;
*f_p = f;
return SVN_NO_ERROR;
}
static svn_error_t *
window_handler(svn_txdelta_window_t *window, void *baton) {
struct handler_baton *hb = baton;
struct file_baton *fb = hb->fb;
svn_error_t *err, *err2;
err = hb->apply_handler(window, hb->apply_baton);
if (window != NULL && !err)
return err;
if (hb->source) {
if (fb->copied_text_base)
err2 = svn_io_file_close(hb->source, hb->pool);
else
err2 = svn_wc__close_text_base(hb->source, fb->path, 0, hb->pool);
if (err2 && !err)
err = err2;
else
svn_error_clear(err2);
}
err2 = svn_wc__close_text_base(hb->dest, fb->path, 0, hb->pool);
if (err2) {
if (!err)
err = err2;
else
svn_error_clear(err2);
}
if (err) {
svn_error_clear(svn_io_remove_file(fb->new_text_base_path, hb->pool));
fb->new_text_base_path = NULL;
}
svn_pool_destroy(hb->pool);
return err;
}
static svn_error_t *
prep_directory(struct dir_baton *db,
const char *ancestor_url,
svn_revnum_t ancestor_revision,
apr_pool_t *pool) {
const char *repos;
SVN_ERR(svn_wc__ensure_directory(db->path, pool));
if (db->edit_baton->repos
&& svn_path_is_ancestor(db->edit_baton->repos, ancestor_url))
repos = db->edit_baton->repos;
else
repos = NULL;
SVN_ERR(svn_wc_ensure_adm3(db->path, NULL,
ancestor_url, repos,
ancestor_revision, db->ambient_depth, pool));
if (! db->edit_baton->adm_access
|| strcmp(svn_wc_adm_access_path(db->edit_baton->adm_access),
db->path)) {
svn_wc_adm_access_t *adm_access;
apr_pool_t *adm_access_pool
= db->edit_baton->adm_access
? svn_wc_adm_access_pool(db->edit_baton->adm_access)
: db->edit_baton->pool;
svn_error_t *err = svn_wc_adm_open3(&adm_access,
db->edit_baton->adm_access,
db->path, TRUE, 0, NULL, NULL,
adm_access_pool);
if (err && err->apr_err == SVN_ERR_WC_LOCKED) {
svn_error_clear(err);
err = svn_wc_adm_retrieve(&adm_access,
db->edit_baton->adm_access,
db->path, adm_access_pool);
}
SVN_ERR(err);
if (!db->edit_baton->adm_access)
db->edit_baton->adm_access = adm_access;
}
return SVN_NO_ERROR;
}
static svn_error_t *
accumulate_entry_props(svn_stringbuf_t *log_accum,
svn_wc_notify_lock_state_t *lock_state,
svn_wc_adm_access_t *adm_access,
const char *path,
apr_array_header_t *entry_props,
apr_pool_t *pool) {
int i;
svn_wc_entry_t tmp_entry;
apr_uint64_t flags = 0;
if (lock_state)
*lock_state = svn_wc_notify_lock_state_unchanged;
for (i = 0; i < entry_props->nelts; ++i) {
const svn_prop_t *prop = &APR_ARRAY_IDX(entry_props, i, svn_prop_t);
const char *val;
if (! strcmp(prop->name, SVN_PROP_ENTRY_LOCK_TOKEN)) {
SVN_ERR(svn_wc__loggy_delete_lock
(&log_accum, adm_access, path, pool));
if (lock_state)
*lock_state = svn_wc_notify_lock_state_unlocked;
continue;
}
if (! prop->value)
continue;
val = prop->value->data;
if (! strcmp(prop->name, SVN_PROP_ENTRY_LAST_AUTHOR)) {
flags |= SVN_WC__ENTRY_MODIFY_CMT_AUTHOR;
tmp_entry.cmt_author = val;
} else if (! strcmp(prop->name, SVN_PROP_ENTRY_COMMITTED_REV)) {
flags |= SVN_WC__ENTRY_MODIFY_CMT_REV;
tmp_entry.cmt_rev = SVN_STR_TO_REV(val);
} else if (! strcmp(prop->name, SVN_PROP_ENTRY_COMMITTED_DATE)) {
flags |= SVN_WC__ENTRY_MODIFY_CMT_DATE;
SVN_ERR(svn_time_from_cstring(&tmp_entry.cmt_date, val, pool));
} else if (! strcmp(prop->name, SVN_PROP_ENTRY_UUID)) {
flags |= SVN_WC__ENTRY_MODIFY_UUID;
tmp_entry.uuid = val;
}
}
if (flags)
SVN_ERR(svn_wc__loggy_entry_modify(&log_accum, adm_access, path,
&tmp_entry, flags, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
accumulate_wcprops(svn_stringbuf_t *log_accum,
svn_wc_adm_access_t *adm_access,
const char *path,
apr_array_header_t *wcprops,
apr_pool_t *pool) {
int i;
for (i = 0; i < wcprops->nelts; ++i) {
const svn_prop_t *prop = &APR_ARRAY_IDX(wcprops, i, svn_prop_t);
SVN_ERR(svn_wc__loggy_modify_wcprop
(&log_accum, adm_access, path,
prop->name, prop->value ? prop->value->data : NULL, pool));
}
return SVN_NO_ERROR;
}
static svn_error_t *
check_path_under_root(const char *base_path,
const char *add_path,
apr_pool_t *pool) {
char *full_path;
apr_status_t path_status;
path_status = apr_filepath_merge
(&full_path, base_path, add_path,
APR_FILEPATH_NOTABOVEROOT | APR_FILEPATH_SECUREROOTTEST,
pool);
if (path_status != APR_SUCCESS) {
return svn_error_createf
(SVN_ERR_WC_OBSTRUCTED_UPDATE, NULL,
_("Path '%s' is not in the working copy"),
svn_path_local_style(svn_path_join(base_path, add_path, pool), pool));
}
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
struct dir_baton *d;
eb->root_opened = TRUE;
SVN_ERR(make_dir_baton(&d, NULL, eb, NULL, FALSE, pool));
*dir_baton = d;
if (! *eb->target) {
svn_wc_adm_access_t *adm_access;
svn_wc_entry_t tmp_entry;
const svn_wc_entry_t *entry;
apr_uint64_t flags = SVN_WC__ENTRY_MODIFY_REVISION |
SVN_WC__ENTRY_MODIFY_URL | SVN_WC__ENTRY_MODIFY_INCOMPLETE;
SVN_ERR(svn_wc_entry(&entry, d->path, eb->adm_access,
FALSE, pool));
if (entry)
d->ambient_depth = entry->depth;
tmp_entry.revision = *(eb->target_revision);
tmp_entry.url = d->new_URL;
if (eb->repos && svn_path_is_ancestor(eb->repos, d->new_URL)) {
tmp_entry.repos = eb->repos;
flags |= SVN_WC__ENTRY_MODIFY_REPOS;
}
tmp_entry.incomplete = TRUE;
SVN_ERR(svn_wc_adm_retrieve(&adm_access, eb->adm_access,
d->path, pool));
SVN_ERR(svn_wc__entry_modify(adm_access, NULL ,
&tmp_entry, flags,
TRUE ,
pool));
}
return SVN_NO_ERROR;
}
static svn_error_t *
leftmod_error_chain(svn_error_t *err,
const char *logfile,
const char *path,
apr_pool_t *pool) {
svn_error_t *tmp_err;
if (! err)
return SVN_NO_ERROR;
for (tmp_err = err; tmp_err; tmp_err = tmp_err->child)
if (tmp_err->apr_err == SVN_ERR_WC_LEFT_LOCAL_MOD)
break;
if (tmp_err) {
svn_error_clear(svn_io_remove_file(logfile, pool));
return svn_error_createf
(SVN_ERR_WC_OBSTRUCTED_UPDATE, tmp_err,
_("Won't delete locally modified directory '%s'"),
svn_path_local_style(path, pool));
}
return err;
}
static svn_error_t *
do_entry_deletion(struct edit_baton *eb,
const char *parent_path,
const char *path,
int *log_number,
apr_pool_t *pool) {
svn_wc_adm_access_t *adm_access;
const svn_wc_entry_t *entry;
const char *full_path = svn_path_join(eb->anchor, path, pool);
svn_stringbuf_t *log_item = svn_stringbuf_create("", pool);
SVN_ERR(svn_wc_adm_retrieve(&adm_access, eb->adm_access,
parent_path, pool));
SVN_ERR(svn_wc__loggy_delete_entry(&log_item, adm_access, full_path, pool));
SVN_ERR(svn_wc__entry_versioned(&entry, full_path, adm_access, FALSE, pool));
if (strcmp(path, eb->target) == 0) {
svn_wc_entry_t tmp_entry;
tmp_entry.revision = *(eb->target_revision);
tmp_entry.kind =
(entry->kind == svn_node_file) ? svn_node_file : svn_node_dir;
tmp_entry.deleted = TRUE;
SVN_ERR(svn_wc__loggy_entry_modify(&log_item, adm_access,
full_path, &tmp_entry,
SVN_WC__ENTRY_MODIFY_REVISION
| SVN_WC__ENTRY_MODIFY_KIND
| SVN_WC__ENTRY_MODIFY_DELETED,
pool));
eb->target_deleted = TRUE;
}
SVN_ERR(svn_wc__write_log(adm_access, *log_number, log_item, pool));
if (eb->switch_url) {
if (entry->kind == svn_node_dir) {
svn_wc_adm_access_t *child_access;
const char *logfile_path
= svn_wc__adm_path(parent_path, FALSE, pool,
svn_wc__logfile_path(*log_number, pool), NULL);
SVN_ERR(svn_wc_adm_retrieve
(&child_access, eb->adm_access,
full_path, pool));
SVN_ERR(leftmod_error_chain
(svn_wc_remove_from_revision_control
(child_access,
SVN_WC_ENTRY_THIS_DIR,
TRUE,
TRUE,
eb->cancel_func,
eb->cancel_baton,
pool),
logfile_path, parent_path, pool));
}
}
SVN_ERR(svn_wc__run_log(adm_access, NULL, pool));
*log_number = 0;
if (eb->notify_func)
(*eb->notify_func)
(eb->notify_baton,
svn_wc_create_notify(full_path,
svn_wc_notify_update_delete, pool), pool);
return SVN_NO_ERROR;
}
static svn_error_t *
delete_entry(const char *path,
svn_revnum_t revision,
void *parent_baton,
apr_pool_t *pool) {
struct dir_baton *pb = parent_baton;
SVN_ERR(check_path_under_root(pb->path, svn_path_basename(path, pool),
pool));
return do_entry_deletion(pb->edit_baton, pb->path, path, &pb->log_number,
pool);
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
struct dir_baton *db;
svn_node_kind_t kind;
SVN_ERR(make_dir_baton(&db, path, eb, pb, TRUE, pool));
*child_baton = db;
if (strcmp(eb->target, path) == 0) {
db->ambient_depth = (eb->requested_depth == svn_depth_unknown)
? svn_depth_infinity : eb->requested_depth;
} else if (eb->requested_depth == svn_depth_immediates
|| (eb->requested_depth == svn_depth_unknown
&& pb->ambient_depth == svn_depth_immediates)) {
db->ambient_depth = svn_depth_empty;
} else {
db->ambient_depth = svn_depth_infinity;
}
SVN_ERR(flush_log(pb, pool));
if ((copyfrom_path && (! SVN_IS_VALID_REVNUM(copyfrom_revision)))
|| ((! copyfrom_path) && (SVN_IS_VALID_REVNUM(copyfrom_revision))))
abort();
SVN_ERR(check_path_under_root(pb->path, db->name, pool));
SVN_ERR(svn_io_check_path(db->path, &kind, db->pool));
if (kind == svn_node_file || kind == svn_node_unknown)
return svn_error_createf
(SVN_ERR_WC_OBSTRUCTED_UPDATE, NULL,
_("Failed to add directory '%s': a non-directory object of the "
"same name already exists"),
svn_path_local_style(db->path, pool));
if (kind == svn_node_dir) {
svn_wc_adm_access_t *adm_access;
svn_error_t *err = svn_wc_adm_open3(&adm_access, NULL,
db->path, FALSE, 0,
NULL, NULL, pool);
if (err && err->apr_err != SVN_ERR_WC_NOT_DIRECTORY) {
return err;
} else if (err) {
svn_error_clear(err);
if (eb->allow_unver_obstructions) {
db->existed = TRUE;
} else {
return svn_error_createf
(SVN_ERR_WC_OBSTRUCTED_UPDATE, NULL,
_("Failed to add directory '%s': an unversioned "
"directory of the same name already exists"),
svn_path_local_style(db->path, pool));
}
} else {
const svn_wc_entry_t *entry;
SVN_ERR(svn_wc_entry(&entry, db->path, adm_access, FALSE, pool));
if (entry
&& entry->schedule == svn_wc_schedule_add
&& ! entry->copied) {
db->add_existed = TRUE;
} else {
return svn_error_createf
(SVN_ERR_WC_OBSTRUCTED_UPDATE, NULL,
_("Failed to add directory '%s': a versioned "
"directory of the same name already exists"),
svn_path_local_style(db->path, pool));
}
}
}
if (svn_wc_is_adm_dir(svn_path_basename(path, pool), pool))
return svn_error_createf
(SVN_ERR_WC_OBSTRUCTED_UPDATE, NULL,
_("Failed to add directory '%s': object of the same name as the "
"administrative directory"),
svn_path_local_style(db->path, pool));
if (copyfrom_path || SVN_IS_VALID_REVNUM(copyfrom_revision)) {
return svn_error_createf
(SVN_ERR_UNSUPPORTED_FEATURE, NULL,
_("Failed to add directory '%s': "
"copyfrom arguments not yet supported"),
svn_path_local_style(db->path, pool));
} else {
svn_wc_adm_access_t *adm_access;
svn_wc_entry_t tmp_entry;
apr_uint64_t modify_flags = SVN_WC__ENTRY_MODIFY_KIND |
SVN_WC__ENTRY_MODIFY_DELETED | SVN_WC__ENTRY_MODIFY_ABSENT;
SVN_ERR(svn_wc_adm_retrieve(&adm_access, eb->adm_access,
pb->path, db->pool));
tmp_entry.kind = svn_node_dir;
tmp_entry.deleted = FALSE;
tmp_entry.absent = FALSE;
if (db->add_existed) {
tmp_entry.schedule = svn_wc_schedule_normal;
modify_flags |= SVN_WC__ENTRY_MODIFY_SCHEDULE |
SVN_WC__ENTRY_MODIFY_FORCE;
}
SVN_ERR(svn_wc__entry_modify(adm_access, db->name, &tmp_entry,
modify_flags,
TRUE , pool));
if (db->add_existed) {
modify_flags = SVN_WC__ENTRY_MODIFY_SCHEDULE
| SVN_WC__ENTRY_MODIFY_FORCE | SVN_WC__ENTRY_MODIFY_REVISION;
SVN_ERR(svn_wc_adm_retrieve(&adm_access,
db->edit_baton->adm_access,
db->path, pool));
tmp_entry.revision = *(eb->target_revision);
if (eb->switch_url) {
tmp_entry.url = svn_path_url_add_component(eb->switch_url,
db->name, pool);
modify_flags |= SVN_WC__ENTRY_MODIFY_URL;
}
SVN_ERR(svn_wc__entry_modify(adm_access, NULL, &tmp_entry,
modify_flags,
TRUE , pool));
}
}
SVN_ERR(prep_directory(db,
db->new_URL,
*(eb->target_revision),
db->pool));
if (eb->notify_func && !(db->add_existed)) {
svn_wc_notify_t *notify = svn_wc_create_notify(
db->path,
db->existed ?
svn_wc_notify_exists : svn_wc_notify_update_add,
pool);
notify->kind = svn_node_dir;
(*eb->notify_func)(eb->notify_baton, notify, pool);
}
return SVN_NO_ERROR;
}
static svn_error_t *
open_directory(const char *path,
void *parent_baton,
svn_revnum_t base_revision,
apr_pool_t *pool,
void **child_baton) {
struct dir_baton *db, *pb = parent_baton;
struct edit_baton *eb = pb->edit_baton;
const svn_wc_entry_t *entry;
svn_wc_entry_t tmp_entry;
apr_uint64_t flags = SVN_WC__ENTRY_MODIFY_REVISION |
SVN_WC__ENTRY_MODIFY_URL | SVN_WC__ENTRY_MODIFY_INCOMPLETE;
svn_wc_adm_access_t *adm_access;
SVN_ERR(make_dir_baton(&db, path, eb, pb, FALSE, pool));
*child_baton = db;
SVN_ERR(flush_log(pb, pool));
SVN_ERR(check_path_under_root(pb->path, db->name, pool));
SVN_ERR(svn_wc_entry(&entry, db->path, eb->adm_access, FALSE, pool));
if (entry) {
svn_boolean_t text_conflicted;
svn_boolean_t prop_conflicted;
db->ambient_depth = entry->depth;
SVN_ERR(svn_wc_conflicted_p(&text_conflicted, &prop_conflicted,
db->path, entry, pool));
assert(! text_conflicted);
if (prop_conflicted) {
db->bump_info->skipped = TRUE;
apr_hash_set(eb->skipped_paths, apr_pstrdup(eb->pool, db->path),
APR_HASH_KEY_STRING, (void*)1);
if (eb->notify_func) {
svn_wc_notify_t *notify
= svn_wc_create_notify(db->path, svn_wc_notify_skip, pool);
notify->kind = svn_node_dir;
notify->prop_state = svn_wc_notify_state_conflicted;
(*eb->notify_func)(eb->notify_baton, notify, pool);
}
return SVN_NO_ERROR;
}
}
tmp_entry.revision = *(eb->target_revision);
tmp_entry.url = db->new_URL;
if (eb->repos && svn_path_is_ancestor(eb->repos, db->new_URL)) {
tmp_entry.repos = eb->repos;
flags |= SVN_WC__ENTRY_MODIFY_REPOS;
}
tmp_entry.incomplete = TRUE;
SVN_ERR(svn_wc_adm_retrieve(&adm_access, eb->adm_access,
db->path, pool));
SVN_ERR(svn_wc__entry_modify(adm_access, NULL ,
&tmp_entry, flags,
TRUE ,
pool));
return SVN_NO_ERROR;
}
static svn_error_t *
change_dir_prop(void *dir_baton,
const char *name,
const svn_string_t *value,
apr_pool_t *pool) {
svn_prop_t *propchange;
struct dir_baton *db = dir_baton;
if (db->bump_info->skipped)
return SVN_NO_ERROR;
propchange = apr_array_push(db->propchanges);
propchange->name = apr_pstrdup(db->pool, name);
propchange->value = value ? svn_string_dup(value, db->pool) : NULL;
return SVN_NO_ERROR;
}
static const svn_prop_t *
externals_prop_changed(apr_array_header_t *propchanges) {
int i;
for (i = 0; i < propchanges->nelts; i++) {
const svn_prop_t *p = &(APR_ARRAY_IDX(propchanges, i, svn_prop_t));
if (strcmp(p->name, SVN_PROP_EXTERNALS) == 0)
return p;
}
return NULL;
}
static svn_error_t *
close_directory(void *dir_baton,
apr_pool_t *pool) {
struct dir_baton *db = dir_baton;
svn_wc_notify_state_t prop_state = svn_wc_notify_state_unknown;
apr_array_header_t *entry_props, *wc_props, *regular_props;
svn_wc_adm_access_t *adm_access;
SVN_ERR(svn_categorize_props(db->propchanges, &entry_props, &wc_props,
&regular_props, pool));
SVN_ERR(svn_wc_adm_retrieve(&adm_access, db->edit_baton->adm_access,
db->path, db->pool));
if (regular_props->nelts || entry_props->nelts || wc_props->nelts) {
svn_stringbuf_t *dirprop_log = svn_stringbuf_create("", pool);
if (regular_props->nelts) {
if (db->edit_baton->traversal_info) {
svn_wc_traversal_info_t *ti = db->edit_baton->traversal_info;
const svn_prop_t *change = externals_prop_changed(regular_props);
if (change) {
const svn_string_t *new_val_s = change->value;
const svn_string_t *old_val_s;
SVN_ERR(svn_wc_prop_get
(&old_val_s, SVN_PROP_EXTERNALS,
db->path, adm_access, db->pool));
if ((new_val_s == NULL) && (old_val_s == NULL))
;
else if (new_val_s && old_val_s
&& (svn_string_compare(old_val_s, new_val_s)))
;
else if (old_val_s || new_val_s)
{
const char *d_path = apr_pstrdup(ti->pool, db->path);
apr_hash_set(ti->depths, d_path, APR_HASH_KEY_STRING,
svn_depth_to_word(db->ambient_depth));
if (old_val_s)
apr_hash_set(ti->externals_old, d_path,
APR_HASH_KEY_STRING,
apr_pstrmemdup(ti->pool, old_val_s->data,
old_val_s->len));
if (new_val_s)
apr_hash_set(ti->externals_new, d_path,
APR_HASH_KEY_STRING,
apr_pstrmemdup(ti->pool, new_val_s->data,
new_val_s->len));
}
}
}
SVN_ERR_W(svn_wc__merge_props(&prop_state,
adm_access, db->path,
NULL ,
NULL, NULL,
regular_props, TRUE, FALSE,
db->edit_baton->conflict_func,
db->edit_baton->conflict_baton,
db->pool, &dirprop_log),
_("Couldn't do property merge"));
}
SVN_ERR(accumulate_entry_props(dirprop_log, NULL,
adm_access, db->path,
entry_props, pool));
SVN_ERR(accumulate_wcprops(dirprop_log, adm_access,
db->path, wc_props, pool));
svn_stringbuf_appendstr(db->log_accum, dirprop_log);
}
SVN_ERR(flush_log(db, pool));
SVN_ERR(svn_wc__run_log(adm_access, db->edit_baton->diff3_cmd, db->pool));
db->log_number = 0;
SVN_ERR(maybe_bump_dir_info(db->edit_baton, db->bump_info, db->pool));
if (! db->bump_info->skipped && (db->add_existed || (! db->added))
&& (db->edit_baton->notify_func)) {
svn_wc_notify_t *notify
= svn_wc_create_notify(db->path,
db->existed || db->add_existed
? svn_wc_notify_exists
: svn_wc_notify_update_update,
pool);
notify->kind = svn_node_dir;
notify->prop_state = prop_state;
(*db->edit_baton->notify_func)(db->edit_baton->notify_baton,
notify, pool);
}
return SVN_NO_ERROR;
}
static svn_error_t *
absent_file_or_dir(const char *path,
svn_node_kind_t kind,
void *parent_baton,
apr_pool_t *pool) {
const char *name = svn_path_basename(path, pool);
struct dir_baton *pb = parent_baton;
struct edit_baton *eb = pb->edit_baton;
svn_wc_adm_access_t *adm_access;
apr_hash_t *entries;
const svn_wc_entry_t *ent;
svn_wc_entry_t tmp_entry;
SVN_ERR(svn_wc_adm_retrieve(&adm_access, eb->adm_access, pb->path, pool));
SVN_ERR(svn_wc_entries_read(&entries, adm_access, FALSE, pool));
ent = apr_hash_get(entries, name, APR_HASH_KEY_STRING);
if (ent && (ent->schedule == svn_wc_schedule_add))
return svn_error_createf
(SVN_ERR_WC_OBSTRUCTED_UPDATE, NULL,
_("Failed to mark '%s' absent: item of the same name is already "
"scheduled for addition"),
svn_path_local_style(path, pool));
tmp_entry.kind = kind;
tmp_entry.deleted = FALSE;
tmp_entry.revision = *(eb->target_revision);
tmp_entry.absent = TRUE;
SVN_ERR(svn_wc__entry_modify(adm_access, name, &tmp_entry,
(SVN_WC__ENTRY_MODIFY_KIND |
SVN_WC__ENTRY_MODIFY_REVISION |
SVN_WC__ENTRY_MODIFY_DELETED |
SVN_WC__ENTRY_MODIFY_ABSENT),
TRUE , pool));
return SVN_NO_ERROR;
}
static svn_error_t *
absent_file(const char *path,
void *parent_baton,
apr_pool_t *pool) {
return absent_file_or_dir(path, svn_node_file, parent_baton, pool);
}
static svn_error_t *
absent_directory(const char *path,
void *parent_baton,
apr_pool_t *pool) {
return absent_file_or_dir(path, svn_node_dir, parent_baton, pool);
}
static svn_error_t *
add_file(const char *path,
void *parent_baton,
const char *copyfrom_path,
svn_revnum_t copyfrom_rev,
apr_pool_t *pool,
void **file_baton) {
struct dir_baton *pb = parent_baton;
struct edit_baton *eb = pb->edit_baton;
struct file_baton *fb;
const svn_wc_entry_t *entry;
svn_node_kind_t kind;
svn_wc_adm_access_t *adm_access;
apr_pool_t *subpool;
if (copyfrom_path || SVN_IS_VALID_REVNUM(copyfrom_rev)) {
if (! (copyfrom_path && SVN_IS_VALID_REVNUM(copyfrom_rev)))
return svn_error_create(SVN_ERR_WC_INVALID_OP_ON_CWD, NULL,
_("Bad copyfrom arguments received"));
return add_file_with_history(path, parent_baton,
copyfrom_path, copyfrom_rev,
file_baton, pool);
}
subpool = svn_pool_create(pool);
SVN_ERR(make_file_baton(&fb, pb, path, TRUE, pool));
*file_baton = fb;
SVN_ERR(check_path_under_root(fb->dir_baton->path, fb->name, subpool));
SVN_ERR(svn_io_check_path(fb->path, &kind, subpool));
SVN_ERR(svn_wc_adm_retrieve(&adm_access, eb->adm_access,
pb->path, subpool));
SVN_ERR(svn_wc_entry(&entry, fb->path, adm_access, FALSE, subpool));
if (kind != svn_node_none) {
if (eb->allow_unver_obstructions
|| (entry && entry->schedule == svn_wc_schedule_add)) {
if (entry && entry->copied) {
return svn_error_createf(SVN_ERR_WC_OBSTRUCTED_UPDATE,
NULL,
_("Failed to add file '%s': a "
"file of the same name is "
"already scheduled for addition "
"with history"),
svn_path_local_style(fb->path,
pool));
}
if (kind != svn_node_file)
return svn_error_createf(SVN_ERR_WC_OBSTRUCTED_UPDATE,
NULL,
_("Failed to add file '%s': "
"a non-file object of the same "
"name already exists"),
svn_path_local_style(fb->path,
pool));
if (entry)
fb->add_existed = TRUE;
else
fb->existed = TRUE;
} else {
return svn_error_createf
(SVN_ERR_WC_OBSTRUCTED_UPDATE, NULL,
_("Failed to add file '%s': object of the same name "
"already exists"), svn_path_local_style(fb->path, pool));
}
}
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
static svn_error_t *
open_file(const char *path,
void *parent_baton,
svn_revnum_t base_revision,
apr_pool_t *pool,
void **file_baton) {
struct dir_baton *pb = parent_baton;
struct edit_baton *eb = pb->edit_baton;
struct file_baton *fb;
const svn_wc_entry_t *entry;
svn_node_kind_t kind;
svn_wc_adm_access_t *adm_access;
svn_boolean_t text_conflicted;
svn_boolean_t prop_conflicted;
apr_pool_t *subpool = svn_pool_create(pool);
SVN_ERR(make_file_baton(&fb, pb, path, FALSE, pool));
*file_baton = fb;
SVN_ERR(check_path_under_root(fb->dir_baton->path, fb->name, subpool));
SVN_ERR(svn_io_check_path(fb->path, &kind, subpool));
SVN_ERR(svn_wc_adm_retrieve(&adm_access, eb->adm_access,
pb->path, subpool));
SVN_ERR(svn_wc_entry(&entry, fb->path, adm_access, FALSE, subpool));
if (! entry)
return svn_error_createf(SVN_ERR_UNVERSIONED_RESOURCE, NULL,
_("File '%s' in directory '%s' "
"is not a versioned resource"),
fb->name,
svn_path_local_style(pb->path, pool));
SVN_ERR(svn_wc_conflicted_p(&text_conflicted, &prop_conflicted,
pb->path, entry, pool));
if (text_conflicted || prop_conflicted) {
fb->skipped = TRUE;
apr_hash_set(eb->skipped_paths, apr_pstrdup(eb->pool, fb->path),
APR_HASH_KEY_STRING, (void*)1);
if (eb->notify_func) {
svn_wc_notify_t *notify
= svn_wc_create_notify(fb->path, svn_wc_notify_skip, pool);
notify->kind = svn_node_file;
notify->content_state = text_conflicted
? svn_wc_notify_state_conflicted
: svn_wc_notify_state_unknown;
notify->prop_state = prop_conflicted
? svn_wc_notify_state_conflicted
: svn_wc_notify_state_unknown;
(*eb->notify_func)(eb->notify_baton, notify, pool);
}
}
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
static svn_error_t *
choose_base_paths(const char **checksum_p,
svn_boolean_t *replaced_p,
svn_boolean_t *use_revert_base_p,
struct file_baton *fb,
apr_pool_t *pool) {
struct edit_baton *eb = fb->edit_baton;
svn_wc_adm_access_t *adm_access;
const svn_wc_entry_t *ent;
svn_boolean_t replaced, use_revert_base;
SVN_ERR(svn_wc_adm_retrieve(&adm_access, eb->adm_access,
svn_path_dirname(fb->path, pool), pool));
SVN_ERR(svn_wc_entry(&ent, fb->path, adm_access, FALSE, pool));
replaced = ent && ent->schedule == svn_wc_schedule_replace;
use_revert_base = replaced && (ent->copyfrom_url != NULL);
if (use_revert_base) {
fb->text_base_path = svn_wc__text_revert_path(fb->path, FALSE, fb->pool);
fb->new_text_base_path = svn_wc__text_revert_path(fb->path, TRUE,
fb->pool);
} else {
fb->text_base_path = svn_wc__text_base_path(fb->path, FALSE, fb->pool);
fb->new_text_base_path = svn_wc__text_base_path(fb->path, TRUE,
fb->pool);
}
if (checksum_p) {
*checksum_p = NULL;
if (ent)
*checksum_p = ent->checksum;
}
if (replaced_p)
*replaced_p = replaced;
if (use_revert_base_p)
*use_revert_base_p = use_revert_base;
return SVN_NO_ERROR;
}
static svn_error_t *
apply_textdelta(void *file_baton,
const char *base_checksum,
apr_pool_t *pool,
svn_txdelta_window_handler_t *handler,
void **handler_baton) {
struct file_baton *fb = file_baton;
apr_pool_t *handler_pool = svn_pool_create(fb->pool);
struct handler_baton *hb = apr_palloc(handler_pool, sizeof(*hb));
svn_error_t *err;
const char *checksum;
svn_boolean_t replaced;
svn_boolean_t use_revert_base;
if (fb->skipped) {
*handler = svn_delta_noop_window_handler;
*handler_baton = NULL;
return SVN_NO_ERROR;
}
fb->received_textdelta = TRUE;
SVN_ERR(choose_base_paths(&checksum, &replaced, &use_revert_base,
fb, pool));
if (checksum) {
unsigned char digest[APR_MD5_DIGESTSIZE];
const char *hex_digest;
SVN_ERR(svn_io_file_checksum(digest, fb->text_base_path, pool));
hex_digest = svn_md5_digest_to_cstring_display(digest, pool);
if (base_checksum) {
if (strcmp(hex_digest, base_checksum) != 0)
return svn_error_createf
(SVN_ERR_WC_CORRUPT_TEXT_BASE, NULL,
_("Checksum mismatch for '%s'; expected: '%s', actual: '%s'"),
svn_path_local_style(fb->text_base_path, pool), base_checksum,
hex_digest);
}
if (! replaced && strcmp(hex_digest, checksum) != 0) {
return svn_error_createf
(SVN_ERR_WC_CORRUPT_TEXT_BASE, NULL,
_("Checksum mismatch for '%s'; recorded: '%s', actual: '%s'"),
svn_path_local_style(fb->text_base_path, pool), checksum,
hex_digest);
}
}
if (! fb->added) {
if (use_revert_base)
SVN_ERR(svn_wc__open_revert_base(&hb->source, fb->path,
APR_READ,
handler_pool));
else
SVN_ERR(svn_wc__open_text_base(&hb->source, fb->path, APR_READ,
handler_pool));
} else {
if (fb->copied_text_base)
SVN_ERR(svn_io_file_open(&hb->source, fb->copied_text_base,
APR_READ, APR_OS_DEFAULT, handler_pool));
else
hb->source = NULL;
}
if (use_revert_base)
err = svn_wc__open_revert_base(&hb->dest, fb->path,
(APR_WRITE | APR_TRUNCATE | APR_CREATE),
handler_pool);
else
err = svn_wc__open_text_base(&hb->dest, fb->path,
(APR_WRITE | APR_TRUNCATE | APR_CREATE),
handler_pool);
if (err) {
svn_pool_destroy(handler_pool);
return err;
}
svn_txdelta_apply(svn_stream_from_aprfile(hb->source, handler_pool),
svn_stream_from_aprfile(hb->dest, handler_pool),
fb->digest, fb->new_text_base_path, handler_pool,
&hb->apply_handler, &hb->apply_baton);
hb->pool = handler_pool;
hb->fb = fb;
*handler_baton = hb;
*handler = window_handler;
return SVN_NO_ERROR;
}
static svn_error_t *
change_file_prop(void *file_baton,
const char *name,
const svn_string_t *value,
apr_pool_t *pool) {
struct file_baton *fb = file_baton;
struct edit_baton *eb = fb->edit_baton;
svn_prop_t *propchange;
if (fb->skipped)
return SVN_NO_ERROR;
propchange = apr_array_push(fb->propchanges);
propchange->name = apr_pstrdup(fb->pool, name);
propchange->value = value ? svn_string_dup(value, fb->pool) : NULL;
if (eb->use_commit_times
&& (strcmp(name, SVN_PROP_ENTRY_COMMITTED_DATE) == 0)
&& value)
fb->last_changed_date = apr_pstrdup(fb->pool, value->data);
return SVN_NO_ERROR;
}
static svn_error_t *
merge_props(svn_stringbuf_t *log_accum,
svn_wc_notify_state_t *prop_state,
svn_wc_notify_lock_state_t *lock_state,
svn_wc_adm_access_t *adm_access,
const char *file_path,
const apr_array_header_t *prop_changes,
apr_hash_t *base_props,
apr_hash_t *working_props,
svn_wc_conflict_resolver_func_t conflict_func,
void *conflict_baton,
apr_pool_t *pool) {
apr_array_header_t *regular_props = NULL, *wc_props = NULL,
*entry_props = NULL;
SVN_ERR(svn_categorize_props(prop_changes,
&entry_props, &wc_props, &regular_props,
pool));
*prop_state = svn_wc_notify_state_unknown;
if (regular_props) {
SVN_ERR(svn_wc__merge_props(prop_state,
adm_access, file_path,
NULL ,
base_props,
working_props,
regular_props, TRUE, FALSE,
conflict_func, conflict_baton,
pool, &log_accum));
}
if (entry_props)
SVN_ERR(accumulate_entry_props(log_accum, lock_state,
adm_access, file_path,
entry_props, pool));
else
*lock_state = svn_wc_notify_lock_state_unchanged;
if (wc_props)
SVN_ERR(accumulate_wcprops(log_accum, adm_access,
file_path, wc_props, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
loggy_tweak_entry(svn_stringbuf_t *log_accum,
svn_wc_adm_access_t *adm_access,
const char *path,
svn_revnum_t new_revision,
const char *new_URL,
apr_pool_t *pool) {
svn_wc_entry_t tmp_entry;
apr_uint64_t modify_flags = SVN_WC__ENTRY_MODIFY_KIND
| SVN_WC__ENTRY_MODIFY_REVISION
| SVN_WC__ENTRY_MODIFY_DELETED
| SVN_WC__ENTRY_MODIFY_ABSENT
| SVN_WC__ENTRY_MODIFY_TEXT_TIME
| SVN_WC__ENTRY_MODIFY_WORKING_SIZE;
tmp_entry.revision = new_revision;
tmp_entry.kind = svn_node_file;
tmp_entry.deleted = FALSE;
tmp_entry.absent = FALSE;
tmp_entry.working_size = SVN_WC_ENTRY_WORKING_SIZE_UNKNOWN;
tmp_entry.text_time = 0;
if (new_URL) {
tmp_entry.url = new_URL;
modify_flags |= SVN_WC__ENTRY_MODIFY_URL;
}
SVN_ERR(svn_wc__loggy_entry_modify(&log_accum, adm_access,
path, &tmp_entry, modify_flags,
pool));
return SVN_NO_ERROR;
}
static svn_error_t *
merge_file(svn_wc_notify_state_t *content_state,
svn_wc_notify_state_t *prop_state,
svn_wc_notify_lock_state_t *lock_state,
struct file_baton *fb,
apr_pool_t *pool) {
const char *parent_dir;
struct edit_baton *eb = fb->edit_baton;
svn_stringbuf_t *log_accum = svn_stringbuf_create("", pool);
svn_wc_adm_access_t *adm_access;
svn_boolean_t is_locally_modified;
svn_boolean_t is_replaced = FALSE;
svn_boolean_t magic_props_changed;
enum svn_wc_merge_outcome_t merge_outcome = svn_wc_merge_unchanged;
const svn_wc_entry_t *entry;
svn_wc_entry_t tmp_entry;
apr_uint64_t flags = 0;
svn_path_split(fb->path, &parent_dir, NULL, pool);
SVN_ERR(svn_wc_adm_retrieve(&adm_access, eb->adm_access,
parent_dir, pool));
SVN_ERR(svn_wc_entry(&entry, fb->path, adm_access, FALSE, pool));
if (! entry && ! fb->added)
return svn_error_createf(
SVN_ERR_UNVERSIONED_RESOURCE, NULL,
_("'%s' is not under version control"),
svn_path_local_style(fb->path, pool));
magic_props_changed = svn_wc__has_magic_property(fb->propchanges);
SVN_ERR(merge_props(log_accum, prop_state, lock_state, adm_access,
fb->path, fb->propchanges,
fb->copied_base_props, fb->copied_working_props,
eb->conflict_func, eb->conflict_baton, pool));
if (fb->copied_working_text)
is_locally_modified = TRUE;
else if (! fb->existed)
SVN_ERR(svn_wc__text_modified_internal_p(&is_locally_modified, fb->path,
FALSE, adm_access, FALSE, pool));
else if (fb->new_text_base_path)
SVN_ERR(svn_wc__versioned_file_modcheck(&is_locally_modified, fb->path,
adm_access,
fb->new_text_base_path,
FALSE, pool));
else
is_locally_modified = FALSE;
if (entry && entry->schedule == svn_wc_schedule_replace)
is_replaced = TRUE;
if (fb->add_existed) {
tmp_entry.schedule = svn_wc_schedule_normal;
flags |= (SVN_WC__ENTRY_MODIFY_SCHEDULE |
SVN_WC__ENTRY_MODIFY_FORCE);
}
SVN_ERR(loggy_tweak_entry(log_accum, adm_access, fb->path,
*eb->target_revision, fb->new_URL, pool));
if (fb->new_text_base_path) {
if (! is_locally_modified && ! is_replaced) {
SVN_ERR(svn_wc__loggy_copy(&log_accum, NULL, adm_access,
svn_wc__copy_translate,
fb->new_text_base_path,
fb->path, FALSE, pool));
} else {
svn_node_kind_t wfile_kind = svn_node_unknown;
SVN_ERR(svn_io_check_path(fb->path, &wfile_kind, pool));
if (wfile_kind == svn_node_none && ! fb->added_with_history) {
SVN_ERR(svn_wc__loggy_copy(&log_accum, NULL, adm_access,
svn_wc__copy_translate,
fb->new_text_base_path,
fb->path, FALSE, pool));
} else if (! fb->existed)
{
const char *oldrev_str, *newrev_str, *mine_str;
const char *merge_left;
const char *path_ext = "";
if (eb->ext_patterns && eb->ext_patterns->nelts) {
svn_path_splitext(NULL, &path_ext, fb->path, pool);
if (! (*path_ext
&& svn_cstring_match_glob_list(path_ext,
eb->ext_patterns)))
path_ext = "";
}
if (fb->added_with_history)
oldrev_str = apr_psprintf(pool, ".copied%s%s",
*path_ext ? "." : "",
*path_ext ? path_ext : "");
else
oldrev_str = apr_psprintf(pool, ".r%ld%s%s",
entry->revision,
*path_ext ? "." : "",
*path_ext ? path_ext : "");
newrev_str = apr_psprintf(pool, ".r%ld%s%s",
*eb->target_revision,
*path_ext ? "." : "",
*path_ext ? path_ext : "");
mine_str = apr_psprintf(pool, ".mine%s%s",
*path_ext ? "." : "",
*path_ext ? path_ext : "");
if (fb->add_existed && ! is_replaced) {
SVN_ERR(svn_wc_create_tmp_file2(NULL, &merge_left,
svn_wc_adm_access_path(
adm_access),
svn_io_file_del_none,
pool));
} else if (fb->copied_text_base)
merge_left = fb->copied_text_base;
else
merge_left = fb->text_base_path;
SVN_ERR(svn_wc__merge_internal
(&log_accum, &merge_outcome,
merge_left,
fb->new_text_base_path,
fb->path,
fb->copied_working_text,
adm_access,
oldrev_str, newrev_str, mine_str,
FALSE, eb->diff3_cmd, NULL, fb->propchanges,
eb->conflict_func, eb->conflict_baton, pool));
if (merge_left != fb->text_base_path)
SVN_ERR(svn_wc__loggy_remove(&log_accum, adm_access,
merge_left, pool));
if (fb->copied_working_text)
SVN_ERR(svn_wc__loggy_remove(&log_accum, adm_access,
fb->copied_working_text, pool));
}
}
}
else {
apr_hash_t *keywords;
SVN_ERR(svn_wc__get_keywords(&keywords, fb->path,
adm_access, NULL, pool));
if (magic_props_changed || keywords)
{
const char *tmptext;
SVN_ERR(svn_wc_translated_file2(&tmptext, fb->path, fb->path,
adm_access,
SVN_WC_TRANSLATE_TO_NF
| SVN_WC_TRANSLATE_NO_OUTPUT_CLEANUP,
pool));
SVN_ERR(svn_wc__loggy_copy(&log_accum, NULL, adm_access,
svn_wc__copy_translate,
tmptext, fb->path, FALSE, pool));
}
if (*lock_state == svn_wc_notify_lock_state_unlocked)
SVN_ERR(svn_wc__loggy_maybe_set_readonly(&log_accum, adm_access,
fb->path, pool));
}
if (fb->new_text_base_path) {
SVN_ERR(svn_wc__loggy_move(&log_accum, NULL,
adm_access, fb->new_text_base_path,
fb->text_base_path, FALSE, pool));
SVN_ERR(svn_wc__loggy_set_readonly(&log_accum, adm_access,
fb->text_base_path, pool));
if (!is_replaced) {
tmp_entry.checksum = svn_md5_digest_to_cstring(fb->digest, pool);
flags |= SVN_WC__ENTRY_MODIFY_CHECKSUM;
}
}
SVN_ERR(svn_wc__loggy_entry_modify(&log_accum, adm_access,
fb->path, &tmp_entry, flags, pool));
if (!is_locally_modified &&
(fb->added || entry->schedule == svn_wc_schedule_normal)) {
if (fb->last_changed_date && !fb->existed)
SVN_ERR(svn_wc__loggy_set_timestamp(&log_accum, adm_access,
fb->path, fb->last_changed_date,
pool));
if (fb->new_text_base_path || magic_props_changed) {
SVN_ERR(svn_wc__loggy_set_entry_timestamp_from_wc
(&log_accum, adm_access,
fb->path, SVN_WC__ENTRY_ATTR_TEXT_TIME, pool));
}
SVN_ERR(svn_wc__loggy_set_entry_working_size_from_wc
(&log_accum, adm_access, fb->path, pool));
}
if (fb->copied_text_base)
SVN_ERR(svn_wc__loggy_remove(&log_accum, adm_access,
fb->copied_text_base,
pool));
if (merge_outcome == svn_wc_merge_conflict)
*content_state = svn_wc_notify_state_conflicted;
else if (fb->new_text_base_path) {
if (is_locally_modified)
*content_state = svn_wc_notify_state_merged;
else
*content_state = svn_wc_notify_state_changed;
} else
*content_state = svn_wc_notify_state_unchanged;
svn_stringbuf_appendstr(fb->dir_baton->log_accum, log_accum);
return SVN_NO_ERROR;
}
static svn_error_t *
close_file(void *file_baton,
const char *text_checksum,
apr_pool_t *pool) {
struct file_baton *fb = file_baton;
struct edit_baton *eb = fb->edit_baton;
svn_wc_notify_state_t content_state, prop_state;
svn_wc_notify_lock_state_t lock_state;
if (fb->skipped) {
SVN_ERR(maybe_bump_dir_info(eb, fb->bump_info, pool));
return SVN_NO_ERROR;
}
if (fb->added_with_history && ! fb->received_textdelta) {
assert(! fb->text_base_path && ! fb->new_text_base_path
&& fb->copied_text_base);
SVN_ERR(choose_base_paths(NULL, NULL, NULL, fb, pool));
SVN_ERR(svn_io_copy_file(fb->copied_text_base,
fb->new_text_base_path,
TRUE, pool));
SVN_ERR(svn_io_file_checksum(fb->digest,
fb->new_text_base_path,
pool));
}
if (fb->new_text_base_path && text_checksum) {
const char *real_sum = svn_md5_digest_to_cstring(fb->digest, pool);
if (real_sum && (strcmp(text_checksum, real_sum) != 0))
return svn_error_createf
(SVN_ERR_CHECKSUM_MISMATCH, NULL,
_("Checksum mismatch for '%s'; expected: '%s', actual: '%s'"),
svn_path_local_style(fb->path, pool), text_checksum, real_sum);
}
SVN_ERR(merge_file(&content_state, &prop_state, &lock_state, fb, pool));
SVN_ERR(maybe_bump_dir_info(eb, fb->bump_info, pool));
if (((content_state != svn_wc_notify_state_unchanged) ||
(prop_state != svn_wc_notify_state_unchanged) ||
(lock_state != svn_wc_notify_lock_state_unchanged))
&& eb->notify_func) {
svn_wc_notify_t *notify;
svn_wc_notify_action_t action = svn_wc_notify_update_update;
if (fb->existed || fb->add_existed) {
if (content_state != svn_wc_notify_state_conflicted)
action = svn_wc_notify_exists;
} else if (fb->added) {
action = svn_wc_notify_update_add;
}
notify = svn_wc_create_notify(fb->path, action, pool);
notify->kind = svn_node_file;
notify->content_state = content_state;
notify->prop_state = prop_state;
notify->lock_state = lock_state;
(*eb->notify_func)(eb->notify_baton, notify, pool);
}
return SVN_NO_ERROR;
}
static svn_error_t *
locate_copyfrom(const char *copyfrom_path,
svn_revnum_t copyfrom_rev,
const char *dest_dir,
const svn_wc_entry_t *dest_entry,
const char **return_path,
const svn_wc_entry_t **return_entry,
svn_wc_adm_access_t **return_access,
apr_pool_t *pool) {
const char *dest_fs_path, *ancestor_fs_path, *ancestor_url, *file_url;
const char *copyfrom_parent, *copyfrom_file;
const char *abs_dest_dir, *extra_components;
const svn_wc_entry_t *ancestor_entry, *file_entry;
svn_wc_adm_access_t *ancestor_access;
apr_size_t levels_up;
svn_stringbuf_t *cwd, *cwd_parent;
svn_node_kind_t kind;
svn_error_t *err;
apr_pool_t *subpool = svn_pool_create(pool);
*return_path = NULL;
if ((! dest_entry->repos) || (! dest_entry->url))
return svn_error_create(SVN_ERR_WC_COPYFROM_PATH_NOT_FOUND, NULL,
_("Destination directory of add-with-history "
"is missing a URL"));
svn_path_split(copyfrom_path, &copyfrom_parent, &copyfrom_file, pool);
SVN_ERR(svn_path_get_absolute(&abs_dest_dir, dest_dir, pool));
dest_fs_path = svn_path_is_child(dest_entry->repos, dest_entry->url, pool);
if (! dest_fs_path) {
if (strcmp(dest_entry->repos, dest_entry->url) == 0)
dest_fs_path = "";
else
return svn_error_create(SVN_ERR_WC_COPYFROM_PATH_NOT_FOUND, NULL,
_("Destination URLs are broken"));
}
dest_fs_path = apr_pstrcat(pool, "/", dest_fs_path, NULL);
dest_fs_path = svn_path_canonicalize(dest_fs_path, pool);
ancestor_fs_path = svn_path_get_longest_ancestor(dest_fs_path,
copyfrom_parent, pool);
if (strlen(ancestor_fs_path) == 0)
return SVN_NO_ERROR;
levels_up = svn_path_component_count(dest_fs_path)
- svn_path_component_count(ancestor_fs_path);
cwd = svn_stringbuf_create(dest_dir, pool);
svn_path_remove_components(cwd, levels_up);
SVN_ERR(svn_io_check_path(cwd->data, &kind, subpool));
if (kind != svn_node_dir)
return SVN_NO_ERROR;
err = svn_wc_adm_open3(&ancestor_access, NULL, cwd->data,
FALSE,
0,
NULL, NULL, subpool);
if (err && err->apr_err == SVN_ERR_WC_NOT_DIRECTORY) {
svn_error_clear(err);
return SVN_NO_ERROR;
} else if (err)
return err;
SVN_ERR(svn_wc_entry(&ancestor_entry, cwd->data, ancestor_access,
FALSE, subpool));
if (dest_entry->uuid && ancestor_entry->uuid
&& (strcmp(dest_entry->uuid, ancestor_entry->uuid) != 0))
return SVN_NO_ERROR;
ancestor_url = apr_pstrcat(subpool,
dest_entry->repos, ancestor_fs_path, NULL);
if (strcmp(ancestor_url, ancestor_entry->url) != 0)
return SVN_NO_ERROR;
svn_pool_clear(subpool);
extra_components = svn_path_is_child(ancestor_fs_path,
copyfrom_path, pool);
svn_path_add_component(cwd, extra_components);
cwd_parent = svn_stringbuf_create(cwd->data, pool);
svn_path_remove_component(cwd_parent);
SVN_ERR(svn_io_check_path(cwd->data, &kind, subpool));
if (kind != svn_node_file)
return SVN_NO_ERROR;
err = svn_wc_adm_open3(&ancestor_access, NULL, cwd_parent->data,
FALSE,
0,
NULL, NULL, pool);
if (err && err->apr_err == SVN_ERR_WC_NOT_DIRECTORY) {
svn_error_clear(err);
return SVN_NO_ERROR;
} else if (err)
return err;
SVN_ERR(svn_wc_entry(&file_entry, cwd->data, ancestor_access,
FALSE, pool));
if (! file_entry)
return SVN_NO_ERROR;
if (file_entry->uuid && dest_entry->uuid
&& (strcmp(file_entry->uuid, dest_entry->uuid) != 0))
return SVN_NO_ERROR;
file_url = apr_pstrcat(subpool, file_entry->repos, copyfrom_path, NULL);
if (strcmp(file_url, file_entry->url) != 0)
return SVN_NO_ERROR;
if (! (SVN_IS_VALID_REVNUM(file_entry->cmt_rev)
&& SVN_IS_VALID_REVNUM(file_entry->revision)))
return SVN_NO_ERROR;
if (! ((file_entry->cmt_rev <= copyfrom_rev)
&& (copyfrom_rev <= file_entry->revision)))
return SVN_NO_ERROR;
*return_path = apr_pstrdup(pool, cwd->data);
*return_entry = file_entry;
*return_access = ancestor_access;
svn_pool_clear(subpool);
return SVN_NO_ERROR;
}
static apr_hash_t *
copy_regular_props(apr_hash_t *props_in,
apr_pool_t *pool) {
apr_hash_t *props_out = apr_hash_make(pool);
apr_hash_index_t *hi;
for (hi = apr_hash_first(pool, props_in); hi; hi = apr_hash_next(hi)) {
const void *key;
void *val;
const char *propname;
svn_string_t *propval;
apr_hash_this(hi, &key, NULL, &val);
propname = key;
propval = val;
if (svn_property_kind(NULL, propname) == svn_prop_regular_kind)
apr_hash_set(props_out, propname, APR_HASH_KEY_STRING, propval);
}
return props_out;
}
static svn_error_t *
add_file_with_history(const char *path,
void *parent_baton,
const char *copyfrom_path,
svn_revnum_t copyfrom_rev,
void **file_baton,
apr_pool_t *pool) {
void *fb;
struct file_baton *tfb;
struct dir_baton *pb = parent_baton;
struct edit_baton *eb = pb->edit_baton;
svn_wc_adm_access_t *adm_access, *src_access;
const char *src_path;
const svn_wc_entry_t *src_entry;
apr_hash_t *base_props, *working_props;
const svn_wc_entry_t *path_entry;
svn_error_t *err;
apr_pool_t *subpool = svn_pool_create(pool);
SVN_ERR(add_file(path, parent_baton, NULL, SVN_INVALID_REVNUM, pool, &fb));
tfb = (struct file_baton *)fb;
tfb->added_with_history = TRUE;
SVN_ERR(svn_wc_entry(&path_entry, pb->path, eb->adm_access, FALSE, subpool));
err = locate_copyfrom(copyfrom_path, copyfrom_rev,
pb->path, path_entry,
&src_path, &src_entry, &src_access, subpool);
if (err && err->apr_err == SVN_ERR_WC_COPYFROM_PATH_NOT_FOUND)
svn_error_clear(err);
else if (err)
return err;
SVN_ERR(svn_wc_adm_retrieve(&adm_access, pb->edit_baton->adm_access,
pb->path, subpool));
SVN_ERR(svn_wc_create_tmp_file2(NULL, &tfb->copied_text_base,
svn_wc_adm_access_path(adm_access),
svn_io_file_del_none,
pool));
if (src_path != NULL) {
const char *src_text_base_path;
if (src_entry->schedule == svn_wc_schedule_replace
&& src_entry->copyfrom_url) {
src_text_base_path = svn_wc__text_revert_path(src_path,
FALSE, subpool);
SVN_ERR(svn_wc__load_props(NULL, NULL, &base_props,
src_access, src_path, pool));
working_props = base_props;
} else {
src_text_base_path = svn_wc__text_base_path(src_path,
FALSE, subpool);
SVN_ERR(svn_wc__load_props(&base_props, &working_props, NULL,
src_access, src_path, pool));
}
SVN_ERR(svn_io_copy_file(src_text_base_path, tfb->copied_text_base,
TRUE, subpool));
} else {
apr_file_t *textbase_file;
svn_stream_t *textbase_stream;
if (! eb->fetch_func)
return svn_error_create(SVN_ERR_WC_INVALID_OP_ON_CWD, NULL,
_("No fetch_func supplied to update_editor"));
SVN_ERR(svn_io_file_open(&textbase_file, tfb->copied_text_base,
(APR_WRITE | APR_TRUNCATE | APR_CREATE),
APR_OS_DEFAULT, subpool));
textbase_stream = svn_stream_from_aprfile2(textbase_file, FALSE, pool);
SVN_ERR(eb->fetch_func(eb->fetch_baton, copyfrom_path + 1, copyfrom_rev,
textbase_stream,
NULL, &base_props, pool));
SVN_ERR(svn_stream_close(textbase_stream));
working_props = base_props;
}
tfb->copied_base_props = copy_regular_props(base_props, pool);
tfb->copied_working_props = copy_regular_props(working_props, pool);
if (src_path != NULL) {
svn_boolean_t text_changed;
SVN_ERR(svn_wc_text_modified_p(&text_changed, src_path, FALSE,
src_access, subpool));
if (text_changed) {
SVN_ERR(svn_wc_create_tmp_file2(NULL, &tfb->copied_working_text,
svn_wc_adm_access_path(adm_access),
svn_io_file_del_none,
pool));
SVN_ERR(svn_io_copy_file(src_path, tfb->copied_working_text, TRUE,
subpool));
}
}
svn_pool_destroy(subpool);
*file_baton = tfb;
return SVN_NO_ERROR;
}
static svn_error_t *
close_edit(void *edit_baton,
apr_pool_t *pool) {
struct edit_baton *eb = edit_baton;
const char *target_path = svn_path_join(eb->anchor, eb->target, pool);
int log_number = 0;
if ((*eb->target) && (svn_wc__adm_missing(eb->adm_access, target_path)))
SVN_ERR(do_entry_deletion(eb, eb->anchor, eb->target, &log_number,
pool));
if (! eb->root_opened) {
SVN_ERR(complete_directory(eb, eb->anchor, TRUE, pool));
}
if (! eb->target_deleted)
SVN_ERR(svn_wc__do_update_cleanup(target_path,
eb->adm_access,
eb->requested_depth,
eb->switch_url,
eb->repos,
*(eb->target_revision),
eb->notify_func,
eb->notify_baton,
TRUE, eb->skipped_paths,
eb->pool));
svn_pool_destroy(eb->pool);
return SVN_NO_ERROR;
}
static svn_error_t *
make_editor(svn_revnum_t *target_revision,
svn_wc_adm_access_t *adm_access,
const char *anchor,
const char *target,
svn_boolean_t use_commit_times,
const char *switch_url,
svn_depth_t depth,
svn_boolean_t depth_is_sticky,
svn_boolean_t allow_unver_obstructions,
svn_wc_notify_func2_t notify_func,
void *notify_baton,
svn_cancel_func_t cancel_func,
void *cancel_baton,
svn_wc_conflict_resolver_func_t conflict_func,
void *conflict_baton,
svn_wc_get_file_t fetch_func,
void *fetch_baton,
const char *diff3_cmd,
apr_array_header_t *preserved_exts,
const svn_delta_editor_t **editor,
void **edit_baton,
svn_wc_traversal_info_t *traversal_info,
apr_pool_t *pool) {
struct edit_baton *eb;
void *inner_baton;
apr_pool_t *subpool = svn_pool_create(pool);
svn_delta_editor_t *tree_editor = svn_delta_default_editor(subpool);
const svn_delta_editor_t *inner_editor;
const svn_wc_entry_t *entry;
if (depth == svn_depth_unknown)
depth_is_sticky = FALSE;
SVN_ERR(svn_wc_entry(&entry, anchor, adm_access, FALSE, pool));
if (switch_url && entry && entry->repos &&
! svn_path_is_ancestor(entry->repos, switch_url))
return svn_error_createf
(SVN_ERR_WC_INVALID_SWITCH, NULL,
_("'%s'\n"
"is not the same repository as\n"
"'%s'"), switch_url, entry->repos);
eb = apr_pcalloc(subpool, sizeof(*eb));
eb->pool = subpool;
eb->use_commit_times = use_commit_times;
eb->target_revision = target_revision;
eb->switch_url = switch_url;
eb->repos = entry ? entry->repos : NULL;
eb->adm_access = adm_access;
eb->anchor = anchor;
eb->target = target;
eb->requested_depth = depth;
eb->depth_is_sticky = depth_is_sticky;
eb->notify_func = notify_func;
eb->notify_baton = notify_baton;
eb->traversal_info = traversal_info;
eb->diff3_cmd = diff3_cmd;
eb->cancel_func = cancel_func;
eb->cancel_baton = cancel_baton;
eb->conflict_func = conflict_func;
eb->conflict_baton = conflict_baton;
eb->fetch_func = fetch_func;
eb->fetch_baton = fetch_baton;
eb->allow_unver_obstructions = allow_unver_obstructions;
eb->skipped_paths = apr_hash_make(subpool);
eb->ext_patterns = preserved_exts;
tree_editor->set_target_revision = set_target_revision;
tree_editor->open_root = open_root;
tree_editor->delete_entry = delete_entry;
tree_editor->add_directory = add_directory;
tree_editor->open_directory = open_directory;
tree_editor->change_dir_prop = change_dir_prop;
tree_editor->close_directory = close_directory;
tree_editor->absent_directory = absent_directory;
tree_editor->add_file = add_file;
tree_editor->open_file = open_file;
tree_editor->apply_textdelta = apply_textdelta;
tree_editor->change_file_prop = change_file_prop;
tree_editor->close_file = close_file;
tree_editor->absent_file = absent_file;
tree_editor->close_edit = close_edit;
inner_editor = tree_editor;
inner_baton = eb;
if (depth_is_sticky) {
const svn_wc_entry_t *target_entry;
SVN_ERR(svn_wc_entry(&target_entry, svn_path_join(anchor, target, pool),
adm_access, FALSE, pool));
if (target_entry && (target_entry->depth > depth))
return svn_error_createf(SVN_ERR_UNSUPPORTED_FEATURE, NULL,
_("Shallowing of working copy depths is not "
"yet supported"));
} else {
SVN_ERR(svn_wc__ambient_depth_filter_editor(&inner_editor,
&inner_baton,
inner_editor,
inner_baton,
anchor,
target,
adm_access,
pool));
}
SVN_ERR(svn_delta_get_cancellation_editor(cancel_func,
cancel_baton,
inner_editor,
inner_baton,
editor,
edit_baton,
pool));
return SVN_NO_ERROR;
}
svn_error_t *
svn_wc_get_update_editor3(svn_revnum_t *target_revision,
svn_wc_adm_access_t *anchor,
const char *target,
svn_boolean_t use_commit_times,
svn_depth_t depth,
svn_boolean_t depth_is_sticky,
svn_boolean_t allow_unver_obstructions,
svn_wc_notify_func2_t notify_func,
void *notify_baton,
svn_cancel_func_t cancel_func,
void *cancel_baton,
svn_wc_conflict_resolver_func_t conflict_func,
void *conflict_baton,
svn_wc_get_file_t fetch_func,
void *fetch_baton,
const char *diff3_cmd,
apr_array_header_t *preserved_exts,
const svn_delta_editor_t **editor,
void **edit_baton,
svn_wc_traversal_info_t *traversal_info,
apr_pool_t *pool) {
return make_editor(target_revision, anchor, svn_wc_adm_access_path(anchor),
target, use_commit_times, NULL, depth, depth_is_sticky,
allow_unver_obstructions, notify_func, notify_baton,
cancel_func, cancel_baton, conflict_func, conflict_baton,
fetch_func, fetch_baton,
diff3_cmd, preserved_exts, editor, edit_baton,
traversal_info, pool);
}
svn_error_t *
svn_wc_get_update_editor2(svn_revnum_t *target_revision,
svn_wc_adm_access_t *anchor,
const char *target,
svn_boolean_t use_commit_times,
svn_boolean_t recurse,
svn_wc_notify_func2_t notify_func,
void *notify_baton,
svn_cancel_func_t cancel_func,
void *cancel_baton,
const char *diff3_cmd,
const svn_delta_editor_t **editor,
void **edit_baton,
svn_wc_traversal_info_t *traversal_info,
apr_pool_t *pool) {
return svn_wc_get_update_editor3(target_revision, anchor, target,
use_commit_times,
SVN_DEPTH_INFINITY_OR_FILES(recurse), FALSE,
FALSE, notify_func, notify_baton,
cancel_func, cancel_baton, NULL, NULL,
NULL, NULL,
diff3_cmd, NULL, editor, edit_baton,
traversal_info, pool);
}
svn_error_t *
svn_wc_get_update_editor(svn_revnum_t *target_revision,
svn_wc_adm_access_t *anchor,
const char *target,
svn_boolean_t use_commit_times,
svn_boolean_t recurse,
svn_wc_notify_func_t notify_func,
void *notify_baton,
svn_cancel_func_t cancel_func,
void *cancel_baton,
const char *diff3_cmd,
const svn_delta_editor_t **editor,
void **edit_baton,
svn_wc_traversal_info_t *traversal_info,
apr_pool_t *pool) {
svn_wc__compat_notify_baton_t *nb = apr_palloc(pool, sizeof(*nb));
nb->func = notify_func;
nb->baton = notify_baton;
return svn_wc_get_update_editor3(target_revision, anchor, target,
use_commit_times,
SVN_DEPTH_INFINITY_OR_FILES(recurse), FALSE,
FALSE, svn_wc__compat_call_notify_func, nb,
cancel_func, cancel_baton, NULL, NULL,
NULL, NULL,
diff3_cmd, NULL, editor, edit_baton,
traversal_info, pool);
}
svn_error_t *
svn_wc_get_switch_editor3(svn_revnum_t *target_revision,
svn_wc_adm_access_t *anchor,
const char *target,
const char *switch_url,
svn_boolean_t use_commit_times,
svn_depth_t depth,
svn_boolean_t depth_is_sticky,
svn_boolean_t allow_unver_obstructions,
svn_wc_notify_func2_t notify_func,
void *notify_baton,
svn_cancel_func_t cancel_func,
void *cancel_baton,
svn_wc_conflict_resolver_func_t conflict_func,
void *conflict_baton,
const char *diff3_cmd,
apr_array_header_t *preserved_exts,
const svn_delta_editor_t **editor,
void **edit_baton,
svn_wc_traversal_info_t *traversal_info,
apr_pool_t *pool) {
assert(switch_url);
return make_editor(target_revision, anchor, svn_wc_adm_access_path(anchor),
target, use_commit_times, switch_url,
depth, depth_is_sticky, allow_unver_obstructions,
notify_func, notify_baton, cancel_func, cancel_baton,
conflict_func, conflict_baton,
NULL, NULL,
diff3_cmd, preserved_exts,
editor, edit_baton, traversal_info, pool);
}
svn_error_t *
svn_wc_get_switch_editor2(svn_revnum_t *target_revision,
svn_wc_adm_access_t *anchor,
const char *target,
const char *switch_url,
svn_boolean_t use_commit_times,
svn_boolean_t recurse,
svn_wc_notify_func2_t notify_func,
void *notify_baton,
svn_cancel_func_t cancel_func,
void *cancel_baton,
const char *diff3_cmd,
const svn_delta_editor_t **editor,
void **edit_baton,
svn_wc_traversal_info_t *traversal_info,
apr_pool_t *pool) {
assert(switch_url);
return svn_wc_get_switch_editor3(target_revision, anchor, target,
switch_url, use_commit_times,
SVN_DEPTH_INFINITY_OR_FILES(recurse), FALSE,
FALSE, notify_func, notify_baton,
cancel_func, cancel_baton,
NULL, NULL, diff3_cmd,
NULL, editor, edit_baton, traversal_info,
pool);
}
svn_error_t *
svn_wc_get_switch_editor(svn_revnum_t *target_revision,
svn_wc_adm_access_t *anchor,
const char *target,
const char *switch_url,
svn_boolean_t use_commit_times,
svn_boolean_t recurse,
svn_wc_notify_func_t notify_func,
void *notify_baton,
svn_cancel_func_t cancel_func,
void *cancel_baton,
const char *diff3_cmd,
const svn_delta_editor_t **editor,
void **edit_baton,
svn_wc_traversal_info_t *traversal_info,
apr_pool_t *pool) {
svn_wc__compat_notify_baton_t *nb = apr_palloc(pool, sizeof(*nb));
nb->func = notify_func;
nb->baton = notify_baton;
return svn_wc_get_switch_editor3(target_revision, anchor, target,
switch_url, use_commit_times,
SVN_DEPTH_INFINITY_OR_FILES(recurse), FALSE,
FALSE, svn_wc__compat_call_notify_func, nb,
cancel_func, cancel_baton,
NULL, NULL, diff3_cmd,
NULL, editor, edit_baton, traversal_info,
pool);
}
svn_wc_traversal_info_t *
svn_wc_init_traversal_info(apr_pool_t *pool) {
svn_wc_traversal_info_t *ti = apr_palloc(pool, sizeof(*ti));
ti->pool = pool;
ti->externals_old = apr_hash_make(pool);
ti->externals_new = apr_hash_make(pool);
ti->depths = apr_hash_make(pool);
return ti;
}
void
svn_wc_edited_externals(apr_hash_t **externals_old,
apr_hash_t **externals_new,
svn_wc_traversal_info_t *traversal_info) {
*externals_old = traversal_info->externals_old;
*externals_new = traversal_info->externals_new;
}
void
svn_wc_traversed_depths(apr_hash_t **depths,
svn_wc_traversal_info_t *traversal_info) {
*depths = traversal_info->depths;
}
static svn_error_t *
check_wc_root(svn_boolean_t *wc_root,
svn_node_kind_t *kind,
const char *path,
svn_wc_adm_access_t *adm_access,
apr_pool_t *pool) {
const char *parent, *base_name;
const svn_wc_entry_t *p_entry, *entry;
svn_error_t *err;
svn_wc_adm_access_t *p_access;
*wc_root = TRUE;
SVN_ERR(svn_wc_entry(&entry, path, adm_access, FALSE, pool));
if (kind)
*kind = entry ? entry->kind : svn_node_file;
if (svn_path_is_empty(path))
return SVN_NO_ERROR;
if (svn_dirent_is_root(path, strlen(path)))
return SVN_NO_ERROR;
p_entry = NULL;
svn_path_split(path, &parent, &base_name, pool);
SVN_ERR(svn_wc__adm_retrieve_internal(&p_access, adm_access, parent,
pool));
err = SVN_NO_ERROR;
if (! p_access)
err = svn_wc_adm_probe_open3(&p_access, NULL, parent, FALSE, 0,
NULL, NULL, pool);
if (! err)
err = svn_wc_entry(&p_entry, parent, p_access, FALSE, pool);
if (err || (! p_entry)) {
svn_error_clear(err);
return SVN_NO_ERROR;
}
if (! p_entry->url)
return svn_error_createf
(SVN_ERR_ENTRY_MISSING_URL, NULL,
_("'%s' has no ancestry information"),
svn_path_local_style(parent, pool));
if (entry && entry->url
&& (strcmp(svn_path_url_add_component(p_entry->url, base_name, pool),
entry->url) != 0))
return SVN_NO_ERROR;
SVN_ERR(svn_wc_entry(&p_entry, path, p_access, FALSE, pool));
if (! p_entry)
return SVN_NO_ERROR;
*wc_root = FALSE;
return SVN_NO_ERROR;
}
svn_error_t *
svn_wc_is_wc_root(svn_boolean_t *wc_root,
const char *path,
svn_wc_adm_access_t *adm_access,
apr_pool_t *pool) {
return check_wc_root(wc_root, NULL, path, adm_access, pool);
}
svn_error_t *
svn_wc_get_actual_target(const char *path,
const char **anchor,
const char **target,
apr_pool_t *pool) {
svn_wc_adm_access_t *adm_access;
svn_boolean_t is_wc_root;
svn_node_kind_t kind;
SVN_ERR(svn_wc_adm_probe_open3(&adm_access, NULL, path, FALSE, 0,
NULL, NULL, pool));
SVN_ERR(check_wc_root(&is_wc_root, &kind, path, adm_access, pool));
SVN_ERR(svn_wc_adm_close(adm_access));
if ((! is_wc_root) || (kind == svn_node_file)) {
svn_path_split(path, anchor, target, pool);
} else {
*anchor = apr_pstrdup(pool, path);
*target = "";
}
return SVN_NO_ERROR;
}
static svn_error_t *
install_added_props(svn_stringbuf_t *log_accum,
svn_wc_adm_access_t *adm_access,
const char *dst_path,
apr_hash_t *new_base_props,
apr_hash_t *new_props,
apr_pool_t *pool) {
apr_array_header_t *regular_props = NULL, *wc_props = NULL,
*entry_props = NULL;
{
apr_array_header_t *prop_array;
int i;
SVN_ERR(svn_prop_diffs(&prop_array, new_base_props,
apr_hash_make(pool), pool));
SVN_ERR(svn_categorize_props(prop_array,
&entry_props, &wc_props, &regular_props,
pool));
new_base_props = apr_hash_make(pool);
for (i = 0; i < regular_props->nelts; ++i) {
const svn_prop_t *prop = &APR_ARRAY_IDX(regular_props, i, svn_prop_t);
apr_hash_set(new_base_props, prop->name, APR_HASH_KEY_STRING,
prop->value);
}
}
SVN_ERR(svn_wc__install_props(&log_accum, adm_access, dst_path,
new_base_props,
new_props ? new_props : new_base_props,
TRUE, pool));
SVN_ERR(accumulate_entry_props(log_accum, NULL,
adm_access, dst_path,
entry_props, pool));
SVN_ERR(accumulate_wcprops(log_accum, adm_access,
dst_path, wc_props, pool));
return SVN_NO_ERROR;
}
svn_error_t *
svn_wc_add_repos_file2(const char *dst_path,
svn_wc_adm_access_t *adm_access,
const char *new_text_base_path,
const char *new_text_path,
apr_hash_t *new_base_props,
apr_hash_t *new_props,
const char *copyfrom_url,
svn_revnum_t copyfrom_rev,
apr_pool_t *pool) {
const char *new_URL;
const char *adm_path = svn_wc_adm_access_path(adm_access);
const char *tmp_text_base_path =
svn_wc__text_base_path(dst_path, TRUE, pool);
const char *text_base_path =
svn_wc__text_base_path(dst_path, FALSE, pool);
const svn_wc_entry_t *ent;
const svn_wc_entry_t *dst_entry;
svn_stringbuf_t *log_accum;
const char *dir_name, *base_name;
svn_path_split(dst_path, &dir_name, &base_name, pool);
{
SVN_ERR(svn_wc__entry_versioned(&ent, dir_name, adm_access, FALSE, pool));
new_URL = svn_path_url_add_component(ent->url, base_name, pool);
if (copyfrom_url && ent->repos &&
! svn_path_is_ancestor(ent->repos, copyfrom_url))
return svn_error_createf(SVN_ERR_UNSUPPORTED_FEATURE, NULL,
_("Copyfrom-url '%s' has different repository"
" root than '%s'"),
copyfrom_url, ent->repos);
}
log_accum = svn_stringbuf_create("", pool);
SVN_ERR(svn_wc_entry(&dst_entry, dst_path, adm_access, FALSE, pool));
if (dst_entry && dst_entry->schedule == svn_wc_schedule_delete) {
const char *dst_rtext = svn_wc__text_revert_path(dst_path, FALSE,
pool);
const char *dst_txtb = svn_wc__text_base_path(dst_path, FALSE, pool);
SVN_ERR(svn_wc__loggy_move(&log_accum, NULL,
adm_access, dst_txtb, dst_rtext,
FALSE, pool));
SVN_ERR(svn_wc__loggy_revert_props_create(&log_accum,
dst_path, adm_access,
TRUE, pool));
}
{
svn_wc_entry_t tmp_entry;
apr_uint64_t modify_flags = SVN_WC__ENTRY_MODIFY_SCHEDULE;
tmp_entry.schedule = svn_wc_schedule_add;
if (copyfrom_url) {
assert(SVN_IS_VALID_REVNUM(copyfrom_rev));
tmp_entry.copyfrom_url = copyfrom_url;
tmp_entry.copyfrom_rev = copyfrom_rev;
tmp_entry.copied = TRUE;
modify_flags |= SVN_WC__ENTRY_MODIFY_COPYFROM_URL
| SVN_WC__ENTRY_MODIFY_COPYFROM_REV
| SVN_WC__ENTRY_MODIFY_COPIED;
}
SVN_ERR(svn_wc__loggy_entry_modify(&log_accum, adm_access,
dst_path, &tmp_entry,
modify_flags, pool));
}
SVN_ERR(loggy_tweak_entry(log_accum, adm_access, dst_path,
dst_entry ? dst_entry->revision : ent->revision,
new_URL, pool));
SVN_ERR(install_added_props(log_accum, adm_access, dst_path,
new_base_props, new_props, pool));
if (strcmp(tmp_text_base_path, new_text_base_path) != 0)
SVN_ERR(svn_io_file_move(new_text_base_path, tmp_text_base_path,
pool));
if (new_text_path) {
const char *tmp_text_path;
SVN_ERR(svn_wc_create_tmp_file2(NULL, &tmp_text_path, adm_path,
svn_io_file_del_none, pool));
SVN_ERR(svn_io_file_move(new_text_path, tmp_text_path, pool));
if (svn_wc__has_special_property(new_base_props)) {
SVN_ERR(svn_wc__loggy_copy(&log_accum, NULL, adm_access,
svn_wc__copy_translate_special_only,
tmp_text_path,
dst_path, FALSE, pool));
SVN_ERR(svn_wc__loggy_remove(&log_accum, adm_access,
tmp_text_path, pool));
} else
SVN_ERR(svn_wc__loggy_move(&log_accum, NULL, adm_access,
tmp_text_path, dst_path,
FALSE, pool));
SVN_ERR(svn_wc__loggy_maybe_set_readonly(&log_accum, adm_access,
dst_path, pool));
} else {
SVN_ERR(svn_wc__loggy_copy(&log_accum, NULL, adm_access,
svn_wc__copy_translate,
tmp_text_base_path, dst_path, FALSE,
pool));
SVN_ERR(svn_wc__loggy_set_entry_timestamp_from_wc
(&log_accum, adm_access,
dst_path, SVN_WC__ENTRY_ATTR_TEXT_TIME, pool));
SVN_ERR(svn_wc__loggy_set_entry_working_size_from_wc
(&log_accum, adm_access, dst_path, pool));
}
{
unsigned char digest[APR_MD5_DIGESTSIZE];
svn_wc_entry_t tmp_entry;
SVN_ERR(svn_wc__loggy_move(&log_accum, NULL,
adm_access, tmp_text_base_path,
text_base_path, FALSE, pool));
SVN_ERR(svn_wc__loggy_set_readonly(&log_accum, adm_access,
text_base_path, pool));
SVN_ERR(svn_io_file_checksum(digest, tmp_text_base_path, pool));
tmp_entry.checksum = svn_md5_digest_to_cstring(digest, pool);
SVN_ERR(svn_wc__loggy_entry_modify(&log_accum, adm_access,
dst_path, &tmp_entry,
SVN_WC__ENTRY_MODIFY_CHECKSUM,
pool));
}
SVN_ERR(svn_wc__write_log(adm_access, 0, log_accum, pool));
SVN_ERR(svn_wc__run_log(adm_access, NULL, pool));
return SVN_NO_ERROR;
}
svn_error_t *
svn_wc_add_repos_file(const char *dst_path,
svn_wc_adm_access_t *adm_access,
const char *new_text_path,
apr_hash_t *new_props,
const char *copyfrom_url,
svn_revnum_t copyfrom_rev,
apr_pool_t *pool) {
return svn_wc_add_repos_file2(dst_path, adm_access,
new_text_path, NULL,
new_props, NULL,
copyfrom_url, copyfrom_rev,
pool);
}