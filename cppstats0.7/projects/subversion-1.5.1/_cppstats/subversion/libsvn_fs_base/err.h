#if !defined(SVN_LIBSVN_FS_ERR_H)
#define SVN_LIBSVN_FS_ERR_H
#include <apr_pools.h>
#include "svn_error.h"
#include "svn_fs.h"
#if defined(__cplusplus)
extern "C" {
#endif
svn_error_t *svn_fs_base__err_corrupt_fs_revision(svn_fs_t *fs,
svn_revnum_t rev);
svn_error_t *svn_fs_base__err_dangling_id(svn_fs_t *fs,
const svn_fs_id_t *id);
svn_error_t *svn_fs_base__err_dangling_rev(svn_fs_t *fs, svn_revnum_t rev);
svn_error_t *svn_fs_base__err_corrupt_txn(svn_fs_t *fs, const char *txn);
svn_error_t *svn_fs_base__err_corrupt_copy(svn_fs_t *fs, const char *copy_id);
svn_error_t *svn_fs_base__err_no_such_txn(svn_fs_t *fs, const char *txn);
svn_error_t *svn_fs_base__err_txn_not_mutable(svn_fs_t *fs, const char *txn);
svn_error_t *svn_fs_base__err_no_such_copy(svn_fs_t *fs, const char *copy_id);
svn_error_t *svn_fs_base__err_bad_lock_token(svn_fs_t *fs,
const char *lock_token);
svn_error_t *svn_fs_base__err_no_lock_token(svn_fs_t *fs, const char *path);
svn_error_t *svn_fs_base__err_corrupt_lock(svn_fs_t *fs,
const char *lock_token);
svn_error_t *svn_fs_base__err_no_such_node_origin(svn_fs_t *fs,
const char *node_id);
#if defined(__cplusplus)
}
#endif
#endif
