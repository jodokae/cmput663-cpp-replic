#if !defined(SVN_LIBSVN_FS_UUIDS_TABLE_H)
#define SVN_LIBSVN_FS_UUIDS_TABLE_H
#define APU_WANT_DB
#include <apu_want.h>
#include "svn_io.h"
#include "svn_fs.h"
#if defined(__cplusplus)
extern "C" {
#endif
int svn_fs_bdb__open_uuids_table(DB **uuids_p,
DB_ENV *env,
svn_boolean_t create);
svn_error_t *svn_fs_bdb__get_uuid(svn_fs_t *fs,
int idx,
const char **uuid,
trail_t *trail,
apr_pool_t *pool);
svn_error_t *svn_fs_bdb__set_uuid(svn_fs_t *fs,
int idx,
const char *uuid,
trail_t *trail,
apr_pool_t *pool);
#if defined(__cplusplus)
}
#endif
#endif