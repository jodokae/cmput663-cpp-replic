#if !defined(__mod_h2__h2_headers__)
#define __mod_h2__h2_headers__
#include "h2.h"
struct h2_bucket_beam;
extern const apr_bucket_type_t h2_bucket_type_headers;
#define H2_BUCKET_IS_HEADERS(e) (e->type == &h2_bucket_type_headers)
apr_bucket * h2_bucket_headers_make(apr_bucket *b, h2_headers *r);
apr_bucket * h2_bucket_headers_create(apr_bucket_alloc_t *list,
h2_headers *r);
h2_headers *h2_bucket_headers_get(apr_bucket *b);
apr_bucket *h2_bucket_headers_beam(struct h2_bucket_beam *beam,
apr_bucket_brigade *dest,
const apr_bucket *src);
h2_headers *h2_headers_create(int status, apr_table_t *header,
apr_table_t *notes, apr_pool_t *pool);
h2_headers *h2_headers_rcreate(request_rec *r, int status,
apr_table_t *header, apr_pool_t *pool);
h2_headers *h2_headers_copy(apr_pool_t *pool, h2_headers *h);
h2_headers *h2_headers_die(apr_status_t type,
const struct h2_request *req, apr_pool_t *pool);
int h2_headers_are_response(h2_headers *headers);
#endif