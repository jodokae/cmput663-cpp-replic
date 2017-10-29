#if !defined(h2_proxy_session_h)
#define h2_proxy_session_h
#define H2_ALEN(a) (sizeof(a)/sizeof((a)[0]))
#include <nghttp2/nghttp2.h>
struct h2_proxy_iqueue;
struct h2_proxy_ihash_t;
typedef enum {
H2_STREAM_ST_IDLE,
H2_STREAM_ST_OPEN,
H2_STREAM_ST_RESV_LOCAL,
H2_STREAM_ST_RESV_REMOTE,
H2_STREAM_ST_CLOSED_INPUT,
H2_STREAM_ST_CLOSED_OUTPUT,
H2_STREAM_ST_CLOSED,
} h2_proxy_stream_state_t;
typedef enum {
H2_PROXYS_ST_INIT,
H2_PROXYS_ST_DONE,
H2_PROXYS_ST_IDLE,
H2_PROXYS_ST_BUSY,
H2_PROXYS_ST_WAIT,
H2_PROXYS_ST_LOCAL_SHUTDOWN,
H2_PROXYS_ST_REMOTE_SHUTDOWN,
} h2_proxys_state;
typedef enum {
H2_PROXYS_EV_INIT,
H2_PROXYS_EV_LOCAL_GOAWAY,
H2_PROXYS_EV_REMOTE_GOAWAY,
H2_PROXYS_EV_CONN_ERROR,
H2_PROXYS_EV_PROTO_ERROR,
H2_PROXYS_EV_CONN_TIMEOUT,
H2_PROXYS_EV_NO_IO,
H2_PROXYS_EV_STREAM_SUBMITTED,
H2_PROXYS_EV_STREAM_DONE,
H2_PROXYS_EV_STREAM_RESUMED,
H2_PROXYS_EV_DATA_READ,
H2_PROXYS_EV_NGH2_DONE,
H2_PROXYS_EV_PRE_CLOSE,
} h2_proxys_event_t;
typedef struct h2_proxy_session h2_proxy_session;
typedef void h2_proxy_request_done(h2_proxy_session *s, request_rec *r,
apr_status_t status, int touched);
struct h2_proxy_session {
const char *id;
conn_rec *c;
proxy_conn_rec *p_conn;
proxy_server_conf *conf;
apr_pool_t *pool;
nghttp2_session *ngh2;
unsigned int aborted : 1;
unsigned int check_ping : 1;
unsigned int h2_front : 1;
h2_proxy_request_done *done;
void *user_data;
unsigned char window_bits_stream;
unsigned char window_bits_connection;
h2_proxys_state state;
apr_interval_time_t wait_timeout;
struct h2_proxy_ihash_t *streams;
struct h2_proxy_iqueue *suspended;
apr_size_t remote_max_concurrent;
int last_stream_id;
apr_time_t last_frame_received;
apr_bucket_brigade *input;
apr_bucket_brigade *output;
};
h2_proxy_session *h2_proxy_session_setup(const char *id, proxy_conn_rec *p_conn,
proxy_server_conf *conf,
int h2_front,
unsigned char window_bits_connection,
unsigned char window_bits_stream,
h2_proxy_request_done *done);
apr_status_t h2_proxy_session_submit(h2_proxy_session *s, const char *url,
request_rec *r, int standalone);
apr_status_t h2_proxy_session_process(h2_proxy_session *s);
void h2_proxy_session_cancel_all(h2_proxy_session *s);
void h2_proxy_session_cleanup(h2_proxy_session *s, h2_proxy_request_done *done);
void h2_proxy_session_update_window(h2_proxy_session *s,
conn_rec *c, apr_off_t bytes);
#define H2_PROXY_REQ_URL_NOTE "h2-proxy-req-url"
#endif
