#include <apr_hash.h>
#include "svn_types.h"
#include "svn_error.h"
#include "svn_pools.h"
typedef svn_error_t *(*svn_iter_apr_hash_cb_t)(void *baton,
const void *key,
apr_ssize_t klen,
void *val, apr_pool_t *pool);
svn_error_t *
svn_iter_apr_hash(svn_boolean_t *completed,
apr_hash_t *hash,
svn_iter_apr_hash_cb_t func,
void *baton,
apr_pool_t *pool);
typedef svn_error_t *(*svn_iter_apr_array_cb_t)(void *baton,
void *item,
apr_pool_t *pool);
svn_error_t *
svn_iter_apr_array(svn_boolean_t *completed,
const apr_array_header_t *array,
svn_iter_apr_array_cb_t func,
void *baton,
apr_pool_t *pool);
svn_error_t *
svn_iter__break(void);
#define svn_iter_break(pool) return svn_iter__break()
