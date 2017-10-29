#if !defined(SVN_AUTH_H)
#define SVN_AUTH_H
#include <apr_pools.h>
#include "svn_types.h"
#if defined(__cplusplus)
extern "C" {
#endif
typedef struct svn_auth_baton_t svn_auth_baton_t;
typedef struct svn_auth_iterstate_t svn_auth_iterstate_t;
typedef struct svn_auth_provider_t {
const char *cred_kind;
svn_error_t * (*first_credentials)(void **credentials,
void **iter_baton,
void *provider_baton,
apr_hash_t *parameters,
const char *realmstring,
apr_pool_t *pool);
svn_error_t * (*next_credentials)(void **credentials,
void *iter_baton,
void *provider_baton,
apr_hash_t *parameters,
const char *realmstring,
apr_pool_t *pool);
svn_error_t * (*save_credentials)(svn_boolean_t *saved,
void *credentials,
void *provider_baton,
apr_hash_t *parameters,
const char *realmstring,
apr_pool_t *pool);
} svn_auth_provider_t;
typedef struct svn_auth_provider_object_t {
const svn_auth_provider_t *vtable;
void *provider_baton;
} svn_auth_provider_object_t;
#define SVN_AUTH_CRED_SIMPLE "svn.simple"
typedef struct svn_auth_cred_simple_t {
const char *username;
const char *password;
svn_boolean_t may_save;
} svn_auth_cred_simple_t;
#define SVN_AUTH_CRED_USERNAME "svn.username"
typedef struct svn_auth_cred_username_t {
const char *username;
svn_boolean_t may_save;
} svn_auth_cred_username_t;
#define SVN_AUTH_CRED_SSL_CLIENT_CERT "svn.ssl.client-cert"
typedef struct svn_auth_cred_ssl_client_cert_t {
const char *cert_file;
svn_boolean_t may_save;
} svn_auth_cred_ssl_client_cert_t;
#define SVN_AUTH_CRED_SSL_CLIENT_CERT_PW "svn.ssl.client-passphrase"
typedef struct svn_auth_cred_ssl_client_cert_pw_t {
const char *password;
svn_boolean_t may_save;
} svn_auth_cred_ssl_client_cert_pw_t;
#define SVN_AUTH_CRED_SSL_SERVER_TRUST "svn.ssl.server"
typedef struct svn_auth_ssl_server_cert_info_t {
const char *hostname;
const char *fingerprint;
const char *valid_from;
const char *valid_until;
const char *issuer_dname;
const char *ascii_cert;
} svn_auth_ssl_server_cert_info_t;
svn_auth_ssl_server_cert_info_t *
svn_auth_ssl_server_cert_info_dup(const svn_auth_ssl_server_cert_info_t *info,
apr_pool_t *pool);
typedef struct svn_auth_cred_ssl_server_trust_t {
svn_boolean_t may_save;
apr_uint32_t accepted_failures;
} svn_auth_cred_ssl_server_trust_t;
typedef svn_error_t *(*svn_auth_simple_prompt_func_t)
(svn_auth_cred_simple_t **cred,
void *baton,
const char *realm,
const char *username,
svn_boolean_t may_save,
apr_pool_t *pool);
typedef svn_error_t *(*svn_auth_username_prompt_func_t)
(svn_auth_cred_username_t **cred,
void *baton,
const char *realm,
svn_boolean_t may_save,
apr_pool_t *pool);
#define SVN_AUTH_SSL_NOTYETVALID 0x00000001
#define SVN_AUTH_SSL_EXPIRED 0x00000002
#define SVN_AUTH_SSL_CNMISMATCH 0x00000004
#define SVN_AUTH_SSL_UNKNOWNCA 0x00000008
#define SVN_AUTH_SSL_OTHER 0x40000000
typedef svn_error_t *(*svn_auth_ssl_server_trust_prompt_func_t)
(svn_auth_cred_ssl_server_trust_t **cred,
void *baton,
const char *realm,
apr_uint32_t failures,
const svn_auth_ssl_server_cert_info_t *cert_info,
svn_boolean_t may_save,
apr_pool_t *pool);
typedef svn_error_t *(*svn_auth_ssl_client_cert_prompt_func_t)
(svn_auth_cred_ssl_client_cert_t **cred,
void *baton,
const char *realm,
svn_boolean_t may_save,
apr_pool_t *pool);
typedef svn_error_t *(*svn_auth_ssl_client_cert_pw_prompt_func_t)
(svn_auth_cred_ssl_client_cert_pw_t **cred,
void *baton,
const char *realm,
svn_boolean_t may_save,
apr_pool_t *pool);
void svn_auth_open(svn_auth_baton_t **auth_baton,
apr_array_header_t *providers,
apr_pool_t *pool);
void svn_auth_set_parameter(svn_auth_baton_t *auth_baton,
const char *name,
const void *value);
const void * svn_auth_get_parameter(svn_auth_baton_t *auth_baton,
const char *name);
#define SVN_AUTH_PARAM_PREFIX "svn:auth:"
#define SVN_AUTH_PARAM_DEFAULT_USERNAME SVN_AUTH_PARAM_PREFIX "username"
#define SVN_AUTH_PARAM_DEFAULT_PASSWORD SVN_AUTH_PARAM_PREFIX "password"
#define SVN_AUTH_PARAM_NON_INTERACTIVE SVN_AUTH_PARAM_PREFIX "non-interactive"
#define SVN_AUTH_PARAM_DONT_STORE_PASSWORDS SVN_AUTH_PARAM_PREFIX "dont-store-passwords"
#define SVN_AUTH_PARAM_NO_AUTH_CACHE SVN_AUTH_PARAM_PREFIX "no-auth-cache"
#define SVN_AUTH_PARAM_SSL_SERVER_FAILURES SVN_AUTH_PARAM_PREFIX "ssl:failures"
#define SVN_AUTH_PARAM_SSL_SERVER_CERT_INFO SVN_AUTH_PARAM_PREFIX "ssl:cert-info"
#define SVN_AUTH_PARAM_CONFIG SVN_AUTH_PARAM_PREFIX "config"
#define SVN_AUTH_PARAM_SERVER_GROUP SVN_AUTH_PARAM_PREFIX "server-group"
#define SVN_AUTH_PARAM_CONFIG_DIR SVN_AUTH_PARAM_PREFIX "config-dir"
svn_error_t * svn_auth_first_credentials(void **credentials,
svn_auth_iterstate_t **state,
const char *cred_kind,
const char *realmstring,
svn_auth_baton_t *auth_baton,
apr_pool_t *pool);
svn_error_t * svn_auth_next_credentials(void **credentials,
svn_auth_iterstate_t *state,
apr_pool_t *pool);
svn_error_t * svn_auth_save_credentials(svn_auth_iterstate_t *state,
apr_pool_t *pool);
void
svn_auth_get_simple_prompt_provider(svn_auth_provider_object_t **provider,
svn_auth_simple_prompt_func_t prompt_func,
void *prompt_baton,
int retry_limit,
apr_pool_t *pool);
void svn_auth_get_username_prompt_provider
(svn_auth_provider_object_t **provider,
svn_auth_username_prompt_func_t prompt_func,
void *prompt_baton,
int retry_limit,
apr_pool_t *pool);
void svn_auth_get_simple_provider(svn_auth_provider_object_t **provider,
apr_pool_t *pool);
#if (defined(WIN32) && !defined(__MINGW32__)) || defined(DOXYGEN)
void
svn_auth_get_windows_simple_provider(svn_auth_provider_object_t **provider,
apr_pool_t *pool);
#endif
#if defined(DARWIN) || defined(DOXYGEN)
void
svn_auth_get_keychain_simple_provider(svn_auth_provider_object_t **provider,
apr_pool_t *pool);
#endif
void svn_auth_get_username_provider(svn_auth_provider_object_t **provider,
apr_pool_t *pool);
void svn_auth_get_ssl_server_trust_file_provider
(svn_auth_provider_object_t **provider,
apr_pool_t *pool);
#if (defined(WIN32) && !defined(__MINGW32__)) || defined(DOXYGEN)
void
svn_auth_get_windows_ssl_server_trust_provider
(svn_auth_provider_object_t **provider,
apr_pool_t *pool);
#endif
void svn_auth_get_ssl_client_cert_file_provider
(svn_auth_provider_object_t **provider,
apr_pool_t *pool);
void svn_auth_get_ssl_client_cert_pw_file_provider
(svn_auth_provider_object_t **provider,
apr_pool_t *pool);
void svn_auth_get_ssl_server_trust_prompt_provider
(svn_auth_provider_object_t **provider,
svn_auth_ssl_server_trust_prompt_func_t prompt_func,
void *prompt_baton,
apr_pool_t *pool);
void svn_auth_get_ssl_client_cert_prompt_provider
(svn_auth_provider_object_t **provider,
svn_auth_ssl_client_cert_prompt_func_t prompt_func,
void *prompt_baton,
int retry_limit,
apr_pool_t *pool);
void svn_auth_get_ssl_client_cert_pw_prompt_provider
(svn_auth_provider_object_t **provider,
svn_auth_ssl_client_cert_pw_prompt_func_t prompt_func,
void *prompt_baton,
int retry_limit,
apr_pool_t *pool);
#if defined(__cplusplus)
}
#endif
#endif