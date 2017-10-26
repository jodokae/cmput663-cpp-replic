#include <apr_pools.h>
#include "svn_types.h"
#include "svn_error.h"
#include "svn_error_codes.h"
#include "svn_path.h"
#include "svn_ra.h"
#include "svn_private_config.h"
#include "private/svn_ra_private.h"
svn_error_t *
svn_ra__assert_mergeinfo_capable_server(svn_ra_session_t *ra_session,
const char *path_or_url,
apr_pool_t *pool) {
svn_boolean_t mergeinfo_capable;
SVN_ERR(svn_ra_has_capability(ra_session, &mergeinfo_capable,
SVN_RA_CAPABILITY_MERGEINFO, pool));
if (! mergeinfo_capable) {
if (path_or_url == NULL) {
svn_error_t *err = svn_ra_get_session_url(ra_session, &path_or_url,
pool);
if (err) {
svn_error_clear(err);
path_or_url = "<repository>";
}
}
return svn_error_createf(SVN_ERR_UNSUPPORTED_FEATURE, NULL,
_("Retrieval of mergeinfo unsupported by '%s'"),
svn_path_local_style(path_or_url, pool));
}
return SVN_NO_ERROR;
}
