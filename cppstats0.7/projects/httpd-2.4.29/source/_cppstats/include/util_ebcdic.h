#if !defined(APACHE_UTIL_EBCDIC_H)
#define APACHE_UTIL_EBCDIC_H
#if defined(__cplusplus)
extern "C" {
#endif
#include "apr_xlate.h"
#include "httpd.h"
#include "util_charset.h"
#if APR_CHARSET_EBCDIC || defined(DOXYGEN)
apr_status_t ap_init_ebcdic(apr_pool_t *pool);
void ap_xlate_proto_to_ascii(char *buffer, apr_size_t len);
void ap_xlate_proto_from_ascii(char *buffer, apr_size_t len);
int ap_rvputs_proto_in_ascii(request_rec *r, ...);
#else
#define ap_xlate_proto_to_ascii(x,y)
#define ap_xlate_proto_from_ascii(x,y)
#define ap_rvputs_proto_in_ascii ap_rvputs
#endif
#if defined(__cplusplus)
}
#endif
#endif
