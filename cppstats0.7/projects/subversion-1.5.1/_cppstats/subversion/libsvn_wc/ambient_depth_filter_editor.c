#include "svn_delta.h"
#include "svn_wc.h"
#include "svn_path.h"
#include "wc.h"
struct edit_baton {
const svn_delta_editor_t *wrapped_editor;
void *wrapped_edit_baton;
const char *anchor;
const char *target;
svn_wc_adm_access_t *adm_access;
};
struct file_baton {
svn_boolean_t ambiently_excluded;
struct edit_baton *edit_baton;
void *wrapped_baton;
};
struct dir_baton {
svn_boolean_t ambiently_excluded;
svn_depth_t ambient_depth;
struct edit_baton *edit_baton;
const char *path;
void *wrapped_baton;
};
static svn_error_t *
make_dir_baton(struct dir_baton **d_p,
const char *path,
struct edit_baton *eb,
struct dir_baton *pb,
apr_pool_t *pool) {
struct dir_baton *d;
if (pb && (! path))
abort();
if (pb && pb->ambiently_excluded) {
*d_p = pb;
return SVN_NO_ERROR;
}
d = apr_pcalloc(pool, sizeof(*d));
d->path = apr_pstrdup(pool, eb->anchor);
if (path)
d->path = svn_path_join(d->path, path, pool);
if (pb
&& (pb->ambient_depth == svn_depth_empty
|| pb->ambient_depth == svn_depth_files)) {
const svn_wc_entry_t *entry;
SVN_ERR(svn_wc_entry(&entry, d->path, eb->adm_access, FALSE, pool));
if (! entry) {
d->ambiently_excluded = TRUE;
*d_p = d;
return SVN_NO_ERROR;
}
}
d->edit_baton = eb;
d->ambient_depth = svn_depth_unknown;
*d_p = d;
return SVN_NO_ERROR;
}
static svn_error_t *
make_file_baton(struct file_baton **f_p,
struct dir_baton *pb,
const char *path,
apr_pool_t *pool) {
struct file_baton *f = apr_pcalloc(pool, sizeof(*f));
if (! path)
abort();
if (pb->ambiently_excluded) {
f->ambiently_excluded = TRUE;
*f_p = f;
return SVN_NO_ERROR;
}
if (pb->ambient_depth == svn_depth_empty) {
const svn_wc_entry_t *entry;
SVN_ERR(svn_wc_entry(&entry,
svn_path_join(pb->edit_baton->anchor, path, pool),
pb->edit_baton->adm_access, FALSE, pool));
if (! entry) {
f->ambiently_excluded = TRUE;
*f_p = f;
return SVN_NO_ERROR;
}
}
f->edit_baton = pb->edit_baton;
*f_p = f;
return SVN_NO_ERROR;
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
struct dir_baton *b;
SVN_ERR(make_dir_baton(&b, NULL, eb, NULL, pool));
*root_baton = b;
if (b->ambiently_excluded)
return SVN_NO_ERROR;
if (! *eb->target) {
const svn_wc_entry_t *entry;
SVN_ERR(svn_wc_entry(&entry, b->path, eb->adm_access,
FALSE, pool));
if (entry)
b->ambient_depth = entry->depth;
}
return eb->wrapped_editor->open_root(eb->wrapped_edit_baton, base_revision,
pool, &b->wrapped_baton);
}
static svn_error_t *
delete_entry(const char *path,
svn_revnum_t base_revision,
void *parent_baton,
apr_pool_t *pool) {
struct dir_baton *pb = parent_baton;
struct edit_baton *eb = pb->edit_baton;
if (pb->ambiently_excluded)
return SVN_NO_ERROR;
if (pb->ambient_depth < svn_depth_immediates) {
const svn_wc_entry_t *entry;
const char *full_path = svn_path_join(eb->anchor, path,
pool);
SVN_ERR(svn_wc_entry(&entry, full_path,
eb->adm_access, FALSE, pool));
if (! entry)
return SVN_NO_ERROR;
}
return eb->wrapped_editor->delete_entry(path, base_revision,
pb->wrapped_baton, pool);
}
static svn_error_t *
add_directory(const char *path,
void *parent_baton,
const char *copyfrom_path,
svn_revnum_t copyfrom_revision,
apr_pool_t *pool,
void **child_baton) {
struct dir_baton *pb = parent_baton;
struct edit_baton *eb = pb->edit_baton;
struct dir_baton *b = NULL;
SVN_ERR(make_dir_baton(&b, path, eb, pb, pool));
*child_baton = b;
if (b->ambiently_excluded)
return SVN_NO_ERROR;
if (strcmp(eb->target, path) == 0) {
b->ambient_depth = svn_depth_infinity;
} else if (pb->ambient_depth == svn_depth_immediates) {
b->ambient_depth = svn_depth_empty;
} else {
b->ambient_depth = svn_depth_infinity;
}
SVN_ERR(eb->wrapped_editor->add_directory(path, pb->wrapped_baton,
copyfrom_path,
copyfrom_revision,
pool, &b->wrapped_baton));
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
struct dir_baton *b;
const svn_wc_entry_t *entry;
SVN_ERR(make_dir_baton(&b, path, eb, pb, pool));
*child_baton = b;
if (b->ambiently_excluded)
return SVN_NO_ERROR;
SVN_ERR(eb->wrapped_editor->open_directory(path, pb->wrapped_baton,
base_revision, pool,
&b->wrapped_baton));
SVN_ERR(svn_wc_entry(&entry, b->path, eb->adm_access, FALSE, pool));
if (entry)
b->ambient_depth = entry->depth;
return SVN_NO_ERROR;
}
static svn_error_t *
add_file(const char *path,
void *parent_baton,
const char *copyfrom_path,
svn_revnum_t copyfrom_revision,
apr_pool_t *pool,
void **child_baton) {
struct dir_baton *pb = parent_baton;
struct edit_baton *eb = pb->edit_baton;
struct file_baton *b = NULL;
SVN_ERR(make_file_baton(&b, pb, path, pool));
*child_baton = b;
if (b->ambiently_excluded)
return SVN_NO_ERROR;
SVN_ERR(eb->wrapped_editor->add_file(path, pb->wrapped_baton,
copyfrom_path, copyfrom_revision,
pool, &b->wrapped_baton));
return SVN_NO_ERROR;
}
static svn_error_t *
open_file(const char *path,
void *parent_baton,
svn_revnum_t base_revision,
apr_pool_t *pool,
void **child_baton) {
struct dir_baton *pb = parent_baton;
struct edit_baton *eb = pb->edit_baton;
struct file_baton *b;
SVN_ERR(make_file_baton(&b, pb, path, pool));
*child_baton = b;
if (b->ambiently_excluded)
return SVN_NO_ERROR;
SVN_ERR(eb->wrapped_editor->open_file(path, pb->wrapped_baton,
base_revision, pool,
&b->wrapped_baton));
return SVN_NO_ERROR;
}
static svn_error_t *
apply_textdelta(void *file_baton,
const char *base_checksum,
apr_pool_t *pool,
svn_txdelta_window_handler_t *handler,
void **handler_baton) {
struct file_baton *fb = file_baton;
struct edit_baton *eb = fb->edit_baton;
if (fb->ambiently_excluded) {
*handler = svn_delta_noop_window_handler;
*handler_baton = NULL;
return SVN_NO_ERROR;
}
return eb->wrapped_editor->apply_textdelta(fb->wrapped_baton,
base_checksum, pool,
handler, handler_baton);;
}
static svn_error_t *
close_file(void *file_baton,
const char *text_checksum,
apr_pool_t *pool) {
struct file_baton *fb = file_baton;
struct edit_baton *eb = fb->edit_baton;
if (fb->ambiently_excluded)
return SVN_NO_ERROR;
return eb->wrapped_editor->close_file(fb->wrapped_baton,
text_checksum, pool);
}
static svn_error_t *
absent_file(const char *path,
void *parent_baton,
apr_pool_t *pool) {
struct dir_baton *pb = parent_baton;
struct edit_baton *eb = pb->edit_baton;
if (pb->ambiently_excluded)
return SVN_NO_ERROR;
return eb->wrapped_editor->absent_file(path, pb->wrapped_baton, pool);
}
static svn_error_t *
close_directory(void *dir_baton,
apr_pool_t *pool) {
struct dir_baton *db = dir_baton;
struct edit_baton *eb = db->edit_baton;
if (db->ambiently_excluded)
return SVN_NO_ERROR;
return eb->wrapped_editor->close_directory(db->wrapped_baton, pool);
}
static svn_error_t *
absent_directory(const char *path,
void *parent_baton,
apr_pool_t *pool) {
struct dir_baton *pb = parent_baton;
struct edit_baton *eb = pb->edit_baton;
if (pb->ambiently_excluded)
return SVN_NO_ERROR;
return eb->wrapped_editor->absent_directory(path, pb->wrapped_baton, pool);
}
static svn_error_t *
change_file_prop(void *file_baton,
const char *name,
const svn_string_t *value,
apr_pool_t *pool) {
struct file_baton *fb = file_baton;
struct edit_baton *eb = fb->edit_baton;
if (fb->ambiently_excluded)
return SVN_NO_ERROR;
return eb->wrapped_editor->change_file_prop(fb->wrapped_baton,
name, value, pool);
}
static svn_error_t *
change_dir_prop(void *dir_baton,
const char *name,
const svn_string_t *value,
apr_pool_t *pool) {
struct dir_baton *db = dir_baton;
struct edit_baton *eb = db->edit_baton;
if (db->ambiently_excluded)
return SVN_NO_ERROR;
return eb->wrapped_editor->change_dir_prop(db->wrapped_baton,
name, value, pool);
}
static svn_error_t *
close_edit(void *edit_baton,
apr_pool_t *pool) {
struct edit_baton *eb = edit_baton;
return eb->wrapped_editor->close_edit(eb->wrapped_edit_baton, pool);
}
svn_error_t *
svn_wc__ambient_depth_filter_editor(const svn_delta_editor_t **editor,
void **edit_baton,
const svn_delta_editor_t *wrapped_editor,
void *wrapped_edit_baton,
const char *anchor,
const char *target,
svn_wc_adm_access_t *adm_access,
apr_pool_t *pool) {
svn_delta_editor_t *depth_filter_editor;
struct edit_baton *eb;
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
eb->anchor = anchor;
eb->target = target;
eb->adm_access = adm_access;
*editor = depth_filter_editor;
*edit_baton = eb;
return SVN_NO_ERROR;
}
