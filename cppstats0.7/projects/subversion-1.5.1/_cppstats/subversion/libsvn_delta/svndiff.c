#include <assert.h>
#include <string.h>
#include "svn_delta.h"
#include "svn_io.h"
#include "delta.h"
#include "svn_pools.h"
#include "svn_private_config.h"
#include <zlib.h>
#define svnCompressBound(LEN) ((LEN) + ((LEN) >> 12) + ((LEN) >> 14) + 11)
#define MIN_COMPRESS_SIZE 512
#define SVNDIFF1_COMPRESS_LEVEL 5
#define NORMAL_BITS 7
#define LENGTH_BITS 5
struct encoder_baton {
svn_stream_t *output;
svn_boolean_t header_done;
int version;
apr_pool_t *pool;
};
static char *
encode_int(char *p, svn_filesize_t val) {
int n;
svn_filesize_t v;
unsigned char cont;
assert(val >= 0);
v = val >> 7;
n = 1;
while (v > 0) {
v = v >> 7;
n++;
}
while (--n >= 0) {
cont = ((n > 0) ? 0x1 : 0x0) << 7;
*p++ = (char)(((val >> (n * 7)) & 0x7f) | cont);
}
return p;
}
static void
append_encoded_int(svn_stringbuf_t *header, svn_filesize_t val) {
char buf[128], *p;
p = encode_int(buf, val);
svn_stringbuf_appendbytes(header, buf, p - buf);
}
static svn_error_t *
zlib_encode(const char *data, apr_size_t len, svn_stringbuf_t *out) {
unsigned long endlen;
unsigned int intlen;
append_encoded_int(out, len);
intlen = out->len;
if (len < MIN_COMPRESS_SIZE) {
svn_stringbuf_appendbytes(out, data, len);
} else {
svn_stringbuf_ensure(out, svnCompressBound(len) + intlen);
endlen = out->blocksize;
if (compress2((unsigned char *)out->data + intlen, &endlen,
(const unsigned char *)data, len,
SVNDIFF1_COMPRESS_LEVEL) != Z_OK)
return svn_error_create(SVN_ERR_SVNDIFF_INVALID_COMPRESSED_DATA,
NULL,
_("Compression of svndiff data failed"));
if (endlen >= len) {
svn_stringbuf_appendbytes(out, data, len);
return SVN_NO_ERROR;
}
out->len = endlen + intlen;
}
return SVN_NO_ERROR;
}
static svn_error_t *
window_handler(svn_txdelta_window_t *window, void *baton) {
struct encoder_baton *eb = baton;
apr_pool_t *pool = svn_pool_create(eb->pool);
svn_stringbuf_t *instructions = svn_stringbuf_create("", pool);
svn_stringbuf_t *i1 = svn_stringbuf_create("", pool);
svn_stringbuf_t *header = svn_stringbuf_create("", pool);
const svn_string_t *newdata;
char ibuf[128], *ip;
const svn_txdelta_op_t *op;
apr_size_t len;
if (eb->header_done == FALSE) {
char svnver[4] = "SVN\0";
len = 4;
svnver[3] = eb->version;
SVN_ERR(svn_stream_write(eb->output, svnver, &len));
eb->header_done = TRUE;
}
if (window == NULL) {
svn_stream_t *output = eb->output;
svn_pool_destroy(eb->pool);
return svn_stream_close(output);
}
for (op = window->ops; op < window->ops + window->num_ops; op++) {
ip = ibuf;
switch (op->action_code) {
case svn_txdelta_source:
*ip = (char)0;
break;
case svn_txdelta_target:
*ip = (char)(0x1 << 6);
break;
case svn_txdelta_new:
*ip = (char)(0x2 << 6);
break;
}
if (op->length >> 6 == 0)
*ip++ |= op->length;
else
ip = encode_int(ip + 1, op->length);
if (op->action_code != svn_txdelta_new)
ip = encode_int(ip, op->offset);
svn_stringbuf_appendbytes(instructions, ibuf, ip - ibuf);
}
append_encoded_int(header, window->sview_offset);
append_encoded_int(header, window->sview_len);
append_encoded_int(header, window->tview_len);
if (eb->version == 1) {
SVN_ERR(zlib_encode(instructions->data, instructions->len, i1));
instructions = i1;
}
append_encoded_int(header, instructions->len);
if (eb->version == 1) {
svn_stringbuf_t *temp = svn_stringbuf_create("", pool);
svn_string_t *tempstr = svn_string_create("", pool);
SVN_ERR(zlib_encode(window->new_data->data, window->new_data->len,
temp));
tempstr->data = temp->data;
tempstr->len = temp->len;
newdata = tempstr;
} else
newdata = window->new_data;
append_encoded_int(header, newdata->len);
len = header->len;
SVN_ERR(svn_stream_write(eb->output, header->data, &len));
if (instructions->len > 0) {
len = instructions->len;
SVN_ERR(svn_stream_write(eb->output, instructions->data, &len));
}
if (newdata->len > 0) {
len = newdata->len;
SVN_ERR(svn_stream_write(eb->output, newdata->data, &len));
}
svn_pool_destroy(pool);
return SVN_NO_ERROR;
}
void
svn_txdelta_to_svndiff2(svn_txdelta_window_handler_t *handler,
void **handler_baton,
svn_stream_t *output,
int svndiff_version,
apr_pool_t *pool) {
apr_pool_t *subpool = svn_pool_create(pool);
struct encoder_baton *eb;
eb = apr_palloc(subpool, sizeof(*eb));
eb->output = output;
eb->header_done = FALSE;
eb->pool = subpool;
eb->version = svndiff_version;
*handler = window_handler;
*handler_baton = eb;
}
void
svn_txdelta_to_svndiff(svn_stream_t *output,
apr_pool_t *pool,
svn_txdelta_window_handler_t *handler,
void **handler_baton) {
svn_txdelta_to_svndiff2(handler, handler_baton, output, 0, pool);
}
struct decode_baton {
svn_txdelta_window_handler_t consumer_func;
void *consumer_baton;
apr_pool_t *pool;
apr_pool_t *subpool;
svn_stringbuf_t *buffer;
svn_filesize_t last_sview_offset;
apr_size_t last_sview_len;
int header_bytes;
svn_boolean_t error_on_early_close;
unsigned char version;
};
static const unsigned char *
decode_file_offset(svn_filesize_t *val,
const unsigned char *p,
const unsigned char *end) {
*val = 0;
while (p < end) {
*val = (*val << 7) | (*p & 0x7f);
if (((*p++ >> 7) & 0x1) == 0)
return p;
}
return NULL;
}
static const unsigned char *
decode_size(apr_size_t *val,
const unsigned char *p,
const unsigned char *end) {
*val = 0;
while (p < end) {
*val = (*val << 7) | (*p & 0x7f);
if (((*p++ >> 7) & 0x1) == 0)
return p;
}
return NULL;
}
static svn_error_t *
zlib_decode(svn_stringbuf_t *in, svn_stringbuf_t *out) {
apr_size_t len;
char *oldplace = in->data;
in->data = (char *)decode_size(&len, (unsigned char *)in->data,
(unsigned char *)in->data+in->len);
in->len -= (in->data - oldplace);
if (in->len == len) {
svn_stringbuf_appendstr(out, in);
return SVN_NO_ERROR;
} else {
unsigned long zliblen;
svn_stringbuf_ensure(out, len);
zliblen = len;
if (uncompress ((unsigned char *)out->data, &zliblen,
(const unsigned char *)in->data, in->len) != Z_OK)
return svn_error_create(SVN_ERR_SVNDIFF_INVALID_COMPRESSED_DATA,
NULL,
_("Decompression of svndiff data failed"));
if (zliblen != len)
return svn_error_create(SVN_ERR_SVNDIFF_INVALID_COMPRESSED_DATA,
NULL,
_("Size of uncompressed data "
"does not match stored original length"));
out->len = zliblen;
}
return SVN_NO_ERROR;
}
static const unsigned char *
decode_instruction(svn_txdelta_op_t *op,
const unsigned char *p,
const unsigned char *end) {
if (p == end)
return NULL;
switch ((*p >> 6) & 0x3) {
case 0x0:
op->action_code = svn_txdelta_source;
break;
case 0x1:
op->action_code = svn_txdelta_target;
break;
case 0x2:
op->action_code = svn_txdelta_new;
break;
case 0x3:
return NULL;
}
op->length = *p++ & 0x3f;
if (op->length == 0) {
p = decode_size(&op->length, p, end);
if (p == NULL)
return NULL;
}
if (op->action_code != svn_txdelta_new) {
p = decode_size(&op->offset, p, end);
if (p == NULL)
return NULL;
}
return p;
}
static svn_error_t *
count_and_verify_instructions(int *ninst,
const unsigned char *p,
const unsigned char *end,
apr_size_t sview_len,
apr_size_t tview_len,
apr_size_t new_len) {
int n = 0;
svn_txdelta_op_t op;
apr_size_t tpos = 0, npos = 0;
while (p < end) {
p = decode_instruction(&op, p, end);
if (p == NULL)
return svn_error_createf
(SVN_ERR_SVNDIFF_INVALID_OPS, NULL,
_("Invalid diff stream: insn %d cannot be decoded"), n);
else if (op.length <= 0)
return svn_error_createf
(SVN_ERR_SVNDIFF_INVALID_OPS, NULL,
_("Invalid diff stream: insn %d has non-positive length"), n);
else if (op.length > tview_len - tpos)
return svn_error_createf
(SVN_ERR_SVNDIFF_INVALID_OPS, NULL,
_("Invalid diff stream: insn %d overflows the target view"), n);
switch (op.action_code) {
case svn_txdelta_source:
if (op.length > sview_len - op.offset)
return svn_error_createf
(SVN_ERR_SVNDIFF_INVALID_OPS, NULL,
_("Invalid diff stream: "
"[src] insn %d overflows the source view"), n);
break;
case svn_txdelta_target:
if (op.offset >= tpos)
return svn_error_createf
(SVN_ERR_SVNDIFF_INVALID_OPS, NULL,
_("Invalid diff stream: "
"[tgt] insn %d starts beyond the target view position"), n);
break;
case svn_txdelta_new:
if (op.length > new_len - npos)
return svn_error_createf
(SVN_ERR_SVNDIFF_INVALID_OPS, NULL,
_("Invalid diff stream: "
"[new] insn %d overflows the new data section"), n);
npos += op.length;
break;
}
tpos += op.length;
n++;
}
if (tpos != tview_len)
return svn_error_create(SVN_ERR_SVNDIFF_INVALID_OPS, NULL,
_("Delta does not fill the target window"));
if (npos != new_len)
return svn_error_create(SVN_ERR_SVNDIFF_INVALID_OPS, NULL,
_("Delta does not contain enough new data"));
*ninst = n;
return SVN_NO_ERROR;
}
static svn_error_t *
decode_window(svn_txdelta_window_t *window, svn_filesize_t sview_offset,
apr_size_t sview_len, apr_size_t tview_len, apr_size_t inslen,
apr_size_t newlen, const unsigned char *data, apr_pool_t *pool,
unsigned int version) {
const unsigned char *insend;
int ninst;
apr_size_t npos;
svn_txdelta_op_t *ops, *op;
svn_string_t *new_data = apr_palloc(pool, sizeof(*new_data));
window->sview_offset = sview_offset;
window->sview_len = sview_len;
window->tview_len = tview_len;
insend = data + inslen;
if (version == 1) {
svn_stringbuf_t *instin, *ndin;
svn_stringbuf_t *instout, *ndout;
instin = svn_stringbuf_ncreate((const char *)data, insend - data, pool);
instout = svn_stringbuf_create("", pool);
SVN_ERR(zlib_decode(instin, instout));
ndin = svn_stringbuf_ncreate((const char *)insend, newlen, pool);
ndout = svn_stringbuf_create("", pool);
SVN_ERR(zlib_decode(ndin, ndout));
newlen = ndout->len;
data = (unsigned char *)instout->data;
insend = (unsigned char *)instout->data + instout->len;
new_data->data = (const char *) ndout->data;
new_data->len = newlen;
} else {
new_data->data = (const char *) insend;
new_data->len = newlen;
}
SVN_ERR(count_and_verify_instructions(&ninst, data, insend,
sview_len, tview_len, newlen));
ops = apr_palloc(pool, ninst * sizeof(*ops));
npos = 0;
window->src_ops = 0;
for (op = ops; op < ops + ninst; op++) {
data = decode_instruction(op, data, insend);
if (op->action_code == svn_txdelta_source)
++window->src_ops;
else if (op->action_code == svn_txdelta_new) {
op->offset = npos;
npos += op->length;
}
}
assert(data == insend);
window->ops = ops;
window->num_ops = ninst;
window->new_data = new_data;
return SVN_NO_ERROR;
}
static svn_error_t *
write_handler(void *baton,
const char *buffer,
apr_size_t *len) {
struct decode_baton *db = (struct decode_baton *) baton;
const unsigned char *p, *end;
svn_filesize_t sview_offset;
apr_size_t sview_len, tview_len, inslen, newlen, remaining;
apr_size_t buflen = *len;
if (db->header_bytes < 4) {
apr_size_t nheader = 4 - db->header_bytes;
if (nheader > buflen)
nheader = buflen;
if (memcmp(buffer, "SVN\0" + db->header_bytes, nheader) == 0)
db->version = 0;
else if (memcmp(buffer, "SVN\1" + db->header_bytes, nheader) == 0)
db->version = 1;
else
return svn_error_create(SVN_ERR_SVNDIFF_INVALID_HEADER, NULL,
_("Svndiff has invalid header"));
buflen -= nheader;
buffer += nheader;
db->header_bytes += nheader;
}
svn_stringbuf_appendbytes(db->buffer, buffer, buflen);
while (1) {
apr_pool_t *newpool;
svn_txdelta_window_t window;
p = (const unsigned char *) db->buffer->data;
end = (const unsigned char *) db->buffer->data + db->buffer->len;
p = decode_file_offset(&sview_offset, p, end);
if (p == NULL)
return SVN_NO_ERROR;
p = decode_size(&sview_len, p, end);
if (p == NULL)
return SVN_NO_ERROR;
p = decode_size(&tview_len, p, end);
if (p == NULL)
return SVN_NO_ERROR;
p = decode_size(&inslen, p, end);
if (p == NULL)
return SVN_NO_ERROR;
p = decode_size(&newlen, p, end);
if (p == NULL)
return SVN_NO_ERROR;
if (sview_offset < 0 || inslen + newlen < inslen
|| sview_len + tview_len < sview_len
|| sview_offset + sview_len < sview_offset)
return svn_error_create(SVN_ERR_SVNDIFF_CORRUPT_WINDOW, NULL,
_("Svndiff contains corrupt window header"));
if (sview_len > 0
&& (sview_offset < db->last_sview_offset
|| (sview_offset + sview_len
< db->last_sview_offset + db->last_sview_len)))
return svn_error_create
(SVN_ERR_SVNDIFF_BACKWARD_VIEW, NULL,
_("Svndiff has backwards-sliding source views"));
if ((apr_size_t) (end - p) < inslen + newlen)
return SVN_NO_ERROR;
SVN_ERR(decode_window(&window, sview_offset, sview_len, tview_len,
inslen, newlen, p, db->subpool,
db->version));
SVN_ERR(db->consumer_func(&window, db->consumer_baton));
newpool = svn_pool_create(db->pool);
p += inslen + newlen;
remaining = db->buffer->data + db->buffer->len - (const char *) p;
db->buffer =
svn_stringbuf_ncreate((const char *) p, remaining, newpool);
db->last_sview_offset = sview_offset;
db->last_sview_len = sview_len;
svn_pool_destroy(db->subpool);
db->subpool = newpool;
}
}
static svn_error_t *
close_handler(void *baton) {
struct decode_baton *db = (struct decode_baton *) baton;
svn_error_t *err;
if ((db->error_on_early_close)
&& (db->header_bytes < 4 || db->buffer->len != 0))
return svn_error_create(SVN_ERR_SVNDIFF_UNEXPECTED_END, NULL,
_("Unexpected end of svndiff input"));
err = db->consumer_func(NULL, db->consumer_baton);
svn_pool_destroy(db->pool);
return err;
}
svn_stream_t *
svn_txdelta_parse_svndiff(svn_txdelta_window_handler_t handler,
void *handler_baton,
svn_boolean_t error_on_early_close,
apr_pool_t *pool) {
apr_pool_t *subpool = svn_pool_create(pool);
struct decode_baton *db = apr_palloc(pool, sizeof(*db));
svn_stream_t *stream;
db->consumer_func = handler;
db->consumer_baton = handler_baton;
db->pool = subpool;
db->subpool = svn_pool_create(subpool);
db->buffer = svn_stringbuf_create("", db->subpool);
db->last_sview_offset = 0;
db->last_sview_len = 0;
db->header_bytes = 0;
db->error_on_early_close = error_on_early_close;
stream = svn_stream_create(db, pool);
svn_stream_set_write(stream, write_handler);
svn_stream_set_close(stream, close_handler);
return stream;
}
static svn_error_t *
read_one_byte(unsigned char *byte, svn_stream_t *stream) {
char c;
apr_size_t len = 1;
SVN_ERR(svn_stream_read(stream, &c, &len));
if (len == 0)
return svn_error_create(SVN_ERR_SVNDIFF_UNEXPECTED_END, NULL,
_("Unexpected end of svndiff input"));
*byte = (unsigned char) c;
return SVN_NO_ERROR;
}
static svn_error_t *
read_one_size(apr_size_t *size, svn_stream_t *stream) {
unsigned char c;
*size = 0;
while (1) {
SVN_ERR(read_one_byte(&c, stream));
*size = (*size << 7) | (c & 0x7f);
if (!(c & 0x80))
break;
}
return SVN_NO_ERROR;
}
static svn_error_t *
read_window_header(svn_stream_t *stream, svn_filesize_t *sview_offset,
apr_size_t *sview_len, apr_size_t *tview_len,
apr_size_t *inslen, apr_size_t *newlen) {
unsigned char c;
*sview_offset = 0;
while (1) {
SVN_ERR(read_one_byte(&c, stream));
*sview_offset = (*sview_offset << 7) | (c & 0x7f);
if (!(c & 0x80))
break;
}
SVN_ERR(read_one_size(sview_len, stream));
SVN_ERR(read_one_size(tview_len, stream));
SVN_ERR(read_one_size(inslen, stream));
SVN_ERR(read_one_size(newlen, stream));
if (*sview_offset < 0 || *inslen + *newlen < *inslen
|| *sview_len + *tview_len < *sview_len
|| *sview_offset + *sview_len < *sview_offset)
return svn_error_create(SVN_ERR_SVNDIFF_CORRUPT_WINDOW, NULL,
_("Svndiff contains corrupt window header"));
return SVN_NO_ERROR;
}
svn_error_t *
svn_txdelta_read_svndiff_window(svn_txdelta_window_t **window,
svn_stream_t *stream,
int svndiff_version,
apr_pool_t *pool) {
svn_filesize_t sview_offset;
apr_size_t sview_len, tview_len, inslen, newlen, len;
unsigned char *buf;
SVN_ERR(read_window_header(stream, &sview_offset, &sview_len, &tview_len,
&inslen, &newlen));
len = inslen + newlen;
buf = apr_palloc(pool, len);
SVN_ERR(svn_stream_read(stream, (char*)buf, &len));
if (len < inslen + newlen)
return svn_error_create(SVN_ERR_SVNDIFF_UNEXPECTED_END, NULL,
_("Unexpected end of svndiff input"));
*window = apr_palloc(pool, sizeof(**window));
SVN_ERR(decode_window(*window, sview_offset, sview_len, tview_len, inslen,
newlen, buf, pool, svndiff_version));
return SVN_NO_ERROR;
}
svn_error_t *
svn_txdelta_skip_svndiff_window(apr_file_t *file,
int svndiff_version,
apr_pool_t *pool) {
svn_stream_t *stream = svn_stream_from_aprfile(file, pool);
svn_filesize_t sview_offset;
apr_size_t sview_len, tview_len, inslen, newlen;
apr_off_t offset;
SVN_ERR(read_window_header(stream, &sview_offset, &sview_len, &tview_len,
&inslen, &newlen));
offset = inslen + newlen;
return svn_io_file_seek(file, APR_CUR, &offset, pool);
}
