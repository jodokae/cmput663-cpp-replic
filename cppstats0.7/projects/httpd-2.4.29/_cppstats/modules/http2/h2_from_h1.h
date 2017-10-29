#if !defined(__mod_h2__h2_from_h1__)
#define __mod_h2__h2_from_h1__
struct h2_headers;
struct h2_task;
apr_status_t h2_from_h1_parse_response(struct h2_task *task, ap_filter_t *f,
apr_bucket_brigade *bb);
apr_status_t h2_filter_headers_out(ap_filter_t *f, apr_bucket_brigade *bb);
apr_status_t h2_filter_request_in(ap_filter_t* f,
apr_bucket_brigade* brigade,
ap_input_mode_t mode,
apr_read_type_e block,
apr_off_t readbytes);
apr_status_t h2_filter_trailers_out(ap_filter_t *f, apr_bucket_brigade *bb);
#endif