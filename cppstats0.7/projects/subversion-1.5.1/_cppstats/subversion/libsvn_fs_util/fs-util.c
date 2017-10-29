#include <string.h>
#include <apr_pools.h>
#include <apr_strings.h>
#include "svn_fs.h"
#include "svn_path.h"
#include "svn_private_config.h"
#include "private/svn_fs_util.h"
#include "../libsvn_fs/fs-loader.h"
const char *
svn_fs__canonicalize_abspath(const char *path, apr_pool_t *pool) {
char *newpath;
int path_len;
int path_i = 0, newpath_i = 0;
svn_boolean_t eating_slashes = FALSE;
if (! path)
return NULL;
if (! *path)
return apr_pstrdup(pool, "/");
path_len = strlen(path);
newpath = apr_pcalloc(pool, path_len + 2);
if (*path != '/') {
newpath[newpath_i++] = '/';
}
for (path_i = 0; path_i < path_len; path_i++) {
if (path[path_i] == '/') {
if (eating_slashes)
continue;
eating_slashes = TRUE;
} else {
if (eating_slashes)
eating_slashes = FALSE;
}
newpath[newpath_i++] = path[path_i];
}
if ((newpath[newpath_i - 1] == '/') && (newpath_i > 1))
newpath[newpath_i - 1] = '\0';
return newpath;
}
svn_error_t *
svn_fs__check_fs(svn_fs_t *fs,
svn_boolean_t expect_open) {
if ((expect_open && fs->fsap_data)
|| ((! expect_open) && (! fs->fsap_data)))
return SVN_NO_ERROR;
if (expect_open)
return svn_error_create(SVN_ERR_FS_NOT_OPEN, 0,
_("Filesystem object has not been opened yet"));
else
return svn_error_create(SVN_ERR_FS_ALREADY_OPEN, 0,
_("Filesystem object already open"));
}
char *
svn_fs__next_entry_name(const char **next_p,
const char *path,
apr_pool_t *pool) {
const char *end;
end = strchr(path, '/');
if (! end) {
*next_p = 0;
return apr_pstrdup(pool, path);
} else {
const char *next = end;
while (*next == '/')
next++;
*next_p = next;
return apr_pstrndup(pool, path, end - path);
}
}