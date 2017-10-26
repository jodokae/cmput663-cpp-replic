#include <stdarg.h>
#include <assert.h>
#include <apr_pools.h>
#include <apr_file_io.h>
#include <apr_strings.h>
#include "svn_types.h"
#include "svn_error.h"
#include "svn_io.h"
#include "svn_path.h"
#include "wc.h"
#include "adm_files.h"
#include "entries.h"
#include "lock.h"
#include "svn_private_config.h"
#include "private/svn_wc_private.h"
static const char default_adm_dir_name[] = ".svn";
static const char *adm_dir_name = default_adm_dir_name;
svn_boolean_t
svn_wc_is_adm_dir(const char *name, apr_pool_t *pool) {
return (0 == strcmp(name, adm_dir_name)
|| 0 == strcmp(name, default_adm_dir_name));
}
const char *
svn_wc_get_adm_dir(apr_pool_t *pool) {
return adm_dir_name;
}
svn_error_t *
svn_wc_set_adm_dir(const char *name, apr_pool_t *pool) {
static const char *valid_dir_names[] = {
default_adm_dir_name,
"_svn",
NULL
};
const char **dir_name;
for (dir_name = valid_dir_names; *dir_name; ++dir_name)
if (0 == strcmp(name, *dir_name)) {
adm_dir_name = *dir_name;
return SVN_NO_ERROR;
}
return svn_error_createf
(SVN_ERR_BAD_FILENAME, NULL,
_("'%s' is not a valid administrative directory name"),
svn_path_local_style(name, pool));
}
static const char *
v_extend_with_adm_name(const char *path,
const char *extension,
svn_boolean_t use_tmp,
apr_pool_t *pool,
va_list ap) {
const char *this;
path = svn_path_join(path, adm_dir_name, pool);
if (use_tmp)
path = svn_path_join(path, SVN_WC__ADM_TMP, pool);
while ((this = va_arg(ap, const char *)) != NULL) {
if (this[0] == '\0')
continue;
path = svn_path_join(path, this, pool);
}
if (extension)
path = apr_pstrcat(pool, path, extension, NULL);
return path;
}
static const char *
extend_with_adm_name(const char *path,
const char *extension,
svn_boolean_t use_tmp,
apr_pool_t *pool,
...) {
va_list ap;
va_start(ap, pool);
path = v_extend_with_adm_name(path, extension, use_tmp, pool, ap);
va_end(ap);
return path;
}
const char *
svn_wc__adm_path(const char *path,
svn_boolean_t tmp,
apr_pool_t *pool,
...) {
va_list ap;
va_start(ap, pool);
path = v_extend_with_adm_name(path, NULL, tmp, pool, ap);
va_end(ap);
return path;
}
svn_boolean_t
svn_wc__adm_path_exists(const char *path,
svn_boolean_t tmp,
apr_pool_t *pool,
...) {
svn_node_kind_t kind;
svn_error_t *err;
va_list ap;
va_start(ap, pool);
path = v_extend_with_adm_name(path, NULL, tmp, pool, ap);
va_end(ap);
err = svn_io_check_path(path, &kind, pool);
if (err) {
svn_error_clear(err);
return FALSE;
}
if (kind == svn_node_none)
return FALSE;
else
return TRUE;
}
svn_error_t *
svn_wc__make_adm_thing(svn_wc_adm_access_t *adm_access,
const char *thing,
svn_node_kind_t type,
apr_fileperms_t perms,
svn_boolean_t tmp,
apr_pool_t *pool) {
svn_error_t *err = SVN_NO_ERROR;
apr_file_t *f = NULL;
const char *path;
SVN_ERR(svn_wc__adm_write_check(adm_access));
path = extend_with_adm_name(svn_wc_adm_access_path(adm_access),
NULL, tmp, pool, thing, NULL);
if (type == svn_node_file) {
SVN_ERR(svn_io_file_open(&f, path,
(APR_WRITE | APR_CREATE | APR_EXCL),
perms,
pool));
SVN_ERR(svn_io_file_close(f, pool));
} else if (type == svn_node_dir) {
SVN_ERR(svn_io_dir_make(path, perms, pool));
} else {
err = svn_error_create
(0, NULL, _("Bad type indicator"));
}
return err;
}
svn_error_t *
svn_wc__make_killme(svn_wc_adm_access_t *adm_access,
svn_boolean_t adm_only,
apr_pool_t *pool) {
const char *path;
SVN_ERR(svn_wc__adm_write_check(adm_access));
path = extend_with_adm_name(svn_wc_adm_access_path(adm_access),
NULL, FALSE, pool, SVN_WC__ADM_KILLME, NULL);
return svn_io_file_create(path, adm_only ? SVN_WC__KILL_ADM_ONLY : "", pool);
}
svn_error_t *
svn_wc__check_killme(svn_wc_adm_access_t *adm_access,
svn_boolean_t *exists,
svn_boolean_t *kill_adm_only,
apr_pool_t *pool) {
const char *path;
svn_error_t *err;
svn_stringbuf_t *contents;
path = extend_with_adm_name(svn_wc_adm_access_path(adm_access),
NULL, FALSE, pool, SVN_WC__ADM_KILLME, NULL);
err = svn_stringbuf_from_file(&contents, path, pool);
if (err) {
if (APR_STATUS_IS_ENOENT(err->apr_err)) {
*exists = FALSE;
svn_error_clear(err);
err = SVN_NO_ERROR;
}
return err;
}
*exists = TRUE;
*kill_adm_only = svn_string_compare_stringbuf
(svn_string_create(SVN_WC__KILL_ADM_ONLY, pool), contents);
return SVN_NO_ERROR;
}
static svn_error_t *
sync_adm_file(const char *path,
const char *extension,
apr_pool_t *pool,
...) {
const char *tmp_path;
va_list ap;
va_start(ap, pool);
tmp_path = v_extend_with_adm_name(path, extension, 1, pool, ap);
va_end(ap);
va_start(ap, pool);
path = v_extend_with_adm_name(path, extension, 0, pool, ap);
va_end(ap);
SVN_ERR(svn_io_file_rename(tmp_path, path, pool));
SVN_ERR(svn_io_set_file_read_only(path, FALSE, pool));
return SVN_NO_ERROR;
}
svn_error_t *
svn_wc__sync_text_base(const char *path, apr_pool_t *pool) {
const char *parent_path, *base_name;
svn_path_split(path, &parent_path, &base_name, pool);
return sync_adm_file(parent_path,
SVN_WC__BASE_EXT,
pool,
SVN_WC__ADM_TEXT_BASE,
base_name,
NULL);
}
const char *
svn_wc__text_base_path(const char *path,
svn_boolean_t tmp,
apr_pool_t *pool) {
const char *newpath, *base_name;
svn_path_split(path, &newpath, &base_name, pool);
return extend_with_adm_name(newpath,
SVN_WC__BASE_EXT,
tmp,
pool,
SVN_WC__ADM_TEXT_BASE,
base_name,
NULL);
}
const char *
svn_wc__text_revert_path(const char *path,
svn_boolean_t tmp,
apr_pool_t *pool) {
const char *newpath, *base_name;
svn_path_split(path, &newpath, &base_name, pool);
return extend_with_adm_name(newpath,
SVN_WC__REVERT_EXT,
tmp,
pool,
SVN_WC__ADM_TEXT_BASE,
base_name,
NULL);
}
svn_error_t *
svn_wc__prop_path(const char **prop_path,
const char *path,
svn_node_kind_t node_kind,
svn_wc__props_kind_t props_kind,
svn_boolean_t tmp,
apr_pool_t *pool) {
if (node_kind == svn_node_dir) {
static const char * names[] = {
SVN_WC__ADM_DIR_PROP_BASE,
SVN_WC__ADM_DIR_PROP_REVERT,
SVN_WC__ADM_DIR_WCPROPS,
SVN_WC__ADM_DIR_PROPS
};
*prop_path = extend_with_adm_name
(path,
NULL,
tmp,
pool,
names[props_kind],
NULL);
} else {
static const char * extensions[] = {
SVN_WC__BASE_EXT,
SVN_WC__REVERT_EXT,
SVN_WC__WORK_EXT,
SVN_WC__WORK_EXT
};
static const char * dirs[] = {
SVN_WC__ADM_PROP_BASE,
SVN_WC__ADM_PROP_BASE,
SVN_WC__ADM_WCPROPS,
SVN_WC__ADM_PROPS
};
const char *base_name;
svn_path_split(path, prop_path, &base_name, pool);
*prop_path = extend_with_adm_name
(*prop_path,
extensions[props_kind],
tmp,
pool,
dirs[props_kind],
base_name,
NULL);
}
return SVN_NO_ERROR;
}
static svn_error_t *
open_adm_file(apr_file_t **handle,
const char *path,
const char *extension,
apr_fileperms_t protection,
apr_int32_t flags,
apr_pool_t *pool,
...) {
svn_error_t *err = SVN_NO_ERROR;
va_list ap;
if (flags & APR_WRITE) {
if (flags & APR_APPEND) {
return svn_error_create
(SVN_ERR_UNSUPPORTED_FEATURE, NULL,
_("APR_APPEND not supported for adm files"));
}
flags |= APR_EXCL | APR_CREATE;
va_start(ap, pool);
path = v_extend_with_adm_name(path, extension, 1, pool, ap);
va_end(ap);
} else {
va_start(ap, pool);
path = v_extend_with_adm_name(path, extension, 0, pool, ap);
va_end(ap);
}
err = svn_io_file_open(handle, path, flags, protection, pool);
if ((flags & APR_WRITE) && err && APR_STATUS_IS_EEXIST(err->apr_err)) {
svn_error_clear(err);
SVN_ERR(svn_io_remove_file(path, pool));
err = svn_io_file_open(handle, path, flags, protection, pool);
}
if (err) {
*handle = NULL;
if (APR_STATUS_IS_ENOENT(err->apr_err) && (flags & APR_WRITE)) {
err = svn_error_quick_wrap(err,
_("Your .svn/tmp directory may be missing or "
"corrupt; run 'svn cleanup' and try again"));
}
}
return err;
}
static svn_error_t *
close_adm_file(apr_file_t *fp,
const char *path,
const char *extension,
svn_boolean_t sync,
apr_pool_t *pool,
...) {
const char *tmp_path;
va_list ap;
va_start(ap, pool);
tmp_path = v_extend_with_adm_name(path, extension, sync, pool, ap);
va_end(ap);
SVN_ERR(svn_io_file_close(fp, pool));
if (sync) {
va_start(ap, pool);
path = v_extend_with_adm_name(path, extension, 0, pool, ap);
va_end(ap);
SVN_ERR(svn_io_file_rename(tmp_path, path, pool));
SVN_ERR(svn_io_set_file_read_only(path, FALSE, pool));
return SVN_NO_ERROR;
}
return SVN_NO_ERROR;
}
svn_error_t *
svn_wc__open_adm_file(apr_file_t **handle,
const char *path,
const char *fname,
apr_int32_t flags,
apr_pool_t *pool) {
return open_adm_file(handle, path, NULL, APR_OS_DEFAULT, flags, pool,
fname, NULL);
}
svn_error_t *
svn_wc__close_adm_file(apr_file_t *fp,
const char *path,
const char *fname,
int sync,
apr_pool_t *pool) {
return close_adm_file(fp, path, NULL, sync, pool, fname, NULL);
}
svn_error_t *
svn_wc__remove_adm_file(const char *path, apr_pool_t *pool, ...) {
va_list ap;
va_start(ap, pool);
path = v_extend_with_adm_name(path, NULL, 0, pool, ap);
va_end(ap);
SVN_ERR(svn_io_remove_file(path, pool));
return SVN_NO_ERROR;
}
svn_error_t *
svn_wc__open_text_base(apr_file_t **handle,
const char *path,
apr_int32_t flags,
apr_pool_t *pool) {
const char *parent_path, *base_name;
svn_path_split(path, &parent_path, &base_name, pool);
return open_adm_file(handle, parent_path, SVN_WC__BASE_EXT, APR_OS_DEFAULT,
flags, pool, SVN_WC__ADM_TEXT_BASE, base_name, NULL);
}
svn_error_t *
svn_wc__open_revert_base(apr_file_t **handle,
const char *path,
apr_int32_t flags,
apr_pool_t *pool) {
const char *parent_path, *base_name;
svn_path_split(path, &parent_path, &base_name, pool);
return open_adm_file(handle, parent_path, SVN_WC__REVERT_EXT, APR_OS_DEFAULT,
flags, pool, SVN_WC__ADM_TEXT_BASE, base_name, NULL);
}
svn_error_t *
svn_wc__close_text_base(apr_file_t *fp,
const char *path,
int write,
apr_pool_t *pool) {
const char *parent_path, *base_name;
svn_path_split(path, &parent_path, &base_name, pool);
return close_adm_file(fp, parent_path, SVN_WC__BASE_EXT, write, pool,
SVN_WC__ADM_TEXT_BASE, base_name, NULL);
}
svn_error_t *
svn_wc__close_revert_base(apr_file_t *fp,
const char *path,
int write,
apr_pool_t *pool) {
const char *parent_path, *base_name;
svn_path_split(path, &parent_path, &base_name, pool);
return close_adm_file(fp, parent_path, SVN_WC__REVERT_EXT, write, pool,
SVN_WC__ADM_TEXT_BASE, base_name, NULL);
}
svn_error_t *
svn_wc__open_props(apr_file_t **handle,
const char *path,
svn_node_kind_t kind,
apr_int32_t flags,
svn_boolean_t base,
svn_boolean_t wcprops,
apr_pool_t *pool) {
const char *parent_dir, *base_name;
int wc_format_version;
if (kind == svn_node_dir)
parent_dir = path;
else
svn_path_split(path, &parent_dir, &base_name, pool);
SVN_ERR(svn_wc_check_wc(parent_dir, &wc_format_version, pool));
if (wc_format_version == 0)
return svn_error_createf
(SVN_ERR_WC_OBSTRUCTED_UPDATE, NULL,
_("'%s' is not a working copy"),
svn_path_local_style(parent_dir, pool));
if (base && wcprops)
return svn_error_create(SVN_ERR_WC_PATH_NOT_FOUND, NULL,
_("No such thing as 'base' "
"working copy properties!"));
else if (base) {
if (kind == svn_node_dir)
return open_adm_file(handle, parent_dir, NULL, APR_OS_DEFAULT, flags,
pool, SVN_WC__ADM_DIR_PROP_BASE, NULL);
else
return open_adm_file(handle, parent_dir, SVN_WC__BASE_EXT,
APR_OS_DEFAULT, flags, pool,
SVN_WC__ADM_PROP_BASE, base_name, NULL);
} else if (wcprops) {
if (kind == svn_node_dir)
return open_adm_file(handle, parent_dir, NULL, APR_OS_DEFAULT, flags,
pool, SVN_WC__ADM_DIR_WCPROPS, NULL);
else {
return open_adm_file
(handle, parent_dir,
SVN_WC__WORK_EXT, APR_OS_DEFAULT,
flags, pool, SVN_WC__ADM_WCPROPS, base_name, NULL);
}
} else {
if (kind == svn_node_dir)
return open_adm_file(handle, parent_dir, NULL, APR_OS_DEFAULT, flags,
pool, SVN_WC__ADM_DIR_PROPS, NULL);
else {
return open_adm_file
(handle, parent_dir,
SVN_WC__WORK_EXT, APR_OS_DEFAULT,
flags, pool, SVN_WC__ADM_PROPS, base_name, NULL);
}
}
}
svn_error_t *
svn_wc__close_props(apr_file_t *fp,
const char *path,
svn_node_kind_t kind,
svn_boolean_t base,
svn_boolean_t wcprops,
int sync,
apr_pool_t *pool) {
const char *parent_dir, *base_name;
if (kind == svn_node_dir)
parent_dir = path;
else
svn_path_split(path, &parent_dir, &base_name, pool);
if (base && wcprops)
return svn_error_create(SVN_ERR_WC_PATH_NOT_FOUND, NULL,
_("No such thing as 'base' "
"working copy properties!"));
else if (base) {
if (kind == svn_node_dir)
return close_adm_file(fp, parent_dir, NULL, sync, pool,
SVN_WC__ADM_DIR_PROP_BASE, NULL);
else
return close_adm_file(fp, parent_dir, SVN_WC__BASE_EXT, sync, pool,
SVN_WC__ADM_PROP_BASE, base_name, NULL);
} else if (wcprops) {
if (kind == svn_node_dir)
return close_adm_file(fp, parent_dir, NULL, sync, pool,
SVN_WC__ADM_DIR_WCPROPS, NULL);
else
return close_adm_file
(fp, parent_dir,
SVN_WC__WORK_EXT,
sync, pool, SVN_WC__ADM_WCPROPS, base_name, NULL);
} else {
if (kind == svn_node_dir)
return close_adm_file(fp, parent_dir, NULL, sync, pool,
SVN_WC__ADM_DIR_PROPS, NULL);
else
return close_adm_file
(fp, parent_dir,
SVN_WC__WORK_EXT,
sync, pool, SVN_WC__ADM_PROPS, base_name, NULL);
}
}
static svn_error_t *
check_adm_exists(svn_boolean_t *exists,
const char *path,
const char *url,
svn_revnum_t revision,
apr_pool_t *pool) {
svn_error_t *err = SVN_NO_ERROR;
svn_wc_adm_access_t *adm_access;
const svn_wc_entry_t *entry;
err = svn_wc_adm_open3(&adm_access, NULL, path, FALSE, 0,
NULL, NULL, pool);
if (err && err->apr_err == SVN_ERR_WC_NOT_DIRECTORY) {
svn_error_clear(err);
*exists = FALSE;
return SVN_NO_ERROR;
} else if (err)
return SVN_NO_ERROR;
SVN_ERR(svn_wc__entry_versioned(&entry, path, adm_access, FALSE, pool));
SVN_ERR(svn_wc_adm_close(adm_access));
if (entry->schedule != svn_wc_schedule_delete) {
if (entry->revision != revision)
return
svn_error_createf
(SVN_ERR_WC_OBSTRUCTED_UPDATE, NULL,
_("Revision %ld doesn't match existing revision %ld in '%s'"),
revision, entry->revision, path);
if (strcmp(entry->url, url) != 0)
return
svn_error_createf
(SVN_ERR_WC_OBSTRUCTED_UPDATE, NULL,
_("URL '%s' doesn't match existing URL '%s' in '%s'"),
url, entry->url, path);
}
*exists = TRUE;
return SVN_NO_ERROR;
}
static svn_error_t *
make_empty_adm(const char *path, apr_pool_t *pool) {
path = extend_with_adm_name(path, NULL, 0, pool, NULL);
SVN_ERR(svn_io_dir_make_hidden(path, APR_OS_DEFAULT, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
init_adm_tmp_area(svn_wc_adm_access_t *adm_access,
apr_pool_t *pool) {
apr_fileperms_t perms = APR_OS_DEFAULT;
SVN_ERR(svn_wc__make_adm_thing(adm_access, SVN_WC__ADM_TMP,
svn_node_dir, perms, 0, pool));
SVN_ERR(svn_wc__make_adm_thing(adm_access, SVN_WC__ADM_TEXT_BASE,
svn_node_dir, perms, 1, pool));
SVN_ERR(svn_wc__make_adm_thing(adm_access, SVN_WC__ADM_PROP_BASE,
svn_node_dir, perms, 1, pool));
SVN_ERR(svn_wc__make_adm_thing(adm_access, SVN_WC__ADM_PROPS,
svn_node_dir, perms, 1, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
init_adm(const char *path,
const char *uuid,
const char *url,
const char *repos,
svn_revnum_t initial_rev,
svn_depth_t depth,
apr_pool_t *pool) {
svn_wc_adm_access_t *adm_access;
apr_fileperms_t perms = APR_OS_DEFAULT;
SVN_ERR(make_empty_adm(path, pool));
SVN_ERR(svn_wc__adm_pre_open(&adm_access, path, pool));
SVN_ERR(svn_wc__make_adm_thing(adm_access, SVN_WC__ADM_TEXT_BASE,
svn_node_dir, perms, 0, pool));
SVN_ERR(svn_wc__make_adm_thing(adm_access, SVN_WC__ADM_PROP_BASE,
svn_node_dir, perms, 0, pool));
SVN_ERR(svn_wc__make_adm_thing(adm_access, SVN_WC__ADM_PROPS,
svn_node_dir, perms, 0, pool));
SVN_ERR(init_adm_tmp_area(adm_access, pool));
SVN_ERR(svn_wc__entries_init(path, uuid, url, repos,
initial_rev, depth, pool));
SVN_ERR(svn_io_write_version_file
(extend_with_adm_name(path, NULL, FALSE, pool,
SVN_WC__ADM_FORMAT, NULL),
SVN_WC__VERSION, pool));
SVN_ERR(svn_wc_adm_close(adm_access));
return SVN_NO_ERROR;
}
svn_error_t *
svn_wc_ensure_adm3(const char *path,
const char *uuid,
const char *url,
const char *repos,
svn_revnum_t revision,
svn_depth_t depth,
apr_pool_t *pool) {
svn_boolean_t exists_already;
SVN_ERR(check_adm_exists(&exists_already, path, url, revision, pool));
return (exists_already ? SVN_NO_ERROR :
init_adm(path, uuid, url, repos, revision, depth, pool));
}
svn_error_t *
svn_wc_ensure_adm2(const char *path,
const char *uuid,
const char *url,
const char *repos,
svn_revnum_t revision,
apr_pool_t *pool) {
return svn_wc_ensure_adm3(path, uuid, url, repos, revision,
svn_depth_infinity, pool);
}
svn_error_t *
svn_wc_ensure_adm(const char *path,
const char *uuid,
const char *url,
svn_revnum_t revision,
apr_pool_t *pool) {
return svn_wc_ensure_adm2(path, uuid, url, NULL, revision, pool);
}
svn_error_t *
svn_wc__adm_destroy(svn_wc_adm_access_t *adm_access,
apr_pool_t *pool) {
const char *path;
SVN_ERR(svn_wc__adm_write_check(adm_access));
path = extend_with_adm_name(svn_wc_adm_access_path(adm_access),
NULL, FALSE, pool, NULL);
SVN_ERR(svn_io_remove_dir2(path, FALSE, NULL, NULL, pool));
SVN_ERR(svn_wc_adm_close(adm_access));
return SVN_NO_ERROR;
}
svn_error_t *
svn_wc__adm_cleanup_tmp_area(svn_wc_adm_access_t *adm_access,
apr_pool_t *pool) {
const char *tmp_path;
SVN_ERR(svn_wc__adm_write_check(adm_access));
tmp_path = extend_with_adm_name(svn_wc_adm_access_path(adm_access),
NULL, 0, pool, SVN_WC__ADM_TMP, NULL);
SVN_ERR(svn_io_remove_dir2(tmp_path, TRUE, NULL, NULL, pool));
SVN_ERR(init_adm_tmp_area(adm_access, pool));
return SVN_NO_ERROR;
}
svn_error_t *
svn_wc_create_tmp_file2(apr_file_t **fp,
const char **new_name,
const char *path,
svn_io_file_del_t delete_when,
apr_pool_t *pool) {
apr_file_t *file;
assert(fp || new_name);
path = svn_wc__adm_path(path, TRUE, pool, "tempfile", NULL);
SVN_ERR(svn_io_open_unique_file2(&file, new_name,
path, ".tmp", delete_when, pool));
if (fp)
*fp = file;
else
SVN_ERR(svn_io_file_close(file, pool));
return SVN_NO_ERROR;
}
svn_error_t *
svn_wc_create_tmp_file(apr_file_t **fp,
const char *path,
svn_boolean_t delete_on_close,
apr_pool_t *pool) {
return svn_wc_create_tmp_file2(fp, NULL, path,
delete_on_close
? svn_io_file_del_on_close
: svn_io_file_del_none,
pool);
}
