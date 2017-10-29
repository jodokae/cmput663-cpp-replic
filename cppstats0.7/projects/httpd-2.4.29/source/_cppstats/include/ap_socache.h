#if !defined(AP_SOCACHE_H)
#define AP_SOCACHE_H
#include "httpd.h"
#include "ap_provider.h"
#include "apr_pools.h"
#include "apr_time.h"
#if defined(__cplusplus)
extern "C" {
#endif
#define AP_SOCACHE_FLAG_NOTMPSAFE (0x0001)
typedef struct ap_socache_instance_t ap_socache_instance_t;
struct ap_socache_hints {
apr_size_t avg_id_len;
apr_size_t avg_obj_size;
apr_interval_time_t expiry_interval;
};
typedef apr_status_t (ap_socache_iterator_t)(ap_socache_instance_t *instance,
server_rec *s,
void *userctx,
const unsigned char *id,
unsigned int idlen,
const unsigned char *data,
unsigned int datalen,
apr_pool_t *pool);
typedef struct ap_socache_provider_t {
const char *name;
unsigned int flags;
const char *(*create)(ap_socache_instance_t **instance, const char *arg,
apr_pool_t *tmp, apr_pool_t *p);
apr_status_t (*init)(ap_socache_instance_t *instance, const char *cname,
const struct ap_socache_hints *hints,
server_rec *s, apr_pool_t *pool);
void (*destroy)(ap_socache_instance_t *instance, server_rec *s);
apr_status_t (*store)(ap_socache_instance_t *instance, server_rec *s,
const unsigned char *id, unsigned int idlen,
apr_time_t expiry,
unsigned char *data, unsigned int datalen,
apr_pool_t *pool);
apr_status_t (*retrieve)(ap_socache_instance_t *instance, server_rec *s,
const unsigned char *id, unsigned int idlen,
unsigned char *data, unsigned int *datalen,
apr_pool_t *pool);
apr_status_t (*remove)(ap_socache_instance_t *instance, server_rec *s,
const unsigned char *id, unsigned int idlen,
apr_pool_t *pool);
void (*status)(ap_socache_instance_t *instance, request_rec *r, int flags);
apr_status_t (*iterate)(ap_socache_instance_t *instance, server_rec *s,
void *userctx, ap_socache_iterator_t *iterator,
apr_pool_t *pool);
} ap_socache_provider_t;
#define AP_SOCACHE_PROVIDER_GROUP "socache"
#define AP_SOCACHE_PROVIDER_VERSION "0"
#define AP_SOCACHE_DEFAULT_PROVIDER "default"
#if defined(__cplusplus)
}
#endif
#endif