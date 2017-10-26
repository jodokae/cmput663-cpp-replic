#if !defined(SVN_MD5_H)
#define SVN_MD5_H
#include <apr_pools.h>
#include <apr_md5.h>
#include "svn_error.h"
#include "svn_pools.h"
#if defined(__cplusplus)
extern "C" {
#endif
const unsigned char *svn_md5_empty_string_digest(void);
const char *svn_md5_digest_to_cstring_display(const unsigned char digest[],
apr_pool_t *pool);
const char *svn_md5_digest_to_cstring(const unsigned char digest[],
apr_pool_t *pool);
svn_boolean_t svn_md5_digests_match(const unsigned char d1[],
const unsigned char d2[]);
#if defined(__cplusplus)
}
#endif
#endif
