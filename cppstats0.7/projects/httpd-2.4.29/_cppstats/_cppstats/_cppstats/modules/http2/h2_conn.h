#if !defined(__mod_h2__h2_conn__)
#define __mod_h2__h2_conn__
struct h2_ctx;
struct h2_task;
apr_status_t h2_conn_setup(struct h2_ctx *ctx, conn_rec *c, request_rec *r);
apr_status_t h2_conn_run(struct h2_ctx *ctx, conn_rec *c);
apr_status_t h2_conn_pre_close(struct h2_ctx *ctx, conn_rec *c);
apr_status_t h2_conn_child_init(apr_pool_t *pool, server_rec *s);
typedef enum {
H2_MPM_UNKNOWN,
H2_MPM_WORKER,
H2_MPM_EVENT,
H2_MPM_PREFORK,
H2_MPM_MOTORZ,
H2_MPM_SIMPLE,
H2_MPM_NETWARE,
H2_MPM_WINNT,
} h2_mpm_type_t;
h2_mpm_type_t h2_conn_mpm_type(void);
const char *h2_conn_mpm_name(void);
int h2_mpm_supported(void);
conn_rec *h2_slave_create(conn_rec *master, int slave_id, apr_pool_t *parent);
void h2_slave_destroy(conn_rec *slave);
apr_status_t h2_slave_run_pre_connection(conn_rec *slave, apr_socket_t *csd);
void h2_slave_run_connection(conn_rec *slave);
#endif