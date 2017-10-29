#if !defined(SVN_REPOS_H)
#define SVN_REPOS_H
#include <apr_pools.h>
#include <apr_hash.h>
#include "svn_fs.h"
#include "svn_delta.h"
#include "svn_types.h"
#include "svn_error.h"
#include "svn_version.h"
#if defined(__cplusplus)
extern "C" {
#endif
const svn_version_t *svn_repos_version(void);
typedef svn_error_t *(*svn_repos_authz_func_t)(svn_boolean_t *allowed,
svn_fs_root_t *root,
const char *path,
void *baton,
apr_pool_t *pool);
typedef enum {
svn_authz_none = 0,
svn_authz_read = 1,
svn_authz_write = 2,
svn_authz_recursive = 4
} svn_repos_authz_access_t;
typedef svn_error_t *(*svn_repos_authz_callback_t)
(svn_repos_authz_access_t required,
svn_boolean_t *allowed,
svn_fs_root_t *root,
const char *path,
void *baton,
apr_pool_t *pool);
typedef svn_error_t *(*svn_repos_file_rev_handler_t)
(void *baton,
const char *path,
svn_revnum_t rev,
apr_hash_t *rev_props,
svn_txdelta_window_handler_t *delta_handler,
void **delta_baton,
apr_array_header_t *prop_diffs,
apr_pool_t *pool);
typedef struct svn_repos_t svn_repos_t;
const char *
svn_repos_find_root_path(const char *path,
apr_pool_t *pool);
svn_error_t *
svn_repos_open(svn_repos_t **repos_p,
const char *path,
apr_pool_t *pool);
svn_error_t *
svn_repos_create(svn_repos_t **repos_p,
const char *path,
const char *unused_1,
const char *unused_2,
apr_hash_t *config,
apr_hash_t *fs_config,
apr_pool_t *pool);
svn_error_t *
svn_repos_upgrade(const char *path,
svn_boolean_t nonblocking,
svn_error_t *(*start_callback)(void *baton),
void *start_callback_baton,
apr_pool_t *pool);
svn_error_t *svn_repos_delete(const char *path, apr_pool_t *pool);
svn_error_t *
svn_repos_has_capability(svn_repos_t *repos,
svn_boolean_t *has,
const char *capability,
apr_pool_t *pool);
#define SVN_REPOS_CAPABILITY_MERGEINFO "mergeinfo"
svn_fs_t *svn_repos_fs(svn_repos_t *repos);
svn_error_t *
svn_repos_hotcopy(const char *src_path,
const char *dst_path,
svn_boolean_t clean_logs,
apr_pool_t *pool);
svn_error_t *
svn_repos_recover3(const char *path,
svn_boolean_t nonblocking,
svn_error_t *(*start_callback)(void *baton),
void *start_callback_baton,
svn_cancel_func_t cancel_func,
void * cancel_baton,
apr_pool_t *pool);
svn_error_t *
svn_repos_recover2(const char *path,
svn_boolean_t nonblocking,
svn_error_t *(*start_callback)(void *baton),
void *start_callback_baton,
apr_pool_t *pool);
svn_error_t *svn_repos_recover(const char *path, apr_pool_t *pool);
svn_error_t *
svn_repos_db_logfiles(apr_array_header_t **logfiles,
const char *path,
svn_boolean_t only_unused,
apr_pool_t *pool);
const char *svn_repos_path(svn_repos_t *repos, apr_pool_t *pool);
const char *svn_repos_db_env(svn_repos_t *repos, apr_pool_t *pool);
const char *svn_repos_conf_dir(svn_repos_t *repos, apr_pool_t *pool);
const char *svn_repos_svnserve_conf(svn_repos_t *repos, apr_pool_t *pool);
const char *svn_repos_lock_dir(svn_repos_t *repos, apr_pool_t *pool);
const char *svn_repos_db_lockfile(svn_repos_t *repos, apr_pool_t *pool);
const char *svn_repos_db_logs_lockfile(svn_repos_t *repos, apr_pool_t *pool);
const char *svn_repos_hook_dir(svn_repos_t *repos, apr_pool_t *pool);
const char *svn_repos_start_commit_hook(svn_repos_t *repos, apr_pool_t *pool);
const char *svn_repos_pre_commit_hook(svn_repos_t *repos, apr_pool_t *pool);
const char *svn_repos_post_commit_hook(svn_repos_t *repos, apr_pool_t *pool);
const char *
svn_repos_pre_revprop_change_hook(svn_repos_t *repos,
apr_pool_t *pool);
const char *
svn_repos_post_revprop_change_hook(svn_repos_t *repos,
apr_pool_t *pool);
const char *svn_repos_pre_lock_hook(svn_repos_t *repos, apr_pool_t *pool);
const char *svn_repos_post_lock_hook(svn_repos_t *repos, apr_pool_t *pool);
const char *svn_repos_pre_unlock_hook(svn_repos_t *repos, apr_pool_t *pool);
const char *svn_repos_post_unlock_hook(svn_repos_t *repos, apr_pool_t *pool);
svn_error_t *
svn_repos_begin_report2(void **report_baton,
svn_revnum_t revnum,
svn_repos_t *repos,
const char *fs_base,
const char *target,
const char *tgt_path,
svn_boolean_t text_deltas,
svn_depth_t depth,
svn_boolean_t ignore_ancestry,
svn_boolean_t send_copyfrom_args,
const svn_delta_editor_t *editor,
void *edit_baton,
svn_repos_authz_func_t authz_read_func,
void *authz_read_baton,
apr_pool_t *pool);
svn_error_t *
svn_repos_begin_report(void **report_baton,
svn_revnum_t revnum,
const char *username,
svn_repos_t *repos,
const char *fs_base,
const char *target,
const char *tgt_path,
svn_boolean_t text_deltas,
svn_boolean_t recurse,
svn_boolean_t ignore_ancestry,
const svn_delta_editor_t *editor,
void *edit_baton,
svn_repos_authz_func_t authz_read_func,
void *authz_read_baton,
apr_pool_t *pool);
svn_error_t *
svn_repos_set_path3(void *report_baton,
const char *path,
svn_revnum_t revision,
svn_depth_t depth,
svn_boolean_t start_empty,
const char *lock_token,
apr_pool_t *pool);
svn_error_t *
svn_repos_set_path2(void *report_baton,
const char *path,
svn_revnum_t revision,
svn_boolean_t start_empty,
const char *lock_token,
apr_pool_t *pool);
svn_error_t *
svn_repos_set_path(void *report_baton,
const char *path,
svn_revnum_t revision,
svn_boolean_t start_empty,
apr_pool_t *pool);
svn_error_t *
svn_repos_link_path3(void *report_baton,
const char *path,
const char *link_path,
svn_revnum_t revision,
svn_depth_t depth,
svn_boolean_t start_empty,
const char *lock_token,
apr_pool_t *pool);
svn_error_t *
svn_repos_link_path2(void *report_baton,
const char *path,
const char *link_path,
svn_revnum_t revision,
svn_boolean_t start_empty,
const char *lock_token,
apr_pool_t *pool);
svn_error_t *
svn_repos_link_path(void *report_baton,
const char *path,
const char *link_path,
svn_revnum_t revision,
svn_boolean_t start_empty,
apr_pool_t *pool);
svn_error_t *
svn_repos_delete_path(void *report_baton,
const char *path,
apr_pool_t *pool);
svn_error_t *
svn_repos_finish_report(void *report_baton,
apr_pool_t *pool);
svn_error_t *
svn_repos_abort_report(void *report_baton,
apr_pool_t *pool);
svn_error_t *
svn_repos_dir_delta2(svn_fs_root_t *src_root,
const char *src_parent_dir,
const char *src_entry,
svn_fs_root_t *tgt_root,
const char *tgt_path,
const svn_delta_editor_t *editor,
void *edit_baton,
svn_repos_authz_func_t authz_read_func,
void *authz_read_baton,
svn_boolean_t text_deltas,
svn_depth_t depth,
svn_boolean_t entry_props,
svn_boolean_t ignore_ancestry,
apr_pool_t *pool);
svn_error_t *
svn_repos_dir_delta(svn_fs_root_t *src_root,
const char *src_parent_dir,
const char *src_entry,
svn_fs_root_t *tgt_root,
const char *tgt_path,
const svn_delta_editor_t *editor,
void *edit_baton,
svn_repos_authz_func_t authz_read_func,
void *authz_read_baton,
svn_boolean_t text_deltas,
svn_boolean_t recurse,
svn_boolean_t entry_props,
svn_boolean_t ignore_ancestry,
apr_pool_t *pool);
svn_error_t *
svn_repos_replay2(svn_fs_root_t *root,
const char *base_dir,
svn_revnum_t low_water_mark,
svn_boolean_t send_deltas,
const svn_delta_editor_t *editor,
void *edit_baton,
svn_repos_authz_func_t authz_read_func,
void *authz_read_baton,
apr_pool_t *pool);
svn_error_t *
svn_repos_replay(svn_fs_root_t *root,
const svn_delta_editor_t *editor,
void *edit_baton,
apr_pool_t *pool);
svn_error_t *
svn_repos_get_commit_editor5(const svn_delta_editor_t **editor,
void **edit_baton,
svn_repos_t *repos,
svn_fs_txn_t *txn,
const char *repos_url,
const char *base_path,
apr_hash_t *revprop_table,
svn_commit_callback2_t callback,
void *callback_baton,
svn_repos_authz_callback_t authz_callback,
void *authz_baton,
apr_pool_t *pool);
svn_error_t *
svn_repos_get_commit_editor4(const svn_delta_editor_t **editor,
void **edit_baton,
svn_repos_t *repos,
svn_fs_txn_t *txn,
const char *repos_url,
const char *base_path,
const char *user,
const char *log_msg,
svn_commit_callback2_t callback,
void *callback_baton,
svn_repos_authz_callback_t authz_callback,
void *authz_baton,
apr_pool_t *pool);
svn_error_t *
svn_repos_get_commit_editor3(const svn_delta_editor_t **editor,
void **edit_baton,
svn_repos_t *repos,
svn_fs_txn_t *txn,
const char *repos_url,
const char *base_path,
const char *user,
const char *log_msg,
svn_commit_callback_t callback,
void *callback_baton,
svn_repos_authz_callback_t authz_callback,
void *authz_baton,
apr_pool_t *pool);
svn_error_t *
svn_repos_get_commit_editor2(const svn_delta_editor_t **editor,
void **edit_baton,
svn_repos_t *repos,
svn_fs_txn_t *txn,
const char *repos_url,
const char *base_path,
const char *user,
const char *log_msg,
svn_commit_callback_t callback,
void *callback_baton,
apr_pool_t *pool);
svn_error_t *
svn_repos_get_commit_editor(const svn_delta_editor_t **editor,
void **edit_baton,
svn_repos_t *repos,
const char *repos_url,
const char *base_path,
const char *user,
const char *log_msg,
svn_commit_callback_t callback,
void *callback_baton,
apr_pool_t *pool);
svn_error_t *
svn_repos_dated_revision(svn_revnum_t *revision,
svn_repos_t *repos,
apr_time_t tm,
apr_pool_t *pool);
svn_error_t *
svn_repos_get_committed_info(svn_revnum_t *committed_rev,
const char **committed_date,
const char **last_author,
svn_fs_root_t *root,
const char *path,
apr_pool_t *pool);
svn_error_t *
svn_repos_stat(svn_dirent_t **dirent,
svn_fs_root_t *root,
const char *path,
apr_pool_t *pool);
svn_error_t *
svn_repos_deleted_rev(svn_fs_t *fs,
const char *path,
svn_revnum_t start,
svn_revnum_t end,
svn_revnum_t *deleted,
apr_pool_t *pool);
typedef svn_error_t *(*svn_repos_history_func_t)(void *baton,
const char *path,
svn_revnum_t revision,
apr_pool_t *pool);
svn_error_t *
svn_repos_history2(svn_fs_t *fs,
const char *path,
svn_repos_history_func_t history_func,
void *history_baton,
svn_repos_authz_func_t authz_read_func,
void *authz_read_baton,
svn_revnum_t start,
svn_revnum_t end,
svn_boolean_t cross_copies,
apr_pool_t *pool);
svn_error_t *
svn_repos_history(svn_fs_t *fs,
const char *path,
svn_repos_history_func_t history_func,
void *history_baton,
svn_revnum_t start,
svn_revnum_t end,
svn_boolean_t cross_copies,
apr_pool_t *pool);
svn_error_t *
svn_repos_trace_node_locations(svn_fs_t *fs,
apr_hash_t **locations,
const char *fs_path,
svn_revnum_t peg_revision,
apr_array_header_t *location_revisions,
svn_repos_authz_func_t authz_read_func,
void *authz_read_baton,
apr_pool_t *pool);
svn_error_t *
svn_repos_node_location_segments(svn_repos_t *repos,
const char *path,
svn_revnum_t peg_revision,
svn_revnum_t start_rev,
svn_revnum_t end_rev,
svn_location_segment_receiver_t receiver,
void *receiver_baton,
svn_repos_authz_func_t authz_read_func,
void *authz_read_baton,
apr_pool_t *pool);
svn_error_t *
svn_repos_get_logs4(svn_repos_t *repos,
const apr_array_header_t *paths,
svn_revnum_t start,
svn_revnum_t end,
int limit,
svn_boolean_t discover_changed_paths,
svn_boolean_t strict_node_history,
svn_boolean_t include_merged_revisions,
const apr_array_header_t *revprops,
svn_repos_authz_func_t authz_read_func,
void *authz_read_baton,
svn_log_entry_receiver_t receiver,
void *receiver_baton,
apr_pool_t *pool);
svn_error_t *
svn_repos_get_logs3(svn_repos_t *repos,
const apr_array_header_t *paths,
svn_revnum_t start,
svn_revnum_t end,
int limit,
svn_boolean_t discover_changed_paths,
svn_boolean_t strict_node_history,
svn_repos_authz_func_t authz_read_func,
void *authz_read_baton,
svn_log_message_receiver_t receiver,
void *receiver_baton,
apr_pool_t *pool);
svn_error_t *
svn_repos_get_logs2(svn_repos_t *repos,
const apr_array_header_t *paths,
svn_revnum_t start,
svn_revnum_t end,
svn_boolean_t discover_changed_paths,
svn_boolean_t strict_node_history,
svn_repos_authz_func_t authz_read_func,
void *authz_read_baton,
svn_log_message_receiver_t receiver,
void *receiver_baton,
apr_pool_t *pool);
svn_error_t *
svn_repos_get_logs(svn_repos_t *repos,
const apr_array_header_t *paths,
svn_revnum_t start,
svn_revnum_t end,
svn_boolean_t discover_changed_paths,
svn_boolean_t strict_node_history,
svn_log_message_receiver_t receiver,
void *receiver_baton,
apr_pool_t *pool);
svn_error_t *
svn_repos_fs_get_mergeinfo(svn_mergeinfo_catalog_t *catalog,
svn_repos_t *repos,
const apr_array_header_t *paths,
svn_revnum_t revision,
svn_mergeinfo_inheritance_t inherit,
svn_boolean_t include_descendants,
svn_repos_authz_func_t authz_read_func,
void *authz_read_baton,
apr_pool_t *pool);
svn_error_t *
svn_repos_get_file_revs2(svn_repos_t *repos,
const char *path,
svn_revnum_t start,
svn_revnum_t end,
svn_boolean_t include_merged_revisions,
svn_repos_authz_func_t authz_read_func,
void *authz_read_baton,
svn_file_rev_handler_t handler,
void *handler_baton,
apr_pool_t *pool);
svn_error_t *
svn_repos_get_file_revs(svn_repos_t *repos,
const char *path,
svn_revnum_t start,
svn_revnum_t end,
svn_repos_authz_func_t authz_read_func,
void *authz_read_baton,
svn_repos_file_rev_handler_t handler,
void *handler_baton,
apr_pool_t *pool);
svn_error_t *
svn_repos_fs_commit_txn(const char **conflict_p,
svn_repos_t *repos,
svn_revnum_t *new_rev,
svn_fs_txn_t *txn,
apr_pool_t *pool);
svn_error_t *
svn_repos_fs_begin_txn_for_commit2(svn_fs_txn_t **txn_p,
svn_repos_t *repos,
svn_revnum_t rev,
apr_hash_t *revprop_table,
apr_pool_t *pool);
svn_error_t *
svn_repos_fs_begin_txn_for_commit(svn_fs_txn_t **txn_p,
svn_repos_t *repos,
svn_revnum_t rev,
const char *author,
const char *log_msg,
apr_pool_t *pool);
svn_error_t *
svn_repos_fs_begin_txn_for_update(svn_fs_txn_t **txn_p,
svn_repos_t *repos,
svn_revnum_t rev,
const char *author,
apr_pool_t *pool);
svn_error_t *
svn_repos_fs_lock(svn_lock_t **lock,
svn_repos_t *repos,
const char *path,
const char *token,
const char *comment,
svn_boolean_t is_dav_comment,
apr_time_t expiration_date,
svn_revnum_t current_rev,
svn_boolean_t steal_lock,
apr_pool_t *pool);
svn_error_t *
svn_repos_fs_unlock(svn_repos_t *repos,
const char *path,
const char *token,
svn_boolean_t break_lock,
apr_pool_t *pool);
svn_error_t *
svn_repos_fs_get_locks(apr_hash_t **locks,
svn_repos_t *repos,
const char *path,
svn_repos_authz_func_t authz_read_func,
void *authz_read_baton,
apr_pool_t *pool);
svn_error_t *
svn_repos_fs_change_rev_prop3(svn_repos_t *repos,
svn_revnum_t rev,
const char *author,
const char *name,
const svn_string_t *new_value,
svn_boolean_t
use_pre_revprop_change_hook,
svn_boolean_t
use_post_revprop_change_hook,
svn_repos_authz_func_t
authz_read_func,
void *authz_read_baton,
apr_pool_t *pool);
svn_error_t *
svn_repos_fs_change_rev_prop2(svn_repos_t *repos,
svn_revnum_t rev,
const char *author,
const char *name,
const svn_string_t *new_value,
svn_repos_authz_func_t
authz_read_func,
void *authz_read_baton,
apr_pool_t *pool);
svn_error_t *
svn_repos_fs_change_rev_prop(svn_repos_t *repos,
svn_revnum_t rev,
const char *author,
const char *name,
const svn_string_t *new_value,
apr_pool_t *pool);
svn_error_t *
svn_repos_fs_revision_prop(svn_string_t **value_p,
svn_repos_t *repos,
svn_revnum_t rev,
const char *propname,
svn_repos_authz_func_t
authz_read_func,
void *authz_read_baton,
apr_pool_t *pool);
svn_error_t *
svn_repos_fs_revision_proplist(apr_hash_t **table_p,
svn_repos_t *repos,
svn_revnum_t rev,
svn_repos_authz_func_t
authz_read_func,
void *authz_read_baton,
apr_pool_t *pool);
svn_error_t *
svn_repos_fs_change_node_prop(svn_fs_root_t *root,
const char *path,
const char *name,
const svn_string_t *value,
apr_pool_t *pool);
svn_error_t *
svn_repos_fs_change_txn_prop(svn_fs_txn_t *txn,
const char *name,
const svn_string_t *value,
apr_pool_t *pool);
svn_error_t *
svn_repos_fs_change_txn_props(svn_fs_txn_t *txn,
apr_array_header_t *props,
apr_pool_t *pool);
typedef struct svn_repos_node_t {
svn_node_kind_t kind;
char action;
svn_boolean_t text_mod;
svn_boolean_t prop_mod;
const char *name;
svn_revnum_t copyfrom_rev;
const char *copyfrom_path;
struct svn_repos_node_t *sibling;
struct svn_repos_node_t *child;
struct svn_repos_node_t *parent;
} svn_repos_node_t;
svn_error_t *
svn_repos_node_editor(const svn_delta_editor_t **editor,
void **edit_baton,
svn_repos_t *repos,
svn_fs_root_t *base_root,
svn_fs_root_t *root,
apr_pool_t *node_pool,
apr_pool_t *pool);
svn_repos_node_t *svn_repos_node_from_baton(void *edit_baton);
#define SVN_REPOS_DUMPFILE_MAGIC_HEADER "SVN-fs-dump-format-version"
#define SVN_REPOS_DUMPFILE_FORMAT_VERSION 3
#define SVN_REPOS_DUMPFILE_UUID "UUID"
#define SVN_REPOS_DUMPFILE_CONTENT_LENGTH "Content-length"
#define SVN_REPOS_DUMPFILE_REVISION_NUMBER "Revision-number"
#define SVN_REPOS_DUMPFILE_NODE_PATH "Node-path"
#define SVN_REPOS_DUMPFILE_NODE_KIND "Node-kind"
#define SVN_REPOS_DUMPFILE_NODE_ACTION "Node-action"
#define SVN_REPOS_DUMPFILE_NODE_COPYFROM_PATH "Node-copyfrom-path"
#define SVN_REPOS_DUMPFILE_NODE_COPYFROM_REV "Node-copyfrom-rev"
#define SVN_REPOS_DUMPFILE_TEXT_COPY_SOURCE_CHECKSUM "Text-copy-source-md5"
#define SVN_REPOS_DUMPFILE_TEXT_CONTENT_CHECKSUM "Text-content-md5"
#define SVN_REPOS_DUMPFILE_PROP_CONTENT_LENGTH "Prop-content-length"
#define SVN_REPOS_DUMPFILE_TEXT_CONTENT_LENGTH "Text-content-length"
#define SVN_REPOS_DUMPFILE_PROP_DELTA "Prop-delta"
#define SVN_REPOS_DUMPFILE_TEXT_DELTA "Text-delta"
#define SVN_REPOS_DUMPFILE_TEXT_DELTA_BASE_CHECKSUM "Text-delta-base-md5"
enum svn_node_action {
svn_node_action_change,
svn_node_action_add,
svn_node_action_delete,
svn_node_action_replace
};
enum svn_repos_load_uuid {
svn_repos_load_uuid_default,
svn_repos_load_uuid_ignore,
svn_repos_load_uuid_force
};
svn_error_t *
svn_repos_verify_fs(svn_repos_t *repos,
svn_stream_t *feedback_stream,
svn_revnum_t start_rev,
svn_revnum_t end_rev,
svn_cancel_func_t cancel_func,
void *cancel_baton,
apr_pool_t *pool);
svn_error_t *
svn_repos_dump_fs2(svn_repos_t *repos,
svn_stream_t *dumpstream,
svn_stream_t *feedback_stream,
svn_revnum_t start_rev,
svn_revnum_t end_rev,
svn_boolean_t incremental,
svn_boolean_t use_deltas,
svn_cancel_func_t cancel_func,
void *cancel_baton,
apr_pool_t *pool);
svn_error_t *
svn_repos_dump_fs(svn_repos_t *repos,
svn_stream_t *dumpstream,
svn_stream_t *feedback_stream,
svn_revnum_t start_rev,
svn_revnum_t end_rev,
svn_boolean_t incremental,
svn_cancel_func_t cancel_func,
void *cancel_baton,
apr_pool_t *pool);
svn_error_t *
svn_repos_load_fs2(svn_repos_t *repos,
svn_stream_t *dumpstream,
svn_stream_t *feedback_stream,
enum svn_repos_load_uuid uuid_action,
const char *parent_dir,
svn_boolean_t use_pre_commit_hook,
svn_boolean_t use_post_commit_hook,
svn_cancel_func_t cancel_func,
void *cancel_baton,
apr_pool_t *pool);
svn_error_t *
svn_repos_load_fs(svn_repos_t *repos,
svn_stream_t *dumpstream,
svn_stream_t *feedback_stream,
enum svn_repos_load_uuid uuid_action,
const char *parent_dir,
svn_cancel_func_t cancel_func,
void *cancel_baton,
apr_pool_t *pool);
typedef struct svn_repos_parse_fns2_t {
svn_error_t *(*new_revision_record)(void **revision_baton,
apr_hash_t *headers,
void *parse_baton,
apr_pool_t *pool);
svn_error_t *(*uuid_record)(const char *uuid,
void *parse_baton,
apr_pool_t *pool);
svn_error_t *(*new_node_record)(void **node_baton,
apr_hash_t *headers,
void *revision_baton,
apr_pool_t *pool);
svn_error_t *(*set_revision_property)(void *revision_baton,
const char *name,
const svn_string_t *value);
svn_error_t *(*set_node_property)(void *node_baton,
const char *name,
const svn_string_t *value);
svn_error_t *(*delete_node_property)(void *node_baton, const char *name);
svn_error_t *(*remove_node_props)(void *node_baton);
svn_error_t *(*set_fulltext)(svn_stream_t **stream,
void *node_baton);
svn_error_t *(*apply_textdelta)(svn_txdelta_window_handler_t *handler,
void **handler_baton,
void *node_baton);
svn_error_t *(*close_node)(void *node_baton);
svn_error_t *(*close_revision)(void *revision_baton);
} svn_repos_parse_fns2_t;
typedef svn_repos_parse_fns2_t svn_repos_parser_fns2_t;
svn_error_t *
svn_repos_parse_dumpstream2(svn_stream_t *stream,
const svn_repos_parse_fns2_t *parse_fns,
void *parse_baton,
svn_cancel_func_t cancel_func,
void *cancel_baton,
apr_pool_t *pool);
svn_error_t *
svn_repos_get_fs_build_parser2(const svn_repos_parse_fns2_t **parser,
void **parse_baton,
svn_repos_t *repos,
svn_boolean_t use_history,
enum svn_repos_load_uuid uuid_action,
svn_stream_t *outstream,
const char *parent_dir,
apr_pool_t *pool);
typedef struct svn_repos_parse_fns_t {
svn_error_t *(*new_revision_record)(void **revision_baton,
apr_hash_t *headers,
void *parse_baton,
apr_pool_t *pool);
svn_error_t *(*uuid_record)(const char *uuid,
void *parse_baton,
apr_pool_t *pool);
svn_error_t *(*new_node_record)(void **node_baton,
apr_hash_t *headers,
void *revision_baton,
apr_pool_t *pool);
svn_error_t *(*set_revision_property)(void *revision_baton,
const char *name,
const svn_string_t *value);
svn_error_t *(*set_node_property)(void *node_baton,
const char *name,
const svn_string_t *value);
svn_error_t *(*remove_node_props)(void *node_baton);
svn_error_t *(*set_fulltext)(svn_stream_t **stream,
void *node_baton);
svn_error_t *(*close_node)(void *node_baton);
svn_error_t *(*close_revision)(void *revision_baton);
} svn_repos_parser_fns_t;
svn_error_t *
svn_repos_parse_dumpstream(svn_stream_t *stream,
const svn_repos_parser_fns_t *parse_fns,
void *parse_baton,
svn_cancel_func_t cancel_func,
void *cancel_baton,
apr_pool_t *pool);
svn_error_t *
svn_repos_get_fs_build_parser(const svn_repos_parser_fns_t **parser,
void **parse_baton,
svn_repos_t *repos,
svn_boolean_t use_history,
enum svn_repos_load_uuid uuid_action,
svn_stream_t *outstream,
const char *parent_dir,
apr_pool_t *pool);
typedef struct svn_authz_t svn_authz_t;
svn_error_t *
svn_repos_authz_read(svn_authz_t **authz_p,
const char *file,
svn_boolean_t must_exist,
apr_pool_t *pool);
svn_error_t *
svn_repos_authz_check_access(svn_authz_t *authz,
const char *repos_name,
const char *path,
const char *user,
svn_repos_authz_access_t required_access,
svn_boolean_t *access_granted,
apr_pool_t *pool);
typedef enum {
svn_repos_revision_access_none,
svn_repos_revision_access_partial,
svn_repos_revision_access_full
}
svn_repos_revision_access_level_t;
svn_error_t *
svn_repos_check_revision_access(svn_repos_revision_access_level_t *access_level,
svn_repos_t *repos,
svn_revnum_t revision,
svn_repos_authz_func_t authz_read_func,
void *authz_read_baton,
apr_pool_t *pool);
svn_error_t *
svn_repos_remember_client_capabilities(svn_repos_t *repos,
apr_array_header_t *capabilities);
#if defined(__cplusplus)
}
#endif
#endif