#if !defined(APACHE_UTIL_SCRIPT_H)
#define APACHE_UTIL_SCRIPT_H
#include "apr_buckets.h"
#include "ap_config.h"
#if defined(__cplusplus)
extern "C" {
#endif
#if !defined(APACHE_ARG_MAX)
#if defined(_POSIX_ARG_MAX)
#define APACHE_ARG_MAX _POSIX_ARG_MAX
#else
#define APACHE_ARG_MAX 512
#endif
#endif
AP_DECLARE(char **) ap_create_environment(apr_pool_t *p, apr_table_t *t);
AP_DECLARE(int) ap_find_path_info(const char *uri, const char *path_info);
AP_DECLARE(void) ap_add_cgi_vars(request_rec *r);
AP_DECLARE(void) ap_add_common_vars(request_rec *r);
AP_DECLARE(int) ap_scan_script_header_err(request_rec *r, apr_file_t *f, char *buffer);
AP_DECLARE(int) ap_scan_script_header_err_ex(request_rec *r, apr_file_t *f,
char *buffer, int module_index);
AP_DECLARE(int) ap_scan_script_header_err_brigade(request_rec *r,
apr_bucket_brigade *bb,
char *buffer);
AP_DECLARE(int) ap_scan_script_header_err_brigade_ex(request_rec *r,
apr_bucket_brigade *bb,
char *buffer,
int module_index);
AP_DECLARE_NONSTD(int) ap_scan_script_header_err_strs(request_rec *r,
char *buffer,
const char **termch,
int *termarg, ...)
AP_FN_ATTR_SENTINEL;
AP_DECLARE_NONSTD(int) ap_scan_script_header_err_strs_ex(request_rec *r,
char *buffer,
int module_index,
const char **termch,
int *termarg, ...)
AP_FN_ATTR_SENTINEL;
AP_DECLARE(int) ap_scan_script_header_err_core(request_rec *r, char *buffer,
int (*getsfunc) (char *, int, void *),
void *getsfunc_data);
AP_DECLARE(int) ap_scan_script_header_err_core_ex(request_rec *r, char *buffer,
int (*getsfunc) (char *, int, void *),
void *getsfunc_data, int module_index);
AP_DECLARE(void) ap_args_to_table(request_rec *r, apr_table_t **table);
#if defined(__cplusplus)
}
#endif
#endif
