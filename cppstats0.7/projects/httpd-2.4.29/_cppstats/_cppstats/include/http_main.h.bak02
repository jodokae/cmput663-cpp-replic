#if !defined(APACHE_HTTP_MAIN_H)
#define APACHE_HTTP_MAIN_H
#include "httpd.h"
#include "apr_optional.h"
#define AP_SERVER_BASEARGS "C:c:D:d:E:e:f:vVlLtTSMh?X"
#if defined(__cplusplus)
extern "C" {
#endif
AP_DECLARE_DATA extern const char *ap_server_argv0;
AP_DECLARE_DATA extern const char *ap_server_root;
AP_DECLARE_DATA extern const char *ap_runtime_dir;
AP_DECLARE_DATA extern server_rec *ap_server_conf;
AP_DECLARE_DATA extern apr_pool_t *ap_pglobal;
AP_DECLARE_DATA extern int ap_main_state;
AP_DECLARE_DATA extern int ap_run_mode;
AP_DECLARE_DATA extern int ap_config_generation;
AP_DECLARE_DATA extern apr_array_header_t *ap_server_pre_read_config;
AP_DECLARE_DATA extern apr_array_header_t *ap_server_post_read_config;
AP_DECLARE_DATA extern apr_array_header_t *ap_server_config_defines;
AP_DECLARE_DATA extern int ap_document_root_check;
APR_DECLARE_OPTIONAL_FN(int, ap_signal_server, (int *status, apr_pool_t *pool));
#if defined(__cplusplus)
}
#endif
#endif
