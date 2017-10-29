#if !defined(__mod_h2__h2_mplx__)
#define __mod_h2__h2_mplx__
struct apr_pool_t;
struct apr_thread_mutex_t;
struct apr_thread_cond_t;
struct h2_bucket_beam;
struct h2_config;
struct h2_ihash_t;
struct h2_task;
struct h2_stream;
struct h2_request;
struct apr_thread_cond_t;
struct h2_workers;
struct h2_iqueue;
struct h2_ngn_shed;
struct h2_req_engine;
#include <apr_queue.h>
typedef struct h2_mplx h2_mplx;
struct h2_mplx {
long id;
conn_rec *c;
apr_pool_t *pool;
server_rec *s;
unsigned int event_pending;
unsigned int aborted;
unsigned int is_registered;
struct h2_ihash_t *streams;
struct h2_ihash_t *sredo;
struct h2_ihash_t *shold;
struct h2_ihash_t *spurge;
struct h2_iqueue *q;
struct h2_ififo *readyq;
struct h2_ihash_t *redo_tasks;
int max_streams;
int max_stream_started;
int tasks_active;
int limit_active;
int max_active;
apr_time_t last_idle_block;
apr_time_t last_limit_change;
apr_interval_time_t limit_change_interval;
apr_thread_mutex_t *lock;
struct apr_thread_cond_t *added_output;
struct apr_thread_cond_t *task_thawed;
struct apr_thread_cond_t *join_wait;
apr_size_t stream_max_mem;
apr_pool_t *spare_io_pool;
apr_array_header_t *spare_slaves;
struct h2_workers *workers;
struct h2_ngn_shed *ngn_shed;
};
apr_status_t h2_mplx_child_init(apr_pool_t *pool, server_rec *s);
h2_mplx *h2_mplx_create(conn_rec *c, apr_pool_t *master,
const struct h2_config *conf,
struct h2_workers *workers);
void h2_mplx_release_and_join(h2_mplx *m, struct apr_thread_cond_t *wait);
apr_status_t h2_mplx_pop_task(h2_mplx *m, struct h2_task **ptask);
void h2_mplx_task_done(h2_mplx *m, struct h2_task *task, struct h2_task **ptask);
int h2_mplx_shutdown(h2_mplx *m);
int h2_mplx_is_busy(h2_mplx *m);
struct h2_stream *h2_mplx_stream_get(h2_mplx *m, int id);
apr_status_t h2_mplx_stream_cleanup(h2_mplx *m, struct h2_stream *stream);
apr_status_t h2_mplx_out_trywait(h2_mplx *m, apr_interval_time_t timeout,
struct apr_thread_cond_t *iowait);
apr_status_t h2_mplx_keep_active(h2_mplx *m, struct h2_stream *stream);
apr_status_t h2_mplx_process(h2_mplx *m, struct h2_stream *stream,
h2_stream_pri_cmp *cmp, void *ctx);
apr_status_t h2_mplx_reprioritize(h2_mplx *m, h2_stream_pri_cmp *cmp, void *ctx);
typedef apr_status_t stream_ev_callback(void *ctx, struct h2_stream *stream);
int h2_mplx_has_master_events(h2_mplx *m);
apr_status_t h2_mplx_dispatch_master_events(h2_mplx *m,
stream_ev_callback *on_resume,
void *ctx);
int h2_mplx_awaits_data(h2_mplx *m);
typedef int h2_mplx_stream_cb(struct h2_stream *s, void *ctx);
apr_status_t h2_mplx_stream_do(h2_mplx *m, h2_mplx_stream_cb *cb, void *ctx);
apr_status_t h2_mplx_out_open(h2_mplx *mplx, int stream_id,
struct h2_bucket_beam *beam);
#define H2_MPLX_LIST_SENTINEL(b) APR_RING_SENTINEL((b), h2_mplx, link)
#define H2_MPLX_LIST_EMPTY(b) APR_RING_EMPTY((b), h2_mplx, link)
#define H2_MPLX_LIST_FIRST(b) APR_RING_FIRST(b)
#define H2_MPLX_LIST_LAST(b) APR_RING_LAST(b)
#define H2_MPLX_LIST_INSERT_HEAD(b, e) do { h2_mplx *ap__b = (e); APR_RING_INSERT_HEAD((b), ap__b, h2_mplx, link); } while (0)
#define H2_MPLX_LIST_INSERT_TAIL(b, e) do { h2_mplx *ap__b = (e); APR_RING_INSERT_TAIL((b), ap__b, h2_mplx, link); } while (0)
#define H2_MPLX_NEXT(e) APR_RING_NEXT((e), link)
#define H2_MPLX_PREV(e) APR_RING_PREV((e), link)
#define H2_MPLX_REMOVE(e) APR_RING_REMOVE((e), link)
apr_status_t h2_mplx_idle(h2_mplx *m);
typedef void h2_output_consumed(void *ctx, conn_rec *c, apr_off_t consumed);
typedef apr_status_t h2_mplx_req_engine_init(struct h2_req_engine *engine,
const char *id,
const char *type,
apr_pool_t *pool,
apr_size_t req_buffer_size,
request_rec *r,
h2_output_consumed **pconsumed,
void **pbaton);
apr_status_t h2_mplx_req_engine_push(const char *ngn_type,
request_rec *r,
h2_mplx_req_engine_init *einit);
apr_status_t h2_mplx_req_engine_pull(struct h2_req_engine *ngn,
apr_read_type_e block,
int capacity,
request_rec **pr);
void h2_mplx_req_engine_done(struct h2_req_engine *ngn, conn_rec *r_conn,
apr_status_t status);
#endif