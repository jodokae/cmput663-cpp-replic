#if !defined(SVN_LIBSVN_WC_ADM_OPS_H)
#define SVN_LIBSVN_WC_ADM_OPS_H
#include <apr_pools.h>
#include "svn_types.h"
#if defined(__cplusplus)
extern "C" {
#endif
svn_error_t *svn_wc__do_update_cleanup(const char *path,
svn_wc_adm_access_t *adm_access,
svn_depth_t depth,
const char *base_url,
const char *repos,
svn_revnum_t new_revision,
svn_wc_notify_func2_t notify_func,
void *notify_baton,
svn_boolean_t remove_missing_dirs,
apr_hash_t *exclude_paths,
apr_pool_t *pool);
#if defined(__cplusplus)
}
#endif
#endif
