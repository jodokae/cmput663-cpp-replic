#if !defined(CACHE_COMMON_H)
#define CACHE_COMMON_H
typedef struct cache_control {
unsigned int parsed:1;
unsigned int cache_control:1;
unsigned int pragma:1;
unsigned int no_cache:1;
unsigned int no_cache_header:1;
unsigned int no_store:1;
unsigned int max_age:1;
unsigned int max_stale:1;
unsigned int min_fresh:1;
unsigned int no_transform:1;
unsigned int only_if_cached:1;
unsigned int public:1;
unsigned int private:1;
unsigned int private_header:1;
unsigned int must_revalidate:1;
unsigned int proxy_revalidate:1;
unsigned int s_maxage:1;
unsigned int invalidated:1;
apr_int64_t max_age_value;
apr_int64_t max_stale_value;
apr_int64_t min_fresh_value;
apr_int64_t s_maxage_value;
} cache_control_t;
#endif
