#include <string.h>
#include <apr.h>
#include <apr_pools.h>
#include <apr_general.h>
#include "svn_pools.h"
#include "svn_io.h"
#include "svn_error.h"
#include "svn_base64.h"
#define BASE64_LINELEN 76
static const char base64tab[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
"abcdefghijklmnopqrstuvwxyz0123456789+/";
struct encode_baton {
svn_stream_t *output;
unsigned char buf[3];
int buflen;
int linelen;
apr_pool_t *pool;
};
static APR_INLINE void
encode_group(const unsigned char *in, char *out) {
out[0] = base64tab[in[0] >> 2];
out[1] = base64tab[((in[0] & 0x3) << 4) | (in[1] >> 4)];
out[2] = base64tab[((in[1] & 0xf) << 2) | (in[2] >> 6)];
out[3] = base64tab[in[2] & 0x3f];
}
static void
encode_bytes(svn_stringbuf_t *str, const char *data, apr_size_t len,
unsigned char *inbuf, int *inbuflen, int *linelen) {
char group[4];
const char *p = data, *end = data + len;
while (*inbuflen + (end - p) >= 3) {
memcpy(inbuf + *inbuflen, p, 3 - *inbuflen);
p += (3 - *inbuflen);
encode_group(inbuf, group);
svn_stringbuf_appendbytes(str, group, 4);
*inbuflen = 0;
*linelen += 4;
if (*linelen == BASE64_LINELEN) {
svn_stringbuf_appendcstr(str, "\n");
*linelen = 0;
}
}
memcpy(inbuf + *inbuflen, p, end - p);
*inbuflen += (end - p);
}
static void
encode_partial_group(svn_stringbuf_t *str, const unsigned char *extra,
int len, int linelen) {
unsigned char ingroup[3];
char outgroup[4];
if (len > 0) {
memcpy(ingroup, extra, len);
memset(ingroup + len, 0, 3 - len);
encode_group(ingroup, outgroup);
memset(outgroup + (len + 1), '=', 4 - (len + 1));
svn_stringbuf_appendbytes(str, outgroup, 4);
linelen += 4;
}
if (linelen > 0)
svn_stringbuf_appendcstr(str, "\n");
}
static svn_error_t *
encode_data(void *baton, const char *data, apr_size_t *len) {
struct encode_baton *eb = baton;
apr_pool_t *subpool = svn_pool_create(eb->pool);
svn_stringbuf_t *encoded = svn_stringbuf_create("", subpool);
apr_size_t enclen;
svn_error_t *err = SVN_NO_ERROR;
encode_bytes(encoded, data, *len, eb->buf, &eb->buflen, &eb->linelen);
enclen = encoded->len;
if (enclen != 0)
err = svn_stream_write(eb->output, encoded->data, &enclen);
svn_pool_destroy(subpool);
return err;
}
static svn_error_t *
finish_encoding_data(void *baton) {
struct encode_baton *eb = baton;
svn_stringbuf_t *encoded = svn_stringbuf_create("", eb->pool);
apr_size_t enclen;
svn_error_t *err = SVN_NO_ERROR;
encode_partial_group(encoded, eb->buf, eb->buflen, eb->linelen);
enclen = encoded->len;
if (enclen != 0)
err = svn_stream_write(eb->output, encoded->data, &enclen);
if (err == SVN_NO_ERROR)
err = svn_stream_close(eb->output);
svn_pool_destroy(eb->pool);
return err;
}
svn_stream_t *
svn_base64_encode(svn_stream_t *output, apr_pool_t *pool) {
apr_pool_t *subpool = svn_pool_create(pool);
struct encode_baton *eb = apr_palloc(subpool, sizeof(*eb));
svn_stream_t *stream;
eb->output = output;
eb->buflen = 0;
eb->linelen = 0;
eb->pool = subpool;
stream = svn_stream_create(eb, pool);
svn_stream_set_write(stream, encode_data);
svn_stream_set_close(stream, finish_encoding_data);
return stream;
}
const svn_string_t *
svn_base64_encode_string(const svn_string_t *str, apr_pool_t *pool) {
svn_stringbuf_t *encoded = svn_stringbuf_create("", pool);
svn_string_t *retval = apr_pcalloc(pool, sizeof(*retval));
unsigned char ingroup[3];
int ingrouplen = 0, linelen = 0;
encode_bytes(encoded, str->data, str->len, ingroup, &ingrouplen, &linelen);
encode_partial_group(encoded, ingroup, ingrouplen, linelen);
retval->data = encoded->data;
retval->len = encoded->len;
return retval;
}
struct decode_baton {
svn_stream_t *output;
unsigned char buf[4];
int buflen;
svn_boolean_t done;
apr_pool_t *pool;
};
static APR_INLINE void
decode_group(const unsigned char *in, char *out) {
out[0] = (in[0] << 2) | (in[1] >> 4);
out[1] = ((in[1] & 0xf) << 4) | (in[2] >> 2);
out[2] = ((in[2] & 0x3) << 6) | in[3];
}
static void
decode_bytes(svn_stringbuf_t *str, const char *data, apr_size_t len,
unsigned char *inbuf, int *inbuflen, svn_boolean_t *done) {
const char *p, *find;
char group[3];
for (p = data; !*done && p < data + len; p++) {
if (*p == '=') {
if (*inbuflen >= 2) {
memset(inbuf + *inbuflen, 0, 4 - *inbuflen);
decode_group(inbuf, group);
svn_stringbuf_appendbytes(str, group, *inbuflen - 1);
}
*done = TRUE;
} else {
find = strchr(base64tab, *p);
if (find != NULL)
inbuf[(*inbuflen)++] = find - base64tab;
if (*inbuflen == 4) {
decode_group(inbuf, group);
svn_stringbuf_appendbytes(str, group, 3);
*inbuflen = 0;
}
}
}
}
static svn_error_t *
decode_data(void *baton, const char *data, apr_size_t *len) {
struct decode_baton *db = baton;
apr_pool_t *subpool;
svn_stringbuf_t *decoded;
apr_size_t declen;
svn_error_t *err = SVN_NO_ERROR;
subpool = svn_pool_create(db->pool);
decoded = svn_stringbuf_create("", subpool);
decode_bytes(decoded, data, *len, db->buf, &db->buflen, &db->done);
declen = decoded->len;
if (declen != 0)
err = svn_stream_write(db->output, decoded->data, &declen);
svn_pool_destroy(subpool);
return err;
}
static svn_error_t *
finish_decoding_data(void *baton) {
struct decode_baton *db = baton;
svn_error_t *err;
err = svn_stream_close(db->output);
svn_pool_destroy(db->pool);
return err;
}
svn_stream_t *
svn_base64_decode(svn_stream_t *output, apr_pool_t *pool) {
apr_pool_t *subpool = svn_pool_create(pool);
struct decode_baton *db = apr_palloc(subpool, sizeof(*db));
svn_stream_t *stream;
db->output = output;
db->buflen = 0;
db->done = FALSE;
db->pool = subpool;
stream = svn_stream_create(db, pool);
svn_stream_set_write(stream, decode_data);
svn_stream_set_close(stream, finish_decoding_data);
return stream;
}
const svn_string_t *
svn_base64_decode_string(const svn_string_t *str, apr_pool_t *pool) {
svn_stringbuf_t *decoded = svn_stringbuf_create("", pool);
svn_string_t *retval = apr_pcalloc(pool, sizeof(*retval));
unsigned char ingroup[4];
int ingrouplen = 0;
svn_boolean_t done = FALSE;
decode_bytes(decoded, str->data, str->len, ingroup, &ingrouplen, &done);
retval->data = decoded->data;
retval->len = decoded->len;
return retval;
}
svn_stringbuf_t *
svn_base64_from_md5(unsigned char digest[], apr_pool_t *pool) {
svn_stringbuf_t *md5str;
unsigned char ingroup[3];
int ingrouplen = 0, linelen = 0;
md5str = svn_stringbuf_create("", pool);
encode_bytes(md5str, (char*)digest, APR_MD5_DIGESTSIZE, ingroup,
&ingrouplen, &linelen);
encode_partial_group(md5str, ingroup, ingrouplen, linelen);
if ((md5str)->len) {
(md5str)->len--;
(md5str)->data[(md5str)->len] = 0;
}
return md5str;
}