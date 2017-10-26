#include <string.h>
#include <apr_pools.h>
#include <apr_file_io.h>
#include <apr_md5.h>
#include "svn_compat.h"
#include "svn_pools.h"
#include "svn_error.h"
#include "svn_path.h"
#include "svn_delta.h"
#include "svn_fs.h"
#include "svn_repos.h"
#include "svn_md5.h"
#include "svn_props.h"
#include "repos.h"
#include "svn_private_config.h"
struct edit_baton {
apr_pool_t *pool;
apr_hash_t *revprop_table;
svn_commit_callback2_t commit_callback;
void *commit_callback_baton;
svn_repos_authz_callback_t authz_callback;
void *authz_baton;
svn_repos_t *repos;
const char *repos_url;
const char *repos_name;
svn_fs_t *fs;
const char *base_path;
svn_boolean_t txn_owner;
svn_fs_txn_t *txn;
const char *txn_name;
svn_fs_root_t *txn_root;
svn_revnum_t *new_rev;
const char **committed_date;
const char **committed_author;
};
struct dir_baton {
struct edit_baton *edit_baton;
struct dir_baton *parent;
const char *path;
svn_revnum_t base_rev;
svn_boolean_t was_copied;
apr_pool_t *pool;
};
struct file_baton {
struct edit_baton *edit_baton;
const char *path;
};
static svn_error_t *
out_of_date(const char *path, svn_node_kind_t kind) {
return svn_error_createf(SVN_ERR_FS_TXN_OUT_OF_DATE, NULL,
(kind == svn_node_dir
? _("Directory '%s' is out of date")
: _("File '%s' is out of date")),
path);
}
static svn_error_t *
check_authz(struct edit_baton *editor_baton, const char *path,
svn_fs_root_t *root, svn_repos_authz_access_t required,
apr_pool_t *pool) {
if (editor_baton->authz_callback) {
svn_boolean_t allowed;
SVN_ERR(editor_baton->authz_callback(required, &allowed, root, path,
editor_baton->authz_baton, pool));
if (!allowed)
return svn_error_create(required & svn_authz_write ?
SVN_ERR_AUTHZ_UNWRITABLE :
SVN_ERR_AUTHZ_UNREADABLE,
NULL, "Access denied");
}
return SVN_NO_ERROR;
}
static svn_error_t *
open_root(void *edit_baton,
svn_revnum_t base_revision,
apr_pool_t *pool,
void **root_baton) {
struct dir_baton *dirb;
struct edit_baton *eb = edit_baton;
svn_revnum_t youngest;
SVN_ERR(svn_fs_youngest_rev(&youngest, eb->fs, eb->pool));
if (eb->txn_owner) {
SVN_ERR(svn_repos_fs_begin_txn_for_commit2(&(eb->txn),
eb->repos,
youngest,
eb->revprop_table,
eb->pool));
} else
{
apr_array_header_t *props = svn_prop_hash_to_array(eb->revprop_table,
pool);
SVN_ERR(svn_repos_fs_change_txn_props(eb->txn, props, pool));
}
SVN_ERR(svn_fs_txn_name(&(eb->txn_name), eb->txn, eb->pool));
SVN_ERR(svn_fs_txn_root(&(eb->txn_root), eb->txn, eb->pool));
dirb = apr_pcalloc(pool, sizeof(*dirb));
dirb->edit_baton = edit_baton;
dirb->parent = NULL;
dirb->pool = pool;
dirb->was_copied = FALSE;
dirb->path = apr_pstrdup(pool, eb->base_path);
dirb->base_rev = base_revision;
*root_baton = dirb;
return SVN_NO_ERROR;
}
static svn_error_t *
delete_entry(const char *path,
svn_revnum_t revision,
void *parent_baton,
apr_pool_t *pool) {
struct dir_baton *parent = parent_baton;
struct edit_baton *eb = parent->edit_baton;
svn_node_kind_t kind;
svn_revnum_t cr_rev;
svn_repos_authz_access_t required = svn_authz_write;
const char *full_path = svn_path_join(eb->base_path, path, pool);
SVN_ERR(svn_fs_check_path(&kind, eb->txn_root, full_path, pool));
if (kind == svn_node_dir)
required |= svn_authz_recursive;
SVN_ERR(check_authz(eb, full_path, eb->txn_root,
required, pool));
SVN_ERR(check_authz(eb, parent->path, eb->txn_root,
svn_authz_write, pool));
if (kind == svn_node_none)
return SVN_NO_ERROR;
SVN_ERR(svn_fs_node_created_rev(&cr_rev, eb->txn_root, full_path, pool));
if (SVN_IS_VALID_REVNUM(revision) && (revision < cr_rev))
return out_of_date(full_path, kind);
return svn_fs_delete(eb->txn_root, full_path, pool);
}
static struct dir_baton *
make_dir_baton(struct edit_baton *edit_baton,
struct dir_baton *parent_baton,
const char *full_path,
svn_boolean_t was_copied,
svn_revnum_t base_revision,
apr_pool_t *pool) {
struct dir_baton *db;
db = apr_pcalloc(pool, sizeof(*db));
db->edit_baton = edit_baton;
db->parent = parent_baton;
db->pool = pool;
db->path = full_path;
db->was_copied = was_copied;
db->base_rev = base_revision;
return db;
}
static svn_error_t *
add_directory(const char *path,
void *parent_baton,
const char *copy_path,
svn_revnum_t copy_revision,
apr_pool_t *pool,
void **child_baton) {
struct dir_baton *pb = parent_baton;
struct edit_baton *eb = pb->edit_baton;
const char *full_path = svn_path_join(eb->base_path, path, pool);
apr_pool_t *subpool = svn_pool_create(pool);
svn_boolean_t was_copied = FALSE;
if (copy_path && (! SVN_IS_VALID_REVNUM(copy_revision)))
return svn_error_createf
(SVN_ERR_FS_GENERAL, NULL,
_("Got source path but no source revision for '%s'"), full_path);
if (copy_path) {
const char *fs_path;
svn_fs_root_t *copy_root;
svn_node_kind_t kind;
int repos_url_len;
SVN_ERR(check_authz(eb, full_path, eb->txn_root,
svn_authz_write | svn_authz_recursive,
subpool));
SVN_ERR(check_authz(eb, pb->path, eb->txn_root,
svn_authz_write, subpool));
SVN_ERR(svn_fs_check_path(&kind, eb->txn_root, full_path, subpool));
if ((kind != svn_node_none) && (! pb->was_copied))
return out_of_date(full_path, kind);
copy_path = svn_path_uri_decode(copy_path, subpool);
repos_url_len = strlen(eb->repos_url);
if (strncmp(copy_path, eb->repos_url, repos_url_len) != 0)
return svn_error_createf
(SVN_ERR_FS_GENERAL, NULL,
_("Source url '%s' is from different repository"), copy_path);
fs_path = apr_pstrdup(subpool, copy_path + repos_url_len);
SVN_ERR(svn_fs_revision_root(&copy_root, eb->fs,
copy_revision, subpool));
SVN_ERR(check_authz(eb, fs_path, copy_root,
svn_authz_read | svn_authz_recursive,
subpool));
SVN_ERR(svn_fs_copy(copy_root, fs_path,
eb->txn_root, full_path, subpool));
was_copied = TRUE;
} else {
SVN_ERR(check_authz(eb, full_path, eb->txn_root,
svn_authz_write, subpool));
SVN_ERR(check_authz(eb, pb->path, eb->txn_root,
svn_authz_write, subpool));
SVN_ERR(svn_fs_make_dir(eb->txn_root, full_path, subpool));
}
svn_pool_destroy(subpool);
*child_baton = make_dir_baton(eb, pb, full_path, was_copied,
SVN_INVALID_REVNUM, pool);
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
svn_node_kind_t kind;
const char *full_path = svn_path_join(eb->base_path, path, pool);
SVN_ERR(svn_fs_check_path(&kind, eb->txn_root, full_path, pool));
if (kind == svn_node_none)
return svn_error_createf(SVN_ERR_FS_NOT_DIRECTORY, NULL,
_("Path '%s' not present"),
path);
*child_baton = make_dir_baton(eb, pb, full_path, pb->was_copied,
base_revision, pool);
return SVN_NO_ERROR;
}
static svn_error_t *
apply_textdelta(void *file_baton,
const char *base_checksum,
apr_pool_t *pool,
svn_txdelta_window_handler_t *handler,
void **handler_baton) {
struct file_baton *fb = file_baton;
SVN_ERR(check_authz(fb->edit_baton, fb->path,
fb->edit_baton->txn_root,
svn_authz_write, pool));
return svn_fs_apply_textdelta(handler, handler_baton,
fb->edit_baton->txn_root,
fb->path,
base_checksum,
NULL,
pool);
}
static svn_error_t *
add_file(const char *path,
void *parent_baton,
const char *copy_path,
svn_revnum_t copy_revision,
apr_pool_t *pool,
void **file_baton) {
struct file_baton *new_fb;
struct dir_baton *pb = parent_baton;
struct edit_baton *eb = pb->edit_baton;
const char *full_path = svn_path_join(eb->base_path, path, pool);
apr_pool_t *subpool = svn_pool_create(pool);
if (copy_path && (! SVN_IS_VALID_REVNUM(copy_revision)))
return svn_error_createf
(SVN_ERR_FS_GENERAL, NULL,
_("Got source path but no source revision for '%s'"), full_path);
if (copy_path) {
const char *fs_path;
svn_fs_root_t *copy_root;
svn_node_kind_t kind;
int repos_url_len;
SVN_ERR(check_authz(eb, full_path, eb->txn_root,
svn_authz_write, subpool));
SVN_ERR(check_authz(eb, pb->path, eb->txn_root,
svn_authz_write, subpool));
SVN_ERR(svn_fs_check_path(&kind, eb->txn_root, full_path, subpool));
if ((kind != svn_node_none) && (! pb->was_copied))
return out_of_date(full_path, kind);
copy_path = svn_path_uri_decode(copy_path, subpool);
repos_url_len = strlen(eb->repos_url);
if (strncmp(copy_path, eb->repos_url, repos_url_len) != 0)
return svn_error_createf
(SVN_ERR_FS_GENERAL, NULL,
_("Source url '%s' is from different repository"), copy_path);
fs_path = apr_pstrdup(subpool, copy_path + repos_url_len);
SVN_ERR(svn_fs_revision_root(&copy_root, eb->fs,
copy_revision, subpool));
SVN_ERR(check_authz(eb, fs_path, copy_root,
svn_authz_read, subpool));
SVN_ERR(svn_fs_copy(copy_root, fs_path,
eb->txn_root, full_path, subpool));
} else {
SVN_ERR(check_authz(eb, full_path, eb->txn_root, svn_authz_write,
subpool));
SVN_ERR(check_authz(eb, pb->path, eb->txn_root, svn_authz_write,
subpool));
SVN_ERR(svn_fs_make_file(eb->txn_root, full_path, subpool));
}
svn_pool_destroy(subpool);
new_fb = apr_pcalloc(pool, sizeof(*new_fb));
new_fb->edit_baton = eb;
new_fb->path = full_path;
*file_baton = new_fb;
return SVN_NO_ERROR;
}
static svn_error_t *
open_file(const char *path,
void *parent_baton,
svn_revnum_t base_revision,
apr_pool_t *pool,
void **file_baton) {
struct file_baton *new_fb;
struct dir_baton *pb = parent_baton;
struct edit_baton *eb = pb->edit_baton;
svn_revnum_t cr_rev;
apr_pool_t *subpool = svn_pool_create(pool);
const char *full_path = svn_path_join(eb->base_path, path, pool);
SVN_ERR(check_authz(eb, full_path, eb->txn_root,
svn_authz_read, subpool));
SVN_ERR(svn_fs_node_created_rev(&cr_rev, eb->txn_root, full_path,
subpool));
if (SVN_IS_VALID_REVNUM(base_revision) && (base_revision < cr_rev))
return out_of_date(full_path, svn_node_file);
new_fb = apr_pcalloc(pool, sizeof(*new_fb));
new_fb->edit_baton = eb;
new_fb->path = full_path;
*file_baton = new_fb;
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
static svn_error_t *
change_file_prop(void *file_baton,
const char *name,
const svn_string_t *value,
apr_pool_t *pool) {
struct file_baton *fb = file_baton;
struct edit_baton *eb = fb->edit_baton;
SVN_ERR(check_authz(eb, fb->path, eb->txn_root,
svn_authz_write, pool));
return svn_repos_fs_change_node_prop(eb->txn_root, fb->path,
name, value, pool);
}
static svn_error_t *
close_file(void *file_baton,
const char *text_checksum,
apr_pool_t *pool) {
struct file_baton *fb = file_baton;
if (text_checksum) {
unsigned char digest[APR_MD5_DIGESTSIZE];
const char *hex_digest;
SVN_ERR(svn_fs_file_md5_checksum
(digest, fb->edit_baton->txn_root, fb->path, pool));
hex_digest = svn_md5_digest_to_cstring(digest, pool);
if (hex_digest && strcmp(text_checksum, hex_digest) != 0) {
return svn_error_createf
(SVN_ERR_CHECKSUM_MISMATCH, NULL,
_("Checksum mismatch for resulting fulltext\n"
"(%s):\n"
" expected checksum: %s\n"
" actual checksum: %s\n"),
fb->path, text_checksum, hex_digest);
}
}
return SVN_NO_ERROR;
}
static svn_error_t *
change_dir_prop(void *dir_baton,
const char *name,
const svn_string_t *value,
apr_pool_t *pool) {
struct dir_baton *db = dir_baton;
struct edit_baton *eb = db->edit_baton;
SVN_ERR(check_authz(eb, db->path, eb->txn_root,
svn_authz_write, pool));
if (SVN_IS_VALID_REVNUM(db->base_rev)) {
svn_revnum_t created_rev;
SVN_ERR(svn_fs_node_created_rev(&created_rev,
eb->txn_root, db->path, pool));
if (db->base_rev < created_rev)
return out_of_date(db->path, svn_node_dir);
}
return svn_repos_fs_change_node_prop(eb->txn_root, db->path,
name, value, pool);
}
static svn_error_t *
close_edit(void *edit_baton,
apr_pool_t *pool) {
struct edit_baton *eb = edit_baton;
svn_revnum_t new_revision = SVN_INVALID_REVNUM;
svn_error_t *err;
const char *conflict;
char *post_commit_err = NULL;
if (! eb->txn)
return svn_error_create(SVN_ERR_REPOS_BAD_ARGS, NULL,
"No valid transaction supplied to close_edit");
err = svn_repos_fs_commit_txn(&conflict, eb->repos,
&new_revision, eb->txn, pool);
if (err && (err->apr_err != SVN_ERR_REPOS_POST_COMMIT_HOOK_FAILED)) {
svn_error_clear(svn_fs_abort_txn(eb->txn, pool));
return err;
} else if (err) {
if (err->child && err->child->message)
post_commit_err = apr_pstrdup(pool, err->child->message) ;
svn_error_clear(err);
err = SVN_NO_ERROR;
}
{
svn_string_t *date, *author;
svn_error_t *err2;
svn_commit_info_t *commit_info;
err2 = svn_fs_revision_prop(&date, svn_repos_fs(eb->repos),
new_revision, SVN_PROP_REVISION_DATE,
pool);
if (! err2)
err2 = svn_fs_revision_prop(&author, svn_repos_fs(eb->repos),
new_revision, SVN_PROP_REVISION_AUTHOR,
pool);
if (! err2) {
commit_info = svn_create_commit_info(pool);
commit_info->revision = new_revision;
commit_info->date = date ? date->data : NULL;
commit_info->author = author ? author->data : NULL;
commit_info->post_commit_err = post_commit_err;
err2 = (*eb->commit_callback)(commit_info,
eb->commit_callback_baton,
pool);
if (err2) {
svn_error_clear(err);
return err2;
}
}
}
return err;
}
static svn_error_t *
abort_edit(void *edit_baton,
apr_pool_t *pool) {
struct edit_baton *eb = edit_baton;
if ((! eb->txn) || (! eb->txn_owner))
return SVN_NO_ERROR;
return svn_fs_abort_txn(eb->txn, pool);
}
static apr_hash_t *
revprop_table_dup(apr_hash_t *revprop_table,
apr_pool_t *pool) {
apr_hash_t *new_revprop_table = NULL;
const void *key;
apr_ssize_t klen;
void *value;
const char *propname;
const svn_string_t *propval;
apr_hash_index_t *hi;
new_revprop_table = apr_hash_make(pool);
for (hi = apr_hash_first(pool, revprop_table); hi; hi = apr_hash_next(hi)) {
apr_hash_this(hi, &key, &klen, &value);
propname = apr_pstrdup(pool, (const char *) key);
propval = svn_string_dup((const svn_string_t *) value, pool);
apr_hash_set(new_revprop_table, propname, klen, propval);
}
return new_revprop_table;
}
svn_error_t *
svn_repos_get_commit_editor5(const svn_delta_editor_t **editor,
void **edit_baton,
svn_repos_t *repos,
svn_fs_txn_t *txn,
const char *repos_url,
const char *base_path,
apr_hash_t *revprop_table,
svn_commit_callback2_t callback,
void *callback_baton,
svn_repos_authz_callback_t authz_callback,
void *authz_baton,
apr_pool_t *pool) {
svn_delta_editor_t *e;
apr_pool_t *subpool = svn_pool_create(pool);
struct edit_baton *eb;
if (authz_callback) {
svn_boolean_t allowed;
SVN_ERR(authz_callback(svn_authz_write, &allowed, NULL, NULL,
authz_baton, pool));
if (!allowed)
return svn_error_create(SVN_ERR_AUTHZ_UNWRITABLE, NULL,
"Not authorized to open a commit editor.");
}
e = svn_delta_default_editor(pool);
eb = apr_pcalloc(subpool, sizeof(*eb));
e->open_root = open_root;
e->delete_entry = delete_entry;
e->add_directory = add_directory;
e->open_directory = open_directory;
e->change_dir_prop = change_dir_prop;
e->add_file = add_file;
e->open_file = open_file;
e->close_file = close_file;
e->apply_textdelta = apply_textdelta;
e->change_file_prop = change_file_prop;
e->close_edit = close_edit;
e->abort_edit = abort_edit;
eb->pool = subpool;
eb->revprop_table = revprop_table_dup(revprop_table, subpool);
eb->commit_callback = callback;
eb->commit_callback_baton = callback_baton;
eb->authz_callback = authz_callback;
eb->authz_baton = authz_baton;
eb->base_path = apr_pstrdup(subpool, base_path);
eb->repos = repos;
eb->repos_url = repos_url;
eb->repos_name = svn_path_basename(svn_repos_path(repos, subpool),
subpool);
eb->fs = svn_repos_fs(repos);
eb->txn = txn;
eb->txn_owner = txn ? FALSE : TRUE;
*edit_baton = eb;
*editor = e;
return SVN_NO_ERROR;
}
svn_error_t *
svn_repos_get_commit_editor4(const svn_delta_editor_t **editor,
void **edit_baton,
svn_repos_t *repos,
svn_fs_txn_t *txn,
const char *repos_url,
const char *base_path,
const char *user,
const char *log_msg,
svn_commit_callback2_t callback,
void *callback_baton,
svn_repos_authz_callback_t authz_callback,
void *authz_baton,
apr_pool_t *pool) {
apr_hash_t *revprop_table = apr_hash_make(pool);
if (user)
apr_hash_set(revprop_table, SVN_PROP_REVISION_AUTHOR,
APR_HASH_KEY_STRING,
svn_string_create(user, pool));
if (log_msg)
apr_hash_set(revprop_table, SVN_PROP_REVISION_LOG,
APR_HASH_KEY_STRING,
svn_string_create(log_msg, pool));
return svn_repos_get_commit_editor5(editor, edit_baton, repos, txn,
repos_url, base_path, revprop_table,
callback, callback_baton,
authz_callback, authz_baton, pool);
}
svn_error_t *
svn_repos_get_commit_editor3(const svn_delta_editor_t **editor,
void **edit_baton,
svn_repos_t *repos,
svn_fs_txn_t *txn,
const char *repos_url,
const char *base_path,
const char *user,
const char *log_msg,
svn_commit_callback_t callback,
void *callback_baton,
svn_repos_authz_callback_t authz_callback,
void *authz_baton,
apr_pool_t *pool) {
svn_commit_callback2_t callback2;
void *callback2_baton;
svn_compat_wrap_commit_callback(&callback2, &callback2_baton,
callback, callback_baton,
pool);
return svn_repos_get_commit_editor4(editor, edit_baton, repos, txn,
repos_url, base_path, user,
log_msg, callback2,
callback2_baton, authz_callback,
authz_baton, pool);
}
svn_error_t *
svn_repos_get_commit_editor2(const svn_delta_editor_t **editor,
void **edit_baton,
svn_repos_t *repos,
svn_fs_txn_t *txn,
const char *repos_url,
const char *base_path,
const char *user,
const char *log_msg,
svn_commit_callback_t callback,
void *callback_baton,
apr_pool_t *pool) {
return svn_repos_get_commit_editor3(editor, edit_baton, repos, txn,
repos_url, base_path, user,
log_msg, callback, callback_baton,
NULL, NULL, pool);
}
svn_error_t *
svn_repos_get_commit_editor(const svn_delta_editor_t **editor,
void **edit_baton,
svn_repos_t *repos,
const char *repos_url,
const char *base_path,
const char *user,
const char *log_msg,
svn_commit_callback_t callback,
void *callback_baton,
apr_pool_t *pool) {
return svn_repos_get_commit_editor2(editor, edit_baton, repos, NULL,
repos_url, base_path, user,
log_msg, callback,
callback_baton, pool);
}
