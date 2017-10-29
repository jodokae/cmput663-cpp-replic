#include "mod_proxy.h"
#include "scoreboard.h"
#include "ap_mpm.h"
#include "apr_version.h"
#include "ap_hooks.h"
module AP_MODULE_DECLARE_DATA lbmethod_byrequests_module;
static int (*ap_proxy_retry_worker_fn)(const char *proxy_function,
proxy_worker *worker, server_rec *s) = NULL;
static proxy_worker *find_best_byrequests(proxy_balancer *balancer,
request_rec *r) {
int i;
int total_factor = 0;
proxy_worker **worker;
proxy_worker *mycandidate = NULL;
int cur_lbset = 0;
int max_lbset = 0;
int checking_standby;
int checked_standby;
if (!ap_proxy_retry_worker_fn) {
ap_proxy_retry_worker_fn =
APR_RETRIEVE_OPTIONAL_FN(ap_proxy_retry_worker);
if (!ap_proxy_retry_worker_fn) {
return NULL;
}
}
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, APLOGNO(01207)
"proxy: Entering byrequests for BALANCER (%s)",
balancer->s->name);
do {
checking_standby = checked_standby = 0;
while (!mycandidate && !checked_standby) {
worker = (proxy_worker **)balancer->workers->elts;
for (i = 0; i < balancer->workers->nelts; i++, worker++) {
if (!checking_standby) {
if ((*worker)->s->lbset > max_lbset)
max_lbset = (*worker)->s->lbset;
}
if (
((*worker)->s->lbset != cur_lbset) ||
(checking_standby ? !PROXY_WORKER_IS_STANDBY(*worker) : PROXY_WORKER_IS_STANDBY(*worker)) ||
(PROXY_WORKER_IS_DRAINING(*worker))
) {
continue;
}
if (!PROXY_WORKER_IS_USABLE(*worker))
ap_proxy_retry_worker_fn("BALANCER", *worker, r->server);
if (PROXY_WORKER_IS_USABLE(*worker)) {
(*worker)->s->lbstatus += (*worker)->s->lbfactor;
total_factor += (*worker)->s->lbfactor;
if (!mycandidate || (*worker)->s->lbstatus > mycandidate->s->lbstatus)
mycandidate = *worker;
}
}
checked_standby = checking_standby++;
}
cur_lbset++;
} while (cur_lbset <= max_lbset && !mycandidate);
if (mycandidate) {
mycandidate->s->lbstatus -= total_factor;
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, APLOGNO(01208)
"proxy: byrequests selected worker \"%s\" : busy %" APR_SIZE_T_FMT " : lbstatus %d",
mycandidate->s->name, mycandidate->s->busy, mycandidate->s->lbstatus);
}
return mycandidate;
}
static apr_status_t reset(proxy_balancer *balancer, server_rec *s) {
int i;
proxy_worker **worker;
worker = (proxy_worker **)balancer->workers->elts;
for (i = 0; i < balancer->workers->nelts; i++, worker++) {
(*worker)->s->lbstatus = 0;
}
return APR_SUCCESS;
}
static apr_status_t age(proxy_balancer *balancer, server_rec *s) {
return APR_SUCCESS;
}
static const proxy_balancer_method byrequests = {
"byrequests",
&find_best_byrequests,
NULL,
&reset,
&age,
NULL
};
static void register_hook(apr_pool_t *p) {
ap_register_provider(p, PROXY_LBMETHOD, "byrequests", "0", &byrequests);
}
AP_DECLARE_MODULE(lbmethod_byrequests) = {
STANDARD20_MODULE_STUFF,
NULL,
NULL,
NULL,
NULL,
NULL,
register_hook
};
