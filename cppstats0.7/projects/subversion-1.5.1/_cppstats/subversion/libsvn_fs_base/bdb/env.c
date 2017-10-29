#include <assert.h>
#include <apr.h>
#if APR_HAS_THREADS
#include <apr_thread_mutex.h>
#include <apr_thread_proc.h>
#include <apr_time.h>
#endif
#include <apr_strings.h>
#include <apr_hash.h>
#include "svn_path.h"
#include "svn_pools.h"
#include "svn_utf.h"
#include "private/svn_atomic.h"
#include "bdb-err.h"
#include "bdb_compat.h"
#include "env.h"
typedef struct {
apr_dev_t device;
apr_ino_t inode;
} bdb_env_key_t;
struct bdb_env_t {
char errpfx_string[sizeof(BDB_ERRPFX_STRING)];
#if APR_HAS_THREADS
apr_threadkey_t *error_info;
#else
bdb_error_info_t error_info;
#endif
DB_ENV *env;
u_int32_t flags;
const char *path;
const char *path_bdb;
unsigned refcount;
volatile svn_atomic_t panic;
bdb_env_key_t key;
apr_file_t *dbconfig_file;
apr_pool_t *pool;
};
#if APR_HAS_THREADS
static bdb_error_info_t *
get_error_info(bdb_env_t *bdb) {
void *priv;
apr_threadkey_private_get(&priv, bdb->error_info);
if (!priv) {
priv = calloc(1, sizeof(bdb_error_info_t));
apr_threadkey_private_set(priv, bdb->error_info);
}
return priv;
}
#else
#define get_error_info(bdb) (&(bdb)->error_info)
#endif
static svn_error_t *
convert_bdb_error(bdb_env_t *bdb, int db_err) {
if (db_err) {
bdb_env_baton_t bdb_baton;
bdb_baton.env = bdb->env;
bdb_baton.bdb = bdb;
bdb_baton.error_info = get_error_info(bdb);
SVN_BDB_ERR(&bdb_baton, db_err);
}
return SVN_NO_ERROR;
}
static void
bdb_error_gatherer(const DB_ENV *dbenv, const char *baton, const char *msg) {
bdb_error_info_t *error_info = get_error_info((bdb_env_t *) baton);
svn_error_t *new_err;
SVN_BDB_ERROR_GATHERER_IGNORE(dbenv);
new_err = svn_error_createf(SVN_NO_ERROR, NULL, "bdb: %s", msg);
if (error_info->pending_errors)
svn_error_compose(error_info->pending_errors, new_err);
else
error_info->pending_errors = new_err;
if (error_info->user_callback)
error_info->user_callback(NULL, (char *)msg);
}
static apr_status_t
cleanup_env(void *data) {
bdb_env_t *bdb = data;
bdb->pool = NULL;
bdb->dbconfig_file = NULL;
#if APR_HAS_THREADS
apr_threadkey_private_delete(bdb->error_info);
#endif
if (!bdb->refcount)
free(data);
return APR_SUCCESS;
}
#if APR_HAS_THREADS
static void
cleanup_error_info(void *baton) {
bdb_error_info_t *error_info = baton;
if (error_info)
svn_error_clear(error_info->pending_errors);
free(error_info);
}
#endif
static svn_error_t *
create_env(bdb_env_t **bdbp, const char *path, apr_pool_t *pool) {
int db_err;
bdb_env_t *bdb;
const char *path_bdb;
char *tmp_path, *tmp_path_bdb;
apr_size_t path_size, path_bdb_size;
#if SVN_BDB_PATH_UTF8
path_bdb = svn_path_local_style(path, pool);
#else
SVN_ERR(svn_utf_cstring_from_utf8(&path_bdb,
svn_path_local_style(path, pool),
pool));
#endif
path_size = strlen(path) + 1;
path_bdb_size = strlen(path_bdb) + 1;
bdb = calloc(1, sizeof(*bdb) + path_size + path_bdb_size);
apr_pool_cleanup_register(pool, bdb, cleanup_env, apr_pool_cleanup_null);
apr_cpystrn(bdb->errpfx_string, BDB_ERRPFX_STRING,
sizeof(bdb->errpfx_string));
bdb->path = tmp_path = (char*)(bdb + 1);
bdb->path_bdb = tmp_path_bdb = tmp_path + path_size;
apr_cpystrn(tmp_path, path, path_size);
apr_cpystrn(tmp_path_bdb, path_bdb, path_bdb_size);
bdb->pool = pool;
*bdbp = bdb;
#if APR_HAS_THREADS
{
apr_status_t apr_err = apr_threadkey_private_create(&bdb->error_info,
cleanup_error_info,
pool);
if (apr_err)
return svn_error_create(apr_err, NULL,
"Can't allocate thread-specific storage"
" for the Berkeley DB environment descriptor");
}
#endif
db_err = db_env_create(&(bdb->env), 0);
if (!db_err) {
bdb->env->set_errpfx(bdb->env, (char *) bdb);
bdb->env->set_errcall(bdb->env, (bdb_error_gatherer));
db_err = bdb->env->set_alloc(bdb->env, malloc, realloc, free);
if (!db_err)
db_err = bdb->env->set_lk_detect(bdb->env, DB_LOCK_RANDOM);
}
return convert_bdb_error(bdb, db_err);
}
static apr_pool_t *bdb_cache_pool = NULL;
static apr_hash_t *bdb_cache = NULL;
#if APR_HAS_THREADS
static apr_thread_mutex_t *bdb_cache_lock = NULL;
static apr_status_t
clear_cache(void *data) {
bdb_cache = NULL;
bdb_cache_lock = NULL;
return APR_SUCCESS;
}
#endif
static volatile svn_atomic_t bdb_cache_state;
static svn_error_t *
bdb_init_cb(apr_pool_t *pool) {
#if APR_HAS_THREADS
apr_status_t apr_err;
#endif
bdb_cache_pool = svn_pool_create(pool);
bdb_cache = apr_hash_make(bdb_cache_pool);
#if APR_HAS_THREADS
apr_err = apr_thread_mutex_create(&bdb_cache_lock,
APR_THREAD_MUTEX_DEFAULT,
bdb_cache_pool);
if (apr_err) {
return svn_error_create(apr_err, NULL,
"Couldn't initialize the cache of"
" Berkeley DB environment descriptors");
}
apr_pool_cleanup_register(bdb_cache_pool, NULL, clear_cache,
apr_pool_cleanup_null);
#endif
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_bdb__init(apr_pool_t* pool) {
SVN_ERR(svn_atomic__init_once(&bdb_cache_state, bdb_init_cb, pool));
return SVN_NO_ERROR;
}
static APR_INLINE void
acquire_cache_mutex(void) {
#if APR_HAS_THREADS
if (bdb_cache_lock)
apr_thread_mutex_lock(bdb_cache_lock);
#endif
}
static APR_INLINE void
release_cache_mutex(void) {
#if APR_HAS_THREADS
if (bdb_cache_lock)
apr_thread_mutex_unlock(bdb_cache_lock);
#endif
}
static svn_error_t *
bdb_cache_key(bdb_env_key_t *keyp, apr_file_t **dbconfig_file,
const char *path, apr_pool_t *pool) {
const char *dbcfg_file_name = svn_path_join(path, BDB_CONFIG_FILE, pool);
apr_file_t *dbcfg_file;
apr_status_t apr_err;
apr_finfo_t finfo;
SVN_ERR(svn_io_file_open(&dbcfg_file, dbcfg_file_name,
APR_READ, APR_OS_DEFAULT, pool));
apr_err = apr_file_info_get(&finfo, APR_FINFO_DEV | APR_FINFO_INODE,
dbcfg_file);
if (apr_err)
return svn_error_wrap_apr
(apr_err, "Can't create BDB environment cache key");
memset(keyp, 0, sizeof *keyp);
keyp->device = finfo.device;
keyp->inode = finfo.inode;
if (dbconfig_file)
*dbconfig_file = dbcfg_file;
else
apr_file_close(dbcfg_file);
return SVN_NO_ERROR;
}
static bdb_env_t *
bdb_cache_get(const bdb_env_key_t *keyp, svn_boolean_t *panicp) {
bdb_env_t *bdb = apr_hash_get(bdb_cache, keyp, sizeof *keyp);
if (bdb && bdb->env) {
*panicp = !!svn_atomic_read(&bdb->panic);
#if SVN_BDB_VERSION_AT_LEAST(4,2)
if (!*panicp) {
u_int32_t flags;
if (bdb->env->get_flags(bdb->env, &flags)
|| (flags & DB_PANIC_ENVIRONMENT)) {
svn_atomic_set(&bdb->panic, TRUE);
*panicp = TRUE;
bdb = NULL;
}
}
#endif
} else {
*panicp = FALSE;
}
return bdb;
}
static svn_error_t *
bdb_close(bdb_env_t *bdb) {
svn_error_t *err = SVN_NO_ERROR;
int db_err = bdb->env->close(bdb->env, 0);
if (db_err && (!SVN_BDB_AUTO_RECOVER || db_err != DB_RUNRECOVERY))
err = convert_bdb_error(bdb, db_err);
if (bdb->pool)
svn_pool_destroy(bdb->pool);
else
free(bdb);
return err;
}
svn_error_t *
svn_fs_bdb__close(bdb_env_baton_t *bdb_baton) {
svn_error_t *err = SVN_NO_ERROR;
bdb_env_t *bdb = bdb_baton->bdb;
assert(bdb_baton->env == bdb_baton->bdb->env);
bdb_baton->bdb = NULL;
if (0 == --bdb_baton->error_info->refcount && bdb->pool) {
svn_error_clear(bdb_baton->error_info->pending_errors);
#if APR_HAS_THREADS
free(bdb_baton->error_info);
apr_threadkey_private_set(NULL, bdb->error_info);
#endif
}
acquire_cache_mutex();
if (--bdb->refcount != 0) {
release_cache_mutex();
if (!SVN_BDB_AUTO_RECOVER && svn_atomic_read(&bdb->panic))
err = svn_error_create(SVN_ERR_FS_BERKELEY_DB, NULL,
db_strerror(DB_RUNRECOVERY));
} else {
if (bdb_cache)
apr_hash_set(bdb_cache, &bdb->key, sizeof bdb->key, NULL);
err = bdb_close(bdb);
release_cache_mutex();
}
return err;
}
static svn_error_t *
bdb_open(bdb_env_t *bdb, u_int32_t flags, int mode) {
#if APR_HAS_THREADS
flags |= DB_THREAD;
#endif
SVN_ERR(convert_bdb_error
(bdb, (bdb->env->open)(bdb->env, bdb->path_bdb, flags, mode)));
#if SVN_BDB_AUTO_COMMIT
SVN_ERR(convert_bdb_error
(bdb, bdb->env->set_flags(bdb->env, SVN_BDB_AUTO_COMMIT, 1)));
#endif
SVN_ERR(bdb_cache_key(&bdb->key, &bdb->dbconfig_file,
bdb->path, bdb->pool));
return SVN_NO_ERROR;
}
static apr_status_t
cleanup_env_baton(void *data) {
bdb_env_baton_t *bdb_baton = data;
if (bdb_baton->bdb)
svn_error_clear(svn_fs_bdb__close(bdb_baton));
return APR_SUCCESS;
}
svn_error_t *
svn_fs_bdb__open(bdb_env_baton_t **bdb_batonp, const char *path,
u_int32_t flags, int mode,
apr_pool_t *pool) {
svn_error_t *err = SVN_NO_ERROR;
bdb_env_key_t key;
bdb_env_t *bdb;
svn_boolean_t panic;
acquire_cache_mutex();
err = bdb_cache_key(&key, NULL, path, pool);
if (err) {
release_cache_mutex();
return err;
}
bdb = bdb_cache_get(&key, &panic);
if (panic) {
release_cache_mutex();
return svn_error_create(SVN_ERR_FS_BERKELEY_DB, NULL,
db_strerror(DB_RUNRECOVERY));
}
if (bdb && bdb->flags != flags) {
release_cache_mutex();
if ((flags ^ bdb->flags) & DB_PRIVATE) {
if (flags & DB_PRIVATE)
return svn_error_create(SVN_ERR_FS_BERKELEY_DB, NULL,
"Reopening a public Berkeley DB"
" environment with private attributes");
else
return svn_error_create(SVN_ERR_FS_BERKELEY_DB, NULL,
"Reopening a private Berkeley DB"
" environment with public attributes");
}
return svn_error_create(SVN_ERR_FS_BERKELEY_DB, NULL,
"Reopening a Berkeley DB environment"
" with different attributes");
}
if (!bdb) {
err = create_env(&bdb, path, svn_pool_create(bdb_cache_pool));
if (!err) {
err = bdb_open(bdb, flags, mode);
if (!err) {
apr_hash_set(bdb_cache, &bdb->key, sizeof bdb->key, bdb);
bdb->flags = flags;
bdb->refcount = 1;
} else {
svn_error_clear(bdb_close(bdb));
}
}
} else {
++bdb->refcount;
}
if (!err) {
*bdb_batonp = apr_palloc(pool, sizeof **bdb_batonp);
(*bdb_batonp)->env = bdb->env;
(*bdb_batonp)->bdb = bdb;
(*bdb_batonp)->error_info = get_error_info(bdb);
++(*bdb_batonp)->error_info->refcount;
apr_pool_cleanup_register(pool, *bdb_batonp, cleanup_env_baton,
apr_pool_cleanup_null);
}
release_cache_mutex();
return err;
}
svn_boolean_t
svn_fs_bdb__get_panic(bdb_env_baton_t *bdb_baton) {
if (!bdb_baton->bdb)
return TRUE;
assert(bdb_baton->env == bdb_baton->bdb->env);
return !!svn_atomic_read(&bdb_baton->bdb->panic);
}
void
svn_fs_bdb__set_panic(bdb_env_baton_t *bdb_baton) {
if (!bdb_baton->bdb)
return;
assert(bdb_baton->env == bdb_baton->bdb->env);
svn_atomic_set(&bdb_baton->bdb->panic, TRUE);
}
svn_error_t *
svn_fs_bdb__remove(const char *path, apr_pool_t *pool) {
bdb_env_t *bdb;
SVN_ERR(create_env(&bdb, path, pool));
return convert_bdb_error
(bdb, bdb->env->remove(bdb->env, bdb->path_bdb, DB_FORCE));
}