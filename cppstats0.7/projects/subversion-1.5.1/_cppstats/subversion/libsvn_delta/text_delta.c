#include <assert.h>
#include <string.h>
#include <apr_general.h>
#include <apr_md5.h>
#include "svn_delta.h"
#include "svn_io.h"
#include "svn_pools.h"
#include "delta.h"
struct svn_txdelta_stream_t {
void *baton;
svn_txdelta_next_window_fn_t next_window;
svn_txdelta_md5_digest_fn_t md5_digest;
};
struct txdelta_baton {
svn_stream_t *source;
svn_stream_t *target;
svn_boolean_t more_source;
svn_boolean_t more;
svn_filesize_t pos;
char *buf;
apr_md5_ctx_t context;
unsigned char digest[APR_MD5_DIGESTSIZE];
};
struct tpush_baton {
svn_stream_t *source;
svn_txdelta_window_handler_t wh;
void *whb;
apr_pool_t *pool;
char *buf;
svn_filesize_t source_offset;
apr_size_t source_len;
svn_boolean_t source_done;
apr_size_t target_len;
};
struct apply_baton {
svn_stream_t *source;
svn_stream_t *target;
apr_pool_t *pool;
char *sbuf;
apr_size_t sbuf_size;
svn_filesize_t sbuf_offset;
apr_size_t sbuf_len;
char *tbuf;
apr_size_t tbuf_size;
apr_md5_ctx_t md5_context;
unsigned char *result_digest;
const char *error_info;
};
svn_txdelta_window_t *
svn_txdelta__make_window(const svn_txdelta__ops_baton_t *build_baton,
apr_pool_t *pool) {
svn_txdelta_window_t *window;
svn_string_t *new_data = apr_palloc(pool, sizeof(*new_data));
window = apr_palloc(pool, sizeof(*window));
window->sview_offset = 0;
window->sview_len = 0;
window->tview_len = 0;
window->num_ops = build_baton->num_ops;
window->src_ops = build_baton->src_ops;
window->ops = build_baton->ops;
new_data->data = build_baton->new_data->data;
new_data->len = build_baton->new_data->len;
window->new_data = new_data;
return window;
}
static svn_txdelta_window_t *
compute_window(const char *data, apr_size_t source_len, apr_size_t target_len,
svn_filesize_t source_offset, apr_pool_t *pool) {
svn_txdelta__ops_baton_t build_baton = { 0 };
svn_txdelta_window_t *window;
build_baton.new_data = svn_stringbuf_create("", pool);
if (source_len == 0)
svn_txdelta__vdelta(&build_baton, data, source_len, target_len, pool);
else
svn_txdelta__xdelta(&build_baton, data, source_len, target_len, pool);
window = svn_txdelta__make_window(&build_baton, pool);
window->sview_offset = source_offset;
window->sview_len = source_len;
window->tview_len = target_len;
return window;
}
svn_txdelta_window_t *
svn_txdelta_window_dup(const svn_txdelta_window_t *window,
apr_pool_t *pool) {
svn_txdelta__ops_baton_t build_baton = { 0 };
svn_txdelta_window_t *new_window;
const apr_size_t ops_size = (window->num_ops * sizeof(*build_baton.ops));
build_baton.num_ops = window->num_ops;
build_baton.src_ops = window->src_ops;
build_baton.ops_size = window->num_ops;
build_baton.ops = apr_palloc(pool, ops_size);
memcpy(build_baton.ops, window->ops, ops_size);
build_baton.new_data =
svn_stringbuf_create_from_string(window->new_data, pool);
new_window = svn_txdelta__make_window(&build_baton, pool);
new_window->sview_offset = window->sview_offset;
new_window->sview_len = window->sview_len;
new_window->tview_len = window->tview_len;
return new_window;
}
svn_txdelta_window_t *
svn_txdelta__copy_window(const svn_txdelta_window_t *window,
apr_pool_t *pool);
svn_txdelta_window_t *
svn_txdelta__copy_window(const svn_txdelta_window_t *window,
apr_pool_t *pool) {
return svn_txdelta_window_dup(window, pool);
}
void
svn_txdelta__insert_op(svn_txdelta__ops_baton_t *build_baton,
enum svn_delta_action opcode,
apr_size_t offset,
apr_size_t length,
const char *new_data,
apr_pool_t *pool) {
svn_txdelta_op_t *op;
if (build_baton->num_ops > 0) {
op = &build_baton->ops[build_baton->num_ops - 1];
if (op->action_code == opcode
&& (opcode == svn_txdelta_new
|| op->offset + op->length == offset)) {
op->length += length;
if (opcode == svn_txdelta_new)
svn_stringbuf_appendbytes(build_baton->new_data,
new_data, length);
return;
}
}
if (build_baton->num_ops == build_baton->ops_size) {
svn_txdelta_op_t *const old_ops = build_baton->ops;
int const new_ops_size = (build_baton->ops_size == 0
? 16 : 2 * build_baton->ops_size);
build_baton->ops =
apr_palloc(pool, new_ops_size * sizeof(*build_baton->ops));
if (old_ops)
memcpy(build_baton->ops, old_ops,
build_baton->ops_size * sizeof(*build_baton->ops));
build_baton->ops_size = new_ops_size;
}
op = &build_baton->ops[build_baton->num_ops];
switch (opcode) {
case svn_txdelta_source:
++build_baton->src_ops;
case svn_txdelta_target:
op->action_code = opcode;
op->offset = offset;
op->length = length;
break;
case svn_txdelta_new:
op->action_code = opcode;
op->offset = build_baton->new_data->len;
op->length = length;
svn_stringbuf_appendbytes(build_baton->new_data, new_data, length);
break;
default:
assert(!"unknown delta op.");
}
++build_baton->num_ops;
}
svn_txdelta_stream_t *
svn_txdelta_stream_create(void *baton,
svn_txdelta_next_window_fn_t next_window,
svn_txdelta_md5_digest_fn_t md5_digest,
apr_pool_t *pool) {
svn_txdelta_stream_t *stream = apr_palloc(pool, sizeof(*stream));
stream->baton = baton;
stream->next_window = next_window;
stream->md5_digest = md5_digest;
return stream;
}
svn_error_t *
svn_txdelta_next_window(svn_txdelta_window_t **window,
svn_txdelta_stream_t *stream,
apr_pool_t *pool) {
return stream->next_window(window, stream->baton, pool);
}
const unsigned char *
svn_txdelta_md5_digest(svn_txdelta_stream_t *stream) {
return stream->md5_digest(stream->baton);
}
static svn_error_t *
txdelta_next_window(svn_txdelta_window_t **window,
void *baton,
apr_pool_t *pool) {
struct txdelta_baton *b = baton;
apr_size_t source_len = SVN_DELTA_WINDOW_SIZE;
apr_size_t target_len = SVN_DELTA_WINDOW_SIZE;
if (b->more_source) {
SVN_ERR(svn_stream_read(b->source, b->buf, &source_len));
b->more_source = (source_len == SVN_DELTA_WINDOW_SIZE);
} else
source_len = 0;
SVN_ERR(svn_stream_read(b->target, b->buf + source_len,
&target_len));
b->pos += source_len;
if (target_len == 0) {
apr_md5_final(b->digest, &(b->context));
*window = NULL;
b->more = FALSE;
return SVN_NO_ERROR;
} else {
apr_md5_update(&(b->context), b->buf + source_len,
target_len);
}
*window = compute_window(b->buf, source_len, target_len,
b->pos - source_len, pool);
return SVN_NO_ERROR;
}
static const unsigned char *
txdelta_md5_digest(void *baton) {
struct txdelta_baton *b = baton;
if (b->more)
return NULL;
return b->digest;
}
void
svn_txdelta(svn_txdelta_stream_t **stream,
svn_stream_t *source,
svn_stream_t *target,
apr_pool_t *pool) {
struct txdelta_baton *b = apr_palloc(pool, sizeof(*b));
b->source = source;
b->target = target;
b->more_source = TRUE;
b->more = TRUE;
b->pos = 0;
b->buf = apr_palloc(pool, 2 * SVN_DELTA_WINDOW_SIZE);
apr_md5_init(&(b->context));
*stream = svn_txdelta_stream_create(b, txdelta_next_window,
txdelta_md5_digest, pool);
}
static svn_error_t *
tpush_write_handler(void *baton, const char *data, apr_size_t *len) {
struct tpush_baton *tb = baton;
apr_size_t chunk_len, data_len = *len;
apr_pool_t *pool = svn_pool_create(tb->pool);
svn_txdelta_window_t *window;
while (data_len > 0) {
svn_pool_clear(pool);
if (tb->source_len == 0 && !tb->source_done) {
tb->source_len = SVN_DELTA_WINDOW_SIZE;
SVN_ERR(svn_stream_read(tb->source, tb->buf, &tb->source_len));
if (tb->source_len < SVN_DELTA_WINDOW_SIZE)
tb->source_done = TRUE;
}
chunk_len = SVN_DELTA_WINDOW_SIZE - tb->target_len;
if (chunk_len > data_len)
chunk_len = data_len;
memcpy(tb->buf + tb->source_len + tb->target_len, data, chunk_len);
data += chunk_len;
data_len -= chunk_len;
tb->target_len += chunk_len;
if (tb->target_len == SVN_DELTA_WINDOW_SIZE) {
window = compute_window(tb->buf, tb->source_len, tb->target_len,
tb->source_offset, pool);
SVN_ERR(tb->wh(window, tb->whb));
tb->source_offset += tb->source_len;
tb->source_len = 0;
tb->target_len = 0;
}
}
svn_pool_destroy(pool);
return SVN_NO_ERROR;
}
static svn_error_t *
tpush_close_handler(void *baton) {
struct tpush_baton *tb = baton;
svn_txdelta_window_t *window;
if (tb->target_len > 0) {
window = compute_window(tb->buf, tb->source_len, tb->target_len,
tb->source_offset, tb->pool);
SVN_ERR(tb->wh(window, tb->whb));
}
SVN_ERR(tb->wh(NULL, tb->whb));
return SVN_NO_ERROR;
}
svn_stream_t *
svn_txdelta_target_push(svn_txdelta_window_handler_t handler,
void *handler_baton, svn_stream_t *source,
apr_pool_t *pool) {
struct tpush_baton *tb;
svn_stream_t *stream;
tb = apr_palloc(pool, sizeof(*tb));
tb->source = source;
tb->wh = handler;
tb->whb = handler_baton;
tb->pool = pool;
tb->buf = apr_palloc(pool, 2 * SVN_DELTA_WINDOW_SIZE);
tb->source_offset = 0;
tb->source_len = 0;
tb->source_done = FALSE;
tb->target_len = 0;
stream = svn_stream_create(tb, pool);
svn_stream_set_write(stream, tpush_write_handler);
svn_stream_set_close(stream, tpush_close_handler);
return stream;
}
static APR_INLINE void
size_buffer(char **buf, apr_size_t *buf_size,
apr_size_t view_len, apr_pool_t *pool) {
if (view_len > *buf_size) {
*buf_size *= 2;
if (*buf_size < view_len)
*buf_size = view_len;
*buf = apr_palloc(pool, *buf_size);
}
}
void
svn_txdelta_apply_instructions(svn_txdelta_window_t *window,
const char *sbuf, char *tbuf,
apr_size_t *tlen) {
const svn_txdelta_op_t *op;
apr_size_t i, j, tpos = 0;
for (op = window->ops; op < window->ops + window->num_ops; op++) {
const apr_size_t buf_len = (op->length < *tlen - tpos
? op->length : *tlen - tpos);
assert(tpos + op->length <= window->tview_len);
switch (op->action_code) {
case svn_txdelta_source:
assert(op->offset + op->length <= window->sview_len);
memcpy(tbuf + tpos, sbuf + op->offset, buf_len);
break;
case svn_txdelta_target:
assert(op->offset < tpos);
for (i = op->offset, j = tpos; i < op->offset + buf_len; i++)
tbuf[j++] = tbuf[i];
break;
case svn_txdelta_new:
assert(op->offset + op->length <= window->new_data->len);
memcpy(tbuf + tpos,
window->new_data->data + op->offset,
buf_len);
break;
default:
assert(!"Invalid delta instruction code");
}
tpos += op->length;
if (tpos >= *tlen)
return;
}
assert(tpos == window->tview_len);
*tlen = tpos;
}
void
svn_txdelta__apply_instructions(svn_txdelta_window_t *window,
const char *sbuf, char *tbuf,
apr_size_t *tlen);
void
svn_txdelta__apply_instructions(svn_txdelta_window_t *window,
const char *sbuf, char *tbuf,
apr_size_t *tlen) {
svn_txdelta_apply_instructions(window, sbuf, tbuf, tlen);
}
static svn_error_t *
apply_window(svn_txdelta_window_t *window, void *baton) {
struct apply_baton *ab = (struct apply_baton *) baton;
apr_size_t len;
svn_error_t *err;
if (window == NULL) {
if (ab->result_digest)
apr_md5_final(ab->result_digest, &(ab->md5_context));
err = svn_stream_close(ab->target);
svn_pool_destroy(ab->pool);
return err;
}
assert(window->sview_len == 0
|| (window->sview_offset >= ab->sbuf_offset
&& (window->sview_offset + window->sview_len
>= ab->sbuf_offset + ab->sbuf_len)));
size_buffer(&ab->tbuf, &ab->tbuf_size, window->tview_len, ab->pool);
if (window->sview_offset != ab->sbuf_offset
|| window->sview_len > ab->sbuf_size) {
char *old_sbuf = ab->sbuf;
size_buffer(&ab->sbuf, &ab->sbuf_size, window->sview_len, ab->pool);
if (ab->sbuf_offset + ab->sbuf_len > window->sview_offset) {
apr_size_t start =
(apr_size_t)(window->sview_offset - ab->sbuf_offset);
memmove(ab->sbuf, old_sbuf + start, ab->sbuf_len - start);
ab->sbuf_len -= start;
} else
ab->sbuf_len = 0;
ab->sbuf_offset = window->sview_offset;
}
if (ab->sbuf_len < window->sview_len) {
len = window->sview_len - ab->sbuf_len;
err = svn_stream_read(ab->source, ab->sbuf + ab->sbuf_len, &len);
if (err == SVN_NO_ERROR && len != window->sview_len - ab->sbuf_len)
err = svn_error_create(SVN_ERR_INCOMPLETE_DATA, NULL,
"Delta source ended unexpectedly");
if (err != SVN_NO_ERROR)
return err;
ab->sbuf_len = window->sview_len;
}
len = window->tview_len;
svn_txdelta_apply_instructions(window, ab->sbuf, ab->tbuf, &len);
assert(len == window->tview_len);
if (ab->result_digest)
apr_md5_update(&(ab->md5_context), ab->tbuf, len);
return svn_stream_write(ab->target, ab->tbuf, &len);
}
void
svn_txdelta_apply(svn_stream_t *source,
svn_stream_t *target,
unsigned char *result_digest,
const char *error_info,
apr_pool_t *pool,
svn_txdelta_window_handler_t *handler,
void **handler_baton) {
apr_pool_t *subpool = svn_pool_create(pool);
struct apply_baton *ab;
ab = apr_palloc(subpool, sizeof(*ab));
ab->source = source;
ab->target = target;
ab->pool = subpool;
ab->sbuf = NULL;
ab->sbuf_size = 0;
ab->sbuf_offset = 0;
ab->sbuf_len = 0;
ab->tbuf = NULL;
ab->tbuf_size = 0;
ab->result_digest = result_digest;
if (result_digest)
apr_md5_init(&(ab->md5_context));
if (error_info)
ab->error_info = apr_pstrdup(subpool, error_info);
else
ab->error_info = NULL;
*handler = apply_window;
*handler_baton = ab;
}
svn_error_t *
svn_txdelta_send_string(const svn_string_t *string,
svn_txdelta_window_handler_t handler,
void *handler_baton,
apr_pool_t *pool) {
svn_txdelta_window_t window = { 0 };
svn_txdelta_op_t op;
op.action_code = svn_txdelta_new;
op.offset = 0;
op.length = string->len;
window.tview_len = string->len;
window.num_ops = 1;
window.ops = &op;
window.new_data = string;
SVN_ERR((*handler)(&window, handler_baton));
SVN_ERR((*handler)(NULL, handler_baton));
return SVN_NO_ERROR;
}
svn_error_t *svn_txdelta_send_stream(svn_stream_t *stream,
svn_txdelta_window_handler_t handler,
void *handler_baton,
unsigned char *digest,
apr_pool_t *pool) {
svn_txdelta_stream_t *txstream;
svn_error_t *err;
svn_txdelta(&txstream, svn_stream_empty(pool), stream, pool);
err = svn_txdelta_send_txstream(txstream, handler, handler_baton, pool);
if (digest && (! err)) {
const unsigned char *result_md5;
result_md5 = svn_txdelta_md5_digest(txstream);
memcpy(digest, result_md5, APR_MD5_DIGESTSIZE);
}
return err;
}
svn_error_t *svn_txdelta_send_txstream(svn_txdelta_stream_t *txstream,
svn_txdelta_window_handler_t handler,
void *handler_baton,
apr_pool_t *pool) {
svn_txdelta_window_t *window;
apr_pool_t *wpool = svn_pool_create(pool);
do {
svn_pool_clear(wpool);
SVN_ERR(svn_txdelta_next_window(&window, txstream, wpool));
SVN_ERR((*handler)(window, handler_baton));
} while (window != NULL);
svn_pool_destroy(wpool);
return SVN_NO_ERROR;
}
