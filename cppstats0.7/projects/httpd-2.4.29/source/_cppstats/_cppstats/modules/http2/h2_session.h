#if !defined(__mod_h2__h2_session__)
#define __mod_h2__h2_session__
#include "h2_conn_io.h"
#include "h2.h"
struct apr_thread_mutext_t;
struct apr_thread_cond_t;
struct h2_ctx;
struct h2_config;
struct h2_filter_cin;
struct h2_ihash_t;
struct h2_mplx;
struct h2_priority;
struct h2_push;
struct h2_push_diary;
struct h2_session;
struct h2_stream;
struct h2_stream_monitor;
struct h2_task;
struct h2_workers;
struct nghttp2_session;
typedef enum {
H2_SESSION_EV_INIT,
H2_SESSION_EV_LOCAL_GOAWAY,
H2_SESSION_EV_REMOTE_GOAWAY,
H2_SESSION_EV_CONN_ERROR,
H2_SESSION_EV_PROTO_ERROR,
H2_SESSION_EV_CONN_TIMEOUT,
H2_SESSION_EV_NO_IO,
H2_SESSION_EV_DATA_READ,
H2_SESSION_EV_NGH2_DONE,
H2_SESSION_EV_MPM_STOPPING,
H2_SESSION_EV_PRE_CLOSE,
} h2_session_event_t;
typedef struct h2_session {
long id;
conn_rec *c;
request_rec *r;
server_rec *s;
const struct h2_config *config;
apr_pool_t *pool;
struct h2_mplx *mplx;
struct h2_workers *workers;
struct h2_filter_cin *cin;
h2_conn_io io;
struct nghttp2_session *ngh2;
h2_session_state state;
h2_session_props local;
h2_session_props remote;
unsigned int reprioritize : 1;
unsigned int flush : 1;
unsigned int have_read : 1;
unsigned int have_written : 1;
apr_interval_time_t wait_us;
struct h2_push_diary *push_diary;
struct h2_stream_monitor *monitor;
int open_streams;
int unsent_submits;
int unsent_promises;
int responses_submitted;
int streams_reset;
int pushes_promised;
int pushes_submitted;
int pushes_reset;
apr_size_t frames_received;
apr_size_t frames_sent;
apr_size_t max_stream_count;
apr_size_t max_stream_mem;
apr_time_t idle_until;
apr_time_t keep_sync_until;
apr_bucket_brigade *bbtmp;
struct apr_thread_cond_t *iowait;
char status[64];
int last_status_code;
const char *last_status_msg;
struct h2_iqueue *in_pending;
struct h2_iqueue *in_process;
} h2_session;
const char *h2_session_state_str(h2_session_state state);
apr_status_t h2_session_create(h2_session **psession,
conn_rec *c, struct h2_ctx *ctx,
struct h2_workers *workers);
apr_status_t h2_session_rcreate(h2_session **psession,
request_rec *r, struct h2_ctx *ctx,
struct h2_workers *workers);
void h2_session_event(h2_session *session, h2_session_event_t ev,
int err, const char *msg);
apr_status_t h2_session_process(h2_session *session, int async);
apr_status_t h2_session_pre_close(h2_session *session, int async);
void h2_session_abort(h2_session *session, apr_status_t reason);
void h2_session_close(h2_session *session);
int h2_session_push_enabled(h2_session *session);
struct h2_stream *h2_session_stream_get(h2_session *session, int stream_id);
struct h2_stream *h2_session_push(h2_session *session,
struct h2_stream *is, struct h2_push *push);
apr_status_t h2_session_set_prio(h2_session *session,
struct h2_stream *stream,
const struct h2_priority *prio);
#define H2_SSSN_MSG(s, msg) "h2_session(%ld,%s,%d): "msg, s->id, h2_session_state_str(s->state), s->open_streams
#define H2_SSSN_LOG(aplogno, s, msg) aplogno H2_SSSN_MSG(s, msg)
#endif
