#if !defined(SVN_LIBSVN_CLIENT_H)
#define SVN_LIBSVN_CLIENT_H
#include <apr_pools.h>
#include "svn_types.h"
#include "svn_opt.h"
#include "svn_string.h"
#include "svn_error.h"
#include "svn_ra.h"
#include "svn_client.h"
#if defined(__cplusplus)
extern "C" {
#endif
svn_error_t *
svn_client__derive_location(const char **url,
svn_revnum_t *peg_revnum,
const char *path_or_url,
const svn_opt_revision_t *peg_revision,
const svn_ra_session_t *ra_session,
svn_wc_adm_access_t *adm_access,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client__entry_location(const char **url,
svn_revnum_t *revnum,
const char *path_or_url,
enum svn_opt_revision_kind peg_rev_kind,
const svn_wc_entry_t *entry,
apr_pool_t *pool);
svn_error_t *
svn_client__get_revision_number(svn_revnum_t *revnum,
svn_revnum_t *youngest_rev,
svn_ra_session_t *ra_session,
const svn_opt_revision_t *revision,
const char *path,
apr_pool_t *pool);
svn_boolean_t
svn_client__compare_revisions(svn_opt_revision_t *revision1,
svn_opt_revision_t *revision2);
svn_boolean_t
svn_client__revision_is_local(const svn_opt_revision_t *revision);
svn_error_t *svn_client__get_copy_source(const char *path_or_url,
const svn_opt_revision_t *revision,
const char **copyfrom_path,
svn_revnum_t *copyfrom_rev,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client__repos_locations(const char **start_url,
svn_opt_revision_t **start_revision,
const char **end_url,
svn_opt_revision_t **end_revision,
svn_ra_session_t *ra_session,
const char *path,
const svn_opt_revision_t *revision,
const svn_opt_revision_t *start,
const svn_opt_revision_t *end,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client__repos_location_segments(apr_array_header_t **segments,
svn_ra_session_t *ra_session,
const char *path,
svn_revnum_t peg_revision,
svn_revnum_t start_revision,
svn_revnum_t end_revision,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client__get_youngest_common_ancestor(const char **ancestor_path,
svn_revnum_t *ancestor_revision,
const char *path_or_url1,
svn_revnum_t rev1,
const char *path_or_url2,
svn_revnum_t rev2,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client__ra_session_from_path(svn_ra_session_t **ra_session_p,
svn_revnum_t *rev_p,
const char **url_p,
const char *path_or_url,
svn_wc_adm_access_t *base_access,
const svn_opt_revision_t *peg_revision,
const svn_opt_revision_t *revision,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client__path_relative_to_session(const char **rel_path,
svn_ra_session_t *ra_session,
const char *url,
apr_pool_t *pool);
svn_error_t *
svn_client__ensure_ra_session_url(const char **old_session_url,
svn_ra_session_t *ra_session,
const char *session_url,
apr_pool_t *pool);
svn_error_t *
svn_client__get_repos_root(const char **repos_root,
const char *path_or_url,
const svn_opt_revision_t *peg_revision,
svn_wc_adm_access_t *adm_access,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client__path_relative_to_root(const char **rel_path,
const char *path_or_url,
const char *repos_root,
svn_boolean_t include_leading_slash,
svn_ra_session_t *ra_session,
svn_wc_adm_access_t *adm_access,
apr_pool_t *pool);
svn_error_t *
svn_client__get_prop_from_wc(apr_hash_t *props,
const char *propname,
const char *target,
svn_boolean_t pristine,
const svn_wc_entry_t *entry,
svn_wc_adm_access_t *adm_access,
svn_depth_t depth,
const apr_array_header_t *changelists,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client__oldest_rev_at_path(svn_revnum_t *oldest_rev,
svn_ra_session_t *ra_session,
const char *rel_path,
svn_revnum_t rev,
apr_pool_t *pool);
svn_error_t *
svn_client__default_walker_error_handler(const char *path,
svn_error_t *err,
void *walk_baton,
apr_pool_t *pool);
#define SVN_CLIENT__HAS_LOG_MSG_FUNC(ctx) ((ctx)->log_msg_func3 || (ctx)->log_msg_func2 || (ctx)->log_msg_func)
typedef struct {
const char *base_dir;
svn_wc_adm_access_t *base_access;
svn_boolean_t read_only_wc;
apr_array_header_t *commit_items;
svn_client_ctx_t *ctx;
apr_pool_t *pool;
} svn_client__callback_baton_t;
svn_error_t *
svn_client__open_ra_session_internal(svn_ra_session_t **ra_session,
const char *base_url,
const char *base_dir,
svn_wc_adm_access_t *base_access,
apr_array_header_t *commit_items,
svn_boolean_t use_admin,
svn_boolean_t read_only_wc,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *svn_client__commit_get_baton(void **baton,
svn_commit_info_t **info,
apr_pool_t *pool);
svn_error_t *svn_client__commit_callback(const svn_commit_info_t *commit_info,
void *baton,
apr_pool_t *pool);
svn_error_t * svn_client__can_delete(const char *path,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *svn_client__get_auto_props(apr_hash_t **properties,
const char **mimetype,
const char *path,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t * svn_client__wc_delete(const char *path,
svn_wc_adm_access_t *adm_access,
svn_boolean_t force,
svn_boolean_t dry_run,
svn_boolean_t keep_local,
svn_wc_notify_func2_t notify_func,
void *notify_baton,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
apr_hash_t *svn_client__dry_run_deletions(void *merge_cmd_baton);
svn_error_t *
svn_client__make_local_parents(const char *path,
svn_boolean_t make_parents,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client__update_internal(svn_revnum_t *result_rev,
const char *path,
const svn_opt_revision_t *revision,
svn_depth_t depth,
svn_boolean_t depth_is_sticky,
svn_boolean_t ignore_externals,
svn_boolean_t allow_unver_obstructions,
svn_boolean_t *timestamp_sleep,
svn_boolean_t send_copyfrom_args,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client__checkout_internal(svn_revnum_t *result_rev,
const char *URL,
const char *path,
const svn_opt_revision_t *peg_revision,
const svn_opt_revision_t *revision,
svn_depth_t depth,
svn_boolean_t ignore_externals,
svn_boolean_t allow_unver_obstructions,
svn_boolean_t *timestamp_sleep,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client__switch_internal(svn_revnum_t *result_rev,
const char *path,
const char *url,
const svn_opt_revision_t *peg_revision,
const svn_opt_revision_t *revision,
svn_depth_t depth,
svn_boolean_t depth_is_sticky,
svn_boolean_t *timestamp_sleep,
svn_boolean_t ignore_externals,
svn_boolean_t allow_unver_obstructions,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client__get_diff_editor(const char *target,
svn_wc_adm_access_t *adm_access,
const svn_wc_diff_callbacks2_t *diff_cmd,
void *diff_cmd_baton,
svn_depth_t depth,
svn_boolean_t dry_run,
svn_ra_session_t *ra_session,
svn_revnum_t revision,
svn_wc_notify_func2_t notify_func,
void *notify_baton,
svn_cancel_func_t cancel_func,
void *cancel_baton,
const svn_delta_editor_t **editor,
void **edit_baton,
apr_pool_t *pool);
svn_error_t *
svn_client__get_diff_summarize_editor(const char *target,
svn_client_diff_summarize_func_t
summarize_func,
void *summarize_baton,
svn_ra_session_t *ra_session,
svn_revnum_t revision,
svn_cancel_func_t cancel_func,
void *cancel_baton,
const svn_delta_editor_t **editor,
void **edit_baton,
apr_pool_t *pool);
typedef struct {
const char *src;
const char *src_abs;
const char *base_name;
svn_node_kind_t src_kind;
const char *src_original;
svn_opt_revision_t src_op_revision;
svn_opt_revision_t src_peg_revision;
svn_revnum_t src_revnum;
const char *dst;
const char *dst_parent;
} svn_client__copy_pair_t;
#define SVN_CLIENT__SINGLE_REPOS_NAME "svn:single-repos"
svn_error_t *
svn_client__harvest_committables(apr_hash_t **committables,
apr_hash_t **lock_tokens,
svn_wc_adm_access_t *parent_dir,
apr_array_header_t *targets,
svn_depth_t depth,
svn_boolean_t just_locked,
const apr_array_header_t *changelists,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client__get_copy_committables(apr_hash_t **committables,
const apr_array_header_t *copy_pairs,
svn_wc_adm_access_t *adm_access,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
int svn_client__sort_commit_item_urls(const void *a, const void *b);
svn_error_t *
svn_client__condense_commit_items(const char **base_url,
apr_array_header_t *commit_items,
apr_pool_t *pool);
svn_error_t *
svn_client__do_commit(const char *base_url,
apr_array_header_t *commit_items,
svn_wc_adm_access_t *adm_access,
const svn_delta_editor_t *editor,
void *edit_baton,
const char *notify_path_prefix,
apr_hash_t **tempfiles,
apr_hash_t **digests,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client__handle_externals(svn_wc_traversal_info_t *traversal_info,
const char *from_url,
const char *to_path,
const char *repos_root_url,
svn_depth_t requested_depth,
svn_boolean_t update_unchanged,
svn_boolean_t *timestamp_sleep,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client__fetch_externals(apr_hash_t *externals,
const char *from_url,
const char *to_path,
const char *repos_root_url,
svn_depth_t requested_depth,
svn_boolean_t is_export,
svn_boolean_t *timestamp_sleep,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client__do_external_status(svn_wc_traversal_info_t *traversal_info,
svn_wc_status_func2_t status_func,
void *status_baton,
svn_depth_t depth,
svn_boolean_t get_all,
svn_boolean_t update,
svn_boolean_t no_ignore,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client__get_log_msg(const char **log_msg,
const char **tmp_file,
const apr_array_header_t *commit_items,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client__ensure_revprop_table(apr_hash_t **revprop_table_out,
const apr_hash_t *revprop_table_in,
const char *log_msg,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
#if defined(__cplusplus)
}
#endif
#endif
