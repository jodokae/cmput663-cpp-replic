#include "apr.h"
#include "apr_strings.h"
#include "apr_buckets.h"
#include "apr_lib.h"
#include "apr_signal.h"
#define APR_WANT_STDIO
#define APR_WANT_STRFUNC
#define APR_WANT_MEMFUNC
#include "apr_want.h"
#include "util_filter.h"
#include "ap_config.h"
#include "httpd.h"
#include "http_config.h"
#include "http_core.h"
#include "http_protocol.h"
#include "http_main.h"
#include "http_request.h"
#include "http_vhost.h"
#include "http_log.h"
#include "apr_date.h"
#include "util_charset.h"
#include "util_ebcdic.h"
#include "util_time.h"
#include "mod_core.h"
#if APR_HAVE_STDARG_H
#include <stdarg.h>
#endif
#if APR_HAVE_UNISTD_H
#include <unistd.h>
#endif
#if !defined(AP_DEFAULT_MAX_RANGES)
#define AP_DEFAULT_MAX_RANGES 200
#endif
#if !defined(AP_DEFAULT_MAX_OVERLAPS)
#define AP_DEFAULT_MAX_OVERLAPS 20
#endif
#if !defined(AP_DEFAULT_MAX_REVERSALS)
#define AP_DEFAULT_MAX_REVERSALS 20
#endif
#define MAX_PREALLOC_RANGES 100
APLOG_USE_MODULE(http);
typedef struct indexes_t {
apr_off_t start;
apr_off_t end;
} indexes_t;
static int ap_set_byterange(request_rec *r, apr_off_t clength,
apr_array_header_t **indexes,
int *overlaps, int *reversals) {
const char *range;
const char *ct;
char *cur;
apr_array_header_t *merged;
int num_ranges = 0, unsatisfiable = 0;
apr_off_t ostart = 0, oend = 0, sum_lengths = 0;
int in_merge = 0;
indexes_t *idx;
int ranges = 1;
int i;
const char *it;
*overlaps = 0;
*reversals = 0;
if (r->assbackwards) {
return 0;
}
if (!(range = apr_table_get(r->headers_in, "Range"))) {
range = apr_table_get(r->headers_in, "Request-Range");
}
if (!range || strncasecmp(range, "bytes=", 6) || r->status != HTTP_OK) {
return 0;
}
if (apr_table_get(r->headers_out, "Content-Range")) {
return 0;
}
if ((ct = apr_table_get(r->headers_out, "Content-Type"))
&& (!strncasecmp(ct, "multipart/byteranges", 20)
|| !strncasecmp(ct, "multipart/x-byteranges", 22))) {
return 0;
}
if (AP_CONDITION_NOMATCH == ap_condition_if_range(r, r->headers_out)) {
return 0;
}
range += 6;
it = range;
while (*it) {
if (*it++ == ',') {
ranges++;
}
}
it = range;
if (ranges > MAX_PREALLOC_RANGES) {
ranges = MAX_PREALLOC_RANGES;
}
*indexes = apr_array_make(r->pool, ranges, sizeof(indexes_t));
while ((cur = ap_getword(r->pool, &range, ','))) {
char *dash;
char *errp;
apr_off_t number, start, end;
if (!*cur)
break;
if (!(dash = strchr(cur, '-'))) {
return 0;
}
if (dash == cur) {
if (apr_strtoff(&number, dash+1, &errp, 10) || *errp) {
return 0;
}
if (number < 1) {
return 0;
}
start = clength - number;
end = clength - 1;
} else {
*dash++ = '\0';
if (apr_strtoff(&number, cur, &errp, 10) || *errp) {
return 0;
}
start = number;
if (*dash) {
if (apr_strtoff(&number, dash, &errp, 10) || *errp) {
return 0;
}
end = number;
if (start > end) {
return 0;
}
} else {
end = clength - 1;
if (start == 0) {
num_ranges = 0;
sum_lengths = 0;
in_merge = 1;
oend = end;
ostart = start;
apr_array_clear(*indexes);
break;
}
}
}
if (start < 0) {
start = 0;
}
if (start >= clength) {
unsatisfiable = 1;
continue;
}
if (end >= clength) {
end = clength - 1;
}
if (!in_merge) {
ostart = start;
oend = end;
in_merge = 1;
continue;
}
in_merge = 0;
if (start >= ostart && end <= oend) {
in_merge = 1;
}
if (start < ostart && end >= ostart-1) {
ostart = start;
++*reversals;
in_merge = 1;
}
if (end >= oend && start <= oend+1 ) {
oend = end;
in_merge = 1;
}
if (in_merge) {
++*overlaps;
continue;
} else {
idx = (indexes_t *)apr_array_push(*indexes);
idx->start = ostart;
idx->end = oend;
sum_lengths += oend - ostart + 1;
in_merge = 1;
ostart = start;
oend = end;
num_ranges++;
}
}
if (in_merge) {
idx = (indexes_t *)apr_array_push(*indexes);
idx->start = ostart;
idx->end = oend;
sum_lengths += oend - ostart + 1;
num_ranges++;
} else if (num_ranges == 0 && unsatisfiable) {
return -1;
}
if (sum_lengths > clength) {
ap_log_rerror(APLOG_MARK, APLOG_TRACE1, 0, r,
"Sum of ranges larger than file, ignoring.");
return 0;
}
merged = apr_array_make(r->pool, num_ranges, sizeof(char *));
idx = (indexes_t *)(*indexes)->elts;
for (i = 0; i < (*indexes)->nelts; i++, idx++) {
char **new = (char **)apr_array_push(merged);
*new = apr_psprintf(r->pool, "%" APR_OFF_T_FMT "-%" APR_OFF_T_FMT,
idx->start, idx->end);
}
r->status = HTTP_PARTIAL_CONTENT;
r->range = apr_array_pstrcat(r->pool, merged, ',');
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(01583)
"Range: %s | %s (%d : %d : %"APR_OFF_T_FMT")",
it, r->range, *overlaps, *reversals, clength);
return num_ranges;
}
static int use_range_x(request_rec *r) {
const char *ua;
return (apr_table_get(r->headers_in, "Request-Range")
|| ((ua = apr_table_get(r->headers_in, "User-Agent"))
&& ap_strstr_c(ua, "MSIE 3")));
}
#define BYTERANGE_FMT "%" APR_OFF_T_FMT "-%" APR_OFF_T_FMT "/%" APR_OFF_T_FMT
static apr_status_t copy_brigade_range(apr_bucket_brigade *bb,
apr_bucket_brigade *bbout,
apr_off_t start,
apr_off_t end) {
apr_bucket *first = NULL, *last = NULL, *out_first = NULL, *e;
apr_uint64_t pos = 0, off_first = 0, off_last = 0;
apr_status_t rv;
apr_uint64_t start64, end64;
apr_off_t pofft = 0;
start64 = (apr_uint64_t)start;
end64 = (apr_uint64_t)end;
if (start < 0 || end < 0 || start64 > end64)
return APR_EINVAL;
for (e = APR_BRIGADE_FIRST(bb);
e != APR_BRIGADE_SENTINEL(bb);
e = APR_BUCKET_NEXT(e)) {
apr_uint64_t elen64;
AP_DEBUG_ASSERT(e->length != (apr_size_t)(-1));
elen64 = (apr_uint64_t)e->length;
if (!first && (elen64 + pos > start64)) {
first = e;
off_first = pos;
}
if (elen64 + pos > end64) {
last = e;
off_last = pos;
break;
}
pos += elen64;
}
if (!first || !last)
return APR_EINVAL;
e = first;
while (1) {
apr_bucket *copy;
AP_DEBUG_ASSERT(e != APR_BRIGADE_SENTINEL(bb));
rv = apr_bucket_copy(e, &copy);
if (rv != APR_SUCCESS) {
apr_brigade_cleanup(bbout);
return rv;
}
APR_BRIGADE_INSERT_TAIL(bbout, copy);
if (e == first) {
if (off_first != start64) {
rv = apr_bucket_split(copy, (apr_size_t)(start64 - off_first));
if (rv != APR_SUCCESS) {
apr_brigade_cleanup(bbout);
return rv;
}
out_first = APR_BUCKET_NEXT(copy);
apr_bucket_delete(copy);
} else {
out_first = copy;
}
}
if (e == last) {
if (e == first) {
off_last += start64 - off_first;
copy = out_first;
}
if (end64 - off_last != (apr_uint64_t)e->length) {
rv = apr_bucket_split(copy, (apr_size_t)(end64 + 1 - off_last));
if (rv != APR_SUCCESS) {
apr_brigade_cleanup(bbout);
return rv;
}
copy = APR_BUCKET_NEXT(copy);
if (copy != APR_BRIGADE_SENTINEL(bbout)) {
apr_bucket_delete(copy);
}
}
break;
}
e = APR_BUCKET_NEXT(e);
}
AP_DEBUG_ASSERT(APR_SUCCESS == apr_brigade_length(bbout, 1, &pofft));
pos = (apr_uint64_t)pofft;
AP_DEBUG_ASSERT(pos == end64 - start64 + 1);
return APR_SUCCESS;
}
static apr_status_t send_416(ap_filter_t *f, apr_bucket_brigade *tmpbb) {
apr_bucket *e;
conn_rec *c = f->r->connection;
ap_remove_output_filter(f);
f->r->status = HTTP_OK;
e = ap_bucket_error_create(HTTP_RANGE_NOT_SATISFIABLE, NULL,
f->r->pool, c->bucket_alloc);
APR_BRIGADE_INSERT_TAIL(tmpbb, e);
e = apr_bucket_eos_create(c->bucket_alloc);
APR_BRIGADE_INSERT_TAIL(tmpbb, e);
return ap_pass_brigade(f->next, tmpbb);
}
AP_CORE_DECLARE_NONSTD(apr_status_t) ap_byterange_filter(ap_filter_t *f,
apr_bucket_brigade *bb) {
request_rec *r = f->r;
conn_rec *c = r->connection;
apr_bucket *e;
apr_bucket_brigade *bsend;
apr_bucket_brigade *tmpbb;
apr_off_t range_start;
apr_off_t range_end;
apr_off_t clength = 0;
apr_status_t rv;
int found = 0;
int num_ranges;
char *bound_head = NULL;
apr_array_header_t *indexes;
indexes_t *idx;
int i;
int original_status;
int max_ranges, max_overlaps, max_reversals;
int overlaps = 0, reversals = 0;
core_dir_config *core_conf = ap_get_core_module_config(r->per_dir_config);
max_ranges = ( (core_conf->max_ranges >= 0 || core_conf->max_ranges == AP_MAXRANGES_UNLIMITED)
? core_conf->max_ranges
: AP_DEFAULT_MAX_RANGES );
max_overlaps = ( (core_conf->max_overlaps >= 0 || core_conf->max_overlaps == AP_MAXRANGES_UNLIMITED)
? core_conf->max_overlaps
: AP_DEFAULT_MAX_OVERLAPS );
max_reversals = ( (core_conf->max_reversals >= 0 || core_conf->max_reversals == AP_MAXRANGES_UNLIMITED)
? core_conf->max_reversals
: AP_DEFAULT_MAX_REVERSALS );
for (e = APR_BRIGADE_FIRST(bb);
(e != APR_BRIGADE_SENTINEL(bb) && !APR_BUCKET_IS_EOS(e)
&& e->length != (apr_size_t)-1);
e = APR_BUCKET_NEXT(e)) {
clength += e->length;
}
if (!APR_BUCKET_IS_EOS(e) || clength <= 0) {
ap_remove_output_filter(f);
return ap_pass_brigade(f->next, bb);
}
original_status = r->status;
num_ranges = ap_set_byterange(r, clength, &indexes, &overlaps, &reversals);
if (num_ranges == 0 ||
(max_ranges >= 0 && num_ranges > max_ranges) ||
(max_overlaps >= 0 && overlaps > max_overlaps) ||
(max_reversals >= 0 && reversals > max_reversals)) {
r->status = original_status;
ap_remove_output_filter(f);
return ap_pass_brigade(f->next, bb);
}
bsend = apr_brigade_create(r->pool, c->bucket_alloc);
if (num_ranges < 0)
return send_416(f, bsend);
if (num_ranges > 1) {
const char *orig_ct = ap_make_content_type(r, r->content_type);
ap_set_content_type(r, apr_pstrcat(r->pool, "multipart",
use_range_x(r) ? "/x-" : "/",
"byteranges; boundary=",
ap_multipart_boundary, NULL));
if (orig_ct) {
bound_head = apr_pstrcat(r->pool,
CRLF "--", ap_multipart_boundary,
CRLF "Content-type: ",
orig_ct,
CRLF "Content-range: bytes ",
NULL);
} else {
bound_head = apr_pstrcat(r->pool,
CRLF "--", ap_multipart_boundary,
CRLF "Content-range: bytes ",
NULL);
}
ap_xlate_proto_to_ascii(bound_head, strlen(bound_head));
}
tmpbb = apr_brigade_create(r->pool, c->bucket_alloc);
idx = (indexes_t *)indexes->elts;
for (i = 0; i < indexes->nelts; i++, idx++) {
range_start = idx->start;
range_end = idx->end;
rv = copy_brigade_range(bb, tmpbb, range_start, range_end);
if (rv != APR_SUCCESS ) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, rv, r, APLOGNO(01584)
"copy_brigade_range() failed [%" APR_OFF_T_FMT
"-%" APR_OFF_T_FMT ",%" APR_OFF_T_FMT "]",
range_start, range_end, clength);
continue;
}
found = 1;
if (num_ranges == 1) {
apr_table_setn(r->headers_out, "Content-Range",
apr_psprintf(r->pool, "bytes " BYTERANGE_FMT,
range_start, range_end, clength));
} else {
char *ts;
e = apr_bucket_pool_create(bound_head, strlen(bound_head),
r->pool, c->bucket_alloc);
APR_BRIGADE_INSERT_TAIL(bsend, e);
ts = apr_psprintf(r->pool, BYTERANGE_FMT CRLF CRLF,
range_start, range_end, clength);
ap_xlate_proto_to_ascii(ts, strlen(ts));
e = apr_bucket_pool_create(ts, strlen(ts), r->pool,
c->bucket_alloc);
APR_BRIGADE_INSERT_TAIL(bsend, e);
}
APR_BRIGADE_CONCAT(bsend, tmpbb);
if (i && !(i & 0x1F)) {
apr_table_unset(r->headers_out, "Content-Length");
if ((rv = ap_pass_brigade(f->next, bsend)) != APR_SUCCESS)
return rv;
apr_brigade_cleanup(bsend);
}
}
if (found == 0) {
return send_416(f, bsend);
}
if (num_ranges > 1) {
char *end;
end = apr_pstrcat(r->pool, CRLF "--", ap_multipart_boundary, "--" CRLF,
NULL);
ap_xlate_proto_to_ascii(end, strlen(end));
e = apr_bucket_pool_create(end, strlen(end), r->pool, c->bucket_alloc);
APR_BRIGADE_INSERT_TAIL(bsend, e);
}
e = apr_bucket_eos_create(c->bucket_alloc);
APR_BRIGADE_INSERT_TAIL(bsend, e);
apr_brigade_cleanup(bb);
apr_brigade_destroy(tmpbb);
return ap_pass_brigade(f->next, bsend);
}