#if !defined(UTIL_LDAP_H)
#define UTIL_LDAP_H
#include "apr.h"
#include "apr_thread_mutex.h"
#include "apr_thread_rwlock.h"
#include "apr_tables.h"
#include "apr_time.h"
#include "apr_version.h"
#if APR_MAJOR_VERSION < 2
#include "apr_ldap.h"
#include "apr_ldap_rebind.h"
#else
#define APR_HAS_LDAP 0
#endif
#if APR_HAS_SHARED_MEMORY
#include "apr_rmm.h"
#include "apr_shm.h"
#endif
#if APR_HAS_LDAP
#if defined(LDAP_UNAVAILABLE) || APR_HAS_MICROSOFT_LDAPSDK
#define AP_LDAP_IS_SERVER_DOWN(s) ((s) == LDAP_SERVER_DOWN ||(s) == LDAP_UNAVAILABLE)
#else
#define AP_LDAP_IS_SERVER_DOWN(s) ((s) == LDAP_SERVER_DOWN)
#endif
#include "ap_config.h"
#include "httpd.h"
#include "http_config.h"
#include "http_core.h"
#include "http_log.h"
#include "http_protocol.h"
#include "http_request.h"
#include "apr_optional.h"
#if !defined(WIN32)
#define LDAP_DECLARE(type) type
#define LDAP_DECLARE_NONSTD(type) type
#define LDAP_DECLARE_DATA
#elif defined(LDAP_DECLARE_STATIC)
#define LDAP_DECLARE(type) type __stdcall
#define LDAP_DECLARE_NONSTD(type) type
#define LDAP_DECLARE_DATA
#elif defined(LDAP_DECLARE_EXPORT)
#define LDAP_DECLARE(type) __declspec(dllexport) type __stdcall
#define LDAP_DECLARE_NONSTD(type) __declspec(dllexport) type
#define LDAP_DECLARE_DATA __declspec(dllexport)
#else
#define LDAP_DECLARE(type) __declspec(dllimport) type __stdcall
#define LDAP_DECLARE_NONSTD(type) __declspec(dllimport) type
#define LDAP_DECLARE_DATA __declspec(dllimport)
#endif
#if APR_HAS_MICROSOFT_LDAPSDK
#define timeval l_timeval
#endif
#if defined(__cplusplus)
extern "C" {
#endif
typedef enum {
never=LDAP_DEREF_NEVER,
searching=LDAP_DEREF_SEARCHING,
finding=LDAP_DEREF_FINDING,
always=LDAP_DEREF_ALWAYS
} deref_options;
typedef struct util_ldap_connection_t {
LDAP *ldap;
apr_pool_t *pool;
#if APR_HAS_THREADS
apr_thread_mutex_t *lock;
#endif
const char *host;
int port;
deref_options deref;
const char *binddn;
const char *bindpw;
int bound;
int secure;
apr_array_header_t *client_certs;
const char *reason;
struct util_ldap_connection_t *next;
struct util_ldap_state_t *st;
int keep;
int ChaseReferrals;
int ReferralHopLimit;
apr_time_t freed;
apr_pool_t *rebind_pool;
int must_rebind;
request_rec *r;
apr_time_t last_backend_conn;
} util_ldap_connection_t;
typedef struct util_ldap_config_t {
int ChaseReferrals;
int ReferralHopLimit;
apr_array_header_t *client_certs;
} util_ldap_config_t;
typedef struct util_ldap_state_t {
apr_pool_t *pool;
#if APR_HAS_THREADS
apr_thread_mutex_t *mutex;
#endif
apr_global_mutex_t *util_ldap_cache_lock;
apr_size_t cache_bytes;
char *cache_file;
long search_cache_ttl;
long search_cache_size;
long compare_cache_ttl;
long compare_cache_size;
struct util_ldap_connection_t *connections;
apr_array_header_t *global_certs;
int ssl_supported;
int secure;
int secure_set;
int verify_svr_cert;
#if APR_HAS_SHARED_MEMORY
apr_shm_t *cache_shm;
apr_rmm_t *cache_rmm;
#endif
void *util_ldap_cache;
long connectionTimeout;
struct timeval *opTimeout;
int debug_level;
apr_interval_time_t connection_pool_ttl;
int retries;
apr_interval_time_t retry_delay;
} util_ldap_state_t;
struct mod_auth_ldap_groupattr_entry_t {
char *name;
};
APR_DECLARE_OPTIONAL_FN(int,uldap_connection_open,(request_rec *r,
util_ldap_connection_t *ldc));
APR_DECLARE_OPTIONAL_FN(void,uldap_connection_close,(util_ldap_connection_t *ldc));
APR_DECLARE_OPTIONAL_FN(apr_status_t,uldap_connection_unbind,(void *param));
APR_DECLARE_OPTIONAL_FN(util_ldap_connection_t *,uldap_connection_find,(request_rec *r, const char *host, int port,
const char *binddn, const char *bindpw, deref_options deref,
int secure));
APR_DECLARE_OPTIONAL_FN(int,uldap_cache_comparedn,(request_rec *r, util_ldap_connection_t *ldc,
const char *url, const char *dn, const char *reqdn,
int compare_dn_on_server));
APR_DECLARE_OPTIONAL_FN(int,uldap_cache_compare,(request_rec *r, util_ldap_connection_t *ldc,
const char *url, const char *dn, const char *attrib, const char *value));
APR_DECLARE_OPTIONAL_FN(int,uldap_cache_check_subgroups,(request_rec *r, util_ldap_connection_t *ldc,
const char *url, const char *dn, const char *attrib, const char *value,
char **subgroupAttrs, apr_array_header_t *subgroupclasses,
int cur_subgroup_depth, int max_subgroup_depth));
APR_DECLARE_OPTIONAL_FN(int,uldap_cache_checkuserid,(request_rec *r, util_ldap_connection_t *ldc,
const char *url, const char *basedn, int scope, char **attrs,
const char *filter, const char *bindpw, const char **binddn, const char ***retvals));
APR_DECLARE_OPTIONAL_FN(int,uldap_cache_getuserdn,(request_rec *r, util_ldap_connection_t *ldc,
const char *url, const char *basedn, int scope, char **attrs,
const char *filter, const char **binddn, const char ***retvals));
APR_DECLARE_OPTIONAL_FN(int,uldap_ssl_supported,(request_rec *r));
apr_status_t util_ldap_cache_init(apr_pool_t *pool, util_ldap_state_t *st);
char *util_ald_cache_display(request_rec *r, util_ldap_state_t *st);
#if defined(__cplusplus)
}
#endif
#endif
#endif