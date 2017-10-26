#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <apr_general.h>
#include <apr_pools.h>
#include "svn_pools.h"
#if APR_POOL_DEBUG
static const char SVN_FILE_LINE_UNDEFINED[] = "svn:<undefined>";
#endif
static int
abort_on_pool_failure(int retcode) {
printf("Out of memory - terminating application.\n");
abort();
return -1;
}
#if APR_POOL_DEBUG
#undef svn_pool_create_ex
#endif
#if !APR_POOL_DEBUG
apr_pool_t *
svn_pool_create_ex(apr_pool_t *parent_pool, apr_allocator_t *allocator) {
apr_pool_t *pool;
apr_pool_create_ex(&pool, parent_pool, abort_on_pool_failure, allocator);
return pool;
}
apr_pool_t *
svn_pool_create_ex_debug(apr_pool_t *pool, apr_allocator_t *allocator,
const char *file_line) {
return svn_pool_create_ex(pool, allocator);
}
#else
apr_pool_t *
svn_pool_create_ex_debug(apr_pool_t *parent_pool, apr_allocator_t *allocator,
const char *file_line) {
apr_pool_t *pool;
apr_pool_create_ex_debug(&pool, parent_pool, abort_on_pool_failure,
allocator, file_line);
return pool;
}
apr_pool_t *
svn_pool_create_ex(apr_pool_t *pool, apr_allocator_t *allocator) {
return svn_pool_create_ex_debug(pool, allocator, SVN_FILE_LINE_UNDEFINED);
}
#endif
