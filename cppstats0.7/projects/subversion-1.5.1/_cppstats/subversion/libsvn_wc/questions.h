#if !defined(SVN_LIBSVN_WC_QUESTIONS_H)
#define SVN_LIBSVN_WC_QUESTIONS_H
#include <apr_pools.h>
#include "svn_types.h"
#include "svn_error.h"
#if defined(__cplusplus)
extern "C" {
#endif
enum svn_wc__timestamp_kind {
svn_wc__text_time = 1,
svn_wc__prop_time
};
svn_error_t *
svn_wc__check_format(int wc_format, const char *path, apr_pool_t *pool);
svn_error_t *
svn_wc__timestamps_equal_p(svn_boolean_t *equal_p,
const char *path,
svn_wc_adm_access_t *adm_access,
enum svn_wc__timestamp_kind timestamp_kind,
apr_pool_t *pool);
svn_error_t *svn_wc__versioned_file_modcheck(svn_boolean_t *modified_p,
const char *versioned_file,
svn_wc_adm_access_t *adm_access,
const char *base_file,
svn_boolean_t compare_textbases,
apr_pool_t *pool);
#if defined(__cplusplus)
}
#endif
#endif