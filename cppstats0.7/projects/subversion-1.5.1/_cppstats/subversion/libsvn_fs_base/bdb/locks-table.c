#include <string.h>
#include <assert.h>
#include "bdb_compat.h"
#include "svn_pools.h"
#include "dbt.h"
#include "../err.h"
#include "../fs.h"
#include "../util/skel.h"
#include "../util/fs_skels.h"
#include "../trail.h"
#include "../../libsvn_fs/fs-loader.h"
#include "bdb-err.h"
#include "locks-table.h"
#include "lock-tokens-table.h"
#include "private/svn_fs_util.h"
int
svn_fs_bdb__open_locks_table(DB **locks_p,
DB_ENV *env,
svn_boolean_t create) {
const u_int32_t open_flags = (create ? (DB_CREATE | DB_EXCL) : 0);
DB *locks;
int error;
BDB_ERR(svn_fs_bdb__check_version());
BDB_ERR(db_create(&locks, env, 0));
error = (locks->open)(SVN_BDB_OPEN_PARAMS(locks, NULL),
"locks", 0, DB_BTREE,
open_flags, 0666);
if (error == ENOENT && (! create)) {
BDB_ERR(locks->close(locks, 0));
return svn_fs_bdb__open_locks_table(locks_p, env, TRUE);
}
BDB_ERR(error);
*locks_p = locks;
return 0;
}
svn_error_t *
svn_fs_bdb__lock_add(svn_fs_t *fs,
const char *lock_token,
svn_lock_t *lock,
trail_t *trail,
apr_pool_t *pool) {
base_fs_data_t *bfd = fs->fsap_data;
skel_t *lock_skel;
DBT key, value;
SVN_ERR(svn_fs_base__unparse_lock_skel(&lock_skel, lock, pool));
svn_fs_base__str_to_dbt(&key, lock_token);
svn_fs_base__skel_to_dbt(&value, lock_skel, pool);
svn_fs_base__trail_debug(trail, "lock", "add");
SVN_ERR(BDB_WRAP(fs, "storing lock record",
bfd->locks->put(bfd->locks, trail->db_txn,
&key, &value, 0)));
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_bdb__lock_delete(svn_fs_t *fs,
const char *lock_token,
trail_t *trail,
apr_pool_t *pool) {
base_fs_data_t *bfd = fs->fsap_data;
DBT key;
int db_err;
svn_fs_base__str_to_dbt(&key, lock_token);
svn_fs_base__trail_debug(trail, "locks", "del");
db_err = bfd->locks->del(bfd->locks, trail->db_txn, &key, 0);
if (db_err == DB_NOTFOUND)
return svn_fs_base__err_bad_lock_token(fs, lock_token);
SVN_ERR(BDB_WRAP(fs, "deleting lock from 'locks' table", db_err));
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_bdb__lock_get(svn_lock_t **lock_p,
svn_fs_t *fs,
const char *lock_token,
trail_t *trail,
apr_pool_t *pool) {
base_fs_data_t *bfd = fs->fsap_data;
DBT key, value;
int db_err;
skel_t *skel;
svn_lock_t *lock;
svn_fs_base__trail_debug(trail, "lock", "get");
db_err = bfd->locks->get(bfd->locks, trail->db_txn,
svn_fs_base__str_to_dbt(&key, lock_token),
svn_fs_base__result_dbt(&value),
0);
svn_fs_base__track_dbt(&value, pool);
if (db_err == DB_NOTFOUND)
return svn_fs_base__err_bad_lock_token(fs, lock_token);
SVN_ERR(BDB_WRAP(fs, "reading lock", db_err));
skel = svn_fs_base__parse_skel(value.data, value.size, pool);
if (! skel)
return svn_fs_base__err_corrupt_lock(fs, lock_token);
SVN_ERR(svn_fs_base__parse_lock_skel(&lock, skel, pool));
if (lock->expiration_date && (apr_time_now() > lock->expiration_date)) {
SVN_ERR(svn_fs_bdb__lock_delete(fs, lock_token, trail, pool));
return SVN_FS__ERR_LOCK_EXPIRED(fs, lock_token);
}
*lock_p = lock;
return SVN_NO_ERROR;
}
static svn_error_t *
get_lock(svn_lock_t **lock_p,
svn_fs_t *fs,
const char *path,
const char *lock_token,
trail_t *trail,
apr_pool_t *pool) {
svn_error_t *err = SVN_NO_ERROR;
*lock_p = NULL;
err = svn_fs_bdb__lock_get(lock_p, fs, lock_token, trail, pool);
if (err && ((err->apr_err == SVN_ERR_FS_LOCK_EXPIRED)
|| (err->apr_err == SVN_ERR_FS_BAD_LOCK_TOKEN))) {
svn_error_clear(err);
err = svn_fs_bdb__lock_token_delete(fs, path, trail, pool);
}
return err;
}
svn_error_t *
svn_fs_bdb__locks_get(svn_fs_t *fs,
const char *path,
svn_fs_get_locks_callback_t get_locks_func,
void *get_locks_baton,
trail_t *trail,
apr_pool_t *pool) {
base_fs_data_t *bfd = fs->fsap_data;
DBC *cursor;
DBT key, value;
int db_err, db_c_err;
apr_pool_t *subpool = svn_pool_create(pool);
const char *lock_token;
svn_lock_t *lock;
svn_error_t *err;
const char *lookup_path = path;
err = svn_fs_bdb__lock_token_get(&lock_token, fs, path, trail, pool);
if (err && ((err->apr_err == SVN_ERR_FS_LOCK_EXPIRED)
|| (err->apr_err == SVN_ERR_FS_BAD_LOCK_TOKEN)
|| (err->apr_err == SVN_ERR_FS_NO_SUCH_LOCK))) {
svn_error_clear(err);
} else if (err) {
return err;
} else {
SVN_ERR(get_lock(&lock, fs, path, lock_token, trail, pool));
if (lock && get_locks_func)
SVN_ERR(get_locks_func(get_locks_baton, lock, pool));
}
if (strcmp(path, "/") != 0)
lookup_path = apr_pstrcat(pool, path, "/", NULL);
svn_fs_base__trail_debug(trail, "lock-tokens", "cursor");
db_err = bfd->lock_tokens->cursor(bfd->lock_tokens, trail->db_txn,
&cursor, 0);
SVN_ERR(BDB_WRAP(fs, "creating cursor for reading lock tokens", db_err));
svn_fs_base__str_to_dbt(&key, lookup_path);
key.flags |= DB_DBT_MALLOC;
db_err = svn_bdb_dbc_get(cursor, &key, svn_fs_base__result_dbt(&value),
DB_SET_RANGE);
while ((! db_err)
&& strncmp(lookup_path, key.data, strlen(lookup_path)) == 0) {
const char *child_path;
svn_pool_clear(subpool);
svn_fs_base__track_dbt(&key, subpool);
svn_fs_base__track_dbt(&value, subpool);
child_path = apr_pstrmemdup(subpool, key.data, key.size);
lock_token = apr_pstrmemdup(subpool, value.data, value.size);
err = get_lock(&lock, fs, child_path, lock_token, trail, subpool);
if (err) {
svn_bdb_dbc_close(cursor);
return err;
}
if (lock && get_locks_func) {
err = get_locks_func(get_locks_baton, lock, subpool);
if (err) {
svn_bdb_dbc_close(cursor);
return err;
}
}
svn_fs_base__result_dbt(&key);
svn_fs_base__result_dbt(&value);
db_err = svn_bdb_dbc_get(cursor, &key, &value, DB_NEXT);
}
svn_pool_destroy(subpool);
db_c_err = svn_bdb_dbc_close(cursor);
if (db_err && (db_err != DB_NOTFOUND))
SVN_ERR(BDB_WRAP(fs, "fetching lock tokens", db_err));
if (db_c_err)
SVN_ERR(BDB_WRAP(fs, "fetching lock tokens (closing cursor)", db_c_err));
return SVN_NO_ERROR;
}