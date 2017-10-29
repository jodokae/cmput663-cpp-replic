#if !defined(UNIXD_H)
#define UNIXD_H
#include "httpd.h"
#include "http_config.h"
#include "scoreboard.h"
#include "ap_listen.h"
#if defined(HAVE_SYS_TIME_H)
#include <sys/time.h>
#endif
#if defined(HAVE_SYS_RESOURCE_H)
#include <sys/resource.h>
#endif
#include "apr_hooks.h"
#include "apr_thread_proc.h"
#include "apr_proc_mutex.h"
#include "apr_global_mutex.h"
#include <pwd.h>
#include <grp.h>
#if APR_HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#if defined(HAVE_SYS_IPC_H)
#include <sys/ipc.h>
#endif
#if defined(__cplusplus)
extern "C" {
#endif
typedef struct {
uid_t uid;
gid_t gid;
int userdir;
} ap_unix_identity_t;
AP_DECLARE_HOOK(ap_unix_identity_t *, get_suexec_identity,(const request_rec *r))
#if !defined(DEFAULT_USER)
#define DEFAULT_USER "#-1"
#endif
#if !defined(DEFAULT_GROUP)
#define DEFAULT_GROUP "#-1"
#endif
typedef struct {
const char *user_name;
const char *group_name;
uid_t user_id;
gid_t group_id;
int suexec_enabled;
const char *chroot_dir;
const char *suexec_disabled_reason;
} unixd_config_rec;
AP_DECLARE_DATA extern unixd_config_rec ap_unixd_config;
#if defined(RLIMIT_CPU) || defined(RLIMIT_DATA) || defined(RLIMIT_VMEM) || defined(RLIMIT_NPROC) || defined(RLIMIT_AS)
AP_DECLARE(void) ap_unixd_set_rlimit(cmd_parms *cmd, struct rlimit **plimit,
const char *arg,
const char * arg2, int type);
#endif
AP_DECLARE(apr_status_t) ap_unixd_set_proc_mutex_perms(apr_proc_mutex_t *pmutex);
AP_DECLARE(apr_status_t) ap_unixd_set_global_mutex_perms(apr_global_mutex_t *gmutex);
AP_DECLARE(apr_status_t) ap_unixd_accept(void **accepted, ap_listen_rec *lr, apr_pool_t *ptrans);
#if defined(HAVE_KILLPG)
#define ap_unixd_killpg(x, y) (killpg ((x), (y)))
#define ap_os_killpg(x, y) (killpg ((x), (y)))
#else
#define ap_unixd_killpg(x, y) (kill (-(x), (y)))
#define ap_os_killpg(x, y) (kill (-(x), (y)))
#endif
typedef struct {
void *baton;
int volatile mpm_state;
int volatile shutdown_pending;
int volatile restart_pending;
int volatile is_ungraceful;
ap_generation_t my_generation;
int module_loads;
int was_graceful;
int num_buckets, max_buckets;
} ap_unixd_mpm_retained_data;
AP_DECLARE(ap_unixd_mpm_retained_data *) ap_unixd_mpm_get_retained_data(void);
AP_DECLARE(void) ap_unixd_mpm_set_signals(apr_pool_t *pconf, int once_process);
#if defined(__cplusplus)
}
#endif
#endif
