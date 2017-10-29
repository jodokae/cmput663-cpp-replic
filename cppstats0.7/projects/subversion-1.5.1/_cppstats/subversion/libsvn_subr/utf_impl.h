#if !defined(SVN_LIBSVN_SUBR_UTF_IMPL_H)
#define SVN_LIBSVN_SUBR_UTF_IMPL_H
#include <apr_pools.h>
#include "svn_types.h"
#if defined(__cplusplus)
extern "C" {
#endif
const char *svn_utf__cstring_from_utf8_fuzzy(const char *src,
apr_pool_t *pool,
svn_error_t *(*convert_from_utf8)
(const char **,
const char *,
apr_pool_t *));
const char *svn_utf__last_valid(const char *src, apr_size_t len);
svn_boolean_t svn_utf__is_valid(const char *src, apr_size_t len);
svn_boolean_t svn_utf__cstring_is_valid(const char *src);
const char *svn_utf__last_valid2(const char *src, apr_size_t len);
#if defined(__cplusplus)
}
#endif
#endif