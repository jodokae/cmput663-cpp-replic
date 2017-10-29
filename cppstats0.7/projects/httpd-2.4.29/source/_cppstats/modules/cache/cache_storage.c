#include "mod_cache.h"
#include "cache_storage.h"
#include "cache_util.h"
APLOG_USE_MODULE(cache);
extern APR_OPTIONAL_FN_TYPE(ap_cache_generate_key) *cache_generate_key;
extern module AP_MODULE_DECLARE_DATA cache_module;
int cache_remove_url(cache_request_rec *cache, request_rec *r) {
cache_provider_list *list;
cache_handle_t *h;
list = cache->providers;
h = cache->stale_handle ? cache->stale_handle : cache->handle;
if (!h) {
return OK;
}
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(00691)
"cache: Removing url %s from the cache", h->cache_obj->key);
while (list) {
list->provider->remove_url(h, r);
list = list->next;
}
return OK;
}
int cache_create_entity(cache_request_rec *cache, request_rec *r,
apr_off_t size, apr_bucket_brigade *in) {
cache_provider_list *list;
cache_handle_t *h = apr_pcalloc(r->pool, sizeof(cache_handle_t));
apr_status_t rv;
if (!cache) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, APR_EGENERAL, r, APLOGNO(00692)
"cache: No cache request information available for key"
" generation");
return APR_EGENERAL;
}
if (!cache->key) {
rv = cache_generate_key(r, r->pool, &cache->key);
if (rv != APR_SUCCESS) {
return rv;
}
}
list = cache->providers;
while (list) {
switch (rv = list->provider->create_entity(h, r, cache->key, size, in)) {
case OK: {
cache->handle = h;
cache->provider = list->provider;
cache->provider_name = list->provider_name;
return OK;
}
case DECLINED: {
list = list->next;
continue;
}
default: {
return rv;
}
}
}
return DECLINED;
}
static int filter_header_do(void *v, const char *key, const char *val) {
if ((*key == 'W' || *key == 'w') && !ap_cstr_casecmp(key, "Warning")
&& *val == '1') {
} else {
apr_table_addn(v, key, val);
}
return 1;
}
static int remove_header_do(void *v, const char *key, const char *val) {
if ((*key == 'W' || *key == 'w') && !ap_cstr_casecmp(key, "Warning")) {
} else {
apr_table_unset(v, key);
}
return 1;
}
static int add_header_do(void *v, const char *key, const char *val) {
apr_table_addn(v, key, val);
return 1;
}
void cache_accept_headers(cache_handle_t *h, request_rec *r, apr_table_t *top,
apr_table_t *bottom, int revalidation) {
const char *v;
if (revalidation) {
r->headers_out = apr_table_make(r->pool, 10);
apr_table_do(filter_header_do, r->headers_out, bottom, NULL);
} else if (r->headers_out != bottom) {
r->headers_out = apr_table_copy(r->pool, bottom);
}
apr_table_do(remove_header_do, r->headers_out, top, NULL);
apr_table_do(add_header_do, r->headers_out, top, NULL);
v = apr_table_get(r->headers_out, "Content-Type");
if (v) {
ap_set_content_type(r, v);
apr_table_unset(r->headers_out, "Content-Type");
apr_table_unset(r->err_headers_out, "Content-Type");
}
v = apr_table_get(r->headers_out, "Last-Modified");
if (v) {
ap_update_mtime(r, apr_date_parse_http(v));
ap_set_last_modified(r);
}
}
int cache_select(cache_request_rec *cache, request_rec *r) {
cache_provider_list *list;
apr_status_t rv;
cache_handle_t *h;
if (!cache) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, APR_EGENERAL, r, APLOGNO(00693)
"cache: No cache request information available for key"
" generation");
return DECLINED;
}
if (!ap_cache_check_no_cache(cache, r)) {
return DECLINED;
}
if (!cache->key) {
rv = cache_generate_key(r, r->pool, &cache->key);
if (rv != APR_SUCCESS) {
return DECLINED;
}
}
h = apr_palloc(r->pool, sizeof(cache_handle_t));
list = cache->providers;
while (list) {
switch ((rv = list->provider->open_entity(h, r, cache->key))) {
case OK: {
char *vary = NULL;
int mismatch = 0;
char *last = NULL;
if (list->provider->recall_headers(h, r) != APR_SUCCESS) {
list = list->next;
continue;
}
vary = cache_strqtok(
apr_pstrdup(r->pool,
cache_table_getm(r->pool, h->resp_hdrs, "Vary")),
CACHE_SEPARATOR, &last);
while (vary) {
const char *h1, *h2;
h1 = cache_table_getm(r->pool, r->headers_in, vary);
h2 = cache_table_getm(r->pool, h->req_hdrs, vary);
if (h1 == h2) {
} else if (h1 && h2 && !strcmp(h1, h2)) {
} else {
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, APR_SUCCESS,
r, APLOGNO(00694) "cache_select(): Vary header mismatch.");
mismatch = 1;
break;
}
vary = cache_strqtok(NULL, CACHE_SEPARATOR, &last);
}
if (mismatch) {
list = list->next;
continue;
}
cache->provider = list->provider;
cache->provider_name = list->provider_name;
if (ap_condition_if_match(r, h->resp_hdrs) == AP_CONDITION_NOMATCH
|| ap_condition_if_unmodified_since(r, h->resp_hdrs)
== AP_CONDITION_NOMATCH
|| ap_condition_if_none_match(r, h->resp_hdrs)
== AP_CONDITION_NOMATCH
|| ap_condition_if_modified_since(r, h->resp_hdrs)
== AP_CONDITION_NOMATCH
|| ap_condition_if_range(r, h->resp_hdrs) == AP_CONDITION_NOMATCH) {
mismatch = 1;
}
if (mismatch || !cache_check_freshness(h, cache, r)) {
const char *etag, *lastmod;
if (cache->control_in.only_if_cached) {
list = list->next;
continue;
}
cache->stale_headers = apr_table_copy(r->pool,
r->headers_in);
cache->stale_handle = h;
if (!mismatch) {
ap_log_rerror(
APLOG_MARK, APLOG_DEBUG, APR_SUCCESS, r, APLOGNO(00695) "Cached response for %s isn't fresh. Adding "
"conditional request headers.", r->uri);
apr_table_unset(r->headers_in, "If-Match");
apr_table_unset(r->headers_in, "If-Modified-Since");
apr_table_unset(r->headers_in, "If-None-Match");
apr_table_unset(r->headers_in, "If-Range");
apr_table_unset(r->headers_in, "If-Unmodified-Since");
etag = apr_table_get(h->resp_hdrs, "ETag");
lastmod = apr_table_get(h->resp_hdrs, "Last-Modified");
if (etag || lastmod) {
if (etag) {
apr_table_set(r->headers_in, "If-None-Match", etag);
}
if (lastmod) {
apr_table_set(r->headers_in, "If-Modified-Since",
lastmod);
}
apr_table_unset(r->headers_in, "Range");
}
}
return DECLINED;
}
cache_accept_headers(h, r, h->resp_hdrs, r->headers_out, 0);
cache->handle = h;
return OK;
}
case DECLINED: {
list = list->next;
continue;
}
default: {
return rv;
}
}
}
if (cache->control_in.only_if_cached) {
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, APR_SUCCESS, r, APLOGNO(00696)
"cache: 'only-if-cached' requested and no cached entity, "
"returning 504 Gateway Timeout for: %s", r->uri);
return HTTP_GATEWAY_TIME_OUT;
}
return DECLINED;
}
static apr_status_t cache_canonicalise_key(request_rec *r, apr_pool_t* p,
const char *path, const char *query,
apr_uri_t *parsed_uri,
const char **key) {
cache_server_conf *conf;
char *port_str, *hn, *lcs;
const char *hostname, *scheme;
int i;
const char *kpath;
const char *kquery;
if (*key) {
return APR_SUCCESS;
}
conf = (cache_server_conf *) ap_get_module_config(r->server->module_config,
&cache_module);
if (!r->proxyreq || (r->proxyreq == PROXYREQ_REVERSE)) {
if (conf->base_uri && conf->base_uri->hostname) {
hostname = conf->base_uri->hostname;
} else {
hostname = ap_get_server_name(r);
if (!hostname) {
hostname = "_default_";
}
}
} else if (parsed_uri->hostname) {
hn = apr_pstrdup(p, parsed_uri->hostname);
ap_str_tolower(hn);
hostname = hn;
} else {
hostname = "_default_";
}
if (r->proxyreq && parsed_uri->scheme) {
lcs = apr_pstrdup(p, parsed_uri->scheme);
ap_str_tolower(lcs);
scheme = lcs;
} else {
if (conf->base_uri && conf->base_uri->scheme) {
scheme = conf->base_uri->scheme;
} else {
scheme = ap_http_scheme(r);
}
}
if (r->proxyreq && (r->proxyreq != PROXYREQ_REVERSE)) {
if (parsed_uri->port_str) {
port_str = apr_pcalloc(p, strlen(parsed_uri->port_str) + 2);
port_str[0] = ':';
for (i = 0; parsed_uri->port_str[i]; i++) {
port_str[i + 1] = apr_tolower(parsed_uri->port_str[i]);
}
} else if (apr_uri_port_of_scheme(scheme)) {
port_str = apr_psprintf(p, ":%u", apr_uri_port_of_scheme(scheme));
} else {
port_str = "";
}
} else {
if (conf->base_uri && conf->base_uri->port_str) {
port_str = conf->base_uri->port_str;
} else if (conf->base_uri && conf->base_uri->hostname) {
port_str = "";
} else {
port_str = apr_psprintf(p, ":%u", ap_get_server_port(r));
}
}
kpath = path;
kquery = conf->ignorequerystring ? NULL : query;
if (conf->ignore_session_id->nelts) {
int i;
char **identifier;
identifier = (char **) conf->ignore_session_id->elts;
for (i = 0; i < conf->ignore_session_id->nelts; i++, identifier++) {
int len;
const char *param;
len = strlen(*identifier);
if ((param = ap_strrchr_c(kpath, ';'))
&& !strncmp(param + 1, *identifier, len)
&& (*(param + len + 1) == '=')
&& !ap_strchr_c(param + len + 2, '/')) {
kpath = apr_pstrmemdup(p, kpath, param - kpath);
continue;
}
if (kquery && *kquery) {
if (!strncmp(kquery, *identifier, len) && kquery[len] == '=') {
param = kquery;
} else {
char *complete;
complete = apr_pstrcat(p, "&", *identifier, "=", NULL);
param = ap_strstr_c(kquery, complete);
if (param) {
param++;
}
}
if (param) {
const char *amp;
char *dup = NULL;
if (kquery != param) {
dup = apr_pstrmemdup(p, kquery, param - kquery);
kquery = dup;
} else {
kquery = "";
}
if ((amp = ap_strchr_c(param + len + 1, '&'))) {
kquery = apr_pstrcat(p, kquery, amp + 1, NULL);
} else {
if (dup) {
dup[strlen(dup) - 1] = '\0';
}
}
}
}
}
}
*key = apr_pstrcat(p, scheme, "://", hostname, port_str,
kpath, "?", kquery, NULL);
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, APR_SUCCESS, r, APLOGNO(00698)
"cache: Key for entity %s?%s is %s", path, query, *key);
return APR_SUCCESS;
}
apr_status_t cache_generate_key_default(request_rec *r, apr_pool_t* p,
const char **key) {
const char *path = r->uri;
const char *query = r->args;
if (cache_use_early_url(r)) {
path = r->parsed_uri.path;
query = r->parsed_uri.query;
}
return cache_canonicalise_key(r, p, path, query, &r->parsed_uri, key);
}
int cache_invalidate(cache_request_rec *cache, request_rec *r) {
cache_provider_list *list;
apr_status_t rv, status = DECLINED;
cache_handle_t *h;
apr_uri_t location_uri;
apr_uri_t content_location_uri;
const char *location, *location_key = NULL;
const char *content_location, *content_location_key = NULL;
if (!cache) {
ap_log_rerror(
APLOG_MARK, APLOG_ERR, APR_EGENERAL, r, APLOGNO(00697) "cache: No cache request information available for key"
" generation");
return DECLINED;
}
if (!cache->key) {
rv = cache_generate_key(r, r->pool, &cache->key);
if (rv != APR_SUCCESS) {
return DECLINED;
}
}
location = apr_table_get(r->headers_out, "Location");
if (location) {
if (apr_uri_parse(r->pool, location, &location_uri)
|| cache_canonicalise_key(r, r->pool,
location_uri.path,
location_uri.query,
&location_uri, &location_key)
|| !(r->parsed_uri.hostname
&& location_uri.hostname
&& !strcmp(r->parsed_uri.hostname,
location_uri.hostname))) {
location_key = NULL;
}
}
content_location = apr_table_get(r->headers_out, "Content-Location");
if (content_location) {
if (apr_uri_parse(r->pool, content_location,
&content_location_uri)
|| cache_canonicalise_key(r, r->pool,
content_location_uri.path,
content_location_uri.query,
&content_location_uri,
&content_location_key)
|| !(r->parsed_uri.hostname
&& content_location_uri.hostname
&& !strcmp(r->parsed_uri.hostname,
content_location_uri.hostname))) {
content_location_key = NULL;
}
}
h = apr_palloc(r->pool, sizeof(cache_handle_t));
list = cache->providers;
while (list) {
rv = list->provider->open_entity(h, r, cache->key);
if (OK == rv) {
rv = list->provider->invalidate_entity(h, r);
status = OK;
}
ap_log_rerror(
APLOG_MARK, APLOG_DEBUG, rv, r, APLOGNO(02468) "cache: Attempted to invalidate cached entity with key: %s", cache->key);
if (location_key) {
rv = list->provider->open_entity(h, r, location_key);
if (OK == rv) {
rv = list->provider->invalidate_entity(h, r);
status = OK;
}
ap_log_rerror(
APLOG_MARK, APLOG_DEBUG, rv, r, APLOGNO(02469) "cache: Attempted to invalidate cached entity with key: %s", location_key);
}
if (content_location_key) {
rv = list->provider->open_entity(h, r, content_location_key);
if (OK == rv) {
rv = list->provider->invalidate_entity(h, r);
status = OK;
}
ap_log_rerror(
APLOG_MARK, APLOG_DEBUG, rv, r, APLOGNO(02470) "cache: Attempted to invalidate cached entity with key: %s", content_location_key);
}
list = list->next;
}
return status;
}