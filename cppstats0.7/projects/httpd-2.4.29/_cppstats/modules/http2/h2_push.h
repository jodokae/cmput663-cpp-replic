#if !defined(__mod_h2__h2_push__)
#define __mod_h2__h2_push__
#include "h2.h"
struct h2_request;
struct h2_headers;
struct h2_ngheader;
struct h2_session;
struct h2_stream;
typedef struct h2_push {
const struct h2_request *req;
h2_priority *priority;
} h2_push;
typedef enum {
H2_PUSH_DIGEST_APR_HASH,
H2_PUSH_DIGEST_SHA256
} h2_push_digest_type;
typedef struct h2_push_diary h2_push_diary;
typedef void h2_push_digest_calc(h2_push_diary *diary, apr_uint64_t *phash, h2_push *push);
struct h2_push_diary {
apr_array_header_t *entries;
int NMax;
int N;
apr_uint64_t mask;
unsigned int mask_bits;
const char *authority;
h2_push_digest_type dtype;
h2_push_digest_calc *dcalc;
};
apr_array_header_t *h2_push_collect(apr_pool_t *p,
const struct h2_request *req,
int push_policy,
const struct h2_headers *res);
h2_push_diary *h2_push_diary_create(apr_pool_t *p, int N);
apr_array_header_t *h2_push_diary_update(struct h2_session *session, apr_array_header_t *pushes);
apr_array_header_t *h2_push_collect_update(struct h2_stream *stream,
const struct h2_request *req,
const struct h2_headers *res);
apr_status_t h2_push_diary_digest_get(h2_push_diary *diary, apr_pool_t *p,
int maxP, const char *authority,
const char **pdata, apr_size_t *plen);
apr_status_t h2_push_diary_digest_set(h2_push_diary *diary, const char *authority,
const char *data, apr_size_t len);
apr_status_t h2_push_diary_digest64_set(h2_push_diary *diary, const char *authority,
const char *data64url, apr_pool_t *pool);
#endif