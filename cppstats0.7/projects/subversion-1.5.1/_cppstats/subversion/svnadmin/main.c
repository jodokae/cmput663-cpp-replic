#include <apr_file_io.h>
#include <apr_signal.h>
#include "svn_pools.h"
#include "svn_cmdline.h"
#include "svn_error.h"
#include "svn_opt.h"
#include "svn_utf.h"
#include "svn_subst.h"
#include "svn_path.h"
#include "svn_config.h"
#include "svn_repos.h"
#include "svn_fs.h"
#include "svn_version.h"
#include "svn_props.h"
#include "svn_time.h"
#include "svn_user.h"
#include "svn_private_config.h"
static volatile sig_atomic_t cancelled = FALSE;
static void
signal_handler(int signum) {
apr_signal(signum, SIG_IGN);
cancelled = TRUE;
}
static void
setup_cancellation_signals(void (*handler)(int signum)) {
apr_signal(SIGINT, handler);
#if defined(SIGBREAK)
apr_signal(SIGBREAK, handler);
#endif
#if defined(SIGHUP)
apr_signal(SIGHUP, handler);
#endif
#if defined(SIGTERM)
apr_signal(SIGTERM, handler);
#endif
}
static svn_error_t *
check_cancel(void *baton) {
if (cancelled)
return svn_error_create(SVN_ERR_CANCELLED, NULL, _("Caught signal"));
else
return SVN_NO_ERROR;
}
static svn_error_t *
create_stdio_stream(svn_stream_t **stream,
APR_DECLARE(apr_status_t) open_fn(apr_file_t **,
apr_pool_t *),
apr_pool_t *pool) {
apr_file_t *stdio_file;
apr_status_t apr_err = open_fn(&stdio_file, pool);
if (apr_err)
return svn_error_wrap_apr(apr_err, _("Can't open stdio file"));
*stream = svn_stream_from_aprfile(stdio_file, pool);
return SVN_NO_ERROR;
}
static svn_error_t *
parse_local_repos_path(apr_getopt_t *os,
const char ** repos_path,
apr_pool_t *pool) {
*repos_path = NULL;
if (os->ind < os->argc) {
const char * path = os->argv[os->ind++];
SVN_ERR(svn_utf_cstring_to_utf8(repos_path, path, pool));
*repos_path = svn_path_internal_style(*repos_path, pool);
}
if (*repos_path == NULL) {
return svn_error_create(SVN_ERR_CL_ARG_PARSING_ERROR, NULL,
_("Repository argument required"));
} else if (svn_path_is_url(*repos_path)) {
return svn_error_createf(SVN_ERR_CL_ARG_PARSING_ERROR, NULL,
_("'%s' is an URL when it should be a path"),
*repos_path);
}
return SVN_NO_ERROR;
}
static void
warning_func(void *baton,
svn_error_t *err) {
if (! err)
return;
svn_handle_error2(err, stderr, FALSE, "svnadmin: ");
}
static svn_error_t *
open_repos(svn_repos_t **repos,
const char *path,
apr_pool_t *pool) {
SVN_ERR(svn_repos_open(repos, path, pool));
svn_fs_set_warning_func(svn_repos_fs(*repos), warning_func, NULL);
return SVN_NO_ERROR;
}
static svn_error_t *
check_lib_versions(void) {
static const svn_version_checklist_t checklist[] = {
{ "svn_subr", svn_subr_version },
{ "svn_repos", svn_repos_version },
{ "svn_fs", svn_fs_version },
{ "svn_delta", svn_delta_version },
{ NULL, NULL }
};
SVN_VERSION_DEFINE(my_version);
return svn_ver_check_list(&my_version, checklist);
}
static svn_opt_subcommand_t
subcommand_crashtest,
subcommand_create,
subcommand_deltify,
subcommand_dump,
subcommand_help,
subcommand_hotcopy,
subcommand_load,
subcommand_list_dblogs,
subcommand_list_unused_dblogs,
subcommand_lslocks,
subcommand_lstxns,
subcommand_recover,
subcommand_rmlocks,
subcommand_rmtxns,
subcommand_setlog,
subcommand_setrevprop,
subcommand_setuuid,
subcommand_upgrade,
subcommand_verify;
enum {
svnadmin__version = SVN_OPT_FIRST_LONGOPT_ID,
svnadmin__incremental,
svnadmin__deltas,
svnadmin__ignore_uuid,
svnadmin__force_uuid,
svnadmin__fs_type,
svnadmin__parent_dir,
svnadmin__bdb_txn_nosync,
svnadmin__bdb_log_keep,
svnadmin__config_dir,
svnadmin__bypass_hooks,
svnadmin__use_pre_commit_hook,
svnadmin__use_post_commit_hook,
svnadmin__use_pre_revprop_change_hook,
svnadmin__use_post_revprop_change_hook,
svnadmin__clean_logs,
svnadmin__wait,
svnadmin__pre_1_4_compatible,
svnadmin__pre_1_5_compatible
};
static const apr_getopt_option_t options_table[] = {
{
"help", 'h', 0,
N_("show help on a subcommand")
},
{
NULL, '?', 0,
N_("show help on a subcommand")
},
{
"version", svnadmin__version, 0,
N_("show program version information")
},
{
"revision", 'r', 1,
N_("specify revision number ARG (or X:Y range)")
},
{
"incremental", svnadmin__incremental, 0,
N_("dump incrementally")
},
{
"deltas", svnadmin__deltas, 0,
N_("use deltas in dump output")
},
{
"bypass-hooks", svnadmin__bypass_hooks, 0,
N_("bypass the repository hook system")
},
{
"quiet", 'q', 0,
N_("no progress (only errors) to stderr")
},
{
"ignore-uuid", svnadmin__ignore_uuid, 0,
N_("ignore any repos UUID found in the stream")
},
{
"force-uuid", svnadmin__force_uuid, 0,
N_("set repos UUID to that found in stream, if any")
},
{
"fs-type", svnadmin__fs_type, 1,
N_("type of repository: 'fsfs' (default) or 'bdb'")
},
{
"parent-dir", svnadmin__parent_dir, 1,
N_("load at specified directory in repository")
},
{
"bdb-txn-nosync", svnadmin__bdb_txn_nosync, 0,
N_("disable fsync at transaction commit [Berkeley DB]")
},
{
"bdb-log-keep", svnadmin__bdb_log_keep, 0,
N_("disable automatic log file removal [Berkeley DB]")
},
{
"config-dir", svnadmin__config_dir, 1,
N_("read user configuration files from directory ARG")
},
{
"clean-logs", svnadmin__clean_logs, 0,
N_("remove redundant Berkeley DB log files\n"
" from source repository [Berkeley DB]")
},
{
"use-pre-commit-hook", svnadmin__use_pre_commit_hook, 0,
N_("call pre-commit hook before committing revisions")
},
{
"use-post-commit-hook", svnadmin__use_post_commit_hook, 0,
N_("call post-commit hook after committing revisions")
},
{
"use-pre-revprop-change-hook", svnadmin__use_pre_revprop_change_hook, 0,
N_("call hook before changing revision property")
},
{
"use-post-revprop-change-hook", svnadmin__use_post_revprop_change_hook, 0,
N_("call hook after changing revision property")
},
{
"wait", svnadmin__wait, 0,
N_("wait instead of exit if the repository is in\n"
" use by another process")
},
{
"pre-1.4-compatible", svnadmin__pre_1_4_compatible, 0,
N_("use format compatible with Subversion versions\n"
" earlier than 1.4")
},
{
"pre-1.5-compatible", svnadmin__pre_1_5_compatible, 0,
N_("use format compatible with Subversion versions\n"
" earlier than 1.5")
},
{NULL}
};
static const svn_opt_subcommand_desc_t cmd_table[] = {
{
"crashtest", subcommand_crashtest, {0}, N_
("usage: svnadmin crashtest REPOS_PATH\n\n"
"Open the repository at REPOS_PATH, then abort, thus simulating\n"
"a process that crashes while holding an open repository handle.\n"),
{0}
},
{
"create", subcommand_create, {0}, N_
("usage: svnadmin create REPOS_PATH\n\n"
"Create a new, empty repository at REPOS_PATH.\n"),
{
svnadmin__bdb_txn_nosync, svnadmin__bdb_log_keep,
svnadmin__config_dir, svnadmin__fs_type, svnadmin__pre_1_4_compatible,
svnadmin__pre_1_5_compatible
}
},
{
"deltify", subcommand_deltify, {0}, N_
("usage: svnadmin deltify [-r LOWER[:UPPER]] REPOS_PATH\n\n"
"Run over the requested revision range, performing predecessor delti-\n"
"fication on the paths changed in those revisions. Deltification in\n"
"essence compresses the repository by only storing the differences or\n"
"delta from the preceding revision. If no revisions are specified,\n"
"this will simply deltify the HEAD revision.\n"),
{'r', 'q'}
},
{
"dump", subcommand_dump, {0}, N_
("usage: svnadmin dump REPOS_PATH [-r LOWER[:UPPER]] [--incremental]\n\n"
"Dump the contents of filesystem to stdout in a 'dumpfile'\n"
"portable format, sending feedback to stderr. Dump revisions\n"
"LOWER rev through UPPER rev. If no revisions are given, dump all\n"
"revision trees. If only LOWER is given, dump that one revision tree.\n"
"If --incremental is passed, then the first revision dumped will be\n"
"a diff against the previous revision, instead of the usual fulltext.\n"),
{'r', svnadmin__incremental, svnadmin__deltas, 'q'}
},
{
"help", subcommand_help, {"?", "h"}, N_
("usage: svnadmin help [SUBCOMMAND...]\n\n"
"Describe the usage of this program or its subcommands.\n"),
{0}
},
{
"hotcopy", subcommand_hotcopy, {0}, N_
("usage: svnadmin hotcopy REPOS_PATH NEW_REPOS_PATH\n\n"
"Makes a hot copy of a repository.\n"),
{svnadmin__clean_logs}
},
{
"list-dblogs", subcommand_list_dblogs, {0}, N_
("usage: svnadmin list-dblogs REPOS_PATH\n\n"
"List all Berkeley DB log files.\n\n"
"WARNING: Modifying or deleting logfiles which are still in use\n"
"will cause your repository to be corrupted.\n"),
{0}
},
{
"list-unused-dblogs", subcommand_list_unused_dblogs, {0}, N_
("usage: svnadmin list-unused-dblogs REPOS_PATH\n\n"
"List unused Berkeley DB log files.\n\n"),
{0}
},
{
"load", subcommand_load, {0}, N_
("usage: svnadmin load REPOS_PATH\n\n"
"Read a 'dumpfile'-formatted stream from stdin, committing\n"
"new revisions into the repository's filesystem. If the repository\n"
"was previously empty, its UUID will, by default, be changed to the\n"
"one specified in the stream. Progress feedback is sent to stdout.\n"),
{
'q', svnadmin__ignore_uuid, svnadmin__force_uuid,
svnadmin__use_pre_commit_hook, svnadmin__use_post_commit_hook,
svnadmin__parent_dir
}
},
{
"lslocks", subcommand_lslocks, {0}, N_
("usage: svnadmin lslocks REPOS_PATH [PATH-IN-REPOS]\n\n"
"Print descriptions of all locks on or under PATH-IN-REPOS (which,\n"
"if not provided, is the root of the repository).\n"),
{0}
},
{
"lstxns", subcommand_lstxns, {0}, N_
("usage: svnadmin lstxns REPOS_PATH\n\n"
"Print the names of all uncommitted transactions.\n"),
{0}
},
{
"recover", subcommand_recover, {0}, N_
("usage: svnadmin recover REPOS_PATH\n\n"
"Run the recovery procedure on a repository. Do this if you've\n"
"been getting errors indicating that recovery ought to be run.\n"
"Berkeley DB recovery requires exclusive access and will\n"
"exit if the repository is in use by another process.\n"),
{svnadmin__wait}
},
{
"rmlocks", subcommand_rmlocks, {0}, N_
("usage: svnadmin rmlocks REPOS_PATH LOCKED_PATH...\n\n"
"Unconditionally remove lock from each LOCKED_PATH.\n"),
{0}
},
{
"rmtxns", subcommand_rmtxns, {0}, N_
("usage: svnadmin rmtxns REPOS_PATH TXN_NAME...\n\n"
"Delete the named transaction(s).\n"),
{'q'}
},
{
"setlog", subcommand_setlog, {0}, N_
("usage: svnadmin setlog REPOS_PATH -r REVISION FILE\n\n"
"Set the log-message on revision REVISION to the contents of FILE. Use\n"
"--bypass-hooks to avoid triggering the revision-property-related hooks\n"
"(for example, if you do not want an email notification sent\n"
"from your post-revprop-change hook, or because the modification of\n"
"revision properties has not been enabled in the pre-revprop-change\n"
"hook).\n\n"
"NOTE: Revision properties are not versioned, so this command will\n"
"overwrite the previous log message.\n"),
{'r', svnadmin__bypass_hooks}
},
{
"setrevprop", subcommand_setrevprop, {0}, N_
("usage: svnadmin setrevprop REPOS_PATH -r REVISION NAME FILE\n\n"
"Set the property NAME on revision REVISION to the contents of FILE. Use\n"
"--use-pre-revprop-change-hook/--use-post-revprop-change-hook to trigger\n"
"the revision property-related hooks (for example, if you want an email\n"
"notification sent from your post-revprop-change hook).\n\n"
"NOTE: Revision properties are not versioned, so this command will\n"
"overwrite the previous value of the property.\n"),
{
'r', svnadmin__use_pre_revprop_change_hook,
svnadmin__use_post_revprop_change_hook
}
},
{
"setuuid", subcommand_setuuid, {0}, N_
("usage: svnadmin setuuid REPOS_PATH [NEW_UUID]\n\n"
"Reset the repository UUID for the repository located at REPOS_PATH. If\n"
"NEW_UUID is provided, use that as the new repository UUID; otherwise,\n"
"generate a brand new UUID for the repository.\n"),
{0}
},
{
"upgrade", subcommand_upgrade, {0}, N_
("usage: svnadmin upgrade REPOS_PATH\n\n"
"Upgrade the repository located at REPOS_PATH to the latest supported\n"
"schema version.\n\n"
"This functionality is provided as a convenience for repository\n"
"administrators who wish to make use of new Subversion functionality\n"
"without having to undertake a potentially costly full repository dump\n"
"and load operation. As such, the upgrade performs only the minimum\n"
"amount of work needed to accomplish this while still maintaining the\n"
"integrity of the repository. It does not guarantee the most optimized\n"
"repository state as a dump and subsequent load would.\n"),
{0}
},
{
"verify", subcommand_verify, {0}, N_
("usage: svnadmin verify REPOS_PATH\n\n"
"Verifies the data stored in the repository.\n"),
{'r', 'q'}
},
{ NULL, NULL, {0}, NULL, {0} }
};
struct svnadmin_opt_state {
const char *repository_path;
const char *new_repository_path;
const char *fs_type;
svn_boolean_t pre_1_4_compatible;
svn_boolean_t pre_1_5_compatible;
svn_opt_revision_t start_revision, end_revision;
svn_boolean_t help;
svn_boolean_t version;
svn_boolean_t incremental;
svn_boolean_t use_deltas;
svn_boolean_t use_pre_commit_hook;
svn_boolean_t use_post_commit_hook;
svn_boolean_t use_pre_revprop_change_hook;
svn_boolean_t use_post_revprop_change_hook;
svn_boolean_t quiet;
svn_boolean_t bdb_txn_nosync;
svn_boolean_t bdb_log_keep;
svn_boolean_t clean_logs;
svn_boolean_t bypass_hooks;
svn_boolean_t wait;
enum svn_repos_load_uuid uuid_action;
const char *parent_dir;
const char *config_dir;
};
static svn_error_t *
get_revnum(svn_revnum_t *revnum, const svn_opt_revision_t *revision,
svn_revnum_t youngest, svn_repos_t *repos, apr_pool_t *pool) {
if (revision->kind == svn_opt_revision_number)
*revnum = revision->value.number;
else if (revision->kind == svn_opt_revision_head)
*revnum = youngest;
else if (revision->kind == svn_opt_revision_date)
SVN_ERR(svn_repos_dated_revision
(revnum, repos, revision->value.date, pool));
else if (revision->kind == svn_opt_revision_unspecified)
*revnum = SVN_INVALID_REVNUM;
else
return svn_error_create
(SVN_ERR_CL_ARG_PARSING_ERROR, NULL, _("Invalid revision specifier"));
if (*revnum > youngest)
return svn_error_createf
(SVN_ERR_CL_ARG_PARSING_ERROR, NULL,
_("Revisions must not be greater than the youngest revision (%ld)"),
youngest);
return SVN_NO_ERROR;
}
static svn_error_t *
subcommand_create(apr_getopt_t *os, void *baton, apr_pool_t *pool) {
struct svnadmin_opt_state *opt_state = baton;
svn_repos_t *repos;
apr_hash_t *config;
apr_hash_t *fs_config = apr_hash_make(pool);
apr_hash_set(fs_config, SVN_FS_CONFIG_BDB_TXN_NOSYNC,
APR_HASH_KEY_STRING,
(opt_state->bdb_txn_nosync ? "1" : "0"));
apr_hash_set(fs_config, SVN_FS_CONFIG_BDB_LOG_AUTOREMOVE,
APR_HASH_KEY_STRING,
(opt_state->bdb_log_keep ? "0" : "1"));
if (opt_state->fs_type)
apr_hash_set(fs_config, SVN_FS_CONFIG_FS_TYPE,
APR_HASH_KEY_STRING,
opt_state->fs_type);
if (opt_state->pre_1_4_compatible)
apr_hash_set(fs_config, SVN_FS_CONFIG_PRE_1_4_COMPATIBLE,
APR_HASH_KEY_STRING,
"1");
if (opt_state->pre_1_5_compatible)
apr_hash_set(fs_config, SVN_FS_CONFIG_PRE_1_5_COMPATIBLE,
APR_HASH_KEY_STRING,
"1");
SVN_ERR(svn_config_get_config(&config, opt_state->config_dir, pool));
SVN_ERR(svn_repos_create(&repos, opt_state->repository_path,
NULL, NULL,
config, fs_config, pool));
svn_fs_set_warning_func(svn_repos_fs(repos), warning_func, NULL);
return SVN_NO_ERROR;
}
static svn_error_t *
subcommand_deltify(apr_getopt_t *os, void *baton, apr_pool_t *pool) {
struct svnadmin_opt_state *opt_state = baton;
svn_repos_t *repos;
svn_fs_t *fs;
svn_revnum_t start = SVN_INVALID_REVNUM, end = SVN_INVALID_REVNUM;
svn_revnum_t youngest, revision;
apr_pool_t *subpool = svn_pool_create(pool);
SVN_ERR(open_repos(&repos, opt_state->repository_path, pool));
fs = svn_repos_fs(repos);
SVN_ERR(svn_fs_youngest_rev(&youngest, fs, pool));
SVN_ERR(get_revnum(&start, &opt_state->start_revision,
youngest, repos, pool));
SVN_ERR(get_revnum(&end, &opt_state->end_revision,
youngest, repos, pool));
if (start == SVN_INVALID_REVNUM)
start = youngest;
if (end == SVN_INVALID_REVNUM)
end = start;
if (start > end)
return svn_error_create
(SVN_ERR_CL_ARG_PARSING_ERROR, NULL,
_("First revision cannot be higher than second"));
for (revision = start; revision <= end; revision++) {
svn_pool_clear(subpool);
SVN_ERR(check_cancel(NULL));
if (! opt_state->quiet)
SVN_ERR(svn_cmdline_printf(subpool, _("Deltifying revision %ld..."),
revision));
SVN_ERR(svn_fs_deltify_revision(fs, revision, subpool));
if (! opt_state->quiet)
SVN_ERR(svn_cmdline_printf(subpool, _("done.\n")));
}
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
struct recode_write_baton {
apr_pool_t *pool;
FILE *out;
};
static svn_error_t *recode_write(void *baton,
const char *data,
apr_size_t *len) {
struct recode_write_baton *rwb = baton;
svn_pool_clear(rwb->pool);
return svn_cmdline_fputs(data, rwb->out, rwb->pool);
}
static svn_stream_t *
recode_stream_create(FILE *std_stream, apr_pool_t *pool) {
struct recode_write_baton *std_stream_rwb =
apr_palloc(pool, sizeof(struct recode_write_baton));
svn_stream_t *rw_stream = svn_stream_create(std_stream_rwb, pool);
std_stream_rwb->pool = svn_pool_create(pool);
std_stream_rwb->out = std_stream;
svn_stream_set_write(rw_stream, recode_write);
return rw_stream;
}
static svn_error_t *
subcommand_dump(apr_getopt_t *os, void *baton, apr_pool_t *pool) {
struct svnadmin_opt_state *opt_state = baton;
svn_repos_t *repos;
svn_fs_t *fs;
svn_stream_t *stdout_stream, *stderr_stream = NULL;
svn_revnum_t lower = SVN_INVALID_REVNUM, upper = SVN_INVALID_REVNUM;
svn_revnum_t youngest;
SVN_ERR(open_repos(&repos, opt_state->repository_path, pool));
fs = svn_repos_fs(repos);
SVN_ERR(svn_fs_youngest_rev(&youngest, fs, pool));
SVN_ERR(get_revnum(&lower, &opt_state->start_revision,
youngest, repos, pool));
SVN_ERR(get_revnum(&upper, &opt_state->end_revision,
youngest, repos, pool));
if (lower == SVN_INVALID_REVNUM) {
lower = 0;
upper = youngest;
} else if (upper == SVN_INVALID_REVNUM) {
upper = lower;
}
if (lower > upper)
return svn_error_create
(SVN_ERR_CL_ARG_PARSING_ERROR, NULL,
_("First revision cannot be higher than second"));
SVN_ERR(create_stdio_stream(&stdout_stream, apr_file_open_stdout, pool));
if (! opt_state->quiet)
stderr_stream = recode_stream_create(stderr, pool);
SVN_ERR(svn_repos_dump_fs2(repos, stdout_stream, stderr_stream,
lower, upper, opt_state->incremental,
opt_state->use_deltas, check_cancel, NULL,
pool));
return SVN_NO_ERROR;
}
static svn_error_t *
subcommand_help(apr_getopt_t *os, void *baton, apr_pool_t *pool) {
struct svnadmin_opt_state *opt_state = baton;
const char *header =
_("general usage: svnadmin SUBCOMMAND REPOS_PATH [ARGS & OPTIONS ...]\n"
"Type 'svnadmin help <subcommand>' for help on a specific subcommand.\n"
"Type 'svnadmin --version' to see the program version and FS modules.\n"
"\n"
"Available subcommands:\n");
const char *fs_desc_start
= _("The following repository back-end (FS) modules are available:\n\n");
svn_stringbuf_t *version_footer;
version_footer = svn_stringbuf_create(fs_desc_start, pool);
SVN_ERR(svn_fs_print_modules(version_footer, pool));
SVN_ERR(svn_opt_print_help(os, "svnadmin",
opt_state ? opt_state->version : FALSE,
FALSE, version_footer->data,
header, cmd_table, options_table, NULL,
pool));
return SVN_NO_ERROR;
}
static svn_error_t *
subcommand_load(apr_getopt_t *os, void *baton, apr_pool_t *pool) {
struct svnadmin_opt_state *opt_state = baton;
svn_repos_t *repos;
svn_stream_t *stdin_stream, *stdout_stream = NULL;
SVN_ERR(open_repos(&repos, opt_state->repository_path, pool));
SVN_ERR(create_stdio_stream(&stdin_stream,
apr_file_open_stdin, pool));
if (! opt_state->quiet)
stdout_stream = recode_stream_create(stdout, pool);
SVN_ERR(svn_repos_load_fs2(repos, stdin_stream, stdout_stream,
opt_state->uuid_action, opt_state->parent_dir,
opt_state->use_pre_commit_hook,
opt_state->use_post_commit_hook,
check_cancel, NULL, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
subcommand_lstxns(apr_getopt_t *os, void *baton, apr_pool_t *pool) {
struct svnadmin_opt_state *opt_state = baton;
svn_repos_t *repos;
svn_fs_t *fs;
apr_array_header_t *txns;
int i;
SVN_ERR(open_repos(&repos, opt_state->repository_path, pool));
fs = svn_repos_fs(repos);
SVN_ERR(svn_fs_list_transactions(&txns, fs, pool));
for (i = 0; i < txns->nelts; i++) {
SVN_ERR(svn_cmdline_printf(pool, "%s\n",
APR_ARRAY_IDX(txns, i, const char *)));
}
return SVN_NO_ERROR;
}
static svn_error_t *
recovery_started(void *baton) {
apr_pool_t *pool = (apr_pool_t *)baton;
SVN_ERR(svn_cmdline_printf(pool,
_("Repository lock acquired.\n"
"Please wait; recovering the"
" repository may take some time...\n")));
SVN_ERR(svn_cmdline_fflush(stdout));
setup_cancellation_signals(signal_handler);
return SVN_NO_ERROR;
}
static svn_error_t *
subcommand_recover(apr_getopt_t *os, void *baton, apr_pool_t *pool) {
svn_revnum_t youngest_rev;
svn_repos_t *repos;
svn_error_t *err;
struct svnadmin_opt_state *opt_state = baton;
setup_cancellation_signals(SIG_DFL);
err = svn_repos_recover3(opt_state->repository_path, TRUE,
recovery_started, pool,
check_cancel, NULL, pool);
if (err) {
if (! APR_STATUS_IS_EAGAIN(err->apr_err))
return err;
svn_error_clear(err);
if (! opt_state->wait)
return svn_error_create(SVN_ERR_REPOS_LOCKED, NULL,
_("Failed to get exclusive repository "
"access; perhaps another process\n"
"such as httpd, svnserve or svn "
"has it open?"));
SVN_ERR(svn_cmdline_printf(pool,
_("Waiting on repository lock; perhaps"
" another process has it open?\n")));
SVN_ERR(svn_cmdline_fflush(stdout));
SVN_ERR(svn_repos_recover3(opt_state->repository_path, FALSE,
recovery_started, pool,
check_cancel, NULL, pool));
}
SVN_ERR(svn_cmdline_printf(pool, _("\nRecovery completed.\n")));
SVN_ERR(open_repos(&repos, opt_state->repository_path, pool));
SVN_ERR(svn_fs_youngest_rev(&youngest_rev, svn_repos_fs(repos), pool));
SVN_ERR(svn_cmdline_printf(pool, _("The latest repos revision is %ld.\n"),
youngest_rev));
return SVN_NO_ERROR;
}
static svn_error_t *
list_dblogs(apr_getopt_t *os, void *baton, svn_boolean_t only_unused,
apr_pool_t *pool) {
struct svnadmin_opt_state *opt_state = baton;
apr_array_header_t *logfiles;
int i;
SVN_ERR(svn_repos_db_logfiles(&logfiles,
opt_state->repository_path,
only_unused,
pool));
for (i = 0; i < logfiles->nelts; i++) {
const char *log_utf8;
log_utf8 = svn_path_join(opt_state->repository_path,
APR_ARRAY_IDX(logfiles, i, const char *),
pool);
log_utf8 = svn_path_local_style(log_utf8, pool);
SVN_ERR(svn_cmdline_printf(pool, "%s\n", log_utf8));
}
return SVN_NO_ERROR;
}
static svn_error_t *
subcommand_list_dblogs(apr_getopt_t *os, void *baton, apr_pool_t *pool) {
SVN_ERR(list_dblogs(os, baton, FALSE, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
subcommand_list_unused_dblogs(apr_getopt_t *os, void *baton, apr_pool_t *pool) {
SVN_ERR(list_dblogs(os, baton, TRUE, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
subcommand_rmtxns(apr_getopt_t *os, void *baton, apr_pool_t *pool) {
struct svnadmin_opt_state *opt_state = baton;
svn_repos_t *repos;
svn_fs_t *fs;
svn_fs_txn_t *txn;
apr_array_header_t *args;
int i;
apr_pool_t *subpool = svn_pool_create(pool);
SVN_ERR(open_repos(&repos, opt_state->repository_path, pool));
fs = svn_repos_fs(repos);
SVN_ERR(svn_opt_parse_all_args(&args, os, pool));
for (i = 0; i < args->nelts; i++) {
const char *txn_name = APR_ARRAY_IDX(args, i, const char *);
const char *txn_name_utf8;
svn_error_t *err;
svn_pool_clear(subpool);
SVN_ERR(svn_utf_cstring_to_utf8(&txn_name_utf8, txn_name, subpool));
err = svn_fs_open_txn(&txn, fs, txn_name_utf8, subpool);
if (! err)
err = svn_fs_abort_txn(txn, subpool);
if (err && (err->apr_err == SVN_ERR_FS_TRANSACTION_DEAD)) {
svn_error_clear(err);
err = svn_fs_purge_txn(fs, txn_name_utf8, subpool);
}
if (err) {
svn_handle_error2(err, stderr, FALSE , "svnadmin: ");
svn_error_clear(err);
} else if (! opt_state->quiet) {
SVN_ERR
(svn_cmdline_printf(subpool, _("Transaction '%s' removed.\n"),
txn_name));
}
}
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
static svn_error_t *
set_revprop(const char *prop_name, const char *filename,
struct svnadmin_opt_state *opt_state, apr_pool_t *pool) {
svn_repos_t *repos;
svn_string_t *prop_value = svn_string_create("", pool);
svn_stringbuf_t *file_contents;
const char *filename_utf8;
SVN_ERR(svn_utf_cstring_to_utf8(&filename_utf8, filename, pool));
filename_utf8 = svn_path_internal_style(filename_utf8, pool);
SVN_ERR(svn_stringbuf_from_file(&file_contents, filename_utf8, pool));
prop_value->data = file_contents->data;
prop_value->len = file_contents->len;
SVN_ERR(svn_subst_translate_string(&prop_value, prop_value, NULL, pool));
SVN_ERR(open_repos(&repos, opt_state->repository_path, pool));
if (opt_state->use_pre_revprop_change_hook ||
opt_state->use_post_revprop_change_hook) {
SVN_ERR(svn_repos_fs_change_rev_prop3
(repos, opt_state->start_revision.value.number,
NULL, prop_name, prop_value,
opt_state->use_pre_revprop_change_hook,
opt_state->use_post_revprop_change_hook, NULL, NULL, pool));
} else {
svn_fs_t *fs = svn_repos_fs(repos);
SVN_ERR(svn_fs_change_rev_prop
(fs, opt_state->start_revision.value.number,
prop_name, prop_value, pool));
}
return SVN_NO_ERROR;
}
static svn_error_t *
subcommand_setrevprop(apr_getopt_t *os, void *baton, apr_pool_t *pool) {
struct svnadmin_opt_state *opt_state = baton;
apr_array_header_t *args;
if (opt_state->start_revision.kind != svn_opt_revision_number)
return svn_error_createf(SVN_ERR_CL_ARG_PARSING_ERROR, NULL,
_("Missing revision"));
else if (opt_state->end_revision.kind != svn_opt_revision_unspecified)
return svn_error_createf(SVN_ERR_CL_ARG_PARSING_ERROR, NULL,
_("Only one revision allowed"));
SVN_ERR(svn_opt_parse_all_args(&args, os, pool));
if (args->nelts != 2)
return svn_error_createf(SVN_ERR_CL_ARG_PARSING_ERROR, NULL,
_("Exactly one property name and one file "
"argument required"));
return set_revprop(APR_ARRAY_IDX(args, 0, const char *),
APR_ARRAY_IDX(args, 1, const char *),
opt_state, pool);
}
static svn_error_t *
subcommand_setuuid(apr_getopt_t *os, void *baton, apr_pool_t *pool) {
struct svnadmin_opt_state *opt_state = baton;
apr_array_header_t *args;
svn_repos_t *repos;
svn_fs_t *fs;
const char *uuid = NULL;
SVN_ERR(svn_opt_parse_all_args(&args, os, pool));
if (args->nelts > 1)
return svn_error_create(SVN_ERR_CL_ARG_PARSING_ERROR, NULL, NULL);
if (args->nelts == 1)
uuid = APR_ARRAY_IDX(args, 0, const char *);
SVN_ERR(open_repos(&repos, opt_state->repository_path, pool));
fs = svn_repos_fs(repos);
return svn_fs_set_uuid(fs, uuid, pool);
}
static svn_error_t *
subcommand_setlog(apr_getopt_t *os, void *baton, apr_pool_t *pool) {
struct svnadmin_opt_state *opt_state = baton;
apr_array_header_t *args;
if (opt_state->start_revision.kind != svn_opt_revision_number)
return svn_error_createf(SVN_ERR_CL_ARG_PARSING_ERROR, NULL,
_("Missing revision"));
else if (opt_state->end_revision.kind != svn_opt_revision_unspecified)
return svn_error_createf(SVN_ERR_CL_ARG_PARSING_ERROR, NULL,
_("Only one revision allowed"));
SVN_ERR(svn_opt_parse_all_args(&args, os, pool));
if (args->nelts != 1)
return svn_error_createf(SVN_ERR_CL_ARG_PARSING_ERROR, NULL,
_("Exactly one file argument required"));
if (!opt_state->bypass_hooks) {
opt_state->use_pre_revprop_change_hook = TRUE;
opt_state->use_post_revprop_change_hook = TRUE;
}
return set_revprop(SVN_PROP_REVISION_LOG,
APR_ARRAY_IDX(args, 0, const char *),
opt_state, pool);
}
static svn_error_t *
subcommand_verify(apr_getopt_t *os, void *baton, apr_pool_t *pool) {
struct svnadmin_opt_state *opt_state = baton;
svn_stream_t *stderr_stream;
svn_repos_t *repos;
svn_fs_t *fs;
svn_revnum_t youngest, lower, upper;
SVN_ERR(open_repos(&repos, opt_state->repository_path, pool));
fs = svn_repos_fs(repos);
SVN_ERR(svn_fs_youngest_rev(&youngest, fs, pool));
SVN_ERR(get_revnum(&lower, &opt_state->start_revision,
youngest, repos, pool));
SVN_ERR(get_revnum(&upper, &opt_state->end_revision,
youngest, repos, pool));
if (upper == SVN_INVALID_REVNUM) {
upper = lower;
}
if (opt_state->quiet)
stderr_stream = NULL;
else
stderr_stream = recode_stream_create(stderr, pool);
SVN_ERR(open_repos(&repos, opt_state->repository_path, pool));
return svn_repos_verify_fs(repos, stderr_stream, lower, upper,
check_cancel, NULL, pool);
}
svn_error_t *
subcommand_hotcopy(apr_getopt_t *os, void *baton, apr_pool_t *pool) {
struct svnadmin_opt_state *opt_state = baton;
SVN_ERR(svn_repos_hotcopy(opt_state->repository_path,
opt_state->new_repository_path,
opt_state->clean_logs,
pool));
return SVN_NO_ERROR;
}
static svn_error_t *
subcommand_lslocks(apr_getopt_t *os, void *baton, apr_pool_t *pool) {
struct svnadmin_opt_state *opt_state = baton;
apr_array_header_t *targets;
svn_repos_t *repos;
const char *fs_path = "/";
svn_fs_t *fs;
apr_hash_t *locks;
apr_hash_index_t *hi;
SVN_ERR(svn_opt_args_to_target_array2(&targets, os,
apr_array_make(pool, 0,
sizeof(const char *)),
pool));
if (targets->nelts > 1)
return svn_error_create(SVN_ERR_CL_ARG_PARSING_ERROR, 0, NULL);
if (targets->nelts)
fs_path = APR_ARRAY_IDX(targets, 0, const char *);
SVN_ERR(open_repos(&repos, opt_state->repository_path, pool));
fs = svn_repos_fs(repos);
SVN_ERR(svn_repos_fs_get_locks(&locks, repos, fs_path, NULL, NULL, pool));
for (hi = apr_hash_first(pool, locks); hi; hi = apr_hash_next(hi)) {
const void *key;
void *val;
const char *path, *cr_date, *exp_date = "";
svn_lock_t *lock;
int comment_lines = 0;
apr_hash_this(hi, &key, NULL, &val);
path = key;
lock = val;
cr_date = svn_time_to_human_cstring(lock->creation_date, pool);
if (lock->expiration_date)
exp_date = svn_time_to_human_cstring(lock->expiration_date, pool);
if (lock->comment)
comment_lines = svn_cstring_count_newlines(lock->comment) + 1;
SVN_ERR(svn_cmdline_printf(pool, _("Path: %s\n"), path));
SVN_ERR(svn_cmdline_printf(pool, _("UUID Token: %s\n"), lock->token));
SVN_ERR(svn_cmdline_printf(pool, _("Owner: %s\n"), lock->owner));
SVN_ERR(svn_cmdline_printf(pool, _("Created: %s\n"), cr_date));
SVN_ERR(svn_cmdline_printf(pool, _("Expires: %s\n"), exp_date));
SVN_ERR(svn_cmdline_printf(pool, (comment_lines != 1)
? _("Comment (%i lines):\n%s\n\n")
: _("Comment (%i line):\n%s\n\n"),
comment_lines,
lock->comment ? lock->comment : ""));
}
return SVN_NO_ERROR;
}
static svn_error_t *
subcommand_rmlocks(apr_getopt_t *os, void *baton, apr_pool_t *pool) {
struct svnadmin_opt_state *opt_state = baton;
svn_repos_t *repos;
svn_fs_t *fs;
svn_fs_access_t *access;
svn_error_t *err;
apr_array_header_t *args;
int i;
const char *username;
apr_pool_t *subpool = svn_pool_create(pool);
SVN_ERR(open_repos(&repos, opt_state->repository_path, pool));
fs = svn_repos_fs(repos);
username = svn_user_get_name(pool);
if (! username)
username = "administrator";
SVN_ERR(svn_fs_create_access(&access, username, pool));
SVN_ERR(svn_fs_set_access(fs, access));
SVN_ERR(svn_opt_parse_all_args(&args, os, pool));
if (args->nelts == 0)
return svn_error_create(SVN_ERR_CL_ARG_PARSING_ERROR, 0,
_("No paths to unlock provided"));
for (i = 0; i < args->nelts; i++) {
const char *lock_path = APR_ARRAY_IDX(args, i, const char *);
const char *lock_path_utf8;
svn_lock_t *lock;
SVN_ERR(svn_utf_cstring_to_utf8(&lock_path_utf8, lock_path, subpool));
err = svn_fs_get_lock(&lock, fs, lock_path_utf8, subpool);
if (err)
goto move_on;
if (! lock) {
SVN_ERR(svn_cmdline_printf(subpool,
_("Path '%s' isn't locked.\n"),
lock_path));
continue;
}
err = svn_fs_unlock(fs, lock_path_utf8,
lock->token, 1 , subpool);
if (err)
goto move_on;
SVN_ERR(svn_cmdline_printf(subpool,
_("Removed lock on '%s'.\n"), lock->path));
move_on:
if (err) {
svn_handle_error2(err, stderr, FALSE , "svnadmin: ");
svn_error_clear(err);
}
svn_pool_clear(subpool);
}
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
static svn_error_t *
upgrade_started(void *baton) {
apr_pool_t *pool = (apr_pool_t *)baton;
SVN_ERR(svn_cmdline_printf(pool,
_("Repository lock acquired.\n"
"Please wait; upgrading the"
" repository may take some time...\n")));
SVN_ERR(svn_cmdline_fflush(stdout));
setup_cancellation_signals(signal_handler);
return SVN_NO_ERROR;
}
static svn_error_t *
subcommand_upgrade(apr_getopt_t *os, void *baton, apr_pool_t *pool) {
svn_error_t *err;
struct svnadmin_opt_state *opt_state = baton;
setup_cancellation_signals(SIG_DFL);
err = svn_repos_upgrade(opt_state->repository_path, TRUE,
upgrade_started, pool, pool);
if (err) {
if (APR_STATUS_IS_EAGAIN(err->apr_err)) {
svn_error_clear(err);
err = SVN_NO_ERROR;
if (! opt_state->wait)
return svn_error_create(SVN_ERR_REPOS_LOCKED, NULL,
_("Failed to get exclusive repository "
"access; perhaps another process\n"
"such as httpd, svnserve or svn "
"has it open?"));
SVN_ERR(svn_cmdline_printf(pool,
_("Waiting on repository lock; perhaps"
" another process has it open?\n")));
SVN_ERR(svn_cmdline_fflush(stdout));
SVN_ERR(svn_repos_upgrade(opt_state->repository_path, FALSE,
upgrade_started, pool, pool));
} else if (err->apr_err == SVN_ERR_FS_UNSUPPORTED_UPGRADE) {
return svn_error_quick_wrap
(err, _("Upgrade of this repository's underlying versioned "
"filesystem is not supported; consider "
"dumping and loading the data elsewhere"));
} else if (err->apr_err == SVN_ERR_REPOS_UNSUPPORTED_UPGRADE) {
return svn_error_quick_wrap
(err, _("Upgrade of this repository is not supported; consider "
"dumping and loading the data elsewhere"));
}
}
SVN_ERR(err);
SVN_ERR(svn_cmdline_printf(pool, _("\nUpgrade completed.\n")));
return SVN_NO_ERROR;
}
int
main(int argc, const char *argv[]) {
svn_error_t *err;
apr_status_t apr_err;
apr_allocator_t *allocator;
apr_pool_t *pool;
const svn_opt_subcommand_desc_t *subcommand = NULL;
struct svnadmin_opt_state opt_state;
apr_getopt_t *os;
int opt_id;
apr_array_header_t *received_opts;
int i;
if (svn_cmdline_init("svnadmin", stderr) != EXIT_SUCCESS)
return EXIT_FAILURE;
if (apr_allocator_create(&allocator))
return EXIT_FAILURE;
apr_allocator_max_free_set(allocator, SVN_ALLOCATOR_RECOMMENDED_MAX_FREE);
pool = svn_pool_create_ex(NULL, allocator);
apr_allocator_owner_set(allocator, pool);
received_opts = apr_array_make(pool, SVN_OPT_MAX_OPTIONS, sizeof(int));
err = check_lib_versions();
if (err)
return svn_cmdline_handle_exit_error(err, pool, "svnadmin: ");
err = svn_fs_initialize(pool);
if (err)
return svn_cmdline_handle_exit_error(err, pool, "svnadmin: ");
if (argc <= 1) {
subcommand_help(NULL, NULL, pool);
svn_pool_destroy(pool);
return EXIT_FAILURE;
}
memset(&opt_state, 0, sizeof(opt_state));
opt_state.start_revision.kind = svn_opt_revision_unspecified;
opt_state.end_revision.kind = svn_opt_revision_unspecified;
err = svn_cmdline__getopt_init(&os, argc, argv, pool);
if (err)
return svn_cmdline_handle_exit_error(err, pool, "svnadmin: ");
os->interleave = 1;
while (1) {
const char *opt_arg;
const char *utf8_opt_arg;
apr_err = apr_getopt_long(os, options_table, &opt_id, &opt_arg);
if (APR_STATUS_IS_EOF(apr_err))
break;
else if (apr_err) {
subcommand_help(NULL, NULL, pool);
svn_pool_destroy(pool);
return EXIT_FAILURE;
}
APR_ARRAY_PUSH(received_opts, int) = opt_id;
switch (opt_id) {
case 'r': {
if (opt_state.start_revision.kind != svn_opt_revision_unspecified) {
err = svn_error_create
(SVN_ERR_CL_ARG_PARSING_ERROR, NULL,
_("Multiple revision arguments encountered; "
"try '-r N:M' instead of '-r N -r M'"));
return svn_cmdline_handle_exit_error(err, pool, "svnadmin: ");
}
if (svn_opt_parse_revision(&(opt_state.start_revision),
&(opt_state.end_revision),
opt_arg, pool) != 0) {
err = svn_utf_cstring_to_utf8(&utf8_opt_arg, opt_arg,
pool);
if (! err)
err = svn_error_createf
(SVN_ERR_CL_ARG_PARSING_ERROR, NULL,
_("Syntax error in revision argument '%s'"),
utf8_opt_arg);
return svn_cmdline_handle_exit_error(err, pool, "svnadmin: ");
}
}
break;
case 'q':
opt_state.quiet = TRUE;
break;
case 'h':
case '?':
opt_state.help = TRUE;
break;
case svnadmin__version:
opt_state.version = TRUE;
break;
case svnadmin__incremental:
opt_state.incremental = TRUE;
break;
case svnadmin__deltas:
opt_state.use_deltas = TRUE;
break;
case svnadmin__ignore_uuid:
opt_state.uuid_action = svn_repos_load_uuid_ignore;
break;
case svnadmin__force_uuid:
opt_state.uuid_action = svn_repos_load_uuid_force;
break;
case svnadmin__pre_1_4_compatible:
opt_state.pre_1_4_compatible = TRUE;
break;
case svnadmin__pre_1_5_compatible:
opt_state.pre_1_5_compatible = TRUE;
break;
case svnadmin__fs_type:
err = svn_utf_cstring_to_utf8(&opt_state.fs_type, opt_arg, pool);
if (err)
return svn_cmdline_handle_exit_error(err, pool, "svnadmin: ");
break;
case svnadmin__parent_dir:
err = svn_utf_cstring_to_utf8(&opt_state.parent_dir, opt_arg,
pool);
if (err)
return svn_cmdline_handle_exit_error(err, pool, "svnadmin: ");
opt_state.parent_dir
= svn_path_internal_style(opt_state.parent_dir, pool);
break;
case svnadmin__use_pre_commit_hook:
opt_state.use_pre_commit_hook = TRUE;
break;
case svnadmin__use_post_commit_hook:
opt_state.use_post_commit_hook = TRUE;
break;
case svnadmin__use_pre_revprop_change_hook:
opt_state.use_pre_revprop_change_hook = TRUE;
break;
case svnadmin__use_post_revprop_change_hook:
opt_state.use_post_revprop_change_hook = TRUE;
break;
case svnadmin__bdb_txn_nosync:
opt_state.bdb_txn_nosync = TRUE;
break;
case svnadmin__bdb_log_keep:
opt_state.bdb_log_keep = TRUE;
break;
case svnadmin__bypass_hooks:
opt_state.bypass_hooks = TRUE;
break;
case svnadmin__clean_logs:
opt_state.clean_logs = TRUE;
break;
case svnadmin__config_dir:
opt_state.config_dir =
apr_pstrdup(pool, svn_path_canonicalize(opt_arg, pool));
break;
case svnadmin__wait:
opt_state.wait = TRUE;
break;
default: {
subcommand_help(NULL, NULL, pool);
svn_pool_destroy(pool);
return EXIT_FAILURE;
}
}
}
if (opt_state.help)
subcommand = svn_opt_get_canonical_subcommand(cmd_table, "help");
if (subcommand == NULL) {
if (os->ind >= os->argc) {
if (opt_state.version) {
static const svn_opt_subcommand_desc_t pseudo_cmd = {
"--version", subcommand_help, {0}, "",
{
svnadmin__version,
}
};
subcommand = &pseudo_cmd;
} else {
svn_error_clear
(svn_cmdline_fprintf(stderr, pool,
_("subcommand argument required\n")));
subcommand_help(NULL, NULL, pool);
svn_pool_destroy(pool);
return EXIT_FAILURE;
}
} else {
const char *first_arg = os->argv[os->ind++];
subcommand = svn_opt_get_canonical_subcommand(cmd_table, first_arg);
if (subcommand == NULL) {
const char* first_arg_utf8;
err = svn_utf_cstring_to_utf8(&first_arg_utf8, first_arg, pool);
if (err)
return svn_cmdline_handle_exit_error(err, pool, "svnadmin: ");
svn_error_clear
(svn_cmdline_fprintf(stderr, pool,
_("Unknown command: '%s'\n"),
first_arg_utf8));
subcommand_help(NULL, NULL, pool);
svn_pool_destroy(pool);
return EXIT_FAILURE;
}
}
}
if (subcommand->cmd_func != subcommand_help) {
err = parse_local_repos_path(os,
&(opt_state.repository_path),
pool);
if (err)
return svn_cmdline_handle_exit_error(err, pool, "svnadmin: ");
}
if (subcommand->cmd_func == subcommand_hotcopy) {
err = parse_local_repos_path(os,
&(opt_state.new_repository_path),
pool);
if (err)
return svn_cmdline_handle_exit_error(err, pool, "svnadmin: ");
}
for (i = 0; i < received_opts->nelts; i++) {
opt_id = APR_ARRAY_IDX(received_opts, i, int);
if (opt_id == 'h' || opt_id == '?')
continue;
if (! svn_opt_subcommand_takes_option(subcommand, opt_id)) {
const char *optstr;
const apr_getopt_option_t *badopt =
svn_opt_get_option_from_code(opt_id, options_table);
svn_opt_format_option(&optstr, badopt, FALSE, pool);
if (subcommand->name[0] == '-')
subcommand_help(NULL, NULL, pool);
else
svn_error_clear
(svn_cmdline_fprintf
(stderr, pool, _("Subcommand '%s' doesn't accept option '%s'\n"
"Type 'svnadmin help %s' for usage.\n"),
subcommand->name, optstr, subcommand->name));
svn_pool_destroy(pool);
return EXIT_FAILURE;
}
}
setup_cancellation_signals(signal_handler);
#if defined(SIGPIPE)
apr_signal(SIGPIPE, SIG_IGN);
#endif
#if defined(SIGXFSZ)
apr_signal(SIGXFSZ, SIG_IGN);
#endif
err = (*subcommand->cmd_func)(os, &opt_state, pool);
if (err) {
if (err->apr_err == SVN_ERR_CL_INSUFFICIENT_ARGS
|| err->apr_err == SVN_ERR_CL_ARG_PARSING_ERROR) {
err = svn_error_quick_wrap(err,
_("Try 'svnadmin help' for more info"));
}
return svn_cmdline_handle_exit_error(err, pool, "svnadmin: ");
} else {
svn_pool_destroy(pool);
err = svn_cmdline_fflush(stdout);
if (err) {
svn_handle_error2(err, stderr, FALSE, "svnadmin: ");
svn_error_clear(err);
return EXIT_FAILURE;
}
return EXIT_SUCCESS;
}
}
static svn_error_t *
subcommand_crashtest(apr_getopt_t *os, void *baton, apr_pool_t *pool) {
struct svnadmin_opt_state *opt_state = baton;
svn_repos_t *repos;
SVN_ERR(open_repos(&repos, opt_state->repository_path, pool));
abort();
return SVN_NO_ERROR;
}
