#if !defined(SVN_WC_PRIVATE_H)
#define SVN_WC_PRIVATE_H
#include "svn_wc.h"
#if defined(__cplusplus)
extern "C" {
#endif
svn_error_t *
svn_wc__entry_versioned_internal(const svn_wc_entry_t **entry,
const char *path,
svn_wc_adm_access_t *adm_access,
svn_boolean_t show_hidden,
const char *caller_filename,
int caller_lineno,
apr_pool_t *pool);
#if defined(SVN_DEBUG)
#define svn_wc__entry_versioned(entry, path, adm_access, show_hidden, pool) svn_wc__entry_versioned_internal((entry), (path), (adm_access), (show_hidden), __FILE__, __LINE__, (pool))
#else
#define svn_wc__entry_versioned(entry, path, adm_access, show_hidden, pool) svn_wc__entry_versioned_internal((entry), (path), (adm_access), (show_hidden), NULL, 0, (pool))
#endif
svn_error_t *svn_wc__props_modified(const char *path,
apr_hash_t **which_props,
svn_wc_adm_access_t *adm_access,
apr_pool_t *pool);
svn_error_t *
svn_wc__path_switched(const char *wcpath,
svn_boolean_t *switched,
const svn_wc_entry_t *entry,
apr_pool_t *pool);
#define SVN_WC__LEVELS_TO_LOCK_FROM_DEPTH(depth) (((depth) == svn_depth_empty || (depth) == svn_depth_files) ? 0 : (((depth) == svn_depth_immediates) ? 1 : -1))
#define SVN_WC__CL_MATCH(clhash, entry) (((clhash == NULL) || (entry && entry->changelist && apr_hash_get(clhash, entry->changelist, APR_HASH_KEY_STRING))) ? TRUE : FALSE)
#if defined(__cplusplus)
}
#endif
#endif