#include "svn_pools.h"
#include "svn_client.h"
#include "svn_cmdline.h"
#include "svn_path.h"
#include "svn_error.h"
#include "svn_error_codes.h"
#include "svn_types.h"
#include "cl.h"
#include "svn_private_config.h"
static svn_error_t *
print_log_rev(void *baton,
svn_log_entry_t *log_entry,
apr_pool_t *pool) {
svn_cmdline_printf(pool, "r%ld\n", log_entry->revision);
return SVN_NO_ERROR;
}
svn_error_t *
svn_cl__mergeinfo(apr_getopt_t *os,
void *baton,
apr_pool_t *pool) {
svn_cl__opt_state_t *opt_state = ((svn_cl__cmd_baton_t *) baton)->opt_state;
svn_client_ctx_t *ctx = ((svn_cl__cmd_baton_t *) baton)->ctx;
apr_array_header_t *targets;
const char *source, *target;
svn_opt_revision_t src_peg_revision, tgt_peg_revision;
SVN_ERR(svn_cl__args_to_target_array_print_reserved(&targets, os,
opt_state->targets,
pool));
if (targets->nelts < 1)
return svn_error_create(SVN_ERR_CL_ARG_PARSING_ERROR, NULL,
_("Not enough arguments given"));
if (targets->nelts > 2)
return svn_error_create(SVN_ERR_CL_ARG_PARSING_ERROR, NULL,
_("Too many arguments given"));
SVN_ERR(svn_opt_parse_path(&src_peg_revision, &source,
APR_ARRAY_IDX(targets, 0, const char *), pool));
if (targets->nelts == 2) {
SVN_ERR(svn_opt_parse_path(&tgt_peg_revision, &target,
APR_ARRAY_IDX(targets, 1, const char *),
pool));
} else {
target = "";
tgt_peg_revision.kind = svn_opt_revision_unspecified;
}
if (src_peg_revision.kind == svn_opt_revision_unspecified)
src_peg_revision.kind = svn_opt_revision_head;
if (tgt_peg_revision.kind == svn_opt_revision_unspecified) {
if (svn_path_is_url(target))
tgt_peg_revision.kind = svn_opt_revision_head;
else
tgt_peg_revision.kind = svn_opt_revision_base;
}
if (opt_state->show_revs == svn_cl__show_revs_merged) {
SVN_ERR(svn_client_mergeinfo_log_merged(target, &tgt_peg_revision,
source, &src_peg_revision,
print_log_rev, NULL,
FALSE, NULL, ctx, pool));
} else if (opt_state->show_revs == svn_cl__show_revs_eligible) {
SVN_ERR(svn_client_mergeinfo_log_eligible(target, &tgt_peg_revision,
source, &src_peg_revision,
print_log_rev, NULL,
FALSE, NULL, ctx, pool));
}
return SVN_NO_ERROR;
}