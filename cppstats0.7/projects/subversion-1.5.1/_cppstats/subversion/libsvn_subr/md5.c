#include "svn_md5.h"
static const unsigned char svn_md5__empty_string_digest[] = {
212, 29, 140, 217, 143, 0, 178, 4, 233, 128, 9, 152, 236, 248, 66, 126
};
const unsigned char *
svn_md5_empty_string_digest(void) {
return svn_md5__empty_string_digest;
}
const char *
svn_md5_digest_to_cstring_display(const unsigned char digest[],
apr_pool_t *pool) {
static const char *hex = "0123456789abcdef";
char *str = apr_palloc(pool, (APR_MD5_DIGESTSIZE * 2) + 1);
int i;
for (i = 0; i < APR_MD5_DIGESTSIZE; i++) {
str[i*2] = hex[digest[i] >> 4];
str[i*2+1] = hex[digest[i] & 0x0f];
}
str[i*2] = '\0';
return str;
}
const char *
svn_md5_digest_to_cstring(const unsigned char digest[], apr_pool_t *pool) {
static const unsigned char zeros_digest[APR_MD5_DIGESTSIZE] = { 0 };
if (memcmp(digest, zeros_digest, APR_MD5_DIGESTSIZE) != 0)
return svn_md5_digest_to_cstring_display(digest, pool);
else
return NULL;
}
svn_boolean_t
svn_md5_digests_match(const unsigned char d1[], const unsigned char d2[]) {
static const unsigned char zeros[APR_MD5_DIGESTSIZE] = { 0 };
return ((memcmp(d1, zeros, APR_MD5_DIGESTSIZE) == 0)
|| (memcmp(d2, zeros, APR_MD5_DIGESTSIZE) == 0)
|| (memcmp(d1, d2, APR_MD5_DIGESTSIZE) == 0));
}
