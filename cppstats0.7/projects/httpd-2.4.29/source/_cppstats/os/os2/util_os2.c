#define INCL_DOS
#define INCL_DOSERRORS
#include <os2.h>
#include "ap_config.h"
#include "httpd.h"
#include "http_log.h"
#include "os.h"
#include <sys/time.h>
#include <sys/signal.h>
#include <ctype.h>
#include <string.h>
#include "apr_strings.h"
AP_DECLARE(apr_status_t) ap_os_create_privileged_process(
const request_rec *r,
apr_proc_t *newproc, const char *progname,
const char * const *args,
const char * const *env,
apr_procattr_t *attr, apr_pool_t *p) {
return apr_proc_create(newproc, progname, args, env, attr, p);
}
