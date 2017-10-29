#if !defined(__mod_h2__h2_ctx__)
#define __mod_h2__h2_ctx__
struct h2_session;
struct h2_task;
struct h2_config;
typedef struct h2_ctx {
const char *protocol;
struct h2_session *session;
struct h2_task *task;
const char *hostname;
server_rec *server;
const struct h2_config *config;
} h2_ctx;
h2_ctx *h2_ctx_get(const conn_rec *c, int create);
void h2_ctx_clear(const conn_rec *c);
h2_ctx *h2_ctx_rget(const request_rec *r);
h2_ctx *h2_ctx_create_for(const conn_rec *c, struct h2_task *task);
h2_ctx *h2_ctx_protocol_set(h2_ctx *ctx, const char *proto);
h2_ctx *h2_ctx_server_set(h2_ctx *ctx, server_rec *s);
server_rec *h2_ctx_server_get(h2_ctx *ctx);
struct h2_session *h2_ctx_session_get(h2_ctx *ctx);
void h2_ctx_session_set(h2_ctx *ctx, struct h2_session *session);
const char *h2_ctx_protocol_get(const conn_rec *c);
int h2_ctx_is_task(h2_ctx *ctx);
struct h2_task *h2_ctx_get_task(h2_ctx *ctx);
struct h2_task *h2_ctx_cget_task(conn_rec *c);
struct h2_task *h2_ctx_rget_task(request_rec *r);
#endif
