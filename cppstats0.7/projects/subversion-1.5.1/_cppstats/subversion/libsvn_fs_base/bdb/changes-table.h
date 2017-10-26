#if !defined(SVN_LIBSVN_FS_CHANGES_TABLE_H)
#define SVN_LIBSVN_FS_CHANGES_TABLE_H
#define APU_WANT_DB
#include <apu_want.h>
#include "svn_io.h"
#include "svn_fs.h"
#include "../fs.h"
#include "../trail.h"
#if defined(__cplusplus)
extern "C" {
#endif
int svn_fs_bdb__open_changes_table(DB **changes_p,
DB_ENV *env,
svn_boolean_t create);
svn_error_t *svn_fs_bdb__changes_add(svn_fs_t *fs,
const char *key,
change_t *change,
trail_t *trail,
apr_pool_t *pool);
svn_error_t *svn_fs_bdb__changes_delete(svn_fs_t *fs,
const char *key,
trail_t *trail,
apr_pool_t *pool);
svn_error_t *svn_fs_bdb__changes_fetch(apr_hash_t **changes_p,
svn_fs_t *fs,
const char *key,
trail_t *trail,
apr_pool_t *pool);
svn_error_t *svn_fs_bdb__changes_fetch_raw(apr_array_header_t **changes_p,
svn_fs_t *fs,
const char *key,
trail_t *trail,
apr_pool_t *pool);
#if defined(__cplusplus)
}
#endif
#endif
