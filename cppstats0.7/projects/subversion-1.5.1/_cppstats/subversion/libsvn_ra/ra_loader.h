#if !defined(LIBSVN_RA_RA_LOADER_H)
#define LIBSVN_RA_RA_LOADER_H
#include "svn_ra.h"
#if defined(__cplusplus)
extern "C" {
#endif
typedef struct svn_ra__vtable_t {
const svn_version_t *(*get_version)(void);
const char *(*get_description)(void);
const char * const *(*get_schemes)(apr_pool_t *pool);
svn_error_t *(*open_session)(svn_ra_session_t *session,
const char *repos_URL,
const svn_ra_callbacks2_t *callbacks,
void *callback_baton,
apr_hash_t *config,
apr_pool_t *pool);
svn_error_t *(*reparent)(svn_ra_session_t *session,
const char *url,
apr_pool_t *pool);
svn_error_t *(*get_session_url)(svn_ra_session_t *session,
const char **url,
apr_pool_t *pool);
svn_error_t *(*get_latest_revnum)(svn_ra_session_t *session,
svn_revnum_t *latest_revnum,
apr_pool_t *pool);
svn_error_t *(*get_dated_revision)(svn_ra_session_t *session,
svn_revnum_t *revision,
apr_time_t tm,
apr_pool_t *pool);
svn_error_t *(*change_rev_prop)(svn_ra_session_t *session,
svn_revnum_t rev,
const char *name,
const svn_string_t *value,
apr_pool_t *pool);
svn_error_t *(*rev_proplist)(svn_ra_session_t *session,
svn_revnum_t rev,
apr_hash_t **props,
apr_pool_t *pool);
svn_error_t *(*rev_prop)(svn_ra_session_t *session,
svn_revnum_t rev,
const char *name,
svn_string_t **value,
apr_pool_t *pool);
svn_error_t *(*get_commit_editor)(svn_ra_session_t *session,
const svn_delta_editor_t **editor,
void **edit_baton,
apr_hash_t *revprop_table,
svn_commit_callback2_t callback,
void *callback_baton,
apr_hash_t *lock_tokens,
svn_boolean_t keep_locks,
apr_pool_t *pool);
svn_error_t *(*get_file)(svn_ra_session_t *session,
const char *path,
svn_revnum_t revision,
svn_stream_t *stream,
svn_revnum_t *fetched_rev,
apr_hash_t **props,
apr_pool_t *pool);
svn_error_t *(*get_dir)(svn_ra_session_t *session,
apr_hash_t **dirents,
svn_revnum_t *fetched_rev,
apr_hash_t **props,
const char *path,
svn_revnum_t revision,
apr_uint32_t dirent_fields,
apr_pool_t *pool);
svn_error_t *(*get_mergeinfo)(svn_ra_session_t *session,
svn_mergeinfo_catalog_t *mergeinfo,
const apr_array_header_t *paths,
svn_revnum_t revision,
svn_mergeinfo_inheritance_t inherit,
svn_boolean_t include_merged_revisions,
apr_pool_t *pool);
svn_error_t *(*do_update)(svn_ra_session_t *session,
const svn_ra_reporter3_t **reporter,
void **report_baton,
svn_revnum_t revision_to_update_to,
const char *update_target,
svn_depth_t depth,
svn_boolean_t send_copyfrom_args,
const svn_delta_editor_t *update_editor,
void *update_baton,
apr_pool_t *pool);
svn_error_t *(*do_switch)(svn_ra_session_t *session,
const svn_ra_reporter3_t **reporter,
void **report_baton,
svn_revnum_t revision_to_switch_to,
const char *switch_target,
svn_depth_t depth,
const char *switch_url,
const svn_delta_editor_t *switch_editor,
void *switch_baton,
apr_pool_t *pool);
svn_error_t *(*do_status)(svn_ra_session_t *session,
const svn_ra_reporter3_t **reporter,
void **report_baton,
const char *status_target,
svn_revnum_t revision,
svn_depth_t depth,
const svn_delta_editor_t *status_editor,
void *status_baton,
apr_pool_t *pool);
svn_error_t *(*do_diff)(svn_ra_session_t *session,
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
svn_error_t *(*get_log)(svn_ra_session_t *session,
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
svn_error_t *(*check_path)(svn_ra_session_t *session,
const char *path,
svn_revnum_t revision,
svn_node_kind_t *kind,
apr_pool_t *pool);
svn_error_t *(*stat)(svn_ra_session_t *session,
const char *path,
svn_revnum_t revision,
svn_dirent_t **dirent,
apr_pool_t *pool);
svn_error_t *(*get_uuid)(svn_ra_session_t *session,
const char **uuid,
apr_pool_t *pool);
svn_error_t *(*get_repos_root)(svn_ra_session_t *session,
const char **url,
apr_pool_t *pool);
svn_error_t *(*get_locations)(svn_ra_session_t *session,
apr_hash_t **locations,
const char *path,
svn_revnum_t peg_revision,
apr_array_header_t *location_revisions,
apr_pool_t *pool);
svn_error_t *(*get_location_segments)(svn_ra_session_t *session,
const char *path,
svn_revnum_t peg_revision,
svn_revnum_t start_rev,
svn_revnum_t end_rev,
svn_location_segment_receiver_t rcvr,
void *receiver_baton,
apr_pool_t *pool);
svn_error_t *(*get_file_revs)(svn_ra_session_t *session,
const char *path,
svn_revnum_t start,
svn_revnum_t end,
svn_boolean_t include_merged_revisions,
svn_file_rev_handler_t handler,
void *handler_baton,
apr_pool_t *pool);
svn_error_t *(*lock)(svn_ra_session_t *session,
apr_hash_t *path_revs,
const char *comment,
svn_boolean_t force,
svn_ra_lock_callback_t lock_func,
void *lock_baton,
apr_pool_t *pool);
svn_error_t *(*unlock)(svn_ra_session_t *session,
apr_hash_t *path_tokens,
svn_boolean_t force,
svn_ra_lock_callback_t lock_func,
void *lock_baton,
apr_pool_t *pool);
svn_error_t *(*get_lock)(svn_ra_session_t *session,
svn_lock_t **lock,
const char *path,
apr_pool_t *pool);
svn_error_t *(*get_locks)(svn_ra_session_t *session,
apr_hash_t **locks,
const char *path,
apr_pool_t *pool);
svn_error_t *(*replay)(svn_ra_session_t *session,
svn_revnum_t revision,
svn_revnum_t low_water_mark,
svn_boolean_t text_deltas,
const svn_delta_editor_t *editor,
void *edit_baton,
apr_pool_t *pool);
svn_error_t *(*has_capability)(svn_ra_session_t *session,
svn_boolean_t *has,
const char *capability,
apr_pool_t *pool);
svn_error_t *
(*replay_range)(svn_ra_session_t *session,
svn_revnum_t start_revision,
svn_revnum_t end_revision,
svn_revnum_t low_water_mark,
svn_boolean_t text_deltas,
svn_ra_replay_revstart_callback_t revstart_func,
svn_ra_replay_revfinish_callback_t revfinish_func,
void *replay_baton,
apr_pool_t *pool);
} svn_ra__vtable_t;
struct svn_ra_session_t {
const svn_ra__vtable_t *vtable;
apr_pool_t *pool;
void *priv;
};
typedef svn_error_t *
(*svn_ra__init_func_t)(const svn_version_t *loader_version,
const svn_ra__vtable_t **vtable,
apr_pool_t *pool);
svn_error_t *svn_ra_local__init(const svn_version_t *loader_version,
const svn_ra__vtable_t **vtable,
apr_pool_t *pool);
svn_error_t *svn_ra_svn__init(const svn_version_t *loader_version,
const svn_ra__vtable_t **vtable,
apr_pool_t *pool);
svn_error_t *svn_ra_neon__init(const svn_version_t *loader_version,
const svn_ra__vtable_t **vtable,
apr_pool_t *pool);
svn_error_t *svn_ra_serf__init(const svn_version_t *loader_version,
const svn_ra__vtable_t **vtable,
apr_pool_t *pool);
svn_error_t *
svn_ra__locations_from_log(svn_ra_session_t *session,
apr_hash_t **locations_p,
const char *path,
svn_revnum_t peg_revision,
apr_array_header_t *location_revisions,
apr_pool_t *pool);
svn_error_t *
svn_ra__location_segments_from_log(svn_ra_session_t *session,
const char *path,
svn_revnum_t peg_revision,
svn_revnum_t start_rev,
svn_revnum_t end_rev,
svn_location_segment_receiver_t receiver,
void *receiver_baton,
apr_pool_t *pool);
svn_error_t *
svn_ra__file_revs_from_log(svn_ra_session_t *session,
const char *path,
svn_revnum_t start,
svn_revnum_t end,
svn_file_rev_handler_t handler,
void *handler_baton,
apr_pool_t *pool);
#if defined(__cplusplus)
}
#endif
#endif
