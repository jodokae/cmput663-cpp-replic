#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <errno.h>
#include <apr_general.h>
#include <apr_pools.h>
#include <apr_file_io.h>
#include <apr_uuid.h>
#include <apr_lib.h>
#include <apr_md5.h>
#include <apr_thread_mutex.h>
#include "svn_pools.h"
#include "svn_fs.h"
#include "svn_path.h"
#include "svn_hash.h"
#include "svn_md5.h"
#include "svn_props.h"
#include "svn_sorts.h"
#include "svn_time.h"
#include "svn_mergeinfo.h"
#include "fs.h"
#include "err.h"
#include "tree.h"
#include "lock.h"
#include "key-gen.h"
#include "fs_fs.h"
#include "id.h"
#include "private/svn_fs_util.h"
#include "../libsvn_fs/fs-loader.h"
#include "svn_private_config.h"
#define FSFS_MAX_PATH_LEN 4096
#if !defined(SVN_FS_FS_DEFAULT_MAX_FILES_PER_DIR)
#define SVN_FS_FS_DEFAULT_MAX_FILES_PER_DIR 1000
#endif
#define HEADER_ID "id"
#define HEADER_TYPE "type"
#define HEADER_COUNT "count"
#define HEADER_PROPS "props"
#define HEADER_TEXT "text"
#define HEADER_CPATH "cpath"
#define HEADER_PRED "pred"
#define HEADER_COPYFROM "copyfrom"
#define HEADER_COPYROOT "copyroot"
#define HEADER_FRESHTXNRT "is-fresh-txn-root"
#define HEADER_MINFO_HERE "minfo-here"
#define HEADER_MINFO_CNT "minfo-cnt"
#define ACTION_MODIFY "modify"
#define ACTION_ADD "add"
#define ACTION_DELETE "delete"
#define ACTION_REPLACE "replace"
#define ACTION_RESET "reset"
#define FLAG_TRUE "true"
#define FLAG_FALSE "false"
#define KIND_FILE "file"
#define KIND_DIR "dir"
#define REP_PLAIN "PLAIN"
#define REP_DELTA "DELTA"
static txn_vtable_t txn_vtable = {
svn_fs_fs__commit_txn,
svn_fs_fs__abort_txn,
svn_fs_fs__txn_prop,
svn_fs_fs__txn_proplist,
svn_fs_fs__change_txn_prop,
svn_fs_fs__txn_root,
svn_fs_fs__change_txn_props
};
static const char *
path_format(svn_fs_t *fs, apr_pool_t *pool) {
return svn_path_join(fs->path, PATH_FORMAT, pool);
}
static APR_INLINE const char *
path_uuid(svn_fs_t *fs, apr_pool_t *pool) {
return svn_path_join(fs->path, PATH_UUID, pool);
}
const char *
svn_fs_fs__path_current(svn_fs_t *fs, apr_pool_t *pool) {
return svn_path_join(fs->path, PATH_CURRENT, pool);
}
static APR_INLINE const char *
path_txn_current(svn_fs_t *fs, apr_pool_t *pool) {
return svn_path_join(fs->path, PATH_TXN_CURRENT, pool);
}
static APR_INLINE const char *
path_txn_current_lock(svn_fs_t *fs, apr_pool_t *pool) {
return svn_path_join(fs->path, PATH_TXN_CURRENT_LOCK, pool);
}
static APR_INLINE const char *
path_lock(svn_fs_t *fs, apr_pool_t *pool) {
return svn_path_join(fs->path, PATH_LOCK_FILE, pool);
}
static const char *
path_rev_shard(svn_fs_t *fs, svn_revnum_t rev, apr_pool_t *pool) {
fs_fs_data_t *ffd = fs->fsap_data;
assert(ffd->max_files_per_dir);
return svn_path_join_many(pool, fs->path, PATH_REVS_DIR,
apr_psprintf(pool, "%ld",
rev / ffd->max_files_per_dir),
NULL);
}
const char *
svn_fs_fs__path_rev(svn_fs_t *fs, svn_revnum_t rev, apr_pool_t *pool) {
fs_fs_data_t *ffd = fs->fsap_data;
if (ffd->max_files_per_dir) {
return svn_path_join(path_rev_shard(fs, rev, pool),
apr_psprintf(pool, "%ld", rev),
pool);
}
return svn_path_join_many(pool, fs->path, PATH_REVS_DIR,
apr_psprintf(pool, "%ld", rev), NULL);
}
static const char *
path_revprops_shard(svn_fs_t *fs, svn_revnum_t rev, apr_pool_t *pool) {
fs_fs_data_t *ffd = fs->fsap_data;
assert(ffd->max_files_per_dir);
return svn_path_join_many(pool, fs->path, PATH_REVPROPS_DIR,
apr_psprintf(pool, "%ld",
rev / ffd->max_files_per_dir),
NULL);
}
static const char *
path_revprops(svn_fs_t *fs, svn_revnum_t rev, apr_pool_t *pool) {
fs_fs_data_t *ffd = fs->fsap_data;
if (ffd->max_files_per_dir) {
return svn_path_join(path_revprops_shard(fs, rev, pool),
apr_psprintf(pool, "%ld", rev),
pool);
}
return svn_path_join_many(pool, fs->path, PATH_REVPROPS_DIR,
apr_psprintf(pool, "%ld", rev), NULL);
}
static APR_INLINE const char *
path_txn_dir(svn_fs_t *fs, const char *txn_id, apr_pool_t *pool) {
return svn_path_join_many(pool, fs->path, PATH_TXNS_DIR,
apr_pstrcat(pool, txn_id, PATH_EXT_TXN, NULL),
NULL);
}
static APR_INLINE const char *
path_txn_changes(svn_fs_t *fs, const char *txn_id, apr_pool_t *pool) {
return svn_path_join(path_txn_dir(fs, txn_id, pool), PATH_CHANGES, pool);
}
static APR_INLINE const char *
path_txn_props(svn_fs_t *fs, const char *txn_id, apr_pool_t *pool) {
return svn_path_join(path_txn_dir(fs, txn_id, pool), PATH_TXN_PROPS, pool);
}
static APR_INLINE const char *
path_txn_next_ids(svn_fs_t *fs, const char *txn_id, apr_pool_t *pool) {
return svn_path_join(path_txn_dir(fs, txn_id, pool), PATH_NEXT_IDS, pool);
}
static APR_INLINE const char *
path_txn_proto_rev(svn_fs_t *fs, const char *txn_id, apr_pool_t *pool) {
fs_fs_data_t *ffd = fs->fsap_data;
if (ffd->format >= SVN_FS_FS__MIN_PROTOREVS_DIR_FORMAT)
return svn_path_join_many(pool, fs->path, PATH_TXN_PROTOS_DIR,
apr_pstrcat(pool, txn_id, PATH_EXT_REV, NULL),
NULL);
else
return svn_path_join(path_txn_dir(fs, txn_id, pool), PATH_REV, pool);
}
static APR_INLINE const char *
path_txn_proto_rev_lock(svn_fs_t *fs, const char *txn_id, apr_pool_t *pool) {
fs_fs_data_t *ffd = fs->fsap_data;
if (ffd->format >= SVN_FS_FS__MIN_PROTOREVS_DIR_FORMAT)
return svn_path_join_many(pool, fs->path, PATH_TXN_PROTOS_DIR,
apr_pstrcat(pool, txn_id, PATH_EXT_REV_LOCK,
NULL),
NULL);
else
return svn_path_join(path_txn_dir(fs, txn_id, pool), PATH_REV_LOCK, pool);
}
static const char *
path_txn_node_rev(svn_fs_t *fs, const svn_fs_id_t *id, apr_pool_t *pool) {
const char *txn_id = svn_fs_fs__id_txn_id(id);
const char *node_id = svn_fs_fs__id_node_id(id);
const char *copy_id = svn_fs_fs__id_copy_id(id);
const char *name = apr_psprintf(pool, PATH_PREFIX_NODE "%s.%s",
node_id, copy_id);
return svn_path_join(path_txn_dir(fs, txn_id, pool), name, pool);
}
static APR_INLINE const char *
path_txn_node_props(svn_fs_t *fs, const svn_fs_id_t *id, apr_pool_t *pool) {
return apr_pstrcat(pool, path_txn_node_rev(fs, id, pool), PATH_EXT_PROPS,
NULL);
}
static APR_INLINE const char *
path_txn_node_children(svn_fs_t *fs, const svn_fs_id_t *id, apr_pool_t *pool) {
return apr_pstrcat(pool, path_txn_node_rev(fs, id, pool),
PATH_EXT_CHILDREN, NULL);
}
static APR_INLINE const char *
path_node_origin(svn_fs_t *fs, const char *node_id, apr_pool_t *pool) {
int len = strlen(node_id);
const char *node_id_minus_last_char =
(len == 1) ? "0" : apr_pstrmemdup(pool, node_id, len - 1);
return svn_path_join_many(pool, fs->path, PATH_NODE_ORIGINS_DIR,
node_id_minus_last_char, NULL);
}
static fs_fs_shared_txn_data_t *
get_shared_txn(svn_fs_t *fs, const char *txn_id, svn_boolean_t create_new) {
fs_fs_data_t *ffd = fs->fsap_data;
fs_fs_shared_data_t *ffsd = ffd->shared;
fs_fs_shared_txn_data_t *txn;
for (txn = ffsd->txns; txn; txn = txn->next)
if (strcmp(txn->txn_id, txn_id) == 0)
break;
if (txn || !create_new)
return txn;
if (ffsd->free_txn) {
txn = ffsd->free_txn;
ffsd->free_txn = NULL;
} else {
apr_pool_t *subpool = svn_pool_create(ffsd->common_pool);
txn = apr_palloc(subpool, sizeof(*txn));
txn->pool = subpool;
}
assert(strlen(txn_id) < sizeof(txn->txn_id));
strcpy(txn->txn_id, txn_id);
txn->being_written = FALSE;
txn->next = ffsd->txns;
ffsd->txns = txn;
return txn;
}
static void
free_shared_txn(svn_fs_t *fs, const char *txn_id) {
fs_fs_data_t *ffd = fs->fsap_data;
fs_fs_shared_data_t *ffsd = ffd->shared;
fs_fs_shared_txn_data_t *txn, *prev = NULL;
for (txn = ffsd->txns; txn; prev = txn, txn = txn->next)
if (strcmp(txn->txn_id, txn_id) == 0)
break;
if (!txn)
return;
if (prev)
prev->next = txn->next;
else
ffsd->txns = txn->next;
if (!ffsd->free_txn)
ffsd->free_txn = txn;
else
svn_pool_destroy(txn->pool);
}
static svn_error_t *
with_txnlist_lock(svn_fs_t *fs,
svn_error_t *(*body)(svn_fs_t *fs,
const void *baton,
apr_pool_t *pool),
void *baton,
apr_pool_t *pool) {
svn_error_t *err;
#if APR_HAS_THREADS
fs_fs_data_t *ffd = fs->fsap_data;
fs_fs_shared_data_t *ffsd = ffd->shared;
apr_status_t apr_err;
apr_err = apr_thread_mutex_lock(ffsd->txn_list_lock);
if (apr_err)
return svn_error_wrap_apr(apr_err, _("Can't grab FSFS txn list mutex"));
#endif
err = body(fs, baton, pool);
#if APR_HAS_THREADS
apr_err = apr_thread_mutex_unlock(ffsd->txn_list_lock);
if (apr_err && !err)
return svn_error_wrap_apr(apr_err, _("Can't ungrab FSFS txn list mutex"));
#endif
return err;
}
static svn_error_t *
get_lock_on_filesystem(const char *lock_filename,
apr_pool_t *pool) {
svn_error_t *err = svn_io_file_lock2(lock_filename, TRUE, FALSE, pool);
if (err && APR_STATUS_IS_ENOENT(err->apr_err)) {
svn_error_clear(err);
err = NULL;
SVN_ERR(svn_io_file_create(lock_filename, "", pool));
SVN_ERR(svn_io_file_lock2(lock_filename, TRUE, FALSE, pool));
}
return err;
}
static svn_error_t *
with_some_lock(svn_error_t *(*body)(void *baton,
apr_pool_t *pool),
void *baton,
const char *lock_filename,
#if APR_HAS_THREADS
apr_thread_mutex_t *lock_mutex,
#endif
apr_pool_t *pool) {
apr_pool_t *subpool = svn_pool_create(pool);
svn_error_t *err;
#if APR_HAS_THREADS
apr_status_t status;
status = apr_thread_mutex_lock(lock_mutex);
if (status)
return svn_error_wrap_apr(status,
_("Can't grab FSFS mutex for '%s'"),
lock_filename);
#endif
err = get_lock_on_filesystem(lock_filename, subpool);
if (!err)
err = body(baton, subpool);
svn_pool_destroy(subpool);
#if APR_HAS_THREADS
status = apr_thread_mutex_unlock(lock_mutex);
if (status && !err)
return svn_error_wrap_apr(status,
_("Can't ungrab FSFS mutex for '%s'"),
lock_filename);
#endif
return err;
}
svn_error_t *
svn_fs_fs__with_write_lock(svn_fs_t *fs,
svn_error_t *(*body)(void *baton,
apr_pool_t *pool),
void *baton,
apr_pool_t *pool) {
#if APR_HAS_THREADS
fs_fs_data_t *ffd = fs->fsap_data;
fs_fs_shared_data_t *ffsd = ffd->shared;
apr_thread_mutex_t *mutex = ffsd->fs_write_lock;
#endif
return with_some_lock(body, baton,
path_lock(fs, pool),
#if APR_HAS_THREADS
mutex,
#endif
pool);
}
static svn_error_t *
with_txn_current_lock(svn_fs_t *fs,
svn_error_t *(*body)(void *baton,
apr_pool_t *pool),
void *baton,
apr_pool_t *pool) {
#if APR_HAS_THREADS
fs_fs_data_t *ffd = fs->fsap_data;
fs_fs_shared_data_t *ffsd = ffd->shared;
apr_thread_mutex_t *mutex = ffsd->txn_current_lock;
#endif
return with_some_lock(body, baton,
path_txn_current_lock(fs, pool),
#if APR_HAS_THREADS
mutex,
#endif
pool);
}
struct unlock_proto_rev_baton {
const char *txn_id;
void *lockcookie;
};
static svn_error_t *
unlock_proto_rev_body(svn_fs_t *fs, const void *baton, apr_pool_t *pool) {
const struct unlock_proto_rev_baton *b = baton;
const char *txn_id = b->txn_id;
apr_file_t *lockfile = b->lockcookie;
fs_fs_shared_txn_data_t *txn = get_shared_txn(fs, txn_id, FALSE);
apr_status_t apr_err;
if (!txn)
return svn_error_createf(SVN_ERR_FS_CORRUPT, NULL,
_("Can't unlock unknown transaction '%s'"),
txn_id);
if (!txn->being_written)
return svn_error_createf(SVN_ERR_FS_CORRUPT, NULL,
_("Can't unlock nonlocked transaction '%s'"),
txn_id);
apr_err = apr_file_unlock(lockfile);
if (apr_err)
return svn_error_wrap_apr
(apr_err,
_("Can't unlock prototype revision lockfile for transaction '%s'"),
txn_id);
apr_err = apr_file_close(lockfile);
if (apr_err)
return svn_error_wrap_apr
(apr_err,
_("Can't close prototype revision lockfile for transaction '%s'"),
txn_id);
txn->being_written = FALSE;
return SVN_NO_ERROR;
}
static svn_error_t *
unlock_proto_rev(svn_fs_t *fs, const char *txn_id, void *lockcookie,
apr_pool_t *pool) {
struct unlock_proto_rev_baton b;
b.txn_id = txn_id;
b.lockcookie = lockcookie;
return with_txnlist_lock(fs, unlock_proto_rev_body, &b, pool);
}
static svn_error_t *
unlock_proto_rev_list_locked(svn_fs_t *fs, const char *txn_id,
void *lockcookie,
apr_pool_t *pool) {
struct unlock_proto_rev_baton b;
b.txn_id = txn_id;
b.lockcookie = lockcookie;
return unlock_proto_rev_body(fs, &b, pool);
}
struct get_writable_proto_rev_baton {
apr_file_t **file;
void **lockcookie;
const char *txn_id;
};
static svn_error_t *
get_writable_proto_rev_body(svn_fs_t *fs, const void *baton, apr_pool_t *pool) {
const struct get_writable_proto_rev_baton *b = baton;
apr_file_t **file = b->file;
void **lockcookie = b->lockcookie;
const char *txn_id = b->txn_id;
svn_error_t *err;
fs_fs_shared_txn_data_t *txn = get_shared_txn(fs, txn_id, TRUE);
if (txn->being_written)
return svn_error_createf(SVN_ERR_FS_REP_BEING_WRITTEN, NULL,
_("Cannot write to the prototype revision file "
"of transaction '%s' because a previous "
"representation is currently being written by "
"this process"),
txn_id);
{
apr_file_t *lockfile;
apr_status_t apr_err;
const char *lockfile_path = path_txn_proto_rev_lock(fs, txn_id, pool);
SVN_ERR(svn_io_file_open(&lockfile, lockfile_path,
APR_WRITE | APR_CREATE, APR_OS_DEFAULT, pool));
apr_err = apr_file_lock(lockfile,
APR_FLOCK_EXCLUSIVE | APR_FLOCK_NONBLOCK);
if (apr_err) {
svn_error_clear(svn_io_file_close(lockfile, pool));
if (APR_STATUS_IS_EAGAIN(apr_err))
return svn_error_createf(SVN_ERR_FS_REP_BEING_WRITTEN, NULL,
_("Cannot write to the prototype revision "
"file of transaction '%s' because a "
"previous representation is currently "
"being written by another process"),
txn_id);
return svn_error_wrap_apr(apr_err,
_("Can't get exclusive lock on file '%s'"),
svn_path_local_style(lockfile_path, pool));
}
*lockcookie = lockfile;
}
txn->being_written = TRUE;
err = svn_io_file_open(file, path_txn_proto_rev(fs, txn_id, pool),
APR_WRITE | APR_BUFFERED, APR_OS_DEFAULT, pool);
if (!err) {
apr_off_t offset = 0;
err = svn_io_file_seek(*file, APR_END, &offset, 0);
}
if (err) {
svn_error_clear(unlock_proto_rev_list_locked(fs, txn_id, *lockcookie,
pool));
*lockcookie = NULL;
}
return err;
}
static svn_error_t *
get_writable_proto_rev(apr_file_t **file,
void **lockcookie,
svn_fs_t *fs, const char *txn_id,
apr_pool_t *pool) {
struct get_writable_proto_rev_baton b;
b.file = file;
b.lockcookie = lockcookie;
b.txn_id = txn_id;
return with_txnlist_lock(fs, get_writable_proto_rev_body, &b, pool);
}
static svn_error_t *
purge_shared_txn_body(svn_fs_t *fs, const void *baton, apr_pool_t *pool) {
const char *txn_id = *(const char **)baton;
free_shared_txn(fs, txn_id);
return SVN_NO_ERROR;
}
static svn_error_t *
purge_shared_txn(svn_fs_t *fs, const char *txn_id, apr_pool_t *pool) {
return with_txnlist_lock(fs, purge_shared_txn_body, (char **) &txn_id, pool);
}
static svn_error_t *
get_file_offset(apr_off_t *offset_p, apr_file_t *file, apr_pool_t *pool) {
apr_off_t offset;
offset = 0;
SVN_ERR(svn_io_file_seek(file, APR_CUR, &offset, pool));
*offset_p = offset;
return SVN_NO_ERROR;
}
static svn_error_t *
check_format_file_buffer_numeric(const char *buf, const char *path,
apr_pool_t *pool) {
const char *p;
for (p = buf; *p; p++)
if (!apr_isdigit(*p))
return svn_error_createf(SVN_ERR_BAD_VERSION_FILE_FORMAT, NULL,
_("Format file '%s' contains an unexpected non-digit"),
svn_path_local_style(path, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
read_format(int *pformat, int *max_files_per_dir,
const char *path, apr_pool_t *pool) {
svn_error_t *err;
apr_file_t *file;
char buf[80];
apr_size_t len;
err = svn_io_file_open(&file, path, APR_READ | APR_BUFFERED,
APR_OS_DEFAULT, pool);
if (err && APR_STATUS_IS_ENOENT(err->apr_err)) {
svn_error_clear(err);
*pformat = 1;
*max_files_per_dir = 0;
return SVN_NO_ERROR;
}
len = sizeof(buf);
err = svn_io_read_length_line(file, buf, &len, pool);
if (err && APR_STATUS_IS_EOF(err->apr_err)) {
svn_error_clear(err);
return svn_error_createf(SVN_ERR_BAD_VERSION_FILE_FORMAT, NULL,
_("Can't read first line of format file '%s'"),
svn_path_local_style(path, pool));
}
SVN_ERR(err);
SVN_ERR(check_format_file_buffer_numeric(buf, path, pool));
*pformat = atoi(buf);
*max_files_per_dir = 0;
while (1) {
len = sizeof(buf);
err = svn_io_read_length_line(file, buf, &len, pool);
if (err && APR_STATUS_IS_EOF(err->apr_err)) {
svn_error_clear(err);
break;
}
SVN_ERR(err);
if (*pformat >= SVN_FS_FS__MIN_LAYOUT_FORMAT_OPTION_FORMAT &&
strncmp(buf, "layout ", 7) == 0) {
if (strcmp(buf+7, "linear") == 0) {
*max_files_per_dir = 0;
continue;
}
if (strncmp(buf+7, "sharded ", 8) == 0) {
SVN_ERR(check_format_file_buffer_numeric(buf+15, path, pool));
*max_files_per_dir = atoi(buf+15);
continue;
}
}
return svn_error_createf(SVN_ERR_BAD_VERSION_FILE_FORMAT, NULL,
_("'%s' contains invalid filesystem format option '%s'"),
svn_path_local_style(path, pool), buf);
}
SVN_ERR(svn_io_file_close(file, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
write_format(const char *path, int format, int max_files_per_dir,
svn_boolean_t overwrite, apr_pool_t *pool) {
const char *contents;
assert (1 <= format && format <= SVN_FS_FS__FORMAT_NUMBER);
if (format >= SVN_FS_FS__MIN_LAYOUT_FORMAT_OPTION_FORMAT) {
if (max_files_per_dir)
contents = apr_psprintf(pool,
"%d\n"
"layout sharded %d\n",
format, max_files_per_dir);
else
contents = apr_psprintf(pool,
"%d\n"
"layout linear",
format);
} else {
contents = apr_psprintf(pool, "%d\n", format);
}
if (! overwrite) {
SVN_ERR(svn_io_file_create(path, contents, pool));
} else {
apr_file_t *format_file;
const char *path_tmp;
SVN_ERR(svn_io_open_unique_file2(&format_file, &path_tmp, path, ".tmp",
svn_io_file_del_none, pool));
SVN_ERR(svn_io_file_write_full(format_file, contents,
strlen(contents), NULL, pool));
SVN_ERR(svn_io_file_close(format_file, pool));
#if defined(WIN32)
SVN_ERR(svn_io_set_file_read_write(path, TRUE, pool));
#endif
SVN_ERR(svn_io_file_rename(path_tmp, path, pool));
}
SVN_ERR(svn_io_set_file_read_only(path, FALSE, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
check_format(int format) {
if (1 <= format && format <= SVN_FS_FS__FORMAT_NUMBER)
return SVN_NO_ERROR;
return svn_error_createf(SVN_ERR_FS_UNSUPPORTED_FORMAT, NULL,
_("Expected FS format between '1' and '%d'; found format '%d'"),
SVN_FS_FS__FORMAT_NUMBER, format);
}
svn_boolean_t
svn_fs_fs__fs_supports_mergeinfo(svn_fs_t *fs) {
fs_fs_data_t *ffd = fs->fsap_data;
return ffd->format >= SVN_FS_FS__MIN_MERGEINFO_FORMAT;
}
static svn_error_t *
get_youngest(svn_revnum_t *youngest_p, const char *fs_path, apr_pool_t *pool);
svn_error_t *
svn_fs_fs__open(svn_fs_t *fs, const char *path, apr_pool_t *pool) {
fs_fs_data_t *ffd = fs->fsap_data;
apr_file_t *uuid_file;
int format, max_files_per_dir;
char buf[APR_UUID_FORMATTED_LENGTH + 2];
apr_size_t limit;
fs->path = apr_pstrdup(fs->pool, path);
SVN_ERR(read_format(&format, &max_files_per_dir,
path_format(fs, pool), pool));
ffd->format = format;
ffd->max_files_per_dir = max_files_per_dir;
SVN_ERR(check_format(format));
SVN_ERR(svn_io_file_open(&uuid_file, path_uuid(fs, pool),
APR_READ | APR_BUFFERED, APR_OS_DEFAULT, pool));
limit = sizeof(buf);
SVN_ERR(svn_io_read_length_line(uuid_file, buf, &limit, pool));
ffd->uuid = apr_pstrdup(fs->pool, buf);
SVN_ERR(svn_io_file_close(uuid_file, pool));
SVN_ERR(get_youngest(&(ffd->youngest_rev_cache), path, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
create_file_ignore_eexist(const char *file,
const char *contents,
apr_pool_t *pool) {
svn_error_t *err = svn_io_file_create(file, contents, pool);
if (err && APR_STATUS_IS_EEXIST(err->apr_err)) {
svn_error_clear(err);
err = SVN_NO_ERROR;
}
return err;
}
static svn_error_t *
upgrade_body(void *baton, apr_pool_t *pool) {
svn_fs_t *fs = baton;
int format, max_files_per_dir;
const char *format_path = path_format(fs, pool);
SVN_ERR(read_format(&format, &max_files_per_dir, format_path, pool));
if (format == SVN_FS_FS__FORMAT_NUMBER)
return SVN_NO_ERROR;
if (format < SVN_FS_FS__MIN_TXN_CURRENT_FORMAT) {
SVN_ERR(create_file_ignore_eexist(path_txn_current(fs, pool), "0\n",
pool));
SVN_ERR(create_file_ignore_eexist(path_txn_current_lock(fs, pool), "",
pool));
}
if (format < SVN_FS_FS__MIN_PROTOREVS_DIR_FORMAT) {
SVN_ERR(svn_io_make_dir_recursively
(svn_path_join(fs->path, PATH_TXN_PROTOS_DIR, pool), pool));
}
SVN_ERR(write_format(format_path, SVN_FS_FS__FORMAT_NUMBER, 0,
TRUE, pool));
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_fs__upgrade(svn_fs_t *fs, apr_pool_t *pool) {
return svn_fs_fs__with_write_lock(fs, upgrade_body, (void *)fs, pool);
}
#define SVN_ESTALE_RETRY_COUNT 10
#if defined(ESTALE)
#define SVN_RETRY_ESTALE(err, expr) { \
svn_error_clear(err); \
err = (expr); \
if (err) \
{ \
if (APR_TO_OS_ERROR(err->apr_err) == ESTALE) \
continue; \
return err; \
} \
}
#define SVN_IGNORE_ESTALE(err, expr) { svn_error_clear(err); err = (expr); if (err) { if (APR_TO_OS_ERROR(err->apr_err) != ESTALE) return err; } }
#else
#define SVN_RETRY_ESTALE(err, expr) SVN_ERR(expr)
#define SVN_IGNORE_ESTALE(err, expr) SVN_ERR(expr)
#endif
#define CURRENT_BUF_LEN 48
static svn_error_t *
read_current(const char *fname, char **buf, apr_pool_t *pool) {
apr_file_t *revision_file;
apr_size_t len;
int i;
svn_error_t *err = SVN_NO_ERROR;
apr_pool_t *iterpool;
*buf = apr_palloc(pool, CURRENT_BUF_LEN);
iterpool = svn_pool_create(pool);
for (i = 0; i < SVN_ESTALE_RETRY_COUNT; i++) {
svn_pool_clear(iterpool);
SVN_RETRY_ESTALE(err, svn_io_file_open(&revision_file, fname,
APR_READ | APR_BUFFERED,
APR_OS_DEFAULT, iterpool));
len = CURRENT_BUF_LEN;
SVN_RETRY_ESTALE(err, svn_io_read_length_line(revision_file,
*buf, &len, iterpool));
SVN_IGNORE_ESTALE(err, svn_io_file_close(revision_file, iterpool));
break;
}
svn_pool_destroy(iterpool);
return err;
}
static svn_error_t *
get_youngest(svn_revnum_t *youngest_p,
const char *fs_path,
apr_pool_t *pool) {
char *buf;
SVN_ERR(read_current(svn_path_join(fs_path, PATH_CURRENT, pool),
&buf, pool));
*youngest_p = SVN_STR_TO_REV(buf);
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_fs__hotcopy(const char *src_path,
const char *dst_path,
apr_pool_t *pool) {
const char *src_subdir, *dst_subdir;
svn_revnum_t youngest, rev;
apr_pool_t *iterpool;
svn_node_kind_t kind;
int format, max_files_per_dir;
SVN_ERR(read_format(&format, &max_files_per_dir,
svn_path_join(src_path, PATH_FORMAT, pool),
pool));
SVN_ERR(check_format(format));
SVN_ERR(svn_io_dir_file_copy(src_path, dst_path, PATH_CURRENT, pool));
SVN_ERR(svn_io_dir_file_copy(src_path, dst_path, PATH_UUID, pool));
SVN_ERR(get_youngest(&youngest, dst_path, pool));
src_subdir = svn_path_join(src_path, PATH_REVS_DIR, pool);
dst_subdir = svn_path_join(dst_path, PATH_REVS_DIR, pool);
SVN_ERR(svn_io_make_dir_recursively(dst_subdir, pool));
iterpool = svn_pool_create(pool);
for (rev = 0; rev <= youngest; rev++) {
const char *src_subdir_shard = src_subdir,
*dst_subdir_shard = dst_subdir;
if (max_files_per_dir) {
const char *shard = apr_psprintf(iterpool, "%ld",
rev / max_files_per_dir);
src_subdir_shard = svn_path_join(src_subdir, shard, iterpool);
dst_subdir_shard = svn_path_join(dst_subdir, shard, iterpool);
if (rev % max_files_per_dir == 0)
SVN_ERR(svn_io_dir_make(dst_subdir_shard, APR_OS_DEFAULT,
iterpool));
}
SVN_ERR(svn_io_dir_file_copy(src_subdir_shard, dst_subdir_shard,
apr_psprintf(iterpool, "%ld", rev),
iterpool));
svn_pool_clear(iterpool);
}
src_subdir = svn_path_join(src_path, PATH_REVPROPS_DIR, pool);
dst_subdir = svn_path_join(dst_path, PATH_REVPROPS_DIR, pool);
SVN_ERR(svn_io_make_dir_recursively(dst_subdir, pool));
for (rev = 0; rev <= youngest; rev++) {
const char *src_subdir_shard = src_subdir,
*dst_subdir_shard = dst_subdir;
svn_pool_clear(iterpool);
if (max_files_per_dir) {
const char *shard = apr_psprintf(iterpool, "%ld",
rev / max_files_per_dir);
src_subdir_shard = svn_path_join(src_subdir, shard, iterpool);
dst_subdir_shard = svn_path_join(dst_subdir, shard, iterpool);
if (rev % max_files_per_dir == 0)
SVN_ERR(svn_io_dir_make(dst_subdir_shard, APR_OS_DEFAULT,
iterpool));
}
SVN_ERR(svn_io_dir_file_copy(src_subdir_shard, dst_subdir_shard,
apr_psprintf(iterpool, "%ld", rev),
iterpool));
}
svn_pool_destroy(iterpool);
dst_subdir = svn_path_join(dst_path, PATH_TXNS_DIR, pool);
SVN_ERR(svn_io_make_dir_recursively(dst_subdir, pool));
if (format >= SVN_FS_FS__MIN_PROTOREVS_DIR_FORMAT) {
dst_subdir = svn_path_join(dst_path, PATH_TXN_PROTOS_DIR, pool);
SVN_ERR(svn_io_make_dir_recursively(dst_subdir, pool));
}
src_subdir = svn_path_join(src_path, PATH_LOCKS_DIR, pool);
SVN_ERR(svn_io_check_path(src_subdir, &kind, pool));
if (kind == svn_node_dir)
SVN_ERR(svn_io_copy_dir_recursively(src_subdir, dst_path,
PATH_LOCKS_DIR, TRUE, NULL,
NULL, pool));
src_subdir = svn_path_join(src_path, PATH_NODE_ORIGINS_DIR, pool);
SVN_ERR(svn_io_check_path(src_subdir, &kind, pool));
if (kind == svn_node_dir)
SVN_ERR(svn_io_copy_dir_recursively(src_subdir, dst_path,
PATH_NODE_ORIGINS_DIR, TRUE, NULL,
NULL, pool));
if (format >= SVN_FS_FS__MIN_TXN_CURRENT_FORMAT)
SVN_ERR(svn_io_dir_file_copy(src_path, dst_path, PATH_TXN_CURRENT, pool));
SVN_ERR(write_format(svn_path_join(dst_path, PATH_FORMAT, pool),
format, max_files_per_dir, FALSE, pool));
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_fs__youngest_rev(svn_revnum_t *youngest_p,
svn_fs_t *fs,
apr_pool_t *pool) {
fs_fs_data_t *ffd = fs->fsap_data;
SVN_ERR(get_youngest(youngest_p, fs->path, pool));
ffd->youngest_rev_cache = *youngest_p;
return SVN_NO_ERROR;
}
#define MAX_HEADERS_STR_LEN FSFS_MAX_PATH_LEN + sizeof(HEADER_CPATH ": \n") - 1
static svn_error_t * read_header_block(apr_hash_t **headers,
apr_file_t *file,
apr_pool_t *pool) {
*headers = apr_hash_make(pool);
while (1) {
char header_str[MAX_HEADERS_STR_LEN];
const char *name, *value;
apr_size_t i = 0, header_len;
apr_size_t limit;
char *local_name, *local_value;
limit = sizeof(header_str);
SVN_ERR(svn_io_read_length_line(file, header_str, &limit, pool));
if (strlen(header_str) == 0)
break;
header_len = strlen(header_str);
while (header_str[i] != ':') {
if (header_str[i] == '\0')
return svn_error_create(SVN_ERR_FS_CORRUPT, NULL,
_("Found malformed header in "
"revision file"));
i++;
}
header_str[i] = '\0';
name = header_str;
i += 2;
if (i > header_len)
return svn_error_create(SVN_ERR_FS_CORRUPT, NULL,
_("Found malformed header in "
"revision file"));
value = header_str + i;
local_name = apr_pstrdup(pool, name);
local_value = apr_pstrdup(pool, value);
apr_hash_set(*headers, local_name, APR_HASH_KEY_STRING, local_value);
}
return SVN_NO_ERROR;
}
static svn_error_t *
ensure_revision_exists(svn_fs_t *fs,
svn_revnum_t rev,
apr_pool_t *pool) {
fs_fs_data_t *ffd = fs->fsap_data;
if (! SVN_IS_VALID_REVNUM(rev))
return svn_error_createf(SVN_ERR_FS_NO_SUCH_REVISION, NULL,
_("Invalid revision number '%ld'"), rev);
if (rev <= ffd->youngest_rev_cache)
return SVN_NO_ERROR;
SVN_ERR(get_youngest(&(ffd->youngest_rev_cache), fs->path, pool));
if (rev <= ffd->youngest_rev_cache)
return SVN_NO_ERROR;
return svn_error_createf(SVN_ERR_FS_NO_SUCH_REVISION, NULL,
_("No such revision %ld"), rev);
}
static svn_error_t *
open_and_seek_revision(apr_file_t **file,
svn_fs_t *fs,
svn_revnum_t rev,
apr_off_t offset,
apr_pool_t *pool) {
apr_file_t *rev_file;
SVN_ERR(ensure_revision_exists(fs, rev, pool));
SVN_ERR(svn_io_file_open(&rev_file, svn_fs_fs__path_rev(fs, rev, pool),
APR_READ | APR_BUFFERED, APR_OS_DEFAULT, pool));
SVN_ERR(svn_io_file_seek(rev_file, APR_SET, &offset, pool));
*file = rev_file;
return SVN_NO_ERROR;
}
static svn_error_t *
open_and_seek_transaction(apr_file_t **file,
svn_fs_t *fs,
const char *txn_id,
representation_t *rep,
apr_pool_t *pool) {
apr_file_t *rev_file;
apr_off_t offset;
SVN_ERR(svn_io_file_open(&rev_file, path_txn_proto_rev(fs, txn_id, pool),
APR_READ | APR_BUFFERED, APR_OS_DEFAULT, pool));
offset = rep->offset;
SVN_ERR(svn_io_file_seek(rev_file, APR_SET, &offset, pool));
*file = rev_file;
return SVN_NO_ERROR;
}
static svn_error_t *
open_and_seek_representation(apr_file_t **file_p,
svn_fs_t *fs,
representation_t *rep,
apr_pool_t *pool) {
if (! rep->txn_id)
return open_and_seek_revision(file_p, fs, rep->revision, rep->offset,
pool);
else
return open_and_seek_transaction(file_p, fs, rep->txn_id, rep, pool);
}
static svn_error_t *
read_rep_offsets(representation_t **rep_p,
char *string,
const char *txn_id,
svn_boolean_t mutable_rep_truncated,
apr_pool_t *pool) {
representation_t *rep;
char *str, *last_str;
int i;
rep = apr_pcalloc(pool, sizeof(*rep));
*rep_p = rep;
str = apr_strtok(string, " ", &last_str);
if (str == NULL)
return svn_error_create(SVN_ERR_FS_CORRUPT, NULL,
_("Malformed text rep offset line in node-rev"));
rep->revision = SVN_STR_TO_REV(str);
if (rep->revision == SVN_INVALID_REVNUM) {
rep->txn_id = txn_id;
if (mutable_rep_truncated)
return SVN_NO_ERROR;
}
str = apr_strtok(NULL, " ", &last_str);
if (str == NULL)
return svn_error_create(SVN_ERR_FS_CORRUPT, NULL,
_("Malformed text rep offset line in node-rev"));
rep->offset = apr_atoi64(str);
str = apr_strtok(NULL, " ", &last_str);
if (str == NULL)
return svn_error_create(SVN_ERR_FS_CORRUPT, NULL,
_("Malformed text rep offset line in node-rev"));
rep->size = apr_atoi64(str);
str = apr_strtok(NULL, " ", &last_str);
if (str == NULL)
return svn_error_create(SVN_ERR_FS_CORRUPT, NULL,
_("Malformed text rep offset line in node-rev"));
rep->expanded_size = apr_atoi64(str);
str = apr_strtok(NULL, " ", &last_str);
if ((str == NULL) || (strlen(str) != (APR_MD5_DIGESTSIZE * 2)))
return svn_error_create(SVN_ERR_FS_CORRUPT, NULL,
_("Malformed text rep offset line in node-rev"));
for (i = 0; i < APR_MD5_DIGESTSIZE; i++) {
if ((! isxdigit(str[i * 2])) || (! isxdigit(str[i * 2 + 1])))
return svn_error_create
(SVN_ERR_FS_CORRUPT, NULL,
_("Malformed text rep offset line in node-rev"));
str[i * 2] = tolower(str[i * 2]);
rep->checksum[i] = (str[i * 2] -
((str[i * 2] <= '9') ? '0' : ('a' - 10))) << 4;
str[i * 2 + 1] = tolower(str[i * 2 + 1]);
rep->checksum[i] |= (str[i * 2 + 1] -
((str[i * 2 + 1] <= '9') ? '0' : ('a' - 10)));
}
return SVN_NO_ERROR;
}
static svn_error_t *
get_node_revision_body(node_revision_t **noderev_p,
svn_fs_t *fs,
const svn_fs_id_t *id,
apr_pool_t *pool) {
apr_file_t *revision_file;
apr_hash_t *headers;
node_revision_t *noderev;
char *value;
svn_error_t *err;
if (svn_fs_fs__id_txn_id(id)) {
err = svn_io_file_open(&revision_file, path_txn_node_rev(fs, id, pool),
APR_READ | APR_BUFFERED, APR_OS_DEFAULT, pool);
} else {
err = open_and_seek_revision(&revision_file, fs,
svn_fs_fs__id_rev(id),
svn_fs_fs__id_offset(id),
pool);
}
if (err) {
if (APR_STATUS_IS_ENOENT(err->apr_err)) {
svn_error_clear(err);
return svn_fs_fs__err_dangling_id(fs, id);
}
return err;
}
SVN_ERR(read_header_block(&headers, revision_file, pool) );
noderev = apr_pcalloc(pool, sizeof(*noderev));
value = apr_hash_get(headers, HEADER_ID, APR_HASH_KEY_STRING);
if (value == NULL)
return svn_error_create(SVN_ERR_FS_CORRUPT, NULL,
_("Missing id field in node-rev"));
SVN_ERR(svn_io_file_close(revision_file, pool));
noderev->id = svn_fs_fs__id_parse(value, strlen(value), pool);
value = apr_hash_get(headers, HEADER_TYPE, APR_HASH_KEY_STRING);
if ((value == NULL) ||
(strcmp(value, KIND_FILE) != 0 && strcmp(value, KIND_DIR)))
return svn_error_create(SVN_ERR_FS_CORRUPT, NULL,
_("Missing kind field in node-rev"));
noderev->kind = (strcmp(value, KIND_FILE) == 0) ? svn_node_file
: svn_node_dir;
value = apr_hash_get(headers, HEADER_COUNT, APR_HASH_KEY_STRING);
noderev->predecessor_count = (value == NULL) ? 0 : atoi(value);
value = apr_hash_get(headers, HEADER_PROPS, APR_HASH_KEY_STRING);
if (value) {
SVN_ERR(read_rep_offsets(&noderev->prop_rep, value,
svn_fs_fs__id_txn_id(id), TRUE, pool));
}
value = apr_hash_get(headers, HEADER_TEXT, APR_HASH_KEY_STRING);
if (value) {
SVN_ERR(read_rep_offsets(&noderev->data_rep, value,
svn_fs_fs__id_txn_id(id),
(noderev->kind == svn_node_dir), pool));
}
value = apr_hash_get(headers, HEADER_CPATH, APR_HASH_KEY_STRING);
if (value == NULL) {
return svn_error_create(SVN_ERR_FS_CORRUPT, NULL,
_("Missing cpath in node-rev"));
} else {
noderev->created_path = apr_pstrdup(pool, value);
}
value = apr_hash_get(headers, HEADER_PRED, APR_HASH_KEY_STRING);
if (value)
noderev->predecessor_id = svn_fs_fs__id_parse(value, strlen(value),
pool);
value = apr_hash_get(headers, HEADER_COPYROOT, APR_HASH_KEY_STRING);
if (value == NULL) {
noderev->copyroot_path = apr_pstrdup(pool, noderev->created_path);
noderev->copyroot_rev = svn_fs_fs__id_rev(noderev->id);
} else {
char *str, *last_str;
str = apr_strtok(value, " ", &last_str);
if (str == NULL)
return svn_error_create(SVN_ERR_FS_CORRUPT, NULL,
_("Malformed copyroot line in node-rev"));
noderev->copyroot_rev = atoi(str);
if (last_str == NULL)
return svn_error_create(SVN_ERR_FS_CORRUPT, NULL,
_("Malformed copyroot line in node-rev"));
noderev->copyroot_path = apr_pstrdup(pool, last_str);
}
value = apr_hash_get(headers, HEADER_COPYFROM, APR_HASH_KEY_STRING);
if (value == NULL) {
noderev->copyfrom_path = NULL;
noderev->copyfrom_rev = SVN_INVALID_REVNUM;
} else {
char *str, *last_str;
str = apr_strtok(value, " ", &last_str);
if (str == NULL)
return svn_error_create(SVN_ERR_FS_CORRUPT, NULL,
_("Malformed copyfrom line in node-rev"));
noderev->copyfrom_rev = atoi(str);
if (last_str == NULL)
return svn_error_create(SVN_ERR_FS_CORRUPT, NULL,
_("Malformed copyfrom line in node-rev"));
noderev->copyfrom_path = apr_pstrdup(pool, last_str);
}
value = apr_hash_get(headers, HEADER_FRESHTXNRT, APR_HASH_KEY_STRING);
noderev->is_fresh_txn_root = (value != NULL);
value = apr_hash_get(headers, HEADER_MINFO_CNT, APR_HASH_KEY_STRING);
noderev->mergeinfo_count = (value == NULL) ? 0 : apr_atoi64(value);
value = apr_hash_get(headers, HEADER_MINFO_HERE, APR_HASH_KEY_STRING);
noderev->has_mergeinfo = (value != NULL);
*noderev_p = noderev;
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_fs__get_node_revision(node_revision_t **noderev_p,
svn_fs_t *fs,
const svn_fs_id_t *id,
apr_pool_t *pool) {
svn_error_t *err = get_node_revision_body(noderev_p, fs, id, pool);
if (err && err->apr_err == SVN_ERR_FS_CORRUPT) {
svn_string_t *id_string = svn_fs_fs__id_unparse(id, pool);
return svn_error_createf(SVN_ERR_FS_CORRUPT, err,
"Corrupt node-revision '%s'",
id_string->data);
}
return err;
}
static const char *
representation_string(representation_t *rep,
svn_boolean_t mutable_rep_truncated, apr_pool_t *pool) {
if (rep->txn_id && mutable_rep_truncated)
return "-1";
else
return apr_psprintf(pool, "%ld %" APR_OFF_T_FMT " %" SVN_FILESIZE_T_FMT
" %" SVN_FILESIZE_T_FMT " %s",
rep->revision, rep->offset, rep->size,
rep->expanded_size,
svn_md5_digest_to_cstring_display(rep->checksum,
pool));
}
static svn_error_t *
write_noderev_txn(apr_file_t *file,
node_revision_t *noderev,
svn_boolean_t include_mergeinfo,
apr_pool_t *pool) {
svn_stream_t *outfile;
outfile = svn_stream_from_aprfile(file, pool);
SVN_ERR(svn_stream_printf(outfile, pool, HEADER_ID ": %s\n",
svn_fs_fs__id_unparse(noderev->id,
pool)->data));
SVN_ERR(svn_stream_printf(outfile, pool, HEADER_TYPE ": %s\n",
(noderev->kind == svn_node_file) ?
KIND_FILE : KIND_DIR));
if (noderev->predecessor_id)
SVN_ERR(svn_stream_printf(outfile, pool, HEADER_PRED ": %s\n",
svn_fs_fs__id_unparse(noderev->predecessor_id,
pool)->data));
SVN_ERR(svn_stream_printf(outfile, pool, HEADER_COUNT ": %d\n",
noderev->predecessor_count));
if (noderev->data_rep)
SVN_ERR(svn_stream_printf(outfile, pool, HEADER_TEXT ": %s\n",
representation_string(noderev->data_rep,
(noderev->kind
== svn_node_dir),
pool)));
if (noderev->prop_rep)
SVN_ERR(svn_stream_printf(outfile, pool, HEADER_PROPS ": %s\n",
representation_string(noderev->prop_rep, TRUE,
pool)));
SVN_ERR(svn_stream_printf(outfile, pool, HEADER_CPATH ": %s\n",
noderev->created_path));
if (noderev->copyfrom_path)
SVN_ERR(svn_stream_printf(outfile, pool, HEADER_COPYFROM ": %ld"
" %s\n",
noderev->copyfrom_rev,
noderev->copyfrom_path));
if ((noderev->copyroot_rev != svn_fs_fs__id_rev(noderev->id)) ||
(strcmp(noderev->copyroot_path, noderev->created_path) != 0))
SVN_ERR(svn_stream_printf(outfile, pool, HEADER_COPYROOT ": %ld"
" %s\n",
noderev->copyroot_rev,
noderev->copyroot_path));
if (noderev->is_fresh_txn_root)
SVN_ERR(svn_stream_printf(outfile, pool, HEADER_FRESHTXNRT ": y\n"));
if (include_mergeinfo) {
if (noderev->mergeinfo_count > 0)
SVN_ERR(svn_stream_printf(outfile, pool, HEADER_MINFO_CNT ": %"
APR_INT64_T_FMT "\n",
noderev->mergeinfo_count));
if (noderev->has_mergeinfo)
SVN_ERR(svn_stream_printf(outfile, pool, HEADER_MINFO_HERE ": y\n"));
}
SVN_ERR(svn_stream_printf(outfile, pool, "\n"));
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_fs__put_node_revision(svn_fs_t *fs,
const svn_fs_id_t *id,
node_revision_t *noderev,
svn_boolean_t fresh_txn_root,
apr_pool_t *pool) {
apr_file_t *noderev_file;
const char *txn_id = svn_fs_fs__id_txn_id(id);
noderev->is_fresh_txn_root = fresh_txn_root;
if (! txn_id)
return svn_error_create(SVN_ERR_FS_CORRUPT, NULL,
_("Attempted to write to non-transaction"));
SVN_ERR(svn_io_file_open(&noderev_file, path_txn_node_rev(fs, id, pool),
APR_WRITE | APR_CREATE | APR_TRUNCATE
| APR_BUFFERED, APR_OS_DEFAULT, pool));
SVN_ERR(write_noderev_txn(noderev_file, noderev,
svn_fs_fs__fs_supports_mergeinfo(fs),
pool));
SVN_ERR(svn_io_file_close(noderev_file, pool));
return SVN_NO_ERROR;
}
struct rep_args {
svn_boolean_t is_delta;
svn_boolean_t is_delta_vs_empty;
svn_revnum_t base_revision;
apr_off_t base_offset;
apr_size_t base_length;
};
static svn_error_t *
read_rep_line(struct rep_args **rep_args_p,
apr_file_t *file,
apr_pool_t *pool) {
char buffer[160];
apr_size_t limit;
struct rep_args *rep_args;
char *str, *last_str;
limit = sizeof(buffer);
SVN_ERR(svn_io_read_length_line(file, buffer, &limit, pool));
rep_args = apr_pcalloc(pool, sizeof(*rep_args));
rep_args->is_delta = FALSE;
if (strcmp(buffer, REP_PLAIN) == 0) {
*rep_args_p = rep_args;
return SVN_NO_ERROR;
}
if (strcmp(buffer, REP_DELTA) == 0) {
rep_args->is_delta = TRUE;
rep_args->is_delta_vs_empty = TRUE;
*rep_args_p = rep_args;
return SVN_NO_ERROR;
}
rep_args->is_delta = TRUE;
rep_args->is_delta_vs_empty = FALSE;
str = apr_strtok(buffer, " ", &last_str);
if (! str || (strcmp(str, REP_DELTA) != 0)) goto err;
str = apr_strtok(NULL, " ", &last_str);
if (! str) goto err;
rep_args->base_revision = atol(str);
str = apr_strtok(NULL, " ", &last_str);
if (! str) goto err;
rep_args->base_offset = (apr_off_t) apr_atoi64(str);
str = apr_strtok(NULL, " ", &last_str);
if (! str) goto err;
rep_args->base_length = (apr_size_t) apr_atoi64(str);
*rep_args_p = rep_args;
return SVN_NO_ERROR;
err:
return svn_error_create(SVN_ERR_FS_CORRUPT, NULL,
_("Malformed representation header"));
}
static svn_error_t *
get_fs_id_at_offset(svn_fs_id_t **id_p,
apr_file_t *rev_file,
apr_off_t offset,
apr_pool_t *pool) {
svn_fs_id_t *id;
apr_hash_t *headers;
const char *node_id_str;
SVN_ERR(svn_io_file_seek(rev_file, APR_SET, &offset, pool));
SVN_ERR(read_header_block(&headers, rev_file, pool));
node_id_str = apr_hash_get(headers, HEADER_ID, APR_HASH_KEY_STRING);
if (node_id_str == NULL)
return svn_error_create(SVN_ERR_FS_CORRUPT, NULL,
_("Missing node-id in node-rev"));
id = svn_fs_fs__id_parse(node_id_str, strlen(node_id_str), pool);
if (id == NULL)
return svn_error_create(SVN_ERR_FS_CORRUPT, NULL,
_("Corrupt node-id in node-rev"));
*id_p = id;
return SVN_NO_ERROR;
}
static svn_error_t *
get_root_changes_offset(apr_off_t *root_offset,
apr_off_t *changes_offset,
apr_file_t *rev_file,
apr_pool_t *pool) {
apr_off_t offset;
char buf[64];
int i, num_bytes;
apr_size_t len;
offset = 0;
SVN_ERR(svn_io_file_seek(rev_file, APR_END, &offset, pool));
offset -= sizeof(buf);
SVN_ERR(svn_io_file_seek(rev_file, APR_SET, &offset, pool));
len = sizeof(buf);
SVN_ERR(svn_io_file_read(rev_file, buf, &len, pool));
num_bytes = (int) len;
if (buf[num_bytes - 1] != '\n') {
return svn_error_createf(SVN_ERR_FS_CORRUPT, NULL,
_("Revision file lacks trailing newline"));
}
for (i = num_bytes - 2; i >= 0; i--) {
if (buf[i] == '\n') break;
}
if (i < 0) {
return svn_error_createf(SVN_ERR_FS_CORRUPT, NULL,
_("Final line in revision file longer than 64 "
"characters"));
}
i++;
if (root_offset)
*root_offset = apr_atoi64(&buf[i]);
for ( ; i < (num_bytes - 2) ; i++)
if (buf[i] == ' ') break;
if (i == (num_bytes - 2))
return svn_error_create(SVN_ERR_FS_CORRUPT, NULL,
_("Final line in revision file missing space"));
i++;
if (changes_offset)
*changes_offset = apr_atoi64(&buf[i]);
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_fs__rev_get_root(svn_fs_id_t **root_id_p,
svn_fs_t *fs,
svn_revnum_t rev,
apr_pool_t *pool) {
fs_fs_data_t *ffd = fs->fsap_data;
apr_file_t *revision_file;
apr_off_t root_offset;
svn_fs_id_t *root_id;
svn_error_t *err;
const char *rev_str = apr_psprintf(pool, "%ld", rev);
svn_fs_id_t *cached_id;
SVN_ERR(ensure_revision_exists(fs, rev, pool));
cached_id = apr_hash_get(ffd->rev_root_id_cache,
rev_str,
APR_HASH_KEY_STRING);
if (cached_id) {
*root_id_p = svn_fs_fs__id_copy(cached_id, pool);
return SVN_NO_ERROR;
}
err = svn_io_file_open(&revision_file, svn_fs_fs__path_rev(fs, rev, pool),
APR_READ | APR_BUFFERED, APR_OS_DEFAULT, pool);
if (err && APR_STATUS_IS_ENOENT(err->apr_err)) {
svn_error_clear(err);
return svn_error_createf(SVN_ERR_FS_NO_SUCH_REVISION, NULL,
_("No such revision %ld"), rev);
} else if (err)
return err;
SVN_ERR(get_root_changes_offset(&root_offset, NULL, revision_file, pool));
SVN_ERR(get_fs_id_at_offset(&root_id, revision_file, root_offset, pool));
SVN_ERR(svn_io_file_close(revision_file, pool));
if (apr_hash_count(ffd->rev_root_id_cache) >= NUM_RRI_CACHE_ENTRIES) {
svn_pool_clear(ffd->rev_root_id_cache_pool);
ffd->rev_root_id_cache = apr_hash_make(ffd->rev_root_id_cache_pool);
}
apr_hash_set(ffd->rev_root_id_cache,
apr_pstrdup(ffd->rev_root_id_cache_pool, rev_str),
APR_HASH_KEY_STRING,
svn_fs_fs__id_copy(root_id, ffd->rev_root_id_cache_pool));
*root_id_p = root_id;
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_fs__set_revision_proplist(svn_fs_t *fs,
svn_revnum_t rev,
apr_hash_t *proplist,
apr_pool_t *pool) {
const char *final_path = path_revprops(fs, rev, pool);
const char *tmp_path;
apr_file_t *f;
SVN_ERR(ensure_revision_exists(fs, rev, pool));
SVN_ERR(svn_io_open_unique_file2
(&f, &tmp_path, final_path, ".tmp", svn_io_file_del_none, pool));
SVN_ERR(svn_hash_write(proplist, f, pool));
SVN_ERR(svn_io_file_close(f, pool));
SVN_ERR(svn_fs_fs__move_into_place(tmp_path, final_path,
svn_fs_fs__path_rev(fs, rev, pool),
pool));
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_fs__revision_proplist(apr_hash_t **proplist_p,
svn_fs_t *fs,
svn_revnum_t rev,
apr_pool_t *pool) {
apr_file_t *revprop_file;
apr_hash_t *proplist;
svn_error_t *err = SVN_NO_ERROR;
int i;
apr_pool_t *iterpool;
SVN_ERR(ensure_revision_exists(fs, rev, pool));
proplist = apr_hash_make(pool);
iterpool = svn_pool_create(pool);
for (i = 0; i < SVN_ESTALE_RETRY_COUNT; i++) {
svn_pool_clear(iterpool);
svn_error_clear(err);
err = svn_io_file_open(&revprop_file, path_revprops(fs, rev, iterpool),
APR_READ | APR_BUFFERED, APR_OS_DEFAULT,
iterpool);
if (err) {
if (APR_STATUS_IS_ENOENT(err->apr_err)) {
svn_error_clear(err);
return svn_error_createf(SVN_ERR_FS_NO_SUCH_REVISION, NULL,
_("No such revision %ld"), rev);
}
#if defined(ESTALE)
else if (APR_TO_OS_ERROR(err->apr_err) == ESTALE)
continue;
#endif
return err;
}
SVN_ERR(svn_hash__clear(proplist));
SVN_RETRY_ESTALE(err,
svn_hash_read2(proplist,
svn_stream_from_aprfile(revprop_file,
iterpool),
SVN_HASH_TERMINATOR, pool));
SVN_IGNORE_ESTALE(err, svn_io_file_close(revprop_file, iterpool));
break;
}
if (err)
return err;
svn_pool_destroy(iterpool);
*proplist_p = proplist;
return SVN_NO_ERROR;
}
struct rep_state {
apr_file_t *file;
apr_off_t start;
apr_off_t off;
apr_off_t end;
int ver;
int chunk_index;
};
static svn_error_t *
create_rep_state_body(struct rep_state **rep_state,
struct rep_args **rep_args,
representation_t *rep,
svn_fs_t *fs,
apr_pool_t *pool) {
struct rep_state *rs = apr_pcalloc(pool, sizeof(*rs));
struct rep_args *ra;
unsigned char buf[4];
SVN_ERR(open_and_seek_representation(&rs->file, fs, rep, pool));
SVN_ERR(read_rep_line(&ra, rs->file, pool));
SVN_ERR(get_file_offset(&rs->start, rs->file, pool));
rs->off = rs->start;
rs->end = rs->start + rep->size;
*rep_state = rs;
*rep_args = ra;
if (ra->is_delta == FALSE)
return SVN_NO_ERROR;
SVN_ERR(svn_io_file_read_full(rs->file, buf, sizeof(buf), NULL, pool));
if (! ((buf[0] == 'S') && (buf[1] == 'V') && (buf[2] == 'N')))
return svn_error_create
(SVN_ERR_FS_CORRUPT, NULL,
_("Malformed svndiff data in representation"));
rs->ver = buf[3];
rs->chunk_index = 0;
rs->off += 4;
return SVN_NO_ERROR;
}
static svn_error_t *
create_rep_state(struct rep_state **rep_state,
struct rep_args **rep_args,
representation_t *rep,
svn_fs_t *fs,
apr_pool_t *pool) {
svn_error_t *err = create_rep_state_body(rep_state, rep_args, rep, fs, pool);
if (err && err->apr_err == SVN_ERR_FS_CORRUPT) {
return svn_error_createf(SVN_ERR_FS_CORRUPT, err,
"Corrupt representation '%s'",
representation_string(rep, TRUE, pool));
}
return err;
}
static svn_error_t *
build_rep_list(apr_array_header_t **list,
struct rep_state **src_state,
svn_fs_t *fs,
representation_t *first_rep,
apr_pool_t *pool) {
representation_t rep;
struct rep_state *rs;
struct rep_args *rep_args;
*list = apr_array_make(pool, 1, sizeof(struct rep_state *));
rep = *first_rep;
while (1) {
SVN_ERR(create_rep_state(&rs, &rep_args, &rep, fs, pool));
if (rep_args->is_delta == FALSE) {
*src_state = rs;
return SVN_NO_ERROR;
}
APR_ARRAY_PUSH(*list, struct rep_state *) = rs;
if (rep_args->is_delta_vs_empty) {
*src_state = NULL;
return SVN_NO_ERROR;
}
rep.revision = rep_args->base_revision;
rep.offset = rep_args->base_offset;
rep.size = rep_args->base_length;
rep.txn_id = NULL;
}
}
struct rep_read_baton {
svn_fs_t *fs;
apr_array_header_t *rs_list;
struct rep_state *src_state;
int chunk_index;
char *buf;
apr_size_t buf_pos;
apr_size_t buf_len;
struct apr_md5_ctx_t md5_context;
svn_boolean_t checksum_finalized;
unsigned char checksum[APR_MD5_DIGESTSIZE];
svn_filesize_t len;
svn_filesize_t off;
apr_pool_t *pool;
apr_pool_t *filehandle_pool;
};
static svn_error_t *
rep_read_get_baton(struct rep_read_baton **rb_p,
svn_fs_t *fs,
representation_t *rep,
apr_pool_t *pool) {
struct rep_read_baton *b;
b = apr_pcalloc(pool, sizeof(*b));
b->fs = fs;
b->chunk_index = 0;
b->buf = NULL;
apr_md5_init(&(b->md5_context));
b->checksum_finalized = FALSE;
memcpy(b->checksum, rep->checksum, sizeof(b->checksum));
b->len = rep->expanded_size;
b->off = 0;
b->pool = svn_pool_create(pool);
b->filehandle_pool = svn_pool_create(pool);
SVN_ERR(build_rep_list(&b->rs_list, &b->src_state, fs, rep,
b->filehandle_pool));
*rb_p = b;
return SVN_NO_ERROR;
}
static svn_error_t *
read_window(svn_txdelta_window_t **nwin, int this_chunk, struct rep_state *rs,
apr_pool_t *pool) {
svn_stream_t *stream;
assert(rs->chunk_index <= this_chunk);
while (rs->chunk_index < this_chunk) {
SVN_ERR(svn_txdelta_skip_svndiff_window(rs->file, rs->ver, pool));
rs->chunk_index++;
SVN_ERR(get_file_offset(&rs->off, rs->file, pool));
if (rs->off >= rs->end)
return svn_error_create(SVN_ERR_FS_CORRUPT, NULL,
_("Reading one svndiff window read "
"beyond the end of the "
"representation"));
}
stream = svn_stream_from_aprfile(rs->file, pool);
SVN_ERR(svn_txdelta_read_svndiff_window(nwin, stream, rs->ver, pool));
rs->chunk_index++;
SVN_ERR(get_file_offset(&rs->off, rs->file, pool));
if (rs->off > rs->end)
return svn_error_create(SVN_ERR_FS_CORRUPT, NULL,
_("Reading one svndiff window read beyond "
"the end of the representation"));
return SVN_NO_ERROR;
}
static svn_error_t *
get_combined_window(svn_txdelta_window_t **result,
struct rep_read_baton *rb) {
apr_pool_t *pool, *new_pool;
int i;
svn_txdelta_window_t *window, *nwin;
struct rep_state *rs;
assert(rb->rs_list->nelts >= 2);
pool = svn_pool_create(rb->pool);
rs = APR_ARRAY_IDX(rb->rs_list, 0, struct rep_state *);
SVN_ERR(read_window(&window, rb->chunk_index, rs, pool));
for (i = 1; i < rb->rs_list->nelts - 1; i++) {
if (window->src_ops == 0)
break;
rs = APR_ARRAY_IDX(rb->rs_list, i, struct rep_state *);
SVN_ERR(read_window(&nwin, rb->chunk_index, rs, pool));
new_pool = svn_pool_create(rb->pool);
window = svn_txdelta_compose_windows(nwin, window, new_pool);
svn_pool_destroy(pool);
pool = new_pool;
}
*result = window;
return SVN_NO_ERROR;
}
static svn_error_t *
rep_read_contents_close(void *baton) {
struct rep_read_baton *rb = baton;
svn_pool_destroy(rb->pool);
svn_pool_destroy(rb->filehandle_pool);
return SVN_NO_ERROR;
}
static svn_error_t *
get_contents(struct rep_read_baton *rb,
char *buf,
apr_size_t *len) {
apr_size_t copy_len, remaining = *len, tlen;
char *sbuf, *tbuf, *cur = buf;
struct rep_state *rs;
svn_txdelta_window_t *cwindow, *lwindow;
if (rb->rs_list->nelts == 0) {
copy_len = remaining;
rs = rb->src_state;
if (((apr_off_t) copy_len) > rs->end - rs->off)
copy_len = (apr_size_t) (rs->end - rs->off);
SVN_ERR(svn_io_file_read_full(rs->file, cur, copy_len, NULL,
rb->pool));
rs->off += copy_len;
*len = copy_len;
return SVN_NO_ERROR;
}
while (remaining > 0) {
if (rb->buf) {
copy_len = rb->buf_len - rb->buf_pos;
if (copy_len > remaining)
copy_len = remaining;
memcpy(cur, rb->buf + rb->buf_pos, copy_len);
rb->buf_pos += copy_len;
cur += copy_len;
remaining -= copy_len;
if (rb->buf_pos == rb->buf_len) {
svn_pool_clear(rb->pool);
rb->buf = NULL;
}
} else {
rs = APR_ARRAY_IDX(rb->rs_list, 0, struct rep_state *);
if (rs->off == rs->end)
break;
if (rb->rs_list->nelts > 1)
SVN_ERR(get_combined_window(&cwindow, rb));
else
cwindow = NULL;
if (!cwindow || cwindow->src_ops > 0) {
rs = APR_ARRAY_IDX(rb->rs_list, rb->rs_list->nelts - 1,
struct rep_state *);
SVN_ERR(read_window(&lwindow, rb->chunk_index, rs, rb->pool));
if (lwindow->src_ops > 0) {
if (! rb->src_state)
return svn_error_create(SVN_ERR_FS_CORRUPT, NULL,
_("svndiff data requested "
"non-existent source"));
rs = rb->src_state;
sbuf = apr_palloc(rb->pool, lwindow->sview_len);
if (! ((rs->start + lwindow->sview_offset) < rs->end))
return svn_error_create(SVN_ERR_FS_CORRUPT, NULL,
_("svndiff requested position "
"beyond end of stream"));
if ((rs->start + lwindow->sview_offset) != rs->off) {
rs->off = rs->start + lwindow->sview_offset;
SVN_ERR(svn_io_file_seek(rs->file, APR_SET, &rs->off,
rb->pool));
}
SVN_ERR(svn_io_file_read_full(rs->file, sbuf,
lwindow->sview_len,
NULL, rb->pool));
rs->off += lwindow->sview_len;
} else
sbuf = NULL;
tlen = lwindow->tview_len;
tbuf = apr_palloc(rb->pool, tlen);
svn_txdelta_apply_instructions(lwindow, sbuf, tbuf,
&tlen);
if (tlen != lwindow->tview_len)
return svn_error_create(SVN_ERR_FS_CORRUPT, NULL,
_("svndiff window length is "
"corrupt"));
sbuf = tbuf;
} else
sbuf = NULL;
rb->chunk_index++;
if (cwindow) {
rb->buf_len = cwindow->tview_len;
rb->buf = apr_palloc(rb->pool, rb->buf_len);
svn_txdelta_apply_instructions(cwindow, sbuf, rb->buf,
&rb->buf_len);
if (rb->buf_len != cwindow->tview_len)
return svn_error_create(SVN_ERR_FS_CORRUPT, NULL,
_("svndiff window length is "
"corrupt"));
} else {
rb->buf_len = lwindow->tview_len;
rb->buf = sbuf;
}
rb->buf_pos = 0;
}
}
*len = cur - buf;
return SVN_NO_ERROR;
}
static svn_error_t *
rep_read_contents(void *baton,
char *buf,
apr_size_t *len) {
struct rep_read_baton *rb = baton;
SVN_ERR(get_contents(rb, buf, len));
if (!rb->checksum_finalized) {
apr_md5_update(&rb->md5_context, buf, *len);
rb->off += *len;
if (rb->off == rb->len) {
unsigned char checksum[APR_MD5_DIGESTSIZE];
rb->checksum_finalized = TRUE;
apr_md5_final(checksum, &rb->md5_context);
if (! svn_md5_digests_match(checksum, rb->checksum))
return svn_error_createf
(SVN_ERR_FS_CORRUPT, NULL,
_("Checksum mismatch while reading representation:\n"
" expected: %s\n"
" actual: %s\n"),
svn_md5_digest_to_cstring_display(rb->checksum, rb->pool),
svn_md5_digest_to_cstring_display(checksum, rb->pool));
}
}
return SVN_NO_ERROR;
}
static svn_error_t *
read_representation(svn_stream_t **contents_p,
svn_fs_t *fs,
representation_t *rep,
apr_pool_t *pool) {
struct rep_read_baton *rb;
if (! rep) {
*contents_p = svn_stream_empty(pool);
} else {
SVN_ERR(rep_read_get_baton(&rb, fs, rep, pool));
*contents_p = svn_stream_create(rb, pool);
svn_stream_set_read(*contents_p, rep_read_contents);
svn_stream_set_close(*contents_p, rep_read_contents_close);
}
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_fs__get_contents(svn_stream_t **contents_p,
svn_fs_t *fs,
node_revision_t *noderev,
apr_pool_t *pool) {
return read_representation(contents_p, fs, noderev->data_rep, pool);
}
struct delta_read_baton {
struct rep_state *rs;
unsigned char checksum[APR_MD5_DIGESTSIZE];
};
static svn_error_t *
delta_read_next_window(svn_txdelta_window_t **window, void *baton,
apr_pool_t *pool) {
struct delta_read_baton *drb = baton;
if (drb->rs->off == drb->rs->end) {
*window = NULL;
return SVN_NO_ERROR;
}
SVN_ERR(read_window(window, drb->rs->chunk_index, drb->rs, pool));
return SVN_NO_ERROR;
}
static const unsigned char *
delta_read_md5_digest(void *baton) {
struct delta_read_baton *drb = baton;
return drb->checksum;
}
svn_error_t *
svn_fs_fs__get_file_delta_stream(svn_txdelta_stream_t **stream_p,
svn_fs_t *fs,
node_revision_t *source,
node_revision_t *target,
apr_pool_t *pool) {
svn_stream_t *source_stream, *target_stream;
if (source && source->data_rep && target->data_rep) {
struct rep_state *rep_state;
struct rep_args *rep_args;
SVN_ERR(create_rep_state(&rep_state, &rep_args, target->data_rep,
fs, pool));
if (rep_args->is_delta
&& (rep_args->is_delta_vs_empty
|| (rep_args->base_revision == source->data_rep->revision
&& rep_args->base_offset == source->data_rep->offset))) {
struct delta_read_baton *drb = apr_pcalloc(pool, sizeof(*drb));
drb->rs = rep_state;
memcpy(drb->checksum, target->data_rep->checksum,
sizeof(drb->checksum));
*stream_p = svn_txdelta_stream_create(drb, delta_read_next_window,
delta_read_md5_digest, pool);
return SVN_NO_ERROR;
} else
SVN_ERR(svn_io_file_close(rep_state->file, pool));
}
if (source)
SVN_ERR(read_representation(&source_stream, fs, source->data_rep, pool));
else
source_stream = svn_stream_empty(pool);
SVN_ERR(read_representation(&target_stream, fs, target->data_rep, pool));
svn_txdelta(stream_p, source_stream, target_stream, pool);
return SVN_NO_ERROR;
}
static svn_error_t *
get_dir_contents(apr_hash_t *entries,
svn_fs_t *fs,
node_revision_t *noderev,
apr_pool_t *pool) {
svn_stream_t *contents;
if (noderev->data_rep && noderev->data_rep->txn_id) {
apr_file_t *dir_file;
const char *filename = path_txn_node_children(fs, noderev->id, pool);
SVN_ERR(svn_io_file_open(&dir_file, filename, APR_READ | APR_BUFFERED,
APR_OS_DEFAULT, pool));
contents = svn_stream_from_aprfile(dir_file, pool);
SVN_ERR(svn_hash_read2(entries, contents, SVN_HASH_TERMINATOR, pool));
SVN_ERR(svn_hash_read_incremental(entries, contents, NULL, pool));
SVN_ERR(svn_io_file_close(dir_file, pool));
} else if (noderev->data_rep) {
SVN_ERR(read_representation(&contents, fs, noderev->data_rep, pool));
SVN_ERR(svn_hash_read2(entries, contents, SVN_HASH_TERMINATOR, pool));
SVN_ERR(svn_stream_close(contents));
}
return SVN_NO_ERROR;
}
static apr_hash_t *
copy_dir_entries(apr_hash_t *entries,
apr_pool_t *pool) {
apr_hash_t *new_entries = apr_hash_make(pool);
apr_hash_index_t *hi;
for (hi = apr_hash_first(pool, entries); hi; hi = apr_hash_next(hi)) {
void *val;
svn_fs_dirent_t *dirent, *new_dirent;
apr_hash_this(hi, NULL, NULL, &val);
dirent = val;
new_dirent = apr_palloc(pool, sizeof(*new_dirent));
new_dirent->name = apr_pstrdup(pool, dirent->name);
new_dirent->kind = dirent->kind;
new_dirent->id = svn_fs_fs__id_copy(dirent->id, pool);
apr_hash_set(new_entries, new_dirent->name, APR_HASH_KEY_STRING,
new_dirent);
}
return new_entries;
}
svn_error_t *
svn_fs_fs__rep_contents_dir(apr_hash_t **entries_p,
svn_fs_t *fs,
node_revision_t *noderev,
apr_pool_t *pool) {
fs_fs_data_t *ffd = fs->fsap_data;
apr_hash_t *unparsed_entries, *parsed_entries;
apr_hash_index_t *hi;
unsigned int hid;
hid = DIR_CACHE_ENTRIES_MASK(svn_fs_fs__id_rev(noderev->id));
if (! svn_fs_fs__id_txn_id(noderev->id) &&
ffd->dir_cache_id[hid] && svn_fs_fs__id_eq(ffd->dir_cache_id[hid],
noderev->id)) {
*entries_p = copy_dir_entries(ffd->dir_cache[hid], pool);
return SVN_NO_ERROR;
}
unparsed_entries = apr_hash_make(pool);
SVN_ERR(get_dir_contents(unparsed_entries, fs, noderev, pool));
parsed_entries = apr_hash_make(pool);
for (hi = apr_hash_first(pool, unparsed_entries); hi; hi = apr_hash_next(hi)) {
const void *key;
void *val;
char *str_val;
char *str, *last_str;
svn_fs_dirent_t *dirent = apr_pcalloc(pool, sizeof(*dirent));
apr_hash_this(hi, &key, NULL, &val);
str_val = apr_pstrdup(pool, *((char **)val));
dirent->name = apr_pstrdup(pool, key);
str = apr_strtok(str_val, " ", &last_str);
if (str == NULL)
return svn_error_create(SVN_ERR_FS_CORRUPT, NULL,
_("Directory entry corrupt"));
if (strcmp(str, KIND_FILE) == 0) {
dirent->kind = svn_node_file;
} else if (strcmp(str, KIND_DIR) == 0) {
dirent->kind = svn_node_dir;
} else {
return svn_error_create(SVN_ERR_FS_CORRUPT, NULL,
_("Directory entry corrupt"));
}
str = apr_strtok(NULL, " ", &last_str);
if (str == NULL)
return svn_error_create(SVN_ERR_FS_CORRUPT, NULL,
_("Directory entry corrupt"));
dirent->id = svn_fs_fs__id_parse(str, strlen(str), pool);
apr_hash_set(parsed_entries, dirent->name, APR_HASH_KEY_STRING, dirent);
}
if (! svn_fs_fs__id_txn_id(noderev->id)) {
ffd->dir_cache_id[hid] = NULL;
if (ffd->dir_cache_pool[hid])
svn_pool_clear(ffd->dir_cache_pool[hid]);
else
ffd->dir_cache_pool[hid] = svn_pool_create(fs->pool);
ffd->dir_cache[hid] = copy_dir_entries(parsed_entries,
ffd->dir_cache_pool[hid]);
ffd->dir_cache_id[hid] = svn_fs_fs__id_copy(noderev->id,
ffd->dir_cache_pool[hid]);
}
*entries_p = parsed_entries;
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_fs__get_proplist(apr_hash_t **proplist_p,
svn_fs_t *fs,
node_revision_t *noderev,
apr_pool_t *pool) {
apr_hash_t *proplist;
svn_stream_t *stream;
proplist = apr_hash_make(pool);
if (noderev->prop_rep && noderev->prop_rep->txn_id) {
apr_file_t *props_file;
const char *filename = path_txn_node_props(fs, noderev->id, pool);
SVN_ERR(svn_io_file_open(&props_file, filename,
APR_READ | APR_BUFFERED, APR_OS_DEFAULT,
pool));
stream = svn_stream_from_aprfile(props_file, pool);
SVN_ERR(svn_hash_read2(proplist, stream, SVN_HASH_TERMINATOR, pool));
SVN_ERR(svn_io_file_close(props_file, pool));
} else if (noderev->prop_rep) {
SVN_ERR(read_representation(&stream, fs, noderev->prop_rep, pool));
SVN_ERR(svn_hash_read2(proplist, stream, SVN_HASH_TERMINATOR, pool));
SVN_ERR(svn_stream_close(stream));
}
*proplist_p = proplist;
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_fs__file_length(svn_filesize_t *length,
node_revision_t *noderev,
apr_pool_t *pool) {
if (noderev->data_rep)
*length = noderev->data_rep->expanded_size;
else
*length = 0;
return SVN_NO_ERROR;
}
svn_boolean_t
svn_fs_fs__noderev_same_rep_key(representation_t *a,
representation_t *b) {
if (a == b)
return TRUE;
if (a && (! b))
return FALSE;
if (b && (! a))
return FALSE;
if (a->offset != b->offset)
return FALSE;
if (a->revision != b->revision)
return FALSE;
return TRUE;
}
svn_error_t *
svn_fs_fs__file_checksum(unsigned char digest[],
node_revision_t *noderev,
apr_pool_t *pool) {
if (noderev->data_rep)
memcpy(digest, noderev->data_rep->checksum, APR_MD5_DIGESTSIZE);
else
memset(digest, 0, APR_MD5_DIGESTSIZE);
return SVN_NO_ERROR;
}
representation_t *
svn_fs_fs__rep_copy(representation_t *rep,
apr_pool_t *pool) {
representation_t *rep_new;
if (rep == NULL)
return NULL;
rep_new = apr_pcalloc(pool, sizeof(*rep_new));
memcpy(rep_new, rep, sizeof(*rep_new));
return rep_new;
}
static svn_error_t *
fold_change(apr_hash_t *changes,
const change_t *change,
apr_hash_t *copyfrom_hash) {
apr_pool_t *pool = apr_hash_pool_get(changes);
apr_pool_t *copyfrom_pool = apr_hash_pool_get(copyfrom_hash);
svn_fs_path_change_t *old_change, *new_change;
const char *path, *copyfrom_string, *copyfrom_path = NULL;
if ((old_change = apr_hash_get(changes, change->path, APR_HASH_KEY_STRING))) {
copyfrom_string = apr_hash_get(copyfrom_hash, change->path,
APR_HASH_KEY_STRING);
if (copyfrom_string)
copyfrom_path = change->path;
path = change->path;
if ((! change->noderev_id) && (change->kind != svn_fs_path_change_reset))
return svn_error_create
(SVN_ERR_FS_CORRUPT, NULL,
_("Missing required node revision ID"));
if (change->noderev_id
&& (! svn_fs_fs__id_eq(old_change->node_rev_id, change->noderev_id))
&& (old_change->change_kind != svn_fs_path_change_delete))
return svn_error_create
(SVN_ERR_FS_CORRUPT, NULL,
_("Invalid change ordering: new node revision ID "
"without delete"));
if ((old_change->change_kind == svn_fs_path_change_delete)
&& (! ((change->kind == svn_fs_path_change_replace)
|| (change->kind == svn_fs_path_change_reset)
|| (change->kind == svn_fs_path_change_add))))
return svn_error_create
(SVN_ERR_FS_CORRUPT, NULL,
_("Invalid change ordering: non-add change on deleted path"));
switch (change->kind) {
case svn_fs_path_change_reset:
old_change = NULL;
copyfrom_string = NULL;
break;
case svn_fs_path_change_delete:
if (old_change->change_kind == svn_fs_path_change_add) {
old_change = NULL;
} else {
old_change->change_kind = svn_fs_path_change_delete;
old_change->text_mod = change->text_mod;
old_change->prop_mod = change->prop_mod;
}
copyfrom_string = NULL;
break;
case svn_fs_path_change_add:
case svn_fs_path_change_replace:
old_change->change_kind = svn_fs_path_change_replace;
old_change->node_rev_id = svn_fs_fs__id_copy(change->noderev_id,
pool);
old_change->text_mod = change->text_mod;
old_change->prop_mod = change->prop_mod;
if (change->copyfrom_rev == SVN_INVALID_REVNUM)
copyfrom_string = apr_pstrdup(copyfrom_pool, "");
else {
copyfrom_string = apr_psprintf(copyfrom_pool,
"%ld %s",
change->copyfrom_rev,
change->copyfrom_path);
}
break;
case svn_fs_path_change_modify:
default:
if (change->text_mod)
old_change->text_mod = TRUE;
if (change->prop_mod)
old_change->prop_mod = TRUE;
break;
}
new_change = old_change;
} else {
new_change = apr_pcalloc(pool, sizeof(*new_change));
new_change->node_rev_id = svn_fs_fs__id_copy(change->noderev_id, pool);
new_change->change_kind = change->kind;
new_change->text_mod = change->text_mod;
new_change->prop_mod = change->prop_mod;
if (change->copyfrom_rev != SVN_INVALID_REVNUM) {
copyfrom_string = apr_psprintf(copyfrom_pool, "%ld %s",
change->copyfrom_rev,
change->copyfrom_path);
} else
copyfrom_string = apr_pstrdup(copyfrom_pool, "");
path = apr_pstrdup(pool, change->path);
}
apr_hash_set(changes, path, APR_HASH_KEY_STRING, new_change);
if (! copyfrom_path) {
copyfrom_path = copyfrom_string ? apr_pstrdup(copyfrom_pool, path)
: path;
}
apr_hash_set(copyfrom_hash, copyfrom_path, APR_HASH_KEY_STRING,
copyfrom_string);
return SVN_NO_ERROR;
}
#define MAX_CHANGE_LINE_LEN FSFS_MAX_PATH_LEN + 256
static svn_error_t *
read_change(change_t **change_p,
apr_file_t *file,
apr_pool_t *pool) {
char buf[MAX_CHANGE_LINE_LEN];
apr_size_t len = sizeof(buf);
change_t *change;
char *str, *last_str;
svn_error_t *err;
*change_p = NULL;
err = svn_io_read_length_line(file, buf, &len, pool);
if (err || (len == 0)) {
if (err && APR_STATUS_IS_EOF(err->apr_err)) {
svn_error_clear(err);
return SVN_NO_ERROR;
}
if ((len == 0) && (! err))
return SVN_NO_ERROR;
return err;
}
change = apr_pcalloc(pool, sizeof(*change));
str = apr_strtok(buf, " ", &last_str);
if (str == NULL)
return svn_error_create(SVN_ERR_FS_CORRUPT, NULL,
_("Invalid changes line in rev-file"));
change->noderev_id = svn_fs_fs__id_parse(str, strlen(str), pool);
str = apr_strtok(NULL, " ", &last_str);
if (str == NULL)
return svn_error_create(SVN_ERR_FS_CORRUPT, NULL,
_("Invalid changes line in rev-file"));
if (strcmp(str, ACTION_MODIFY) == 0) {
change->kind = svn_fs_path_change_modify;
} else if (strcmp(str, ACTION_ADD) == 0) {
change->kind = svn_fs_path_change_add;
} else if (strcmp(str, ACTION_DELETE) == 0) {
change->kind = svn_fs_path_change_delete;
} else if (strcmp(str, ACTION_REPLACE) == 0) {
change->kind = svn_fs_path_change_replace;
} else if (strcmp(str, ACTION_RESET) == 0) {
change->kind = svn_fs_path_change_reset;
} else {
return svn_error_create(SVN_ERR_FS_CORRUPT, NULL,
_("Invalid change kind in rev file"));
}
str = apr_strtok(NULL, " ", &last_str);
if (str == NULL)
return svn_error_create(SVN_ERR_FS_CORRUPT, NULL,
_("Invalid changes line in rev-file"));
if (strcmp(str, FLAG_TRUE) == 0) {
change->text_mod = TRUE;
} else if (strcmp(str, FLAG_FALSE) == 0) {
change->text_mod = FALSE;
} else {
return svn_error_create(SVN_ERR_FS_CORRUPT, NULL,
_("Invalid text-mod flag in rev-file"));
}
str = apr_strtok(NULL, " ", &last_str);
if (str == NULL)
return svn_error_create(SVN_ERR_FS_CORRUPT, NULL,
_("Invalid changes line in rev-file"));
if (strcmp(str, FLAG_TRUE) == 0) {
change->prop_mod = TRUE;
} else if (strcmp(str, FLAG_FALSE) == 0) {
change->prop_mod = FALSE;
} else {
return svn_error_create(SVN_ERR_FS_CORRUPT, NULL,
_("Invalid prop-mod flag in rev-file"));
}
change->path = apr_pstrdup(pool, last_str);
len = sizeof(buf);
SVN_ERR(svn_io_read_length_line(file, buf, &len, pool));
if (len == 0) {
change->copyfrom_rev = SVN_INVALID_REVNUM;
change->copyfrom_path = NULL;
} else {
str = apr_strtok(buf, " ", &last_str);
if (! str)
return svn_error_create(SVN_ERR_FS_CORRUPT, NULL,
_("Invalid changes line in rev-file"));
change->copyfrom_rev = atol(str);
if (! last_str)
return svn_error_create(SVN_ERR_FS_CORRUPT, NULL,
_("Invalid changes line in rev-file"));
change->copyfrom_path = apr_pstrdup(pool, last_str);
}
*change_p = change;
return SVN_NO_ERROR;
}
static svn_error_t *
fetch_all_changes(apr_hash_t *changed_paths,
apr_hash_t *copyfrom_hash,
apr_file_t *file,
svn_boolean_t prefolded,
apr_pool_t *pool) {
change_t *change;
apr_pool_t *iterpool = svn_pool_create(pool);
apr_hash_t *my_hash;
my_hash = copyfrom_hash ? copyfrom_hash : apr_hash_make(pool);
SVN_ERR(read_change(&change, file, iterpool));
while (change) {
SVN_ERR(fold_change(changed_paths, change, my_hash));
if (((change->kind == svn_fs_path_change_delete)
|| (change->kind == svn_fs_path_change_replace))
&& ! prefolded) {
apr_hash_index_t *hi;
for (hi = apr_hash_first(iterpool, changed_paths);
hi;
hi = apr_hash_next(hi)) {
const void *hashkey;
apr_ssize_t klen;
apr_hash_this(hi, &hashkey, &klen, NULL);
if (strcmp(change->path, hashkey) == 0)
continue;
if (svn_path_is_child(change->path, hashkey, iterpool))
apr_hash_set(changed_paths, hashkey, klen, NULL);
}
}
svn_pool_clear(iterpool);
SVN_ERR(read_change(&change, file, iterpool));
}
svn_pool_destroy(iterpool);
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_fs__txn_changes_fetch(apr_hash_t **changed_paths_p,
svn_fs_t *fs,
const char *txn_id,
apr_hash_t *copyfrom_cache,
apr_pool_t *pool) {
apr_file_t *file;
apr_hash_t *changed_paths = apr_hash_make(pool);
SVN_ERR(svn_io_file_open(&file, path_txn_changes(fs, txn_id, pool),
APR_READ | APR_BUFFERED, APR_OS_DEFAULT, pool));
SVN_ERR(fetch_all_changes(changed_paths, copyfrom_cache, file, FALSE,
pool));
SVN_ERR(svn_io_file_close(file, pool));
*changed_paths_p = changed_paths;
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_fs__paths_changed(apr_hash_t **changed_paths_p,
svn_fs_t *fs,
svn_revnum_t rev,
apr_hash_t *copyfrom_cache,
apr_pool_t *pool) {
apr_off_t changes_offset;
apr_hash_t *changed_paths;
apr_file_t *revision_file;
SVN_ERR(ensure_revision_exists(fs, rev, pool));
SVN_ERR(svn_io_file_open(&revision_file, svn_fs_fs__path_rev(fs, rev, pool),
APR_READ | APR_BUFFERED, APR_OS_DEFAULT, pool));
SVN_ERR(get_root_changes_offset(NULL, &changes_offset, revision_file,
pool));
SVN_ERR(svn_io_file_seek(revision_file, APR_SET, &changes_offset, pool));
changed_paths = apr_hash_make(pool);
SVN_ERR(fetch_all_changes(changed_paths, copyfrom_cache, revision_file,
TRUE, pool));
SVN_ERR(svn_io_file_close(revision_file, pool));
*changed_paths_p = changed_paths;
return SVN_NO_ERROR;
}
static svn_error_t *
create_new_txn_noderev_from_rev(svn_fs_t *fs,
const char *txn_id,
svn_fs_id_t *src,
apr_pool_t *pool) {
node_revision_t *noderev;
const char *node_id, *copy_id;
SVN_ERR(svn_fs_fs__get_node_revision(&noderev, fs, src, pool));
if (svn_fs_fs__id_txn_id(noderev->id))
return svn_error_create(SVN_ERR_FS_CORRUPT, NULL,
_("Copying from transactions not allowed"));
noderev->predecessor_id = noderev->id;
noderev->predecessor_count++;
noderev->copyfrom_path = NULL;
noderev->copyfrom_rev = SVN_INVALID_REVNUM;
node_id = svn_fs_fs__id_node_id(noderev->id);
copy_id = svn_fs_fs__id_copy_id(noderev->id);
noderev->id = svn_fs_fs__id_txn_create(node_id, copy_id, txn_id, pool);
SVN_ERR(svn_fs_fs__put_node_revision(fs, noderev->id, noderev, TRUE, pool));
return SVN_NO_ERROR;
}
struct get_and_increment_txn_key_baton {
svn_fs_t *fs;
char *txn_id;
apr_pool_t *pool;
};
static svn_error_t *
get_and_increment_txn_key_body(void *baton, apr_pool_t *pool) {
struct get_and_increment_txn_key_baton *cb = baton;
const char *txn_current_filename = path_txn_current(cb->fs, pool);
apr_file_t *txn_current_file;
const char *tmp_filename;
char next_txn_id[MAX_KEY_SIZE+3];
svn_error_t *err = SVN_NO_ERROR;
apr_pool_t *iterpool;
apr_size_t len;
int i;
cb->txn_id = apr_palloc(cb->pool, MAX_KEY_SIZE);
iterpool = svn_pool_create(pool);
for (i = 0; i < SVN_ESTALE_RETRY_COUNT; ++i) {
svn_pool_clear(iterpool);
SVN_RETRY_ESTALE(err, svn_io_file_open(&txn_current_file,
txn_current_filename,
APR_READ | APR_BUFFERED,
APR_OS_DEFAULT, iterpool));
len = MAX_KEY_SIZE;
SVN_RETRY_ESTALE(err, svn_io_read_length_line(txn_current_file,
cb->txn_id,
&len,
iterpool));
SVN_IGNORE_ESTALE(err, svn_io_file_close(txn_current_file, iterpool));
break;
}
if (err)
return err;
svn_pool_destroy(iterpool);
svn_fs_fs__next_key(cb->txn_id, &len, next_txn_id);
next_txn_id[len] = '\n';
++len;
next_txn_id[len] = '\0';
SVN_ERR(svn_io_open_unique_file2(&txn_current_file, &tmp_filename,
txn_current_filename, ".tmp",
svn_io_file_del_none, pool));
SVN_ERR(svn_io_file_write_full(txn_current_file,
next_txn_id,
len,
NULL,
pool));
SVN_ERR(svn_io_file_flush_to_disk(txn_current_file, pool));
SVN_ERR(svn_io_file_close(txn_current_file, pool));
SVN_ERR(svn_fs_fs__move_into_place(tmp_filename, txn_current_filename,
txn_current_filename, pool));
return err;
}
static svn_error_t *
create_txn_dir(const char **id_p, svn_fs_t *fs, svn_revnum_t rev,
apr_pool_t *pool) {
struct get_and_increment_txn_key_baton cb;
const char *txn_dir;
cb.pool = pool;
cb.fs = fs;
SVN_ERR(with_txn_current_lock(fs,
get_and_increment_txn_key_body,
&cb,
pool));
*id_p = apr_psprintf(pool, "%ld-%s", rev, cb.txn_id);
txn_dir = svn_path_join_many(pool,
fs->path,
PATH_TXNS_DIR,
apr_pstrcat(pool, *id_p, PATH_EXT_TXN, NULL),
NULL);
SVN_ERR(svn_io_dir_make(txn_dir, APR_OS_DEFAULT, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
create_txn_dir_pre_1_5(const char **id_p, svn_fs_t *fs, svn_revnum_t rev,
apr_pool_t *pool) {
unsigned int i;
apr_pool_t *subpool;
const char *unique_path, *prefix;
prefix = svn_path_join_many(pool, fs->path, PATH_TXNS_DIR,
apr_psprintf(pool, "%ld", rev), NULL);
subpool = svn_pool_create(pool);
for (i = 1; i <= 99999; i++) {
svn_error_t *err;
svn_pool_clear(subpool);
unique_path = apr_psprintf(subpool, "%s-%u" PATH_EXT_TXN, prefix, i);
err = svn_io_dir_make(unique_path, APR_OS_DEFAULT, subpool);
if (! err) {
const char *name = svn_path_basename(unique_path, subpool);
*id_p = apr_pstrndup(pool, name,
strlen(name) - strlen(PATH_EXT_TXN));
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
if (! APR_STATUS_IS_EEXIST(err->apr_err))
return err;
svn_error_clear(err);
}
return svn_error_createf(SVN_ERR_IO_UNIQUE_NAMES_EXHAUSTED,
NULL,
_("Unable to create transaction directory "
"in '%s' for revision %ld"),
fs->path, rev);
}
svn_error_t *
svn_fs_fs__create_txn(svn_fs_txn_t **txn_p,
svn_fs_t *fs,
svn_revnum_t rev,
apr_pool_t *pool) {
fs_fs_data_t *ffd = fs->fsap_data;
svn_fs_txn_t *txn;
svn_fs_id_t *root_id;
txn = apr_pcalloc(pool, sizeof(*txn));
if (ffd->format >= SVN_FS_FS__MIN_TXN_CURRENT_FORMAT)
SVN_ERR(create_txn_dir(&txn->id, fs, rev, pool));
else
SVN_ERR(create_txn_dir_pre_1_5(&txn->id, fs, rev, pool));
txn->fs = fs;
txn->base_rev = rev;
txn->vtable = &txn_vtable;
*txn_p = txn;
SVN_ERR(svn_fs_fs__rev_get_root(&root_id, fs, rev, pool));
SVN_ERR(create_new_txn_noderev_from_rev(fs, txn->id, root_id, pool));
SVN_ERR(svn_io_file_create(path_txn_proto_rev(fs, txn->id, pool), "",
pool));
SVN_ERR(svn_io_file_create(path_txn_proto_rev_lock(fs, txn->id, pool), "",
pool));
SVN_ERR(svn_io_file_create(path_txn_changes(fs, txn->id, pool), "",
pool));
SVN_ERR(svn_io_file_create(path_txn_next_ids(fs, txn->id, pool), "0 0\n",
pool));
return SVN_NO_ERROR;
}
static svn_error_t *
get_txn_proplist(apr_hash_t *proplist,
svn_fs_t *fs,
const char *txn_id,
apr_pool_t *pool) {
apr_file_t *txn_prop_file;
SVN_ERR(svn_io_file_open(&txn_prop_file, path_txn_props(fs, txn_id, pool),
APR_READ | APR_BUFFERED,
APR_OS_DEFAULT, pool));
SVN_ERR(svn_hash_read2(proplist,
svn_stream_from_aprfile(txn_prop_file, pool),
SVN_HASH_TERMINATOR, pool));
SVN_ERR(svn_io_file_close(txn_prop_file, pool));
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_fs__change_txn_prop(svn_fs_txn_t *txn,
const char *name,
const svn_string_t *value,
apr_pool_t *pool) {
apr_array_header_t *props = apr_array_make(pool, 1, sizeof(svn_prop_t));
svn_prop_t prop;
prop.name = name;
prop.value = value;
APR_ARRAY_PUSH(props, svn_prop_t) = prop;
return svn_fs_fs__change_txn_props(txn, props, pool);
}
svn_error_t *
svn_fs_fs__change_txn_props(svn_fs_txn_t *txn,
apr_array_header_t *props,
apr_pool_t *pool) {
apr_file_t *txn_prop_file;
apr_hash_t *txn_prop = apr_hash_make(pool);
int i;
svn_error_t *err;
err = get_txn_proplist(txn_prop, txn->fs, txn->id, pool);
if (err && (APR_STATUS_IS_ENOENT(err->apr_err)))
svn_error_clear(err);
else if (err)
return err;
for (i = 0; i < props->nelts; i++) {
svn_prop_t *prop = &APR_ARRAY_IDX(props, i, svn_prop_t);
apr_hash_set(txn_prop, prop->name, APR_HASH_KEY_STRING, prop->value);
}
SVN_ERR(svn_io_file_open(&txn_prop_file,
path_txn_props(txn->fs, txn->id, pool),
APR_WRITE | APR_CREATE | APR_TRUNCATE
| APR_BUFFERED, APR_OS_DEFAULT, pool));
SVN_ERR(svn_hash_write(txn_prop, txn_prop_file, pool));
SVN_ERR(svn_io_file_close(txn_prop_file, pool));
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_fs__get_txn(transaction_t **txn_p,
svn_fs_t *fs,
const char *txn_id,
apr_pool_t *pool) {
transaction_t *txn;
node_revision_t *noderev;
svn_fs_id_t *root_id;
txn = apr_pcalloc(pool, sizeof(*txn));
txn->proplist = apr_hash_make(pool);
SVN_ERR(get_txn_proplist(txn->proplist, fs, txn_id, pool));
root_id = svn_fs_fs__id_txn_create("0", "0", txn_id, pool);
SVN_ERR(svn_fs_fs__get_node_revision(&noderev, fs, root_id, pool));
txn->root_id = svn_fs_fs__id_copy(noderev->id, pool);
txn->base_id = svn_fs_fs__id_copy(noderev->predecessor_id, pool);
txn->copies = NULL;
*txn_p = txn;
return SVN_NO_ERROR;
}
static svn_error_t *
write_next_ids(svn_fs_t *fs,
const char *txn_id,
const char *node_id,
const char *copy_id,
apr_pool_t *pool) {
apr_file_t *file;
svn_stream_t *out_stream;
SVN_ERR(svn_io_file_open(&file, path_txn_next_ids(fs, txn_id, pool),
APR_WRITE | APR_TRUNCATE,
APR_OS_DEFAULT, pool));
out_stream = svn_stream_from_aprfile(file, pool);
SVN_ERR(svn_stream_printf(out_stream, pool, "%s %s\n", node_id, copy_id));
SVN_ERR(svn_stream_close(out_stream));
SVN_ERR(svn_io_file_close(file, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
read_next_ids(const char **node_id,
const char **copy_id,
svn_fs_t *fs,
const char *txn_id,
apr_pool_t *pool) {
apr_file_t *file;
char buf[MAX_KEY_SIZE*2+3];
apr_size_t limit;
char *str, *last_str;
SVN_ERR(svn_io_file_open(&file, path_txn_next_ids(fs, txn_id, pool),
APR_READ | APR_BUFFERED, APR_OS_DEFAULT, pool));
limit = sizeof(buf);
SVN_ERR(svn_io_read_length_line(file, buf, &limit, pool));
SVN_ERR(svn_io_file_close(file, pool));
str = apr_strtok(buf, " ", &last_str);
if (! str)
return svn_error_create(SVN_ERR_FS_CORRUPT, NULL,
_("next-id file corrupt"));
*node_id = apr_pstrdup(pool, str);
str = apr_strtok(NULL, " ", &last_str);
if (! str)
return svn_error_create(SVN_ERR_FS_CORRUPT, NULL,
_("next-id file corrupt"));
*copy_id = apr_pstrdup(pool, str);
return SVN_NO_ERROR;
}
static svn_error_t *
get_new_txn_node_id(const char **node_id_p,
svn_fs_t *fs,
const char *txn_id,
apr_pool_t *pool) {
const char *cur_node_id, *cur_copy_id;
char *node_id;
apr_size_t len;
SVN_ERR(read_next_ids(&cur_node_id, &cur_copy_id, fs, txn_id, pool));
node_id = apr_pcalloc(pool, strlen(cur_node_id) + 2);
len = strlen(cur_node_id);
svn_fs_fs__next_key(cur_node_id, &len, node_id);
SVN_ERR(write_next_ids(fs, txn_id, node_id, cur_copy_id, pool));
*node_id_p = apr_pstrcat(pool, "_", cur_node_id, NULL);
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_fs__create_node(const svn_fs_id_t **id_p,
svn_fs_t *fs,
node_revision_t *noderev,
const char *copy_id,
const char *txn_id,
apr_pool_t *pool) {
const char *node_id;
const svn_fs_id_t *id;
SVN_ERR(get_new_txn_node_id(&node_id, fs, txn_id, pool));
id = svn_fs_fs__id_txn_create(node_id, copy_id, txn_id, pool);
noderev->id = id;
SVN_ERR(svn_fs_fs__put_node_revision(fs, noderev->id, noderev, FALSE, pool));
*id_p = id;
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_fs__purge_txn(svn_fs_t *fs,
const char *txn_id,
apr_pool_t *pool) {
fs_fs_data_t *ffd = fs->fsap_data;
SVN_ERR(purge_shared_txn(fs, txn_id, pool));
SVN_ERR(svn_io_remove_dir2(path_txn_dir(fs, txn_id, pool), FALSE,
NULL, NULL, pool));
if (ffd->format >= SVN_FS_FS__MIN_PROTOREVS_DIR_FORMAT) {
svn_error_t *err = svn_io_remove_file(path_txn_proto_rev(fs, txn_id,
pool), pool);
if (err && APR_STATUS_IS_ENOENT(err->apr_err)) {
svn_error_clear(err);
err = NULL;
}
if (err)
return err;
err = svn_io_remove_file(path_txn_proto_rev_lock(fs, txn_id, pool),
pool);
if (err && APR_STATUS_IS_ENOENT(err->apr_err)) {
svn_error_clear(err);
err = NULL;
}
if (err)
return err;
}
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_fs__abort_txn(svn_fs_txn_t *txn,
apr_pool_t *pool) {
fs_fs_data_t *ffd;
SVN_ERR(svn_fs__check_fs(txn->fs, TRUE));
ffd = txn->fs->fsap_data;
memset(&ffd->dir_cache_id, 0,
sizeof(svn_fs_id_t *) * NUM_DIR_CACHE_ENTRIES);
SVN_ERR_W(svn_fs_fs__purge_txn(txn->fs, txn->id, pool),
_("Transaction cleanup failed"));
return SVN_NO_ERROR;
}
static const char *
unparse_dir_entry(svn_node_kind_t kind, const svn_fs_id_t *id,
apr_pool_t *pool) {
return apr_psprintf(pool, "%s %s",
(kind == svn_node_file) ? KIND_FILE : KIND_DIR,
svn_fs_fs__id_unparse(id, pool)->data);
}
static svn_error_t *
unparse_dir_entries(apr_hash_t **str_entries_p,
apr_hash_t *entries,
apr_pool_t *pool) {
apr_hash_index_t *hi;
*str_entries_p = apr_hash_make(pool);
for (hi = apr_hash_first(pool, entries); hi; hi = apr_hash_next(hi)) {
const void *key;
apr_ssize_t klen;
void *val;
svn_fs_dirent_t *dirent;
const char *new_val;
apr_hash_this(hi, &key, &klen, &val);
dirent = val;
new_val = unparse_dir_entry(dirent->kind, dirent->id, pool);
apr_hash_set(*str_entries_p, key, klen,
svn_string_create(new_val, pool));
}
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_fs__set_entry(svn_fs_t *fs,
const char *txn_id,
node_revision_t *parent_noderev,
const char *name,
const svn_fs_id_t *id,
svn_node_kind_t kind,
apr_pool_t *pool) {
representation_t *rep = parent_noderev->data_rep;
const char *filename = path_txn_node_children(fs, parent_noderev->id, pool);
apr_file_t *file;
svn_stream_t *out;
if (!rep || !rep->txn_id) {
{
apr_hash_t *entries;
apr_pool_t *subpool = svn_pool_create(pool);
SVN_ERR(svn_fs_fs__rep_contents_dir(&entries, fs, parent_noderev,
subpool));
SVN_ERR(unparse_dir_entries(&entries, entries, subpool));
SVN_ERR(svn_io_file_open(&file, filename,
APR_WRITE | APR_CREATE | APR_BUFFERED,
APR_OS_DEFAULT, pool));
out = svn_stream_from_aprfile(file, pool);
SVN_ERR(svn_hash_write2(entries, out, SVN_HASH_TERMINATOR, subpool));
svn_pool_destroy(subpool);
}
rep = apr_pcalloc(pool, sizeof(*rep));
rep->revision = SVN_INVALID_REVNUM;
rep->txn_id = txn_id;
parent_noderev->data_rep = rep;
SVN_ERR(svn_fs_fs__put_node_revision(fs, parent_noderev->id,
parent_noderev, FALSE, pool));
} else {
SVN_ERR(svn_io_file_open(&file, filename, APR_WRITE | APR_APPEND,
APR_OS_DEFAULT, pool));
out = svn_stream_from_aprfile(file, pool);
}
if (id) {
const char *val = unparse_dir_entry(kind, id, pool);
SVN_ERR(svn_stream_printf(out, pool, "K %" APR_SIZE_T_FMT "\n%s\n"
"V %" APR_SIZE_T_FMT "\n%s\n",
strlen(name), name,
strlen(val), val));
} else {
SVN_ERR(svn_stream_printf(out, pool, "D %" APR_SIZE_T_FMT "\n%s\n",
strlen(name), name));
}
SVN_ERR(svn_io_file_close(file, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
write_change_entry(apr_file_t *file,
const char *path,
svn_fs_path_change_t *change,
const char *copyfrom,
apr_pool_t *pool) {
const char *idstr, *buf;
const char *change_string = NULL;
switch (change->change_kind) {
case svn_fs_path_change_modify:
change_string = ACTION_MODIFY;
break;
case svn_fs_path_change_add:
change_string = ACTION_ADD;
break;
case svn_fs_path_change_delete:
change_string = ACTION_DELETE;
break;
case svn_fs_path_change_replace:
change_string = ACTION_REPLACE;
break;
case svn_fs_path_change_reset:
change_string = ACTION_RESET;
break;
default:
return svn_error_create(SVN_ERR_FS_CORRUPT, NULL,
_("Invalid change type"));
}
if (change->node_rev_id)
idstr = svn_fs_fs__id_unparse(change->node_rev_id, pool)->data;
else
idstr = ACTION_RESET;
buf = apr_psprintf(pool, "%s %s %s %s %s\n",
idstr, change_string,
change->text_mod ? FLAG_TRUE : FLAG_FALSE,
change->prop_mod ? FLAG_TRUE : FLAG_FALSE,
path);
SVN_ERR(svn_io_file_write_full(file, buf, strlen(buf), NULL, pool));
if (copyfrom) {
SVN_ERR(svn_io_file_write_full(file, copyfrom, strlen(copyfrom),
NULL, pool));
}
SVN_ERR(svn_io_file_write_full(file, "\n", 1, NULL, pool));
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_fs__add_change(svn_fs_t *fs,
const char *txn_id,
const char *path,
const svn_fs_id_t *id,
svn_fs_path_change_kind_t change_kind,
svn_boolean_t text_mod,
svn_boolean_t prop_mod,
svn_revnum_t copyfrom_rev,
const char *copyfrom_path,
apr_pool_t *pool) {
apr_file_t *file;
const char *copyfrom;
svn_fs_path_change_t *change = apr_pcalloc(pool, sizeof(*change));
SVN_ERR(svn_io_file_open(&file, path_txn_changes(fs, txn_id, pool),
APR_APPEND | APR_WRITE | APR_CREATE
| APR_BUFFERED, APR_OS_DEFAULT, pool));
if (copyfrom_rev != SVN_INVALID_REVNUM)
copyfrom = apr_psprintf(pool, "%ld %s", copyfrom_rev, copyfrom_path);
else
copyfrom = "";
change->node_rev_id = id;
change->change_kind = change_kind;
change->text_mod = text_mod;
change->prop_mod = prop_mod;
SVN_ERR(write_change_entry(file, path, change, copyfrom, pool));
SVN_ERR(svn_io_file_close(file, pool));
return SVN_NO_ERROR;
}
struct rep_write_baton {
svn_fs_t *fs;
svn_stream_t *rep_stream;
svn_stream_t *delta_stream;
apr_off_t rep_offset;
apr_off_t delta_start;
svn_filesize_t rep_size;
node_revision_t *noderev;
apr_file_t *file;
void *lockcookie;
struct apr_md5_ctx_t md5_context;
apr_pool_t *pool;
apr_pool_t *parent_pool;
};
static svn_error_t *
rep_write_contents(void *baton,
const char *data,
apr_size_t *len) {
struct rep_write_baton *b = baton;
apr_md5_update(&b->md5_context, data, *len);
b->rep_size += *len;
if (b->delta_stream) {
SVN_ERR(svn_stream_write(b->delta_stream, data, len));
} else {
SVN_ERR(svn_stream_write(b->rep_stream, data, len));
}
return SVN_NO_ERROR;
}
static svn_error_t *
choose_delta_base(representation_t **rep,
svn_fs_t *fs,
node_revision_t *noderev,
apr_pool_t *pool) {
int count;
node_revision_t *base;
if (! noderev->predecessor_count) {
*rep = NULL;
return SVN_NO_ERROR;
}
count = noderev->predecessor_count;
count = count & (count - 1);
base = noderev;
while ((count++) < noderev->predecessor_count)
SVN_ERR(svn_fs_fs__get_node_revision(&base, fs,
base->predecessor_id, pool));
*rep = base->data_rep;
return SVN_NO_ERROR;
}
static svn_error_t *
rep_write_get_baton(struct rep_write_baton **wb_p,
svn_fs_t *fs,
node_revision_t *noderev,
apr_pool_t *pool) {
struct rep_write_baton *b;
apr_file_t *file;
representation_t *base_rep;
svn_stream_t *source;
const char *header;
svn_txdelta_window_handler_t wh;
void *whb;
fs_fs_data_t *ffd = fs->fsap_data;
b = apr_pcalloc(pool, sizeof(*b));
apr_md5_init(&(b->md5_context));
b->fs = fs;
b->parent_pool = pool;
b->pool = svn_pool_create(pool);
b->rep_size = 0;
b->noderev = noderev;
SVN_ERR(get_writable_proto_rev(&file, &b->lockcookie,
fs, svn_fs_fs__id_txn_id(noderev->id),
b->pool));
b->file = file;
b->rep_stream = svn_stream_from_aprfile(file, b->pool);
SVN_ERR(get_file_offset(&b->rep_offset, file, b->pool));
SVN_ERR(choose_delta_base(&base_rep, fs, noderev, b->pool));
SVN_ERR(read_representation(&source, fs, base_rep, b->pool));
if (base_rep) {
header = apr_psprintf(b->pool, REP_DELTA " %ld %" APR_OFF_T_FMT " %"
SVN_FILESIZE_T_FMT "\n",
base_rep->revision, base_rep->offset,
base_rep->size);
} else {
header = REP_DELTA "\n";
}
SVN_ERR(svn_io_file_write_full(file, header, strlen(header), NULL,
b->pool));
SVN_ERR(get_file_offset(&b->delta_start, file, b->pool));
if (ffd->format >= SVN_FS_FS__MIN_SVNDIFF1_FORMAT)
svn_txdelta_to_svndiff2(&wh, &whb, b->rep_stream, 1, pool);
else
svn_txdelta_to_svndiff2(&wh, &whb, b->rep_stream, 0, pool);
b->delta_stream = svn_txdelta_target_push(wh, whb, source, b->pool);
*wb_p = b;
return SVN_NO_ERROR;
}
static svn_error_t *
rep_write_contents_close(void *baton) {
struct rep_write_baton *b = baton;
representation_t *rep;
apr_off_t offset;
rep = apr_pcalloc(b->parent_pool, sizeof(*rep));
rep->offset = b->rep_offset;
if (b->delta_stream)
SVN_ERR(svn_stream_close(b->delta_stream));
SVN_ERR(get_file_offset(&offset, b->file, b->pool));
rep->size = offset - b->delta_start;
rep->expanded_size = b->rep_size;
rep->txn_id = svn_fs_fs__id_txn_id(b->noderev->id);
rep->revision = SVN_INVALID_REVNUM;
apr_md5_final(rep->checksum, &b->md5_context);
SVN_ERR(svn_stream_printf(b->rep_stream, b->pool, "ENDREP\n"));
b->noderev->data_rep = rep;
SVN_ERR(svn_fs_fs__put_node_revision(b->fs, b->noderev->id, b->noderev, FALSE,
b->pool));
SVN_ERR(svn_io_file_close(b->file, b->pool));
SVN_ERR(unlock_proto_rev(b->fs, rep->txn_id, b->lockcookie, b->pool));
svn_pool_destroy(b->pool);
return SVN_NO_ERROR;
}
static svn_error_t *
set_representation(svn_stream_t **contents_p,
svn_fs_t *fs,
node_revision_t *noderev,
apr_pool_t *pool) {
struct rep_write_baton *wb;
if (! svn_fs_fs__id_txn_id(noderev->id))
return svn_error_create(SVN_ERR_FS_CORRUPT, NULL,
_("Attempted to write to non-transaction"));
SVN_ERR(rep_write_get_baton(&wb, fs, noderev, pool));
*contents_p = svn_stream_create(wb, pool);
svn_stream_set_write(*contents_p, rep_write_contents);
svn_stream_set_close(*contents_p, rep_write_contents_close);
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_fs__set_contents(svn_stream_t **stream,
svn_fs_t *fs,
node_revision_t *noderev,
apr_pool_t *pool) {
if (noderev->kind != svn_node_file)
return svn_error_create(SVN_ERR_FS_NOT_FILE, NULL,
_("Can't set text contents of a directory"));
return set_representation(stream, fs, noderev, pool);
}
svn_error_t *
svn_fs_fs__create_successor(const svn_fs_id_t **new_id_p,
svn_fs_t *fs,
const svn_fs_id_t *old_idp,
node_revision_t *new_noderev,
const char *copy_id,
const char *txn_id,
apr_pool_t *pool) {
const svn_fs_id_t *id;
if (! copy_id)
copy_id = svn_fs_fs__id_copy_id(old_idp);
id = svn_fs_fs__id_txn_create(svn_fs_fs__id_node_id(old_idp), copy_id,
txn_id, pool);
new_noderev->id = id;
if (! new_noderev->copyroot_path) {
new_noderev->copyroot_path = apr_pstrdup(pool,
new_noderev->created_path);
new_noderev->copyroot_rev = svn_fs_fs__id_rev(new_noderev->id);
}
SVN_ERR(svn_fs_fs__put_node_revision(fs, new_noderev->id, new_noderev, FALSE,
pool));
*new_id_p = id;
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_fs__set_proplist(svn_fs_t *fs,
node_revision_t *noderev,
apr_hash_t *proplist,
apr_pool_t *pool) {
const char *filename = path_txn_node_props(fs, noderev->id, pool);
apr_file_t *file;
svn_stream_t *out;
SVN_ERR(svn_io_file_open(&file, filename,
APR_WRITE | APR_CREATE | APR_TRUNCATE
| APR_BUFFERED, APR_OS_DEFAULT, pool));
out = svn_stream_from_aprfile(file, pool);
SVN_ERR(svn_hash_write2(proplist, out, SVN_HASH_TERMINATOR, pool));
SVN_ERR(svn_io_file_close(file, pool));
if (!noderev->prop_rep || !noderev->prop_rep->txn_id) {
noderev->prop_rep = apr_pcalloc(pool, sizeof(*noderev->prop_rep));
noderev->prop_rep->txn_id = svn_fs_fs__id_txn_id(noderev->id);
SVN_ERR(svn_fs_fs__put_node_revision(fs, noderev->id, noderev, FALSE, pool));
}
return SVN_NO_ERROR;
}
static svn_error_t *
get_next_revision_ids(const char **node_id,
const char **copy_id,
svn_fs_t *fs,
apr_pool_t *pool) {
char *buf;
char *str, *last_str;
SVN_ERR(read_current(svn_fs_fs__path_current(fs, pool), &buf, pool));
str = apr_strtok(buf, " ", &last_str);
if (! str)
return svn_error_create(SVN_ERR_FS_CORRUPT, NULL,
_("Corrupt current file"));
str = apr_strtok(NULL, " ", &last_str);
if (! str)
return svn_error_create(SVN_ERR_FS_CORRUPT, NULL,
_("Corrupt current file"));
*node_id = apr_pstrdup(pool, str);
str = apr_strtok(NULL, " ", &last_str);
if (! str)
return svn_error_create(SVN_ERR_FS_CORRUPT, NULL,
_("Corrupt current file"));
*copy_id = apr_pstrdup(pool, str);
return SVN_NO_ERROR;
}
struct write_hash_baton {
svn_stream_t *stream;
apr_size_t size;
struct apr_md5_ctx_t md5_context;
};
static svn_error_t *
write_hash_handler(void *baton,
const char *data,
apr_size_t *len) {
struct write_hash_baton *whb = baton;
apr_md5_update(&whb->md5_context, data, *len);
SVN_ERR(svn_stream_write(whb->stream, data, len));
whb->size += *len;
return SVN_NO_ERROR;
}
static svn_error_t *
write_hash_rep(svn_filesize_t *size,
unsigned char checksum[APR_MD5_DIGESTSIZE],
apr_file_t *file,
apr_hash_t *hash,
apr_pool_t *pool) {
svn_stream_t *stream;
struct write_hash_baton *whb;
whb = apr_pcalloc(pool, sizeof(*whb));
whb->stream = svn_stream_from_aprfile(file, pool);
whb->size = 0;
apr_md5_init(&(whb->md5_context));
stream = svn_stream_create(whb, pool);
svn_stream_set_write(stream, write_hash_handler);
SVN_ERR(svn_stream_printf(whb->stream, pool, "PLAIN\n"));
SVN_ERR(svn_hash_write2(hash, stream, SVN_HASH_TERMINATOR, pool));
apr_md5_final(checksum, &whb->md5_context);
*size = whb->size;
SVN_ERR(svn_stream_printf(whb->stream, pool, "ENDREP\n"));
return SVN_NO_ERROR;
}
static svn_error_t *
write_final_rev(const svn_fs_id_t **new_id_p,
apr_file_t *file,
svn_revnum_t rev,
svn_fs_t *fs,
const svn_fs_id_t *id,
const char *start_node_id,
const char *start_copy_id,
apr_pool_t *pool) {
node_revision_t *noderev;
apr_off_t my_offset;
char my_node_id_buf[MAX_KEY_SIZE + 2];
char my_copy_id_buf[MAX_KEY_SIZE + 2];
const svn_fs_id_t *new_id;
const char *node_id, *copy_id, *my_node_id, *my_copy_id;
fs_fs_data_t *ffd = fs->fsap_data;
*new_id_p = NULL;
if (! svn_fs_fs__id_txn_id(id))
return SVN_NO_ERROR;
SVN_ERR(svn_fs_fs__get_node_revision(&noderev, fs, id, pool));
if (noderev->kind == svn_node_dir) {
apr_pool_t *subpool;
apr_hash_t *entries, *str_entries;
svn_fs_dirent_t *dirent;
void *val;
apr_hash_index_t *hi;
subpool = svn_pool_create(pool);
SVN_ERR(svn_fs_fs__rep_contents_dir(&entries, fs, noderev, pool));
for (hi = apr_hash_first(pool, entries); hi; hi = apr_hash_next(hi)) {
svn_pool_clear(subpool);
apr_hash_this(hi, NULL, NULL, &val);
dirent = val;
SVN_ERR(write_final_rev(&new_id, file, rev, fs, dirent->id,
start_node_id, start_copy_id,
subpool));
if (new_id && (svn_fs_fs__id_rev(new_id) == rev))
dirent->id = svn_fs_fs__id_copy(new_id, pool);
}
svn_pool_destroy(subpool);
if (noderev->data_rep && noderev->data_rep->txn_id) {
SVN_ERR(unparse_dir_entries(&str_entries, entries, pool));
noderev->data_rep->txn_id = NULL;
noderev->data_rep->revision = rev;
SVN_ERR(get_file_offset(&noderev->data_rep->offset, file, pool));
SVN_ERR(write_hash_rep(&noderev->data_rep->size,
noderev->data_rep->checksum, file,
str_entries, pool));
noderev->data_rep->expanded_size = noderev->data_rep->size;
}
} else {
if (noderev->data_rep && noderev->data_rep->txn_id) {
noderev->data_rep->txn_id = NULL;
noderev->data_rep->revision = rev;
}
}
if (noderev->prop_rep && noderev->prop_rep->txn_id) {
apr_hash_t *proplist;
SVN_ERR(svn_fs_fs__get_proplist(&proplist, fs, noderev, pool));
SVN_ERR(get_file_offset(&noderev->prop_rep->offset, file, pool));
SVN_ERR(write_hash_rep(&noderev->prop_rep->size,
noderev->prop_rep->checksum, file,
proplist, pool));
noderev->prop_rep->txn_id = NULL;
noderev->prop_rep->revision = rev;
}
SVN_ERR(get_file_offset(&my_offset, file, pool));
node_id = svn_fs_fs__id_node_id(noderev->id);
if (*node_id == '_') {
if (ffd->format >= SVN_FS_FS__MIN_NO_GLOBAL_IDS_FORMAT)
my_node_id = apr_psprintf(pool, "%s-%ld", node_id + 1, rev);
else {
svn_fs_fs__add_keys(start_node_id, node_id + 1, my_node_id_buf);
my_node_id = my_node_id_buf;
}
} else
my_node_id = node_id;
copy_id = svn_fs_fs__id_copy_id(noderev->id);
if (*copy_id == '_') {
if (ffd->format >= SVN_FS_FS__MIN_NO_GLOBAL_IDS_FORMAT)
my_copy_id = apr_psprintf(pool, "%s-%ld", copy_id + 1, rev);
else {
svn_fs_fs__add_keys(start_copy_id, copy_id + 1, my_copy_id_buf);
my_copy_id = my_copy_id_buf;
}
} else
my_copy_id = copy_id;
if (noderev->copyroot_rev == SVN_INVALID_REVNUM)
noderev->copyroot_rev = rev;
new_id = svn_fs_fs__id_rev_create(my_node_id, my_copy_id, rev, my_offset,
pool);
noderev->id = new_id;
SVN_ERR(write_noderev_txn(file, noderev,
svn_fs_fs__fs_supports_mergeinfo(fs),
pool));
*new_id_p = noderev->id;
return SVN_NO_ERROR;
}
static svn_error_t *
write_final_changed_path_info(apr_off_t *offset_p,
apr_file_t *file,
svn_fs_t *fs,
const char *txn_id,
apr_pool_t *pool) {
const char *copyfrom;
apr_hash_t *changed_paths, *copyfrom_cache = apr_hash_make(pool);
apr_off_t offset;
apr_hash_index_t *hi;
apr_pool_t *iterpool = svn_pool_create(pool);
SVN_ERR(get_file_offset(&offset, file, pool));
SVN_ERR(svn_fs_fs__txn_changes_fetch(&changed_paths, fs, txn_id,
copyfrom_cache, pool));
for (hi = apr_hash_first(pool, changed_paths); hi; hi = apr_hash_next(hi)) {
node_revision_t *noderev;
const svn_fs_id_t *id;
svn_fs_path_change_t *change;
const void *key;
void *val;
svn_pool_clear(iterpool);
apr_hash_this(hi, &key, NULL, &val);
change = val;
id = change->node_rev_id;
if ((change->change_kind != svn_fs_path_change_delete) &&
(! svn_fs_fs__id_txn_id(id))) {
SVN_ERR(svn_fs_fs__get_node_revision(&noderev, fs, id, iterpool));
change->node_rev_id = noderev->id;
}
copyfrom = apr_hash_get(copyfrom_cache, key, APR_HASH_KEY_STRING);
SVN_ERR(write_change_entry(file, key, change, copyfrom, iterpool));
}
svn_pool_destroy(iterpool);
*offset_p = offset;
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_fs__dup_perms(const char *filename,
const char *perms_reference,
apr_pool_t *pool) {
#if !defined(WIN32)
apr_status_t status;
apr_finfo_t finfo;
const char *filename_apr, *perms_reference_apr;
SVN_ERR(svn_path_cstring_from_utf8(&filename_apr, filename, pool));
SVN_ERR(svn_path_cstring_from_utf8(&perms_reference_apr, perms_reference,
pool));
status = apr_stat(&finfo, perms_reference_apr, APR_FINFO_PROT, pool);
if (status)
return svn_error_wrap_apr(status, _("Can't stat '%s'"),
svn_path_local_style(perms_reference, pool));
status = apr_file_perms_set(filename_apr, finfo.protection);
if (status)
return svn_error_wrap_apr(status, _("Can't chmod '%s'"),
svn_path_local_style(filename, pool));
#endif
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_fs__move_into_place(const char *old_filename,
const char *new_filename,
const char *perms_reference,
apr_pool_t *pool) {
svn_error_t *err;
SVN_ERR(svn_fs_fs__dup_perms(old_filename, perms_reference, pool));
err = svn_io_file_rename(old_filename, new_filename, pool);
if (err && APR_STATUS_IS_EXDEV(err->apr_err)) {
apr_file_t *file;
svn_error_clear(err);
err = SVN_NO_ERROR;
SVN_ERR(svn_io_copy_file(old_filename, new_filename, TRUE, pool));
SVN_ERR(svn_io_file_open(&file, new_filename, APR_READ,
APR_OS_DEFAULT, pool));
SVN_ERR(svn_io_file_flush_to_disk(file, pool));
SVN_ERR(svn_io_file_close(file, pool));
}
if (err)
return err;
#if defined(__linux__)
{
const char *dirname;
apr_file_t *file;
dirname = svn_path_dirname(new_filename, pool);
SVN_ERR(svn_io_file_open(&file, dirname, APR_READ, APR_OS_DEFAULT,
pool));
SVN_ERR(svn_io_file_flush_to_disk(file, pool));
SVN_ERR(svn_io_file_close(file, pool));
}
#endif
return SVN_NO_ERROR;
}
static svn_error_t *
write_current(svn_fs_t *fs, svn_revnum_t rev, const char *next_node_id,
const char *next_copy_id, apr_pool_t *pool) {
char *buf;
const char *tmp_name, *name;
apr_file_t *file;
fs_fs_data_t *ffd = fs->fsap_data;
if (ffd->format >= SVN_FS_FS__MIN_NO_GLOBAL_IDS_FORMAT)
buf = apr_psprintf(pool, "%ld\n", rev);
else
buf = apr_psprintf(pool, "%ld %s %s\n", rev, next_node_id, next_copy_id);
name = svn_fs_fs__path_current(fs, pool);
SVN_ERR(svn_io_open_unique_file2(&file, &tmp_name, name, ".tmp",
svn_io_file_del_none, pool));
SVN_ERR(svn_io_file_write_full(file, buf, strlen(buf), NULL, pool));
SVN_ERR(svn_io_file_flush_to_disk(file, pool));
SVN_ERR(svn_io_file_close(file, pool));
SVN_ERR(svn_fs_fs__move_into_place(tmp_name, name, name, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
write_final_current(svn_fs_t *fs,
const char *txn_id,
svn_revnum_t rev,
const char *start_node_id,
const char *start_copy_id,
apr_pool_t *pool) {
const char *txn_node_id, *txn_copy_id;
char new_node_id[MAX_KEY_SIZE + 2];
char new_copy_id[MAX_KEY_SIZE + 2];
fs_fs_data_t *ffd = fs->fsap_data;
if (ffd->format >= SVN_FS_FS__MIN_NO_GLOBAL_IDS_FORMAT)
return write_current(fs, rev, NULL, NULL, pool);
SVN_ERR(read_next_ids(&txn_node_id, &txn_copy_id, fs, txn_id, pool));
svn_fs_fs__add_keys(start_node_id, txn_node_id, new_node_id);
svn_fs_fs__add_keys(start_copy_id, txn_copy_id, new_copy_id);
return write_current(fs, rev, new_node_id, new_copy_id, pool);
}
static svn_error_t *
verify_locks(svn_fs_t *fs,
const char *txn_name,
apr_pool_t *pool) {
apr_pool_t *subpool = svn_pool_create(pool);
apr_hash_t *changes;
apr_hash_index_t *hi;
apr_array_header_t *changed_paths;
svn_stringbuf_t *last_recursed = NULL;
int i;
SVN_ERR(svn_fs_fs__txn_changes_fetch(&changes, fs, txn_name, NULL, pool));
changed_paths = apr_array_make(pool, apr_hash_count(changes) + 1,
sizeof(const char *));
for (hi = apr_hash_first(pool, changes); hi; hi = apr_hash_next(hi)) {
const void *key;
apr_hash_this(hi, &key, NULL, NULL);
APR_ARRAY_PUSH(changed_paths, const char *) = key;
}
qsort(changed_paths->elts, changed_paths->nelts,
changed_paths->elt_size, svn_sort_compare_paths);
for (i = 0; i < changed_paths->nelts; i++) {
const char *path;
svn_fs_path_change_t *change;
svn_boolean_t recurse = TRUE;
svn_pool_clear(subpool);
path = APR_ARRAY_IDX(changed_paths, i, const char *);
if (last_recursed
&& svn_path_is_child(last_recursed->data, path, subpool))
continue;
change = apr_hash_get(changes, path, APR_HASH_KEY_STRING);
if (change->change_kind == svn_fs_path_change_modify)
recurse = FALSE;
SVN_ERR(svn_fs_fs__allow_locked_operation(path, fs, recurse, TRUE,
subpool));
if (recurse) {
if (! last_recursed)
last_recursed = svn_stringbuf_create(path, pool);
else
svn_stringbuf_set(last_recursed, path);
}
}
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
struct commit_baton {
svn_revnum_t *new_rev_p;
svn_fs_t *fs;
svn_fs_txn_t *txn;
};
static svn_error_t *
commit_body(void *baton, apr_pool_t *pool) {
struct commit_baton *cb = baton;
fs_fs_data_t *ffd = cb->fs->fsap_data;
const char *old_rev_filename, *rev_filename, *proto_filename;
const char *revprop_filename, *final_revprop;
const svn_fs_id_t *root_id, *new_root_id;
const char *start_node_id = NULL, *start_copy_id = NULL;
svn_revnum_t old_rev, new_rev;
apr_file_t *proto_file;
void *proto_file_lockcookie;
apr_off_t changed_path_offset;
char *buf;
apr_hash_t *txnprops;
svn_string_t date;
SVN_ERR(svn_fs_fs__youngest_rev(&old_rev, cb->fs, pool));
if (cb->txn->base_rev != old_rev)
return svn_error_create(SVN_ERR_FS_TXN_OUT_OF_DATE, NULL,
_("Transaction out of date"));
SVN_ERR(verify_locks(cb->fs, cb->txn->id, pool));
if (ffd->format < SVN_FS_FS__MIN_NO_GLOBAL_IDS_FORMAT)
SVN_ERR(get_next_revision_ids(&start_node_id, &start_copy_id, cb->fs,
pool));
new_rev = old_rev + 1;
SVN_ERR(get_writable_proto_rev(&proto_file, &proto_file_lockcookie,
cb->fs, cb->txn->id, pool));
root_id = svn_fs_fs__id_txn_create("0", "0", cb->txn->id, pool);
SVN_ERR(write_final_rev(&new_root_id, proto_file, new_rev, cb->fs, root_id,
start_node_id, start_copy_id,
pool));
SVN_ERR(write_final_changed_path_info(&changed_path_offset, proto_file,
cb->fs, cb->txn->id, pool));
buf = apr_psprintf(pool, "\n%" APR_OFF_T_FMT " %" APR_OFF_T_FMT "\n",
svn_fs_fs__id_offset(new_root_id),
changed_path_offset);
SVN_ERR(svn_io_file_write_full(proto_file, buf, strlen(buf), NULL,
pool));
SVN_ERR(svn_io_file_flush_to_disk(proto_file, pool));
SVN_ERR(svn_io_file_close(proto_file, pool));
SVN_ERR(svn_fs_fs__txn_proplist(&txnprops, cb->txn, pool));
if (txnprops) {
apr_array_header_t *props = apr_array_make(pool, 3, sizeof(svn_prop_t));
svn_prop_t prop;
prop.value = NULL;
if (apr_hash_get(txnprops, SVN_FS__PROP_TXN_CHECK_OOD,
APR_HASH_KEY_STRING)) {
prop.name = SVN_FS__PROP_TXN_CHECK_OOD;
APR_ARRAY_PUSH(props, svn_prop_t) = prop;
}
if (apr_hash_get(txnprops, SVN_FS__PROP_TXN_CHECK_LOCKS,
APR_HASH_KEY_STRING)) {
prop.name = SVN_FS__PROP_TXN_CHECK_LOCKS;
APR_ARRAY_PUSH(props, svn_prop_t) = prop;
}
if (! apr_is_empty_array(props))
SVN_ERR(svn_fs_fs__change_txn_props(cb->txn, props, pool));
}
if (ffd->max_files_per_dir && new_rev % ffd->max_files_per_dir == 0) {
svn_error_t *err;
err = svn_io_dir_make(path_rev_shard(cb->fs, new_rev, pool),
APR_OS_DEFAULT, pool);
if (err && APR_STATUS_IS_EEXIST(err->apr_err))
svn_error_clear(err);
else
SVN_ERR(err);
err = svn_io_dir_make(path_revprops_shard(cb->fs, new_rev, pool),
APR_OS_DEFAULT, pool);
if (err && APR_STATUS_IS_EEXIST(err->apr_err))
svn_error_clear(err);
else
SVN_ERR(err);
}
old_rev_filename = svn_fs_fs__path_rev(cb->fs, old_rev, pool);
rev_filename = svn_fs_fs__path_rev(cb->fs, new_rev, pool);
proto_filename = path_txn_proto_rev(cb->fs, cb->txn->id, pool);
SVN_ERR(svn_fs_fs__move_into_place(proto_filename, rev_filename,
old_rev_filename, pool));
SVN_ERR(unlock_proto_rev(cb->fs, cb->txn->id, proto_file_lockcookie, pool));
date.data = svn_time_to_cstring(apr_time_now(), pool);
date.len = strlen(date.data);
SVN_ERR(svn_fs_fs__change_txn_prop(cb->txn, SVN_PROP_REVISION_DATE,
&date, pool));
revprop_filename = path_txn_props(cb->fs, cb->txn->id, pool);
final_revprop = path_revprops(cb->fs, new_rev, pool);
SVN_ERR(svn_fs_fs__move_into_place(revprop_filename, final_revprop,
old_rev_filename, pool));
SVN_ERR(write_final_current(cb->fs, cb->txn->id, new_rev, start_node_id,
start_copy_id, pool));
ffd->youngest_rev_cache = new_rev;
SVN_ERR(svn_fs_fs__purge_txn(cb->fs, cb->txn->id, pool));
*cb->new_rev_p = new_rev;
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_fs__commit(svn_revnum_t *new_rev_p,
svn_fs_t *fs,
svn_fs_txn_t *txn,
apr_pool_t *pool) {
struct commit_baton cb;
cb.new_rev_p = new_rev_p;
cb.fs = fs;
cb.txn = txn;
return svn_fs_fs__with_write_lock(fs, commit_body, &cb, pool);
}
svn_error_t *
svn_fs_fs__reserve_copy_id(const char **copy_id_p,
svn_fs_t *fs,
const char *txn_id,
apr_pool_t *pool) {
const char *cur_node_id, *cur_copy_id;
char *copy_id;
apr_size_t len;
SVN_ERR(read_next_ids(&cur_node_id, &cur_copy_id, fs, txn_id, pool));
copy_id = apr_pcalloc(pool, strlen(cur_copy_id) + 2);
len = strlen(cur_copy_id);
svn_fs_fs__next_key(cur_copy_id, &len, copy_id);
SVN_ERR(write_next_ids(fs, txn_id, cur_node_id, copy_id, pool));
*copy_id_p = apr_pstrcat(pool, "_", cur_copy_id, NULL);
return SVN_NO_ERROR;
}
static svn_error_t *
write_revision_zero(svn_fs_t *fs) {
apr_hash_t *proplist;
svn_string_t date;
SVN_ERR(svn_io_file_create(svn_fs_fs__path_rev(fs, 0, fs->pool),
"PLAIN\nEND\nENDREP\n"
"id: 0.0.r0/17\n"
"type: dir\n"
"count: 0\n"
"text: 0 0 4 4 "
"2d2977d1c96f487abe4a1e202dd03b4e\n"
"cpath: /\n"
"\n\n17 107\n", fs->pool));
date.data = svn_time_to_cstring(apr_time_now(), fs->pool);
date.len = strlen(date.data);
proplist = apr_hash_make(fs->pool);
apr_hash_set(proplist, SVN_PROP_REVISION_DATE, APR_HASH_KEY_STRING, &date);
return svn_fs_fs__set_revision_proplist(fs, 0, proplist, fs->pool);
}
svn_error_t *
svn_fs_fs__create(svn_fs_t *fs,
const char *path,
apr_pool_t *pool) {
int format = SVN_FS_FS__FORMAT_NUMBER;
fs_fs_data_t *ffd = fs->fsap_data;
fs->path = apr_pstrdup(pool, path);
if (fs->config) {
if (apr_hash_get(fs->config, SVN_FS_CONFIG_PRE_1_4_COMPATIBLE,
APR_HASH_KEY_STRING))
format = 1;
else if (apr_hash_get(fs->config, SVN_FS_CONFIG_PRE_1_5_COMPATIBLE,
APR_HASH_KEY_STRING))
format = 2;
}
ffd->format = format;
if (format >= SVN_FS_FS__MIN_LAYOUT_FORMAT_OPTION_FORMAT)
ffd->max_files_per_dir = SVN_FS_FS_DEFAULT_MAX_FILES_PER_DIR;
if (ffd->max_files_per_dir) {
SVN_ERR(svn_io_make_dir_recursively(path_rev_shard(fs, 0, pool),
pool));
SVN_ERR(svn_io_make_dir_recursively(path_revprops_shard(fs, 0, pool),
pool));
} else {
SVN_ERR(svn_io_make_dir_recursively(svn_path_join(path, PATH_REVS_DIR,
pool),
pool));
SVN_ERR(svn_io_make_dir_recursively(svn_path_join(path,
PATH_REVPROPS_DIR,
pool),
pool));
}
SVN_ERR(svn_io_make_dir_recursively(svn_path_join(path, PATH_TXNS_DIR,
pool),
pool));
if (format >= SVN_FS_FS__MIN_PROTOREVS_DIR_FORMAT)
SVN_ERR(svn_io_make_dir_recursively(svn_path_join(path, PATH_TXN_PROTOS_DIR,
pool),
pool));
SVN_ERR(svn_io_file_create(svn_fs_fs__path_current(fs, pool),
(format >= SVN_FS_FS__MIN_NO_GLOBAL_IDS_FORMAT
? "0\n" : "0 1 1\n"),
pool));
SVN_ERR(svn_io_file_create(path_lock(fs, pool), "", pool));
SVN_ERR(svn_fs_fs__set_uuid(fs, svn_uuid_generate(pool), pool));
SVN_ERR(write_revision_zero(fs));
if (format >= SVN_FS_FS__MIN_TXN_CURRENT_FORMAT) {
SVN_ERR(svn_io_file_create(path_txn_current(fs, pool),
"0\n", pool));
SVN_ERR(svn_io_file_create(path_txn_current_lock(fs, pool),
"", pool));
}
SVN_ERR(write_format(path_format(fs, pool),
ffd->format, ffd->max_files_per_dir, FALSE, pool));
ffd->youngest_rev_cache = 0;
return SVN_NO_ERROR;
}
static svn_error_t *
recover_get_largest_revision(svn_fs_t *fs, svn_revnum_t *rev, apr_pool_t *pool) {
apr_pool_t *iterpool;
svn_revnum_t left, right = 1;
iterpool = svn_pool_create(pool);
while (1) {
svn_node_kind_t kind;
SVN_ERR(svn_io_check_path(svn_fs_fs__path_rev(fs, right, iterpool),
&kind, iterpool));
svn_pool_clear(iterpool);
if (kind == svn_node_none)
break;
right <<= 1;
}
left = right >> 1;
while (left + 1 < right) {
svn_revnum_t probe = left + ((right - left) / 2);
svn_node_kind_t kind;
SVN_ERR(svn_io_check_path(svn_fs_fs__path_rev(fs, probe, iterpool),
&kind, iterpool));
svn_pool_clear(iterpool);
if (kind == svn_node_none)
right = probe;
else
left = probe;
}
svn_pool_destroy(iterpool);
*rev = left;
return SVN_NO_ERROR;
}
struct recover_read_from_file_baton {
apr_file_t *file;
apr_pool_t *pool;
apr_size_t remaining;
};
static svn_error_t *
read_handler_recover(void *baton, char *buffer, apr_size_t *len) {
struct recover_read_from_file_baton *b = baton;
apr_size_t bytes_to_read = *len;
if (b->remaining == 0) {
*len = 0;
return SVN_NO_ERROR;
}
if (bytes_to_read > b->remaining)
bytes_to_read = b->remaining;
b->remaining -= bytes_to_read;
return svn_io_file_read_full(b->file, buffer, bytes_to_read, len, b->pool);
}
static svn_error_t *
recover_find_max_ids(svn_fs_t *fs, svn_revnum_t rev,
apr_file_t *rev_file, apr_off_t offset,
char *max_node_id, char *max_copy_id,
apr_pool_t *pool) {
apr_hash_t *headers;
char *value;
node_revision_t noderev;
struct rep_args *ra;
struct recover_read_from_file_baton baton;
svn_stream_t *stream;
apr_hash_t *entries;
apr_hash_index_t *hi;
apr_pool_t *iterpool;
SVN_ERR(svn_io_file_seek(rev_file, APR_SET, &offset, pool));
SVN_ERR(read_header_block(&headers, rev_file, pool));
value = apr_hash_get(headers, HEADER_ID, APR_HASH_KEY_STRING);
noderev.id = svn_fs_fs__id_parse(value, strlen(value), pool);
value = apr_hash_get(headers, HEADER_TYPE, APR_HASH_KEY_STRING);
if (value == NULL || strcmp(value, KIND_DIR) != 0)
return svn_error_create(SVN_ERR_FS_CORRUPT, NULL,
_("Recovery encountered a non-directory node"));
value = apr_hash_get(headers, HEADER_TEXT, APR_HASH_KEY_STRING);
if (!value)
return SVN_NO_ERROR;
SVN_ERR(read_rep_offsets(&noderev.data_rep, value, NULL, FALSE, pool));
if (noderev.data_rep->revision != rev)
return SVN_NO_ERROR;
offset = noderev.data_rep->offset;
SVN_ERR(svn_io_file_seek(rev_file, APR_SET, &offset, pool));
SVN_ERR(read_rep_line(&ra, rev_file, pool));
if (ra->is_delta)
return svn_error_create(SVN_ERR_FS_CORRUPT, NULL,
_("Recovery encountered a deltified directory "
"representation"));
baton.file = rev_file;
baton.pool = pool;
baton.remaining = noderev.data_rep->expanded_size;
stream = svn_stream_create(&baton, pool);
svn_stream_set_read(stream, read_handler_recover);
entries = apr_hash_make(pool);
SVN_ERR(svn_hash_read2(entries, stream, SVN_HASH_TERMINATOR, pool));
SVN_ERR(svn_stream_close(stream));
iterpool = svn_pool_create(pool);
for (hi = apr_hash_first(NULL, entries); hi; hi = apr_hash_next(hi)) {
void *val;
char *str_val;
char *str, *last_str;
svn_node_kind_t kind;
svn_fs_id_t *id;
const char *node_id, *copy_id;
apr_off_t child_dir_offset;
svn_pool_clear(iterpool);
apr_hash_this(hi, NULL, NULL, &val);
str_val = apr_pstrdup(iterpool, *((char **)val));
str = apr_strtok(str_val, " ", &last_str);
if (str == NULL)
return svn_error_create(SVN_ERR_FS_CORRUPT, NULL,
_("Directory entry corrupt"));
if (strcmp(str, KIND_FILE) == 0)
kind = svn_node_file;
else if (strcmp(str, KIND_DIR) == 0)
kind = svn_node_dir;
else {
return svn_error_create(SVN_ERR_FS_CORRUPT, NULL,
_("Directory entry corrupt"));
}
str = apr_strtok(NULL, " ", &last_str);
if (str == NULL)
return svn_error_create(SVN_ERR_FS_CORRUPT, NULL,
_("Directory entry corrupt"));
id = svn_fs_fs__id_parse(str, strlen(str), iterpool);
if (svn_fs_fs__id_rev(id) != rev) {
continue;
}
node_id = svn_fs_fs__id_node_id(id);
copy_id = svn_fs_fs__id_copy_id(id);
if (svn_fs_fs__key_compare(node_id, max_node_id) > 0)
strcpy(max_node_id, node_id);
if (svn_fs_fs__key_compare(copy_id, max_copy_id) > 0)
strcpy(max_copy_id, copy_id);
if (kind == svn_node_file)
continue;
child_dir_offset = svn_fs_fs__id_offset(id);
SVN_ERR(recover_find_max_ids(fs, rev, rev_file, child_dir_offset,
max_node_id, max_copy_id, iterpool));
}
svn_pool_destroy(iterpool);
return SVN_NO_ERROR;
}
struct recover_baton {
svn_fs_t *fs;
svn_cancel_func_t cancel_func;
void *cancel_baton;
};
static svn_error_t *
recover_body(void *baton, apr_pool_t *pool) {
struct recover_baton *b = baton;
svn_fs_t *fs = b->fs;
fs_fs_data_t *ffd = fs->fsap_data;
svn_revnum_t max_rev;
char next_node_id_buf[MAX_KEY_SIZE], next_copy_id_buf[MAX_KEY_SIZE];
char *next_node_id = NULL, *next_copy_id = NULL;
SVN_ERR(recover_get_largest_revision(fs, &max_rev, pool));
if (ffd->format < SVN_FS_FS__MIN_NO_GLOBAL_IDS_FORMAT) {
svn_revnum_t rev;
apr_pool_t *iterpool = svn_pool_create(pool);
char max_node_id[MAX_KEY_SIZE] = "0", max_copy_id[MAX_KEY_SIZE] = "0";
apr_size_t len;
for (rev = 0; rev <= max_rev; rev++) {
apr_file_t *rev_file;
apr_off_t root_offset;
svn_pool_clear(iterpool);
if (b->cancel_func)
SVN_ERR(b->cancel_func(b->cancel_baton));
SVN_ERR(svn_io_file_open(&rev_file,
svn_fs_fs__path_rev(fs, rev, iterpool),
APR_READ | APR_BUFFERED, APR_OS_DEFAULT,
iterpool));
SVN_ERR(get_root_changes_offset(&root_offset, NULL, rev_file,
iterpool));
SVN_ERR(recover_find_max_ids(fs, rev, rev_file, root_offset,
max_node_id, max_copy_id, iterpool));
}
svn_pool_destroy(iterpool);
len = strlen(max_node_id);
svn_fs_fs__next_key(max_node_id, &len, next_node_id_buf);
next_node_id = next_node_id_buf;
len = strlen(max_copy_id);
svn_fs_fs__next_key(max_copy_id, &len, next_copy_id_buf);
next_copy_id = next_copy_id_buf;
}
SVN_ERR(write_current(fs, max_rev, next_node_id, next_copy_id, pool));
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_fs__recover(svn_fs_t *fs,
svn_cancel_func_t cancel_func, void *cancel_baton,
apr_pool_t *pool) {
struct recover_baton b;
b.fs = fs;
b.cancel_func = cancel_func;
b.cancel_baton = cancel_baton;
return svn_fs_fs__with_write_lock(fs, recover_body, &b, pool);
}
svn_error_t *
svn_fs_fs__get_uuid(svn_fs_t *fs,
const char **uuid_p,
apr_pool_t *pool) {
fs_fs_data_t *ffd = fs->fsap_data;
*uuid_p = apr_pstrdup(pool, ffd->uuid);
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_fs__set_uuid(svn_fs_t *fs,
const char *uuid,
apr_pool_t *pool) {
apr_file_t *uuid_file;
const char *tmp_path;
const char *uuid_path = path_uuid(fs, pool);
fs_fs_data_t *ffd = fs->fsap_data;
SVN_ERR(svn_io_open_unique_file2(&uuid_file, &tmp_path, uuid_path,
".tmp", svn_io_file_del_none, pool));
if (! uuid)
uuid = svn_uuid_generate(pool);
SVN_ERR(svn_io_file_write_full(uuid_file, uuid, strlen(uuid), NULL,
pool));
SVN_ERR(svn_io_file_write_full(uuid_file, "\n", 1, NULL, pool));
SVN_ERR(svn_io_file_close(uuid_file, pool));
SVN_ERR(svn_fs_fs__move_into_place(tmp_path, uuid_path,
svn_fs_fs__path_current(fs, pool), pool));
ffd->uuid = apr_pstrdup(fs->pool, uuid);
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_fs__ensure_dir_exists(const char *path,
svn_fs_t *fs,
apr_pool_t *pool) {
svn_error_t *err = svn_io_dir_make(path, APR_OS_DEFAULT, pool);
if (err && APR_STATUS_IS_EEXIST(err->apr_err)) {
svn_error_clear(err);
return SVN_NO_ERROR;
}
SVN_ERR(err);
SVN_ERR(svn_fs_fs__dup_perms(path, fs->path, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
get_node_origins_from_file(svn_fs_t *fs,
apr_hash_t **node_origins,
const char *node_origins_file,
apr_pool_t *pool) {
apr_file_t *fd;
svn_error_t *err;
svn_stream_t *stream;
*node_origins = NULL;
err = svn_io_file_open(&fd, node_origins_file,
APR_READ, APR_OS_DEFAULT, pool);
if (err && APR_STATUS_IS_ENOENT(err->apr_err)) {
svn_error_clear(err);
return SVN_NO_ERROR;
}
SVN_ERR(err);
stream = svn_stream_from_aprfile2(fd, FALSE, pool);
*node_origins = apr_hash_make(pool);
SVN_ERR(svn_hash_read2(*node_origins, stream, SVN_HASH_TERMINATOR, pool));
return svn_stream_close(stream);
}
svn_error_t *
svn_fs_fs__get_node_origin(const svn_fs_id_t **origin_id,
svn_fs_t *fs,
const char *node_id,
apr_pool_t *pool) {
apr_hash_t *node_origins;
*origin_id = NULL;
SVN_ERR(get_node_origins_from_file(fs, &node_origins,
path_node_origin(fs, node_id, pool),
pool));
if (node_origins) {
svn_string_t *origin_id_str =
apr_hash_get(node_origins, node_id, APR_HASH_KEY_STRING);
if (origin_id_str)
*origin_id = svn_fs_fs__id_parse(origin_id_str->data,
origin_id_str->len, pool);
}
return SVN_NO_ERROR;
}
static svn_error_t *
set_node_origins_for_file(svn_fs_t *fs,
const char *node_origins_path,
const char *node_id,
svn_string_t *node_rev_id,
apr_pool_t *pool) {
apr_file_t *fd;
const char *path_tmp;
svn_stream_t *stream;
apr_hash_t *origins_hash;
svn_string_t *old_node_rev_id;
SVN_ERR(svn_fs_fs__ensure_dir_exists(svn_path_join(fs->path,
PATH_NODE_ORIGINS_DIR,
pool),
fs, pool));
SVN_ERR(get_node_origins_from_file(fs, &origins_hash,
node_origins_path, pool));
if (! origins_hash)
origins_hash = apr_hash_make(pool);
old_node_rev_id = apr_hash_get(origins_hash, node_id, APR_HASH_KEY_STRING);
if (old_node_rev_id && !svn_string_compare(node_rev_id, old_node_rev_id))
return svn_error_createf(SVN_ERR_FS_CORRUPT, NULL,
_("Node origin for '%s' exists with a different "
"value (%s) than what we were about to store "
"(%s)"),
node_id, old_node_rev_id->data, node_rev_id->data);
apr_hash_set(origins_hash, node_id, APR_HASH_KEY_STRING, node_rev_id);
SVN_ERR(svn_io_open_unique_file2(&fd, &path_tmp, node_origins_path, ".tmp",
svn_io_file_del_none, pool));
stream = svn_stream_from_aprfile2(fd, FALSE, pool);
SVN_ERR(svn_hash_write2(origins_hash, stream, SVN_HASH_TERMINATOR, pool));
SVN_ERR(svn_stream_close(stream));
SVN_ERR(svn_io_file_rename(path_tmp, node_origins_path, pool));
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_fs__set_node_origin(svn_fs_t *fs,
const char *node_id,
const svn_fs_id_t *node_rev_id,
apr_pool_t *pool) {
svn_error_t *err;
const char *filename = path_node_origin(fs, node_id, pool);
err = set_node_origins_for_file(fs, filename,
node_id,
svn_fs_fs__id_unparse(node_rev_id, pool),
pool);
if (err && APR_STATUS_IS_EACCES(err->apr_err)) {
svn_error_clear(err);
err = NULL;
}
return err;
}
svn_error_t *
svn_fs_fs__list_transactions(apr_array_header_t **names_p,
svn_fs_t *fs,
apr_pool_t *pool) {
const char *txn_dir;
apr_hash_t *dirents;
apr_hash_index_t *hi;
apr_array_header_t *names;
apr_size_t ext_len = strlen(PATH_EXT_TXN);
names = apr_array_make(pool, 1, sizeof(const char *));
txn_dir = svn_path_join(fs->path, PATH_TXNS_DIR, pool);
SVN_ERR(svn_io_get_dirents2(&dirents, txn_dir, pool));
for (hi = apr_hash_first(pool, dirents); hi; hi = apr_hash_next(hi)) {
const void *key;
const char *name, *id;
apr_ssize_t klen;
apr_hash_this(hi, &key, &klen, NULL);
name = key;
if ((apr_size_t) klen <= ext_len
|| (strcmp(name + klen - ext_len, PATH_EXT_TXN)) != 0)
continue;
id = apr_pstrndup(pool, name, strlen(name) - ext_len);
APR_ARRAY_PUSH(names, const char *) = id;
}
*names_p = names;
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_fs__open_txn(svn_fs_txn_t **txn_p,
svn_fs_t *fs,
const char *name,
apr_pool_t *pool) {
svn_fs_txn_t *txn;
svn_node_kind_t kind;
transaction_t *local_txn;
SVN_ERR(svn_io_check_path(path_txn_dir(fs, name, pool), &kind, pool));
if (kind != svn_node_dir)
return svn_error_create(SVN_ERR_FS_NO_SUCH_TRANSACTION, NULL,
_("No such transaction"));
txn = apr_pcalloc(pool, sizeof(*txn));
txn->id = apr_pstrdup(pool, name);
txn->fs = fs;
SVN_ERR(svn_fs_fs__get_txn(&local_txn, fs, name, pool));
txn->base_rev = svn_fs_fs__id_rev(local_txn->base_id);
txn->vtable = &txn_vtable;
*txn_p = txn;
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_fs__txn_proplist(apr_hash_t **table_p,
svn_fs_txn_t *txn,
apr_pool_t *pool) {
apr_hash_t *proplist = apr_hash_make(pool);
SVN_ERR(get_txn_proplist(proplist, txn->fs, txn->id, pool));
*table_p = proplist;
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_fs__delete_node_revision(svn_fs_t *fs,
const svn_fs_id_t *id,
apr_pool_t *pool) {
node_revision_t *noderev;
SVN_ERR(svn_fs_fs__get_node_revision(&noderev, fs, id, pool));
if (noderev->prop_rep && noderev->prop_rep->txn_id)
SVN_ERR(svn_io_remove_file(path_txn_node_props(fs, id, pool), pool));
if (noderev->data_rep && noderev->data_rep->txn_id
&& noderev->kind == svn_node_dir)
SVN_ERR(svn_io_remove_file(path_txn_node_children(fs, id, pool), pool));
return svn_io_remove_file(path_txn_node_rev(fs, id, pool), pool);
}
svn_error_t *
svn_fs_fs__revision_prop(svn_string_t **value_p,
svn_fs_t *fs,
svn_revnum_t rev,
const char *propname,
apr_pool_t *pool) {
apr_hash_t *table;
SVN_ERR(svn_fs__check_fs(fs, TRUE));
SVN_ERR(svn_fs_fs__revision_proplist(&table, fs, rev, pool));
*value_p = apr_hash_get(table, propname, APR_HASH_KEY_STRING);
return SVN_NO_ERROR;
}
struct change_rev_prop_baton {
svn_fs_t *fs;
svn_revnum_t rev;
const char *name;
const svn_string_t *value;
};
static svn_error_t *
change_rev_prop_body(void *baton, apr_pool_t *pool) {
struct change_rev_prop_baton *cb = baton;
apr_hash_t *table;
SVN_ERR(svn_fs_fs__revision_proplist(&table, cb->fs, cb->rev, pool));
apr_hash_set(table, cb->name, APR_HASH_KEY_STRING, cb->value);
SVN_ERR(svn_fs_fs__set_revision_proplist(cb->fs, cb->rev, table, pool));
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_fs__change_rev_prop(svn_fs_t *fs,
svn_revnum_t rev,
const char *name,
const svn_string_t *value,
apr_pool_t *pool) {
struct change_rev_prop_baton cb;
SVN_ERR(svn_fs__check_fs(fs, TRUE));
cb.fs = fs;
cb.rev = rev;
cb.name = name;
cb.value = value;
return svn_fs_fs__with_write_lock(fs, change_rev_prop_body, &cb, pool);
}
svn_error_t *
svn_fs_fs__get_txn_ids(const svn_fs_id_t **root_id_p,
const svn_fs_id_t **base_root_id_p,
svn_fs_t *fs,
const char *txn_name,
apr_pool_t *pool) {
transaction_t *txn;
SVN_ERR(svn_fs_fs__get_txn(&txn, fs, txn_name, pool));
*root_id_p = txn->root_id;
*base_root_id_p = txn->base_id;
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_fs__txn_prop(svn_string_t **value_p,
svn_fs_txn_t *txn,
const char *propname,
apr_pool_t *pool) {
apr_hash_t *table;
svn_fs_t *fs = txn->fs;
SVN_ERR(svn_fs__check_fs(fs, TRUE));
SVN_ERR(svn_fs_fs__txn_proplist(&table, txn, pool));
*value_p = apr_hash_get(table, propname, APR_HASH_KEY_STRING);
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_fs__begin_txn(svn_fs_txn_t **txn_p,
svn_fs_t *fs,
svn_revnum_t rev,
apr_uint32_t flags,
apr_pool_t *pool) {
svn_string_t date;
svn_prop_t prop;
apr_array_header_t *props = apr_array_make(pool, 3, sizeof(svn_prop_t));
SVN_ERR(svn_fs__check_fs(fs, TRUE));
SVN_ERR(svn_fs_fs__create_txn(txn_p, fs, rev, pool));
date.data = svn_time_to_cstring(apr_time_now(), pool);
date.len = strlen(date.data);
prop.name = SVN_PROP_REVISION_DATE;
prop.value = &date;
APR_ARRAY_PUSH(props, svn_prop_t) = prop;
if (flags & SVN_FS_TXN_CHECK_OOD) {
prop.name = SVN_FS__PROP_TXN_CHECK_OOD;
prop.value = svn_string_create("true", pool);
APR_ARRAY_PUSH(props, svn_prop_t) = prop;
}
if (flags & SVN_FS_TXN_CHECK_LOCKS) {
prop.name = SVN_FS__PROP_TXN_CHECK_LOCKS;
prop.value = svn_string_create("true", pool);
APR_ARRAY_PUSH(props, svn_prop_t) = prop;
}
return svn_fs_fs__change_txn_props(*txn_p, props, pool);
}
