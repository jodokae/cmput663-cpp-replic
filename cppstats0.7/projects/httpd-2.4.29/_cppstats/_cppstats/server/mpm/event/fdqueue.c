#include "fdqueue.h"
#include "apr_atomic.h"
static const apr_uint32_t zero_pt = APR_UINT32_MAX/2;
struct recycled_pool {
apr_pool_t *pool;
struct recycled_pool *next;
};
struct fd_queue_info_t {
apr_uint32_t idlers;
apr_thread_mutex_t *idlers_mutex;
apr_thread_cond_t *wait_for_idler;
int terminated;
int max_idlers;
int max_recycled_pools;
apr_uint32_t recycled_pools_count;
struct recycled_pool *recycled_pools;
};
static apr_status_t queue_info_cleanup(void *data_) {
fd_queue_info_t *qi = data_;
apr_thread_cond_destroy(qi->wait_for_idler);
apr_thread_mutex_destroy(qi->idlers_mutex);
for (;;) {
struct recycled_pool *first_pool = qi->recycled_pools;
if (first_pool == NULL) {
break;
}
if (apr_atomic_casptr
((void*) &(qi->recycled_pools), first_pool->next,
first_pool) == first_pool) {
apr_pool_destroy(first_pool->pool);
}
}
return APR_SUCCESS;
}
apr_status_t ap_queue_info_create(fd_queue_info_t ** queue_info,
apr_pool_t * pool, int max_idlers,
int max_recycled_pools) {
apr_status_t rv;
fd_queue_info_t *qi;
qi = apr_pcalloc(pool, sizeof(*qi));
rv = apr_thread_mutex_create(&qi->idlers_mutex, APR_THREAD_MUTEX_DEFAULT,
pool);
if (rv != APR_SUCCESS) {
return rv;
}
rv = apr_thread_cond_create(&qi->wait_for_idler, pool);
if (rv != APR_SUCCESS) {
return rv;
}
qi->recycled_pools = NULL;
qi->max_recycled_pools = max_recycled_pools;
qi->max_idlers = max_idlers;
qi->idlers = zero_pt;
apr_pool_cleanup_register(pool, qi, queue_info_cleanup,
apr_pool_cleanup_null);
*queue_info = qi;
return APR_SUCCESS;
}
apr_status_t ap_queue_info_set_idle(fd_queue_info_t * queue_info,
apr_pool_t * pool_to_recycle) {
apr_status_t rv;
apr_int32_t prev_idlers;
ap_push_pool(queue_info, pool_to_recycle);
prev_idlers = apr_atomic_inc32(&(queue_info->idlers)) - zero_pt;
if (prev_idlers < 0) {
rv = apr_thread_mutex_lock(queue_info->idlers_mutex);
if (rv != APR_SUCCESS) {
AP_DEBUG_ASSERT(0);
return rv;
}
rv = apr_thread_cond_signal(queue_info->wait_for_idler);
if (rv != APR_SUCCESS) {
apr_thread_mutex_unlock(queue_info->idlers_mutex);
return rv;
}
rv = apr_thread_mutex_unlock(queue_info->idlers_mutex);
if (rv != APR_SUCCESS) {
return rv;
}
}
return APR_SUCCESS;
}
apr_status_t ap_queue_info_try_get_idler(fd_queue_info_t * queue_info) {
apr_int32_t new_idlers;
new_idlers = apr_atomic_add32(&(queue_info->idlers), -1) - zero_pt;
if (--new_idlers <= 0) {
apr_atomic_inc32(&(queue_info->idlers));
return APR_EAGAIN;
}
return APR_SUCCESS;
}
apr_status_t ap_queue_info_wait_for_idler(fd_queue_info_t * queue_info,
int *had_to_block) {
apr_status_t rv;
apr_int32_t prev_idlers;
prev_idlers = apr_atomic_add32(&(queue_info->idlers), -1) - zero_pt;
if (prev_idlers <= 0) {
rv = apr_thread_mutex_lock(queue_info->idlers_mutex);
if (rv != APR_SUCCESS) {
AP_DEBUG_ASSERT(0);
apr_atomic_inc32(&(queue_info->idlers));
return rv;
}
if (queue_info->idlers < zero_pt) {
*had_to_block = 1;
rv = apr_thread_cond_wait(queue_info->wait_for_idler,
queue_info->idlers_mutex);
if (rv != APR_SUCCESS) {
apr_status_t rv2;
AP_DEBUG_ASSERT(0);
rv2 = apr_thread_mutex_unlock(queue_info->idlers_mutex);
if (rv2 != APR_SUCCESS) {
return rv2;
}
return rv;
}
}
rv = apr_thread_mutex_unlock(queue_info->idlers_mutex);
if (rv != APR_SUCCESS) {
return rv;
}
}
if (queue_info->terminated) {
return APR_EOF;
} else {
return APR_SUCCESS;
}
}
apr_uint32_t ap_queue_info_get_idlers(fd_queue_info_t * queue_info) {
apr_int32_t val;
val = (apr_int32_t)apr_atomic_read32(&queue_info->idlers) - zero_pt;
if (val < 0)
return 0;
return val;
}
void ap_push_pool(fd_queue_info_t * queue_info,
apr_pool_t * pool_to_recycle) {
struct recycled_pool *new_recycle;
if (!pool_to_recycle)
return;
if (queue_info->max_recycled_pools >= 0) {
apr_uint32_t cnt = apr_atomic_read32(&queue_info->recycled_pools_count);
if (cnt >= queue_info->max_recycled_pools) {
apr_pool_destroy(pool_to_recycle);
return;
}
apr_atomic_inc32(&queue_info->recycled_pools_count);
}
apr_pool_clear(pool_to_recycle);
new_recycle = (struct recycled_pool *) apr_palloc(pool_to_recycle,
sizeof (*new_recycle));
new_recycle->pool = pool_to_recycle;
for (;;) {
struct recycled_pool *next = queue_info->recycled_pools;
new_recycle->next = next;
if (apr_atomic_casptr((void*) &(queue_info->recycled_pools),
new_recycle, next) == next)
break;
}
}
void ap_pop_pool(apr_pool_t ** recycled_pool, fd_queue_info_t * queue_info) {
*recycled_pool = NULL;
for (;;) {
struct recycled_pool *first_pool = queue_info->recycled_pools;
if (first_pool == NULL) {
break;
}
if (apr_atomic_casptr
((void*) &(queue_info->recycled_pools),
first_pool->next, first_pool) == first_pool) {
*recycled_pool = first_pool->pool;
if (queue_info->max_recycled_pools >= 0)
apr_atomic_dec32(&queue_info->recycled_pools_count);
break;
}
}
}
void ap_free_idle_pools(fd_queue_info_t *queue_info) {
apr_pool_t *p;
queue_info->max_recycled_pools = 0;
do {
ap_pop_pool(&p, queue_info);
if (p != NULL)
apr_pool_destroy(p);
} while (p != NULL);
}
apr_status_t ap_queue_info_term(fd_queue_info_t * queue_info) {
apr_status_t rv;
rv = apr_thread_mutex_lock(queue_info->idlers_mutex);
if (rv != APR_SUCCESS) {
return rv;
}
queue_info->terminated = 1;
apr_thread_cond_broadcast(queue_info->wait_for_idler);
return apr_thread_mutex_unlock(queue_info->idlers_mutex);
}
#define ap_queue_full(queue) ((queue)->nelts == (queue)->bounds)
#define ap_queue_empty(queue) ((queue)->nelts == 0 && APR_RING_EMPTY(&queue->timers ,timer_event_t, link))
static apr_status_t ap_queue_destroy(void *data) {
fd_queue_t *queue = data;
apr_thread_cond_destroy(queue->not_empty);
apr_thread_mutex_destroy(queue->one_big_mutex);
return APR_SUCCESS;
}
apr_status_t ap_queue_init(fd_queue_t * queue, int queue_capacity,
apr_pool_t * a) {
int i;
apr_status_t rv;
if ((rv = apr_thread_mutex_create(&queue->one_big_mutex,
APR_THREAD_MUTEX_DEFAULT,
a)) != APR_SUCCESS) {
return rv;
}
if ((rv = apr_thread_cond_create(&queue->not_empty, a)) != APR_SUCCESS) {
return rv;
}
APR_RING_INIT(&queue->timers, timer_event_t, link);
queue->data = apr_palloc(a, queue_capacity * sizeof(fd_queue_elem_t));
queue->bounds = queue_capacity;
queue->nelts = 0;
queue->in = 0;
queue->out = 0;
for (i = 0; i < queue_capacity; ++i)
queue->data[i].sd = NULL;
apr_pool_cleanup_register(a, queue, ap_queue_destroy,
apr_pool_cleanup_null);
return APR_SUCCESS;
}
apr_status_t ap_queue_push(fd_queue_t * queue, apr_socket_t * sd,
event_conn_state_t * ecs, apr_pool_t * p) {
fd_queue_elem_t *elem;
apr_status_t rv;
if ((rv = apr_thread_mutex_lock(queue->one_big_mutex)) != APR_SUCCESS) {
return rv;
}
AP_DEBUG_ASSERT(!queue->terminated);
AP_DEBUG_ASSERT(!ap_queue_full(queue));
elem = &queue->data[queue->in];
queue->in++;
if (queue->in >= queue->bounds)
queue->in -= queue->bounds;
elem->sd = sd;
elem->ecs = ecs;
elem->p = p;
queue->nelts++;
apr_thread_cond_signal(queue->not_empty);
if ((rv = apr_thread_mutex_unlock(queue->one_big_mutex)) != APR_SUCCESS) {
return rv;
}
return APR_SUCCESS;
}
apr_status_t ap_queue_push_timer(fd_queue_t * queue, timer_event_t *te) {
apr_status_t rv;
if ((rv = apr_thread_mutex_lock(queue->one_big_mutex)) != APR_SUCCESS) {
return rv;
}
AP_DEBUG_ASSERT(!queue->terminated);
APR_RING_INSERT_TAIL(&queue->timers, te, timer_event_t, link);
apr_thread_cond_signal(queue->not_empty);
if ((rv = apr_thread_mutex_unlock(queue->one_big_mutex)) != APR_SUCCESS) {
return rv;
}
return APR_SUCCESS;
}
apr_status_t ap_queue_pop_something(fd_queue_t * queue, apr_socket_t ** sd,
event_conn_state_t ** ecs, apr_pool_t ** p,
timer_event_t ** te_out) {
fd_queue_elem_t *elem;
apr_status_t rv;
if ((rv = apr_thread_mutex_lock(queue->one_big_mutex)) != APR_SUCCESS) {
return rv;
}
if (ap_queue_empty(queue)) {
if (!queue->terminated) {
apr_thread_cond_wait(queue->not_empty, queue->one_big_mutex);
}
if (ap_queue_empty(queue)) {
rv = apr_thread_mutex_unlock(queue->one_big_mutex);
if (rv != APR_SUCCESS) {
return rv;
}
if (queue->terminated) {
return APR_EOF;
} else {
return APR_EINTR;
}
}
}
*te_out = NULL;
if (!APR_RING_EMPTY(&queue->timers, timer_event_t, link)) {
*te_out = APR_RING_FIRST(&queue->timers);
APR_RING_REMOVE(*te_out, link);
} else {
elem = &queue->data[queue->out];
queue->out++;
if (queue->out >= queue->bounds)
queue->out -= queue->bounds;
queue->nelts--;
*sd = elem->sd;
*ecs = elem->ecs;
*p = elem->p;
#if defined(AP_DEBUG)
elem->sd = NULL;
elem->p = NULL;
#endif
}
rv = apr_thread_mutex_unlock(queue->one_big_mutex);
return rv;
}
static apr_status_t queue_interrupt(fd_queue_t * queue, int all) {
apr_status_t rv;
if ((rv = apr_thread_mutex_lock(queue->one_big_mutex)) != APR_SUCCESS) {
return rv;
}
if (all)
apr_thread_cond_broadcast(queue->not_empty);
else
apr_thread_cond_signal(queue->not_empty);
return apr_thread_mutex_unlock(queue->one_big_mutex);
}
apr_status_t ap_queue_interrupt_all(fd_queue_t * queue) {
return queue_interrupt(queue, 1);
}
apr_status_t ap_queue_interrupt_one(fd_queue_t * queue) {
return queue_interrupt(queue, 0);
}
apr_status_t ap_queue_term(fd_queue_t * queue) {
apr_status_t rv;
if ((rv = apr_thread_mutex_lock(queue->one_big_mutex)) != APR_SUCCESS) {
return rv;
}
queue->terminated = 1;
if ((rv = apr_thread_mutex_unlock(queue->one_big_mutex)) != APR_SUCCESS) {
return rv;
}
return ap_queue_interrupt_all(queue);
}