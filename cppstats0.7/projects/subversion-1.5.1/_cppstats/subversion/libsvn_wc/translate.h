#if !defined(SVN_LIBSVN_WC_TRANSLATE_H)
#define SVN_LIBSVN_WC_TRANSLATE_H
#include <apr_pools.h>
#include "svn_types.h"
#include "svn_subst.h"
#if defined(__cplusplus)
extern "C" {
#endif
svn_error_t *svn_wc__get_eol_style(svn_subst_eol_style_t *style,
const char **eol,
const char *path,
svn_wc_adm_access_t *adm_access,
apr_pool_t *pool);
void svn_wc__eol_value_from_string(const char **value,
const char *eol);
svn_error_t *svn_wc__get_keywords(apr_hash_t **keywords,
const char *path,
svn_wc_adm_access_t *adm_access,
const char *force_list,
apr_pool_t *pool);
svn_error_t *svn_wc__get_special(svn_boolean_t *special,
const char *path,
svn_wc_adm_access_t *adm_access,
apr_pool_t *pool);
svn_error_t *
svn_wc__maybe_set_executable(svn_boolean_t *did_set,
const char *path,
svn_wc_adm_access_t *adm_access,
apr_pool_t *pool);
svn_error_t * svn_wc__maybe_set_read_only(svn_boolean_t *did_set,
const char *path,
svn_wc_adm_access_t *adm_access,
apr_pool_t *pool);
#if defined(__cplusplus)
}
#endif
#endif
