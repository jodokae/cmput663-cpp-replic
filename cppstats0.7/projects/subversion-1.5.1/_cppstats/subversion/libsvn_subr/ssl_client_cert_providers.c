#include <apr_pools.h>
#include "svn_auth.h"
#include "svn_error.h"
#include "svn_config.h"
static svn_error_t *
ssl_client_cert_file_first_credentials(void **credentials_p,
void **iter_baton,
void *provider_baton,
apr_hash_t *parameters,
const char *realmstring,
apr_pool_t *pool) {
svn_config_t *cfg = apr_hash_get(parameters,
SVN_AUTH_PARAM_CONFIG,
APR_HASH_KEY_STRING);
const char *server_group = apr_hash_get(parameters,
SVN_AUTH_PARAM_SERVER_GROUP,
APR_HASH_KEY_STRING);
const char *cert_file;
cert_file =
svn_config_get_server_setting(cfg, server_group,
SVN_CONFIG_OPTION_SSL_CLIENT_CERT_FILE,
NULL);
if (cert_file != NULL) {
svn_auth_cred_ssl_client_cert_t *cred =
apr_palloc(pool, sizeof(*cred));
cred->cert_file = cert_file;
cred->may_save = FALSE;
*credentials_p = cred;
} else {
*credentials_p = NULL;
}
*iter_baton = NULL;
return SVN_NO_ERROR;
}
static const svn_auth_provider_t ssl_client_cert_file_provider = {
SVN_AUTH_CRED_SSL_CLIENT_CERT,
ssl_client_cert_file_first_credentials,
NULL,
NULL
};
void svn_auth_get_ssl_client_cert_file_provider
(svn_auth_provider_object_t **provider, apr_pool_t *pool) {
svn_auth_provider_object_t *po = apr_pcalloc(pool, sizeof(*po));
po->vtable = &ssl_client_cert_file_provider;
*provider = po;
}
typedef struct {
svn_auth_ssl_client_cert_prompt_func_t prompt_func;
void *prompt_baton;
int retry_limit;
} ssl_client_cert_prompt_provider_baton_t;
typedef struct {
ssl_client_cert_prompt_provider_baton_t *pb;
const char *realmstring;
int retries;
} ssl_client_cert_prompt_iter_baton_t;
static svn_error_t *
ssl_client_cert_prompt_first_cred(void **credentials_p,
void **iter_baton,
void *provider_baton,
apr_hash_t *parameters,
const char *realmstring,
apr_pool_t *pool) {
ssl_client_cert_prompt_provider_baton_t *pb = provider_baton;
ssl_client_cert_prompt_iter_baton_t *ib =
apr_pcalloc(pool, sizeof(*ib));
const char *no_auth_cache = apr_hash_get(parameters,
SVN_AUTH_PARAM_NO_AUTH_CACHE,
APR_HASH_KEY_STRING);
SVN_ERR(pb->prompt_func((svn_auth_cred_ssl_client_cert_t **) credentials_p,
pb->prompt_baton, realmstring, ! no_auth_cache,
pool));
ib->pb = pb;
ib->realmstring = apr_pstrdup(pool, realmstring);
ib->retries = 0;
*iter_baton = ib;
return SVN_NO_ERROR;
}
static svn_error_t *
ssl_client_cert_prompt_next_cred(void **credentials_p,
void *iter_baton,
void *provider_baton,
apr_hash_t *parameters,
const char *realmstring,
apr_pool_t *pool) {
ssl_client_cert_prompt_iter_baton_t *ib = iter_baton;
const char *no_auth_cache = apr_hash_get(parameters,
SVN_AUTH_PARAM_NO_AUTH_CACHE,
APR_HASH_KEY_STRING);
if (ib->retries >= ib->pb->retry_limit) {
*credentials_p = NULL;
return SVN_NO_ERROR;
}
ib->retries++;
SVN_ERR(ib->pb->prompt_func((svn_auth_cred_ssl_client_cert_t **)
credentials_p, ib->pb->prompt_baton,
ib->realmstring, ! no_auth_cache, pool));
return SVN_NO_ERROR;
}
static const svn_auth_provider_t ssl_client_cert_prompt_provider = {
SVN_AUTH_CRED_SSL_CLIENT_CERT,
ssl_client_cert_prompt_first_cred,
ssl_client_cert_prompt_next_cred,
NULL
};
void svn_auth_get_ssl_client_cert_prompt_provider
(svn_auth_provider_object_t **provider,
svn_auth_ssl_client_cert_prompt_func_t prompt_func,
void *prompt_baton,
int retry_limit,
apr_pool_t *pool) {
svn_auth_provider_object_t *po = apr_pcalloc(pool, sizeof(*po));
ssl_client_cert_prompt_provider_baton_t *pb = apr_palloc(pool, sizeof(*pb));
pb->prompt_func = prompt_func;
pb->prompt_baton = prompt_baton;
pb->retry_limit = retry_limit;
po->vtable = &ssl_client_cert_prompt_provider;
po->provider_baton = pb;
*provider = po;
}