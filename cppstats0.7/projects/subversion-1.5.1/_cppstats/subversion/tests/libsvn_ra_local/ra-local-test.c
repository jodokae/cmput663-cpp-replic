#include <apr_general.h>
#include <apr_pools.h>
#if defined(_MSC_VER)
#include <direct.h>
#define getcwd _getcwd
#else
#include <unistd.h>
#endif
#include "svn_string.h"
#include "svn_utf.h"
#include "svn_error.h"
#include "svn_delta.h"
#include "svn_ra.h"
#include "svn_client.h"
#include "../svn_test.h"
#include "../svn_test_fs.h"
#include "../../libsvn_ra_local/ra_local.h"
static svn_error_t *
current_directory_url(const char **url,
const char *suffix,
apr_pool_t *pool) {
char curdir[8192];
const char *utf8_ls_curdir, *utf8_is_curdir, *unencoded_url;
if (! getcwd(curdir, sizeof(curdir)))
return svn_error_create(SVN_ERR_BASE, NULL, "getcwd() failed");
#if !defined(AS400_UTF8)
SVN_ERR(svn_utf_cstring_to_utf8(&utf8_ls_curdir, curdir, pool));
#else
SVN_ERR (svn_utf_cstring_to_utf8_ex2(&utf8_ls_curdir, curdir,
(const char *)0, pool));
#endif
utf8_is_curdir = svn_path_internal_style(utf8_ls_curdir, pool);
unencoded_url = apr_psprintf(pool, "file://%s%s%s%s",
(utf8_is_curdir[0] != '/') ? "/" : "",
utf8_is_curdir,
(suffix[0] && suffix[0] != '/') ? "/" : "",
suffix);
*url = svn_path_uri_encode(unencoded_url, pool);
return SVN_NO_ERROR;
}
static svn_error_t *
make_and_open_local_repos(svn_ra_session_t **session,
const char *repos_name,
const char *fs_type,
apr_pool_t *pool) {
svn_repos_t *repos;
const char *url;
svn_ra_callbacks2_t *cbtable;
SVN_ERR(svn_ra_create_callbacks(&cbtable, pool));
SVN_ERR(svn_test__create_repos(&repos, repos_name, fs_type, pool));
SVN_ERR(svn_ra_initialize(pool));
SVN_ERR(current_directory_url(&url, repos_name, pool));
SVN_ERR(svn_ra_open3(session,
url,
NULL,
cbtable,
NULL,
NULL,
pool));
return SVN_NO_ERROR;
}
static svn_error_t *
open_ra_session(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
svn_ra_session_t *session;
*msg = "open an ra session to a local repository";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(make_and_open_local_repos(&session,
"test-repo-open", opts->fs_type, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
get_youngest_rev(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
svn_ra_session_t *session;
svn_revnum_t latest_rev;
*msg = "get the youngest revision in a repository";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(make_and_open_local_repos(&session,
"test-repo-getrev", opts->fs_type,
pool));
SVN_ERR(svn_ra_get_latest_revnum(session, &latest_rev, pool));
if (latest_rev != 0)
return svn_error_create(SVN_ERR_FS_GENERAL, NULL,
"youngest rev isn't 0!");
return SVN_NO_ERROR;
}
static apr_status_t
try_split_url(const char *url, apr_pool_t *pool) {
svn_repos_t *repos;
const char *repos_path, *fs_path;
svn_error_t *err;
apr_status_t apr_err;
err = svn_ra_local__split_URL(&repos, &repos_path, &fs_path, url, pool);
if (! err)
return SVN_NO_ERROR;
apr_err = err->apr_err;
svn_error_clear(err);
return apr_err;
}
static svn_error_t *
split_url_syntax(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
apr_status_t apr_err;
*msg = "svn_ra_local__split_URL: syntax validation";
if (msg_only)
return SVN_NO_ERROR;
apr_err = try_split_url("blah:///bin/svn", pool);
if (apr_err != SVN_ERR_RA_ILLEGAL_URL)
return svn_error_create
(SVN_ERR_TEST_FAILED, NULL,
"svn_ra_local__split_URL failed to catch bad URL (scheme)");
apr_err = try_split_url("file:/path/to/repos", pool);
if (apr_err != SVN_ERR_RA_ILLEGAL_URL)
return svn_error_create
(SVN_ERR_TEST_FAILED, NULL,
"svn_ra_local__split_URL failed to catch bad URL (slashes)");
apr_err = try_split_url("file://hostname", pool);
if (apr_err != SVN_ERR_RA_ILLEGAL_URL)
return svn_error_create
(SVN_ERR_TEST_FAILED, NULL,
"svn_ra_local__split_URL failed to catch bad URL (no path)");
return SVN_NO_ERROR;
}
static svn_error_t *
split_url_bad_host(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
apr_status_t apr_err;
*msg = "svn_ra_local__split_URL: invalid host names";
if (msg_only)
return SVN_NO_ERROR;
apr_err = try_split_url("file://myhost/repos/path", pool);
if (apr_err != SVN_ERR_RA_ILLEGAL_URL)
return svn_error_create
(SVN_ERR_TEST_FAILED, NULL,
"svn_ra_local__split_URL failed to catch bad URL (hostname)");
return SVN_NO_ERROR;
}
static svn_error_t *
split_url_host(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
apr_status_t apr_err;
*msg = "svn_ra_local__split_URL: valid host names";
if (msg_only)
return SVN_NO_ERROR;
apr_err = try_split_url("file:///repos/path", pool);
if (apr_err == SVN_ERR_RA_ILLEGAL_URL)
return svn_error_create
(SVN_ERR_TEST_FAILED, NULL,
"svn_ra_local__split_URL cried foul about a good URL (no hostname)");
apr_err = try_split_url("file://localhost/repos/path", pool);
if (apr_err == SVN_ERR_RA_ILLEGAL_URL)
return svn_error_create
(SVN_ERR_TEST_FAILED, NULL,
"svn_ra_local__split_URL cried foul about a good URL (localhost)");
return SVN_NO_ERROR;
}
static svn_error_t *
check_split_url(const char *repos_path,
const char *in_repos_path,
const char *fs_type,
apr_pool_t *pool) {
svn_repos_t *repos;
const char *url, *root_url, *repos_part, *in_repos_part;
SVN_ERR(svn_test__create_repos(&repos, repos_path, fs_type, pool));
SVN_ERR(current_directory_url(&root_url, repos_path, pool));
if (in_repos_path)
url = apr_pstrcat(pool, root_url, in_repos_path, NULL);
else
url = root_url;
SVN_ERR(svn_ra_local__split_URL(&repos, &repos_part, &in_repos_part,
url, pool));
if ((strcmp(repos_part, root_url) == 0)
&& ((in_repos_path && (strcmp(in_repos_part, in_repos_path) == 0))
|| ((! in_repos_path) && (strcmp(in_repos_part, "/") == 0))))
return SVN_NO_ERROR;
return svn_error_createf
(SVN_ERR_TEST_FAILED, NULL,
"svn_ra_local__split_URL failed to properly split the URL\n"
"%s\n%s\n%s\n%s",
repos_part, root_url, in_repos_part,
in_repos_path ? in_repos_path : "(null)");
}
static svn_error_t *
split_url_test(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
*msg = "test svn_ra_local__split_URL correctness";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(check_split_url("test-repo-split-fs1",
"/trunk/foobar/quux.c",
opts->fs_type,
pool));
SVN_ERR(check_split_url("test-repo-split-fs2",
"/alpha/beta/gamma/delta/epsilon/zeta/eta/theta",
opts->fs_type,
pool));
SVN_ERR(check_split_url("test-repo-split-fs3",
NULL,
opts->fs_type,
pool));
return SVN_NO_ERROR;
}
#if defined(WIN32) || defined(__CYGWIN__)
#define HAS_UNC_HOST 1
#else
#define HAS_UNC_HOST 0
#endif
struct svn_test_descriptor_t test_funcs[] = {
SVN_TEST_NULL,
SVN_TEST_PASS(open_ra_session),
SVN_TEST_PASS(get_youngest_rev),
SVN_TEST_PASS(split_url_syntax),
SVN_TEST_SKIP(split_url_bad_host, HAS_UNC_HOST),
SVN_TEST_PASS(split_url_host),
SVN_TEST_PASS(split_url_test),
SVN_TEST_NULL
};