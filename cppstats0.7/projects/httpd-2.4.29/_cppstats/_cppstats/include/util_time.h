#if !defined(APACHE_UTIL_TIME_H)
#define APACHE_UTIL_TIME_H
#include "apr.h"
#include "apr_time.h"
#include "httpd.h"
#if defined(__cplusplus)
extern "C" {
#endif
#define AP_TIME_RECENT_THRESHOLD 15
#define AP_CTIME_OPTION_NONE 0x0
#define AP_CTIME_OPTION_USEC 0x1
#define AP_CTIME_OPTION_COMPACT 0x2
AP_DECLARE(apr_status_t) ap_explode_recent_localtime(apr_time_exp_t *tm,
apr_time_t t);
AP_DECLARE(apr_status_t) ap_explode_recent_gmt(apr_time_exp_t *tm,
apr_time_t t);
AP_DECLARE(apr_status_t) ap_recent_ctime(char *date_str, apr_time_t t);
AP_DECLARE(apr_status_t) ap_recent_ctime_ex(char *date_str, apr_time_t t,
int option, int *len);
AP_DECLARE(apr_status_t) ap_recent_rfc822_date(char *date_str, apr_time_t t);
#if defined(__cplusplus)
}
#endif
#endif