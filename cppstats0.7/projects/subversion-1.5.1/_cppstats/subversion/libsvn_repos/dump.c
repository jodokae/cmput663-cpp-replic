#include "svn_private_config.h"
#include "svn_pools.h"
#include "svn_error.h"
#include "svn_fs.h"
#include "svn_iter.h"
#include "svn_repos.h"
#include "svn_string.h"
#include "svn_path.h"
#include "svn_time.h"
#include "svn_md5.h"
#include "svn_props.h"
#define ARE_VALID_COPY_ARGS(p,r) ((p && SVN_IS_VALID_REVNUM(r)) ? 1 : 0)
static void
write_hash_to_stringbuf(apr_hash_t *hash,
apr_hash_t *oldhash,
svn_stringbuf_t **strbuf,
apr_pool_t *pool) {
apr_hash_index_t *this;
*strbuf = svn_stringbuf_create("", pool);
for (this = apr_hash_first(pool, hash); this; this = apr_hash_next(this)) {
const void *key;
void *val;
apr_ssize_t keylen;
svn_string_t *value;
apr_hash_this(this, &key, &keylen, &val);
value = val;
if (oldhash) {
svn_string_t *oldvalue = apr_hash_get(oldhash, key, keylen);
if (oldvalue && svn_string_compare(value, oldvalue))
continue;
}
svn_stringbuf_appendcstr(*strbuf,
apr_psprintf(pool, "K %" APR_SSIZE_T_FMT "\n",
keylen));
svn_stringbuf_appendbytes(*strbuf, (const char *) key, keylen);
svn_stringbuf_appendbytes(*strbuf, "\n", 1);
svn_stringbuf_appendcstr(*strbuf,
apr_psprintf(pool, "V %" APR_SIZE_T_FMT "\n",
value->len));
svn_stringbuf_appendbytes(*strbuf, value->data, value->len);
svn_stringbuf_appendbytes(*strbuf, "\n", 1);
}
if (oldhash) {
for (this = apr_hash_first(pool, oldhash); this;
this = apr_hash_next(this)) {
const void *key;
apr_ssize_t keylen;
apr_hash_this(this, &key, &keylen, NULL);
if (apr_hash_get(hash, key, keylen))
continue;
svn_stringbuf_appendcstr(*strbuf,
apr_psprintf(pool,
"D %" APR_SSIZE_T_FMT "\n",
keylen));
svn_stringbuf_appendbytes(*strbuf, (const char *) key, keylen);
svn_stringbuf_appendbytes(*strbuf, "\n", 1);
}
}
svn_stringbuf_appendbytes(*strbuf, "PROPS-END\n", 10);
}
static svn_error_t *
store_delta(apr_file_t **tempfile, svn_filesize_t *len,
svn_fs_root_t *oldroot, const char *oldpath,
svn_fs_root_t *newroot, const char *newpath, apr_pool_t *pool) {
const char *tempdir;
svn_stream_t *temp_stream;
apr_off_t offset = 0;
svn_txdelta_stream_t *delta_stream;
svn_txdelta_window_handler_t wh;
void *whb;
SVN_ERR(svn_io_temp_dir(&tempdir, pool));
SVN_ERR(svn_io_open_unique_file2(tempfile, NULL,
apr_psprintf(pool, "%s/dump", tempdir),
".tmp", svn_io_file_del_on_close, pool));
temp_stream = svn_stream_from_aprfile(*tempfile, pool);
SVN_ERR(svn_fs_get_file_delta_stream(&delta_stream, oldroot, oldpath,
newroot, newpath, pool));
svn_txdelta_to_svndiff2(&wh, &whb, temp_stream, 0, pool);
SVN_ERR(svn_txdelta_send_txstream(delta_stream, wh, whb, pool));
SVN_ERR(svn_io_file_seek(*tempfile, APR_CUR, &offset, pool));
*len = offset;
offset = 0;
SVN_ERR(svn_io_file_seek(*tempfile, APR_SET, &offset, pool));
return SVN_NO_ERROR;
}
struct edit_baton {
const char *path;
svn_stream_t *stream;
svn_stream_t *feedback_stream;
svn_fs_root_t *fs_root;
svn_revnum_t current_rev;
svn_boolean_t use_deltas;
svn_boolean_t verify;
svn_revnum_t oldest_dumped_rev;
char buffer[SVN__STREAM_CHUNK_SIZE];
apr_size_t bufsize;
};
struct dir_baton {
struct edit_baton *edit_baton;
struct dir_baton *parent_dir_baton;
svn_boolean_t added;
svn_boolean_t written_out;
const char *path;
const char *cmp_path;
svn_revnum_t cmp_rev;
apr_hash_t *deleted_entries;
apr_pool_t *pool;
};
static struct dir_baton *
make_dir_baton(const char *path,
const char *cmp_path,
svn_revnum_t cmp_rev,
void *edit_baton,
void *parent_dir_baton,
svn_boolean_t added,
apr_pool_t *pool) {
struct edit_baton *eb = edit_baton;
struct dir_baton *pb = parent_dir_baton;
struct dir_baton *new_db = apr_pcalloc(pool, sizeof(*new_db));
const char *full_path;
if (path && (! pb))
abort();
if (pb)
full_path = svn_path_join(eb->path, path, pool);
else
full_path = apr_pstrdup(pool, eb->path);
if (cmp_path)
cmp_path = ((*cmp_path == '/') ? cmp_path + 1 : cmp_path);
new_db->edit_baton = eb;
new_db->parent_dir_baton = pb;
new_db->path = full_path;
new_db->cmp_path = cmp_path ? apr_pstrdup(pool, cmp_path) : NULL;
new_db->cmp_rev = cmp_rev;
new_db->added = added;
new_db->written_out = FALSE;
new_db->deleted_entries = apr_hash_make(pool);
new_db->pool = pool;
return new_db;
}
static svn_error_t *
dump_node(struct edit_baton *eb,
const char *path,
svn_node_kind_t kind,
enum svn_node_action action,
svn_boolean_t is_copy,
const char *cmp_path,
svn_revnum_t cmp_rev,
apr_pool_t *pool) {
svn_stringbuf_t *propstring;
svn_filesize_t content_length = 0;
apr_size_t len;
svn_boolean_t must_dump_text = FALSE, must_dump_props = FALSE;
const char *compare_path = path;
svn_revnum_t compare_rev = eb->current_rev - 1;
svn_fs_root_t *compare_root = NULL;
apr_file_t *delta_file = NULL;
SVN_ERR(svn_stream_printf(eb->stream, pool,
SVN_REPOS_DUMPFILE_NODE_PATH ": %s\n",
(*path == '/') ? path + 1 : path));
if (kind == svn_node_file)
SVN_ERR(svn_stream_printf(eb->stream, pool,
SVN_REPOS_DUMPFILE_NODE_KIND ": file\n"));
else if (kind == svn_node_dir)
SVN_ERR(svn_stream_printf(eb->stream, pool,
SVN_REPOS_DUMPFILE_NODE_KIND ": dir\n"));
if (cmp_path)
cmp_path = ((*cmp_path == '/') ? cmp_path + 1 : cmp_path);
if (ARE_VALID_COPY_ARGS(cmp_path, cmp_rev)) {
compare_path = cmp_path;
compare_rev = cmp_rev;
}
if (action == svn_node_action_change) {
SVN_ERR(svn_stream_printf(eb->stream, pool,
SVN_REPOS_DUMPFILE_NODE_ACTION
": change\n"));
SVN_ERR(svn_fs_revision_root(&compare_root,
svn_fs_root_fs(eb->fs_root),
compare_rev, pool));
SVN_ERR(svn_fs_props_changed(&must_dump_props,
compare_root, compare_path,
eb->fs_root, path, pool));
if (kind == svn_node_file)
SVN_ERR(svn_fs_contents_changed(&must_dump_text,
compare_root, compare_path,
eb->fs_root, path, pool));
} else if (action == svn_node_action_replace) {
if (! is_copy) {
SVN_ERR(svn_stream_printf(eb->stream, pool,
SVN_REPOS_DUMPFILE_NODE_ACTION
": replace\n"));
if (kind == svn_node_file)
must_dump_text = TRUE;
must_dump_props = TRUE;
} else {
SVN_ERR(svn_stream_printf(eb->stream, pool,
SVN_REPOS_DUMPFILE_NODE_ACTION
": delete\n\n"));
SVN_ERR(dump_node(eb, path, kind, svn_node_action_add,
is_copy, compare_path, compare_rev, pool));
must_dump_text = FALSE;
must_dump_props = FALSE;
}
} else if (action == svn_node_action_delete) {
SVN_ERR(svn_stream_printf(eb->stream, pool,
SVN_REPOS_DUMPFILE_NODE_ACTION
": delete\n"));
must_dump_text = FALSE;
must_dump_props = FALSE;
} else if (action == svn_node_action_add) {
SVN_ERR(svn_stream_printf(eb->stream, pool,
SVN_REPOS_DUMPFILE_NODE_ACTION ": add\n"));
if (! is_copy) {
if (kind == svn_node_file)
must_dump_text = TRUE;
must_dump_props = TRUE;
} else {
if (!eb->verify && cmp_rev < eb->oldest_dumped_rev)
SVN_ERR(svn_stream_printf
(eb->feedback_stream, pool,
_("WARNING: Referencing data in revision %ld"
", which is older than the oldest\nWARNING: dumped revision "
"(%ld). Loading this dump into an empty "
"repository\nWARNING: will fail.\n"),
cmp_rev, eb->oldest_dumped_rev));
SVN_ERR(svn_stream_printf(eb->stream, pool,
SVN_REPOS_DUMPFILE_NODE_COPYFROM_REV
": %ld\n"
SVN_REPOS_DUMPFILE_NODE_COPYFROM_PATH
": %s\n",
cmp_rev, cmp_path));
SVN_ERR(svn_fs_revision_root(&compare_root,
svn_fs_root_fs(eb->fs_root),
compare_rev, pool));
SVN_ERR(svn_fs_props_changed(&must_dump_props,
compare_root, compare_path,
eb->fs_root, path, pool));
if (kind == svn_node_file) {
unsigned char md5_digest[APR_MD5_DIGESTSIZE];
const char *hex_digest;
SVN_ERR(svn_fs_contents_changed(&must_dump_text,
compare_root, compare_path,
eb->fs_root, path, pool));
SVN_ERR(svn_fs_file_md5_checksum(md5_digest, compare_root,
compare_path, pool));
hex_digest = svn_md5_digest_to_cstring(md5_digest, pool);
if (hex_digest)
SVN_ERR(svn_stream_printf(eb->stream, pool,
SVN_REPOS_DUMPFILE_TEXT_COPY_SOURCE_CHECKSUM
": %s\n", hex_digest));
}
}
}
if ((! must_dump_text) && (! must_dump_props)) {
len = 2;
return svn_stream_write(eb->stream, "\n\n", &len);
}
if (must_dump_props) {
apr_hash_t *prophash, *oldhash = NULL;
apr_size_t proplen;
SVN_ERR(svn_fs_node_proplist(&prophash, eb->fs_root, path, pool));
if (eb->use_deltas && compare_root) {
SVN_ERR(svn_fs_node_proplist(&oldhash, compare_root, compare_path,
pool));
SVN_ERR(svn_stream_printf(eb->stream, pool,
SVN_REPOS_DUMPFILE_PROP_DELTA
": true\n"));
}
write_hash_to_stringbuf(prophash, oldhash, &propstring, pool);
proplen = propstring->len;
content_length += proplen;
SVN_ERR(svn_stream_printf(eb->stream, pool,
SVN_REPOS_DUMPFILE_PROP_CONTENT_LENGTH
": %" APR_SIZE_T_FMT "\n", proplen));
}
if (must_dump_text && (kind == svn_node_file)) {
unsigned char md5_digest[APR_MD5_DIGESTSIZE];
const char *hex_digest;
svn_filesize_t textlen;
if (eb->use_deltas) {
SVN_ERR(store_delta(&delta_file, &textlen, compare_root,
compare_path, eb->fs_root, path, pool));
SVN_ERR(svn_stream_printf(eb->stream, pool,
SVN_REPOS_DUMPFILE_TEXT_DELTA
": true\n"));
if (compare_root) {
SVN_ERR(svn_fs_file_md5_checksum(md5_digest, compare_root,
compare_path, pool));
hex_digest = svn_md5_digest_to_cstring(md5_digest, pool);
if (hex_digest)
SVN_ERR(svn_stream_printf(eb->stream, pool,
SVN_REPOS_DUMPFILE_TEXT_DELTA_BASE_CHECKSUM
": %s\n", hex_digest));
}
} else {
SVN_ERR(svn_fs_file_length(&textlen, eb->fs_root, path, pool));
}
content_length += textlen;
SVN_ERR(svn_stream_printf(eb->stream, pool,
SVN_REPOS_DUMPFILE_TEXT_CONTENT_LENGTH
": %" SVN_FILESIZE_T_FMT "\n", textlen));
SVN_ERR(svn_fs_file_md5_checksum(md5_digest, eb->fs_root, path, pool));
hex_digest = svn_md5_digest_to_cstring(md5_digest, pool);
if (hex_digest)
SVN_ERR(svn_stream_printf(eb->stream, pool,
SVN_REPOS_DUMPFILE_TEXT_CONTENT_CHECKSUM
": %s\n", hex_digest));
}
SVN_ERR(svn_stream_printf(eb->stream, pool,
SVN_REPOS_DUMPFILE_CONTENT_LENGTH
": %" SVN_FILESIZE_T_FMT "\n\n",
content_length));
if (must_dump_props) {
len = propstring->len;
SVN_ERR(svn_stream_write(eb->stream, propstring->data, &len));
}
if (must_dump_text && (kind == svn_node_file)) {
svn_stream_t *contents;
if (delta_file)
contents = svn_stream_from_aprfile(delta_file, pool);
else
SVN_ERR(svn_fs_file_contents(&contents, eb->fs_root, path, pool));
SVN_ERR(svn_stream_copy(contents, eb->stream, pool));
}
len = 2;
SVN_ERR(svn_stream_write(eb->stream, "\n\n", &len));
return SVN_NO_ERROR;
}
static svn_error_t *
open_root(void *edit_baton,
svn_revnum_t base_revision,
apr_pool_t *pool,
void **root_baton) {
*root_baton = make_dir_baton(NULL, NULL, SVN_INVALID_REVNUM,
edit_baton, NULL, FALSE, pool);
return SVN_NO_ERROR;
}
static svn_error_t *
delete_entry(const char *path,
svn_revnum_t revision,
void *parent_baton,
apr_pool_t *pool) {
struct dir_baton *pb = parent_baton;
const char *mypath = apr_pstrdup(pb->pool, path);
apr_hash_set(pb->deleted_entries, mypath, APR_HASH_KEY_STRING, pb);
return SVN_NO_ERROR;
}
static svn_error_t *
add_directory(const char *path,
void *parent_baton,
const char *copyfrom_path,
svn_revnum_t copyfrom_rev,
apr_pool_t *pool,
void **child_baton) {
struct dir_baton *pb = parent_baton;
struct edit_baton *eb = pb->edit_baton;
void *val;
svn_boolean_t is_copy = FALSE;
struct dir_baton *new_db
= make_dir_baton(path, copyfrom_path, copyfrom_rev, eb, pb, TRUE, pool);
val = apr_hash_get(pb->deleted_entries, path, APR_HASH_KEY_STRING);
is_copy = ARE_VALID_COPY_ARGS(copyfrom_path, copyfrom_rev) ? TRUE : FALSE;
SVN_ERR(dump_node(eb, path,
svn_node_dir,
val ? svn_node_action_replace : svn_node_action_add,
is_copy,
is_copy ? copyfrom_path : NULL,
is_copy ? copyfrom_rev : SVN_INVALID_REVNUM,
pool));
if (val)
apr_hash_set(pb->deleted_entries, path, APR_HASH_KEY_STRING, NULL);
new_db->written_out = TRUE;
*child_baton = new_db;
return SVN_NO_ERROR;
}
static svn_error_t *
open_directory(const char *path,
void *parent_baton,
svn_revnum_t base_revision,
apr_pool_t *pool,
void **child_baton) {
struct dir_baton *pb = parent_baton;
struct edit_baton *eb = pb->edit_baton;
struct dir_baton *new_db;
const char *cmp_path = NULL;
svn_revnum_t cmp_rev = SVN_INVALID_REVNUM;
if (pb && ARE_VALID_COPY_ARGS(pb->cmp_path, pb->cmp_rev)) {
cmp_path = svn_path_join(pb->cmp_path,
svn_path_basename(path, pool), pool);
cmp_rev = pb->cmp_rev;
}
new_db = make_dir_baton(path, cmp_path, cmp_rev, eb, pb, FALSE, pool);
*child_baton = new_db;
return SVN_NO_ERROR;
}
static svn_error_t *
close_directory(void *dir_baton,
apr_pool_t *pool) {
struct dir_baton *db = dir_baton;
struct edit_baton *eb = db->edit_baton;
apr_hash_index_t *hi;
apr_pool_t *subpool = svn_pool_create(pool);
for (hi = apr_hash_first(pool, db->deleted_entries);
hi;
hi = apr_hash_next(hi)) {
const void *key;
const char *path;
apr_hash_this(hi, &key, NULL, NULL);
path = key;
svn_pool_clear(subpool);
SVN_ERR(dump_node(eb, path,
svn_node_unknown, svn_node_action_delete,
FALSE, NULL, SVN_INVALID_REVNUM, subpool));
}
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
static svn_error_t *
add_file(const char *path,
void *parent_baton,
const char *copyfrom_path,
svn_revnum_t copyfrom_rev,
apr_pool_t *pool,
void **file_baton) {
struct dir_baton *pb = parent_baton;
struct edit_baton *eb = pb->edit_baton;
void *val;
svn_boolean_t is_copy = FALSE;
val = apr_hash_get(pb->deleted_entries, path, APR_HASH_KEY_STRING);
is_copy = ARE_VALID_COPY_ARGS(copyfrom_path, copyfrom_rev) ? TRUE : FALSE;
SVN_ERR(dump_node(eb, path,
svn_node_file,
val ? svn_node_action_replace : svn_node_action_add,
is_copy,
is_copy ? copyfrom_path : NULL,
is_copy ? copyfrom_rev : SVN_INVALID_REVNUM,
pool));
if (val)
apr_hash_set(pb->deleted_entries, path, APR_HASH_KEY_STRING, NULL);
*file_baton = NULL;
return SVN_NO_ERROR;
}
static svn_error_t *
open_file(const char *path,
void *parent_baton,
svn_revnum_t ancestor_revision,
apr_pool_t *pool,
void **file_baton) {
struct dir_baton *pb = parent_baton;
struct edit_baton *eb = pb->edit_baton;
const char *cmp_path = NULL;
svn_revnum_t cmp_rev = SVN_INVALID_REVNUM;
if (pb && ARE_VALID_COPY_ARGS(pb->cmp_path, pb->cmp_rev)) {
cmp_path = svn_path_join(pb->cmp_path,
svn_path_basename(path, pool), pool);
cmp_rev = pb->cmp_rev;
}
SVN_ERR(dump_node(eb, path,
svn_node_file, svn_node_action_change,
FALSE, cmp_path, cmp_rev, pool));
*file_baton = NULL;
return SVN_NO_ERROR;
}
static svn_error_t *
change_dir_prop(void *parent_baton,
const char *name,
const svn_string_t *value,
apr_pool_t *pool) {
struct dir_baton *db = parent_baton;
struct edit_baton *eb = db->edit_baton;
if (! db->written_out) {
SVN_ERR(dump_node(eb, db->path,
svn_node_dir, svn_node_action_change,
FALSE, db->cmp_path, db->cmp_rev, pool));
db->written_out = TRUE;
}
return SVN_NO_ERROR;
}
static svn_error_t *
get_dump_editor(const svn_delta_editor_t **editor,
void **edit_baton,
svn_fs_t *fs,
svn_revnum_t to_rev,
const char *root_path,
svn_stream_t *stream,
svn_stream_t *feedback_stream,
svn_revnum_t oldest_dumped_rev,
svn_boolean_t use_deltas,
svn_boolean_t verify,
apr_pool_t *pool) {
struct edit_baton *eb = apr_pcalloc(pool, sizeof(*eb));
svn_delta_editor_t *dump_editor = svn_delta_default_editor(pool);
eb->stream = stream;
eb->feedback_stream = feedback_stream;
eb->oldest_dumped_rev = oldest_dumped_rev;
eb->bufsize = sizeof(eb->buffer);
eb->path = apr_pstrdup(pool, root_path);
SVN_ERR(svn_fs_revision_root(&(eb->fs_root), fs, to_rev, pool));
eb->current_rev = to_rev;
eb->use_deltas = use_deltas;
eb->verify = verify;
dump_editor->open_root = open_root;
dump_editor->delete_entry = delete_entry;
dump_editor->add_directory = add_directory;
dump_editor->open_directory = open_directory;
dump_editor->close_directory = close_directory;
dump_editor->change_dir_prop = change_dir_prop;
dump_editor->add_file = add_file;
dump_editor->open_file = open_file;
*edit_baton = eb;
*editor = dump_editor;
return SVN_NO_ERROR;
}
static svn_error_t *
write_revision_record(svn_stream_t *stream,
svn_fs_t *fs,
svn_revnum_t rev,
apr_pool_t *pool) {
apr_size_t len;
apr_hash_t *props;
svn_stringbuf_t *encoded_prophash;
apr_time_t timetemp;
svn_string_t *datevalue;
SVN_ERR(svn_fs_revision_proplist(&props, fs, rev, pool));
datevalue = apr_hash_get(props, SVN_PROP_REVISION_DATE,
APR_HASH_KEY_STRING);
if (datevalue) {
SVN_ERR(svn_time_from_cstring(&timetemp, datevalue->data, pool));
datevalue = svn_string_create(svn_time_to_cstring(timetemp, pool),
pool);
apr_hash_set(props, SVN_PROP_REVISION_DATE, APR_HASH_KEY_STRING,
datevalue);
}
write_hash_to_stringbuf(props, NULL, &encoded_prophash, pool);
SVN_ERR(svn_stream_printf(stream, pool,
SVN_REPOS_DUMPFILE_REVISION_NUMBER
": %ld\n", rev));
SVN_ERR(svn_stream_printf(stream, pool,
SVN_REPOS_DUMPFILE_PROP_CONTENT_LENGTH
": %" APR_SIZE_T_FMT "\n",
encoded_prophash->len));
SVN_ERR(svn_stream_printf(stream, pool,
SVN_REPOS_DUMPFILE_CONTENT_LENGTH
": %" APR_SIZE_T_FMT "\n\n",
encoded_prophash->len));
len = encoded_prophash->len;
SVN_ERR(svn_stream_write(stream, encoded_prophash->data, &len));
len = 1;
SVN_ERR(svn_stream_write(stream, "\n", &len));
return SVN_NO_ERROR;
}
svn_error_t *
svn_repos_dump_fs2(svn_repos_t *repos,
svn_stream_t *stream,
svn_stream_t *feedback_stream,
svn_revnum_t start_rev,
svn_revnum_t end_rev,
svn_boolean_t incremental,
svn_boolean_t use_deltas,
svn_cancel_func_t cancel_func,
void *cancel_baton,
apr_pool_t *pool) {
const svn_delta_editor_t *dump_editor;
void *dump_edit_baton;
svn_revnum_t i;
svn_fs_t *fs = svn_repos_fs(repos);
apr_pool_t *subpool = svn_pool_create(pool);
svn_revnum_t youngest;
const char *uuid;
int version;
svn_boolean_t dumping = (stream != NULL);
SVN_ERR(svn_fs_youngest_rev(&youngest, fs, pool));
if (! SVN_IS_VALID_REVNUM(start_rev))
start_rev = 0;
if (! SVN_IS_VALID_REVNUM(end_rev))
end_rev = youngest;
if (! stream)
stream = svn_stream_empty(pool);
if (! feedback_stream)
feedback_stream = svn_stream_empty(pool);
if (start_rev > end_rev)
return svn_error_createf(SVN_ERR_REPOS_BAD_ARGS, NULL,
_("Start revision %ld"
" is greater than end revision %ld"),
start_rev, end_rev);
if (end_rev > youngest)
return svn_error_createf(SVN_ERR_REPOS_BAD_ARGS, NULL,
_("End revision %ld is invalid "
"(youngest revision is %ld)"),
end_rev, youngest);
if ((start_rev == 0) && incremental)
incremental = FALSE;
SVN_ERR(svn_fs_get_uuid(fs, &uuid, pool));
version = SVN_REPOS_DUMPFILE_FORMAT_VERSION;
if (!use_deltas)
version--;
SVN_ERR(svn_stream_printf(stream, pool,
SVN_REPOS_DUMPFILE_MAGIC_HEADER ": %d\n\n",
version));
SVN_ERR(svn_stream_printf(stream, pool, SVN_REPOS_DUMPFILE_UUID
": %s\n\n", uuid));
for (i = start_rev; i <= end_rev; i++) {
svn_revnum_t from_rev, to_rev;
svn_fs_root_t *to_root;
svn_boolean_t use_deltas_for_rev;
svn_pool_clear(subpool);
if (cancel_func)
SVN_ERR(cancel_func(cancel_baton));
if ((i == start_rev) && (! incremental)) {
if (i == 0) {
SVN_ERR(write_revision_record(stream, fs, 0, subpool));
to_rev = 0;
goto loop_end;
}
from_rev = 0;
to_rev = i;
} else {
from_rev = i - 1;
to_rev = i;
}
SVN_ERR(write_revision_record(stream, fs, to_rev, subpool));
use_deltas_for_rev = use_deltas && (incremental || i != start_rev);
SVN_ERR(get_dump_editor(&dump_editor, &dump_edit_baton, fs, to_rev,
"/", stream, feedback_stream, start_rev,
use_deltas_for_rev, FALSE, subpool));
SVN_ERR(svn_fs_revision_root(&to_root, fs, to_rev, subpool));
if ((i == start_rev) && (! incremental)) {
svn_fs_root_t *from_root;
SVN_ERR(svn_fs_revision_root(&from_root, fs, from_rev, subpool));
SVN_ERR(svn_repos_dir_delta2(from_root, "/", "",
to_root, "/",
dump_editor, dump_edit_baton,
NULL,
NULL,
FALSE,
svn_depth_infinity,
FALSE,
FALSE,
subpool));
} else {
SVN_ERR(svn_repos_replay2(to_root, "", SVN_INVALID_REVNUM, FALSE,
dump_editor, dump_edit_baton,
NULL, NULL, subpool));
}
loop_end:
SVN_ERR(svn_stream_printf(feedback_stream, pool,
dumping
? _("* Dumped revision %ld.\n")
: _("* Verified revision %ld.\n"),
to_rev));
}
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
svn_error_t *
svn_repos_dump_fs(svn_repos_t *repos,
svn_stream_t *stream,
svn_stream_t *feedback_stream,
svn_revnum_t start_rev,
svn_revnum_t end_rev,
svn_boolean_t incremental,
svn_cancel_func_t cancel_func,
void *cancel_baton,
apr_pool_t *pool) {
return svn_repos_dump_fs2(repos, stream, feedback_stream, start_rev,
end_rev, incremental, FALSE, cancel_func,
cancel_baton, pool);
}
static svn_error_t *
verify_directory_entry(void *baton, const void *key, apr_ssize_t klen,
void *val, apr_pool_t *pool) {
struct dir_baton *db = baton;
char *path = svn_path_join(db->path, (const char *)key, pool);
svn_node_kind_t kind;
apr_hash_t *dirents;
svn_filesize_t len;
SVN_ERR(svn_fs_check_path(&kind, db->edit_baton->fs_root, path, pool));
switch (kind) {
case svn_node_dir:
SVN_ERR(svn_fs_dir_entries(&dirents, db->edit_baton->fs_root, path, pool));
break;
case svn_node_file:
SVN_ERR(svn_fs_file_length(&len, db->edit_baton->fs_root, path, pool));
break;
default:
return svn_error_createf(SVN_ERR_NODE_UNEXPECTED_KIND, NULL,
_("Unexpected node kind %d for '%s'"), kind, path);
}
return SVN_NO_ERROR;
}
static svn_error_t *
verify_close_directory(void *dir_baton,
apr_pool_t *pool) {
struct dir_baton *db = dir_baton;
apr_hash_t *dirents;
SVN_ERR(svn_fs_dir_entries(&dirents, db->edit_baton->fs_root,
db->path, pool));
SVN_ERR(svn_iter_apr_hash(NULL, dirents, verify_directory_entry,
dir_baton, pool));
return close_directory(dir_baton, pool);
}
svn_error_t *
svn_repos_verify_fs(svn_repos_t *repos,
svn_stream_t *feedback_stream,
svn_revnum_t start_rev,
svn_revnum_t end_rev,
svn_cancel_func_t cancel_func,
void *cancel_baton,
apr_pool_t *pool) {
svn_fs_t *fs = svn_repos_fs(repos);
svn_revnum_t youngest;
svn_revnum_t rev;
apr_pool_t *iterpool = svn_pool_create(pool);
SVN_ERR(svn_fs_youngest_rev(&youngest, fs, pool));
if (! SVN_IS_VALID_REVNUM(start_rev))
start_rev = 0;
if (! SVN_IS_VALID_REVNUM(end_rev))
end_rev = youngest;
if (! feedback_stream)
feedback_stream = svn_stream_empty(pool);
if (start_rev > end_rev)
return svn_error_createf(SVN_ERR_REPOS_BAD_ARGS, NULL,
_("Start revision %ld"
" is greater than end revision %ld"),
start_rev, end_rev);
if (end_rev > youngest)
return svn_error_createf(SVN_ERR_REPOS_BAD_ARGS, NULL,
_("End revision %ld is invalid "
"(youngest revision is %ld)"),
end_rev, youngest);
for (rev = start_rev; rev <= end_rev; rev++) {
svn_delta_editor_t *dump_editor;
void *dump_edit_baton;
const svn_delta_editor_t *cancel_editor;
void *cancel_edit_baton;
svn_fs_root_t *to_root;
svn_pool_clear(iterpool);
SVN_ERR(get_dump_editor((const svn_delta_editor_t **)&dump_editor,
&dump_edit_baton, fs, rev, "",
svn_stream_empty(pool), feedback_stream,
start_rev,
FALSE, TRUE,
iterpool));
dump_editor->close_directory = verify_close_directory;
SVN_ERR(svn_delta_get_cancellation_editor(cancel_func, cancel_baton,
dump_editor, dump_edit_baton,
&cancel_editor,
&cancel_edit_baton,
iterpool));
SVN_ERR(svn_fs_revision_root(&to_root, fs, rev, iterpool));
SVN_ERR(svn_repos_replay2(to_root, "", SVN_INVALID_REVNUM, FALSE,
cancel_editor, cancel_edit_baton,
NULL, NULL, iterpool));
SVN_ERR(svn_stream_printf(feedback_stream, iterpool,
_("* Verified revision %ld.\n"),
rev));
}
svn_pool_destroy(iterpool);
return SVN_NO_ERROR;
}