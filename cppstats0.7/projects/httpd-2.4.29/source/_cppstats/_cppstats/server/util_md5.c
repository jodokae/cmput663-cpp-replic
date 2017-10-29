#include "ap_config.h"
#include "apr_portable.h"
#include "apr_strings.h"
#include "httpd.h"
#include "util_md5.h"
#include "util_ebcdic.h"
AP_DECLARE(char *) ap_md5_binary(apr_pool_t *p, const unsigned char *buf, int length) {
apr_md5_ctx_t my_md5;
unsigned char hash[APR_MD5_DIGESTSIZE];
char result[2 * APR_MD5_DIGESTSIZE + 1];
apr_md5_init(&my_md5);
#if APR_CHARSET_EBCDIC
apr_md5_set_xlate(&my_md5, ap_hdrs_to_ascii);
#endif
apr_md5_update(&my_md5, buf, (unsigned int)length);
apr_md5_final(hash, &my_md5);
ap_bin2hex(hash, APR_MD5_DIGESTSIZE, result);
return apr_pstrndup(p, result, APR_MD5_DIGESTSIZE*2);
}
AP_DECLARE(char *) ap_md5(apr_pool_t *p, const unsigned char *string) {
return ap_md5_binary(p, string, (int) strlen((char *)string));
}
static char basis_64[] =
"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
AP_DECLARE(char *) ap_md5contextTo64(apr_pool_t *a, apr_md5_ctx_t *context) {
unsigned char digest[18];
char *encodedDigest;
int i;
char *p;
encodedDigest = (char *) apr_pcalloc(a, 25 * sizeof(char));
apr_md5_final(digest, context);
digest[sizeof(digest) - 1] = digest[sizeof(digest) - 2] = 0;
p = encodedDigest;
for (i = 0; i < sizeof(digest); i += 3) {
*p++ = basis_64[digest[i] >> 2];
*p++ = basis_64[((digest[i] & 0x3) << 4) | ((int) (digest[i + 1] & 0xF0) >> 4)];
*p++ = basis_64[((digest[i + 1] & 0xF) << 2) | ((int) (digest[i + 2] & 0xC0) >> 6)];
*p++ = basis_64[digest[i + 2] & 0x3F];
}
*p-- = '\0';
*p-- = '=';
*p-- = '=';
return encodedDigest;
}
AP_DECLARE(char *) ap_md5digest(apr_pool_t *p, apr_file_t *infile) {
apr_md5_ctx_t context;
unsigned char buf[4096];
apr_size_t nbytes;
apr_off_t offset = 0L;
apr_md5_init(&context);
nbytes = sizeof(buf);
while (apr_file_read(infile, buf, &nbytes) == APR_SUCCESS) {
apr_md5_update(&context, buf, nbytes);
nbytes = sizeof(buf);
}
apr_file_seek(infile, APR_SET, &offset);
return ap_md5contextTo64(p, &context);
}