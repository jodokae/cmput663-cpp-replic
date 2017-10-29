#include <assert.h>
#include <apr_hash.h>
#include <apr_md5.h>
#include "svn_types.h"
#include "svn_delta.h"
#include "svn_fs.h"
#include "svn_md5.h"
#include "svn_repos.h"
#include "svn_props.h"
#include "svn_pools.h"
#include "svn_path.h"
#include "svn_private_config.h"
struct copy_info {
const char *path;
const char *copyfrom_path;
svn_revnum_t copyfrom_rev;
};
struct path_driver_cb_baton {
const svn_delta_editor_t *editor;
void *edit_baton;
svn_fs_root_t *root;
svn_fs_root_t *compare_root;
apr_hash_t *changed_paths;
svn_repos_authz_func_t authz_read_func;
void *authz_read_baton;
const char *base_path;
int base_path_len;
svn_revnum_t low_water_mark;
apr_array_header_t *copies;
apr_pool_t *pool;
};
static svn_error_t *
add_subdir(svn_fs_root_t *source_root,
svn_fs_root_t *target_root,
const svn_delta_editor_t *editor,
void *edit_baton,
const char *path,
void *parent_baton,
const char *source_path,
svn_repos_authz_func_t authz_read_func,
void *authz_read_baton,
apr_hash_t *changed_paths,
apr_pool_t *pool,
void **dir_baton) {
apr_pool_t *subpool = svn_pool_create(pool);
apr_hash_index_t *hi, *phi;
apr_hash_t *dirents;
apr_hash_t *props;
SVN_ERR(editor->add_directory(path, parent_baton, NULL,
SVN_INVALID_REVNUM, pool, dir_baton));
SVN_ERR(svn_fs_node_proplist(&props, target_root, path, pool));
for (phi = apr_hash_first(pool, props); phi; phi = apr_hash_next(phi)) {
const void *key;
void *val;
svn_pool_clear(subpool);
apr_hash_this(phi, &key, NULL, &val);
SVN_ERR(editor->change_dir_prop(*dir_baton,
key,
val,
subpool));
}
SVN_ERR(svn_fs_dir_entries(&dirents, source_root, source_path, pool));
for (hi = apr_hash_first(pool, dirents); hi; hi = apr_hash_next(hi)) {
svn_fs_path_change_t *change;
svn_boolean_t readable = TRUE;
svn_fs_dirent_t *dent;
const char *new_path;
void *val;
svn_pool_clear(subpool);
apr_hash_this(hi, NULL, NULL, &val);
dent = val;
new_path = svn_path_join(path, dent->name, subpool);
change = apr_hash_get(changed_paths, new_path, APR_HASH_KEY_STRING);
if (change) {
apr_hash_set(changed_paths, new_path, APR_HASH_KEY_STRING, NULL);
if (change->change_kind == svn_fs_path_change_delete)
continue;
}
if (authz_read_func)
SVN_ERR(authz_read_func(&readable, target_root, new_path,
authz_read_baton, pool));
if (! readable)
continue;
if (dent->kind == svn_node_dir) {
void *new_dir_baton;
SVN_ERR(add_subdir(source_root, target_root, editor, edit_baton,
new_path, *dir_baton,
svn_path_join(source_path, dent->name,
subpool),
authz_read_func, authz_read_baton,
changed_paths, subpool, &new_dir_baton));
SVN_ERR(editor->close_directory(new_dir_baton, subpool));
} else if (dent->kind == svn_node_file) {
svn_txdelta_window_handler_t delta_handler;
void *delta_handler_baton, *file_baton;
svn_txdelta_stream_t *delta_stream;
unsigned char digest[APR_MD5_DIGESTSIZE];
SVN_ERR(editor->add_file(new_path, *dir_baton, NULL,
SVN_INVALID_REVNUM, pool, &file_baton));
SVN_ERR(svn_fs_node_proplist(&props, target_root, new_path, subpool));
for (phi = apr_hash_first(pool, props);
phi;
phi = apr_hash_next(phi)) {
const void *key;
apr_hash_this(phi, &key, NULL, &val);
SVN_ERR(editor->change_file_prop(file_baton,
key,
val,
subpool));
}
SVN_ERR(editor->apply_textdelta(file_baton, NULL, pool,
&delta_handler,
&delta_handler_baton));
SVN_ERR(svn_fs_get_file_delta_stream
(&delta_stream, NULL, NULL, target_root, new_path,
pool));
SVN_ERR(svn_txdelta_send_txstream(delta_stream,
delta_handler,
delta_handler_baton,
pool));
SVN_ERR(svn_fs_file_md5_checksum(digest,
target_root,
new_path,
pool));
SVN_ERR(editor->close_file(file_baton,
svn_md5_digest_to_cstring(digest, pool),
pool));
} else
abort();
}
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
static svn_boolean_t
is_within_base_path(const char *path, const char *base_path, int base_len) {
if (base_path[0] == '\0')
return TRUE;
if (strncmp(base_path, path, base_len) == 0
&& (path[base_len] == '/' || path[base_len] == '\0'))
return TRUE;
return FALSE;
}
static svn_error_t *
path_driver_cb_func(void **dir_baton,
void *parent_baton,
void *callback_baton,
const char *path,
apr_pool_t *pool) {
struct path_driver_cb_baton *cb = callback_baton;
const svn_delta_editor_t *editor = cb->editor;
void *edit_baton = cb->edit_baton;
svn_fs_root_t *root = cb->root;
svn_fs_path_change_t *change;
svn_boolean_t do_add = FALSE, do_delete = FALSE;
svn_node_kind_t kind;
void *file_baton = NULL;
const char *copyfrom_path = NULL;
const char *real_copyfrom_path = NULL;
svn_revnum_t copyfrom_rev;
svn_boolean_t src_readable = TRUE;
svn_fs_root_t *source_root = cb->compare_root;
const char *source_path = source_root ? path : NULL;
const char *base_path = cb->base_path;
int base_path_len = cb->base_path_len;
*dir_baton = NULL;
while (cb->copies->nelts > 0
&& ! svn_path_is_ancestor(APR_ARRAY_IDX(cb->copies,
cb->copies->nelts - 1,
struct copy_info).path,
path))
cb->copies->nelts--;
change = apr_hash_get(cb->changed_paths, path, APR_HASH_KEY_STRING);
if (! change) {
return SVN_NO_ERROR;
}
switch (change->change_kind) {
case svn_fs_path_change_add:
do_add = TRUE;
break;
case svn_fs_path_change_delete:
do_delete = TRUE;
break;
case svn_fs_path_change_replace:
do_add = TRUE;
do_delete = TRUE;
break;
case svn_fs_path_change_modify:
default:
break;
}
if (do_delete)
SVN_ERR(editor->delete_entry(path, SVN_INVALID_REVNUM,
parent_baton, pool));
if (! do_delete || do_add) {
SVN_ERR(svn_fs_check_path(&kind, root, path, pool));
if ((kind != svn_node_dir) && (kind != svn_node_file))
return svn_error_createf
(SVN_ERR_FS_NOT_FOUND, NULL,
_("Filesystem path '%s' is neither a file nor a directory"), path);
}
if (do_add) {
svn_fs_root_t *copyfrom_root = NULL;
SVN_ERR(svn_fs_copied_from(&copyfrom_rev, &copyfrom_path,
root, path, pool));
if (copyfrom_path && SVN_IS_VALID_REVNUM(copyfrom_rev)) {
SVN_ERR(svn_fs_revision_root(&copyfrom_root,
svn_fs_root_fs(root),
copyfrom_rev, pool));
if (cb->authz_read_func) {
SVN_ERR(cb->authz_read_func(&src_readable, copyfrom_root,
copyfrom_path,
cb->authz_read_baton, pool));
}
}
real_copyfrom_path = copyfrom_path;
if (copyfrom_path
&& (! src_readable
|| ! is_within_base_path(copyfrom_path + 1, base_path,
base_path_len)
|| cb->low_water_mark > copyfrom_rev)) {
copyfrom_path = NULL;
copyfrom_rev = SVN_INVALID_REVNUM;
}
if (kind == svn_node_dir) {
if (real_copyfrom_path && ! copyfrom_path) {
SVN_ERR(add_subdir(copyfrom_root, root, editor, edit_baton,
path, parent_baton, real_copyfrom_path,
cb->authz_read_func, cb->authz_read_baton,
cb->changed_paths, pool, dir_baton));
} else {
SVN_ERR(editor->add_directory(path, parent_baton,
copyfrom_path, copyfrom_rev,
pool, dir_baton));
}
} else {
SVN_ERR(editor->add_file(path, parent_baton, copyfrom_path,
copyfrom_rev, pool, &file_baton));
}
if (copyfrom_path) {
if (kind == svn_node_dir) {
struct copy_info *info = &APR_ARRAY_PUSH(cb->copies,
struct copy_info);
info->path = apr_pstrdup(cb->pool, path);
info->copyfrom_path = apr_pstrdup(cb->pool, copyfrom_path);
info->copyfrom_rev = copyfrom_rev;
}
source_root = copyfrom_root;
source_path = copyfrom_path;
} else
{
if (kind == svn_node_dir && cb->copies->nelts > 0) {
struct copy_info *info = &APR_ARRAY_PUSH(cb->copies,
struct copy_info);
info->path = apr_pstrdup(cb->pool, path);
info->copyfrom_path = NULL;
info->copyfrom_rev = SVN_INVALID_REVNUM;
}
source_root = NULL;
source_path = NULL;
}
} else if (! do_delete) {
if (kind == svn_node_dir) {
if (parent_baton) {
SVN_ERR(editor->open_directory(path, parent_baton,
SVN_INVALID_REVNUM,
pool, dir_baton));
} else {
SVN_ERR(editor->open_root(edit_baton, SVN_INVALID_REVNUM,
pool, dir_baton));
}
} else {
SVN_ERR(editor->open_file(path, parent_baton, SVN_INVALID_REVNUM,
pool, &file_baton));
}
if (cb->copies->nelts > 0) {
struct copy_info *info = &APR_ARRAY_IDX(cb->copies,
cb->copies->nelts - 1,
struct copy_info);
if (info->copyfrom_path) {
SVN_ERR(svn_fs_revision_root(&source_root,
svn_fs_root_fs(root),
info->copyfrom_rev, pool));
source_path = svn_path_join(info->copyfrom_path,
svn_path_is_child(info->path, path,
pool), pool);
} else {
source_root = NULL;
source_path = NULL;
}
}
}
if (! do_delete || do_add) {
if (change->prop_mod) {
if (cb->compare_root) {
apr_array_header_t *prop_diffs;
apr_hash_t *old_props;
apr_hash_t *new_props;
int i;
if (source_root)
SVN_ERR(svn_fs_node_proplist
(&old_props, source_root, source_path, pool));
else
old_props = apr_hash_make(pool);
SVN_ERR(svn_fs_node_proplist(&new_props, root, path, pool));
SVN_ERR(svn_prop_diffs(&prop_diffs, new_props, old_props,
pool));
for (i = 0; i < prop_diffs->nelts; ++i) {
svn_prop_t *pc = &APR_ARRAY_IDX(prop_diffs, i, svn_prop_t);
if (kind == svn_node_dir)
SVN_ERR(editor->change_dir_prop(*dir_baton, pc->name,
pc->value, pool));
else if (kind == svn_node_file)
SVN_ERR(editor->change_file_prop(file_baton, pc->name,
pc->value, pool));
}
} else {
if (kind == svn_node_dir)
SVN_ERR(editor->change_dir_prop(*dir_baton, "", NULL,
pool));
else if (kind == svn_node_file)
SVN_ERR(editor->change_file_prop(file_baton, "", NULL,
pool));
}
}
if (kind == svn_node_file
&& (change->text_mod || (real_copyfrom_path && ! copyfrom_path))) {
svn_txdelta_window_handler_t delta_handler;
void *delta_handler_baton;
const char *checksum = NULL;
if (cb->compare_root && source_root && source_path) {
unsigned char digest[APR_MD5_DIGESTSIZE];
SVN_ERR(svn_fs_file_md5_checksum(digest,
source_root,
source_path,
pool));
checksum = svn_md5_digest_to_cstring(digest, pool);
}
SVN_ERR(editor->apply_textdelta(file_baton, checksum, pool,
&delta_handler,
&delta_handler_baton));
if (cb->compare_root) {
svn_txdelta_stream_t *delta_stream;
SVN_ERR(svn_fs_get_file_delta_stream
(&delta_stream, source_root, source_path,
root, path, pool));
SVN_ERR(svn_txdelta_send_txstream(delta_stream,
delta_handler,
delta_handler_baton,
pool));
} else
SVN_ERR(delta_handler(NULL, delta_handler_baton));
}
}
if (file_baton) {
unsigned char digest[APR_MD5_DIGESTSIZE];
SVN_ERR(svn_fs_file_md5_checksum(digest, root, path, pool));
SVN_ERR(editor->close_file(file_baton,
svn_md5_digest_to_cstring(digest, pool),
pool));
}
return SVN_NO_ERROR;
}
svn_error_t *
svn_repos_replay2(svn_fs_root_t *root,
const char *base_path,
svn_revnum_t low_water_mark,
svn_boolean_t send_deltas,
const svn_delta_editor_t *editor,
void *edit_baton,
svn_repos_authz_func_t authz_read_func,
void *authz_read_baton,
apr_pool_t *pool) {
apr_hash_t *fs_changes;
apr_hash_t *changed_paths;
apr_hash_index_t *hi;
apr_array_header_t *paths;
struct path_driver_cb_baton cb_baton;
int base_path_len;
SVN_ERR(svn_fs_paths_changed(&fs_changes, root, pool));
if (! base_path)
base_path = "";
else if (base_path[0] == '/')
++base_path;
base_path_len = strlen(base_path);
paths = apr_array_make(pool, apr_hash_count(fs_changes),
sizeof(const char *));
changed_paths = apr_hash_make(pool);
for (hi = apr_hash_first(pool, fs_changes); hi; hi = apr_hash_next(hi)) {
const void *key;
void *val;
apr_ssize_t keylen;
const char *path;
svn_fs_path_change_t *change;
svn_boolean_t allowed = TRUE;
apr_hash_this(hi, &key, &keylen, &val);
path = key;
change = val;
if (authz_read_func)
SVN_ERR(authz_read_func(&allowed, root, path, authz_read_baton,
pool));
if (allowed) {
if (path[0] == '/') {
path++;
keylen--;
}
if (is_within_base_path(path, base_path, base_path_len)) {
APR_ARRAY_PUSH(paths, const char *) = path;
apr_hash_set(changed_paths, path, keylen, change);
}
else if (is_within_base_path(base_path, path, keylen)) {
APR_ARRAY_PUSH(paths, const char *) = path;
apr_hash_set(changed_paths, path, keylen, change);
}
}
}
if (! SVN_IS_VALID_REVNUM(low_water_mark))
low_water_mark = 0;
cb_baton.editor = editor;
cb_baton.edit_baton = edit_baton;
cb_baton.root = root;
cb_baton.changed_paths = changed_paths;
cb_baton.authz_read_func = authz_read_func;
cb_baton.authz_read_baton = authz_read_baton;
cb_baton.base_path = base_path;
cb_baton.base_path_len = base_path_len;
cb_baton.low_water_mark = low_water_mark;
cb_baton.compare_root = NULL;
if (send_deltas) {
SVN_ERR(svn_fs_revision_root(&cb_baton.compare_root,
svn_fs_root_fs(root),
svn_fs_is_revision_root(root)
? svn_fs_revision_root_revision(root) - 1
: svn_fs_txn_root_base_revision(root),
pool));
}
cb_baton.copies = apr_array_make(pool, 4, sizeof(struct copy_info));
cb_baton.pool = pool;
if (svn_fs_is_revision_root(root)) {
svn_revnum_t revision = svn_fs_revision_root_revision(root);
SVN_ERR(editor->set_target_revision(edit_baton, revision, pool));
}
SVN_ERR(svn_delta_path_driver(editor, edit_baton,
SVN_INVALID_REVNUM, paths,
path_driver_cb_func, &cb_baton, pool));
return SVN_NO_ERROR;
}
svn_error_t *
svn_repos_replay(svn_fs_root_t *root,
const svn_delta_editor_t *editor,
void *edit_baton,
apr_pool_t *pool) {
return svn_repos_replay2(root,
"" ,
SVN_INVALID_REVNUM,
FALSE ,
editor, edit_baton,
NULL ,
NULL ,
pool);
}