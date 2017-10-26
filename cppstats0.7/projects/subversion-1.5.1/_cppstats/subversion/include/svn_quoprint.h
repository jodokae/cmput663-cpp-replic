#if !defined(SVN_QUOPRINT_H)
#define SVN_QUOPRINT_H
#include "svn_io.h"
#if defined(__cplusplus)
extern "C" {
#endif
svn_stream_t *svn_quoprint_encode(svn_stream_t *output, apr_pool_t *pool);
svn_stream_t *svn_quoprint_decode(svn_stream_t *output, apr_pool_t *pool);
svn_stringbuf_t *svn_quoprint_encode_string(svn_stringbuf_t *str,
apr_pool_t *pool);
svn_stringbuf_t *svn_quoprint_decode_string(svn_stringbuf_t *str,
apr_pool_t *pool);
#if defined(__cplusplus)
}
#endif
#endif
