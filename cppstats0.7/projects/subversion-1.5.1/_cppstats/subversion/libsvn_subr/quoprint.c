#include <string.h>
#include <apr.h>
#include <apr_pools.h>
#include <apr_general.h>
#include "svn_pools.h"
#include "svn_io.h"
#include "svn_error.h"
#include "svn_quoprint.h"
#define QUOPRINT_LINELEN 76
#define VALID_LITERAL(c) ((c) == '\t' || ((c) >= ' ' && (c) <= '~' && (c) != '='))
#define ENCODE_AS_LITERAL(c) (VALID_LITERAL(c) && (c) != '\t' && (c) != '<' && (c) != '>' && (c) != '\'' && (c) != '"' && (c) != '&')
static const char hextab[] = "0123456789ABCDEF";
struct encode_baton {
svn_stream_t *output;
int linelen;
apr_pool_t *pool;
};
static void
encode_bytes(svn_stringbuf_t *str, const char *data, apr_size_t len,
int *linelen) {
char buf[3];
const char *p;
for (p = data; p < data + len; p++) {
if (ENCODE_AS_LITERAL(*p)) {
svn_stringbuf_appendbytes(str, p, 1);
(*linelen)++;
} else {
buf[0] = '=';
buf[1] = hextab[(*p >> 4) & 0xf];
buf[2] = hextab[*p & 0xf];
svn_stringbuf_appendbytes(str, buf, 3);
*linelen += 3;
}
if (*linelen + 3 > QUOPRINT_LINELEN) {
svn_stringbuf_appendcstr(str, "=\n");
*linelen = 0;
}
}
}
static svn_error_t *
encode_data(void *baton, const char *data, apr_size_t *len) {
struct encode_baton *eb = baton;
apr_pool_t *subpool = svn_pool_create(eb->pool);
svn_stringbuf_t *encoded = svn_stringbuf_create("", subpool);
apr_size_t enclen;
svn_error_t *err = SVN_NO_ERROR;
encode_bytes(encoded, data, *len, &eb->linelen);
enclen = encoded->len;
if (enclen != 0)
err = svn_stream_write(eb->output, encoded->data, &enclen);
svn_pool_destroy(subpool);
return err;
}
static svn_error_t *
finish_encoding_data(void *baton) {
struct encode_baton *eb = baton;
svn_error_t *err = SVN_NO_ERROR;
apr_size_t len;
if (eb->linelen > 0) {
len = 2;
err = svn_stream_write(eb->output, "=\n", &len);
}
if (err == SVN_NO_ERROR)
err = svn_stream_close(eb->output);
svn_pool_destroy(eb->pool);
return err;
}
svn_stream_t *
svn_quoprint_encode(svn_stream_t *output, apr_pool_t *pool) {
apr_pool_t *subpool = svn_pool_create(pool);
struct encode_baton *eb = apr_palloc(subpool, sizeof(*eb));
svn_stream_t *stream;
eb->output = output;
eb->linelen = 0;
eb->pool = subpool;
stream = svn_stream_create(eb, pool);
svn_stream_set_write(stream, encode_data);
svn_stream_set_close(stream, finish_encoding_data);
return stream;
}
svn_stringbuf_t *
svn_quoprint_encode_string(svn_stringbuf_t *str, apr_pool_t *pool) {
svn_stringbuf_t *encoded = svn_stringbuf_create("", pool);
int linelen = 0;
encode_bytes(encoded, str->data, str->len, &linelen);
if (linelen > 0)
svn_stringbuf_appendcstr(encoded, "=\n");
return encoded;
}
struct decode_baton {
svn_stream_t *output;
char buf[3];
int buflen;
apr_pool_t *pool;
};
static void
decode_bytes(svn_stringbuf_t *str, const char *data, apr_size_t len,
char *inbuf, int *inbuflen) {
const char *p, *find1, *find2;
char c;
for (p = data; p <= data + len; p++) {
inbuf[(*inbuflen)++] = *p;
if (*inbuf != '=') {
if (VALID_LITERAL(*inbuf))
svn_stringbuf_appendbytes(str, inbuf, 1);
*inbuflen = 0;
} else if (*inbuf == '=' && *inbuflen == 2 && inbuf[1] == '\n') {
*inbuflen = 0;
} else if (*inbuf == '=' && *inbuflen == 3) {
find1 = strchr(hextab, inbuf[1]);
find2 = strchr(hextab, inbuf[2]);
if (find1 != NULL && find2 != NULL) {
c = ((find1 - hextab) << 4) | (find2 - hextab);
svn_stringbuf_appendbytes(str, &c, 1);
}
*inbuflen = 0;
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
decode_bytes(decoded, data, *len, db->buf, &db->buflen);
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
svn_quoprint_decode(svn_stream_t *output, apr_pool_t *pool) {
apr_pool_t *subpool = svn_pool_create(pool);
struct decode_baton *db = apr_palloc(subpool, sizeof(*db));
svn_stream_t *stream;
db->output = output;
db->buflen = 0;
db->pool = subpool;
stream = svn_stream_create(db, pool);
svn_stream_set_write(stream, decode_data);
svn_stream_set_close(stream, finish_decoding_data);
return stream;
}
svn_stringbuf_t *
svn_quoprint_decode_string(svn_stringbuf_t *str, apr_pool_t *pool) {
svn_stringbuf_t *decoded = svn_stringbuf_create("", pool);
char ingroup[4];
int ingrouplen = 0;
decode_bytes(decoded, str->data, str->len, ingroup, &ingrouplen);
return decoded;
}
