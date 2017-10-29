#if !defined(CACHE_SOCACHE_COMMON_H)
#define CACHE_SOCACHE_COMMON_H
#include "apr_time.h"
#include "cache_common.h"
#define CACHE_SOCACHE_VARY_FORMAT_VERSION 1
#define CACHE_SOCACHE_DISK_FORMAT_VERSION 2
typedef struct {
apr_uint32_t format;
int status;
apr_size_t name_len;
apr_size_t entity_version;
apr_time_t date;
apr_time_t expire;
apr_time_t request_time;
apr_time_t response_time;
unsigned int header_only:1;
cache_control_t control;
} cache_socache_info_t;
#endif
