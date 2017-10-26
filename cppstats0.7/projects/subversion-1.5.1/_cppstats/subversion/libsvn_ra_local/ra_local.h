#if !defined(SVN_LIBSVN_RA_LOCAL_H)
#define SVN_LIBSVN_RA_LOCAL_H
#include <apr_pools.h>
#include <apr_tables.h>
#include "svn_types.h"
#include "svn_fs.h"
#include "svn_repos.h"
#include "svn_ra.h"
#if defined(__cplusplus)
extern "C" {
#endif
typedef struct svn_ra_local__session_baton_t {
const char *username;
const char *repos_url;
svn_stringbuf_t *fs_path;
svn_repos_t *repos;
svn_fs_t *fs;
const char *uuid;
const svn_ra_callbacks2_t *callbacks;
void *callback_baton;
} svn_ra_local__session_baton_t;
svn_error_t *
svn_ra_local__split_URL(svn_repos_t **repos,
const char **repos_url,
const char **fs_path,
const char *URL,
apr_pool_t *pool);
#if defined(__cplusplus)
}
#endif
#endif
