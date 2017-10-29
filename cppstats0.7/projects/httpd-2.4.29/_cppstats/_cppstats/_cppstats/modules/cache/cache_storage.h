#if !defined(CACHE_STORAGE_H)
#define CACHE_STORAGE_H
#if defined(__cplusplus)
extern "C" {
#endif
#include "mod_cache.h"
#include "cache_util.h"
int cache_remove_url(cache_request_rec *cache, request_rec *r);
int cache_create_entity(cache_request_rec *cache, request_rec *r,
apr_off_t size, apr_bucket_brigade *in);
int cache_select(cache_request_rec *cache, request_rec *r);
int cache_invalidate(cache_request_rec *cache, request_rec *r);
apr_status_t cache_generate_key_default(request_rec *r, apr_pool_t* p,
const char **key);
void cache_accept_headers(cache_handle_t *h, request_rec *r, apr_table_t *top,
apr_table_t *bottom, int revalidation);
#if defined(__cplusplus)
}
#endif
#endif