#if !defined(__mod_h2__h2_config_h__)
#define __mod_h2__h2_config_h__
#undef PACKAGE_VERSION
#undef PACKAGE_TARNAME
#undef PACKAGE_STRING
#undef PACKAGE_NAME
#undef PACKAGE_BUGREPORT
typedef enum {
H2_CONF_MAX_STREAMS,
H2_CONF_WIN_SIZE,
H2_CONF_MIN_WORKERS,
H2_CONF_MAX_WORKERS,
H2_CONF_MAX_WORKER_IDLE_SECS,
H2_CONF_STREAM_MAX_MEM,
H2_CONF_ALT_SVCS,
H2_CONF_ALT_SVC_MAX_AGE,
H2_CONF_SER_HEADERS,
H2_CONF_DIRECT,
H2_CONF_MODERN_TLS_ONLY,
H2_CONF_UPGRADE,
H2_CONF_TLS_WARMUP_SIZE,
H2_CONF_TLS_COOLDOWN_SECS,
H2_CONF_PUSH,
H2_CONF_PUSH_DIARY_SIZE,
H2_CONF_COPY_FILES,
H2_CONF_EARLY_HINTS,
} h2_config_var_t;
struct apr_hash_t;
struct h2_priority;
struct h2_push_res;
typedef struct h2_push_res {
const char *uri_ref;
int critical;
} h2_push_res;
typedef struct h2_config {
const char *name;
int h2_max_streams;
int h2_window_size;
int min_workers;
int max_workers;
int max_worker_idle_secs;
int stream_max_mem_size;
apr_array_header_t *alt_svcs;
int alt_svc_max_age;
int serialize_headers;
int h2_direct;
int modern_tls_only;
int h2_upgrade;
apr_int64_t tls_warmup_size;
int tls_cooldown_secs;
int h2_push;
struct apr_hash_t *priorities;
int push_diary_size;
int copy_files;
apr_array_header_t *push_list;
int early_hints;
} h2_config;
void *h2_config_create_dir(apr_pool_t *pool, char *x);
void *h2_config_merge_dir(apr_pool_t *pool, void *basev, void *addv);
void *h2_config_create_svr(apr_pool_t *pool, server_rec *s);
void *h2_config_merge_svr(apr_pool_t *pool, void *basev, void *addv);
extern const command_rec h2_cmds[];
const h2_config *h2_config_get(conn_rec *c);
const h2_config *h2_config_sget(server_rec *s);
const h2_config *h2_config_rget(request_rec *r);
int h2_config_geti(const h2_config *conf, h2_config_var_t var);
apr_int64_t h2_config_geti64(const h2_config *conf, h2_config_var_t var);
void h2_config_init(apr_pool_t *pool);
const struct h2_priority *h2_config_get_priority(const h2_config *conf,
const char *content_type);
#endif