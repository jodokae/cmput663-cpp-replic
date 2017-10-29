#if !defined(SLOTMEM_H)
#define SLOTMEM_H
#include "httpd.h"
#include "http_config.h"
#include "http_log.h"
#include "ap_provider.h"
#include "apr.h"
#include "apr_strings.h"
#include "apr_pools.h"
#include "apr_shm.h"
#include "apr_global_mutex.h"
#include "apr_file_io.h"
#include "apr_md5.h"
#if APR_HAVE_UNISTD_H
#include <unistd.h>
#endif
#if defined(__cplusplus)
extern "C" {
#endif
#define AP_SLOTMEM_PROVIDER_GROUP "slotmem"
#define AP_SLOTMEM_PROVIDER_VERSION "0"
typedef unsigned int ap_slotmem_type_t;
#define AP_SLOTMEM_TYPE_PERSIST (1 << 0)
#define AP_SLOTMEM_TYPE_NOTMPSAFE (1 << 1)
#define AP_SLOTMEM_TYPE_PREGRAB (1 << 2)
#define AP_SLOTMEM_TYPE_CLEARINUSE (1 << 3)
typedef struct ap_slotmem_instance_t ap_slotmem_instance_t;
typedef apr_status_t ap_slotmem_callback_fn_t(void* mem, void *data, apr_pool_t *pool);
struct ap_slotmem_provider_t {
const char *name;
apr_status_t (* doall)(ap_slotmem_instance_t *s, ap_slotmem_callback_fn_t *func, void *data, apr_pool_t *pool);
apr_status_t (* create)(ap_slotmem_instance_t **inst, const char *name, apr_size_t item_size, unsigned int item_num, ap_slotmem_type_t type, apr_pool_t *pool);
apr_status_t (* attach)(ap_slotmem_instance_t **inst, const char *name, apr_size_t *item_size, unsigned int *item_num, apr_pool_t *pool);
apr_status_t (* dptr)(ap_slotmem_instance_t *s, unsigned int item_id, void**mem);
apr_status_t (* get)(ap_slotmem_instance_t *s, unsigned int item_id, unsigned char *dest, apr_size_t dest_len);
apr_status_t (* put)(ap_slotmem_instance_t *slot, unsigned int item_id, unsigned char *src, apr_size_t src_len);
unsigned int (* num_slots)(ap_slotmem_instance_t *s);
unsigned int (* num_free_slots)(ap_slotmem_instance_t *s);
apr_size_t (* slot_size)(ap_slotmem_instance_t *s);
apr_status_t (* grab)(ap_slotmem_instance_t *s, unsigned int *item_id);
apr_status_t (* release)(ap_slotmem_instance_t *s, unsigned int item_id);
apr_status_t (* fgrab)(ap_slotmem_instance_t *s, unsigned int item_id);
};
typedef struct ap_slotmem_provider_t ap_slotmem_provider_t;
#if defined(__cplusplus)
}
#endif
#endif