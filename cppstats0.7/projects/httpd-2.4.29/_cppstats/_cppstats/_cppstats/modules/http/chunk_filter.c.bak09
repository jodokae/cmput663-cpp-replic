#include "apr_strings.h"
#include "apr_thread_proc.h"
#define APR_WANT_STRFUNC
#include "apr_want.h"
#include "httpd.h"
#include "http_config.h"
#include "http_connection.h"
#include "http_core.h"
#include "http_protocol.h"
#include "http_request.h"
#include "util_filter.h"
#include "util_ebcdic.h"
#include "ap_mpm.h"
#include "scoreboard.h"
#include "mod_core.h"
static char bad_gateway_seen;
apr_status_t ap_http_chunk_filter(ap_filter_t *f, apr_bucket_brigade *b) {
#define ASCII_CRLF "\015\012"
#define ASCII_ZERO "\060"
conn_rec *c = f->r->connection;
apr_bucket_brigade *more, *tmp;
apr_bucket *e;
apr_status_t rv;
for (more = tmp = NULL; b; b = more, more = NULL) {
apr_off_t bytes = 0;
apr_bucket *eos = NULL;
apr_bucket *flush = NULL;
char chunk_hdr[20];
for (e = APR_BRIGADE_FIRST(b);
e != APR_BRIGADE_SENTINEL(b);
e = APR_BUCKET_NEXT(e)) {
if (APR_BUCKET_IS_EOS(e)) {
eos = e;
break;
}
if (AP_BUCKET_IS_ERROR(e)
&& (((ap_bucket_error *)(e->data))->status
== HTTP_BAD_GATEWAY)) {
f->ctx = &bad_gateway_seen;
continue;
}
if (APR_BUCKET_IS_FLUSH(e)) {
flush = e;
if (e != APR_BRIGADE_LAST(b)) {
more = apr_brigade_split_ex(b, APR_BUCKET_NEXT(e), tmp);
}
break;
} else if (e->length == (apr_size_t)-1) {
const char *data;
apr_size_t len;
rv = apr_bucket_read(e, &data, &len, APR_BLOCK_READ);
if (rv != APR_SUCCESS) {
return rv;
}
if (len > 0) {
bytes += len;
more = apr_brigade_split_ex(b, APR_BUCKET_NEXT(e), tmp);
break;
} else {
continue;
}
} else {
bytes += e->length;
}
}
if (bytes > 0) {
apr_size_t hdr_len;
hdr_len = apr_snprintf(chunk_hdr, sizeof(chunk_hdr),
"%" APR_UINT64_T_HEX_FMT CRLF, (apr_uint64_t)bytes);
ap_xlate_proto_to_ascii(chunk_hdr, hdr_len);
e = apr_bucket_transient_create(chunk_hdr, hdr_len,
c->bucket_alloc);
APR_BRIGADE_INSERT_HEAD(b, e);
e = apr_bucket_immortal_create(ASCII_CRLF, 2, c->bucket_alloc);
if (eos != NULL) {
APR_BUCKET_INSERT_BEFORE(eos, e);
} else if (flush != NULL) {
APR_BUCKET_INSERT_BEFORE(flush, e);
} else {
APR_BRIGADE_INSERT_TAIL(b, e);
}
}
if (eos && !f->ctx) {
e = apr_bucket_immortal_create(ASCII_ZERO ASCII_CRLF
ASCII_CRLF, 5, c->bucket_alloc);
APR_BUCKET_INSERT_BEFORE(eos, e);
}
rv = ap_pass_brigade(f->next, b);
if (rv != APR_SUCCESS || eos != NULL) {
return rv;
}
tmp = b;
apr_brigade_cleanup(tmp);
}
return APR_SUCCESS;
}
