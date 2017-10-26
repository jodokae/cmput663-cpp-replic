#if !defined(SVN_LIBSVN_FS_LOCKS_TABLE_H)
#define SVN_LIBSVN_FS_LOCKS_TABLE_H
#include "svn_fs.h"
#include "svn_error.h"
#include "../trail.h"
#include "../fs.h"
#if defined(__cplusplus)
extern "C" {
#endif
int svn_fs_bdb__open_locks_table(DB **locks_p,
DB_ENV *env,
svn_boolean_t create);
svn_error_t *svn_fs_bdb__lock_add(svn_fs_t *fs,
const char *lock_token,
svn_lock_t *lock,
trail_t *trail,
apr_pool_t *pool);
svn_error_t *svn_fs_bdb__lock_delete(svn_fs_t *fs,
const char *lock_token,
trail_t *trail,
apr_pool_t *pool);
svn_error_t *svn_fs_bdb__lock_get(svn_lock_t **lock_p,
svn_fs_t *fs,
const char *lock_token,
trail_t *trail,
apr_pool_t *pool);
svn_error_t *svn_fs_bdb__locks_get(svn_fs_t *fs,
const char *path,
svn_fs_get_locks_callback_t get_locks_func,
void *get_locks_baton,
trail_t *trail,
apr_pool_t *pool);
#if defined(__cplusplus)
}
#endif
#endif
