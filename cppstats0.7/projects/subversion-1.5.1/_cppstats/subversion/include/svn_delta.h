#if !defined(SVN_DELTA_H)
#define SVN_DELTA_H
#include <apr.h>
#include <apr_pools.h>
#include "svn_types.h"
#include "svn_string.h"
#include "svn_error.h"
#include "svn_io.h"
#include "svn_version.h"
#if defined(__cplusplus)
extern "C" {
#endif
const svn_version_t *svn_delta_version(void);
enum svn_delta_action {
svn_txdelta_source,
svn_txdelta_target,
svn_txdelta_new
};
typedef struct svn_txdelta_op_t {
enum svn_delta_action action_code;
apr_size_t offset;
apr_size_t length;
} svn_txdelta_op_t;
typedef struct svn_txdelta_window_t {
svn_filesize_t sview_offset;
apr_size_t sview_len;
apr_size_t tview_len;
int num_ops;
int src_ops;
const svn_txdelta_op_t *ops;
const svn_string_t *new_data;
} svn_txdelta_window_t;
svn_txdelta_window_t *
svn_txdelta_window_dup(const svn_txdelta_window_t *window,
apr_pool_t *pool);
svn_txdelta_window_t *
svn_txdelta_compose_windows(const svn_txdelta_window_t *window_A,
const svn_txdelta_window_t *window_B,
apr_pool_t *pool);
void
svn_txdelta_apply_instructions(svn_txdelta_window_t *window,
const char *sbuf, char *tbuf,
apr_size_t *tlen);
typedef svn_error_t *(*svn_txdelta_window_handler_t)
(svn_txdelta_window_t *window, void *baton);
typedef struct svn_txdelta_stream_t svn_txdelta_stream_t;
typedef svn_error_t *
(*svn_txdelta_next_window_fn_t)(svn_txdelta_window_t **window,
void *baton,
apr_pool_t *pool);
typedef const unsigned char *
(*svn_txdelta_md5_digest_fn_t)(void *baton);
svn_txdelta_stream_t *
svn_txdelta_stream_create(void *baton,
svn_txdelta_next_window_fn_t next_window,
svn_txdelta_md5_digest_fn_t md5_digest,
apr_pool_t *pool);
svn_error_t *svn_txdelta_next_window(svn_txdelta_window_t **window,
svn_txdelta_stream_t *stream,
apr_pool_t *pool);
const unsigned char *svn_txdelta_md5_digest(svn_txdelta_stream_t *stream);
void svn_txdelta(svn_txdelta_stream_t **stream,
svn_stream_t *source,
svn_stream_t *target,
apr_pool_t *pool);
svn_stream_t *svn_txdelta_target_push(svn_txdelta_window_handler_t handler,
void *handler_baton,
svn_stream_t *source,
apr_pool_t *pool);
svn_error_t *svn_txdelta_send_string(const svn_string_t *string,
svn_txdelta_window_handler_t handler,
void *handler_baton,
apr_pool_t *pool);
svn_error_t *svn_txdelta_send_stream(svn_stream_t *stream,
svn_txdelta_window_handler_t handler,
void *handler_baton,
unsigned char *digest,
apr_pool_t *pool);
svn_error_t *svn_txdelta_send_txstream(svn_txdelta_stream_t *txstream,
svn_txdelta_window_handler_t handler,
void *handler_baton,
apr_pool_t *pool);
void svn_txdelta_apply(svn_stream_t *source,
svn_stream_t *target,
unsigned char *result_digest,
const char *error_info,
apr_pool_t *pool,
svn_txdelta_window_handler_t *handler,
void **handler_baton);
void svn_txdelta_to_svndiff2(svn_txdelta_window_handler_t *handler,
void **handler_baton,
svn_stream_t *output,
int svndiff_version,
apr_pool_t *pool);
void svn_txdelta_to_svndiff(svn_stream_t *output,
apr_pool_t *pool,
svn_txdelta_window_handler_t *handler,
void **handler_baton);
svn_stream_t *svn_txdelta_parse_svndiff(svn_txdelta_window_handler_t handler,
void *handler_baton,
svn_boolean_t error_on_early_close,
apr_pool_t *pool);
svn_error_t *svn_txdelta_read_svndiff_window(svn_txdelta_window_t **window,
svn_stream_t *stream,
int svndiff_version,
apr_pool_t *pool);
svn_error_t *svn_txdelta_skip_svndiff_window(apr_file_t *file,
int svndiff_version,
apr_pool_t *pool);
typedef struct svn_delta_editor_t {
svn_error_t *(*set_target_revision)(void *edit_baton,
svn_revnum_t target_revision,
apr_pool_t *pool);
svn_error_t *(*open_root)(void *edit_baton,
svn_revnum_t base_revision,
apr_pool_t *dir_pool,
void **root_baton);
svn_error_t *(*delete_entry)(const char *path,
svn_revnum_t revision,
void *parent_baton,
apr_pool_t *pool);
svn_error_t *(*add_directory)(const char *path,
void *parent_baton,
const char *copyfrom_path,
svn_revnum_t copyfrom_revision,
apr_pool_t *dir_pool,
void **child_baton);
svn_error_t *(*open_directory)(const char *path,
void *parent_baton,
svn_revnum_t base_revision,
apr_pool_t *dir_pool,
void **child_baton);
svn_error_t *(*change_dir_prop)(void *dir_baton,
const char *name,
const svn_string_t *value,
apr_pool_t *pool);
svn_error_t *(*close_directory)(void *dir_baton,
apr_pool_t *pool);
svn_error_t *(*absent_directory)(const char *path,
void *parent_baton,
apr_pool_t *pool);
svn_error_t *(*add_file)(const char *path,
void *parent_baton,
const char *copyfrom_path,
svn_revnum_t copyfrom_revision,
apr_pool_t *file_pool,
void **file_baton);
svn_error_t *(*open_file)(const char *path,
void *parent_baton,
svn_revnum_t base_revision,
apr_pool_t *file_pool,
void **file_baton);
svn_error_t *(*apply_textdelta)(void *file_baton,
const char *base_checksum,
apr_pool_t *pool,
svn_txdelta_window_handler_t *handler,
void **handler_baton);
svn_error_t *(*change_file_prop)(void *file_baton,
const char *name,
const svn_string_t *value,
apr_pool_t *pool);
svn_error_t *(*close_file)(void *file_baton,
const char *text_checksum,
apr_pool_t *pool);
svn_error_t *(*absent_file)(const char *path,
void *parent_baton,
apr_pool_t *pool);
svn_error_t *(*close_edit)(void *edit_baton,
apr_pool_t *pool);
svn_error_t *(*abort_edit)(void *edit_baton,
apr_pool_t *pool);
} svn_delta_editor_t;
svn_delta_editor_t *svn_delta_default_editor(apr_pool_t *pool);
svn_error_t *svn_delta_noop_window_handler(svn_txdelta_window_t *window,
void *baton);
svn_error_t *
svn_delta_get_cancellation_editor(svn_cancel_func_t cancel_func,
void *cancel_baton,
const svn_delta_editor_t *wrapped_editor,
void *wrapped_baton,
const svn_delta_editor_t **editor,
void **edit_baton,
apr_pool_t *pool);
svn_error_t *
svn_delta_depth_filter_editor(const svn_delta_editor_t **editor,
void **edit_baton,
const svn_delta_editor_t *wrapped_editor,
void *wrapped_edit_baton,
svn_depth_t requested_depth,
svn_boolean_t has_target,
apr_pool_t *pool);
typedef svn_error_t *(*svn_delta_path_driver_cb_func_t)
(void **dir_baton,
void *parent_baton,
void *callback_baton,
const char *path,
apr_pool_t *pool);
svn_error_t *
svn_delta_path_driver(const svn_delta_editor_t *editor,
void *edit_baton,
svn_revnum_t revision,
apr_array_header_t *paths,
svn_delta_path_driver_cb_func_t callback_func,
void *callback_baton,
apr_pool_t *pool);
typedef svn_error_t *(*svn_file_rev_handler_t)
(void *baton,
const char *path,
svn_revnum_t rev,
apr_hash_t *rev_props,
svn_boolean_t result_of_merge,
svn_txdelta_window_handler_t *delta_handler,
void **delta_baton,
apr_array_header_t *prop_diffs,
apr_pool_t *pool);
typedef svn_error_t *(*svn_file_rev_handler_old_t)
(void *baton,
const char *path,
svn_revnum_t rev,
apr_hash_t *rev_props,
svn_txdelta_window_handler_t *delta_handler,
void **delta_baton,
apr_array_header_t *prop_diffs,
apr_pool_t *pool);
void
svn_compat_wrap_file_rev_handler(svn_file_rev_handler_t *handler2,
void **handler2_baton,
svn_file_rev_handler_old_t handler,
void *handler_baton,
apr_pool_t *pool);
#if defined(__cplusplus)
}
#endif
#endif
