#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#define APU_WANT_DB
#include <apu_want.h>
#include <apr_general.h>
#include <apr_pools.h>
#include <apr_file_io.h>
#include "svn_pools.h"
#include "svn_fs.h"
#include "svn_path.h"
#include "svn_utf.h"
#include "svn_delta.h"
#include "svn_version.h"
#include "fs.h"
#include "err.h"
#include "dag.h"
#include "revs-txns.h"
#include "uuid.h"
#include "tree.h"
#include "id.h"
#include "lock.h"
#include "svn_private_config.h"
#include "bdb/bdb-err.h"
#include "bdb/bdb_compat.h"
#include "bdb/env.h"
#include "bdb/nodes-table.h"
#include "bdb/rev-table.h"
#include "bdb/txn-table.h"
#include "bdb/copies-table.h"
#include "bdb/changes-table.h"
#include "bdb/reps-table.h"
#include "bdb/strings-table.h"
#include "bdb/uuids-table.h"
#include "bdb/locks-table.h"
#include "bdb/lock-tokens-table.h"
#include "bdb/node-origins-table.h"
#include "../libsvn_fs/fs-loader.h"
#include "private/svn_fs_util.h"
static svn_error_t *
check_bdb_version(void) {
int major, minor, patch;
db_version(&major, &minor, &patch);
if ((major < SVN_FS_WANT_DB_MAJOR)
|| (major == SVN_FS_WANT_DB_MAJOR && minor < SVN_FS_WANT_DB_MINOR)
|| (major == SVN_FS_WANT_DB_MAJOR && minor == SVN_FS_WANT_DB_MINOR
&& patch < SVN_FS_WANT_DB_PATCH))
return svn_error_createf(SVN_ERR_FS_GENERAL, 0,
_("Bad database version: got %d.%d.%d,"
" should be at least %d.%d.%d"),
major, minor, patch,
SVN_FS_WANT_DB_MAJOR,
SVN_FS_WANT_DB_MINOR,
SVN_FS_WANT_DB_PATCH);
if (major != DB_VERSION_MAJOR || minor != DB_VERSION_MINOR)
return svn_error_createf(SVN_ERR_FS_GENERAL, 0,
_("Bad database version:"
" compiled with %d.%d.%d,"
" running against %d.%d.%d"),
DB_VERSION_MAJOR,
DB_VERSION_MINOR,
DB_VERSION_PATCH,
major, minor, patch);
return SVN_NO_ERROR;
}
static svn_error_t *
cleanup_fs_db(svn_fs_t *fs, DB **db_ptr, const char *name) {
base_fs_data_t *bfd = fs->fsap_data;
if (*db_ptr && !svn_fs_bdb__get_panic(bfd->bdb)) {
DB *db = *db_ptr;
char *msg = apr_psprintf(fs->pool, "closing '%s' database", name);
int db_err;
*db_ptr = 0;
db_err = db->close(db, 0);
if (db_err == DB_RUNRECOVERY) {
svn_fs_bdb__set_panic(bfd->bdb);
db_err = 0;
}
#if SVN_BDB_HAS_DB_INCOMPLETE
if (db_err == DB_INCOMPLETE)
db_err = 0;
#endif
SVN_ERR(BDB_WRAP(fs, msg, db_err));
}
return SVN_NO_ERROR;
}
static svn_error_t *
cleanup_fs(svn_fs_t *fs) {
base_fs_data_t *bfd = fs->fsap_data;
bdb_env_baton_t *bdb = (bfd ? bfd->bdb : NULL);
if (!bdb)
return SVN_NO_ERROR;
SVN_ERR(cleanup_fs_db(fs, &bfd->nodes, "nodes"));
SVN_ERR(cleanup_fs_db(fs, &bfd->revisions, "revisions"));
SVN_ERR(cleanup_fs_db(fs, &bfd->transactions, "transactions"));
SVN_ERR(cleanup_fs_db(fs, &bfd->copies, "copies"));
SVN_ERR(cleanup_fs_db(fs, &bfd->changes, "changes"));
SVN_ERR(cleanup_fs_db(fs, &bfd->representations, "representations"));
SVN_ERR(cleanup_fs_db(fs, &bfd->strings, "strings"));
SVN_ERR(cleanup_fs_db(fs, &bfd->uuids, "uuids"));
SVN_ERR(cleanup_fs_db(fs, &bfd->locks, "locks"));
SVN_ERR(cleanup_fs_db(fs, &bfd->lock_tokens, "lock-tokens"));
SVN_ERR(cleanup_fs_db(fs, &bfd->node_origins, "node-origins"));
bfd->bdb = 0;
{
svn_error_t *err = svn_fs_bdb__close(bdb);
if (err)
return svn_error_createf
(err->apr_err, err,
_("Berkeley DB error for filesystem '%s'"
" while closing environment:\n"),
fs->path);
}
return SVN_NO_ERROR;
}
#if 0
static void print_fs_stats(svn_fs_t *fs) {
base_fs_data_t *bfd = fs->fsap_data;
DB_TXN_STAT *t;
DB_LOCK_STAT *l;
int db_err;
if ((db_err = bfd->bdb->env->txn_stat(bfd->bdb->env, &t, 0)) != 0)
fprintf(stderr, "Error running bfd->bdb->env->txn_stat(): %s",
db_strerror(db_err));
else {
printf("*** DB transaction stats, right before closing env:\n");
printf(" Number of transactions currently active: %d\n",
t->st_nactive);
printf(" Max number of active transactions at any one time: %d\n",
t->st_maxnactive);
printf(" Number of transactions that have begun: %d\n",
t->st_nbegins);
printf(" Number of transactions that have aborted: %d\n",
t->st_naborts);
printf(" Number of transactions that have committed: %d\n",
t->st_ncommits);
printf(" Number of times a thread was forced to wait: %d\n",
t->st_region_wait);
printf(" Number of times a thread didn't need to wait: %d\n",
t->st_region_nowait);
printf("*** End DB transaction stats.\n\n");
}
if ((db_err = bfd->bdb->env->lock_stat(bfd->bdb->env, &l, 0)) != 0)
fprintf(stderr, "Error running bfd->bdb->env->lock_stat(): %s",
db_strerror(db_err));
else {
printf("*** DB lock stats, right before closing env:\n");
printf(" The number of current locks: %d\n",
l->st_nlocks);
printf(" Max number of locks at any one time: %d\n",
l->st_maxnlocks);
printf(" Number of current lockers: %d\n",
l->st_nlockers);
printf(" Max number of lockers at any one time: %d\n",
l->st_maxnlockers);
printf(" Number of current objects: %d\n",
l->st_nobjects);
printf(" Max number of objects at any one time: %d\n",
l->st_maxnobjects);
printf(" Total number of locks requested: %d\n",
l->st_nrequests);
printf(" Total number of locks released: %d\n",
l->st_nreleases);
printf(" Total number of lock reqs failed because "
"DB_LOCK_NOWAIT was set: %d\n", l->st_nnowaits);
printf(" Total number of locks not immediately available "
"due to conflicts: %d\n", l->st_nconflicts);
printf(" Number of deadlocks detected: %d\n", l->st_ndeadlocks);
printf(" Number of times a thread waited before "
"obtaining the region lock: %d\n", l->st_region_wait);
printf(" Number of times a thread didn't have to wait: %d\n",
l->st_region_nowait);
printf("*** End DB lock stats.\n\n");
}
}
#else
#define print_fs_stats(fs)
#endif
static apr_status_t
cleanup_fs_apr(void *data) {
svn_fs_t *fs = data;
svn_error_t *err;
print_fs_stats(fs);
err = cleanup_fs(fs);
if (! err)
return APR_SUCCESS;
(*fs->warning)(fs->warning_baton, err);
svn_error_clear(err);
return SVN_ERR_FS_CLEANUP;
}
static svn_error_t *
base_bdb_set_errcall(svn_fs_t *fs,
void (*db_errcall_fcn)(const char *errpfx, char *msg)) {
base_fs_data_t *bfd = fs->fsap_data;
SVN_ERR(svn_fs__check_fs(fs, TRUE));
bfd->bdb->error_info->user_callback = db_errcall_fcn;
return SVN_NO_ERROR;
}
static svn_error_t *
bdb_write_config(svn_fs_t *fs) {
const char *dbconfig_file_name =
svn_path_join(fs->path, BDB_CONFIG_FILE, fs->pool);
apr_file_t *dbconfig_file = NULL;
int i;
static const char dbconfig_contents[] =
"#This is the configuration file for the Berkeley DB environment\n"
"#used by your Subversion repository.\n"
"#You must run 'svnadmin recover' whenever you modify this file,\n"
"#for your changes to take effect.\n"
"\n"
"###Lock subsystem\n"
"#\n"
"#Make sure you read the documentation at:\n"
"#\n"
"#http://www.oracle.com/technology/documentation/berkeley-db/db/ref/lock/max.html\n"
"#\n"
"#before tweaking these values.\n"
"set_lk_max_locks 2000\n"
"set_lk_max_lockers 2000\n"
"set_lk_max_objects 2000\n"
"\n"
"###Log file subsystem\n"
"#\n"
"#Make sure you read the documentation at:\n"
"#\n"
"#http://www.oracle.com/technology/documentation/berkeley-db/db/api_c/env_set_lg_bsize.html\n"
"#http://www.oracle.com/technology/documentation/berkeley-db/db/api_c/env_set_lg_max.html\n"
"#http://www.oracle.com/technology/documentation/berkeley-db/db/ref/log/limits.html\n"
"#\n"
"#Increase the size of the in-memory log buffer from the default\n"
"#of 32 Kbytes to 256 Kbytes. Decrease the log file size from\n"
"#10 Mbytes to 1 Mbyte. This will help reduce the amount of disk\n"
"#space required for hot backups. The size of the log file must be\n"
"#at least four times the size of the in-memory log buffer.\n"
"#\n"
"#Note: Decreasing the in-memory buffer size below 256 Kbytes\n"
"#will hurt commit performance. For details, see this post from\n"
"#Daniel Berlin <dan@dberlin.org>:\n"
"#\n"
"#http://subversion.tigris.org/servlets/ReadMsg?list=dev&msgId=161960\n"
"set_lg_bsize 262144\n"
"set_lg_max 1048576\n"
"#\n"
"#If you see \"log region out of memory\" errors, bump lg_regionmax.\n"
"#See http://www.oracle.com/technology/documentation/berkeley-db/db/ref/log/config.html\n"
"#and http://svn.haxx.se/users/archive-2004-10/1001.shtml for more.\n"
"set_lg_regionmax 131072\n"
"#\n"
"#The default cache size in BDB is only 256k. As explained in\n"
"#http://svn.haxx.se/dev/archive-2004-12/0369.shtml, this is too\n"
"#small for most applications. Bump this number if \"db_stat -m\"\n"
"#shows too many cache misses.\n"
"set_cachesize 0 1048576 1\n";
static const struct {
int bdb_major;
int bdb_minor;
const char *config_key;
const char *header;
const char *inactive;
const char *active;
} dbconfig_options[] = {
{
4, 0, SVN_FS_CONFIG_BDB_TXN_NOSYNC,
"#\n"
"#Disable fsync of log files on transaction commit. Read the\n"
"#documentation about DB_TXN_NOSYNC at:\n"
"#\n"
"#http://www.oracle.com/technology/documentation/berkeley-db/db/ref/log/config.html\n"
"#\n"
"#[requires Berkeley DB 4.0]\n",
"#set_flags DB_TXN_NOSYNC\n",
"set_flags DB_TXN_NOSYNC\n"
},
{
4, 2, SVN_FS_CONFIG_BDB_LOG_AUTOREMOVE,
"#\n"
"#Enable automatic removal of unused transaction log files.\n"
"#Read the documentation about DB_LOG_AUTOREMOVE at:\n"
"#\n"
"#http://www.oracle.com/technology/documentation/berkeley-db/db/ref/log/config.html\n"
"#\n"
"#[requires Berkeley DB 4.2]\n",
"#set_flags DB_LOG_AUTOREMOVE\n",
"set_flags DB_LOG_AUTOREMOVE\n"
},
};
static const int dbconfig_options_length =
sizeof(dbconfig_options)/sizeof(*dbconfig_options);
SVN_ERR(svn_io_file_open(&dbconfig_file, dbconfig_file_name,
APR_WRITE | APR_CREATE, APR_OS_DEFAULT,
fs->pool));
SVN_ERR(svn_io_file_write_full(dbconfig_file, dbconfig_contents,
sizeof(dbconfig_contents) - 1, NULL,
fs->pool));
for (i = 0; i < dbconfig_options_length; ++i) {
void *value = NULL;
const char *choice;
if (fs->config) {
value = apr_hash_get(fs->config,
dbconfig_options[i].config_key,
APR_HASH_KEY_STRING);
}
SVN_ERR(svn_io_file_write_full(dbconfig_file,
dbconfig_options[i].header,
strlen(dbconfig_options[i].header),
NULL, fs->pool));
if (((DB_VERSION_MAJOR == dbconfig_options[i].bdb_major
&& DB_VERSION_MINOR >= dbconfig_options[i].bdb_minor)
|| DB_VERSION_MAJOR > dbconfig_options[i].bdb_major)
&& value != NULL && strcmp(value, "0") != 0)
choice = dbconfig_options[i].active;
else
choice = dbconfig_options[i].inactive;
SVN_ERR(svn_io_file_write_full(dbconfig_file, choice, strlen(choice),
NULL, fs->pool));
}
SVN_ERR(svn_io_file_close(dbconfig_file, fs->pool));
return SVN_NO_ERROR;
}
static svn_error_t *
base_serialized_init(svn_fs_t *fs, apr_pool_t *common_pool, apr_pool_t *pool) {
return SVN_NO_ERROR;
}
static fs_vtable_t fs_vtable = {
svn_fs_base__youngest_rev,
svn_fs_base__revision_prop,
svn_fs_base__revision_proplist,
svn_fs_base__change_rev_prop,
svn_fs_base__get_uuid,
svn_fs_base__set_uuid,
svn_fs_base__revision_root,
svn_fs_base__begin_txn,
svn_fs_base__open_txn,
svn_fs_base__purge_txn,
svn_fs_base__list_transactions,
svn_fs_base__deltify,
svn_fs_base__lock,
svn_fs_base__generate_lock_token,
svn_fs_base__unlock,
svn_fs_base__get_lock,
svn_fs_base__get_locks,
base_bdb_set_errcall,
};
#define FORMAT_FILE "format"
static svn_error_t *
open_databases(svn_fs_t *fs,
svn_boolean_t create,
int format,
const char *path,
apr_pool_t *pool) {
base_fs_data_t *bfd;
SVN_ERR(svn_fs__check_fs(fs, FALSE));
bfd = apr_pcalloc(fs->pool, sizeof(*bfd));
fs->vtable = &fs_vtable;
fs->fsap_data = bfd;
fs->path = apr_pstrdup(fs->pool, path);
if (create)
SVN_ERR(bdb_write_config(fs));
{
svn_error_t *err = svn_fs_bdb__open(&(bfd->bdb), path,
SVN_BDB_STANDARD_ENV_FLAGS,
0666, fs->pool);
if (err) {
if (create)
return svn_error_createf
(err->apr_err, err,
_("Berkeley DB error for filesystem '%s'"
" while creating environment:\n"),
fs->path);
else
return svn_error_createf
(err->apr_err, err,
_("Berkeley DB error for filesystem '%s'"
" while opening environment:\n"),
fs->path);
}
}
apr_pool_cleanup_register(fs->pool, fs, cleanup_fs_apr,
apr_pool_cleanup_null);
SVN_ERR(BDB_WRAP(fs, (create
? "creating 'nodes' table"
: "opening 'nodes' table"),
svn_fs_bdb__open_nodes_table(&bfd->nodes,
bfd->bdb->env,
create)));
SVN_ERR(BDB_WRAP(fs, (create
? "creating 'revisions' table"
: "opening 'revisions' table"),
svn_fs_bdb__open_revisions_table(&bfd->revisions,
bfd->bdb->env,
create)));
SVN_ERR(BDB_WRAP(fs, (create
? "creating 'transactions' table"
: "opening 'transactions' table"),
svn_fs_bdb__open_transactions_table(&bfd->transactions,
bfd->bdb->env,
create)));
SVN_ERR(BDB_WRAP(fs, (create
? "creating 'copies' table"
: "opening 'copies' table"),
svn_fs_bdb__open_copies_table(&bfd->copies,
bfd->bdb->env,
create)));
SVN_ERR(BDB_WRAP(fs, (create
? "creating 'changes' table"
: "opening 'changes' table"),
svn_fs_bdb__open_changes_table(&bfd->changes,
bfd->bdb->env,
create)));
SVN_ERR(BDB_WRAP(fs, (create
? "creating 'representations' table"
: "opening 'representations' table"),
svn_fs_bdb__open_reps_table(&bfd->representations,
bfd->bdb->env,
create)));
SVN_ERR(BDB_WRAP(fs, (create
? "creating 'strings' table"
: "opening 'strings' table"),
svn_fs_bdb__open_strings_table(&bfd->strings,
bfd->bdb->env,
create)));
SVN_ERR(BDB_WRAP(fs, (create
? "creating 'uuids' table"
: "opening 'uuids' table"),
svn_fs_bdb__open_uuids_table(&bfd->uuids,
bfd->bdb->env,
create)));
SVN_ERR(BDB_WRAP(fs, (create
? "creating 'locks' table"
: "opening 'locks' table"),
svn_fs_bdb__open_locks_table(&bfd->locks,
bfd->bdb->env,
create)));
SVN_ERR(BDB_WRAP(fs, (create
? "creating 'lock-tokens' table"
: "opening 'lock-tokens' table"),
svn_fs_bdb__open_lock_tokens_table(&bfd->lock_tokens,
bfd->bdb->env,
create)));
if (format >= SVN_FS_BASE__MIN_NODE_ORIGINS_FORMAT) {
SVN_ERR(BDB_WRAP(fs, (create
? "creating 'node-origins' table"
: "opening 'node-origins' table"),
svn_fs_bdb__open_node_origins_table(&bfd->node_origins,
bfd->bdb->env,
create)));
}
return SVN_NO_ERROR;
}
static svn_error_t *
base_create(svn_fs_t *fs, const char *path, apr_pool_t *pool,
apr_pool_t *common_pool) {
int format = SVN_FS_BASE__FORMAT_NUMBER;
svn_error_t *svn_err;
if (fs->config && apr_hash_get(fs->config, SVN_FS_CONFIG_PRE_1_5_COMPATIBLE,
APR_HASH_KEY_STRING))
format = 2;
if (fs->config && apr_hash_get(fs->config, SVN_FS_CONFIG_PRE_1_4_COMPATIBLE,
APR_HASH_KEY_STRING))
format = 1;
svn_err = open_databases(fs, TRUE, format, path, pool);
if (svn_err) goto error;
svn_err = svn_fs_base__dag_init_fs(fs);
if (svn_err) goto error;
svn_err = svn_io_write_version_file
(svn_path_join(fs->path, FORMAT_FILE, pool), format, pool);
if (svn_err) goto error;
((base_fs_data_t *) fs->fsap_data)->format = format;
return base_serialized_init(fs, common_pool, pool);
error:
svn_error_clear(cleanup_fs(fs));
return svn_err;
}
svn_error_t *
svn_fs_base__test_required_feature_format(svn_fs_t *fs,
const char *feature,
int requires) {
base_fs_data_t *bfd = fs->fsap_data;
if (bfd->format < requires)
return svn_error_createf
(SVN_ERR_UNSUPPORTED_FEATURE, NULL,
_("The '%s' feature requires version %d of the filesystem schema; "
"filesystem '%s' uses only version %d"),
feature, requires, fs->path, bfd->format);
return SVN_NO_ERROR;
}
static svn_error_t *
check_format(int format) {
if (format == 1 && SVN_FS_BASE__FORMAT_NUMBER == 2)
return SVN_NO_ERROR;
if ((format == 1 || format == 2) && SVN_FS_BASE__FORMAT_NUMBER == 3)
return SVN_NO_ERROR;
if (format != SVN_FS_BASE__FORMAT_NUMBER) {
return svn_error_createf
(SVN_ERR_FS_UNSUPPORTED_FORMAT, NULL,
_("Expected FS format '%d'; found format '%d'"),
SVN_FS_BASE__FORMAT_NUMBER, format);
}
return SVN_NO_ERROR;
}
static svn_error_t *
base_open(svn_fs_t *fs, const char *path, apr_pool_t *pool,
apr_pool_t *common_pool) {
int format;
svn_error_t *svn_err;
svn_boolean_t write_format_file = FALSE;
svn_err = svn_io_read_version_file(&format,
svn_path_join(path, FORMAT_FILE, pool),
pool);
if (svn_err && APR_STATUS_IS_ENOENT(svn_err->apr_err)) {
svn_error_clear(svn_err);
svn_err = SVN_NO_ERROR;
format = SVN_FS_BASE__FORMAT_NUMBER;
write_format_file = TRUE;
} else if (svn_err)
goto error;
svn_err = open_databases(fs, FALSE, format, path, pool);
if (svn_err) goto error;
((base_fs_data_t *) fs->fsap_data)->format = format;
SVN_ERR(check_format(format));
if (write_format_file) {
svn_err = svn_io_write_version_file(svn_path_join(path, FORMAT_FILE,
pool), format, pool);
if (svn_err) goto error;
}
return base_serialized_init(fs, common_pool, pool);
error:
svn_error_clear(cleanup_fs(fs));
return svn_err;
}
static svn_error_t *
bdb_recover(const char *path, svn_boolean_t fatal, apr_pool_t *pool) {
bdb_env_baton_t *bdb;
SVN_ERR(svn_fs_bdb__open(&bdb, path,
((fatal ? DB_RECOVER_FATAL : DB_RECOVER)
| SVN_BDB_PRIVATE_ENV_FLAGS),
0666, pool));
SVN_ERR(svn_fs_bdb__close(bdb));
return SVN_NO_ERROR;
}
static svn_error_t *
base_open_for_recovery(svn_fs_t *fs, const char *path, apr_pool_t *pool,
apr_pool_t *common_pool) {
fs->path = apr_pstrdup(fs->pool, path);
return SVN_NO_ERROR;
}
static svn_error_t *
base_upgrade(svn_fs_t *fs, const char *path, apr_pool_t *pool,
apr_pool_t *common_pool) {
return svn_io_write_version_file(svn_path_join(path, FORMAT_FILE, pool),
SVN_FS_BASE__FORMAT_NUMBER, pool);
}
static svn_error_t *
base_bdb_recover(svn_fs_t *fs,
svn_cancel_func_t cancel_func, void *cancel_baton,
apr_pool_t *pool) {
return bdb_recover(fs->path, FALSE, pool);
}
static svn_error_t *
base_bdb_logfiles(apr_array_header_t **logfiles,
const char *path,
svn_boolean_t only_unused,
apr_pool_t *pool) {
bdb_env_baton_t *bdb;
char **filelist;
char **filename;
u_int32_t flags = only_unused ? 0 : DB_ARCH_LOG;
*logfiles = apr_array_make(pool, 4, sizeof(const char *));
SVN_ERR(svn_fs_bdb__open(&bdb, path,
SVN_BDB_STANDARD_ENV_FLAGS,
0666, pool));
SVN_BDB_ERR(bdb, bdb->env->log_archive(bdb->env, &filelist, flags));
if (filelist == NULL)
return svn_fs_bdb__close(bdb);
for (filename = filelist; *filename != NULL; ++filename) {
APR_ARRAY_PUSH(*logfiles, const char *) = apr_pstrdup(pool, *filename);
}
free(filelist);
return svn_fs_bdb__close(bdb);
}
static svn_error_t *
svn_fs_base__clean_logs(const char *live_path,
const char *backup_path,
apr_pool_t *pool) {
apr_array_header_t *logfiles;
SVN_ERR(base_bdb_logfiles(&logfiles,
live_path,
TRUE,
pool));
{
int idx;
apr_pool_t *sub_pool = svn_pool_create(pool);
for (idx = 0; idx < logfiles->nelts; idx++) {
const char *log_file = APR_ARRAY_IDX(logfiles, idx, const char *);
const char *live_log_path;
const char *backup_log_path;
svn_pool_clear(sub_pool);
live_log_path = svn_path_join(live_path, log_file, sub_pool);
backup_log_path = svn_path_join(backup_path, log_file, sub_pool);
{
svn_boolean_t files_match = FALSE;
svn_node_kind_t kind;
SVN_ERR(svn_io_check_path(backup_log_path, &kind, pool));
if (kind == svn_node_file)
SVN_ERR(svn_io_files_contents_same_p(&files_match,
live_log_path,
backup_log_path,
sub_pool));
if (files_match == FALSE)
continue;
}
SVN_ERR(svn_io_remove_file(live_log_path, sub_pool));
}
svn_pool_destroy(sub_pool);
}
return SVN_NO_ERROR;
}
#if SVN_BDB_VERSION_AT_LEAST(4, 2)
static svn_error_t *
check_env_flags(svn_boolean_t *match,
u_int32_t flags,
const char *path,
apr_pool_t *pool) {
bdb_env_baton_t *bdb;
#if SVN_BDB_VERSION_AT_LEAST(4, 7)
int flag_state;
#else
u_int32_t envflags;
#endif
SVN_ERR(svn_fs_bdb__open(&bdb, path,
SVN_BDB_STANDARD_ENV_FLAGS,
0666, pool));
#if SVN_BDB_VERSION_AT_LEAST(4, 7)
SVN_BDB_ERR(bdb, bdb->env->log_get_config(bdb->env, flags, &flag_state));
#else
SVN_BDB_ERR(bdb, bdb->env->get_flags(bdb->env, &envflags));
#endif
SVN_ERR(svn_fs_bdb__close(bdb));
#if SVN_BDB_VERSION_AT_LEAST(4, 7)
if (flag_state == 0)
#else
if (flags & envflags)
#endif
*match = TRUE;
else
*match = FALSE;
return SVN_NO_ERROR;
}
static svn_error_t *
get_db_pagesize(u_int32_t *pagesize,
const char *path,
apr_pool_t *pool) {
bdb_env_baton_t *bdb;
DB *nodes_table;
SVN_ERR(svn_fs_bdb__open(&bdb, path,
SVN_BDB_STANDARD_ENV_FLAGS,
0666, pool));
SVN_BDB_ERR(bdb, svn_fs_bdb__open_nodes_table(&nodes_table, bdb->env,
FALSE));
SVN_BDB_ERR(bdb, nodes_table->get_pagesize(nodes_table, pagesize));
SVN_BDB_ERR(bdb, nodes_table->close(nodes_table, 0));
return svn_fs_bdb__close(bdb);
}
#endif
static svn_error_t *
copy_db_file_safely(const char *src_dir,
const char *dst_dir,
const char *filename,
u_int32_t chunksize,
svn_boolean_t allow_missing,
apr_pool_t *pool) {
apr_file_t *s = NULL, *d = NULL;
const char *file_src_path = svn_path_join(src_dir, filename, pool);
const char *file_dst_path = svn_path_join(dst_dir, filename, pool);
svn_error_t *err;
char *buf;
err = svn_io_file_open(&s, file_src_path,
(APR_READ | APR_LARGEFILE | APR_BINARY),
APR_OS_DEFAULT, pool);
if (err && APR_STATUS_IS_ENOENT(err->apr_err) && allow_missing) {
svn_error_clear(err);
return SVN_NO_ERROR;
}
SVN_ERR(err);
SVN_ERR(svn_io_file_open(&d, file_dst_path, (APR_WRITE | APR_CREATE |
APR_LARGEFILE | APR_BINARY),
APR_OS_DEFAULT, pool));
buf = apr_palloc(pool, chunksize);
while (1) {
apr_size_t bytes_this_time = chunksize;
svn_error_t *read_err, *write_err;
if ((read_err = svn_io_file_read(s, buf, &bytes_this_time, pool))) {
if (APR_STATUS_IS_EOF(read_err->apr_err))
svn_error_clear(read_err);
else {
svn_error_clear(svn_io_file_close(s, pool));
svn_error_clear(svn_io_file_close(d, pool));
return read_err;
}
}
if ((write_err = svn_io_file_write_full(d, buf, bytes_this_time, NULL,
pool))) {
svn_error_clear(svn_io_file_close(s, pool));
svn_error_clear(svn_io_file_close(d, pool));
return write_err;
}
if (read_err) {
SVN_ERR(svn_io_file_close(s, pool));
SVN_ERR(svn_io_file_close(d, pool));
break;
}
}
return SVN_NO_ERROR;
}
static svn_error_t *
base_hotcopy(const char *src_path,
const char *dest_path,
svn_boolean_t clean_logs,
apr_pool_t *pool) {
svn_error_t *err;
u_int32_t pagesize;
svn_boolean_t log_autoremove = FALSE;
int format;
SVN_ERR(svn_io_read_version_file
(&format, svn_path_join(src_path, FORMAT_FILE, pool), pool));
SVN_ERR(check_format(format));
#if SVN_BDB_VERSION_AT_LEAST(4, 2)
err = check_env_flags(&log_autoremove,
#if SVN_BDB_VERSION_AT_LEAST(4, 7)
DB_LOG_AUTO_REMOVE,
#else
DB_LOG_AUTOREMOVE,
#endif
src_path, pool);
#endif
SVN_ERR(err);
SVN_ERR(svn_io_dir_file_copy(src_path, dest_path, "DB_CONFIG", pool));
#if SVN_BDB_VERSION_AT_LEAST(4, 2)
SVN_ERR(get_db_pagesize(&pagesize, src_path, pool));
if (pagesize < SVN__STREAM_CHUNK_SIZE) {
int multiple = SVN__STREAM_CHUNK_SIZE / pagesize;
pagesize *= multiple;
}
#else
pagesize = (4096 * 32);
#endif
SVN_ERR(copy_db_file_safely(src_path, dest_path,
"nodes", pagesize, FALSE, pool));
SVN_ERR(copy_db_file_safely(src_path, dest_path,
"transactions", pagesize, FALSE, pool));
SVN_ERR(copy_db_file_safely(src_path, dest_path,
"revisions", pagesize, FALSE, pool));
SVN_ERR(copy_db_file_safely(src_path, dest_path,
"copies", pagesize, FALSE, pool));
SVN_ERR(copy_db_file_safely(src_path, dest_path,
"changes", pagesize, FALSE, pool));
SVN_ERR(copy_db_file_safely(src_path, dest_path,
"representations", pagesize, FALSE, pool));
SVN_ERR(copy_db_file_safely(src_path, dest_path,
"strings", pagesize, FALSE, pool));
SVN_ERR(copy_db_file_safely(src_path, dest_path,
"uuids", pagesize, TRUE, pool));
SVN_ERR(copy_db_file_safely(src_path, dest_path,
"locks", pagesize, TRUE, pool));
SVN_ERR(copy_db_file_safely(src_path, dest_path,
"lock-tokens", pagesize, TRUE, pool));
SVN_ERR(copy_db_file_safely(src_path, dest_path,
"node-origins", pagesize, TRUE, pool));
{
apr_array_header_t *logfiles;
int idx;
apr_pool_t *subpool;
SVN_ERR(base_bdb_logfiles(&logfiles,
src_path,
FALSE,
pool));
subpool = svn_pool_create(pool);
for (idx = 0; idx < logfiles->nelts; idx++) {
svn_pool_clear(subpool);
err = svn_io_dir_file_copy(src_path, dest_path,
APR_ARRAY_IDX(logfiles, idx,
const char *),
subpool);
if (err) {
if (log_autoremove)
return
svn_error_quick_wrap
(err,
_("Error copying logfile; the DB_LOG_AUTOREMOVE feature\n"
"may be interfering with the hotcopy algorithm. If\n"
"the problem persists, try deactivating this feature\n"
"in DB_CONFIG"));
else
return err;
}
}
svn_pool_destroy(subpool);
}
err = bdb_recover(dest_path, TRUE, pool);
if (err) {
if (log_autoremove)
return
svn_error_quick_wrap
(err,
_("Error running catastrophic recovery on hotcopy; the\n"
"DB_LOG_AUTOREMOVE feature may be interfering with the\n"
"hotcopy algorithm. If the problem persists, try deactivating\n"
"this feature in DB_CONFIG"));
else
return err;
}
SVN_ERR(svn_io_write_version_file
(svn_path_join(dest_path, FORMAT_FILE, pool), format, pool));
if (clean_logs == TRUE)
SVN_ERR(svn_fs_base__clean_logs(src_path, dest_path, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
base_delete_fs(const char *path,
apr_pool_t *pool) {
SVN_ERR(svn_fs_bdb__remove(path, pool));
SVN_ERR(svn_io_remove_dir2(path, FALSE, NULL, NULL, pool));
return SVN_NO_ERROR;
}
static const svn_version_t *
base_version(void) {
SVN_VERSION_BODY;
}
static const char *
base_get_description(void) {
return _("Module for working with a Berkeley DB repository.");
}
static fs_library_vtable_t library_vtable = {
base_version,
base_create,
base_open,
base_open_for_recovery,
base_upgrade,
base_delete_fs,
base_hotcopy,
base_get_description,
base_bdb_recover,
base_bdb_logfiles,
svn_fs_base__id_parse
};
svn_error_t *
svn_fs_base__init(const svn_version_t *loader_version,
fs_library_vtable_t **vtable, apr_pool_t* common_pool) {
static const svn_version_checklist_t checklist[] = {
{ "svn_subr", svn_subr_version },
{ "svn_delta", svn_delta_version },
{ NULL, NULL }
};
if (loader_version->major != SVN_VER_MAJOR)
return svn_error_createf(SVN_ERR_VERSION_MISMATCH, NULL,
_("Unsupported FS loader version (%d) for bdb"),
loader_version->major);
SVN_ERR(svn_ver_check_list(base_version(), checklist));
SVN_ERR(check_bdb_version());
SVN_ERR(svn_fs_bdb__init(common_pool));
*vtable = &library_vtable;
return SVN_NO_ERROR;
}
