#if !defined(SVN_LIBSVN_FS_REPS_TABLE_H)
#define SVN_LIBSVN_FS_REPS_TABLE_H
#define APU_WANT_DB
#include <apu_want.h>
#include "svn_io.h"
#include "svn_fs.h"
#include "../fs.h"
#include "../trail.h"
#if defined(__cplusplus)
extern "C" {
#endif
int svn_fs_bdb__open_reps_table(DB **reps_p,
DB_ENV *env,
svn_boolean_t create);
svn_error_t *svn_fs_bdb__read_rep(representation_t **rep_p,
svn_fs_t *fs,
const char *key,
trail_t *trail,
apr_pool_t *pool);
svn_error_t *svn_fs_bdb__write_rep(svn_fs_t *fs,
const char *key,
const representation_t *rep,
trail_t *trail,
apr_pool_t *pool);
svn_error_t *svn_fs_bdb__write_new_rep(const char **key,
svn_fs_t *fs,
const representation_t *rep,
trail_t *trail,
apr_pool_t *pool);
svn_error_t *svn_fs_bdb__delete_rep(svn_fs_t *fs,
const char *key,
trail_t *trail,
apr_pool_t *pool);
#if defined(__cplusplus)
}
#endif
#endif
