#if !defined(SVN_LIBSVN_FS_NODE_REV_H)
#define SVN_LIBSVN_FS_NODE_REV_H
#define APU_WANT_DB
#include <apu_want.h>
#include "svn_fs.h"
#include "trail.h"
#if defined(__cplusplus)
extern "C" {
#endif
svn_error_t *svn_fs_base__create_node(const svn_fs_id_t **id_p,
svn_fs_t *fs,
node_revision_t *noderev,
const char *copy_id,
const char *txn_id,
trail_t *trail,
apr_pool_t *pool);
svn_error_t *svn_fs_base__create_successor(const svn_fs_id_t **new_id_p,
svn_fs_t *fs,
const svn_fs_id_t *old_id,
node_revision_t *new_nr,
const char *copy_id,
const char *txn_id,
trail_t *trail,
apr_pool_t *pool);
svn_error_t *svn_fs_base__delete_node_revision(svn_fs_t *fs,
const svn_fs_id_t *id,
svn_boolean_t origin_also,
trail_t *trail,
apr_pool_t *pool);
#if defined(__cplusplus)
}
#endif
#endif