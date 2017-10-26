#if !defined(SVN_LIBSVN_FS_REV_TABLE_H)
#define SVN_LIBSVN_FS_REV_TABLE_H
#define APU_WANT_DB
#include <apu_want.h>
#include "svn_fs.h"
#include "../fs.h"
#include "../trail.h"
#if defined(__cplusplus)
extern "C" {
#endif
int svn_fs_bdb__open_revisions_table(DB **revisions_p,
DB_ENV *env,
svn_boolean_t create);
svn_error_t *svn_fs_bdb__get_rev(revision_t **revision_p,
svn_fs_t *fs,
svn_revnum_t rev,
trail_t *trail,
apr_pool_t *pool);
svn_error_t *svn_fs_bdb__put_rev(svn_revnum_t *rev,
svn_fs_t *fs,
const revision_t *revision,
trail_t *trail,
apr_pool_t *pool);
svn_error_t *svn_fs_bdb__youngest_rev(svn_revnum_t *youngest_p,
svn_fs_t *fs,
trail_t *trail,
apr_pool_t *pool);
#if defined(__cplusplus)
}
#endif
#endif
