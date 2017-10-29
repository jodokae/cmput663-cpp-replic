#if !defined(APACHE_UTIL_MD5_H)
#define APACHE_UTIL_MD5_H
#if defined(__cplusplus)
extern "C" {
#endif
#include "apr_md5.h"
AP_DECLARE(char *) ap_md5(apr_pool_t *a, const unsigned char *string);
AP_DECLARE(char *) ap_md5_binary(apr_pool_t *a, const unsigned char *buf, int len);
AP_DECLARE(char *) ap_md5contextTo64(apr_pool_t *p, apr_md5_ctx_t *context);
AP_DECLARE(char *) ap_md5digest(apr_pool_t *p, apr_file_t *infile);
#if defined(__cplusplus)
}
#endif
#endif
