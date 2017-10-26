#if !defined(SVN_TIME_H)
#define SVN_TIME_H
#include <apr_pools.h>
#include <apr_time.h>
#include "svn_error.h"
#if defined(__cplusplus)
extern "C" {
#endif
const char *svn_time_to_cstring(apr_time_t when, apr_pool_t *pool);
svn_error_t *svn_time_from_cstring(apr_time_t *when, const char *data,
apr_pool_t *pool);
const char *svn_time_to_human_cstring(apr_time_t when, apr_pool_t *pool);
svn_error_t *
svn_parse_date(svn_boolean_t *matched, apr_time_t *result, const char *text,
apr_time_t now, apr_pool_t *pool);
void svn_sleep_for_timestamps(void);
#if defined(__cplusplus)
}
#endif
#endif
