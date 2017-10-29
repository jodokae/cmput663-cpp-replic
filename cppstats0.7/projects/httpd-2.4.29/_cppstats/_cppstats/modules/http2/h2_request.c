#include <assert.h>
#include <apr_strings.h>
#include <httpd.h>
#include <http_core.h>
#include <http_connection.h>
#include <http_protocol.h>
#include <http_request.h>
#include <http_log.h>
#include <http_vhost.h>
#include <util_filter.h>
#include <ap_mpm.h>
#include <mod_core.h>
#include <scoreboard.h>
#include "h2_private.h"
#include "h2_config.h"
#include "h2_push.h"
#include "h2_request.h"
#include "h2_util.h"
typedef struct {
apr_table_t *headers;
apr_pool_t *pool;
apr_status_t status;
} h1_ctx;
static int set_h1_header(void *ctx, const char *key, const char *value) {
h1_ctx *x = ctx;
x->status = h2_req_add_header(x->headers, x->pool, key, strlen(key),
value, strlen(value));
return (x->status == APR_SUCCESS)? 1 : 0;
}
apr_status_t h2_request_rcreate(h2_request **preq, apr_pool_t *pool,
request_rec *r) {
h2_request *req;
const char *scheme, *authority, *path;
h1_ctx x;
*preq = NULL;
scheme = apr_pstrdup(pool, r->parsed_uri.scheme? r->parsed_uri.scheme
: ap_http_scheme(r));
authority = apr_pstrdup(pool, r->hostname);
path = apr_uri_unparse(pool, &r->parsed_uri, APR_URI_UNP_OMITSITEPART);
if (!r->method || !scheme || !r->hostname || !path) {
return APR_EINVAL;
}
if (!ap_strchr_c(authority, ':') && r->server && r->server->port) {
apr_port_t defport = apr_uri_port_of_scheme(scheme);
if (defport != r->server->port) {
authority = apr_psprintf(pool, "%s:%d", authority,
(int)r->server->port);
}
}
req = apr_pcalloc(pool, sizeof(*req));
req->method = apr_pstrdup(pool, r->method);
req->scheme = scheme;
req->authority = authority;
req->path = path;
req->headers = apr_table_make(pool, 10);
if (r->server) {
req->serialize = h2_config_geti(h2_config_sget(r->server),
H2_CONF_SER_HEADERS);
}
x.pool = pool;
x.headers = req->headers;
x.status = APR_SUCCESS;
apr_table_do(set_h1_header, &x, r->headers_in, NULL);
*preq = req;
return x.status;
}
apr_status_t h2_request_add_header(h2_request *req, apr_pool_t *pool,
const char *name, size_t nlen,
const char *value, size_t vlen) {
apr_status_t status = APR_SUCCESS;
if (nlen <= 0) {
return status;
}
if (name[0] == ':') {
if (!apr_is_empty_table(req->headers)) {
ap_log_perror(APLOG_MARK, APLOG_ERR, 0, pool,
APLOGNO(02917)
"h2_request: pseudo header after request start");
return APR_EGENERAL;
}
if (H2_HEADER_METHOD_LEN == nlen
&& !strncmp(H2_HEADER_METHOD, name, nlen)) {
req->method = apr_pstrndup(pool, value, vlen);
} else if (H2_HEADER_SCHEME_LEN == nlen
&& !strncmp(H2_HEADER_SCHEME, name, nlen)) {
req->scheme = apr_pstrndup(pool, value, vlen);
} else if (H2_HEADER_PATH_LEN == nlen
&& !strncmp(H2_HEADER_PATH, name, nlen)) {
req->path = apr_pstrndup(pool, value, vlen);
} else if (H2_HEADER_AUTH_LEN == nlen
&& !strncmp(H2_HEADER_AUTH, name, nlen)) {
req->authority = apr_pstrndup(pool, value, vlen);
} else {
char buffer[32];
memset(buffer, 0, 32);
strncpy(buffer, name, (nlen > 31)? 31 : nlen);
ap_log_perror(APLOG_MARK, APLOG_WARNING, 0, pool,
APLOGNO(02954)
"h2_request: ignoring unknown pseudo header %s",
buffer);
}
} else {
status = h2_req_add_header(req->headers, pool, name, nlen, value, vlen);
}
return status;
}
apr_status_t h2_request_end_headers(h2_request *req, apr_pool_t *pool, int eos) {
const char *s;
if (!req->authority) {
const char *host = apr_table_get(req->headers, "Host");
if (!host) {
return APR_BADARG;
}
req->authority = host;
} else {
apr_table_setn(req->headers, "Host", req->authority);
}
s = apr_table_get(req->headers, "Content-Length");
if (!s) {
if (!eos) {
req->chunked = 1;
apr_table_mergen(req->headers, "Transfer-Encoding", "chunked");
} else if (apr_table_get(req->headers, "Content-Type")) {
apr_table_setn(req->headers, "Content-Length", "0");
}
}
return APR_SUCCESS;
}
h2_request *h2_request_clone(apr_pool_t *p, const h2_request *src) {
h2_request *dst = apr_pmemdup(p, src, sizeof(*dst));
dst->method = apr_pstrdup(p, src->method);
dst->scheme = apr_pstrdup(p, src->scheme);
dst->authority = apr_pstrdup(p, src->authority);
dst->path = apr_pstrdup(p, src->path);
dst->headers = apr_table_clone(p, src->headers);
return dst;
}
request_rec *h2_request_create_rec(const h2_request *req, conn_rec *c) {
int access_status = HTTP_OK;
const char *rpath;
apr_pool_t *p;
request_rec *r;
const char *s;
apr_pool_create(&p, c->pool);
apr_pool_tag(p, "request");
r = apr_pcalloc(p, sizeof(request_rec));
AP_READ_REQUEST_ENTRY((intptr_t)r, (uintptr_t)c);
r->pool = p;
r->connection = c;
r->server = c->base_server;
r->user = NULL;
r->ap_auth_type = NULL;
r->allowed_methods = ap_make_method_list(p, 2);
r->headers_in = apr_table_clone(r->pool, req->headers);
r->trailers_in = apr_table_make(r->pool, 5);
r->subprocess_env = apr_table_make(r->pool, 25);
r->headers_out = apr_table_make(r->pool, 12);
r->err_headers_out = apr_table_make(r->pool, 5);
r->trailers_out = apr_table_make(r->pool, 5);
r->notes = apr_table_make(r->pool, 5);
r->request_config = ap_create_request_config(r->pool);
r->proto_output_filters = c->output_filters;
r->output_filters = r->proto_output_filters;
r->proto_input_filters = c->input_filters;
r->input_filters = r->proto_input_filters;
ap_run_create_request(r);
r->per_dir_config = r->server->lookup_defaults;
r->sent_bodyct = 0;
r->read_length = 0;
r->read_body = REQUEST_NO_BODY;
r->status = HTTP_OK;
r->header_only = 0;
r->the_request = NULL;
r->used_path_info = AP_REQ_DEFAULT_PATH_INFO;
r->useragent_addr = c->client_addr;
r->useragent_ip = c->client_ip;
ap_run_pre_read_request(r, c);
r->request_time = req->request_time;
r->method = req->method;
r->method_number = ap_method_number_of(r->method);
if (r->method_number == M_GET && r->method[0] == 'H') {
r->header_only = 1;
}
rpath = (req->path ? req->path : "");
ap_parse_uri(r, rpath);
r->protocol = (char*)"HTTP/2.0";
r->proto_num = HTTP_VERSION(2, 0);
r->the_request = apr_psprintf(r->pool, "%s %s %s",
r->method, rpath, r->protocol);
r->hostname = NULL;
ap_update_vhost_from_headers(r);
r->per_dir_config = r->server->lookup_defaults;
s = apr_table_get(r->headers_in, "Expect");
if (s && s[0]) {
if (ap_cstr_casecmp(s, "100-continue") == 0) {
r->expecting_100 = 1;
} else {
r->status = HTTP_EXPECTATION_FAILED;
ap_send_error_response(r, 0);
}
}
ap_add_input_filter_handle(ap_http_input_filter_handle,
NULL, r, r->connection);
if (access_status != HTTP_OK
|| (access_status = ap_run_post_read_request(r))) {
ap_log_cerror(APLOG_MARK, APLOG_DEBUG, 0, c, APLOGNO(03367)
"h2_request: access_status=%d, request_create failed",
access_status);
ap_die(access_status, r);
ap_update_child_status(c->sbh, SERVER_BUSY_LOG, r);
ap_run_log_transaction(r);
r = NULL;
goto traceout;
}
AP_READ_REQUEST_SUCCESS((uintptr_t)r, (char *)r->method,
(char *)r->uri, (char *)r->server->defn_name,
r->status);
return r;
traceout:
AP_READ_REQUEST_FAILURE((uintptr_t)r);
return r;
}