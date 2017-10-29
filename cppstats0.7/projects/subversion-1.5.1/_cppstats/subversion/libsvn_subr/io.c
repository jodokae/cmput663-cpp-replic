#include <stdio.h>
#include <assert.h>
#if !defined(WIN32)
#include <unistd.h>
#endif
#if !defined(APR_STATUS_IS_EPERM)
#include <errno.h>
#if defined(EPERM)
#define APR_STATUS_IS_EPERM(s) ((s) == EPERM)
#else
#define APR_STATUS_IS_EPERM(s) (0)
#endif
#endif
#include <apr_lib.h>
#include <apr_pools.h>
#include <apr_file_io.h>
#include <apr_file_info.h>
#include <apr_general.h>
#include <apr_strings.h>
#include <apr_portable.h>
#include <apr_md5.h>
#include "svn_types.h"
#include "svn_path.h"
#include "svn_string.h"
#include "svn_error.h"
#include "svn_io.h"
#include "svn_pools.h"
#include "svn_utf.h"
#include "svn_config.h"
#include "svn_private_config.h"
#if defined(WIN32)
#define WIN32_RETRY_LOOP(err, expr) do { apr_status_t os_err = APR_TO_OS_ERROR(err); int sleep_count = 1000; int retries; for (retries = 0; retries < 100 && (os_err == ERROR_ACCESS_DENIED || os_err == ERROR_SHARING_VIOLATION || os_err == ERROR_DIR_NOT_EMPTY); ++retries, os_err = APR_TO_OS_ERROR(err)) { apr_sleep(sleep_count); if (sleep_count < 128000) sleep_count *= 2; (err) = (expr); } } while (0)
#else
#define WIN32_RETRY_LOOP(err, expr) ((void)0)
#endif
static void
map_apr_finfo_to_node_kind(svn_node_kind_t *kind,
svn_boolean_t *is_special,
apr_finfo_t *finfo) {
*is_special = FALSE;
if (finfo->filetype == APR_REG)
*kind = svn_node_file;
else if (finfo->filetype == APR_DIR)
*kind = svn_node_dir;
else if (finfo->filetype == APR_LNK) {
*is_special = TRUE;
*kind = svn_node_file;
} else
*kind = svn_node_unknown;
}
static svn_error_t *
io_check_path(const char *path,
svn_boolean_t resolve_symlinks,
svn_boolean_t *is_special_p,
svn_node_kind_t *kind,
apr_pool_t *pool) {
apr_int32_t flags;
apr_finfo_t finfo;
apr_status_t apr_err;
const char *path_apr;
svn_boolean_t is_special = FALSE;
if (path[0] == '\0')
path = ".";
SVN_ERR(svn_path_cstring_from_utf8(&path_apr, path, pool));
flags = resolve_symlinks ? APR_FINFO_MIN : (APR_FINFO_MIN | APR_FINFO_LINK);
apr_err = apr_stat(&finfo, path_apr, flags, pool);
if (APR_STATUS_IS_ENOENT(apr_err))
*kind = svn_node_none;
else if (APR_STATUS_IS_ENOTDIR(apr_err))
*kind = svn_node_none;
else if (apr_err)
return svn_error_wrap_apr(apr_err, _("Can't check path '%s'"),
svn_path_local_style(path, pool));
else
map_apr_finfo_to_node_kind(kind, &is_special, &finfo);
*is_special_p = is_special;
return SVN_NO_ERROR;
}
static apr_status_t
file_open(apr_file_t **f,
const char *fname,
apr_int32_t flag,
apr_fileperms_t perm,
apr_pool_t *pool) {
apr_status_t status;
#if defined(AS400)
apr_status_t apr_err;
if (flag & APR_CREATE) {
apr_err = apr_file_open(f, fname, flag & ~APR_BINARY, perm, pool);
if (apr_err)
return apr_err;
apr_file_close(*f);
flag &= ~APR_EXCL;
}
#endif
status = apr_file_open(f, fname, flag, perm, pool);
WIN32_RETRY_LOOP(status, apr_file_open(f, fname, flag, perm, pool));
return status;
}
svn_error_t *
svn_io_check_resolved_path(const char *path,
svn_node_kind_t *kind,
apr_pool_t *pool) {
svn_boolean_t ignored;
return io_check_path(path, TRUE, &ignored, kind, pool);
}
svn_error_t *
svn_io_check_path(const char *path,
svn_node_kind_t *kind,
apr_pool_t *pool) {
svn_boolean_t ignored;
return io_check_path(path, FALSE, &ignored, kind, pool);
}
svn_error_t *
svn_io_check_special_path(const char *path,
svn_node_kind_t *kind,
svn_boolean_t *is_special,
apr_pool_t *pool) {
return io_check_path(path, FALSE, is_special, kind, pool);
}
struct temp_file_cleanup_s {
apr_pool_t *pool;
const char *name;
};
static apr_status_t
temp_file_plain_cleanup_handler(void *baton) {
struct temp_file_cleanup_s *b = baton;
apr_status_t apr_err = APR_SUCCESS;
if (b->name) {
apr_err = apr_file_remove(b->name, b->pool);
WIN32_RETRY_LOOP(apr_err, apr_file_remove(b->name, b->pool));
}
return apr_err;
}
static apr_status_t
temp_file_child_cleanup_handler(void *baton) {
struct temp_file_cleanup_s *b = baton;
apr_pool_cleanup_kill(b->pool, b,
temp_file_plain_cleanup_handler);
return APR_SUCCESS;
}
svn_error_t *
svn_io_open_unique_file2(apr_file_t **f,
const char **unique_name_p,
const char *path,
const char *suffix,
svn_io_file_del_t delete_when,
apr_pool_t *pool) {
unsigned int i;
apr_file_t *file;
const char *unique_name;
const char *unique_name_apr;
struct temp_file_cleanup_s *baton = NULL;
assert(f || unique_name_p);
if (delete_when == svn_io_file_del_on_pool_cleanup) {
baton = apr_palloc(pool, sizeof(*baton));
baton->pool = pool;
baton->name = NULL;
apr_pool_cleanup_register(pool, baton, temp_file_plain_cleanup_handler,
temp_file_child_cleanup_handler);
}
for (i = 1; i <= 99999; i++) {
apr_status_t apr_err;
apr_int32_t flag = (APR_READ | APR_WRITE | APR_CREATE | APR_EXCL
| APR_BUFFERED | APR_BINARY);
if (delete_when == svn_io_file_del_on_close)
flag |= APR_DELONCLOSE;
if (i == 1)
unique_name = apr_psprintf(pool, "%s%s", path, suffix);
else
unique_name = apr_psprintf(pool, "%s.%u%s", path, i, suffix);
SVN_ERR(svn_path_cstring_from_utf8(&unique_name_apr, unique_name,
pool));
apr_err = file_open(&file, unique_name_apr, flag,
APR_OS_DEFAULT, pool);
if (APR_STATUS_IS_EEXIST(apr_err))
continue;
else if (apr_err) {
if (APR_STATUS_IS_EACCES(apr_err)) {
apr_finfo_t finfo;
apr_status_t apr_err_2 = apr_stat(&finfo, unique_name_apr,
APR_FINFO_TYPE, pool);
if (!apr_err_2
&& (finfo.filetype == APR_DIR))
continue;
}
if (f) *f = NULL;
if (unique_name_p) *unique_name_p = NULL;
return svn_error_wrap_apr(apr_err, _("Can't open '%s'"),
svn_path_local_style(unique_name, pool));
} else {
if (delete_when == svn_io_file_del_on_pool_cleanup)
baton->name = unique_name_apr;
if (f)
*f = file;
else
apr_file_close(file);
if (unique_name_p) *unique_name_p = unique_name;
return SVN_NO_ERROR;
}
}
if (f) *f = NULL;
if (unique_name_p) *unique_name_p = NULL;
return svn_error_createf(SVN_ERR_IO_UNIQUE_NAMES_EXHAUSTED,
NULL,
_("Unable to make name for '%s'"),
svn_path_local_style(path, pool));
}
svn_error_t *
svn_io_open_unique_file(apr_file_t **f,
const char **unique_name_p,
const char *path,
const char *suffix,
svn_boolean_t delete_on_close,
apr_pool_t *pool) {
return svn_io_open_unique_file2(f, unique_name_p,
path, suffix,
delete_on_close
? svn_io_file_del_on_close
: svn_io_file_del_none,
pool);
}
svn_error_t *
svn_io_create_unique_link(const char **unique_name_p,
const char *path,
const char *dest,
const char *suffix,
apr_pool_t *pool) {
#if defined(HAVE_SYMLINK)
unsigned int i;
const char *unique_name;
const char *unique_name_apr;
const char *dest_apr;
int rv;
#if defined(AS400_UTF8)
const char *dest_apr_ebcdic;
#endif
SVN_ERR(svn_path_cstring_from_utf8(&dest_apr, dest, pool));
#if defined(AS400_UTF8)
SVN_ERR(svn_utf_cstring_from_utf8_ex2(&dest_apr_ebcdic, dest_apr,
(const char*)0, pool));
dest_apr = dest_apr_ebcdic;
#endif
for (i = 1; i <= 99999; i++) {
apr_status_t apr_err;
if (i == 1)
unique_name = apr_psprintf(pool, "%s%s", path, suffix);
else
unique_name = apr_psprintf(pool, "%s.%u%s", path, i, suffix);
#if !defined(AS400_UTF8)
SVN_ERR(svn_path_cstring_from_utf8(&unique_name_apr, unique_name,
pool));
#else
SVN_ERR(svn_utf_cstring_from_utf8_ex2(&unique_name_apr, unique_name,
(const char*)0, pool));
#endif
do {
rv = symlink(dest_apr, unique_name_apr);
} while (rv == -1 && APR_STATUS_IS_EINTR(apr_get_os_error()));
apr_err = apr_get_os_error();
if (rv == -1 && APR_STATUS_IS_EEXIST(apr_err))
continue;
else if (rv == -1 && apr_err) {
if (APR_STATUS_IS_EACCES(apr_err)) {
apr_finfo_t finfo;
apr_status_t apr_err_2 = apr_stat(&finfo, unique_name_apr,
APR_FINFO_TYPE, pool);
if (!apr_err_2
&& (finfo.filetype == APR_DIR))
continue;
}
*unique_name_p = NULL;
return svn_error_wrap_apr(apr_err,
_("Can't create symbolic link '%s'"),
svn_path_local_style(unique_name, pool));
} else {
*unique_name_p = unique_name;
return SVN_NO_ERROR;
}
}
*unique_name_p = NULL;
return svn_error_createf(SVN_ERR_IO_UNIQUE_NAMES_EXHAUSTED,
NULL,
_("Unable to make name for '%s'"),
svn_path_local_style(path, pool));
#else
return svn_error_create(SVN_ERR_UNSUPPORTED_FEATURE, NULL,
_("Symbolic links are not supported on this "
"platform"));
#endif
}
svn_error_t *
svn_io_read_link(svn_string_t **dest,
const char *path,
apr_pool_t *pool) {
#if defined(HAVE_READLINK)
svn_string_t dest_apr;
const char *path_apr;
char buf[1025];
int rv;
#if defined(AS400_UTF8)
const char *buf_utf8;
#endif
#if !defined(AS400_UTF8)
SVN_ERR(svn_path_cstring_from_utf8(&path_apr, path, pool));
#else
SVN_ERR(svn_utf_cstring_from_utf8_ex2(&path_apr, path, (const char*)0,
pool));
#endif
do {
rv = readlink(path_apr, buf, sizeof(buf) - 1);
} while (rv == -1 && APR_STATUS_IS_EINTR(apr_get_os_error()));
if (rv == -1)
return svn_error_wrap_apr
(apr_get_os_error(), _("Can't read contents of link"));
buf[rv] = '\0';
dest_apr.data = buf;
dest_apr.len = rv;
#if !defined(AS400_UTF8)
SVN_ERR(svn_utf_string_to_utf8((const svn_string_t **)dest, &dest_apr,
pool));
#else
SVN_ERR(svn_utf_cstring_to_utf8_ex2(&buf_utf8, dest_apr.data,
(const char *)0, pool));
*dest = svn_string_create(buf_utf8, pool);
#endif
return SVN_NO_ERROR;
#else
return svn_error_create(SVN_ERR_UNSUPPORTED_FEATURE, NULL,
_("Symbolic links are not supported on this "
"platform"));
#endif
}
svn_error_t *
svn_io_copy_link(const char *src,
const char *dst,
apr_pool_t *pool)
{
#if defined(HAVE_READLINK)
svn_string_t *link_dest;
const char *dst_tmp;
SVN_ERR(svn_io_read_link(&link_dest, src, pool));
SVN_ERR(svn_io_create_unique_link(&dst_tmp, dst, link_dest->data,
".tmp", pool));
return svn_io_file_rename(dst_tmp, dst, pool);
#else
return svn_error_create(SVN_ERR_UNSUPPORTED_FEATURE, NULL,
_("Symbolic links are not supported on this "
"platform"));
#endif
}
svn_error_t *
svn_io_temp_dir(const char **dir,
apr_pool_t *pool) {
apr_status_t apr_err = apr_temp_dir_get(dir, pool);
if (apr_err)
return svn_error_wrap_apr(apr_err, _("Can't find a temporary directory"));
*dir = svn_path_canonicalize(*dir, pool);
return svn_path_cstring_to_utf8(dir, *dir, pool);
}
static apr_status_t
copy_contents(apr_file_t *from_file,
apr_file_t *to_file,
apr_pool_t *pool) {
while (1) {
char buf[SVN__STREAM_CHUNK_SIZE];
apr_size_t bytes_this_time = sizeof(buf);
apr_status_t read_err;
apr_status_t write_err;
read_err = apr_file_read(from_file, buf, &bytes_this_time);
if (read_err && !APR_STATUS_IS_EOF(read_err)) {
return read_err;
}
write_err = apr_file_write_full(to_file, buf, bytes_this_time, NULL);
if (write_err) {
return write_err;
}
if (read_err && APR_STATUS_IS_EOF(read_err)) {
return APR_SUCCESS;
}
}
}
svn_error_t *
svn_io_copy_file(const char *src,
const char *dst,
svn_boolean_t copy_perms,
apr_pool_t *pool) {
apr_file_t *from_file, *to_file;
apr_status_t apr_err;
const char *src_apr, *dst_tmp_apr;
const char *dst_tmp;
svn_error_t *err, *err2;
SVN_ERR(svn_path_cstring_from_utf8(&src_apr, src, pool));
SVN_ERR(svn_io_file_open(&from_file, src, APR_READ | APR_BINARY,
APR_OS_DEFAULT, pool));
SVN_ERR(svn_io_open_unique_file2(&to_file, &dst_tmp, dst, ".tmp",
svn_io_file_del_none, pool));
SVN_ERR(svn_path_cstring_from_utf8(&dst_tmp_apr, dst_tmp, pool));
apr_err = copy_contents(from_file, to_file, pool);
if (apr_err) {
err = svn_error_wrap_apr
(apr_err, _("Can't copy '%s' to '%s'"),
svn_path_local_style(src, pool),
svn_path_local_style(dst_tmp, pool));
} else
err = NULL;
err2 = svn_io_file_close(from_file, pool);
if (! err)
err = err2;
else
svn_error_clear(err2);
err2 = svn_io_file_close(to_file, pool);
if (! err)
err = err2;
else
svn_error_clear(err2);
if (err) {
apr_err = apr_file_remove(dst_tmp_apr, pool);
WIN32_RETRY_LOOP(apr_err, apr_file_remove(dst_tmp_apr, pool));
return err;
}
#if !defined(WIN32)
if (copy_perms) {
apr_file_t *s;
apr_finfo_t finfo;
SVN_ERR(svn_io_file_open(&s, src, APR_READ, APR_OS_DEFAULT, pool));
SVN_ERR(svn_io_file_info_get(&finfo, APR_FINFO_PROT, s, pool));
SVN_ERR(svn_io_file_close(s, pool));
apr_err = apr_file_perms_set(dst_tmp_apr, finfo.protection);
if ((apr_err != APR_SUCCESS)
&& (apr_err != APR_INCOMPLETE)
&& (apr_err != APR_ENOTIMPL)) {
return svn_error_wrap_apr
(apr_err, _("Can't set permissions on '%s'"),
svn_path_local_style(dst_tmp, pool));
}
}
#endif
return svn_io_file_rename(dst_tmp, dst, pool);
}
svn_error_t *
svn_io_append_file(const char *src, const char *dst, apr_pool_t *pool) {
apr_status_t apr_err;
const char *src_apr, *dst_apr;
SVN_ERR(svn_path_cstring_from_utf8(&src_apr, src, pool));
SVN_ERR(svn_path_cstring_from_utf8(&dst_apr, dst, pool));
apr_err = apr_file_append(src_apr, dst_apr, APR_OS_DEFAULT, pool);
if (apr_err)
return svn_error_wrap_apr(apr_err, _("Can't append '%s' to '%s'"),
svn_path_local_style(src, pool),
svn_path_local_style(dst, pool));
return SVN_NO_ERROR;
}
svn_error_t *svn_io_copy_dir_recursively(const char *src,
const char *dst_parent,
const char *dst_basename,
svn_boolean_t copy_perms,
svn_cancel_func_t cancel_func,
void *cancel_baton,
apr_pool_t *pool) {
svn_node_kind_t kind;
apr_status_t status;
const char *dst_path;
apr_dir_t *this_dir;
apr_finfo_t this_entry;
apr_int32_t flags = APR_FINFO_TYPE | APR_FINFO_NAME;
apr_pool_t *subpool = svn_pool_create(pool);
dst_path = svn_path_join(dst_parent, dst_basename, pool);
SVN_ERR(svn_io_check_path(src, &kind, subpool));
if (kind != svn_node_dir)
return svn_error_createf(SVN_ERR_NODE_UNEXPECTED_KIND, NULL,
_("Source '%s' is not a directory"),
svn_path_local_style(src, pool));
SVN_ERR(svn_io_check_path(dst_parent, &kind, subpool));
if (kind != svn_node_dir)
return svn_error_createf(SVN_ERR_NODE_UNEXPECTED_KIND, NULL,
_("Destination '%s' is not a directory"),
svn_path_local_style(dst_parent, pool));
SVN_ERR(svn_io_check_path(dst_path, &kind, subpool));
if (kind != svn_node_none)
return svn_error_createf(SVN_ERR_ENTRY_EXISTS, NULL,
_("Destination '%s' already exists"),
svn_path_local_style(dst_path, pool));
SVN_ERR(svn_io_dir_make(dst_path, APR_OS_DEFAULT, pool));
SVN_ERR(svn_io_dir_open(&this_dir, src, subpool));
for (status = apr_dir_read(&this_entry, flags, this_dir);
status == APR_SUCCESS;
status = apr_dir_read(&this_entry, flags, this_dir)) {
if ((this_entry.name[0] == '.')
&& ((this_entry.name[1] == '\0')
|| ((this_entry.name[1] == '.')
&& (this_entry.name[2] == '\0')))) {
continue;
} else {
const char *src_target, *entryname_utf8;
if (cancel_func)
SVN_ERR(cancel_func(cancel_baton));
SVN_ERR(svn_path_cstring_to_utf8(&entryname_utf8,
this_entry.name, subpool));
src_target = svn_path_join(src, entryname_utf8, subpool);
if (this_entry.filetype == APR_REG) {
const char *dst_target = svn_path_join(dst_path, entryname_utf8,
subpool);
SVN_ERR(svn_io_copy_file(src_target, dst_target,
copy_perms, subpool));
} else if (this_entry.filetype == APR_LNK) {
const char *dst_target = svn_path_join(dst_path, entryname_utf8,
subpool);
SVN_ERR(svn_io_copy_link(src_target, dst_target,
subpool));
} else if (this_entry.filetype == APR_DIR) {
if (strcmp(src, dst_parent) == 0
&& strcmp(entryname_utf8, dst_basename) == 0)
continue;
SVN_ERR(svn_io_copy_dir_recursively
(src_target,
dst_path,
entryname_utf8,
copy_perms,
cancel_func,
cancel_baton,
subpool));
}
}
}
if (! (APR_STATUS_IS_ENOENT(status)))
return svn_error_wrap_apr(status, _("Can't read directory '%s'"),
svn_path_local_style(src, pool));
status = apr_dir_close(this_dir);
if (status)
return svn_error_wrap_apr(status, _("Error closing directory '%s'"),
svn_path_local_style(src, pool));
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
svn_error_t *
svn_io_make_dir_recursively(const char *path, apr_pool_t *pool) {
const char *path_apr;
apr_status_t apr_err;
if (svn_path_is_empty(path))
return SVN_NO_ERROR;
SVN_ERR(svn_path_cstring_from_utf8(&path_apr, path, pool));
apr_err = apr_dir_make_recursive(path_apr, APR_OS_DEFAULT, pool);
WIN32_RETRY_LOOP(apr_err, apr_dir_make_recursive(path_apr,
APR_OS_DEFAULT, pool));
if (apr_err)
return svn_error_wrap_apr(apr_err, _("Can't make directory '%s'"),
svn_path_local_style(path, pool));
return SVN_NO_ERROR;
}
svn_error_t *svn_io_file_create(const char *file,
const char *contents,
apr_pool_t *pool) {
apr_file_t *f;
apr_size_t written;
SVN_ERR(svn_io_file_open(&f, file,
(APR_WRITE | APR_CREATE | APR_EXCL),
APR_OS_DEFAULT,
pool));
SVN_ERR(svn_io_file_write_full(f, contents, strlen(contents),
&written, pool));
SVN_ERR(svn_io_file_close(f, pool));
return SVN_NO_ERROR;
}
svn_error_t *svn_io_dir_file_copy(const char *src_path,
const char *dest_path,
const char *file,
apr_pool_t *pool) {
const char *file_dest_path = svn_path_join(dest_path, file, pool);
const char *file_src_path = svn_path_join(src_path, file, pool);
SVN_ERR(svn_io_copy_file(file_src_path, file_dest_path, TRUE, pool));
return SVN_NO_ERROR;
}
svn_error_t *
svn_io_file_affected_time(apr_time_t *apr_time,
const char *path,
apr_pool_t *pool) {
apr_finfo_t finfo;
SVN_ERR(svn_io_stat(&finfo, path, APR_FINFO_MIN | APR_FINFO_LINK, pool));
*apr_time = finfo.mtime;
return SVN_NO_ERROR;
}
svn_error_t *
svn_io_set_file_affected_time(apr_time_t apr_time,
const char *path,
apr_pool_t *pool) {
apr_status_t status;
const char *native_path;
#if defined(AS400)
apr_utimbuf_t aubuf;
apr_finfo_t finfo;
#endif
SVN_ERR(svn_path_cstring_from_utf8(&native_path, path, pool));
#if !defined(AS400)
status = apr_file_mtime_set(native_path, apr_time, pool);
#else
status = apr_stat(&finfo, native_path, APR_FINFO_ATIME, pool);
if (!status) {
aubuf.atime = finfo.atime;
aubuf.mtime = apr_time;
status = apr_utime(native_path, &aubuf);
}
#endif
if (status)
return svn_error_wrap_apr
(status, _("Can't set access time of '%s'"),
svn_path_local_style(path, pool));
return SVN_NO_ERROR;
}
svn_error_t *
svn_io_filesizes_different_p(svn_boolean_t *different_p,
const char *file1,
const char *file2,
apr_pool_t *pool) {
apr_finfo_t finfo1;
apr_finfo_t finfo2;
apr_status_t status;
const char *file1_apr, *file2_apr;
SVN_ERR(svn_path_cstring_from_utf8(&file1_apr, file1, pool));
SVN_ERR(svn_path_cstring_from_utf8(&file2_apr, file2, pool));
status = apr_stat(&finfo1, file1_apr, APR_FINFO_MIN, pool);
if (status) {
*different_p = FALSE;
return SVN_NO_ERROR;
}
status = apr_stat(&finfo2, file2_apr, APR_FINFO_MIN, pool);
if (status) {
*different_p = FALSE;
return SVN_NO_ERROR;
}
if (finfo1.size == finfo2.size)
*different_p = FALSE;
else
*different_p = TRUE;
return SVN_NO_ERROR;
}
svn_error_t *
svn_io_file_checksum(unsigned char digest[],
const char *file,
apr_pool_t *pool) {
struct apr_md5_ctx_t context;
apr_file_t *f = NULL;
svn_error_t *err;
char *buf = apr_palloc(pool, SVN__STREAM_CHUNK_SIZE);
apr_size_t len;
apr_md5_init(&context);
SVN_ERR(svn_io_file_open(&f, file, APR_READ, APR_OS_DEFAULT, pool));
len = SVN__STREAM_CHUNK_SIZE;
err = svn_io_file_read(f, buf, &len, pool);
while (! err) {
apr_md5_update(&context, buf, len);
len = SVN__STREAM_CHUNK_SIZE;
err = svn_io_file_read(f, buf, &len, pool);
};
if (err && ! APR_STATUS_IS_EOF(err->apr_err))
return err;
svn_error_clear(err);
SVN_ERR(svn_io_file_close(f, pool));
apr_md5_final(digest, &context);
return SVN_NO_ERROR;
}
#if !defined(WIN32)
static svn_error_t *
reown_file(const char *path_apr,
apr_pool_t *pool) {
const char *unique_name;
SVN_ERR(svn_io_open_unique_file2(NULL, &unique_name, path_apr,
".tmp", svn_io_file_del_none, pool));
SVN_ERR(svn_io_file_rename(path_apr, unique_name, pool));
SVN_ERR(svn_io_copy_file(unique_name, path_apr, TRUE, pool));
SVN_ERR(svn_io_remove_file(unique_name, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
get_default_file_perms(const char *path, apr_fileperms_t *perms,
apr_pool_t *pool) {
apr_status_t status;
apr_finfo_t tmp_finfo, finfo;
apr_file_t *fd;
const char *tmp_path;
const char *apr_path;
SVN_ERR(svn_io_open_unique_file2(&fd, &tmp_path, path,
".tmp", svn_io_file_del_on_close, pool));
status = apr_stat(&tmp_finfo, tmp_path, APR_FINFO_PROT, pool);
if (status)
return svn_error_wrap_apr(status, _("Can't get default file perms "
"for file at '%s' (file stat error)"),
path);
apr_file_close(fd);
SVN_ERR(svn_path_cstring_from_utf8(&apr_path, path, pool));
status = apr_file_open(&fd, apr_path, APR_READ | APR_BINARY,
APR_OS_DEFAULT, pool);
if (status)
return svn_error_wrap_apr(status, _("Can't open file at '%s'"), path);
status = apr_stat(&finfo, apr_path, APR_FINFO_PROT, pool);
if (status)
return svn_error_wrap_apr(status, _("Can't get file perms for file at "
"'%s' (file stat error)"), path);
apr_file_close(fd);
*perms = tmp_finfo.protection | finfo.protection;
return SVN_NO_ERROR;
}
static svn_error_t *
io_set_file_perms(const char *path,
svn_boolean_t change_readwrite,
svn_boolean_t enable_write,
svn_boolean_t change_executable,
svn_boolean_t executable,
svn_boolean_t ignore_enoent,
apr_pool_t *pool) {
apr_status_t status;
const char *path_apr;
apr_finfo_t finfo;
apr_fileperms_t perms_to_set;
SVN_ERR(svn_path_cstring_from_utf8(&path_apr, path, pool));
status = apr_stat(&finfo, path_apr, APR_FINFO_PROT | APR_FINFO_LINK, pool);
if (status) {
if (ignore_enoent && APR_STATUS_IS_ENOENT(status))
return SVN_NO_ERROR;
else if (status != APR_ENOTIMPL)
return svn_error_wrap_apr(status,
_("Can't change perms of file '%s'"),
svn_path_local_style(path, pool));
return SVN_NO_ERROR;
}
if (finfo.filetype == APR_LNK)
return SVN_NO_ERROR;
perms_to_set = finfo.protection;
if (change_readwrite) {
if (enable_write)
SVN_ERR(get_default_file_perms(path, &perms_to_set, pool));
else {
if (finfo.protection & APR_UREAD)
perms_to_set &= ~APR_UWRITE;
if (finfo.protection & APR_GREAD)
perms_to_set &= ~APR_GWRITE;
if (finfo.protection & APR_WREAD)
perms_to_set &= ~APR_WWRITE;
}
}
if (change_executable) {
if (executable) {
if (finfo.protection & APR_UREAD)
perms_to_set |= APR_UEXECUTE;
if (finfo.protection & APR_GREAD)
perms_to_set |= APR_GEXECUTE;
if (finfo.protection & APR_WREAD)
perms_to_set |= APR_WEXECUTE;
} else {
if (finfo.protection & APR_UREAD)
perms_to_set &= ~APR_UEXECUTE;
if (finfo.protection & APR_GREAD)
perms_to_set &= ~APR_GEXECUTE;
if (finfo.protection & APR_WREAD)
perms_to_set &= ~APR_WEXECUTE;
}
}
if (perms_to_set == finfo.protection)
return SVN_NO_ERROR;
status = apr_file_perms_set(path_apr, perms_to_set);
if (!status)
return SVN_NO_ERROR;
if (APR_STATUS_IS_EPERM(status)) {
SVN_ERR(reown_file(path_apr, pool));
status = apr_file_perms_set(path_apr, perms_to_set);
}
if (!status)
return SVN_NO_ERROR;
if (ignore_enoent && APR_STATUS_IS_ENOENT(status))
return SVN_NO_ERROR;
else if (status == APR_ENOTIMPL) {
apr_fileattrs_t attrs = 0;
apr_fileattrs_t attrs_values = 0;
if (change_readwrite) {
attrs = APR_FILE_ATTR_READONLY;
if (!enable_write)
attrs_values = APR_FILE_ATTR_READONLY;
}
if (change_executable) {
attrs = APR_FILE_ATTR_EXECUTABLE;
if (executable)
attrs_values = APR_FILE_ATTR_EXECUTABLE;
}
status = apr_file_attrs_set(path_apr, attrs, attrs_values, pool);
}
return svn_error_wrap_apr(status,
_("Can't change perms of file '%s'"),
svn_path_local_style(path, pool));
}
#endif
svn_error_t *
svn_io_set_file_read_write_carefully(const char *path,
svn_boolean_t enable_write,
svn_boolean_t ignore_enoent,
apr_pool_t *pool) {
if (enable_write)
return svn_io_set_file_read_write(path, ignore_enoent, pool);
return svn_io_set_file_read_only(path, ignore_enoent, pool);
}
svn_error_t *
svn_io_set_file_read_only(const char *path,
svn_boolean_t ignore_enoent,
apr_pool_t *pool) {
#if !defined(WIN32)
return io_set_file_perms(path, TRUE, FALSE, FALSE, FALSE,
ignore_enoent, pool);
#else
apr_status_t status;
const char *path_apr;
SVN_ERR(svn_path_cstring_from_utf8(&path_apr, path, pool));
status = apr_file_attrs_set(path_apr,
APR_FILE_ATTR_READONLY,
APR_FILE_ATTR_READONLY,
pool);
if (status && status != APR_ENOTIMPL)
if (!ignore_enoent || !APR_STATUS_IS_ENOENT(status))
return svn_error_wrap_apr(status,
_("Can't set file '%s' read-only"),
svn_path_local_style(path, pool));
return SVN_NO_ERROR;
#endif
}
svn_error_t *
svn_io_set_file_read_write(const char *path,
svn_boolean_t ignore_enoent,
apr_pool_t *pool) {
#if !defined(WIN32)
return io_set_file_perms(path, TRUE, TRUE, FALSE, FALSE,
ignore_enoent, pool);
#else
apr_status_t status;
const char *path_apr;
SVN_ERR(svn_path_cstring_from_utf8(&path_apr, path, pool));
status = apr_file_attrs_set(path_apr,
0,
APR_FILE_ATTR_READONLY,
pool);
if (status && status != APR_ENOTIMPL)
if (!ignore_enoent || !APR_STATUS_IS_ENOENT(status))
return svn_error_wrap_apr(status,
_("Can't set file '%s' read-write"),
svn_path_local_style(path, pool));
return SVN_NO_ERROR;
#endif
}
svn_error_t *
svn_io_set_file_executable(const char *path,
svn_boolean_t executable,
svn_boolean_t ignore_enoent,
apr_pool_t *pool) {
#if !defined(WIN32)
return io_set_file_perms(path, FALSE, FALSE, TRUE, executable,
ignore_enoent, pool);
#else
return SVN_NO_ERROR;
#endif
}
svn_error_t *
svn_io_is_file_executable(svn_boolean_t *executable,
const char *path,
apr_pool_t *pool) {
#if defined(APR_HAS_USER) && !defined(WIN32)
apr_finfo_t file_info;
apr_status_t apr_err;
apr_uid_t uid;
apr_gid_t gid;
*executable = FALSE;
SVN_ERR(svn_io_stat(&file_info, path,
(APR_FINFO_PROT | APR_FINFO_OWNER),
pool));
apr_err = apr_uid_current(&uid, &gid, pool);
if (apr_err)
return svn_error_wrap_apr(apr_err, _("Error getting UID of process"));
if (apr_uid_compare(uid, file_info.user) == APR_SUCCESS)
*executable = (file_info.protection & APR_UEXECUTE);
else if (apr_gid_compare(gid, file_info.group) == APR_SUCCESS)
*executable = (file_info.protection & APR_GEXECUTE);
else
*executable = (file_info.protection & APR_WEXECUTE);
#else
*executable = FALSE;
#endif
return SVN_NO_ERROR;
}
static apr_status_t
svn_io__file_clear_and_close(void *arg) {
apr_status_t apr_err;
apr_file_t *f = arg;
apr_err = apr_file_unlock(f);
if (apr_err)
return apr_err;
apr_err = apr_file_close(f);
if (apr_err)
return apr_err;
return 0;
}
svn_error_t *svn_io_file_lock(const char *lock_file,
svn_boolean_t exclusive,
apr_pool_t *pool) {
return svn_io_file_lock2(lock_file, exclusive, FALSE, pool);
}
svn_error_t *svn_io_file_lock2(const char *lock_file,
svn_boolean_t exclusive,
svn_boolean_t nonblocking,
apr_pool_t *pool) {
int locktype = APR_FLOCK_SHARED;
apr_file_t *lockfile_handle;
apr_int32_t flags;
apr_status_t apr_err;
if (exclusive == TRUE)
locktype = APR_FLOCK_EXCLUSIVE;
flags = APR_READ;
if (locktype == APR_FLOCK_EXCLUSIVE)
flags |= APR_WRITE;
if (nonblocking == TRUE)
locktype |= APR_FLOCK_NONBLOCK;
SVN_ERR(svn_io_file_open(&lockfile_handle, lock_file, flags,
APR_OS_DEFAULT,
pool));
apr_err = apr_file_lock(lockfile_handle, locktype);
if (apr_err) {
switch (locktype & APR_FLOCK_TYPEMASK) {
case APR_FLOCK_SHARED:
return svn_error_wrap_apr
(apr_err, _("Can't get shared lock on file '%s'"),
svn_path_local_style(lock_file, pool));
case APR_FLOCK_EXCLUSIVE:
return svn_error_wrap_apr
(apr_err, _("Can't get exclusive lock on file '%s'"),
svn_path_local_style(lock_file, pool));
default:
abort();
}
}
apr_pool_cleanup_register(pool, lockfile_handle,
svn_io__file_clear_and_close,
apr_pool_cleanup_null);
return SVN_NO_ERROR;
}
static svn_error_t *
do_io_file_wrapper_cleanup(apr_file_t *file, apr_status_t status,
const char *msg, const char *msg_no_name,
apr_pool_t *pool);
svn_error_t *svn_io_file_flush_to_disk(apr_file_t *file,
apr_pool_t *pool) {
apr_os_file_t filehand;
SVN_ERR(do_io_file_wrapper_cleanup(file, apr_file_flush(file),
N_("Can't flush file '%s'"),
N_("Can't flush stream"),
pool));
apr_os_file_get(&filehand, file);
{
#if defined(WIN32)
if (! FlushFileBuffers(filehand))
return svn_error_wrap_apr
(apr_get_os_error(), _("Can't flush file to disk"));
#else
int rv;
do {
rv = fsync(filehand);
} while (rv == -1 && APR_STATUS_IS_EINTR(apr_get_os_error()));
if (rv == -1 && APR_STATUS_IS_EINVAL(apr_get_os_error()))
return SVN_NO_ERROR;
if (rv == -1)
return svn_error_wrap_apr
(apr_get_os_error(), _("Can't flush file to disk"));
#endif
}
return SVN_NO_ERROR;
}
svn_error_t *
svn_stringbuf_from_file2(svn_stringbuf_t **result,
const char *filename,
apr_pool_t *pool) {
apr_file_t *f = NULL;
if (filename[0] == '-' && filename[1] == '\0') {
apr_status_t apr_err;
if ((apr_err = apr_file_open_stdin(&f, pool)))
return svn_error_wrap_apr(apr_err, _("Can't open stdin"));
} else {
SVN_ERR(svn_io_file_open(&f, filename, APR_READ, APR_OS_DEFAULT, pool));
}
SVN_ERR(svn_stringbuf_from_aprfile(result, f, pool));
SVN_ERR(svn_io_file_close(f, pool));
return SVN_NO_ERROR;
}
svn_error_t *
svn_stringbuf_from_file(svn_stringbuf_t **result,
const char *filename,
apr_pool_t *pool) {
if (filename[0] == '-' && filename[1] == '\0')
return svn_error_create
(SVN_ERR_UNSUPPORTED_FEATURE, NULL,
_("Reading from stdin is disallowed"));
return svn_stringbuf_from_file2(result, filename, pool);
}
static svn_error_t *
file_name_get(const char **fname_utf8, apr_file_t *file, apr_pool_t *pool) {
apr_status_t apr_err;
const char *fname;
apr_err = apr_file_name_get(&fname, file);
if (apr_err)
return svn_error_wrap_apr(apr_err, _("Can't get file name"));
if (fname)
SVN_ERR(svn_path_cstring_to_utf8(fname_utf8, fname, pool));
else
*fname_utf8 = NULL;
return SVN_NO_ERROR;
}
svn_error_t *
svn_stringbuf_from_aprfile(svn_stringbuf_t **result,
apr_file_t *file,
apr_pool_t *pool) {
apr_size_t len;
svn_error_t *err;
svn_stringbuf_t *res = svn_stringbuf_create("", pool);
char *buf = apr_palloc(pool, SVN__STREAM_CHUNK_SIZE);
len = SVN__STREAM_CHUNK_SIZE;
err = svn_io_file_read(file, buf, &len, pool);
while (! err) {
svn_stringbuf_appendbytes(res, buf, len);
len = SVN__STREAM_CHUNK_SIZE;
err = svn_io_file_read(file, buf, &len, pool);
}
if (err && !APR_STATUS_IS_EOF(err->apr_err))
return err;
svn_error_clear(err);
res->data[res->len] = 0;
*result = res;
return SVN_NO_ERROR;
}
svn_error_t *
svn_io_remove_file(const char *path, apr_pool_t *pool) {
apr_status_t apr_err;
const char *path_apr;
#if defined(WIN32)
SVN_ERR(svn_io_set_file_read_write(path, TRUE, pool));
#endif
SVN_ERR(svn_path_cstring_from_utf8(&path_apr, path, pool));
apr_err = apr_file_remove(path_apr, pool);
WIN32_RETRY_LOOP(apr_err, apr_file_remove(path_apr, pool));
if (apr_err)
return svn_error_wrap_apr(apr_err, _("Can't remove file '%s'"),
svn_path_local_style(path, pool));
return SVN_NO_ERROR;
}
svn_error_t *
svn_io_remove_dir(const char *path, apr_pool_t *pool) {
return svn_io_remove_dir2(path, FALSE, NULL, NULL, pool);
}
svn_error_t *
svn_io_remove_dir2(const char *path, svn_boolean_t ignore_enoent,
svn_cancel_func_t cancel_func, void *cancel_baton,
apr_pool_t *pool) {
apr_status_t status;
apr_dir_t *this_dir;
apr_finfo_t this_entry;
apr_pool_t *subpool;
apr_int32_t flags = APR_FINFO_TYPE | APR_FINFO_NAME;
const char *path_apr;
int need_rewind;
if (cancel_func)
SVN_ERR((*cancel_func)(cancel_baton));
if (path[0] == '\0')
SVN_ERR(svn_path_cstring_from_utf8(&path_apr, ".", pool));
else
SVN_ERR(svn_path_cstring_from_utf8(&path_apr, path, pool));
status = apr_dir_open(&this_dir, path_apr, pool);
if (status) {
if (ignore_enoent && APR_STATUS_IS_ENOENT(status))
return SVN_NO_ERROR;
else
return svn_error_wrap_apr(status,
_("Can't open directory '%s'"),
svn_path_local_style(path, pool));
}
subpool = svn_pool_create(pool);
do {
need_rewind = FALSE;
for (status = apr_dir_read(&this_entry, flags, this_dir);
status == APR_SUCCESS;
status = apr_dir_read(&this_entry, flags, this_dir)) {
svn_pool_clear(subpool);
if ((this_entry.filetype == APR_DIR)
&& ((this_entry.name[0] == '.')
&& ((this_entry.name[1] == '\0')
|| ((this_entry.name[1] == '.')
&& (this_entry.name[2] == '\0'))))) {
continue;
} else {
const char *fullpath, *entry_utf8;
#if !defined(WIN32)
need_rewind = TRUE;
#endif
SVN_ERR(svn_path_cstring_to_utf8(&entry_utf8, this_entry.name,
subpool));
fullpath = svn_path_join(path, entry_utf8, subpool);
if (this_entry.filetype == APR_DIR) {
SVN_ERR(svn_io_remove_dir2(fullpath, FALSE,
cancel_func, cancel_baton,
subpool));
} else {
svn_error_t *err;
if (cancel_func)
SVN_ERR((*cancel_func)(cancel_baton));
err = svn_io_remove_file(fullpath, subpool);
if (err)
return svn_error_createf
(err->apr_err, err, _("Can't remove '%s'"),
svn_path_local_style(fullpath, subpool));
}
}
}
if (need_rewind) {
status = apr_dir_rewind(this_dir);
if (status)
return svn_error_wrap_apr(status, _("Can't rewind directory '%s'"),
svn_path_local_style (path, pool));
}
} while (need_rewind);
svn_pool_destroy(subpool);
if (!APR_STATUS_IS_ENOENT(status))
return svn_error_wrap_apr(status, _("Can't read directory '%s'"),
svn_path_local_style(path, pool));
status = apr_dir_close(this_dir);
if (status)
return svn_error_wrap_apr(status, _("Error closing directory '%s'"),
svn_path_local_style(path, pool));
status = apr_dir_remove(path_apr, pool);
WIN32_RETRY_LOOP(status, apr_dir_remove(path_apr, pool));
if (status)
return svn_error_wrap_apr(status, _("Can't remove '%s'"),
svn_path_local_style(path, pool));
return APR_SUCCESS;
}
svn_error_t *
svn_io_get_dir_filenames(apr_hash_t **dirents,
const char *path,
apr_pool_t *pool) {
apr_status_t status;
apr_dir_t *this_dir;
apr_finfo_t this_entry;
apr_int32_t flags = APR_FINFO_NAME;
*dirents = apr_hash_make(pool);
SVN_ERR(svn_io_dir_open(&this_dir, path, pool));
for (status = apr_dir_read(&this_entry, flags, this_dir);
status == APR_SUCCESS;
status = apr_dir_read(&this_entry, flags, this_dir)) {
if ((this_entry.name[0] == '.')
&& ((this_entry.name[1] == '\0')
|| ((this_entry.name[1] == '.')
&& (this_entry.name[2] == '\0')))) {
continue;
} else {
const char *name;
SVN_ERR(svn_path_cstring_to_utf8(&name, this_entry.name, pool));
apr_hash_set(*dirents, name, APR_HASH_KEY_STRING, name);
}
}
if (! (APR_STATUS_IS_ENOENT(status)))
return svn_error_wrap_apr(status, _("Can't read directory '%s'"),
svn_path_local_style(path, pool));
status = apr_dir_close(this_dir);
if (status)
return svn_error_wrap_apr(status, _("Error closing directory '%s'"),
svn_path_local_style(path, pool));
return SVN_NO_ERROR;
}
svn_error_t *
svn_io_get_dirents2(apr_hash_t **dirents,
const char *path,
apr_pool_t *pool) {
apr_status_t status;
apr_dir_t *this_dir;
apr_finfo_t this_entry;
apr_int32_t flags = APR_FINFO_TYPE | APR_FINFO_NAME;
*dirents = apr_hash_make(pool);
SVN_ERR(svn_io_dir_open(&this_dir, path, pool));
for (status = apr_dir_read(&this_entry, flags, this_dir);
status == APR_SUCCESS;
status = apr_dir_read(&this_entry, flags, this_dir)) {
if ((this_entry.name[0] == '.')
&& ((this_entry.name[1] == '\0')
|| ((this_entry.name[1] == '.')
&& (this_entry.name[2] == '\0')))) {
continue;
} else {
const char *name;
svn_io_dirent_t *dirent = apr_palloc(pool, sizeof(*dirent));
SVN_ERR(svn_path_cstring_to_utf8(&name, this_entry.name, pool));
map_apr_finfo_to_node_kind(&(dirent->kind),
&(dirent->special),
&this_entry);
apr_hash_set(*dirents, name, APR_HASH_KEY_STRING, dirent);
}
}
if (! (APR_STATUS_IS_ENOENT(status)))
return svn_error_wrap_apr(status, _("Can't read directory '%s'"),
svn_path_local_style(path, pool));
status = apr_dir_close(this_dir);
if (status)
return svn_error_wrap_apr(status, _("Error closing directory '%s'"),
svn_path_local_style(path, pool));
return SVN_NO_ERROR;
}
svn_error_t *
svn_io_get_dirents(apr_hash_t **dirents,
const char *path,
apr_pool_t *pool) {
return svn_io_get_dirents2(dirents, path, pool);
}
#define ERRFILE_KEY "svn-io-start-cmd-errfile"
static void
handle_child_process_error(apr_pool_t *pool, apr_status_t status,
const char *desc) {
char errbuf[256];
apr_file_t *errfile;
void *p;
if (apr_pool_userdata_get(&p, ERRFILE_KEY, pool))
return;
errfile = p;
if (errfile)
apr_file_printf(errfile, "%s: %s",
desc, apr_strerror(status, errbuf,
sizeof(errbuf)));
}
svn_error_t *
svn_io_start_cmd(apr_proc_t *cmd_proc,
const char *path,
const char *cmd,
const char *const *args,
svn_boolean_t inherit,
apr_file_t *infile,
apr_file_t *outfile,
apr_file_t *errfile,
apr_pool_t *pool) {
apr_status_t apr_err;
apr_procattr_t *cmdproc_attr;
int num_args;
const char **args_native;
const char *cmd_apr;
apr_err = apr_procattr_create(&cmdproc_attr, pool);
if (apr_err)
return svn_error_wrap_apr
(apr_err, _("Can't create process '%s' attributes"), cmd);
apr_err = apr_procattr_cmdtype_set(cmdproc_attr,
inherit?APR_PROGRAM_PATH:APR_PROGRAM);
if (apr_err)
return svn_error_wrap_apr(apr_err, _("Can't set process '%s' cmdtype"),
cmd);
if (path) {
const char *path_apr;
SVN_ERR(svn_path_cstring_from_utf8(&path_apr, path, pool));
apr_err = apr_procattr_dir_set(cmdproc_attr, path_apr);
if (apr_err)
return svn_error_wrap_apr
(apr_err, _("Can't set process '%s' directory"), cmd);
}
if (infile) {
apr_err = apr_procattr_child_in_set(cmdproc_attr, infile, NULL);
if (apr_err)
return svn_error_wrap_apr
(apr_err, _("Can't set process '%s' child input"), cmd);
}
if (outfile) {
apr_err = apr_procattr_child_out_set(cmdproc_attr, outfile, NULL);
if (apr_err)
return svn_error_wrap_apr
(apr_err, _("Can't set process '%s' child outfile"), cmd);
}
if (errfile) {
apr_err = apr_procattr_child_err_set(cmdproc_attr, errfile, NULL);
if (apr_err)
return svn_error_wrap_apr
(apr_err, _("Can't set process '%s' child errfile"), cmd);
}
apr_err = apr_pool_userdata_set(errfile, ERRFILE_KEY, NULL, pool);
if (apr_err)
return svn_error_wrap_apr
(apr_err, _("Can't set process '%s' child errfile for error handler"),
cmd);
apr_err = apr_procattr_child_errfn_set(cmdproc_attr,
handle_child_process_error);
if (apr_err)
return svn_error_wrap_apr
(apr_err, _("Can't set process '%s' error handler"), cmd);
SVN_ERR(svn_path_cstring_from_utf8(&cmd_apr, cmd, pool));
for (num_args = 0; args[num_args]; num_args++)
;
args_native = apr_palloc(pool, (num_args + 1) * sizeof(char *));
args_native[num_args] = NULL;
while (num_args--) {
SVN_ERR(svn_path_cstring_from_utf8(&args_native[num_args],
args[num_args],
pool));
}
apr_err = apr_proc_create(cmd_proc, cmd_apr, args_native, NULL,
cmdproc_attr, pool);
if (apr_err)
return svn_error_wrap_apr(apr_err, _("Can't start process '%s'"), cmd);
return SVN_NO_ERROR;
}
#undef ERRFILE_KEY
svn_error_t *
svn_io_wait_for_cmd(apr_proc_t *cmd_proc,
const char *cmd,
int *exitcode,
apr_exit_why_e *exitwhy,
apr_pool_t *pool) {
apr_status_t apr_err;
apr_exit_why_e exitwhy_val;
int exitcode_val;
exitwhy_val = APR_PROC_EXIT;
apr_err = apr_proc_wait(cmd_proc, &exitcode_val, &exitwhy_val, APR_WAIT);
if (!APR_STATUS_IS_CHILD_DONE(apr_err))
return svn_error_wrap_apr(apr_err, _("Error waiting for process '%s'"),
cmd);
if (exitwhy)
*exitwhy = exitwhy_val;
else if (! APR_PROC_CHECK_EXIT(exitwhy_val))
return svn_error_createf
(SVN_ERR_EXTERNAL_PROGRAM, NULL,
_("Process '%s' failed (exitwhy %d)"), cmd, exitwhy_val);
if (exitcode)
*exitcode = exitcode_val;
else if (exitcode_val != 0)
return svn_error_createf
(SVN_ERR_EXTERNAL_PROGRAM, NULL,
_("Process '%s' returned error exitcode %d"), cmd, exitcode_val);
return SVN_NO_ERROR;
}
svn_error_t *
svn_io_run_cmd(const char *path,
const char *cmd,
const char *const *args,
int *exitcode,
apr_exit_why_e *exitwhy,
svn_boolean_t inherit,
apr_file_t *infile,
apr_file_t *outfile,
apr_file_t *errfile,
apr_pool_t *pool) {
apr_proc_t cmd_proc;
SVN_ERR(svn_io_start_cmd(&cmd_proc, path, cmd, args, inherit,
infile, outfile, errfile, pool));
SVN_ERR(svn_io_wait_for_cmd(&cmd_proc, cmd, exitcode, exitwhy, pool));
return SVN_NO_ERROR;
}
svn_error_t *
svn_io_run_diff(const char *dir,
const char *const *user_args,
int num_user_args,
const char *label1,
const char *label2,
const char *from,
const char *to,
int *pexitcode,
apr_file_t *outfile,
apr_file_t *errfile,
const char *diff_cmd,
apr_pool_t *pool) {
const char **args;
int i;
int exitcode;
int nargs = 4;
const char *diff_utf8;
apr_pool_t *subpool = svn_pool_create(pool);
SVN_ERR(svn_path_cstring_to_utf8(&diff_utf8, diff_cmd, pool));
if (pexitcode == NULL)
pexitcode = &exitcode;
if (user_args != NULL)
nargs += num_user_args;
else
nargs += 1;
if (label1 != NULL)
nargs += 2;
if (label2 != NULL)
nargs += 2;
args = apr_palloc(subpool, nargs * sizeof(char *));
i = 0;
args[i++] = diff_utf8;
if (user_args != NULL) {
int j;
for (j = 0; j < num_user_args; ++j)
args[i++] = user_args[j];
} else
args[i++] = "-u";
if (label1 != NULL) {
args[i++] = "-L";
args[i++] = label1;
}
if (label2 != NULL) {
args[i++] = "-L";
args[i++] = label2;
}
args[i++] = svn_path_local_style(from, subpool);
args[i++] = svn_path_local_style(to, subpool);
args[i++] = NULL;
assert(i == nargs);
SVN_ERR(svn_io_run_cmd(dir, diff_utf8, args, pexitcode, NULL, TRUE,
NULL, outfile, errfile, subpool));
if (*pexitcode != 0 && *pexitcode != 1)
return svn_error_createf(SVN_ERR_EXTERNAL_PROGRAM, NULL,
_("'%s' returned %d"),
svn_path_local_style(diff_utf8, pool),
*pexitcode);
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
svn_error_t *
svn_io_run_diff3_2(int *exitcode,
const char *dir,
const char *mine,
const char *older,
const char *yours,
const char *mine_label,
const char *older_label,
const char *yours_label,
apr_file_t *merged,
const char *diff3_cmd,
const apr_array_header_t *user_args,
apr_pool_t *pool) {
const char **args = apr_palloc(pool,
sizeof(char*) * (13
+ (user_args
? user_args->nelts
: 1)));
const char *diff3_utf8;
#if !defined(NDEBUG)
int nargs = 12;
#endif
int i = 0;
SVN_ERR(svn_path_cstring_to_utf8(&diff3_utf8, diff3_cmd, pool));
if (mine_label == NULL)
mine_label = ".working";
if (older_label == NULL)
older_label = ".old";
if (yours_label == NULL)
yours_label = ".new";
args[i++] = diff3_utf8;
if (user_args) {
int j;
for (j = 0; j < user_args->nelts; ++j)
args[i++] = APR_ARRAY_IDX(user_args, j, const char *);
#if !defined(NDEBUG)
nargs += user_args->nelts;
#endif
} else {
args[i++] = "-E";
#if !defined(NDEBUG)
++nargs;
#endif
}
args[i++] = "-m";
args[i++] = "-L";
args[i++] = mine_label;
args[i++] = "-L";
args[i++] = older_label;
args[i++] = "-L";
args[i++] = yours_label;
#if defined(SVN_DIFF3_HAS_DIFF_PROGRAM_ARG)
{
svn_boolean_t has_arg;
apr_hash_t *config;
svn_config_t *cfg;
SVN_ERR(svn_config_get_config(&config, pool));
cfg = config ? apr_hash_get(config, SVN_CONFIG_CATEGORY_CONFIG,
APR_HASH_KEY_STRING) : NULL;
SVN_ERR(svn_config_get_bool(cfg, &has_arg, SVN_CONFIG_SECTION_HELPERS,
SVN_CONFIG_OPTION_DIFF3_HAS_PROGRAM_ARG,
TRUE));
if (has_arg) {
const char *diff_cmd, *diff_utf8;
svn_config_get(cfg, &diff_cmd, SVN_CONFIG_SECTION_HELPERS,
SVN_CONFIG_OPTION_DIFF_CMD, SVN_CLIENT_DIFF);
SVN_ERR(svn_path_cstring_to_utf8(&diff_utf8, diff_cmd, pool));
args[i++] = apr_pstrcat(pool, "--diff-program=", diff_utf8, NULL);
#if !defined(NDEBUG)
++nargs;
#endif
}
}
#endif
args[i++] = svn_path_local_style(mine, pool);
args[i++] = svn_path_local_style(older, pool);
args[i++] = svn_path_local_style(yours, pool);
args[i++] = NULL;
assert(i == nargs);
SVN_ERR(svn_io_run_cmd(dir, diff3_utf8, args,
exitcode, NULL,
TRUE,
NULL, merged, NULL,
pool));
if ((*exitcode != 0) && (*exitcode != 1))
return svn_error_createf(SVN_ERR_EXTERNAL_PROGRAM, NULL,
_("Error running '%s': exitcode was %d, "
"args were:"
"\nin directory '%s', basenames:\n%s\n%s\n%s"),
svn_path_local_style(diff3_utf8, pool),
*exitcode,
svn_path_local_style(dir, pool),
mine, older, yours);
return SVN_NO_ERROR;
}
svn_error_t *
svn_io_run_diff3(const char *dir,
const char *mine,
const char *older,
const char *yours,
const char *mine_label,
const char *older_label,
const char *yours_label,
apr_file_t *merged,
int *exitcode,
const char *diff3_cmd,
apr_pool_t *pool) {
return svn_io_run_diff3_2(exitcode, dir, mine, older, yours,
mine_label, older_label, yours_label,
merged, diff3_cmd, NULL, pool);
}
svn_error_t *
svn_io_parse_mimetypes_file(apr_hash_t **type_map,
const char *mimetypes_file,
apr_pool_t *pool) {
svn_error_t *err = SVN_NO_ERROR;
apr_hash_t *types = apr_hash_make(pool);
svn_boolean_t eof = FALSE;
svn_stringbuf_t *buf;
apr_pool_t *subpool = svn_pool_create(pool);
apr_file_t *types_file;
svn_stream_t *mimetypes_stream;
SVN_ERR(svn_io_file_open(&types_file, mimetypes_file,
APR_READ, APR_OS_DEFAULT, pool));
mimetypes_stream = svn_stream_from_aprfile2(types_file, FALSE, pool);
while (1) {
apr_array_header_t *tokens;
const char *type;
int i;
svn_pool_clear(subpool);
if ((err = svn_stream_readline(mimetypes_stream, &buf,
APR_EOL_STR, &eof, subpool)))
break;
if (buf->len) {
if (buf->data[0] == '#')
continue;
tokens = svn_cstring_split(buf->data, " \t", TRUE, pool);
if (tokens->nelts < 2)
continue;
type = APR_ARRAY_IDX(tokens, 0, const char *);
for (i = 1; i < tokens->nelts; i++) {
const char *ext = APR_ARRAY_IDX(tokens, i, const char *);
apr_hash_set(types, ext, APR_HASH_KEY_STRING, type);
}
}
if (eof)
break;
}
svn_pool_destroy(subpool);
if (err) {
svn_error_clear(svn_stream_close(mimetypes_stream));
return err;
}
SVN_ERR(svn_stream_close(mimetypes_stream));
*type_map = types;
return SVN_NO_ERROR;
}
svn_error_t *
svn_io_detect_mimetype2(const char **mimetype,
const char *file,
apr_hash_t *mimetype_map,
apr_pool_t *pool) {
static const char * const generic_binary = "application/octet-stream";
svn_node_kind_t kind;
apr_file_t *fh;
svn_error_t *err;
unsigned char block[1024];
apr_size_t amt_read = sizeof(block);
*mimetype = NULL;
SVN_ERR(svn_io_check_path(file, &kind, pool));
if (kind != svn_node_file)
return svn_error_createf(SVN_ERR_BAD_FILENAME, NULL,
_("Can't detect MIME type of non-file '%s'"),
svn_path_local_style(file, pool));
if (mimetype_map) {
const char *type_from_map, *path_ext;
svn_path_splitext(NULL, &path_ext, file, pool);
if ((type_from_map = apr_hash_get(mimetype_map, path_ext,
APR_HASH_KEY_STRING))) {
*mimetype = type_from_map;
return SVN_NO_ERROR;
}
}
SVN_ERR(svn_io_file_open(&fh, file, APR_READ, 0, pool));
err = svn_io_file_read(fh, block, &amt_read, pool);
if (err && ! APR_STATUS_IS_EOF(err->apr_err))
return err;
svn_error_clear(err);
SVN_ERR(svn_io_file_close(fh, pool));
if (amt_read > 0) {
apr_size_t i;
int binary_count = 0;
for (i = 0; i < amt_read; i++) {
if (block[i] == 0) {
binary_count = amt_read;
break;
}
if ((block[i] < 0x07)
|| ((block[i] > 0x0D) && (block[i] < 0x20))
|| (block[i] > 0x7F)) {
binary_count++;
}
}
if (((binary_count * 1000) / amt_read) > 850) {
*mimetype = generic_binary;
return SVN_NO_ERROR;
}
}
return SVN_NO_ERROR;
}
svn_error_t *
svn_io_detect_mimetype(const char **mimetype,
const char *file,
apr_pool_t *pool) {
return svn_io_detect_mimetype2(mimetype, file, NULL, pool);
}
svn_error_t *
svn_io_file_open(apr_file_t **new_file, const char *fname,
apr_int32_t flag, apr_fileperms_t perm,
apr_pool_t *pool) {
const char *fname_apr;
apr_status_t status;
SVN_ERR(svn_path_cstring_from_utf8(&fname_apr, fname, pool));
status = file_open(new_file, fname_apr, flag | APR_BINARY, perm, pool);
if (status)
return svn_error_wrap_apr(status, _("Can't open file '%s'"),
svn_path_local_style(fname, pool));
else
return SVN_NO_ERROR;
}
static svn_error_t *
do_io_file_wrapper_cleanup(apr_file_t *file, apr_status_t status,
const char *msg, const char *msg_no_name,
apr_pool_t *pool) {
const char *name;
svn_error_t *err;
if (! status)
return SVN_NO_ERROR;
err = file_name_get(&name, file, pool);
if (err)
name = NULL;
svn_error_clear(err);
if (name)
return svn_error_wrap_apr(status, _(msg),
svn_path_local_style(name, pool));
else
return svn_error_wrap_apr(status, _(msg_no_name));
}
svn_error_t *
svn_io_file_close(apr_file_t *file, apr_pool_t *pool) {
return do_io_file_wrapper_cleanup
(file, apr_file_close(file),
N_("Can't close file '%s'"),
N_("Can't close stream"),
pool);
}
svn_error_t *
svn_io_file_getc(char *ch, apr_file_t *file, apr_pool_t *pool) {
return do_io_file_wrapper_cleanup
(file, apr_file_getc(ch, file),
N_("Can't read file '%s'"),
N_("Can't read stream"),
pool);
}
svn_error_t *
svn_io_file_info_get(apr_finfo_t *finfo, apr_int32_t wanted,
apr_file_t *file, apr_pool_t *pool) {
return do_io_file_wrapper_cleanup
(file, apr_file_info_get(finfo, wanted, file),
N_("Can't get attribute information from file '%s'"),
N_("Can't get attribute information from stream"),
pool);
}
svn_error_t *
svn_io_file_read(apr_file_t *file, void *buf,
apr_size_t *nbytes, apr_pool_t *pool) {
return do_io_file_wrapper_cleanup
(file, apr_file_read(file, buf, nbytes),
N_("Can't read file '%s'"),
N_("Can't read stream"),
pool);
}
svn_error_t *
svn_io_file_read_full(apr_file_t *file, void *buf,
apr_size_t nbytes, apr_size_t *bytes_read,
apr_pool_t *pool) {
return do_io_file_wrapper_cleanup
(file, apr_file_read_full(file, buf, nbytes, bytes_read),
N_("Can't read file '%s'"),
N_("Can't read stream"),
pool);
}
svn_error_t *
svn_io_file_seek(apr_file_t *file, apr_seek_where_t where,
apr_off_t *offset, apr_pool_t *pool) {
return do_io_file_wrapper_cleanup
(file, apr_file_seek(file, where, offset),
N_("Can't set position pointer in file '%s'"),
N_("Can't set position pointer in stream"),
pool);
}
svn_error_t *
svn_io_file_write(apr_file_t *file, const void *buf,
apr_size_t *nbytes, apr_pool_t *pool) {
return do_io_file_wrapper_cleanup
(file, apr_file_write(file, buf, nbytes),
N_("Can't write to file '%s'"),
N_("Can't write to stream"),
pool);
}
svn_error_t *
svn_io_file_write_full(apr_file_t *file, const void *buf,
apr_size_t nbytes, apr_size_t *bytes_written,
apr_pool_t *pool) {
apr_status_t rv = apr_file_write_full(file, buf, nbytes, bytes_written);
#if defined(WIN32)
#define MAXBUFSIZE 30*1024
if (rv == APR_FROM_OS_ERROR(ERROR_NOT_ENOUGH_MEMORY)
&& nbytes > MAXBUFSIZE) {
apr_size_t bw = 0;
*bytes_written = 0;
do {
rv = apr_file_write_full(file, buf,
nbytes > MAXBUFSIZE ? MAXBUFSIZE : nbytes, &bw);
*bytes_written += bw;
buf = (char *)buf + bw;
nbytes -= bw;
} while (rv == APR_SUCCESS && nbytes > 0);
}
#undef MAXBUFSIZE
#endif
return do_io_file_wrapper_cleanup
(file, rv,
N_("Can't write to file '%s'"),
N_("Can't write to stream"),
pool);
}
svn_error_t *
svn_io_read_length_line(apr_file_t *file, char *buf, apr_size_t *limit,
apr_pool_t *pool) {
const char *name;
svn_error_t *err;
apr_size_t i;
char c;
for (i = 0; i < *limit; i++) {
SVN_ERR(svn_io_file_getc(&c, file, pool));
if (c == '\n') {
buf[i] = '\0';
*limit = i;
return SVN_NO_ERROR;
} else {
buf[i] = c;
}
}
err = file_name_get(&name, file, pool);
if (err)
name = NULL;
svn_error_clear(err);
if (name)
return svn_error_createf(SVN_ERR_MALFORMED_FILE, NULL,
_("Can't read length line in file '%s'"),
svn_path_local_style(name, pool));
else
return svn_error_create(SVN_ERR_MALFORMED_FILE, NULL,
_("Can't read length line in stream"));
}
svn_error_t *
svn_io_stat(apr_finfo_t *finfo, const char *fname,
apr_int32_t wanted, apr_pool_t *pool) {
apr_status_t status;
const char *fname_apr;
if (fname[0] == '\0')
fname = ".";
SVN_ERR(svn_path_cstring_from_utf8(&fname_apr, fname, pool));
status = apr_stat(finfo, fname_apr, wanted, pool);
if (status)
return svn_error_wrap_apr(status, _("Can't stat '%s'"),
svn_path_local_style(fname, pool));
return SVN_NO_ERROR;
}
svn_error_t *
svn_io_file_rename(const char *from_path, const char *to_path,
apr_pool_t *pool) {
apr_status_t status = APR_SUCCESS;
const char *from_path_apr, *to_path_apr;
#if defined(WIN32)
SVN_ERR(svn_io_set_file_read_write(to_path, TRUE, pool));
#endif
SVN_ERR(svn_path_cstring_from_utf8(&from_path_apr, from_path, pool));
SVN_ERR(svn_path_cstring_from_utf8(&to_path_apr, to_path, pool));
status = apr_file_rename(from_path_apr, to_path_apr, pool);
WIN32_RETRY_LOOP(status,
apr_file_rename(from_path_apr, to_path_apr, pool));
if (status)
return svn_error_wrap_apr(status, _("Can't move '%s' to '%s'"),
svn_path_local_style(from_path, pool),
svn_path_local_style(to_path, pool));
return SVN_NO_ERROR;
}
svn_error_t *
svn_io_file_move(const char *from_path, const char *to_path,
apr_pool_t *pool) {
svn_error_t *err = svn_io_file_rename(from_path, to_path, pool);
if (err && APR_STATUS_IS_EXDEV(err->apr_err)) {
const char *tmp_to_path;
svn_error_clear(err);
SVN_ERR(svn_io_open_unique_file2(NULL, &tmp_to_path, to_path,
"tmp", svn_io_file_del_none, pool));
err = svn_io_copy_file(from_path, tmp_to_path, TRUE, pool);
if (err)
goto failed_tmp;
err = svn_io_file_rename(tmp_to_path, to_path, pool);
if (err)
goto failed_tmp;
err = svn_io_remove_file(from_path, pool);
if (! err)
return SVN_NO_ERROR;
svn_error_clear(svn_io_remove_file(to_path, pool));
return err;
failed_tmp:
svn_error_clear(svn_io_remove_file(tmp_to_path, pool));
}
return err;
}
static svn_error_t *
dir_make(const char *path, apr_fileperms_t perm,
svn_boolean_t hidden, svn_boolean_t sgid, apr_pool_t *pool) {
apr_status_t status;
const char *path_apr;
SVN_ERR(svn_path_cstring_from_utf8(&path_apr, path, pool));
if (path_apr[0] == '\0')
path_apr = ".";
#if (APR_OS_DEFAULT & APR_WSTICKY)
if (perm == APR_OS_DEFAULT)
perm &= ~(APR_USETID | APR_GSETID | APR_WSTICKY);
#endif
status = apr_dir_make(path_apr, perm, pool);
WIN32_RETRY_LOOP(status, apr_dir_make(path_apr, perm, pool));
if (status)
return svn_error_wrap_apr(status, _("Can't create directory '%s'"),
svn_path_local_style(path, pool));
#if defined(APR_FILE_ATTR_HIDDEN)
if (hidden) {
status = apr_file_attrs_set(path_apr,
APR_FILE_ATTR_HIDDEN,
APR_FILE_ATTR_HIDDEN,
pool);
if (status)
return svn_error_wrap_apr(status, _("Can't hide directory '%s'"),
svn_path_local_style(path, pool));
}
#endif
if (sgid) {
apr_finfo_t finfo;
status = apr_stat(&finfo, path_apr, APR_FINFO_PROT, pool);
if (!status)
apr_file_perms_set(path_apr, finfo.protection | APR_GSETID);
}
return SVN_NO_ERROR;
}
svn_error_t *
svn_io_dir_make(const char *path, apr_fileperms_t perm, apr_pool_t *pool) {
return dir_make(path, perm, FALSE, FALSE, pool);
}
svn_error_t *
svn_io_dir_make_hidden(const char *path, apr_fileperms_t perm,
apr_pool_t *pool) {
return dir_make(path, perm, TRUE, FALSE, pool);
}
svn_error_t *
svn_io_dir_make_sgid(const char *path, apr_fileperms_t perm,
apr_pool_t *pool) {
return dir_make(path, perm, FALSE, TRUE, pool);
}
svn_error_t *
svn_io_dir_open(apr_dir_t **new_dir, const char *dirname, apr_pool_t *pool) {
apr_status_t status;
const char *dirname_apr;
if (dirname[0] == '\0')
dirname = ".";
SVN_ERR(svn_path_cstring_from_utf8(&dirname_apr, dirname, pool));
status = apr_dir_open(new_dir, dirname_apr, pool);
if (status)
return svn_error_wrap_apr(status, _("Can't open directory '%s'"),
svn_path_local_style(dirname, pool));
return SVN_NO_ERROR;
}
svn_error_t *
svn_io_dir_remove_nonrecursive(const char *dirname, apr_pool_t *pool) {
apr_status_t status;
const char *dirname_apr;
SVN_ERR(svn_path_cstring_from_utf8(&dirname_apr, dirname, pool));
status = apr_dir_remove(dirname_apr, pool);
WIN32_RETRY_LOOP(status, apr_dir_remove(dirname_apr, pool));
if (status)
return svn_error_wrap_apr(status, _("Can't remove directory '%s'"),
svn_path_local_style(dirname, pool));
return SVN_NO_ERROR;
}
svn_error_t *
svn_io_dir_read(apr_finfo_t *finfo,
apr_int32_t wanted,
apr_dir_t *thedir,
apr_pool_t *pool) {
apr_status_t status;
status = apr_dir_read(finfo, wanted, thedir);
if (status)
return svn_error_wrap_apr(status, _("Can't read directory"));
if (finfo->fname)
SVN_ERR(svn_path_cstring_to_utf8(&finfo->fname, finfo->fname, pool));
if (finfo->name)
SVN_ERR(svn_path_cstring_to_utf8(&finfo->name, finfo->name, pool));
return SVN_NO_ERROR;
}
svn_error_t *
svn_io_dir_walk(const char *dirname,
apr_int32_t wanted,
svn_io_walk_func_t walk_func,
void *walk_baton,
apr_pool_t *pool) {
apr_status_t apr_err;
apr_dir_t *handle;
apr_pool_t *subpool;
const char *dirname_apr;
apr_finfo_t finfo;
wanted |= APR_FINFO_TYPE | APR_FINFO_NAME;
SVN_ERR(svn_io_stat(&finfo, dirname, wanted & ~APR_FINFO_NAME, pool));
SVN_ERR(svn_path_cstring_from_utf8(&finfo.name,
svn_path_basename(dirname, pool),
pool));
finfo.valid |= APR_FINFO_NAME;
SVN_ERR((*walk_func)(walk_baton, dirname, &finfo, pool));
SVN_ERR(svn_path_cstring_from_utf8(&dirname_apr, dirname, pool));
apr_err = apr_dir_open(&handle, dirname_apr, pool);
if (apr_err)
return svn_error_wrap_apr(apr_err, _("Can't open directory '%s'"),
svn_path_local_style(dirname, pool));
subpool = svn_pool_create(pool);
while (1) {
const char *name_utf8;
const char *full_path;
svn_pool_clear(subpool);
apr_err = apr_dir_read(&finfo, wanted, handle);
if (APR_STATUS_IS_ENOENT(apr_err))
break;
else if (apr_err) {
return svn_error_wrap_apr
(apr_err, _("Can't read directory entry in '%s'"),
svn_path_local_style(dirname, pool));
}
if (finfo.filetype == APR_DIR) {
if (finfo.name[0] == '.'
&& (finfo.name[1] == '\0'
|| (finfo.name[1] == '.' && finfo.name[2] == '\0')))
continue;
SVN_ERR(svn_path_cstring_to_utf8(&name_utf8, finfo.name,
subpool));
full_path = svn_path_join(dirname, name_utf8, subpool);
SVN_ERR(svn_io_dir_walk(full_path,
wanted,
walk_func,
walk_baton,
subpool));
} else if (finfo.filetype == APR_REG) {
SVN_ERR(svn_path_cstring_to_utf8(&name_utf8, finfo.name,
subpool));
full_path = svn_path_join(dirname, name_utf8, subpool);
SVN_ERR((*walk_func)(walk_baton,
full_path,
&finfo,
subpool));
}
}
svn_pool_destroy(subpool);
apr_err = apr_dir_close(handle);
if (apr_err)
return svn_error_wrap_apr(apr_err, _("Error closing directory '%s'"),
svn_path_local_style(dirname, pool));
return SVN_NO_ERROR;
}
static apr_status_t
dir_is_empty(const char *dir, apr_pool_t *pool) {
apr_status_t apr_err;
apr_dir_t *dir_handle;
apr_finfo_t finfo;
apr_status_t retval = APR_SUCCESS;
if (dir[0] == '\0')
dir = ".";
apr_err = apr_dir_open(&dir_handle, dir, pool);
if (apr_err != APR_SUCCESS)
return apr_err;
for (apr_err = apr_dir_read(&finfo, APR_FINFO_NAME, dir_handle);
apr_err == APR_SUCCESS;
apr_err = apr_dir_read(&finfo, APR_FINFO_NAME, dir_handle)) {
if (! (finfo.name[0] == '.'
&& (finfo.name[1] == '\0'
|| (finfo.name[1] == '.' && finfo.name[2] == '\0')))) {
retval = APR_ENOTEMPTY;
break;
}
}
if (apr_err && ! APR_STATUS_IS_ENOENT(apr_err))
return apr_err;
apr_err = apr_dir_close(dir_handle);
if (apr_err != APR_SUCCESS)
return apr_err;
return retval;
}
svn_error_t *
svn_io_dir_empty(svn_boolean_t *is_empty_p,
const char *path,
apr_pool_t *pool) {
apr_status_t status;
const char *path_apr;
SVN_ERR(svn_path_cstring_from_utf8(&path_apr, path, pool));
status = dir_is_empty(path_apr, pool);
if (!status)
*is_empty_p = TRUE;
else if (APR_STATUS_IS_ENOTEMPTY(status))
*is_empty_p = FALSE;
else
return svn_error_wrap_apr(status, _("Can't check directory '%s'"),
svn_path_local_style(path, pool));
return SVN_NO_ERROR;
}
svn_error_t *
svn_io_write_version_file(const char *path,
int version,
apr_pool_t *pool) {
apr_file_t *format_file = NULL;
const char *path_tmp;
const char *format_contents = apr_psprintf(pool, "%d\n", version);
if (version < 0)
return svn_error_createf(SVN_ERR_INCORRECT_PARAMS, NULL,
_("Version %d is not non-negative"), version);
SVN_ERR(svn_io_open_unique_file2(&format_file, &path_tmp, path, ".tmp",
svn_io_file_del_none, pool));
SVN_ERR(svn_io_file_write_full(format_file, format_contents,
strlen(format_contents), NULL, pool));
SVN_ERR(svn_io_file_close(format_file, pool));
#if defined(WIN32)
SVN_ERR(svn_io_set_file_read_write(path, TRUE, pool));
#endif
SVN_ERR(svn_io_file_rename(path_tmp, path, pool));
SVN_ERR(svn_io_set_file_read_only(path, FALSE, pool));
return SVN_NO_ERROR;
}
svn_error_t *
svn_io_read_version_file(int *version,
const char *path,
apr_pool_t *pool) {
apr_file_t *format_file;
char buf[80];
apr_size_t len;
SVN_ERR(svn_io_file_open(&format_file, path, APR_READ,
APR_OS_DEFAULT, pool));
len = sizeof(buf);
SVN_ERR(svn_io_file_read(format_file, buf, &len, pool));
SVN_ERR(svn_io_file_close(format_file, pool));
if (len == 0)
return svn_error_createf(SVN_ERR_STREAM_UNEXPECTED_EOF, NULL,
_("Reading '%s'"),
svn_path_local_style(path, pool));
{
apr_size_t i;
for (i = 0; i < len; ++i) {
char c = buf[i];
if (i > 0 && (c == '\r' || c == '\n'))
break;
if (! apr_isdigit(c))
return svn_error_createf
(SVN_ERR_BAD_VERSION_FILE_FORMAT, NULL,
_("First line of '%s' contains non-digit"),
svn_path_local_style(path, pool));
}
}
*version = atoi(buf);
return SVN_NO_ERROR;
}
static svn_error_t *
contents_identical_p(svn_boolean_t *identical_p,
const char *file1,
const char *file2,
apr_pool_t *pool) {
svn_error_t *err1;
svn_error_t *err2;
apr_size_t bytes_read1, bytes_read2;
char *buf1 = apr_palloc(pool, SVN__STREAM_CHUNK_SIZE);
char *buf2 = apr_palloc(pool, SVN__STREAM_CHUNK_SIZE);
apr_file_t *file1_h = NULL;
apr_file_t *file2_h = NULL;
SVN_ERR(svn_io_file_open(&file1_h, file1, APR_READ, APR_OS_DEFAULT,
pool));
SVN_ERR(svn_io_file_open(&file2_h, file2, APR_READ, APR_OS_DEFAULT,
pool));
*identical_p = TRUE;
do {
err1 = svn_io_file_read_full(file1_h, buf1,
SVN__STREAM_CHUNK_SIZE, &bytes_read1, pool);
if (err1 && !APR_STATUS_IS_EOF(err1->apr_err))
return err1;
err2 = svn_io_file_read_full(file2_h, buf2,
SVN__STREAM_CHUNK_SIZE, &bytes_read2, pool);
if (err2 && !APR_STATUS_IS_EOF(err2->apr_err)) {
svn_error_clear(err1);
return err2;
}
if ((bytes_read1 != bytes_read2)
|| (memcmp(buf1, buf2, bytes_read1))) {
*identical_p = FALSE;
break;
}
} while (! err1 && ! err2);
svn_error_clear(err1);
svn_error_clear(err2);
SVN_ERR(svn_io_file_close(file1_h, pool));
SVN_ERR(svn_io_file_close(file2_h, pool));
return SVN_NO_ERROR;
}
svn_error_t *
svn_io_files_contents_same_p(svn_boolean_t *same,
const char *file1,
const char *file2,
apr_pool_t *pool) {
svn_boolean_t q;
SVN_ERR(svn_io_filesizes_different_p(&q, file1, file2, pool));
if (q) {
*same = 0;
return SVN_NO_ERROR;
}
SVN_ERR(contents_identical_p(&q, file1, file2, pool));
if (q)
*same = 1;
else
*same = 0;
return SVN_NO_ERROR;
}