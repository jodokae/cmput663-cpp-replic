#include "apr.h"
#include "apr_strings.h"
#include "apr_buckets.h"
#include "apr_lib.h"
#include "apr_signal.h"
#define APR_WANT_STDIO
#define APR_WANT_STRFUNC
#define APR_WANT_MEMFUNC
#include "apr_want.h"
#include "util_filter.h"
#include "ap_config.h"
#include "httpd.h"
#include "http_config.h"
#include "http_core.h"
#include "http_protocol.h"
#include "http_main.h"
#include "http_request.h"
#include "http_vhost.h"
#include "http_log.h"
#include "apr_date.h"
#include "util_charset.h"
#include "util_ebcdic.h"
#include "util_time.h"
#include "ap_mpm.h"
#include "mod_core.h"
#if APR_HAVE_STDARG_H
#include <stdarg.h>
#endif
#if APR_HAVE_UNISTD_H
#include <unistd.h>
#endif
APLOG_USE_MODULE(http);
static const char * const status_lines[RESPONSE_CODES] = {
"100 Continue",
"101 Switching Protocols",
"102 Processing",
#define LEVEL_200 3
"200 OK",
"201 Created",
"202 Accepted",
"203 Non-Authoritative Information",
"204 No Content",
"205 Reset Content",
"206 Partial Content",
"207 Multi-Status",
"208 Already Reported",
NULL,
NULL,
NULL,
NULL,
NULL,
NULL,
NULL,
NULL,
NULL,
NULL,
NULL,
NULL,
NULL,
NULL,
NULL,
NULL,
NULL,
"226 IM Used",
#define LEVEL_300 30
"300 Multiple Choices",
"301 Moved Permanently",
"302 Found",
"303 See Other",
"304 Not Modified",
"305 Use Proxy",
NULL,
"307 Temporary Redirect",
"308 Permanent Redirect",
#define LEVEL_400 39
"400 Bad Request",
"401 Unauthorized",
"402 Payment Required",
"403 Forbidden",
"404 Not Found",
"405 Method Not Allowed",
"406 Not Acceptable",
"407 Proxy Authentication Required",
"408 Request Timeout",
"409 Conflict",
"410 Gone",
"411 Length Required",
"412 Precondition Failed",
"413 Request Entity Too Large",
"414 Request-URI Too Long",
"415 Unsupported Media Type",
"416 Requested Range Not Satisfiable",
"417 Expectation Failed",
NULL,
NULL,
NULL,
"421 Misdirected Request",
"422 Unprocessable Entity",
"423 Locked",
"424 Failed Dependency",
NULL,
"426 Upgrade Required",
NULL,
"428 Precondition Required",
"429 Too Many Requests",
NULL,
"431 Request Header Fields Too Large",
NULL,
NULL,
NULL,
NULL,
NULL,
NULL,
NULL,
NULL,
NULL,
NULL,
NULL,
NULL,
NULL,
NULL,
NULL,
NULL,
NULL,
NULL,
NULL,
"451 Unavailable For Legal Reasons",
#define LEVEL_500 91
"500 Internal Server Error",
"501 Not Implemented",
"502 Bad Gateway",
"503 Service Unavailable",
"504 Gateway Timeout",
"505 HTTP Version Not Supported",
"506 Variant Also Negotiates",
"507 Insufficient Storage",
"508 Loop Detected",
NULL,
"510 Not Extended",
"511 Network Authentication Required"
};
APR_HOOK_STRUCT(
APR_HOOK_LINK(insert_error_filter)
)
AP_IMPLEMENT_HOOK_VOID(insert_error_filter, (request_rec *r), (r))
#define METHOD_NUMBER_FIRST (M_INVALID + 1)
#define METHOD_NUMBER_LAST 62
static int is_mpm_running(void) {
int mpm_state = 0;
if (ap_mpm_query(AP_MPMQ_MPM_STATE, &mpm_state)) {
return 0;
}
if (mpm_state == AP_MPMQ_STOPPING) {
return 0;
}
return 1;
}
AP_DECLARE(int) ap_set_keepalive(request_rec *r) {
int ka_sent = 0;
int left = r->server->keep_alive_max - r->connection->keepalives;
int wimpy = ap_find_token(r->pool,
apr_table_get(r->headers_out, "Connection"),
"close");
const char *conn = apr_table_get(r->headers_in, "Connection");
if ((r->connection->keepalive != AP_CONN_CLOSE)
&& !r->expecting_100
&& ((r->status == HTTP_NOT_MODIFIED)
|| (r->status == HTTP_NO_CONTENT)
|| r->header_only
|| apr_table_get(r->headers_out, "Content-Length")
|| ap_find_last_token(r->pool,
apr_table_get(r->headers_out,
"Transfer-Encoding"),
"chunked")
|| ((r->proto_num >= HTTP_VERSION(1,1))
&& (r->chunked = 1)))
&& r->server->keep_alive
&& (r->server->keep_alive_timeout > 0)
&& ((r->server->keep_alive_max == 0)
|| (left > 0))
&& !ap_status_drops_connection(r->status)
&& !wimpy
&& !ap_find_token(r->pool, conn, "close")
&& (!apr_table_get(r->subprocess_env, "nokeepalive")
|| apr_table_get(r->headers_in, "Via"))
&& ((ka_sent = ap_find_token(r->pool, conn, "keep-alive"))
|| (r->proto_num >= HTTP_VERSION(1,1)))
&& is_mpm_running()) {
r->connection->keepalive = AP_CONN_KEEPALIVE;
r->connection->keepalives++;
if (ka_sent) {
if (r->server->keep_alive_max) {
apr_table_setn(r->headers_out, "Keep-Alive",
apr_psprintf(r->pool, "timeout=%d, max=%d",
(int)apr_time_sec(r->server->keep_alive_timeout),
left));
} else {
apr_table_setn(r->headers_out, "Keep-Alive",
apr_psprintf(r->pool, "timeout=%d",
(int)apr_time_sec(r->server->keep_alive_timeout)));
}
apr_table_mergen(r->headers_out, "Connection", "Keep-Alive");
}
return 1;
}
if (!wimpy) {
apr_table_mergen(r->headers_out, "Connection", "close");
}
if ((r->connection->keepalive != AP_CONN_CLOSE)
&& r->server->keep_alive_max
&& !left) {
r->connection->keepalives++;
}
r->connection->keepalive = AP_CONN_CLOSE;
return 0;
}
AP_DECLARE(ap_condition_e) ap_condition_if_match(request_rec *r,
apr_table_t *headers) {
const char *if_match, *etag;
if ((if_match = apr_table_get(r->headers_in, "If-Match")) != NULL) {
if (if_match[0] == '*'
|| ((etag = apr_table_get(headers, "ETag")) != NULL
&& ap_find_etag_strong(r->pool, if_match, etag))) {
return AP_CONDITION_STRONG;
} else {
return AP_CONDITION_NOMATCH;
}
}
return AP_CONDITION_NONE;
}
AP_DECLARE(ap_condition_e) ap_condition_if_unmodified_since(request_rec *r,
apr_table_t *headers) {
const char *if_unmodified;
if_unmodified = apr_table_get(r->headers_in, "If-Unmodified-Since");
if (if_unmodified) {
apr_int64_t mtime, reqtime;
apr_time_t ius = apr_time_sec(apr_date_parse_http(if_unmodified));
mtime = apr_time_sec(apr_date_parse_http(
apr_table_get(headers, "Last-Modified")));
if (mtime == APR_DATE_BAD) {
mtime = apr_time_sec(r->mtime ? r->mtime : apr_time_now());
}
reqtime = apr_time_sec(apr_date_parse_http(
apr_table_get(headers, "Date")));
if (!reqtime) {
reqtime = apr_time_sec(r->request_time);
}
if ((ius != APR_DATE_BAD) && (mtime > ius)) {
if (reqtime < mtime + 60) {
if (apr_table_get(r->headers_in, "Range")) {
return AP_CONDITION_NOMATCH;
} else {
return AP_CONDITION_WEAK;
}
} else {
return AP_CONDITION_STRONG;
}
} else {
return AP_CONDITION_NOMATCH;
}
}
return AP_CONDITION_NONE;
}
AP_DECLARE(ap_condition_e) ap_condition_if_none_match(request_rec *r,
apr_table_t *headers) {
const char *if_nonematch, *etag;
if_nonematch = apr_table_get(r->headers_in, "If-None-Match");
if (if_nonematch != NULL) {
if (if_nonematch[0] == '*') {
return AP_CONDITION_STRONG;
}
if (r->method_number == M_GET) {
if ((etag = apr_table_get(headers, "ETag")) != NULL) {
if (apr_table_get(r->headers_in, "Range")) {
if (ap_find_etag_strong(r->pool, if_nonematch, etag)) {
return AP_CONDITION_STRONG;
}
} else {
if (ap_find_etag_weak(r->pool, if_nonematch, etag)) {
return AP_CONDITION_WEAK;
}
}
}
} else if ((etag = apr_table_get(headers, "ETag")) != NULL
&& ap_find_etag_strong(r->pool, if_nonematch, etag)) {
return AP_CONDITION_STRONG;
}
return AP_CONDITION_NOMATCH;
}
return AP_CONDITION_NONE;
}
AP_DECLARE(ap_condition_e) ap_condition_if_modified_since(request_rec *r,
apr_table_t *headers) {
const char *if_modified_since;
if ((if_modified_since = apr_table_get(r->headers_in, "If-Modified-Since"))
!= NULL) {
apr_int64_t mtime;
apr_int64_t ims, reqtime;
mtime = apr_time_sec(apr_date_parse_http(
apr_table_get(headers, "Last-Modified")));
if (mtime == APR_DATE_BAD) {
mtime = apr_time_sec(r->mtime ? r->mtime : apr_time_now());
}
reqtime = apr_time_sec(apr_date_parse_http(
apr_table_get(headers, "Date")));
if (!reqtime) {
reqtime = apr_time_sec(r->request_time);
}
ims = apr_time_sec(apr_date_parse_http(if_modified_since));
if (ims >= mtime && ims <= reqtime) {
if (reqtime < mtime + 60) {
if (apr_table_get(r->headers_in, "Range")) {
return AP_CONDITION_NOMATCH;
} else {
return AP_CONDITION_WEAK;
}
} else {
return AP_CONDITION_STRONG;
}
} else {
return AP_CONDITION_NOMATCH;
}
}
return AP_CONDITION_NONE;
}
AP_DECLARE(ap_condition_e) ap_condition_if_range(request_rec *r,
apr_table_t *headers) {
const char *if_range, *etag;
if ((if_range = apr_table_get(r->headers_in, "If-Range"))
&& apr_table_get(r->headers_in, "Range")) {
if (if_range[0] == '"') {
if ((etag = apr_table_get(headers, "ETag"))
&& !strcmp(if_range, etag)) {
return AP_CONDITION_STRONG;
} else {
return AP_CONDITION_NOMATCH;
}
} else {
apr_int64_t mtime;
apr_int64_t rtime, reqtime;
mtime = apr_time_sec(apr_date_parse_http(
apr_table_get(headers, "Last-Modified")));
if (mtime == APR_DATE_BAD) {
mtime = apr_time_sec(r->mtime ? r->mtime : apr_time_now());
}
reqtime = apr_time_sec(apr_date_parse_http(
apr_table_get(headers, "Date")));
if (!reqtime) {
reqtime = apr_time_sec(r->request_time);
}
rtime = apr_time_sec(apr_date_parse_http(if_range));
if (rtime == mtime) {
if (reqtime < mtime + 60) {
return AP_CONDITION_NOMATCH;
} else {
return AP_CONDITION_STRONG;
}
} else {
return AP_CONDITION_NOMATCH;
}
}
}
return AP_CONDITION_NONE;
}
AP_DECLARE(int) ap_meets_conditions(request_rec *r) {
int not_modified = -1;
ap_condition_e cond;
if (!ap_is_HTTP_SUCCESS(r->status) || r->no_local_copy) {
return OK;
}
cond = ap_condition_if_match(r, r->headers_out);
if (AP_CONDITION_NOMATCH == cond) {
return HTTP_PRECONDITION_FAILED;
}
cond = ap_condition_if_unmodified_since(r, r->headers_out);
if (AP_CONDITION_NOMATCH == cond) {
not_modified = 0;
} else if (cond >= AP_CONDITION_WEAK) {
return HTTP_PRECONDITION_FAILED;
}
cond = ap_condition_if_none_match(r, r->headers_out);
if (AP_CONDITION_NOMATCH == cond) {
not_modified = 0;
} else if (cond >= AP_CONDITION_WEAK) {
if (r->method_number == M_GET) {
if (not_modified) {
not_modified = 1;
}
} else {
return HTTP_PRECONDITION_FAILED;
}
}
cond = ap_condition_if_modified_since(r, r->headers_out);
if (AP_CONDITION_NOMATCH == cond) {
not_modified = 0;
} else if (cond >= AP_CONDITION_WEAK) {
if (r->method_number == M_GET) {
if (not_modified) {
not_modified = 1;
}
}
}
cond = ap_condition_if_range(r, r->headers_out);
if (cond > AP_CONDITION_NONE) {
return OK;
}
if (not_modified == 1) {
return HTTP_NOT_MODIFIED;
}
return OK;
}
static apr_hash_t *methods_registry = NULL;
static int cur_method_number = METHOD_NUMBER_FIRST;
static void register_one_method(apr_pool_t *p, const char *methname,
int methnum) {
int *pnum = apr_palloc(p, sizeof(*pnum));
*pnum = methnum;
apr_hash_set(methods_registry, methname, APR_HASH_KEY_STRING, pnum);
}
static apr_status_t ap_method_registry_destroy(void *notused) {
methods_registry = NULL;
cur_method_number = METHOD_NUMBER_FIRST;
return APR_SUCCESS;
}
AP_DECLARE(void) ap_method_registry_init(apr_pool_t *p) {
methods_registry = apr_hash_make(p);
apr_pool_cleanup_register(p, NULL,
ap_method_registry_destroy,
apr_pool_cleanup_null);
register_one_method(p, "GET", M_GET);
register_one_method(p, "HEAD", M_GET);
register_one_method(p, "PUT", M_PUT);
register_one_method(p, "POST", M_POST);
register_one_method(p, "DELETE", M_DELETE);
register_one_method(p, "CONNECT", M_CONNECT);
register_one_method(p, "OPTIONS", M_OPTIONS);
register_one_method(p, "TRACE", M_TRACE);
register_one_method(p, "PATCH", M_PATCH);
register_one_method(p, "PROPFIND", M_PROPFIND);
register_one_method(p, "PROPPATCH", M_PROPPATCH);
register_one_method(p, "MKCOL", M_MKCOL);
register_one_method(p, "COPY", M_COPY);
register_one_method(p, "MOVE", M_MOVE);
register_one_method(p, "LOCK", M_LOCK);
register_one_method(p, "UNLOCK", M_UNLOCK);
register_one_method(p, "VERSION-CONTROL", M_VERSION_CONTROL);
register_one_method(p, "CHECKOUT", M_CHECKOUT);
register_one_method(p, "UNCHECKOUT", M_UNCHECKOUT);
register_one_method(p, "CHECKIN", M_CHECKIN);
register_one_method(p, "UPDATE", M_UPDATE);
register_one_method(p, "LABEL", M_LABEL);
register_one_method(p, "REPORT", M_REPORT);
register_one_method(p, "MKWORKSPACE", M_MKWORKSPACE);
register_one_method(p, "MKACTIVITY", M_MKACTIVITY);
register_one_method(p, "BASELINE-CONTROL", M_BASELINE_CONTROL);
register_one_method(p, "MERGE", M_MERGE);
}
AP_DECLARE(int) ap_method_register(apr_pool_t *p, const char *methname) {
int *methnum;
if (methods_registry == NULL) {
ap_method_registry_init(p);
}
if (methname == NULL) {
return M_INVALID;
}
methnum = (int *)apr_hash_get(methods_registry, methname,
APR_HASH_KEY_STRING);
if (methnum != NULL)
return *methnum;
if (cur_method_number > METHOD_NUMBER_LAST) {
ap_log_perror(APLOG_MARK, APLOG_ERR, 0, p, APLOGNO(01610)
"Maximum new request methods %d reached while "
"registering method %s.",
METHOD_NUMBER_LAST, methname);
return M_INVALID;
}
register_one_method(p, methname, cur_method_number);
return cur_method_number++;
}
#define UNKNOWN_METHOD (-1)
static int lookup_builtin_method(const char *method, apr_size_t len) {
switch (len) {
case 3:
switch (method[0]) {
case 'P':
return (method[1] == 'U'
&& method[2] == 'T'
? M_PUT : UNKNOWN_METHOD);
case 'G':
return (method[1] == 'E'
&& method[2] == 'T'
? M_GET : UNKNOWN_METHOD);
default:
return UNKNOWN_METHOD;
}
case 4:
switch (method[0]) {
case 'H':
return (method[1] == 'E'
&& method[2] == 'A'
&& method[3] == 'D'
? M_GET : UNKNOWN_METHOD);
case 'P':
return (method[1] == 'O'
&& method[2] == 'S'
&& method[3] == 'T'
? M_POST : UNKNOWN_METHOD);
case 'M':
return (method[1] == 'O'
&& method[2] == 'V'
&& method[3] == 'E'
? M_MOVE : UNKNOWN_METHOD);
case 'L':
return (method[1] == 'O'
&& method[2] == 'C'
&& method[3] == 'K'
? M_LOCK : UNKNOWN_METHOD);
case 'C':
return (method[1] == 'O'
&& method[2] == 'P'
&& method[3] == 'Y'
? M_COPY : UNKNOWN_METHOD);
default:
return UNKNOWN_METHOD;
}
case 5:
switch (method[2]) {
case 'T':
return (memcmp(method, "PATCH", 5) == 0
? M_PATCH : UNKNOWN_METHOD);
case 'R':
return (memcmp(method, "MERGE", 5) == 0
? M_MERGE : UNKNOWN_METHOD);
case 'C':
return (memcmp(method, "MKCOL", 5) == 0
? M_MKCOL : UNKNOWN_METHOD);
case 'B':
return (memcmp(method, "LABEL", 5) == 0
? M_LABEL : UNKNOWN_METHOD);
case 'A':
return (memcmp(method, "TRACE", 5) == 0
? M_TRACE : UNKNOWN_METHOD);
default:
return UNKNOWN_METHOD;
}
case 6:
switch (method[0]) {
case 'U':
switch (method[5]) {
case 'K':
return (memcmp(method, "UNLOCK", 6) == 0
? M_UNLOCK : UNKNOWN_METHOD);
case 'E':
return (memcmp(method, "UPDATE", 6) == 0
? M_UPDATE : UNKNOWN_METHOD);
default:
return UNKNOWN_METHOD;
}
case 'R':
return (memcmp(method, "REPORT", 6) == 0
? M_REPORT : UNKNOWN_METHOD);
case 'D':
return (memcmp(method, "DELETE", 6) == 0
? M_DELETE : UNKNOWN_METHOD);
default:
return UNKNOWN_METHOD;
}
case 7:
switch (method[1]) {
case 'P':
return (memcmp(method, "OPTIONS", 7) == 0
? M_OPTIONS : UNKNOWN_METHOD);
case 'O':
return (memcmp(method, "CONNECT", 7) == 0
? M_CONNECT : UNKNOWN_METHOD);
case 'H':
return (memcmp(method, "CHECKIN", 7) == 0
? M_CHECKIN : UNKNOWN_METHOD);
default:
return UNKNOWN_METHOD;
}
case 8:
switch (method[0]) {
case 'P':
return (memcmp(method, "PROPFIND", 8) == 0
? M_PROPFIND : UNKNOWN_METHOD);
case 'C':
return (memcmp(method, "CHECKOUT", 8) == 0
? M_CHECKOUT : UNKNOWN_METHOD);
default:
return UNKNOWN_METHOD;
}
case 9:
return (memcmp(method, "PROPPATCH", 9) == 0
? M_PROPPATCH : UNKNOWN_METHOD);
case 10:
switch (method[0]) {
case 'U':
return (memcmp(method, "UNCHECKOUT", 10) == 0
? M_UNCHECKOUT : UNKNOWN_METHOD);
case 'M':
return (memcmp(method, "MKACTIVITY", 10) == 0
? M_MKACTIVITY : UNKNOWN_METHOD);
default:
return UNKNOWN_METHOD;
}
case 11:
return (memcmp(method, "MKWORKSPACE", 11) == 0
? M_MKWORKSPACE : UNKNOWN_METHOD);
case 15:
return (memcmp(method, "VERSION-CONTROL", 15) == 0
? M_VERSION_CONTROL : UNKNOWN_METHOD);
case 16:
return (memcmp(method, "BASELINE-CONTROL", 16) == 0
? M_BASELINE_CONTROL : UNKNOWN_METHOD);
default:
return UNKNOWN_METHOD;
}
}
AP_DECLARE(int) ap_method_number_of(const char *method) {
int len = strlen(method);
int which = lookup_builtin_method(method, len);
if (which != UNKNOWN_METHOD)
return which;
if (methods_registry != NULL) {
int *methnum = apr_hash_get(methods_registry, method, len);
if (methnum != NULL) {
return *methnum;
}
}
return M_INVALID;
}
AP_DECLARE(const char *) ap_method_name_of(apr_pool_t *p, int methnum) {
apr_hash_index_t *hi = apr_hash_first(p, methods_registry);
for (; hi; hi = apr_hash_next(hi)) {
const void *key;
void *val;
apr_hash_this(hi, &key, NULL, &val);
if (*(int *)val == methnum)
return key;
}
return NULL;
}
AP_DECLARE(int) ap_index_of_response(int status) {
static int shortcut[6] = {0, LEVEL_200, LEVEL_300, LEVEL_400,
LEVEL_500, RESPONSE_CODES
};
int i, pos;
if (status < 100) {
return LEVEL_500;
}
for (i = 0; i < 5; i++) {
status -= 100;
if (status < 100) {
pos = (status + shortcut[i]);
if (pos < shortcut[i + 1] && status_lines[pos] != NULL) {
return pos;
} else {
return LEVEL_500;
}
}
}
return LEVEL_500;
}
AP_DECLARE(const char *) ap_get_status_line(int status) {
return status_lines[ap_index_of_response(status)];
}
static char *make_allow(request_rec *r) {
apr_int64_t mask;
apr_array_header_t *allow = apr_array_make(r->pool, 10, sizeof(char *));
apr_hash_index_t *hi = apr_hash_first(r->pool, methods_registry);
core_server_config *conf =
ap_get_core_module_config(r->server->module_config);
mask = r->allowed_methods->method_mask;
for (; hi; hi = apr_hash_next(hi)) {
const void *key;
void *val;
apr_hash_this(hi, &key, NULL, &val);
if ((mask & (AP_METHOD_BIT << *(int *)val)) != 0) {
APR_ARRAY_PUSH(allow, const char *) = key;
}
}
if (conf->trace_enable != AP_TRACE_DISABLE)
*(const char **)apr_array_push(allow) = "TRACE";
if ((mask & (AP_METHOD_BIT << M_INVALID))
&& (r->allowed_methods->method_list != NULL)
&& (r->allowed_methods->method_list->nelts != 0)) {
apr_array_cat(allow, r->allowed_methods->method_list);
}
return apr_array_pstrcat(r->pool, allow, ',');
}
AP_DECLARE(int) ap_send_http_options(request_rec *r) {
if (r->assbackwards) {
return DECLINED;
}
apr_table_setn(r->headers_out, "Allow", make_allow(r));
return OK;
}
AP_DECLARE(void) ap_set_content_type(request_rec *r, const char *ct) {
if (!ct) {
r->content_type = NULL;
} else if (!r->content_type || strcmp(r->content_type, ct)) {
r->content_type = ct;
}
}
AP_DECLARE(void) ap_set_accept_ranges(request_rec *r) {
core_dir_config *d = ap_get_core_module_config(r->per_dir_config);
apr_table_setn(r->headers_out, "Accept-Ranges",
(d->max_ranges == AP_MAXRANGES_NORANGES) ? "none"
: "bytes");
}
static const char *add_optional_notes(request_rec *r,
const char *prefix,
const char *key,
const char *suffix) {
const char *notes, *result;
if ((notes = apr_table_get(r->notes, key)) == NULL) {
result = apr_pstrcat(r->pool, prefix, suffix, NULL);
} else {
result = apr_pstrcat(r->pool, prefix, notes, suffix, NULL);
}
return result;
}
static const char *get_canned_error_string(int status,
request_rec *r,
const char *location) {
apr_pool_t *p = r->pool;
const char *error_notes, *h1, *s1;
switch (status) {
case HTTP_MOVED_PERMANENTLY:
case HTTP_MOVED_TEMPORARILY:
case HTTP_TEMPORARY_REDIRECT:
case HTTP_PERMANENT_REDIRECT:
return(apr_pstrcat(p,
"<p>The document has moved <a href=\"",
ap_escape_html(r->pool, location),
"\">here</a>.</p>\n",
NULL));
case HTTP_SEE_OTHER:
return(apr_pstrcat(p,
"<p>The answer to your request is located "
"<a href=\"",
ap_escape_html(r->pool, location),
"\">here</a>.</p>\n",
NULL));
case HTTP_USE_PROXY:
return(apr_pstrcat(p,
"<p>This resource is only accessible "
"through the proxy\n",
ap_escape_html(r->pool, location),
"<br />\nYou will need to configure "
"your client to use that proxy.</p>\n",
NULL));
case HTTP_PROXY_AUTHENTICATION_REQUIRED:
case HTTP_UNAUTHORIZED:
return("<p>This server could not verify that you\n"
"are authorized to access the document\n"
"requested. Either you supplied the wrong\n"
"credentials (e.g., bad password), or your\n"
"browser doesn't understand how to supply\n"
"the credentials required.</p>\n");
case HTTP_BAD_REQUEST:
return(add_optional_notes(r,
"<p>Your browser sent a request that "
"this server could not understand.<br />\n",
"error-notes",
"</p>\n"));
case HTTP_FORBIDDEN:
s1 = apr_pstrcat(p,
"<p>You don't have permission to access ",
ap_escape_html(r->pool, r->uri),
"\non this server.<br />\n",
NULL);
return(add_optional_notes(r, s1, "error-notes", "</p>\n"));
case HTTP_NOT_FOUND:
return(apr_pstrcat(p,
"<p>The requested URL ",
ap_escape_html(r->pool, r->uri),
" was not found on this server.</p>\n",
NULL));
case HTTP_METHOD_NOT_ALLOWED:
return(apr_pstrcat(p,
"<p>The requested method ",
ap_escape_html(r->pool, r->method),
" is not allowed for the URL ",
ap_escape_html(r->pool, r->uri),
".</p>\n",
NULL));
case HTTP_NOT_ACCEPTABLE:
s1 = apr_pstrcat(p,
"<p>An appropriate representation of the "
"requested resource ",
ap_escape_html(r->pool, r->uri),
" could not be found on this server.</p>\n",
NULL);
return(add_optional_notes(r, s1, "variant-list", ""));
case HTTP_MULTIPLE_CHOICES:
return(add_optional_notes(r, "", "variant-list", ""));
case HTTP_LENGTH_REQUIRED:
s1 = apr_pstrcat(p,
"<p>A request of the requested method ",
ap_escape_html(r->pool, r->method),
" requires a valid Content-length.<br />\n",
NULL);
return(add_optional_notes(r, s1, "error-notes", "</p>\n"));
case HTTP_PRECONDITION_FAILED:
return(apr_pstrcat(p,
"<p>The precondition on the request "
"for the URL ",
ap_escape_html(r->pool, r->uri),
" evaluated to false.</p>\n",
NULL));
case HTTP_NOT_IMPLEMENTED:
s1 = apr_pstrcat(p,
"<p>",
ap_escape_html(r->pool, r->method), " to ",
ap_escape_html(r->pool, r->uri),
" not supported.<br />\n",
NULL);
return(add_optional_notes(r, s1, "error-notes", "</p>\n"));
case HTTP_BAD_GATEWAY:
s1 = "<p>The proxy server received an invalid" CRLF
"response from an upstream server.<br />" CRLF;
return(add_optional_notes(r, s1, "error-notes", "</p>\n"));
case HTTP_VARIANT_ALSO_VARIES:
return(apr_pstrcat(p,
"<p>A variant for the requested "
"resource\n<pre>\n",
ap_escape_html(r->pool, r->uri),
"\n</pre>\nis itself a negotiable resource. "
"This indicates a configuration error.</p>\n",
NULL));
case HTTP_REQUEST_TIME_OUT:
return("<p>Server timeout waiting for the HTTP request from the client.</p>\n");
case HTTP_GONE:
return(apr_pstrcat(p,
"<p>The requested resource<br />",
ap_escape_html(r->pool, r->uri),
"<br />\nis no longer available on this server "
"and there is no forwarding address.\n"
"Please remove all references to this "
"resource.</p>\n",
NULL));
case HTTP_REQUEST_ENTITY_TOO_LARGE:
return(apr_pstrcat(p,
"The requested resource<br />",
ap_escape_html(r->pool, r->uri), "<br />\n",
"does not allow request data with ",
ap_escape_html(r->pool, r->method),
" requests, or the amount of data provided in\n"
"the request exceeds the capacity limit.\n",
NULL));
case HTTP_REQUEST_URI_TOO_LARGE:
s1 = "<p>The requested URL's length exceeds the capacity\n"
"limit for this server.<br />\n";
return(add_optional_notes(r, s1, "error-notes", "</p>\n"));
case HTTP_UNSUPPORTED_MEDIA_TYPE:
return("<p>The supplied request data is not in a format\n"
"acceptable for processing by this resource.</p>\n");
case HTTP_RANGE_NOT_SATISFIABLE:
return("<p>None of the range-specifier values in the Range\n"
"request-header field overlap the current extent\n"
"of the selected resource.</p>\n");
case HTTP_EXPECTATION_FAILED:
s1 = apr_table_get(r->headers_in, "Expect");
if (s1)
s1 = apr_pstrcat(p,
"<p>The expectation given in the Expect request-header\n"
"field could not be met by this server.\n"
"The client sent<pre>\n Expect: ",
ap_escape_html(r->pool, s1), "\n</pre>\n",
NULL);
else
s1 = "<p>No expectation was seen, the Expect request-header \n"
"field was not presented by the client.\n";
return add_optional_notes(r, s1, "error-notes", "</p>"
"<p>Only the 100-continue expectation is supported.</p>\n");
case HTTP_UNPROCESSABLE_ENTITY:
return("<p>The server understands the media type of the\n"
"request entity, but was unable to process the\n"
"contained instructions.</p>\n");
case HTTP_LOCKED:
return("<p>The requested resource is currently locked.\n"
"The lock must be released or proper identification\n"
"given before the method can be applied.</p>\n");
case HTTP_FAILED_DEPENDENCY:
return("<p>The method could not be performed on the resource\n"
"because the requested action depended on another\n"
"action and that other action failed.</p>\n");
case HTTP_UPGRADE_REQUIRED:
return("<p>The requested resource can only be retrieved\n"
"using SSL. The server is willing to upgrade the current\n"
"connection to SSL, but your client doesn't support it.\n"
"Either upgrade your client, or try requesting the page\n"
"using https://\n");
case HTTP_PRECONDITION_REQUIRED:
return("<p>The request is required to be conditional.</p>\n");
case HTTP_TOO_MANY_REQUESTS:
return("<p>The user has sent too many requests\n"
"in a given amount of time.</p>\n");
case HTTP_REQUEST_HEADER_FIELDS_TOO_LARGE:
return("<p>The server refused this request because\n"
"the request header fields are too large.</p>\n");
case HTTP_INSUFFICIENT_STORAGE:
return("<p>The method could not be performed on the resource\n"
"because the server is unable to store the\n"
"representation needed to successfully complete the\n"
"request. There is insufficient free space left in\n"
"your storage allocation.</p>\n");
case HTTP_SERVICE_UNAVAILABLE:
return("<p>The server is temporarily unable to service your\n"
"request due to maintenance downtime or capacity\n"
"problems. Please try again later.</p>\n");
case HTTP_GATEWAY_TIME_OUT:
return("<p>The gateway did not receive a timely response\n"
"from the upstream server or application.</p>\n");
case HTTP_LOOP_DETECTED:
return("<p>The server terminated an operation because\n"
"it encountered an infinite loop.</p>\n");
case HTTP_NOT_EXTENDED:
return("<p>A mandatory extension policy in the request is not\n"
"accepted by the server for this resource.</p>\n");
case HTTP_NETWORK_AUTHENTICATION_REQUIRED:
return("<p>The client needs to authenticate to gain\n"
"network access.</p>\n");
case HTTP_MISDIRECTED_REQUEST:
return("<p>The client needs a new connection for this\n"
"request as the requested host name does not match\n"
"the Server Name Indication (SNI) in use for this\n"
"connection.</p>\n");
case HTTP_UNAVAILABLE_FOR_LEGAL_REASONS:
s1 = apr_pstrcat(p,
"<p>Access to ", ap_escape_html(r->pool, r->uri),
"\nhas been denied for legal reasons.<br />\n",
NULL);
return(add_optional_notes(r, s1, "error-notes", "</p>\n"));
default:
if (((error_notes = apr_table_get(r->notes,
"error-notes")) != NULL)
&& (h1 = apr_table_get(r->notes, "verbose-error-to")) != NULL
&& (strcmp(h1, "*") == 0)) {
return(apr_pstrcat(p, error_notes, "<p />\n", NULL));
} else {
return(apr_pstrcat(p,
"<p>The server encountered an internal "
"error or\n"
"misconfiguration and was unable to complete\n"
"your request.</p>\n"
"<p>Please contact the server "
"administrator at \n ",
ap_escape_html(r->pool,
r->server->server_admin),
" to inform them of the time this "
"error occurred,\n"
" and the actions you performed just before "
"this error.</p>\n"
"<p>More information about this error "
"may be available\n"
"in the server error log.</p>\n",
NULL));
}
}
}
AP_DECLARE(void) ap_send_error_response(request_rec *r, int recursive_error) {
int status = r->status;
int idx = ap_index_of_response(status);
char *custom_response;
const char *location = apr_table_get(r->headers_out, "Location");
r->eos_sent = 0;
r->output_filters = r->proto_output_filters;
ap_run_insert_error_filter(r);
if (location == NULL) {
location = apr_table_get(r->err_headers_out, "Location");
}
if (status == HTTP_NOT_MODIFIED) {
ap_finalize_request_protocol(r);
return;
}
if (status == HTTP_NO_CONTENT) {
ap_finalize_request_protocol(r);
return;
}
if (!r->assbackwards) {
apr_table_t *tmp = r->headers_out;
r->headers_out = r->err_headers_out;
r->err_headers_out = tmp;
apr_table_clear(r->err_headers_out);
if (ap_is_HTTP_REDIRECT(status) || (status == HTTP_CREATED)) {
if ((location != NULL) && *location) {
apr_table_setn(r->headers_out, "Location", location);
} else {
location = "";
}
}
r->content_languages = NULL;
r->content_encoding = NULL;
r->clength = 0;
if (apr_table_get(r->subprocess_env,
"suppress-error-charset") != NULL) {
core_request_config *request_conf =
ap_get_core_module_config(r->request_config);
request_conf->suppress_charset = 1;
ap_set_content_type(r, "text/html");
} else {
ap_set_content_type(r, "text/html; charset=iso-8859-1");
}
if ((status == HTTP_METHOD_NOT_ALLOWED)
|| (status == HTTP_NOT_IMPLEMENTED)) {
apr_table_setn(r->headers_out, "Allow", make_allow(r));
}
if (r->header_only) {
ap_finalize_request_protocol(r);
return;
}
}
if ((custom_response = ap_response_code_string(r, idx))) {
if (custom_response[0] == '\"') {
ap_rputs(custom_response + 1, r);
ap_finalize_request_protocol(r);
return;
}
}
{
const char *title = status_lines[idx];
const char *h1;
if (r->status_line) {
char *end;
int len = strlen(r->status_line);
if (len >= 3
&& apr_strtoi64(r->status_line, &end, 10) == r->status
&& (end - 3) == r->status_line
&& (len < 4 || apr_isspace(r->status_line[3]))
&& (len < 5 || apr_isalnum(r->status_line[4]))) {
if (len == 3) {
r->status_line = apr_pstrcat(r->pool, r->status_line, " Unknown Reason", NULL);
} else if (len == 4) {
r->status_line = apr_pstrcat(r->pool, r->status_line, "Unknown Reason", NULL);
}
title = r->status_line;
}
}
h1 = &title[4];
ap_rvputs_proto_in_ascii(r,
DOCTYPE_HTML_2_0
"<html><head>\n<title>", title,
"</title>\n</head><body>\n<h1>", h1, "</h1>\n",
NULL);
ap_rvputs_proto_in_ascii(r,
get_canned_error_string(status, r, location),
NULL);
if (recursive_error) {
ap_rvputs_proto_in_ascii(r, "<p>Additionally, a ",
status_lines[ap_index_of_response(recursive_error)],
"\nerror was encountered while trying to use an "
"ErrorDocument to handle the request.</p>\n", NULL);
}
ap_rvputs_proto_in_ascii(r, ap_psignature("<hr>\n", r), NULL);
ap_rvputs_proto_in_ascii(r, "</body></html>\n", NULL);
}
ap_finalize_request_protocol(r);
}
AP_DECLARE(ap_method_list_t *) ap_make_method_list(apr_pool_t *p, int nelts) {
ap_method_list_t *ml;
ml = (ap_method_list_t *) apr_palloc(p, sizeof(ap_method_list_t));
ml->method_mask = 0;
ml->method_list = apr_array_make(p, nelts, sizeof(char *));
return ml;
}
AP_DECLARE(void) ap_copy_method_list(ap_method_list_t *dest,
ap_method_list_t *src) {
int i;
char **imethods;
char **omethods;
dest->method_mask = src->method_mask;
imethods = (char **) src->method_list->elts;
for (i = 0; i < src->method_list->nelts; ++i) {
omethods = (char **) apr_array_push(dest->method_list);
*omethods = apr_pstrdup(dest->method_list->pool, imethods[i]);
}
}
AP_DECLARE(int) ap_method_in_list(ap_method_list_t *l, const char *method) {
int methnum;
methnum = ap_method_number_of(method);
if (methnum != M_INVALID) {
return !!(l->method_mask & (AP_METHOD_BIT << methnum));
}
if ((l->method_list == NULL) || (l->method_list->nelts == 0)) {
return 0;
}
return ap_array_str_contains(l->method_list, method);
}
AP_DECLARE(void) ap_method_list_add(ap_method_list_t *l, const char *method) {
int methnum;
const char **xmethod;
methnum = ap_method_number_of(method);
if (methnum != M_INVALID) {
l->method_mask |= (AP_METHOD_BIT << methnum);
return;
}
if (ap_array_str_contains(l->method_list, method)) {
return;
}
xmethod = (const char **) apr_array_push(l->method_list);
*xmethod = method;
}
AP_DECLARE(void) ap_method_list_remove(ap_method_list_t *l,
const char *method) {
int methnum;
char **methods;
methnum = ap_method_number_of(method);
if (methnum != M_INVALID) {
l->method_mask &= ~(AP_METHOD_BIT << methnum);
return;
}
if (l->method_list->nelts != 0) {
int i, j, k;
methods = (char **)l->method_list->elts;
for (i = 0; i < l->method_list->nelts; ) {
if (strcmp(method, methods[i]) == 0) {
for (j = i, k = i + 1; k < l->method_list->nelts; ++j, ++k) {
methods[j] = methods[k];
}
--l->method_list->nelts;
} else {
++i;
}
}
}
}
AP_DECLARE(void) ap_clear_method_list(ap_method_list_t *l) {
l->method_mask = 0;
l->method_list->nelts = 0;
}
