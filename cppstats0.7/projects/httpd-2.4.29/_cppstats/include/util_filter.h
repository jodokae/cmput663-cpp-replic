#if !defined(AP_FILTER_H)
#define AP_FILTER_H
#include "apr.h"
#include "apr_buckets.h"
#include "httpd.h"
#if APR_HAVE_STDARG_H
#include <stdarg.h>
#endif
#if defined(__cplusplus)
extern "C" {
#endif
typedef enum {
AP_MODE_READBYTES,
AP_MODE_GETLINE,
AP_MODE_EATCRLF,
AP_MODE_SPECULATIVE,
AP_MODE_EXHAUSTIVE,
AP_MODE_INIT
} ap_input_mode_t;
typedef struct ap_filter_t ap_filter_t;
typedef apr_status_t (*ap_out_filter_func)(ap_filter_t *f,
apr_bucket_brigade *b);
typedef apr_status_t (*ap_in_filter_func)(ap_filter_t *f,
apr_bucket_brigade *b,
ap_input_mode_t mode,
apr_read_type_e block,
apr_off_t readbytes);
typedef int (*ap_init_filter_func)(ap_filter_t *f);
typedef union ap_filter_func {
ap_out_filter_func out_func;
ap_in_filter_func in_func;
} ap_filter_func;
typedef enum {
AP_FTYPE_RESOURCE = 10,
AP_FTYPE_CONTENT_SET = 20,
AP_FTYPE_PROTOCOL = 30,
AP_FTYPE_TRANSCODE = 40,
AP_FTYPE_CONNECTION = 50,
AP_FTYPE_NETWORK = 60
} ap_filter_type;
typedef struct ap_filter_rec_t ap_filter_rec_t;
typedef struct ap_filter_provider_t ap_filter_provider_t;
struct ap_filter_rec_t {
const char *name;
ap_filter_func filter_func;
ap_init_filter_func filter_init_func;
struct ap_filter_rec_t *next;
ap_filter_provider_t *providers;
ap_filter_type ftype;
int debug;
unsigned int proto_flags;
};
struct ap_filter_t {
ap_filter_rec_t *frec;
void *ctx;
ap_filter_t *next;
request_rec *r;
conn_rec *c;
};
AP_DECLARE(apr_status_t) ap_get_brigade(ap_filter_t *filter,
apr_bucket_brigade *bucket,
ap_input_mode_t mode,
apr_read_type_e block,
apr_off_t readbytes);
AP_DECLARE(apr_status_t) ap_pass_brigade(ap_filter_t *filter,
apr_bucket_brigade *bucket);
AP_DECLARE(apr_status_t) ap_pass_brigade_fchk(request_rec *r,
apr_bucket_brigade *bucket,
const char *fmt,
...)
__attribute__((format(printf,3,4)));
AP_DECLARE(ap_filter_rec_t *) ap_register_input_filter(const char *name,
ap_in_filter_func filter_func,
ap_init_filter_func filter_init,
ap_filter_type ftype);
AP_DECLARE(ap_filter_rec_t *) ap_register_output_filter(const char *name,
ap_out_filter_func filter_func,
ap_init_filter_func filter_init,
ap_filter_type ftype);
AP_DECLARE(ap_filter_rec_t *) ap_register_output_filter_protocol(
const char *name,
ap_out_filter_func filter_func,
ap_init_filter_func filter_init,
ap_filter_type ftype,
unsigned int proto_flags);
AP_DECLARE(ap_filter_t *) ap_add_input_filter(const char *name, void *ctx,
request_rec *r, conn_rec *c);
AP_DECLARE(ap_filter_t *) ap_add_input_filter_handle(ap_filter_rec_t *f,
void *ctx,
request_rec *r,
conn_rec *c);
AP_DECLARE(ap_filter_rec_t *) ap_get_input_filter_handle(const char *name);
AP_DECLARE(ap_filter_t *) ap_add_output_filter(const char *name, void *ctx,
request_rec *r, conn_rec *c);
AP_DECLARE(ap_filter_t *) ap_add_output_filter_handle(ap_filter_rec_t *f,
void *ctx,
request_rec *r,
conn_rec *c);
AP_DECLARE(ap_filter_rec_t *) ap_get_output_filter_handle(const char *name);
AP_DECLARE(void) ap_remove_input_filter(ap_filter_t *f);
AP_DECLARE(void) ap_remove_output_filter(ap_filter_t *f);
AP_DECLARE(apr_status_t) ap_remove_input_filter_byhandle(ap_filter_t *next,
const char *handle);
AP_DECLARE(apr_status_t) ap_remove_output_filter_byhandle(ap_filter_t *next,
const char *handle);
AP_DECLARE(apr_status_t) ap_save_brigade(ap_filter_t *f,
apr_bucket_brigade **save_to,
apr_bucket_brigade **b, apr_pool_t *p);
AP_DECLARE_NONSTD(apr_status_t) ap_filter_flush(apr_bucket_brigade *bb,
void *ctx);
AP_DECLARE(apr_status_t) ap_fflush(ap_filter_t *f, apr_bucket_brigade *bb);
#define ap_fwrite(f, bb, data, nbyte) apr_brigade_write(bb, ap_filter_flush, f, data, nbyte)
#define ap_fputs(f, bb, str) apr_brigade_write(bb, ap_filter_flush, f, str, strlen(str))
#define ap_fputc(f, bb, c) apr_brigade_putc(bb, ap_filter_flush, f, c)
AP_DECLARE_NONSTD(apr_status_t) ap_fputstrs(ap_filter_t *f,
apr_bucket_brigade *bb,
...)
AP_FN_ATTR_SENTINEL;
AP_DECLARE_NONSTD(apr_status_t) ap_fprintf(ap_filter_t *f,
apr_bucket_brigade *bb,
const char *fmt,
...)
__attribute__((format(printf,3,4)));
AP_DECLARE(void) ap_filter_protocol(ap_filter_t* f, unsigned int proto_flags);
#define AP_FILTER_PROTO_CHANGE 0x1
#define AP_FILTER_PROTO_CHANGE_LENGTH 0x2
#define AP_FILTER_PROTO_NO_BYTERANGE 0x4
#define AP_FILTER_PROTO_NO_PROXY 0x8
#define AP_FILTER_PROTO_NO_CACHE 0x10
#define AP_FILTER_PROTO_TRANSFORM 0x20
#if defined(__cplusplus)
}
#endif
#endif
