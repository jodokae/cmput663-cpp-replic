#if !defined(SVN_USER_H)
#define SVN_USER_H
#include <apr_pools.h>
#include "svn_types.h"
#if defined(__cplusplus)
extern "C" {
#endif
const char *
svn_user_get_name(apr_pool_t *pool);
const char *
svn_user_get_homedir(apr_pool_t *pool);
#if defined(__cplusplus)
}
#endif
#endif