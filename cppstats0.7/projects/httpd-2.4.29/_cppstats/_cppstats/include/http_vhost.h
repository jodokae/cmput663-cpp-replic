#if !defined(APACHE_HTTP_VHOST_H)
#define APACHE_HTTP_VHOST_H
#if defined(__cplusplus)
extern "C" {
#endif
AP_DECLARE(void) ap_init_vhost_config(apr_pool_t *p);
AP_DECLARE(void) ap_fini_vhost_config(apr_pool_t *p, server_rec *main_server);
const char *ap_parse_vhost_addrs(apr_pool_t *p, const char *hostname, server_rec *s);
AP_DECLARE_NONSTD(const char *)ap_set_name_virtual_host(cmd_parms *cmd,
void *dummy,
const char *arg);
typedef int(*ap_vhost_iterate_conn_cb)(void* baton, conn_rec* conn, server_rec* s);
AP_DECLARE(int) ap_vhost_iterate_given_conn(conn_rec *conn,
ap_vhost_iterate_conn_cb func_cb,
void* baton);
AP_DECLARE(void) ap_update_vhost_given_ip(conn_rec *conn);
AP_DECLARE(void) ap_update_vhost_from_headers(request_rec *r);
AP_DECLARE(int) ap_matches_request_vhost(request_rec *r, const char *host,
apr_port_t port);
#if defined(__cplusplus)
}
#endif
#endif
