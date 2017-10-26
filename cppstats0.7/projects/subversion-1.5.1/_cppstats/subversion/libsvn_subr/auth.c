#include <apr_pools.h>
#include <apr_tables.h>
#include <apr_strings.h>
#include "svn_types.h"
#include "svn_error.h"
#include "svn_auth.h"
typedef struct {
apr_array_header_t *providers;
} provider_set_t;
struct svn_auth_baton_t {
apr_hash_t *tables;
apr_pool_t *pool;
apr_hash_t *parameters;
apr_hash_t *creds_cache;
};
struct svn_auth_iterstate_t {
provider_set_t *table;
int provider_idx;
svn_boolean_t got_first;
void *provider_iter_baton;
const char *realmstring;
const char *cache_key;
svn_auth_baton_t *auth_baton;
};
void
svn_auth_open(svn_auth_baton_t **auth_baton,
apr_array_header_t *providers,
apr_pool_t *pool) {
svn_auth_baton_t *ab;
svn_auth_provider_object_t *provider;
int i;
ab = apr_pcalloc(pool, sizeof(*ab));
ab->tables = apr_hash_make(pool);
ab->parameters = apr_hash_make(pool);
ab->creds_cache = apr_hash_make(pool);
ab->pool = pool;
for (i = 0; i < providers->nelts; i++) {
provider_set_t *table;
provider = APR_ARRAY_IDX(providers, i, svn_auth_provider_object_t *);
table = apr_hash_get(ab->tables,
provider->vtable->cred_kind, APR_HASH_KEY_STRING);
if (! table) {
table = apr_pcalloc(pool, sizeof(*table));
table->providers
= apr_array_make(pool, 1, sizeof(svn_auth_provider_object_t *));
apr_hash_set(ab->tables,
provider->vtable->cred_kind, APR_HASH_KEY_STRING,
table);
}
APR_ARRAY_PUSH(table->providers, svn_auth_provider_object_t *)
= provider;
}
*auth_baton = ab;
}
void
svn_auth_set_parameter(svn_auth_baton_t *auth_baton,
const char *name,
const void *value) {
apr_hash_set(auth_baton->parameters, name, APR_HASH_KEY_STRING, value);
}
const void *
svn_auth_get_parameter(svn_auth_baton_t *auth_baton,
const char *name) {
return apr_hash_get(auth_baton->parameters, name, APR_HASH_KEY_STRING);
}
svn_error_t *
svn_auth_first_credentials(void **credentials,
svn_auth_iterstate_t **state,
const char *cred_kind,
const char *realmstring,
svn_auth_baton_t *auth_baton,
apr_pool_t *pool) {
int i = 0;
provider_set_t *table;
svn_auth_provider_object_t *provider = NULL;
void *creds = NULL;
void *iter_baton = NULL;
svn_boolean_t got_first = FALSE;
svn_auth_iterstate_t *iterstate;
const char *cache_key;
table = apr_hash_get(auth_baton->tables, cred_kind, APR_HASH_KEY_STRING);
if (! table)
return svn_error_createf(SVN_ERR_AUTHN_NO_PROVIDER, NULL,
"No provider registered for '%s' credentials",
cred_kind);
cache_key = apr_pstrcat(pool, cred_kind, ":", realmstring, NULL);
creds = apr_hash_get(auth_baton->creds_cache,
cache_key, APR_HASH_KEY_STRING);
if (creds) {
got_first = FALSE;
} else
{
for (i = 0; i < table->providers->nelts; i++) {
provider = APR_ARRAY_IDX(table->providers, i,
svn_auth_provider_object_t *);
SVN_ERR(provider->vtable->first_credentials
(&creds, &iter_baton, provider->provider_baton,
auth_baton->parameters, realmstring, auth_baton->pool));
if (creds != NULL) {
got_first = TRUE;
break;
}
}
}
if (! creds)
*state = NULL;
else {
iterstate = apr_pcalloc(pool, sizeof(*iterstate));
iterstate->table = table;
iterstate->provider_idx = i;
iterstate->got_first = got_first;
iterstate->provider_iter_baton = iter_baton;
iterstate->realmstring = apr_pstrdup(pool, realmstring);
iterstate->cache_key = cache_key;
iterstate->auth_baton = auth_baton;
*state = iterstate;
apr_hash_set(auth_baton->creds_cache,
apr_pstrdup(auth_baton->pool, cache_key),
APR_HASH_KEY_STRING,
creds);
}
*credentials = creds;
return SVN_NO_ERROR;
}
svn_error_t *
svn_auth_next_credentials(void **credentials,
svn_auth_iterstate_t *state,
apr_pool_t *pool) {
svn_auth_baton_t *auth_baton = state->auth_baton;
svn_auth_provider_object_t *provider;
provider_set_t *table = state->table;
void *creds = NULL;
for (;
state->provider_idx < table->providers->nelts;
state->provider_idx++) {
provider = APR_ARRAY_IDX(table->providers,
state->provider_idx,
svn_auth_provider_object_t *);
if (! state->got_first) {
SVN_ERR(provider->vtable->first_credentials
(&creds, &(state->provider_iter_baton),
provider->provider_baton, auth_baton->parameters,
state->realmstring, auth_baton->pool));
state->got_first = TRUE;
} else {
if (provider->vtable->next_credentials)
SVN_ERR(provider->vtable->next_credentials
(&creds, state->provider_iter_baton,
provider->provider_baton, auth_baton->parameters,
state->realmstring, auth_baton->pool));
}
if (creds != NULL) {
apr_hash_set(auth_baton->creds_cache,
state->cache_key, APR_HASH_KEY_STRING,
creds);
break;
}
state->got_first = FALSE;
}
*credentials = creds;
return SVN_NO_ERROR;
}
svn_error_t *
svn_auth_save_credentials(svn_auth_iterstate_t *state,
apr_pool_t *pool) {
int i;
svn_auth_provider_object_t *provider;
svn_boolean_t save_succeeded = FALSE;
const char *no_auth_cache;
svn_auth_baton_t *auth_baton;
void *creds;
if (! state || state->table->providers->nelts <= state->provider_idx)
return SVN_NO_ERROR;
auth_baton = state->auth_baton;
creds = apr_hash_get(state->auth_baton->creds_cache,
state->cache_key, APR_HASH_KEY_STRING);
if (! creds)
return SVN_NO_ERROR;
no_auth_cache = apr_hash_get(auth_baton->parameters,
SVN_AUTH_PARAM_NO_AUTH_CACHE,
APR_HASH_KEY_STRING);
if (no_auth_cache)
return SVN_NO_ERROR;
provider = APR_ARRAY_IDX(state->table->providers,
state->provider_idx,
svn_auth_provider_object_t *);
if (provider->vtable->save_credentials)
SVN_ERR(provider->vtable->save_credentials(&save_succeeded,
creds,
provider->provider_baton,
auth_baton->parameters,
state->realmstring,
pool));
if (save_succeeded)
return SVN_NO_ERROR;
for (i = 0; i < state->table->providers->nelts; i++) {
provider = APR_ARRAY_IDX(state->table->providers, i,
svn_auth_provider_object_t *);
if (provider->vtable->save_credentials)
SVN_ERR(provider->vtable->save_credentials
(&save_succeeded, creds,
provider->provider_baton, auth_baton->parameters,
state->realmstring, pool));
if (save_succeeded)
break;
}
return SVN_NO_ERROR;
}
svn_auth_ssl_server_cert_info_t *
svn_auth_ssl_server_cert_info_dup
(const svn_auth_ssl_server_cert_info_t *info, apr_pool_t *pool) {
svn_auth_ssl_server_cert_info_t *new_info
= apr_palloc(pool, sizeof(*new_info));
*new_info = *info;
new_info->hostname = apr_pstrdup(pool, new_info->hostname);
new_info->fingerprint = apr_pstrdup(pool, new_info->fingerprint);
new_info->valid_from = apr_pstrdup(pool, new_info->valid_from);
new_info->valid_until = apr_pstrdup(pool, new_info->valid_until);
new_info->issuer_dname = apr_pstrdup(pool, new_info->issuer_dname);
new_info->ascii_cert = apr_pstrdup(pool, new_info->ascii_cert);
return new_info;
}
