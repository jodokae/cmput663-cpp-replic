#if !defined(SVN_CLIENT_H)
#define SVN_CLIENT_H
#include <apr_tables.h>
#include "svn_types.h"
#include "svn_wc.h"
#include "svn_string.h"
#include "svn_error.h"
#include "svn_opt.h"
#include "svn_version.h"
#include "svn_ra.h"
#include "svn_diff.h"
#if defined(__cplusplus)
extern "C" {
#endif
const svn_version_t *svn_client_version(void);
void svn_client_get_simple_prompt_provider
(svn_auth_provider_object_t **provider,
svn_auth_simple_prompt_func_t prompt_func,
void *prompt_baton,
int retry_limit,
apr_pool_t *pool);
void svn_client_get_username_prompt_provider
(svn_auth_provider_object_t **provider,
svn_auth_username_prompt_func_t prompt_func,
void *prompt_baton,
int retry_limit,
apr_pool_t *pool);
void
svn_client_get_simple_provider(svn_auth_provider_object_t **provider,
apr_pool_t *pool);
#if (defined(WIN32) && !defined(__MINGW32__)) || defined(DOXYGEN)
void
svn_client_get_windows_simple_provider(svn_auth_provider_object_t **provider,
apr_pool_t *pool);
#endif
void
svn_client_get_username_provider(svn_auth_provider_object_t **provider,
apr_pool_t *pool);
void
svn_client_get_ssl_server_trust_file_provider
(svn_auth_provider_object_t **provider,
apr_pool_t *pool);
void
svn_client_get_ssl_client_cert_file_provider
(svn_auth_provider_object_t **provider,
apr_pool_t *pool);
void
svn_client_get_ssl_client_cert_pw_file_provider
(svn_auth_provider_object_t **provider,
apr_pool_t *pool);
void
svn_client_get_ssl_server_trust_prompt_provider
(svn_auth_provider_object_t **provider,
svn_auth_ssl_server_trust_prompt_func_t prompt_func,
void *prompt_baton,
apr_pool_t *pool);
void
svn_client_get_ssl_client_cert_prompt_provider
(svn_auth_provider_object_t **provider,
svn_auth_ssl_client_cert_prompt_func_t prompt_func,
void *prompt_baton,
int retry_limit,
apr_pool_t *pool);
void
svn_client_get_ssl_client_cert_pw_prompt_provider
(svn_auth_provider_object_t **provider,
svn_auth_ssl_client_cert_pw_prompt_func_t prompt_func,
void *prompt_baton,
int retry_limit,
apr_pool_t *pool);
typedef struct svn_client_proplist_item_t {
svn_stringbuf_t *node_name;
apr_hash_t *prop_hash;
} svn_client_proplist_item_t;
typedef svn_error_t *(*svn_proplist_receiver_t)
(void *baton,
const char *path,
apr_hash_t *prop_hash,
apr_pool_t *pool);
svn_client_proplist_item_t *
svn_client_proplist_item_dup(const svn_client_proplist_item_t *item,
apr_pool_t *pool);
typedef struct svn_client_commit_info_t {
svn_revnum_t revision;
const char *date;
const char *author;
} svn_client_commit_info_t;
#define SVN_CLIENT_COMMIT_ITEM_ADD 0x01
#define SVN_CLIENT_COMMIT_ITEM_DELETE 0x02
#define SVN_CLIENT_COMMIT_ITEM_TEXT_MODS 0x04
#define SVN_CLIENT_COMMIT_ITEM_PROP_MODS 0x08
#define SVN_CLIENT_COMMIT_ITEM_IS_COPY 0x10
#define SVN_CLIENT_COMMIT_ITEM_LOCK_TOKEN 0x20
typedef struct svn_client_commit_item3_t {
const char *path;
svn_node_kind_t kind;
const char *url;
svn_revnum_t revision;
const char *copyfrom_url;
svn_revnum_t copyfrom_rev;
apr_byte_t state_flags;
apr_array_header_t *incoming_prop_changes;
apr_array_header_t *outgoing_prop_changes;
} svn_client_commit_item3_t;
typedef struct svn_client_commit_item2_t {
const char *path;
svn_node_kind_t kind;
const char *url;
svn_revnum_t revision;
const char *copyfrom_url;
svn_revnum_t copyfrom_rev;
apr_byte_t state_flags;
apr_array_header_t *wcprop_changes;
} svn_client_commit_item2_t;
typedef struct svn_client_commit_item_t {
const char *path;
svn_node_kind_t kind;
const char *url;
svn_revnum_t revision;
const char *copyfrom_url;
apr_byte_t state_flags;
apr_array_header_t *wcprop_changes;
} svn_client_commit_item_t;
svn_error_t *
svn_client_commit_item_create(const svn_client_commit_item3_t **item,
apr_pool_t *pool);
svn_client_commit_item3_t *
svn_client_commit_item3_dup(const svn_client_commit_item3_t *item,
apr_pool_t *pool);
svn_client_commit_item2_t *
svn_client_commit_item2_dup(const svn_client_commit_item2_t *item,
apr_pool_t *pool);
typedef svn_error_t *(*svn_client_get_commit_log3_t)
(const char **log_msg,
const char **tmp_file,
const apr_array_header_t *commit_items,
void *baton,
apr_pool_t *pool);
typedef svn_error_t *(*svn_client_get_commit_log2_t)
(const char **log_msg,
const char **tmp_file,
const apr_array_header_t *commit_items,
void *baton,
apr_pool_t *pool);
typedef svn_error_t *(*svn_client_get_commit_log_t)
(const char **log_msg,
const char **tmp_file,
apr_array_header_t *commit_items,
void *baton,
apr_pool_t *pool);
typedef svn_error_t *(*svn_client_blame_receiver2_t)
(void *baton,
apr_int64_t line_no,
svn_revnum_t revision,
const char *author,
const char *date,
svn_revnum_t merged_revision,
const char *merged_author,
const char *merged_date,
const char *merged_path,
const char *line,
apr_pool_t *pool);
typedef svn_error_t *(*svn_client_blame_receiver_t)
(void *baton,
apr_int64_t line_no,
svn_revnum_t revision,
const char *author,
const char *date,
const char *line,
apr_pool_t *pool);
typedef enum svn_client_diff_summarize_kind_t {
svn_client_diff_summarize_kind_normal,
svn_client_diff_summarize_kind_added,
svn_client_diff_summarize_kind_modified,
svn_client_diff_summarize_kind_deleted
} svn_client_diff_summarize_kind_t;
typedef struct svn_client_diff_summarize_t {
const char *path;
svn_client_diff_summarize_kind_t summarize_kind;
svn_boolean_t prop_changed;
svn_node_kind_t node_kind;
} svn_client_diff_summarize_t;
svn_client_diff_summarize_t *
svn_client_diff_summarize_dup(const svn_client_diff_summarize_t *diff,
apr_pool_t *pool);
typedef svn_error_t *(*svn_client_diff_summarize_func_t)
(const svn_client_diff_summarize_t *diff,
void *baton,
apr_pool_t *pool);
typedef struct svn_client_ctx_t {
svn_auth_baton_t *auth_baton;
svn_wc_notify_func_t notify_func;
void *notify_baton;
svn_client_get_commit_log_t log_msg_func;
void *log_msg_baton;
apr_hash_t *config;
svn_cancel_func_t cancel_func;
void *cancel_baton;
svn_wc_notify_func2_t notify_func2;
void *notify_baton2;
svn_client_get_commit_log2_t log_msg_func2;
void *log_msg_baton2;
svn_ra_progress_notify_func_t progress_func;
void *progress_baton;
svn_client_get_commit_log3_t log_msg_func3;
void *log_msg_baton3;
apr_hash_t *mimetypes_map;
svn_wc_conflict_resolver_func_t conflict_func;
void *conflict_baton;
const char *client_name;
} svn_client_ctx_t;
#define SVN_CLIENT_AUTH_USERNAME "username"
#define SVN_CLIENT_AUTH_PASSWORD "password"
svn_error_t *
svn_client_create_context(svn_client_ctx_t **ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_checkout3(svn_revnum_t *result_rev,
const char *URL,
const char *path,
const svn_opt_revision_t *peg_revision,
const svn_opt_revision_t *revision,
svn_depth_t depth,
svn_boolean_t ignore_externals,
svn_boolean_t allow_unver_obstructions,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_checkout2(svn_revnum_t *result_rev,
const char *URL,
const char *path,
const svn_opt_revision_t *peg_revision,
const svn_opt_revision_t *revision,
svn_boolean_t recurse,
svn_boolean_t ignore_externals,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_checkout(svn_revnum_t *result_rev,
const char *URL,
const char *path,
const svn_opt_revision_t *revision,
svn_boolean_t recurse,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_update3(apr_array_header_t **result_revs,
const apr_array_header_t *paths,
const svn_opt_revision_t *revision,
svn_depth_t depth,
svn_boolean_t depth_is_sticky,
svn_boolean_t ignore_externals,
svn_boolean_t allow_unver_obstructions,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_update2(apr_array_header_t **result_revs,
const apr_array_header_t *paths,
const svn_opt_revision_t *revision,
svn_boolean_t recurse,
svn_boolean_t ignore_externals,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_update(svn_revnum_t *result_rev,
const char *path,
const svn_opt_revision_t *revision,
svn_boolean_t recurse,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_switch2(svn_revnum_t *result_rev,
const char *path,
const char *url,
const svn_opt_revision_t *peg_revision,
const svn_opt_revision_t *revision,
svn_depth_t depth,
svn_boolean_t depth_is_sticky,
svn_boolean_t ignore_externals,
svn_boolean_t allow_unver_obstructions,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_switch(svn_revnum_t *result_rev,
const char *path,
const char *url,
const svn_opt_revision_t *revision,
svn_boolean_t recurse,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_add4(const char *path,
svn_depth_t depth,
svn_boolean_t force,
svn_boolean_t no_ignore,
svn_boolean_t add_parents,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_add3(const char *path,
svn_boolean_t recursive,
svn_boolean_t force,
svn_boolean_t no_ignore,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_add2(const char *path,
svn_boolean_t recursive,
svn_boolean_t force,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_add(const char *path,
svn_boolean_t recursive,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_mkdir3(svn_commit_info_t **commit_info_p,
const apr_array_header_t *paths,
svn_boolean_t make_parents,
const apr_hash_t *revprop_table,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_mkdir2(svn_commit_info_t **commit_info_p,
const apr_array_header_t *paths,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_mkdir(svn_client_commit_info_t **commit_info_p,
const apr_array_header_t *paths,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_delete3(svn_commit_info_t **commit_info_p,
const apr_array_header_t *paths,
svn_boolean_t force,
svn_boolean_t keep_local,
const apr_hash_t *revprop_table,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_delete2(svn_commit_info_t **commit_info_p,
const apr_array_header_t *paths,
svn_boolean_t force,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_delete(svn_client_commit_info_t **commit_info_p,
const apr_array_header_t *paths,
svn_boolean_t force,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_import3(svn_commit_info_t **commit_info_p,
const char *path,
const char *url,
svn_depth_t depth,
svn_boolean_t no_ignore,
svn_boolean_t ignore_unknown_node_types,
const apr_hash_t *revprop_table,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_import2(svn_commit_info_t **commit_info_p,
const char *path,
const char *url,
svn_boolean_t nonrecursive,
svn_boolean_t no_ignore,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_import(svn_client_commit_info_t **commit_info_p,
const char *path,
const char *url,
svn_boolean_t nonrecursive,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_commit4(svn_commit_info_t **commit_info_p,
const apr_array_header_t *targets,
svn_depth_t depth,
svn_boolean_t keep_locks,
svn_boolean_t keep_changelists,
const apr_array_header_t *changelists,
const apr_hash_t *revprop_table,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_commit3(svn_commit_info_t **commit_info_p,
const apr_array_header_t *targets,
svn_boolean_t recurse,
svn_boolean_t keep_locks,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_commit2(svn_client_commit_info_t **commit_info_p,
const apr_array_header_t *targets,
svn_boolean_t recurse,
svn_boolean_t keep_locks,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_commit(svn_client_commit_info_t **commit_info_p,
const apr_array_header_t *targets,
svn_boolean_t nonrecursive,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_status3(svn_revnum_t *result_rev,
const char *path,
const svn_opt_revision_t *revision,
svn_wc_status_func2_t status_func,
void *status_baton,
svn_depth_t depth,
svn_boolean_t get_all,
svn_boolean_t update,
svn_boolean_t no_ignore,
svn_boolean_t ignore_externals,
const apr_array_header_t *changelists,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_status2(svn_revnum_t *result_rev,
const char *path,
const svn_opt_revision_t *revision,
svn_wc_status_func2_t status_func,
void *status_baton,
svn_boolean_t recurse,
svn_boolean_t get_all,
svn_boolean_t update,
svn_boolean_t no_ignore,
svn_boolean_t ignore_externals,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_status(svn_revnum_t *result_rev,
const char *path,
svn_opt_revision_t *revision,
svn_wc_status_func_t status_func,
void *status_baton,
svn_boolean_t recurse,
svn_boolean_t get_all,
svn_boolean_t update,
svn_boolean_t no_ignore,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_log4(const apr_array_header_t *targets,
const svn_opt_revision_t *peg_revision,
const svn_opt_revision_t *start,
const svn_opt_revision_t *end,
int limit,
svn_boolean_t discover_changed_paths,
svn_boolean_t strict_node_history,
svn_boolean_t include_merged_revisions,
const apr_array_header_t *revprops,
svn_log_entry_receiver_t receiver,
void *receiver_baton,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_log3(const apr_array_header_t *targets,
const svn_opt_revision_t *peg_revision,
const svn_opt_revision_t *start,
const svn_opt_revision_t *end,
int limit,
svn_boolean_t discover_changed_paths,
svn_boolean_t strict_node_history,
svn_log_message_receiver_t receiver,
void *receiver_baton,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_log2(const apr_array_header_t *targets,
const svn_opt_revision_t *start,
const svn_opt_revision_t *end,
int limit,
svn_boolean_t discover_changed_paths,
svn_boolean_t strict_node_history,
svn_log_message_receiver_t receiver,
void *receiver_baton,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_log(const apr_array_header_t *targets,
const svn_opt_revision_t *start,
const svn_opt_revision_t *end,
svn_boolean_t discover_changed_paths,
svn_boolean_t strict_node_history,
svn_log_message_receiver_t receiver,
void *receiver_baton,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_blame4(const char *path_or_url,
const svn_opt_revision_t *peg_revision,
const svn_opt_revision_t *start,
const svn_opt_revision_t *end,
const svn_diff_file_options_t *diff_options,
svn_boolean_t ignore_mime_type,
svn_boolean_t include_merged_revisions,
svn_client_blame_receiver2_t receiver,
void *receiver_baton,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_blame3(const char *path_or_url,
const svn_opt_revision_t *peg_revision,
const svn_opt_revision_t *start,
const svn_opt_revision_t *end,
const svn_diff_file_options_t *diff_options,
svn_boolean_t ignore_mime_type,
svn_client_blame_receiver_t receiver,
void *receiver_baton,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_blame2(const char *path_or_url,
const svn_opt_revision_t *peg_revision,
const svn_opt_revision_t *start,
const svn_opt_revision_t *end,
svn_client_blame_receiver_t receiver,
void *receiver_baton,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_blame(const char *path_or_url,
const svn_opt_revision_t *start,
const svn_opt_revision_t *end,
svn_client_blame_receiver_t receiver,
void *receiver_baton,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_diff4(const apr_array_header_t *diff_options,
const char *path1,
const svn_opt_revision_t *revision1,
const char *path2,
const svn_opt_revision_t *revision2,
const char *relative_to_dir,
svn_depth_t depth,
svn_boolean_t ignore_ancestry,
svn_boolean_t no_diff_deleted,
svn_boolean_t ignore_content_type,
const char *header_encoding,
apr_file_t *outfile,
apr_file_t *errfile,
const apr_array_header_t *changelists,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_diff3(const apr_array_header_t *diff_options,
const char *path1,
const svn_opt_revision_t *revision1,
const char *path2,
const svn_opt_revision_t *revision2,
svn_boolean_t recurse,
svn_boolean_t ignore_ancestry,
svn_boolean_t no_diff_deleted,
svn_boolean_t ignore_content_type,
const char *header_encoding,
apr_file_t *outfile,
apr_file_t *errfile,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_diff2(const apr_array_header_t *diff_options,
const char *path1,
const svn_opt_revision_t *revision1,
const char *path2,
const svn_opt_revision_t *revision2,
svn_boolean_t recurse,
svn_boolean_t ignore_ancestry,
svn_boolean_t no_diff_deleted,
svn_boolean_t ignore_content_type,
apr_file_t *outfile,
apr_file_t *errfile,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_diff(const apr_array_header_t *diff_options,
const char *path1,
const svn_opt_revision_t *revision1,
const char *path2,
const svn_opt_revision_t *revision2,
svn_boolean_t recurse,
svn_boolean_t ignore_ancestry,
svn_boolean_t no_diff_deleted,
apr_file_t *outfile,
apr_file_t *errfile,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_diff_peg4(const apr_array_header_t *diff_options,
const char *path,
const svn_opt_revision_t *peg_revision,
const svn_opt_revision_t *start_revision,
const svn_opt_revision_t *end_revision,
const char *relative_to_dir,
svn_depth_t depth,
svn_boolean_t ignore_ancestry,
svn_boolean_t no_diff_deleted,
svn_boolean_t ignore_content_type,
const char *header_encoding,
apr_file_t *outfile,
apr_file_t *errfile,
const apr_array_header_t *changelists,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_diff_peg3(const apr_array_header_t *diff_options,
const char *path,
const svn_opt_revision_t *peg_revision,
const svn_opt_revision_t *start_revision,
const svn_opt_revision_t *end_revision,
svn_boolean_t recurse,
svn_boolean_t ignore_ancestry,
svn_boolean_t no_diff_deleted,
svn_boolean_t ignore_content_type,
const char *header_encoding,
apr_file_t *outfile,
apr_file_t *errfile,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_diff_peg2(const apr_array_header_t *diff_options,
const char *path,
const svn_opt_revision_t *peg_revision,
const svn_opt_revision_t *start_revision,
const svn_opt_revision_t *end_revision,
svn_boolean_t recurse,
svn_boolean_t ignore_ancestry,
svn_boolean_t no_diff_deleted,
svn_boolean_t ignore_content_type,
apr_file_t *outfile,
apr_file_t *errfile,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_diff_peg(const apr_array_header_t *diff_options,
const char *path,
const svn_opt_revision_t *peg_revision,
const svn_opt_revision_t *start_revision,
const svn_opt_revision_t *end_revision,
svn_boolean_t recurse,
svn_boolean_t ignore_ancestry,
svn_boolean_t no_diff_deleted,
apr_file_t *outfile,
apr_file_t *errfile,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_diff_summarize2(const char *path1,
const svn_opt_revision_t *revision1,
const char *path2,
const svn_opt_revision_t *revision2,
svn_depth_t depth,
svn_boolean_t ignore_ancestry,
const apr_array_header_t *changelists,
svn_client_diff_summarize_func_t summarize_func,
void *summarize_baton,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_diff_summarize(const char *path1,
const svn_opt_revision_t *revision1,
const char *path2,
const svn_opt_revision_t *revision2,
svn_boolean_t recurse,
svn_boolean_t ignore_ancestry,
svn_client_diff_summarize_func_t summarize_func,
void *summarize_baton,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_diff_summarize_peg2(const char *path,
const svn_opt_revision_t *peg_revision,
const svn_opt_revision_t *start_revision,
const svn_opt_revision_t *end_revision,
svn_depth_t depth,
svn_boolean_t ignore_ancestry,
const apr_array_header_t *changelists,
svn_client_diff_summarize_func_t summarize_func,
void *summarize_baton,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_diff_summarize_peg(const char *path,
const svn_opt_revision_t *peg_revision,
const svn_opt_revision_t *start_revision,
const svn_opt_revision_t *end_revision,
svn_boolean_t recurse,
svn_boolean_t ignore_ancestry,
svn_client_diff_summarize_func_t summarize_func,
void *summarize_baton,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_merge3(const char *source1,
const svn_opt_revision_t *revision1,
const char *source2,
const svn_opt_revision_t *revision2,
const char *target_wcpath,
svn_depth_t depth,
svn_boolean_t ignore_ancestry,
svn_boolean_t force,
svn_boolean_t record_only,
svn_boolean_t dry_run,
const apr_array_header_t *merge_options,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_merge2(const char *source1,
const svn_opt_revision_t *revision1,
const char *source2,
const svn_opt_revision_t *revision2,
const char *target_wcpath,
svn_boolean_t recurse,
svn_boolean_t ignore_ancestry,
svn_boolean_t force,
svn_boolean_t dry_run,
const apr_array_header_t *merge_options,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_merge(const char *source1,
const svn_opt_revision_t *revision1,
const char *source2,
const svn_opt_revision_t *revision2,
const char *target_wcpath,
svn_boolean_t recurse,
svn_boolean_t ignore_ancestry,
svn_boolean_t force,
svn_boolean_t dry_run,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_merge_reintegrate(const char *source,
const svn_opt_revision_t *peg_revision,
const char *target_wcpath,
svn_boolean_t dry_run,
const apr_array_header_t *merge_options,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_merge_peg3(const char *source,
const apr_array_header_t *ranges_to_merge,
const svn_opt_revision_t *peg_revision,
const char *target_wcpath,
svn_depth_t depth,
svn_boolean_t ignore_ancestry,
svn_boolean_t force,
svn_boolean_t record_only,
svn_boolean_t dry_run,
const apr_array_header_t *merge_options,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_merge_peg2(const char *source,
const svn_opt_revision_t *revision1,
const svn_opt_revision_t *revision2,
const svn_opt_revision_t *peg_revision,
const char *target_wcpath,
svn_boolean_t recurse,
svn_boolean_t ignore_ancestry,
svn_boolean_t force,
svn_boolean_t dry_run,
const apr_array_header_t *merge_options,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_merge_peg(const char *source,
const svn_opt_revision_t *revision1,
const svn_opt_revision_t *revision2,
const svn_opt_revision_t *peg_revision,
const char *target_wcpath,
svn_boolean_t recurse,
svn_boolean_t ignore_ancestry,
svn_boolean_t force,
svn_boolean_t dry_run,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_suggest_merge_sources(apr_array_header_t **suggestions,
const char *path_or_url,
const svn_opt_revision_t *peg_revision,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_mergeinfo_get_merged(apr_hash_t **mergeinfo,
const char *path_or_url,
const svn_opt_revision_t *peg_revision,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_mergeinfo_log_merged(const char *path_or_url,
const svn_opt_revision_t *peg_revision,
const char *merge_source_path_or_url,
const svn_opt_revision_t *src_peg_revision,
svn_log_entry_receiver_t receiver,
void *receiver_baton,
svn_boolean_t discover_changed_paths,
const apr_array_header_t *revprops,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_mergeinfo_log_eligible(const char *path_or_url,
const svn_opt_revision_t *peg_revision,
const char *merge_source_path_or_url,
const svn_opt_revision_t *src_peg_revision,
svn_log_entry_receiver_t receiver,
void *receiver_baton,
svn_boolean_t discover_changed_paths,
const apr_array_header_t *revprops,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_cleanup(const char *dir,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_relocate(const char *dir,
const char *from,
const char *to,
svn_boolean_t recurse,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_revert2(const apr_array_header_t *paths,
svn_depth_t depth,
const apr_array_header_t *changelists,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_revert(const apr_array_header_t *paths,
svn_boolean_t recursive,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_resolved(const char *path,
svn_boolean_t recursive,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_resolve(const char *path,
svn_depth_t depth,
svn_wc_conflict_choice_t conflict_choice,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
typedef struct svn_client_copy_source_t {
const char *path;
const svn_opt_revision_t *revision;
const svn_opt_revision_t *peg_revision;
} svn_client_copy_source_t;
svn_error_t *
svn_client_copy4(svn_commit_info_t **commit_info_p,
apr_array_header_t *sources,
const char *dst_path,
svn_boolean_t copy_as_child,
svn_boolean_t make_parents,
const apr_hash_t *revprop_table,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_copy3(svn_commit_info_t **commit_info_p,
const char *src_path,
const svn_opt_revision_t *src_revision,
const char *dst_path,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_copy2(svn_commit_info_t **commit_info_p,
const char *src_path,
const svn_opt_revision_t *src_revision,
const char *dst_path,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_copy(svn_client_commit_info_t **commit_info_p,
const char *src_path,
const svn_opt_revision_t *src_revision,
const char *dst_path,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_move5(svn_commit_info_t **commit_info_p,
apr_array_header_t *src_paths,
const char *dst_path,
svn_boolean_t force,
svn_boolean_t move_as_child,
svn_boolean_t make_parents,
const apr_hash_t *revprop_table,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_move4(svn_commit_info_t **commit_info_p,
const char *src_path,
const char *dst_path,
svn_boolean_t force,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_move3(svn_commit_info_t **commit_info_p,
const char *src_path,
const char *dst_path,
svn_boolean_t force,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_move2(svn_client_commit_info_t **commit_info_p,
const char *src_path,
const char *dst_path,
svn_boolean_t force,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_move(svn_client_commit_info_t **commit_info_p,
const char *src_path,
const svn_opt_revision_t *src_revision,
const char *dst_path,
svn_boolean_t force,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_propset3(svn_commit_info_t **commit_info_p,
const char *propname,
const svn_string_t *propval,
const char *target,
svn_depth_t depth,
svn_boolean_t skip_checks,
svn_revnum_t base_revision_for_url,
const apr_array_header_t *changelists,
const apr_hash_t *revprop_table,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_propset2(const char *propname,
const svn_string_t *propval,
const char *target,
svn_boolean_t recurse,
svn_boolean_t skip_checks,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_propset(const char *propname,
const svn_string_t *propval,
const char *target,
svn_boolean_t recurse,
apr_pool_t *pool);
svn_error_t *
svn_client_revprop_set(const char *propname,
const svn_string_t *propval,
const char *URL,
const svn_opt_revision_t *revision,
svn_revnum_t *set_rev,
svn_boolean_t force,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_propget3(apr_hash_t **props,
const char *propname,
const char *target,
const svn_opt_revision_t *peg_revision,
const svn_opt_revision_t *revision,
svn_revnum_t *actual_revnum,
svn_depth_t depth,
const apr_array_header_t *changelists,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_propget2(apr_hash_t **props,
const char *propname,
const char *target,
const svn_opt_revision_t *peg_revision,
const svn_opt_revision_t *revision,
svn_boolean_t recurse,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_propget(apr_hash_t **props,
const char *propname,
const char *target,
const svn_opt_revision_t *revision,
svn_boolean_t recurse,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_revprop_get(const char *propname,
svn_string_t **propval,
const char *URL,
const svn_opt_revision_t *revision,
svn_revnum_t *set_rev,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_proplist3(const char *target,
const svn_opt_revision_t *peg_revision,
const svn_opt_revision_t *revision,
svn_depth_t depth,
const apr_array_header_t *changelists,
svn_proplist_receiver_t receiver,
void *receiver_baton,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_proplist2(apr_array_header_t **props,
const char *target,
const svn_opt_revision_t *peg_revision,
const svn_opt_revision_t *revision,
svn_boolean_t recurse,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_proplist(apr_array_header_t **props,
const char *target,
const svn_opt_revision_t *revision,
svn_boolean_t recurse,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_revprop_list(apr_hash_t **props,
const char *URL,
const svn_opt_revision_t *revision,
svn_revnum_t *set_rev,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_export4(svn_revnum_t *result_rev,
const char *from,
const char *to,
const svn_opt_revision_t *peg_revision,
const svn_opt_revision_t *revision,
svn_boolean_t overwrite,
svn_boolean_t ignore_externals,
svn_depth_t depth,
const char *native_eol,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_export3(svn_revnum_t *result_rev,
const char *from,
const char *to,
const svn_opt_revision_t *peg_revision,
const svn_opt_revision_t *revision,
svn_boolean_t overwrite,
svn_boolean_t ignore_externals,
svn_boolean_t recurse,
const char *native_eol,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_export2(svn_revnum_t *result_rev,
const char *from,
const char *to,
svn_opt_revision_t *revision,
svn_boolean_t force,
const char *native_eol,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_export(svn_revnum_t *result_rev,
const char *from,
const char *to,
svn_opt_revision_t *revision,
svn_boolean_t force,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
typedef svn_error_t *(*svn_client_list_func_t)(void *baton,
const char *path,
const svn_dirent_t *dirent,
const svn_lock_t *lock,
const char *abs_path,
apr_pool_t *pool);
svn_error_t *
svn_client_list2(const char *path_or_url,
const svn_opt_revision_t *peg_revision,
const svn_opt_revision_t *revision,
svn_depth_t depth,
apr_uint32_t dirent_fields,
svn_boolean_t fetch_locks,
svn_client_list_func_t list_func,
void *baton,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_list(const char *path_or_url,
const svn_opt_revision_t *peg_revision,
const svn_opt_revision_t *revision,
svn_boolean_t recurse,
apr_uint32_t dirent_fields,
svn_boolean_t fetch_locks,
svn_client_list_func_t list_func,
void *baton,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_ls3(apr_hash_t **dirents,
apr_hash_t **locks,
const char *path_or_url,
const svn_opt_revision_t *peg_revision,
const svn_opt_revision_t *revision,
svn_boolean_t recurse,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_ls2(apr_hash_t **dirents,
const char *path_or_url,
const svn_opt_revision_t *peg_revision,
const svn_opt_revision_t *revision,
svn_boolean_t recurse,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_ls(apr_hash_t **dirents,
const char *path_or_url,
svn_opt_revision_t *revision,
svn_boolean_t recurse,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_cat2(svn_stream_t *out,
const char *path_or_url,
const svn_opt_revision_t *peg_revision,
const svn_opt_revision_t *revision,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_cat(svn_stream_t *out,
const char *path_or_url,
const svn_opt_revision_t *revision,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_add_to_changelist(const apr_array_header_t *paths,
const char *changelist,
svn_depth_t depth,
const apr_array_header_t *changelists,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_remove_from_changelists(const apr_array_header_t *paths,
svn_depth_t depth,
const apr_array_header_t *changelists,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
typedef svn_error_t *(*svn_changelist_receiver_t) (void *baton,
const char *path,
const char *changelist,
apr_pool_t *pool);
svn_error_t *
svn_client_get_changelists(const char *path,
const apr_array_header_t *changelists,
svn_depth_t depth,
svn_changelist_receiver_t callback_func,
void *callback_baton,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_lock(const apr_array_header_t *targets,
const char *comment,
svn_boolean_t steal_lock,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_unlock(const apr_array_header_t *targets,
svn_boolean_t break_lock,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
#define SVN_INFO_SIZE_UNKNOWN ((apr_size_t) -1)
typedef struct svn_info_t {
const char *URL;
svn_revnum_t rev;
svn_node_kind_t kind;
const char *repos_root_URL;
const char *repos_UUID;
svn_revnum_t last_changed_rev;
apr_time_t last_changed_date;
const char *last_changed_author;
svn_lock_t *lock;
svn_boolean_t has_wc_info;
svn_wc_schedule_t schedule;
const char *copyfrom_url;
svn_revnum_t copyfrom_rev;
apr_time_t text_time;
apr_time_t prop_time;
const char *checksum;
const char *conflict_old;
const char *conflict_new;
const char *conflict_wrk;
const char *prejfile;
const char *changelist;
svn_depth_t depth;
apr_size_t working_size;
apr_size_t size;
} svn_info_t;
typedef svn_error_t *(*svn_info_receiver_t)
(void *baton,
const char *path,
const svn_info_t *info,
apr_pool_t *pool);
svn_info_t *
svn_info_dup(const svn_info_t *info, apr_pool_t *pool);
svn_error_t *
svn_client_info2(const char *path_or_url,
const svn_opt_revision_t *peg_revision,
const svn_opt_revision_t *revision,
svn_info_receiver_t receiver,
void *receiver_baton,
svn_depth_t depth,
const apr_array_header_t *changelists,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_info(const char *path_or_url,
const svn_opt_revision_t *peg_revision,
const svn_opt_revision_t *revision,
svn_info_receiver_t receiver,
void *receiver_baton,
svn_boolean_t recurse,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_url_from_path(const char **url,
const char *path_or_url,
apr_pool_t *pool);
svn_error_t *
svn_client_root_url_from_path(const char **url,
const char *path_or_url,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_uuid_from_url(const char **uuid,
const char *url,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_uuid_from_path(const char **uuid,
const char *path,
svn_wc_adm_access_t *adm_access,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client_open_ra_session(svn_ra_session_t **session,
const char *url,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
#if defined(__cplusplus)
}
#endif
#endif
