#if !defined(SSL_PRIVATE_H)
#define SSL_PRIVATE_H
#include "httpd.h"
#include "http_config.h"
#include "http_core.h"
#include "http_log.h"
#include "http_main.h"
#include "http_connection.h"
#include "http_request.h"
#include "http_protocol.h"
#include "http_vhost.h"
#include "util_script.h"
#include "util_filter.h"
#include "util_ebcdic.h"
#include "util_mutex.h"
#include "apr.h"
#include "apr_strings.h"
#define APR_WANT_STRFUNC
#define APR_WANT_MEMFUNC
#include "apr_want.h"
#include "apr_tables.h"
#include "apr_lib.h"
#include "apr_fnmatch.h"
#include "apr_strings.h"
#include "apr_global_mutex.h"
#include "apr_optional.h"
#include "ap_socache.h"
#include "mod_auth.h"
#if APR_HAVE_STDLIB_H
#include <stdlib.h>
#endif
#if APR_HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#if APR_HAVE_UNISTD_H
#include <unistd.h>
#endif
#if !defined(FALSE)
#define FALSE 0
#endif
#if !defined(TRUE)
#define TRUE !FALSE
#endif
#if !defined(BOOL)
#define BOOL unsigned int
#endif
#include "ap_expr.h"
#include <openssl/opensslv.h>
#if (OPENSSL_VERSION_NUMBER >= 0x10001000)
#define OPENSSL_NO_SSL_INTERN
#endif
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/x509.h>
#include <openssl/pem.h>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/x509v3.h>
#include <openssl/x509_vfy.h>
#include <openssl/ocsp.h>
#if defined(HAVE_OPENSSL_ENGINE_H) && defined(HAVE_ENGINE_INIT)
#include <openssl/engine.h>
#endif
#if (OPENSSL_VERSION_NUMBER < 0x0090801f)
#error mod_ssl requires OpenSSL 0.9.8a or later
#endif
#if (OPENSSL_VERSION_NUMBER >= 0x10000000)
#define MODSSL_SSL_CIPHER_CONST const
#define MODSSL_SSL_METHOD_CONST const
#else
#define MODSSL_SSL_CIPHER_CONST
#define MODSSL_SSL_METHOD_CONST
#endif
#if defined(LIBRESSL_VERSION_NUMBER)
#if LIBRESSL_VERSION_NUMBER < 0x2060000f
#define SSL_CTRL_SET_MIN_PROTO_VERSION 123
#define SSL_CTRL_SET_MAX_PROTO_VERSION 124
#define SSL_CTX_set_min_proto_version(ctx, version) SSL_CTX_ctrl(ctx, SSL_CTRL_SET_MIN_PROTO_VERSION, version, NULL)
#define SSL_CTX_set_max_proto_version(ctx, version) SSL_CTX_ctrl(ctx, SSL_CTRL_SET_MAX_PROTO_VERSION, version, NULL)
#endif
#define MODSSL_USE_OPENSSL_PRE_1_1_API (1)
#else
#define MODSSL_USE_OPENSSL_PRE_1_1_API (OPENSSL_VERSION_NUMBER < 0x10100000L)
#endif
#if defined(OPENSSL_FIPS)
#define HAVE_FIPS
#endif
#if defined(SSL_OP_NO_TLSv1_2)
#define HAVE_TLSV1_X
#endif
#if defined(SSL_CONF_FLAG_FILE)
#define HAVE_SSL_CONF_CMD
#endif
#if MODSSL_USE_OPENSSL_PRE_1_1_API
#define IDCONST
#else
#define IDCONST const
#endif
#if !defined(OPENSSL_NO_TLSEXT) && defined(SSL_set_tlsext_host_name)
#define HAVE_TLSEXT
#if !defined(OPENSSL_NO_EC) && defined(TLSEXT_ECPOINTFORMAT_uncompressed)
#define HAVE_ECC
#endif
#if !defined(OPENSSL_NO_OCSP) && defined(SSL_CTX_set_tlsext_status_cb)
#define HAVE_OCSP_STAPLING
#if OPENSSL_VERSION_NUMBER < 0x10100000L
#if !defined(sk_OPENSSL_STRING_num)
#define sk_OPENSSL_STRING_num sk_num
#endif
#if !defined(sk_OPENSSL_STRING_value)
#define sk_OPENSSL_STRING_value sk_value
#endif
#if !defined(sk_OPENSSL_STRING_pop)
#define sk_OPENSSL_STRING_pop sk_pop
#endif
#endif
#endif
#if defined(SSL_CTX_set_tlsext_ticket_key_cb)
#define HAVE_TLS_SESSION_TICKETS
#define TLSEXT_TICKET_KEY_LEN 48
#if !defined(tlsext_tick_md)
#if defined(OPENSSL_NO_SHA256)
#define tlsext_tick_md EVP_sha1
#else
#define tlsext_tick_md EVP_sha256
#endif
#endif
#endif
#if !defined(OPENSSL_NO_SRP) && defined(SSL_CTRL_SET_TLS_EXT_SRP_USERNAME_CB)
#define HAVE_SRP
#include <openssl/srp.h>
#endif
#if defined(TLSEXT_TYPE_application_layer_protocol_negotiation)
#define HAVE_TLS_ALPN
#endif
#endif
#if MODSSL_USE_OPENSSL_PRE_1_1_API
#define BN_get_rfc2409_prime_768 get_rfc2409_prime_768
#define BN_get_rfc2409_prime_1024 get_rfc2409_prime_1024
#define BN_get_rfc3526_prime_1536 get_rfc3526_prime_1536
#define BN_get_rfc3526_prime_2048 get_rfc3526_prime_2048
#define BN_get_rfc3526_prime_3072 get_rfc3526_prime_3072
#define BN_get_rfc3526_prime_4096 get_rfc3526_prime_4096
#define BN_get_rfc3526_prime_6144 get_rfc3526_prime_6144
#define BN_get_rfc3526_prime_8192 get_rfc3526_prime_8192
#define BIO_set_init(x,v) (x->init=v)
#define BIO_get_data(x) (x->ptr)
#define BIO_set_data(x,v) (x->ptr=v)
#define BIO_get_shutdown(x) (x->shutdown)
#define BIO_set_shutdown(x,v) (x->shutdown=v)
#define DH_bits(x) (BN_num_bits(x->p))
#else
void init_bio_methods(void);
void free_bio_methods(void);
#endif
#if OPENSSL_VERSION_NUMBER < 0x10002000L || defined(LIBRESSL_VERSION_NUMBER)
#define X509_STORE_CTX_get0_store(x) (x->ctx)
#endif
#if OPENSSL_VERSION_NUMBER < 0x10000000L
#if !defined(X509_STORE_CTX_get0_current_issuer)
#define X509_STORE_CTX_get0_current_issuer(x) (x->current_issuer)
#endif
#endif
#include "ssl_util_ssl.h"
APLOG_USE_MODULE(ssl);
#if !defined(PFALSE)
#define PFALSE ((void *)FALSE)
#endif
#if !defined(PTRUE)
#define PTRUE ((void *)TRUE)
#endif
#if !defined(UNSET)
#define UNSET (-1)
#endif
#if !defined(NUL)
#define NUL '\0'
#endif
#if !defined(RAND_MAX)
#include <limits.h>
#define RAND_MAX INT_MAX
#endif
#if !defined(UCHAR)
#define UCHAR unsigned char
#endif
#define strEQ(s1,s2) (strcmp(s1,s2) == 0)
#define strNE(s1,s2) (strcmp(s1,s2) != 0)
#define strEQn(s1,s2,n) (strncmp(s1,s2,n) == 0)
#define strNEn(s1,s2,n) (strncmp(s1,s2,n) != 0)
#define strcEQ(s1,s2) (strcasecmp(s1,s2) == 0)
#define strcNE(s1,s2) (strcasecmp(s1,s2) != 0)
#define strcEQn(s1,s2,n) (strncasecmp(s1,s2,n) == 0)
#define strcNEn(s1,s2,n) (strncasecmp(s1,s2,n) != 0)
#define strIsEmpty(s) (s == NULL || s[0] == NUL)
#define myConnConfig(c) (SSLConnRec *)ap_get_module_config(c->conn_config, &ssl_module)
#define myCtxConfig(sslconn, sc) (sslconn->is_proxy ? sc->proxy : sc->server)
#define myConnConfigSet(c, val) ap_set_module_config(c->conn_config, &ssl_module, val)
#define mySrvConfig(srv) (SSLSrvConfigRec *)ap_get_module_config(srv->module_config, &ssl_module)
#define myDirConfig(req) (SSLDirConfigRec *)ap_get_module_config(req->per_dir_config, &ssl_module)
#define myModConfig(srv) (mySrvConfig((srv)))->mc
#define mySrvFromConn(c) (myConnConfig(c))->server
#define mySrvConfigFromConn(c) mySrvConfig(mySrvFromConn(c))
#define myModConfigFromConn(c) myModConfig(mySrvFromConn(c))
#if !defined(SSL_SESSION_CACHE_TIMEOUT)
#define SSL_SESSION_CACHE_TIMEOUT 300
#endif
#if !defined(DEFAULT_RENEG_BUFFER_SIZE)
#define DEFAULT_RENEG_BUFFER_SIZE (128 * 1024)
#endif
#if !defined(DEFAULT_OCSP_MAX_SKEW)
#define DEFAULT_OCSP_MAX_SKEW (60 * 5)
#endif
#if !defined(DEFAULT_OCSP_TIMEOUT)
#define DEFAULT_OCSP_TIMEOUT 10
#endif
#if defined(HAVE_ECC)
#define CERTKEYS_IDX_MAX 2
#else
#define CERTKEYS_IDX_MAX 1
#endif
#define SSL_OPT_NONE (0)
#define SSL_OPT_RELSET (1<<0)
#define SSL_OPT_STDENVVARS (1<<1)
#define SSL_OPT_EXPORTCERTDATA (1<<3)
#define SSL_OPT_FAKEBASICAUTH (1<<4)
#define SSL_OPT_STRICTREQUIRE (1<<5)
#define SSL_OPT_OPTRENEGOTIATE (1<<6)
#define SSL_OPT_LEGACYDNFORMAT (1<<7)
typedef int ssl_opt_t;
#define SSL_PROTOCOL_NONE (0)
#if !defined(OPENSSL_NO_SSL3)
#define SSL_PROTOCOL_SSLV3 (1<<1)
#endif
#define SSL_PROTOCOL_TLSV1 (1<<2)
#if !defined(OPENSSL_NO_SSL3)
#define SSL_PROTOCOL_BASIC (SSL_PROTOCOL_SSLV3|SSL_PROTOCOL_TLSV1)
#else
#define SSL_PROTOCOL_BASIC (SSL_PROTOCOL_TLSV1)
#endif
#if defined(HAVE_TLSV1_X)
#define SSL_PROTOCOL_TLSV1_1 (1<<3)
#define SSL_PROTOCOL_TLSV1_2 (1<<4)
#define SSL_PROTOCOL_ALL (SSL_PROTOCOL_BASIC| SSL_PROTOCOL_TLSV1_1|SSL_PROTOCOL_TLSV1_2)
#else
#define SSL_PROTOCOL_ALL (SSL_PROTOCOL_BASIC)
#endif
#if !defined(OPENSSL_NO_SSL3)
#define SSL_PROTOCOL_DEFAULT (SSL_PROTOCOL_ALL & ~SSL_PROTOCOL_SSLV3)
#else
#define SSL_PROTOCOL_DEFAULT (SSL_PROTOCOL_ALL)
#endif
typedef int ssl_proto_t;
typedef enum {
SSL_CVERIFY_UNSET = UNSET,
SSL_CVERIFY_NONE = 0,
SSL_CVERIFY_OPTIONAL = 1,
SSL_CVERIFY_REQUIRE = 2,
SSL_CVERIFY_OPTIONAL_NO_CA = 3
} ssl_verify_t;
#define SSL_VERIFY_PEER_STRICT (SSL_VERIFY_PEER|SSL_VERIFY_FAIL_IF_NO_PEER_CERT)
#define ssl_verify_error_is_optional(errnum) ((errnum == X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT) || (errnum == X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN) || (errnum == X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY) || (errnum == X509_V_ERR_CERT_UNTRUSTED) || (errnum == X509_V_ERR_UNABLE_TO_VERIFY_LEAF_SIGNATURE))
typedef enum {
SSL_CRLCHECK_NONE = (0),
SSL_CRLCHECK_LEAF = (1 << 0),
SSL_CRLCHECK_CHAIN = (1 << 1),
#define SSL_CRLCHECK_FLAGS (~0x3)
SSL_CRLCHECK_NO_CRL_FOR_CERT_OK = (1 << 2)
} ssl_crlcheck_t;
typedef enum {
SSL_PPTYPE_UNSET = UNSET,
SSL_PPTYPE_BUILTIN = 0,
SSL_PPTYPE_FILTER = 1,
SSL_PPTYPE_PIPE = 2
} ssl_pphrase_t;
#define SSL_PCM_EXISTS 1
#define SSL_PCM_ISREG 2
#define SSL_PCM_ISDIR 4
#define SSL_PCM_ISNONZERO 8
typedef unsigned int ssl_pathcheck_t;
typedef enum {
SSL_ENABLED_UNSET = UNSET,
SSL_ENABLED_FALSE = 0,
SSL_ENABLED_TRUE = 1,
SSL_ENABLED_OPTIONAL = 3
} ssl_enabled_t;
typedef struct {
const char *cpExpr;
ap_expr_info_t *mpExpr;
} ssl_require_t;
typedef enum {
SSL_RSCTX_STARTUP = 1,
SSL_RSCTX_CONNECT = 2
} ssl_rsctx_t;
typedef enum {
SSL_RSSRC_BUILTIN = 1,
SSL_RSSRC_FILE = 2,
SSL_RSSRC_EXEC = 3,
SSL_RSSRC_EGD = 4
} ssl_rssrc_t;
typedef struct {
ssl_rsctx_t nCtx;
ssl_rssrc_t nSrc;
char *cpPath;
int nBytes;
} ssl_randseed_t;
typedef struct {
long int nData;
unsigned char *cpData;
apr_time_t source_mtime;
} ssl_asn1_t;
typedef enum {
SSL_SHUTDOWN_TYPE_UNSET,
SSL_SHUTDOWN_TYPE_STANDARD,
SSL_SHUTDOWN_TYPE_UNCLEAN,
SSL_SHUTDOWN_TYPE_ACCURATE
} ssl_shutdown_type_e;
typedef struct {
SSL *ssl;
const char *client_dn;
X509 *client_cert;
ssl_shutdown_type_e shutdown_type;
const char *verify_info;
const char *verify_error;
int verify_depth;
int is_proxy;
int disabled;
enum {
NON_SSL_OK = 0,
NON_SSL_SEND_REQLINE,
NON_SSL_SEND_HDR_SEP,
NON_SSL_SET_ERROR_MSG
} non_ssl_request;
enum {
RENEG_INIT = 0,
RENEG_REJECT,
RENEG_ALLOW,
RENEG_ABORT
} reneg_state;
server_rec *server;
const char *cipher_suite;
} SSLConnRec;
typedef struct {
pid_t pid;
apr_pool_t *pPool;
BOOL bFixed;
long sesscache_mode;
const ap_socache_provider_t *sesscache;
ap_socache_instance_t *sesscache_context;
apr_global_mutex_t *pMutex;
apr_array_header_t *aRandSeed;
apr_hash_t *tVHostKeys;
apr_hash_t *tPrivateKey;
#if defined(HAVE_OPENSSL_ENGINE_H) && defined(HAVE_ENGINE_INIT)
const char *szCryptoDevice;
#endif
#if defined(HAVE_OCSP_STAPLING)
const ap_socache_provider_t *stapling_cache;
ap_socache_instance_t *stapling_cache_context;
apr_global_mutex_t *stapling_cache_mutex;
apr_global_mutex_t *stapling_refresh_mutex;
#endif
} SSLModConfigRec;
typedef struct {
apr_array_header_t *cert_files;
apr_array_header_t *key_files;
const char *ca_name_path;
const char *ca_name_file;
} modssl_pk_server_t;
typedef struct {
const char *cert_file;
const char *cert_path;
const char *ca_cert_file;
STACK_OF(X509_INFO) *certs;
STACK_OF(X509) **ca_certs;
} modssl_pk_proxy_t;
typedef struct {
const char *ca_cert_path;
const char *ca_cert_file;
const char *cipher_suite;
int verify_depth;
ssl_verify_t verify_mode;
} modssl_auth_ctx_t;
#if defined(HAVE_TLS_SESSION_TICKETS)
typedef struct {
const char *file_path;
unsigned char key_name[16];
unsigned char hmac_secret[16];
unsigned char aes_key[16];
} modssl_ticket_key_t;
#endif
#if defined(HAVE_SSL_CONF_CMD)
typedef struct {
const char *name;
const char *value;
} ssl_ctx_param_t;
#endif
typedef struct SSLSrvConfigRec SSLSrvConfigRec;
typedef struct {
SSLSrvConfigRec *sc;
SSL_CTX *ssl_ctx;
modssl_pk_server_t *pks;
modssl_pk_proxy_t *pkp;
#if defined(HAVE_TLS_SESSION_TICKETS)
modssl_ticket_key_t *ticket_key;
#endif
ssl_proto_t protocol;
int protocol_set;
ssl_pphrase_t pphrase_dialog_type;
const char *pphrase_dialog_path;
const char *cert_chain;
const char *crl_path;
const char *crl_file;
int crl_check_mask;
#if defined(HAVE_OCSP_STAPLING)
BOOL stapling_enabled;
long stapling_resptime_skew;
long stapling_resp_maxage;
int stapling_cache_timeout;
BOOL stapling_return_errors;
BOOL stapling_fake_trylater;
int stapling_errcache_timeout;
apr_interval_time_t stapling_responder_timeout;
const char *stapling_force_url;
#endif
#if defined(HAVE_SRP)
char *srp_vfile;
char *srp_unknown_user_seed;
SRP_VBASE *srp_vbase;
#endif
modssl_auth_ctx_t auth;
BOOL ocsp_enabled;
BOOL ocsp_force_default;
const char *ocsp_responder;
long ocsp_resptime_skew;
long ocsp_resp_maxage;
apr_interval_time_t ocsp_responder_timeout;
BOOL ocsp_use_request_nonce;
apr_uri_t *proxy_uri;
BOOL ocsp_noverify;
int ocsp_verify_flags;
const char *ocsp_certs_file;
STACK_OF(X509) *ocsp_certs;
#if defined(HAVE_SSL_CONF_CMD)
SSL_CONF_CTX *ssl_ctx_config;
apr_array_header_t *ssl_ctx_param;
#endif
} modssl_ctx_t;
struct SSLSrvConfigRec {
SSLModConfigRec *mc;
ssl_enabled_t enabled;
BOOL proxy_enabled;
const char *vhost_id;
int vhost_id_len;
int session_cache_timeout;
BOOL cipher_server_pref;
BOOL insecure_reneg;
modssl_ctx_t *server;
modssl_ctx_t *proxy;
ssl_enabled_t proxy_ssl_check_peer_expire;
ssl_enabled_t proxy_ssl_check_peer_cn;
ssl_enabled_t proxy_ssl_check_peer_name;
#if defined(HAVE_TLSEXT)
ssl_enabled_t strict_sni_vhost_check;
#endif
#if defined(HAVE_FIPS)
BOOL fips;
#endif
#if !defined(OPENSSL_NO_COMP)
BOOL compression;
#endif
BOOL session_tickets;
};
typedef struct {
BOOL bSSLRequired;
apr_array_header_t *aRequirement;
ssl_opt_t nOptions;
ssl_opt_t nOptionsAdd;
ssl_opt_t nOptionsDel;
const char *szCipherSuite;
ssl_verify_t nVerifyClient;
int nVerifyDepth;
const char *szCACertificatePath;
const char *szCACertificateFile;
const char *szUserName;
apr_size_t nRenegBufferSize;
} SSLDirConfigRec;
extern module AP_MODULE_DECLARE_DATA ssl_module;
SSLModConfigRec *ssl_config_global_create(server_rec *);
void ssl_config_global_fix(SSLModConfigRec *);
BOOL ssl_config_global_isfixed(SSLModConfigRec *);
void *ssl_config_server_create(apr_pool_t *, server_rec *);
void *ssl_config_server_merge(apr_pool_t *, void *, void *);
void *ssl_config_perdir_create(apr_pool_t *, char *);
void *ssl_config_perdir_merge(apr_pool_t *, void *, void *);
const char *ssl_cmd_SSLPassPhraseDialog(cmd_parms *, void *, const char *);
const char *ssl_cmd_SSLCryptoDevice(cmd_parms *, void *, const char *);
const char *ssl_cmd_SSLRandomSeed(cmd_parms *, void *, const char *, const char *, const char *);
const char *ssl_cmd_SSLEngine(cmd_parms *, void *, const char *);
const char *ssl_cmd_SSLCipherSuite(cmd_parms *, void *, const char *);
const char *ssl_cmd_SSLCertificateFile(cmd_parms *, void *, const char *);
const char *ssl_cmd_SSLCertificateKeyFile(cmd_parms *, void *, const char *);
const char *ssl_cmd_SSLCertificateChainFile(cmd_parms *, void *, const char *);
const char *ssl_cmd_SSLCACertificatePath(cmd_parms *, void *, const char *);
const char *ssl_cmd_SSLCACertificateFile(cmd_parms *, void *, const char *);
const char *ssl_cmd_SSLCADNRequestPath(cmd_parms *, void *, const char *);
const char *ssl_cmd_SSLCADNRequestFile(cmd_parms *, void *, const char *);
const char *ssl_cmd_SSLCARevocationPath(cmd_parms *, void *, const char *);
const char *ssl_cmd_SSLCARevocationFile(cmd_parms *, void *, const char *);
const char *ssl_cmd_SSLCARevocationCheck(cmd_parms *, void *, const char *);
const char *ssl_cmd_SSLHonorCipherOrder(cmd_parms *cmd, void *dcfg, int flag);
const char *ssl_cmd_SSLCompression(cmd_parms *, void *, int flag);
const char *ssl_cmd_SSLSessionTickets(cmd_parms *, void *, int flag);
const char *ssl_cmd_SSLVerifyClient(cmd_parms *, void *, const char *);
const char *ssl_cmd_SSLVerifyDepth(cmd_parms *, void *, const char *);
const char *ssl_cmd_SSLSessionCache(cmd_parms *, void *, const char *);
const char *ssl_cmd_SSLSessionCacheTimeout(cmd_parms *, void *, const char *);
const char *ssl_cmd_SSLProtocol(cmd_parms *, void *, const char *);
const char *ssl_cmd_SSLOptions(cmd_parms *, void *, const char *);
const char *ssl_cmd_SSLRequireSSL(cmd_parms *, void *);
const char *ssl_cmd_SSLRequire(cmd_parms *, void *, const char *);
const char *ssl_cmd_SSLUserName(cmd_parms *, void *, const char *);
const char *ssl_cmd_SSLRenegBufferSize(cmd_parms *cmd, void *dcfg, const char *arg);
const char *ssl_cmd_SSLStrictSNIVHostCheck(cmd_parms *cmd, void *dcfg, int flag);
const char *ssl_cmd_SSLInsecureRenegotiation(cmd_parms *cmd, void *dcfg, int flag);
const char *ssl_cmd_SSLProxyEngine(cmd_parms *cmd, void *dcfg, int flag);
const char *ssl_cmd_SSLProxyProtocol(cmd_parms *, void *, const char *);
const char *ssl_cmd_SSLProxyCipherSuite(cmd_parms *, void *, const char *);
const char *ssl_cmd_SSLProxyVerify(cmd_parms *, void *, const char *);
const char *ssl_cmd_SSLProxyVerifyDepth(cmd_parms *, void *, const char *);
const char *ssl_cmd_SSLProxyCACertificatePath(cmd_parms *, void *, const char *);
const char *ssl_cmd_SSLProxyCACertificateFile(cmd_parms *, void *, const char *);
const char *ssl_cmd_SSLProxyCARevocationPath(cmd_parms *, void *, const char *);
const char *ssl_cmd_SSLProxyCARevocationFile(cmd_parms *, void *, const char *);
const char *ssl_cmd_SSLProxyCARevocationCheck(cmd_parms *, void *, const char *);
const char *ssl_cmd_SSLProxyMachineCertificatePath(cmd_parms *, void *, const char *);
const char *ssl_cmd_SSLProxyMachineCertificateFile(cmd_parms *, void *, const char *);
const char *ssl_cmd_SSLProxyMachineCertificateChainFile(cmd_parms *, void *, const char *);
#if defined(HAVE_TLS_SESSION_TICKETS)
const char *ssl_cmd_SSLSessionTicketKeyFile(cmd_parms *cmd, void *dcfg, const char *arg);
#endif
const char *ssl_cmd_SSLProxyCheckPeerExpire(cmd_parms *cmd, void *dcfg, int flag);
const char *ssl_cmd_SSLProxyCheckPeerCN(cmd_parms *cmd, void *dcfg, int flag);
const char *ssl_cmd_SSLProxyCheckPeerName(cmd_parms *cmd, void *dcfg, int flag);
const char *ssl_cmd_SSLOCSPOverrideResponder(cmd_parms *cmd, void *dcfg, int flag);
const char *ssl_cmd_SSLOCSPDefaultResponder(cmd_parms *cmd, void *dcfg, const char *arg);
const char *ssl_cmd_SSLOCSPResponseTimeSkew(cmd_parms *cmd, void *dcfg, const char *arg);
const char *ssl_cmd_SSLOCSPResponseMaxAge(cmd_parms *cmd, void *dcfg, const char *arg);
const char *ssl_cmd_SSLOCSPResponderTimeout(cmd_parms *cmd, void *dcfg, const char *arg);
const char *ssl_cmd_SSLOCSPUseRequestNonce(cmd_parms *cmd, void *dcfg, int flag);
const char *ssl_cmd_SSLOCSPEnable(cmd_parms *cmd, void *dcfg, int flag);
const char *ssl_cmd_SSLOCSPProxyURL(cmd_parms *cmd, void *dcfg, const char *arg);
const char *ssl_cmd_SSLOCSPNoVerify(cmd_parms *cmd, void *dcfg, int flag);
const char *ssl_cmd_SSLOCSPResponderCertificateFile(cmd_parms *cmd, void *dcfg, const char *arg);
#if defined(HAVE_SSL_CONF_CMD)
const char *ssl_cmd_SSLOpenSSLConfCmd(cmd_parms *cmd, void *dcfg, const char *arg1, const char *arg2);
#endif
#if defined(HAVE_SRP)
const char *ssl_cmd_SSLSRPVerifierFile(cmd_parms *cmd, void *dcfg, const char *arg);
const char *ssl_cmd_SSLSRPUnknownUserSeed(cmd_parms *cmd, void *dcfg, const char *arg);
#endif
const char *ssl_cmd_SSLFIPS(cmd_parms *cmd, void *dcfg, int flag);
apr_status_t ssl_init_Module(apr_pool_t *, apr_pool_t *, apr_pool_t *, server_rec *);
apr_status_t ssl_init_Engine(server_rec *, apr_pool_t *);
apr_status_t ssl_init_ConfigureServer(server_rec *, apr_pool_t *, apr_pool_t *, SSLSrvConfigRec *,
apr_array_header_t *);
apr_status_t ssl_init_CheckServers(server_rec *, apr_pool_t *);
STACK_OF(X509_NAME)
*ssl_init_FindCAList(server_rec *, apr_pool_t *, const char *, const char *);
void ssl_init_Child(apr_pool_t *, server_rec *);
apr_status_t ssl_init_ModuleKill(void *data);
int ssl_hook_Auth(request_rec *);
int ssl_hook_UserCheck(request_rec *);
int ssl_hook_Access(request_rec *);
int ssl_hook_Fixup(request_rec *);
int ssl_hook_ReadReq(request_rec *);
int ssl_hook_Upgrade(request_rec *);
void ssl_hook_ConfigTest(apr_pool_t *pconf, server_rec *s);
extern const authz_provider ssl_authz_provider_require_ssl;
extern const authz_provider ssl_authz_provider_verify_client;
DH *ssl_callback_TmpDH(SSL *, int, int);
int ssl_callback_SSLVerify(int, X509_STORE_CTX *);
int ssl_callback_SSLVerify_CRL(int, X509_STORE_CTX *, conn_rec *);
int ssl_callback_proxy_cert(SSL *ssl, X509 **x509, EVP_PKEY **pkey);
int ssl_callback_NewSessionCacheEntry(SSL *, SSL_SESSION *);
SSL_SESSION *ssl_callback_GetSessionCacheEntry(SSL *, IDCONST unsigned char *, int, int *);
void ssl_callback_DelSessionCacheEntry(SSL_CTX *, SSL_SESSION *);
void ssl_callback_Info(const SSL *, int, int);
#if defined(HAVE_TLSEXT)
int ssl_callback_ServerNameIndication(SSL *, int *, modssl_ctx_t *);
#endif
#if defined(HAVE_TLS_SESSION_TICKETS)
int ssl_callback_SessionTicket(SSL *, unsigned char *, unsigned char *,
EVP_CIPHER_CTX *, HMAC_CTX *, int);
#endif
#if defined(HAVE_TLS_ALPN)
int ssl_callback_alpn_select(SSL *ssl, const unsigned char **out,
unsigned char *outlen, const unsigned char *in,
unsigned int inlen, void *arg);
#endif
apr_status_t ssl_scache_init(server_rec *, apr_pool_t *);
void ssl_scache_status_register(apr_pool_t *p);
void ssl_scache_kill(server_rec *);
BOOL ssl_scache_store(server_rec *, IDCONST UCHAR *, int,
apr_time_t, SSL_SESSION *, apr_pool_t *);
SSL_SESSION *ssl_scache_retrieve(server_rec *, IDCONST UCHAR *, int, apr_pool_t *);
void ssl_scache_remove(server_rec *, IDCONST UCHAR *, int,
apr_pool_t *);
int ssl_proxy_enable(conn_rec *c);
int ssl_engine_disable(conn_rec *c);
#if defined(HAVE_OCSP_STAPLING)
const char *ssl_cmd_SSLStaplingCache(cmd_parms *, void *, const char *);
const char *ssl_cmd_SSLUseStapling(cmd_parms *, void *, int);
const char *ssl_cmd_SSLStaplingResponseTimeSkew(cmd_parms *, void *, const char *);
const char *ssl_cmd_SSLStaplingResponseMaxAge(cmd_parms *, void *, const char *);
const char *ssl_cmd_SSLStaplingStandardCacheTimeout(cmd_parms *, void *, const char *);
const char *ssl_cmd_SSLStaplingErrorCacheTimeout(cmd_parms *, void *, const char *);
const char *ssl_cmd_SSLStaplingReturnResponderErrors(cmd_parms *, void *, int);
const char *ssl_cmd_SSLStaplingFakeTryLater(cmd_parms *, void *, int);
const char *ssl_cmd_SSLStaplingResponderTimeout(cmd_parms *, void *, const char *);
const char *ssl_cmd_SSLStaplingForceURL(cmd_parms *, void *, const char *);
apr_status_t modssl_init_stapling(server_rec *, apr_pool_t *, apr_pool_t *, modssl_ctx_t *);
void ssl_stapling_certinfo_hash_init(apr_pool_t *);
int ssl_stapling_init_cert(server_rec *, apr_pool_t *, apr_pool_t *,
modssl_ctx_t *, X509 *);
#endif
#if defined(HAVE_SRP)
int ssl_callback_SRPServerParams(SSL *, int *, void *);
#endif
void ssl_io_filter_init(conn_rec *, request_rec *r, SSL *);
void ssl_io_filter_register(apr_pool_t *);
long ssl_io_data_cb(BIO *, int, const char *, int, long, long);
int ssl_io_buffer_fill(request_rec *r, apr_size_t maxlen);
int ssl_rand_seed(server_rec *, apr_pool_t *, ssl_rsctx_t, char *);
char *ssl_util_vhostid(apr_pool_t *, server_rec *);
apr_file_t *ssl_util_ppopen(server_rec *, apr_pool_t *, const char *,
const char * const *);
void ssl_util_ppclose(server_rec *, apr_pool_t *, apr_file_t *);
char *ssl_util_readfilter(server_rec *, apr_pool_t *, const char *,
const char * const *);
BOOL ssl_util_path_check(ssl_pathcheck_t, const char *, apr_pool_t *);
#if APR_HAS_THREADS && MODSSL_USE_OPENSSL_PRE_1_1_API
void ssl_util_thread_setup(apr_pool_t *);
void ssl_util_thread_id_setup(apr_pool_t *);
#endif
int ssl_init_ssl_connection(conn_rec *c, request_rec *r);
BOOL ssl_util_vhost_matches(const char *servername, server_rec *s);
apr_status_t ssl_load_encrypted_pkey(server_rec *, apr_pool_t *, int,
const char *, apr_array_header_t **);
DH *ssl_dh_GetParamFromFile(const char *);
#if defined(HAVE_ECC)
EC_GROUP *ssl_ec_GetParamFromFile(const char *);
#endif
unsigned char *ssl_asn1_table_set(apr_hash_t *table,
const char *key,
long int length);
ssl_asn1_t *ssl_asn1_table_get(apr_hash_t *table,
const char *key);
void ssl_asn1_table_unset(apr_hash_t *table,
const char *key);
int ssl_mutex_init(server_rec *, apr_pool_t *);
int ssl_mutex_reinit(server_rec *, apr_pool_t *);
int ssl_mutex_on(server_rec *);
int ssl_mutex_off(server_rec *);
int ssl_stapling_mutex_reinit(server_rec *, apr_pool_t *);
#define SSL_CACHE_MUTEX_TYPE "ssl-cache"
#define SSL_STAPLING_CACHE_MUTEX_TYPE "ssl-stapling"
#define SSL_STAPLING_REFRESH_MUTEX_TYPE "ssl-stapling-refresh"
apr_status_t ssl_die(server_rec *);
void ssl_log_ssl_error(const char *, int, int, server_rec *);
void ssl_log_xerror(const char *file, int line, int level,
apr_status_t rv, apr_pool_t *p, server_rec *s,
X509 *cert, const char *format, ...)
__attribute__((format(printf,8,9)));
void ssl_log_cxerror(const char *file, int line, int level,
apr_status_t rv, conn_rec *c, X509 *cert,
const char *format, ...)
__attribute__((format(printf,7,8)));
void ssl_log_rxerror(const char *file, int line, int level,
apr_status_t rv, request_rec *r, X509 *cert,
const char *format, ...)
__attribute__((format(printf,7,8)));
#define SSLLOG_MARK __FILE__,__LINE__
void ssl_var_register(apr_pool_t *p);
char *ssl_var_lookup(apr_pool_t *, server_rec *, conn_rec *, request_rec *, char *);
apr_array_header_t *ssl_ext_list(apr_pool_t *p, conn_rec *c, int peer, const char *extension);
void ssl_var_log_config_register(apr_pool_t *p);
void modssl_var_extract_dns(apr_table_t *t, SSL *ssl, apr_pool_t *p);
void modssl_var_extract_san_entries(apr_table_t *t, SSL *ssl, apr_pool_t *p);
#if !defined(OPENSSL_NO_OCSP)
int modssl_verify_ocsp(X509_STORE_CTX *ctx, SSLSrvConfigRec *sc,
server_rec *s, conn_rec *c, apr_pool_t *pool);
OCSP_RESPONSE *modssl_dispatch_ocsp_request(const apr_uri_t *uri,
apr_interval_time_t timeout,
OCSP_REQUEST *request,
conn_rec *c, apr_pool_t *p);
void ssl_init_ocsp_certificates(server_rec *s, modssl_ctx_t *mctx);
#endif
DH *modssl_get_dh_params(unsigned keylen);
#endif
