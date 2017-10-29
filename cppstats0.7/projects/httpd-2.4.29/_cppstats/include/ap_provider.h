#if !defined(AP_PROVIDER_H)
#define AP_PROVIDER_H
#include "ap_config.h"
#if defined(__cplusplus)
extern "C" {
#endif
typedef struct {
const char *provider_name;
} ap_list_provider_names_t;
typedef struct {
const char *provider_group;
const char *provider_version;
} ap_list_provider_groups_t;
AP_DECLARE(apr_status_t) ap_register_provider(apr_pool_t *pool,
const char *provider_group,
const char *provider_name,
const char *provider_version,
const void *provider);
AP_DECLARE(void *) ap_lookup_provider(const char *provider_group,
const char *provider_name,
const char *provider_version);
AP_DECLARE(apr_array_header_t *) ap_list_provider_names(apr_pool_t *pool,
const char *provider_group,
const char *provider_version);
AP_DECLARE(apr_array_header_t *) ap_list_provider_groups(apr_pool_t *pool);
#if defined(__cplusplus)
}
#endif
#endif