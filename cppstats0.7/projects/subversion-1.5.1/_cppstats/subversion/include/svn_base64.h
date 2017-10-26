#if !defined(SVN_BASE64_H)
#define SVN_BASE64_H
#include <apr_md5.h>
#include "svn_io.h"
#if defined(__cplusplus)
extern "C" {
#endif
svn_stream_t *svn_base64_encode(svn_stream_t *output, apr_pool_t *pool);
svn_stream_t *svn_base64_decode(svn_stream_t *output, apr_pool_t *pool);
const svn_string_t *svn_base64_encode_string(const svn_string_t *str,
apr_pool_t *pool);
const svn_string_t *svn_base64_decode_string(const svn_string_t *str,
apr_pool_t *pool);
svn_stringbuf_t *svn_base64_from_md5(unsigned char digest[],
apr_pool_t *pool);
#if defined(__cplusplus)
}
#endif
#endif
