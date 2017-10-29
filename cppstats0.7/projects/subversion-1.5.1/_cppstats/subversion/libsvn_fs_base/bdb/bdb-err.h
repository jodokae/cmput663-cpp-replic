#if !defined(SVN_LIBSVN_FS_BDB_ERR_H)
#define SVN_LIBSVN_FS_BDB_ERR_H
#include <apr_pools.h>
#include "svn_error.h"
#include "svn_fs.h"
#include "env.h"
#if defined(__cplusplus)
extern "C" {
#endif
svn_error_t *svn_fs_bdb__dberr(bdb_env_baton_t *bdb_baton, int db_err);
svn_error_t *svn_fs_bdb__dberrf(bdb_env_baton_t *bdb_baton, int db_err,
const char *fmt, ...)
__attribute__((format(printf, 3, 4)));
void svn_fs_bdb__clear_err(bdb_env_t *bdb);
svn_error_t *svn_fs_bdb__wrap_db(svn_fs_t *fs,
const char *operation,
int db_err);
#define BDB_WRAP(fs, op, err) (svn_fs_bdb__wrap_db((fs), (op), (err)))
#define SVN_BDB_ERR(bdb, expr) do { int db_err__temp = (expr); if (db_err__temp) return svn_fs_bdb__dberr((bdb), db_err__temp); svn_error_clear((bdb)->error_info->pending_errors); (bdb)->error_info->pending_errors = NULL; } while (0)
#define BDB_ERR(expr) do { int db_err__temp = (expr); if (db_err__temp) return db_err__temp; } while (0)
svn_error_t *svn_fs_bdb__check_fs(svn_fs_t *fs);
#if defined(__cplusplus)
}
#endif
#endif