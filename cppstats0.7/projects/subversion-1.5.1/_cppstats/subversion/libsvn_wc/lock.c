#include <assert.h>
#include <apr_pools.h>
#include <apr_time.h>
#include "svn_pools.h"
#include "svn_path.h"
#include "svn_sorts.h"
#include "svn_types.h"
#include "wc.h"
#include "adm_files.h"
#include "lock.h"
#include "questions.h"
#include "props.h"
#include "log.h"
#include "entries.h"
#include "svn_private_config.h"
#include "private/svn_wc_private.h"
struct svn_wc_adm_access_t {
const char *path;
enum svn_wc__adm_access_type {
svn_wc__adm_access_unlocked,
svn_wc__adm_access_write_lock,
svn_wc__adm_access_closed
} type;
svn_boolean_t lock_exists;
svn_boolean_t set_owner;
int wc_format;
apr_hash_t *set;
apr_hash_t *entries;
apr_hash_t *entries_hidden;
apr_hash_t *wcprops;
apr_pool_t *pool;
};
static svn_wc_adm_access_t missing;
static svn_error_t *
do_close(svn_wc_adm_access_t *adm_access, svn_boolean_t preserve_lock,
svn_boolean_t recurse);
#if !defined(SVN_DISABLE_WC_UPGRADE)
static svn_error_t *
introduce_propcaching(svn_stringbuf_t *log_accum,
svn_wc_adm_access_t *adm_access,
apr_pool_t *pool) {
apr_hash_t *entries;
apr_hash_index_t *hi;
apr_pool_t *subpool = svn_pool_create(pool);
const char *adm_path = svn_wc_adm_access_path(adm_access);
SVN_ERR(svn_wc_entries_read(&entries, adm_access, FALSE, pool));
for (hi = apr_hash_first(pool, entries); hi; hi = apr_hash_next(hi)) {
void *val;
const svn_wc_entry_t *entry;
const char *entrypath;
svn_wc_entry_t tmpentry;
apr_hash_t *base_props, *props;
apr_hash_this(hi, NULL, NULL, &val);
entry = val;
if (entry->kind != svn_node_file
&& strcmp(entry->name, SVN_WC_ENTRY_THIS_DIR) != 0)
continue;
svn_pool_clear(subpool);
entrypath = svn_path_join(adm_path, entry->name, subpool);
SVN_ERR(svn_wc__load_props(&base_props, &props, NULL, adm_access,
entrypath, subpool));
SVN_ERR(svn_wc__install_props(&log_accum, adm_access, entrypath,
base_props, props, TRUE, subpool));
tmpentry.prop_time = 0;
SVN_ERR(svn_wc__loggy_entry_modify
(&log_accum, adm_access,
entrypath,
&tmpentry,
SVN_WC__ENTRY_MODIFY_PROP_TIME,
subpool));
}
return SVN_NO_ERROR;
}
static svn_error_t *
convert_wcprops(svn_stringbuf_t *log_accum,
svn_wc_adm_access_t *adm_access,
apr_pool_t *pool) {
apr_hash_t *entries;
apr_hash_index_t *hi;
apr_pool_t *subpool = svn_pool_create(pool);
SVN_ERR(svn_wc_entries_read(&entries, adm_access, FALSE, pool));
for (hi = apr_hash_first(pool, entries); hi; hi = apr_hash_next(hi)) {
void *val;
const svn_wc_entry_t *entry;
apr_hash_t *wcprops;
apr_hash_index_t *hj;
const char *full_path;
apr_hash_this(hi, NULL, NULL, &val);
entry = val;
full_path = svn_path_join(svn_wc_adm_access_path(adm_access),
entry->name, pool);
if (entry->kind != svn_node_file
&& strcmp(entry->name, SVN_WC_ENTRY_THIS_DIR) != 0)
continue;
svn_pool_clear(subpool);
SVN_ERR(svn_wc__wcprop_list(&wcprops, entry->name, adm_access, subpool));
for (hj = apr_hash_first(subpool, wcprops); hj; hj = apr_hash_next(hj)) {
const void *key2;
void *val2;
const char *propname;
svn_string_t *propval;
apr_hash_this(hj, &key2, NULL, &val2);
propname = key2;
propval = val2;
SVN_ERR(svn_wc__loggy_modify_wcprop(&log_accum, adm_access,
full_path, propname,
propval->data,
subpool));
}
}
return SVN_NO_ERROR;
}
static svn_error_t *
maybe_upgrade_format(svn_wc_adm_access_t *adm_access, apr_pool_t *pool) {
SVN_ERR(svn_wc__check_format(adm_access->wc_format,
adm_access->path,
pool));
if (adm_access->wc_format != SVN_WC__VERSION) {
svn_boolean_t cleanup_required;
svn_stringbuf_t *log_accum = svn_stringbuf_create("", pool);
SVN_ERR(svn_wc__adm_is_cleanup_required(&cleanup_required,
adm_access, pool));
if (cleanup_required)
return SVN_NO_ERROR;
SVN_ERR(svn_wc__loggy_upgrade_format(&log_accum, adm_access,
SVN_WC__VERSION, pool));
if (adm_access->wc_format <= SVN_WC__NO_PROPCACHING_VERSION)
SVN_ERR(introduce_propcaching(log_accum, adm_access, pool));
if (adm_access->wc_format <= SVN_WC__WCPROPS_MANY_FILES_VERSION)
SVN_ERR(convert_wcprops(log_accum, adm_access, pool));
SVN_ERR(svn_wc__write_log(adm_access, 0, log_accum, pool));
if (adm_access->wc_format <= SVN_WC__WCPROPS_MANY_FILES_VERSION) {
const char *access_path = svn_wc_adm_access_path(adm_access);
svn_error_clear(svn_io_remove_dir2
(svn_wc__adm_path(access_path, FALSE, pool, SVN_WC__ADM_WCPROPS,
NULL), FALSE, NULL, NULL, pool));
svn_error_clear(svn_io_remove_file
(svn_wc__adm_path(access_path, FALSE, pool,
SVN_WC__ADM_DIR_WCPROPS, NULL), pool));
svn_error_clear(svn_io_remove_file
(svn_wc__adm_path(access_path, FALSE, pool,
SVN_WC__ADM_EMPTY_FILE, NULL), pool));
svn_error_clear(svn_io_remove_file
(svn_wc__adm_path(access_path, FALSE, pool,
SVN_WC__ADM_README, NULL), pool));
}
SVN_ERR(svn_wc__run_log(adm_access, NULL, pool));
}
return SVN_NO_ERROR;
}
#else
static svn_error_t *
maybe_upgrade_format(svn_wc_adm_access_t *adm_access, apr_pool_t *pool) {
SVN_ERR(svn_wc__check_format(adm_access->wc_format,
adm_access->path,
pool));
if (adm_access->wc_format != SVN_WC__VERSION) {
return svn_error_createf(SVN_ERR_WC_UNSUPPORTED_FORMAT, NULL,
"Would upgrade working copy '%s' from old "
"format (%d) to current format (%d), "
"but automatic upgrade has been disabled",
svn_path_local_style(adm_access->path, pool),
adm_access->wc_format, SVN_WC__VERSION);
}
return SVN_NO_ERROR;
}
#endif
static svn_error_t *
create_lock(svn_wc_adm_access_t *adm_access, int wait_for, apr_pool_t *pool) {
svn_error_t *err;
for (;;) {
err = svn_wc__make_adm_thing(adm_access, SVN_WC__ADM_LOCK,
svn_node_file, APR_OS_DEFAULT, 0, pool);
if (err) {
if (APR_STATUS_IS_EEXIST(err->apr_err)) {
svn_error_clear(err);
if (wait_for <= 0)
break;
wait_for--;
apr_sleep(apr_time_from_sec(1));
} else
return err;
} else
return SVN_NO_ERROR;
}
return svn_error_createf(SVN_ERR_WC_LOCKED, NULL,
_("Working copy '%s' locked"),
svn_path_local_style(adm_access->path, pool));
}
static svn_error_t *
remove_lock(const char *path, apr_pool_t *pool) {
svn_error_t *err = svn_wc__remove_adm_file(path, pool, SVN_WC__ADM_LOCK,
NULL);
if (err) {
if (svn_wc__adm_path_exists(path, FALSE, pool, NULL))
return err;
svn_error_clear(err);
}
return SVN_NO_ERROR;
}
static apr_status_t
pool_cleanup(void *p) {
svn_wc_adm_access_t *lock = p;
svn_boolean_t cleanup;
svn_error_t *err;
if (lock->type == svn_wc__adm_access_closed)
return SVN_NO_ERROR;
err = svn_wc__adm_is_cleanup_required(&cleanup, lock, lock->pool);
if (!err)
err = do_close(lock, cleanup, TRUE);
if (err) {
apr_status_t apr_err = err->apr_err;
svn_error_clear(err);
return apr_err;
} else
return APR_SUCCESS;
}
static apr_status_t
pool_cleanup_child(void *p) {
svn_wc_adm_access_t *lock = p;
apr_pool_cleanup_kill(lock->pool, lock, pool_cleanup);
return APR_SUCCESS;
}
static svn_wc_adm_access_t *
adm_access_alloc(enum svn_wc__adm_access_type type,
const char *path,
apr_pool_t *pool) {
svn_wc_adm_access_t *lock = apr_palloc(pool, sizeof(*lock));
lock->type = type;
lock->entries = NULL;
lock->entries_hidden = NULL;
lock->wcprops = NULL;
lock->wc_format = 0;
lock->set = NULL;
lock->lock_exists = FALSE;
lock->set_owner = FALSE;
lock->path = apr_pstrdup(pool, path);
lock->pool = pool;
return lock;
}
static void
adm_ensure_set(svn_wc_adm_access_t *adm_access) {
if (! adm_access->set) {
adm_access->set_owner = TRUE;
adm_access->set = apr_hash_make(adm_access->pool);
apr_hash_set(adm_access->set, adm_access->path, APR_HASH_KEY_STRING,
adm_access);
}
}
static svn_error_t *
probe(const char **dir,
const char *path,
int *wc_format,
apr_pool_t *pool) {
svn_node_kind_t kind;
SVN_ERR(svn_io_check_path(path, &kind, pool));
if (kind == svn_node_dir)
SVN_ERR(svn_wc_check_wc(path, wc_format, pool));
else
*wc_format = 0;
if (kind != svn_node_dir || *wc_format == 0) {
const char *base_name = svn_path_basename(path, pool);
if ((strcmp(base_name, "..") == 0)
|| (strcmp(base_name, ".") == 0)) {
return svn_error_createf
(SVN_ERR_WC_BAD_PATH, NULL,
_("Path '%s' ends in '%s', "
"which is unsupported for this operation"),
svn_path_local_style(path, pool), base_name);
}
*dir = svn_path_dirname(path, pool);
} else
*dir = path;
return SVN_NO_ERROR;
}
svn_error_t *
svn_wc__adm_steal_write_lock(svn_wc_adm_access_t **adm_access,
svn_wc_adm_access_t *associated,
const char *path,
apr_pool_t *pool) {
svn_error_t *err;
svn_wc_adm_access_t *lock = adm_access_alloc(svn_wc__adm_access_write_lock,
path, pool);
err = create_lock(lock, 0, pool);
if (err) {
if (err->apr_err == SVN_ERR_WC_LOCKED)
svn_error_clear(err);
else
return err;
}
if (associated) {
adm_ensure_set(associated);
lock->set = associated->set;
apr_hash_set(lock->set, lock->path, APR_HASH_KEY_STRING, lock);
}
SVN_ERR(svn_wc_check_wc(path, &lock->wc_format, pool));
SVN_ERR(maybe_upgrade_format(lock, pool));
lock->lock_exists = TRUE;
*adm_access = lock;
return SVN_NO_ERROR;
}
static svn_error_t *
do_open(svn_wc_adm_access_t **adm_access,
svn_wc_adm_access_t *associated,
const char *path,
svn_boolean_t write_lock,
int levels_to_lock,
svn_boolean_t under_construction,
svn_cancel_func_t cancel_func,
void *cancel_baton,
apr_pool_t *pool) {
svn_wc_adm_access_t *lock;
int wc_format;
svn_error_t *err;
apr_pool_t *subpool = svn_pool_create(pool);
if (associated) {
adm_ensure_set(associated);
lock = apr_hash_get(associated->set, path, APR_HASH_KEY_STRING);
if (lock && lock != &missing)
return svn_error_createf(SVN_ERR_WC_LOCKED, NULL,
_("Working copy '%s' locked"),
svn_path_local_style(path, pool));
}
if (! under_construction) {
err = svn_io_read_version_file(&wc_format,
svn_wc__adm_path(path, FALSE, subpool,
SVN_WC__ADM_ENTRIES,
NULL),
subpool);
if (err && err->apr_err == SVN_ERR_BAD_VERSION_FILE_FORMAT) {
svn_error_clear(err);
err = svn_io_read_version_file(&wc_format,
svn_wc__adm_path(path, FALSE, subpool,
SVN_WC__ADM_FORMAT,
NULL),
subpool);
}
if (err) {
return svn_error_createf(SVN_ERR_WC_NOT_DIRECTORY, err,
_("'%s' is not a working copy"),
svn_path_local_style(path, pool));
}
SVN_ERR(svn_wc__check_format(wc_format,
svn_path_local_style(path, subpool),
subpool));
}
if (write_lock) {
lock = adm_access_alloc(svn_wc__adm_access_write_lock, path, pool);
SVN_ERR(create_lock(lock, 0, subpool));
lock->lock_exists = TRUE;
} else {
lock = adm_access_alloc(svn_wc__adm_access_unlocked, path, pool);
}
if (! under_construction) {
lock->wc_format = wc_format;
if (write_lock)
SVN_ERR(maybe_upgrade_format(lock, subpool));
}
if (levels_to_lock != 0) {
apr_hash_t *entries;
apr_hash_index_t *hi;
if (levels_to_lock > 0)
levels_to_lock--;
SVN_ERR(svn_wc_entries_read(&entries, lock, FALSE, subpool));
if (associated)
lock->set = apr_hash_make(subpool);
for (hi = apr_hash_first(subpool, entries); hi; hi = apr_hash_next(hi)) {
void *val;
const svn_wc_entry_t *entry;
svn_wc_adm_access_t *entry_access;
const char *entry_path;
if (cancel_func) {
err = cancel_func(cancel_baton);
if (err) {
svn_error_clear(svn_wc_adm_close(lock));
svn_pool_destroy(subpool);
lock->set = NULL;
return err;
}
}
apr_hash_this(hi, NULL, NULL, &val);
entry = val;
if (entry->kind != svn_node_dir
|| ! strcmp(entry->name, SVN_WC_ENTRY_THIS_DIR))
continue;
entry_path = svn_path_join(lock->path, entry->name, subpool);
err = do_open(&entry_access, lock, entry_path, write_lock,
levels_to_lock, FALSE, cancel_func, cancel_baton,
lock->pool);
if (err) {
if (err->apr_err != SVN_ERR_WC_NOT_DIRECTORY) {
svn_error_clear(svn_wc_adm_close(lock));
svn_pool_destroy(subpool);
lock->set = NULL;
return err;
}
svn_error_clear(err);
adm_ensure_set(lock);
apr_hash_set(lock->set, apr_pstrdup(lock->pool, entry_path),
APR_HASH_KEY_STRING, &missing);
continue;
}
}
if (associated) {
for (hi = apr_hash_first(subpool, lock->set);
hi;
hi = apr_hash_next(hi)) {
const void *key;
void *val;
const char *entry_path;
svn_wc_adm_access_t *entry_access;
apr_hash_this(hi, &key, NULL, &val);
entry_path = key;
entry_access = val;
apr_hash_set(associated->set, entry_path, APR_HASH_KEY_STRING,
entry_access);
entry_access->set = associated->set;
}
lock->set = associated->set;
}
}
if (associated) {
lock->set = associated->set;
apr_hash_set(lock->set, lock->path, APR_HASH_KEY_STRING, lock);
}
apr_pool_cleanup_register(lock->pool, lock, pool_cleanup,
pool_cleanup_child);
*adm_access = lock;
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
svn_error_t *
svn_wc_adm_open(svn_wc_adm_access_t **adm_access,
svn_wc_adm_access_t *associated,
const char *path,
svn_boolean_t write_lock,
svn_boolean_t tree_lock,
apr_pool_t *pool) {
return svn_wc_adm_open3(adm_access, associated, path, write_lock,
(tree_lock ? -1 : 0), NULL, NULL, pool);
}
svn_error_t *
svn_wc_adm_open2(svn_wc_adm_access_t **adm_access,
svn_wc_adm_access_t *associated,
const char *path,
svn_boolean_t write_lock,
int levels_to_lock,
apr_pool_t *pool) {
return svn_wc_adm_open3(adm_access, associated, path, write_lock,
levels_to_lock, NULL, NULL, pool);
}
svn_error_t *
svn_wc_adm_open3(svn_wc_adm_access_t **adm_access,
svn_wc_adm_access_t *associated,
const char *path,
svn_boolean_t write_lock,
int levels_to_lock,
svn_cancel_func_t cancel_func,
void *cancel_baton,
apr_pool_t *pool) {
return do_open(adm_access, associated, path, write_lock, levels_to_lock,
FALSE, cancel_func, cancel_baton, pool);
}
svn_error_t *
svn_wc__adm_pre_open(svn_wc_adm_access_t **adm_access,
const char *path,
apr_pool_t *pool) {
return do_open(adm_access, NULL, path, TRUE, 0, TRUE, NULL, NULL, pool);
}
svn_error_t *
svn_wc_adm_probe_open(svn_wc_adm_access_t **adm_access,
svn_wc_adm_access_t *associated,
const char *path,
svn_boolean_t write_lock,
svn_boolean_t tree_lock,
apr_pool_t *pool) {
return svn_wc_adm_probe_open3(adm_access, associated, path,
write_lock, (tree_lock ? -1 : 0),
NULL, NULL, pool);
}
svn_error_t *
svn_wc_adm_probe_open2(svn_wc_adm_access_t **adm_access,
svn_wc_adm_access_t *associated,
const char *path,
svn_boolean_t write_lock,
int levels_to_lock,
apr_pool_t *pool) {
return svn_wc_adm_probe_open3(adm_access, associated, path, write_lock,
levels_to_lock, NULL, NULL, pool);
}
svn_error_t *
svn_wc_adm_probe_open3(svn_wc_adm_access_t **adm_access,
svn_wc_adm_access_t *associated,
const char *path,
svn_boolean_t write_lock,
int levels_to_lock,
svn_cancel_func_t cancel_func,
void *cancel_baton,
apr_pool_t *pool) {
svn_error_t *err;
const char *dir;
int wc_format;
SVN_ERR(probe(&dir, path, &wc_format, pool));
if (dir != path)
levels_to_lock = 0;
err = svn_wc_adm_open3(adm_access, associated, dir, write_lock,
levels_to_lock, cancel_func, cancel_baton, pool);
if (err) {
svn_error_t *err2;
svn_node_kind_t child_kind;
if ((err2 = svn_io_check_path(path, &child_kind, pool))) {
svn_error_compose(err, err2);
return err;
}
if ((dir != path)
&& (child_kind == svn_node_dir)
&& (err->apr_err == SVN_ERR_WC_NOT_DIRECTORY)) {
svn_error_clear(err);
return svn_error_createf(SVN_ERR_WC_NOT_DIRECTORY, NULL,
_("'%s' is not a working copy"),
svn_path_local_style(path, pool));
} else {
return err;
}
}
if (wc_format && ! (*adm_access)->wc_format)
(*adm_access)->wc_format = wc_format;
return SVN_NO_ERROR;
}
svn_error_t *
svn_wc__adm_retrieve_internal(svn_wc_adm_access_t **adm_access,
svn_wc_adm_access_t *associated,
const char *path,
apr_pool_t *pool) {
if (associated->set)
*adm_access = apr_hash_get(associated->set, path, APR_HASH_KEY_STRING);
else if (! strcmp(associated->path, path))
*adm_access = associated;
else
*adm_access = NULL;
if (*adm_access == &missing)
*adm_access = NULL;
return SVN_NO_ERROR;
}
svn_error_t *
svn_wc_adm_retrieve(svn_wc_adm_access_t **adm_access,
svn_wc_adm_access_t *associated,
const char *path,
apr_pool_t *pool) {
SVN_ERR(svn_wc__adm_retrieve_internal(adm_access, associated, path, pool));
if (! *adm_access) {
const char *wcpath;
const svn_wc_entry_t *subdir_entry;
svn_node_kind_t wckind;
svn_node_kind_t kind;
svn_error_t *err;
err = svn_wc_entry(&subdir_entry, path, associated, TRUE, pool);
if (err) {
svn_error_clear(err);
subdir_entry = NULL;
}
err = svn_io_check_path(path, &kind, pool);
if (err) {
return svn_error_createf(SVN_ERR_WC_NOT_LOCKED, err,
_("Unable to check path existence for '%s'"),
svn_path_local_style(path, pool));
}
if (subdir_entry) {
if (subdir_entry->kind == svn_node_dir
&& kind == svn_node_file) {
const char *err_msg = apr_psprintf
(pool, _("Expected '%s' to be a directory but found a file"),
svn_path_local_style(path, pool));
return svn_error_create(SVN_ERR_WC_NOT_LOCKED,
svn_error_create
(SVN_ERR_WC_NOT_DIRECTORY, NULL,
err_msg),
err_msg);
} else if (subdir_entry->kind == svn_node_file
&& kind == svn_node_dir) {
const char *err_msg = apr_psprintf
(pool, _("Expected '%s' to be a file but found a directory"),
svn_path_local_style(path, pool));
return svn_error_create(SVN_ERR_WC_NOT_LOCKED,
svn_error_create(SVN_ERR_WC_NOT_FILE,
NULL, err_msg),
err_msg);
}
}
wcpath = svn_wc__adm_path(path, FALSE, pool, NULL);
err = svn_io_check_path(wcpath, &wckind, pool);
if (err) {
return svn_error_createf(SVN_ERR_WC_NOT_LOCKED, err,
_("Unable to check path existence for '%s'"),
svn_path_local_style(wcpath, pool));
}
if (kind == svn_node_none) {
const char *err_msg = apr_psprintf(pool,
_("Directory '%s' is missing"),
svn_path_local_style(path, pool));
return svn_error_create(SVN_ERR_WC_NOT_LOCKED,
svn_error_create(SVN_ERR_WC_PATH_NOT_FOUND,
NULL, err_msg),
err_msg);
}
else if (kind == svn_node_dir && wckind == svn_node_none)
return svn_error_createf(SVN_ERR_WC_NOT_LOCKED, NULL,
_("Directory '%s' containing working copy admin area is missing"),
svn_path_local_style(wcpath, pool));
else if (kind == svn_node_dir && wckind == svn_node_dir)
return svn_error_createf(SVN_ERR_WC_NOT_LOCKED, NULL,
_("Unable to lock '%s'"),
svn_path_local_style(path, pool));
return svn_error_createf(SVN_ERR_WC_NOT_LOCKED, NULL,
_("Working copy '%s' is not locked"),
svn_path_local_style(path, pool));
}
return SVN_NO_ERROR;
}
svn_error_t *
svn_wc_adm_probe_retrieve(svn_wc_adm_access_t **adm_access,
svn_wc_adm_access_t *associated,
const char *path,
apr_pool_t *pool) {
const char *dir;
const svn_wc_entry_t *entry;
int wc_format;
svn_error_t *err;
SVN_ERR(svn_wc_entry(&entry, path, associated, TRUE, pool));
if (! entry)
SVN_ERR(probe(&dir, path, &wc_format, pool));
else if (entry->kind != svn_node_dir)
dir = svn_path_dirname(path, pool);
else
dir = path;
err = svn_wc_adm_retrieve(adm_access, associated, dir, pool);
if (err && err->apr_err == SVN_ERR_WC_NOT_LOCKED) {
svn_error_clear(err);
SVN_ERR(probe(&dir, path, &wc_format, pool));
SVN_ERR(svn_wc_adm_retrieve(adm_access, associated, dir, pool));
} else
return err;
return SVN_NO_ERROR;
}
svn_error_t *
svn_wc_adm_probe_try(svn_wc_adm_access_t **adm_access,
svn_wc_adm_access_t *associated,
const char *path,
svn_boolean_t write_lock,
svn_boolean_t tree_lock,
apr_pool_t *pool) {
return svn_wc_adm_probe_try3(adm_access, associated, path, write_lock,
(tree_lock ? -1 : 0), NULL, NULL, pool);
}
svn_error_t *
svn_wc_adm_probe_try2(svn_wc_adm_access_t **adm_access,
svn_wc_adm_access_t *associated,
const char *path,
svn_boolean_t write_lock,
int levels_to_lock,
apr_pool_t *pool) {
return svn_wc_adm_probe_try3(adm_access, associated, path, write_lock,
levels_to_lock, NULL, NULL, pool);
}
svn_error_t *
svn_wc_adm_probe_try3(svn_wc_adm_access_t **adm_access,
svn_wc_adm_access_t *associated,
const char *path,
svn_boolean_t write_lock,
int levels_to_lock,
svn_cancel_func_t cancel_func,
void *cancel_baton,
apr_pool_t *pool) {
svn_error_t *err;
err = svn_wc_adm_probe_retrieve(adm_access, associated, path, pool);
if (err && (err->apr_err == SVN_ERR_WC_NOT_LOCKED)) {
svn_error_clear(err);
err = svn_wc_adm_probe_open3(adm_access, associated,
path, write_lock, levels_to_lock,
cancel_func, cancel_baton,
svn_wc_adm_access_pool(associated));
if (err && (err->apr_err == SVN_ERR_WC_NOT_DIRECTORY)) {
svn_error_clear(err);
*adm_access = NULL;
err = NULL;
}
}
return err;
}
static void join_batons(svn_wc_adm_access_t *p_access,
svn_wc_adm_access_t *t_access,
apr_pool_t *pool) {
apr_hash_index_t *hi;
adm_ensure_set(p_access);
if (! t_access->set) {
t_access->set = p_access->set;
apr_hash_set(p_access->set, t_access->path, APR_HASH_KEY_STRING,
t_access);
return;
}
for (hi = apr_hash_first(pool, t_access->set); hi; hi = apr_hash_next(hi)) {
const void *key;
void *val;
svn_wc_adm_access_t *adm_access;
apr_hash_this(hi, &key, NULL, &val);
adm_access = val;
if (adm_access != &missing)
adm_access->set = p_access->set;
apr_hash_set(p_access->set, key, APR_HASH_KEY_STRING, adm_access);
}
t_access->set_owner = FALSE;
}
svn_error_t *
svn_wc_adm_open_anchor(svn_wc_adm_access_t **anchor_access,
svn_wc_adm_access_t **target_access,
const char **target,
const char *path,
svn_boolean_t write_lock,
int levels_to_lock,
svn_cancel_func_t cancel_func,
void *cancel_baton,
apr_pool_t *pool) {
const char *base_name = svn_path_basename(path, pool);
if (svn_path_is_empty(path)
|| svn_dirent_is_root(path, strlen(path))
|| ! strcmp(base_name, "..")) {
SVN_ERR(do_open(anchor_access, NULL, path, write_lock, levels_to_lock,
FALSE, cancel_func, cancel_baton, pool));
*target_access = *anchor_access;
*target = "";
} else {
svn_error_t *err;
svn_wc_adm_access_t *p_access, *t_access;
const char *parent = svn_path_dirname(path, pool);
svn_error_t *p_access_err = SVN_NO_ERROR;
err = do_open(&p_access, NULL, parent, write_lock, 0, FALSE,
cancel_func, cancel_baton, pool);
if (err) {
if (err->apr_err == SVN_ERR_WC_NOT_DIRECTORY) {
svn_error_clear(err);
p_access = NULL;
} else if (write_lock && (err->apr_err == SVN_ERR_WC_LOCKED
|| APR_STATUS_IS_EACCES(err->apr_err))) {
svn_error_t *err2 = do_open(&p_access, NULL, parent, FALSE, 0,
FALSE, cancel_func, cancel_baton,
pool);
if (err2) {
svn_error_clear(err2);
return err;
}
p_access_err = err;
} else
return err;
}
err = do_open(&t_access, NULL, path, write_lock, levels_to_lock,
FALSE, cancel_func, cancel_baton, pool);
if (err) {
if (! p_access || err->apr_err != SVN_ERR_WC_NOT_DIRECTORY) {
if (p_access)
svn_error_clear(do_close(p_access, FALSE, TRUE));
svn_error_clear(p_access_err);
return err;
}
svn_error_clear(err);
t_access = NULL;
}
if (p_access && t_access) {
const svn_wc_entry_t *t_entry, *p_entry, *t_entry_in_p;
err = svn_wc_entry(&t_entry_in_p, path, p_access, FALSE, pool);
if (! err)
err = svn_wc_entry(&t_entry, path, t_access, FALSE, pool);
if (! err)
err = svn_wc_entry(&p_entry, parent, p_access, FALSE, pool);
if (err) {
svn_error_clear(p_access_err);
svn_error_clear(do_close(p_access, FALSE, TRUE));
svn_error_clear(do_close(t_access, FALSE, TRUE));
return err;
}
if (! t_entry_in_p
||
(p_entry->url && t_entry->url
&& (strcmp(svn_path_dirname(t_entry->url, pool), p_entry->url)
|| strcmp(svn_path_uri_encode(base_name, pool),
svn_path_basename(t_entry->url, pool))))) {
err = do_close(p_access, FALSE, TRUE);
if (err) {
svn_error_clear(p_access_err);
svn_error_clear(do_close(t_access, FALSE, TRUE));
return err;
}
p_access = NULL;
}
}
if (p_access) {
if (p_access_err) {
if (t_access)
svn_error_clear(do_close(t_access, FALSE, TRUE));
svn_error_clear(do_close(p_access, FALSE, TRUE));
return p_access_err;
} else if (t_access)
join_batons(p_access, t_access, pool);
}
svn_error_clear(p_access_err);
if (! t_access) {
const svn_wc_entry_t *t_entry;
err = svn_wc_entry(&t_entry, path, p_access, FALSE, pool);
if (err) {
if (p_access)
svn_error_clear(do_close(p_access, FALSE, TRUE));
return err;
}
if (t_entry && t_entry->kind == svn_node_dir) {
adm_ensure_set(p_access);
apr_hash_set(p_access->set, apr_pstrdup(p_access->pool, path),
APR_HASH_KEY_STRING, &missing);
}
}
*anchor_access = p_access ? p_access : t_access;
*target_access = t_access ? t_access : p_access;
if (! p_access)
*target = "";
else
*target = base_name;
}
return SVN_NO_ERROR;
}
static svn_error_t *
do_close(svn_wc_adm_access_t *adm_access,
svn_boolean_t preserve_lock,
svn_boolean_t recurse) {
if (adm_access->type == svn_wc__adm_access_closed)
return SVN_NO_ERROR;
if (recurse && adm_access->set) {
int i;
apr_array_header_t *children
= svn_sort__hash(adm_access->set, svn_sort_compare_items_as_paths,
adm_access->pool);
for (i = children->nelts - 1; i >= 0; --i) {
svn_sort__item_t *item = &APR_ARRAY_IDX(children, i,
svn_sort__item_t);
const char *path = item->key;
svn_wc_adm_access_t *child = item->value;
if (child == &missing) {
apr_hash_set(adm_access->set, path, APR_HASH_KEY_STRING, NULL);
continue;
}
if (! svn_path_is_ancestor(adm_access->path, path)
|| strcmp(adm_access->path, path) == 0)
continue;
SVN_ERR(do_close(child, preserve_lock, FALSE));
}
}
if (adm_access->type == svn_wc__adm_access_write_lock) {
if (adm_access->lock_exists && ! preserve_lock) {
SVN_ERR(remove_lock(adm_access->path, adm_access->pool));
adm_access->lock_exists = FALSE;
}
}
adm_access->type = svn_wc__adm_access_closed;
if (adm_access->set) {
apr_hash_set(adm_access->set, adm_access->path, APR_HASH_KEY_STRING,
NULL);
assert(! adm_access->set_owner || apr_hash_count(adm_access->set) == 0);
}
return SVN_NO_ERROR;
}
svn_error_t *
svn_wc_adm_close(svn_wc_adm_access_t *adm_access) {
return do_close(adm_access, FALSE, TRUE);
}
svn_boolean_t
svn_wc_adm_locked(svn_wc_adm_access_t *adm_access) {
return adm_access->type == svn_wc__adm_access_write_lock;
}
svn_error_t *
svn_wc__adm_write_check(svn_wc_adm_access_t *adm_access) {
if (adm_access->type == svn_wc__adm_access_write_lock) {
if (adm_access->lock_exists) {
svn_boolean_t locked;
SVN_ERR(svn_wc_locked(&locked, adm_access->path, adm_access->pool));
if (! locked)
return svn_error_createf(SVN_ERR_WC_NOT_LOCKED, NULL,
_("Write-lock stolen in '%s'"),
svn_path_local_style(adm_access->path,
adm_access->pool));
}
} else {
return svn_error_createf(SVN_ERR_WC_NOT_LOCKED, NULL,
_("No write-lock in '%s'"),
svn_path_local_style(adm_access->path,
adm_access->pool));
}
return SVN_NO_ERROR;
}
svn_error_t *
svn_wc_locked(svn_boolean_t *locked, const char *path, apr_pool_t *pool) {
svn_node_kind_t kind;
const char *lockfile
= svn_wc__adm_path(path, 0, pool, SVN_WC__ADM_LOCK, NULL);
SVN_ERR(svn_io_check_path(lockfile, &kind, pool));
if (kind == svn_node_file)
*locked = TRUE;
else if (kind == svn_node_none)
*locked = FALSE;
else
return svn_error_createf(SVN_ERR_WC_LOCKED, NULL,
_("Lock file '%s' is not a regular file"),
svn_path_local_style(lockfile, pool));
return SVN_NO_ERROR;
}
const char *
svn_wc_adm_access_path(svn_wc_adm_access_t *adm_access) {
return adm_access->path;
}
apr_pool_t *
svn_wc_adm_access_pool(svn_wc_adm_access_t *adm_access) {
return adm_access->pool;
}
svn_error_t *
svn_wc__adm_is_cleanup_required(svn_boolean_t *cleanup,
svn_wc_adm_access_t *adm_access,
apr_pool_t *pool) {
if (adm_access->type == svn_wc__adm_access_write_lock) {
svn_node_kind_t kind;
const char *log_path
= svn_wc__adm_path(svn_wc_adm_access_path(adm_access),
FALSE, pool, SVN_WC__ADM_LOG, NULL);
SVN_ERR(svn_io_check_path(log_path, &kind, pool));
*cleanup = (kind == svn_node_file);
} else
*cleanup = FALSE;
return SVN_NO_ERROR;
}
static void
prune_deleted(svn_wc_adm_access_t *adm_access,
apr_pool_t *pool) {
if (! adm_access->entries && adm_access->entries_hidden) {
apr_hash_index_t *hi;
for (hi = apr_hash_first(pool, adm_access->entries_hidden);
hi;
hi = apr_hash_next(hi)) {
void *val;
const svn_wc_entry_t *entry;
apr_hash_this(hi, NULL, NULL, &val);
entry = val;
if ((entry->deleted
&& (entry->schedule != svn_wc_schedule_add)
&& (entry->schedule != svn_wc_schedule_replace))
|| entry->absent)
break;
}
if (! hi) {
adm_access->entries = adm_access->entries_hidden;
return;
}
adm_access->entries = apr_hash_make(adm_access->pool);
for (hi = apr_hash_first(pool, adm_access->entries_hidden);
hi;
hi = apr_hash_next(hi)) {
void *val;
const void *key;
const svn_wc_entry_t *entry;
apr_hash_this(hi, &key, NULL, &val);
entry = val;
if (((entry->deleted == FALSE) && (entry->absent == FALSE))
|| (entry->schedule == svn_wc_schedule_add)
|| (entry->schedule == svn_wc_schedule_replace)) {
apr_hash_set(adm_access->entries, key,
APR_HASH_KEY_STRING, entry);
}
}
}
}
void
svn_wc__adm_access_set_entries(svn_wc_adm_access_t *adm_access,
svn_boolean_t show_hidden,
apr_hash_t *entries) {
if (show_hidden)
adm_access->entries_hidden = entries;
else
adm_access->entries = entries;
}
apr_hash_t *
svn_wc__adm_access_entries(svn_wc_adm_access_t *adm_access,
svn_boolean_t show_hidden,
apr_pool_t *pool) {
if (! show_hidden) {
prune_deleted(adm_access, pool);
return adm_access->entries;
} else
return adm_access->entries_hidden;
}
void
svn_wc__adm_access_set_wcprops(svn_wc_adm_access_t *adm_access,
apr_hash_t *wcprops) {
adm_access->wcprops = wcprops;
}
apr_hash_t *
svn_wc__adm_access_wcprops(svn_wc_adm_access_t *adm_access) {
return adm_access->wcprops;
}
int
svn_wc__adm_wc_format(svn_wc_adm_access_t *adm_access) {
return adm_access->wc_format;
}
void
svn_wc__adm_set_wc_format(svn_wc_adm_access_t *adm_access,
int format) {
adm_access->wc_format = format;
}
svn_boolean_t
svn_wc__adm_missing(svn_wc_adm_access_t *adm_access,
const char *path) {
if (adm_access->set
&& apr_hash_get(adm_access->set, path, APR_HASH_KEY_STRING) == &missing)
return TRUE;
return FALSE;
}
static svn_error_t *
extend_lock_found_entry(const char *path,
const svn_wc_entry_t *entry,
void *walk_baton,
apr_pool_t *pool) {
if (entry->kind == svn_node_dir &&
strcmp(entry->name, SVN_WC_ENTRY_THIS_DIR) != 0) {
svn_wc_adm_access_t *anchor_access = walk_baton, *adm_access;
svn_boolean_t write_lock =
(anchor_access->type == svn_wc__adm_access_write_lock);
svn_error_t *err = svn_wc_adm_probe_try3(&adm_access, anchor_access, path,
write_lock, -1, NULL, NULL, pool);
if (err) {
if (err->apr_err == SVN_ERR_WC_LOCKED)
svn_error_clear(err);
else
return err;
}
}
return SVN_NO_ERROR;
}
static svn_wc_entry_callbacks2_t extend_lock_walker = {
extend_lock_found_entry,
svn_wc__walker_default_error_handler
};
svn_error_t *
svn_wc__adm_extend_lock_to_tree(svn_wc_adm_access_t *adm_access,
apr_pool_t *pool) {
return svn_wc_walk_entries3(adm_access->path, adm_access,
&extend_lock_walker, adm_access,
svn_depth_infinity, FALSE, NULL, NULL, pool);
}
