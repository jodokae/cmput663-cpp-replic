#include <stdlib.h>
#include <stdarg.h>
#include "svn_private_config.h"
#include "svn_fs.h"
#include "err.h"
#include "id.h"
#include "../libsvn_fs/fs-loader.h"
svn_error_t *
svn_fs_fs__err_dangling_id(svn_fs_t *fs, const svn_fs_id_t *id) {
svn_string_t *id_str = svn_fs_fs__id_unparse(id, fs->pool);
return svn_error_createf
(SVN_ERR_FS_ID_NOT_FOUND, 0,
_("Reference to non-existent node '%s' in filesystem '%s'"),
id_str->data, fs->path);
}
svn_error_t *
svn_fs_fs__err_corrupt_lockfile(svn_fs_t *fs, const char *path) {
return
svn_error_createf
(SVN_ERR_FS_CORRUPT, 0,
_("Corrupt lockfile for path '%s' in filesystem '%s'"),
path, fs->path);
}