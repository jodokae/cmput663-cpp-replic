#if !defined(SVN_LIBSVN_REPOS_H)
#define SVN_LIBSVN_REPOS_H
#include <apr_pools.h>
#include <apr_hash.h>
#include "svn_fs.h"
#if defined(__cplusplus)
extern "C" {
#endif
#define SVN_REPOS__FORMAT_NUMBER 5
#define SVN_REPOS__FORMAT_NUMBER_LEGACY 3
#define SVN_REPOS__README "README.txt"
#define SVN_REPOS__FORMAT "format"
#define SVN_REPOS__DB_DIR "db"
#define SVN_REPOS__DAV_DIR "dav"
#define SVN_REPOS__LOCK_DIR "locks"
#define SVN_REPOS__HOOK_DIR "hooks"
#define SVN_REPOS__CONF_DIR "conf"
#define SVN_REPOS__DB_LOCKFILE "db.lock"
#define SVN_REPOS__DB_LOGS_LOCKFILE "db-logs.lock"
#define SVN_REPOS__HOOK_START_COMMIT "start-commit"
#define SVN_REPOS__HOOK_PRE_COMMIT "pre-commit"
#define SVN_REPOS__HOOK_POST_COMMIT "post-commit"
#define SVN_REPOS__HOOK_READ_SENTINEL "read-sentinels"
#define SVN_REPOS__HOOK_WRITE_SENTINEL "write-sentinels"
#define SVN_REPOS__HOOK_PRE_REVPROP_CHANGE "pre-revprop-change"
#define SVN_REPOS__HOOK_POST_REVPROP_CHANGE "post-revprop-change"
#define SVN_REPOS__HOOK_PRE_LOCK "pre-lock"
#define SVN_REPOS__HOOK_POST_LOCK "post-lock"
#define SVN_REPOS__HOOK_PRE_UNLOCK "pre-unlock"
#define SVN_REPOS__HOOK_POST_UNLOCK "post-unlock"
#define SVN_REPOS__HOOK_DESC_EXT ".tmpl"
#define SVN_REPOS__CONF_SVNSERVE_CONF "svnserve.conf"
#define SVN_REPOS__CONF_PASSWD "passwd"
#define SVN_REPOS__CONF_AUTHZ "authz"
struct svn_repos_t {
svn_fs_t *fs;
char *path;
char *conf_path;
char *hook_path;
char *lock_path;
char *db_path;
int format;
const char *fs_type;
apr_array_header_t *client_capabilities;
apr_hash_t *repository_capabilities;
};
svn_error_t *
svn_repos__hooks_start_commit(svn_repos_t *repos,
const char *user,
apr_array_header_t *capabilities,
apr_pool_t *pool);
svn_error_t *
svn_repos__hooks_pre_commit(svn_repos_t *repos,
const char *txn_name,
apr_pool_t *pool);
svn_error_t *
svn_repos__hooks_post_commit(svn_repos_t *repos,
svn_revnum_t rev,
apr_pool_t *pool);
svn_error_t *
svn_repos__hooks_pre_revprop_change(svn_repos_t *repos,
svn_revnum_t rev,
const char *author,
const char *name,
const svn_string_t *new_value,
char action,
apr_pool_t *pool);
svn_error_t *
svn_repos__hooks_post_revprop_change(svn_repos_t *repos,
svn_revnum_t rev,
const char *author,
const char *name,
svn_string_t *old_value,
char action,
apr_pool_t *pool);
svn_error_t *
svn_repos__hooks_pre_lock(svn_repos_t *repos,
const char *path,
const char *username,
apr_pool_t *pool);
svn_error_t *
svn_repos__hooks_post_lock(svn_repos_t *repos,
apr_array_header_t *paths,
const char *username,
apr_pool_t *pool);
svn_error_t *
svn_repos__hooks_pre_unlock(svn_repos_t *repos,
const char *path,
const char *username,
apr_pool_t *pool);
svn_error_t *
svn_repos__hooks_post_unlock(svn_repos_t *repos,
apr_array_header_t *paths,
const char *username,
apr_pool_t *pool);
svn_error_t *
svn_repos__compare_files(svn_boolean_t *changed_p,
svn_fs_root_t *root1,
const char *path1,
svn_fs_root_t *root2,
const char *path2,
apr_pool_t *pool);
svn_error_t *
svn_repos__prev_location(svn_revnum_t *appeared_rev,
const char **prev_path,
svn_revnum_t *prev_rev,
svn_fs_t *fs,
svn_revnum_t revision,
const char *path,
apr_pool_t *pool);
#if defined(__cplusplus)
}
#endif
#endif
