#include "svn_iter.h"
#include "svn_error_codes.h"
static svn_error_t internal_break_error = {
SVN_ERR_ITER_BREAK,
NULL,
NULL,
NULL,
__FILE__,
__LINE__
};
svn_error_t *
svn_iter_apr_hash(svn_boolean_t *completed,
apr_hash_t *hash,
svn_iter_apr_hash_cb_t func,
void *baton,
apr_pool_t *pool) {
svn_error_t *err = SVN_NO_ERROR;
apr_pool_t *iterpool = svn_pool_create(pool);
apr_hash_index_t *hi;
for (hi = apr_hash_first(pool, hash);
! err && hi; hi = apr_hash_next(hi)) {
const void *key;
void *val;
apr_ssize_t len;
svn_pool_clear(iterpool);
apr_hash_this(hi, &key, &len, &val);
err = (*func)(baton, key, len, val, iterpool);
}
if (completed)
*completed = ! err;
if (err && err->apr_err == SVN_ERR_ITER_BREAK) {
if (err != &internal_break_error)
svn_error_clear(err);
err = SVN_NO_ERROR;
}
svn_pool_destroy(iterpool);
return err;
}
svn_error_t *
svn_iter_apr_array(svn_boolean_t *completed,
const apr_array_header_t *array,
svn_iter_apr_array_cb_t func,
void *baton,
apr_pool_t *pool) {
svn_error_t *err = SVN_NO_ERROR;
apr_pool_t *iterpool = svn_pool_create(pool);
int i;
for (i = 0; (! err) && i < array->nelts; ++i) {
void *item = array->elts + array->elt_size*i;
svn_pool_clear(iterpool);
err = (*func)(baton, item, pool);
}
if (completed)
*completed = ! err;
if (err && err->apr_err == SVN_ERR_ITER_BREAK) {
if (err != &internal_break_error)
svn_error_clear(err);
err = SVN_NO_ERROR;
}
svn_pool_destroy(iterpool);
return err;
}
svn_error_t *
svn_iter__break(void) {
return &internal_break_error;
}
