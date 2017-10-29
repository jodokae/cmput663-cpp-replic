#if !defined(SVN_LIBSVN_FS_TREE_H)
#define SVN_LIBSVN_FS_TREE_H
#if defined(__cplusplus)
extern "C" {
#endif
#include "svn_props.h"
svn_error_t *svn_fs_base__revision_root(svn_fs_root_t **root_p, svn_fs_t *fs,
svn_revnum_t rev, apr_pool_t *pool);
svn_error_t *svn_fs_base__deltify(svn_fs_t *fs, svn_revnum_t rev,
apr_pool_t *pool);
svn_error_t *svn_fs_base__commit_txn(const char **conflict_p,
svn_revnum_t *new_rev, svn_fs_txn_t *txn,
apr_pool_t *pool);
svn_error_t *svn_fs_base__txn_root(svn_fs_root_t **root_p, svn_fs_txn_t *txn,
apr_pool_t *pool);
svn_error_t *svn_fs_base__get_path_kind(svn_node_kind_t *kind,
const char *path,
trail_t *trail,
apr_pool_t *pool);
svn_error_t *svn_fs_base__get_path_created_rev(svn_revnum_t *rev,
const char *path,
trail_t *trail,
apr_pool_t *pool);
#if defined(__cplusplus)
}
#endif
#endif