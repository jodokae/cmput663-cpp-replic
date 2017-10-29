#if !defined(SVN_LIBSVN_FS_TREE_H)
#define SVN_LIBSVN_FS_TREE_H
#if defined(__cplusplus)
extern "C" {
#endif
svn_error_t *svn_fs_fs__revision_root(svn_fs_root_t **root_p, svn_fs_t *fs,
svn_revnum_t rev, apr_pool_t *pool);
svn_error_t *svn_fs_fs__deltify(svn_fs_t *fs, svn_revnum_t rev,
apr_pool_t *pool);
svn_error_t *svn_fs_fs__commit_txn(const char **conflict_p,
svn_revnum_t *new_rev, svn_fs_txn_t *txn,
apr_pool_t *pool);
svn_error_t *svn_fs_fs__txn_root(svn_fs_root_t **root_p, svn_fs_txn_t *txn,
apr_pool_t *pool);
svn_error_t *
svn_fs_fs__check_path(svn_node_kind_t *kind_p,
svn_fs_root_t *root,
const char *path,
apr_pool_t *pool);
svn_error_t *
svn_fs_fs__node_created_rev(svn_revnum_t *revision,
svn_fs_root_t *root,
const char *path,
apr_pool_t *pool);
#if defined(__cplusplus)
}
#endif
#endif