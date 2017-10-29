#if !defined(SVN_LIBSVN_FS_TRAIL_H)
#define SVN_LIBSVN_FS_TRAIL_H
#define APU_WANT_DB
#include <apu_want.h>
#include <apr_pools.h>
#include "svn_fs.h"
#include "fs.h"
#if defined(__cplusplus)
extern "C" {
#endif
struct trail_t {
DB_TXN *db_txn;
svn_fs_t *fs;
apr_pool_t *pool;
#if defined(SVN_FS__TRAIL_DEBUG)
struct trail_debug_t *trail_debug;
#endif
};
typedef struct trail_t trail_t;
svn_error_t *svn_fs_base__retry_txn(svn_fs_t *fs,
svn_error_t *(*txn_body)(void *baton,
trail_t *trail),
void *baton,
apr_pool_t *pool);
svn_error_t *
svn_fs_base__retry_debug(svn_fs_t *fs,
svn_error_t *(*txn_body)(void *baton,
trail_t *trail),
void *baton,
apr_pool_t *pool,
const char *txn_body_fn_name,
const char *filename,
int line);
#if defined(SVN_FS__TRAIL_DEBUG)
#define svn_fs_base__retry_txn(fs, txn_body, baton, pool) svn_fs_base__retry_debug(fs, txn_body, baton, pool, #txn_body,
__FILE__, __LINE__)
#endif
svn_error_t *svn_fs_base__retry(svn_fs_t *fs,
svn_error_t *(*txn_body)(void *baton,
trail_t *trail),
void *baton,
apr_pool_t *pool);
#if defined(SVN_FS__TRAIL_DEBUG)
void svn_fs_base__trail_debug(trail_t *trail, const char *table,
const char *op);
#else
#define svn_fs_base__trail_debug(trail, table, operation)
#endif
#if defined(__cplusplus)
}
#endif
#endif