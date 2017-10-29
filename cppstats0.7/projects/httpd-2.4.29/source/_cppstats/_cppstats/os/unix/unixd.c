#include "ap_config.h"
#include "httpd.h"
#include "http_config.h"
#include "http_main.h"
#include "http_log.h"
#include "unixd.h"
#include "mpm_common.h"
#include "os.h"
#include "ap_mpm.h"
#include "apr_thread_proc.h"
#include "apr_strings.h"
#include "apr_portable.h"
#if defined(HAVE_PWD_H)
#include <pwd.h>
#endif
#if defined(HAVE_SYS_RESOURCE_H)
#include <sys/resource.h>
#endif
#include <sys/stat.h>
#if defined(HAVE_UNISTD_H)
#include <unistd.h>
#endif
#if defined(HAVE_GRP_H)
#include <grp.h>
#endif
#if defined(HAVE_STRINGS_H)
#include <strings.h>
#endif
#if defined(HAVE_SYS_SEM_H)
#include <sys/sem.h>
#endif
#if defined(HAVE_SYS_PRCTL_H)
#include <sys/prctl.h>
#endif
unixd_config_rec ap_unixd_config;
APLOG_USE_MODULE(core);
AP_DECLARE(void) ap_unixd_set_rlimit(cmd_parms *cmd, struct rlimit **plimit,
const char *arg,
const char * arg2, int type) {
#if (defined(RLIMIT_CPU) || defined(RLIMIT_DATA) || defined(RLIMIT_VMEM) || defined(RLIMIT_NPROC) || defined(RLIMIT_AS)) && APR_HAVE_STRUCT_RLIMIT && APR_HAVE_GETRLIMIT
char *str;
struct rlimit *limit;
rlim_t cur = 0;
rlim_t max = 0;
*plimit = (struct rlimit *)apr_pcalloc(cmd->pool, sizeof(**plimit));
limit = *plimit;
if ((getrlimit(type, limit)) != 0) {
*plimit = NULL;
ap_log_error(APLOG_MARK, APLOG_ERR, errno, cmd->server, APLOGNO(02172)
"%s: getrlimit failed", cmd->cmd->name);
return;
}
if (*(str = ap_getword_conf(cmd->temp_pool, &arg)) != '\0') {
if (!strcasecmp(str, "max")) {
cur = limit->rlim_max;
} else {
cur = atol(str);
}
} else {
ap_log_error(APLOG_MARK, APLOG_ERR, 0, cmd->server, APLOGNO(02173)
"Invalid parameters for %s", cmd->cmd->name);
return;
}
if (arg2 && (*(str = ap_getword_conf(cmd->temp_pool, &arg2)) != '\0')) {
max = atol(str);
}
if (geteuid()) {
limit->rlim_cur = cur;
if (max && (max > limit->rlim_max)) {
ap_log_error(APLOG_MARK, APLOG_ERR, 0, cmd->server, APLOGNO(02174)
"Must be uid 0 to raise maximum %s", cmd->cmd->name);
} else if (max) {
limit->rlim_max = max;
}
} else {
if (cur) {
limit->rlim_cur = cur;
}
if (max) {
limit->rlim_max = max;
}
}
#else
ap_log_error(APLOG_MARK, APLOG_ERR, 0, cmd->server, APLOGNO(02175)
"Platform does not support rlimit for %s", cmd->cmd->name);
#endif
}
APR_HOOK_STRUCT(
APR_HOOK_LINK(get_suexec_identity)
)
AP_IMPLEMENT_HOOK_RUN_FIRST(ap_unix_identity_t *, get_suexec_identity,
(const request_rec *r), (r), NULL)
static apr_status_t ap_unix_create_privileged_process(
apr_proc_t *newproc, const char *progname,
const char * const *args,
const char * const *env,
apr_procattr_t *attr, ap_unix_identity_t *ugid,
apr_pool_t *p) {
int i = 0;
const char **newargs;
char *newprogname;
char *execuser, *execgroup;
const char *argv0;
if (!ap_unixd_config.suexec_enabled) {
return apr_proc_create(newproc, progname, args, env, attr, p);
}
argv0 = ap_strrchr_c(progname, '/');
if (argv0 != NULL) {
argv0++;
} else {
argv0 = progname;
}
if (ugid->userdir) {
execuser = apr_psprintf(p, "~%ld", (long) ugid->uid);
} else {
execuser = apr_psprintf(p, "%ld", (long) ugid->uid);
}
execgroup = apr_psprintf(p, "%ld", (long) ugid->gid);
if (!execuser || !execgroup) {
return APR_ENOMEM;
}
i = 0;
while (args[i])
i++;
newargs = apr_palloc(p, sizeof(char *) * (i + 4));
newprogname = SUEXEC_BIN;
newargs[0] = SUEXEC_BIN;
newargs[1] = execuser;
newargs[2] = execgroup;
newargs[3] = apr_pstrdup(p, argv0);
if(apr_procattr_cmdtype_set(attr, APR_PROGRAM) != APR_SUCCESS) {
return APR_EGENERAL;
}
i = 1;
do {
newargs[i + 3] = args[i];
} while (args[i++]);
return apr_proc_create(newproc, newprogname, newargs, env, attr, p);
}
AP_DECLARE(apr_status_t) ap_os_create_privileged_process(
const request_rec *r,
apr_proc_t *newproc, const char *progname,
const char * const *args,
const char * const *env,
apr_procattr_t *attr, apr_pool_t *p) {
ap_unix_identity_t *ugid = ap_run_get_suexec_identity(r);
if (ugid == NULL) {
return apr_proc_create(newproc, progname, args, env, attr, p);
}
return ap_unix_create_privileged_process(newproc, progname, args, env,
attr, ugid, p);
}
static apr_lockmech_e proc_mutex_mech(apr_proc_mutex_t *pmutex) {
const char *mechname = apr_proc_mutex_name(pmutex);
if (!strcmp(mechname, "sysvsem")) {
return APR_LOCK_SYSVSEM;
} else if (!strcmp(mechname, "flock")) {
return APR_LOCK_FLOCK;
}
return APR_LOCK_DEFAULT;
}
AP_DECLARE(apr_status_t) ap_unixd_set_proc_mutex_perms(apr_proc_mutex_t *pmutex) {
if (!geteuid()) {
apr_lockmech_e mech = proc_mutex_mech(pmutex);
switch(mech) {
#if APR_HAS_SYSVSEM_SERIALIZE
case APR_LOCK_SYSVSEM: {
apr_os_proc_mutex_t ospmutex;
#if !APR_HAVE_UNION_SEMUN
union semun {
long val;
struct semid_ds *buf;
unsigned short *array;
};
#endif
union semun ick;
struct semid_ds buf = { { 0 } };
apr_os_proc_mutex_get(&ospmutex, pmutex);
buf.sem_perm.uid = ap_unixd_config.user_id;
buf.sem_perm.gid = ap_unixd_config.group_id;
buf.sem_perm.mode = 0600;
ick.buf = &buf;
if (semctl(ospmutex.crossproc, 0, IPC_SET, ick) < 0) {
return errno;
}
}
break;
#endif
#if APR_HAS_FLOCK_SERIALIZE
case APR_LOCK_FLOCK: {
const char *lockfile = apr_proc_mutex_lockfile(pmutex);
if (lockfile) {
if (chown(lockfile, ap_unixd_config.user_id,
-1 ) < 0) {
return errno;
}
}
}
break;
#endif
default:
break;
}
}
return APR_SUCCESS;
}
AP_DECLARE(apr_status_t) ap_unixd_set_global_mutex_perms(apr_global_mutex_t *gmutex) {
#if !APR_PROC_MUTEX_IS_GLOBAL
apr_os_global_mutex_t osgmutex;
apr_os_global_mutex_get(&osgmutex, gmutex);
return ap_unixd_set_proc_mutex_perms(osgmutex.proc_mutex);
#else
return ap_unixd_set_proc_mutex_perms(gmutex);
#endif
}
AP_DECLARE(apr_status_t) ap_unixd_accept(void **accepted, ap_listen_rec *lr,
apr_pool_t *ptrans) {
apr_socket_t *csd;
apr_status_t status;
#if defined(_OSD_POSIX)
int sockdes;
#endif
*accepted = NULL;
status = apr_socket_accept(&csd, lr->sd, ptrans);
if (status == APR_SUCCESS) {
*accepted = csd;
#if defined(_OSD_POSIX)
apr_os_sock_get(&sockdes, csd);
if (sockdes >= FD_SETSIZE) {
ap_log_error(APLOG_MARK, APLOG_WARNING, 0, ap_server_conf, APLOGNO(02176)
"new file descriptor %d is too large; you probably need "
"to rebuild Apache with a larger FD_SETSIZE "
"(currently %d)",
sockdes, FD_SETSIZE);
apr_socket_close(csd);
return APR_EINTR;
}
#endif
return APR_SUCCESS;
}
if (APR_STATUS_IS_EINTR(status)) {
return status;
}
switch (status) {
#if defined(HPUX11) && defined(ENOBUFS)
case ENOBUFS:
#endif
#if defined(EPROTO)
case EPROTO:
#endif
#if defined(ECONNABORTED)
case ECONNABORTED:
#endif
#if defined(ECONNRESET)
case ECONNRESET:
#endif
#if defined(ETIMEDOUT)
case ETIMEDOUT:
#endif
#if defined(EHOSTUNREACH)
case EHOSTUNREACH:
#endif
#if defined(ENETUNREACH)
case ENETUNREACH:
#endif
#if defined(EAGAIN)
case EAGAIN:
#endif
#if defined(EWOULDBLOCK)
#if !defined(EAGAIN) || EAGAIN != EWOULDBLOCK
case EWOULDBLOCK:
#endif
#endif
break;
#if defined(ENETDOWN)
case ENETDOWN:
ap_log_error(APLOG_MARK, APLOG_EMERG, status, ap_server_conf, APLOGNO(02177)
"apr_socket_accept: giving up.");
return APR_EGENERAL;
#endif
default:
if (!lr->active) {
ap_log_error(APLOG_MARK, APLOG_DEBUG, status, ap_server_conf, APLOGNO(02178)
"apr_socket_accept failed for inactive listener");
return status;
}
ap_log_error(APLOG_MARK, APLOG_ERR, status, ap_server_conf, APLOGNO(02179)
"apr_socket_accept: (client socket)");
return APR_EGENERAL;
}
return status;
}
static ap_unixd_mpm_retained_data *retained_data = NULL;
AP_DECLARE(ap_unixd_mpm_retained_data *) ap_unixd_mpm_get_retained_data() {
if (!retained_data) {
retained_data = ap_retained_data_create("ap_unixd_mpm_retained_data",
sizeof(*retained_data));
retained_data->mpm_state = AP_MPMQ_STARTING;
}
return retained_data;
}
static void sig_term(int sig) {
retained_data->mpm_state = AP_MPMQ_STOPPING;
if (retained_data->shutdown_pending
&& (retained_data->is_ungraceful
|| sig == AP_SIG_GRACEFUL_STOP)) {
return;
}
retained_data->shutdown_pending = 1;
if (sig != AP_SIG_GRACEFUL_STOP) {
retained_data->is_ungraceful = 1;
}
}
static void sig_restart(int sig) {
retained_data->mpm_state = AP_MPMQ_STOPPING;
if (retained_data->restart_pending
&& (retained_data->is_ungraceful
|| sig == AP_SIG_GRACEFUL)) {
return;
}
retained_data->restart_pending = 1;
if (sig != AP_SIG_GRACEFUL) {
retained_data->is_ungraceful = 1;
}
}
static apr_status_t unset_signals(void *unused) {
retained_data->shutdown_pending = retained_data->restart_pending = 0;
retained_data->was_graceful = !retained_data->is_ungraceful;
retained_data->is_ungraceful = 0;
return APR_SUCCESS;
}
AP_DECLARE(void) ap_unixd_mpm_set_signals(apr_pool_t *pconf, int one_process) {
#if !defined(NO_USE_SIGACTION)
struct sigaction sa;
#endif
(void)ap_unixd_mpm_get_retained_data();
#if !defined(NO_USE_SIGACTION)
memset(&sa, 0, sizeof sa);
sigemptyset(&sa.sa_mask);
#if defined(SIGPIPE)
sa.sa_handler = SIG_IGN;
if (sigaction(SIGPIPE, &sa, NULL) < 0)
ap_log_error(APLOG_MARK, APLOG_WARNING, errno, ap_server_conf, APLOGNO(00269)
"sigaction(SIGPIPE)");
#endif
#if defined(SIGXCPU)
sa.sa_handler = SIG_DFL;
if (sigaction(SIGXCPU, &sa, NULL) < 0)
ap_log_error(APLOG_MARK, APLOG_WARNING, errno, ap_server_conf, APLOGNO(00267)
"sigaction(SIGXCPU)");
#endif
#if defined(SIGXFSZ)
sa.sa_handler = SIG_IGN;
if (sigaction(SIGXFSZ, &sa, NULL) < 0)
ap_log_error(APLOG_MARK, APLOG_WARNING, errno, ap_server_conf, APLOGNO(00268)
"sigaction(SIGXFSZ)");
#endif
sa.sa_handler = sig_term;
if (sigaction(SIGTERM, &sa, NULL) < 0)
ap_log_error(APLOG_MARK, APLOG_WARNING, errno, ap_server_conf, APLOGNO(00264)
"sigaction(SIGTERM)");
#if defined(SIGINT)
if (sigaction(SIGINT, &sa, NULL) < 0)
ap_log_error(APLOG_MARK, APLOG_WARNING, errno, ap_server_conf, APLOGNO(00266)
"sigaction(SIGINT)");
#endif
#if defined(AP_SIG_GRACEFUL_STOP)
if (sigaction(AP_SIG_GRACEFUL_STOP, &sa, NULL) < 0)
ap_log_error(APLOG_MARK, APLOG_WARNING, errno, ap_server_conf, APLOGNO(00265)
"sigaction(" AP_SIG_GRACEFUL_STOP_STRING ")");
#endif
if (!one_process) {
sa.sa_handler = sig_restart;
if (sigaction(SIGHUP, &sa, NULL) < 0)
ap_log_error(APLOG_MARK, APLOG_WARNING, errno, ap_server_conf, APLOGNO(00270)
"sigaction(SIGHUP)");
if (sigaction(AP_SIG_GRACEFUL, &sa, NULL) < 0)
ap_log_error(APLOG_MARK, APLOG_WARNING, errno, ap_server_conf, APLOGNO(00271)
"sigaction(" AP_SIG_GRACEFUL_STRING ")");
}
#else
#if defined(SIGPIPE)
apr_signal(SIGPIPE, SIG_IGN);
#endif
#if defined(SIGXCPU)
apr_signal(SIGXCPU, SIG_DFL);
#endif
#if defined(SIGXFSZ)
apr_signal(SIGXFSZ, SIG_IGN);
#endif
apr_signal(SIGTERM, sig_term);
#if defined(AP_SIG_GRACEFUL_STOP)
apr_signal(AP_SIG_GRACEFUL_STOP, sig_term);
#endif
if (!one_process) {
#if defined(SIGHUP)
apr_signal(SIGHUP, sig_restart);
#endif
#if defined(AP_SIG_GRACEFUL)
apr_signal(AP_SIG_GRACEFUL, sig_restart);
#endif
}
#endif
apr_pool_cleanup_register(pconf, NULL, unset_signals,
apr_pool_cleanup_null);
}
#if defined(_OSD_POSIX)
#include "apr_lib.h"
#define USER_LEN 8
typedef enum {
bs2_unknown,
bs2_noFORK,
bs2_FORK,
bs2_UFORK
} bs2_ForkType;
static bs2_ForkType forktype = bs2_unknown;
static bs2_ForkType os_forktype(int one_process) {
if (forktype == bs2_unknown) {
if (one_process) {
forktype = bs2_noFORK;
}
else if (getuid() != 0) {
forktype = bs2_FORK;
} else
forktype = bs2_UFORK;
}
return forktype;
}
int os_init_job_environment(server_rec *server, const char *user_name, int one_process) {
bs2_ForkType type = os_forktype(one_process);
if (one_process) {
type = forktype = bs2_noFORK;
ap_log_error(APLOG_MARK, APLOG_ERR, 0, server, APLOGNO(02180)
"The debug mode of Apache should only "
"be started by an unprivileged user!");
return 0;
}
return 0;
}
pid_t os_fork(const char *user) {
pid_t pid;
char username[USER_LEN+1];
switch (os_forktype(0)) {
case bs2_FORK:
pid = fork();
break;
case bs2_UFORK:
apr_cpystrn(username, user, sizeof username);
ap_str_toupper(username);
pid = ufork(username);
if (pid == -1 && errno == EPERM) {
ap_log_error(APLOG_MARK, APLOG_EMERG, errno, ap_server_conf,
APLOGNO(02181) "ufork: Possible mis-configuration "
"for user %s - Aborting.", user);
exit(1);
}
break;
default:
pid = 0;
break;
}
return pid;
}
#endif
