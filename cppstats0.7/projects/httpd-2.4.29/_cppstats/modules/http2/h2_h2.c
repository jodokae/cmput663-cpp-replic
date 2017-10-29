#include <assert.h>
#include <apr_strings.h>
#include <apr_optional.h>
#include <apr_optional_hooks.h>
#include <httpd.h>
#include <http_core.h>
#include <http_config.h>
#include <http_connection.h>
#include <http_protocol.h>
#include <http_request.h>
#include <http_log.h>
#include "mod_ssl.h"
#include "mod_http2.h"
#include "h2_private.h"
#include "h2_bucket_beam.h"
#include "h2_stream.h"
#include "h2_task.h"
#include "h2_config.h"
#include "h2_ctx.h"
#include "h2_conn.h"
#include "h2_filter.h"
#include "h2_request.h"
#include "h2_headers.h"
#include "h2_session.h"
#include "h2_util.h"
#include "h2_h2.h"
#include "mod_http2.h"
const char *h2_tls_protos[] = {
"h2", NULL
};
const char *h2_clear_protos[] = {
"h2c", NULL
};
const char *H2_MAGIC_TOKEN = "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n";
static APR_OPTIONAL_FN_TYPE(ssl_engine_disable) *opt_ssl_engine_disable;
static APR_OPTIONAL_FN_TYPE(ssl_is_https) *opt_ssl_is_https;
static APR_OPTIONAL_FN_TYPE(ssl_var_lookup) *opt_ssl_var_lookup;
static const char *h2_err_descr[] = {
"no error",
"protocol error",
"internal error",
"flow control error",
"settings timeout",
"stream closed",
"frame size error",
"refused stream",
"cancel",
"compression error",
"connect error",
"enhance your calm",
"inadequate security",
"http/1.1 required",
};
const char *h2_h2_err_description(unsigned int h2_error) {
if (h2_error < (sizeof(h2_err_descr)/sizeof(h2_err_descr[0]))) {
return h2_err_descr[h2_error];
}
return "unknown http/2 error code";
}
static const char *RFC7540_names[] = {
"NULL-MD5",
"NULL-SHA",
"NULL-SHA256",
"PSK-NULL-SHA",
"DHE-PSK-NULL-SHA",
"RSA-PSK-NULL-SHA",
"PSK-NULL-SHA256",
"PSK-NULL-SHA384",
"DHE-PSK-NULL-SHA256",
"DHE-PSK-NULL-SHA384",
"RSA-PSK-NULL-SHA256",
"RSA-PSK-NULL-SHA384",
"ECDH-ECDSA-NULL-SHA",
"ECDHE-ECDSA-NULL-SHA",
"ECDH-RSA-NULL-SHA",
"ECDHE-RSA-NULL-SHA",
"AECDH-NULL-SHA",
"ECDHE-PSK-NULL-SHA",
"ECDHE-PSK-NULL-SHA256",
"ECDHE-PSK-NULL-SHA384",
"PSK-3DES-EDE-CBC-SHA",
"DHE-PSK-3DES-EDE-CBC-SHA",
"RSA-PSK-3DES-EDE-CBC-SHA",
"ECDH-ECDSA-DES-CBC3-SHA",
"ECDHE-ECDSA-DES-CBC3-SHA",
"ECDH-RSA-DES-CBC3-SHA",
"ECDHE-RSA-DES-CBC3-SHA",
"AECDH-DES-CBC3-SHA",
"SRP-3DES-EDE-CBC-SHA",
"SRP-RSA-3DES-EDE-CBC-SHA",
"SRP-DSS-3DES-EDE-CBC-SHA",
"ECDHE-PSK-3DES-EDE-CBC-SHA",
"DES-CBC-SHA",
"DES-CBC3-SHA",
"DHE-DSS-DES-CBC3-SHA",
"DHE-RSA-DES-CBC-SHA",
"DHE-RSA-DES-CBC3-SHA",
"ADH-DES-CBC-SHA",
"ADH-DES-CBC3-SHA",
"EXP-DH-DSS-DES-CBC-SHA",
"DH-DSS-DES-CBC-SHA",
"DH-DSS-DES-CBC3-SHA",
"EXP-DH-RSA-DES-CBC-SHA",
"DH-RSA-DES-CBC-SHA",
"DH-RSA-DES-CBC3-SHA",
"EXP-RC4-MD5",
"EXP-RC2-CBC-MD5",
"EXP-DES-CBC-SHA",
"EXP-DHE-DSS-DES-CBC-SHA",
"EXP-DHE-RSA-DES-CBC-SHA",
"EXP-ADH-DES-CBC-SHA",
"EXP-ADH-RC4-MD5",
"RC4-MD5",
"RC4-SHA",
"ADH-RC4-MD5",
"KRB5-RC4-SHA",
"KRB5-RC4-MD5",
"EXP-KRB5-RC4-SHA",
"EXP-KRB5-RC4-MD5",
"PSK-RC4-SHA",
"DHE-PSK-RC4-SHA",
"RSA-PSK-RC4-SHA",
"ECDH-ECDSA-RC4-SHA",
"ECDHE-ECDSA-RC4-SHA",
"ECDH-RSA-RC4-SHA",
"ECDHE-RSA-RC4-SHA",
"AECDH-RC4-SHA",
"ECDHE-PSK-RC4-SHA",
"AES128-SHA256",
"DH-DSS-AES128-SHA",
"DH-RSA-AES128-SHA",
"DHE-DSS-AES128-SHA",
"DHE-RSA-AES128-SHA",
"ADH-AES128-SHA",
"AES128-SHA256",
"DH-DSS-AES128-SHA256",
"DH-RSA-AES128-SHA256",
"DHE-DSS-AES128-SHA256",
"DHE-RSA-AES128-SHA256",
"ECDH-ECDSA-AES128-SHA",
"ECDHE-ECDSA-AES128-SHA",
"ECDH-RSA-AES128-SHA",
"ECDHE-RSA-AES128-SHA",
"AECDH-AES128-SHA",
"ECDHE-ECDSA-AES128-SHA256",
"ECDH-ECDSA-AES128-SHA256",
"ECDHE-RSA-AES128-SHA256",
"ECDH-RSA-AES128-SHA256",
"ADH-AES128-SHA256",
"PSK-AES128-CBC-SHA",
"DHE-PSK-AES128-CBC-SHA",
"RSA-PSK-AES128-CBC-SHA",
"PSK-AES128-CBC-SHA256",
"DHE-PSK-AES128-CBC-SHA256",
"RSA-PSK-AES128-CBC-SHA256",
"ECDHE-PSK-AES128-CBC-SHA",
"ECDHE-PSK-AES128-CBC-SHA256",
"AES128-CCM",
"AES128-CCM8",
"PSK-AES128-CCM",
"PSK-AES128-CCM8",
"AES128-GCM-SHA256",
"DH-RSA-AES128-GCM-SHA256",
"DH-DSS-AES128-GCM-SHA256",
"ADH-AES128-GCM-SHA256",
"PSK-AES128-GCM-SHA256",
"RSA-PSK-AES128-GCM-SHA256",
"ECDH-ECDSA-AES128-GCM-SHA256",
"ECDH-RSA-AES128-GCM-SHA256",
"SRP-AES-128-CBC-SHA",
"SRP-RSA-AES-128-CBC-SHA",
"SRP-DSS-AES-128-CBC-SHA",
"AES256-SHA",
"DH-DSS-AES256-SHA",
"DH-RSA-AES256-SHA",
"DHE-DSS-AES256-SHA",
"DHE-RSA-AES256-SHA",
"ADH-AES256-SHA",
"AES256-SHA256",
"DH-DSS-AES256-SHA256",
"DH-RSA-AES256-SHA256",
"DHE-DSS-AES256-SHA256",
"DHE-RSA-AES256-SHA256",
"ADH-AES256-SHA256",
"ECDH-ECDSA-AES256-SHA",
"ECDHE-ECDSA-AES256-SHA",
"ECDH-RSA-AES256-SHA",
"ECDHE-RSA-AES256-SHA",
"AECDH-AES256-SHA",
"ECDHE-ECDSA-AES256-SHA384",
"ECDH-ECDSA-AES256-SHA384",
"ECDHE-RSA-AES256-SHA384",
"ECDH-RSA-AES256-SHA384",
"PSK-AES256-CBC-SHA",
"DHE-PSK-AES256-CBC-SHA",
"RSA-PSK-AES256-CBC-SHA",
"PSK-AES256-CBC-SHA384",
"DHE-PSK-AES256-CBC-SHA384",
"RSA-PSK-AES256-CBC-SHA384",
"ECDHE-PSK-AES256-CBC-SHA",
"ECDHE-PSK-AES256-CBC-SHA384",
"SRP-AES-256-CBC-SHA",
"SRP-RSA-AES-256-CBC-SHA",
"SRP-DSS-AES-256-CBC-SHA",
"AES256-CCM",
"AES256-CCM8",
"PSK-AES256-CCM",
"PSK-AES256-CCM8",
"AES256-GCM-SHA384",
"DH-RSA-AES256-GCM-SHA384",
"DH-DSS-AES256-GCM-SHA384",
"ADH-AES256-GCM-SHA384",
"PSK-AES256-GCM-SHA384",
"RSA-PSK-AES256-GCM-SHA384",
"ECDH-ECDSA-AES256-GCM-SHA384",
"ECDH-RSA-AES256-GCM-SHA384",
"CAMELLIA128-SHA",
"DH-DSS-CAMELLIA128-SHA",
"DH-RSA-CAMELLIA128-SHA",
"DHE-DSS-CAMELLIA128-SHA",
"DHE-RSA-CAMELLIA128-SHA",
"ADH-CAMELLIA128-SHA",
"ECDHE-ECDSA-CAMELLIA128-SHA256",
"ECDH-ECDSA-CAMELLIA128-SHA256",
"ECDHE-RSA-CAMELLIA128-SHA256",
"ECDH-RSA-CAMELLIA128-SHA256",
"PSK-CAMELLIA128-SHA256",
"DHE-PSK-CAMELLIA128-SHA256",
"RSA-PSK-CAMELLIA128-SHA256",
"ECDHE-PSK-CAMELLIA128-SHA256",
"CAMELLIA128-GCM-SHA256",
"DH-RSA-CAMELLIA128-GCM-SHA256",
"DH-DSS-CAMELLIA128-GCM-SHA256",
"ADH-CAMELLIA128-GCM-SHA256",
"ECDH-ECDSA-CAMELLIA128-GCM-SHA256",
"ECDH-RSA-CAMELLIA128-GCM-SHA256",
"PSK-CAMELLIA128-GCM-SHA256",
"RSA-PSK-CAMELLIA128-GCM-SHA256",
"CAMELLIA128-SHA256",
"DH-DSS-CAMELLIA128-SHA256",
"DH-RSA-CAMELLIA128-SHA256",
"DHE-DSS-CAMELLIA128-SHA256",
"DHE-RSA-CAMELLIA128-SHA256",
"ADH-CAMELLIA128-SHA256",
"CAMELLIA256-SHA",
"DH-RSA-CAMELLIA256-SHA",
"DH-DSS-CAMELLIA256-SHA",
"DHE-DSS-CAMELLIA256-SHA",
"DHE-RSA-CAMELLIA256-SHA",
"ADH-CAMELLIA256-SHA",
"ECDHE-ECDSA-CAMELLIA256-SHA384",
"ECDH-ECDSA-CAMELLIA256-SHA384",
"ECDHE-RSA-CAMELLIA256-SHA384",
"ECDH-RSA-CAMELLIA256-SHA384",
"PSK-CAMELLIA256-SHA384",
"DHE-PSK-CAMELLIA256-SHA384",
"RSA-PSK-CAMELLIA256-SHA384",
"ECDHE-PSK-CAMELLIA256-SHA384",
"CAMELLIA256-SHA256",
"DH-DSS-CAMELLIA256-SHA256",
"DH-RSA-CAMELLIA256-SHA256",
"DHE-DSS-CAMELLIA256-SHA256",
"DHE-RSA-CAMELLIA256-SHA256",
"ADH-CAMELLIA256-SHA256",
"CAMELLIA256-GCM-SHA384",
"DH-RSA-CAMELLIA256-GCM-SHA384",
"DH-DSS-CAMELLIA256-GCM-SHA384",
"ADH-CAMELLIA256-GCM-SHA384",
"ECDH-ECDSA-CAMELLIA256-GCM-SHA384",
"ECDH-RSA-CAMELLIA256-GCM-SHA384",
"PSK-CAMELLIA256-GCM-SHA384",
"RSA-PSK-CAMELLIA256-GCM-SHA384",
"ARIA128-SHA256",
"ARIA256-SHA384",
"DH-DSS-ARIA128-SHA256",
"DH-DSS-ARIA256-SHA384",
"DH-RSA-ARIA128-SHA256",
"DH-RSA-ARIA256-SHA384",
"DHE-DSS-ARIA128-SHA256",
"DHE-DSS-ARIA256-SHA384",
"DHE-RSA-ARIA128-SHA256",
"DHE-RSA-ARIA256-SHA384",
"ADH-ARIA128-SHA256",
"ADH-ARIA256-SHA384",
"ECDHE-ECDSA-ARIA128-SHA256",
"ECDHE-ECDSA-ARIA256-SHA384",
"ECDH-ECDSA-ARIA128-SHA256",
"ECDH-ECDSA-ARIA256-SHA384",
"ECDHE-RSA-ARIA128-SHA256",
"ECDHE-RSA-ARIA256-SHA384",
"ECDH-RSA-ARIA128-SHA256",
"ECDH-RSA-ARIA256-SHA384",
"ARIA128-GCM-SHA256",
"ARIA256-GCM-SHA384",
"DH-DSS-ARIA128-GCM-SHA256",
"DH-DSS-ARIA256-GCM-SHA384",
"DH-RSA-ARIA128-GCM-SHA256",
"DH-RSA-ARIA256-GCM-SHA384",
"ADH-ARIA128-GCM-SHA256",
"ADH-ARIA256-GCM-SHA384",
"ECDH-ECDSA-ARIA128-GCM-SHA256",
"ECDH-ECDSA-ARIA256-GCM-SHA384",
"ECDH-RSA-ARIA128-GCM-SHA256",
"ECDH-RSA-ARIA256-GCM-SHA384",
"PSK-ARIA128-SHA256",
"PSK-ARIA256-SHA384",
"DHE-PSK-ARIA128-SHA256",
"DHE-PSK-ARIA256-SHA384",
"RSA-PSK-ARIA128-SHA256",
"RSA-PSK-ARIA256-SHA384",
"ARIA128-GCM-SHA256",
"ARIA256-GCM-SHA384",
"RSA-PSK-ARIA128-GCM-SHA256",
"RSA-PSK-ARIA256-GCM-SHA384",
"ECDHE-PSK-ARIA128-SHA256",
"ECDHE-PSK-ARIA256-SHA384",
"SEED-SHA",
"DH-DSS-SEED-SHA",
"DH-RSA-SEED-SHA",
"DHE-DSS-SEED-SHA",
"DHE-RSA-SEED-SHA",
"ADH-SEED-SHA",
"KRB5-DES-CBC-SHA",
"KRB5-DES-CBC3-SHA",
"KRB5-IDEA-CBC-SHA",
"KRB5-DES-CBC-MD5",
"KRB5-DES-CBC3-MD5",
"KRB5-IDEA-CBC-MD5",
"EXP-KRB5-DES-CBC-SHA",
"EXP-KRB5-DES-CBC-MD5",
"EXP-KRB5-RC2-CBC-SHA",
"EXP-KRB5-RC2-CBC-MD5",
"DHE-DSS-CBC-SHA",
"IDEA-CBC-SHA",
"SSL3_CK_SCSV",
"SSL3_CK_FALLBACK_SCSV"
};
static size_t RFC7540_names_LEN = sizeof(RFC7540_names)/sizeof(RFC7540_names[0]);
static apr_hash_t *BLCNames;
static void cipher_init(apr_pool_t *pool) {
apr_hash_t *hash = apr_hash_make(pool);
const char *source;
unsigned int i;
source = "rfc7540";
for (i = 0; i < RFC7540_names_LEN; ++i) {
apr_hash_set(hash, RFC7540_names[i], APR_HASH_KEY_STRING, source);
}
BLCNames = hash;
}
static int cipher_is_blacklisted(const char *cipher, const char **psource) {
*psource = apr_hash_get(BLCNames, cipher, APR_HASH_KEY_STRING);
return !!*psource;
}
static int h2_h2_process_conn(conn_rec* c);
static int h2_h2_pre_close_conn(conn_rec* c);
static int h2_h2_post_read_req(request_rec *r);
static int h2_h2_late_fixups(request_rec *r);
apr_status_t h2_h2_init(apr_pool_t *pool, server_rec *s) {
(void)pool;
ap_log_error(APLOG_MARK, APLOG_TRACE1, 0, s, "h2_h2, child_init");
opt_ssl_engine_disable = APR_RETRIEVE_OPTIONAL_FN(ssl_engine_disable);
opt_ssl_is_https = APR_RETRIEVE_OPTIONAL_FN(ssl_is_https);
opt_ssl_var_lookup = APR_RETRIEVE_OPTIONAL_FN(ssl_var_lookup);
if (!opt_ssl_is_https || !opt_ssl_var_lookup) {
ap_log_error(APLOG_MARK, APLOG_WARNING, 0, s,
APLOGNO(02951) "mod_ssl does not seem to be enabled");
}
cipher_init(pool);
return APR_SUCCESS;
}
int h2_h2_is_tls(conn_rec *c) {
return opt_ssl_is_https && opt_ssl_is_https(c);
}
int h2_is_acceptable_connection(conn_rec *c, int require_all) {
int is_tls = h2_h2_is_tls(c);
const h2_config *cfg = h2_config_get(c);
if (is_tls && h2_config_geti(cfg, H2_CONF_MODERN_TLS_ONLY) > 0) {
apr_pool_t *pool = c->pool;
server_rec *s = c->base_server;
char *val;
if (!opt_ssl_var_lookup) {
return 0;
}
val = opt_ssl_var_lookup(pool, s, c, NULL, (char*)"SSL_PROTOCOL");
if (val && *val) {
if (strncmp("TLS", val, 3)
|| !strcmp("TLSv1", val)
|| !strcmp("TLSv1.1", val)) {
ap_log_cerror(APLOG_MARK, APLOG_DEBUG, 0, c, APLOGNO(03050)
"h2_h2(%ld): tls protocol not suitable: %s",
(long)c->id, val);
return 0;
}
} else if (require_all) {
ap_log_cerror(APLOG_MARK, APLOG_DEBUG, 0, c, APLOGNO(03051)
"h2_h2(%ld): tls protocol is indetermined", (long)c->id);
return 0;
}
val = opt_ssl_var_lookup(pool, s, c, NULL, (char*)"SSL_CIPHER");
if (val && *val) {
const char *source;
if (cipher_is_blacklisted(val, &source)) {
ap_log_cerror(APLOG_MARK, APLOG_DEBUG, 0, c, APLOGNO(03052)
"h2_h2(%ld): tls cipher %s blacklisted by %s",
(long)c->id, val, source);
return 0;
}
} else if (require_all) {
ap_log_cerror(APLOG_MARK, APLOG_DEBUG, 0, c, APLOGNO(03053)
"h2_h2(%ld): tls cipher is indetermined", (long)c->id);
return 0;
}
}
return 1;
}
int h2_allows_h2_direct(conn_rec *c) {
const h2_config *cfg = h2_config_get(c);
int is_tls = h2_h2_is_tls(c);
const char *needed_protocol = is_tls? "h2" : "h2c";
int h2_direct = h2_config_geti(cfg, H2_CONF_DIRECT);
if (h2_direct < 0) {
h2_direct = is_tls? 0 : 1;
}
return (h2_direct
&& ap_is_allowed_protocol(c, NULL, NULL, needed_protocol));
}
int h2_allows_h2_upgrade(conn_rec *c) {
const h2_config *cfg = h2_config_get(c);
int h2_upgrade = h2_config_geti(cfg, H2_CONF_UPGRADE);
return h2_upgrade > 0 || (h2_upgrade < 0 && !h2_h2_is_tls(c));
}
static const char* const mod_ssl[] = { "mod_ssl.c", NULL};
static const char* const mod_reqtimeout[] = { "mod_reqtimeout.c", NULL};
void h2_h2_register_hooks(void) {
ap_hook_process_connection(h2_h2_process_conn,
mod_ssl, mod_reqtimeout, APR_HOOK_LAST);
ap_hook_pre_close_connection(h2_h2_pre_close_conn, NULL, mod_ssl, APR_HOOK_LAST);
ap_hook_post_read_request(h2_h2_post_read_req, NULL, NULL, APR_HOOK_REALLY_FIRST);
ap_hook_fixups(h2_h2_late_fixups, NULL, NULL, APR_HOOK_LAST);
h2_register_bucket_beamer(h2_bucket_headers_beam);
h2_register_bucket_beamer(h2_bucket_observer_beam);
}
int h2_h2_process_conn(conn_rec* c) {
apr_status_t status;
h2_ctx *ctx;
if (c->master) {
return DECLINED;
}
ctx = h2_ctx_get(c, 0);
ap_log_cerror(APLOG_MARK, APLOG_TRACE1, 0, c, "h2_h2, process_conn");
if (h2_ctx_is_task(ctx)) {
ap_log_cerror(APLOG_MARK, APLOG_TRACE2, 0, c, "h2_h2, task, declined");
return DECLINED;
}
if (!ctx && c->keepalives == 0) {
const char *proto = ap_get_protocol(c);
if (APLOGctrace1(c)) {
ap_log_cerror(APLOG_MARK, APLOG_TRACE1, 0, c, "h2_h2, process_conn, "
"new connection using protocol '%s', direct=%d, "
"tls acceptable=%d", proto, h2_allows_h2_direct(c),
h2_is_acceptable_connection(c, 1));
}
if (!strcmp(AP_PROTOCOL_HTTP1, proto)
&& h2_allows_h2_direct(c)
&& h2_is_acceptable_connection(c, 1)) {
apr_bucket_brigade *temp;
char *s = NULL;
apr_size_t slen;
temp = apr_brigade_create(c->pool, c->bucket_alloc);
status = ap_get_brigade(c->input_filters, temp,
AP_MODE_SPECULATIVE, APR_BLOCK_READ, 24);
if (status != APR_SUCCESS) {
ap_log_cerror(APLOG_MARK, APLOG_DEBUG, status, c, APLOGNO(03054)
"h2_h2, error reading 24 bytes speculative");
apr_brigade_destroy(temp);
return DECLINED;
}
apr_brigade_pflatten(temp, &s, &slen, c->pool);
if ((slen >= 24) && !memcmp(H2_MAGIC_TOKEN, s, 24)) {
ap_log_cerror(APLOG_MARK, APLOG_TRACE1, 0, c,
"h2_h2, direct mode detected");
if (!ctx) {
ctx = h2_ctx_get(c, 1);
}
h2_ctx_protocol_set(ctx, h2_h2_is_tls(c)? "h2" : "h2c");
} else {
ap_log_cerror(APLOG_MARK, APLOG_TRACE2, 0, c,
"h2_h2, not detected in %d bytes: %s",
(int)slen, s);
}
apr_brigade_destroy(temp);
}
}
if (ctx) {
ap_log_cerror(APLOG_MARK, APLOG_TRACE1, 0, c, "process_conn");
if (!h2_ctx_session_get(ctx)) {
status = h2_conn_setup(ctx, c, NULL);
ap_log_cerror(APLOG_MARK, APLOG_TRACE1, status, c, "conn_setup");
if (status != APR_SUCCESS) {
h2_ctx_clear(c);
return status;
}
}
return h2_conn_run(ctx, c);
}
ap_log_cerror(APLOG_MARK, APLOG_TRACE1, 0, c, "h2_h2, declined");
return DECLINED;
}
static int h2_h2_pre_close_conn(conn_rec *c) {
h2_ctx *ctx;
if (c->master) {
return DECLINED;
}
ctx = h2_ctx_get(c, 0);
if (ctx) {
return h2_conn_pre_close(ctx, c);
}
return DECLINED;
}
static void check_push(request_rec *r, const char *tag) {
const h2_config *conf = h2_config_rget(r);
if (!r->expecting_100
&& conf && conf->push_list && conf->push_list->nelts > 0) {
int i, old_status;
const char *old_line;
ap_log_rerror(APLOG_MARK, APLOG_TRACE1, 0, r,
"%s, early announcing %d resources for push",
tag, conf->push_list->nelts);
for (i = 0; i < conf->push_list->nelts; ++i) {
h2_push_res *push = &APR_ARRAY_IDX(conf->push_list, i, h2_push_res);
apr_table_addn(r->headers_out, "Link",
apr_psprintf(r->pool, "<%s>; rel=preload%s",
push->uri_ref, push->critical? "; critical" : ""));
}
old_status = r->status;
old_line = r->status_line;
r->status = 103;
r->status_line = "103 Early Hints";
ap_send_interim_response(r, 1);
r->status = old_status;
r->status_line = old_line;
}
}
static int h2_h2_post_read_req(request_rec *r) {
if (r->connection->master) {
h2_ctx *ctx = h2_ctx_rget(r);
struct h2_task *task = h2_ctx_get_task(ctx);
if (task && !task->filters_set) {
ap_filter_t *f;
ap_log_rerror(APLOG_MARK, APLOG_TRACE3, 0, r,
"h2_task(%s): adding request filters", task->id);
ap_add_input_filter("H2_REQUEST", task, r, r->connection);
ap_remove_output_filter_byhandle(r->output_filters, "HTTP_HEADER");
ap_add_output_filter("H2_RESPONSE", task, r, r->connection);
for (f = r->input_filters; f; f = f->next) {
if (!strcmp("H2_SLAVE_IN", f->frec->name)) {
f->r = r;
break;
}
}
ap_add_output_filter("H2_TRAILERS_OUT", task, r, r->connection);
task->filters_set = 1;
}
}
return DECLINED;
}
static int h2_h2_late_fixups(request_rec *r) {
if (r->connection->master) {
h2_ctx *ctx = h2_ctx_rget(r);
struct h2_task *task = h2_ctx_get_task(ctx);
if (task) {
task->output.copy_files = h2_config_geti(h2_config_rget(r),
H2_CONF_COPY_FILES);
if (task->output.copy_files) {
ap_log_cerror(APLOG_MARK, APLOG_TRACE1, 0, task->c,
"h2_slave_out(%s): copy_files on", task->id);
h2_beam_on_file_beam(task->output.beam, h2_beam_no_files, NULL);
}
check_push(r, "late_fixup");
}
}
return DECLINED;
}
