#if !defined(SVN_LIBSVN_FS_LOCK_H)
#define SVN_LIBSVN_FS_LOCK_H
#include "trail.h"
#if defined(__cplusplus)
extern "C" {
#endif
svn_error_t *svn_fs_base__lock(svn_lock_t **lock,
svn_fs_t *fs,
const char *path,
const char *token,
const char *comment,
svn_boolean_t is_dav_comment,
apr_time_t expiration_date,
svn_revnum_t current_rev,
svn_boolean_t steal_lock,
apr_pool_t *pool);
svn_error_t *svn_fs_base__generate_lock_token(const char **token,
svn_fs_t *fs,
apr_pool_t *pool);
svn_error_t *svn_fs_base__unlock(svn_fs_t *fs,
const char *path,
const char *token,
svn_boolean_t break_lock,
apr_pool_t *pool);
svn_error_t *svn_fs_base__get_lock(svn_lock_t **lock,
svn_fs_t *fs,
const char *path,
apr_pool_t *pool);
svn_error_t *
svn_fs_base__get_locks(svn_fs_t *fs,
const char *path,
svn_fs_get_locks_callback_t get_locks_func,
void *get_locks_baton,
apr_pool_t *pool);
svn_error_t *
svn_fs_base__get_lock_helper(svn_lock_t **lock_p,
const char *path,
trail_t *trail,
apr_pool_t *pool);
svn_error_t *svn_fs_base__allow_locked_operation(const char *path,
svn_boolean_t recurse,
trail_t *trail,
apr_pool_t *pool);
#if defined(__cplusplus)
}
#endif
#endif
