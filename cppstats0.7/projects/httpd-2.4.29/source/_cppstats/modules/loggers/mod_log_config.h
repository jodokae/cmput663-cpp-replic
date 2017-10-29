#include "apr_optional.h"
#include "httpd.h"
#include "scoreboard.h"
#if !defined(_MOD_LOG_CONFIG_H)
#define _MOD_LOG_CONFIG_H 1
typedef const char *ap_log_handler_fn_t(request_rec *r, char *a);
typedef void *ap_log_writer_init(apr_pool_t *p, server_rec *s,
const char *name);
typedef apr_status_t ap_log_writer(
request_rec *r,
void *handle,
const char **portions,
int *lengths,
int nelts,
apr_size_t len);
typedef struct ap_log_handler {
ap_log_handler_fn_t *func;
int want_orig_default;
} ap_log_handler;
APR_DECLARE_OPTIONAL_FN(void, ap_register_log_handler,
(apr_pool_t *p, char *tag, ap_log_handler_fn_t *func,
int def));
APR_DECLARE_OPTIONAL_FN(ap_log_writer_init*, ap_log_set_writer_init,(ap_log_writer_init *func));
APR_DECLARE_OPTIONAL_FN(ap_log_writer*, ap_log_set_writer, (ap_log_writer* func));
#endif