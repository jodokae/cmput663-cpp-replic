#include <assert.h>
#include <apr_md5.h>
#define APU_WANT_DB
#include <apu_want.h>
#include "svn_fs.h"
#include "svn_pools.h"
#include "svn_md5.h"
#include "fs.h"
#include "err.h"
#include "trail.h"
#include "reps-strings.h"
#include "bdb/reps-table.h"
#include "bdb/strings-table.h"
#include "../libsvn_fs/fs-loader.h"
#include "svn_private_config.h"
static svn_boolean_t rep_is_mutable(representation_t *rep,
const char *txn_id) {
if ((! rep->txn_id) || (strcmp(rep->txn_id, txn_id) != 0))
return FALSE;
return TRUE;
}
#define UNKNOWN_NODE_KIND(x) svn_error_createf (SVN_ERR_FS_CORRUPT, NULL, _("Unknown node kind for representation '%s'"), x)
static representation_t *
make_fulltext_rep(const char *str_key,
const char *txn_id,
const unsigned char *checksum,
apr_pool_t *pool)
{
representation_t *rep = apr_pcalloc(pool, sizeof(*rep));
if (txn_id && *txn_id)
rep->txn_id = apr_pstrdup(pool, txn_id);
rep->kind = rep_kind_fulltext;
if (checksum)
memcpy(rep->checksum, checksum, APR_MD5_DIGESTSIZE);
else
memset(rep->checksum, 0, APR_MD5_DIGESTSIZE);
rep->contents.fulltext.string_key
= str_key ? apr_pstrdup(pool, str_key) : NULL;
return rep;
}
static svn_error_t *
delta_string_keys(apr_array_header_t **keys,
const representation_t *rep,
apr_pool_t *pool) {
const char *key;
int i;
apr_array_header_t *chunks;
if (rep->kind != rep_kind_delta)
return svn_error_create
(SVN_ERR_FS_GENERAL, NULL,
_("Representation is not of type 'delta'"));
chunks = rep->contents.delta.chunks;
*keys = apr_array_make(pool, chunks->nelts, sizeof(key));
if (! chunks->nelts)
return SVN_NO_ERROR;
for (i = 0; i < chunks->nelts; i++) {
rep_delta_chunk_t *chunk = APR_ARRAY_IDX(chunks, i, rep_delta_chunk_t *);
key = apr_pstrdup(pool, chunk->string_key);
APR_ARRAY_PUSH(*keys, const char *) = key;
}
return SVN_NO_ERROR;
}
static svn_error_t *
delete_strings(apr_array_header_t *keys,
svn_fs_t *fs,
trail_t *trail,
apr_pool_t *pool) {
int i;
const char *str_key;
apr_pool_t *subpool = svn_pool_create(pool);
for (i = 0; i < keys->nelts; i++) {
svn_pool_clear(subpool);
str_key = APR_ARRAY_IDX(keys, i, const char *);
SVN_ERR(svn_fs_bdb__string_delete(fs, str_key, trail, subpool));
}
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
struct compose_handler_baton {
svn_txdelta_window_t *window;
apr_pool_t *window_pool;
char *source_buf;
trail_t *trail;
svn_boolean_t done;
svn_boolean_t init;
};
static svn_error_t *
compose_handler(svn_txdelta_window_t *window, void *baton) {
struct compose_handler_baton *cb = baton;
assert(!cb->done || window == NULL);
assert(cb->trail && cb->trail->pool);
if (!cb->init && !window)
return SVN_NO_ERROR;
assert(!cb->source_buf);
if (cb->window) {
if (window && (window->sview_len == 0 || window->src_ops == 0)) {
apr_size_t source_len = window->tview_len;
assert(cb->window->sview_len == source_len);
cb->source_buf = apr_palloc(cb->window_pool, source_len);
svn_txdelta_apply_instructions(window, NULL,
cb->source_buf, &source_len);
cb->done = TRUE;
} else {
apr_pool_t *composite_pool = svn_pool_create(cb->trail->pool);
svn_txdelta_window_t *composite;
composite = svn_txdelta_compose_windows(window, cb->window,
composite_pool);
svn_pool_destroy(cb->window_pool);
cb->window = composite;
cb->window_pool = composite_pool;
cb->done = (composite->sview_len == 0 || composite->src_ops == 0);
}
} else if (window) {
apr_pool_t *window_pool = svn_pool_create(cb->trail->pool);
assert(cb->window_pool == NULL);
cb->window = svn_txdelta_window_dup(window, window_pool);
cb->window_pool = window_pool;
cb->done = (window->sview_len == 0 || window->src_ops == 0);
} else
cb->done = TRUE;
cb->init = FALSE;
return SVN_NO_ERROR;
}
static svn_error_t *
get_one_window(struct compose_handler_baton *cb,
svn_fs_t *fs,
representation_t *rep,
int cur_chunk) {
svn_stream_t *wstream;
char diffdata[4096];
svn_filesize_t off;
apr_size_t amt;
const char *str_key;
apr_array_header_t *chunks = rep->contents.delta.chunks;
rep_delta_chunk_t *this_chunk, *first_chunk;
cb->init = TRUE;
if (chunks->nelts <= cur_chunk)
return compose_handler(NULL, cb);
wstream = svn_txdelta_parse_svndiff(compose_handler, cb, TRUE,
cb->trail->pool);
first_chunk = APR_ARRAY_IDX(chunks, 0, rep_delta_chunk_t*);
diffdata[0] = 'S';
diffdata[1] = 'V';
diffdata[2] = 'N';
diffdata[3] = (char) (first_chunk->version);
amt = 4;
SVN_ERR(svn_stream_write(wstream, diffdata, &amt));
this_chunk = APR_ARRAY_IDX(chunks, cur_chunk, rep_delta_chunk_t*);
str_key = this_chunk->string_key;
off = 0;
do {
amt = sizeof(diffdata);
SVN_ERR(svn_fs_bdb__string_read(fs, str_key, diffdata,
off, &amt, cb->trail,
cb->trail->pool));
off += amt;
SVN_ERR(svn_stream_write(wstream, diffdata, &amt));
} while (amt != 0);
SVN_ERR(svn_stream_close(wstream));
assert(!cb->init);
assert(cb->window != NULL);
assert(cb->window_pool != NULL);
return SVN_NO_ERROR;
}
static svn_error_t *
rep_undeltify_range(svn_fs_t *fs,
apr_array_header_t *deltas,
representation_t *fulltext,
int cur_chunk,
char *buf,
apr_size_t offset,
apr_size_t *len,
trail_t *trail,
apr_pool_t *pool) {
apr_size_t len_read = 0;
do {
struct compose_handler_baton cb = { 0 };
char *source_buf, *target_buf;
apr_size_t target_len;
int cur_rep;
cb.trail = trail;
cb.done = FALSE;
for (cur_rep = 0; !cb.done && cur_rep < deltas->nelts; ++cur_rep) {
representation_t *const rep =
APR_ARRAY_IDX(deltas, cur_rep, representation_t*);
SVN_ERR(get_one_window(&cb, fs, rep, cur_chunk));
}
if (!cb.window)
break;
assert(cb.window->sview_len > 0 || cb.window->src_ops == 0);
if (cb.source_buf) {
source_buf = cb.source_buf;
} else if (fulltext && cb.window->sview_len > 0 && cb.window->src_ops > 0) {
apr_size_t source_len = cb.window->sview_len;
source_buf = apr_palloc(cb.window_pool, source_len);
SVN_ERR(svn_fs_bdb__string_read
(fs, fulltext->contents.fulltext.string_key,
source_buf, cb.window->sview_offset, &source_len,
trail, pool));
if (source_len != cb.window->sview_len)
return svn_error_create
(SVN_ERR_FS_CORRUPT, NULL,
_("Svndiff source length inconsistency"));
} else {
source_buf = NULL;
}
if (offset > 0) {
target_len = *len - len_read + offset;
target_buf = apr_palloc(cb.window_pool, target_len);
} else {
target_len = *len - len_read;
target_buf = buf;
}
svn_txdelta_apply_instructions(cb.window, source_buf,
target_buf, &target_len);
if (offset > 0) {
assert(target_len > offset);
target_len -= offset;
memcpy(buf, target_buf + offset, target_len);
offset = 0;
}
svn_pool_destroy(cb.window_pool);
len_read += target_len;
buf += target_len;
++cur_chunk;
} while (len_read < *len);
*len = len_read;
return SVN_NO_ERROR;
}
static int
get_chunk_offset(representation_t *rep,
svn_filesize_t rep_offset,
apr_size_t *chunk_offset) {
const apr_array_header_t *chunks = rep->contents.delta.chunks;
int cur_chunk;
assert(chunks->nelts);
for (cur_chunk = 0; cur_chunk < chunks->nelts; ++cur_chunk) {
const rep_delta_chunk_t *const this_chunk
= APR_ARRAY_IDX(chunks, cur_chunk, rep_delta_chunk_t*);
if ((this_chunk->offset + this_chunk->size) > rep_offset) {
assert(this_chunk->offset <= rep_offset);
assert(rep_offset - this_chunk->offset < SVN_MAX_OBJECT_SIZE);
*chunk_offset = (apr_size_t) (rep_offset - this_chunk->offset);
return cur_chunk;
}
}
return -1;
}
static svn_error_t *
rep_read_range(svn_fs_t *fs,
const char *rep_key,
svn_filesize_t offset,
char *buf,
apr_size_t *len,
trail_t *trail,
apr_pool_t *pool) {
representation_t *rep;
apr_size_t chunk_offset;
SVN_ERR(svn_fs_bdb__read_rep(&rep, fs, rep_key, trail, pool));
if (rep->kind == rep_kind_fulltext) {
SVN_ERR(svn_fs_bdb__string_read(fs, rep->contents.fulltext.string_key,
buf, offset, len, trail, pool));
} else if (rep->kind == rep_kind_delta) {
const int cur_chunk = get_chunk_offset(rep, offset, &chunk_offset);
if (cur_chunk < 0)
*len = 0;
else {
svn_error_t *err;
const char *first_rep_key = rep_key;
apr_array_header_t *reps =
apr_array_make(pool, 666, sizeof(rep));
do {
const rep_delta_chunk_t *const first_chunk
= APR_ARRAY_IDX(rep->contents.delta.chunks,
0, rep_delta_chunk_t*);
const rep_delta_chunk_t *const chunk
= APR_ARRAY_IDX(rep->contents.delta.chunks,
cur_chunk, rep_delta_chunk_t*);
if (first_chunk->version != chunk->version)
return svn_error_createf
(SVN_ERR_FS_CORRUPT, NULL,
_("Diff version inconsistencies in representation '%s'"),
rep_key);
rep_key = chunk->rep_key;
APR_ARRAY_PUSH(reps, representation_t *) = rep;
SVN_ERR(svn_fs_bdb__read_rep(&rep, fs, rep_key,
trail, pool));
} while (rep->kind == rep_kind_delta
&& rep->contents.delta.chunks->nelts > cur_chunk);
if (rep->kind != rep_kind_delta && rep->kind != rep_kind_fulltext)
return UNKNOWN_NODE_KIND(rep_key);
if (rep->kind == rep_kind_delta)
rep = NULL;
err = rep_undeltify_range(fs, reps, rep, cur_chunk, buf,
chunk_offset, len, trail, pool);
if (err) {
if (err->apr_err == SVN_ERR_FS_CORRUPT)
return svn_error_createf
(SVN_ERR_FS_CORRUPT, err,
_("Corruption detected whilst reading delta chain from "
"representation '%s' to '%s'"), first_rep_key, rep_key);
else
return err;
}
}
} else
return UNKNOWN_NODE_KIND(rep_key);
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_base__get_mutable_rep(const char **new_rep_key,
const char *rep_key,
svn_fs_t *fs,
const char *txn_id,
trail_t *trail,
apr_pool_t *pool) {
representation_t *rep = NULL;
const char *new_str = NULL;
if (rep_key && (rep_key[0] != '\0')) {
SVN_ERR(svn_fs_bdb__read_rep(&rep, fs, rep_key, trail, pool));
if (rep_is_mutable(rep, txn_id)) {
*new_rep_key = rep_key;
return SVN_NO_ERROR;
}
}
SVN_ERR(svn_fs_bdb__string_append(fs, &new_str, 0, NULL, trail, pool));
rep = make_fulltext_rep(new_str, txn_id,
svn_md5_empty_string_digest(), pool);
SVN_ERR(svn_fs_bdb__write_new_rep(new_rep_key, fs, rep, trail, pool));
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_base__delete_rep_if_mutable(svn_fs_t *fs,
const char *rep_key,
const char *txn_id,
trail_t *trail,
apr_pool_t *pool) {
representation_t *rep;
SVN_ERR(svn_fs_bdb__read_rep(&rep, fs, rep_key, trail, pool));
if (! rep_is_mutable(rep, txn_id))
return SVN_NO_ERROR;
if (rep->kind == rep_kind_fulltext) {
SVN_ERR(svn_fs_bdb__string_delete(fs,
rep->contents.fulltext.string_key,
trail, pool));
} else if (rep->kind == rep_kind_delta) {
apr_array_header_t *keys;
SVN_ERR(delta_string_keys(&keys, rep, pool));
SVN_ERR(delete_strings(keys, fs, trail, pool));
} else
return UNKNOWN_NODE_KIND(rep_key);
SVN_ERR(svn_fs_bdb__delete_rep(fs, rep_key, trail, pool));
return SVN_NO_ERROR;
}
struct rep_read_baton {
svn_fs_t *fs;
const char *rep_key;
svn_filesize_t offset;
trail_t *trail;
struct apr_md5_ctx_t md5_context;
svn_filesize_t size;
svn_boolean_t checksum_finalized;
apr_pool_t *pool;
};
static svn_error_t *
rep_read_get_baton(struct rep_read_baton **rb_p,
svn_fs_t *fs,
const char *rep_key,
svn_boolean_t use_trail_for_reads,
trail_t *trail,
apr_pool_t *pool) {
struct rep_read_baton *b;
b = apr_pcalloc(pool, sizeof(*b));
apr_md5_init(&(b->md5_context));
if (rep_key)
SVN_ERR(svn_fs_base__rep_contents_size(&(b->size), fs, rep_key,
trail, pool));
else
b->size = 0;
b->checksum_finalized = FALSE;
b->fs = fs;
b->trail = use_trail_for_reads ? trail : NULL;
b->pool = pool;
b->rep_key = rep_key;
b->offset = 0;
*rb_p = b;
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_base__rep_contents_size(svn_filesize_t *size_p,
svn_fs_t *fs,
const char *rep_key,
trail_t *trail,
apr_pool_t *pool) {
representation_t *rep;
SVN_ERR(svn_fs_bdb__read_rep(&rep, fs, rep_key, trail, pool));
if (rep->kind == rep_kind_fulltext) {
SVN_ERR(svn_fs_bdb__string_size(size_p, fs,
rep->contents.fulltext.string_key,
trail, pool));
} else if (rep->kind == rep_kind_delta) {
apr_array_header_t *chunks = rep->contents.delta.chunks;
rep_delta_chunk_t *last_chunk;
assert(chunks->nelts);
last_chunk = APR_ARRAY_IDX(chunks, chunks->nelts - 1,
rep_delta_chunk_t *);
*size_p = last_chunk->offset + last_chunk->size;
} else
return UNKNOWN_NODE_KIND(rep_key);
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_base__rep_contents_checksum(unsigned char digest[],
svn_fs_t *fs,
const char *rep_key,
trail_t *trail,
apr_pool_t *pool) {
representation_t *rep;
SVN_ERR(svn_fs_bdb__read_rep(&rep, fs, rep_key, trail, pool));
memcpy(digest, rep->checksum, APR_MD5_DIGESTSIZE);
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_base__rep_contents(svn_string_t *str,
svn_fs_t *fs,
const char *rep_key,
trail_t *trail,
apr_pool_t *pool) {
svn_filesize_t contents_size;
apr_size_t len;
char *data;
SVN_ERR(svn_fs_base__rep_contents_size(&contents_size, fs, rep_key,
trail, pool));
if (contents_size > SVN_MAX_OBJECT_SIZE)
return svn_error_createf
(SVN_ERR_FS_GENERAL, NULL,
_("Rep contents are too large: "
"got %s, limit is %s"),
apr_psprintf(pool, "%" SVN_FILESIZE_T_FMT, contents_size),
apr_psprintf(pool, "%" APR_SIZE_T_FMT, SVN_MAX_OBJECT_SIZE));
else
str->len = (apr_size_t) contents_size;
data = apr_palloc(pool, str->len);
str->data = data;
len = str->len;
SVN_ERR(rep_read_range(fs, rep_key, 0, data, &len, trail, pool));
if (len != str->len)
return svn_error_createf
(SVN_ERR_FS_CORRUPT, NULL,
_("Failure reading rep '%s'"), rep_key);
{
representation_t *rep;
apr_md5_ctx_t md5_context;
unsigned char checksum[APR_MD5_DIGESTSIZE];
apr_md5_init(&md5_context);
apr_md5_update(&md5_context, str->data, str->len);
apr_md5_final(checksum, &md5_context);
SVN_ERR(svn_fs_bdb__read_rep(&rep, fs, rep_key, trail, pool));
if (! svn_md5_digests_match(checksum, rep->checksum))
return svn_error_createf
(SVN_ERR_FS_CORRUPT, NULL,
_("Checksum mismatch on rep '%s':\n"
" expected: %s\n"
" actual: %s\n"), rep_key,
svn_md5_digest_to_cstring_display(rep->checksum, pool),
svn_md5_digest_to_cstring_display(checksum, pool));
}
return SVN_NO_ERROR;
}
struct read_rep_args {
struct rep_read_baton *rb;
char *buf;
apr_size_t *len;
};
static svn_error_t *
txn_body_read_rep(void *baton, trail_t *trail) {
struct read_rep_args *args = baton;
if (args->rb->rep_key) {
SVN_ERR(rep_read_range(args->rb->fs,
args->rb->rep_key,
args->rb->offset,
args->buf,
args->len,
trail,
trail->pool));
args->rb->offset += *(args->len);
if (! args->rb->checksum_finalized) {
apr_md5_update(&(args->rb->md5_context), args->buf, *(args->len));
if (args->rb->offset == args->rb->size) {
representation_t *rep;
unsigned char checksum[APR_MD5_DIGESTSIZE];
apr_md5_final(checksum, &(args->rb->md5_context));
args->rb->checksum_finalized = TRUE;
SVN_ERR(svn_fs_bdb__read_rep(&rep, args->rb->fs,
args->rb->rep_key,
trail, trail->pool));
if (! svn_md5_digests_match(checksum, rep->checksum))
return svn_error_createf
(SVN_ERR_FS_CORRUPT, NULL,
_("Checksum mismatch on rep '%s':\n"
" expected: %s\n"
" actual: %s\n"), args->rb->rep_key,
svn_md5_digest_to_cstring_display(rep->checksum,
trail->pool),
svn_md5_digest_to_cstring_display(checksum, trail->pool));
}
}
} else if (args->rb->offset > 0) {
return
svn_error_create
(SVN_ERR_FS_REP_CHANGED, NULL,
_("Null rep, but offset past zero already"));
} else
*(args->len) = 0;
return SVN_NO_ERROR;
}
static svn_error_t *
rep_read_contents(void *baton, char *buf, apr_size_t *len) {
struct rep_read_baton *rb = baton;
struct read_rep_args args;
args.rb = rb;
args.buf = buf;
args.len = len;
if (rb->trail)
SVN_ERR(txn_body_read_rep(&args, rb->trail));
else {
apr_pool_t *subpool = svn_pool_create(rb->pool);
SVN_ERR(svn_fs_base__retry_txn(rb->fs,
txn_body_read_rep,
&args,
subpool));
svn_pool_destroy(subpool);
}
return SVN_NO_ERROR;
}
struct rep_write_baton {
svn_fs_t *fs;
const char *rep_key;
const char *txn_id;
trail_t *trail;
struct apr_md5_ctx_t md5_context;
unsigned char md5_digest[APR_MD5_DIGESTSIZE];
svn_boolean_t finalized;
apr_pool_t *pool;
};
static struct rep_write_baton *
rep_write_get_baton(svn_fs_t *fs,
const char *rep_key,
const char *txn_id,
trail_t *trail,
apr_pool_t *pool) {
struct rep_write_baton *b;
b = apr_pcalloc(pool, sizeof(*b));
apr_md5_init(&(b->md5_context));
b->fs = fs;
b->trail = trail;
b->pool = pool;
b->rep_key = rep_key;
b->txn_id = txn_id;
return b;
}
static svn_error_t *
rep_write(svn_fs_t *fs,
const char *rep_key,
const char *buf,
apr_size_t len,
const char *txn_id,
trail_t *trail,
apr_pool_t *pool) {
representation_t *rep;
SVN_ERR(svn_fs_bdb__read_rep(&rep, fs, rep_key, trail, pool));
if (! rep_is_mutable(rep, txn_id))
return svn_error_createf
(SVN_ERR_FS_REP_NOT_MUTABLE, NULL,
_("Rep '%s' is not mutable"), rep_key);
if (rep->kind == rep_kind_fulltext) {
SVN_ERR(svn_fs_bdb__string_append
(fs, &(rep->contents.fulltext.string_key), len, buf,
trail, pool));
} else if (rep->kind == rep_kind_delta) {
return svn_error_createf
(SVN_ERR_FS_CORRUPT, NULL,
_("Rep '%s' both mutable and non-fulltext"), rep_key);
} else
return UNKNOWN_NODE_KIND(rep_key);
return SVN_NO_ERROR;
}
struct write_rep_args {
struct rep_write_baton *wb;
const char *buf;
apr_size_t len;
};
static svn_error_t *
txn_body_write_rep(void *baton, trail_t *trail) {
struct write_rep_args *args = baton;
SVN_ERR(rep_write(args->wb->fs,
args->wb->rep_key,
args->buf,
args->len,
args->wb->txn_id,
trail,
trail->pool));
apr_md5_update(&(args->wb->md5_context), args->buf, args->len);
return SVN_NO_ERROR;
}
static svn_error_t *
rep_write_contents(void *baton,
const char *buf,
apr_size_t *len) {
struct rep_write_baton *wb = baton;
struct write_rep_args args;
args.wb = wb;
args.buf = buf;
args.len = *len;
if (wb->trail)
SVN_ERR(txn_body_write_rep(&args, wb->trail));
else {
apr_pool_t *subpool = svn_pool_create(wb->pool);
SVN_ERR(svn_fs_base__retry_txn(wb->fs,
txn_body_write_rep,
&args,
subpool));
svn_pool_destroy(subpool);
}
return SVN_NO_ERROR;
}
static svn_error_t *
txn_body_write_close_rep(void *baton, trail_t *trail) {
struct rep_write_baton *wb = baton;
representation_t *rep;
SVN_ERR(svn_fs_bdb__read_rep(&rep, wb->fs, wb->rep_key,
trail, trail->pool));
memcpy(rep->checksum, wb->md5_digest, APR_MD5_DIGESTSIZE);
SVN_ERR(svn_fs_bdb__write_rep(wb->fs, wb->rep_key, rep,
trail, trail->pool));
return SVN_NO_ERROR;
}
static svn_error_t *
rep_write_close_contents(void *baton) {
struct rep_write_baton *wb = baton;
if (! wb->finalized) {
apr_md5_final(wb->md5_digest, &wb->md5_context);
wb->finalized = TRUE;
}
if (wb->trail) {
SVN_ERR(txn_body_write_close_rep(wb, wb->trail));
} else {
SVN_ERR(svn_fs_base__retry_txn(wb->fs,
txn_body_write_close_rep,
wb,
wb->pool));
}
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_base__rep_contents_read_stream(svn_stream_t **rs_p,
svn_fs_t *fs,
const char *rep_key,
svn_boolean_t use_trail_for_reads,
trail_t *trail,
apr_pool_t *pool) {
struct rep_read_baton *rb;
SVN_ERR(rep_read_get_baton(&rb, fs, rep_key, use_trail_for_reads,
trail, pool));
*rs_p = svn_stream_create(rb, pool);
svn_stream_set_read(*rs_p, rep_read_contents);
return SVN_NO_ERROR;
}
static svn_error_t *
rep_contents_clear(svn_fs_t *fs,
const char *rep_key,
const char *txn_id,
trail_t *trail,
apr_pool_t *pool) {
representation_t *rep;
const char *str_key;
SVN_ERR(svn_fs_bdb__read_rep(&rep, fs, rep_key, trail, pool));
if (! rep_is_mutable(rep, txn_id))
return svn_error_createf
(SVN_ERR_FS_REP_NOT_MUTABLE, NULL,
_("Rep '%s' is not mutable"), rep_key);
assert(rep->kind == rep_kind_fulltext);
str_key = rep->contents.fulltext.string_key;
if (str_key && *str_key) {
SVN_ERR(svn_fs_bdb__string_clear(fs, str_key, trail, pool));
memcpy(rep->checksum, svn_md5_empty_string_digest(),
APR_MD5_DIGESTSIZE);
SVN_ERR(svn_fs_bdb__write_rep(fs, rep_key, rep, trail, pool));
}
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_base__rep_contents_write_stream(svn_stream_t **ws_p,
svn_fs_t *fs,
const char *rep_key,
const char *txn_id,
svn_boolean_t use_trail_for_writes,
trail_t *trail,
apr_pool_t *pool) {
struct rep_write_baton *wb;
SVN_ERR(rep_contents_clear(fs, rep_key, txn_id, trail, pool));
wb = rep_write_get_baton(fs, rep_key, txn_id,
use_trail_for_writes ? trail : NULL, pool);
*ws_p = svn_stream_create(wb, pool);
svn_stream_set_write(*ws_p, rep_write_contents);
svn_stream_set_close(*ws_p, rep_write_close_contents);
return SVN_NO_ERROR;
}
struct write_svndiff_strings_baton {
svn_fs_t *fs;
const char *key;
apr_size_t size;
apr_size_t header_read;
apr_byte_t version;
trail_t *trail;
};
static svn_error_t *
write_svndiff_strings(void *baton, const char *data, apr_size_t *len) {
struct write_svndiff_strings_baton *wb = baton;
const char *buf = data;
apr_size_t nheader = 0;
if (wb->header_read < 4) {
nheader = 4 - wb->header_read;
*len -= nheader;
buf += nheader;
wb->header_read += nheader;
if (wb->header_read == 4)
wb->version = *(buf - 1);
}
SVN_ERR(svn_fs_bdb__string_append(wb->fs, &(wb->key), *len,
buf, wb->trail, wb->trail->pool));
if (wb->key == NULL)
return svn_error_create(SVN_ERR_FS_GENERAL, NULL,
_("Failed to get new string key"));
*len += nheader;
wb->size += *len;
return SVN_NO_ERROR;
}
typedef struct window_write_t {
const char *key;
apr_size_t svndiff_len;
svn_filesize_t text_off;
apr_size_t text_len;
} window_write_t;
svn_error_t *
svn_fs_base__rep_deltify(svn_fs_t *fs,
const char *target,
const char *source,
trail_t *trail,
apr_pool_t *pool) {
base_fs_data_t *bfd = fs->fsap_data;
svn_stream_t *source_stream;
svn_stream_t *target_stream;
svn_txdelta_stream_t *txdelta_stream;
window_write_t *ww;
apr_array_header_t *windows;
svn_stream_t *new_target_stream;
struct write_svndiff_strings_baton new_target_baton;
svn_txdelta_window_handler_t new_target_handler;
void *new_target_handler_baton;
svn_txdelta_window_t *window;
svn_filesize_t tview_off = 0;
svn_filesize_t diffsize = 0;
apr_array_header_t *orig_str_keys;
unsigned char rep_digest[APR_MD5_DIGESTSIZE];
const unsigned char *digest;
apr_pool_t *wpool;
if (strcmp(target, source) == 0)
return svn_error_createf
(SVN_ERR_FS_CORRUPT, NULL,
_("Attempt to deltify '%s' against itself"),
target);
new_target_baton.fs = fs;
new_target_baton.trail = trail;
new_target_baton.header_read = FALSE;
new_target_stream = svn_stream_create(&new_target_baton, pool);
svn_stream_set_write(new_target_stream, write_svndiff_strings);
SVN_ERR(svn_fs_base__rep_contents_read_stream(&source_stream, fs, source,
TRUE, trail, pool));
SVN_ERR(svn_fs_base__rep_contents_read_stream(&target_stream, fs, target,
TRUE, trail, pool));
svn_txdelta(&txdelta_stream, source_stream, target_stream, pool);
if (bfd->format >= SVN_FS_BASE__MIN_SVNDIFF1_FORMAT)
svn_txdelta_to_svndiff2(&new_target_handler, &new_target_handler_baton,
new_target_stream, 1, pool);
else
svn_txdelta_to_svndiff2(&new_target_handler, &new_target_handler_baton,
new_target_stream, 0, pool);
wpool = svn_pool_create(pool);
windows = apr_array_make(pool, 1, sizeof(ww));
do {
new_target_baton.size = 0;
new_target_baton.key = NULL;
svn_pool_clear(wpool);
SVN_ERR(svn_txdelta_next_window(&window, txdelta_stream, wpool));
SVN_ERR(new_target_handler(window, new_target_handler_baton));
if (window) {
ww = apr_pcalloc(pool, sizeof(*ww));
ww->key = new_target_baton.key;
ww->svndiff_len = new_target_baton.size;
ww->text_off = tview_off;
ww->text_len = window->tview_len;
APR_ARRAY_PUSH(windows, window_write_t *) = ww;
tview_off += window->tview_len;
diffsize += ww->svndiff_len;
}
} while (window);
svn_pool_destroy(wpool);
digest = svn_txdelta_md5_digest(txdelta_stream);
if (! digest)
return svn_error_createf
(SVN_ERR_DELTA_MD5_CHECKSUM_ABSENT, NULL,
_("Failed to calculate MD5 digest for '%s'"),
source);
{
representation_t *old_rep;
const char *str_key;
SVN_ERR(svn_fs_bdb__read_rep(&old_rep, fs, target, trail, pool));
if (old_rep->kind == rep_kind_fulltext) {
svn_filesize_t old_size = 0;
str_key = old_rep->contents.fulltext.string_key;
SVN_ERR(svn_fs_bdb__string_size(&old_size, fs, str_key,
trail, pool));
orig_str_keys = apr_array_make(pool, 1, sizeof(str_key));
APR_ARRAY_PUSH(orig_str_keys, const char *) = str_key;
if (diffsize >= old_size) {
int i;
for (i = 0; i < windows->nelts; i++) {
ww = APR_ARRAY_IDX(windows, i, window_write_t *);
SVN_ERR(svn_fs_bdb__string_delete(fs, ww->key, trail, pool));
}
return SVN_NO_ERROR;
}
} else if (old_rep->kind == rep_kind_delta)
SVN_ERR(delta_string_keys(&orig_str_keys, old_rep, pool));
else
return UNKNOWN_NODE_KIND(target);
memcpy(rep_digest, old_rep->checksum, APR_MD5_DIGESTSIZE);
}
{
representation_t new_rep;
rep_delta_chunk_t *chunk;
apr_array_header_t *chunks;
int i;
new_rep.kind = rep_kind_delta;
new_rep.txn_id = NULL;
memcpy(new_rep.checksum, rep_digest, APR_MD5_DIGESTSIZE);
chunks = apr_array_make(pool, windows->nelts, sizeof(chunk));
for (i = 0; i < windows->nelts; i++) {
ww = APR_ARRAY_IDX(windows, i, window_write_t *);
chunk = apr_palloc(pool, sizeof(*chunk));
chunk->offset = ww->text_off;
chunk->version = new_target_baton.version;
chunk->string_key = ww->key;
chunk->size = ww->text_len;
chunk->rep_key = source;
APR_ARRAY_PUSH(chunks, rep_delta_chunk_t *) = chunk;
}
new_rep.contents.delta.chunks = chunks;
SVN_ERR(svn_fs_bdb__write_rep(fs, target, &new_rep, trail, pool));
SVN_ERR(delete_strings(orig_str_keys, fs, trail, pool));
}
return SVN_NO_ERROR;
}