#if !defined(SVN_FS_UTIL_H)
#define SVN_FS_UTIL_H
#include "svn_private_config.h"
#if defined(__cplusplus)
extern "C" {
#endif
const char *
svn_fs__canonicalize_abspath(const char *path, apr_pool_t *pool);
svn_error_t *svn_fs__check_fs(svn_fs_t *fs, svn_boolean_t expect_open);
#define SVN_FS__NOT_FOUND(root, path) ( root->is_txn_root ? svn_error_createf (SVN_ERR_FS_NOT_FOUND, 0, _("File not found: transaction '%s', path '%s'"), root->txn, path) : svn_error_createf (SVN_ERR_FS_NOT_FOUND, 0, _("File not found: revision %ld, path '%s'"), root->rev, path) )
#define SVN_FS__ALREADY_EXISTS(root, path_str) ( root->is_txn_root ? svn_error_createf (SVN_ERR_FS_ALREADY_EXISTS, 0, _("File already exists: filesystem '%s', transaction '%s', path '%s'"), root->fs->path, root->txn, path_str) : svn_error_createf (SVN_ERR_FS_ALREADY_EXISTS, 0, _("File already exists: filesystem '%s', revision %ld, path '%s'"), root->fs->path, root->rev, path_str) )
#define SVN_FS__NOT_TXN(root) svn_error_create (SVN_ERR_FS_NOT_TXN_ROOT, NULL, _("Root object must be a transaction root"))
#define SVN_FS__ERR_NOT_MUTABLE(fs, rev, path_in_repo) svn_error_createf (SVN_ERR_FS_NOT_MUTABLE, 0, _("File is not mutable: filesystem '%s', revision %ld, path '%s'"), fs->path, rev, path_in_repo)
#define SVN_FS__ERR_NOT_DIRECTORY(fs, path_in_repo) svn_error_createf (SVN_ERR_FS_NOT_DIRECTORY, 0, _("'%s' is not a directory in filesystem '%s'"), path_in_repo, fs->path)
#define SVN_FS__ERR_NOT_FILE(fs, path_in_repo) svn_error_createf (SVN_ERR_FS_NOT_FILE, 0, _("'%s' is not a file in filesystem '%s'"), path_in_repo, fs->path)
#define SVN_FS__ERR_PATH_ALREADY_LOCKED(fs, lock) svn_error_createf (SVN_ERR_FS_PATH_ALREADY_LOCKED, 0, _("Path '%s' is already locked by user '%s' in filesystem '%s'"), lock->path, lock->owner, fs->path)
#define SVN_FS__ERR_NO_SUCH_LOCK(fs, path_in_repo) svn_error_createf (SVN_ERR_FS_NO_SUCH_LOCK, 0, _("No lock on path '%s' in filesystem '%s'"), path_in_repo, fs->path)
#define SVN_FS__ERR_LOCK_EXPIRED(fs, token) svn_error_createf (SVN_ERR_FS_LOCK_EXPIRED, 0, _("Lock has expired: lock-token '%s' in filesystem '%s'"), token, fs->path)
#define SVN_FS__ERR_NO_USER(fs) svn_error_createf (SVN_ERR_FS_NO_USER, 0, _("No username is currently associated with filesystem '%s'"), fs->path)
#define SVN_FS__ERR_LOCK_OWNER_MISMATCH(fs, username, lock_owner) svn_error_createf (SVN_ERR_FS_LOCK_OWNER_MISMATCH, 0, _("User '%s' is trying to use a lock owned by '%s' in " "filesystem '%s'"), username, lock_owner, fs->path)
char *
svn_fs__next_entry_name(const char **next_p,
const char *path,
apr_pool_t *pool);
#if defined(__cplusplus)
}
#endif
#endif
