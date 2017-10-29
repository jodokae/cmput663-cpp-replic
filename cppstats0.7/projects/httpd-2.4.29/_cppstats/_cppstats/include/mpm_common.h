#if !defined(APACHE_MPM_COMMON_H)
#define APACHE_MPM_COMMON_H
#include "ap_config.h"
#include "ap_mpm.h"
#include "scoreboard.h"
#if APR_HAVE_NETINET_TCP_H
#include <netinet/tcp.h>
#endif
#include "apr_proc_mutex.h"
#if defined(__cplusplus)
extern "C" {
#endif
#if !defined(DEFAULT_LISTENBACKLOG)
#define DEFAULT_LISTENBACKLOG 511
#endif
#define AP_SIG_GRACEFUL SIGUSR1
#define AP_SIG_GRACEFUL_SHORT USR1
#define AP_SIG_GRACEFUL_STRING "SIGUSR1"
#define AP_SIG_GRACEFUL_STOP SIGWINCH
#define AP_SIG_GRACEFUL_STOP_SHORT WINCH
#define AP_SIG_GRACEFUL_STOP_STRING "SIGWINCH"
typedef void ap_reclaim_callback_fn_t(int childnum, pid_t pid,
ap_generation_t gen);
#if (!defined(WIN32) && !defined(NETWARE)) || defined(DOXYGEN)
AP_DECLARE(void) ap_reclaim_child_processes(int terminate,
ap_reclaim_callback_fn_t *mpm_callback);
AP_DECLARE(void) ap_relieve_child_processes(ap_reclaim_callback_fn_t *mpm_callback);
AP_DECLARE(void) ap_register_extra_mpm_process(pid_t pid, ap_generation_t gen);
AP_DECLARE(int) ap_unregister_extra_mpm_process(pid_t pid, ap_generation_t *old_gen);
AP_DECLARE(apr_status_t) ap_mpm_safe_kill(pid_t pid, int sig);
AP_DECLARE(int) ap_process_child_status(apr_proc_t *pid, apr_exit_why_e why, int status);
AP_DECLARE(apr_status_t) ap_fatal_signal_setup(server_rec *s, apr_pool_t *in_pconf);
AP_DECLARE(apr_status_t) ap_fatal_signal_child_setup(server_rec *s);
#endif
apr_status_t ap_mpm_end_gen_helper(void *unused);
AP_DECLARE(void) ap_wait_or_timeout(apr_exit_why_e *status, int *exitcode,
apr_proc_t *ret, apr_pool_t *p,
server_rec *s);
#if defined(TCP_NODELAY)
void ap_sock_disable_nagle(apr_socket_t *s);
#else
#define ap_sock_disable_nagle(s)
#endif
#if defined(HAVE_GETPWNAM)
AP_DECLARE(uid_t) ap_uname2id(const char *name);
#endif
#if defined(HAVE_GETGRNAM)
AP_DECLARE(gid_t) ap_gname2id(const char *name);
#endif
#if !defined(HAVE_INITGROUPS)
int initgroups(const char *name, gid_t basegid);
#endif
#if (!defined(WIN32) && !defined(NETWARE)) || defined(DOXYGEN)
typedef struct ap_pod_t ap_pod_t;
struct ap_pod_t {
apr_file_t *pod_in;
apr_file_t *pod_out;
apr_pool_t *p;
};
AP_DECLARE(apr_status_t) ap_mpm_pod_open(apr_pool_t *p, ap_pod_t **pod);
AP_DECLARE(apr_status_t) ap_mpm_pod_check(ap_pod_t *pod);
AP_DECLARE(apr_status_t) ap_mpm_pod_close(ap_pod_t *pod);
AP_DECLARE(apr_status_t) ap_mpm_pod_signal(ap_pod_t *pod);
AP_DECLARE(void) ap_mpm_pod_killpg(ap_pod_t *pod, int num);
#define AP_MPM_PODX_RESTART_CHAR '$'
#define AP_MPM_PODX_GRACEFUL_CHAR '!'
typedef enum { AP_MPM_PODX_NORESTART, AP_MPM_PODX_RESTART, AP_MPM_PODX_GRACEFUL } ap_podx_restart_t;
AP_DECLARE(apr_status_t) ap_mpm_podx_open(apr_pool_t *p, ap_pod_t **pod);
AP_DECLARE(int) ap_mpm_podx_check(ap_pod_t *pod);
AP_DECLARE(apr_status_t) ap_mpm_podx_close(ap_pod_t *pod);
AP_DECLARE(apr_status_t) ap_mpm_podx_signal(ap_pod_t *pod,
ap_podx_restart_t graceful);
AP_DECLARE(void) ap_mpm_podx_killpg(ap_pod_t *pod, int num,
ap_podx_restart_t graceful);
#endif
AP_DECLARE(const char *) ap_check_mpm(void);
AP_DECLARE_DATA extern int ap_max_requests_per_child;
const char *ap_mpm_set_max_requests(cmd_parms *cmd, void *dummy,
const char *arg);
AP_DECLARE_DATA extern const char *ap_pid_fname;
const char *ap_mpm_set_pidfile(cmd_parms *cmd, void *dummy,
const char *arg);
void ap_mpm_dump_pidfile(apr_pool_t *p, apr_file_t *out);
AP_DECLARE_DATA extern char ap_coredump_dir[MAX_STRING_LEN];
AP_DECLARE_DATA extern int ap_coredumpdir_configured;
const char *ap_mpm_set_coredumpdir(cmd_parms *cmd, void *dummy,
const char *arg);
AP_DECLARE_DATA extern int ap_graceful_shutdown_timeout;
AP_DECLARE(const char *)ap_mpm_set_graceful_shutdown(cmd_parms *cmd, void *dummy,
const char *arg);
#define AP_GRACEFUL_SHUTDOWN_TIMEOUT_COMMAND AP_INIT_TAKE1("GracefulShutdownTimeout", ap_mpm_set_graceful_shutdown, NULL, RSRC_CONF, "Maximum time in seconds to wait for child " "processes to complete transactions during shutdown")
int ap_signal_server(int *, apr_pool_t *);
void ap_mpm_rewrite_args(process_rec *);
AP_DECLARE_DATA extern apr_uint32_t ap_max_mem_free;
extern const char *ap_mpm_set_max_mem_free(cmd_parms *cmd, void *dummy,
const char *arg);
AP_DECLARE_DATA extern apr_size_t ap_thread_stacksize;
extern const char *ap_mpm_set_thread_stacksize(cmd_parms *cmd, void *dummy,
const char *arg);
extern void ap_core_child_status(server_rec *s, pid_t pid, ap_generation_t gen,
int slot, mpm_child_status status);
#if AP_ENABLE_EXCEPTION_HOOK
extern const char *ap_mpm_set_exception_hook(cmd_parms *cmd, void *dummy,
const char *arg);
#endif
AP_DECLARE_HOOK(int,monitor,(apr_pool_t *p, server_rec *s))
AP_DECLARE(int) ap_sys_privileges_handlers(int inc);
AP_DECLARE_HOOK(int, drop_privileges, (apr_pool_t * pchild, server_rec * s))
AP_DECLARE_HOOK(int, mpm_query, (int query_code, int *result, apr_status_t *rv))
AP_DECLARE_HOOK(apr_status_t, mpm_register_timed_callback,
(apr_time_t t, ap_mpm_callback_fn_t *cbfn, void *baton))
AP_DECLARE_HOOK(const char *,mpm_get_name,(void))
AP_DECLARE_HOOK(void, suspend_connection,
(conn_rec *c, request_rec *r))
AP_DECLARE_HOOK(void, resume_connection,
(conn_rec *c, request_rec *r))
#define AP_ACCEPT_MUTEX_TYPE "mpm-accept"
void mpm_common_pre_config(apr_pool_t *pconf);
#if defined(__cplusplus)
}
#endif
#endif