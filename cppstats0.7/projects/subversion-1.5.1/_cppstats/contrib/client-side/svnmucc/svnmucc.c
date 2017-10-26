#include "svn_cmdline.h"
#include "svn_client.h"
#include "svn_pools.h"
#include "svn_error.h"
#include "svn_path.h"
#include "svn_ra.h"
#include "svn_config.h"
#include <apr_lib.h>
#include <stdio.h>
#include <string.h>
static void handle_error(svn_error_t *err, apr_pool_t *pool) {
if (err)
svn_handle_error2(err, stderr, FALSE, "svnmucc: ");
svn_error_clear(err);
if (pool)
svn_pool_destroy(pool);
exit(EXIT_FAILURE);
}
static apr_pool_t *
init(const char *application) {
apr_allocator_t *allocator;
apr_pool_t *pool;
svn_error_t *err;
const svn_version_checklist_t checklist[] = {
{"svn_client", svn_client_version},
{"svn_subr", svn_subr_version},
{"svn_ra", svn_ra_version},
{NULL, NULL}
};
SVN_VERSION_DEFINE(my_version);
if (svn_cmdline_init(application, stderr)
|| apr_allocator_create(&allocator))
exit(EXIT_FAILURE);
err = svn_ver_check_list(&my_version, checklist);
if (err)
handle_error(err, NULL);
apr_allocator_max_free_set(allocator, SVN_ALLOCATOR_RECOMMENDED_MAX_FREE);
pool = svn_pool_create_ex(NULL, allocator);
apr_allocator_owner_set(allocator, pool);
return pool;
}
static svn_error_t *
open_tmp_file(apr_file_t **fp,
void *callback_baton,
apr_pool_t *pool) {
const char *temp_dir;
SVN_ERR(svn_io_temp_dir(&temp_dir, pool));
return svn_io_open_unique_file2(fp, NULL,
svn_path_join(temp_dir, "svnmucc", pool),
".tmp", svn_io_file_del_on_close, pool);
}
static svn_ra_callbacks_t *
ra_callbacks(const char *username,
const char *password,
svn_boolean_t non_interactive,
apr_pool_t *pool) {
svn_ra_callbacks_t *callbacks = apr_palloc(pool, sizeof(*callbacks));
svn_cmdline_setup_auth_baton(&callbacks->auth_baton, non_interactive,
username, password,
NULL, FALSE, NULL, NULL, NULL, pool);
callbacks->open_tmp_file = open_tmp_file;
callbacks->get_wc_prop = NULL;
callbacks->set_wc_prop = NULL;
callbacks->push_wc_prop = NULL;
callbacks->invalidate_wc_props = NULL;
return callbacks;
}
static svn_error_t *
commit_callback(svn_revnum_t revision,
const char *date,
const char *author,
void *baton) {
apr_pool_t *pool = baton;
SVN_ERR(svn_cmdline_printf(pool, "r%ld committed by %s at %s\n",
revision, author ? author : "(no author)",
date));
return SVN_NO_ERROR;
}
typedef enum {
ACTION_MV,
ACTION_MKDIR,
ACTION_CP,
ACTION_PROPSET,
ACTION_PROPDEL,
ACTION_PUT,
ACTION_RM
} action_code_t;
struct operation {
enum {
OP_OPEN,
OP_DELETE,
OP_ADD,
OP_REPLACE,
OP_PROPSET
} operation;
svn_node_kind_t kind;
svn_revnum_t rev;
const char *url;
const char *src_file;
apr_hash_t *children;
apr_table_t *props;
void *baton;
};
struct driver_state {
const svn_delta_editor_t *editor;
svn_node_kind_t kind;
apr_pool_t *pool;
void *baton;
svn_error_t* err;
};
static int
set_props(void *rec, const char *key, const char *value) {
struct driver_state *d_state = (struct driver_state*)rec;
svn_string_t *value_svnstring
= value ? svn_string_create(value, d_state->pool) : NULL;
if (d_state->kind == svn_node_dir)
d_state->err = d_state->editor->change_dir_prop(d_state->baton, key,
value_svnstring,
d_state->pool);
else
d_state->err = d_state->editor->change_file_prop(d_state->baton, key,
value_svnstring,
d_state->pool);
if (d_state->err)
return 0;
return 1;
}
static svn_error_t *
drive(struct operation *operation,
svn_revnum_t head,
const svn_delta_editor_t *editor,
apr_pool_t *pool) {
apr_pool_t *subpool = svn_pool_create(pool);
apr_hash_index_t *hi;
struct driver_state state;
for (hi = apr_hash_first(pool, operation->children);
hi; hi = apr_hash_next(hi)) {
const void *key;
void *val;
struct operation *child;
void *file_baton = NULL;
svn_pool_clear(subpool);
apr_hash_this(hi, &key, NULL, &val);
child = val;
if (child->operation == OP_DELETE || child->operation == OP_REPLACE) {
SVN_ERR(editor->delete_entry(key, head, operation->baton, subpool));
}
if (child->operation == OP_OPEN) {
if (child->kind == svn_node_dir) {
SVN_ERR(editor->open_directory(key, operation->baton, head,
subpool, &child->baton));
} else {
SVN_ERR(editor->open_file(key, operation->baton, head,
subpool, &file_baton));
}
}
if (child->operation == OP_ADD || child->operation == OP_REPLACE
|| child->operation == OP_PROPSET) {
if (child->kind == svn_node_dir) {
SVN_ERR(editor->add_directory(key, operation->baton,
child->url, child->rev,
subpool, &child->baton));
} else {
SVN_ERR(editor->add_file(key, operation->baton, child->url,
child->rev, subpool, &file_baton));
}
}
if ((child->src_file) && (file_baton)) {
svn_txdelta_window_handler_t handler;
void *handler_baton;
svn_stream_t *contents;
apr_file_t *f = NULL;
SVN_ERR(editor->apply_textdelta(file_baton, NULL, subpool,
&handler, &handler_baton));
SVN_ERR(svn_io_file_open(&f, child->src_file, APR_READ,
APR_OS_DEFAULT, pool));
contents = svn_stream_from_aprfile(f, pool);
SVN_ERR(svn_txdelta_send_stream(contents, handler,
handler_baton, NULL, pool));
SVN_ERR(svn_io_file_close(f, pool));
}
if (file_baton) {
if ((child->kind == svn_node_file)
&& (! apr_is_empty_table(child->props))) {
state.baton = file_baton;
state.pool = subpool;
state.editor = editor;
state.kind = child->kind;
if (! apr_table_do(set_props, &state, child->props, NULL))
SVN_ERR(state.err);
}
SVN_ERR(editor->close_file(file_baton, NULL, subpool));
}
if ((child->kind == svn_node_dir)
&& (child->operation == OP_OPEN
|| child->operation == OP_ADD
|| child->operation == OP_REPLACE)) {
SVN_ERR(drive(child, head, editor, subpool));
if ((child->kind == svn_node_dir)
&& (! apr_is_empty_table(child->props))) {
state.baton = child->baton;
state.pool = subpool;
state.editor = editor;
state.kind = child->kind;
if (! apr_table_do(set_props, &state, child->props, NULL))
SVN_ERR(state.err);
}
SVN_ERR(editor->close_directory(child->baton, subpool));
}
}
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
static struct operation *
get_operation(const char *path,
struct operation *operation,
apr_pool_t *pool) {
struct operation *child = apr_hash_get(operation->children, path,
APR_HASH_KEY_STRING);
if (! child) {
child = apr_pcalloc(pool, sizeof(*child));
child->children = apr_hash_make(pool);
child->operation = OP_OPEN;
child->rev = SVN_INVALID_REVNUM;
child->kind = svn_node_dir;
child->props = apr_table_make(pool, 0);
apr_hash_set(operation->children, path, APR_HASH_KEY_STRING, child);
}
return child;
}
static const char *
subtract_anchor(const char *anchor, const char *url, apr_pool_t *pool) {
if (! strcmp(url, anchor))
return "";
else
return svn_path_uri_decode(svn_path_is_child(anchor, url, pool), pool);
}
static svn_error_t *
build(action_code_t action,
const char *path,
const char *url,
svn_revnum_t rev,
const char *prop_name,
const char *prop_value,
const char *src_file,
svn_revnum_t head,
const char *anchor,
svn_ra_session_t *session,
struct operation *operation,
apr_pool_t *pool) {
apr_array_header_t *path_bits = svn_path_decompose(path, pool);
const char *path_so_far = "";
const char *copy_src = NULL;
svn_revnum_t copy_rev = SVN_INVALID_REVNUM;
int i;
for (i = 0; i < path_bits->nelts; ++i) {
const char *path_bit = APR_ARRAY_IDX(path_bits, i, const char *);
path_so_far = svn_path_join(path_so_far, path_bit, pool);
operation = get_operation(path_so_far, operation, pool);
if (operation->url
&& SVN_IS_VALID_REVNUM(operation->rev)
&& (operation->operation == OP_REPLACE
|| operation->operation == OP_ADD)) {
copy_src = subtract_anchor(anchor, operation->url, pool);
copy_rev = operation->rev;
} else if (copy_src) {
copy_src = svn_path_join(copy_src, path_bit, pool);
}
}
if (prop_name) {
if (operation->operation == OP_DELETE)
return svn_error_createf(SVN_ERR_BAD_URL, NULL,
"cannot set properties on a location being"
" deleted ('%s')", path);
SVN_ERR(svn_ra_check_path(session,
copy_src ? copy_src : path,
copy_src ? copy_rev : head,
&operation->kind, pool));
if (operation->kind == svn_node_none)
return svn_error_createf(SVN_ERR_BAD_URL, NULL,
"propset: '%s' not found", path);
else if ((operation->kind == svn_node_file)
&& (operation->operation == OP_OPEN))
operation->operation = OP_PROPSET;
apr_table_set(operation->props, prop_name, prop_value);
if (!operation->rev)
operation->rev = rev;
return SVN_NO_ERROR;
}
if (operation->operation != OP_OPEN
&& operation->operation != OP_PROPSET
&& operation->operation != OP_DELETE)
return svn_error_createf(SVN_ERR_BAD_URL, NULL,
"unsupported multiple operations on '%s'", path);
if (action == ACTION_RM) {
operation->operation = OP_DELETE;
SVN_ERR(svn_ra_check_path(session,
copy_src ? copy_src : path,
copy_src ? copy_rev : head,
&operation->kind, pool));
if (operation->kind == svn_node_none) {
if (copy_src && strcmp(path, copy_src))
return svn_error_createf(SVN_ERR_BAD_URL, NULL,
"'%s' (from '%s:%ld') not found",
path, copy_src, copy_rev);
else
return svn_error_createf(SVN_ERR_BAD_URL, NULL, "'%s' not found",
path);
}
}
else if (action == ACTION_CP) {
if (rev > head)
return svn_error_create(SVN_ERR_CL_ARG_PARSING_ERROR, NULL,
"Copy source revision cannot be younger "
"than base revision");
operation->operation =
operation->operation == OP_DELETE ? OP_REPLACE : OP_ADD;
SVN_ERR(svn_ra_check_path(session, subtract_anchor(anchor, url, pool),
rev, &operation->kind, pool));
if (operation->kind == svn_node_none)
return svn_error_createf(SVN_ERR_BAD_URL, NULL,
"'%s' not found", url);
operation->url = url;
operation->rev = rev;
}
else if (action == ACTION_MKDIR) {
operation->operation =
operation->operation == OP_DELETE ? OP_REPLACE : OP_ADD;
operation->kind = svn_node_dir;
}
else if (action == ACTION_PUT) {
if (operation->operation == OP_DELETE) {
operation->operation = OP_REPLACE;
} else {
SVN_ERR(svn_ra_check_path(session,
copy_src ? copy_src : path,
copy_src ? copy_rev : head,
&operation->kind, pool));
if (operation->kind == svn_node_file)
operation->operation = OP_OPEN;
else if (operation->kind == svn_node_none)
operation->operation = OP_ADD;
else
return svn_error_createf(SVN_ERR_BAD_URL, NULL,
"'%s' is not a file", path);
}
operation->kind = svn_node_file;
operation->src_file = src_file;
} else {
abort();
}
return SVN_NO_ERROR;
}
struct action {
action_code_t action;
svn_revnum_t rev;
const char *path[2];
const char *prop_name;
const char *prop_value;
};
static svn_error_t *
execute(const apr_array_header_t *actions,
const char *anchor,
const char *message,
const char *username,
const char *password,
const char *config_dir,
svn_boolean_t non_interactive,
svn_revnum_t base_revision,
apr_pool_t *pool) {
svn_ra_session_t *session;
svn_revnum_t head;
const svn_delta_editor_t *editor;
void *editor_baton;
struct operation root;
svn_error_t *err;
apr_hash_t *config;
int i;
SVN_ERR(svn_config_get_config(&config, config_dir, pool));
SVN_ERR(svn_ra_open(&session, anchor,
ra_callbacks(username, password, non_interactive, pool),
NULL, config, pool));
SVN_ERR(svn_ra_get_latest_revnum(session, &head, pool));
if (SVN_IS_VALID_REVNUM(base_revision)) {
if (base_revision > head)
return svn_error_createf(SVN_ERR_FS_NO_SUCH_REVISION, NULL,
"No such revision %ld (youngest is %ld)",
base_revision, head);
head = base_revision;
}
root.children = apr_hash_make(pool);
root.operation = OP_OPEN;
for (i = 0; i < actions->nelts; ++i) {
struct action *action = APR_ARRAY_IDX(actions, i, struct action *);
switch (action->action) {
const char *path1, *path2;
case ACTION_MV:
path1 = subtract_anchor(anchor, action->path[0], pool);
path2 = subtract_anchor(anchor, action->path[1], pool);
SVN_ERR(build(ACTION_RM, path1, NULL,
SVN_INVALID_REVNUM, NULL, NULL, NULL, head, anchor,
session, &root, pool));
SVN_ERR(build(ACTION_CP, path2, action->path[0],
head, NULL, NULL, NULL, head, anchor,
session, &root, pool));
break;
case ACTION_CP:
path1 = subtract_anchor(anchor, action->path[0], pool);
path2 = subtract_anchor(anchor, action->path[1], pool);
if (action->rev == SVN_INVALID_REVNUM)
action->rev = head;
SVN_ERR(build(ACTION_CP, path2, action->path[0],
action->rev, NULL, NULL, NULL, head, anchor,
session, &root, pool));
break;
case ACTION_RM:
path1 = subtract_anchor(anchor, action->path[0], pool);
SVN_ERR(build(ACTION_RM, path1, NULL,
SVN_INVALID_REVNUM, NULL, NULL, NULL, head, anchor,
session, &root, pool));
break;
case ACTION_MKDIR:
path1 = subtract_anchor(anchor, action->path[0], pool);
SVN_ERR(build(ACTION_MKDIR, path1, action->path[0],
SVN_INVALID_REVNUM, NULL, NULL, NULL, head, anchor,
session, &root, pool));
break;
case ACTION_PUT:
path1 = subtract_anchor(anchor, action->path[0], pool);
SVN_ERR(build(ACTION_PUT, path1, action->path[0],
SVN_INVALID_REVNUM, NULL, NULL, action->path[1],
head, anchor, session, &root, pool));
break;
case ACTION_PROPSET:
case ACTION_PROPDEL:
path1 = subtract_anchor(anchor, action->path[0], pool);
SVN_ERR(build(action->action, path1, action->path[0],
SVN_INVALID_REVNUM,
action->prop_name, action->prop_value,
NULL, head, anchor, session, &root, pool));
break;
}
}
SVN_ERR(svn_ra_get_commit_editor(session, &editor, &editor_baton, message,
commit_callback, pool, NULL, FALSE, pool));
SVN_ERR(editor->open_root(editor_baton, head, pool, &root.baton));
err = drive(&root, head, editor, pool);
if (!err)
err = editor->close_edit(editor_baton, pool);
if (err)
svn_error_clear(editor->abort_edit(editor_baton, pool));
return err;
}
static void
usage(apr_pool_t *pool, int exit_val) {
FILE *stream = exit_val == EXIT_SUCCESS ? stdout : stderr;
const char msg[] =
"Multiple URL Command Client (for Subversion)\n"
"\nUsage: svnmucc [OPTION]... [ACTION]...\n"
"\nActions:\n"
" cp REV URL1 URL2 copy URL1@REV to URL2\n"
" mkdir URL create new directory URL\n"
" mv URL1 URL2 move URL1 to URL2\n"
" rm URL delete URL\n"
" put SRC-FILE URL add or modify file URL with contents copied\n"
" from SRC-FILE\n"
" propset NAME VAL URL Set property NAME on URL to value VAL\n"
" propdel NAME URL Delete property NAME from URL\n"
"\nOptions:\n"
" -h, --help display this text\n"
" -m, --message ARG use ARG as a log message\n"
" -F, --file ARG read log message from file ARG\n"
" -u, --username ARG commit the changes as username ARG\n"
" -p, --password ARG use ARG as the password\n"
" -U, --root-url ARG interpret all action URLs are relative to ARG\n"
" -r, --revision ARG use revision ARG as baseline for changes\n"
" -n, --non-interactive don't prompt the user about anything\n"
" -X, --extra-args ARG append arguments from file ARG (one per line;\n"
" use \"-\" to read from standard input)\n"
" --config-dir ARG use ARG to override the config directory\n";
svn_error_clear(svn_cmdline_fputs(msg, stream, pool));
apr_pool_destroy(pool);
exit(exit_val);
}
static void
insufficient(apr_pool_t *pool) {
handle_error(svn_error_create(SVN_ERR_INCORRECT_PARAMS, NULL,
"insufficient arguments"),
pool);
}
int
main(int argc, const char **argv) {
apr_pool_t *pool = init("svnmucc");
apr_array_header_t *actions = apr_array_make(pool, 1,
sizeof(struct action *));
const char *anchor = NULL;
svn_error_t *err = SVN_NO_ERROR;
apr_getopt_t *getopt;
enum {
config_dir_opt = SVN_OPT_FIRST_LONGOPT_ID
};
const apr_getopt_option_t options[] = {
{"message", 'm', 1, ""},
{"file", 'F', 1, ""},
{"username", 'u', 1, ""},
{"password", 'p', 1, ""},
{"root-url", 'U', 1, ""},
{"revision", 'r', 1, ""},
{"extra-args", 'X', 1, ""},
{"help", 'h', 0, ""},
{"non-interactive", 'n', 0, ""},
{"config-dir", config_dir_opt, 1, ""},
{NULL, 0, 0, NULL}
};
const char *message = "committed using svnmucc";
const char *username = NULL, *password = NULL;
const char *root_url = NULL, *extra_args_file = NULL;
const char *config_dir = NULL;
svn_boolean_t non_interactive = FALSE;
svn_revnum_t base_revision = SVN_INVALID_REVNUM;
apr_array_header_t *action_args;
int i;
apr_getopt_init(&getopt, pool, argc, argv);
getopt->interleave = 1;
while (1) {
int opt;
const char *arg;
apr_status_t status = apr_getopt_long(getopt, options, &opt, &arg);
if (APR_STATUS_IS_EOF(status))
break;
if (status != APR_SUCCESS)
handle_error(svn_error_wrap_apr(status, "getopt failure"), pool);
switch(opt) {
case 'm':
err = svn_utf_cstring_to_utf8(&message, arg, pool);
if (err)
handle_error(err, pool);
break;
case 'F': {
const char *arg_utf8;
svn_stringbuf_t *contents;
err = svn_utf_cstring_to_utf8(&arg_utf8, arg, pool);
if (! err)
err = svn_stringbuf_from_file(&contents, arg, pool);
if (! err)
err = svn_utf_cstring_to_utf8(&message, contents->data, pool);
if (err)
handle_error(err, pool);
}
break;
case 'u':
username = apr_pstrdup(pool, arg);
break;
case 'p':
password = apr_pstrdup(pool, arg);
break;
case 'U':
err = svn_utf_cstring_to_utf8(&root_url, arg, pool);
if (err)
handle_error(err, pool);
root_url = svn_path_canonicalize(root_url, pool);
if (! svn_path_is_url(root_url))
handle_error(svn_error_createf(SVN_ERR_INCORRECT_PARAMS, NULL,
"'%s' is not a URL\n", root_url),
pool);
break;
case 'r': {
char *digits_end = NULL;
base_revision = strtol(arg, &digits_end, 10);
if ((! SVN_IS_VALID_REVNUM(base_revision))
|| (! digits_end)
|| *digits_end)
handle_error(svn_error_create(SVN_ERR_CL_ARG_PARSING_ERROR,
NULL, "Invalid revision number"),
pool);
}
break;
case 'X':
extra_args_file = apr_pstrdup(pool, arg);
break;
case 'n':
non_interactive = TRUE;
break;
case config_dir_opt:
err = svn_utf_cstring_to_utf8(&config_dir, arg, pool);
if (err)
handle_error(err, pool);
break;
case 'h':
usage(pool, EXIT_SUCCESS);
}
}
action_args = apr_array_make(pool, getopt->argc, sizeof(const char *));
while (getopt->ind < getopt->argc) {
const char *arg = getopt->argv[getopt->ind++];
if ((err = svn_utf_cstring_to_utf8(&(APR_ARRAY_PUSH(action_args,
const char *)),
arg, pool)))
handle_error(err, pool);
}
if (extra_args_file) {
const char *extra_args_file_utf8;
svn_stringbuf_t *contents, *contents_utf8;
err = svn_utf_cstring_to_utf8(&extra_args_file_utf8,
extra_args_file, pool);
if (! err)
err = svn_stringbuf_from_file2(&contents, extra_args_file_utf8, pool);
if (! err)
err = svn_utf_stringbuf_to_utf8(&contents_utf8, contents, pool);
if (err)
handle_error(err, pool);
svn_cstring_split_append(action_args, contents_utf8->data, "\n\r",
FALSE, pool);
}
for (i = 0; i < action_args->nelts; ) {
int j, num_url_args;
const char *action_string = APR_ARRAY_IDX(action_args, i, const char *);
struct action *action = apr_palloc(pool, sizeof(*action));
if (! strcmp(action_string, "mv"))
action->action = ACTION_MV;
else if (! strcmp(action_string, "cp"))
action->action = ACTION_CP;
else if (! strcmp(action_string, "mkdir"))
action->action = ACTION_MKDIR;
else if (! strcmp(action_string, "rm"))
action->action = ACTION_RM;
else if (! strcmp(action_string, "put"))
action->action = ACTION_PUT;
else if (! strcmp(action_string, "propset"))
action->action = ACTION_PROPSET;
else if (! strcmp(action_string, "propdel"))
action->action = ACTION_PROPDEL;
else
handle_error(svn_error_createf(SVN_ERR_INCORRECT_PARAMS, NULL,
"'%s' is not an action\n",
action_string), pool);
if (++i == action_args->nelts)
insufficient(pool);
if (action->action == ACTION_CP) {
const char *rev_str = APR_ARRAY_IDX(action_args, i, const char *);
if (strcmp(rev_str, "head") == 0)
action->rev = SVN_INVALID_REVNUM;
else if (strcmp(rev_str, "HEAD") == 0)
action->rev = SVN_INVALID_REVNUM;
else {
char *end;
action->rev = strtol(rev_str, &end, 0);
if (*end)
handle_error(svn_error_createf(SVN_ERR_INCORRECT_PARAMS, NULL,
"'%s' is not a revision\n",
rev_str), pool);
}
if (++i == action_args->nelts)
insufficient(pool);
} else {
action->rev = SVN_INVALID_REVNUM;
}
if (action->action == ACTION_PUT) {
action->path[1] = svn_path_canonicalize(APR_ARRAY_IDX(action_args,
i,
const char *),
pool);
if (++i == action_args->nelts)
insufficient(pool);
}
if ((action->action == ACTION_PROPSET)
|| (action->action == ACTION_PROPDEL)) {
action->prop_name = APR_ARRAY_IDX(action_args, i, const char *);
if (++i == action_args->nelts)
insufficient(pool);
if (action->action == ACTION_PROPDEL) {
action->prop_value = NULL;
} else {
action->prop_value = APR_ARRAY_IDX(action_args, i, const char *);
if (++i == action_args->nelts)
insufficient(pool);
}
}
if (action->action == ACTION_RM
|| action->action == ACTION_MKDIR
|| action->action == ACTION_PUT
|| action->action == ACTION_PROPSET
|| action->action == ACTION_PROPDEL)
num_url_args = 1;
else
num_url_args = 2;
for (j = 0; j < num_url_args; ++j) {
const char *url = APR_ARRAY_IDX(action_args, i, const char *);
if (root_url)
url = svn_path_join(root_url, url, pool);
else if (! svn_path_is_url(url))
handle_error(svn_error_createf(SVN_ERR_INCORRECT_PARAMS, NULL,
"'%s' is not a URL\n", url), pool);
url = svn_path_uri_from_iri(url, pool);
url = svn_path_uri_autoescape(url, pool);
url = svn_path_canonicalize(url, pool);
action->path[j] = url;
if (! (action->action == ACTION_CP && j == 0))
url = svn_path_dirname(url, pool);
if (! anchor)
anchor = url;
else
anchor = svn_path_get_longest_ancestor(anchor, url, pool);
if ((++i == action_args->nelts) && (j >= num_url_args))
insufficient(pool);
}
APR_ARRAY_PUSH(actions, struct action *) = action;
}
if (! actions->nelts)
usage(pool, EXIT_FAILURE);
if ((err = execute(actions, anchor, message, username, password,
config_dir, non_interactive, base_revision, pool)))
handle_error(err, pool);
svn_pool_destroy(pool);
return EXIT_SUCCESS;
}
