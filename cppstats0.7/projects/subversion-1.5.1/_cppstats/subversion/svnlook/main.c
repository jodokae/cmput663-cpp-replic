#include <assert.h>
#include <stdlib.h>
#include <apr_general.h>
#include <apr_pools.h>
#include <apr_time.h>
#include <apr_file_io.h>
#include <apr_signal.h>
#define APR_WANT_STDIO
#define APR_WANT_STRFUNC
#include <apr_want.h>
#include "svn_cmdline.h"
#include "svn_types.h"
#include "svn_pools.h"
#include "svn_error.h"
#include "svn_error_codes.h"
#include "svn_path.h"
#include "svn_repos.h"
#include "svn_fs.h"
#include "svn_time.h"
#include "svn_utf.h"
#include "svn_subst.h"
#include "svn_opt.h"
#include "svn_props.h"
#include "svn_diff.h"
#include "svn_private_config.h"
static svn_opt_subcommand_t
subcommand_author,
subcommand_cat,
subcommand_changed,
subcommand_date,
subcommand_diff,
subcommand_dirschanged,
subcommand_help,
subcommand_history,
subcommand_info,
subcommand_lock,
subcommand_log,
subcommand_pget,
subcommand_plist,
subcommand_tree,
subcommand_uuid,
subcommand_youngest;
enum {
svnlook__version = SVN_OPT_FIRST_LONGOPT_ID,
svnlook__show_ids,
svnlook__no_diff_deleted,
svnlook__no_diff_added,
svnlook__diff_copy_from,
svnlook__revprop_opt,
svnlook__full_paths,
svnlook__copy_info
};
static const apr_getopt_option_t options_table[] = {
{
NULL, '?', 0,
N_("show help on a subcommand")
},
{
"copy-info", svnlook__copy_info, 0,
N_("show details for copies")
},
{
"diff-copy-from", svnlook__diff_copy_from, 0,
N_("print differences against the copy source")
},
{
"full-paths", svnlook__full_paths, 0,
N_("show full paths instead of indenting them")
},
{
"help", 'h', 0,
N_("show help on a subcommand")
},
{
"limit", 'l', 1,
N_("maximum number of history entries")
},
{
"no-diff-added", svnlook__no_diff_added, 0,
N_("do not print differences for added files")
},
{
"no-diff-deleted", svnlook__no_diff_deleted, 0,
N_("do not print differences for deleted files")
},
{
"non-recursive", 'N', 0,
N_("operate on single directory only")
},
{
"revision", 'r', 1,
N_("specify revision number ARG")
},
{
"revprop", svnlook__revprop_opt, 0,
N_("operate on a revision property (use with -r or -t)")
},
{
"show-ids", svnlook__show_ids, 0,
N_("show node revision ids for each path")
},
{
"transaction", 't', 1,
N_("specify transaction name ARG")
},
{
"verbose", 'v', 0,
N_("be verbose")
},
{
"version", svnlook__version, 0,
N_("show program version information")
},
#if !defined(AS400)
{
"extensions", 'x', 1,
N_("Default: '-u'. When Subversion is invoking an\n"
" "
" external diff program, ARG is simply passed along\n"
" "
" to the program. But when Subversion is using its\n"
" "
" default internal diff implementation, or when\n"
" "
" Subversion is displaying blame annotations, ARG\n"
" "
" could be any of the following:\n"
" "
" -u (--unified):\n"
" "
" Output 3 lines of unified context.\n"
" "
" -b (--ignore-space-change):\n"
" "
" Ignore changes in the amount of white space.\n"
" "
" -w (--ignore-all-space):\n"
" "
" Ignore all white space.\n"
" "
" --ignore-eol-style:\n"
" "
" Ignore changes in EOL style")
},
#endif
{0, 0, 0, 0}
};
static const svn_opt_subcommand_desc_t cmd_table[] = {
{
"author", subcommand_author, {0},
N_("usage: svnlook author REPOS_PATH\n\n"
"Print the author.\n"),
{'r', 't'}
},
{
"cat", subcommand_cat, {0},
N_("usage: svnlook cat REPOS_PATH FILE_PATH\n\n"
"Print the contents of a file. Leading '/' on FILE_PATH is optional.\n"),
{'r', 't'}
},
{
"changed", subcommand_changed, {0},
N_("usage: svnlook changed REPOS_PATH\n\n"
"Print the paths that were changed.\n"),
{'r', 't', svnlook__copy_info}
},
{
"date", subcommand_date, {0},
N_("usage: svnlook date REPOS_PATH\n\n"
"Print the datestamp.\n"),
{'r', 't'}
},
{
"diff", subcommand_diff, {0},
N_("usage: svnlook diff REPOS_PATH\n\n"
"Print GNU-style diffs of changed files and properties.\n"),
{
'r', 't', svnlook__no_diff_deleted, svnlook__no_diff_added,
svnlook__diff_copy_from, 'x'
}
},
{
"dirs-changed", subcommand_dirschanged, {0},
N_("usage: svnlook dirs-changed REPOS_PATH\n\n"
"Print the directories that were themselves changed (property edits)\n"
"or whose file children were changed.\n"),
{'r', 't'}
},
{
"help", subcommand_help, {"?", "h"},
N_("usage: svnlook help [SUBCOMMAND...]\n\n"
"Describe the usage of this program or its subcommands.\n"),
{0}
},
{
"history", subcommand_history, {0},
N_("usage: svnlook history REPOS_PATH [PATH_IN_REPOS]\n\n"
"Print information about the history of a path in the repository (or\n"
"the root directory if no path is supplied).\n"),
{'r', svnlook__show_ids, 'l'}
},
{
"info", subcommand_info, {0},
N_("usage: svnlook info REPOS_PATH\n\n"
"Print the author, datestamp, log message size, and log message.\n"),
{'r', 't'}
},
{
"lock", subcommand_lock, {0},
N_("usage: svnlook lock REPOS_PATH PATH_IN_REPOS\n\n"
"If a lock exists on a path in the repository, describe it.\n"),
{0}
},
{
"log", subcommand_log, {0},
N_("usage: svnlook log REPOS_PATH\n\n"
"Print the log message.\n"),
{'r', 't'}
},
{
"propget", subcommand_pget, {"pget", "pg"},
N_("usage: svnlook propget REPOS_PATH PROPNAME [PATH_IN_REPOS]\n\n"
"Print the raw value of a property on a path in the repository.\n"
"With --revprop, prints the raw value of a revision property.\n"),
{'r', 't', svnlook__revprop_opt}
},
{
"proplist", subcommand_plist, {"plist", "pl"},
N_("usage: svnlook proplist REPOS_PATH [PATH_IN_REPOS]\n\n"
"List the properties of a path in the repository, or\n"
"with the --revprop option, revision properties.\n"
"With -v, show the property values too.\n"),
{'r', 't', 'v', svnlook__revprop_opt}
},
{
"tree", subcommand_tree, {0},
N_("usage: svnlook tree REPOS_PATH [PATH_IN_REPOS]\n\n"
"Print the tree, starting at PATH_IN_REPOS (if supplied, at the root\n"
"of the tree otherwise), optionally showing node revision ids.\n"),
{'r', 't', 'N', svnlook__show_ids, svnlook__full_paths}
},
{
"uuid", subcommand_uuid, {0},
N_("usage: svnlook uuid REPOS_PATH\n\n"
"Print the repository's UUID.\n"),
{0}
},
{
"youngest", subcommand_youngest, {0},
N_("usage: svnlook youngest REPOS_PATH\n\n"
"Print the youngest revision number.\n"),
{0}
},
{ NULL, NULL, {0}, NULL, {0} }
};
struct svnlook_opt_state {
const char *repos_path;
const char *arg1;
const char *arg2;
svn_revnum_t rev;
const char *txn;
svn_boolean_t version;
svn_boolean_t show_ids;
apr_size_t limit;
svn_boolean_t help;
svn_boolean_t no_diff_deleted;
svn_boolean_t no_diff_added;
svn_boolean_t diff_copy_from;
svn_boolean_t verbose;
svn_boolean_t revprop;
svn_boolean_t full_paths;
svn_boolean_t copy_info;
svn_boolean_t non_recursive;
const char *extensions;
};
typedef struct svnlook_ctxt_t {
svn_repos_t *repos;
svn_fs_t *fs;
svn_boolean_t is_revision;
svn_boolean_t show_ids;
apr_size_t limit;
svn_boolean_t no_diff_deleted;
svn_boolean_t no_diff_added;
svn_boolean_t diff_copy_from;
svn_boolean_t full_paths;
svn_boolean_t copy_info;
svn_revnum_t rev_id;
svn_fs_txn_t *txn;
const char *txn_name ;
const apr_array_header_t *diff_options;
} svnlook_ctxt_t;
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
{ "svn_repos", svn_repos_version },
{ "svn_fs", svn_fs_version },
{ "svn_delta", svn_delta_version },
{ "svn_diff", svn_diff_version },
{ NULL, NULL }
};
SVN_VERSION_DEFINE(my_version);
return svn_ver_check_list(&my_version, checklist);
}
static svn_error_t *
get_property(svn_string_t **prop_value,
svnlook_ctxt_t *c,
const char *prop_name,
apr_pool_t *pool) {
svn_string_t *raw_value;
if (! c->is_revision)
SVN_ERR(svn_fs_txn_prop(&raw_value, c->txn, prop_name, pool));
else
SVN_ERR(svn_fs_revision_prop(&raw_value, c->fs, c->rev_id,
prop_name, pool));
*prop_value = raw_value;
return SVN_NO_ERROR;
}
static svn_error_t *
get_root(svn_fs_root_t **root,
svnlook_ctxt_t *c,
apr_pool_t *pool) {
if (c->is_revision) {
if (! SVN_IS_VALID_REVNUM(c->rev_id))
SVN_ERR(svn_fs_youngest_rev(&(c->rev_id), c->fs, pool));
SVN_ERR(svn_fs_revision_root(root, c->fs, c->rev_id, pool));
} else {
SVN_ERR(svn_fs_txn_root(root, c->txn, pool));
}
return SVN_NO_ERROR;
}
static svn_error_t *
generate_delta_tree(svn_repos_node_t **tree,
svn_repos_t *repos,
svn_fs_root_t *root,
svn_revnum_t base_rev,
svn_boolean_t use_copy_history,
apr_pool_t *pool) {
svn_fs_root_t *base_root;
const svn_delta_editor_t *editor;
void *edit_baton;
apr_pool_t *edit_pool = svn_pool_create(pool);
svn_fs_t *fs = svn_repos_fs(repos);
SVN_ERR(svn_fs_revision_root(&base_root, fs, base_rev, pool));
SVN_ERR(svn_repos_node_editor(&editor, &edit_baton, repos,
base_root, root, pool, edit_pool));
SVN_ERR(svn_repos_replay2(root, "", SVN_INVALID_REVNUM, FALSE,
editor, edit_baton, NULL, NULL, edit_pool));
*tree = svn_repos_node_from_baton(edit_baton);
svn_pool_destroy(edit_pool);
return SVN_NO_ERROR;
}
static svn_error_t *
print_dirs_changed_tree(svn_repos_node_t *node,
const char *path ,
apr_pool_t *pool) {
svn_repos_node_t *tmp_node;
int print_me = 0;
const char *full_path;
apr_pool_t *subpool;
SVN_ERR(check_cancel(NULL));
if (! node)
return SVN_NO_ERROR;
if (node->kind != svn_node_dir)
return SVN_NO_ERROR;
if (node->prop_mod)
print_me = 1;
if (! print_me) {
tmp_node = node->child;
if (tmp_node) {
if ((tmp_node->kind == svn_node_file)
|| (tmp_node->text_mod)
|| (tmp_node->action == 'A')
|| (tmp_node->action == 'D')) {
print_me = 1;
}
while (tmp_node->sibling && (! print_me )) {
tmp_node = tmp_node->sibling;
if ((tmp_node->kind == svn_node_file)
|| (tmp_node->text_mod)
|| (tmp_node->action == 'A')
|| (tmp_node->action == 'D')) {
print_me = 1;
}
}
}
}
if (print_me) {
SVN_ERR(svn_cmdline_printf(pool, "%s/\n", path));
}
tmp_node = node->child;
if (! tmp_node)
return SVN_NO_ERROR;
subpool = svn_pool_create(pool);
full_path = svn_path_join(path, tmp_node->name, subpool);
SVN_ERR(print_dirs_changed_tree(tmp_node, full_path, subpool));
while (tmp_node->sibling) {
svn_pool_clear(subpool);
tmp_node = tmp_node->sibling;
full_path = svn_path_join(path, tmp_node->name, subpool);
SVN_ERR(print_dirs_changed_tree(tmp_node, full_path, subpool));
}
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
static svn_error_t *
print_changed_tree(svn_repos_node_t *node,
const char *path ,
svn_boolean_t copy_info,
apr_pool_t *pool) {
const char *full_path;
char status[4] = "_ ";
int print_me = 1;
apr_pool_t *subpool;
SVN_ERR(check_cancel(NULL));
if (! node)
return SVN_NO_ERROR;
if (node->action == 'A') {
status[0] = 'A';
if (copy_info && node->copyfrom_path)
status[2] = '+';
} else if (node->action == 'D')
status[0] = 'D';
else if (node->action == 'R') {
if ((! node->text_mod) && (! node->prop_mod))
print_me = 0;
if (node->text_mod)
status[0] = 'U';
if (node->prop_mod)
status[1] = 'U';
} else
print_me = 0;
if (print_me) {
SVN_ERR(svn_cmdline_printf(pool, "%s %s%s\n",
status,
path,
node->kind == svn_node_dir ? "/" : ""));
if (copy_info && node->copyfrom_path)
SVN_ERR(svn_cmdline_printf(pool, " (from %s%s:r%ld)\n",
(node->copyfrom_path[0] == '/'
? node->copyfrom_path + 1
: node->copyfrom_path),
(node->kind == svn_node_dir ? "/" : ""),
node->copyfrom_rev));
}
node = node->child;
if (! node)
return SVN_NO_ERROR;
subpool = svn_pool_create(pool);
full_path = svn_path_join(path, node->name, subpool);
SVN_ERR(print_changed_tree(node, full_path, copy_info, subpool));
while (node->sibling) {
svn_pool_clear(subpool);
node = node->sibling;
full_path = svn_path_join(path, node->name, subpool);
SVN_ERR(print_changed_tree(node, full_path, copy_info, subpool));
}
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
static svn_error_t *
dump_contents(apr_file_t *fh,
svn_fs_root_t *root,
const char *path ,
apr_pool_t *pool) {
svn_stream_t *contents, *file_stream;
SVN_ERR(svn_fs_file_contents(&contents, root, path, pool));
file_stream = svn_stream_from_aprfile(fh, pool);
SVN_ERR(svn_stream_copy(contents, file_stream, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
prepare_tmpfiles(const char **tmpfile1,
const char **tmpfile2,
svn_boolean_t *is_binary,
svn_fs_root_t *root1,
const char *path1,
svn_fs_root_t *root2,
const char *path2,
const char *tmpdir,
apr_pool_t *pool) {
svn_string_t *mimetype;
apr_file_t *fh;
*tmpfile1 = NULL;
*tmpfile2 = NULL;
*is_binary = FALSE;
assert(path1 && path2);
if (root1) {
SVN_ERR(svn_fs_node_prop(&mimetype, root1, path1,
SVN_PROP_MIME_TYPE, pool));
if (mimetype && svn_mime_type_is_binary(mimetype->data)) {
*is_binary = TRUE;
return SVN_NO_ERROR;
}
}
if (root2) {
SVN_ERR(svn_fs_node_prop(&mimetype, root2, path2,
SVN_PROP_MIME_TYPE, pool));
if (mimetype && svn_mime_type_is_binary(mimetype->data)) {
*is_binary = TRUE;
return SVN_NO_ERROR;
}
}
SVN_ERR(svn_io_open_unique_file2(&fh, tmpfile2,
apr_psprintf(pool, "%s/diff", tmpdir),
".tmp", svn_io_file_del_none, pool));
if (root2)
SVN_ERR(dump_contents(fh, root2, path2, pool));
apr_file_close(fh);
SVN_ERR(svn_io_open_unique_file2(&fh, tmpfile1, *tmpfile2,
".tmp", svn_io_file_del_none, pool));
if (root1)
SVN_ERR(dump_contents(fh, root1, path1, pool));
apr_file_close(fh);
return SVN_NO_ERROR;
}
static svn_error_t *
generate_label(const char **label,
svn_fs_root_t *root,
const char *path,
apr_pool_t *pool) {
svn_string_t *date;
const char *datestr;
const char *name = NULL;
svn_revnum_t rev = SVN_INVALID_REVNUM;
if (root) {
svn_fs_t *fs = svn_fs_root_fs(root);
if (svn_fs_is_revision_root(root)) {
rev = svn_fs_revision_root_revision(root);
SVN_ERR(svn_fs_revision_prop(&date, fs, rev,
SVN_PROP_REVISION_DATE, pool));
} else {
svn_fs_txn_t *txn;
name = svn_fs_txn_root_name(root, pool);
SVN_ERR(svn_fs_open_txn(&txn, fs, name, pool));
SVN_ERR(svn_fs_txn_prop(&date, txn, SVN_PROP_REVISION_DATE, pool));
}
} else {
rev = 0;
date = NULL;
}
if (date)
datestr = apr_psprintf(pool, "%.10s %.8s UTC", date->data, date->data + 11);
else
datestr = " ";
if (name)
*label = apr_psprintf(pool, "%s\t%s (txn %s)",
path, datestr, name);
else
*label = apr_psprintf(pool, "%s\t%s (rev %ld)",
path, datestr, rev);
return SVN_NO_ERROR;
}
static const char equal_string[] =
"===================================================================";
static const char under_string[] =
"___________________________________________________________________";
static svn_error_t *
display_prop_diffs(const apr_array_header_t *prop_diffs,
apr_hash_t *orig_props,
const char *path,
apr_pool_t *pool) {
int i;
SVN_ERR(svn_cmdline_printf(pool, "\nProperty changes on: %s\n%s\n",
path, under_string));
for (i = 0; i < prop_diffs->nelts; i++) {
const char *header_fmt;
const svn_string_t *orig_value;
const svn_prop_t *pc = &APR_ARRAY_IDX(prop_diffs, i, svn_prop_t);
SVN_ERR(check_cancel(NULL));
if (orig_props)
orig_value = apr_hash_get(orig_props, pc->name, APR_HASH_KEY_STRING);
else
orig_value = NULL;
if (! orig_value)
header_fmt = _("Added: %s\n");
else if (! pc->value)
header_fmt = _("Deleted: %s\n");
else
header_fmt = _("Modified: %s\n");
SVN_ERR(svn_cmdline_printf(pool, header_fmt, pc->name));
{
svn_boolean_t val_to_utf8 = svn_prop_is_svn_prop(pc->name);
const char *printable_val;
if (orig_value != NULL) {
if (val_to_utf8)
SVN_ERR(svn_cmdline_cstring_from_utf8(&printable_val,
orig_value->data, pool));
else
printable_val = orig_value->data;
printf(" - %s\n", printable_val);
}
if (pc->value != NULL) {
if (val_to_utf8)
SVN_ERR(svn_cmdline_cstring_from_utf8
(&printable_val, pc->value->data, pool));
else
printable_val = pc->value->data;
printf(" + %s\n", printable_val);
}
}
}
SVN_ERR(svn_cmdline_printf(pool, "\n"));
return svn_cmdline_fflush(stdout);
}
static svn_error_t *
print_diff_tree(svn_fs_root_t *root,
svn_fs_root_t *base_root,
svn_repos_node_t *node,
const char *path ,
const char *base_path ,
const svnlook_ctxt_t *c,
const char *tmpdir,
apr_pool_t *pool) {
const char *orig_path = NULL, *new_path = NULL;
svn_boolean_t do_diff = FALSE;
svn_boolean_t orig_empty = FALSE;
svn_boolean_t is_copy = FALSE;
svn_boolean_t binary = FALSE;
apr_pool_t *subpool;
svn_stringbuf_t *header;
SVN_ERR(check_cancel(NULL));
if (! node)
return SVN_NO_ERROR;
header = svn_stringbuf_create("", pool);
if ((SVN_IS_VALID_REVNUM(node->copyfrom_rev))
&& (node->copyfrom_path != NULL)) {
is_copy = TRUE;
if (node->copyfrom_path[0] == '/')
base_path = apr_pstrdup(pool, node->copyfrom_path + 1);
else
base_path = apr_pstrdup(pool, node->copyfrom_path);
svn_stringbuf_appendcstr
(header,
apr_psprintf(pool, _("Copied: %s (from rev %ld, %s)\n"),
path, node->copyfrom_rev, base_path));
SVN_ERR(svn_fs_revision_root(&base_root,
svn_fs_root_fs(base_root),
node->copyfrom_rev, pool));
}
if (node->kind == svn_node_file) {
if (node->action == 'R' && node->text_mod) {
do_diff = TRUE;
SVN_ERR(prepare_tmpfiles(&orig_path, &new_path, &binary,
base_root, base_path, root, path,
tmpdir, pool));
} else if (c->diff_copy_from && node->action == 'A' && is_copy) {
if (node->text_mod) {
do_diff = TRUE;
SVN_ERR(prepare_tmpfiles(&orig_path, &new_path, &binary,
base_root, base_path, root, path,
tmpdir, pool));
}
} else if (! c->no_diff_added && node->action == 'A') {
do_diff = TRUE;
orig_empty = TRUE;
SVN_ERR(prepare_tmpfiles(&orig_path, &new_path, &binary,
NULL, base_path, root, path,
tmpdir, pool));
} else if (! c->no_diff_deleted && node->action == 'D') {
do_diff = TRUE;
SVN_ERR(prepare_tmpfiles(&orig_path, &new_path, &binary,
base_root, base_path, NULL, path,
tmpdir, pool));
}
if (header->len == 0
&& (node->action != 'R' || node->text_mod)) {
svn_stringbuf_appendcstr
(header, apr_psprintf(pool, "%s: %s\n",
((node->action == 'A') ? _("Added") :
((node->action == 'D') ? _("Deleted") :
((node->action == 'R') ? _("Modified")
: _("Index")))),
path));
}
}
if (do_diff) {
svn_stringbuf_appendcstr(header, equal_string);
svn_stringbuf_appendcstr(header, "\n");
if (binary) {
svn_stringbuf_appendcstr(header, _("(Binary files differ)\n\n"));
SVN_ERR(svn_cmdline_printf(pool, header->data));
} else {
svn_diff_t *diff;
svn_diff_file_options_t *opts = svn_diff_file_options_create(pool);
if (c->diff_options)
SVN_ERR(svn_diff_file_options_parse(opts, c->diff_options, pool));
SVN_ERR(svn_diff_file_diff_2(&diff, orig_path,
new_path, opts, pool));
if (svn_diff_contains_diffs(diff)) {
svn_stream_t *ostream;
const char *orig_label, *new_label;
SVN_ERR(svn_cmdline_printf(pool, header->data));
SVN_ERR(svn_stream_for_stdout(&ostream, pool));
if (orig_empty)
SVN_ERR(generate_label(&orig_label, NULL, path, pool));
else
SVN_ERR(generate_label(&orig_label, base_root,
base_path, pool));
SVN_ERR(generate_label(&new_label, root, path, pool));
SVN_ERR(svn_diff_file_output_unified2
(ostream, diff, orig_path, new_path,
orig_label, new_label,
svn_cmdline_output_encoding(pool), pool));
SVN_ERR(svn_stream_close(ostream));
SVN_ERR(svn_cmdline_printf(pool, "\n"));
}
}
SVN_ERR(svn_cmdline_fflush(stdout));
}
if (orig_path)
SVN_ERR(svn_io_remove_file(orig_path, pool));
if (new_path)
SVN_ERR(svn_io_remove_file(new_path, pool));
if ((node->prop_mod) && (node->action != 'D')) {
apr_hash_t *local_proptable;
apr_hash_t *base_proptable;
apr_array_header_t *propchanges, *props;
SVN_ERR(svn_fs_node_proplist(&local_proptable, root, path, pool));
if (c->diff_copy_from && node->action == 'A' && is_copy)
SVN_ERR(svn_fs_node_proplist(&base_proptable, base_root,
base_path, pool));
else if (node->action == 'A')
base_proptable = apr_hash_make(pool);
else
SVN_ERR(svn_fs_node_proplist(&base_proptable, base_root,
base_path, pool));
SVN_ERR(svn_prop_diffs(&propchanges, local_proptable,
base_proptable, pool));
SVN_ERR(svn_categorize_props(propchanges, NULL, NULL, &props, pool));
if (props->nelts > 0)
SVN_ERR(display_prop_diffs(props, base_proptable, path, pool));
}
node = node->child;
if (! node)
return SVN_NO_ERROR;
subpool = svn_pool_create(pool);
SVN_ERR(print_diff_tree(root, base_root, node,
svn_path_join(path, node->name, subpool),
svn_path_join(base_path, node->name, subpool),
c, tmpdir, subpool));
while (node->sibling) {
svn_pool_clear(subpool);
node = node->sibling;
SVN_ERR(print_diff_tree(root, base_root, node,
svn_path_join(path, node->name, subpool),
svn_path_join(base_path, node->name, subpool),
c, tmpdir, subpool));
}
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
static svn_error_t *
print_tree(svn_fs_root_t *root,
const char *path ,
const svn_fs_id_t *id,
svn_boolean_t is_dir,
int indentation,
svn_boolean_t show_ids,
svn_boolean_t full_paths,
svn_boolean_t recurse,
apr_pool_t *pool) {
apr_pool_t *subpool;
int i;
apr_hash_t *entries;
apr_hash_index_t *hi;
SVN_ERR(check_cancel(NULL));
if (!full_paths)
for (i = 0; i < indentation; i++)
SVN_ERR(svn_cmdline_fputs(" ", stdout, pool));
SVN_ERR(svn_cmdline_printf(pool, "%s%s",
full_paths ? path : svn_path_basename(path,
pool),
is_dir && strcmp(path, "/") ? "/" : ""));
if (show_ids) {
svn_string_t *unparsed_id = NULL;
if (id)
unparsed_id = svn_fs_unparse_id(id, pool);
SVN_ERR(svn_cmdline_printf(pool, " <%s>",
unparsed_id
? unparsed_id->data
: _("unknown")));
}
SVN_ERR(svn_cmdline_fputs("\n", stdout, pool));
if (! is_dir)
return SVN_NO_ERROR;
if (recurse || (indentation == 0)) {
SVN_ERR(svn_fs_dir_entries(&entries, root, path, pool));
subpool = svn_pool_create(pool);
for (hi = apr_hash_first(pool, entries); hi; hi = apr_hash_next(hi)) {
void *val;
svn_fs_dirent_t *entry;
svn_pool_clear(subpool);
apr_hash_this(hi, NULL, NULL, &val);
entry = val;
SVN_ERR(print_tree(root, svn_path_join(path, entry->name, pool),
entry->id, (entry->kind == svn_node_dir),
indentation + 1, show_ids, full_paths,
recurse, subpool));
}
svn_pool_destroy(subpool);
}
return SVN_NO_ERROR;
}
static svn_error_t *
do_log(svnlook_ctxt_t *c, svn_boolean_t print_size, apr_pool_t *pool) {
svn_string_t *prop_value;
const char *prop_value_eol, *prop_value_native;
svn_stream_t *stream;
svn_error_t *err;
apr_size_t len;
SVN_ERR(get_property(&prop_value, c, SVN_PROP_REVISION_LOG, pool));
if (! (prop_value && prop_value->data)) {
SVN_ERR(svn_cmdline_printf(pool, "%s\n", print_size ? "0" : ""));
return SVN_NO_ERROR;
}
SVN_ERR(svn_subst_translate_cstring2(prop_value->data, &prop_value_eol,
APR_EOL_STR, TRUE,
NULL, FALSE, pool));
err = svn_cmdline_cstring_from_utf8(&prop_value_native, prop_value_eol,
pool);
if (err) {
svn_error_clear(err);
prop_value_native = svn_cmdline_cstring_from_utf8_fuzzy(prop_value_eol,
pool);
}
len = strlen(prop_value_native);
if (print_size)
SVN_ERR(svn_cmdline_printf(pool, "%" APR_SIZE_T_FMT "\n", len));
SVN_ERR(svn_cmdline_fflush(stdout));
SVN_ERR(svn_stream_for_stdout(&stream, pool));
SVN_ERR(svn_stream_write(stream, prop_value_native, &len));
SVN_ERR(svn_stream_close(stream));
SVN_ERR(svn_cmdline_fputs("\n", stdout, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
do_date(svnlook_ctxt_t *c, apr_pool_t *pool) {
svn_string_t *prop_value;
SVN_ERR(get_property(&prop_value, c, SVN_PROP_REVISION_DATE, pool));
if (prop_value && prop_value->data) {
apr_time_t aprtime;
const char *time_utf8;
SVN_ERR(svn_time_from_cstring(&aprtime, prop_value->data, pool));
time_utf8 = svn_time_to_human_cstring(aprtime, pool);
SVN_ERR(svn_cmdline_printf(pool, "%s", time_utf8));
}
SVN_ERR(svn_cmdline_printf(pool, "\n"));
return SVN_NO_ERROR;
}
static svn_error_t *
do_author(svnlook_ctxt_t *c, apr_pool_t *pool) {
svn_string_t *prop_value;
SVN_ERR(get_property(&prop_value, c,
SVN_PROP_REVISION_AUTHOR, pool));
if (prop_value && prop_value->data)
SVN_ERR(svn_cmdline_printf(pool, "%s", prop_value->data));
SVN_ERR(svn_cmdline_printf(pool, "\n"));
return SVN_NO_ERROR;
}
static svn_error_t *
do_dirs_changed(svnlook_ctxt_t *c, apr_pool_t *pool) {
svn_fs_root_t *root;
svn_revnum_t base_rev_id;
svn_repos_node_t *tree;
SVN_ERR(get_root(&root, c, pool));
if (c->is_revision)
base_rev_id = c->rev_id - 1;
else
base_rev_id = svn_fs_txn_base_revision(c->txn);
if (! SVN_IS_VALID_REVNUM(base_rev_id))
return svn_error_createf
(SVN_ERR_FS_NO_SUCH_REVISION, NULL,
_("Transaction '%s' is not based on a revision; how odd"),
c->txn_name);
SVN_ERR(generate_delta_tree(&tree, c->repos, root, base_rev_id,
TRUE, pool));
if (tree)
SVN_ERR(print_dirs_changed_tree(tree, "", pool));
return SVN_NO_ERROR;
}
static svn_error_t *
verify_path(svn_node_kind_t *kind,
svn_fs_root_t *root,
const char *path,
apr_pool_t *pool) {
SVN_ERR(svn_fs_check_path(kind, root, path, pool));
if (*kind == svn_node_none) {
if (svn_path_is_url(path))
return svn_error_createf
(SVN_ERR_FS_NOT_FOUND, NULL,
_("'%s' is a URL, probably should be a path"), path);
else
return svn_error_createf
(SVN_ERR_FS_NOT_FOUND, NULL, _("Path '%s' does not exist"), path);
}
return SVN_NO_ERROR;
}
static svn_error_t *
do_cat(svnlook_ctxt_t *c, const char *path, apr_pool_t *pool) {
svn_fs_root_t *root;
svn_node_kind_t kind;
svn_stream_t *fstream, *stdout_stream;
char *buf = apr_palloc(pool, SVN__STREAM_CHUNK_SIZE);
apr_size_t len = SVN__STREAM_CHUNK_SIZE;
SVN_ERR(get_root(&root, c, pool));
SVN_ERR(verify_path(&kind, root, path, pool));
if (kind != svn_node_file)
return svn_error_createf
(SVN_ERR_FS_NOT_FILE, NULL, _("Path '%s' is not a file"), path);
SVN_ERR(svn_fs_file_contents(&fstream, root, path, pool));
SVN_ERR(svn_stream_for_stdout(&stdout_stream, pool));
do {
SVN_ERR(check_cancel(NULL));
SVN_ERR(svn_stream_read(fstream, buf, &len));
SVN_ERR(svn_stream_write(stdout_stream, buf, &len));
} while (len == SVN__STREAM_CHUNK_SIZE);
return SVN_NO_ERROR;
}
static svn_error_t *
do_changed(svnlook_ctxt_t *c, apr_pool_t *pool) {
svn_fs_root_t *root;
svn_revnum_t base_rev_id;
svn_repos_node_t *tree;
SVN_ERR(get_root(&root, c, pool));
if (c->is_revision)
base_rev_id = c->rev_id - 1;
else
base_rev_id = svn_fs_txn_base_revision(c->txn);
if (! SVN_IS_VALID_REVNUM(base_rev_id))
return svn_error_createf
(SVN_ERR_FS_NO_SUCH_REVISION, NULL,
_("Transaction '%s' is not based on a revision; how odd"),
c->txn_name);
SVN_ERR(generate_delta_tree(&tree, c->repos, root, base_rev_id,
TRUE, pool));
if (tree)
SVN_ERR(print_changed_tree(tree, "", c->copy_info, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
do_diff(svnlook_ctxt_t *c, apr_pool_t *pool) {
svn_fs_root_t *root, *base_root;
svn_revnum_t base_rev_id;
svn_repos_node_t *tree;
SVN_ERR(get_root(&root, c, pool));
if (c->is_revision)
base_rev_id = c->rev_id - 1;
else
base_rev_id = svn_fs_txn_base_revision(c->txn);
if (! SVN_IS_VALID_REVNUM(base_rev_id))
return svn_error_createf
(SVN_ERR_FS_NO_SUCH_REVISION, NULL,
_("Transaction '%s' is not based on a revision; how odd"),
c->txn_name);
SVN_ERR(generate_delta_tree(&tree, c->repos, root, base_rev_id,
TRUE, pool));
if (tree) {
const char *tmpdir;
SVN_ERR(svn_fs_revision_root(&base_root, c->fs, base_rev_id, pool));
SVN_ERR(svn_io_temp_dir(&tmpdir, pool));
SVN_ERR(print_diff_tree(root, base_root, tree, "", "",
c, tmpdir, pool));
}
return SVN_NO_ERROR;
}
struct print_history_baton {
svn_fs_t *fs;
svn_boolean_t show_ids;
apr_size_t limit;
apr_size_t count;
};
static svn_error_t *
print_history(void *baton,
const char *path,
svn_revnum_t revision,
apr_pool_t *pool) {
struct print_history_baton *phb = baton;
SVN_ERR(check_cancel(NULL));
if (phb->show_ids) {
const svn_fs_id_t *node_id;
svn_fs_root_t *rev_root;
svn_string_t *id_string;
SVN_ERR(svn_fs_revision_root(&rev_root, phb->fs, revision, pool));
SVN_ERR(svn_fs_node_id(&node_id, rev_root, path, pool));
id_string = svn_fs_unparse_id(node_id, pool);
SVN_ERR(svn_cmdline_printf(pool, "%8ld %s <%s>\n",
revision, path, id_string->data));
} else {
SVN_ERR(svn_cmdline_printf(pool, "%8ld %s\n", revision, path));
}
if (phb->limit > 0) {
phb->count++;
if (phb->count >= phb->limit)
return svn_error_create(SVN_ERR_CEASE_INVOCATION, NULL,
"History item limit reached");
}
return SVN_NO_ERROR;
}
static svn_error_t *
do_history(svnlook_ctxt_t *c,
const char *path,
apr_pool_t *pool) {
struct print_history_baton args;
if (c->show_ids) {
SVN_ERR(svn_cmdline_printf(pool, _("REVISION PATH <ID>\n"
"-------- ---------\n")));
} else {
SVN_ERR(svn_cmdline_printf(pool, _("REVISION PATH\n"
"-------- ----\n")));
}
args.fs = c->fs;
args.show_ids = c->show_ids;
args.limit = c->limit;
args.count = 0;
SVN_ERR(svn_repos_history2(c->fs, path, print_history, &args,
NULL, NULL, 0, c->rev_id, TRUE, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
do_pget(svnlook_ctxt_t *c,
const char *propname,
const char *path,
apr_pool_t *pool) {
svn_fs_root_t *root;
svn_string_t *prop;
svn_node_kind_t kind;
svn_stream_t *stdout_stream;
apr_size_t len;
SVN_ERR(get_root(&root, c, pool));
if (path != NULL) {
SVN_ERR(verify_path(&kind, root, path, pool));
SVN_ERR(svn_fs_node_prop(&prop, root, path, propname, pool));
} else
SVN_ERR(get_property(&prop, c, propname, pool));
if (prop == NULL) {
const char *err_msg;
if (path == NULL) {
err_msg = apr_psprintf(pool,
_("Property '%s' not found on revision %ld"),
propname, c->rev_id);
} else {
if (SVN_IS_VALID_REVNUM(c->rev_id))
err_msg = apr_psprintf(pool,
_("Property '%s' not found on path '%s' "
"in revision %ld"),
propname, path, c->rev_id);
else
err_msg = apr_psprintf(pool,
_("Property '%s' not found on path '%s' "
"in transaction %s"),
propname, path, c->txn_name);
}
return svn_error_create(SVN_ERR_PROPERTY_NOT_FOUND, NULL, err_msg);
}
SVN_ERR(svn_stream_for_stdout(&stdout_stream, pool));
len = prop->len;
SVN_ERR(svn_stream_write(stdout_stream, prop->data, &len));
return SVN_NO_ERROR;
}
static svn_error_t *
do_plist(svnlook_ctxt_t *c,
const char *path,
svn_boolean_t verbose,
apr_pool_t *pool) {
svn_stream_t *stdout_stream;
svn_fs_root_t *root;
apr_hash_t *props;
apr_hash_index_t *hi;
svn_node_kind_t kind;
SVN_ERR(svn_stream_for_stdout(&stdout_stream, pool));
if (path != NULL) {
SVN_ERR(get_root(&root, c, pool));
SVN_ERR(verify_path(&kind, root, path, pool));
SVN_ERR(svn_fs_node_proplist(&props, root, path, pool));
} else
SVN_ERR(svn_fs_revision_proplist(&props, c->fs, c->rev_id, pool));
for (hi = apr_hash_first(pool, props); hi; hi = apr_hash_next(hi)) {
const void *key;
void *val;
const char *pname;
svn_string_t *propval;
SVN_ERR(check_cancel(NULL));
apr_hash_this(hi, &key, NULL, &val);
pname = key;
propval = val;
if (svn_prop_needs_translation(pname))
SVN_ERR(svn_subst_detranslate_string(&propval, propval, TRUE, pool));
if (verbose) {
const char *pname_stdout;
SVN_ERR(svn_cmdline_cstring_from_utf8(&pname_stdout, pname, pool));
printf(" %s : %s\n", pname_stdout, propval->data);
} else
printf(" %s\n", pname);
}
return SVN_NO_ERROR;
}
static svn_error_t *
do_tree(svnlook_ctxt_t *c,
const char *path,
svn_boolean_t show_ids,
svn_boolean_t full_paths,
svn_boolean_t recurse,
apr_pool_t *pool) {
svn_fs_root_t *root;
const svn_fs_id_t *id;
svn_boolean_t is_dir;
SVN_ERR(get_root(&root, c, pool));
SVN_ERR(svn_fs_node_id(&id, root, path, pool));
SVN_ERR(svn_fs_is_dir(&is_dir, root, path, pool));
SVN_ERR(print_tree(root, path, id, is_dir, 0, show_ids, full_paths,
recurse, pool));
return SVN_NO_ERROR;
}
static void
warning_func(void *baton,
svn_error_t *err) {
if (! err)
return;
svn_handle_error2(err, stderr, FALSE, "svnlook: ");
}
static svn_error_t *
get_ctxt_baton(svnlook_ctxt_t **baton_p,
struct svnlook_opt_state *opt_state,
apr_pool_t *pool) {
svnlook_ctxt_t *baton = apr_pcalloc(pool, sizeof(*baton));
SVN_ERR(svn_repos_open(&(baton->repos), opt_state->repos_path, pool));
baton->fs = svn_repos_fs(baton->repos);
svn_fs_set_warning_func(baton->fs, warning_func, NULL);
baton->show_ids = opt_state->show_ids;
baton->limit = opt_state->limit;
baton->no_diff_deleted = opt_state->no_diff_deleted;
baton->no_diff_added = opt_state->no_diff_added;
baton->diff_copy_from = opt_state->diff_copy_from;
baton->full_paths = opt_state->full_paths;
baton->copy_info = opt_state->copy_info;
baton->is_revision = opt_state->txn ? FALSE : TRUE;
baton->rev_id = opt_state->rev;
baton->txn_name = apr_pstrdup(pool, opt_state->txn);
baton->diff_options = svn_cstring_split(opt_state->extensions
? opt_state->extensions : "",
" \t\n\r", TRUE, pool);
if (baton->txn_name)
SVN_ERR(svn_fs_open_txn(&(baton->txn), baton->fs,
baton->txn_name, pool));
else if (baton->rev_id == SVN_INVALID_REVNUM)
SVN_ERR(svn_fs_youngest_rev(&(baton->rev_id), baton->fs, pool));
*baton_p = baton;
return SVN_NO_ERROR;
}
static svn_error_t *
subcommand_author(apr_getopt_t *os, void *baton, apr_pool_t *pool) {
struct svnlook_opt_state *opt_state = baton;
svnlook_ctxt_t *c;
SVN_ERR(get_ctxt_baton(&c, opt_state, pool));
SVN_ERR(do_author(c, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
subcommand_cat(apr_getopt_t *os, void *baton, apr_pool_t *pool) {
struct svnlook_opt_state *opt_state = baton;
svnlook_ctxt_t *c;
if (opt_state->arg1 == NULL)
return svn_error_createf
(SVN_ERR_CL_INSUFFICIENT_ARGS, NULL,
_("Missing repository path argument"));
SVN_ERR(get_ctxt_baton(&c, opt_state, pool));
SVN_ERR(do_cat(c, opt_state->arg1, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
subcommand_changed(apr_getopt_t *os, void *baton, apr_pool_t *pool) {
struct svnlook_opt_state *opt_state = baton;
svnlook_ctxt_t *c;
SVN_ERR(get_ctxt_baton(&c, opt_state, pool));
SVN_ERR(do_changed(c, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
subcommand_date(apr_getopt_t *os, void *baton, apr_pool_t *pool) {
struct svnlook_opt_state *opt_state = baton;
svnlook_ctxt_t *c;
SVN_ERR(get_ctxt_baton(&c, opt_state, pool));
SVN_ERR(do_date(c, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
subcommand_diff(apr_getopt_t *os, void *baton, apr_pool_t *pool) {
struct svnlook_opt_state *opt_state = baton;
svnlook_ctxt_t *c;
SVN_ERR(get_ctxt_baton(&c, opt_state, pool));
SVN_ERR(do_diff(c, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
subcommand_dirschanged(apr_getopt_t *os, void *baton, apr_pool_t *pool) {
struct svnlook_opt_state *opt_state = baton;
svnlook_ctxt_t *c;
SVN_ERR(get_ctxt_baton(&c, opt_state, pool));
SVN_ERR(do_dirs_changed(c, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
subcommand_help(apr_getopt_t *os, void *baton, apr_pool_t *pool) {
struct svnlook_opt_state *opt_state = baton;
const char *header =
_("general usage: svnlook SUBCOMMAND REPOS_PATH [ARGS & OPTIONS ...]\n"
"Note: any subcommand which takes the '--revision' and '--transaction'\n"
" options will, if invoked without one of those options, act on\n"
" the repository's youngest revision.\n"
"Type 'svnlook help <subcommand>' for help on a specific subcommand.\n"
"Type 'svnlook --version' to see the program version and FS modules.\n"
"\n"
"Available subcommands:\n");
const char *fs_desc_start
= _("The following repository back-end (FS) modules are available:\n\n");
svn_stringbuf_t *version_footer;
version_footer = svn_stringbuf_create(fs_desc_start, pool);
SVN_ERR(svn_fs_print_modules(version_footer, pool));
SVN_ERR(svn_opt_print_help(os, "svnlook",
opt_state ? opt_state->version : FALSE,
FALSE, version_footer->data,
header, cmd_table, options_table, NULL,
pool));
return SVN_NO_ERROR;
}
static svn_error_t *
subcommand_history(apr_getopt_t *os, void *baton, apr_pool_t *pool) {
struct svnlook_opt_state *opt_state = baton;
svnlook_ctxt_t *c;
const char *path = "/";
if (opt_state->arg1)
path = opt_state->arg1;
SVN_ERR(get_ctxt_baton(&c, opt_state, pool));
SVN_ERR(do_history(c, path, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
subcommand_lock(apr_getopt_t *os, void *baton, apr_pool_t *pool) {
struct svnlook_opt_state *opt_state = baton;
svnlook_ctxt_t *c;
const char *path;
svn_lock_t *lock;
if (opt_state->arg1)
path = opt_state->arg1;
else
return svn_error_create(SVN_ERR_CL_INSUFFICIENT_ARGS, NULL,
_("Missing path argument"));
SVN_ERR(get_ctxt_baton(&c, opt_state, pool));
SVN_ERR(svn_fs_get_lock(&lock, c->fs, path, pool));
if (lock) {
const char *cr_date, *exp_date = "";
int comment_lines = 0;
cr_date = svn_time_to_human_cstring(lock->creation_date, pool);
if (lock->expiration_date)
exp_date = svn_time_to_human_cstring(lock->expiration_date, pool);
if (lock->comment)
comment_lines = svn_cstring_count_newlines(lock->comment) + 1;
SVN_ERR(svn_cmdline_printf(pool, _("UUID Token: %s\n"), lock->token));
SVN_ERR(svn_cmdline_printf(pool, _("Owner: %s\n"), lock->owner));
SVN_ERR(svn_cmdline_printf(pool, _("Created: %s\n"), cr_date));
SVN_ERR(svn_cmdline_printf(pool, _("Expires: %s\n"), exp_date));
SVN_ERR(svn_cmdline_printf(pool,
(comment_lines != 1)
? _("Comment (%i lines):\n%s\n")
: _("Comment (%i line):\n%s\n"),
comment_lines,
lock->comment ? lock->comment : ""));
}
return SVN_NO_ERROR;
}
static svn_error_t *
subcommand_info(apr_getopt_t *os, void *baton, apr_pool_t *pool) {
struct svnlook_opt_state *opt_state = baton;
svnlook_ctxt_t *c;
SVN_ERR(get_ctxt_baton(&c, opt_state, pool));
SVN_ERR(do_author(c, pool));
SVN_ERR(do_date(c, pool));
SVN_ERR(do_log(c, TRUE, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
subcommand_log(apr_getopt_t *os, void *baton, apr_pool_t *pool) {
struct svnlook_opt_state *opt_state = baton;
svnlook_ctxt_t *c;
SVN_ERR(get_ctxt_baton(&c, opt_state, pool));
SVN_ERR(do_log(c, FALSE, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
subcommand_pget(apr_getopt_t *os, void *baton, apr_pool_t *pool) {
struct svnlook_opt_state *opt_state = baton;
svnlook_ctxt_t *c;
if (opt_state->arg1 == NULL) {
return svn_error_createf
(SVN_ERR_CL_INSUFFICIENT_ARGS, NULL,
opt_state->revprop ? _("Missing propname argument") :
_("Missing propname and repository path arguments"));
} else if (!opt_state->revprop && opt_state->arg2 == NULL) {
return svn_error_create
(SVN_ERR_CL_INSUFFICIENT_ARGS, NULL,
_("Missing propname or repository path argument"));
}
SVN_ERR(get_ctxt_baton(&c, opt_state, pool));
SVN_ERR(do_pget(c, opt_state->arg1,
opt_state->revprop ? NULL : opt_state->arg2, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
subcommand_plist(apr_getopt_t *os, void *baton, apr_pool_t *pool) {
struct svnlook_opt_state *opt_state = baton;
svnlook_ctxt_t *c;
if (!opt_state->revprop && opt_state->arg1 == NULL)
return svn_error_create
(SVN_ERR_CL_INSUFFICIENT_ARGS, NULL,
_("Missing repository path argument"));
SVN_ERR(get_ctxt_baton(&c, opt_state, pool));
SVN_ERR(do_plist(c, opt_state->revprop ? NULL : opt_state->arg1,
opt_state->verbose, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
subcommand_tree(apr_getopt_t *os, void *baton, apr_pool_t *pool) {
struct svnlook_opt_state *opt_state = baton;
svnlook_ctxt_t *c;
SVN_ERR(get_ctxt_baton(&c, opt_state, pool));
SVN_ERR(do_tree(c, opt_state->arg1 ? opt_state->arg1 : "",
opt_state->show_ids, opt_state->full_paths,
opt_state->non_recursive ? FALSE : TRUE, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
subcommand_youngest(apr_getopt_t *os, void *baton, apr_pool_t *pool) {
struct svnlook_opt_state *opt_state = baton;
svnlook_ctxt_t *c;
SVN_ERR(get_ctxt_baton(&c, opt_state, pool));
SVN_ERR(svn_cmdline_printf(pool, "%ld\n", c->rev_id));
return SVN_NO_ERROR;
}
static svn_error_t *
subcommand_uuid(apr_getopt_t *os, void *baton, apr_pool_t *pool) {
struct svnlook_opt_state *opt_state = baton;
svnlook_ctxt_t *c;
const char *uuid;
SVN_ERR(get_ctxt_baton(&c, opt_state, pool));
SVN_ERR(svn_fs_get_uuid(c->fs, &uuid, pool));
SVN_ERR(svn_cmdline_printf(pool, "%s\n", uuid));
return SVN_NO_ERROR;
}
int
main(int argc, const char *argv[]) {
svn_error_t *err;
apr_status_t apr_err;
apr_allocator_t *allocator;
apr_pool_t *pool;
const svn_opt_subcommand_desc_t *subcommand = NULL;
struct svnlook_opt_state opt_state;
apr_getopt_t *os;
int opt_id;
apr_array_header_t *received_opts;
int i;
if (svn_cmdline_init("svnlook", stderr) != EXIT_SUCCESS)
return EXIT_FAILURE;
if (apr_allocator_create(&allocator))
return EXIT_FAILURE;
apr_allocator_max_free_set(allocator, SVN_ALLOCATOR_RECOMMENDED_MAX_FREE);
pool = svn_pool_create_ex(NULL, allocator);
apr_allocator_owner_set(allocator, pool);
received_opts = apr_array_make(pool, SVN_OPT_MAX_OPTIONS, sizeof(int));
err = check_lib_versions();
if (err)
return svn_cmdline_handle_exit_error(err, pool, "svnlook: ");
err = svn_fs_initialize(pool);
if (err)
return svn_cmdline_handle_exit_error(err, pool, "svnlook: ");
if (argc <= 1) {
subcommand_help(NULL, NULL, pool);
svn_pool_destroy(pool);
return EXIT_FAILURE;
}
memset(&opt_state, 0, sizeof(opt_state));
opt_state.rev = SVN_INVALID_REVNUM;
err = svn_cmdline__getopt_init(&os, argc, argv, pool);
if (err)
return svn_cmdline_handle_exit_error(err, pool, "svnlook: ");
os->interleave = 1;
while (1) {
const char *opt_arg;
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
char *digits_end = NULL;
opt_state.rev = strtol(opt_arg, &digits_end, 10);
if ((! SVN_IS_VALID_REVNUM(opt_state.rev))
|| (! digits_end)
|| *digits_end)
SVN_INT_ERR(svn_error_create
(SVN_ERR_CL_ARG_PARSING_ERROR, NULL,
_("Invalid revision number supplied")));
}
break;
case 't':
opt_state.txn = opt_arg;
break;
case 'N':
opt_state.non_recursive = TRUE;
break;
case 'v':
opt_state.verbose = TRUE;
break;
case 'h':
case '?':
opt_state.help = TRUE;
break;
case svnlook__revprop_opt:
opt_state.revprop = TRUE;
break;
case svnlook__version:
opt_state.version = TRUE;
break;
case svnlook__show_ids:
opt_state.show_ids = TRUE;
break;
case 'l': {
char *end;
opt_state.limit = strtol(opt_arg, &end, 10);
if (end == opt_arg || *end != '\0') {
err = svn_error_create(SVN_ERR_CL_ARG_PARSING_ERROR, NULL,
_("Non-numeric limit argument given"));
return svn_cmdline_handle_exit_error(err, pool, "svnlook: ");
}
if (opt_state.limit <= 0) {
err = svn_error_create(SVN_ERR_INCORRECT_PARAMS, NULL,
_("Argument to --limit must be positive"));
return svn_cmdline_handle_exit_error(err, pool, "svnlook: ");
}
}
break;
case svnlook__no_diff_deleted:
opt_state.no_diff_deleted = TRUE;
break;
case svnlook__no_diff_added:
opt_state.no_diff_added = TRUE;
break;
case svnlook__diff_copy_from:
opt_state.diff_copy_from = TRUE;
break;
case svnlook__full_paths:
opt_state.full_paths = TRUE;
break;
case svnlook__copy_info:
opt_state.copy_info = TRUE;
break;
case 'x':
opt_state.extensions = opt_arg;
break;
default:
subcommand_help(NULL, NULL, pool);
svn_pool_destroy(pool);
return EXIT_FAILURE;
}
}
if ((opt_state.rev != SVN_INVALID_REVNUM) && opt_state.txn)
SVN_INT_ERR(svn_error_create
(SVN_ERR_CL_MUTUALLY_EXCLUSIVE_ARGS, NULL,
_("The '--transaction' (-t) and '--revision' (-r) arguments "
"can not co-exist")));
if (opt_state.help)
subcommand = svn_opt_get_canonical_subcommand(cmd_table, "help");
if (subcommand == NULL) {
if (os->ind >= os->argc) {
if (opt_state.version) {
static const svn_opt_subcommand_desc_t pseudo_cmd = {
"--version", subcommand_help, {0}, "",
{
svnlook__version,
}
};
subcommand = &pseudo_cmd;
} else {
svn_error_clear
(svn_cmdline_fprintf(stderr, pool,
_("Subcommand argument required\n")));
subcommand_help(NULL, NULL, pool);
svn_pool_destroy(pool);
return EXIT_FAILURE;
}
} else {
const char *first_arg = os->argv[os->ind++];
subcommand = svn_opt_get_canonical_subcommand(cmd_table, first_arg);
if (subcommand == NULL) {
const char *first_arg_utf8;
err = svn_utf_cstring_to_utf8(&first_arg_utf8, first_arg,
pool);
if (err)
return svn_cmdline_handle_exit_error(err, pool, "svnlook: ");
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
const char *repos_path = NULL;
const char *arg1 = NULL, *arg2 = NULL;
if (os->ind < os->argc) {
SVN_INT_ERR(svn_utf_cstring_to_utf8(&repos_path,
os->argv[os->ind++],
pool));
repos_path = svn_path_internal_style(repos_path, pool);
}
if (repos_path == NULL) {
svn_error_clear
(svn_cmdline_fprintf(stderr, pool,
_("Repository argument required\n")));
subcommand_help(NULL, NULL, pool);
svn_pool_destroy(pool);
return EXIT_FAILURE;
} else if (svn_path_is_url(repos_path)) {
svn_error_clear
(svn_cmdline_fprintf(stderr, pool,
_("'%s' is a URL when it should be a path\n"),
repos_path));
svn_pool_destroy(pool);
return EXIT_FAILURE;
}
opt_state.repos_path = repos_path;
if (os->ind < os->argc) {
SVN_INT_ERR(svn_utf_cstring_to_utf8
(&arg1, os->argv[os->ind++], pool));
arg1 = svn_path_internal_style(arg1, pool);
}
opt_state.arg1 = arg1;
if (os->ind < os->argc) {
SVN_INT_ERR(svn_utf_cstring_to_utf8
(&arg2, os->argv[os->ind++], pool));
arg2 = svn_path_internal_style(arg2, pool);
}
opt_state.arg2 = arg2;
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
(stderr, pool,
_("Subcommand '%s' doesn't accept option '%s'\n"
"Type 'svnlook help %s' for usage.\n"),
subcommand->name, optstr, subcommand->name));
svn_pool_destroy(pool);
return EXIT_FAILURE;
}
}
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
err = (*subcommand->cmd_func)(os, &opt_state, pool);
if (err) {
if (err->apr_err == SVN_ERR_CL_INSUFFICIENT_ARGS
|| err->apr_err == SVN_ERR_CL_ARG_PARSING_ERROR) {
err = svn_error_quick_wrap(err,
_("Try 'svnlook help' for more info"));
}
return svn_cmdline_handle_exit_error(err, pool, "svnlook: ");
} else {
svn_pool_destroy(pool);
SVN_INT_ERR(svn_cmdline_fflush(stdout));
return EXIT_SUCCESS;
}
}