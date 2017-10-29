#define APR_WANT_STRFUNC
#include <apr_want.h>
#include <stdio.h>
#include <assert.h>
#include <apr_pools.h>
#include <apr_general.h>
#include <apr_lib.h>
#include <apr_file_info.h>
#include "svn_cmdline.h"
#include "svn_version.h"
#include "svn_types.h"
#include "svn_opt.h"
#include "svn_error.h"
#include "svn_path.h"
#include "svn_utf.h"
#include "svn_time.h"
#include "svn_private_config.h"
const svn_opt_subcommand_desc2_t *
svn_opt_get_canonical_subcommand2(const svn_opt_subcommand_desc2_t *table,
const char *cmd_name) {
int i = 0;
if (cmd_name == NULL)
return NULL;
while (table[i].name) {
int j;
if (strcmp(cmd_name, table[i].name) == 0)
return table + i;
for (j = 0; (j < SVN_OPT_MAX_ALIASES) && table[i].aliases[j]; j++)
if (strcmp(cmd_name, table[i].aliases[j]) == 0)
return table + i;
i++;
}
return NULL;
}
const svn_opt_subcommand_desc_t *
svn_opt_get_canonical_subcommand(const svn_opt_subcommand_desc_t *table,
const char *cmd_name) {
int i = 0;
if (cmd_name == NULL)
return NULL;
while (table[i].name) {
int j;
if (strcmp(cmd_name, table[i].name) == 0)
return table + i;
for (j = 0; (j < SVN_OPT_MAX_ALIASES) && table[i].aliases[j]; j++)
if (strcmp(cmd_name, table[i].aliases[j]) == 0)
return table + i;
i++;
}
return NULL;
}
const apr_getopt_option_t *
svn_opt_get_option_from_code2(int code,
const apr_getopt_option_t *option_table,
const svn_opt_subcommand_desc2_t *command,
apr_pool_t *pool) {
apr_size_t i;
for (i = 0; option_table[i].optch; i++)
if (option_table[i].optch == code) {
int j;
if (command)
for (j = 0; ((j < SVN_OPT_MAX_OPTIONS) &&
command->desc_overrides[j].optch); j++)
if (command->desc_overrides[j].optch == code) {
apr_getopt_option_t *tmpopt =
apr_palloc(pool, sizeof(*tmpopt));
*tmpopt = option_table[i];
tmpopt->description = command->desc_overrides[j].desc;
return tmpopt;
}
return &(option_table[i]);
}
return NULL;
}
const apr_getopt_option_t *
svn_opt_get_option_from_code(int code,
const apr_getopt_option_t *option_table) {
apr_size_t i;
for (i = 0; option_table[i].optch; i++)
if (option_table[i].optch == code)
return &(option_table[i]);
return NULL;
}
svn_boolean_t
svn_opt_subcommand_takes_option3(const svn_opt_subcommand_desc2_t *command,
int option_code,
const int *global_options) {
apr_size_t i;
for (i = 0; i < SVN_OPT_MAX_OPTIONS; i++)
if (command->valid_options[i] == option_code)
return TRUE;
if (global_options)
for (i = 0; global_options[i]; i++)
if (global_options[i] == option_code)
return TRUE;
return FALSE;
}
svn_boolean_t
svn_opt_subcommand_takes_option2(const svn_opt_subcommand_desc2_t *command,
int option_code) {
return svn_opt_subcommand_takes_option3(command,
option_code,
NULL);
}
svn_boolean_t
svn_opt_subcommand_takes_option(const svn_opt_subcommand_desc_t *command,
int option_code) {
apr_size_t i;
for (i = 0; i < SVN_OPT_MAX_OPTIONS; i++)
if (command->valid_options[i] == option_code)
return TRUE;
return FALSE;
}
static svn_error_t *
print_command_info2(const svn_opt_subcommand_desc2_t *cmd,
const apr_getopt_option_t *options_table,
const int *global_options,
svn_boolean_t help,
apr_pool_t *pool,
FILE *stream) {
svn_boolean_t first_time;
apr_size_t i;
SVN_ERR(svn_cmdline_fputs(cmd->name, stream, pool));
first_time = TRUE;
for (i = 0; i < SVN_OPT_MAX_ALIASES; i++) {
if (cmd->aliases[i] == NULL)
break;
if (first_time) {
SVN_ERR(svn_cmdline_fputs(" (", stream, pool));
first_time = FALSE;
} else
SVN_ERR(svn_cmdline_fputs(", ", stream, pool));
SVN_ERR(svn_cmdline_fputs(cmd->aliases[i], stream, pool));
}
if (! first_time)
SVN_ERR(svn_cmdline_fputs(")", stream, pool));
if (help) {
const apr_getopt_option_t *option;
svn_boolean_t have_options = FALSE;
SVN_ERR(svn_cmdline_fprintf(stream, pool, ": %s", _(cmd->help)));
for (i = 0; i < SVN_OPT_MAX_OPTIONS; i++) {
if (cmd->valid_options[i]) {
if (have_options == FALSE) {
SVN_ERR(svn_cmdline_fputs(_("\nValid options:\n"),
stream, pool));
have_options = TRUE;
}
option =
svn_opt_get_option_from_code2(cmd->valid_options[i],
options_table,
cmd, pool);
if (option && option->description) {
const char *optstr;
svn_opt_format_option(&optstr, option, TRUE, pool);
SVN_ERR(svn_cmdline_fprintf(stream, pool, " %s\n",
optstr));
}
}
}
if (global_options && *global_options) {
SVN_ERR(svn_cmdline_fputs(_("\nGlobal options:\n"),
stream, pool));
have_options = TRUE;
for (i = 0; global_options[i]; i++) {
option =
svn_opt_get_option_from_code2(global_options[i],
options_table,
cmd, pool);
if (option && option->description) {
const char *optstr;
svn_opt_format_option(&optstr, option, TRUE, pool);
SVN_ERR(svn_cmdline_fprintf(stream, pool, " %s\n",
optstr));
}
}
}
if (have_options)
SVN_ERR(svn_cmdline_fprintf(stream, pool, "\n"));
}
return SVN_NO_ERROR;
}
static svn_error_t *
print_command_info(const svn_opt_subcommand_desc_t *cmd,
const apr_getopt_option_t *options_table,
svn_boolean_t help,
apr_pool_t *pool,
FILE *stream) {
svn_boolean_t first_time;
apr_size_t i;
SVN_ERR(svn_cmdline_fputs(cmd->name, stream, pool));
first_time = TRUE;
for (i = 0; i < SVN_OPT_MAX_ALIASES; i++) {
if (cmd->aliases[i] == NULL)
break;
if (first_time) {
SVN_ERR(svn_cmdline_fputs(" (", stream, pool));
first_time = FALSE;
} else
SVN_ERR(svn_cmdline_fputs(", ", stream, pool));
SVN_ERR(svn_cmdline_fputs(cmd->aliases[i], stream, pool));
}
if (! first_time)
SVN_ERR(svn_cmdline_fputs(")", stream, pool));
if (help) {
const apr_getopt_option_t *option;
svn_boolean_t have_options = FALSE;
SVN_ERR(svn_cmdline_fprintf(stream, pool, ": %s", _(cmd->help)));
for (i = 0; i < SVN_OPT_MAX_OPTIONS; i++) {
if (cmd->valid_options[i]) {
if (have_options == FALSE) {
SVN_ERR(svn_cmdline_fputs(_("\nValid options:\n"),
stream, pool));
have_options = TRUE;
}
option =
svn_opt_get_option_from_code(cmd->valid_options[i],
options_table);
if (option && option->description) {
const char *optstr;
svn_opt_format_option(&optstr, option, TRUE, pool);
SVN_ERR(svn_cmdline_fprintf(stream, pool, " %s\n",
optstr));
}
}
}
if (have_options)
SVN_ERR(svn_cmdline_fprintf(stream, pool, "\n"));
}
return SVN_NO_ERROR;
}
void
svn_opt_print_generic_help2(const char *header,
const svn_opt_subcommand_desc2_t *cmd_table,
const apr_getopt_option_t *opt_table,
const char *footer,
apr_pool_t *pool, FILE *stream) {
int i = 0;
svn_error_t *err;
if (header)
if ((err = svn_cmdline_fputs(header, stream, pool)))
goto print_error;
while (cmd_table[i].name) {
if ((err = svn_cmdline_fputs(" ", stream, pool))
|| (err = print_command_info2(cmd_table + i, opt_table,
NULL, FALSE,
pool, stream))
|| (err = svn_cmdline_fputs("\n", stream, pool)))
goto print_error;
i++;
}
if ((err = svn_cmdline_fputs("\n", stream, pool)))
goto print_error;
if (footer)
if ((err = svn_cmdline_fputs(footer, stream, pool)))
goto print_error;
return;
print_error:
svn_handle_error2(err, stderr, FALSE, "svn: ");
svn_error_clear(err);
}
void
svn_opt_print_generic_help(const char *header,
const svn_opt_subcommand_desc_t *cmd_table,
const apr_getopt_option_t *opt_table,
const char *footer,
apr_pool_t *pool, FILE *stream) {
int i = 0;
svn_error_t *err;
if (header)
if ((err = svn_cmdline_fputs(header, stream, pool)))
goto print_error;
while (cmd_table[i].name) {
if ((err = svn_cmdline_fputs(" ", stream, pool))
|| (err = print_command_info(cmd_table + i, opt_table, FALSE,
pool, stream))
|| (err = svn_cmdline_fputs("\n", stream, pool)))
goto print_error;
i++;
}
if ((err = svn_cmdline_fputs("\n", stream, pool)))
goto print_error;
if (footer)
if ((err = svn_cmdline_fputs(footer, stream, pool)))
goto print_error;
return;
print_error:
svn_handle_error2(err, stderr, FALSE, "svn: ");
svn_error_clear(err);
}
void
svn_opt_format_option(const char **string,
const apr_getopt_option_t *opt,
svn_boolean_t doc,
apr_pool_t *pool) {
char *opts;
if (opt == NULL) {
*string = "?";
return;
}
if (opt->optch <= 255)
opts = apr_psprintf(pool, "-%c [--%s]", opt->optch, opt->name);
else
opts = apr_psprintf(pool, "--%s", opt->name);
if (opt->has_arg)
opts = apr_pstrcat(pool, opts, _(" ARG"), NULL);
if (doc)
opts = apr_psprintf(pool, "%-24s : %s", opts, _(opt->description));
*string = opts;
}
void
svn_opt_subcommand_help3(const char *subcommand,
const svn_opt_subcommand_desc2_t *table,
const apr_getopt_option_t *options_table,
const int *global_options,
apr_pool_t *pool) {
const svn_opt_subcommand_desc2_t *cmd =
svn_opt_get_canonical_subcommand2(table, subcommand);
svn_error_t *err;
if (cmd)
err = print_command_info2(cmd, options_table, global_options,
TRUE, pool, stdout);
else
err = svn_cmdline_fprintf(stderr, pool,
_("\"%s\": unknown command.\n\n"), subcommand);
if (err) {
svn_handle_error2(err, stderr, FALSE, "svn: ");
svn_error_clear(err);
}
}
void
svn_opt_subcommand_help2(const char *subcommand,
const svn_opt_subcommand_desc2_t *table,
const apr_getopt_option_t *options_table,
apr_pool_t *pool) {
svn_opt_subcommand_help3(subcommand, table, options_table,
NULL, pool);
}
void
svn_opt_subcommand_help(const char *subcommand,
const svn_opt_subcommand_desc_t *table,
const apr_getopt_option_t *options_table,
apr_pool_t *pool) {
const svn_opt_subcommand_desc_t *cmd =
svn_opt_get_canonical_subcommand(table, subcommand);
svn_error_t *err;
if (cmd)
err = print_command_info(cmd, options_table, TRUE, pool, stdout);
else
err = svn_cmdline_fprintf(stderr, pool,
_("\"%s\": unknown command.\n\n"), subcommand);
if (err) {
svn_handle_error2(err, stderr, FALSE, "svn: ");
svn_error_clear(err);
}
}
static int
revision_from_word(svn_opt_revision_t *revision, const char *word) {
if (svn_cstring_casecmp(word, "head") == 0) {
revision->kind = svn_opt_revision_head;
} else if (svn_cstring_casecmp(word, "prev") == 0) {
revision->kind = svn_opt_revision_previous;
} else if (svn_cstring_casecmp(word, "base") == 0) {
revision->kind = svn_opt_revision_base;
} else if (svn_cstring_casecmp(word, "committed") == 0) {
revision->kind = svn_opt_revision_committed;
} else
return -1;
return 0;
}
static char *parse_one_rev(svn_opt_revision_t *revision, char *str,
apr_pool_t *pool) {
char *end, save;
while (*str == 'r')
str++;
if (*str == '{') {
svn_boolean_t matched;
apr_time_t tm;
svn_error_t *err;
str++;
end = strchr(str, '}');
if (!end)
return NULL;
*end = '\0';
err = svn_parse_date(&matched, &tm, str, apr_time_now(), pool);
if (err) {
svn_error_clear(err);
return NULL;
}
if (!matched)
return NULL;
revision->kind = svn_opt_revision_date;
revision->value.date = tm;
return end + 1;
} else if (apr_isdigit(*str)) {
end = str + 1;
while (apr_isdigit(*end))
end++;
save = *end;
*end = '\0';
revision->kind = svn_opt_revision_number;
revision->value.number = SVN_STR_TO_REV(str);
*end = save;
return end;
} else if (apr_isalpha(*str)) {
end = str + 1;
while (apr_isalpha(*end))
end++;
save = *end;
*end = '\0';
if (revision_from_word(revision, str) != 0)
return NULL;
*end = save;
return end;
} else
return NULL;
}
int
svn_opt_parse_revision(svn_opt_revision_t *start_revision,
svn_opt_revision_t *end_revision,
const char *arg,
apr_pool_t *pool) {
char *left_rev, *right_rev, *end;
left_rev = apr_pstrdup(pool, arg);
right_rev = parse_one_rev(start_revision, left_rev, pool);
if (right_rev && *right_rev == ':') {
right_rev++;
end = parse_one_rev(end_revision, right_rev, pool);
if (!end || *end != '\0')
return -1;
} else if (!right_rev || *right_rev != '\0')
return -1;
return 0;
}
int
svn_opt_parse_revision_to_range(apr_array_header_t *opt_ranges,
const char *arg,
apr_pool_t *pool) {
svn_opt_revision_range_t *range = apr_palloc(pool, sizeof(*range));
range->start.kind = svn_opt_revision_unspecified;
range->end.kind = svn_opt_revision_unspecified;
if (svn_opt_parse_revision(&(range->start), &(range->end),
arg, pool) == -1)
return -1;
APR_ARRAY_PUSH(opt_ranges, svn_opt_revision_range_t *) = range;
return 0;
}
svn_error_t *
svn_opt_resolve_revisions(svn_opt_revision_t *peg_rev,
svn_opt_revision_t *op_rev,
svn_boolean_t is_url,
svn_boolean_t notice_local_mods,
apr_pool_t *pool) {
if (peg_rev->kind == svn_opt_revision_unspecified) {
if (is_url) {
peg_rev->kind = svn_opt_revision_head;
} else {
if (notice_local_mods)
peg_rev->kind = svn_opt_revision_working;
else
peg_rev->kind = svn_opt_revision_base;
}
}
if (op_rev->kind == svn_opt_revision_unspecified)
*op_rev = *peg_rev;
return SVN_NO_ERROR;
}
#define DEFAULT_ARRAY_SIZE 5
static void
array_push_str(apr_array_header_t *array,
const char *str,
apr_pool_t *pool) {
APR_ARRAY_PUSH(array, const char *) = apr_pstrdup(pool, str);
}
void
svn_opt_push_implicit_dot_target(apr_array_header_t *targets,
apr_pool_t *pool) {
if (targets->nelts == 0)
array_push_str(targets, "", pool);
assert(targets->nelts);
}
svn_error_t *
svn_opt_parse_num_args(apr_array_header_t **args_p,
apr_getopt_t *os,
int num_args,
apr_pool_t *pool) {
int i;
apr_array_header_t *args
= apr_array_make(pool, DEFAULT_ARRAY_SIZE, sizeof(const char *));
for (i = 0; i < num_args; i++) {
if (os->ind >= os->argc) {
return svn_error_create(SVN_ERR_CL_INSUFFICIENT_ARGS, 0, NULL);
}
array_push_str(args, os->argv[os->ind++], pool);
}
*args_p = args;
return SVN_NO_ERROR;
}
svn_error_t *
svn_opt_parse_all_args(apr_array_header_t **args_p,
apr_getopt_t *os,
apr_pool_t *pool) {
apr_array_header_t *args
= apr_array_make(pool, DEFAULT_ARRAY_SIZE, sizeof(const char *));
if (os->ind > os->argc) {
return svn_error_create(SVN_ERR_CL_ARG_PARSING_ERROR, 0, NULL);
}
while (os->ind < os->argc) {
array_push_str(args, os->argv[os->ind++], pool);
}
*args_p = args;
return SVN_NO_ERROR;
}
svn_error_t *
svn_opt_parse_path(svn_opt_revision_t *rev,
const char **truepath,
const char *path ,
apr_pool_t *pool) {
int i;
for (i = (strlen(path) - 1); i >= 0; i--) {
if (path[i] == '/')
break;
if (path[i] == '@') {
int ret;
svn_opt_revision_t start_revision, end_revision;
end_revision.kind = svn_opt_revision_unspecified;
if (path[i + 1] == '\0') {
ret = 0;
start_revision.kind = svn_opt_revision_unspecified;
} else {
const char *rev_str = path + i + 1;
if (svn_path_is_url(path)) {
int rev_len = strlen(rev_str);
if (rev_len > 6
&& rev_str[0] == '%'
&& rev_str[1] == '7'
&& (rev_str[2] == 'B'
|| rev_str[2] == 'b')
&& rev_str[rev_len-3] == '%'
&& rev_str[rev_len-2] == '7'
&& (rev_str[rev_len-1] == 'D'
|| rev_str[rev_len-1] == 'd')) {
rev_str = svn_path_uri_decode(rev_str, pool);
}
}
ret = svn_opt_parse_revision(&start_revision,
&end_revision,
rev_str, pool);
}
if (ret || end_revision.kind != svn_opt_revision_unspecified)
return svn_error_createf(SVN_ERR_CL_ARG_PARSING_ERROR, NULL,
_("Syntax error parsing revision '%s'"),
path + i + 1);
*truepath = apr_pstrmemdup(pool, path, i);
rev->kind = start_revision.kind;
rev->value = start_revision.value;
return SVN_NO_ERROR;
}
}
*truepath = path;
rev->kind = svn_opt_revision_unspecified;
return SVN_NO_ERROR;
}
svn_error_t *
svn_opt_args_to_target_array2(apr_array_header_t **targets_p,
apr_getopt_t *os,
apr_array_header_t *known_targets,
apr_pool_t *pool) {
svn_error_t *err = svn_opt_args_to_target_array3(targets_p, os,
known_targets, pool);
if (err && err->apr_err == SVN_ERR_RESERVED_FILENAME_SPECIFIED) {
svn_error_clear(err);
return SVN_NO_ERROR;
}
return err;
}
svn_error_t *
svn_opt_args_to_target_array3(apr_array_header_t **targets_p,
apr_getopt_t *os,
apr_array_header_t *known_targets,
apr_pool_t *pool) {
int i;
svn_error_t *err = SVN_NO_ERROR;
apr_array_header_t *input_targets =
apr_array_make(pool, DEFAULT_ARRAY_SIZE, sizeof(const char *));
apr_array_header_t *output_targets =
apr_array_make(pool, DEFAULT_ARRAY_SIZE, sizeof(const char *));
for (; os->ind < os->argc; os->ind++) {
const char *raw_target = os->argv[os->ind];
SVN_ERR(svn_utf_cstring_to_utf8
((const char **) apr_array_push(input_targets),
raw_target, pool));
}
if (known_targets) {
for (i = 0; i < known_targets->nelts; i++) {
const char *utf8_target = APR_ARRAY_IDX(known_targets,
i, const char *);
APR_ARRAY_PUSH(input_targets, const char *) = utf8_target;
}
}
for (i = 0; i < input_targets->nelts; i++) {
const char *utf8_target = APR_ARRAY_IDX(input_targets, i, const char *);
const char *peg_start = NULL;
const char *target;
int j;
for (j = (strlen(utf8_target) - 1); j >= 0; --j) {
if (utf8_target[j] == '/')
break;
if (utf8_target[j] == '@') {
peg_start = utf8_target + j;
break;
}
}
if (peg_start)
utf8_target = apr_pstrmemdup(pool,
utf8_target,
peg_start - utf8_target);
if (svn_path_is_url(utf8_target)) {
target = svn_path_uri_from_iri(utf8_target, pool);
target = svn_path_uri_autoescape(target, pool);
if (! svn_path_is_uri_safe(target))
return svn_error_createf(SVN_ERR_BAD_URL, 0,
_("URL '%s' is not properly URI-encoded"),
utf8_target);
if (svn_path_is_backpath_present(target))
return svn_error_createf(SVN_ERR_BAD_URL, 0,
_("URL '%s' contains a '..' element"),
utf8_target);
target = svn_path_canonicalize(target, pool);
} else {
const char *apr_target;
const char *base_name;
char *truenamed_target;
apr_status_t apr_err;
SVN_ERR(svn_path_cstring_from_utf8(&apr_target, utf8_target,
pool));
apr_err = apr_filepath_merge(&truenamed_target, "", apr_target,
APR_FILEPATH_TRUENAME, pool);
if (!apr_err)
apr_target = truenamed_target;
else if (APR_STATUS_IS_ENOENT(apr_err))
;
else
return svn_error_createf(apr_err, NULL,
_("Error resolving case of '%s'"),
svn_path_local_style(utf8_target,
pool));
SVN_ERR(svn_path_cstring_to_utf8(&target, apr_target, pool));
target = svn_path_canonicalize(target, pool);
base_name = svn_path_basename(target, pool);
if (0 == strcmp(base_name, ".svn")
|| 0 == strcmp(base_name, "_svn")) {
err = svn_error_createf(SVN_ERR_RESERVED_FILENAME_SPECIFIED,
err, _("'%s' ends in a reserved name"),
target);
continue;
}
}
if (peg_start)
target = apr_pstrcat(pool, target, peg_start, NULL);
APR_ARRAY_PUSH(output_targets, const char *) = target;
}
*targets_p = output_targets;
return err;
}
svn_error_t *
svn_opt_args_to_target_array(apr_array_header_t **targets_p,
apr_getopt_t *os,
apr_array_header_t *known_targets,
svn_opt_revision_t *start_revision,
svn_opt_revision_t *end_revision,
svn_boolean_t extract_revisions,
apr_pool_t *pool) {
apr_array_header_t *output_targets;
SVN_ERR(svn_opt_args_to_target_array2(&output_targets, os,
known_targets, pool));
if (extract_revisions) {
svn_opt_revision_t temprev;
const char *path;
if (output_targets->nelts > 0) {
path = APR_ARRAY_IDX(output_targets, 0, const char *);
SVN_ERR(svn_opt_parse_path(&temprev, &path, path, pool));
if (temprev.kind != svn_opt_revision_unspecified) {
APR_ARRAY_IDX(output_targets, 0, const char *) = path;
start_revision->kind = temprev.kind;
start_revision->value = temprev.value;
}
}
if (output_targets->nelts > 1) {
path = APR_ARRAY_IDX(output_targets, 1, const char *);
SVN_ERR(svn_opt_parse_path(&temprev, &path, path, pool));
if (temprev.kind != svn_opt_revision_unspecified) {
APR_ARRAY_IDX(output_targets, 1, const char *) = path;
end_revision->kind = temprev.kind;
end_revision->value = temprev.value;
}
}
}
*targets_p = output_targets;
return SVN_NO_ERROR;
}
static svn_error_t *
print_version_info(const char *pgm_name,
const char *footer,
svn_boolean_t quiet,
apr_pool_t *pool) {
if (quiet) {
SVN_ERR(svn_cmdline_printf(pool, "%s\n", SVN_VER_NUMBER));
return SVN_NO_ERROR;
}
SVN_ERR(svn_cmdline_printf(pool, _("%s, version %s\n"
" compiled %s, %s\n\n"), pgm_name,
SVN_VERSION, __DATE__, __TIME__));
SVN_ERR(svn_cmdline_fputs(_("Copyright (C) 2000-2008 CollabNet.\n"
"Subversion is open source software, see"
" http://subversion.tigris.org/\n"
"This product includes software developed by "
"CollabNet (http://www.Collab.Net/).\n\n"),
stdout, pool));
if (footer) {
SVN_ERR(svn_cmdline_printf(pool, "%s\n", footer));
}
return SVN_NO_ERROR;
}
svn_error_t *
svn_opt_print_help3(apr_getopt_t *os,
const char *pgm_name,
svn_boolean_t print_version,
svn_boolean_t quiet,
const char *version_footer,
const char *header,
const svn_opt_subcommand_desc2_t *cmd_table,
const apr_getopt_option_t *option_table,
const int *global_options,
const char *footer,
apr_pool_t *pool) {
apr_array_header_t *targets = NULL;
int i;
if (os)
SVN_ERR(svn_opt_parse_all_args(&targets, os, pool));
if (os && targets->nelts)
for (i = 0; i < targets->nelts; i++) {
svn_opt_subcommand_help3(APR_ARRAY_IDX(targets, i, const char *),
cmd_table, option_table,
global_options, pool);
}
else if (print_version)
SVN_ERR(print_version_info(pgm_name, version_footer, quiet, pool));
else if (os && !targets->nelts)
svn_opt_print_generic_help2(header,
cmd_table,
option_table,
footer,
pool,
stdout);
else
SVN_ERR(svn_cmdline_fprintf(stderr, pool,
_("Type '%s help' for usage.\n"), pgm_name));
return SVN_NO_ERROR;
}
svn_error_t *
svn_opt_print_help2(apr_getopt_t *os,
const char *pgm_name,
svn_boolean_t print_version,
svn_boolean_t quiet,
const char *version_footer,
const char *header,
const svn_opt_subcommand_desc2_t *cmd_table,
const apr_getopt_option_t *option_table,
const char *footer,
apr_pool_t *pool) {
return svn_opt_print_help3(os,
pgm_name,
print_version,
quiet,
version_footer,
header,
cmd_table,
option_table,
NULL,
footer,
pool);
}
svn_error_t *
svn_opt_print_help(apr_getopt_t *os,
const char *pgm_name,
svn_boolean_t print_version,
svn_boolean_t quiet,
const char *version_footer,
const char *header,
const svn_opt_subcommand_desc_t *cmd_table,
const apr_getopt_option_t *option_table,
const char *footer,
apr_pool_t *pool) {
apr_array_header_t *targets = NULL;
int i;
if (os)
SVN_ERR(svn_opt_parse_all_args(&targets, os, pool));
if (os && targets->nelts)
for (i = 0; i < targets->nelts; i++) {
svn_opt_subcommand_help(APR_ARRAY_IDX(targets, i, const char *),
cmd_table, option_table, pool);
}
else if (print_version)
SVN_ERR(print_version_info(pgm_name, version_footer, quiet, pool));
else if (os && !targets->nelts)
svn_opt_print_generic_help(header,
cmd_table,
option_table,
footer,
pool,
stdout);
else
SVN_ERR(svn_cmdline_fprintf(stderr, pool,
_("Type '%s help' for usage.\n"), pgm_name));
return SVN_NO_ERROR;
}