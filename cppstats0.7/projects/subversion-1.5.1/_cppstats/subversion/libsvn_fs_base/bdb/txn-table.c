#include <string.h>
#include <assert.h>
#include "bdb_compat.h"
#include "svn_pools.h"
#include "dbt.h"
#include "../err.h"
#include "../fs.h"
#include "../key-gen.h"
#include "../util/skel.h"
#include "../util/fs_skels.h"
#include "../trail.h"
#include "../../libsvn_fs/fs-loader.h"
#include "bdb-err.h"
#include "txn-table.h"
#include "svn_private_config.h"
static svn_boolean_t
is_committed(transaction_t *txn) {
return (txn->kind == transaction_kind_committed) ? TRUE : FALSE;
}
int
svn_fs_bdb__open_transactions_table(DB **transactions_p,
DB_ENV *env,
svn_boolean_t create) {
const u_int32_t open_flags = (create ? (DB_CREATE | DB_EXCL) : 0);
DB *txns;
BDB_ERR(svn_fs_bdb__check_version());
BDB_ERR(db_create(&txns, env, 0));
BDB_ERR((txns->open)(SVN_BDB_OPEN_PARAMS(txns, NULL),
"transactions", 0, DB_BTREE,
open_flags, 0666));
if (create) {
DBT key, value;
BDB_ERR(txns->put(txns, 0,
svn_fs_base__str_to_dbt(&key, NEXT_KEY_KEY),
svn_fs_base__str_to_dbt(&value, "0"), 0));
}
*transactions_p = txns;
return 0;
}
svn_error_t *
svn_fs_bdb__put_txn(svn_fs_t *fs,
const transaction_t *txn,
const char *txn_name,
trail_t *trail,
apr_pool_t *pool) {
base_fs_data_t *bfd = fs->fsap_data;
skel_t *txn_skel;
DBT key, value;
SVN_ERR(svn_fs_base__unparse_transaction_skel(&txn_skel, txn, pool));
svn_fs_base__str_to_dbt(&key, txn_name);
svn_fs_base__skel_to_dbt(&value, txn_skel, pool);
svn_fs_base__trail_debug(trail, "transactions", "put");
SVN_ERR(BDB_WRAP(fs, _("storing transaction record"),
bfd->transactions->put(bfd->transactions, trail->db_txn,
&key, &value, 0)));
return SVN_NO_ERROR;
}
static svn_error_t *
allocate_txn_id(const char **id_p,
svn_fs_t *fs,
trail_t *trail,
apr_pool_t *pool) {
base_fs_data_t *bfd = fs->fsap_data;
DBT query, result;
apr_size_t len;
char next_key[MAX_KEY_SIZE];
int db_err;
svn_fs_base__str_to_dbt(&query, NEXT_KEY_KEY);
svn_fs_base__trail_debug(trail, "transactions", "get");
SVN_ERR(BDB_WRAP(fs, "allocating new transaction ID (getting 'next-key')",
bfd->transactions->get(bfd->transactions, trail->db_txn,
&query,
svn_fs_base__result_dbt(&result),
0)));
svn_fs_base__track_dbt(&result, pool);
*id_p = apr_pstrmemdup(pool, result.data, result.size);
len = result.size;
svn_fs_base__next_key(result.data, &len, next_key);
svn_fs_base__str_to_dbt(&query, NEXT_KEY_KEY);
svn_fs_base__str_to_dbt(&result, next_key);
svn_fs_base__trail_debug(trail, "transactions", "put");
db_err = bfd->transactions->put(bfd->transactions, trail->db_txn,
&query, &result, 0);
SVN_ERR(BDB_WRAP(fs, "bumping next transaction key", db_err));
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_bdb__create_txn(const char **txn_name_p,
svn_fs_t *fs,
const svn_fs_id_t *root_id,
trail_t *trail,
apr_pool_t *pool) {
const char *txn_name;
transaction_t txn;
SVN_ERR(allocate_txn_id(&txn_name, fs, trail, pool));
txn.kind = transaction_kind_normal;
txn.root_id = root_id;
txn.base_id = root_id;
txn.proplist = NULL;
txn.copies = NULL;
txn.revision = SVN_INVALID_REVNUM;
SVN_ERR(svn_fs_bdb__put_txn(fs, &txn, txn_name, trail, pool));
*txn_name_p = txn_name;
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_bdb__delete_txn(svn_fs_t *fs,
const char *txn_name,
trail_t *trail,
apr_pool_t *pool) {
base_fs_data_t *bfd = fs->fsap_data;
DBT key;
transaction_t *txn;
SVN_ERR(svn_fs_bdb__get_txn(&txn, fs, txn_name, trail, pool));
if (is_committed(txn))
return svn_fs_base__err_txn_not_mutable(fs, txn_name);
svn_fs_base__str_to_dbt(&key, txn_name);
svn_fs_base__trail_debug(trail, "transactions", "del");
SVN_ERR(BDB_WRAP(fs, "deleting entry from 'transactions' table",
bfd->transactions->del(bfd->transactions,
trail->db_txn, &key, 0)));
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_bdb__get_txn(transaction_t **txn_p,
svn_fs_t *fs,
const char *txn_name,
trail_t *trail,
apr_pool_t *pool) {
base_fs_data_t *bfd = fs->fsap_data;
DBT key, value;
int db_err;
skel_t *skel;
transaction_t *transaction;
svn_fs_base__trail_debug(trail, "transactions", "get");
db_err = bfd->transactions->get(bfd->transactions, trail->db_txn,
svn_fs_base__str_to_dbt(&key, txn_name),
svn_fs_base__result_dbt(&value),
0);
svn_fs_base__track_dbt(&value, pool);
if (db_err == DB_NOTFOUND)
return svn_fs_base__err_no_such_txn(fs, txn_name);
SVN_ERR(BDB_WRAP(fs, "reading transaction", db_err));
skel = svn_fs_base__parse_skel(value.data, value.size, pool);
if (! skel)
return svn_fs_base__err_corrupt_txn(fs, txn_name);
SVN_ERR(svn_fs_base__parse_transaction_skel(&transaction, skel, pool));
*txn_p = transaction;
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_bdb__get_txn_list(apr_array_header_t **names_p,
svn_fs_t *fs,
trail_t *trail,
apr_pool_t *pool) {
base_fs_data_t *bfd = fs->fsap_data;
apr_size_t const next_key_key_len = strlen(NEXT_KEY_KEY);
apr_pool_t *subpool = svn_pool_create(pool);
apr_array_header_t *names;
DBC *cursor;
DBT key, value;
int db_err, db_c_err;
names = apr_array_make(pool, 4, sizeof(const char *));
svn_fs_base__trail_debug(trail, "transactions", "cursor");
SVN_ERR(BDB_WRAP(fs, "reading transaction list (opening cursor)",
bfd->transactions->cursor(bfd->transactions,
trail->db_txn, &cursor, 0)));
for (db_err = svn_bdb_dbc_get(cursor,
svn_fs_base__result_dbt(&key),
svn_fs_base__result_dbt(&value),
DB_FIRST);
db_err == 0;
db_err = svn_bdb_dbc_get(cursor,
svn_fs_base__result_dbt(&key),
svn_fs_base__result_dbt(&value),
DB_NEXT)) {
transaction_t *txn;
skel_t *txn_skel;
svn_error_t *err;
svn_pool_clear(subpool);
svn_fs_base__track_dbt(&key, subpool);
svn_fs_base__track_dbt(&value, subpool);
if (key.size == next_key_key_len
&& 0 == memcmp(key.data, NEXT_KEY_KEY, next_key_key_len))
continue;
txn_skel = svn_fs_base__parse_skel(value.data, value.size, subpool);
if (! txn_skel) {
svn_bdb_dbc_close(cursor);
return svn_fs_base__err_corrupt_txn
(fs, apr_pstrmemdup(pool, key.data, key.size));
}
if ((err = svn_fs_base__parse_transaction_skel(&txn, txn_skel,
subpool))) {
svn_bdb_dbc_close(cursor);
return err;
}
if (is_committed(txn))
continue;
APR_ARRAY_PUSH(names, const char *) = apr_pstrmemdup(pool, key.data,
key.size);
}
db_c_err = svn_bdb_dbc_close(cursor);
if (db_err != DB_NOTFOUND) {
SVN_ERR(BDB_WRAP(fs, "reading transaction list (listing keys)",
db_err));
}
SVN_ERR(BDB_WRAP(fs, "reading transaction list (closing cursor)",
db_c_err));
svn_pool_destroy(subpool);
*names_p = names;
return SVN_NO_ERROR;
}