#include "apr.h"
#include "apr_strings.h"
#include "httpd.h"
#include "http_config.h"
#include "http_core.h"
#include "http_log.h"
#include "http_protocol.h"
#include "util_mutex.h"
#include "ap_config.h"
#if APR_HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#if APR_HAVE_UNISTD_H
#include <unistd.h>
#endif
#define HTML_HEADER "<html>\n<head>\n<title>Mod_example_IPC Status Page " "</title>\n</head>\n<body>\n<h1>Mod_example_IPC Status</h1>\n"
#define HTML_FOOTER "</body>\n</html>\n"
#define CAMPOUT 10
#define MAXCAMP 10
#define SLEEPYTIME 1000
apr_shm_t *exipc_shm;
char *shmfilename;
apr_global_mutex_t *exipc_mutex;
static const char *exipc_mutex_type = "example-ipc-shm";
typedef struct exipc_data {
apr_uint64_t counter;
} exipc_data;
static apr_status_t shm_cleanup_wrapper(void *unused) {
if (exipc_shm)
return apr_shm_destroy(exipc_shm);
return OK;
}
static int exipc_pre_config(apr_pool_t *pconf, apr_pool_t *plog,
apr_pool_t *ptemp) {
ap_mutex_register(pconf, exipc_mutex_type, NULL, APR_LOCK_DEFAULT, 0);
return OK;
}
static int exipc_post_config(apr_pool_t *pconf, apr_pool_t *plog,
apr_pool_t *ptemp, server_rec *s) {
apr_status_t rs;
exipc_data *base;
const char *tempdir;
if (ap_state_query(AP_SQ_MAIN_STATE) == AP_SQ_MS_CREATE_PRE_CONFIG)
return OK;
rs = apr_temp_dir_get(&tempdir, pconf);
if (APR_SUCCESS != rs) {
ap_log_error(APLOG_MARK, APLOG_ERR, rs, s, APLOGNO(02992)
"Failed to find temporary directory");
return HTTP_INTERNAL_SERVER_ERROR;
}
shmfilename = apr_psprintf(pconf, "%s/httpd_shm.%ld", tempdir,
(long int)getpid());
rs = apr_shm_create(&exipc_shm, sizeof(exipc_data),
(const char *) shmfilename, pconf);
if (APR_SUCCESS != rs) {
ap_log_error(APLOG_MARK, APLOG_ERR, rs, s, APLOGNO(02993)
"Failed to create shared memory segment on file %s",
shmfilename);
return HTTP_INTERNAL_SERVER_ERROR;
}
base = (exipc_data *)apr_shm_baseaddr_get(exipc_shm);
base->counter = 0;
rs = ap_global_mutex_create(&exipc_mutex, NULL, exipc_mutex_type, NULL,
s, pconf, 0);
if (APR_SUCCESS != rs) {
return HTTP_INTERNAL_SERVER_ERROR;
}
apr_pool_cleanup_register(pconf, NULL, shm_cleanup_wrapper,
apr_pool_cleanup_null);
return OK;
}
static void exipc_child_init(apr_pool_t *p, server_rec *s) {
apr_status_t rs;
rs = apr_global_mutex_child_init(&exipc_mutex,
apr_global_mutex_lockfile(exipc_mutex),
p);
if (APR_SUCCESS != rs) {
ap_log_error(APLOG_MARK, APLOG_CRIT, rs, s, APLOGNO(02994)
"Failed to reopen mutex %s in child",
exipc_mutex_type);
exit(1);
}
}
static int exipc_handler(request_rec *r) {
int gotlock = 0;
int camped;
apr_time_t startcamp;
apr_int64_t timecamped;
apr_status_t rs;
exipc_data *base;
if (strcmp(r->handler, "example_ipc")) {
return DECLINED;
}
for (camped = 0, timecamped = 0; camped < MAXCAMP; camped++) {
rs = apr_global_mutex_trylock(exipc_mutex);
if (APR_STATUS_IS_EBUSY(rs)) {
apr_sleep(CAMPOUT);
} else if (APR_SUCCESS == rs) {
gotlock = 1;
break;
} else if (APR_STATUS_IS_ENOTIMPL(rs)) {
startcamp = apr_time_now();
rs = apr_global_mutex_lock(exipc_mutex);
timecamped = (apr_int64_t) (apr_time_now() - startcamp);
if (APR_SUCCESS == rs) {
gotlock = 1;
break;
} else {
ap_log_error(APLOG_MARK, APLOG_ERR, rs, r->server, APLOGNO(02995)
"Child %ld failed to acquire lock",
(long int)getpid());
break;
}
} else {
ap_log_error(APLOG_MARK, APLOG_ERR, rs, r->server, APLOGNO(02996)
"Child %ld failed to try and acquire lock",
(long int)getpid());
break;
}
timecamped += CAMPOUT;
ap_log_error(APLOG_MARK, APLOG_NOTICE, 0, r->server, APLOGNO(03187)
"Child %ld camping out on mutex for %" APR_INT64_T_FMT
" microseconds",
(long int) getpid(), timecamped);
}
apr_sleep(SLEEPYTIME);
r->content_type = "text/html";
if (!r->header_only) {
ap_rputs(HTML_HEADER, r);
if (gotlock) {
base = (exipc_data *)apr_shm_baseaddr_get(exipc_shm);
base->counter++;
ap_rprintf(r, "<p>Lock acquired after %ld microseoncds.</p>\n",
(long int) timecamped);
ap_rputs("<table border=\"1\">\n", r);
ap_rprintf(r, "<tr><td>Child pid:</td><td>%d</td></tr>\n",
(int) getpid());
ap_rprintf(r, "<tr><td>Counter:</td><td>%u</td></tr>\n",
(unsigned int)base->counter);
ap_rputs("</table>\n", r);
} else {
ap_rprintf(r, "<p>Child %d failed to acquire lock "
"after camping out for %d microseconds.</p>\n",
(int) getpid(), (int) timecamped);
}
ap_rputs(HTML_FOOTER, r);
}
if (gotlock)
rs = apr_global_mutex_unlock(exipc_mutex);
return OK;
}
static void exipc_register_hooks(apr_pool_t *p) {
ap_hook_pre_config(exipc_pre_config, NULL, NULL, APR_HOOK_MIDDLE);
ap_hook_post_config(exipc_post_config, NULL, NULL, APR_HOOK_MIDDLE);
ap_hook_child_init(exipc_child_init, NULL, NULL, APR_HOOK_MIDDLE);
ap_hook_handler(exipc_handler, NULL, NULL, APR_HOOK_MIDDLE);
}
AP_DECLARE_MODULE(example_ipc) = {
STANDARD20_MODULE_STUFF,
NULL,
NULL,
NULL,
NULL,
NULL,
exipc_register_hooks
};
