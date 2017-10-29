#include <apr_md5.h>
#include "svn_path.h"
#include "svn_md5.h"
#include "svn_hash.h"
#include "svn_io.h"
#include "config_impl.h"
#include "svn_private_config.h"
static svn_error_t *
auth_file_path(const char **path,
const char *cred_kind,
const char *realmstring,
const char *config_dir,
apr_pool_t *pool) {
const char *authdir_path, *hexname;
unsigned char digest[APR_MD5_DIGESTSIZE];
SVN_ERR(svn_config__user_config_path(config_dir, &authdir_path,
SVN_CONFIG__AUTH_SUBDIR, pool));
if (authdir_path) {
authdir_path = svn_path_join(authdir_path, cred_kind, pool);
apr_md5(digest, realmstring, strlen(realmstring));
hexname = svn_md5_digest_to_cstring(digest, pool);
*path = svn_path_join(authdir_path, hexname, pool);
} else
*path = NULL;
return SVN_NO_ERROR;
}
svn_error_t *
svn_config_read_auth_data(apr_hash_t **hash,
const char *cred_kind,
const char *realmstring,
const char *config_dir,
apr_pool_t *pool) {
svn_node_kind_t kind;
const char *auth_path;
*hash = NULL;
SVN_ERR(auth_file_path(&auth_path, cred_kind, realmstring, config_dir,
pool));
if (! auth_path)
return SVN_NO_ERROR;
SVN_ERR(svn_io_check_path(auth_path, &kind, pool));
if (kind == svn_node_file) {
apr_file_t *authfile = NULL;
SVN_ERR_W(svn_io_file_open(&authfile, auth_path,
APR_READ | APR_BUFFERED, APR_OS_DEFAULT,
pool),
_("Unable to open auth file for reading"));
*hash = apr_hash_make(pool);
SVN_ERR_W(svn_hash_read(*hash, authfile, pool),
apr_psprintf(pool, _("Error parsing '%s'"),
svn_path_local_style(auth_path, pool)));
SVN_ERR(svn_io_file_close(authfile, pool));
}
return SVN_NO_ERROR;
}
svn_error_t *
svn_config_write_auth_data(apr_hash_t *hash,
const char *cred_kind,
const char *realmstring,
const char *config_dir,
apr_pool_t *pool) {
apr_file_t *authfile = NULL;
const char *auth_path;
SVN_ERR(auth_file_path(&auth_path, cred_kind, realmstring, config_dir,
pool));
if (! auth_path)
return svn_error_create(SVN_ERR_NO_AUTH_FILE_PATH, NULL,
_("Unable to locate auth file"));
apr_hash_set(hash, SVN_CONFIG_REALMSTRING_KEY, APR_HASH_KEY_STRING,
svn_string_create(realmstring, pool));
SVN_ERR_W(svn_io_file_open(&authfile, auth_path,
(APR_WRITE | APR_CREATE | APR_TRUNCATE
| APR_BUFFERED),
APR_OS_DEFAULT, pool),
_("Unable to open auth file for writing"));
SVN_ERR_W(svn_hash_write(hash, authfile, pool),
apr_psprintf(pool, _("Error writing hash to '%s'"),
svn_path_local_style(auth_path, pool)));
SVN_ERR(svn_io_file_close(authfile, pool));
apr_hash_set(hash, SVN_CONFIG_REALMSTRING_KEY, APR_HASH_KEY_STRING, NULL);
return SVN_NO_ERROR;
}
