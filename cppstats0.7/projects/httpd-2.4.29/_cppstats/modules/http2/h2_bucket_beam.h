#if !defined(h2_bucket_beam_h)
#define h2_bucket_beam_h
struct apr_thread_mutex_t;
struct apr_thread_cond_t;
typedef struct {
APR_RING_HEAD(h2_bucket_list, apr_bucket) list;
} h2_blist;
#define H2_BLIST_INIT(b) APR_RING_INIT(&(b)->list, apr_bucket, link);
#define H2_BLIST_SENTINEL(b) APR_RING_SENTINEL(&(b)->list, apr_bucket, link)
#define H2_BLIST_EMPTY(b) APR_RING_EMPTY(&(b)->list, apr_bucket, link)
#define H2_BLIST_FIRST(b) APR_RING_FIRST(&(b)->list)
#define H2_BLIST_LAST(b) APR_RING_LAST(&(b)->list)
#define H2_BLIST_INSERT_HEAD(b, e) do { apr_bucket *ap__b = (e); APR_RING_INSERT_HEAD(&(b)->list, ap__b, apr_bucket, link); } while (0)
#define H2_BLIST_INSERT_TAIL(b, e) do { apr_bucket *ap__b = (e); APR_RING_INSERT_TAIL(&(b)->list, ap__b, apr_bucket, link); } while (0)
#define H2_BLIST_CONCAT(a, b) do { APR_RING_CONCAT(&(a)->list, &(b)->list, apr_bucket, link); } while (0)
#define H2_BLIST_PREPEND(a, b) do { APR_RING_PREPEND(&(a)->list, &(b)->list, apr_bucket, link); } while (0)
typedef void h2_beam_mutex_leave(void *ctx, struct apr_thread_mutex_t *lock);
typedef struct {
apr_thread_mutex_t *mutex;
h2_beam_mutex_leave *leave;
void *leave_ctx;
} h2_beam_lock;
typedef struct h2_bucket_beam h2_bucket_beam;
typedef apr_status_t h2_beam_mutex_enter(void *ctx, h2_beam_lock *pbl);
typedef void h2_beam_io_callback(void *ctx, h2_bucket_beam *beam,
apr_off_t bytes);
typedef void h2_beam_ev_callback(void *ctx, h2_bucket_beam *beam);
typedef struct h2_beam_proxy h2_beam_proxy;
typedef struct {
APR_RING_HEAD(h2_beam_proxy_list, h2_beam_proxy) list;
} h2_bproxy_list;
typedef int h2_beam_can_beam_callback(void *ctx, h2_bucket_beam *beam,
apr_file_t *file);
typedef enum {
H2_BEAM_OWNER_SEND,
H2_BEAM_OWNER_RECV
} h2_beam_owner_t;
int h2_beam_no_files(void *ctx, h2_bucket_beam *beam, apr_file_t *file);
struct h2_bucket_beam {
int id;
const char *tag;
apr_pool_t *pool;
h2_beam_owner_t owner;
h2_blist send_list;
h2_blist hold_list;
h2_blist purge_list;
apr_bucket_brigade *recv_buffer;
h2_bproxy_list proxies;
apr_pool_t *send_pool;
apr_pool_t *recv_pool;
apr_size_t max_buf_size;
apr_interval_time_t timeout;
apr_off_t sent_bytes;
apr_off_t received_bytes;
apr_size_t buckets_sent;
apr_size_t files_beamed;
unsigned int aborted : 1;
unsigned int closed : 1;
unsigned int close_sent : 1;
unsigned int tx_mem_limits : 1;
struct apr_thread_mutex_t *lock;
struct apr_thread_cond_t *change;
apr_off_t cons_bytes_reported;
h2_beam_ev_callback *cons_ev_cb;
h2_beam_io_callback *cons_io_cb;
void *cons_ctx;
apr_off_t prod_bytes_reported;
h2_beam_io_callback *prod_io_cb;
void *prod_ctx;
h2_beam_can_beam_callback *can_beam_fn;
void *can_beam_ctx;
};
apr_status_t h2_beam_create(h2_bucket_beam **pbeam,
apr_pool_t *pool,
int id, const char *tag,
h2_beam_owner_t owner,
apr_size_t buffer_size,
apr_interval_time_t timeout);
apr_status_t h2_beam_destroy(h2_bucket_beam *beam);
apr_status_t h2_beam_send(h2_bucket_beam *beam,
apr_bucket_brigade *bb,
apr_read_type_e block);
void h2_beam_send_from(h2_bucket_beam *beam, apr_pool_t *p);
apr_status_t h2_beam_receive(h2_bucket_beam *beam,
apr_bucket_brigade *green_buckets,
apr_read_type_e block,
apr_off_t readbytes);
int h2_beam_empty(h2_bucket_beam *beam);
int h2_beam_holds_proxies(h2_bucket_beam *beam);
void h2_beam_abort(h2_bucket_beam *beam);
apr_status_t h2_beam_close(h2_bucket_beam *beam);
apr_status_t h2_beam_leave(h2_bucket_beam *beam);
int h2_beam_is_closed(h2_bucket_beam *beam);
apr_status_t h2_beam_wait_empty(h2_bucket_beam *beam, apr_read_type_e block);
void h2_beam_timeout_set(h2_bucket_beam *beam,
apr_interval_time_t timeout);
apr_interval_time_t h2_beam_timeout_get(h2_bucket_beam *beam);
void h2_beam_buffer_size_set(h2_bucket_beam *beam,
apr_size_t buffer_size);
apr_size_t h2_beam_buffer_size_get(h2_bucket_beam *beam);
void h2_beam_on_consumed(h2_bucket_beam *beam,
h2_beam_ev_callback *ev_cb,
h2_beam_io_callback *io_cb, void *ctx);
int h2_beam_report_consumption(h2_bucket_beam *beam);
void h2_beam_on_produced(h2_bucket_beam *beam,
h2_beam_io_callback *io_cb, void *ctx);
void h2_beam_on_file_beam(h2_bucket_beam *beam,
h2_beam_can_beam_callback *cb, void *ctx);
apr_off_t h2_beam_get_buffered(h2_bucket_beam *beam);
apr_off_t h2_beam_get_mem_used(h2_bucket_beam *beam);
int h2_beam_was_received(h2_bucket_beam *beam);
apr_size_t h2_beam_get_files_beamed(h2_bucket_beam *beam);
typedef apr_bucket *h2_bucket_beamer(h2_bucket_beam *beam,
apr_bucket_brigade *dest,
const apr_bucket *src);
void h2_register_bucket_beamer(h2_bucket_beamer *beamer);
void h2_beam_log(h2_bucket_beam *beam, conn_rec *c, int level, const char *msg);
#endif
