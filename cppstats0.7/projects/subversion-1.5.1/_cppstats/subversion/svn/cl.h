#if !defined(SVN_CL_H)
#define SVN_CL_H
#include <apr_tables.h>
#include <apr_getopt.h>
#include "svn_wc.h"
#include "svn_client.h"
#include "svn_string.h"
#include "svn_opt.h"
#include "svn_auth.h"
#include "svn_cmdline.h"
#if defined(__cplusplus)
extern "C" {
#endif
typedef enum {
svn_cl__accept_invalid = -2,
svn_cl__accept_unspecified = -1,
svn_cl__accept_postpone,
svn_cl__accept_base,
svn_cl__accept_working,
svn_cl__accept_mine_conflict,
svn_cl__accept_theirs_conflict,
svn_cl__accept_mine_full,
svn_cl__accept_theirs_full,
svn_cl__accept_edit,
svn_cl__accept_launch,
} svn_cl__accept_t;
#define SVN_CL__ACCEPT_POSTPONE "postpone"
#define SVN_CL__ACCEPT_BASE "base"
#define SVN_CL__ACCEPT_WORKING "working"
#define SVN_CL__ACCEPT_MINE_CONFLICT "mine-conflict"
#define SVN_CL__ACCEPT_THEIRS_CONFLICT "theirs-conflict"
#define SVN_CL__ACCEPT_MINE_FULL "mine-full"
#define SVN_CL__ACCEPT_THEIRS_FULL "theirs-full"
#define SVN_CL__ACCEPT_EDIT "edit"
#define SVN_CL__ACCEPT_LAUNCH "launch"
svn_cl__accept_t
svn_cl__accept_from_word(const char *word);
typedef enum {
svn_cl__show_revs_invalid = -1,
svn_cl__show_revs_merged,
svn_cl__show_revs_eligible
} svn_cl__show_revs_t;
#define SVN_CL__SHOW_REVS_MERGED "merged"
#define SVN_CL__SHOW_REVS_ELIGIBLE "eligible"
svn_cl__show_revs_t
svn_cl__show_revs_from_word(const char *word);
typedef struct svn_cl__opt_state_t {
apr_array_header_t *revision_ranges;
svn_opt_revision_t start_revision;
svn_opt_revision_t end_revision;
svn_boolean_t used_change_arg;
int limit;
svn_depth_t depth;
svn_boolean_t no_unlock;
const char *message;
const char *ancestor_path;
svn_boolean_t force;
svn_boolean_t force_log;
svn_boolean_t incremental;
svn_boolean_t quiet;
svn_boolean_t non_interactive;
svn_boolean_t version;
svn_boolean_t verbose;
svn_boolean_t update;
svn_boolean_t strict;
svn_stringbuf_t *filedata;
const char *encoding;
svn_boolean_t help;
const char *auth_username;
const char *auth_password;
const char *extensions;
apr_array_header_t *targets;
svn_boolean_t xml;
svn_boolean_t no_ignore;
svn_boolean_t no_auth_cache;
svn_boolean_t no_diff_deleted;
svn_boolean_t notice_ancestry;
svn_boolean_t ignore_ancestry;
svn_boolean_t ignore_externals;
svn_boolean_t stop_on_copy;
svn_boolean_t dry_run;
svn_boolean_t revprop;
const char *diff_cmd;
const char *merge_cmd;
const char *editor_cmd;
svn_boolean_t record_only;
const char *old_target;
const char *new_target;
svn_boolean_t relocate;
const char *config_dir;
svn_boolean_t autoprops;
svn_boolean_t no_autoprops;
const char *native_eol;
svn_boolean_t summarize;
svn_boolean_t remove;
apr_array_header_t *changelists;
const char *changelist;
svn_boolean_t keep_changelists;
svn_boolean_t keep_local;
svn_boolean_t all_revprops;
apr_hash_t *revprop_table;
svn_boolean_t parents;
svn_boolean_t use_merge_history;
svn_cl__accept_t accept_which;
svn_cl__show_revs_t show_revs;
svn_depth_t set_depth;
svn_boolean_t reintegrate;
} svn_cl__opt_state_t;
typedef struct {
svn_cl__opt_state_t *opt_state;
svn_client_ctx_t *ctx;
} svn_cl__cmd_baton_t;
svn_opt_subcommand_t
svn_cl__add,
svn_cl__blame,
svn_cl__cat,
svn_cl__changelist,
svn_cl__checkout,
svn_cl__cleanup,
svn_cl__commit,
svn_cl__copy,
svn_cl__delete,
svn_cl__diff,
svn_cl__export,
svn_cl__help,
svn_cl__import,
svn_cl__info,
svn_cl__lock,
svn_cl__log,
svn_cl__list,
svn_cl__merge,
svn_cl__mergeinfo,
svn_cl__mkdir,
svn_cl__move,
svn_cl__propdel,
svn_cl__propedit,
svn_cl__propget,
svn_cl__proplist,
svn_cl__propset,
svn_cl__revert,
svn_cl__resolve,
svn_cl__resolved,
svn_cl__status,
svn_cl__switch,
svn_cl__unlock,
svn_cl__update;
extern const svn_opt_subcommand_desc2_t svn_cl__cmd_table[];
extern const int svn_cl__global_options[];
extern const apr_getopt_option_t svn_cl__options[];
svn_error_t *
svn_cl__try(svn_error_t *err,
svn_boolean_t *success,
svn_boolean_t quiet,
...);
svn_error_t *svn_cl__check_cancel(void *baton);
typedef struct {
svn_cl__accept_t accept_which;
apr_hash_t *config;
const char *editor_cmd;
svn_boolean_t external_failed;
svn_cmdline_prompt_baton_t *pb;
} svn_cl__conflict_baton_t;
svn_cl__conflict_baton_t *
svn_cl__conflict_baton_make(svn_cl__accept_t accept_which,
apr_hash_t *config,
const char *editor_cmd,
svn_cmdline_prompt_baton_t *pb,
apr_pool_t *pool);
svn_error_t *
svn_cl__conflict_handler(svn_wc_conflict_result_t **result,
const svn_wc_conflict_description_t *desc,
void *baton,
apr_pool_t *pool);
svn_error_t *svn_cl__print_commit_info(svn_commit_info_t *commit_info,
apr_pool_t *pool);
svn_error_t *
svn_cl__time_cstring_to_human_cstring(const char **human_cstring,
const char *data,
apr_pool_t *pool);
svn_error_t *svn_cl__print_status(const char *path,
svn_wc_status2_t *status,
svn_boolean_t detailed,
svn_boolean_t show_last_committed,
svn_boolean_t skip_unrecognized,
svn_boolean_t repos_locks,
apr_pool_t *pool);
svn_error_t *
svn_cl__print_status_xml(const char *path,
svn_wc_status2_t *status,
apr_pool_t *pool);
svn_error_t *
svn_cl__print_prop_hash(apr_hash_t *prop_hash,
svn_boolean_t names_only,
apr_pool_t *pool);
void
svn_cl__print_xml_prop(svn_stringbuf_t **outstr,
const char* propname,
svn_string_t *propval,
apr_pool_t *pool);
svn_error_t *
svn_cl__print_xml_prop_hash(svn_stringbuf_t **outstr,
apr_hash_t *prop_hash,
svn_boolean_t names_only,
apr_pool_t *pool);
void
svn_cl__print_xml_commit(svn_stringbuf_t **outstr,
svn_revnum_t revision,
const char *author,
const char *date,
apr_pool_t *pool);
svn_error_t *
svn_cl__revprop_prepare(const svn_opt_revision_t *revision,
apr_array_header_t *targets,
const char **URL,
apr_pool_t *pool);
svn_error_t *
svn_cl__edit_string_externally(svn_string_t **edited_contents,
const char **tmpfile_left,
const char *editor_cmd,
const char *base_dir,
const svn_string_t *contents,
const char *prefix,
apr_hash_t *config,
svn_boolean_t as_text,
const char *encoding,
apr_pool_t *pool);
svn_error_t *
svn_cl__edit_file_externally(const char *path,
const char *editor_cmd,
apr_hash_t *config,
apr_pool_t *pool);
svn_error_t *
svn_cl__merge_file_externally(const char *base_path,
const char *their_path,
const char *my_path,
const char *merged_path,
apr_hash_t *config,
apr_pool_t *pool);
void svn_cl__get_notifier(svn_wc_notify_func2_t *notify_func_p,
void **notify_baton_p,
svn_boolean_t is_checkout,
svn_boolean_t is_export,
svn_boolean_t suppress_final_line,
apr_pool_t *pool);
svn_error_t *svn_cl__make_log_msg_baton(void **baton,
svn_cl__opt_state_t *opt_state,
const char *base_dir,
apr_hash_t *config,
apr_pool_t *pool);
svn_error_t *svn_cl__get_log_message(const char **log_msg,
const char **tmp_file,
const apr_array_header_t *commit_items,
void *baton,
apr_pool_t *pool);
svn_error_t *svn_cl__cleanup_log_msg(void *log_msg_baton,
svn_error_t *commit_err);
svn_error_t *svn_cl__may_need_force(svn_error_t *err);
svn_error_t *svn_cl__error_checked_fputs(const char *string,
FILE* stream);
void svn_cl__xml_tagged_cdata(svn_stringbuf_t **sb,
apr_pool_t *pool,
const char *tagname,
const char *string);
svn_error_t *svn_cl__xml_print_header(const char *tagname,
apr_pool_t *pool);
svn_error_t *svn_cl__xml_print_footer(const char *tagname,
apr_pool_t *pool);
const char *svn_cl__node_kind_str(svn_node_kind_t kind);
void svn_cl__check_boolean_prop_val(const char *propname,
const char *propval,
apr_pool_t *pool);
svn_error_t *svn_cl__changelist_paths(apr_array_header_t **paths,
const apr_array_header_t *changelists,
const apr_array_header_t *targets,
svn_depth_t depth,
svn_client_ctx_t *ctx,
apr_pool_t *pool);
svn_error_t *
svn_cl__args_to_target_array_print_reserved(apr_array_header_t **targets_p,
apr_getopt_t *os,
apr_array_header_t *known_targets,
apr_pool_t *pool);
#if defined(__cplusplus)
}
#endif
#endif
