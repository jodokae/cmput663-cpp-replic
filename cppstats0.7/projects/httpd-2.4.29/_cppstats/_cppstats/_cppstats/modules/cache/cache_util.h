#if !defined(CACHE_UTIL_H)
#define CACHE_UTIL_H
#if defined(__cplusplus)
extern "C" {
#endif
#include "mod_cache.h"
#include "apr_hooks.h"
#include "apr.h"
#include "apr_lib.h"
#include "apr_strings.h"
#include "apr_buckets.h"
#include "apr_md5.h"
#include "apr_pools.h"
#include "apr_strings.h"
#include "apr_optional.h"
#define APR_WANT_STRFUNC
#include "apr_want.h"
#include "httpd.h"
#include "http_config.h"
#include "ap_config.h"
#include "http_core.h"
#include "http_protocol.h"
#include "http_request.h"
#include "http_vhost.h"
#include "http_main.h"
#include "http_log.h"
#include "http_connection.h"
#include "util_filter.h"
#include "apr_uri.h"
#if defined(HAVE_NETDB_H)
#include <netdb.h>
#endif
#if defined(HAVE_SYS_SOCKET_H)
#include <sys/socket.h>
#endif
#if defined(HAVE_NETINET_IN_H)
#include <netinet/in.h>
#endif
#if defined(HAVE_ARPA_INET_H)
#include <arpa/inet.h>
#endif
#include "apr_atomic.h"
#if !defined(MAX)
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif
#if !defined(MIN)
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif
#define MSEC_ONE_DAY ((apr_time_t)(86400*APR_USEC_PER_SEC))
#define MSEC_ONE_HR ((apr_time_t)(3600*APR_USEC_PER_SEC))
#define MSEC_ONE_MIN ((apr_time_t)(60*APR_USEC_PER_SEC))
#define MSEC_ONE_SEC ((apr_time_t)(APR_USEC_PER_SEC))
#define DEFAULT_CACHE_MAXEXPIRE MSEC_ONE_DAY
#define DEFAULT_CACHE_MINEXPIRE 0
#define DEFAULT_CACHE_EXPIRE MSEC_ONE_HR
#define DEFAULT_CACHE_LMFACTOR (0.1)
#define DEFAULT_CACHE_MAXAGE 5
#define DEFAULT_X_CACHE 0
#define DEFAULT_X_CACHE_DETAIL 0
#define DEFAULT_CACHE_STALE_ON_ERROR 1
#define DEFAULT_CACHE_LOCKPATH "/mod_cache-lock"
#define CACHE_LOCKNAME_KEY "mod_cache-lockname"
#define CACHE_LOCKFILE_KEY "mod_cache-lockfile"
#define CACHE_CTX_KEY "mod_cache-ctx"
#define CACHE_SEPARATOR ", \t"
struct cache_enable {
apr_uri_t url;
const char *type;
apr_size_t pathlen;
};
struct cache_disable {
apr_uri_t url;
apr_size_t pathlen;
};
typedef struct {
apr_array_header_t *cacheenable;
apr_array_header_t *cachedisable;
apr_array_header_t *ignore_headers;
apr_array_header_t *ignore_session_id;
const char *lockpath;
apr_time_t lockmaxage;
apr_uri_t *base_uri;
unsigned int ignorecachecontrol:1;
unsigned int ignorequerystring:1;
unsigned int quick:1;
unsigned int lock:1;
unsigned int x_cache:1;
unsigned int x_cache_detail:1;
#define CACHE_IGNORE_HEADERS_SET 1
#define CACHE_IGNORE_HEADERS_UNSET 0
unsigned int ignore_headers_set:1;
#define CACHE_IGNORE_SESSION_ID_SET 1
#define CACHE_IGNORE_SESSION_ID_UNSET 0
unsigned int ignore_session_id_set:1;
unsigned int base_uri_set:1;
unsigned int ignorecachecontrol_set:1;
unsigned int ignorequerystring_set:1;
unsigned int quick_set:1;
unsigned int lock_set:1;
unsigned int lockpath_set:1;
unsigned int lockmaxage_set:1;
unsigned int x_cache_set:1;
unsigned int x_cache_detail_set:1;
} cache_server_conf;
typedef struct {
apr_time_t minex;
apr_time_t maxex;
apr_time_t defex;
double factor;
apr_array_header_t *cacheenable;
unsigned int disable:1;
unsigned int x_cache:1;
unsigned int x_cache_detail:1;
unsigned int stale_on_error:1;
unsigned int no_last_mod_ignore:1;
unsigned int store_expired:1;
unsigned int store_private:1;
unsigned int store_nostore:1;
unsigned int minex_set:1;
unsigned int maxex_set:1;
unsigned int defex_set:1;
unsigned int factor_set:1;
unsigned int x_cache_set:1;
unsigned int x_cache_detail_set:1;
unsigned int stale_on_error_set:1;
unsigned int no_last_mod_ignore_set:1;
unsigned int store_expired_set:1;
unsigned int store_private_set:1;
unsigned int store_nostore_set:1;
unsigned int enable_set:1;
unsigned int disable_set:1;
} cache_dir_conf;
typedef struct cache_provider_list cache_provider_list;
struct cache_provider_list {
const char *provider_name;
const cache_provider *provider;
cache_provider_list *next;
};
typedef struct {
cache_provider_list *providers;
const cache_provider *provider;
const char *provider_name;
int fresh;
cache_handle_t *handle;
cache_handle_t *stale_handle;
apr_table_t *stale_headers;
int in_checked;
int block_response;
apr_bucket_brigade *saved_brigade;
apr_off_t saved_size;
apr_time_t exp;
apr_time_t lastmod;
cache_info *info;
ap_filter_t *save_filter;
ap_filter_t *remove_url_filter;
const char *key;
apr_off_t size;
apr_bucket_brigade *out;
cache_control_t control_in;
} cache_request_rec;
int ap_cache_check_no_cache(cache_request_rec *cache, request_rec *r);
int ap_cache_check_no_store(cache_request_rec *cache, request_rec *r);
int cache_check_freshness(cache_handle_t *h, cache_request_rec *cache,
request_rec *r);
apr_status_t cache_try_lock(cache_server_conf *conf, cache_request_rec *cache,
request_rec *r);
apr_status_t cache_remove_lock(cache_server_conf *conf,
cache_request_rec *cache, request_rec *r, apr_bucket_brigade *bb);
cache_provider_list *cache_get_providers(request_rec *r,
cache_server_conf *conf);
const char *cache_table_getm(apr_pool_t *p, const apr_table_t *t,
const char *key);
char *cache_strqtok(char *str, const char *sep, char **last);
apr_table_t *cache_merge_headers_out(request_rec *r);
int cache_use_early_url(request_rec *r);
#if defined(__cplusplus)
}
#endif
#endif