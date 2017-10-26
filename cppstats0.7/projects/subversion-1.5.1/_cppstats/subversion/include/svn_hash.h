#if !defined(SVN_HASH_H)
#define SVN_HASH_H
#include <apr_pools.h>
#include <apr_hash.h>
#include <apr_file_io.h>
#include "svn_types.h"
#include "svn_io.h"
#include "svn_error.h"
#if defined(__cplusplus)
extern "C" {
#endif
#define SVN_KEYLINE_MAXLEN 100
#define SVN_HASH_TERMINATOR "END"
svn_error_t *svn_hash_read2(apr_hash_t *hash,
svn_stream_t *stream,
const char *terminator,
apr_pool_t *pool);
svn_error_t *svn_hash_write2(apr_hash_t *hash,
svn_stream_t *stream,
const char *terminator,
apr_pool_t *pool);
svn_error_t *svn_hash_read_incremental(apr_hash_t *hash,
svn_stream_t *stream,
const char *terminator,
apr_pool_t *pool);
svn_error_t *svn_hash_write_incremental(apr_hash_t *hash,
apr_hash_t *oldhash,
svn_stream_t *stream,
const char *terminator,
apr_pool_t *pool);
svn_error_t *svn_hash_read(apr_hash_t *hash,
apr_file_t *srcfile,
apr_pool_t *pool);
svn_error_t *svn_hash_write(apr_hash_t *hash,
apr_file_t *destfile,
apr_pool_t *pool);
enum svn_hash_diff_key_status {
svn_hash_diff_key_both,
svn_hash_diff_key_a,
svn_hash_diff_key_b
};
typedef svn_error_t *(*svn_hash_diff_func_t)
(const void *key, apr_ssize_t klen,
enum svn_hash_diff_key_status status,
void *baton);
svn_error_t *svn_hash_diff(apr_hash_t *hash_a,
apr_hash_t *hash_b,
svn_hash_diff_func_t diff_func,
void *diff_func_baton,
apr_pool_t *pool);
svn_error_t *svn_hash_keys(apr_array_header_t **array,
apr_hash_t *hash,
apr_pool_t *pool);
svn_error_t *svn_hash_from_cstring_keys(apr_hash_t **hash,
const apr_array_header_t *keys,
apr_pool_t *pool);
svn_error_t *svn_hash__clear(apr_hash_t *hash);
#if defined(__cplusplus)
}
#endif
#endif
