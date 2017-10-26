#include <string.h>
#include <assert.h>
#include "bdb_compat.h"
#include "svn_fs.h"
#include "../fs.h"
#include "../err.h"
#include "dbt.h"
#include "../util/skel.h"
#include "../util/fs_skels.h"
#include "../trail.h"
#include "../key-gen.h"
#include "../id.h"
#include "../../libsvn_fs/fs-loader.h"
#include "bdb-err.h"
#include "nodes-table.h"
#include "svn_private_config.h"
int
svn_fs_bdb__open_nodes_table(DB **nodes_p,
DB_ENV *env,
svn_boolean_t create) {
const u_int32_t open_flags = (create ? (DB_CREATE | DB_EXCL) : 0);
DB *nodes;
BDB_ERR(svn_fs_bdb__check_version());
BDB_ERR(db_create(&nodes, env, 0));
BDB_ERR((nodes->open)(SVN_BDB_OPEN_PARAMS(nodes, NULL),
"nodes", 0, DB_BTREE,
open_flags, 0666));
if (create) {
DBT key, value;
BDB_ERR(nodes->put(nodes, 0,
svn_fs_base__str_to_dbt(&key, NEXT_KEY_KEY),
svn_fs_base__str_to_dbt(&value, "1"), 0));
}
*nodes_p = nodes;
return 0;
}
svn_error_t *
svn_fs_bdb__new_node_id(svn_fs_id_t **id_p,
svn_fs_t *fs,
const char *copy_id,
const char *txn_id,
trail_t *trail,
apr_pool_t *pool) {
base_fs_data_t *bfd = fs->fsap_data;
DBT query, result;
apr_size_t len;
char next_key[MAX_KEY_SIZE];
int db_err;
const char *next_node_id;
assert(txn_id);
svn_fs_base__str_to_dbt(&query, NEXT_KEY_KEY);
svn_fs_base__trail_debug(trail, "nodes", "get");
SVN_ERR(BDB_WRAP(fs, _("allocating new node ID (getting 'next-key')"),
bfd->nodes->get(bfd->nodes, trail->db_txn,
&query,
svn_fs_base__result_dbt(&result),
0)));
svn_fs_base__track_dbt(&result, pool);
next_node_id = apr_pstrmemdup(pool, result.data, result.size);
len = result.size;
svn_fs_base__next_key(result.data, &len, next_key);
svn_fs_base__trail_debug(trail, "nodes", "put");
db_err = bfd->nodes->put(bfd->nodes, trail->db_txn,
svn_fs_base__str_to_dbt(&query, NEXT_KEY_KEY),
svn_fs_base__str_to_dbt(&result, next_key),
0);
SVN_ERR(BDB_WRAP(fs, _("bumping next node ID key"), db_err));
*id_p = svn_fs_base__id_create(next_node_id, copy_id, txn_id, pool);
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_bdb__new_successor_id(svn_fs_id_t **successor_p,
svn_fs_t *fs,
const svn_fs_id_t *id,
const char *copy_id,
const char *txn_id,
trail_t *trail,
apr_pool_t *pool) {
svn_fs_id_t *new_id;
svn_error_t *err;
assert(txn_id);
new_id = svn_fs_base__id_create(svn_fs_base__id_node_id(id),
copy_id ? copy_id
: svn_fs_base__id_copy_id(id),
txn_id, pool);
err = svn_fs_bdb__get_node_revision(NULL, fs, new_id, trail, trail->pool);
if ((! err) || (err->apr_err != SVN_ERR_FS_ID_NOT_FOUND)) {
svn_string_t *id_str = svn_fs_base__id_unparse(id, pool);
svn_string_t *new_id_str = svn_fs_base__id_unparse(new_id, pool);
return svn_error_createf
(SVN_ERR_FS_ALREADY_EXISTS, err,
_("Successor id '%s' (for '%s') already exists in filesystem '%s'"),
new_id_str->data, id_str->data, fs->path);
}
svn_error_clear(err);
*successor_p = new_id;
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_bdb__delete_nodes_entry(svn_fs_t *fs,
const svn_fs_id_t *id,
trail_t *trail,
apr_pool_t *pool) {
base_fs_data_t *bfd = fs->fsap_data;
DBT key;
svn_fs_base__trail_debug(trail, "nodes", "del");
SVN_ERR(BDB_WRAP(fs, _("deleting entry from 'nodes' table"),
bfd->nodes->del(bfd->nodes,
trail->db_txn,
svn_fs_base__id_to_dbt(&key, id, pool),
0)));
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_bdb__get_node_revision(node_revision_t **noderev_p,
svn_fs_t *fs,
const svn_fs_id_t *id,
trail_t *trail,
apr_pool_t *pool) {
base_fs_data_t *bfd = fs->fsap_data;
node_revision_t *noderev;
skel_t *skel;
int db_err;
DBT key, value;
svn_fs_base__trail_debug(trail, "nodes", "get");
db_err = bfd->nodes->get(bfd->nodes, trail->db_txn,
svn_fs_base__id_to_dbt(&key, id, pool),
svn_fs_base__result_dbt(&value),
0);
svn_fs_base__track_dbt(&value, pool);
if (db_err == DB_NOTFOUND)
return svn_fs_base__err_dangling_id(fs, id);
SVN_ERR(BDB_WRAP(fs, _("reading node revision"), db_err));
if (! noderev_p)
return SVN_NO_ERROR;
skel = svn_fs_base__parse_skel(value.data, value.size, pool);
SVN_ERR(svn_fs_base__parse_node_revision_skel(&noderev, skel, pool));
*noderev_p = noderev;
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_bdb__put_node_revision(svn_fs_t *fs,
const svn_fs_id_t *id,
node_revision_t *noderev,
trail_t *trail,
apr_pool_t *pool) {
base_fs_data_t *bfd = fs->fsap_data;
DB_TXN *db_txn = trail->db_txn;
DBT key, value;
skel_t *skel;
SVN_ERR(svn_fs_base__unparse_node_revision_skel(&skel, noderev,
bfd->format, pool));
svn_fs_base__trail_debug(trail, "nodes", "put");
return BDB_WRAP(fs, _("storing node revision"),
bfd->nodes->put(bfd->nodes, db_txn,
svn_fs_base__id_to_dbt(&key, id, pool),
svn_fs_base__skel_to_dbt(&value, skel,
pool),
0));
}
