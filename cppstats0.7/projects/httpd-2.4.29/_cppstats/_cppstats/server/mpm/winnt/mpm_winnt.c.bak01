#if defined(WIN32)
#include "httpd.h"
#include "http_main.h"
#include "http_log.h"
#include "http_config.h"
#include "http_core.h"
#include "http_connection.h"
#include "apr_portable.h"
#include "apr_thread_proc.h"
#include "apr_getopt.h"
#include "apr_strings.h"
#include "apr_lib.h"
#include "apr_shm.h"
#include "apr_thread_mutex.h"
#include "ap_mpm.h"
#include "apr_general.h"
#include "ap_config.h"
#include "ap_listen.h"
#include "mpm_default.h"
#include "mpm_winnt.h"
#include "mpm_common.h"
#include <malloc.h>
#include "apr_atomic.h"
#include "scoreboard.h"
#if defined(__WATCOMC__)
#define _environ environ
#endif
#if !defined(STACK_SIZE_PARAM_IS_A_RESERVATION)
#define STACK_SIZE_PARAM_IS_A_RESERVATION 0x00010000
#endif
#if defined(AP_ENABLE_V4_MAPPED)
#error The WinNT MPM does not currently support AP_ENABLE_V4_MAPPED
#endif
extern apr_shm_t *ap_scoreboard_shm;
static volatile ap_generation_t my_generation=0;
static HANDLE shutdown_event;
static HANDLE restart_event;
static int one_process = 0;
static char const* signal_arg = NULL;
OSVERSIONINFO osver;
DWORD stack_res_flag;
static DWORD parent_pid;
DWORD my_pid;
apr_proc_mutex_t *start_mutex;
HANDLE exit_event;
int ap_threads_per_child = 0;
static int thread_limit = 0;
static int first_thread_limit = 0;
int winnt_mpm_state = AP_MPMQ_STARTING;
apr_pool_t *pconf;
static HANDLE pipe;
static const char *set_threads_per_child (cmd_parms *cmd, void *dummy, const char *arg) {
const char *err = ap_check_cmd_context(cmd, GLOBAL_ONLY);
if (err != NULL) {
return err;
}
ap_threads_per_child = atoi(arg);
return NULL;
}
static const char *set_thread_limit (cmd_parms *cmd, void *dummy, const char *arg) {
const char *err = ap_check_cmd_context(cmd, GLOBAL_ONLY);
if (err != NULL) {
return err;
}
thread_limit = atoi(arg);
return NULL;
}
static const command_rec winnt_cmds[] = {
LISTEN_COMMANDS,
AP_INIT_TAKE1("ThreadsPerChild", set_threads_per_child, NULL, RSRC_CONF,
"Number of threads each child creates" ),
AP_INIT_TAKE1("ThreadLimit", set_thread_limit, NULL, RSRC_CONF,
"Maximum worker threads in a server for this run of Apache"),
{ NULL }
};
static void winnt_note_child_started(int slot, pid_t pid) {
ap_scoreboard_image->parent[slot].pid = pid;
ap_run_child_status(ap_server_conf,
ap_scoreboard_image->parent[slot].pid,
my_generation, slot, MPM_CHILD_STARTED);
}
static void winnt_note_child_killed(int slot) {
ap_run_child_status(ap_server_conf,
ap_scoreboard_image->parent[slot].pid,
ap_scoreboard_image->parent[slot].generation,
slot, MPM_CHILD_EXITED);
ap_scoreboard_image->parent[slot].pid = 0;
}
#define MAX_SIGNAL_NAME 30
static char signal_name_prefix[MAX_SIGNAL_NAME];
static char signal_restart_name[MAX_SIGNAL_NAME];
static char signal_shutdown_name[MAX_SIGNAL_NAME];
static void setup_signal_names(char *prefix) {
apr_snprintf(signal_name_prefix, sizeof(signal_name_prefix), prefix);
apr_snprintf(signal_shutdown_name, sizeof(signal_shutdown_name),
"%s_shutdown", signal_name_prefix);
apr_snprintf(signal_restart_name, sizeof(signal_restart_name),
"%s_restart", signal_name_prefix);
}
AP_DECLARE(void) ap_signal_parent(ap_signal_parent_e type) {
HANDLE e;
char *signal_name;
if (parent_pid == my_pid) {
switch(type) {
case SIGNAL_PARENT_SHUTDOWN: {
SetEvent(shutdown_event);
break;
}
case SIGNAL_PARENT_RESTART:
case SIGNAL_PARENT_RESTART_GRACEFUL: {
SetEvent(restart_event);
break;
}
}
return;
}
switch(type) {
case SIGNAL_PARENT_SHUTDOWN: {
signal_name = signal_shutdown_name;
break;
}
case SIGNAL_PARENT_RESTART:
case SIGNAL_PARENT_RESTART_GRACEFUL: {
signal_name = signal_restart_name;
break;
}
default:
return;
}
e = OpenEvent(EVENT_MODIFY_STATE, FALSE, signal_name);
if (!e) {
ap_log_error(APLOG_MARK, APLOG_EMERG, apr_get_os_error(), ap_server_conf, APLOGNO(00382)
"OpenEvent on %s event", signal_name);
return;
}
if (SetEvent(e) == 0) {
ap_log_error(APLOG_MARK, APLOG_EMERG, apr_get_os_error(), ap_server_conf, APLOGNO(00383)
"SetEvent on %s event", signal_name);
CloseHandle(e);
return;
}
CloseHandle(e);
}
static void get_handles_from_parent(server_rec *s, HANDLE *child_exit_event,
apr_proc_mutex_t **child_start_mutex,
apr_shm_t **scoreboard_shm) {
HANDLE hScore;
HANDLE ready_event;
HANDLE os_start;
DWORD BytesRead;
void *sb_shared;
apr_status_t rv;
if (!ReadFile(pipe, &ready_event, sizeof(HANDLE),
&BytesRead, (LPOVERLAPPED) NULL)
|| (BytesRead != sizeof(HANDLE))) {
ap_log_error(APLOG_MARK, APLOG_CRIT, apr_get_os_error(), ap_server_conf, APLOGNO(00384)
"Child: Unable to retrieve the ready event from the parent");
exit(APEXIT_CHILDINIT);
}
SetEvent(ready_event);
CloseHandle(ready_event);
if (!ReadFile(pipe, child_exit_event, sizeof(HANDLE),
&BytesRead, (LPOVERLAPPED) NULL)
|| (BytesRead != sizeof(HANDLE))) {
ap_log_error(APLOG_MARK, APLOG_CRIT, apr_get_os_error(), ap_server_conf, APLOGNO(00385)
"Child: Unable to retrieve the exit event from the parent");
exit(APEXIT_CHILDINIT);
}
if (!ReadFile(pipe, &os_start, sizeof(os_start),
&BytesRead, (LPOVERLAPPED) NULL)
|| (BytesRead != sizeof(os_start))) {
ap_log_error(APLOG_MARK, APLOG_CRIT, apr_get_os_error(), ap_server_conf, APLOGNO(00386)
"Child: Unable to retrieve the start_mutex from the parent");
exit(APEXIT_CHILDINIT);
}
*child_start_mutex = NULL;
if ((rv = apr_os_proc_mutex_put(child_start_mutex, &os_start, s->process->pool))
!= APR_SUCCESS) {
ap_log_error(APLOG_MARK, APLOG_CRIT, rv, ap_server_conf, APLOGNO(00387)
"Child: Unable to access the start_mutex from the parent");
exit(APEXIT_CHILDINIT);
}
if (!ReadFile(pipe, &hScore, sizeof(hScore),
&BytesRead, (LPOVERLAPPED) NULL)
|| (BytesRead != sizeof(hScore))) {
ap_log_error(APLOG_MARK, APLOG_CRIT, apr_get_os_error(), ap_server_conf, APLOGNO(00388)
"Child: Unable to retrieve the scoreboard from the parent");
exit(APEXIT_CHILDINIT);
}
*scoreboard_shm = NULL;
if ((rv = apr_os_shm_put(scoreboard_shm, &hScore, s->process->pool))
!= APR_SUCCESS) {
ap_log_error(APLOG_MARK, APLOG_CRIT, rv, ap_server_conf, APLOGNO(00389)
"Child: Unable to access the scoreboard from the parent");
exit(APEXIT_CHILDINIT);
}
rv = ap_reopen_scoreboard(s->process->pool, scoreboard_shm, 1);
if (rv || !(sb_shared = apr_shm_baseaddr_get(*scoreboard_shm))) {
ap_log_error(APLOG_MARK, APLOG_CRIT, rv, ap_server_conf, APLOGNO(00390)
"Child: Unable to reopen the scoreboard from the parent");
exit(APEXIT_CHILDINIT);
}
ap_init_scoreboard(sb_shared);
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, ap_server_conf, APLOGNO(00391)
"Child: Retrieved our scoreboard from the parent.");
}
static int send_handles_to_child(apr_pool_t *p,
HANDLE child_ready_event,
HANDLE child_exit_event,
apr_proc_mutex_t *child_start_mutex,
apr_shm_t *scoreboard_shm,
HANDLE hProcess,
apr_file_t *child_in) {
apr_status_t rv;
HANDLE hCurrentProcess = GetCurrentProcess();
HANDLE hDup;
HANDLE os_start;
HANDLE hScore;
apr_size_t BytesWritten;
if ((rv = apr_file_write_full(child_in, &my_generation,
sizeof(my_generation), NULL))
!= APR_SUCCESS) {
ap_log_error(APLOG_MARK, APLOG_CRIT, rv, ap_server_conf, APLOGNO(02964)
"Parent: Unable to send its generation to the child");
return -1;
}
if (!DuplicateHandle(hCurrentProcess, child_ready_event, hProcess, &hDup,
EVENT_MODIFY_STATE | SYNCHRONIZE, FALSE, 0)) {
ap_log_error(APLOG_MARK, APLOG_CRIT, apr_get_os_error(), ap_server_conf, APLOGNO(00392)
"Parent: Unable to duplicate the ready event handle for the child");
return -1;
}
if ((rv = apr_file_write_full(child_in, &hDup, sizeof(hDup), &BytesWritten))
!= APR_SUCCESS) {
ap_log_error(APLOG_MARK, APLOG_CRIT, rv, ap_server_conf, APLOGNO(00393)
"Parent: Unable to send the exit event handle to the child");
return -1;
}
if (!DuplicateHandle(hCurrentProcess, child_exit_event, hProcess, &hDup,
EVENT_MODIFY_STATE | SYNCHRONIZE, FALSE, 0)) {
ap_log_error(APLOG_MARK, APLOG_CRIT, apr_get_os_error(), ap_server_conf, APLOGNO(00394)
"Parent: Unable to duplicate the exit event handle for the child");
return -1;
}
if ((rv = apr_file_write_full(child_in, &hDup, sizeof(hDup), &BytesWritten))
!= APR_SUCCESS) {
ap_log_error(APLOG_MARK, APLOG_CRIT, rv, ap_server_conf, APLOGNO(00395)
"Parent: Unable to send the exit event handle to the child");
return -1;
}
if ((rv = apr_os_proc_mutex_get(&os_start, child_start_mutex)) != APR_SUCCESS) {
ap_log_error(APLOG_MARK, APLOG_CRIT, rv, ap_server_conf, APLOGNO(00396)
"Parent: Unable to retrieve the start mutex for the child");
return -1;
}
if (!DuplicateHandle(hCurrentProcess, os_start, hProcess, &hDup,
SYNCHRONIZE, FALSE, 0)) {
ap_log_error(APLOG_MARK, APLOG_CRIT, apr_get_os_error(), ap_server_conf, APLOGNO(00397)
"Parent: Unable to duplicate the start mutex to the child");
return -1;
}
if ((rv = apr_file_write_full(child_in, &hDup, sizeof(hDup), &BytesWritten))
!= APR_SUCCESS) {
ap_log_error(APLOG_MARK, APLOG_CRIT, rv, ap_server_conf, APLOGNO(00398)
"Parent: Unable to send the start mutex to the child");
return -1;
}
if ((rv = apr_os_shm_get(&hScore, scoreboard_shm)) != APR_SUCCESS) {
ap_log_error(APLOG_MARK, APLOG_CRIT, rv, ap_server_conf, APLOGNO(00399)
"Parent: Unable to retrieve the scoreboard handle for the child");
return -1;
}
if (!DuplicateHandle(hCurrentProcess, hScore, hProcess, &hDup,
FILE_MAP_READ | FILE_MAP_WRITE, FALSE, 0)) {
ap_log_error(APLOG_MARK, APLOG_CRIT, apr_get_os_error(), ap_server_conf, APLOGNO(00400)
"Parent: Unable to duplicate the scoreboard handle to the child");
return -1;
}
if ((rv = apr_file_write_full(child_in, &hDup, sizeof(hDup), &BytesWritten))
!= APR_SUCCESS) {
ap_log_error(APLOG_MARK, APLOG_CRIT, rv, ap_server_conf, APLOGNO(00401)
"Parent: Unable to send the scoreboard handle to the child");
return -1;
}
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, ap_server_conf, APLOGNO(00402)
"Parent: Sent the scoreboard to the child");
return 0;
}
static void get_listeners_from_parent(server_rec *s) {
WSAPROTOCOL_INFO WSAProtocolInfo;
ap_listen_rec *lr;
DWORD BytesRead;
int lcnt = 0;
SOCKET nsd;
if (ap_listeners == NULL) {
ap_listen_rec *lr;
lr = apr_palloc(s->process->pool, sizeof(ap_listen_rec));
lr->sd = NULL;
lr->next = ap_listeners;
ap_listeners = lr;
}
for (lr = ap_listeners; lr; lr = lr->next, ++lcnt) {
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, ap_server_conf, APLOGNO(00403)
"Child: Waiting for data for listening socket %pI",
lr->bind_addr);
if (!ReadFile(pipe, &WSAProtocolInfo, sizeof(WSAPROTOCOL_INFO),
&BytesRead, (LPOVERLAPPED) NULL)) {
ap_log_error(APLOG_MARK, APLOG_CRIT, apr_get_os_error(), ap_server_conf, APLOGNO(00404)
"Child: Unable to read socket data from parent");
exit(APEXIT_CHILDINIT);
}
nsd = WSASocket(FROM_PROTOCOL_INFO, FROM_PROTOCOL_INFO, FROM_PROTOCOL_INFO,
&WSAProtocolInfo, 0, 0);
if (nsd == INVALID_SOCKET) {
ap_log_error(APLOG_MARK, APLOG_CRIT, apr_get_netos_error(), ap_server_conf, APLOGNO(00405)
"Child: WSASocket failed to open the inherited socket");
exit(APEXIT_CHILDINIT);
}
if (!SetHandleInformation((HANDLE)nsd, HANDLE_FLAG_INHERIT, 0)) {
ap_log_error(APLOG_MARK, APLOG_ERR, apr_get_os_error(), ap_server_conf, APLOGNO(00406)
"Child: SetHandleInformation failed");
}
apr_os_sock_put(&lr->sd, &nsd, s->process->pool);
}
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, ap_server_conf, APLOGNO(00407)
"Child: retrieved %d listeners from parent", lcnt);
}
static int send_listeners_to_child(apr_pool_t *p, DWORD dwProcessId,
apr_file_t *child_in) {
apr_status_t rv;
int lcnt = 0;
ap_listen_rec *lr;
LPWSAPROTOCOL_INFO lpWSAProtocolInfo;
apr_size_t BytesWritten;
for (lr = ap_listeners; lr; lr = lr->next, ++lcnt) {
apr_os_sock_t nsd;
lpWSAProtocolInfo = apr_pcalloc(p, sizeof(WSAPROTOCOL_INFO));
apr_os_sock_get(&nsd, lr->sd);
ap_log_error(APLOG_MARK, APLOG_DEBUG, APR_SUCCESS, ap_server_conf, APLOGNO(00408)
"Parent: Duplicating socket %d (%pI) and sending it to child process %lu",
nsd, lr->bind_addr, dwProcessId);
if (WSADuplicateSocket(nsd, dwProcessId,
lpWSAProtocolInfo) == SOCKET_ERROR) {
ap_log_error(APLOG_MARK, APLOG_CRIT, apr_get_netos_error(), ap_server_conf, APLOGNO(00409)
"Parent: WSADuplicateSocket failed for socket %d. Check the FAQ.", nsd);
return -1;
}
if ((rv = apr_file_write_full(child_in, lpWSAProtocolInfo,
sizeof(WSAPROTOCOL_INFO), &BytesWritten))
!= APR_SUCCESS) {
ap_log_error(APLOG_MARK, APLOG_CRIT, rv, ap_server_conf, APLOGNO(00410)
"Parent: Unable to write duplicated socket %d to the child.", nsd);
return -1;
}
}
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, ap_server_conf, APLOGNO(00411)
"Parent: Sent %d listeners to child %lu", lcnt, dwProcessId);
return 0;
}
enum waitlist_e {
waitlist_ready = 0,
waitlist_term = 1
};
static int create_process(apr_pool_t *p, HANDLE *child_proc, HANDLE *child_exit_event,
DWORD *child_pid) {
static char **args = NULL;
static char pidbuf[28];
apr_status_t rv;
apr_pool_t *ptemp;
apr_procattr_t *attr;
apr_proc_t new_child;
HANDLE hExitEvent;
HANDLE waitlist[2];
char *cmd;
char *cwd;
char **env;
int envc;
apr_pool_create_ex(&ptemp, p, NULL, NULL);
apr_procattr_create(&attr, ptemp);
apr_procattr_cmdtype_set(attr, APR_PROGRAM);
apr_procattr_detach_set(attr, 1);
if (((rv = apr_filepath_get(&cwd, 0, ptemp)) != APR_SUCCESS)
|| ((rv = apr_procattr_dir_set(attr, cwd)) != APR_SUCCESS)) {
ap_log_error(APLOG_MARK, APLOG_CRIT, rv, ap_server_conf, APLOGNO(00412)
"Parent: Failed to get the current path");
}
if (!args) {
if ((rv = ap_os_proc_filepath(&cmd, ptemp))
!= APR_SUCCESS) {
ap_log_error(APLOG_MARK, APLOG_CRIT, ERROR_BAD_PATHNAME, ap_server_conf, APLOGNO(00413)
"Parent: Failed to get full path of %s",
ap_server_conf->process->argv[0]);
apr_pool_destroy(ptemp);
return -1;
}
args = malloc((ap_server_conf->process->argc + 1) * sizeof (char*));
memcpy(args + 1, ap_server_conf->process->argv + 1,
(ap_server_conf->process->argc - 1) * sizeof (char*));
args[0] = malloc(strlen(cmd) + 1);
strcpy(args[0], cmd);
args[ap_server_conf->process->argc] = NULL;
} else {
cmd = args[0];
}
if ((rv = apr_procattr_io_set(attr, APR_FULL_BLOCK,
APR_NO_PIPE, APR_NO_PIPE)) != APR_SUCCESS) {
ap_log_error(APLOG_MARK, APLOG_CRIT, rv, ap_server_conf, APLOGNO(00414)
"Parent: Unable to create child stdin pipe.");
apr_pool_destroy(ptemp);
return -1;
}
waitlist[waitlist_ready] = CreateEvent(NULL, TRUE, FALSE, NULL);
if (!waitlist[waitlist_ready]) {
ap_log_error(APLOG_MARK, APLOG_CRIT, apr_get_os_error(), ap_server_conf, APLOGNO(00415)
"Parent: Could not create ready event for child process");
apr_pool_destroy (ptemp);
return -1;
}
hExitEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
if (!hExitEvent) {
ap_log_error(APLOG_MARK, APLOG_CRIT, apr_get_os_error(), ap_server_conf, APLOGNO(00416)
"Parent: Could not create exit event for child process");
apr_pool_destroy(ptemp);
CloseHandle(waitlist[waitlist_ready]);
return -1;
}
for (envc = 0; _environ[envc]; ++envc) {
;
}
env = apr_palloc(ptemp, (envc + 2) * sizeof (char*));
memcpy(env, _environ, envc * sizeof (char*));
apr_snprintf(pidbuf, sizeof(pidbuf), "AP_PARENT_PID=%lu", parent_pid);
env[envc] = pidbuf;
env[envc + 1] = NULL;
rv = apr_proc_create(&new_child, cmd, (const char * const *)args,
(const char * const *)env, attr, ptemp);
if (rv != APR_SUCCESS) {
ap_log_error(APLOG_MARK, APLOG_CRIT, rv, ap_server_conf, APLOGNO(00417)
"Parent: Failed to create the child process.");
apr_pool_destroy(ptemp);
CloseHandle(hExitEvent);
CloseHandle(waitlist[waitlist_ready]);
CloseHandle(new_child.hproc);
return -1;
}
ap_log_error(APLOG_MARK, APLOG_NOTICE, APR_SUCCESS, ap_server_conf, APLOGNO(00418)
"Parent: Created child process %d", new_child.pid);
if (send_handles_to_child(ptemp, waitlist[waitlist_ready], hExitEvent,
start_mutex, ap_scoreboard_shm,
new_child.hproc, new_child.in)) {
SetEvent(hExitEvent);
apr_pool_destroy(ptemp);
CloseHandle(hExitEvent);
CloseHandle(waitlist[waitlist_ready]);
CloseHandle(new_child.hproc);
return -1;
}
waitlist[waitlist_term] = new_child.hproc;
rv = WaitForMultipleObjects(2, waitlist, FALSE, INFINITE);
CloseHandle(waitlist[waitlist_ready]);
if (rv != WAIT_OBJECT_0) {
SetEvent(hExitEvent);
apr_pool_destroy(ptemp);
CloseHandle(hExitEvent);
CloseHandle(new_child.hproc);
return -1;
}
if (send_listeners_to_child(ptemp, new_child.pid, new_child.in)) {
SetEvent(hExitEvent);
apr_pool_destroy(ptemp);
CloseHandle(hExitEvent);
CloseHandle(new_child.hproc);
return -1;
}
apr_file_close(new_child.in);
*child_exit_event = hExitEvent;
*child_proc = new_child.hproc;
*child_pid = new_child.pid;
return 0;
}
#define NUM_WAIT_HANDLES 3
#define CHILD_HANDLE 0
#define SHUTDOWN_HANDLE 1
#define RESTART_HANDLE 2
static int master_main(server_rec *s, HANDLE shutdown_event, HANDLE restart_event) {
int rv, cld;
int child_created;
int restart_pending;
int shutdown_pending;
HANDLE child_exit_event;
HANDLE event_handles[NUM_WAIT_HANDLES];
DWORD child_pid;
child_created = restart_pending = shutdown_pending = 0;
event_handles[SHUTDOWN_HANDLE] = shutdown_event;
event_handles[RESTART_HANDLE] = restart_event;
rv = create_process(pconf, &event_handles[CHILD_HANDLE],
&child_exit_event, &child_pid);
if (rv < 0) {
ap_log_error(APLOG_MARK, APLOG_CRIT, apr_get_os_error(), ap_server_conf, APLOGNO(00419)
"master_main: create child process failed. Exiting.");
shutdown_pending = 1;
goto die_now;
}
child_created = 1;
if (!strcasecmp(signal_arg, "runservice")) {
mpm_service_started();
}
ap_scoreboard_image->parent[0].quiescing = 0;
winnt_note_child_started( 0, child_pid);
winnt_mpm_state = AP_MPMQ_RUNNING;
rv = WaitForMultipleObjects(NUM_WAIT_HANDLES, (HANDLE *) event_handles, FALSE, INFINITE);
cld = rv - WAIT_OBJECT_0;
if (rv == WAIT_FAILED) {
ap_log_error(APLOG_MARK,APLOG_CRIT, apr_get_os_error(), ap_server_conf, APLOGNO(00420)
"master_main: WaitForMultipleObjects WAIT_FAILED -- doing server shutdown");
shutdown_pending = 1;
} else if (rv == WAIT_TIMEOUT) {
ap_log_error(APLOG_MARK, APLOG_ERR, apr_get_os_error(), s, APLOGNO(00421)
"master_main: WaitForMultipleObjects with INFINITE wait exited with WAIT_TIMEOUT");
shutdown_pending = 1;
} else if (cld == SHUTDOWN_HANDLE) {
shutdown_pending = 1;
ap_log_error(APLOG_MARK, APLOG_NOTICE, APR_SUCCESS, s, APLOGNO(00422)
"Parent: Received shutdown signal -- Shutting down the server.");
if (ResetEvent(shutdown_event) == 0) {
ap_log_error(APLOG_MARK, APLOG_ERR, apr_get_os_error(), s, APLOGNO(00423)
"ResetEvent(shutdown_event)");
}
} else if (cld == RESTART_HANDLE) {
restart_pending = 1;
ap_log_error(APLOG_MARK, APLOG_NOTICE, 0, s, APLOGNO(00424)
"Parent: Received restart signal -- Restarting the server.");
if (ResetEvent(restart_event) == 0) {
ap_log_error(APLOG_MARK, APLOG_ERR, apr_get_os_error(), s, APLOGNO(00425)
"Parent: ResetEvent(restart_event) failed.");
}
if (SetEvent(child_exit_event) == 0) {
ap_log_error(APLOG_MARK, APLOG_ERR, apr_get_os_error(), s, APLOGNO(00426)
"Parent: SetEvent for child process event %pp failed.",
event_handles[CHILD_HANDLE]);
}
CloseHandle(event_handles[CHILD_HANDLE]);
event_handles[CHILD_HANDLE] = NULL;
} else {
DWORD exitcode;
if (!GetExitCodeProcess(event_handles[CHILD_HANDLE], &exitcode)) {
exitcode = APEXIT_CHILDFATAL;
}
if ( exitcode == APEXIT_CHILDFATAL
|| exitcode == APEXIT_CHILDINIT
|| exitcode == APEXIT_INIT) {
ap_log_error(APLOG_MARK, APLOG_CRIT, 0, ap_server_conf, APLOGNO(00427)
"Parent: child process %lu exited with status %lu -- Aborting.",
child_pid, exitcode);
shutdown_pending = 1;
} else {
int i;
restart_pending = 1;
ap_log_error(APLOG_MARK, APLOG_NOTICE, APR_SUCCESS, ap_server_conf, APLOGNO(00428)
"Parent: child process %lu exited with status %lu -- Restarting.",
child_pid, exitcode);
for (i = 0; i < ap_threads_per_child; i++) {
ap_update_child_status_from_indexes(0, i, SERVER_DEAD, NULL);
}
}
CloseHandle(event_handles[CHILD_HANDLE]);
event_handles[CHILD_HANDLE] = NULL;
}
winnt_note_child_killed( 0);
if (restart_pending) {
++my_generation;
ap_scoreboard_image->global->running_generation = my_generation;
}
die_now:
if (shutdown_pending) {
int timeout = 30000;
winnt_mpm_state = AP_MPMQ_STOPPING;
if (!child_created) {
return 0;
}
if (!strcasecmp(signal_arg, "runservice")) {
mpm_service_stopping();
}
if (SetEvent(child_exit_event) == 0) {
ap_log_error(APLOG_MARK,APLOG_ERR, apr_get_os_error(), ap_server_conf, APLOGNO(00429)
"Parent: SetEvent for child process event %pp failed",
event_handles[CHILD_HANDLE]);
}
if (event_handles[CHILD_HANDLE]) {
rv = WaitForSingleObject(event_handles[CHILD_HANDLE], timeout);
if (rv == WAIT_OBJECT_0) {
ap_log_error(APLOG_MARK,APLOG_NOTICE, APR_SUCCESS, ap_server_conf, APLOGNO(00430)
"Parent: Child process %lu exited successfully.", child_pid);
CloseHandle(event_handles[CHILD_HANDLE]);
event_handles[CHILD_HANDLE] = NULL;
} else {
ap_log_error(APLOG_MARK,APLOG_NOTICE, APR_SUCCESS, ap_server_conf, APLOGNO(00431)
"Parent: Forcing termination of child process %lu",
child_pid);
TerminateProcess(event_handles[CHILD_HANDLE], 1);
CloseHandle(event_handles[CHILD_HANDLE]);
event_handles[CHILD_HANDLE] = NULL;
}
}
CloseHandle(child_exit_event);
return 0;
}
winnt_mpm_state = AP_MPMQ_STARTING;
CloseHandle(child_exit_event);
return 1;
}
apr_array_header_t *mpm_new_argv;
static int winnt_query(int query_code, int *result, apr_status_t *rv) {
*rv = APR_SUCCESS;
switch (query_code) {
case AP_MPMQ_MAX_DAEMON_USED:
*result = MAXIMUM_WAIT_OBJECTS;
break;
case AP_MPMQ_IS_THREADED:
*result = AP_MPMQ_STATIC;
break;
case AP_MPMQ_IS_FORKED:
*result = AP_MPMQ_NOT_SUPPORTED;
break;
case AP_MPMQ_HARD_LIMIT_DAEMONS:
*result = HARD_SERVER_LIMIT;
break;
case AP_MPMQ_HARD_LIMIT_THREADS:
*result = thread_limit;
break;
case AP_MPMQ_MAX_THREADS:
*result = ap_threads_per_child;
break;
case AP_MPMQ_MIN_SPARE_DAEMONS:
*result = 0;
break;
case AP_MPMQ_MIN_SPARE_THREADS:
*result = 0;
break;
case AP_MPMQ_MAX_SPARE_DAEMONS:
*result = 0;
break;
case AP_MPMQ_MAX_SPARE_THREADS:
*result = 0;
break;
case AP_MPMQ_MAX_REQUESTS_DAEMON:
*result = ap_max_requests_per_child;
break;
case AP_MPMQ_MAX_DAEMONS:
*result = 1;
break;
case AP_MPMQ_MPM_STATE:
*result = winnt_mpm_state;
break;
case AP_MPMQ_GENERATION:
*result = my_generation;
break;
default:
*rv = APR_ENOTIMPL;
break;
}
return OK;
}
static const char *winnt_get_name(void) {
return "WinNT";
}
#define SERVICE_UNSET (-1)
static apr_status_t service_set = SERVICE_UNSET;
static apr_status_t service_to_start_success;
static int inst_argc;
static const char * const *inst_argv;
static const char *service_name = NULL;
static void winnt_rewrite_args(process_rec *process) {
apr_status_t rv;
char *def_server_root;
char *binpath;
char optbuf[3];
const char *opt_arg;
int fixed_args;
char *pid;
apr_getopt_t *opt;
int running_as_service = 1;
int errout = 0;
apr_file_t *nullfile;
pconf = process->pconf;
osver.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
GetVersionEx(&osver);
if (osver.dwPlatformId == VER_PLATFORM_WIN32_NT
&& ((osver.dwMajorVersion > 5)
|| ((osver.dwMajorVersion == 5) && (osver.dwMinorVersion > 0)))) {
stack_res_flag = STACK_SIZE_PARAM_IS_A_RESERVATION;
}
pid = getenv("AP_PARENT_PID");
if (pid) {
HANDLE filehand;
HANDLE hproc = GetCurrentProcess();
DWORD BytesRead;
my_pid = GetCurrentProcessId();
parent_pid = (DWORD) atol(pid);
ap_real_exit_code = 0;
pipe = GetStdHandle(STD_INPUT_HANDLE);
if (DuplicateHandle(hproc, pipe,
hproc, &filehand, 0, FALSE,
DUPLICATE_SAME_ACCESS)) {
pipe = filehand;
}
{
apr_file_t *infile, *outfile;
if ((apr_file_open_stdout(&outfile, process->pool) == APR_SUCCESS)
&& (apr_file_open_stdin(&infile, process->pool) == APR_SUCCESS))
apr_file_dup2(infile, outfile, process->pool);
}
if (!ReadFile(pipe, &my_generation, sizeof(my_generation),
&BytesRead, (LPOVERLAPPED) NULL)
|| (BytesRead != sizeof(my_generation))) {
ap_log_error(APLOG_MARK, APLOG_CRIT, apr_get_os_error(), NULL, APLOGNO(02965)
"Child: Unable to retrieve my generation from the parent");
exit(APEXIT_CHILDINIT);
}
signal_arg = "runchild";
return;
}
parent_pid = my_pid = GetCurrentProcessId();
atexit(hold_console_open_on_error);
if ((rv = ap_os_proc_filepath(&binpath, process->pconf))
!= APR_SUCCESS) {
ap_log_error(APLOG_MARK,APLOG_CRIT, rv, NULL, APLOGNO(00432)
"Failed to get the full path of %s", process->argv[0]);
exit(APEXIT_INIT);
}
def_server_root = (char *) apr_filepath_name_get(binpath);
if (def_server_root > binpath) {
*(def_server_root - 1) = '\0';
def_server_root = (char *) apr_filepath_name_get(binpath);
if (!strcasecmp(def_server_root, "bin"))
*(def_server_root - 1) = '\0';
}
apr_filepath_merge(&def_server_root, NULL, binpath,
APR_FILEPATH_TRUENAME, process->pool);
mpm_new_argv = apr_array_make(process->pool, process->argc + 2,
sizeof(const char *));
*(const char **)apr_array_push(mpm_new_argv) = process->argv[0];
*(const char **)apr_array_push(mpm_new_argv) = "-d";
*(const char **)apr_array_push(mpm_new_argv) = def_server_root;
fixed_args = mpm_new_argv->nelts;
optbuf[0] = '-';
optbuf[2] = '\0';
apr_getopt_init(&opt, process->pool, process->argc, process->argv);
opt->errfn = NULL;
while ((rv = apr_getopt(opt, "wn:k:" AP_SERVER_BASEARGS,
optbuf + 1, &opt_arg)) == APR_SUCCESS) {
switch (optbuf[1]) {
case 'w':
if (ap_real_exit_code)
ap_real_exit_code = 2;
break;
case 'n':
service_set = mpm_service_set_name(process->pool, &service_name,
opt_arg);
break;
case 'k':
signal_arg = opt_arg;
break;
case 'E':
errout = 1;
default:
*(const char **)apr_array_push(mpm_new_argv) =
apr_pstrdup(process->pool, optbuf);
if (opt_arg) {
*(const char **)apr_array_push(mpm_new_argv) = opt_arg;
}
break;
}
}
if (rv == APR_BADCH || rv == APR_BADARG) {
opt->ind--;
}
while (opt->ind < opt->argc) {
*(const char **)apr_array_push(mpm_new_argv) =
apr_pstrdup(process->pool, opt->argv[opt->ind++]);
}
inst_argc = mpm_new_argv->nelts - fixed_args;
if (!signal_arg) {
signal_arg = "run";
running_as_service = 0;
}
if (!strcasecmp(signal_arg, "runservice")) {
apr_filepath_set(def_server_root, process->pool);
if (!errout) {
mpm_nt_eventlog_stderr_open(service_name, process->pool);
}
service_to_start_success = mpm_service_to_start(&service_name,
process->pool);
if (service_to_start_success == APR_SUCCESS) {
service_set = APR_SUCCESS;
}
if ((rv = apr_file_open(&nullfile, "NUL",
APR_READ | APR_WRITE, APR_OS_DEFAULT,
process->pool)) == APR_SUCCESS) {
apr_file_t *nullstdout;
if (apr_file_open_stdout(&nullstdout, process->pool)
== APR_SUCCESS)
apr_file_dup2(nullstdout, nullfile, process->pool);
apr_file_close(nullfile);
}
}
if (service_set == SERVICE_UNSET && strcasecmp(signal_arg, "run")) {
service_set = mpm_service_set_name(process->pool, &service_name,
AP_DEFAULT_SERVICE_NAME);
}
if (!strcasecmp(signal_arg, "install")) {
if (service_set == APR_SUCCESS) {
ap_log_error(APLOG_MARK,APLOG_ERR, 0, NULL, APLOGNO(00433)
"%s: Service is already installed.", service_name);
exit(APEXIT_INIT);
}
} else if (running_as_service) {
if (service_set == APR_SUCCESS) {
if (!strcasecmp(signal_arg, "uninstall")) {
rv = mpm_service_uninstall();
exit(rv);
}
if ((!strcasecmp(signal_arg, "stop")) ||
(!strcasecmp(signal_arg, "shutdown"))) {
mpm_signal_service(process->pool, 0);
exit(0);
}
rv = mpm_merge_service_args(process->pool, mpm_new_argv,
fixed_args);
if (rv == APR_SUCCESS) {
ap_log_error(APLOG_MARK,APLOG_INFO, 0, NULL, APLOGNO(00434)
"Using ConfigArgs of the installed service "
"\"%s\".", service_name);
} else {
ap_log_error(APLOG_MARK,APLOG_WARNING, rv, NULL, APLOGNO(00435)
"No installed ConfigArgs for the service "
"\"%s\", using Apache defaults.", service_name);
}
} else {
ap_log_error(APLOG_MARK,APLOG_ERR, service_set, NULL, APLOGNO(00436)
"No installed service named \"%s\".", service_name);
exit(APEXIT_INIT);
}
}
if (strcasecmp(signal_arg, "install") && service_set && service_set != SERVICE_UNSET) {
ap_log_error(APLOG_MARK,APLOG_ERR, service_set, NULL, APLOGNO(00437)
"No installed service named \"%s\".", service_name);
exit(APEXIT_INIT);
}
inst_argv = (const char * const *)mpm_new_argv->elts
+ mpm_new_argv->nelts - inst_argc;
if (!strcasecmp(signal_arg, "config")) {
rv = mpm_service_install(process->pool, inst_argc, inst_argv, 1);
if (rv != APR_SUCCESS) {
exit(rv);
}
fprintf(stderr,"Testing httpd.conf....\n");
fprintf(stderr,"Errors reported here must be corrected before the "
"service can be started.\n");
} else if (!strcasecmp(signal_arg, "install")) {
rv = mpm_service_install(process->pool, inst_argc, inst_argv, 0);
if (rv != APR_SUCCESS) {
exit(rv);
}
fprintf(stderr,"Testing httpd.conf....\n");
fprintf(stderr,"Errors reported here must be corrected before the "
"service can be started.\n");
}
process->argc = mpm_new_argv->nelts;
process->argv = (const char * const *) mpm_new_argv->elts;
}
static int winnt_pre_config(apr_pool_t *pconf_, apr_pool_t *plog, apr_pool_t *ptemp) {
pconf = pconf_;
if (ap_exists_config_define("ONE_PROCESS") ||
ap_exists_config_define("DEBUG"))
one_process = -1;
ap_sys_privileges_handlers(1);
if (!strcasecmp(signal_arg, "runservice")
&& (service_to_start_success != APR_SUCCESS)) {
ap_log_error(APLOG_MARK,APLOG_CRIT, service_to_start_success, NULL, APLOGNO(00438)
"%s: Unable to start the service manager.",
service_name);
exit(APEXIT_INIT);
} else if (ap_state_query(AP_SQ_RUN_MODE) == AP_SQ_RM_NORMAL
&& !one_process && !my_generation) {
apr_file_t *nullfile;
apr_status_t rv;
apr_pool_t *pproc = apr_pool_parent_get(pconf);
if ((rv = apr_file_open(&nullfile, "NUL",
APR_READ | APR_WRITE, APR_OS_DEFAULT,
pproc)) == APR_SUCCESS) {
apr_file_t *nullstdout;
if (apr_file_open_stdout(&nullstdout, pproc)
== APR_SUCCESS)
apr_file_dup2(nullstdout, nullfile, pproc);
apr_file_close(nullfile);
}
}
ap_listen_pre_config();
thread_limit = DEFAULT_THREAD_LIMIT;
ap_threads_per_child = DEFAULT_THREADS_PER_CHILD;
return OK;
}
static int winnt_check_config(apr_pool_t *pconf, apr_pool_t *plog,
apr_pool_t *ptemp, server_rec* s) {
int is_parent;
int startup = 0;
is_parent = (parent_pid == my_pid);
if (is_parent &&
ap_state_query(AP_SQ_MAIN_STATE) == AP_SQ_MS_CREATE_PRE_CONFIG) {
startup = 1;
}
if (thread_limit > MAX_THREAD_LIMIT) {
if (startup) {
ap_log_error(APLOG_MARK, APLOG_WARNING | APLOG_STARTUP, 0, NULL, APLOGNO(00439)
"WARNING: ThreadLimit of %d exceeds compile-time "
"limit of %d threads, decreasing to %d.",
thread_limit, MAX_THREAD_LIMIT, MAX_THREAD_LIMIT);
} else if (is_parent) {
ap_log_error(APLOG_MARK, APLOG_WARNING, 0, s, APLOGNO(00440)
"ThreadLimit of %d exceeds compile-time limit "
"of %d, decreasing to match",
thread_limit, MAX_THREAD_LIMIT);
}
thread_limit = MAX_THREAD_LIMIT;
} else if (thread_limit < 1) {
if (startup) {
ap_log_error(APLOG_MARK, APLOG_WARNING | APLOG_STARTUP, 0, NULL, APLOGNO(00441)
"WARNING: ThreadLimit of %d not allowed, "
"increasing to 1.", thread_limit);
} else if (is_parent) {
ap_log_error(APLOG_MARK, APLOG_WARNING, 0, s, APLOGNO(00442)
"ThreadLimit of %d not allowed, increasing to 1",
thread_limit);
}
thread_limit = 1;
}
if (!first_thread_limit) {
first_thread_limit = thread_limit;
} else if (thread_limit != first_thread_limit) {
if (is_parent) {
ap_log_error(APLOG_MARK, APLOG_WARNING, 0, s, APLOGNO(00443)
"changing ThreadLimit to %d from original value "
"of %d not allowed during restart",
thread_limit, first_thread_limit);
}
thread_limit = first_thread_limit;
}
if (ap_threads_per_child > thread_limit) {
if (startup) {
ap_log_error(APLOG_MARK, APLOG_WARNING | APLOG_STARTUP, 0, NULL, APLOGNO(00444)
"WARNING: ThreadsPerChild of %d exceeds ThreadLimit "
"of %d threads, decreasing to %d. To increase, please "
"see the ThreadLimit directive.",
ap_threads_per_child, thread_limit, thread_limit);
} else if (is_parent) {
ap_log_error(APLOG_MARK, APLOG_WARNING, 0, s, APLOGNO(00445)
"ThreadsPerChild of %d exceeds ThreadLimit "
"of %d, decreasing to match",
ap_threads_per_child, thread_limit);
}
ap_threads_per_child = thread_limit;
} else if (ap_threads_per_child < 1) {
if (startup) {
ap_log_error(APLOG_MARK, APLOG_WARNING | APLOG_STARTUP, 0, NULL, APLOGNO(00446)
"WARNING: ThreadsPerChild of %d not allowed, "
"increasing to 1.", ap_threads_per_child);
} else if (is_parent) {
ap_log_error(APLOG_MARK, APLOG_WARNING, 0, s, APLOGNO(00447)
"ThreadsPerChild of %d not allowed, increasing to 1",
ap_threads_per_child);
}
ap_threads_per_child = 1;
}
return OK;
}
static int winnt_post_config(apr_pool_t *pconf, apr_pool_t *plog, apr_pool_t *ptemp, server_rec* s) {
apr_status_t rv = 0;
if (!strcasecmp(signal_arg, "install")) {
apr_pool_destroy(s->process->pool);
apr_terminate();
exit(0);
}
if (!strcasecmp(signal_arg, "config")) {
apr_pool_destroy(s->process->pool);
apr_terminate();
exit(0);
}
if (!strcasecmp(signal_arg, "start")) {
ap_listen_rec *lr;
for (lr = ap_listeners; lr; lr = lr->next) {
apr_socket_close(lr->sd);
lr->active = 0;
}
rv = mpm_service_start(ptemp, inst_argc, inst_argv);
apr_pool_destroy(s->process->pool);
apr_terminate();
exit (rv);
}
if (!strcasecmp(signal_arg, "restart")) {
mpm_signal_service(ptemp, 1);
apr_pool_destroy(s->process->pool);
apr_terminate();
exit (rv);
}
if (parent_pid == my_pid) {
if (ap_state_query(AP_SQ_MAIN_STATE) != AP_SQ_MS_CREATE_PRE_CONFIG
&& ap_state_query(AP_SQ_CONFIG_GEN) == 1) {
PSECURITY_ATTRIBUTES sa = GetNullACL();
setup_signal_names(apr_psprintf(pconf, "ap%lu", parent_pid));
ap_log_pid(pconf, ap_pid_fname);
shutdown_event = CreateEvent(sa, FALSE, FALSE, signal_shutdown_name);
if (!shutdown_event) {
ap_log_error(APLOG_MARK, APLOG_CRIT, apr_get_os_error(), ap_server_conf, APLOGNO(00448)
"Parent: Cannot create shutdown event %s", signal_shutdown_name);
CleanNullACL((void *)sa);
return HTTP_INTERNAL_SERVER_ERROR;
}
restart_event = CreateEvent(sa, FALSE, FALSE, signal_restart_name);
if (!restart_event) {
CloseHandle(shutdown_event);
ap_log_error(APLOG_MARK, APLOG_CRIT, apr_get_os_error(), ap_server_conf, APLOGNO(00449)
"Parent: Cannot create restart event %s", signal_restart_name);
CleanNullACL((void *)sa);
return HTTP_INTERNAL_SERVER_ERROR;
}
CleanNullACL((void *)sa);
rv = apr_proc_mutex_create(&start_mutex, NULL,
APR_LOCK_DEFAULT,
ap_server_conf->process->pool);
if (rv != APR_SUCCESS) {
ap_log_error(APLOG_MARK,APLOG_ERR, rv, ap_server_conf, APLOGNO(00450)
"%s: Unable to create the start_mutex.",
service_name);
return HTTP_INTERNAL_SERVER_ERROR;
}
}
if (strcasecmp(signal_arg, "runservice"))
mpm_start_console_handler();
} else {
mpm_start_child_console_handler();
}
return OK;
}
static int winnt_open_logs(apr_pool_t *p, apr_pool_t *plog, apr_pool_t *ptemp, server_rec *s) {
if (parent_pid != my_pid) {
return OK;
}
if (!strcasecmp(signal_arg, "restart")
|| !strcasecmp(signal_arg, "config")) {
return OK;
}
if (ap_setup_listeners(s) < 1) {
ap_log_error(APLOG_MARK, APLOG_ALERT|APLOG_STARTUP, 0,
NULL, APLOGNO(00451) "no listening sockets available, shutting down");
return !OK;
}
return OK;
}
static void winnt_child_init(apr_pool_t *pchild, struct server_rec *s) {
apr_status_t rv;
setup_signal_names(apr_psprintf(pchild, "ap%lu", parent_pid));
if (!one_process) {
get_handles_from_parent(s, &exit_event, &start_mutex,
&ap_scoreboard_shm);
get_listeners_from_parent(s);
CloseHandle(pipe);
} else {
rv = apr_proc_mutex_create(&start_mutex, signal_name_prefix,
APR_LOCK_DEFAULT, s->process->pool);
if (rv != APR_SUCCESS) {
ap_log_error(APLOG_MARK,APLOG_ERR, rv, ap_server_conf, APLOGNO(00452)
"%s child: Unable to init the start_mutex.",
service_name);
exit(APEXIT_CHILDINIT);
}
exit_event = shutdown_event;
}
}
static int winnt_run(apr_pool_t *_pconf, apr_pool_t *plog, server_rec *s ) {
static int restart = 0;
if (!restart && ((parent_pid == my_pid) || one_process)) {
if (ap_run_pre_mpm(s->process->pool, SB_SHARED) != OK) {
return !OK;
}
}
if ((parent_pid != my_pid) || one_process) {
ap_log_error(APLOG_MARK, APLOG_DEBUG, APR_SUCCESS, ap_server_conf, APLOGNO(00453)
"Child process is running");
child_main(pconf, parent_pid);
ap_log_error(APLOG_MARK, APLOG_DEBUG, APR_SUCCESS, ap_server_conf, APLOGNO(00454)
"Child process is exiting");
return DONE;
} else {
ap_log_error(APLOG_MARK, APLOG_NOTICE, 0, ap_server_conf, APLOGNO(00455)
"%s configured -- resuming normal operations",
ap_get_server_description());
ap_log_error(APLOG_MARK, APLOG_NOTICE, 0, ap_server_conf, APLOGNO(00456)
"Server built: %s", ap_get_server_built());
ap_log_command_line(plog, s);
ap_log_mpm_common(s);
restart = master_main(ap_server_conf, shutdown_event, restart_event);
if (!restart) {
ap_remove_pid(pconf, ap_pid_fname);
apr_proc_mutex_destroy(start_mutex);
CloseHandle(restart_event);
CloseHandle(shutdown_event);
return DONE;
}
}
return OK;
}
static void winnt_hooks(apr_pool_t *p) {
static const char *const aszSucc[] = {"core.c", NULL};
ap_hook_pre_config(winnt_pre_config, NULL, NULL, APR_HOOK_MIDDLE);
ap_hook_check_config(winnt_check_config, NULL, NULL, APR_HOOK_MIDDLE);
ap_hook_post_config(winnt_post_config, NULL, NULL, 0);
ap_hook_child_init(winnt_child_init, NULL, NULL, APR_HOOK_MIDDLE);
ap_hook_open_logs(winnt_open_logs, NULL, aszSucc, APR_HOOK_REALLY_FIRST);
ap_hook_mpm(winnt_run, NULL, NULL, APR_HOOK_MIDDLE);
ap_hook_mpm_query(winnt_query, NULL, NULL, APR_HOOK_MIDDLE);
ap_hook_mpm_get_name(winnt_get_name, NULL, NULL, APR_HOOK_MIDDLE);
}
AP_DECLARE_MODULE(mpm_winnt) = {
MPM20_MODULE_STUFF,
winnt_rewrite_args,
NULL,
NULL,
NULL,
NULL,
winnt_cmds,
winnt_hooks
};
#endif
