#include "apr.h"
#include "apr_strings.h"
#include "apr_buckets.h"
#include "apr_base64.h"
#include "apr_lib.h"
#include "ap_config.h"
#include "util_filter.h"
#include "httpd.h"
#include "http_config.h"
#include "http_log.h"
#include "http_request.h"
#include "http_protocol.h"
#define DATA_FILTER "DATA"
module AP_MODULE_DECLARE_DATA data_module;
typedef struct data_ctx {
unsigned char overflow[3];
int count;
apr_bucket_brigade *bb;
} data_ctx;
static apr_status_t data_out_filter(ap_filter_t *f, apr_bucket_brigade *bb) {
apr_bucket *e, *ee;
request_rec *r = f->r;
data_ctx *ctx = f->ctx;
apr_status_t rv = APR_SUCCESS;
if (!ctx) {
char *type;
char *charset = NULL;
char *end;
const char *content_length;
if (!ap_is_initial_req(f->r)) {
ap_remove_output_filter(f);
return ap_pass_brigade(f->next, bb);
}
ctx = f->ctx = apr_pcalloc(r->pool, sizeof(*ctx));
ctx->bb = apr_brigade_create(r->pool, f->c->bucket_alloc);
type = apr_pstrdup(r->pool, r->content_type);
if (type) {
charset = strchr(type, ' ');
if (charset) {
*charset++ = 0;
end = strchr(charset, ' ');
if (end) {
*end++ = 0;
}
}
}
apr_brigade_printf(ctx->bb, NULL, NULL, "data:%s%s;base64,",
type ? type : "", charset ? charset : "");
content_length = apr_table_get(r->headers_out, "Content-Length");
if (content_length) {
apr_off_t len, clen;
apr_brigade_length(ctx->bb, 1, &len);
clen = apr_atoi64(content_length);
if (clen >= 0 && clen < APR_INT32_MAX) {
ap_set_content_length(r, len +
apr_base64_encode_len((int)clen) - 1);
} else {
apr_table_unset(r->headers_out, "Content-Length");
}
}
ap_set_content_type(r, "text/plain");
}
if (APR_BRIGADE_EMPTY(bb)) {
return ap_pass_brigade(f->next, bb);
}
while (APR_SUCCESS == rv && !APR_BRIGADE_EMPTY(bb)) {
const char *data;
apr_size_t size;
apr_size_t tail;
apr_size_t len;
char buffer[APR_BUCKET_BUFF_SIZE + 1];
char encoded[((sizeof(ctx->overflow)) / 3) * 4 + 1];
e = APR_BRIGADE_FIRST(bb);
if (APR_BUCKET_IS_EOS(e)) {
if (ctx->count) {
len = apr_base64_encode_binary(encoded, ctx->overflow,
ctx->count);
apr_brigade_write(ctx->bb, NULL, NULL, encoded, len - 1);
ctx->count = 0;
}
APR_BUCKET_REMOVE(e);
APR_BRIGADE_INSERT_TAIL(ctx->bb, e);
ap_remove_output_filter(f);
rv = ap_pass_brigade(f->next, ctx->bb);
if ((APR_SUCCESS == rv) && (!APR_BRIGADE_EMPTY(bb))) {
rv = ap_pass_brigade(f->next, bb);
}
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
apr_brigade_partition(bb, (APR_BUCKET_BUFF_SIZE / 4 * 3), &ee);
if (APR_SUCCESS == (rv = apr_bucket_read(e, &data, &size,
APR_BLOCK_READ))) {
while (size && ctx->count && ctx->count < sizeof(ctx->overflow)) {
ctx->overflow[ctx->count++] = *data++;
size--;
}
if (ctx->count == sizeof(ctx->overflow)) {
len = apr_base64_encode_binary(encoded, ctx->overflow,
sizeof(ctx->overflow));
apr_brigade_write(ctx->bb, NULL, NULL, encoded, len - 1);
ctx->count = 0;
}
tail = size % sizeof(ctx->overflow);
size -= tail;
if (size) {
len = apr_base64_encode_binary(buffer,
(const unsigned char *) data, size);
apr_brigade_write(ctx->bb, NULL, NULL, buffer, len - 1);
}
if (tail) {
memcpy(ctx->overflow, data + size, tail);
ctx->count += tail;
}
apr_bucket_delete(e);
rv = ap_pass_brigade(f->next, ctx->bb);
if (rv) {
continue;
}
}
}
return rv;
}
static const command_rec data_cmds[] = { { NULL } };
static void register_hooks(apr_pool_t *p) {
ap_register_output_filter(DATA_FILTER, data_out_filter, NULL,
AP_FTYPE_RESOURCE);
}
AP_DECLARE_MODULE(data) = { STANDARD20_MODULE_STUFF,
NULL,
NULL,
NULL,
NULL,
data_cmds,
register_hooks
};