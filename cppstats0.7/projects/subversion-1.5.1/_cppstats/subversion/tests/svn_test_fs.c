#include <stdlib.h>
#include <string.h>
#include <apr_pools.h>
#include "svn_pools.h"
#include "svn_error.h"
#include "svn_fs.h"
#include "svn_path.h"
#include "svn_delta.h"
#include "svn_test.h"
#include "svn_test_fs.h"
static void
fs_warning_handler(void *baton, svn_error_t *err) {
svn_handle_warning(stderr, err);
}
svn_error_t *
svn_test__fs_new(svn_fs_t **fs_p, apr_pool_t *pool) {
apr_hash_t *fs_config = apr_hash_make(pool);
apr_hash_set(fs_config, SVN_FS_CONFIG_BDB_TXN_NOSYNC,
APR_HASH_KEY_STRING, "1");
*fs_p = svn_fs_new(fs_config, pool);
if (! *fs_p)
return svn_error_create(SVN_ERR_FS_GENERAL, NULL,
"Couldn't alloc a new fs object.");
svn_fs_set_warning_func(*fs_p, fs_warning_handler, NULL);
return SVN_NO_ERROR;
}
static apr_hash_t *
make_fs_config(const char *fs_type,
apr_pool_t *pool) {
apr_hash_t *fs_config = apr_hash_make(pool);
apr_hash_set(fs_config, SVN_FS_CONFIG_BDB_TXN_NOSYNC,
APR_HASH_KEY_STRING, "1");
apr_hash_set(fs_config, SVN_FS_CONFIG_FS_TYPE,
APR_HASH_KEY_STRING,
fs_type);
return fs_config;
}
svn_error_t *
svn_test__create_fs(svn_fs_t **fs_p,
const char *name,
const char *fs_type,
apr_pool_t *pool) {
apr_finfo_t finfo;
apr_hash_t *fs_config = make_fs_config(fs_type, pool);
if (apr_stat(&finfo, name, APR_FINFO_TYPE, pool) == APR_SUCCESS) {
if (finfo.filetype == APR_DIR)
SVN_ERR(svn_fs_delete_fs(name, pool));
else
return svn_error_createf(SVN_ERR_TEST_FAILED, NULL,
"there is already a file named '%s'", name);
}
SVN_ERR(svn_fs_create(fs_p, name, fs_config, pool));
if (! *fs_p)
return svn_error_create(SVN_ERR_FS_GENERAL, NULL,
"Couldn't alloc a new fs object.");
svn_fs_set_warning_func(*fs_p, fs_warning_handler, NULL);
svn_test_add_dir_cleanup(name);
return SVN_NO_ERROR;
}
svn_error_t *
svn_test__create_repos(svn_repos_t **repos_p,
const char *name,
const char *fs_type,
apr_pool_t *pool) {
apr_finfo_t finfo;
apr_hash_t *fs_config = make_fs_config(fs_type, pool);
if (apr_stat(&finfo, name, APR_FINFO_TYPE, pool) == APR_SUCCESS) {
if (finfo.filetype == APR_DIR)
SVN_ERR(svn_repos_delete(name, pool));
else
return svn_error_createf(SVN_ERR_TEST_FAILED, NULL,
"there is already a file named '%s'", name);
}
SVN_ERR(svn_repos_create(repos_p, name, NULL, NULL, NULL,
fs_config, pool));
svn_test_add_dir_cleanup(name);
return SVN_NO_ERROR;
}
svn_error_t *
svn_test__stream_to_string(svn_stringbuf_t **string,
svn_stream_t *stream,
apr_pool_t *pool) {
char buf[10];
apr_size_t len;
svn_stringbuf_t *str = svn_stringbuf_create("", pool);
do {
len = sizeof(buf);
SVN_ERR(svn_stream_read(stream, buf, &len));
svn_stringbuf_appendbytes(str, buf, len);
} while (len);
*string = str;
return SVN_NO_ERROR;
}
svn_error_t *
svn_test__set_file_contents(svn_fs_root_t *root,
const char *path,
const char *contents,
apr_pool_t *pool) {
svn_txdelta_window_handler_t consumer_func;
void *consumer_baton;
svn_string_t string;
SVN_ERR(svn_fs_apply_textdelta(&consumer_func, &consumer_baton,
root, path, NULL, NULL, pool));
string.data = contents;
string.len = strlen(contents);
SVN_ERR(svn_txdelta_send_string(&string, consumer_func,
consumer_baton, pool));
return SVN_NO_ERROR;
}
svn_error_t *
svn_test__get_file_contents(svn_fs_root_t *root,
const char *path,
svn_stringbuf_t **str,
apr_pool_t *pool) {
svn_stream_t *stream;
SVN_ERR(svn_fs_file_contents(&stream, root, path, pool));
SVN_ERR(svn_test__stream_to_string(str, stream, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
get_dir_entries(apr_hash_t *tree_entries,
svn_fs_root_t *root,
const char *path,
apr_pool_t *pool) {
apr_hash_t *entries;
apr_hash_index_t *hi;
SVN_ERR(svn_fs_dir_entries(&entries, root, path, pool));
for (hi = apr_hash_first(pool, entries); hi; hi = apr_hash_next(hi)) {
void *val;
svn_fs_dirent_t *dirent;
const char *full_path;
apr_hash_this(hi, NULL, NULL, &val);
dirent = val;
full_path = svn_path_join(path, dirent->name, pool);
apr_hash_set(tree_entries, full_path, APR_HASH_KEY_STRING, dirent);
if (dirent->kind == svn_node_dir)
SVN_ERR(get_dir_entries(tree_entries, root, full_path, pool));
}
return SVN_NO_ERROR;
}
static svn_error_t *
validate_tree_entry(svn_fs_root_t *root,
const char *path,
const char *contents,
apr_pool_t *pool) {
svn_stream_t *rstream;
svn_stringbuf_t *rstring;
svn_boolean_t is_dir;
SVN_ERR(svn_fs_is_dir(&is_dir, root, path, pool));
if ((!is_dir && !contents) || (is_dir && contents))
return svn_error_createf
(SVN_ERR_FS_GENERAL, NULL,
"node '%s' in tree was of unexpected node type",
path);
if (! is_dir) {
SVN_ERR(svn_fs_file_contents(&rstream, root, path, pool));
SVN_ERR(svn_test__stream_to_string(&rstring, rstream, pool));
if (! svn_stringbuf_compare(rstring,
svn_stringbuf_create(contents, pool)))
return svn_error_createf
(SVN_ERR_FS_GENERAL, NULL,
"node '%s' in tree had unexpected contents",
path);
}
return SVN_NO_ERROR;
}
svn_error_t *
svn_test__validate_tree(svn_fs_root_t *root,
svn_test__tree_entry_t *entries,
int num_entries,
apr_pool_t *pool) {
apr_hash_t *tree_entries, *expected_entries;
apr_pool_t *subpool = svn_pool_create(pool);
svn_stringbuf_t *extra_entries = NULL;
svn_stringbuf_t *missing_entries = NULL;
svn_stringbuf_t *corrupt_entries = NULL;
apr_hash_index_t *hi;
int i;
expected_entries = apr_hash_make(subpool);
for (i = 0; i < num_entries; i++)
apr_hash_set(expected_entries, entries[i].path,
APR_HASH_KEY_STRING, &(entries[i]));
tree_entries = apr_hash_make(pool);
SVN_ERR(get_dir_entries(tree_entries, root, "", subpool));
for (hi = apr_hash_first(subpool, expected_entries);
hi;
hi = apr_hash_next(hi)) {
const void *key;
apr_ssize_t keylen;
void *val;
svn_test__tree_entry_t *entry;
apr_hash_this(hi, &key, &keylen, &val);
entry = val;
val = apr_hash_get(tree_entries, key, keylen);
if (val) {
svn_error_t *err;
if ((err = validate_tree_entry(root, entry->path,
entry->contents, subpool))) {
if (! corrupt_entries)
corrupt_entries = svn_stringbuf_create("", subpool);
svn_stringbuf_appendcstr(corrupt_entries, " ");
svn_stringbuf_appendbytes(corrupt_entries, (const char *)key,
keylen);
svn_stringbuf_appendcstr(corrupt_entries, "\n");
svn_error_clear(err);
}
apr_hash_set(tree_entries, key, keylen, NULL);
} else {
if (! missing_entries)
missing_entries = svn_stringbuf_create("", subpool);
svn_stringbuf_appendcstr(missing_entries, " ");
svn_stringbuf_appendbytes(missing_entries, (const char *)key,
keylen);
svn_stringbuf_appendcstr(missing_entries, "\n");
}
}
for (hi = apr_hash_first(subpool, tree_entries);
hi;
hi = apr_hash_next(hi)) {
const void *key;
apr_ssize_t keylen;
apr_hash_this(hi, &key, &keylen, NULL);
if (! extra_entries)
extra_entries = svn_stringbuf_create("", subpool);
svn_stringbuf_appendcstr(extra_entries, " ");
svn_stringbuf_appendbytes(extra_entries, (const char *)key, keylen);
svn_stringbuf_appendcstr(extra_entries, "\n");
}
if (missing_entries || extra_entries || corrupt_entries) {
return svn_error_createf
(SVN_ERR_FS_GENERAL, NULL,
"Repository tree does not look as expected.\n"
"Corrupt entries:\n%s"
"Missing entries:\n%s"
"Extra entries:\n%s",
corrupt_entries ? corrupt_entries->data : "",
missing_entries ? missing_entries->data : "",
extra_entries ? extra_entries->data : "");
}
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
svn_error_t *
svn_test__txn_script_exec(svn_fs_root_t *txn_root,
svn_test__txn_script_command_t *script,
int num_edits,
apr_pool_t *pool) {
int i;
for (i = 0; i < num_edits; i++) {
const char *path = script[i].path;
const char *param1 = script[i].param1;
int cmd = script[i].cmd;
svn_boolean_t is_dir = (param1 == 0);
switch (cmd) {
case 'a':
if (is_dir) {
SVN_ERR(svn_fs_make_dir(txn_root, path, pool));
} else {
SVN_ERR(svn_fs_make_file(txn_root, path, pool));
SVN_ERR(svn_test__set_file_contents(txn_root, path,
param1, pool));
}
break;
case 'c': {
svn_revnum_t youngest;
svn_fs_root_t *rev_root;
svn_fs_t *fs = svn_fs_root_fs(txn_root);
SVN_ERR(svn_fs_youngest_rev(&youngest, fs, pool));
SVN_ERR(svn_fs_revision_root(&rev_root, fs, youngest, pool));
SVN_ERR(svn_fs_copy(rev_root, path, txn_root, param1, pool));
}
break;
case 'd':
SVN_ERR(svn_fs_delete(txn_root, path, pool));
break;
case 'e':
if (! is_dir) {
SVN_ERR(svn_test__set_file_contents(txn_root, path,
param1, pool));
}
break;
default:
break;
}
}
return SVN_NO_ERROR;
}
svn_error_t *
svn_test__check_greek_tree(svn_fs_root_t *root,
apr_pool_t *pool) {
svn_stream_t *rstream;
svn_stringbuf_t *rstring;
svn_stringbuf_t *content;
int i;
const char *file_contents[12][2] = {
{ "iota", "This is the file 'iota'.\n" },
{ "A/mu", "This is the file 'mu'.\n" },
{ "A/B/lambda", "This is the file 'lambda'.\n" },
{ "A/B/E/alpha", "This is the file 'alpha'.\n" },
{ "A/B/E/beta", "This is the file 'beta'.\n" },
{ "A/D/gamma", "This is the file 'gamma'.\n" },
{ "A/D/G/pi", "This is the file 'pi'.\n" },
{ "A/D/G/rho", "This is the file 'rho'.\n" },
{ "A/D/G/tau", "This is the file 'tau'.\n" },
{ "A/D/H/chi", "This is the file 'chi'.\n" },
{ "A/D/H/psi", "This is the file 'psi'.\n" },
{ "A/D/H/omega", "This is the file 'omega'.\n" }
};
for (i = 0; i < 12; i++) {
SVN_ERR(svn_fs_file_contents(&rstream, root,
file_contents[i][0], pool));
SVN_ERR(svn_test__stream_to_string(&rstring, rstream, pool));
content = svn_stringbuf_create(file_contents[i][1], pool);
if (! svn_stringbuf_compare(rstring, content))
return svn_error_createf(SVN_ERR_FS_GENERAL, NULL,
"data read != data written in file '%s'.",
file_contents[i][0]);
}
return SVN_NO_ERROR;
}
svn_error_t *
svn_test__create_greek_tree(svn_fs_root_t *txn_root,
apr_pool_t *pool) {
SVN_ERR(svn_fs_make_file(txn_root, "iota", pool));
SVN_ERR(svn_test__set_file_contents
(txn_root, "iota", "This is the file 'iota'.\n", pool));
SVN_ERR(svn_fs_make_dir (txn_root, "A", pool));
SVN_ERR(svn_fs_make_file(txn_root, "A/mu", pool));
SVN_ERR(svn_test__set_file_contents
(txn_root, "A/mu", "This is the file 'mu'.\n", pool));
SVN_ERR(svn_fs_make_dir (txn_root, "A/B", pool));
SVN_ERR(svn_fs_make_file(txn_root, "A/B/lambda", pool));
SVN_ERR(svn_test__set_file_contents
(txn_root, "A/B/lambda", "This is the file 'lambda'.\n", pool));
SVN_ERR(svn_fs_make_dir (txn_root, "A/B/E", pool));
SVN_ERR(svn_fs_make_file(txn_root, "A/B/E/alpha", pool));
SVN_ERR(svn_test__set_file_contents
(txn_root, "A/B/E/alpha", "This is the file 'alpha'.\n", pool));
SVN_ERR(svn_fs_make_file(txn_root, "A/B/E/beta", pool));
SVN_ERR(svn_test__set_file_contents
(txn_root, "A/B/E/beta", "This is the file 'beta'.\n", pool));
SVN_ERR(svn_fs_make_dir (txn_root, "A/B/F", pool));
SVN_ERR(svn_fs_make_dir (txn_root, "A/C", pool));
SVN_ERR(svn_fs_make_dir (txn_root, "A/D", pool));
SVN_ERR(svn_fs_make_file(txn_root, "A/D/gamma", pool));
SVN_ERR(svn_test__set_file_contents
(txn_root, "A/D/gamma", "This is the file 'gamma'.\n", pool));
SVN_ERR(svn_fs_make_dir (txn_root, "A/D/G", pool));
SVN_ERR(svn_fs_make_file(txn_root, "A/D/G/pi", pool));
SVN_ERR(svn_test__set_file_contents
(txn_root, "A/D/G/pi", "This is the file 'pi'.\n", pool));
SVN_ERR(svn_fs_make_file(txn_root, "A/D/G/rho", pool));
SVN_ERR(svn_test__set_file_contents
(txn_root, "A/D/G/rho", "This is the file 'rho'.\n", pool));
SVN_ERR(svn_fs_make_file(txn_root, "A/D/G/tau", pool));
SVN_ERR(svn_test__set_file_contents
(txn_root, "A/D/G/tau", "This is the file 'tau'.\n", pool));
SVN_ERR(svn_fs_make_dir (txn_root, "A/D/H", pool));
SVN_ERR(svn_fs_make_file(txn_root, "A/D/H/chi", pool));
SVN_ERR(svn_test__set_file_contents
(txn_root, "A/D/H/chi", "This is the file 'chi'.\n", pool));
SVN_ERR(svn_fs_make_file(txn_root, "A/D/H/psi", pool));
SVN_ERR(svn_test__set_file_contents
(txn_root, "A/D/H/psi", "This is the file 'psi'.\n", pool));
SVN_ERR(svn_fs_make_file(txn_root, "A/D/H/omega", pool));
SVN_ERR(svn_test__set_file_contents
(txn_root, "A/D/H/omega", "This is the file 'omega'.\n", pool));
return SVN_NO_ERROR;
}