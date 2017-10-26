#if !defined(SVN_RA_PRIVATE_H)
#define SVN_RA_PRIVATE_H
#include <apr_pools.h>
#include "svn_error.h"
#include "svn_ra.h"
#if defined(__cplusplus)
extern "C" {
#endif
svn_error_t *
svn_ra__assert_mergeinfo_capable_server(svn_ra_session_t *ra_session,
const char *path_or_url,
apr_pool_t *pool);
#if defined(__cplusplus)
}
#endif
#endif
