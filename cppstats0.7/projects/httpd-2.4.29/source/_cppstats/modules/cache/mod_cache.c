#include "mod_cache.h"
#include "cache_storage.h"
#include "cache_util.h"
module AP_MODULE_DECLARE_DATA cache_module;
APR_OPTIONAL_FN_TYPE(ap_cache_generate_key) *cache_generate_key;
static ap_filter_rec_t *cache_filter_handle;
static ap_filter_rec_t *cache_save_filter_handle;
static ap_filter_rec_t *cache_save_subreq_filter_handle;
static ap_filter_rec_t *cache_out_filter_handle;
static ap_filter_rec_t *cache_out_subreq_filter_handle;
static ap_filter_rec_t *cache_remove_url_filter_handle;
static ap_filter_rec_t *cache_invalidate_filter_handle;
static const char *MOD_CACHE_ENTITY_HEADERS[] = {
"Allow",
"Content-Encoding",
"Content-Language",
"Content-Length",
"Content-Location",
"Content-MD5",
"Content-Range",
"Content-Type",
"Last-Modified",
NULL
};
static int cache_quick_handler(request_rec *r, int lookup) {
apr_status_t rv;
const char *auth;
cache_provider_list *providers;
cache_request_rec *cache;
apr_bucket_brigade *out;
apr_bucket *e;
ap_filter_t *next;
ap_filter_rec_t *cache_out_handle;
cache_server_conf *conf;
conf = (cache_server_conf *) ap_get_module_config(r->server->module_config,
&cache_module);
if (!conf->quick) {
return DECLINED;
}
if (!(providers = cache_get_providers(r, conf))) {
return DECLINED;
}
cache = apr_pcalloc(r->pool, sizeof(cache_request_rec));
cache->size = -1;
cache->out = apr_brigade_create(r->pool, r->connection->bucket_alloc);
cache->providers = providers;
if (!ap_cache_check_no_store(cache, r)) {
return DECLINED;
}
auth = apr_table_get(r->headers_in, "Authorization");
if (auth) {
return DECLINED;
}
switch (r->method_number) {
case M_PUT:
case M_POST:
case M_DELETE: {
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, APR_SUCCESS, r, APLOGNO(02461)
"PUT/POST/DELETE: Adding CACHE_INVALIDATE filter for %s",
r->uri);
ap_add_output_filter_handle(
cache_invalidate_filter_handle, cache, r,
r->connection);
return DECLINED;
}
case M_GET: {
break;
}
default : {
ap_log_rerror(
APLOG_MARK, APLOG_DEBUG, APR_SUCCESS, r, APLOGNO(02462) "cache: Method '%s' not cacheable by mod_cache, ignoring: %s", r->method, r->uri);
return DECLINED;
}
}
rv = cache_select(cache, r);
if (rv != OK) {
if (rv == DECLINED) {
if (!lookup) {
rv = cache_try_lock(conf, cache, r);
if (APR_SUCCESS == rv) {
if (r->main) {
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, APR_SUCCESS,
r, APLOGNO(00749) "Adding CACHE_SAVE_SUBREQ filter for %s",
r->uri);
cache->save_filter = ap_add_output_filter_handle(
cache_save_subreq_filter_handle, cache, r,
r->connection);
} else {
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, APR_SUCCESS,
r, APLOGNO(00750) "Adding CACHE_SAVE filter for %s",
r->uri);
cache->save_filter = ap_add_output_filter_handle(
cache_save_filter_handle, cache, r,
r->connection);
}
apr_pool_userdata_setn(cache, CACHE_CTX_KEY, NULL, r->pool);
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, APR_SUCCESS, r, APLOGNO(00751)
"Adding CACHE_REMOVE_URL filter for %s",
r->uri);
cache->remove_url_filter = ap_add_output_filter_handle(
cache_remove_url_filter_handle, cache, r,
r->connection);
} else {
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, rv,
r, APLOGNO(00752) "Cache locked for url, not caching "
"response: %s", r->uri);
if (cache->stale_headers) {
r->headers_in = cache->stale_headers;
}
}
} else {
if (cache->stale_headers) {
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, APR_SUCCESS,
r, APLOGNO(00753) "Restoring request headers for %s",
r->uri);
r->headers_in = cache->stale_headers;
}
}
} else {
return rv;
}
return DECLINED;
}
cache_run_cache_status(cache->handle, r, r->headers_out, AP_CACHE_HIT,
"cache hit");
if (lookup) {
if (cache->stale_headers) {
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, APR_SUCCESS, r, APLOGNO(00754)
"Restoring request headers.");
r->headers_in = cache->stale_headers;
}
}
rv = ap_meets_conditions(r);
if (rv != OK) {
if (lookup) {
return DECLINED;
}
return rv;
}
if (lookup) {
return OK;
}
ap_run_insert_filter(r);
if (r->main) {
cache_out_handle = cache_out_subreq_filter_handle;
} else {
cache_out_handle = cache_out_filter_handle;
}
ap_add_output_filter_handle(cache_out_handle, cache, r, r->connection);
next = r->output_filters;
while (next && (next->frec != cache_out_handle)) {
ap_remove_output_filter(next);
next = next->next;
}
out = apr_brigade_create(r->pool, r->connection->bucket_alloc);
e = apr_bucket_eos_create(out->bucket_alloc);
APR_BRIGADE_INSERT_TAIL(out, e);
return ap_pass_brigade_fchk(r, out,
"cache_quick_handler(%s): ap_pass_brigade returned",
cache->provider_name);
}
static int cache_replace_filter(ap_filter_t *next, ap_filter_rec_t *from,
ap_filter_rec_t *to, ap_filter_rec_t *stop) {
ap_filter_t *ffrom = NULL, *fto = NULL;
while (next && next->frec != stop) {
if (next->frec == from) {
ffrom = next;
}
if (next->frec == to) {
fto = next;
}
next = next->next;
}
if (ffrom && fto) {
ffrom->frec = fto->frec;
ffrom->ctx = fto->ctx;
ap_remove_output_filter(fto);
return 1;
}
if (ffrom) {
ap_remove_output_filter(ffrom);
}
return 0;
}
static ap_filter_t *cache_get_filter(ap_filter_t *next, ap_filter_rec_t *rec) {
while (next) {
if (next->frec == rec && next->ctx) {
break;
}
next = next->next;
}
return next;
}
static int cache_handler(request_rec *r) {
apr_status_t rv;
cache_provider_list *providers;
cache_request_rec *cache;
apr_bucket_brigade *out;
apr_bucket *e;
ap_filter_t *next;
ap_filter_rec_t *cache_out_handle;
ap_filter_rec_t *cache_save_handle;
cache_server_conf *conf;
conf = (cache_server_conf *) ap_get_module_config(r->server->module_config,
&cache_module);
if (conf->quick) {
return DECLINED;
}
if (!(providers = cache_get_providers(r, conf))) {
return DECLINED;
}
cache = apr_pcalloc(r->pool, sizeof(cache_request_rec));
cache->size = -1;
cache->out = apr_brigade_create(r->pool, r->connection->bucket_alloc);
cache->providers = providers;
if (!ap_cache_check_no_store(cache, r)) {
return DECLINED;
}
switch (r->method_number) {
case M_PUT:
case M_POST:
case M_DELETE: {
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, APR_SUCCESS, r, APLOGNO(02463)
"PUT/POST/DELETE: Adding CACHE_INVALIDATE filter for %s",
r->uri);
ap_add_output_filter_handle(
cache_invalidate_filter_handle, cache, r,
r->connection);
return DECLINED;
}
case M_GET: {
break;
}
default : {
ap_log_rerror(
APLOG_MARK, APLOG_DEBUG, APR_SUCCESS, r, APLOGNO(02464) "cache: Method '%s' not cacheable by mod_cache, ignoring: %s", r->method, r->uri);
return DECLINED;
}
}
rv = cache_select(cache, r);
if (rv != OK) {
if (rv == DECLINED) {
rv = cache_try_lock(conf, cache, r);
if (APR_SUCCESS == rv) {
if (r->main) {
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, APR_SUCCESS,
r, APLOGNO(00756) "Adding CACHE_SAVE_SUBREQ filter for %s",
r->uri);
cache_save_handle = cache_save_subreq_filter_handle;
} else {
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, APR_SUCCESS,
r, APLOGNO(00757) "Adding CACHE_SAVE filter for %s",
r->uri);
cache_save_handle = cache_save_filter_handle;
}
ap_add_output_filter_handle(cache_save_handle, cache, r,
r->connection);
if (cache_replace_filter(r->output_filters,
cache_filter_handle, cache_save_handle,
ap_get_input_filter_handle("SUBREQ_CORE"))) {
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, APR_SUCCESS,
r, APLOGNO(00758) "Replacing CACHE with CACHE_SAVE "
"filter for %s", r->uri);
}
cache->save_filter = cache_get_filter(r->output_filters,
cache_save_filter_handle);
apr_pool_userdata_setn(cache, CACHE_CTX_KEY, NULL, r->pool);
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, APR_SUCCESS, r, APLOGNO(00759)
"Adding CACHE_REMOVE_URL filter for %s",
r->uri);
cache->remove_url_filter
= ap_add_output_filter_handle(
cache_remove_url_filter_handle, cache, r,
r->connection);
} else {
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, rv,
r, APLOGNO(00760) "Cache locked for url, not caching "
"response: %s", r->uri);
}
} else {
return rv;
}
return DECLINED;
}
cache_run_cache_status(cache->handle, r, r->headers_out, AP_CACHE_HIT,
"cache hit");
rv = ap_meets_conditions(r);
if (rv != OK) {
return rv;
}
if (r->main) {
cache_out_handle = cache_out_subreq_filter_handle;
} else {
cache_out_handle = cache_out_filter_handle;
}
ap_add_output_filter_handle(cache_out_handle, cache, r, r->connection);
if (cache_replace_filter(r->output_filters, cache_filter_handle,
cache_out_handle, ap_get_input_filter_handle("SUBREQ_CORE"))) {
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, APR_SUCCESS,
r, APLOGNO(00761) "Replacing CACHE with CACHE_OUT filter for %s",
r->uri);
}
next = r->output_filters;
while (next && (next->frec != cache_out_handle)) {
ap_remove_output_filter(next);
next = next->next;
}
out = apr_brigade_create(r->pool, r->connection->bucket_alloc);
e = apr_bucket_eos_create(out->bucket_alloc);
APR_BRIGADE_INSERT_TAIL(out, e);
return ap_pass_brigade_fchk(r, out, "cache(%s): ap_pass_brigade returned",
cache->provider_name);
}
static apr_status_t cache_out_filter(ap_filter_t *f, apr_bucket_brigade *in) {
request_rec *r = f->r;
cache_request_rec *cache = (cache_request_rec *)f->ctx;
if (!cache) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(00762)
"CACHE/CACHE_OUT filter enabled while caching is disabled, ignoring");
ap_remove_output_filter(f);
return ap_pass_brigade(f->next, in);
}
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, APR_SUCCESS, r, APLOGNO(00763)
"cache: running CACHE_OUT filter");
while (!APR_BRIGADE_EMPTY(in)) {
apr_bucket *e = APR_BRIGADE_FIRST(in);
if (APR_BUCKET_IS_EOS(e)) {
apr_bucket_brigade *bb = apr_brigade_create(r->pool,
r->connection->bucket_alloc);
const char *ct = apr_table_get(cache->handle->resp_hdrs, "Content-Type");
if (ct) {
ap_set_content_type(r, ct);
}
r->status = cache->handle->cache_obj->info.status;
cache->provider->recall_body(cache->handle, r->pool, bb);
APR_BRIGADE_PREPEND(in, bb);
ap_remove_output_filter(f);
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, APR_SUCCESS, r, APLOGNO(00764)
"cache: serving %s", r->uri);
return ap_pass_brigade(f->next, in);
}
apr_bucket_delete(e);
}
return APR_SUCCESS;
}
static int cache_save_store(ap_filter_t *f, apr_bucket_brigade *in,
cache_server_conf *conf, cache_request_rec *cache) {
int rv = APR_SUCCESS;
apr_bucket *e;
while (APR_SUCCESS == rv && !APR_BRIGADE_EMPTY(in)) {
rv = cache->provider->store_body(cache->handle, f->r, in, cache->out);
if (rv != APR_SUCCESS) {
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, rv, f->r, APLOGNO(00765)
"cache: Cache provider's store_body failed for URI %s", f->r->uri);
ap_remove_output_filter(f);
cache_remove_lock(conf, cache, f->r, NULL);
APR_BRIGADE_PREPEND(in, cache->out);
return ap_pass_brigade(f->next, in);
}
for (e = APR_BRIGADE_FIRST(cache->out);
e != APR_BRIGADE_SENTINEL(cache->out);
e = APR_BUCKET_NEXT(e)) {
if (APR_BUCKET_IS_EOS(e)) {
rv = cache->provider->commit_entity(cache->handle, f->r);
break;
}
}
cache_remove_lock(conf, cache, f->r, cache->out);
if (APR_BRIGADE_EMPTY(cache->out)) {
if (APR_BRIGADE_EMPTY(in)) {
break;
} else {
ap_log_rerror(APLOG_MARK, APLOG_WARNING, rv, f->r, APLOGNO(00766)
"cache: Cache provider's store_body returned an "
"empty brigade, but didn't consume all of the "
"input brigade, standing down to prevent a spin");
ap_remove_output_filter(f);
cache_remove_lock(conf, cache, f->r, NULL);
return ap_pass_brigade(f->next, in);
}
}
rv = ap_pass_brigade(f->next, cache->out);
}
return rv;
}
static int cache_header_cmp(apr_pool_t *pool, apr_table_t *left,
apr_table_t *right, const char *key) {
const char *h1, *h2;
if ((h1 = cache_table_getm(pool, left, key))
&& (h2 = cache_table_getm(pool, right, key)) && (strcmp(h1, h2))) {
return 1;
}
return 0;
}
static apr_status_t cache_save_filter(ap_filter_t *f, apr_bucket_brigade *in) {
int rv = !OK;
request_rec *r = f->r;
cache_request_rec *cache = (cache_request_rec *)f->ctx;
cache_server_conf *conf;
cache_dir_conf *dconf;
cache_control_t control;
const char *cc_out, *cl, *pragma;
const char *exps, *lastmods, *dates, *etag;
apr_time_t exp, date, lastmod, now;
apr_off_t size = -1;
cache_info *info = NULL;
const char *reason, **eh;
apr_pool_t *p;
apr_bucket *e;
apr_table_t *headers;
const char *query;
conf = (cache_server_conf *) ap_get_module_config(r->server->module_config,
&cache_module);
if (!cache) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(00767)
"CACHE/CACHE_SAVE filter enabled while caching is disabled, ignoring");
ap_remove_output_filter(f);
return ap_pass_brigade(f->next, in);
}
reason = NULL;
p = r->pool;
if (cache->block_response) {
return APR_SUCCESS;
}
if (cache->in_checked) {
return cache_save_store(f, in, conf, cache);
}
dconf = ap_get_module_config(r->per_dir_config, &cache_module);
if (dconf->stale_on_error && r->status >= HTTP_INTERNAL_SERVER_ERROR) {
ap_remove_output_filter(cache->remove_url_filter);
if (cache->stale_handle
&& !cache->stale_handle->cache_obj->info.control.must_revalidate
&& !cache->stale_handle->cache_obj->info.control.proxy_revalidate) {
const char *warn_head;
cache->handle = cache->stale_handle;
if (r->main) {
f->frec = cache_out_subreq_filter_handle;
} else {
f->frec = cache_out_filter_handle;
}
r->headers_out = cache->stale_handle->resp_hdrs;
ap_set_content_type(r, apr_table_get(
cache->stale_handle->resp_hdrs, "Content-Type"));
warn_head = apr_table_get(r->err_headers_out, "Warning");
if ((warn_head == NULL) || ((warn_head != NULL)
&& (ap_strstr_c(warn_head, "111") == NULL))) {
apr_table_mergen(r->err_headers_out, "Warning",
"111 Revalidation failed");
}
cache_run_cache_status(cache->handle, r, r->headers_out, AP_CACHE_HIT,
apr_psprintf(r->pool,
"cache hit: %d status; stale content returned",
r->status));
cache_remove_lock(conf, cache, f->r, NULL);
return ap_pass_brigade(f, in);
}
}
query = cache_use_early_url(r) ? r->parsed_uri.query : r->args;
exps = apr_table_get(r->err_headers_out, "Expires");
if (exps == NULL) {
exps = apr_table_get(r->headers_out, "Expires");
}
if (exps != NULL) {
exp = apr_date_parse_http(exps);
} else {
exp = APR_DATE_BAD;
}
lastmods = apr_table_get(r->err_headers_out, "Last-Modified");
if (lastmods == NULL) {
lastmods = apr_table_get(r->headers_out, "Last-Modified");
}
if (lastmods != NULL) {
lastmod = apr_date_parse_http(lastmods);
if (lastmod == APR_DATE_BAD) {
lastmods = NULL;
}
} else {
lastmod = APR_DATE_BAD;
}
etag = apr_table_get(r->err_headers_out, "Etag");
if (etag == NULL) {
etag = apr_table_get(r->headers_out, "Etag");
}
cc_out = cache_table_getm(r->pool, r->err_headers_out, "Cache-Control");
pragma = cache_table_getm(r->pool, r->err_headers_out, "Pragma");
headers = r->err_headers_out;
if (!cc_out && !pragma) {
cc_out = cache_table_getm(r->pool, r->headers_out, "Cache-Control");
pragma = cache_table_getm(r->pool, r->headers_out, "Pragma");
headers = r->headers_out;
}
if (r->status == HTTP_NOT_MODIFIED && cache->stale_handle) {
if (!cc_out && !pragma) {
cc_out = cache_table_getm(r->pool, cache->stale_handle->resp_hdrs,
"Cache-Control");
pragma = cache_table_getm(r->pool, cache->stale_handle->resp_hdrs,
"Pragma");
}
ap_set_content_type(r, apr_table_get(
cache->stale_handle->resp_hdrs, "Content-Type"));
}
memset(&control, 0, sizeof(cache_control_t));
ap_cache_control(r, &control, cc_out, pragma, headers);
if (r->status != HTTP_OK && r->status != HTTP_NON_AUTHORITATIVE
&& r->status != HTTP_PARTIAL_CONTENT
&& r->status != HTTP_MULTIPLE_CHOICES
&& r->status != HTTP_MOVED_PERMANENTLY
&& r->status != HTTP_NOT_MODIFIED) {
if (exps != NULL || cc_out != NULL) {
} else {
reason = apr_psprintf(p, "Response status %d", r->status);
}
}
if (reason) {
} else if (exps != NULL && exp == APR_DATE_BAD) {
reason = apr_pstrcat(p, "Broken expires header: ", exps, NULL);
} else if (!control.s_maxage && !control.max_age
&& !dconf->store_expired && exp != APR_DATE_BAD
&& exp < r->request_time) {
reason = "Expires header already expired; not cacheable";
} else if (!dconf->store_expired && (control.must_revalidate
|| control.proxy_revalidate) && (!control.s_maxage_value
|| (!control.s_maxage && !control.max_age_value)) && lastmods
== NULL && etag == NULL) {
reason
= "s-maxage or max-age zero and no Last-Modified or Etag; not cacheable";
} else if (!conf->ignorequerystring && query && exps == NULL
&& !control.max_age && !control.s_maxage) {
reason = "Query string present but no explicit expiration time";
} else if (r->status == HTTP_NOT_MODIFIED &&
!cache->handle && !cache->stale_handle) {
reason = "HTTP Status 304 Not Modified";
} else if (r->status == HTTP_OK && lastmods == NULL && etag == NULL && (exps
== NULL) && (dconf->no_last_mod_ignore == 0) && !control.max_age
&& !control.s_maxage) {
reason = "No Last-Modified; Etag; Expires; Cache-Control:max-age or Cache-Control:s-maxage headers";
} else if (!dconf->store_nostore && control.no_store) {
reason = "Cache-Control: no-store present";
} else if (!dconf->store_private && control.private) {
reason = "Cache-Control: private present";
} else if (apr_table_get(r->headers_in, "Authorization")
&& !(control.s_maxage || control.must_revalidate
|| control.proxy_revalidate || control.public)) {
reason = "Authorization required";
} else if (ap_find_token(NULL, apr_table_get(r->headers_out, "Vary"), "*")) {
reason = "Vary header contains '*'";
} else if (apr_table_get(r->subprocess_env, "no-cache") != NULL) {
reason = "environment variable 'no-cache' is set";
} else if (r->no_cache) {
reason = "r->no_cache present";
} else if (cache->stale_handle
&& APR_DATE_BAD
!= (date = apr_date_parse_http(
apr_table_get(r->headers_out, "Date")))
&& date < cache->stale_handle->cache_obj->info.date) {
reason = "updated entity is older than cached entity";
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(02474)
"cache: Removing CACHE_REMOVE_URL filter.");
ap_remove_output_filter(cache->remove_url_filter);
} else if (r->status == HTTP_NOT_MODIFIED && cache->stale_handle) {
apr_table_t *left = cache->stale_handle->resp_hdrs;
apr_table_t *right = r->headers_out;
const char *ehs = NULL;
if (cache_header_cmp(r->pool, left, right, "ETag")) {
ehs = "ETag";
}
for (eh = MOD_CACHE_ENTITY_HEADERS; *eh; ++eh) {
if (cache_header_cmp(r->pool, left, right, *eh)) {
ehs = (ehs) ? apr_pstrcat(r->pool, ehs, ", ", *eh, NULL) : *eh;
}
}
if (ehs) {
reason = apr_pstrcat(r->pool, "contradiction: 304 Not Modified; "
"but ", ehs, " modified", NULL);
}
}
if (r->status == HTTP_NOT_MODIFIED) {
for (eh = MOD_CACHE_ENTITY_HEADERS; *eh; ++eh) {
apr_table_unset(r->headers_out, *eh);
}
}
if (reason && r->status == HTTP_NOT_MODIFIED && cache->stale_handle) {
ap_log_rerror(APLOG_MARK, APLOG_INFO, 0, r, APLOGNO(02473)
"cache: %s responded with an uncacheable 304, "
"retrying the request %s. Reason: %s",
cache->key, r->unparsed_uri, reason);
cache_run_cache_status(cache->handle, r, r->headers_out, AP_CACHE_MISS,
apr_psprintf(r->pool,
"conditional cache miss: 304 was uncacheable, entity removed: %s",
reason));
ap_remove_output_filter(cache->remove_url_filter);
cache_remove_url(cache, r);
cache_remove_lock(conf, cache, r, NULL);
ap_remove_output_filter(f);
apr_table_unset(r->headers_in, "If-Match");
apr_table_unset(r->headers_in, "If-Modified-Since");
apr_table_unset(r->headers_in, "If-None-Match");
apr_table_unset(r->headers_in, "If-Range");
apr_table_unset(r->headers_in, "If-Unmodified-Since");
r->status = HTTP_OK;
ap_internal_redirect(r->unparsed_uri, r);
return APR_SUCCESS;
}
if (reason) {
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(00768)
"cache: %s not cached for request %s. Reason: %s",
cache->key, r->unparsed_uri, reason);
cache_run_cache_status(cache->handle, r, r->headers_out, AP_CACHE_MISS,
reason);
ap_remove_output_filter(f);
cache_remove_lock(conf, cache, r, NULL);
return ap_pass_brigade(f->next, in);
}
cache->in_checked = 1;
cl = apr_table_get(r->err_headers_out, "Content-Length");
if (cl == NULL) {
cl = apr_table_get(r->headers_out, "Content-Length");
}
if (cl) {
char *errp;
if (apr_strtoff(&size, cl, &errp, 10) || *errp || size < 0) {
cl = NULL;
}
}
if (!cl) {
int all_buckets_here=0;
size=0;
for (e = APR_BRIGADE_FIRST(in);
e != APR_BRIGADE_SENTINEL(in);
e = APR_BUCKET_NEXT(e)) {
if (APR_BUCKET_IS_EOS(e)) {
all_buckets_here=1;
break;
}
if (APR_BUCKET_IS_FLUSH(e)) {
continue;
}
if (e->length == (apr_size_t)-1) {
break;
}
size += e->length;
}
if (!all_buckets_here) {
size = -1;
}
}
cache->size = size;
if (cache->stale_handle) {
if (r->status == HTTP_NOT_MODIFIED) {
cache->handle = cache->stale_handle;
info = &cache->handle->cache_obj->info;
rv = OK;
} else {
cache->provider->remove_entity(cache->stale_handle);
cache->stale_handle = NULL;
r->headers_in = cache->stale_headers;
}
}
if (!cache->handle) {
rv = cache_create_entity(cache, r, size, in);
info = apr_pcalloc(r->pool, sizeof(cache_info));
info->status = r->status;
}
if (rv != OK) {
cache_run_cache_status(cache->handle, r, r->headers_out, AP_CACHE_MISS,
"cache miss: cache unwilling to store response");
ap_remove_output_filter(f);
cache_remove_lock(conf, cache, r, NULL);
return ap_pass_brigade(f->next, in);
}
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(00769)
"cache: Caching url %s for request %s",
cache->key, r->unparsed_uri);
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(00770)
"cache: Removing CACHE_REMOVE_URL filter.");
ap_remove_output_filter(cache->remove_url_filter);
memcpy(&info->control, &control, sizeof(cache_control_t));
dates = apr_table_get(r->err_headers_out, "Date");
if (dates == NULL) {
dates = apr_table_get(r->headers_out, "Date");
}
if (dates != NULL) {
info->date = apr_date_parse_http(dates);
} else {
info->date = APR_DATE_BAD;
}
now = apr_time_now();
if (info->date == APR_DATE_BAD) {
info->date = now;
}
date = info->date;
info->response_time = now;
info->request_time = r->request_time;
if (lastmod != APR_DATE_BAD && lastmod > date) {
lastmod = date;
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0,
r, APLOGNO(00771) "cache: Last modified is in the future, "
"replacing with now");
}
if (control.s_maxage || control.max_age) {
apr_int64_t x;
x = control.s_maxage ? control.s_maxage_value : control.max_age_value;
x = x * MSEC_ONE_SEC;
if (x < dconf->minex) {
x = dconf->minex;
}
if (x > dconf->maxex) {
x = dconf->maxex;
}
exp = date + x;
}
if (exp == APR_DATE_BAD) {
if ((lastmod != APR_DATE_BAD) && (lastmod < date)) {
apr_time_t x = (apr_time_t) ((date - lastmod) * dconf->factor);
if (x < dconf->minex) {
x = dconf->minex;
}
if (x > dconf->maxex) {
x = dconf->maxex;
}
exp = date + x;
} else {
exp = date + dconf->defex;
}
}
info->expire = exp;
if (cache->stale_handle) {
r->headers_out = cache_merge_headers_out(r);
apr_table_clear(r->err_headers_out);
cache_accept_headers(cache->handle, r, r->headers_out,
cache->handle->resp_hdrs, 1);
}
rv = cache->provider->store_headers(cache->handle, r, info);
if (cache->stale_handle) {
apr_bucket_brigade *bb;
apr_bucket *bkt;
int status;
r->status = info->status;
r->status_line = NULL;
if (rv == APR_SUCCESS) {
rv = cache->provider->commit_entity(cache->handle, r);
}
bb = apr_brigade_create(r->pool, r->connection->bucket_alloc);
r->headers_in = cache->stale_headers;
status = ap_meets_conditions(r);
if (status != OK) {
r->status = status;
for (eh = MOD_CACHE_ENTITY_HEADERS; *eh; ++eh) {
apr_table_unset(r->headers_out, *eh);
}
bkt = apr_bucket_flush_create(bb->bucket_alloc);
APR_BRIGADE_INSERT_TAIL(bb, bkt);
} else {
cache->provider->recall_body(cache->handle, r->pool, bb);
bkt = apr_bucket_eos_create(bb->bucket_alloc);
APR_BRIGADE_INSERT_TAIL(bb, bkt);
}
cache->block_response = 1;
if (rv != APR_SUCCESS) {
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, rv, r, APLOGNO(00772)
"cache: updating headers with store_headers failed. "
"Removing cached url.");
rv = cache->provider->remove_url(cache->stale_handle, r);
if (rv != OK) {
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, rv, r, APLOGNO(00773)
"cache: attempt to remove url from cache unsuccessful.");
}
cache_run_cache_status(cache->handle, r, r->headers_out,
AP_CACHE_REVALIDATE,
"conditional cache hit: entity refresh failed");
} else {
cache_run_cache_status(cache->handle, r, r->headers_out,
AP_CACHE_REVALIDATE,
"conditional cache hit: entity refreshed");
}
cache_remove_lock(conf, cache, r, NULL);
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, APR_SUCCESS, r, APLOGNO(02971)
"cache: serving %s (revalidated)", r->uri);
return ap_pass_brigade(f->next, bb);
}
if (rv != APR_SUCCESS) {
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, rv, r, APLOGNO(00774)
"cache: store_headers failed");
cache_run_cache_status(cache->handle, r, r->headers_out, AP_CACHE_MISS,
"cache miss: store_headers failed");
ap_remove_output_filter(f);
cache_remove_lock(conf, cache, r, NULL);
return ap_pass_brigade(f->next, in);
}
cache_run_cache_status(cache->handle, r, r->headers_out, AP_CACHE_MISS,
"cache miss: attempting entity save");
return cache_save_store(f, in, conf, cache);
}
static apr_status_t cache_remove_url_filter(ap_filter_t *f,
apr_bucket_brigade *in) {
request_rec *r = f->r;
cache_request_rec *cache;
cache = (cache_request_rec *) f->ctx;
if (!cache) {
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(00775)
"cache: CACHE_REMOVE_URL enabled unexpectedly");
ap_remove_output_filter(f);
return ap_pass_brigade(f->next, in);
}
cache_remove_url(cache, r);
ap_remove_output_filter(f);
return ap_pass_brigade(f->next, in);
}
static apr_status_t cache_invalidate_filter(ap_filter_t *f,
apr_bucket_brigade *in) {
request_rec *r = f->r;
cache_request_rec *cache;
cache = (cache_request_rec *) f->ctx;
if (!cache) {
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(02465)
"cache: CACHE_INVALIDATE enabled unexpectedly: %s", r->uri);
} else {
if (r->status > 299) {
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(02466)
"cache: response status to '%s' method is %d (>299), not invalidating cached entity: %s", r->method, r->status, r->uri);
} else {
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, APR_SUCCESS, r, APLOGNO(02467)
"cache: Invalidating all cached entities in response to '%s' request for %s",
r->method, r->uri);
cache_invalidate(cache, r);
cache_run_cache_status(cache->handle, r, r->headers_out,
AP_CACHE_INVALIDATE, apr_psprintf(r->pool,
"cache invalidated by %s", r->method));
}
}
ap_remove_output_filter(f);
return ap_pass_brigade(f->next, in);
}
static apr_status_t cache_filter(ap_filter_t *f, apr_bucket_brigade *in) {
cache_server_conf
*conf =
(cache_server_conf *) ap_get_module_config(f->r->server->module_config,
&cache_module);
if (conf->quick) {
ap_log_rerror(APLOG_MARK, APLOG_WARNING, 0, f->r, APLOGNO(00776)
"cache: CACHE filter was added in quick handler mode and "
"will be ignored: %s", f->r->unparsed_uri);
}
else {
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, f->r, APLOGNO(00777)
"cache: CACHE filter was added twice, or was added where "
"the cache has been bypassed and will be ignored: %s",
f->r->unparsed_uri);
}
ap_remove_output_filter(f);
return ap_pass_brigade(f->next, in);
}
static int cache_status(cache_handle_t *h, request_rec *r,
apr_table_t *headers, ap_cache_status_e status, const char *reason) {
cache_server_conf
*conf =
(cache_server_conf *) ap_get_module_config(r->server->module_config,
&cache_module);
cache_dir_conf *dconf = ap_get_module_config(r->per_dir_config, &cache_module);
int x_cache = 0, x_cache_detail = 0;
switch (status) {
case AP_CACHE_HIT: {
apr_table_setn(r->subprocess_env, AP_CACHE_HIT_ENV, reason);
break;
}
case AP_CACHE_REVALIDATE: {
apr_table_setn(r->subprocess_env, AP_CACHE_REVALIDATE_ENV, reason);
break;
}
case AP_CACHE_MISS: {
apr_table_setn(r->subprocess_env, AP_CACHE_MISS_ENV, reason);
break;
}
case AP_CACHE_INVALIDATE: {
apr_table_setn(r->subprocess_env, AP_CACHE_INVALIDATE_ENV, reason);
break;
}
}
apr_table_setn(r->subprocess_env, AP_CACHE_STATUS_ENV, reason);
if (dconf && dconf->x_cache_set) {
x_cache = dconf->x_cache;
} else {
x_cache = conf->x_cache;
}
if (x_cache) {
apr_table_setn(headers, "X-Cache", apr_psprintf(r->pool, "%s from %s",
status == AP_CACHE_HIT ? "HIT"
: status == AP_CACHE_REVALIDATE ? "REVALIDATE" : status
== AP_CACHE_INVALIDATE ? "INVALIDATE" : "MISS",
r->server->server_hostname));
}
if (dconf && dconf->x_cache_detail_set) {
x_cache_detail = dconf->x_cache_detail;
} else {
x_cache_detail = conf->x_cache_detail;
}
if (x_cache_detail) {
apr_table_setn(headers, "X-Cache-Detail", apr_psprintf(r->pool,
"\"%s\" from %s", reason, r->server->server_hostname));
}
return OK;
}
static void cache_insert_error_filter(request_rec *r) {
void *dummy;
cache_dir_conf *dconf;
if (r->status < HTTP_INTERNAL_SERVER_ERROR) {
return;
}
dconf = ap_get_module_config(r->per_dir_config, &cache_module);
if (!dconf->stale_on_error) {
return;
}
apr_pool_userdata_get(&dummy, CACHE_CTX_KEY, r->pool);
if (dummy) {
cache_request_rec *cache = (cache_request_rec *) dummy;
ap_remove_output_filter(cache->remove_url_filter);
if (cache->stale_handle && cache->save_filter
&& !cache->stale_handle->cache_obj->info.control.must_revalidate
&& !cache->stale_handle->cache_obj->info.control.proxy_revalidate
&& !cache->stale_handle->cache_obj->info.control.s_maxage) {
const char *warn_head;
cache_server_conf
*conf =
(cache_server_conf *) ap_get_module_config(r->server->module_config,
&cache_module);
cache->handle = cache->stale_handle;
if (r->main) {
cache->save_filter->frec = cache_out_subreq_filter_handle;
} else {
cache->save_filter->frec = cache_out_filter_handle;
}
r->output_filters = cache->save_filter;
r->err_headers_out = cache->stale_handle->resp_hdrs;
warn_head = apr_table_get(r->err_headers_out, "Warning");
if ((warn_head == NULL) || ((warn_head != NULL)
&& (ap_strstr_c(warn_head, "111") == NULL))) {
apr_table_mergen(r->err_headers_out, "Warning",
"111 Revalidation failed");
}
cache_run_cache_status(
cache->handle,
r,
r->err_headers_out,
AP_CACHE_HIT,
apr_psprintf(
r->pool,
"cache hit: %d status; stale content returned",
r->status));
cache_remove_lock(conf, cache, r, NULL);
}
}
return;
}
static void *create_dir_config(apr_pool_t *p, char *dummy) {
cache_dir_conf *dconf = apr_pcalloc(p, sizeof(cache_dir_conf));
dconf->no_last_mod_ignore = 0;
dconf->store_expired = 0;
dconf->store_private = 0;
dconf->store_nostore = 0;
dconf->maxex = DEFAULT_CACHE_MAXEXPIRE;
dconf->minex = DEFAULT_CACHE_MINEXPIRE;
dconf->defex = DEFAULT_CACHE_EXPIRE;
dconf->factor = DEFAULT_CACHE_LMFACTOR;
dconf->x_cache = DEFAULT_X_CACHE;
dconf->x_cache_detail = DEFAULT_X_CACHE_DETAIL;
dconf->stale_on_error = DEFAULT_CACHE_STALE_ON_ERROR;
dconf->cacheenable = apr_array_make(p, 10, sizeof(struct cache_enable));
return dconf;
}
static void *merge_dir_config(apr_pool_t *p, void *basev, void *addv) {
cache_dir_conf *new = (cache_dir_conf *) apr_pcalloc(p, sizeof(cache_dir_conf));
cache_dir_conf *add = (cache_dir_conf *) addv;
cache_dir_conf *base = (cache_dir_conf *) basev;
new->no_last_mod_ignore = (add->no_last_mod_ignore_set == 0) ? base->no_last_mod_ignore : add->no_last_mod_ignore;
new->no_last_mod_ignore_set = add->no_last_mod_ignore_set || base->no_last_mod_ignore_set;
new->store_expired = (add->store_expired_set == 0) ? base->store_expired : add->store_expired;
new->store_expired_set = add->store_expired_set || base->store_expired_set;
new->store_private = (add->store_private_set == 0) ? base->store_private : add->store_private;
new->store_private_set = add->store_private_set || base->store_private_set;
new->store_nostore = (add->store_nostore_set == 0) ? base->store_nostore : add->store_nostore;
new->store_nostore_set = add->store_nostore_set || base->store_nostore_set;
new->maxex = (add->maxex_set == 0) ? base->maxex : add->maxex;
new->maxex_set = add->maxex_set || base->maxex_set;
new->minex = (add->minex_set == 0) ? base->minex : add->minex;
new->minex_set = add->minex_set || base->minex_set;
new->defex = (add->defex_set == 0) ? base->defex : add->defex;
new->defex_set = add->defex_set || base->defex_set;
new->factor = (add->factor_set == 0) ? base->factor : add->factor;
new->factor_set = add->factor_set || base->factor_set;
new->x_cache = (add->x_cache_set == 0) ? base->x_cache : add->x_cache;
new->x_cache_set = add->x_cache_set || base->x_cache_set;
new->x_cache_detail = (add->x_cache_detail_set == 0) ? base->x_cache_detail
: add->x_cache_detail;
new->x_cache_detail_set = add->x_cache_detail_set
|| base->x_cache_detail_set;
new->stale_on_error = (add->stale_on_error_set == 0) ? base->stale_on_error
: add->stale_on_error;
new->stale_on_error_set = add->stale_on_error_set
|| base->stale_on_error_set;
new->cacheenable = add->enable_set ? apr_array_append(p, base->cacheenable,
add->cacheenable) : base->cacheenable;
new->enable_set = add->enable_set || base->enable_set;
new->disable = (add->disable_set == 0) ? base->disable : add->disable;
new->disable_set = add->disable_set || base->disable_set;
return new;
}
static void * create_cache_config(apr_pool_t *p, server_rec *s) {
const char *tmppath = NULL;
cache_server_conf *ps = apr_pcalloc(p, sizeof(cache_server_conf));
ps->cacheenable = apr_array_make(p, 10, sizeof(struct cache_enable));
ps->cachedisable = apr_array_make(p, 10, sizeof(struct cache_disable));
ps->ignorecachecontrol = 0;
ps->ignorecachecontrol_set = 0;
ps->ignore_headers = apr_array_make(p, 10, sizeof(char *));
ps->ignore_headers_set = CACHE_IGNORE_HEADERS_UNSET;
ps->ignorequerystring = 0;
ps->ignorequerystring_set = 0;
ps->quick = 1;
ps->quick_set = 0;
ps->ignore_session_id = apr_array_make(p, 10, sizeof(char *));
ps->ignore_session_id_set = CACHE_IGNORE_SESSION_ID_UNSET;
ps->lock = 0;
ps->lock_set = 0;
apr_temp_dir_get(&tmppath, p);
if (tmppath) {
ps->lockpath = apr_pstrcat(p, tmppath, DEFAULT_CACHE_LOCKPATH, NULL);
}
ps->lockmaxage = apr_time_from_sec(DEFAULT_CACHE_MAXAGE);
ps->x_cache = DEFAULT_X_CACHE;
ps->x_cache_detail = DEFAULT_X_CACHE_DETAIL;
return ps;
}
static void * merge_cache_config(apr_pool_t *p, void *basev, void *overridesv) {
cache_server_conf *ps = apr_pcalloc(p, sizeof(cache_server_conf));
cache_server_conf *base = (cache_server_conf *) basev;
cache_server_conf *overrides = (cache_server_conf *) overridesv;
ps->cachedisable = apr_array_append(p,
base->cachedisable,
overrides->cachedisable);
ps->cacheenable = apr_array_append(p,
base->cacheenable,
overrides->cacheenable);
ps->ignorecachecontrol =
(overrides->ignorecachecontrol_set == 0)
? base->ignorecachecontrol
: overrides->ignorecachecontrol;
ps->ignore_headers =
(overrides->ignore_headers_set == CACHE_IGNORE_HEADERS_UNSET)
? base->ignore_headers
: overrides->ignore_headers;
ps->ignorequerystring =
(overrides->ignorequerystring_set == 0)
? base->ignorequerystring
: overrides->ignorequerystring;
ps->ignore_session_id =
(overrides->ignore_session_id_set == CACHE_IGNORE_SESSION_ID_UNSET)
? base->ignore_session_id
: overrides->ignore_session_id;
ps->lock =
(overrides->lock_set == 0)
? base->lock
: overrides->lock;
ps->lockpath =
(overrides->lockpath_set == 0)
? base->lockpath
: overrides->lockpath;
ps->lockmaxage =
(overrides->lockmaxage_set == 0)
? base->lockmaxage
: overrides->lockmaxage;
ps->quick =
(overrides->quick_set == 0)
? base->quick
: overrides->quick;
ps->x_cache =
(overrides->x_cache_set == 0)
? base->x_cache
: overrides->x_cache;
ps->x_cache_detail =
(overrides->x_cache_detail_set == 0)
? base->x_cache_detail
: overrides->x_cache_detail;
ps->base_uri =
(overrides->base_uri_set == 0)
? base->base_uri
: overrides->base_uri;
return ps;
}
static const char *set_cache_quick_handler(cmd_parms *parms, void *dummy,
int flag) {
cache_server_conf *conf;
conf =
(cache_server_conf *)ap_get_module_config(parms->server->module_config,
&cache_module);
conf->quick = flag;
conf->quick_set = 1;
return NULL;
}
static const char *set_cache_ignore_no_last_mod(cmd_parms *parms, void *dummy,
int flag) {
cache_dir_conf *dconf = (cache_dir_conf *)dummy;
dconf->no_last_mod_ignore = flag;
dconf->no_last_mod_ignore_set = 1;
return NULL;
}
static const char *set_cache_ignore_cachecontrol(cmd_parms *parms,
void *dummy, int flag) {
cache_server_conf *conf;
conf =
(cache_server_conf *)ap_get_module_config(parms->server->module_config,
&cache_module);
conf->ignorecachecontrol = flag;
conf->ignorecachecontrol_set = 1;
return NULL;
}
static const char *set_cache_store_expired(cmd_parms *parms, void *dummy,
int flag) {
cache_dir_conf *dconf = (cache_dir_conf *)dummy;
dconf->store_expired = flag;
dconf->store_expired_set = 1;
return NULL;
}
static const char *set_cache_store_private(cmd_parms *parms, void *dummy,
int flag) {
cache_dir_conf *dconf = (cache_dir_conf *)dummy;
dconf->store_private = flag;
dconf->store_private_set = 1;
return NULL;
}
static const char *set_cache_store_nostore(cmd_parms *parms, void *dummy,
int flag) {
cache_dir_conf *dconf = (cache_dir_conf *)dummy;
dconf->store_nostore = flag;
dconf->store_nostore_set = 1;
return NULL;
}
static const char *add_ignore_header(cmd_parms *parms, void *dummy,
const char *header) {
cache_server_conf *conf;
char **new;
conf =
(cache_server_conf *)ap_get_module_config(parms->server->module_config,
&cache_module);
if (!strcasecmp(header, "None")) {
conf->ignore_headers->nelts = 0;
} else {
if ((conf->ignore_headers_set == CACHE_IGNORE_HEADERS_UNSET) ||
(conf->ignore_headers->nelts)) {
new = (char **)apr_array_push(conf->ignore_headers);
(*new) = (char *)header;
}
}
conf->ignore_headers_set = CACHE_IGNORE_HEADERS_SET;
return NULL;
}
static const char *add_ignore_session_id(cmd_parms *parms, void *dummy,
const char *identifier) {
cache_server_conf *conf;
char **new;
conf =
(cache_server_conf *)ap_get_module_config(parms->server->module_config,
&cache_module);
if (!strcasecmp(identifier, "None")) {
conf->ignore_session_id->nelts = 0;
} else {
if ((conf->ignore_session_id_set == CACHE_IGNORE_SESSION_ID_UNSET) ||
(conf->ignore_session_id->nelts)) {
new = (char **)apr_array_push(conf->ignore_session_id);
(*new) = (char *)identifier;
}
}
conf->ignore_session_id_set = CACHE_IGNORE_SESSION_ID_SET;
return NULL;
}
static const char *add_cache_enable(cmd_parms *parms, void *dummy,
const char *type,
const char *url) {
cache_dir_conf *dconf = (cache_dir_conf *)dummy;
cache_server_conf *conf;
struct cache_enable *new;
const char *err = ap_check_cmd_context(parms,
NOT_IN_DIRECTORY|NOT_IN_LIMIT|NOT_IN_FILES);
if (err != NULL) {
return err;
}
if (*type == '/') {
return apr_psprintf(parms->pool,
"provider (%s) starts with a '/'. Are url and provider switched?",
type);
}
if (!url) {
url = parms->path;
}
if (!url) {
return apr_psprintf(parms->pool,
"CacheEnable provider (%s) is missing an URL.", type);
}
if (parms->path && strncmp(parms->path, url, strlen(parms->path))) {
return "When in a Location, CacheEnable must specify a path or an URL below "
"that location.";
}
conf =
(cache_server_conf *)ap_get_module_config(parms->server->module_config,
&cache_module);
if (parms->path) {
new = apr_array_push(dconf->cacheenable);
dconf->enable_set = 1;
} else {
new = apr_array_push(conf->cacheenable);
}
new->type = type;
if (apr_uri_parse(parms->pool, url, &(new->url))) {
return NULL;
}
if (new->url.path) {
new->pathlen = strlen(new->url.path);
} else {
new->pathlen = 1;
new->url.path = "/";
}
return NULL;
}
static const char *add_cache_disable(cmd_parms *parms, void *dummy,
const char *url) {
cache_dir_conf *dconf = (cache_dir_conf *)dummy;
cache_server_conf *conf;
struct cache_disable *new;
const char *err = ap_check_cmd_context(parms,
NOT_IN_DIRECTORY|NOT_IN_LIMIT|NOT_IN_FILES);
if (err != NULL) {
return err;
}
conf =
(cache_server_conf *)ap_get_module_config(parms->server->module_config,
&cache_module);
if (parms->path) {
if (!strcasecmp(url, "on")) {
dconf->disable = 1;
dconf->disable_set = 1;
return NULL;
} else {
return "CacheDisable must be followed by the word 'on' when in a Location.";
}
}
if (!url || (url[0] != '/' && !ap_strchr_c(url, ':'))) {
return "CacheDisable must specify a path or an URL.";
}
new = apr_array_push(conf->cachedisable);
if (apr_uri_parse(parms->pool, url, &(new->url))) {
return NULL;
}
if (new->url.path) {
new->pathlen = strlen(new->url.path);
} else {
new->pathlen = 1;
new->url.path = "/";
}
return NULL;
}
static const char *set_cache_maxex(cmd_parms *parms, void *dummy,
const char *arg) {
cache_dir_conf *dconf = (cache_dir_conf *)dummy;
dconf->maxex = (apr_time_t) (atol(arg) * MSEC_ONE_SEC);
dconf->maxex_set = 1;
return NULL;
}
static const char *set_cache_minex(cmd_parms *parms, void *dummy,
const char *arg) {
cache_dir_conf *dconf = (cache_dir_conf *)dummy;
dconf->minex = (apr_time_t) (atol(arg) * MSEC_ONE_SEC);
dconf->minex_set = 1;
return NULL;
}
static const char *set_cache_defex(cmd_parms *parms, void *dummy,
const char *arg) {
cache_dir_conf *dconf = (cache_dir_conf *)dummy;
dconf->defex = (apr_time_t) (atol(arg) * MSEC_ONE_SEC);
dconf->defex_set = 1;
return NULL;
}
static const char *set_cache_factor(cmd_parms *parms, void *dummy,
const char *arg) {
cache_dir_conf *dconf = (cache_dir_conf *)dummy;
double val;
if (sscanf(arg, "%lg", &val) != 1) {
return "CacheLastModifiedFactor value must be a float";
}
dconf->factor = val;
dconf->factor_set = 1;
return NULL;
}
static const char *set_cache_ignore_querystring(cmd_parms *parms, void *dummy,
int flag) {
cache_server_conf *conf;
conf =
(cache_server_conf *)ap_get_module_config(parms->server->module_config,
&cache_module);
conf->ignorequerystring = flag;
conf->ignorequerystring_set = 1;
return NULL;
}
static const char *set_cache_lock(cmd_parms *parms, void *dummy,
int flag) {
cache_server_conf *conf;
conf =
(cache_server_conf *)ap_get_module_config(parms->server->module_config,
&cache_module);
conf->lock = flag;
conf->lock_set = 1;
return NULL;
}
static const char *set_cache_lock_path(cmd_parms *parms, void *dummy,
const char *arg) {
cache_server_conf *conf;
conf =
(cache_server_conf *)ap_get_module_config(parms->server->module_config,
&cache_module);
conf->lockpath = ap_server_root_relative(parms->pool, arg);
if (!conf->lockpath) {
return apr_pstrcat(parms->pool, "Invalid CacheLockPath path ",
arg, NULL);
}
conf->lockpath_set = 1;
return NULL;
}
static const char *set_cache_lock_maxage(cmd_parms *parms, void *dummy,
const char *arg) {
cache_server_conf *conf;
apr_int64_t seconds;
conf =
(cache_server_conf *)ap_get_module_config(parms->server->module_config,
&cache_module);
seconds = apr_atoi64(arg);
if (seconds <= 0) {
return "CacheLockMaxAge value must be a non-zero positive integer";
}
conf->lockmaxage = apr_time_from_sec(seconds);
conf->lockmaxage_set = 1;
return NULL;
}
static const char *set_cache_x_cache(cmd_parms *parms, void *dummy, int flag) {
if (parms->path) {
cache_dir_conf *dconf = (cache_dir_conf *)dummy;
dconf->x_cache = flag;
dconf->x_cache_set = 1;
} else {
cache_server_conf *conf =
(cache_server_conf *)ap_get_module_config(parms->server->module_config,
&cache_module);
conf->x_cache = flag;
conf->x_cache_set = 1;
}
return NULL;
}
static const char *set_cache_x_cache_detail(cmd_parms *parms, void *dummy, int flag) {
if (parms->path) {
cache_dir_conf *dconf = (cache_dir_conf *)dummy;
dconf->x_cache_detail = flag;
dconf->x_cache_detail_set = 1;
} else {
cache_server_conf *conf =
(cache_server_conf *)ap_get_module_config(parms->server->module_config,
&cache_module);
conf->x_cache_detail = flag;
conf->x_cache_detail_set = 1;
}
return NULL;
}
static const char *set_cache_key_base_url(cmd_parms *parms, void *dummy,
const char *arg) {
cache_server_conf *conf;
apr_status_t rv;
conf =
(cache_server_conf *)ap_get_module_config(parms->server->module_config,
&cache_module);
conf->base_uri = apr_pcalloc(parms->pool, sizeof(apr_uri_t));
rv = apr_uri_parse(parms->pool, arg, conf->base_uri);
if (rv != APR_SUCCESS) {
return apr_psprintf(parms->pool, "Could not parse '%s' as an URL.", arg);
} else if (!conf->base_uri->scheme && !conf->base_uri->hostname &&
!conf->base_uri->port_str) {
return apr_psprintf(parms->pool, "URL '%s' must contain at least one of a scheme, a hostname or a port.", arg);
}
conf->base_uri_set = 1;
return NULL;
}
static const char *set_cache_stale_on_error(cmd_parms *parms, void *dummy,
int flag) {
cache_dir_conf *dconf = (cache_dir_conf *)dummy;
dconf->stale_on_error = flag;
dconf->stale_on_error_set = 1;
return NULL;
}
static int cache_post_config(apr_pool_t *p, apr_pool_t *plog,
apr_pool_t *ptemp, server_rec *s) {
cache_generate_key = APR_RETRIEVE_OPTIONAL_FN(ap_cache_generate_key);
if (!cache_generate_key) {
cache_generate_key = cache_generate_key_default;
}
return OK;
}
static const command_rec cache_cmds[] = {
AP_INIT_TAKE12("CacheEnable", add_cache_enable, NULL, RSRC_CONF|ACCESS_CONF,
"A cache type and partial URL prefix below which "
"caching is enabled"),
AP_INIT_TAKE1("CacheDisable", add_cache_disable, NULL, RSRC_CONF|ACCESS_CONF,
"A partial URL prefix below which caching is disabled"),
AP_INIT_TAKE1("CacheMaxExpire", set_cache_maxex, NULL, RSRC_CONF|ACCESS_CONF,
"The maximum time in seconds to cache a document"),
AP_INIT_TAKE1("CacheMinExpire", set_cache_minex, NULL, RSRC_CONF|ACCESS_CONF,
"The minimum time in seconds to cache a document"),
AP_INIT_TAKE1("CacheDefaultExpire", set_cache_defex, NULL, RSRC_CONF|ACCESS_CONF,
"The default time in seconds to cache a document"),
AP_INIT_FLAG("CacheQuickHandler", set_cache_quick_handler, NULL,
RSRC_CONF,
"Run the cache in the quick handler, default on"),
AP_INIT_FLAG("CacheIgnoreNoLastMod", set_cache_ignore_no_last_mod, NULL,
RSRC_CONF|ACCESS_CONF,
"Ignore Responses where there is no Last Modified Header"),
AP_INIT_FLAG("CacheIgnoreCacheControl", set_cache_ignore_cachecontrol,
NULL, RSRC_CONF,
"Ignore requests from the client for uncached content"),
AP_INIT_FLAG("CacheStoreExpired", set_cache_store_expired,
NULL, RSRC_CONF|ACCESS_CONF,
"Ignore expiration dates when populating cache, resulting in "
"an If-Modified-Since request to the backend on retrieval"),
AP_INIT_FLAG("CacheStorePrivate", set_cache_store_private,
NULL, RSRC_CONF|ACCESS_CONF,
"Ignore 'Cache-Control: private' and store private content"),
AP_INIT_FLAG("CacheStoreNoStore", set_cache_store_nostore,
NULL, RSRC_CONF|ACCESS_CONF,
"Ignore 'Cache-Control: no-store' and store sensitive content"),
AP_INIT_ITERATE("CacheIgnoreHeaders", add_ignore_header, NULL, RSRC_CONF,
"A space separated list of headers that should not be "
"stored by the cache"),
AP_INIT_FLAG("CacheIgnoreQueryString", set_cache_ignore_querystring,
NULL, RSRC_CONF,
"Ignore query-string when caching"),
AP_INIT_ITERATE("CacheIgnoreURLSessionIdentifiers", add_ignore_session_id,
NULL, RSRC_CONF, "A space separated list of session "
"identifiers that should be ignored for creating the key "
"of the cached entity."),
AP_INIT_TAKE1("CacheLastModifiedFactor", set_cache_factor, NULL, RSRC_CONF|ACCESS_CONF,
"The factor used to estimate Expires date from "
"LastModified date"),
AP_INIT_FLAG("CacheLock", set_cache_lock,
NULL, RSRC_CONF,
"Enable or disable the thundering herd lock."),
AP_INIT_TAKE1("CacheLockPath", set_cache_lock_path, NULL, RSRC_CONF,
"The thundering herd lock path. Defaults to the '"
DEFAULT_CACHE_LOCKPATH "' directory in the system "
"temp directory."),
AP_INIT_TAKE1("CacheLockMaxAge", set_cache_lock_maxage, NULL, RSRC_CONF,
"Maximum age of any thundering herd lock."),
AP_INIT_FLAG("CacheHeader", set_cache_x_cache, NULL, RSRC_CONF | ACCESS_CONF,
"Add a X-Cache header to responses. Default is off."),
AP_INIT_FLAG("CacheDetailHeader", set_cache_x_cache_detail, NULL,
RSRC_CONF | ACCESS_CONF,
"Add a X-Cache-Detail header to responses. Default is off."),
AP_INIT_TAKE1("CacheKeyBaseURL", set_cache_key_base_url, NULL, RSRC_CONF,
"Override the base URL of reverse proxied cache keys."),
AP_INIT_FLAG("CacheStaleOnError", set_cache_stale_on_error,
NULL, RSRC_CONF|ACCESS_CONF,
"Serve stale content on 5xx errors if present. Defaults to on."),
{NULL}
};
static void register_hooks(apr_pool_t *p) {
ap_hook_quick_handler(cache_quick_handler, NULL, NULL, APR_HOOK_FIRST);
ap_hook_handler(cache_handler, NULL, NULL, APR_HOOK_REALLY_FIRST);
cache_hook_cache_status(cache_status, NULL, NULL, APR_HOOK_MIDDLE);
ap_hook_insert_error_filter(cache_insert_error_filter, NULL, NULL, APR_HOOK_MIDDLE);
cache_filter_handle =
ap_register_output_filter("CACHE",
cache_filter,
NULL,
AP_FTYPE_RESOURCE);
cache_save_filter_handle =
ap_register_output_filter("CACHE_SAVE",
cache_save_filter,
NULL,
AP_FTYPE_CONTENT_SET+1);
cache_save_subreq_filter_handle =
ap_register_output_filter("CACHE_SAVE_SUBREQ",
cache_save_filter,
NULL,
AP_FTYPE_CONTENT_SET-1);
cache_out_filter_handle =
ap_register_output_filter("CACHE_OUT",
cache_out_filter,
NULL,
AP_FTYPE_CONTENT_SET+1);
cache_out_subreq_filter_handle =
ap_register_output_filter("CACHE_OUT_SUBREQ",
cache_out_filter,
NULL,
AP_FTYPE_CONTENT_SET-1);
cache_remove_url_filter_handle =
ap_register_output_filter("CACHE_REMOVE_URL",
cache_remove_url_filter,
NULL,
AP_FTYPE_PROTOCOL);
cache_invalidate_filter_handle =
ap_register_output_filter("CACHE_INVALIDATE",
cache_invalidate_filter,
NULL,
AP_FTYPE_PROTOCOL);
ap_hook_post_config(cache_post_config, NULL, NULL, APR_HOOK_REALLY_FIRST);
}
AP_DECLARE_MODULE(cache) = {
STANDARD20_MODULE_STUFF,
create_dir_config,
merge_dir_config,
create_cache_config,
merge_cache_config,
cache_cmds,
register_hooks
};
APR_HOOK_STRUCT(
APR_HOOK_LINK(cache_status)
)
APR_IMPLEMENT_EXTERNAL_HOOK_RUN_ALL(cache, CACHE, int, cache_status,
(cache_handle_t *h, request_rec *r,
apr_table_t *headers, ap_cache_status_e status,
const char *reason), (h, r, headers, status, reason),
OK, DECLINED)
