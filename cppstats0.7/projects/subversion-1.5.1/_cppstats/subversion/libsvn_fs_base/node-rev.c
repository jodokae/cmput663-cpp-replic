#include <string.h>
#define APU_WANT_DB
#include <apu_want.h>
#include "svn_fs.h"
#include "fs.h"
#include "err.h"
#include "node-rev.h"
#include "reps-strings.h"
#include "id.h"
#include "../libsvn_fs/fs-loader.h"
#include "bdb/nodes-table.h"
#include "bdb/node-origins-table.h"
svn_error_t *
svn_fs_base__create_node(const svn_fs_id_t **id_p,
svn_fs_t *fs,
node_revision_t *noderev,
const char *copy_id,
const char *txn_id,
trail_t *trail,
apr_pool_t *pool) {
svn_fs_id_t *id;
base_fs_data_t *bfd = fs->fsap_data;
SVN_ERR(svn_fs_bdb__new_node_id(&id, fs, copy_id, txn_id, trail, pool));
SVN_ERR(svn_fs_bdb__put_node_revision(fs, id, noderev, trail, pool));
if (bfd->format >= SVN_FS_BASE__MIN_NODE_ORIGINS_FORMAT) {
SVN_ERR(svn_fs_bdb__set_node_origin(fs, svn_fs_base__id_node_id(id),
id, trail, pool));
}
*id_p = id;
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_base__create_successor(const svn_fs_id_t **new_id_p,
svn_fs_t *fs,
const svn_fs_id_t *old_id,
node_revision_t *new_noderev,
const char *copy_id,
const char *txn_id,
trail_t *trail,
apr_pool_t *pool) {
svn_fs_id_t *new_id;
SVN_ERR(svn_fs_bdb__new_successor_id(&new_id, fs, old_id, copy_id,
txn_id, trail, pool));
SVN_ERR(svn_fs_bdb__put_node_revision(fs, new_id, new_noderev,
trail, pool));
*new_id_p = new_id;
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_base__delete_node_revision(svn_fs_t *fs,
const svn_fs_id_t *id,
svn_boolean_t origin_also,
trail_t *trail,
apr_pool_t *pool) {
base_fs_data_t *bfd = fs->fsap_data;
if (origin_also && (bfd->format >= SVN_FS_BASE__MIN_NODE_ORIGINS_FORMAT)) {
SVN_ERR(svn_fs_bdb__delete_node_origin(fs, svn_fs_base__id_node_id(id),
trail, pool));
}
return svn_fs_bdb__delete_nodes_entry(fs, id, trail, pool);
}
