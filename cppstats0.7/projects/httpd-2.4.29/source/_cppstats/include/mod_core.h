#if !defined(MOD_CORE_H)
#define MOD_CORE_H
#include "apr.h"
#include "apr_buckets.h"
#include "httpd.h"
#include "util_filter.h"
#if defined(__cplusplus)
extern "C" {
#endif
AP_DECLARE_DATA extern ap_filter_rec_t *ap_http_input_filter_handle;
AP_DECLARE_DATA extern ap_filter_rec_t *ap_http_header_filter_handle;
AP_DECLARE_DATA extern ap_filter_rec_t *ap_chunk_filter_handle;
AP_DECLARE_DATA extern ap_filter_rec_t *ap_http_outerror_filter_handle;
AP_DECLARE_DATA extern ap_filter_rec_t *ap_byterange_filter_handle;
apr_status_t ap_http_filter(ap_filter_t *f, apr_bucket_brigade *b,
ap_input_mode_t mode, apr_read_type_e block,
apr_off_t readbytes);
apr_status_t ap_http_chunk_filter(ap_filter_t *f, apr_bucket_brigade *b);
apr_status_t ap_http_outerror_filter(ap_filter_t *f,
apr_bucket_brigade *b);
char *ap_response_code_string(request_rec *r, int error_index);
AP_DECLARE(void) ap_basic_http_header(request_rec *r, apr_bucket_brigade *bb);
AP_DECLARE_NONSTD(int) ap_send_http_trace(request_rec *r);
AP_DECLARE(int) ap_send_http_options(request_rec *r);
AP_DECLARE_DATA extern const char *ap_multipart_boundary;
AP_CORE_DECLARE(void) ap_init_rng(apr_pool_t *p);
AP_CORE_DECLARE(void) ap_random_parent_after_fork(void);
#if defined(__cplusplus)
}
#endif
#endif
