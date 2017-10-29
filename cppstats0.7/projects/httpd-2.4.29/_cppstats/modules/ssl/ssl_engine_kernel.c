#include "ssl_private.h"
#include "mod_ssl.h"
#include "util_md5.h"
#include "scoreboard.h"
static void ssl_configure_env(request_rec *r, SSLConnRec *sslconn);
#if defined(HAVE_TLSEXT)
static int ssl_find_vhost(void *servername, conn_rec *c, server_rec *s);
#endif
#define SWITCH_STATUS_LINE "HTTP/1.1 101 Switching Protocols"
#define UPGRADE_HEADER "Upgrade: TLS/1.0, HTTP/1.1"
#define CONNECTION_HEADER "Connection: Upgrade"
static apr_status_t upgrade_connection(request_rec *r) {
struct conn_rec *conn = r->connection;
apr_bucket_brigade *bb;
SSLConnRec *sslconn;
apr_status_t rv;
SSL *ssl;
ap_log_rerror(APLOG_MARK, APLOG_INFO, 0, r, APLOGNO(02028)
"upgrading connection to TLS");
bb = apr_brigade_create(r->pool, conn->bucket_alloc);
rv = ap_fputs(conn->output_filters, bb, SWITCH_STATUS_LINE CRLF
UPGRADE_HEADER CRLF CONNECTION_HEADER CRLF CRLF);
if (rv == APR_SUCCESS) {
APR_BRIGADE_INSERT_TAIL(bb,
apr_bucket_flush_create(conn->bucket_alloc));
rv = ap_pass_brigade(conn->output_filters, bb);
}
if (rv) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(02029)
"failed to send 101 interim response for connection "
"upgrade");
return rv;
}
ssl_init_ssl_connection(conn, r);
sslconn = myConnConfig(conn);
ssl = sslconn->ssl;
SSL_set_accept_state(ssl);
SSL_do_handshake(ssl);
if (!SSL_is_init_finished(ssl)) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(02030)
"TLS upgrade handshake failed");
ssl_log_ssl_error(SSLLOG_MARK, APLOG_ERR, r->server);
return APR_ECONNABORTED;
}
return APR_SUCCESS;
}
static int has_buffered_data(request_rec *r) {
apr_bucket_brigade *bb;
apr_off_t len;
apr_status_t rv;
int result;
bb = apr_brigade_create(r->pool, r->connection->bucket_alloc);
rv = ap_get_brigade(r->connection->input_filters, bb, AP_MODE_SPECULATIVE,
APR_NONBLOCK_READ, 1);
result = rv == APR_SUCCESS
&& apr_brigade_length(bb, 1, &len) == APR_SUCCESS
&& len > 0;
apr_brigade_destroy(bb);
return result;
}
#if defined(HAVE_TLSEXT)
static int ap_array_same_str_set(apr_array_header_t *s1, apr_array_header_t *s2) {
int i;
const char *c;
if (s1 == s2) {
return 1;
} else if (!s1 || !s2 || (s1->nelts != s2->nelts)) {
return 0;
}
for (i = 0; i < s1->nelts; i++) {
c = APR_ARRAY_IDX(s1, i, const char *);
if (!c || !ap_array_str_contains(s2, c)) {
return 0;
}
}
return 1;
}
static int ssl_pk_server_compatible(modssl_pk_server_t *pks1,
modssl_pk_server_t *pks2) {
if (!pks1 || !pks2) {
return 0;
}
if ((pks1->ca_name_path != pks2->ca_name_path)
&& (!pks1->ca_name_path || !pks2->ca_name_path
|| strcmp(pks1->ca_name_path, pks2->ca_name_path))) {
return 0;
}
if ((pks1->ca_name_file != pks2->ca_name_file)
&& (!pks1->ca_name_file || !pks2->ca_name_file
|| strcmp(pks1->ca_name_file, pks2->ca_name_file))) {
return 0;
}
if (!ap_array_same_str_set(pks1->cert_files, pks2->cert_files)
|| !ap_array_same_str_set(pks1->key_files, pks2->key_files)) {
return 0;
}
return 1;
}
static int ssl_auth_compatible(modssl_auth_ctx_t *a1,
modssl_auth_ctx_t *a2) {
if (!a1 || !a2) {
return 0;
}
if ((a1->verify_depth != a2->verify_depth)
|| (a1->verify_mode != a2->verify_mode)) {
return 0;
}
if ((a1->ca_cert_path != a2->ca_cert_path)
&& (!a1->ca_cert_path || !a2->ca_cert_path
|| strcmp(a1->ca_cert_path, a2->ca_cert_path))) {
return 0;
}
if ((a1->ca_cert_file != a2->ca_cert_file)
&& (!a1->ca_cert_file || !a2->ca_cert_file
|| strcmp(a1->ca_cert_file, a2->ca_cert_file))) {
return 0;
}
if ((a1->cipher_suite != a2->cipher_suite)
&& (!a1->cipher_suite || !a2->cipher_suite
|| strcmp(a1->cipher_suite, a2->cipher_suite))) {
return 0;
}
return 1;
}
static int ssl_ctx_compatible(modssl_ctx_t *ctx1,
modssl_ctx_t *ctx2) {
if (!ctx1 || !ctx2
|| (ctx1->protocol != ctx2->protocol)
|| !ssl_auth_compatible(&ctx1->auth, &ctx2->auth)
|| !ssl_pk_server_compatible(ctx1->pks, ctx2->pks)) {
return 0;
}
return 1;
}
static int ssl_server_compatible(server_rec *s1, server_rec *s2) {
SSLSrvConfigRec *sc1 = s1? mySrvConfig(s1) : NULL;
SSLSrvConfigRec *sc2 = s2? mySrvConfig(s2) : NULL;
if (!sc1 || !sc2
|| !ssl_ctx_compatible(sc1->server, sc2->server)) {
return 0;
}
return 1;
}
#endif
int ssl_hook_ReadReq(request_rec *r) {
SSLSrvConfigRec *sc = mySrvConfig(r->server);
SSLConnRec *sslconn;
const char *upgrade;
#if defined(HAVE_TLSEXT)
const char *servername;
#endif
SSL *ssl;
if (sc->enabled == SSL_ENABLED_OPTIONAL && !myConnConfig(r->connection)
&& (upgrade = apr_table_get(r->headers_in, "Upgrade")) != NULL
&& ap_find_token(r->pool, upgrade, "TLS/1.0")) {
if (upgrade_connection(r)) {
return AP_FILTER_ERROR;
}
}
sslconn = myConnConfig(r->connection);
if (!(sslconn && sslconn->ssl) && r->connection->master) {
sslconn = myConnConfig(r->connection->master);
}
if (sc->enabled == SSL_ENABLED_OPTIONAL && !(sslconn && sslconn->ssl)
&& !r->main) {
apr_table_setn(r->headers_out, "Upgrade", "TLS/1.0, HTTP/1.1");
apr_table_mergen(r->headers_out, "Connection", "upgrade");
}
if (!sslconn) {
return DECLINED;
}
if (sslconn->non_ssl_request == NON_SSL_SET_ERROR_MSG) {
apr_table_setn(r->notes, "error-notes",
"Reason: You're speaking plain HTTP to an SSL-enabled "
"server port.<br />\n Instead use the HTTPS scheme to "
"access this URL, please.<br />\n");
sslconn->non_ssl_request = NON_SSL_OK;
return HTTP_BAD_REQUEST;
}
ssl = sslconn->ssl;
if (!ssl) {
return DECLINED;
}
#if defined(HAVE_TLSEXT)
if (r->proxyreq != PROXYREQ_PROXY && ap_is_initial_req(r)) {
server_rec *handshakeserver = sslconn->server;
SSLSrvConfigRec *hssc = mySrvConfig(handshakeserver);
if ((servername = SSL_get_servername(ssl, TLSEXT_NAMETYPE_host_name))) {
if (!r->hostname) {
ap_log_error(APLOG_MARK, APLOG_ERR, 0, r->server, APLOGNO(02031)
"Hostname %s provided via SNI, but no hostname"
" provided in HTTP request", servername);
return HTTP_BAD_REQUEST;
}
if (r->server != handshakeserver
&& !ssl_server_compatible(sslconn->server, r->server)) {
ap_log_error(APLOG_MARK, APLOG_ERR, 0, r->server, APLOGNO(02032)
"Hostname %s provided via SNI and hostname %s provided"
" via HTTP have no compatible SSL setup",
servername, r->hostname);
return HTTP_MISDIRECTED_REQUEST;
}
} else if (((sc->strict_sni_vhost_check == SSL_ENABLED_TRUE)
|| hssc->strict_sni_vhost_check == SSL_ENABLED_TRUE)
&& r->connection->vhost_lookup_data) {
ap_log_error(APLOG_MARK, APLOG_ERR, 0, r->server, APLOGNO(02033)
"No hostname was provided via SNI for a name based"
" virtual host");
apr_table_setn(r->notes, "error-notes",
"Reason: The client software did not provide a "
"hostname using Server Name Indication (SNI), "
"which is required to access this server.<br />\n");
return HTTP_FORBIDDEN;
}
}
#endif
modssl_set_app_data2(ssl, r);
if (APLOGrinfo(r) && ap_is_initial_req(r)) {
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(02034)
"%s HTTPS request received for child %ld (server %s)",
(r->connection->keepalives <= 0 ?
"Initial (No.1)" :
apr_psprintf(r->pool, "Subsequent (No.%d)",
r->connection->keepalives+1)),
r->connection->id,
ssl_util_vhostid(r->pool, r->server));
}
if (sslconn->shutdown_type == SSL_SHUTDOWN_TYPE_UNSET) {
ssl_configure_env(r, sslconn);
}
return DECLINED;
}
static void ssl_configure_env(request_rec *r, SSLConnRec *sslconn) {
int i;
const apr_array_header_t *arr = apr_table_elts(r->subprocess_env);
const apr_table_entry_t *elts = (const apr_table_entry_t *)arr->elts;
sslconn->shutdown_type = SSL_SHUTDOWN_TYPE_STANDARD;
for (i = 0; i < arr->nelts; i++) {
const char *key = elts[i].key;
switch (*key) {
case 's':
if (!strncmp(key+1, "sl-", 3)) {
key += 4;
if (!strncmp(key, "unclean", 7)) {
sslconn->shutdown_type = SSL_SHUTDOWN_TYPE_UNCLEAN;
} else if (!strncmp(key, "accurate", 8)) {
sslconn->shutdown_type = SSL_SHUTDOWN_TYPE_ACCURATE;
}
return;
}
break;
}
}
}
int ssl_hook_Access(request_rec *r) {
SSLDirConfigRec *dc = myDirConfig(r);
SSLSrvConfigRec *sc = mySrvConfig(r->server);
SSLConnRec *sslconn = myConnConfig(r->connection);
SSL *ssl = sslconn ? sslconn->ssl : NULL;
server_rec *handshakeserver = sslconn ? sslconn->server : NULL;
SSLSrvConfigRec *hssc = handshakeserver? mySrvConfig(handshakeserver) : NULL;
SSL_CTX *ctx = NULL;
apr_array_header_t *requires;
ssl_require_t *ssl_requires;
int ok, i;
BOOL renegotiate = FALSE, renegotiate_quick = FALSE;
X509 *cert;
X509 *peercert;
X509_STORE *cert_store = NULL;
X509_STORE_CTX *cert_store_ctx;
STACK_OF(SSL_CIPHER) *cipher_list_old = NULL, *cipher_list = NULL;
const SSL_CIPHER *cipher = NULL;
int depth, verify_old, verify, n, is_slave = 0;
const char *ncipher_suite;
if (!(sslconn && ssl) && r->connection->master) {
sslconn = myConnConfig(r->connection->master);
ssl = sslconn ? sslconn->ssl : NULL;
handshakeserver = sslconn ? sslconn->server : NULL;
hssc = handshakeserver? mySrvConfig(handshakeserver) : NULL;
is_slave = 1;
}
if (ssl) {
if (!SSL_is_init_finished(ssl)) {
return HTTP_FORBIDDEN;
}
ctx = SSL_get_SSL_CTX(ssl);
}
if (dc->bSSLRequired && !ssl) {
if ((sc->enabled == SSL_ENABLED_OPTIONAL) && !is_slave) {
apr_table_setn(r->err_headers_out, "Upgrade", "TLS/1.0, HTTP/1.1");
apr_table_setn(r->err_headers_out, "Connection", "Upgrade");
return HTTP_UPGRADE_REQUIRED;
}
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(02219)
"access to %s failed, reason: %s",
r->filename, "SSL connection required");
apr_table_setn(r->notes, "ssl-access-forbidden", "1");
return HTTP_FORBIDDEN;
}
if (sc->enabled == SSL_ENABLED_FALSE || !ssl) {
return DECLINED;
}
#if defined(HAVE_SRP)
if (SSL_get_srp_username(ssl)) {
return DECLINED;
}
#endif
ncipher_suite = (dc->szCipherSuite?
dc->szCipherSuite : (r->server != handshakeserver)?
sc->server->auth.cipher_suite : NULL);
if (ncipher_suite && (!sslconn->cipher_suite
|| strcmp(ncipher_suite, sslconn->cipher_suite))) {
if (dc->nOptions & SSL_OPT_OPTRENEGOTIATE) {
cipher = SSL_get_current_cipher(ssl);
} else {
cipher_list_old = (STACK_OF(SSL_CIPHER) *)SSL_get_ciphers(ssl);
if (cipher_list_old) {
cipher_list_old = sk_SSL_CIPHER_dup(cipher_list_old);
}
}
if (is_slave) {
apr_table_setn(r->notes, "ssl-renegotiate-forbidden", "cipher-suite");
return HTTP_FORBIDDEN;
}
if (!SSL_set_cipher_list(ssl, ncipher_suite)) {
ap_log_rerror(APLOG_MARK, APLOG_WARNING, 0, r, APLOGNO(02253)
"Unable to reconfigure (per-directory) "
"permitted SSL ciphers");
ssl_log_ssl_error(SSLLOG_MARK, APLOG_ERR, r->server);
if (cipher_list_old) {
sk_SSL_CIPHER_free(cipher_list_old);
}
return HTTP_FORBIDDEN;
}
cipher_list = (STACK_OF(SSL_CIPHER) *)SSL_get_ciphers(ssl);
if (dc->nOptions & SSL_OPT_OPTRENEGOTIATE) {
if ((!cipher && cipher_list) ||
(cipher && !cipher_list)) {
renegotiate = TRUE;
} else if (cipher && cipher_list &&
(sk_SSL_CIPHER_find(cipher_list, cipher) < 0)) {
renegotiate = TRUE;
}
} else {
if ((!cipher_list_old && cipher_list) ||
(cipher_list_old && !cipher_list)) {
renegotiate = TRUE;
} else if (cipher_list_old && cipher_list) {
for (n = 0;
!renegotiate && (n < sk_SSL_CIPHER_num(cipher_list));
n++) {
const SSL_CIPHER *value = sk_SSL_CIPHER_value(cipher_list, n);
if (sk_SSL_CIPHER_find(cipher_list_old, value) < 0) {
renegotiate = TRUE;
}
}
for (n = 0;
!renegotiate && (n < sk_SSL_CIPHER_num(cipher_list_old));
n++) {
const SSL_CIPHER *value = sk_SSL_CIPHER_value(cipher_list_old, n);
if (sk_SSL_CIPHER_find(cipher_list, value) < 0) {
renegotiate = TRUE;
}
}
}
}
if (cipher_list_old) {
sk_SSL_CIPHER_free(cipher_list_old);
}
if (renegotiate) {
if (is_slave) {
apr_table_setn(r->notes, "ssl-renegotiate-forbidden", "cipher-suite");
return HTTP_FORBIDDEN;
}
#if defined(SSL_OP_CIPHER_SERVER_PREFERENCE)
if (sc->cipher_server_pref == TRUE) {
SSL_set_options(ssl, SSL_OP_CIPHER_SERVER_PREFERENCE);
}
#endif
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(02220)
"Reconfigured cipher suite will force renegotiation");
}
}
if ((dc->nVerifyClient != SSL_CVERIFY_UNSET) ||
(sc->server->auth.verify_mode != SSL_CVERIFY_UNSET)) {
verify_old = SSL_get_verify_mode(ssl);
verify = SSL_VERIFY_NONE;
if ((dc->nVerifyClient == SSL_CVERIFY_REQUIRE) ||
(sc->server->auth.verify_mode == SSL_CVERIFY_REQUIRE)) {
verify |= SSL_VERIFY_PEER_STRICT;
}
if ((dc->nVerifyClient == SSL_CVERIFY_OPTIONAL) ||
(dc->nVerifyClient == SSL_CVERIFY_OPTIONAL_NO_CA) ||
(sc->server->auth.verify_mode == SSL_CVERIFY_OPTIONAL) ||
(sc->server->auth.verify_mode == SSL_CVERIFY_OPTIONAL_NO_CA)) {
verify |= SSL_VERIFY_PEER;
}
SSL_set_verify(ssl, verify, ssl_callback_SSLVerify);
SSL_set_verify_result(ssl, X509_V_OK);
if (!renegotiate && verify != verify_old) {
if (((verify_old == SSL_VERIFY_NONE) &&
(verify != SSL_VERIFY_NONE)) ||
(!(verify_old & SSL_VERIFY_PEER) &&
(verify & SSL_VERIFY_PEER)) ||
(!(verify_old & SSL_VERIFY_FAIL_IF_NO_PEER_CERT) &&
(verify & SSL_VERIFY_FAIL_IF_NO_PEER_CERT))) {
renegotiate = TRUE;
if (is_slave) {
apr_table_setn(r->notes, "ssl-renegotiate-forbidden", "verify-client");
SSL_set_verify(ssl, verify_old, ssl_callback_SSLVerify);
return HTTP_FORBIDDEN;
}
if ((dc->nOptions & SSL_OPT_OPTRENEGOTIATE) &&
(verify_old == SSL_VERIFY_NONE) &&
((peercert = SSL_get_peer_certificate(ssl)) != NULL)) {
renegotiate_quick = TRUE;
X509_free(peercert);
}
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(02255)
"Changed client verification type will force "
"%srenegotiation",
renegotiate_quick ? "quick " : "");
} else if (verify != SSL_VERIFY_NONE) {
n = (sslconn->verify_depth != UNSET)
? sslconn->verify_depth
: hssc->server->auth.verify_depth;
sslconn->verify_depth = (dc->nVerifyDepth != UNSET)
? dc->nVerifyDepth
: sc->server->auth.verify_depth;
if (sslconn->verify_depth < n) {
renegotiate = TRUE;
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(02254)
"Reduced client verification depth will "
"force renegotiation");
}
}
}
if ((r->server != handshakeserver)
&& renegotiate
&& ((verify & SSL_VERIFY_PEER) ||
(verify & SSL_VERIFY_FAIL_IF_NO_PEER_CERT))) {
#define MODSSL_CFG_CA_NE(f, sc1, sc2) (sc1->server->auth.f && (!sc2->server->auth.f || strNE(sc1->server->auth.f, sc2->server->auth.f)))
if (MODSSL_CFG_CA_NE(ca_cert_file, sc, hssc) ||
MODSSL_CFG_CA_NE(ca_cert_path, sc, hssc)) {
if (verify & SSL_VERIFY_FAIL_IF_NO_PEER_CERT) {
ap_log_rerror(APLOG_MARK, APLOG_INFO, 0, r, APLOGNO(02256)
"Non-default virtual host with SSLVerify set to "
"'require' and VirtualHost-specific CA certificate "
"list is only available to clients with TLS server "
"name indication (SNI) support");
SSL_set_verify(ssl, verify_old, NULL);
return HTTP_FORBIDDEN;
} else
sslconn->verify_info = "GENEROUS";
}
}
}
if (renegotiate && !renegotiate_quick
&& (apr_table_get(r->headers_in, "transfer-encoding")
|| (apr_table_get(r->headers_in, "content-length")
&& strcmp(apr_table_get(r->headers_in, "content-length"), "0")))
&& !r->expecting_100) {
int rv;
apr_size_t rsize;
rsize = dc->nRenegBufferSize == UNSET ? DEFAULT_RENEG_BUFFER_SIZE :
dc->nRenegBufferSize;
if (rsize > 0) {
rv = ssl_io_buffer_fill(r, rsize);
} else {
rv = HTTP_REQUEST_ENTITY_TOO_LARGE;
}
if (rv) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(02257)
"could not buffer message body to allow "
"SSL renegotiation to proceed");
return rv;
}
}
if (renegotiate) {
ap_log_rerror(APLOG_MARK, APLOG_INFO, 0, r, APLOGNO(02221)
"Requesting connection re-negotiation");
if (renegotiate_quick) {
STACK_OF(X509) *cert_stack;
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(02258)
"Performing quick renegotiation: "
"just re-verifying the peer");
cert_stack = (STACK_OF(X509) *)SSL_get_peer_cert_chain(ssl);
cert = SSL_get_peer_certificate(ssl);
if (!cert_stack || (sk_X509_num(cert_stack) == 0)) {
if (!cert) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(02222)
"Cannot find peer certificate chain");
return HTTP_FORBIDDEN;
}
cert_stack = sk_X509_new_null();
sk_X509_push(cert_stack, cert);
}
if (!(cert_store ||
(cert_store = SSL_CTX_get_cert_store(ctx)))) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(02223)
"Cannot find certificate storage");
return HTTP_FORBIDDEN;
}
if (!cert) {
cert = sk_X509_value(cert_stack, 0);
}
cert_store_ctx = X509_STORE_CTX_new();
X509_STORE_CTX_init(cert_store_ctx, cert_store, cert, cert_stack);
depth = SSL_get_verify_depth(ssl);
if (depth >= 0) {
X509_STORE_CTX_set_depth(cert_store_ctx, depth);
}
X509_STORE_CTX_set_ex_data(cert_store_ctx,
SSL_get_ex_data_X509_STORE_CTX_idx(),
(char *)ssl);
if (!X509_verify_cert(cert_store_ctx)) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(02224)
"Re-negotiation verification step failed");
ssl_log_ssl_error(SSLLOG_MARK, APLOG_ERR, r->server);
}
SSL_set_verify_result(ssl, X509_STORE_CTX_get_error(cert_store_ctx));
X509_STORE_CTX_cleanup(cert_store_ctx);
X509_STORE_CTX_free(cert_store_ctx);
if (cert_stack != SSL_get_peer_cert_chain(ssl)) {
sk_X509_pop_free(cert_stack, X509_free);
}
} else {
char peekbuf[1];
const char *reneg_support;
request_rec *id = r->main ? r->main : r;
if (has_buffered_data(r)) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(02259)
"insecure SSL re-negotiation required, but "
"a pipelined request is present; keepalive "
"disabled");
r->connection->keepalive = AP_CONN_CLOSE;
}
#if defined(SSL_get_secure_renegotiation_support)
reneg_support = SSL_get_secure_renegotiation_support(ssl) ?
"client does" : "client does not";
#else
reneg_support = "server does not";
#endif
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(02260)
"Performing full renegotiation: complete handshake "
"protocol (%s support secure renegotiation)",
reneg_support);
SSL_set_session_id_context(ssl,
(unsigned char *)&id,
sizeof(id));
sslconn->reneg_state = RENEG_ALLOW;
SSL_renegotiate(ssl);
SSL_do_handshake(ssl);
if (!SSL_is_init_finished(ssl)) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(02225)
"Re-negotiation request failed");
ssl_log_ssl_error(SSLLOG_MARK, APLOG_ERR, r->server);
r->connection->keepalive = AP_CONN_CLOSE;
return HTTP_FORBIDDEN;
}
ap_log_rerror(APLOG_MARK, APLOG_INFO, 0, r, APLOGNO(02226)
"Awaiting re-negotiation handshake");
SSL_peek(ssl, peekbuf, 0);
sslconn->reneg_state = RENEG_REJECT;
if (!SSL_is_init_finished(ssl)) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(02261)
"Re-negotiation handshake failed");
ssl_log_ssl_error(SSLLOG_MARK, APLOG_ERR, r->server);
r->connection->keepalive = AP_CONN_CLOSE;
return HTTP_FORBIDDEN;
}
sslconn->server = r->server;
}
if ((cert = SSL_get_peer_certificate(ssl))) {
if (sslconn->client_cert) {
X509_free(sslconn->client_cert);
}
sslconn->client_cert = cert;
sslconn->client_dn = NULL;
}
if ((dc->nVerifyClient != SSL_CVERIFY_NONE) ||
(sc->server->auth.verify_mode != SSL_CVERIFY_NONE)) {
BOOL do_verify = ((dc->nVerifyClient == SSL_CVERIFY_REQUIRE) ||
(sc->server->auth.verify_mode == SSL_CVERIFY_REQUIRE));
if (do_verify && (SSL_get_verify_result(ssl) != X509_V_OK)) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(02262)
"Re-negotiation handshake failed: "
"Client verification failed");
return HTTP_FORBIDDEN;
}
if (do_verify) {
if ((peercert = SSL_get_peer_certificate(ssl)) == NULL) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(02263)
"Re-negotiation handshake failed: "
"Client certificate missing");
return HTTP_FORBIDDEN;
}
X509_free(peercert);
}
}
if (cipher_list) {
cipher = SSL_get_current_cipher(ssl);
if (sk_SSL_CIPHER_find(cipher_list, cipher) < 0) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(02264)
"SSL cipher suite not renegotiated: "
"access to %s denied using cipher %s",
r->filename,
SSL_CIPHER_get_name(cipher));
return HTTP_FORBIDDEN;
}
}
if (ncipher_suite) {
sslconn->cipher_suite = ncipher_suite;
}
}
if ((dc->nOptions & SSL_OPT_FAKEBASICAUTH) == 0 && dc->szUserName) {
char *val = ssl_var_lookup(r->pool, r->server, r->connection,
r, (char *)dc->szUserName);
if (val && val[0])
r->user = val;
else
ap_log_rerror(APLOG_MARK, APLOG_WARNING, 0, r, APLOGNO(02227)
"Failed to set r->user to '%s'", dc->szUserName);
}
requires = dc->aRequirement;
ssl_requires = (ssl_require_t *)requires->elts;
for (i = 0; i < requires->nelts; i++) {
ssl_require_t *req = &ssl_requires[i];
const char *errstring;
ok = ap_expr_exec(r, req->mpExpr, &errstring);
if (ok < 0) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(02265)
"access to %s failed, reason: Failed to execute "
"SSL requirement expression: %s",
r->filename, errstring);
apr_table_setn(r->notes, "ssl-access-forbidden", "1");
return HTTP_FORBIDDEN;
}
if (ok != 1) {
ap_log_rerror(APLOG_MARK, APLOG_INFO, 0, r, APLOGNO(02266)
"Access to %s denied for %s "
"(requirement expression not fulfilled)",
r->filename, r->useragent_ip);
ap_log_rerror(APLOG_MARK, APLOG_INFO, 0, r, APLOGNO(02228)
"Failed expression: %s", req->cpExpr);
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(02229)
"access to %s failed, reason: %s",
r->filename,
"SSL requirement expression not fulfilled");
apr_table_setn(r->notes, "ssl-access-forbidden", "1");
return HTTP_FORBIDDEN;
}
}
return DECLINED;
}
int ssl_hook_UserCheck(request_rec *r) {
SSLConnRec *sslconn = myConnConfig(r->connection);
SSLSrvConfigRec *sc = mySrvConfig(r->server);
SSLDirConfigRec *dc = myDirConfig(r);
char *clientdn;
const char *auth_line, *username, *password;
if ((dc->nOptions & SSL_OPT_STRICTREQUIRE) &&
(apr_table_get(r->notes, "ssl-access-forbidden"))) {
return HTTP_FORBIDDEN;
}
if (!ap_is_initial_req(r)) {
return DECLINED;
}
if ((auth_line = apr_table_get(r->headers_in, "Authorization"))) {
if (strcEQ(ap_getword(r->pool, &auth_line, ' '), "Basic")) {
while ((*auth_line == ' ') || (*auth_line == '\t')) {
auth_line++;
}
auth_line = ap_pbase64decode(r->pool, auth_line);
username = ap_getword_nulls(r->pool, &auth_line, ':');
password = auth_line;
if ((username[0] == '/') && strEQ(password, "password")) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(02035)
"Encountered FakeBasicAuth spoof: %s", username);
return HTTP_FORBIDDEN;
}
}
}
if (!((sc->enabled == SSL_ENABLED_TRUE || sc->enabled == SSL_ENABLED_OPTIONAL)
&& sslconn && sslconn->ssl && sslconn->client_cert) ||
!(dc->nOptions & SSL_OPT_FAKEBASICAUTH) || r->user) {
return DECLINED;
}
if (!sslconn->client_dn) {
X509_NAME *name = X509_get_subject_name(sslconn->client_cert);
char *cp = X509_NAME_oneline(name, NULL, 0);
sslconn->client_dn = apr_pstrdup(r->connection->pool, cp);
OPENSSL_free(cp);
}
clientdn = (char *)sslconn->client_dn;
auth_line = apr_pstrcat(r->pool, "Basic ",
ap_pbase64encode(r->pool,
apr_pstrcat(r->pool, clientdn,
":password", NULL)),
NULL);
apr_table_setn(r->headers_in, "Authorization", auth_line);
ap_log_rerror(APLOG_MARK, APLOG_INFO, 0, r, APLOGNO(02036)
"Faking HTTP Basic Auth header: \"Authorization: %s\"",
auth_line);
return DECLINED;
}
int ssl_hook_Auth(request_rec *r) {
SSLDirConfigRec *dc = myDirConfig(r);
if ((dc->nOptions & SSL_OPT_STRICTREQUIRE) &&
(apr_table_get(r->notes, "ssl-access-forbidden"))) {
return HTTP_FORBIDDEN;
}
return DECLINED;
}
static const char *const ssl_hook_Fixup_vars[] = {
"SSL_VERSION_INTERFACE",
"SSL_VERSION_LIBRARY",
"SSL_PROTOCOL",
"SSL_SECURE_RENEG",
"SSL_COMPRESS_METHOD",
"SSL_CIPHER",
"SSL_CIPHER_EXPORT",
"SSL_CIPHER_USEKEYSIZE",
"SSL_CIPHER_ALGKEYSIZE",
"SSL_CLIENT_VERIFY",
"SSL_CLIENT_M_VERSION",
"SSL_CLIENT_M_SERIAL",
"SSL_CLIENT_V_START",
"SSL_CLIENT_V_END",
"SSL_CLIENT_V_REMAIN",
"SSL_CLIENT_S_DN",
"SSL_CLIENT_I_DN",
"SSL_CLIENT_A_KEY",
"SSL_CLIENT_A_SIG",
"SSL_CLIENT_CERT_RFC4523_CEA",
"SSL_SERVER_M_VERSION",
"SSL_SERVER_M_SERIAL",
"SSL_SERVER_V_START",
"SSL_SERVER_V_END",
"SSL_SERVER_S_DN",
"SSL_SERVER_I_DN",
"SSL_SERVER_A_KEY",
"SSL_SERVER_A_SIG",
"SSL_SESSION_ID",
"SSL_SESSION_RESUMED",
#if defined(HAVE_SRP)
"SSL_SRP_USER",
"SSL_SRP_USERINFO",
#endif
NULL
};
int ssl_hook_Fixup(request_rec *r) {
SSLConnRec *sslconn = myConnConfig(r->connection);
SSLSrvConfigRec *sc = mySrvConfig(r->server);
SSLDirConfigRec *dc = myDirConfig(r);
apr_table_t *env = r->subprocess_env;
char *var, *val = "";
#if defined(HAVE_TLSEXT)
const char *servername;
#endif
STACK_OF(X509) *peer_certs;
SSL *ssl;
int i;
if (!(sslconn && sslconn->ssl) && r->connection->master) {
sslconn = myConnConfig(r->connection->master);
}
if (!(((sc->enabled == SSL_ENABLED_TRUE) || (sc->enabled == SSL_ENABLED_OPTIONAL)) && sslconn && (ssl = sslconn->ssl))) {
return DECLINED;
}
apr_table_setn(env, "HTTPS", "on");
#if defined(HAVE_TLSEXT)
if ((servername = SSL_get_servername(ssl, TLSEXT_NAMETYPE_host_name))) {
apr_table_set(env, "SSL_TLS_SNI", servername);
}
#endif
if (dc->nOptions & SSL_OPT_STDENVVARS) {
modssl_var_extract_dns(env, ssl, r->pool);
modssl_var_extract_san_entries(env, ssl, r->pool);
for (i = 0; ssl_hook_Fixup_vars[i]; i++) {
var = (char *)ssl_hook_Fixup_vars[i];
val = ssl_var_lookup(r->pool, r->server, r->connection, r, var);
if (!strIsEmpty(val)) {
apr_table_setn(env, var, val);
}
}
}
if (dc->nOptions & SSL_OPT_EXPORTCERTDATA) {
val = ssl_var_lookup(r->pool, r->server, r->connection,
r, "SSL_SERVER_CERT");
apr_table_setn(env, "SSL_SERVER_CERT", val);
val = ssl_var_lookup(r->pool, r->server, r->connection,
r, "SSL_CLIENT_CERT");
apr_table_setn(env, "SSL_CLIENT_CERT", val);
if ((peer_certs = (STACK_OF(X509) *)SSL_get_peer_cert_chain(ssl))) {
for (i = 0; i < sk_X509_num(peer_certs); i++) {
var = apr_psprintf(r->pool, "SSL_CLIENT_CERT_CHAIN_%d", i);
val = ssl_var_lookup(r->pool, r->server, r->connection,
r, var);
if (val) {
apr_table_setn(env, var, val);
}
}
}
}
#if defined(SSL_get_secure_renegotiation_support)
apr_table_setn(r->notes, "ssl-secure-reneg",
SSL_get_secure_renegotiation_support(ssl) ? "1" : "0");
#endif
return DECLINED;
}
static authz_status ssl_authz_require_ssl_check(request_rec *r,
const char *require_line,
const void *parsed) {
SSLConnRec *sslconn = myConnConfig(r->connection);
SSL *ssl = sslconn ? sslconn->ssl : NULL;
if (ssl)
return AUTHZ_GRANTED;
else
return AUTHZ_DENIED;
}
static const char *ssl_authz_require_ssl_parse(cmd_parms *cmd,
const char *require_line,
const void **parsed) {
if (require_line && require_line[0])
return "'Require ssl' does not take arguments";
return NULL;
}
const authz_provider ssl_authz_provider_require_ssl = {
&ssl_authz_require_ssl_check,
&ssl_authz_require_ssl_parse,
};
static authz_status ssl_authz_verify_client_check(request_rec *r,
const char *require_line,
const void *parsed) {
SSLConnRec *sslconn = myConnConfig(r->connection);
SSL *ssl = sslconn ? sslconn->ssl : NULL;
if (!ssl)
return AUTHZ_DENIED;
if (sslconn->verify_error == NULL &&
sslconn->verify_info == NULL &&
SSL_get_verify_result(ssl) == X509_V_OK) {
X509 *xs = SSL_get_peer_certificate(ssl);
if (xs) {
X509_free(xs);
return AUTHZ_GRANTED;
} else {
X509_free(xs);
}
}
return AUTHZ_DENIED;
}
static const char *ssl_authz_verify_client_parse(cmd_parms *cmd,
const char *require_line,
const void **parsed) {
if (require_line && require_line[0])
return "'Require ssl-verify-client' does not take arguments";
return NULL;
}
const authz_provider ssl_authz_provider_verify_client = {
&ssl_authz_verify_client_check,
&ssl_authz_verify_client_parse,
};
DH *ssl_callback_TmpDH(SSL *ssl, int export, int keylen) {
conn_rec *c = (conn_rec *)SSL_get_app_data(ssl);
EVP_PKEY *pkey;
int type;
#if defined(SSL_CERT_SET_SERVER)
SSL_set_current_cert(ssl, SSL_CERT_SET_SERVER);
#endif
pkey = SSL_get_privatekey(ssl);
#if OPENSSL_VERSION_NUMBER < 0x10100000L
type = pkey ? EVP_PKEY_type(pkey->type) : EVP_PKEY_NONE;
#else
type = pkey ? EVP_PKEY_base_id(pkey) : EVP_PKEY_NONE;
#endif
if ((type == EVP_PKEY_RSA) || (type == EVP_PKEY_DSA)) {
keylen = EVP_PKEY_bits(pkey);
}
ap_log_cerror(APLOG_MARK, APLOG_TRACE2, 0, c,
"handing out built-in DH parameters for %d-bit authenticated connection", keylen);
return modssl_get_dh_params(keylen);
}
int ssl_callback_SSLVerify(int ok, X509_STORE_CTX *ctx) {
SSL *ssl = X509_STORE_CTX_get_ex_data(ctx,
SSL_get_ex_data_X509_STORE_CTX_idx());
conn_rec *conn = (conn_rec *)SSL_get_app_data(ssl);
request_rec *r = (request_rec *)modssl_get_app_data2(ssl);
server_rec *s = r ? r->server : mySrvFromConn(conn);
SSLSrvConfigRec *sc = mySrvConfig(s);
SSLDirConfigRec *dc = r ? myDirConfig(r) : NULL;
SSLConnRec *sslconn = myConnConfig(conn);
modssl_ctx_t *mctx = myCtxConfig(sslconn, sc);
int crl_check_mode = mctx->crl_check_mask & ~SSL_CRLCHECK_FLAGS;
int errnum = X509_STORE_CTX_get_error(ctx);
int errdepth = X509_STORE_CTX_get_error_depth(ctx);
int depth, verify;
ssl_log_cxerror(SSLLOG_MARK, APLOG_DEBUG, 0, conn,
X509_STORE_CTX_get_current_cert(ctx), APLOGNO(02275)
"Certificate Verification, depth %d, "
"CRL checking mode: %s (%x)", errdepth,
crl_check_mode == SSL_CRLCHECK_CHAIN ? "chain" :
crl_check_mode == SSL_CRLCHECK_LEAF ? "leaf" : "none",
mctx->crl_check_mask);
if (dc && (dc->nVerifyClient != SSL_CVERIFY_UNSET)) {
verify = dc->nVerifyClient;
} else {
verify = mctx->auth.verify_mode;
}
if (verify == SSL_CVERIFY_NONE) {
return TRUE;
}
if (ssl_verify_error_is_optional(errnum) &&
(verify == SSL_CVERIFY_OPTIONAL_NO_CA)) {
ap_log_cerror(APLOG_MARK, APLOG_DEBUG, 0, conn, APLOGNO(02037)
"Certificate Verification: Verifiable Issuer is "
"configured as optional, therefore we're accepting "
"the certificate");
sslconn->verify_info = "GENEROUS";
ok = TRUE;
}
if (!ok && errnum == X509_V_ERR_CRL_HAS_EXPIRED) {
X509_STORE_CTX_set_error(ctx, -1);
}
if (!ok && errnum == X509_V_ERR_UNABLE_TO_GET_CRL
&& (mctx->crl_check_mask & SSL_CRLCHECK_NO_CRL_FOR_CERT_OK)) {
ap_log_cerror(APLOG_MARK, APLOG_TRACE3, 0, conn,
"Certificate Verification: Temporary error (%d): %s: "
"optional therefore we're accepting the certificate",
errnum, X509_verify_cert_error_string(errnum));
X509_STORE_CTX_set_error(ctx, X509_V_OK);
errnum = X509_V_OK;
ok = TRUE;
}
#if !defined(OPENSSL_NO_OCSP)
if (ok && sc->server->ocsp_enabled) {
if (ssl_verify_error_is_optional(errnum)) {
X509_STORE_CTX_set_error(ctx, X509_V_ERR_APPLICATION_VERIFICATION);
errnum = X509_V_ERR_APPLICATION_VERIFICATION;
ap_log_cerror(APLOG_MARK, APLOG_ERR, 0, conn, APLOGNO(02038)
"cannot perform OCSP validation for cert "
"if issuer has not been verified "
"(optional_no_ca configured)");
ok = FALSE;
} else {
ok = modssl_verify_ocsp(ctx, sc, s, conn, conn->pool);
if (!ok) {
errnum = X509_STORE_CTX_get_error(ctx);
}
}
}
#endif
if (!ok) {
if (APLOGcinfo(conn)) {
ssl_log_cxerror(SSLLOG_MARK, APLOG_INFO, 0, conn,
X509_STORE_CTX_get_current_cert(ctx), APLOGNO(02276)
"Certificate Verification: Error (%d): %s",
errnum, X509_verify_cert_error_string(errnum));
} else {
ap_log_cerror(APLOG_MARK, APLOG_ERR, 0, conn, APLOGNO(02039)
"Certificate Verification: Error (%d): %s",
errnum, X509_verify_cert_error_string(errnum));
}
if (sslconn->client_cert) {
X509_free(sslconn->client_cert);
sslconn->client_cert = NULL;
}
sslconn->client_dn = NULL;
sslconn->verify_error = X509_verify_cert_error_string(errnum);
}
if (dc && (dc->nVerifyDepth != UNSET)) {
depth = dc->nVerifyDepth;
} else {
depth = mctx->auth.verify_depth;
}
if (errdepth > depth) {
ap_log_cerror(APLOG_MARK, APLOG_ERR, 0, conn, APLOGNO(02040)
"Certificate Verification: Certificate Chain too long "
"(chain has %d certificates, but maximum allowed are "
"only %d)",
errdepth, depth);
errnum = X509_V_ERR_CERT_CHAIN_TOO_LONG;
sslconn->verify_error = X509_verify_cert_error_string(errnum);
ok = FALSE;
}
return ok;
}
#define SSLPROXY_CERT_CB_LOG_FMT "Proxy client certificate callback: (%s) "
static void modssl_proxy_info_log(conn_rec *c,
X509_INFO *info,
const char *msg) {
ssl_log_cxerror(SSLLOG_MARK, APLOG_DEBUG, 0, c, info->x509, APLOGNO(02277)
SSLPROXY_CERT_CB_LOG_FMT "%s, sending",
(mySrvConfigFromConn(c))->vhost_id, msg);
}
#if MODSSL_USE_OPENSSL_PRE_1_1_API
#define modssl_set_cert_info(info, cert, pkey) *cert = info->x509; CRYPTO_add(&(*cert)->references, +1, CRYPTO_LOCK_X509); *pkey = info->x_pkey->dec_pkey; CRYPTO_add(&(*pkey)->references, +1, CRYPTO_LOCK_X509_PKEY)
#else
#define modssl_set_cert_info(info, cert, pkey) *cert = info->x509; X509_up_ref(*cert); *pkey = info->x_pkey->dec_pkey; EVP_PKEY_up_ref(*pkey);
#endif
int ssl_callback_proxy_cert(SSL *ssl, X509 **x509, EVP_PKEY **pkey) {
conn_rec *c = (conn_rec *)SSL_get_app_data(ssl);
server_rec *s = mySrvFromConn(c);
SSLSrvConfigRec *sc = mySrvConfig(s);
X509_NAME *ca_name, *issuer, *ca_issuer;
X509_INFO *info;
X509 *ca_cert;
STACK_OF(X509_NAME) *ca_list;
STACK_OF(X509_INFO) *certs = sc->proxy->pkp->certs;
STACK_OF(X509) *ca_certs;
STACK_OF(X509) **ca_cert_chains;
int i, j, k;
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s, APLOGNO(02267)
SSLPROXY_CERT_CB_LOG_FMT "entered",
sc->vhost_id);
if (!certs || (sk_X509_INFO_num(certs) <= 0)) {
ap_log_error(APLOG_MARK, APLOG_WARNING, 0, s, APLOGNO(02268)
SSLPROXY_CERT_CB_LOG_FMT
"downstream server wanted client certificate "
"but none are configured", sc->vhost_id);
return FALSE;
}
ca_list = SSL_get_client_CA_list(ssl);
if (!ca_list || (sk_X509_NAME_num(ca_list) <= 0)) {
info = sk_X509_INFO_value(certs, 0);
modssl_proxy_info_log(c, info, APLOGNO(02278) "no acceptable CA list");
modssl_set_cert_info(info, x509, pkey);
return TRUE;
}
ca_cert_chains = sc->proxy->pkp->ca_certs;
for (i = 0; i < sk_X509_NAME_num(ca_list); i++) {
ca_name = sk_X509_NAME_value(ca_list, i);
for (j = 0; j < sk_X509_INFO_num(certs); j++) {
info = sk_X509_INFO_value(certs, j);
issuer = X509_get_issuer_name(info->x509);
if (X509_NAME_cmp(issuer, ca_name) == 0) {
modssl_proxy_info_log(c, info, APLOGNO(02279)
"found acceptable cert");
modssl_set_cert_info(info, x509, pkey);
return TRUE;
}
if (ca_cert_chains) {
ca_certs = ca_cert_chains[j];
for (k = 0; k < sk_X509_num(ca_certs); k++) {
ca_cert = sk_X509_value(ca_certs, k);
ca_issuer = X509_get_issuer_name(ca_cert);
if(X509_NAME_cmp(ca_issuer, ca_name) == 0 ) {
modssl_proxy_info_log(c, info, APLOGNO(02280)
"found acceptable cert by intermediate CA");
modssl_set_cert_info(info, x509, pkey);
return TRUE;
}
}
}
}
}
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s, APLOGNO(02269)
SSLPROXY_CERT_CB_LOG_FMT
"no client certificate found!?", sc->vhost_id);
return FALSE;
}
static void ssl_session_log(server_rec *s,
const char *request,
IDCONST unsigned char *id,
unsigned int idlen,
const char *status,
const char *result,
long timeout) {
char buf[MODSSL_SESSION_ID_STRING_LEN];
char timeout_str[56] = {'\0'};
if (!APLOGdebug(s)) {
return;
}
if (timeout) {
apr_snprintf(timeout_str, sizeof(timeout_str),
"timeout=%lds ", timeout);
}
ap_log_error(APLOG_MARK, APLOG_TRACE2, 0, s,
"Inter-Process Session Cache: "
"request=%s status=%s id=%s %s(session %s)",
request, status,
modssl_SSL_SESSION_id2sz(id, idlen, buf, sizeof(buf)),
timeout_str, result);
}
int ssl_callback_NewSessionCacheEntry(SSL *ssl, SSL_SESSION *session) {
conn_rec *conn = (conn_rec *)SSL_get_app_data(ssl);
server_rec *s = mySrvFromConn(conn);
SSLSrvConfigRec *sc = mySrvConfig(s);
long timeout = sc->session_cache_timeout;
BOOL rc;
IDCONST unsigned char *id;
unsigned int idlen;
SSL_set_timeout(session, timeout);
#if defined(OPENSSL_NO_SSL_INTERN)
id = (unsigned char *)SSL_SESSION_get_id(session, &idlen);
#else
id = session->session_id;
idlen = session->session_id_length;
#endif
rc = ssl_scache_store(s, id, idlen,
apr_time_from_sec(SSL_SESSION_get_time(session)
+ timeout),
session, conn->pool);
ssl_session_log(s, "SET", id, idlen,
rc == TRUE ? "OK" : "BAD",
"caching", timeout);
return 0;
}
SSL_SESSION *ssl_callback_GetSessionCacheEntry(SSL *ssl,
IDCONST unsigned char *id,
int idlen, int *do_copy) {
conn_rec *conn = (conn_rec *)SSL_get_app_data(ssl);
server_rec *s = mySrvFromConn(conn);
SSL_SESSION *session;
session = ssl_scache_retrieve(s, id, idlen, conn->pool);
ssl_session_log(s, "GET", id, idlen,
session ? "FOUND" : "MISSED",
session ? "reuse" : "renewal", 0);
*do_copy = 0;
return session;
}
void ssl_callback_DelSessionCacheEntry(SSL_CTX *ctx,
SSL_SESSION *session) {
server_rec *s;
SSLSrvConfigRec *sc;
IDCONST unsigned char *id;
unsigned int idlen;
if (!(s = (server_rec *)SSL_CTX_get_app_data(ctx))) {
return;
}
sc = mySrvConfig(s);
#if defined(OPENSSL_NO_SSL_INTERN)
id = (unsigned char *)SSL_SESSION_get_id(session, &idlen);
#else
id = session->session_id;
idlen = session->session_id_length;
#endif
ssl_scache_remove(s, id, idlen, sc->mc->pPool);
ssl_session_log(s, "REM", id, idlen,
"OK", "dead", 0);
return;
}
static void log_tracing_state(const SSL *ssl, conn_rec *c,
server_rec *s, int where, int rc) {
if (where & SSL_CB_HANDSHAKE_START) {
ap_log_cerror(APLOG_MARK, APLOG_TRACE3, 0, c,
"%s: Handshake: start", MODSSL_LIBRARY_NAME);
} else if (where & SSL_CB_HANDSHAKE_DONE) {
ap_log_cerror(APLOG_MARK, APLOG_TRACE3, 0, c,
"%s: Handshake: done", MODSSL_LIBRARY_NAME);
} else if (where & SSL_CB_LOOP) {
ap_log_cerror(APLOG_MARK, APLOG_TRACE3, 0, c,
"%s: Loop: %s",
MODSSL_LIBRARY_NAME, SSL_state_string_long(ssl));
} else if (where & SSL_CB_READ) {
ap_log_cerror(APLOG_MARK, APLOG_TRACE3, 0, c,
"%s: Read: %s",
MODSSL_LIBRARY_NAME, SSL_state_string_long(ssl));
} else if (where & SSL_CB_WRITE) {
ap_log_cerror(APLOG_MARK, APLOG_TRACE3, 0, c,
"%s: Write: %s",
MODSSL_LIBRARY_NAME, SSL_state_string_long(ssl));
} else if (where & SSL_CB_ALERT) {
char *str = (where & SSL_CB_READ) ? "read" : "write";
ap_log_cerror(APLOG_MARK, APLOG_TRACE3, 0, c,
"%s: Alert: %s:%s:%s",
MODSSL_LIBRARY_NAME, str,
SSL_alert_type_string_long(rc),
SSL_alert_desc_string_long(rc));
} else if (where & SSL_CB_EXIT) {
if (rc == 0) {
ap_log_cerror(APLOG_MARK, APLOG_TRACE3, 0, c,
"%s: Exit: failed in %s",
MODSSL_LIBRARY_NAME, SSL_state_string_long(ssl));
} else if (rc < 0) {
ap_log_cerror(APLOG_MARK, APLOG_TRACE3, 0, c,
"%s: Exit: error in %s",
MODSSL_LIBRARY_NAME, SSL_state_string_long(ssl));
}
}
if (where & SSL_CB_HANDSHAKE_DONE) {
ap_log_cerror(APLOG_MARK, APLOG_DEBUG, 0, c, APLOGNO(02041)
"Protocol: %s, Cipher: %s (%s/%s bits)",
ssl_var_lookup(NULL, s, c, NULL, "SSL_PROTOCOL"),
ssl_var_lookup(NULL, s, c, NULL, "SSL_CIPHER"),
ssl_var_lookup(NULL, s, c, NULL, "SSL_CIPHER_USEKEYSIZE"),
ssl_var_lookup(NULL, s, c, NULL, "SSL_CIPHER_ALGKEYSIZE"));
}
}
void ssl_callback_Info(const SSL *ssl, int where, int rc) {
conn_rec *c;
server_rec *s;
SSLConnRec *scr;
if ((c = (conn_rec *)SSL_get_app_data((SSL *)ssl)) == NULL) {
return;
}
if ((scr = myConnConfig(c)) == NULL) {
return;
}
if (!scr->is_proxy &&
(where & SSL_CB_HANDSHAKE_START) &&
scr->reneg_state == RENEG_REJECT) {
scr->reneg_state = RENEG_ABORT;
ap_log_cerror(APLOG_MARK, APLOG_ERR, 0, c, APLOGNO(02042)
"rejecting client initiated renegotiation");
}
else if ((where & SSL_CB_HANDSHAKE_DONE) && scr->reneg_state == RENEG_INIT) {
scr->reneg_state = RENEG_REJECT;
}
s = mySrvFromConn(c);
if (s && APLOGdebug(s)) {
log_tracing_state(ssl, c, s, where, rc);
}
}
#if defined(HAVE_TLSEXT)
static apr_status_t init_vhost(conn_rec *c, SSL *ssl) {
const char *servername;
if (c) {
SSLConnRec *sslcon = myConnConfig(c);
if (sslcon->server != c->base_server) {
return APR_SUCCESS;
}
servername = SSL_get_servername(ssl, TLSEXT_NAMETYPE_host_name);
if (servername) {
if (ap_vhost_iterate_given_conn(c, ssl_find_vhost,
(void *)servername)) {
ap_log_cerror(APLOG_MARK, APLOG_DEBUG, 0, c, APLOGNO(02043)
"SSL virtual host for servername %s found",
servername);
return APR_SUCCESS;
} else {
ap_log_cerror(APLOG_MARK, APLOG_DEBUG, 0, c, APLOGNO(02044)
"No matching SSL virtual host for servername "
"%s found (using default/first virtual host)",
servername);
}
} else {
ap_log_cerror(APLOG_MARK, APLOG_DEBUG, 0, c, APLOGNO(02645)
"Server name not provided via TLS extension "
"(using default/first virtual host)");
}
}
return APR_NOTFOUND;
}
int ssl_callback_ServerNameIndication(SSL *ssl, int *al, modssl_ctx_t *mctx) {
conn_rec *c = (conn_rec *)SSL_get_app_data(ssl);
apr_status_t status = init_vhost(c, ssl);
return (status == APR_SUCCESS)? SSL_TLSEXT_ERR_OK : SSL_TLSEXT_ERR_NOACK;
}
static int ssl_find_vhost(void *servername, conn_rec *c, server_rec *s) {
SSLSrvConfigRec *sc;
SSL *ssl;
BOOL found;
SSLConnRec *sslcon;
found = ssl_util_vhost_matches(servername, s);
sslcon = myConnConfig(c);
if (found && (ssl = sslcon->ssl) &&
(sc = mySrvConfig(s))) {
SSL_CTX *ctx = SSL_set_SSL_CTX(ssl, sc->server->ssl_ctx);
SSL_set_options(ssl, SSL_CTX_get_options(ctx));
if ((SSL_get_verify_mode(ssl) == SSL_VERIFY_NONE) ||
(SSL_num_renegotiations(ssl) == 0)) {
SSL_set_verify(ssl, SSL_CTX_get_verify_mode(ctx),
SSL_CTX_get_verify_callback(ctx));
}
if (SSL_num_renegotiations(ssl) == 0) {
unsigned char *sid_ctx =
(unsigned char *)ap_md5_binary(c->pool,
(unsigned char *)sc->vhost_id,
sc->vhost_id_len);
SSL_set_session_id_context(ssl, sid_ctx, APR_MD5_DIGESTSIZE*2);
}
sslcon->server = s;
sslcon->cipher_suite = sc->server->auth.cipher_suite;
ap_update_child_status_from_server(c->sbh, SERVER_BUSY_READ, c, s);
if (APLOGtrace4(s)) {
BIO *rbio = SSL_get_rbio(ssl),
*wbio = SSL_get_wbio(ssl);
BIO_set_callback(rbio, ssl_io_data_cb);
BIO_set_callback_arg(rbio, (void *)ssl);
if (wbio && wbio != rbio) {
BIO_set_callback(wbio, ssl_io_data_cb);
BIO_set_callback_arg(wbio, (void *)ssl);
}
}
return 1;
}
return 0;
}
#endif
#if defined(HAVE_TLS_SESSION_TICKETS)
int ssl_callback_SessionTicket(SSL *ssl,
unsigned char *keyname,
unsigned char *iv,
EVP_CIPHER_CTX *cipher_ctx,
HMAC_CTX *hctx,
int mode) {
conn_rec *c = (conn_rec *)SSL_get_app_data(ssl);
server_rec *s = mySrvFromConn(c);
SSLSrvConfigRec *sc = mySrvConfig(s);
SSLConnRec *sslconn = myConnConfig(c);
modssl_ctx_t *mctx = myCtxConfig(sslconn, sc);
modssl_ticket_key_t *ticket_key = mctx->ticket_key;
if (mode == 1) {
if (ticket_key == NULL) {
return -1;
}
memcpy(keyname, ticket_key->key_name, 16);
RAND_bytes(iv, EVP_MAX_IV_LENGTH);
EVP_EncryptInit_ex(cipher_ctx, EVP_aes_128_cbc(), NULL,
ticket_key->aes_key, iv);
HMAC_Init_ex(hctx, ticket_key->hmac_secret, 16, tlsext_tick_md(), NULL);
ap_log_cerror(APLOG_MARK, APLOG_DEBUG, 0, c, APLOGNO(02289)
"TLS session ticket key for %s successfully set, "
"creating new session ticket", sc->vhost_id);
return 1;
} else if (mode == 0) {
if (ticket_key == NULL || memcmp(keyname, ticket_key->key_name, 16)) {
return 0;
}
EVP_DecryptInit_ex(cipher_ctx, EVP_aes_128_cbc(), NULL,
ticket_key->aes_key, iv);
HMAC_Init_ex(hctx, ticket_key->hmac_secret, 16, tlsext_tick_md(), NULL);
ap_log_cerror(APLOG_MARK, APLOG_DEBUG, 0, c, APLOGNO(02290)
"TLS session ticket key for %s successfully set, "
"decrypting existing session ticket", sc->vhost_id);
return 1;
}
return -1;
}
#endif
#if defined(HAVE_TLS_ALPN)
int ssl_callback_alpn_select(SSL *ssl,
const unsigned char **out, unsigned char *outlen,
const unsigned char *in, unsigned int inlen,
void *arg) {
conn_rec *c = (conn_rec*)SSL_get_app_data(ssl);
SSLConnRec *sslconn = myConnConfig(c);
apr_array_header_t *client_protos;
const char *proposed;
size_t len;
int i;
if (c == NULL) {
return SSL_TLSEXT_ERR_OK;
}
if (inlen == 0) {
ap_log_cerror(APLOG_MARK, APLOG_ERR, 0, c, APLOGNO(02837)
"ALPN client protocol list empty");
return SSL_TLSEXT_ERR_ALERT_FATAL;
}
client_protos = apr_array_make(c->pool, 0, sizeof(char *));
for (i = 0; i < inlen; ) {
unsigned int plen = in[i++];
if (plen + i > inlen) {
ap_log_cerror(APLOG_MARK, APLOG_ERR, 0, c, APLOGNO(02838)
"ALPN protocol identifier too long");
return SSL_TLSEXT_ERR_ALERT_FATAL;
}
APR_ARRAY_PUSH(client_protos, char *) =
apr_pstrndup(c->pool, (const char *)in+i, plen);
i += plen;
}
init_vhost(c, ssl);
proposed = ap_select_protocol(c, NULL, sslconn->server, client_protos);
if (!proposed) {
proposed = ap_get_protocol(c);
}
len = strlen(proposed);
if (len > 255) {
ap_log_cerror(APLOG_MARK, APLOG_ERR, 0, c, APLOGNO(02840)
"ALPN negotiated protocol name too long");
return SSL_TLSEXT_ERR_ALERT_FATAL;
}
*out = (const unsigned char *)proposed;
*outlen = (unsigned char)len;
if (strcmp(proposed, ap_get_protocol(c))) {
apr_status_t status;
status = ap_switch_protocol(c, NULL, sslconn->server, proposed);
if (status != APR_SUCCESS) {
ap_log_cerror(APLOG_MARK, APLOG_ERR, status, c,
APLOGNO(02908) "protocol switch to '%s' failed",
proposed);
return SSL_TLSEXT_ERR_ALERT_FATAL;
}
}
return SSL_TLSEXT_ERR_OK;
}
#endif
#if defined(HAVE_SRP)
int ssl_callback_SRPServerParams(SSL *ssl, int *ad, void *arg) {
modssl_ctx_t *mctx = (modssl_ctx_t *)arg;
char *username = SSL_get_srp_username(ssl);
SRP_user_pwd *u;
if (username == NULL
#if OPENSSL_VERSION_NUMBER < 0x10100000L
|| (u = SRP_VBASE_get_by_user(mctx->srp_vbase, username)) == NULL) {
#else
|| (u = SRP_VBASE_get1_by_user(mctx->srp_vbase, username)) == NULL) {
#endif
*ad = SSL_AD_UNKNOWN_PSK_IDENTITY;
return SSL3_AL_FATAL;
}
if (SSL_set_srp_server_param(ssl, u->N, u->g, u->s, u->v, u->info) < 0) {
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
SRP_user_pwd_free(u);
#endif
*ad = SSL_AD_INTERNAL_ERROR;
return SSL3_AL_FATAL;
}
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
SRP_user_pwd_free(u);
#endif
SSL_set_verify(ssl, SSL_VERIFY_NONE, ssl_callback_SSLVerify);
return SSL_ERROR_NONE;
}
#endif