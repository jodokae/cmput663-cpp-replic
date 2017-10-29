#if !defined(SVN_RA_H)
#define SVN_RA_H
#include <apr_pools.h>
#include <apr_tables.h>
#include "svn_error.h"
#include "svn_delta.h"
#include "svn_auth.h"
#include "svn_mergeinfo.h"
#if defined(__cplusplus)
extern "C" {
#endif
const svn_version_t *svn_ra_version(void);
typedef svn_error_t *(*svn_ra_get_wc_prop_func_t)(void *baton,
const char *relpath,
const char *name,
const svn_string_t **value,
apr_pool_t *pool);
typedef svn_error_t *(*svn_ra_set_wc_prop_func_t)(void *baton,
const char *path,
const char *name,
const svn_string_t *value,
apr_pool_t *pool);
typedef svn_error_t *(*svn_ra_push_wc_prop_func_t)(void *baton,
const char *path,
const char *name,
const svn_string_t *value,
apr_pool_t *pool);
typedef svn_error_t *(*svn_ra_invalidate_wc_props_func_t)(void *baton,
const char *path,
const char *name,
apr_pool_t *pool);
typedef svn_error_t *(*svn_ra_get_latest_revnum_func_t)
(void *session_baton,
svn_revnum_t *latest_revnum);
typedef svn_error_t *(*svn_ra_get_client_string_func_t)(void *baton,
const char **name,
apr_pool_t *pool);
typedef svn_error_t *(*svn_ra_file_rev_handler_t)
(void *baton,
const char *path,
svn_revnum_t rev,
apr_hash_t *rev_props,
svn_txdelta_window_handler_t *delta_handler,
void **delta_baton,
apr_array_header_t *prop_diffs,
apr_pool_t *pool);
typedef svn_error_t *(*svn_ra_lock_callback_t)(void *baton,
const char *path,
svn_boolean_t do_lock,
const svn_lock_t *lock,
svn_error_t *ra_err,
apr_pool_t *pool);
typedef void (*svn_ra_progress_notify_func_t)(apr_off_t progress,
apr_off_t total,
void *baton,
apr_pool_t *pool);
typedef svn_error_t *(*svn_ra_replay_revstart_callback_t)
(svn_revnum_t revision,
void *replay_baton,
const svn_delta_editor_t **editor,
void **edit_baton,
apr_hash_t *rev_props,
apr_pool_t *pool);
typedef svn_error_t *(*svn_ra_replay_revfinish_callback_t)
(svn_revnum_t revision,
void *replay_baton,
const svn_delta_editor_t *editor,
void *edit_baton,
apr_hash_t *rev_props,
apr_pool_t *pool);
typedef struct svn_ra_reporter3_t {
svn_error_t *(*set_path)(void *report_baton,
const char *path,
svn_revnum_t revision,
svn_depth_t depth,
svn_boolean_t start_empty,
const char *lock_token,
apr_pool_t *pool);
svn_error_t *(*delete_path)(void *report_baton,
const char *path,
apr_pool_t *pool);
svn_error_t *(*link_path)(void *report_baton,
const char *path,
const char *url,
svn_revnum_t revision,
svn_depth_t depth,
svn_boolean_t start_empty,
const char *lock_token,
apr_pool_t *pool);
svn_error_t *(*finish_report)(void *report_baton,
apr_pool_t *pool);
svn_error_t *(*abort_report)(void *report_baton,
apr_pool_t *pool);
} svn_ra_reporter3_t;
typedef struct svn_ra_reporter2_t {
svn_error_t *(*set_path)(void *report_baton,
const char *path,
svn_revnum_t revision,
svn_boolean_t start_empty,
const char *lock_token,
apr_pool_t *pool);
svn_error_t *(*delete_path)(void *report_baton,
const char *path,
apr_pool_t *pool);
svn_error_t *(*link_path)(void *report_baton,
const char *path,
const char *url,
svn_revnum_t revision,
svn_boolean_t start_empty,
const char *lock_token,
apr_pool_t *pool);
svn_error_t *(*finish_report)(void *report_baton,
apr_pool_t *pool);
svn_error_t *(*abort_report)(void *report_baton,
apr_pool_t *pool);
} svn_ra_reporter2_t;
typedef struct svn_ra_reporter_t {
svn_error_t *(*set_path)(void *report_baton,
const char *path,
svn_revnum_t revision,
svn_boolean_t start_empty,
apr_pool_t *pool);
svn_error_t *(*delete_path)(void *report_baton,
const char *path,
apr_pool_t *pool);
svn_error_t *(*link_path)(void *report_baton,
const char *path,
const char *url,
svn_revnum_t revision,
svn_boolean_t start_empty,
apr_pool_t *pool);
svn_error_t *(*finish_report)(void *report_baton,
apr_pool_t *pool);
svn_error_t *(*abort_report)(void *report_baton,
apr_pool_t *pool);
} svn_ra_reporter_t;
typedef struct svn_ra_callbacks2_t {
svn_error_t *(*open_tmp_file)(apr_file_t **fp,
void *callback_baton,
apr_pool_t *pool);
svn_auth_baton_t *auth_baton;
svn_ra_get_wc_prop_func_t get_wc_prop;
svn_ra_set_wc_prop_func_t set_wc_prop;
svn_ra_push_wc_prop_func_t push_wc_prop;
svn_ra_invalidate_wc_props_func_t invalidate_wc_props;
svn_ra_progress_notify_func_t progress_func;
void *progress_baton;
svn_cancel_func_t cancel_func;
svn_ra_get_client_string_func_t get_client_string;
} svn_ra_callbacks2_t;
typedef struct svn_ra_callbacks_t {
svn_error_t *(*open_tmp_file)(apr_file_t **fp,
void *callback_baton,
apr_pool_t *pool);
svn_auth_baton_t *auth_baton;
svn_ra_get_wc_prop_func_t get_wc_prop;
svn_ra_set_wc_prop_func_t set_wc_prop;
svn_ra_push_wc_prop_func_t push_wc_prop;
svn_ra_invalidate_wc_props_func_t invalidate_wc_props;
} svn_ra_callbacks_t;
svn_error_t *
svn_ra_initialize(apr_pool_t *pool);
svn_error_t *
svn_ra_create_callbacks(svn_ra_callbacks2_t **callbacks,
apr_pool_t *pool);
typedef struct svn_ra_session_t svn_ra_session_t;
svn_error_t *
svn_ra_open3(svn_ra_session_t **session_p,
const char *repos_URL,
const char *uuid,
const svn_ra_callbacks2_t *callbacks,
void *callback_baton,
apr_hash_t *config,
apr_pool_t *pool);
svn_error_t *
svn_ra_open2(svn_ra_session_t **session_p,
const char *repos_URL,
const svn_ra_callbacks2_t *callbacks,
void *callback_baton,
apr_hash_t *config,
apr_pool_t *pool);
svn_error_t *
svn_ra_open(svn_ra_session_t **session_p,
const char *repos_URL,
const svn_ra_callbacks_t *callbacks,
void *callback_baton,
apr_hash_t *config,
apr_pool_t *pool);
svn_error_t *
svn_ra_reparent(svn_ra_session_t *ra_session,
const char *url,
apr_pool_t *pool);
svn_error_t *
svn_ra_get_session_url(svn_ra_session_t *ra_session,
const char **url,
apr_pool_t *pool);
svn_error_t *
svn_ra_get_latest_revnum(svn_ra_session_t *session,
svn_revnum_t *latest_revnum,
apr_pool_t *pool);
svn_error_t *
svn_ra_get_dated_revision(svn_ra_session_t *session,
svn_revnum_t *revision,
apr_time_t tm,
apr_pool_t *pool);
svn_error_t *
svn_ra_change_rev_prop(svn_ra_session_t *session,
svn_revnum_t rev,
const char *name,
const svn_string_t *value,
apr_pool_t *pool);
svn_error_t *
svn_ra_rev_proplist(svn_ra_session_t *session,
svn_revnum_t rev,
apr_hash_t **props,
apr_pool_t *pool);
svn_error_t *
svn_ra_rev_prop(svn_ra_session_t *session,
svn_revnum_t rev,
const char *name,
svn_string_t **value,
apr_pool_t *pool);
svn_error_t *
svn_ra_get_commit_editor3(svn_ra_session_t *session,
const svn_delta_editor_t **editor,
void **edit_baton,
apr_hash_t *revprop_table,
svn_commit_callback2_t callback,
void *callback_baton,
apr_hash_t *lock_tokens,
svn_boolean_t keep_locks,
apr_pool_t *pool);
svn_error_t *
svn_ra_get_commit_editor2(svn_ra_session_t *session,
const svn_delta_editor_t **editor,
void **edit_baton,
const char *log_msg,
svn_commit_callback2_t callback,
void *callback_baton,
apr_hash_t *lock_tokens,
svn_boolean_t keep_locks,
apr_pool_t *pool);
svn_error_t *
svn_ra_get_commit_editor(svn_ra_session_t *session,
const svn_delta_editor_t **editor,
void **edit_baton,
const char *log_msg,
svn_commit_callback_t callback,
void *callback_baton,
apr_hash_t *lock_tokens,
svn_boolean_t keep_locks,
apr_pool_t *pool);
svn_error_t *
svn_ra_get_file(svn_ra_session_t *session,
const char *path,
svn_revnum_t revision,
svn_stream_t *stream,
svn_revnum_t *fetched_rev,
apr_hash_t **props,
apr_pool_t *pool);
svn_error_t *
svn_ra_get_dir2(svn_ra_session_t *session,
apr_hash_t **dirents,
svn_revnum_t *fetched_rev,
apr_hash_t **props,
const char *path,
svn_revnum_t revision,
apr_uint32_t dirent_fields,
apr_pool_t *pool);
svn_error_t *
svn_ra_get_dir(svn_ra_session_t *session,
const char *path,
svn_revnum_t revision,
apr_hash_t **dirents,
svn_revnum_t *fetched_rev,
apr_hash_t **props,
apr_pool_t *pool);
svn_error_t *
svn_ra_get_mergeinfo(svn_ra_session_t *session,
svn_mergeinfo_catalog_t *catalog,
const apr_array_header_t *paths,
svn_revnum_t revision,
svn_mergeinfo_inheritance_t inherit,
svn_boolean_t include_descendants,
apr_pool_t *pool);
svn_error_t *
svn_ra_do_update2(svn_ra_session_t *session,
const svn_ra_reporter3_t **reporter,
void **report_baton,
svn_revnum_t revision_to_update_to,
const char *update_target,
svn_depth_t depth,
svn_boolean_t send_copyfrom_args,
const svn_delta_editor_t *update_editor,
void *update_baton,
apr_pool_t *pool);
svn_error_t *
svn_ra_do_update(svn_ra_session_t *session,
const svn_ra_reporter2_t **reporter,
void **report_baton,
svn_revnum_t revision_to_update_to,
const char *update_target,
svn_boolean_t recurse,
const svn_delta_editor_t *update_editor,
void *update_baton,
apr_pool_t *pool);
svn_error_t *
svn_ra_do_switch2(svn_ra_session_t *session,
const svn_ra_reporter3_t **reporter,
void **report_baton,
svn_revnum_t revision_to_switch_to,
const char *switch_target,
svn_depth_t depth,
const char *switch_url,
const svn_delta_editor_t *switch_editor,
void *switch_baton,
apr_pool_t *pool);
svn_error_t *
svn_ra_do_switch(svn_ra_session_t *session,
const svn_ra_reporter2_t **reporter,
void **report_baton,
svn_revnum_t revision_to_switch_to,
const char *switch_target,
svn_boolean_t recurse,
const char *switch_url,
const svn_delta_editor_t *switch_editor,
void *switch_baton,
apr_pool_t *pool);
svn_error_t *
svn_ra_do_status2(svn_ra_session_t *session,
const svn_ra_reporter3_t **reporter,
void **report_baton,
const char *status_target,
svn_revnum_t revision,
svn_depth_t depth,
const svn_delta_editor_t *status_editor,
void *status_baton,
apr_pool_t *pool);
svn_error_t *
svn_ra_do_status(svn_ra_session_t *session,
const svn_ra_reporter2_t **reporter,
void **report_baton,
const char *status_target,
svn_revnum_t revision,
svn_boolean_t recurse,
const svn_delta_editor_t *status_editor,
void *status_baton,
apr_pool_t *pool);
svn_error_t *
svn_ra_do_diff3(svn_ra_session_t *session,
const svn_ra_reporter3_t **reporter,
void **report_baton,
svn_revnum_t revision,
const char *diff_target,
svn_depth_t depth,
svn_boolean_t ignore_ancestry,
svn_boolean_t text_deltas,
const char *versus_url,
const svn_delta_editor_t *diff_editor,
void *diff_baton,
apr_pool_t *pool);
svn_error_t *
svn_ra_do_diff2(svn_ra_session_t *session,
const svn_ra_reporter2_t **reporter,
void **report_baton,
svn_revnum_t revision,
const char *diff_target,
svn_boolean_t recurse,
svn_boolean_t ignore_ancestry,
svn_boolean_t text_deltas,
const char *versus_url,
const svn_delta_editor_t *diff_editor,
void *diff_baton,
apr_pool_t *pool);
svn_error_t *
svn_ra_do_diff(svn_ra_session_t *session,
const svn_ra_reporter2_t **reporter,
void **report_baton,
svn_revnum_t revision,
const char *diff_target,
svn_boolean_t recurse,
svn_boolean_t ignore_ancestry,
const char *versus_url,
const svn_delta_editor_t *diff_editor,
void *diff_baton,
apr_pool_t *pool);
svn_error_t *
svn_ra_get_log2(svn_ra_session_t *session,
const apr_array_header_t *paths,
svn_revnum_t start,
svn_revnum_t end,
int limit,
svn_boolean_t discover_changed_paths,
svn_boolean_t strict_node_history,
svn_boolean_t include_merged_revisions,
const apr_array_header_t *revprops,
svn_log_entry_receiver_t receiver,
void *receiver_baton,
apr_pool_t *pool);
svn_error_t *
svn_ra_get_log(svn_ra_session_t *session,
const apr_array_header_t *paths,
svn_revnum_t start,
svn_revnum_t end,
int limit,
svn_boolean_t discover_changed_paths,
svn_boolean_t strict_node_history,
svn_log_message_receiver_t receiver,
void *receiver_baton,
apr_pool_t *pool);
svn_error_t *
svn_ra_check_path(svn_ra_session_t *session,
const char *path,
svn_revnum_t revision,
svn_node_kind_t *kind,
apr_pool_t *pool);
svn_error_t *
svn_ra_stat(svn_ra_session_t *session,
const char *path,
svn_revnum_t revision,
svn_dirent_t **dirent,
apr_pool_t *pool);
svn_error_t *
svn_ra_get_uuid2(svn_ra_session_t *session,
const char **uuid,
apr_pool_t *pool);
svn_error_t *
svn_ra_get_uuid(svn_ra_session_t *session,
const char **uuid,
apr_pool_t *pool);
svn_error_t *
svn_ra_get_repos_root2(svn_ra_session_t *session,
const char **url,
apr_pool_t *pool);
svn_error_t *
svn_ra_get_repos_root(svn_ra_session_t *session,
const char **url,
apr_pool_t *pool);
svn_error_t *
svn_ra_get_locations(svn_ra_session_t *session,
apr_hash_t **locations,
const char *path,
svn_revnum_t peg_revision,
apr_array_header_t *location_revisions,
apr_pool_t *pool);
svn_error_t *
svn_ra_get_location_segments(svn_ra_session_t *session,
const char *path,
svn_revnum_t peg_revision,
svn_revnum_t start_rev,
svn_revnum_t end_rev,
svn_location_segment_receiver_t receiver,
void *receiver_baton,
apr_pool_t *pool);
svn_error_t *
svn_ra_get_file_revs2(svn_ra_session_t *session,
const char *path,
svn_revnum_t start,
svn_revnum_t end,
svn_boolean_t include_merged_revisions,
svn_file_rev_handler_t handler,
void *handler_baton,
apr_pool_t *pool);
svn_error_t *
svn_ra_get_file_revs(svn_ra_session_t *session,
const char *path,
svn_revnum_t start,
svn_revnum_t end,
svn_ra_file_rev_handler_t handler,
void *handler_baton,
apr_pool_t *pool);
svn_error_t *
svn_ra_lock(svn_ra_session_t *session,
apr_hash_t *path_revs,
const char *comment,
svn_boolean_t steal_lock,
svn_ra_lock_callback_t lock_func,
void *lock_baton,
apr_pool_t *pool);
svn_error_t *
svn_ra_unlock(svn_ra_session_t *session,
apr_hash_t *path_tokens,
svn_boolean_t break_lock,
svn_ra_lock_callback_t lock_func,
void *lock_baton,
apr_pool_t *pool);
svn_error_t *
svn_ra_get_lock(svn_ra_session_t *session,
svn_lock_t **lock,
const char *path,
apr_pool_t *pool);
svn_error_t *
svn_ra_get_locks(svn_ra_session_t *session,
apr_hash_t **locks,
const char *path,
apr_pool_t *pool);
svn_error_t *
svn_ra_replay_range(svn_ra_session_t *session,
svn_revnum_t start_revision,
svn_revnum_t end_revision,
svn_revnum_t low_water_mark,
svn_boolean_t send_deltas,
svn_ra_replay_revstart_callback_t revstart_func,
svn_ra_replay_revfinish_callback_t revfinish_func,
void *replay_baton,
apr_pool_t *pool);
svn_error_t *
svn_ra_replay(svn_ra_session_t *session,
svn_revnum_t revision,
svn_revnum_t low_water_mark,
svn_boolean_t send_deltas,
const svn_delta_editor_t *editor,
void *edit_baton,
apr_pool_t *pool);
svn_error_t *
svn_ra_has_capability(svn_ra_session_t *session,
svn_boolean_t *has,
const char *capability,
apr_pool_t *pool);
#define SVN_RA_CAPABILITY_DEPTH "depth"
#define SVN_RA_CAPABILITY_MERGEINFO "mergeinfo"
#define SVN_RA_CAPABILITY_LOG_REVPROPS "log-revprops"
#define SVN_RA_CAPABILITY_PARTIAL_REPLAY "partial-replay"
#define SVN_RA_CAPABILITY_COMMIT_REVPROPS "commit-revprops"
svn_error_t *
svn_ra_print_modules(svn_stringbuf_t *output,
apr_pool_t *pool);
svn_error_t *
svn_ra_print_ra_libraries(svn_stringbuf_t **descriptions,
void *ra_baton,
apr_pool_t *pool);
typedef struct svn_ra_plugin_t {
const char *name;
const char *description;
svn_error_t *(*open)(void **session_baton,
const char *repos_URL,
const svn_ra_callbacks_t *callbacks,
void *callback_baton,
apr_hash_t *config,
apr_pool_t *pool);
svn_error_t *(*get_latest_revnum)(void *session_baton,
svn_revnum_t *latest_revnum,
apr_pool_t *pool);
svn_error_t *(*get_dated_revision)(void *session_baton,
svn_revnum_t *revision,
apr_time_t tm,
apr_pool_t *pool);
svn_error_t *(*change_rev_prop)(void *session_baton,
svn_revnum_t rev,
const char *name,
const svn_string_t *value,
apr_pool_t *pool);
svn_error_t *(*rev_proplist)(void *session_baton,
svn_revnum_t rev,
apr_hash_t **props,
apr_pool_t *pool);
svn_error_t *(*rev_prop)(void *session_baton,
svn_revnum_t rev,
const char *name,
svn_string_t **value,
apr_pool_t *pool);
svn_error_t *(*get_commit_editor)(void *session_baton,
const svn_delta_editor_t **editor,
void **edit_baton,
const char *log_msg,
svn_commit_callback_t callback,
void *callback_baton,
apr_pool_t *pool);
svn_error_t *(*get_file)(void *session_baton,
const char *path,
svn_revnum_t revision,
svn_stream_t *stream,
svn_revnum_t *fetched_rev,
apr_hash_t **props,
apr_pool_t *pool);
svn_error_t *(*get_dir)(void *session_baton,
const char *path,
svn_revnum_t revision,
apr_hash_t **dirents,
svn_revnum_t *fetched_rev,
apr_hash_t **props,
apr_pool_t *pool);
svn_error_t *(*do_update)(void *session_baton,
const svn_ra_reporter_t **reporter,
void **report_baton,
svn_revnum_t revision_to_update_to,
const char *update_target,
svn_boolean_t recurse,
const svn_delta_editor_t *update_editor,
void *update_baton,
apr_pool_t *pool);
svn_error_t *(*do_switch)(void *session_baton,
const svn_ra_reporter_t **reporter,
void **report_baton,
svn_revnum_t revision_to_switch_to,
const char *switch_target,
svn_boolean_t recurse,
const char *switch_url,
const svn_delta_editor_t *switch_editor,
void *switch_baton,
apr_pool_t *pool);
svn_error_t *(*do_status)(void *session_baton,
const svn_ra_reporter_t **reporter,
void **report_baton,
const char *status_target,
svn_revnum_t revision,
svn_boolean_t recurse,
const svn_delta_editor_t *status_editor,
void *status_baton,
apr_pool_t *pool);
svn_error_t *(*do_diff)(void *session_baton,
const svn_ra_reporter_t **reporter,
void **report_baton,
svn_revnum_t revision,
const char *diff_target,
svn_boolean_t recurse,
svn_boolean_t ignore_ancestry,
const char *versus_url,
const svn_delta_editor_t *diff_editor,
void *diff_baton,
apr_pool_t *pool);
svn_error_t *(*get_log)(void *session_baton,
const apr_array_header_t *paths,
svn_revnum_t start,
svn_revnum_t end,
svn_boolean_t discover_changed_paths,
svn_boolean_t strict_node_history,
svn_log_message_receiver_t receiver,
void *receiver_baton,
apr_pool_t *pool);
svn_error_t *(*check_path)(void *session_baton,
const char *path,
svn_revnum_t revision,
svn_node_kind_t *kind,
apr_pool_t *pool);
svn_error_t *(*get_uuid)(void *session_baton,
const char **uuid,
apr_pool_t *pool);
svn_error_t *(*get_repos_root)(void *session_baton,
const char **url,
apr_pool_t *pool);
svn_error_t *(*get_locations)(void *session_baton,
apr_hash_t **locations,
const char *path,
svn_revnum_t peg_revision,
apr_array_header_t *location_revisions,
apr_pool_t *pool);
svn_error_t *(*get_file_revs)(void *session_baton,
const char *path,
svn_revnum_t start,
svn_revnum_t end,
svn_ra_file_rev_handler_t handler,
void *handler_baton,
apr_pool_t *pool);
const svn_version_t *(*get_version)(void);
} svn_ra_plugin_t;
typedef svn_error_t *(*svn_ra_init_func_t)(int abi_version,
apr_pool_t *pool,
apr_hash_t *hash);
#define SVN_RA_ABI_VERSION 2
svn_error_t *
svn_ra_dav_init(int abi_version,
apr_pool_t *pool,
apr_hash_t *hash);
svn_error_t *
svn_ra_local_init(int abi_version,
apr_pool_t *pool,
apr_hash_t *hash);
svn_error_t *
svn_ra_svn_init(int abi_version,
apr_pool_t *pool,
apr_hash_t *hash);
svn_error_t *
svn_ra_serf_init(int abi_version,
apr_pool_t *pool,
apr_hash_t *hash);
svn_error_t *svn_ra_init_ra_libs(void **ra_baton, apr_pool_t *pool);
svn_error_t *
svn_ra_get_ra_library(svn_ra_plugin_t **library,
void *ra_baton,
const char *url,
apr_pool_t *pool);
#if defined(__cplusplus)
}
#endif
#endif