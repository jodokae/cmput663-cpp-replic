#include "apr.h"
#include "apr_portable.h"
#include "apr_strings.h"
#include "apr_thread_proc.h"
#include "apr_signal.h"
#define APR_WANT_STDIO
#define APR_WANT_STRFUNC
#include "apr_want.h"
#if APR_HAVE_UNISTD_H
#include <unistd.h>
#endif
#if APR_HAVE_SYS_TYPES_H
#include <sys/types.h>
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
#include "util_mutex.h"
#include "unixd.h"
#include "mpm_common.h"
#include "ap_listen.h"
#include "ap_mmn.h"
#include "apr_poll.h"
#include <stdlib.h>
#if defined(HAVE_TIME_H)
#include <time.h>
#endif
#if defined(HAVE_SYS_PROCESSOR_H)
#include <sys/processor.h>
#endif
#include <signal.h>
#include <sys/times.h>
#if !defined(DEFAULT_SERVER_LIMIT)
#define DEFAULT_SERVER_LIMIT 256
#endif
#if !defined(MAX_SERVER_LIMIT)
#define MAX_SERVER_LIMIT 200000
#endif
#if !defined(HARD_THREAD_LIMIT)
#define HARD_THREAD_LIMIT 1
#endif
static int ap_daemons_to_start=0;
static int ap_daemons_min_free=0;
static int ap_daemons_max_free=0;
static int ap_daemons_limit=0;
static int server_limit = 0;
typedef struct prefork_retained_data {
ap_unixd_mpm_retained_data *mpm;
int first_server_limit;
int maxclients_reported;
int max_daemons_limit;
int idle_spawn_rate;
#if !defined(MAX_SPAWN_RATE)
#define MAX_SPAWN_RATE (32)
#endif
int hold_off_on_exponential_spawning;
} prefork_retained_data;
static prefork_retained_data *retained;
typedef struct prefork_child_bucket {
ap_pod_t *pod;
ap_listen_rec *listeners;
apr_proc_mutex_t *mutex;
} prefork_child_bucket;
static prefork_child_bucket *all_buckets,
*my_bucket;
#define MPM_CHILD_PID(i) (ap_scoreboard_image->parent[i].pid)
static int one_process = 0;
static apr_pool_t *pconf;
static apr_pool_t *pchild;
static pid_t ap_my_pid;
static pid_t parent_pid;
static int my_child_num;
#if defined(GPROF)
static void chdir_for_gprof(void) {
core_server_config *sconf =
ap_get_core_module_config(ap_server_conf->module_config);
char *dir = sconf->gprof_dir;
const char *use_dir;
if(dir) {
apr_status_t res;
char *buf = NULL ;
int len = strlen(sconf->gprof_dir) - 1;
if(*(dir + len) == '%') {
dir[len] = '\0';
buf = ap_append_pid(pconf, dir, "gprof.");
}
use_dir = ap_server_root_relative(pconf, buf ? buf : dir);
res = apr_dir_make(use_dir,
APR_UREAD | APR_UWRITE | APR_UEXECUTE |
APR_GREAD | APR_GEXECUTE |
APR_WREAD | APR_WEXECUTE, pconf);
if(res != APR_SUCCESS && !APR_STATUS_IS_EEXIST(res)) {
ap_log_error(APLOG_MARK, APLOG_ERR, res, ap_server_conf, APLOGNO(00142)
"gprof: error creating directory %s", dir);
}
} else {
use_dir = ap_runtime_dir_relative(pconf, "");
}
chdir(use_dir);
}
#else
#define chdir_for_gprof()
#endif
static void prefork_note_child_killed(int childnum, pid_t pid,
ap_generation_t gen) {
AP_DEBUG_ASSERT(childnum != -1);
ap_run_child_status(ap_server_conf,
ap_scoreboard_image->parent[childnum].pid,
ap_scoreboard_image->parent[childnum].generation,
childnum, MPM_CHILD_EXITED);
ap_scoreboard_image->parent[childnum].pid = 0;
}
static void prefork_note_child_started(int slot, pid_t pid) {
ap_scoreboard_image->parent[slot].pid = pid;
ap_run_child_status(ap_server_conf,
ap_scoreboard_image->parent[slot].pid,
retained->mpm->my_generation, slot, MPM_CHILD_STARTED);
}
static void clean_child_exit(int code) __attribute__ ((noreturn));
static void clean_child_exit(int code) {
retained->mpm->mpm_state = AP_MPMQ_STOPPING;
apr_signal(SIGHUP, SIG_IGN);
apr_signal(SIGTERM, SIG_IGN);
if (pchild) {
apr_pool_destroy(pchild);
}
if (one_process) {
prefork_note_child_killed( 0, 0, 0);
}
ap_mpm_pod_close(my_bucket->pod);
chdir_for_gprof();
exit(code);
}
static apr_status_t accept_mutex_on(void) {
apr_status_t rv = apr_proc_mutex_lock(my_bucket->mutex);
if (rv != APR_SUCCESS) {
const char *msg = "couldn't grab the accept mutex";
if (retained->mpm->my_generation !=
ap_scoreboard_image->global->running_generation) {
ap_log_error(APLOG_MARK, APLOG_DEBUG, rv, ap_server_conf, APLOGNO(00143) "%s", msg);
clean_child_exit(0);
} else {
ap_log_error(APLOG_MARK, APLOG_EMERG, rv, ap_server_conf, APLOGNO(00144) "%s", msg);
exit(APEXIT_CHILDFATAL);
}
}
return APR_SUCCESS;
}
static apr_status_t accept_mutex_off(void) {
apr_status_t rv = apr_proc_mutex_unlock(my_bucket->mutex);
if (rv != APR_SUCCESS) {
const char *msg = "couldn't release the accept mutex";
if (retained->mpm->my_generation !=
ap_scoreboard_image->global->running_generation) {
ap_log_error(APLOG_MARK, APLOG_DEBUG, rv, ap_server_conf, APLOGNO(00145) "%s", msg);
} else {
ap_log_error(APLOG_MARK, APLOG_EMERG, rv, ap_server_conf, APLOGNO(00146) "%s", msg);
exit(APEXIT_CHILDFATAL);
}
}
return APR_SUCCESS;
}
#if defined(SINGLE_LISTEN_UNSERIALIZED_ACCEPT)
#define SAFE_ACCEPT(stmt) (ap_listeners->next ? (stmt) : APR_SUCCESS)
#else
#define SAFE_ACCEPT(stmt) (stmt)
#endif
static int prefork_query(int query_code, int *result, apr_status_t *rv) {
*rv = APR_SUCCESS;
switch(query_code) {
case AP_MPMQ_MAX_DAEMON_USED:
*result = ap_daemons_limit;
break;
case AP_MPMQ_IS_THREADED:
*result = AP_MPMQ_NOT_SUPPORTED;
break;
case AP_MPMQ_IS_FORKED:
*result = AP_MPMQ_DYNAMIC;
break;
case AP_MPMQ_HARD_LIMIT_DAEMONS:
*result = server_limit;
break;
case AP_MPMQ_HARD_LIMIT_THREADS:
*result = HARD_THREAD_LIMIT;
break;
case AP_MPMQ_MAX_THREADS:
*result = 1;
break;
case AP_MPMQ_MIN_SPARE_DAEMONS:
*result = ap_daemons_min_free;
break;
case AP_MPMQ_MIN_SPARE_THREADS:
*result = 0;
break;
case AP_MPMQ_MAX_SPARE_DAEMONS:
*result = ap_daemons_max_free;
break;
case AP_MPMQ_MAX_SPARE_THREADS:
*result = 0;
break;
case AP_MPMQ_MAX_REQUESTS_DAEMON:
*result = ap_max_requests_per_child;
break;
case AP_MPMQ_MAX_DAEMONS:
*result = ap_daemons_limit;
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
static const char *prefork_get_name(void) {
return "prefork";
}
static void just_die(int sig) {
clean_child_exit(0);
}
static int volatile die_now = 0;
static void stop_listening(int sig) {
retained->mpm->mpm_state = AP_MPMQ_STOPPING;
ap_close_listeners_ex(my_bucket->listeners);
die_now = 1;
}
static int requests_this_child;
static int num_listensocks = 0;
static void child_main(int child_num_arg, int child_bucket) {
#if APR_HAS_THREADS
apr_thread_t *thd = NULL;
apr_os_thread_t osthd;
#endif
apr_pool_t *ptrans;
apr_allocator_t *allocator;
apr_status_t status;
int i;
ap_listen_rec *lr;
apr_pollset_t *pollset;
ap_sb_handle_t *sbh;
apr_bucket_alloc_t *bucket_alloc;
int last_poll_idx = 0;
const char *lockfile;
retained->mpm->mpm_state = AP_MPMQ_STARTING;
my_child_num = child_num_arg;
ap_my_pid = getpid();
requests_this_child = 0;
ap_fatal_signal_child_setup(ap_server_conf);
apr_allocator_create(&allocator);
apr_allocator_max_free_set(allocator, ap_max_mem_free);
apr_pool_create_ex(&pchild, pconf, NULL, allocator);
apr_allocator_owner_set(allocator, pchild);
apr_pool_tag(pchild, "pchild");
#if APR_HAS_THREADS
osthd = apr_os_thread_current();
apr_os_thread_put(&thd, &osthd, pchild);
#endif
apr_pool_create(&ptrans, pchild);
apr_pool_tag(ptrans, "transaction");
for (i = 0; i < retained->mpm->num_buckets; i++) {
if (i != child_bucket) {
ap_close_listeners_ex(all_buckets[i].listeners);
ap_mpm_pod_close(all_buckets[i].pod);
}
}
ap_reopen_scoreboard(pchild, NULL, 0);
status = SAFE_ACCEPT(apr_proc_mutex_child_init(&my_bucket->mutex,
apr_proc_mutex_lockfile(my_bucket->mutex),
pchild));
if (status != APR_SUCCESS) {
lockfile = apr_proc_mutex_lockfile(my_bucket->mutex);
ap_log_error(APLOG_MARK, APLOG_EMERG, status, ap_server_conf, APLOGNO(00155)
"Couldn't initialize cross-process lock in child "
"(%s) (%s)",
lockfile ? lockfile : "none",
apr_proc_mutex_name(my_bucket->mutex));
clean_child_exit(APEXIT_CHILDFATAL);
}
if (ap_run_drop_privileges(pchild, ap_server_conf)) {
clean_child_exit(APEXIT_CHILDFATAL);
}
ap_run_child_init(pchild, ap_server_conf);
ap_create_sb_handle(&sbh, pchild, my_child_num, 0);
(void) ap_update_child_status(sbh, SERVER_READY, (request_rec *) NULL);
status = apr_pollset_create(&pollset, num_listensocks, pchild,
APR_POLLSET_NOCOPY);
if (status != APR_SUCCESS) {
ap_log_error(APLOG_MARK, APLOG_EMERG, status, ap_server_conf, APLOGNO(00156)
"Couldn't create pollset in child; check system or user limits");
clean_child_exit(APEXIT_CHILDSICK);
}
for (lr = my_bucket->listeners, i = num_listensocks; i--; lr = lr->next) {
apr_pollfd_t *pfd = apr_pcalloc(pchild, sizeof *pfd);
pfd->desc_type = APR_POLL_SOCKET;
pfd->desc.s = lr->sd;
pfd->reqevents = APR_POLLIN;
pfd->client_data = lr;
status = apr_pollset_add(pollset, pfd);
if (status != APR_SUCCESS) {
if (!die_now) {
ap_log_error(APLOG_MARK, APLOG_EMERG, status, ap_server_conf, APLOGNO(00157)
"Couldn't add listener to pollset; check system or user limits");
clean_child_exit(APEXIT_CHILDSICK);
}
clean_child_exit(0);
}
lr->accept_func = ap_unixd_accept;
}
retained->mpm->mpm_state = AP_MPMQ_RUNNING;
bucket_alloc = apr_bucket_alloc_create(pchild);
while (!die_now
&& !retained->mpm->shutdown_pending
&& !retained->mpm->restart_pending) {
conn_rec *current_conn;
void *csd;
apr_pool_clear(ptrans);
if ((ap_max_requests_per_child > 0
&& requests_this_child++ >= ap_max_requests_per_child)) {
clean_child_exit(0);
}
(void) ap_update_child_status(sbh, SERVER_READY, (request_rec *) NULL);
SAFE_ACCEPT(accept_mutex_on());
if (num_listensocks == 1) {
lr = my_bucket->listeners;
} else {
for (;;) {
apr_int32_t numdesc;
const apr_pollfd_t *pdesc;
if (die_now
|| retained->mpm->shutdown_pending
|| retained->mpm->restart_pending) {
SAFE_ACCEPT(accept_mutex_off());
clean_child_exit(0);
}
status = apr_pollset_poll(pollset, apr_time_from_sec(10),
&numdesc, &pdesc);
if (status != APR_SUCCESS) {
if (APR_STATUS_IS_TIMEUP(status) ||
APR_STATUS_IS_EINTR(status)) {
continue;
}
ap_log_error(APLOG_MARK, APLOG_ERR, status,
ap_server_conf, APLOGNO(00158) "apr_pollset_poll: (listen)");
SAFE_ACCEPT(accept_mutex_off());
clean_child_exit(APEXIT_CHILDSICK);
}
if (last_poll_idx >= numdesc)
last_poll_idx = 0;
lr = pdesc[last_poll_idx++].client_data;
goto got_fd;
}
}
got_fd:
status = lr->accept_func(&csd, lr, ptrans);
SAFE_ACCEPT(accept_mutex_off());
if (status == APR_EGENERAL) {
clean_child_exit(APEXIT_CHILDSICK);
} else if (status != APR_SUCCESS) {
continue;
}
current_conn = ap_run_create_connection(ptrans, ap_server_conf, csd, my_child_num, sbh, bucket_alloc);
if (current_conn) {
#if APR_HAS_THREADS
current_conn->current_thread = thd;
#endif
ap_process_connection(current_conn, csd);
ap_lingering_close(current_conn);
}
if (ap_mpm_pod_check(my_bucket->pod) == APR_SUCCESS) {
die_now = 1;
} else if (retained->mpm->my_generation !=
ap_scoreboard_image->global->running_generation) {
die_now = 1;
}
}
apr_pool_clear(ptrans);
clean_child_exit(0);
}
static int make_child(server_rec *s, int slot, int bucket) {
int pid;
if (slot + 1 > retained->max_daemons_limit) {
retained->max_daemons_limit = slot + 1;
}
if (one_process) {
my_bucket = &all_buckets[0];
prefork_note_child_started(slot, getpid());
child_main(slot, 0);
ap_assert(0);
return -1;
}
(void) ap_update_child_status_from_indexes(slot, 0, SERVER_STARTING,
(request_rec *) NULL);
#if defined(_OSD_POSIX)
if ((pid = os_fork(ap_unixd_config.user_name)) == -1) {
#else
if ((pid = fork()) == -1) {
#endif
ap_log_error(APLOG_MARK, APLOG_ERR, errno, s, APLOGNO(00159) "fork: Unable to fork new process");
(void) ap_update_child_status_from_indexes(slot, 0, SERVER_DEAD,
(request_rec *) NULL);
sleep(10);
return -1;
}
if (!pid) {
my_bucket = &all_buckets[bucket];
#if defined(HAVE_BINDPROCESSOR)
int status = bindprocessor(BINDPROCESS, (int)getpid(),
PROCESSOR_CLASS_ANY);
if (status != OK) {
ap_log_error(APLOG_MARK, APLOG_DEBUG, errno,
ap_server_conf, APLOGNO(00160) "processor unbind failed");
}
#endif
RAISE_SIGSTOP(MAKE_CHILD);
AP_MONCONTROL(1);
apr_signal(SIGHUP, just_die);
apr_signal(SIGTERM, just_die);
apr_signal(SIGINT, SIG_IGN);
apr_signal(AP_SIG_GRACEFUL, stop_listening);
child_main(slot, bucket);
}
ap_scoreboard_image->parent[slot].bucket = bucket;
prefork_note_child_started(slot, pid);
return 0;
}
static void startup_children(int number_to_start) {
int i;
for (i = 0; number_to_start && i < ap_daemons_limit; ++i) {
if (ap_scoreboard_image->servers[i][0].status != SERVER_DEAD) {
continue;
}
if (make_child(ap_server_conf, i, i % retained->mpm->num_buckets) < 0) {
break;
}
--number_to_start;
}
}
static void perform_idle_server_maintenance(apr_pool_t *p) {
static int bucket_make_child_record = -1;
static int bucket_kill_child_record = -1;
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
for (i = 0; i < ap_daemons_limit; ++i) {
int status;
if (i >= retained->max_daemons_limit && free_length == retained->idle_spawn_rate)
break;
ws = &ap_scoreboard_image->servers[i][0];
status = ws->status;
if (status == SERVER_DEAD) {
if (free_length < retained->idle_spawn_rate) {
free_slots[free_length] = i;
++free_length;
}
} else {
if (status <= SERVER_READY) {
++ idle_count;
}
++total_non_dead;
last_non_dead = i;
}
}
retained->max_daemons_limit = last_non_dead + 1;
if (idle_count > ap_daemons_max_free) {
bucket_kill_child_record = (bucket_kill_child_record + 1) % retained->mpm->num_buckets;
ap_mpm_pod_signal(all_buckets[bucket_kill_child_record].pod);
retained->idle_spawn_rate = 1;
} else if (idle_count < ap_daemons_min_free) {
if (free_length == 0) {
if (!retained->maxclients_reported) {
ap_log_error(APLOG_MARK, APLOG_ERR, 0, ap_server_conf, APLOGNO(00161)
"server reached MaxRequestWorkers setting, consider"
" raising the MaxRequestWorkers setting");
retained->maxclients_reported = 1;
}
retained->idle_spawn_rate = 1;
} else {
if (retained->idle_spawn_rate >= 8) {
ap_log_error(APLOG_MARK, APLOG_INFO, 0, ap_server_conf, APLOGNO(00162)
"server seems busy, (you may need "
"to increase StartServers, or Min/MaxSpareServers), "
"spawning %d children, there are %d idle, and "
"%d total children", retained->idle_spawn_rate,
idle_count, total_non_dead);
}
for (i = 0; i < free_length; ++i) {
bucket_make_child_record++;
bucket_make_child_record %= retained->mpm->num_buckets;
make_child(ap_server_conf, free_slots[i],
bucket_make_child_record);
}
if (retained->hold_off_on_exponential_spawning) {
--retained->hold_off_on_exponential_spawning;
} else if (retained->idle_spawn_rate < MAX_SPAWN_RATE) {
retained->idle_spawn_rate *= 2;
}
}
} else {
retained->idle_spawn_rate = 1;
}
}
static int prefork_run(apr_pool_t *_pconf, apr_pool_t *plog, server_rec *s) {
int index;
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
if (one_process) {
AP_MONCONTROL(1);
make_child(ap_server_conf, 0, 0);
ap_assert(0);
return !OK;
}
if (ap_daemons_limit < retained->mpm->num_buckets)
ap_daemons_limit = retained->mpm->num_buckets;
if (ap_daemons_to_start < retained->mpm->num_buckets)
ap_daemons_to_start = retained->mpm->num_buckets;
if (ap_daemons_min_free < retained->mpm->num_buckets)
ap_daemons_min_free = retained->mpm->num_buckets;
if (ap_daemons_max_free < ap_daemons_min_free + retained->mpm->num_buckets)
ap_daemons_max_free = ap_daemons_min_free + retained->mpm->num_buckets;
remaining_children_to_start = ap_daemons_to_start;
if (remaining_children_to_start > ap_daemons_limit) {
remaining_children_to_start = ap_daemons_limit;
}
if (!retained->mpm->was_graceful) {
startup_children(remaining_children_to_start);
remaining_children_to_start = 0;
} else {
retained->hold_off_on_exponential_spawning = 10;
}
ap_log_error(APLOG_MARK, APLOG_NOTICE, 0, ap_server_conf, APLOGNO(00163)
"%s configured -- resuming normal operations",
ap_get_server_description());
ap_log_error(APLOG_MARK, APLOG_INFO, 0, ap_server_conf, APLOGNO(00164)
"Server built: %s", ap_get_server_built());
ap_log_command_line(plog, s);
ap_log_mpm_common(s);
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, ap_server_conf, APLOGNO(00165)
"Accept mutex: %s (default: %s)",
(all_buckets[0].mutex)
? apr_proc_mutex_name(all_buckets[0].mutex)
: "none",
apr_proc_mutex_defname());
retained->mpm->mpm_state = AP_MPMQ_RUNNING;
while (!retained->mpm->restart_pending && !retained->mpm->shutdown_pending) {
int child_slot;
apr_exit_why_e exitwhy;
int status, processed_status;
apr_proc_t pid;
ap_wait_or_timeout(&exitwhy, &status, &pid, pconf, ap_server_conf);
if (pid.pid != -1) {
processed_status = ap_process_child_status(&pid, exitwhy, status);
child_slot = ap_find_child_by_pid(&pid);
if (processed_status == APEXIT_CHILDFATAL) {
if (child_slot < 0
|| ap_get_scoreboard_process(child_slot)->generation
== retained->mpm->my_generation) {
retained->mpm->mpm_state = AP_MPMQ_STOPPING;
return !OK;
} else {
ap_log_error(APLOG_MARK, APLOG_WARNING, 0, ap_server_conf, APLOGNO(00166)
"Ignoring fatal error in child of previous "
"generation (pid %ld).",
(long)pid.pid);
}
}
if (child_slot >= 0) {
(void) ap_update_child_status_from_indexes(child_slot, 0, SERVER_DEAD,
(request_rec *) NULL);
prefork_note_child_killed(child_slot, 0, 0);
if (processed_status == APEXIT_CHILDSICK) {
retained->idle_spawn_rate = 1;
} else if (remaining_children_to_start
&& child_slot < ap_daemons_limit) {
make_child(ap_server_conf, child_slot,
ap_get_scoreboard_process(child_slot)->bucket);
--remaining_children_to_start;
}
#if APR_HAS_OTHER_CHILD
} else if (apr_proc_other_child_alert(&pid, APR_OC_REASON_DEATH, status) == APR_SUCCESS) {
#endif
} else if (retained->mpm->was_graceful) {
ap_log_error(APLOG_MARK, APLOG_WARNING,
0, ap_server_conf, APLOGNO(00167)
"long lost child came home! (pid %ld)", (long)pid.pid);
}
continue;
} else if (remaining_children_to_start) {
startup_children(remaining_children_to_start);
remaining_children_to_start = 0;
continue;
}
perform_idle_server_maintenance(pconf);
}
retained->mpm->mpm_state = AP_MPMQ_STOPPING;
if (retained->mpm->shutdown_pending && retained->mpm->is_ungraceful) {
if (ap_unixd_killpg(getpgrp(), SIGTERM) < 0) {
ap_log_error(APLOG_MARK, APLOG_WARNING, errno, ap_server_conf, APLOGNO(00168) "killpg SIGTERM");
}
ap_reclaim_child_processes(1,
prefork_note_child_killed);
ap_remove_pid(pconf, ap_pid_fname);
ap_log_error(APLOG_MARK, APLOG_NOTICE, 0, ap_server_conf, APLOGNO(00169)
"caught SIGTERM, shutting down");
return DONE;
}
if (retained->mpm->shutdown_pending) {
int active_children;
apr_time_t cutoff = 0;
ap_close_listeners();
for (i = 0; i < retained->mpm->num_buckets; i++) {
ap_mpm_pod_killpg(all_buckets[i].pod, retained->max_daemons_limit);
}
active_children = 0;
for (index = 0; index < ap_daemons_limit; ++index) {
if (ap_scoreboard_image->servers[index][0].status != SERVER_DEAD) {
ap_mpm_safe_kill(MPM_CHILD_PID(index), AP_SIG_GRACEFUL);
active_children++;
}
}
ap_relieve_child_processes(prefork_note_child_killed);
ap_remove_pid(pconf, ap_pid_fname);
ap_log_error(APLOG_MARK, APLOG_NOTICE, 0, ap_server_conf, APLOGNO(00170)
"caught " AP_SIG_GRACEFUL_STOP_STRING ", shutting down gracefully");
if (ap_graceful_shutdown_timeout) {
cutoff = apr_time_now() +
apr_time_from_sec(ap_graceful_shutdown_timeout);
}
retained->mpm->shutdown_pending = 0;
do {
sleep(1);
ap_relieve_child_processes(prefork_note_child_killed);
active_children = 0;
for (index = 0; index < ap_daemons_limit; ++index) {
if (ap_mpm_safe_kill(MPM_CHILD_PID(index), 0) == APR_SUCCESS) {
active_children = 1;
break;
}
}
} while (!retained->mpm->shutdown_pending && active_children &&
(!ap_graceful_shutdown_timeout || apr_time_now() < cutoff));
ap_unixd_killpg(getpgrp(), SIGTERM);
return DONE;
}
if (one_process) {
return DONE;
}
++retained->mpm->my_generation;
ap_scoreboard_image->global->running_generation = retained->mpm->my_generation;
if (!retained->mpm->is_ungraceful) {
ap_log_error(APLOG_MARK, APLOG_NOTICE, 0, ap_server_conf, APLOGNO(00171)
"Graceful restart requested, doing restart");
for (i = 0; i < retained->mpm->num_buckets; i++) {
ap_mpm_pod_killpg(all_buckets[i].pod, retained->max_daemons_limit);
}
for (index = 0; index < ap_daemons_limit; ++index) {
if (ap_scoreboard_image->servers[index][0].status != SERVER_DEAD) {
ap_scoreboard_image->servers[index][0].status = SERVER_GRACEFUL;
ap_mpm_safe_kill(ap_scoreboard_image->parent[index].pid, AP_SIG_GRACEFUL);
}
}
} else {
if (ap_unixd_killpg(getpgrp(), SIGHUP) < 0) {
ap_log_error(APLOG_MARK, APLOG_WARNING, errno, ap_server_conf, APLOGNO(00172) "killpg SIGHUP");
}
ap_reclaim_child_processes(0,
prefork_note_child_killed);
ap_log_error(APLOG_MARK, APLOG_NOTICE, 0, ap_server_conf, APLOGNO(00173)
"SIGHUP received. Attempting to restart");
}
return OK;
}
static int prefork_open_logs(apr_pool_t *p, apr_pool_t *plog, apr_pool_t *ptemp, server_rec *s) {
int startup = 0;
int level_flags = 0;
ap_listen_rec **listen_buckets;
apr_status_t rv;
char id[16];
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
retained->mpm->num_buckets = 1;
} else if (!retained->mpm->was_graceful) {
retained->mpm->num_buckets = 0;
}
if ((rv = ap_duplicate_listeners(pconf, ap_server_conf,
&listen_buckets, &retained->mpm->num_buckets))) {
ap_log_error(APLOG_MARK, APLOG_CRIT | level_flags, rv,
(startup ? NULL : s),
"could not duplicate listeners");
return !OK;
}
all_buckets = apr_pcalloc(pconf, retained->mpm->num_buckets *
sizeof(prefork_child_bucket));
for (i = 0; i < retained->mpm->num_buckets; i++) {
if ((rv = ap_mpm_pod_open(pconf, &all_buckets[i].pod))) {
ap_log_error(APLOG_MARK, APLOG_CRIT | level_flags, rv,
(startup ? NULL : s),
"could not open pipe-of-death");
return !OK;
}
if ((rv = SAFE_ACCEPT((apr_snprintf(id, sizeof id, "%i", i),
ap_proc_mutex_create(&all_buckets[i].mutex,
NULL, AP_ACCEPT_MUTEX_TYPE,
id, s, pconf, 0))))) {
ap_log_error(APLOG_MARK, APLOG_CRIT | level_flags, rv,
(startup ? NULL : s),
"could not create accept mutex");
return !OK;
}
all_buckets[i].listeners = listen_buckets[i];
}
return OK;
}
static int prefork_pre_config(apr_pool_t *p, apr_pool_t *plog, apr_pool_t *ptemp) {
int no_detach, debug, foreground;
apr_status_t rv;
const char *userdata_key = "mpm_prefork_module";
debug = ap_exists_config_define("DEBUG");
if (debug) {
foreground = one_process = 1;
no_detach = 0;
} else {
no_detach = ap_exists_config_define("NO_DETACH");
one_process = ap_exists_config_define("ONE_PROCESS");
foreground = ap_exists_config_define("FOREGROUND");
}
ap_mutex_register(p, AP_ACCEPT_MUTEX_TYPE, NULL, APR_LOCK_DEFAULT, 0);
retained = ap_retained_data_get(userdata_key);
if (!retained) {
retained = ap_retained_data_create(userdata_key, sizeof(*retained));
retained->mpm = ap_unixd_mpm_get_retained_data();
retained->max_daemons_limit = -1;
retained->idle_spawn_rate = 1;
}
retained->mpm->mpm_state = AP_MPMQ_STARTING;
if (retained->mpm->baton != retained) {
retained->mpm->was_graceful = 0;
retained->mpm->baton = retained;
}
++retained->mpm->module_loads;
if (retained->mpm->module_loads == 2) {
if (!one_process && !foreground) {
ap_fatal_signal_setup(ap_server_conf, pconf);
rv = apr_proc_detach(no_detach ? APR_PROC_DETACH_FOREGROUND
: APR_PROC_DETACH_DAEMONIZE);
if (rv != APR_SUCCESS) {
ap_log_error(APLOG_MARK, APLOG_CRIT, rv, NULL, APLOGNO(00174)
"apr_proc_detach failed");
return HTTP_INTERNAL_SERVER_ERROR;
}
}
}
parent_pid = ap_my_pid = getpid();
ap_listen_pre_config();
ap_daemons_to_start = DEFAULT_START_DAEMON;
ap_daemons_min_free = DEFAULT_MIN_FREE_DAEMON;
ap_daemons_max_free = DEFAULT_MAX_FREE_DAEMON;
server_limit = DEFAULT_SERVER_LIMIT;
ap_daemons_limit = server_limit;
ap_extended_status = 0;
return OK;
}
static int prefork_check_config(apr_pool_t *p, apr_pool_t *plog,
apr_pool_t *ptemp, server_rec *s) {
int startup = 0;
if (retained->mpm->module_loads == 1) {
startup = 1;
}
if (server_limit > MAX_SERVER_LIMIT) {
if (startup) {
ap_log_error(APLOG_MARK, APLOG_WARNING | APLOG_STARTUP, 0, NULL, APLOGNO(00175)
"WARNING: ServerLimit of %d exceeds compile-time "
"limit of %d servers, decreasing to %d.",
server_limit, MAX_SERVER_LIMIT, MAX_SERVER_LIMIT);
} else {
ap_log_error(APLOG_MARK, APLOG_WARNING, 0, s, APLOGNO(00176)
"ServerLimit of %d exceeds compile-time limit "
"of %d, decreasing to match",
server_limit, MAX_SERVER_LIMIT);
}
server_limit = MAX_SERVER_LIMIT;
} else if (server_limit < 1) {
if (startup) {
ap_log_error(APLOG_MARK, APLOG_WARNING | APLOG_STARTUP, 0, NULL, APLOGNO(00177)
"WARNING: ServerLimit of %d not allowed, "
"increasing to 1.", server_limit);
} else {
ap_log_error(APLOG_MARK, APLOG_WARNING, 0, s, APLOGNO(00178)
"ServerLimit of %d not allowed, increasing to 1",
server_limit);
}
server_limit = 1;
}
if (!retained->first_server_limit) {
retained->first_server_limit = server_limit;
} else if (server_limit != retained->first_server_limit) {
ap_log_error(APLOG_MARK, APLOG_WARNING, 0, s, APLOGNO(00179)
"changing ServerLimit to %d from original value of %d "
"not allowed during restart",
server_limit, retained->first_server_limit);
server_limit = retained->first_server_limit;
}
if (ap_daemons_limit > server_limit) {
if (startup) {
ap_log_error(APLOG_MARK, APLOG_WARNING | APLOG_STARTUP, 0, NULL, APLOGNO(00180)
"WARNING: MaxRequestWorkers of %d exceeds ServerLimit "
"value of %d servers, decreasing MaxRequestWorkers to %d. "
"To increase, please see the ServerLimit directive.",
ap_daemons_limit, server_limit, server_limit);
} else {
ap_log_error(APLOG_MARK, APLOG_WARNING, 0, s, APLOGNO(00181)
"MaxRequestWorkers of %d exceeds ServerLimit value "
"of %d, decreasing to match",
ap_daemons_limit, server_limit);
}
ap_daemons_limit = server_limit;
} else if (ap_daemons_limit < 1) {
if (startup) {
ap_log_error(APLOG_MARK, APLOG_WARNING | APLOG_STARTUP, 0, NULL, APLOGNO(00182)
"WARNING: MaxRequestWorkers of %d not allowed, "
"increasing to 1.", ap_daemons_limit);
} else {
ap_log_error(APLOG_MARK, APLOG_WARNING, 0, s, APLOGNO(00183)
"MaxRequestWorkers of %d not allowed, increasing to 1",
ap_daemons_limit);
}
ap_daemons_limit = 1;
}
if (ap_daemons_to_start < 1) {
if (startup) {
ap_log_error(APLOG_MARK, APLOG_WARNING | APLOG_STARTUP, 0, NULL, APLOGNO(00184)
"WARNING: StartServers of %d not allowed, "
"increasing to 1.", ap_daemons_to_start);
} else {
ap_log_error(APLOG_MARK, APLOG_WARNING, 0, s, APLOGNO(00185)
"StartServers of %d not allowed, increasing to 1",
ap_daemons_to_start);
}
ap_daemons_to_start = 1;
}
if (ap_daemons_min_free < 1) {
if (startup) {
ap_log_error(APLOG_MARK, APLOG_WARNING | APLOG_STARTUP, 0, NULL, APLOGNO(00186)
"WARNING: MinSpareServers of %d not allowed, "
"increasing to 1 to avoid almost certain server failure. "
"Please read the documentation.", ap_daemons_min_free);
} else {
ap_log_error(APLOG_MARK, APLOG_WARNING, 0, s, APLOGNO(00187)
"MinSpareServers of %d not allowed, increasing to 1",
ap_daemons_min_free);
}
ap_daemons_min_free = 1;
}
return OK;
}
static void prefork_hooks(apr_pool_t *p) {
static const char *const aszSucc[] = {"core.c", NULL};
ap_hook_open_logs(prefork_open_logs, NULL, aszSucc, APR_HOOK_REALLY_FIRST);
ap_hook_pre_config(prefork_pre_config, NULL, NULL, APR_HOOK_REALLY_FIRST);
ap_hook_check_config(prefork_check_config, NULL, NULL, APR_HOOK_MIDDLE);
ap_hook_mpm(prefork_run, NULL, NULL, APR_HOOK_MIDDLE);
ap_hook_mpm_query(prefork_query, NULL, NULL, APR_HOOK_MIDDLE);
ap_hook_mpm_get_name(prefork_get_name, NULL, NULL, APR_HOOK_MIDDLE);
}
static const char *set_daemons_to_start(cmd_parms *cmd, void *dummy, const char *arg) {
const char *err = ap_check_cmd_context(cmd, GLOBAL_ONLY);
if (err != NULL) {
return err;
}
ap_daemons_to_start = atoi(arg);
return NULL;
}
static const char *set_min_free_servers(cmd_parms *cmd, void *dummy, const char *arg) {
const char *err = ap_check_cmd_context(cmd, GLOBAL_ONLY);
if (err != NULL) {
return err;
}
ap_daemons_min_free = atoi(arg);
return NULL;
}
static const char *set_max_free_servers(cmd_parms *cmd, void *dummy, const char *arg) {
const char *err = ap_check_cmd_context(cmd, GLOBAL_ONLY);
if (err != NULL) {
return err;
}
ap_daemons_max_free = atoi(arg);
return NULL;
}
static const char *set_max_clients (cmd_parms *cmd, void *dummy, const char *arg) {
const char *err = ap_check_cmd_context(cmd, GLOBAL_ONLY);
if (err != NULL) {
return err;
}
if (!strcasecmp(cmd->cmd->name, "MaxClients")) {
ap_log_error(APLOG_MARK, APLOG_INFO, 0, NULL, APLOGNO(00188)
"MaxClients is deprecated, use MaxRequestWorkers "
"instead.");
}
ap_daemons_limit = atoi(arg);
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
static const command_rec prefork_cmds[] = {
LISTEN_COMMANDS,
AP_INIT_TAKE1("StartServers", set_daemons_to_start, NULL, RSRC_CONF,
"Number of child processes launched at server startup"),
AP_INIT_TAKE1("MinSpareServers", set_min_free_servers, NULL, RSRC_CONF,
"Minimum number of idle children, to handle request spikes"),
AP_INIT_TAKE1("MaxSpareServers", set_max_free_servers, NULL, RSRC_CONF,
"Maximum number of idle children"),
AP_INIT_TAKE1("MaxClients", set_max_clients, NULL, RSRC_CONF,
"Deprecated name of MaxRequestWorkers"),
AP_INIT_TAKE1("MaxRequestWorkers", set_max_clients, NULL, RSRC_CONF,
"Maximum number of children alive at the same time"),
AP_INIT_TAKE1("ServerLimit", set_server_limit, NULL, RSRC_CONF,
"Maximum value of MaxRequestWorkers for this run of Apache"),
AP_GRACEFUL_SHUTDOWN_TIMEOUT_COMMAND,
{ NULL }
};
AP_DECLARE_MODULE(mpm_prefork) = {
MPM20_MODULE_STUFF,
NULL,
NULL,
NULL,
NULL,
NULL,
prefork_cmds,
prefork_hooks,
};
