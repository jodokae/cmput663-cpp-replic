#include "svn_cmdline.h"
#include "svn_config.h"
#include "svn_pools.h"
#include "svn_delta.h"
#include "svn_path.h"
#include "svn_props.h"
#include "svn_auth.h"
#include "svn_opt.h"
#include "svn_ra.h"
#include "svn_private_config.h"
#include <apr_network_io.h>
#include <apr_signal.h>
#include <apr_uuid.h>
static svn_opt_subcommand_t initialize_cmd,
synchronize_cmd,
copy_revprops_cmd,
help_cmd;
enum {
svnsync_opt_non_interactive = SVN_OPT_FIRST_LONGOPT_ID,
svnsync_opt_no_auth_cache,
svnsync_opt_auth_username,
svnsync_opt_auth_password,
svnsync_opt_source_username,
svnsync_opt_source_password,
svnsync_opt_sync_username,
svnsync_opt_sync_password,
svnsync_opt_config_dir,
svnsync_opt_version
};
#define SVNSYNC_OPTS_DEFAULT svnsync_opt_non_interactive, svnsync_opt_no_auth_cache, svnsync_opt_auth_username, svnsync_opt_auth_password, svnsync_opt_source_username, svnsync_opt_source_password, svnsync_opt_sync_username, svnsync_opt_sync_password, svnsync_opt_config_dir, 'q'
static const svn_opt_subcommand_desc_t svnsync_cmd_table[] = {
{
"initialize", initialize_cmd, { "init" },
N_("usage: svnsync initialize DEST_URL SOURCE_URL\n"
"\n"
"Initialize a destination repository for synchronization from\n"
"another repository.\n"
"\n"
"The destination URL must point to the root of a repository with\n"
"no committed revisions. The destination repository must allow\n"
"revision property changes.\n"
"\n"
"You should not commit to, or make revision property changes in,\n"
"the destination repository by any method other than 'svnsync'.\n"
"In other words, the destination repository should be a read-only\n"
"mirror of the source repository.\n"),
{ SVNSYNC_OPTS_DEFAULT }
},
{
"synchronize", synchronize_cmd, { "sync" },
N_("usage: svnsync synchronize DEST_URL\n"
"\n"
"Transfer all pending revisions to the destination from the source\n"
"with which it was initialized.\n"),
{ SVNSYNC_OPTS_DEFAULT }
},
{
"copy-revprops", copy_revprops_cmd, { 0 },
N_("usage: svnsync copy-revprops DEST_URL [REV[:REV2]]\n"
"\n"
"Copy the revision properties in a given range of revisions to the\n"
"destination from the source with which it was initialized.\n"
"\n"
"If REV and REV2 are provided, copy properties for the revisions\n"
"specified by that range, inclusively. If only REV is provided,\n"
"copy properties for that revision alone. If REV is not provided,\n"
"copy properties for all revisions previously transferred to the\n"
"destination.\n"
"\n"
"REV and REV2 must be revisions which were previously transferred\n"
"to the destination. You may use \"HEAD\" for either revision to\n"
"mean \"the last revision transferred\".\n"),
{ SVNSYNC_OPTS_DEFAULT }
},
{
"help", help_cmd, { "?", "h" },
N_("usage: svnsync help [SUBCOMMAND...]\n"
"\n"
"Describe the usage of this program or its subcommands.\n"),
{ 0 }
},
{ NULL, NULL, { 0 }, NULL, { 0 } }
};
static const apr_getopt_option_t svnsync_options[] = {
{
"quiet", 'q', 0,
N_("print as little as possible")
},
{
"non-interactive", svnsync_opt_non_interactive, 0,
N_("do no interactive prompting")
},
{
"no-auth-cache", svnsync_opt_no_auth_cache, 0,
N_("do not cache authentication tokens")
},
{
"username", svnsync_opt_auth_username, 1,
N_("specify a username ARG (deprecated;\n"
" "
"see --source-username and --sync-username)")
},
{
"password", svnsync_opt_auth_password, 1,
N_("specify a password ARG (deprecated;\n"
" "
"see --source-password and --sync-password)")
},
{
"source-username", svnsync_opt_source_username, 1,
N_("connect to source repository with username ARG")
},
{
"source-password", svnsync_opt_source_password, 1,
N_("connect to source repository with password ARG")
},
{
"sync-username", svnsync_opt_sync_username, 1,
N_("connect to sync repository with username ARG")
},
{
"sync-password", svnsync_opt_sync_password, 1,
N_("connect to sync repository with password ARG")
},
{
"config-dir", svnsync_opt_config_dir, 1,
N_("read user configuration files from directory ARG")
},
{
"version", svnsync_opt_version, 0,
N_("show program version information")
},
{
"help", 'h', 0,
N_("show help on a subcommand")
},
{
NULL, '?', 0,
N_("show help on a subcommand")
},
{ 0, 0, 0, 0 }
};
typedef struct {
svn_boolean_t non_interactive;
svn_boolean_t no_auth_cache;
svn_auth_baton_t *source_auth_baton;
svn_auth_baton_t *sync_auth_baton;
const char *source_username;
const char *source_password;
const char *sync_username;
const char *sync_password;
const char *config_dir;
apr_hash_t *config;
svn_boolean_t quiet;
svn_boolean_t version;
svn_boolean_t help;
} opt_baton_t;
static volatile sig_atomic_t cancelled = FALSE;
static void
signal_handler(int signum) {
apr_signal(signum, SIG_IGN);
cancelled = TRUE;
}
static svn_error_t *
check_cancel(void *baton) {
if (cancelled)
return svn_error_create(SVN_ERR_CANCELLED, NULL, _("Caught signal"));
else
return SVN_NO_ERROR;
}
static svn_error_t *
check_lib_versions(void) {
static const svn_version_checklist_t checklist[] = {
{ "svn_subr", svn_subr_version },
{ "svn_delta", svn_delta_version },
{ "svn_ra", svn_ra_version },
{ NULL, NULL }
};
SVN_VERSION_DEFINE(my_version);
return svn_ver_check_list(&my_version, checklist);
}
static svn_error_t *
get_lock(svn_ra_session_t *session, apr_pool_t *pool) {
char hostname_str[APRMAXHOSTLEN + 1] = { 0 };
svn_string_t *mylocktoken, *reposlocktoken;
apr_status_t apr_err;
apr_pool_t *subpool;
int i;
apr_err = apr_gethostname(hostname_str, sizeof(hostname_str), pool);
if (apr_err)
return svn_error_wrap_apr(apr_err, _("Can't get local hostname"));
mylocktoken = svn_string_createf(pool, "%s:%s", hostname_str,
svn_uuid_generate(pool));
subpool = svn_pool_create(pool);
for (i = 0; i < 10; ++i) {
svn_pool_clear(subpool);
SVN_ERR(check_cancel(NULL));
SVN_ERR(svn_ra_rev_prop(session, 0, SVNSYNC_PROP_LOCK, &reposlocktoken,
subpool));
if (reposlocktoken) {
if (strcmp(reposlocktoken->data, mylocktoken->data) == 0)
return SVN_NO_ERROR;
else {
SVN_ERR(svn_cmdline_printf
(pool, _("Failed to get lock on destination "
"repos, currently held by '%s'\n"),
reposlocktoken->data));
apr_sleep(apr_time_from_sec(1));
}
} else {
SVN_ERR(svn_ra_change_rev_prop(session, 0, SVNSYNC_PROP_LOCK,
mylocktoken, subpool));
}
}
return svn_error_createf(APR_EINVAL, NULL,
"Couldn't get lock on destination repos "
"after %d attempts\n", i);
}
typedef struct {
apr_hash_t *config;
svn_ra_callbacks2_t source_callbacks;
svn_ra_callbacks2_t sync_callbacks;
svn_boolean_t quiet;
const char *to_url;
const char *from_url;
svn_revnum_t committed_rev;
svn_revnum_t start_rev;
svn_revnum_t end_rev;
} subcommand_baton_t;
typedef svn_error_t *(*with_locked_func_t)(svn_ra_session_t *session,
subcommand_baton_t *baton,
apr_pool_t *pool);
static svn_error_t *
with_locked(svn_ra_session_t *session,
with_locked_func_t func,
subcommand_baton_t *baton,
apr_pool_t *pool) {
svn_error_t *err, *err2;
SVN_ERR(get_lock(session, pool));
err = func(session, baton, pool);
err2 = svn_ra_change_rev_prop(session, 0, SVNSYNC_PROP_LOCK, NULL, pool);
if (err2 && err) {
svn_error_clear(err2);
return err;
} else if (err2) {
return err2;
} else {
return err;
}
}
static svn_error_t *
open_tmp_file(apr_file_t **fp, void *callback_baton, apr_pool_t *pool) {
const char *path;
SVN_ERR(svn_io_temp_dir(&path, pool));
path = svn_path_join(path, "tempfile", pool);
SVN_ERR(svn_io_open_unique_file2(fp, NULL, path, ".tmp",
svn_io_file_del_on_close, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
check_if_session_is_at_repos_root(svn_ra_session_t *sess,
const char *url,
apr_pool_t *pool) {
const char *sess_root;
SVN_ERR(svn_ra_get_repos_root2(sess, &sess_root, pool));
if (strcmp(url, sess_root) == 0)
return SVN_NO_ERROR;
else
return svn_error_createf
(APR_EINVAL, NULL,
_("Session is rooted at '%s' but the repos root is '%s'"),
url, sess_root);
}
static svn_error_t *
remove_props_not_in_source(svn_ra_session_t *session,
svn_revnum_t rev,
apr_hash_t *source_props,
apr_hash_t *target_props,
apr_pool_t *pool) {
apr_pool_t *subpool = svn_pool_create(pool);
apr_hash_index_t *hi;
for (hi = apr_hash_first(pool, target_props);
hi;
hi = apr_hash_next(hi)) {
const void *key;
svn_pool_clear(subpool);
apr_hash_this(hi, &key, NULL, NULL);
if (! apr_hash_get(source_props, key, APR_HASH_KEY_STRING))
SVN_ERR(svn_ra_change_rev_prop(session, rev, key, NULL,
subpool));
}
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
typedef svn_boolean_t (*filter_func_t)(const char *key);
static apr_hash_t *
filter_props(int *filtered_count, apr_hash_t *props,
filter_func_t filter,
apr_pool_t *pool) {
apr_hash_index_t *hi;
apr_hash_t *filtered = apr_hash_make(pool);
*filtered_count = 0;
for (hi = apr_hash_first(pool, props); hi ; hi = apr_hash_next(hi)) {
void *val;
const void *key;
apr_ssize_t len;
apr_hash_this(hi, &key, &len, &val);
if (!filter || filter(key) == FALSE) {
apr_hash_set(filtered, key, APR_HASH_KEY_STRING, val);
} else {
*filtered_count += 1;
}
}
return filtered;
}
static svn_error_t *
write_revprops(int *filtered_count,
svn_ra_session_t *session,
svn_revnum_t rev,
apr_hash_t *rev_props,
apr_pool_t *pool) {
apr_pool_t *subpool = svn_pool_create(pool);
apr_hash_index_t *hi;
*filtered_count = 0;
for (hi = apr_hash_first(pool, rev_props); hi; hi = apr_hash_next(hi)) {
const void *key;
void *val;
svn_pool_clear(subpool);
apr_hash_this(hi, &key, NULL, &val);
if (strncmp(key, SVNSYNC_PROP_PREFIX,
sizeof(SVNSYNC_PROP_PREFIX) - 1) != 0) {
SVN_ERR(svn_ra_change_rev_prop(session, rev, key, val, subpool));
} else {
*filtered_count += 1;
}
}
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
static svn_error_t *
log_properties_copied(svn_boolean_t syncprops_found,
svn_revnum_t rev,
apr_pool_t *pool) {
if (syncprops_found)
SVN_ERR(svn_cmdline_printf(pool,
_("Copied properties for revision %ld "
"(%s* properties skipped).\n"),
rev, SVNSYNC_PROP_PREFIX));
else
SVN_ERR(svn_cmdline_printf(pool,
_("Copied properties for revision %ld.\n"),
rev));
return SVN_NO_ERROR;
}
static svn_error_t *
copy_revprops(svn_ra_session_t *from_session,
svn_ra_session_t *to_session,
svn_revnum_t rev,
svn_boolean_t sync,
svn_boolean_t quiet,
apr_pool_t *pool) {
apr_pool_t *subpool = svn_pool_create(pool);
apr_hash_t *existing_props, *rev_props;
int filtered_count = 0;
if (sync)
SVN_ERR(svn_ra_rev_proplist(to_session, rev, &existing_props, subpool));
SVN_ERR(svn_ra_rev_proplist(from_session, rev, &rev_props, subpool));
SVN_ERR(write_revprops(&filtered_count, to_session, rev, rev_props, pool));
if (sync)
SVN_ERR(remove_props_not_in_source(to_session, rev,
rev_props, existing_props, pool));
if (! quiet)
SVN_ERR(log_properties_copied(filtered_count > 0, rev, pool));
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
static subcommand_baton_t *
make_subcommand_baton(opt_baton_t *opt_baton,
const char *to_url,
const char *from_url,
svn_revnum_t start_rev,
svn_revnum_t end_rev,
apr_pool_t *pool) {
subcommand_baton_t *b = apr_pcalloc(pool, sizeof(*b));
b->config = opt_baton->config;
b->source_callbacks.open_tmp_file = open_tmp_file;
b->source_callbacks.auth_baton = opt_baton->source_auth_baton;
b->sync_callbacks.open_tmp_file = open_tmp_file;
b->sync_callbacks.auth_baton = opt_baton->sync_auth_baton;
b->quiet = opt_baton->quiet;
b->to_url = to_url;
b->from_url = from_url;
b->start_rev = start_rev;
b->end_rev = end_rev;
return b;
}
static svn_error_t *
do_initialize(svn_ra_session_t *to_session,
subcommand_baton_t *baton,
apr_pool_t *pool) {
svn_ra_session_t *from_session;
svn_string_t *from_url;
svn_revnum_t latest;
const char *uuid, *root_url;
SVN_ERR(svn_ra_get_latest_revnum(to_session, &latest, pool));
if (latest != 0)
return svn_error_create
(APR_EINVAL, NULL,
_("Cannot initialize a repository with content in it"));
SVN_ERR(svn_ra_rev_prop(to_session, 0, SVNSYNC_PROP_FROM_URL,
&from_url, pool));
if (from_url)
return svn_error_createf
(APR_EINVAL, NULL,
_("Destination repository is already synchronizing from '%s'"),
from_url->data);
SVN_ERR(svn_ra_open3(&from_session, baton->from_url, NULL,
&(baton->source_callbacks), baton,
baton->config, pool));
SVN_ERR(svn_ra_get_repos_root2(from_session, &root_url, pool));
if (svn_path_is_child(root_url, baton->from_url, pool)) {
svn_boolean_t server_supports_partial_replay;
svn_error_t *err = svn_ra_has_capability(from_session,
&server_supports_partial_replay,
SVN_RA_CAPABILITY_PARTIAL_REPLAY,
pool);
if (err && err->apr_err == SVN_ERR_UNKNOWN_CAPABILITY) {
svn_error_clear(err);
server_supports_partial_replay = FALSE;
}
if (!server_supports_partial_replay)
return svn_error_create(SVN_ERR_RA_PARTIAL_REPLAY_NOT_SUPPORTED, NULL,
NULL);
}
SVN_ERR(svn_ra_change_rev_prop(to_session, 0, SVNSYNC_PROP_FROM_URL,
svn_string_create(baton->from_url, pool),
pool));
SVN_ERR(svn_ra_get_uuid2(from_session, &uuid, pool));
SVN_ERR(svn_ra_change_rev_prop(to_session, 0, SVNSYNC_PROP_FROM_UUID,
svn_string_create(uuid, pool), pool));
SVN_ERR(svn_ra_change_rev_prop(to_session, 0, SVNSYNC_PROP_LAST_MERGED_REV,
svn_string_create("0", pool), pool));
SVN_ERR(copy_revprops(from_session, to_session, 0, FALSE,
baton->quiet, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
initialize_cmd(apr_getopt_t *os, void *b, apr_pool_t *pool) {
const char *to_url, *from_url;
svn_ra_session_t *to_session;
opt_baton_t *opt_baton = b;
apr_array_header_t *targets;
subcommand_baton_t *baton;
SVN_ERR(svn_opt_args_to_target_array2(&targets, os,
apr_array_make(pool, 0,
sizeof(const char *)),
pool));
if (targets->nelts < 2)
return svn_error_create(SVN_ERR_CL_INSUFFICIENT_ARGS, 0, NULL);
if (targets->nelts > 2)
return svn_error_create(SVN_ERR_CL_ARG_PARSING_ERROR, 0, NULL);
to_url = APR_ARRAY_IDX(targets, 0, const char *);
from_url = APR_ARRAY_IDX(targets, 1, const char *);
if (! svn_path_is_url(to_url))
return svn_error_createf(SVN_ERR_CL_ARG_PARSING_ERROR, NULL,
_("Path '%s' is not a URL"), to_url);
if (! svn_path_is_url(from_url))
return svn_error_createf(SVN_ERR_CL_ARG_PARSING_ERROR, NULL,
_("Path '%s' is not a URL"), from_url);
baton = make_subcommand_baton(opt_baton, to_url, from_url, 0, 0, pool);
SVN_ERR(svn_ra_open3(&to_session, baton->to_url, NULL,
&(baton->sync_callbacks), baton, baton->config, pool));
SVN_ERR(check_if_session_is_at_repos_root(to_session, baton->to_url, pool));
SVN_ERR(with_locked(to_session, do_initialize, baton, pool));
return SVN_NO_ERROR;
}
typedef struct {
const svn_delta_editor_t *wrapped_editor;
void *wrapped_edit_baton;
const char *to_url;
svn_boolean_t called_open_root;
svn_boolean_t got_textdeltas;
svn_revnum_t base_revision;
svn_boolean_t quiet;
} edit_baton_t;
typedef struct {
void *edit_baton;
void *wrapped_node_baton;
} node_baton_t;
static svn_error_t *
set_target_revision(void *edit_baton,
svn_revnum_t target_revision,
apr_pool_t *pool) {
edit_baton_t *eb = edit_baton;
return eb->wrapped_editor->set_target_revision(eb->wrapped_edit_baton,
target_revision, pool);
}
static svn_error_t *
open_root(void *edit_baton,
svn_revnum_t base_revision,
apr_pool_t *pool,
void **root_baton) {
edit_baton_t *eb = edit_baton;
node_baton_t *dir_baton = apr_palloc(pool, sizeof(*dir_baton));
SVN_ERR(eb->wrapped_editor->open_root(eb->wrapped_edit_baton,
base_revision, pool,
&dir_baton->wrapped_node_baton));
eb->called_open_root = TRUE;
dir_baton->edit_baton = edit_baton;
*root_baton = dir_baton;
return SVN_NO_ERROR;
}
static svn_error_t *
delete_entry(const char *path,
svn_revnum_t base_revision,
void *parent_baton,
apr_pool_t *pool) {
node_baton_t *pb = parent_baton;
edit_baton_t *eb = pb->edit_baton;
return eb->wrapped_editor->delete_entry(path, base_revision,
pb->wrapped_node_baton, pool);
}
static svn_error_t *
add_directory(const char *path,
void *parent_baton,
const char *copyfrom_path,
svn_revnum_t copyfrom_rev,
apr_pool_t *pool,
void **child_baton) {
node_baton_t *pb = parent_baton;
edit_baton_t *eb = pb->edit_baton;
node_baton_t *b = apr_palloc(pool, sizeof(*b));
if (copyfrom_path)
copyfrom_path = apr_psprintf(pool, "%s%s", eb->to_url,
svn_path_uri_encode(copyfrom_path, pool));
SVN_ERR(eb->wrapped_editor->add_directory(path, pb->wrapped_node_baton,
copyfrom_path,
copyfrom_rev, pool,
&b->wrapped_node_baton));
b->edit_baton = eb;
*child_baton = b;
return SVN_NO_ERROR;
}
static svn_error_t *
open_directory(const char *path,
void *parent_baton,
svn_revnum_t base_revision,
apr_pool_t *pool,
void **child_baton) {
node_baton_t *pb = parent_baton;
edit_baton_t *eb = pb->edit_baton;
node_baton_t *db = apr_palloc(pool, sizeof(*db));
SVN_ERR(eb->wrapped_editor->open_directory(path, pb->wrapped_node_baton,
base_revision, pool,
&db->wrapped_node_baton));
db->edit_baton = eb;
*child_baton = db;
return SVN_NO_ERROR;
}
static svn_error_t *
add_file(const char *path,
void *parent_baton,
const char *copyfrom_path,
svn_revnum_t copyfrom_rev,
apr_pool_t *pool,
void **file_baton) {
node_baton_t *pb = parent_baton;
edit_baton_t *eb = pb->edit_baton;
node_baton_t *fb = apr_palloc(pool, sizeof(*fb));
if (copyfrom_path)
copyfrom_path = apr_psprintf(pool, "%s%s", eb->to_url,
svn_path_uri_encode(copyfrom_path, pool));
SVN_ERR(eb->wrapped_editor->add_file(path, pb->wrapped_node_baton,
copyfrom_path, copyfrom_rev,
pool, &fb->wrapped_node_baton));
fb->edit_baton = eb;
*file_baton = fb;
return SVN_NO_ERROR;
}
static svn_error_t *
open_file(const char *path,
void *parent_baton,
svn_revnum_t base_revision,
apr_pool_t *pool,
void **file_baton) {
node_baton_t *pb = parent_baton;
edit_baton_t *eb = pb->edit_baton;
node_baton_t *fb = apr_palloc(pool, sizeof(*fb));
SVN_ERR(eb->wrapped_editor->open_file(path, pb->wrapped_node_baton,
base_revision, pool,
&fb->wrapped_node_baton));
fb->edit_baton = eb;
*file_baton = fb;
return SVN_NO_ERROR;
}
static svn_error_t *
apply_textdelta(void *file_baton,
const char *base_checksum,
apr_pool_t *pool,
svn_txdelta_window_handler_t *handler,
void **handler_baton) {
node_baton_t *fb = file_baton;
edit_baton_t *eb = fb->edit_baton;
if (! eb->quiet) {
if (! eb->got_textdeltas)
SVN_ERR(svn_cmdline_printf(pool, _("Transmitting file data ")));
SVN_ERR(svn_cmdline_printf(pool, "."));
SVN_ERR(svn_cmdline_fflush(stdout));
}
eb->got_textdeltas = TRUE;
return eb->wrapped_editor->apply_textdelta(fb->wrapped_node_baton,
base_checksum, pool,
handler, handler_baton);
}
static svn_error_t *
close_file(void *file_baton,
const char *text_checksum,
apr_pool_t *pool) {
node_baton_t *fb = file_baton;
edit_baton_t *eb = fb->edit_baton;
return eb->wrapped_editor->close_file(fb->wrapped_node_baton,
text_checksum, pool);
}
static svn_error_t *
absent_file(const char *path,
void *file_baton,
apr_pool_t *pool) {
node_baton_t *fb = file_baton;
edit_baton_t *eb = fb->edit_baton;
return eb->wrapped_editor->absent_file(path, fb->wrapped_node_baton, pool);
}
static svn_error_t *
close_directory(void *dir_baton,
apr_pool_t *pool) {
node_baton_t *db = dir_baton;
edit_baton_t *eb = db->edit_baton;
return eb->wrapped_editor->close_directory(db->wrapped_node_baton, pool);
}
static svn_error_t *
absent_directory(const char *path,
void *dir_baton,
apr_pool_t *pool) {
node_baton_t *db = dir_baton;
edit_baton_t *eb = db->edit_baton;
return eb->wrapped_editor->absent_directory(path, db->wrapped_node_baton,
pool);
}
static svn_error_t *
change_file_prop(void *file_baton,
const char *name,
const svn_string_t *value,
apr_pool_t *pool) {
node_baton_t *fb = file_baton;
edit_baton_t *eb = fb->edit_baton;
if (svn_property_kind(NULL, name) != svn_prop_regular_kind)
return SVN_NO_ERROR;
return eb->wrapped_editor->change_file_prop(fb->wrapped_node_baton,
name, value, pool);
}
static svn_error_t *
change_dir_prop(void *dir_baton,
const char *name,
const svn_string_t *value,
apr_pool_t *pool) {
node_baton_t *db = dir_baton;
edit_baton_t *eb = db->edit_baton;
if (svn_property_kind(NULL, name) != svn_prop_regular_kind)
return SVN_NO_ERROR;
return eb->wrapped_editor->change_dir_prop(db->wrapped_node_baton,
name, value, pool);
}
static svn_error_t *
close_edit(void *edit_baton,
apr_pool_t *pool) {
edit_baton_t *eb = edit_baton;
if (! eb->called_open_root) {
void *baton;
SVN_ERR(eb->wrapped_editor->open_root(eb->wrapped_edit_baton,
eb->base_revision, pool,
&baton));
SVN_ERR(eb->wrapped_editor->close_directory(baton, pool));
}
if (! eb->quiet) {
if (eb->got_textdeltas)
SVN_ERR(svn_cmdline_printf(pool, "\n"));
}
return eb->wrapped_editor->close_edit(eb->wrapped_edit_baton, pool);
}
static svn_error_t *
get_sync_editor(const svn_delta_editor_t *wrapped_editor,
void *wrapped_edit_baton,
svn_revnum_t base_revision,
const char *to_url,
svn_boolean_t quiet,
const svn_delta_editor_t **editor,
void **edit_baton,
apr_pool_t *pool) {
svn_delta_editor_t *tree_editor = svn_delta_default_editor(pool);
edit_baton_t *eb = apr_pcalloc(pool, sizeof(*eb));
tree_editor->set_target_revision = set_target_revision;
tree_editor->open_root = open_root;
tree_editor->delete_entry = delete_entry;
tree_editor->add_directory = add_directory;
tree_editor->open_directory = open_directory;
tree_editor->change_dir_prop = change_dir_prop;
tree_editor->close_directory = close_directory;
tree_editor->absent_directory = absent_directory;
tree_editor->add_file = add_file;
tree_editor->open_file = open_file;
tree_editor->apply_textdelta = apply_textdelta;
tree_editor->change_file_prop = change_file_prop;
tree_editor->close_file = close_file;
tree_editor->absent_file = absent_file;
tree_editor->close_edit = close_edit;
eb->wrapped_editor = wrapped_editor;
eb->wrapped_edit_baton = wrapped_edit_baton;
eb->base_revision = base_revision;
eb->to_url = to_url;
eb->quiet = quiet;
*editor = tree_editor;
*edit_baton = eb;
return SVN_NO_ERROR;
}
static svn_error_t *
commit_callback(const svn_commit_info_t *commit_info,
void *baton,
apr_pool_t *pool) {
subcommand_baton_t *sb = baton;
if (! sb->quiet) {
SVN_ERR(svn_cmdline_printf(pool, _("Committed revision %ld.\n"),
commit_info->revision));
}
sb->committed_rev = commit_info->revision;
return SVN_NO_ERROR;
}
static svn_error_t *
open_source_session(svn_ra_session_t **from_session,
svn_string_t **last_merged_rev,
svn_ra_session_t *to_session,
svn_ra_callbacks2_t *callbacks,
apr_hash_t *config,
void *baton,
apr_pool_t *pool) {
svn_string_t *from_url, *from_uuid;
SVN_ERR(svn_ra_rev_prop(to_session, 0, SVNSYNC_PROP_FROM_URL,
&from_url, pool));
SVN_ERR(svn_ra_rev_prop(to_session, 0, SVNSYNC_PROP_FROM_UUID,
&from_uuid, pool));
SVN_ERR(svn_ra_rev_prop(to_session, 0, SVNSYNC_PROP_LAST_MERGED_REV,
last_merged_rev, pool));
if (! from_url || ! from_uuid || ! *last_merged_rev)
return svn_error_create
(APR_EINVAL, NULL,
_("Destination repository has not been initialized"));
SVN_ERR(svn_ra_open3(from_session, from_url->data, from_uuid->data,
callbacks, baton, config, pool));
return SVN_NO_ERROR;
}
typedef struct {
svn_ra_session_t *from_session;
svn_ra_session_t *to_session;
subcommand_baton_t *sb;
svn_boolean_t has_commit_revprops_capability;
} replay_baton_t;
static replay_baton_t *
make_replay_baton(svn_ra_session_t *from_session,
svn_ra_session_t *to_session,
subcommand_baton_t *sb, apr_pool_t *pool) {
replay_baton_t *rb = apr_pcalloc(pool, sizeof(*rb));
rb->from_session = from_session;
rb->to_session = to_session;
rb->sb = sb;
return rb;
}
static svn_boolean_t
filter_exclude_date_author_sync(const char *key) {
if (strcmp(key, SVN_PROP_REVISION_AUTHOR) == 0)
return TRUE;
else if (strcmp(key, SVN_PROP_REVISION_DATE) == 0)
return TRUE;
else if (strncmp(key, SVNSYNC_PROP_PREFIX,
sizeof(SVNSYNC_PROP_PREFIX) - 1) == 0)
return TRUE;
return FALSE;
}
static svn_boolean_t
filter_include_date_author_sync(const char *key) {
return ! filter_exclude_date_author_sync(key);
}
static svn_boolean_t
filter_exclude_log(const char *key) {
if (strcmp(key, SVN_PROP_REVISION_LOG) == 0)
return TRUE;
else
return FALSE;
}
static svn_boolean_t
filter_include_log(const char *key) {
return ! filter_exclude_log(key);
}
static svn_error_t *
replay_rev_started(svn_revnum_t revision,
void *replay_baton,
const svn_delta_editor_t **editor,
void **edit_baton,
apr_hash_t *rev_props,
apr_pool_t *pool) {
const svn_delta_editor_t *commit_editor;
const svn_delta_editor_t *cancel_editor;
const svn_delta_editor_t *sync_editor;
void *commit_baton;
void *cancel_baton;
void *sync_baton;
replay_baton_t *rb = replay_baton;
apr_hash_t *filtered;
int filtered_count;
SVN_ERR(svn_ra_change_rev_prop(rb->to_session, 0,
SVNSYNC_PROP_CURRENTLY_COPYING,
svn_string_createf(pool, "%ld",
revision),
pool));
filtered = filter_props(&filtered_count, rev_props,
(rb->has_commit_revprops_capability
? filter_exclude_date_author_sync
: filter_include_log),
pool);
if (! apr_hash_get(filtered, SVN_PROP_REVISION_LOG, APR_HASH_KEY_STRING))
apr_hash_set(filtered, SVN_PROP_REVISION_LOG, APR_HASH_KEY_STRING,
svn_string_create("", pool));
SVN_ERR(svn_ra_get_commit_editor3(rb->to_session, &commit_editor,
&commit_baton,
filtered,
commit_callback, rb->sb,
NULL, FALSE, pool));
SVN_ERR(get_sync_editor(commit_editor, commit_baton, revision - 1,
rb->sb->to_url, rb->sb->quiet,
&sync_editor, &sync_baton, pool));
SVN_ERR(svn_delta_get_cancellation_editor(check_cancel, NULL,
sync_editor, sync_baton,
&cancel_editor,
&cancel_baton,
pool));
*editor = cancel_editor;
*edit_baton = cancel_baton;
return SVN_NO_ERROR;
}
static svn_error_t *
replay_rev_finished(svn_revnum_t revision,
void *replay_baton,
const svn_delta_editor_t *editor,
void *edit_baton,
apr_hash_t *rev_props,
apr_pool_t *pool) {
apr_pool_t *subpool = svn_pool_create(pool);
replay_baton_t *rb = replay_baton;
apr_hash_t *filtered, *existing_props;
int filtered_count;
SVN_ERR(editor->close_edit(edit_baton, pool));
if (rb->sb->committed_rev != revision)
return svn_error_createf
(APR_EINVAL, NULL,
_("Commit created rev %ld but should have created %ld"),
rb->sb->committed_rev, revision);
SVN_ERR(svn_ra_rev_proplist(rb->to_session, revision, &existing_props,
subpool));
filtered = filter_props(&filtered_count, rev_props,
(rb->has_commit_revprops_capability
? filter_include_date_author_sync
: filter_exclude_log),
subpool);
SVN_ERR(write_revprops(&filtered_count, rb->to_session, revision, filtered,
subpool));
SVN_ERR(remove_props_not_in_source(rb->to_session, revision,
rev_props, existing_props, subpool));
svn_pool_clear(subpool);
SVN_ERR(svn_ra_change_rev_prop
(rb->to_session,
0,
SVNSYNC_PROP_LAST_MERGED_REV,
svn_string_create(apr_psprintf(pool, "%ld", revision),
subpool),
subpool));
SVN_ERR(svn_ra_change_rev_prop(rb->to_session, 0,
SVNSYNC_PROP_CURRENTLY_COPYING,
NULL, subpool));
if (! rb->sb->quiet)
SVN_ERR(log_properties_copied(filtered_count > 0, revision, subpool));
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
static svn_error_t *
do_synchronize(svn_ra_session_t *to_session,
subcommand_baton_t *baton, apr_pool_t *pool) {
svn_string_t *last_merged_rev;
svn_revnum_t from_latest;
svn_ra_session_t *from_session;
svn_string_t *currently_copying;
svn_revnum_t to_latest, copying, last_merged;
svn_revnum_t start_revision, end_revision;
replay_baton_t *rb;
SVN_ERR(open_source_session(&from_session,
&last_merged_rev, to_session,
&(baton->source_callbacks), baton->config,
baton, pool));
SVN_ERR(svn_ra_rev_prop(to_session, 0, SVNSYNC_PROP_CURRENTLY_COPYING,
&currently_copying, pool));
SVN_ERR(svn_ra_get_latest_revnum(to_session, &to_latest, pool));
last_merged = SVN_STR_TO_REV(last_merged_rev->data);
if (currently_copying) {
copying = SVN_STR_TO_REV(currently_copying->data);
if ((copying < last_merged)
|| (copying > (last_merged + 1))
|| ((to_latest != last_merged) && (to_latest != copying))) {
return svn_error_createf
(APR_EINVAL, NULL,
_("Revision being currently copied (%ld), last merged revision "
"(%ld), and destination HEAD (%ld) are inconsistent; have you "
"committed to the destination without using svnsync?"),
copying, last_merged, to_latest);
} else if (copying == to_latest) {
if (copying > last_merged) {
SVN_ERR(copy_revprops(from_session, to_session,
to_latest, TRUE, baton->quiet,
pool));
last_merged = copying;
last_merged_rev = svn_string_create
(apr_psprintf(pool, "%ld", last_merged), pool);
}
SVN_ERR(svn_ra_change_rev_prop(to_session, 0,
SVNSYNC_PROP_LAST_MERGED_REV,
last_merged_rev, pool));
SVN_ERR(svn_ra_change_rev_prop(to_session, 0,
SVNSYNC_PROP_CURRENTLY_COPYING,
NULL, pool));
}
} else {
if (to_latest != last_merged)
return svn_error_createf(APR_EINVAL, NULL,
_("Destination HEAD (%ld) is not the last "
"merged revision (%ld); have you "
"committed to the destination without "
"using svnsync?"),
to_latest, last_merged);
}
SVN_ERR(svn_ra_get_latest_revnum(from_session, &from_latest, pool));
if (from_latest < atol(last_merged_rev->data))
return SVN_NO_ERROR;
rb = make_replay_baton(from_session, to_session, baton, pool);
SVN_ERR(svn_ra_has_capability(rb->to_session,
&rb->has_commit_revprops_capability,
SVN_RA_CAPABILITY_COMMIT_REVPROPS,
pool));
start_revision = atol(last_merged_rev->data) + 1;
end_revision = from_latest;
SVN_ERR(check_cancel(NULL));
SVN_ERR(svn_ra_replay_range(from_session, start_revision, end_revision,
0, TRUE, replay_rev_started,
replay_rev_finished, rb, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
synchronize_cmd(apr_getopt_t *os, void *b, apr_pool_t *pool) {
svn_ra_session_t *to_session;
opt_baton_t *opt_baton = b;
apr_array_header_t *targets;
subcommand_baton_t *baton;
const char *to_url;
SVN_ERR(svn_opt_args_to_target_array2(&targets, os,
apr_array_make(pool, 0,
sizeof(const char *)),
pool));
if (targets->nelts < 1)
return svn_error_create(SVN_ERR_CL_INSUFFICIENT_ARGS, 0, NULL);
if (targets->nelts > 1)
return svn_error_create(SVN_ERR_CL_ARG_PARSING_ERROR, 0, NULL);
to_url = APR_ARRAY_IDX(targets, 0, const char *);
if (! svn_path_is_url(to_url))
return svn_error_createf(SVN_ERR_CL_ARG_PARSING_ERROR, NULL,
_("Path '%s' is not a URL"), to_url);
baton = make_subcommand_baton(opt_baton, to_url, NULL, 0, 0, pool);
SVN_ERR(svn_ra_open3(&to_session, baton->to_url, NULL,
&(baton->sync_callbacks), baton, baton->config, pool));
SVN_ERR(check_if_session_is_at_repos_root(to_session, baton->to_url, pool));
SVN_ERR(with_locked(to_session, do_synchronize, baton, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
do_copy_revprops(svn_ra_session_t *to_session,
subcommand_baton_t *baton, apr_pool_t *pool) {
svn_ra_session_t *from_session;
svn_string_t *last_merged_rev;
svn_revnum_t i;
svn_revnum_t step = 1;
SVN_ERR(open_source_session(&from_session, &last_merged_rev,
to_session,
&(baton->source_callbacks), baton->config,
baton, pool));
if (! SVN_IS_VALID_REVNUM(baton->start_rev))
baton->start_rev = SVN_STR_TO_REV(last_merged_rev->data);
if (! SVN_IS_VALID_REVNUM(baton->end_rev))
baton->end_rev = SVN_STR_TO_REV(last_merged_rev->data);
if (baton->start_rev > SVN_STR_TO_REV(last_merged_rev->data))
return svn_error_createf
(APR_EINVAL, NULL,
_("Cannot copy revprops for a revision (%ld) that has not "
"been synchronized yet"), baton->start_rev);
if (baton->end_rev > SVN_STR_TO_REV(last_merged_rev->data))
return svn_error_createf
(APR_EINVAL, NULL,
_("Cannot copy revprops for a revision (%ld) that has not "
"been synchronized yet"), baton->end_rev);
step = (baton->start_rev > baton->end_rev) ? -1 : 1;
for (i = baton->start_rev; i != baton->end_rev + step; i = i + step) {
SVN_ERR(check_cancel(NULL));
SVN_ERR(copy_revprops(from_session, to_session, i, FALSE,
baton->quiet, pool));
}
return SVN_NO_ERROR;
}
static svn_error_t *
copy_revprops_cmd(apr_getopt_t *os, void *b, apr_pool_t *pool) {
svn_ra_session_t *to_session;
opt_baton_t *opt_baton = b;
apr_array_header_t *targets;
subcommand_baton_t *baton;
const char *to_url;
svn_opt_revision_t start_revision, end_revision;
svn_revnum_t start_rev = 0, end_rev = SVN_INVALID_REVNUM;
if (os->argc - os->ind > 2)
return svn_error_create(SVN_ERR_CL_ARG_PARSING_ERROR, 0, NULL);
if (os->argc - os->ind < 1)
return svn_error_create(SVN_ERR_CL_INSUFFICIENT_ARGS, 0, NULL);
if (os->argc - os->ind == 2) {
const char *rev_str = os->argv[--(os->argc)];
start_revision.kind = svn_opt_revision_unspecified;
end_revision.kind = svn_opt_revision_unspecified;
if ((svn_opt_parse_revision(&start_revision, &end_revision,
rev_str, pool) != 0)
|| ((start_revision.kind != svn_opt_revision_number)
&& (start_revision.kind != svn_opt_revision_head))
|| ((end_revision.kind != svn_opt_revision_number)
&& (end_revision.kind != svn_opt_revision_head)
&& (end_revision.kind != svn_opt_revision_unspecified)))
return svn_error_createf(SVN_ERR_CL_ARG_PARSING_ERROR, NULL,
_("'%s' is not a valid revision range"),
rev_str);
if (start_revision.kind == svn_opt_revision_head) {
start_rev = SVN_INVALID_REVNUM;
} else {
start_rev = start_revision.value.number;
if (! SVN_IS_VALID_REVNUM(start_rev))
return svn_error_createf(SVN_ERR_CL_ARG_PARSING_ERROR, NULL,
_("Invalid revision number (%ld)"),
start_rev);
}
if (end_revision.kind == svn_opt_revision_unspecified) {
end_rev = start_rev;
} else if (end_revision.kind == svn_opt_revision_head) {
end_rev = SVN_INVALID_REVNUM;
} else {
end_rev = end_revision.value.number;
if (! SVN_IS_VALID_REVNUM(end_rev))
return svn_error_createf(SVN_ERR_CL_ARG_PARSING_ERROR, NULL,
_("Invalid revision number (%ld)"),
end_rev);
}
}
SVN_ERR(svn_opt_args_to_target_array2(&targets, os,
apr_array_make(pool, 1,
sizeof(const char *)),
pool));
if (targets->nelts != 1)
return svn_error_create(SVN_ERR_CL_INSUFFICIENT_ARGS, 0, NULL);
to_url = APR_ARRAY_IDX(targets, 0, const char *);
if (! svn_path_is_url(to_url))
return svn_error_createf(SVN_ERR_CL_ARG_PARSING_ERROR, NULL,
_("Path '%s' is not a URL"), to_url);
baton = make_subcommand_baton(opt_baton, to_url, NULL,
start_rev, end_rev, pool);
SVN_ERR(svn_ra_open3(&to_session, baton->to_url, NULL,
&(baton->sync_callbacks), baton, baton->config, pool));
SVN_ERR(check_if_session_is_at_repos_root(to_session, baton->to_url, pool));
SVN_ERR(with_locked(to_session, do_copy_revprops, baton, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
help_cmd(apr_getopt_t *os, void *baton, apr_pool_t *pool) {
opt_baton_t *opt_baton = baton;
const char *header =
_("general usage: svnsync SUBCOMMAND DEST_URL [ARGS & OPTIONS ...]\n"
"Type 'svnsync help <subcommand>' for help on a specific subcommand.\n"
"Type 'svnsync --version' to see the program version and RA modules.\n"
"\n"
"Available subcommands:\n");
const char *ra_desc_start
= _("The following repository access (RA) modules are available:\n\n");
svn_stringbuf_t *version_footer = svn_stringbuf_create(ra_desc_start,
pool);
SVN_ERR(svn_ra_print_modules(version_footer, pool));
SVN_ERR(svn_opt_print_help(os, "svnsync",
opt_baton ? opt_baton->version : FALSE,
FALSE, version_footer->data, header,
svnsync_cmd_table, svnsync_options, NULL,
pool));
return SVN_NO_ERROR;
}
int
main(int argc, const char *argv[]) {
const svn_opt_subcommand_desc_t *subcommand = NULL;
apr_array_header_t *received_opts;
opt_baton_t opt_baton;
svn_config_t *config;
apr_status_t apr_err;
apr_getopt_t *os;
apr_pool_t *pool;
svn_error_t *err;
int opt_id, i;
const char *username = NULL, *source_username = NULL, *sync_username = NULL;
const char *password = NULL, *source_password = NULL, *sync_password = NULL;
if (svn_cmdline_init("svnsync", stderr) != EXIT_SUCCESS) {
return EXIT_FAILURE;
}
err = check_lib_versions();
if (err)
return svn_cmdline_handle_exit_error(err, NULL, "svnsync: ");
pool = svn_pool_create(NULL);
err = svn_ra_initialize(pool);
if (err)
return svn_cmdline_handle_exit_error(err, pool, "svnsync: ");
memset(&opt_baton, 0, sizeof(opt_baton));
received_opts = apr_array_make(pool, SVN_OPT_MAX_OPTIONS, sizeof(int));
if (argc <= 1) {
help_cmd(NULL, NULL, pool);
svn_pool_destroy(pool);
return EXIT_FAILURE;
}
err = svn_cmdline__getopt_init(&os, argc, argv, pool);
if (err)
return svn_cmdline_handle_exit_error(err, pool, "svnsync: ");
os->interleave = 1;
for (;;) {
const char *opt_arg;
apr_err = apr_getopt_long(os, svnsync_options, &opt_id, &opt_arg);
if (APR_STATUS_IS_EOF(apr_err))
break;
else if (apr_err) {
help_cmd(NULL, NULL, pool);
svn_pool_destroy(pool);
return EXIT_FAILURE;
}
APR_ARRAY_PUSH(received_opts, int) = opt_id;
switch (opt_id) {
case svnsync_opt_non_interactive:
opt_baton.non_interactive = TRUE;
break;
case svnsync_opt_no_auth_cache:
opt_baton.no_auth_cache = TRUE;
break;
case svnsync_opt_auth_username:
username = opt_arg;
break;
case svnsync_opt_auth_password:
password = opt_arg;
break;
case svnsync_opt_source_username:
source_username = opt_arg;
break;
case svnsync_opt_source_password:
source_password = opt_arg;
break;
case svnsync_opt_sync_username:
sync_username = opt_arg;
break;
case svnsync_opt_sync_password:
sync_password = opt_arg;
break;
case svnsync_opt_config_dir:
opt_baton.config_dir = opt_arg;
break;
case svnsync_opt_version:
opt_baton.version = TRUE;
break;
case 'q':
opt_baton.quiet = TRUE;
break;
case '?':
case 'h':
opt_baton.help = TRUE;
break;
default: {
help_cmd(NULL, NULL, pool);
svn_pool_destroy(pool);
return EXIT_FAILURE;
}
}
}
if (opt_baton.help)
subcommand = svn_opt_get_canonical_subcommand(svnsync_cmd_table, "help");
if ((username || password)
&& (source_username || sync_username
|| source_password || sync_password)) {
err = svn_error_create
(SVN_ERR_CL_ARG_PARSING_ERROR, NULL,
_("Cannot use --username or --password with any of "
"--source-username, --source-password, --sync-username, "
"or --sync-password.\n"));
return svn_cmdline_handle_exit_error(err, pool, "svnsync: ");
}
if (username) {
source_username = username;
sync_username = username;
}
if (password) {
source_password = password;
sync_password = password;
}
opt_baton.source_username = source_username;
opt_baton.source_password = source_password;
opt_baton.sync_username = sync_username;
opt_baton.sync_password = sync_password;
err = svn_config_ensure(opt_baton.config_dir, pool);
if (err)
return svn_cmdline_handle_exit_error(err, pool, "synsync: ");
if (subcommand == NULL) {
if (os->ind >= os->argc) {
if (opt_baton.version) {
static const svn_opt_subcommand_desc_t pseudo_cmd = {
"--version", help_cmd, {0}, "",
{
svnsync_opt_version,
}
};
subcommand = &pseudo_cmd;
} else {
help_cmd(NULL, NULL, pool);
svn_pool_destroy(pool);
return EXIT_FAILURE;
}
} else {
const char *first_arg = os->argv[os->ind++];
subcommand = svn_opt_get_canonical_subcommand(svnsync_cmd_table,
first_arg);
if (subcommand == NULL) {
help_cmd(NULL, NULL, pool);
svn_pool_destroy(pool);
return EXIT_FAILURE;
}
}
}
for (i = 0; i < received_opts->nelts; ++i) {
opt_id = APR_ARRAY_IDX(received_opts, i, int);
if (opt_id == 'h' || opt_id == '?')
continue;
if (! svn_opt_subcommand_takes_option(subcommand, opt_id)) {
const char *optstr;
const apr_getopt_option_t *badopt =
svn_opt_get_option_from_code(opt_id, svnsync_options);
svn_opt_format_option(&optstr, badopt, FALSE, pool);
if (subcommand->name[0] == '-') {
help_cmd(NULL, NULL, pool);
} else {
err = svn_error_createf
(SVN_ERR_CL_ARG_PARSING_ERROR, NULL,
_("Subcommand '%s' doesn't accept option '%s'\n"
"Type 'svnsync help %s' for usage.\n"),
subcommand->name, optstr, subcommand->name);
return svn_cmdline_handle_exit_error(err, pool, "svnsync: ");
}
}
}
err = svn_config_get_config(&opt_baton.config, opt_baton.config_dir, pool);
if (err)
return svn_cmdline_handle_exit_error(err, pool, "svnsync: ");
config = apr_hash_get(opt_baton.config, SVN_CONFIG_CATEGORY_CONFIG,
APR_HASH_KEY_STRING);
apr_signal(SIGINT, signal_handler);
#if defined(SIGBREAK)
apr_signal(SIGBREAK, signal_handler);
#endif
#if defined(SIGHUP)
apr_signal(SIGHUP, signal_handler);
#endif
#if defined(SIGTERM)
apr_signal(SIGTERM, signal_handler);
#endif
#if defined(SIGPIPE)
apr_signal(SIGPIPE, SIG_IGN);
#endif
#if defined(SIGXFSZ)
apr_signal(SIGXFSZ, SIG_IGN);
#endif
err = svn_cmdline_setup_auth_baton(&opt_baton.source_auth_baton,
opt_baton.non_interactive,
opt_baton.source_username,
opt_baton.source_password,
opt_baton.config_dir,
opt_baton.no_auth_cache,
config,
check_cancel, NULL,
pool);
if (! err)
err = svn_cmdline_setup_auth_baton(&opt_baton.sync_auth_baton,
opt_baton.non_interactive,
opt_baton.sync_username,
opt_baton.sync_password,
opt_baton.config_dir,
opt_baton.no_auth_cache,
config,
check_cancel, NULL,
pool);
if (! err)
err = (*subcommand->cmd_func)(os, &opt_baton, pool);
if (err) {
if (err->apr_err == SVN_ERR_CL_INSUFFICIENT_ARGS
|| err->apr_err == SVN_ERR_CL_ARG_PARSING_ERROR) {
err = svn_error_quick_wrap(err,
_("Try 'svnsync help' for more info"));
}
return svn_cmdline_handle_exit_error(err, pool, "svnsync: ");
}
svn_pool_destroy(pool);
return EXIT_SUCCESS;
}
