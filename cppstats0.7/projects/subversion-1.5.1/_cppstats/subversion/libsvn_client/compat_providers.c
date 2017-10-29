#include "svn_auth.h"
#include "svn_client.h"
void
svn_client_get_simple_prompt_provider
(svn_auth_provider_object_t **provider,
svn_auth_simple_prompt_func_t prompt_func,
void *prompt_baton,
int retry_limit,
apr_pool_t *pool) {
svn_auth_get_simple_prompt_provider(provider, prompt_func, prompt_baton,
retry_limit, pool);
}
void
svn_client_get_username_prompt_provider
(svn_auth_provider_object_t **provider,
svn_auth_username_prompt_func_t prompt_func,
void *prompt_baton,
int retry_limit,
apr_pool_t *pool) {
svn_auth_get_username_prompt_provider(provider, prompt_func, prompt_baton,
retry_limit, pool);
}
void svn_client_get_simple_provider(svn_auth_provider_object_t **provider,
apr_pool_t *pool) {
svn_auth_get_simple_provider(provider, pool);
}
#if defined(WIN32) && !defined(__MINGW32__)
void
svn_client_get_windows_simple_provider(svn_auth_provider_object_t **provider,
apr_pool_t *pool) {
svn_auth_get_windows_simple_provider(provider, pool);
}
#endif
void svn_client_get_username_provider(svn_auth_provider_object_t **provider,
apr_pool_t *pool) {
svn_auth_get_username_provider(provider, pool);
}
void
svn_client_get_ssl_server_trust_file_provider
(svn_auth_provider_object_t **provider, apr_pool_t *pool) {
svn_auth_get_ssl_server_trust_file_provider(provider, pool);
}
void
svn_client_get_ssl_client_cert_file_provider
(svn_auth_provider_object_t **provider, apr_pool_t *pool) {
svn_auth_get_ssl_client_cert_file_provider(provider, pool);
}
void
svn_client_get_ssl_client_cert_pw_file_provider
(svn_auth_provider_object_t **provider, apr_pool_t *pool) {
svn_auth_get_ssl_client_cert_pw_file_provider(provider, pool);
}
void
svn_client_get_ssl_server_trust_prompt_provider
(svn_auth_provider_object_t **provider,
svn_auth_ssl_server_trust_prompt_func_t prompt_func,
void *prompt_baton,
apr_pool_t *pool) {
svn_auth_get_ssl_server_trust_prompt_provider(provider, prompt_func,
prompt_baton, pool);
}
void
svn_client_get_ssl_client_cert_prompt_provider
(svn_auth_provider_object_t **provider,
svn_auth_ssl_client_cert_prompt_func_t prompt_func,
void *prompt_baton,
int retry_limit,
apr_pool_t *pool) {
svn_auth_get_ssl_client_cert_prompt_provider(provider, prompt_func,
prompt_baton, retry_limit,
pool);
}
void
svn_client_get_ssl_client_cert_pw_prompt_provider
(svn_auth_provider_object_t **provider,
svn_auth_ssl_client_cert_pw_prompt_func_t prompt_func,
void *prompt_baton,
int retry_limit,
apr_pool_t *pool) {
svn_auth_get_ssl_client_cert_pw_prompt_provider(provider, prompt_func,
prompt_baton, retry_limit,
pool);
}