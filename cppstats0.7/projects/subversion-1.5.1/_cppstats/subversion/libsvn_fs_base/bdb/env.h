#if !defined(SVN_LIBSVN_FS_BDB_ENV_H)
#define SVN_LIBSVN_FS_BDB_ENV_H
#define APU_WANT_DB
#include <apu_want.h>
#include <apr_pools.h>
#include <apr_file_io.h>
#include "bdb_compat.h"
#if defined(__cplusplus)
extern "C" {
#endif
#define BDB_CONFIG_FILE "DB_CONFIG"
#define BDB_ERRPFX_STRING "svn (bdb): "
typedef struct bdb_env_t bdb_env_t;
typedef struct {
svn_error_t *pending_errors;
void (*user_callback)(const char *errpfx, char *msg);
unsigned refcount;
} bdb_error_info_t;
typedef struct {
DB_ENV *env;
bdb_env_t *bdb;
bdb_error_info_t *error_info;
} bdb_env_baton_t;
#define SVN_BDB_STANDARD_ENV_FLAGS (DB_CREATE | DB_INIT_LOCK | DB_INIT_LOG | DB_INIT_MPOOL | DB_INIT_TXN | SVN_BDB_AUTO_RECOVER)
#define SVN_BDB_PRIVATE_ENV_FLAGS (DB_CREATE | DB_INIT_LOG | DB_INIT_MPOOL | DB_INIT_TXN | DB_PRIVATE)
svn_error_t *svn_fs_bdb__init(apr_pool_t* pool);
svn_error_t *svn_fs_bdb__open(bdb_env_baton_t **bdb_batonp,
const char *path,
u_int32_t flags, int mode,
apr_pool_t *pool);
svn_error_t *svn_fs_bdb__close(bdb_env_baton_t *bdb_baton);
svn_boolean_t svn_fs_bdb__get_panic(bdb_env_baton_t *bdb_baton);
void svn_fs_bdb__set_panic(bdb_env_baton_t *bdb_baton);
svn_error_t *svn_fs_bdb__remove(const char *path, apr_pool_t *pool);
#if defined(__cplusplus)
}
#endif
#endif