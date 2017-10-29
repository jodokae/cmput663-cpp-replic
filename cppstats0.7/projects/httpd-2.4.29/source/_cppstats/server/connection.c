#include "apr.h"
#include "apr_strings.h"
#include "ap_config.h"
#include "httpd.h"
#include "http_connection.h"
#include "http_request.h"
#include "http_protocol.h"
#include "ap_mpm.h"
#include "http_config.h"
#include "http_core.h"
#include "http_vhost.h"
#include "scoreboard.h"
#include "http_log.h"
#include "util_filter.h"
APR_HOOK_STRUCT(
APR_HOOK_LINK(create_connection)
APR_HOOK_LINK(process_connection)
APR_HOOK_LINK(pre_connection)
APR_HOOK_LINK(pre_close_connection)
)
AP_IMPLEMENT_HOOK_RUN_FIRST(conn_rec *,create_connection,
(apr_pool_t *p, server_rec *server, apr_socket_t *csd, long conn_id, void *sbh, apr_bucket_alloc_t *alloc),
(p, server, csd, conn_id, sbh, alloc), NULL)
AP_IMPLEMENT_HOOK_RUN_FIRST(int,process_connection,(conn_rec *c),(c),DECLINED)
AP_IMPLEMENT_HOOK_RUN_ALL(int,pre_connection,(conn_rec *c, void *csd),(c, csd),OK,DECLINED)
AP_IMPLEMENT_HOOK_RUN_ALL(int,pre_close_connection,(conn_rec *c),(c),OK,DECLINED)
#if !defined(MAX_SECS_TO_LINGER)
#define MAX_SECS_TO_LINGER 30
#endif
AP_CORE_DECLARE(apr_status_t) ap_shutdown_conn(conn_rec *c, int flush) {
apr_status_t rv;
apr_bucket_brigade *bb;
apr_bucket *b;
bb = apr_brigade_create(c->pool, c->bucket_alloc);
if (flush) {
b = apr_bucket_flush_create(c->bucket_alloc);
APR_BRIGADE_INSERT_TAIL(bb, b);
}
b = ap_bucket_eoc_create(c->bucket_alloc);
APR_BRIGADE_INSERT_TAIL(bb, b);
rv = ap_pass_brigade(c->output_filters, bb);
apr_brigade_destroy(bb);
return rv;
}
AP_CORE_DECLARE(void) ap_flush_conn(conn_rec *c) {
(void)ap_shutdown_conn(c, 1);
}
AP_DECLARE(int) ap_prep_lingering_close(conn_rec *c) {
ap_run_pre_close_connection(c);
if (c->sbh) {
ap_update_child_status(c->sbh, SERVER_CLOSING, NULL);
}
return 0;
}
#define SECONDS_TO_LINGER 2
AP_DECLARE(int) ap_start_lingering_close(conn_rec *c) {
apr_socket_t *csd = ap_get_conn_socket(c);
if (!csd) {
return 1;
}
if (ap_prep_lingering_close(c)) {
return 1;
}
#if defined(NO_LINGCLOSE)
ap_flush_conn(c);
apr_socket_close(csd);
return 1;
#endif
ap_flush_conn(c);
if (c->aborted) {
apr_socket_close(csd);
return 1;
}
if (apr_socket_shutdown(csd, APR_SHUTDOWN_WRITE) != APR_SUCCESS
|| c->aborted) {
apr_socket_close(csd);
return 1;
}
return 0;
}
AP_DECLARE(void) ap_lingering_close(conn_rec *c) {
char dummybuf[512];
apr_size_t nbytes;
apr_time_t now, timeup = 0;
apr_socket_t *csd = ap_get_conn_socket(c);
if (ap_start_lingering_close(c)) {
return;
}
apr_socket_timeout_set(csd, apr_time_from_sec(SECONDS_TO_LINGER));
apr_socket_opt_set(csd, APR_INCOMPLETE_READ, 1);
do {
nbytes = sizeof(dummybuf);
if (apr_socket_recv(csd, dummybuf, &nbytes) || nbytes == 0)
break;
now = apr_time_now();
if (timeup == 0) {
if (apr_table_get(c->notes, "short-lingering-close")) {
timeup = now + apr_time_from_sec(SECONDS_TO_LINGER);
} else {
timeup = now + apr_time_from_sec(MAX_SECS_TO_LINGER);
}
continue;
}
} while (now < timeup);
apr_socket_close(csd);
}
AP_CORE_DECLARE(void) ap_process_connection(conn_rec *c, void *csd) {
int rc;
ap_update_vhost_given_ip(c);
rc = ap_run_pre_connection(c, csd);
if (rc != OK && rc != DONE) {
c->aborted = 1;
}
if (!c->aborted) {
ap_run_process_connection(c);
}
}