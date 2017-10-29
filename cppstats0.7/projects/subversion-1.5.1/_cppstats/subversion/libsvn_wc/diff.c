#include <assert.h>
#include <apr_hash.h>
#include "svn_pools.h"
#include "svn_path.h"
#include "svn_hash.h"
#include "private/svn_wc_private.h"
#include "wc.h"
#include "props.h"
#include "adm_files.h"
#include "svn_private_config.h"
static void
reverse_propchanges(apr_hash_t *baseprops,
apr_array_header_t *propchanges,
apr_pool_t *pool) {
int i;
for (i = 0; i < propchanges->nelts; i++) {
svn_prop_t *propchange
= &APR_ARRAY_IDX(propchanges, i, svn_prop_t);
const svn_string_t *original_value =
apr_hash_get(baseprops, propchange->name, APR_HASH_KEY_STRING);
if ((original_value == NULL) && (propchange->value != NULL)) {
apr_hash_set(baseprops, propchange->name, APR_HASH_KEY_STRING,
svn_string_dup(propchange->value, pool));
propchange->value = NULL;
}
else if ((original_value != NULL) && (propchange->value == NULL)) {
propchange->value = svn_string_dup(original_value, pool);
apr_hash_set(baseprops, propchange->name, APR_HASH_KEY_STRING,
NULL);
}
else if ((original_value != NULL) && (propchange->value != NULL)) {
const svn_string_t *str = svn_string_dup(propchange->value, pool);
propchange->value = svn_string_dup(original_value, pool);
apr_hash_set(baseprops, propchange->name, APR_HASH_KEY_STRING, str);
}
}
}
struct edit_baton {
svn_wc_adm_access_t *anchor;
const char *anchor_path;
const char *target;
svn_revnum_t revnum;
svn_boolean_t root_opened;
const svn_wc_diff_callbacks2_t *callbacks;
void *callback_baton;
svn_depth_t depth;
svn_boolean_t ignore_ancestry;
svn_boolean_t use_text_base;
svn_boolean_t reverse_order;
const char *empty_file;
apr_hash_t *changelist_hash;
apr_pool_t *pool;
};
struct dir_baton {
svn_boolean_t added;
svn_depth_t depth;
const char *path;
apr_hash_t *compared;
struct dir_baton *dir_baton;
apr_array_header_t *propchanges;
struct edit_baton *edit_baton;
apr_pool_t *pool;
};
struct file_baton {
svn_boolean_t added;
const char *path;
const char *wc_path;
apr_file_t *original_file;
apr_file_t *temp_file;
const char *temp_file_path;
apr_array_header_t *propchanges;
svn_txdelta_window_handler_t apply_handler;
void *apply_baton;
struct edit_baton *edit_baton;
apr_pool_t *pool;
};
struct callbacks_wrapper_baton {
const svn_wc_diff_callbacks_t *callbacks;
void *baton;
};
static svn_error_t *
make_editor_baton(struct edit_baton **edit_baton,
svn_wc_adm_access_t *anchor,
const char *target,
const svn_wc_diff_callbacks2_t *callbacks,
void *callback_baton,
svn_depth_t depth,
svn_boolean_t ignore_ancestry,
svn_boolean_t use_text_base,
svn_boolean_t reverse_order,
const apr_array_header_t *changelists,
apr_pool_t *pool) {
apr_hash_t *changelist_hash = NULL;
struct edit_baton *eb;
if (changelists && changelists->nelts)
SVN_ERR(svn_hash_from_cstring_keys(&changelist_hash, changelists, pool));
eb = apr_pcalloc(pool, sizeof(*eb));
eb->anchor = anchor;
eb->anchor_path = svn_wc_adm_access_path(anchor);
eb->target = apr_pstrdup(pool, target);
eb->callbacks = callbacks;
eb->callback_baton = callback_baton;
eb->depth = depth;
eb->ignore_ancestry = ignore_ancestry;
eb->use_text_base = use_text_base;
eb->reverse_order = reverse_order;
eb->changelist_hash = changelist_hash;
eb->pool = pool;
*edit_baton = eb;
return SVN_NO_ERROR;
}
static struct dir_baton *
make_dir_baton(const char *path,
struct dir_baton *parent_baton,
struct edit_baton *edit_baton,
svn_boolean_t added,
svn_depth_t depth,
apr_pool_t *pool) {
struct dir_baton *dir_baton = apr_pcalloc(pool, sizeof(*dir_baton));
dir_baton->dir_baton = parent_baton;
dir_baton->edit_baton = edit_baton;
dir_baton->added = added;
dir_baton->depth = depth;
dir_baton->pool = pool;
dir_baton->propchanges = apr_array_make(pool, 1, sizeof(svn_prop_t));
dir_baton->compared = apr_hash_make(dir_baton->pool);
dir_baton->path = path;
return dir_baton;
}
static struct file_baton *
make_file_baton(const char *path,
svn_boolean_t added,
struct dir_baton *parent_baton,
apr_pool_t *pool) {
struct file_baton *file_baton = apr_pcalloc(pool, sizeof(*file_baton));
struct edit_baton *edit_baton = parent_baton->edit_baton;
file_baton->edit_baton = edit_baton;
file_baton->added = added;
file_baton->pool = pool;
file_baton->propchanges = apr_array_make(pool, 1, sizeof(svn_prop_t));
file_baton->path = path;
if (parent_baton->added) {
struct dir_baton *wc_dir_baton = parent_baton;
while (wc_dir_baton->added)
wc_dir_baton = wc_dir_baton->dir_baton;
file_baton->wc_path = svn_path_join(wc_dir_baton->path, "unimportant",
file_baton->pool);
} else {
file_baton->wc_path = file_baton->path;
}
return file_baton;
}
static svn_error_t *
get_empty_file(struct edit_baton *b,
const char **empty_file) {
if (!b->empty_file) {
const char *temp_dir;
SVN_ERR(svn_io_temp_dir(&temp_dir, b->pool));
SVN_ERR(svn_io_open_unique_file2
(NULL, &(b->empty_file),
svn_path_join(temp_dir, "tmp", b->pool),
"", svn_io_file_del_on_pool_cleanup,
b->pool));
}
*empty_file = b->empty_file;
return SVN_NO_ERROR;
}
static const char *
get_prop_mimetype(apr_hash_t *props) {
const svn_string_t *mimetype_val;
mimetype_val = apr_hash_get(props,
SVN_PROP_MIME_TYPE,
strlen(SVN_PROP_MIME_TYPE));
return (mimetype_val) ? mimetype_val->data : NULL;
}
static svn_error_t *
get_base_mimetype(const char **mimetype,
apr_hash_t **baseprops,
svn_wc_adm_access_t *adm_access,
const char *path,
apr_pool_t *pool) {
apr_hash_t *props = NULL;
if (baseprops == NULL)
baseprops = &props;
if (*baseprops == NULL)
SVN_ERR(svn_wc_get_prop_diffs(NULL, baseprops, path, adm_access, pool));
*mimetype = get_prop_mimetype(*baseprops);
return SVN_NO_ERROR;
}
static svn_error_t *
get_working_mimetype(const char **mimetype,
apr_hash_t **workingprops,
svn_wc_adm_access_t *adm_access,
const char *path,
apr_pool_t *pool) {
apr_hash_t *props = NULL;
if (workingprops == NULL)
workingprops = &props;
if (*workingprops == NULL)
SVN_ERR(svn_wc_prop_list(workingprops, path, adm_access, pool));
*mimetype = get_prop_mimetype(*workingprops);
return SVN_NO_ERROR;
}
static apr_hash_t *
apply_propchanges(apr_hash_t *props,
apr_array_header_t *propchanges) {
apr_hash_t *newprops = apr_hash_copy(apr_hash_pool_get(props), props);
int i;
for (i = 0; i < propchanges->nelts; ++i) {
const svn_prop_t *prop = &APR_ARRAY_IDX(propchanges, i, svn_prop_t);
apr_hash_set(newprops, prop->name, APR_HASH_KEY_STRING, prop->value);
}
return newprops;
}
static svn_error_t *
file_diff(struct dir_baton *dir_baton,
const char *path,
const svn_wc_entry_t *entry,
apr_pool_t *pool) {
struct edit_baton *eb = dir_baton->edit_baton;
const char *textbase, *empty_file;
svn_boolean_t modified;
enum svn_wc_schedule_t schedule = entry->schedule;
svn_boolean_t copied = entry->copied;
svn_wc_adm_access_t *adm_access;
const char *base_mimetype, *working_mimetype;
const char *translated = NULL;
apr_array_header_t *propchanges = NULL;
apr_hash_t *baseprops = NULL;
assert(! eb->use_text_base);
SVN_ERR(svn_wc_adm_retrieve(&adm_access, dir_baton->edit_baton->anchor,
dir_baton->path, pool));
if (! SVN_WC__CL_MATCH(dir_baton->edit_baton->changelist_hash, entry))
return SVN_NO_ERROR;
if (copied)
schedule = svn_wc_schedule_normal;
if (eb->ignore_ancestry && (schedule == svn_wc_schedule_replace))
schedule = svn_wc_schedule_normal;
textbase = svn_wc__text_base_path(path, FALSE, pool);
SVN_ERR(get_empty_file(eb, &empty_file));
if (schedule != svn_wc_schedule_delete) {
SVN_ERR(svn_wc_props_modified_p(&modified, path, adm_access, pool));
if (modified)
SVN_ERR(svn_wc_get_prop_diffs(&propchanges, &baseprops, path,
adm_access, pool));
else
propchanges = apr_array_make(pool, 1, sizeof(svn_prop_t));
} else {
SVN_ERR(svn_wc_get_prop_diffs(NULL, &baseprops, path,
adm_access, pool));
}
switch (schedule) {
case svn_wc_schedule_replace:
case svn_wc_schedule_delete:
SVN_ERR(get_base_mimetype(&base_mimetype, &baseprops,
adm_access, path, pool));
SVN_ERR(dir_baton->edit_baton->callbacks->file_deleted
(NULL, NULL, path,
textbase,
empty_file,
base_mimetype,
NULL,
baseprops,
dir_baton->edit_baton->callback_baton));
if (schedule == svn_wc_schedule_delete)
break;
case svn_wc_schedule_add:
SVN_ERR(get_working_mimetype(&working_mimetype, NULL,
adm_access, path, pool));
SVN_ERR(svn_wc_translated_file2
(&translated, path, path, adm_access,
SVN_WC_TRANSLATE_TO_NF
| SVN_WC_TRANSLATE_USE_GLOBAL_TMP,
pool));
SVN_ERR(dir_baton->edit_baton->callbacks->file_added
(NULL, NULL, NULL, path,
empty_file,
translated,
0, entry->revision,
NULL,
working_mimetype,
propchanges, baseprops,
dir_baton->edit_baton->callback_baton));
break;
default:
SVN_ERR(svn_wc_text_modified_p(&modified, path, FALSE,
adm_access, pool));
if (modified) {
SVN_ERR(svn_wc_translated_file2
(&translated, path,
path, adm_access,
SVN_WC_TRANSLATE_TO_NF
| SVN_WC_TRANSLATE_USE_GLOBAL_TMP,
pool));
}
if (modified || propchanges->nelts > 0) {
SVN_ERR(get_base_mimetype(&base_mimetype, &baseprops,
adm_access, path, pool));
SVN_ERR(get_working_mimetype(&working_mimetype, NULL,
adm_access, path, pool));
SVN_ERR(dir_baton->edit_baton->callbacks->file_changed
(NULL, NULL, NULL,
path,
modified ? textbase : NULL,
translated,
entry->revision,
SVN_INVALID_REVNUM,
base_mimetype,
working_mimetype,
propchanges, baseprops,
dir_baton->edit_baton->callback_baton));
}
}
return SVN_NO_ERROR;
}
static svn_error_t *
directory_elements_diff(struct dir_baton *dir_baton) {
apr_hash_t *entries;
apr_hash_index_t *hi;
const svn_wc_entry_t *this_dir_entry;
svn_boolean_t in_anchor_not_target;
apr_pool_t *subpool;
svn_wc_adm_access_t *adm_access;
assert(!dir_baton->added);
if (dir_baton->edit_baton->use_text_base)
return SVN_NO_ERROR;
in_anchor_not_target =
(*dir_baton->edit_baton->target
&& (! svn_path_compare_paths
(dir_baton->path,
svn_wc_adm_access_path(dir_baton->edit_baton->anchor))));
SVN_ERR(svn_wc_adm_retrieve(&adm_access, dir_baton->edit_baton->anchor,
dir_baton->path, dir_baton->pool));
SVN_ERR(svn_wc_entries_read(&entries, adm_access, FALSE, dir_baton->pool));
this_dir_entry = apr_hash_get(entries, SVN_WC_ENTRY_THIS_DIR,
APR_HASH_KEY_STRING);
if (SVN_WC__CL_MATCH(dir_baton->edit_baton->changelist_hash, this_dir_entry)
&& (! in_anchor_not_target)
&& (! apr_hash_get(dir_baton->compared, "", 0))) {
svn_boolean_t modified;
SVN_ERR(svn_wc_props_modified_p(&modified,
dir_baton->path, adm_access,
dir_baton->pool));
if (modified) {
apr_array_header_t *propchanges;
apr_hash_t *baseprops;
SVN_ERR(svn_wc_get_prop_diffs(&propchanges, &baseprops,
dir_baton->path, adm_access,
dir_baton->pool));
SVN_ERR(dir_baton->edit_baton->callbacks->dir_props_changed
(adm_access, NULL,
dir_baton->path,
propchanges, baseprops,
dir_baton->edit_baton->callback_baton));
}
}
if (dir_baton->depth == svn_depth_empty && !in_anchor_not_target)
return SVN_NO_ERROR;
subpool = svn_pool_create(dir_baton->pool);
for (hi = apr_hash_first(dir_baton->pool, entries); hi;
hi = apr_hash_next(hi)) {
const void *key;
void *val;
const svn_wc_entry_t *entry;
struct dir_baton *subdir_baton;
const char *name, *path;
svn_pool_clear(subpool);
apr_hash_this(hi, &key, NULL, &val);
name = key;
entry = val;
if (strcmp(key, SVN_WC_ENTRY_THIS_DIR) == 0)
continue;
if (in_anchor_not_target
&& strcmp(dir_baton->edit_baton->target, name))
continue;
path = svn_path_join(dir_baton->path, name, subpool);
if (apr_hash_get(dir_baton->compared, path, APR_HASH_KEY_STRING))
continue;
switch (entry->kind) {
case svn_node_file:
SVN_ERR(file_diff(dir_baton, path, entry, subpool));
break;
case svn_node_dir:
if (entry->schedule == svn_wc_schedule_replace) {
}
if (in_anchor_not_target
|| (dir_baton->depth > svn_depth_files)
|| (dir_baton->depth == svn_depth_unknown)) {
svn_depth_t depth_below_here = dir_baton->depth;
if (depth_below_here == svn_depth_immediates)
depth_below_here = svn_depth_empty;
subdir_baton = make_dir_baton(path, dir_baton,
dir_baton->edit_baton,
FALSE, depth_below_here,
subpool);
SVN_ERR(directory_elements_diff(subdir_baton));
}
break;
default:
break;
}
}
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
static svn_error_t *
report_wc_file_as_added(struct dir_baton *dir_baton,
svn_wc_adm_access_t *adm_access,
const char *path,
const svn_wc_entry_t *entry,
apr_pool_t *pool) {
struct edit_baton *eb = dir_baton->edit_baton;
apr_hash_t *emptyprops;
const char *mimetype;
apr_hash_t *wcprops = NULL;
apr_array_header_t *propchanges;
const char *empty_file;
const char *source_file;
const char *translated_file;
if (! SVN_WC__CL_MATCH(dir_baton->edit_baton->changelist_hash, entry))
return SVN_NO_ERROR;
SVN_ERR(get_empty_file(eb, &empty_file));
assert(!(entry->schedule == svn_wc_schedule_delete && !eb->use_text_base));
if (entry->copied) {
if (eb->use_text_base)
return SVN_NO_ERROR;
return file_diff(dir_baton, path, entry, pool);
}
emptyprops = apr_hash_make(pool);
if (eb->use_text_base)
SVN_ERR(get_base_mimetype(&mimetype, &wcprops,
adm_access, path, pool));
else
SVN_ERR(get_working_mimetype(&mimetype, &wcprops,
adm_access, path, pool));
SVN_ERR(svn_prop_diffs(&propchanges,
wcprops, emptyprops, pool));
if (eb->use_text_base)
source_file = svn_wc__text_base_path(path, FALSE, pool);
else
source_file = path;
SVN_ERR(svn_wc_translated_file2
(&translated_file,
source_file, path, adm_access,
SVN_WC_TRANSLATE_TO_NF
| SVN_WC_TRANSLATE_USE_GLOBAL_TMP,
pool));
SVN_ERR(eb->callbacks->file_added
(adm_access, NULL, NULL,
path,
empty_file, translated_file,
0, entry->revision,
NULL, mimetype,
propchanges, emptyprops,
eb->callback_baton));
return SVN_NO_ERROR;
}
static svn_error_t *
report_wc_directory_as_added(struct dir_baton *dir_baton,
apr_pool_t *pool) {
struct edit_baton *eb = dir_baton->edit_baton;
svn_wc_adm_access_t *adm_access;
apr_hash_t *emptyprops = apr_hash_make(pool), *wcprops = NULL;
const svn_wc_entry_t *this_dir_entry;
apr_array_header_t *propchanges;
apr_hash_t *entries;
apr_hash_index_t *hi;
apr_pool_t *subpool;
SVN_ERR(svn_wc_adm_retrieve(&adm_access, eb->anchor,
dir_baton->path, pool));
SVN_ERR(svn_wc_entries_read(&entries, adm_access, FALSE, pool));
this_dir_entry = apr_hash_get(entries, SVN_WC_ENTRY_THIS_DIR,
APR_HASH_KEY_STRING);
if (SVN_WC__CL_MATCH(dir_baton->edit_baton->changelist_hash, this_dir_entry)) {
if (eb->use_text_base)
SVN_ERR(svn_wc_get_prop_diffs(NULL, &wcprops,
dir_baton->path, adm_access, pool));
else
SVN_ERR(svn_wc_prop_list(&wcprops,
dir_baton->path, adm_access, pool));
SVN_ERR(svn_prop_diffs(&propchanges,
wcprops, emptyprops, pool));
if (propchanges->nelts > 0)
SVN_ERR(eb->callbacks->dir_props_changed
(adm_access, NULL,
dir_baton->path,
propchanges, emptyprops,
eb->callback_baton));
}
subpool = svn_pool_create(pool);
for (hi = apr_hash_first(pool, entries); hi;
hi = apr_hash_next(hi)) {
const void *key;
void *val;
const char *name, *path;
const svn_wc_entry_t *entry;
svn_pool_clear(subpool);
apr_hash_this(hi, &key, NULL, &val);
name = key;
entry = val;
if (strcmp(key, SVN_WC_ENTRY_THIS_DIR) == 0)
continue;
if (!eb->use_text_base && entry->schedule == svn_wc_schedule_delete)
continue;
path = svn_path_join(dir_baton->path, name, subpool);
switch (entry->kind) {
case svn_node_file:
SVN_ERR(report_wc_file_as_added(dir_baton,
adm_access, path, entry, subpool));
break;
case svn_node_dir:
if (dir_baton->depth > svn_depth_files
|| dir_baton->depth == svn_depth_unknown) {
svn_depth_t depth_below_here = dir_baton->depth;
struct dir_baton *subdir_baton;
if (depth_below_here == svn_depth_immediates)
depth_below_here = svn_depth_empty;
subdir_baton = make_dir_baton(path, dir_baton, eb, FALSE,
depth_below_here, subpool);
SVN_ERR(report_wc_directory_as_added(subdir_baton, subpool));
}
break;
default:
break;
}
}
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
static svn_error_t *
set_target_revision(void *edit_baton,
svn_revnum_t target_revision,
apr_pool_t *pool) {
struct edit_baton *eb = edit_baton;
eb->revnum = target_revision;
return SVN_NO_ERROR;
}
static svn_error_t *
open_root(void *edit_baton,
svn_revnum_t base_revision,
apr_pool_t *dir_pool,
void **root_baton) {
struct edit_baton *eb = edit_baton;
struct dir_baton *b;
eb->root_opened = TRUE;
b = make_dir_baton(eb->anchor_path, NULL, eb, FALSE, eb->depth, dir_pool);
*root_baton = b;
return SVN_NO_ERROR;
}
static svn_error_t *
delete_entry(const char *path,
svn_revnum_t base_revision,
void *parent_baton,
apr_pool_t *pool) {
struct dir_baton *pb = parent_baton;
struct edit_baton *eb = pb->edit_baton;
const svn_wc_entry_t *entry;
struct dir_baton *b;
const char *empty_file;
const char *full_path = svn_path_join(pb->edit_baton->anchor_path, path,
pb->pool);
svn_wc_adm_access_t *adm_access;
SVN_ERR(svn_wc_adm_probe_retrieve(&adm_access, pb->edit_baton->anchor,
full_path, pool));
SVN_ERR(svn_wc_entry(&entry, full_path, adm_access, FALSE, pool));
if (! entry)
return SVN_NO_ERROR;
apr_hash_set(pb->compared, full_path, APR_HASH_KEY_STRING, "");
if (!eb->use_text_base && entry->schedule == svn_wc_schedule_delete)
return SVN_NO_ERROR;
SVN_ERR(get_empty_file(pb->edit_baton, &empty_file));
switch (entry->kind) {
case svn_node_file:
if (eb->reverse_order) {
const char *textbase = svn_wc__text_base_path(full_path,
FALSE, pool);
apr_hash_t *baseprops = NULL;
const char *base_mimetype;
SVN_ERR(get_base_mimetype(&base_mimetype, &baseprops,
adm_access, full_path, pool));
SVN_ERR(pb->edit_baton->callbacks->file_deleted
(NULL, NULL, full_path,
textbase,
empty_file,
base_mimetype,
NULL,
baseprops,
pb->edit_baton->callback_baton));
} else {
SVN_ERR(report_wc_file_as_added(pb, adm_access, full_path, entry,
pool));
}
break;
case svn_node_dir:
b = make_dir_baton(full_path, pb, pb->edit_baton, FALSE,
svn_depth_infinity, pool);
SVN_ERR(report_wc_directory_as_added(b, pool));
break;
default:
break;
}
return SVN_NO_ERROR;
}
static svn_error_t *
add_directory(const char *path,
void *parent_baton,
const char *copyfrom_path,
svn_revnum_t copyfrom_revision,
apr_pool_t *dir_pool,
void **child_baton) {
struct dir_baton *pb = parent_baton;
struct dir_baton *b;
const char *full_path;
svn_depth_t subdir_depth = (pb->depth == svn_depth_immediates)
? svn_depth_empty : pb->depth;
full_path = svn_path_join(pb->edit_baton->anchor_path, path, dir_pool);
b = make_dir_baton(full_path, pb, pb->edit_baton, TRUE,
subdir_depth, dir_pool);
*child_baton = b;
return SVN_NO_ERROR;
}
static svn_error_t *
open_directory(const char *path,
void *parent_baton,
svn_revnum_t base_revision,
apr_pool_t *dir_pool,
void **child_baton) {
struct dir_baton *pb = parent_baton;
struct dir_baton *b;
const char *full_path;
svn_depth_t subdir_depth = (pb->depth == svn_depth_immediates)
? svn_depth_empty : pb->depth;
full_path = svn_path_join(pb->edit_baton->anchor_path, path, pb->pool);
b = make_dir_baton(full_path, pb, pb->edit_baton, FALSE,
subdir_depth, dir_pool);
*child_baton = b;
return SVN_NO_ERROR;
}
static svn_error_t *
close_directory(void *dir_baton,
apr_pool_t *pool) {
struct dir_baton *b = dir_baton;
struct dir_baton *pb = b->dir_baton;
if (b->propchanges->nelts > 0) {
apr_hash_t *originalprops;
if (b->added) {
originalprops = apr_hash_make(b->pool);
} else {
svn_wc_adm_access_t *adm_access;
SVN_ERR(svn_wc_adm_retrieve(&adm_access,
b->edit_baton->anchor, b->path,
b->pool));
if (b->edit_baton->use_text_base) {
SVN_ERR(svn_wc_get_prop_diffs(NULL, &originalprops,
b->path, adm_access, pool));
} else {
apr_hash_t *base_props, *repos_props;
SVN_ERR(svn_wc_prop_list(&originalprops, b->path,
b->edit_baton->anchor, pool));
SVN_ERR(svn_wc_get_prop_diffs(NULL, &base_props,
b->path, adm_access, pool));
repos_props = apply_propchanges(base_props, b->propchanges);
SVN_ERR(svn_prop_diffs(&b->propchanges,
repos_props, originalprops, b->pool));
}
}
if (! b->edit_baton->reverse_order)
reverse_propchanges(originalprops, b->propchanges, b->pool);
SVN_ERR(b->edit_baton->callbacks->dir_props_changed
(NULL, NULL,
b->path,
b->propchanges,
originalprops,
b->edit_baton->callback_baton));
apr_hash_set(b->compared, "", 0, "");
}
if (!b->added)
SVN_ERR(directory_elements_diff(dir_baton));
if (pb)
apr_hash_set(pb->compared, b->path, APR_HASH_KEY_STRING, "");
return SVN_NO_ERROR;
}
static svn_error_t *
add_file(const char *path,
void *parent_baton,
const char *copyfrom_path,
svn_revnum_t copyfrom_revision,
apr_pool_t *file_pool,
void **file_baton) {
struct dir_baton *pb = parent_baton;
struct file_baton *b;
const char *full_path;
full_path = svn_path_join(pb->edit_baton->anchor_path, path, file_pool);
b = make_file_baton(full_path, TRUE, pb, file_pool);
*file_baton = b;
apr_hash_set(pb->compared, apr_pstrdup(pb->pool, full_path),
APR_HASH_KEY_STRING, "");
return SVN_NO_ERROR;
}
static svn_error_t *
open_file(const char *path,
void *parent_baton,
svn_revnum_t base_revision,
apr_pool_t *file_pool,
void **file_baton) {
struct dir_baton *pb = parent_baton;
struct file_baton *b;
const char *full_path;
full_path = svn_path_join(pb->edit_baton->anchor_path, path, file_pool);
b = make_file_baton(full_path, FALSE, pb, file_pool);
*file_baton = b;
apr_hash_set(pb->compared, apr_pstrdup(pb->pool, full_path),
APR_HASH_KEY_STRING, "");
return SVN_NO_ERROR;
}
static svn_error_t *
window_handler(svn_txdelta_window_t *window,
void *window_baton) {
struct file_baton *b = window_baton;
SVN_ERR(b->apply_handler(window, b->apply_baton));
if (!window) {
SVN_ERR(svn_io_file_close(b->temp_file, b->pool));
if (b->added)
SVN_ERR(svn_io_file_close(b->original_file, b->pool));
else {
SVN_ERR(svn_wc__close_text_base(b->original_file, b->path, 0,
b->pool));
}
}
return SVN_NO_ERROR;
}
static svn_error_t *
apply_textdelta(void *file_baton,
const char *base_checksum,
apr_pool_t *pool,
svn_txdelta_window_handler_t *handler,
void **handler_baton) {
struct file_baton *b = file_baton;
struct edit_baton *eb = b->edit_baton;
const svn_wc_entry_t *entry;
const char *parent, *base_name;
SVN_ERR(svn_wc_entry(&entry, b->wc_path, eb->anchor, FALSE, b->pool));
svn_path_split(b->wc_path, &parent, &base_name, b->pool);
if (entry && entry->copyfrom_url)
b->added = FALSE;
if (b->added) {
const char *empty_file;
SVN_ERR(get_empty_file(eb, &empty_file));
SVN_ERR(svn_io_file_open(&b->original_file, empty_file,
APR_READ, APR_OS_DEFAULT, pool));
} else {
SVN_ERR(svn_wc__open_text_base(&b->original_file, b->path,
APR_READ, b->pool));
}
SVN_ERR(svn_wc_create_tmp_file2(&b->temp_file, &b->temp_file_path,
parent, svn_io_file_del_on_pool_cleanup,
b->pool));
svn_txdelta_apply(svn_stream_from_aprfile(b->original_file, b->pool),
svn_stream_from_aprfile(b->temp_file, b->pool),
NULL,
b->temp_file_path,
b->pool,
&b->apply_handler, &b->apply_baton);
*handler = window_handler;
*handler_baton = file_baton;
return SVN_NO_ERROR;
}
static svn_error_t *
close_file(void *file_baton,
const char *text_checksum,
apr_pool_t *pool) {
struct file_baton *b = file_baton;
struct edit_baton *eb = b->edit_baton;
svn_wc_adm_access_t *adm_access;
const svn_wc_entry_t *entry;
const char *repos_mimetype;
const char *empty_file;
apr_hash_t *base_props;
apr_hash_t *repos_props;
const char *localfile;
const char *temp_file_path;
svn_boolean_t modified;
apr_hash_t *originalprops;
SVN_ERR(svn_wc_adm_probe_retrieve(&adm_access, b->edit_baton->anchor,
b->wc_path, b->pool));
SVN_ERR(svn_wc_entry(&entry, b->wc_path, adm_access, FALSE, b->pool));
SVN_ERR(get_empty_file(b->edit_baton, &empty_file));
if (b->added)
base_props = apr_hash_make(pool);
else
SVN_ERR(svn_wc_get_prop_diffs(NULL, &base_props,
b->path, adm_access, pool));
repos_props = apply_propchanges(base_props, b->propchanges);
repos_mimetype = get_prop_mimetype(repos_props);
temp_file_path = b->temp_file_path;
if (!temp_file_path)
temp_file_path = svn_wc__text_base_path(b->path, FALSE, b->pool);
if (b->added ||
(!eb->use_text_base && entry->schedule == svn_wc_schedule_delete)) {
if (eb->reverse_order)
return b->edit_baton->callbacks->file_added
(NULL, NULL, NULL, b->path,
empty_file,
temp_file_path,
0,
eb->revnum,
NULL,
repos_mimetype,
b->propchanges,
apr_hash_make(pool),
b->edit_baton->callback_baton);
else
return b->edit_baton->callbacks->file_deleted
(NULL, NULL, b->path,
temp_file_path,
empty_file,
repos_mimetype,
NULL,
repos_props,
b->edit_baton->callback_baton);
}
modified = (b->temp_file_path != NULL);
if (!modified && !eb->use_text_base)
SVN_ERR(svn_wc_text_modified_p(&modified, b->path, FALSE,
adm_access, pool));
if (modified) {
if (eb->use_text_base)
localfile = svn_wc__text_base_path(b->path, FALSE, b->pool);
else
SVN_ERR(svn_wc_translated_file2
(&localfile, b->path,
b->path, adm_access,
SVN_WC_TRANSLATE_TO_NF
| SVN_WC_TRANSLATE_USE_GLOBAL_TMP,
pool));
} else
localfile = temp_file_path = NULL;
if (eb->use_text_base) {
originalprops = base_props;
} else {
SVN_ERR(svn_wc_prop_list(&originalprops,
b->path, adm_access, pool));
SVN_ERR(svn_prop_diffs(&b->propchanges,
repos_props, originalprops, b->pool));
}
if (localfile || b->propchanges->nelts > 0) {
const char *original_mimetype = get_prop_mimetype(originalprops);
if (b->propchanges->nelts > 0
&& ! eb->reverse_order)
reverse_propchanges(originalprops, b->propchanges, b->pool);
SVN_ERR(b->edit_baton->callbacks->file_changed
(NULL, NULL, NULL,
b->path,
eb->reverse_order ? localfile : temp_file_path,
eb->reverse_order ? temp_file_path : localfile,
eb->reverse_order ? SVN_INVALID_REVNUM : b->edit_baton->revnum,
eb->reverse_order ? b->edit_baton->revnum : SVN_INVALID_REVNUM,
eb->reverse_order ? original_mimetype : repos_mimetype,
eb->reverse_order ? repos_mimetype : original_mimetype,
b->propchanges, originalprops,
b->edit_baton->callback_baton));
}
return SVN_NO_ERROR;
}
static svn_error_t *
change_file_prop(void *file_baton,
const char *name,
const svn_string_t *value,
apr_pool_t *pool) {
struct file_baton *b = file_baton;
svn_prop_t *propchange;
propchange = apr_array_push(b->propchanges);
propchange->name = apr_pstrdup(b->pool, name);
propchange->value = value ? svn_string_dup(value, b->pool) : NULL;
return SVN_NO_ERROR;
}
static svn_error_t *
change_dir_prop(void *dir_baton,
const char *name,
const svn_string_t *value,
apr_pool_t *pool) {
struct dir_baton *db = dir_baton;
svn_prop_t *propchange;
propchange = apr_array_push(db->propchanges);
propchange->name = apr_pstrdup(db->pool, name);
propchange->value = value ? svn_string_dup(value, db->pool) : NULL;
return SVN_NO_ERROR;
}
static svn_error_t *
close_edit(void *edit_baton,
apr_pool_t *pool) {
struct edit_baton *eb = edit_baton;
if (!eb->root_opened) {
struct dir_baton *b;
b = make_dir_baton(eb->anchor_path, NULL, eb, FALSE,
eb->depth, eb->pool);
SVN_ERR(directory_elements_diff(b));
}
return SVN_NO_ERROR;
}
static svn_error_t *
file_changed(svn_wc_adm_access_t *adm_access,
svn_wc_notify_state_t *contentstate,
svn_wc_notify_state_t *propstate,
const char *path,
const char *tmpfile1,
const char *tmpfile2,
svn_revnum_t rev1,
svn_revnum_t rev2,
const char *mimetype1,
const char *mimetype2,
const apr_array_header_t *propchanges,
apr_hash_t *originalprops,
void *diff_baton) {
struct callbacks_wrapper_baton *b = diff_baton;
if (tmpfile2 != NULL)
SVN_ERR(b->callbacks->file_changed(adm_access, contentstate, path,
tmpfile1, tmpfile2,
rev1, rev2, mimetype1, mimetype2,
b->baton));
if (propchanges->nelts > 0)
SVN_ERR(b->callbacks->props_changed(adm_access, propstate, path,
propchanges, originalprops,
b->baton));
return SVN_NO_ERROR;
}
static svn_error_t *
file_added(svn_wc_adm_access_t *adm_access,
svn_wc_notify_state_t *contentstate,
svn_wc_notify_state_t *propstate,
const char *path,
const char *tmpfile1,
const char *tmpfile2,
svn_revnum_t rev1,
svn_revnum_t rev2,
const char *mimetype1,
const char *mimetype2,
const apr_array_header_t *propchanges,
apr_hash_t *originalprops,
void *diff_baton) {
struct callbacks_wrapper_baton *b = diff_baton;
SVN_ERR(b->callbacks->file_added(adm_access, contentstate, path,
tmpfile1, tmpfile2, rev1, rev2,
mimetype1, mimetype2, b->baton));
if (propchanges->nelts > 0)
SVN_ERR(b->callbacks->props_changed(adm_access, propstate, path,
propchanges, originalprops,
b->baton));
return SVN_NO_ERROR;
}
static svn_error_t *
file_deleted(svn_wc_adm_access_t *adm_access,
svn_wc_notify_state_t *state,
const char *path,
const char *tmpfile1,
const char *tmpfile2,
const char *mimetype1,
const char *mimetype2,
apr_hash_t *originalprops,
void *diff_baton) {
struct callbacks_wrapper_baton *b = diff_baton;
assert(originalprops);
return b->callbacks->file_deleted(adm_access, state, path,
tmpfile1, tmpfile2, mimetype1, mimetype2,
b->baton);
}
static svn_error_t *
dir_added(svn_wc_adm_access_t *adm_access,
svn_wc_notify_state_t *state,
const char *path,
svn_revnum_t rev,
void *diff_baton) {
struct callbacks_wrapper_baton *b = diff_baton;
return b->callbacks->dir_added(adm_access, state, path, rev, b->baton);
}
static svn_error_t *
dir_deleted(svn_wc_adm_access_t *adm_access,
svn_wc_notify_state_t *state,
const char *path,
void *diff_baton) {
struct callbacks_wrapper_baton *b = diff_baton;
return b->callbacks->dir_deleted(adm_access, state, path, b->baton);
}
static svn_error_t *
dir_props_changed(svn_wc_adm_access_t *adm_access,
svn_wc_notify_state_t *state,
const char *path,
const apr_array_header_t *propchanges,
apr_hash_t *originalprops,
void *diff_baton) {
struct callbacks_wrapper_baton *b = diff_baton;
return b->callbacks->props_changed(adm_access, state, path, propchanges,
originalprops, b->baton);
}
static struct svn_wc_diff_callbacks2_t callbacks_wrapper = {
file_changed,
file_added,
file_deleted,
dir_added,
dir_deleted,
dir_props_changed
};
svn_error_t *
svn_wc_get_diff_editor4(svn_wc_adm_access_t *anchor,
const char *target,
const svn_wc_diff_callbacks2_t *callbacks,
void *callback_baton,
svn_depth_t depth,
svn_boolean_t ignore_ancestry,
svn_boolean_t use_text_base,
svn_boolean_t reverse_order,
svn_cancel_func_t cancel_func,
void *cancel_baton,
const apr_array_header_t *changelists,
const svn_delta_editor_t **editor,
void **edit_baton,
apr_pool_t *pool) {
struct edit_baton *eb;
void *inner_baton;
svn_delta_editor_t *tree_editor;
const svn_delta_editor_t *inner_editor;
SVN_ERR(make_editor_baton(&eb, anchor, target, callbacks, callback_baton,
depth, ignore_ancestry, use_text_base,
reverse_order, changelists, pool));
tree_editor = svn_delta_default_editor(eb->pool);
tree_editor->set_target_revision = set_target_revision;
tree_editor->open_root = open_root;
tree_editor->delete_entry = delete_entry;
tree_editor->add_directory = add_directory;
tree_editor->open_directory = open_directory;
tree_editor->close_directory = close_directory;
tree_editor->add_file = add_file;
tree_editor->open_file = open_file;
tree_editor->apply_textdelta = apply_textdelta;
tree_editor->change_file_prop = change_file_prop;
tree_editor->change_dir_prop = change_dir_prop;
tree_editor->close_file = close_file;
tree_editor->close_edit = close_edit;
inner_editor = tree_editor;
inner_baton = eb;
if (depth == svn_depth_unknown)
SVN_ERR(svn_wc__ambient_depth_filter_editor(&inner_editor,
&inner_baton,
inner_editor,
inner_baton,
svn_wc_adm_access_path(anchor),
target,
anchor,
pool));
SVN_ERR(svn_delta_get_cancellation_editor(cancel_func,
cancel_baton,
inner_editor,
inner_baton,
editor,
edit_baton,
pool));
return SVN_NO_ERROR;
}
svn_error_t *
svn_wc_get_diff_editor3(svn_wc_adm_access_t *anchor,
const char *target,
const svn_wc_diff_callbacks2_t *callbacks,
void *callback_baton,
svn_boolean_t recurse,
svn_boolean_t ignore_ancestry,
svn_boolean_t use_text_base,
svn_boolean_t reverse_order,
svn_cancel_func_t cancel_func,
void *cancel_baton,
const svn_delta_editor_t **editor,
void **edit_baton,
apr_pool_t *pool) {
return svn_wc_get_diff_editor4(anchor,
target,
callbacks,
callback_baton,
SVN_DEPTH_INFINITY_OR_FILES(recurse),
ignore_ancestry,
use_text_base,
reverse_order,
cancel_func,
cancel_baton,
NULL,
editor,
edit_baton,
pool);
}
svn_error_t *
svn_wc_get_diff_editor2(svn_wc_adm_access_t *anchor,
const char *target,
const svn_wc_diff_callbacks_t *callbacks,
void *callback_baton,
svn_boolean_t recurse,
svn_boolean_t ignore_ancestry,
svn_boolean_t use_text_base,
svn_boolean_t reverse_order,
svn_cancel_func_t cancel_func,
void *cancel_baton,
const svn_delta_editor_t **editor,
void **edit_baton,
apr_pool_t *pool) {
struct callbacks_wrapper_baton *b = apr_pcalloc(pool, sizeof(*b));
b->callbacks = callbacks;
b->baton = callback_baton;
return svn_wc_get_diff_editor3(anchor, target, &callbacks_wrapper, b,
recurse, ignore_ancestry, use_text_base,
reverse_order, cancel_func, cancel_baton,
editor, edit_baton, pool);
}
svn_error_t *
svn_wc_get_diff_editor(svn_wc_adm_access_t *anchor,
const char *target,
const svn_wc_diff_callbacks_t *callbacks,
void *callback_baton,
svn_boolean_t recurse,
svn_boolean_t use_text_base,
svn_boolean_t reverse_order,
svn_cancel_func_t cancel_func,
void *cancel_baton,
const svn_delta_editor_t **editor,
void **edit_baton,
apr_pool_t *pool) {
return svn_wc_get_diff_editor2(anchor, target, callbacks, callback_baton,
recurse, FALSE, use_text_base, reverse_order,
cancel_func, cancel_baton,
editor, edit_baton, pool);
}
svn_error_t *
svn_wc_diff4(svn_wc_adm_access_t *anchor,
const char *target,
const svn_wc_diff_callbacks2_t *callbacks,
void *callback_baton,
svn_depth_t depth,
svn_boolean_t ignore_ancestry,
const apr_array_header_t *changelists,
apr_pool_t *pool) {
struct edit_baton *eb;
struct dir_baton *b;
const svn_wc_entry_t *entry;
const char *target_path;
svn_wc_adm_access_t *adm_access;
SVN_ERR(make_editor_baton(&eb, anchor, target, callbacks, callback_baton,
depth, ignore_ancestry, FALSE, FALSE,
changelists, pool));
target_path = svn_path_join(svn_wc_adm_access_path(anchor), target,
eb->pool);
SVN_ERR(svn_wc_adm_probe_retrieve(&adm_access, anchor, target_path,
eb->pool));
SVN_ERR(svn_wc__entry_versioned(&entry, target_path, adm_access, FALSE,
eb->pool));
if (entry->kind == svn_node_dir)
b = make_dir_baton(target_path, NULL, eb, FALSE, depth, eb->pool);
else
b = make_dir_baton(eb->anchor_path, NULL, eb, FALSE, depth, eb->pool);
SVN_ERR(directory_elements_diff(b));
return SVN_NO_ERROR;
}
svn_error_t *
svn_wc_diff3(svn_wc_adm_access_t *anchor,
const char *target,
const svn_wc_diff_callbacks2_t *callbacks,
void *callback_baton,
svn_boolean_t recurse,
svn_boolean_t ignore_ancestry,
apr_pool_t *pool) {
return svn_wc_diff4(anchor, target, callbacks, callback_baton,
SVN_DEPTH_INFINITY_OR_FILES(recurse), ignore_ancestry,
NULL, pool);
}
svn_error_t *
svn_wc_diff2(svn_wc_adm_access_t *anchor,
const char *target,
const svn_wc_diff_callbacks_t *callbacks,
void *callback_baton,
svn_boolean_t recurse,
svn_boolean_t ignore_ancestry,
apr_pool_t *pool) {
struct callbacks_wrapper_baton *b = apr_pcalloc(pool, sizeof(*b));
b->callbacks = callbacks;
b->baton = callback_baton;
return svn_wc_diff3(anchor, target, &callbacks_wrapper, b,
recurse, ignore_ancestry, pool);
}
svn_error_t *
svn_wc_diff(svn_wc_adm_access_t *anchor,
const char *target,
const svn_wc_diff_callbacks_t *callbacks,
void *callback_baton,
svn_boolean_t recurse,
apr_pool_t *pool) {
return svn_wc_diff2(anchor, target, callbacks, callback_baton,
recurse, FALSE, pool);
}