#if !defined(__mod_h2__h2_stream__)
#define __mod_h2__h2_stream__
#include "h2.h"
struct h2_mplx;
struct h2_priority;
struct h2_request;
struct h2_headers;
struct h2_session;
struct h2_task;
struct h2_bucket_beam;
typedef struct h2_stream h2_stream;
typedef void h2_stream_state_cb(void *ctx, h2_stream *stream);
typedef void h2_stream_event_cb(void *ctx, h2_stream *stream,
h2_stream_event_t ev);
typedef struct h2_stream_monitor {
void *ctx;
h2_stream_state_cb *on_state_enter;
h2_stream_state_cb *on_state_invalid;
h2_stream_event_cb *on_state_event;
h2_stream_event_cb *on_event;
} h2_stream_monitor;
struct h2_stream {
int id;
int initiated_on;
apr_pool_t *pool;
struct h2_session *session;
h2_stream_state_t state;
apr_time_t created;
const struct h2_request *request;
struct h2_request *rtmp;
apr_table_t *trailers;
int request_headers_added;
struct h2_bucket_beam *input;
apr_bucket_brigade *in_buffer;
int in_window_size;
apr_time_t in_last_write;
struct h2_bucket_beam *output;
apr_bucket_brigade *out_buffer;
apr_size_t max_mem;
int rst_error;
unsigned int aborted : 1;
unsigned int scheduled : 1;
unsigned int has_response : 1;
unsigned int input_eof : 1;
unsigned int out_checked : 1;
unsigned int push_policy;
struct h2_task *task;
const h2_priority *pref_priority;
apr_off_t out_data_frames;
apr_off_t out_data_octets;
apr_off_t in_data_frames;
apr_off_t in_data_octets;
h2_stream_monitor *monitor;
};
#define H2_STREAM_RST(s, def) (s->rst_error? s->rst_error : (def))
h2_stream *h2_stream_create(int id, apr_pool_t *pool,
struct h2_session *session,
h2_stream_monitor *monitor,
int initiated_on);
void h2_stream_destroy(h2_stream *stream);
apr_status_t h2_stream_prep_processing(h2_stream *stream);
void h2_stream_set_monitor(h2_stream *stream, h2_stream_monitor *monitor);
void h2_stream_dispatch(h2_stream *stream, h2_stream_event_t ev);
void h2_stream_cleanup(h2_stream *stream);
apr_pool_t *h2_stream_detach_pool(h2_stream *stream);
apr_status_t h2_stream_in_consumed(h2_stream *stream, apr_off_t amount);
void h2_stream_set_request(h2_stream *stream, const h2_request *r);
apr_status_t h2_stream_set_request_rec(h2_stream *stream,
request_rec *r, int eos);
apr_status_t h2_stream_add_header(h2_stream *stream,
const char *name, size_t nlen,
const char *value, size_t vlen);
apr_status_t h2_stream_send_frame(h2_stream *stream, int frame_type, int flags);
apr_status_t h2_stream_recv_frame(h2_stream *stream, int frame_type, int flags);
apr_status_t h2_stream_recv_DATA(h2_stream *stream, uint8_t flags,
const uint8_t *data, size_t len);
apr_status_t h2_stream_flush_input(h2_stream *stream);
void h2_stream_rst(h2_stream *stream, int error_code);
int h2_stream_was_closed(const h2_stream *stream);
apr_status_t h2_stream_out_prepare(h2_stream *stream, apr_off_t *plen,
int *peos, h2_headers **presponse);
apr_status_t h2_stream_read_to(h2_stream *stream, apr_bucket_brigade *bb,
apr_off_t *plen, int *peos);
apr_table_t *h2_stream_get_trailers(h2_stream *stream);
apr_status_t h2_stream_submit_pushes(h2_stream *stream, h2_headers *response);
const struct h2_priority *h2_stream_get_priority(h2_stream *stream,
h2_headers *response);
const char *h2_stream_state_str(h2_stream *stream);
int h2_stream_is_ready(h2_stream *stream);
#define H2_STRM_MSG(s, msg) "h2_stream(%ld-%d,%s): "msg, s->session->id, s->id, h2_stream_state_str(s)
#define H2_STRM_LOG(aplogno, s, msg) aplogno H2_STRM_MSG(s, msg)
#endif
