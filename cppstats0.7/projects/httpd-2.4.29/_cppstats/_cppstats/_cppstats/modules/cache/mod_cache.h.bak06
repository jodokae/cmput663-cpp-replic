#if !defined(MOD_CACHE_H)
#define MOD_CACHE_H
#include "httpd.h"
#include "apr_date.h"
#include "apr_optional.h"
#include "apr_hooks.h"
#include "cache_common.h"
#if !defined(WIN32)
#define CACHE_DECLARE(type) type
#define CACHE_DECLARE_NONSTD(type) type
#define CACHE_DECLARE_DATA
#elif defined(CACHE_DECLARE_STATIC)
#define CACHE_DECLARE(type) type __stdcall
#define CACHE_DECLARE_NONSTD(type) type
#define CACHE_DECLARE_DATA
#elif defined(CACHE_DECLARE_EXPORT)
#define CACHE_DECLARE(type) __declspec(dllexport) type __stdcall
#define CACHE_DECLARE_NONSTD(type) __declspec(dllexport) type
#define CACHE_DECLARE_DATA __declspec(dllexport)
#else
#define CACHE_DECLARE(type) __declspec(dllimport) type __stdcall
#define CACHE_DECLARE_NONSTD(type) __declspec(dllimport) type
#define CACHE_DECLARE_DATA __declspec(dllimport)
#endif
typedef struct cache_info cache_info;
struct cache_info {
apr_time_t date;
apr_time_t expire;
apr_time_t request_time;
apr_time_t response_time;
int status;
cache_control_t control;
};
typedef struct cache_object cache_object_t;
struct cache_object {
const char *key;
cache_object_t *next;
cache_info info;
void *vobj;
};
typedef struct cache_handle cache_handle_t;
struct cache_handle {
cache_object_t *cache_obj;
apr_table_t *req_hdrs;
apr_table_t *resp_hdrs;
};
#define CACHE_PROVIDER_GROUP "cache"
typedef struct {
int (*remove_entity) (cache_handle_t *h);
apr_status_t (*store_headers)(cache_handle_t *h, request_rec *r, cache_info *i);
apr_status_t (*store_body)(cache_handle_t *h, request_rec *r, apr_bucket_brigade *in,
apr_bucket_brigade *out);
apr_status_t (*recall_headers) (cache_handle_t *h, request_rec *r);
apr_status_t (*recall_body) (cache_handle_t *h, apr_pool_t *p, apr_bucket_brigade *bb);
int (*create_entity) (cache_handle_t *h, request_rec *r,
const char *urlkey, apr_off_t len, apr_bucket_brigade *bb);
int (*open_entity) (cache_handle_t *h, request_rec *r,
const char *urlkey);
int (*remove_url) (cache_handle_t *h, request_rec *r);
apr_status_t (*commit_entity)(cache_handle_t *h, request_rec *r);
apr_status_t (*invalidate_entity)(cache_handle_t *h, request_rec *r);
} cache_provider;
typedef enum {
AP_CACHE_HIT,
AP_CACHE_REVALIDATE,
AP_CACHE_MISS,
AP_CACHE_INVALIDATE
} ap_cache_status_e;
#define AP_CACHE_HIT_ENV "cache-hit"
#define AP_CACHE_REVALIDATE_ENV "cache-revalidate"
#define AP_CACHE_MISS_ENV "cache-miss"
#define AP_CACHE_INVALIDATE_ENV "cache-invalidate"
#define AP_CACHE_STATUS_ENV "cache-status"
CACHE_DECLARE(apr_time_t) ap_cache_current_age(cache_info *info, const apr_time_t age_value,
apr_time_t now);
CACHE_DECLARE(apr_time_t) ap_cache_hex2usec(const char *x);
CACHE_DECLARE(void) ap_cache_usec2hex(apr_time_t j, char *y);
CACHE_DECLARE(char *) ap_cache_generate_name(apr_pool_t *p, int dirlevels,
int dirlength,
const char *name);
CACHE_DECLARE(const char *)ap_cache_tokstr(apr_pool_t *p, const char *list, const char **str);
CACHE_DECLARE(apr_table_t *)ap_cache_cacheable_headers(apr_pool_t *pool,
apr_table_t *t,
server_rec *s);
CACHE_DECLARE(apr_table_t *)ap_cache_cacheable_headers_in(request_rec *r);
CACHE_DECLARE(apr_table_t *)ap_cache_cacheable_headers_out(request_rec *r);
int ap_cache_control(request_rec *r, cache_control_t *cc, const char *cc_header,
const char *pragma_header, apr_table_t *headers);
APR_DECLARE_EXTERNAL_HOOK(cache, CACHE, int, cache_status, (cache_handle_t *h,
request_rec *r, apr_table_t *headers, ap_cache_status_e status,
const char *reason))
APR_DECLARE_OPTIONAL_FN(apr_status_t,
ap_cache_generate_key,
(request_rec *r, apr_pool_t*p, const char **key));
#endif
