#include <errno.h>
#include <apr_md5.h>
#include <httpd.h>
#include <mod_dav.h>
#include "svn_error.h"
#include "svn_io.h"
#include "svn_md5.h"
#include "svn_path.h"
#include "svn_fs.h"
#include "svn_repos.h"
#include "private/svn_fs_private.h"
#include "dav_svn.h"
static const char *
escape_activity(const char *activity_id, apr_pool_t *pool) {
unsigned char digest[APR_MD5_DIGESTSIZE];
apr_md5(digest, activity_id, strlen(activity_id));
return svn_md5_digest_to_cstring_display(digest, pool);
}
static const char *
activity_pathname(const dav_svn_repos *repos, const char *activity_id) {
return svn_path_join(repos->activities_db,
escape_activity(activity_id, repos->pool),
repos->pool);
}
static const char *
read_txn(const char *pathname, apr_pool_t *pool) {
apr_file_t *activity_file;
apr_pool_t *iterpool = svn_pool_create(pool);
apr_size_t len;
svn_error_t *err = SVN_NO_ERROR;
char *txn_name = apr_palloc(pool, SVN_FS__TXN_MAX_LEN+1);
int i;
for (i = 0; i < 10; i++) {
svn_error_clear(err);
svn_pool_clear(iterpool);
err = svn_io_file_open(&activity_file, pathname,
APR_READ | APR_BUFFERED,
APR_OS_DEFAULT, iterpool);
if (err) {
#if defined(ESTALE)
if (APR_TO_OS_ERROR(err->apr_err) == ESTALE)
continue;
#endif
break;
}
len = SVN_FS__TXN_MAX_LEN;
err = svn_io_read_length_line(activity_file, txn_name, &len, iterpool);
if (err) {
#if defined(ESTALE)
if (APR_TO_OS_ERROR(err->apr_err) == ESTALE)
continue;
#endif
break;
}
err = svn_io_file_close(activity_file, iterpool);
#if defined(ESTALE)
if (err) {
if (APR_TO_OS_ERROR(err->apr_err) == ESTALE) {
svn_error_clear(err);
err = SVN_NO_ERROR;
}
}
#endif
break;
}
svn_pool_destroy(iterpool);
if (err) {
svn_error_clear(err);
return NULL;
}
return txn_name;
}
const char *
dav_svn__get_txn(const dav_svn_repos *repos, const char *activity_id) {
return read_txn(activity_pathname(repos, activity_id), repos->pool);
}
dav_error *
dav_svn__delete_activity(const dav_svn_repos *repos, const char *activity_id) {
dav_error *err = NULL;
const char *pathname;
svn_fs_txn_t *txn;
const char *txn_name;
svn_error_t *serr;
pathname = activity_pathname(repos, activity_id);
txn_name = read_txn(pathname, repos->pool);
if (txn_name == NULL) {
return dav_new_error(repos->pool, HTTP_NOT_FOUND, 0,
"could not find activity.");
}
if (*txn_name) {
if ((serr = svn_fs_open_txn(&txn, repos->fs, txn_name, repos->pool))) {
if (serr->apr_err == SVN_ERR_FS_NO_SUCH_TRANSACTION) {
svn_error_clear(serr);
serr = SVN_NO_ERROR;
} else {
err = dav_svn__convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
"could not open transaction.",
repos->pool);
return err;
}
} else {
serr = svn_fs_abort_txn(txn, repos->pool);
if (serr) {
err = dav_svn__convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
"could not abort transaction.",
repos->pool);
return err;
}
}
}
serr = svn_io_remove_file(pathname, repos->pool);
if (serr)
err = dav_svn__convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
"unable to remove activity.",
repos->pool);
return err;
}
dav_error *
dav_svn__store_activity(const dav_svn_repos *repos,
const char *activity_id,
const char *txn_name) {
const char *final_path, *tmp_path, *activity_contents;
svn_error_t *err;
apr_file_t *activity_file;
err = svn_io_make_dir_recursively(repos->activities_db, repos->pool);
if (err != NULL)
return dav_svn__convert_err(err, HTTP_INTERNAL_SERVER_ERROR,
"could not initialize activity db.",
repos->pool);
final_path = activity_pathname(repos, activity_id);
err = svn_io_open_unique_file2(&activity_file, &tmp_path, final_path,
".tmp", svn_io_file_del_none, repos->pool);
if (err) {
svn_error_t *serr = svn_error_quick_wrap(err, "Can't open activity db");
return dav_svn__convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
"could not open files.",
repos->pool);
}
activity_contents = apr_psprintf(repos->pool, "%s\n%s\n",
txn_name, activity_id);
err = svn_io_file_write_full(activity_file, activity_contents,
strlen(activity_contents), NULL, repos->pool);
if (err) {
svn_error_t *serr = svn_error_quick_wrap(err,
"Can't write to activity db");
svn_error_clear(svn_io_file_close(activity_file, repos->pool));
svn_error_clear(svn_io_remove_file(tmp_path, repos->pool));
return dav_svn__convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
"could not write files.",
repos->pool);
}
err = svn_io_file_close(activity_file, repos->pool);
if (err) {
svn_error_clear(svn_io_remove_file(tmp_path, repos->pool));
return dav_svn__convert_err(err, HTTP_INTERNAL_SERVER_ERROR,
"could not close files.",
repos->pool);
}
err = svn_io_file_rename(tmp_path, final_path, repos->pool);
if (err) {
svn_error_clear(svn_io_remove_file(tmp_path, repos->pool));
return dav_svn__convert_err(err, HTTP_INTERNAL_SERVER_ERROR,
"could not replace files.",
repos->pool);
}
return NULL;
}
dav_error *
dav_svn__create_activity(const dav_svn_repos *repos,
const char **ptxn_name,
apr_pool_t *pool) {
svn_revnum_t rev;
svn_fs_txn_t *txn;
svn_error_t *serr;
serr = svn_fs_youngest_rev(&rev, repos->fs, pool);
if (serr != NULL) {
return dav_svn__convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
"could not determine youngest revision",
repos->pool);
}
serr = svn_repos_fs_begin_txn_for_commit(&txn, repos->repos, rev,
repos->username, NULL,
repos->pool);
if (serr != NULL) {
return dav_svn__convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
"could not begin a transaction",
repos->pool);
}
serr = svn_fs_txn_name(ptxn_name, txn, pool);
if (serr != NULL) {
return dav_svn__convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
"could not fetch transaction name",
repos->pool);
}
return NULL;
}