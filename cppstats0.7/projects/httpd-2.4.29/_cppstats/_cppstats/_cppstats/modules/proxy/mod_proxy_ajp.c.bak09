#include "mod_proxy.h"
#include "ajp.h"
module AP_MODULE_DECLARE_DATA proxy_ajp_module;
static int proxy_ajp_canon(request_rec *r, char *url) {
char *host, *path, sport[7];
char *search = NULL;
const char *err;
apr_port_t port, def_port;
if (strncasecmp(url, "ajp:", 4) == 0) {
url += 4;
} else {
return DECLINED;
}
ap_log_rerror(APLOG_MARK, APLOG_TRACE1, 0, r, "canonicalising URL %s", url);
port = def_port = ap_proxy_port_of_scheme("ajp");
err = ap_proxy_canon_netloc(r->pool, &url, NULL, NULL, &host, &port);
if (err) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(00867) "error parsing URL %s: %s",
url, err);
return HTTP_BAD_REQUEST;
}
if (apr_table_get(r->notes, "proxy-nocanon")) {
path = url;
} else {
path = ap_proxy_canonenc(r->pool, url, strlen(url), enc_path, 0,
r->proxyreq);
search = r->args;
}
if (path == NULL)
return HTTP_BAD_REQUEST;
if (port != def_port)
apr_snprintf(sport, sizeof(sport), ":%d", port);
else
sport[0] = '\0';
if (ap_strchr_c(host, ':')) {
host = apr_pstrcat(r->pool, "[", host, "]", NULL);
}
r->filename = apr_pstrcat(r->pool, "proxy:ajp://", host, sport,
"/", path, (search) ? "?" : "",
(search) ? search : "", NULL);
return OK;
}
#define METHOD_NON_IDEMPOTENT 0
#define METHOD_IDEMPOTENT 1
#define METHOD_IDEMPOTENT_WITH_ARGS 2
static int is_idempotent(request_rec *r) {
switch (r->method_number) {
case M_GET:
case M_DELETE:
case M_PUT:
case M_OPTIONS:
case M_TRACE:
if (r->args) {
return METHOD_IDEMPOTENT_WITH_ARGS;
}
return METHOD_IDEMPOTENT;
default:
return METHOD_NON_IDEMPOTENT;
}
}
static apr_off_t get_content_length(request_rec * r) {
apr_off_t len = 0;
if (r->main == NULL) {
const char *clp = apr_table_get(r->headers_in, "Content-Length");
if (clp) {
char *errp;
if (apr_strtoff(&len, clp, &errp, 10) || *errp || len < 0) {
len = 0;
}
}
}
return len;
}
static int ap_proxy_ajp_request(apr_pool_t *p, request_rec *r,
proxy_conn_rec *conn,
conn_rec *origin,
proxy_dir_conf *conf,
apr_uri_t *uri,
char *url, char *server_portstr) {
apr_status_t status;
int result;
apr_bucket *e;
apr_bucket_brigade *input_brigade;
apr_bucket_brigade *output_brigade;
ajp_msg_t *msg;
apr_size_t bufsiz = 0;
char *buff;
char *send_body_chunk_buff;
apr_uint16_t size;
apr_byte_t conn_reuse = 0;
const char *tenc;
int havebody = 1;
int client_failed = 0;
int backend_failed = 0;
apr_off_t bb_len;
int data_sent = 0;
int request_ended = 0;
int headers_sent = 0;
int rv = OK;
apr_int32_t conn_poll_fd;
apr_pollfd_t *conn_poll;
proxy_server_conf *psf =
ap_get_module_config(r->server->module_config, &proxy_module);
apr_size_t maxsize = AJP_MSG_BUFFER_SZ;
int send_body = 0;
apr_off_t content_length = 0;
int original_status = r->status;
const char *original_status_line = r->status_line;
if (psf->io_buffer_size_set)
maxsize = psf->io_buffer_size;
if (maxsize > AJP_MAX_BUFFER_SZ)
maxsize = AJP_MAX_BUFFER_SZ;
else if (maxsize < AJP_MSG_BUFFER_SZ)
maxsize = AJP_MSG_BUFFER_SZ;
maxsize = APR_ALIGN(maxsize, 1024);
status = ajp_send_header(conn->sock, r, maxsize, uri);
if (status != APR_SUCCESS) {
conn->close = 1;
ap_log_rerror(APLOG_MARK, APLOG_ERR, status, r, APLOGNO(00868)
"request failed to %pI (%s)",
conn->worker->cp->addr,
conn->worker->s->hostname);
if (status == AJP_EOVERFLOW)
return HTTP_BAD_REQUEST;
else if (status == AJP_EBAD_METHOD) {
return HTTP_NOT_IMPLEMENTED;
} else {
if (is_idempotent(r) == METHOD_IDEMPOTENT) {
return HTTP_SERVICE_UNAVAILABLE;
}
return HTTP_INTERNAL_SERVER_ERROR;
}
}
bufsiz = maxsize;
status = ajp_alloc_data_msg(r->pool, &buff, &bufsiz, &msg);
if (status != APR_SUCCESS) {
conn->close = 1;
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(00869)
"ajp_alloc_data_msg failed");
return HTTP_INTERNAL_SERVER_ERROR;
}
input_brigade = apr_brigade_create(p, r->connection->bucket_alloc);
tenc = apr_table_get(r->headers_in, "Transfer-Encoding");
if (tenc && (strcasecmp(tenc, "chunked") == 0)) {
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(00870) "request is chunked");
} else {
content_length = get_content_length(r);
status = ap_get_brigade(r->input_filters, input_brigade,
AP_MODE_READBYTES, APR_BLOCK_READ,
maxsize - AJP_HEADER_SZ);
if (status != APR_SUCCESS) {
conn->close = 1;
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, status, r, APLOGNO(00871)
"ap_get_brigade failed");
apr_brigade_destroy(input_brigade);
return ap_map_http_request_error(status, HTTP_BAD_REQUEST);
}
if (APR_BUCKET_IS_EOS(APR_BRIGADE_LAST(input_brigade))) {
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(00872) "APR_BUCKET_IS_EOS");
}
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(00873)
"data to read (max %" APR_SIZE_T_FMT
" at %" APR_SIZE_T_FMT ")", bufsiz, msg->pos);
status = apr_brigade_flatten(input_brigade, buff, &bufsiz);
if (status != APR_SUCCESS) {
conn->close = 1;
apr_brigade_destroy(input_brigade);
ap_log_rerror(APLOG_MARK, APLOG_ERR, status, r, APLOGNO(00874)
"apr_brigade_flatten");
return HTTP_INTERNAL_SERVER_ERROR;
}
apr_brigade_cleanup(input_brigade);
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(00875)
"got %" APR_SIZE_T_FMT " bytes of data", bufsiz);
if (bufsiz > 0) {
status = ajp_send_data_msg(conn->sock, msg, bufsiz);
ajp_msg_log(r, msg, "First ajp_send_data_msg: ajp_ilink_send packet dump");
if (status != APR_SUCCESS) {
conn->close = 1;
apr_brigade_destroy(input_brigade);
ap_log_rerror(APLOG_MARK, APLOG_ERR, status, r, APLOGNO(00876)
"send failed to %pI (%s)",
conn->worker->cp->addr,
conn->worker->s->hostname);
return HTTP_INTERNAL_SERVER_ERROR;
}
conn->worker->s->transferred += bufsiz;
send_body = 1;
} else if (content_length > 0) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, status, r, APLOGNO(00877)
"read zero bytes, expecting"
" %" APR_OFF_T_FMT " bytes",
content_length);
conn->close = 1;
return HTTP_BAD_REQUEST;
}
}
conn->data = NULL;
status = ajp_read_header(conn->sock, r, maxsize,
(ajp_msg_t **)&(conn->data));
if (status != APR_SUCCESS) {
conn->close = 1;
apr_brigade_destroy(input_brigade);
ap_log_rerror(APLOG_MARK, APLOG_ERR, status, r, APLOGNO(00878)
"read response failed from %pI (%s)",
conn->worker->cp->addr,
conn->worker->s->hostname);
if (APR_STATUS_IS_TIMEUP(status) && conn->worker->s->ping_timeout_set) {
return HTTP_GATEWAY_TIME_OUT;
}
if (!send_body && (is_idempotent(r) == METHOD_IDEMPOTENT)) {
return HTTP_SERVICE_UNAVAILABLE;
}
return HTTP_INTERNAL_SERVER_ERROR;
}
result = ajp_parse_type(r, conn->data);
output_brigade = apr_brigade_create(p, r->connection->bucket_alloc);
conn_poll = apr_pcalloc(p, sizeof(apr_pollfd_t));
conn_poll->reqevents = APR_POLLIN;
conn_poll->desc_type = APR_POLL_SOCKET;
conn_poll->desc.s = conn->sock;
bufsiz = maxsize;
for (;;) {
switch (result) {
case CMD_AJP13_GET_BODY_CHUNK:
if (havebody) {
if (APR_BUCKET_IS_EOS(APR_BRIGADE_LAST(input_brigade))) {
bufsiz = 0;
havebody = 0;
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, status, r, APLOGNO(00879)
"APR_BUCKET_IS_EOS");
} else {
status = ap_get_brigade(r->input_filters, input_brigade,
AP_MODE_READBYTES,
APR_BLOCK_READ,
maxsize - AJP_HEADER_SZ);
if (status != APR_SUCCESS) {
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, status, r, APLOGNO(00880)
"ap_get_brigade failed");
if (APR_STATUS_IS_TIMEUP(status)) {
rv = HTTP_REQUEST_TIME_OUT;
} else if (status == AP_FILTER_ERROR) {
rv = AP_FILTER_ERROR;
}
client_failed = 1;
break;
}
bufsiz = maxsize;
status = apr_brigade_flatten(input_brigade, buff,
&bufsiz);
apr_brigade_cleanup(input_brigade);
if (status != APR_SUCCESS) {
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, status, r, APLOGNO(00881)
"apr_brigade_flatten failed");
rv = HTTP_INTERNAL_SERVER_ERROR;
client_failed = 1;
break;
}
}
ajp_msg_reset(msg);
status = ajp_send_data_msg(conn->sock, msg, bufsiz);
ajp_msg_log(r, msg, "ajp_send_data_msg after CMD_AJP13_GET_BODY_CHUNK: ajp_ilink_send packet dump");
if (status != APR_SUCCESS) {
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, status, r, APLOGNO(00882)
"ajp_send_data_msg failed");
backend_failed = 1;
break;
}
conn->worker->s->transferred += bufsiz;
} else {
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(00883)
"ap_proxy_ajp_request error read after end");
backend_failed = 1;
}
break;
case CMD_AJP13_SEND_HEADERS:
if (headers_sent) {
backend_failed = 1;
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(00884)
"Backend sent headers twice.");
break;
}
status = ajp_parse_header(r, conf, conn->data);
if (status != APR_SUCCESS) {
backend_failed = 1;
} else if ((r->status == 401) && conf->error_override) {
const char *buf;
const char *wa = "WWW-Authenticate";
if ((buf = apr_table_get(r->headers_out, wa))) {
apr_table_set(r->err_headers_out, wa, buf);
} else {
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(00885)
"ap_proxy_ajp_request: origin server "
"sent 401 without WWW-Authenticate header");
}
}
headers_sent = 1;
break;
case CMD_AJP13_SEND_BODY_CHUNK:
status = ajp_parse_data(r, conn->data, &size, &send_body_chunk_buff);
if (status == APR_SUCCESS) {
if (!conf->error_override || !ap_is_HTTP_ERROR(r->status)) {
if (size == 0) {
if (headers_sent) {
e = apr_bucket_flush_create(r->connection->bucket_alloc);
APR_BRIGADE_INSERT_TAIL(output_brigade, e);
} else {
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(00886)
"Ignoring flush message "
"received before headers");
}
} else {
apr_status_t rv;
if (conf->error_override && !ap_is_HTTP_ERROR(r->status)
&& ap_is_HTTP_ERROR(original_status)) {
r->status = original_status;
r->status_line = original_status_line;
}
e = apr_bucket_transient_create(send_body_chunk_buff, size,
r->connection->bucket_alloc);
APR_BRIGADE_INSERT_TAIL(output_brigade, e);
if ((conn->worker->s->flush_packets == flush_on) ||
((conn->worker->s->flush_packets == flush_auto) &&
((rv = apr_poll(conn_poll, 1, &conn_poll_fd,
conn->worker->s->flush_wait))
!= APR_SUCCESS) &&
APR_STATUS_IS_TIMEUP(rv))) {
e = apr_bucket_flush_create(r->connection->bucket_alloc);
APR_BRIGADE_INSERT_TAIL(output_brigade, e);
}
apr_brigade_length(output_brigade, 0, &bb_len);
if (bb_len != -1)
conn->worker->s->read += bb_len;
}
if (headers_sent) {
if (ap_pass_brigade(r->output_filters,
output_brigade) != APR_SUCCESS) {
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(00887)
"error processing body.%s",
r->connection->aborted ?
" Client aborted connection." : "");
client_failed = 1;
}
data_sent = 1;
apr_brigade_cleanup(output_brigade);
}
}
} else {
backend_failed = 1;
}
break;
case CMD_AJP13_END_RESPONSE:
status = ajp_parse_reuse(r, conn->data, &conn_reuse);
if (status != APR_SUCCESS) {
backend_failed = 1;
}
if (!conf->error_override || !ap_is_HTTP_ERROR(r->status)) {
e = apr_bucket_eos_create(r->connection->bucket_alloc);
APR_BRIGADE_INSERT_TAIL(output_brigade, e);
if (ap_pass_brigade(r->output_filters,
output_brigade) != APR_SUCCESS) {
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(00888)
"error processing end");
client_failed = 1;
}
data_sent = 1;
}
request_ended = 1;
break;
default:
backend_failed = 1;
break;
}
if (r->connection->aborted) {
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(02821)
"client connection aborted");
if (!headers_sent) {
r->status = HTTP_BAD_REQUEST;
}
client_failed = 1;
data_sent = 1;
break;
}
if ((result == CMD_AJP13_END_RESPONSE)
|| backend_failed || client_failed)
break;
status = ajp_read_header(conn->sock, r, maxsize,
(ajp_msg_t **)&(conn->data));
if (status != APR_SUCCESS) {
backend_failed = 1;
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, status, r, APLOGNO(00889)
"ajp_read_header failed");
break;
}
result = ajp_parse_type(r, conn->data);
}
apr_brigade_destroy(input_brigade);
apr_brigade_cleanup(output_brigade);
if (backend_failed || client_failed) {
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(00890)
"Processing of request failed backend: %i, client: %i",
backend_failed, client_failed);
conn->close = 1;
if (data_sent) {
rv = DONE;
}
} else if (!request_ended) {
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(00891)
"Processing of request didn't terminate cleanly");
conn->close = 1;
backend_failed = 1;
if (data_sent) {
rv = DONE;
}
} else if (!conn_reuse) {
conn->close = 1;
} else {
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(00892)
"got response from %pI (%s)",
conn->worker->cp->addr,
conn->worker->s->hostname);
if (conf->error_override && ap_is_HTTP_ERROR(r->status)) {
rv = r->status;
r->status = HTTP_OK;
apr_table_setn(r->notes, "proxy-error-override", "1");
} else {
rv = OK;
}
}
if (backend_failed) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, status, r, APLOGNO(00893)
"dialog to %pI (%s) failed",
conn->worker->cp->addr,
conn->worker->s->hostname);
if (data_sent) {
ap_proxy_backend_broke(r, output_brigade);
} else if (!send_body && (is_idempotent(r) == METHOD_IDEMPOTENT)) {
rv = HTTP_SERVICE_UNAVAILABLE;
} else {
rv = HTTP_INTERNAL_SERVER_ERROR;
}
} else if (client_failed) {
int level = (r->connection->aborted) ? APLOG_DEBUG : APLOG_ERR;
ap_log_rerror(APLOG_MARK, level, status, r, APLOGNO(02822)
"dialog with client %pI failed",
r->connection->client_addr);
if (rv == OK) {
rv = HTTP_BAD_REQUEST;
}
}
if (data_sent && !r->eos_sent && !r->connection->aborted
&& APR_BRIGADE_EMPTY(output_brigade)) {
e = apr_bucket_eos_create(r->connection->bucket_alloc);
APR_BRIGADE_INSERT_TAIL(output_brigade, e);
}
if (!APR_BRIGADE_EMPTY(output_brigade)
&& ap_pass_brigade(r->output_filters, output_brigade) != APR_SUCCESS) {
rv = AP_FILTER_ERROR;
}
apr_brigade_destroy(output_brigade);
if (apr_table_get(r->subprocess_env, "proxy-nokeepalive")) {
conn->close = 1;
}
return rv;
}
static int proxy_ajp_handler(request_rec *r, proxy_worker *worker,
proxy_server_conf *conf,
char *url, const char *proxyname,
apr_port_t proxyport) {
int status;
char server_portstr[32];
conn_rec *origin = NULL;
proxy_conn_rec *backend = NULL;
const char *scheme = "AJP";
int retry;
proxy_dir_conf *dconf = ap_get_module_config(r->per_dir_config,
&proxy_module);
apr_pool_t *p = r->pool;
apr_uri_t *uri;
if (strncasecmp(url, "ajp:", 4) != 0) {
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(00894) "declining URL %s", url);
return DECLINED;
}
uri = apr_palloc(p, sizeof(*uri));
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(00895) "serving URL %s", url);
status = ap_proxy_acquire_connection(scheme, &backend, worker,
r->server);
if (status != OK) {
if (backend) {
backend->close = 1;
ap_proxy_release_connection(scheme, backend, r->server);
}
return status;
}
backend->is_ssl = 0;
backend->close = 0;
retry = 0;
while (retry < 2) {
char *locurl = url;
status = ap_proxy_determine_connection(p, r, conf, worker, backend,
uri, &locurl, proxyname, proxyport,
server_portstr,
sizeof(server_portstr));
if (status != OK)
break;
if (ap_proxy_check_connection(scheme, backend, r->server, 0,
PROXY_CHECK_CONN_EMPTY)
&& ap_proxy_connect_backend(scheme, backend, worker,
r->server)) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(00896)
"failed to make connection to backend: %s",
backend->hostname);
status = HTTP_SERVICE_UNAVAILABLE;
break;
}
if (worker->s->ping_timeout_set) {
status = ajp_handle_cping_cpong(backend->sock, r,
worker->s->ping_timeout);
if (status != APR_SUCCESS) {
backend->close = 1;
ap_log_rerror(APLOG_MARK, APLOG_ERR, status, r, APLOGNO(00897)
"cping/cpong failed to %pI (%s)",
worker->cp->addr, worker->s->hostname);
status = HTTP_SERVICE_UNAVAILABLE;
retry++;
continue;
}
}
status = ap_proxy_ajp_request(p, r, backend, origin, dconf, uri, locurl,
server_portstr);
break;
}
ap_proxy_release_connection(scheme, backend, r->server);
return status;
}
static void ap_proxy_http_register_hook(apr_pool_t *p) {
proxy_hook_scheme_handler(proxy_ajp_handler, NULL, NULL, APR_HOOK_FIRST);
proxy_hook_canon_handler(proxy_ajp_canon, NULL, NULL, APR_HOOK_FIRST);
}
AP_DECLARE_MODULE(proxy_ajp) = {
STANDARD20_MODULE_STUFF,
NULL,
NULL,
NULL,
NULL,
NULL,
ap_proxy_http_register_hook
};
