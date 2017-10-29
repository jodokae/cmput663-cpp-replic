#include "apr.h"
#include "apr_portable.h"
#include "apr_strings.h"
#include "apr_file_io.h"
#include "apr_thread_proc.h"
#include "apr_signal.h"
#include "apr_thread_mutex.h"
#include "apr_poll.h"
#include "apr_ring.h"
#include "apr_queue.h"
#include "apr_atomic.h"
#define APR_WANT_STRFUNC
#include "apr_want.h"
#include "apr_version.h"
#include <stdlib.h>
#if APR_HAVE_UNISTD_H
#include <unistd.h>
#endif
#if APR_HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#if APR_HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#if defined(HAVE_SYS_PROCESSOR_H)
#include <sys/processor.h>
#endif
#if !APR_HAS_THREADS
#error The Event MPM requires APR threads, but they are unavailable.
#endif
#include "ap_config.h"
#include "httpd.h"
#include "http_main.h"
#include "http_log.h"
#include "http_config.h"
#include "http_core.h"
#include "http_connection.h"
#include "http_protocol.h"
#include "ap_mpm.h"
#include "mpm_common.h"
#include "ap_listen.h"
#include "scoreboard.h"
#include "fdqueue.h"
#include "mpm_default.h"
#include "http_vhost.h"
#include "unixd.h"
#include "apr_skiplist.h"
#include <signal.h>
#include <limits.h>
#if !defined(DEFAULT_SERVER_LIMIT)
#define DEFAULT_SERVER_LIMIT 16
#endif
#if !defined(MAX_SERVER_LIMIT)
#define MAX_SERVER_LIMIT 20000
#endif
#if !defined(DEFAULT_THREAD_LIMIT)
#define DEFAULT_THREAD_LIMIT 64
#endif
#if !defined(MAX_THREAD_LIMIT)
#define MAX_THREAD_LIMIT 100000
#endif
#define MPM_CHILD_PID(i) (ap_scoreboard_image->parent[i].pid)
#if !APR_VERSION_AT_LEAST(1,4,0)
#define apr_time_from_msec(x) (x * 1000)
#endif
#if !defined(MAX_SECS_TO_LINGER)
#define MAX_SECS_TO_LINGER 30
#endif
#define SECONDS_TO_LINGER 2
#if !defined(DEFAULT_WORKER_FACTOR)
#define DEFAULT_WORKER_FACTOR 2
#endif
#define WORKER_FACTOR_SCALE 16
static unsigned int worker_factor = DEFAULT_WORKER_FACTOR * WORKER_FACTOR_SCALE;
static int threads_per_child = 0;
static int ap_daemons_to_start = 0;
static int min_spare_threads = 0;
static int max_spare_threads = 0;
static int active_daemons_limit = 0;
static int active_daemons = 0;
static int max_workers = 0;
static int server_limit = 0;
static int thread_limit = 0;
static int had_healthy_child = 0;
static int dying = 0;
static int workers_may_exit = 0;
static int start_thread_may_exit = 0;
static int listener_may_exit = 0;
static int listener_is_wakeable = 0;
static int num_listensocks = 0;
static apr_int32_t conns_this_child;
static apr_uint32_t connection_count = 0;
static apr_uint32_t lingering_count = 0;
static apr_uint32_t suspended_count = 0;
static apr_uint32_t clogged_count = 0;
static apr_uint32_t threads_shutdown = 0;
static int resource_shortage = 0;
static fd_queue_t *worker_queue;
static fd_queue_info_t *worker_queue_info;
static apr_thread_mutex_t *timeout_mutex;
module AP_MODULE_DECLARE_DATA mpm_event_module;
struct event_srv_cfg_s;
typedef struct event_srv_cfg_s event_srv_cfg;
static apr_pollfd_t *listener_pollfd;
static apr_pollset_t *event_pollset;
static event_conn_state_t *volatile defer_linger_chain;
struct event_conn_state_t {
APR_RING_ENTRY(event_conn_state_t) timeout_list;
apr_time_t queue_timestamp;
conn_rec *c;
request_rec *r;
event_srv_cfg *sc;
int suspended;
apr_pool_t *p;
apr_bucket_alloc_t *bucket_alloc;
apr_pollfd_t pfd;
conn_state_t pub;
struct event_conn_state_t *chain;
};
APR_RING_HEAD(timeout_head_t, event_conn_state_t);
struct timeout_queue {
struct timeout_head_t head;
apr_interval_time_t timeout;
apr_uint32_t count;
apr_uint32_t *total;
struct timeout_queue *next;
};
static struct timeout_queue *write_completion_q,
*keepalive_q,
*linger_q,
*short_linger_q;
static volatile apr_time_t queues_next_expiry;
#define TIMEOUT_FUDGE_FACTOR apr_time_from_msec(100)
static void TO_QUEUE_APPEND(struct timeout_queue *q, event_conn_state_t *el) {
apr_time_t q_expiry;
apr_time_t next_expiry;
APR_RING_INSERT_TAIL(&q->head, el, event_conn_state_t, timeout_list);
apr_atomic_inc32(q->total);
++q->count;
el = APR_RING_FIRST(&q->head);
q_expiry = el->queue_timestamp + q->timeout;
next_expiry = queues_next_expiry;
if (!next_expiry || next_expiry > q_expiry + TIMEOUT_FUDGE_FACTOR) {
queues_next_expiry = q_expiry;
if (listener_is_wakeable) {
apr_pollset_wakeup(event_pollset);
}
}
}
static void TO_QUEUE_REMOVE(struct timeout_queue *q, event_conn_state_t *el) {
APR_RING_REMOVE(el, timeout_list);
apr_atomic_dec32(q->total);
--q->count;
}
static struct timeout_queue *TO_QUEUE_MAKE(apr_pool_t *p, apr_time_t t,
struct timeout_queue *ref) {
struct timeout_queue *q;
q = apr_pcalloc(p, sizeof *q);
APR_RING_INIT(&q->head, event_conn_state_t, timeout_list);
q->total = (ref) ? ref->total : apr_pcalloc(p, sizeof *q->total);
q->timeout = t;
return q;
}
#define TO_QUEUE_ELEM_INIT(el) APR_RING_ELEM_INIT((el), timeout_list)
typedef struct {
int pslot;
int tslot;
} proc_info;
typedef struct {
apr_thread_t **threads;
apr_thread_t *listener;
int child_num_arg;
apr_threadattr_t *threadattr;
} thread_starter;
typedef enum {
PT_CSD,
PT_ACCEPT
} poll_type_e;
typedef struct {
poll_type_e type;
void *baton;
} listener_poll_type;
typedef struct event_retained_data {
ap_unixd_mpm_retained_data *mpm;
int first_server_limit;
int first_thread_limit;
int sick_child_detected;
int maxclients_reported;
int max_daemons_limit;
int total_daemons;
int *idle_spawn_rate;
#if !defined(MAX_SPAWN_RATE)
#define MAX_SPAWN_RATE (32)
#endif
int hold_off_on_exponential_spawning;
} event_retained_data;
static event_retained_data *retained;
typedef struct event_child_bucket {
ap_pod_t *pod;
ap_listen_rec *listeners;
} event_child_bucket;
static event_child_bucket *all_buckets,
*my_bucket;
struct event_srv_cfg_s {
struct timeout_queue *wc_q,
*ka_q;
};
#define ID_FROM_CHILD_THREAD(c, t) ((c * thread_limit) + t)
static int one_process = 0;
#if defined(DEBUG_SIGSTOP)
int raise_sigstop_flags;
#endif
static apr_pool_t *pconf;
static apr_pool_t *pchild;
static pid_t ap_my_pid;
static pid_t parent_pid;
static apr_os_thread_t *listener_os_thread;
#define LISTENER_SIGNAL SIGHUP
static apr_socket_t **worker_sockets;
static void disable_listensocks(int process_slot) {
int i;
for (i = 0; i < num_listensocks; i++) {
apr_pollset_remove(event_pollset, &listener_pollfd[i]);
}
ap_scoreboard_image->parent[process_slot].not_accepting = 1;
}
static void enable_listensocks(int process_slot) {
int i;
if (listener_may_exit) {
return;
}
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, ap_server_conf, APLOGNO(00457)
"Accepting new connections again: "
"%u active conns (%u lingering/%u clogged/%u suspended), "
"%u idle workers",
apr_atomic_read32(&connection_count),
apr_atomic_read32(&lingering_count),
apr_atomic_read32(&clogged_count),
apr_atomic_read32(&suspended_count),
ap_queue_info_get_idlers(worker_queue_info));
for (i = 0; i < num_listensocks; i++)
apr_pollset_add(event_pollset, &listener_pollfd[i]);
ap_scoreboard_image->parent[process_slot].not_accepting = 0;
}
static void abort_socket_nonblocking(apr_socket_t *csd) {
apr_status_t rv;
apr_socket_timeout_set(csd, 0);
#if defined(SOL_SOCKET) && defined(SO_LINGER)
{
apr_os_sock_t osd = -1;
struct linger opt;
opt.l_onoff = 1;
opt.l_linger = 0;
apr_os_sock_get(&osd, csd);
setsockopt(osd, SOL_SOCKET, SO_LINGER, (void *)&opt, sizeof opt);
}
#endif
rv = apr_socket_close(csd);
if (rv != APR_SUCCESS) {
ap_log_error(APLOG_MARK, APLOG_ERR, rv, ap_server_conf, APLOGNO(00468)
"error closing socket");
AP_DEBUG_ASSERT(0);
}
}
static void close_worker_sockets(void) {
int i;
for (i = 0; i < threads_per_child; i++) {
apr_socket_t *csd = worker_sockets[i];
if (csd) {
worker_sockets[i] = NULL;
abort_socket_nonblocking(csd);
}
}
for (;;) {
event_conn_state_t *cs = defer_linger_chain;
if (!cs) {
break;
}
if (apr_atomic_casptr((void *)&defer_linger_chain, cs->chain,
cs) != cs) {
continue;
}
cs->chain = NULL;
abort_socket_nonblocking(cs->pfd.desc.s);
}
}
static void wakeup_listener(void) {
listener_may_exit = 1;
if (!listener_os_thread) {
return;
}
if (listener_is_wakeable) {
apr_pollset_wakeup(event_pollset);
}
ap_queue_info_term(worker_queue_info);
#if defined(HAVE_PTHREAD_KILL)
pthread_kill(*listener_os_thread, LISTENER_SIGNAL);
#else
kill(ap_my_pid, LISTENER_SIGNAL);
#endif
}
#define ST_INIT 0
#define ST_GRACEFUL 1
#define ST_UNGRACEFUL 2
static int terminate_mode = ST_INIT;
static void signal_threads(int mode) {
if (terminate_mode == mode) {
return;
}
terminate_mode = mode;
retained->mpm->mpm_state = AP_MPMQ_STOPPING;
wakeup_listener();
if (mode == ST_UNGRACEFUL) {
workers_may_exit = 1;
ap_queue_interrupt_all(worker_queue);
close_worker_sockets();
}
}
static int event_query(int query_code, int *result, apr_status_t *rv) {
*rv = APR_SUCCESS;
switch (query_code) {
case AP_MPMQ_MAX_DAEMON_USED:
*result = retained->max_daemons_limit;
break;
case AP_MPMQ_IS_THREADED:
*result = AP_MPMQ_STATIC;
break;
case AP_MPMQ_IS_FORKED:
*result = AP_MPMQ_DYNAMIC;
break;
case AP_MPMQ_IS_ASYNC:
*result = 1;
break;
case AP_MPMQ_HARD_LIMIT_DAEMONS:
*result = server_limit;
break;
case AP_MPMQ_HARD_LIMIT_THREADS:
*result = thread_limit;
break;
case AP_MPMQ_MAX_THREADS:
*result = threads_per_child;
break;
case AP_MPMQ_MIN_SPARE_DAEMONS:
*result = 0;
break;
case AP_MPMQ_MIN_SPARE_THREADS:
*result = min_spare_threads;
break;
case AP_MPMQ_MAX_SPARE_DAEMONS:
*result = 0;
break;
case AP_MPMQ_MAX_SPARE_THREADS:
*result = max_spare_threads;
break;
case AP_MPMQ_MAX_REQUESTS_DAEMON:
*result = ap_max_requests_per_child;
break;
case AP_MPMQ_MAX_DAEMONS:
*result = active_daemons_limit;
break;
case AP_MPMQ_MPM_STATE:
*result = retained->mpm->mpm_state;
break;
case AP_MPMQ_GENERATION:
*result = retained->mpm->my_generation;
break;
default:
*rv = APR_ENOTIMPL;
break;
}
return OK;
}
static void event_note_child_killed(int childnum, pid_t pid, ap_generation_t gen) {
if (childnum != -1) {
ap_run_child_status(ap_server_conf,
ap_scoreboard_image->parent[childnum].pid,
ap_scoreboard_image->parent[childnum].generation,
childnum, MPM_CHILD_EXITED);
ap_scoreboard_image->parent[childnum].pid = 0;
} else {
ap_run_child_status(ap_server_conf, pid, gen, -1, MPM_CHILD_EXITED);
}
}
static void event_note_child_started(int slot, pid_t pid) {
ap_scoreboard_image->parent[slot].pid = pid;
ap_run_child_status(ap_server_conf,
ap_scoreboard_image->parent[slot].pid,
retained->mpm->my_generation, slot, MPM_CHILD_STARTED);
}
static const char *event_get_name(void) {
return "event";
}
static void clean_child_exit(int code) __attribute__ ((noreturn));
static void clean_child_exit(int code) {
retained->mpm->mpm_state = AP_MPMQ_STOPPING;
if (pchild) {
apr_pool_destroy(pchild);
}
if (one_process) {
event_note_child_killed( 0, 0, 0);
}
exit(code);
}
static void just_die(int sig) {
clean_child_exit(0);
}
static int child_fatal;
static apr_status_t decrement_connection_count(void *cs_) {
event_conn_state_t *cs = cs_;
switch (cs->pub.state) {
case CONN_STATE_LINGER_NORMAL:
case CONN_STATE_LINGER_SHORT:
apr_atomic_dec32(&lingering_count);
break;
case CONN_STATE_SUSPENDED:
apr_atomic_dec32(&suspended_count);
break;
default:
break;
}
if (!apr_atomic_dec32(&connection_count)
&& listener_is_wakeable && listener_may_exit) {
apr_pollset_wakeup(event_pollset);
}
return APR_SUCCESS;
}
static void notify_suspend(event_conn_state_t *cs) {
ap_run_suspend_connection(cs->c, cs->r);
cs->suspended = 1;
cs->c->sbh = NULL;
}
static void notify_resume(event_conn_state_t *cs, ap_sb_handle_t *sbh) {
cs->c->sbh = sbh;
cs->suspended = 0;
ap_run_resume_connection(cs->c, cs->r);
}
static int start_lingering_close_blocking(event_conn_state_t *cs) {
apr_status_t rv;
struct timeout_queue *q;
apr_socket_t *csd = cs->pfd.desc.s;
if (ap_start_lingering_close(cs->c)) {
notify_suspend(cs);
apr_socket_close(csd);
ap_push_pool(worker_queue_info, cs->p);
return 0;
}
#if defined(AP_DEBUG)
{
rv = apr_socket_timeout_set(csd, 0);
AP_DEBUG_ASSERT(rv == APR_SUCCESS);
}
#else
apr_socket_timeout_set(csd, 0);
#endif
cs->queue_timestamp = apr_time_now();
if (apr_table_get(cs->c->notes, "short-lingering-close")) {
q = short_linger_q;
cs->pub.state = CONN_STATE_LINGER_SHORT;
} else {
q = linger_q;
cs->pub.state = CONN_STATE_LINGER_NORMAL;
}
apr_atomic_inc32(&lingering_count);
notify_suspend(cs);
cs->pfd.reqevents = (
cs->pub.sense == CONN_SENSE_WANT_WRITE ? APR_POLLOUT :
APR_POLLIN) | APR_POLLHUP | APR_POLLERR;
cs->pub.sense = CONN_SENSE_DEFAULT;
apr_thread_mutex_lock(timeout_mutex);
TO_QUEUE_APPEND(q, cs);
rv = apr_pollset_add(event_pollset, &cs->pfd);
apr_thread_mutex_unlock(timeout_mutex);
if (rv != APR_SUCCESS && !APR_STATUS_IS_EEXIST(rv)) {
ap_log_error(APLOG_MARK, APLOG_ERR, rv, ap_server_conf, APLOGNO(03092)
"start_lingering_close: apr_pollset_add failure");
apr_thread_mutex_lock(timeout_mutex);
TO_QUEUE_REMOVE(q, cs);
apr_thread_mutex_unlock(timeout_mutex);
apr_socket_close(cs->pfd.desc.s);
ap_push_pool(worker_queue_info, cs->p);
return 0;
}
return 1;
}
static int start_lingering_close_nonblocking(event_conn_state_t *cs) {
event_conn_state_t *chain;
for (;;) {
cs->chain = chain = defer_linger_chain;
if (apr_atomic_casptr((void *)&defer_linger_chain, cs,
chain) != chain) {
continue;
}
return 1;
}
}
static int stop_lingering_close(event_conn_state_t *cs) {
apr_socket_t *csd = ap_get_conn_socket(cs->c);
ap_log_error(APLOG_MARK, APLOG_TRACE4, 0, ap_server_conf,
"socket reached timeout in lingering-close state");
abort_socket_nonblocking(csd);
ap_push_pool(worker_queue_info, cs->p);
if (dying)
ap_queue_interrupt_one(worker_queue);
return 0;
}
static apr_status_t ptrans_pre_cleanup(void *dummy) {
event_conn_state_t *cs = dummy;
if (cs->suspended) {
notify_resume(cs, NULL);
}
return APR_SUCCESS;
}
static apr_status_t event_request_cleanup(void *dummy) {
conn_rec *c = dummy;
event_conn_state_t *cs = ap_get_module_config(c->conn_config,
&mpm_event_module);
cs->r = NULL;
return APR_SUCCESS;
}
static void event_pre_read_request(request_rec *r, conn_rec *c) {
event_conn_state_t *cs = ap_get_module_config(c->conn_config,
&mpm_event_module);
cs->r = r;
cs->sc = ap_get_module_config(ap_server_conf->module_config,
&mpm_event_module);
apr_pool_cleanup_register(r->pool, c, event_request_cleanup,
apr_pool_cleanup_null);
}
static int event_post_read_request(request_rec *r) {
conn_rec *c = r->connection;
event_conn_state_t *cs = ap_get_module_config(c->conn_config,
&mpm_event_module);
if (r->server->keep_alive_timeout_set) {
cs->sc = ap_get_module_config(r->server->module_config,
&mpm_event_module);
} else {
cs->sc = ap_get_module_config(c->base_server->module_config,
&mpm_event_module);
}
return OK;
}
static void process_socket(apr_thread_t *thd, apr_pool_t * p, apr_socket_t * sock,
event_conn_state_t * cs, int my_child_num,
int my_thread_num) {
conn_rec *c;
long conn_id = ID_FROM_CHILD_THREAD(my_child_num, my_thread_num);
int rc;
ap_sb_handle_t *sbh;
ap_create_sb_handle(&sbh, p, my_child_num, my_thread_num);
if (cs == NULL) {
listener_poll_type *pt = apr_pcalloc(p, sizeof(*pt));
cs = apr_pcalloc(p, sizeof(event_conn_state_t));
cs->bucket_alloc = apr_bucket_alloc_create(p);
c = ap_run_create_connection(p, ap_server_conf, sock,
conn_id, sbh, cs->bucket_alloc);
if (!c) {
ap_push_pool(worker_queue_info, p);
return;
}
apr_atomic_inc32(&connection_count);
apr_pool_cleanup_register(c->pool, cs, decrement_connection_count,
apr_pool_cleanup_null);
ap_set_module_config(c->conn_config, &mpm_event_module, cs);
c->current_thread = thd;
cs->c = c;
c->cs = &(cs->pub);
cs->p = p;
cs->sc = ap_get_module_config(ap_server_conf->module_config,
&mpm_event_module);
cs->pfd.desc_type = APR_POLL_SOCKET;
cs->pfd.reqevents = APR_POLLIN;
cs->pfd.desc.s = sock;
pt->type = PT_CSD;
pt->baton = cs;
cs->pfd.client_data = pt;
apr_pool_pre_cleanup_register(p, cs, ptrans_pre_cleanup);
TO_QUEUE_ELEM_INIT(cs);
ap_update_vhost_given_ip(c);
rc = ap_run_pre_connection(c, sock);
if (rc != OK && rc != DONE) {
ap_log_cerror(APLOG_MARK, APLOG_DEBUG, 0, c, APLOGNO(00469)
"process_socket: connection aborted");
c->aborted = 1;
}
cs->pub.state = CONN_STATE_READ_REQUEST_LINE;
cs->pub.sense = CONN_SENSE_DEFAULT;
} else {
c = cs->c;
notify_resume(cs, sbh);
c->current_thread = thd;
c->id = conn_id;
if (c->aborted) {
cs->pub.state = CONN_STATE_LINGER;
}
}
if (cs->pub.state == CONN_STATE_LINGER) {
} else if (c->clogging_input_filters) {
apr_atomic_inc32(&clogged_count);
ap_run_process_connection(c);
if (cs->pub.state != CONN_STATE_SUSPENDED) {
cs->pub.state = CONN_STATE_LINGER;
}
apr_atomic_dec32(&clogged_count);
} else if (cs->pub.state == CONN_STATE_READ_REQUEST_LINE) {
read_request:
ap_run_process_connection(c);
}
if (cs->pub.state == CONN_STATE_WRITE_COMPLETION) {
ap_filter_t *output_filter = c->output_filters;
apr_status_t rv;
ap_update_child_status(sbh, SERVER_BUSY_WRITE, NULL);
while (output_filter->next != NULL) {
output_filter = output_filter->next;
}
rv = output_filter->frec->filter_func.out_func(output_filter, NULL);
if (rv != APR_SUCCESS) {
ap_log_cerror(APLOG_MARK, APLOG_DEBUG, rv, c, APLOGNO(00470)
"network write failure in core output filter");
cs->pub.state = CONN_STATE_LINGER;
} else if (c->data_in_output_filters) {
cs->queue_timestamp = apr_time_now();
notify_suspend(cs);
cs->pfd.reqevents = (
cs->pub.sense == CONN_SENSE_WANT_READ ? APR_POLLIN :
APR_POLLOUT) | APR_POLLHUP | APR_POLLERR;
cs->pub.sense = CONN_SENSE_DEFAULT;
apr_thread_mutex_lock(timeout_mutex);
TO_QUEUE_APPEND(cs->sc->wc_q, cs);
rc = apr_pollset_add(event_pollset, &cs->pfd);
apr_thread_mutex_unlock(timeout_mutex);
if (rc != APR_SUCCESS && !APR_STATUS_IS_EEXIST(rc)) {
ap_log_error(APLOG_MARK, APLOG_ERR, rc, ap_server_conf, APLOGNO(03465)
"process_socket: apr_pollset_add failure for "
"write completion");
apr_thread_mutex_lock(timeout_mutex);
TO_QUEUE_REMOVE(cs->sc->wc_q, cs);
apr_thread_mutex_unlock(timeout_mutex);
apr_socket_close(cs->pfd.desc.s);
ap_push_pool(worker_queue_info, cs->p);
}
return;
} else if (c->keepalive != AP_CONN_KEEPALIVE || c->aborted ||
listener_may_exit) {
cs->pub.state = CONN_STATE_LINGER;
} else if (c->data_in_input_filters) {
cs->pub.state = CONN_STATE_READ_REQUEST_LINE;
goto read_request;
} else {
cs->pub.state = CONN_STATE_CHECK_REQUEST_LINE_READABLE;
}
}
if (cs->pub.state == CONN_STATE_LINGER) {
start_lingering_close_blocking(cs);
} else if (cs->pub.state == CONN_STATE_CHECK_REQUEST_LINE_READABLE) {
cs->queue_timestamp = apr_time_now();
notify_suspend(cs);
cs->pfd.reqevents = APR_POLLIN;
apr_thread_mutex_lock(timeout_mutex);
TO_QUEUE_APPEND(cs->sc->ka_q, cs);
rc = apr_pollset_add(event_pollset, &cs->pfd);
apr_thread_mutex_unlock(timeout_mutex);
if (rc != APR_SUCCESS && !APR_STATUS_IS_EEXIST(rc)) {
ap_log_error(APLOG_MARK, APLOG_ERR, rc, ap_server_conf, APLOGNO(03093)
"process_socket: apr_pollset_add failure for "
"keep alive");
apr_thread_mutex_lock(timeout_mutex);
TO_QUEUE_REMOVE(cs->sc->ka_q, cs);
apr_thread_mutex_unlock(timeout_mutex);
apr_socket_close(cs->pfd.desc.s);
ap_push_pool(worker_queue_info, cs->p);
return;
}
} else if (cs->pub.state == CONN_STATE_SUSPENDED) {
apr_atomic_inc32(&suspended_count);
notify_suspend(cs);
}
}
static void check_infinite_requests(void) {
if (ap_max_requests_per_child) {
ap_log_error(APLOG_MARK, APLOG_TRACE1, 0, ap_server_conf,
"Stopping process due to MaxConnectionsPerChild");
signal_threads(ST_GRACEFUL);
} else {
conns_this_child = APR_INT32_MAX;
}
}
static void close_listeners(int process_slot, int *closed) {
if (!*closed) {
int i;
disable_listensocks(process_slot);
ap_close_listeners_ex(my_bucket->listeners);
*closed = 1;
dying = 1;
ap_scoreboard_image->parent[process_slot].quiescing = 1;
for (i = 0; i < threads_per_child; ++i) {
ap_update_child_status_from_indexes(process_slot, i,
SERVER_GRACEFUL, NULL);
}
kill(ap_my_pid, SIGTERM);
ap_free_idle_pools(worker_queue_info);
ap_queue_interrupt_all(worker_queue);
}
}
static void unblock_signal(int sig) {
sigset_t sig_mask;
sigemptyset(&sig_mask);
sigaddset(&sig_mask, sig);
#if defined(SIGPROCMASK_SETS_THREAD_MASK)
sigprocmask(SIG_UNBLOCK, &sig_mask, NULL);
#else
pthread_sigmask(SIG_UNBLOCK, &sig_mask, NULL);
#endif
}
static void dummy_signal_handler(int sig) {
}
static apr_status_t init_pollset(apr_pool_t *p) {
ap_listen_rec *lr;
listener_poll_type *pt;
int i = 0;
listener_pollfd = apr_palloc(p, sizeof(apr_pollfd_t) * num_listensocks);
for (lr = my_bucket->listeners; lr != NULL; lr = lr->next, i++) {
apr_pollfd_t *pfd;
AP_DEBUG_ASSERT(i < num_listensocks);
pfd = &listener_pollfd[i];
pt = apr_pcalloc(p, sizeof(*pt));
pfd->desc_type = APR_POLL_SOCKET;
pfd->desc.s = lr->sd;
pfd->reqevents = APR_POLLIN;
pt->type = PT_ACCEPT;
pt->baton = lr;
pfd->client_data = pt;
apr_socket_opt_set(pfd->desc.s, APR_SO_NONBLOCK, 1);
apr_pollset_add(event_pollset, pfd);
lr->accept_func = ap_unixd_accept;
}
return APR_SUCCESS;
}
static apr_status_t push_timer2worker(timer_event_t* te) {
return ap_queue_push_timer(worker_queue, te);
}
static apr_status_t push2worker(event_conn_state_t *cs, apr_socket_t *csd,
apr_pool_t *ptrans) {
apr_status_t rc;
if (cs) {
csd = cs->pfd.desc.s;
ptrans = cs->p;
}
rc = ap_queue_push(worker_queue, csd, cs, ptrans);
if (rc != APR_SUCCESS) {
ap_log_error(APLOG_MARK, APLOG_CRIT, rc, ap_server_conf, APLOGNO(00471)
"push2worker: ap_queue_push failed");
if (csd) {
abort_socket_nonblocking(csd);
}
if (ptrans) {
ap_push_pool(worker_queue_info, ptrans);
}
signal_threads(ST_GRACEFUL);
}
return rc;
}
static void get_worker(int *have_idle_worker_p, int blocking, int *all_busy) {
apr_status_t rc;
if (*have_idle_worker_p) {
return;
}
if (blocking)
rc = ap_queue_info_wait_for_idler(worker_queue_info, all_busy);
else
rc = ap_queue_info_try_get_idler(worker_queue_info);
if (rc == APR_SUCCESS || APR_STATUS_IS_EOF(rc)) {
*have_idle_worker_p = 1;
} else if (!blocking && rc == APR_EAGAIN) {
*all_busy = 1;
} else {
ap_log_error(APLOG_MARK, APLOG_ERR, rc, ap_server_conf, APLOGNO(00472)
"ap_queue_info_wait_for_idler failed. "
"Attempting to shutdown process gracefully");
signal_threads(ST_GRACEFUL);
}
}
static APR_RING_HEAD(timer_free_ring_t, timer_event_t) timer_free_ring;
static apr_skiplist *timer_skiplist;
static volatile apr_time_t timers_next_expiry;
#define EVENT_FUDGE_FACTOR apr_time_from_msec(10)
static int timer_comp(void *a, void *b) {
apr_time_t t1 = (apr_time_t) ((timer_event_t *)a)->when;
apr_time_t t2 = (apr_time_t) ((timer_event_t *)b)->when;
AP_DEBUG_ASSERT(t1);
AP_DEBUG_ASSERT(t2);
return ((t1 < t2) ? -1 : 1);
}
static apr_thread_mutex_t *g_timer_skiplist_mtx;
static apr_status_t event_register_timed_callback(apr_time_t t,
ap_mpm_callback_fn_t *cbfn,
void *baton) {
timer_event_t *te;
apr_thread_mutex_lock(g_timer_skiplist_mtx);
if (!APR_RING_EMPTY(&timer_free_ring, timer_event_t, link)) {
te = APR_RING_FIRST(&timer_free_ring);
APR_RING_REMOVE(te, link);
} else {
te = apr_skiplist_alloc(timer_skiplist, sizeof(timer_event_t));
APR_RING_ELEM_INIT(te, link);
}
te->cbfunc = cbfn;
te->baton = baton;
te->when = t + apr_time_now();
{
apr_time_t next_expiry;
apr_skiplist_insert(timer_skiplist, te);
next_expiry = timers_next_expiry;
if (!next_expiry || next_expiry > te->when + EVENT_FUDGE_FACTOR) {
timers_next_expiry = te->when;
if (listener_is_wakeable) {
apr_pollset_wakeup(event_pollset);
}
}
}
apr_thread_mutex_unlock(g_timer_skiplist_mtx);
return APR_SUCCESS;
}
static void process_lingering_close(event_conn_state_t *cs, const apr_pollfd_t *pfd) {
apr_socket_t *csd = ap_get_conn_socket(cs->c);
char dummybuf[2048];
apr_size_t nbytes;
apr_status_t rv;
struct timeout_queue *q;
q = (cs->pub.state == CONN_STATE_LINGER_SHORT) ? short_linger_q : linger_q;
do {
nbytes = sizeof(dummybuf);
rv = apr_socket_recv(csd, dummybuf, &nbytes);
} while (rv == APR_SUCCESS);
if (APR_STATUS_IS_EAGAIN(rv)) {
return;
}
apr_thread_mutex_lock(timeout_mutex);
TO_QUEUE_REMOVE(q, cs);
rv = apr_pollset_remove(event_pollset, pfd);
apr_thread_mutex_unlock(timeout_mutex);
AP_DEBUG_ASSERT(rv == APR_SUCCESS || APR_STATUS_IS_NOTFOUND(rv));
TO_QUEUE_ELEM_INIT(cs);
rv = apr_socket_close(csd);
AP_DEBUG_ASSERT(rv == APR_SUCCESS);
ap_push_pool(worker_queue_info, cs->p);
if (dying)
ap_queue_interrupt_one(worker_queue);
}
static void process_timeout_queue(struct timeout_queue *q,
apr_time_t timeout_time,
int (*func)(event_conn_state_t *)) {
apr_uint32_t total = 0, count;
event_conn_state_t *first, *cs, *last;
struct timeout_head_t trash;
struct timeout_queue *qp;
apr_status_t rv;
if (!apr_atomic_read32(q->total)) {
return;
}
APR_RING_INIT(&trash, event_conn_state_t, timeout_list);
for (qp = q; qp; qp = qp->next) {
count = 0;
cs = first = last = APR_RING_FIRST(&qp->head);
while (cs != APR_RING_SENTINEL(&qp->head, event_conn_state_t,
timeout_list)) {
if (timeout_time
&& cs->queue_timestamp + qp->timeout > timeout_time
&& cs->queue_timestamp < timeout_time + qp->timeout) {
apr_time_t q_expiry = cs->queue_timestamp + qp->timeout;
apr_time_t next_expiry = queues_next_expiry;
if (!next_expiry || next_expiry > q_expiry) {
queues_next_expiry = q_expiry;
}
break;
}
last = cs;
rv = apr_pollset_remove(event_pollset, &cs->pfd);
if (rv != APR_SUCCESS && !APR_STATUS_IS_NOTFOUND(rv)) {
ap_log_cerror(APLOG_MARK, APLOG_ERR, rv, cs->c, APLOGNO(00473)
"apr_pollset_remove failed");
}
cs = APR_RING_NEXT(cs, timeout_list);
count++;
}
if (!count)
continue;
APR_RING_UNSPLICE(first, last, timeout_list);
APR_RING_SPLICE_TAIL(&trash, first, last, event_conn_state_t,
timeout_list);
AP_DEBUG_ASSERT(apr_atomic_read32(q->total) >= count);
apr_atomic_sub32(q->total, count);
qp->count -= count;
total += count;
}
if (!total)
return;
apr_thread_mutex_unlock(timeout_mutex);
first = APR_RING_FIRST(&trash);
do {
cs = APR_RING_NEXT(first, timeout_list);
TO_QUEUE_ELEM_INIT(first);
func(first);
first = cs;
} while (--total);
apr_thread_mutex_lock(timeout_mutex);
}
static void process_keepalive_queue(apr_time_t timeout_time) {
if (!timeout_time) {
ap_log_error(APLOG_MARK, APLOG_TRACE1, 0, ap_server_conf,
"All workers are busy or dying, will close %u "
"keep-alive connections",
apr_atomic_read32(keepalive_q->total));
}
process_timeout_queue(keepalive_q, timeout_time,
start_lingering_close_nonblocking);
}
static void * APR_THREAD_FUNC listener_thread(apr_thread_t * thd, void *dummy) {
timer_event_t *te;
apr_status_t rc;
proc_info *ti = dummy;
int process_slot = ti->pslot;
struct process_score *ps = ap_get_scoreboard_process(process_slot);
apr_pool_t *tpool = apr_thread_pool_get(thd);
void *csd = NULL;
apr_pool_t *ptrans;
ap_listen_rec *lr;
int have_idle_worker = 0;
const apr_pollfd_t *out_pfd;
apr_int32_t num = 0;
apr_interval_time_t timeout_interval;
apr_time_t timeout_time = 0, now, last_log;
listener_poll_type *pt;
int closed = 0, listeners_disabled = 0;
last_log = apr_time_now();
free(ti);
rc = init_pollset(tpool);
if (rc != APR_SUCCESS) {
ap_log_error(APLOG_MARK, APLOG_ERR, rc, ap_server_conf,
"failed to initialize pollset, "
"attempting to shutdown process gracefully");
signal_threads(ST_GRACEFUL);
return NULL;
}
unblock_signal(LISTENER_SIGNAL);
apr_signal(LISTENER_SIGNAL, dummy_signal_handler);
for (;;) {
int workers_were_busy = 0;
if (listener_may_exit) {
close_listeners(process_slot, &closed);
if (terminate_mode == ST_UNGRACEFUL
|| apr_atomic_read32(&connection_count) == 0)
break;
}
if (conns_this_child <= 0)
check_infinite_requests();
now = apr_time_now();
if (APLOGtrace6(ap_server_conf)) {
if (now - last_log > apr_time_from_sec(1)) {
last_log = now;
apr_thread_mutex_lock(timeout_mutex);
ap_log_error(APLOG_MARK, APLOG_TRACE6, 0, ap_server_conf,
"connections: %u (clogged: %u write-completion: %d "
"keep-alive: %d lingering: %d suspended: %u)",
apr_atomic_read32(&connection_count),
apr_atomic_read32(&clogged_count),
apr_atomic_read32(write_completion_q->total),
apr_atomic_read32(keepalive_q->total),
apr_atomic_read32(&lingering_count),
apr_atomic_read32(&suspended_count));
if (dying) {
ap_log_error(APLOG_MARK, APLOG_TRACE6, 0, ap_server_conf,
"%u/%u workers shutdown",
apr_atomic_read32(&threads_shutdown),
threads_per_child);
}
apr_thread_mutex_unlock(timeout_mutex);
}
}
timeout_interval = -1;
timeout_time = timers_next_expiry;
if (timeout_time && timeout_time < now + EVENT_FUDGE_FACTOR) {
apr_thread_mutex_lock(g_timer_skiplist_mtx);
while ((te = apr_skiplist_peek(timer_skiplist))) {
if (te->when > now + EVENT_FUDGE_FACTOR) {
timers_next_expiry = te->when;
timeout_interval = te->when - now;
break;
}
apr_skiplist_pop(timer_skiplist, NULL);
push_timer2worker(te);
}
if (!te) {
timers_next_expiry = 0;
}
apr_thread_mutex_unlock(g_timer_skiplist_mtx);
}
timeout_time = queues_next_expiry;
if (timeout_time
&& (timeout_interval < 0
|| timeout_time <= now
|| timeout_interval > timeout_time - now)) {
timeout_interval = timeout_time > now ? timeout_time - now : 1;
}
#define NON_WAKEABLE_POLL_TIMEOUT apr_time_from_msec(100)
if (!listener_is_wakeable
&& (timeout_interval < 0
|| timeout_interval > NON_WAKEABLE_POLL_TIMEOUT)) {
timeout_interval = NON_WAKEABLE_POLL_TIMEOUT;
}
rc = apr_pollset_poll(event_pollset, timeout_interval, &num, &out_pfd);
if (rc != APR_SUCCESS) {
if (APR_STATUS_IS_EINTR(rc)) {
if (!listener_may_exit) {
continue;
}
timeout_time = 0;
} else if (!APR_STATUS_IS_TIMEUP(rc)) {
ap_log_error(APLOG_MARK, APLOG_CRIT, rc, ap_server_conf,
"apr_pollset_poll failed. Attempting to "
"shutdown process gracefully");
signal_threads(ST_GRACEFUL);
}
num = 0;
}
if (listener_may_exit) {
close_listeners(process_slot, &closed);
if (terminate_mode == ST_UNGRACEFUL
|| apr_atomic_read32(&connection_count) == 0)
break;
}
while (num) {
pt = (listener_poll_type *) out_pfd->client_data;
if (pt->type == PT_CSD) {
event_conn_state_t *cs = (event_conn_state_t *) pt->baton;
struct timeout_queue *remove_from_q = cs->sc->wc_q;
int blocking = 1;
switch (cs->pub.state) {
case CONN_STATE_CHECK_REQUEST_LINE_READABLE:
cs->pub.state = CONN_STATE_READ_REQUEST_LINE;
remove_from_q = cs->sc->ka_q;
blocking = 0;
case CONN_STATE_WRITE_COMPLETION:
get_worker(&have_idle_worker, blocking,
&workers_were_busy);
apr_thread_mutex_lock(timeout_mutex);
TO_QUEUE_REMOVE(remove_from_q, cs);
rc = apr_pollset_remove(event_pollset, &cs->pfd);
apr_thread_mutex_unlock(timeout_mutex);
TO_QUEUE_ELEM_INIT(cs);
if (rc != APR_SUCCESS && !APR_STATUS_IS_NOTFOUND(rc)) {
ap_log_error(APLOG_MARK, APLOG_ERR, rc, ap_server_conf,
APLOGNO(03094) "pollset remove failed");
start_lingering_close_nonblocking(cs);
break;
}
if (!have_idle_worker) {
start_lingering_close_nonblocking(cs);
} else if (push2worker(cs, NULL, NULL) == APR_SUCCESS) {
have_idle_worker = 0;
}
break;
case CONN_STATE_LINGER_NORMAL:
case CONN_STATE_LINGER_SHORT:
process_lingering_close(cs, out_pfd);
break;
default:
ap_log_error(APLOG_MARK, APLOG_CRIT, rc,
ap_server_conf, APLOGNO(03096)
"event_loop: unexpected state %d",
cs->pub.state);
ap_assert(0);
}
} else if (pt->type == PT_ACCEPT) {
if (workers_were_busy) {
if (!listeners_disabled)
disable_listensocks(process_slot);
listeners_disabled = 1;
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, ap_server_conf,
"All workers busy, not accepting new conns "
"in this process");
} else if ( (int)apr_atomic_read32(&connection_count)
- (int)apr_atomic_read32(&lingering_count)
> threads_per_child
+ ap_queue_info_get_idlers(worker_queue_info) *
worker_factor / WORKER_FACTOR_SCALE) {
if (!listeners_disabled)
disable_listensocks(process_slot);
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, ap_server_conf,
"Too many open connections (%u), "
"not accepting new conns in this process",
apr_atomic_read32(&connection_count));
ap_log_error(APLOG_MARK, APLOG_TRACE1, 0, ap_server_conf,
"Idle workers: %u",
ap_queue_info_get_idlers(worker_queue_info));
listeners_disabled = 1;
} else if (listeners_disabled) {
listeners_disabled = 0;
enable_listensocks(process_slot);
}
if (!listeners_disabled) {
lr = (ap_listen_rec *) pt->baton;
ap_pop_pool(&ptrans, worker_queue_info);
if (ptrans == NULL) {
apr_allocator_t *allocator;
apr_allocator_create(&allocator);
apr_allocator_max_free_set(allocator,
ap_max_mem_free);
apr_pool_create_ex(&ptrans, pconf, NULL, allocator);
apr_allocator_owner_set(allocator, ptrans);
if (ptrans == NULL) {
ap_log_error(APLOG_MARK, APLOG_CRIT, rc,
ap_server_conf, APLOGNO(03097)
"Failed to create transaction pool");
signal_threads(ST_GRACEFUL);
return NULL;
}
}
apr_pool_tag(ptrans, "transaction");
get_worker(&have_idle_worker, 1, &workers_were_busy);
rc = lr->accept_func(&csd, lr, ptrans);
AP_DEBUG_ASSERT(rc == APR_SUCCESS || !csd);
if (rc == APR_EGENERAL) {
resource_shortage = 1;
signal_threads(ST_GRACEFUL);
}
if (csd != NULL) {
conns_this_child--;
if (push2worker(NULL, csd, ptrans) == APR_SUCCESS) {
have_idle_worker = 0;
}
} else {
ap_push_pool(worker_queue_info, ptrans);
}
}
}
out_pfd++;
num--;
}
if (timeout_time && timeout_time < (now = apr_time_now())) {
timeout_time = now + TIMEOUT_FUDGE_FACTOR;
apr_thread_mutex_lock(timeout_mutex);
queues_next_expiry = 0;
if (workers_were_busy || dying) {
process_keepalive_queue(0);
} else {
process_keepalive_queue(timeout_time);
}
process_timeout_queue(write_completion_q, timeout_time,
start_lingering_close_nonblocking);
process_timeout_queue(linger_q, timeout_time,
stop_lingering_close);
process_timeout_queue(short_linger_q, timeout_time,
stop_lingering_close);
apr_thread_mutex_unlock(timeout_mutex);
ps->keep_alive = apr_atomic_read32(keepalive_q->total);
ps->write_completion = apr_atomic_read32(write_completion_q->total);
ps->connections = apr_atomic_read32(&connection_count);
ps->suspended = apr_atomic_read32(&suspended_count);
ps->lingering_close = apr_atomic_read32(&lingering_count);
} else if ((workers_were_busy || dying)
&& apr_atomic_read32(keepalive_q->total)) {
apr_thread_mutex_lock(timeout_mutex);
process_keepalive_queue(0);
apr_thread_mutex_unlock(timeout_mutex);
ps->keep_alive = 0;
}
if (defer_linger_chain) {
get_worker(&have_idle_worker, 0, &workers_were_busy);
if (have_idle_worker
&& defer_linger_chain
&& push2worker(NULL, NULL, NULL) == APR_SUCCESS) {
have_idle_worker = 0;
}
}
if (listeners_disabled && !workers_were_busy
&& (int)apr_atomic_read32(&connection_count)
- (int)apr_atomic_read32(&lingering_count)
< ((int)ap_queue_info_get_idlers(worker_queue_info) - 1)
* worker_factor / WORKER_FACTOR_SCALE + threads_per_child) {
listeners_disabled = 0;
enable_listensocks(process_slot);
}
}
close_listeners(process_slot, &closed);
ap_queue_term(worker_queue);
apr_thread_exit(thd, APR_SUCCESS);
return NULL;
}
static int worker_thread_should_exit_early(void) {
for (;;) {
apr_uint32_t conns = apr_atomic_read32(&connection_count);
apr_uint32_t dead = apr_atomic_read32(&threads_shutdown);
apr_uint32_t newdead;
AP_DEBUG_ASSERT(dead <= threads_per_child);
if (conns >= threads_per_child - dead)
return 0;
newdead = dead + 1;
if (apr_atomic_cas32(&threads_shutdown, newdead, dead) == dead) {
return 1;
}
}
}
static void *APR_THREAD_FUNC worker_thread(apr_thread_t * thd, void *dummy) {
proc_info *ti = dummy;
int process_slot = ti->pslot;
int thread_slot = ti->tslot;
apr_socket_t *csd = NULL;
event_conn_state_t *cs;
apr_pool_t *ptrans;
apr_status_t rv;
int is_idle = 0;
timer_event_t *te = NULL;
free(ti);
ap_scoreboard_image->servers[process_slot][thread_slot].pid = ap_my_pid;
ap_scoreboard_image->servers[process_slot][thread_slot].tid = apr_os_thread_current();
ap_scoreboard_image->servers[process_slot][thread_slot].generation = retained->mpm->my_generation;
ap_update_child_status_from_indexes(process_slot, thread_slot,
SERVER_STARTING, NULL);
while (!workers_may_exit) {
if (!is_idle) {
rv = ap_queue_info_set_idle(worker_queue_info, NULL);
if (rv != APR_SUCCESS) {
ap_log_error(APLOG_MARK, APLOG_EMERG, rv, ap_server_conf,
"ap_queue_info_set_idle failed. Attempting to "
"shutdown process gracefully.");
signal_threads(ST_GRACEFUL);
break;
}
is_idle = 1;
}
ap_update_child_status_from_indexes(process_slot, thread_slot,
dying ? SERVER_GRACEFUL
: SERVER_READY, NULL);
worker_pop:
if (workers_may_exit) {
break;
}
if (dying && worker_thread_should_exit_early()) {
break;
}
te = NULL;
rv = ap_queue_pop_something(worker_queue, &csd, &cs, &ptrans, &te);
if (rv != APR_SUCCESS) {
if (APR_STATUS_IS_EOF(rv)) {
break;
} else if (APR_STATUS_IS_EINTR(rv)) {
goto worker_pop;
} else if (!workers_may_exit) {
ap_log_error(APLOG_MARK, APLOG_CRIT, rv, ap_server_conf,
APLOGNO(03099) "ap_queue_pop failed");
}
continue;
}
if (te != NULL) {
te->cbfunc(te->baton);
{
apr_thread_mutex_lock(g_timer_skiplist_mtx);
APR_RING_INSERT_TAIL(&timer_free_ring, te, timer_event_t, link);
apr_thread_mutex_unlock(g_timer_skiplist_mtx);
}
} else {
is_idle = 0;
if (csd != NULL) {
worker_sockets[thread_slot] = csd;
process_socket(thd, ptrans, csd, cs, process_slot, thread_slot);
worker_sockets[thread_slot] = NULL;
}
}
while (!workers_may_exit) {
cs = defer_linger_chain;
if (!cs) {
break;
}
if (apr_atomic_casptr((void *)&defer_linger_chain, cs->chain,
cs) != cs) {
continue;
}
cs->chain = NULL;
worker_sockets[thread_slot] = csd = cs->pfd.desc.s;
#if defined(AP_DEBUG)
rv = apr_socket_timeout_set(csd, SECONDS_TO_LINGER);
AP_DEBUG_ASSERT(rv == APR_SUCCESS);
#else
apr_socket_timeout_set(csd, SECONDS_TO_LINGER);
#endif
cs->pub.state = CONN_STATE_LINGER;
process_socket(thd, cs->p, csd, cs, process_slot, thread_slot);
worker_sockets[thread_slot] = NULL;
}
}
ap_update_child_status_from_indexes(process_slot, thread_slot,
dying ? SERVER_DEAD
: SERVER_GRACEFUL, NULL);
apr_thread_exit(thd, APR_SUCCESS);
return NULL;
}
static int check_signal(int signum) {
switch (signum) {
case SIGTERM:
case SIGINT:
return 1;
}
return 0;
}
static void create_listener_thread(thread_starter * ts) {
int my_child_num = ts->child_num_arg;
apr_threadattr_t *thread_attr = ts->threadattr;
proc_info *my_info;
apr_status_t rv;
my_info = (proc_info *) ap_malloc(sizeof(proc_info));
my_info->pslot = my_child_num;
my_info->tslot = -1;
rv = apr_thread_create(&ts->listener, thread_attr, listener_thread,
my_info, pchild);
if (rv != APR_SUCCESS) {
ap_log_error(APLOG_MARK, APLOG_ALERT, rv, ap_server_conf, APLOGNO(00474)
"apr_thread_create: unable to create listener thread");
clean_child_exit(APEXIT_CHILDSICK);
}
apr_os_thread_get(&listener_os_thread, ts->listener);
}
static void *APR_THREAD_FUNC start_threads(apr_thread_t * thd, void *dummy) {
thread_starter *ts = dummy;
apr_thread_t **threads = ts->threads;
apr_threadattr_t *thread_attr = ts->threadattr;
int my_child_num = ts->child_num_arg;
proc_info *my_info;
apr_status_t rv;
int i;
int threads_created = 0;
int listener_started = 0;
int loops;
int prev_threads_created;
int max_recycled_pools = -1;
int good_methods[] = {APR_POLLSET_KQUEUE, APR_POLLSET_PORT, APR_POLLSET_EPOLL};
const apr_uint32_t pollset_size = threads_per_child * 2;
worker_queue = apr_pcalloc(pchild, sizeof(*worker_queue));
rv = ap_queue_init(worker_queue, threads_per_child, pchild);
if (rv != APR_SUCCESS) {
ap_log_error(APLOG_MARK, APLOG_ALERT, rv, ap_server_conf, APLOGNO(03100)
"ap_queue_init() failed");
clean_child_exit(APEXIT_CHILDFATAL);
}
if (ap_max_mem_free != APR_ALLOCATOR_MAX_FREE_UNLIMITED) {
max_recycled_pools = threads_per_child * 3 / 4 ;
}
rv = ap_queue_info_create(&worker_queue_info, pchild,
threads_per_child, max_recycled_pools);
if (rv != APR_SUCCESS) {
ap_log_error(APLOG_MARK, APLOG_ALERT, rv, ap_server_conf, APLOGNO(03101)
"ap_queue_info_create() failed");
clean_child_exit(APEXIT_CHILDFATAL);
}
rv = apr_thread_mutex_create(&timeout_mutex, APR_THREAD_MUTEX_DEFAULT,
pchild);
if (rv != APR_SUCCESS) {
ap_log_error(APLOG_MARK, APLOG_ERR, rv, ap_server_conf, APLOGNO(03102)
"creation of the timeout mutex failed.");
clean_child_exit(APEXIT_CHILDFATAL);
}
for (i = 0; i < sizeof(good_methods) / sizeof(good_methods[0]); i++) {
apr_uint32_t flags = APR_POLLSET_THREADSAFE | APR_POLLSET_NOCOPY |
APR_POLLSET_NODEFAULT | APR_POLLSET_WAKEABLE;
rv = apr_pollset_create_ex(&event_pollset, pollset_size, pchild, flags,
good_methods[i]);
if (rv == APR_SUCCESS) {
listener_is_wakeable = 1;
break;
}
flags &= ~APR_POLLSET_WAKEABLE;
rv = apr_pollset_create_ex(&event_pollset, pollset_size, pchild, flags,
good_methods[i]);
if (rv == APR_SUCCESS) {
break;
}
}
if (rv != APR_SUCCESS) {
rv = apr_pollset_create(&event_pollset, pollset_size, pchild,
APR_POLLSET_THREADSAFE | APR_POLLSET_NOCOPY);
}
if (rv != APR_SUCCESS) {
ap_log_error(APLOG_MARK, APLOG_ERR, rv, ap_server_conf, APLOGNO(03103)
"apr_pollset_create with Thread Safety failed.");
clean_child_exit(APEXIT_CHILDFATAL);
}
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, ap_server_conf, APLOGNO(02471)
"start_threads: Using %s (%swakeable)",
apr_pollset_method_name(event_pollset),
listener_is_wakeable ? "" : "not ");
worker_sockets = apr_pcalloc(pchild, threads_per_child
* sizeof(apr_socket_t *));
loops = prev_threads_created = 0;
while (1) {
for (i = 0; i < threads_per_child; i++) {
int status =
ap_scoreboard_image->servers[my_child_num][i].status;
if (status != SERVER_DEAD) {
continue;
}
my_info = (proc_info *) ap_malloc(sizeof(proc_info));
my_info->pslot = my_child_num;
my_info->tslot = i;
ap_update_child_status_from_indexes(my_child_num, i,
SERVER_STARTING, NULL);
rv = apr_thread_create(&threads[i], thread_attr,
worker_thread, my_info, pchild);
if (rv != APR_SUCCESS) {
ap_log_error(APLOG_MARK, APLOG_ALERT, rv, ap_server_conf,
APLOGNO(03104)
"apr_thread_create: unable to create worker thread");
clean_child_exit(APEXIT_CHILDSICK);
}
threads_created++;
}
if (!listener_started && threads_created) {
create_listener_thread(ts);
listener_started = 1;
}
if (start_thread_may_exit || threads_created == threads_per_child) {
break;
}
apr_sleep(apr_time_from_sec(1));
++loops;
if (loops % 120 == 0) {
if (prev_threads_created == threads_created) {
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, ap_server_conf,
"child %" APR_PID_T_FMT " isn't taking over "
"slots very quickly (%d of %d)",
ap_my_pid, threads_created,
threads_per_child);
}
prev_threads_created = threads_created;
}
}
apr_thread_exit(thd, APR_SUCCESS);
return NULL;
}
static void join_workers(apr_thread_t * listener, apr_thread_t ** threads) {
int i;
apr_status_t rv, thread_rv;
if (listener) {
int iter;
iter = 0;
while (iter < 10 && !dying) {
apr_sleep(apr_time_make(0, 500000));
wakeup_listener();
++iter;
}
if (iter >= 10) {
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, ap_server_conf, APLOGNO(00475)
"the listener thread didn't stop accepting");
} else {
rv = apr_thread_join(&thread_rv, listener);
if (rv != APR_SUCCESS) {
ap_log_error(APLOG_MARK, APLOG_CRIT, rv, ap_server_conf, APLOGNO(00476)
"apr_thread_join: unable to join listener thread");
}
}
}
for (i = 0; i < threads_per_child; i++) {
if (threads[i]) {
rv = apr_thread_join(&thread_rv, threads[i]);
if (rv != APR_SUCCESS) {
ap_log_error(APLOG_MARK, APLOG_CRIT, rv, ap_server_conf, APLOGNO(00477)
"apr_thread_join: unable to join worker "
"thread %d", i);
}
}
}
}
static void join_start_thread(apr_thread_t * start_thread_id) {
apr_status_t rv, thread_rv;
start_thread_may_exit = 1;
rv = apr_thread_join(&thread_rv, start_thread_id);
if (rv != APR_SUCCESS) {
ap_log_error(APLOG_MARK, APLOG_CRIT, rv, ap_server_conf, APLOGNO(00478)
"apr_thread_join: unable to join the start " "thread");
}
}
static void child_main(int child_num_arg, int child_bucket) {
apr_thread_t **threads;
apr_status_t rv;
thread_starter *ts;
apr_threadattr_t *thread_attr;
apr_thread_t *start_thread_id;
int i;
retained->mpm->mpm_state = AP_MPMQ_STARTING;
ap_my_pid = getpid();
ap_fatal_signal_child_setup(ap_server_conf);
apr_pool_create(&pchild, pconf);
for (i = 0; i < retained->mpm->num_buckets; i++) {
if (i != child_bucket) {
ap_close_listeners_ex(all_buckets[i].listeners);
ap_mpm_podx_close(all_buckets[i].pod);
}
}
ap_reopen_scoreboard(pchild, NULL, 0);
if (ap_run_drop_privileges(pchild, ap_server_conf)) {
clean_child_exit(APEXIT_CHILDFATAL);
}
apr_thread_mutex_create(&g_timer_skiplist_mtx, APR_THREAD_MUTEX_DEFAULT, pchild);
APR_RING_INIT(&timer_free_ring, timer_event_t, link);
apr_skiplist_init(&timer_skiplist, pchild);
apr_skiplist_set_compare(timer_skiplist, timer_comp, timer_comp);
ap_run_child_init(pchild, ap_server_conf);
rv = apr_setup_signal_thread();
if (rv != APR_SUCCESS) {
ap_log_error(APLOG_MARK, APLOG_EMERG, rv, ap_server_conf, APLOGNO(00479)
"Couldn't initialize signal thread");
clean_child_exit(APEXIT_CHILDFATAL);
}
if (ap_max_requests_per_child) {
conns_this_child = ap_max_requests_per_child;
} else {
conns_this_child = APR_INT32_MAX;
}
threads = ap_calloc(threads_per_child, sizeof(apr_thread_t *));
ts = apr_palloc(pchild, sizeof(*ts));
apr_threadattr_create(&thread_attr, pchild);
apr_threadattr_detach_set(thread_attr, 0);
if (ap_thread_stacksize != 0) {
rv = apr_threadattr_stacksize_set(thread_attr, ap_thread_stacksize);
if (rv != APR_SUCCESS && rv != APR_ENOTIMPL) {
ap_log_error(APLOG_MARK, APLOG_WARNING, rv, ap_server_conf, APLOGNO(02436)
"WARNING: ThreadStackSize of %" APR_SIZE_T_FMT " is "
"inappropriate, using default",
ap_thread_stacksize);
}
}
ts->threads = threads;
ts->listener = NULL;
ts->child_num_arg = child_num_arg;
ts->threadattr = thread_attr;
rv = apr_thread_create(&start_thread_id, thread_attr, start_threads,
ts, pchild);
if (rv != APR_SUCCESS) {
ap_log_error(APLOG_MARK, APLOG_ALERT, rv, ap_server_conf, APLOGNO(00480)
"apr_thread_create: unable to create worker thread");
clean_child_exit(APEXIT_CHILDSICK);
}
retained->mpm->mpm_state = AP_MPMQ_RUNNING;
if (one_process) {
apr_signal_thread(check_signal);
join_start_thread(start_thread_id);
signal_threads(ST_UNGRACEFUL);
join_workers(ts->listener, threads);
} else {
unblock_signal(SIGTERM);
apr_signal(SIGTERM, dummy_signal_handler);
while (1) {
rv = ap_mpm_podx_check(my_bucket->pod);
if (rv == AP_MPM_PODX_NORESTART) {
switch (terminate_mode) {
case ST_GRACEFUL:
rv = AP_MPM_PODX_GRACEFUL;
break;
case ST_UNGRACEFUL:
rv = AP_MPM_PODX_RESTART;
break;
}
}
if (rv == AP_MPM_PODX_GRACEFUL || rv == AP_MPM_PODX_RESTART) {
join_start_thread(start_thread_id);
signal_threads(rv ==
AP_MPM_PODX_GRACEFUL ? ST_GRACEFUL : ST_UNGRACEFUL);
break;
}
}
join_workers(ts->listener, threads);
}
free(threads);
clean_child_exit(resource_shortage ? APEXIT_CHILDSICK : 0);
}
static int make_child(server_rec * s, int slot, int bucket) {
int pid;
if (slot + 1 > retained->max_daemons_limit) {
retained->max_daemons_limit = slot + 1;
}
if (ap_scoreboard_image->parent[slot].pid != 0) {
ap_log_error(APLOG_MARK, APLOG_ERR, 0, ap_server_conf, APLOGNO(03455)
"BUG: Scoreboard slot %d should be empty but is "
"in use by pid %" APR_PID_T_FMT,
slot, ap_scoreboard_image->parent[slot].pid);
return -1;
}
if (one_process) {
my_bucket = &all_buckets[0];
event_note_child_started(slot, getpid());
child_main(slot, 0);
ap_assert(0);
return -1;
}
if ((pid = fork()) == -1) {
ap_log_error(APLOG_MARK, APLOG_ERR, errno, s, APLOGNO(00481)
"fork: Unable to fork new process");
apr_sleep(apr_time_from_sec(10));
return -1;
}
if (!pid) {
my_bucket = &all_buckets[bucket];
#if defined(HAVE_BINDPROCESSOR)
int status = bindprocessor(BINDPROCESS, (int) getpid(),
PROCESSOR_CLASS_ANY);
if (status != OK)
ap_log_error(APLOG_MARK, APLOG_DEBUG, errno,
ap_server_conf, APLOGNO(00482)
"processor unbind failed");
#endif
RAISE_SIGSTOP(MAKE_CHILD);
apr_signal(SIGTERM, just_die);
child_main(slot, bucket);
ap_assert(0);
return -1;
}
ap_scoreboard_image->parent[slot].quiescing = 0;
ap_scoreboard_image->parent[slot].not_accepting = 0;
ap_scoreboard_image->parent[slot].bucket = bucket;
event_note_child_started(slot, pid);
active_daemons++;
retained->total_daemons++;
return 0;
}
static void startup_children(int number_to_start) {
int i;
for (i = 0; number_to_start && i < server_limit; ++i) {
if (ap_scoreboard_image->parent[i].pid != 0) {
continue;
}
if (make_child(ap_server_conf, i, i % retained->mpm->num_buckets) < 0) {
break;
}
--number_to_start;
}
}
static void perform_idle_server_maintenance(int child_bucket, int num_buckets) {
int i, j;
int idle_thread_count = 0;
worker_score *ws;
process_score *ps;
int free_length = 0;
int free_slots[MAX_SPAWN_RATE];
int last_non_dead = -1;
int active_thread_count = 0;
for (i = 0; i < server_limit; ++i) {
int status = SERVER_DEAD;
int child_threads_active = 0;
if (i >= retained->max_daemons_limit &&
free_length == retained->idle_spawn_rate[child_bucket]) {
break;
}
ps = &ap_scoreboard_image->parent[i];
if (ps->pid != 0) {
for (j = 0; j < threads_per_child; j++) {
ws = &ap_scoreboard_image->servers[i][j];
status = ws->status;
if (status <= SERVER_READY && !ps->quiescing && !ps->not_accepting
&& ps->generation == retained->mpm->my_generation
&& ps->bucket == child_bucket) {
++idle_thread_count;
}
if (status >= SERVER_READY && status < SERVER_GRACEFUL) {
++child_threads_active;
}
}
last_non_dead = i;
}
active_thread_count += child_threads_active;
if (!ps->pid && free_length < retained->idle_spawn_rate[child_bucket])
free_slots[free_length++] = i;
else if (child_threads_active == threads_per_child)
had_healthy_child = 1;
}
if (retained->sick_child_detected) {
if (had_healthy_child) {
retained->sick_child_detected = 0;
} else {
retained->mpm->shutdown_pending = 1;
child_fatal = 1;
ap_log_error(APLOG_MARK, APLOG_ALERT, 0,
ap_server_conf, APLOGNO(02324)
"A resource shortage or other unrecoverable failure "
"was encountered before any child process initialized "
"successfully... httpd is exiting!");
return;
}
}
retained->max_daemons_limit = last_non_dead + 1;
if (idle_thread_count > max_spare_threads / num_buckets) {
if (retained->total_daemons <= active_daemons_limit &&
retained->total_daemons < server_limit) {
ap_mpm_podx_signal(all_buckets[child_bucket].pod,
AP_MPM_PODX_GRACEFUL);
retained->idle_spawn_rate[child_bucket] = 1;
active_daemons--;
} else {
ap_log_error(APLOG_MARK, APLOG_TRACE5, 0, ap_server_conf,
"Not shutting down child: total daemons %d / "
"active limit %d / ServerLimit %d",
retained->total_daemons, active_daemons_limit,
server_limit);
}
} else if (idle_thread_count < min_spare_threads / num_buckets) {
if (active_thread_count >= max_workers) {
if (!retained->maxclients_reported) {
ap_log_error(APLOG_MARK, APLOG_ERR, 0, ap_server_conf, APLOGNO(00484)
"server reached MaxRequestWorkers setting, "
"consider raising the MaxRequestWorkers "
"setting");
retained->maxclients_reported = 1;
}
retained->idle_spawn_rate[child_bucket] = 1;
} else if (free_length == 0) {
ap_log_error(APLOG_MARK, APLOG_ERR, 0, ap_server_conf, APLOGNO(03490)
"scoreboard is full, not at MaxRequestWorkers."
"Increase ServerLimit.");
retained->idle_spawn_rate[child_bucket] = 1;
} else {
if (free_length > retained->idle_spawn_rate[child_bucket]) {
free_length = retained->idle_spawn_rate[child_bucket];
}
if (retained->idle_spawn_rate[child_bucket] >= 8) {
ap_log_error(APLOG_MARK, APLOG_INFO, 0, ap_server_conf, APLOGNO(00486)
"server seems busy, (you may need "
"to increase StartServers, ThreadsPerChild "
"or Min/MaxSpareThreads), "
"spawning %d children, there are around %d idle "
"threads, %d active children, and %d children "
"that are shutting down", free_length,
idle_thread_count, active_daemons,
retained->total_daemons);
}
for (i = 0; i < free_length; ++i) {
ap_log_error(APLOG_MARK, APLOG_TRACE5, 0, ap_server_conf,
"Spawning new child: slot %d active / "
"total daemons: %d/%d",
free_slots[i], active_daemons,
retained->total_daemons);
make_child(ap_server_conf, free_slots[i], child_bucket);
}
if (retained->hold_off_on_exponential_spawning) {
--retained->hold_off_on_exponential_spawning;
} else if (retained->idle_spawn_rate[child_bucket]
< MAX_SPAWN_RATE / num_buckets) {
retained->idle_spawn_rate[child_bucket] *= 2;
}
}
} else {
retained->idle_spawn_rate[child_bucket] = 1;
}
}
static void server_main_loop(int remaining_children_to_start, int num_buckets) {
int child_slot;
apr_exit_why_e exitwhy;
int status, processed_status;
apr_proc_t pid;
int i;
while (!retained->mpm->restart_pending && !retained->mpm->shutdown_pending) {
ap_wait_or_timeout(&exitwhy, &status, &pid, pconf, ap_server_conf);
if (pid.pid != -1) {
processed_status = ap_process_child_status(&pid, exitwhy, status);
child_slot = ap_find_child_by_pid(&pid);
if (processed_status == APEXIT_CHILDFATAL) {
if (child_slot < 0
|| ap_get_scoreboard_process(child_slot)->generation
== retained->mpm->my_generation) {
retained->mpm->shutdown_pending = 1;
child_fatal = 1;
return;
} else {
ap_log_error(APLOG_MARK, APLOG_WARNING, 0, ap_server_conf, APLOGNO(00487)
"Ignoring fatal error in child of previous "
"generation (pid %ld).",
(long)pid.pid);
retained->sick_child_detected = 1;
}
} else if (processed_status == APEXIT_CHILDSICK) {
retained->sick_child_detected = 1;
}
if (child_slot >= 0) {
process_score *ps;
for (i = 0; i < threads_per_child; i++)
ap_update_child_status_from_indexes(child_slot, i,
SERVER_DEAD, NULL);
event_note_child_killed(child_slot, 0, 0);
ps = &ap_scoreboard_image->parent[child_slot];
if (!ps->quiescing)
active_daemons--;
ps->quiescing = 0;
retained->total_daemons--;
if (processed_status == APEXIT_CHILDSICK) {
retained->idle_spawn_rate[ps->bucket] = 1;
} else if (remaining_children_to_start) {
make_child(ap_server_conf, child_slot, ps->bucket);
--remaining_children_to_start;
}
}
#if APR_HAS_OTHER_CHILD
else if (apr_proc_other_child_alert(&pid, APR_OC_REASON_DEATH,
status) == 0) {
}
#endif
else if (retained->mpm->was_graceful) {
ap_log_error(APLOG_MARK, APLOG_WARNING, 0,
ap_server_conf, APLOGNO(00488)
"long lost child came home! (pid %ld)",
(long) pid.pid);
}
continue;
} else if (remaining_children_to_start) {
startup_children(remaining_children_to_start);
remaining_children_to_start = 0;
continue;
}
for (i = 0; i < num_buckets; i++) {
perform_idle_server_maintenance(i, num_buckets);
}
}
}
static int event_run(apr_pool_t * _pconf, apr_pool_t * plog, server_rec * s) {
int num_buckets = retained->mpm->num_buckets;
int remaining_children_to_start;
int i;
ap_log_pid(pconf, ap_pid_fname);
if (!retained->mpm->was_graceful) {
if (ap_run_pre_mpm(s->process->pool, SB_SHARED) != OK) {
retained->mpm->mpm_state = AP_MPMQ_STOPPING;
return !OK;
}
ap_scoreboard_image->global->running_generation = retained->mpm->my_generation;
}
if (!one_process) {
ap_fatal_signal_setup(ap_server_conf, pconf);
}
ap_unixd_mpm_set_signals(pconf, one_process);
if (active_daemons_limit < num_buckets)
active_daemons_limit = num_buckets;
if (ap_daemons_to_start < num_buckets)
ap_daemons_to_start = num_buckets;
if (min_spare_threads < threads_per_child * (num_buckets - 1) + num_buckets)
min_spare_threads = threads_per_child * (num_buckets - 1) + num_buckets;
if (max_spare_threads < min_spare_threads + (threads_per_child + 1) * num_buckets)
max_spare_threads = min_spare_threads + (threads_per_child + 1) * num_buckets;
remaining_children_to_start = ap_daemons_to_start;
if (remaining_children_to_start > active_daemons_limit) {
remaining_children_to_start = active_daemons_limit;
}
if (!retained->mpm->was_graceful) {
startup_children(remaining_children_to_start);
remaining_children_to_start = 0;
} else {
retained->hold_off_on_exponential_spawning = 10;
}
ap_log_error(APLOG_MARK, APLOG_NOTICE, 0, ap_server_conf, APLOGNO(00489)
"%s configured -- resuming normal operations",
ap_get_server_description());
ap_log_error(APLOG_MARK, APLOG_INFO, 0, ap_server_conf, APLOGNO(00490)
"Server built: %s", ap_get_server_built());
ap_log_command_line(plog, s);
ap_log_mpm_common(s);
retained->mpm->mpm_state = AP_MPMQ_RUNNING;
server_main_loop(remaining_children_to_start, num_buckets);
retained->mpm->mpm_state = AP_MPMQ_STOPPING;
if (retained->mpm->shutdown_pending && retained->mpm->is_ungraceful) {
for (i = 0; i < num_buckets; i++) {
ap_mpm_podx_killpg(all_buckets[i].pod, active_daemons_limit,
AP_MPM_PODX_RESTART);
}
ap_reclaim_child_processes(1,
event_note_child_killed);
if (!child_fatal) {
ap_remove_pid(pconf, ap_pid_fname);
ap_log_error(APLOG_MARK, APLOG_NOTICE, 0,
ap_server_conf, APLOGNO(00491) "caught SIGTERM, shutting down");
}
return DONE;
}
if (retained->mpm->shutdown_pending) {
int active_children;
int index;
apr_time_t cutoff = 0;
ap_close_listeners();
for (i = 0; i < num_buckets; i++) {
ap_mpm_podx_killpg(all_buckets[i].pod, active_daemons_limit,
AP_MPM_PODX_GRACEFUL);
}
ap_relieve_child_processes(event_note_child_killed);
if (!child_fatal) {
ap_remove_pid(pconf, ap_pid_fname);
ap_log_error(APLOG_MARK, APLOG_NOTICE, 0, ap_server_conf, APLOGNO(00492)
"caught " AP_SIG_GRACEFUL_STOP_STRING
", shutting down gracefully");
}
if (ap_graceful_shutdown_timeout) {
cutoff = apr_time_now() +
apr_time_from_sec(ap_graceful_shutdown_timeout);
}
retained->mpm->shutdown_pending = 0;
do {
apr_sleep(apr_time_from_sec(1));
ap_relieve_child_processes(event_note_child_killed);
active_children = 0;
for (index = 0; index < retained->max_daemons_limit; ++index) {
if (ap_mpm_safe_kill(MPM_CHILD_PID(index), 0) == APR_SUCCESS) {
active_children = 1;
break;
}
}
} while (!retained->mpm->shutdown_pending && active_children &&
(!ap_graceful_shutdown_timeout || apr_time_now() < cutoff));
for (i = 0; i < num_buckets; i++) {
ap_mpm_podx_killpg(all_buckets[i].pod, active_daemons_limit,
AP_MPM_PODX_RESTART);
}
ap_reclaim_child_processes(1, event_note_child_killed);
return DONE;
}
if (one_process) {
return DONE;
}
++retained->mpm->my_generation;
ap_scoreboard_image->global->running_generation = retained->mpm->my_generation;
if (!retained->mpm->is_ungraceful) {
ap_log_error(APLOG_MARK, APLOG_NOTICE, 0, ap_server_conf, APLOGNO(00493)
AP_SIG_GRACEFUL_STRING
" received. Doing graceful restart");
for (i = 0; i < num_buckets; i++) {
ap_mpm_podx_killpg(all_buckets[i].pod, active_daemons_limit,
AP_MPM_PODX_GRACEFUL);
}
} else {
for (i = 0; i < num_buckets; i++) {
ap_mpm_podx_killpg(all_buckets[i].pod, active_daemons_limit,
AP_MPM_PODX_RESTART);
}
ap_reclaim_child_processes(1,
event_note_child_killed);
ap_log_error(APLOG_MARK, APLOG_NOTICE, 0, ap_server_conf, APLOGNO(00494)
"SIGHUP received. Attempting to restart");
}
active_daemons = 0;
return OK;
}
static void setup_slave_conn(conn_rec *c, void *csd) {
event_conn_state_t *mcs;
event_conn_state_t *cs;
mcs = ap_get_module_config(c->master->conn_config, &mpm_event_module);
cs = apr_pcalloc(c->pool, sizeof(*cs));
cs->c = c;
cs->r = NULL;
cs->sc = mcs->sc;
cs->suspended = 0;
cs->p = c->pool;
cs->bucket_alloc = c->bucket_alloc;
cs->pfd = mcs->pfd;
cs->pub = mcs->pub;
cs->pub.state = CONN_STATE_READ_REQUEST_LINE;
cs->pub.sense = CONN_SENSE_DEFAULT;
c->cs = &(cs->pub);
ap_set_module_config(c->conn_config, &mpm_event_module, cs);
}
static int event_pre_connection(conn_rec *c, void *csd) {
if (c->master && (!c->cs || c->cs == c->master->cs)) {
setup_slave_conn(c, csd);
}
return OK;
}
static int event_protocol_switch(conn_rec *c, request_rec *r, server_rec *s,
const char *protocol) {
if (!r && s) {
event_conn_state_t *cs;
cs = ap_get_module_config(c->conn_config, &mpm_event_module);
cs->sc = ap_get_module_config(s->module_config, &mpm_event_module);
}
return DECLINED;
}
static int event_open_logs(apr_pool_t * p, apr_pool_t * plog,
apr_pool_t * ptemp, server_rec * s) {
int startup = 0;
int level_flags = 0;
int num_buckets = 0;
ap_listen_rec **listen_buckets;
apr_status_t rv;
int i;
pconf = p;
if (retained->mpm->module_loads == 1) {
startup = 1;
level_flags |= APLOG_STARTUP;
}
if ((num_listensocks = ap_setup_listeners(ap_server_conf)) < 1) {
ap_log_error(APLOG_MARK, APLOG_ALERT | level_flags, 0,
(startup ? NULL : s),
"no listening sockets available, shutting down");
return !OK;
}
if (one_process) {
num_buckets = 1;
} else if (retained->mpm->was_graceful) {
num_buckets = retained->mpm->num_buckets;
}
if ((rv = ap_duplicate_listeners(pconf, ap_server_conf,
&listen_buckets, &num_buckets))) {
ap_log_error(APLOG_MARK, APLOG_CRIT | level_flags, rv,
(startup ? NULL : s),
"could not duplicate listeners");
return !OK;
}
all_buckets = apr_pcalloc(pconf, num_buckets * sizeof(*all_buckets));
for (i = 0; i < num_buckets; i++) {
if (!one_process &&
(rv = ap_mpm_podx_open(pconf, &all_buckets[i].pod))) {
ap_log_error(APLOG_MARK, APLOG_CRIT | level_flags, rv,
(startup ? NULL : s),
"could not open pipe-of-death");
return !OK;
}
all_buckets[i].listeners = listen_buckets[i];
}
if (retained->mpm->max_buckets < num_buckets) {
int new_max, *new_ptr;
new_max = retained->mpm->max_buckets * 2;
if (new_max < num_buckets) {
new_max = num_buckets;
}
new_ptr = (int *)apr_palloc(ap_pglobal, new_max * sizeof(int));
memcpy(new_ptr, retained->idle_spawn_rate,
retained->mpm->num_buckets * sizeof(int));
retained->idle_spawn_rate = new_ptr;
retained->mpm->max_buckets = new_max;
}
if (retained->mpm->num_buckets < num_buckets) {
int rate_max = 1;
for (i = 0; i < retained->mpm->num_buckets; i++) {
if (rate_max < retained->idle_spawn_rate[i]) {
rate_max = retained->idle_spawn_rate[i];
}
}
for (; i < num_buckets; i++) {
retained->idle_spawn_rate[i] = rate_max;
}
}
retained->mpm->num_buckets = num_buckets;
srand((unsigned int)apr_time_now());
return OK;
}
static int event_pre_config(apr_pool_t * pconf, apr_pool_t * plog,
apr_pool_t * ptemp) {
int no_detach, debug, foreground;
apr_status_t rv;
const char *userdata_key = "mpm_event_module";
int test_atomics = 0;
debug = ap_exists_config_define("DEBUG");
if (debug) {
foreground = one_process = 1;
no_detach = 0;
} else {
one_process = ap_exists_config_define("ONE_PROCESS");
no_detach = ap_exists_config_define("NO_DETACH");
foreground = ap_exists_config_define("FOREGROUND");
}
retained = ap_retained_data_get(userdata_key);
if (!retained) {
retained = ap_retained_data_create(userdata_key, sizeof(*retained));
retained->mpm = ap_unixd_mpm_get_retained_data();
retained->max_daemons_limit = -1;
if (retained->mpm->module_loads) {
test_atomics = 1;
}
}
retained->mpm->mpm_state = AP_MPMQ_STARTING;
if (retained->mpm->baton != retained) {
retained->mpm->was_graceful = 0;
retained->mpm->baton = retained;
}
++retained->mpm->module_loads;
if (test_atomics || retained->mpm->module_loads == 2) {
static apr_uint32_t foo1, foo2;
apr_atomic_set32(&foo1, 100);
foo2 = apr_atomic_add32(&foo1, -10);
if (foo2 != 100 || foo1 != 90) {
ap_log_error(APLOG_MARK, APLOG_CRIT, 0, NULL, APLOGNO(02405)
"atomics not working as expected - add32 of negative number");
return HTTP_INTERNAL_SERVER_ERROR;
}
}
if (retained->mpm->module_loads == 2) {
rv = apr_pollset_create(&event_pollset, 1, plog,
APR_POLLSET_THREADSAFE | APR_POLLSET_NOCOPY);
if (rv != APR_SUCCESS) {
ap_log_error(APLOG_MARK, APLOG_CRIT, rv, NULL, APLOGNO(00495)
"Couldn't create a Thread Safe Pollset. "
"Is it supported on your platform?"
"Also check system or user limits!");
return HTTP_INTERNAL_SERVER_ERROR;
}
apr_pollset_destroy(event_pollset);
if (!one_process && !foreground) {
ap_fatal_signal_setup(ap_server_conf, pconf);
rv = apr_proc_detach(no_detach ? APR_PROC_DETACH_FOREGROUND
: APR_PROC_DETACH_DAEMONIZE);
if (rv != APR_SUCCESS) {
ap_log_error(APLOG_MARK, APLOG_CRIT, rv, NULL, APLOGNO(00496)
"apr_proc_detach failed");
return HTTP_INTERNAL_SERVER_ERROR;
}
}
}
parent_pid = ap_my_pid = getpid();
ap_listen_pre_config();
ap_daemons_to_start = DEFAULT_START_DAEMON;
min_spare_threads = DEFAULT_MIN_FREE_DAEMON * DEFAULT_THREADS_PER_CHILD;
max_spare_threads = DEFAULT_MAX_FREE_DAEMON * DEFAULT_THREADS_PER_CHILD;
server_limit = DEFAULT_SERVER_LIMIT;
thread_limit = DEFAULT_THREAD_LIMIT;
active_daemons_limit = server_limit;
threads_per_child = DEFAULT_THREADS_PER_CHILD;
max_workers = active_daemons_limit * threads_per_child;
defer_linger_chain = NULL;
had_healthy_child = 0;
ap_extended_status = 0;
return OK;
}
static int event_post_config(apr_pool_t *pconf, apr_pool_t *plog,
apr_pool_t *ptemp, server_rec *s) {
struct {
struct timeout_queue *tail, *q;
apr_hash_t *hash;
} wc, ka;
if (ap_state_query(AP_SQ_MAIN_STATE) == AP_SQ_MS_CREATE_PRE_CONFIG) {
return OK;
}
wc.tail = ka.tail = NULL;
wc.hash = apr_hash_make(ptemp);
ka.hash = apr_hash_make(ptemp);
linger_q = TO_QUEUE_MAKE(pconf, apr_time_from_sec(MAX_SECS_TO_LINGER),
NULL);
short_linger_q = TO_QUEUE_MAKE(pconf, apr_time_from_sec(SECONDS_TO_LINGER),
NULL);
for (; s; s = s->next) {
event_srv_cfg *sc = apr_pcalloc(pconf, sizeof *sc);
ap_set_module_config(s->module_config, &mpm_event_module, sc);
if (!wc.tail) {
wc.q = TO_QUEUE_MAKE(pconf, s->timeout, NULL);
apr_hash_set(wc.hash, &s->timeout, sizeof s->timeout, wc.q);
wc.tail = write_completion_q = wc.q;
ka.q = TO_QUEUE_MAKE(pconf, s->keep_alive_timeout, NULL);
apr_hash_set(ka.hash, &s->keep_alive_timeout,
sizeof s->keep_alive_timeout, ka.q);
ka.tail = keepalive_q = ka.q;
} else {
wc.q = apr_hash_get(wc.hash, &s->timeout, sizeof s->timeout);
if (!wc.q) {
wc.q = TO_QUEUE_MAKE(pconf, s->timeout, wc.tail);
apr_hash_set(wc.hash, &s->timeout, sizeof s->timeout, wc.q);
wc.tail = wc.tail->next = wc.q;
}
ka.q = apr_hash_get(ka.hash, &s->keep_alive_timeout,
sizeof s->keep_alive_timeout);
if (!ka.q) {
ka.q = TO_QUEUE_MAKE(pconf, s->keep_alive_timeout, ka.tail);
apr_hash_set(ka.hash, &s->keep_alive_timeout,
sizeof s->keep_alive_timeout, ka.q);
ka.tail = ka.tail->next = ka.q;
}
}
sc->wc_q = wc.q;
sc->ka_q = ka.q;
}
return OK;
}
static int event_check_config(apr_pool_t *p, apr_pool_t *plog,
apr_pool_t *ptemp, server_rec *s) {
int startup = 0;
if (retained->mpm->module_loads == 1) {
startup = 1;
}
if (server_limit > MAX_SERVER_LIMIT) {
if (startup) {
ap_log_error(APLOG_MARK, APLOG_WARNING | APLOG_STARTUP, 0, NULL, APLOGNO(00497)
"WARNING: ServerLimit of %d exceeds compile-time "
"limit of %d servers, decreasing to %d.",
server_limit, MAX_SERVER_LIMIT, MAX_SERVER_LIMIT);
} else {
ap_log_error(APLOG_MARK, APLOG_WARNING, 0, s, APLOGNO(00498)
"ServerLimit of %d exceeds compile-time limit "
"of %d, decreasing to match",
server_limit, MAX_SERVER_LIMIT);
}
server_limit = MAX_SERVER_LIMIT;
} else if (server_limit < 1) {
if (startup) {
ap_log_error(APLOG_MARK, APLOG_WARNING | APLOG_STARTUP, 0, NULL, APLOGNO(00499)
"WARNING: ServerLimit of %d not allowed, "
"increasing to 1.", server_limit);
} else {
ap_log_error(APLOG_MARK, APLOG_WARNING, 0, s, APLOGNO(00500)
"ServerLimit of %d not allowed, increasing to 1",
server_limit);
}
server_limit = 1;
}
if (!retained->first_server_limit) {
retained->first_server_limit = server_limit;
} else if (server_limit != retained->first_server_limit) {
ap_log_error(APLOG_MARK, APLOG_WARNING, 0, s, APLOGNO(00501)
"changing ServerLimit to %d from original value of %d "
"not allowed during restart",
server_limit, retained->first_server_limit);
server_limit = retained->first_server_limit;
}
if (thread_limit > MAX_THREAD_LIMIT) {
if (startup) {
ap_log_error(APLOG_MARK, APLOG_WARNING | APLOG_STARTUP, 0, NULL, APLOGNO(00502)
"WARNING: ThreadLimit of %d exceeds compile-time "
"limit of %d threads, decreasing to %d.",
thread_limit, MAX_THREAD_LIMIT, MAX_THREAD_LIMIT);
} else {
ap_log_error(APLOG_MARK, APLOG_WARNING, 0, s, APLOGNO(00503)
"ThreadLimit of %d exceeds compile-time limit "
"of %d, decreasing to match",
thread_limit, MAX_THREAD_LIMIT);
}
thread_limit = MAX_THREAD_LIMIT;
} else if (thread_limit < 1) {
if (startup) {
ap_log_error(APLOG_MARK, APLOG_WARNING | APLOG_STARTUP, 0, NULL, APLOGNO(00504)
"WARNING: ThreadLimit of %d not allowed, "
"increasing to 1.", thread_limit);
} else {
ap_log_error(APLOG_MARK, APLOG_WARNING, 0, s, APLOGNO(00505)
"ThreadLimit of %d not allowed, increasing to 1",
thread_limit);
}
thread_limit = 1;
}
if (!retained->first_thread_limit) {
retained->first_thread_limit = thread_limit;
} else if (thread_limit != retained->first_thread_limit) {
ap_log_error(APLOG_MARK, APLOG_WARNING, 0, s, APLOGNO(00506)
"changing ThreadLimit to %d from original value of %d "
"not allowed during restart",
thread_limit, retained->first_thread_limit);
thread_limit = retained->first_thread_limit;
}
if (threads_per_child > thread_limit) {
if (startup) {
ap_log_error(APLOG_MARK, APLOG_WARNING | APLOG_STARTUP, 0, NULL, APLOGNO(00507)
"WARNING: ThreadsPerChild of %d exceeds ThreadLimit "
"of %d threads, decreasing to %d. "
"To increase, please see the ThreadLimit directive.",
threads_per_child, thread_limit, thread_limit);
} else {
ap_log_error(APLOG_MARK, APLOG_WARNING, 0, s, APLOGNO(00508)
"ThreadsPerChild of %d exceeds ThreadLimit "
"of %d, decreasing to match",
threads_per_child, thread_limit);
}
threads_per_child = thread_limit;
} else if (threads_per_child < 1) {
if (startup) {
ap_log_error(APLOG_MARK, APLOG_WARNING | APLOG_STARTUP, 0, NULL, APLOGNO(00509)
"WARNING: ThreadsPerChild of %d not allowed, "
"increasing to 1.", threads_per_child);
} else {
ap_log_error(APLOG_MARK, APLOG_WARNING, 0, s, APLOGNO(00510)
"ThreadsPerChild of %d not allowed, increasing to 1",
threads_per_child);
}
threads_per_child = 1;
}
if (max_workers < threads_per_child) {
if (startup) {
ap_log_error(APLOG_MARK, APLOG_WARNING | APLOG_STARTUP, 0, NULL, APLOGNO(00511)
"WARNING: MaxRequestWorkers of %d is less than "
"ThreadsPerChild of %d, increasing to %d. "
"MaxRequestWorkers must be at least as large "
"as the number of threads in a single server.",
max_workers, threads_per_child, threads_per_child);
} else {
ap_log_error(APLOG_MARK, APLOG_WARNING, 0, s, APLOGNO(00512)
"MaxRequestWorkers of %d is less than ThreadsPerChild "
"of %d, increasing to match",
max_workers, threads_per_child);
}
max_workers = threads_per_child;
}
active_daemons_limit = max_workers / threads_per_child;
if (max_workers % threads_per_child) {
int tmp_max_workers = active_daemons_limit * threads_per_child;
if (startup) {
ap_log_error(APLOG_MARK, APLOG_WARNING | APLOG_STARTUP, 0, NULL, APLOGNO(00513)
"WARNING: MaxRequestWorkers of %d is not an integer "
"multiple of ThreadsPerChild of %d, decreasing to nearest "
"multiple %d, for a maximum of %d servers.",
max_workers, threads_per_child, tmp_max_workers,
active_daemons_limit);
} else {
ap_log_error(APLOG_MARK, APLOG_WARNING, 0, s, APLOGNO(00514)
"MaxRequestWorkers of %d is not an integer multiple "
"of ThreadsPerChild of %d, decreasing to nearest "
"multiple %d", max_workers, threads_per_child,
tmp_max_workers);
}
max_workers = tmp_max_workers;
}
if (active_daemons_limit > server_limit) {
if (startup) {
ap_log_error(APLOG_MARK, APLOG_WARNING | APLOG_STARTUP, 0, NULL, APLOGNO(00515)
"WARNING: MaxRequestWorkers of %d would require %d servers "
"and would exceed ServerLimit of %d, decreasing to %d. "
"To increase, please see the ServerLimit directive.",
max_workers, active_daemons_limit, server_limit,
server_limit * threads_per_child);
} else {
ap_log_error(APLOG_MARK, APLOG_WARNING, 0, s, APLOGNO(00516)
"MaxRequestWorkers of %d would require %d servers and "
"exceed ServerLimit of %d, decreasing to %d",
max_workers, active_daemons_limit, server_limit,
server_limit * threads_per_child);
}
active_daemons_limit = server_limit;
}
if (ap_daemons_to_start < 1) {
if (startup) {
ap_log_error(APLOG_MARK, APLOG_WARNING | APLOG_STARTUP, 0, NULL, APLOGNO(00517)
"WARNING: StartServers of %d not allowed, "
"increasing to 1.", ap_daemons_to_start);
} else {
ap_log_error(APLOG_MARK, APLOG_WARNING, 0, s, APLOGNO(00518)
"StartServers of %d not allowed, increasing to 1",
ap_daemons_to_start);
}
ap_daemons_to_start = 1;
}
if (min_spare_threads < 1) {
if (startup) {
ap_log_error(APLOG_MARK, APLOG_WARNING | APLOG_STARTUP, 0, NULL, APLOGNO(00519)
"WARNING: MinSpareThreads of %d not allowed, "
"increasing to 1 to avoid almost certain server "
"failure. Please read the documentation.",
min_spare_threads);
} else {
ap_log_error(APLOG_MARK, APLOG_WARNING, 0, s, APLOGNO(00520)
"MinSpareThreads of %d not allowed, increasing to 1",
min_spare_threads);
}
min_spare_threads = 1;
}
return OK;
}
static void event_hooks(apr_pool_t * p) {
static const char *const aszSucc[] = { "core.c", NULL };
one_process = 0;
ap_hook_open_logs(event_open_logs, NULL, aszSucc, APR_HOOK_REALLY_FIRST);
ap_hook_pre_config(event_pre_config, NULL, NULL, APR_HOOK_REALLY_FIRST);
ap_hook_post_config(event_post_config, NULL, NULL, APR_HOOK_MIDDLE);
ap_hook_check_config(event_check_config, NULL, NULL, APR_HOOK_MIDDLE);
ap_hook_mpm(event_run, NULL, NULL, APR_HOOK_MIDDLE);
ap_hook_mpm_query(event_query, NULL, NULL, APR_HOOK_MIDDLE);
ap_hook_mpm_register_timed_callback(event_register_timed_callback, NULL, NULL,
APR_HOOK_MIDDLE);
ap_hook_pre_read_request(event_pre_read_request, NULL, NULL, APR_HOOK_MIDDLE);
ap_hook_post_read_request(event_post_read_request, NULL, NULL, APR_HOOK_MIDDLE);
ap_hook_mpm_get_name(event_get_name, NULL, NULL, APR_HOOK_MIDDLE);
ap_hook_pre_connection(event_pre_connection, NULL, NULL, APR_HOOK_REALLY_FIRST);
ap_hook_protocol_switch(event_protocol_switch, NULL, NULL, APR_HOOK_REALLY_FIRST);
}
static const char *set_daemons_to_start(cmd_parms *cmd, void *dummy,
const char *arg) {
const char *err = ap_check_cmd_context(cmd, GLOBAL_ONLY);
if (err != NULL) {
return err;
}
ap_daemons_to_start = atoi(arg);
return NULL;
}
static const char *set_min_spare_threads(cmd_parms * cmd, void *dummy,
const char *arg) {
const char *err = ap_check_cmd_context(cmd, GLOBAL_ONLY);
if (err != NULL) {
return err;
}
min_spare_threads = atoi(arg);
return NULL;
}
static const char *set_max_spare_threads(cmd_parms * cmd, void *dummy,
const char *arg) {
const char *err = ap_check_cmd_context(cmd, GLOBAL_ONLY);
if (err != NULL) {
return err;
}
max_spare_threads = atoi(arg);
return NULL;
}
static const char *set_max_workers(cmd_parms * cmd, void *dummy,
const char *arg) {
const char *err = ap_check_cmd_context(cmd, GLOBAL_ONLY);
if (err != NULL) {
return err;
}
if (!strcasecmp(cmd->cmd->name, "MaxClients")) {
ap_log_error(APLOG_MARK, APLOG_INFO, 0, NULL, APLOGNO(00521)
"MaxClients is deprecated, use MaxRequestWorkers "
"instead.");
}
max_workers = atoi(arg);
return NULL;
}
static const char *set_threads_per_child(cmd_parms * cmd, void *dummy,
const char *arg) {
const char *err = ap_check_cmd_context(cmd, GLOBAL_ONLY);
if (err != NULL) {
return err;
}
threads_per_child = atoi(arg);
return NULL;
}
static const char *set_server_limit (cmd_parms *cmd, void *dummy, const char *arg) {
const char *err = ap_check_cmd_context(cmd, GLOBAL_ONLY);
if (err != NULL) {
return err;
}
server_limit = atoi(arg);
return NULL;
}
static const char *set_thread_limit(cmd_parms * cmd, void *dummy,
const char *arg) {
const char *err = ap_check_cmd_context(cmd, GLOBAL_ONLY);
if (err != NULL) {
return err;
}
thread_limit = atoi(arg);
return NULL;
}
static const char *set_worker_factor(cmd_parms * cmd, void *dummy,
const char *arg) {
double val;
char *endptr;
const char *err = ap_check_cmd_context(cmd, GLOBAL_ONLY);
if (err != NULL) {
return err;
}
val = strtod(arg, &endptr);
if (*endptr)
return "error parsing value";
if (val <= 0)
return "AsyncRequestWorkerFactor argument must be a positive number";
worker_factor = val * WORKER_FACTOR_SCALE;
if (worker_factor == 0)
worker_factor = 1;
return NULL;
}
static const command_rec event_cmds[] = {
LISTEN_COMMANDS,
AP_INIT_TAKE1("StartServers", set_daemons_to_start, NULL, RSRC_CONF,
"Number of child processes launched at server startup"),
AP_INIT_TAKE1("ServerLimit", set_server_limit, NULL, RSRC_CONF,
"Maximum number of child processes for this run of Apache"),
AP_INIT_TAKE1("MinSpareThreads", set_min_spare_threads, NULL, RSRC_CONF,
"Minimum number of idle threads, to handle request spikes"),
AP_INIT_TAKE1("MaxSpareThreads", set_max_spare_threads, NULL, RSRC_CONF,
"Maximum number of idle threads"),
AP_INIT_TAKE1("MaxClients", set_max_workers, NULL, RSRC_CONF,
"Deprecated name of MaxRequestWorkers"),
AP_INIT_TAKE1("MaxRequestWorkers", set_max_workers, NULL, RSRC_CONF,
"Maximum number of threads alive at the same time"),
AP_INIT_TAKE1("ThreadsPerChild", set_threads_per_child, NULL, RSRC_CONF,
"Number of threads each child creates"),
AP_INIT_TAKE1("ThreadLimit", set_thread_limit, NULL, RSRC_CONF,
"Maximum number of worker threads per child process for this "
"run of Apache - Upper limit for ThreadsPerChild"),
AP_INIT_TAKE1("AsyncRequestWorkerFactor", set_worker_factor, NULL, RSRC_CONF,
"How many additional connects will be accepted per idle "
"worker thread"),
AP_GRACEFUL_SHUTDOWN_TIMEOUT_COMMAND,
{NULL}
};
AP_DECLARE_MODULE(mpm_event) = {
MPM20_MODULE_STUFF,
NULL,
NULL,
NULL,
NULL,
NULL,
event_cmds,
event_hooks
};