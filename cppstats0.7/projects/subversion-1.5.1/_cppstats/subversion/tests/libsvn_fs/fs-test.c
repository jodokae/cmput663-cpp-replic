#include <stdlib.h>
#include <string.h>
#include <apr_pools.h>
#include <assert.h>
#include "svn_pools.h"
#include "svn_time.h"
#include "svn_string.h"
#include "svn_fs.h"
#include "svn_md5.h"
#include "svn_mergeinfo.h"
#include "../svn_test.h"
#include "../svn_test_fs.h"
#include "../../libsvn_delta/delta.h"
#define SET_STR(ps, s) ((ps)->data = (s), (ps)->len = strlen(s))
static svn_error_t *
test_commit_txn(svn_revnum_t *new_rev,
svn_fs_txn_t *txn,
const char *expected_conflict,
apr_pool_t *pool) {
const char *conflict;
svn_error_t *err;
err = svn_fs_commit_txn(&conflict, new_rev, txn, pool);
if (err && (err->apr_err == SVN_ERR_FS_CONFLICT)) {
svn_error_clear(err);
if (! expected_conflict) {
return svn_error_createf
(SVN_ERR_FS_CONFLICT, NULL,
"commit conflicted at '%s', but no conflict expected",
conflict ? conflict : "(missing conflict info!)");
} else if (conflict == NULL) {
return svn_error_createf
(SVN_ERR_FS_CONFLICT, NULL,
"commit conflicted as expected, "
"but no conflict path was returned ('%s' expected)",
expected_conflict);
} else if ((strcmp(expected_conflict, "") != 0)
&& (strcmp(conflict, expected_conflict) != 0)) {
return svn_error_createf
(SVN_ERR_FS_CONFLICT, NULL,
"commit conflicted at '%s', but expected conflict at '%s')",
conflict, expected_conflict);
}
} else if (err) {
return svn_error_quick_wrap
(err, "commit failed due to something other than a conflict");
} else {
if (expected_conflict) {
return svn_error_createf
(SVN_ERR_FS_GENERAL, NULL,
"commit succeeded that was expected to fail at '%s'",
expected_conflict);
}
}
return SVN_NO_ERROR;
}
static svn_error_t *
trivial_transaction(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
svn_fs_t *fs;
svn_fs_txn_t *txn;
const char *txn_name;
int is_invalid_char[256];
int i;
const char *p;
*msg = "begin a txn, check its name, then close it";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(svn_test__create_fs(&fs, "test-repo-trivial-txn",
opts->fs_type, pool));
SVN_ERR(svn_fs_begin_txn(&txn, fs, 0, pool));
SVN_ERR(svn_fs_txn_name(&txn_name, txn, pool));
if (! txn_name)
return svn_error_create(SVN_ERR_FS_GENERAL, NULL,
"Got a NULL txn name.");
for (i = 0; i < sizeof(is_invalid_char)/sizeof(*is_invalid_char); ++i)
is_invalid_char[i] = 1;
for (i = '0'; i <= '9'; ++i)
is_invalid_char[i] = 0;
for (i = 'a'; i <= 'z'; ++i)
is_invalid_char[i] = 0;
for (i = 'A'; i <= 'Z'; ++i)
is_invalid_char[i] = 0;
for (p = "-."; *p; ++p)
is_invalid_char[(unsigned char) *p] = 0;
for (p = txn_name; *p; ++p) {
if (is_invalid_char[(unsigned char) *p])
return svn_error_createf(SVN_ERR_FS_GENERAL, NULL,
"The txn name '%s' contains an illegal '%c' "
"character", txn_name, *p);
}
return SVN_NO_ERROR;
}
static svn_error_t *
reopen_trivial_transaction(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
svn_fs_t *fs;
svn_fs_txn_t *txn;
const char *txn_name;
apr_pool_t *subpool = svn_pool_create(pool);
*msg = "open an existing transaction by name";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(svn_test__create_fs(&fs, "test-repo-reopen-trivial-txn",
opts->fs_type, pool));
SVN_ERR(svn_fs_begin_txn(&txn, fs, 0, subpool));
SVN_ERR(svn_fs_txn_name(&txn_name, txn, pool));
svn_pool_clear(subpool);
SVN_ERR(svn_fs_open_txn(&txn, fs, txn_name, subpool));
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
static svn_error_t *
create_file_transaction(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
svn_fs_t *fs;
svn_fs_txn_t *txn;
svn_fs_root_t *txn_root;
*msg = "begin a txn, get the txn root, and add a file";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(svn_test__create_fs(&fs, "test-repo-create-file-txn",
opts->fs_type, pool));
SVN_ERR(svn_fs_begin_txn(&txn, fs, 0, pool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, pool));
SVN_ERR(svn_fs_make_file(txn_root, "beer.txt", pool));
return SVN_NO_ERROR;
}
static svn_error_t *
verify_txn_list(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
svn_fs_t *fs;
apr_pool_t *subpool;
svn_fs_txn_t *txn1, *txn2;
const char *name1, *name2;
apr_array_header_t *txn_list;
*msg = "create 2 txns, list them, and verify the list";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(svn_test__create_fs(&fs, "test-repo-verify-txn-list",
opts->fs_type, pool));
subpool = svn_pool_create(pool);
SVN_ERR(svn_fs_begin_txn(&txn1, fs, 0, subpool));
SVN_ERR(svn_fs_txn_name(&name1, txn1, pool));
svn_pool_destroy(subpool);
subpool = svn_pool_create(pool);
SVN_ERR(svn_fs_begin_txn(&txn2, fs, 0, subpool));
SVN_ERR(svn_fs_txn_name(&name2, txn2, pool));
svn_pool_destroy(subpool);
SVN_ERR(svn_fs_list_transactions(&txn_list, fs, pool));
if (txn_list->nelts != 2)
goto all_bad;
if ((! strcmp(name1, APR_ARRAY_IDX(txn_list, 0, const char *)))
&& (! strcmp(name2, APR_ARRAY_IDX(txn_list, 1, const char *))))
goto all_good;
else if ((! strcmp(name2, APR_ARRAY_IDX(txn_list, 0, const char *)))
&& (! strcmp(name1, APR_ARRAY_IDX(txn_list, 1, const char *))))
goto all_good;
all_bad:
return svn_error_create(SVN_ERR_FS_GENERAL, NULL,
"Got a bogus txn list.");
all_good:
return SVN_NO_ERROR;
}
static svn_error_t *
txn_names_are_not_reused_helper1(apr_hash_t **txn_names,
svn_fs_t *fs,
apr_pool_t *pool) {
apr_hash_index_t *hi;
const int N = 10;
int i;
*txn_names = apr_hash_make(pool);
for (i = 0; i < N; ++i) {
svn_fs_txn_t *txn;
const char *name;
SVN_ERR(svn_fs_begin_txn(&txn, fs, 0, pool));
SVN_ERR(svn_fs_txn_name(&name, txn, pool));
if (apr_hash_get(*txn_names, name, APR_HASH_KEY_STRING) != NULL)
return svn_error_createf(SVN_ERR_FS_GENERAL, NULL,
"beginning a new transaction used an "
"existing transaction name '%s'",
name);
apr_hash_set(*txn_names, name, APR_HASH_KEY_STRING, txn);
}
i = 0;
for (hi = apr_hash_first(pool, *txn_names); hi; hi = apr_hash_next(hi)) {
void *val;
apr_hash_this(hi, NULL, NULL, &val);
SVN_ERR(svn_fs_abort_txn((svn_fs_txn_t *)val, pool));
++i;
}
if (i != N)
return svn_error_createf(SVN_ERR_FS_GENERAL, NULL,
"created %d transactions, but only aborted %d",
N, i);
return SVN_NO_ERROR;
}
static svn_error_t *
txn_names_are_not_reused_helper2(apr_hash_t *ht1,
apr_hash_t *ht2,
apr_pool_t *pool) {
apr_hash_index_t *hi;
for (hi = apr_hash_first(pool, ht1); hi; hi = apr_hash_next(hi)) {
const void *key;
const char *key_string;
apr_hash_this(hi, &key, NULL, NULL);
key_string = key;
if (apr_hash_get(ht2, key, APR_HASH_KEY_STRING) != NULL)
return svn_error_createf(SVN_ERR_FS_GENERAL, NULL,
"the transaction name '%s' was reused",
key_string);
}
return SVN_NO_ERROR;
}
static svn_error_t *
txn_names_are_not_reused(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
svn_fs_t *fs;
apr_pool_t *subpool;
apr_hash_t *txn_names1, *txn_names2;
*msg = "check that transaction names are not reused";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(svn_test__create_fs(&fs, "test-repo-txn-names-are-not-reused",
opts->fs_type, pool));
subpool = svn_pool_create(pool);
SVN_ERR(txn_names_are_not_reused_helper1(&txn_names1, fs, subpool));
SVN_ERR(txn_names_are_not_reused_helper1(&txn_names2, fs, subpool));
SVN_ERR(txn_names_are_not_reused_helper2(txn_names1, txn_names2, subpool));
SVN_ERR(txn_names_are_not_reused_helper2(txn_names2, txn_names1, subpool));
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
static svn_error_t *
write_and_read_file(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
svn_fs_t *fs;
svn_fs_txn_t *txn;
svn_fs_root_t *txn_root;
svn_stream_t *rstream;
svn_stringbuf_t *rstring;
svn_stringbuf_t *wstring;
*msg = "write and read a file's contents";
if (msg_only)
return SVN_NO_ERROR;
wstring = svn_stringbuf_create("Wicki wild, wicki wicki wild.", pool);
SVN_ERR(svn_test__create_fs(&fs, "test-repo-read-and-write-file",
opts->fs_type, pool));
SVN_ERR(svn_fs_begin_txn(&txn, fs, 0, pool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, pool));
SVN_ERR(svn_fs_make_file(txn_root, "beer.txt", pool));
SVN_ERR(svn_test__set_file_contents(txn_root, "beer.txt",
wstring->data, pool));
SVN_ERR(svn_fs_file_contents(&rstream, txn_root, "beer.txt", pool));
SVN_ERR(svn_test__stream_to_string(&rstring, rstream, pool));
if (! svn_stringbuf_compare(rstring, wstring))
return svn_error_create(SVN_ERR_FS_GENERAL, NULL,
"data read != data written.");
return SVN_NO_ERROR;
}
static svn_error_t *
create_mini_tree_transaction(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
svn_fs_t *fs;
svn_fs_txn_t *txn;
svn_fs_root_t *txn_root;
*msg = "test basic file and subdirectory creation";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(svn_test__create_fs(&fs, "test-repo-create-mini-tree-txn",
opts->fs_type, pool));
SVN_ERR(svn_fs_begin_txn(&txn, fs, 0, pool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, pool));
SVN_ERR(svn_fs_make_file(txn_root, "wine.txt", pool));
SVN_ERR(svn_fs_make_dir(txn_root, "keg", pool));
SVN_ERR(svn_fs_make_file(txn_root, "keg/beer.txt", pool));
return SVN_NO_ERROR;
}
static svn_error_t *
create_greek_tree_transaction(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
svn_fs_t *fs;
svn_fs_txn_t *txn;
svn_fs_root_t *txn_root;
*msg = "make The Official Subversion Test Tree";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(svn_test__create_fs(&fs, "test-repo-create-greek-tree-txn",
opts->fs_type, pool));
SVN_ERR(svn_fs_begin_txn(&txn, fs, 0, pool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, pool));
SVN_ERR(svn_test__create_greek_tree(txn_root, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
verify_entry(apr_hash_t *entries, const char *key) {
svn_fs_dirent_t *ent = apr_hash_get(entries, key,
APR_HASH_KEY_STRING);
if (ent == NULL)
return svn_error_createf
(SVN_ERR_FS_GENERAL, NULL,
"didn't find dir entry for \"%s\"", key);
if ((ent->name == NULL) && (ent->id == NULL))
return svn_error_createf
(SVN_ERR_FS_GENERAL, NULL,
"dir entry for \"%s\" has null name and null id", key);
if (ent->name == NULL)
return svn_error_createf
(SVN_ERR_FS_GENERAL, NULL,
"dir entry for \"%s\" has null name", key);
if (ent->id == NULL)
return svn_error_createf
(SVN_ERR_FS_GENERAL, NULL,
"dir entry for \"%s\" has null id", key);
if (strcmp(ent->name, key) != 0)
return svn_error_createf
(SVN_ERR_FS_GENERAL, NULL,
"dir entry for \"%s\" contains wrong name (\"%s\")", key, ent->name);
return SVN_NO_ERROR;
}
static svn_error_t *
list_directory(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
svn_fs_t *fs;
svn_fs_txn_t *txn;
svn_fs_root_t *txn_root;
apr_hash_t *entries;
*msg = "fill a directory, then list it";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(svn_test__create_fs(&fs, "test-repo-list-dir",
opts->fs_type, pool));
SVN_ERR(svn_fs_begin_txn(&txn, fs, 0, pool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, pool));
SVN_ERR(svn_fs_make_file(txn_root, "q", pool));
SVN_ERR(svn_fs_make_dir(txn_root, "A", pool));
SVN_ERR(svn_fs_make_file(txn_root, "A/x", pool));
SVN_ERR(svn_fs_make_file(txn_root, "A/y", pool));
SVN_ERR(svn_fs_make_file(txn_root, "A/z", pool));
SVN_ERR(svn_fs_make_dir(txn_root, "B", pool));
SVN_ERR(svn_fs_make_file(txn_root, "B/m", pool));
SVN_ERR(svn_fs_make_file(txn_root, "B/n", pool));
SVN_ERR(svn_fs_make_file(txn_root, "B/o", pool));
SVN_ERR(svn_fs_dir_entries(&entries, txn_root, "A", pool));
if (apr_hash_count(entries) != 3) {
return svn_error_create(SVN_ERR_FS_GENERAL, NULL,
"unexpected number of entries in dir");
} else {
SVN_ERR(verify_entry(entries, "x"));
SVN_ERR(verify_entry(entries, "y"));
SVN_ERR(verify_entry(entries, "z"));
}
return SVN_NO_ERROR;
}
static svn_error_t *
revision_props(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
svn_fs_t *fs;
apr_hash_t *proplist;
svn_string_t *value;
int i;
svn_string_t s1;
const char *initial_props[4][2] = {
{ "color", "red" },
{ "size", "XXL" },
{ "favorite saturday morning cartoon", "looney tunes" },
{ "auto", "Green 1997 Saturn SL1" }
};
const char *final_props[4][2] = {
{ "color", "violet" },
{ "flower", "violet" },
{ "favorite saturday morning cartoon", "looney tunes" },
{ "auto", "Red 2000 Chevrolet Blazer" }
};
*msg = "set and get some revision properties";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(svn_test__create_fs(&fs, "test-repo-rev-props",
opts->fs_type, pool));
for (i = 0; i < 4; i++) {
SET_STR(&s1, initial_props[i][1]);
SVN_ERR(svn_fs_change_rev_prop(fs, 0, initial_props[i][0], &s1, pool));
}
SET_STR(&s1, "violet");
SVN_ERR(svn_fs_change_rev_prop(fs, 0, "color", &s1, pool));
SET_STR(&s1, "Red 2000 Chevrolet Blazer");
SVN_ERR(svn_fs_change_rev_prop(fs, 0, "auto", &s1, pool));
SVN_ERR(svn_fs_change_rev_prop(fs, 0, "size", NULL, pool));
SVN_ERR(svn_fs_revision_prop(&value, fs, 0, "color", pool));
s1.data = value->data;
s1.len = value->len;
SVN_ERR(svn_fs_change_rev_prop(fs, 0, "flower", &s1, pool));
SVN_ERR(svn_fs_revision_proplist(&proplist, fs, 0, pool));
{
svn_string_t *prop_value;
if (apr_hash_count(proplist) < 4 )
return svn_error_createf
(SVN_ERR_FS_GENERAL, NULL,
"too few revision properties found");
for (i = 0; i < 4; i++) {
prop_value = apr_hash_get(proplist,
final_props[i][0],
APR_HASH_KEY_STRING);
if (! prop_value)
return svn_error_createf
(SVN_ERR_FS_GENERAL, NULL,
"unable to find expected revision property");
if (strcmp(prop_value->data, final_props[i][1]))
return svn_error_createf
(SVN_ERR_FS_GENERAL, NULL,
"revision property had an unexpected value");
}
}
return SVN_NO_ERROR;
}
static svn_error_t *
transaction_props(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
svn_fs_t *fs;
svn_fs_txn_t *txn;
apr_hash_t *proplist;
svn_string_t *value;
svn_revnum_t after_rev;
int i;
svn_string_t s1;
const char *initial_props[4][2] = {
{ "color", "red" },
{ "size", "XXL" },
{ "favorite saturday morning cartoon", "looney tunes" },
{ "auto", "Green 1997 Saturn SL1" }
};
const char *final_props[5][2] = {
{ "color", "violet" },
{ "flower", "violet" },
{ "favorite saturday morning cartoon", "looney tunes" },
{ "auto", "Red 2000 Chevrolet Blazer" },
{ SVN_PROP_REVISION_DATE, "<some datestamp value>" }
};
*msg = "set/get txn props, commit, validate new rev props";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(svn_test__create_fs(&fs, "test-repo-txn-props",
opts->fs_type, pool));
SVN_ERR(svn_fs_begin_txn(&txn, fs, 0, pool));
for (i = 0; i < 4; i++) {
SET_STR(&s1, initial_props[i][1]);
SVN_ERR(svn_fs_change_txn_prop(txn, initial_props[i][0], &s1, pool));
}
SET_STR(&s1, "violet");
SVN_ERR(svn_fs_change_txn_prop(txn, "color", &s1, pool));
SET_STR(&s1, "Red 2000 Chevrolet Blazer");
SVN_ERR(svn_fs_change_txn_prop(txn, "auto", &s1, pool));
SVN_ERR(svn_fs_change_txn_prop(txn, "size", NULL, pool));
SVN_ERR(svn_fs_txn_prop(&value, txn, "color", pool));
s1.data = value->data;
s1.len = value->len;
SVN_ERR(svn_fs_change_txn_prop(txn, "flower", &s1, pool));
SVN_ERR(svn_fs_txn_proplist(&proplist, txn, pool));
{
svn_string_t *prop_value;
if (apr_hash_count(proplist) != 5 )
return svn_error_createf
(SVN_ERR_FS_GENERAL, NULL,
"unexpected number of transaction properties were found");
for (i = 0; i < 5; i++) {
prop_value = apr_hash_get(proplist,
final_props[i][0],
APR_HASH_KEY_STRING);
if (! prop_value)
return svn_error_createf
(SVN_ERR_FS_GENERAL, NULL,
"unable to find expected transaction property");
if (strcmp(final_props[i][0], SVN_PROP_REVISION_DATE))
if (strcmp(prop_value->data, final_props[i][1]))
return svn_error_createf
(SVN_ERR_FS_GENERAL, NULL,
"transaction property had an unexpected value");
}
}
SVN_ERR(test_commit_txn(&after_rev, txn, NULL, pool));
if (after_rev != 1)
return svn_error_createf
(SVN_ERR_FS_GENERAL, NULL,
"committed transaction got wrong revision number");
SVN_ERR(svn_fs_revision_proplist(&proplist, fs, after_rev, pool));
{
svn_string_t *prop_value;
if (apr_hash_count(proplist) < 5 )
return svn_error_createf
(SVN_ERR_FS_GENERAL, NULL,
"unexpected number of revision properties were found");
for (i = 0; i < 5; i++) {
prop_value = apr_hash_get(proplist,
final_props[i][0],
APR_HASH_KEY_STRING);
if (! prop_value)
return svn_error_createf
(SVN_ERR_FS_GENERAL, NULL,
"unable to find expected revision property");
if (strcmp(final_props[i][0], SVN_PROP_REVISION_DATE))
if (strcmp(prop_value->data, final_props[i][1]))
return svn_error_createf
(SVN_ERR_FS_GENERAL, NULL,
"revision property had an unexpected value");
}
}
return SVN_NO_ERROR;
}
static svn_error_t *
node_props(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
svn_fs_t *fs;
svn_fs_txn_t *txn;
svn_fs_root_t *txn_root;
apr_hash_t *proplist;
svn_string_t *value;
int i;
svn_string_t s1;
const char *initial_props[4][2] = {
{ "Best Rock Artist", "Creed" },
{ "Best Rap Artist", "Eminem" },
{ "Best Country Artist", "(null)" },
{ "Best Sound Designer", "Pluessman" }
};
const char *final_props[4][2] = {
{ "Best Rock Artist", "P.O.D." },
{ "Best Rap Artist", "Busta Rhymes" },
{ "Best Sound Designer", "Pluessman" },
{ "Biggest Cakewalk Fanatic", "Pluessman" }
};
*msg = "set and get some node properties";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(svn_test__create_fs(&fs, "test-repo-node-props",
opts->fs_type, pool));
SVN_ERR(svn_fs_begin_txn(&txn, fs, 0, pool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, pool));
SVN_ERR(svn_fs_make_file(txn_root, "music.txt", pool));
for (i = 0; i < 4; i++) {
SET_STR(&s1, initial_props[i][1]);
SVN_ERR(svn_fs_change_node_prop
(txn_root, "music.txt", initial_props[i][0], &s1, pool));
}
SET_STR(&s1, "P.O.D.");
SVN_ERR(svn_fs_change_node_prop(txn_root, "music.txt", "Best Rock Artist",
&s1, pool));
SET_STR(&s1, "Busta Rhymes");
SVN_ERR(svn_fs_change_node_prop(txn_root, "music.txt", "Best Rap Artist",
&s1, pool));
SVN_ERR(svn_fs_change_node_prop(txn_root, "music.txt",
"Best Country Artist", NULL, pool));
SVN_ERR(svn_fs_node_prop(&value, txn_root, "music.txt",
"Best Sound Designer", pool));
s1.data = value->data;
s1.len = value->len;
SVN_ERR(svn_fs_change_node_prop(txn_root, "music.txt",
"Biggest Cakewalk Fanatic", &s1, pool));
SVN_ERR(svn_fs_node_proplist(&proplist, txn_root, "music.txt", pool));
{
svn_string_t *prop_value;
if (apr_hash_count(proplist) != 4 )
return svn_error_createf
(SVN_ERR_FS_GENERAL, NULL,
"unexpected number of node properties were found");
for (i = 0; i < 4; i++) {
prop_value = apr_hash_get(proplist,
final_props[i][0],
APR_HASH_KEY_STRING);
if (! prop_value)
return svn_error_createf
(SVN_ERR_FS_GENERAL, NULL,
"unable to find expected node property");
if (strcmp(prop_value->data, final_props[i][1]))
return svn_error_createf
(SVN_ERR_FS_GENERAL, NULL,
"node property had an unexpected value");
}
}
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
static svn_error_t *
fetch_youngest_rev(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
svn_fs_t *fs;
svn_fs_txn_t *txn;
svn_fs_root_t *txn_root;
svn_revnum_t new_rev;
svn_revnum_t youngest_rev, new_youngest_rev;
*msg = "fetch the youngest revision from a filesystem";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(svn_test__create_fs(&fs, "test-repo-youngest-rev",
opts->fs_type, pool));
SVN_ERR(svn_fs_youngest_rev(&youngest_rev, fs, pool));
SVN_ERR(svn_fs_begin_txn(&txn, fs, 0, pool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, pool));
SVN_ERR(svn_test__create_greek_tree(txn_root, pool));
SVN_ERR(test_commit_txn(&new_rev, txn, NULL, pool));
SVN_ERR(svn_fs_youngest_rev(&new_youngest_rev, fs, pool));
if (youngest_rev == new_rev)
return svn_error_create(SVN_ERR_FS_GENERAL, NULL,
"commit didn't bump up revision number");
if (new_youngest_rev != new_rev)
return svn_error_create(SVN_ERR_FS_GENERAL, NULL,
"couldn't fetch youngest revision");
return SVN_NO_ERROR;
}
static svn_error_t *
basic_commit(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
svn_fs_t *fs;
svn_fs_txn_t *txn;
svn_fs_root_t *txn_root, *revision_root;
svn_revnum_t before_rev, after_rev;
const char *conflict;
*msg = "basic commit";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(svn_test__create_fs(&fs, "test-repo-basic-commit",
opts->fs_type, pool));
SVN_ERR(svn_fs_youngest_rev(&before_rev, fs, pool));
SVN_ERR(svn_fs_begin_txn(&txn, fs, 0, pool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, pool));
SVN_ERR(svn_fs_youngest_rev(&after_rev, fs, pool));
if (after_rev != before_rev)
return svn_error_create
(SVN_ERR_FS_GENERAL, NULL,
"youngest revision changed unexpectedly");
SVN_ERR(svn_test__create_greek_tree(txn_root, pool));
SVN_ERR(svn_fs_commit_txn(&conflict, &after_rev, txn, pool));
if (after_rev == before_rev)
return svn_error_create
(SVN_ERR_FS_GENERAL, NULL,
"youngest revision failed to change");
SVN_ERR(svn_fs_revision_root(&revision_root, fs, after_rev, pool));
SVN_ERR(svn_test__check_greek_tree(revision_root, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
test_tree_node_validation(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
svn_fs_t *fs;
svn_fs_txn_t *txn;
svn_fs_root_t *txn_root, *revision_root;
svn_revnum_t after_rev;
const char *conflict;
apr_pool_t *subpool;
*msg = "testing tree validation helper";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(svn_test__create_fs(&fs, "test-repo-validate-tree-entries",
opts->fs_type, pool));
subpool = svn_pool_create(pool);
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
SVN_ERR(svn_fs_begin_txn(&txn, fs, 0, subpool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, subpool));
SVN_ERR(svn_test__create_greek_tree(txn_root, subpool));
SVN_ERR(svn_test__validate_tree(txn_root, expected_entries, 20,
subpool));
SVN_ERR(svn_fs_commit_txn(&conflict, &after_rev, txn, subpool));
SVN_ERR(svn_fs_revision_root(&revision_root, fs, after_rev, subpool));
SVN_ERR(svn_test__validate_tree(revision_root, expected_entries, 20,
subpool));
}
svn_pool_destroy(subpool);
subpool = svn_pool_create(pool);
{
static svn_test__tree_entry_t expected_entries[] = {
{ "iota", "This is a new version of 'iota'.\n" },
{ "A", 0 },
{ "A/B", 0 },
{ "A/B/lambda", "This is the file 'lambda'.\n" },
{ "A/B/E", 0 },
{ "A/B/E/alpha", "This is the file 'alpha'.\n" },
{ "A/B/E/beta", "This is the file 'beta'.\n" },
{ "A/B/F", 0 },
{ "A/C", 0 },
{ "A/C/kappa", "This is the file 'kappa'.\n" },
{ "A/D", 0 },
{ "A/D/gamma", "This is the file 'gamma'.\n" },
{ "A/D/H", 0 },
{ "A/D/H/chi", "This is the file 'chi'.\n" },
{ "A/D/H/psi", "This is the file 'psi'.\n" },
{ "A/D/H/omega", "This is the file 'omega'.\n" },
{ "A/D/I", 0 },
{ "A/D/I/delta", "This is the file 'delta'.\n" },
{ "A/D/I/epsilon", "This is the file 'epsilon'.\n" }
};
SVN_ERR(svn_fs_begin_txn(&txn, fs, after_rev, subpool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, subpool));
SVN_ERR(svn_test__set_file_contents
(txn_root, "iota", "This is a new version of 'iota'.\n",
subpool));
SVN_ERR(svn_fs_delete(txn_root, "A/mu", subpool));
SVN_ERR(svn_fs_delete(txn_root, "A/D/G", subpool));
SVN_ERR(svn_fs_make_dir(txn_root, "A/D/I", subpool));
SVN_ERR(svn_fs_make_file(txn_root, "A/D/I/delta", subpool));
SVN_ERR(svn_test__set_file_contents
(txn_root, "A/D/I/delta", "This is the file 'delta'.\n",
subpool));
SVN_ERR(svn_fs_make_file(txn_root, "A/D/I/epsilon", subpool));
SVN_ERR(svn_test__set_file_contents
(txn_root, "A/D/I/epsilon", "This is the file 'epsilon'.\n",
subpool));
SVN_ERR(svn_fs_make_file(txn_root, "A/C/kappa", subpool));
SVN_ERR(svn_test__set_file_contents
(txn_root, "A/C/kappa", "This is the file 'kappa'.\n",
subpool));
SVN_ERR(svn_test__validate_tree(txn_root, expected_entries, 19,
subpool));
SVN_ERR(svn_fs_commit_txn(&conflict, &after_rev, txn, subpool));
SVN_ERR(svn_fs_revision_root(&revision_root, fs, after_rev, subpool));
SVN_ERR(svn_test__validate_tree(revision_root, expected_entries,
19, subpool));
}
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
static svn_error_t *
merging_commit(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
svn_fs_t *fs;
svn_fs_txn_t *txn;
svn_fs_root_t *txn_root, *revision_root;
svn_revnum_t after_rev;
svn_revnum_t revisions[24];
apr_size_t i;
svn_revnum_t revision_count;
*msg = "merging commit";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(svn_test__create_fs(&fs, "test-repo-merging-commit",
opts->fs_type, pool));
for (i = 0;
i < ((sizeof(revisions)) / (sizeof(svn_revnum_t)));
i++)
revisions[i] = SVN_INVALID_REVNUM;
revision_count = 0;
revisions[revision_count++] = 0;
SVN_ERR(svn_fs_begin_txn(&txn, fs, 0, pool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, pool));
SVN_ERR(svn_test__create_greek_tree(txn_root, pool));
SVN_ERR(test_commit_txn(&after_rev, txn, NULL, pool));
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
SVN_ERR(svn_fs_revision_root(&revision_root, fs, after_rev, pool));
SVN_ERR(svn_test__validate_tree(revision_root, expected_entries,
20, pool));
}
revisions[revision_count++] = after_rev;
SVN_ERR(svn_fs_begin_txn(&txn, fs, revisions[revision_count-1], pool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, pool));
SVN_ERR(svn_fs_make_dir(txn_root, "A/D/I", pool));
SVN_ERR(svn_fs_make_file(txn_root, "A/D/I/delta", pool));
SVN_ERR(svn_test__set_file_contents
(txn_root, "A/D/I/delta", "This is the file 'delta'.\n", pool));
SVN_ERR(svn_fs_make_file(txn_root, "A/D/I/epsilon", pool));
SVN_ERR(svn_test__set_file_contents
(txn_root, "A/D/I/epsilon", "This is the file 'epsilon'.\n", pool));
SVN_ERR(svn_fs_make_file(txn_root, "A/C/kappa", pool));
SVN_ERR(svn_test__set_file_contents
(txn_root, "A/C/kappa", "This is the file 'kappa'.\n", pool));
SVN_ERR(svn_fs_delete(txn_root, "iota", pool));
SVN_ERR(test_commit_txn(&after_rev, txn, NULL, pool));
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
{ "A/C/kappa", "This is the file 'kappa'.\n" },
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
{ "A/D/I", 0 },
{ "A/D/I/delta", "This is the file 'delta'.\n" },
{ "A/D/I/epsilon", "This is the file 'epsilon'.\n" }
};
SVN_ERR(svn_fs_revision_root(&revision_root, fs, after_rev, pool));
SVN_ERR(svn_test__validate_tree(revision_root, expected_entries,
23, pool));
}
revisions[revision_count++] = after_rev;
SVN_ERR(svn_fs_begin_txn(&txn, fs, revisions[revision_count-1], pool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, pool));
SVN_ERR(svn_fs_delete(txn_root, "A/D/H", pool));
SVN_ERR(svn_fs_make_file(txn_root, "iota", pool));
SVN_ERR(svn_test__set_file_contents
(txn_root, "iota", "This is the new file 'iota'.\n", pool));
SVN_ERR(test_commit_txn(&after_rev, txn, NULL, pool));
{
static svn_test__tree_entry_t expected_entries[] = {
{ "iota", "This is the new file 'iota'.\n" },
{ "A", 0 },
{ "A/mu", "This is the file 'mu'.\n" },
{ "A/B", 0 },
{ "A/B/lambda", "This is the file 'lambda'.\n" },
{ "A/B/E", 0 },
{ "A/B/E/alpha", "This is the file 'alpha'.\n" },
{ "A/B/E/beta", "This is the file 'beta'.\n" },
{ "A/B/F", 0 },
{ "A/C", 0 },
{ "A/C/kappa", "This is the file 'kappa'.\n" },
{ "A/D", 0 },
{ "A/D/gamma", "This is the file 'gamma'.\n" },
{ "A/D/G", 0 },
{ "A/D/G/pi", "This is the file 'pi'.\n" },
{ "A/D/G/rho", "This is the file 'rho'.\n" },
{ "A/D/G/tau", "This is the file 'tau'.\n" },
{ "A/D/I", 0 },
{ "A/D/I/delta", "This is the file 'delta'.\n" },
{ "A/D/I/epsilon", "This is the file 'epsilon'.\n" }
};
SVN_ERR(svn_fs_revision_root(&revision_root, fs, after_rev, pool));
SVN_ERR(svn_test__validate_tree(revision_root, expected_entries,
20, pool));
}
revisions[revision_count++] = after_rev;
SVN_ERR(svn_fs_begin_txn(&txn, fs, revisions[revision_count-1], pool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, pool));
SVN_ERR(svn_fs_delete(txn_root, "iota", pool));
SVN_ERR(test_commit_txn(&after_rev, txn, NULL, pool));
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
{ "A/C/kappa", "This is the file 'kappa'.\n" },
{ "A/D", 0 },
{ "A/D/gamma", "This is the file 'gamma'.\n" },
{ "A/D/G", 0 },
{ "A/D/G/pi", "This is the file 'pi'.\n" },
{ "A/D/G/rho", "This is the file 'rho'.\n" },
{ "A/D/G/tau", "This is the file 'tau'.\n" },
{ "A/D/I", 0 },
{ "A/D/I/delta", "This is the file 'delta'.\n" },
{ "A/D/I/epsilon", "This is the file 'epsilon'.\n" }
};
SVN_ERR(svn_fs_revision_root(&revision_root, fs, after_rev, pool));
SVN_ERR(svn_test__validate_tree(revision_root, expected_entries,
19, pool));
}
revisions[revision_count++] = after_rev;
{
SVN_ERR(svn_fs_begin_txn(&txn, fs, revisions[0], pool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, pool));
SVN_ERR(svn_fs_make_file(txn_root, "theta", pool));
SVN_ERR(svn_test__set_file_contents
(txn_root, "theta", "This is the file 'theta'.\n", pool));
SVN_ERR(test_commit_txn(&after_rev, txn, NULL, pool));
{
static svn_test__tree_entry_t expected_entries[] = {
{ "theta", "This is the file 'theta'.\n" },
{ "A", 0 },
{ "A/mu", "This is the file 'mu'.\n" },
{ "A/B", 0 },
{ "A/B/lambda", "This is the file 'lambda'.\n" },
{ "A/B/E", 0 },
{ "A/B/E/alpha", "This is the file 'alpha'.\n" },
{ "A/B/E/beta", "This is the file 'beta'.\n" },
{ "A/B/F", 0 },
{ "A/C", 0 },
{ "A/C/kappa", "This is the file 'kappa'.\n" },
{ "A/D", 0 },
{ "A/D/gamma", "This is the file 'gamma'.\n" },
{ "A/D/G", 0 },
{ "A/D/G/pi", "This is the file 'pi'.\n" },
{ "A/D/G/rho", "This is the file 'rho'.\n" },
{ "A/D/G/tau", "This is the file 'tau'.\n" },
{ "A/D/I", 0 },
{ "A/D/I/delta", "This is the file 'delta'.\n" },
{ "A/D/I/epsilon", "This is the file 'epsilon'.\n" }
};
SVN_ERR(svn_fs_revision_root(&revision_root, fs, after_rev, pool));
SVN_ERR(svn_test__validate_tree(revision_root,
expected_entries,
20, pool));
}
revisions[revision_count++] = after_rev;
}
{
SVN_ERR(svn_fs_begin_txn(&txn, fs, revisions[4], pool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, pool));
SVN_ERR(svn_fs_make_file(txn_root, "theta", pool));
SVN_ERR(svn_test__set_file_contents
(txn_root, "theta", "This is another file 'theta'.\n", pool));
SVN_ERR(test_commit_txn(&after_rev, txn, "/theta", pool));
SVN_ERR(svn_fs_abort_txn(txn, pool));
}
{
SVN_ERR(svn_fs_begin_txn(&txn, fs, revisions[1], pool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, pool));
SVN_ERR(svn_fs_delete(txn_root, "A/D/H", pool));
SVN_ERR(test_commit_txn(&after_rev, txn, NULL, pool));
{
static svn_test__tree_entry_t expected_entries[] = {
{ "theta", "This is the file 'theta'.\n" },
{ "A", 0 },
{ "A/mu", "This is the file 'mu'.\n" },
{ "A/B", 0 },
{ "A/B/lambda", "This is the file 'lambda'.\n" },
{ "A/B/E", 0 },
{ "A/B/E/alpha", "This is the file 'alpha'.\n" },
{ "A/B/E/beta", "This is the file 'beta'.\n" },
{ "A/B/F", 0 },
{ "A/C", 0 },
{ "A/C/kappa", "This is the file 'kappa'.\n" },
{ "A/D", 0 },
{ "A/D/gamma", "This is the file 'gamma'.\n" },
{ "A/D/G", 0 },
{ "A/D/G/pi", "This is the file 'pi'.\n" },
{ "A/D/G/rho", "This is the file 'rho'.\n" },
{ "A/D/G/tau", "This is the file 'tau'.\n" },
{ "A/D/I", 0 },
{ "A/D/I/delta", "This is the file 'delta'.\n" },
{ "A/D/I/epsilon", "This is the file 'epsilon'.\n" }
};
SVN_ERR(svn_fs_revision_root(&revision_root, fs, after_rev, pool));
SVN_ERR(svn_test__validate_tree(revision_root,
expected_entries,
20, pool));
}
revisions[revision_count++] = after_rev;
SVN_ERR(svn_fs_begin_txn(&txn, fs, revisions[1], pool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, pool));
SVN_ERR(svn_fs_delete(txn_root, "A/D/H/omega", pool));
SVN_ERR(test_commit_txn(&after_rev, txn, "/A/D/H", pool));
SVN_ERR(svn_fs_abort_txn(txn, pool));
{
SVN_ERR(svn_fs_begin_txn(&txn, fs, revisions[1], pool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, pool));
SVN_ERR(svn_fs_delete(txn_root, "A/D/H", pool));
SVN_ERR(svn_fs_make_dir(txn_root, "A/D/H", pool));
SVN_ERR(test_commit_txn(&after_rev, txn, NULL, pool));
revisions[revision_count++] = after_rev;
{
SVN_ERR(svn_fs_begin_txn
(&txn, fs, revisions[revision_count - 1], pool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, pool));
SVN_ERR(svn_fs_delete(txn_root, "A/D/H", pool));
SVN_ERR(test_commit_txn(&after_rev, txn, NULL, pool));
revisions[revision_count++] = after_rev;
}
SVN_ERR(svn_fs_begin_txn(&txn, fs, revisions[1], pool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, pool));
SVN_ERR(svn_fs_make_file(txn_root, "A/D/H/zeta", pool));
SVN_ERR(test_commit_txn(&after_rev, txn, "/A/D/H", pool));
SVN_ERR(svn_fs_abort_txn(txn, pool));
SVN_ERR(svn_fs_begin_txn(&txn, fs, revisions[1], pool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, pool));
SVN_ERR(svn_fs_delete(txn_root, "A/mu", pool));
SVN_ERR(test_commit_txn(&after_rev, txn, NULL, pool));
{
static svn_test__tree_entry_t expected_entries[] = {
{ "theta", "This is the file 'theta'.\n" },
{ "A", 0 },
{ "A/B", 0 },
{ "A/B/lambda", "This is the file 'lambda'.\n" },
{ "A/B/E", 0 },
{ "A/B/E/alpha", "This is the file 'alpha'.\n" },
{ "A/B/E/beta", "This is the file 'beta'.\n" },
{ "A/B/F", 0 },
{ "A/C", 0 },
{ "A/C/kappa", "This is the file 'kappa'.\n" },
{ "A/D", 0 },
{ "A/D/gamma", "This is the file 'gamma'.\n" },
{ "A/D/G", 0 },
{ "A/D/G/pi", "This is the file 'pi'.\n" },
{ "A/D/G/rho", "This is the file 'rho'.\n" },
{ "A/D/G/tau", "This is the file 'tau'.\n" },
{ "A/D/I", 0 },
{ "A/D/I/delta", "This is the file 'delta'.\n" },
{ "A/D/I/epsilon", "This is the file 'epsilon'.\n" }
};
SVN_ERR(svn_fs_revision_root(&revision_root, fs, after_rev, pool));
SVN_ERR(svn_test__validate_tree(revision_root,
expected_entries,
19, pool));
}
revisions[revision_count++] = after_rev;
}
}
SVN_ERR(svn_fs_begin_txn(&txn, fs, revisions[revision_count-1], pool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, pool));
SVN_ERR(svn_fs_make_file(txn_root, "A/mu", pool));
SVN_ERR(svn_test__set_file_contents
(txn_root, "A/mu", "A new file 'mu'.\n", pool));
SVN_ERR(svn_fs_make_file(txn_root, "A/D/G/xi", pool));
SVN_ERR(svn_test__set_file_contents
(txn_root, "A/D/G/xi", "This is the file 'xi'.\n", pool));
SVN_ERR(test_commit_txn(&after_rev, txn, NULL, pool));
{
static svn_test__tree_entry_t expected_entries[] = {
{ "theta", "This is the file 'theta'.\n" },
{ "A", 0 },
{ "A/mu", "A new file 'mu'.\n" },
{ "A/B", 0 },
{ "A/B/lambda", "This is the file 'lambda'.\n" },
{ "A/B/E", 0 },
{ "A/B/E/alpha", "This is the file 'alpha'.\n" },
{ "A/B/E/beta", "This is the file 'beta'.\n" },
{ "A/B/F", 0 },
{ "A/C", 0 },
{ "A/C/kappa", "This is the file 'kappa'.\n" },
{ "A/D", 0 },
{ "A/D/gamma", "This is the file 'gamma'.\n" },
{ "A/D/G", 0 },
{ "A/D/G/pi", "This is the file 'pi'.\n" },
{ "A/D/G/rho", "This is the file 'rho'.\n" },
{ "A/D/G/tau", "This is the file 'tau'.\n" },
{ "A/D/G/xi", "This is the file 'xi'.\n" },
{ "A/D/I", 0 },
{ "A/D/I/delta", "This is the file 'delta'.\n" },
{ "A/D/I/epsilon", "This is the file 'epsilon'.\n" }
};
SVN_ERR(svn_fs_revision_root(&revision_root, fs, after_rev, pool));
SVN_ERR(svn_test__validate_tree(revision_root, expected_entries,
21, pool));
}
revisions[revision_count++] = after_rev;
{
SVN_ERR(svn_fs_begin_txn(&txn, fs, revisions[1], pool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, pool));
SVN_ERR(svn_fs_delete(txn_root, "A/mu", pool));
SVN_ERR(svn_fs_make_file(txn_root, "A/mu", pool));
SVN_ERR(svn_test__set_file_contents
(txn_root, "A/mu", "This is the file 'mu'.\n", pool));
SVN_ERR(test_commit_txn(&after_rev, txn, "/A/mu", pool));
SVN_ERR(svn_fs_abort_txn(txn, pool));
SVN_ERR(svn_fs_begin_txn(&txn, fs, revisions[1], pool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, pool));
SVN_ERR(svn_test__set_file_contents
(txn_root, "A/mu", "A change to file 'mu'.\n", pool));
SVN_ERR(test_commit_txn(&after_rev, txn, "/A/mu", pool));
SVN_ERR(svn_fs_abort_txn(txn, pool));
{
svn_stringbuf_t *old_mu_contents;
SVN_ERR(svn_fs_begin_txn(&txn, fs, revisions[1], pool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, pool));
SVN_ERR(svn_test__get_file_contents
(txn_root, "A/mu", &old_mu_contents, pool));
if ((! old_mu_contents) || (strcmp(old_mu_contents->data,
"This is the file 'mu'.\n") != 0)) {
return svn_error_create
(SVN_ERR_FS_GENERAL, NULL,
"got wrong contents from an old revision tree");
}
SVN_ERR(svn_fs_make_file(txn_root, "A/sigma", pool));
SVN_ERR(svn_test__set_file_contents
(txn_root, "A/sigma", "This is the file 'sigma'.\n", pool));
SVN_ERR(test_commit_txn(&after_rev, txn, NULL, pool));
{
static svn_test__tree_entry_t expected_entries[] = {
{ "theta", "This is the file 'theta'.\n" },
{ "A", 0 },
{ "A/mu", "A new file 'mu'.\n" },
{ "A/sigma", "This is the file 'sigma'.\n" },
{ "A/B", 0 },
{ "A/B/lambda", "This is the file 'lambda'.\n" },
{ "A/B/E", 0 },
{ "A/B/E/alpha", "This is the file 'alpha'.\n" },
{ "A/B/E/beta", "This is the file 'beta'.\n" },
{ "A/B/F", 0 },
{ "A/C", 0 },
{ "A/C/kappa", "This is the file 'kappa'.\n" },
{ "A/D", 0 },
{ "A/D/gamma", "This is the file 'gamma'.\n" },
{ "A/D/G", 0 },
{ "A/D/G/pi", "This is the file 'pi'.\n" },
{ "A/D/G/rho", "This is the file 'rho'.\n" },
{ "A/D/G/tau", "This is the file 'tau'.\n" },
{ "A/D/G/xi", "This is the file 'xi'.\n" },
{ "A/D/I", 0 },
{ "A/D/I/delta", "This is the file 'delta'.\n" },
{ "A/D/I/epsilon", "This is the file 'epsilon'.\n" }
};
SVN_ERR(svn_fs_revision_root(&revision_root, fs, after_rev, pool));
SVN_ERR(svn_test__validate_tree(revision_root,
expected_entries,
22, pool));
}
revisions[revision_count++] = after_rev;
}
}
SVN_ERR(svn_fs_begin_txn(&txn, fs, revisions[revision_count-1], pool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, pool));
SVN_ERR(svn_test__set_file_contents
(txn_root, "A/B/lambda", "Change to file 'lambda'.\n", pool));
SVN_ERR(test_commit_txn(&after_rev, txn, NULL, pool));
{
static svn_test__tree_entry_t expected_entries[] = {
{ "theta", "This is the file 'theta'.\n" },
{ "A", 0 },
{ "A/mu", "A new file 'mu'.\n" },
{ "A/sigma", "This is the file 'sigma'.\n" },
{ "A/B", 0 },
{ "A/B/lambda", "Change to file 'lambda'.\n" },
{ "A/B/E", 0 },
{ "A/B/E/alpha", "This is the file 'alpha'.\n" },
{ "A/B/E/beta", "This is the file 'beta'.\n" },
{ "A/B/F", 0 },
{ "A/C", 0 },
{ "A/C/kappa", "This is the file 'kappa'.\n" },
{ "A/D", 0 },
{ "A/D/gamma", "This is the file 'gamma'.\n" },
{ "A/D/G", 0 },
{ "A/D/G/pi", "This is the file 'pi'.\n" },
{ "A/D/G/rho", "This is the file 'rho'.\n" },
{ "A/D/G/tau", "This is the file 'tau'.\n" },
{ "A/D/G/xi", "This is the file 'xi'.\n" },
{ "A/D/I", 0 },
{ "A/D/I/delta", "This is the file 'delta'.\n" },
{ "A/D/I/epsilon", "This is the file 'epsilon'.\n" }
};
SVN_ERR(svn_fs_revision_root(&revision_root, fs, after_rev, pool));
SVN_ERR(svn_test__validate_tree(revision_root, expected_entries,
22, pool));
}
revisions[revision_count++] = after_rev;
{
SVN_ERR(svn_fs_begin_txn(&txn, fs, revisions[1], pool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, pool));
SVN_ERR(svn_test__set_file_contents
(txn_root, "A/B/lambda", "A different change to 'lambda'.\n",
pool));
SVN_ERR(test_commit_txn(&after_rev, txn, "/A/B/lambda", pool));
SVN_ERR(svn_fs_abort_txn(txn, pool));
SVN_ERR(svn_fs_begin_txn(&txn, fs, revisions[1], pool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, pool));
SVN_ERR(svn_fs_make_file(txn_root, "A/D/G/nu", pool));
SVN_ERR(svn_test__set_file_contents
(txn_root, "A/D/G/nu", "This is the file 'nu'.\n", pool));
SVN_ERR(test_commit_txn(&after_rev, txn, NULL, pool));
{
static svn_test__tree_entry_t expected_entries[] = {
{ "theta", "This is the file 'theta'.\n" },
{ "A", 0 },
{ "A/mu", "A new file 'mu'.\n" },
{ "A/sigma", "This is the file 'sigma'.\n" },
{ "A/B", 0 },
{ "A/B/lambda", "Change to file 'lambda'.\n" },
{ "A/B/E", 0 },
{ "A/B/E/alpha", "This is the file 'alpha'.\n" },
{ "A/B/E/beta", "This is the file 'beta'.\n" },
{ "A/B/F", 0 },
{ "A/C", 0 },
{ "A/C/kappa", "This is the file 'kappa'.\n" },
{ "A/D", 0 },
{ "A/D/gamma", "This is the file 'gamma'.\n" },
{ "A/D/G", 0 },
{ "A/D/G/pi", "This is the file 'pi'.\n" },
{ "A/D/G/rho", "This is the file 'rho'.\n" },
{ "A/D/G/tau", "This is the file 'tau'.\n" },
{ "A/D/G/xi", "This is the file 'xi'.\n" },
{ "A/D/G/nu", "This is the file 'nu'.\n" },
{ "A/D/I", 0 },
{ "A/D/I/delta", "This is the file 'delta'.\n" },
{ "A/D/I/epsilon", "This is the file 'epsilon'.\n" }
};
SVN_ERR(svn_fs_revision_root(&revision_root, fs, after_rev, pool));
SVN_ERR(svn_test__validate_tree(revision_root,
expected_entries,
23, pool));
}
revisions[revision_count++] = after_rev;
SVN_ERR(svn_fs_begin_txn(&txn, fs, revisions[1], pool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, pool));
SVN_ERR(svn_fs_make_file(txn_root, "A/D/G/xi", pool));
SVN_ERR(svn_test__set_file_contents
(txn_root, "A/D/G/xi", "This is a different file 'xi'.\n", pool));
SVN_ERR(test_commit_txn(&after_rev, txn, "/A/D/G/xi", pool));
SVN_ERR(svn_fs_abort_txn(txn, pool));
{
svn_stringbuf_t *old_lambda_ctnts;
SVN_ERR(svn_fs_begin_txn(&txn, fs, revisions[1], pool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, pool));
SVN_ERR(svn_test__get_file_contents
(txn_root, "A/B/lambda", &old_lambda_ctnts, pool));
if ((! old_lambda_ctnts)
|| (strcmp(old_lambda_ctnts->data,
"This is the file 'lambda'.\n") != 0)) {
return svn_error_create
(SVN_ERR_FS_GENERAL, NULL,
"got wrong contents from an old revision tree");
}
SVN_ERR(svn_test__set_file_contents
(txn_root, "A/D/G/rho",
"This is an irrelevant change to 'rho'.\n", pool));
SVN_ERR(test_commit_txn(&after_rev, txn, NULL, pool));
{
static svn_test__tree_entry_t expected_entries[] = {
{ "theta", "This is the file 'theta'.\n" },
{ "A", 0 },
{ "A/mu", "A new file 'mu'.\n" },
{ "A/sigma", "This is the file 'sigma'.\n" },
{ "A/B", 0 },
{ "A/B/lambda", "Change to file 'lambda'.\n" },
{ "A/B/E", 0 },
{ "A/B/E/alpha", "This is the file 'alpha'.\n" },
{ "A/B/E/beta", "This is the file 'beta'.\n" },
{ "A/B/F", 0 },
{ "A/C", 0 },
{ "A/C/kappa", "This is the file 'kappa'.\n" },
{ "A/D", 0 },
{ "A/D/gamma", "This is the file 'gamma'.\n" },
{ "A/D/G", 0 },
{ "A/D/G/pi", "This is the file 'pi'.\n" },
{ "A/D/G/rho", "This is an irrelevant change to 'rho'.\n" },
{ "A/D/G/tau", "This is the file 'tau'.\n" },
{ "A/D/G/xi", "This is the file 'xi'.\n" },
{ "A/D/G/nu", "This is the file 'nu'.\n"},
{ "A/D/I", 0 },
{ "A/D/I/delta", "This is the file 'delta'.\n" },
{ "A/D/I/epsilon", "This is the file 'epsilon'.\n" }
};
SVN_ERR(svn_fs_revision_root(&revision_root, fs, after_rev, pool));
SVN_ERR(svn_test__validate_tree(revision_root,
expected_entries,
23, pool));
}
revisions[revision_count++] = after_rev;
}
}
{
}
SVN_ERR(svn_fs_begin_txn(&txn, fs, revisions[1], pool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, pool));
SVN_ERR(svn_test__set_file_contents
(txn_root, "iota", "New contents for 'iota'.\n", pool));
SVN_ERR(test_commit_txn(&after_rev, txn, "/iota", pool));
SVN_ERR(svn_fs_abort_txn(txn, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
copy_test(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
svn_fs_t *fs;
svn_fs_txn_t *txn;
svn_fs_root_t *txn_root, *rev_root;
svn_revnum_t after_rev;
*msg = "copying and tracking copy history";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(svn_test__create_fs(&fs, "test-repo-copy-test",
opts->fs_type, pool));
SVN_ERR(svn_fs_begin_txn(&txn, fs, 0, pool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, pool));
SVN_ERR(svn_test__create_greek_tree(txn_root, pool));
SVN_ERR(test_commit_txn(&after_rev, txn, NULL, pool));
SVN_ERR(svn_fs_revision_root(&rev_root, fs, after_rev, pool));
SVN_ERR(svn_fs_begin_txn(&txn, fs, after_rev, pool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, pool));
SVN_ERR(svn_fs_copy(rev_root, "A/D/G/pi",
txn_root, "A/D/H/pi2",
pool));
{
svn_revnum_t rev;
const char *path;
SVN_ERR(svn_fs_copied_from(&rev, &path, txn_root,
"A/D/H/pi2", pool));
if (rev != after_rev)
return svn_error_create
(SVN_ERR_FS_GENERAL, NULL,
"pre-commit copy history not preserved (rev lost) for A/D/H/pi2");
if (strcmp(path, "/A/D/G/pi") != 0)
return svn_error_create
(SVN_ERR_FS_GENERAL, NULL,
"pre-commit copy history not preserved (path lost) for A/D/H/pi2");
}
SVN_ERR(svn_test__set_file_contents
(txn_root, "A/D/H/pi2", "This is the file 'pi2'.\n", pool));
SVN_ERR(test_commit_txn(&after_rev, txn, NULL, pool));
{
svn_fs_root_t *root;
svn_revnum_t rev;
const char *path;
SVN_ERR(svn_fs_revision_root(&root, fs, after_rev, pool));
SVN_ERR(svn_fs_copied_from(&rev, &path, root, "A/D/H/pi2", pool));
if (rev != (after_rev - 1))
return svn_error_create
(SVN_ERR_FS_GENERAL, NULL,
"post-commit copy history wrong (rev) for A/D/H/pi2");
if (strcmp(path, "/A/D/G/pi") != 0)
return svn_error_create
(SVN_ERR_FS_GENERAL, NULL,
"post-commit copy history wrong (path) for A/D/H/pi2");
}
SVN_ERR(svn_fs_revision_root(&rev_root, fs, after_rev, pool));
SVN_ERR(svn_fs_begin_txn(&txn, fs, after_rev, pool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, pool));
SVN_ERR(svn_fs_copy(rev_root, "A/D/H/pi2", txn_root, "A/D/H/pi3", pool));
SVN_ERR(test_commit_txn(&after_rev, txn, NULL, pool));
{
svn_fs_root_t *root;
svn_revnum_t rev;
const char *path;
SVN_ERR(svn_fs_revision_root(&root, fs, (after_rev - 1), pool));
SVN_ERR(svn_fs_copied_from(&rev, &path, root, "A/D/H/pi2", pool));
if (rev != (after_rev - 2))
return svn_error_create
(SVN_ERR_FS_GENERAL, NULL,
"first copy history wrong (rev) for A/D/H/pi2");
if (strcmp(path, "/A/D/G/pi") != 0)
return svn_error_create
(SVN_ERR_FS_GENERAL, NULL,
"first copy history wrong (path) for A/D/H/pi2");
SVN_ERR(svn_fs_revision_root(&root, fs, after_rev, pool));
SVN_ERR(svn_fs_copied_from(&rev, &path, root, "A/D/H/pi3", pool));
if (rev != (after_rev - 1))
return svn_error_create
(SVN_ERR_FS_GENERAL, NULL,
"second copy history wrong (rev) for A/D/H/pi3");
if (strcmp(path, "/A/D/H/pi2") != 0)
return svn_error_create
(SVN_ERR_FS_GENERAL, NULL,
"second copy history wrong (path) for A/D/H/pi3");
}
SVN_ERR(svn_fs_revision_root(&rev_root, fs, after_rev, pool));
SVN_ERR(svn_fs_begin_txn(&txn, fs, after_rev, pool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, pool));
SVN_ERR(svn_test__set_file_contents
(txn_root, "A/D/H/pi3", "This is the file 'pi3'.\n", pool));
SVN_ERR(test_commit_txn(&after_rev, txn, NULL, pool));
{
svn_fs_root_t *root;
svn_revnum_t rev;
const char *path;
SVN_ERR(svn_fs_revision_root(&root, fs, (after_rev - 1), pool));
SVN_ERR(svn_fs_copied_from(&rev, &path, root, "A/D/H/pi3", pool));
if (rev != (after_rev - 2))
return svn_error_create
(SVN_ERR_FS_GENERAL, NULL,
"copy history wrong (rev) for A/D/H/pi3");
if (strcmp(path, "/A/D/H/pi2") != 0)
return svn_error_create
(SVN_ERR_FS_GENERAL, NULL,
"copy history wrong (path) for A/D/H/pi3");
SVN_ERR(svn_fs_revision_root(&root, fs, after_rev, pool));
SVN_ERR(svn_fs_copied_from(&rev, &path, root, "A/D/H/pi3", pool));
if (rev != SVN_INVALID_REVNUM)
return svn_error_create
(SVN_ERR_FS_GENERAL, NULL,
"copy history wrong (rev) for A/D/H/pi3");
if (path != NULL)
return svn_error_create
(SVN_ERR_FS_GENERAL, NULL,
"copy history wrong (path) for A/D/H/pi3");
}
SVN_ERR(svn_fs_revision_root(&rev_root, fs, after_rev, pool));
SVN_ERR(svn_fs_begin_txn(&txn, fs, after_rev, pool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, pool));
SVN_ERR(svn_fs_copy(rev_root, "A/D/H", txn_root, "H2", pool));
SVN_ERR(test_commit_txn(&after_rev, txn, NULL, pool));
{
svn_fs_root_t *root;
svn_revnum_t rev;
const char *path;
SVN_ERR(svn_fs_revision_root(&root, fs, after_rev, pool));
SVN_ERR(svn_fs_copied_from(&rev, &path, root, "H2", pool));
if (rev != (after_rev - 1))
return svn_error_create
(SVN_ERR_FS_GENERAL, NULL,
"copy history wrong (rev) for H2");
if (strcmp(path, "/A/D/H") != 0)
return svn_error_create
(SVN_ERR_FS_GENERAL, NULL,
"copy history wrong (path) for H2");
SVN_ERR(svn_fs_copied_from(&rev, &path, root, "H2/omega", pool));
if (rev != SVN_INVALID_REVNUM)
return svn_error_create
(SVN_ERR_FS_GENERAL, NULL,
"copy history wrong (rev) for H2/omega");
if (path != NULL)
return svn_error_create
(SVN_ERR_FS_GENERAL, NULL,
"copy history wrong (path) for H2/omega");
}
SVN_ERR(svn_fs_revision_root(&rev_root, fs, after_rev, pool));
SVN_ERR(svn_fs_begin_txn(&txn, fs, after_rev, pool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, pool));
SVN_ERR(svn_fs_copy(rev_root, "A/B", txn_root, "A/B/E/B", pool));
SVN_ERR(test_commit_txn(&after_rev, txn, NULL, pool));
{
svn_fs_root_t *root;
svn_revnum_t rev;
const char *path;
SVN_ERR(svn_fs_revision_root(&root, fs, after_rev, pool));
SVN_ERR(svn_fs_copied_from(&rev, &path, root, "A/B/E/B", pool));
if (rev != (after_rev - 1))
return svn_error_create
(SVN_ERR_FS_GENERAL, NULL,
"copy history wrong (rev) for A/B/E/B");
if (strcmp(path, "/A/B") != 0)
return svn_error_create
(SVN_ERR_FS_GENERAL, NULL,
"copy history wrong (path) for A/B/E/B");
SVN_ERR(svn_fs_revision_root(&root, fs, after_rev, pool));
SVN_ERR(svn_fs_copied_from(&rev, &path, root, "A/B", pool));
if (rev != SVN_INVALID_REVNUM)
return svn_error_create
(SVN_ERR_FS_GENERAL, NULL,
"copy history wrong (rev) for A/B");
if (path != NULL)
return svn_error_create
(SVN_ERR_FS_GENERAL, NULL,
"copy history wrong (path) for A/B");
}
{
static svn_test__tree_entry_t expected_entries[] = {
{ "iota", "This is the file 'iota'.\n" },
{ "H2", 0 },
{ "H2/chi", "This is the file 'chi'.\n" },
{ "H2/pi2", "This is the file 'pi2'.\n" },
{ "H2/pi3", "This is the file 'pi3'.\n" },
{ "H2/psi", "This is the file 'psi'.\n" },
{ "H2/omega", "This is the file 'omega'.\n" },
{ "A", 0 },
{ "A/mu", "This is the file 'mu'.\n" },
{ "A/B", 0 },
{ "A/B/lambda", "This is the file 'lambda'.\n" },
{ "A/B/E", 0 },
{ "A/B/E/alpha", "This is the file 'alpha'.\n" },
{ "A/B/E/beta", "This is the file 'beta'.\n" },
{ "A/B/E/B", 0 },
{ "A/B/E/B/lambda", "This is the file 'lambda'.\n" },
{ "A/B/E/B/E", 0 },
{ "A/B/E/B/E/alpha", "This is the file 'alpha'.\n" },
{ "A/B/E/B/E/beta", "This is the file 'beta'.\n" },
{ "A/B/E/B/F", 0 },
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
{ "A/D/H/pi2", "This is the file 'pi2'.\n" },
{ "A/D/H/pi3", "This is the file 'pi3'.\n" },
{ "A/D/H/psi", "This is the file 'psi'.\n" },
{ "A/D/H/omega", "This is the file 'omega'.\n" }
};
SVN_ERR(svn_fs_revision_root(&rev_root, fs, after_rev, pool));
SVN_ERR(svn_test__validate_tree(rev_root, expected_entries,
34, pool));
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
opts->fs_type, pool));
SVN_ERR(svn_fs_begin_txn(&txn, fs, 0, pool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, pool));
SVN_ERR(svn_test__create_greek_tree(txn_root, pool));
{
const svn_fs_id_t *gamma_id;
SVN_ERR(svn_fs_node_id(&gamma_id, txn_root, "A/D/gamma", pool));
SVN_ERR(check_entry_present(txn_root, "A/D", "gamma", pool));
SVN_ERR(svn_fs_delete(txn_root, "A/D/gamma", pool));
SVN_ERR(check_entry_absent(txn_root, "A/D", "gamma", pool));
}
{
const svn_fs_id_t *pi_id, *rho_id, *tau_id;
SVN_ERR(svn_fs_node_id(&pi_id, txn_root, "A/D/G/pi", pool));
SVN_ERR(svn_fs_node_id(&rho_id, txn_root, "A/D/G/rho", pool));
SVN_ERR(svn_fs_node_id(&tau_id, txn_root, "A/D/G/tau", pool));
SVN_ERR(check_entry_present(txn_root, "A/D/G", "pi", pool));
SVN_ERR(check_entry_present(txn_root, "A/D/G", "rho", pool));
SVN_ERR(check_entry_present(txn_root, "A/D/G", "tau", pool));
SVN_ERR(svn_fs_delete(txn_root, "A/D/G/pi", pool));
SVN_ERR(check_entry_absent(txn_root, "A/D/G", "pi", pool));
SVN_ERR(check_entry_present(txn_root, "A/D/G", "rho", pool));
SVN_ERR(check_entry_present(txn_root, "A/D/G", "tau", pool));
SVN_ERR(svn_fs_delete(txn_root, "A/D/G/rho", pool));
SVN_ERR(check_entry_absent(txn_root, "A/D/G", "pi", pool));
SVN_ERR(check_entry_absent(txn_root, "A/D/G", "rho", pool));
SVN_ERR(check_entry_present(txn_root, "A/D/G", "tau", pool));
}
{
const svn_fs_id_t *tau_id;
SVN_ERR(svn_fs_node_id(&tau_id, txn_root, "A/D/G/tau", pool));
SVN_ERR(check_entry_present(txn_root, "A/D/G", "tau", pool));
SVN_ERR(svn_fs_delete(txn_root, "A/D/G/tau", pool));
SVN_ERR(check_entry_absent(txn_root, "A/D/G", "tau", pool));
}
{
const svn_fs_id_t *G_id;
SVN_ERR(svn_fs_node_id(&G_id, txn_root, "A/D/G", pool));
SVN_ERR(check_entry_present(txn_root, "A/D", "G", pool));
SVN_ERR(svn_fs_delete(txn_root, "A/D/G", pool));
SVN_ERR(check_entry_absent(txn_root, "A/D", "G", pool));
}
{
const svn_fs_id_t *C_id;
SVN_ERR(svn_fs_node_id(&C_id, txn_root, "A/C", pool));
SVN_ERR(check_entry_present(txn_root, "A", "C", pool));
SVN_ERR(svn_fs_delete(txn_root, "A/C", pool));
SVN_ERR(check_entry_absent(txn_root, "A", "C", pool));
}
{
const svn_fs_id_t *root_id;
SVN_ERR(svn_fs_node_id(&root_id, txn_root, "", pool));
err = svn_fs_delete(txn_root, "", pool);
if (err && (err->apr_err != SVN_ERR_FS_ROOT_DIR)) {
return svn_error_createf
(SVN_ERR_FS_GENERAL, NULL,
"deleting root directory got wrong error");
} else if (! err) {
return svn_error_createf
(SVN_ERR_FS_GENERAL, NULL,
"deleting root directory failed to get error");
}
svn_error_clear(err);
}
{
const svn_fs_id_t *iota_id;
SVN_ERR(svn_fs_node_id(&iota_id, txn_root, "iota", pool));
SVN_ERR(check_entry_present(txn_root, "", "iota", pool));
SVN_ERR(svn_fs_delete(txn_root, "iota", pool));
SVN_ERR(check_entry_absent(txn_root, "", "iota", pool));
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
opts->fs_type, pool));
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
SVN_ERR(svn_fs_delete(txn_root, "iota", pool));
SVN_ERR(svn_fs_delete(txn_root, "A/D/gamma", pool));
SVN_ERR(check_entry_absent(txn_root, "", "iota", pool));
SVN_ERR(check_entry_absent(txn_root, "A/D", "gamma", pool));
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
SVN_ERR(svn_fs_delete(txn_root, "A", pool));
SVN_ERR(check_entry_absent(txn_root, "", "A", pool));
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
SVN_ERR(svn_fs_delete(txn_root, "iota", pool));
SVN_ERR(svn_fs_delete(txn_root, "A/D/gamma", pool));
SVN_ERR(check_entry_absent(txn_root, "", "iota", pool));
SVN_ERR(check_entry_absent(txn_root, "A/D", "iota", pool));
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
{
static svn_test__tree_entry_t expected_entries[] = {
{ "iota", "This is the file 'iota'.\n" }
};
SVN_ERR(svn_test__validate_tree(txn_root, expected_entries, 1, pool));
}
}
return SVN_NO_ERROR;
}
static svn_error_t *
commit_date(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
svn_fs_t *fs;
svn_fs_txn_t *txn;
svn_fs_root_t *txn_root;
svn_revnum_t rev;
svn_string_t *datestamp;
apr_time_t before_commit, at_commit, after_commit;
*msg = "commit datestamps";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(svn_test__create_fs(&fs, "test-repo-commit-date",
opts->fs_type, pool));
before_commit = apr_time_now();
SVN_ERR(svn_fs_begin_txn(&txn, fs, 0, pool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, pool));
SVN_ERR(svn_test__create_greek_tree(txn_root, pool));
SVN_ERR(svn_fs_commit_txn(NULL, &rev, txn, pool));
after_commit = apr_time_now();
SVN_ERR(svn_fs_revision_prop(&datestamp, fs, rev, SVN_PROP_REVISION_DATE,
pool));
if (datestamp == NULL)
return svn_error_create
(SVN_ERR_FS_GENERAL, NULL,
"failed to get datestamp of committed revision");
SVN_ERR(svn_time_from_cstring(&at_commit, datestamp->data, pool));
if (at_commit < before_commit)
return svn_error_create
(SVN_ERR_FS_GENERAL, NULL,
"datestamp too early");
if (at_commit > after_commit)
return svn_error_create
(SVN_ERR_FS_GENERAL, NULL,
"datestamp too late");
return SVN_NO_ERROR;
}
static svn_error_t *
check_old_revisions(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
svn_fs_t *fs;
svn_fs_txn_t *txn;
svn_fs_root_t *txn_root;
svn_revnum_t rev;
apr_pool_t *subpool = svn_pool_create(pool);
*msg = "check old revisions";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(svn_test__create_fs(&fs, "test-repo-check-old-revisions",
opts->fs_type, pool));
SVN_ERR(svn_fs_begin_txn(&txn, fs, 0, subpool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, subpool));
SVN_ERR(svn_test__create_greek_tree(txn_root, subpool));
SVN_ERR(svn_fs_commit_txn(NULL, &rev, txn, subpool));
svn_pool_clear(subpool);
{
#define iota_contents_1 "This is the file 'iota'.\n"
#define iota_contents_2 "XThis is the file 'iota'.\n"
#define iota_contents_3 "XThis is the file 'iota'.\nX"
#define iota_contents_4 "XThis is the X file 'iota'.\nX"
#define iota_contents_5 "XTYhQis is ACK, PHHHT! no longer 'ioZZZZZta'.blarf\nbye"
#define iota_contents_6 "Matthew 5:18 (Revised Standard Version) --\nFor truly, I say to you, till heaven and earth pass away, not an iota,\nnot a dot, will pass from the law until all is accomplished."
#define iota_contents_7 "This is the file 'iota'.\n"
SVN_ERR(svn_fs_begin_txn(&txn, fs, rev, subpool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, subpool));
SVN_ERR(svn_test__set_file_contents
(txn_root, "iota", iota_contents_2, subpool));
SVN_ERR(svn_fs_commit_txn(NULL, &rev, txn, subpool));
svn_pool_clear(subpool);
SVN_ERR(svn_fs_begin_txn(&txn, fs, rev, subpool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, subpool));
SVN_ERR(svn_test__set_file_contents
(txn_root, "iota", iota_contents_3, subpool));
SVN_ERR(svn_fs_commit_txn(NULL, &rev, txn, subpool));
svn_pool_clear(subpool);
SVN_ERR(svn_fs_begin_txn(&txn, fs, rev, subpool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, subpool));
SVN_ERR(svn_test__set_file_contents
(txn_root, "iota", iota_contents_4, subpool));
SVN_ERR(svn_fs_commit_txn(NULL, &rev, txn, subpool));
svn_pool_clear(subpool);
SVN_ERR(svn_fs_begin_txn(&txn, fs, rev, subpool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, subpool));
SVN_ERR(svn_test__set_file_contents
(txn_root, "iota", iota_contents_5, subpool));
SVN_ERR(svn_fs_commit_txn(NULL, &rev, txn, subpool));
svn_pool_clear(subpool);
SVN_ERR(svn_fs_begin_txn(&txn, fs, rev, subpool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, subpool));
SVN_ERR(svn_test__set_file_contents
(txn_root, "iota", iota_contents_6, subpool));
SVN_ERR(svn_fs_commit_txn(NULL, &rev, txn, subpool));
svn_pool_clear(subpool);
SVN_ERR(svn_fs_begin_txn(&txn, fs, rev, subpool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, subpool));
SVN_ERR(svn_test__set_file_contents
(txn_root, "iota", iota_contents_7, subpool));
SVN_ERR(svn_fs_commit_txn(NULL, &rev, txn, subpool));
svn_pool_clear(subpool);
{
svn_fs_root_t *root;
static svn_test__tree_entry_t expected_entries[] = {
{ "iota", iota_contents_1 },
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
SVN_ERR(svn_fs_revision_root(&root, fs, 1, pool));
SVN_ERR(svn_test__validate_tree(root, expected_entries, 20, pool));
}
{
svn_fs_root_t *root;
static svn_test__tree_entry_t expected_entries[] = {
{ "iota", iota_contents_2 },
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
SVN_ERR(svn_fs_revision_root(&root, fs, 2, pool));
SVN_ERR(svn_test__validate_tree(root, expected_entries, 20, pool));
}
{
svn_fs_root_t *root;
static svn_test__tree_entry_t expected_entries[] = {
{ "iota", iota_contents_3 },
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
SVN_ERR(svn_fs_revision_root(&root, fs, 3, pool));
SVN_ERR(svn_test__validate_tree(root, expected_entries, 20, pool));
}
{
svn_fs_root_t *root;
static svn_test__tree_entry_t expected_entries[] = {
{ "iota", iota_contents_4 },
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
SVN_ERR(svn_fs_revision_root(&root, fs, 4, pool));
SVN_ERR(svn_test__validate_tree(root, expected_entries, 20, pool));
}
{
svn_fs_root_t *root;
static svn_test__tree_entry_t expected_entries[] = {
{ "iota", iota_contents_5 },
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
{ "A/D/gamma", "This is the file 'gamma'.\n" },
{ "A/D/G/pi", "This is the file 'pi'.\n" },
{ "A/D/G/rho", "This is the file 'rho'.\n" },
{ "A/D/G/tau", "This is the file 'tau'.\n" },
{ "A/D/H", 0 },
{ "A/D/H/chi", "This is the file 'chi'.\n" },
{ "A/D/H/psi", "This is the file 'psi'.\n" },
{ "A/D/H/omega", "This is the file 'omega'.\n" }
};
SVN_ERR(svn_fs_revision_root(&root, fs, 5, pool));
SVN_ERR(svn_test__validate_tree(root, expected_entries, 20, pool));
}
{
svn_fs_root_t *root;
static svn_test__tree_entry_t expected_entries[] = {
{ "iota", iota_contents_6 },
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
SVN_ERR(svn_fs_revision_root(&root, fs, 6, pool));
SVN_ERR(svn_test__validate_tree(root, expected_entries, 20, pool));
}
{
svn_fs_root_t *root;
static svn_test__tree_entry_t expected_entries[] = {
{ "iota", iota_contents_7 },
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
SVN_ERR(svn_fs_revision_root(&root, fs, 7, pool));
SVN_ERR(svn_test__validate_tree(root, expected_entries, 20, pool));
}
}
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
static svn_error_t *
validate_revisions(svn_fs_t *fs,
svn_test__tree_t *expected_trees,
svn_revnum_t max_rev,
apr_pool_t *pool) {
svn_fs_root_t *revision_root;
svn_revnum_t i;
svn_error_t *err;
apr_pool_t *subpool = svn_pool_create(pool);
for (i = 0; i <= max_rev; i++) {
SVN_ERR(svn_fs_revision_root(&revision_root, fs,
(svn_revnum_t)i, subpool));
err = svn_test__validate_tree(revision_root,
expected_trees[i].entries,
expected_trees[i].num_entries,
subpool);
if (err)
return svn_error_createf
(SVN_ERR_FS_GENERAL, err,
"Error validating revision %ld (youngest is %ld)", i, max_rev);
svn_pool_clear(subpool);
}
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
static svn_error_t *
check_all_revisions(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
svn_fs_t *fs;
svn_fs_txn_t *txn;
svn_fs_root_t *txn_root;
svn_revnum_t youngest_rev;
svn_test__tree_t expected_trees[5];
svn_revnum_t revision_count = 0;
apr_pool_t *subpool = svn_pool_create(pool);
*msg = "after each commit, check all revisions";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(svn_test__create_fs(&fs, "test-repo-check-all-revisions",
opts->fs_type, pool));
{
expected_trees[revision_count].num_entries = 0;
expected_trees[revision_count].entries = 0;
SVN_ERR(validate_revisions(fs, expected_trees, revision_count, subpool));
revision_count++;
}
svn_pool_clear(subpool);
SVN_ERR(svn_fs_begin_txn(&txn, fs, 0, subpool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, subpool));
SVN_ERR(svn_test__create_greek_tree(txn_root, subpool));
SVN_ERR(svn_fs_commit_txn(NULL, &youngest_rev, txn, subpool));
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
SVN_ERR(validate_revisions(fs, expected_trees, revision_count, subpool));
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
SVN_ERR(svn_fs_commit_txn(NULL, &youngest_rev, txn, subpool));
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
SVN_ERR(validate_revisions(fs, expected_trees, revision_count, subpool));
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
SVN_ERR(svn_test__txn_script_exec(txn_root, script_entries, 4,
subpool));
}
SVN_ERR(svn_fs_commit_txn(NULL, &youngest_rev, txn, subpool));
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
SVN_ERR(validate_revisions(fs, expected_trees, revision_count, subpool));
revision_count++;
}
svn_pool_clear(subpool);
SVN_ERR(svn_fs_begin_txn(&txn, fs, youngest_rev, subpool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, subpool));
{
static svn_test__txn_script_command_t script_entries[] = {
{ 'c', "A/D/G", "A/D/G2" },
{ 'c', "A/epsilon", "A/B/epsilon" },
};
SVN_ERR(svn_test__txn_script_exec(txn_root, script_entries, 2, subpool));
}
SVN_ERR(svn_fs_commit_txn(NULL, &youngest_rev, txn, subpool));
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
SVN_ERR(validate_revisions(fs, expected_trees, revision_count, subpool));
revision_count++;
}
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
static svn_error_t *
get_file_digest(unsigned char digest[APR_MD5_DIGESTSIZE],
svn_fs_root_t *root,
const char *path,
apr_pool_t *pool) {
svn_stream_t *stream;
apr_size_t len;
const apr_size_t buf_size = 100000;
apr_md5_ctx_t context;
char *buf = apr_palloc(pool, buf_size);
SVN_ERR(svn_fs_file_contents(&stream, root, path, pool));
apr_md5_init(&context);
do {
len = buf_size;
SVN_ERR(svn_stream_read(stream, buf, &len));
apr_md5_update(&context, buf, len);
} while (len == buf_size);
apr_md5_final(digest, &context);
return SVN_NO_ERROR;
}
static int my_rand(int scalar, apr_uint32_t *seed) {
static const apr_uint32_t TEST_RAND_MAX = 0xffffffffUL;
return (int)(((double)svn_test_rand(seed)
/ ((double)TEST_RAND_MAX+1.0))
* (double)scalar);
}
static void
random_data_to_buffer(char *buf,
apr_size_t buf_len,
svn_boolean_t full,
apr_uint32_t *seed) {
apr_size_t i;
apr_size_t num_bytes;
apr_size_t offset;
int ds_off = 0;
const char *dataset = "0123456789";
int dataset_size = strlen(dataset);
if (full) {
for (i = 0; i < buf_len; i++) {
ds_off = my_rand(dataset_size, seed);
buf[i] = dataset[ds_off];
}
return;
}
num_bytes = my_rand(buf_len / 100, seed) + 1;
for (i = 0; i < num_bytes; i++) {
offset = my_rand(buf_len - 1, seed);
ds_off = my_rand(dataset_size, seed);
buf[offset] = dataset[ds_off];
}
return;
}
static svn_error_t *
file_integrity_helper(apr_size_t filesize, apr_uint32_t *seed,
const char *fs_type, const char *fs_name,
apr_pool_t *pool) {
svn_fs_t *fs;
svn_fs_txn_t *txn;
svn_fs_root_t *txn_root, *rev_root;
svn_revnum_t youngest_rev = 0;
apr_pool_t *subpool = svn_pool_create(pool);
svn_string_t contents;
char *content_buffer;
unsigned char digest[APR_MD5_DIGESTSIZE];
unsigned char digest_list[100][APR_MD5_DIGESTSIZE];
svn_txdelta_window_handler_t wh_func;
void *wh_baton;
svn_revnum_t j;
SVN_ERR(svn_test__create_fs(&fs, fs_name, fs_type, pool));
content_buffer = apr_palloc(pool, filesize);
contents.data = content_buffer;
contents.len = filesize;
SVN_ERR(svn_fs_begin_txn(&txn, fs, youngest_rev, subpool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, subpool));
SVN_ERR(svn_fs_make_file(txn_root, "bigfile", subpool));
random_data_to_buffer(content_buffer, filesize, TRUE, seed);
apr_md5(digest, contents.data, contents.len);
SVN_ERR(svn_fs_apply_textdelta
(&wh_func, &wh_baton, txn_root, "bigfile", NULL, NULL, subpool));
SVN_ERR(svn_txdelta_send_string(&contents, wh_func, wh_baton, subpool));
SVN_ERR(svn_fs_commit_txn(NULL, &youngest_rev, txn, subpool));
SVN_ERR(svn_fs_deltify_revision(fs, youngest_rev, subpool));
memcpy(digest_list[youngest_rev], digest, APR_MD5_DIGESTSIZE);
svn_pool_clear(subpool);
SVN_ERR(svn_fs_begin_txn(&txn, fs, youngest_rev, subpool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, subpool));
random_data_to_buffer(content_buffer, 20, TRUE, seed);
apr_md5(digest, contents.data, contents.len);
SVN_ERR(svn_fs_apply_textdelta
(&wh_func, &wh_baton, txn_root, "bigfile", NULL, NULL, subpool));
SVN_ERR(svn_txdelta_send_string(&contents, wh_func, wh_baton, subpool));
SVN_ERR(svn_fs_commit_txn(NULL, &youngest_rev, txn, subpool));
SVN_ERR(svn_fs_deltify_revision(fs, youngest_rev, subpool));
memcpy(digest_list[youngest_rev], digest, APR_MD5_DIGESTSIZE);
svn_pool_clear(subpool);
SVN_ERR(svn_fs_begin_txn(&txn, fs, youngest_rev, subpool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, subpool));
random_data_to_buffer(content_buffer + (filesize - 20), 20, TRUE, seed);
apr_md5(digest, contents.data, contents.len);
SVN_ERR(svn_fs_apply_textdelta
(&wh_func, &wh_baton, txn_root, "bigfile", NULL, NULL, subpool));
SVN_ERR(svn_txdelta_send_string(&contents, wh_func, wh_baton, subpool));
SVN_ERR(svn_fs_commit_txn(NULL, &youngest_rev, txn, subpool));
SVN_ERR(svn_fs_deltify_revision(fs, youngest_rev, subpool));
memcpy(digest_list[youngest_rev], digest, APR_MD5_DIGESTSIZE);
svn_pool_clear(subpool);
SVN_ERR(svn_fs_begin_txn(&txn, fs, youngest_rev, subpool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, subpool));
random_data_to_buffer(content_buffer, 20, TRUE, seed);
random_data_to_buffer(content_buffer + (filesize - 20), 20, TRUE, seed);
apr_md5(digest, contents.data, contents.len);
SVN_ERR(svn_fs_apply_textdelta
(&wh_func, &wh_baton, txn_root, "bigfile", NULL, NULL, subpool));
SVN_ERR(svn_txdelta_send_string(&contents, wh_func, wh_baton, subpool));
SVN_ERR(svn_fs_commit_txn(NULL, &youngest_rev, txn, subpool));
SVN_ERR(svn_fs_deltify_revision(fs, youngest_rev, subpool));
memcpy(digest_list[youngest_rev], digest, APR_MD5_DIGESTSIZE);
svn_pool_clear(subpool);
for (j = youngest_rev; j < 30; j = youngest_rev) {
SVN_ERR(svn_fs_begin_txn(&txn, fs, youngest_rev, subpool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, subpool));
random_data_to_buffer(content_buffer, filesize, FALSE, seed);
apr_md5(digest, contents.data, contents.len);
SVN_ERR(svn_fs_apply_textdelta(&wh_func, &wh_baton, txn_root,
"bigfile", NULL, NULL, subpool));
SVN_ERR(svn_txdelta_send_string
(&contents, wh_func, wh_baton, subpool));
SVN_ERR(svn_fs_commit_txn(NULL, &youngest_rev, txn, subpool));
SVN_ERR(svn_fs_deltify_revision(fs, youngest_rev, subpool));
memcpy(digest_list[youngest_rev], digest, APR_MD5_DIGESTSIZE);
svn_pool_clear(subpool);
}
for (j = youngest_rev; j > 0; j--) {
SVN_ERR(svn_fs_revision_root(&rev_root, fs, j, subpool));
SVN_ERR(get_file_digest(digest, rev_root, "bigfile", subpool));
if (memcmp(digest, digest_list[j], APR_MD5_DIGESTSIZE))
return svn_error_createf
(SVN_ERR_FS_GENERAL, NULL,
"MD5 checksum failure, revision %ld", j);
svn_pool_clear(subpool);
}
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
static svn_error_t *
medium_file_integrity(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
apr_uint32_t seed = (apr_uint32_t) apr_time_now();
*msg = apr_psprintf(pool,
"create and modify medium file (seed=%lu)",
(unsigned long) seed);
if (msg_only)
return SVN_NO_ERROR;
return file_integrity_helper(SVN_DELTA_WINDOW_SIZE, &seed, opts->fs_type,
"test-repo-medium-file-integrity", pool);
}
static svn_error_t *
large_file_integrity(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
apr_uint32_t seed = (apr_uint32_t) apr_time_now();
*msg = apr_psprintf(pool,
"create and modify large file (seed=%lu)",
(unsigned long) seed);
if (msg_only)
return SVN_NO_ERROR;
return file_integrity_helper(SVN_DELTA_WINDOW_SIZE + 1, &seed, opts->fs_type,
"test-repo-large-file-integrity", pool);
}
static svn_error_t *
check_root_revision(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
svn_fs_t *fs;
svn_fs_txn_t *txn;
svn_fs_root_t *txn_root, *rev_root;
svn_revnum_t youngest_rev, test_rev;
apr_pool_t *subpool = svn_pool_create(pool);
int i;
*msg = "ensure accurate storage of root node";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(svn_test__create_fs(&fs, "test-repo-check-root-revision",
opts->fs_type, pool));
SVN_ERR(svn_fs_begin_txn(&txn, fs, 0, subpool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, subpool));
SVN_ERR(svn_test__create_greek_tree(txn_root, subpool));
SVN_ERR(svn_fs_commit_txn(NULL, &youngest_rev, txn, subpool));
SVN_ERR(svn_fs_revision_root(&rev_root, fs, youngest_rev, subpool));
SVN_ERR(svn_fs_node_created_rev(&test_rev, rev_root, "", subpool));
if (test_rev != youngest_rev)
return svn_error_createf
(SVN_ERR_FS_GENERAL, NULL,
"Root node in revision %ld has unexpected stored revision %ld",
youngest_rev, test_rev);
svn_pool_clear(subpool);
for (i = 0; i < 10; i++) {
SVN_ERR(svn_fs_begin_txn(&txn, fs, youngest_rev, subpool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, subpool));
SVN_ERR(svn_test__set_file_contents
(txn_root, "iota",
apr_psprintf(subpool, "iota version %d", i + 2), subpool));
SVN_ERR(svn_fs_commit_txn(NULL, &youngest_rev, txn, subpool));
SVN_ERR(svn_fs_revision_root(&rev_root, fs, youngest_rev, subpool));
SVN_ERR(svn_fs_node_created_rev(&test_rev, rev_root, "", subpool));
if (test_rev != youngest_rev)
return svn_error_createf
(SVN_ERR_FS_GENERAL, NULL,
"Root node in revision %ld has unexpected stored revision %ld",
youngest_rev, test_rev);
svn_pool_clear(subpool);
}
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
struct node_created_rev_args {
const char *path;
svn_revnum_t rev;
};
static svn_error_t *
verify_path_revs(svn_fs_root_t *root,
struct node_created_rev_args *args,
int num_path_revs,
apr_pool_t *pool) {
apr_pool_t *subpool = svn_pool_create(pool);
int i;
svn_revnum_t rev;
for (i = 0; i < num_path_revs; i++) {
svn_pool_clear(subpool);
SVN_ERR(svn_fs_node_created_rev(&rev, root, args[i].path, subpool));
if (rev != args[i].rev)
return svn_error_createf
(SVN_ERR_FS_GENERAL, NULL,
"verify_path_revs: '%s' has created rev '%ld' "
"(expected '%ld')",
args[i].path, rev, args[i].rev);
}
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
static svn_error_t *
test_node_created_rev(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
apr_pool_t *subpool = svn_pool_create(pool);
svn_fs_t *fs;
svn_fs_txn_t *txn;
svn_fs_root_t *txn_root, *rev_root;
svn_revnum_t youngest_rev = 0;
int i;
struct node_created_rev_args path_revs[21];
const char *greek_paths[21] = {
"",
"iota",
"A",
"A/mu",
"A/B",
"A/B/lambda",
"A/B/E",
"A/B/E/alpha",
"A/B/E/beta",
"A/B/F",
"A/C",
"A/D",
"A/D/gamma",
"A/D/G",
"A/D/G/pi",
"A/D/G/rho",
"A/D/G/tau",
"A/D/H",
"A/D/H/chi",
"A/D/H/psi",
"A/D/H/omega",
};
*msg = "svn_fs_node_created_rev test";
if (msg_only)
return SVN_NO_ERROR;
for (i = 0; i < 20; i++)
path_revs[i].path = greek_paths[i];
SVN_ERR(svn_test__create_fs(&fs, "test-repo-node-created-rev",
opts->fs_type, pool));
SVN_ERR(svn_fs_begin_txn(&txn, fs, youngest_rev, subpool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, subpool));
SVN_ERR(svn_test__create_greek_tree(txn_root, subpool));
for (i = 0; i < 20; i++)
path_revs[i].rev = SVN_INVALID_REVNUM;
SVN_ERR(verify_path_revs(txn_root, path_revs, 20, subpool));
SVN_ERR(svn_fs_commit_txn(NULL, &youngest_rev, txn, subpool));
SVN_ERR(svn_fs_revision_root(&rev_root, fs, youngest_rev, subpool));
for (i = 0; i < 20; i++)
path_revs[i].rev = 1;
SVN_ERR(verify_path_revs(rev_root, path_revs, 20, subpool));
svn_pool_clear(subpool);
SVN_ERR(svn_fs_begin_txn(&txn, fs, youngest_rev, subpool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, subpool));
SVN_ERR(verify_path_revs(txn_root, path_revs, 20, subpool));
SVN_ERR(svn_test__set_file_contents
(txn_root, "iota", "pointless mod here", subpool));
path_revs[0].rev = SVN_INVALID_REVNUM;
path_revs[1].rev = SVN_INVALID_REVNUM;
SVN_ERR(verify_path_revs(txn_root, path_revs, 20, subpool));
SVN_ERR(svn_fs_commit_txn(NULL, &youngest_rev, txn, subpool));
SVN_ERR(svn_fs_revision_root(&rev_root, fs, youngest_rev, subpool));
path_revs[0].rev = 2;
path_revs[1].rev = 2;
SVN_ERR(verify_path_revs(rev_root, path_revs, 20, subpool));
svn_pool_clear(subpool);
SVN_ERR(svn_fs_begin_txn(&txn, fs, youngest_rev, subpool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, subpool));
SVN_ERR(svn_test__set_file_contents
(txn_root, "A/D/H/omega", "pointless mod here", subpool));
path_revs[0].rev = SVN_INVALID_REVNUM;
path_revs[2].rev = SVN_INVALID_REVNUM;
path_revs[11].rev = SVN_INVALID_REVNUM;
path_revs[17].rev = SVN_INVALID_REVNUM;
path_revs[20].rev = SVN_INVALID_REVNUM;
SVN_ERR(verify_path_revs(txn_root, path_revs, 20, subpool));
SVN_ERR(svn_fs_commit_txn(NULL, &youngest_rev, txn, subpool));
SVN_ERR(svn_fs_revision_root(&rev_root, fs, youngest_rev, subpool));
path_revs[0].rev = 3;
path_revs[2].rev = 3;
path_revs[11].rev = 3;
path_revs[17].rev = 3;
path_revs[20].rev = 3;
SVN_ERR(verify_path_revs(rev_root, path_revs, 20, subpool));
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
static svn_error_t *
check_related(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
apr_pool_t *subpool = svn_pool_create(pool);
svn_fs_t *fs;
svn_fs_txn_t *txn;
svn_fs_root_t *txn_root, *rev_root;
svn_revnum_t youngest_rev = 0;
*msg = "test svn_fs_check_related";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(svn_test__create_fs(&fs, "test-repo-check-related",
opts->fs_type, pool));
SVN_ERR(svn_fs_begin_txn(&txn, fs, youngest_rev, subpool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, subpool));
SVN_ERR(svn_fs_make_file(txn_root, "A", subpool));
SVN_ERR(svn_test__set_file_contents(txn_root, "A", "1", subpool));
SVN_ERR(svn_fs_commit_txn(NULL, &youngest_rev, txn, subpool));
svn_pool_clear(subpool);
SVN_ERR(svn_fs_begin_txn(&txn, fs, youngest_rev, subpool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, subpool));
SVN_ERR(svn_test__set_file_contents(txn_root, "A", "2", subpool));
SVN_ERR(svn_fs_commit_txn(NULL, &youngest_rev, txn, subpool));
svn_pool_clear(subpool);
SVN_ERR(svn_fs_begin_txn(&txn, fs, youngest_rev, subpool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, subpool));
SVN_ERR(svn_test__set_file_contents(txn_root, "A", "3", subpool));
SVN_ERR(svn_fs_commit_txn(NULL, &youngest_rev, txn, subpool));
svn_pool_clear(subpool);
SVN_ERR(svn_fs_begin_txn(&txn, fs, youngest_rev, subpool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, subpool));
SVN_ERR(svn_test__set_file_contents(txn_root, "A", "4", subpool));
SVN_ERR(svn_fs_revision_root(&rev_root, fs, 3, subpool));
SVN_ERR(svn_fs_copy(rev_root, "A", txn_root, "B", subpool));
SVN_ERR(svn_test__set_file_contents(txn_root, "B", "4", subpool));
SVN_ERR(svn_fs_revision_root(&rev_root, fs, 1, subpool));
SVN_ERR(svn_fs_copy(rev_root, "A", txn_root, "C", subpool));
SVN_ERR(svn_test__set_file_contents(txn_root, "C", "4", subpool));
SVN_ERR(svn_fs_commit_txn(NULL, &youngest_rev, txn, subpool));
svn_pool_clear(subpool);
SVN_ERR(svn_fs_begin_txn(&txn, fs, youngest_rev, subpool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, subpool));
SVN_ERR(svn_test__set_file_contents(txn_root, "B", "5", subpool));
SVN_ERR(svn_test__set_file_contents(txn_root, "C", "5", subpool));
SVN_ERR(svn_fs_commit_txn(NULL, &youngest_rev, txn, subpool));
svn_pool_clear(subpool);
SVN_ERR(svn_fs_begin_txn(&txn, fs, youngest_rev, subpool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, subpool));
SVN_ERR(svn_test__set_file_contents(txn_root, "B", "6", subpool));
SVN_ERR(svn_test__set_file_contents(txn_root, "C", "6", subpool));
SVN_ERR(svn_fs_revision_root(&rev_root, fs, 5, subpool));
SVN_ERR(svn_fs_copy(rev_root, "B", txn_root, "D", subpool));
SVN_ERR(svn_test__set_file_contents(txn_root, "D", "5", subpool));
SVN_ERR(svn_fs_commit_txn(NULL, &youngest_rev, txn, subpool));
svn_pool_clear(subpool);
SVN_ERR(svn_fs_begin_txn(&txn, fs, youngest_rev, subpool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, subpool));
SVN_ERR(svn_test__set_file_contents(txn_root, "D", "7", subpool));
SVN_ERR(svn_fs_make_file(txn_root, "E", subpool));
SVN_ERR(svn_test__set_file_contents(txn_root, "E", "7", subpool));
SVN_ERR(svn_fs_commit_txn(NULL, &youngest_rev, txn, subpool));
svn_pool_clear(subpool);
SVN_ERR(svn_fs_begin_txn(&txn, fs, youngest_rev, subpool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, subpool));
SVN_ERR(svn_test__set_file_contents(txn_root, "E", "8", subpool));
SVN_ERR(svn_fs_commit_txn(NULL, &youngest_rev, txn, subpool));
svn_pool_clear(subpool);
SVN_ERR(svn_fs_begin_txn(&txn, fs, youngest_rev, subpool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, subpool));
SVN_ERR(svn_fs_revision_root(&rev_root, fs, 8, subpool));
SVN_ERR(svn_fs_copy(rev_root, "E", txn_root, "F", subpool));
SVN_ERR(svn_test__set_file_contents(txn_root, "F", "9", subpool));
SVN_ERR(svn_fs_commit_txn(NULL, &youngest_rev, txn, subpool));
svn_pool_clear(subpool);
SVN_ERR(svn_fs_begin_txn(&txn, fs, youngest_rev, subpool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, subpool));
SVN_ERR(svn_test__set_file_contents(txn_root, "F", "10", subpool));
SVN_ERR(svn_fs_commit_txn(NULL, &youngest_rev, txn, subpool));
svn_pool_clear(subpool);
{
int i, j;
struct path_rev_t {
const char *path;
svn_revnum_t rev;
};
struct path_rev_t path_revs[16] = {
{ "A", 1 }, { "A", 2 }, { "A", 3 }, { "A", 4 },
{ "B", 4 }, { "B", 5 }, { "B", 6 }, { "C", 4 },
{ "C", 5 }, { "C", 6 }, { "D", 6 }, { "D", 7 },
{ "E", 7 }, { "E", 8 }, { "F", 9 }, { "F", 10 }
};
int related_matrix[16][16] = {
{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0 },
{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0 },
{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0 },
{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0 },
{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0 },
{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0 },
{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0 },
{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0 },
{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0 },
{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0 },
{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0 },
{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0 },
{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1 },
{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1 },
{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1 },
{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1 }
};
for (i = 0; i < 16; i++) {
for (j = 0; j < 16; j++) {
struct path_rev_t pr1 = path_revs[i];
struct path_rev_t pr2 = path_revs[j];
const svn_fs_id_t *id1, *id2;
int related = 0;
SVN_ERR(svn_fs_revision_root(&rev_root, fs, pr1.rev, pool));
SVN_ERR(svn_fs_node_id(&id1, rev_root, pr1.path, pool));
SVN_ERR(svn_fs_revision_root(&rev_root, fs, pr2.rev, pool));
SVN_ERR(svn_fs_node_id(&id2, rev_root, pr2.path, pool));
related = svn_fs_check_related(id1, id2) ? 1 : 0;
if (related == related_matrix[i][j]) {
} else if (related && (! related_matrix[i][j])) {
return svn_error_createf
(SVN_ERR_TEST_FAILED, NULL,
"expected '%s:%d' to be related to '%s:%d'; it was not",
pr1.path, (int)pr1.rev, pr2.path, (int)pr2.rev);
} else if ((! related) && related_matrix[i][j]) {
return svn_error_createf
(SVN_ERR_TEST_FAILED, NULL,
"expected '%s:%d' to not be related to '%s:%d'; it was",
pr1.path, (int)pr1.rev, pr2.path, (int)pr2.rev);
}
svn_pool_clear(subpool);
}
}
}
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
static svn_error_t *
branch_test(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
apr_pool_t *spool = svn_pool_create(pool);
svn_fs_t *fs;
svn_fs_txn_t *txn;
svn_fs_root_t *txn_root, *rev_root;
svn_revnum_t youngest_rev = 0;
*msg = "test complex copies (branches)";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(svn_test__create_fs(&fs, "test-repo-branch-test",
opts->fs_type, pool));
SVN_ERR(svn_fs_begin_txn(&txn, fs, youngest_rev, spool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, spool));
SVN_ERR(svn_test__create_greek_tree(txn_root, spool));
SVN_ERR(svn_fs_commit_txn(NULL, &youngest_rev, txn, spool));
svn_pool_clear(spool);
SVN_ERR(svn_fs_begin_txn(&txn, fs, youngest_rev, spool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, spool));
SVN_ERR(svn_fs_revision_root(&rev_root, fs, youngest_rev, spool));
SVN_ERR(svn_fs_copy(rev_root, "A/D/G/rho", txn_root, "A/D/G/rho2", spool));
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
SVN_ERR(svn_fs_commit_txn(NULL, &youngest_rev, txn, spool));
svn_pool_clear(spool);
SVN_ERR(svn_fs_begin_txn(&txn, fs, youngest_rev, spool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, spool));
SVN_ERR(svn_fs_revision_root(&rev_root, fs, youngest_rev, spool));
SVN_ERR(svn_test__set_file_contents(txn_root, "A/D/G/rho",
"Edited text.", spool));
SVN_ERR(svn_test__set_file_contents(txn_root, "A/D/G/rho2",
"Edited text.", spool));
SVN_ERR(svn_test__set_file_contents(txn_root, "A/D/G2/rho",
"Edited text.", spool));
SVN_ERR(svn_test__set_file_contents(txn_root, "A/D/G2/rho2",
"Edited text.", spool));
SVN_ERR(svn_test__set_file_contents(txn_root, "A/D2/G/rho",
"Edited text.", spool));
SVN_ERR(svn_test__set_file_contents(txn_root, "A/D2/G/rho2",
"Edited text.", spool));
SVN_ERR(svn_test__set_file_contents(txn_root, "A/D2/G2/rho",
"Edited text.", spool));
SVN_ERR(svn_test__set_file_contents(txn_root, "A/D2/G2/rho2",
"Edited text.", spool));
SVN_ERR(svn_fs_commit_txn(NULL, &youngest_rev, txn, spool));
svn_pool_destroy(spool);
return SVN_NO_ERROR;
}
static svn_error_t *
verify_checksum(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
svn_fs_t *fs;
svn_fs_txn_t *txn;
svn_fs_root_t *txn_root;
svn_stringbuf_t *str;
unsigned char expected_digest[APR_MD5_DIGESTSIZE];
unsigned char actual_digest[APR_MD5_DIGESTSIZE];
*msg = "test checksums";
if (msg_only)
return SVN_NO_ERROR;
str = svn_stringbuf_create("My text editor charges me rent.", pool);
apr_md5(expected_digest, str->data, str->len);
SVN_ERR(svn_test__create_fs(&fs, "test-repo-verify-checksum",
opts->fs_type, pool));
SVN_ERR(svn_fs_begin_txn(&txn, fs, 0, pool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, pool));
SVN_ERR(svn_fs_make_file(txn_root, "fact", pool));
SVN_ERR(svn_test__set_file_contents(txn_root, "fact", str->data, pool));
SVN_ERR(svn_fs_file_md5_checksum(actual_digest, txn_root, "fact", pool));
if (memcmp(expected_digest, actual_digest, APR_MD5_DIGESTSIZE) != 0)
return svn_error_createf
(SVN_ERR_FS_GENERAL, NULL,
"verify-checksum: checksum mismatch:\n"
" expected: %s\n"
" actual: %s\n",
svn_md5_digest_to_cstring(expected_digest, pool),
svn_md5_digest_to_cstring(actual_digest, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
test_closest_copy_pair(svn_fs_root_t *closest_root,
const char *closest_path,
svn_revnum_t expected_revision,
const char *expected_path) {
svn_revnum_t closest_rev = SVN_INVALID_REVNUM;
assert(((! expected_path) && (! SVN_IS_VALID_REVNUM(expected_revision)))
|| (expected_path && SVN_IS_VALID_REVNUM(expected_revision)));
if (closest_path && (! closest_root))
return svn_error_create(SVN_ERR_FS_GENERAL, NULL,
"got closest path but no closest root");
if ((! closest_path) && closest_root)
return svn_error_create(SVN_ERR_FS_GENERAL, NULL,
"got closest root but no closest path");
if (closest_path && (! expected_path))
return svn_error_createf(SVN_ERR_FS_GENERAL, NULL,
"got closest path ('%s') when none expected",
closest_path);
if ((! closest_path) && expected_path)
return svn_error_createf(SVN_ERR_FS_GENERAL, NULL,
"got no closest path; expected '%s'",
expected_path);
if (closest_path && (strcmp(closest_path, expected_path) != 0))
return svn_error_createf(SVN_ERR_FS_GENERAL, NULL,
"got a different closest path than expected:\n"
" expected: %s\n"
" actual: %s",
expected_path, closest_path);
if (closest_root)
closest_rev = svn_fs_revision_root_revision(closest_root);
if (closest_rev != expected_revision)
return svn_error_createf(SVN_ERR_FS_GENERAL, NULL,
"got a different closest rev than expected:\n"
" expected: %ld\n"
" actual: %ld",
expected_revision, closest_rev);
return SVN_NO_ERROR;
}
static svn_error_t *
closest_copy_test(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
svn_fs_t *fs;
svn_fs_txn_t *txn;
svn_fs_root_t *txn_root, *rev_root, *croot;
svn_revnum_t after_rev;
const char *cpath;
apr_pool_t *spool = svn_pool_create(pool);
*msg = "calculating closest history-affecting copies";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(svn_test__create_fs(&fs, "test-repo-closest-copy",
opts->fs_type, pool));
SVN_ERR(svn_fs_begin_txn(&txn, fs, 0, spool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, spool));
SVN_ERR(svn_test__create_greek_tree(txn_root, spool));
SVN_ERR(test_commit_txn(&after_rev, txn, NULL, spool));
SVN_ERR(svn_fs_revision_root(&rev_root, fs, after_rev, spool));
SVN_ERR(svn_fs_begin_txn(&txn, fs, after_rev, spool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, spool));
SVN_ERR(svn_fs_copy(rev_root, "A", txn_root, "Z", spool));
SVN_ERR(test_commit_txn(&after_rev, txn, NULL, spool));
SVN_ERR(svn_fs_revision_root(&rev_root, fs, after_rev, spool));
SVN_ERR(svn_fs_closest_copy(&croot, &cpath, rev_root, "Z", spool));
SVN_ERR(test_closest_copy_pair(croot, cpath, 2, "/Z"));
SVN_ERR(svn_fs_closest_copy(&croot, &cpath, rev_root, "Z/D/G", spool));
SVN_ERR(test_closest_copy_pair(croot, cpath, 2, "/Z"));
SVN_ERR(svn_fs_closest_copy(&croot, &cpath, rev_root, "Z/mu", spool));
SVN_ERR(test_closest_copy_pair(croot, cpath, 2, "/Z"));
SVN_ERR(svn_fs_closest_copy(&croot, &cpath, rev_root, "Z/B/E/beta", spool));
SVN_ERR(test_closest_copy_pair(croot, cpath, 2, "/Z"));
SVN_ERR(svn_fs_closest_copy(&croot, &cpath, rev_root, "A", spool));
SVN_ERR(test_closest_copy_pair(croot, cpath, SVN_INVALID_REVNUM, NULL));
SVN_ERR(svn_fs_closest_copy(&croot, &cpath, rev_root, "A/D/G", spool));
SVN_ERR(test_closest_copy_pair(croot, cpath, SVN_INVALID_REVNUM, NULL));
SVN_ERR(svn_fs_closest_copy(&croot, &cpath, rev_root, "A/mu", spool));
SVN_ERR(test_closest_copy_pair(croot, cpath, SVN_INVALID_REVNUM, NULL));
SVN_ERR(svn_fs_closest_copy(&croot, &cpath, rev_root, "A/B/E/beta", spool));
SVN_ERR(test_closest_copy_pair(croot, cpath, SVN_INVALID_REVNUM, NULL));
SVN_ERR(svn_fs_begin_txn(&txn, fs, after_rev, spool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, spool));
SVN_ERR(svn_test__set_file_contents(txn_root, "Z/mu",
"Edited text.", spool));
SVN_ERR(svn_fs_copy(rev_root, "A", txn_root, "Z2", spool));
SVN_ERR(svn_fs_copy(rev_root, "A/D/H", txn_root, "Z2/D/H2", spool));
SVN_ERR(svn_test__set_file_contents(txn_root, "Z2/D/H2/chi",
"Edited text.", spool));
SVN_ERR(svn_fs_make_file(txn_root, "Z/t", pool));
SVN_ERR(svn_fs_make_file(txn_root, "Z2/D/H2/t", pool));
SVN_ERR(test_commit_txn(&after_rev, txn, NULL, spool));
SVN_ERR(svn_fs_revision_root(&rev_root, fs, after_rev, spool));
SVN_ERR(svn_fs_begin_txn(&txn, fs, after_rev, spool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, spool));
SVN_ERR(svn_test__set_file_contents(txn_root, "Z2/D/H2/t",
"Edited text.", spool));
SVN_ERR(test_commit_txn(&after_rev, txn, NULL, spool));
SVN_ERR(svn_fs_revision_root(&rev_root, fs, after_rev, spool));
SVN_ERR(svn_fs_closest_copy(&croot, &cpath, rev_root, "A/mu", spool));
SVN_ERR(test_closest_copy_pair(croot, cpath, SVN_INVALID_REVNUM, NULL));
SVN_ERR(svn_fs_closest_copy(&croot, &cpath, rev_root, "Z/mu", spool));
SVN_ERR(test_closest_copy_pair(croot, cpath, 2, "/Z"));
SVN_ERR(svn_fs_closest_copy(&croot, &cpath, rev_root, "Z2/D/H2", spool));
SVN_ERR(test_closest_copy_pair(croot, cpath, 3, "/Z2/D/H2"));
SVN_ERR(svn_fs_closest_copy(&croot, &cpath, rev_root, "Z2/D", spool));
SVN_ERR(test_closest_copy_pair(croot, cpath, 3, "/Z2"));
SVN_ERR(svn_fs_closest_copy(&croot, &cpath, rev_root, "Z/t", spool));
SVN_ERR(test_closest_copy_pair(croot, cpath, SVN_INVALID_REVNUM, NULL));
SVN_ERR(svn_fs_closest_copy(&croot, &cpath, rev_root, "Z2/D/H2/t", spool));
SVN_ERR(test_closest_copy_pair(croot, cpath, SVN_INVALID_REVNUM, NULL));
return SVN_NO_ERROR;
}
static svn_error_t *
root_revisions(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
svn_fs_t *fs;
svn_fs_txn_t *txn;
svn_fs_root_t *txn_root, *rev_root;
svn_revnum_t after_rev, fetched_rev;
apr_pool_t *spool = svn_pool_create(pool);
*msg = "svn_fs_root_t (base) revisions";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(svn_test__create_fs(&fs, "test-repo-root-revisions",
opts->fs_type, pool));
SVN_ERR(svn_fs_begin_txn(&txn, fs, 0, spool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, spool));
SVN_ERR(svn_test__create_greek_tree(txn_root, spool));
SVN_ERR(test_commit_txn(&after_rev, txn, NULL, spool));
SVN_ERR(svn_fs_revision_root(&rev_root, fs, after_rev, spool));
fetched_rev = svn_fs_revision_root_revision(rev_root);
if (after_rev != fetched_rev)
return svn_error_createf
(SVN_ERR_TEST_FAILED, NULL,
"expected revision '%d'; "
"got '%d' from svn_fs_revision_root_revision(rev_root)",
(int)after_rev, (int)fetched_rev);
fetched_rev = svn_fs_txn_root_base_revision(rev_root);
if (fetched_rev != SVN_INVALID_REVNUM)
return svn_error_createf
(SVN_ERR_TEST_FAILED, NULL,
"expected SVN_INVALID_REVNUM; "
"got '%d' from svn_fs_txn_root_base_revision(rev_root)",
(int)fetched_rev);
SVN_ERR(svn_fs_begin_txn(&txn, fs, after_rev, spool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, spool));
fetched_rev = svn_fs_txn_root_base_revision(txn_root);
if (after_rev != fetched_rev)
return svn_error_createf
(SVN_ERR_TEST_FAILED, NULL,
"expected '%d'; "
"got '%d' from svn_fs_txn_root_base_revision(txn_root)",
(int)after_rev, (int)fetched_rev);
fetched_rev = svn_fs_revision_root_revision(txn_root);
if (fetched_rev != SVN_INVALID_REVNUM)
return svn_error_createf
(SVN_ERR_TEST_FAILED, NULL,
"expected SVN_INVALID_REVNUM; "
"got '%d' from svn_fs_revision_root_revision(txn_root)",
(int)fetched_rev);
return SVN_NO_ERROR;
}
static svn_error_t *
unordered_txn_dirprops(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
svn_fs_t *fs;
svn_fs_txn_t *txn, *txn2;
svn_fs_root_t *txn_root, *txn_root2;
svn_string_t pval;
svn_revnum_t new_rev, not_rev;
*msg = "test dir prop preservation in unordered txns";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(svn_test__create_fs(&fs, "test-repo-unordered-txn-dirprops",
opts->fs_type, pool));
SVN_ERR(svn_fs_begin_txn(&txn, fs, 0, pool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, pool));
SVN_ERR(svn_test__create_greek_tree(txn_root, pool));
SVN_ERR(test_commit_txn(&new_rev, txn, NULL, pool));
SVN_ERR(svn_fs_begin_txn(&txn, fs, new_rev, pool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, pool));
SVN_ERR(svn_fs_begin_txn(&txn2, fs, new_rev, pool));
SVN_ERR(svn_fs_txn_root(&txn_root2, txn2, pool));
SVN_ERR(svn_test__set_file_contents(txn_root, "/A/B/E/alpha",
"New contents", pool));
SET_STR(&pval, "/A/C:1");
SVN_ERR(svn_fs_change_node_prop(txn_root2, "/A/B", "svn:mergeinfo",
&pval, pool));
SVN_ERR(test_commit_txn(&new_rev, txn2, NULL, pool));
SVN_ERR(test_commit_txn(&not_rev, txn, "/A/B", pool));
SVN_ERR(svn_fs_abort_txn(txn, pool));
SVN_ERR(svn_fs_begin_txn(&txn, fs, new_rev, pool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, pool));
SVN_ERR(svn_fs_begin_txn(&txn2, fs, new_rev, pool));
SVN_ERR(svn_fs_txn_root(&txn_root2, txn2, pool));
SVN_ERR(svn_test__set_file_contents(txn_root, "/A/B/E/alpha",
"New contents", pool));
SET_STR(&pval, "/A/C:1");
SVN_ERR(svn_fs_change_node_prop(txn_root2, "/A/B", "svn:mergeinfo",
&pval, pool));
SVN_ERR(test_commit_txn(&new_rev, txn, NULL, pool));
SVN_ERR(test_commit_txn(&not_rev, txn2, "/A/B", pool));
SVN_ERR(svn_fs_abort_txn(txn2, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
set_uuid(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
svn_fs_t *fs;
const char *fixed_uuid = svn_uuid_generate(pool);
const char *fetched_uuid;
*msg = "test svn_fs_set_uuid";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(svn_test__create_fs(&fs, "test-repo-set-uuid",
opts->fs_type, pool));
SVN_ERR(svn_fs_set_uuid(fs, fixed_uuid, pool));
SVN_ERR(svn_fs_get_uuid(fs, &fetched_uuid, pool));
if (strcmp(fixed_uuid, fetched_uuid) != 0)
return svn_error_createf
(SVN_ERR_TEST_FAILED, NULL, "expected UUID '%s'; got '%s'",
fixed_uuid, fetched_uuid);
SVN_ERR(svn_fs_set_uuid(fs, NULL, pool));
SVN_ERR(svn_fs_get_uuid(fs, &fetched_uuid, pool));
if (strcmp(fixed_uuid, fetched_uuid) == 0)
return svn_error_createf
(SVN_ERR_TEST_FAILED, NULL,
"expected something other than UUID '%s', but got that one",
fixed_uuid);
return SVN_NO_ERROR;
}
static svn_error_t *
node_origin_rev(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
apr_pool_t *subpool = svn_pool_create(pool);
svn_fs_t *fs;
svn_fs_txn_t *txn;
svn_fs_root_t *txn_root, *root;
svn_revnum_t youngest_rev = 0;
int i;
struct path_rev_t {
const char *path;
svn_revnum_t rev;
};
*msg = "test svn_fs_node_origin_rev";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(svn_test__create_fs(&fs, "test-repo-node-origin-rev",
opts->fs_type, pool));
SVN_ERR(svn_fs_begin_txn(&txn, fs, 0, subpool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, subpool));
SVN_ERR(svn_test__create_greek_tree(txn_root, subpool));
SVN_ERR(svn_fs_commit_txn(NULL, &youngest_rev, txn, subpool));
svn_pool_clear(subpool);
SVN_ERR(svn_fs_begin_txn(&txn, fs, youngest_rev, subpool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, subpool));
SVN_ERR(svn_test__set_file_contents(txn_root, "A/D/H/chi", "2", subpool));
SVN_ERR(svn_test__set_file_contents(txn_root, "A/B/E/alpha", "2", subpool));
SVN_ERR(svn_fs_commit_txn(NULL, &youngest_rev, txn, subpool));
svn_pool_clear(subpool);
SVN_ERR(svn_fs_begin_txn(&txn, fs, youngest_rev, subpool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, subpool));
SVN_ERR(svn_fs_revision_root(&root, fs, youngest_rev, subpool));
SVN_ERR(svn_fs_copy(root, "A/D", txn_root, "A/D2", subpool));
SVN_ERR(svn_fs_make_file(txn_root, "A/D2/floop", subpool));
SVN_ERR(svn_fs_commit_txn(NULL, &youngest_rev, txn, subpool));
svn_pool_clear(subpool);
SVN_ERR(svn_fs_begin_txn(&txn, fs, youngest_rev, subpool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, subpool));
SVN_ERR(svn_test__set_file_contents(txn_root, "A/D/H/chi", "4", subpool));
SVN_ERR(svn_test__set_file_contents(txn_root, "A/D2/H/chi", "4", subpool));
SVN_ERR(svn_fs_commit_txn(NULL, &youngest_rev, txn, subpool));
svn_pool_clear(subpool);
SVN_ERR(svn_fs_begin_txn(&txn, fs, youngest_rev, subpool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, subpool));
SVN_ERR(svn_fs_delete(txn_root, "A/D2/G", subpool));
SVN_ERR(svn_fs_make_file(txn_root, "A/B/E/alfalfa", subpool));
SVN_ERR(svn_fs_commit_txn(NULL, &youngest_rev, txn, subpool));
svn_pool_clear(subpool);
SVN_ERR(svn_fs_begin_txn(&txn, fs, youngest_rev, subpool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, subpool));
SVN_ERR(svn_fs_revision_root(&root, fs, 4, subpool));
SVN_ERR(svn_fs_copy(root, "A/D2/G", txn_root, "A/D2/G", subpool));
SVN_ERR(svn_fs_commit_txn(NULL, &youngest_rev, txn, subpool));
svn_pool_clear(subpool);
SVN_ERR(svn_fs_begin_txn(&txn, fs, youngest_rev, subpool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, subpool));
SVN_ERR(svn_fs_revision_root(&root, fs, youngest_rev, subpool));
SVN_ERR(svn_fs_delete(txn_root, "A/D", subpool));
SVN_ERR(svn_fs_copy(root, "A/D2", txn_root, "A/D", subpool));
SVN_ERR(svn_fs_delete(txn_root, "A/D2", subpool));
SVN_ERR(svn_test__set_file_contents(txn_root, "A/D/floop", "7", subpool));
SVN_ERR(svn_fs_commit_txn(NULL, &youngest_rev, txn, subpool));
svn_pool_clear(subpool);
{
struct path_rev_t pathrevs[4] = { { "A/D", 1 },
{ "A/D/floop", 3 },
{ "iota", 1 },
{ "A/B/E/alfalfa", 5 }
};
SVN_ERR(svn_fs_revision_root(&root, fs, youngest_rev, pool));
for (i = 0; i < (sizeof(pathrevs) / sizeof(struct path_rev_t)); i++) {
struct path_rev_t path_rev = pathrevs[i];
svn_revnum_t revision;
SVN_ERR(svn_fs_node_origin_rev(&revision, root, path_rev.path, pool));
if (path_rev.rev != revision)
return svn_error_createf
(SVN_ERR_TEST_FAILED, NULL,
"expected origin revision of '%ld' for '%s'; got '%ld'",
path_rev.rev, path_rev.path, revision);
}
}
SVN_ERR(svn_fs_begin_txn(&txn, fs, youngest_rev, subpool));
SVN_ERR(svn_fs_txn_root(&txn_root, txn, subpool));
SVN_ERR(svn_fs_make_file(txn_root, "bloop", subpool));
SVN_ERR(svn_fs_make_dir(txn_root, "A/D/blarp", subpool));
{
struct path_rev_t pathrevs[6] = { { "A/D", 1 },
{ "A/D/floop", 3 },
{ "bloop", -1 },
{ "A/D/blarp", -1 },
{ "iota", 1 },
{ "A/B/E/alfalfa", 5 }
};
root = txn_root;
for (i = 0; i < (sizeof(pathrevs) / sizeof(struct path_rev_t)); i++) {
struct path_rev_t path_rev = pathrevs[i];
svn_revnum_t revision;
SVN_ERR(svn_fs_node_origin_rev(&revision, root, path_rev.path, pool));
if (! SVN_IS_VALID_REVNUM(revision))
revision = -1;
if (path_rev.rev != revision)
return svn_error_createf
(SVN_ERR_TEST_FAILED, NULL,
"expected origin revision of '%ld' for '%s'; got '%ld'",
path_rev.rev, path_rev.path, revision);
}
}
return SVN_NO_ERROR;
}
struct svn_test_descriptor_t test_funcs[] = {
SVN_TEST_NULL,
SVN_TEST_PASS(trivial_transaction),
SVN_TEST_PASS(reopen_trivial_transaction),
SVN_TEST_PASS(create_file_transaction),
SVN_TEST_PASS(verify_txn_list),
SVN_TEST_PASS(txn_names_are_not_reused),
SVN_TEST_PASS(write_and_read_file),
SVN_TEST_PASS(create_mini_tree_transaction),
SVN_TEST_PASS(create_greek_tree_transaction),
SVN_TEST_PASS(list_directory),
SVN_TEST_PASS(revision_props),
SVN_TEST_PASS(transaction_props),
SVN_TEST_PASS(node_props),
SVN_TEST_PASS(delete_mutables),
SVN_TEST_PASS(delete),
SVN_TEST_PASS(fetch_youngest_rev),
SVN_TEST_PASS(basic_commit),
SVN_TEST_PASS(test_tree_node_validation),
SVN_TEST_XFAIL(merging_commit),
SVN_TEST_PASS(copy_test),
SVN_TEST_PASS(commit_date),
SVN_TEST_PASS(check_old_revisions),
SVN_TEST_PASS(check_all_revisions),
SVN_TEST_PASS(medium_file_integrity),
SVN_TEST_PASS(large_file_integrity),
SVN_TEST_PASS(check_root_revision),
SVN_TEST_PASS(test_node_created_rev),
SVN_TEST_PASS(check_related),
SVN_TEST_PASS(branch_test),
SVN_TEST_PASS(verify_checksum),
SVN_TEST_PASS(closest_copy_test),
SVN_TEST_PASS(root_revisions),
SVN_TEST_PASS(unordered_txn_dirprops),
SVN_TEST_PASS(set_uuid),
SVN_TEST_PASS(node_origin_rev),
SVN_TEST_NULL
};
