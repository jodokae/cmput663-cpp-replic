#include "bdb_compat.h"
#include <apr_hash.h>
#include <apr_tables.h>
#include "svn_fs.h"
#include "svn_pools.h"
#include "svn_path.h"
#include "../fs.h"
#include "../err.h"
#include "../trail.h"
#include "../id.h"
#include "../util/fs_skels.h"
#include "../../libsvn_fs/fs-loader.h"
#include "bdb-err.h"
#include "dbt.h"
#include "changes-table.h"
#include "svn_private_config.h"
int
svn_fs_bdb__open_changes_table(DB **changes_p,
DB_ENV *env,
svn_boolean_t create) {
const u_int32_t open_flags = (create ? (DB_CREATE | DB_EXCL) : 0);
DB *changes;
BDB_ERR(svn_fs_bdb__check_version());
BDB_ERR(db_create(&changes, env, 0));
BDB_ERR(changes->set_flags(changes, DB_DUP));
BDB_ERR((changes->open)(SVN_BDB_OPEN_PARAMS(changes, NULL),
"changes", 0, DB_BTREE,
open_flags, 0666));
*changes_p = changes;
return 0;
}
svn_error_t *
svn_fs_bdb__changes_add(svn_fs_t *fs,
const char *key,
change_t *change,
trail_t *trail,
apr_pool_t *pool) {
base_fs_data_t *bfd = fs->fsap_data;
DBT query, value;
skel_t *skel;
SVN_ERR(svn_fs_base__unparse_change_skel(&skel, change, pool));
svn_fs_base__str_to_dbt(&query, key);
svn_fs_base__skel_to_dbt(&value, skel, pool);
svn_fs_base__trail_debug(trail, "changes", "put");
SVN_ERR(BDB_WRAP(fs, _("creating change"),
bfd->changes->put(bfd->changes, trail->db_txn,
&query, &value, 0)));
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_bdb__changes_delete(svn_fs_t *fs,
const char *key,
trail_t *trail,
apr_pool_t *pool) {
int db_err;
DBT query;
base_fs_data_t *bfd = fs->fsap_data;
svn_fs_base__trail_debug(trail, "changes", "del");
db_err = bfd->changes->del(bfd->changes, trail->db_txn,
svn_fs_base__str_to_dbt(&query, key), 0);
if ((db_err) && (db_err != DB_NOTFOUND)) {
SVN_ERR(BDB_WRAP(fs, _("deleting changes"), db_err));
}
return SVN_NO_ERROR;
}
static svn_error_t *
fold_change(apr_hash_t *changes,
const change_t *change) {
apr_pool_t *pool = apr_hash_pool_get(changes);
svn_fs_path_change_t *old_change, *new_change;
const char *path;
if ((old_change = apr_hash_get(changes, change->path, APR_HASH_KEY_STRING))) {
path = change->path;
if ((! change->noderev_id) && (change->kind != svn_fs_path_change_reset))
return svn_error_create
(SVN_ERR_FS_CORRUPT, NULL,
_("Missing required node revision ID"));
if (change->noderev_id
&& (! svn_fs_base__id_eq(old_change->node_rev_id,
change->noderev_id))
&& (old_change->change_kind != svn_fs_path_change_delete))
return svn_error_create
(SVN_ERR_FS_CORRUPT, NULL,
_("Invalid change ordering: new node revision ID without delete"));
if ((old_change->change_kind == svn_fs_path_change_delete)
&& (! ((change->kind == svn_fs_path_change_replace)
|| (change->kind == svn_fs_path_change_reset)
|| (change->kind == svn_fs_path_change_add))))
return svn_error_create
(SVN_ERR_FS_CORRUPT, NULL,
_("Invalid change ordering: non-add change on deleted path"));
switch (change->kind) {
case svn_fs_path_change_reset:
old_change = NULL;
break;
case svn_fs_path_change_delete:
if (old_change->change_kind == svn_fs_path_change_add) {
old_change = NULL;
} else {
old_change->change_kind = svn_fs_path_change_delete;
old_change->text_mod = change->text_mod;
old_change->prop_mod = change->prop_mod;
}
break;
case svn_fs_path_change_add:
case svn_fs_path_change_replace:
old_change->change_kind = svn_fs_path_change_replace;
old_change->node_rev_id = svn_fs_base__id_copy(change->noderev_id,
pool);
old_change->text_mod = change->text_mod;
old_change->prop_mod = change->prop_mod;
break;
case svn_fs_path_change_modify:
default:
if (change->text_mod)
old_change->text_mod = TRUE;
if (change->prop_mod)
old_change->prop_mod = TRUE;
break;
}
new_change = old_change;
} else {
new_change = apr_pcalloc(pool, sizeof(*new_change));
new_change->node_rev_id = svn_fs_base__id_copy(change->noderev_id,
pool);
new_change->change_kind = change->kind;
new_change->text_mod = change->text_mod;
new_change->prop_mod = change->prop_mod;
path = apr_pstrdup(pool, change->path);
}
apr_hash_set(changes, path, APR_HASH_KEY_STRING, new_change);
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_bdb__changes_fetch(apr_hash_t **changes_p,
svn_fs_t *fs,
const char *key,
trail_t *trail,
apr_pool_t *pool) {
base_fs_data_t *bfd = fs->fsap_data;
DBC *cursor;
DBT query, result;
int db_err = 0, db_c_err = 0;
svn_error_t *err = SVN_NO_ERROR;
apr_hash_t *changes = apr_hash_make(pool);
apr_pool_t *subpool = svn_pool_create(pool);
svn_fs_base__trail_debug(trail, "changes", "cursor");
SVN_ERR(BDB_WRAP(fs, _("creating cursor for reading changes"),
bfd->changes->cursor(bfd->changes, trail->db_txn,
&cursor, 0)));
svn_fs_base__str_to_dbt(&query, key);
svn_fs_base__result_dbt(&result);
db_err = svn_bdb_dbc_get(cursor, &query, &result, DB_SET);
if (! db_err)
svn_fs_base__track_dbt(&result, pool);
while (! db_err) {
change_t *change;
skel_t *result_skel;
svn_pool_clear(subpool);
result_skel = svn_fs_base__parse_skel(result.data, result.size,
subpool);
if (! result_skel) {
err = svn_error_createf(SVN_ERR_FS_CORRUPT, NULL,
_("Error reading changes for key '%s'"),
key);
goto cleanup;
}
err = svn_fs_base__parse_change_skel(&change, result_skel, subpool);
if (err)
goto cleanup;
err = fold_change(changes, change);
if (err)
goto cleanup;
if ((change->kind == svn_fs_path_change_delete)
|| (change->kind == svn_fs_path_change_replace)) {
apr_hash_index_t *hi;
for (hi = apr_hash_first(subpool, changes);
hi;
hi = apr_hash_next(hi)) {
const void *hashkey;
apr_ssize_t klen;
apr_hash_this(hi, &hashkey, &klen, NULL);
if (strcmp(change->path, hashkey) == 0)
continue;
if (svn_path_is_child(change->path, hashkey, subpool))
apr_hash_set(changes, hashkey, klen, NULL);
}
}
svn_fs_base__result_dbt(&result);
db_err = svn_bdb_dbc_get(cursor, &query, &result, DB_NEXT_DUP);
if (! db_err)
svn_fs_base__track_dbt(&result, pool);
}
svn_pool_destroy(subpool);
if (db_err && (db_err != DB_NOTFOUND))
err = BDB_WRAP(fs, _("fetching changes"), db_err);
cleanup:
db_c_err = svn_bdb_dbc_close(cursor);
if (err)
return err;
if (db_c_err)
SVN_ERR(BDB_WRAP(fs, _("closing changes cursor"), db_c_err));
*changes_p = changes;
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_bdb__changes_fetch_raw(apr_array_header_t **changes_p,
svn_fs_t *fs,
const char *key,
trail_t *trail,
apr_pool_t *pool) {
base_fs_data_t *bfd = fs->fsap_data;
DBC *cursor;
DBT query, result;
int db_err = 0, db_c_err = 0;
svn_error_t *err = SVN_NO_ERROR;
change_t *change;
apr_array_header_t *changes = apr_array_make(pool, 4, sizeof(change));
svn_fs_base__trail_debug(trail, "changes", "cursor");
SVN_ERR(BDB_WRAP(fs, _("creating cursor for reading changes"),
bfd->changes->cursor(bfd->changes, trail->db_txn,
&cursor, 0)));
svn_fs_base__str_to_dbt(&query, key);
svn_fs_base__result_dbt(&result);
db_err = svn_bdb_dbc_get(cursor, &query, &result, DB_SET);
if (! db_err)
svn_fs_base__track_dbt(&result, pool);
while (! db_err) {
skel_t *result_skel;
result_skel = svn_fs_base__parse_skel(result.data, result.size, pool);
if (! result_skel) {
err = svn_error_createf(SVN_ERR_FS_CORRUPT, NULL,
_("Error reading changes for key '%s'"),
key);
goto cleanup;
}
err = svn_fs_base__parse_change_skel(&change, result_skel, pool);
if (err)
goto cleanup;
APR_ARRAY_PUSH(changes, change_t *) = change;
svn_fs_base__result_dbt(&result);
db_err = svn_bdb_dbc_get(cursor, &query, &result, DB_NEXT_DUP);
if (! db_err)
svn_fs_base__track_dbt(&result, pool);
}
if (db_err && (db_err != DB_NOTFOUND))
err = BDB_WRAP(fs, _("fetching changes"), db_err);
cleanup:
db_c_err = svn_bdb_dbc_close(cursor);
if (err)
return err;
if (db_c_err)
SVN_ERR(BDB_WRAP(fs, _("closing changes cursor"), db_c_err));
*changes_p = changes;
return SVN_NO_ERROR;
}