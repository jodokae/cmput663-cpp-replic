#if !defined(__mod_h2__h2_filter__)
#define __mod_h2__h2_filter__
struct h2_bucket_beam;
struct h2_headers;
struct h2_stream;
struct h2_session;
typedef struct h2_filter_cin {
apr_pool_t *pool;
apr_socket_t *socket;
apr_interval_time_t timeout;
apr_bucket_brigade *bb;
struct h2_session *session;
apr_bucket *cur;
} h2_filter_cin;
h2_filter_cin *h2_filter_cin_create(struct h2_session *session);
void h2_filter_cin_timeout_set(h2_filter_cin *cin, apr_interval_time_t timeout);
apr_status_t h2_filter_core_input(ap_filter_t* filter,
apr_bucket_brigade* brigade,
ap_input_mode_t mode,
apr_read_type_e block,
apr_off_t readbytes);
typedef enum {
H2_BUCKET_EV_BEFORE_DESTROY,
H2_BUCKET_EV_BEFORE_MASTER_SEND
} h2_bucket_event;
extern const apr_bucket_type_t h2_bucket_type_observer;
typedef apr_status_t h2_bucket_event_cb(void *ctx, h2_bucket_event event, apr_bucket *b);
#define H2_BUCKET_IS_OBSERVER(e) (e->type == &h2_bucket_type_observer)
apr_bucket * h2_bucket_observer_make(apr_bucket *b, h2_bucket_event_cb *cb,
void *ctx);
apr_bucket * h2_bucket_observer_create(apr_bucket_alloc_t *list,
h2_bucket_event_cb *cb, void *ctx);
apr_status_t h2_bucket_observer_fire(apr_bucket *b, h2_bucket_event event);
apr_bucket *h2_bucket_observer_beam(struct h2_bucket_beam *beam,
apr_bucket_brigade *dest,
const apr_bucket *src);
int h2_filter_h2_status_handler(request_rec *r);
#endif