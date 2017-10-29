#if !defined(__mod_h2__h2_h2__)
#define __mod_h2__h2_h2__
extern const char *h2_clear_protos[];
extern const char *h2_tls_protos[];
const char *h2_h2_err_description(unsigned int h2_error);
apr_status_t h2_h2_init(apr_pool_t *pool, server_rec *s);
int h2_h2_is_tls(conn_rec *c);
void h2_h2_register_hooks(void);
int h2_is_acceptable_connection(conn_rec *c, int require_all);
int h2_allows_h2_direct(conn_rec *c);
int h2_allows_h2_upgrade(conn_rec *c);
#endif
