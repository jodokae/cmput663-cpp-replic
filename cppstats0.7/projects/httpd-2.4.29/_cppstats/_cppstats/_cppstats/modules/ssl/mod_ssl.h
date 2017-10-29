#if !defined(__MOD_SSL_H__)
#define __MOD_SSL_H__
#include "httpd.h"
#include "apr_optional.h"
#if !defined(WIN32)
#define SSL_DECLARE(type) type
#define SSL_DECLARE_NONSTD(type) type
#define SSL_DECLARE_DATA
#elif defined(SSL_DECLARE_STATIC)
#define SSL_DECLARE(type) type __stdcall
#define SSL_DECLARE_NONSTD(type) type
#define SSL_DECLARE_DATA
#elif defined(SSL_DECLARE_EXPORT)
#define SSL_DECLARE(type) __declspec(dllexport) type __stdcall
#define SSL_DECLARE_NONSTD(type) __declspec(dllexport) type
#define SSL_DECLARE_DATA __declspec(dllexport)
#else
#define SSL_DECLARE(type) __declspec(dllimport) type __stdcall
#define SSL_DECLARE_NONSTD(type) __declspec(dllimport) type
#define SSL_DECLARE_DATA __declspec(dllimport)
#endif
APR_DECLARE_OPTIONAL_FN(char *, ssl_var_lookup,
(apr_pool_t *, server_rec *,
conn_rec *, request_rec *,
char *));
APR_DECLARE_OPTIONAL_FN(apr_array_header_t *, ssl_ext_list,
(apr_pool_t *p, conn_rec *c, int peer,
const char *extension));
APR_DECLARE_OPTIONAL_FN(int, ssl_is_https, (conn_rec *));
APR_DECLARE_OPTIONAL_FN(int, ssl_proxy_enable, (conn_rec *));
APR_DECLARE_OPTIONAL_FN(int, ssl_engine_disable, (conn_rec *));
#endif
