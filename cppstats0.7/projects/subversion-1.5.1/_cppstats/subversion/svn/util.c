#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <apr_env.h>
#include <apr_errno.h>
#include <apr_file_info.h>
#include <apr_strings.h>
#include <apr_tables.h>
#include <apr_general.h>
#include <apr_lib.h>
#include "svn_pools.h"
#include "svn_error.h"
#include "svn_ctype.h"
#include "svn_client.h"
#include "svn_cmdline.h"
#include "svn_string.h"
#include "svn_path.h"
#include "svn_hash.h"
#include "svn_io.h"
#include "svn_utf.h"
#include "svn_subst.h"
#include "svn_config.h"
#include "svn_xml.h"
#include "svn_time.h"
#include "svn_private_config.h"
#include "cl.h"
svn_error_t *
svn_cl__print_commit_info(svn_commit_info_t *commit_info,
apr_pool_t *pool) {
if (SVN_IS_VALID_REVNUM(commit_info->revision))
SVN_ERR(svn_cmdline_printf(pool, _("\nCommitted revision %ld.\n"),
commit_info->revision));
if (commit_info->post_commit_err)
SVN_ERR(svn_cmdline_printf(pool, _("\nWarning: %s\n"),
commit_info->post_commit_err));
return SVN_NO_ERROR;
}
static svn_error_t *
find_editor_binary(const char **editor,
const char *editor_cmd,
apr_hash_t *config) {
const char *e;
struct svn_config_t *cfg;
e = editor_cmd;
if (! e)
e = getenv("SVN_EDITOR");
if (! e) {
cfg = config ? apr_hash_get(config, SVN_CONFIG_CATEGORY_CONFIG,
APR_HASH_KEY_STRING) : NULL;
svn_config_get(cfg, &e, SVN_CONFIG_SECTION_HELPERS,
SVN_CONFIG_OPTION_EDITOR_CMD, NULL);
}
if (! e)
e = getenv("VISUAL");
if (! e)
e = getenv("EDITOR");
#if defined(SVN_CLIENT_EDITOR)
if (! e)
e = SVN_CLIENT_EDITOR;
#endif
if (e) {
const char *c;
for (c = e; *c; c++)
if (!svn_ctype_isspace(*c))
break;
if (! *c)
return svn_error_create
(SVN_ERR_CL_NO_EXTERNAL_EDITOR, NULL,
_("The EDITOR, SVN_EDITOR or VISUAL environment variable or "
"'editor-cmd' run-time configuration option is empty or "
"consists solely of whitespace. Expected a shell command."));
} else
return svn_error_create
(SVN_ERR_CL_NO_EXTERNAL_EDITOR, NULL,
_("None of the environment variables SVN_EDITOR, VISUAL or EDITOR are "
"set, and no 'editor-cmd' run-time configuration option was found"));
*editor = e;
return SVN_NO_ERROR;
}
svn_error_t *
svn_cl__edit_file_externally(const char *path,
const char *editor_cmd,
apr_hash_t *config,
apr_pool_t *pool) {
const char *editor, *cmd, *base_dir, *file_name, *base_dir_apr;
char *old_cwd;
int sys_err;
apr_status_t apr_err;
svn_path_split(path, &base_dir, &file_name, pool);
SVN_ERR(find_editor_binary(&editor, editor_cmd, config));
apr_err = apr_filepath_get(&old_cwd, APR_FILEPATH_NATIVE, pool);
if (apr_err)
return svn_error_wrap_apr(apr_err, _("Can't get working directory"));
if (base_dir[0] == '\0')
base_dir_apr = ".";
else
SVN_ERR(svn_path_cstring_from_utf8(&base_dir_apr, base_dir, pool));
apr_err = apr_filepath_set(base_dir_apr, pool);
if (apr_err)
return svn_error_wrap_apr
(apr_err, _("Can't change working directory to '%s'"), base_dir);
cmd = apr_psprintf(pool, "%s %s", editor, file_name);
sys_err = system(cmd);
apr_err = apr_filepath_set(old_cwd, pool);
if (apr_err)
svn_handle_error2(svn_error_wrap_apr
(apr_err, _("Can't restore working directory")),
stderr, TRUE , "svn: ");
if (sys_err)
return svn_error_createf(SVN_ERR_EXTERNAL_PROGRAM, NULL,
_("system('%s') returned %d"), cmd, sys_err);
return SVN_NO_ERROR;
}
svn_error_t *
svn_cl__merge_file_externally(const char *base_path,
const char *their_path,
const char *my_path,
const char *merged_path,
apr_hash_t *config,
apr_pool_t *pool) {
char *merge_tool;
if (apr_env_get(&merge_tool, "SVN_MERGE", pool) != APR_SUCCESS) {
struct svn_config_t *cfg;
merge_tool = NULL;
cfg = config ? apr_hash_get(config, SVN_CONFIG_CATEGORY_CONFIG,
APR_HASH_KEY_STRING) : NULL;
svn_config_get(cfg, (const char **)&merge_tool,
SVN_CONFIG_SECTION_HELPERS,
SVN_CONFIG_OPTION_MERGE_TOOL_CMD, NULL);
}
if (merge_tool) {
const char *c;
for (c = merge_tool; *c; c++)
if (!svn_ctype_isspace(*c))
break;
if (! *c)
return svn_error_create
(SVN_ERR_CL_NO_EXTERNAL_MERGE_TOOL, NULL,
_("The SVN_MERGE environment variable is empty or "
"consists solely of whitespace. Expected a shell command.\n"));
} else
return svn_error_create
(SVN_ERR_CL_NO_EXTERNAL_MERGE_TOOL, NULL,
_("The environment variable SVN_MERGE and the merge-tool-cmd run-time "
"configuration option were not set.\n"));
{
const char *arguments[] = { merge_tool, base_path, their_path,
my_path, merged_path, NULL
};
char *cwd;
apr_status_t status = apr_filepath_get(&cwd, APR_FILEPATH_NATIVE, pool);
if (status != 0)
return svn_error_wrap_apr(status, NULL);
return svn_io_run_cmd(svn_path_internal_style(cwd, pool), merge_tool,
arguments, NULL, NULL, TRUE, NULL, NULL, NULL,
pool);
}
}
svn_error_t *
svn_cl__edit_string_externally(svn_string_t **edited_contents ,
const char **tmpfile_left ,
const char *editor_cmd,
const char *base_dir ,
const svn_string_t *contents ,
const char *prefix,
apr_hash_t *config,
svn_boolean_t as_text,
const char *encoding,
apr_pool_t *pool) {
const char *editor;
const char *cmd;
apr_file_t *tmp_file;
const char *tmpfile_name;
const char *tmpfile_native;
const char *tmpfile_apr, *base_dir_apr;
svn_string_t *translated_contents;
apr_status_t apr_err, apr_err2;
apr_size_t written;
apr_finfo_t finfo_before, finfo_after;
svn_error_t *err = SVN_NO_ERROR, *err2;
char *old_cwd;
int sys_err;
svn_boolean_t remove_file = TRUE;
SVN_ERR(find_editor_binary(&editor, editor_cmd, config));
if (as_text) {
const char *translated;
SVN_ERR(svn_subst_translate_cstring2(contents->data, &translated,
APR_EOL_STR, FALSE,
NULL, FALSE, pool));
translated_contents = svn_string_create("", pool);
if (encoding)
SVN_ERR(svn_utf_cstring_from_utf8_ex2(&translated_contents->data,
translated, encoding, pool));
else
SVN_ERR(svn_utf_cstring_from_utf8(&translated_contents->data,
translated, pool));
translated_contents->len = strlen(translated_contents->data);
} else
translated_contents = svn_string_dup(contents, pool);
apr_err = apr_filepath_get(&old_cwd, APR_FILEPATH_NATIVE, pool);
if (apr_err)
return svn_error_wrap_apr(apr_err, _("Can't get working directory"));
if (base_dir[0] == '\0')
base_dir_apr = ".";
else
SVN_ERR(svn_path_cstring_from_utf8(&base_dir_apr, base_dir, pool));
apr_err = apr_filepath_set(base_dir_apr, pool);
if (apr_err) {
return svn_error_wrap_apr
(apr_err, _("Can't change working directory to '%s'"), base_dir);
}
err = svn_io_open_unique_file2(&tmp_file, &tmpfile_name,
prefix, ".tmp", svn_io_file_del_none, pool);
if (err && (APR_STATUS_IS_EACCES(err->apr_err) || err->apr_err == EROFS)) {
const char *temp_dir_apr;
svn_error_clear(err);
SVN_ERR(svn_io_temp_dir(&base_dir, pool));
SVN_ERR(svn_path_cstring_from_utf8(&temp_dir_apr, base_dir, pool));
apr_err = apr_filepath_set(temp_dir_apr, pool);
if (apr_err) {
return svn_error_wrap_apr
(apr_err, _("Can't change working directory to '%s'"), base_dir);
}
err = svn_io_open_unique_file2(&tmp_file, &tmpfile_name,
prefix, ".tmp",
svn_io_file_del_none, pool);
}
if (err)
goto cleanup2;
apr_err = apr_file_write_full(tmp_file, translated_contents->data,
translated_contents->len, &written);
apr_err2 = apr_file_close(tmp_file);
if (! apr_err)
apr_err = apr_err2;
if (apr_err) {
err = svn_error_wrap_apr(apr_err, _("Can't write to '%s'"),
tmpfile_name);
goto cleanup;
}
err = svn_path_cstring_from_utf8(&tmpfile_apr, tmpfile_name, pool);
if (err)
goto cleanup;
apr_err = apr_stat(&finfo_before, tmpfile_apr,
APR_FINFO_MTIME, pool);
if (apr_err) {
err = svn_error_wrap_apr(apr_err, _("Can't stat '%s'"), tmpfile_name);
goto cleanup;
}
apr_file_mtime_set(tmpfile_apr, finfo_before.mtime - 2000, pool);
apr_err = apr_stat(&finfo_before, tmpfile_apr,
APR_FINFO_MTIME | APR_FINFO_SIZE, pool);
if (apr_err) {
err = svn_error_wrap_apr(apr_err, _("Can't stat '%s'"), tmpfile_name);
goto cleanup;
}
err = svn_utf_cstring_from_utf8(&tmpfile_native, tmpfile_name, pool);
if (err)
goto cleanup;
cmd = apr_psprintf(pool, "%s %s", editor, tmpfile_native);
sys_err = system(cmd);
if (sys_err != 0) {
err = svn_error_createf(SVN_ERR_EXTERNAL_PROGRAM, NULL,
_("system('%s') returned %d"), cmd, sys_err);
goto cleanup;
}
apr_err = apr_stat(&finfo_after, tmpfile_apr,
APR_FINFO_MTIME | APR_FINFO_SIZE, pool);
if (apr_err) {
err = svn_error_wrap_apr(apr_err, _("Can't stat '%s'"), tmpfile_name);
goto cleanup;
}
if (tmpfile_left) {
*tmpfile_left = svn_path_join(base_dir, tmpfile_name, pool);
remove_file = FALSE;
}
if ((finfo_before.mtime != finfo_after.mtime) ||
(finfo_before.size != finfo_after.size)) {
svn_stringbuf_t *edited_contents_s;
err = svn_stringbuf_from_file(&edited_contents_s, tmpfile_name, pool);
if (err)
goto cleanup;
*edited_contents = svn_string_create_from_buf(edited_contents_s, pool);
if (as_text) {
err = svn_subst_translate_string(edited_contents, *edited_contents,
encoding, pool);
if (err) {
err = svn_error_quick_wrap
(err,
_("Error normalizing edited contents to internal format"));
goto cleanup;
}
}
} else {
*edited_contents = NULL;
}
cleanup:
if (remove_file) {
err2 = svn_io_remove_file(tmpfile_name, pool);
if (! err && err2)
err = err2;
else
svn_error_clear(err2);
}
cleanup2:
apr_err = apr_filepath_set(old_cwd, pool);
if (apr_err) {
svn_handle_error2(svn_error_wrap_apr
(apr_err, _("Can't restore working directory")),
stderr, TRUE , "svn: ");
}
return err;
}
struct log_msg_baton {
const char *editor_cmd;
const char *message;
const char *message_encoding;
const char *base_dir;
const char *tmpfile_left;
svn_boolean_t non_interactive;
apr_hash_t *config;
svn_boolean_t keep_locks;
apr_pool_t *pool;
};
svn_error_t *
svn_cl__make_log_msg_baton(void **baton,
svn_cl__opt_state_t *opt_state,
const char *base_dir ,
apr_hash_t *config,
apr_pool_t *pool) {
struct log_msg_baton *lmb = apr_palloc(pool, sizeof(*lmb));
if (opt_state->filedata) {
if (strlen(opt_state->filedata->data) < opt_state->filedata->len) {
return svn_error_create(SVN_ERR_CL_BAD_LOG_MESSAGE, NULL,
_("Log message contains a zero byte"));
}
lmb->message = opt_state->filedata->data;
} else {
lmb->message = opt_state->message;
}
lmb->editor_cmd = opt_state->editor_cmd;
if (opt_state->encoding) {
lmb->message_encoding = opt_state->encoding;
} else if (config) {
svn_config_t *cfg = apr_hash_get(config, SVN_CONFIG_CATEGORY_CONFIG,
APR_HASH_KEY_STRING);
svn_config_get(cfg, &(lmb->message_encoding),
SVN_CONFIG_SECTION_MISCELLANY,
SVN_CONFIG_OPTION_LOG_ENCODING,
NULL);
}
lmb->base_dir = base_dir ? base_dir : "";
lmb->tmpfile_left = NULL;
lmb->config = config;
lmb->keep_locks = opt_state->no_unlock;
lmb->non_interactive = opt_state->non_interactive;
lmb->pool = pool;
*baton = lmb;
return SVN_NO_ERROR;
}
svn_error_t *
svn_cl__cleanup_log_msg(void *log_msg_baton,
svn_error_t *commit_err) {
struct log_msg_baton *lmb = log_msg_baton;
if ((! lmb) || (! lmb->tmpfile_left))
return commit_err;
if (! commit_err)
return svn_io_remove_file(lmb->tmpfile_left, lmb->pool);
svn_error_compose
(commit_err,
svn_error_create(commit_err->apr_err,
svn_error_createf(commit_err->apr_err, NULL,
" '%s'", lmb->tmpfile_left),
_("Your commit message was left in "
"a temporary file:")));
return commit_err;
}
static void
truncate_buffer_at_prefix(apr_size_t *new_len,
char *buffer,
const char *prefix) {
char *substring = buffer;
assert(buffer && prefix);
if (new_len)
*new_len = strlen(buffer);
while (1) {
substring = strstr(substring, prefix);
if (! substring)
return;
if ((substring == buffer)
|| (*(substring - 1) == '\r')
|| (*(substring - 1) == '\n')) {
*substring = '\0';
if (new_len)
*new_len = substring - buffer;
} else if (substring) {
substring++;
}
}
}
#define EDITOR_EOF_PREFIX _("--This line, and those below, will be ignored--")
svn_error_t *
svn_cl__get_log_message(const char **log_msg,
const char **tmp_file,
const apr_array_header_t *commit_items,
void *baton,
apr_pool_t *pool) {
svn_stringbuf_t *default_msg = NULL;
struct log_msg_baton *lmb = baton;
svn_stringbuf_t *message = NULL;
default_msg = svn_stringbuf_create(APR_EOL_STR, pool);
svn_stringbuf_appendcstr(default_msg, EDITOR_EOF_PREFIX);
svn_stringbuf_appendcstr(default_msg, APR_EOL_STR APR_EOL_STR);
*tmp_file = NULL;
if (lmb->message) {
svn_string_t *log_msg_string = svn_string_create(lmb->message, pool);
SVN_ERR_W(svn_subst_translate_string(&log_msg_string, log_msg_string,
lmb->message_encoding, pool),
_("Error normalizing log message to internal format"));
*log_msg = log_msg_string->data;
truncate_buffer_at_prefix(NULL, (char*)*log_msg, EDITOR_EOF_PREFIX);
return SVN_NO_ERROR;
}
#if defined(AS400)
else
return svn_error_create
(SVN_ERR_CL_NO_EXTERNAL_EDITOR, NULL,
_("Use of an external editor to fetch log message is not supported "
"on OS400; consider using the --message (-m) or --file (-F) "
"options"));
#endif
if (! commit_items->nelts) {
*log_msg = "";
return SVN_NO_ERROR;
}
while (! message) {
int i;
svn_stringbuf_t *tmp_message = svn_stringbuf_dup(default_msg, pool);
svn_error_t *err = SVN_NO_ERROR;
svn_string_t *msg_string = svn_string_create("", pool);
for (i = 0; i < commit_items->nelts; i++) {
svn_client_commit_item3_t *item
= APR_ARRAY_IDX(commit_items, i, svn_client_commit_item3_t *);
const char *path = item->path;
char text_mod = '_', prop_mod = ' ', unlock = ' ';
if (! path)
path = item->url;
else if (! *path)
path = ".";
if (path && lmb->base_dir)
path = svn_path_is_child(lmb->base_dir, path, pool);
if (! path)
path = ".";
if ((item->state_flags & SVN_CLIENT_COMMIT_ITEM_DELETE)
&& (item->state_flags & SVN_CLIENT_COMMIT_ITEM_ADD))
text_mod = 'R';
else if (item->state_flags & SVN_CLIENT_COMMIT_ITEM_ADD)
text_mod = 'A';
else if (item->state_flags & SVN_CLIENT_COMMIT_ITEM_DELETE)
text_mod = 'D';
else if (item->state_flags & SVN_CLIENT_COMMIT_ITEM_TEXT_MODS)
text_mod = 'M';
if (item->state_flags & SVN_CLIENT_COMMIT_ITEM_PROP_MODS)
prop_mod = 'M';
if (! lmb->keep_locks
&& item->state_flags & SVN_CLIENT_COMMIT_ITEM_LOCK_TOKEN)
unlock = 'U';
svn_stringbuf_appendbytes(tmp_message, &text_mod, 1);
svn_stringbuf_appendbytes(tmp_message, &prop_mod, 1);
svn_stringbuf_appendbytes(tmp_message, &unlock, 1);
if (item->state_flags & SVN_CLIENT_COMMIT_ITEM_IS_COPY)
svn_stringbuf_appendcstr(tmp_message, "+ ");
else
svn_stringbuf_appendcstr(tmp_message, " ");
svn_stringbuf_appendcstr(tmp_message, path);
svn_stringbuf_appendcstr(tmp_message, APR_EOL_STR);
}
msg_string->data = tmp_message->data;
msg_string->len = tmp_message->len;
if (! lmb->non_interactive) {
err = svn_cl__edit_string_externally(&msg_string, &lmb->tmpfile_left,
lmb->editor_cmd, lmb->base_dir,
msg_string, "svn-commit",
lmb->config, TRUE,
lmb->message_encoding,
pool);
} else {
return svn_error_create
(SVN_ERR_CL_INSUFFICIENT_ARGS, NULL,
_("Cannot invoke editor to get log message "
"when non-interactive"));
}
*tmp_file = lmb->tmpfile_left = apr_pstrdup(lmb->pool,
lmb->tmpfile_left);
if (err) {
if (err->apr_err == SVN_ERR_CL_NO_EXTERNAL_EDITOR)
err = svn_error_quick_wrap
(err, _("Could not use external editor to fetch log message; "
"consider setting the $SVN_EDITOR environment variable "
"or using the --message (-m) or --file (-F) options"));
return err;
}
if (msg_string)
message = svn_stringbuf_create_from_string(msg_string, pool);
if (message)
truncate_buffer_at_prefix(&message->len, message->data,
EDITOR_EOF_PREFIX);
if (message) {
int len;
for (len = message->len - 1; len >= 0; len--) {
if (! apr_isspace(message->data[len]))
break;
}
if (len < 0)
message = NULL;
}
if (! message) {
const char *reply;
SVN_ERR(svn_cmdline_prompt_user
(&reply,
_("\nLog message unchanged or not specified\n"
"(a)bort, (c)ontinue, (e)dit :\n"), pool));
if (reply) {
char letter = apr_tolower(reply[0]);
if ('a' == letter) {
SVN_ERR(svn_io_remove_file(lmb->tmpfile_left, pool));
*tmp_file = lmb->tmpfile_left = NULL;
break;
}
if ('c' == letter) {
SVN_ERR(svn_io_remove_file(lmb->tmpfile_left, pool));
*tmp_file = lmb->tmpfile_left = NULL;
message = svn_stringbuf_create("", pool);
}
}
}
}
*log_msg = message ? message->data : NULL;
return SVN_NO_ERROR;
}
svn_error_t *
svn_cl__may_need_force(svn_error_t *err) {
if (err
&& (err->apr_err == SVN_ERR_UNVERSIONED_RESOURCE ||
err->apr_err == SVN_ERR_CLIENT_MODIFIED)) {
err = svn_error_quick_wrap
(err, _("Use --force to override this restriction") );
}
return err;
}
svn_error_t *
svn_cl__error_checked_fputs(const char *string, FILE* stream) {
errno = 0;
if (fputs(string, stream) == EOF) {
if (errno)
return svn_error_wrap_apr(errno, _("Write error"));
else
return svn_error_create(SVN_ERR_IO_WRITE_ERROR, NULL, NULL);
}
return SVN_NO_ERROR;
}
svn_error_t *
svn_cl__try(svn_error_t *err,
svn_boolean_t *success,
svn_boolean_t quiet,
...) {
if (err) {
apr_status_t apr_err;
va_list ap;
if (success)
*success = FALSE;
va_start(ap, quiet);
while ((apr_err = va_arg(ap, apr_status_t)) != SVN_NO_ERROR) {
if (err->apr_err == apr_err) {
if (! quiet)
svn_handle_warning(stderr, err);
svn_error_clear(err);
return SVN_NO_ERROR;
}
}
va_end(ap);
} else if (success) {
*success = TRUE;
}
return err;
}
void
svn_cl__xml_tagged_cdata(svn_stringbuf_t **sb,
apr_pool_t *pool,
const char *tagname,
const char *string) {
if (string) {
svn_xml_make_open_tag(sb, pool, svn_xml_protect_pcdata,
tagname, NULL);
svn_xml_escape_cdata_cstring(sb, string, pool);
svn_xml_make_close_tag(sb, pool, tagname);
}
}
void
svn_cl__print_xml_commit(svn_stringbuf_t **sb,
svn_revnum_t revision,
const char *author,
const char *date,
apr_pool_t *pool) {
svn_xml_make_open_tag(sb, pool, svn_xml_normal, "commit",
"revision",
apr_psprintf(pool, "%ld", revision), NULL);
if (author)
svn_cl__xml_tagged_cdata(sb, pool, "author", author);
if (date)
svn_cl__xml_tagged_cdata(sb, pool, "date", date);
svn_xml_make_close_tag(sb, pool, "commit");
}
svn_error_t *
svn_cl__xml_print_header(const char *tagname,
apr_pool_t *pool) {
svn_stringbuf_t *sb = svn_stringbuf_create("", pool);
svn_xml_make_header(&sb, pool);
svn_xml_make_open_tag(&sb, pool, svn_xml_normal, tagname, NULL);
return svn_cl__error_checked_fputs(sb->data, stdout);
}
svn_error_t *
svn_cl__xml_print_footer(const char *tagname,
apr_pool_t *pool) {
svn_stringbuf_t *sb = svn_stringbuf_create("", pool);
svn_xml_make_close_tag(&sb, pool, tagname);
return svn_cl__error_checked_fputs(sb->data, stdout);
}
const char *
svn_cl__node_kind_str(svn_node_kind_t kind) {
switch (kind) {
case svn_node_dir:
return "dir";
case svn_node_file:
return "file";
default:
return "";
}
}
svn_error_t *
svn_cl__args_to_target_array_print_reserved(apr_array_header_t **targets,
apr_getopt_t *os,
apr_array_header_t *known_targets,
apr_pool_t *pool) {
svn_error_t *error = svn_opt_args_to_target_array3(targets, os,
known_targets, pool);
if (error) {
if (error->apr_err == SVN_ERR_RESERVED_FILENAME_SPECIFIED) {
svn_handle_error2(error, stderr, FALSE, "svn: Skipping argument: ");
svn_error_clear(error);
} else
return error;
}
return SVN_NO_ERROR;
}
static svn_error_t *
changelist_receiver(void *baton,
const char *path,
const char *changelist,
apr_pool_t *pool) {
apr_array_header_t *paths = baton;
APR_ARRAY_PUSH(paths, const char *) = apr_pstrdup(paths->pool, path);
return SVN_NO_ERROR;
}
svn_error_t *
svn_cl__changelist_paths(apr_array_header_t **paths,
const apr_array_header_t *changelists,
const apr_array_header_t *targets,
svn_depth_t depth,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
apr_array_header_t *found;
apr_pool_t *subpool = svn_pool_create(pool);
apr_hash_t *paths_hash;
int i;
if (! (changelists && changelists->nelts)) {
*paths = (apr_array_header_t *)targets;
return SVN_NO_ERROR;
}
found = apr_array_make(pool, 8, sizeof(const char *));
for (i = 0; i < targets->nelts; i++) {
const char *target = APR_ARRAY_IDX(targets, i, const char *);
svn_pool_clear(subpool);
SVN_ERR(svn_client_get_changelists(target, changelists, depth,
changelist_receiver, (void *)found,
ctx, subpool));
}
svn_pool_destroy(subpool);
SVN_ERR(svn_hash_from_cstring_keys(&paths_hash, found, pool));
return svn_hash_keys(paths, paths_hash, pool);
}
svn_cl__show_revs_t
svn_cl__show_revs_from_word(const char *word) {
if (strcmp(word, SVN_CL__SHOW_REVS_MERGED) == 0)
return svn_cl__show_revs_merged;
if (strcmp(word, SVN_CL__SHOW_REVS_ELIGIBLE) == 0)
return svn_cl__show_revs_eligible;
return svn_cl__show_revs_invalid;
}
svn_error_t *
svn_cl__time_cstring_to_human_cstring(const char **human_cstring,
const char *data,
apr_pool_t *pool) {
svn_error_t *err;
apr_time_t when;
err = svn_time_from_cstring(&when, data, pool);
if (err && err->apr_err == SVN_ERR_BAD_DATE) {
svn_error_clear(err);
*human_cstring = _("(invalid date)");
return SVN_NO_ERROR;
} else if (err)
return err;
*human_cstring = svn_time_to_human_cstring(when, pool);
return SVN_NO_ERROR;
}
