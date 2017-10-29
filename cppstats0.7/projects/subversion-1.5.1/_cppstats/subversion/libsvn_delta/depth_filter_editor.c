#include "svn_delta.h"
struct edit_baton {
const svn_delta_editor_t *wrapped_editor;
void *wrapped_edit_baton;
svn_depth_t requested_depth;
svn_boolean_t has_target;
};
struct node_baton {
svn_boolean_t filtered;
void *edit_baton;
void *wrapped_baton;
int dir_depth;
};
static struct node_baton *
make_node_baton(void *edit_baton,
svn_boolean_t filtered,
int dir_depth,
apr_pool_t *pool) {
struct node_baton *b = apr_palloc(pool, sizeof(*b));
b->edit_baton = edit_baton;
b->wrapped_baton = NULL;
b->filtered = filtered;
b->dir_depth = dir_depth;
return b;
}
static svn_boolean_t
okay_to_edit(struct edit_baton *eb,
struct node_baton *pb,
svn_node_kind_t kind) {
int effective_depth;
if (pb->filtered)
return FALSE;
effective_depth = pb->dir_depth - (eb->has_target ? 1 : 0);
switch (eb->requested_depth) {
case svn_depth_empty:
return (effective_depth <= 0);
case svn_depth_files:
return ((effective_depth <= 0)
|| (kind == svn_node_file && effective_depth == 1));
case svn_depth_immediates:
return (effective_depth <= 1);
case svn_depth_infinity:
return TRUE;
default:
break;
}
abort();
return FALSE;
}
static svn_error_t *
set_target_revision(void *edit_baton,
svn_revnum_t target_revision,
apr_pool_t *pool) {
struct edit_baton *eb = edit_baton;
return eb->wrapped_editor->set_target_revision(eb->wrapped_edit_baton,
target_revision, pool);
}
static svn_error_t *
open_root(void *edit_baton,
svn_revnum_t base_revision,
apr_pool_t *pool,
void **root_baton) {
struct edit_baton *eb = edit_baton;
struct node_baton *b;
b = make_node_baton(edit_baton, FALSE, 1, pool);
SVN_ERR(eb->wrapped_editor->open_root(eb->wrapped_edit_baton, base_revision,
pool, &b->wrapped_baton));
*root_baton = b;
return SVN_NO_ERROR;
}
static svn_error_t *
delete_entry(const char *path,
svn_revnum_t base_revision,
void *parent_baton,
apr_pool_t *pool) {
struct node_baton *pb = parent_baton;
struct edit_baton *eb = pb->edit_baton;
if (okay_to_edit(eb, pb, svn_node_file))
SVN_ERR(eb->wrapped_editor->delete_entry(path, base_revision,
pb->wrapped_baton, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
add_directory(const char *path,
void *parent_baton,
const char *copyfrom_path,
svn_revnum_t copyfrom_revision,
apr_pool_t *pool,
void **child_baton) {
struct node_baton *pb = parent_baton;
struct edit_baton *eb = pb->edit_baton;
struct node_baton *b = NULL;
if (okay_to_edit(eb, pb, svn_node_dir)) {
b = make_node_baton(eb, FALSE, pb->dir_depth + 1, pool);
SVN_ERR(eb->wrapped_editor->add_directory(path, pb->wrapped_baton,
copyfrom_path,
copyfrom_revision,
pool, &b->wrapped_baton));
} else {
b = make_node_baton(eb, TRUE, pb->dir_depth + 1, pool);
}
*child_baton = b;
return SVN_NO_ERROR;
}
static svn_error_t *
open_directory(const char *path,
void *parent_baton,
svn_revnum_t base_revision,
apr_pool_t *pool,
void **child_baton) {
struct node_baton *pb = parent_baton;
struct edit_baton *eb = pb->edit_baton;
struct node_baton *b;
if (okay_to_edit(eb, pb, svn_node_dir)) {
b = make_node_baton(eb, FALSE, pb->dir_depth + 1, pool);
SVN_ERR(eb->wrapped_editor->open_directory(path, pb->wrapped_baton,
base_revision, pool,
&b->wrapped_baton));
} else {
b = make_node_baton(eb, TRUE, pb->dir_depth + 1, pool);
}
*child_baton = b;
return SVN_NO_ERROR;
}
static svn_error_t *
add_file(const char *path,
void *parent_baton,
const char *copyfrom_path,
svn_revnum_t copyfrom_revision,
apr_pool_t *pool,
void **child_baton) {
struct node_baton *pb = parent_baton;
struct edit_baton *eb = pb->edit_baton;
struct node_baton *b = NULL;
if (okay_to_edit(eb, pb, svn_node_file)) {
b = make_node_baton(eb, FALSE, pb->dir_depth, pool);
SVN_ERR(eb->wrapped_editor->add_file(path, pb->wrapped_baton,
copyfrom_path, copyfrom_revision,
pool, &b->wrapped_baton));
} else {
b = make_node_baton(eb, TRUE, pb->dir_depth, pool);
}
*child_baton = b;
return SVN_NO_ERROR;
}
static svn_error_t *
open_file(const char *path,
void *parent_baton,
svn_revnum_t base_revision,
apr_pool_t *pool,
void **child_baton) {
struct node_baton *pb = parent_baton;
struct edit_baton *eb = pb->edit_baton;
struct node_baton *b;
if (okay_to_edit(eb, pb, svn_node_file)) {
b = make_node_baton(eb, FALSE, pb->dir_depth, pool);
SVN_ERR(eb->wrapped_editor->open_file(path, pb->wrapped_baton,
base_revision, pool,
&b->wrapped_baton));
} else {
b = make_node_baton(eb, TRUE, pb->dir_depth, pool);
}
*child_baton = b;
return SVN_NO_ERROR;
}
static svn_error_t *
apply_textdelta(void *file_baton,
const char *base_checksum,
apr_pool_t *pool,
svn_txdelta_window_handler_t *handler,
void **handler_baton) {
struct node_baton *fb = file_baton;
struct edit_baton *eb = fb->edit_baton;
if (fb->filtered) {
*handler = svn_delta_noop_window_handler;
*handler_baton = NULL;
} else {
SVN_ERR(eb->wrapped_editor->apply_textdelta(fb->wrapped_baton,
base_checksum, pool,
handler, handler_baton));
}
return SVN_NO_ERROR;
}
static svn_error_t *
close_file(void *file_baton,
const char *text_checksum,
apr_pool_t *pool) {
struct node_baton *fb = file_baton;
struct edit_baton *eb = fb->edit_baton;
if (! fb->filtered)
SVN_ERR(eb->wrapped_editor->close_file(fb->wrapped_baton,
text_checksum, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
absent_file(const char *path,
void *parent_baton,
apr_pool_t *pool) {
struct node_baton *pb = parent_baton;
struct edit_baton *eb = pb->edit_baton;
if (! pb->filtered)
SVN_ERR(eb->wrapped_editor->absent_file(path, pb->wrapped_baton, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
close_directory(void *dir_baton,
apr_pool_t *pool) {
struct node_baton *db = dir_baton;
struct edit_baton *eb = db->edit_baton;
if (! db->filtered)
SVN_ERR(eb->wrapped_editor->close_directory(db->wrapped_baton, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
absent_directory(const char *path,
void *parent_baton,
apr_pool_t *pool) {
struct node_baton *pb = parent_baton;
struct edit_baton *eb = pb->edit_baton;
if (! pb->filtered)
SVN_ERR(eb->wrapped_editor->absent_directory(path, pb->wrapped_baton,
pool));
return SVN_NO_ERROR;
}
static svn_error_t *
change_file_prop(void *file_baton,
const char *name,
const svn_string_t *value,
apr_pool_t *pool) {
struct node_baton *fb = file_baton;
struct edit_baton *eb = fb->edit_baton;
if (! fb->filtered)
SVN_ERR(eb->wrapped_editor->change_file_prop(fb->wrapped_baton,
name, value, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
change_dir_prop(void *dir_baton,
const char *name,
const svn_string_t *value,
apr_pool_t *pool) {
struct node_baton *db = dir_baton;
struct edit_baton *eb = db->edit_baton;
if (! db->filtered)
SVN_ERR(eb->wrapped_editor->change_dir_prop(db->wrapped_baton,
name, value, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
close_edit(void *edit_baton,
apr_pool_t *pool) {
struct edit_baton *eb = edit_baton;
return eb->wrapped_editor->close_edit(eb->wrapped_edit_baton, pool);
}
svn_error_t *
svn_delta_depth_filter_editor(const svn_delta_editor_t **editor,
void **edit_baton,
const svn_delta_editor_t *wrapped_editor,
void *wrapped_edit_baton,
svn_depth_t requested_depth,
svn_boolean_t has_target,
apr_pool_t *pool) {
svn_delta_editor_t *depth_filter_editor;
struct edit_baton *eb;
if ((requested_depth == svn_depth_unknown)
|| (requested_depth == svn_depth_infinity)) {
*editor = wrapped_editor;
*edit_baton = wrapped_edit_baton;
return SVN_NO_ERROR;
}
depth_filter_editor = svn_delta_default_editor(pool);
depth_filter_editor->set_target_revision = set_target_revision;
depth_filter_editor->open_root = open_root;
depth_filter_editor->delete_entry = delete_entry;
depth_filter_editor->add_directory = add_directory;
depth_filter_editor->open_directory = open_directory;
depth_filter_editor->change_dir_prop = change_dir_prop;
depth_filter_editor->close_directory = close_directory;
depth_filter_editor->absent_directory = absent_directory;
depth_filter_editor->add_file = add_file;
depth_filter_editor->open_file = open_file;
depth_filter_editor->apply_textdelta = apply_textdelta;
depth_filter_editor->change_file_prop = change_file_prop;
depth_filter_editor->close_file = close_file;
depth_filter_editor->absent_file = absent_file;
depth_filter_editor->close_edit = close_edit;
eb = apr_palloc(pool, sizeof(*eb));
eb->wrapped_editor = wrapped_editor;
eb->wrapped_edit_baton = wrapped_edit_baton;
eb->has_target = has_target;
eb->requested_depth = requested_depth;
*editor = depth_filter_editor;
*edit_baton = eb;
return SVN_NO_ERROR;
}