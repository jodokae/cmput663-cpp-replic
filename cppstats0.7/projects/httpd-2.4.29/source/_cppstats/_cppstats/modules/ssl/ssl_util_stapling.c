#include "ssl_private.h"
#include "ap_mpm.h"
#include "apr_thread_mutex.h"
#if defined(HAVE_OCSP_STAPLING)
static int stapling_cache_mutex_on(server_rec *s);
static int stapling_cache_mutex_off(server_rec *s);
#define MAX_STAPLING_DER 10240
typedef struct {
UCHAR idx[SHA_DIGEST_LENGTH];
OCSP_CERTID *cid;
char *uri;
} certinfo;
static apr_status_t ssl_stapling_certid_free(void *data) {
OCSP_CERTID *cid = data;
if (cid) {
OCSP_CERTID_free(cid);
}
return APR_SUCCESS;
}
static apr_hash_t *stapling_certinfo;
void ssl_stapling_certinfo_hash_init(apr_pool_t *p) {
stapling_certinfo = apr_hash_make(p);
}
static X509 *stapling_get_issuer(modssl_ctx_t *mctx, X509 *x) {
X509 *issuer = NULL;
int i;
X509_STORE *st = SSL_CTX_get_cert_store(mctx->ssl_ctx);
X509_STORE_CTX *inctx;
STACK_OF(X509) *extra_certs = NULL;
#if defined(OPENSSL_NO_SSL_INTERN)
SSL_CTX_get_extra_chain_certs(mctx->ssl_ctx, &extra_certs);
#else
extra_certs = mctx->ssl_ctx->extra_certs;
#endif
for (i = 0; i < sk_X509_num(extra_certs); i++) {
issuer = sk_X509_value(extra_certs, i);
if (X509_check_issued(issuer, x) == X509_V_OK) {
#if OPENSSL_VERSION_NUMBER < 0x10100000L
CRYPTO_add(&issuer->references, 1, CRYPTO_LOCK_X509);
#else
X509_up_ref(issuer);
#endif
return issuer;
}
}
inctx = X509_STORE_CTX_new();
if (!X509_STORE_CTX_init(inctx, st, NULL, NULL))
return 0;
if (X509_STORE_CTX_get1_issuer(&issuer, inctx, x) <= 0)
issuer = NULL;
X509_STORE_CTX_cleanup(inctx);
X509_STORE_CTX_free(inctx);
return issuer;
}
int ssl_stapling_init_cert(server_rec *s, apr_pool_t *p, apr_pool_t *ptemp,
modssl_ctx_t *mctx, X509 *x) {
UCHAR idx[SHA_DIGEST_LENGTH];
certinfo *cinf = NULL;
X509 *issuer = NULL;
OCSP_CERTID *cid = NULL;
STACK_OF(OPENSSL_STRING) *aia = NULL;
if ((x == NULL) || (X509_digest(x, EVP_sha1(), idx, NULL) != 1))
return 0;
cinf = apr_hash_get(stapling_certinfo, idx, sizeof(idx));
if (cinf) {
if (!cinf->uri && !mctx->stapling_force_url) {
ssl_log_xerror(SSLLOG_MARK, APLOG_ERR, 0, ptemp, s, x,
APLOGNO(02814) "ssl_stapling_init_cert: no OCSP URI "
"in certificate and no SSLStaplingForceURL "
"configured for server %s", mctx->sc->vhost_id);
return 0;
}
return 1;
}
if (!(issuer = stapling_get_issuer(mctx, x))) {
ssl_log_xerror(SSLLOG_MARK, APLOG_ERR, 0, ptemp, s, x, APLOGNO(02217)
"ssl_stapling_init_cert: can't retrieve issuer "
"certificate!");
return 0;
}
cid = OCSP_cert_to_id(NULL, x, issuer);
X509_free(issuer);
if (!cid) {
ssl_log_xerror(SSLLOG_MARK, APLOG_ERR, 0, ptemp, s, x, APLOGNO(02815)
"ssl_stapling_init_cert: can't create CertID "
"for OCSP request");
return 0;
}
aia = X509_get1_ocsp(x);
if (!aia && !mctx->stapling_force_url) {
OCSP_CERTID_free(cid);
ssl_log_xerror(SSLLOG_MARK, APLOG_ERR, 0, ptemp, s, x,
APLOGNO(02218) "ssl_stapling_init_cert: no OCSP URI "
"in certificate and no SSLStaplingForceURL set");
return 0;
}
cinf = apr_pcalloc(p, sizeof(certinfo));
memcpy (cinf->idx, idx, sizeof(idx));
cinf->cid = cid;
apr_pool_cleanup_register(p, cid, ssl_stapling_certid_free,
apr_pool_cleanup_null);
if (aia) {
cinf->uri = apr_pstrdup(p, sk_OPENSSL_STRING_value(aia, 0));
X509_email_free(aia);
}
ssl_log_xerror(SSLLOG_MARK, APLOG_TRACE1, 0, ptemp, s, x,
"ssl_stapling_init_cert: storing certinfo for server %s",
mctx->sc->vhost_id);
apr_hash_set(stapling_certinfo, cinf->idx, sizeof(cinf->idx), cinf);
return 1;
}
static certinfo *stapling_get_certinfo(server_rec *s, modssl_ctx_t *mctx,
SSL *ssl) {
certinfo *cinf;
X509 *x;
UCHAR idx[SHA_DIGEST_LENGTH];
x = SSL_get_certificate(ssl);
if ((x == NULL) || (X509_digest(x, EVP_sha1(), idx, NULL) != 1))
return NULL;
cinf = apr_hash_get(stapling_certinfo, idx, sizeof(idx));
if (cinf && cinf->cid)
return cinf;
ap_log_error(APLOG_MARK, APLOG_INFO, 0, s, APLOGNO(01926)
"stapling_get_certinfo: stapling not supported for certificate");
return NULL;
}
static BOOL stapling_cache_response(server_rec *s, modssl_ctx_t *mctx,
OCSP_RESPONSE *rsp, certinfo *cinf,
BOOL ok, apr_pool_t *pool) {
SSLModConfigRec *mc = myModConfig(s);
unsigned char resp_der[MAX_STAPLING_DER];
unsigned char *p;
int resp_derlen, stored_len;
BOOL rv;
apr_time_t expiry;
resp_derlen = i2d_OCSP_RESPONSE(rsp, NULL);
if (resp_derlen <= 0) {
ap_log_error(APLOG_MARK, APLOG_ERR, 0, s, APLOGNO(01927)
"OCSP stapling response encode error??");
return FALSE;
}
stored_len = resp_derlen + 1;
if (stored_len > sizeof resp_der) {
ap_log_error(APLOG_MARK, APLOG_ERR, 0, s, APLOGNO(01928)
"OCSP stapling response too big (%u bytes)", resp_derlen);
return FALSE;
}
p = resp_der;
if (ok == TRUE) {
*p++ = 1;
expiry = apr_time_from_sec(mctx->stapling_cache_timeout);
} else {
*p++ = 0;
expiry = apr_time_from_sec(mctx->stapling_errcache_timeout);
}
expiry += apr_time_now();
i2d_OCSP_RESPONSE(rsp, &p);
if (mc->stapling_cache->flags & AP_SOCACHE_FLAG_NOTMPSAFE)
stapling_cache_mutex_on(s);
rv = mc->stapling_cache->store(mc->stapling_cache_context, s,
cinf->idx, sizeof(cinf->idx),
expiry, resp_der, stored_len, pool);
if (mc->stapling_cache->flags & AP_SOCACHE_FLAG_NOTMPSAFE)
stapling_cache_mutex_off(s);
if (rv != APR_SUCCESS) {
ap_log_error(APLOG_MARK, APLOG_ERR, 0, s, APLOGNO(01929)
"stapling_cache_response: OCSP response session store error!");
return FALSE;
}
return TRUE;
}
static void stapling_get_cached_response(server_rec *s, OCSP_RESPONSE **prsp,
BOOL *pok, certinfo *cinf,
apr_pool_t *pool) {
SSLModConfigRec *mc = myModConfig(s);
apr_status_t rv;
OCSP_RESPONSE *rsp;
unsigned char resp_der[MAX_STAPLING_DER];
const unsigned char *p;
unsigned int resp_derlen = MAX_STAPLING_DER;
if (mc->stapling_cache->flags & AP_SOCACHE_FLAG_NOTMPSAFE)
stapling_cache_mutex_on(s);
rv = mc->stapling_cache->retrieve(mc->stapling_cache_context, s,
cinf->idx, sizeof(cinf->idx),
resp_der, &resp_derlen, pool);
if (mc->stapling_cache->flags & AP_SOCACHE_FLAG_NOTMPSAFE)
stapling_cache_mutex_off(s);
if (rv != APR_SUCCESS) {
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s, APLOGNO(01930)
"stapling_get_cached_response: cache miss");
return;
}
if (resp_derlen <= 1) {
ap_log_error(APLOG_MARK, APLOG_ERR, 0, s, APLOGNO(01931)
"stapling_get_cached_response: response length invalid??");
return;
}
p = resp_der;
if (*p)
*pok = TRUE;
else
*pok = FALSE;
p++;
resp_derlen--;
rsp = d2i_OCSP_RESPONSE(NULL, &p, resp_derlen);
if (!rsp) {
ap_log_error(APLOG_MARK, APLOG_ERR, 0, s, APLOGNO(01932)
"stapling_get_cached_response: response parse error??");
return;
}
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s, APLOGNO(01933)
"stapling_get_cached_response: cache hit");
*prsp = rsp;
}
static int stapling_set_response(SSL *ssl, OCSP_RESPONSE *rsp) {
int rspderlen;
unsigned char *rspder = NULL;
rspderlen = i2d_OCSP_RESPONSE(rsp, &rspder);
if (rspderlen <= 0)
return 0;
SSL_set_tlsext_status_ocsp_resp(ssl, rspder, rspderlen);
return 1;
}
static int stapling_check_response(server_rec *s, modssl_ctx_t *mctx,
certinfo *cinf, OCSP_RESPONSE *rsp,
BOOL *pok) {
int status = V_OCSP_CERTSTATUS_UNKNOWN;
int reason = OCSP_REVOKED_STATUS_NOSTATUS;
OCSP_BASICRESP *bs = NULL;
ASN1_GENERALIZEDTIME *rev, *thisupd, *nextupd;
int response_status = OCSP_response_status(rsp);
int rv = SSL_TLSEXT_ERR_OK;
if (pok)
*pok = FALSE;
if (response_status != OCSP_RESPONSE_STATUS_SUCCESSFUL) {
if (mctx->stapling_return_errors)
return SSL_TLSEXT_ERR_OK;
else
return SSL_TLSEXT_ERR_NOACK;
}
bs = OCSP_response_get1_basic(rsp);
if (bs == NULL) {
ap_log_error(APLOG_MARK, APLOG_ERR, 0, s, APLOGNO(01934)
"stapling_check_response: Error Parsing Response!");
return SSL_TLSEXT_ERR_OK;
}
if (!OCSP_resp_find_status(bs, cinf->cid, &status, &reason, &rev,
&thisupd, &nextupd)) {
ap_log_error(APLOG_MARK, APLOG_ERR, 0, s, APLOGNO(01935)
"stapling_check_response: certificate ID not present in response!");
if (mctx->stapling_return_errors == FALSE)
rv = SSL_TLSEXT_ERR_NOACK;
} else {
if (OCSP_check_validity(thisupd, nextupd,
mctx->stapling_resptime_skew,
mctx->stapling_resp_maxage)) {
if (pok)
*pok = TRUE;
} else {
if (pok) {
ap_log_error(APLOG_MARK, APLOG_ERR, 0, s, APLOGNO(01936)
"stapling_check_response: response times invalid");
} else {
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s, APLOGNO(01937)
"stapling_check_response: cached response expired");
}
rv = SSL_TLSEXT_ERR_NOACK;
}
if (status != V_OCSP_CERTSTATUS_GOOD) {
char snum[MAX_STRING_LEN] = { '\0' };
BIO *bio = BIO_new(BIO_s_mem());
if (bio) {
int n;
ASN1_INTEGER *pserial;
OCSP_id_get0_info(NULL, NULL, NULL, &pserial, cinf->cid);
if ((i2a_ASN1_INTEGER(bio, pserial) != -1) &&
((n = BIO_read(bio, snum, sizeof snum - 1)) > 0))
snum[n] = '\0';
BIO_free(bio);
}
ap_log_error(APLOG_MARK, APLOG_ERR, 0, s, APLOGNO(02969)
"stapling_check_response: response has certificate "
"status %s (reason: %s) for serial number %s",
OCSP_cert_status_str(status),
(reason != OCSP_REVOKED_STATUS_NOSTATUS) ?
OCSP_crl_reason_str(reason) : "n/a",
snum[0] ? snum : "[n/a]");
if (mctx->stapling_return_errors == FALSE) {
if (pok)
*pok = FALSE;
rv = SSL_TLSEXT_ERR_NOACK;
}
}
}
OCSP_BASICRESP_free(bs);
return rv;
}
static BOOL stapling_renew_response(server_rec *s, modssl_ctx_t *mctx, SSL *ssl,
certinfo *cinf, OCSP_RESPONSE **prsp,
BOOL *pok, apr_pool_t *pool) {
conn_rec *conn = (conn_rec *)SSL_get_app_data(ssl);
apr_pool_t *vpool;
OCSP_REQUEST *req = NULL;
OCSP_CERTID *id = NULL;
STACK_OF(X509_EXTENSION) *exts;
int i;
BOOL rv = TRUE;
const char *ocspuri;
apr_uri_t uri;
*prsp = NULL;
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s, APLOGNO(01938)
"stapling_renew_response: querying responder");
req = OCSP_REQUEST_new();
if (!req)
goto err;
id = OCSP_CERTID_dup(cinf->cid);
if (!id)
goto err;
if (!OCSP_request_add0_id(req, id))
goto err;
id = NULL;
SSL_get_tlsext_status_exts(ssl, &exts);
for (i = 0; i < sk_X509_EXTENSION_num(exts); i++) {
X509_EXTENSION *ext = sk_X509_EXTENSION_value(exts, i);
if (!OCSP_REQUEST_add_ext(req, ext, -1))
goto err;
}
if (mctx->stapling_force_url)
ocspuri = mctx->stapling_force_url;
else
ocspuri = cinf->uri;
if (!ocspuri) {
ap_log_error(APLOG_MARK, APLOG_ERR, 0, s, APLOGNO(02621)
"stapling_renew_response: no uri for responder");
rv = FALSE;
goto done;
}
apr_pool_create(&vpool, conn->pool);
if (apr_uri_parse(vpool, ocspuri, &uri) != APR_SUCCESS) {
ap_log_error(APLOG_MARK, APLOG_ERR, 0, s, APLOGNO(01939)
"stapling_renew_response: Error parsing uri %s",
ocspuri);
rv = FALSE;
goto done;
} else if (strcmp(uri.scheme, "http")) {
ap_log_error(APLOG_MARK, APLOG_ERR, 0, s, APLOGNO(01940)
"stapling_renew_response: Unsupported uri %s", ocspuri);
rv = FALSE;
goto done;
}
if (!uri.port) {
uri.port = apr_uri_port_of_scheme(uri.scheme);
}
*prsp = modssl_dispatch_ocsp_request(&uri, mctx->stapling_responder_timeout,
req, conn, vpool);
apr_pool_destroy(vpool);
if (!*prsp) {
ap_log_error(APLOG_MARK, APLOG_ERR, 0, s, APLOGNO(01941)
"stapling_renew_response: responder error");
if (mctx->stapling_fake_trylater) {
*prsp = OCSP_response_create(OCSP_RESPONSE_STATUS_TRYLATER, NULL);
} else {
goto done;
}
} else {
int response_status = OCSP_response_status(*prsp);
if (response_status == OCSP_RESPONSE_STATUS_SUCCESSFUL) {
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s, APLOGNO(01942)
"stapling_renew_response: query response received");
stapling_check_response(s, mctx, cinf, *prsp, pok);
if (*pok == FALSE) {
ap_log_error(APLOG_MARK, APLOG_ERR, 0, s, APLOGNO(01943)
"stapling_renew_response: error in retrieved response!");
}
} else {
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s, APLOGNO(01944)
"stapling_renew_response: responder error %s",
OCSP_response_status_str(response_status));
*pok = FALSE;
}
}
if (stapling_cache_response(s, mctx, *prsp, cinf, *pok, pool) == FALSE) {
ap_log_error(APLOG_MARK, APLOG_ERR, 0, s, APLOGNO(01945)
"stapling_renew_response: error caching response!");
}
done:
if (id)
OCSP_CERTID_free(id);
if (req)
OCSP_REQUEST_free(req);
return rv;
err:
rv = FALSE;
goto done;
}
static int ssl_stapling_mutex_init(server_rec *s, apr_pool_t *p) {
SSLModConfigRec *mc = myModConfig(s);
SSLSrvConfigRec *sc = mySrvConfig(s);
apr_status_t rv;
if (mc->stapling_refresh_mutex || sc->server->stapling_enabled != TRUE) {
return TRUE;
}
if (mc->stapling_cache->flags & AP_SOCACHE_FLAG_NOTMPSAFE) {
if ((rv = ap_global_mutex_create(&mc->stapling_cache_mutex, NULL,
SSL_STAPLING_CACHE_MUTEX_TYPE, NULL, s,
s->process->pool, 0)) != APR_SUCCESS) {
return FALSE;
}
}
if ((rv = ap_global_mutex_create(&mc->stapling_refresh_mutex, NULL,
SSL_STAPLING_REFRESH_MUTEX_TYPE, NULL, s,
s->process->pool, 0)) != APR_SUCCESS) {
return FALSE;
}
return TRUE;
}
static int stapling_mutex_reinit_helper(server_rec *s, apr_pool_t *p,
apr_global_mutex_t **mutex,
const char *type) {
apr_status_t rv;
const char *lockfile;
lockfile = apr_global_mutex_lockfile(*mutex);
if ((rv = apr_global_mutex_child_init(mutex,
lockfile, p)) != APR_SUCCESS) {
if (lockfile) {
ap_log_error(APLOG_MARK, APLOG_ERR, rv, s, APLOGNO(01946)
"Cannot reinit %s mutex with file `%s'",
type, lockfile);
} else {
ap_log_error(APLOG_MARK, APLOG_WARNING, rv, s, APLOGNO(01947)
"Cannot reinit %s mutex", type);
}
return FALSE;
}
return TRUE;
}
int ssl_stapling_mutex_reinit(server_rec *s, apr_pool_t *p) {
SSLModConfigRec *mc = myModConfig(s);
if (mc->stapling_cache_mutex != NULL
&& stapling_mutex_reinit_helper(s, p, &mc->stapling_cache_mutex,
SSL_STAPLING_CACHE_MUTEX_TYPE) == FALSE) {
return FALSE;
}
if (mc->stapling_refresh_mutex != NULL
&& stapling_mutex_reinit_helper(s, p, &mc->stapling_refresh_mutex,
SSL_STAPLING_REFRESH_MUTEX_TYPE) == FALSE) {
return FALSE;
}
return TRUE;
}
static int stapling_mutex_on(server_rec *s, apr_global_mutex_t *mutex,
const char *name) {
apr_status_t rv;
if ((rv = apr_global_mutex_lock(mutex)) != APR_SUCCESS) {
ap_log_error(APLOG_MARK, APLOG_WARNING, rv, s, APLOGNO(01948)
"Failed to acquire OCSP %s lock", name);
return FALSE;
}
return TRUE;
}
static int stapling_mutex_off(server_rec *s, apr_global_mutex_t *mutex,
const char *name) {
apr_status_t rv;
if ((rv = apr_global_mutex_unlock(mutex)) != APR_SUCCESS) {
ap_log_error(APLOG_MARK, APLOG_WARNING, rv, s, APLOGNO(01949)
"Failed to release OCSP %s lock", name);
return FALSE;
}
return TRUE;
}
static int stapling_cache_mutex_on(server_rec *s) {
SSLModConfigRec *mc = myModConfig(s);
return stapling_mutex_on(s, mc->stapling_cache_mutex,
SSL_STAPLING_CACHE_MUTEX_TYPE);
}
static int stapling_cache_mutex_off(server_rec *s) {
SSLModConfigRec *mc = myModConfig(s);
return stapling_mutex_off(s, mc->stapling_cache_mutex,
SSL_STAPLING_CACHE_MUTEX_TYPE);
}
static int stapling_refresh_mutex_on(server_rec *s) {
SSLModConfigRec *mc = myModConfig(s);
return stapling_mutex_on(s, mc->stapling_refresh_mutex,
SSL_STAPLING_REFRESH_MUTEX_TYPE);
}
static int stapling_refresh_mutex_off(server_rec *s) {
SSLModConfigRec *mc = myModConfig(s);
return stapling_mutex_off(s, mc->stapling_refresh_mutex,
SSL_STAPLING_REFRESH_MUTEX_TYPE);
}
static int get_and_check_cached_response(server_rec *s, modssl_ctx_t *mctx,
OCSP_RESPONSE **rsp, BOOL *pok,
certinfo *cinf, apr_pool_t *p) {
BOOL ok = FALSE;
int rv;
AP_DEBUG_ASSERT(*rsp == NULL);
stapling_get_cached_response(s, rsp, &ok, cinf, p);
if (*rsp) {
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s, APLOGNO(01953)
"stapling_cb: retrieved cached response");
rv = stapling_check_response(s, mctx, cinf, *rsp, NULL);
if (rv == SSL_TLSEXT_ERR_ALERT_FATAL) {
OCSP_RESPONSE_free(*rsp);
*rsp = NULL;
return SSL_TLSEXT_ERR_ALERT_FATAL;
} else if (rv == SSL_TLSEXT_ERR_NOACK) {
if (ok) {
OCSP_RESPONSE_free(*rsp);
*rsp = NULL;
} else if (!mctx->stapling_return_errors) {
OCSP_RESPONSE_free(*rsp);
*rsp = NULL;
*pok = FALSE;
return SSL_TLSEXT_ERR_NOACK;
}
}
}
return 0;
}
static int stapling_cb(SSL *ssl, void *arg) {
conn_rec *conn = (conn_rec *)SSL_get_app_data(ssl);
server_rec *s = mySrvFromConn(conn);
SSLSrvConfigRec *sc = mySrvConfig(s);
SSLConnRec *sslconn = myConnConfig(conn);
modssl_ctx_t *mctx = myCtxConfig(sslconn, sc);
certinfo *cinf = NULL;
OCSP_RESPONSE *rsp = NULL;
int rv;
BOOL ok = TRUE;
if (sc->server->stapling_enabled != TRUE) {
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s, APLOGNO(01950)
"stapling_cb: OCSP Stapling disabled");
return SSL_TLSEXT_ERR_NOACK;
}
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s, APLOGNO(01951)
"stapling_cb: OCSP Stapling callback called");
cinf = stapling_get_certinfo(s, mctx, ssl);
if (cinf == NULL) {
return SSL_TLSEXT_ERR_NOACK;
}
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s, APLOGNO(01952)
"stapling_cb: retrieved cached certificate data");
rv = get_and_check_cached_response(s, mctx, &rsp, &ok, cinf, conn->pool);
if (rv != 0) {
return rv;
}
if (rsp == NULL) {
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s, APLOGNO(01954)
"stapling_cb: renewing cached response");
stapling_refresh_mutex_on(s);
rv = get_and_check_cached_response(s, mctx, &rsp, &ok, cinf,
conn->pool);
if (rv != 0) {
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s, APLOGNO(03236)
"stapling_cb: error checking for cached response "
"after obtaining refresh mutex");
stapling_refresh_mutex_off(s);
return rv;
} else if (rsp) {
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s, APLOGNO(03237)
"stapling_cb: don't need to refresh cached response "
"after obtaining refresh mutex");
stapling_refresh_mutex_off(s);
} else {
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s, APLOGNO(03238)
"stapling_cb: still must refresh cached response "
"after obtaining refresh mutex");
rv = stapling_renew_response(s, mctx, ssl, cinf, &rsp, &ok,
conn->pool);
stapling_refresh_mutex_off(s);
if (rv == TRUE) {
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s, APLOGNO(03040)
"stapling_cb: success renewing response");
} else {
ap_log_error(APLOG_MARK, APLOG_ERR, 0, s, APLOGNO(01955)
"stapling_cb: fatal error renewing response");
return SSL_TLSEXT_ERR_ALERT_FATAL;
}
}
}
if (rsp && ((ok == TRUE) || (mctx->stapling_return_errors == TRUE))) {
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s, APLOGNO(01956)
"stapling_cb: setting response");
if (!stapling_set_response(ssl, rsp))
return SSL_TLSEXT_ERR_ALERT_FATAL;
return SSL_TLSEXT_ERR_OK;
}
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s, APLOGNO(01957)
"stapling_cb: no suitable response available");
return SSL_TLSEXT_ERR_NOACK;
}
apr_status_t modssl_init_stapling(server_rec *s, apr_pool_t *p,
apr_pool_t *ptemp, modssl_ctx_t *mctx) {
SSL_CTX *ctx = mctx->ssl_ctx;
SSLModConfigRec *mc = myModConfig(s);
if (mc->stapling_cache == NULL) {
ap_log_error(APLOG_MARK, APLOG_EMERG, 0, s, APLOGNO(01958)
"SSLStapling: no stapling cache available");
return ssl_die(s);
}
if (ssl_stapling_mutex_init(s, ptemp) == FALSE) {
ap_log_error(APLOG_MARK, APLOG_EMERG, 0, s, APLOGNO(01959)
"SSLStapling: cannot initialise stapling mutex");
return ssl_die(s);
}
if (mctx->stapling_resptime_skew == UNSET) {
mctx->stapling_resptime_skew = 60 * 5;
}
if (mctx->stapling_cache_timeout == UNSET) {
mctx->stapling_cache_timeout = 3600;
}
if (mctx->stapling_return_errors == UNSET) {
mctx->stapling_return_errors = TRUE;
}
if (mctx->stapling_fake_trylater == UNSET) {
mctx->stapling_fake_trylater = TRUE;
}
if (mctx->stapling_errcache_timeout == UNSET) {
mctx->stapling_errcache_timeout = 600;
}
if (mctx->stapling_responder_timeout == UNSET) {
mctx->stapling_responder_timeout = 10 * APR_USEC_PER_SEC;
}
SSL_CTX_set_tlsext_status_cb(ctx, stapling_cb);
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s, APLOGNO(01960) "OCSP stapling initialized");
return APR_SUCCESS;
}
#endif