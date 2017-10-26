#if !defined(SVN_LIBSVN_FS_LOCK_TOKENS_TABLE_H)
#define SVN_LIBSVN_FS_LOCK_TOKENS_TABLE_H
#include "svn_fs.h"
#include "svn_error.h"
#include "../trail.h"
#include "../fs.h"
#if defined(__cplusplus)
extern "C" {
#endif
int svn_fs_bdb__open_lock_tokens_table(DB **locks_tokens_p,
DB_ENV *env,
svn_boolean_t create);
svn_error_t *
svn_fs_bdb__lock_token_add(svn_fs_t *fs,
const char *path,
const char *lock_token,
trail_t *trail,
apr_pool_t *pool);
svn_error_t *
svn_fs_bdb__lock_token_delete(svn_fs_t *fs,
const char *path,
trail_t *trail,
apr_pool_t *pool);
svn_error_t *
svn_fs_bdb__lock_token_get(const char **lock_token_p,
svn_fs_t *fs,
const char *path,
trail_t *trail,
apr_pool_t *pool);
#if defined(__cplusplus)
}
#endif
#endif
