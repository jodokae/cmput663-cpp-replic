#include "apr.h"
#include "apr_portable.h"
#include "apr_strings.h"
#include "apr_thread_proc.h"
#include "apr_signal.h"
#include "apr_tables.h"
#include "apr_getopt.h"
#include "apr_thread_mutex.h"
#define APR_WANT_STDIO
#define APR_WANT_STRFUNC
#include "apr_want.h"
#if APR_HAVE_UNISTD_H
#include <unistd.h>
#endif
#if APR_HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#if !defined(USE_WINSOCK)
#include <sys/select.h>
#endif
#include "ap_config.h"
#include "httpd.h"
#include "mpm_default.h"
#include "http_main.h"
#include "http_log.h"
#include "http_config.h"
#include "http_core.h"
#include "http_connection.h"
#include "scoreboard.h"
#include "ap_mpm.h"
#include "mpm_common.h"
#include "ap_listen.h"
#include "ap_mmn.h"
#if defined(HAVE_TIME_H)
#include <time.h>
#endif
#include <signal.h>
#include <netware.h>
#include <nks/netware.h>
#include <library.h>
#include <screen.h>
int nlmUnloadSignaled(int wait);
#if !defined(HARD_SERVER_LIMIT)
#define HARD_SERVER_LIMIT 1
#endif
#define WORKER_DEAD SERVER_DEAD
#define WORKER_STARTING SERVER_STARTING
#define WORKER_READY SERVER_READY
#define WORKER_IDLE_KILL SERVER_IDLE_KILL
#define MPM_HARD_LIMITS_FILE "/mpm_default.h"
static int ap_threads_per_child=0;
static int ap_threads_to_start=0;
static int ap_threads_min_free=0;
static int ap_threads_max_free=0;
static int ap_threads_limit=0;
static int mpm_state = AP_MPMQ_STARTING;
static int ap_max_workers_limit = -1;
int hold_screen_on_exit = 0;
static fd_set listenfds;
static int listenmaxfd;
static apr_pool_t *pconf;
static apr_pool_t *pmain;
static pid_t ap_my_pid;
static char *ap_my_addrspace = NULL;
static int die_now = 0;
static unsigned long worker_thread_count;
static int request_count;
static int InstallConsoleHandler(void);
static void RemoveConsoleHandler(void);
static int CommandLineInterpreter(scr_t screenID, const char *commandLine);
static CommandParser_t ConsoleHandler = {0, NULL, 0};
#define HANDLEDCOMMAND 0
#define NOTMYCOMMAND 1
static int show_settings = 0;
#if defined(DBPRINT_ON)
#define DBPRINT0(s) printf(s)
#define DBPRINT1(s,v1) printf(s,v1)
#define DBPRINT2(s,v1,v2) printf(s,v1,v2)
#else
#define DBPRINT0(s)
#define DBPRINT1(s,v1)
#define DBPRINT2(s,v1,v2)
#endif
static int volatile shutdown_pending;
static int volatile restart_pending;
static int volatile is_graceful;
static int volatile wait_to_finish=1;
static ap_generation_t volatile ap_my_generation=0;
static void clean_child_exit(int code, int worker_num, apr_pool_t *ptrans,
apr_bucket_alloc_t *bucket_alloc) __attribute__ ((noreturn));
static void clean_child_exit(int code, int worker_num, apr_pool_t *ptrans,
apr_bucket_alloc_t *bucket_alloc) {
apr_bucket_alloc_destroy(bucket_alloc);
if (!shutdown_pending) {
apr_pool_destroy(ptrans);
}
atomic_dec (&worker_thread_count);
if (worker_num >=0)
ap_update_child_status_from_indexes(0, worker_num, WORKER_DEAD,
(request_rec *) NULL);
NXThreadExit((void*)&code);
}
static void mpm_main_cleanup(void) {
if (pmain) {
apr_pool_destroy(pmain);
}
}
static int netware_query(int query_code, int *result, apr_status_t *rv) {
*rv = APR_SUCCESS;
switch(query_code) {
case AP_MPMQ_MAX_DAEMON_USED:
*result = 1;
break;
case AP_MPMQ_IS_THREADED:
*result = AP_MPMQ_DYNAMIC;
break;
case AP_MPMQ_IS_FORKED:
*result = AP_MPMQ_NOT_SUPPORTED;
break;
case AP_MPMQ_HARD_LIMIT_DAEMONS:
*result = HARD_SERVER_LIMIT;
break;
case AP_MPMQ_HARD_LIMIT_THREADS:
*result = HARD_THREAD_LIMIT;
break;
case AP_MPMQ_MAX_THREADS:
*result = ap_threads_limit;
break;
case AP_MPMQ_MIN_SPARE_DAEMONS:
*result = 0;
break;
case AP_MPMQ_MIN_SPARE_THREADS:
*result = ap_threads_min_free;
break;
case AP_MPMQ_MAX_SPARE_DAEMONS:
*result = 0;
break;
case AP_MPMQ_MAX_SPARE_THREADS:
*result = ap_threads_max_free;
break;
case AP_MPMQ_MAX_REQUESTS_DAEMON:
*result = ap_max_requests_per_child;
break;
case AP_MPMQ_MAX_DAEMONS:
*result = 1;
break;
case AP_MPMQ_MPM_STATE:
*result = mpm_state;
break;
case AP_MPMQ_GENERATION:
*result = ap_my_generation;
break;
default:
*rv = APR_ENOTIMPL;
break;
}
return OK;
}
static const char *netware_get_name(void) {
return "NetWare";
}
static void mpm_term(void) {
RemoveConsoleHandler();
wait_to_finish = 0;
NXThreadYield();
}
static void sig_term(int sig) {
if (shutdown_pending == 1) {
return;
}
shutdown_pending = 1;
DBPRINT0 ("waiting for threads\n");
while (wait_to_finish) {
apr_thread_yield();
}
DBPRINT0 ("goodbye\n");
}
static void restart(void) {
if (restart_pending == 1) {
return;
}
restart_pending = 1;
is_graceful = 1;
}
static void set_signals(void) {
apr_signal(SIGTERM, sig_term);
apr_signal(SIGABRT, sig_term);
}
int nlmUnloadSignaled(int wait) {
shutdown_pending = 1;
if (wait) {
while (wait_to_finish) {
NXThreadYield();
}
}
return 0;
}
#define MAX_WB_RETRIES 3
#if defined(DBINFO_ON)
static int would_block = 0;
static int retry_success = 0;
static int retry_fail = 0;
static int avg_retries = 0;
#endif
void worker_main(void *arg) {
ap_listen_rec *lr, *first_lr, *last_lr = NULL;
apr_pool_t *ptrans;
apr_allocator_t *allocator;
apr_bucket_alloc_t *bucket_alloc;
conn_rec *current_conn;
apr_status_t stat = APR_EINIT;
ap_sb_handle_t *sbh;
apr_thread_t *thd = NULL;
apr_os_thread_t osthd;
int my_worker_num = (int)arg;
apr_socket_t *csd = NULL;
int requests_this_child = 0;
apr_socket_t *sd = NULL;
fd_set main_fds;
int sockdes;
int srv;
struct timeval tv;
int wouldblock_retry;
osthd = apr_os_thread_current();
apr_os_thread_put(&thd, &osthd, pmain);
tv.tv_sec = 1;
tv.tv_usec = 0;
apr_allocator_create(&allocator);
apr_allocator_max_free_set(allocator, ap_max_mem_free);
apr_pool_create_ex(&ptrans, pmain, NULL, allocator);
apr_allocator_owner_set(allocator, ptrans);
apr_pool_tag(ptrans, "transaction");
bucket_alloc = apr_bucket_alloc_create_ex(allocator);
atomic_inc (&worker_thread_count);
while (!die_now) {
current_conn = NULL;
apr_pool_clear(ptrans);
if ((ap_max_requests_per_child > 0
&& requests_this_child++ >= ap_max_requests_per_child)) {
DBPRINT1 ("\n**Thread slot %d is shutting down", my_worker_num);
clean_child_exit(0, my_worker_num, ptrans, bucket_alloc);
}
ap_update_child_status_from_indexes(0, my_worker_num, WORKER_READY,
(request_rec *) NULL);
for (;;) {
if (shutdown_pending || restart_pending || (ap_scoreboard_image->servers[0][my_worker_num].status == WORKER_IDLE_KILL)) {
DBPRINT1 ("\nThread slot %d is shutting down\n", my_worker_num);
clean_child_exit(0, my_worker_num, ptrans, bucket_alloc);
}
memcpy(&main_fds, &listenfds, sizeof(fd_set));
srv = select(listenmaxfd + 1, &main_fds, NULL, NULL, &tv);
if (srv <= 0) {
if (srv < 0) {
ap_log_error(APLOG_MARK, APLOG_NOTICE, 0, ap_server_conf, APLOGNO(00217)
"select() failed on listen socket");
apr_thread_yield();
}
continue;
}
if (last_lr == NULL) {
lr = ap_listeners;
} else {
lr = last_lr->next;
if (!lr)
lr = ap_listeners;
}
first_lr = lr;
do {
apr_os_sock_get(&sockdes, lr->sd);
if (FD_ISSET(sockdes, &main_fds))
goto got_listener;
lr = lr->next;
if (!lr)
lr = ap_listeners;
} while (lr != first_lr);
continue;
got_listener:
last_lr = lr;
sd = lr->sd;
wouldblock_retry = MAX_WB_RETRIES;
while (wouldblock_retry) {
if ((stat = apr_socket_accept(&csd, sd, ptrans)) == APR_SUCCESS) {
break;
} else {
if (!APR_STATUS_IS_EAGAIN(stat)) {
break;
}
if (wouldblock_retry--) {
apr_thread_yield();
}
}
}
if (stat == APR_SUCCESS) {
apr_socket_opt_set(csd, APR_SO_NONBLOCK, 0);
#if defined(DBINFO_ON)
if (wouldblock_retry < MAX_WB_RETRIES) {
retry_success++;
avg_retries += (MAX_WB_RETRIES-wouldblock_retry);
}
#endif
break;
} else {
#if defined(DBINFO_ON)
if (APR_STATUS_IS_EAGAIN(stat)) {
would_block++;
retry_fail++;
} else if (
#else
if (APR_STATUS_IS_EAGAIN(stat) ||
#endif
APR_STATUS_IS_ECONNRESET(stat) ||
APR_STATUS_IS_ETIMEDOUT(stat) ||
APR_STATUS_IS_EHOSTUNREACH(stat) ||
APR_STATUS_IS_ENETUNREACH(stat)) {
;
}
#if defined(USE_WINSOCK)
else if (APR_STATUS_IS_ENETDOWN(stat)) {
ap_log_error(APLOG_MARK, APLOG_EMERG, stat, ap_server_conf, APLOGNO(00218)
"apr_socket_accept: giving up.");
clean_child_exit(APEXIT_CHILDFATAL, my_worker_num, ptrans,
bucket_alloc);
}
#endif
else {
ap_log_error(APLOG_MARK, APLOG_ERR, stat, ap_server_conf, APLOGNO(00219)
"apr_socket_accept: (client socket)");
clean_child_exit(1, my_worker_num, ptrans, bucket_alloc);
}
}
}
ap_create_sb_handle(&sbh, ptrans, 0, my_worker_num);
current_conn = ap_run_create_connection(ptrans, ap_server_conf, csd,
my_worker_num, sbh,
bucket_alloc);
if (current_conn) {
current_conn->current_thread = thd;
ap_process_connection(current_conn, csd);
ap_lingering_close(current_conn);
}
request_count++;
}
clean_child_exit(0, my_worker_num, ptrans, bucket_alloc);
}
static int make_child(server_rec *s, int slot) {
int tid;
int err=0;
NXContext_t ctx;
if (slot + 1 > ap_max_workers_limit) {
ap_max_workers_limit = slot + 1;
}
ap_update_child_status_from_indexes(0, slot, WORKER_STARTING,
(request_rec *) NULL);
if (ctx = NXContextAlloc((void (*)(void *)) worker_main, (void*)slot, NX_PRIO_MED, ap_thread_stacksize, NX_CTX_NORMAL, &err)) {
char threadName[32];
sprintf (threadName, "Apache_Worker %d", slot);
NXContextSetName(ctx, threadName);
err = NXThreadCreate(ctx, NX_THR_BIND_CONTEXT, &tid);
if (err) {
NXContextFree (ctx);
}
}
if (err) {
ap_update_child_status_from_indexes(0, slot, WORKER_DEAD,
(request_rec *) NULL);
apr_thread_yield();
return -1;
}
ap_scoreboard_image->servers[0][slot].tid = tid;
return 0;
}
static void startup_workers(int number_to_start) {
int i;
for (i = 0; number_to_start && i < ap_threads_limit; ++i) {
if (ap_scoreboard_image->servers[0][i].status != WORKER_DEAD) {
continue;
}
if (make_child(ap_server_conf, i) < 0) {
break;
}
--number_to_start;
}
}
static int idle_spawn_rate = 1;
#if !defined(MAX_SPAWN_RATE)
#define MAX_SPAWN_RATE (64)
#endif
static int hold_off_on_exponential_spawning;
static void perform_idle_server_maintenance(apr_pool_t *p) {
int i;
int idle_count;
worker_score *ws;
int free_length;
int free_slots[MAX_SPAWN_RATE];
int last_non_dead;
int total_non_dead;
free_length = 0;
idle_count = 0;
last_non_dead = -1;
total_non_dead = 0;
for (i = 0; i < ap_threads_limit; ++i) {
int status;
if (i >= ap_max_workers_limit && free_length == idle_spawn_rate)
break;
ws = &ap_scoreboard_image->servers[0][i];
status = ws->status;
if (status == WORKER_DEAD) {
if (free_length < idle_spawn_rate) {
free_slots[free_length] = i;
++free_length;
}
} else if (status == WORKER_IDLE_KILL) {
continue;
} else {
if (status <= WORKER_READY) {
++ idle_count;
}
++total_non_dead;
last_non_dead = i;
}
}
DBPRINT2("Total: %d Idle Count: %d \r", total_non_dead, idle_count);
ap_max_workers_limit = last_non_dead + 1;
if (idle_count > ap_threads_max_free) {
idle_spawn_rate = 1;
ap_update_child_status_from_indexes(0, last_non_dead, WORKER_IDLE_KILL,
(request_rec *) NULL);
DBPRINT1("\nKilling idle thread: %d\n", last_non_dead);
} else if (idle_count < ap_threads_min_free) {
if (free_length == 0) {
static int reported = 0;
if (!reported) {
ap_log_error(APLOG_MARK, APLOG_ERR, 0, ap_server_conf, APLOGNO(00220)
"server reached MaxRequestWorkers setting, consider"
" raising the MaxRequestWorkers setting");
reported = 1;
}
idle_spawn_rate = 1;
} else {
if (idle_spawn_rate >= 8) {
ap_log_error(APLOG_MARK, APLOG_INFO, 0, ap_server_conf, APLOGNO(00221)
"server seems busy, (you may need "
"to increase StartServers, or Min/MaxSpareServers), "
"spawning %d children, there are %d idle, and "
"%d total children", idle_spawn_rate,
idle_count, total_non_dead);
}
DBPRINT0("\n");
for (i = 0; i < free_length; ++i) {
DBPRINT1("Spawning additional thread slot: %d\n", free_slots[i]);
make_child(ap_server_conf, free_slots[i]);
}
if (hold_off_on_exponential_spawning) {
--hold_off_on_exponential_spawning;
} else if (idle_spawn_rate < MAX_SPAWN_RATE) {
idle_spawn_rate *= 2;
}
}
} else {
idle_spawn_rate = 1;
}
}
static void display_settings() {
int status_array[SERVER_NUM_STATUS];
int i, status, total=0;
int reqs = request_count;
#if defined(DBINFO_ON)
int wblock = would_block;
would_block = 0;
#endif
request_count = 0;
ClearScreen (getscreenhandle());
printf("%s \n", ap_get_server_description());
for (i=0; i<SERVER_NUM_STATUS; i++) {
status_array[i] = 0;
}
for (i = 0; i < ap_threads_limit; ++i) {
status = (ap_scoreboard_image->servers[0][i]).status;
status_array[status]++;
}
for (i=0; i<SERVER_NUM_STATUS; i++) {
switch(i) {
case SERVER_DEAD:
printf ("Available:\t%d\n", status_array[i]);
break;
case SERVER_STARTING:
printf ("Starting:\t%d\n", status_array[i]);
break;
case SERVER_READY:
printf ("Ready:\t\t%d\n", status_array[i]);
break;
case SERVER_BUSY_READ:
printf ("Busy:\t\t%d\n", status_array[i]);
break;
case SERVER_BUSY_WRITE:
printf ("Busy Write:\t%d\n", status_array[i]);
break;
case SERVER_BUSY_KEEPALIVE:
printf ("Busy Keepalive:\t%d\n", status_array[i]);
break;
case SERVER_BUSY_LOG:
printf ("Busy Log:\t%d\n", status_array[i]);
break;
case SERVER_BUSY_DNS:
printf ("Busy DNS:\t%d\n", status_array[i]);
break;
case SERVER_CLOSING:
printf ("Closing:\t%d\n", status_array[i]);
break;
case SERVER_GRACEFUL:
printf ("Restart:\t%d\n", status_array[i]);
break;
case SERVER_IDLE_KILL:
printf ("Idle Kill:\t%d\n", status_array[i]);
break;
default:
printf ("Unknown Status:\t%d\n", status_array[i]);
break;
}
if (i != SERVER_DEAD)
total+=status_array[i];
}
printf ("Total Running:\t%d\tout of: \t%d\n", total, ap_threads_limit);
printf ("Requests per interval:\t%d\n", reqs);
#if defined(DBINFO_ON)
printf ("Would blocks:\t%d\n", wblock);
printf ("Successful retries:\t%d\n", retry_success);
printf ("Failed retries:\t%d\n", retry_fail);
printf ("Avg retries:\t%d\n", retry_success == 0 ? 0 : avg_retries / retry_success);
#endif
}
static void show_server_data() {
ap_listen_rec *lr;
module **m;
printf("%s\n", ap_get_server_description());
if (ap_my_addrspace && (ap_my_addrspace[0] != 'O') && (ap_my_addrspace[1] != 'S'))
printf(" Running in address space %s\n", ap_my_addrspace);
printf(" Listening on port(s):");
lr = ap_listeners;
do {
printf(" %d", lr->bind_addr->port);
lr = lr->next;
} while (lr && lr != ap_listeners);
printf("\n");
for (m = ap_loaded_modules; *m != NULL; m++) {
if (((module*)*m)->dynamic_load_handle) {
printf(" Loaded dynamic module %s\n", ((module*)*m)->name);
}
}
}
static int setup_listeners(server_rec *s) {
ap_listen_rec *lr;
int sockdes;
if (ap_setup_listeners(s) < 1 ) {
ap_log_error(APLOG_MARK, APLOG_ALERT, 0, s, APLOGNO(00222)
"no listening sockets available, shutting down");
return -1;
}
listenmaxfd = -1;
FD_ZERO(&listenfds);
for (lr = ap_listeners; lr; lr = lr->next) {
apr_os_sock_get(&sockdes, lr->sd);
FD_SET(sockdes, &listenfds);
if (sockdes > listenmaxfd) {
listenmaxfd = sockdes;
}
}
return 0;
}
static int shutdown_listeners() {
ap_listen_rec *lr;
for (lr = ap_listeners; lr; lr = lr->next) {
apr_socket_close(lr->sd);
}
ap_listeners = NULL;
return 0;
}
static int netware_run(apr_pool_t *_pconf, apr_pool_t *plog, server_rec *s) {
apr_status_t status=0;
pconf = _pconf;
ap_server_conf = s;
if (setup_listeners(s)) {
ap_log_error(APLOG_MARK, APLOG_ALERT, status, s, APLOGNO(00223)
"no listening sockets available, shutting down");
return !OK;
}
restart_pending = shutdown_pending = 0;
worker_thread_count = 0;
if (!is_graceful) {
if (ap_run_pre_mpm(s->process->pool, SB_NOT_SHARED) != OK) {
return !OK;
}
}
ap_scoreboard_image->parent[0].pid = getpid();
ap_run_child_status(ap_server_conf,
ap_scoreboard_image->parent[0].pid,
ap_my_generation,
0,
MPM_CHILD_STARTED);
set_signals();
apr_pool_create(&pmain, pconf);
ap_run_child_init(pmain, ap_server_conf);
if (ap_threads_max_free < ap_threads_min_free + 1)
ap_threads_max_free = ap_threads_min_free + 1;
request_count = 0;
startup_workers(ap_threads_to_start);
if (hold_screen_on_exit > 0) {
hold_screen_on_exit = 0;
}
ap_log_error(APLOG_MARK, APLOG_NOTICE, 0, ap_server_conf, APLOGNO(00224)
"%s configured -- resuming normal operations",
ap_get_server_description());
ap_log_error(APLOG_MARK, APLOG_INFO, 0, ap_server_conf, APLOGNO(00225)
"Server built: %s", ap_get_server_built());
ap_log_command_line(plog, s);
ap_log_mpm_common(s);
show_server_data();
mpm_state = AP_MPMQ_RUNNING;
while (!restart_pending && !shutdown_pending) {
perform_idle_server_maintenance(pconf);
if (show_settings)
display_settings();
apr_thread_yield();
apr_sleep(SCOREBOARD_MAINTENANCE_INTERVAL);
}
mpm_state = AP_MPMQ_STOPPING;
ap_run_child_status(ap_server_conf,
ap_scoreboard_image->parent[0].pid,
ap_my_generation,
0,
MPM_CHILD_EXITED);
if (shutdown_pending) {
ap_log_error(APLOG_MARK, APLOG_NOTICE, 0, ap_server_conf, APLOGNO(00226)
"caught SIGTERM, shutting down");
while (worker_thread_count > 0) {
printf ("\rShutdown pending. Waiting for %lu thread(s) to terminate...",
worker_thread_count);
apr_thread_yield();
}
mpm_main_cleanup();
return DONE;
} else {
++ap_my_generation;
ap_scoreboard_image->global->running_generation = ap_my_generation;
ap_log_error(APLOG_MARK, APLOG_NOTICE, 0, ap_server_conf, APLOGNO(00227)
"Graceful restart requested, doing restart");
while (worker_thread_count > 0) {
printf ("\rRestart pending. Waiting for %lu thread(s) to terminate...",
worker_thread_count);
apr_thread_yield();
}
printf ("\nRestarting...\n");
}
mpm_main_cleanup();
return OK;
}
static int netware_pre_config(apr_pool_t *p, apr_pool_t *plog, apr_pool_t *ptemp) {
char *addrname = NULL;
mpm_state = AP_MPMQ_STARTING;
is_graceful = 0;
ap_my_pid = getpid();
addrname = getaddressspacename (NULL, NULL);
if (addrname) {
ap_my_addrspace = apr_pstrdup (p, addrname);
free (addrname);
}
#if !defined(USE_WINSOCK)
ap_listen_pre_config();
#endif
ap_threads_to_start = DEFAULT_START_THREADS;
ap_threads_min_free = DEFAULT_MIN_FREE_THREADS;
ap_threads_max_free = DEFAULT_MAX_FREE_THREADS;
ap_threads_limit = HARD_THREAD_LIMIT;
ap_extended_status = 0;
ap_thread_stacksize = DEFAULT_THREAD_STACKSIZE;
return OK;
}
static int netware_check_config(apr_pool_t *p, apr_pool_t *plog,
apr_pool_t *ptemp, server_rec *s) {
static int restart_num = 0;
int startup = 0;
if (restart_num++ == 0) {
startup = 1;
}
if (ap_threads_limit > HARD_THREAD_LIMIT) {
if (startup) {
ap_log_error(APLOG_MARK, APLOG_WARNING | APLOG_STARTUP, 0, NULL, APLOGNO(00228)
"WARNING: MaxThreads of %d exceeds compile-time "
"limit of %d threads, decreasing to %d. "
"To increase, please see the HARD_THREAD_LIMIT "
"define in server/mpm/netware%s.",
ap_threads_limit, HARD_THREAD_LIMIT, HARD_THREAD_LIMIT,
MPM_HARD_LIMITS_FILE);
} else {
ap_log_error(APLOG_MARK, APLOG_WARNING, 0, s, APLOGNO(00229)
"MaxThreads of %d exceeds compile-time limit "
"of %d, decreasing to match",
ap_threads_limit, HARD_THREAD_LIMIT);
}
ap_threads_limit = HARD_THREAD_LIMIT;
} else if (ap_threads_limit < 1) {
if (startup) {
ap_log_error(APLOG_MARK, APLOG_WARNING | APLOG_STARTUP, 0, NULL, APLOGNO(00230)
"WARNING: MaxThreads of %d not allowed, "
"increasing to 1.", ap_threads_limit);
} else {
ap_log_error(APLOG_MARK, APLOG_WARNING, 0, s, APLOGNO(02661)
"MaxThreads of %d not allowed, increasing to 1",
ap_threads_limit);
}
ap_threads_limit = 1;
}
if (ap_threads_to_start < 0) {
if (startup) {
ap_log_error(APLOG_MARK, APLOG_WARNING | APLOG_STARTUP, 0, NULL, APLOGNO(00231)
"WARNING: StartThreads of %d not allowed, "
"increasing to 1.", ap_threads_to_start);
} else {
ap_log_error(APLOG_MARK, APLOG_WARNING, 0, s, APLOGNO(00232)
"StartThreads of %d not allowed, increasing to 1",
ap_threads_to_start);
}
ap_threads_to_start = 1;
}
if (ap_threads_min_free < 1) {
if (startup) {
ap_log_error(APLOG_MARK, APLOG_WARNING | APLOG_STARTUP, 0, NULL, APLOGNO(00233)
"WARNING: MinSpareThreads of %d not allowed, "
"increasing to 1 to avoid almost certain server failure. "
"Please read the documentation.", ap_threads_min_free);
} else {
ap_log_error(APLOG_MARK, APLOG_WARNING, 0, s, APLOGNO(00234)
"MinSpareThreads of %d not allowed, increasing to 1",
ap_threads_min_free);
}
ap_threads_min_free = 1;
}
return OK;
}
static void netware_mpm_hooks(apr_pool_t *p) {
static const char * const predecessors[] = {"core.c", NULL};
ap_hook_pre_config(netware_pre_config, predecessors, NULL, APR_HOOK_MIDDLE);
ap_hook_check_config(netware_check_config, NULL, NULL, APR_HOOK_MIDDLE);
ap_hook_mpm(netware_run, NULL, NULL, APR_HOOK_MIDDLE);
ap_hook_mpm_query(netware_query, NULL, NULL, APR_HOOK_MIDDLE);
ap_hook_mpm_get_name(netware_get_name, NULL, NULL, APR_HOOK_MIDDLE);
}
static void netware_rewrite_args(process_rec *process) {
char *def_server_root;
char optbuf[3];
const char *opt_arg;
apr_getopt_t *opt;
apr_array_header_t *mpm_new_argv;
atexit (mpm_term);
InstallConsoleHandler();
hold_screen_on_exit = 1;
if (process->argc > 0) {
char *s = apr_pstrdup (process->pconf, process->argv[0]);
if (s) {
int i, len = strlen(s);
for (i=len; i; i--) {
if (s[i] == '\\' || s[i] == '/') {
s[i] = '\0';
apr_filepath_merge(&def_server_root, NULL, s,
APR_FILEPATH_TRUENAME, process->pool);
break;
}
}
mpm_new_argv = apr_array_make(process->pool, process->argc + 2,
sizeof(const char *));
*(const char **)apr_array_push(mpm_new_argv) = process->argv[0];
*(const char **)apr_array_push(mpm_new_argv) = "-d";
*(const char **)apr_array_push(mpm_new_argv) = def_server_root;
optbuf[0] = '-';
optbuf[2] = '\0';
apr_getopt_init(&opt, process->pool, process->argc, process->argv);
while (apr_getopt(opt, AP_SERVER_BASEARGS"n:", optbuf + 1, &opt_arg) == APR_SUCCESS) {
switch (optbuf[1]) {
case 'n':
if (opt_arg) {
renamescreen(opt_arg);
}
break;
case 'E':
hold_screen_on_exit = -1;
default:
*(const char **)apr_array_push(mpm_new_argv) =
apr_pstrdup(process->pool, optbuf);
if (opt_arg) {
*(const char **)apr_array_push(mpm_new_argv) = opt_arg;
}
break;
}
}
process->argc = mpm_new_argv->nelts;
process->argv = (const char * const *) mpm_new_argv->elts;
}
}
}
static int CommandLineInterpreter(scr_t screenID, const char *commandLine) {
char *szCommand = "APACHE2 ";
int iCommandLen = 8;
char szcommandLine[256];
char *pID;
screenID = screenID;
if (commandLine == NULL)
return NOTMYCOMMAND;
if (strlen(commandLine) <= strlen(szCommand))
return NOTMYCOMMAND;
apr_cpystrn(szcommandLine, commandLine, sizeof(szcommandLine));
if (!strnicmp(szCommand, szcommandLine, iCommandLen)) {
ActivateScreen (getscreenhandle());
pID = strstr (szcommandLine, "-p");
if ((pID == NULL) && nlmisloadedprotected())
return NOTMYCOMMAND;
if (pID) {
pID = &pID[2];
while (*pID && (*pID == ' '))
pID++;
}
if (pID && ap_my_addrspace && strnicmp(pID, ap_my_addrspace, strlen(ap_my_addrspace)))
return NOTMYCOMMAND;
if (!strnicmp("RESTART",&szcommandLine[iCommandLen],3)) {
printf("Restart Requested...\n");
restart();
} else if (!strnicmp("VERSION",&szcommandLine[iCommandLen],3)) {
printf("Server version: %s\n", ap_get_server_description());
printf("Server built: %s\n", ap_get_server_built());
} else if (!strnicmp("MODULES",&szcommandLine[iCommandLen],3)) {
ap_show_modules();
} else if (!strnicmp("DIRECTIVES",&szcommandLine[iCommandLen],3)) {
ap_show_directives();
} else if (!strnicmp("SHUTDOWN",&szcommandLine[iCommandLen],3)) {
printf("Shutdown Requested...\n");
shutdown_pending = 1;
} else if (!strnicmp("SETTINGS",&szcommandLine[iCommandLen],3)) {
if (show_settings) {
show_settings = 0;
ClearScreen (getscreenhandle());
show_server_data();
} else {
show_settings = 1;
display_settings();
}
} else {
show_settings = 0;
if (strnicmp("HELP",&szcommandLine[iCommandLen],3))
printf("Unknown APACHE2 command %s\n", &szcommandLine[iCommandLen]);
printf("Usage: APACHE2 [command] [-p <instance ID>]\n");
printf("Commands:\n");
printf("\tDIRECTIVES - Show directives\n");
printf("\tHELP - Display this help information\n");
printf("\tMODULES - Show a list of the loaded modules\n");
printf("\tRESTART - Reread the configuration file and restart Apache\n");
printf("\tSETTINGS - Show current thread status\n");
printf("\tSHUTDOWN - Shutdown Apache\n");
printf("\tVERSION - Display the server version information\n");
}
return HANDLEDCOMMAND;
}
return NOTMYCOMMAND;
}
static int InstallConsoleHandler(void) {
NX_WRAP_INTERFACE(CommandLineInterpreter, 2, (void*)&(ConsoleHandler.parser));
ConsoleHandler.rTag = AllocateResourceTag(getnlmhandle(), "Command Line Processor",
ConsoleCommandSignature);
if (!ConsoleHandler.rTag) {
printf("Error on allocate resource tag\n");
return 1;
}
RegisterConsoleCommand(&ConsoleHandler);
return 0;
}
static void RemoveConsoleHandler(void) {
UnRegisterConsoleCommand(&ConsoleHandler);
NX_UNWRAP_INTERFACE(ConsoleHandler.parser);
}
static const char *set_threads_to_start(cmd_parms *cmd, void *dummy, const char *arg) {
const char *err = ap_check_cmd_context(cmd, GLOBAL_ONLY);
if (err != NULL) {
return err;
}
ap_threads_to_start = atoi(arg);
return NULL;
}
static const char *set_min_free_threads(cmd_parms *cmd, void *dummy, const char *arg) {
const char *err = ap_check_cmd_context(cmd, GLOBAL_ONLY);
if (err != NULL) {
return err;
}
ap_threads_min_free = atoi(arg);
return NULL;
}
static const char *set_max_free_threads(cmd_parms *cmd, void *dummy, const char *arg) {
const char *err = ap_check_cmd_context(cmd, GLOBAL_ONLY);
if (err != NULL) {
return err;
}
ap_threads_max_free = atoi(arg);
return NULL;
}
static const char *set_thread_limit (cmd_parms *cmd, void *dummy, const char *arg) {
const char *err = ap_check_cmd_context(cmd, GLOBAL_ONLY);
if (err != NULL) {
return err;
}
ap_threads_limit = atoi(arg);
return NULL;
}
static const command_rec netware_mpm_cmds[] = {
LISTEN_COMMANDS,
AP_INIT_TAKE1("StartThreads", set_threads_to_start, NULL, RSRC_CONF,
"Number of worker threads launched at server startup"),
AP_INIT_TAKE1("MinSpareThreads", set_min_free_threads, NULL, RSRC_CONF,
"Minimum number of idle threads, to handle request spikes"),
AP_INIT_TAKE1("MaxSpareThreads", set_max_free_threads, NULL, RSRC_CONF,
"Maximum number of idle threads"),
AP_INIT_TAKE1("MaxThreads", set_thread_limit, NULL, RSRC_CONF,
"Maximum number of worker threads alive at the same time"),
{ NULL }
};
AP_DECLARE_MODULE(mpm_netware) = {
MPM20_MODULE_STUFF,
netware_rewrite_args,
NULL,
NULL,
NULL,
NULL,
netware_mpm_cmds,
netware_mpm_hooks,
};
