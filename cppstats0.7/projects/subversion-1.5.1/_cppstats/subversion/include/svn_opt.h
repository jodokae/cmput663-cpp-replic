#if !defined(SVN_OPTS_H)
#define SVN_OPTS_H
#include <apr.h>
#include <apr_pools.h>
#include <apr_getopt.h>
#include "svn_types.h"
#include "svn_error.h"
#if defined(__cplusplus)
extern "C" {
#endif
typedef svn_error_t *(svn_opt_subcommand_t)
(apr_getopt_t *os, void *baton, apr_pool_t *pool);
#define SVN_OPT_MAX_ALIASES 3
#define SVN_OPT_MAX_OPTIONS 50
#define SVN_OPT_FIRST_LONGOPT_ID 256
typedef struct svn_opt_subcommand_desc2_t {
const char *name;
svn_opt_subcommand_t *cmd_func;
const char *aliases[SVN_OPT_MAX_ALIASES];
const char *help;
int valid_options[SVN_OPT_MAX_OPTIONS];
struct {
int optch;
const char *desc;
} desc_overrides[SVN_OPT_MAX_OPTIONS];
} svn_opt_subcommand_desc2_t;
typedef struct svn_opt_subcommand_desc_t {
const char *name;
svn_opt_subcommand_t *cmd_func;
const char *aliases[SVN_OPT_MAX_ALIASES];
const char *help;
int valid_options[SVN_OPT_MAX_OPTIONS];
} svn_opt_subcommand_desc_t;
const svn_opt_subcommand_desc2_t *
svn_opt_get_canonical_subcommand2(const svn_opt_subcommand_desc2_t *table,
const char *cmd_name);
const svn_opt_subcommand_desc_t *
svn_opt_get_canonical_subcommand(const svn_opt_subcommand_desc_t *table,
const char *cmd_name);
const apr_getopt_option_t *
svn_opt_get_option_from_code2(int code,
const apr_getopt_option_t *option_table,
const svn_opt_subcommand_desc2_t *command,
apr_pool_t *pool);
const apr_getopt_option_t *
svn_opt_get_option_from_code(int code,
const apr_getopt_option_t *option_table);
svn_boolean_t
svn_opt_subcommand_takes_option3(const svn_opt_subcommand_desc2_t *command,
int option_code,
const int *global_options);
svn_boolean_t
svn_opt_subcommand_takes_option2(const svn_opt_subcommand_desc2_t *command,
int option_code);
svn_boolean_t
svn_opt_subcommand_takes_option(const svn_opt_subcommand_desc_t *command,
int option_code);
void
svn_opt_print_generic_help2(const char *header,
const svn_opt_subcommand_desc2_t *cmd_table,
const apr_getopt_option_t *opt_table,
const char *footer,
apr_pool_t *pool,
FILE *stream);
void
svn_opt_print_generic_help(const char *header,
const svn_opt_subcommand_desc_t *cmd_table,
const apr_getopt_option_t *opt_table,
const char *footer,
apr_pool_t *pool,
FILE *stream);
void
svn_opt_format_option(const char **string,
const apr_getopt_option_t *opt,
svn_boolean_t doc,
apr_pool_t *pool);
void
svn_opt_subcommand_help3(const char *subcommand,
const svn_opt_subcommand_desc2_t *table,
const apr_getopt_option_t *options_table,
const int *global_options,
apr_pool_t *pool);
void
svn_opt_subcommand_help2(const char *subcommand,
const svn_opt_subcommand_desc2_t *table,
const apr_getopt_option_t *options_table,
apr_pool_t *pool);
void
svn_opt_subcommand_help(const char *subcommand,
const svn_opt_subcommand_desc_t *table,
const apr_getopt_option_t *options_table,
apr_pool_t *pool);
enum svn_opt_revision_kind {
svn_opt_revision_unspecified,
svn_opt_revision_number,
svn_opt_revision_date,
svn_opt_revision_committed,
svn_opt_revision_previous,
svn_opt_revision_base,
svn_opt_revision_working,
svn_opt_revision_head
};
typedef union svn_opt_revision_value_t {
svn_revnum_t number;
apr_time_t date;
} svn_opt_revision_value_t;
typedef struct svn_opt_revision_t {
enum svn_opt_revision_kind kind;
svn_opt_revision_value_t value;
} svn_opt_revision_t;
typedef struct svn_opt_revision_range_t {
svn_opt_revision_t start;
svn_opt_revision_t end;
} svn_opt_revision_range_t;
int svn_opt_parse_revision(svn_opt_revision_t *start_revision,
svn_opt_revision_t *end_revision,
const char *arg,
apr_pool_t *pool);
int
svn_opt_parse_revision_to_range(apr_array_header_t *opt_ranges,
const char *arg,
apr_pool_t *pool);
svn_error_t *
svn_opt_resolve_revisions(svn_opt_revision_t *peg_rev,
svn_opt_revision_t *op_rev,
svn_boolean_t is_url,
svn_boolean_t notice_local_mods,
apr_pool_t *pool);
svn_error_t *
svn_opt_args_to_target_array3(apr_array_header_t **targets_p,
apr_getopt_t *os,
apr_array_header_t *known_targets,
apr_pool_t *pool);
svn_error_t *
svn_opt_args_to_target_array2(apr_array_header_t **targets_p,
apr_getopt_t *os,
apr_array_header_t *known_targets,
apr_pool_t *pool);
svn_error_t *
svn_opt_args_to_target_array(apr_array_header_t **targets_p,
apr_getopt_t *os,
apr_array_header_t *known_targets,
svn_opt_revision_t *start_revision,
svn_opt_revision_t *end_revision,
svn_boolean_t extract_revisions,
apr_pool_t *pool);
void svn_opt_push_implicit_dot_target(apr_array_header_t *targets,
apr_pool_t *pool);
svn_error_t *
svn_opt_parse_num_args(apr_array_header_t **args_p,
apr_getopt_t *os,
int num_args,
apr_pool_t *pool);
svn_error_t *
svn_opt_parse_all_args(apr_array_header_t **args_p,
apr_getopt_t *os,
apr_pool_t *pool);
svn_error_t *
svn_opt_parse_path(svn_opt_revision_t *rev,
const char **truepath,
const char *path,
apr_pool_t *pool);
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
apr_pool_t *pool);
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
apr_pool_t *pool);
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
apr_pool_t *pool);
#if defined(__cplusplus)
}
#endif
#endif