#include "bdb_compat.h"
#include "svn_fs.h"
#include "svn_pools.h"
#include "../fs.h"
#include "../err.h"
#include "dbt.h"
#include "../trail.h"
#include "../key-gen.h"
#include "../../libsvn_fs/fs-loader.h"
#include "bdb-err.h"
#include "strings-table.h"
#include "svn_private_config.h"
int
svn_fs_bdb__open_strings_table(DB **strings_p,
DB_ENV *env,
svn_boolean_t create) {
const u_int32_t open_flags = (create ? (DB_CREATE | DB_EXCL) : 0);
DB *strings;
BDB_ERR(svn_fs_bdb__check_version());
BDB_ERR(db_create(&strings, env, 0));
BDB_ERR(strings->set_flags(strings, DB_DUP));
BDB_ERR((strings->open)(SVN_BDB_OPEN_PARAMS(strings, NULL),
"strings", 0, DB_BTREE,
open_flags, 0666));
if (create) {
DBT key, value;
BDB_ERR(strings->put
(strings, 0,
svn_fs_base__str_to_dbt(&key, NEXT_KEY_KEY),
svn_fs_base__str_to_dbt(&value, "0"), 0));
}
*strings_p = strings;
return 0;
}
static svn_error_t *
locate_key(apr_size_t *length,
DBC **cursor,
DBT *query,
svn_fs_t *fs,
trail_t *trail,
apr_pool_t *pool) {
base_fs_data_t *bfd = fs->fsap_data;
int db_err;
DBT result;
svn_fs_base__trail_debug(trail, "strings", "cursor");
SVN_ERR(BDB_WRAP(fs, _("creating cursor for reading a string"),
bfd->strings->cursor(bfd->strings, trail->db_txn,
cursor, 0)));
svn_fs_base__clear_dbt(&result);
result.ulen = 0;
result.flags |= DB_DBT_USERMEM;
db_err = svn_bdb_dbc_get(*cursor, query, &result, DB_SET);
if (db_err == DB_NOTFOUND) {
svn_bdb_dbc_close(*cursor);
return svn_error_createf
(SVN_ERR_FS_NO_SUCH_STRING, 0,
"No such string '%s'", (const char *)query->data);
}
if (db_err) {
DBT rerun;
if (db_err != SVN_BDB_DB_BUFFER_SMALL) {
svn_bdb_dbc_close(*cursor);
return BDB_WRAP(fs, "moving cursor", db_err);
}
svn_fs_base__clear_dbt(&rerun);
rerun.flags |= DB_DBT_USERMEM | DB_DBT_PARTIAL;
db_err = svn_bdb_dbc_get(*cursor, query, &rerun, DB_SET);
if (db_err) {
svn_bdb_dbc_close(*cursor);
return BDB_WRAP(fs, "rerunning cursor move", db_err);
}
}
*length = (apr_size_t) result.size;
return SVN_NO_ERROR;
}
static int
get_next_length(apr_size_t *length, DBC *cursor, DBT *query) {
DBT result;
int db_err;
svn_fs_base__clear_dbt(&result);
result.ulen = 0;
result.flags |= DB_DBT_USERMEM;
db_err = svn_bdb_dbc_get(cursor, query, &result, DB_NEXT_DUP);
if (db_err) {
DBT rerun;
if (db_err != SVN_BDB_DB_BUFFER_SMALL) {
svn_bdb_dbc_close(cursor);
return db_err;
}
svn_fs_base__clear_dbt(&rerun);
rerun.flags |= DB_DBT_USERMEM | DB_DBT_PARTIAL;
db_err = svn_bdb_dbc_get(cursor, query, &rerun, DB_NEXT_DUP);
if (db_err)
svn_bdb_dbc_close(cursor);
}
*length = (apr_size_t) result.size;
return db_err;
}
svn_error_t *
svn_fs_bdb__string_read(svn_fs_t *fs,
const char *key,
char *buf,
svn_filesize_t offset,
apr_size_t *len,
trail_t *trail,
apr_pool_t *pool) {
int db_err;
DBT query, result;
DBC *cursor;
apr_size_t length, bytes_read = 0;
svn_fs_base__str_to_dbt(&query, key);
SVN_ERR(locate_key(&length, &cursor, &query, fs, trail, pool));
while (length <= offset) {
offset -= length;
db_err = get_next_length(&length, cursor, &query);
if (db_err == DB_NOTFOUND) {
*len = 0;
return SVN_NO_ERROR;
}
if (db_err)
return BDB_WRAP(fs, "reading string", db_err);
}
while (1) {
svn_fs_base__clear_dbt(&result);
result.data = buf + bytes_read;
result.ulen = *len - bytes_read;
result.doff = (u_int32_t)offset;
result.dlen = *len - bytes_read;
result.flags |= (DB_DBT_USERMEM | DB_DBT_PARTIAL);
db_err = svn_bdb_dbc_get(cursor, &query, &result, DB_CURRENT);
if (db_err) {
svn_bdb_dbc_close(cursor);
return BDB_WRAP(fs, "reading string", db_err);
}
bytes_read += result.size;
if (bytes_read == *len) {
SVN_ERR(BDB_WRAP(fs, "closing string-reading cursor",
svn_bdb_dbc_close(cursor)));
break;
}
db_err = get_next_length(&length, cursor, &query);
if (db_err == DB_NOTFOUND)
break;
if (db_err)
return BDB_WRAP(fs, "reading string", db_err);
offset = 0;
}
*len = bytes_read;
return SVN_NO_ERROR;
}
static svn_error_t *
get_key_and_bump(svn_fs_t *fs,
const char **key,
trail_t *trail,
apr_pool_t *pool) {
base_fs_data_t *bfd = fs->fsap_data;
DBC *cursor;
char next_key[MAX_KEY_SIZE];
apr_size_t key_len;
int db_err;
DBT query;
DBT result;
svn_fs_base__trail_debug(trail, "strings", "cursor");
SVN_ERR(BDB_WRAP(fs, "creating cursor for reading a string",
bfd->strings->cursor(bfd->strings, trail->db_txn,
&cursor, 0)));
db_err = svn_bdb_dbc_get(cursor,
svn_fs_base__str_to_dbt(&query, NEXT_KEY_KEY),
svn_fs_base__result_dbt(&result),
DB_SET);
if (db_err) {
svn_bdb_dbc_close(cursor);
return BDB_WRAP(fs, "getting next-key value", db_err);
}
svn_fs_base__track_dbt(&result, pool);
*key = apr_pstrmemdup(pool, result.data, result.size);
key_len = result.size;
svn_fs_base__next_key(result.data, &key_len, next_key);
db_err = svn_bdb_dbc_put(cursor, &query,
svn_fs_base__str_to_dbt(&result, next_key),
DB_CURRENT);
if (db_err) {
svn_bdb_dbc_close(cursor);
return BDB_WRAP(fs, "bumping next string key", db_err);
}
return BDB_WRAP(fs, "closing string-reading cursor",
svn_bdb_dbc_close(cursor));
}
svn_error_t *
svn_fs_bdb__string_append(svn_fs_t *fs,
const char **key,
apr_size_t len,
const char *buf,
trail_t *trail,
apr_pool_t *pool) {
base_fs_data_t *bfd = fs->fsap_data;
DBT query, result;
if (*key == NULL) {
SVN_ERR(get_key_and_bump(fs, key, trail, pool));
}
svn_fs_base__trail_debug(trail, "strings", "put");
SVN_ERR(BDB_WRAP(fs, "appending string",
bfd->strings->put
(bfd->strings, trail->db_txn,
svn_fs_base__str_to_dbt(&query, *key),
svn_fs_base__set_dbt(&result, buf, len),
0)));
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_bdb__string_clear(svn_fs_t *fs,
const char *key,
trail_t *trail,
apr_pool_t *pool) {
base_fs_data_t *bfd = fs->fsap_data;
int db_err;
DBT query, result;
svn_fs_base__str_to_dbt(&query, key);
svn_fs_base__trail_debug(trail, "strings", "del");
db_err = bfd->strings->del(bfd->strings, trail->db_txn, &query, 0);
if (db_err == DB_NOTFOUND)
return svn_error_createf
(SVN_ERR_FS_NO_SUCH_STRING, 0,
"No such string '%s'", key);
SVN_ERR(BDB_WRAP(fs, "clearing string", db_err));
svn_fs_base__clear_dbt(&result);
result.data = 0;
result.size = 0;
result.flags |= DB_DBT_USERMEM;
svn_fs_base__trail_debug(trail, "strings", "put");
return BDB_WRAP(fs, "storing empty contents",
bfd->strings->put(bfd->strings, trail->db_txn,
&query, &result, 0));
}
svn_error_t *
svn_fs_bdb__string_size(svn_filesize_t *size,
svn_fs_t *fs,
const char *key,
trail_t *trail,
apr_pool_t *pool) {
int db_err;
DBT query;
DBC *cursor;
apr_size_t length;
svn_filesize_t total;
svn_fs_base__str_to_dbt(&query, key);
SVN_ERR(locate_key(&length, &cursor, &query, fs, trail, pool));
total = length;
while (1) {
db_err = get_next_length(&length, cursor, &query);
if (db_err == DB_NOTFOUND) {
*size = total;
return SVN_NO_ERROR;
}
if (db_err)
return BDB_WRAP(fs, "fetching string length", db_err);
total += length;
}
}
svn_error_t *
svn_fs_bdb__string_delete(svn_fs_t *fs,
const char *key,
trail_t *trail,
apr_pool_t *pool) {
base_fs_data_t *bfd = fs->fsap_data;
int db_err;
DBT query;
svn_fs_base__trail_debug(trail, "strings", "del");
db_err = bfd->strings->del(bfd->strings, trail->db_txn,
svn_fs_base__str_to_dbt(&query, key), 0);
if (db_err == DB_NOTFOUND)
return svn_error_createf
(SVN_ERR_FS_NO_SUCH_STRING, 0,
"No such string '%s'", key);
SVN_ERR(BDB_WRAP(fs, "deleting string", db_err));
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_bdb__string_copy(svn_fs_t *fs,
const char **new_key,
const char *key,
trail_t *trail,
apr_pool_t *pool) {
base_fs_data_t *bfd = fs->fsap_data;
DBT query;
DBT result;
DBT copykey;
DBC *cursor;
int db_err;
const char *old_key = apr_pstrdup(pool, key);
SVN_ERR(get_key_and_bump(fs, new_key, trail, pool));
svn_fs_base__trail_debug(trail, "strings", "cursor");
SVN_ERR(BDB_WRAP(fs, "creating cursor for reading a string",
bfd->strings->cursor(bfd->strings, trail->db_txn,
&cursor, 0)));
svn_fs_base__str_to_dbt(&query, old_key);
svn_fs_base__str_to_dbt(&copykey, *new_key);
svn_fs_base__clear_dbt(&result);
db_err = svn_bdb_dbc_get(cursor, &query, &result, DB_SET);
if (db_err) {
svn_bdb_dbc_close(cursor);
return BDB_WRAP(fs, "getting next-key value", db_err);
}
while (1) {
svn_fs_base__trail_debug(trail, "strings", "put");
db_err = bfd->strings->put(bfd->strings, trail->db_txn,
&copykey, &result, 0);
if (db_err) {
svn_bdb_dbc_close(cursor);
return BDB_WRAP(fs, "writing copied data", db_err);
}
svn_fs_base__clear_dbt(&result);
db_err = svn_bdb_dbc_get(cursor, &query, &result, DB_NEXT_DUP);
if (db_err == DB_NOTFOUND)
break;
if (db_err) {
svn_bdb_dbc_close(cursor);
return BDB_WRAP(fs, "fetching string data for a copy", db_err);
}
}
return BDB_WRAP(fs, "closing string-reading cursor",
svn_bdb_dbc_close(cursor));
}