#include "svn_hash.h"
#include "svn_wc.h"
#include "svn_pools.h"
#include "svn_path.h"
#include "svn_io.h"
#include "svn_props.h"
#include "client.h"
struct edit_baton {
const char *target;
svn_wc_adm_access_t *adm_access;
const svn_wc_diff_callbacks2_t *diff_callbacks;
void *diff_cmd_baton;
svn_boolean_t dry_run;
svn_ra_session_t *ra_session;
svn_revnum_t revision;
svn_revnum_t target_revision;
const char *empty_file;
apr_hash_t *empty_hash;
apr_hash_t *deleted_paths;
svn_wc_notify_func2_t notify_func;
void *notify_baton;
apr_pool_t *pool;
};
typedef struct kind_action_state_t {
svn_node_kind_t kind;
svn_wc_notify_action_t action;
svn_wc_notify_state_t state;
} kind_action_state_t;
struct dir_baton {
svn_boolean_t added;
const char *path;
const char *wcpath;
struct dir_baton *dir_baton;
struct edit_baton *edit_baton;
apr_array_header_t *propchanges;
apr_hash_t *pristine_props;
apr_pool_t *pool;
};
struct file_baton {
svn_boolean_t added;
const char *path;
const char *wcpath;
const char *path_start_revision;
apr_file_t *file_start_revision;
apr_hash_t *pristine_props;
const char *path_end_revision;
apr_file_t *file_end_revision;
svn_txdelta_window_handler_t apply_handler;
void *apply_baton;
struct edit_baton *edit_baton;
apr_array_header_t *propchanges;
apr_pool_t *pool;
};
static struct dir_baton *
make_dir_baton(const char *path,
struct dir_baton *parent_baton,
struct edit_baton *edit_baton,
svn_boolean_t added,
apr_pool_t *pool) {
struct dir_baton *dir_baton = apr_pcalloc(pool, sizeof(*dir_baton));
dir_baton->dir_baton = parent_baton;
dir_baton->edit_baton = edit_baton;
dir_baton->added = added;
dir_baton->pool = pool;
dir_baton->path = apr_pstrdup(pool, path);
dir_baton->wcpath = svn_path_join(edit_baton->target, path, pool);
dir_baton->propchanges = apr_array_make(pool, 1, sizeof(svn_prop_t));
return dir_baton;
}
static struct file_baton *
make_file_baton(const char *path,
svn_boolean_t added,
void *edit_baton,
apr_pool_t *pool) {
struct file_baton *file_baton = apr_pcalloc(pool, sizeof(*file_baton));
struct edit_baton *eb = edit_baton;
file_baton->edit_baton = edit_baton;
file_baton->added = added;
file_baton->pool = pool;
file_baton->path = apr_pstrdup(pool, path);
file_baton->wcpath = svn_path_join(eb->target, path, pool);
file_baton->propchanges = apr_array_make(pool, 1, sizeof(svn_prop_t));
return file_baton;
}
static void
get_file_mime_types(const char **mimetype1,
const char **mimetype2,
struct file_baton *b) {
*mimetype1 = NULL;
*mimetype2 = NULL;
if (b->pristine_props) {
svn_string_t *pristine_val;
pristine_val = apr_hash_get(b->pristine_props, SVN_PROP_MIME_TYPE,
strlen(SVN_PROP_MIME_TYPE));
if (pristine_val)
*mimetype1 = pristine_val->data;
}
if (b->propchanges) {
int i;
svn_prop_t *propchange;
for (i = 0; i < b->propchanges->nelts; i++) {
propchange = &APR_ARRAY_IDX(b->propchanges, i, svn_prop_t);
if (strcmp(propchange->name, SVN_PROP_MIME_TYPE) == 0) {
if (propchange->value)
*mimetype2 = propchange->value->data;
break;
}
}
}
}
static svn_error_t *
get_file_from_ra(struct file_baton *b, svn_revnum_t revision) {
apr_file_t *file;
svn_stream_t *fstream;
const char *temp_dir;
SVN_ERR(svn_io_temp_dir(&temp_dir, b->pool));
SVN_ERR(svn_io_open_unique_file2(&file, &(b->path_start_revision),
svn_path_join(temp_dir, "tmp", b->pool),
"", svn_io_file_del_on_pool_cleanup,
b->pool));
fstream = svn_stream_from_aprfile(file, b->pool);
SVN_ERR(svn_ra_get_file(b->edit_baton->ra_session,
b->path,
revision,
fstream, NULL,
&(b->pristine_props),
b->pool));
SVN_ERR(svn_io_file_close(file, b->pool));
return SVN_NO_ERROR;
}
static svn_error_t *
get_dirprops_from_ra(struct dir_baton *b, svn_revnum_t base_revision) {
SVN_ERR(svn_ra_get_dir2(b->edit_baton->ra_session,
NULL, NULL, &(b->pristine_props),
b->path,
base_revision,
0,
b->pool));
return SVN_NO_ERROR;
}
static svn_error_t *
create_empty_file(apr_file_t **file,
const char **empty_file_path,
svn_wc_adm_access_t *adm_access,
svn_io_file_del_t delete_when,
apr_pool_t *pool) {
if (adm_access && svn_wc_adm_locked(adm_access))
SVN_ERR(svn_wc_create_tmp_file2(file, empty_file_path,
svn_wc_adm_access_path(adm_access),
delete_when, pool));
else {
const char *temp_dir;
SVN_ERR(svn_io_temp_dir(&temp_dir, pool));
SVN_ERR(svn_io_open_unique_file2(file, empty_file_path,
svn_path_join(temp_dir, "tmp", pool),
"", delete_when, pool));
}
return SVN_NO_ERROR;
}
static svn_error_t *
get_path_access(svn_wc_adm_access_t **path_access,
svn_wc_adm_access_t *adm_access,
const char *path,
svn_boolean_t lenient,
apr_pool_t *pool) {
if (! adm_access)
*path_access = NULL;
else {
svn_error_t *err = svn_wc_adm_retrieve(path_access, adm_access, path,
pool);
if (err) {
if (! lenient)
return err;
svn_error_clear(err);
*path_access = NULL;
}
}
return SVN_NO_ERROR;
}
static svn_error_t *
get_parent_access(svn_wc_adm_access_t **parent_access,
svn_wc_adm_access_t *adm_access,
const char *path,
svn_boolean_t lenient,
apr_pool_t *pool) {
if (! adm_access)
*parent_access = NULL;
else {
const char *parent_path = svn_path_dirname(path, pool);
SVN_ERR(get_path_access(parent_access, adm_access, parent_path,
lenient, pool));
}
return SVN_NO_ERROR;
}
static svn_error_t *
get_empty_file(struct edit_baton *eb,
const char **empty_file_path) {
if (!eb->empty_file)
SVN_ERR(create_empty_file(NULL, &(eb->empty_file), eb->adm_access,
svn_io_file_del_on_pool_cleanup, eb->pool));
*empty_file_path = eb->empty_file;
return SVN_NO_ERROR;
}
static svn_error_t *
set_target_revision(void *edit_baton,
svn_revnum_t target_revision,
apr_pool_t *pool) {
struct edit_baton *eb = edit_baton;
eb->target_revision = target_revision;
return SVN_NO_ERROR;
}
static svn_error_t *
open_root(void *edit_baton,
svn_revnum_t base_revision,
apr_pool_t *pool,
void **root_baton) {
struct edit_baton *eb = edit_baton;
struct dir_baton *b = make_dir_baton("", NULL, eb, FALSE, pool);
b->wcpath = apr_pstrdup(pool, eb->target);
SVN_ERR(get_dirprops_from_ra(b, base_revision));
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
svn_node_kind_t kind;
svn_wc_adm_access_t *adm_access;
svn_wc_notify_state_t state = svn_wc_notify_state_inapplicable;
svn_wc_notify_action_t action = svn_wc_notify_skip;
SVN_ERR(svn_ra_check_path(eb->ra_session, path, eb->revision, &kind, pool));
SVN_ERR(get_path_access(&adm_access, eb->adm_access, pb->wcpath,
TRUE, pool));
if ((! eb->adm_access) || adm_access) {
switch (kind) {
case svn_node_file: {
const char *mimetype1, *mimetype2;
struct file_baton *b;
b = make_file_baton(path, FALSE, eb, pool);
SVN_ERR(get_file_from_ra(b, eb->revision));
SVN_ERR(get_empty_file(b->edit_baton, &(b->path_end_revision)));
get_file_mime_types(&mimetype1, &mimetype2, b);
SVN_ERR(eb->diff_callbacks->file_deleted
(adm_access, &state, b->wcpath,
b->path_start_revision,
b->path_end_revision,
mimetype1, mimetype2,
b->pristine_props,
b->edit_baton->diff_cmd_baton));
break;
}
case svn_node_dir: {
SVN_ERR(eb->diff_callbacks->dir_deleted
(adm_access, &state,
svn_path_join(eb->target, path, pool),
eb->diff_cmd_baton));
break;
}
default:
break;
}
if ((state != svn_wc_notify_state_missing)
&& (state != svn_wc_notify_state_obstructed)) {
action = svn_wc_notify_update_delete;
if (eb->dry_run) {
const char *wcpath = svn_path_join(eb->target, path, pb->pool);
apr_hash_set(svn_client__dry_run_deletions(eb->diff_cmd_baton),
wcpath, APR_HASH_KEY_STRING, wcpath);
}
}
}
if (eb->notify_func) {
const char* deleted_path;
kind_action_state_t *kas = apr_palloc(eb->pool, sizeof(*kas));
deleted_path = svn_path_join(eb->target, path, eb->pool);
kas->kind = kind;
kas->action = action;
kas->state = state;
apr_hash_set(eb->deleted_paths, deleted_path, APR_HASH_KEY_STRING, kas);
}
return SVN_NO_ERROR;
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
struct dir_baton *b;
svn_wc_adm_access_t *adm_access;
svn_wc_notify_state_t state;
svn_wc_notify_action_t action;
b = make_dir_baton(path, pb, eb, TRUE, pool);
b->pristine_props = eb->empty_hash;
*child_baton = b;
SVN_ERR(get_path_access(&adm_access, eb->adm_access, pb->wcpath, TRUE,
pool));
SVN_ERR(eb->diff_callbacks->dir_added
(adm_access, &state, b->wcpath, eb->target_revision,
eb->diff_cmd_baton));
if ((state == svn_wc_notify_state_missing)
|| (state == svn_wc_notify_state_obstructed))
action = svn_wc_notify_skip;
else
action = svn_wc_notify_update_add;
if (eb->notify_func) {
svn_wc_notify_t *notify;
svn_boolean_t is_replace = FALSE;
kind_action_state_t *kas = apr_hash_get(eb->deleted_paths, b->wcpath,
APR_HASH_KEY_STRING);
if (kas) {
svn_wc_notify_action_t new_action;
if (kas->action == svn_wc_notify_update_delete
&& action == svn_wc_notify_update_add) {
is_replace = TRUE;
new_action = svn_wc_notify_update_replace;
} else
new_action = kas->action;
notify = svn_wc_create_notify(b->wcpath, new_action, pool);
notify->kind = kas->kind;
notify->content_state = notify->prop_state = kas->state;
notify->lock_state = svn_wc_notify_lock_state_inapplicable;
(*eb->notify_func)(eb->notify_baton, notify, pool);
apr_hash_set(eb->deleted_paths, b->wcpath,
APR_HASH_KEY_STRING, NULL);
}
if (!is_replace) {
notify = svn_wc_create_notify(b->wcpath, action, pool);
notify->kind = svn_node_dir;
(*eb->notify_func)(eb->notify_baton, notify, pool);
}
}
return SVN_NO_ERROR;
}
static svn_error_t *
open_directory(const char *path,
void *parent_baton,
svn_revnum_t base_revision,
apr_pool_t *pool,
void **child_baton) {
struct dir_baton *pb = parent_baton;
struct dir_baton *b;
b = make_dir_baton(path, pb, pb->edit_baton, FALSE, pool);
*child_baton = b;
SVN_ERR(get_dirprops_from_ra(b, base_revision));
return SVN_NO_ERROR;
}
static svn_error_t *
add_file(const char *path,
void *parent_baton,
const char *copyfrom_path,
svn_revnum_t copyfrom_revision,
apr_pool_t *pool,
void **file_baton) {
struct dir_baton *pb = parent_baton;
struct file_baton *b;
b = make_file_baton(path, TRUE, pb->edit_baton, pool);
*file_baton = b;
SVN_ERR(get_empty_file(b->edit_baton, &(b->path_start_revision)));
b->pristine_props = pb->edit_baton->empty_hash;
return SVN_NO_ERROR;
}
static svn_error_t *
open_file(const char *path,
void *parent_baton,
svn_revnum_t base_revision,
apr_pool_t *pool,
void **file_baton) {
struct dir_baton *pb = parent_baton;
struct file_baton *b;
b = make_file_baton(path, FALSE, pb->edit_baton, pool);
*file_baton = b;
SVN_ERR(get_file_from_ra(b, base_revision));
return SVN_NO_ERROR;
}
static svn_error_t *
window_handler(svn_txdelta_window_t *window,
void *window_baton) {
struct file_baton *b = window_baton;
SVN_ERR(b->apply_handler(window, b->apply_baton));
if (!window) {
SVN_ERR(svn_io_file_close(b->file_start_revision, b->pool));
SVN_ERR(svn_io_file_close(b->file_end_revision, b->pool));
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
svn_wc_adm_access_t *adm_access;
SVN_ERR(svn_io_file_open(&(b->file_start_revision),
b->path_start_revision,
APR_READ, APR_OS_DEFAULT, b->pool));
if (b->edit_baton->adm_access) {
svn_error_t *err;
err = svn_wc_adm_probe_retrieve(&adm_access, b->edit_baton->adm_access,
b->wcpath, pool);
if (err) {
svn_error_clear(err);
adm_access = NULL;
}
} else
adm_access = NULL;
SVN_ERR(create_empty_file(&(b->file_end_revision),
&(b->path_end_revision), adm_access,
svn_io_file_del_on_pool_cleanup, b->pool));
svn_txdelta_apply(svn_stream_from_aprfile(b->file_start_revision, b->pool),
svn_stream_from_aprfile(b->file_end_revision, b->pool),
NULL,
b->path,
b->pool,
&(b->apply_handler), &(b->apply_baton));
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
svn_error_t *err;
svn_wc_notify_action_t action;
svn_wc_notify_state_t
content_state = svn_wc_notify_state_unknown,
prop_state = svn_wc_notify_state_unknown;
err = get_parent_access(&adm_access, eb->adm_access,
b->wcpath, eb->dry_run, b->pool);
if (err && err->apr_err == SVN_ERR_WC_NOT_LOCKED) {
if (eb->notify_func) {
svn_wc_notify_t *notify = svn_wc_create_notify(b->wcpath,
svn_wc_notify_skip,
pool);
notify->kind = svn_node_file;
notify->content_state = svn_wc_notify_state_missing;
notify->prop_state = prop_state;
(*eb->notify_func)(eb->notify_baton, notify, pool);
}
svn_error_clear(err);
return SVN_NO_ERROR;
} else if (err)
return err;
if (b->path_end_revision || b->propchanges->nelts > 0) {
const char *mimetype1, *mimetype2;
get_file_mime_types(&mimetype1, &mimetype2, b);
if (b->added)
SVN_ERR(eb->diff_callbacks->file_added
(adm_access, &content_state, &prop_state,
b->wcpath,
b->path_end_revision ? b->path_start_revision : NULL,
b->path_end_revision,
0,
b->edit_baton->target_revision,
mimetype1, mimetype2,
b->propchanges, b->pristine_props,
b->edit_baton->diff_cmd_baton));
else
SVN_ERR(eb->diff_callbacks->file_changed
(adm_access, &content_state, &prop_state,
b->wcpath,
b->path_end_revision ? b->path_start_revision : NULL,
b->path_end_revision,
b->edit_baton->revision,
b->edit_baton->target_revision,
mimetype1, mimetype2,
b->propchanges, b->pristine_props,
b->edit_baton->diff_cmd_baton));
}
if ((content_state == svn_wc_notify_state_missing)
|| (content_state == svn_wc_notify_state_obstructed))
action = svn_wc_notify_skip;
else if (b->added)
action = svn_wc_notify_update_add;
else
action = svn_wc_notify_update_update;
if (eb->notify_func) {
svn_wc_notify_t *notify;
svn_boolean_t is_replace = FALSE;
kind_action_state_t *kas = apr_hash_get(eb->deleted_paths, b->wcpath,
APR_HASH_KEY_STRING);
if (kas) {
svn_wc_notify_action_t new_action;
if (kas->action == svn_wc_notify_update_delete
&& action == svn_wc_notify_update_add) {
is_replace = TRUE;
new_action = svn_wc_notify_update_replace;
} else
new_action = kas->action;
notify = svn_wc_create_notify(b->wcpath, new_action, pool);
notify->kind = kas->kind;
notify->content_state = notify->prop_state = kas->state;
notify->lock_state = svn_wc_notify_lock_state_inapplicable;
(*eb->notify_func)(eb->notify_baton, notify, pool);
apr_hash_set(eb->deleted_paths, b->wcpath,
APR_HASH_KEY_STRING, NULL);
}
if (!is_replace) {
notify = svn_wc_create_notify(b->wcpath, action, pool);
notify->kind = svn_node_file;
notify->content_state = content_state;
notify->prop_state = prop_state;
(*eb->notify_func)(eb->notify_baton, notify, pool);
}
}
return SVN_NO_ERROR;
}
static svn_error_t *
close_directory(void *dir_baton,
apr_pool_t *pool) {
struct dir_baton *b = dir_baton;
struct edit_baton *eb = b->edit_baton;
svn_wc_notify_state_t prop_state = svn_wc_notify_state_unknown;
svn_error_t *err;
if (eb->dry_run)
svn_hash__clear(svn_client__dry_run_deletions(eb->diff_cmd_baton));
if (b->propchanges->nelts > 0) {
svn_wc_adm_access_t *adm_access;
err = get_path_access(&adm_access, eb->adm_access, b->wcpath,
eb->dry_run, b->pool);
if (err && err->apr_err == SVN_ERR_WC_NOT_LOCKED) {
if (eb->notify_func) {
svn_wc_notify_t *notify
= svn_wc_create_notify(b->wcpath, svn_wc_notify_skip, pool);
notify->kind = svn_node_dir;
notify->content_state = notify->prop_state
= svn_wc_notify_state_missing;
(*eb->notify_func)(eb->notify_baton, notify, pool);
}
svn_error_clear(err);
return SVN_NO_ERROR;
} else if (err)
return err;
if (! eb->dry_run || adm_access)
SVN_ERR(eb->diff_callbacks->dir_props_changed
(adm_access, &prop_state,
b->wcpath,
b->propchanges, b->pristine_props,
b->edit_baton->diff_cmd_baton));
}
if (!b->added && eb->notify_func) {
svn_wc_notify_t *notify;
apr_hash_index_t *hi;
for (hi = apr_hash_first(NULL, eb->deleted_paths); hi;
hi = apr_hash_next(hi)) {
const void *deleted_path;
kind_action_state_t *kas;
apr_hash_this(hi, &deleted_path, NULL, (void *)&kas);
notify = svn_wc_create_notify(deleted_path, kas->action, pool);
notify->kind = kas->kind;
notify->content_state = notify->prop_state = kas->state;
notify->lock_state = svn_wc_notify_lock_state_inapplicable;
(*eb->notify_func)(eb->notify_baton, notify, pool);
apr_hash_set(eb->deleted_paths, deleted_path,
APR_HASH_KEY_STRING, NULL);
}
notify = svn_wc_create_notify(b->wcpath,
svn_wc_notify_update_update, pool);
notify->kind = svn_node_dir;
notify->content_state = svn_wc_notify_state_inapplicable;
notify->prop_state = prop_state;
notify->lock_state = svn_wc_notify_lock_state_inapplicable;
(*eb->notify_func)(eb->notify_baton, notify, pool);
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
svn_pool_destroy(eb->pool);
return SVN_NO_ERROR;
}
static svn_error_t *
absent_directory(const char *path,
void *parent_baton,
apr_pool_t *pool) {
struct dir_baton *pb = parent_baton;
struct edit_baton *eb = pb->edit_baton;
if (eb->notify_func) {
svn_wc_notify_t *notify
= svn_wc_create_notify(svn_path_join(pb->wcpath,
svn_path_basename(path, pool),
pool),
svn_wc_notify_skip, pool);
notify->kind = svn_node_dir;
notify->content_state = notify->prop_state
= svn_wc_notify_state_missing;
(*eb->notify_func)(eb->notify_baton, notify, pool);
}
return SVN_NO_ERROR;
}
static svn_error_t *
absent_file(const char *path,
void *parent_baton,
apr_pool_t *pool) {
struct dir_baton *pb = parent_baton;
struct edit_baton *eb = pb->edit_baton;
if (eb->notify_func) {
svn_wc_notify_t *notify
= svn_wc_create_notify(svn_path_join(pb->wcpath,
svn_path_basename(path, pool),
pool),
svn_wc_notify_skip, pool);
notify->kind = svn_node_file;
notify->content_state = notify->prop_state
= svn_wc_notify_state_missing;
(*eb->notify_func)(eb->notify_baton, notify, pool);
}
return SVN_NO_ERROR;
}
svn_error_t *
svn_client__get_diff_editor(const char *target,
svn_wc_adm_access_t *adm_access,
const svn_wc_diff_callbacks2_t *diff_callbacks,
void *diff_cmd_baton,
svn_depth_t depth,
svn_boolean_t dry_run,
svn_ra_session_t *ra_session,
svn_revnum_t revision,
svn_wc_notify_func2_t notify_func,
void *notify_baton,
svn_cancel_func_t cancel_func,
void *cancel_baton,
const svn_delta_editor_t **editor,
void **edit_baton,
apr_pool_t *pool) {
apr_pool_t *subpool = svn_pool_create(pool);
svn_delta_editor_t *tree_editor = svn_delta_default_editor(subpool);
struct edit_baton *eb = apr_palloc(subpool, sizeof(*eb));
eb->target = target;
eb->adm_access = adm_access;
eb->diff_callbacks = diff_callbacks;
eb->diff_cmd_baton = diff_cmd_baton;
eb->dry_run = dry_run;
eb->ra_session = ra_session;
eb->revision = revision;
eb->empty_file = NULL;
eb->empty_hash = apr_hash_make(subpool);
eb->deleted_paths = apr_hash_make(subpool);
eb->pool = subpool;
eb->notify_func = notify_func;
eb->notify_baton = notify_baton;
tree_editor->set_target_revision = set_target_revision;
tree_editor->open_root = open_root;
tree_editor->delete_entry = delete_entry;
tree_editor->add_directory = add_directory;
tree_editor->open_directory = open_directory;
tree_editor->add_file = add_file;
tree_editor->open_file = open_file;
tree_editor->apply_textdelta = apply_textdelta;
tree_editor->close_file = close_file;
tree_editor->close_directory = close_directory;
tree_editor->change_file_prop = change_file_prop;
tree_editor->change_dir_prop = change_dir_prop;
tree_editor->close_edit = close_edit;
tree_editor->absent_directory = absent_directory;
tree_editor->absent_file = absent_file;
SVN_ERR(svn_delta_get_cancellation_editor(cancel_func,
cancel_baton,
tree_editor,
eb,
editor,
edit_baton,
pool));
return SVN_NO_ERROR;
}
