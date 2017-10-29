#include "ssl_private.h"
#include "mod_ssl.h"
#include "mod_ssl_openssl.h"
#include "util_md5.h"
#include "util_mutex.h"
#include "ap_provider.h"
#include "http_config.h"
#include <assert.h>
static int modssl_running_statically = 0;
APR_IMPLEMENT_OPTIONAL_HOOK_RUN_ALL(ssl, SSL, int, pre_handshake,
(conn_rec *c,SSL *ssl,int is_proxy),
(c,ssl,is_proxy), OK, DECLINED);
#define SSL_CMD_ALL(name, args, desc) AP_INIT_##args("SSL"#name, ssl_cmd_SSL##name, NULL, RSRC_CONF|OR_AUTHCFG, desc),
#define SSL_CMD_SRV(name, args, desc) AP_INIT_##args("SSL"#name, ssl_cmd_SSL##name, NULL, RSRC_CONF, desc),
#define SSL_CMD_DIR(name, type, args, desc) AP_INIT_##args("SSL"#name, ssl_cmd_SSL##name, NULL, OR_##type, desc),
#define AP_END_CMD { NULL }
static const command_rec ssl_config_cmds[] = {
SSL_CMD_SRV(PassPhraseDialog, TAKE1,
"SSL dialog mechanism for the pass phrase query "
"('builtin', '|/path/to/pipe_program', "
"or 'exec:/path/to/cgi_program')")
SSL_CMD_SRV(SessionCache, TAKE1,
"SSL Session Cache storage "
"('none', 'nonenotnull', 'dbm:/path/to/file')")
#if defined(HAVE_OPENSSL_ENGINE_H) && defined(HAVE_ENGINE_INIT)
SSL_CMD_SRV(CryptoDevice, TAKE1,
"SSL external Crypto Device usage "
"('builtin', '...')")
#endif
SSL_CMD_SRV(RandomSeed, TAKE23,
"SSL Pseudo Random Number Generator (PRNG) seeding source "
"('startup|connect builtin|file:/path|exec:/path [bytes]')")
SSL_CMD_SRV(Engine, TAKE1,
"SSL switch for the protocol engine "
"('on', 'off')")
SSL_CMD_SRV(FIPS, FLAG,
"Enable FIPS-140 mode "
"(`on', `off')")
SSL_CMD_ALL(CipherSuite, TAKE1,
"Colon-delimited list of permitted SSL Ciphers "
"('XXX:...:XXX' - see manual)")
SSL_CMD_SRV(CertificateFile, TAKE1,
"SSL Server Certificate file "
"('/path/to/file' - PEM or DER encoded)")
SSL_CMD_SRV(CertificateKeyFile, TAKE1,
"SSL Server Private Key file "
"('/path/to/file' - PEM or DER encoded)")
SSL_CMD_SRV(CertificateChainFile, TAKE1,
"SSL Server CA Certificate Chain file "
"('/path/to/file' - PEM encoded)")
#if defined(HAVE_TLS_SESSION_TICKETS)
SSL_CMD_SRV(SessionTicketKeyFile, TAKE1,
"TLS session ticket encryption/decryption key file (RFC 5077) "
"('/path/to/file' - file with 48 bytes of random data)")
#endif
SSL_CMD_ALL(CACertificatePath, TAKE1,
"SSL CA Certificate path "
"('/path/to/dir' - contains PEM encoded files)")
SSL_CMD_ALL(CACertificateFile, TAKE1,
"SSL CA Certificate file "
"('/path/to/file' - PEM encoded)")
SSL_CMD_SRV(CADNRequestPath, TAKE1,
"SSL CA Distinguished Name path "
"('/path/to/dir' - symlink hashes to PEM of acceptable CA names to request)")
SSL_CMD_SRV(CADNRequestFile, TAKE1,
"SSL CA Distinguished Name file "
"('/path/to/file' - PEM encoded to derive acceptable CA names to request)")
SSL_CMD_SRV(CARevocationPath, TAKE1,
"SSL CA Certificate Revocation List (CRL) path "
"('/path/to/dir' - contains PEM encoded files)")
SSL_CMD_SRV(CARevocationFile, TAKE1,
"SSL CA Certificate Revocation List (CRL) file "
"('/path/to/file' - PEM encoded)")
SSL_CMD_SRV(CARevocationCheck, RAW_ARGS,
"SSL CA Certificate Revocation List (CRL) checking mode")
SSL_CMD_ALL(VerifyClient, TAKE1,
"SSL Client verify type "
"('none', 'optional', 'require', 'optional_no_ca')")
SSL_CMD_ALL(VerifyDepth, TAKE1,
"SSL Client verify depth "
"('N' - number of intermediate certificates)")
SSL_CMD_SRV(SessionCacheTimeout, TAKE1,
"SSL Session Cache object lifetime "
"('N' - number of seconds)")
#if defined(OPENSSL_NO_SSL3)
#define SSLv3_PROTO_PREFIX ""
#else
#define SSLv3_PROTO_PREFIX "SSLv3|"
#endif
#if defined(HAVE_TLSV1_X)
#define SSL_PROTOCOLS SSLv3_PROTO_PREFIX "TLSv1|TLSv1.1|TLSv1.2"
#else
#define SSL_PROTOCOLS SSLv3_PROTO_PREFIX "TLSv1"
#endif
SSL_CMD_SRV(Protocol, RAW_ARGS,
"Enable or disable various SSL protocols "
"('[+-][" SSL_PROTOCOLS "] ...' - see manual)")
SSL_CMD_SRV(HonorCipherOrder, FLAG,
"Use the server's cipher ordering preference")
SSL_CMD_SRV(Compression, FLAG,
"Enable SSL level compression "
"(`on', `off')")
SSL_CMD_SRV(SessionTickets, FLAG,
"Enable or disable TLS session tickets"
"(`on', `off')")
SSL_CMD_SRV(InsecureRenegotiation, FLAG,
"Enable support for insecure renegotiation")
SSL_CMD_ALL(UserName, TAKE1,
"Set user name to SSL variable value")
SSL_CMD_SRV(StrictSNIVHostCheck, FLAG,
"Strict SNI virtual host checking")
#if defined(HAVE_SRP)
SSL_CMD_SRV(SRPVerifierFile, TAKE1,
"SRP verifier file "
"('/path/to/file' - created by srptool)")
SSL_CMD_SRV(SRPUnknownUserSeed, TAKE1,
"SRP seed for unknown users (to avoid leaking a user's existence) "
"('some secret text')")
#endif
SSL_CMD_SRV(ProxyEngine, FLAG,
"SSL switch for the proxy protocol engine "
"('on', 'off')")
SSL_CMD_SRV(ProxyProtocol, RAW_ARGS,
"SSL Proxy: enable or disable SSL protocol flavors "
"('[+-][" SSL_PROTOCOLS "] ...' - see manual)")
SSL_CMD_SRV(ProxyCipherSuite, TAKE1,
"SSL Proxy: colon-delimited list of permitted SSL ciphers "
"('XXX:...:XXX' - see manual)")
SSL_CMD_SRV(ProxyVerify, TAKE1,
"SSL Proxy: whether to verify the remote certificate "
"('on' or 'off')")
SSL_CMD_SRV(ProxyVerifyDepth, TAKE1,
"SSL Proxy: maximum certificate verification depth "
"('N' - number of intermediate certificates)")
SSL_CMD_SRV(ProxyCACertificateFile, TAKE1,
"SSL Proxy: file containing server certificates "
"('/path/to/file' - PEM encoded certificates)")
SSL_CMD_SRV(ProxyCACertificatePath, TAKE1,
"SSL Proxy: directory containing server certificates "
"('/path/to/dir' - contains PEM encoded certificates)")
SSL_CMD_SRV(ProxyCARevocationPath, TAKE1,
"SSL Proxy: CA Certificate Revocation List (CRL) path "
"('/path/to/dir' - contains PEM encoded files)")
SSL_CMD_SRV(ProxyCARevocationFile, TAKE1,
"SSL Proxy: CA Certificate Revocation List (CRL) file "
"('/path/to/file' - PEM encoded)")
SSL_CMD_SRV(ProxyCARevocationCheck, RAW_ARGS,
"SSL Proxy: CA Certificate Revocation List (CRL) checking mode")
SSL_CMD_SRV(ProxyMachineCertificateFile, TAKE1,
"SSL Proxy: file containing client certificates "
"('/path/to/file' - PEM encoded certificates)")
SSL_CMD_SRV(ProxyMachineCertificatePath, TAKE1,
"SSL Proxy: directory containing client certificates "
"('/path/to/dir' - contains PEM encoded certificates)")
SSL_CMD_SRV(ProxyMachineCertificateChainFile, TAKE1,
"SSL Proxy: file containing issuing certificates "
"of the client certificate "
"(`/path/to/file' - PEM encoded certificates)")
SSL_CMD_SRV(ProxyCheckPeerExpire, FLAG,
"SSL Proxy: check the peer certificate's expiration date")
SSL_CMD_SRV(ProxyCheckPeerCN, FLAG,
"SSL Proxy: check the peer certificate's CN")
SSL_CMD_SRV(ProxyCheckPeerName, FLAG,
"SSL Proxy: check the peer certificate's name "
"(must be present in subjectAltName extension or CN")
SSL_CMD_DIR(Options, OPTIONS, RAW_ARGS,
"Set one or more options to configure the SSL engine"
"('[+-]option[=value] ...' - see manual)")
SSL_CMD_DIR(RequireSSL, AUTHCFG, NO_ARGS,
"Require the SSL protocol for the per-directory context "
"(no arguments)")
SSL_CMD_DIR(Require, AUTHCFG, RAW_ARGS,
"Require a boolean expression to evaluate to true for granting access"
"(arbitrary complex boolean expression - see manual)")
SSL_CMD_DIR(RenegBufferSize, AUTHCFG, TAKE1,
"Configure the amount of memory that will be used for buffering the "
"request body if a per-location SSL renegotiation is required due to "
"changed access control requirements")
SSL_CMD_SRV(OCSPEnable, FLAG,
"Enable use of OCSP to verify certificate revocation ('on', 'off')")
SSL_CMD_SRV(OCSPDefaultResponder, TAKE1,
"URL of the default OCSP Responder")
SSL_CMD_SRV(OCSPOverrideResponder, FLAG,
"Force use of the default responder URL ('on', 'off')")
SSL_CMD_SRV(OCSPResponseTimeSkew, TAKE1,
"Maximum time difference in OCSP responses")
SSL_CMD_SRV(OCSPResponseMaxAge, TAKE1,
"Maximum age of OCSP responses")
SSL_CMD_SRV(OCSPResponderTimeout, TAKE1,
"OCSP responder query timeout")
SSL_CMD_SRV(OCSPUseRequestNonce, FLAG,
"Whether OCSP queries use a nonce or not ('on', 'off')")
SSL_CMD_SRV(OCSPProxyURL, TAKE1,
"Proxy URL to use for OCSP requests")
SSL_CMD_SRV(OCSPNoVerify, FLAG,
"Do not verify OCSP Responder certificate ('on', 'off')")
SSL_CMD_SRV(OCSPResponderCertificateFile, TAKE1,
"Trusted OCSP responder certificates"
"(`/path/to/file' - PEM encoded certificates)")
#if defined(HAVE_OCSP_STAPLING)
SSL_CMD_SRV(StaplingCache, TAKE1,
"SSL Stapling Response Cache storage "
"(`dbm:/path/to/file')")
SSL_CMD_SRV(UseStapling, FLAG,
"SSL switch for the OCSP Stapling protocol " "(`on', `off')")
SSL_CMD_SRV(StaplingResponseTimeSkew, TAKE1,
"SSL stapling option for maximum time difference in OCSP responses")
SSL_CMD_SRV(StaplingResponderTimeout, TAKE1,
"SSL stapling option for OCSP responder timeout")
SSL_CMD_SRV(StaplingResponseMaxAge, TAKE1,
"SSL stapling option for maximum age of OCSP responses")
SSL_CMD_SRV(StaplingStandardCacheTimeout, TAKE1,
"SSL stapling option for normal OCSP Response Cache Lifetime")
SSL_CMD_SRV(StaplingReturnResponderErrors, FLAG,
"SSL stapling switch to return Status Errors Back to Client"
"(`on', `off')")
SSL_CMD_SRV(StaplingFakeTryLater, FLAG,
"SSL stapling switch to send tryLater response to client on error "
"(`on', `off')")
SSL_CMD_SRV(StaplingErrorCacheTimeout, TAKE1,
"SSL stapling option for OCSP Response Error Cache Lifetime")
SSL_CMD_SRV(StaplingForceURL, TAKE1,
"SSL stapling option to Force the OCSP Stapling URL")
#endif
#if defined(HAVE_SSL_CONF_CMD)
SSL_CMD_SRV(OpenSSLConfCmd, TAKE2,
"OpenSSL configuration command")
#endif
AP_INIT_RAW_ARGS("SSLLog", ap_set_deprecated, NULL, OR_ALL,
"SSLLog directive is no longer supported - use ErrorLog."),
AP_INIT_RAW_ARGS("SSLLogLevel", ap_set_deprecated, NULL, OR_ALL,
"SSLLogLevel directive is no longer supported - use LogLevel."),
AP_END_CMD
};
static int modssl_is_prelinked(void) {
apr_size_t i = 0;
const module *mod;
while ((mod = ap_prelinked_modules[i++])) {
if (strcmp(mod->name, "mod_ssl.c") == 0) {
return 1;
}
}
return 0;
}
static apr_status_t ssl_cleanup_pre_config(void *data) {
#if defined(HAVE_FIPS)
FIPS_mode_set(0);
#endif
OBJ_cleanup();
CONF_modules_free();
EVP_cleanup();
#if HAVE_ENGINE_LOAD_BUILTIN_ENGINES
ENGINE_cleanup();
#endif
#if OPENSSL_VERSION_NUMBER >= 0x1000200fL
#if !defined(OPENSSL_NO_COMP)
SSL_COMP_free_compression_methods();
#endif
#endif
#if MODSSL_USE_OPENSSL_PRE_1_1_API
#if OPENSSL_VERSION_NUMBER >= 0x1000000fL
ERR_remove_thread_state(NULL);
#else
ERR_remove_state(0);
#endif
#endif
#if (OPENSSL_VERSION_NUMBER >= 0x00090805f)
ERR_free_strings();
#endif
if (!modssl_running_statically) {
CRYPTO_cleanup_all_ex_data();
}
return APR_SUCCESS;
}
static int ssl_hook_pre_config(apr_pool_t *pconf,
apr_pool_t *plog,
apr_pool_t *ptemp) {
modssl_running_statically = modssl_is_prelinked();
#if APR_HAS_THREADS && MODSSL_USE_OPENSSL_PRE_1_1_API
ssl_util_thread_id_setup(pconf);
#endif
#if MODSSL_USE_OPENSSL_PRE_1_1_API
(void)CRYPTO_malloc_init();
#else
OPENSSL_malloc_init();
#endif
ERR_load_crypto_strings();
SSL_load_error_strings();
SSL_library_init();
#if HAVE_ENGINE_LOAD_BUILTIN_ENGINES
ENGINE_load_builtin_engines();
#endif
OpenSSL_add_all_algorithms();
OPENSSL_load_builtin_modules();
if (OBJ_txt2nid("id-on-dnsSRV") == NID_undef) {
(void)OBJ_create("1.3.6.1.5.5.7.8.7", "id-on-dnsSRV",
"SRVName otherName form");
}
ERR_clear_error();
apr_pool_cleanup_register(pconf, NULL, ssl_cleanup_pre_config,
apr_pool_cleanup_null);
ssl_var_log_config_register(pconf);
ssl_scache_status_register(pconf);
ap_mutex_register(pconf, SSL_CACHE_MUTEX_TYPE, NULL, APR_LOCK_DEFAULT, 0);
#if defined(HAVE_OCSP_STAPLING)
ap_mutex_register(pconf, SSL_STAPLING_CACHE_MUTEX_TYPE, NULL,
APR_LOCK_DEFAULT, 0);
ap_mutex_register(pconf, SSL_STAPLING_REFRESH_MUTEX_TYPE, NULL,
APR_LOCK_DEFAULT, 0);
#endif
return OK;
}
static SSLConnRec *ssl_init_connection_ctx(conn_rec *c) {
SSLConnRec *sslconn = myConnConfig(c);
SSLSrvConfigRec *sc;
if (sslconn) {
return sslconn;
}
sslconn = apr_pcalloc(c->pool, sizeof(*sslconn));
sslconn->server = c->base_server;
sslconn->verify_depth = UNSET;
sc = mySrvConfig(c->base_server);
sslconn->cipher_suite = sc->server->auth.cipher_suite;
myConnConfigSet(c, sslconn);
return sslconn;
}
int ssl_proxy_enable(conn_rec *c) {
SSLSrvConfigRec *sc;
SSLConnRec *sslconn = ssl_init_connection_ctx(c);
sc = mySrvConfig(sslconn->server);
if (!sc->proxy_enabled) {
ap_log_cerror(APLOG_MARK, APLOG_ERR, 0, c, APLOGNO(01961)
"SSL Proxy requested for %s but not enabled "
"[Hint: SSLProxyEngine]", sc->vhost_id);
return 0;
}
sslconn->is_proxy = 1;
sslconn->disabled = 0;
return 1;
}
int ssl_engine_disable(conn_rec *c) {
SSLSrvConfigRec *sc;
SSLConnRec *sslconn = myConnConfig(c);
if (sslconn) {
sc = mySrvConfig(sslconn->server);
} else {
sc = mySrvConfig(c->base_server);
}
if (sc->enabled == SSL_ENABLED_FALSE) {
return 0;
}
sslconn = ssl_init_connection_ctx(c);
sslconn->disabled = 1;
return 1;
}
int ssl_init_ssl_connection(conn_rec *c, request_rec *r) {
SSLSrvConfigRec *sc;
SSL *ssl;
SSLConnRec *sslconn = myConnConfig(c);
char *vhost_md5;
int rc;
modssl_ctx_t *mctx;
server_rec *server;
if (!sslconn) {
sslconn = ssl_init_connection_ctx(c);
}
server = sslconn->server;
sc = mySrvConfig(server);
ssl_rand_seed(server, c->pool, SSL_RSCTX_CONNECT, "");
mctx = sslconn->is_proxy ? sc->proxy : sc->server;
if (!(sslconn->ssl = ssl = SSL_new(mctx->ssl_ctx))) {
ap_log_cerror(APLOG_MARK, APLOG_ERR, 0, c, APLOGNO(01962)
"Unable to create a new SSL connection from the SSL "
"context");
ssl_log_ssl_error(SSLLOG_MARK, APLOG_ERR, server);
c->aborted = 1;
return DECLINED;
}
rc = ssl_run_pre_handshake(c, ssl, sslconn->is_proxy ? 1 : 0);
if (rc != OK && rc != DECLINED) {
return rc;
}
vhost_md5 = ap_md5_binary(c->pool, (unsigned char *)sc->vhost_id,
sc->vhost_id_len);
if (!SSL_set_session_id_context(ssl, (unsigned char *)vhost_md5,
APR_MD5_DIGESTSIZE*2)) {
ap_log_cerror(APLOG_MARK, APLOG_ERR, 0, c, APLOGNO(01963)
"Unable to set session id context to '%s'", vhost_md5);
ssl_log_ssl_error(SSLLOG_MARK, APLOG_ERR, server);
c->aborted = 1;
return DECLINED;
}
SSL_set_app_data(ssl, c);
modssl_set_app_data2(ssl, NULL);
SSL_set_verify_result(ssl, X509_V_OK);
ssl_io_filter_init(c, r, ssl);
return APR_SUCCESS;
}
static const char *ssl_hook_http_scheme(const request_rec *r) {
SSLSrvConfigRec *sc = mySrvConfig(r->server);
if (sc->enabled == SSL_ENABLED_FALSE || sc->enabled == SSL_ENABLED_OPTIONAL) {
return NULL;
}
return "https";
}
static apr_port_t ssl_hook_default_port(const request_rec *r) {
SSLSrvConfigRec *sc = mySrvConfig(r->server);
if (sc->enabled == SSL_ENABLED_FALSE || sc->enabled == SSL_ENABLED_OPTIONAL) {
return 0;
}
return 443;
}
static int ssl_hook_pre_connection(conn_rec *c, void *csd) {
SSLSrvConfigRec *sc;
SSLConnRec *sslconn = myConnConfig(c);
if (sslconn) {
sc = mySrvConfig(sslconn->server);
} else {
sc = mySrvConfig(c->base_server);
}
if (c->master || !(sc && (sc->enabled == SSL_ENABLED_TRUE ||
(sslconn && sslconn->is_proxy)))) {
return DECLINED;
}
if (!sslconn) {
sslconn = ssl_init_connection_ctx(c);
}
if (sslconn->disabled) {
return DECLINED;
}
ap_log_cerror(APLOG_MARK, APLOG_INFO, 0, c, APLOGNO(01964)
"Connection to child %ld established "
"(server %s)", c->id, sc->vhost_id);
return ssl_init_ssl_connection(c, NULL);
}
static int ssl_hook_process_connection(conn_rec* c) {
SSLConnRec *sslconn = myConnConfig(c);
if (sslconn && !sslconn->disabled) {
apr_bucket_brigade* temp;
temp = apr_brigade_create(c->pool, c->bucket_alloc);
ap_get_brigade(c->input_filters, temp,
AP_MODE_INIT, APR_BLOCK_READ, 0);
apr_brigade_destroy(temp);
}
return DECLINED;
}
static void ssl_register_hooks(apr_pool_t *p) {
static const char *pre_prr[] = { "mod_setenvif.c", NULL };
ssl_io_filter_register(p);
ap_hook_pre_connection(ssl_hook_pre_connection,NULL,NULL, APR_HOOK_MIDDLE);
ap_hook_process_connection(ssl_hook_process_connection,
NULL, NULL, APR_HOOK_MIDDLE);
ap_hook_test_config (ssl_hook_ConfigTest, NULL,NULL, APR_HOOK_MIDDLE);
ap_hook_post_config (ssl_init_Module, NULL,NULL, APR_HOOK_MIDDLE);
ap_hook_http_scheme (ssl_hook_http_scheme, NULL,NULL, APR_HOOK_MIDDLE);
ap_hook_default_port (ssl_hook_default_port, NULL,NULL, APR_HOOK_MIDDLE);
ap_hook_pre_config (ssl_hook_pre_config, NULL,NULL, APR_HOOK_MIDDLE);
ap_hook_child_init (ssl_init_Child, NULL,NULL, APR_HOOK_MIDDLE);
ap_hook_check_authn (ssl_hook_UserCheck, NULL,NULL, APR_HOOK_FIRST,
AP_AUTH_INTERNAL_PER_CONF);
ap_hook_fixups (ssl_hook_Fixup, NULL,NULL, APR_HOOK_MIDDLE);
ap_hook_check_access (ssl_hook_Access, NULL,NULL, APR_HOOK_MIDDLE,
AP_AUTH_INTERNAL_PER_CONF);
ap_hook_check_authz (ssl_hook_Auth, NULL,NULL, APR_HOOK_MIDDLE,
AP_AUTH_INTERNAL_PER_CONF);
ap_hook_post_read_request(ssl_hook_ReadReq, pre_prr,NULL, APR_HOOK_MIDDLE);
ssl_var_register(p);
APR_REGISTER_OPTIONAL_FN(ssl_proxy_enable);
APR_REGISTER_OPTIONAL_FN(ssl_engine_disable);
ap_register_auth_provider(p, AUTHZ_PROVIDER_GROUP, "ssl",
AUTHZ_PROVIDER_VERSION,
&ssl_authz_provider_require_ssl,
AP_AUTH_INTERNAL_PER_CONF);
ap_register_auth_provider(p, AUTHZ_PROVIDER_GROUP, "ssl-verify-client",
AUTHZ_PROVIDER_VERSION,
&ssl_authz_provider_verify_client,
AP_AUTH_INTERNAL_PER_CONF);
}
module AP_MODULE_DECLARE_DATA ssl_module = {
STANDARD20_MODULE_STUFF,
ssl_config_perdir_create,
ssl_config_perdir_merge,
ssl_config_server_create,
ssl_config_server_merge,
ssl_config_cmds,
ssl_register_hooks
};
