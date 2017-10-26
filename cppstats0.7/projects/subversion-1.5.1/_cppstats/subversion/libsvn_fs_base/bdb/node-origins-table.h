#if !defined(SVN_LIBSVN_FS_NODE_ORIGINS_TABLE_H)
#define SVN_LIBSVN_FS_NODE_ORIGINS_TABLE_H
#include "svn_fs.h"
#include "svn_error.h"
#include "../trail.h"
#include "../fs.h"
#if defined(__cplusplus)
extern "C" {
#endif
int svn_fs_bdb__open_node_origins_table(DB **node_origins_p,
DB_ENV *env,
svn_boolean_t create);
svn_error_t *svn_fs_bdb__get_node_origin(const svn_fs_id_t **origin_id,
svn_fs_t *fs,
const char *node_id,
trail_t *trail,
apr_pool_t *pool);
svn_error_t *svn_fs_bdb__set_node_origin(svn_fs_t *fs,
const char *node_id,
const svn_fs_id_t *origin_id,
trail_t *trail,
apr_pool_t *pool);
svn_error_t *svn_fs_bdb__delete_node_origin(svn_fs_t *fs,
const char *node_id,
trail_t *trail,
apr_pool_t *pool);
#if defined(__cplusplus)
}
#endif
#endif
