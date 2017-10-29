#include "httpd.h"
#include "http_config.h"
#include "http_log.h"
#include "util_filter.h"
#include "mod_ratelimit.h"
#define RATE_LIMIT_FILTER_NAME "RATE_LIMIT"
#define RATE_INTERVAL_MS (200)
typedef enum rl_state_e {
RATE_ERROR,
RATE_LIMIT,
RATE_FULLSPEED
} rl_state_e;
typedef struct rl_ctx_t {
int speed;
int chunk_size;
int burst;
rl_state_e state;
apr_bucket_brigade *tmpbb;
apr_bucket_brigade *holdingbb;
} rl_ctx_t;
#if defined(RLFDEBUG)
static void brigade_dump(request_rec *r, apr_bucket_brigade *bb) {
apr_bucket *e;
int i = 0;
for (e = APR_BRIGADE_FIRST(bb);
e != APR_BRIGADE_SENTINEL(bb); e = APR_BUCKET_NEXT(e), i++) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(03193)
"brigade: [%d] %s", i, e->type->name);
}
}
#endif
static apr_status_t
rate_limit_filter(ap_filter_t *f, apr_bucket_brigade *input_bb) {
apr_status_t rv = APR_SUCCESS;
rl_ctx_t *ctx = f->ctx;
apr_bucket *fb;
int do_sleep = 0;
apr_bucket_alloc_t *ba = f->r->connection->bucket_alloc;
apr_bucket_brigade *bb = input_bb;
if (f->c->aborted) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, f->r, APLOGNO(01454) "rl: conn aborted");
apr_brigade_cleanup(bb);
return APR_ECONNABORTED;
}
if (ctx == NULL) {
const char *rl = NULL;
int ratelimit;
int burst = 0;
if (f->r->main != NULL) {
ap_remove_output_filter(f);
return ap_pass_brigade(f->next, bb);
}
rl = apr_table_get(f->r->subprocess_env, "rate-limit");
if (rl == NULL) {
ap_remove_output_filter(f);
return ap_pass_brigade(f->next, bb);
}
ratelimit = atoi(rl) * 1024;
if (ratelimit <= 0) {
ap_log_rerror(APLOG_MARK, APLOG_INFO, 0, f->r,
APLOGNO(03488) "rl: disabling: rate-limit = %s (too high?)", rl);
ap_remove_output_filter(f);
return ap_pass_brigade(f->next, bb);
}
rl = apr_table_get(f->r->subprocess_env, "rate-initial-burst");
if (rl != NULL) {
burst = atoi(rl) * 1024;
if (burst <= 0) {
ap_log_rerror(APLOG_MARK, APLOG_INFO, 0, f->r,
APLOGNO(03489) "rl: disabling burst: rate-initial-burst = %s (too high?)", rl);
burst = 0;
}
}
ctx = apr_palloc(f->r->pool, sizeof(rl_ctx_t));
f->ctx = ctx;
ctx->state = RATE_LIMIT;
ctx->speed = ratelimit;
ctx->burst = burst;
ctx->chunk_size = (ctx->speed / (1000 / RATE_INTERVAL_MS));
ctx->tmpbb = apr_brigade_create(f->r->pool, ba);
ctx->holdingbb = apr_brigade_create(f->r->pool, ba);
}
while (ctx->state != RATE_ERROR &&
(!APR_BRIGADE_EMPTY(bb) || !APR_BRIGADE_EMPTY(ctx->holdingbb))) {
apr_bucket *e;
if (!APR_BRIGADE_EMPTY(ctx->holdingbb)) {
APR_BRIGADE_CONCAT(bb, ctx->holdingbb);
}
while (ctx->state == RATE_FULLSPEED && !APR_BRIGADE_EMPTY(bb)) {
for (e = APR_BRIGADE_FIRST(bb);
e != APR_BRIGADE_SENTINEL(bb); e = APR_BUCKET_NEXT(e)) {
if (AP_RL_BUCKET_IS_END(e)) {
apr_bucket *f;
f = APR_RING_LAST(&bb->list);
APR_RING_UNSPLICE(e, f, link);
APR_RING_SPLICE_TAIL(&ctx->holdingbb->list, e, f,
apr_bucket, link);
ctx->state = RATE_LIMIT;
break;
}
}
if (f->c->aborted) {
apr_brigade_cleanup(bb);
ctx->state = RATE_ERROR;
break;
}
fb = apr_bucket_flush_create(ba);
APR_BRIGADE_INSERT_TAIL(bb, fb);
rv = ap_pass_brigade(f->next, bb);
if (rv != APR_SUCCESS) {
ctx->state = RATE_ERROR;
ap_log_rerror(APLOG_MARK, APLOG_TRACE1, rv, f->r, APLOGNO(01455)
"rl: full speed brigade pass failed.");
}
}
while (ctx->state == RATE_LIMIT && !APR_BRIGADE_EMPTY(bb)) {
for (e = APR_BRIGADE_FIRST(bb);
e != APR_BRIGADE_SENTINEL(bb); e = APR_BUCKET_NEXT(e)) {
if (AP_RL_BUCKET_IS_START(e)) {
apr_bucket *f;
f = APR_RING_LAST(&bb->list);
APR_RING_UNSPLICE(e, f, link);
APR_RING_SPLICE_TAIL(&ctx->holdingbb->list, e, f,
apr_bucket, link);
ctx->state = RATE_FULLSPEED;
break;
}
}
while (!APR_BRIGADE_EMPTY(bb)) {
apr_bucket *stop_point;
apr_off_t len = 0;
if (f->c->aborted) {
apr_brigade_cleanup(bb);
ctx->state = RATE_ERROR;
break;
}
if (do_sleep) {
apr_sleep(RATE_INTERVAL_MS * 1000);
} else {
do_sleep = 1;
}
apr_brigade_length(bb, 1, &len);
rv = apr_brigade_partition(bb,
ctx->chunk_size + ctx->burst, &stop_point);
if (rv != APR_SUCCESS && rv != APR_INCOMPLETE) {
ctx->state = RATE_ERROR;
ap_log_rerror(APLOG_MARK, APLOG_ERR, rv, f->r, APLOGNO(01456)
"rl: partition failed.");
break;
}
if (stop_point != APR_BRIGADE_SENTINEL(bb)) {
apr_bucket *f;
apr_bucket *e = APR_BUCKET_PREV(stop_point);
f = APR_RING_FIRST(&bb->list);
APR_RING_UNSPLICE(f, e, link);
APR_RING_SPLICE_HEAD(&ctx->tmpbb->list, f, e, apr_bucket,
link);
} else {
APR_BRIGADE_CONCAT(ctx->tmpbb, bb);
}
fb = apr_bucket_flush_create(ba);
APR_BRIGADE_INSERT_TAIL(ctx->tmpbb, fb);
if (ctx->burst) {
len = ctx->burst;
apr_brigade_length(ctx->tmpbb, 1, &len);
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, f->r,
APLOGNO(03485) "rl: burst %d; len %"APR_OFF_T_FMT, ctx->burst, len);
if (len < ctx->burst) {
ctx->burst -= len;
} else {
ctx->burst = 0;
}
}
#if defined(RLFDEBUG)
brigade_dump(f->r, ctx->tmpbb);
brigade_dump(f->r, bb);
#endif
rv = ap_pass_brigade(f->next, ctx->tmpbb);
apr_brigade_cleanup(ctx->tmpbb);
if (rv != APR_SUCCESS) {
ctx->state = RATE_ERROR;
ap_log_rerror(APLOG_MARK, APLOG_TRACE1, rv, f->r, APLOGNO(01457)
"rl: brigade pass failed.");
break;
}
}
}
}
return rv;
}
static apr_status_t
rl_bucket_read(apr_bucket *b, const char **str,
apr_size_t *len, apr_read_type_e block) {
*str = NULL;
*len = 0;
return APR_SUCCESS;
}
AP_RL_DECLARE(apr_bucket *)
ap_rl_end_create(apr_bucket_alloc_t *list) {
apr_bucket *b = apr_bucket_alloc(sizeof(*b), list);
APR_BUCKET_INIT(b);
b->free = apr_bucket_free;
b->list = list;
b->length = 0;
b->start = 0;
b->data = NULL;
b->type = &ap_rl_bucket_type_end;
return b;
}
AP_RL_DECLARE(apr_bucket *)
ap_rl_start_create(apr_bucket_alloc_t *list) {
apr_bucket *b = apr_bucket_alloc(sizeof(*b), list);
APR_BUCKET_INIT(b);
b->free = apr_bucket_free;
b->list = list;
b->length = 0;
b->start = 0;
b->data = NULL;
b->type = &ap_rl_bucket_type_start;
return b;
}
AP_RL_DECLARE_DATA const apr_bucket_type_t ap_rl_bucket_type_end = {
"RL_END", 5, APR_BUCKET_METADATA,
apr_bucket_destroy_noop,
rl_bucket_read,
apr_bucket_setaside_noop,
apr_bucket_split_notimpl,
apr_bucket_simple_copy
};
AP_RL_DECLARE_DATA const apr_bucket_type_t ap_rl_bucket_type_start = {
"RL_START", 5, APR_BUCKET_METADATA,
apr_bucket_destroy_noop,
rl_bucket_read,
apr_bucket_setaside_noop,
apr_bucket_split_notimpl,
apr_bucket_simple_copy
};
static void register_hooks(apr_pool_t *p) {
ap_register_output_filter(RATE_LIMIT_FILTER_NAME, rate_limit_filter,
NULL, AP_FTYPE_PROTOCOL + 3);
}
AP_DECLARE_MODULE(ratelimit) = {
STANDARD20_MODULE_STUFF,
NULL,
NULL,
NULL,
NULL,
NULL,
register_hooks
};
