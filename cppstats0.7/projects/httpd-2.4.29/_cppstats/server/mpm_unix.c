#if !defined(WIN32)
#include "apr.h"
#include "apr_thread_proc.h"
#include "apr_signal.h"
#include "apr_strings.h"
#define APR_WANT_STRFUNC
#include "apr_want.h"
#include "apr_getopt.h"
#include "apr_optional.h"
#include "apr_allocator.h"
#include "httpd.h"
#include "http_config.h"
#include "http_core.h"
#include "http_log.h"
#include "http_main.h"
#include "mpm_common.h"
#include "ap_mpm.h"
#include "ap_listen.h"
#include "scoreboard.h"
#include "util_mutex.h"
#if defined(HAVE_PWD_H)
#include <pwd.h>
#endif
#if defined(HAVE_GRP_H)
#include <grp.h>
#endif
#if APR_HAVE_UNISTD_H
#include <unistd.h>
#endif
#undef APLOG_MODULE_INDEX
#define APLOG_MODULE_INDEX AP_CORE_MODULE_INDEX
typedef enum {
DO_NOTHING,
SEND_SIGTERM,
SEND_SIGTERM_NOLOG,
SEND_SIGKILL,
GIVEUP
} action_t;
typedef struct extra_process_t {
struct extra_process_t *next;
pid_t pid;
ap_generation_t gen;
} extra_process_t;
static extra_process_t *extras;
AP_DECLARE(void) ap_register_extra_mpm_process(pid_t pid, ap_generation_t gen) {
extra_process_t *p = (extra_process_t *)ap_malloc(sizeof(extra_process_t));
p->next = extras;
p->pid = pid;
p->gen = gen;
extras = p;
}
AP_DECLARE(int) ap_unregister_extra_mpm_process(pid_t pid, ap_generation_t *old_gen) {
extra_process_t *cur = extras;
extra_process_t *prev = NULL;
while (cur && cur->pid != pid) {
prev = cur;
cur = cur->next;
}
if (cur) {
if (prev) {
prev->next = cur->next;
} else {
extras = cur->next;
}
*old_gen = cur->gen;
free(cur);
return 1;
} else {
return 0;
}
}
static int reclaim_one_pid(pid_t pid, action_t action) {
apr_proc_t proc;
apr_status_t waitret;
apr_exit_why_e why;
int status;
if (pid < 1) {
return 1;
}
proc.pid = pid;
waitret = apr_proc_wait(&proc, &status, &why, APR_NOWAIT);
if (waitret != APR_CHILD_NOTDONE) {
if (waitret == APR_CHILD_DONE)
ap_process_child_status(&proc, why, status);
return 1;
}
switch(action) {
case DO_NOTHING:
break;
case SEND_SIGTERM:
ap_log_error(APLOG_MARK, APLOG_WARNING,
0, ap_server_conf, APLOGNO(00045)
"child process %" APR_PID_T_FMT
" still did not exit, "
"sending a SIGTERM",
pid);
case SEND_SIGTERM_NOLOG:
kill(pid, SIGTERM);
break;
case SEND_SIGKILL:
ap_log_error(APLOG_MARK, APLOG_ERR,
0, ap_server_conf, APLOGNO(00046)
"child process %" APR_PID_T_FMT
" still did not exit, "
"sending a SIGKILL",
pid);
kill(pid, SIGKILL);
break;
case GIVEUP:
ap_log_error(APLOG_MARK, APLOG_ERR,
0, ap_server_conf, APLOGNO(00047)
"could not make child process %" APR_PID_T_FMT
" exit, "
"attempting to continue anyway",
pid);
break;
}
return 0;
}
AP_DECLARE(void) ap_reclaim_child_processes(int terminate,
ap_reclaim_callback_fn_t *mpm_callback) {
apr_time_t waittime = 1024 * 16;
int i;
extra_process_t *cur_extra;
int not_dead_yet;
int max_daemons;
apr_time_t starttime = apr_time_now();
struct {
action_t action;
apr_time_t action_time;
} action_table[] = {
{DO_NOTHING, 0},
{SEND_SIGTERM_NOLOG, 0},
{SEND_SIGTERM, apr_time_from_sec(3)},
{SEND_SIGTERM, apr_time_from_sec(5)},
{SEND_SIGTERM, apr_time_from_sec(7)},
{SEND_SIGKILL, apr_time_from_sec(9)},
{GIVEUP, apr_time_from_sec(10)}
};
int cur_action;
int next_action = terminate ? 1 : 2;
ap_mpm_query(AP_MPMQ_MAX_DAEMON_USED, &max_daemons);
do {
if (action_table[next_action].action_time > 0) {
apr_sleep(waittime);
waittime = waittime * 4;
if (waittime > apr_time_from_sec(1)) {
waittime = apr_time_from_sec(1);
}
}
if (action_table[next_action].action_time <= apr_time_now() - starttime) {
cur_action = next_action;
++next_action;
} else {
cur_action = 0;
}
not_dead_yet = 0;
for (i = 0; i < max_daemons; ++i) {
process_score *ps = ap_get_scoreboard_process(i);
pid_t pid = ps->pid;
if (pid == 0) {
continue;
}
if (reclaim_one_pid(pid, action_table[cur_action].action)) {
mpm_callback(i, 0, 0);
} else {
++not_dead_yet;
}
}
cur_extra = extras;
while (cur_extra) {
ap_generation_t old_gen;
extra_process_t *next = cur_extra->next;
if (reclaim_one_pid(cur_extra->pid, action_table[cur_action].action)) {
if (ap_unregister_extra_mpm_process(cur_extra->pid, &old_gen) == 1) {
mpm_callback(-1, cur_extra->pid, old_gen);
} else {
AP_DEBUG_ASSERT(1 == 0);
}
} else {
++not_dead_yet;
}
cur_extra = next;
}
#if APR_HAS_OTHER_CHILD
apr_proc_other_child_refresh_all(APR_OC_REASON_RESTART);
#endif
} while (not_dead_yet > 0 &&
action_table[cur_action].action != GIVEUP);
}
AP_DECLARE(void) ap_relieve_child_processes(ap_reclaim_callback_fn_t *mpm_callback) {
int i;
extra_process_t *cur_extra;
int max_daemons;
ap_mpm_query(AP_MPMQ_MAX_DAEMON_USED, &max_daemons);
for (i = 0; i < max_daemons; ++i) {
process_score *ps = ap_get_scoreboard_process(i);
pid_t pid = ps->pid;
if (pid == 0) {
continue;
}
if (reclaim_one_pid(pid, DO_NOTHING)) {
mpm_callback(i, 0, 0);
}
}
cur_extra = extras;
while (cur_extra) {
ap_generation_t old_gen;
extra_process_t *next = cur_extra->next;
if (reclaim_one_pid(cur_extra->pid, DO_NOTHING)) {
if (ap_unregister_extra_mpm_process(cur_extra->pid, &old_gen) == 1) {
mpm_callback(-1, cur_extra->pid, old_gen);
} else {
AP_DEBUG_ASSERT(1 == 0);
}
}
cur_extra = next;
}
}
AP_DECLARE(apr_status_t) ap_mpm_safe_kill(pid_t pid, int sig) {
#if !defined(HAVE_GETPGID)
apr_proc_t proc;
apr_status_t rv;
apr_exit_why_e why;
int status;
if (pid < 1) {
return APR_EINVAL;
}
proc.pid = pid;
rv = apr_proc_wait(&proc, &status, &why, APR_NOWAIT);
if (rv == APR_CHILD_DONE) {
ap_process_child_status(&proc, why, status);
return APR_EINVAL;
} else if (rv != APR_CHILD_NOTDONE) {
ap_log_error(APLOG_MARK, APLOG_NOTICE, rv, ap_server_conf, APLOGNO(00048)
"cannot send signal %d to pid %ld (non-child or "
"already dead)", sig, (long)pid);
return APR_EINVAL;
}
#else
pid_t pg;
if (pid < 1) {
return APR_EINVAL;
}
pg = getpgid(pid);
if (pg == -1) {
return errno;
}
if (pg != getpgrp()) {
ap_log_error(APLOG_MARK, APLOG_ALERT, 0, ap_server_conf, APLOGNO(00049)
"refusing to send signal %d to pid %ld outside "
"process group", sig, (long)pid);
return APR_EINVAL;
}
#endif
return kill(pid, sig) ? errno : APR_SUCCESS;
}
AP_DECLARE(int) ap_process_child_status(apr_proc_t *pid, apr_exit_why_e why,
int status) {
int signum = status;
const char *sigdesc;
if (APR_PROC_CHECK_EXIT(why)) {
if (status == APEXIT_CHILDSICK) {
return status;
}
if (status == APEXIT_CHILDFATAL) {
ap_log_error(APLOG_MARK, APLOG_ALERT,
0, ap_server_conf, APLOGNO(00050)
"Child %" APR_PID_T_FMT
" returned a Fatal error... Apache is exiting!",
pid->pid);
return APEXIT_CHILDFATAL;
}
return 0;
}
if (APR_PROC_CHECK_SIGNALED(why)) {
sigdesc = apr_signal_description_get(signum);
switch (signum) {
case SIGTERM:
case SIGHUP:
case AP_SIG_GRACEFUL:
case SIGKILL:
break;
default:
if (APR_PROC_CHECK_CORE_DUMP(why)) {
ap_log_error(APLOG_MARK, APLOG_NOTICE,
0, ap_server_conf, APLOGNO(00051)
"child pid %ld exit signal %s (%d), "
"possible coredump in %s",
(long)pid->pid, sigdesc, signum,
ap_coredump_dir);
} else {
ap_log_error(APLOG_MARK, APLOG_NOTICE,
0, ap_server_conf, APLOGNO(00052)
"child pid %ld exit signal %s (%d)",
(long)pid->pid, sigdesc, signum);
}
}
}
return 0;
}
AP_DECLARE(apr_status_t) ap_mpm_pod_open(apr_pool_t *p, ap_pod_t **pod) {
apr_status_t rv;
*pod = apr_palloc(p, sizeof(**pod));
rv = apr_file_pipe_create_ex(&((*pod)->pod_in), &((*pod)->pod_out),
APR_WRITE_BLOCK, p);
if (rv != APR_SUCCESS) {
return rv;
}
apr_file_pipe_timeout_set((*pod)->pod_in, 0);
(*pod)->p = p;
apr_file_inherit_unset((*pod)->pod_in);
apr_file_inherit_unset((*pod)->pod_out);
return APR_SUCCESS;
}
AP_DECLARE(apr_status_t) ap_mpm_pod_check(ap_pod_t *pod) {
char c;
apr_size_t len = 1;
apr_status_t rv;
rv = apr_file_read(pod->pod_in, &c, &len);
if ((rv == APR_SUCCESS) && (len == 1)) {
return APR_SUCCESS;
}
if (rv != APR_SUCCESS) {
return rv;
}
return AP_NORESTART;
}
AP_DECLARE(apr_status_t) ap_mpm_pod_close(ap_pod_t *pod) {
apr_status_t rv;
rv = apr_file_close(pod->pod_out);
if (rv != APR_SUCCESS) {
return rv;
}
rv = apr_file_close(pod->pod_in);
if (rv != APR_SUCCESS) {
return rv;
}
return APR_SUCCESS;
}
static apr_status_t pod_signal_internal(ap_pod_t *pod) {
apr_status_t rv;
char char_of_death = '!';
apr_size_t one = 1;
rv = apr_file_write(pod->pod_out, &char_of_death, &one);
if (rv != APR_SUCCESS) {
ap_log_error(APLOG_MARK, APLOG_WARNING, rv, ap_server_conf, APLOGNO(00053)
"write pipe_of_death");
}
return rv;
}
AP_DECLARE(apr_status_t) ap_mpm_podx_open(apr_pool_t *p, ap_pod_t **pod) {
apr_status_t rv;
*pod = apr_palloc(p, sizeof(**pod));
rv = apr_file_pipe_create(&((*pod)->pod_in), &((*pod)->pod_out), p);
if (rv != APR_SUCCESS) {
return rv;
}
(*pod)->p = p;
apr_file_inherit_unset((*pod)->pod_in);
apr_file_inherit_unset((*pod)->pod_out);
return APR_SUCCESS;
}
AP_DECLARE(int) ap_mpm_podx_check(ap_pod_t *pod) {
char c;
apr_os_file_t fd;
int rc;
apr_os_file_get(&fd, pod->pod_in);
rc = read(fd, &c, 1);
if (rc == 1) {
switch (c) {
case AP_MPM_PODX_RESTART_CHAR:
return AP_MPM_PODX_RESTART;
case AP_MPM_PODX_GRACEFUL_CHAR:
return AP_MPM_PODX_GRACEFUL;
}
}
return AP_MPM_PODX_NORESTART;
}
AP_DECLARE(apr_status_t) ap_mpm_podx_close(ap_pod_t *pod) {
apr_status_t rv;
rv = apr_file_close(pod->pod_out);
if (rv != APR_SUCCESS) {
return rv;
}
rv = apr_file_close(pod->pod_in);
if (rv != APR_SUCCESS) {
return rv;
}
return rv;
}
static apr_status_t podx_signal_internal(ap_pod_t *pod,
ap_podx_restart_t graceful) {
apr_status_t rv;
apr_size_t one = 1;
char char_of_death = ' ';
switch (graceful) {
case AP_MPM_PODX_RESTART:
char_of_death = AP_MPM_PODX_RESTART_CHAR;
break;
case AP_MPM_PODX_GRACEFUL:
char_of_death = AP_MPM_PODX_GRACEFUL_CHAR;
break;
case AP_MPM_PODX_NORESTART:
break;
}
rv = apr_file_write(pod->pod_out, &char_of_death, &one);
if (rv != APR_SUCCESS) {
ap_log_error(APLOG_MARK, APLOG_WARNING, rv, ap_server_conf, APLOGNO(02404)
"write pipe_of_death");
}
return rv;
}
AP_DECLARE(apr_status_t) ap_mpm_podx_signal(ap_pod_t * pod,
ap_podx_restart_t graceful) {
return podx_signal_internal(pod, graceful);
}
AP_DECLARE(void) ap_mpm_podx_killpg(ap_pod_t * pod, int num,
ap_podx_restart_t graceful) {
int i;
apr_status_t rv = APR_SUCCESS;
for (i = 0; i < num && rv == APR_SUCCESS; i++) {
rv = podx_signal_internal(pod, graceful);
}
}
static apr_status_t dummy_connection(ap_pod_t *pod) {
const char *data;
apr_status_t rv;
apr_socket_t *sock;
apr_pool_t *p;
apr_size_t len;
ap_listen_rec *lp;
rv = apr_pool_create(&p, pod->p);
if (rv != APR_SUCCESS) {
return rv;
}
lp = ap_listeners;
while (lp && lp->protocol && strcasecmp(lp->protocol, "http") != 0) {
lp = lp->next;
}
if (!lp) {
lp = ap_listeners;
}
rv = apr_socket_create(&sock, lp->bind_addr->family, SOCK_STREAM, 0, p);
if (rv != APR_SUCCESS) {
ap_log_error(APLOG_MARK, APLOG_WARNING, rv, ap_server_conf, APLOGNO(00054)
"get socket to connect to listener");
apr_pool_destroy(p);
return rv;
}
rv = apr_socket_timeout_set(sock, apr_time_from_sec(3));
if (rv != APR_SUCCESS) {
ap_log_error(APLOG_MARK, APLOG_WARNING, rv, ap_server_conf, APLOGNO(00055)
"set timeout on socket to connect to listener");
apr_socket_close(sock);
apr_pool_destroy(p);
return rv;
}
rv = apr_socket_connect(sock, lp->bind_addr);
if (rv != APR_SUCCESS) {
int log_level = APLOG_WARNING;
if (APR_STATUS_IS_TIMEUP(rv)) {
log_level = APLOG_DEBUG;
}
ap_log_error(APLOG_MARK, log_level, rv, ap_server_conf, APLOGNO(00056)
"connect to listener on %pI", lp->bind_addr);
apr_pool_destroy(p);
return rv;
}
if (lp->protocol && strcasecmp(lp->protocol, "https") == 0) {
static const unsigned char tls10_close_notify[7] = {
'\x15',
'\x03', '\x01',
'\x00', '\x02',
'\x01',
'\x00'
};
data = (const char *)tls10_close_notify;
len = sizeof(tls10_close_notify);
} else {
data = apr_pstrcat(p, "OPTIONS * HTTP/1.0\r\nUser-Agent: ",
ap_get_server_description(),
" (internal dummy connection)\r\n\r\n", NULL);
len = strlen(data);
}
apr_socket_send(sock, data, &len);
apr_socket_close(sock);
apr_pool_destroy(p);
return rv;
}
AP_DECLARE(apr_status_t) ap_mpm_pod_signal(ap_pod_t *pod) {
apr_status_t rv;
rv = pod_signal_internal(pod);
if (rv != APR_SUCCESS) {
return rv;
}
return dummy_connection(pod);
}
void ap_mpm_pod_killpg(ap_pod_t *pod, int num) {
int i;
apr_status_t rv = APR_SUCCESS;
for (i = 0; i < num && rv == APR_SUCCESS; i++) {
if (ap_scoreboard_image->servers[i][0].status != SERVER_READY ||
ap_scoreboard_image->servers[i][0].pid == 0) {
continue;
}
rv = dummy_connection(pod);
}
}
static const char *dash_k_arg = NULL;
static const char *dash_k_arg_noarg = "noarg";
static int send_signal(pid_t pid, int sig) {
if (kill(pid, sig) < 0) {
ap_log_error(APLOG_MARK, APLOG_STARTUP, errno, NULL, APLOGNO(00057)
"sending signal to server");
return 1;
}
return 0;
}
int ap_signal_server(int *exit_status, apr_pool_t *pconf) {
apr_status_t rv;
pid_t otherpid;
int running = 0;
const char *status;
*exit_status = 0;
rv = ap_read_pid(pconf, ap_pid_fname, &otherpid);
if (rv != APR_SUCCESS) {
if (!APR_STATUS_IS_ENOENT(rv)) {
ap_log_error(APLOG_MARK, APLOG_STARTUP, rv, NULL, APLOGNO(00058)
"Error retrieving pid file %s", ap_pid_fname);
ap_log_error(APLOG_MARK, APLOG_STARTUP, 0, NULL, APLOGNO(00059)
"Remove it before continuing if it is corrupted.");
*exit_status = 1;
return 1;
}
status = "httpd (no pid file) not running";
} else {
if (otherpid != getpid() && kill(otherpid, 0) == 0) {
running = 1;
status = apr_psprintf(pconf,
"httpd (pid %" APR_PID_T_FMT ") already "
"running", otherpid);
} else {
status = apr_psprintf(pconf,
"httpd (pid %" APR_PID_T_FMT "?) not running",
otherpid);
}
}
if (!strcmp(dash_k_arg, "start") || dash_k_arg == dash_k_arg_noarg) {
if (running) {
printf("%s\n", status);
return 1;
}
}
if (!strcmp(dash_k_arg, "stop")) {
if (!running) {
printf("%s\n", status);
} else {
send_signal(otherpid, SIGTERM);
}
return 1;
}
if (!strcmp(dash_k_arg, "restart")) {
if (!running) {
printf("httpd not running, trying to start\n");
} else {
*exit_status = send_signal(otherpid, SIGHUP);
return 1;
}
}
if (!strcmp(dash_k_arg, "graceful")) {
if (!running) {
printf("httpd not running, trying to start\n");
} else {
*exit_status = send_signal(otherpid, AP_SIG_GRACEFUL);
return 1;
}
}
if (!strcmp(dash_k_arg, "graceful-stop")) {
if (!running) {
printf("%s\n", status);
} else {
*exit_status = send_signal(otherpid, AP_SIG_GRACEFUL_STOP);
}
return 1;
}
return 0;
}
void ap_mpm_rewrite_args(process_rec *process) {
apr_array_header_t *mpm_new_argv;
apr_status_t rv;
apr_getopt_t *opt;
char optbuf[3];
const char *optarg;
mpm_new_argv = apr_array_make(process->pool, process->argc,
sizeof(const char **));
*(const char **)apr_array_push(mpm_new_argv) = process->argv[0];
apr_getopt_init(&opt, process->pool, process->argc, process->argv);
opt->errfn = NULL;
optbuf[0] = '-';
optbuf[2] = '\0';
while ((rv = apr_getopt(opt, "k:" AP_SERVER_BASEARGS,
optbuf + 1, &optarg)) == APR_SUCCESS) {
switch(optbuf[1]) {
case 'k':
if (!dash_k_arg) {
if (!strcmp(optarg, "start") || !strcmp(optarg, "stop") ||
!strcmp(optarg, "restart") || !strcmp(optarg, "graceful") ||
!strcmp(optarg, "graceful-stop")) {
dash_k_arg = optarg;
break;
}
}
default:
*(const char **)apr_array_push(mpm_new_argv) =
apr_pstrdup(process->pool, optbuf);
if (optarg) {
*(const char **)apr_array_push(mpm_new_argv) = optarg;
}
}
}
if (rv == APR_BADCH || rv == APR_BADARG) {
opt->ind--;
}
while (opt->ind < opt->argc) {
*(const char **)apr_array_push(mpm_new_argv) =
apr_pstrdup(process->pool, opt->argv[opt->ind++]);
}
process->argc = mpm_new_argv->nelts;
process->argv = (const char * const *)mpm_new_argv->elts;
if (NULL == dash_k_arg) {
dash_k_arg = dash_k_arg_noarg;
}
APR_REGISTER_OPTIONAL_FN(ap_signal_server);
}
static pid_t parent_pid, my_pid;
static apr_pool_t *pconf;
#if AP_ENABLE_EXCEPTION_HOOK
static int exception_hook_enabled;
const char *ap_mpm_set_exception_hook(cmd_parms *cmd, void *dummy,
const char *arg) {
const char *err = ap_check_cmd_context(cmd, GLOBAL_ONLY);
if (err != NULL) {
return err;
}
if (cmd->server->is_virtual) {
return "EnableExceptionHook directive not allowed in <VirtualHost>";
}
if (strcasecmp(arg, "on") == 0) {
exception_hook_enabled = 1;
} else if (strcasecmp(arg, "off") == 0) {
exception_hook_enabled = 0;
} else {
return "parameter must be 'on' or 'off'";
}
return NULL;
}
static void run_fatal_exception_hook(int sig) {
ap_exception_info_t ei = {0};
if (exception_hook_enabled &&
geteuid() != 0 &&
my_pid != parent_pid) {
ei.sig = sig;
ei.pid = my_pid;
ap_run_fatal_exception(&ei);
}
}
#endif
static void sig_coredump(int sig) {
apr_filepath_set(ap_coredump_dir, pconf);
apr_signal(sig, SIG_DFL);
#if AP_ENABLE_EXCEPTION_HOOK
run_fatal_exception_hook(sig);
#endif
if (getpid() == parent_pid) {
ap_log_error(APLOG_MARK, APLOG_NOTICE,
0, ap_server_conf, APLOGNO(00060)
"seg fault or similar nasty error detected "
"in the parent process");
}
kill(getpid(), sig);
}
AP_DECLARE(apr_status_t) ap_fatal_signal_child_setup(server_rec *s) {
my_pid = getpid();
return APR_SUCCESS;
}
AP_DECLARE(apr_status_t) ap_fatal_signal_setup(server_rec *s,
apr_pool_t *in_pconf) {
#if !defined(NO_USE_SIGACTION)
struct sigaction sa;
memset(&sa, 0, sizeof sa);
sigemptyset(&sa.sa_mask);
#if defined(SA_ONESHOT)
sa.sa_flags = SA_ONESHOT;
#elif defined(SA_RESETHAND)
sa.sa_flags = SA_RESETHAND;
#endif
sa.sa_handler = sig_coredump;
if (sigaction(SIGSEGV, &sa, NULL) < 0)
ap_log_error(APLOG_MARK, APLOG_WARNING, errno, s, APLOGNO(00061) "sigaction(SIGSEGV)");
#if defined(SIGBUS)
if (sigaction(SIGBUS, &sa, NULL) < 0)
ap_log_error(APLOG_MARK, APLOG_WARNING, errno, s, APLOGNO(00062) "sigaction(SIGBUS)");
#endif
#if defined(SIGABORT)
if (sigaction(SIGABORT, &sa, NULL) < 0)
ap_log_error(APLOG_MARK, APLOG_WARNING, errno, s, APLOGNO(00063) "sigaction(SIGABORT)");
#endif
#if defined(SIGABRT)
if (sigaction(SIGABRT, &sa, NULL) < 0)
ap_log_error(APLOG_MARK, APLOG_WARNING, errno, s, APLOGNO(00064) "sigaction(SIGABRT)");
#endif
#if defined(SIGILL)
if (sigaction(SIGILL, &sa, NULL) < 0)
ap_log_error(APLOG_MARK, APLOG_WARNING, errno, s, APLOGNO(00065) "sigaction(SIGILL)");
#endif
#if defined(SIGFPE)
if (sigaction(SIGFPE, &sa, NULL) < 0)
ap_log_error(APLOG_MARK, APLOG_WARNING, errno, s, APLOGNO(00066) "sigaction(SIGFPE)");
#endif
#else
apr_signal(SIGSEGV, sig_coredump);
#if defined(SIGBUS)
apr_signal(SIGBUS, sig_coredump);
#endif
#if defined(SIGABORT)
apr_signal(SIGABORT, sig_coredump);
#endif
#if defined(SIGABRT)
apr_signal(SIGABRT, sig_coredump);
#endif
#if defined(SIGILL)
apr_signal(SIGILL, sig_coredump);
#endif
#if defined(SIGFPE)
apr_signal(SIGFPE, sig_coredump);
#endif
#endif
pconf = in_pconf;
parent_pid = my_pid = getpid();
return APR_SUCCESS;
}
#endif
