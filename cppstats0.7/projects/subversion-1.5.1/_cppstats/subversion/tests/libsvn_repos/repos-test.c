#include <stdlib.h>
#include <string.h>
#include <apr_pools.h>
#include <apr_md5.h>
#include "svn_pools.h"
#include "svn_error.h"
#include "svn_fs.h"
#include "svn_repos.h"
#include "svn_path.h"
#include "svn_delta.h"
#include "svn_config.h"
#include "../svn_test.h"
#include "../svn_test_fs.h"
#include "dir-delta-editor.h"
#define NL APR_EOL_STR
static svn_error_t *
dir_deltas(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
svn_repos_t *repos;
svn_fs_t *fs;
svn_fs_txn_t *txn;
svn_fs_root_t *txn_root, *revision_root;
svn_revnum_t youngest_rev;
void *edit_baton;
const svn_delta_editor_t *editor;
svn_test__tree_t expected_trees[8];
int revision_count = 0;
int i, j;
apr_pool_t *subpool = svn_pool_create(pool);
*msg = "test svn_repos_dir_delta2";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(svn_test__create_repos(&repos, "test-repo-dir-deltas",
opts->fs_type, pool));
fs = svn_repos_fs(repos);
expected_trees[revision_count].num_entries = 0;
expected_trees[revision_count++].entries = 0;
SVN_ERR(svn_fs_begin_txn(&txn, fs, 0, subpool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, subpool));
SVN_ERR(svn_test__create_greek_tree(txn_root, subpool));
SVN_ERR(svn_repos_fs_commit_txn(NULL, repos, &youngest_rev, txn, subpool));
{
static svn_test__tree_entry_t expected_entries[] = {
{ "iota", "This is the file 'iota'.\n" },
{ "A", 0 },
{ "A/mu", "This is the file 'mu'.\n" },
{ "A/B", 0 },
{ "A/B/lambda", "This is the file 'lambda'.\n" },
{ "A/B/E", 0 },
{ "A/B/E/alpha", "This is the file 'alpha'.\n" },
{ "A/B/E/beta", "This is the file 'beta'.\n" },
{ "A/B/F", 0 },
{ "A/C", 0 },
{ "A/D", 0 },
{ "A/D/gamma", "This is the file 'gamma'.\n" },
{ "A/D/G", 0 },
{ "A/D/G/pi", "This is the file 'pi'.\n" },
{ "A/D/G/rho", "This is the file 'rho'.\n" },
{ "A/D/G/tau", "This is the file 'tau'.\n" },
{ "A/D/H", 0 },
{ "A/D/H/chi", "This is the file 'chi'.\n" },
{ "A/D/H/psi", "This is the file 'psi'.\n" },
{ "A/D/H/omega", "This is the file 'omega'.\n" }
};
expected_trees[revision_count].entries = expected_entries;
expected_trees[revision_count].num_entries = 20;
SVN_ERR(svn_fs_revision_root(&revision_root, fs,
youngest_rev, subpool));
SVN_ERR(svn_test__validate_tree
(revision_root, expected_trees[revision_count].entries,
expected_trees[revision_count].num_entries, subpool));
revision_count++;
}
svn_pool_clear(subpool);
SVN_ERR(svn_fs_begin_txn(&txn, fs, youngest_rev, subpool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, subpool));
{
static svn_test__txn_script_command_t script_entries[] = {
{ 'a', "A/delta", "This is the file 'delta'.\n" },
{ 'a', "A/epsilon", "This is the file 'epsilon'.\n" },
{ 'a', "A/B/Z", 0 },
{ 'a', "A/B/Z/zeta", "This is the file 'zeta'.\n" },
{ 'd', "A/C", 0 },
{ 'd', "A/mu", "" },
{ 'd', "A/D/G/tau", "" },
{ 'd', "A/D/H/omega", "" },
{ 'e', "iota", "Changed file 'iota'.\n" },
{ 'e', "A/D/G/rho", "Changed file 'rho'.\n" }
};
SVN_ERR(svn_test__txn_script_exec(txn_root, script_entries, 10,
subpool));
}
SVN_ERR(svn_repos_fs_commit_txn(NULL, repos, &youngest_rev, txn, subpool));
{
static svn_test__tree_entry_t expected_entries[] = {
{ "iota", "Changed file 'iota'.\n" },
{ "A", 0 },
{ "A/delta", "This is the file 'delta'.\n" },
{ "A/epsilon", "This is the file 'epsilon'.\n" },
{ "A/B", 0 },
{ "A/B/lambda", "This is the file 'lambda'.\n" },
{ "A/B/E", 0 },
{ "A/B/E/alpha", "This is the file 'alpha'.\n" },
{ "A/B/E/beta", "This is the file 'beta'.\n" },
{ "A/B/F", 0 },
{ "A/B/Z", 0 },
{ "A/B/Z/zeta", "This is the file 'zeta'.\n" },
{ "A/D", 0 },
{ "A/D/gamma", "This is the file 'gamma'.\n" },
{ "A/D/G", 0 },
{ "A/D/G/pi", "This is the file 'pi'.\n" },
{ "A/D/G/rho", "Changed file 'rho'.\n" },
{ "A/D/H", 0 },
{ "A/D/H/chi", "This is the file 'chi'.\n" },
{ "A/D/H/psi", "This is the file 'psi'.\n" }
};
expected_trees[revision_count].entries = expected_entries;
expected_trees[revision_count].num_entries = 20;
SVN_ERR(svn_fs_revision_root(&revision_root, fs,
youngest_rev, subpool));
SVN_ERR(svn_test__validate_tree
(revision_root, expected_trees[revision_count].entries,
expected_trees[revision_count].num_entries, subpool));
revision_count++;
}
svn_pool_clear(subpool);
SVN_ERR(svn_fs_begin_txn(&txn, fs, youngest_rev, subpool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, subpool));
{
static svn_test__txn_script_command_t script_entries[] = {
{ 'a', "A/mu", "Re-added file 'mu'.\n" },
{ 'a', "A/D/H/omega", 0 },
{ 'd', "iota", "" },
{ 'e', "A/delta", "This is the file 'delta'.\nLine 2.\n" }
};
SVN_ERR(svn_test__txn_script_exec(txn_root, script_entries, 4, subpool));
}
SVN_ERR(svn_repos_fs_commit_txn(NULL, repos, &youngest_rev, txn, subpool));
{
static svn_test__tree_entry_t expected_entries[] = {
{ "A", 0 },
{ "A/delta", "This is the file 'delta'.\nLine 2.\n" },
{ "A/epsilon", "This is the file 'epsilon'.\n" },
{ "A/mu", "Re-added file 'mu'.\n" },
{ "A/B", 0 },
{ "A/B/lambda", "This is the file 'lambda'.\n" },
{ "A/B/E", 0 },
{ "A/B/E/alpha", "This is the file 'alpha'.\n" },
{ "A/B/E/beta", "This is the file 'beta'.\n" },
{ "A/B/F", 0 },
{ "A/B/Z", 0 },
{ "A/B/Z/zeta", "This is the file 'zeta'.\n" },
{ "A/D", 0 },
{ "A/D/gamma", "This is the file 'gamma'.\n" },
{ "A/D/G", 0 },
{ "A/D/G/pi", "This is the file 'pi'.\n" },
{ "A/D/G/rho", "Changed file 'rho'.\n" },
{ "A/D/H", 0 },
{ "A/D/H/chi", "This is the file 'chi'.\n" },
{ "A/D/H/psi", "This is the file 'psi'.\n" },
{ "A/D/H/omega", 0 }
};
expected_trees[revision_count].entries = expected_entries;
expected_trees[revision_count].num_entries = 21;
SVN_ERR(svn_fs_revision_root(&revision_root, fs,
youngest_rev, subpool));
SVN_ERR(svn_test__validate_tree
(revision_root, expected_trees[revision_count].entries,
expected_trees[revision_count].num_entries, subpool));
revision_count++;
}
svn_pool_clear(subpool);
SVN_ERR(svn_fs_begin_txn(&txn, fs, youngest_rev, subpool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, subpool));
SVN_ERR(svn_fs_revision_root(&revision_root, fs, youngest_rev, subpool));
SVN_ERR(svn_fs_copy(revision_root, "A/D/G",
txn_root, "A/D/G2",
subpool));
SVN_ERR(svn_fs_copy(revision_root, "A/epsilon",
txn_root, "A/B/epsilon",
subpool));
SVN_ERR(svn_repos_fs_commit_txn(NULL, repos, &youngest_rev, txn, subpool));
{
static svn_test__tree_entry_t expected_entries[] = {
{ "A", 0 },
{ "A/delta", "This is the file 'delta'.\nLine 2.\n" },
{ "A/epsilon", "This is the file 'epsilon'.\n" },
{ "A/mu", "Re-added file 'mu'.\n" },
{ "A/B", 0 },
{ "A/B/epsilon", "This is the file 'epsilon'.\n" },
{ "A/B/lambda", "This is the file 'lambda'.\n" },
{ "A/B/E", 0 },
{ "A/B/E/alpha", "This is the file 'alpha'.\n" },
{ "A/B/E/beta", "This is the file 'beta'.\n" },
{ "A/B/F", 0 },
{ "A/B/Z", 0 },
{ "A/B/Z/zeta", "This is the file 'zeta'.\n" },
{ "A/D", 0 },
{ "A/D/gamma", "This is the file 'gamma'.\n" },
{ "A/D/G", 0 },
{ "A/D/G/pi", "This is the file 'pi'.\n" },
{ "A/D/G/rho", "Changed file 'rho'.\n" },
{ "A/D/G2", 0 },
{ "A/D/G2/pi", "This is the file 'pi'.\n" },
{ "A/D/G2/rho", "Changed file 'rho'.\n" },
{ "A/D/H", 0 },
{ "A/D/H/chi", "This is the file 'chi'.\n" },
{ "A/D/H/psi", "This is the file 'psi'.\n" },
{ "A/D/H/omega", 0 }
};
expected_trees[revision_count].entries = expected_entries;
expected_trees[revision_count].num_entries = 25;
SVN_ERR(svn_fs_revision_root(&revision_root, fs,
youngest_rev, pool));
SVN_ERR(svn_test__validate_tree
(revision_root, expected_trees[revision_count].entries,
expected_trees[revision_count].num_entries, subpool));
revision_count++;
}
svn_pool_clear(subpool);
for (i = 0; i < revision_count; i++) {
for (j = 0; j < revision_count; j++) {
SVN_ERR(svn_fs_begin_txn(&txn, fs, i, subpool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, subpool));
SVN_ERR(dir_delta_get_editor(&editor,
&edit_baton,
fs,
txn_root,
"",
subpool));
SVN_ERR(svn_fs_revision_root(&revision_root, fs, j, subpool));
SVN_ERR(svn_repos_dir_delta2(txn_root,
"",
"",
revision_root,
"",
editor,
edit_baton,
NULL,
NULL,
TRUE,
svn_depth_infinity,
FALSE,
FALSE,
subpool));
SVN_ERR(svn_test__validate_tree
(txn_root, expected_trees[j].entries,
expected_trees[j].num_entries, subpool));
svn_error_clear(svn_fs_abort_txn(txn, subpool));
svn_pool_clear(subpool);
}
}
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
static svn_error_t *
node_tree_delete_under_copy(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
svn_repos_t *repos;
svn_fs_t *fs;
svn_fs_txn_t *txn;
svn_fs_root_t *txn_root, *revision_root, *revision_2_root;
svn_revnum_t youngest_rev;
void *edit_baton;
const svn_delta_editor_t *editor;
svn_repos_node_t *tree;
apr_pool_t *subpool = svn_pool_create(pool);
*msg = "test deletions under copies in node_tree code";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(svn_test__create_repos(&repos, "test-repo-del-under-copy",
opts->fs_type, pool));
fs = svn_repos_fs(repos);
SVN_ERR(svn_fs_begin_txn(&txn, fs, 0, pool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, pool));
SVN_ERR(svn_test__create_greek_tree(txn_root, pool));
SVN_ERR(svn_repos_fs_commit_txn(NULL, repos, &youngest_rev, txn, pool));
SVN_ERR(svn_fs_revision_root(&revision_root, fs, youngest_rev, pool));
SVN_ERR(svn_fs_begin_txn(&txn, fs, youngest_rev, pool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, pool));
SVN_ERR(svn_fs_copy(revision_root, "A", txn_root, "Z", pool));
SVN_ERR(svn_fs_delete(txn_root, "Z/D/G/rho", pool));
SVN_ERR(svn_fs_delete(txn_root, "Z/D/H", pool));
SVN_ERR(svn_repos_fs_commit_txn(NULL, repos, &youngest_rev, txn, pool));
SVN_ERR(svn_fs_revision_root(&revision_2_root, fs, youngest_rev, pool));
SVN_ERR(svn_repos_node_editor(&editor, &edit_baton, repos,
revision_root, revision_2_root,
pool, subpool));
SVN_ERR(svn_repos_replay2(revision_2_root, "", SVN_INVALID_REVNUM, FALSE,
editor, edit_baton, NULL, NULL, subpool));
tree = svn_repos_node_from_baton(edit_baton);
svn_pool_destroy(subpool);
if (! (tree
&& tree->child
&& tree->child->child
&& tree->child->child->child
&& tree->child->child->child->child
&& tree->child->child->child->sibling))
return svn_error_create(SVN_ERR_TEST_FAILED, NULL,
"Generated node tree is bogus.");
if (! ((strcmp(tree->name, "") == 0)
&& (strcmp(tree->child->name, "Z") == 0)
&& (strcmp(tree->child->child->name, "D") == 0)
&& (strcmp(tree->child->child->child->name, "G") == 0)
&& ((strcmp(tree->child->child->child->child->name, "rho") == 0)
&& (tree->child->child->child->child->kind == svn_node_file)
&& (tree->child->child->child->child->action == 'D'))
&& ((strcmp(tree->child->child->child->sibling->name, "H") == 0)
&& (tree->child->child->child->sibling->kind == svn_node_dir)
&& (tree->child->child->child->sibling->action == 'D'))))
return svn_error_create(SVN_ERR_TEST_FAILED, NULL,
"Generated node tree is bogus.");
return SVN_NO_ERROR;
}
static const char *
print_chrevs(const apr_array_header_t *revs_got,
int num_revs_expected,
const svn_revnum_t *revs_expected,
apr_pool_t *pool) {
int i;
const char *outstr;
svn_revnum_t rev;
outstr = apr_psprintf(pool, "Got: { ");
if (revs_got) {
for (i = 0; i < revs_got->nelts; i++) {
rev = APR_ARRAY_IDX(revs_got, i, svn_revnum_t);
outstr = apr_pstrcat(pool,
outstr,
apr_psprintf(pool, "%ld ", rev),
NULL);
}
}
outstr = apr_pstrcat(pool, outstr, "} Expected: { ", NULL);
for (i = 0; i < num_revs_expected; i++) {
outstr = apr_pstrcat(pool,
outstr,
apr_psprintf(pool, "%ld ",
revs_expected[i]),
NULL);
}
return apr_pstrcat(pool, outstr, "}", NULL);
}
static svn_error_t *
history_to_revs_array(void *baton,
const char *path,
svn_revnum_t revision,
apr_pool_t *pool) {
apr_array_header_t *revs_array = baton;
APR_ARRAY_PUSH(revs_array, svn_revnum_t) = revision;
return SVN_NO_ERROR;
}
struct revisions_changed_results {
const char *path;
int num_revs;
svn_revnum_t revs_changed[11];
};
static svn_error_t *
revisions_changed(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
apr_pool_t *spool = svn_pool_create(pool);
svn_repos_t *repos;
svn_fs_t *fs;
svn_fs_txn_t *txn;
svn_fs_root_t *txn_root, *rev_root;
svn_revnum_t youngest_rev = 0;
*msg = "test svn_repos_history() (partially)";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(svn_test__create_repos(&repos, "test-repo-revisions-changed",
opts->fs_type, pool));
fs = svn_repos_fs(repos);
SVN_ERR(svn_fs_begin_txn(&txn, fs, youngest_rev, spool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, spool));
SVN_ERR(svn_test__create_greek_tree(txn_root, spool));
SVN_ERR(svn_fs_commit_txn(NULL, &youngest_rev, txn, spool));
svn_pool_clear(spool);
SVN_ERR(svn_fs_begin_txn(&txn, fs, youngest_rev, spool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, spool));
SVN_ERR(svn_test__set_file_contents(txn_root, "A/mu", "2", spool));
SVN_ERR(svn_test__set_file_contents(txn_root, "A/B/E/alpha", "2", spool));
SVN_ERR(svn_test__set_file_contents(txn_root, "A/D/H/omega", "2", spool));
SVN_ERR(svn_fs_commit_txn(NULL, &youngest_rev, txn, spool));
svn_pool_clear(spool);
SVN_ERR(svn_fs_begin_txn(&txn, fs, youngest_rev, spool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, spool));
SVN_ERR(svn_test__set_file_contents(txn_root, "iota", "3", spool));
SVN_ERR(svn_test__set_file_contents(txn_root, "A/B/lambda", "3", spool));
SVN_ERR(svn_test__set_file_contents(txn_root, "A/D/H/psi", "3", spool));
SVN_ERR(svn_test__set_file_contents(txn_root, "A/D/H/omega", "3", spool));
SVN_ERR(svn_fs_commit_txn(NULL, &youngest_rev, txn, spool));
svn_pool_clear(spool);
SVN_ERR(svn_fs_begin_txn(&txn, fs, youngest_rev, spool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, spool));
SVN_ERR(svn_test__set_file_contents(txn_root, "iota", "4", spool));
SVN_ERR(svn_test__set_file_contents(txn_root, "A/B/E/beta", "4", spool));
SVN_ERR(svn_test__set_file_contents(txn_root, "A/D/gamma", "4", spool));
SVN_ERR(svn_test__set_file_contents(txn_root, "A/D/G/pi", "4", spool));
SVN_ERR(svn_test__set_file_contents(txn_root, "A/D/G/rho", "4", spool));
SVN_ERR(svn_fs_commit_txn(NULL, &youngest_rev, txn, spool));
svn_pool_clear(spool);
SVN_ERR(svn_fs_begin_txn(&txn, fs, youngest_rev, spool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, spool));
SVN_ERR(svn_test__set_file_contents(txn_root, "A/mu", "5", spool));
SVN_ERR(svn_test__set_file_contents(txn_root, "A/B/E/alpha", "5", spool));
SVN_ERR(svn_test__set_file_contents(txn_root, "A/D/G/tau", "5", spool));
SVN_ERR(svn_test__set_file_contents(txn_root, "A/D/H/chi", "5", spool));
SVN_ERR(svn_fs_commit_txn(NULL, &youngest_rev, txn, spool));
svn_pool_clear(spool);
SVN_ERR(svn_fs_begin_txn(&txn, fs, youngest_rev, spool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, spool));
SVN_ERR(svn_fs_revision_root(&rev_root, fs, youngest_rev, spool));
SVN_ERR(svn_fs_copy(rev_root, "A/D", txn_root, "A/Z", spool));
SVN_ERR(svn_fs_delete(txn_root, "A/D", spool));
SVN_ERR(svn_fs_commit_txn(NULL, &youngest_rev, txn, spool));
svn_pool_clear(spool);
SVN_ERR(svn_fs_begin_txn(&txn, fs, youngest_rev, spool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, spool));
SVN_ERR(svn_test__set_file_contents(txn_root, "A/Z/G/pi", "7", spool));
SVN_ERR(svn_fs_commit_txn(NULL, &youngest_rev, txn, spool));
svn_pool_clear(spool);
SVN_ERR(svn_fs_begin_txn(&txn, fs, youngest_rev, spool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, spool));
SVN_ERR(svn_fs_revision_root(&rev_root, fs, youngest_rev, spool));
SVN_ERR(svn_fs_copy(rev_root, "A/Z", txn_root, "A/D", spool));
SVN_ERR(svn_fs_delete(txn_root, "A/Z", spool));
SVN_ERR(svn_test__set_file_contents(txn_root, "iota", "8", spool));
SVN_ERR(svn_fs_commit_txn(NULL, &youngest_rev, txn, spool));
svn_pool_clear(spool);
SVN_ERR(svn_fs_begin_txn(&txn, fs, youngest_rev, spool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, spool));
SVN_ERR(svn_fs_revision_root(&rev_root, fs, youngest_rev, spool));
SVN_ERR(svn_fs_copy(rev_root, "A/D/G", txn_root, "A/D/Q", spool));
SVN_ERR(svn_fs_commit_txn(NULL, &youngest_rev, txn, spool));
svn_pool_clear(spool);
SVN_ERR(svn_fs_begin_txn(&txn, fs, youngest_rev, spool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, spool));
SVN_ERR(svn_test__set_file_contents(txn_root, "A/D/Q/pi", "10", spool));
SVN_ERR(svn_test__set_file_contents(txn_root, "A/D/Q/rho", "10", spool));
SVN_ERR(svn_fs_commit_txn(NULL, &youngest_rev, txn, spool));
svn_pool_clear(spool);
{
int j;
static const struct revisions_changed_results test_data[25] = {
{ "", 11, { 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0 } },
{ "iota", 4, { 8, 4, 3, 1 } },
{ "A", 10, { 10, 9, 8, 7, 6, 5, 4, 3, 2, 1 } },
{ "A/mu", 3, { 5, 2, 1 } },
{ "A/B", 5, { 5, 4, 3, 2, 1 } },
{ "A/B/lambda", 2, { 3, 1 } },
{ "A/B/E", 4, { 5, 4, 2, 1 } },
{ "A/B/E/alpha", 3, { 5, 2, 1 } },
{ "A/B/E/beta", 2, { 4, 1 } },
{ "A/B/F", 1, { 1 } },
{ "A/C", 1, { 1 } },
{ "A/D", 10, { 10, 9, 8, 7, 6, 5, 4, 3, 2, 1 } },
{ "A/D/gamma", 4, { 8, 6, 4, 1 } },
{ "A/D/G", 6, { 8, 7, 6, 5, 4, 1 } },
{ "A/D/G/pi", 5, { 8, 7, 6, 4, 1 } },
{ "A/D/G/rho", 4, { 8, 6, 4, 1 } },
{ "A/D/G/tau", 4, { 8, 6, 5, 1 } },
{ "A/D/Q", 8, { 10, 9, 8, 7, 6, 5, 4, 1 } },
{ "A/D/Q/pi", 7, { 10, 9, 8, 7, 6, 4, 1 } },
{ "A/D/Q/rho", 6, { 10, 9, 8, 6, 4, 1 } },
{ "A/D/Q/tau", 5, { 9, 8, 6, 5, 1 } },
{ "A/D/H", 6, { 8, 6, 5, 3, 2, 1 } },
{ "A/D/H/chi", 4, { 8, 6, 5, 1 } },
{ "A/D/H/psi", 4, { 8, 6, 3, 1 } },
{ "A/D/H/omega", 5, { 8, 6, 3, 2, 1 } }
};
for (j = 0; j < 25; j++) {
int i;
const char *path = test_data[j].path;
int num_revs = test_data[j].num_revs;
const svn_revnum_t *revs_changed = test_data[j].revs_changed;
apr_array_header_t *revs = apr_array_make(spool, 10,
sizeof(svn_revnum_t));
SVN_ERR(svn_repos_history(fs, path, history_to_revs_array, revs,
0, youngest_rev, TRUE, spool));
if ((! revs) || (revs->nelts != num_revs))
return svn_error_createf
(SVN_ERR_FS_GENERAL, NULL,
"Changed revisions differ from expected for '%s'\n%s",
path, print_chrevs(revs, num_revs, revs_changed, spool));
for (i = 0; i < num_revs; i++) {
svn_revnum_t rev = APR_ARRAY_IDX(revs, i, svn_revnum_t);
if (rev != revs_changed[i])
return svn_error_createf
(SVN_ERR_FS_GENERAL, NULL,
"Changed revisions differ from expected for '%s'\n%s",
path, print_chrevs(revs, num_revs, revs_changed, spool));
}
svn_pool_clear(spool);
}
}
svn_pool_destroy(spool);
return SVN_NO_ERROR;
}
struct locations_info {
svn_revnum_t rev;
const char *path;
};
static svn_error_t *
check_locations_info(apr_hash_t *locations, const struct locations_info *info) {
unsigned int i;
for (i = 0; info->rev != 0; ++i, ++info) {
const char *p = apr_hash_get(locations, &info->rev, sizeof
(svn_revnum_t));
if (!p)
return svn_error_createf(SVN_ERR_TEST_FAILED, NULL,
"Missing path for revision %ld", info->rev);
if (strcmp(p, info->path) != 0)
return svn_error_createf(SVN_ERR_TEST_FAILED, NULL,
"Pth mismatch for rev %ld", info->rev);
}
if (apr_hash_count(locations) > i)
return svn_error_create(SVN_ERR_TEST_FAILED, NULL,
"Returned locations contain too many elements.");
return SVN_NO_ERROR;
}
static svn_error_t *
check_locations(svn_fs_t *fs, struct locations_info *info,
const char *path, svn_revnum_t peg_revision,
apr_pool_t *pool) {
apr_array_header_t *a = apr_array_make(pool, 0, sizeof(svn_revnum_t));
apr_hash_t *h;
struct locations_info *iter;
for (iter = info; iter->rev != 0; ++iter)
APR_ARRAY_PUSH(a, svn_revnum_t) = iter->rev;
SVN_ERR(svn_repos_trace_node_locations(fs, &h, path, peg_revision, a,
NULL, NULL, pool));
SVN_ERR(check_locations_info(h, info));
return SVN_NO_ERROR;
}
static svn_error_t *
node_locations(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
apr_pool_t *subpool = svn_pool_create(pool);
svn_repos_t *repos;
svn_fs_t *fs;
svn_fs_txn_t *txn;
svn_fs_root_t *txn_root, *root;
svn_revnum_t youngest_rev;
*msg = "test svn_repos_node_locations";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(svn_test__create_repos(&repos, "test-repo-node-locations",
opts->fs_type, pool));
fs = svn_repos_fs(repos);
SVN_ERR(svn_fs_begin_txn(&txn, fs, 0, subpool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, subpool));
SVN_ERR(svn_test__create_greek_tree(txn_root, subpool));
SVN_ERR(svn_repos_fs_commit_txn(NULL, repos, &youngest_rev, txn, subpool));
svn_pool_clear(subpool);
SVN_ERR(svn_fs_revision_root(&root, fs, youngest_rev, subpool));
SVN_ERR(svn_fs_begin_txn(&txn, fs, 0, subpool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, subpool));
SVN_ERR(svn_fs_copy(root, "/A/mu", txn_root, "/mu.new", subpool));
SVN_ERR(svn_repos_fs_commit_txn(NULL, repos, &youngest_rev, txn, subpool));
{
struct locations_info info[] = {
{ 1, "/A/mu" },
{ 2, "/mu.new" },
{ 0 }
};
SVN_ERR(check_locations(fs, info, "/mu.new", 2, pool));
SVN_ERR(check_locations(fs, info, "mu.new", 2, pool));
}
svn_pool_clear(subpool);
return SVN_NO_ERROR;
}
static svn_error_t *
node_locations2(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
apr_pool_t *subpool = svn_pool_create(pool);
svn_repos_t *repos;
svn_fs_t *fs;
svn_fs_txn_t *txn;
svn_fs_root_t *txn_root, *root;
svn_revnum_t youngest_rev = 0;
*msg = "test svn_repos_node_locations some more";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(svn_test__create_repos(&repos, "test-repo-node-locations2",
opts->fs_type, pool));
fs = svn_repos_fs(repos);
SVN_ERR(svn_fs_begin_txn(&txn, fs, youngest_rev, subpool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, subpool));
SVN_ERR(svn_fs_make_dir(txn_root, "/foo", subpool));
SVN_ERR(svn_repos_fs_commit_txn(NULL, repos, &youngest_rev, txn, subpool));
svn_pool_clear(subpool);
SVN_ERR(svn_fs_begin_txn(&txn, fs, youngest_rev, subpool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, subpool));
SVN_ERR(svn_fs_revision_root(&root, fs, youngest_rev, subpool));
SVN_ERR(svn_fs_copy(root, "/foo", txn_root, "/bar", subpool));
SVN_ERR(svn_fs_make_file(txn_root, "/bar/baz", subpool));
SVN_ERR(svn_repos_fs_commit_txn(NULL, repos, &youngest_rev, txn, subpool));
svn_pool_clear(subpool);
SVN_ERR(svn_fs_begin_txn(&txn, fs, youngest_rev, subpool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, subpool));
SVN_ERR(svn_test__set_file_contents(txn_root, "/bar/baz", "brrt", subpool));
SVN_ERR(svn_repos_fs_commit_txn(NULL, repos, &youngest_rev, txn, subpool));
svn_pool_clear(subpool);
SVN_ERR(svn_fs_begin_txn(&txn, fs, youngest_rev, subpool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, subpool));
SVN_ERR(svn_test__set_file_contents(txn_root, "/bar/baz", "bzzz", subpool));
SVN_ERR(svn_repos_fs_commit_txn(NULL, repos, &youngest_rev, txn, subpool));
svn_pool_clear(subpool);
{
struct locations_info info[] = {
{ 3, "/bar/baz" },
{ 2, "/bar/baz" },
{ 0 }
};
SVN_ERR(check_locations(fs, info, "/bar/baz", youngest_rev, pool));
}
return SVN_NO_ERROR;
}
typedef struct rmlocks_baton_t {
apr_hash_t *removed;
apr_pool_t *pool;
} rmlocks_baton_t;
typedef struct rmlocks_file_baton_t {
rmlocks_baton_t *main_baton;
const char *path;
} rmlocks_file_baton_t;
static svn_error_t *
rmlocks_open_file(const char *path,
void *parent_baton,
svn_revnum_t base_revision,
apr_pool_t *file_pool,
void **file_baton) {
rmlocks_file_baton_t *fb = apr_palloc(file_pool, sizeof(*fb));
rmlocks_baton_t *b = parent_baton;
fb->main_baton = b;
fb->path = apr_pstrdup(b->pool, path);
*file_baton = fb;
return SVN_NO_ERROR;
}
static svn_error_t *
rmlocks_change_prop(void *file_baton,
const char *name,
const svn_string_t *value,
apr_pool_t *pool) {
rmlocks_file_baton_t *fb = file_baton;
if (strcmp(name, SVN_PROP_ENTRY_LOCK_TOKEN) == 0) {
if (value != NULL)
return svn_error_create(SVN_ERR_TEST_FAILED, NULL,
"Value for lock-token property not NULL");
if (apr_hash_get(fb->main_baton->removed, fb->path,
APR_HASH_KEY_STRING) != NULL)
return svn_error_createf(SVN_ERR_TEST_FAILED, NULL,
"Lock token for '%s' already removed",
fb->path);
apr_hash_set(fb->main_baton->removed, fb->path, APR_HASH_KEY_STRING,
(void *)1);
}
return SVN_NO_ERROR;
}
static svn_error_t *
rmlocks_open_root(void *edit_baton,
svn_revnum_t base_revision,
apr_pool_t *dir_pool,
void **root_baton) {
*root_baton = edit_baton;
return SVN_NO_ERROR;
}
static svn_error_t *
rmlocks_open_directory(const char *path,
void *parent_baton,
svn_revnum_t base_revision,
apr_pool_t *pool,
void **dir_baton) {
*dir_baton = parent_baton;
return SVN_NO_ERROR;
}
static svn_error_t *
create_rmlocks_editor(svn_delta_editor_t **editor,
void **edit_baton,
apr_hash_t **removed,
apr_pool_t *pool) {
rmlocks_baton_t *baton = apr_palloc(pool, sizeof(*baton));
*editor = svn_delta_default_editor(pool);
(*editor)->open_root = rmlocks_open_root;
(*editor)->open_directory = rmlocks_open_directory;
(*editor)->open_file = rmlocks_open_file;
(*editor)->change_file_prop = rmlocks_change_prop;
baton->removed = apr_hash_make(pool);
baton->pool = pool;
*edit_baton = baton;
*removed = baton->removed;
return SVN_NO_ERROR;
}
static svn_error_t *
rmlocks_check(const char **spec, apr_hash_t *hash) {
apr_size_t n = 0;
for (; *spec; ++spec, ++n) {
if (! apr_hash_get(hash, *spec, APR_HASH_KEY_STRING))
return svn_error_createf
(SVN_ERR_TEST_FAILED, NULL,
"Lock token for '%s' should have been removed", *spec);
}
if (n < apr_hash_count(hash))
return svn_error_create(SVN_ERR_TEST_FAILED, NULL,
"Lock token for one or more paths unexpectedly "
"removed");
return SVN_NO_ERROR;
}
static svn_error_t *
rmlocks(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
svn_repos_t *repos;
svn_fs_t *fs;
svn_fs_txn_t *txn;
svn_fs_root_t *txn_root;
apr_pool_t *subpool = svn_pool_create(pool);
svn_revnum_t youngest_rev;
svn_delta_editor_t *editor;
void *edit_baton, *report_baton;
svn_lock_t *l1, *l2, *l3, *l4;
svn_fs_access_t *fs_access;
apr_hash_t *removed;
*msg = "test removal of defunct locks";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(svn_test__create_repos(&repos, "test-repo-rmlocks",
opts->fs_type, pool));
fs = svn_repos_fs(repos);
SVN_ERR(svn_fs_begin_txn(&txn, fs, 0, subpool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, subpool));
SVN_ERR(svn_test__create_greek_tree(txn_root, subpool));
SVN_ERR(svn_repos_fs_commit_txn(NULL, repos, &youngest_rev, txn, subpool));
svn_pool_clear(subpool);
SVN_ERR(svn_fs_create_access(&fs_access, "user1", pool));
SVN_ERR(svn_fs_set_access(fs, fs_access));
{
const char *expected [] = { "A/mu", "A/D/gamma", NULL };
SVN_ERR(svn_fs_lock(&l1, fs, "/iota", NULL, NULL, 0, 0, youngest_rev,
FALSE, subpool));
SVN_ERR(svn_fs_lock(&l2, fs, "/A/mu", NULL, NULL, 0, 0, youngest_rev,
FALSE, subpool));
SVN_ERR(svn_fs_lock(&l3, fs, "/A/D/gamma", NULL, NULL, 0, 0, youngest_rev,
FALSE, subpool));
SVN_ERR(svn_fs_unlock(fs, "/A/mu", NULL, TRUE, subpool));
SVN_ERR(svn_fs_lock(&l4, fs, "/A/D/gamma", NULL, NULL, 0, 0, youngest_rev,
TRUE, subpool));
SVN_ERR(create_rmlocks_editor(&editor, &edit_baton, &removed, subpool));
SVN_ERR(svn_repos_begin_report2(&report_baton, 1, repos, "/", "", NULL,
FALSE, svn_depth_infinity, FALSE, FALSE,
editor, edit_baton, NULL, NULL, subpool));
SVN_ERR(svn_repos_set_path3(report_baton, "", 1,
svn_depth_infinity,
FALSE, NULL, subpool));
SVN_ERR(svn_repos_set_path3(report_baton, "iota", 1,
svn_depth_infinity,
FALSE, l1->token, subpool));
SVN_ERR(svn_repos_set_path3(report_baton, "A/mu", 1,
svn_depth_infinity,
FALSE, l2->token, subpool));
SVN_ERR(svn_repos_set_path3(report_baton, "A/D/gamma", 1,
svn_depth_infinity,
FALSE, l3->token, subpool));
SVN_ERR(svn_repos_finish_report(report_baton, pool));
SVN_ERR(rmlocks_check(expected, removed));
}
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
static svn_error_t *
authz_get_handle(svn_authz_t **authz_p, const char *authz_contents,
apr_pool_t *pool) {
apr_file_t *authz_file;
apr_status_t apr_err;
const char *authz_file_path;
svn_error_t *err;
SVN_ERR_W(svn_io_open_unique_file2(&authz_file, &authz_file_path,
"authz_file", "tmp",
svn_io_file_del_none, pool),
"Opening temporary file");
if ((apr_err = apr_file_write_full(authz_file, authz_contents,
strlen(authz_contents), NULL))) {
(void) apr_file_close(authz_file);
(void) apr_file_remove(authz_file_path, pool);
return svn_error_wrap_apr(apr_err, "Writing test authz file");
}
if ((apr_err = apr_file_close(authz_file))) {
(void) apr_file_remove(authz_file_path, pool);
return svn_error_wrap_apr(apr_err, "Closing test authz file");
}
if ((err = svn_repos_authz_read(authz_p, authz_file_path, TRUE, pool))) {
(void) apr_file_remove(authz_file_path, pool);
return svn_error_quick_wrap(err, "Opening test authz file");
}
if ((apr_err = apr_file_remove(authz_file_path, pool)))
return svn_error_wrap_apr(apr_err, "Removing test authz file");
return SVN_NO_ERROR;
}
static svn_error_t *
authz(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
const char *contents;
svn_authz_t *authz_cfg;
svn_error_t *err;
svn_boolean_t access_granted;
apr_pool_t *subpool = svn_pool_create(pool);
int i;
struct {
const char *path;
const char *user;
const svn_repos_authz_access_t required;
const svn_boolean_t expected;
} test_set[] = {
{ "/A", NULL, svn_authz_read, TRUE },
{ "/iota", NULL, svn_authz_read, FALSE },
{ "/A", "plato", svn_authz_write, TRUE },
{ "/A", NULL, svn_authz_write, FALSE },
{ "/A/B/lambda", "plato", svn_authz_read, TRUE },
{ "/A/B/lambda", NULL, svn_authz_read, FALSE },
{ "/A/C", NULL, svn_authz_read, TRUE },
{ "/A/D", "plato", svn_authz_read | svn_authz_recursive, TRUE },
{ "/A/D", NULL, svn_authz_read | svn_authz_recursive, FALSE },
{ NULL, "plato", svn_authz_read, TRUE },
{ NULL, NULL, svn_authz_write, FALSE },
{ NULL, NULL, svn_authz_none, FALSE }
};
*msg = "test authz access control";
if (msg_only)
return SVN_NO_ERROR;
contents =
"[greek:/A]" NL
"* = r" NL
"plato = w" NL
"" NL
"[greek:/iota]" NL
"* =" NL
"" NL
"[/A/B/lambda]" NL
"plato = r" NL
"* =" NL
"" NL
"[greek:/A/D]" NL
"plato = r" NL
"* = r" NL
"" NL
"[greek:/A/D/G]" NL
"plato = r" NL
"* =" NL
"" NL
"[greek:/A/B/E/beta]" NL
"* =" NL
"" NL
"[/nowhere]" NL
"nobody = r" NL
"" NL;
SVN_ERR(authz_get_handle(&authz_cfg, contents, subpool));
for (i = 0; !(test_set[i].path == NULL
&& test_set[i].required == svn_authz_none); i++) {
SVN_ERR(svn_repos_authz_check_access(authz_cfg, "greek",
test_set[i].path,
test_set[i].user,
test_set[i].required,
&access_granted, subpool));
if (access_granted != test_set[i].expected) {
return svn_error_createf(SVN_ERR_TEST_FAILED, NULL,
"Authz incorrectly %s %s%s access "
"to greek:%s for user %s",
access_granted ?
"grants" : "denies",
test_set[i].required
& svn_authz_recursive ?
"recursive " : "",
test_set[i].required
& svn_authz_read ?
"read" : "write",
test_set[i].path,
test_set[i].user ?
test_set[i].user : "-");
}
}
contents =
"[groups]" NL
"slaves = cooks,scribes,@gladiators" NL
"gladiators = equites,thraces,@slaves" NL
"" NL
"[greek:/A]" NL
"@slaves = r" NL;
err = authz_get_handle(&authz_cfg, contents, subpool);
if (!err || err->apr_err != SVN_ERR_AUTHZ_INVALID_CONFIG)
return svn_error_createf(SVN_ERR_TEST_FAILED, err,
"Got %s error instead of expected "
"SVN_ERR_AUTHZ_INVALID_CONFIG",
err ? "unexpected" : "no");
svn_error_clear(err);
contents =
"[greek:/A]" NL
"@senate = r" NL;
err = authz_get_handle(&authz_cfg, contents, subpool);
if (!err || err->apr_err != SVN_ERR_AUTHZ_INVALID_CONFIG)
return svn_error_createf(SVN_ERR_TEST_FAILED, err,
"Got %s error instead of expected "
"SVN_ERR_AUTHZ_INVALID_CONFIG",
err ? "unexpected" : "no");
svn_error_clear(err);
contents =
"[/]" NL
"* = rw" NL
"" NL
"[greek:/dir2/secret]" NL
"* =" NL;
SVN_ERR(authz_get_handle(&authz_cfg, contents, subpool));
SVN_ERR(svn_repos_authz_check_access(authz_cfg, "greek",
"/dir", NULL,
(svn_authz_read
| svn_authz_recursive),
&access_granted, subpool));
if (!access_granted)
return svn_error_create(SVN_ERR_TEST_FAILED, NULL,
"Regression: incomplete ancestry test "
"for recursive access lookup.");
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
static svn_error_t *
commit_authz_cb(svn_repos_authz_access_t required,
svn_boolean_t *allowed,
svn_fs_root_t *root,
const char *path,
void *baton,
apr_pool_t *pool) {
svn_authz_t *authz_file = baton;
return svn_repos_authz_check_access(authz_file, "test", path,
"plato", required, allowed,
pool);
}
static svn_error_t *
commit_editor_authz(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
svn_repos_t *repos;
svn_fs_t *fs;
svn_fs_txn_t *txn;
svn_fs_root_t *txn_root;
svn_revnum_t youngest_rev;
void *edit_baton;
void *root_baton, *dir_baton, *dir2_baton, *file_baton;
svn_error_t *err;
const svn_delta_editor_t *editor;
svn_authz_t *authz_file;
apr_pool_t *subpool = svn_pool_create(pool);
const char *authz_contents;
*msg = "test authz in the commit editor";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(svn_test__create_repos(&repos, "test-repo-commit-authz",
opts->fs_type, subpool));
fs = svn_repos_fs(repos);
SVN_ERR(svn_fs_begin_txn(&txn, fs, 0, subpool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, subpool));
SVN_ERR(svn_test__create_greek_tree(txn_root, subpool));
SVN_ERR(svn_repos_fs_commit_txn(NULL, repos, &youngest_rev, txn, subpool));
authz_contents =
"" NL
"" NL
"[/]" NL
"plato = r" NL
"" NL
"[/A]" NL
"plato = rw" NL
"" NL
"[/A/alpha]" NL
"plato = " NL
"" NL
"[/A/C]" NL
"" NL
"plato = " NL
"" NL
"[/A/D]" NL
"plato = rw" NL
"" NL
"[/A/D/G]" NL
"plato = r";
SVN_ERR(authz_get_handle(&authz_file, authz_contents, subpool));
SVN_ERR(svn_repos_get_commit_editor4(&editor, &edit_baton, repos,
NULL, "file://test", "/",
"plato", "test commit", NULL,
NULL, commit_authz_cb, authz_file,
subpool));
SVN_ERR(editor->open_root(edit_baton, 1, subpool, &root_baton));
err = editor->delete_entry("/iota", SVN_INVALID_REVNUM, root_baton, subpool);
if (err == SVN_NO_ERROR || err->apr_err != SVN_ERR_AUTHZ_UNWRITABLE)
return svn_error_createf(SVN_ERR_TEST_FAILED, err,
"Got %s error instead of expected "
"SVN_ERR_AUTHZ_UNWRITABLE",
err ? "unexpected" : "no");
svn_error_clear(err);
SVN_ERR(editor->open_file("/iota", root_baton, SVN_INVALID_REVNUM,
subpool, &file_baton));
err = editor->change_file_prop(file_baton, "svn:test",
svn_string_create("test", subpool),
subpool);
if (err == SVN_NO_ERROR || err->apr_err != SVN_ERR_AUTHZ_UNWRITABLE)
return svn_error_createf(SVN_ERR_TEST_FAILED, err,
"Got %s error instead of expected "
"SVN_ERR_AUTHZ_UNWRITABLE",
err ? "unexpected" : "no");
svn_error_clear(err);
err = editor->add_file("/alpha", root_baton, NULL, SVN_INVALID_REVNUM,
subpool, &file_baton);
if (err == SVN_NO_ERROR || err->apr_err != SVN_ERR_AUTHZ_UNWRITABLE)
return svn_error_createf(SVN_ERR_TEST_FAILED, err,
"Got %s error instead of expected "
"SVN_ERR_AUTHZ_UNWRITABLE",
err ? "unexpected" : "no");
svn_error_clear(err);
err = editor->add_file("/alpha", root_baton, "file://test/A/B/lambda",
youngest_rev, subpool, &file_baton);
if (err == SVN_NO_ERROR || err->apr_err != SVN_ERR_AUTHZ_UNWRITABLE)
return svn_error_createf(SVN_ERR_TEST_FAILED, err,
"Got %s error instead of expected "
"SVN_ERR_AUTHZ_UNWRITABLE",
err ? "unexpected" : "no");
svn_error_clear(err);
err = editor->add_directory("/I", root_baton, NULL,
SVN_INVALID_REVNUM, subpool, &dir_baton);
if (err == SVN_NO_ERROR || err->apr_err != SVN_ERR_AUTHZ_UNWRITABLE)
return svn_error_createf(SVN_ERR_TEST_FAILED, err,
"Got %s error instead of expected "
"SVN_ERR_AUTHZ_UNWRITABLE",
err ? "unexpected" : "no");
svn_error_clear(err);
err = editor->add_directory("/J", root_baton, "file://test/A/D",
youngest_rev, subpool, &dir_baton);
if (err == SVN_NO_ERROR || err->apr_err != SVN_ERR_AUTHZ_UNWRITABLE)
return svn_error_createf(SVN_ERR_TEST_FAILED, err,
"Got %s error instead of expected "
"SVN_ERR_AUTHZ_UNWRITABLE",
err ? "unexpected" : "no");
svn_error_clear(err);
SVN_ERR(editor->open_directory("/A", root_baton,
SVN_INVALID_REVNUM,
subpool, &dir_baton));
err = editor->add_file("/A/alpha", dir_baton, NULL,
SVN_INVALID_REVNUM, subpool, &file_baton);
if (err == SVN_NO_ERROR || err->apr_err != SVN_ERR_AUTHZ_UNWRITABLE)
return svn_error_createf(SVN_ERR_TEST_FAILED, err,
"Got %s error instead of expected "
"SVN_ERR_AUTHZ_UNWRITABLE",
err ? "unexpected" : "no");
svn_error_clear(err);
SVN_ERR(editor->add_file("/A/B/theta", dir_baton, NULL,
SVN_INVALID_REVNUM, subpool,
&file_baton));
SVN_ERR(editor->delete_entry("/A/mu", SVN_INVALID_REVNUM, dir_baton,
subpool));
SVN_ERR(editor->add_directory("/A/E", dir_baton, NULL,
SVN_INVALID_REVNUM, subpool,
&dir2_baton));
SVN_ERR(editor->add_directory("/A/J", dir_baton, "file://test/A/D",
youngest_rev, subpool,
&dir2_baton));
SVN_ERR(editor->open_directory("/A/D", dir_baton, SVN_INVALID_REVNUM,
subpool, &dir_baton));
err = editor->delete_entry("/A/D/G", SVN_INVALID_REVNUM, dir_baton,
subpool);
if (err == SVN_NO_ERROR || err->apr_err != SVN_ERR_AUTHZ_UNWRITABLE)
return svn_error_createf(SVN_ERR_TEST_FAILED, err,
"Got %s error instead of expected "
"SVN_ERR_AUTHZ_UNWRITABLE",
err ? "unexpected" : "no");
svn_error_clear(err);
SVN_ERR(editor->delete_entry("/A/D/H", SVN_INVALID_REVNUM,
dir_baton, subpool));
SVN_ERR(editor->open_file("/A/D/gamma", dir_baton, SVN_INVALID_REVNUM,
subpool, &file_baton));
SVN_ERR(editor->change_file_prop(file_baton, "svn:test",
svn_string_create("test", subpool),
subpool));
SVN_ERR(editor->abort_edit(edit_baton, subpool));
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
static svn_error_t *
dummy_commit_cb(const svn_commit_info_t *commit_info,
void *baton, apr_pool_t *pool) {
return SVN_NO_ERROR;
}
static svn_error_t *
commit_continue_txn(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
svn_repos_t *repos;
svn_fs_t *fs;
svn_fs_txn_t *txn;
svn_fs_root_t *txn_root, *revision_root;
svn_revnum_t youngest_rev;
void *edit_baton;
void *root_baton, *file_baton;
const svn_delta_editor_t *editor;
apr_pool_t *subpool = svn_pool_create(pool);
const char *txn_name;
*msg = "test commit with explicit txn";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(svn_test__create_repos(&repos, "test-repo-commit-continue",
opts->fs_type, subpool));
fs = svn_repos_fs(repos);
SVN_ERR(svn_fs_begin_txn(&txn, fs, 0, subpool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, subpool));
SVN_ERR(svn_test__create_greek_tree(txn_root, subpool));
SVN_ERR(svn_repos_fs_commit_txn(NULL, repos, &youngest_rev, txn, subpool));
SVN_ERR(svn_fs_begin_txn(&txn, fs, youngest_rev, subpool));
SVN_ERR(svn_fs_txn_name(&txn_name, txn, subpool));
SVN_ERR(svn_repos_get_commit_editor4(&editor, &edit_baton, repos,
txn, "file://test", "/",
"plato", "test commit",
dummy_commit_cb, NULL, NULL, NULL,
subpool));
SVN_ERR(editor->open_root(edit_baton, 1, subpool, &root_baton));
SVN_ERR(editor->add_file("/f1", root_baton, NULL, SVN_INVALID_REVNUM,
subpool, &file_baton));
SVN_ERR(editor->close_file(file_baton, NULL, subpool));
SVN_ERR(editor->abort_edit(edit_baton, subpool));
SVN_ERR(svn_fs_open_txn(&txn, fs, txn_name, subpool));
SVN_ERR(svn_repos_get_commit_editor4(&editor, &edit_baton, repos,
txn, "file://test", "/",
"plato", "test commit",
dummy_commit_cb,
NULL, NULL, NULL,
subpool));
SVN_ERR(editor->open_root(edit_baton, 1, subpool, &root_baton));
SVN_ERR(editor->add_file("/f2", root_baton, NULL, SVN_INVALID_REVNUM,
subpool, &file_baton));
SVN_ERR(editor->close_file(file_baton, NULL, subpool));
SVN_ERR(editor->close_edit(edit_baton, subpool));
{
static svn_test__tree_entry_t expected_entries[] = {
{ "iota", "This is the file 'iota'.\n" },
{ "A", 0 },
{ "A/mu", "This is the file 'mu'.\n" },
{ "A/B", 0 },
{ "A/B/lambda", "This is the file 'lambda'.\n" },
{ "A/B/E", 0 },
{ "A/B/E/alpha", "This is the file 'alpha'.\n" },
{ "A/B/E/beta", "This is the file 'beta'.\n" },
{ "A/B/F", 0 },
{ "A/C", 0 },
{ "A/D", 0 },
{ "A/D/gamma", "This is the file 'gamma'.\n" },
{ "A/D/G", 0 },
{ "A/D/G/pi", "This is the file 'pi'.\n" },
{ "A/D/G/rho", "This is the file 'rho'.\n" },
{ "A/D/G/tau", "This is the file 'tau'.\n" },
{ "A/D/H", 0 },
{ "A/D/H/chi", "This is the file 'chi'.\n" },
{ "A/D/H/psi", "This is the file 'psi'.\n" },
{ "A/D/H/omega", "This is the file 'omega'.\n" },
{ "f1", "" },
{ "f2", "" }
};
SVN_ERR(svn_fs_revision_root(&revision_root, fs,
2, subpool));
SVN_ERR(svn_test__validate_tree
(revision_root, expected_entries,
sizeof(expected_entries) / sizeof(expected_entries[0]),
subpool));
}
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
struct nls_receiver_baton {
int count;
svn_location_segment_t *expected_segments;
};
static const char *
format_segment(svn_location_segment_t *segment,
apr_pool_t *pool) {
return apr_psprintf(pool, "[r%ld-r%ld: /%s]",
segment->range_start,
segment->range_end,
segment->path ? segment->path : "(null)");
}
static svn_error_t *
nls_receiver(svn_location_segment_t *segment,
void *baton,
apr_pool_t *pool) {
struct nls_receiver_baton *b = baton;
svn_location_segment_t *expected_segment = b->expected_segments + b->count;
if (! expected_segment->range_end)
return svn_error_createf(SVN_ERR_TEST_FAILED, NULL,
"Got unexpected location segment: %s",
format_segment(segment, pool));
if (expected_segment->range_start != segment->range_start)
return svn_error_createf(SVN_ERR_TEST_FAILED, NULL,
"Location segments differ\n"
" Expected location segment: %s\n"
" Actual location segment: %s",
format_segment(expected_segment, pool),
format_segment(segment, pool));
b->count++;
return SVN_NO_ERROR;
}
static svn_error_t *
check_location_segments(svn_repos_t *repos,
const char *path,
svn_revnum_t peg_rev,
svn_revnum_t start_rev,
svn_revnum_t end_rev,
svn_location_segment_t *expected_segments,
apr_pool_t *pool) {
struct nls_receiver_baton b;
svn_location_segment_t *segment;
b.count = 0;
b.expected_segments = expected_segments;
SVN_ERR(svn_repos_node_location_segments(repos, path, peg_rev,
start_rev, end_rev, nls_receiver,
&b, NULL, NULL, pool));
segment = expected_segments + b.count;
if (segment->range_end)
return svn_error_createf(SVN_ERR_TEST_FAILED, NULL,
"Failed to get expected location segment: %s",
format_segment(segment, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
node_location_segments(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
apr_pool_t *subpool = svn_pool_create(pool);
svn_repos_t *repos;
svn_fs_t *fs;
svn_fs_txn_t *txn;
svn_fs_root_t *txn_root, *root;
svn_revnum_t youngest_rev = 0;
*msg = "test svn_repos_node_location_segments";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(svn_test__create_repos(&repos, "test-repo-node-location-segments",
opts->fs_type, pool));
fs = svn_repos_fs(repos);
SVN_ERR(svn_fs_begin_txn(&txn, fs, 0, subpool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, subpool));
SVN_ERR(svn_test__create_greek_tree(txn_root, subpool));
SVN_ERR(svn_repos_fs_commit_txn(NULL, repos, &youngest_rev, txn, subpool));
svn_pool_clear(subpool);
SVN_ERR(svn_fs_begin_txn(&txn, fs, youngest_rev, subpool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, subpool));
SVN_ERR(svn_test__set_file_contents(txn_root, "A/D/H/chi", "2", subpool));
SVN_ERR(svn_test__set_file_contents(txn_root, "A/B/E/alpha", "2", subpool));
SVN_ERR(svn_repos_fs_commit_txn(NULL, repos, &youngest_rev, txn, subpool));
svn_pool_clear(subpool);
SVN_ERR(svn_fs_begin_txn(&txn, fs, youngest_rev, subpool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, subpool));
SVN_ERR(svn_fs_revision_root(&root, fs, youngest_rev, subpool));
SVN_ERR(svn_fs_copy(root, "A/D", txn_root, "A/D2", subpool));
SVN_ERR(svn_repos_fs_commit_txn(NULL, repos, &youngest_rev, txn, subpool));
svn_pool_clear(subpool);
SVN_ERR(svn_fs_begin_txn(&txn, fs, youngest_rev, subpool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, subpool));
SVN_ERR(svn_test__set_file_contents(txn_root, "A/D/H/chi", "4", subpool));
SVN_ERR(svn_test__set_file_contents(txn_root, "A/D2/H/chi", "4", subpool));
SVN_ERR(svn_repos_fs_commit_txn(NULL, repos, &youngest_rev, txn, subpool));
svn_pool_clear(subpool);
SVN_ERR(svn_fs_begin_txn(&txn, fs, youngest_rev, subpool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, subpool));
SVN_ERR(svn_fs_delete(txn_root, "A/D2/G", subpool));
SVN_ERR(svn_repos_fs_commit_txn(NULL, repos, &youngest_rev, txn, subpool));
svn_pool_clear(subpool);
SVN_ERR(svn_fs_begin_txn(&txn, fs, youngest_rev, subpool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, subpool));
SVN_ERR(svn_fs_revision_root(&root, fs, 4, subpool));
SVN_ERR(svn_fs_copy(root, "A/D2/G", txn_root, "A/D2/G", subpool));
SVN_ERR(svn_repos_fs_commit_txn(NULL, repos, &youngest_rev, txn, subpool));
svn_pool_clear(subpool);
SVN_ERR(svn_fs_begin_txn(&txn, fs, youngest_rev, subpool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, subpool));
SVN_ERR(svn_fs_revision_root(&root, fs, youngest_rev, subpool));
SVN_ERR(svn_fs_delete(txn_root, "A/D", subpool));
SVN_ERR(svn_fs_copy(root, "A/D2", txn_root, "A/D", subpool));
SVN_ERR(svn_fs_delete(txn_root, "A/D2", subpool));
SVN_ERR(svn_repos_fs_commit_txn(NULL, repos, &youngest_rev, txn, subpool));
svn_pool_clear(subpool);
{
svn_location_segment_t expected_segments[] = {
{ 0, 7, "" },
{ 0 }
};
SVN_ERR(check_location_segments(repos, "",
SVN_INVALID_REVNUM,
SVN_INVALID_REVNUM,
SVN_INVALID_REVNUM,
expected_segments, pool));
}
{
svn_location_segment_t expected_segments[] = {
{ 7, 7, "A/D" },
{ 3, 6, "A/D2" },
{ 1, 2, "A/D" },
{ 0 }
};
SVN_ERR(check_location_segments(repos, "A/D",
SVN_INVALID_REVNUM,
SVN_INVALID_REVNUM,
SVN_INVALID_REVNUM,
expected_segments, pool));
}
{
svn_location_segment_t expected_segments[] = {
{ 3, 5, "A/D2" },
{ 2, 2, "A/D" },
{ 0 }
};
SVN_ERR(check_location_segments(repos, "A/D",
SVN_INVALID_REVNUM,
5,
2,
expected_segments, pool));
}
{
svn_location_segment_t expected_segments[] = {
{ 3, 3, "A/D2" },
{ 2, 2, "A/D" },
{ 0 }
};
SVN_ERR(check_location_segments(repos, "A/D2",
5,
3,
2,
expected_segments, pool));
}
{
svn_location_segment_t expected_segments[] = {
{ 1, 6, "A/D" },
{ 0 }
};
SVN_ERR(check_location_segments(repos, "A/D",
6,
6,
SVN_INVALID_REVNUM,
expected_segments, pool));
}
{
svn_location_segment_t expected_segments[] = {
{ 7, 7, "A/D/G" },
{ 6, 6, "A/D2/G" },
{ 5, 5, NULL },
{ 3, 4, "A/D2/G" },
{ 1, 2, "A/D2/G" },
{ 0 }
};
SVN_ERR(check_location_segments(repos, "A/D/G",
SVN_INVALID_REVNUM,
SVN_INVALID_REVNUM,
SVN_INVALID_REVNUM,
expected_segments, pool));
}
{
svn_location_segment_t expected_segments[] = {
{ 3, 3, "A/D2/G" },
{ 2, 2, "A/D2/G" },
{ 0 }
};
SVN_ERR(check_location_segments(repos, "A/D/G",
SVN_INVALID_REVNUM,
3,
2,
expected_segments, pool));
}
return SVN_NO_ERROR;
}
static svn_error_t *
reporter_depth_exclude(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
svn_repos_t *repos;
svn_fs_t *fs;
svn_fs_txn_t *txn;
svn_fs_root_t *txn_root;
apr_pool_t *subpool = svn_pool_create(pool);
svn_revnum_t youngest_rev;
const svn_delta_editor_t *editor;
void *edit_baton, *report_baton;
svn_error_t *err;
*msg = "test reporter and svn_depth_exclude";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(svn_test__create_repos(&repos, "test-repo-reporter-depth-exclude",
opts->fs_type, pool));
fs = svn_repos_fs(repos);
SVN_ERR(svn_fs_begin_txn(&txn, fs, 0, subpool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, subpool));
SVN_ERR(svn_test__create_greek_tree(txn_root, subpool));
SVN_ERR(svn_repos_fs_commit_txn(NULL, repos, &youngest_rev, txn, subpool));
svn_pool_clear(subpool);
SVN_ERR(svn_fs_begin_txn(&txn, fs, youngest_rev, subpool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, subpool));
{
static svn_test__txn_script_command_t script_entries[] = {
{ 'e', "iota", "Changed file 'iota'.\n" },
{ 'e', "A/D/G/pi", "Changed file 'pi'.\n" },
{ 'e', "A/mu", "Changed file 'mu'.\n" },
{ 'a', "A/D/foo", "New file 'foo'.\n" },
{ 'a', "A/B/bar", "New file 'bar'.\n" },
{ 'd', "A/D/H", NULL },
{ 'd', "A/B/E/beta", NULL }
};
SVN_ERR(svn_test__txn_script_exec(txn_root,
script_entries,
sizeof(script_entries)/
sizeof(script_entries[0]),
subpool));
}
SVN_ERR(svn_repos_fs_commit_txn(NULL, repos, &youngest_rev, txn, subpool));
svn_pool_clear(subpool);
{
svn_fs_root_t *revision_root;
static svn_test__tree_entry_t entries[] = {
{ "iota", "Changed file 'iota'.\n" },
{ "A", 0 },
{ "A/mu", "Changed file 'mu'.\n" },
{ "A/B", 0 },
{ "A/B/bar", "New file 'bar'.\n" },
{ "A/B/lambda", "This is the file 'lambda'.\n" },
{ "A/B/E", 0 },
{ "A/B/E/alpha", "This is the file 'alpha'.\n" },
{ "A/B/F", 0 },
{ "A/C", 0 },
{ "A/D", 0 },
{ "A/D/foo", "New file 'foo'.\n" },
{ "A/D/gamma", "This is the file 'gamma'.\n" },
{ "A/D/G", 0 },
{ "A/D/G/pi", "Changed file 'pi'.\n" },
{ "A/D/G/rho", "This is the file 'rho'.\n" },
{ "A/D/G/tau", "This is the file 'tau'.\n" },
};
SVN_ERR(svn_fs_revision_root(&revision_root, fs,
youngest_rev, subpool));
SVN_ERR(svn_test__validate_tree(revision_root,
entries,
sizeof(entries)/sizeof(entries[0]),
subpool));
}
SVN_ERR(svn_fs_begin_txn(&txn, fs, 1, subpool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, subpool));
SVN_ERR(dir_delta_get_editor(&editor, &edit_baton, fs,
txn_root, "", subpool));
SVN_ERR(svn_repos_begin_report2(&report_baton, 2, repos, "/", "", NULL,
TRUE, svn_depth_infinity, FALSE, FALSE,
editor, edit_baton, NULL, NULL, subpool));
SVN_ERR(svn_repos_set_path3(report_baton, "", 1,
svn_depth_infinity,
FALSE, NULL, subpool));
SVN_ERR(svn_repos_set_path3(report_baton, "iota", SVN_INVALID_REVNUM,
svn_depth_exclude,
FALSE, NULL, subpool));
SVN_ERR(svn_repos_set_path3(report_baton, "A/D", SVN_INVALID_REVNUM,
svn_depth_exclude,
FALSE, NULL, subpool));
SVN_ERR(svn_repos_finish_report(report_baton, subpool));
{
static svn_test__tree_entry_t entries[] = {
{ "iota", "This is the file 'iota'.\n" },
{ "A", 0 },
{ "A/mu", "Changed file 'mu'.\n" },
{ "A/B", 0 },
{ "A/B/bar", "New file 'bar'.\n" },
{ "A/B/lambda", "This is the file 'lambda'.\n" },
{ "A/B/E", 0 },
{ "A/B/E/alpha", "This is the file 'alpha'.\n" },
{ "A/B/F", 0 },
{ "A/C", 0 },
{ "A/D", 0 },
{ "A/D/gamma", "This is the file 'gamma'.\n" },
{ "A/D/G", 0 },
{ "A/D/G/pi", "This is the file 'pi'.\n" },
{ "A/D/G/rho", "This is the file 'rho'.\n" },
{ "A/D/G/tau", "This is the file 'tau'.\n" },
{ "A/D/H", 0 },
{ "A/D/H/chi", "This is the file 'chi'.\n" },
{ "A/D/H/psi", "This is the file 'psi'.\n" },
{ "A/D/H/omega", "This is the file 'omega'.\n" }
};
SVN_ERR(svn_test__validate_tree(txn_root,
entries,
sizeof(entries)/sizeof(entries[0]),
subpool));
svn_pool_clear(subpool);
}
svn_error_clear(svn_fs_abort_txn(txn, subpool));
SVN_ERR(svn_fs_begin_txn(&txn, fs, 1, subpool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, subpool));
SVN_ERR(dir_delta_get_editor(&editor, &edit_baton, fs,
txn_root, "", subpool));
SVN_ERR(svn_repos_begin_report2(&report_baton, 2, repos, "/", "", NULL,
TRUE, svn_depth_infinity, FALSE, FALSE,
editor, edit_baton, NULL, NULL, subpool));
SVN_ERR(svn_repos_set_path3(report_baton, "", 1,
svn_depth_infinity,
FALSE, NULL, subpool));
SVN_ERR(svn_repos_set_path3(report_baton, "iota", SVN_INVALID_REVNUM,
svn_depth_exclude,
FALSE, NULL, subpool));
SVN_ERR(svn_repos_set_path3(report_baton, "A/D", SVN_INVALID_REVNUM,
svn_depth_exclude,
FALSE, NULL, subpool));
SVN_ERR(svn_repos_set_path3(report_baton, "A/D/G/pi",
SVN_INVALID_REVNUM,
svn_depth_infinity,
FALSE, NULL, subpool));
err = svn_repos_finish_report(report_baton, subpool);
if (! err) {
return svn_error_createf
(SVN_ERR_TEST_FAILED, NULL,
"Illegal report of \"A/D/G/pi\" did not error as expected");
} else if (err->apr_err != SVN_ERR_FS_NOT_FOUND) {
return svn_error_createf
(SVN_ERR_TEST_FAILED, err,
"Illegal report of \"A/D/G/pi\" got wrong kind of error:");
}
svn_error_clear(err);
svn_error_clear(svn_fs_abort_txn(txn, subpool));
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
struct svn_test_descriptor_t test_funcs[] = {
SVN_TEST_NULL,
SVN_TEST_PASS(dir_deltas),
SVN_TEST_PASS(node_tree_delete_under_copy),
SVN_TEST_PASS(revisions_changed),
SVN_TEST_PASS(node_locations),
SVN_TEST_PASS(node_locations2),
SVN_TEST_PASS(rmlocks),
SVN_TEST_PASS(authz),
SVN_TEST_PASS(commit_editor_authz),
SVN_TEST_PASS(commit_continue_txn),
SVN_TEST_PASS(node_location_segments),
SVN_TEST_PASS(reporter_depth_exclude),
SVN_TEST_NULL
};
