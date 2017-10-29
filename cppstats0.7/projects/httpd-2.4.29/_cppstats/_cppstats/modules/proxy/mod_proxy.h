#if !defined(MOD_PROXY_H)
#define MOD_PROXY_H
#include "apr_hooks.h"
#include "apr_optional.h"
#include "apr.h"
#include "apr_lib.h"
#include "apr_strings.h"
#include "apr_buckets.h"
#include "apr_md5.h"
#include "apr_network_io.h"
#include "apr_pools.h"
#include "apr_strings.h"
#include "apr_uri.h"
#include "apr_date.h"
#include "apr_strmatch.h"
#include "apr_fnmatch.h"
#include "apr_reslist.h"
#define APR_WANT_STRFUNC
#include "apr_want.h"
#include "apr_uuid.h"
#include "util_mutex.h"
#include "apr_global_mutex.h"
#include "apr_thread_mutex.h"
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
#include "util_ebcdic.h"
#include "ap_provider.h"
#include "ap_slotmem.h"
#if APR_HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#if APR_HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
enum enctype {
enc_path, enc_search, enc_user, enc_fpath, enc_parm
};
typedef enum {
NONE, TCP, OPTIONS, HEAD, GET, CPING, PROVIDER, EOT
} hcmethod_t;
typedef struct {
hcmethod_t method;
char *name;
int implemented;
} proxy_hcmethods_t;
typedef struct {
unsigned int bit;
char flag;
const char *name;
} proxy_wstat_t;
#define BALANCER_PREFIX "balancer://"
#if APR_CHARSET_EBCDIC
#define CRLF "\r\n"
#else
#define CRLF "\015\012"
#endif
#define DEFAULT_MAX_FORWARDS -1
typedef struct proxy_balancer proxy_balancer;
typedef struct proxy_worker proxy_worker;
typedef struct proxy_conn_pool proxy_conn_pool;
typedef struct proxy_balancer_method proxy_balancer_method;
struct proxy_remote {
const char *scheme;
const char *protocol;
const char *hostname;
ap_regex_t *regexp;
int use_regex;
apr_port_t port;
};
#define PROXYPASS_NOCANON 0x01
#define PROXYPASS_INTERPOLATE 0x02
#define PROXYPASS_NOQUERY 0x04
struct proxy_alias {
const char *real;
const char *fake;
ap_regex_t *regex;
unsigned int flags;
proxy_balancer *balancer;
};
struct dirconn_entry {
char *name;
struct in_addr addr, mask;
struct apr_sockaddr_t *hostaddr;
int (*matcher) (struct dirconn_entry * This, request_rec *r);
};
struct noproxy_entry {
const char *name;
struct apr_sockaddr_t *addr;
};
typedef struct {
apr_array_header_t *proxies;
apr_array_header_t *sec_proxy;
apr_array_header_t *aliases;
apr_array_header_t *noproxies;
apr_array_header_t *dirconn;
apr_array_header_t *workers;
apr_array_header_t *balancers;
proxy_worker *forward;
proxy_worker *reverse;
const char *domain;
const char *id;
apr_pool_t *pool;
int req;
int max_balancers;
int bgrowth;
enum {
via_off,
via_on,
via_block,
via_full
} viaopt;
apr_size_t recv_buffer_size;
apr_size_t io_buffer_size;
long maxfwd;
apr_interval_time_t timeout;
enum {
bad_error,
bad_ignore,
bad_body
} badopt;
enum {
status_off,
status_on,
status_full
} proxy_status;
apr_sockaddr_t *source_address;
apr_global_mutex_t *mutex;
ap_slotmem_instance_t *bslot;
ap_slotmem_provider_t *storage;
unsigned int req_set:1;
unsigned int viaopt_set:1;
unsigned int recv_buffer_size_set:1;
unsigned int io_buffer_size_set:1;
unsigned int maxfwd_set:1;
unsigned int timeout_set:1;
unsigned int badopt_set:1;
unsigned int proxy_status_set:1;
unsigned int source_address_set:1;
unsigned int bgrowth_set:1;
unsigned int bal_persist:1;
unsigned int inherit:1;
unsigned int inherit_set:1;
unsigned int ppinherit:1;
unsigned int ppinherit_set:1;
} proxy_server_conf;
typedef struct {
const char *p;
ap_regex_t *r;
apr_array_header_t *raliases;
apr_array_header_t* cookie_paths;
apr_array_header_t* cookie_domains;
signed char p_is_fnmatch;
signed char interpolate_env;
struct proxy_alias *alias;
unsigned int error_override:1;
unsigned int preserve_host:1;
unsigned int preserve_host_set:1;
unsigned int error_override_set:1;
unsigned int alias_set:1;
unsigned int add_forwarded_headers:1;
unsigned int add_forwarded_headers_set:1;
apr_array_header_t *refs;
} proxy_dir_conf;
typedef struct {
apr_array_header_t *raliases;
apr_array_header_t* cookie_paths;
apr_array_header_t* cookie_domains;
} proxy_req_conf;
typedef struct {
conn_rec *connection;
request_rec *r;
proxy_worker *worker;
apr_pool_t *pool;
const char *hostname;
apr_sockaddr_t *addr;
apr_pool_t *scpool;
apr_socket_t *sock;
void *data;
void *forward;
apr_uint32_t flags;
apr_port_t port;
unsigned int is_ssl:1;
unsigned int close:1;
unsigned int need_flush:1;
unsigned int inreslist:1;
const char *uds_path;
const char *ssl_hostname;
apr_bucket_brigade *tmp_bb;
} proxy_conn_rec;
typedef struct {
float cache_completion;
int content_length;
} proxy_completion;
struct proxy_conn_pool {
apr_pool_t *pool;
apr_sockaddr_t *addr;
apr_reslist_t *res;
proxy_conn_rec *conn;
};
#define PROXY_WORKER_INITIALIZED 0x0001
#define PROXY_WORKER_IGNORE_ERRORS 0x0002
#define PROXY_WORKER_DRAIN 0x0004
#define PROXY_WORKER_GENERIC 0x0008
#define PROXY_WORKER_IN_SHUTDOWN 0x0010
#define PROXY_WORKER_DISABLED 0x0020
#define PROXY_WORKER_STOPPED 0x0040
#define PROXY_WORKER_IN_ERROR 0x0080
#define PROXY_WORKER_HOT_STANDBY 0x0100
#define PROXY_WORKER_FREE 0x0200
#define PROXY_WORKER_HC_FAIL 0x0400
#define PROXY_WORKER_INITIALIZED_FLAG 'O'
#define PROXY_WORKER_IGNORE_ERRORS_FLAG 'I'
#define PROXY_WORKER_DRAIN_FLAG 'N'
#define PROXY_WORKER_GENERIC_FLAG 'G'
#define PROXY_WORKER_IN_SHUTDOWN_FLAG 'U'
#define PROXY_WORKER_DISABLED_FLAG 'D'
#define PROXY_WORKER_STOPPED_FLAG 'S'
#define PROXY_WORKER_IN_ERROR_FLAG 'E'
#define PROXY_WORKER_HOT_STANDBY_FLAG 'H'
#define PROXY_WORKER_FREE_FLAG 'F'
#define PROXY_WORKER_HC_FAIL_FLAG 'C'
#define PROXY_WORKER_NOT_USABLE_BITMAP ( PROXY_WORKER_IN_SHUTDOWN | PROXY_WORKER_DISABLED | PROXY_WORKER_STOPPED | PROXY_WORKER_IN_ERROR | PROXY_WORKER_HC_FAIL )
#define PROXY_WORKER_IS_INITIALIZED(f) ( (f)->s->status & PROXY_WORKER_INITIALIZED )
#define PROXY_WORKER_IS_STANDBY(f) ( (f)->s->status & PROXY_WORKER_HOT_STANDBY )
#define PROXY_WORKER_IS_USABLE(f) ( ( !( (f)->s->status & PROXY_WORKER_NOT_USABLE_BITMAP) ) && PROXY_WORKER_IS_INITIALIZED(f) )
#define PROXY_WORKER_IS_DRAINING(f) ( (f)->s->status & PROXY_WORKER_DRAIN )
#define PROXY_WORKER_IS_GENERIC(f) ( (f)->s->status & PROXY_WORKER_GENERIC )
#define PROXY_WORKER_IS_HCFAILED(f) ( (f)->s->status & PROXY_WORKER_HC_FAIL )
#define PROXY_WORKER_IS(f, b) ( (f)->s->status & (b) )
#define PROXY_WORKER_DEFAULT_RETRY 60
#define PROXY_WORKER_MAX_SCHEME_SIZE 16
#define PROXY_WORKER_MAX_ROUTE_SIZE 64
#define PROXY_BALANCER_MAX_ROUTE_SIZE PROXY_WORKER_MAX_ROUTE_SIZE
#define PROXY_WORKER_MAX_NAME_SIZE 96
#define PROXY_BALANCER_MAX_NAME_SIZE PROXY_WORKER_MAX_NAME_SIZE
#define PROXY_WORKER_MAX_HOSTNAME_SIZE 64
#define PROXY_BALANCER_MAX_HOSTNAME_SIZE PROXY_WORKER_MAX_HOSTNAME_SIZE
#define PROXY_BALANCER_MAX_STICKY_SIZE 64
#define PROXY_WORKER_RFC1035_NAME_SIZE 512
#define PROXY_MAX_PROVIDER_NAME_SIZE 16
#define PROXY_STRNCPY(dst, src) ap_proxy_strncpy((dst), (src), (sizeof(dst)))
#define PROXY_COPY_CONF_PARAMS(w, c) do { (w)->s->timeout = (c)->timeout; (w)->s->timeout_set = (c)->timeout_set; (w)->s->recv_buffer_size = (c)->recv_buffer_size; (w)->s->recv_buffer_size_set = (c)->recv_buffer_size_set; (w)->s->io_buffer_size = (c)->io_buffer_size; (w)->s->io_buffer_size_set = (c)->io_buffer_size_set; } while (0)
typedef struct {
unsigned int def;
unsigned int fnv;
} proxy_hashes ;
typedef struct {
char name[PROXY_WORKER_MAX_NAME_SIZE];
char scheme[PROXY_WORKER_MAX_SCHEME_SIZE];
char hostname[PROXY_WORKER_MAX_HOSTNAME_SIZE];
char route[PROXY_WORKER_MAX_ROUTE_SIZE];
char redirect[PROXY_WORKER_MAX_ROUTE_SIZE];
char flusher[PROXY_WORKER_MAX_SCHEME_SIZE];
char uds_path[PROXY_WORKER_MAX_NAME_SIZE];
int lbset;
int retries;
int lbstatus;
int lbfactor;
int min;
int smax;
int hmax;
int flush_wait;
int index;
proxy_hashes hash;
unsigned int status;
enum {
flush_off,
flush_on,
flush_auto
} flush_packets;
apr_time_t updated;
apr_time_t error_time;
apr_interval_time_t ttl;
apr_interval_time_t retry;
apr_interval_time_t timeout;
apr_interval_time_t acquire;
apr_interval_time_t ping_timeout;
apr_interval_time_t conn_timeout;
apr_size_t recv_buffer_size;
apr_size_t io_buffer_size;
apr_size_t elected;
apr_size_t busy;
apr_port_t port;
apr_off_t transferred;
apr_off_t read;
void *context;
unsigned int keepalive:1;
unsigned int disablereuse:1;
unsigned int is_address_reusable:1;
unsigned int retry_set:1;
unsigned int timeout_set:1;
unsigned int acquire_set:1;
unsigned int ping_timeout_set:1;
unsigned int conn_timeout_set:1;
unsigned int recv_buffer_size_set:1;
unsigned int io_buffer_size_set:1;
unsigned int keepalive_set:1;
unsigned int disablereuse_set:1;
unsigned int was_malloced:1;
char hcuri[PROXY_WORKER_MAX_ROUTE_SIZE];
char hcexpr[PROXY_WORKER_MAX_SCHEME_SIZE];
int passes;
int pcount;
int fails;
int fcount;
hcmethod_t method;
apr_interval_time_t interval;
char upgrade[PROXY_WORKER_MAX_SCHEME_SIZE];
} proxy_worker_shared;
#define ALIGNED_PROXY_WORKER_SHARED_SIZE (APR_ALIGN_DEFAULT(sizeof(proxy_worker_shared)))
struct proxy_worker {
proxy_hashes hash;
unsigned int local_status;
proxy_conn_pool *cp;
proxy_worker_shared *s;
proxy_balancer *balancer;
apr_thread_mutex_t *tmutex;
void *context;
};
#define HCHECK_WATHCHDOG_DEFAULT_INTERVAL (30)
#define HCHECK_WATHCHDOG_INTERVAL (2)
#define PROXY_FLUSH_WAIT 10000
typedef struct {
char sticky_path[PROXY_BALANCER_MAX_STICKY_SIZE];
char sticky[PROXY_BALANCER_MAX_STICKY_SIZE];
char lbpname[PROXY_MAX_PROVIDER_NAME_SIZE];
char nonce[APR_UUID_FORMATTED_LENGTH + 1];
char name[PROXY_BALANCER_MAX_NAME_SIZE];
char sname[PROXY_BALANCER_MAX_NAME_SIZE];
char vpath[PROXY_BALANCER_MAX_ROUTE_SIZE];
char vhost[PROXY_BALANCER_MAX_HOSTNAME_SIZE];
apr_interval_time_t timeout;
apr_time_t wupdated;
int max_attempts;
int index;
proxy_hashes hash;
unsigned int sticky_force:1;
unsigned int scolonsep:1;
unsigned int max_attempts_set:1;
unsigned int was_malloced:1;
unsigned int need_reset:1;
unsigned int vhosted:1;
unsigned int inactive:1;
unsigned int forcerecovery:1;
char sticky_separator;
} proxy_balancer_shared;
#define ALIGNED_PROXY_BALANCER_SHARED_SIZE (APR_ALIGN_DEFAULT(sizeof(proxy_balancer_shared)))
struct proxy_balancer {
apr_array_header_t *workers;
apr_array_header_t *errstatuses;
ap_slotmem_instance_t *wslot;
ap_slotmem_provider_t *storage;
int growth;
int max_workers;
proxy_hashes hash;
apr_time_t wupdated;
proxy_balancer_method *lbmethod;
apr_global_mutex_t *gmutex;
apr_thread_mutex_t *tmutex;
proxy_server_conf *sconf;
void *context;
proxy_balancer_shared *s;
int failontimeout;
};
struct proxy_balancer_method {
const char *name;
proxy_worker *(*finder)(proxy_balancer *balancer,
request_rec *r);
void *context;
apr_status_t (*reset)(proxy_balancer *balancer, server_rec *s);
apr_status_t (*age)(proxy_balancer *balancer, server_rec *s);
apr_status_t (*updatelbstatus)(proxy_balancer *balancer, proxy_worker *elected, server_rec *s);
};
#define PROXY_THREAD_LOCK(x) ( (x) && (x)->tmutex ? apr_thread_mutex_lock((x)->tmutex) : APR_SUCCESS)
#define PROXY_THREAD_UNLOCK(x) ( (x) && (x)->tmutex ? apr_thread_mutex_unlock((x)->tmutex) : APR_SUCCESS)
#define PROXY_GLOBAL_LOCK(x) ( (x) && (x)->gmutex ? apr_global_mutex_lock((x)->gmutex) : APR_SUCCESS)
#define PROXY_GLOBAL_UNLOCK(x) ( (x) && (x)->gmutex ? apr_global_mutex_unlock((x)->gmutex) : APR_SUCCESS)
#if !defined(WIN32)
#define PROXY_DECLARE(type) type
#define PROXY_DECLARE_NONSTD(type) type
#define PROXY_DECLARE_DATA
#elif defined(PROXY_DECLARE_STATIC)
#define PROXY_DECLARE(type) type __stdcall
#define PROXY_DECLARE_NONSTD(type) type
#define PROXY_DECLARE_DATA
#elif defined(PROXY_DECLARE_EXPORT)
#define PROXY_DECLARE(type) __declspec(dllexport) type __stdcall
#define PROXY_DECLARE_NONSTD(type) __declspec(dllexport) type
#define PROXY_DECLARE_DATA __declspec(dllexport)
#else
#define PROXY_DECLARE(type) __declspec(dllimport) type __stdcall
#define PROXY_DECLARE_NONSTD(type) __declspec(dllimport) type
#define PROXY_DECLARE_DATA __declspec(dllimport)
#endif
#define PROXY_DECLARE_OPTIONAL_HOOK APR_DECLARE_EXTERNAL_HOOK
extern PROXY_DECLARE_DATA proxy_hcmethods_t proxy_hcmethods[];
extern PROXY_DECLARE_DATA proxy_wstat_t proxy_wstat_tbl[];
APR_DECLARE_OPTIONAL_FN(void, hc_show_exprs, (request_rec *));
APR_DECLARE_OPTIONAL_FN(void, hc_select_exprs, (request_rec *, const char *));
APR_DECLARE_OPTIONAL_FN(int, hc_valid_expr, (request_rec *, const char *));
APR_DECLARE_OPTIONAL_FN(const char *, set_worker_hc_param,
(apr_pool_t *, server_rec *, proxy_worker *,
const char *, const char *, void *));
APR_DECLARE_EXTERNAL_HOOK(proxy, PROXY, int, scheme_handler, (request_rec *r,
proxy_worker *worker, proxy_server_conf *conf, char *url,
const char *proxyhost, apr_port_t proxyport))
APR_DECLARE_EXTERNAL_HOOK(proxy, PROXY, int, canon_handler, (request_rec *r,
char *url))
APR_DECLARE_EXTERNAL_HOOK(proxy, PROXY, int, create_req, (request_rec *r, request_rec *pr))
APR_DECLARE_EXTERNAL_HOOK(proxy, PROXY, int, fixups, (request_rec *r))
APR_DECLARE_EXTERNAL_HOOK(proxy, PROXY, int, pre_request, (proxy_worker **worker,
proxy_balancer **balancer,
request_rec *r,
proxy_server_conf *conf, char **url))
APR_DECLARE_EXTERNAL_HOOK(proxy, PROXY, int, post_request, (proxy_worker *worker,
proxy_balancer *balancer, request_rec *r,
proxy_server_conf *conf))
APR_DECLARE_EXTERNAL_HOOK(proxy, PROXY, int, request_status,
(int *status, request_rec *r))
PROXY_DECLARE(apr_status_t) ap_proxy_strncpy(char *dst, const char *src,
apr_size_t dlen);
PROXY_DECLARE(int) ap_proxy_hex2c(const char *x);
PROXY_DECLARE(void) ap_proxy_c2hex(int ch, char *x);
PROXY_DECLARE(char *)ap_proxy_canonenc(apr_pool_t *p, const char *x, int len, enum enctype t,
int forcedec, int proxyreq);
PROXY_DECLARE(char *)ap_proxy_canon_netloc(apr_pool_t *p, char **const urlp, char **userp,
char **passwordp, char **hostp, apr_port_t *port);
PROXY_DECLARE(int) ap_proxyerror(request_rec *r, int statuscode, const char *message);
PROXY_DECLARE(int) ap_proxy_checkproxyblock(request_rec *r, proxy_server_conf *conf, apr_sockaddr_t *uri_addr);
PROXY_DECLARE(int) ap_proxy_checkproxyblock2(request_rec *r, proxy_server_conf *conf,
const char *hostname, apr_sockaddr_t *addr);
PROXY_DECLARE(int) ap_proxy_pre_http_request(conn_rec *c, request_rec *r);
PROXY_DECLARE(int) ap_proxy_connect_to_backend(apr_socket_t **, const char *, apr_sockaddr_t *, const char *, proxy_server_conf *, request_rec *);
PROXY_DECLARE(apr_status_t) ap_proxy_ssl_connection_cleanup(proxy_conn_rec *conn,
request_rec *r);
PROXY_DECLARE(int) ap_proxy_ssl_enable(conn_rec *c);
PROXY_DECLARE(int) ap_proxy_ssl_disable(conn_rec *c);
PROXY_DECLARE(int) ap_proxy_conn_is_https(conn_rec *c);
PROXY_DECLARE(const char *) ap_proxy_ssl_val(apr_pool_t *p, server_rec *s, conn_rec *c, request_rec *r, const char *var);
PROXY_DECLARE(const char *) ap_proxy_location_reverse_map(request_rec *r, proxy_dir_conf *conf, const char *url);
PROXY_DECLARE(const char *) ap_proxy_cookie_reverse_map(request_rec *r, proxy_dir_conf *conf, const char *str);
#if !defined(WIN32)
typedef const char *(*ap_proxy_header_reverse_map_fn)(request_rec *,
proxy_dir_conf *, const char *);
#elif defined(PROXY_DECLARE_STATIC)
typedef const char *(__stdcall *ap_proxy_header_reverse_map_fn)(request_rec *,
proxy_dir_conf *, const char *);
#elif defined(PROXY_DECLARE_EXPORT)
typedef __declspec(dllexport) const char *
(__stdcall *ap_proxy_header_reverse_map_fn)(request_rec *,
proxy_dir_conf *, const char *);
#else
typedef __declspec(dllimport) const char *
(__stdcall *ap_proxy_header_reverse_map_fn)(request_rec *,
proxy_dir_conf *, const char *);
#endif
PROXY_DECLARE(char *) ap_proxy_worker_name(apr_pool_t *p,
proxy_worker *worker);
PROXY_DECLARE(proxy_worker *) ap_proxy_get_worker(apr_pool_t *p,
proxy_balancer *balancer,
proxy_server_conf *conf,
const char *url);
PROXY_DECLARE(char *) ap_proxy_define_worker(apr_pool_t *p,
proxy_worker **worker,
proxy_balancer *balancer,
proxy_server_conf *conf,
const char *url,
int do_malloc);
PROXY_DECLARE(apr_status_t) ap_proxy_share_worker(proxy_worker *worker,
proxy_worker_shared *shm,
int i);
PROXY_DECLARE(apr_status_t) ap_proxy_initialize_worker(proxy_worker *worker,
server_rec *s,
apr_pool_t *p);
PROXY_DECLARE(int) ap_proxy_valid_balancer_name(char *name, int i);
PROXY_DECLARE(proxy_balancer *) ap_proxy_get_balancer(apr_pool_t *p,
proxy_server_conf *conf,
const char *url,
int careactive);
PROXY_DECLARE(char *) ap_proxy_update_balancer(apr_pool_t *p,
proxy_balancer *balancer,
const char *url);
PROXY_DECLARE(char *) ap_proxy_define_balancer(apr_pool_t *p,
proxy_balancer **balancer,
proxy_server_conf *conf,
const char *url,
const char *alias,
int do_malloc);
PROXY_DECLARE(apr_status_t) ap_proxy_share_balancer(proxy_balancer *balancer,
proxy_balancer_shared *shm,
int i);
PROXY_DECLARE(apr_status_t) ap_proxy_initialize_balancer(proxy_balancer *balancer,
server_rec *s,
apr_pool_t *p);
PROXY_DECLARE(proxy_worker_shared *) ap_proxy_find_workershm(ap_slotmem_provider_t *storage,
ap_slotmem_instance_t *slot,
proxy_worker *worker,
unsigned int *index);
PROXY_DECLARE(proxy_balancer_shared *) ap_proxy_find_balancershm(ap_slotmem_provider_t *storage,
ap_slotmem_instance_t *slot,
proxy_balancer *balancer,
unsigned int *index);
PROXY_DECLARE(int) ap_proxy_pre_request(proxy_worker **worker,
proxy_balancer **balancer,
request_rec *r,
proxy_server_conf *conf,
char **url);
PROXY_DECLARE(int) ap_proxy_post_request(proxy_worker *worker,
proxy_balancer *balancer,
request_rec *r,
proxy_server_conf *conf);
PROXY_DECLARE(int) ap_proxy_determine_connection(apr_pool_t *p, request_rec *r,
proxy_server_conf *conf,
proxy_worker *worker,
proxy_conn_rec *conn,
apr_uri_t *uri,
char **url,
const char *proxyname,
apr_port_t proxyport,
char *server_portstr,
int server_portstr_size);
APR_DECLARE_OPTIONAL_FN(int, ap_proxy_retry_worker,
(const char *proxy_function, proxy_worker *worker, server_rec *s));
PROXY_DECLARE(int) ap_proxy_acquire_connection(const char *proxy_function,
proxy_conn_rec **conn,
proxy_worker *worker,
server_rec *s);
PROXY_DECLARE(int) ap_proxy_release_connection(const char *proxy_function,
proxy_conn_rec *conn,
server_rec *s);
#define PROXY_CHECK_CONN_EMPTY (1 << 0)
PROXY_DECLARE(apr_status_t) ap_proxy_check_connection(const char *scheme,
proxy_conn_rec *conn,
server_rec *server,
unsigned max_blank_lines,
int flags);
PROXY_DECLARE(int) ap_proxy_connect_backend(const char *proxy_function,
proxy_conn_rec *conn,
proxy_worker *worker,
server_rec *s);
PROXY_DECLARE(apr_status_t) ap_proxy_connect_uds(apr_socket_t *sock,
const char *uds_path,
apr_pool_t *p);
PROXY_DECLARE(int) ap_proxy_connection_create(const char *proxy_function,
proxy_conn_rec *conn,
conn_rec *c, server_rec *s);
PROXY_DECLARE(int) ap_proxy_connection_reusable(proxy_conn_rec *conn);
PROXY_DECLARE(void) ap_proxy_backend_broke(request_rec *r,
apr_bucket_brigade *brigade);
typedef enum { PROXY_HASHFUNC_DEFAULT, PROXY_HASHFUNC_APR, PROXY_HASHFUNC_FNV } proxy_hash_t;
PROXY_DECLARE(unsigned int) ap_proxy_hashfunc(const char *str, proxy_hash_t method);
PROXY_DECLARE(apr_status_t) ap_proxy_set_wstatus(char c, int set, proxy_worker *w);
PROXY_DECLARE(char *) ap_proxy_parse_wstatus(apr_pool_t *p, proxy_worker *w);
PROXY_DECLARE(apr_status_t) ap_proxy_sync_balancer(proxy_balancer *b,
server_rec *s,
proxy_server_conf *conf);
PROXY_DECLARE(int) ap_proxy_trans_match(request_rec *r,
struct proxy_alias *ent,
proxy_dir_conf *dconf);
PROXY_DECLARE(int) ap_proxy_create_hdrbrgd(apr_pool_t *p,
apr_bucket_brigade *header_brigade,
request_rec *r,
proxy_conn_rec *p_conn,
proxy_worker *worker,
proxy_server_conf *conf,
apr_uri_t *uri,
char *url, char *server_portstr,
char **old_cl_val,
char **old_te_val);
PROXY_DECLARE(int) ap_proxy_pass_brigade(apr_bucket_alloc_t *bucket_alloc,
request_rec *r, proxy_conn_rec *p_conn,
conn_rec *origin, apr_bucket_brigade *bb,
int flush);
APR_DECLARE_OPTIONAL_FN(int, ap_proxy_clear_connection,
(request_rec *r, apr_table_t *headers));
PROXY_DECLARE(int) ap_proxy_is_socket_connected(apr_socket_t *socket);
#define PROXY_LBMETHOD "proxylbmethod"
#define PROXY_DYNAMIC_BALANCER_LIMIT 16
int ap_proxy_lb_workers(void);
PROXY_DECLARE(apr_port_t) ap_proxy_port_of_scheme(const char *scheme);
PROXY_DECLARE (const char *) ap_proxy_show_hcmethod(hcmethod_t method);
PROXY_DECLARE(const char *) ap_proxy_de_socketfy(apr_pool_t *p, const char *url);
PROXY_DECLARE(apr_status_t) ap_proxy_buckets_lifetime_transform(request_rec *r,
apr_bucket_brigade *from,
apr_bucket_brigade *to);
PROXY_DECLARE(apr_status_t) ap_proxy_transfer_between_connections(
request_rec *r,
conn_rec *c_i,
conn_rec *c_o,
apr_bucket_brigade *bb_i,
apr_bucket_brigade *bb_o,
const char *name,
int *sent,
apr_off_t bsize,
int after);
extern module PROXY_DECLARE_DATA proxy_module;
#endif