#if !defined(__SSL_UTIL_SSL_H__)
#define __SSL_UTIL_SSL_H__
#define MODSSL_LIBRARY_VERSION OPENSSL_VERSION_NUMBER
#define MODSSL_LIBRARY_NAME "OpenSSL"
#define MODSSL_LIBRARY_TEXT OPENSSL_VERSION_TEXT
#if MODSSL_USE_OPENSSL_PRE_1_1_API
#define MODSSL_LIBRARY_DYNTEXT SSLeay_version(SSLEAY_VERSION)
#else
#define MODSSL_LIBRARY_DYNTEXT OpenSSL_version(OPENSSL_VERSION)
#endif
#define MODSSL_SESSION_MAX_DER 1024*10
#define MODSSL_SESSION_ID_STRING_LEN ((SSL_MAX_SSL_SESSION_ID_LENGTH + 1) * 2)
void modssl_init_app_data2_idx(void);
void *modssl_get_app_data2(SSL *);
void modssl_set_app_data2(SSL *, void *);
EVP_PKEY *modssl_read_privatekey(const char *, EVP_PKEY **, pem_password_cb *, void *);
int modssl_smart_shutdown(SSL *ssl);
BOOL modssl_X509_getBC(X509 *, int *, int *);
char *modssl_X509_NAME_ENTRY_to_string(apr_pool_t *p, X509_NAME_ENTRY *xsne);
char *modssl_X509_NAME_to_string(apr_pool_t *, X509_NAME *, int);
BOOL modssl_X509_getSAN(apr_pool_t *, X509 *, int, const char *, int, apr_array_header_t **);
BOOL modssl_X509_match_name(apr_pool_t *, X509 *, const char *, BOOL, server_rec *);
char *modssl_SSL_SESSION_id2sz(IDCONST unsigned char *, int, char *, int);
#endif
