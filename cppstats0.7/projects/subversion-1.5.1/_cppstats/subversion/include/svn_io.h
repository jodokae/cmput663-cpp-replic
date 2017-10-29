#if !defined(SVN_IO_H)
#define SVN_IO_H
#include <apr.h>
#include <apr_pools.h>
#include <apr_file_io.h>
#include <apr_thread_proc.h>
#include "svn_types.h"
#include "svn_error.h"
#include "svn_string.h"
#if defined(__cplusplus)
extern "C" {
#endif
typedef enum svn_io_file_del_t {
svn_io_file_del_none = 0,
svn_io_file_del_on_close,
svn_io_file_del_on_pool_cleanup
} svn_io_file_del_t;
typedef struct svn_io_dirent_t {
svn_node_kind_t kind;
svn_boolean_t special;
} svn_io_dirent_t;
svn_error_t *svn_io_check_path(const char *path,
svn_node_kind_t *kind,
apr_pool_t *pool);
svn_error_t *svn_io_check_special_path(const char *path,
svn_node_kind_t *kind,
svn_boolean_t *is_special,
apr_pool_t *pool);
svn_error_t *svn_io_check_resolved_path(const char *path,
svn_node_kind_t *kind,
apr_pool_t *pool);
svn_error_t *svn_io_open_unique_file2(apr_file_t **f,
const char **unique_name_p,
const char *path,
const char *suffix,
svn_io_file_del_t delete_when,
apr_pool_t *pool);
svn_error_t *svn_io_open_unique_file(apr_file_t **f,
const char **unique_name_p,
const char *path,
const char *suffix,
svn_boolean_t delete_on_close,
apr_pool_t *pool);
svn_error_t *svn_io_create_unique_link(const char **unique_name_p,
const char *path,
const char *dest,
const char *suffix,
apr_pool_t *pool);
svn_error_t *svn_io_read_link(svn_string_t **dest,
const char *path,
apr_pool_t *pool);
svn_error_t *svn_io_temp_dir(const char **dir,
apr_pool_t *pool);
svn_error_t *svn_io_copy_file(const char *src,
const char *dst,
svn_boolean_t copy_perms,
apr_pool_t *pool);
svn_error_t *svn_io_copy_link(const char *src,
const char *dst,
apr_pool_t *pool);
svn_error_t *svn_io_copy_dir_recursively(const char *src,
const char *dst_parent,
const char *dst_basename,
svn_boolean_t copy_perms,
svn_cancel_func_t cancel_func,
void *cancel_baton,
apr_pool_t *pool);
svn_error_t *svn_io_make_dir_recursively(const char *path, apr_pool_t *pool);
svn_error_t *
svn_io_dir_empty(svn_boolean_t *is_empty_p,
const char *path,
apr_pool_t *pool);
svn_error_t *svn_io_append_file(const char *src,
const char *dst,
apr_pool_t *pool);
svn_error_t *svn_io_set_file_read_only(const char *path,
svn_boolean_t ignore_enoent,
apr_pool_t *pool);
svn_error_t *svn_io_set_file_read_write(const char *path,
svn_boolean_t ignore_enoent,
apr_pool_t *pool);
svn_error_t *svn_io_set_file_read_write_carefully(const char *path,
svn_boolean_t enable_write,
svn_boolean_t ignore_enoent,
apr_pool_t *pool);
svn_error_t *svn_io_set_file_executable(const char *path,
svn_boolean_t executable,
svn_boolean_t ignore_enoent,
apr_pool_t *pool);
svn_error_t *svn_io_is_file_executable(svn_boolean_t *executable,
const char *path,
apr_pool_t *pool);
svn_error_t *
svn_io_read_length_line(apr_file_t *file, char *buf, apr_size_t *limit,
apr_pool_t *pool);
svn_error_t *svn_io_file_affected_time(apr_time_t *apr_time,
const char *path,
apr_pool_t *pool);
svn_error_t *svn_io_set_file_affected_time(apr_time_t apr_time,
const char *path,
apr_pool_t *pool);
svn_error_t *svn_io_filesizes_different_p(svn_boolean_t *different_p,
const char *file1,
const char *file2,
apr_pool_t *pool);
svn_error_t *svn_io_file_checksum(unsigned char digest[],
const char *file,
apr_pool_t *pool);
svn_error_t *svn_io_files_contents_same_p(svn_boolean_t *same,
const char *file1,
const char *file2,
apr_pool_t *pool);
svn_error_t *svn_io_file_create(const char *file,
const char *contents,
apr_pool_t *pool);
svn_error_t *svn_io_file_lock(const char *lock_file,
svn_boolean_t exclusive,
apr_pool_t *pool);
svn_error_t *svn_io_file_lock2(const char *lock_file,
svn_boolean_t exclusive,
svn_boolean_t nonblocking,
apr_pool_t *pool);
svn_error_t *svn_io_file_flush_to_disk(apr_file_t *file,
apr_pool_t *pool);
svn_error_t *svn_io_dir_file_copy(const char *src_path,
const char *dest_path,
const char *file,
apr_pool_t *pool);
typedef struct svn_stream_t svn_stream_t;
typedef svn_error_t *(*svn_read_fn_t)(void *baton,
char *buffer,
apr_size_t *len);
typedef svn_error_t *(*svn_write_fn_t)(void *baton,
const char *data,
apr_size_t *len);
typedef svn_error_t *(*svn_close_fn_t)(void *baton);
svn_stream_t *svn_stream_create(void *baton, apr_pool_t *pool);
void svn_stream_set_baton(svn_stream_t *stream, void *baton);
void svn_stream_set_read(svn_stream_t *stream, svn_read_fn_t read_fn);
void svn_stream_set_write(svn_stream_t *stream, svn_write_fn_t write_fn);
void svn_stream_set_close(svn_stream_t *stream, svn_close_fn_t close_fn);
svn_stream_t *svn_stream_empty(apr_pool_t *pool);
svn_stream_t *svn_stream_disown(svn_stream_t *stream, apr_pool_t *pool);
svn_stream_t * svn_stream_from_aprfile2(apr_file_t *file,
svn_boolean_t disown,
apr_pool_t *pool);
svn_stream_t *svn_stream_from_aprfile(apr_file_t *file, apr_pool_t *pool);
svn_error_t *svn_stream_for_stdout(svn_stream_t **out, apr_pool_t *pool);
svn_stream_t *svn_stream_from_stringbuf(svn_stringbuf_t *str,
apr_pool_t *pool);
svn_stream_t *svn_stream_compressed(svn_stream_t *stream,
apr_pool_t *pool);
svn_stream_t *svn_stream_checksummed(svn_stream_t *stream,
const unsigned char **read_digest,
const unsigned char **write_digest,
svn_boolean_t read_all,
apr_pool_t *pool);
svn_error_t *svn_stream_read(svn_stream_t *stream, char *buffer,
apr_size_t *len);
svn_error_t *svn_stream_write(svn_stream_t *stream, const char *data,
apr_size_t *len);
svn_error_t *svn_stream_close(svn_stream_t *stream);
svn_error_t *svn_stream_printf(svn_stream_t *stream,
apr_pool_t *pool,
const char *fmt,
...)
__attribute__((format(printf, 3, 4)));
svn_error_t *svn_stream_printf_from_utf8(svn_stream_t *stream,
const char *encoding,
apr_pool_t *pool,
const char *fmt,
...)
__attribute__((format(printf, 4, 5)));
svn_error_t *
svn_stream_readline(svn_stream_t *stream,
svn_stringbuf_t **stringbuf,
const char *eol,
svn_boolean_t *eof,
apr_pool_t *pool);
svn_error_t *svn_stream_copy2(svn_stream_t *from, svn_stream_t *to,
svn_cancel_func_t cancel_func,
void *cancel_baton,
apr_pool_t *pool);
svn_error_t *svn_stream_copy(svn_stream_t *from, svn_stream_t *to,
apr_pool_t *pool);
svn_error_t *
svn_stream_contents_same(svn_boolean_t *same,
svn_stream_t *stream1,
svn_stream_t *stream2,
apr_pool_t *pool);
svn_error_t *svn_stringbuf_from_file2(svn_stringbuf_t **result,
const char *filename,
apr_pool_t *pool);
svn_error_t *svn_stringbuf_from_file(svn_stringbuf_t **result,
const char *filename,
apr_pool_t *pool);
svn_error_t *svn_stringbuf_from_aprfile(svn_stringbuf_t **result,
apr_file_t *file,
apr_pool_t *pool);
svn_error_t *svn_io_remove_file(const char *path, apr_pool_t *pool);
svn_error_t *svn_io_remove_dir2(const char *path,
svn_boolean_t ignore_enoent,
svn_cancel_func_t cancel_func,
void *cancel_baton,
apr_pool_t *pool);
svn_error_t *svn_io_remove_dir(const char *path, apr_pool_t *pool);
svn_error_t *svn_io_get_dir_filenames(apr_hash_t **dirents,
const char *path,
apr_pool_t *pool);
svn_error_t *svn_io_get_dirents2(apr_hash_t **dirents,
const char *path,
apr_pool_t *pool);
svn_error_t *svn_io_get_dirents(apr_hash_t **dirents,
const char *path,
apr_pool_t *pool);
typedef svn_error_t * (*svn_io_walk_func_t)(void *baton,
const char *path,
const apr_finfo_t *finfo,
apr_pool_t *pool);
svn_error_t *svn_io_dir_walk(const char *dirname,
apr_int32_t wanted,
svn_io_walk_func_t walk_func,
void *walk_baton,
apr_pool_t *pool);
svn_error_t *svn_io_start_cmd(apr_proc_t *cmd_proc,
const char *path,
const char *cmd,
const char *const *args,
svn_boolean_t inherit,
apr_file_t *infile,
apr_file_t *outfile,
apr_file_t *errfile,
apr_pool_t *pool);
svn_error_t *svn_io_wait_for_cmd(apr_proc_t *cmd_proc,
const char *cmd,
int *exitcode,
apr_exit_why_e *exitwhy,
apr_pool_t *pool);
svn_error_t *svn_io_run_cmd(const char *path,
const char *cmd,
const char *const *args,
int *exitcode,
apr_exit_why_e *exitwhy,
svn_boolean_t inherit,
apr_file_t *infile,
apr_file_t *outfile,
apr_file_t *errfile,
apr_pool_t *pool);
svn_error_t *svn_io_run_diff(const char *dir,
const char *const *user_args,
int num_user_args,
const char *label1,
const char *label2,
const char *from,
const char *to,
int *exitcode,
apr_file_t *outfile,
apr_file_t *errfile,
const char *diff_cmd,
apr_pool_t *pool);
svn_error_t *svn_io_run_diff3_2(int *exitcode,
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
apr_pool_t *pool);
svn_error_t *svn_io_run_diff3(const char *dir,
const char *mine,
const char *older,
const char *yours,
const char *mine_label,
const char *older_label,
const char *yours_label,
apr_file_t *merged,
int *exitcode,
const char *diff3_cmd,
apr_pool_t *pool);
svn_error_t *svn_io_parse_mimetypes_file(apr_hash_t **type_map,
const char *mimetypes_file,
apr_pool_t *pool);
svn_error_t *svn_io_detect_mimetype2(const char **mimetype,
const char *file,
apr_hash_t *mimetype_map,
apr_pool_t *pool);
svn_error_t *svn_io_detect_mimetype(const char **mimetype,
const char *file,
apr_pool_t *pool);
svn_error_t *
svn_io_file_open(apr_file_t **new_file, const char *fname,
apr_int32_t flag, apr_fileperms_t perm,
apr_pool_t *pool);
svn_error_t *
svn_io_file_close(apr_file_t *file, apr_pool_t *pool);
svn_error_t *
svn_io_file_getc(char *ch, apr_file_t *file, apr_pool_t *pool);
svn_error_t *
svn_io_file_info_get(apr_finfo_t *finfo, apr_int32_t wanted,
apr_file_t *file, apr_pool_t *pool);
svn_error_t *
svn_io_file_read(apr_file_t *file, void *buf,
apr_size_t *nbytes, apr_pool_t *pool);
svn_error_t *
svn_io_file_read_full(apr_file_t *file, void *buf,
apr_size_t nbytes, apr_size_t *bytes_read,
apr_pool_t *pool);
svn_error_t *
svn_io_file_seek(apr_file_t *file, apr_seek_where_t where,
apr_off_t *offset, apr_pool_t *pool);
svn_error_t *
svn_io_file_write(apr_file_t *file, const void *buf,
apr_size_t *nbytes, apr_pool_t *pool);
svn_error_t *
svn_io_file_write_full(apr_file_t *file, const void *buf,
apr_size_t nbytes, apr_size_t *bytes_written,
apr_pool_t *pool);
svn_error_t *
svn_io_stat(apr_finfo_t *finfo, const char *fname,
apr_int32_t wanted, apr_pool_t *pool);
svn_error_t *
svn_io_file_rename(const char *from_path, const char *to_path,
apr_pool_t *pool);
svn_error_t *
svn_io_file_move(const char *from_path, const char *to_path,
apr_pool_t *pool);
svn_error_t *
svn_io_dir_make(const char *path, apr_fileperms_t perm, apr_pool_t *pool);
svn_error_t *
svn_io_dir_make_hidden(const char *path, apr_fileperms_t perm,
apr_pool_t *pool);
svn_error_t *
svn_io_dir_make_sgid(const char *path, apr_fileperms_t perm,
apr_pool_t *pool);
svn_error_t *
svn_io_dir_open(apr_dir_t **new_dir, const char *dirname, apr_pool_t *pool);
svn_error_t *
svn_io_dir_remove_nonrecursive(const char *dirname, apr_pool_t *pool);
svn_error_t *
svn_io_dir_read(apr_finfo_t *finfo,
apr_int32_t wanted,
apr_dir_t *thedir,
apr_pool_t *pool);
svn_error_t *
svn_io_read_version_file(int *version, const char *path, apr_pool_t *pool);
svn_error_t *
svn_io_write_version_file(const char *path, int version, apr_pool_t *pool);
#if defined(__cplusplus)
}
#endif
#endif