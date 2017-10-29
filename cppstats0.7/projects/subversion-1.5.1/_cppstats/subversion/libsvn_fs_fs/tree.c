#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <apr_pools.h>
#include <apr_hash.h>
#include "svn_private_config.h"
#include "svn_pools.h"
#include "svn_error.h"
#include "svn_path.h"
#include "svn_md5.h"
#include "svn_mergeinfo.h"
#include "svn_fs.h"
#include "fs.h"
#include "err.h"
#include "key-gen.h"
#include "dag.h"
#include "lock.h"
#include "tree.h"
#include "fs_fs.h"
#include "id.h"
#include "private/svn_mergeinfo_private.h"
#include "private/svn_fs_util.h"
#include "../libsvn_fs/fs-loader.h"
#define WRITE_BUFFER_SIZE 512000
#define TXN_NODE_CACHE_MAX_KEYS 32
#define REV_NODE_CACHE_MAX_KEYS 128
typedef struct {
dag_node_t *root_dir;
apr_hash_t *copyfrom_cache;
} fs_rev_root_data_t;
typedef struct {
dag_node_cache_t txn_node_list;
apr_hash_t *txn_node_cache;
} fs_txn_root_data_t;
static svn_error_t * get_dag(dag_node_t **dag_node_p, svn_fs_root_t *root,
const char *path, apr_pool_t *pool);
static svn_fs_root_t *make_revision_root(svn_fs_t *fs, svn_revnum_t rev,
dag_node_t *root_dir,
apr_pool_t *pool);
static svn_fs_root_t *make_txn_root(svn_fs_t *fs, const char *txn,
svn_revnum_t base_rev, apr_uint32_t flags,
apr_pool_t *pool);
static void
locate_cache(dag_node_cache_t **node_list,
apr_hash_t **node_cache,
const char **key,
svn_fs_root_t *root,
const char *path,
apr_pool_t *pool) {
if (root->is_txn_root) {
fs_txn_root_data_t *frd = root->fsap_data;
*node_list = &frd->txn_node_list;
*node_cache = frd->txn_node_cache;
*key = path;
} else {
fs_fs_data_t *ffd = root->fs->fsap_data;
*node_list = &ffd->rev_node_list;
*node_cache = ffd->rev_node_cache;
*key = apr_psprintf(pool, "%ld%s",
root->rev, path);
}
}
static dag_node_t *
dag_node_cache_get(svn_fs_root_t *root,
const char *path,
apr_pool_t *pool) {
dag_node_cache_t *item, *node_list;
apr_hash_t *node_cache;
const char *key;
assert(*path == '/');
locate_cache(&node_list, &node_cache, &key,
root, path, pool);
item = apr_hash_get(node_cache, key, APR_HASH_KEY_STRING);
if (item && item->node) {
item->prev->next = item->next;
item->next->prev = item->prev;
item->prev = node_list;
item->next = node_list->next;
item->prev->next = item;
item->next->prev = item;
return svn_fs_fs__dag_dup(item->node, pool);
}
return NULL;
}
static void
dag_node_cache_set(svn_fs_root_t *root,
const char *path,
dag_node_t *node,
apr_pool_t *temp_pool) {
dag_node_cache_t *item, *node_list;
apr_hash_t *node_cache;
const char *key;
apr_pool_t *pool;
int max_keys = root->is_txn_root
? TXN_NODE_CACHE_MAX_KEYS : REV_NODE_CACHE_MAX_KEYS;
assert(*path == '/');
locate_cache(&node_list, &node_cache, &key,
root, path, temp_pool);
item = apr_hash_get(node_cache, key, APR_HASH_KEY_STRING);
if (!item && apr_hash_count(node_cache) == max_keys)
item = node_list->prev;
if (item) {
item->prev->next = item->next;
item->next->prev = item->prev;
apr_hash_set(node_cache, item->key, APR_HASH_KEY_STRING, NULL);
pool = item->pool;
svn_pool_clear(pool);
} else {
apr_pool_t *parent_pool = root->is_txn_root ? root->pool : root->fs->pool;
pool = svn_pool_create(parent_pool);
}
item = apr_palloc(pool, sizeof(*item));
item->key = apr_pstrdup(pool, key);
item->node = svn_fs_fs__dag_dup(node, pool);
item->pool = pool;
item->prev = node_list;
item->next = node_list->next;
item->prev->next = item;
item->next->prev = item;
apr_hash_set(node_cache, item->key, APR_HASH_KEY_STRING, item);
}
static void
dag_node_cache_invalidate(svn_fs_root_t *root,
const char *path) {
fs_txn_root_data_t *frd;
apr_size_t len = strlen(path);
const char *key;
dag_node_cache_t *item;
assert(root->is_txn_root);
frd = root->fsap_data;
for (item = frd->txn_node_list.next;
item != &frd->txn_node_list;
item = item->next) {
key = item->key;
if (strncmp(key, path, len) == 0 && (key[len] == '/' || !key[len]))
item->node = NULL;
}
}
svn_error_t *
svn_fs_fs__txn_root(svn_fs_root_t **root_p,
svn_fs_txn_t *txn,
apr_pool_t *pool) {
svn_fs_root_t *root;
apr_uint32_t flags = 0;
apr_hash_t *txnprops;
SVN_ERR(svn_fs_fs__txn_proplist(&txnprops, txn, pool));
if (txnprops) {
if (apr_hash_get(txnprops, SVN_FS__PROP_TXN_CHECK_OOD,
APR_HASH_KEY_STRING))
flags |= SVN_FS_TXN_CHECK_OOD;
if (apr_hash_get(txnprops, SVN_FS__PROP_TXN_CHECK_LOCKS,
APR_HASH_KEY_STRING))
flags |= SVN_FS_TXN_CHECK_LOCKS;
}
root = make_txn_root(txn->fs, txn->id, txn->base_rev, flags, pool);
*root_p = root;
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_fs__revision_root(svn_fs_root_t **root_p,
svn_fs_t *fs,
svn_revnum_t rev,
apr_pool_t *pool) {
dag_node_t *root_dir;
SVN_ERR(svn_fs__check_fs(fs, TRUE));
SVN_ERR(svn_fs_fs__dag_revision_root(&root_dir, fs, rev, pool));
*root_p = make_revision_root(fs, rev, root_dir, pool);
return SVN_NO_ERROR;
}
static svn_error_t *
root_node(dag_node_t **node_p,
svn_fs_root_t *root,
apr_pool_t *pool) {
if (root->is_txn_root) {
return svn_fs_fs__dag_txn_root(node_p, root->fs, root->txn, pool);
} else {
fs_rev_root_data_t *frd = root->fsap_data;
*node_p = svn_fs_fs__dag_dup(frd->root_dir, pool);
return SVN_NO_ERROR;
}
}
static svn_error_t *
mutable_root_node(dag_node_t **node_p,
svn_fs_root_t *root,
const char *error_path,
apr_pool_t *pool) {
if (root->is_txn_root)
return svn_fs_fs__dag_clone_root(node_p, root->fs, root->txn, pool);
else
return SVN_FS__ERR_NOT_MUTABLE(root->fs, root->rev, error_path);
}
typedef enum copy_id_inherit_t {
copy_id_inherit_unknown = 0,
copy_id_inherit_self,
copy_id_inherit_parent,
copy_id_inherit_new
} copy_id_inherit_t;
typedef struct parent_path_t {
dag_node_t *node;
char *entry;
struct parent_path_t *parent;
copy_id_inherit_t copy_inherit;
const char *copy_src_path;
} parent_path_t;
static const char *
parent_path_path(parent_path_t *parent_path,
apr_pool_t *pool) {
const char *path_so_far = "/";
if (parent_path->parent)
path_so_far = parent_path_path(parent_path->parent, pool);
return parent_path->entry
? svn_path_join(path_so_far, parent_path->entry, pool)
: path_so_far;
}
static const char *
parent_path_relpath(parent_path_t *child,
parent_path_t *ancestor,
apr_pool_t *pool) {
const char *path_so_far = "";
parent_path_t *this_node = child;
while (this_node != ancestor) {
assert(this_node != NULL);
path_so_far = svn_path_join(this_node->entry, path_so_far, pool);
this_node = this_node->parent;
}
return path_so_far;
}
static svn_error_t *
get_copy_inheritance(copy_id_inherit_t *inherit_p,
const char **copy_src_path,
svn_fs_t *fs,
parent_path_t *child,
const char *txn_id,
apr_pool_t *pool) {
const svn_fs_id_t *child_id, *parent_id, *copyroot_id;
const char *child_copy_id, *parent_copy_id;
const char *id_path = NULL;
svn_fs_root_t *copyroot_root;
dag_node_t *copyroot_node;
svn_revnum_t copyroot_rev;
const char *copyroot_path;
assert(child && child->parent && txn_id);
child_id = svn_fs_fs__dag_get_id(child->node);
parent_id = svn_fs_fs__dag_get_id(child->parent->node);
child_copy_id = svn_fs_fs__id_copy_id(child_id);
parent_copy_id = svn_fs_fs__id_copy_id(parent_id);
if (svn_fs_fs__id_txn_id(child_id)) {
*inherit_p = copy_id_inherit_self;
*copy_src_path = NULL;
return SVN_NO_ERROR;
}
*inherit_p = copy_id_inherit_parent;
*copy_src_path = NULL;
if (strcmp(child_copy_id, "0") == 0)
return SVN_NO_ERROR;
if (svn_fs_fs__key_compare(child_copy_id, parent_copy_id) == 0)
return SVN_NO_ERROR;
SVN_ERR(svn_fs_fs__dag_get_copyroot(&copyroot_rev, &copyroot_path,
child->node,pool));
SVN_ERR(svn_fs_fs__revision_root(&copyroot_root, fs, copyroot_rev, pool));
SVN_ERR(get_dag(&copyroot_node, copyroot_root, copyroot_path, pool));
copyroot_id = svn_fs_fs__dag_get_id(copyroot_node);
if (svn_fs_fs__id_compare(copyroot_id, child_id) == -1)
return SVN_NO_ERROR;
id_path = svn_fs_fs__dag_get_created_path(child->node);
if (strcmp(id_path, parent_path_path(child, pool)) == 0) {
*inherit_p = copy_id_inherit_self;
return SVN_NO_ERROR;
}
*inherit_p = copy_id_inherit_new;
*copy_src_path = id_path;
return SVN_NO_ERROR;
}
static parent_path_t *
make_parent_path(dag_node_t *node,
char *entry,
parent_path_t *parent,
apr_pool_t *pool) {
parent_path_t *parent_path = apr_pcalloc(pool, sizeof(*parent_path));
parent_path->node = node;
parent_path->entry = entry;
parent_path->parent = parent;
parent_path->copy_inherit = copy_id_inherit_unknown;
parent_path->copy_src_path = NULL;
return parent_path;
}
typedef enum open_path_flags_t {
open_path_last_optional = 1
} open_path_flags_t;
static svn_error_t *
open_path(parent_path_t **parent_path_p,
svn_fs_root_t *root,
const char *path,
int flags,
const char *txn_id,
apr_pool_t *pool) {
svn_fs_t *fs = root->fs;
const svn_fs_id_t *id;
dag_node_t *here;
parent_path_t *parent_path;
const char *rest;
const char *canon_path = svn_fs__canonicalize_abspath(path, pool);
const char *path_so_far = "/";
SVN_ERR(root_node(&here, root, pool));
id = svn_fs_fs__dag_get_id(here);
parent_path = make_parent_path(here, 0, 0, pool);
parent_path->copy_inherit = copy_id_inherit_self;
rest = canon_path + 1;
for (;;) {
const char *next;
char *entry;
dag_node_t *child;
entry = svn_fs__next_entry_name(&next, rest, pool);
path_so_far = svn_path_join(path_so_far, entry, pool);
if (*entry == '\0') {
child = here;
} else {
copy_id_inherit_t inherit;
const char *copy_path = NULL;
svn_error_t *err = SVN_NO_ERROR;
dag_node_t *cached_node;
cached_node = dag_node_cache_get(root, path_so_far, pool);
if (cached_node)
child = cached_node;
else
err = svn_fs_fs__dag_open(&child, here, entry, pool);
if (err && err->apr_err == SVN_ERR_FS_NOT_FOUND) {
svn_error_clear(err);
if ((flags & open_path_last_optional)
&& (! next || *next == '\0')) {
parent_path = make_parent_path(NULL, entry, parent_path,
pool);
break;
} else {
return SVN_FS__NOT_FOUND(root, path);
}
}
SVN_ERR(err);
parent_path = make_parent_path(child, entry, parent_path, pool);
if (txn_id) {
SVN_ERR(get_copy_inheritance(&inherit, &copy_path,
fs, parent_path, txn_id, pool));
parent_path->copy_inherit = inherit;
parent_path->copy_src_path = apr_pstrdup(pool, copy_path);
}
if (! cached_node)
dag_node_cache_set(root, path_so_far, child, pool);
}
if (! next)
break;
if (svn_fs_fs__dag_node_kind(child) != svn_node_dir)
SVN_ERR_W(SVN_FS__ERR_NOT_DIRECTORY(fs, path_so_far),
apr_psprintf(pool, _("Failure opening '%s'"), path));
rest = next;
here = child;
}
*parent_path_p = parent_path;
return SVN_NO_ERROR;
}
static svn_error_t *
make_path_mutable(svn_fs_root_t *root,
parent_path_t *parent_path,
const char *error_path,
apr_pool_t *pool) {
dag_node_t *clone;
const char *txn_id = root->txn;
if (svn_fs_fs__dag_check_mutable(parent_path->node))
return SVN_NO_ERROR;
if (parent_path->parent) {
const svn_fs_id_t *parent_id, *child_id, *copyroot_id;
const char *copy_id = NULL;
copy_id_inherit_t inherit = parent_path->copy_inherit;
const char *clone_path, *copyroot_path;
svn_revnum_t copyroot_rev;
svn_boolean_t is_parent_copyroot = FALSE;
svn_fs_root_t *copyroot_root;
dag_node_t *copyroot_node;
SVN_ERR(make_path_mutable(root, parent_path->parent,
error_path, pool));
switch (inherit) {
case copy_id_inherit_parent:
parent_id = svn_fs_fs__dag_get_id(parent_path->parent->node);
copy_id = svn_fs_fs__id_copy_id(parent_id);
break;
case copy_id_inherit_new:
SVN_ERR(svn_fs_fs__reserve_copy_id(&copy_id, root->fs, txn_id,
pool));
break;
case copy_id_inherit_self:
copy_id = NULL;
break;
case copy_id_inherit_unknown:
default:
abort();
}
SVN_ERR(svn_fs_fs__dag_get_copyroot(&copyroot_rev, &copyroot_path,
parent_path->node, pool));
SVN_ERR(svn_fs_fs__revision_root(&copyroot_root, root->fs,
copyroot_rev, pool));
SVN_ERR(get_dag(&copyroot_node, copyroot_root, copyroot_path, pool));
child_id = svn_fs_fs__dag_get_id(parent_path->node);
copyroot_id = svn_fs_fs__dag_get_id(copyroot_node);
if (strcmp(svn_fs_fs__id_node_id(child_id),
svn_fs_fs__id_node_id(copyroot_id)) != 0)
is_parent_copyroot = TRUE;
clone_path = parent_path_path(parent_path->parent, pool);
SVN_ERR(svn_fs_fs__dag_clone_child(&clone,
parent_path->parent->node,
clone_path,
parent_path->entry,
copy_id, txn_id,
is_parent_copyroot,
pool));
dag_node_cache_set(root, parent_path_path(parent_path, pool), clone,
pool);
} else {
SVN_ERR(mutable_root_node(&clone, root, error_path, pool));
}
parent_path->node = clone;
return SVN_NO_ERROR;
}
static svn_error_t *
get_dag(dag_node_t **dag_node_p,
svn_fs_root_t *root,
const char *path,
apr_pool_t *pool) {
parent_path_t *parent_path;
dag_node_t *node = NULL;
path = svn_fs__canonicalize_abspath(path, pool);
node = dag_node_cache_get(root, path, pool);
if (! node) {
SVN_ERR(open_path(&parent_path, root, path, 0, NULL, pool));
node = parent_path->node;
}
*dag_node_p = node;
return SVN_NO_ERROR;
}
static svn_error_t *
add_change(svn_fs_t *fs,
const char *txn_id,
const char *path,
const svn_fs_id_t *noderev_id,
svn_fs_path_change_kind_t change_kind,
svn_boolean_t text_mod,
svn_boolean_t prop_mod,
svn_revnum_t copyfrom_rev,
const char *copyfrom_path,
apr_pool_t *pool) {
SVN_ERR(svn_fs_fs__add_change(fs, txn_id,
svn_fs__canonicalize_abspath(path, pool),
noderev_id, change_kind, text_mod, prop_mod,
copyfrom_rev, copyfrom_path,
pool));
return SVN_NO_ERROR;
}
static svn_error_t *
fs_node_id(const svn_fs_id_t **id_p,
svn_fs_root_t *root,
const char *path,
apr_pool_t *pool) {
if ((! root->is_txn_root)
&& (path[0] == '\0' || ((path[0] == '/') && (path[1] == '\0')))) {
fs_rev_root_data_t *frd = root->fsap_data;
*id_p = svn_fs_fs__id_copy(svn_fs_fs__dag_get_id(frd->root_dir), pool);
} else {
dag_node_t *node;
SVN_ERR(get_dag(&node, root, path, pool));
*id_p = svn_fs_fs__id_copy(svn_fs_fs__dag_get_id(node), pool);
}
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_fs__node_created_rev(svn_revnum_t *revision,
svn_fs_root_t *root,
const char *path,
apr_pool_t *pool) {
dag_node_t *node;
SVN_ERR(get_dag(&node, root, path, pool));
SVN_ERR(svn_fs_fs__dag_get_revision(revision, node, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
fs_node_created_path(const char **created_path,
svn_fs_root_t *root,
const char *path,
apr_pool_t *pool) {
dag_node_t *node;
SVN_ERR(get_dag(&node, root, path, pool));
*created_path = svn_fs_fs__dag_get_created_path(node);
return SVN_NO_ERROR;
}
static svn_error_t *
node_kind(svn_node_kind_t *kind_p,
svn_fs_root_t *root,
const char *path,
apr_pool_t *pool) {
const svn_fs_id_t *node_id;
dag_node_t *node;
SVN_ERR(fs_node_id(&node_id, root, path, pool));
SVN_ERR(svn_fs_fs__dag_get_node(&node, root->fs, node_id, pool));
*kind_p = svn_fs_fs__dag_node_kind(node);
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_fs__check_path(svn_node_kind_t *kind_p,
svn_fs_root_t *root,
const char *path,
apr_pool_t *pool) {
svn_error_t *err = node_kind(kind_p, root, path, pool);
if (err &&
((err->apr_err == SVN_ERR_FS_NOT_FOUND)
|| (err->apr_err == SVN_ERR_FS_NOT_DIRECTORY))) {
svn_error_clear(err);
*kind_p = svn_node_none;
} else if (err) {
return err;
}
return SVN_NO_ERROR;
}
static svn_error_t *
fs_node_prop(svn_string_t **value_p,
svn_fs_root_t *root,
const char *path,
const char *propname,
apr_pool_t *pool) {
dag_node_t *node;
apr_hash_t *proplist;
SVN_ERR(get_dag(&node, root, path, pool));
SVN_ERR(svn_fs_fs__dag_get_proplist(&proplist, node, pool));
*value_p = NULL;
if (proplist)
*value_p = apr_hash_get(proplist, propname, APR_HASH_KEY_STRING);
return SVN_NO_ERROR;
}
static svn_error_t *
fs_node_proplist(apr_hash_t **table_p,
svn_fs_root_t *root,
const char *path,
apr_pool_t *pool) {
apr_hash_t *table;
dag_node_t *node;
SVN_ERR(get_dag(&node, root, path, pool));
SVN_ERR(svn_fs_fs__dag_get_proplist(&table, node, pool));
*table_p = table ? table : apr_hash_make(pool);
return SVN_NO_ERROR;
}
static svn_error_t *
increment_mergeinfo_up_tree(parent_path_t *pp,
apr_int64_t increment,
apr_pool_t *pool) {
for (; pp; pp = pp->parent)
SVN_ERR(svn_fs_fs__dag_increment_mergeinfo_count(pp->node,
increment,
pool));
return SVN_NO_ERROR;
}
static svn_error_t *
fs_change_node_prop(svn_fs_root_t *root,
const char *path,
const char *name,
const svn_string_t *value,
apr_pool_t *pool) {
parent_path_t *parent_path;
apr_hash_t *proplist;
const char *txn_id;
if (! root->is_txn_root)
return SVN_FS__NOT_TXN(root);
txn_id = root->txn;
SVN_ERR(open_path(&parent_path, root, path, 0, txn_id, pool));
if (root->txn_flags & SVN_FS_TXN_CHECK_LOCKS)
SVN_ERR(svn_fs_fs__allow_locked_operation(path, root->fs, FALSE, FALSE,
pool));
SVN_ERR(make_path_mutable(root, parent_path, path, pool));
SVN_ERR(svn_fs_fs__dag_get_proplist(&proplist, parent_path->node, pool));
if ((! proplist) && (! value))
return SVN_NO_ERROR;
if (! proplist)
proplist = apr_hash_make(pool);
if (svn_fs_fs__fs_supports_mergeinfo(root->fs)
&& strcmp (name, SVN_PROP_MERGEINFO) == 0) {
apr_int64_t increment = 0;
svn_boolean_t had_mergeinfo;
SVN_ERR(svn_fs_fs__dag_has_mergeinfo(&had_mergeinfo, parent_path->node,
pool));
if (value && !had_mergeinfo)
increment = 1;
else if (!value && had_mergeinfo)
increment = -1;
if (increment != 0) {
SVN_ERR(increment_mergeinfo_up_tree(parent_path, increment, pool));
SVN_ERR(svn_fs_fs__dag_set_has_mergeinfo(parent_path->node,
(value != NULL), pool));
}
}
apr_hash_set(proplist, name, APR_HASH_KEY_STRING, value);
SVN_ERR(svn_fs_fs__dag_set_proplist(parent_path->node, proplist,
pool));
SVN_ERR(add_change(root->fs, txn_id, path,
svn_fs_fs__dag_get_id(parent_path->node),
svn_fs_path_change_modify, FALSE, TRUE, SVN_INVALID_REVNUM,
NULL, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
fs_props_changed(svn_boolean_t *changed_p,
svn_fs_root_t *root1,
const char *path1,
svn_fs_root_t *root2,
const char *path2,
apr_pool_t *pool) {
dag_node_t *node1, *node2;
if (root1->fs != root2->fs)
return svn_error_create
(SVN_ERR_FS_GENERAL, NULL,
_("Cannot compare property value between two different filesystems"));
SVN_ERR(get_dag(&node1, root1, path1, pool));
SVN_ERR(get_dag(&node2, root2, path2, pool));
SVN_ERR(svn_fs_fs__dag_things_different(changed_p, NULL,
node1, node2, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
get_root(dag_node_t **node, svn_fs_root_t *root, apr_pool_t *pool) {
SVN_ERR(get_dag(node, root, "", pool));
return SVN_NO_ERROR;
}
static svn_error_t *
update_ancestry(svn_fs_t *fs,
const svn_fs_id_t *source_id,
const svn_fs_id_t *target_id,
const char *target_path,
int source_pred_count,
apr_pool_t *pool) {
node_revision_t *noderev;
if (svn_fs_fs__id_txn_id(target_id) == NULL)
return svn_error_createf
(SVN_ERR_FS_NOT_MUTABLE, NULL,
_("Unexpected immutable node at '%s'"), target_path);
SVN_ERR(svn_fs_fs__get_node_revision(&noderev, fs, target_id, pool));
noderev->predecessor_id = source_id;
noderev->predecessor_count = source_pred_count;
if (noderev->predecessor_count != -1)
noderev->predecessor_count++;
SVN_ERR(svn_fs_fs__put_node_revision(fs, target_id, noderev, FALSE, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
conflict_err(svn_stringbuf_t *conflict_path,
const char *path) {
svn_stringbuf_set(conflict_path, path);
return svn_error_createf(SVN_ERR_FS_CONFLICT, NULL,
_("Conflict at '%s'"), path);
}
static svn_error_t *
merge(svn_stringbuf_t *conflict_p,
const char *target_path,
dag_node_t *target,
dag_node_t *source,
dag_node_t *ancestor,
const char *txn_id,
apr_int64_t *mergeinfo_increment_out,
apr_pool_t *pool) {
const svn_fs_id_t *source_id, *target_id, *ancestor_id;
apr_hash_t *s_entries, *t_entries, *a_entries;
apr_hash_index_t *hi;
svn_fs_t *fs;
apr_pool_t *iterpool;
int pred_count;
apr_int64_t mergeinfo_increment = 0;
fs = svn_fs_fs__dag_get_fs(ancestor);
if ((fs != svn_fs_fs__dag_get_fs(source))
|| (fs != svn_fs_fs__dag_get_fs(target))) {
return svn_error_create
(SVN_ERR_FS_CORRUPT, NULL,
_("Bad merge; ancestor, source, and target not all in same fs"));
}
SVN_ERR(svn_fs__check_fs(fs, TRUE));
source_id = svn_fs_fs__dag_get_id(source);
target_id = svn_fs_fs__dag_get_id(target);
ancestor_id = svn_fs_fs__dag_get_id(ancestor);
if (svn_fs_fs__id_eq(ancestor_id, target_id)) {
svn_string_t *id_str = svn_fs_fs__id_unparse(target_id, pool);
return svn_error_createf
(SVN_ERR_FS_GENERAL, NULL,
_("Bad merge; target '%s' has id '%s', same as ancestor"),
target_path, id_str->data);
}
svn_stringbuf_setempty(conflict_p);
if (svn_fs_fs__id_eq(ancestor_id, source_id)
|| (svn_fs_fs__id_eq(source_id, target_id)))
return SVN_NO_ERROR;
if ((svn_fs_fs__dag_node_kind(source) != svn_node_dir)
|| (svn_fs_fs__dag_node_kind(target) != svn_node_dir)
|| (svn_fs_fs__dag_node_kind(ancestor) != svn_node_dir)) {
return conflict_err(conflict_p, target_path);
}
{
node_revision_t *tgt_nr, *anc_nr, *src_nr;
SVN_ERR(svn_fs_fs__get_node_revision(&tgt_nr, fs, target_id, pool));
SVN_ERR(svn_fs_fs__get_node_revision(&anc_nr, fs, ancestor_id, pool));
SVN_ERR(svn_fs_fs__get_node_revision(&src_nr, fs, source_id, pool));
if (! svn_fs_fs__noderev_same_rep_key(tgt_nr->prop_rep, anc_nr->prop_rep))
return conflict_err(conflict_p, target_path);
if (! svn_fs_fs__noderev_same_rep_key(src_nr->prop_rep, anc_nr->prop_rep))
return conflict_err(conflict_p, target_path);
}
SVN_ERR(svn_fs_fs__dag_dir_entries(&s_entries, source, pool, pool));
SVN_ERR(svn_fs_fs__dag_dir_entries(&t_entries, target, pool, pool));
SVN_ERR(svn_fs_fs__dag_dir_entries(&a_entries, ancestor, pool, pool));
iterpool = svn_pool_create(pool);
for (hi = apr_hash_first(pool, a_entries);
hi;
hi = apr_hash_next(hi)) {
svn_fs_dirent_t *s_entry, *t_entry, *a_entry;
const void *key;
void *val;
apr_ssize_t klen;
svn_pool_clear(iterpool);
apr_hash_this(hi, &key, &klen, &val);
a_entry = val;
s_entry = apr_hash_get(s_entries, key, klen);
t_entry = apr_hash_get(t_entries, key, klen);
if (s_entry && svn_fs_fs__id_eq(a_entry->id, s_entry->id))
goto end;
else if (t_entry && svn_fs_fs__id_eq(a_entry->id, t_entry->id)) {
dag_node_t *t_ent_node;
SVN_ERR(svn_fs_fs__dag_get_node(&t_ent_node, fs,
t_entry->id, iterpool));
if (svn_fs_fs__fs_supports_mergeinfo(fs)) {
apr_int64_t mergeinfo_start;
SVN_ERR(svn_fs_fs__dag_get_mergeinfo_count(&mergeinfo_start,
t_ent_node,
iterpool));
mergeinfo_increment -= mergeinfo_start;
}
if (s_entry) {
dag_node_t *s_ent_node;
SVN_ERR(svn_fs_fs__dag_get_node(&s_ent_node, fs,
s_entry->id, iterpool));
if (svn_fs_fs__fs_supports_mergeinfo(fs)) {
apr_int64_t mergeinfo_end;
SVN_ERR(svn_fs_fs__dag_get_mergeinfo_count(&mergeinfo_end,
s_ent_node,
iterpool));
mergeinfo_increment += mergeinfo_end;
}
SVN_ERR(svn_fs_fs__dag_set_entry(target, key,
s_entry->id,
s_entry->kind,
txn_id,
iterpool));
} else {
SVN_ERR(svn_fs_fs__dag_delete(target, key, txn_id, iterpool));
}
}
else {
dag_node_t *s_ent_node, *t_ent_node, *a_ent_node;
const char *new_tpath;
apr_int64_t sub_mergeinfo_increment;
if (s_entry == NULL || t_entry == NULL)
return conflict_err(conflict_p,
svn_path_join(target_path,
a_entry->name,
iterpool));
if (s_entry->kind == svn_node_file
|| t_entry->kind == svn_node_file
|| a_entry->kind == svn_node_file)
return conflict_err(conflict_p,
svn_path_join(target_path,
a_entry->name,
iterpool));
if (strcmp(svn_fs_fs__id_node_id(s_entry->id),
svn_fs_fs__id_node_id(a_entry->id)) != 0
|| strcmp(svn_fs_fs__id_copy_id(s_entry->id),
svn_fs_fs__id_copy_id(a_entry->id)) != 0
|| strcmp(svn_fs_fs__id_node_id(t_entry->id),
svn_fs_fs__id_node_id(a_entry->id)) != 0
|| strcmp(svn_fs_fs__id_copy_id(t_entry->id),
svn_fs_fs__id_copy_id(a_entry->id)) != 0)
return conflict_err(conflict_p,
svn_path_join(target_path,
a_entry->name,
iterpool));
SVN_ERR(svn_fs_fs__dag_get_node(&s_ent_node, fs,
s_entry->id, iterpool));
SVN_ERR(svn_fs_fs__dag_get_node(&t_ent_node, fs,
t_entry->id, iterpool));
SVN_ERR(svn_fs_fs__dag_get_node(&a_ent_node, fs,
a_entry->id, iterpool));
new_tpath = svn_path_join(target_path, t_entry->name, iterpool);
SVN_ERR(merge(conflict_p, new_tpath,
t_ent_node, s_ent_node, a_ent_node,
txn_id,
&sub_mergeinfo_increment,
iterpool));
if (svn_fs_fs__fs_supports_mergeinfo(fs))
mergeinfo_increment += sub_mergeinfo_increment;
}
end:
apr_hash_set(s_entries, key, klen, NULL);
}
for (hi = apr_hash_first(pool, s_entries);
hi;
hi = apr_hash_next(hi)) {
svn_fs_dirent_t *s_entry, *t_entry;
const void *key;
void *val;
apr_ssize_t klen;
dag_node_t *s_ent_node;
svn_pool_clear(iterpool);
apr_hash_this(hi, &key, &klen, &val);
s_entry = val;
t_entry = apr_hash_get(t_entries, key, klen);
if (t_entry)
return conflict_err(conflict_p,
svn_path_join(target_path,
t_entry->name,
iterpool));
SVN_ERR(svn_fs_fs__dag_get_node(&s_ent_node, fs,
s_entry->id, iterpool));
if (svn_fs_fs__fs_supports_mergeinfo(fs)) {
apr_int64_t mergeinfo_s;
SVN_ERR(svn_fs_fs__dag_get_mergeinfo_count(&mergeinfo_s,
s_ent_node,
iterpool));
mergeinfo_increment += mergeinfo_s;
}
SVN_ERR(svn_fs_fs__dag_set_entry
(target, s_entry->name, s_entry->id, s_entry->kind,
txn_id, iterpool));
}
svn_pool_destroy(iterpool);
SVN_ERR(svn_fs_fs__dag_get_predecessor_count(&pred_count, source, pool));
SVN_ERR(update_ancestry(fs, source_id, target_id, target_path,
pred_count, pool));
if (svn_fs_fs__fs_supports_mergeinfo(fs))
SVN_ERR(svn_fs_fs__dag_increment_mergeinfo_count(target,
mergeinfo_increment,
pool));
if (mergeinfo_increment_out)
*mergeinfo_increment_out = mergeinfo_increment;
return SVN_NO_ERROR;
}
static svn_error_t *
merge_changes(dag_node_t *ancestor_node,
dag_node_t *source_node,
svn_fs_txn_t *txn,
svn_stringbuf_t *conflict,
apr_pool_t *pool) {
dag_node_t *txn_root_node;
const svn_fs_id_t *source_id;
svn_fs_t *fs = txn->fs;
const char *txn_id = txn->id;
source_id = svn_fs_fs__dag_get_id(source_node);
SVN_ERR(svn_fs_fs__dag_txn_root(&txn_root_node, fs, txn_id, pool));
if (ancestor_node == NULL) {
SVN_ERR(svn_fs_fs__dag_txn_base_root(&ancestor_node, fs,
txn_id, pool));
}
if (svn_fs_fs__id_eq(svn_fs_fs__dag_get_id(ancestor_node),
svn_fs_fs__dag_get_id(txn_root_node))) {
abort();
} else
SVN_ERR(merge(conflict, "/", txn_root_node,
source_node, ancestor_node, txn_id, NULL, pool));
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_fs__commit_txn(const char **conflict_p,
svn_revnum_t *new_rev_p,
svn_fs_txn_t *txn,
apr_pool_t *pool) {
svn_error_t *err;
svn_revnum_t new_rev;
svn_fs_t *fs = txn->fs;
new_rev = SVN_INVALID_REVNUM;
if (conflict_p)
*conflict_p = NULL;
while (1729) {
svn_revnum_t youngish_rev;
svn_fs_root_t *youngish_root;
dag_node_t *youngish_root_node;
svn_stringbuf_t *conflict = svn_stringbuf_create("", pool);
SVN_ERR(svn_fs_fs__youngest_rev(&youngish_rev, fs, pool));
SVN_ERR(svn_fs_fs__revision_root(&youngish_root, fs, youngish_rev,
pool));
SVN_ERR(get_root(&youngish_root_node, youngish_root, pool));
err = merge_changes(NULL, youngish_root_node, txn, conflict, pool);
if (err) {
if ((err->apr_err == SVN_ERR_FS_CONFLICT) && conflict_p)
*conflict_p = conflict->data;
return err;
}
txn->base_rev = youngish_rev;
err = svn_fs_fs__commit(&new_rev, fs, txn, pool);
if (err && (err->apr_err == SVN_ERR_FS_TXN_OUT_OF_DATE)) {
svn_revnum_t youngest_rev;
SVN_ERR(svn_fs_fs__youngest_rev(&youngest_rev, fs, pool));
if (youngest_rev == youngish_rev)
return err;
else
svn_error_clear(err);
} else if (err) {
return err;
} else {
*new_rev_p = new_rev;
return SVN_NO_ERROR;
}
}
}
static svn_error_t *
fs_merge(const char **conflict_p,
svn_fs_root_t *source_root,
const char *source_path,
svn_fs_root_t *target_root,
const char *target_path,
svn_fs_root_t *ancestor_root,
const char *ancestor_path,
apr_pool_t *pool) {
dag_node_t *source, *ancestor;
svn_fs_txn_t *txn;
svn_error_t *err;
svn_stringbuf_t *conflict = svn_stringbuf_create("", pool);
if (! target_root->is_txn_root)
return SVN_FS__NOT_TXN(target_root);
if ((source_root->fs != ancestor_root->fs)
|| (target_root->fs != ancestor_root->fs)) {
return svn_error_create
(SVN_ERR_FS_CORRUPT, NULL,
_("Bad merge; ancestor, source, and target not all in same fs"));
}
SVN_ERR(get_root(&ancestor, ancestor_root, pool));
SVN_ERR(get_root(&source, source_root, pool));
SVN_ERR(svn_fs_fs__open_txn(&txn, ancestor_root->fs, target_root->txn,
pool));
err = merge_changes(ancestor, source, txn, conflict, pool);
if (err) {
if ((err->apr_err == SVN_ERR_FS_CONFLICT) && conflict_p)
*conflict_p = conflict->data;
return err;
}
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_fs__deltify(svn_fs_t *fs,
svn_revnum_t revision,
apr_pool_t *pool) {
return SVN_NO_ERROR;
}
static svn_error_t *
fs_dir_entries(apr_hash_t **table_p,
svn_fs_root_t *root,
const char *path,
apr_pool_t *pool) {
dag_node_t *node;
SVN_ERR(get_dag(&node, root, path, pool));
SVN_ERR(svn_fs_fs__dag_dir_entries(table_p, node, pool, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
fs_make_dir(svn_fs_root_t *root,
const char *path,
apr_pool_t *pool) {
parent_path_t *parent_path;
dag_node_t *sub_dir;
const char *txn_id = root->txn;
SVN_ERR(open_path(&parent_path, root, path, open_path_last_optional,
txn_id, pool));
if (root->txn_flags & SVN_FS_TXN_CHECK_LOCKS)
SVN_ERR(svn_fs_fs__allow_locked_operation(path, root->fs, TRUE, FALSE,
pool));
if (parent_path->node)
return SVN_FS__ALREADY_EXISTS(root, path);
SVN_ERR(make_path_mutable(root, parent_path->parent, path, pool));
SVN_ERR(svn_fs_fs__dag_make_dir(&sub_dir,
parent_path->parent->node,
parent_path_path(parent_path->parent,
pool),
parent_path->entry,
txn_id,
pool));
dag_node_cache_set(root, parent_path_path(parent_path, pool), sub_dir, pool);
SVN_ERR(add_change(root->fs, txn_id, path, svn_fs_fs__dag_get_id(sub_dir),
svn_fs_path_change_add, FALSE, FALSE, SVN_INVALID_REVNUM,
NULL, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
fs_delete_node(svn_fs_root_t *root,
const char *path,
apr_pool_t *pool) {
parent_path_t *parent_path;
const char *txn_id = root->txn;
apr_int64_t mergeinfo_count;
if (! root->is_txn_root)
return SVN_FS__NOT_TXN(root);
SVN_ERR(open_path(&parent_path, root, path, 0, txn_id, pool));
if (! parent_path->parent)
return svn_error_create(SVN_ERR_FS_ROOT_DIR, NULL,
_("The root directory cannot be deleted"));
if (root->txn_flags & SVN_FS_TXN_CHECK_LOCKS)
SVN_ERR(svn_fs_fs__allow_locked_operation(path, root->fs, TRUE, FALSE,
pool));
SVN_ERR(make_path_mutable(root, parent_path->parent, path, pool));
if (svn_fs_fs__fs_supports_mergeinfo(root->fs))
SVN_ERR(svn_fs_fs__dag_get_mergeinfo_count(&mergeinfo_count,
parent_path->node,
pool));
SVN_ERR(svn_fs_fs__dag_delete(parent_path->parent->node,
parent_path->entry,
txn_id, pool));
dag_node_cache_invalidate(root, parent_path_path(parent_path, pool));
if (svn_fs_fs__fs_supports_mergeinfo(root->fs) && mergeinfo_count > 0)
SVN_ERR(increment_mergeinfo_up_tree(parent_path->parent,
-mergeinfo_count,
pool));
SVN_ERR(add_change(root->fs, txn_id, path,
svn_fs_fs__dag_get_id(parent_path->node),
svn_fs_path_change_delete, FALSE, FALSE,
SVN_INVALID_REVNUM, NULL, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
fs_same_p(svn_boolean_t *same_p,
svn_fs_t *fs1,
svn_fs_t *fs2,
apr_pool_t *pool) {
const char *uuid1;
const char *uuid2;
SVN_ERR(fs1->vtable->get_uuid(fs1, &uuid1, pool));
SVN_ERR(fs2->vtable->get_uuid(fs2, &uuid2, pool));
*same_p = ! strcmp(uuid1, uuid2);
return SVN_NO_ERROR;
}
static svn_error_t *
copy_helper(svn_fs_root_t *from_root,
const char *from_path,
svn_fs_root_t *to_root,
const char *to_path,
svn_boolean_t preserve_history,
apr_pool_t *pool) {
dag_node_t *from_node;
parent_path_t *to_parent_path;
const char *txn_id = to_root->txn;
svn_boolean_t same_p;
SVN_ERR(fs_same_p(&same_p, from_root->fs, to_root->fs, pool));
if (! same_p)
return svn_error_createf
(SVN_ERR_UNSUPPORTED_FEATURE, NULL,
_("Cannot copy between two different filesystems ('%s' and '%s')"),
from_root->fs->path, to_root->fs->path);
if (from_root->is_txn_root)
return svn_error_create
(SVN_ERR_UNSUPPORTED_FEATURE, NULL,
_("Copy from mutable tree not currently supported"));
SVN_ERR(get_dag(&from_node, from_root, from_path, pool));
SVN_ERR(open_path(&to_parent_path, to_root, to_path,
open_path_last_optional, txn_id, pool));
if (to_root->txn_flags & SVN_FS_TXN_CHECK_LOCKS)
SVN_ERR(svn_fs_fs__allow_locked_operation(to_path, to_root->fs,
TRUE, FALSE, pool));
if (to_parent_path->node &&
svn_fs_fs__id_eq(svn_fs_fs__dag_get_id(from_node),
svn_fs_fs__dag_get_id(to_parent_path->node)))
return SVN_NO_ERROR;
if (! from_root->is_txn_root) {
svn_fs_path_change_kind_t kind;
dag_node_t *new_node;
const char *from_canonpath;
apr_int64_t mergeinfo_start;
apr_int64_t mergeinfo_end;
if (to_parent_path->node) {
kind = svn_fs_path_change_replace;
if (svn_fs_fs__fs_supports_mergeinfo(to_root->fs))
SVN_ERR(svn_fs_fs__dag_get_mergeinfo_count(&mergeinfo_start,
to_parent_path->node,
pool));
} else {
kind = svn_fs_path_change_add;
mergeinfo_start = 0;
}
if (svn_fs_fs__fs_supports_mergeinfo(to_root->fs))
SVN_ERR(svn_fs_fs__dag_get_mergeinfo_count(&mergeinfo_end,
from_node, pool));
SVN_ERR(make_path_mutable(to_root, to_parent_path->parent,
to_path, pool));
from_canonpath = svn_fs__canonicalize_abspath(from_path, pool);
SVN_ERR(svn_fs_fs__dag_copy(to_parent_path->parent->node,
to_parent_path->entry,
from_node,
preserve_history,
from_root->rev,
from_canonpath,
txn_id, pool));
if (kind == svn_fs_path_change_replace)
dag_node_cache_invalidate(to_root, parent_path_path(to_parent_path,
pool));
if (svn_fs_fs__fs_supports_mergeinfo(to_root->fs)
&& mergeinfo_start != mergeinfo_end)
SVN_ERR(increment_mergeinfo_up_tree(to_parent_path->parent,
mergeinfo_end - mergeinfo_start,
pool));
SVN_ERR(get_dag(&new_node, to_root, to_path, pool));
SVN_ERR(add_change(to_root->fs, txn_id, to_path,
svn_fs_fs__dag_get_id(new_node), kind, FALSE, FALSE,
from_root->rev, from_canonpath, pool));
} else {
abort();
}
return SVN_NO_ERROR;
}
static svn_error_t *
fs_copy(svn_fs_root_t *from_root,
const char *from_path,
svn_fs_root_t *to_root,
const char *to_path,
apr_pool_t *pool) {
return copy_helper(from_root, from_path, to_root, to_path, TRUE, pool);
}
static svn_error_t *
fs_revision_link(svn_fs_root_t *from_root,
svn_fs_root_t *to_root,
const char *path,
apr_pool_t *pool) {
if (! to_root->is_txn_root)
return SVN_FS__NOT_TXN(to_root);
return copy_helper(from_root, path, to_root, path, FALSE, pool);
}
static svn_error_t *
fs_copied_from(svn_revnum_t *rev_p,
const char **path_p,
svn_fs_root_t *root,
const char *path,
apr_pool_t *pool) {
dag_node_t *node;
const char *copyfrom_path, *copyfrom_str = NULL;
svn_revnum_t copyfrom_rev;
char *str, *last_str, *buf;
if (! root->is_txn_root) {
fs_rev_root_data_t *frd = root->fsap_data;
copyfrom_str = apr_hash_get(frd->copyfrom_cache, path, APR_HASH_KEY_STRING);
}
if (copyfrom_str) {
if (strlen(copyfrom_str) == 0) {
copyfrom_rev = SVN_INVALID_REVNUM;
copyfrom_path = NULL;
} else {
buf = apr_pstrdup(pool, copyfrom_str);
str = apr_strtok(buf, " ", &last_str);
copyfrom_rev = atol(str);
copyfrom_path = last_str;
}
} else {
SVN_ERR(get_dag(&node, root, path, pool));
SVN_ERR(svn_fs_fs__dag_get_copyfrom_rev(&copyfrom_rev, node, pool));
SVN_ERR(svn_fs_fs__dag_get_copyfrom_path(&copyfrom_path, node, pool));
}
*rev_p = copyfrom_rev;
*path_p = copyfrom_path;
return SVN_NO_ERROR;
}
static svn_error_t *
fs_make_file(svn_fs_root_t *root,
const char *path,
apr_pool_t *pool) {
parent_path_t *parent_path;
dag_node_t *child;
const char *txn_id = root->txn;
SVN_ERR(open_path(&parent_path, root, path, open_path_last_optional,
txn_id, pool));
if (parent_path->node)
return SVN_FS__ALREADY_EXISTS(root, path);
if (root->txn_flags & SVN_FS_TXN_CHECK_LOCKS)
SVN_ERR(svn_fs_fs__allow_locked_operation(path, root->fs, FALSE, FALSE,
pool));
SVN_ERR(make_path_mutable(root, parent_path->parent, path, pool));
SVN_ERR(svn_fs_fs__dag_make_file(&child,
parent_path->parent->node,
parent_path_path(parent_path->parent,
pool),
parent_path->entry,
txn_id,
pool));
dag_node_cache_set(root, parent_path_path(parent_path, pool), child, pool);
SVN_ERR(add_change(root->fs, txn_id, path, svn_fs_fs__dag_get_id(child),
svn_fs_path_change_add, TRUE, FALSE, SVN_INVALID_REVNUM,
NULL, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
fs_file_length(svn_filesize_t *length_p,
svn_fs_root_t *root,
const char *path,
apr_pool_t *pool) {
dag_node_t *file;
SVN_ERR(get_dag(&file, root, path, pool));
SVN_ERR(svn_fs_fs__dag_file_length(length_p, file, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
fs_file_md5_checksum(unsigned char digest[],
svn_fs_root_t *root,
const char *path,
apr_pool_t *pool) {
dag_node_t *file;
SVN_ERR(get_dag(&file, root, path, pool));
return svn_fs_fs__dag_file_checksum(digest, file, pool);
}
static svn_error_t *
fs_file_contents(svn_stream_t **contents,
svn_fs_root_t *root,
const char *path,
apr_pool_t *pool) {
dag_node_t *node;
svn_stream_t *file_stream;
SVN_ERR(get_dag(&node, root, path, pool));
SVN_ERR(svn_fs_fs__dag_get_contents(&file_stream, node, pool));
*contents = file_stream;
return SVN_NO_ERROR;
}
typedef struct txdelta_baton_t {
svn_txdelta_window_handler_t interpreter;
void *interpreter_baton;
svn_fs_root_t *root;
const char *path;
dag_node_t *node;
svn_stream_t *source_stream;
svn_stream_t *target_stream;
svn_stream_t *string_stream;
svn_stringbuf_t *target_string;
const char *base_checksum;
const char *result_checksum;
apr_pool_t *pool;
} txdelta_baton_t;
static svn_error_t *
write_to_string(void *baton, const char *data, apr_size_t *len) {
txdelta_baton_t *tb = (txdelta_baton_t *) baton;
svn_stringbuf_appendbytes(tb->target_string, data, *len);
return SVN_NO_ERROR;
}
static svn_error_t *
window_consumer(svn_txdelta_window_t *window, void *baton) {
txdelta_baton_t *tb = (txdelta_baton_t *) baton;
SVN_ERR(tb->interpreter(window, tb->interpreter_baton));
if ((! window) || (tb->target_string->len > WRITE_BUFFER_SIZE)) {
apr_size_t len = tb->target_string->len;
SVN_ERR(svn_stream_write(tb->target_stream,
tb->target_string->data,
&len));
svn_stringbuf_set(tb->target_string, "");
}
if (! window) {
SVN_ERR(svn_stream_close(tb->target_stream));
SVN_ERR(svn_fs_fs__dag_finalize_edits(tb->node, tb->result_checksum,
tb->pool));
}
return SVN_NO_ERROR;
}
static svn_error_t *
apply_textdelta(void *baton, apr_pool_t *pool) {
txdelta_baton_t *tb = (txdelta_baton_t *) baton;
parent_path_t *parent_path;
const char *txn_id = tb->root->txn;
SVN_ERR(open_path(&parent_path, tb->root, tb->path, 0, txn_id, pool));
if (tb->root->txn_flags & SVN_FS_TXN_CHECK_LOCKS)
SVN_ERR(svn_fs_fs__allow_locked_operation(tb->path, tb->root->fs,
FALSE, FALSE, pool));
SVN_ERR(make_path_mutable(tb->root, parent_path, tb->path, pool));
tb->node = parent_path->node;
if (tb->base_checksum) {
unsigned char digest[APR_MD5_DIGESTSIZE];
const char *hex;
SVN_ERR(svn_fs_fs__dag_file_checksum(digest, tb->node, pool));
hex = svn_md5_digest_to_cstring(digest, pool);
if (hex && (strcmp(tb->base_checksum, hex) != 0))
return svn_error_createf
(SVN_ERR_CHECKSUM_MISMATCH,
NULL,
_("Base checksum mismatch on '%s':\n"
" expected: %s\n"
" actual: %s\n"),
tb->path, tb->base_checksum, hex);
}
SVN_ERR(svn_fs_fs__dag_get_contents(&(tb->source_stream),
tb->node, tb->pool));
SVN_ERR(svn_fs_fs__dag_get_edit_stream(&(tb->target_stream), tb->node,
tb->pool));
tb->target_string = svn_stringbuf_create("", tb->pool);
tb->string_stream = svn_stream_create(tb, tb->pool);
svn_stream_set_write(tb->string_stream, write_to_string);
svn_txdelta_apply(tb->source_stream,
tb->string_stream,
NULL,
tb->path,
tb->pool,
&(tb->interpreter),
&(tb->interpreter_baton));
SVN_ERR(add_change(tb->root->fs, txn_id, tb->path,
svn_fs_fs__dag_get_id(tb->node),
svn_fs_path_change_modify, TRUE, FALSE, SVN_INVALID_REVNUM,
NULL, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
fs_apply_textdelta(svn_txdelta_window_handler_t *contents_p,
void **contents_baton_p,
svn_fs_root_t *root,
const char *path,
const char *base_checksum,
const char *result_checksum,
apr_pool_t *pool) {
txdelta_baton_t *tb = apr_pcalloc(pool, sizeof(*tb));
tb->root = root;
tb->path = path;
tb->pool = pool;
if (base_checksum)
tb->base_checksum = apr_pstrdup(pool, base_checksum);
else
tb->base_checksum = NULL;
if (result_checksum)
tb->result_checksum = apr_pstrdup(pool, result_checksum);
else
tb->result_checksum = NULL;
SVN_ERR(apply_textdelta(tb, pool));
*contents_p = window_consumer;
*contents_baton_p = tb;
return SVN_NO_ERROR;
}
struct text_baton_t {
svn_fs_root_t *root;
const char *path;
dag_node_t *node;
svn_stream_t *stream;
svn_stream_t *file_stream;
const char *result_checksum;
apr_pool_t *pool;
};
static svn_error_t *
text_stream_writer(void *baton,
const char *data,
apr_size_t *len) {
struct text_baton_t *tb = baton;
return svn_stream_write(tb->file_stream, data, len);
}
static svn_error_t *
text_stream_closer(void *baton) {
struct text_baton_t *tb = baton;
SVN_ERR(svn_stream_close(tb->file_stream));
SVN_ERR(svn_fs_fs__dag_finalize_edits(tb->node, tb->result_checksum,
tb->pool));
return SVN_NO_ERROR;
}
static svn_error_t *
apply_text(void *baton, apr_pool_t *pool) {
struct text_baton_t *tb = baton;
parent_path_t *parent_path;
const char *txn_id = tb->root->txn;
SVN_ERR(open_path(&parent_path, tb->root, tb->path, 0, txn_id, pool));
if (tb->root->txn_flags & SVN_FS_TXN_CHECK_LOCKS)
SVN_ERR(svn_fs_fs__allow_locked_operation(tb->path, tb->root->fs,
FALSE, FALSE, pool));
SVN_ERR(make_path_mutable(tb->root, parent_path, tb->path, pool));
tb->node = parent_path->node;
SVN_ERR(svn_fs_fs__dag_get_edit_stream(&(tb->file_stream), tb->node,
tb->pool));
tb->stream = svn_stream_create(tb, tb->pool);
svn_stream_set_write(tb->stream, text_stream_writer);
svn_stream_set_close(tb->stream, text_stream_closer);
SVN_ERR(add_change(tb->root->fs, txn_id, tb->path,
svn_fs_fs__dag_get_id(tb->node),
svn_fs_path_change_modify, TRUE, FALSE, SVN_INVALID_REVNUM,
NULL, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
fs_apply_text(svn_stream_t **contents_p,
svn_fs_root_t *root,
const char *path,
const char *result_checksum,
apr_pool_t *pool) {
struct text_baton_t *tb = apr_pcalloc(pool, sizeof(*tb));
tb->root = root;
tb->path = path;
tb->pool = pool;
if (result_checksum)
tb->result_checksum = apr_pstrdup(pool, result_checksum);
else
tb->result_checksum = NULL;
SVN_ERR(apply_text(tb, pool));
*contents_p = tb->stream;
return SVN_NO_ERROR;
}
static svn_error_t *
fs_contents_changed(svn_boolean_t *changed_p,
svn_fs_root_t *root1,
const char *path1,
svn_fs_root_t *root2,
const char *path2,
apr_pool_t *pool) {
dag_node_t *node1, *node2;
if (root1->fs != root2->fs)
return svn_error_create
(SVN_ERR_FS_GENERAL, NULL,
_("Cannot compare file contents between two different filesystems"));
{
svn_node_kind_t kind;
SVN_ERR(svn_fs_fs__check_path(&kind, root1, path1, pool));
if (kind != svn_node_file)
return svn_error_createf
(SVN_ERR_FS_GENERAL, NULL, _("'%s' is not a file"), path1);
SVN_ERR(svn_fs_fs__check_path(&kind, root2, path2, pool));
if (kind != svn_node_file)
return svn_error_createf
(SVN_ERR_FS_GENERAL, NULL, _("'%s' is not a file"), path2);
}
SVN_ERR(get_dag(&node1, root1, path1, pool));
SVN_ERR(get_dag(&node2, root2, path2, pool));
SVN_ERR(svn_fs_fs__dag_things_different(NULL, changed_p,
node1, node2, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
fs_get_file_delta_stream(svn_txdelta_stream_t **stream_p,
svn_fs_root_t *source_root,
const char *source_path,
svn_fs_root_t *target_root,
const char *target_path,
apr_pool_t *pool) {
dag_node_t *source_node, *target_node;
if (source_root && source_path)
SVN_ERR(get_dag(&source_node, source_root, source_path, pool));
else
source_node = NULL;
SVN_ERR(get_dag(&target_node, target_root, target_path, pool));
SVN_ERR(svn_fs_fs__dag_get_file_delta_stream(stream_p, source_node,
target_node, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
fs_paths_changed(apr_hash_t **changed_paths_p,
svn_fs_root_t *root,
apr_pool_t *pool) {
if (root->is_txn_root)
return svn_fs_fs__txn_changes_fetch(changed_paths_p, root->fs, root->txn,
NULL, pool);
else {
fs_rev_root_data_t *frd = root->fsap_data;
return svn_fs_fs__paths_changed(changed_paths_p, root->fs, root->rev,
frd->copyfrom_cache, pool);
}
}
typedef struct {
svn_fs_t *fs;
const char *path;
svn_revnum_t revision;
const char *path_hint;
svn_revnum_t rev_hint;
svn_boolean_t is_interesting;
} fs_history_data_t;
static svn_fs_history_t *
assemble_history(svn_fs_t *fs,
const char *path,
svn_revnum_t revision,
svn_boolean_t is_interesting,
const char *path_hint,
svn_revnum_t rev_hint,
apr_pool_t *pool);
static svn_error_t *
fs_node_history(svn_fs_history_t **history_p,
svn_fs_root_t *root,
const char *path,
apr_pool_t *pool) {
svn_node_kind_t kind;
if (root->is_txn_root)
return svn_error_create(SVN_ERR_FS_NOT_REVISION_ROOT, NULL, NULL);
SVN_ERR(svn_fs_fs__check_path(&kind, root, path, pool));
if (kind == svn_node_none)
return SVN_FS__NOT_FOUND(root, path);
*history_p = assemble_history(root->fs,
svn_fs__canonicalize_abspath(path, pool),
root->rev, FALSE, NULL,
SVN_INVALID_REVNUM, pool);
return SVN_NO_ERROR;
}
static svn_error_t *
find_youngest_copyroot(svn_revnum_t *rev_p,
const char **path_p,
svn_fs_t *fs,
parent_path_t *parent_path,
apr_pool_t *pool) {
svn_revnum_t rev_mine, rev_parent = -1;
const char *path_mine, *path_parent;
if (parent_path->parent)
SVN_ERR(find_youngest_copyroot(&rev_parent, &path_parent, fs,
parent_path->parent, pool));
SVN_ERR(svn_fs_fs__dag_get_copyroot(&rev_mine, &path_mine,
parent_path->node, pool));
if (rev_mine >= rev_parent) {
*rev_p = rev_mine;
*path_p = path_mine;
} else {
*rev_p = rev_parent;
*path_p = path_parent;
}
return SVN_NO_ERROR;
}
static svn_error_t *fs_closest_copy(svn_fs_root_t **root_p,
const char **path_p,
svn_fs_root_t *root,
const char *path,
apr_pool_t *pool) {
svn_fs_t *fs = root->fs;
parent_path_t *parent_path, *copy_dst_parent_path;
svn_revnum_t copy_dst_rev, created_rev;
const char *copy_dst_path;
svn_fs_root_t *copy_dst_root;
dag_node_t *copy_dst_node;
svn_node_kind_t kind;
*root_p = NULL;
*path_p = NULL;
SVN_ERR(open_path(&parent_path, root, path, 0, NULL, pool));
SVN_ERR(find_youngest_copyroot(&copy_dst_rev, &copy_dst_path,
fs, parent_path, pool));
if (copy_dst_rev == 0)
return SVN_NO_ERROR;
SVN_ERR(svn_fs_fs__revision_root(&copy_dst_root, fs, copy_dst_rev, pool));
SVN_ERR(svn_fs_fs__check_path(&kind, copy_dst_root, path, pool));
if (kind == svn_node_none)
return SVN_NO_ERROR;
SVN_ERR(open_path(&copy_dst_parent_path, copy_dst_root, path,
0, NULL, pool));
copy_dst_node = copy_dst_parent_path->node;
if (! svn_fs_fs__id_check_related(svn_fs_fs__dag_get_id(copy_dst_node),
svn_fs_fs__dag_get_id(parent_path->node)))
return SVN_NO_ERROR;
SVN_ERR(svn_fs_fs__dag_get_revision(&created_rev, copy_dst_node, pool));
if (created_rev == copy_dst_rev) {
const svn_fs_id_t *pred;
SVN_ERR(svn_fs_fs__dag_get_predecessor_id(&pred, copy_dst_node, pool));
if (! pred)
return SVN_NO_ERROR;
}
*root_p = copy_dst_root;
*path_p = copy_dst_path;
return SVN_NO_ERROR;
}
static svn_error_t *
prev_location(const char **prev_path,
svn_revnum_t *prev_rev,
svn_fs_t *fs,
svn_fs_root_t *root,
const char *path,
apr_pool_t *pool) {
const char *copy_path, *copy_src_path, *remainder = "";
svn_fs_root_t *copy_root;
svn_revnum_t copy_src_rev;
SVN_ERR(fs_closest_copy(&copy_root, &copy_path, root, path, pool));
if (! copy_root) {
*prev_rev = SVN_INVALID_REVNUM;
*prev_path = NULL;
return SVN_NO_ERROR;
}
SVN_ERR(fs_copied_from(&copy_src_rev, &copy_src_path,
copy_root, copy_path, pool));
if (strcmp(copy_path, path) != 0)
remainder = svn_path_is_child(copy_path, path, pool);
*prev_path = svn_path_join(copy_src_path, remainder, pool);
*prev_rev = copy_src_rev;
return SVN_NO_ERROR;
}
static svn_error_t *
fs_node_origin_rev(svn_revnum_t *revision,
svn_fs_root_t *root,
const char *path,
apr_pool_t *pool) {
svn_fs_t *fs = root->fs;
const svn_fs_id_t *given_noderev_id, *cached_origin_id;
const char *node_id, *dash;
path = svn_fs__canonicalize_abspath(path, pool);
SVN_ERR(fs_node_id(&given_noderev_id, root, path, pool));
node_id = svn_fs_fs__id_node_id(given_noderev_id);
if (node_id[0] == '_') {
*revision = SVN_INVALID_REVNUM;
return SVN_NO_ERROR;
}
dash = strchr(node_id, '-');
if (dash && *(dash+1)) {
*revision = SVN_STR_TO_REV(dash + 1);
return SVN_NO_ERROR;
}
SVN_ERR(svn_fs_fs__get_node_origin(&cached_origin_id,
fs,
node_id,
pool));
if (cached_origin_id != NULL) {
*revision = svn_fs_fs__id_rev(cached_origin_id);
return SVN_NO_ERROR;
}
{
svn_fs_root_t *curroot = root;
apr_pool_t *subpool = svn_pool_create(pool);
apr_pool_t *predidpool = svn_pool_create(pool);
svn_stringbuf_t *lastpath = svn_stringbuf_create(path, pool);
svn_revnum_t lastrev = SVN_INVALID_REVNUM;
dag_node_t *node;
const svn_fs_id_t *pred_id;
while (1) {
svn_revnum_t currev;
const char *curpath = lastpath->data;
svn_pool_clear(subpool);
if (SVN_IS_VALID_REVNUM(lastrev))
SVN_ERR(svn_fs_fs__revision_root(&curroot, fs, lastrev, subpool));
SVN_ERR(prev_location(&curpath, &currev, fs, curroot, curpath,
subpool));
if (! curpath)
break;
svn_stringbuf_set(lastpath, curpath);
lastrev = currev;
}
SVN_ERR(fs_node_id(&pred_id, curroot, lastpath->data, predidpool));
while (pred_id) {
svn_pool_clear(subpool);
SVN_ERR(svn_fs_fs__dag_get_node(&node, fs, pred_id, subpool));
svn_pool_clear(predidpool);
SVN_ERR(svn_fs_fs__dag_get_predecessor_id(&pred_id, node,
predidpool));
}
SVN_ERR(svn_fs_fs__dag_get_revision(revision, node, pool));
if (node_id[0] != '_')
SVN_ERR(svn_fs_fs__set_node_origin(fs, node_id,
svn_fs_fs__dag_get_id(node), pool));
svn_pool_destroy(subpool);
svn_pool_destroy(predidpool);
return SVN_NO_ERROR;
}
}
struct history_prev_args {
svn_fs_history_t **prev_history_p;
svn_fs_history_t *history;
svn_boolean_t cross_copies;
apr_pool_t *pool;
};
static svn_error_t *
history_prev(void *baton, apr_pool_t *pool) {
struct history_prev_args *args = baton;
svn_fs_history_t **prev_history = args->prev_history_p;
svn_fs_history_t *history = args->history;
fs_history_data_t *fhd = history->fsap_data;
const char *commit_path, *src_path, *path = fhd->path;
svn_revnum_t commit_rev, src_rev, dst_rev;
svn_revnum_t revision = fhd->revision;
apr_pool_t *retpool = args->pool;
svn_fs_t *fs = fhd->fs;
parent_path_t *parent_path;
dag_node_t *node;
svn_fs_root_t *root;
const svn_fs_id_t *node_id;
svn_boolean_t reported = fhd->is_interesting;
svn_boolean_t retry = FALSE;
svn_revnum_t copyroot_rev;
const char *copyroot_path;
*prev_history = NULL;
if (fhd->path_hint && SVN_IS_VALID_REVNUM(fhd->rev_hint)) {
reported = FALSE;
if (! args->cross_copies)
return SVN_NO_ERROR;
path = fhd->path_hint;
revision = fhd->rev_hint;
}
SVN_ERR(svn_fs_fs__revision_root(&root, fs, revision, pool));
SVN_ERR(open_path(&parent_path, root, path, 0, NULL, pool));
node = parent_path->node;
node_id = svn_fs_fs__dag_get_id(node);
commit_path = svn_fs_fs__dag_get_created_path(node);
SVN_ERR(svn_fs_fs__dag_get_revision(&commit_rev, node, pool));
if (revision == commit_rev) {
if (! reported) {
*prev_history = assemble_history(fs,
apr_pstrdup(retpool, commit_path),
commit_rev, TRUE, NULL,
SVN_INVALID_REVNUM, retpool);
return SVN_NO_ERROR;
} else {
const svn_fs_id_t *pred_id;
SVN_ERR(svn_fs_fs__dag_get_predecessor_id(&pred_id, node, pool));
if (! pred_id)
return SVN_NO_ERROR;
SVN_ERR(svn_fs_fs__dag_get_node(&node, fs, pred_id, pool));
node_id = svn_fs_fs__dag_get_id(node);
commit_path = svn_fs_fs__dag_get_created_path(node);
SVN_ERR(svn_fs_fs__dag_get_revision(&commit_rev, node, pool));
}
}
SVN_ERR(find_youngest_copyroot(&copyroot_rev, &copyroot_path, fs,
parent_path, pool));
src_path = NULL;
src_rev = SVN_INVALID_REVNUM;
dst_rev = SVN_INVALID_REVNUM;
if (copyroot_rev > commit_rev) {
const char *remainder;
const char *copy_dst, *copy_src;
svn_fs_root_t *copyroot_root;
SVN_ERR(svn_fs_fs__revision_root(&copyroot_root, fs, copyroot_rev,
pool));
SVN_ERR(get_dag(&node, copyroot_root, copyroot_path, pool));
copy_dst = svn_fs_fs__dag_get_created_path(node);
if (strcmp(path, copy_dst) == 0)
remainder = "";
else
remainder = svn_path_is_child(copy_dst, path, pool);
if (remainder) {
SVN_ERR(svn_fs_fs__dag_get_copyfrom_rev(&src_rev, node, pool));
SVN_ERR(svn_fs_fs__dag_get_copyfrom_path(&copy_src, node, pool));
dst_rev = copyroot_rev;
src_path = svn_path_join(copy_src, remainder, pool);
}
}
if (src_path && SVN_IS_VALID_REVNUM(src_rev)) {
if ((dst_rev == revision) && reported)
retry = TRUE;
*prev_history = assemble_history(fs, apr_pstrdup(retpool, path),
dst_rev, retry ? FALSE : TRUE,
src_path, src_rev, retpool);
} else {
*prev_history = assemble_history(fs, apr_pstrdup(retpool, commit_path),
commit_rev, TRUE, NULL,
SVN_INVALID_REVNUM, retpool);
}
return SVN_NO_ERROR;
}
static svn_error_t *
fs_history_prev(svn_fs_history_t **prev_history_p,
svn_fs_history_t *history,
svn_boolean_t cross_copies,
apr_pool_t *pool) {
svn_fs_history_t *prev_history = NULL;
fs_history_data_t *fhd = history->fsap_data;
svn_fs_t *fs = fhd->fs;
if (strcmp(fhd->path, "/") == 0) {
if (! fhd->is_interesting)
prev_history = assemble_history(fs, "/", fhd->revision,
1, NULL, SVN_INVALID_REVNUM, pool);
else if (fhd->revision > 0)
prev_history = assemble_history(fs, "/", fhd->revision - 1,
1, NULL, SVN_INVALID_REVNUM, pool);
} else {
struct history_prev_args args;
prev_history = history;
while (1) {
args.prev_history_p = &prev_history;
args.history = prev_history;
args.cross_copies = cross_copies;
args.pool = pool;
SVN_ERR(history_prev(&args, pool));
if (! prev_history)
break;
fhd = prev_history->fsap_data;
if (fhd->is_interesting)
break;
}
}
*prev_history_p = prev_history;
return SVN_NO_ERROR;
}
static svn_error_t *
fs_history_location(const char **path,
svn_revnum_t *revision,
svn_fs_history_t *history,
apr_pool_t *pool) {
fs_history_data_t *fhd = history->fsap_data;
*path = apr_pstrdup(pool, fhd->path);
*revision = fhd->revision;
return SVN_NO_ERROR;
}
static history_vtable_t history_vtable = {
fs_history_prev,
fs_history_location
};
static svn_fs_history_t *
assemble_history(svn_fs_t *fs,
const char *path,
svn_revnum_t revision,
svn_boolean_t is_interesting,
const char *path_hint,
svn_revnum_t rev_hint,
apr_pool_t *pool) {
svn_fs_history_t *history = apr_pcalloc(pool, sizeof(*history));
fs_history_data_t *fhd = apr_pcalloc(pool, sizeof(*fhd));
fhd->path = path;
fhd->revision = revision;
fhd->is_interesting = is_interesting;
fhd->path_hint = path_hint;
fhd->rev_hint = rev_hint;
fhd->fs = fs;
history->vtable = &history_vtable;
history->fsap_data = fhd;
return history;
}
static svn_error_t *
crawl_directory_dag_for_mergeinfo(svn_fs_root_t *root,
const char *this_path,
dag_node_t *dir_dag,
svn_mergeinfo_catalog_t result_catalog,
apr_pool_t *pool,
apr_pool_t *result_pool) {
apr_hash_t *entries;
apr_hash_index_t *hi;
apr_pool_t *iterpool = svn_pool_create(pool);
SVN_ERR(svn_fs_fs__dag_dir_entries(&entries, dir_dag, pool, pool));
for (hi = apr_hash_first(pool, entries);
hi;
hi = apr_hash_next(hi)) {
void *val;
svn_fs_dirent_t *dirent;
const char *kid_path;
dag_node_t *kid_dag;
svn_boolean_t has_mergeinfo, go_down;
svn_pool_clear(iterpool);
apr_hash_this(hi, NULL, NULL, &val);
dirent = val;
kid_path = svn_path_join(this_path, dirent->name, iterpool);
SVN_ERR(get_dag(&kid_dag, root, kid_path, iterpool));
SVN_ERR(svn_fs_fs__dag_has_mergeinfo(&has_mergeinfo, kid_dag, iterpool));
SVN_ERR(svn_fs_fs__dag_has_descendants_with_mergeinfo(&go_down, kid_dag,
iterpool));
if (has_mergeinfo) {
apr_hash_t *proplist;
svn_mergeinfo_t kid_mergeinfo;
svn_string_t *mergeinfo_string;
SVN_ERR(svn_fs_fs__dag_get_proplist(&proplist, kid_dag, iterpool));
mergeinfo_string = apr_hash_get(proplist, SVN_PROP_MERGEINFO,
APR_HASH_KEY_STRING);
if (!mergeinfo_string) {
svn_string_t *idstr = svn_fs_fs__id_unparse(dirent->id, iterpool);
return svn_error_createf
(SVN_ERR_FS_CORRUPT, NULL,
_("Node-revision #'%s' claims to have mergeinfo but doesn't"),
idstr->data);
}
SVN_ERR(svn_mergeinfo_parse(&kid_mergeinfo,
mergeinfo_string->data,
result_pool));
apr_hash_set(result_catalog,
apr_pstrdup(result_pool, kid_path),
APR_HASH_KEY_STRING,
kid_mergeinfo);
}
if (go_down)
SVN_ERR(crawl_directory_dag_for_mergeinfo(root,
kid_path,
kid_dag,
result_catalog,
iterpool,
result_pool));
}
svn_pool_destroy(iterpool);
return SVN_NO_ERROR;
}
static svn_error_t *
append_to_merged_froms(svn_mergeinfo_t *output,
svn_mergeinfo_t input,
const char *path_piece,
apr_pool_t *pool) {
apr_hash_index_t *hi;
*output = apr_hash_make(pool);
for (hi = apr_hash_first(pool, input); hi; hi = apr_hash_next(hi)) {
const void *key;
void *val;
char *newpath;
apr_hash_this(hi, &key, NULL, &val);
newpath = svn_path_join((const char *) key, path_piece, pool);
apr_hash_set(*output, newpath, APR_HASH_KEY_STRING,
svn_rangelist_dup((apr_array_header_t *) val, pool));
}
return SVN_NO_ERROR;
}
static svn_error_t *
get_mergeinfo_for_path(svn_mergeinfo_t *mergeinfo,
svn_fs_root_t *rev_root,
const char *path,
svn_mergeinfo_inheritance_t inherit,
apr_pool_t *pool,
apr_pool_t *result_pool) {
parent_path_t *parent_path, *nearest_ancestor;
apr_hash_t *proplist;
svn_string_t *mergeinfo_string;
apr_pool_t *iterpool = svn_pool_create(pool);
*mergeinfo = NULL;
path = svn_fs__canonicalize_abspath(path, pool);
SVN_ERR(open_path(&parent_path, rev_root, path, 0, NULL, pool));
if (inherit == svn_mergeinfo_nearest_ancestor && ! parent_path->parent)
return SVN_NO_ERROR;
if (inherit == svn_mergeinfo_nearest_ancestor)
nearest_ancestor = parent_path->parent;
else
nearest_ancestor = parent_path;
while (TRUE) {
svn_boolean_t has_mergeinfo;
svn_pool_clear(iterpool);
SVN_ERR(svn_fs_fs__dag_has_mergeinfo(&has_mergeinfo,
nearest_ancestor->node, iterpool));
if (has_mergeinfo)
break;
if (inherit == svn_mergeinfo_explicit) {
svn_pool_destroy(iterpool);
return SVN_NO_ERROR;
}
nearest_ancestor = nearest_ancestor->parent;
if (!nearest_ancestor) {
svn_pool_destroy(iterpool);
return SVN_NO_ERROR;
}
}
svn_pool_destroy(iterpool);
SVN_ERR(svn_fs_fs__dag_get_proplist(&proplist, nearest_ancestor->node, pool));
mergeinfo_string = apr_hash_get(proplist, SVN_PROP_MERGEINFO,
APR_HASH_KEY_STRING);
if (!mergeinfo_string)
return svn_error_createf
(SVN_ERR_FS_CORRUPT, NULL,
_("Node-revision '%s@%ld' claims to have mergeinfo but doesn't"),
parent_path_path(nearest_ancestor, pool), rev_root->rev);
if (nearest_ancestor == parent_path) {
SVN_ERR(svn_mergeinfo_parse(mergeinfo,
mergeinfo_string->data,
result_pool));
return SVN_NO_ERROR;
} else {
svn_mergeinfo_t temp_mergeinfo;
SVN_ERR(svn_mergeinfo_parse(&temp_mergeinfo,
mergeinfo_string->data,
pool));
SVN_ERR(svn_mergeinfo_inheritable(&temp_mergeinfo,
temp_mergeinfo,
NULL, SVN_INVALID_REVNUM,
SVN_INVALID_REVNUM, pool));
SVN_ERR(append_to_merged_froms(mergeinfo,
temp_mergeinfo,
parent_path_relpath(parent_path,
nearest_ancestor,
pool),
result_pool));
return SVN_NO_ERROR;
}
}
static svn_error_t *
add_descendant_mergeinfo(svn_mergeinfo_catalog_t result_catalog,
svn_fs_root_t *root,
const char *path,
apr_pool_t *pool,
apr_pool_t *result_pool) {
dag_node_t *this_dag;
svn_boolean_t go_down;
SVN_ERR(get_dag(&this_dag, root, path, pool));
SVN_ERR(svn_fs_fs__dag_has_descendants_with_mergeinfo(&go_down,
this_dag,
pool));
if (go_down)
SVN_ERR(crawl_directory_dag_for_mergeinfo(root,
path,
this_dag,
result_catalog,
pool,
result_pool));
return SVN_NO_ERROR;
}
static svn_error_t *
get_mergeinfos_for_paths(svn_fs_root_t *root,
svn_mergeinfo_catalog_t *mergeinfo_catalog,
const apr_array_header_t *paths,
svn_mergeinfo_inheritance_t inherit,
svn_boolean_t include_descendants,
apr_pool_t *pool) {
svn_mergeinfo_catalog_t result_catalog = apr_hash_make(pool);
apr_pool_t *iterpool = svn_pool_create(pool);
int i;
for (i = 0; i < paths->nelts; i++) {
svn_mergeinfo_t path_mergeinfo;
const char *path = APR_ARRAY_IDX(paths, i, const char *);
svn_pool_clear(iterpool);
SVN_ERR(get_mergeinfo_for_path(&path_mergeinfo, root, path,
inherit, iterpool, pool));
if (path_mergeinfo)
apr_hash_set(result_catalog, path, APR_HASH_KEY_STRING,
path_mergeinfo);
if (include_descendants)
SVN_ERR(add_descendant_mergeinfo(result_catalog, root, path, iterpool,
pool));
}
svn_pool_destroy(iterpool);
*mergeinfo_catalog = result_catalog;
return SVN_NO_ERROR;
}
static svn_error_t *
fs_get_mergeinfo(svn_mergeinfo_catalog_t *catalog,
svn_fs_root_t *root,
const apr_array_header_t *paths,
svn_mergeinfo_inheritance_t inherit,
svn_boolean_t include_descendants,
apr_pool_t *pool) {
fs_fs_data_t *ffd = root->fs->fsap_data;
if (root->is_txn_root)
return svn_error_create(SVN_ERR_FS_NOT_REVISION_ROOT, NULL, NULL);
if (! svn_fs_fs__fs_supports_mergeinfo(root->fs))
return svn_error_createf
(SVN_ERR_UNSUPPORTED_FEATURE, NULL,
_("Querying mergeinfo requires version %d of the FSFS filesystem "
"schema; filesystem '%s' uses only version %d"),
SVN_FS_FS__MIN_MERGEINFO_FORMAT, root->fs->path, ffd->format);
return get_mergeinfos_for_paths(root, catalog, paths,
inherit, include_descendants,
pool);
}
static root_vtable_t root_vtable = {
fs_paths_changed,
svn_fs_fs__check_path,
fs_node_history,
fs_node_id,
svn_fs_fs__node_created_rev,
fs_node_origin_rev,
fs_node_created_path,
fs_delete_node,
fs_copied_from,
fs_closest_copy,
fs_node_prop,
fs_node_proplist,
fs_change_node_prop,
fs_props_changed,
fs_dir_entries,
fs_make_dir,
fs_copy,
fs_revision_link,
fs_file_length,
fs_file_md5_checksum,
fs_file_contents,
fs_make_file,
fs_apply_textdelta,
fs_apply_text,
fs_contents_changed,
fs_get_file_delta_stream,
fs_merge,
fs_get_mergeinfo
};
static svn_fs_root_t *
make_root(svn_fs_t *fs,
apr_pool_t *pool) {
apr_pool_t *subpool = svn_pool_create(pool);
svn_fs_root_t *root = apr_pcalloc(subpool, sizeof(*root));
root->fs = fs;
root->pool = subpool;
root->vtable = &root_vtable;
return root;
}
static svn_fs_root_t *
make_revision_root(svn_fs_t *fs,
svn_revnum_t rev,
dag_node_t *root_dir,
apr_pool_t *pool) {
svn_fs_root_t *root = make_root(fs, pool);
fs_rev_root_data_t *frd = apr_pcalloc(root->pool, sizeof(*frd));
root->is_txn_root = FALSE;
root->rev = rev;
frd->root_dir = root_dir;
frd->copyfrom_cache = apr_hash_make(root->pool);
root->fsap_data = frd;
return root;
}
static svn_fs_root_t *
make_txn_root(svn_fs_t *fs,
const char *txn,
svn_revnum_t base_rev,
apr_uint32_t flags,
apr_pool_t *pool) {
svn_fs_root_t *root = make_root(fs, pool);
fs_txn_root_data_t *frd = apr_pcalloc(root->pool, sizeof(*frd));
root->is_txn_root = TRUE;
root->txn = apr_pstrdup(root->pool, txn);
root->txn_flags = flags;
root->rev = base_rev;
frd->txn_node_cache = apr_hash_make(root->pool);
frd->txn_node_list.prev = &frd->txn_node_list;
frd->txn_node_list.next = &frd->txn_node_list;
root->fsap_data = frd;
return root;
}