#if !defined(AP_REGKEY_H)
#define AP_REGKEY_H
#if defined(WIN32) || defined(DOXYGEN)
#include "apr.h"
#include "apr_pools.h"
#include "ap_config.h"
#if defined(__cplusplus)
extern "C" {
#endif
typedef struct ap_regkey_t ap_regkey_t;
AP_DECLARE(const ap_regkey_t *) ap_regkey_const(int i);
#define AP_REGKEY_CLASSES_ROOT ap_regkey_const(0)
#define AP_REGKEY_CURRENT_CONFIG ap_regkey_const(1)
#define AP_REGKEY_CURRENT_USER ap_regkey_const(2)
#define AP_REGKEY_LOCAL_MACHINE ap_regkey_const(3)
#define AP_REGKEY_USERS ap_regkey_const(4)
#define AP_REGKEY_PERFORMANCE_DATA ap_regkey_const(5)
#define AP_REGKEY_DYN_DATA ap_regkey_const(6)
#define AP_REGKEY_EXPAND 0x0001
AP_DECLARE(apr_status_t) ap_regkey_open(ap_regkey_t **newkey,
const ap_regkey_t *parentkey,
const char *keyname,
apr_int32_t flags,
apr_pool_t *pool);
AP_DECLARE(apr_status_t) ap_regkey_close(ap_regkey_t *key);
AP_DECLARE(apr_status_t) ap_regkey_remove(const ap_regkey_t *parent,
const char *keyname,
apr_pool_t *pool);
AP_DECLARE(apr_status_t) ap_regkey_value_get(char **result,
ap_regkey_t *key,
const char *valuename,
apr_pool_t *pool);
AP_DECLARE(apr_status_t) ap_regkey_value_set(ap_regkey_t *key,
const char *valuename,
const char *value,
apr_int32_t flags,
apr_pool_t *pool);
AP_DECLARE(apr_status_t) ap_regkey_value_raw_get(void **result,
apr_size_t *resultsize,
apr_int32_t *resulttype,
ap_regkey_t *key,
const char *valuename,
apr_pool_t *pool);
AP_DECLARE(apr_status_t) ap_regkey_value_raw_set(ap_regkey_t *key,
const char *valuename,
const void *value,
apr_size_t valuesize,
apr_int32_t valuetype,
apr_pool_t *pool);
AP_DECLARE(apr_status_t) ap_regkey_value_array_get(apr_array_header_t **result,
ap_regkey_t *key,
const char *valuename,
apr_pool_t *pool);
AP_DECLARE(apr_status_t) ap_regkey_value_array_set(ap_regkey_t *key,
const char *valuename,
int nelts,
const char * const * elts,
apr_pool_t *pool);
AP_DECLARE(apr_status_t) ap_regkey_value_remove(const ap_regkey_t *key,
const char *valuename,
apr_pool_t *pool);
#if defined(__cplusplus)
}
#endif
#endif
#endif