#if !defined(__mod_h2__h2_proxy_util__)
#define __mod_h2__h2_proxy_util__
struct h2_proxy_request;
struct nghttp2_frame;
int h2_proxy_util_frame_print(const nghttp2_frame *frame, char *buffer, size_t maxlen);
typedef struct h2_proxy_ihash_t h2_proxy_ihash_t;
typedef int h2_proxy_ihash_iter_t(void *ctx, void *val);
h2_proxy_ihash_t *h2_proxy_ihash_create(apr_pool_t *pool, size_t offset_of_int);
size_t h2_proxy_ihash_count(h2_proxy_ihash_t *ih);
int h2_proxy_ihash_empty(h2_proxy_ihash_t *ih);
void *h2_proxy_ihash_get(h2_proxy_ihash_t *ih, int id);
int h2_proxy_ihash_iter(h2_proxy_ihash_t *ih, h2_proxy_ihash_iter_t *fn, void *ctx);
void h2_proxy_ihash_add(h2_proxy_ihash_t *ih, void *val);
void h2_proxy_ihash_remove(h2_proxy_ihash_t *ih, int id);
void h2_proxy_ihash_remove_val(h2_proxy_ihash_t *ih, void *val);
void h2_proxy_ihash_clear(h2_proxy_ihash_t *ih);
size_t h2_proxy_ihash_shift(h2_proxy_ihash_t *ih, void **buffer, size_t max);
size_t h2_proxy_ihash_ishift(h2_proxy_ihash_t *ih, int *buffer, size_t max);
typedef struct h2_proxy_iqueue {
int *elts;
int head;
int nelts;
int nalloc;
apr_pool_t *pool;
} h2_proxy_iqueue;
typedef int h2_proxy_iq_cmp(int i1, int i2, void *ctx);
h2_proxy_iqueue *h2_proxy_iq_create(apr_pool_t *pool, int capacity);
int h2_proxy_iq_empty(h2_proxy_iqueue *q);
int h2_proxy_iq_count(h2_proxy_iqueue *q);
void h2_proxy_iq_add(h2_proxy_iqueue *q, int sid, h2_proxy_iq_cmp *cmp, void *ctx);
int h2_proxy_iq_remove(h2_proxy_iqueue *q, int sid);
void h2_proxy_iq_clear(h2_proxy_iqueue *q);
void h2_proxy_iq_sort(h2_proxy_iqueue *q, h2_proxy_iq_cmp *cmp, void *ctx);
int h2_proxy_iq_shift(h2_proxy_iqueue *q);
unsigned char h2_proxy_log2(int n);
void h2_proxy_util_camel_case_header(char *s, size_t len);
int h2_proxy_res_ignore_header(const char *name, size_t len);
typedef struct h2_proxy_ngheader {
nghttp2_nv *nv;
apr_size_t nvlen;
} h2_proxy_ngheader;
h2_proxy_ngheader *h2_proxy_util_nghd_make_req(apr_pool_t *p,
const struct h2_proxy_request *req);
typedef struct h2_proxy_request h2_proxy_request;
struct h2_proxy_request {
const char *method;
const char *scheme;
const char *authority;
const char *path;
apr_table_t *headers;
apr_time_t request_time;
unsigned int chunked : 1;
unsigned int serialize : 1;
};
h2_proxy_request *h2_proxy_req_create(int id, apr_pool_t *pool, int serialize);
apr_status_t h2_proxy_req_make(h2_proxy_request *req, apr_pool_t *pool,
const char *method, const char *scheme,
const char *authority, const char *path,
apr_table_t *headers);
const char *h2_proxy_link_reverse_map(request_rec *r,
proxy_dir_conf *conf,
const char *real_server_uri,
const char *proxy_server_uri,
const char *s);
typedef struct h2_proxy_fifo h2_proxy_fifo;
apr_status_t h2_proxy_fifo_create(h2_proxy_fifo **pfifo, apr_pool_t *pool, int capacity);
apr_status_t h2_proxy_fifo_set_create(h2_proxy_fifo **pfifo, apr_pool_t *pool, int capacity);
apr_status_t h2_proxy_fifo_term(h2_proxy_fifo *fifo);
apr_status_t h2_proxy_fifo_interrupt(h2_proxy_fifo *fifo);
int h2_proxy_fifo_capacity(h2_proxy_fifo *fifo);
int h2_proxy_fifo_count(h2_proxy_fifo *fifo);
apr_status_t h2_proxy_fifo_push(h2_proxy_fifo *fifo, void *elem);
apr_status_t h2_proxy_fifo_try_push(h2_proxy_fifo *fifo, void *elem);
apr_status_t h2_proxy_fifo_pull(h2_proxy_fifo *fifo, void **pelem);
apr_status_t h2_proxy_fifo_try_pull(h2_proxy_fifo *fifo, void **pelem);
apr_status_t h2_proxy_fifo_remove(h2_proxy_fifo *fifo, void *elem);
#endif