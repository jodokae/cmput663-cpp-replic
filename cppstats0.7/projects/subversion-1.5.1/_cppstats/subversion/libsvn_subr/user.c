#include <apr_pools.h>
#include <apr_user.h>
#include <apr_env.h>
#include "svn_user.h"
#include "svn_utf.h"
static const char *
get_os_username(apr_pool_t *pool) {
#if APR_HAS_USER
char *username;
apr_uid_t uid;
apr_gid_t gid;
if (apr_uid_current(&uid, &gid, pool) == APR_SUCCESS &&
apr_uid_name_get(&username, uid, pool) == APR_SUCCESS)
return username;
#endif
return NULL;
}
static const char *
utf8_or_nothing(const char *str, apr_pool_t *pool) {
if (str) {
const char *utf8_str;
svn_error_t *err = svn_utf_cstring_to_utf8(&utf8_str, str, pool);
if (! err)
return utf8_str;
svn_error_clear(err);
}
return NULL;
}
const char *
svn_user_get_name(apr_pool_t *pool) {
const char *username = get_os_username(pool);
return utf8_or_nothing(username, pool);
}
const char *
svn_user_get_homedir(apr_pool_t *pool) {
const char *username;
char *homedir;
if (apr_env_get(&homedir, "HOME", pool) == APR_SUCCESS)
return utf8_or_nothing(homedir, pool);
username = get_os_username(pool);
if (username != NULL &&
apr_uid_homepath_get(&homedir, username, pool) == APR_SUCCESS)
return utf8_or_nothing(homedir, pool);
return NULL;
}
