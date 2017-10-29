#if !defined(MOD_REQUEST_H)
#define MOD_REQUEST_H
#include "apr.h"
#include "apr_buckets.h"
#include "apr_optional.h"
#include "httpd.h"
#include "util_filter.h"
#if defined(__cplusplus)
extern "C" {
#endif
extern module AP_MODULE_DECLARE_DATA request_module;
#define KEEP_BODY_FILTER "KEEP_BODY"
#define KEPT_BODY_FILTER "KEPT_BODY"
typedef struct {
apr_off_t keep_body;
int keep_body_set;
} request_dir_conf;
APR_DECLARE_OPTIONAL_FN(void, ap_request_insert_filter, (request_rec * r));
APR_DECLARE_OPTIONAL_FN(void, ap_request_remove_filter, (request_rec * r));
#if defined(__cplusplus)
}
#endif
#endif
