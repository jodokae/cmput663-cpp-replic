#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <apr_general.h>
#include <apr_pools.h>
#include <apr_file_io.h>
#include <apr_thread_mutex.h>
#include "svn_fs.h"
#include "svn_delta.h"
#include "svn_version.h"
#include "svn_pools.h"
#include "fs.h"
#include "err.h"
#include "fs_fs.h"
#include "tree.h"
#include "lock.h"
#include "svn_private_config.h"
#include "private/svn_fs_util.h"
#include "../libsvn_fs/fs-loader.h"
#define SVN_FSFS_SHARED_USERDATA_PREFIX "svn-fsfs-shared-"
static svn_error_t *
fs_serialized_init(svn_fs_t *fs, apr_pool_t *common_pool, apr_pool_t *pool) {
fs_fs_data_t *ffd = fs->fsap_data;
const char *key;
void *val;
fs_fs_shared_data_t *ffsd;
apr_status_t status;
key = apr_pstrcat(pool, SVN_FSFS_SHARED_USERDATA_PREFIX, ffd->uuid,
(char *) NULL);
status = apr_pool_userdata_get(&val, key, common_pool);
if (status)
return svn_error_wrap_apr(status, _("Can't fetch FSFS shared data"));
ffsd = val;
if (!ffsd) {
ffsd = apr_pcalloc(common_pool, sizeof(*ffsd));
ffsd->common_pool = common_pool;
#if APR_HAS_THREADS
status = apr_thread_mutex_create(&ffsd->fs_write_lock,
APR_THREAD_MUTEX_DEFAULT, common_pool);
if (status)
return svn_error_wrap_apr(status,
_("Can't create FSFS write-lock mutex"));
status = apr_thread_mutex_create(&ffsd->txn_list_lock,
APR_THREAD_MUTEX_DEFAULT, common_pool);
if (status)
return svn_error_wrap_apr(status,
_("Can't create FSFS txn list mutex"));
status = apr_thread_mutex_create(&ffsd->txn_current_lock,
APR_THREAD_MUTEX_DEFAULT, common_pool);
if (status)
return svn_error_wrap_apr(status,
_("Can't create FSFS txn-current mutex"));
#endif
key = apr_pstrdup(common_pool, key);
status = apr_pool_userdata_set(ffsd, key, NULL, common_pool);
if (status)
return svn_error_wrap_apr(status, _("Can't store FSFS shared data"));
}
ffd->shared = ffsd;
return SVN_NO_ERROR;
}
static svn_error_t *
fs_set_errcall(svn_fs_t *fs,
void (*db_errcall_fcn)(const char *errpfx, char *msg)) {
return SVN_NO_ERROR;
}
static fs_vtable_t fs_vtable = {
svn_fs_fs__youngest_rev,
svn_fs_fs__revision_prop,
svn_fs_fs__revision_proplist,
svn_fs_fs__change_rev_prop,
svn_fs_fs__get_uuid,
svn_fs_fs__set_uuid,
svn_fs_fs__revision_root,
svn_fs_fs__begin_txn,
svn_fs_fs__open_txn,
svn_fs_fs__purge_txn,
svn_fs_fs__list_transactions,
svn_fs_fs__deltify,
svn_fs_fs__lock,
svn_fs_fs__generate_lock_token,
svn_fs_fs__unlock,
svn_fs_fs__get_lock,
svn_fs_fs__get_locks,
fs_set_errcall
};
static void
initialize_fs_struct(svn_fs_t *fs) {
fs_fs_data_t *ffd = apr_pcalloc(fs->pool, sizeof(*ffd));
fs->vtable = &fs_vtable;
fs->fsap_data = ffd;
ffd->rev_root_id_cache_pool = svn_pool_create(fs->pool);
ffd->rev_root_id_cache = apr_hash_make(ffd->rev_root_id_cache_pool);
ffd->rev_node_cache = apr_hash_make(fs->pool);
ffd->rev_node_list.prev = &ffd->rev_node_list;
ffd->rev_node_list.next = &ffd->rev_node_list;
}
static svn_error_t *
fs_create(svn_fs_t *fs, const char *path, apr_pool_t *pool,
apr_pool_t *common_pool) {
SVN_ERR(svn_fs__check_fs(fs, FALSE));
initialize_fs_struct(fs);
SVN_ERR(svn_fs_fs__create(fs, path, pool));
return fs_serialized_init(fs, common_pool, pool);
}
static svn_error_t *
fs_open(svn_fs_t *fs, const char *path, apr_pool_t *pool,
apr_pool_t *common_pool) {
initialize_fs_struct(fs);
SVN_ERR(svn_fs_fs__open(fs, path, pool));
return fs_serialized_init(fs, common_pool, pool);
}
static svn_error_t *
fs_open_for_recovery(svn_fs_t *fs,
const char *path,
apr_pool_t *pool, apr_pool_t *common_pool) {
fs->path = apr_pstrdup(fs->pool, path);
svn_error_clear(svn_io_file_create(svn_fs_fs__path_current(fs, pool),
"0 1 1\n", pool));
return fs_open(fs, path, pool, common_pool);
}
static svn_error_t *
fs_upgrade(svn_fs_t *fs, const char *path, apr_pool_t *pool,
apr_pool_t *common_pool) {
SVN_ERR(svn_fs__check_fs(fs, FALSE));
initialize_fs_struct(fs);
SVN_ERR(svn_fs_fs__open(fs, path, pool));
SVN_ERR(fs_serialized_init(fs, common_pool, pool));
return svn_fs_fs__upgrade(fs, pool);
}
static svn_error_t *
fs_hotcopy(const char *src_path,
const char *dest_path,
svn_boolean_t clean_logs,
apr_pool_t *pool) {
return svn_fs_fs__hotcopy(src_path, dest_path, pool);
}
static svn_error_t *
fs_logfiles(apr_array_header_t **logfiles,
const char *path,
svn_boolean_t only_unused,
apr_pool_t *pool) {
*logfiles = apr_array_make(pool, 0, sizeof(const char *));
return SVN_NO_ERROR;
}
static svn_error_t *
fs_delete_fs(const char *path,
apr_pool_t *pool) {
return svn_io_remove_dir2(path, FALSE, NULL, NULL, pool);
}
static const svn_version_t *
fs_version(void) {
SVN_VERSION_BODY;
}
static const char *
fs_get_description(void) {
return _("Module for working with a plain file (FSFS) repository.");
}
static fs_library_vtable_t library_vtable = {
fs_version,
fs_create,
fs_open,
fs_open_for_recovery,
fs_upgrade,
fs_delete_fs,
fs_hotcopy,
fs_get_description,
svn_fs_fs__recover,
fs_logfiles
};
svn_error_t *
svn_fs_fs__init(const svn_version_t *loader_version,
fs_library_vtable_t **vtable, apr_pool_t* common_pool) {
static const svn_version_checklist_t checklist[] = {
{ "svn_subr", svn_subr_version },
{ "svn_delta", svn_delta_version },
{ NULL, NULL }
};
if (loader_version->major != SVN_VER_MAJOR)
return svn_error_createf(SVN_ERR_VERSION_MISMATCH, NULL,
_("Unsupported FS loader version (%d) for fsfs"),
loader_version->major);
SVN_ERR(svn_ver_check_list(fs_version(), checklist));
*vtable = &library_vtable;
return SVN_NO_ERROR;
}
