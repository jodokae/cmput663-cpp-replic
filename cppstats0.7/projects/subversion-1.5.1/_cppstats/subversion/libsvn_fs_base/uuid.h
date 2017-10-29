#if !defined(SVN_LIBSVN_FS_UUID_H)
#define SVN_LIBSVN_FS_UUID_H
#if defined(__cplusplus)
extern "C" {
#endif
svn_error_t *svn_fs_base__get_uuid(svn_fs_t *fs, const char **uuid,
apr_pool_t *pool);
svn_error_t *svn_fs_base__set_uuid(svn_fs_t *fs, const char *uuid,
apr_pool_t *pool);
#if defined(__cplusplus)
}
#endif
#endif
