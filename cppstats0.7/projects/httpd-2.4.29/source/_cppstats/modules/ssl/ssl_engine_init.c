#include "ssl_private.h"
#include "mod_ssl.h"
#include "mod_ssl_openssl.h"
#include "mpm_common.h"
APR_IMPLEMENT_OPTIONAL_HOOK_RUN_ALL(ssl, SSL, int, init_server,
(server_rec *s,apr_pool_t *p,int is_proxy,SSL_CTX *ctx),
(s,p,is_proxy,ctx), OK, DECLINED)
#if defined(HAVE_ECC)
#define KEYTYPES "RSA, DSA or ECC"
#else
#define KEYTYPES "RSA or DSA"
#endif
#if MODSSL_USE_OPENSSL_PRE_1_1_API
static int DH_set0_pqg(DH *dh, BIGNUM *p, BIGNUM *q, BIGNUM *g) {
if (p == NULL || g == NULL)
return 0;
BN_free(dh->p);
BN_free(dh->q);
BN_free(dh->g);
dh->p = p;
dh->q = q;
dh->g = g;
if (q != NULL) {
dh->length = BN_num_bits(q);
}
return 1;
}
#endif
static DH *make_dh_params(BIGNUM *(*prime)(BIGNUM *)) {
DH *dh = DH_new();
BIGNUM *p, *g;
if (!dh) {
return NULL;
}
p = prime(NULL);
g = BN_new();
if (g != NULL) {
BN_set_word(g, 2);
}
if (!p || !g || !DH_set0_pqg(dh, p, NULL, g)) {
DH_free(dh);
BN_free(p);
BN_free(g);
return NULL;
}
return dh;
}
static struct dhparam {
BIGNUM *(*const prime)(BIGNUM *);
DH *dh;
const unsigned int min;
} dhparams[] = {
{ BN_get_rfc3526_prime_8192, NULL, 6145 },
{ BN_get_rfc3526_prime_6144, NULL, 4097 },
{ BN_get_rfc3526_prime_4096, NULL, 3073 },
{ BN_get_rfc3526_prime_3072, NULL, 2049 },
{ BN_get_rfc3526_prime_2048, NULL, 1025 },
{ BN_get_rfc2409_prime_1024, NULL, 0 }
};
static void init_dh_params(void) {
unsigned n;
for (n = 0; n < sizeof(dhparams)/sizeof(dhparams[0]); n++)
dhparams[n].dh = make_dh_params(dhparams[n].prime);
}
static void free_dh_params(void) {
unsigned n;
for (n = 0; n < sizeof(dhparams)/sizeof(dhparams[0]); n++) {
DH_free(dhparams[n].dh);
dhparams[n].dh = NULL;
}
}
DH *modssl_get_dh_params(unsigned keylen) {
unsigned n;
for (n = 0; n < sizeof(dhparams)/sizeof(dhparams[0]); n++)
if (keylen >= dhparams[n].min)
return dhparams[n].dh;
return NULL;
}
static void ssl_add_version_components(apr_pool_t *p,
server_rec *s) {
char *modver = ssl_var_lookup(p, s, NULL, NULL, "SSL_VERSION_INTERFACE");
char *libver = ssl_var_lookup(p, s, NULL, NULL, "SSL_VERSION_LIBRARY");
char *incver = ssl_var_lookup(p, s, NULL, NULL,
"SSL_VERSION_LIBRARY_INTERFACE");
ap_add_version_component(p, libver);
ap_log_error(APLOG_MARK, APLOG_INFO, 0, s, APLOGNO(01876)
"%s compiled against Server: %s, Library: %s",
modver, AP_SERVER_BASEVERSION, incver);
}
apr_status_t ssl_init_Module(apr_pool_t *p, apr_pool_t *plog,
apr_pool_t *ptemp,
server_rec *base_server) {
SSLModConfigRec *mc = myModConfig(base_server);
SSLSrvConfigRec *sc;
server_rec *s;
apr_status_t rv;
apr_array_header_t *pphrases;
if (SSLeay() < MODSSL_LIBRARY_VERSION) {
ap_log_error(APLOG_MARK, APLOG_WARNING, 0, base_server, APLOGNO(01882)
"Init: this version of mod_ssl was compiled against "
"a newer library (%s, version currently loaded is %s)"
" - may result in undefined or erroneous behavior",
MODSSL_LIBRARY_TEXT, MODSSL_LIBRARY_DYNTEXT);
}
mc->pid = getpid();
apr_pool_cleanup_register(p, base_server,
ssl_init_ModuleKill,
apr_pool_cleanup_null);
ssl_config_global_create(base_server);
ssl_config_global_fix(mc);
for (s = base_server; s; s = s->next) {
sc = mySrvConfig(s);
if (sc->server) {
sc->server->sc = sc;
}
if (sc->proxy) {
sc->proxy->sc = sc;
}
sc->vhost_id = ssl_util_vhostid(p, s);
sc->vhost_id_len = strlen(sc->vhost_id);
if (ap_get_server_protocol(s)
&& strcmp("https", ap_get_server_protocol(s)) == 0
&& sc->enabled == SSL_ENABLED_UNSET) {
sc->enabled = SSL_ENABLED_TRUE;
}
if (sc->enabled == SSL_ENABLED_UNSET) {
sc->enabled = SSL_ENABLED_FALSE;
}
if (sc->proxy_enabled == UNSET) {
sc->proxy_enabled = FALSE;
}
if (sc->session_cache_timeout == UNSET) {
sc->session_cache_timeout = SSL_SESSION_CACHE_TIMEOUT;
}
if (sc->server && sc->server->pphrase_dialog_type == SSL_PPTYPE_UNSET) {
sc->server->pphrase_dialog_type = SSL_PPTYPE_BUILTIN;
}
#if defined(HAVE_FIPS)
if (sc->fips == UNSET) {
sc->fips = FALSE;
}
#endif
}
#if APR_HAS_THREADS && MODSSL_USE_OPENSSL_PRE_1_1_API
ssl_util_thread_setup(p);
#endif
#if defined(HAVE_OPENSSL_ENGINE_H) && defined(HAVE_ENGINE_INIT)
if ((rv = ssl_init_Engine(base_server, p)) != APR_SUCCESS) {
return rv;
}
#endif
ap_log_error(APLOG_MARK, APLOG_INFO, 0, s, APLOGNO(01883)
"Init: Initialized %s library", MODSSL_LIBRARY_NAME);
ssl_rand_seed(base_server, ptemp, SSL_RSCTX_STARTUP, "Init: ");
#if defined(HAVE_FIPS)
if(sc->fips) {
if (!FIPS_mode()) {
if (FIPS_mode_set(1)) {
ap_log_error(APLOG_MARK, APLOG_NOTICE, 0, s, APLOGNO(01884)
"Operating in SSL FIPS mode");
} else {
ap_log_error(APLOG_MARK, APLOG_EMERG, 0, s, APLOGNO(01885) "FIPS mode failed");
ssl_log_ssl_error(SSLLOG_MARK, APLOG_EMERG, s);
return ssl_die(s);
}
}
} else {
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s, APLOGNO(01886)
"SSL FIPS mode disabled");
}
#endif
if (!ssl_mutex_init(base_server, p)) {
return HTTP_INTERNAL_SERVER_ERROR;
}
#if defined(HAVE_OCSP_STAPLING)
ssl_stapling_certinfo_hash_init(p);
#endif
if ((rv = ssl_scache_init(base_server, p)) != APR_SUCCESS) {
return rv;
}
pphrases = apr_array_make(ptemp, 2, sizeof(char *));
ap_log_error(APLOG_MARK, APLOG_INFO, 0, base_server, APLOGNO(01887)
"Init: Initializing (virtual) servers for SSL");
for (s = base_server; s; s = s->next) {
sc = mySrvConfig(s);
if ((rv = ssl_init_ConfigureServer(s, p, ptemp, sc, pphrases))
!= APR_SUCCESS) {
return rv;
}
}
if (pphrases->nelts > 0) {
memset(pphrases->elts, 0, pphrases->elt_size * pphrases->nelts);
pphrases->nelts = 0;
ap_log_error(APLOG_MARK, APLOG_INFO, 0, s, APLOGNO(02560)
"Init: Wiped out the queried pass phrases from memory");
}
if ((rv = ssl_init_CheckServers(base_server, ptemp)) != APR_SUCCESS) {
return rv;
}
for (s = base_server; s; s = s->next) {
sc = mySrvConfig(s);
if (sc->enabled == SSL_ENABLED_TRUE || sc->enabled == SSL_ENABLED_OPTIONAL) {
if ((rv = ssl_run_init_server(s, p, 0, sc->server->ssl_ctx)) != APR_SUCCESS) {
return rv;
}
} else if (sc->proxy_enabled == SSL_ENABLED_TRUE) {
if ((rv = ssl_run_init_server(s, p, 1, sc->proxy->ssl_ctx)) != APR_SUCCESS) {
return rv;
}
}
}
ssl_add_version_components(p, base_server);
modssl_init_app_data2_idx();
init_dh_params();
#if !MODSSL_USE_OPENSSL_PRE_1_1_API
init_bio_methods();
#endif
return OK;
}
#if defined(HAVE_OPENSSL_ENGINE_H) && defined(HAVE_ENGINE_INIT)
apr_status_t ssl_init_Engine(server_rec *s, apr_pool_t *p) {
SSLModConfigRec *mc = myModConfig(s);
ENGINE *e;
if (mc->szCryptoDevice) {
if (!(e = ENGINE_by_id(mc->szCryptoDevice))) {
ap_log_error(APLOG_MARK, APLOG_EMERG, 0, s, APLOGNO(01888)
"Init: Failed to load Crypto Device API `%s'",
mc->szCryptoDevice);
ssl_log_ssl_error(SSLLOG_MARK, APLOG_EMERG, s);
return ssl_die(s);
}
#if defined(ENGINE_CTRL_CHIL_SET_FORKCHECK)
if (strEQ(mc->szCryptoDevice, "chil")) {
ENGINE_ctrl(e, ENGINE_CTRL_CHIL_SET_FORKCHECK, 1, 0, 0);
}
#endif
if (!ENGINE_set_default(e, ENGINE_METHOD_ALL)) {
ap_log_error(APLOG_MARK, APLOG_EMERG, 0, s, APLOGNO(01889)
"Init: Failed to enable Crypto Device API `%s'",
mc->szCryptoDevice);
ssl_log_ssl_error(SSLLOG_MARK, APLOG_EMERG, s);
return ssl_die(s);
}
ap_log_error(APLOG_MARK, APLOG_INFO, 0, s, APLOGNO(01890)
"Init: loaded Crypto Device API `%s'",
mc->szCryptoDevice);
ENGINE_free(e);
}
return APR_SUCCESS;
}
#endif
#if defined(HAVE_TLSEXT)
static apr_status_t ssl_init_ctx_tls_extensions(server_rec *s,
apr_pool_t *p,
apr_pool_t *ptemp,
modssl_ctx_t *mctx) {
apr_status_t rv;
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s, APLOGNO(01893)
"Configuring TLS extension handling");
if (!SSL_CTX_set_tlsext_servername_callback(mctx->ssl_ctx,
ssl_callback_ServerNameIndication) ||
!SSL_CTX_set_tlsext_servername_arg(mctx->ssl_ctx, mctx)) {
ap_log_error(APLOG_MARK, APLOG_EMERG, 0, s, APLOGNO(01894)
"Unable to initialize TLS servername extension "
"callback (incompatible OpenSSL version?)");
ssl_log_ssl_error(SSLLOG_MARK, APLOG_EMERG, s);
return ssl_die(s);
}
#if defined(HAVE_OCSP_STAPLING)
if ((mctx->pkp == FALSE) && (mctx->stapling_enabled == TRUE)) {
if ((rv = modssl_init_stapling(s, p, ptemp, mctx)) != APR_SUCCESS) {
return rv;
}
}
#endif
#if defined(HAVE_SRP)
if (mctx->srp_vfile != NULL) {
int err;
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s, APLOGNO(02308)
"Using SRP verifier file [%s]", mctx->srp_vfile);
if (!(mctx->srp_vbase = SRP_VBASE_new(mctx->srp_unknown_user_seed))) {
ap_log_error(APLOG_MARK, APLOG_EMERG, 0, s, APLOGNO(02309)
"Unable to initialize SRP verifier structure "
"[%s seed]",
mctx->srp_unknown_user_seed ? "with" : "without");
ssl_log_ssl_error(SSLLOG_MARK, APLOG_EMERG, s);
return ssl_die(s);
}
err = SRP_VBASE_init(mctx->srp_vbase, mctx->srp_vfile);
if (err != SRP_NO_ERROR) {
ap_log_error(APLOG_MARK, APLOG_EMERG, 0, s, APLOGNO(02310)
"Unable to load SRP verifier file [error %d]", err);
ssl_log_ssl_error(SSLLOG_MARK, APLOG_EMERG, s);
return ssl_die(s);
}
SSL_CTX_set_srp_username_callback(mctx->ssl_ctx,
ssl_callback_SRPServerParams);
SSL_CTX_set_srp_cb_arg(mctx->ssl_ctx, mctx);
}
#endif
return APR_SUCCESS;
}
#endif
static apr_status_t ssl_init_ctx_protocol(server_rec *s,
apr_pool_t *p,
apr_pool_t *ptemp,
modssl_ctx_t *mctx) {
SSL_CTX *ctx = NULL;
MODSSL_SSL_METHOD_CONST SSL_METHOD *method = NULL;
char *cp;
int protocol = mctx->protocol;
SSLSrvConfigRec *sc = mySrvConfig(s);
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
int prot;
#endif
if (protocol == SSL_PROTOCOL_NONE) {
ap_log_error(APLOG_MARK, APLOG_EMERG, 0, s, APLOGNO(02231)
"No SSL protocols available [hint: SSLProtocol]");
return ssl_die(s);
}
cp = apr_pstrcat(p,
#if !defined(OPENSSL_NO_SSL3)
(protocol & SSL_PROTOCOL_SSLV3 ? "SSLv3, " : ""),
#endif
(protocol & SSL_PROTOCOL_TLSV1 ? "TLSv1, " : ""),
#if defined(HAVE_TLSV1_X)
(protocol & SSL_PROTOCOL_TLSV1_1 ? "TLSv1.1, " : ""),
(protocol & SSL_PROTOCOL_TLSV1_2 ? "TLSv1.2, " : ""),
#endif
NULL);
cp[strlen(cp)-2] = NUL;
ap_log_error(APLOG_MARK, APLOG_TRACE3, 0, s,
"Creating new SSL context (protocols: %s)", cp);
#if OPENSSL_VERSION_NUMBER < 0x10100000L
#if !defined(OPENSSL_NO_SSL3)
if (protocol == SSL_PROTOCOL_SSLV3) {
method = mctx->pkp ?
SSLv3_client_method() :
SSLv3_server_method();
} else
#endif
if (protocol == SSL_PROTOCOL_TLSV1) {
method = mctx->pkp ?
TLSv1_client_method() :
TLSv1_server_method();
}
#if defined(HAVE_TLSV1_X)
else if (protocol == SSL_PROTOCOL_TLSV1_1) {
method = mctx->pkp ?
TLSv1_1_client_method() :
TLSv1_1_server_method();
} else if (protocol == SSL_PROTOCOL_TLSV1_2) {
method = mctx->pkp ?
TLSv1_2_client_method() :
TLSv1_2_server_method();
}
#endif
else {
method = mctx->pkp ?
SSLv23_client_method() :
SSLv23_server_method();
}
#else
method = mctx->pkp ?
TLS_client_method() :
TLS_server_method();
#endif
ctx = SSL_CTX_new(method);
mctx->ssl_ctx = ctx;
SSL_CTX_set_options(ctx, SSL_OP_ALL);
#if OPENSSL_VERSION_NUMBER < 0x10100000L
SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv2);
#if !defined(OPENSSL_NO_SSL3)
if (!(protocol & SSL_PROTOCOL_SSLV3)) {
SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv3);
}
#endif
if (!(protocol & SSL_PROTOCOL_TLSV1)) {
SSL_CTX_set_options(ctx, SSL_OP_NO_TLSv1);
}
#if defined(HAVE_TLSV1_X)
if (!(protocol & SSL_PROTOCOL_TLSV1_1)) {
SSL_CTX_set_options(ctx, SSL_OP_NO_TLSv1_1);
}
if (!(protocol & SSL_PROTOCOL_TLSV1_2)) {
SSL_CTX_set_options(ctx, SSL_OP_NO_TLSv1_2);
}
#endif
#else
if (protocol & SSL_PROTOCOL_TLSV1_2) {
prot = TLS1_2_VERSION;
} else if (protocol & SSL_PROTOCOL_TLSV1_1) {
prot = TLS1_1_VERSION;
} else if (protocol & SSL_PROTOCOL_TLSV1) {
prot = TLS1_VERSION;
#if !defined(OPENSSL_NO_SSL3)
} else if (protocol & SSL_PROTOCOL_SSLV3) {
prot = SSL3_VERSION;
#endif
} else {
SSL_CTX_free(ctx);
mctx->ssl_ctx = NULL;
ap_log_error(APLOG_MARK, APLOG_EMERG, 0, s, APLOGNO(03378)
"No SSL protocols available [hint: SSLProtocol]");
return ssl_die(s);
}
SSL_CTX_set_max_proto_version(ctx, prot);
if (prot == TLS1_2_VERSION && protocol & SSL_PROTOCOL_TLSV1_1) {
prot = TLS1_1_VERSION;
}
if (prot == TLS1_1_VERSION && protocol & SSL_PROTOCOL_TLSV1) {
prot = TLS1_VERSION;
}
#if !defined(OPENSSL_NO_SSL3)
if (prot == TLS1_VERSION && protocol & SSL_PROTOCOL_SSLV3) {
prot = SSL3_VERSION;
}
#endif
SSL_CTX_set_min_proto_version(ctx, prot);
#endif
#if defined(SSL_OP_CIPHER_SERVER_PREFERENCE)
if (sc->cipher_server_pref == TRUE) {
SSL_CTX_set_options(ctx, SSL_OP_CIPHER_SERVER_PREFERENCE);
}
#endif
#if !defined(OPENSSL_NO_COMP)
if (sc->compression != TRUE) {
#if defined(SSL_OP_NO_COMPRESSION)
SSL_CTX_set_options(ctx, SSL_OP_NO_COMPRESSION);
#else
sk_SSL_COMP_zero(SSL_COMP_get_compression_methods());
#endif
}
#endif
#if defined(SSL_OP_NO_TICKET)
if (sc->session_tickets == FALSE) {
SSL_CTX_set_options(ctx, SSL_OP_NO_TICKET);
}
#endif
#if defined(SSL_OP_ALLOW_UNSAFE_LEGACY_RENEGOTIATION)
if (sc->insecure_reneg == TRUE) {
SSL_CTX_set_options(ctx, SSL_OP_ALLOW_UNSAFE_LEGACY_RENEGOTIATION);
}
#endif
SSL_CTX_set_app_data(ctx, s);
SSL_CTX_set_options(ctx, SSL_OP_SINGLE_DH_USE);
#if defined(HAVE_ECC)
SSL_CTX_set_options(ctx, SSL_OP_SINGLE_ECDH_USE);
#endif
#if defined(SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION)
SSL_CTX_set_options(ctx, SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION);
#endif
#if defined(SSL_MODE_RELEASE_BUFFERS)
if (ap_max_mem_free != APR_ALLOCATOR_MAX_FREE_UNLIMITED)
SSL_CTX_set_mode(ctx, SSL_MODE_RELEASE_BUFFERS);
#endif
return APR_SUCCESS;
}
static void ssl_init_ctx_session_cache(server_rec *s,
apr_pool_t *p,
apr_pool_t *ptemp,
modssl_ctx_t *mctx) {
SSL_CTX *ctx = mctx->ssl_ctx;
SSLModConfigRec *mc = myModConfig(s);
SSL_CTX_set_session_cache_mode(ctx, mc->sesscache_mode);
if (mc->sesscache) {
SSL_CTX_sess_set_new_cb(ctx, ssl_callback_NewSessionCacheEntry);
SSL_CTX_sess_set_get_cb(ctx, ssl_callback_GetSessionCacheEntry);
SSL_CTX_sess_set_remove_cb(ctx, ssl_callback_DelSessionCacheEntry);
}
}
static void ssl_init_ctx_callbacks(server_rec *s,
apr_pool_t *p,
apr_pool_t *ptemp,
modssl_ctx_t *mctx) {
SSL_CTX *ctx = mctx->ssl_ctx;
SSL_CTX_set_tmp_dh_callback(ctx, ssl_callback_TmpDH);
SSL_CTX_set_info_callback(ctx, ssl_callback_Info);
#if defined(HAVE_TLS_ALPN)
SSL_CTX_set_alpn_select_cb(ctx, ssl_callback_alpn_select, NULL);
#endif
}
static apr_status_t ssl_init_ctx_verify(server_rec *s,
apr_pool_t *p,
apr_pool_t *ptemp,
modssl_ctx_t *mctx) {
SSL_CTX *ctx = mctx->ssl_ctx;
int verify = SSL_VERIFY_NONE;
STACK_OF(X509_NAME) *ca_list;
if (mctx->auth.verify_mode == SSL_CVERIFY_UNSET) {
mctx->auth.verify_mode = SSL_CVERIFY_NONE;
}
if (mctx->auth.verify_depth == UNSET) {
mctx->auth.verify_depth = 1;
}
if (mctx->auth.verify_mode == SSL_CVERIFY_REQUIRE) {
verify |= SSL_VERIFY_PEER_STRICT;
}
if ((mctx->auth.verify_mode == SSL_CVERIFY_OPTIONAL) ||
(mctx->auth.verify_mode == SSL_CVERIFY_OPTIONAL_NO_CA)) {
verify |= SSL_VERIFY_PEER;
}
SSL_CTX_set_verify(ctx, verify, ssl_callback_SSLVerify);
if (mctx->auth.ca_cert_file || mctx->auth.ca_cert_path) {
ap_log_error(APLOG_MARK, APLOG_TRACE1, 0, s,
"Configuring client authentication");
if (!SSL_CTX_load_verify_locations(ctx,
mctx->auth.ca_cert_file,
mctx->auth.ca_cert_path)) {
ap_log_error(APLOG_MARK, APLOG_EMERG, 0, s, APLOGNO(01895)
"Unable to configure verify locations "
"for client authentication");
ssl_log_ssl_error(SSLLOG_MARK, APLOG_EMERG, s);
return ssl_die(s);
}
if (mctx->pks && (mctx->pks->ca_name_file || mctx->pks->ca_name_path)) {
ca_list = ssl_init_FindCAList(s, ptemp,
mctx->pks->ca_name_file,
mctx->pks->ca_name_path);
} else
ca_list = ssl_init_FindCAList(s, ptemp,
mctx->auth.ca_cert_file,
mctx->auth.ca_cert_path);
if (sk_X509_NAME_num(ca_list) <= 0) {
ap_log_error(APLOG_MARK, APLOG_EMERG, 0, s, APLOGNO(01896)
"Unable to determine list of acceptable "
"CA certificates for client authentication");
return ssl_die(s);
}
SSL_CTX_set_client_CA_list(ctx, ca_list);
}
if (mctx->auth.verify_mode == SSL_CVERIFY_REQUIRE) {
ca_list = SSL_CTX_get_client_CA_list(ctx);
if (sk_X509_NAME_num(ca_list) == 0) {
ap_log_error(APLOG_MARK, APLOG_WARNING, 0, s, APLOGNO(01897)
"Init: Oops, you want to request client "
"authentication, but no CAs are known for "
"verification!? [Hint: SSLCACertificate*]");
}
}
return APR_SUCCESS;
}
static apr_status_t ssl_init_ctx_cipher_suite(server_rec *s,
apr_pool_t *p,
apr_pool_t *ptemp,
modssl_ctx_t *mctx) {
SSL_CTX *ctx = mctx->ssl_ctx;
const char *suite;
suite = mctx->auth.cipher_suite ? mctx->auth.cipher_suite :
apr_pstrcat(ptemp, SSL_DEFAULT_CIPHER_LIST, ":!aNULL:!eNULL:!EXP",
NULL);
ap_log_error(APLOG_MARK, APLOG_TRACE1, 0, s,
"Configuring permitted SSL ciphers [%s]",
suite);
if (!SSL_CTX_set_cipher_list(ctx, suite)) {
ap_log_error(APLOG_MARK, APLOG_EMERG, 0, s, APLOGNO(01898)
"Unable to configure permitted SSL ciphers");
ssl_log_ssl_error(SSLLOG_MARK, APLOG_EMERG, s);
return ssl_die(s);
}
return APR_SUCCESS;
}
static apr_status_t ssl_init_ctx_crl(server_rec *s,
apr_pool_t *p,
apr_pool_t *ptemp,
modssl_ctx_t *mctx) {
X509_STORE *store = SSL_CTX_get_cert_store(mctx->ssl_ctx);
unsigned long crlflags = 0;
char *cfgp = mctx->pkp ? "SSLProxy" : "SSL";
int crl_check_mode;
if (mctx->crl_check_mask == UNSET) {
mctx->crl_check_mask = SSL_CRLCHECK_NONE;
}
crl_check_mode = mctx->crl_check_mask & ~SSL_CRLCHECK_FLAGS;
if (!(mctx->crl_file || mctx->crl_path)) {
if (crl_check_mode == SSL_CRLCHECK_LEAF ||
crl_check_mode == SSL_CRLCHECK_CHAIN) {
ap_log_error(APLOG_MARK, APLOG_EMERG, 0, s, APLOGNO(01899)
"Host %s: CRL checking has been enabled, but "
"neither %sCARevocationFile nor %sCARevocationPath "
"is configured", mctx->sc->vhost_id, cfgp, cfgp);
return ssl_die(s);
}
return APR_SUCCESS;
}
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s, APLOGNO(01900)
"Configuring certificate revocation facility");
if (!store || !X509_STORE_load_locations(store, mctx->crl_file,
mctx->crl_path)) {
ap_log_error(APLOG_MARK, APLOG_EMERG, 0, s, APLOGNO(01901)
"Host %s: unable to configure X.509 CRL storage "
"for certificate revocation", mctx->sc->vhost_id);
ssl_log_ssl_error(SSLLOG_MARK, APLOG_EMERG, s);
return ssl_die(s);
}
switch (crl_check_mode) {
case SSL_CRLCHECK_LEAF:
crlflags = X509_V_FLAG_CRL_CHECK;
break;
case SSL_CRLCHECK_CHAIN:
crlflags = X509_V_FLAG_CRL_CHECK|X509_V_FLAG_CRL_CHECK_ALL;
break;
default:
crlflags = 0;
}
if (crlflags) {
X509_STORE_set_flags(store, crlflags);
} else {
ap_log_error(APLOG_MARK, APLOG_WARNING, 0, s, APLOGNO(01902)
"Host %s: X.509 CRL storage locations configured, "
"but CRL checking (%sCARevocationCheck) is not "
"enabled", mctx->sc->vhost_id, cfgp);
}
return APR_SUCCESS;
}
static int use_certificate_chain(
SSL_CTX *ctx, char *file, int skipfirst, pem_password_cb *cb) {
BIO *bio;
X509 *x509;
unsigned long err;
int n;
if ((bio = BIO_new(BIO_s_file())) == NULL)
return -1;
if (BIO_read_filename(bio, file) <= 0) {
BIO_free(bio);
return -1;
}
if (skipfirst) {
if ((x509 = PEM_read_bio_X509(bio, NULL, cb, NULL)) == NULL) {
BIO_free(bio);
return -1;
}
X509_free(x509);
}
#if defined(OPENSSL_NO_SSL_INTERN)
SSL_CTX_clear_extra_chain_certs(ctx);
#else
if (ctx->extra_certs != NULL) {
sk_X509_pop_free((STACK_OF(X509) *)ctx->extra_certs, X509_free);
ctx->extra_certs = NULL;
}
#endif
n = 0;
while ((x509 = PEM_read_bio_X509(bio, NULL, cb, NULL)) != NULL) {
if (!SSL_CTX_add_extra_chain_cert(ctx, x509)) {
X509_free(x509);
BIO_free(bio);
return -1;
}
n++;
}
if ((err = ERR_peek_error()) > 0) {
if (!( ERR_GET_LIB(err) == ERR_LIB_PEM
&& ERR_GET_REASON(err) == PEM_R_NO_START_LINE)) {
BIO_free(bio);
return -1;
}
while (ERR_get_error() > 0) ;
}
BIO_free(bio);
return n;
}
static apr_status_t ssl_init_ctx_cert_chain(server_rec *s,
apr_pool_t *p,
apr_pool_t *ptemp,
modssl_ctx_t *mctx) {
BOOL skip_first = FALSE;
int i, n;
const char *chain = mctx->cert_chain;
if (!chain) {
return APR_SUCCESS;
}
for (i = 0; (i < mctx->pks->cert_files->nelts) &&
APR_ARRAY_IDX(mctx->pks->cert_files, i, const char *); i++) {
if (strEQ(APR_ARRAY_IDX(mctx->pks->cert_files, i, const char *), chain)) {
skip_first = TRUE;
break;
}
}
n = use_certificate_chain(mctx->ssl_ctx, (char *)chain, skip_first, NULL);
if (n < 0) {
ap_log_error(APLOG_MARK, APLOG_EMERG, 0, s, APLOGNO(01903)
"Failed to configure CA certificate chain!");
return ssl_die(s);
}
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s, APLOGNO(01904)
"Configuring server certificate chain "
"(%d CA certificate%s)",
n, n == 1 ? "" : "s");
return APR_SUCCESS;
}
static apr_status_t ssl_init_ctx(server_rec *s,
apr_pool_t *p,
apr_pool_t *ptemp,
modssl_ctx_t *mctx) {
apr_status_t rv;
if ((rv = ssl_init_ctx_protocol(s, p, ptemp, mctx)) != APR_SUCCESS) {
return rv;
}
ssl_init_ctx_session_cache(s, p, ptemp, mctx);
ssl_init_ctx_callbacks(s, p, ptemp, mctx);
if ((rv = ssl_init_ctx_verify(s, p, ptemp, mctx)) != APR_SUCCESS) {
return rv;
}
if ((rv = ssl_init_ctx_cipher_suite(s, p, ptemp, mctx)) != APR_SUCCESS) {
return rv;
}
if ((rv = ssl_init_ctx_crl(s, p, ptemp, mctx)) != APR_SUCCESS) {
return rv;
}
if (mctx->pks) {
if ((rv = ssl_init_ctx_cert_chain(s, p, ptemp, mctx)) != APR_SUCCESS) {
return rv;
}
#if defined(HAVE_TLSEXT)
if ((rv = ssl_init_ctx_tls_extensions(s, p, ptemp, mctx)) !=
APR_SUCCESS) {
return rv;
}
#endif
}
return APR_SUCCESS;
}
static void ssl_check_public_cert(server_rec *s,
apr_pool_t *ptemp,
X509 *cert,
const char *key_id) {
int is_ca, pathlen;
if (!cert) {
return;
}
if (modssl_X509_getBC(cert, &is_ca, &pathlen)) {
if (is_ca) {
ap_log_error(APLOG_MARK, APLOG_WARNING, 0, s, APLOGNO(01906)
"%s server certificate is a CA certificate "
"(BasicConstraints: CA == TRUE !?)", key_id);
}
if (pathlen > 0) {
ap_log_error(APLOG_MARK, APLOG_WARNING, 0, s, APLOGNO(01907)
"%s server certificate is not a leaf certificate "
"(BasicConstraints: pathlen == %d > 0 !?)",
key_id, pathlen);
}
}
if (modssl_X509_match_name(ptemp, cert, (const char *)s->server_hostname,
TRUE, s) == FALSE) {
ap_log_error(APLOG_MARK, APLOG_WARNING, 0, s, APLOGNO(01909)
"%s server certificate does NOT include an ID "
"which matches the server name", key_id);
}
}
static int ssl_no_passwd_prompt_cb(char *buf, int size, int rwflag,
void *userdata) {
return 0;
}
static apr_status_t ssl_init_server_certs(server_rec *s,
apr_pool_t *p,
apr_pool_t *ptemp,
modssl_ctx_t *mctx,
apr_array_header_t *pphrases) {
SSLModConfigRec *mc = myModConfig(s);
const char *vhost_id = mctx->sc->vhost_id, *key_id, *certfile, *keyfile;
int i;
X509 *cert;
DH *dhparams;
#if defined(HAVE_ECC)
EC_GROUP *ecparams = NULL;
int nid;
EC_KEY *eckey = NULL;
#endif
#if !defined(HAVE_SSL_CONF_CMD)
SSL *ssl;
#endif
SSL_CTX_set_default_passwd_cb(mctx->ssl_ctx, ssl_no_passwd_prompt_cb);
for (i = 0; (i < mctx->pks->cert_files->nelts) &&
(certfile = APR_ARRAY_IDX(mctx->pks->cert_files, i,
const char *));
i++) {
key_id = apr_psprintf(ptemp, "%s:%d", vhost_id, i);
ERR_clear_error();
if (mctx->cert_chain) {
if ((SSL_CTX_use_certificate_file(mctx->ssl_ctx, certfile,
SSL_FILETYPE_PEM) < 1)) {
ap_log_error(APLOG_MARK, APLOG_EMERG, 0, s, APLOGNO(02561)
"Failed to configure certificate %s, check %s",
key_id, certfile);
ssl_log_ssl_error(SSLLOG_MARK, APLOG_EMERG, s);
return APR_EGENERAL;
}
} else {
if ((SSL_CTX_use_certificate_chain_file(mctx->ssl_ctx,
certfile) < 1)) {
ap_log_error(APLOG_MARK, APLOG_EMERG, 0, s, APLOGNO(02562)
"Failed to configure certificate %s (with chain),"
" check %s", key_id, certfile);
ssl_log_ssl_error(SSLLOG_MARK, APLOG_EMERG, s);
return APR_EGENERAL;
}
}
if (i < mctx->pks->key_files->nelts) {
keyfile = APR_ARRAY_IDX(mctx->pks->key_files, i, const char *);
} else {
keyfile = certfile;
}
ERR_clear_error();
if ((SSL_CTX_use_PrivateKey_file(mctx->ssl_ctx, keyfile,
SSL_FILETYPE_PEM) < 1) &&
(ERR_GET_FUNC(ERR_peek_last_error())
!= X509_F_X509_CHECK_PRIVATE_KEY)) {
ssl_asn1_t *asn1;
EVP_PKEY *pkey;
const unsigned char *ptr;
ERR_clear_error();
ssl_load_encrypted_pkey(s, ptemp, i, keyfile, &pphrases);
if (!(asn1 = ssl_asn1_table_get(mc->tPrivateKey, key_id)) ||
!(ptr = asn1->cpData) ||
!(pkey = d2i_AutoPrivateKey(NULL, &ptr, asn1->nData)) ||
(SSL_CTX_use_PrivateKey(mctx->ssl_ctx, pkey) < 1)) {
ap_log_error(APLOG_MARK, APLOG_EMERG, 0, s, APLOGNO(02564)
"Failed to configure encrypted (?) private key %s,"
" check %s", key_id, keyfile);
ssl_log_ssl_error(SSLLOG_MARK, APLOG_EMERG, s);
return APR_EGENERAL;
}
}
if (SSL_CTX_check_private_key(mctx->ssl_ctx) < 1) {
ap_log_error(APLOG_MARK, APLOG_EMERG, 0, s, APLOGNO(02565)
"Certificate and private key %s from %s and %s "
"do not match", key_id, certfile, keyfile);
return APR_EGENERAL;
}
#if defined(HAVE_SSL_CONF_CMD)
if (!(cert = SSL_CTX_get0_certificate(mctx->ssl_ctx))) {
#else
ssl = SSL_new(mctx->ssl_ctx);
if (ssl) {
SSL_set_connect_state(ssl);
cert = SSL_get_certificate(ssl);
}
if (!ssl || !cert) {
#endif
ap_log_error(APLOG_MARK, APLOG_ERR, 0, s, APLOGNO(02566)
"Unable to retrieve certificate %s", key_id);
#if !defined(HAVE_SSL_CONF_CMD)
if (ssl)
SSL_free(ssl);
#endif
return APR_EGENERAL;
}
ssl_check_public_cert(s, ptemp, cert, key_id);
#if defined(HAVE_OCSP_STAPLING) && !defined(SSL_CTRL_SET_CURRENT_CERT)
if ((mctx->stapling_enabled == TRUE) &&
!ssl_stapling_init_cert(s, p, ptemp, mctx, cert)) {
ap_log_error(APLOG_MARK, APLOG_ERR, 0, s, APLOGNO(02567)
"Unable to configure certificate %s for stapling",
key_id);
}
#endif
#if !defined(HAVE_SSL_CONF_CMD)
SSL_free(ssl);
#endif
ap_log_error(APLOG_MARK, APLOG_INFO, 0, s, APLOGNO(02568)
"Certificate and private key %s configured from %s and %s",
key_id, certfile, keyfile);
}
if ((certfile = APR_ARRAY_IDX(mctx->pks->cert_files, 0, const char *)) &&
(dhparams = ssl_dh_GetParamFromFile(certfile))) {
SSL_CTX_set_tmp_dh(mctx->ssl_ctx, dhparams);
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s, APLOGNO(02540)
"Custom DH parameters (%d bits) for %s loaded from %s",
DH_bits(dhparams), vhost_id, certfile);
DH_free(dhparams);
}
#if defined(HAVE_ECC)
if ((certfile != NULL) &&
(ecparams = ssl_ec_GetParamFromFile(certfile)) &&
(nid = EC_GROUP_get_curve_name(ecparams)) &&
(eckey = EC_KEY_new_by_curve_name(nid))) {
SSL_CTX_set_tmp_ecdh(mctx->ssl_ctx, eckey);
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s, APLOGNO(02541)
"ECDH curve %s for %s specified in %s",
OBJ_nid2sn(nid), vhost_id, certfile);
}
#if MODSSL_USE_OPENSSL_PRE_1_1_API
else {
#if defined(SSL_CTX_set_ecdh_auto)
SSL_CTX_set_ecdh_auto(mctx->ssl_ctx, 1);
#else
SSL_CTX_set_tmp_ecdh(mctx->ssl_ctx,
EC_KEY_new_by_curve_name(NID_X9_62_prime256v1));
#endif
}
#endif
EC_KEY_free(eckey);
EC_GROUP_free(ecparams);
#endif
return APR_SUCCESS;
}
#if defined(HAVE_TLS_SESSION_TICKETS)
static apr_status_t ssl_init_ticket_key(server_rec *s,
apr_pool_t *p,
apr_pool_t *ptemp,
modssl_ctx_t *mctx) {
apr_status_t rv;
apr_file_t *fp;
apr_size_t len;
char buf[TLSEXT_TICKET_KEY_LEN];
char *path;
modssl_ticket_key_t *ticket_key = mctx->ticket_key;
if (!ticket_key->file_path) {
return APR_SUCCESS;
}
path = ap_server_root_relative(p, ticket_key->file_path);
rv = apr_file_open(&fp, path, APR_READ|APR_BINARY,
APR_OS_DEFAULT, ptemp);
if (rv != APR_SUCCESS) {
ap_log_error(APLOG_MARK, APLOG_EMERG, 0, s, APLOGNO(02286)
"Failed to open ticket key file %s: (%d) %pm",
path, rv, &rv);
return ssl_die(s);
}
rv = apr_file_read_full(fp, &buf[0], TLSEXT_TICKET_KEY_LEN, &len);
if (rv != APR_SUCCESS) {
ap_log_error(APLOG_MARK, APLOG_EMERG, 0, s, APLOGNO(02287)
"Failed to read %d bytes from %s: (%d) %pm",
TLSEXT_TICKET_KEY_LEN, path, rv, &rv);
return ssl_die(s);
}
memcpy(ticket_key->key_name, buf, 16);
memcpy(ticket_key->hmac_secret, buf + 16, 16);
memcpy(ticket_key->aes_key, buf + 32, 16);
if (!SSL_CTX_set_tlsext_ticket_key_cb(mctx->ssl_ctx,
ssl_callback_SessionTicket)) {
ap_log_error(APLOG_MARK, APLOG_EMERG, 0, s, APLOGNO(01913)
"Unable to initialize TLS session ticket key callback "
"(incompatible OpenSSL version?)");
ssl_log_ssl_error(SSLLOG_MARK, APLOG_EMERG, s);
return ssl_die(s);
}
ap_log_error(APLOG_MARK, APLOG_INFO, 0, s, APLOGNO(02288)
"TLS session ticket key for %s successfully loaded from %s",
(mySrvConfig(s))->vhost_id, path);
return APR_SUCCESS;
}
#endif
static BOOL load_x509_info(apr_pool_t *ptemp,
STACK_OF(X509_INFO) *sk,
const char *filename) {
BIO *in;
if (!(in = BIO_new(BIO_s_file()))) {
return FALSE;
}
if (BIO_read_filename(in, filename) <= 0) {
BIO_free(in);
return FALSE;
}
ERR_clear_error();
PEM_X509_INFO_read_bio(in, sk, NULL, NULL);
BIO_free(in);
return TRUE;
}
static apr_status_t ssl_init_proxy_certs(server_rec *s,
apr_pool_t *p,
apr_pool_t *ptemp,
modssl_ctx_t *mctx) {
int n, ncerts = 0;
STACK_OF(X509_INFO) *sk;
modssl_pk_proxy_t *pkp = mctx->pkp;
STACK_OF(X509) *chain;
X509_STORE_CTX *sctx;
X509_STORE *store = SSL_CTX_get_cert_store(mctx->ssl_ctx);
SSL_CTX_set_client_cert_cb(mctx->ssl_ctx,
ssl_callback_proxy_cert);
if (!(pkp->cert_file || pkp->cert_path)) {
return APR_SUCCESS;
}
sk = sk_X509_INFO_new_null();
if (pkp->cert_file) {
load_x509_info(ptemp, sk, pkp->cert_file);
}
if (pkp->cert_path) {
apr_dir_t *dir;
apr_finfo_t dirent;
apr_int32_t finfo_flags = APR_FINFO_TYPE|APR_FINFO_NAME;
if (apr_dir_open(&dir, pkp->cert_path, ptemp) == APR_SUCCESS) {
while ((apr_dir_read(&dirent, finfo_flags, dir)) == APR_SUCCESS) {
const char *fullname;
if (dirent.filetype == APR_DIR) {
continue;
}
fullname = apr_pstrcat(ptemp,
pkp->cert_path, "/", dirent.name,
NULL);
load_x509_info(ptemp, sk, fullname);
}
apr_dir_close(dir);
}
}
if ((ncerts = sk_X509_INFO_num(sk)) <= 0) {
sk_X509_INFO_free(sk);
ap_log_error(APLOG_MARK, APLOG_WARNING, 0, s, APLOGNO(02206)
"no client certs found for SSL proxy");
return APR_SUCCESS;
}
for (n = 0; n < ncerts; n++) {
X509_INFO *inf = sk_X509_INFO_value(sk, n);
if (!inf->x509 || !inf->x_pkey || !inf->x_pkey->dec_pkey ||
inf->enc_data) {
sk_X509_INFO_free(sk);
ap_log_error(APLOG_MARK, APLOG_STARTUP, 0, s, APLOGNO(02252)
"incomplete client cert configured for SSL proxy "
"(missing or encrypted private key?)");
return ssl_die(s);
}
if (X509_check_private_key(inf->x509, inf->x_pkey->dec_pkey) != 1) {
ssl_log_xerror(SSLLOG_MARK, APLOG_STARTUP, 0, ptemp, s, inf->x509,
APLOGNO(02326) "proxy client certificate and "
"private key do not match");
ssl_log_ssl_error(SSLLOG_MARK, APLOG_ERR, s);
return ssl_die(s);
}
}
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s, APLOGNO(02207)
"loaded %d client certs for SSL proxy",
ncerts);
pkp->certs = sk;
if (!pkp->ca_cert_file || !store) {
return APR_SUCCESS;
}
pkp->ca_certs = (STACK_OF(X509) **) apr_pcalloc(p, ncerts * sizeof(sk));
sctx = X509_STORE_CTX_new();
if (!sctx) {
ap_log_error(APLOG_MARK, APLOG_EMERG, 0, s, APLOGNO(02208)
"SSL proxy client cert initialization failed");
ssl_log_ssl_error(SSLLOG_MARK, APLOG_EMERG, s);
return ssl_die(s);
}
X509_STORE_load_locations(store, pkp->ca_cert_file, NULL);
for (n = 0; n < ncerts; n++) {
int i;
X509_INFO *inf = sk_X509_INFO_value(pkp->certs, n);
X509_STORE_CTX_init(sctx, store, inf->x509, NULL);
if (X509_verify_cert(sctx) != 1) {
int err = X509_STORE_CTX_get_error(sctx);
ssl_log_xerror(SSLLOG_MARK, APLOG_WARNING, 0, ptemp, s, inf->x509,
APLOGNO(02270) "SSL proxy client cert chain "
"verification failed: %s :",
X509_verify_cert_error_string(err));
}
ERR_clear_error();
chain = X509_STORE_CTX_get1_chain(sctx);
if (chain != NULL) {
X509_free(sk_X509_shift(chain));
if ((i = sk_X509_num(chain)) > 0) {
pkp->ca_certs[n] = chain;
} else {
sk_X509_pop_free(chain, X509_free);
pkp->ca_certs[n] = NULL;
}
ssl_log_xerror(SSLLOG_MARK, APLOG_DEBUG, 0, ptemp, s, inf->x509,
APLOGNO(02271)
"loaded %i intermediate CA%s for cert %i: ",
i, i == 1 ? "" : "s", n);
if (i > 0) {
int j;
for (j = 0; j < i; j++) {
ssl_log_xerror(SSLLOG_MARK, APLOG_DEBUG, 0, ptemp, s,
sk_X509_value(chain, j), APLOGNO(03039)
"%i:", j);
}
}
}
X509_STORE_CTX_cleanup(sctx);
}
X509_STORE_CTX_free(sctx);
return APR_SUCCESS;
}
static apr_status_t ssl_init_proxy_ctx(server_rec *s,
apr_pool_t *p,
apr_pool_t *ptemp,
SSLSrvConfigRec *sc) {
apr_status_t rv;
if ((rv = ssl_init_ctx(s, p, ptemp, sc->proxy)) != APR_SUCCESS) {
return rv;
}
if ((rv = ssl_init_proxy_certs(s, p, ptemp, sc->proxy)) != APR_SUCCESS) {
return rv;
}
return APR_SUCCESS;
}
static apr_status_t ssl_init_server_ctx(server_rec *s,
apr_pool_t *p,
apr_pool_t *ptemp,
SSLSrvConfigRec *sc,
apr_array_header_t *pphrases) {
apr_status_t rv;
#if defined(HAVE_SSL_CONF_CMD)
ssl_ctx_param_t *param = (ssl_ctx_param_t *)sc->server->ssl_ctx_param->elts;
SSL_CONF_CTX *cctx = sc->server->ssl_ctx_config;
int i;
#endif
if (sc->server->ssl_ctx) {
ap_log_error(APLOG_MARK, APLOG_EMERG, 0, s, APLOGNO(02569)
"Illegal attempt to re-initialise SSL for server "
"(SSLEngine On should go in the VirtualHost, not in global scope.)");
return APR_EGENERAL;
}
if ((rv = ssl_init_ctx(s, p, ptemp, sc->server)) != APR_SUCCESS) {
return rv;
}
if ((rv = ssl_init_server_certs(s, p, ptemp, sc->server, pphrases))
!= APR_SUCCESS) {
return rv;
}
#if defined(HAVE_SSL_CONF_CMD)
SSL_CONF_CTX_set_ssl_ctx(cctx, sc->server->ssl_ctx);
for (i = 0; i < sc->server->ssl_ctx_param->nelts; i++, param++) {
ERR_clear_error();
if (SSL_CONF_cmd(cctx, param->name, param->value) <= 0) {
ap_log_error(APLOG_MARK, APLOG_EMERG, 0, s, APLOGNO(02407)
"\"SSLOpenSSLConfCmd %s %s\" failed for %s",
param->name, param->value, sc->vhost_id);
ssl_log_ssl_error(SSLLOG_MARK, APLOG_EMERG, s);
return ssl_die(s);
} else {
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s, APLOGNO(02556)
"\"SSLOpenSSLConfCmd %s %s\" applied to %s",
param->name, param->value, sc->vhost_id);
}
}
if (SSL_CONF_CTX_finish(cctx) == 0) {
ap_log_error(APLOG_MARK, APLOG_EMERG, 0, s, APLOGNO(02547)
"SSL_CONF_CTX_finish() failed");
SSL_CONF_CTX_free(cctx);
ssl_log_ssl_error(SSLLOG_MARK, APLOG_EMERG, s);
return ssl_die(s);
}
#endif
if (SSL_CTX_check_private_key(sc->server->ssl_ctx) != 1) {
ap_log_error(APLOG_MARK, APLOG_EMERG, 0, s, APLOGNO(02572)
"Failed to configure at least one certificate and key "
"for %s", sc->vhost_id);
ssl_log_ssl_error(SSLLOG_MARK, APLOG_EMERG, s);
return ssl_die(s);
}
#if defined(HAVE_OCSP_STAPLING) && defined(SSL_CTRL_SET_CURRENT_CERT)
if (sc->server->stapling_enabled == TRUE) {
X509 *cert;
int i = 0;
int ret = SSL_CTX_set_current_cert(sc->server->ssl_ctx,
SSL_CERT_SET_FIRST);
while (ret) {
cert = SSL_CTX_get0_certificate(sc->server->ssl_ctx);
if (!cert || !ssl_stapling_init_cert(s, p, ptemp, sc->server,
cert)) {
ap_log_error(APLOG_MARK, APLOG_ERR, 0, s, APLOGNO(02604)
"Unable to configure certificate %s:%d "
"for stapling", sc->vhost_id, i);
}
ret = SSL_CTX_set_current_cert(sc->server->ssl_ctx,
SSL_CERT_SET_NEXT);
i++;
}
}
#endif
#if defined(HAVE_TLS_SESSION_TICKETS)
if ((rv = ssl_init_ticket_key(s, p, ptemp, sc->server)) != APR_SUCCESS) {
return rv;
}
#endif
SSL_CTX_set_timeout(sc->server->ssl_ctx,
sc->session_cache_timeout == UNSET ?
SSL_SESSION_CACHE_TIMEOUT : sc->session_cache_timeout);
return APR_SUCCESS;
}
apr_status_t ssl_init_ConfigureServer(server_rec *s,
apr_pool_t *p,
apr_pool_t *ptemp,
SSLSrvConfigRec *sc,
apr_array_header_t *pphrases) {
apr_status_t rv;
if ((sc->enabled == SSL_ENABLED_TRUE) || (sc->enabled == SSL_ENABLED_OPTIONAL)) {
ap_log_error(APLOG_MARK, APLOG_INFO, 0, s, APLOGNO(01914)
"Configuring server %s for SSL protocol", sc->vhost_id);
if ((rv = ssl_init_server_ctx(s, p, ptemp, sc, pphrases))
!= APR_SUCCESS) {
return rv;
}
#if !defined(OPENSSL_NO_OCSP)
ssl_init_ocsp_certificates(s, sc->server);
#endif
}
if (sc->proxy_enabled) {
if ((rv = ssl_init_proxy_ctx(s, p, ptemp, sc)) != APR_SUCCESS) {
return rv;
}
}
return APR_SUCCESS;
}
apr_status_t ssl_init_CheckServers(server_rec *base_server, apr_pool_t *p) {
server_rec *s;
SSLSrvConfigRec *sc;
#if !defined(HAVE_TLSEXT)
server_rec *ps;
apr_hash_t *table;
const char *key;
apr_ssize_t klen;
BOOL conflict = FALSE;
#endif
for (s = base_server; s; s = s->next) {
sc = mySrvConfig(s);
if ((sc->enabled == SSL_ENABLED_TRUE) && (s->port == DEFAULT_HTTP_PORT)) {
ap_log_error(APLOG_MARK, APLOG_WARNING, 0,
base_server, APLOGNO(01915)
"Init: (%s) You configured HTTPS(%d) "
"on the standard HTTP(%d) port!",
ssl_util_vhostid(p, s),
DEFAULT_HTTPS_PORT, DEFAULT_HTTP_PORT);
}
if ((sc->enabled == SSL_ENABLED_FALSE) && (s->port == DEFAULT_HTTPS_PORT)) {
ap_log_error(APLOG_MARK, APLOG_WARNING, 0,
base_server, APLOGNO(01916)
"Init: (%s) You configured HTTP(%d) "
"on the standard HTTPS(%d) port!",
ssl_util_vhostid(p, s),
DEFAULT_HTTP_PORT, DEFAULT_HTTPS_PORT);
}
}
#if !defined(HAVE_TLSEXT)
table = apr_hash_make(p);
for (s = base_server; s; s = s->next) {
char *addr;
sc = mySrvConfig(s);
if (!((sc->enabled == SSL_ENABLED_TRUE) && s->addrs)) {
continue;
}
apr_sockaddr_ip_get(&addr, s->addrs->host_addr);
key = apr_psprintf(p, "%s:%u", addr, s->addrs->host_port);
klen = strlen(key);
if ((ps = (server_rec *)apr_hash_get(table, key, klen))) {
ap_log_error(APLOG_MARK, APLOG_WARNING, 0, base_server, APLOGNO(02662)
"Init: SSL server IP/port conflict: "
"%s (%s:%d) vs. %s (%s:%d)",
ssl_util_vhostid(p, s),
(s->defn_name ? s->defn_name : "unknown"),
s->defn_line_number,
ssl_util_vhostid(p, ps),
(ps->defn_name ? ps->defn_name : "unknown"),
ps->defn_line_number);
conflict = TRUE;
continue;
}
apr_hash_set(table, key, klen, s);
}
if (conflict) {
ap_log_error(APLOG_MARK, APLOG_WARNING, 0, base_server, APLOGNO(01917)
"Init: Name-based SSL virtual hosts require "
"an OpenSSL version with support for TLS extensions "
"(RFC 6066 - Server Name Indication / SNI), "
"but the currently used library version (%s) is "
"lacking this feature", MODSSL_LIBRARY_DYNTEXT);
}
#endif
return APR_SUCCESS;
}
static int ssl_init_FindCAList_X509NameCmp(const X509_NAME * const *a,
const X509_NAME * const *b) {
return(X509_NAME_cmp(*a, *b));
}
static void ssl_init_PushCAList(STACK_OF(X509_NAME) *ca_list,
server_rec *s, apr_pool_t *ptemp,
const char *file) {
int n;
STACK_OF(X509_NAME) *sk;
sk = (STACK_OF(X509_NAME) *)
SSL_load_client_CA_file(file);
if (!sk) {
return;
}
for (n = 0; n < sk_X509_NAME_num(sk); n++) {
X509_NAME *name = sk_X509_NAME_value(sk, n);
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s, APLOGNO(02209)
"CA certificate: %s",
modssl_X509_NAME_to_string(ptemp, name, 0));
if (sk_X509_NAME_find(ca_list, name) < 0) {
sk_X509_NAME_push(ca_list, name);
} else {
X509_NAME_free(name);
}
}
sk_X509_NAME_free(sk);
}
STACK_OF(X509_NAME) *ssl_init_FindCAList(server_rec *s,
apr_pool_t *ptemp,
const char *ca_file,
const char *ca_path) {
STACK_OF(X509_NAME) *ca_list;
ca_list = sk_X509_NAME_new(ssl_init_FindCAList_X509NameCmp);
if (ca_file) {
ssl_init_PushCAList(ca_list, s, ptemp, ca_file);
if (sk_X509_NAME_num(ca_list) == 0) {
ap_log_error(APLOG_MARK, APLOG_ERR, 0, s, APLOGNO(02210)
"Failed to load SSLCACertificateFile: %s", ca_file);
ssl_log_ssl_error(SSLLOG_MARK, APLOG_ERR, s);
}
}
if (ca_path) {
apr_dir_t *dir;
apr_finfo_t direntry;
apr_int32_t finfo_flags = APR_FINFO_TYPE|APR_FINFO_NAME;
apr_status_t rv;
if ((rv = apr_dir_open(&dir, ca_path, ptemp)) != APR_SUCCESS) {
ap_log_error(APLOG_MARK, APLOG_EMERG, rv, s, APLOGNO(02211)
"Failed to open Certificate Path `%s'",
ca_path);
sk_X509_NAME_pop_free(ca_list, X509_NAME_free);
return NULL;
}
while ((apr_dir_read(&direntry, finfo_flags, dir)) == APR_SUCCESS) {
const char *file;
if (direntry.filetype == APR_DIR) {
continue;
}
file = apr_pstrcat(ptemp, ca_path, "/", direntry.name, NULL);
ssl_init_PushCAList(ca_list, s, ptemp, file);
}
apr_dir_close(dir);
}
(void) sk_X509_NAME_set_cmp_func(ca_list, NULL);
return ca_list;
}
void ssl_init_Child(apr_pool_t *p, server_rec *s) {
SSLModConfigRec *mc = myModConfig(s);
mc->pid = getpid();
srand((unsigned int)time(NULL));
ssl_mutex_reinit(s, p);
#if defined(HAVE_OCSP_STAPLING)
ssl_stapling_mutex_reinit(s, p);
#endif
}
#define MODSSL_CFG_ITEM_FREE(func, item) if (item) { func(item); item = NULL; }
static void ssl_init_ctx_cleanup(modssl_ctx_t *mctx) {
MODSSL_CFG_ITEM_FREE(SSL_CTX_free, mctx->ssl_ctx);
#if defined(HAVE_SRP)
if (mctx->srp_vbase != NULL) {
SRP_VBASE_free(mctx->srp_vbase);
mctx->srp_vbase = NULL;
}
#endif
}
static void ssl_init_ctx_cleanup_proxy(modssl_ctx_t *mctx) {
ssl_init_ctx_cleanup(mctx);
if (mctx->pkp->certs) {
int i = 0;
int ncerts = sk_X509_INFO_num(mctx->pkp->certs);
if (mctx->pkp->ca_certs) {
for (i = 0; i < ncerts; i++) {
if (mctx->pkp->ca_certs[i] != NULL) {
sk_X509_pop_free(mctx->pkp->ca_certs[i], X509_free);
}
}
}
sk_X509_INFO_pop_free(mctx->pkp->certs, X509_INFO_free);
mctx->pkp->certs = NULL;
}
}
apr_status_t ssl_init_ModuleKill(void *data) {
SSLSrvConfigRec *sc;
server_rec *base_server = (server_rec *)data;
server_rec *s;
ssl_scache_kill(base_server);
for (s = base_server; s; s = s->next) {
sc = mySrvConfig(s);
ssl_init_ctx_cleanup_proxy(sc->proxy);
ssl_init_ctx_cleanup(sc->server);
#if !defined(OPENSSL_NO_OCSP)
sk_X509_pop_free(sc->server->ocsp_certs, X509_free);
#endif
}
#if !MODSSL_USE_OPENSSL_PRE_1_1_API
free_bio_methods();
#endif
free_dh_params();
return APR_SUCCESS;
}