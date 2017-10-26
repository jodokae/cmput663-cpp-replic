#include <apr_pools.h>
#include <apr_hash.h>
#include "svn_delta.h"
#if !defined(SVN_LIBSVN_DELTA_H)
#define SVN_LIBSVN_DELTA_H
#if defined(__cplusplus)
extern "C" {
#endif
#define SVN_DELTA_WINDOW_SIZE 102400
typedef struct svn_txdelta__ops_baton_t {
int num_ops;
int src_ops;
int ops_size;
svn_txdelta_op_t *ops;
svn_stringbuf_t *new_data;
} svn_txdelta__ops_baton_t;
void svn_txdelta__insert_op(svn_txdelta__ops_baton_t *build_baton,
enum svn_delta_action opcode,
apr_size_t offset,
apr_size_t length,
const char *new_data,
apr_pool_t *pool);
svn_txdelta_window_t *
svn_txdelta__make_window(const svn_txdelta__ops_baton_t *build_baton,
apr_pool_t *pool);
void svn_txdelta__vdelta(svn_txdelta__ops_baton_t *build_baton,
const char *start,
apr_size_t source_len,
apr_size_t target_len,
apr_pool_t *pool);
void svn_txdelta__xdelta(svn_txdelta__ops_baton_t *build_baton,
const char *start,
apr_size_t source_len,
apr_size_t target_len,
apr_pool_t *pool);
#if defined(__cplusplus)
}
#endif
#endif
