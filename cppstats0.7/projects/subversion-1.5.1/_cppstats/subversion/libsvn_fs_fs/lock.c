#include <assert.h>
#include "svn_pools.h"
#include "svn_error.h"
#include "svn_path.h"
#include "svn_fs.h"
#include "svn_hash.h"
#include "svn_time.h"
#include "svn_utf.h"
#include "svn_md5.h"
#include <apr_uuid.h>
#include <apr_file_io.h>
#include <apr_file_info.h>
#include <apr_md5.h>
#include "lock.h"
#include "tree.h"
#include "err.h"
#include "fs_fs.h"
#include "../libsvn_fs/fs-loader.h"
#include "private/svn_fs_util.h"
#include "svn_private_config.h"
#define PATH_KEY "path"
#define TOKEN_KEY "token"
#define OWNER_KEY "owner"
#define CREATION_DATE_KEY "creation_date"
#define EXPIRATION_DATE_KEY "expiration_date"
#define COMMENT_KEY "comment"
#define IS_DAV_COMMENT_KEY "is_dav_comment"
#define CHILDREN_KEY "children"
#define DIGEST_SUBDIR_LEN 3
static const char *
make_digest(const char *str,
apr_pool_t *pool) {
unsigned char digest[APR_MD5_DIGESTSIZE];
apr_md5(digest, str, strlen(str));
return svn_md5_digest_to_cstring_display(digest, pool);
}
static void
hash_store(apr_hash_t *hash,
const char *key,
apr_ssize_t key_len,
const char *value,
apr_ssize_t value_len,
apr_pool_t *pool) {
if (! (key && value))
return;
if (value_len == APR_HASH_KEY_STRING)
value_len = strlen(value);
apr_hash_set(hash, key, key_len,
svn_string_ncreate(value, value_len, pool));
}
static const char *
hash_fetch(apr_hash_t *hash,
const char *key,
apr_pool_t *pool) {
svn_string_t *str = apr_hash_get(hash, key, APR_HASH_KEY_STRING);
return str ? str->data : NULL;
}
static const char *
digest_path_from_digest(svn_fs_t *fs,
const char *digest,
apr_pool_t *pool) {
return svn_path_join_many(pool, fs->path, PATH_LOCKS_DIR,
apr_pstrmemdup(pool, digest, DIGEST_SUBDIR_LEN),
digest, NULL);
}
static const char *
digest_path_from_path(svn_fs_t *fs,
const char *path,
apr_pool_t *pool) {
const char *digest = make_digest(path, pool);
return svn_path_join_many(pool, fs->path, PATH_LOCKS_DIR,
apr_pstrmemdup(pool, digest, DIGEST_SUBDIR_LEN),
digest, NULL);
}
static svn_error_t *
write_digest_file(apr_hash_t *children,
svn_lock_t *lock,
svn_fs_t *fs,
const char *digest_path,
apr_pool_t *pool) {
svn_error_t *err = SVN_NO_ERROR;
apr_file_t *fd;
apr_hash_index_t *hi;
apr_hash_t *hash = apr_hash_make(pool);
const char *tmp_path;
SVN_ERR(svn_fs_fs__ensure_dir_exists(svn_path_join(fs->path, PATH_LOCKS_DIR,
pool), fs, pool));
SVN_ERR(svn_fs_fs__ensure_dir_exists(svn_path_dirname(digest_path, pool), fs,
pool));
SVN_ERR(svn_io_open_unique_file2
(&fd, &tmp_path, digest_path, ".tmp", svn_io_file_del_none, pool));
if (lock) {
const char *creation_date = NULL, *expiration_date = NULL;
if (lock->creation_date)
creation_date = svn_time_to_cstring(lock->creation_date, pool);
if (lock->expiration_date)
expiration_date = svn_time_to_cstring(lock->expiration_date, pool);
hash_store(hash, PATH_KEY, sizeof(PATH_KEY)-1,
lock->path, APR_HASH_KEY_STRING, pool);
hash_store(hash, TOKEN_KEY, sizeof(TOKEN_KEY)-1,
lock->token, APR_HASH_KEY_STRING, pool);
hash_store(hash, OWNER_KEY, sizeof(OWNER_KEY)-1,
lock->owner, APR_HASH_KEY_STRING, pool);
hash_store(hash, COMMENT_KEY, sizeof(COMMENT_KEY)-1,
lock->comment, APR_HASH_KEY_STRING, pool);
hash_store(hash, IS_DAV_COMMENT_KEY, sizeof(IS_DAV_COMMENT_KEY)-1,
lock->is_dav_comment ? "1" : "0", 1, pool);
hash_store(hash, CREATION_DATE_KEY, sizeof(CREATION_DATE_KEY)-1,
creation_date, APR_HASH_KEY_STRING, pool);
hash_store(hash, EXPIRATION_DATE_KEY, sizeof(EXPIRATION_DATE_KEY)-1,
expiration_date, APR_HASH_KEY_STRING, pool);
}
if (apr_hash_count(children)) {
svn_stringbuf_t *children_list = svn_stringbuf_create("", pool);
for (hi = apr_hash_first(pool, children); hi; hi = apr_hash_next(hi)) {
const void *key;
apr_ssize_t klen;
apr_hash_this(hi, &key, &klen, NULL);
svn_stringbuf_appendbytes(children_list, key, klen);
svn_stringbuf_appendbytes(children_list, "\n", 1);
}
hash_store(hash, CHILDREN_KEY, sizeof(CHILDREN_KEY)-1,
children_list->data, children_list->len, pool);
}
if ((err = svn_hash_write2(hash,
svn_stream_from_aprfile(fd, pool),
SVN_HASH_TERMINATOR, pool))) {
svn_error_clear(svn_io_file_close(fd, pool));
return svn_error_createf(err->apr_err,
err,
_("Cannot write lock/entries hashfile '%s'"),
svn_path_local_style(tmp_path, pool));
}
SVN_ERR(svn_io_file_close(fd, pool));
SVN_ERR(svn_io_file_rename(tmp_path, digest_path, pool));
SVN_ERR(svn_fs_fs__dup_perms
(digest_path, svn_fs_fs__path_rev(fs, 0, pool), pool));
return SVN_NO_ERROR;
}
static svn_error_t *
read_digest_file(apr_hash_t **children_p,
svn_lock_t **lock_p,
svn_fs_t *fs,
const char *digest_path,
apr_pool_t *pool) {
svn_error_t *err = SVN_NO_ERROR;
svn_lock_t *lock;
apr_hash_t *hash;
apr_file_t *fd;
const char *val;
if (lock_p)
*lock_p = NULL;
if (children_p)
*children_p = apr_hash_make(pool);
err = svn_io_file_open(&fd, digest_path, APR_READ, APR_OS_DEFAULT, pool);
if (err && APR_STATUS_IS_ENOENT(err->apr_err)) {
svn_error_clear(err);
return SVN_NO_ERROR;
}
SVN_ERR(err);
if (! (lock_p || children_p))
return svn_io_file_close(fd, pool);
hash = apr_hash_make(pool);
if ((err = svn_hash_read2(hash,
svn_stream_from_aprfile(fd, pool),
SVN_HASH_TERMINATOR, pool))) {
svn_error_clear(svn_io_file_close(fd, pool));
return svn_error_createf(err->apr_err,
err,
_("Can't parse lock/entries hashfile '%s'"),
svn_path_local_style(digest_path, pool));
}
SVN_ERR(svn_io_file_close(fd, pool));
val = hash_fetch(hash, PATH_KEY, pool);
if (val && lock_p) {
const char *path = val;
lock = svn_lock_create(pool);
lock->path = path;
if (! ((lock->token = hash_fetch(hash, TOKEN_KEY, pool))))
return svn_fs_fs__err_corrupt_lockfile(fs, path);
if (! ((lock->owner = hash_fetch(hash, OWNER_KEY, pool))))
return svn_fs_fs__err_corrupt_lockfile(fs, path);
if (! ((val = hash_fetch(hash, IS_DAV_COMMENT_KEY, pool))))
return svn_fs_fs__err_corrupt_lockfile(fs, path);
lock->is_dav_comment = (val[0] == '1') ? TRUE : FALSE;
if (! ((val = hash_fetch(hash, CREATION_DATE_KEY, pool))))
return svn_fs_fs__err_corrupt_lockfile(fs, path);
SVN_ERR(svn_time_from_cstring(&(lock->creation_date), val, pool));
if ((val = hash_fetch(hash, EXPIRATION_DATE_KEY, pool)))
SVN_ERR(svn_time_from_cstring(&(lock->expiration_date), val, pool));
lock->comment = hash_fetch(hash, COMMENT_KEY, pool);
*lock_p = lock;
}
val = hash_fetch(hash, CHILDREN_KEY, pool);
if (val && children_p) {
apr_array_header_t *kiddos = svn_cstring_split(val, "\n", FALSE, pool);
int i;
for (i = 0; i < kiddos->nelts; i++) {
apr_hash_set(*children_p, APR_ARRAY_IDX(kiddos, i, const char *),
APR_HASH_KEY_STRING, (void *)1);
}
}
return SVN_NO_ERROR;
}
static svn_error_t *
set_lock(svn_fs_t *fs,
svn_lock_t *lock,
apr_pool_t *pool) {
svn_stringbuf_t *this_path = svn_stringbuf_create(lock->path, pool);
svn_stringbuf_t *last_child = svn_stringbuf_create("", pool);
apr_pool_t *subpool;
assert(lock);
subpool = svn_pool_create(pool);
while (1729) {
const char *digest_path, *parent_dir, *digest_file;
apr_hash_t *this_children;
svn_lock_t *this_lock;
svn_pool_clear(subpool);
digest_path = digest_path_from_path(fs, this_path->data, subpool);
svn_path_split(digest_path, &parent_dir, &digest_file, subpool);
SVN_ERR(read_digest_file(&this_children, &this_lock, fs,
digest_path, subpool));
if (lock) {
this_lock = lock;
lock = NULL;
svn_stringbuf_set(last_child, digest_file);
} else {
if (apr_hash_get(this_children, last_child->data, last_child->len))
break;
apr_hash_set(this_children, last_child->data,
last_child->len, (void *)1);
}
SVN_ERR(write_digest_file(this_children, this_lock, fs,
digest_path, subpool));
if ((this_path->len == 1) && (this_path->data[0] == '/'))
break;
svn_stringbuf_set(this_path,
svn_path_dirname(this_path->data, subpool));
}
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
static svn_error_t *
delete_lock(svn_fs_t *fs,
svn_lock_t *lock,
apr_pool_t *pool) {
svn_stringbuf_t *this_path = svn_stringbuf_create(lock->path, pool);
svn_stringbuf_t *child_to_kill = svn_stringbuf_create("", pool);
apr_pool_t *subpool;
assert(lock);
subpool = svn_pool_create(pool);
while (1729) {
const char *digest_path, *parent_dir, *digest_file;
apr_hash_t *this_children;
svn_lock_t *this_lock;
svn_pool_clear(subpool);
digest_path = digest_path_from_path(fs, this_path->data, subpool);
svn_path_split(digest_path, &parent_dir, &digest_file, subpool);
SVN_ERR(read_digest_file(&this_children, &this_lock, fs,
digest_path, subpool));
if (child_to_kill->len)
apr_hash_set(this_children, child_to_kill->data,
child_to_kill->len, NULL);
if (lock) {
this_lock = NULL;
lock = NULL;
}
if (! (this_lock || apr_hash_count(this_children) != 0)) {
svn_stringbuf_set(child_to_kill,
svn_path_basename(digest_path, subpool));
SVN_ERR(svn_io_remove_file(digest_path, subpool));
} else {
SVN_ERR(write_digest_file(this_children, this_lock, fs,
digest_path, subpool));
svn_stringbuf_setempty(child_to_kill);
}
if ((this_path->len == 1) && (this_path->data[0] == '/'))
break;
svn_stringbuf_set(this_path,
svn_path_dirname(this_path->data, subpool));
}
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
static svn_error_t *
get_lock(svn_lock_t **lock_p,
svn_fs_t *fs,
const char *path,
svn_boolean_t have_write_lock,
apr_pool_t *pool) {
svn_lock_t *lock;
const char *digest_path = digest_path_from_path(fs, path, pool);
SVN_ERR(read_digest_file(NULL, &lock, fs, digest_path, pool));
if (! lock)
return SVN_FS__ERR_NO_SUCH_LOCK(fs, path);
if (lock->expiration_date && (apr_time_now() > lock->expiration_date)) {
if (have_write_lock)
SVN_ERR(delete_lock(fs, lock, pool));
*lock_p = NULL;
return SVN_FS__ERR_LOCK_EXPIRED(fs, lock->token);
}
*lock_p = lock;
return SVN_NO_ERROR;
}
static svn_error_t *
get_lock_helper(svn_fs_t *fs,
svn_lock_t **lock_p,
const char *path,
svn_boolean_t have_write_lock,
apr_pool_t *pool) {
svn_lock_t *lock;
svn_error_t *err;
err = get_lock(&lock, fs, path, have_write_lock, pool);
if (err && ((err->apr_err == SVN_ERR_FS_NO_SUCH_LOCK)
|| (err->apr_err == SVN_ERR_FS_LOCK_EXPIRED))) {
svn_error_clear(err);
*lock_p = NULL;
return SVN_NO_ERROR;
} else
SVN_ERR(err);
*lock_p = lock;
return SVN_NO_ERROR;
}
static svn_error_t *
walk_digest_files(svn_fs_t *fs,
const char *digest_path,
svn_fs_get_locks_callback_t get_locks_func,
void *get_locks_baton,
svn_boolean_t have_write_lock,
apr_pool_t *pool) {
apr_hash_t *children;
svn_lock_t *lock;
apr_hash_index_t *hi;
apr_pool_t *subpool;
SVN_ERR(read_digest_file(&children, &lock, fs, digest_path, pool));
if (lock) {
if (lock->expiration_date == 0
|| (apr_time_now() <= lock->expiration_date)) {
if (get_locks_func)
SVN_ERR(get_locks_func(get_locks_baton, lock, pool));
} else {
if (have_write_lock)
SVN_ERR(delete_lock(fs, lock, pool));
}
}
if (! apr_hash_count(children))
return SVN_NO_ERROR;
subpool = svn_pool_create(pool);
for (hi = apr_hash_first(pool, children); hi; hi = apr_hash_next(hi)) {
const void *key;
svn_pool_clear(subpool);
apr_hash_this(hi, &key, NULL, NULL);
SVN_ERR(walk_digest_files
(fs, digest_path_from_digest(fs, key, subpool),
get_locks_func, get_locks_baton, have_write_lock, subpool));
}
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
static svn_error_t *
verify_lock(svn_fs_t *fs,
svn_lock_t *lock,
apr_pool_t *pool) {
if ((! fs->access_ctx) || (! fs->access_ctx->username))
return svn_error_createf
(SVN_ERR_FS_NO_USER, NULL,
_("Cannot verify lock on path '%s'; no username available"),
lock->path);
else if (strcmp(fs->access_ctx->username, lock->owner) != 0)
return svn_error_createf
(SVN_ERR_FS_LOCK_OWNER_MISMATCH, NULL,
_("User %s does not own lock on path '%s' (currently locked by %s)"),
fs->access_ctx->username, lock->path, lock->owner);
else if (apr_hash_get(fs->access_ctx->lock_tokens, lock->token,
APR_HASH_KEY_STRING) == NULL)
return svn_error_createf
(SVN_ERR_FS_BAD_LOCK_TOKEN, NULL,
_("Cannot verify lock on path '%s'; no matching lock-token available"),
lock->path);
return SVN_NO_ERROR;
}
static svn_error_t *
get_locks_callback(void *baton,
svn_lock_t *lock,
apr_pool_t *pool) {
return verify_lock(baton, lock, pool);
}
svn_error_t *
svn_fs_fs__allow_locked_operation(const char *path,
svn_fs_t *fs,
svn_boolean_t recurse,
svn_boolean_t have_write_lock,
apr_pool_t *pool) {
path = svn_fs__canonicalize_abspath(path, pool);
if (recurse) {
const char *digest_path = digest_path_from_path(fs, path, pool);
SVN_ERR(walk_digest_files(fs, digest_path, get_locks_callback,
fs, have_write_lock, pool));
} else {
svn_lock_t *lock;
SVN_ERR(get_lock_helper(fs, &lock, path, have_write_lock, pool));
if (lock)
SVN_ERR(verify_lock(fs, lock, pool));
}
return SVN_NO_ERROR;
}
struct lock_baton {
svn_lock_t **lock_p;
svn_fs_t *fs;
const char *path;
const char *token;
const char *comment;
svn_boolean_t is_dav_comment;
apr_time_t expiration_date;
svn_revnum_t current_rev;
svn_boolean_t steal_lock;
apr_pool_t *pool;
};
static svn_error_t *
lock_body(void *baton, apr_pool_t *pool) {
struct lock_baton *lb = baton;
svn_node_kind_t kind;
svn_lock_t *existing_lock;
svn_lock_t *lock;
svn_fs_root_t *root;
svn_revnum_t youngest;
SVN_ERR(lb->fs->vtable->youngest_rev(&youngest, lb->fs, pool));
SVN_ERR(lb->fs->vtable->revision_root(&root, lb->fs, youngest, pool));
SVN_ERR(svn_fs_fs__check_path(&kind, root, lb->path, pool));
if (kind == svn_node_dir)
return SVN_FS__ERR_NOT_FILE(lb->fs, lb->path);
if (kind == svn_node_none)
return svn_error_createf(SVN_ERR_FS_NOT_FOUND, NULL,
_("Path '%s' doesn't exist in HEAD revision"),
lb->path);
if (!lb->fs->access_ctx || !lb->fs->access_ctx->username)
return SVN_FS__ERR_NO_USER(lb->fs);
if (SVN_IS_VALID_REVNUM(lb->current_rev)) {
svn_revnum_t created_rev;
SVN_ERR(svn_fs_fs__node_created_rev(&created_rev, root, lb->path,
pool));
if (! SVN_IS_VALID_REVNUM(created_rev))
return svn_error_createf
(SVN_ERR_FS_OUT_OF_DATE, NULL,
_("Path '%s' doesn't exist in HEAD revision"), lb->path);
if (lb->current_rev < created_rev)
return svn_error_createf
(SVN_ERR_FS_OUT_OF_DATE, NULL,
_("Lock failed: newer version of '%s' exists"), lb->path);
}
SVN_ERR(get_lock_helper(lb->fs, &existing_lock, lb->path, TRUE, pool));
if (existing_lock) {
if (! lb->steal_lock) {
return SVN_FS__ERR_PATH_ALREADY_LOCKED(lb->fs, existing_lock);
} else {
SVN_ERR(delete_lock(lb->fs, existing_lock, pool));
}
}
lock = svn_lock_create(lb->pool);
if (lb->token)
lock->token = apr_pstrdup(lb->pool, lb->token);
else
SVN_ERR(svn_fs_fs__generate_lock_token(&(lock->token), lb->fs,
lb->pool));
lock->path = apr_pstrdup(lb->pool, lb->path);
lock->owner = apr_pstrdup(lb->pool, lb->fs->access_ctx->username);
lock->comment = apr_pstrdup(lb->pool, lb->comment);
lock->is_dav_comment = lb->is_dav_comment;
lock->creation_date = apr_time_now();
lock->expiration_date = lb->expiration_date;
SVN_ERR(set_lock(lb->fs, lock, pool));
*lb->lock_p = lock;
return SVN_NO_ERROR;
}
struct unlock_baton {
svn_fs_t *fs;
const char *path;
const char *token;
svn_boolean_t break_lock;
};
static svn_error_t *
unlock_body(void *baton, apr_pool_t *pool) {
struct unlock_baton *ub = baton;
svn_lock_t *lock;
SVN_ERR(get_lock(&lock, ub->fs, ub->path, TRUE, pool));
if (! ub->break_lock) {
if (strcmp(ub->token, lock->token) != 0)
return SVN_FS__ERR_NO_SUCH_LOCK(ub->fs, lock->path);
if (! (ub->fs->access_ctx && ub->fs->access_ctx->username))
return SVN_FS__ERR_NO_USER(ub->fs);
if (strcmp(ub->fs->access_ctx->username, lock->owner) != 0)
return SVN_FS__ERR_LOCK_OWNER_MISMATCH
(ub->fs, ub->fs->access_ctx->username, lock->owner);
}
SVN_ERR(delete_lock(ub->fs, lock, pool));
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_fs__lock(svn_lock_t **lock_p,
svn_fs_t *fs,
const char *path,
const char *token,
const char *comment,
svn_boolean_t is_dav_comment,
apr_time_t expiration_date,
svn_revnum_t current_rev,
svn_boolean_t steal_lock,
apr_pool_t *pool) {
struct lock_baton lb;
SVN_ERR(svn_fs__check_fs(fs, TRUE));
path = svn_fs__canonicalize_abspath(path, pool);
lb.lock_p = lock_p;
lb.fs = fs;
lb.path = path;
lb.token = token;
lb.comment = comment;
lb.is_dav_comment = is_dav_comment;
lb.expiration_date = expiration_date;
lb.current_rev = current_rev;
lb.steal_lock = steal_lock;
lb.pool = pool;
SVN_ERR(svn_fs_fs__with_write_lock(fs, lock_body, &lb, pool));
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_fs__generate_lock_token(const char **token,
svn_fs_t *fs,
apr_pool_t *pool) {
SVN_ERR(svn_fs__check_fs(fs, TRUE));
*token = apr_pstrcat(pool, "opaquelocktoken:",
svn_uuid_generate(pool), NULL);
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_fs__unlock(svn_fs_t *fs,
const char *path,
const char *token,
svn_boolean_t break_lock,
apr_pool_t *pool) {
struct unlock_baton ub;
SVN_ERR(svn_fs__check_fs(fs, TRUE));
path = svn_fs__canonicalize_abspath(path, pool);
ub.fs = fs;
ub.path = path;
ub.token = token;
ub.break_lock = break_lock;
SVN_ERR(svn_fs_fs__with_write_lock(fs, unlock_body, &ub, pool));
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_fs__get_lock(svn_lock_t **lock_p,
svn_fs_t *fs,
const char *path,
apr_pool_t *pool) {
SVN_ERR(svn_fs__check_fs(fs, TRUE));
path = svn_fs__canonicalize_abspath(path, pool);
return get_lock_helper(fs, lock_p, path, FALSE, pool);
}
svn_error_t *
svn_fs_fs__get_locks(svn_fs_t *fs,
const char *path,
svn_fs_get_locks_callback_t get_locks_func,
void *get_locks_baton,
apr_pool_t *pool) {
const char *digest_path;
SVN_ERR(svn_fs__check_fs(fs, TRUE));
path = svn_fs__canonicalize_abspath(path, pool);
digest_path = digest_path_from_path(fs, path, pool);
return walk_digest_files(fs, digest_path, get_locks_func,
get_locks_baton, FALSE, pool);
}
