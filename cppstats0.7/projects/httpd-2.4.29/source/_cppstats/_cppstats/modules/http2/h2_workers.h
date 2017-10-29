#if !defined(__mod_h2__h2_workers__)
#define __mod_h2__h2_workers__
struct apr_thread_mutex_t;
struct apr_thread_cond_t;
struct h2_mplx;
struct h2_request;
struct h2_task;
struct h2_fifo;
struct h2_slot;
typedef struct h2_workers h2_workers;
struct h2_workers {
server_rec *s;
apr_pool_t *pool;
int next_worker_id;
int min_workers;
int max_workers;
int max_idle_secs;
int aborted;
int dynamic;
apr_threadattr_t *thread_attr;
int nslots;
struct h2_slot *slots;
volatile apr_uint32_t worker_count;
struct h2_slot *free;
struct h2_slot *idle;
struct h2_slot *zombies;
struct h2_fifo *mplxs;
struct apr_thread_mutex_t *lock;
};
h2_workers *h2_workers_create(server_rec *s, apr_pool_t *pool,
int min_size, int max_size, int idle_secs);
apr_status_t h2_workers_register(h2_workers *workers, struct h2_mplx *m);
apr_status_t h2_workers_unregister(h2_workers *workers, struct h2_mplx *m);
#endif
