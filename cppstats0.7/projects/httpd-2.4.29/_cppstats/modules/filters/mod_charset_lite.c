#include "httpd.h"
#include "http_config.h"
#include "http_core.h"
#include "http_log.h"
#include "http_main.h"
#include "http_protocol.h"
#include "http_request.h"
#include "util_charset.h"
#include "apr_buckets.h"
#include "util_filter.h"
#include "apr_strings.h"
#include "apr_lib.h"
#include "apr_xlate.h"
#define APR_WANT_STRFUNC
#include "apr_want.h"
#define OUTPUT_XLATE_BUF_SIZE (16*1024)
#define INPUT_XLATE_BUF_SIZE (8*1024)
#define XLATE_MIN_BUFF_LEFT 128
#define FATTEST_CHAR 8
typedef enum {
EES_INIT = 0,
EES_LIMIT,
EES_INCOMPLETE_CHAR,
EES_BUCKET_READ,
EES_DOWNSTREAM,
EES_BAD_INPUT
} ees_t;
#define XLATEOUT_FILTER_NAME "XLATEOUT"
#define XLATEIN_FILTER_NAME "XLATEIN"
typedef struct charset_dir_t {
const char *charset_source;
const char *charset_default;
enum {IA_INIT, IA_IMPADD, IA_NOIMPADD} implicit_add;
enum {FX_INIT, FX_FORCE, FX_NOFORCE} force_xlate;
} charset_dir_t;
typedef struct charset_filter_ctx_t {
apr_xlate_t *xlate;
int is_sb;
charset_dir_t *dc;
ees_t ees;
apr_size_t saved;
char buf[FATTEST_CHAR];
int ran;
int noop;
char *tmp;
apr_bucket_brigade *bb;
apr_bucket_brigade *tmpbb;
} charset_filter_ctx_t;
typedef struct charset_req_t {
charset_dir_t *dc;
charset_filter_ctx_t *output_ctx, *input_ctx;
} charset_req_t;
module AP_MODULE_DECLARE_DATA charset_lite_module;
static void *create_charset_dir_conf(apr_pool_t *p,char *dummy) {
charset_dir_t *dc = (charset_dir_t *)apr_pcalloc(p,sizeof(charset_dir_t));
return dc;
}
static void *merge_charset_dir_conf(apr_pool_t *p, void *basev, void *overridesv) {
charset_dir_t *a = (charset_dir_t *)apr_pcalloc (p, sizeof(charset_dir_t));
charset_dir_t *base = (charset_dir_t *)basev,
*over = (charset_dir_t *)overridesv;
a->charset_default =
over->charset_default ? over->charset_default : base->charset_default;
a->charset_source =
over->charset_source ? over->charset_source : base->charset_source;
a->implicit_add =
over->implicit_add != IA_INIT ? over->implicit_add : base->implicit_add;
a->force_xlate=
over->force_xlate != FX_INIT ? over->force_xlate : base->force_xlate;
return a;
}
static const char *add_charset_source(cmd_parms *cmd, void *in_dc,
const char *name) {
charset_dir_t *dc = in_dc;
dc->charset_source = name;
return NULL;
}
static const char *add_charset_default(cmd_parms *cmd, void *in_dc,
const char *name) {
charset_dir_t *dc = in_dc;
dc->charset_default = name;
return NULL;
}
static const char *add_charset_options(cmd_parms *cmd, void *in_dc,
const char *flag) {
charset_dir_t *dc = in_dc;
if (!strcasecmp(flag, "ImplicitAdd")) {
dc->implicit_add = IA_IMPADD;
} else if (!strcasecmp(flag, "NoImplicitAdd")) {
dc->implicit_add = IA_NOIMPADD;
} else if (!strcasecmp(flag, "TranslateAllMimeTypes")) {
dc->force_xlate = FX_FORCE;
} else if (!strcasecmp(flag, "NoTranslateAllMimeTypes")) {
dc->force_xlate = FX_NOFORCE;
} else {
return apr_pstrcat(cmd->temp_pool,
"Invalid CharsetOptions option: ",
flag,
NULL);
}
return NULL;
}
static int find_code_page(request_rec *r) {
charset_dir_t *dc = ap_get_module_config(r->per_dir_config,
&charset_lite_module);
charset_req_t *reqinfo;
charset_filter_ctx_t *input_ctx, *output_ctx;
apr_status_t rv;
ap_log_rerror(APLOG_MARK, APLOG_TRACE3, 0, r,
"uri: %s file: %s method: %d "
"imt: %s flags: %s%s%s %s->%s",
r->uri,
r->filename ? r->filename : "(none)",
r->method_number,
r->content_type ? r->content_type : "(unknown)",
r->main ? "S" : "",
r->prev ? "R" : "",
r->proxyreq ? "P" : "",
dc->charset_source, dc->charset_default);
if (!dc->charset_source || !dc->charset_default) {
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(01448)
"incomplete configuration: src %s, dst %s",
dc->charset_source ? dc->charset_source : "unspecified",
dc->charset_default ? dc->charset_default : "unspecified");
return DECLINED;
}
if (r->proxyreq) {
return DECLINED;
}
if (r->filename
&& (!strncmp(r->filename, "redirect:", 9)
|| !strncmp(r->filename, "gone:", 5)
|| !strncmp(r->filename, "passthrough:", 12)
|| !strncmp(r->filename, "forbidden:", 10))) {
return DECLINED;
}
if (!strcasecmp(dc->charset_source, dc->charset_default)) {
return DECLINED;
}
reqinfo = (charset_req_t *)apr_pcalloc(r->pool,
sizeof(charset_req_t) +
sizeof(charset_filter_ctx_t));
output_ctx = (charset_filter_ctx_t *)(reqinfo + 1);
reqinfo->dc = dc;
output_ctx->dc = dc;
output_ctx->tmpbb = apr_brigade_create(r->pool,
r->connection->bucket_alloc);
ap_set_module_config(r->request_config, &charset_lite_module, reqinfo);
reqinfo->output_ctx = output_ctx;
switch (r->method_number) {
case M_PUT:
case M_POST:
input_ctx = apr_pcalloc(r->pool, sizeof(charset_filter_ctx_t));
input_ctx->bb = apr_brigade_create(r->pool,
r->connection->bucket_alloc);
input_ctx->tmp = apr_palloc(r->pool, INPUT_XLATE_BUF_SIZE);
input_ctx->dc = dc;
reqinfo->input_ctx = input_ctx;
rv = apr_xlate_open(&input_ctx->xlate, dc->charset_source,
dc->charset_default, r->pool);
if (rv != APR_SUCCESS) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, rv, r, APLOGNO(01449)
"can't open translation %s->%s",
dc->charset_default, dc->charset_source);
return HTTP_INTERNAL_SERVER_ERROR;
}
if (apr_xlate_sb_get(input_ctx->xlate, &input_ctx->is_sb) != APR_SUCCESS) {
input_ctx->is_sb = 0;
}
}
return DECLINED;
}
static int configured_in_list(request_rec *r, const char *filter_name,
struct ap_filter_t *filter_list) {
struct ap_filter_t *filter = filter_list;
while (filter) {
if (!strcasecmp(filter_name, filter->frec->name)) {
return 1;
}
filter = filter->next;
}
return 0;
}
static int configured_on_input(request_rec *r, const char *filter_name) {
return configured_in_list(r, filter_name, r->input_filters);
}
static int configured_on_output(request_rec *r, const char *filter_name) {
return configured_in_list(r, filter_name, r->output_filters);
}
static void xlate_insert_filter(request_rec *r) {
charset_req_t *reqinfo = ap_get_module_config(r->request_config,
&charset_lite_module);
charset_dir_t *dc = ap_get_module_config(r->per_dir_config,
&charset_lite_module);
if (dc && (dc->implicit_add == IA_NOIMPADD)) {
ap_log_rerror(APLOG_MARK, APLOG_TRACE6, 0, r,
"xlate output filter not added implicitly because "
"CharsetOptions included 'NoImplicitAdd'");
return;
}
if (reqinfo) {
if (reqinfo->output_ctx && !configured_on_output(r, XLATEOUT_FILTER_NAME)) {
ap_add_output_filter(XLATEOUT_FILTER_NAME, reqinfo->output_ctx, r,
r->connection);
}
ap_log_rerror(APLOG_MARK, APLOG_TRACE3, 0, r,
"xlate output filter not added implicitly because %s",
!reqinfo->output_ctx ?
"no output configuration available" :
"another module added the filter");
if (reqinfo->input_ctx && !configured_on_input(r, XLATEIN_FILTER_NAME)) {
ap_add_input_filter(XLATEIN_FILTER_NAME, reqinfo->input_ctx, r,
r->connection);
}
ap_log_rerror(APLOG_MARK, APLOG_TRACE3, 0, r,
"xlate input filter not added implicitly because %s",
!reqinfo->input_ctx ?
"no input configuration available" :
"another module added the filter");
}
}
static apr_status_t send_bucket_downstream(ap_filter_t *f, apr_bucket *b) {
charset_filter_ctx_t *ctx = f->ctx;
apr_status_t rv;
APR_BRIGADE_INSERT_TAIL(ctx->tmpbb, b);
rv = ap_pass_brigade(f->next, ctx->tmpbb);
if (rv != APR_SUCCESS) {
ctx->ees = EES_DOWNSTREAM;
}
apr_brigade_cleanup(ctx->tmpbb);
return rv;
}
static apr_status_t send_downstream(ap_filter_t *f, const char *tmp, apr_size_t len) {
request_rec *r = f->r;
conn_rec *c = r->connection;
apr_bucket *b;
b = apr_bucket_transient_create(tmp, len, c->bucket_alloc);
return send_bucket_downstream(f, b);
}
static apr_status_t send_eos(ap_filter_t *f) {
request_rec *r = f->r;
conn_rec *c = r->connection;
apr_bucket_brigade *bb;
apr_bucket *b;
charset_filter_ctx_t *ctx = f->ctx;
apr_status_t rv;
bb = apr_brigade_create(r->pool, c->bucket_alloc);
b = apr_bucket_eos_create(c->bucket_alloc);
APR_BRIGADE_INSERT_TAIL(bb, b);
rv = ap_pass_brigade(f->next, bb);
if (rv != APR_SUCCESS) {
ctx->ees = EES_DOWNSTREAM;
}
return rv;
}
static apr_status_t set_aside_partial_char(charset_filter_ctx_t *ctx,
const char *partial,
apr_size_t partial_len) {
apr_status_t rv;
if (sizeof(ctx->buf) > partial_len) {
ctx->saved = partial_len;
memcpy(ctx->buf, partial, partial_len);
rv = APR_SUCCESS;
} else {
rv = APR_INCOMPLETE;
ctx->ees = EES_LIMIT;
}
return rv;
}
static apr_status_t finish_partial_char(charset_filter_ctx_t *ctx,
const char **cur_str,
apr_size_t *cur_len,
char **out_str,
apr_size_t *out_len) {
apr_status_t rv;
apr_size_t tmp_input_len;
do {
ctx->buf[ctx->saved] = **cur_str;
++ctx->saved;
++*cur_str;
--*cur_len;
tmp_input_len = ctx->saved;
rv = apr_xlate_conv_buffer(ctx->xlate,
ctx->buf,
&tmp_input_len,
*out_str,
out_len);
} while (rv == APR_INCOMPLETE && *cur_len);
if (rv == APR_SUCCESS) {
ctx->saved = 0;
} else {
ctx->ees = EES_LIMIT;
}
return rv;
}
static void log_xlate_error(ap_filter_t *f, apr_status_t rv) {
charset_filter_ctx_t *ctx = f->ctx;
const char *msg;
char msgbuf[100];
apr_size_t len;
switch(ctx->ees) {
case EES_LIMIT:
rv = 0;
msg = APLOGNO(02193) "xlate filter - a built-in restriction was encountered";
break;
case EES_BAD_INPUT:
rv = 0;
msg = APLOGNO(02194) "xlate filter - an input character was invalid";
break;
case EES_BUCKET_READ:
rv = 0;
msg = APLOGNO(02195) "xlate filter - bucket read routine failed";
break;
case EES_INCOMPLETE_CHAR:
rv = 0;
strcpy(msgbuf, APLOGNO(02196) "xlate filter - incomplete char at end of input - ");
len = ctx->saved;
if (len > (sizeof(msgbuf) - strlen(msgbuf) - 1) / 2)
len = (sizeof(msgbuf) - strlen(msgbuf) - 1) / 2;
ap_bin2hex(ctx->buf, len, msgbuf + strlen(msgbuf));
msg = msgbuf;
break;
case EES_DOWNSTREAM:
msg = APLOGNO(02197) "xlate filter - an error occurred in a lower filter";
break;
default:
msg = APLOGNO(02198) "xlate filter - returning error";
}
ap_log_rerror(APLOG_MARK, APLOG_ERR, rv, f->r, APLOGNO(02997) "%s", msg);
}
static void chk_filter_chain(ap_filter_t *f) {
ap_filter_t *curf;
charset_filter_ctx_t *curctx, *last_xlate_ctx = NULL,
*ctx = f->ctx;
int output = !strcasecmp(f->frec->name, XLATEOUT_FILTER_NAME);
if (ctx->noop) {
return;
}
curf = output ? f->r->output_filters : f->r->input_filters;
while (curf) {
if (!strcasecmp(curf->frec->name, f->frec->name) &&
curf->ctx) {
curctx = (charset_filter_ctx_t *)curf->ctx;
if (!last_xlate_ctx) {
last_xlate_ctx = curctx;
} else {
if (strcmp(last_xlate_ctx->dc->charset_default,
curctx->dc->charset_source)) {
if (last_xlate_ctx == f->ctx) {
last_xlate_ctx->noop = 1;
if (APLOGrtrace1(f->r)) {
const char *symbol = output ? "->" : "<-";
ap_log_rerror(APLOG_MARK, APLOG_DEBUG,
0, f->r, APLOGNO(01451)
"%s %s - disabling "
"translation %s%s%s; existing "
"translation %s%s%s",
f->r->uri ? "uri" : "file",
f->r->uri ? f->r->uri : f->r->filename,
last_xlate_ctx->dc->charset_source,
symbol,
last_xlate_ctx->dc->charset_default,
curctx->dc->charset_source,
symbol,
curctx->dc->charset_default);
}
} else {
const char *symbol = output ? "->" : "<-";
ap_log_rerror(APLOG_MARK, APLOG_ERR,
0, f->r, APLOGNO(01452)
"chk_filter_chain() - can't disable "
"translation %s%s%s; existing "
"translation %s%s%s",
last_xlate_ctx->dc->charset_source,
symbol,
last_xlate_ctx->dc->charset_default,
curctx->dc->charset_source,
symbol,
curctx->dc->charset_default);
}
break;
}
}
}
curf = curf->next;
}
}
static apr_status_t xlate_brigade(charset_filter_ctx_t *ctx,
apr_bucket_brigade *bb,
char *buffer,
apr_size_t *buffer_avail,
int *hit_eos) {
apr_bucket *b = NULL;
apr_bucket *consumed_bucket;
const char *bucket;
apr_size_t bytes_in_bucket;
apr_size_t bucket_avail;
apr_status_t rv = APR_SUCCESS;
*hit_eos = 0;
bucket_avail = 0;
consumed_bucket = NULL;
while (1) {
if (!bucket_avail) {
if (consumed_bucket) {
apr_bucket_delete(consumed_bucket);
consumed_bucket = NULL;
}
b = APR_BRIGADE_FIRST(bb);
if (b == APR_BRIGADE_SENTINEL(bb) ||
APR_BUCKET_IS_METADATA(b)) {
break;
}
rv = apr_bucket_read(b, &bucket, &bytes_in_bucket, APR_BLOCK_READ);
if (rv != APR_SUCCESS) {
ctx->ees = EES_BUCKET_READ;
break;
}
bucket_avail = bytes_in_bucket;
consumed_bucket = b;
}
if (bucket_avail) {
if (ctx->saved) {
apr_size_t old_buffer_avail = *buffer_avail;
rv = finish_partial_char(ctx,
&bucket, &bucket_avail,
&buffer, buffer_avail);
buffer += old_buffer_avail - *buffer_avail;
} else {
apr_size_t old_buffer_avail = *buffer_avail;
apr_size_t old_bucket_avail = bucket_avail;
rv = apr_xlate_conv_buffer(ctx->xlate,
bucket, &bucket_avail,
buffer,
buffer_avail);
buffer += old_buffer_avail - *buffer_avail;
bucket += old_bucket_avail - bucket_avail;
if (rv == APR_INCOMPLETE) {
rv = set_aside_partial_char(ctx, bucket, bucket_avail);
bucket_avail = 0;
}
}
if (rv != APR_SUCCESS) {
break;
}
if (*buffer_avail < XLATE_MIN_BUFF_LEFT) {
if (bucket_avail) {
apr_bucket_split(b, bytes_in_bucket - bucket_avail);
}
apr_bucket_delete(b);
break;
}
}
}
if (!APR_BRIGADE_EMPTY(bb)) {
b = APR_BRIGADE_FIRST(bb);
if (APR_BUCKET_IS_EOS(b)) {
*hit_eos = 1;
if (ctx->saved) {
rv = APR_INCOMPLETE;
ctx->ees = EES_INCOMPLETE_CHAR;
}
}
}
return rv;
}
static apr_status_t xlate_out_filter(ap_filter_t *f, apr_bucket_brigade *bb) {
charset_req_t *reqinfo = ap_get_module_config(f->r->request_config,
&charset_lite_module);
charset_dir_t *dc = ap_get_module_config(f->r->per_dir_config,
&charset_lite_module);
charset_filter_ctx_t *ctx = f->ctx;
apr_bucket *dptr, *consumed_bucket;
const char *cur_str;
apr_size_t cur_len, cur_avail;
char tmp[OUTPUT_XLATE_BUF_SIZE];
apr_size_t space_avail;
int done;
apr_status_t rv = APR_SUCCESS;
if (!ctx) {
if (reqinfo) {
ctx = f->ctx = reqinfo->output_ctx;
reqinfo->output_ctx = NULL;
}
if (!ctx) {
ctx = f->ctx = apr_pcalloc(f->r->pool, sizeof(charset_filter_ctx_t));
ctx->dc = dc;
ctx->noop = 1;
}
}
if (!ctx->noop && ctx->xlate == NULL) {
const char *mime_type = f->r->content_type;
if (mime_type && (strncasecmp(mime_type, "text/", 5) == 0 ||
#if APR_CHARSET_EBCDIC
strcmp(mime_type, DIR_MAGIC_TYPE) == 0 ||
#endif
strncasecmp(mime_type, "message/", 8) == 0 ||
dc->force_xlate == FX_FORCE)) {
rv = apr_xlate_open(&ctx->xlate,
dc->charset_default, dc->charset_source, f->r->pool);
if (rv != APR_SUCCESS) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, rv, f->r, APLOGNO(01453)
"can't open translation %s->%s",
dc->charset_source, dc->charset_default);
ctx->noop = 1;
} else {
if (apr_xlate_sb_get(ctx->xlate, &ctx->is_sb) != APR_SUCCESS) {
ctx->is_sb = 0;
}
}
} else {
ctx->noop = 1;
if (mime_type) {
ap_log_rerror(APLOG_MARK, APLOG_TRACE6, 0, f->r,
"mime type is %s; no translation selected",
mime_type);
}
}
}
ap_log_rerror(APLOG_MARK, APLOG_TRACE6, 0, f->r,
"xlate_out_filter() - "
"charset_source: %s charset_default: %s",
dc && dc->charset_source ? dc->charset_source : "(none)",
dc && dc->charset_default ? dc->charset_default : "(none)");
if (!ctx->ran) {
chk_filter_chain(f);
ctx->ran = 1;
if (!ctx->noop && !ctx->is_sb) {
apr_table_unset(f->r->headers_out, "Content-Length");
}
}
if (ctx->noop) {
return ap_pass_brigade(f->next, bb);
}
dptr = APR_BRIGADE_FIRST(bb);
done = 0;
cur_len = 0;
space_avail = sizeof(tmp);
consumed_bucket = NULL;
while (!done) {
if (!cur_len) {
if (consumed_bucket) {
apr_bucket_delete(consumed_bucket);
consumed_bucket = NULL;
}
if (dptr == APR_BRIGADE_SENTINEL(bb)) {
break;
}
if (APR_BUCKET_IS_EOS(dptr)) {
cur_len = -1;
if (ctx->saved) {
rv = APR_INCOMPLETE;
ctx->ees = EES_INCOMPLETE_CHAR;
}
break;
}
if (APR_BUCKET_IS_METADATA(dptr)) {
apr_bucket *metadata_bucket;
metadata_bucket = dptr;
dptr = APR_BUCKET_NEXT(dptr);
APR_BUCKET_REMOVE(metadata_bucket);
rv = send_bucket_downstream(f, metadata_bucket);
if (rv != APR_SUCCESS) {
done = 1;
}
continue;
}
rv = apr_bucket_read(dptr, &cur_str, &cur_len, APR_BLOCK_READ);
if (rv != APR_SUCCESS) {
ctx->ees = EES_BUCKET_READ;
break;
}
consumed_bucket = dptr;
dptr = APR_BUCKET_NEXT(dptr);
}
cur_avail = cur_len;
if (cur_len) {
if (ctx->saved) {
char *tmp_tmp;
tmp_tmp = tmp + sizeof(tmp) - space_avail;
rv = finish_partial_char(ctx,
&cur_str, &cur_len,
&tmp_tmp, &space_avail);
} else {
rv = apr_xlate_conv_buffer(ctx->xlate,
cur_str, &cur_avail,
tmp + sizeof(tmp) - space_avail, &space_avail);
cur_str += cur_len - cur_avail;
cur_len = cur_avail;
if (rv == APR_INCOMPLETE) {
rv = set_aside_partial_char(ctx, cur_str, cur_len);
cur_len = 0;
}
}
}
if (rv != APR_SUCCESS) {
done = 1;
}
if (space_avail < XLATE_MIN_BUFF_LEFT) {
rv = send_downstream(f, tmp, sizeof(tmp) - space_avail);
if (rv != APR_SUCCESS) {
done = 1;
}
space_avail = sizeof(tmp);
}
}
if (rv == APR_SUCCESS) {
if (space_avail < sizeof(tmp)) {
rv = send_downstream(f, tmp, sizeof(tmp) - space_avail);
}
}
if (rv == APR_SUCCESS) {
if (cur_len == -1) {
rv = send_eos(f);
}
} else {
log_xlate_error(f, rv);
}
return rv;
}
static apr_status_t xlate_in_filter(ap_filter_t *f, apr_bucket_brigade *bb,
ap_input_mode_t mode, apr_read_type_e block,
apr_off_t readbytes) {
apr_status_t rv;
charset_req_t *reqinfo = ap_get_module_config(f->r->request_config,
&charset_lite_module);
charset_dir_t *dc = ap_get_module_config(f->r->per_dir_config,
&charset_lite_module);
charset_filter_ctx_t *ctx = f->ctx;
apr_size_t buffer_size;
int hit_eos;
if (mode != AP_MODE_READBYTES) {
return ap_get_brigade(f->next, bb, mode, block, readbytes);
}
if (!ctx) {
if (reqinfo) {
ctx = f->ctx = reqinfo->input_ctx;
reqinfo->input_ctx = NULL;
}
if (!ctx) {
ctx = f->ctx = apr_pcalloc(f->r->pool, sizeof(charset_filter_ctx_t));
ctx->dc = dc;
ctx->noop = 1;
}
}
ap_log_rerror(APLOG_MARK, APLOG_TRACE6, 0, f->r,
"xlate_in_filter() - "
"charset_source: %s charset_default: %s",
dc && dc->charset_source ? dc->charset_source : "(none)",
dc && dc->charset_default ? dc->charset_default : "(none)");
if (!ctx->ran) {
chk_filter_chain(f);
ctx->ran = 1;
if (!ctx->noop && !ctx->is_sb
&& apr_table_get(f->r->headers_in, "Content-Length")) {
ap_log_rerror(APLOG_MARK, APLOG_TRACE1, 0, f->r,
"Request body length may change, resulting in "
"misprocessing by some modules or scripts");
}
}
if (ctx->noop) {
return ap_get_brigade(f->next, bb, mode, block, readbytes);
}
if (APR_BRIGADE_EMPTY(ctx->bb)) {
if ((rv = ap_get_brigade(f->next, bb, mode, block,
readbytes)) != APR_SUCCESS) {
return rv;
}
} else {
APR_BRIGADE_PREPEND(bb, ctx->bb);
}
buffer_size = INPUT_XLATE_BUF_SIZE;
rv = xlate_brigade(ctx, bb, ctx->tmp, &buffer_size, &hit_eos);
if (rv == APR_SUCCESS) {
if (!hit_eos) {
APR_BRIGADE_CONCAT(ctx->bb, bb);
}
if (buffer_size < INPUT_XLATE_BUF_SIZE) {
apr_bucket *e;
e = apr_bucket_heap_create(ctx->tmp,
INPUT_XLATE_BUF_SIZE - buffer_size,
NULL, f->r->connection->bucket_alloc);
APR_BRIGADE_INSERT_HEAD(bb, e);
} else {
}
if (!APR_BRIGADE_EMPTY(ctx->bb)) {
apr_bucket *b = APR_BRIGADE_FIRST(ctx->bb);
while (b != APR_BRIGADE_SENTINEL(ctx->bb)
&& APR_BUCKET_IS_METADATA(b)) {
APR_BUCKET_REMOVE(b);
APR_BRIGADE_INSERT_TAIL(bb, b);
b = APR_BRIGADE_FIRST(ctx->bb);
}
}
} else {
log_xlate_error(f, rv);
}
return rv;
}
static const command_rec cmds[] = {
AP_INIT_TAKE1("CharsetSourceEnc",
add_charset_source,
NULL,
OR_FILEINFO,
"source (html,cgi,ssi) file charset"),
AP_INIT_TAKE1("CharsetDefault",
add_charset_default,
NULL,
OR_FILEINFO,
"name of default charset"),
AP_INIT_ITERATE("CharsetOptions",
add_charset_options,
NULL,
OR_FILEINFO,
"valid options: ImplicitAdd, NoImplicitAdd, TranslateAllMimeTypes, "
"NoTranslateAllMimeTypes"),
{NULL}
};
static void charset_register_hooks(apr_pool_t *p) {
ap_hook_fixups(find_code_page, NULL, NULL, APR_HOOK_MIDDLE);
ap_hook_insert_filter(xlate_insert_filter, NULL, NULL, APR_HOOK_REALLY_LAST);
ap_register_output_filter(XLATEOUT_FILTER_NAME, xlate_out_filter, NULL,
AP_FTYPE_RESOURCE);
ap_register_input_filter(XLATEIN_FILTER_NAME, xlate_in_filter, NULL,
AP_FTYPE_RESOURCE);
}
AP_DECLARE_MODULE(charset_lite) = {
STANDARD20_MODULE_STUFF,
create_charset_dir_conf,
merge_charset_dir_conf,
NULL,
NULL,
cmds,
charset_register_hooks
};