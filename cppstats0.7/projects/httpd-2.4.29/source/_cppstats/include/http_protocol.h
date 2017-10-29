#if !defined(APACHE_HTTP_PROTOCOL_H)
#define APACHE_HTTP_PROTOCOL_H
#include "httpd.h"
#include "apr_portable.h"
#include "apr_mmap.h"
#include "apr_buckets.h"
#include "util_filter.h"
#if defined(__cplusplus)
extern "C" {
#endif
AP_DECLARE_HOOK(void,insert_error_filter,(request_rec *r))
AP_DECLARE_DATA extern ap_filter_rec_t *ap_old_write_func;
request_rec *ap_read_request(conn_rec *c);
AP_DECLARE(void) ap_get_mime_headers(request_rec *r);
AP_DECLARE(void) ap_get_mime_headers_core(request_rec *r,
apr_bucket_brigade *bb);
AP_DECLARE(void) ap_finalize_request_protocol(request_rec *r);
AP_DECLARE(void) ap_send_error_response(request_rec *r, int recursive_error);
AP_DECLARE(void) ap_set_content_length(request_rec *r, apr_off_t length);
AP_DECLARE(int) ap_set_keepalive(request_rec *r);
AP_DECLARE(apr_time_t) ap_rationalize_mtime(request_rec *r, apr_time_t mtime);
AP_DECLARE(const char *) ap_make_content_type(request_rec *r,
const char *type);
AP_DECLARE(void) ap_setup_make_content_type(apr_pool_t *pool);
AP_DECLARE(char *) ap_make_etag(request_rec *r, int force_weak);
AP_DECLARE(void) ap_set_etag(request_rec *r);
AP_DECLARE(void) ap_set_last_modified(request_rec *r);
typedef enum {
AP_CONDITION_NONE,
AP_CONDITION_NOMATCH,
AP_CONDITION_WEAK,
AP_CONDITION_STRONG
} ap_condition_e;
AP_DECLARE(ap_condition_e) ap_condition_if_match(request_rec *r,
apr_table_t *headers);
AP_DECLARE(ap_condition_e) ap_condition_if_unmodified_since(request_rec *r,
apr_table_t *headers);
AP_DECLARE(ap_condition_e) ap_condition_if_none_match(request_rec *r,
apr_table_t *headers);
AP_DECLARE(ap_condition_e) ap_condition_if_modified_since(request_rec *r,
apr_table_t *headers);
AP_DECLARE(ap_condition_e) ap_condition_if_range(request_rec *r,
apr_table_t *headers);
AP_DECLARE(int) ap_meets_conditions(request_rec *r);
AP_DECLARE(apr_status_t) ap_send_fd(apr_file_t *fd, request_rec *r, apr_off_t offset,
apr_size_t length, apr_size_t *nbytes);
#if APR_HAS_MMAP
AP_DECLARE(apr_size_t) ap_send_mmap(apr_mmap_t *mm,
request_rec *r,
apr_size_t offset,
apr_size_t length);
#endif
AP_DECLARE(int) ap_method_register(apr_pool_t *p, const char *methname);
AP_DECLARE(void) ap_method_registry_init(apr_pool_t *p);
#define AP_METHOD_CHECK_ALLOWED(mask, methname) ((mask) & (AP_METHOD_BIT << ap_method_number_of((methname))))
AP_DECLARE(ap_method_list_t *) ap_make_method_list(apr_pool_t *p, int nelts);
AP_DECLARE(void) ap_copy_method_list(ap_method_list_t *dest,
ap_method_list_t *src);
AP_DECLARE(int) ap_method_in_list(ap_method_list_t *l, const char *method);
AP_DECLARE(void) ap_method_list_add(ap_method_list_t *l, const char *method);
AP_DECLARE(void) ap_method_list_remove(ap_method_list_t *l,
const char *method);
AP_DECLARE(void) ap_clear_method_list(ap_method_list_t *l);
AP_DECLARE(void) ap_set_content_type(request_rec *r, const char *ct);
AP_DECLARE(void) ap_set_accept_ranges(request_rec *r);
AP_DECLARE(int) ap_rputc(int c, request_rec *r);
AP_DECLARE(int) ap_rwrite(const void *buf, int nbyte, request_rec *r);
static APR_INLINE int ap_rputs(const char *str, request_rec *r) {
return ap_rwrite(str, (int)strlen(str), r);
}
AP_DECLARE_NONSTD(int) ap_rvputs(request_rec *r,...)
AP_FN_ATTR_SENTINEL;
AP_DECLARE(int) ap_vrprintf(request_rec *r, const char *fmt, va_list vlist);
AP_DECLARE_NONSTD(int) ap_rprintf(request_rec *r, const char *fmt,...)
__attribute__((format(printf,2,3)));
AP_DECLARE(int) ap_rflush(request_rec *r);
AP_DECLARE(int) ap_index_of_response(int status);
AP_DECLARE(const char *) ap_get_status_line(int status);
AP_DECLARE(int) ap_setup_client_block(request_rec *r, int read_policy);
AP_DECLARE(int) ap_should_client_block(request_rec *r);
AP_DECLARE(long) ap_get_client_block(request_rec *r, char *buffer, apr_size_t bufsiz);
AP_DECLARE(int) ap_map_http_request_error(apr_status_t rv, int status);
AP_DECLARE(int) ap_discard_request_body(request_rec *r);
AP_DECLARE(void) ap_note_auth_failure(request_rec *r);
AP_DECLARE(void) ap_note_basic_auth_failure(request_rec *r);
AP_DECLARE(void) ap_note_digest_auth_failure(request_rec *r);
AP_DECLARE_HOOK(int, note_auth_failure, (request_rec *r, const char *auth_type))
AP_DECLARE(int) ap_get_basic_auth_pw(request_rec *r, const char **pw);
#define AP_GET_BASIC_AUTH_PW_NOTE "AP_GET_BASIC_AUTH_PW_NOTE"
AP_DECLARE(apr_status_t) ap_get_basic_auth_components(const request_rec *r,
const char **username,
const char **password);
AP_CORE_DECLARE(void) ap_parse_uri(request_rec *r, const char *uri);
#define AP_GETLINE_FOLD 1
#define AP_GETLINE_CRLF 2
AP_DECLARE(int) ap_getline(char *s, int n, request_rec *r, int flags);
#if APR_CHARSET_EBCDIC
AP_DECLARE(apr_status_t) ap_rgetline(char **s, apr_size_t n,
apr_size_t *read,
request_rec *r, int flags,
apr_bucket_brigade *bb);
#else
#define ap_rgetline(s, n, read, r, fold, bb) ap_rgetline_core((s), (n), (read), (r), (fold), (bb))
#endif
AP_DECLARE(apr_status_t) ap_rgetline_core(char **s, apr_size_t n,
apr_size_t *read,
request_rec *r, int flags,
apr_bucket_brigade *bb);
AP_DECLARE(int) ap_method_number_of(const char *method);
AP_DECLARE(const char *) ap_method_name_of(apr_pool_t *p, int methnum);
AP_DECLARE_HOOK(void,pre_read_request,(request_rec *r, conn_rec *c))
AP_DECLARE_HOOK(int,post_read_request,(request_rec *r))
AP_DECLARE_HOOK(int,log_transaction,(request_rec *r))
AP_DECLARE_HOOK(const char *,http_scheme,(const request_rec *r))
AP_DECLARE_HOOK(apr_port_t,default_port,(const request_rec *r))
#define AP_PROTOCOL_HTTP1 "http/1.1"
AP_DECLARE_HOOK(int,protocol_propose,(conn_rec *c, request_rec *r,
server_rec *s,
const apr_array_header_t *offers,
apr_array_header_t *proposals))
AP_DECLARE_HOOK(int,protocol_switch,(conn_rec *c, request_rec *r,
server_rec *s,
const char *protocol))
AP_DECLARE_HOOK(const char *,protocol_get,(const conn_rec *c))
AP_DECLARE(apr_status_t) ap_get_protocol_upgrades(conn_rec *c, request_rec *r,
server_rec *s, int report_all,
const apr_array_header_t **pupgrades);
AP_DECLARE(const char *) ap_select_protocol(conn_rec *c, request_rec *r,
server_rec *s,
const apr_array_header_t *choices);
AP_DECLARE(apr_status_t) ap_switch_protocol(conn_rec *c, request_rec *r,
server_rec *s,
const char *protocol);
AP_DECLARE(const char *) ap_get_protocol(conn_rec *c);
AP_DECLARE(int) ap_is_allowed_protocol(conn_rec *c, request_rec *r,
server_rec *s, const char *protocol);
typedef struct ap_bucket_error ap_bucket_error;
struct ap_bucket_error {
apr_bucket_refcount refcount;
int status;
const char *data;
};
AP_DECLARE_DATA extern const apr_bucket_type_t ap_bucket_type_error;
#define AP_BUCKET_IS_ERROR(e) (e->type == &ap_bucket_type_error)
AP_DECLARE(apr_bucket *) ap_bucket_error_make(apr_bucket *b, int error,
const char *buf, apr_pool_t *p);
AP_DECLARE(apr_bucket *) ap_bucket_error_create(int error, const char *buf,
apr_pool_t *p,
apr_bucket_alloc_t *list);
AP_DECLARE_NONSTD(apr_status_t) ap_byterange_filter(ap_filter_t *f, apr_bucket_brigade *b);
AP_DECLARE_NONSTD(apr_status_t) ap_http_header_filter(ap_filter_t *f, apr_bucket_brigade *b);
AP_DECLARE_NONSTD(apr_status_t) ap_content_length_filter(ap_filter_t *,
apr_bucket_brigade *);
AP_DECLARE_NONSTD(apr_status_t) ap_old_write_filter(ap_filter_t *f, apr_bucket_brigade *b);
AP_DECLARE(void) ap_set_sub_req_protocol(request_rec *rnew, const request_rec *r);
AP_DECLARE(void) ap_finalize_sub_req_protocol(request_rec *sub_r);
AP_DECLARE(void) ap_send_interim_response(request_rec *r, int send_headers);
#if defined(__cplusplus)
}
#endif
#endif