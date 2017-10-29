#include "httpd.h"
#include "http_config.h"
#include "http_log.h"
#include "apr_strings.h"
#include "apr_general.h"
#include "util_filter.h"
#include "apr_buckets.h"
#include "http_request.h"
#include "libsed.h"
static const char *sed_filter_name = "Sed";
#define MODSED_OUTBUF_SIZE 8000
#define MAX_TRANSIENT_BUCKETS 50
typedef struct sed_expr_config {
sed_commands_t *sed_cmds;
const char *last_error;
} sed_expr_config;
typedef struct sed_config {
sed_expr_config output;
sed_expr_config input;
} sed_config;
typedef struct sed_filter_ctxt {
sed_eval_t eval;
ap_filter_t *f;
request_rec *r;
apr_bucket_brigade *bb;
apr_bucket_brigade *bbinp;
char *outbuf;
char *curoutbuf;
int bufsize;
apr_pool_t *tpool;
int numbuckets;
} sed_filter_ctxt;
module AP_MODULE_DECLARE_DATA sed_module;
static apr_status_t log_sed_errf(void *data, const char *error) {
request_rec *r = (request_rec *) data;
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(02998) "%s", error);
return APR_SUCCESS;
}
static apr_status_t sed_compile_errf(void *data, const char *error) {
sed_expr_config *sed_cfg = (sed_expr_config *) data;
sed_cfg->last_error = error;
return APR_SUCCESS;
}
static void clear_ctxpool(sed_filter_ctxt* ctx) {
apr_pool_clear(ctx->tpool);
ctx->outbuf = NULL;
ctx->curoutbuf = NULL;
ctx->numbuckets = 0;
}
static void alloc_outbuf(sed_filter_ctxt* ctx) {
ctx->outbuf = apr_palloc(ctx->tpool, ctx->bufsize + 1);
ctx->curoutbuf = ctx->outbuf;
}
static apr_status_t append_bucket(sed_filter_ctxt* ctx, char* buf, int sz) {
apr_status_t status = APR_SUCCESS;
apr_bucket *b;
if (ctx->tpool == ctx->r->pool) {
b = apr_bucket_pool_create(buf, sz, ctx->r->pool,
ctx->r->connection->bucket_alloc);
APR_BRIGADE_INSERT_TAIL(ctx->bb, b);
} else {
b = apr_bucket_transient_create(buf, sz,
ctx->r->connection->bucket_alloc);
APR_BRIGADE_INSERT_TAIL(ctx->bb, b);
ctx->numbuckets++;
if (ctx->numbuckets >= MAX_TRANSIENT_BUCKETS) {
b = apr_bucket_flush_create(ctx->r->connection->bucket_alloc);
APR_BRIGADE_INSERT_TAIL(ctx->bb, b);
status = ap_pass_brigade(ctx->f->next, ctx->bb);
apr_brigade_cleanup(ctx->bb);
clear_ctxpool(ctx);
}
}
return status;
}
static apr_status_t flush_output_buffer(sed_filter_ctxt *ctx) {
int size = ctx->curoutbuf - ctx->outbuf;
char *out;
apr_status_t status = APR_SUCCESS;
if ((ctx->outbuf == NULL) || (size <=0))
return status;
out = apr_pmemdup(ctx->tpool, ctx->outbuf, size);
status = append_bucket(ctx, out, size);
ctx->curoutbuf = ctx->outbuf;
return status;
}
static apr_status_t sed_write_output(void *dummy, char *buf, int sz) {
int remainbytes = 0;
apr_status_t status = APR_SUCCESS;
sed_filter_ctxt *ctx = (sed_filter_ctxt *) dummy;
if (ctx->outbuf == NULL) {
alloc_outbuf(ctx);
}
remainbytes = ctx->bufsize - (ctx->curoutbuf - ctx->outbuf);
if (sz >= remainbytes) {
if (remainbytes > 0) {
memcpy(ctx->curoutbuf, buf, remainbytes);
buf += remainbytes;
sz -= remainbytes;
ctx->curoutbuf += remainbytes;
}
status = append_bucket(ctx, ctx->outbuf, ctx->bufsize);
alloc_outbuf(ctx);
if ((status == APR_SUCCESS) && (sz >= ctx->bufsize)) {
char* newbuf = apr_pmemdup(ctx->tpool, buf, sz);
status = append_bucket(ctx, newbuf, sz);
if (ctx->outbuf == NULL) {
alloc_outbuf(ctx);
}
} else {
memcpy(ctx->curoutbuf, buf, sz);
ctx->curoutbuf += sz;
}
} else {
memcpy(ctx->curoutbuf, buf, sz);
ctx->curoutbuf += sz;
}
return status;
}
static apr_status_t compile_sed_expr(sed_expr_config *sed_cfg,
cmd_parms *cmd,
const char *expr) {
apr_status_t status = APR_SUCCESS;
if (!sed_cfg->sed_cmds) {
sed_commands_t *sed_cmds;
sed_cmds = apr_pcalloc(cmd->pool, sizeof(sed_commands_t));
status = sed_init_commands(sed_cmds, sed_compile_errf, sed_cfg,
cmd->pool);
if (status != APR_SUCCESS) {
sed_destroy_commands(sed_cmds);
return status;
}
sed_cfg->sed_cmds = sed_cmds;
}
status = sed_compile_string(sed_cfg->sed_cmds, expr);
if (status != APR_SUCCESS) {
sed_destroy_commands(sed_cfg->sed_cmds);
sed_cfg->sed_cmds = NULL;
}
return status;
}
static apr_status_t sed_eval_cleanup(void *data) {
sed_eval_t *eval = (sed_eval_t *) data;
sed_destroy_eval(eval);
return APR_SUCCESS;
}
static apr_status_t init_context(ap_filter_t *f, sed_expr_config *sed_cfg, int usetpool) {
apr_status_t status;
sed_filter_ctxt* ctx;
request_rec *r = f->r;
ctx = apr_pcalloc(r->pool, sizeof(sed_filter_ctxt));
ctx->r = r;
ctx->bb = NULL;
ctx->numbuckets = 0;
ctx->f = f;
status = sed_init_eval(&ctx->eval, sed_cfg->sed_cmds, log_sed_errf,
r, &sed_write_output, r->pool);
if (status != APR_SUCCESS) {
return status;
}
apr_pool_cleanup_register(r->pool, &ctx->eval, sed_eval_cleanup,
apr_pool_cleanup_null);
ctx->bufsize = MODSED_OUTBUF_SIZE;
if (usetpool) {
apr_pool_create(&(ctx->tpool), r->pool);
} else {
ctx->tpool = r->pool;
}
alloc_outbuf(ctx);
f->ctx = ctx;
return APR_SUCCESS;
}
static apr_status_t sed_response_filter(ap_filter_t *f,
apr_bucket_brigade *bb) {
apr_bucket *b;
apr_status_t status;
sed_config *cfg = ap_get_module_config(f->r->per_dir_config,
&sed_module);
sed_filter_ctxt *ctx = f->ctx;
sed_expr_config *sed_cfg = &cfg->output;
if ((sed_cfg == NULL) || (sed_cfg->sed_cmds == NULL)) {
ap_remove_output_filter(f);
return ap_pass_brigade(f->next, bb);
}
if (ctx == NULL) {
if (APR_BUCKET_IS_EOS(APR_BRIGADE_FIRST(bb))) {
ap_remove_output_filter(f);
return ap_pass_brigade(f->next, bb);
}
status = init_context(f, sed_cfg, 1);
if (status != APR_SUCCESS)
return status;
ctx = f->ctx;
apr_table_unset(f->r->headers_out, "Content-Length");
}
ctx->bb = apr_brigade_create(f->r->pool, f->c->bucket_alloc);
for (b = APR_BRIGADE_FIRST(bb); b != APR_BRIGADE_SENTINEL(bb);) {
const char *buf = NULL;
apr_size_t bytes = 0;
if (APR_BUCKET_IS_EOS(b)) {
apr_bucket *b1 = APR_BUCKET_NEXT(b);
sed_finalize_eval(&ctx->eval, ctx);
status = flush_output_buffer(ctx);
if (status != APR_SUCCESS) {
clear_ctxpool(ctx);
return status;
}
APR_BUCKET_REMOVE(b);
APR_BRIGADE_INSERT_TAIL(ctx->bb, b);
b = b1;
} else if (APR_BUCKET_IS_FLUSH(b)) {
apr_bucket *b1 = APR_BUCKET_NEXT(b);
APR_BUCKET_REMOVE(b);
status = flush_output_buffer(ctx);
if (status != APR_SUCCESS) {
clear_ctxpool(ctx);
return status;
}
APR_BRIGADE_INSERT_TAIL(ctx->bb, b);
b = b1;
} else if (APR_BUCKET_IS_METADATA(b)) {
b = APR_BUCKET_NEXT(b);
} else if (apr_bucket_read(b, &buf, &bytes, APR_BLOCK_READ)
== APR_SUCCESS) {
apr_bucket *b1 = APR_BUCKET_NEXT(b);
status = sed_eval_buffer(&ctx->eval, buf, bytes, ctx);
if (status != APR_SUCCESS) {
clear_ctxpool(ctx);
return status;
}
APR_BUCKET_REMOVE(b);
apr_bucket_delete(b);
b = b1;
} else {
apr_bucket *b1 = APR_BUCKET_NEXT(b);
APR_BUCKET_REMOVE(b);
b = b1;
}
}
apr_brigade_cleanup(bb);
status = flush_output_buffer(ctx);
if (status != APR_SUCCESS) {
clear_ctxpool(ctx);
return status;
}
if (!APR_BRIGADE_EMPTY(ctx->bb)) {
status = ap_pass_brigade(f->next, ctx->bb);
apr_brigade_cleanup(ctx->bb);
}
clear_ctxpool(ctx);
return status;
}
static apr_status_t sed_request_filter(ap_filter_t *f,
apr_bucket_brigade *bb,
ap_input_mode_t mode,
apr_read_type_e block,
apr_off_t readbytes) {
sed_config *cfg = ap_get_module_config(f->r->per_dir_config,
&sed_module);
sed_filter_ctxt *ctx = f->ctx;
apr_status_t status;
apr_bucket_brigade *bbinp;
sed_expr_config *sed_cfg = &cfg->input;
if (mode != AP_MODE_READBYTES) {
return ap_get_brigade(f->next, bb, mode, block, readbytes);
}
if ((sed_cfg == NULL) || (sed_cfg->sed_cmds == NULL)) {
return ap_get_brigade(f->next, bb, mode, block, readbytes);
}
if (!ctx) {
if (!ap_is_initial_req(f->r)) {
ap_remove_input_filter(f);
return ap_get_brigade(f->next, bb, mode, block, readbytes);
}
status = init_context(f, sed_cfg, 0);
if (status != APR_SUCCESS)
return status;
ctx = f->ctx;
ctx->bb = apr_brigade_create(f->r->pool, f->c->bucket_alloc);
ctx->bbinp = apr_brigade_create(f->r->pool, f->c->bucket_alloc);
}
bbinp = ctx->bbinp;
while (APR_BRIGADE_EMPTY(ctx->bb)) {
apr_bucket *b;
apr_brigade_cleanup(bbinp);
status = ap_get_brigade(f->next, bbinp, mode, block, readbytes);
if (status != APR_SUCCESS) {
return status;
}
for (b = APR_BRIGADE_FIRST(bbinp); b != APR_BRIGADE_SENTINEL(bbinp);
b = APR_BUCKET_NEXT(b)) {
const char *buf = NULL;
apr_size_t bytes;
if (APR_BUCKET_IS_EOS(b)) {
sed_finalize_eval(&ctx->eval, ctx);
flush_output_buffer(ctx);
APR_BUCKET_REMOVE(b);
APR_BRIGADE_INSERT_TAIL(ctx->bb, b);
break;
} else if (APR_BUCKET_IS_FLUSH(b)) {
continue;
}
if (apr_bucket_read(b, &buf, &bytes, APR_BLOCK_READ)
== APR_SUCCESS) {
status = sed_eval_buffer(&ctx->eval, buf, bytes, ctx);
if (status != APR_SUCCESS)
return status;
flush_output_buffer(ctx);
}
}
}
if (!APR_BRIGADE_EMPTY(ctx->bb)) {
apr_bucket *b = NULL;
if (apr_brigade_partition(ctx->bb, readbytes, &b) == APR_INCOMPLETE) {
APR_BRIGADE_CONCAT(bb, ctx->bb);
} else {
APR_BRIGADE_CONCAT(bb, ctx->bb);
apr_brigade_split_ex(bb, b, ctx->bb);
}
}
return APR_SUCCESS;
}
static const char *sed_add_expr(cmd_parms *cmd, void *cfg, const char *arg) {
int offset = (int) (long) cmd->info;
sed_expr_config *sed_cfg =
(sed_expr_config *) (((char *) cfg) + offset);
if (compile_sed_expr(sed_cfg, cmd, arg) != APR_SUCCESS) {
return apr_psprintf(cmd->temp_pool,
"Failed to compile sed expression. %s",
sed_cfg->last_error);
}
return NULL;
}
static void *create_sed_dir_config(apr_pool_t *p, char *s) {
sed_config *cfg = apr_pcalloc(p, sizeof(sed_config));
return cfg;
}
static const command_rec sed_filter_cmds[] = {
AP_INIT_TAKE1("OutputSed", sed_add_expr,
(void *) APR_OFFSETOF(sed_config, output),
ACCESS_CONF,
"Sed regular expression for Response"),
AP_INIT_TAKE1("InputSed", sed_add_expr,
(void *) APR_OFFSETOF(sed_config, input),
ACCESS_CONF,
"Sed regular expression for Request"),
{NULL}
};
static void register_hooks(apr_pool_t *p) {
ap_register_output_filter(sed_filter_name, sed_response_filter, NULL,
AP_FTYPE_RESOURCE);
ap_register_input_filter(sed_filter_name, sed_request_filter, NULL,
AP_FTYPE_RESOURCE);
}
AP_DECLARE_MODULE(sed) = {
STANDARD20_MODULE_STUFF,
create_sed_dir_config,
NULL,
NULL,
NULL,
sed_filter_cmds,
register_hooks
};
