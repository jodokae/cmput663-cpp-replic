#if !defined(AP_MPM_H)
#define AP_MPM_H
#include "apr_thread_proc.h"
#include "httpd.h"
#include "scoreboard.h"
#if defined(__cplusplus)
extern "C" {
#endif
AP_DECLARE_HOOK(int, mpm, (apr_pool_t *pconf, apr_pool_t *plog, server_rec *server_conf))
AP_DECLARE(apr_status_t) ap_os_create_privileged_process(
const request_rec *r,
apr_proc_t *newproc,
const char *progname,
const char * const *args,
const char * const *env,
apr_procattr_t *attr,
apr_pool_t *p);
#define AP_MPMQ_NOT_SUPPORTED 0
#define AP_MPMQ_STATIC 1
#define AP_MPMQ_DYNAMIC 2
#define AP_MPMQ_STARTING 0
#define AP_MPMQ_RUNNING 1
#define AP_MPMQ_STOPPING 2
#define AP_MPMQ_MAX_DAEMON_USED 1
#define AP_MPMQ_IS_THREADED 2
#define AP_MPMQ_IS_FORKED 3
#define AP_MPMQ_HARD_LIMIT_DAEMONS 4
#define AP_MPMQ_HARD_LIMIT_THREADS 5
#define AP_MPMQ_MAX_THREADS 6
#define AP_MPMQ_MIN_SPARE_DAEMONS 7
#define AP_MPMQ_MIN_SPARE_THREADS 8
#define AP_MPMQ_MAX_SPARE_DAEMONS 9
#define AP_MPMQ_MAX_SPARE_THREADS 10
#define AP_MPMQ_MAX_REQUESTS_DAEMON 11
#define AP_MPMQ_MAX_DAEMONS 12
#define AP_MPMQ_MPM_STATE 13
#define AP_MPMQ_IS_ASYNC 14
#define AP_MPMQ_GENERATION 15
#define AP_MPMQ_HAS_SERF 16
AP_DECLARE(apr_status_t) ap_mpm_query(int query_code, int *result);
typedef void (ap_mpm_callback_fn_t)(void *baton);
AP_DECLARE(apr_status_t) ap_mpm_register_timed_callback(apr_time_t t,
ap_mpm_callback_fn_t *cbfn,
void *baton);
typedef enum mpm_child_status {
MPM_CHILD_STARTED,
MPM_CHILD_EXITED,
MPM_CHILD_LOST_SLOT
} mpm_child_status;
AP_DECLARE_HOOK(void,child_status,(server_rec *s, pid_t pid, ap_generation_t gen,
int slot, mpm_child_status state))
AP_DECLARE_HOOK(void,end_generation,(server_rec *s, ap_generation_t gen))
#if defined(GPROF)
extern void moncontrol(int);
#define AP_MONCONTROL(x) moncontrol(x)
#else
#define AP_MONCONTROL(x)
#endif
#if defined(AP_ENABLE_EXCEPTION_HOOK)
typedef struct ap_exception_info_t {
int sig;
pid_t pid;
} ap_exception_info_t;
AP_DECLARE_HOOK(int,fatal_exception,(ap_exception_info_t *ei))
#endif
#if defined(__cplusplus)
}
#endif
#endif
