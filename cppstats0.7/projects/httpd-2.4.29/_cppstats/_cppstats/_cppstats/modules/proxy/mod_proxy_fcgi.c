#include "mod_proxy.h"
#include "util_fcgi.h"
#include "util_script.h"
#include "ap_expr.h"
module AP_MODULE_DECLARE_DATA proxy_fcgi_module;
typedef struct {
ap_expr_info_t *cond;
ap_expr_info_t *subst;
const char *envname;
} sei_entry;
typedef struct {
int need_dirwalk;
} fcgi_req_config_t;
typedef enum {
BACKEND_DEFAULT_UNKNOWN = 0,
BACKEND_FPM,
BACKEND_GENERIC,
} fcgi_backend_t;
#define FCGI_MAY_BE_FPM(dconf) (dconf && ((dconf->backend_type == BACKEND_DEFAULT_UNKNOWN) || (dconf->backend_type == BACKEND_FPM)))
typedef struct {
fcgi_backend_t backend_type;
apr_array_header_t *env_fixups;
} fcgi_dirconf_t;
static int proxy_fcgi_canon(request_rec *r, char *url) {
char *host, sport[7];
const char *err;
char *path;
apr_port_t port, def_port;
fcgi_req_config_t *rconf = NULL;
const char *pathinfo_type = NULL;
if (ap_cstr_casecmpn(url, "fcgi:", 5) == 0) {
url += 5;
} else {
return DECLINED;
}
port = def_port = ap_proxy_port_of_scheme("fcgi");
ap_log_rerror(APLOG_MARK, APLOG_TRACE1, 0, r,
"canonicalising URL %s", url);
err = ap_proxy_canon_netloc(r->pool, &url, NULL, NULL, &host, &port);
if (err) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(01059)
"error parsing URL %s: %s", url, err);
return HTTP_BAD_REQUEST;
}
if (port != def_port)
apr_snprintf(sport, sizeof(sport), ":%d", port);
else
sport[0] = '\0';
if (ap_strchr_c(host, ':')) {
host = apr_pstrcat(r->pool, "[", host, "]", NULL);
}
if (apr_table_get(r->notes, "proxy-nocanon")) {
path = url;
} else {
path = ap_proxy_canonenc(r->pool, url, strlen(url), enc_path, 0,
r->proxyreq);
}
if (path == NULL)
return HTTP_BAD_REQUEST;
r->filename = apr_pstrcat(r->pool, "proxy:fcgi://", host, sport, "/",
path, NULL);
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(01060)
"set r->filename to %s", r->filename);
rconf = ap_get_module_config(r->request_config, &proxy_fcgi_module);
if (rconf == NULL) {
rconf = apr_pcalloc(r->pool, sizeof(fcgi_req_config_t));
ap_set_module_config(r->request_config, &proxy_fcgi_module, rconf);
}
if (NULL != (pathinfo_type = apr_table_get(r->subprocess_env, "proxy-fcgi-pathinfo"))) {
if (!strcasecmp(pathinfo_type, "full")) {
rconf->need_dirwalk = 1;
ap_unescape_url_keep2f(path, 0);
} else if (!strcasecmp(pathinfo_type, "first-dot")) {
char *split = ap_strchr(path, '.');
if (split) {
char *slash = ap_strchr(split, '/');
if (slash) {
r->path_info = apr_pstrdup(r->pool, slash);
ap_unescape_url_keep2f(r->path_info, 0);
*slash = '\0';
}
}
} else if (!strcasecmp(pathinfo_type, "last-dot")) {
char *split = ap_strrchr(path, '.');
if (split) {
char *slash = ap_strchr(split, '/');
if (slash) {
r->path_info = apr_pstrdup(r->pool, slash);
ap_unescape_url_keep2f(r->path_info, 0);
*slash = '\0';
}
}
} else {
r->path_info = apr_pstrcat(r->pool, "/", path, NULL);
if (!strcasecmp(pathinfo_type, "unescape")) {
ap_unescape_url_keep2f(r->path_info, 0);
}
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(01061)
"set r->path_info to %s", r->path_info);
}
}
return OK;
}
static void fix_cgivars(request_rec *r, fcgi_dirconf_t *dconf) {
sei_entry *entries;
const char *err, *src;
int i = 0, rc = 0;
ap_regmatch_t regm[AP_MAX_REG_MATCH];
entries = (sei_entry *) dconf->env_fixups->elts;
for (i = 0; i < dconf->env_fixups->nelts; i++) {
sei_entry *entry = &entries[i];
if (entry->envname[0] == '!') {
apr_table_unset(r->subprocess_env, entry->envname+1);
} else if (0 < (rc = ap_expr_exec_re(r, entry->cond, AP_MAX_REG_MATCH, regm, &src, &err))) {
const char *val = ap_expr_str_exec_re(r, entry->subst, AP_MAX_REG_MATCH, regm, &src, &err);
if (err) {
ap_log_rerror(APLOG_MARK, APLOG_INFO, 0, r, APLOGNO(03514)
"Error evaluating expression for replacement of %s: '%s'",
entry->envname, err);
continue;
}
if (APLOGrtrace4(r)) {
const char *oldval = apr_table_get(r->subprocess_env, entry->envname);
ap_log_rerror(APLOG_MARK, APLOG_TRACE4, 0, r,
"fix_cgivars: override %s from '%s' to '%s'",
entry->envname, oldval, val);
}
apr_table_setn(r->subprocess_env, entry->envname, val);
} else {
ap_log_rerror(APLOG_MARK, APLOG_TRACE8, 0, r, "fix_cgivars: Condition returned %d", rc);
}
}
}
static apr_status_t send_data(proxy_conn_rec *conn,
struct iovec *vec,
int nvec,
apr_size_t *len) {
apr_status_t rv = APR_SUCCESS;
apr_size_t written = 0, to_write = 0;
int i, offset;
apr_socket_t *s = conn->sock;
for (i = 0; i < nvec; i++) {
to_write += vec[i].iov_len;
}
offset = 0;
while (to_write) {
apr_size_t n = 0;
rv = apr_socket_sendv(s, vec + offset, nvec - offset, &n);
if (rv != APR_SUCCESS) {
break;
}
if (n > 0) {
written += n;
if (written >= to_write)
break;
for (i = offset; i < nvec; ) {
if (n >= vec[i].iov_len) {
offset++;
n -= vec[i++].iov_len;
} else {
vec[i].iov_len -= n;
vec[i].iov_base = (char *) vec[i].iov_base + n;
break;
}
}
}
}
conn->worker->s->transferred += written;
*len = written;
return rv;
}
static apr_status_t get_data(proxy_conn_rec *conn,
char *buffer,
apr_size_t *buflen) {
apr_status_t rv = apr_socket_recv(conn->sock, buffer, buflen);
if (rv == APR_SUCCESS) {
conn->worker->s->read += *buflen;
}
return rv;
}
static apr_status_t get_data_full(proxy_conn_rec *conn,
char *buffer,
apr_size_t buflen) {
apr_size_t readlen;
apr_size_t cumulative_len = 0;
apr_status_t rv;
do {
readlen = buflen - cumulative_len;
rv = get_data(conn, buffer + cumulative_len, &readlen);
if (rv != APR_SUCCESS) {
return rv;
}
cumulative_len += readlen;
} while (cumulative_len < buflen);
return APR_SUCCESS;
}
static apr_status_t send_begin_request(proxy_conn_rec *conn,
apr_uint16_t request_id) {
struct iovec vec[2];
ap_fcgi_header header;
unsigned char farray[AP_FCGI_HEADER_LEN];
ap_fcgi_begin_request_body brb;
unsigned char abrb[AP_FCGI_HEADER_LEN];
apr_size_t len;
ap_fcgi_fill_in_header(&header, AP_FCGI_BEGIN_REQUEST, request_id,
sizeof(abrb), 0);
ap_fcgi_fill_in_request_body(&brb, AP_FCGI_RESPONDER,
ap_proxy_connection_reusable(conn)
? AP_FCGI_KEEP_CONN : 0);
ap_fcgi_header_to_array(&header, farray);
ap_fcgi_begin_request_body_to_array(&brb, abrb);
vec[0].iov_base = (void *)farray;
vec[0].iov_len = sizeof(farray);
vec[1].iov_base = (void *)abrb;
vec[1].iov_len = sizeof(abrb);
return send_data(conn, vec, 2, &len);
}
static apr_status_t send_environment(proxy_conn_rec *conn, request_rec *r,
apr_pool_t *temp_pool,
apr_uint16_t request_id) {
const apr_array_header_t *envarr;
const apr_table_entry_t *elts;
struct iovec vec[2];
ap_fcgi_header header;
unsigned char farray[AP_FCGI_HEADER_LEN];
char *body;
apr_status_t rv;
apr_size_t avail_len, len, required_len;
int next_elem, starting_elem;
fcgi_req_config_t *rconf = ap_get_module_config(r->request_config, &proxy_fcgi_module);
fcgi_dirconf_t *dconf = ap_get_module_config(r->per_dir_config, &proxy_fcgi_module);
if (rconf) {
if (rconf->need_dirwalk) {
ap_directory_walk(r);
}
}
if (r->filename) {
char *newfname = NULL;
if (!strncmp(r->filename, "proxy:balancer://", 17)) {
newfname = apr_pstrdup(r->pool, r->filename+17);
}
if (!FCGI_MAY_BE_FPM(dconf)) {
if (!strncmp(r->filename, "proxy:fcgi://", 13)) {
newfname = apr_pstrdup(r->pool, r->filename+13);
}
if (newfname && r->args && *r->args) {
char *qs = strrchr(newfname, '?');
if (qs && !strcmp(qs+1, r->args)) {
*qs = '\0';
}
}
}
if (newfname) {
newfname = ap_strchr(newfname, '/');
r->filename = newfname;
}
}
ap_add_common_vars(r);
ap_add_cgi_vars(r);
fix_cgivars(r, dconf);
envarr = apr_table_elts(r->subprocess_env);
elts = (const apr_table_entry_t *) envarr->elts;
if (APLOGrtrace8(r)) {
int i;
for (i = 0; i < envarr->nelts; ++i) {
ap_log_rerror(APLOG_MARK, APLOG_TRACE8, 0, r, APLOGNO(01062)
"sending env var '%s' value '%s'",
elts[i].key, elts[i].val);
}
}
next_elem = 0;
avail_len = 16 * 1024;
while (next_elem < envarr->nelts) {
starting_elem = next_elem;
required_len = ap_fcgi_encoded_env_len(r->subprocess_env,
avail_len,
&next_elem);
if (!required_len) {
if (next_elem < envarr->nelts) {
ap_log_rerror(APLOG_MARK, APLOG_WARNING, 0, r,
APLOGNO(02536) "couldn't encode envvar '%s' in %"
APR_SIZE_T_FMT " bytes",
elts[next_elem].key, avail_len);
++next_elem;
continue;
}
break;
}
body = apr_palloc(temp_pool, required_len);
rv = ap_fcgi_encode_env(r, r->subprocess_env, body, required_len,
&starting_elem);
ap_assert(rv == APR_SUCCESS);
ap_assert(starting_elem == next_elem);
ap_fcgi_fill_in_header(&header, AP_FCGI_PARAMS, request_id,
(apr_uint16_t)required_len, 0);
ap_fcgi_header_to_array(&header, farray);
vec[0].iov_base = (void *)farray;
vec[0].iov_len = sizeof(farray);
vec[1].iov_base = body;
vec[1].iov_len = required_len;
rv = send_data(conn, vec, 2, &len);
apr_pool_clear(temp_pool);
if (rv) {
return rv;
}
}
ap_fcgi_fill_in_header(&header, AP_FCGI_PARAMS, request_id, 0, 0);
ap_fcgi_header_to_array(&header, farray);
vec[0].iov_base = (void *)farray;
vec[0].iov_len = sizeof(farray);
return send_data(conn, vec, 1, &len);
}
enum {
HDR_STATE_READING_HEADERS,
HDR_STATE_GOT_CR,
HDR_STATE_GOT_CRLF,
HDR_STATE_GOT_CRLFCR,
HDR_STATE_GOT_LF,
HDR_STATE_DONE_WITH_HEADERS
};
static int handle_headers(request_rec *r, int *state,
const char *readbuf, apr_size_t readlen) {
const char *itr = readbuf;
while (readlen--) {
if (*itr == '\r') {
switch (*state) {
case HDR_STATE_GOT_CRLF:
*state = HDR_STATE_GOT_CRLFCR;
break;
default:
*state = HDR_STATE_GOT_CR;
break;
}
} else if (*itr == '\n') {
switch (*state) {
case HDR_STATE_GOT_LF:
*state = HDR_STATE_DONE_WITH_HEADERS;
break;
case HDR_STATE_GOT_CR:
*state = HDR_STATE_GOT_CRLF;
break;
case HDR_STATE_GOT_CRLFCR:
*state = HDR_STATE_DONE_WITH_HEADERS;
break;
default:
*state = HDR_STATE_GOT_LF;
break;
}
} else {
*state = HDR_STATE_READING_HEADERS;
}
if (*state == HDR_STATE_DONE_WITH_HEADERS)
break;
++itr;
}
if (*state == HDR_STATE_DONE_WITH_HEADERS) {
return 1;
}
return 0;
}
static apr_status_t dispatch(proxy_conn_rec *conn, proxy_dir_conf *conf,
request_rec *r, apr_pool_t *setaside_pool,
apr_uint16_t request_id, const char **err,
int *bad_request, int *has_responded) {
apr_bucket_brigade *ib, *ob;
int seen_end_of_headers = 0, done = 0, ignore_body = 0;
apr_status_t rv = APR_SUCCESS;
int script_error_status = HTTP_OK;
conn_rec *c = r->connection;
struct iovec vec[2];
ap_fcgi_header header;
unsigned char farray[AP_FCGI_HEADER_LEN];
apr_pollfd_t pfd;
int header_state = HDR_STATE_READING_HEADERS;
char stack_iobuf[AP_IOBUFSIZE];
apr_size_t iobuf_size = AP_IOBUFSIZE;
char *iobuf = stack_iobuf;
*err = NULL;
if (conn->worker->s->io_buffer_size_set) {
iobuf_size = conn->worker->s->io_buffer_size;
iobuf = apr_palloc(r->pool, iobuf_size);
}
pfd.desc_type = APR_POLL_SOCKET;
pfd.desc.s = conn->sock;
pfd.p = r->pool;
pfd.reqevents = APR_POLLIN | APR_POLLOUT;
ib = apr_brigade_create(r->pool, c->bucket_alloc);
ob = apr_brigade_create(r->pool, c->bucket_alloc);
while (! done) {
apr_interval_time_t timeout;
apr_size_t len;
int n;
apr_socket_timeout_get(conn->sock, &timeout);
rv = apr_poll(&pfd, 1, &n, timeout);
if (rv != APR_SUCCESS) {
if (APR_STATUS_IS_EINTR(rv)) {
continue;
}
*err = "polling";
break;
}
if (pfd.rtnevents & APR_POLLOUT) {
apr_size_t to_send, writebuflen;
int last_stdin = 0;
char *iobuf_cursor;
rv = ap_get_brigade(r->input_filters, ib,
AP_MODE_READBYTES, APR_BLOCK_READ,
iobuf_size);
if (rv != APR_SUCCESS) {
*err = "reading input brigade";
*bad_request = 1;
break;
}
if (APR_BUCKET_IS_EOS(APR_BRIGADE_LAST(ib))) {
last_stdin = 1;
}
writebuflen = iobuf_size;
rv = apr_brigade_flatten(ib, iobuf, &writebuflen);
apr_brigade_cleanup(ib);
if (rv != APR_SUCCESS) {
*err = "flattening brigade";
break;
}
to_send = writebuflen;
iobuf_cursor = iobuf;
while (to_send > 0) {
int nvec = 0;
apr_size_t write_this_time;
write_this_time =
to_send < AP_FCGI_MAX_CONTENT_LEN ? to_send : AP_FCGI_MAX_CONTENT_LEN;
ap_fcgi_fill_in_header(&header, AP_FCGI_STDIN, request_id,
(apr_uint16_t)write_this_time, 0);
ap_fcgi_header_to_array(&header, farray);
vec[nvec].iov_base = (void *)farray;
vec[nvec].iov_len = sizeof(farray);
++nvec;
if (writebuflen) {
vec[nvec].iov_base = iobuf_cursor;
vec[nvec].iov_len = write_this_time;
++nvec;
}
rv = send_data(conn, vec, nvec, &len);
if (rv != APR_SUCCESS) {
*err = "sending stdin";
break;
}
to_send -= write_this_time;
iobuf_cursor += write_this_time;
}
if (rv != APR_SUCCESS) {
break;
}
if (last_stdin) {
pfd.reqevents = APR_POLLIN;
ap_fcgi_fill_in_header(&header, AP_FCGI_STDIN, request_id,
0, 0);
ap_fcgi_header_to_array(&header, farray);
vec[0].iov_base = (void *)farray;
vec[0].iov_len = sizeof(farray);
rv = send_data(conn, vec, 1, &len);
if (rv != APR_SUCCESS) {
*err = "sending empty stdin";
break;
}
}
}
if (pfd.rtnevents & APR_POLLIN) {
apr_size_t readbuflen;
apr_uint16_t clen, rid;
apr_bucket *b;
unsigned char plen;
unsigned char type, version;
rv = get_data_full(conn, (char *) farray, AP_FCGI_HEADER_LEN);
if (rv != APR_SUCCESS) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(01067)
"Failed to read FastCGI header");
break;
}
ap_log_rdata(APLOG_MARK, APLOG_TRACE8, r, "FastCGI header",
farray, AP_FCGI_HEADER_LEN, 0);
ap_fcgi_header_fields_from_array(&version, &type, &rid,
&clen, &plen, farray);
if (version != AP_FCGI_VERSION_1) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(01068)
"Got bogus version %d", (int)version);
rv = APR_EINVAL;
break;
}
if (rid != request_id) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(01069)
"Got bogus rid %d, expected %d",
rid, request_id);
rv = APR_EINVAL;
break;
}
recv_again:
if (clen > iobuf_size) {
readbuflen = iobuf_size;
} else {
readbuflen = clen;
}
if (readbuflen != 0) {
rv = get_data(conn, iobuf, &readbuflen);
if (rv != APR_SUCCESS) {
*err = "reading response body";
break;
}
}
switch (type) {
case AP_FCGI_STDOUT:
if (clen != 0) {
b = apr_bucket_transient_create(iobuf,
readbuflen,
c->bucket_alloc);
APR_BRIGADE_INSERT_TAIL(ob, b);
if (! seen_end_of_headers) {
int st = handle_headers(r, &header_state,
iobuf, readbuflen);
if (st == 1) {
int status;
seen_end_of_headers = 1;
status = ap_scan_script_header_err_brigade_ex(r, ob,
NULL, APLOG_MODULE_INDEX);
if (status != OK) {
apr_bucket *tmp_b;
apr_brigade_cleanup(ob);
tmp_b = apr_bucket_eos_create(c->bucket_alloc);
APR_BRIGADE_INSERT_TAIL(ob, tmp_b);
*has_responded = 1;
r->status = status;
rv = ap_pass_brigade(r->output_filters, ob);
if (rv != APR_SUCCESS) {
*err = "passing headers brigade to output filters";
break;
} else if (status == HTTP_NOT_MODIFIED
|| status == HTTP_PRECONDITION_FAILED) {
ignore_body = 1;
} else {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(01070)
"Error parsing script headers");
rv = APR_EINVAL;
break;
}
}
if (conf->error_override
&& ap_is_HTTP_ERROR(r->status) && ap_is_initial_req(r)) {
script_error_status = r->status;
r->status = HTTP_OK;
}
if (script_error_status == HTTP_OK
&& !APR_BRIGADE_EMPTY(ob) && !ignore_body) {
*has_responded = 1;
rv = ap_pass_brigade(r->output_filters, ob);
if (rv != APR_SUCCESS) {
*err = "passing brigade to output filters";
break;
}
}
apr_brigade_cleanup(ob);
apr_pool_clear(setaside_pool);
} else {
apr_bucket_setaside(b, setaside_pool);
}
} else {
if (script_error_status == HTTP_OK && !ignore_body) {
*has_responded = 1;
rv = ap_pass_brigade(r->output_filters, ob);
if (rv != APR_SUCCESS) {
*err = "passing brigade to output filters";
break;
}
}
apr_brigade_cleanup(ob);
}
if (clen > readbuflen) {
clen -= readbuflen;
goto recv_again;
}
} else {
if (script_error_status == HTTP_OK) {
b = apr_bucket_eos_create(c->bucket_alloc);
APR_BRIGADE_INSERT_TAIL(ob, b);
*has_responded = 1;
rv = ap_pass_brigade(r->output_filters, ob);
if (rv != APR_SUCCESS) {
*err = "passing brigade to output filters";
break;
}
}
}
break;
case AP_FCGI_STDERR:
if (clen) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(01071)
"Got error '%.*s'", (int)readbuflen, iobuf);
}
if (clen > readbuflen) {
clen -= readbuflen;
goto recv_again;
}
break;
case AP_FCGI_END_REQUEST:
done = 1;
break;
default:
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(01072)
"Got bogus record %d", type);
break;
}
if (rv != APR_SUCCESS) {
break;
}
if (plen) {
rv = get_data_full(conn, iobuf, plen);
if (rv != APR_SUCCESS) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, rv, r, APLOGNO(02537)
"Error occurred reading padding");
break;
}
}
}
}
apr_brigade_destroy(ib);
apr_brigade_destroy(ob);
if (script_error_status != HTTP_OK) {
ap_die(script_error_status, r);
*has_responded = 1;
}
return rv;
}
static int fcgi_do_request(apr_pool_t *p, request_rec *r,
proxy_conn_rec *conn,
conn_rec *origin,
proxy_dir_conf *conf,
apr_uri_t *uri,
char *url, char *server_portstr) {
apr_uint16_t request_id = 1;
apr_status_t rv;
apr_pool_t *temp_pool;
const char *err;
int bad_request = 0,
has_responded = 0;
rv = send_begin_request(conn, request_id);
if (rv != APR_SUCCESS) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, rv, r, APLOGNO(01073)
"Failed Writing Request to %s:", server_portstr);
conn->close = 1;
return HTTP_SERVICE_UNAVAILABLE;
}
apr_pool_create(&temp_pool, r->pool);
rv = send_environment(conn, r, temp_pool, request_id);
if (rv != APR_SUCCESS) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, rv, r, APLOGNO(01074)
"Failed writing Environment to %s:", server_portstr);
conn->close = 1;
return HTTP_SERVICE_UNAVAILABLE;
}
rv = dispatch(conn, conf, r, temp_pool, request_id,
&err, &bad_request, &has_responded);
if (rv != APR_SUCCESS) {
if (r->connection->aborted) {
ap_log_rerror(APLOG_MARK, APLOG_TRACE1, rv, r,
"The client aborted the connection.");
conn->close = 1;
return OK;
}
ap_log_rerror(APLOG_MARK, APLOG_ERR, rv, r, APLOGNO(01075)
"Error dispatching request to %s: %s%s%s",
server_portstr,
err ? "(" : "",
err ? err : "",
err ? ")" : "");
conn->close = 1;
if (has_responded) {
return AP_FILTER_ERROR;
}
if (bad_request) {
return ap_map_http_request_error(rv, HTTP_BAD_REQUEST);
}
if (APR_STATUS_IS_TIMEUP(rv)) {
return HTTP_GATEWAY_TIME_OUT;
}
return HTTP_SERVICE_UNAVAILABLE;
}
return OK;
}
#define FCGI_SCHEME "FCGI"
static int proxy_fcgi_handler(request_rec *r, proxy_worker *worker,
proxy_server_conf *conf,
char *url, const char *proxyname,
apr_port_t proxyport) {
int status;
char server_portstr[32];
conn_rec *origin = NULL;
proxy_conn_rec *backend = NULL;
apr_uri_t *uri;
proxy_dir_conf *dconf = ap_get_module_config(r->per_dir_config,
&proxy_module);
apr_pool_t *p = r->pool;
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(01076)
"url: %s proxyname: %s proxyport: %d",
url, proxyname, proxyport);
if (ap_cstr_casecmpn(url, "fcgi:", 5) != 0) {
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(01077) "declining URL %s", url);
return DECLINED;
}
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(01078) "serving URL %s", url);
status = ap_proxy_acquire_connection(FCGI_SCHEME, &backend, worker,
r->server);
if (status != OK) {
if (backend) {
backend->close = 1;
ap_proxy_release_connection(FCGI_SCHEME, backend, r->server);
}
return status;
}
backend->is_ssl = 0;
uri = apr_palloc(p, sizeof(*uri));
status = ap_proxy_determine_connection(p, r, conf, worker, backend,
uri, &url, proxyname, proxyport,
server_portstr,
sizeof(server_portstr));
if (status != OK) {
goto cleanup;
}
backend->close = 1;
if (worker->s->disablereuse_set && !worker->s->disablereuse) {
backend->close = 0;
}
if (ap_proxy_check_connection(FCGI_SCHEME, backend, r->server, 0,
PROXY_CHECK_CONN_EMPTY)
&& ap_proxy_connect_backend(FCGI_SCHEME, backend, worker,
r->server)) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(01079)
"failed to make connection to backend: %s",
backend->hostname);
status = HTTP_SERVICE_UNAVAILABLE;
goto cleanup;
}
status = fcgi_do_request(p, r, backend, origin, dconf, uri, url,
server_portstr);
cleanup:
ap_proxy_release_connection(FCGI_SCHEME, backend, r->server);
return status;
}
static void *fcgi_create_dconf(apr_pool_t *p, char *path) {
fcgi_dirconf_t *a;
a = (fcgi_dirconf_t *)apr_pcalloc(p, sizeof(fcgi_dirconf_t));
a->backend_type = BACKEND_DEFAULT_UNKNOWN;
a->env_fixups = apr_array_make(p, 20, sizeof(sei_entry));
return a;
}
static void *fcgi_merge_dconf(apr_pool_t *p, void *basev, void *overridesv) {
fcgi_dirconf_t *a, *base, *over;
a = (fcgi_dirconf_t *)apr_pcalloc(p, sizeof(fcgi_dirconf_t));
base = (fcgi_dirconf_t *)basev;
over = (fcgi_dirconf_t *)overridesv;
a->backend_type = (over->backend_type != BACKEND_DEFAULT_UNKNOWN)
? over->backend_type
: base->backend_type;
a->env_fixups = apr_array_append(p, base->env_fixups, over->env_fixups);
return a;
}
static const char *cmd_servertype(cmd_parms *cmd, void *in_dconf,
const char *val) {
fcgi_dirconf_t *dconf = in_dconf;
if (!strcasecmp(val, "GENERIC")) {
dconf->backend_type = BACKEND_GENERIC;
} else if (!strcasecmp(val, "FPM")) {
dconf->backend_type = BACKEND_FPM;
} else {
return "ProxyFCGIBackendType requires one of the following arguments: "
"'GENERIC', 'FPM'";
}
return NULL;
}
static const char *cmd_setenv(cmd_parms *cmd, void *in_dconf,
const char *arg1, const char *arg2,
const char *arg3) {
fcgi_dirconf_t *dconf = in_dconf;
const char *err;
sei_entry *new;
const char *envvar = arg2;
new = apr_array_push(dconf->env_fixups);
new->cond = ap_expr_parse_cmd(cmd, arg1, 0, &err, NULL);
if (err) {
return apr_psprintf(cmd->pool, "Could not parse expression \"%s\": %s",
arg1, err);
}
if (envvar[0] == '!') {
if (arg3) {
return apr_psprintf(cmd->pool, "Third argument (\"%s\") is not "
"allowed when using ProxyFCGISetEnvIf's unset "
"mode (%s)", arg3, envvar);
} else if (!envvar[1]) {
return "ProxyFCGISetEnvIf: \"!\" is not a valid variable name";
}
new->subst = NULL;
} else {
if (!arg3) {
arg3 = "";
}
new->subst = ap_expr_parse_cmd(cmd, arg3, AP_EXPR_FLAG_STRING_RESULT, &err, NULL);
if (err) {
return apr_psprintf(cmd->pool, "Could not parse expression \"%s\": %s",
arg3, err);
}
}
new->envname = envvar;
return NULL;
}
static void register_hooks(apr_pool_t *p) {
proxy_hook_scheme_handler(proxy_fcgi_handler, NULL, NULL, APR_HOOK_FIRST);
proxy_hook_canon_handler(proxy_fcgi_canon, NULL, NULL, APR_HOOK_FIRST);
}
static const command_rec command_table[] = {
AP_INIT_TAKE1("ProxyFCGIBackendType", cmd_servertype, NULL, OR_FILEINFO,
"Specify the type of FastCGI server: 'Generic', 'FPM'"),
AP_INIT_TAKE23("ProxyFCGISetEnvIf", cmd_setenv, NULL, OR_FILEINFO,
"expr-condition env-name expr-value"),
{ NULL }
};
AP_DECLARE_MODULE(proxy_fcgi) = {
STANDARD20_MODULE_STUFF,
fcgi_create_dconf,
fcgi_merge_dconf,
NULL,
NULL,
command_table,
register_hooks
};
