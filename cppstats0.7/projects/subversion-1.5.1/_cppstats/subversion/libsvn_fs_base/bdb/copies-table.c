#include <string.h>
#include "bdb_compat.h"
#include "../fs.h"
#include "../err.h"
#include "../key-gen.h"
#include "dbt.h"
#include "../util/skel.h"
#include "../util/fs_skels.h"
#include "../trail.h"
#include "../../libsvn_fs/fs-loader.h"
#include "bdb-err.h"
#include "copies-table.h"
#include "rev-table.h"
#include "svn_private_config.h"
int
svn_fs_bdb__open_copies_table(DB **copies_p,
DB_ENV *env,
svn_boolean_t create) {
const u_int32_t open_flags = (create ? (DB_CREATE | DB_EXCL) : 0);
DB *copies;
BDB_ERR(svn_fs_bdb__check_version());
BDB_ERR(db_create(&copies, env, 0));
BDB_ERR((copies->open)(SVN_BDB_OPEN_PARAMS(copies, NULL),
"copies", 0, DB_BTREE,
open_flags, 0666));
if (create) {
DBT key, value;
BDB_ERR(copies->put(copies, 0,
svn_fs_base__str_to_dbt(&key, NEXT_KEY_KEY),
svn_fs_base__str_to_dbt(&value, "0"), 0));
}
*copies_p = copies;
return 0;
}
static svn_error_t *
put_copy(svn_fs_t *fs,
const copy_t *copy,
const char *copy_id,
trail_t *trail,
apr_pool_t *pool) {
base_fs_data_t *bfd = fs->fsap_data;
skel_t *copy_skel;
DBT key, value;
SVN_ERR(svn_fs_base__unparse_copy_skel(&copy_skel, copy, pool));
svn_fs_base__str_to_dbt(&key, copy_id);
svn_fs_base__skel_to_dbt(&value, copy_skel, pool);
svn_fs_base__trail_debug(trail, "copies", "put");
SVN_ERR(BDB_WRAP(fs, _("storing copy record"),
bfd->copies->put(bfd->copies, trail->db_txn,
&key, &value, 0)));
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_bdb__reserve_copy_id(const char **id_p,
svn_fs_t *fs,
trail_t *trail,
apr_pool_t *pool) {
base_fs_data_t *bfd = fs->fsap_data;
DBT query, result;
apr_size_t len;
char next_key[MAX_KEY_SIZE];
int db_err;
svn_fs_base__str_to_dbt(&query, NEXT_KEY_KEY);
svn_fs_base__trail_debug(trail, "copies", "get");
SVN_ERR(BDB_WRAP(fs, _("allocating new copy ID (getting 'next-key')"),
bfd->copies->get(bfd->copies, trail->db_txn, &query,
svn_fs_base__result_dbt(&result),
0)));
svn_fs_base__track_dbt(&result, pool);
*id_p = apr_pstrmemdup(pool, result.data, result.size);
len = result.size;
svn_fs_base__next_key(result.data, &len, next_key);
svn_fs_base__trail_debug(trail, "copies", "put");
db_err = bfd->copies->put(bfd->copies, trail->db_txn,
svn_fs_base__str_to_dbt(&query, NEXT_KEY_KEY),
svn_fs_base__str_to_dbt(&result, next_key),
0);
SVN_ERR(BDB_WRAP(fs, _("bumping next copy key"), db_err));
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_bdb__create_copy(svn_fs_t *fs,
const char *copy_id,
const char *src_path,
const char *src_txn_id,
const svn_fs_id_t *dst_noderev_id,
copy_kind_t kind,
trail_t *trail,
apr_pool_t *pool) {
copy_t copy;
copy.kind = kind;
copy.src_path = src_path;
copy.src_txn_id = src_txn_id;
copy.dst_noderev_id = dst_noderev_id;
return put_copy(fs, &copy, copy_id, trail, pool);
}
svn_error_t *
svn_fs_bdb__delete_copy(svn_fs_t *fs,
const char *copy_id,
trail_t *trail,
apr_pool_t *pool) {
base_fs_data_t *bfd = fs->fsap_data;
DBT key;
int db_err;
svn_fs_base__str_to_dbt(&key, copy_id);
svn_fs_base__trail_debug(trail, "copies", "del");
db_err = bfd->copies->del(bfd->copies, trail->db_txn, &key, 0);
if (db_err == DB_NOTFOUND)
return svn_fs_base__err_no_such_copy(fs, copy_id);
return BDB_WRAP(fs, _("deleting entry from 'copies' table"), db_err);
}
svn_error_t *
svn_fs_bdb__get_copy(copy_t **copy_p,
svn_fs_t *fs,
const char *copy_id,
trail_t *trail,
apr_pool_t *pool) {
base_fs_data_t *bfd = fs->fsap_data;
DBT key, value;
int db_err;
skel_t *skel;
copy_t *copy;
svn_fs_base__trail_debug(trail, "copies", "get");
db_err = bfd->copies->get(bfd->copies, trail->db_txn,
svn_fs_base__str_to_dbt(&key, copy_id),
svn_fs_base__result_dbt(&value),
0);
svn_fs_base__track_dbt(&value, pool);
if (db_err == DB_NOTFOUND)
return svn_fs_base__err_no_such_copy(fs, copy_id);
SVN_ERR(BDB_WRAP(fs, _("reading copy"), db_err));
skel = svn_fs_base__parse_skel(value.data, value.size, pool);
if (! skel)
return svn_fs_base__err_corrupt_copy(fs, copy_id);
SVN_ERR(svn_fs_base__parse_copy_skel(&copy, skel, pool));
*copy_p = copy;
return SVN_NO_ERROR;
}
