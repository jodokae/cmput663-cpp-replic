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
#define HEX_DIGITS "0123456789abcdef"
static char *etag_uint64_to_hex(char *next, apr_uint64_t u) {
int printing = 0;
int shift = sizeof(apr_uint64_t) * 8 - 4;
do {
unsigned short next_digit = (unsigned short)
((u >> shift) & (apr_uint64_t)0xf);
if (next_digit) {
*next++ = HEX_DIGITS[next_digit];
printing = 1;
} else if (printing) {
*next++ = HEX_DIGITS[next_digit];
}
shift -= 4;
} while (shift);
*next++ = HEX_DIGITS[u & (apr_uint64_t)0xf];
return next;
}
#define ETAG_WEAK "W/"
#define CHARS_PER_UINT64 (sizeof(apr_uint64_t) * 2)
AP_DECLARE(char *) ap_make_etag(request_rec *r, int force_weak) {
char *weak;
apr_size_t weak_len;
char *etag;
char *next;
core_dir_config *cfg;
etag_components_t etag_bits;
etag_components_t bits_added;
cfg = (core_dir_config *)ap_get_core_module_config(r->per_dir_config);
etag_bits = (cfg->etag_bits & (~ cfg->etag_remove)) | cfg->etag_add;
if (etag_bits & ETAG_NONE) {
apr_table_setn(r->notes, "no-etag", "omit");
return "";
}
if (etag_bits == ETAG_UNSET) {
etag_bits = ETAG_BACKWARD;
}
if ((r->request_time - r->mtime > (1 * APR_USEC_PER_SEC)) &&
!force_weak) {
weak = NULL;
weak_len = 0;
} else {
weak = ETAG_WEAK;
weak_len = sizeof(ETAG_WEAK);
}
if (r->finfo.filetype != APR_NOFILE) {
etag = apr_palloc(r->pool, weak_len + sizeof("\"--\"") +
3 * CHARS_PER_UINT64 + 1);
next = etag;
if (weak) {
while (*weak) {
*next++ = *weak++;
}
}
*next++ = '"';
bits_added = 0;
if (etag_bits & ETAG_INODE) {
next = etag_uint64_to_hex(next, r->finfo.inode);
bits_added |= ETAG_INODE;
}
if (etag_bits & ETAG_SIZE) {
if (bits_added != 0) {
*next++ = '-';
}
next = etag_uint64_to_hex(next, r->finfo.size);
bits_added |= ETAG_SIZE;
}
if (etag_bits & ETAG_MTIME) {
if (bits_added != 0) {
*next++ = '-';
}
next = etag_uint64_to_hex(next, r->mtime);
}
*next++ = '"';
*next = '\0';
} else {
etag = apr_palloc(r->pool, weak_len + sizeof("\"\"") +
CHARS_PER_UINT64 + 1);
next = etag;
if (weak) {
while (*weak) {
*next++ = *weak++;
}
}
*next++ = '"';
next = etag_uint64_to_hex(next, r->mtime);
*next++ = '"';
*next = '\0';
}
return etag;
}
AP_DECLARE(void) ap_set_etag(request_rec *r) {
char *etag;
char *variant_etag, *vlv;
int vlv_weak;
if (!r->vlist_validator) {
etag = ap_make_etag(r, 0);
if (!etag[0]) {
return;
}
} else {
vlv = r->vlist_validator;
vlv_weak = (vlv[0] == 'W');
variant_etag = ap_make_etag(r, vlv_weak);
if (!variant_etag[0]) {
return;
}
variant_etag[strlen(variant_etag) - 1] = '\0';
if (vlv_weak) {
vlv += 3;
} else {
vlv++;
}
etag = apr_pstrcat(r->pool, variant_etag, ";", vlv, NULL);
}
apr_table_setn(r->headers_out, "ETag", etag);
}
