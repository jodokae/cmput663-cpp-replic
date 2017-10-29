#include "ra_local.h"
#include <assert.h>
#include <string.h>
#include "svn_path.h"
#include "svn_private_config.h"
svn_error_t *
svn_ra_local__split_URL(svn_repos_t **repos,
const char **repos_url,
const char **fs_path,
const char *URL,
apr_pool_t *pool) {
svn_error_t *err = SVN_NO_ERROR;
const char *repos_root;
const char *hostname, *path;
svn_stringbuf_t *urlbuf;
if (strncmp(URL, "file://", 7) != 0)
return svn_error_createf
(SVN_ERR_RA_ILLEGAL_URL, NULL,
_("Local URL '%s' does not contain 'file://' prefix"), URL);
hostname = URL + 7;
path = strchr(hostname, '/');
if (! path)
return svn_error_createf
(SVN_ERR_RA_ILLEGAL_URL, NULL,
_("Local URL '%s' contains only a hostname, no path"), URL);
if (hostname != path) {
hostname = svn_path_uri_decode(apr_pstrmemdup(pool, hostname,
path - hostname), pool);
if (strncmp(hostname, "localhost", 9) == 0)
hostname = NULL;
} else
hostname = NULL;
#if defined(WIN32) || defined(__CYGWIN__)
{
static const char valid_drive_letters[] =
"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
char *dup_path = (char *)svn_path_uri_decode(path, pool);
if (!hostname && dup_path[1] && strchr(valid_drive_letters, dup_path[1])
&& (dup_path[2] == ':' || dup_path[2] == '|')
&& dup_path[3] == '/') {
++dup_path;
++path;
if (dup_path[1] == '|')
dup_path[1] = ':';
}
if (hostname)
repos_root = apr_pstrcat(pool, "//", hostname, path, NULL);
else
repos_root = dup_path;
}
#else
if (hostname)
return svn_error_createf
(SVN_ERR_RA_ILLEGAL_URL, NULL,
_("Local URL '%s' contains unsupported hostname"), URL);
repos_root = svn_path_uri_decode(path, pool);
#endif
repos_root = svn_repos_find_root_path(repos_root, pool);
if (!repos_root)
return svn_error_createf
(SVN_ERR_RA_LOCAL_REPOS_OPEN_FAILED, NULL,
_("Unable to open repository '%s'"), URL);
err = svn_repos_open(repos, repos_root, pool);
if (err)
return svn_error_createf
(SVN_ERR_RA_LOCAL_REPOS_OPEN_FAILED, err,
_("Unable to open repository '%s'"), URL);
{
apr_array_header_t *caps = apr_array_make(pool, 1, sizeof(const char *));
APR_ARRAY_PUSH(caps, const char *) = SVN_RA_CAPABILITY_MERGEINFO;
SVN_ERR(svn_repos_remember_client_capabilities(*repos, caps));
}
*fs_path = svn_path_uri_decode(path, pool)
+ (strlen(repos_root)
- (hostname ? strlen(hostname) + 2 : 0));
if (**fs_path != '/')
*fs_path = apr_pstrcat(pool, "/", *fs_path, NULL);
urlbuf = svn_stringbuf_create(URL, pool);
svn_path_remove_components(urlbuf,
svn_path_component_count(*fs_path));
*repos_url = urlbuf->data;
return SVN_NO_ERROR;
}