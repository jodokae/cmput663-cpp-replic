#if !defined(SVN_LIBSVN_FS_NODES_TABLE_H)
#define SVN_LIBSVN_FS_NODES_TABLE_H
#define APU_WANT_DB
#include <apu_want.h>
#include "svn_fs.h"
#include "../trail.h"
#if defined(__cplusplus)
extern "C" {
#endif
int svn_fs_bdb__open_nodes_table(DB **nodes_p,
DB_ENV *env,
svn_boolean_t create);
svn_error_t *svn_fs_bdb__new_node_id(svn_fs_id_t **id_p,
svn_fs_t *fs,
const char *copy_id,
const char *txn_id,
trail_t *trail,
apr_pool_t *pool);
svn_error_t *svn_fs_bdb__delete_nodes_entry(svn_fs_t *fs,
const svn_fs_id_t *id,
trail_t *trail,
apr_pool_t *pool);
svn_error_t *svn_fs_bdb__new_successor_id(svn_fs_id_t **successor_p,
svn_fs_t *fs,
const svn_fs_id_t *id,
const char *copy_id,
const char *txn_id,
trail_t *trail,
apr_pool_t *pool);
svn_error_t *svn_fs_bdb__get_node_revision(node_revision_t **noderev_p,
svn_fs_t *fs,
const svn_fs_id_t *id,
trail_t *trail,
apr_pool_t *pool);
svn_error_t *svn_fs_bdb__put_node_revision(svn_fs_t *fs,
const svn_fs_id_t *id,
node_revision_t *noderev,
trail_t *trail,
apr_pool_t *pool);
#if defined(__cplusplus)
}
#endif
#endif
