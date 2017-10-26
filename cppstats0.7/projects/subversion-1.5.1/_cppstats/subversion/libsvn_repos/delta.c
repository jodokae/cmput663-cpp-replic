#include <assert.h>
#include <apr_hash.h>
#include <apr_md5.h>
#include "svn_types.h"
#include "svn_delta.h"
#include "svn_fs.h"
#include "svn_md5.h"
#include "svn_path.h"
#include "svn_repos.h"
#include "svn_pools.h"
#include "svn_props.h"
#include "svn_private_config.h"
#include "repos.h"
struct context {
const svn_delta_editor_t *editor;
const char *edit_base_path;
svn_fs_root_t *source_root;
svn_fs_root_t *target_root;
svn_repos_authz_func_t authz_read_func;
void *authz_read_baton;
svn_boolean_t text_deltas;
svn_boolean_t entry_props;
svn_boolean_t ignore_ancestry;
};
typedef svn_error_t *proplist_change_fn_t(struct context *c,
void *object,
const char *name,
const svn_string_t *value,
apr_pool_t *pool);
static svn_revnum_t get_path_revision(svn_fs_root_t *root,
const char *path,
apr_pool_t *pool);
static svn_error_t *change_dir_prop(struct context *c,
void *object,
const char *path,
const svn_string_t *value,
apr_pool_t *pool);
static svn_error_t *change_file_prop(struct context *c,
void *object,
const char *path,
const svn_string_t *value,
apr_pool_t *pool);
static svn_error_t *delta_proplists(struct context *c,
const char *source_path,
const char *target_path,
proplist_change_fn_t *change_fn,
void *object,
apr_pool_t *pool);
static svn_error_t *send_text_delta(struct context *c,
void *file_baton,
const char *base_checksum,
svn_txdelta_stream_t *delta_stream,
apr_pool_t *pool);
static svn_error_t *delta_files(struct context *c,
void *file_baton,
const char *source_path,
const char *target_path,
apr_pool_t *pool);
static svn_error_t *delete(struct context *c,
void *dir_baton,
const char *edit_path,
apr_pool_t *pool);
static svn_error_t *add_file_or_dir(struct context *c,
void *dir_baton,
svn_depth_t depth,
const char *target_path,
const char *edit_path,
svn_node_kind_t tgt_kind,
apr_pool_t *pool);
static svn_error_t *replace_file_or_dir(struct context *c,
void *dir_baton,
svn_depth_t depth,
const char *source_path,
const char *target_path,
const char *edit_path,
svn_node_kind_t tgt_kind,
apr_pool_t *pool);
static svn_error_t *absent_file_or_dir(struct context *c,
void *dir_baton,
const char *edit_path,
svn_node_kind_t tgt_kind,
apr_pool_t *pool);
static svn_error_t *delta_dirs(struct context *c,
void *dir_baton,
svn_depth_t depth,
const char *source_path,
const char *target_path,
const char *edit_path,
apr_pool_t *pool);
#define MAYBE_DEMOTE_DEPTH(depth) (((depth) == svn_depth_immediates || (depth) == svn_depth_files) ? svn_depth_empty : (depth))
static svn_error_t *
authz_root_check(svn_fs_root_t *root,
const char *path,
svn_repos_authz_func_t authz_read_func,
void *authz_read_baton,
apr_pool_t *pool) {
svn_boolean_t allowed;
if (authz_read_func) {
SVN_ERR(authz_read_func(&allowed, root, path, authz_read_baton, pool));
if (! allowed)
return svn_error_create(SVN_ERR_AUTHZ_ROOT_UNREADABLE, 0,
_("Unable to open root of edit"));
}
return SVN_NO_ERROR;
}
static svn_error_t *
not_a_dir_error(const char *role,
const char *path) {
return svn_error_createf
(SVN_ERR_FS_NOT_DIRECTORY, 0,
"Invalid %s directory '%s'",
role, path ? path : "(null)");
}
svn_error_t *
svn_repos_dir_delta2(svn_fs_root_t *src_root,
const char *src_parent_dir,
const char *src_entry,
svn_fs_root_t *tgt_root,
const char *tgt_fullpath,
const svn_delta_editor_t *editor,
void *edit_baton,
svn_repos_authz_func_t authz_read_func,
void *authz_read_baton,
svn_boolean_t text_deltas,
svn_depth_t depth,
svn_boolean_t entry_props,
svn_boolean_t ignore_ancestry,
apr_pool_t *pool) {
void *root_baton = NULL;
struct context c;
const char *src_fullpath;
const svn_fs_id_t *src_id, *tgt_id;
svn_node_kind_t src_kind, tgt_kind;
svn_revnum_t rootrev;
int distance;
const char *authz_root_path;
if (! src_parent_dir)
return not_a_dir_error("source parent", src_parent_dir);
if (! tgt_fullpath)
return svn_error_create(SVN_ERR_FS_PATH_SYNTAX, 0,
_("Invalid target path"));
if (depth == svn_depth_exclude)
return svn_error_create(SVN_ERR_REPOS_BAD_ARGS, NULL,
_("Delta depth 'exclude' not supported"));
if (*src_entry)
authz_root_path = svn_path_dirname(tgt_fullpath, pool);
else
authz_root_path = tgt_fullpath;
src_fullpath = svn_path_join(src_parent_dir, src_entry, pool);
SVN_ERR(svn_fs_check_path(&tgt_kind, tgt_root, tgt_fullpath, pool));
SVN_ERR(svn_fs_check_path(&src_kind, src_root, src_fullpath, pool));
if ((tgt_kind == svn_node_none) && (src_kind == svn_node_none))
goto cleanup;
if ((! *src_entry) && ((src_kind != svn_node_dir)
|| tgt_kind != svn_node_dir))
return svn_error_create
(SVN_ERR_FS_PATH_SYNTAX, 0,
_("Invalid editor anchoring; at least one of the "
"input paths is not a directory and there was no source entry"));
if (svn_fs_is_revision_root(tgt_root)) {
SVN_ERR(editor->set_target_revision
(edit_baton, svn_fs_revision_root_revision(tgt_root), pool));
} else if (svn_fs_is_txn_root(tgt_root)) {
SVN_ERR(editor->set_target_revision
(edit_baton, svn_fs_txn_root_base_revision(tgt_root), pool));
}
c.editor = editor;
c.source_root = src_root;
c.target_root = tgt_root;
c.authz_read_func = authz_read_func;
c.authz_read_baton = authz_read_baton;
c.text_deltas = text_deltas;
c.entry_props = entry_props;
c.ignore_ancestry = ignore_ancestry;
rootrev = get_path_revision(src_root, src_parent_dir, pool);
if (tgt_kind == svn_node_none) {
SVN_ERR(authz_root_check(tgt_root, authz_root_path,
authz_read_func, authz_read_baton, pool));
SVN_ERR(editor->open_root(edit_baton, rootrev, pool, &root_baton));
SVN_ERR(delete(&c, root_baton, src_entry, pool));
goto cleanup;
}
if (src_kind == svn_node_none) {
SVN_ERR(authz_root_check(tgt_root, authz_root_path,
authz_read_func, authz_read_baton, pool));
SVN_ERR(editor->open_root(edit_baton, rootrev, pool, &root_baton));
SVN_ERR(add_file_or_dir(&c, root_baton, depth, tgt_fullpath,
src_entry, tgt_kind, pool));
goto cleanup;
}
SVN_ERR(svn_fs_node_id(&tgt_id, tgt_root, tgt_fullpath, pool));
SVN_ERR(svn_fs_node_id(&src_id, src_root, src_fullpath, pool));
distance = svn_fs_compare_ids(src_id, tgt_id);
if (distance == 0) {
goto cleanup;
} else if (*src_entry) {
if ((src_kind != tgt_kind)
|| ((distance == -1) && (! ignore_ancestry))) {
SVN_ERR(authz_root_check(tgt_root, authz_root_path,
authz_read_func, authz_read_baton, pool));
SVN_ERR(editor->open_root(edit_baton, rootrev, pool, &root_baton));
SVN_ERR(delete(&c, root_baton, src_entry, pool));
SVN_ERR(add_file_or_dir(&c, root_baton, depth, tgt_fullpath,
src_entry, tgt_kind, pool));
}
else {
SVN_ERR(authz_root_check(tgt_root, authz_root_path,
authz_read_func, authz_read_baton, pool));
SVN_ERR(editor->open_root(edit_baton, rootrev, pool, &root_baton));
SVN_ERR(replace_file_or_dir(&c, root_baton, depth, src_fullpath,
tgt_fullpath, src_entry,
tgt_kind, pool));
}
} else {
SVN_ERR(authz_root_check(tgt_root, authz_root_path,
authz_read_func, authz_read_baton, pool));
SVN_ERR(editor->open_root(edit_baton, rootrev, pool, &root_baton));
SVN_ERR(delta_dirs(&c, root_baton, depth, src_fullpath,
tgt_fullpath, "", pool));
}
cleanup:
if (root_baton)
SVN_ERR(editor->close_directory(root_baton, pool));
SVN_ERR(editor->close_edit(edit_baton, pool));
return SVN_NO_ERROR;
}
svn_error_t *
svn_repos_dir_delta(svn_fs_root_t *src_root,
const char *src_parent_dir,
const char *src_entry,
svn_fs_root_t *tgt_root,
const char *tgt_fullpath,
const svn_delta_editor_t *editor,
void *edit_baton,
svn_repos_authz_func_t authz_read_func,
void *authz_read_baton,
svn_boolean_t text_deltas,
svn_boolean_t recurse,
svn_boolean_t entry_props,
svn_boolean_t ignore_ancestry,
apr_pool_t *pool) {
return svn_repos_dir_delta2(src_root,
src_parent_dir,
src_entry,
tgt_root,
tgt_fullpath,
editor,
edit_baton,
authz_read_func,
authz_read_baton,
text_deltas,
SVN_DEPTH_INFINITY_OR_FILES(recurse),
entry_props,
ignore_ancestry,
pool);
}
static svn_revnum_t
get_path_revision(svn_fs_root_t *root,
const char *path,
apr_pool_t *pool) {
svn_revnum_t revision;
svn_error_t *err;
if (svn_fs_is_revision_root(root))
return svn_fs_revision_root_revision(root);
if ((err = svn_fs_node_created_rev(&revision, root, path, pool))) {
revision = SVN_INVALID_REVNUM;
svn_error_clear(err);
}
return revision;
}
static svn_error_t *
change_dir_prop(struct context *c,
void *object,
const char *name,
const svn_string_t *value,
apr_pool_t *pool) {
return c->editor->change_dir_prop(object, name, value, pool);
}
static svn_error_t *
change_file_prop(struct context *c,
void *object,
const char *name,
const svn_string_t *value,
apr_pool_t *pool) {
return c->editor->change_file_prop(object, name, value, pool);
}
static svn_error_t *
delta_proplists(struct context *c,
const char *source_path,
const char *target_path,
proplist_change_fn_t *change_fn,
void *object,
apr_pool_t *pool) {
apr_hash_t *s_props = 0;
apr_hash_t *t_props = 0;
apr_pool_t *subpool;
apr_array_header_t *prop_diffs;
int i;
assert(target_path);
subpool = svn_pool_create(pool);
if (c->entry_props) {
svn_revnum_t committed_rev = SVN_INVALID_REVNUM;
svn_string_t *cr_str = NULL;
svn_string_t *committed_date = NULL;
svn_string_t *last_author = NULL;
SVN_ERR(svn_fs_node_created_rev(&committed_rev, c->target_root,
target_path, subpool));
if (SVN_IS_VALID_REVNUM(committed_rev)) {
svn_fs_t *fs = svn_fs_root_fs(c->target_root);
apr_hash_t *r_props;
const char *uuid;
cr_str = svn_string_createf(subpool, "%ld",
committed_rev);
SVN_ERR(change_fn(c, object, SVN_PROP_ENTRY_COMMITTED_REV,
cr_str, subpool));
SVN_ERR(svn_fs_revision_proplist(&r_props, fs, committed_rev,
pool));
committed_date = apr_hash_get(r_props, SVN_PROP_REVISION_DATE,
APR_HASH_KEY_STRING);
if (committed_date || source_path) {
SVN_ERR(change_fn(c, object, SVN_PROP_ENTRY_COMMITTED_DATE,
committed_date, subpool));
}
last_author = apr_hash_get(r_props, SVN_PROP_REVISION_AUTHOR,
APR_HASH_KEY_STRING);
if (last_author || source_path) {
SVN_ERR(change_fn(c, object, SVN_PROP_ENTRY_LAST_AUTHOR,
last_author, subpool));
}
SVN_ERR(svn_fs_get_uuid(fs, &uuid, subpool));
SVN_ERR(change_fn(c, object, SVN_PROP_ENTRY_UUID,
svn_string_create(uuid, subpool),
subpool));
}
}
if (source_path) {
svn_boolean_t changed;
SVN_ERR(svn_fs_props_changed(&changed, c->target_root, target_path,
c->source_root, source_path, subpool));
if (! changed)
goto cleanup;
SVN_ERR(svn_fs_node_proplist(&s_props, c->source_root,
source_path, subpool));
} else {
s_props = apr_hash_make(subpool);
}
SVN_ERR(svn_fs_node_proplist(&t_props, c->target_root,
target_path, subpool));
SVN_ERR(svn_prop_diffs(&prop_diffs, t_props, s_props, subpool));
for (i = 0; i < prop_diffs->nelts; i++) {
const svn_prop_t *pc = &APR_ARRAY_IDX(prop_diffs, i, svn_prop_t);
SVN_ERR(change_fn(c, object, pc->name, pc->value, subpool));
}
cleanup:
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
static svn_error_t *
send_text_delta(struct context *c,
void *file_baton,
const char *base_checksum,
svn_txdelta_stream_t *delta_stream,
apr_pool_t *pool) {
svn_txdelta_window_handler_t delta_handler;
void *delta_handler_baton;
SVN_ERR(c->editor->apply_textdelta
(file_baton, base_checksum, pool,
&delta_handler, &delta_handler_baton));
if (c->text_deltas && delta_stream) {
SVN_ERR(svn_txdelta_send_txstream(delta_stream,
delta_handler,
delta_handler_baton,
pool));
} else {
SVN_ERR(delta_handler(NULL, delta_handler_baton));
}
return SVN_NO_ERROR;
}
svn_error_t *
svn_repos__compare_files(svn_boolean_t *changed_p,
svn_fs_root_t *root1,
const char *path1,
svn_fs_root_t *root2,
const char *path2,
apr_pool_t *pool) {
svn_filesize_t size1, size2;
unsigned char digest1[APR_MD5_DIGESTSIZE], digest2[APR_MD5_DIGESTSIZE];
svn_stream_t *stream1, *stream2;
char *buf1, *buf2;
apr_size_t len1, len2;
SVN_ERR(svn_fs_contents_changed(changed_p, root1, path1,
root2, path2, pool));
if (!*changed_p)
return SVN_NO_ERROR;
*changed_p = FALSE;
SVN_ERR(svn_fs_file_length(&size1, root1, path1, pool));
SVN_ERR(svn_fs_file_length(&size2, root2, path2, pool));
if (size1 != size2) {
*changed_p = TRUE;
return SVN_NO_ERROR;
}
SVN_ERR(svn_fs_file_md5_checksum(digest1, root1, path1, pool));
SVN_ERR(svn_fs_file_md5_checksum(digest2, root2, path2, pool));
if (! svn_md5_digests_match(digest1, digest2)) {
*changed_p = TRUE;
return SVN_NO_ERROR;
}
SVN_ERR(svn_fs_file_contents(&stream1, root1, path1, pool));
SVN_ERR(svn_fs_file_contents(&stream2, root2, path2, pool));
buf1 = apr_palloc(pool, SVN__STREAM_CHUNK_SIZE);
buf2 = apr_palloc(pool, SVN__STREAM_CHUNK_SIZE);
do {
len1 = len2 = SVN__STREAM_CHUNK_SIZE;
SVN_ERR(svn_stream_read(stream1, buf1, &len1));
SVN_ERR(svn_stream_read(stream2, buf2, &len2));
if (len1 != len2 || memcmp(buf1, buf2, len1)) {
*changed_p = TRUE;
return SVN_NO_ERROR;
}
} while (len1 > 0);
return SVN_NO_ERROR;
}
static svn_error_t *
delta_files(struct context *c,
void *file_baton,
const char *source_path,
const char *target_path,
apr_pool_t *pool) {
apr_pool_t *subpool;
svn_boolean_t changed = TRUE;
assert(target_path);
subpool = svn_pool_create(pool);
SVN_ERR(delta_proplists(c, source_path, target_path,
change_file_prop, file_baton, subpool));
if (source_path) {
if (c->ignore_ancestry)
SVN_ERR(svn_repos__compare_files(&changed,
c->target_root, target_path,
c->source_root, source_path,
subpool));
else
SVN_ERR(svn_fs_contents_changed(&changed,
c->target_root, target_path,
c->source_root, source_path,
subpool));
} else {
}
if (changed) {
svn_txdelta_stream_t *delta_stream = NULL;
unsigned char source_digest[APR_MD5_DIGESTSIZE];
const char *source_hex_digest = NULL;
if (c->text_deltas) {
SVN_ERR(svn_fs_get_file_delta_stream
(&delta_stream,
source_path ? c->source_root : NULL,
source_path ? source_path : NULL,
c->target_root, target_path, subpool));
}
if (source_path) {
SVN_ERR(svn_fs_file_md5_checksum
(source_digest, c->source_root, source_path, subpool));
source_hex_digest = svn_md5_digest_to_cstring(source_digest,
subpool);
}
SVN_ERR(send_text_delta(c, file_baton, source_hex_digest,
delta_stream, subpool));
}
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
static svn_error_t *
delete(struct context *c,
void *dir_baton,
const char *edit_path,
apr_pool_t *pool) {
return c->editor->delete_entry(edit_path, SVN_INVALID_REVNUM,
dir_baton, pool);
}
static svn_error_t *
add_file_or_dir(struct context *c, void *dir_baton,
svn_depth_t depth,
const char *target_path,
const char *edit_path,
svn_node_kind_t tgt_kind,
apr_pool_t *pool) {
struct context *context = c;
svn_boolean_t allowed;
assert(target_path && edit_path);
if (c->authz_read_func) {
SVN_ERR(c->authz_read_func(&allowed, c->target_root, target_path,
c->authz_read_baton, pool));
if (!allowed)
return absent_file_or_dir(c, dir_baton, edit_path, tgt_kind, pool);
}
if (tgt_kind == svn_node_dir) {
void *subdir_baton;
SVN_ERR(context->editor->add_directory(edit_path, dir_baton, NULL,
SVN_INVALID_REVNUM, pool,
&subdir_baton));
SVN_ERR(delta_dirs(context, subdir_baton, MAYBE_DEMOTE_DEPTH(depth),
NULL, target_path, edit_path, pool));
SVN_ERR(context->editor->close_directory(subdir_baton, pool));
} else {
void *file_baton;
unsigned char digest[APR_MD5_DIGESTSIZE];
SVN_ERR(context->editor->add_file(edit_path, dir_baton,
NULL, SVN_INVALID_REVNUM, pool,
&file_baton));
SVN_ERR(delta_files(context, file_baton, NULL, target_path, pool));
SVN_ERR(svn_fs_file_md5_checksum(digest, context->target_root,
target_path, pool));
SVN_ERR(context->editor->close_file
(file_baton, svn_md5_digest_to_cstring(digest, pool), pool));
}
return SVN_NO_ERROR;
}
static svn_error_t *
replace_file_or_dir(struct context *c,
void *dir_baton,
svn_depth_t depth,
const char *source_path,
const char *target_path,
const char *edit_path,
svn_node_kind_t tgt_kind,
apr_pool_t *pool) {
svn_revnum_t base_revision = SVN_INVALID_REVNUM;
svn_boolean_t allowed;
assert(target_path && source_path && edit_path);
if (c->authz_read_func) {
SVN_ERR(c->authz_read_func(&allowed, c->target_root, target_path,
c->authz_read_baton, pool));
if (!allowed)
return absent_file_or_dir(c, dir_baton, edit_path, tgt_kind, pool);
}
base_revision = get_path_revision(c->source_root, source_path, pool);
if (tgt_kind == svn_node_dir) {
void *subdir_baton;
SVN_ERR(c->editor->open_directory(edit_path, dir_baton,
base_revision, pool,
&subdir_baton));
SVN_ERR(delta_dirs(c, subdir_baton, MAYBE_DEMOTE_DEPTH(depth),
source_path, target_path, edit_path, pool));
SVN_ERR(c->editor->close_directory(subdir_baton, pool));
} else {
void *file_baton;
unsigned char digest[APR_MD5_DIGESTSIZE];
SVN_ERR(c->editor->open_file(edit_path, dir_baton, base_revision,
pool, &file_baton));
SVN_ERR(delta_files(c, file_baton, source_path, target_path, pool));
SVN_ERR(svn_fs_file_md5_checksum(digest, c->target_root,
target_path, pool));
SVN_ERR(c->editor->close_file
(file_baton, svn_md5_digest_to_cstring(digest, pool), pool));
}
return SVN_NO_ERROR;
}
static svn_error_t *
absent_file_or_dir(struct context *c,
void *dir_baton,
const char *edit_path,
svn_node_kind_t tgt_kind,
apr_pool_t *pool) {
assert(edit_path);
if (tgt_kind == svn_node_dir)
SVN_ERR(c->editor->absent_directory(edit_path, dir_baton, pool));
else
SVN_ERR(c->editor->absent_file(edit_path, dir_baton, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
delta_dirs(struct context *c,
void *dir_baton,
svn_depth_t depth,
const char *source_path,
const char *target_path,
const char *edit_path,
apr_pool_t *pool) {
apr_hash_t *s_entries = 0, *t_entries = 0;
apr_hash_index_t *hi;
apr_pool_t *subpool;
assert(target_path);
SVN_ERR(delta_proplists(c, source_path, target_path,
change_dir_prop, dir_baton, pool));
SVN_ERR(svn_fs_dir_entries(&t_entries, c->target_root,
target_path, pool));
if (source_path)
SVN_ERR(svn_fs_dir_entries(&s_entries, c->source_root,
source_path, pool));
subpool = svn_pool_create(pool);
for (hi = apr_hash_first(pool, t_entries); hi; hi = apr_hash_next(hi)) {
const svn_fs_dirent_t *s_entry, *t_entry;
const void *key;
void *val;
apr_ssize_t klen;
const char *t_fullpath;
const char *e_fullpath;
const char *s_fullpath;
svn_node_kind_t tgt_kind;
svn_pool_clear(subpool);
apr_hash_this(hi, &key, &klen, &val);
t_entry = val;
tgt_kind = t_entry->kind;
t_fullpath = svn_path_join(target_path, t_entry->name, subpool);
e_fullpath = svn_path_join(edit_path, t_entry->name, subpool);
if (s_entries && ((s_entry = apr_hash_get(s_entries, key, klen)) != 0)) {
int distance;
svn_node_kind_t src_kind;
s_fullpath = svn_path_join(source_path, t_entry->name, subpool);
src_kind = s_entry->kind;
if (depth == svn_depth_infinity
|| src_kind != svn_node_dir
|| (src_kind == svn_node_dir
&& depth == svn_depth_immediates)) {
distance = svn_fs_compare_ids(s_entry->id, t_entry->id);
if (distance == 0) {
} else if ((src_kind != tgt_kind)
|| ((distance == -1) && (! c->ignore_ancestry))) {
SVN_ERR(delete(c, dir_baton, e_fullpath, subpool));
SVN_ERR(add_file_or_dir(c, dir_baton,
MAYBE_DEMOTE_DEPTH(depth),
t_fullpath, e_fullpath, tgt_kind,
subpool));
} else {
SVN_ERR(replace_file_or_dir(c, dir_baton,
MAYBE_DEMOTE_DEPTH(depth),
s_fullpath, t_fullpath,
e_fullpath, tgt_kind,
subpool));
}
}
apr_hash_set(s_entries, key, APR_HASH_KEY_STRING, NULL);
} else {
if (depth == svn_depth_infinity
|| tgt_kind != svn_node_dir
|| (tgt_kind == svn_node_dir
&& depth == svn_depth_immediates)) {
SVN_ERR(add_file_or_dir(c, dir_baton,
MAYBE_DEMOTE_DEPTH(depth),
t_fullpath, e_fullpath, tgt_kind,
subpool));
}
}
}
if (s_entries) {
for (hi = apr_hash_first(pool, s_entries); hi; hi = apr_hash_next(hi)) {
const svn_fs_dirent_t *s_entry;
void *val;
const char *e_fullpath;
svn_node_kind_t src_kind;
svn_pool_clear(subpool);
apr_hash_this(hi, NULL, NULL, &val);
s_entry = val;
src_kind = s_entry->kind;
e_fullpath = svn_path_join(edit_path, s_entry->name, subpool);
if (depth == svn_depth_infinity
|| src_kind != svn_node_dir
|| (src_kind == svn_node_dir
&& depth == svn_depth_immediates)) {
SVN_ERR(delete(c, dir_baton, e_fullpath, subpool));
}
}
}
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
