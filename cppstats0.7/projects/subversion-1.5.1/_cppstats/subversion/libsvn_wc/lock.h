#if !defined(SVN_LIBSVN_WC_LOCK_H)
#define SVN_LIBSVN_WC_LOCK_H
#include <apr_pools.h>
#include <apr_hash.h>
#include "svn_types.h"
#include "svn_error.h"
#include "svn_wc.h"
#if defined(__cplusplus)
extern "C" {
#endif
svn_error_t *svn_wc__adm_steal_write_lock(svn_wc_adm_access_t **adm_access,
svn_wc_adm_access_t *associated,
const char *path, apr_pool_t *pool);
svn_error_t *svn_wc__adm_is_cleanup_required(svn_boolean_t *cleanup,
svn_wc_adm_access_t *adm_access,
apr_pool_t *pool);
void svn_wc__adm_access_set_entries(svn_wc_adm_access_t *adm_access,
svn_boolean_t show_deleted,
apr_hash_t *entries);
apr_hash_t *svn_wc__adm_access_entries(svn_wc_adm_access_t *adm_access,
svn_boolean_t show_deleted,
apr_pool_t *pool);
void svn_wc__adm_access_set_wcprops(svn_wc_adm_access_t *adm_access,
apr_hash_t *wcprops);
apr_hash_t *svn_wc__adm_access_wcprops(svn_wc_adm_access_t *adm_access);
svn_error_t *svn_wc__adm_pre_open(svn_wc_adm_access_t **adm_access,
const char *path,
apr_pool_t *pool);
svn_boolean_t svn_wc__adm_missing(svn_wc_adm_access_t *adm_access,
const char *path);
svn_error_t * svn_wc__adm_retrieve_internal(svn_wc_adm_access_t **adm_access,
svn_wc_adm_access_t *associated,
const char *path,
apr_pool_t *pool);
int svn_wc__adm_wc_format(svn_wc_adm_access_t *adm_access);
void svn_wc__adm_set_wc_format(svn_wc_adm_access_t *adm_access,
int format);
svn_error_t *svn_wc__adm_write_check(svn_wc_adm_access_t *adm_access);
svn_error_t *svn_wc__adm_extend_lock_to_tree(svn_wc_adm_access_t *adm_access,
apr_pool_t *pool);
#if defined(__cplusplus)
}
#endif
#endif