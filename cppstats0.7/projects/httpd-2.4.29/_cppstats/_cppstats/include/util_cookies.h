#if !defined(UTIL_COOKIES_H)
#define UTIL_COOKIES_H
#if defined(__cplusplus)
extern "C" {
#endif
#include "apr_errno.h"
#include "httpd.h"
#define SET_COOKIE "Set-Cookie"
#define SET_COOKIE2 "Set-Cookie2"
#define DEFAULT_ATTRS "HttpOnly;Secure;Version=1"
#define CLEAR_ATTRS "Version=1"
typedef struct {
request_rec *r;
const char *name;
const char *encoded;
apr_table_t *new_cookies;
int duplicated;
} ap_cookie_do;
AP_DECLARE(apr_status_t) ap_cookie_write(request_rec * r, const char *name,
const char *val, const char *attrs,
long maxage, ...)
AP_FN_ATTR_SENTINEL;
AP_DECLARE(apr_status_t) ap_cookie_write2(request_rec * r, const char *name2,
const char *val, const char *attrs2,
long maxage, ...)
AP_FN_ATTR_SENTINEL;
AP_DECLARE(apr_status_t) ap_cookie_remove(request_rec * r, const char *name,
const char *attrs, ...)
AP_FN_ATTR_SENTINEL;
AP_DECLARE(apr_status_t) ap_cookie_remove2(request_rec * r, const char *name2,
const char *attrs2, ...)
AP_FN_ATTR_SENTINEL;
AP_DECLARE(apr_status_t) ap_cookie_read(request_rec * r, const char *name, const char **val,
int remove);
AP_DECLARE(apr_status_t) ap_cookie_check_string(const char *string);
#if defined(__cplusplus)
}
#endif
#endif