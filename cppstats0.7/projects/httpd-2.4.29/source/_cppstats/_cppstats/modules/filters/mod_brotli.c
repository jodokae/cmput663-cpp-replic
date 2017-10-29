#include "httpd.h"
#include "http_core.h"
#include "http_log.h"
#include "apr_strings.h"
#include <brotli/encode.h>
module AP_MODULE_DECLARE_DATA brotli_module;
typedef enum {
ETAG_MODE_ADDSUFFIX = 0,
ETAG_MODE_NOCHANGE = 1,
ETAG_MODE_REMOVE = 2
} etag_mode_e;
typedef struct brotli_server_config_t {
int quality;
int lgwin;
int lgblock;
etag_mode_e etag_mode;
const char *note_ratio_name;
const char *note_input_name;
const char *note_output_name;
} brotli_server_config_t;
static void *create_server_config(apr_pool_t *p, server_rec *s) {
brotli_server_config_t *conf = apr_pcalloc(p, sizeof(*conf));
conf->quality = 5;
conf->lgwin = 18;
conf->lgblock = 0;
conf->etag_mode = ETAG_MODE_ADDSUFFIX;
return conf;
}
static const char *set_filter_note(cmd_parms *cmd, void *dummy,
const char *arg1, const char *arg2) {
brotli_server_config_t *conf =
ap_get_module_config(cmd->server->module_config, &brotli_module);
if (!arg2) {
conf->note_ratio_name = arg1;
return NULL;
}
if (ap_cstr_casecmp(arg1, "Ratio") == 0) {
conf->note_ratio_name = arg2;
} else if (ap_cstr_casecmp(arg1, "Input") == 0) {
conf->note_input_name = arg2;
} else if (ap_cstr_casecmp(arg1, "Output") == 0) {
conf->note_output_name = arg2;
} else {
return apr_psprintf(cmd->pool, "Unknown BrotliFilterNote type '%s'",
arg1);
}
return NULL;
}
static const char *set_compression_quality(cmd_parms *cmd, void *dummy,
const char *arg) {
brotli_server_config_t *conf =
ap_get_module_config(cmd->server->module_config, &brotli_module);
int val = atoi(arg);
if (val < 0 || val > 11) {
return "BrotliCompressionQuality must be between 0 and 11";
}
conf->quality = val;
return NULL;
}
static const char *set_compression_lgwin(cmd_parms *cmd, void *dummy,
const char *arg) {
brotli_server_config_t *conf =
ap_get_module_config(cmd->server->module_config, &brotli_module);
int val = atoi(arg);
if (val < 10 || val > 24) {
return "BrotliCompressionWindow must be between 10 and 24";
}
conf->lgwin = val;
return NULL;
}
static const char *set_compression_lgblock(cmd_parms *cmd, void *dummy,
const char *arg) {
brotli_server_config_t *conf =
ap_get_module_config(cmd->server->module_config, &brotli_module);
int val = atoi(arg);
if (val < 16 || val > 24) {
return "BrotliCompressionMaxInputBlock must be between 16 and 24";
}
conf->lgblock = val;
return NULL;
}
static const char *set_etag_mode(cmd_parms *cmd, void *dummy,
const char *arg) {
brotli_server_config_t *conf =
ap_get_module_config(cmd->server->module_config, &brotli_module);
if (ap_cstr_casecmp(arg, "AddSuffix") == 0) {
conf->etag_mode = ETAG_MODE_ADDSUFFIX;
} else if (ap_cstr_casecmp(arg, "NoChange") == 0) {
conf->etag_mode = ETAG_MODE_NOCHANGE;
} else if (ap_cstr_casecmp(arg, "Remove") == 0) {
conf->etag_mode = ETAG_MODE_REMOVE;
} else {
return "BrotliAlterETag accepts only 'AddSuffix', 'NoChange' and 'Remove'";
}
return NULL;
}
typedef struct brotli_ctx_t {
BrotliEncoderState *state;
apr_bucket_brigade *bb;
apr_off_t total_in;
apr_off_t total_out;
} brotli_ctx_t;
static void *alloc_func(void *opaque, size_t size) {
return apr_bucket_alloc(size, opaque);
}
static void free_func(void *opaque, void *block) {
if (block) {
apr_bucket_free(block);
}
}
static apr_status_t cleanup_ctx(void *data) {
brotli_ctx_t *ctx = data;
BrotliEncoderDestroyInstance(ctx->state);
ctx->state = NULL;
return APR_SUCCESS;
}
static brotli_ctx_t *create_ctx(int quality,
int lgwin,
int lgblock,
apr_bucket_alloc_t *alloc,
apr_pool_t *pool) {
brotli_ctx_t *ctx = apr_pcalloc(pool, sizeof(*ctx));
ctx->state = BrotliEncoderCreateInstance(alloc_func, free_func, alloc);
BrotliEncoderSetParameter(ctx->state, BROTLI_PARAM_QUALITY, quality);
BrotliEncoderSetParameter(ctx->state, BROTLI_PARAM_LGWIN, lgwin);
BrotliEncoderSetParameter(ctx->state, BROTLI_PARAM_LGBLOCK, lgblock);
apr_pool_cleanup_register(pool, ctx, cleanup_ctx, apr_pool_cleanup_null);
ctx->bb = apr_brigade_create(pool, alloc);
ctx->total_in = 0;
ctx->total_out = 0;
return ctx;
}
static apr_status_t process_chunk(brotli_ctx_t *ctx,
const void *data,
apr_size_t len,
ap_filter_t *f) {
const uint8_t *next_in = data;
apr_size_t avail_in = len;
while (avail_in > 0) {
uint8_t *next_out = NULL;
apr_size_t avail_out = 0;
if (!BrotliEncoderCompressStream(ctx->state,
BROTLI_OPERATION_PROCESS,
&avail_in, &next_in,
&avail_out, &next_out, NULL)) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, f->r, APLOGNO(03459)
"Error while compressing data");
return APR_EGENERAL;
}
if (BrotliEncoderHasMoreOutput(ctx->state)) {
apr_size_t output_len = 0;
const uint8_t *output;
apr_status_t rv;
apr_bucket *b;
output = BrotliEncoderTakeOutput(ctx->state, &output_len);
ctx->total_out += output_len;
b = apr_bucket_transient_create((const char *)output, output_len,
ctx->bb->bucket_alloc);
APR_BRIGADE_INSERT_TAIL(ctx->bb, b);
rv = ap_pass_brigade(f->next, ctx->bb);
apr_brigade_cleanup(ctx->bb);
if (rv != APR_SUCCESS) {
return rv;
}
}
}
ctx->total_in += len;
return APR_SUCCESS;
}
static apr_status_t flush(brotli_ctx_t *ctx,
BrotliEncoderOperation op,
ap_filter_t *f) {
while (1) {
const uint8_t *next_in = NULL;
apr_size_t avail_in = 0;
uint8_t *next_out = NULL;
apr_size_t avail_out = 0;
apr_size_t output_len;
const uint8_t *output;
apr_bucket *b;
if (!BrotliEncoderCompressStream(ctx->state, op,
&avail_in, &next_in,
&avail_out, &next_out, NULL)) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, f->r, APLOGNO(03460)
"Error while compressing data");
return APR_EGENERAL;
}
if (!BrotliEncoderHasMoreOutput(ctx->state)) {
break;
}
output_len = 0;
output = BrotliEncoderTakeOutput(ctx->state, &output_len);
ctx->total_out += output_len;
b = apr_bucket_heap_create((const char *)output, output_len, NULL,
ctx->bb->bucket_alloc);
APR_BRIGADE_INSERT_TAIL(ctx->bb, b);
}
return APR_SUCCESS;
}
static const char *get_content_encoding(request_rec *r) {
const char *encoding;
encoding = apr_table_get(r->headers_out, "Content-Encoding");
if (encoding) {
const char *err_enc;
err_enc = apr_table_get(r->err_headers_out, "Content-Encoding");
if (err_enc) {
encoding = apr_pstrcat(r->pool, encoding, ",", err_enc, NULL);
}
} else {
encoding = apr_table_get(r->err_headers_out, "Content-Encoding");
}
if (r->content_encoding) {
encoding = encoding ? apr_pstrcat(r->pool, encoding, ",",
r->content_encoding, NULL)
: r->content_encoding;
}
return encoding;
}
static apr_status_t compress_filter(ap_filter_t *f, apr_bucket_brigade *bb) {
request_rec *r = f->r;
brotli_ctx_t *ctx = f->ctx;
apr_status_t rv;
brotli_server_config_t *conf;
if (APR_BRIGADE_EMPTY(bb)) {
return APR_SUCCESS;
}
conf = ap_get_module_config(r->server->module_config, &brotli_module);
if (!ctx) {
const char *encoding;
const char *token;
const char *accepts;
if (r->main || r->status == HTTP_NO_CONTENT
|| apr_table_get(r->subprocess_env, "no-brotli")
|| apr_table_get(r->headers_out, "Content-Range")) {
ap_remove_output_filter(f);
return ap_pass_brigade(f->next, bb);
}
encoding = get_content_encoding(r);
if (encoding) {
const char *tmp = encoding;
token = ap_get_token(r->pool, &tmp, 0);
while (token && *token) {
if (strcmp(token, "identity") != 0 &&
strcmp(token, "7bit") != 0 &&
strcmp(token, "8bit") != 0 &&
strcmp(token, "binary") != 0) {
ap_remove_output_filter(f);
return ap_pass_brigade(f->next, bb);
}
if (*tmp) {
++tmp;
}
token = (*tmp) ? ap_get_token(r->pool, &tmp, 0) : NULL;
}
}
apr_table_mergen(r->headers_out, "Vary", "Accept-Encoding");
accepts = apr_table_get(r->headers_in, "Accept-Encoding");
if (!accepts) {
ap_remove_output_filter(f);
return ap_pass_brigade(f->next, bb);
}
token = ap_get_token(r->pool, &accepts, 0);
while (token && token[0] && ap_cstr_casecmp(token, "br") != 0) {
while (*accepts == ';') {
++accepts;
ap_get_token(r->pool, &accepts, 1);
}
if (*accepts == ',') {
++accepts;
}
token = (*accepts) ? ap_get_token(r->pool, &accepts, 0) : NULL;
}
if (!token || token[0] == '\0') {
ap_remove_output_filter(f);
return ap_pass_brigade(f->next, bb);
}
if (!encoding || ap_cstr_casecmp(encoding, "identity") == 0) {
apr_table_setn(r->headers_out, "Content-Encoding", "br");
} else {
apr_table_mergen(r->headers_out, "Content-Encoding", "br");
}
if (r->content_encoding) {
r->content_encoding = apr_table_get(r->headers_out,
"Content-Encoding");
}
apr_table_unset(r->headers_out, "Content-Length");
apr_table_unset(r->headers_out, "Content-MD5");
if (conf->etag_mode == ETAG_MODE_REMOVE) {
apr_table_unset(r->headers_out, "ETag");
} else if (conf->etag_mode == ETAG_MODE_ADDSUFFIX) {
const char *etag = apr_table_get(r->headers_out, "ETag");
if (etag) {
apr_size_t len = strlen(etag);
if (len > 2 && etag[len - 1] == '"') {
etag = apr_pstrmemdup(r->pool, etag, len - 1);
etag = apr_pstrcat(r->pool, etag, "-br\"", NULL);
apr_table_setn(r->headers_out, "ETag", etag);
}
}
}
if (r->status == HTTP_NOT_MODIFIED) {
ap_remove_output_filter(f);
return ap_pass_brigade(f->next, bb);
}
ctx = create_ctx(conf->quality, conf->lgwin, conf->lgblock,
f->c->bucket_alloc, r->pool);
f->ctx = ctx;
}
while (!APR_BRIGADE_EMPTY(bb)) {
apr_bucket *e = APR_BRIGADE_FIRST(bb);
if (r->header_only && r->bytes_sent) {
ap_remove_output_filter(f);
return ap_pass_brigade(f->next, bb);
}
if (APR_BUCKET_IS_EOS(e)) {
rv = flush(ctx, BROTLI_OPERATION_FINISH, f);
if (rv != APR_SUCCESS) {
return rv;
}
if (conf->note_input_name) {
apr_table_setn(r->notes, conf->note_input_name,
apr_off_t_toa(r->pool, ctx->total_in));
}
if (conf->note_output_name) {
apr_table_setn(r->notes, conf->note_output_name,
apr_off_t_toa(r->pool, ctx->total_out));
}
if (conf->note_ratio_name) {
if (ctx->total_in > 0) {
int ratio = (int) (ctx->total_out * 100 / ctx->total_in);
apr_table_setn(r->notes, conf->note_ratio_name,
apr_itoa(r->pool, ratio));
} else {
apr_table_setn(r->notes, conf->note_ratio_name, "-");
}
}
APR_BUCKET_REMOVE(e);
APR_BRIGADE_INSERT_TAIL(ctx->bb, e);
rv = ap_pass_brigade(f->next, ctx->bb);
apr_brigade_cleanup(ctx->bb);
apr_pool_cleanup_run(r->pool, ctx, cleanup_ctx);
return rv;
} else if (APR_BUCKET_IS_FLUSH(e)) {
rv = flush(ctx, BROTLI_OPERATION_FLUSH, f);
if (rv != APR_SUCCESS) {
return rv;
}
APR_BUCKET_REMOVE(e);
APR_BRIGADE_INSERT_TAIL(ctx->bb, e);
rv = ap_pass_brigade(f->next, ctx->bb);
apr_brigade_cleanup(ctx->bb);
if (rv != APR_SUCCESS) {
return rv;
}
} else if (APR_BUCKET_IS_METADATA(e)) {
APR_BUCKET_REMOVE(e);
APR_BRIGADE_INSERT_TAIL(ctx->bb, e);
} else {
const char *data;
apr_size_t len;
rv = apr_bucket_read(e, &data, &len, APR_BLOCK_READ);
if (rv != APR_SUCCESS) {
return rv;
}
rv = process_chunk(ctx, data, len, f);
if (rv != APR_SUCCESS) {
return rv;
}
apr_bucket_delete(e);
}
}
return APR_SUCCESS;
}
static void register_hooks(apr_pool_t *p) {
ap_register_output_filter("BROTLI_COMPRESS", compress_filter, NULL,
AP_FTYPE_CONTENT_SET);
}
static const command_rec cmds[] = {
AP_INIT_TAKE12("BrotliFilterNote", set_filter_note,
NULL, RSRC_CONF,
"Set a note to report on compression ratio"),
AP_INIT_TAKE1("BrotliCompressionQuality", set_compression_quality,
NULL, RSRC_CONF,
"Compression quality between 0 and 11 (higher quality means "
"slower compression)"),
AP_INIT_TAKE1("BrotliCompressionWindow", set_compression_lgwin,
NULL, RSRC_CONF,
"Sliding window size between 10 and 24 (larger windows can "
"improve compression, but require more memory)"),
AP_INIT_TAKE1("BrotliCompressionMaxInputBlock", set_compression_lgblock,
NULL, RSRC_CONF,
"Maximum input block size between 16 and 24 (larger block "
"sizes require more memory)"),
AP_INIT_TAKE1("BrotliAlterETag", set_etag_mode,
NULL, RSRC_CONF,
"Set how mod_brotli should modify ETag response headers: "
"'AddSuffix' (default), 'NoChange', 'Remove'"),
{NULL}
};
AP_DECLARE_MODULE(brotli) = {
STANDARD20_MODULE_STUFF,
NULL,
NULL,
create_server_config,
NULL,
cmds,
register_hooks
};
