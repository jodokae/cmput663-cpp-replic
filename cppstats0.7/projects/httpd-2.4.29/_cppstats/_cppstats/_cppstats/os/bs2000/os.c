#if defined(_OSD_POSIX)
#include "os.h"
#include "httpd.h"
#include "http_config.h"
#include "http_log.h"
#include "apr_lib.h"
#define USER_LEN 8
APLOG_USE_MODULE(core);
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
} else if (getuid() != 0) {
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
ap_log_error(APLOG_MARK, APLOG_ERR, 0, server, APLOGNO(02170)
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
APLOGNO(02171) "ufork: Possible mis-configuration "
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
#else
void bs2000_os_is_not_here() {
}
#endif
