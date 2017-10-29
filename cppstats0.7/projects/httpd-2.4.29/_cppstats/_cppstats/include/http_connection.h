#if !defined(APACHE_HTTP_CONNECTION_H)
#define APACHE_HTTP_CONNECTION_H
#include "apr_network_io.h"
#include "apr_buckets.h"
#if defined(__cplusplus)
extern "C" {
#endif
AP_CORE_DECLARE(void) ap_process_connection(conn_rec *c, void *csd);
AP_CORE_DECLARE(apr_status_t) ap_shutdown_conn(conn_rec *c, int flush);
AP_CORE_DECLARE(void) ap_flush_conn(conn_rec *c);
AP_DECLARE(void) ap_lingering_close(conn_rec *c);
AP_DECLARE(int) ap_prep_lingering_close(conn_rec *c);
AP_DECLARE(int) ap_start_lingering_close(conn_rec *c);
AP_DECLARE_HOOK(conn_rec *, create_connection,
(apr_pool_t *p, server_rec *server, apr_socket_t *csd,
long conn_id, void *sbh, apr_bucket_alloc_t *alloc))
AP_DECLARE_HOOK(int,pre_connection,(conn_rec *c, void *csd))
AP_DECLARE_HOOK(int,process_connection,(conn_rec *c))
AP_DECLARE_HOOK(int,pre_close_connection,(conn_rec *c))
AP_DECLARE_DATA extern const apr_bucket_type_t ap_bucket_type_eoc;
#define AP_BUCKET_IS_EOC(e) (e->type == &ap_bucket_type_eoc)
AP_DECLARE(apr_bucket *) ap_bucket_eoc_make(apr_bucket *b);
AP_DECLARE(apr_bucket *) ap_bucket_eoc_create(apr_bucket_alloc_t *list);
#if defined(__cplusplus)
}
#endif
#endif
