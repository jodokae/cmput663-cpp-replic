#if !defined(__MOD_SSL_OPENSSL_H__)
#define __MOD_SSL_OPENSSL_H__
#include "mod_ssl.h"
#if !defined(SSL_PRIVATE_H)
#include <openssl/opensslv.h>
#if (OPENSSL_VERSION_NUMBER >= 0x10001000)
#define OPENSSL_NO_SSL_INTERN
#endif
#include <openssl/ssl.h>
#endif
APR_DECLARE_EXTERNAL_HOOK(ssl, SSL, int, init_server,
(server_rec *s, apr_pool_t *p, int is_proxy, SSL_CTX *ctx))
APR_DECLARE_EXTERNAL_HOOK(ssl, SSL, int, pre_handshake,
(conn_rec *c, SSL *ssl, int is_proxy))
APR_DECLARE_EXTERNAL_HOOK(ssl, SSL, int, proxy_post_handshake,
(conn_rec *c, SSL *ssl))
#endif
