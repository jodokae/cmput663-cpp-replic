#include <stdlib.h>
#include <limits.h>
#include <assert.h>
#include <apr_version.h>
#include <apr_pools.h>
#include <apr_hash.h>
#include <apr_file_io.h>
#include "svn_types.h"
#include "svn_string.h"
#include "svn_error.h"
#include "svn_hash.h"
#include "svn_sorts.h"
#include "svn_io.h"
#include "svn_pools.h"
#include "private/svn_dep_compat.h"
static svn_error_t *
hash_read(apr_hash_t *hash, svn_stream_t *stream, const char *terminator,
svn_boolean_t incremental, apr_pool_t *pool) {
svn_stringbuf_t *buf;
svn_boolean_t eof;
apr_size_t len, keylen, vallen;
char c, *end, *keybuf, *valbuf;
while (1) {
SVN_ERR(svn_stream_readline(stream, &buf, "\n", &eof, pool));
if ((!terminator && eof && buf->len == 0)
|| (terminator && (strcmp(buf->data, terminator) == 0)))
return SVN_NO_ERROR;
if (eof)
return svn_error_create(SVN_ERR_MALFORMED_FILE, NULL, NULL);
if ((buf->len >= 3) && (buf->data[0] == 'K') && (buf->data[1] == ' ')) {
keylen = (size_t) strtoul(buf->data + 2, &end, 10);
if (keylen == (size_t) ULONG_MAX || *end != '\0')
return svn_error_create(SVN_ERR_MALFORMED_FILE, NULL, NULL);
keybuf = apr_palloc(pool, keylen + 1);
SVN_ERR(svn_stream_read(stream, keybuf, &keylen));
keybuf[keylen] = '\0';
len = 1;
SVN_ERR(svn_stream_read(stream, &c, &len));
if (c != '\n')
return svn_error_create(SVN_ERR_MALFORMED_FILE, NULL, NULL);
SVN_ERR(svn_stream_readline(stream, &buf, "\n", &eof, pool));
if ((buf->data[0] == 'V') && (buf->data[1] == ' ')) {
vallen = (size_t) strtoul(buf->data + 2, &end, 10);
if (vallen == (size_t) ULONG_MAX || *end != '\0')
return svn_error_create(SVN_ERR_MALFORMED_FILE, NULL, NULL);
valbuf = apr_palloc(pool, vallen + 1);
SVN_ERR(svn_stream_read(stream, valbuf, &vallen));
valbuf[vallen] = '\0';
len = 1;
SVN_ERR(svn_stream_read(stream, &c, &len));
if (c != '\n')
return svn_error_create(SVN_ERR_MALFORMED_FILE, NULL, NULL);
apr_hash_set(hash, keybuf, keylen,
svn_string_ncreate(valbuf, vallen, pool));
} else
return svn_error_create(SVN_ERR_MALFORMED_FILE, NULL, NULL);
} else if (incremental && (buf->len >= 3)
&& (buf->data[0] == 'D') && (buf->data[1] == ' ')) {
keylen = (size_t) strtoul(buf->data + 2, &end, 10);
if (keylen == (size_t) ULONG_MAX || *end != '\0')
return svn_error_create(SVN_ERR_MALFORMED_FILE, NULL, NULL);
keybuf = apr_palloc(pool, keylen + 1);
SVN_ERR(svn_stream_read(stream, keybuf, &keylen));
keybuf[keylen] = '\0';
len = 1;
SVN_ERR(svn_stream_read(stream, &c, &len));
if (c != '\n')
return svn_error_create(SVN_ERR_MALFORMED_FILE, NULL, NULL);
apr_hash_set(hash, keybuf, keylen, NULL);
} else
return svn_error_create(SVN_ERR_MALFORMED_FILE, NULL, NULL);
}
}
static svn_error_t *
hash_write(apr_hash_t *hash, apr_hash_t *oldhash, svn_stream_t *stream,
const char *terminator, apr_pool_t *pool) {
apr_pool_t *subpool;
apr_size_t len;
apr_array_header_t *list;
int i;
subpool = svn_pool_create(pool);
list = svn_sort__hash(hash, svn_sort_compare_items_lexically, pool);
for (i = 0; i < list->nelts; i++) {
svn_sort__item_t *item = &APR_ARRAY_IDX(list, i, svn_sort__item_t);
svn_string_t *valstr = item->value;
svn_pool_clear(subpool);
if (oldhash) {
svn_string_t *oldstr = apr_hash_get(oldhash, item->key, item->klen);
if (oldstr && svn_string_compare(valstr, oldstr))
continue;
}
SVN_ERR(svn_stream_printf(stream, subpool,
"K %" APR_SSIZE_T_FMT "\n%s\n"
"V %" APR_SIZE_T_FMT "\n",
item->klen, (const char *) item->key,
valstr->len));
len = valstr->len;
SVN_ERR(svn_stream_write(stream, valstr->data, &len));
SVN_ERR(svn_stream_printf(stream, subpool, "\n"));
}
if (oldhash) {
list = svn_sort__hash(oldhash, svn_sort_compare_items_lexically,
pool);
for (i = 0; i < list->nelts; i++) {
svn_sort__item_t *item = &APR_ARRAY_IDX(list, i, svn_sort__item_t);
svn_pool_clear(subpool);
if (! apr_hash_get(hash, item->key, item->klen))
SVN_ERR(svn_stream_printf(stream, subpool,
"D %" APR_SSIZE_T_FMT "\n%s\n",
item->klen, (const char *) item->key));
}
}
if (terminator)
SVN_ERR(svn_stream_printf(stream, subpool, "%s\n", terminator));
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
svn_error_t *svn_hash_read2(apr_hash_t *hash, svn_stream_t *stream,
const char *terminator, apr_pool_t *pool) {
return hash_read(hash, stream, terminator, FALSE, pool);
}
svn_error_t *svn_hash_read_incremental(apr_hash_t *hash,
svn_stream_t *stream,
const char *terminator,
apr_pool_t *pool) {
return hash_read(hash, stream, terminator, TRUE, pool);
}
svn_error_t *
svn_hash_write2(apr_hash_t *hash, svn_stream_t *stream,
const char *terminator, apr_pool_t *pool) {
return hash_write(hash, NULL, stream, terminator, pool);
}
svn_error_t *
svn_hash_write_incremental(apr_hash_t *hash, apr_hash_t *oldhash,
svn_stream_t *stream, const char *terminator,
apr_pool_t *pool) {
assert(oldhash != NULL);
return hash_write(hash, oldhash, stream, terminator, pool);
}
svn_error_t *
svn_hash_write(apr_hash_t *hash, apr_file_t *destfile, apr_pool_t *pool) {
return hash_write(hash, NULL, svn_stream_from_aprfile(destfile, pool),
SVN_HASH_TERMINATOR, pool);
}
svn_error_t *
svn_hash_read(apr_hash_t *hash,
apr_file_t *srcfile,
apr_pool_t *pool) {
svn_error_t *err;
char buf[SVN_KEYLINE_MAXLEN];
apr_size_t num_read;
char c;
int first_time = 1;
while (1) {
apr_size_t len = sizeof(buf);
err = svn_io_read_length_line(srcfile, buf, &len, pool);
if (err && APR_STATUS_IS_EOF(err->apr_err) && first_time) {
svn_error_clear(err);
return SVN_NO_ERROR;
} else if (err)
return err;
first_time = 0;
if (((len == 3) && (buf[0] == 'E') && (buf[1] == 'N') && (buf[2] == 'D'))
|| ((len == 9)
&& (buf[0] == 'P')
&& (buf[1] == 'R')
&& (buf[2] == 'O')
&& (buf[3] == 'P')
&& (buf[4] == 'S')
&& (buf[5] == '-')
&& (buf[6] == 'E')
&& (buf[7] == 'N')
&& (buf[8] == 'D'))) {
return SVN_NO_ERROR;
} else if ((buf[0] == 'K') && (buf[1] == ' ')) {
size_t keylen = (size_t) atoi(buf + 2);
void *keybuf = apr_palloc(pool, keylen + 1);
SVN_ERR(svn_io_file_read_full(srcfile,
keybuf, keylen, &num_read, pool));
((char *) keybuf)[keylen] = '\0';
SVN_ERR(svn_io_file_getc(&c, srcfile, pool));
if (c != '\n')
return svn_error_create(SVN_ERR_MALFORMED_FILE, NULL, NULL);
len = sizeof(buf);
SVN_ERR(svn_io_read_length_line(srcfile, buf, &len, pool));
if ((buf[0] == 'V') && (buf[1] == ' ')) {
svn_string_t *value = apr_palloc(pool, sizeof(*value));
apr_size_t vallen = atoi(buf + 2);
void *valbuf = apr_palloc(pool, vallen + 1);
SVN_ERR(svn_io_file_read_full(srcfile,
valbuf, vallen,
&num_read, pool));
((char *) valbuf)[vallen] = '\0';
SVN_ERR(svn_io_file_getc(&c, srcfile, pool));
if (c != '\n')
return svn_error_create(SVN_ERR_MALFORMED_FILE, NULL, NULL);
value->data = valbuf;
value->len = vallen;
apr_hash_set(hash, keybuf, keylen, value);
} else {
return svn_error_create(SVN_ERR_MALFORMED_FILE, NULL, NULL);
}
} else {
return svn_error_create(SVN_ERR_MALFORMED_FILE, NULL, NULL);
}
}
}
svn_error_t *
svn_hash_diff(apr_hash_t *hash_a,
apr_hash_t *hash_b,
svn_hash_diff_func_t diff_func,
void *diff_func_baton,
apr_pool_t *pool) {
apr_hash_index_t *hi;
if (hash_a)
for (hi = apr_hash_first(pool, hash_a); hi; hi = apr_hash_next(hi)) {
const void *key;
apr_ssize_t klen;
apr_hash_this(hi, &key, &klen, NULL);
if (hash_b && (apr_hash_get(hash_b, key, klen)))
SVN_ERR((*diff_func)(key, klen, svn_hash_diff_key_both,
diff_func_baton));
else
SVN_ERR((*diff_func)(key, klen, svn_hash_diff_key_a,
diff_func_baton));
}
if (hash_b)
for (hi = apr_hash_first(pool, hash_b); hi; hi = apr_hash_next(hi)) {
const void *key;
apr_ssize_t klen;
apr_hash_this(hi, &key, &klen, NULL);
if (! (hash_a && apr_hash_get(hash_a, key, klen)))
SVN_ERR((*diff_func)(key, klen, svn_hash_diff_key_b,
diff_func_baton));
}
return SVN_NO_ERROR;
}
svn_error_t *
svn_hash_keys(apr_array_header_t **array,
apr_hash_t *hash,
apr_pool_t *pool) {
apr_hash_index_t *hi;
*array = apr_array_make(pool, apr_hash_count(hash), sizeof(const char *));
for (hi = apr_hash_first(pool, hash); hi; hi = apr_hash_next(hi)) {
const void *key;
const char *path;
apr_hash_this(hi, &key, NULL, NULL);
path = key;
APR_ARRAY_PUSH(*array, const char *) = path;
}
return SVN_NO_ERROR;
}
svn_error_t *
svn_hash_from_cstring_keys(apr_hash_t **hash_p,
const apr_array_header_t *keys,
apr_pool_t *pool) {
int i;
apr_hash_t *hash = apr_hash_make(pool);
for (i = 0; i < keys->nelts; i++) {
const char *key =
apr_pstrdup(pool, APR_ARRAY_IDX(keys, i, const char *));
apr_hash_set(hash, key, APR_HASH_KEY_STRING, key);
}
*hash_p = hash;
return SVN_NO_ERROR;
}
svn_error_t *
svn_hash__clear(apr_hash_t *hash) {
#if APR_VERSION_AT_LEAST(1, 3, 0)
apr_hash_clear(hash);
#else
apr_hash_index_t *hi;
const void *key;
apr_ssize_t klen;
for (hi = apr_hash_first(NULL, hash); hi; hi = apr_hash_next(hi)) {
apr_hash_this(hi, &key, &klen, NULL);
apr_hash_set(hash, key, klen, NULL);
}
#endif
return SVN_NO_ERROR;
}
