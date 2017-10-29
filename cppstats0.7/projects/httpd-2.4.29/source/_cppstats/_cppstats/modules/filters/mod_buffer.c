#include "apr.h"
#include "apr_strings.h"
#include "apr_buckets.h"
#include "apr_lib.h"
#include "ap_config.h"
#include "util_filter.h"
#include "httpd.h"
#include "http_config.h"
#include "http_log.h"
#include "http_request.h"
static const char bufferFilterName[] = "BUFFER";
module AP_MODULE_DECLARE_DATA buffer_module;
#define DEFAULT_BUFFER_SIZE 128*1024
typedef struct buffer_conf {
apr_off_t size;
int size_set;
} buffer_conf;
typedef struct buffer_ctx {
apr_bucket_brigade *bb;
apr_bucket_brigade *tmp;
buffer_conf *conf;
apr_off_t remaining;
int seen_eos;
} buffer_ctx;
static apr_status_t buffer_out_filter(ap_filter_t *f, apr_bucket_brigade *bb) {
apr_bucket *e;
request_rec *r = f->r;
buffer_ctx *ctx = f->ctx;
apr_status_t rv = APR_SUCCESS;
int move = 0;
if (!ctx) {
if (f->r->main) {
ap_remove_output_filter(f);
return ap_pass_brigade(f->next, bb);
}
ctx = f->ctx = apr_pcalloc(r->pool, sizeof(*ctx));
ctx->bb = apr_brigade_create(r->pool, f->c->bucket_alloc);
ctx->conf = ap_get_module_config(f->r->per_dir_config, &buffer_module);
}
if (APR_BRIGADE_EMPTY(bb)) {
return ap_pass_brigade(f->next, bb);
}
if (APR_BRIGADE_EMPTY(ctx->bb)) {
move = 1;
}
while (APR_SUCCESS == rv && !APR_BRIGADE_EMPTY(bb)) {
const char *data;
apr_off_t len;
apr_size_t size;
e = APR_BRIGADE_FIRST(bb);
if (APR_BUCKET_IS_EOS(e)) {
APR_BUCKET_REMOVE(e);
APR_BRIGADE_INSERT_TAIL(ctx->bb, e);
rv = ap_pass_brigade(f->next, ctx->bb);
continue;
}
if (APR_BUCKET_IS_FLUSH(e)) {
APR_BUCKET_REMOVE(e);
APR_BRIGADE_INSERT_TAIL(ctx->bb, e);
rv = ap_pass_brigade(f->next, ctx->bb);
continue;
}
if (APR_BUCKET_IS_METADATA(e)) {
APR_BUCKET_REMOVE(e);
APR_BRIGADE_INSERT_TAIL(ctx->bb, e);
continue;
}
apr_brigade_length(ctx->bb, 1, &len);
if (len > ctx->conf->size) {
rv = ap_pass_brigade(f->next, ctx->bb);
if (rv) {
continue;
}
}
if (APR_SUCCESS == (rv = apr_bucket_read(e, &data, &size,
APR_BLOCK_READ))) {
if (move && APR_BUCKET_IS_HEAP(e)) {
APR_BUCKET_REMOVE(e);
APR_BRIGADE_INSERT_TAIL(ctx->bb, e);
if (APR_BUCKET_BUFF_SIZE != size) {
move = 0;
}
} else {
apr_brigade_write(ctx->bb, NULL, NULL, data, size);
apr_bucket_delete(e);
}
}
}
return rv;
}
static apr_status_t buffer_in_filter(ap_filter_t *f, apr_bucket_brigade *bb,
ap_input_mode_t mode, apr_read_type_e block, apr_off_t readbytes) {
apr_bucket *e, *after;
apr_status_t rv;
buffer_ctx *ctx = f->ctx;
if (!ap_is_initial_req(f->r)) {
ap_remove_input_filter(f);
return ap_get_brigade(f->next, bb, mode, block, readbytes);
}
if (!ctx) {
ctx = f->ctx = apr_pcalloc(f->r->pool, sizeof(*ctx));
ctx->bb = apr_brigade_create(f->r->pool, f->c->bucket_alloc);
ctx->tmp = apr_brigade_create(f->r->pool, f->c->bucket_alloc);
ctx->conf = ap_get_module_config(f->r->per_dir_config, &buffer_module);
}
if (mode != AP_MODE_READBYTES) {
return ap_get_brigade(f->next, bb, mode, block, readbytes);
}
if (APR_BRIGADE_EMPTY(ctx->bb)) {
int seen_flush = 0;
ctx->remaining = ctx->conf->size;
while (!ctx->seen_eos && !seen_flush && ctx->remaining > 0) {
const char *data;
apr_size_t size = 0;
if (APR_BRIGADE_EMPTY(ctx->tmp)) {
rv = ap_get_brigade(f->next, ctx->tmp, mode, block,
ctx->remaining);
if (rv != APR_SUCCESS || APR_BRIGADE_EMPTY(ctx->tmp)) {
return rv;
}
}
do {
e = APR_BRIGADE_FIRST(ctx->tmp);
if (APR_BUCKET_IS_EOS(e)) {
APR_BUCKET_REMOVE(e);
APR_BRIGADE_INSERT_TAIL(ctx->bb, e);
ctx->seen_eos = 1;
break;
}
if (APR_BUCKET_IS_FLUSH(e)) {
APR_BUCKET_REMOVE(e);
APR_BRIGADE_INSERT_TAIL(ctx->bb, e);
seen_flush = 1;
break;
}
if (APR_BUCKET_IS_METADATA(e)) {
APR_BUCKET_REMOVE(e);
APR_BRIGADE_INSERT_TAIL(ctx->bb, e);
continue;
}
if (APR_SUCCESS == (rv = apr_bucket_read(e, &data, &size,
APR_BLOCK_READ))) {
apr_brigade_write(ctx->bb, NULL, NULL, data, size);
ctx->remaining -= size;
apr_bucket_delete(e);
} else {
return rv;
}
} while (!APR_BRIGADE_EMPTY(ctx->tmp));
}
}
apr_brigade_partition(ctx->bb, readbytes, &after);
e = APR_BRIGADE_FIRST(ctx->bb);
while (e != after) {
if (APR_BUCKET_IS_EOS(e)) {
ap_remove_input_filter(f);
}
APR_BUCKET_REMOVE(e);
APR_BRIGADE_INSERT_TAIL(bb, e);
e = APR_BRIGADE_FIRST(ctx->bb);
}
return APR_SUCCESS;
}
static void *create_buffer_config(apr_pool_t *p, char *dummy) {
buffer_conf *new = (buffer_conf *) apr_pcalloc(p, sizeof(buffer_conf));
new->size_set = 0;
new->size = DEFAULT_BUFFER_SIZE;
return (void *) new;
}
static void *merge_buffer_config(apr_pool_t *p, void *basev, void *addv) {
buffer_conf *new = (buffer_conf *) apr_pcalloc(p, sizeof(buffer_conf));
buffer_conf *add = (buffer_conf *) addv;
buffer_conf *base = (buffer_conf *) basev;
new->size = (add->size_set == 0) ? base->size : add->size;
new->size_set = add->size_set || base->size_set;
return new;
}
static const char *set_buffer_size(cmd_parms *cmd, void *dconf, const char *arg) {
buffer_conf *conf = dconf;
if (APR_SUCCESS != apr_strtoff(&(conf->size), arg, NULL, 10) || conf->size
<= 0) {
return "BufferSize must be a size in bytes, and greater than zero";
}
conf->size_set = 1;
return NULL;
}
static const command_rec buffer_cmds[] = { AP_INIT_TAKE1("BufferSize",
set_buffer_size, NULL, ACCESS_CONF,
"Maximum size of the buffer used by the buffer filter"), { NULL }
};
static void register_hooks(apr_pool_t *p) {
ap_register_output_filter(bufferFilterName, buffer_out_filter, NULL,
AP_FTYPE_CONTENT_SET);
ap_register_input_filter(bufferFilterName, buffer_in_filter, NULL,
AP_FTYPE_CONTENT_SET);
}
AP_DECLARE_MODULE(buffer) = {
STANDARD20_MODULE_STUFF,
create_buffer_config,
merge_buffer_config,
NULL,
NULL,
buffer_cmds,
register_hooks
};