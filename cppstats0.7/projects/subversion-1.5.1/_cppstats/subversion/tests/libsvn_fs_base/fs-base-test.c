#include <stdlib.h>
#include <string.h>
#include <apr_pools.h>
#include "svn_pools.h"
#include "svn_time.h"
#include "svn_string.h"
#include "svn_fs.h"
#include "svn_md5.h"
#include "../svn_test.h"
#include "../svn_test_fs.h"
#include "../../libsvn_fs_base/id.h"
#include "../../libsvn_fs_base/trail.h"
#include "../../libsvn_fs_base/bdb/txn-table.h"
#include "../../libsvn_fs_base/bdb/nodes-table.h"
#include "private/svn_fs_util.h"
#include "../../libsvn_delta/delta.h"
#define SET_STR(ps, s) ((ps)->data = (s), (ps)->len = strlen(s))
static svn_error_t *
create_berkeley_filesystem(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
svn_fs_t *fs;
*msg = "svn_fs_create_berkeley";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(svn_test__create_fs(&fs, "test-repo-create-berkeley",
"bdb", pool));
return SVN_NO_ERROR;
}
static void
berkeley_error_handler(const char *errpfx, char *msg) {
fprintf(stderr, "%s%s\n", errpfx ? errpfx : "", msg);
}
static svn_error_t *
open_berkeley_filesystem(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
svn_fs_t *fs, *fs2;
*msg = "open an existing Berkeley DB filesystem";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(svn_test__create_fs(&fs, "test-repo-open-berkeley",
"bdb", pool));
SVN_ERR(svn_test__fs_new(&fs2, pool));
SVN_ERR(svn_fs_open_berkeley(fs2, "test-repo-open-berkeley"));
SVN_ERR(svn_fs_set_berkeley_errcall(fs2, berkeley_error_handler));
return SVN_NO_ERROR;
}
static svn_error_t *
check_entry(svn_fs_root_t *root,
const char *path,
const char *name,
svn_boolean_t *present,
apr_pool_t *pool) {
apr_hash_t *entries;
svn_fs_dirent_t *ent;
SVN_ERR(svn_fs_dir_entries(&entries, root, path, pool));
ent = apr_hash_get(entries, name, APR_HASH_KEY_STRING);
if (ent)
*present = TRUE;
else
*present = FALSE;
return SVN_NO_ERROR;
}
static svn_error_t *
check_entry_present(svn_fs_root_t *root, const char *path,
const char *name, apr_pool_t *pool) {
svn_boolean_t present;
SVN_ERR(check_entry(root, path, name, &present, pool));
if (! present)
return svn_error_createf
(SVN_ERR_FS_GENERAL, NULL,
"entry \"%s\" absent when it should be present", name);
return SVN_NO_ERROR;
}
static svn_error_t *
check_entry_absent(svn_fs_root_t *root, const char *path,
const char *name, apr_pool_t *pool) {
svn_boolean_t present;
SVN_ERR(check_entry(root, path, name, &present, pool));
if (present)
return svn_error_createf
(SVN_ERR_FS_GENERAL, NULL,
"entry \"%s\" present when it should be absent", name);
return SVN_NO_ERROR;
}
struct check_id_args {
svn_fs_t *fs;
const svn_fs_id_t *id;
svn_boolean_t present;
};
static svn_error_t *
txn_body_check_id(void *baton, trail_t *trail) {
struct check_id_args *args = baton;
node_revision_t *noderev;
svn_error_t *err;
err = svn_fs_bdb__get_node_revision(&noderev, args->fs, args->id,
trail, trail->pool);
if (err && (err->apr_err == SVN_ERR_FS_ID_NOT_FOUND))
args->present = FALSE;
else if (! err)
args->present = TRUE;
else {
svn_string_t *id_str = svn_fs_unparse_id(args->id, trail->pool);
return svn_error_createf
(SVN_ERR_FS_GENERAL, err,
"error looking for node revision id \"%s\"", id_str->data);
}
svn_error_clear(err);
return SVN_NO_ERROR;
}
static svn_error_t *
check_id(svn_fs_t *fs, const svn_fs_id_t *id, svn_boolean_t *present,
apr_pool_t *pool) {
struct check_id_args args;
args.id = id;
args.fs = fs;
SVN_ERR(svn_fs_base__retry_txn(fs, txn_body_check_id, &args, pool));
if (args.present)
*present = TRUE;
else
*present = FALSE;
return SVN_NO_ERROR;
}
static svn_error_t *
check_id_present(svn_fs_t *fs, const svn_fs_id_t *id, apr_pool_t *pool) {
svn_boolean_t present;
SVN_ERR(check_id(fs, id, &present, pool));
if (! present) {
svn_string_t *id_str = svn_fs_unparse_id(id, pool);
return svn_error_createf
(SVN_ERR_FS_GENERAL, NULL,
"node revision id \"%s\" absent when should be present",
id_str->data);
}
return SVN_NO_ERROR;
}
static svn_error_t *
check_id_absent(svn_fs_t *fs, const svn_fs_id_t *id, apr_pool_t *pool) {
svn_boolean_t present;
SVN_ERR(check_id(fs, id, &present, pool));
if (present) {
svn_string_t *id_str = svn_fs_unparse_id(id, pool);
return svn_error_createf
(SVN_ERR_FS_GENERAL, NULL,
"node revision id \"%s\" present when should be absent",
id_str->data);
}
return SVN_NO_ERROR;
}
static svn_error_t *
abort_txn(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
svn_fs_t *fs;
svn_fs_txn_t *txn1, *txn2;
svn_fs_root_t *txn1_root, *txn2_root;
const char *txn1_name, *txn2_name;
*msg = "abort a transaction";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(svn_test__create_fs(&fs, "test-repo-abort-txn",
"bdb", pool));
SVN_ERR(svn_fs_begin_txn(&txn1, fs, 0, pool));
SVN_ERR(svn_fs_begin_txn(&txn2, fs, 0, pool));
SVN_ERR(svn_fs_txn_root(&txn1_root, txn1, pool));
SVN_ERR(svn_fs_txn_root(&txn2_root, txn2, pool));
SVN_ERR(svn_fs_txn_name(&txn1_name, txn1, pool));
SVN_ERR(svn_fs_txn_name(&txn2_name, txn2, pool));
SVN_ERR(svn_test__create_greek_tree(txn1_root, pool));
SVN_ERR(svn_test__create_greek_tree(txn2_root, pool));
{
const svn_fs_id_t
*t1_root_id, *t2_root_id,
*t1_iota_id, *t2_iota_id,
*t1_A_id, *t2_A_id,
*t1_mu_id, *t2_mu_id,
*t1_B_id, *t2_B_id,
*t1_lambda_id, *t2_lambda_id,
*t1_E_id, *t2_E_id,
*t1_alpha_id, *t2_alpha_id,
*t1_beta_id, *t2_beta_id,
*t1_F_id, *t2_F_id,
*t1_C_id, *t2_C_id,
*t1_D_id, *t2_D_id,
*t1_gamma_id, *t2_gamma_id,
*t1_H_id, *t2_H_id,
*t1_chi_id, *t2_chi_id,
*t1_psi_id, *t2_psi_id,
*t1_omega_id, *t2_omega_id,
*t1_G_id, *t2_G_id,
*t1_pi_id, *t2_pi_id,
*t1_rho_id, *t2_rho_id,
*t1_tau_id, *t2_tau_id;
SVN_ERR(svn_fs_node_id(&t1_root_id, txn1_root, "", pool));
SVN_ERR(svn_fs_node_id(&t2_root_id, txn2_root, "", pool));
SVN_ERR(svn_fs_node_id(&t1_iota_id, txn1_root, "iota", pool));
SVN_ERR(svn_fs_node_id(&t2_iota_id, txn2_root, "iota", pool));
SVN_ERR(svn_fs_node_id(&t1_A_id, txn1_root, "/A", pool));
SVN_ERR(svn_fs_node_id(&t2_A_id, txn2_root, "/A", pool));
SVN_ERR(svn_fs_node_id(&t1_mu_id, txn1_root, "/A/mu", pool));
SVN_ERR(svn_fs_node_id(&t2_mu_id, txn2_root, "/A/mu", pool));
SVN_ERR(svn_fs_node_id(&t1_B_id, txn1_root, "/A/B", pool));
SVN_ERR(svn_fs_node_id(&t2_B_id, txn2_root, "/A/B", pool));
SVN_ERR(svn_fs_node_id(&t1_lambda_id, txn1_root, "/A/B/lambda", pool));
SVN_ERR(svn_fs_node_id(&t2_lambda_id, txn2_root, "/A/B/lambda", pool));
SVN_ERR(svn_fs_node_id(&t1_E_id, txn1_root, "/A/B/E", pool));
SVN_ERR(svn_fs_node_id(&t2_E_id, txn2_root, "/A/B/E", pool));
SVN_ERR(svn_fs_node_id(&t1_alpha_id, txn1_root, "/A/B/E/alpha", pool));
SVN_ERR(svn_fs_node_id(&t2_alpha_id, txn2_root, "/A/B/E/alpha", pool));
SVN_ERR(svn_fs_node_id(&t1_beta_id, txn1_root, "/A/B/E/beta", pool));
SVN_ERR(svn_fs_node_id(&t2_beta_id, txn2_root, "/A/B/E/beta", pool));
SVN_ERR(svn_fs_node_id(&t1_F_id, txn1_root, "/A/B/F", pool));
SVN_ERR(svn_fs_node_id(&t2_F_id, txn2_root, "/A/B/F", pool));
SVN_ERR(svn_fs_node_id(&t1_C_id, txn1_root, "/A/C", pool));
SVN_ERR(svn_fs_node_id(&t2_C_id, txn2_root, "/A/C", pool));
SVN_ERR(svn_fs_node_id(&t1_D_id, txn1_root, "/A/D", pool));
SVN_ERR(svn_fs_node_id(&t2_D_id, txn2_root, "/A/D", pool));
SVN_ERR(svn_fs_node_id(&t1_gamma_id, txn1_root, "/A/D/gamma", pool));
SVN_ERR(svn_fs_node_id(&t2_gamma_id, txn2_root, "/A/D/gamma", pool));
SVN_ERR(svn_fs_node_id(&t1_H_id, txn1_root, "/A/D/H", pool));
SVN_ERR(svn_fs_node_id(&t2_H_id, txn2_root, "/A/D/H", pool));
SVN_ERR(svn_fs_node_id(&t1_chi_id, txn1_root, "/A/D/H/chi", pool));
SVN_ERR(svn_fs_node_id(&t2_chi_id, txn2_root, "/A/D/H/chi", pool));
SVN_ERR(svn_fs_node_id(&t1_psi_id, txn1_root, "/A/D/H/psi", pool));
SVN_ERR(svn_fs_node_id(&t2_psi_id, txn2_root, "/A/D/H/psi", pool));
SVN_ERR(svn_fs_node_id(&t1_omega_id, txn1_root, "/A/D/H/omega", pool));
SVN_ERR(svn_fs_node_id(&t2_omega_id, txn2_root, "/A/D/H/omega", pool));
SVN_ERR(svn_fs_node_id(&t1_G_id, txn1_root, "/A/D/G", pool));
SVN_ERR(svn_fs_node_id(&t2_G_id, txn2_root, "/A/D/G", pool));
SVN_ERR(svn_fs_node_id(&t1_pi_id, txn1_root, "/A/D/G/pi", pool));
SVN_ERR(svn_fs_node_id(&t2_pi_id, txn2_root, "/A/D/G/pi", pool));
SVN_ERR(svn_fs_node_id(&t1_rho_id, txn1_root, "/A/D/G/rho", pool));
SVN_ERR(svn_fs_node_id(&t2_rho_id, txn2_root, "/A/D/G/rho", pool));
SVN_ERR(svn_fs_node_id(&t1_tau_id, txn1_root, "/A/D/G/tau", pool));
SVN_ERR(svn_fs_node_id(&t2_tau_id, txn2_root, "/A/D/G/tau", pool));
SVN_ERR(svn_fs_abort_txn(txn2, pool));
SVN_ERR(check_id_absent(fs, t2_root_id, pool));
SVN_ERR(check_id_absent(fs, t2_iota_id, pool));
SVN_ERR(check_id_absent(fs, t2_A_id, pool));
SVN_ERR(check_id_absent(fs, t2_mu_id, pool));
SVN_ERR(check_id_absent(fs, t2_B_id, pool));
SVN_ERR(check_id_absent(fs, t2_lambda_id, pool));
SVN_ERR(check_id_absent(fs, t2_E_id, pool));
SVN_ERR(check_id_absent(fs, t2_alpha_id, pool));
SVN_ERR(check_id_absent(fs, t2_beta_id, pool));
SVN_ERR(check_id_absent(fs, t2_F_id, pool));
SVN_ERR(check_id_absent(fs, t2_C_id, pool));
SVN_ERR(check_id_absent(fs, t2_D_id, pool));
SVN_ERR(check_id_absent(fs, t2_gamma_id, pool));
SVN_ERR(check_id_absent(fs, t2_H_id, pool));
SVN_ERR(check_id_absent(fs, t2_chi_id, pool));
SVN_ERR(check_id_absent(fs, t2_psi_id, pool));
SVN_ERR(check_id_absent(fs, t2_omega_id, pool));
SVN_ERR(check_id_absent(fs, t2_G_id, pool));
SVN_ERR(check_id_absent(fs, t2_pi_id, pool));
SVN_ERR(check_id_absent(fs, t2_rho_id, pool));
SVN_ERR(check_id_absent(fs, t2_tau_id, pool));
SVN_ERR(check_id_present(fs, t1_root_id, pool));
SVN_ERR(check_id_present(fs, t1_iota_id, pool));
SVN_ERR(check_id_present(fs, t1_A_id, pool));
SVN_ERR(check_id_present(fs, t1_mu_id, pool));
SVN_ERR(check_id_present(fs, t1_B_id, pool));
SVN_ERR(check_id_present(fs, t1_lambda_id, pool));
SVN_ERR(check_id_present(fs, t1_E_id, pool));
SVN_ERR(check_id_present(fs, t1_alpha_id, pool));
SVN_ERR(check_id_present(fs, t1_beta_id, pool));
SVN_ERR(check_id_present(fs, t1_F_id, pool));
SVN_ERR(check_id_present(fs, t1_C_id, pool));
SVN_ERR(check_id_present(fs, t1_D_id, pool));
SVN_ERR(check_id_present(fs, t1_gamma_id, pool));
SVN_ERR(check_id_present(fs, t1_H_id, pool));
SVN_ERR(check_id_present(fs, t1_chi_id, pool));
SVN_ERR(check_id_present(fs, t1_psi_id, pool));
SVN_ERR(check_id_present(fs, t1_omega_id, pool));
SVN_ERR(check_id_present(fs, t1_G_id, pool));
SVN_ERR(check_id_present(fs, t1_pi_id, pool));
SVN_ERR(check_id_present(fs, t1_rho_id, pool));
SVN_ERR(check_id_present(fs, t1_tau_id, pool));
}
{
svn_fs_txn_t *txn2_again;
svn_error_t *err;
err = svn_fs_open_txn(&txn2_again, fs, txn2_name, pool);
if (err && (err->apr_err != SVN_ERR_FS_NO_SUCH_TRANSACTION)) {
return svn_error_create
(SVN_ERR_FS_GENERAL, err,
"opening non-existent txn got wrong error");
} else if (! err) {
return svn_error_create
(SVN_ERR_FS_GENERAL, NULL,
"opening non-existent txn failed to get error");
}
svn_error_clear(err);
}
{
svn_fs_txn_t *txn3;
const char *txn3_name;
SVN_ERR(svn_fs_begin_txn(&txn3, fs, 0, pool));
SVN_ERR(svn_fs_txn_name(&txn3_name, txn3, pool));
if ((strcmp(txn3_name, txn2_name) == 0)
|| (strcmp(txn3_name, txn1_name) == 0)) {
return svn_error_createf
(SVN_ERR_FS_GENERAL, NULL,
"txn name \"%s\" was recycled", txn3_name);
}
}
{
svn_fs_txn_t *txn4;
const char *txn4_name;
svn_revnum_t new_rev;
const char *conflict;
svn_error_t *err;
SVN_ERR(svn_fs_begin_txn(&txn4, fs, 0, pool));
SVN_ERR(svn_fs_txn_name(&txn4_name, txn4, pool));
SVN_ERR(svn_fs_commit_txn(&conflict, &new_rev, txn4, pool));
err = svn_fs_abort_txn(txn4, pool);
if (! err)
return svn_error_create
(SVN_ERR_FS_GENERAL, NULL,
"expected error trying to abort a committed txn; got none");
else if (err->apr_err != SVN_ERR_FS_TRANSACTION_NOT_MUTABLE)
return svn_error_create
(SVN_ERR_FS_GENERAL, err,
"got an unexpected error trying to abort a committed txn");
else
svn_error_clear(err);
}
return SVN_NO_ERROR;
}
static svn_error_t *
delete_mutables(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
svn_fs_t *fs;
svn_fs_txn_t *txn;
svn_fs_root_t *txn_root;
svn_error_t *err;
*msg = "delete mutable nodes from directories";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(svn_test__create_fs(&fs, "test-repo-del-from-dir",
"bdb", pool));
SVN_ERR(svn_fs_begin_txn(&txn, fs, 0, pool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, pool));
SVN_ERR(svn_test__create_greek_tree(txn_root, pool));
{
const svn_fs_id_t *gamma_id;
SVN_ERR(svn_fs_node_id(&gamma_id, txn_root, "A/D/gamma", pool));
SVN_ERR(check_entry_present(txn_root, "A/D", "gamma", pool));
SVN_ERR(check_id_present(fs, gamma_id, pool));
SVN_ERR(svn_fs_delete(txn_root, "A/D/gamma", pool));
SVN_ERR(check_entry_absent(txn_root, "A/D", "gamma", pool));
SVN_ERR(check_id_absent(fs, gamma_id, pool));
}
{
const svn_fs_id_t *pi_id, *rho_id, *tau_id;
SVN_ERR(svn_fs_node_id(&pi_id, txn_root, "A/D/G/pi", pool));
SVN_ERR(svn_fs_node_id(&rho_id, txn_root, "A/D/G/rho", pool));
SVN_ERR(svn_fs_node_id(&tau_id, txn_root, "A/D/G/tau", pool));
SVN_ERR(check_entry_present(txn_root, "A/D/G", "pi", pool));
SVN_ERR(check_entry_present(txn_root, "A/D/G", "rho", pool));
SVN_ERR(check_entry_present(txn_root, "A/D/G", "tau", pool));
SVN_ERR(check_id_present(fs, pi_id, pool));
SVN_ERR(check_id_present(fs, rho_id, pool));
SVN_ERR(check_id_present(fs, tau_id, pool));
SVN_ERR(svn_fs_delete(txn_root, "A/D/G/pi", pool));
SVN_ERR(check_entry_absent(txn_root, "A/D/G", "pi", pool));
SVN_ERR(check_entry_present(txn_root, "A/D/G", "rho", pool));
SVN_ERR(check_entry_present(txn_root, "A/D/G", "tau", pool));
SVN_ERR(check_id_absent(fs, pi_id, pool));
SVN_ERR(check_id_present(fs, rho_id, pool));
SVN_ERR(check_id_present(fs, tau_id, pool));
SVN_ERR(svn_fs_delete(txn_root, "A/D/G/rho", pool));
SVN_ERR(check_entry_absent(txn_root, "A/D/G", "pi", pool));
SVN_ERR(check_entry_absent(txn_root, "A/D/G", "rho", pool));
SVN_ERR(check_entry_present(txn_root, "A/D/G", "tau", pool));
SVN_ERR(check_id_absent(fs, pi_id, pool));
SVN_ERR(check_id_absent(fs, rho_id, pool));
SVN_ERR(check_id_present(fs, tau_id, pool));
}
{
const svn_fs_id_t *tau_id;
SVN_ERR(svn_fs_node_id(&tau_id, txn_root, "A/D/G/tau", pool));
SVN_ERR(check_entry_present(txn_root, "A/D/G", "tau", pool));
SVN_ERR(check_id_present(fs, tau_id, pool));
SVN_ERR(svn_fs_delete(txn_root, "A/D/G/tau", pool));
SVN_ERR(check_entry_absent(txn_root, "A/D/G", "tau", pool));
SVN_ERR(check_id_absent(fs, tau_id, pool));
}
{
const svn_fs_id_t *G_id;
SVN_ERR(svn_fs_node_id(&G_id, txn_root, "A/D/G", pool));
SVN_ERR(check_entry_present(txn_root, "A/D", "G", pool));
SVN_ERR(check_id_present(fs, G_id, pool));
SVN_ERR(svn_fs_delete(txn_root, "A/D/G", pool));
SVN_ERR(check_entry_absent(txn_root, "A/D", "G", pool));
SVN_ERR(check_id_absent(fs, G_id, pool));
}
{
const svn_fs_id_t *C_id;
SVN_ERR(svn_fs_node_id(&C_id, txn_root, "A/C", pool));
SVN_ERR(check_entry_present(txn_root, "A", "C", pool));
SVN_ERR(check_id_present(fs, C_id, pool));
SVN_ERR(svn_fs_delete(txn_root, "A/C", pool));
SVN_ERR(check_entry_absent(txn_root, "A", "C", pool));
SVN_ERR(check_id_absent(fs, C_id, pool));
}
{
const svn_fs_id_t *root_id;
SVN_ERR(svn_fs_node_id(&root_id, txn_root, "", pool));
err = svn_fs_delete(txn_root, "", pool);
if (err && (err->apr_err != SVN_ERR_FS_ROOT_DIR)) {
return svn_error_createf
(SVN_ERR_FS_GENERAL, err,
"deleting root directory got wrong error");
} else if (! err) {
return svn_error_createf
(SVN_ERR_FS_GENERAL, NULL,
"deleting root directory failed to get error");
}
svn_error_clear(err);
SVN_ERR(check_id_present(fs, root_id, pool));
}
{
const svn_fs_id_t *iota_id;
SVN_ERR(svn_fs_node_id(&iota_id, txn_root, "iota", pool));
SVN_ERR(check_entry_present(txn_root, "", "iota", pool));
SVN_ERR(check_id_present(fs, iota_id, pool));
SVN_ERR(svn_fs_delete(txn_root, "iota", pool));
SVN_ERR(check_entry_absent(txn_root, "", "iota", pool));
SVN_ERR(check_id_absent(fs, iota_id, pool));
}
return SVN_NO_ERROR;
}
static svn_error_t *
delete(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
svn_fs_t *fs;
svn_fs_txn_t *txn;
svn_fs_root_t *txn_root;
svn_revnum_t new_rev;
*msg = "delete nodes tree";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(svn_test__create_fs(&fs, "test-repo-del-tree",
"bdb", pool));
SVN_ERR(svn_fs_begin_txn(&txn, fs, 0, pool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, pool));
SVN_ERR(svn_test__create_greek_tree(txn_root, pool));
{
const svn_fs_id_t *iota_id, *gamma_id;
static svn_test__tree_entry_t expected_entries[] = {
{ "A", 0 },
{ "A/mu", "This is the file 'mu'.\n" },
{ "A/B", 0 },
{ "A/B/lambda", "This is the file 'lambda'.\n" },
{ "A/B/E", 0 },
{ "A/B/E/alpha", "This is the file 'alpha'.\n" },
{ "A/B/E/beta", "This is the file 'beta'.\n" },
{ "A/C", 0 },
{ "A/B/F", 0 },
{ "A/D", 0 },
{ "A/D/G", 0 },
{ "A/D/G/pi", "This is the file 'pi'.\n" },
{ "A/D/G/rho", "This is the file 'rho'.\n" },
{ "A/D/G/tau", "This is the file 'tau'.\n" },
{ "A/D/H", 0 },
{ "A/D/H/chi", "This is the file 'chi'.\n" },
{ "A/D/H/psi", "This is the file 'psi'.\n" },
{ "A/D/H/omega", "This is the file 'omega'.\n" }
};
SVN_ERR(svn_fs_node_id(&iota_id, txn_root, "iota", pool));
SVN_ERR(svn_fs_node_id(&gamma_id, txn_root, "A/D/gamma", pool));
SVN_ERR(check_entry_present(txn_root, "", "iota", pool));
SVN_ERR(check_id_present(fs, iota_id, pool));
SVN_ERR(check_id_present(fs, gamma_id, pool));
SVN_ERR(svn_fs_delete(txn_root, "iota", pool));
SVN_ERR(svn_fs_delete(txn_root, "A/D/gamma", pool));
SVN_ERR(check_entry_absent(txn_root, "", "iota", pool));
SVN_ERR(check_entry_absent(txn_root, "A/D", "gamma", pool));
SVN_ERR(check_id_absent(fs, iota_id, pool));
SVN_ERR(check_id_absent(fs, gamma_id, pool));
SVN_ERR(svn_test__validate_tree(txn_root, expected_entries, 18, pool));
}
SVN_ERR(svn_fs_abort_txn(txn, pool));
SVN_ERR(svn_fs_begin_txn(&txn, fs, 0, pool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, pool));
SVN_ERR(svn_test__create_greek_tree(txn_root, pool));
{
const svn_fs_id_t *A_id, *mu_id, *B_id, *lambda_id, *E_id, *alpha_id,
*beta_id, *F_id, *C_id, *D_id, *gamma_id, *H_id, *chi_id,
*psi_id, *omega_id, *G_id, *pi_id, *rho_id, *tau_id;
SVN_ERR(svn_fs_node_id(&A_id, txn_root, "/A", pool));
SVN_ERR(check_entry_present(txn_root, "", "A", pool));
SVN_ERR(svn_fs_node_id(&mu_id, txn_root, "/A/mu", pool));
SVN_ERR(check_entry_present(txn_root, "A", "mu", pool));
SVN_ERR(svn_fs_node_id(&B_id, txn_root, "/A/B", pool));
SVN_ERR(check_entry_present(txn_root, "A", "B", pool));
SVN_ERR(svn_fs_node_id(&lambda_id, txn_root, "/A/B/lambda", pool));
SVN_ERR(check_entry_present(txn_root, "A/B", "lambda", pool));
SVN_ERR(svn_fs_node_id(&E_id, txn_root, "/A/B/E", pool));
SVN_ERR(check_entry_present(txn_root, "A/B", "E", pool));
SVN_ERR(svn_fs_node_id(&alpha_id, txn_root, "/A/B/E/alpha", pool));
SVN_ERR(check_entry_present(txn_root, "A/B/E", "alpha", pool));
SVN_ERR(svn_fs_node_id(&beta_id, txn_root, "/A/B/E/beta", pool));
SVN_ERR(check_entry_present(txn_root, "A/B/E", "beta", pool));
SVN_ERR(svn_fs_node_id(&F_id, txn_root, "/A/B/F", pool));
SVN_ERR(check_entry_present(txn_root, "A/B", "F", pool));
SVN_ERR(svn_fs_node_id(&C_id, txn_root, "/A/C", pool));
SVN_ERR(check_entry_present(txn_root, "A", "C", pool));
SVN_ERR(svn_fs_node_id(&D_id, txn_root, "/A/D", pool));
SVN_ERR(check_entry_present(txn_root, "A", "D", pool));
SVN_ERR(svn_fs_node_id(&gamma_id, txn_root, "/A/D/gamma", pool));
SVN_ERR(check_entry_present(txn_root, "A/D", "gamma", pool));
SVN_ERR(svn_fs_node_id(&H_id, txn_root, "/A/D/H", pool));
SVN_ERR(check_entry_present(txn_root, "A/D", "H", pool));
SVN_ERR(svn_fs_node_id(&chi_id, txn_root, "/A/D/H/chi", pool));
SVN_ERR(check_entry_present(txn_root, "A/D/H", "chi", pool));
SVN_ERR(svn_fs_node_id(&psi_id, txn_root, "/A/D/H/psi", pool));
SVN_ERR(check_entry_present(txn_root, "A/D/H", "psi", pool));
SVN_ERR(svn_fs_node_id(&omega_id, txn_root, "/A/D/H/omega", pool));
SVN_ERR(check_entry_present(txn_root, "A/D/H", "omega", pool));
SVN_ERR(svn_fs_node_id(&G_id, txn_root, "/A/D/G", pool));
SVN_ERR(check_entry_present(txn_root, "A/D", "G", pool));
SVN_ERR(svn_fs_node_id(&pi_id, txn_root, "/A/D/G/pi", pool));
SVN_ERR(check_entry_present(txn_root, "A/D/G", "pi", pool));
SVN_ERR(svn_fs_node_id(&rho_id, txn_root, "/A/D/G/rho", pool));
SVN_ERR(check_entry_present(txn_root, "A/D/G", "rho", pool));
SVN_ERR(svn_fs_node_id(&tau_id, txn_root, "/A/D/G/tau", pool));
SVN_ERR(check_entry_present(txn_root, "A/D/G", "tau", pool));
SVN_ERR(svn_fs_delete(txn_root, "A/C", pool));
SVN_ERR(svn_fs_delete(txn_root, "A/B/F", pool));
SVN_ERR(check_entry_absent(txn_root, "A", "C", pool));
SVN_ERR(check_entry_absent(txn_root, "A/B", "F", pool));
SVN_ERR(check_id_absent(fs, C_id, pool));
SVN_ERR(check_id_absent(fs, F_id, pool));
SVN_ERR(svn_fs_delete(txn_root, "A", pool));
SVN_ERR(check_entry_absent(txn_root, "", "A", pool));
SVN_ERR(check_id_absent(fs, A_id, pool));
SVN_ERR(check_id_absent(fs, mu_id, pool));
SVN_ERR(check_id_absent(fs, B_id, pool));
SVN_ERR(check_id_absent(fs, lambda_id, pool));
SVN_ERR(check_id_absent(fs, E_id, pool));
SVN_ERR(check_id_absent(fs, alpha_id, pool));
SVN_ERR(check_id_absent(fs, beta_id, pool));
SVN_ERR(check_id_absent(fs, D_id, pool));
SVN_ERR(check_id_absent(fs, gamma_id, pool));
SVN_ERR(check_id_absent(fs, H_id, pool));
SVN_ERR(check_id_absent(fs, chi_id, pool));
SVN_ERR(check_id_absent(fs, psi_id, pool));
SVN_ERR(check_id_absent(fs, omega_id, pool));
SVN_ERR(check_id_absent(fs, G_id, pool));
SVN_ERR(check_id_absent(fs, pi_id, pool));
SVN_ERR(check_id_absent(fs, rho_id, pool));
SVN_ERR(check_id_absent(fs, tau_id, pool));
{
static svn_test__tree_entry_t expected_entries[] = {
{ "iota", "This is the file 'iota'.\n" }
};
SVN_ERR(svn_test__validate_tree(txn_root, expected_entries, 1, pool));
}
}
SVN_ERR(svn_fs_abort_txn(txn, pool));
SVN_ERR(svn_fs_begin_txn(&txn, fs, 0, pool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, pool));
SVN_ERR(svn_test__create_greek_tree(txn_root, pool));
SVN_ERR(svn_fs_commit_txn(NULL, &new_rev, txn, pool));
SVN_ERR(svn_fs_begin_txn(&txn, fs, new_rev, pool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, pool));
{
const svn_fs_id_t *A_id, *mu_id, *B_id, *lambda_id, *E_id, *alpha_id,
*beta_id, *F_id, *C_id, *D_id, *gamma_id, *H_id, *chi_id,
*psi_id, *omega_id, *G_id, *pi_id, *rho_id, *tau_id, *sigma_id;
SVN_ERR(svn_fs_make_file(txn_root, "A/D/G/sigma", pool));
SVN_ERR(svn_test__set_file_contents(txn_root, "A/D/G/sigma",
"This is another file 'sigma'.\n", pool));
SVN_ERR(svn_fs_node_id(&A_id, txn_root, "/A", pool));
SVN_ERR(check_entry_present(txn_root, "", "A", pool));
SVN_ERR(svn_fs_node_id(&mu_id, txn_root, "/A/mu", pool));
SVN_ERR(check_entry_present(txn_root, "A", "mu", pool));
SVN_ERR(svn_fs_node_id(&B_id, txn_root, "/A/B", pool));
SVN_ERR(check_entry_present(txn_root, "A", "B", pool));
SVN_ERR(svn_fs_node_id(&lambda_id, txn_root, "/A/B/lambda", pool));
SVN_ERR(check_entry_present(txn_root, "A/B", "lambda", pool));
SVN_ERR(svn_fs_node_id(&E_id, txn_root, "/A/B/E", pool));
SVN_ERR(check_entry_present(txn_root, "A/B", "E", pool));
SVN_ERR(svn_fs_node_id(&alpha_id, txn_root, "/A/B/E/alpha", pool));
SVN_ERR(check_entry_present(txn_root, "A/B/E", "alpha", pool));
SVN_ERR(svn_fs_node_id(&beta_id, txn_root, "/A/B/E/beta", pool));
SVN_ERR(check_entry_present(txn_root, "A/B/E", "beta", pool));
SVN_ERR(svn_fs_node_id(&F_id, txn_root, "/A/B/F", pool));
SVN_ERR(check_entry_present(txn_root, "A/B", "F", pool));
SVN_ERR(svn_fs_node_id(&C_id, txn_root, "/A/C", pool));
SVN_ERR(check_entry_present(txn_root, "A", "C", pool));
SVN_ERR(svn_fs_node_id(&D_id, txn_root, "/A/D", pool));
SVN_ERR(check_entry_present(txn_root, "A", "D", pool));
SVN_ERR(svn_fs_node_id(&gamma_id, txn_root, "/A/D/gamma", pool));
SVN_ERR(check_entry_present(txn_root, "A/D", "gamma", pool));
SVN_ERR(svn_fs_node_id(&H_id, txn_root, "/A/D/H", pool));
SVN_ERR(check_entry_present(txn_root, "A/D", "H", pool));
SVN_ERR(svn_fs_node_id(&chi_id, txn_root, "/A/D/H/chi", pool));
SVN_ERR(check_entry_present(txn_root, "A/D/H", "chi", pool));
SVN_ERR(svn_fs_node_id(&psi_id, txn_root, "/A/D/H/psi", pool));
SVN_ERR(check_entry_present(txn_root, "A/D/H", "psi", pool));
SVN_ERR(svn_fs_node_id(&omega_id, txn_root, "/A/D/H/omega", pool));
SVN_ERR(check_entry_present(txn_root, "A/D/H", "omega", pool));
SVN_ERR(svn_fs_node_id(&G_id, txn_root, "/A/D/G", pool));
SVN_ERR(check_entry_present(txn_root, "A/D", "G", pool));
SVN_ERR(svn_fs_node_id(&pi_id, txn_root, "/A/D/G/pi", pool));
SVN_ERR(check_entry_present(txn_root, "A/D/G", "pi", pool));
SVN_ERR(svn_fs_node_id(&rho_id, txn_root, "/A/D/G/rho", pool));
SVN_ERR(check_entry_present(txn_root, "A/D/G", "rho", pool));
SVN_ERR(svn_fs_node_id(&tau_id, txn_root, "/A/D/G/tau", pool));
SVN_ERR(check_entry_present(txn_root, "A/D/G", "tau", pool));
SVN_ERR(svn_fs_node_id(&sigma_id, txn_root, "/A/D/G/sigma", pool));
SVN_ERR(check_entry_present(txn_root, "A/D/G", "sigma", pool));
SVN_ERR(svn_fs_delete(txn_root, "A", pool));
SVN_ERR(check_entry_absent(txn_root, "", "A", pool));
SVN_ERR(check_id_absent(fs, A_id, pool));
SVN_ERR(check_id_present(fs, mu_id, pool));
SVN_ERR(check_id_present(fs, B_id, pool));
SVN_ERR(check_id_present(fs, lambda_id, pool));
SVN_ERR(check_id_present(fs, E_id, pool));
SVN_ERR(check_id_present(fs, alpha_id, pool));
SVN_ERR(check_id_present(fs, beta_id, pool));
SVN_ERR(check_id_present(fs, F_id, pool));
SVN_ERR(check_id_present(fs, C_id, pool));
SVN_ERR(check_id_absent(fs, D_id, pool));
SVN_ERR(check_id_present(fs, gamma_id, pool));
SVN_ERR(check_id_present(fs, H_id, pool));
SVN_ERR(check_id_present(fs, chi_id, pool));
SVN_ERR(check_id_present(fs, psi_id, pool));
SVN_ERR(check_id_present(fs, omega_id, pool));
SVN_ERR(check_id_absent(fs, G_id, pool));
SVN_ERR(check_id_present(fs, pi_id, pool));
SVN_ERR(check_id_present(fs, rho_id, pool));
SVN_ERR(check_id_present(fs, tau_id, pool));
SVN_ERR(check_id_absent(fs, sigma_id, pool));
{
static svn_test__tree_entry_t expected_entries[] = {
{ "iota", "This is the file 'iota'.\n" }
};
SVN_ERR(svn_test__validate_tree(txn_root, expected_entries, 1, pool));
}
}
SVN_ERR(svn_fs_abort_txn(txn, pool));
SVN_ERR(svn_fs_begin_txn(&txn, fs, new_rev, pool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, pool));
{
const svn_fs_id_t *iota_id, *gamma_id;
SVN_ERR(svn_fs_node_id(&iota_id, txn_root, "iota", pool));
SVN_ERR(svn_fs_node_id(&gamma_id, txn_root, "A/D/gamma", pool));
SVN_ERR(check_entry_present(txn_root, "", "iota", pool));
SVN_ERR(check_entry_present(txn_root, "A/D", "gamma", pool));
SVN_ERR(check_id_present(fs, iota_id, pool));
SVN_ERR(check_id_present(fs, gamma_id, pool));
SVN_ERR(svn_fs_delete(txn_root, "iota", pool));
SVN_ERR(svn_fs_delete(txn_root, "A/D/gamma", pool));
SVN_ERR(check_entry_absent(txn_root, "", "iota", pool));
SVN_ERR(check_entry_absent(txn_root, "A/D", "iota", pool));
SVN_ERR(check_id_present(fs, iota_id, pool));
SVN_ERR(check_id_present(fs, gamma_id, pool));
{
static svn_test__tree_entry_t expected_entries[] = {
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
{ "A/D/G", 0 },
{ "A/D/G/pi", "This is the file 'pi'.\n" },
{ "A/D/G/rho", "This is the file 'rho'.\n" },
{ "A/D/G/tau", "This is the file 'tau'.\n" },
{ "A/D/H", 0 },
{ "A/D/H/chi", "This is the file 'chi'.\n" },
{ "A/D/H/psi", "This is the file 'psi'.\n" },
{ "A/D/H/omega", "This is the file 'omega'.\n" }
};
SVN_ERR(svn_test__validate_tree(txn_root, expected_entries, 18, pool));
}
}
SVN_ERR(svn_fs_abort_txn(txn, pool));
SVN_ERR(svn_fs_begin_txn(&txn, fs, new_rev, pool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, pool));
{
const svn_fs_id_t *A_id, *mu_id, *B_id, *lambda_id, *E_id, *alpha_id,
*beta_id, *F_id, *C_id, *D_id, *gamma_id, *H_id, *chi_id,
*psi_id, *omega_id, *G_id, *pi_id, *rho_id, *tau_id;
SVN_ERR(svn_fs_node_id(&A_id, txn_root, "/A", pool));
SVN_ERR(check_entry_present(txn_root, "", "A", pool));
SVN_ERR(svn_fs_node_id(&mu_id, txn_root, "/A/mu", pool));
SVN_ERR(check_entry_present(txn_root, "A", "mu", pool));
SVN_ERR(svn_fs_node_id(&B_id, txn_root, "/A/B", pool));
SVN_ERR(check_entry_present(txn_root, "A", "B", pool));
SVN_ERR(svn_fs_node_id(&lambda_id, txn_root, "/A/B/lambda", pool));
SVN_ERR(check_entry_present(txn_root, "A/B", "lambda", pool));
SVN_ERR(svn_fs_node_id(&E_id, txn_root, "/A/B/E", pool));
SVN_ERR(check_entry_present(txn_root, "A/B", "E", pool));
SVN_ERR(svn_fs_node_id(&alpha_id, txn_root, "/A/B/E/alpha", pool));
SVN_ERR(check_entry_present(txn_root, "A/B/E", "alpha", pool));
SVN_ERR(svn_fs_node_id(&beta_id, txn_root, "/A/B/E/beta", pool));
SVN_ERR(check_entry_present(txn_root, "A/B/E", "beta", pool));
SVN_ERR(svn_fs_node_id(&F_id, txn_root, "/A/B/F", pool));
SVN_ERR(check_entry_present(txn_root, "A/B", "F", pool));
SVN_ERR(svn_fs_node_id(&C_id, txn_root, "/A/C", pool));
SVN_ERR(check_entry_present(txn_root, "A", "C", pool));
SVN_ERR(svn_fs_node_id(&D_id, txn_root, "/A/D", pool));
SVN_ERR(check_entry_present(txn_root, "A", "D", pool));
SVN_ERR(svn_fs_node_id(&gamma_id, txn_root, "/A/D/gamma", pool));
SVN_ERR(check_entry_present(txn_root, "A/D", "gamma", pool));
SVN_ERR(svn_fs_node_id(&H_id, txn_root, "/A/D/H", pool));
SVN_ERR(check_entry_present(txn_root, "A/D", "H", pool));
SVN_ERR(svn_fs_node_id(&chi_id, txn_root, "/A/D/H/chi", pool));
SVN_ERR(check_entry_present(txn_root, "A/D/H", "chi", pool));
SVN_ERR(svn_fs_node_id(&psi_id, txn_root, "/A/D/H/psi", pool));
SVN_ERR(check_entry_present(txn_root, "A/D/H", "psi", pool));
SVN_ERR(svn_fs_node_id(&omega_id, txn_root, "/A/D/H/omega", pool));
SVN_ERR(check_entry_present(txn_root, "A/D/H", "omega", pool));
SVN_ERR(svn_fs_node_id(&G_id, txn_root, "/A/D/G", pool));
SVN_ERR(check_entry_present(txn_root, "A/D", "G", pool));
SVN_ERR(svn_fs_node_id(&pi_id, txn_root, "/A/D/G/pi", pool));
SVN_ERR(check_entry_present(txn_root, "A/D/G", "pi", pool));
SVN_ERR(svn_fs_node_id(&rho_id, txn_root, "/A/D/G/rho", pool));
SVN_ERR(check_entry_present(txn_root, "A/D/G", "rho", pool));
SVN_ERR(svn_fs_node_id(&tau_id, txn_root, "/A/D/G/tau", pool));
SVN_ERR(check_entry_present(txn_root, "A/D/G", "tau", pool));
SVN_ERR(svn_fs_delete(txn_root, "A", pool));
SVN_ERR(check_entry_absent(txn_root, "", "A", pool));
SVN_ERR(check_id_present(fs, A_id, pool));
SVN_ERR(check_id_present(fs, mu_id, pool));
SVN_ERR(check_id_present(fs, B_id, pool));
SVN_ERR(check_id_present(fs, lambda_id, pool));
SVN_ERR(check_id_present(fs, E_id, pool));
SVN_ERR(check_id_present(fs, alpha_id, pool));
SVN_ERR(check_id_present(fs, beta_id, pool));
SVN_ERR(check_id_present(fs, F_id, pool));
SVN_ERR(check_id_present(fs, C_id, pool));
SVN_ERR(check_id_present(fs, D_id, pool));
SVN_ERR(check_id_present(fs, gamma_id, pool));
SVN_ERR(check_id_present(fs, H_id, pool));
SVN_ERR(check_id_present(fs, chi_id, pool));
SVN_ERR(check_id_present(fs, psi_id, pool));
SVN_ERR(check_id_present(fs, omega_id, pool));
SVN_ERR(check_id_present(fs, G_id, pool));
SVN_ERR(check_id_present(fs, pi_id, pool));
SVN_ERR(check_id_present(fs, rho_id, pool));
SVN_ERR(check_id_present(fs, tau_id, pool));
{
static svn_test__tree_entry_t expected_entries[] = {
{ "iota", "This is the file 'iota'.\n" }
};
SVN_ERR(svn_test__validate_tree(txn_root, expected_entries, 1, pool));
}
}
return SVN_NO_ERROR;
}
struct node_created_rev_args {
const char *path;
svn_revnum_t rev;
};
static svn_error_t *
canonicalize_abspath(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
apr_size_t i;
const char *paths[21][2] =
{ { NULL, NULL },
{ "", "/" },
{ "/", "/" },
{ "//", "/" },
{ "///", "/" },
{ "foo", "/foo" },
{ "foo/", "/foo" },
{ "foo//", "/foo" },
{ "/foo", "/foo" },
{ "/foo/", "/foo" },
{ "/foo//", "/foo" },
{ "//foo//", "/foo" },
{ "foo/bar", "/foo/bar" },
{ "foo/bar/", "/foo/bar" },
{ "foo/bar//", "/foo/bar" },
{ "foo//bar", "/foo/bar" },
{ "foo//bar/", "/foo/bar" },
{ "foo//bar//", "/foo/bar" },
{ "/foo//bar//", "/foo/bar" },
{ "//foo//bar//", "/foo/bar" },
{ "///foo///bar///baz///", "/foo/bar/baz" },
};
*msg = "test svn_fs__canonicalize_abspath";
if (msg_only)
return SVN_NO_ERROR;
for (i = 0; i < (sizeof(paths) / 2 / sizeof(const char *)); i++) {
const char *input = paths[i][0];
const char *output = paths[i][1];
const char *actual = svn_fs__canonicalize_abspath(input, pool);
if ((! output) && (! actual))
continue;
if ((! output) && actual)
return svn_error_createf(SVN_ERR_TEST_FAILED, NULL,
"expected NULL path; got '%s'", actual);
if (output && (! actual))
return svn_error_createf(SVN_ERR_TEST_FAILED, NULL,
"expected '%s' path; got NULL", output);
if (strcmp(output, actual))
return svn_error_createf(SVN_ERR_TEST_FAILED, NULL,
"expected '%s' path; got '%s'",
output, actual);
}
return SVN_NO_ERROR;
}
static svn_error_t *
create_within_copy(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
apr_pool_t *spool = svn_pool_create(pool);
svn_fs_t *fs;
svn_fs_txn_t *txn;
svn_fs_root_t *txn_root, *rev_root;
int i;
svn_revnum_t youngest_rev = 0;
*msg = "create new items within a copied directory";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(svn_test__create_fs(&fs, "test-repo-create-within-copy",
"bdb", pool));
SVN_ERR(svn_fs_begin_txn(&txn, fs, youngest_rev, spool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, spool));
SVN_ERR(svn_test__create_greek_tree(txn_root, spool));
SVN_ERR(svn_fs_commit_txn(NULL, &youngest_rev, txn, spool));
svn_pool_clear(spool);
SVN_ERR(svn_fs_begin_txn(&txn, fs, youngest_rev, spool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, spool));
SVN_ERR(svn_fs_revision_root(&rev_root, fs, youngest_rev, spool));
SVN_ERR(svn_fs_copy(rev_root, "A/D", txn_root, "A/D3", spool));
SVN_ERR(svn_fs_commit_txn(NULL, &youngest_rev, txn, spool));
svn_pool_clear(spool);
SVN_ERR(svn_fs_begin_txn(&txn, fs, youngest_rev, spool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, spool));
SVN_ERR(svn_fs_revision_root(&rev_root, fs, youngest_rev, spool));
SVN_ERR(svn_fs_copy(rev_root, "A/D/G", txn_root, "A/D/G2", spool));
SVN_ERR(svn_fs_commit_txn(NULL, &youngest_rev, txn, spool));
svn_pool_clear(spool);
SVN_ERR(svn_fs_begin_txn(&txn, fs, youngest_rev, spool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, spool));
SVN_ERR(svn_fs_revision_root(&rev_root, fs, youngest_rev, spool));
SVN_ERR(svn_fs_copy(rev_root, "A/D", txn_root, "A/D2", spool));
SVN_ERR(svn_fs_make_dir(txn_root, "A/D/G2/I", spool));
SVN_ERR(svn_fs_make_file(txn_root, "A/D/G2/up", spool));
SVN_ERR(svn_fs_make_dir(txn_root, "A/D2/I", spool));
SVN_ERR(svn_fs_make_file(txn_root, "A/D2/up", spool));
SVN_ERR(svn_fs_make_dir(txn_root, "A/D2/G2/I", spool));
SVN_ERR(svn_fs_make_file(txn_root, "A/D2/G2/up", spool));
SVN_ERR(svn_fs_commit_txn(NULL, &youngest_rev, txn, spool));
svn_pool_clear(spool);
SVN_ERR(svn_fs_begin_txn(&txn, fs, youngest_rev, spool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, spool));
SVN_ERR(svn_fs_make_file(txn_root, "A/D3/down", spool));
SVN_ERR(svn_fs_make_dir(txn_root, "A/D3/J", spool));
SVN_ERR(svn_fs_commit_txn(NULL, &youngest_rev, txn, spool));
svn_pool_clear(spool);
{
const char *pathgroup[4][3] = {
{
"A/D/G2",
"A/D/G2/I",
"A/D/G2/up"
},
{
"A/D2",
"A/D2/I",
"A/D2/up"
},
{
"A/D2/G2",
"A/D2/G2/I",
"A/D2/G2/up"
},
{
"A/D3",
"A/D3/down",
"A/D3/J"
}
};
SVN_ERR(svn_fs_revision_root(&rev_root, fs, youngest_rev, spool));
for (i = 0; i < 4; i++) {
const svn_fs_id_t *lead_id;
const char *lead_copy_id;
int j;
SVN_ERR(svn_fs_node_id(&lead_id, rev_root, pathgroup[i][0], spool));
lead_copy_id = svn_fs_base__id_copy_id(lead_id);
for (j = 1; j < 3; j++) {
const svn_fs_id_t *id;
const char *copy_id;
SVN_ERR(svn_fs_node_id(&id, rev_root, pathgroup[i][j], spool));
copy_id = svn_fs_base__id_copy_id(id);
if (strcmp(copy_id, lead_copy_id) != 0)
return svn_error_createf
(SVN_ERR_TEST_FAILED, NULL,
"'%s' id: expected copy_id '%s'; got copy_id '%s'",
pathgroup[i][j], lead_copy_id, copy_id);
}
}
svn_pool_clear(spool);
}
svn_pool_destroy(spool);
return SVN_NO_ERROR;
}
static svn_error_t *
skip_deltas(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
svn_fs_t *fs;
svn_fs_txn_t *txn;
svn_fs_root_t *txn_root, *rev_root;
apr_pool_t *subpool = svn_pool_create(pool);
svn_revnum_t youngest_rev = 0;
const char *one_line = "This is a line in file 'f'.\n";
svn_stringbuf_t *f = svn_stringbuf_create(one_line, pool);
*msg = "test skip deltas";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(svn_test__create_fs(&fs, "test-repo-skip-deltas",
"bdb", pool));
SVN_ERR(svn_fs_begin_txn(&txn, fs, youngest_rev, subpool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, subpool));
SVN_ERR(svn_fs_make_file(txn_root, "f", subpool));
SVN_ERR(svn_test__set_file_contents(txn_root, "f", f->data, subpool));
SVN_ERR(svn_fs_commit_txn(NULL, &youngest_rev, txn, subpool));
SVN_ERR(svn_fs_deltify_revision(fs, youngest_rev, subpool));
svn_pool_clear(subpool);
while (youngest_rev <= 128) {
svn_stringbuf_appendcstr(f, one_line);
SVN_ERR(svn_fs_begin_txn(&txn, fs, youngest_rev, subpool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, subpool));
SVN_ERR(svn_test__set_file_contents(txn_root, "f", f->data, subpool));
SVN_ERR(svn_fs_commit_txn(NULL, &youngest_rev, txn, subpool));
SVN_ERR(svn_fs_deltify_revision(fs, youngest_rev, subpool));
svn_pool_clear(subpool);
}
SVN_ERR(svn_fs_revision_root(&rev_root, fs, 1, pool));
SVN_ERR(svn_test__get_file_contents(rev_root, "f", &f, pool));
if (strcmp(one_line, f->data) != 0)
return svn_error_createf
(SVN_ERR_TEST_FAILED, NULL,
"Wrong contents. Expected:\n '%s'\nGot:\n '%s'\n",
one_line, f->data);
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
struct get_txn_args {
transaction_t **txn;
const char *txn_name;
svn_fs_t *fs;
};
static svn_error_t *
txn_body_get_txn(void *baton, trail_t *trail) {
struct get_txn_args *args = baton;
return svn_fs_bdb__get_txn(args->txn, args->fs, args->txn_name,
trail, trail->pool);
}
static svn_error_t *
redundant_copy(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
svn_fs_t *fs;
svn_fs_txn_t *txn;
const char *txn_name;
transaction_t *transaction;
svn_fs_root_t *txn_root, *rev_root;
const svn_fs_id_t *old_D_id, *new_D_id;
svn_revnum_t youngest_rev = 0;
struct get_txn_args args;
*msg = "ensure no-op for redundant copies";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(svn_test__create_fs(&fs, "test-repo-redundant-copy",
"bdb", pool));
SVN_ERR(svn_fs_begin_txn(&txn, fs, youngest_rev, pool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, pool));
SVN_ERR(svn_test__create_greek_tree(txn_root, pool));
SVN_ERR(svn_fs_commit_txn(NULL, &youngest_rev, txn, pool));
SVN_ERR(svn_fs_begin_txn(&txn, fs, youngest_rev, pool));
SVN_ERR(svn_fs_txn_name(&txn_name, txn, pool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, pool));
SVN_ERR(svn_fs_revision_root(&rev_root, fs, youngest_rev, pool));
SVN_ERR(svn_fs_copy(rev_root, "A", txn_root, "Z", pool));
args.fs = fs;
args.txn_name = txn_name;
args.txn = &transaction;
SVN_ERR(svn_fs_base__retry_txn(fs, txn_body_get_txn, &args, pool));
if (transaction->copies->nelts != 1)
return svn_error_createf(SVN_ERR_TEST_FAILED, NULL,
"Expected 1 copy; got %d",
transaction->copies->nelts);
SVN_ERR(svn_fs_node_id(&old_D_id, txn_root, "A/D", pool));
SVN_ERR(svn_fs_copy(rev_root, "A/D/G", txn_root, "Z/D/G", pool));
SVN_ERR(svn_fs_base__retry_txn(fs, txn_body_get_txn, &args, pool));
if (transaction->copies->nelts != 1)
return svn_error_createf(SVN_ERR_TEST_FAILED, NULL,
"Expected only 1 copy; got %d",
transaction->copies->nelts);
SVN_ERR(svn_fs_node_id(&new_D_id, txn_root, "A/D", pool));
if (! svn_string_compare(svn_fs_unparse_id(old_D_id, pool),
svn_fs_unparse_id(new_D_id, pool)))
return svn_error_create
(SVN_ERR_TEST_FAILED, NULL,
"Expected equivalent node-rev-ids; got differing ones");
return SVN_NO_ERROR;
}
struct svn_test_descriptor_t test_funcs[] = {
SVN_TEST_NULL,
SVN_TEST_PASS(create_berkeley_filesystem),
SVN_TEST_PASS(open_berkeley_filesystem),
SVN_TEST_PASS(delete_mutables),
SVN_TEST_PASS(delete),
SVN_TEST_PASS(abort_txn),
SVN_TEST_PASS(create_within_copy),
SVN_TEST_PASS(canonicalize_abspath),
SVN_TEST_PASS(skip_deltas),
SVN_TEST_PASS(redundant_copy),
SVN_TEST_NULL
};
