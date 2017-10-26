#if !defined(SVN_LIBSVN_CLIENT_MERGEINFO_H)
#define SVN_LIBSVN_CLIENT_MERGEINFO_H
#include "svn_wc.h"
#include "svn_client.h"
typedef struct svn_client__merge_path_t {
const char *path;
svn_boolean_t missing_child;
svn_boolean_t switched;
svn_boolean_t has_noninheritable;
svn_boolean_t absent;
apr_array_header_t *remaining_ranges;
svn_mergeinfo_t pre_merge_mergeinfo;
svn_mergeinfo_t implicit_mergeinfo;
svn_boolean_t indirect_mergeinfo;
svn_boolean_t scheduled_for_deletion;
} svn_client__merge_path_t;
svn_error_t *
svn_client__get_wc_mergeinfo(svn_mergeinfo_t *mergeinfo,
svn_boolean_t *inherited,
svn_boolean_t pristine,
svn_mergeinfo_inheritance_t inherit,
const svn_wc_entry_t *entry,
const char *wcpath,
const char *limit_path,
const char **walked_path,
svn_wc_adm_access_t *adm_access,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client__get_repos_mergeinfo(svn_ra_session_t *ra_session,
svn_mergeinfo_t *target_mergeinfo,
const char *rel_path,
svn_revnum_t rev,
svn_mergeinfo_inheritance_t inherit,
svn_boolean_t squelch_incapable,
apr_pool_t *pool);
svn_error_t *
svn_client__get_wc_or_repos_mergeinfo(svn_mergeinfo_t *target_mergeinfo,
const svn_wc_entry_t *entry,
svn_boolean_t *indirect,
svn_boolean_t repos_only,
svn_mergeinfo_inheritance_t inherit,
svn_ra_session_t *ra_session,
const char *target_wcpath,
svn_wc_adm_access_t *adm_access,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client__get_history_as_mergeinfo(svn_mergeinfo_t *mergeinfo_p,
const char *path_or_url,
const svn_opt_revision_t *peg_revision,
svn_revnum_t range_youngest,
svn_revnum_t range_oldest,
svn_ra_session_t *ra_session,
svn_wc_adm_access_t *adm_access,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client__mergeinfo_from_segments(svn_mergeinfo_t *mergeinfo_p,
apr_array_header_t *segments,
apr_pool_t *pool);
svn_error_t *
svn_client__parse_mergeinfo(svn_mergeinfo_t *mergeinfo,
const svn_wc_entry_t *entry,
const char *wcpath,
svn_boolean_t pristine,
svn_wc_adm_access_t *adm_access,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client__record_wc_mergeinfo(const char *wcpath,
svn_mergeinfo_t mergeinfo,
svn_wc_adm_access_t *adm_access,
apr_pool_t *pool);
svn_error_t *
svn_client__elide_mergeinfo(const char *target_wcpath,
const char *wc_elision_limit_path,
const svn_wc_entry_t *entry,
svn_wc_adm_access_t *adm_access,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client__elide_children(apr_array_header_t *children_with_mergeinfo,
const char *target_wcpath,
const svn_wc_entry_t *entry,
svn_wc_adm_access_t *adm_access,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client__elide_mergeinfo_for_tree(apr_hash_t *children_with_mergeinfo,
svn_wc_adm_access_t *adm_access,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_client__elide_mergeinfo_catalog(svn_mergeinfo_t mergeinfo_catalog,
apr_pool_t *pool);
#endif
