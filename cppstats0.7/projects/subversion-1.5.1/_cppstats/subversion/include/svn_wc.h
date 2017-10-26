#if !defined(SVN_WC_H)
#define SVN_WC_H
#include <apr.h>
#include <apr_pools.h>
#include <apr_tables.h>
#include <apr_hash.h>
#include "svn_types.h"
#include "svn_string.h"
#include "svn_delta.h"
#include "svn_error.h"
#include "svn_opt.h"
#include "svn_ra.h"
#if defined(__cplusplus)
extern "C" {
#endif
const svn_version_t *svn_wc_version(void);
#define SVN_WC_TRANSLATE_FROM_NF 0x00000000
#define SVN_WC_TRANSLATE_TO_NF 0x00000001
#define SVN_WC_TRANSLATE_FORCE_EOL_REPAIR 0x00000002
#define SVN_WC_TRANSLATE_NO_OUTPUT_CLEANUP 0x00000004
#define SVN_WC_TRANSLATE_FORCE_COPY 0x00000008
#define SVN_WC_TRANSLATE_USE_GLOBAL_TMP 0x00000010
typedef struct svn_wc_adm_access_t svn_wc_adm_access_t;
svn_error_t *
svn_wc_adm_open3(svn_wc_adm_access_t **adm_access,
svn_wc_adm_access_t *associated,
const char *path,
svn_boolean_t write_lock,
int levels_to_lock,
svn_cancel_func_t cancel_func,
void *cancel_baton,
apr_pool_t *pool);
svn_error_t *
svn_wc_adm_open2(svn_wc_adm_access_t **adm_access,
svn_wc_adm_access_t *associated,
const char *path,
svn_boolean_t write_lock,
int levels_to_lock,
apr_pool_t *pool);
svn_error_t *
svn_wc_adm_open(svn_wc_adm_access_t **adm_access,
svn_wc_adm_access_t *associated,
const char *path,
svn_boolean_t write_lock,
svn_boolean_t tree_lock,
apr_pool_t *pool);
svn_error_t *
svn_wc_adm_probe_open3(svn_wc_adm_access_t **adm_access,
svn_wc_adm_access_t *associated,
const char *path,
svn_boolean_t write_lock,
int levels_to_lock,
svn_cancel_func_t cancel_func,
void *cancel_baton,
apr_pool_t *pool);
svn_error_t *
svn_wc_adm_probe_open2(svn_wc_adm_access_t **adm_access,
svn_wc_adm_access_t *associated,
const char *path,
svn_boolean_t write_lock,
int levels_to_lock,
apr_pool_t *pool);
svn_error_t *
svn_wc_adm_probe_open(svn_wc_adm_access_t **adm_access,
svn_wc_adm_access_t *associated,
const char *path,
svn_boolean_t write_lock,
svn_boolean_t tree_lock,
apr_pool_t *pool);
svn_error_t *
svn_wc_adm_open_anchor(svn_wc_adm_access_t **anchor_access,
svn_wc_adm_access_t **target_access,
const char **target,
const char *path,
svn_boolean_t write_lock,
int levels_to_lock,
svn_cancel_func_t cancel_func,
void *cancel_baton,
apr_pool_t *pool);
svn_error_t *
svn_wc_adm_retrieve(svn_wc_adm_access_t **adm_access,
svn_wc_adm_access_t *associated,
const char *path,
apr_pool_t *pool);
svn_error_t *
svn_wc_adm_probe_retrieve(svn_wc_adm_access_t **adm_access,
svn_wc_adm_access_t *associated,
const char *path,
apr_pool_t *pool);
svn_error_t *
svn_wc_adm_probe_try3(svn_wc_adm_access_t **adm_access,
svn_wc_adm_access_t *associated,
const char *path,
svn_boolean_t write_lock,
int levels_to_lock,
svn_cancel_func_t cancel_func,
void *cancel_baton,
apr_pool_t *pool);
svn_error_t *
svn_wc_adm_probe_try2(svn_wc_adm_access_t **adm_access,
svn_wc_adm_access_t *associated,
const char *path,
svn_boolean_t write_lock,
int levels_to_lock,
apr_pool_t *pool);
svn_error_t *
svn_wc_adm_probe_try(svn_wc_adm_access_t **adm_access,
svn_wc_adm_access_t *associated,
const char *path,
svn_boolean_t write_lock,
svn_boolean_t tree_lock,
apr_pool_t *pool);
svn_error_t *svn_wc_adm_close(svn_wc_adm_access_t *adm_access);
const char *svn_wc_adm_access_path(svn_wc_adm_access_t *adm_access);
apr_pool_t *svn_wc_adm_access_pool(svn_wc_adm_access_t *adm_access);
svn_boolean_t svn_wc_adm_locked(svn_wc_adm_access_t *adm_access);
svn_error_t *
svn_wc_locked(svn_boolean_t *locked,
const char *path,
apr_pool_t *pool);
svn_boolean_t svn_wc_is_adm_dir(const char *name, apr_pool_t *pool);
const char *svn_wc_get_adm_dir(apr_pool_t *pool);
svn_error_t *svn_wc_set_adm_dir(const char *name, apr_pool_t *pool);
typedef struct svn_wc_traversal_info_t svn_wc_traversal_info_t;
svn_wc_traversal_info_t *svn_wc_init_traversal_info(apr_pool_t *pool);
void
svn_wc_edited_externals(apr_hash_t **externals_old,
apr_hash_t **externals_new,
svn_wc_traversal_info_t *traversal_info);
void
svn_wc_traversed_depths(apr_hash_t **depths,
svn_wc_traversal_info_t *traversal_info);
typedef struct svn_wc_external_item2_t {
const char *target_dir;
const char *url;
svn_opt_revision_t revision;
svn_opt_revision_t peg_revision;
} svn_wc_external_item2_t;
svn_error_t *
svn_wc_external_item_create(const svn_wc_external_item2_t **item,
apr_pool_t *pool);
svn_wc_external_item2_t *
svn_wc_external_item2_dup(const svn_wc_external_item2_t *item,
apr_pool_t *pool);
typedef struct svn_wc_external_item_t {
const char *target_dir;
const char *url;
svn_opt_revision_t revision;
} svn_wc_external_item_t;
svn_wc_external_item_t *
svn_wc_external_item_dup(const svn_wc_external_item_t *item,
apr_pool_t *pool);
svn_error_t *
svn_wc_parse_externals_description3(apr_array_header_t **externals_p,
const char *parent_directory,
const char *desc,
svn_boolean_t canonicalize_url,
apr_pool_t *pool);
svn_error_t *
svn_wc_parse_externals_description2(apr_array_header_t **externals_p,
const char *parent_directory,
const char *desc,
apr_pool_t *pool);
svn_error_t *
svn_wc_parse_externals_description(apr_hash_t **externals_p,
const char *parent_directory,
const char *desc,
apr_pool_t *pool);
typedef enum svn_wc_notify_action_t {
svn_wc_notify_add = 0,
svn_wc_notify_copy,
svn_wc_notify_delete,
svn_wc_notify_restore,
svn_wc_notify_revert,
svn_wc_notify_failed_revert,
svn_wc_notify_resolved,
svn_wc_notify_skip,
svn_wc_notify_update_delete,
svn_wc_notify_update_add,
svn_wc_notify_update_update,
svn_wc_notify_update_completed,
svn_wc_notify_update_external,
svn_wc_notify_status_completed,
svn_wc_notify_status_external,
svn_wc_notify_commit_modified,
svn_wc_notify_commit_added,
svn_wc_notify_commit_deleted,
svn_wc_notify_commit_replaced,
svn_wc_notify_commit_postfix_txdelta,
svn_wc_notify_blame_revision,
svn_wc_notify_locked,
svn_wc_notify_unlocked,
svn_wc_notify_failed_lock,
svn_wc_notify_failed_unlock,
svn_wc_notify_exists,
svn_wc_notify_changelist_set,
svn_wc_notify_changelist_clear,
svn_wc_notify_changelist_moved,
svn_wc_notify_merge_begin,
svn_wc_notify_foreign_merge_begin,
svn_wc_notify_update_replace
} svn_wc_notify_action_t;
typedef enum svn_wc_notify_state_t {
svn_wc_notify_state_inapplicable = 0,
svn_wc_notify_state_unknown,
svn_wc_notify_state_unchanged,
svn_wc_notify_state_missing,
svn_wc_notify_state_obstructed,
svn_wc_notify_state_changed,
svn_wc_notify_state_merged,
svn_wc_notify_state_conflicted
} svn_wc_notify_state_t;
typedef enum svn_wc_notify_lock_state_t {
svn_wc_notify_lock_state_inapplicable = 0,
svn_wc_notify_lock_state_unknown,
svn_wc_notify_lock_state_unchanged,
svn_wc_notify_lock_state_locked,
svn_wc_notify_lock_state_unlocked
} svn_wc_notify_lock_state_t;
typedef struct svn_wc_notify_t {
const char *path;
svn_wc_notify_action_t action;
svn_node_kind_t kind;
const char *mime_type;
const svn_lock_t *lock;
svn_error_t *err;
svn_wc_notify_state_t content_state;
svn_wc_notify_state_t prop_state;
svn_wc_notify_lock_state_t lock_state;
svn_revnum_t revision;
const char *changelist_name;
svn_merge_range_t *merge_range;
} svn_wc_notify_t;
svn_wc_notify_t *
svn_wc_create_notify(const char *path,
svn_wc_notify_action_t action,
apr_pool_t *pool);
svn_wc_notify_t *
svn_wc_dup_notify(const svn_wc_notify_t *notify,
apr_pool_t *pool);
typedef void (*svn_wc_notify_func2_t)(void *baton,
const svn_wc_notify_t *notify,
apr_pool_t *pool);
typedef void (*svn_wc_notify_func_t)(void *baton,
const char *path,
svn_wc_notify_action_t action,
svn_node_kind_t kind,
const char *mime_type,
svn_wc_notify_state_t content_state,
svn_wc_notify_state_t prop_state,
svn_revnum_t revision);
typedef svn_error_t *(*svn_wc_get_file_t)(void *baton,
const char *path,
svn_revnum_t revision,
svn_stream_t *stream,
svn_revnum_t *fetched_rev,
apr_hash_t **props,
apr_pool_t *pool);
typedef enum svn_wc_conflict_action_t {
svn_wc_conflict_action_edit,
svn_wc_conflict_action_add,
svn_wc_conflict_action_delete
} svn_wc_conflict_action_t;
typedef enum svn_wc_conflict_reason_t {
svn_wc_conflict_reason_edited,
svn_wc_conflict_reason_obstructed,
svn_wc_conflict_reason_deleted,
svn_wc_conflict_reason_missing,
svn_wc_conflict_reason_unversioned
} svn_wc_conflict_reason_t;
typedef enum svn_wc_conflict_kind_t {
svn_wc_conflict_kind_text,
svn_wc_conflict_kind_property
} svn_wc_conflict_kind_t;
typedef struct svn_wc_conflict_description_t {
const char *path;
svn_node_kind_t node_kind;
svn_wc_conflict_kind_t kind;
const char *property_name;
svn_boolean_t is_binary;
const char *mime_type;
svn_wc_adm_access_t *access;
svn_wc_conflict_action_t action;
svn_wc_conflict_reason_t reason;
const char *base_file;
const char *their_file;
const char *my_file;
const char *merged_file;
} svn_wc_conflict_description_t;
typedef enum svn_wc_conflict_choice_t {
svn_wc_conflict_choose_postpone,
svn_wc_conflict_choose_base,
svn_wc_conflict_choose_theirs_full,
svn_wc_conflict_choose_mine_full,
svn_wc_conflict_choose_theirs_conflict,
svn_wc_conflict_choose_mine_conflict,
svn_wc_conflict_choose_merged
} svn_wc_conflict_choice_t;
typedef struct svn_wc_conflict_result_t {
svn_wc_conflict_choice_t choice;
const char *merged_file;
} svn_wc_conflict_result_t;
svn_wc_conflict_result_t *
svn_wc_create_conflict_result(svn_wc_conflict_choice_t choice,
const char *merged_file,
apr_pool_t *pool);
typedef svn_error_t *(*svn_wc_conflict_resolver_func_t)
(svn_wc_conflict_result_t **result,
const svn_wc_conflict_description_t *description,
void *baton,
apr_pool_t *pool);
typedef struct svn_wc_diff_callbacks2_t {
svn_error_t *(*file_changed)(svn_wc_adm_access_t *adm_access,
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
void *diff_baton);
svn_error_t *(*file_added)(svn_wc_adm_access_t *adm_access,
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
void *diff_baton);
svn_error_t *(*file_deleted)(svn_wc_adm_access_t *adm_access,
svn_wc_notify_state_t *state,
const char *path,
const char *tmpfile1,
const char *tmpfile2,
const char *mimetype1,
const char *mimetype2,
apr_hash_t *originalprops,
void *diff_baton);
svn_error_t *(*dir_added)(svn_wc_adm_access_t *adm_access,
svn_wc_notify_state_t *state,
const char *path,
svn_revnum_t rev,
void *diff_baton);
svn_error_t *(*dir_deleted)(svn_wc_adm_access_t *adm_access,
svn_wc_notify_state_t *state,
const char *path,
void *diff_baton);
svn_error_t *(*dir_props_changed)(svn_wc_adm_access_t *adm_access,
svn_wc_notify_state_t *state,
const char *path,
const apr_array_header_t *propchanges,
apr_hash_t *original_props,
void *diff_baton);
} svn_wc_diff_callbacks2_t;
typedef struct svn_wc_diff_callbacks_t {
svn_error_t *(*file_changed)(svn_wc_adm_access_t *adm_access,
svn_wc_notify_state_t *state,
const char *path,
const char *tmpfile1,
const char *tmpfile2,
svn_revnum_t rev1,
svn_revnum_t rev2,
const char *mimetype1,
const char *mimetype2,
void *diff_baton);
svn_error_t *(*file_added)(svn_wc_adm_access_t *adm_access,
svn_wc_notify_state_t *state,
const char *path,
const char *tmpfile1,
const char *tmpfile2,
svn_revnum_t rev1,
svn_revnum_t rev2,
const char *mimetype1,
const char *mimetype2,
void *diff_baton);
svn_error_t *(*file_deleted)(svn_wc_adm_access_t *adm_access,
svn_wc_notify_state_t *state,
const char *path,
const char *tmpfile1,
const char *tmpfile2,
const char *mimetype1,
const char *mimetype2,
void *diff_baton);
svn_error_t *(*dir_added)(svn_wc_adm_access_t *adm_access,
svn_wc_notify_state_t *state,
const char *path,
svn_revnum_t rev,
void *diff_baton);
svn_error_t *(*dir_deleted)(svn_wc_adm_access_t *adm_access,
svn_wc_notify_state_t *state,
const char *path,
void *diff_baton);
svn_error_t *(*props_changed)(svn_wc_adm_access_t *adm_access,
svn_wc_notify_state_t *state,
const char *path,
const apr_array_header_t *propchanges,
apr_hash_t *original_props,
void *diff_baton);
} svn_wc_diff_callbacks_t;
svn_error_t *
svn_wc_check_wc(const char *path,
int *wc_format,
apr_pool_t *pool);
svn_error_t *
svn_wc_has_binary_prop(svn_boolean_t *has_binary_prop,
const char *path,
svn_wc_adm_access_t *adm_access,
apr_pool_t *pool);
svn_error_t *
svn_wc_text_modified_p(svn_boolean_t *modified_p,
const char *filename,
svn_boolean_t force_comparison,
svn_wc_adm_access_t *adm_access,
apr_pool_t *pool);
svn_error_t *
svn_wc_props_modified_p(svn_boolean_t *modified_p,
const char *path,
svn_wc_adm_access_t *adm_access,
apr_pool_t *pool);
#define SVN_WC_ADM_DIR_NAME ".svn"
typedef enum svn_wc_schedule_t {
svn_wc_schedule_normal,
svn_wc_schedule_add,
svn_wc_schedule_delete,
svn_wc_schedule_replace
} svn_wc_schedule_t;
#define SVN_WC_ENTRY_WORKING_SIZE_UNKNOWN -1
typedef struct svn_wc_entry_t {
const char *name;
svn_revnum_t revision;
const char *url;
const char *repos;
const char *uuid;
svn_node_kind_t kind;
svn_wc_schedule_t schedule;
svn_boolean_t copied;
svn_boolean_t deleted;
svn_boolean_t absent;
svn_boolean_t incomplete;
const char *copyfrom_url;
svn_revnum_t copyfrom_rev;
const char *conflict_old;
const char *conflict_new;
const char *conflict_wrk;
const char *prejfile;
apr_time_t text_time;
apr_time_t prop_time;
const char *checksum;
svn_revnum_t cmt_rev;
apr_time_t cmt_date;
const char *cmt_author;
const char *lock_token;
const char *lock_owner;
const char *lock_comment;
apr_time_t lock_creation_date;
svn_boolean_t has_props;
svn_boolean_t has_prop_mods;
const char *cachable_props;
const char *present_props;
const char *changelist;
apr_off_t working_size;
svn_boolean_t keep_local;
svn_depth_t depth;
} svn_wc_entry_t;
#define SVN_WC_ENTRY_THIS_DIR ""
svn_error_t *
svn_wc_entry(const svn_wc_entry_t **entry,
const char *path,
svn_wc_adm_access_t *adm_access,
svn_boolean_t show_hidden,
apr_pool_t *pool);
svn_error_t *
svn_wc_entries_read(apr_hash_t **entries,
svn_wc_adm_access_t *adm_access,
svn_boolean_t show_hidden,
apr_pool_t *pool);
svn_wc_entry_t *
svn_wc_entry_dup(const svn_wc_entry_t *entry,
apr_pool_t *pool);
svn_error_t *
svn_wc_conflicted_p(svn_boolean_t *text_conflicted_p,
svn_boolean_t *prop_conflicted_p,
const char *dir_path,
const svn_wc_entry_t *entry,
apr_pool_t *pool);
svn_error_t *
svn_wc_get_ancestry(char **url,
svn_revnum_t *rev,
const char *path,
svn_wc_adm_access_t *adm_access,
apr_pool_t *pool);
typedef struct svn_wc_entry_callbacks2_t {
svn_error_t *(*found_entry)(const char *path,
const svn_wc_entry_t *entry,
void *walk_baton,
apr_pool_t *pool);
svn_error_t *(*handle_error)(const char *path,
svn_error_t *err,
void *walk_baton,
apr_pool_t *pool);
} svn_wc_entry_callbacks2_t;
typedef struct svn_wc_entry_callbacks_t {
svn_error_t *(*found_entry)(const char *path,
const svn_wc_entry_t *entry,
void *walk_baton,
apr_pool_t *pool);
} svn_wc_entry_callbacks_t;
svn_error_t *
svn_wc_walk_entries3(const char *path,
svn_wc_adm_access_t *adm_access,
const svn_wc_entry_callbacks2_t
*walk_callbacks,
void *walk_baton,
svn_depth_t depth,
svn_boolean_t show_hidden,
svn_cancel_func_t cancel_func,
void *cancel_baton,
apr_pool_t *pool);
svn_error_t *
svn_wc_walk_entries2(const char *path,
svn_wc_adm_access_t *adm_access,
const svn_wc_entry_callbacks_t
*walk_callbacks,
void *walk_baton,
svn_boolean_t show_hidden,
svn_cancel_func_t cancel_func,
void *cancel_baton,
apr_pool_t *pool);
svn_error_t *
svn_wc_walk_entries(const char *path,
svn_wc_adm_access_t *adm_access,
const svn_wc_entry_callbacks_t
*walk_callbacks,
void *walk_baton,
svn_boolean_t show_hidden,
apr_pool_t *pool);
svn_error_t *
svn_wc_mark_missing_deleted(const char *path,
svn_wc_adm_access_t *parent,
apr_pool_t *pool);
svn_error_t *
svn_wc_ensure_adm3(const char *path,
const char *uuid,
const char *url,
const char *repos,
svn_revnum_t revision,
svn_depth_t depth,
apr_pool_t *pool);
svn_error_t *
svn_wc_ensure_adm2(const char *path,
const char *uuid,
const char *url,
const char *repos,
svn_revnum_t revision,
apr_pool_t *pool);
svn_error_t *
svn_wc_ensure_adm(const char *path,
const char *uuid,
const char *url,
svn_revnum_t revision,
apr_pool_t *pool);
svn_error_t *
svn_wc_maybe_set_repos_root(svn_wc_adm_access_t *adm_access,
const char *path,
const char *repos,
apr_pool_t *pool);
enum svn_wc_status_kind {
svn_wc_status_none = 1,
svn_wc_status_unversioned,
svn_wc_status_normal,
svn_wc_status_added,
svn_wc_status_missing,
svn_wc_status_deleted,
svn_wc_status_replaced,
svn_wc_status_modified,
svn_wc_status_merged,
svn_wc_status_conflicted,
svn_wc_status_ignored,
svn_wc_status_obstructed,
svn_wc_status_external,
svn_wc_status_incomplete
};
typedef struct svn_wc_status2_t {
svn_wc_entry_t *entry;
enum svn_wc_status_kind text_status;
enum svn_wc_status_kind prop_status;
svn_boolean_t locked;
svn_boolean_t copied;
svn_boolean_t switched;
enum svn_wc_status_kind repos_text_status;
enum svn_wc_status_kind repos_prop_status;
svn_lock_t *repos_lock;
const char *url;
svn_revnum_t ood_last_cmt_rev;
apr_time_t ood_last_cmt_date;
svn_node_kind_t ood_kind;
const char *ood_last_cmt_author;
} svn_wc_status2_t;
typedef struct svn_wc_status_t {
svn_wc_entry_t *entry;
enum svn_wc_status_kind text_status;
enum svn_wc_status_kind prop_status;
svn_boolean_t locked;
svn_boolean_t copied;
svn_boolean_t switched;
enum svn_wc_status_kind repos_text_status;
enum svn_wc_status_kind repos_prop_status;
} svn_wc_status_t;
svn_wc_status2_t *
svn_wc_dup_status2(svn_wc_status2_t *orig_stat,
apr_pool_t *pool);
svn_wc_status_t *
svn_wc_dup_status(svn_wc_status_t *orig_stat,
apr_pool_t *pool);
svn_error_t *
svn_wc_status2(svn_wc_status2_t **status,
const char *path,
svn_wc_adm_access_t *adm_access,
apr_pool_t *pool);
svn_error_t *
svn_wc_status(svn_wc_status_t **status,
const char *path,
svn_wc_adm_access_t *adm_access,
apr_pool_t *pool);
typedef void (*svn_wc_status_func2_t)(void *baton,
const char *path,
svn_wc_status2_t *status);
typedef void (*svn_wc_status_func_t)(void *baton,
const char *path,
svn_wc_status_t *status);
svn_error_t *
svn_wc_get_status_editor3(const svn_delta_editor_t **editor,
void **edit_baton,
void **set_locks_baton,
svn_revnum_t *edit_revision,
svn_wc_adm_access_t *anchor,
const char *target,
svn_depth_t depth,
svn_boolean_t get_all,
svn_boolean_t no_ignore,
apr_array_header_t *ignore_patterns,
svn_wc_status_func2_t status_func,
void *status_baton,
svn_cancel_func_t cancel_func,
void *cancel_baton,
svn_wc_traversal_info_t *traversal_info,
apr_pool_t *pool);
svn_error_t *
svn_wc_get_status_editor2(const svn_delta_editor_t **editor,
void **edit_baton,
void **set_locks_baton,
svn_revnum_t *edit_revision,
svn_wc_adm_access_t *anchor,
const char *target,
apr_hash_t *config,
svn_boolean_t recurse,
svn_boolean_t get_all,
svn_boolean_t no_ignore,
svn_wc_status_func2_t status_func,
void *status_baton,
svn_cancel_func_t cancel_func,
void *cancel_baton,
svn_wc_traversal_info_t *traversal_info,
apr_pool_t *pool);
svn_error_t *
svn_wc_get_status_editor(const svn_delta_editor_t **editor,
void **edit_baton,
svn_revnum_t *edit_revision,
svn_wc_adm_access_t *anchor,
const char *target,
apr_hash_t *config,
svn_boolean_t recurse,
svn_boolean_t get_all,
svn_boolean_t no_ignore,
svn_wc_status_func_t status_func,
void *status_baton,
svn_cancel_func_t cancel_func,
void *cancel_baton,
svn_wc_traversal_info_t *traversal_info,
apr_pool_t *pool);
svn_error_t *
svn_wc_status_set_repos_locks(void *set_locks_baton,
apr_hash_t *locks,
const char *repos_root,
apr_pool_t *pool);
svn_error_t *
svn_wc_copy2(const char *src,
svn_wc_adm_access_t *dst_parent,
const char *dst_basename,
svn_cancel_func_t cancel_func,
void *cancel_baton,
svn_wc_notify_func2_t notify_func,
void *notify_baton,
apr_pool_t *pool);
svn_error_t *
svn_wc_copy(const char *src,
svn_wc_adm_access_t *dst_parent,
const char *dst_basename,
svn_cancel_func_t cancel_func,
void *cancel_baton,
svn_wc_notify_func_t notify_func,
void *notify_baton,
apr_pool_t *pool);
svn_error_t *
svn_wc_delete3(const char *path,
svn_wc_adm_access_t *adm_access,
svn_cancel_func_t cancel_func,
void *cancel_baton,
svn_wc_notify_func2_t notify_func,
void *notify_baton,
svn_boolean_t keep_local,
apr_pool_t *pool);
svn_error_t *
svn_wc_delete2(const char *path,
svn_wc_adm_access_t *adm_access,
svn_cancel_func_t cancel_func,
void *cancel_baton,
svn_wc_notify_func2_t notify_func,
void *notify_baton,
apr_pool_t *pool);
svn_error_t *
svn_wc_delete(const char *path,
svn_wc_adm_access_t *adm_access,
svn_cancel_func_t cancel_func,
void *cancel_baton,
svn_wc_notify_func_t notify_func,
void *notify_baton,
apr_pool_t *pool);
svn_error_t *
svn_wc_add2(const char *path,
svn_wc_adm_access_t *parent_access,
const char *copyfrom_url,
svn_revnum_t copyfrom_rev,
svn_cancel_func_t cancel_func,
void *cancel_baton,
svn_wc_notify_func2_t notify_func,
void *notify_baton,
apr_pool_t *pool);
svn_error_t *
svn_wc_add(const char *path,
svn_wc_adm_access_t *parent_access,
const char *copyfrom_url,
svn_revnum_t copyfrom_rev,
svn_cancel_func_t cancel_func,
void *cancel_baton,
svn_wc_notify_func_t notify_func,
void *notify_baton,
apr_pool_t *pool);
svn_error_t *
svn_wc_add_repos_file2(const char *dst_path,
svn_wc_adm_access_t *adm_access,
const char *new_text_base_path,
const char *new_text_path,
apr_hash_t *new_base_props,
apr_hash_t *new_props,
const char *copyfrom_url,
svn_revnum_t copyfrom_rev,
apr_pool_t *pool);
svn_error_t *
svn_wc_add_repos_file(const char *dst_path,
svn_wc_adm_access_t *adm_access,
const char *new_text_path,
apr_hash_t *new_props,
const char *copyfrom_url,
svn_revnum_t copyfrom_rev,
apr_pool_t *pool);
svn_error_t *
svn_wc_remove_from_revision_control(svn_wc_adm_access_t *adm_access,
const char *name,
svn_boolean_t destroy_wf,
svn_boolean_t instant_error,
svn_cancel_func_t cancel_func,
void *cancel_baton,
apr_pool_t *pool);
svn_error_t *
svn_wc_resolved_conflict3(const char *path,
svn_wc_adm_access_t *adm_access,
svn_boolean_t resolve_text,
svn_boolean_t resolve_props,
svn_depth_t depth,
svn_wc_conflict_choice_t conflict_choice,
svn_wc_notify_func2_t notify_func,
void *notify_baton,
svn_cancel_func_t cancel_func,
void *cancel_baton,
apr_pool_t *pool);
svn_error_t *
svn_wc_resolved_conflict2(const char *path,
svn_wc_adm_access_t *adm_access,
svn_boolean_t resolve_text,
svn_boolean_t resolve_props,
svn_boolean_t recurse,
svn_wc_notify_func2_t notify_func,
void *notify_baton,
svn_cancel_func_t cancel_func,
void *cancel_baton,
apr_pool_t *pool);
svn_error_t *
svn_wc_resolved_conflict(const char *path,
svn_wc_adm_access_t *adm_access,
svn_boolean_t resolve_text,
svn_boolean_t resolve_props,
svn_boolean_t recurse,
svn_wc_notify_func_t notify_func,
void *notify_baton,
apr_pool_t *pool);
typedef struct svn_wc_committed_queue_t svn_wc_committed_queue_t;
svn_wc_committed_queue_t *
svn_wc_committed_queue_create(apr_pool_t *pool);
svn_error_t *
svn_wc_queue_committed(svn_wc_committed_queue_t **queue,
const char *path,
svn_wc_adm_access_t *adm_access,
svn_boolean_t recurse,
apr_array_header_t *wcprop_changes,
svn_boolean_t remove_lock,
svn_boolean_t remove_changelist,
const unsigned char *digest,
apr_pool_t *pool);
svn_error_t *
svn_wc_process_committed_queue(svn_wc_committed_queue_t *queue,
svn_wc_adm_access_t *adm_access,
svn_revnum_t new_revnum,
const char *rev_date,
const char *rev_author,
apr_pool_t *pool);
svn_error_t *
svn_wc_process_committed4(const char *path,
svn_wc_adm_access_t *adm_access,
svn_boolean_t recurse,
svn_revnum_t new_revnum,
const char *rev_date,
const char *rev_author,
apr_array_header_t *wcprop_changes,
svn_boolean_t remove_lock,
svn_boolean_t remove_changelist,
const unsigned char *digest,
apr_pool_t *pool);
svn_error_t *
svn_wc_process_committed3(const char *path,
svn_wc_adm_access_t *adm_access,
svn_boolean_t recurse,
svn_revnum_t new_revnum,
const char *rev_date,
const char *rev_author,
apr_array_header_t *wcprop_changes,
svn_boolean_t remove_lock,
const unsigned char *digest,
apr_pool_t *pool);
svn_error_t *
svn_wc_process_committed2(const char *path,
svn_wc_adm_access_t *adm_access,
svn_boolean_t recurse,
svn_revnum_t new_revnum,
const char *rev_date,
const char *rev_author,
apr_array_header_t *wcprop_changes,
svn_boolean_t remove_lock,
apr_pool_t *pool);
svn_error_t *
svn_wc_process_committed(const char *path,
svn_wc_adm_access_t *adm_access,
svn_boolean_t recurse,
svn_revnum_t new_revnum,
const char *rev_date,
const char *rev_author,
apr_array_header_t *wcprop_changes,
apr_pool_t *pool);
svn_error_t *
svn_wc_crawl_revisions3(const char *path,
svn_wc_adm_access_t *adm_access,
const svn_ra_reporter3_t *reporter,
void *report_baton,
svn_boolean_t restore_files,
svn_depth_t depth,
svn_boolean_t depth_compatibility_trick,
svn_boolean_t use_commit_times,
svn_wc_notify_func2_t notify_func,
void *notify_baton,
svn_wc_traversal_info_t *traversal_info,
apr_pool_t *pool);
svn_error_t *
svn_wc_crawl_revisions2(const char *path,
svn_wc_adm_access_t *adm_access,
const svn_ra_reporter2_t *reporter,
void *report_baton,
svn_boolean_t restore_files,
svn_boolean_t recurse,
svn_boolean_t use_commit_times,
svn_wc_notify_func2_t notify_func,
void *notify_baton,
svn_wc_traversal_info_t *traversal_info,
apr_pool_t *pool);
svn_error_t *
svn_wc_crawl_revisions(const char *path,
svn_wc_adm_access_t *adm_access,
const svn_ra_reporter_t *reporter,
void *report_baton,
svn_boolean_t restore_files,
svn_boolean_t recurse,
svn_boolean_t use_commit_times,
svn_wc_notify_func_t notify_func,
void *notify_baton,
svn_wc_traversal_info_t *traversal_info,
apr_pool_t *pool);
svn_error_t *
svn_wc_is_wc_root(svn_boolean_t *wc_root,
const char *path,
svn_wc_adm_access_t *adm_access,
apr_pool_t *pool);
svn_error_t *
svn_wc_get_actual_target(const char *path,
const char **anchor,
const char **target,
apr_pool_t *pool);
svn_error_t *
svn_wc_get_update_editor3(svn_revnum_t *target_revision,
svn_wc_adm_access_t *anchor,
const char *target,
svn_boolean_t use_commit_times,
svn_depth_t depth,
svn_boolean_t depth_is_sticky,
svn_boolean_t allow_unver_obstructions,
svn_wc_notify_func2_t notify_func,
void *notify_baton,
svn_cancel_func_t cancel_func,
void *cancel_baton,
svn_wc_conflict_resolver_func_t conflict_func,
void *conflict_baton,
svn_wc_get_file_t fetch_func,
void *fetch_baton,
const char *diff3_cmd,
apr_array_header_t *preserved_exts,
const svn_delta_editor_t **editor,
void **edit_baton,
svn_wc_traversal_info_t *ti,
apr_pool_t *pool);
svn_error_t *
svn_wc_get_update_editor2(svn_revnum_t *target_revision,
svn_wc_adm_access_t *anchor,
const char *target,
svn_boolean_t use_commit_times,
svn_boolean_t recurse,
svn_wc_notify_func2_t notify_func,
void *notify_baton,
svn_cancel_func_t cancel_func,
void *cancel_baton,
const char *diff3_cmd,
const svn_delta_editor_t **editor,
void **edit_baton,
svn_wc_traversal_info_t *ti,
apr_pool_t *pool);
svn_error_t *
svn_wc_get_update_editor(svn_revnum_t *target_revision,
svn_wc_adm_access_t *anchor,
const char *target,
svn_boolean_t use_commit_times,
svn_boolean_t recurse,
svn_wc_notify_func_t notify_func,
void *notify_baton,
svn_cancel_func_t cancel_func,
void *cancel_baton,
const char *diff3_cmd,
const svn_delta_editor_t **editor,
void **edit_baton,
svn_wc_traversal_info_t *ti,
apr_pool_t *pool);
svn_error_t *
svn_wc_get_switch_editor3(svn_revnum_t *target_revision,
svn_wc_adm_access_t *anchor,
const char *target,
const char *switch_url,
svn_boolean_t use_commit_times,
svn_depth_t depth,
svn_boolean_t depth_is_sticky,
svn_boolean_t allow_unver_obstructions,
svn_wc_notify_func2_t notify_func,
void *notify_baton,
svn_cancel_func_t cancel_func,
void *cancel_baton,
svn_wc_conflict_resolver_func_t conflict_func,
void *conflict_baton,
const char *diff3_cmd,
apr_array_header_t *preserved_exts,
const svn_delta_editor_t **editor,
void **edit_baton,
svn_wc_traversal_info_t *ti,
apr_pool_t *pool);
svn_error_t *
svn_wc_get_switch_editor2(svn_revnum_t *target_revision,
svn_wc_adm_access_t *anchor,
const char *target,
const char *switch_url,
svn_boolean_t use_commit_times,
svn_boolean_t recurse,
svn_wc_notify_func2_t notify_func,
void *notify_baton,
svn_cancel_func_t cancel_func,
void *cancel_baton,
const char *diff3_cmd,
const svn_delta_editor_t **editor,
void **edit_baton,
svn_wc_traversal_info_t *ti,
apr_pool_t *pool);
svn_error_t *
svn_wc_get_switch_editor(svn_revnum_t *target_revision,
svn_wc_adm_access_t *anchor,
const char *target,
const char *switch_url,
svn_boolean_t use_commit_times,
svn_boolean_t recurse,
svn_wc_notify_func_t notify_func,
void *notify_baton,
svn_cancel_func_t cancel_func,
void *cancel_baton,
const char *diff3_cmd,
const svn_delta_editor_t **editor,
void **edit_baton,
svn_wc_traversal_info_t *ti,
apr_pool_t *pool);
svn_error_t *
svn_wc_prop_list(apr_hash_t **props,
const char *path,
svn_wc_adm_access_t *adm_access,
apr_pool_t *pool);
svn_error_t *
svn_wc_prop_get(const svn_string_t **value,
const char *name,
const char *path,
svn_wc_adm_access_t *adm_access,
apr_pool_t *pool);
svn_error_t *
svn_wc_prop_set2(const char *name,
const svn_string_t *value,
const char *path,
svn_wc_adm_access_t *adm_access,
svn_boolean_t skip_checks,
apr_pool_t *pool);
svn_error_t *
svn_wc_prop_set(const char *name,
const svn_string_t *value,
const char *path,
svn_wc_adm_access_t *adm_access,
apr_pool_t *pool);
svn_boolean_t svn_wc_is_normal_prop(const char *name);
svn_boolean_t svn_wc_is_wc_prop(const char *name);
svn_boolean_t svn_wc_is_entry_prop(const char *name);
typedef svn_error_t *(*svn_wc_canonicalize_svn_prop_get_file_t)
(const svn_string_t **mime_type,
svn_stream_t *stream,
void *baton,
apr_pool_t *pool);
svn_error_t *
svn_wc_canonicalize_svn_prop(const svn_string_t **propval_p,
const char *propname,
const svn_string_t *propval,
const char *path,
svn_node_kind_t kind,
svn_boolean_t skip_some_checks,
svn_wc_canonicalize_svn_prop_get_file_t prop_getter,
void *getter_baton,
apr_pool_t *pool);
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
apr_pool_t *pool);
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
apr_pool_t *pool);
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
apr_pool_t *pool);
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
apr_pool_t *pool);
svn_error_t *
svn_wc_diff4(svn_wc_adm_access_t *anchor,
const char *target,
const svn_wc_diff_callbacks2_t *callbacks,
void *callback_baton,
svn_depth_t depth,
svn_boolean_t ignore_ancestry,
const apr_array_header_t *changelists,
apr_pool_t *pool);
svn_error_t *
svn_wc_diff3(svn_wc_adm_access_t *anchor,
const char *target,
const svn_wc_diff_callbacks2_t *callbacks,
void *callback_baton,
svn_boolean_t recurse,
svn_boolean_t ignore_ancestry,
apr_pool_t *pool);
svn_error_t *
svn_wc_diff2(svn_wc_adm_access_t *anchor,
const char *target,
const svn_wc_diff_callbacks_t *callbacks,
void *callback_baton,
svn_boolean_t recurse,
svn_boolean_t ignore_ancestry,
apr_pool_t *pool);
svn_error_t *
svn_wc_diff(svn_wc_adm_access_t *anchor,
const char *target,
const svn_wc_diff_callbacks_t *callbacks,
void *callback_baton,
svn_boolean_t recurse,
apr_pool_t *pool);
svn_error_t *
svn_wc_get_prop_diffs(apr_array_header_t **propchanges,
apr_hash_t **original_props,
const char *path,
svn_wc_adm_access_t *adm_access,
apr_pool_t *pool);
typedef enum svn_wc_merge_outcome_t {
svn_wc_merge_unchanged,
svn_wc_merge_merged,
svn_wc_merge_conflict,
svn_wc_merge_no_merge
} svn_wc_merge_outcome_t;
svn_error_t *
svn_wc_merge3(enum svn_wc_merge_outcome_t *merge_outcome,
const char *left,
const char *right,
const char *merge_target,
svn_wc_adm_access_t *adm_access,
const char *left_label,
const char *right_label,
const char *target_label,
svn_boolean_t dry_run,
const char *diff3_cmd,
const apr_array_header_t *merge_options,
const apr_array_header_t *prop_diff,
svn_wc_conflict_resolver_func_t conflict_func,
void *conflict_baton,
apr_pool_t *pool);
svn_error_t *
svn_wc_merge2(enum svn_wc_merge_outcome_t *merge_outcome,
const char *left,
const char *right,
const char *merge_target,
svn_wc_adm_access_t *adm_access,
const char *left_label,
const char *right_label,
const char *target_label,
svn_boolean_t dry_run,
const char *diff3_cmd,
const apr_array_header_t *merge_options,
apr_pool_t *pool);
svn_error_t *
svn_wc_merge(const char *left,
const char *right,
const char *merge_target,
svn_wc_adm_access_t *adm_access,
const char *left_label,
const char *right_label,
const char *target_label,
svn_boolean_t dry_run,
enum svn_wc_merge_outcome_t *merge_outcome,
const char *diff3_cmd,
apr_pool_t *pool);
svn_error_t *
svn_wc_merge_props2(svn_wc_notify_state_t *state,
const char *path,
svn_wc_adm_access_t *adm_access,
apr_hash_t *baseprops,
const apr_array_header_t *propchanges,
svn_boolean_t base_merge,
svn_boolean_t dry_run,
svn_wc_conflict_resolver_func_t conflict_func,
void *conflict_baton,
apr_pool_t *pool);
svn_error_t *
svn_wc_merge_props(svn_wc_notify_state_t *state,
const char *path,
svn_wc_adm_access_t *adm_access,
apr_hash_t *baseprops,
const apr_array_header_t *propchanges,
svn_boolean_t base_merge,
svn_boolean_t dry_run,
apr_pool_t *pool);
svn_error_t *
svn_wc_merge_prop_diffs(svn_wc_notify_state_t *state,
const char *path,
svn_wc_adm_access_t *adm_access,
const apr_array_header_t *propchanges,
svn_boolean_t base_merge,
svn_boolean_t dry_run,
apr_pool_t *pool);
svn_error_t *
svn_wc_get_pristine_copy_path(const char *path,
const char **pristine_path,
apr_pool_t *pool);
svn_error_t *
svn_wc_cleanup2(const char *path,
const char *diff3_cmd,
svn_cancel_func_t cancel_func,
void *cancel_baton,
apr_pool_t *pool);
svn_error_t *
svn_wc_cleanup(const char *path,
svn_wc_adm_access_t *optional_adm_access,
const char *diff3_cmd,
svn_cancel_func_t cancel_func,
void *cancel_baton,
apr_pool_t *pool);
typedef svn_error_t *(*svn_wc_relocation_validator3_t)(void *baton,
const char *uuid,
const char *url,
const char *root_url,
apr_pool_t *pool);
typedef svn_error_t *(*svn_wc_relocation_validator2_t)(void *baton,
const char *uuid,
const char *url,
svn_boolean_t root,
apr_pool_t *pool);
typedef svn_error_t *(*svn_wc_relocation_validator_t)(void *baton,
const char *uuid,
const char *url);
svn_error_t *
svn_wc_relocate3(const char *path,
svn_wc_adm_access_t *adm_access,
const char *from,
const char *to,
svn_boolean_t recurse,
svn_wc_relocation_validator3_t validator,
void *validator_baton,
apr_pool_t *pool);
svn_error_t *
svn_wc_relocate2(const char *path,
svn_wc_adm_access_t *adm_access,
const char *from,
const char *to,
svn_boolean_t recurse,
svn_wc_relocation_validator2_t validator,
void *validator_baton,
apr_pool_t *pool);
svn_error_t *
svn_wc_relocate(const char *path,
svn_wc_adm_access_t *adm_access,
const char *from,
const char *to,
svn_boolean_t recurse,
svn_wc_relocation_validator_t validator,
void *validator_baton,
apr_pool_t *pool);
svn_error_t *
svn_wc_revert3(const char *path,
svn_wc_adm_access_t *parent_access,
svn_depth_t depth,
svn_boolean_t use_commit_times,
const apr_array_header_t *changelists,
svn_cancel_func_t cancel_func,
void *cancel_baton,
svn_wc_notify_func2_t notify_func,
void *notify_baton,
apr_pool_t *pool);
svn_error_t *
svn_wc_revert2(const char *path,
svn_wc_adm_access_t *parent_access,
svn_boolean_t recursive,
svn_boolean_t use_commit_times,
svn_cancel_func_t cancel_func,
void *cancel_baton,
svn_wc_notify_func2_t notify_func,
void *notify_baton,
apr_pool_t *pool);
svn_error_t *
svn_wc_revert(const char *path,
svn_wc_adm_access_t *parent_access,
svn_boolean_t recursive,
svn_boolean_t use_commit_times,
svn_cancel_func_t cancel_func,
void *cancel_baton,
svn_wc_notify_func_t notify_func,
void *notify_baton,
apr_pool_t *pool);
svn_error_t *
svn_wc_create_tmp_file2(apr_file_t **fp,
const char **new_name,
const char *path,
svn_io_file_del_t delete_when,
apr_pool_t *pool);
svn_error_t *
svn_wc_create_tmp_file(apr_file_t **fp,
const char *path,
svn_boolean_t delete_on_close,
apr_pool_t *pool);
svn_error_t *
svn_wc_translated_file2(const char **xlated_path,
const char *src,
const char *versioned_file,
svn_wc_adm_access_t *adm_access,
apr_uint32_t flags,
apr_pool_t *pool);
svn_error_t *
svn_wc_translated_file(const char **xlated_p,
const char *vfile,
svn_wc_adm_access_t *adm_access,
svn_boolean_t force_repair,
apr_pool_t *pool);
svn_error_t *
svn_wc_translated_stream(svn_stream_t **stream,
const char *path,
const char *versioned_file,
svn_wc_adm_access_t *adm_access,
apr_uint32_t flags,
apr_pool_t *pool);
svn_error_t *
svn_wc_transmit_text_deltas2(const char **tempfile,
unsigned char digest[],
const char *path,
svn_wc_adm_access_t *adm_access,
svn_boolean_t fulltext,
const svn_delta_editor_t *editor,
void *file_baton,
apr_pool_t *pool);
svn_error_t *
svn_wc_transmit_text_deltas(const char *path,
svn_wc_adm_access_t *adm_access,
svn_boolean_t fulltext,
const svn_delta_editor_t *editor,
void *file_baton,
const char **tempfile,
apr_pool_t *pool);
svn_error_t *
svn_wc_transmit_prop_deltas(const char *path,
svn_wc_adm_access_t *adm_access,
const svn_wc_entry_t *entry,
const svn_delta_editor_t *editor,
void *baton,
const char **tempfile,
apr_pool_t *pool);
svn_error_t *
svn_wc_get_default_ignores(apr_array_header_t **patterns,
apr_hash_t *config,
apr_pool_t *pool);
svn_error_t *
svn_wc_get_ignores(apr_array_header_t **patterns,
apr_hash_t *config,
svn_wc_adm_access_t *adm_access,
apr_pool_t *pool);
svn_boolean_t
svn_wc_match_ignore_list(const char *str,
apr_array_header_t *list,
apr_pool_t *pool);
svn_error_t *
svn_wc_add_lock(const char *path,
const svn_lock_t *lock,
svn_wc_adm_access_t *adm_access,
apr_pool_t *pool);
svn_error_t *
svn_wc_remove_lock(const char *path,
svn_wc_adm_access_t *adm_access,
apr_pool_t *pool);
typedef struct svn_wc_revision_status_t {
svn_revnum_t min_rev;
svn_revnum_t max_rev;
svn_boolean_t switched;
svn_boolean_t modified;
svn_boolean_t sparse_checkout;
}
svn_wc_revision_status_t;
svn_error_t *
svn_wc_revision_status(svn_wc_revision_status_t **result_p,
const char *wc_path,
const char *trail_url,
svn_boolean_t committed,
svn_cancel_func_t cancel_func,
void *cancel_baton,
apr_pool_t *pool);
svn_error_t *
svn_wc_set_changelist(const char *path,
const char *changelist,
svn_wc_adm_access_t *adm_access,
svn_cancel_func_t cancel_func,
void *cancel_baton,
svn_wc_notify_func2_t notify_func,
void *notify_baton,
apr_pool_t *pool);
#if defined(__cplusplus)
}
#endif
#endif
