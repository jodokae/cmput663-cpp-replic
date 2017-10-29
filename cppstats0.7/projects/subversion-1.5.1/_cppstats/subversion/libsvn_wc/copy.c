#include <string.h>
#include "svn_pools.h"
#include "svn_error.h"
#include "svn_path.h"
#include "wc.h"
#include "adm_files.h"
#include "entries.h"
#include "props.h"
#include "translate.h"
#include "svn_private_config.h"
#include "private/svn_wc_private.h"
static svn_error_t *
copy_added_file_administratively(const char *src_path,
svn_boolean_t src_is_added,
svn_wc_adm_access_t *dst_parent_access,
const char *dst_basename,
svn_cancel_func_t cancel_func,
void *cancel_baton,
svn_wc_notify_func2_t notify_func,
void *notify_baton,
apr_pool_t *pool) {
const char *dst_path
= svn_path_join(svn_wc_adm_access_path(dst_parent_access),
dst_basename, pool);
SVN_ERR(svn_io_copy_file(src_path, dst_path, TRUE, pool));
if (src_is_added) {
SVN_ERR(svn_wc_add2(dst_path, dst_parent_access, NULL,
SVN_INVALID_REVNUM, cancel_func,
cancel_baton, notify_func,
notify_baton, pool));
}
return SVN_NO_ERROR;
}
static svn_error_t *
copy_added_dir_administratively(const char *src_path,
svn_boolean_t src_is_added,
svn_wc_adm_access_t *dst_parent_access,
svn_wc_adm_access_t *src_access,
const char *dst_basename,
svn_cancel_func_t cancel_func,
void *cancel_baton,
svn_wc_notify_func2_t notify_func,
void *notify_baton,
apr_pool_t *pool) {
const char *dst_parent = svn_wc_adm_access_path(dst_parent_access);
if (! src_is_added) {
SVN_ERR(svn_io_copy_dir_recursively(src_path, dst_parent, dst_basename,
TRUE, cancel_func, cancel_baton,
pool));
} else {
const svn_wc_entry_t *entry;
svn_wc_adm_access_t *dst_child_dir_access;
svn_wc_adm_access_t *src_child_dir_access;
apr_dir_t *dir;
apr_finfo_t this_entry;
svn_error_t *err;
apr_pool_t *subpool;
apr_int32_t flags = APR_FINFO_TYPE | APR_FINFO_NAME;
const char *dst_path = svn_path_join(dst_parent, dst_basename, pool);
if (cancel_func)
SVN_ERR(cancel_func(cancel_baton));
SVN_ERR(svn_io_dir_make(dst_path, APR_OS_DEFAULT, pool));
SVN_ERR(svn_wc_add2(dst_path, dst_parent_access, NULL,
SVN_INVALID_REVNUM, cancel_func, cancel_baton,
notify_func, notify_baton, pool));
SVN_ERR(svn_wc_adm_retrieve(&dst_child_dir_access, dst_parent_access,
dst_path, pool));
SVN_ERR(svn_wc_adm_retrieve(&src_child_dir_access, src_access,
src_path, pool));
SVN_ERR(svn_io_dir_open(&dir, src_path, pool));
subpool = svn_pool_create(pool);
while (1) {
const char *src_fullpath;
svn_pool_clear(subpool);
err = svn_io_dir_read(&this_entry, flags, dir, subpool);
if (err) {
if (APR_STATUS_IS_ENOENT(err->apr_err)) {
apr_status_t apr_err;
svn_error_clear(err);
apr_err = apr_dir_close(dir);
if (apr_err)
return svn_error_wrap_apr(apr_err,
_("Can't close "
"directory '%s'"),
svn_path_local_style(src_path,
subpool));
break;
} else {
return svn_error_createf(err->apr_err, err,
_("Error during recursive copy "
"of '%s'"),
svn_path_local_style(src_path,
subpool));
}
}
if (this_entry.name[0] == '.'
&& (this_entry.name[1] == '\0'
|| (this_entry.name[1] == '.'
&& this_entry.name[2] == '\0')))
continue;
if (cancel_func)
SVN_ERR(cancel_func(cancel_baton));
if (svn_wc_is_adm_dir(this_entry.name, subpool))
continue;
src_fullpath = svn_path_join(src_path, this_entry.name, subpool);
SVN_ERR(svn_wc_entry(&entry, src_fullpath, src_child_dir_access,
TRUE, subpool));
if (this_entry.filetype == APR_DIR) {
SVN_ERR(copy_added_dir_administratively(src_fullpath,
entry ? TRUE : FALSE,
dst_child_dir_access,
src_child_dir_access,
this_entry.name,
cancel_func,
cancel_baton,
notify_func,
notify_baton,
subpool));
} else if (this_entry.filetype != APR_UNKFILE) {
SVN_ERR(copy_added_file_administratively(src_fullpath,
entry ? TRUE : FALSE,
dst_child_dir_access,
this_entry.name,
cancel_func,
cancel_baton,
notify_func,
notify_baton,
subpool));
}
}
svn_pool_destroy(subpool);
}
return SVN_NO_ERROR;
}
static svn_error_t *
get_copyfrom_url_rev_via_parent(const char *src_path,
const char **copyfrom_url,
svn_revnum_t *copyfrom_rev,
svn_wc_adm_access_t *src_access,
apr_pool_t *pool) {
const char *parent_path = svn_path_dirname(src_path, pool);
const char *rest = svn_path_basename(src_path, pool);
*copyfrom_url = NULL;
while (! *copyfrom_url) {
svn_wc_adm_access_t *parent_access;
const svn_wc_entry_t *entry;
if (svn_path_is_ancestor(svn_wc_adm_access_path(src_access),
parent_path)) {
SVN_ERR(svn_wc_adm_retrieve(&parent_access, src_access,
parent_path, pool));
SVN_ERR(svn_wc__entry_versioned(&entry, parent_path, parent_access,
FALSE, pool));
} else {
SVN_ERR(svn_wc_adm_probe_open3(&parent_access, NULL,
parent_path, FALSE, -1,
NULL, NULL, pool));
SVN_ERR(svn_wc__entry_versioned(&entry, parent_path, parent_access,
FALSE, pool));
SVN_ERR(svn_wc_adm_close(parent_access));
}
if (entry->copyfrom_url) {
*copyfrom_url = svn_path_join(entry->copyfrom_url, rest,
pool);
*copyfrom_rev = entry->copyfrom_rev;
} else {
rest = svn_path_join(svn_path_basename(parent_path, pool),
rest, pool);
parent_path = svn_path_dirname(parent_path, pool);
}
}
return SVN_NO_ERROR;
}
static APR_INLINE svn_error_t *
determine_copyfrom_info(const char **copyfrom_url, svn_revnum_t *copyfrom_rev,
const char *src_path, svn_wc_adm_access_t *src_access,
const svn_wc_entry_t *src_entry,
const svn_wc_entry_t *dst_entry, apr_pool_t *pool) {
const char *url;
svn_revnum_t rev;
if (src_entry->copyfrom_url) {
url = src_entry->copyfrom_url;
rev = src_entry->copyfrom_rev;
} else {
SVN_ERR(get_copyfrom_url_rev_via_parent(src_path, &url, &rev,
src_access, pool));
}
if (dst_entry && rev == dst_entry->revision &&
strcmp(url, dst_entry->url) == 0) {
url = NULL;
rev = SVN_INVALID_REVNUM;
} else if (src_entry->copyfrom_url) {
url = apr_pstrdup(pool, url);
}
*copyfrom_url = url;
*copyfrom_rev = rev;
return SVN_NO_ERROR;
}
static svn_error_t *
copy_file_administratively(const char *src_path,
svn_wc_adm_access_t *src_access,
svn_wc_adm_access_t *dst_parent,
const char *dst_basename,
svn_wc_notify_func2_t notify_copied,
void *notify_baton,
apr_pool_t *pool) {
svn_node_kind_t dst_kind;
const svn_wc_entry_t *src_entry, *dst_entry;
const char *dst_path
= svn_path_join(svn_wc_adm_access_path(dst_parent), dst_basename, pool);
const char *src_txtb = svn_wc__text_base_path(src_path, FALSE, pool);
const char *tmp_txtb = svn_wc__text_base_path(dst_path, TRUE, pool);
SVN_ERR(svn_io_check_path(dst_path, &dst_kind, pool));
if (dst_kind != svn_node_none)
return svn_error_createf(SVN_ERR_ENTRY_EXISTS, NULL,
_("'%s' already exists and is in the way"),
svn_path_local_style(dst_path, pool));
SVN_ERR(svn_wc_entry(&dst_entry, dst_path, dst_parent, FALSE, pool));
if (dst_entry && dst_entry->kind == svn_node_file) {
if (dst_entry->schedule != svn_wc_schedule_delete)
return svn_error_createf(SVN_ERR_ENTRY_EXISTS, NULL,
_("There is already a versioned item '%s'"),
svn_path_local_style(dst_path, pool));
}
SVN_ERR(svn_wc__entry_versioned(&src_entry, src_path, src_access, FALSE,
pool));
if ((src_entry->schedule == svn_wc_schedule_add && (! src_entry->copied))
|| (! src_entry->url))
return svn_error_createf
(SVN_ERR_UNSUPPORTED_FEATURE, NULL,
_("Cannot copy or move '%s': it is not in the repository yet; "
"try committing first"),
svn_path_local_style(src_path, pool));
{
const char *copyfrom_url;
const char *tmp_wc_text;
svn_revnum_t copyfrom_rev;
apr_hash_t *props, *base_props;
if (src_entry->copied) {
SVN_ERR(determine_copyfrom_info(&copyfrom_url, &copyfrom_rev, src_path,
src_access, src_entry, dst_entry,
pool));
} else {
char *tmp;
SVN_ERR(svn_wc_get_ancestry(&tmp, &copyfrom_rev, src_path, src_access,
pool));
copyfrom_url = tmp;
}
SVN_ERR(svn_wc__load_props(&base_props, &props, NULL, src_access,
src_path, pool));
SVN_ERR(svn_io_copy_file(src_txtb, tmp_txtb, TRUE, pool));
{
svn_boolean_t special;
SVN_ERR(svn_wc_create_tmp_file2(NULL, &tmp_wc_text,
svn_wc_adm_access_path(dst_parent),
svn_io_file_del_none, pool));
SVN_ERR(svn_wc__get_special(&special, src_path, src_access, pool));
if (special) {
SVN_ERR(svn_subst_copy_and_translate3(src_path, tmp_wc_text,
NULL, FALSE, NULL,
FALSE, special, pool));
} else
SVN_ERR(svn_io_copy_file(src_path, tmp_wc_text, TRUE, pool));
}
SVN_ERR(svn_wc_add_repos_file2(dst_path, dst_parent,
tmp_txtb, tmp_wc_text,
base_props, props,
copyfrom_url, copyfrom_rev, pool));
}
if (notify_copied != NULL) {
svn_wc_notify_t *notify = svn_wc_create_notify(dst_path,
svn_wc_notify_add,
pool);
notify->kind = svn_node_file;
(*notify_copied)(notify_baton, notify, pool);
}
return SVN_NO_ERROR;
}
static svn_error_t *
post_copy_cleanup(svn_wc_adm_access_t *adm_access,
apr_pool_t *pool) {
apr_pool_t *subpool = svn_pool_create(pool);
apr_hash_t *entries;
apr_hash_index_t *hi;
svn_wc_entry_t *entry;
const char *path = svn_wc_adm_access_path(adm_access);
SVN_ERR(svn_wc__props_delete(path, svn_wc__props_wcprop, adm_access, pool));
SVN_ERR(svn_wc_entries_read(&entries, adm_access, FALSE, pool));
#if defined(APR_FILE_ATTR_HIDDEN)
{
const char *adm_dir = svn_wc__adm_path(path, FALSE, pool, NULL);
const char *path_apr;
apr_status_t status;
SVN_ERR(svn_path_cstring_from_utf8(&path_apr, adm_dir, pool));
status = apr_file_attrs_set(path_apr,
APR_FILE_ATTR_HIDDEN,
APR_FILE_ATTR_HIDDEN,
pool);
if (status)
return svn_error_wrap_apr(status, _("Can't hide directory '%s'"),
svn_path_local_style(adm_dir, pool));
}
#endif
SVN_ERR(svn_wc_entries_read(&entries, adm_access, TRUE, pool));
for (hi = apr_hash_first(pool, entries); hi; hi = apr_hash_next(hi)) {
const void *key;
void *val;
svn_node_kind_t kind;
svn_boolean_t deleted = FALSE;
apr_uint64_t flags = SVN_WC__ENTRY_MODIFY_FORCE;
svn_pool_clear(subpool);
apr_hash_this(hi, &key, NULL, &val);
entry = val;
kind = entry->kind;
deleted = entry->deleted;
if (entry->deleted) {
entry->schedule = svn_wc_schedule_delete;
flags |= SVN_WC__ENTRY_MODIFY_SCHEDULE;
entry->deleted = FALSE;
flags |= SVN_WC__ENTRY_MODIFY_DELETED;
if (entry->kind == svn_node_dir) {
entry->kind = svn_node_file;
flags |= SVN_WC__ENTRY_MODIFY_KIND;
}
}
if (entry->lock_token) {
entry->lock_token = NULL;
entry->lock_owner = NULL;
entry->lock_comment = NULL;
entry->lock_creation_date = 0;
flags |= (SVN_WC__ENTRY_MODIFY_LOCK_TOKEN
| SVN_WC__ENTRY_MODIFY_LOCK_OWNER
| SVN_WC__ENTRY_MODIFY_LOCK_COMMENT
| SVN_WC__ENTRY_MODIFY_LOCK_CREATION_DATE);
}
if (flags != SVN_WC__ENTRY_MODIFY_FORCE)
SVN_ERR(svn_wc__entry_modify(adm_access, key, entry,
flags, TRUE, subpool));
if ((! deleted)
&& (kind == svn_node_dir)
&& (strcmp(key, SVN_WC_ENTRY_THIS_DIR) != 0)) {
svn_wc_adm_access_t *child_access;
const char *child_path;
child_path = svn_path_join
(svn_wc_adm_access_path(adm_access), key, subpool);
SVN_ERR(svn_wc_adm_retrieve(&child_access, adm_access,
child_path, subpool));
SVN_ERR(post_copy_cleanup(child_access, subpool));
}
}
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
static svn_error_t *
copy_dir_administratively(const char *src_path,
svn_wc_adm_access_t *src_access,
svn_wc_adm_access_t *dst_parent,
const char *dst_basename,
svn_cancel_func_t cancel_func,
void *cancel_baton,
svn_wc_notify_func2_t notify_copied,
void *notify_baton,
apr_pool_t *pool) {
const svn_wc_entry_t *src_entry;
svn_wc_adm_access_t *adm_access;
const char *dst_path = svn_path_join(svn_wc_adm_access_path(dst_parent),
dst_basename, pool);
SVN_ERR(svn_wc__entry_versioned(&src_entry, src_path, src_access, FALSE,
pool));
if ((src_entry->schedule == svn_wc_schedule_add && (! src_entry->copied))
|| (! src_entry->url))
return svn_error_createf
(SVN_ERR_UNSUPPORTED_FEATURE, NULL,
_("Cannot copy or move '%s': it is not in the repository yet; "
"try committing first"),
svn_path_local_style(src_path, pool));
SVN_ERR(svn_io_copy_dir_recursively(src_path,
svn_wc_adm_access_path(dst_parent),
dst_basename,
TRUE,
cancel_func, cancel_baton,
pool));
SVN_ERR(svn_wc_cleanup2(dst_path, NULL, cancel_func, cancel_baton, pool));
SVN_ERR(svn_wc_adm_open3(&adm_access, NULL, dst_path, TRUE, -1,
cancel_func, cancel_baton, pool));
SVN_ERR(post_copy_cleanup(adm_access, pool));
{
const char *copyfrom_url;
svn_revnum_t copyfrom_rev;
svn_wc_entry_t tmp_entry;
if (src_entry->copied) {
const svn_wc_entry_t *dst_entry;
SVN_ERR(svn_wc_entry(&dst_entry, dst_path, dst_parent, FALSE, pool));
SVN_ERR(determine_copyfrom_info(&copyfrom_url, &copyfrom_rev, src_path,
src_access, src_entry, dst_entry,
pool));
tmp_entry.url = apr_pstrdup(pool, copyfrom_url);
SVN_ERR(svn_wc__entry_modify(adm_access, NULL,
&tmp_entry,
SVN_WC__ENTRY_MODIFY_URL, TRUE,
pool));
} else {
char *tmp;
SVN_ERR(svn_wc_get_ancestry(&tmp, &copyfrom_rev, src_path, src_access,
pool));
copyfrom_url = tmp;
}
SVN_ERR(svn_wc_adm_close(adm_access));
SVN_ERR(svn_wc_add2(dst_path, dst_parent,
copyfrom_url, copyfrom_rev,
cancel_func, cancel_baton,
notify_copied, notify_baton, pool));
}
return SVN_NO_ERROR;
}
svn_error_t *
svn_wc_copy2(const char *src_path,
svn_wc_adm_access_t *dst_parent,
const char *dst_basename,
svn_cancel_func_t cancel_func,
void *cancel_baton,
svn_wc_notify_func2_t notify_func,
void *notify_baton,
apr_pool_t *pool) {
svn_wc_adm_access_t *adm_access;
svn_node_kind_t src_kind;
const char *dst_path;
const svn_wc_entry_t *dst_entry, *src_entry;
SVN_ERR(svn_wc_adm_probe_open3(&adm_access, NULL, src_path, FALSE, -1,
cancel_func, cancel_baton, pool));
dst_path = svn_wc_adm_access_path(dst_parent);
SVN_ERR(svn_wc__entry_versioned(&dst_entry, dst_path, dst_parent, FALSE,
pool));
SVN_ERR(svn_wc__entry_versioned(&src_entry, src_path, adm_access, FALSE,
pool));
if ((src_entry->repos != NULL && dst_entry->repos != NULL) &&
strcmp(src_entry->repos, dst_entry->repos) != 0)
return svn_error_createf
(SVN_ERR_WC_INVALID_SCHEDULE, NULL,
_("Cannot copy to '%s', as it is not from repository '%s'; "
"it is from '%s'"),
svn_path_local_style(svn_wc_adm_access_path(dst_parent), pool),
src_entry->repos, dst_entry->repos);
if (dst_entry->schedule == svn_wc_schedule_delete)
return svn_error_createf
(SVN_ERR_WC_INVALID_SCHEDULE, NULL,
_("Cannot copy to '%s' as it is scheduled for deletion"),
svn_path_local_style(svn_wc_adm_access_path(dst_parent), pool));
SVN_ERR(svn_io_check_path(src_path, &src_kind, pool));
if (src_kind == svn_node_file) {
if (src_entry->schedule == svn_wc_schedule_add
&& (! src_entry->copied)) {
SVN_ERR(copy_added_file_administratively(src_path, TRUE,
dst_parent, dst_basename,
cancel_func, cancel_baton,
notify_func, notify_baton,
pool));
} else {
SVN_ERR(copy_file_administratively(src_path, adm_access,
dst_parent, dst_basename,
notify_func, notify_baton, pool));
}
} else if (src_kind == svn_node_dir) {
if (src_entry->schedule == svn_wc_schedule_add
&& (! src_entry->copied)) {
SVN_ERR(copy_added_dir_administratively(src_path, TRUE,
dst_parent, adm_access,
dst_basename, cancel_func,
cancel_baton, notify_func,
notify_baton, pool));
} else {
SVN_ERR(copy_dir_administratively(src_path, adm_access,
dst_parent, dst_basename,
cancel_func, cancel_baton,
notify_func, notify_baton, pool));
}
}
SVN_ERR(svn_wc_adm_close(adm_access));
return SVN_NO_ERROR;
}
svn_error_t *
svn_wc_copy(const char *src_path,
svn_wc_adm_access_t *dst_parent,
const char *dst_basename,
svn_cancel_func_t cancel_func,
void *cancel_baton,
svn_wc_notify_func_t notify_func,
void *notify_baton,
apr_pool_t *pool) {
svn_wc__compat_notify_baton_t nb;
nb.func = notify_func;
nb.baton = notify_baton;
return svn_wc_copy2(src_path, dst_parent, dst_basename, cancel_func,
cancel_baton, svn_wc__compat_call_notify_func,
&nb, pool);
}