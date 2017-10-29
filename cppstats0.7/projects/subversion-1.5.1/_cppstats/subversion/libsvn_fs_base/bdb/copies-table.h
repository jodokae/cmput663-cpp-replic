#if !defined(SVN_LIBSVN_FS_COPIES_TABLE_H)
#define SVN_LIBSVN_FS_COPIES_TABLE_H
#include "svn_fs.h"
#include "../fs.h"
#include "../trail.h"
#if defined(__cplusplus)
extern "C" {
#endif
int svn_fs_bdb__open_copies_table(DB **copies_p,
DB_ENV *env,
svn_boolean_t create);
svn_error_t *svn_fs_bdb__reserve_copy_id(const char **copy_id_p,
svn_fs_t *fs,
trail_t *trail,
apr_pool_t *pool);
svn_error_t *svn_fs_bdb__create_copy(svn_fs_t *fs,
const char *copy_id,
const char *src_path,
const char *src_txn_id,
const svn_fs_id_t *dst_noderev_id,
copy_kind_t kind,
trail_t *trail,
apr_pool_t *pool);
svn_error_t *svn_fs_bdb__delete_copy(svn_fs_t *fs,
const char *copy_id,
trail_t *trail,
apr_pool_t *pool);
svn_error_t *svn_fs_bdb__get_copy(copy_t **copy_p,
svn_fs_t *fs,
const char *copy_id,
trail_t *trail,
apr_pool_t *pool);
#if defined(__cplusplus)
}
#endif
#endif
