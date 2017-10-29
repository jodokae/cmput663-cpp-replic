#if !defined(__MOD_HTTP2_H__)
#define __MOD_HTTP2_H__
APR_DECLARE_OPTIONAL_FN(char *,
http2_var_lookup, (apr_pool_t *, server_rec *,
conn_rec *, request_rec *, char *));
APR_DECLARE_OPTIONAL_FN(int,
http2_is_h2, (conn_rec *));
struct apr_thread_cond_t;
typedef struct h2_req_engine h2_req_engine;
typedef void http2_output_consumed(void *ctx, conn_rec *c, apr_off_t consumed);
typedef apr_status_t http2_req_engine_init(h2_req_engine *engine,
const char *id,
const char *type,
apr_pool_t *pool,
apr_size_t req_buffer_size,
request_rec *r,
http2_output_consumed **pconsumed,
void **pbaton);
APR_DECLARE_OPTIONAL_FN(apr_status_t,
http2_req_engine_push, (const char *engine_type,
request_rec *r,
http2_req_engine_init *einit));
APR_DECLARE_OPTIONAL_FN(apr_status_t,
http2_req_engine_pull, (h2_req_engine *engine,
apr_read_type_e block,
int capacity,
request_rec **pr));
APR_DECLARE_OPTIONAL_FN(void,
http2_req_engine_done, (h2_req_engine *engine,
conn_rec *rconn,
apr_status_t status));
#endif