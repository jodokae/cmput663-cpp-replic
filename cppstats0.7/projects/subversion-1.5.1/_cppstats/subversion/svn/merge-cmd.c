#include "svn_client.h"
#include "svn_path.h"
#include "svn_error.h"
#include "svn_types.h"
#include "cl.h"
#include "svn_private_config.h"
svn_error_t *
svn_cl__merge(apr_getopt_t *os,
void *baton,
apr_pool_t *pool) {
svn_cl__opt_state_t *opt_state = ((svn_cl__cmd_baton_t *) baton)->opt_state;
svn_client_ctx_t *ctx = ((svn_cl__cmd_baton_t *) baton)->ctx;
apr_array_header_t *targets;
const char *sourcepath1 = NULL, *sourcepath2 = NULL, *targetpath = "";
svn_boolean_t two_sources_specified = TRUE;
svn_error_t *err;
svn_opt_revision_t first_range_start, first_range_end, peg_revision1,
peg_revision2;
apr_array_header_t *options, *ranges_to_merge = opt_state->revision_ranges;
SVN_ERR(svn_cl__args_to_target_array_print_reserved(&targets, os,
opt_state->targets,
pool));
if (targets->nelts < 1) {
return svn_error_create(SVN_ERR_CL_INSUFFICIENT_ARGS, 0,
_("Merge source required"));
} else {
SVN_ERR(svn_opt_parse_path(&peg_revision1, &sourcepath1,
APR_ARRAY_IDX(targets, 0, const char *),
pool));
if (targets->nelts >= 2)
SVN_ERR(svn_opt_parse_path(&peg_revision2, &sourcepath2,
APR_ARRAY_IDX(targets, 1, const char *),
pool));
}
if (targets->nelts <= 1) {
two_sources_specified = FALSE;
} else if (targets->nelts == 2) {
if (svn_path_is_url(sourcepath1) && !svn_path_is_url(sourcepath2))
two_sources_specified = FALSE;
}
if (opt_state->revision_ranges->nelts > 0) {
first_range_start = APR_ARRAY_IDX(opt_state->revision_ranges, 0,
svn_opt_revision_range_t *)->start;
first_range_end = APR_ARRAY_IDX(opt_state->revision_ranges, 0,
svn_opt_revision_range_t *)->end;
} else {
first_range_start.kind = first_range_end.kind =
svn_opt_revision_unspecified;
}
if (first_range_start.kind != svn_opt_revision_unspecified) {
if (first_range_end.kind == svn_opt_revision_unspecified)
return svn_error_create(SVN_ERR_CL_INSUFFICIENT_ARGS, 0,
_("Second revision required"));
two_sources_specified = FALSE;
}
if (! two_sources_specified) {
if (targets->nelts > 2)
return svn_error_create(SVN_ERR_CL_ARG_PARSING_ERROR, NULL,
_("Too many arguments given"));
if (targets->nelts == 0) {
peg_revision1.kind = svn_opt_revision_head;
} else {
sourcepath2 = sourcepath1;
if (peg_revision1.kind == svn_opt_revision_unspecified)
peg_revision1.kind = svn_path_is_url(sourcepath1)
? svn_opt_revision_head : svn_opt_revision_working;
if (targets->nelts == 2) {
targetpath = APR_ARRAY_IDX(targets, 1, const char *);
if (svn_path_is_url(targetpath))
return svn_error_create(SVN_ERR_CL_ARG_PARSING_ERROR, NULL,
_("Cannot specify a revision range "
"with two URLs"));
}
}
} else {
if (targets->nelts < 2)
return svn_error_create(SVN_ERR_CL_INSUFFICIENT_ARGS, NULL, NULL);
if (targets->nelts > 3)
return svn_error_create(SVN_ERR_CL_ARG_PARSING_ERROR, NULL,
_("Too many arguments given"));
first_range_start = peg_revision1;
first_range_end = peg_revision2;
if ((first_range_start.kind == svn_opt_revision_unspecified
&& ! svn_path_is_url(sourcepath1))
||
(first_range_end.kind == svn_opt_revision_unspecified
&& ! svn_path_is_url(sourcepath2)))
return svn_error_create
(SVN_ERR_CLIENT_BAD_REVISION, 0,
_("A working copy merge source needs an explicit revision"));
if (first_range_start.kind == svn_opt_revision_unspecified)
first_range_start.kind = svn_opt_revision_head;
if (first_range_end.kind == svn_opt_revision_unspecified)
first_range_end.kind = svn_opt_revision_head;
if (targets->nelts == 3)
targetpath = APR_ARRAY_IDX(targets, 2, const char *);
}
if (sourcepath1 && sourcepath2 && strcmp(targetpath, "") == 0) {
if (svn_path_is_url(sourcepath1)) {
char *sp1_basename, *sp2_basename;
sp1_basename = svn_path_basename(sourcepath1, pool);
sp2_basename = svn_path_basename(sourcepath2, pool);
if (strcmp(sp1_basename, sp2_basename) == 0) {
svn_node_kind_t kind;
const char *decoded_path = svn_path_uri_decode(sp1_basename, pool);
SVN_ERR(svn_io_check_path(decoded_path, &kind, pool));
if (kind == svn_node_file) {
targetpath = decoded_path;
}
}
} else if (strcmp(sourcepath1, sourcepath2) == 0) {
svn_node_kind_t kind;
const char *decoded_path = svn_path_uri_decode(sourcepath1, pool);
SVN_ERR(svn_io_check_path(decoded_path, &kind, pool));
if (kind == svn_node_file) {
targetpath = decoded_path;
}
}
}
if (! opt_state->quiet)
svn_cl__get_notifier(&ctx->notify_func2, &ctx->notify_baton2, FALSE,
FALSE, FALSE, pool);
if (opt_state->extensions)
options = svn_cstring_split(opt_state->extensions, " \t\n\r", TRUE, pool);
else
options = NULL;
if (! two_sources_specified) {
if (! sourcepath1)
sourcepath1 = targetpath;
if ((first_range_start.kind == svn_opt_revision_unspecified)
&& (first_range_end.kind == svn_opt_revision_unspecified)) {
svn_opt_revision_range_t *range = apr_pcalloc(pool, sizeof(*range));
ranges_to_merge = apr_array_make(pool, 1, sizeof(range));
range->start.kind = svn_opt_revision_number;
range->start.value.number = 1;
range->end = peg_revision1;
APR_ARRAY_PUSH(ranges_to_merge, svn_opt_revision_range_t *) = range;
}
if (opt_state->reintegrate) {
if (opt_state->depth != svn_depth_unknown)
return svn_error_create(SVN_ERR_CL_MUTUALLY_EXCLUSIVE_ARGS, NULL,
_("--depth cannot be used with "
"--reintegrate"));
if (opt_state->force)
return svn_error_create(SVN_ERR_CL_MUTUALLY_EXCLUSIVE_ARGS, NULL,
_("--force cannot be used with "
"--reintegrate"));
err = svn_client_merge_reintegrate(sourcepath1,
&peg_revision1,
targetpath,
opt_state->dry_run,
options, ctx, pool);
} else
err = svn_client_merge_peg3(sourcepath1,
ranges_to_merge,
&peg_revision1,
targetpath,
opt_state->depth,
opt_state->ignore_ancestry,
opt_state->force,
opt_state->record_only,
opt_state->dry_run,
options,
ctx,
pool);
} else {
err = svn_client_merge3(sourcepath1,
&first_range_start,
sourcepath2,
&first_range_end,
targetpath,
opt_state->depth,
opt_state->ignore_ancestry,
opt_state->force,
opt_state->record_only,
opt_state->dry_run,
options,
ctx,
pool);
}
if (err && (! opt_state->reintegrate))
return svn_cl__may_need_force(err);
return err;
}