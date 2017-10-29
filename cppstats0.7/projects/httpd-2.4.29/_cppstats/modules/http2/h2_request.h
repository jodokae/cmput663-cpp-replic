#if !defined(__mod_h2__h2_request__)
#define __mod_h2__h2_request__
#include "h2.h"
apr_status_t h2_request_rcreate(h2_request **preq, apr_pool_t *pool,
request_rec *r);
apr_status_t h2_request_add_header(h2_request *req, apr_pool_t *pool,
const char *name, size_t nlen,
const char *value, size_t vlen);
apr_status_t h2_request_add_trailer(h2_request *req, apr_pool_t *pool,
const char *name, size_t nlen,
const char *value, size_t vlen);
apr_status_t h2_request_end_headers(h2_request *req, apr_pool_t *pool, int eos);
h2_request *h2_request_clone(apr_pool_t *p, const h2_request *src);
request_rec *h2_request_create_rec(const h2_request *req, conn_rec *conn);
#endif