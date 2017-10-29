#include "svn_pools.h"
#include "svn_error.h"
#include "svn_fs.h"
#include "svn_private_config.h"
#include <apr_uuid.h>
#include "lock.h"
#include "tree.h"
#include "err.h"
#include "bdb/locks-table.h"
#include "bdb/lock-tokens-table.h"
#include "../libsvn_fs/fs-loader.h"
#include "private/svn_fs_util.h"
static svn_error_t *
add_lock_and_token(svn_lock_t *lock,
const char *lock_token,
const char *path,
trail_t *trail) {
SVN_ERR(svn_fs_bdb__lock_add(trail->fs, lock_token, lock,
trail, trail->pool));
SVN_ERR(svn_fs_bdb__lock_token_add(trail->fs, path, lock_token,
trail, trail->pool));
return SVN_NO_ERROR;
}
static svn_error_t *
delete_lock_and_token(const char *lock_token,
const char *path,
trail_t *trail) {
SVN_ERR(svn_fs_bdb__lock_delete(trail->fs, lock_token,
trail, trail->pool));
SVN_ERR(svn_fs_bdb__lock_token_delete(trail->fs, path,
trail, trail->pool));
return SVN_NO_ERROR;
}
struct lock_args {
svn_lock_t **lock_p;
const char *path;
const char *token;
const char *comment;
svn_boolean_t is_dav_comment;
svn_boolean_t steal_lock;
apr_time_t expiration_date;
svn_revnum_t current_rev;
};
static svn_error_t *
txn_body_lock(void *baton, trail_t *trail) {
struct lock_args *args = baton;
svn_node_kind_t kind = svn_node_file;
svn_lock_t *existing_lock;
const char *fs_username;
svn_lock_t *lock;
SVN_ERR(svn_fs_base__get_path_kind(&kind, args->path, trail, trail->pool));
if (kind == svn_node_dir)
return SVN_FS__ERR_NOT_FILE(trail->fs, args->path);
if (kind == svn_node_none)
return svn_error_createf(SVN_ERR_FS_NOT_FOUND, NULL,
"Path '%s' doesn't exist in HEAD revision",
args->path);
if (!trail->fs->access_ctx || !trail->fs->access_ctx->username)
return SVN_FS__ERR_NO_USER(trail->fs);
else
fs_username = trail->fs->access_ctx->username;
if (SVN_IS_VALID_REVNUM(args->current_rev)) {
svn_revnum_t created_rev;
SVN_ERR(svn_fs_base__get_path_created_rev(&created_rev, args->path,
trail, trail->pool));
if (! SVN_IS_VALID_REVNUM(created_rev))
return svn_error_createf(SVN_ERR_FS_OUT_OF_DATE, NULL,
"Path '%s' doesn't exist in HEAD revision",
args->path);
if (args->current_rev < created_rev)
return svn_error_createf(SVN_ERR_FS_OUT_OF_DATE, NULL,
"Lock failed: newer version of '%s' exists",
args->path);
}
if (args->token) {
svn_lock_t *lock_from_token;
svn_error_t *err = svn_fs_bdb__lock_get(&lock_from_token, trail->fs,
args->token, trail,
trail->pool);
if (err && ((err->apr_err == SVN_ERR_FS_LOCK_EXPIRED)
|| (err->apr_err == SVN_ERR_FS_BAD_LOCK_TOKEN))) {
svn_error_clear(err);
} else {
SVN_ERR(err);
if (strcmp(lock_from_token->path, args->path) != 0)
return svn_error_create(SVN_ERR_FS_BAD_LOCK_TOKEN, NULL,
"Lock failed: token refers to existing "
"lock with non-matching path.");
}
}
SVN_ERR(svn_fs_base__get_lock_helper(&existing_lock, args->path,
trail, trail->pool));
if (existing_lock) {
if (! args->steal_lock) {
return SVN_FS__ERR_PATH_ALREADY_LOCKED(trail->fs,
existing_lock);
} else {
SVN_ERR(delete_lock_and_token(existing_lock->token,
existing_lock->path, trail));
}
}
lock = svn_lock_create(trail->pool);
if (args->token)
lock->token = apr_pstrdup(trail->pool, args->token);
else
SVN_ERR(svn_fs_base__generate_lock_token(&(lock->token), trail->fs,
trail->pool));
lock->path = apr_pstrdup(trail->pool, args->path);
lock->owner = apr_pstrdup(trail->pool, trail->fs->access_ctx->username);
lock->comment = apr_pstrdup(trail->pool, args->comment);
lock->is_dav_comment = args->is_dav_comment;
lock->creation_date = apr_time_now();
lock->expiration_date = args->expiration_date;
SVN_ERR(add_lock_and_token(lock, lock->token, args->path, trail));
*(args->lock_p) = lock;
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_base__lock(svn_lock_t **lock,
svn_fs_t *fs,
const char *path,
const char *token,
const char *comment,
svn_boolean_t is_dav_comment,
apr_time_t expiration_date,
svn_revnum_t current_rev,
svn_boolean_t steal_lock,
apr_pool_t *pool) {
struct lock_args args;
SVN_ERR(svn_fs__check_fs(fs, TRUE));
args.lock_p = lock;
args.path = svn_fs__canonicalize_abspath(path, pool);
args.token = token;
args.comment = comment;
args.is_dav_comment = is_dav_comment;
args.steal_lock = steal_lock;
args.expiration_date = expiration_date;
args.current_rev = current_rev;
return svn_fs_base__retry_txn(fs, txn_body_lock, &args, pool);
}
svn_error_t *
svn_fs_base__generate_lock_token(const char **token,
svn_fs_t *fs,
apr_pool_t *pool) {
*token = apr_pstrcat(pool, "opaquelocktoken:",
svn_uuid_generate(pool), NULL);
return SVN_NO_ERROR;
}
struct unlock_args {
const char *path;
const char *token;
svn_boolean_t break_lock;
};
static svn_error_t *
txn_body_unlock(void *baton, trail_t *trail) {
struct unlock_args *args = baton;
const char *lock_token;
svn_lock_t *lock;
SVN_ERR(svn_fs_bdb__lock_token_get(&lock_token, trail->fs, args->path,
trail, trail->pool));
if (!args->break_lock) {
if (args->token == NULL)
return svn_fs_base__err_no_lock_token(trail->fs, args->path);
else if (strcmp(lock_token, args->token) != 0)
return SVN_FS__ERR_NO_SUCH_LOCK(trail->fs, args->path);
SVN_ERR(svn_fs_bdb__lock_get(&lock, trail->fs, lock_token,
trail, trail->pool));
if (!trail->fs->access_ctx || !trail->fs->access_ctx->username)
return SVN_FS__ERR_NO_USER(trail->fs);
if (strcmp(trail->fs->access_ctx->username, lock->owner) != 0)
return SVN_FS__ERR_LOCK_OWNER_MISMATCH
(trail->fs,
trail->fs->access_ctx->username,
lock->owner);
}
SVN_ERR(delete_lock_and_token(lock_token, args->path, trail));
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_base__unlock(svn_fs_t *fs,
const char *path,
const char *token,
svn_boolean_t break_lock,
apr_pool_t *pool) {
struct unlock_args args;
SVN_ERR(svn_fs__check_fs(fs, TRUE));
args.path = svn_fs__canonicalize_abspath(path, pool);
args.token = token;
args.break_lock = break_lock;
return svn_fs_base__retry_txn(fs, txn_body_unlock, &args, pool);
}
svn_error_t *
svn_fs_base__get_lock_helper(svn_lock_t **lock_p,
const char *path,
trail_t *trail,
apr_pool_t *pool) {
const char *lock_token;
svn_error_t *err;
err = svn_fs_bdb__lock_token_get(&lock_token, trail->fs, path,
trail, pool);
if (err && ((err->apr_err == SVN_ERR_FS_NO_SUCH_LOCK)
|| (err->apr_err == SVN_ERR_FS_LOCK_EXPIRED)
|| (err->apr_err == SVN_ERR_FS_BAD_LOCK_TOKEN))) {
svn_error_clear(err);
*lock_p = NULL;
return SVN_NO_ERROR;
} else
SVN_ERR(err);
err = svn_fs_bdb__lock_get(lock_p, trail->fs, lock_token, trail, pool);
if (err && ((err->apr_err == SVN_ERR_FS_LOCK_EXPIRED)
|| (err->apr_err == SVN_ERR_FS_BAD_LOCK_TOKEN))) {
svn_error_clear(err);
*lock_p = NULL;
return SVN_NO_ERROR;
} else
SVN_ERR(err);
return err;
}
struct lock_token_get_args {
svn_lock_t **lock_p;
const char *path;
};
static svn_error_t *
txn_body_get_lock(void *baton, trail_t *trail) {
struct lock_token_get_args *args = baton;
return svn_fs_base__get_lock_helper(args->lock_p, args->path,
trail, trail->pool);
}
svn_error_t *
svn_fs_base__get_lock(svn_lock_t **lock,
svn_fs_t *fs,
const char *path,
apr_pool_t *pool) {
struct lock_token_get_args args;
SVN_ERR(svn_fs__check_fs(fs, TRUE));
args.path = svn_fs__canonicalize_abspath(path, pool);
args.lock_p = lock;
return svn_fs_base__retry_txn(fs, txn_body_get_lock, &args, pool);
}
struct locks_get_args {
const char *path;
svn_fs_get_locks_callback_t get_locks_func;
void *get_locks_baton;
};
static svn_error_t *
txn_body_get_locks(void *baton, trail_t *trail) {
struct locks_get_args *args = baton;
return svn_fs_bdb__locks_get(trail->fs, args->path,
args->get_locks_func, args->get_locks_baton,
trail, trail->pool);
}
svn_error_t *
svn_fs_base__get_locks(svn_fs_t *fs,
const char *path,
svn_fs_get_locks_callback_t get_locks_func,
void *get_locks_baton,
apr_pool_t *pool) {
struct locks_get_args args;
SVN_ERR(svn_fs__check_fs(fs, TRUE));
args.path = svn_fs__canonicalize_abspath(path, pool);
args.get_locks_func = get_locks_func;
args.get_locks_baton = get_locks_baton;
return svn_fs_base__retry_txn(fs, txn_body_get_locks, &args, pool);
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
svn_fs_base__allow_locked_operation(const char *path,
svn_boolean_t recurse,
trail_t *trail,
apr_pool_t *pool) {
if (recurse) {
SVN_ERR(svn_fs_bdb__locks_get(trail->fs, path, get_locks_callback,
trail->fs, trail, pool));
} else {
svn_lock_t *lock;
SVN_ERR(svn_fs_base__get_lock_helper(&lock, path, trail, pool));
if (lock)
SVN_ERR(verify_lock(trail->fs, lock, pool));
}
return SVN_NO_ERROR;
}