#if !defined(SVN_POOLS_H)
#define SVN_POOLS_H
#include <apr.h>
#include <apr_errno.h>
#include <apr_pools.h>
#include "svn_types.h"
#if defined(__cplusplus)
extern "C" {
#endif
#define SVN_ALLOCATOR_RECOMMENDED_MAX_FREE (4096 * 1024)
apr_pool_t *svn_pool_create_ex(apr_pool_t *parent_pool,
apr_allocator_t *allocator);
#if !defined(DOXYGEN_SHOULD_SKIP_THIS)
apr_pool_t *svn_pool_create_ex_debug(apr_pool_t *parent_pool,
apr_allocator_t *allocator,
const char *file_line);
#if APR_POOL_DEBUG
#define svn_pool_create_ex(pool, allocator) svn_pool_create_ex_debug(pool, allocator, APR_POOL__FILE_LINE__)
#endif
#endif
#define svn_pool_create(parent_pool) svn_pool_create_ex(parent_pool, NULL)
#define svn_pool_clear apr_pool_clear
#define svn_pool_destroy apr_pool_destroy
#if defined(__cplusplus)
}
#endif
#endif
