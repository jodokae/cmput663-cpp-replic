#include "svn_private_config.h"
#include "svn_pools.h"
#include "svn_error.h"
#include "svn_fs.h"
#include "svn_repos.h"
#include "svn_string.h"
#include "svn_path.h"
#include "svn_props.h"
#include "repos.h"
#include "svn_private_config.h"
#include "svn_mergeinfo.h"
#include "svn_md5.h"
#include <apr_lib.h>
#include "private/svn_mergeinfo_private.h"
struct parse_baton {
svn_repos_t *repos;
svn_fs_t *fs;
svn_boolean_t use_history;
svn_boolean_t use_pre_commit_hook;
svn_boolean_t use_post_commit_hook;
svn_stream_t *outstream;
enum svn_repos_load_uuid uuid_action;
const char *parent_dir;
apr_pool_t *pool;
apr_hash_t *rev_map;
};
struct revision_baton {
svn_revnum_t rev;
svn_fs_txn_t *txn;
svn_fs_root_t *txn_root;
const svn_string_t *datestamp;
apr_int32_t rev_offset;
struct parse_baton *pb;
apr_pool_t *pool;
};
struct node_baton {
const char *path;
svn_node_kind_t kind;
enum svn_node_action action;
const char *base_checksum;
const char *result_checksum;
const char *copy_source_checksum;
svn_revnum_t copyfrom_rev;
const char *copyfrom_path;
struct revision_baton *rb;
apr_pool_t *pool;
};
static svn_repos_parse_fns2_t *
fns2_from_fns(const svn_repos_parser_fns_t *fns,
apr_pool_t *pool) {
svn_repos_parse_fns2_t *fns2;
fns2 = apr_palloc(pool, sizeof(*fns2));
fns2->new_revision_record = fns->new_revision_record;
fns2->uuid_record = fns->uuid_record;
fns2->new_node_record = fns->new_node_record;
fns2->set_revision_property = fns->set_revision_property;
fns2->set_node_property = fns->set_node_property;
fns2->remove_node_props = fns->remove_node_props;
fns2->set_fulltext = fns->set_fulltext;
fns2->close_node = fns->close_node;
fns2->close_revision = fns->close_revision;
fns2->delete_node_property = NULL;
fns2->apply_textdelta = NULL;
return fns2;
}
static svn_error_t *
stream_ran_dry(void) {
return svn_error_create(SVN_ERR_INCOMPLETE_DATA, NULL,
_("Premature end of content data in dumpstream"));
}
static svn_error_t *
stream_malformed(void) {
return svn_error_create(SVN_ERR_STREAM_MALFORMED_DATA, NULL,
_("Dumpstream data appears to be malformed"));
}
static svn_error_t *
read_header_block(svn_stream_t *stream,
svn_stringbuf_t *first_header,
apr_hash_t **headers,
apr_pool_t *pool) {
*headers = apr_hash_make(pool);
while (1) {
svn_stringbuf_t *header_str;
const char *name, *value;
svn_boolean_t eof;
apr_size_t i = 0;
if (first_header != NULL) {
header_str = first_header;
first_header = NULL;
eof = FALSE;
}
else
SVN_ERR(svn_stream_readline(stream, &header_str, "\n", &eof, pool));
if (svn_stringbuf_isempty(header_str))
break;
else if (eof)
return stream_ran_dry();
while (header_str->data[i] != ':') {
if (header_str->data[i] == '\0')
return svn_error_createf(SVN_ERR_STREAM_MALFORMED_DATA, NULL,
_("Dump stream contains a malformed "
"header (with no ':') at '%.20s'"),
header_str->data);
i++;
}
header_str->data[i] = '\0';
name = header_str->data;
i += 2;
if (i > header_str->len)
return svn_error_createf(SVN_ERR_STREAM_MALFORMED_DATA, NULL,
_("Dump stream contains a malformed "
"header (with no value) at '%.20s'"),
header_str->data);
value = header_str->data + i;
apr_hash_set(*headers, name, APR_HASH_KEY_STRING, value);
}
return SVN_NO_ERROR;
}
static svn_error_t *
read_key_or_val(char **pbuf,
svn_filesize_t *actual_length,
svn_stream_t *stream,
apr_size_t len,
apr_pool_t *pool) {
char *buf = apr_pcalloc(pool, len + 1);
apr_size_t numread;
char c;
numread = len;
SVN_ERR(svn_stream_read(stream, buf, &numread));
*actual_length += numread;
if (numread != len)
return stream_ran_dry();
buf[len] = '\0';
numread = 1;
SVN_ERR(svn_stream_read(stream, &c, &numread));
*actual_length += numread;
if (numread != 1)
return stream_ran_dry();
if (c != '\n')
return stream_malformed();
*pbuf = buf;
return SVN_NO_ERROR;
}
static svn_error_t *
prefix_mergeinfo_paths(svn_string_t **mergeinfo_val,
const svn_string_t *mergeinfo_orig,
const char *parent_dir,
apr_pool_t *pool) {
apr_hash_t *prefixed_mergeinfo, *mergeinfo;
apr_hash_index_t *hi;
void *rangelist;
SVN_ERR(svn_mergeinfo_parse(&mergeinfo, mergeinfo_orig->data, pool));
prefixed_mergeinfo = apr_hash_make(pool);
for (hi = apr_hash_first(NULL, mergeinfo); hi; hi = apr_hash_next(hi)) {
const char *path;
const void *merge_source;
apr_hash_this(hi, &merge_source, NULL, &rangelist);
path = svn_path_join(parent_dir, (const char*)merge_source+1, pool);
apr_hash_set(prefixed_mergeinfo, path, APR_HASH_KEY_STRING, rangelist);
}
SVN_ERR(svn_mergeinfo_to_string(mergeinfo_val, prefixed_mergeinfo, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
renumber_mergeinfo_revs(svn_string_t **final_val,
const svn_string_t *initial_val,
struct revision_baton *rb,
apr_pool_t *pool) {
apr_pool_t *subpool = svn_pool_create(pool);
apr_hash_t *mergeinfo;
apr_hash_t *final_mergeinfo = apr_hash_make(subpool);
apr_hash_index_t *hi;
SVN_ERR(svn_mergeinfo_parse(&mergeinfo, initial_val->data, subpool));
for (hi = apr_hash_first(NULL, mergeinfo); hi; hi = apr_hash_next(hi)) {
const char *merge_source;
apr_array_header_t *rangelist;
struct parse_baton *pb = rb->pb;
int i;
const void *key;
void *val;
apr_hash_this(hi, &key, NULL, &val);
merge_source = (const char *) key;
rangelist = (apr_array_header_t *) val;
for (i = 0; i < rangelist->nelts; i++) {
svn_revnum_t *rev_from_map;
svn_merge_range_t *range = APR_ARRAY_IDX(rangelist, i,
svn_merge_range_t *);
rev_from_map = apr_hash_get(pb->rev_map, &range->start,
sizeof(svn_revnum_t));
if (rev_from_map && SVN_IS_VALID_REVNUM(*rev_from_map))
range->start = *rev_from_map;
rev_from_map = apr_hash_get(pb->rev_map, &range->end,
sizeof(svn_revnum_t));
if (rev_from_map && SVN_IS_VALID_REVNUM(*rev_from_map))
range->end = *rev_from_map;
}
apr_hash_set(final_mergeinfo, merge_source,
APR_HASH_KEY_STRING, rangelist);
}
SVN_ERR(svn_mergeinfo_sort(final_mergeinfo, subpool));
SVN_ERR(svn_mergeinfo_to_string(final_val, final_mergeinfo, pool));
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
static svn_error_t *
parse_property_block(svn_stream_t *stream,
svn_filesize_t content_length,
const svn_repos_parse_fns2_t *parse_fns,
void *record_baton,
svn_boolean_t is_node,
svn_filesize_t *actual_length,
apr_pool_t *pool) {
svn_stringbuf_t *strbuf;
apr_pool_t *proppool = svn_pool_create(pool);
*actual_length = 0;
while (content_length != *actual_length) {
char *buf;
svn_boolean_t eof;
svn_pool_clear(proppool);
SVN_ERR(svn_stream_readline(stream, &strbuf, "\n", &eof, proppool));
if (eof) {
return svn_error_create
(SVN_ERR_STREAM_MALFORMED_DATA, NULL,
_("Incomplete or unterminated property block"));
}
*actual_length += (strbuf->len + 1);
buf = strbuf->data;
if (! strcmp(buf, "PROPS-END"))
break;
else if ((buf[0] == 'K') && (buf[1] == ' ')) {
char *keybuf;
SVN_ERR(read_key_or_val(&keybuf, actual_length,
stream, atoi(buf + 2), proppool));
SVN_ERR(svn_stream_readline(stream, &strbuf, "\n", &eof, proppool));
if (eof)
return stream_ran_dry();
*actual_length += (strbuf->len + 1);
buf = strbuf->data;
if ((buf[0] == 'V') && (buf[1] == ' ')) {
svn_string_t propstring;
char *valbuf;
propstring.len = atoi(buf + 2);
SVN_ERR(read_key_or_val(&valbuf, actual_length,
stream, propstring.len, proppool));
propstring.data = valbuf;
if (is_node)
SVN_ERR(parse_fns->set_node_property(record_baton,
keybuf,
&propstring));
else
SVN_ERR(parse_fns->set_revision_property(record_baton,
keybuf,
&propstring));
} else
return stream_malformed();
} else if ((buf[0] == 'D') && (buf[1] == ' ')) {
char *keybuf;
SVN_ERR(read_key_or_val(&keybuf, actual_length,
stream, atoi(buf + 2), proppool));
if (!is_node || !parse_fns->delete_node_property)
return stream_malformed();
SVN_ERR(parse_fns->delete_node_property(record_baton, keybuf));
} else
return stream_malformed();
}
svn_pool_destroy(proppool);
return SVN_NO_ERROR;
}
static svn_error_t *
parse_text_block(svn_stream_t *stream,
svn_filesize_t content_length,
svn_boolean_t is_delta,
const svn_repos_parse_fns2_t *parse_fns,
void *record_baton,
char *buffer,
apr_size_t buflen,
apr_pool_t *pool) {
svn_stream_t *text_stream = NULL;
apr_size_t num_to_read, rlen, wlen;
if (is_delta) {
svn_txdelta_window_handler_t wh;
void *whb;
SVN_ERR(parse_fns->apply_textdelta(&wh, &whb, record_baton));
if (wh)
text_stream = svn_txdelta_parse_svndiff(wh, whb, TRUE, pool);
} else {
SVN_ERR(parse_fns->set_fulltext(&text_stream, record_baton));
}
if (content_length == 0) {
wlen = 0;
if (text_stream)
SVN_ERR(svn_stream_write(text_stream, "", &wlen));
}
while (content_length) {
if (content_length >= buflen)
rlen = buflen;
else
rlen = (apr_size_t) content_length;
num_to_read = rlen;
SVN_ERR(svn_stream_read(stream, buffer, &rlen));
content_length -= rlen;
if (rlen != num_to_read)
return stream_ran_dry();
if (text_stream) {
wlen = rlen;
SVN_ERR(svn_stream_write(text_stream, buffer, &wlen));
if (wlen != rlen) {
return svn_error_create(SVN_ERR_STREAM_UNEXPECTED_EOF, NULL,
_("Unexpected EOF writing contents"));
}
}
}
if (text_stream)
SVN_ERR(svn_stream_close(text_stream));
return SVN_NO_ERROR;
}
static svn_error_t *
parse_format_version(const char *versionstring, int *version) {
static const int magic_len = sizeof(SVN_REPOS_DUMPFILE_MAGIC_HEADER) - 1;
const char *p = strchr(versionstring, ':');
int value;
if (p == NULL
|| p != (versionstring + magic_len)
|| strncmp(versionstring,
SVN_REPOS_DUMPFILE_MAGIC_HEADER,
magic_len))
return svn_error_create(SVN_ERR_STREAM_MALFORMED_DATA, NULL,
_("Malformed dumpfile header"));
value = atoi(p+1);
if (value > SVN_REPOS_DUMPFILE_FORMAT_VERSION)
return svn_error_createf(SVN_ERR_STREAM_MALFORMED_DATA, NULL,
_("Unsupported dumpfile version: %d"),
value);
*version = value;
return SVN_NO_ERROR;
}
svn_error_t *
svn_repos_parse_dumpstream2(svn_stream_t *stream,
const svn_repos_parse_fns2_t *parse_fns,
void *parse_baton,
svn_cancel_func_t cancel_func,
void *cancel_baton,
apr_pool_t *pool) {
svn_boolean_t eof;
svn_stringbuf_t *linebuf;
void *rev_baton = NULL;
char *buffer = apr_palloc(pool, SVN__STREAM_CHUNK_SIZE);
apr_size_t buflen = SVN__STREAM_CHUNK_SIZE;
apr_pool_t *linepool = svn_pool_create(pool);
apr_pool_t *revpool = svn_pool_create(pool);
apr_pool_t *nodepool = svn_pool_create(pool);
int version;
SVN_ERR(svn_stream_readline(stream, &linebuf, "\n", &eof, linepool));
if (eof)
return stream_ran_dry();
SVN_ERR(parse_format_version(linebuf->data, &version));
if (version == SVN_REPOS_DUMPFILE_FORMAT_VERSION
&& (!parse_fns->delete_node_property || !parse_fns->apply_textdelta))
return svn_error_createf(SVN_ERR_STREAM_MALFORMED_DATA, NULL,
_("Unsupported dumpfile version: %d"), version);
while (1) {
apr_hash_t *headers;
void *node_baton;
svn_boolean_t found_node = FALSE;
svn_boolean_t old_v1_with_cl = FALSE;
const char *content_length;
const char *prop_cl;
const char *text_cl;
const char *value;
svn_filesize_t actual_prop_length;
svn_pool_clear(linepool);
if (cancel_func)
SVN_ERR(cancel_func(cancel_baton));
SVN_ERR(svn_stream_readline(stream, &linebuf, "\n", &eof, linepool));
if (eof) {
if (svn_stringbuf_isempty(linebuf))
break;
else
return stream_ran_dry();
}
if ((linebuf->len == 0) || (apr_isspace(linebuf->data[0])))
continue;
SVN_ERR(read_header_block(stream, linebuf, &headers, linepool));
if (apr_hash_get(headers, SVN_REPOS_DUMPFILE_REVISION_NUMBER,
APR_HASH_KEY_STRING)) {
if (rev_baton != NULL) {
SVN_ERR(parse_fns->close_revision(rev_baton));
svn_pool_clear(revpool);
}
SVN_ERR(parse_fns->new_revision_record(&rev_baton,
headers, parse_baton,
revpool));
}
else if (apr_hash_get(headers, SVN_REPOS_DUMPFILE_NODE_PATH,
APR_HASH_KEY_STRING)) {
SVN_ERR(parse_fns->new_node_record(&node_baton,
headers,
rev_baton,
nodepool));
found_node = TRUE;
}
else if ((value = apr_hash_get(headers, SVN_REPOS_DUMPFILE_UUID,
APR_HASH_KEY_STRING))) {
SVN_ERR(parse_fns->uuid_record(value, parse_baton, pool));
}
else if ((value = apr_hash_get(headers,
SVN_REPOS_DUMPFILE_MAGIC_HEADER,
APR_HASH_KEY_STRING))) {
version = atoi(value);
}
else {
return svn_error_create(SVN_ERR_STREAM_MALFORMED_DATA, NULL,
_("Unrecognized record type in stream"));
}
content_length = apr_hash_get(headers,
SVN_REPOS_DUMPFILE_CONTENT_LENGTH,
APR_HASH_KEY_STRING);
prop_cl = apr_hash_get(headers,
SVN_REPOS_DUMPFILE_PROP_CONTENT_LENGTH,
APR_HASH_KEY_STRING);
text_cl = apr_hash_get(headers,
SVN_REPOS_DUMPFILE_TEXT_CONTENT_LENGTH,
APR_HASH_KEY_STRING);
old_v1_with_cl =
version == 1 && content_length && ! prop_cl && ! text_cl;
if (prop_cl || old_v1_with_cl) {
const char *delta = apr_hash_get(headers,
SVN_REPOS_DUMPFILE_PROP_DELTA,
APR_HASH_KEY_STRING);
svn_boolean_t is_delta = (delta && strcmp(delta, "true") == 0);
if (found_node && !is_delta)
SVN_ERR(parse_fns->remove_node_props(node_baton));
SVN_ERR(parse_property_block
(stream,
svn__atoui64(prop_cl ? prop_cl : content_length),
parse_fns,
found_node ? node_baton : rev_baton,
found_node,
&actual_prop_length,
found_node ? nodepool : revpool));
}
if (text_cl) {
const char *delta = apr_hash_get(headers,
SVN_REPOS_DUMPFILE_TEXT_DELTA,
APR_HASH_KEY_STRING);
svn_boolean_t is_delta = (delta && strcmp(delta, "true") == 0);
SVN_ERR(parse_text_block(stream,
svn__atoui64(text_cl),
is_delta,
parse_fns,
found_node ? node_baton : rev_baton,
buffer,
buflen,
found_node ? nodepool : revpool));
} else if (old_v1_with_cl) {
const char *node_kind;
svn_filesize_t cl_value = svn__atoui64(content_length)
- actual_prop_length;
if (cl_value ||
((node_kind = apr_hash_get(headers,
SVN_REPOS_DUMPFILE_NODE_KIND,
APR_HASH_KEY_STRING))
&& strcmp(node_kind, "file") == 0)
)
SVN_ERR(parse_text_block(stream,
cl_value,
FALSE,
parse_fns,
found_node ? node_baton : rev_baton,
buffer,
buflen,
found_node ? nodepool : revpool));
}
if (content_length && ! old_v1_with_cl) {
apr_size_t rlen, num_to_read;
svn_filesize_t remaining =
svn__atoui64(content_length) -
(prop_cl ? svn__atoui64(prop_cl) : 0) -
(text_cl ? svn__atoui64(text_cl) : 0);
if (remaining < 0)
return svn_error_create(SVN_ERR_STREAM_MALFORMED_DATA, NULL,
_("Sum of subblock sizes larger than "
"total block content length"));
while (remaining > 0) {
if (remaining >= buflen)
rlen = buflen;
else
rlen = (apr_size_t) remaining;
num_to_read = rlen;
SVN_ERR(svn_stream_read(stream, buffer, &rlen));
remaining -= rlen;
if (rlen != num_to_read)
return stream_ran_dry();
}
}
if (found_node) {
SVN_ERR(parse_fns->close_node(node_baton));
svn_pool_clear(nodepool);
}
}
if (rev_baton != NULL)
SVN_ERR(parse_fns->close_revision(rev_baton));
svn_pool_destroy(linepool);
svn_pool_destroy(revpool);
svn_pool_destroy(nodepool);
return SVN_NO_ERROR;
}
svn_error_t *
svn_repos_parse_dumpstream(svn_stream_t *stream,
const svn_repos_parser_fns_t *parse_fns,
void *parse_baton,
svn_cancel_func_t cancel_func,
void *cancel_baton,
apr_pool_t *pool) {
svn_repos_parse_fns2_t *fns2 = fns2_from_fns(parse_fns, pool);
return svn_repos_parse_dumpstream2(stream, fns2, parse_baton,
cancel_func, cancel_baton, pool);
}
static struct node_baton *
make_node_baton(apr_hash_t *headers,
struct revision_baton *rb,
apr_pool_t *pool) {
struct node_baton *nb = apr_pcalloc(pool, sizeof(*nb));
const char *val;
nb->rb = rb;
nb->pool = pool;
nb->kind = svn_node_unknown;
if ((val = apr_hash_get(headers, SVN_REPOS_DUMPFILE_NODE_PATH,
APR_HASH_KEY_STRING))) {
if (rb->pb->parent_dir)
nb->path = svn_path_join(rb->pb->parent_dir, val, pool);
else
nb->path = apr_pstrdup(pool, val);
}
if ((val = apr_hash_get(headers, SVN_REPOS_DUMPFILE_NODE_KIND,
APR_HASH_KEY_STRING))) {
if (! strcmp(val, "file"))
nb->kind = svn_node_file;
else if (! strcmp(val, "dir"))
nb->kind = svn_node_dir;
}
nb->action = (enum svn_node_action)(-1);
if ((val = apr_hash_get(headers, SVN_REPOS_DUMPFILE_NODE_ACTION,
APR_HASH_KEY_STRING))) {
if (! strcmp(val, "change"))
nb->action = svn_node_action_change;
else if (! strcmp(val, "add"))
nb->action = svn_node_action_add;
else if (! strcmp(val, "delete"))
nb->action = svn_node_action_delete;
else if (! strcmp(val, "replace"))
nb->action = svn_node_action_replace;
}
nb->copyfrom_rev = SVN_INVALID_REVNUM;
if ((val = apr_hash_get(headers, SVN_REPOS_DUMPFILE_NODE_COPYFROM_REV,
APR_HASH_KEY_STRING))) {
nb->copyfrom_rev = (svn_revnum_t) atoi(val);
}
if ((val = apr_hash_get(headers, SVN_REPOS_DUMPFILE_NODE_COPYFROM_PATH,
APR_HASH_KEY_STRING))) {
if (rb->pb->parent_dir)
nb->copyfrom_path = svn_path_join(rb->pb->parent_dir,
(*val == '/' ? val + 1 : val), pool);
else
nb->copyfrom_path = apr_pstrdup(pool, val);
}
if ((val = apr_hash_get(headers, SVN_REPOS_DUMPFILE_TEXT_CONTENT_CHECKSUM,
APR_HASH_KEY_STRING))) {
nb->result_checksum = apr_pstrdup(pool, val);
}
if ((val = apr_hash_get(headers, SVN_REPOS_DUMPFILE_TEXT_DELTA_BASE_CHECKSUM,
APR_HASH_KEY_STRING))) {
nb->base_checksum = apr_pstrdup(pool, val);
}
if ((val = apr_hash_get(headers, SVN_REPOS_DUMPFILE_TEXT_COPY_SOURCE_CHECKSUM,
APR_HASH_KEY_STRING))) {
nb->copy_source_checksum = apr_pstrdup(pool, val);
}
return nb;
}
static struct revision_baton *
make_revision_baton(apr_hash_t *headers,
struct parse_baton *pb,
apr_pool_t *pool) {
struct revision_baton *rb = apr_pcalloc(pool, sizeof(*rb));
const char *val;
rb->pb = pb;
rb->pool = pool;
rb->rev = SVN_INVALID_REVNUM;
if ((val = apr_hash_get(headers, SVN_REPOS_DUMPFILE_REVISION_NUMBER,
APR_HASH_KEY_STRING)))
rb->rev = SVN_STR_TO_REV(val);
return rb;
}
static svn_error_t *
new_revision_record(void **revision_baton,
apr_hash_t *headers,
void *parse_baton,
apr_pool_t *pool) {
struct parse_baton *pb = parse_baton;
struct revision_baton *rb;
svn_revnum_t head_rev;
rb = make_revision_baton(headers, pb, pool);
SVN_ERR(svn_fs_youngest_rev(&head_rev, pb->fs, pool));
rb->rev_offset = (rb->rev) - (head_rev + 1);
if (rb->rev > 0) {
SVN_ERR(svn_fs_begin_txn2(&(rb->txn), pb->fs, head_rev, 0, pool));
SVN_ERR(svn_fs_txn_root(&(rb->txn_root), rb->txn, pool));
SVN_ERR(svn_stream_printf(pb->outstream, pool,
_("<<< Started new transaction, based on "
"original revision %ld\n"), rb->rev));
}
*revision_baton = rb;
return SVN_NO_ERROR;
}
static svn_error_t *
maybe_add_with_history(struct node_baton *nb,
struct revision_baton *rb,
apr_pool_t *pool) {
struct parse_baton *pb = rb->pb;
apr_size_t len;
if ((nb->copyfrom_path == NULL) || (! pb->use_history)) {
if (nb->kind == svn_node_file)
SVN_ERR(svn_fs_make_file(rb->txn_root, nb->path, pool));
else if (nb->kind == svn_node_dir)
SVN_ERR(svn_fs_make_dir(rb->txn_root, nb->path, pool));
} else {
svn_fs_root_t *copy_root;
svn_revnum_t src_rev = nb->copyfrom_rev - rb->rev_offset;
svn_revnum_t *src_rev_from_map;
if ((src_rev_from_map = apr_hash_get(pb->rev_map, &nb->copyfrom_rev,
sizeof(nb->copyfrom_rev))))
src_rev = *src_rev_from_map;
if (! SVN_IS_VALID_REVNUM(src_rev))
return svn_error_createf(SVN_ERR_FS_NO_SUCH_REVISION, NULL,
_("Relative source revision %ld is not"
" available in current repository"),
src_rev);
SVN_ERR(svn_fs_revision_root(&copy_root, pb->fs, src_rev, pool));
if (nb->copy_source_checksum) {
unsigned char md5_digest[APR_MD5_DIGESTSIZE];
const char *hex;
SVN_ERR(svn_fs_file_md5_checksum(md5_digest, copy_root,
nb->copyfrom_path, pool));
hex = svn_md5_digest_to_cstring(md5_digest, pool);
if (hex && (strcmp(nb->copy_source_checksum, hex) != 0))
return svn_error_createf
(SVN_ERR_CHECKSUM_MISMATCH,
NULL,
_("Copy source checksum mismatch on copy from '%s'@%ld\n"
" to '%s' in rev based on r%ld:\n"
" expected: %s\n"
" actual: %s\n"),
nb->copyfrom_path, src_rev,
nb->path, rb->rev,
nb->copy_source_checksum, hex);
}
SVN_ERR(svn_fs_copy(copy_root, nb->copyfrom_path,
rb->txn_root, nb->path, pool));
len = 9;
SVN_ERR(svn_stream_write(pb->outstream, "COPIED...", &len));
}
return SVN_NO_ERROR;
}
static svn_error_t *
uuid_record(const char *uuid,
void *parse_baton,
apr_pool_t *pool) {
struct parse_baton *pb = parse_baton;
svn_revnum_t youngest_rev;
if (pb->uuid_action == svn_repos_load_uuid_ignore)
return SVN_NO_ERROR;
if (pb->uuid_action != svn_repos_load_uuid_force) {
SVN_ERR(svn_fs_youngest_rev(&youngest_rev, pb->fs, pool));
if (youngest_rev != 0)
return SVN_NO_ERROR;
}
return svn_fs_set_uuid(pb->fs, uuid, pool);
}
static svn_error_t *
new_node_record(void **node_baton,
apr_hash_t *headers,
void *revision_baton,
apr_pool_t *pool) {
struct revision_baton *rb = revision_baton;
struct parse_baton *pb = rb->pb;
struct node_baton *nb;
if (rb->rev == 0)
return svn_error_create(SVN_ERR_STREAM_MALFORMED_DATA, NULL,
_("Malformed dumpstream: "
"Revision 0 must not contain node records"));
nb = make_node_baton(headers, rb, pool);
switch (nb->action) {
case svn_node_action_change: {
SVN_ERR(svn_stream_printf(pb->outstream, pool,
_(" * editing path : %s ..."),
nb->path));
break;
}
case svn_node_action_delete: {
SVN_ERR(svn_stream_printf(pb->outstream, pool,
_(" * deleting path : %s ..."),
nb->path));
SVN_ERR(svn_fs_delete(rb->txn_root, nb->path, pool));
break;
}
case svn_node_action_add: {
SVN_ERR(svn_stream_printf(pb->outstream, pool,
_(" * adding path : %s ..."),
nb->path));
SVN_ERR(maybe_add_with_history(nb, rb, pool));
break;
}
case svn_node_action_replace: {
SVN_ERR(svn_stream_printf(pb->outstream, pool,
_(" * replacing path : %s ..."),
nb->path));
SVN_ERR(svn_fs_delete(rb->txn_root, nb->path, pool));
SVN_ERR(maybe_add_with_history(nb, rb, pool));
break;
}
default:
return svn_error_createf(SVN_ERR_STREAM_UNRECOGNIZED_DATA, NULL,
_("Unrecognized node-action on node '%s'"),
nb->path);
}
*node_baton = nb;
return SVN_NO_ERROR;
}
static svn_error_t *
set_revision_property(void *baton,
const char *name,
const svn_string_t *value) {
struct revision_baton *rb = baton;
if (rb->rev > 0) {
SVN_ERR(svn_fs_change_txn_prop(rb->txn, name, value, rb->pool));
if (! strcmp(name, SVN_PROP_REVISION_DATE))
rb->datestamp = svn_string_dup(value, rb->pool);
} else if (rb->rev == 0) {
struct parse_baton *pb = rb->pb;
svn_revnum_t youngest_rev;
SVN_ERR(svn_fs_youngest_rev(&youngest_rev, pb->fs, rb->pool));
if (youngest_rev == 0)
SVN_ERR(svn_fs_change_rev_prop(pb->fs, 0, name, value, rb->pool));
}
return SVN_NO_ERROR;
}
static svn_error_t *
set_node_property(void *baton,
const char *name,
const svn_string_t *value) {
struct node_baton *nb = baton;
struct revision_baton *rb = nb->rb;
const char *parent_dir = rb->pb->parent_dir;
if (strcmp(name, SVN_PROP_MERGEINFO) == 0) {
svn_string_t *renumbered_mergeinfo;
SVN_ERR(renumber_mergeinfo_revs(&renumbered_mergeinfo, value, rb,
nb->pool));
value = renumbered_mergeinfo;
if (parent_dir) {
svn_string_t *mergeinfo_val;
SVN_ERR(prefix_mergeinfo_paths(&mergeinfo_val, value, parent_dir,
nb->pool));
value = mergeinfo_val;
}
}
SVN_ERR(svn_fs_change_node_prop(rb->txn_root, nb->path,
name, value, nb->pool));
return SVN_NO_ERROR;
}
static svn_error_t *
delete_node_property(void *baton,
const char *name) {
struct node_baton *nb = baton;
struct revision_baton *rb = nb->rb;
SVN_ERR(svn_fs_change_node_prop(rb->txn_root, nb->path,
name, NULL, nb->pool));
return SVN_NO_ERROR;
}
static svn_error_t *
remove_node_props(void *baton) {
struct node_baton *nb = baton;
struct revision_baton *rb = nb->rb;
apr_hash_t *proplist;
apr_hash_index_t *hi;
SVN_ERR(svn_fs_node_proplist(&proplist,
rb->txn_root, nb->path, nb->pool));
for (hi = apr_hash_first(nb->pool, proplist); hi; hi = apr_hash_next(hi)) {
const void *key;
apr_hash_this(hi, &key, NULL, NULL);
SVN_ERR(svn_fs_change_node_prop(rb->txn_root, nb->path,
(const char *) key, NULL,
nb->pool));
}
return SVN_NO_ERROR;
}
static svn_error_t *
apply_textdelta(svn_txdelta_window_handler_t *handler,
void **handler_baton,
void *node_baton) {
struct node_baton *nb = node_baton;
struct revision_baton *rb = nb->rb;
return svn_fs_apply_textdelta(handler, handler_baton,
rb->txn_root, nb->path,
nb->base_checksum, nb->result_checksum,
nb->pool);
}
static svn_error_t *
set_fulltext(svn_stream_t **stream,
void *node_baton) {
struct node_baton *nb = node_baton;
struct revision_baton *rb = nb->rb;
return svn_fs_apply_text(stream,
rb->txn_root, nb->path,
nb->result_checksum,
nb->pool);
}
static svn_error_t *
close_node(void *baton) {
struct node_baton *nb = baton;
struct revision_baton *rb = nb->rb;
struct parse_baton *pb = rb->pb;
apr_size_t len = 7;
SVN_ERR(svn_stream_write(pb->outstream, _(" done.\n"), &len));
return SVN_NO_ERROR;
}
static svn_error_t *
close_revision(void *baton) {
struct revision_baton *rb = baton;
struct parse_baton *pb = rb->pb;
const char *conflict_msg = NULL;
svn_revnum_t *old_rev, *new_rev;
svn_error_t *err;
if (rb->rev <= 0)
return SVN_NO_ERROR;
old_rev = apr_palloc(pb->pool, sizeof(*old_rev) * 2);
new_rev = old_rev + 1;
*old_rev = rb->rev;
if (pb->use_pre_commit_hook) {
const char *txn_name;
err = svn_fs_txn_name(&txn_name, rb->txn, rb->pool);
if (! err)
err = svn_repos__hooks_pre_commit(pb->repos, txn_name, rb->pool);
if (err) {
svn_error_clear(svn_fs_abort_txn(rb->txn, rb->pool));
return err;
}
}
if ((err = svn_fs_commit_txn(&conflict_msg, new_rev, rb->txn, rb->pool))) {
svn_error_clear(svn_fs_abort_txn(rb->txn, rb->pool));
if (conflict_msg)
return svn_error_quick_wrap(err, conflict_msg);
else
return err;
}
if (pb->use_post_commit_hook) {
if ((err = svn_repos__hooks_post_commit(pb->repos, *new_rev, rb->pool)))
return svn_error_create
(SVN_ERR_REPOS_POST_COMMIT_HOOK_FAILED, err,
_("Commit succeeded, but post-commit hook failed"));
}
apr_hash_set(pb->rev_map, old_rev, sizeof(svn_revnum_t), new_rev);
SVN_ERR(svn_fs_deltify_revision(pb->fs, *new_rev, rb->pool));
SVN_ERR(svn_fs_change_rev_prop(pb->fs, *new_rev,
SVN_PROP_REVISION_DATE, rb->datestamp,
rb->pool));
if (*new_rev == rb->rev) {
SVN_ERR(svn_stream_printf(pb->outstream, rb->pool,
_("\n------- Committed revision %ld"
" >>>\n\n"), *new_rev));
} else {
SVN_ERR(svn_stream_printf(pb->outstream, rb->pool,
_("\n------- Committed new rev %ld"
" (loaded from original rev %ld"
") >>>\n\n"), *new_rev, rb->rev));
}
return SVN_NO_ERROR;
}
svn_error_t *
svn_repos_get_fs_build_parser2(const svn_repos_parse_fns2_t **callbacks,
void **parse_baton,
svn_repos_t *repos,
svn_boolean_t use_history,
enum svn_repos_load_uuid uuid_action,
svn_stream_t *outstream,
const char *parent_dir,
apr_pool_t *pool) {
const svn_repos_parser_fns_t *fns;
svn_repos_parse_fns2_t *parser;
SVN_ERR(svn_repos_get_fs_build_parser(&fns, parse_baton, repos,
use_history, uuid_action, outstream,
parent_dir, pool));
parser = fns2_from_fns(fns, pool);
parser->delete_node_property = delete_node_property;
parser->apply_textdelta = apply_textdelta;
*callbacks = parser;
return SVN_NO_ERROR;
}
svn_error_t *
svn_repos_get_fs_build_parser(const svn_repos_parser_fns_t **parser_callbacks,
void **parse_baton,
svn_repos_t *repos,
svn_boolean_t use_history,
enum svn_repos_load_uuid uuid_action,
svn_stream_t *outstream,
const char *parent_dir,
apr_pool_t *pool) {
svn_repos_parser_fns_t *parser = apr_pcalloc(pool, sizeof(*parser));
struct parse_baton *pb = apr_pcalloc(pool, sizeof(*pb));
parser->new_revision_record = new_revision_record;
parser->new_node_record = new_node_record;
parser->uuid_record = uuid_record;
parser->set_revision_property = set_revision_property;
parser->set_node_property = set_node_property;
parser->remove_node_props = remove_node_props;
parser->set_fulltext = set_fulltext;
parser->close_node = close_node;
parser->close_revision = close_revision;
pb->repos = repos;
pb->fs = svn_repos_fs(repos);
pb->use_history = use_history;
pb->outstream = outstream ? outstream : svn_stream_empty(pool);
pb->uuid_action = uuid_action;
pb->parent_dir = parent_dir;
pb->pool = pool;
pb->rev_map = apr_hash_make(pool);
*parser_callbacks = parser;
*parse_baton = pb;
return SVN_NO_ERROR;
}
svn_error_t *
svn_repos_load_fs2(svn_repos_t *repos,
svn_stream_t *dumpstream,
svn_stream_t *feedback_stream,
enum svn_repos_load_uuid uuid_action,
const char *parent_dir,
svn_boolean_t use_pre_commit_hook,
svn_boolean_t use_post_commit_hook,
svn_cancel_func_t cancel_func,
void *cancel_baton,
apr_pool_t *pool) {
const svn_repos_parse_fns2_t *parser;
void *parse_baton;
struct parse_baton *pb;
SVN_ERR(svn_repos_get_fs_build_parser2(&parser, &parse_baton,
repos,
TRUE,
uuid_action,
feedback_stream,
parent_dir,
pool));
pb = parse_baton;
pb->use_pre_commit_hook = use_pre_commit_hook;
pb->use_post_commit_hook = use_post_commit_hook;
SVN_ERR(svn_repos_parse_dumpstream2(dumpstream, parser, parse_baton,
cancel_func, cancel_baton, pool));
return SVN_NO_ERROR;
}
svn_error_t *
svn_repos_load_fs(svn_repos_t *repos,
svn_stream_t *dumpstream,
svn_stream_t *feedback_stream,
enum svn_repos_load_uuid uuid_action,
const char *parent_dir,
svn_cancel_func_t cancel_func,
void *cancel_baton,
apr_pool_t *pool) {
return svn_repos_load_fs2(repos, dumpstream, feedback_stream,
uuid_action, parent_dir, FALSE, FALSE,
cancel_func, cancel_baton, pool);
}