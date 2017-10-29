#include <stdio.h>
#include <apr_pools.h>
#include <apr_file_io.h>
#include "svn_types.h"
#include "svn_error.h"
#include "svn_path.h"
#include "svn_delta.h"
#include "svn_fs.h"
#include "../svn_test.h"
#include "dir-delta-editor.h"
struct edit_baton {
svn_fs_t *fs;
svn_fs_root_t *txn_root;
const char *root_path;
apr_pool_t *pool;
};
struct dir_baton {
struct edit_baton *edit_baton;
const char *full_path;
};
struct file_baton {
struct edit_baton *edit_baton;
const char *path;
};
static svn_error_t *
test_delete_entry(const char *path,
svn_revnum_t revision,
void *parent_baton,
apr_pool_t *pool) {
struct dir_baton *pb = parent_baton;
return svn_fs_delete(pb->edit_baton->txn_root,
svn_path_join(pb->edit_baton->root_path, path, pool),
pool);
}
static svn_error_t *
test_open_root(void *edit_baton,
svn_revnum_t base_revision,
apr_pool_t *dir_pool,
void **root_baton) {
struct dir_baton *db = apr_pcalloc(dir_pool, sizeof(*db));
struct edit_baton *eb = edit_baton;
db->full_path = eb->root_path;
db->edit_baton = edit_baton;
*root_baton = db;
return SVN_NO_ERROR;
}
static svn_error_t *
test_open_directory(const char *path,
void *parent_baton,
svn_revnum_t base_revision,
apr_pool_t *dir_pool,
void **child_baton) {
struct dir_baton *pb = parent_baton;
struct edit_baton *eb = pb->edit_baton;
struct dir_baton *db = apr_pcalloc(dir_pool, sizeof(*db));
svn_fs_root_t *rev_root = NULL;
db->full_path = svn_path_join(eb->root_path, path, eb->pool);
db->edit_baton = eb;
SVN_ERR(svn_fs_revision_root(&rev_root, eb->fs, base_revision, dir_pool));
SVN_ERR(svn_fs_revision_link(rev_root, eb->txn_root, db->full_path,
dir_pool));
*child_baton = db;
return SVN_NO_ERROR;
}
static svn_error_t *
test_add_directory(const char *path,
void *parent_baton,
const char *copyfrom_path,
svn_revnum_t copyfrom_revision,
apr_pool_t *dir_pool,
void **child_baton) {
struct dir_baton *pb = parent_baton;
struct edit_baton *eb = pb->edit_baton;
struct dir_baton *db = apr_pcalloc(dir_pool, sizeof(*db));
db->full_path = svn_path_join(eb->root_path, path, eb->pool);
db->edit_baton = eb;
if (copyfrom_path) {
svn_fs_root_t *rev_root = NULL;
SVN_ERR(svn_fs_revision_root(&rev_root,
eb->fs,
copyfrom_revision,
dir_pool));
SVN_ERR(svn_fs_copy(rev_root,
copyfrom_path,
eb->txn_root,
db->full_path,
dir_pool));
} else
SVN_ERR(svn_fs_make_dir(eb->txn_root, db->full_path, dir_pool));
*child_baton = db;
return SVN_NO_ERROR;
}
static svn_error_t *
test_open_file(const char *path,
void *parent_baton,
svn_revnum_t base_revision,
apr_pool_t *file_pool,
void **file_baton) {
struct dir_baton *pb = parent_baton;
struct edit_baton *eb = pb->edit_baton;
struct file_baton *fb = apr_pcalloc(file_pool, sizeof(*fb));
svn_fs_root_t *rev_root = NULL;
fb->path = svn_path_join(eb->root_path, path, eb->pool);
fb->edit_baton = eb;
SVN_ERR(svn_fs_revision_root(&rev_root, eb->fs, base_revision, file_pool));
SVN_ERR(svn_fs_revision_link(rev_root, eb->txn_root, fb->path, file_pool));
*file_baton = fb;
return SVN_NO_ERROR;
}
static svn_error_t *
test_add_file(const char *path,
void *parent_baton,
const char *copyfrom_path,
svn_revnum_t copyfrom_revision,
apr_pool_t *file_pool,
void **file_baton) {
struct dir_baton *db = parent_baton;
struct edit_baton *eb = db->edit_baton;
struct file_baton *fb = apr_pcalloc(file_pool, sizeof(*fb));
fb->path = svn_path_join(eb->root_path, path, eb->pool);
fb->edit_baton = eb;
if (copyfrom_path) {
svn_fs_root_t *rev_root = NULL;
SVN_ERR(svn_fs_revision_root(&rev_root,
eb->fs,
copyfrom_revision,
file_pool));
SVN_ERR(svn_fs_copy(rev_root,
copyfrom_path,
eb->txn_root,
fb->path,
file_pool));
} else
SVN_ERR(svn_fs_make_file(eb->txn_root, fb->path, file_pool));
*file_baton = fb;
return SVN_NO_ERROR;
}
static svn_error_t *
test_apply_textdelta(void *file_baton,
const char *base_checksum,
apr_pool_t *pool,
svn_txdelta_window_handler_t *handler,
void **handler_baton) {
struct file_baton *fb = file_baton;
return svn_fs_apply_textdelta(handler, handler_baton,
fb->edit_baton->txn_root,
fb->path,
base_checksum,
NULL,
pool);
}
static svn_error_t *
test_change_file_prop(void *file_baton,
const char *name, const svn_string_t *value,
apr_pool_t *pool) {
struct file_baton *fb = file_baton;
return svn_fs_change_node_prop(fb->edit_baton->txn_root,
fb->path, name, value, pool);
}
static svn_error_t *
test_change_dir_prop(void *parent_baton,
const char *name, const svn_string_t *value,
apr_pool_t *pool) {
struct dir_baton *db = parent_baton;
struct edit_baton *eb = db->edit_baton;
return svn_fs_change_node_prop(eb->txn_root, db->full_path,
name, value, pool);
}
svn_error_t *
dir_delta_get_editor(const svn_delta_editor_t **editor,
void **edit_baton,
svn_fs_t *fs,
svn_fs_root_t *txn_root,
const char *path,
apr_pool_t *pool) {
svn_delta_editor_t *my_editor;
struct edit_baton *my_edit_baton;
my_editor = svn_delta_default_editor(pool);
my_editor->open_root = test_open_root;
my_editor->delete_entry = test_delete_entry;
my_editor->add_directory = test_add_directory;
my_editor->open_directory = test_open_directory;
my_editor->add_file = test_add_file;
my_editor->open_file = test_open_file;
my_editor->apply_textdelta = test_apply_textdelta;
my_editor->change_file_prop = test_change_file_prop;
my_editor->change_dir_prop = test_change_dir_prop;
my_edit_baton = apr_pcalloc(pool, sizeof(*my_edit_baton));
my_edit_baton->root_path = apr_pstrdup(pool, path);
my_edit_baton->pool = pool;
my_edit_baton->fs = fs;
my_edit_baton->txn_root = txn_root;
*editor = my_editor;
*edit_baton = my_edit_baton;
return SVN_NO_ERROR;
}