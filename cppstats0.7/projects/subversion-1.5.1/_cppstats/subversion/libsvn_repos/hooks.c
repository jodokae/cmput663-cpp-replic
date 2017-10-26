#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <apr_pools.h>
#include <apr_file_io.h>
#if defined(AS400)
#include <apr_portable.h>
#include <spawn.h>
#include <fcntl.h>
#endif
#include "svn_error.h"
#include "svn_path.h"
#include "svn_repos.h"
#include "svn_utf.h"
#include "repos.h"
#include "svn_private_config.h"
#if !defined(AS400)
static svn_error_t *
check_hook_result(const char *name, const char *cmd, apr_proc_t *cmd_proc,
apr_file_t *read_errhandle, apr_pool_t *pool) {
svn_error_t *err, *err2;
svn_stringbuf_t *native_stderr, *failure_message;
const char *utf8_stderr;
int exitcode;
apr_exit_why_e exitwhy;
err2 = svn_stringbuf_from_aprfile(&native_stderr, read_errhandle, pool);
err = svn_io_wait_for_cmd(cmd_proc, cmd, &exitcode, &exitwhy, pool);
if (err) {
svn_error_clear(err2);
return err;
}
if (APR_PROC_CHECK_EXIT(exitwhy) && exitcode == 0) {
if (err2) {
return svn_error_createf
(SVN_ERR_REPOS_HOOK_FAILURE, err2,
_("'%s' hook succeeded, but error output could not be read"),
name);
}
return SVN_NO_ERROR;
}
if (!err2) {
err2 = svn_utf_cstring_to_utf8(&utf8_stderr, native_stderr->data, pool);
if (err2)
utf8_stderr = _("[Error output could not be translated from the "
"native locale to UTF-8.]");
} else {
utf8_stderr = _("[Error output could not be read.]");
}
svn_error_clear(err2);
if (!APR_PROC_CHECK_EXIT(exitwhy)) {
failure_message = svn_stringbuf_createf(pool,
_("'%s' hook failed (did not exit cleanly: "
"apr_exit_why_e was %d, exitcode was %d). "),
name, exitwhy, exitcode);
} else {
const char *action;
if (strcmp(name, "start-commit") == 0
|| strcmp(name, "pre-commit") == 0)
action = _("Commit");
else if (strcmp(name, "pre-revprop-change") == 0)
action = _("Revprop change");
else if (strcmp(name, "pre-lock") == 0)
action = _("Lock");
else if (strcmp(name, "pre-unlock") == 0)
action = _("Unlock");
else
action = NULL;
if (action == NULL)
failure_message = svn_stringbuf_createf(
pool, _("%s hook failed (exit code %d)"),
name, exitcode);
else
failure_message = svn_stringbuf_createf(
pool, _("%s blocked by %s hook (exit code %d)"),
action, name, exitcode);
}
if (utf8_stderr[0]) {
svn_stringbuf_appendcstr(failure_message,
_(" with output:\n"));
svn_stringbuf_appendcstr(failure_message, utf8_stderr);
} else {
svn_stringbuf_appendcstr(failure_message,
_(" with no output."));
}
return svn_error_create(SVN_ERR_REPOS_HOOK_FAILURE, err,
failure_message->data);
}
#endif
static svn_error_t *
run_hook_cmd(const char *name,
const char *cmd,
const char **args,
apr_file_t *stdin_handle,
apr_pool_t *pool)
#if !defined(AS400)
{
apr_file_t *read_errhandle, *write_errhandle, *null_handle;
apr_status_t apr_err;
svn_error_t *err;
apr_proc_t cmd_proc;
apr_err = apr_file_pipe_create(&read_errhandle, &write_errhandle, pool);
if (apr_err)
return svn_error_wrap_apr
(apr_err, _("Can't create pipe for hook '%s'"), cmd);
apr_err = apr_file_inherit_unset(read_errhandle);
if (apr_err)
return svn_error_wrap_apr
(apr_err, _("Can't make pipe read handle non-inherited for hook '%s'"),
cmd);
apr_err = apr_file_inherit_unset(write_errhandle);
if (apr_err)
return svn_error_wrap_apr
(apr_err, _("Can't make pipe write handle non-inherited for hook '%s'"),
cmd);
apr_err = apr_file_open(&null_handle, SVN_NULL_DEVICE_NAME, APR_WRITE,
APR_OS_DEFAULT, pool);
if (apr_err)
return svn_error_wrap_apr
(apr_err, _("Can't create null stdout for hook '%s'"), cmd);
err = svn_io_start_cmd(&cmd_proc, ".", cmd, args, FALSE,
stdin_handle, null_handle, write_errhandle, pool);
apr_err = apr_file_close(write_errhandle);
if (!err && apr_err)
return svn_error_wrap_apr
(apr_err, _("Error closing write end of stderr pipe"));
if (err) {
err = svn_error_createf
(SVN_ERR_REPOS_HOOK_FAILURE, err, _("Failed to start '%s' hook"), cmd);
} else {
err = check_hook_result(name, cmd, &cmd_proc, read_errhandle, pool);
}
apr_err = apr_file_close(read_errhandle);
if (!err && apr_err)
return svn_error_wrap_apr
(apr_err, _("Error closing read end of stderr pipe"));
apr_err = apr_file_close(null_handle);
if (!err && apr_err)
return svn_error_wrap_apr(apr_err, _("Error closing null file"));
return err;
}
#else
#define AS400_BUFFER_SIZE 256
{
const char **native_args;
int fd_map[3], stderr_pipe[2], exitcode;
svn_stringbuf_t *script_output;
pid_t child_pid, wait_rv;
apr_size_t args_arr_size = 0, i;
struct inheritance xmp_inherit = {0};
#pragma convert(0)
char *xmp_envp[2] = {"QIBM_USE_DESCRIPTOR_STDIO=Y", NULL};
const char *dev_null_ebcdic = SVN_NULL_DEVICE_NAME;
#pragma convert(1208)
while (args[args_arr_size] != NULL)
args_arr_size++;
native_args = apr_palloc(pool, sizeof(char *) * args_arr_size + 1);
for (i = 0; args[i] != NULL; i++) {
SVN_ERR(svn_utf_cstring_from_utf8_ex2((const char**)(&(native_args[i])),
args[i], (const char *)0,
pool));
}
native_args[args_arr_size] = NULL;
if (stdin_handle) {
if (apr_os_file_get(&fd_map[0], stdin_handle)) {
return svn_error_createf(SVN_ERR_REPOS_HOOK_FAILURE, NULL,
"Error converting APR file to OS400 "
"type for hook script '%s'", cmd);
}
} else {
fd_map[0] = open(dev_null_ebcdic, O_RDONLY);
if (fd_map[0] == -1)
return svn_error_createf(SVN_ERR_REPOS_HOOK_FAILURE, NULL,
"Error opening /dev/null for hook "
"script '%s'", cmd);
}
fd_map[1] = open(dev_null_ebcdic, O_WRONLY);
if (fd_map[1] == -1)
return svn_error_createf(SVN_ERR_REPOS_HOOK_FAILURE, NULL,
"Error opening /dev/null for hook script '%s'",
cmd);
if (pipe(stderr_pipe) != 0) {
return svn_error_createf(SVN_ERR_REPOS_HOOK_FAILURE, NULL,
"Can't create stderr pipe for "
"hook '%s'", cmd);
}
fd_map[2] = stderr_pipe[1];
child_pid = spawn(native_args[0], 3, fd_map, &xmp_inherit, native_args,
xmp_envp);
if (child_pid == -1) {
return svn_error_createf(SVN_ERR_REPOS_HOOK_FAILURE, NULL,
"Error spawning process for hook script '%s'",
cmd);
}
if (close(fd_map[1]) == -1)
return svn_error_createf(SVN_ERR_REPOS_HOOK_FAILURE, NULL,
"Error closing write end of stdout pipe to "
"hook script '%s'", cmd);
if (close(fd_map[2]) == -1)
return svn_error_createf(SVN_ERR_REPOS_HOOK_FAILURE, NULL,
"Error closing write end of stderr pipe to "
"hook script '%s'", cmd);
script_output = svn_stringbuf_create("", pool);
while (1) {
int rc;
svn_stringbuf_ensure(script_output,
script_output->len + AS400_BUFFER_SIZE + 1);
rc = read(stderr_pipe[0],
&(script_output->data[script_output->len]),
AS400_BUFFER_SIZE);
if (rc == -1) {
return svn_error_createf(SVN_ERR_REPOS_HOOK_FAILURE, NULL,
"Error reading stderr of hook "
"script '%s'", cmd);
}
script_output->len += rc;
if (rc == 0) {
script_output->data[script_output->len] = '\0';
break;
}
}
if (close(stderr_pipe[0]) == -1)
return svn_error_createf(SVN_ERR_REPOS_HOOK_FAILURE, NULL,
"Error closing read end of stderr "
"pipe to hook script '%s'", cmd);
wait_rv = waitpid(child_pid, &exitcode, 0);
if (wait_rv == -1) {
return svn_error_createf(SVN_ERR_REPOS_HOOK_FAILURE, NULL,
"Error waiting for process completion of "
"hook script '%s'", cmd);
}
if (WIFEXITED(exitcode)) {
if (WEXITSTATUS(exitcode)) {
svn_error_t *err;
const char *utf8_stderr = NULL;
svn_stringbuf_t *failure_message = svn_stringbuf_createf(
pool, "'%s' hook failed (exited with a non-zero exitcode "
"of %d). ", name, exitcode);
if (!svn_stringbuf_isempty(script_output)) {
err = svn_utf_cstring_to_utf8_ex2(&utf8_stderr,
script_output->data,
(const char*)0, pool);
if (err) {
utf8_stderr = "[Error output could not be translated from "
"the native locale to UTF-8.]";
svn_error_clear(err);
}
}
if (utf8_stderr) {
svn_stringbuf_appendcstr(failure_message,
"The following error output was "
"produced by the hook:\n");
svn_stringbuf_appendcstr(failure_message, utf8_stderr);
} else {
svn_stringbuf_appendcstr(failure_message,
"No error output was produced by "
"the hook.");
}
return svn_error_create(SVN_ERR_REPOS_HOOK_FAILURE, NULL,
failure_message->data);
} else
return SVN_NO_ERROR;
} else if (WIFSIGNALED(exitcode)) {
return svn_error_createf(SVN_ERR_REPOS_HOOK_FAILURE, NULL,
"Process '%s' failed because of an "
"uncaught terminating signal", cmd);
} else if (WIFEXCEPTION(exitcode)) {
return svn_error_createf(SVN_ERR_REPOS_HOOK_FAILURE, NULL,
"Process '%s' failed unexpectedly with "
"OS400 exception %d", cmd,
WEXCEPTNUMBER(exitcode));
} else if (WIFSTOPPED(exitcode)) {
return svn_error_createf(SVN_ERR_REPOS_HOOK_FAILURE, NULL,
"Process '%s' stopped unexpectedly by "
"signal %d", cmd, WSTOPSIG(exitcode));
} else {
return svn_error_createf(SVN_ERR_REPOS_HOOK_FAILURE, NULL,
"Process '%s' failed unexpectedly", cmd);
}
}
#endif
static svn_error_t *
create_temp_file(apr_file_t **f, const svn_string_t *value, apr_pool_t *pool) {
const char *dir;
apr_off_t offset = 0;
SVN_ERR(svn_io_temp_dir(&dir, pool));
SVN_ERR(svn_io_open_unique_file2(f, NULL,
svn_path_join(dir, "hook-input", pool),
"", svn_io_file_del_on_close, pool));
SVN_ERR(svn_io_file_write_full(*f, value->data, value->len, NULL, pool));
SVN_ERR(svn_io_file_seek(*f, APR_SET, &offset, pool));
return SVN_NO_ERROR;
}
static const char*
check_hook_cmd(const char *hook, svn_boolean_t *broken_link, apr_pool_t *pool) {
static const char* const check_extns[] = {
#if defined(WIN32)
".exe", ".cmd", ".bat", ".wsf",
#else
"",
#endif
NULL
};
const char *const *extn;
svn_error_t *err = NULL;
svn_boolean_t is_special;
for (extn = check_extns; *extn; ++extn) {
const char *const hook_path =
(**extn ? apr_pstrcat(pool, hook, *extn, 0) : hook);
svn_node_kind_t kind;
if (!(err = svn_io_check_resolved_path(hook_path, &kind, pool))
&& kind == svn_node_file) {
*broken_link = FALSE;
return hook_path;
}
svn_error_clear(err);
if (!(err = svn_io_check_special_path(hook_path, &kind, &is_special,
pool))
&& is_special == TRUE) {
*broken_link = TRUE;
return hook_path;
}
svn_error_clear(err);
}
return NULL;
}
static svn_error_t *
hook_symlink_error(const char *hook) {
return svn_error_createf
(SVN_ERR_REPOS_HOOK_FAILURE, NULL,
_("Failed to run '%s' hook; broken symlink"), hook);
}
svn_error_t *
svn_repos__hooks_start_commit(svn_repos_t *repos,
const char *user,
apr_array_header_t *capabilities,
apr_pool_t *pool) {
const char *hook = svn_repos_start_commit_hook(repos, pool);
svn_boolean_t broken_link;
if ((hook = check_hook_cmd(hook, &broken_link, pool)) && broken_link) {
return hook_symlink_error(hook);
} else if (hook) {
const char *args[5];
char *capabilities_string;
if (capabilities) {
capabilities_string = svn_cstring_join(capabilities, ":", pool);
if (capabilities_string[0])
capabilities_string[strlen(capabilities_string) - 1] = '\0';
} else {
capabilities_string = apr_pstrdup(pool, "");
}
args[0] = hook;
args[1] = svn_path_local_style(svn_repos_path(repos, pool), pool);
args[2] = user ? user : "";
args[3] = capabilities_string;
args[4] = NULL;
SVN_ERR(run_hook_cmd(SVN_REPOS__HOOK_START_COMMIT, hook, args, NULL,
pool));
}
return SVN_NO_ERROR;
}
svn_error_t *
svn_repos__hooks_pre_commit(svn_repos_t *repos,
const char *txn_name,
apr_pool_t *pool) {
const char *hook = svn_repos_pre_commit_hook(repos, pool);
svn_boolean_t broken_link;
if ((hook = check_hook_cmd(hook, &broken_link, pool)) && broken_link) {
return hook_symlink_error(hook);
} else if (hook) {
const char *args[4];
args[0] = hook;
args[1] = svn_path_local_style(svn_repos_path(repos, pool), pool);
args[2] = txn_name;
args[3] = NULL;
SVN_ERR(run_hook_cmd(SVN_REPOS__HOOK_PRE_COMMIT, hook, args, NULL,
pool));
}
return SVN_NO_ERROR;
}
svn_error_t *
svn_repos__hooks_post_commit(svn_repos_t *repos,
svn_revnum_t rev,
apr_pool_t *pool) {
const char *hook = svn_repos_post_commit_hook(repos, pool);
svn_boolean_t broken_link;
if ((hook = check_hook_cmd(hook, &broken_link, pool)) && broken_link) {
return hook_symlink_error(hook);
} else if (hook) {
const char *args[4];
args[0] = hook;
args[1] = svn_path_local_style(svn_repos_path(repos, pool), pool);
args[2] = apr_psprintf(pool, "%ld", rev);
args[3] = NULL;
SVN_ERR(run_hook_cmd(SVN_REPOS__HOOK_POST_COMMIT, hook, args, NULL,
pool));
}
return SVN_NO_ERROR;
}
svn_error_t *
svn_repos__hooks_pre_revprop_change(svn_repos_t *repos,
svn_revnum_t rev,
const char *author,
const char *name,
const svn_string_t *new_value,
char action,
apr_pool_t *pool) {
const char *hook = svn_repos_pre_revprop_change_hook(repos, pool);
svn_boolean_t broken_link;
if ((hook = check_hook_cmd(hook, &broken_link, pool)) && broken_link) {
return hook_symlink_error(hook);
} else if (hook) {
const char *args[7];
apr_file_t *stdin_handle = NULL;
char action_string[2];
if (new_value)
SVN_ERR(create_temp_file(&stdin_handle, new_value, pool));
else
SVN_ERR(svn_io_file_open(&stdin_handle, SVN_NULL_DEVICE_NAME,
APR_READ, APR_OS_DEFAULT, pool));
action_string[0] = action;
action_string[1] = '\0';
args[0] = hook;
args[1] = svn_path_local_style(svn_repos_path(repos, pool), pool);
args[2] = apr_psprintf(pool, "%ld", rev);
args[3] = author ? author : "";
args[4] = name;
args[5] = action_string;
args[6] = NULL;
SVN_ERR(run_hook_cmd(SVN_REPOS__HOOK_PRE_REVPROP_CHANGE, hook, args,
stdin_handle, pool));
SVN_ERR(svn_io_file_close(stdin_handle, pool));
} else {
return
svn_error_create
(SVN_ERR_REPOS_DISABLED_FEATURE, NULL,
_("Repository has not been enabled to accept revision propchanges;\n"
"ask the administrator to create a pre-revprop-change hook"));
}
return SVN_NO_ERROR;
}
svn_error_t *
svn_repos__hooks_post_revprop_change(svn_repos_t *repos,
svn_revnum_t rev,
const char *author,
const char *name,
svn_string_t *old_value,
char action,
apr_pool_t *pool) {
const char *hook = svn_repos_post_revprop_change_hook(repos, pool);
svn_boolean_t broken_link;
if ((hook = check_hook_cmd(hook, &broken_link, pool)) && broken_link) {
return hook_symlink_error(hook);
} else if (hook) {
const char *args[7];
apr_file_t *stdin_handle = NULL;
char action_string[2];
if (old_value)
SVN_ERR(create_temp_file(&stdin_handle, old_value, pool));
else
SVN_ERR(svn_io_file_open(&stdin_handle, SVN_NULL_DEVICE_NAME,
APR_READ, APR_OS_DEFAULT, pool));
action_string[0] = action;
action_string[1] = '\0';
args[0] = hook;
args[1] = svn_path_local_style(svn_repos_path(repos, pool), pool);
args[2] = apr_psprintf(pool, "%ld", rev);
args[3] = author ? author : "";
args[4] = name;
args[5] = action_string;
args[6] = NULL;
SVN_ERR(run_hook_cmd(SVN_REPOS__HOOK_POST_REVPROP_CHANGE, hook, args,
stdin_handle, pool));
SVN_ERR(svn_io_file_close(stdin_handle, pool));
}
return SVN_NO_ERROR;
}
svn_error_t *
svn_repos__hooks_pre_lock(svn_repos_t *repos,
const char *path,
const char *username,
apr_pool_t *pool) {
const char *hook = svn_repos_pre_lock_hook(repos, pool);
svn_boolean_t broken_link;
if ((hook = check_hook_cmd(hook, &broken_link, pool)) && broken_link) {
return hook_symlink_error(hook);
} else if (hook) {
const char *args[5];
args[0] = hook;
args[1] = svn_path_local_style(svn_repos_path(repos, pool), pool);
args[2] = path;
args[3] = username;
args[4] = NULL;
SVN_ERR(run_hook_cmd(SVN_REPOS__HOOK_PRE_LOCK, hook, args, NULL, pool));
}
return SVN_NO_ERROR;
}
svn_error_t *
svn_repos__hooks_post_lock(svn_repos_t *repos,
apr_array_header_t *paths,
const char *username,
apr_pool_t *pool) {
const char *hook = svn_repos_post_lock_hook(repos, pool);
svn_boolean_t broken_link;
if ((hook = check_hook_cmd(hook, &broken_link, pool)) && broken_link) {
return hook_symlink_error(hook);
} else if (hook) {
const char *args[5];
apr_file_t *stdin_handle = NULL;
svn_string_t *paths_str = svn_string_create(svn_cstring_join
(paths, "\n", pool),
pool);
SVN_ERR(create_temp_file(&stdin_handle, paths_str, pool));
args[0] = hook;
args[1] = svn_path_local_style(svn_repos_path(repos, pool), pool);
args[2] = username;
args[3] = NULL;
args[4] = NULL;
SVN_ERR(run_hook_cmd(SVN_REPOS__HOOK_POST_LOCK, hook, args, stdin_handle,
pool));
SVN_ERR(svn_io_file_close(stdin_handle, pool));
}
return SVN_NO_ERROR;
}
svn_error_t *
svn_repos__hooks_pre_unlock(svn_repos_t *repos,
const char *path,
const char *username,
apr_pool_t *pool) {
const char *hook = svn_repos_pre_unlock_hook(repos, pool);
svn_boolean_t broken_link;
if ((hook = check_hook_cmd(hook, &broken_link, pool)) && broken_link) {
return hook_symlink_error(hook);
} else if (hook) {
const char *args[5];
args[0] = hook;
args[1] = svn_path_local_style(svn_repos_path(repos, pool), pool);
args[2] = path;
args[3] = username ? username : "";
args[4] = NULL;
SVN_ERR(run_hook_cmd(SVN_REPOS__HOOK_PRE_UNLOCK, hook, args, NULL,
pool));
}
return SVN_NO_ERROR;
}
svn_error_t *
svn_repos__hooks_post_unlock(svn_repos_t *repos,
apr_array_header_t *paths,
const char *username,
apr_pool_t *pool) {
const char *hook = svn_repos_post_unlock_hook(repos, pool);
svn_boolean_t broken_link;
if ((hook = check_hook_cmd(hook, &broken_link, pool)) && broken_link) {
return hook_symlink_error(hook);
} else if (hook) {
const char *args[5];
apr_file_t *stdin_handle = NULL;
svn_string_t *paths_str = svn_string_create(svn_cstring_join
(paths, "\n", pool),
pool);
SVN_ERR(create_temp_file(&stdin_handle, paths_str, pool));
args[0] = hook;
args[1] = svn_path_local_style(svn_repos_path(repos, pool), pool);
args[2] = username ? username : "";
args[3] = NULL;
args[4] = NULL;
SVN_ERR(run_hook_cmd(SVN_REPOS__HOOK_POST_UNLOCK, hook, args,
stdin_handle, pool));
SVN_ERR(svn_io_file_close(stdin_handle, pool));
}
return SVN_NO_ERROR;
}
