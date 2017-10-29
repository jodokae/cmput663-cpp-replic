#include <assert.h>
#include <stdio.h>
#include <apr_date.h>
#include <apr_lib.h>
#include <apr_strings.h>
#include <httpd.h>
#include <http_core.h>
#include <http_log.h>
#include <http_connection.h>
#include <http_protocol.h>
#include <http_request.h>
#include <util_time.h>
#include "h2_private.h"
#include "h2_headers.h"
#include "h2_from_h1.h"
#include "h2_task.h"
#include "h2_util.h"
static int uniq_field_values(void *d, const char *key, const char *val) {
apr_array_header_t *values;
char *start;
char *e;
char **strpp;
int i;
(void)key;
values = (apr_array_header_t *)d;
e = apr_pstrdup(values->pool, val);
do {
while (*e == ',' || apr_isspace(*e)) {
++e;
}
if (*e == '\0') {
break;
}
start = e;
while (*e != '\0' && *e != ',' && !apr_isspace(*e)) {
++e;
}
if (*e != '\0') {
*e++ = '\0';
}
for (i = 0, strpp = (char **) values->elts; i < values->nelts;
++i, ++strpp) {
if (*strpp && apr_strnatcasecmp(*strpp, start) == 0) {
break;
}
}
if (i == values->nelts) {
*(char **)apr_array_push(values) = start;
}
} while (*e != '\0');
return 1;
}
static void fix_vary(request_rec *r) {
apr_array_header_t *varies;
varies = apr_array_make(r->pool, 5, sizeof(char *));
apr_table_do(uniq_field_values, varies, r->headers_out, "Vary", NULL);
if (varies->nelts > 0) {
apr_table_setn(r->headers_out, "Vary",
apr_array_pstrcat(r->pool, varies, ','));
}
}
static void set_basic_http_header(apr_table_t *headers, request_rec *r,
apr_pool_t *pool) {
char *date = NULL;
const char *proxy_date = NULL;
const char *server = NULL;
const char *us = ap_get_server_banner();
if (r && r->proxyreq != PROXYREQ_NONE) {
proxy_date = apr_table_get(r->headers_out, "Date");
if (!proxy_date) {
date = apr_palloc(pool, APR_RFC822_DATE_LEN);
ap_recent_rfc822_date(date, r->request_time);
}
server = apr_table_get(r->headers_out, "Server");
} else {
date = apr_palloc(pool, APR_RFC822_DATE_LEN);
ap_recent_rfc822_date(date, r? r->request_time : apr_time_now());
}
apr_table_setn(headers, "Date", proxy_date ? proxy_date : date );
if (r) {
apr_table_unset(r->headers_out, "Date");
}
if (!server && *us) {
server = us;
}
if (server) {
apr_table_setn(headers, "Server", server);
if (r) {
apr_table_unset(r->headers_out, "Server");
}
}
}
static int copy_header(void *ctx, const char *name, const char *value) {
apr_table_t *headers = ctx;
apr_table_addn(headers, name, value);
return 1;
}
static h2_headers *create_response(h2_task *task, request_rec *r) {
const char *clheader;
const char *ctype;
apr_table_t *headers;
if (!apr_is_empty_table(r->err_headers_out)) {
r->headers_out = apr_table_overlay(r->pool, r->err_headers_out,
r->headers_out);
apr_table_clear(r->err_headers_out);
}
if (apr_table_get(r->subprocess_env, "force-no-vary") != NULL) {
apr_table_unset(r->headers_out, "Vary");
r->proto_num = HTTP_VERSION(1,0);
apr_table_setn(r->subprocess_env, "force-response-1.0", "1");
} else {
fix_vary(r);
}
if (apr_table_get(r->notes, "no-etag") != NULL) {
apr_table_unset(r->headers_out, "ETag");
}
ap_set_keepalive(r);
if (r->chunked) {
apr_table_unset(r->headers_out, "Content-Length");
}
ctype = ap_make_content_type(r, r->content_type);
if (ctype) {
apr_table_setn(r->headers_out, "Content-Type", ctype);
}
if (r->content_encoding) {
apr_table_setn(r->headers_out, "Content-Encoding",
r->content_encoding);
}
if (!apr_is_empty_array(r->content_languages)) {
unsigned int i;
char *token;
char **languages = (char **)(r->content_languages->elts);
const char *field = apr_table_get(r->headers_out, "Content-Language");
while (field && (token = ap_get_list_item(r->pool, &field)) != NULL) {
for (i = 0; i < r->content_languages->nelts; ++i) {
if (!apr_strnatcasecmp(token, languages[i]))
break;
}
if (i == r->content_languages->nelts) {
*((char **) apr_array_push(r->content_languages)) = token;
}
}
field = apr_array_pstrcat(r->pool, r->content_languages, ',');
apr_table_setn(r->headers_out, "Content-Language", field);
}
if (r->no_cache && !apr_table_get(r->headers_out, "Expires")) {
char *date = apr_palloc(r->pool, APR_RFC822_DATE_LEN);
ap_recent_rfc822_date(date, r->request_time);
apr_table_addn(r->headers_out, "Expires", date);
}
if (r->header_only
&& (clheader = apr_table_get(r->headers_out, "Content-Length"))
&& !strcmp(clheader, "0")) {
apr_table_unset(r->headers_out, "Content-Length");
}
headers = apr_table_make(r->pool, 10);
set_basic_http_header(headers, r, r->pool);
if (r->status == HTTP_NOT_MODIFIED) {
apr_table_do(copy_header, headers, r->headers_out,
"ETag",
"Content-Location",
"Expires",
"Cache-Control",
"Vary",
"Warning",
"WWW-Authenticate",
"Proxy-Authenticate",
"Set-Cookie",
"Set-Cookie2",
NULL);
} else {
apr_table_do(copy_header, headers, r->headers_out, NULL);
}
return h2_headers_rcreate(r, r->status, headers, r->pool);
}
typedef enum {
H2_RP_STATUS_LINE,
H2_RP_HEADER_LINE,
H2_RP_DONE
} h2_rp_state_t;
typedef struct h2_response_parser {
h2_rp_state_t state;
h2_task *task;
int http_status;
apr_array_header_t *hlines;
apr_bucket_brigade *tmp;
} h2_response_parser;
static apr_status_t parse_header(h2_response_parser *parser, char *line) {
const char *hline;
if (line[0] == ' ' || line[0] == '\t') {
char **plast;
while (line[0] == ' ' || line[0] == '\t') {
++line;
}
plast = apr_array_pop(parser->hlines);
if (plast == NULL) {
return APR_EINVAL;
}
hline = apr_psprintf(parser->task->pool, "%s %s", *plast, line);
} else {
hline = apr_pstrdup(parser->task->pool, line);
}
APR_ARRAY_PUSH(parser->hlines, const char*) = hline;
return APR_SUCCESS;
}
static apr_status_t get_line(h2_response_parser *parser, apr_bucket_brigade *bb,
char *line, apr_size_t len) {
h2_task *task = parser->task;
apr_status_t status;
if (!parser->tmp) {
parser->tmp = apr_brigade_create(task->pool, task->c->bucket_alloc);
}
status = apr_brigade_split_line(parser->tmp, bb, APR_BLOCK_READ,
HUGE_STRING_LEN);
if (status == APR_SUCCESS) {
--len;
status = apr_brigade_flatten(parser->tmp, line, &len);
if (status == APR_SUCCESS) {
line[len] = '\0';
if (len >= 2 && !strcmp(H2_CRLF, line + len - 2)) {
len -= 2;
line[len] = '\0';
apr_brigade_cleanup(parser->tmp);
ap_log_cerror(APLOG_MARK, APLOG_DEBUG, 0, task->c,
"h2_task(%s): read response line: %s",
task->id, line);
} else {
ap_log_cerror(APLOG_MARK, APLOG_DEBUG, 0, task->c,
"h2_task(%s): read response, incomplete line: %s",
task->id, line);
return APR_EAGAIN;
}
}
}
apr_brigade_cleanup(parser->tmp);
return status;
}
static apr_table_t *make_table(h2_response_parser *parser) {
h2_task *task = parser->task;
apr_array_header_t *hlines = parser->hlines;
if (hlines) {
apr_table_t *headers = apr_table_make(task->pool, hlines->nelts);
int i;
for (i = 0; i < hlines->nelts; ++i) {
char *hline = ((char **)hlines->elts)[i];
char *sep = ap_strchr(hline, ':');
if (!sep) {
ap_log_cerror(APLOG_MARK, APLOG_WARNING, APR_EINVAL, task->c,
APLOGNO(02955) "h2_task(%s): invalid header[%d] '%s'",
task->id, i, (char*)hline);
return NULL;
}
(*sep++) = '\0';
while (*sep == ' ' || *sep == '\t') {
++sep;
}
if (!h2_util_ignore_header(hline)) {
apr_table_merge(headers, hline, sep);
}
}
return headers;
} else {
return apr_table_make(task->pool, 0);
}
}
static apr_status_t pass_response(h2_task *task, ap_filter_t *f,
h2_response_parser *parser) {
apr_bucket *b;
apr_status_t status;
h2_headers *response = h2_headers_create(parser->http_status,
make_table(parser),
NULL, task->pool);
apr_brigade_cleanup(parser->tmp);
b = h2_bucket_headers_create(task->c->bucket_alloc, response);
APR_BRIGADE_INSERT_TAIL(parser->tmp, b);
b = apr_bucket_flush_create(task->c->bucket_alloc);
APR_BRIGADE_INSERT_TAIL(parser->tmp, b);
status = ap_pass_brigade(f->next, parser->tmp);
apr_brigade_cleanup(parser->tmp);
parser->state = H2_RP_STATUS_LINE;
apr_array_clear(parser->hlines);
if (response->status >= 200) {
task->output.sent_response = 1;
}
ap_log_cerror(APLOG_MARK, APLOG_DEBUG, 0, task->c,
APLOGNO(03197) "h2_task(%s): passed response %d",
task->id, response->status);
return status;
}
static apr_status_t parse_status(h2_task *task, char *line) {
h2_response_parser *parser = task->output.rparser;
int sindex = (apr_date_checkmask(line, "HTTP/#.####*")? 9 :
(apr_date_checkmask(line, "HTTP/####*")? 7 : 0));
if (sindex > 0) {
int k = sindex + 3;
char keepchar = line[k];
line[k] = '\0';
parser->http_status = atoi(&line[sindex]);
line[k] = keepchar;
parser->state = H2_RP_HEADER_LINE;
return APR_SUCCESS;
}
ap_log_cerror(APLOG_MARK, APLOG_DEBUG, 0, task->c, APLOGNO(03467)
"h2_task(%s): unable to parse status line: %s",
task->id, line);
return APR_EINVAL;
}
apr_status_t h2_from_h1_parse_response(h2_task *task, ap_filter_t *f,
apr_bucket_brigade *bb) {
h2_response_parser *parser = task->output.rparser;
char line[HUGE_STRING_LEN];
apr_status_t status = APR_SUCCESS;
if (!parser) {
parser = apr_pcalloc(task->pool, sizeof(*parser));
parser->task = task;
parser->state = H2_RP_STATUS_LINE;
parser->hlines = apr_array_make(task->pool, 10, sizeof(char *));
task->output.rparser = parser;
}
while (!APR_BRIGADE_EMPTY(bb) && status == APR_SUCCESS) {
switch (parser->state) {
case H2_RP_STATUS_LINE:
case H2_RP_HEADER_LINE:
status = get_line(parser, bb, line, sizeof(line));
if (status == APR_EAGAIN) {
return APR_SUCCESS;
} else if (status != APR_SUCCESS) {
return status;
}
if (parser->state == H2_RP_STATUS_LINE) {
status = parse_status(task, line);
} else if (line[0] == '\0') {
ap_log_cerror(APLOG_MARK, APLOG_TRACE1, 0, f->c,
"h2_task(%s): end of response", task->id);
return pass_response(task, f, parser);
} else {
ap_log_cerror(APLOG_MARK, APLOG_TRACE1, 0, f->c,
"h2_task(%s): response header %s", task->id, line);
status = parse_header(parser, line);
}
break;
default:
return status;
}
}
return status;
}
apr_status_t h2_filter_headers_out(ap_filter_t *f, apr_bucket_brigade *bb) {
h2_task *task = f->ctx;
request_rec *r = f->r;
apr_bucket *b, *bresp, *body_bucket = NULL, *next;
ap_bucket_error *eb = NULL;
h2_headers *response = NULL;
int headers_passing = 0;
ap_log_cerror(APLOG_MARK, APLOG_TRACE1, 0, f->c,
"h2_task(%s): output_filter called", task->id);
if (!task->output.sent_response && !f->c->aborted) {
for (b = APR_BRIGADE_FIRST(bb);
b != APR_BRIGADE_SENTINEL(bb);
b = APR_BUCKET_NEXT(b)) {
if (AP_BUCKET_IS_ERROR(b) && !eb) {
eb = b->data;
} else if (AP_BUCKET_IS_EOC(b)) {
ap_remove_output_filter(f);
ap_log_cerror(APLOG_MARK, APLOG_TRACE2, 0, f->c,
"h2_task(%s): eoc bucket passed", task->id);
return ap_pass_brigade(f->next, bb);
} else if (H2_BUCKET_IS_HEADERS(b)) {
headers_passing = 1;
} else if (!APR_BUCKET_IS_FLUSH(b)) {
body_bucket = b;
break;
}
}
if (eb) {
int st = eb->status;
ap_log_cerror(APLOG_MARK, APLOG_DEBUG, 0, f->c, APLOGNO(03047)
"h2_task(%s): err bucket status=%d", task->id, st);
apr_brigade_cleanup(bb);
ap_die(st, r);
return AP_FILTER_ERROR;
}
if (body_bucket || !headers_passing) {
response = create_response(task, r);
if (response == NULL) {
ap_log_cerror(APLOG_MARK, APLOG_NOTICE, 0, f->c, APLOGNO(03048)
"h2_task(%s): unable to create response", task->id);
return APR_ENOMEM;
}
bresp = h2_bucket_headers_create(f->c->bucket_alloc, response);
if (body_bucket) {
APR_BUCKET_INSERT_BEFORE(body_bucket, bresp);
} else {
APR_BRIGADE_INSERT_HEAD(bb, bresp);
}
task->output.sent_response = 1;
r->sent_bodyct = 1;
}
}
if (r->header_only) {
ap_log_cerror(APLOG_MARK, APLOG_TRACE1, 0, f->c,
"h2_task(%s): header_only, cleanup output brigade",
task->id);
b = body_bucket? body_bucket : APR_BRIGADE_FIRST(bb);
while (b != APR_BRIGADE_SENTINEL(bb)) {
next = APR_BUCKET_NEXT(b);
if (APR_BUCKET_IS_EOS(b) || AP_BUCKET_IS_EOR(b)) {
break;
}
APR_BUCKET_REMOVE(b);
apr_bucket_destroy(b);
b = next;
}
} else if (task->output.sent_response) {
ap_remove_output_filter(f);
}
return ap_pass_brigade(f->next, bb);
}
static void make_chunk(h2_task *task, apr_bucket_brigade *bb,
apr_bucket *first, apr_off_t chunk_len,
apr_bucket *tail) {
char buffer[128];
apr_bucket *c;
int len;
len = apr_snprintf(buffer, H2_ALEN(buffer),
"%"APR_UINT64_T_HEX_FMT"\r\n", (apr_uint64_t)chunk_len);
c = apr_bucket_heap_create(buffer, len, NULL, bb->bucket_alloc);
APR_BUCKET_INSERT_BEFORE(first, c);
c = apr_bucket_heap_create("\r\n", 2, NULL, bb->bucket_alloc);
if (tail) {
APR_BUCKET_INSERT_BEFORE(tail, c);
} else {
APR_BRIGADE_INSERT_TAIL(bb, c);
}
task->input.chunked_total += chunk_len;
ap_log_cerror(APLOG_MARK, APLOG_TRACE2, 0, task->c,
"h2_task(%s): added chunk %ld, total %ld",
task->id, (long)chunk_len, (long)task->input.chunked_total);
}
static int ser_header(void *ctx, const char *name, const char *value) {
apr_bucket_brigade *bb = ctx;
apr_brigade_printf(bb, NULL, NULL, "%s: %s\r\n", name, value);
return 1;
}
static apr_status_t read_and_chunk(ap_filter_t *f, h2_task *task,
apr_read_type_e block) {
request_rec *r = f->r;
apr_status_t status = APR_SUCCESS;
apr_bucket_brigade *bb = task->input.bbchunk;
if (!bb) {
bb = apr_brigade_create(r->pool, f->c->bucket_alloc);
task->input.bbchunk = bb;
}
if (APR_BRIGADE_EMPTY(bb)) {
apr_bucket *b, *next, *first_data = NULL;
apr_bucket_brigade *tmp;
apr_off_t bblen = 0;
status = ap_get_brigade(f->next, bb,
AP_MODE_READBYTES, block, 32*1024);
if (status == APR_EOF) {
if (!task->input.eos) {
status = apr_brigade_puts(bb, NULL, NULL, "0\r\n\r\n");
task->input.eos = 1;
return APR_SUCCESS;
}
ap_remove_input_filter(f);
return status;
} else if (status != APR_SUCCESS) {
return status;
}
for (b = APR_BRIGADE_FIRST(bb);
b != APR_BRIGADE_SENTINEL(bb) && !task->input.eos;
b = next) {
next = APR_BUCKET_NEXT(b);
if (APR_BUCKET_IS_METADATA(b)) {
if (first_data) {
make_chunk(task, bb, first_data, bblen, b);
first_data = NULL;
}
if (H2_BUCKET_IS_HEADERS(b)) {
h2_headers *headers = h2_bucket_headers_get(b);
ap_assert(headers);
ap_log_rerror(APLOG_MARK, APLOG_TRACE2, 0, r,
"h2_task(%s): receiving trailers", task->id);
tmp = apr_brigade_split_ex(bb, b, NULL);
if (!apr_is_empty_table(headers->headers)) {
status = apr_brigade_puts(bb, NULL, NULL, "0\r\n");
apr_table_do(ser_header, bb, headers->headers, NULL);
status = apr_brigade_puts(bb, NULL, NULL, "\r\n");
} else {
status = apr_brigade_puts(bb, NULL, NULL, "0\r\n\r\n");
}
r->trailers_in = apr_table_clone(r->pool, headers->headers);
APR_BUCKET_REMOVE(b);
apr_bucket_destroy(b);
APR_BRIGADE_CONCAT(bb, tmp);
apr_brigade_destroy(tmp);
task->input.eos = 1;
} else if (APR_BUCKET_IS_EOS(b)) {
tmp = apr_brigade_split_ex(bb, b, NULL);
status = apr_brigade_puts(bb, NULL, NULL, "0\r\n\r\n");
APR_BRIGADE_CONCAT(bb, tmp);
apr_brigade_destroy(tmp);
task->input.eos = 1;
}
} else if (b->length == 0) {
APR_BUCKET_REMOVE(b);
apr_bucket_destroy(b);
} else {
if (!first_data) {
first_data = b;
bblen = 0;
}
bblen += b->length;
}
}
if (first_data) {
make_chunk(task, bb, first_data, bblen, NULL);
}
}
return status;
}
apr_status_t h2_filter_request_in(ap_filter_t* f,
apr_bucket_brigade* bb,
ap_input_mode_t mode,
apr_read_type_e block,
apr_off_t readbytes) {
h2_task *task = f->ctx;
request_rec *r = f->r;
apr_status_t status = APR_SUCCESS;
apr_bucket *b, *next;
core_server_config *conf =
(core_server_config *) ap_get_module_config(r->server->module_config,
&core_module);
ap_log_rerror(APLOG_MARK, APLOG_TRACE2, 0, f->r,
"h2_task(%s): request filter, exp=%d", task->id, r->expecting_100);
if (!task->request->chunked) {
status = ap_get_brigade(f->next, bb, mode, block, readbytes);
for (b = APR_BRIGADE_FIRST(bb);
b != APR_BRIGADE_SENTINEL(bb); b = next) {
next = APR_BUCKET_NEXT(b);
if (H2_BUCKET_IS_HEADERS(b)) {
h2_headers *headers = h2_bucket_headers_get(b);
ap_assert(headers);
ap_log_rerror(APLOG_MARK, APLOG_TRACE2, 0, r,
"h2_task(%s): receiving trailers", task->id);
r->trailers_in = headers->headers;
if (conf && conf->merge_trailers == AP_MERGE_TRAILERS_ENABLE) {
r->headers_in = apr_table_overlay(r->pool, r->headers_in,
r->trailers_in);
}
APR_BUCKET_REMOVE(b);
apr_bucket_destroy(b);
ap_remove_input_filter(f);
break;
}
}
return status;
}
if ((status = read_and_chunk(f, task, block)) != APR_SUCCESS) {
return status;
}
if (mode == AP_MODE_EXHAUSTIVE) {
APR_BRIGADE_CONCAT(bb, task->input.bbchunk);
} else if (mode == AP_MODE_READBYTES) {
status = h2_brigade_concat_length(bb, task->input.bbchunk, readbytes);
} else if (mode == AP_MODE_SPECULATIVE) {
status = h2_brigade_copy_length(bb, task->input.bbchunk, readbytes);
} else if (mode == AP_MODE_GETLINE) {
status = apr_brigade_split_line(bb, task->input.bbchunk, block,
HUGE_STRING_LEN);
if (APLOGctrace1(f->c)) {
char buffer[1024];
apr_size_t len = sizeof(buffer)-1;
apr_brigade_flatten(bb, buffer, &len);
buffer[len] = 0;
ap_log_cerror(APLOG_MARK, APLOG_TRACE1, status, f->c,
"h2_task(%s): getline: %s",
task->id, buffer);
}
} else {
ap_log_cerror(APLOG_MARK, APLOG_ERR, APR_ENOTIMPL, f->c,
APLOGNO(02942)
"h2_task, unsupported READ mode %d", mode);
status = APR_ENOTIMPL;
}
h2_util_bb_log(f->c, task->stream_id, APLOG_TRACE2, "forwarding input", bb);
return status;
}
apr_status_t h2_filter_trailers_out(ap_filter_t *f, apr_bucket_brigade *bb) {
h2_task *task = f->ctx;
request_rec *r = f->r;
apr_bucket *b, *e;
if (task && r) {
for (b = APR_BRIGADE_FIRST(bb);
b != APR_BRIGADE_SENTINEL(bb);
b = APR_BUCKET_NEXT(b)) {
if ((APR_BUCKET_IS_EOS(b) || AP_BUCKET_IS_EOR(b))
&& r->trailers_out && !apr_is_empty_table(r->trailers_out)) {
h2_headers *headers;
apr_table_t *trailers;
ap_log_cerror(APLOG_MARK, APLOG_DEBUG, 0, f->c, APLOGNO(03049)
"h2_task(%s): sending trailers", task->id);
trailers = apr_table_clone(r->pool, r->trailers_out);
headers = h2_headers_rcreate(r, HTTP_OK, trailers, r->pool);
e = h2_bucket_headers_create(bb->bucket_alloc, headers);
APR_BUCKET_INSERT_BEFORE(b, e);
apr_table_clear(r->trailers_out);
ap_remove_output_filter(f);
break;
}
}
}
return ap_pass_brigade(f->next, bb);
}