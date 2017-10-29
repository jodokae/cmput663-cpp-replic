#if !defined(__mod_h2__h2_util__)
#define __mod_h2__h2_util__
#include <nghttp2/nghttp2.h>
struct h2_request;
struct nghttp2_frame;
size_t h2_util_hex_dump(char *buffer, size_t maxlen,
const char *data, size_t datalen);
size_t h2_util_header_print(char *buffer, size_t maxlen,
const char *name, size_t namelen,
const char *value, size_t valuelen);
void h2_util_camel_case_header(char *s, size_t len);
int h2_util_frame_print(const nghttp2_frame *frame, char *buffer, size_t maxlen);
typedef struct h2_ihash_t h2_ihash_t;
typedef int h2_ihash_iter_t(void *ctx, void *val);
h2_ihash_t *h2_ihash_create(apr_pool_t *pool, size_t offset_of_int);
size_t h2_ihash_count(h2_ihash_t *ih);
int h2_ihash_empty(h2_ihash_t *ih);
void *h2_ihash_get(h2_ihash_t *ih, int id);
int h2_ihash_iter(h2_ihash_t *ih, h2_ihash_iter_t *fn, void *ctx);
void h2_ihash_add(h2_ihash_t *ih, void *val);
void h2_ihash_remove(h2_ihash_t *ih, int id);
void h2_ihash_remove_val(h2_ihash_t *ih, void *val);
void h2_ihash_clear(h2_ihash_t *ih);
size_t h2_ihash_shift(h2_ihash_t *ih, void **buffer, size_t max);
typedef struct h2_iqueue {
int *elts;
int head;
int nelts;
int nalloc;
apr_pool_t *pool;
} h2_iqueue;
typedef int h2_iq_cmp(int i1, int i2, void *ctx);
h2_iqueue *h2_iq_create(apr_pool_t *pool, int capacity);
int h2_iq_empty(h2_iqueue *q);
int h2_iq_count(h2_iqueue *q);
int h2_iq_add(h2_iqueue *q, int sid, h2_iq_cmp *cmp, void *ctx);
int h2_iq_append(h2_iqueue *q, int sid);
int h2_iq_remove(h2_iqueue *q, int sid);
void h2_iq_clear(h2_iqueue *q);
void h2_iq_sort(h2_iqueue *q, h2_iq_cmp *cmp, void *ctx);
int h2_iq_shift(h2_iqueue *q);
size_t h2_iq_mshift(h2_iqueue *q, int *pint, size_t max);
int h2_iq_contains(h2_iqueue *q, int sid);
typedef struct h2_fifo h2_fifo;
apr_status_t h2_fifo_create(h2_fifo **pfifo, apr_pool_t *pool, int capacity);
apr_status_t h2_fifo_set_create(h2_fifo **pfifo, apr_pool_t *pool, int capacity);
apr_status_t h2_fifo_term(h2_fifo *fifo);
apr_status_t h2_fifo_interrupt(h2_fifo *fifo);
int h2_fifo_count(h2_fifo *fifo);
apr_status_t h2_fifo_push(h2_fifo *fifo, void *elem);
apr_status_t h2_fifo_try_push(h2_fifo *fifo, void *elem);
apr_status_t h2_fifo_pull(h2_fifo *fifo, void **pelem);
apr_status_t h2_fifo_try_pull(h2_fifo *fifo, void **pelem);
typedef enum {
H2_FIFO_OP_PULL,
H2_FIFO_OP_REPUSH,
} h2_fifo_op_t;
typedef h2_fifo_op_t h2_fifo_peek_fn(void *head, void *ctx);
apr_status_t h2_fifo_peek(h2_fifo *fifo, h2_fifo_peek_fn *fn, void *ctx);
apr_status_t h2_fifo_try_peek(h2_fifo *fifo, h2_fifo_peek_fn *fn, void *ctx);
apr_status_t h2_fifo_remove(h2_fifo *fifo, void *elem);
typedef struct h2_ififo h2_ififo;
apr_status_t h2_ififo_create(h2_ififo **pfifo, apr_pool_t *pool, int capacity);
apr_status_t h2_ififo_set_create(h2_ififo **pfifo, apr_pool_t *pool, int capacity);
apr_status_t h2_ififo_term(h2_ififo *fifo);
apr_status_t h2_ififo_interrupt(h2_ififo *fifo);
int h2_ififo_count(h2_ififo *fifo);
apr_status_t h2_ififo_push(h2_ififo *fifo, int id);
apr_status_t h2_ififo_try_push(h2_ififo *fifo, int id);
apr_status_t h2_ififo_pull(h2_ififo *fifo, int *pi);
apr_status_t h2_ififo_try_pull(h2_ififo *fifo, int *pi);
typedef h2_fifo_op_t h2_ififo_peek_fn(int head, void *ctx);
apr_status_t h2_ififo_peek(h2_ififo *fifo, h2_ififo_peek_fn *fn, void *ctx);
apr_status_t h2_ififo_try_peek(h2_ififo *fifo, h2_ififo_peek_fn *fn, void *ctx);
apr_status_t h2_ififo_remove(h2_ififo *fifo, int id);
unsigned char h2_log2(int n);
apr_size_t h2_util_table_bytes(apr_table_t *t, apr_size_t pair_extra);
#define H2_HD_MATCH_LIT(l, name, nlen) ((nlen == sizeof(l) - 1) && !apr_strnatcasecmp(l, name))
int h2_req_ignore_header(const char *name, size_t len);
int h2_req_ignore_trailer(const char *name, size_t len);
int h2_res_ignore_trailer(const char *name, size_t len);
int h2_push_policy_determine(apr_table_t *headers, apr_pool_t *p, int push_enabled);
apr_size_t h2_util_base64url_decode(const char **decoded,
const char *encoded,
apr_pool_t *pool);
const char *h2_util_base64url_encode(const char *data,
apr_size_t len, apr_pool_t *pool);
#define H2_HD_MATCH_LIT_CS(l, name) ((strlen(name) == sizeof(l) - 1) && !apr_strnatcasecmp(l, name))
#define H2_CREATE_NV_LIT_CS(nv, NAME, VALUE) nv->name = (uint8_t *)NAME; nv->namelen = sizeof(NAME) - 1; nv->value = (uint8_t *)VALUE; nv->valuelen = strlen(VALUE)
#define H2_CREATE_NV_CS_LIT(nv, NAME, VALUE) nv->name = (uint8_t *)NAME; nv->namelen = strlen(NAME); nv->value = (uint8_t *)VALUE; nv->valuelen = sizeof(VALUE) - 1
#define H2_CREATE_NV_CS_CS(nv, NAME, VALUE) nv->name = (uint8_t *)NAME; nv->namelen = strlen(NAME); nv->value = (uint8_t *)VALUE; nv->valuelen = strlen(VALUE)
int h2_util_ignore_header(const char *name);
struct h2_headers;
typedef struct h2_ngheader {
nghttp2_nv *nv;
apr_size_t nvlen;
} h2_ngheader;
apr_status_t h2_res_create_ngtrailer(h2_ngheader **ph, apr_pool_t *p,
struct h2_headers *headers);
apr_status_t h2_res_create_ngheader(h2_ngheader **ph, apr_pool_t *p,
struct h2_headers *headers);
apr_status_t h2_req_create_ngheader(h2_ngheader **ph, apr_pool_t *p,
const struct h2_request *req);
apr_status_t h2_req_add_header(apr_table_t *headers, apr_pool_t *pool,
const char *name, size_t nlen,
const char *value, size_t vlen);
struct h2_request *h2_req_create(int id, apr_pool_t *pool, const char *method,
const char *scheme, const char *authority,
const char *path, apr_table_t *header,
int serialize);
apr_status_t h2_brigade_concat_length(apr_bucket_brigade *dest,
apr_bucket_brigade *src,
apr_off_t length);
apr_status_t h2_brigade_copy_length(apr_bucket_brigade *dest,
apr_bucket_brigade *src,
apr_off_t length);
int h2_util_has_eos(apr_bucket_brigade *bb, apr_off_t len);
apr_status_t h2_util_bb_avail(apr_bucket_brigade *bb,
apr_off_t *plen, int *peos);
typedef apr_status_t h2_util_pass_cb(void *ctx,
const char *data, apr_off_t len);
apr_status_t h2_util_bb_readx(apr_bucket_brigade *bb,
h2_util_pass_cb *cb, void *ctx,
apr_off_t *plen, int *peos);
apr_size_t h2_util_bucket_print(char *buffer, apr_size_t bmax,
apr_bucket *b, const char *sep);
apr_size_t h2_util_bb_print(char *buffer, apr_size_t bmax,
const char *tag, const char *sep,
apr_bucket_brigade *bb);
#define h2_util_bb_log(c, sid, level, tag, bb) do { char buffer[4 * 1024]; const char *line = "(null)"; apr_size_t len, bmax = sizeof(buffer)/sizeof(buffer[0]); len = h2_util_bb_print(buffer, bmax, (tag), "", (bb)); ap_log_cerror(APLOG_MARK, level, 0, (c), "bb_dump(%ld): %s", ((c)->master? (c)->master->id : (c)->id), (len? buffer : line)); } while(0)
typedef int h2_bucket_gate(apr_bucket *b);
apr_status_t h2_append_brigade(apr_bucket_brigade *to,
apr_bucket_brigade *from,
apr_off_t *plen,
int *peos,
h2_bucket_gate *should_append);
apr_off_t h2_brigade_mem_size(apr_bucket_brigade *bb);
#endif
