#if !defined(SVN_LIBSVN_FS_ERR_H)
#define SVN_LIBSVN_FS_ERR_H
#include <apr_pools.h>
#include "svn_error.h"
#include "svn_fs.h"
#if defined(__cplusplus)
extern "C" {
#endif
svn_error_t *svn_fs_fs__err_dangling_id(svn_fs_t *fs,
const svn_fs_id_t *id);
svn_error_t *svn_fs_fs__err_corrupt_lockfile(svn_fs_t *fs,
const char *path);
#if defined(__cplusplus)
}
#endif
#endif