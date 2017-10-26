#include <string.h>
#include <assert.h>
#include "svn_path.h"
#include "svn_time.h"
#include "svn_error.h"
#include "svn_md5.h"
#include "svn_fs.h"
#include "svn_hash.h"
#include "svn_props.h"
#include "dag.h"
#include "err.h"
#include "fs.h"
#include "key-gen.h"
#include "node-rev.h"
#include "trail.h"
#include "reps-strings.h"
#include "revs-txns.h"
#include "id.h"
#include "util/fs_skels.h"
#include "bdb/txn-table.h"
#include "bdb/rev-table.h"
#include "bdb/nodes-table.h"
#include "bdb/copies-table.h"
#include "bdb/reps-table.h"
#include "bdb/strings-table.h"
#include "private/svn_fs_util.h"
#include "../libsvn_fs/fs-loader.h"
#include "svn_private_config.h"
struct dag_node_t {
svn_fs_t *fs;
svn_fs_id_t *id;
svn_node_kind_t kind;
const char *created_path;
};
svn_node_kind_t svn_fs_base__dag_node_kind(dag_node_t *node) {
return node->kind;
}
const svn_fs_id_t *
svn_fs_base__dag_get_id(dag_node_t *node) {
return node->id;
}
const char *
svn_fs_base__dag_get_created_path(dag_node_t *node) {
return node->created_path;
}
svn_fs_t *
svn_fs_base__dag_get_fs(dag_node_t *node) {
return node->fs;
}
svn_boolean_t svn_fs_base__dag_check_mutable(dag_node_t *node,
const char *txn_id) {
return (strcmp(svn_fs_base__id_txn_id(svn_fs_base__dag_get_id(node)),
txn_id) == 0);
}
svn_error_t *
svn_fs_base__dag_get_node(dag_node_t **node,
svn_fs_t *fs,
const svn_fs_id_t *id,
trail_t *trail,
apr_pool_t *pool) {
dag_node_t *new_node;
node_revision_t *noderev;
new_node = apr_pcalloc(pool, sizeof(*new_node));
new_node->fs = fs;
new_node->id = svn_fs_base__id_copy(id, pool);
SVN_ERR(svn_fs_bdb__get_node_revision(&noderev, fs, id, trail, pool));
new_node->kind = noderev->kind;
new_node->created_path = noderev->created_path;
*node = new_node;
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_base__dag_get_revision(svn_revnum_t *rev,
dag_node_t *node,
trail_t *trail,
apr_pool_t *pool) {
return svn_fs_base__txn_get_revision
(rev, svn_fs_base__dag_get_fs(node),
svn_fs_base__id_txn_id(svn_fs_base__dag_get_id(node)), trail, pool);
}
svn_error_t *
svn_fs_base__dag_get_predecessor_id(const svn_fs_id_t **id_p,
dag_node_t *node,
trail_t *trail,
apr_pool_t *pool) {
node_revision_t *noderev;
SVN_ERR(svn_fs_bdb__get_node_revision(&noderev, node->fs, node->id,
trail, pool));
*id_p = noderev->predecessor_id;
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_base__dag_get_predecessor_count(int *count,
dag_node_t *node,
trail_t *trail,
apr_pool_t *pool) {
node_revision_t *noderev;
SVN_ERR(svn_fs_bdb__get_node_revision(&noderev, node->fs, node->id,
trail, pool));
*count = noderev->predecessor_count;
return SVN_NO_ERROR;
}
static svn_error_t *
txn_body_dag_init_fs(void *baton,
trail_t *trail) {
node_revision_t noderev;
revision_t revision;
svn_revnum_t rev = SVN_INVALID_REVNUM;
svn_fs_t *fs = trail->fs;
svn_string_t date;
const char *txn_id;
const char *copy_id;
svn_fs_id_t *root_id = svn_fs_base__id_create("0", "0", "0", trail->pool);
memset(&noderev, 0, sizeof(noderev));
noderev.kind = svn_node_dir;
noderev.created_path = "/";
SVN_ERR(svn_fs_bdb__put_node_revision(fs, root_id, &noderev,
trail, trail->pool));
SVN_ERR(svn_fs_bdb__create_txn(&txn_id, fs, root_id, trail, trail->pool));
if (strcmp(txn_id, "0"))
return svn_error_createf
(SVN_ERR_FS_CORRUPT, 0,
_("Corrupt DB: initial transaction id not '0' in filesystem '%s'"),
fs->path);
SVN_ERR(svn_fs_bdb__reserve_copy_id(&copy_id, fs, trail, trail->pool));
if (strcmp(copy_id, "0"))
return svn_error_createf
(SVN_ERR_FS_CORRUPT, 0,
_("Corrupt DB: initial copy id not '0' in filesystem '%s'"), fs->path);
SVN_ERR(svn_fs_bdb__create_copy(fs, copy_id, NULL, NULL, root_id,
copy_kind_real, trail, trail->pool));
revision.txn_id = txn_id;
SVN_ERR(svn_fs_bdb__put_rev(&rev, fs, &revision, trail, trail->pool));
if (rev != 0)
return svn_error_createf(SVN_ERR_FS_CORRUPT, 0,
_("Corrupt DB: initial revision number "
"is not '0' in filesystem '%s'"), fs->path);
SVN_ERR(svn_fs_base__txn_make_committed(fs, txn_id, rev,
trail, trail->pool));
date.data = svn_time_to_cstring(apr_time_now(), trail->pool);
date.len = strlen(date.data);
return svn_fs_base__set_rev_prop(fs, 0, SVN_PROP_REVISION_DATE, &date,
trail, trail->pool);
}
svn_error_t *
svn_fs_base__dag_init_fs(svn_fs_t *fs) {
return svn_fs_base__retry_txn(fs, txn_body_dag_init_fs, NULL, fs->pool);
}
static svn_error_t *
get_dir_entries(apr_hash_t **entries_p,
svn_fs_t *fs,
node_revision_t *noderev,
trail_t *trail,
apr_pool_t *pool) {
apr_hash_t *entries = apr_hash_make(pool);
apr_hash_index_t *hi;
svn_string_t entries_raw;
skel_t *entries_skel;
if (noderev->kind != svn_node_dir)
return svn_error_create
(SVN_ERR_FS_NOT_DIRECTORY, NULL,
_("Attempted to create entry in non-directory parent"));
if (noderev->data_key) {
SVN_ERR(svn_fs_base__rep_contents(&entries_raw, fs, noderev->data_key,
trail, pool));
entries_skel = svn_fs_base__parse_skel(entries_raw.data,
entries_raw.len, pool);
if (entries_skel)
SVN_ERR(svn_fs_base__parse_entries_skel(&entries, entries_skel,
pool));
}
*entries_p = NULL;
if (! entries)
return SVN_NO_ERROR;
*entries_p = apr_hash_make(pool);
for (hi = apr_hash_first(pool, entries); hi; hi = apr_hash_next(hi)) {
const void *key;
apr_ssize_t klen;
void *val;
svn_fs_dirent_t *dirent = apr_palloc(pool, sizeof(*dirent));
apr_hash_this(hi, &key, &klen, &val);
dirent->name = key;
dirent->id = val;
dirent->kind = svn_node_unknown;
apr_hash_set(*entries_p, key, klen, dirent);
}
return SVN_NO_ERROR;
}
static svn_error_t *
dir_entry_id_from_node(const svn_fs_id_t **id_p,
dag_node_t *parent,
const char *name,
trail_t *trail,
apr_pool_t *pool) {
apr_hash_t *entries;
svn_fs_dirent_t *dirent;
SVN_ERR(svn_fs_base__dag_dir_entries(&entries, parent, trail, pool));
if (entries)
dirent = apr_hash_get(entries, name, APR_HASH_KEY_STRING);
else
dirent = NULL;
*id_p = dirent ? dirent->id : NULL;
return SVN_NO_ERROR;
}
static svn_error_t *
set_entry(dag_node_t *parent,
const char *name,
const svn_fs_id_t *id,
const char *txn_id,
trail_t *trail,
apr_pool_t *pool) {
node_revision_t *parent_noderev;
const char *rep_key, *mutable_rep_key;
apr_hash_t *entries = NULL;
svn_stream_t *wstream;
apr_size_t len;
svn_string_t raw_entries;
svn_stringbuf_t *raw_entries_buf;
skel_t *entries_skel;
svn_fs_t *fs = svn_fs_base__dag_get_fs(parent);
SVN_ERR(svn_fs_bdb__get_node_revision(&parent_noderev, fs, parent->id,
trail, pool));
rep_key = parent_noderev->data_key;
SVN_ERR(svn_fs_base__get_mutable_rep(&mutable_rep_key, rep_key,
fs, txn_id, trail, pool));
if (! svn_fs_base__same_keys(rep_key, mutable_rep_key)) {
parent_noderev->data_key = mutable_rep_key;
SVN_ERR(svn_fs_bdb__put_node_revision(fs, parent->id, parent_noderev,
trail, pool));
}
if (rep_key) {
SVN_ERR(svn_fs_base__rep_contents(&raw_entries, fs, rep_key,
trail, pool));
entries_skel = svn_fs_base__parse_skel(raw_entries.data,
raw_entries.len, pool);
if (entries_skel)
SVN_ERR(svn_fs_base__parse_entries_skel(&entries, entries_skel,
pool));
}
if (! entries)
entries = apr_hash_make(pool);
apr_hash_set(entries, name, APR_HASH_KEY_STRING, id);
SVN_ERR(svn_fs_base__unparse_entries_skel(&entries_skel, entries,
pool));
raw_entries_buf = svn_fs_base__unparse_skel(entries_skel, pool);
SVN_ERR(svn_fs_base__rep_contents_write_stream(&wstream, fs,
mutable_rep_key, txn_id,
TRUE, trail, pool));
len = raw_entries_buf->len;
SVN_ERR(svn_stream_write(wstream, raw_entries_buf->data, &len));
SVN_ERR(svn_stream_close(wstream));
return SVN_NO_ERROR;
}
static svn_error_t *
make_entry(dag_node_t **child_p,
dag_node_t *parent,
const char *parent_path,
const char *name,
svn_boolean_t is_dir,
const char *txn_id,
trail_t *trail,
apr_pool_t *pool) {
const svn_fs_id_t *new_node_id;
node_revision_t new_noderev;
if (! svn_path_is_single_path_component(name))
return svn_error_createf
(SVN_ERR_FS_NOT_SINGLE_PATH_COMPONENT, NULL,
_("Attempted to create a node with an illegal name '%s'"), name);
if (parent->kind != svn_node_dir)
return svn_error_create
(SVN_ERR_FS_NOT_DIRECTORY, NULL,
_("Attempted to create entry in non-directory parent"));
if (! svn_fs_base__dag_check_mutable(parent, txn_id))
return svn_error_createf
(SVN_ERR_FS_NOT_MUTABLE, NULL,
_("Attempted to clone child of non-mutable node"));
SVN_ERR(dir_entry_id_from_node(&new_node_id, parent, name, trail, pool));
if (new_node_id)
return svn_error_createf
(SVN_ERR_FS_ALREADY_EXISTS, NULL,
_("Attempted to create entry that already exists"));
memset(&new_noderev, 0, sizeof(new_noderev));
new_noderev.kind = is_dir ? svn_node_dir : svn_node_file;
new_noderev.created_path = svn_path_join(parent_path, name, pool);
SVN_ERR(svn_fs_base__create_node
(&new_node_id, svn_fs_base__dag_get_fs(parent), &new_noderev,
svn_fs_base__id_copy_id(svn_fs_base__dag_get_id(parent)),
txn_id, trail, pool));
SVN_ERR(svn_fs_base__dag_get_node(child_p,
svn_fs_base__dag_get_fs(parent),
new_node_id, trail, pool));
SVN_ERR(set_entry(parent, name, svn_fs_base__dag_get_id(*child_p),
txn_id, trail, pool));
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_base__dag_dir_entries(apr_hash_t **entries,
dag_node_t *node,
trail_t *trail,
apr_pool_t *pool) {
node_revision_t *noderev;
SVN_ERR(svn_fs_bdb__get_node_revision(&noderev, node->fs, node->id,
trail, pool));
return get_dir_entries(entries, node->fs, noderev, trail, pool);
}
svn_error_t *
svn_fs_base__dag_set_entry(dag_node_t *node,
const char *entry_name,
const svn_fs_id_t *id,
const char *txn_id,
trail_t *trail,
apr_pool_t *pool) {
if (node->kind != svn_node_dir)
return svn_error_create
(SVN_ERR_FS_NOT_DIRECTORY, NULL,
_("Attempted to set entry in non-directory node"));
if (! svn_fs_base__dag_check_mutable(node, txn_id))
return svn_error_create
(SVN_ERR_FS_NOT_MUTABLE, NULL,
_("Attempted to set entry in immutable node"));
return set_entry(node, entry_name, id, txn_id, trail, pool);
}
svn_error_t *
svn_fs_base__dag_get_proplist(apr_hash_t **proplist_p,
dag_node_t *node,
trail_t *trail,
apr_pool_t *pool) {
node_revision_t *noderev;
apr_hash_t *proplist = NULL;
svn_string_t raw_proplist;
skel_t *proplist_skel;
SVN_ERR(svn_fs_bdb__get_node_revision(&noderev, node->fs, node->id,
trail, pool));
if (! noderev->prop_key) {
*proplist_p = NULL;
return SVN_NO_ERROR;
}
SVN_ERR(svn_fs_base__rep_contents(&raw_proplist,
svn_fs_base__dag_get_fs(node),
noderev->prop_key, trail, pool));
proplist_skel = svn_fs_base__parse_skel(raw_proplist.data,
raw_proplist.len, pool);
if (proplist_skel)
SVN_ERR(svn_fs_base__parse_proplist_skel(&proplist,
proplist_skel, pool));
*proplist_p = proplist;
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_base__dag_set_proplist(dag_node_t *node,
apr_hash_t *proplist,
const char *txn_id,
trail_t *trail,
apr_pool_t *pool) {
node_revision_t *noderev;
const char *rep_key, *mutable_rep_key;
svn_fs_t *fs = svn_fs_base__dag_get_fs(node);
if (! svn_fs_base__dag_check_mutable(node, txn_id)) {
svn_string_t *idstr = svn_fs_base__id_unparse(node->id, pool);
return svn_error_createf
(SVN_ERR_FS_NOT_MUTABLE, NULL,
_("Can't set proplist on *immutable* node-revision %s"),
idstr->data);
}
SVN_ERR(svn_fs_bdb__get_node_revision(&noderev, fs, node->id,
trail, pool));
rep_key = noderev->prop_key;
SVN_ERR(svn_fs_base__get_mutable_rep(&mutable_rep_key, rep_key,
fs, txn_id, trail, pool));
if (! svn_fs_base__same_keys(mutable_rep_key, rep_key)) {
noderev->prop_key = mutable_rep_key;
SVN_ERR(svn_fs_bdb__put_node_revision(fs, node->id, noderev,
trail, pool));
}
{
svn_stream_t *wstream;
apr_size_t len;
skel_t *proplist_skel;
svn_stringbuf_t *raw_proplist_buf;
SVN_ERR(svn_fs_base__unparse_proplist_skel(&proplist_skel,
proplist, pool));
raw_proplist_buf = svn_fs_base__unparse_skel(proplist_skel, pool);
SVN_ERR(svn_fs_base__rep_contents_write_stream(&wstream, fs,
mutable_rep_key, txn_id,
TRUE, trail, pool));
len = raw_proplist_buf->len;
SVN_ERR(svn_stream_write(wstream, raw_proplist_buf->data, &len));
SVN_ERR(svn_stream_close(wstream));
}
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_base__dag_revision_root(dag_node_t **node_p,
svn_fs_t *fs,
svn_revnum_t rev,
trail_t *trail,
apr_pool_t *pool) {
const svn_fs_id_t *root_id;
SVN_ERR(svn_fs_base__rev_get_root(&root_id, fs, rev, trail, pool));
return svn_fs_base__dag_get_node(node_p, fs, root_id, trail, pool);
}
svn_error_t *
svn_fs_base__dag_txn_root(dag_node_t **node_p,
svn_fs_t *fs,
const char *txn_id,
trail_t *trail,
apr_pool_t *pool) {
const svn_fs_id_t *root_id, *ignored;
SVN_ERR(svn_fs_base__get_txn_ids(&root_id, &ignored, fs, txn_id,
trail, pool));
return svn_fs_base__dag_get_node(node_p, fs, root_id, trail, pool);
}
svn_error_t *
svn_fs_base__dag_txn_base_root(dag_node_t **node_p,
svn_fs_t *fs,
const char *txn_id,
trail_t *trail,
apr_pool_t *pool) {
const svn_fs_id_t *base_root_id, *ignored;
SVN_ERR(svn_fs_base__get_txn_ids(&ignored, &base_root_id, fs, txn_id,
trail, pool));
return svn_fs_base__dag_get_node(node_p, fs, base_root_id, trail, pool);
}
svn_error_t *
svn_fs_base__dag_clone_child(dag_node_t **child_p,
dag_node_t *parent,
const char *parent_path,
const char *name,
const char *copy_id,
const char *txn_id,
trail_t *trail,
apr_pool_t *pool) {
dag_node_t *cur_entry;
const svn_fs_id_t *new_node_id;
svn_fs_t *fs = svn_fs_base__dag_get_fs(parent);
if (! svn_fs_base__dag_check_mutable(parent, txn_id))
return svn_error_createf
(SVN_ERR_FS_NOT_MUTABLE, NULL,
_("Attempted to clone child of non-mutable node"));
if (! svn_path_is_single_path_component(name))
return svn_error_createf
(SVN_ERR_FS_NOT_SINGLE_PATH_COMPONENT, NULL,
_("Attempted to make a child clone with an illegal name '%s'"), name);
SVN_ERR(svn_fs_base__dag_open(&cur_entry, parent, name, trail, pool));
if (svn_fs_base__dag_check_mutable(cur_entry, txn_id)) {
new_node_id = cur_entry->id;
} else {
node_revision_t *noderev;
SVN_ERR(svn_fs_bdb__get_node_revision(&noderev, fs, cur_entry->id,
trail, pool));
noderev->predecessor_id = cur_entry->id;
if (noderev->predecessor_count != -1)
noderev->predecessor_count++;
noderev->created_path = svn_path_join(parent_path, name, pool);
SVN_ERR(svn_fs_base__create_successor(&new_node_id, fs, cur_entry->id,
noderev, copy_id, txn_id,
trail, pool));
SVN_ERR(set_entry(parent, name, new_node_id, txn_id, trail, pool));
}
return svn_fs_base__dag_get_node(child_p, fs, new_node_id, trail, pool);
}
svn_error_t *
svn_fs_base__dag_clone_root(dag_node_t **root_p,
svn_fs_t *fs,
const char *txn_id,
trail_t *trail,
apr_pool_t *pool) {
const svn_fs_id_t *base_root_id, *root_id;
node_revision_t *noderev;
SVN_ERR(svn_fs_base__get_txn_ids(&root_id, &base_root_id, fs, txn_id,
trail, pool));
if (svn_fs_base__id_eq(root_id, base_root_id)) {
const char *base_copy_id = svn_fs_base__id_copy_id(base_root_id);
SVN_ERR(svn_fs_bdb__get_node_revision(&noderev, fs, base_root_id,
trail, pool));
noderev->predecessor_id = svn_fs_base__id_copy(base_root_id, pool);
if (noderev->predecessor_count != -1)
noderev->predecessor_count++;
SVN_ERR(svn_fs_base__create_successor(&root_id, fs, base_root_id,
noderev, base_copy_id,
txn_id, trail, pool));
SVN_ERR(svn_fs_base__set_txn_root(fs, txn_id, root_id, trail, pool));
}
SVN_ERR(svn_fs_base__dag_get_node(root_p, fs, root_id, trail, pool));
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_base__dag_delete(dag_node_t *parent,
const char *name,
const char *txn_id,
trail_t *trail,
apr_pool_t *pool) {
node_revision_t *parent_noderev;
const char *rep_key, *mutable_rep_key;
apr_hash_t *entries = NULL;
skel_t *entries_skel;
svn_fs_t *fs = parent->fs;
svn_string_t str;
svn_fs_id_t *id = NULL;
dag_node_t *node;
if (parent->kind != svn_node_dir)
return svn_error_createf
(SVN_ERR_FS_NOT_DIRECTORY, NULL,
_("Attempted to delete entry '%s' from *non*-directory node"), name);
if (! svn_fs_base__dag_check_mutable(parent, txn_id))
return svn_error_createf
(SVN_ERR_FS_NOT_MUTABLE, NULL,
_("Attempted to delete entry '%s' from immutable directory node"),
name);
if (! svn_path_is_single_path_component(name))
return svn_error_createf
(SVN_ERR_FS_NOT_SINGLE_PATH_COMPONENT, NULL,
_("Attempted to delete a node with an illegal name '%s'"), name);
SVN_ERR(svn_fs_bdb__get_node_revision(&parent_noderev, fs, parent->id,
trail, pool));
rep_key = parent_noderev->data_key;
if (! rep_key)
return svn_error_createf
(SVN_ERR_FS_NO_SUCH_ENTRY, NULL,
_("Delete failed: directory has no entry '%s'"), name);
SVN_ERR(svn_fs_base__get_mutable_rep(&mutable_rep_key, rep_key,
fs, txn_id, trail, pool));
if (! svn_fs_base__same_keys(mutable_rep_key, rep_key)) {
parent_noderev->data_key = mutable_rep_key;
SVN_ERR(svn_fs_bdb__put_node_revision(fs, parent->id, parent_noderev,
trail, pool));
}
SVN_ERR(svn_fs_base__rep_contents(&str, fs, rep_key, trail, pool));
entries_skel = svn_fs_base__parse_skel(str.data, str.len, pool);
if (entries_skel)
SVN_ERR(svn_fs_base__parse_entries_skel(&entries, entries_skel, pool));
if (entries)
id = apr_hash_get(entries, name, APR_HASH_KEY_STRING);
if (! id)
return svn_error_createf
(SVN_ERR_FS_NO_SUCH_ENTRY, NULL,
_("Delete failed: directory has no entry '%s'"), name);
SVN_ERR(svn_fs_base__dag_get_node(&node, svn_fs_base__dag_get_fs(parent),
id, trail, pool));
SVN_ERR(svn_fs_base__dag_delete_if_mutable(parent->fs, id, txn_id,
trail, pool));
apr_hash_set(entries, name, APR_HASH_KEY_STRING, NULL);
{
svn_stream_t *ws;
svn_stringbuf_t *unparsed_entries;
apr_size_t len;
SVN_ERR(svn_fs_base__unparse_entries_skel(&entries_skel, entries, pool));
unparsed_entries = svn_fs_base__unparse_skel(entries_skel, pool);
SVN_ERR(svn_fs_base__rep_contents_write_stream(&ws, fs, mutable_rep_key,
txn_id, TRUE, trail,
pool));
len = unparsed_entries->len;
SVN_ERR(svn_stream_write(ws, unparsed_entries->data, &len));
SVN_ERR(svn_stream_close(ws));
}
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_base__dag_remove_node(svn_fs_t *fs,
const svn_fs_id_t *id,
const char *txn_id,
trail_t *trail,
apr_pool_t *pool) {
dag_node_t *node;
node_revision_t *noderev;
SVN_ERR(svn_fs_base__dag_get_node(&node, fs, id, trail, pool));
if (! svn_fs_base__dag_check_mutable(node, txn_id))
return svn_error_createf(SVN_ERR_FS_NOT_MUTABLE, NULL,
_("Attempted removal of immutable node"));
SVN_ERR(svn_fs_bdb__get_node_revision(&noderev, fs, id, trail, pool));
if (noderev->prop_key)
SVN_ERR(svn_fs_base__delete_rep_if_mutable(fs, noderev->prop_key,
txn_id, trail, pool));
if (noderev->data_key)
SVN_ERR(svn_fs_base__delete_rep_if_mutable(fs, noderev->data_key,
txn_id, trail, pool));
if (noderev->edit_key)
SVN_ERR(svn_fs_base__delete_rep_if_mutable(fs, noderev->edit_key,
txn_id, trail, pool));
SVN_ERR(svn_fs_base__delete_node_revision(fs, id,
noderev->predecessor_id
? FALSE : TRUE,
trail, pool));
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_base__dag_delete_if_mutable(svn_fs_t *fs,
const svn_fs_id_t *id,
const char *txn_id,
trail_t *trail,
apr_pool_t *pool) {
dag_node_t *node;
SVN_ERR(svn_fs_base__dag_get_node(&node, fs, id, trail, pool));
if (! svn_fs_base__dag_check_mutable(node, txn_id))
return SVN_NO_ERROR;
if (node->kind == svn_node_dir) {
apr_hash_t *entries;
apr_hash_index_t *hi;
SVN_ERR(svn_fs_base__dag_dir_entries(&entries, node, trail, pool));
if (entries) {
apr_pool_t *subpool = svn_pool_create(pool);
for (hi = apr_hash_first(pool, entries);
hi;
hi = apr_hash_next(hi)) {
void *val;
svn_fs_dirent_t *dirent;
apr_hash_this(hi, NULL, NULL, &val);
dirent = val;
SVN_ERR(svn_fs_base__dag_delete_if_mutable(fs, dirent->id,
txn_id, trail,
subpool));
}
}
}
SVN_ERR(svn_fs_base__dag_remove_node(fs, id, txn_id, trail, pool));
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_base__dag_make_file(dag_node_t **child_p,
dag_node_t *parent,
const char *parent_path,
const char *name,
const char *txn_id,
trail_t *trail,
apr_pool_t *pool) {
return make_entry(child_p, parent, parent_path, name, FALSE,
txn_id, trail, pool);
}
svn_error_t *
svn_fs_base__dag_make_dir(dag_node_t **child_p,
dag_node_t *parent,
const char *parent_path,
const char *name,
const char *txn_id,
trail_t *trail,
apr_pool_t *pool) {
return make_entry(child_p, parent, parent_path, name, TRUE,
txn_id, trail, pool);
}
svn_error_t *
svn_fs_base__dag_get_contents(svn_stream_t **contents,
dag_node_t *file,
trail_t *trail,
apr_pool_t *pool) {
node_revision_t *noderev;
if (file->kind != svn_node_file)
return svn_error_createf
(SVN_ERR_FS_NOT_FILE, NULL,
_("Attempted to get textual contents of a *non*-file node"));
SVN_ERR(svn_fs_bdb__get_node_revision(&noderev, file->fs, file->id,
trail, pool));
SVN_ERR(svn_fs_base__rep_contents_read_stream(contents, file->fs,
noderev->data_key,
FALSE, trail, pool));
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_base__dag_file_length(svn_filesize_t *length,
dag_node_t *file,
trail_t *trail,
apr_pool_t *pool) {
node_revision_t *noderev;
if (file->kind != svn_node_file)
return svn_error_createf
(SVN_ERR_FS_NOT_FILE, NULL,
_("Attempted to get length of a *non*-file node"));
SVN_ERR(svn_fs_bdb__get_node_revision(&noderev, file->fs, file->id,
trail, pool));
if (noderev->data_key)
SVN_ERR(svn_fs_base__rep_contents_size(length, file->fs,
noderev->data_key, trail, pool));
else
*length = 0;
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_base__dag_file_checksum(unsigned char digest[],
dag_node_t *file,
trail_t *trail,
apr_pool_t *pool) {
node_revision_t *noderev;
if (file->kind != svn_node_file)
return svn_error_createf
(SVN_ERR_FS_NOT_FILE, NULL,
_("Attempted to get checksum of a *non*-file node"));
SVN_ERR(svn_fs_bdb__get_node_revision(&noderev, file->fs, file->id,
trail, pool));
if (noderev->data_key)
SVN_ERR(svn_fs_base__rep_contents_checksum(digest, file->fs,
noderev->data_key,
trail, pool));
else
memset(digest, 0, APR_MD5_DIGESTSIZE);
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_base__dag_get_edit_stream(svn_stream_t **contents,
dag_node_t *file,
const char *txn_id,
trail_t *trail,
apr_pool_t *pool) {
svn_fs_t *fs = file->fs;
node_revision_t *noderev;
const char *mutable_rep_key;
svn_stream_t *ws;
if (file->kind != svn_node_file)
return svn_error_createf
(SVN_ERR_FS_NOT_FILE, NULL,
_("Attempted to set textual contents of a *non*-file node"));
if (! svn_fs_base__dag_check_mutable(file, txn_id))
return svn_error_createf
(SVN_ERR_FS_NOT_MUTABLE, NULL,
_("Attempted to set textual contents of an immutable node"));
SVN_ERR(svn_fs_bdb__get_node_revision(&noderev, fs, file->id,
trail, pool));
if (noderev->edit_key)
SVN_ERR(svn_fs_base__delete_rep_if_mutable(fs, noderev->edit_key,
txn_id, trail, pool));
SVN_ERR(svn_fs_base__get_mutable_rep(&mutable_rep_key, NULL, fs,
txn_id, trail, pool));
noderev->edit_key = mutable_rep_key;
SVN_ERR(svn_fs_bdb__put_node_revision(fs, file->id, noderev,
trail, pool));
SVN_ERR(svn_fs_base__rep_contents_write_stream(&ws, fs, mutable_rep_key,
txn_id, FALSE, trail,
pool));
*contents = ws;
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_base__dag_finalize_edits(dag_node_t *file,
const char *checksum,
const char *txn_id,
trail_t *trail,
apr_pool_t *pool) {
svn_fs_t *fs = file->fs;
node_revision_t *noderev;
const char *old_data_key;
if (file->kind != svn_node_file)
return svn_error_createf
(SVN_ERR_FS_NOT_FILE, NULL,
_("Attempted to set textual contents of a *non*-file node"));
if (! svn_fs_base__dag_check_mutable(file, txn_id))
return svn_error_createf
(SVN_ERR_FS_NOT_MUTABLE, NULL,
_("Attempted to set textual contents of an immutable node"));
SVN_ERR(svn_fs_bdb__get_node_revision(&noderev, fs, file->id,
trail, pool));
if (! noderev->edit_key)
return SVN_NO_ERROR;
if (checksum) {
unsigned char digest[APR_MD5_DIGESTSIZE];
const char *hex;
SVN_ERR(svn_fs_base__rep_contents_checksum
(digest, fs, noderev->edit_key, trail, pool));
hex = svn_md5_digest_to_cstring_display(digest, pool);
if (strcmp(checksum, hex) != 0)
return svn_error_createf
(SVN_ERR_CHECKSUM_MISMATCH,
NULL,
_("Checksum mismatch, rep '%s':\n"
" expected: %s\n"
" actual: %s\n"),
noderev->edit_key, checksum, hex);
}
old_data_key = noderev->data_key;
noderev->data_key = noderev->edit_key;
noderev->edit_key = NULL;
SVN_ERR(svn_fs_bdb__put_node_revision(fs, file->id, noderev, trail, pool));
if (old_data_key)
SVN_ERR(svn_fs_base__delete_rep_if_mutable(fs, old_data_key, txn_id,
trail, pool));
return SVN_NO_ERROR;
}
dag_node_t *
svn_fs_base__dag_dup(dag_node_t *node,
apr_pool_t *pool) {
dag_node_t *new_node = apr_pcalloc(pool, sizeof(*new_node));
new_node->fs = node->fs;
new_node->id = svn_fs_base__id_copy(node->id, pool);
new_node->kind = node->kind;
new_node->created_path = apr_pstrdup(pool, node->created_path);
return new_node;
}
svn_error_t *
svn_fs_base__dag_open(dag_node_t **child_p,
dag_node_t *parent,
const char *name,
trail_t *trail,
apr_pool_t *pool) {
const svn_fs_id_t *node_id;
SVN_ERR(dir_entry_id_from_node(&node_id, parent, name, trail, pool));
if (! node_id)
return svn_error_createf
(SVN_ERR_FS_NOT_FOUND, NULL,
_("Attempted to open non-existent child node '%s'"), name);
if (! svn_path_is_single_path_component(name))
return svn_error_createf
(SVN_ERR_FS_NOT_SINGLE_PATH_COMPONENT, NULL,
_("Attempted to open node with an illegal name '%s'"), name);
return svn_fs_base__dag_get_node(child_p, svn_fs_base__dag_get_fs(parent),
node_id, trail, pool);
}
svn_error_t *
svn_fs_base__dag_copy(dag_node_t *to_node,
const char *entry,
dag_node_t *from_node,
svn_boolean_t preserve_history,
svn_revnum_t from_rev,
const char *from_path,
const char *txn_id,
trail_t *trail,
apr_pool_t *pool) {
const svn_fs_id_t *id;
if (preserve_history) {
node_revision_t *noderev;
const char *copy_id;
svn_fs_t *fs = svn_fs_base__dag_get_fs(from_node);
const svn_fs_id_t *src_id = svn_fs_base__dag_get_id(from_node);
const char *from_txn_id = NULL;
SVN_ERR(svn_fs_bdb__get_node_revision(&noderev, fs, from_node->id,
trail, pool));
SVN_ERR(svn_fs_bdb__reserve_copy_id(&copy_id, fs, trail, pool));
noderev->predecessor_id = svn_fs_base__id_copy(src_id, pool);
if (noderev->predecessor_count != -1)
noderev->predecessor_count++;
noderev->created_path = svn_path_join
(svn_fs_base__dag_get_created_path(to_node), entry, pool);
SVN_ERR(svn_fs_base__create_successor(&id, fs, src_id, noderev,
copy_id, txn_id, trail, pool));
SVN_ERR(svn_fs_base__rev_get_txn_id(&from_txn_id, fs, from_rev,
trail, pool));
SVN_ERR(svn_fs_bdb__create_copy
(fs, copy_id,
svn_fs__canonicalize_abspath(from_path, pool),
from_txn_id, id, copy_kind_real, trail, pool));
SVN_ERR(svn_fs_base__add_txn_copy(fs, txn_id, copy_id, trail, pool));
} else {
id = svn_fs_base__dag_get_id(from_node);
}
SVN_ERR(svn_fs_base__dag_set_entry(to_node, entry, id, txn_id,
trail, pool));
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_base__dag_deltify(dag_node_t *target,
dag_node_t *source,
svn_boolean_t props_only,
trail_t *trail,
apr_pool_t *pool) {
node_revision_t *source_nr, *target_nr;
svn_fs_t *fs = svn_fs_base__dag_get_fs(target);
SVN_ERR(svn_fs_bdb__get_node_revision(&target_nr, fs, target->id,
trail, pool));
SVN_ERR(svn_fs_bdb__get_node_revision(&source_nr, fs, source->id,
trail, pool));
if (target_nr->prop_key
&& source_nr->prop_key
&& (strcmp(target_nr->prop_key, source_nr->prop_key)))
SVN_ERR(svn_fs_base__rep_deltify(fs, target_nr->prop_key,
source_nr->prop_key, trail, pool));
if ((! props_only)
&& target_nr->data_key
&& source_nr->data_key
&& (strcmp(target_nr->data_key, source_nr->data_key)))
SVN_ERR(svn_fs_base__rep_deltify(fs, target_nr->data_key,
source_nr->data_key, trail, pool));
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_base__dag_commit_txn(svn_revnum_t *new_rev,
svn_fs_txn_t *txn,
trail_t *trail,
apr_pool_t *pool) {
revision_t revision;
svn_string_t date;
apr_hash_t *txnprops;
svn_fs_t *fs = txn->fs;
const char *txn_id = txn->id;
SVN_ERR(svn_fs_base__txn_proplist_in_trail(&txnprops, txn_id, trail));
revision.txn_id = txn_id;
*new_rev = SVN_INVALID_REVNUM;
SVN_ERR(svn_fs_bdb__put_rev(new_rev, fs, &revision, trail, pool));
if (apr_hash_get(txnprops, SVN_FS__PROP_TXN_CHECK_OOD, APR_HASH_KEY_STRING))
SVN_ERR(svn_fs_base__set_txn_prop
(fs, txn_id, SVN_FS__PROP_TXN_CHECK_OOD, NULL, trail, pool));
if (apr_hash_get(txnprops, SVN_FS__PROP_TXN_CHECK_LOCKS,
APR_HASH_KEY_STRING))
SVN_ERR(svn_fs_base__set_txn_prop
(fs, txn_id, SVN_FS__PROP_TXN_CHECK_LOCKS, NULL, trail, pool));
SVN_ERR(svn_fs_base__txn_make_committed(fs, txn_id, *new_rev,
trail, pool));
date.data = svn_time_to_cstring(apr_time_now(), pool);
date.len = strlen(date.data);
SVN_ERR(svn_fs_base__set_rev_prop(fs, *new_rev, SVN_PROP_REVISION_DATE,
&date, trail, pool));
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_base__things_different(svn_boolean_t *props_changed,
svn_boolean_t *contents_changed,
dag_node_t *node1,
dag_node_t *node2,
trail_t *trail,
apr_pool_t *pool) {
node_revision_t *noderev1, *noderev2;
if (! props_changed && ! contents_changed)
return SVN_NO_ERROR;
SVN_ERR(svn_fs_bdb__get_node_revision(&noderev1, node1->fs, node1->id,
trail, pool));
SVN_ERR(svn_fs_bdb__get_node_revision(&noderev2, node2->fs, node2->id,
trail, pool));
if (props_changed != NULL)
*props_changed = (! svn_fs_base__same_keys(noderev1->prop_key,
noderev2->prop_key));
if (contents_changed != NULL)
*contents_changed = (! svn_fs_base__same_keys(noderev1->data_key,
noderev2->data_key));
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_base__dag_get_mergeinfo_stats(svn_boolean_t *has_mergeinfo,
apr_int64_t *count,
dag_node_t *node,
trail_t *trail,
apr_pool_t *pool) {
node_revision_t *node_rev;
svn_fs_t *fs = svn_fs_base__dag_get_fs(node);
const svn_fs_id_t *id = svn_fs_base__dag_get_id(node);
SVN_ERR(svn_fs_bdb__get_node_revision(&node_rev, fs, id, trail, pool));
if (has_mergeinfo)
*has_mergeinfo = node_rev->has_mergeinfo;
if (count)
*count = node_rev->mergeinfo_count;
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_base__dag_set_has_mergeinfo(dag_node_t *node,
svn_boolean_t has_mergeinfo,
svn_boolean_t *had_mergeinfo,
const char *txn_id,
trail_t *trail,
apr_pool_t *pool) {
node_revision_t *node_rev;
svn_fs_t *fs = svn_fs_base__dag_get_fs(node);
const svn_fs_id_t *id = svn_fs_base__dag_get_id(node);
SVN_ERR(svn_fs_base__test_required_feature_format
(trail->fs, "mergeinfo", SVN_FS_BASE__MIN_MERGEINFO_FORMAT));
if (! svn_fs_base__dag_check_mutable(node, txn_id))
return svn_error_createf(SVN_ERR_FS_NOT_MUTABLE, NULL,
_("Attempted merge tracking info change on "
"immutable node"));
SVN_ERR(svn_fs_bdb__get_node_revision(&node_rev, fs, id, trail, pool));
*had_mergeinfo = node_rev->has_mergeinfo;
if ((! has_mergeinfo) != (! *had_mergeinfo)) {
node_rev->has_mergeinfo = has_mergeinfo;
if (has_mergeinfo)
node_rev->mergeinfo_count++;
else
node_rev->mergeinfo_count--;
SVN_ERR(svn_fs_bdb__put_node_revision(fs, id, node_rev, trail, pool));
}
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_base__dag_adjust_mergeinfo_count(dag_node_t *node,
apr_int64_t count_delta,
const char *txn_id,
trail_t *trail,
apr_pool_t *pool) {
node_revision_t *node_rev;
svn_fs_t *fs = svn_fs_base__dag_get_fs(node);
const svn_fs_id_t *id = svn_fs_base__dag_get_id(node);
SVN_ERR(svn_fs_base__test_required_feature_format
(trail->fs, "mergeinfo", SVN_FS_BASE__MIN_MERGEINFO_FORMAT));
if (! svn_fs_base__dag_check_mutable(node, txn_id))
return svn_error_createf(SVN_ERR_FS_NOT_MUTABLE, NULL,
_("Attempted mergeinfo count change on "
"immutable node"));
if (count_delta == 0)
return SVN_NO_ERROR;
SVN_ERR(svn_fs_bdb__get_node_revision(&node_rev, fs, id, trail, pool));
node_rev->mergeinfo_count = node_rev->mergeinfo_count + count_delta;
if ((node_rev->mergeinfo_count < 0)
|| ((node->kind == svn_node_file) && (node_rev->mergeinfo_count > 1)))
return svn_error_createf(SVN_ERR_FS_CORRUPT, NULL,
apr_psprintf(pool,
_("Invalid value (%%%s) for node "
"revision mergeinfo count"),
APR_INT64_T_FMT),
node_rev->mergeinfo_count);
SVN_ERR(svn_fs_bdb__put_node_revision(fs, id, node_rev, trail, pool));
return SVN_NO_ERROR;
}
