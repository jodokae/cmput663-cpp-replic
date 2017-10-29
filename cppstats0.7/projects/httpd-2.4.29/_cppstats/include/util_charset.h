#if !defined(APACHE_UTIL_CHARSET_H)
#define APACHE_UTIL_CHARSET_H
#if defined(__cplusplus)
extern "C" {
#endif
#include "apr.h"
#if APR_CHARSET_EBCDIC || defined(DOXYGEN)
#include "apr_xlate.h"
extern apr_xlate_t *ap_hdrs_to_ascii;
extern apr_xlate_t *ap_hdrs_from_ascii;
#endif
#if defined(__cplusplus)
}
#endif
#endif