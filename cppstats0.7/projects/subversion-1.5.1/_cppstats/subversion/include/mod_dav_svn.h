#if !defined(MOD_DAV_SVN_H)
#define MOD_DAV_SVN_H
#include <httpd.h>
#include <mod_dav.h>
#if defined(__cplusplus)
extern "C" {
#endif
AP_MODULE_DECLARE(dav_error *) dav_svn_split_uri(request_rec *r,
const char *uri,
const char *root_path,
const char **cleaned_uri,
int *trailing_slash,
const char **repos_name,
const char **relative_path,
const char **repos_path);
AP_MODULE_DECLARE(dav_error *) dav_svn_get_repos_path(request_rec *r,
const char *root_path,
const char **repos_path);
#if defined(__cplusplus)
}
#endif
#endif