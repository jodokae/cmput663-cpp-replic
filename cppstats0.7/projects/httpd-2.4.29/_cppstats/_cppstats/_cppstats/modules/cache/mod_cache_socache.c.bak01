#include "apr_lib.h"
#include "apr_file_io.h"
#include "apr_strings.h"
#include "apr_buckets.h"
#include "httpd.h"
#include "http_config.h"
#include "http_log.h"
#include "http_core.h"
#include "http_protocol.h"
#include "ap_provider.h"
#include "ap_socache.h"
#include "util_filter.h"
#include "util_script.h"
#include "util_charset.h"
#include "util_mutex.h"
#include "mod_cache.h"
#include "mod_status.h"
#include "cache_socache_common.h"
module AP_MODULE_DECLARE_DATA cache_socache_module;
typedef struct cache_socache_object_t {
apr_pool_t *pool;
unsigned char *buffer;
apr_size_t buffer_len;
apr_bucket_brigade *body;
apr_table_t *headers_in;
apr_table_t *headers_out;
cache_socache_info_t socache_info;
apr_size_t body_offset;
apr_off_t body_length;
apr_time_t expire;
const char *name;
const char *key;
apr_off_t offset;
apr_time_t timeout;
unsigned int newbody :1;
unsigned int done :1;
} cache_socache_object_t;
#define DEFAULT_MAX_FILE_SIZE 100*1024
#define DEFAULT_MAXTIME 86400
#define DEFAULT_MINTIME 600
#define DEFAULT_READSIZE 0
#define DEFAULT_READTIME 0
typedef struct cache_socache_provider_conf {
const char *args;
ap_socache_provider_t *socache_provider;
ap_socache_instance_t *socache_instance;
} cache_socache_provider_conf;
typedef struct cache_socache_conf {
cache_socache_provider_conf *provider;
} cache_socache_conf;
typedef struct cache_socache_dir_conf {
apr_off_t max;
apr_time_t maxtime;
apr_time_t mintime;
apr_off_t readsize;
apr_time_t readtime;
unsigned int max_set :1;
unsigned int maxtime_set :1;
unsigned int mintime_set :1;
unsigned int readsize_set :1;
unsigned int readtime_set :1;
} cache_socache_dir_conf;
static const char * const cache_socache_id = "cache-socache";
static apr_global_mutex_t *socache_mutex = NULL;
static apr_status_t read_array(request_rec *r, apr_array_header_t *arr,
unsigned char *buffer, apr_size_t buffer_len, apr_size_t *slider) {
apr_size_t val = *slider;
while (*slider < buffer_len) {
if (buffer[*slider] == '\r') {
if (val == *slider) {
(*slider)++;
return APR_SUCCESS;
}
*((const char **) apr_array_push(arr)) = apr_pstrndup(r->pool,
(const char *) buffer + val, *slider - val);
(*slider)++;
if (buffer[*slider] == '\n') {
(*slider)++;
}
val = *slider;
} else if (buffer[*slider] == '\0') {
(*slider)++;
return APR_SUCCESS;
} else {
(*slider)++;
}
}
return APR_EOF;
}
static apr_status_t store_array(apr_array_header_t *arr, unsigned char *buffer,
apr_size_t buffer_len, apr_size_t *slider) {
int i, len;
const char **elts;
elts = (const char **) arr->elts;
for (i = 0; i < arr->nelts; i++) {
apr_size_t e_len = strlen(elts[i]);
if (e_len + 3 >= buffer_len - *slider) {
return APR_EOF;
}
len = apr_snprintf(buffer ? (char *) buffer + *slider : NULL,
buffer ? buffer_len - *slider : 0, "%s" CRLF, elts[i]);
*slider += len;
}
if (buffer) {
memcpy(buffer + *slider, CRLF, sizeof(CRLF) - 1);
}
*slider += sizeof(CRLF) - 1;
return APR_SUCCESS;
}
static apr_status_t read_table(cache_handle_t *handle, request_rec *r,
apr_table_t *table, unsigned char *buffer, apr_size_t buffer_len,
apr_size_t *slider) {
apr_size_t key = *slider, colon = 0, len = 0;
while (*slider < buffer_len) {
if (buffer[*slider] == ':') {
if (!colon) {
colon = *slider;
}
(*slider)++;
} else if (buffer[*slider] == '\r') {
len = colon;
if (key == *slider) {
(*slider)++;
if (buffer[*slider] == '\n') {
(*slider)++;
}
return APR_SUCCESS;
}
if (!colon || buffer[colon++] != ':') {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(02344)
"Premature end of cache headers.");
return APR_EGENERAL;
}
while (apr_isspace(buffer[colon])) {
colon++;
}
apr_table_addn(table, apr_pstrndup(r->pool, (const char *) buffer
+ key, len - key), apr_pstrndup(r->pool,
(const char *) buffer + colon, *slider - colon));
(*slider)++;
if (buffer[*slider] == '\n') {
(*slider)++;
}
key = *slider;
colon = 0;
} else if (buffer[*slider] == '\0') {
(*slider)++;
return APR_SUCCESS;
} else {
(*slider)++;
}
}
return APR_EOF;
}
static apr_status_t store_table(apr_table_t *table, unsigned char *buffer,
apr_size_t buffer_len, apr_size_t *slider) {
int i, len;
apr_table_entry_t *elts;
elts = (apr_table_entry_t *) apr_table_elts(table)->elts;
for (i = 0; i < apr_table_elts(table)->nelts; ++i) {
if (elts[i].key != NULL) {
apr_size_t key_len = strlen(elts[i].key);
apr_size_t val_len = strlen(elts[i].val);
if (key_len + val_len + 5 >= buffer_len - *slider) {
return APR_EOF;
}
len = apr_snprintf(buffer ? (char *) buffer + *slider : NULL,
buffer ? buffer_len - *slider : 0, "%s: %s" CRLF,
elts[i].key, elts[i].val);
*slider += len;
}
}
if (3 >= buffer_len - *slider) {
return APR_EOF;
}
if (buffer) {
memcpy(buffer + *slider, CRLF, sizeof(CRLF) - 1);
}
*slider += sizeof(CRLF) - 1;
return APR_SUCCESS;
}
static const char* regen_key(apr_pool_t *p, apr_table_t *headers,
apr_array_header_t *varray, const char *oldkey,
apr_size_t *newkeylen) {
struct iovec *iov;
int i, k;
int nvec;
const char *header;
const char **elts;
nvec = (varray->nelts * 2) + 1;
iov = apr_palloc(p, sizeof(struct iovec) * nvec);
elts = (const char **) varray->elts;
for (i = 0, k = 0; i < varray->nelts; i++) {
header = apr_table_get(headers, elts[i]);
if (!header) {
header = "";
}
iov[k].iov_base = (char*) elts[i];
iov[k].iov_len = strlen(elts[i]);
k++;
iov[k].iov_base = (char*) header;
iov[k].iov_len = strlen(header);
k++;
}
iov[k].iov_base = (char*) oldkey;
iov[k].iov_len = strlen(oldkey);
k++;
return apr_pstrcatv(p, iov, k, newkeylen);
}
static int array_alphasort(const void *fn1, const void *fn2) {
return strcmp(*(char**) fn1, *(char**) fn2);
}
static void tokens_to_array(apr_pool_t *p, const char *data,
apr_array_header_t *arr) {
char *token;
while ((token = ap_get_list_item(p, &data)) != NULL) {
*((const char **) apr_array_push(arr)) = token;
}
qsort((void *) arr->elts, arr->nelts, sizeof(char *), array_alphasort);
}
static int create_entity(cache_handle_t *h, request_rec *r, const char *key,
apr_off_t len, apr_bucket_brigade *bb) {
cache_socache_dir_conf *dconf =
ap_get_module_config(r->per_dir_config, &cache_socache_module);
cache_socache_conf *conf = ap_get_module_config(r->server->module_config,
&cache_socache_module);
cache_object_t *obj;
cache_socache_object_t *sobj;
apr_size_t total;
if (conf->provider == NULL) {
return DECLINED;
}
if (r->status == HTTP_PARTIAL_CONTENT) {
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(02345)
"URL %s partial content response not cached",
key);
return DECLINED;
}
if (len < 0) {
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(02346)
"URL '%s' had no explicit size, ignoring", key);
return DECLINED;
}
if (len > dconf->max) {
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(02347)
"URL '%s' body larger than limit, ignoring "
"(%" APR_OFF_T_FMT " > %" APR_OFF_T_FMT ")",
key, len, dconf->max);
return DECLINED;
}
total = len + sizeof(cache_socache_info_t) + strlen(key);
if (APR_SUCCESS != store_table(r->headers_out, NULL, dconf->max, &total)
|| APR_SUCCESS != store_table(r->headers_in, NULL, dconf->max,
&total)) {
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(02348)
"URL '%s' estimated headers size larger than limit, ignoring "
"(%" APR_SIZE_T_FMT " > %" APR_OFF_T_FMT ")",
key, total, dconf->max);
return DECLINED;
}
if (total >= dconf->max) {
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(02349)
"URL '%s' body and headers larger than limit, ignoring "
"(%" APR_OFF_T_FMT " > %" APR_OFF_T_FMT ")",
key, len, dconf->max);
return DECLINED;
}
h->cache_obj = obj = apr_pcalloc(r->pool, sizeof(*obj));
obj->vobj = sobj = apr_pcalloc(r->pool, sizeof(*sobj));
obj->key = apr_pstrdup(r->pool, key);
sobj->key = obj->key;
sobj->name = obj->key;
return OK;
}
static int open_entity(cache_handle_t *h, request_rec *r, const char *key) {
cache_socache_dir_conf *dconf =
ap_get_module_config(r->per_dir_config, &cache_socache_module);
cache_socache_conf *conf = ap_get_module_config(r->server->module_config,
&cache_socache_module);
apr_uint32_t format;
apr_size_t slider;
unsigned int buffer_len;
const char *nkey;
apr_status_t rc;
cache_object_t *obj;
cache_info *info;
cache_socache_object_t *sobj;
apr_size_t len;
nkey = NULL;
h->cache_obj = NULL;
if (!conf->provider || !conf->provider->socache_instance) {
return DECLINED;
}
obj = apr_pcalloc(r->pool, sizeof(cache_object_t));
sobj = apr_pcalloc(r->pool, sizeof(cache_socache_object_t));
info = &(obj->info);
apr_pool_create(&sobj->pool, r->pool);
sobj->buffer = apr_palloc(sobj->pool, dconf->max);
sobj->buffer_len = dconf->max;
if (socache_mutex) {
apr_status_t status = apr_global_mutex_lock(socache_mutex);
if (status != APR_SUCCESS) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, status, r, APLOGNO(02350)
"could not acquire lock, ignoring: %s", obj->key);
apr_pool_destroy(sobj->pool);
sobj->pool = NULL;
return DECLINED;
}
}
buffer_len = sobj->buffer_len;
rc = conf->provider->socache_provider->retrieve(
conf->provider->socache_instance, r->server, (unsigned char *) key,
strlen(key), sobj->buffer, &buffer_len, r->pool);
if (socache_mutex) {
apr_status_t status = apr_global_mutex_unlock(socache_mutex);
if (status != APR_SUCCESS) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, status, r, APLOGNO(02351)
"could not release lock, ignoring: %s", obj->key);
apr_pool_destroy(sobj->pool);
sobj->pool = NULL;
return DECLINED;
}
}
if (rc != APR_SUCCESS) {
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, rc, r, APLOGNO(02352)
"Key not found in cache: %s", key);
apr_pool_destroy(sobj->pool);
sobj->pool = NULL;
return DECLINED;
}
if (buffer_len >= sobj->buffer_len) {
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, rc, r, APLOGNO(02353)
"Key found in cache but too big, ignoring: %s", key);
apr_pool_destroy(sobj->pool);
sobj->pool = NULL;
return DECLINED;
}
memcpy(&format, sobj->buffer, sizeof(format));
slider = sizeof(format);
if (format == CACHE_SOCACHE_VARY_FORMAT_VERSION) {
apr_array_header_t* varray;
apr_time_t expire;
memcpy(&expire, sobj->buffer + slider, sizeof(expire));
slider += sizeof(expire);
varray = apr_array_make(r->pool, 5, sizeof(char*));
rc = read_array(r, varray, sobj->buffer, buffer_len, &slider);
if (rc != APR_SUCCESS) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, rc, r, APLOGNO(02354)
"Cannot parse vary entry for key: %s", key);
apr_pool_destroy(sobj->pool);
sobj->pool = NULL;
return DECLINED;
}
nkey = regen_key(r->pool, r->headers_in, varray, key, &len);
if (socache_mutex) {
apr_status_t status = apr_global_mutex_lock(socache_mutex);
if (status != APR_SUCCESS) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, status, r, APLOGNO(02355)
"could not acquire lock, ignoring: %s", obj->key);
apr_pool_destroy(sobj->pool);
sobj->pool = NULL;
return DECLINED;
}
}
buffer_len = sobj->buffer_len;
rc = conf->provider->socache_provider->retrieve(
conf->provider->socache_instance, r->server,
(unsigned char *) nkey, len, sobj->buffer,
&buffer_len, r->pool);
if (socache_mutex) {
apr_status_t status = apr_global_mutex_unlock(socache_mutex);
if (status != APR_SUCCESS) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, status, r, APLOGNO(02356)
"could not release lock, ignoring: %s", obj->key);
apr_pool_destroy(sobj->pool);
sobj->pool = NULL;
return DECLINED;
}
}
if (rc != APR_SUCCESS) {
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, rc, r, APLOGNO(02357)
"Key not found in cache: %s", key);
apr_pool_destroy(sobj->pool);
sobj->pool = NULL;
return DECLINED;
}
if (buffer_len >= sobj->buffer_len) {
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, rc, r, APLOGNO(02358)
"Key found in cache but too big, ignoring: %s", key);
goto fail;
}
} else if (format != CACHE_SOCACHE_DISK_FORMAT_VERSION) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(02359)
"Key '%s' found in cache has version %d, expected %d, ignoring",
key, format, CACHE_SOCACHE_DISK_FORMAT_VERSION);
goto fail;
} else {
nkey = key;
}
obj->key = nkey;
sobj->key = nkey;
sobj->name = key;
if (buffer_len >= sizeof(cache_socache_info_t)) {
memcpy(&sobj->socache_info, sobj->buffer, sizeof(cache_socache_info_t));
} else {
ap_log_rerror(APLOG_MARK, APLOG_ERR, rc, r, APLOGNO(02360)
"Cache entry for key '%s' too short, removing", nkey);
goto fail;
}
slider = sizeof(cache_socache_info_t);
info->status = sobj->socache_info.status;
info->date = sobj->socache_info.date;
info->expire = sobj->socache_info.expire;
info->request_time = sobj->socache_info.request_time;
info->response_time = sobj->socache_info.response_time;
memcpy(&info->control, &sobj->socache_info.control, sizeof(cache_control_t));
if (sobj->socache_info.name_len <= buffer_len - slider) {
if (strncmp((const char *) sobj->buffer + slider, sobj->name,
sobj->socache_info.name_len)) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, rc, r, APLOGNO(02361)
"Cache entry for key '%s' URL mismatch, ignoring", nkey);
apr_pool_destroy(sobj->pool);
sobj->pool = NULL;
return DECLINED;
}
slider += sobj->socache_info.name_len;
} else {
ap_log_rerror(APLOG_MARK, APLOG_ERR, rc, r, APLOGNO(02362)
"Cache entry for key '%s' too short, removing", nkey);
goto fail;
}
if (sobj->socache_info.header_only && !r->header_only) {
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, APR_SUCCESS, r, APLOGNO(02363)
"HEAD request cached, non-HEAD requested, ignoring: %s",
sobj->key);
apr_pool_destroy(sobj->pool);
sobj->pool = NULL;
return DECLINED;
}
h->req_hdrs = apr_table_make(r->pool, 20);
h->resp_hdrs = apr_table_make(r->pool, 20);
if (APR_SUCCESS != read_table(h, r, h->resp_hdrs, sobj->buffer, buffer_len,
&slider)) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, rc, r, APLOGNO(02364)
"Cache entry for key '%s' response headers unreadable, removing", nkey);
goto fail;
}
if (APR_SUCCESS != read_table(h, r, h->req_hdrs, sobj->buffer, buffer_len,
&slider)) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, rc, r, APLOGNO(02365)
"Cache entry for key '%s' request headers unreadable, removing", nkey);
goto fail;
}
sobj->body = apr_brigade_create(r->pool, r->connection->bucket_alloc);
len = buffer_len - slider;
if (len * 2 > dconf->max) {
apr_bucket *e;
e = apr_bucket_immortal_create((const char *) sobj->buffer + slider,
len, r->connection->bucket_alloc);
APR_BRIGADE_INSERT_TAIL(sobj->body, e);
} else {
apr_brigade_write(sobj->body, NULL, NULL, (const char *) sobj->buffer
+ slider, len);
apr_pool_destroy(sobj->pool);
sobj->pool = NULL;
}
h->cache_obj = obj;
obj->vobj = sobj;
return OK;
fail:
if (socache_mutex) {
apr_status_t status = apr_global_mutex_lock(socache_mutex);
if (status != APR_SUCCESS) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, status, r, APLOGNO(02366)
"could not acquire lock, ignoring: %s", obj->key);
apr_pool_destroy(sobj->pool);
sobj->pool = NULL;
return DECLINED;
}
}
conf->provider->socache_provider->remove(
conf->provider->socache_instance, r->server,
(unsigned char *) nkey, strlen(nkey), r->pool);
if (socache_mutex) {
apr_status_t status = apr_global_mutex_unlock(socache_mutex);
if (status != APR_SUCCESS) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, status, r, APLOGNO(02367)
"could not release lock, ignoring: %s", obj->key);
}
}
apr_pool_destroy(sobj->pool);
sobj->pool = NULL;
return DECLINED;
}
static int remove_entity(cache_handle_t *h) {
h->cache_obj = NULL;
return OK;
}
static int remove_url(cache_handle_t *h, request_rec *r) {
cache_socache_conf *conf = ap_get_module_config(r->server->module_config,
&cache_socache_module);
cache_socache_object_t *sobj;
sobj = (cache_socache_object_t *) h->cache_obj->vobj;
if (!sobj) {
return DECLINED;
}
if (socache_mutex) {
apr_status_t status = apr_global_mutex_lock(socache_mutex);
if (status != APR_SUCCESS) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, status, r, APLOGNO(02368)
"could not acquire lock, ignoring: %s", sobj->key);
apr_pool_destroy(sobj->pool);
sobj->pool = NULL;
return DECLINED;
}
}
conf->provider->socache_provider->remove(conf->provider->socache_instance,
r->server, (unsigned char *) sobj->key, strlen(sobj->key), r->pool);
if (socache_mutex) {
apr_status_t status = apr_global_mutex_unlock(socache_mutex);
if (status != APR_SUCCESS) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, status, r, APLOGNO(02369)
"could not release lock, ignoring: %s", sobj->key);
apr_pool_destroy(sobj->pool);
sobj->pool = NULL;
return DECLINED;
}
}
return OK;
}
static apr_status_t recall_headers(cache_handle_t *h, request_rec *r) {
return APR_SUCCESS;
}
static apr_status_t recall_body(cache_handle_t *h, apr_pool_t *p,
apr_bucket_brigade *bb) {
cache_socache_object_t *sobj = (cache_socache_object_t*) h->cache_obj->vobj;
apr_bucket *e;
e = APR_BRIGADE_FIRST(sobj->body);
if (e != APR_BRIGADE_SENTINEL(sobj->body)) {
APR_BUCKET_REMOVE(e);
APR_BRIGADE_INSERT_TAIL(bb, e);
}
return APR_SUCCESS;
}
static apr_status_t store_headers(cache_handle_t *h, request_rec *r,
cache_info *info) {
cache_socache_dir_conf *dconf =
ap_get_module_config(r->per_dir_config, &cache_socache_module);
cache_socache_conf *conf = ap_get_module_config(r->server->module_config,
&cache_socache_module);
apr_size_t slider;
apr_status_t rv;
cache_object_t *obj = h->cache_obj;
cache_socache_object_t *sobj = (cache_socache_object_t*) obj->vobj;
cache_socache_info_t *socache_info;
memcpy(&h->cache_obj->info, info, sizeof(cache_info));
if (r->headers_out) {
sobj->headers_out = ap_cache_cacheable_headers_out(r);
}
if (r->headers_in) {
sobj->headers_in = ap_cache_cacheable_headers_in(r);
}
sobj->expire
= obj->info.expire > r->request_time + dconf->maxtime ? r->request_time
+ dconf->maxtime
: obj->info.expire + dconf->mintime;
apr_pool_create(&sobj->pool, r->pool);
sobj->buffer = apr_palloc(sobj->pool, dconf->max);
sobj->buffer_len = dconf->max;
socache_info = (cache_socache_info_t *) sobj->buffer;
if (sobj->headers_out) {
const char *vary;
vary = apr_table_get(sobj->headers_out, "Vary");
if (vary) {
apr_array_header_t* varray;
apr_uint32_t format = CACHE_SOCACHE_VARY_FORMAT_VERSION;
memcpy(sobj->buffer, &format, sizeof(format));
slider = sizeof(format);
memcpy(sobj->buffer + slider, &obj->info.expire,
sizeof(obj->info.expire));
slider += sizeof(obj->info.expire);
varray = apr_array_make(r->pool, 6, sizeof(char*));
tokens_to_array(r->pool, vary, varray);
if (APR_SUCCESS != (rv = store_array(varray, sobj->buffer,
sobj->buffer_len, &slider))) {
ap_log_rerror(APLOG_MARK, APLOG_WARNING, 0, r, APLOGNO(02370)
"buffer too small for Vary array, caching aborted: %s",
obj->key);
apr_pool_destroy(sobj->pool);
sobj->pool = NULL;
return rv;
}
if (socache_mutex) {
apr_status_t status = apr_global_mutex_lock(socache_mutex);
if (status != APR_SUCCESS) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, status, r, APLOGNO(02371)
"could not acquire lock, ignoring: %s", obj->key);
apr_pool_destroy(sobj->pool);
sobj->pool = NULL;
return status;
}
}
rv = conf->provider->socache_provider->store(
conf->provider->socache_instance, r->server,
(unsigned char *) obj->key, strlen(obj->key), sobj->expire,
(unsigned char *) sobj->buffer, (unsigned int) slider,
sobj->pool);
if (socache_mutex) {
apr_status_t status = apr_global_mutex_unlock(socache_mutex);
if (status != APR_SUCCESS) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, status, r, APLOGNO(02372)
"could not release lock, ignoring: %s", obj->key);
}
}
if (rv != APR_SUCCESS) {
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, rv, r, APLOGNO(02373)
"Vary not written to cache, ignoring: %s", obj->key);
apr_pool_destroy(sobj->pool);
sobj->pool = NULL;
return rv;
}
obj->key = sobj->key = regen_key(r->pool, sobj->headers_in, varray,
sobj->name, NULL);
}
}
socache_info->format = CACHE_SOCACHE_DISK_FORMAT_VERSION;
socache_info->date = obj->info.date;
socache_info->expire = obj->info.expire;
socache_info->entity_version = sobj->socache_info.entity_version++;
socache_info->request_time = obj->info.request_time;
socache_info->response_time = obj->info.response_time;
socache_info->status = obj->info.status;
if (r->header_only && r->status != HTTP_NOT_MODIFIED) {
socache_info->header_only = 1;
} else {
socache_info->header_only = sobj->socache_info.header_only;
}
socache_info->name_len = strlen(sobj->name);
memcpy(&socache_info->control, &obj->info.control, sizeof(cache_control_t));
slider = sizeof(cache_socache_info_t);
if (slider + socache_info->name_len >= sobj->buffer_len) {
ap_log_rerror(APLOG_MARK, APLOG_WARNING, 0, r, APLOGNO(02374)
"cache buffer too small for name: %s",
sobj->name);
apr_pool_destroy(sobj->pool);
sobj->pool = NULL;
return APR_EGENERAL;
}
memcpy(sobj->buffer + slider, sobj->name, socache_info->name_len);
slider += socache_info->name_len;
if (sobj->headers_out) {
if (APR_SUCCESS != store_table(sobj->headers_out, sobj->buffer,
sobj->buffer_len, &slider)) {
ap_log_rerror(APLOG_MARK, APLOG_WARNING, 0, r, APLOGNO(02375)
"out-headers didn't fit in buffer: %s", sobj->name);
apr_pool_destroy(sobj->pool);
sobj->pool = NULL;
return APR_EGENERAL;
}
}
if (sobj->headers_in) {
if (APR_SUCCESS != store_table(sobj->headers_in, sobj->buffer,
sobj->buffer_len, &slider)) {
ap_log_rerror(APLOG_MARK, APLOG_WARNING, 0, r, APLOGNO(02376)
"in-headers didn't fit in buffer %s",
sobj->key);
apr_pool_destroy(sobj->pool);
sobj->pool = NULL;
return APR_EGENERAL;
}
}
sobj->body_offset = slider;
return APR_SUCCESS;
}
static apr_status_t store_body(cache_handle_t *h, request_rec *r,
apr_bucket_brigade *in, apr_bucket_brigade *out) {
apr_bucket *e;
apr_status_t rv = APR_SUCCESS;
cache_socache_object_t *sobj =
(cache_socache_object_t *) h->cache_obj->vobj;
cache_socache_dir_conf *dconf =
ap_get_module_config(r->per_dir_config, &cache_socache_module);
int seen_eos = 0;
if (!sobj->offset) {
sobj->offset = dconf->readsize;
}
if (!sobj->timeout && dconf->readtime) {
sobj->timeout = apr_time_now() + dconf->readtime;
}
if (!sobj->newbody) {
sobj->body_length = 0;
sobj->newbody = 1;
}
if (sobj->offset) {
apr_brigade_partition(in, sobj->offset, &e);
}
while (APR_SUCCESS == rv && !APR_BRIGADE_EMPTY(in)) {
const char *str;
apr_size_t length;
e = APR_BRIGADE_FIRST(in);
if (sobj->done || !sobj->pool) {
APR_BUCKET_REMOVE(e);
APR_BRIGADE_INSERT_TAIL(out, e);
continue;
}
if (APR_BUCKET_IS_EOS(e)) {
seen_eos = 1;
sobj->done = 1;
APR_BUCKET_REMOVE(e);
APR_BRIGADE_INSERT_TAIL(out, e);
break;
}
if (APR_BUCKET_IS_FLUSH(e)) {
APR_BUCKET_REMOVE(e);
APR_BRIGADE_INSERT_TAIL(out, e);
break;
}
if (APR_BUCKET_IS_METADATA(e)) {
APR_BUCKET_REMOVE(e);
APR_BRIGADE_INSERT_TAIL(out, e);
continue;
}
rv = apr_bucket_read(e, &str, &length, APR_BLOCK_READ);
APR_BUCKET_REMOVE(e);
APR_BRIGADE_INSERT_TAIL(out, e);
if (rv != APR_SUCCESS) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(02377)
"Error when reading bucket for URL %s",
h->cache_obj->key);
apr_pool_destroy(sobj->pool);
sobj->pool = NULL;
return rv;
}
if (!length) {
continue;
}
sobj->body_length += length;
if (sobj->body_length >= sobj->buffer_len - sobj->body_offset) {
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(02378)
"URL %s failed the buffer size check "
"(%" APR_OFF_T_FMT ">=%" APR_SIZE_T_FMT ")",
h->cache_obj->key, sobj->body_length,
sobj->buffer_len - sobj->body_offset);
apr_pool_destroy(sobj->pool);
sobj->pool = NULL;
return APR_EGENERAL;
}
memcpy(sobj->buffer + sobj->body_offset + sobj->body_length - length,
str, length);
sobj->offset -= length;
if (sobj->offset <= 0) {
sobj->offset = 0;
break;
}
if ((dconf->readtime && apr_time_now() > sobj->timeout)) {
sobj->timeout = 0;
break;
}
}
if (seen_eos) {
const char *cl_header = apr_table_get(r->headers_out, "Content-Length");
if (r->connection->aborted || r->no_cache) {
ap_log_rerror(APLOG_MARK, APLOG_INFO, 0, r, APLOGNO(02380)
"Discarding body for URL %s "
"because connection has been aborted.",
h->cache_obj->key);
apr_pool_destroy(sobj->pool);
sobj->pool = NULL;
return APR_EGENERAL;
}
if (cl_header) {
apr_off_t cl;
char *cl_endp;
if (apr_strtoff(&cl, cl_header, &cl_endp, 10) != APR_SUCCESS
|| *cl_endp != '\0' || cl != sobj->body_length) {
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(02381)
"URL %s didn't receive complete response, not caching",
h->cache_obj->key);
apr_pool_destroy(sobj->pool);
sobj->pool = NULL;
return APR_EGENERAL;
}
}
}
return APR_SUCCESS;
}
static apr_status_t commit_entity(cache_handle_t *h, request_rec *r) {
cache_socache_conf *conf = ap_get_module_config(r->server->module_config,
&cache_socache_module);
cache_object_t *obj = h->cache_obj;
cache_socache_object_t *sobj = (cache_socache_object_t *) obj->vobj;
apr_status_t rv;
if (socache_mutex) {
apr_status_t status = apr_global_mutex_lock(socache_mutex);
if (status != APR_SUCCESS) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, status, r, APLOGNO(02384)
"could not acquire lock, ignoring: %s", obj->key);
apr_pool_destroy(sobj->pool);
sobj->pool = NULL;
return status;
}
}
rv = conf->provider->socache_provider->store(
conf->provider->socache_instance, r->server,
(unsigned char *) sobj->key, strlen(sobj->key), sobj->expire,
sobj->buffer, sobj->body_offset + sobj->body_length, sobj->pool);
if (socache_mutex) {
apr_status_t status = apr_global_mutex_unlock(socache_mutex);
if (status != APR_SUCCESS) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, status, r, APLOGNO(02385)
"could not release lock, ignoring: %s", obj->key);
apr_pool_destroy(sobj->pool);
sobj->pool = NULL;
return status;
}
}
if (rv != APR_SUCCESS) {
ap_log_rerror(APLOG_MARK, APLOG_WARNING, rv, r, APLOGNO(02386)
"could not write to cache, ignoring: %s", sobj->key);
goto fail;
}
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(02387)
"commit_entity: Headers and body for URL %s cached for maximum of %d seconds.",
sobj->name, (apr_uint32_t)apr_time_sec(sobj->expire - r->request_time));
apr_pool_destroy(sobj->pool);
sobj->pool = NULL;
return APR_SUCCESS;
fail:
if (socache_mutex) {
apr_status_t status = apr_global_mutex_lock(socache_mutex);
if (status != APR_SUCCESS) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, status, r, APLOGNO(02388)
"could not acquire lock, ignoring: %s", obj->key);
apr_pool_destroy(sobj->pool);
sobj->pool = NULL;
return rv;
}
}
conf->provider->socache_provider->remove(conf->provider->socache_instance,
r->server, (unsigned char *) sobj->key, strlen(sobj->key), r->pool);
if (socache_mutex) {
apr_status_t status = apr_global_mutex_unlock(socache_mutex);
if (status != APR_SUCCESS) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, status, r, APLOGNO(02389)
"could not release lock, ignoring: %s", obj->key);
}
}
apr_pool_destroy(sobj->pool);
sobj->pool = NULL;
return rv;
}
static apr_status_t invalidate_entity(cache_handle_t *h, request_rec *r) {
h->cache_obj->info.control.invalidated = 1;
return commit_entity(h, r);
}
static void *create_dir_config(apr_pool_t *p, char *dummy) {
cache_socache_dir_conf *dconf =
apr_pcalloc(p, sizeof(cache_socache_dir_conf));
dconf->max = DEFAULT_MAX_FILE_SIZE;
dconf->maxtime = apr_time_from_sec(DEFAULT_MAXTIME);
dconf->mintime = apr_time_from_sec(DEFAULT_MINTIME);
dconf->readsize = DEFAULT_READSIZE;
dconf->readtime = DEFAULT_READTIME;
return dconf;
}
static void *merge_dir_config(apr_pool_t *p, void *basev, void *addv) {
cache_socache_dir_conf
*new =
(cache_socache_dir_conf *) apr_pcalloc(p, sizeof(cache_socache_dir_conf));
cache_socache_dir_conf *add = (cache_socache_dir_conf *) addv;
cache_socache_dir_conf *base = (cache_socache_dir_conf *) basev;
new->max = (add->max_set == 0) ? base->max : add->max;
new->max_set = add->max_set || base->max_set;
new->maxtime = (add->maxtime_set == 0) ? base->maxtime : add->maxtime;
new->maxtime_set = add->maxtime_set || base->maxtime_set;
new->mintime = (add->mintime_set == 0) ? base->mintime : add->mintime;
new->mintime_set = add->mintime_set || base->mintime_set;
new->readsize = (add->readsize_set == 0) ? base->readsize : add->readsize;
new->readsize_set = add->readsize_set || base->readsize_set;
new->readtime = (add->readtime_set == 0) ? base->readtime : add->readtime;
new->readtime_set = add->readtime_set || base->readtime_set;
return new;
}
static void *create_config(apr_pool_t *p, server_rec *s) {
cache_socache_conf *conf = apr_pcalloc(p, sizeof(cache_socache_conf));
return conf;
}
static void *merge_config(apr_pool_t *p, void *basev, void *overridesv) {
cache_socache_conf *ps;
cache_socache_conf *base = (cache_socache_conf *) basev;
cache_socache_conf *overrides = (cache_socache_conf *) overridesv;
ps = overrides ? overrides : base;
return ps;
}
static const char *set_cache_socache(cmd_parms *cmd, void *in_struct_ptr,
const char *arg) {
cache_socache_conf *conf = ap_get_module_config(cmd->server->module_config,
&cache_socache_module);
cache_socache_provider_conf *provider = conf->provider
= apr_pcalloc(cmd->pool, sizeof(cache_socache_provider_conf));
const char *err = NULL, *sep, *name;
sep = ap_strchr_c(arg, ':');
if (sep) {
name = apr_pstrmemdup(cmd->pool, arg, sep - arg);
sep++;
provider->args = sep;
} else {
name = arg;
}
provider->socache_provider = ap_lookup_provider(AP_SOCACHE_PROVIDER_GROUP,
name, AP_SOCACHE_PROVIDER_VERSION);
if (provider->socache_provider == NULL) {
err = apr_psprintf(cmd->pool,
"Unknown socache provider '%s'. Maybe you need "
"to load the appropriate socache module "
"(mod_socache_%s?)", name, name);
}
return err;
}
static const char *set_cache_max(cmd_parms *parms, void *in_struct_ptr,
const char *arg) {
cache_socache_dir_conf *dconf = (cache_socache_dir_conf *) in_struct_ptr;
if (apr_strtoff(&dconf->max, arg, NULL, 10) != APR_SUCCESS
|| dconf->max < 1024 || dconf->max > APR_UINT32_MAX) {
return "CacheSocacheMaxSize argument must be a integer representing "
"the max size of a cached entry (headers and body), at least 1024 "
"and at most " APR_STRINGIFY(APR_UINT32_MAX);
}
dconf->max_set = 1;
return NULL;
}
static const char *set_cache_maxtime(cmd_parms *parms, void *in_struct_ptr,
const char *arg) {
cache_socache_dir_conf *dconf = (cache_socache_dir_conf *) in_struct_ptr;
apr_off_t seconds;
if (apr_strtoff(&seconds, arg, NULL, 10) != APR_SUCCESS || seconds < 0) {
return "CacheSocacheMaxTime argument must be the maximum amount of time in seconds to cache an entry.";
}
dconf->maxtime = apr_time_from_sec(seconds);
dconf->maxtime_set = 1;
return NULL;
}
static const char *set_cache_mintime(cmd_parms *parms, void *in_struct_ptr,
const char *arg) {
cache_socache_dir_conf *dconf = (cache_socache_dir_conf *) in_struct_ptr;
apr_off_t seconds;
if (apr_strtoff(&seconds, arg, NULL, 10) != APR_SUCCESS || seconds < 0) {
return "CacheSocacheMinTime argument must be the minimum amount of time in seconds to cache an entry.";
}
dconf->mintime = apr_time_from_sec(seconds);
dconf->mintime_set = 1;
return NULL;
}
static const char *set_cache_readsize(cmd_parms *parms, void *in_struct_ptr,
const char *arg) {
cache_socache_dir_conf *dconf = (cache_socache_dir_conf *) in_struct_ptr;
if (apr_strtoff(&dconf->readsize, arg, NULL, 10) != APR_SUCCESS
|| dconf->readsize < 0) {
return "CacheSocacheReadSize argument must be a non-negative integer representing the max amount of data to cache in go.";
}
dconf->readsize_set = 1;
return NULL;
}
static const char *set_cache_readtime(cmd_parms *parms, void *in_struct_ptr,
const char *arg) {
cache_socache_dir_conf *dconf = (cache_socache_dir_conf *) in_struct_ptr;
apr_off_t milliseconds;
if (apr_strtoff(&milliseconds, arg, NULL, 10) != APR_SUCCESS
|| milliseconds < 0) {
return "CacheSocacheReadTime argument must be a non-negative integer representing the max amount of time taken to cache in go.";
}
dconf->readtime = apr_time_from_msec(milliseconds);
dconf->readtime_set = 1;
return NULL;
}
static apr_status_t remove_lock(void *data) {
if (socache_mutex) {
apr_global_mutex_destroy(socache_mutex);
socache_mutex = NULL;
}
return APR_SUCCESS;
}
static apr_status_t destroy_cache(void *data) {
server_rec *s = data;
cache_socache_conf *conf =
ap_get_module_config(s->module_config, &cache_socache_module);
if (conf->provider && conf->provider->socache_instance) {
conf->provider->socache_provider->destroy(
conf->provider->socache_instance, s);
conf->provider->socache_instance = NULL;
}
return APR_SUCCESS;
}
static int socache_status_hook(request_rec *r, int flags) {
apr_status_t status = APR_SUCCESS;
cache_socache_conf *conf = ap_get_module_config(r->server->module_config,
&cache_socache_module);
if (!conf->provider || !conf->provider->socache_provider ||
!conf->provider->socache_instance) {
return DECLINED;
}
if (!(flags & AP_STATUS_SHORT)) {
ap_rputs("<hr>\n"
"<table cellspacing=0 cellpadding=0>\n"
"<tr><td bgcolor=\"#000000\">\n"
"<b><font color=\"#ffffff\" face=\"Arial,Helvetica\">"
"mod_cache_socache Status:</font></b>\n"
"</td></tr>\n"
"<tr><td bgcolor=\"#ffffff\">\n", r);
} else {
ap_rputs("ModCacheSocacheStatus\n", r);
}
if (socache_mutex) {
status = apr_global_mutex_lock(socache_mutex);
if (status != APR_SUCCESS) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, status, r, APLOGNO(02816)
"could not acquire lock for cache status");
}
}
if (status != APR_SUCCESS) {
if (!(flags & AP_STATUS_SHORT)) {
ap_rputs("No cache status data available\n", r);
} else {
ap_rputs("NotAvailable\n", r);
}
} else {
conf->provider->socache_provider->status(conf->provider->socache_instance,
r, flags);
}
if (socache_mutex && status == APR_SUCCESS) {
status = apr_global_mutex_unlock(socache_mutex);
if (status != APR_SUCCESS) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, status, r, APLOGNO(02817)
"could not release lock for cache status");
}
}
if (!(flags & AP_STATUS_SHORT)) {
ap_rputs("</td></tr>\n</table>\n", r);
}
return OK;
}
static void socache_status_register(apr_pool_t *p) {
APR_OPTIONAL_HOOK(ap, status_hook, socache_status_hook, NULL, NULL, APR_HOOK_MIDDLE);
}
static int socache_precfg(apr_pool_t *pconf, apr_pool_t *plog, apr_pool_t *ptmp) {
apr_status_t rv = ap_mutex_register(pconf, cache_socache_id, NULL,
APR_LOCK_DEFAULT, 0);
if (rv != APR_SUCCESS) {
ap_log_perror(APLOG_MARK, APLOG_CRIT, rv, plog, APLOGNO(02390)
"failed to register %s mutex", cache_socache_id);
return 500;
}
socache_status_register(pconf);
return OK;
}
static int socache_post_config(apr_pool_t *pconf, apr_pool_t *plog,
apr_pool_t *ptmp, server_rec *base_server) {
server_rec *s;
apr_status_t rv;
const char *errmsg;
static struct ap_socache_hints socache_hints =
{ 64, 2048, 60000000 };
for (s = base_server; s; s = s->next) {
cache_socache_conf *conf =
ap_get_module_config(s->module_config, &cache_socache_module);
if (!conf->provider) {
continue;
}
if (!socache_mutex && conf->provider->socache_provider->flags
& AP_SOCACHE_FLAG_NOTMPSAFE) {
rv = ap_global_mutex_create(&socache_mutex, NULL, cache_socache_id,
NULL, s, pconf, 0);
if (rv != APR_SUCCESS) {
ap_log_perror(APLOG_MARK, APLOG_CRIT, rv, plog, APLOGNO(02391)
"failed to create %s mutex", cache_socache_id);
return 500;
}
apr_pool_cleanup_register(pconf, NULL, remove_lock,
apr_pool_cleanup_null);
}
errmsg = conf->provider->socache_provider->create(
&conf->provider->socache_instance, conf->provider->args, ptmp,
pconf);
if (errmsg) {
ap_log_perror(APLOG_MARK, APLOG_CRIT, 0, plog,
APLOGNO(02392) "%s", errmsg);
return 500;
}
rv = conf->provider->socache_provider->init(
conf->provider->socache_instance, cache_socache_id,
&socache_hints, s, pconf);
if (rv != APR_SUCCESS) {
ap_log_perror(APLOG_MARK, APLOG_CRIT, rv, plog, APLOGNO(02393)
"failed to initialise %s cache", cache_socache_id);
return 500;
}
apr_pool_cleanup_register(pconf, (void *) s, destroy_cache,
apr_pool_cleanup_null);
}
return OK;
}
static void socache_child_init(apr_pool_t *p, server_rec *s) {
const char *lock;
apr_status_t rv;
if (!socache_mutex) {
return;
}
lock = apr_global_mutex_lockfile(socache_mutex);
rv = apr_global_mutex_child_init(&socache_mutex, lock, p);
if (rv != APR_SUCCESS) {
ap_log_error(APLOG_MARK, APLOG_CRIT, rv, s, APLOGNO(02394)
"failed to initialise mutex in child_init");
}
}
static const command_rec cache_socache_cmds[] = {
AP_INIT_TAKE1("CacheSocache", set_cache_socache, NULL, RSRC_CONF,
"The shared object cache to store cache files"),
AP_INIT_TAKE1("CacheSocacheMaxTime", set_cache_maxtime, NULL, RSRC_CONF | ACCESS_CONF,
"The maximum cache expiry age to cache a document in seconds"),
AP_INIT_TAKE1("CacheSocacheMinTime", set_cache_mintime, NULL, RSRC_CONF | ACCESS_CONF,
"The minimum cache expiry age to cache a document in seconds"),
AP_INIT_TAKE1("CacheSocacheMaxSize", set_cache_max, NULL, RSRC_CONF | ACCESS_CONF,
"The maximum cache entry size (headers and body) to cache a document"),
AP_INIT_TAKE1("CacheSocacheReadSize", set_cache_readsize, NULL, RSRC_CONF | ACCESS_CONF,
"The maximum quantity of data to attempt to read and cache in one go"),
AP_INIT_TAKE1("CacheSocacheReadTime", set_cache_readtime, NULL, RSRC_CONF | ACCESS_CONF,
"The maximum time taken to attempt to read and cache in go"),
{ NULL }
};
static const cache_provider cache_socache_provider = {
&remove_entity, &store_headers, &store_body, &recall_headers, &recall_body,
&create_entity, &open_entity, &remove_url, &commit_entity,
&invalidate_entity
};
static void cache_socache_register_hook(apr_pool_t *p) {
ap_register_provider(p, CACHE_PROVIDER_GROUP, "socache", "0",
&cache_socache_provider);
ap_hook_pre_config(socache_precfg, NULL, NULL, APR_HOOK_MIDDLE);
ap_hook_post_config(socache_post_config, NULL, NULL, APR_HOOK_MIDDLE);
ap_hook_child_init(socache_child_init, NULL, NULL, APR_HOOK_MIDDLE);
}
AP_DECLARE_MODULE(cache_socache) = { STANDARD20_MODULE_STUFF,
create_dir_config,
merge_dir_config,
create_config,
merge_config,
cache_socache_cmds,
cache_socache_register_hook
};
