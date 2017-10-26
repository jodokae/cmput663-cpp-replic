#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "svn_private_config.h"
#include "svn_pools.h"
#include "svn_error.h"
#include "svn_path.h"
#include "svn_md5.h"
#include "svn_mergeinfo.h"
#include "svn_fs.h"
#include "svn_sorts.h"
#include "fs.h"
#include "err.h"
#include "trail.h"
#include "node-rev.h"
#include "key-gen.h"
#include "dag.h"
#include "tree.h"
#include "lock.h"
#include "revs-txns.h"
#include "id.h"
#include "bdb/txn-table.h"
#include "bdb/rev-table.h"
#include "bdb/nodes-table.h"
#include "bdb/changes-table.h"
#include "bdb/copies-table.h"
#include "bdb/node-origins-table.h"
#include "../libsvn_fs/fs-loader.h"
#include "private/svn_fs_util.h"
#include "private/svn_mergeinfo_private.h"
#define WRITE_BUFFER_SIZE 512000
#define NODE_CACHE_MAX_KEYS 32
struct dag_node_cache_t {
dag_node_t *node;
int idx;
apr_pool_t *pool;
};
typedef struct {
dag_node_t *root_dir;
apr_hash_t *node_cache;
const char *node_cache_keys[NODE_CACHE_MAX_KEYS];
int node_cache_idx;
} base_root_data_t;
static svn_fs_root_t *make_revision_root(svn_fs_t *fs, svn_revnum_t rev,
dag_node_t *root_dir,
apr_pool_t *pool);
static svn_fs_root_t *make_txn_root(svn_fs_t *fs, const char *txn,
svn_revnum_t base_rev, apr_uint32_t flags,
apr_pool_t *pool);
static dag_node_t *
dag_node_cache_get(svn_fs_root_t *root,
const char *path,
apr_pool_t *pool) {
base_root_data_t *brd = root->fsap_data;
struct dag_node_cache_t *cache_item;
assert(*path == '/');
if (root->is_txn_root)
return NULL;
cache_item = apr_hash_get(brd->node_cache, path, APR_HASH_KEY_STRING);
if (cache_item)
return svn_fs_base__dag_dup(cache_item->node, pool);
return NULL;
}
static void
dag_node_cache_set(svn_fs_root_t *root,
const char *path,
dag_node_t *node) {
base_root_data_t *brd = root->fsap_data;
const char *cache_path;
apr_pool_t *cache_pool;
struct dag_node_cache_t *cache_item;
int num_keys = apr_hash_count(brd->node_cache);
assert(*path == '/');
assert((brd->node_cache_idx <= num_keys)
&& (num_keys <= NODE_CACHE_MAX_KEYS));
if (root->is_txn_root)
return;
cache_item = apr_hash_get(brd->node_cache, path, APR_HASH_KEY_STRING);
if (cache_item) {
abort();
#if 0
int cache_index = cache_item->idx;
cache_path = brd->node_cache_keys[cache_index];
cache_pool = cache_item->pool;
cache_item->node = svn_fs_base__dag_dup(node, cache_pool);
if (cache_index != (num_keys - 1)) {
int move_num = NODE_CACHE_MAX_KEYS - cache_index - 1;
memmove(brd->node_cache_keys + cache_index,
brd->node_cache_keys + cache_index + 1,
move_num * sizeof(const char *));
cache_index = num_keys - 1;
brd->node_cache_keys[cache_index] = cache_path;
}
cache_item->idx = cache_index;
brd->node_cache_idx = (cache_index + 1) % NODE_CACHE_MAX_KEYS;
return;
#endif
}
if (apr_hash_count(brd->node_cache) == NODE_CACHE_MAX_KEYS) {
cache_path = brd->node_cache_keys[brd->node_cache_idx];
cache_item = apr_hash_get(brd->node_cache, cache_path,
APR_HASH_KEY_STRING);
apr_hash_set(brd->node_cache, cache_path, APR_HASH_KEY_STRING, NULL);
cache_pool = cache_item->pool;
svn_pool_clear(cache_pool);
} else {
cache_pool = svn_pool_create(root->pool);
}
cache_item = apr_palloc(cache_pool, sizeof(*cache_item));
cache_item->node = svn_fs_base__dag_dup(node, cache_pool);
cache_item->idx = brd->node_cache_idx;
cache_item->pool = cache_pool;
cache_path = apr_pstrdup(cache_pool, path);
apr_hash_set(brd->node_cache, cache_path, APR_HASH_KEY_STRING, cache_item);
brd->node_cache_keys[brd->node_cache_idx] = cache_path;
brd->node_cache_idx = (brd->node_cache_idx + 1) % NODE_CACHE_MAX_KEYS;
}
struct txn_root_args {
svn_fs_root_t **root_p;
svn_fs_txn_t *txn;
};
static svn_error_t *
txn_body_txn_root(void *baton,
trail_t *trail) {
struct txn_root_args *args = baton;
svn_fs_root_t **root_p = args->root_p;
svn_fs_txn_t *txn = args->txn;
svn_fs_t *fs = txn->fs;
const char *svn_txn_id = txn->id;
const svn_fs_id_t *root_id, *base_root_id;
svn_fs_root_t *root;
apr_hash_t *txnprops;
apr_uint32_t flags = 0;
SVN_ERR(svn_fs_base__get_txn_ids(&root_id, &base_root_id, fs,
svn_txn_id, trail, trail->pool));
SVN_ERR(svn_fs_base__txn_proplist_in_trail(&txnprops, svn_txn_id, trail));
if (apr_hash_get(txnprops, SVN_FS__PROP_TXN_CHECK_OOD, APR_HASH_KEY_STRING))
flags |= SVN_FS_TXN_CHECK_OOD;
if (apr_hash_get(txnprops, SVN_FS__PROP_TXN_CHECK_LOCKS,
APR_HASH_KEY_STRING))
flags |= SVN_FS_TXN_CHECK_LOCKS;
root = make_txn_root(fs, svn_txn_id, txn->base_rev, flags, trail->pool);
*root_p = root;
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_base__txn_root(svn_fs_root_t **root_p,
svn_fs_txn_t *txn,
apr_pool_t *pool) {
svn_fs_root_t *root;
struct txn_root_args args;
args.root_p = &root;
args.txn = txn;
SVN_ERR(svn_fs_base__retry_txn(txn->fs, txn_body_txn_root, &args, pool));
*root_p = root;
return SVN_NO_ERROR;
}
struct revision_root_args {
svn_fs_root_t **root_p;
svn_revnum_t rev;
};
static svn_error_t *
txn_body_revision_root(void *baton,
trail_t *trail) {
struct revision_root_args *args = baton;
dag_node_t *root_dir;
svn_fs_root_t *root;
SVN_ERR(svn_fs_base__dag_revision_root(&root_dir, trail->fs, args->rev,
trail, trail->pool));
root = make_revision_root(trail->fs, args->rev, root_dir, trail->pool);
*args->root_p = root;
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_base__revision_root(svn_fs_root_t **root_p,
svn_fs_t *fs,
svn_revnum_t rev,
apr_pool_t *pool) {
struct revision_root_args args;
svn_fs_root_t *root;
SVN_ERR(svn_fs__check_fs(fs, TRUE));
args.root_p = &root;
args.rev = rev;
SVN_ERR(svn_fs_base__retry_txn(fs, txn_body_revision_root, &args, pool));
*root_p = root;
return SVN_NO_ERROR;
}
static svn_error_t *
root_node(dag_node_t **node_p,
svn_fs_root_t *root,
trail_t *trail,
apr_pool_t *pool) {
base_root_data_t *brd = root->fsap_data;
if (! root->is_txn_root) {
*node_p = svn_fs_base__dag_dup(brd->root_dir, pool);
return SVN_NO_ERROR;
} else {
return svn_fs_base__dag_txn_root(node_p, root->fs, root->txn,
trail, pool);
}
}
static svn_error_t *
mutable_root_node(dag_node_t **node_p,
svn_fs_root_t *root,
const char *error_path,
trail_t *trail,
apr_pool_t *pool) {
if (root->is_txn_root)
return svn_fs_base__dag_clone_root(node_p, root->fs, root->txn,
trail, pool);
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
trail_t *trail,
apr_pool_t *pool) {
const svn_fs_id_t *child_id, *parent_id;
const char *child_copy_id, *parent_copy_id;
const char *id_path = NULL;
assert(child && child->parent && txn_id);
*inherit_p = copy_id_inherit_self;
*copy_src_path = NULL;
child_id = svn_fs_base__dag_get_id(child->node);
parent_id = svn_fs_base__dag_get_id(child->parent->node);
child_copy_id = svn_fs_base__id_copy_id(child_id);
parent_copy_id = svn_fs_base__id_copy_id(parent_id);
if (svn_fs_base__key_compare(svn_fs_base__id_txn_id(child_id), txn_id) == 0)
return SVN_NO_ERROR;
if ((strcmp(child_copy_id, "0") == 0)
|| (svn_fs_base__key_compare(child_copy_id, parent_copy_id) == 0)) {
*inherit_p = copy_id_inherit_parent;
return SVN_NO_ERROR;
} else {
copy_t *copy;
SVN_ERR(svn_fs_bdb__get_copy(&copy, fs, child_copy_id, trail, pool));
if (svn_fs_base__id_compare(copy->dst_noderev_id, child_id) == -1) {
*inherit_p = copy_id_inherit_parent;
return SVN_NO_ERROR;
}
}
id_path = svn_fs_base__dag_get_created_path(child->node);
if (strcmp(id_path, parent_path_path(child, pool)) != 0) {
*inherit_p = copy_id_inherit_new;
*copy_src_path = id_path;
return SVN_NO_ERROR;
}
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
trail_t *trail,
apr_pool_t *pool) {
svn_fs_t *fs = root->fs;
const svn_fs_id_t *id;
dag_node_t *here;
parent_path_t *parent_path;
const char *rest;
const char *canon_path = svn_fs__canonicalize_abspath(path, pool);
const char *path_so_far = "/";
SVN_ERR(root_node(&here, root, trail, pool));
id = svn_fs_base__dag_get_id(here);
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
err = svn_fs_base__dag_open(&child, here, entry, trail, pool);
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
fs, parent_path, txn_id,
trail, pool));
parent_path->copy_inherit = inherit;
parent_path->copy_src_path = apr_pstrdup(pool, copy_path);
}
if (! cached_node)
dag_node_cache_set(root, path_so_far, child);
}
if (! next)
break;
if (svn_fs_base__dag_node_kind(child) != svn_node_dir)
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
trail_t *trail,
apr_pool_t *pool) {
dag_node_t *cloned_node;
const char *txn_id = root->txn;
svn_fs_t *fs = root->fs;
if (svn_fs_base__dag_check_mutable(parent_path->node, txn_id))
return SVN_NO_ERROR;
if (parent_path->parent) {
const svn_fs_id_t *parent_id;
const svn_fs_id_t *node_id = svn_fs_base__dag_get_id(parent_path->node);
const char *copy_id = NULL;
const char *copy_src_path = parent_path->copy_src_path;
copy_id_inherit_t inherit = parent_path->copy_inherit;
const char *clone_path;
SVN_ERR(make_path_mutable(root, parent_path->parent,
error_path, trail, pool));
switch (inherit) {
case copy_id_inherit_parent:
parent_id = svn_fs_base__dag_get_id(parent_path->parent->node);
copy_id = svn_fs_base__id_copy_id(parent_id);
break;
case copy_id_inherit_new:
SVN_ERR(svn_fs_bdb__reserve_copy_id(&copy_id, fs, trail, pool));
break;
case copy_id_inherit_self:
copy_id = NULL;
break;
case copy_id_inherit_unknown:
default:
abort();
}
clone_path = parent_path_path(parent_path->parent, pool);
SVN_ERR(svn_fs_base__dag_clone_child(&cloned_node,
parent_path->parent->node,
clone_path,
parent_path->entry,
copy_id, txn_id,
trail, pool));
if (inherit == copy_id_inherit_new) {
const svn_fs_id_t *new_node_id =
svn_fs_base__dag_get_id(cloned_node);
SVN_ERR(svn_fs_bdb__create_copy(fs, copy_id, copy_src_path,
svn_fs_base__id_txn_id(node_id),
new_node_id,
copy_kind_soft, trail, pool));
SVN_ERR(svn_fs_base__add_txn_copy(fs, txn_id, copy_id,
trail, pool));
}
} else {
SVN_ERR(mutable_root_node(&cloned_node, root, error_path, trail, pool));
}
parent_path->node = cloned_node;
return SVN_NO_ERROR;
}
static svn_error_t *
adjust_parent_mergeinfo_counts(parent_path_t *parent_path,
apr_int64_t count_delta,
const char *txn_id,
trail_t *trail,
apr_pool_t *pool) {
apr_pool_t *iterpool;
parent_path_t *pp = parent_path;
if (count_delta == 0)
return SVN_NO_ERROR;
iterpool = svn_pool_create(pool);
while (pp) {
svn_pool_clear(iterpool);
SVN_ERR(svn_fs_base__dag_adjust_mergeinfo_count(pp->node, count_delta,
txn_id, trail,
iterpool));
pp = pp->parent;
}
svn_pool_destroy(iterpool);
return SVN_NO_ERROR;
}
static svn_error_t *
get_dag(dag_node_t **dag_node_p,
svn_fs_root_t *root,
const char *path,
trail_t *trail,
apr_pool_t *pool) {
parent_path_t *parent_path;
dag_node_t *node = NULL;
path = svn_fs__canonicalize_abspath(path, pool);
node = dag_node_cache_get(root, path, pool);
if (! node) {
SVN_ERR(open_path(&parent_path, root, path, 0, NULL, trail, pool));
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
trail_t *trail,
apr_pool_t *pool) {
change_t change;
change.path = svn_fs__canonicalize_abspath(path, pool);
change.noderev_id = noderev_id;
change.kind = change_kind;
change.text_mod = text_mod;
change.prop_mod = prop_mod;
return svn_fs_bdb__changes_add(fs, txn_id, &change, trail, pool);
}
struct node_id_args {
const svn_fs_id_t **id_p;
svn_fs_root_t *root;
const char *path;
};
static svn_error_t *
txn_body_node_id(void *baton, trail_t *trail) {
struct node_id_args *args = baton;
dag_node_t *node;
SVN_ERR(get_dag(&node, args->root, args->path, trail, trail->pool));
*args->id_p = svn_fs_base__id_copy(svn_fs_base__dag_get_id(node),
trail->pool);
return SVN_NO_ERROR;
}
static svn_error_t *
base_node_id(const svn_fs_id_t **id_p,
svn_fs_root_t *root,
const char *path,
apr_pool_t *pool) {
base_root_data_t *brd = root->fsap_data;
if (! root->is_txn_root
&& (path[0] == '\0' || ((path[0] == '/') && (path[1] == '\0')))) {
*id_p = svn_fs_base__id_copy(svn_fs_base__dag_get_id(brd->root_dir),
pool);
} else {
const svn_fs_id_t *id;
struct node_id_args args;
args.id_p = &id;
args.root = root;
args.path = path;
SVN_ERR(svn_fs_base__retry_txn(root->fs, txn_body_node_id, &args,
pool));
*id_p = id;
}
return SVN_NO_ERROR;
}
struct node_created_rev_args {
svn_revnum_t revision;
svn_fs_root_t *root;
const char *path;
};
static svn_error_t *
txn_body_node_created_rev(void *baton, trail_t *trail) {
struct node_created_rev_args *args = baton;
dag_node_t *node;
SVN_ERR(get_dag(&node, args->root, args->path, trail, trail->pool));
SVN_ERR(svn_fs_base__dag_get_revision(&(args->revision), node,
trail, trail->pool));
return SVN_NO_ERROR;
}
static svn_error_t *
base_node_created_rev(svn_revnum_t *revision,
svn_fs_root_t *root,
const char *path,
apr_pool_t *pool) {
struct node_created_rev_args args;
args.revision = SVN_INVALID_REVNUM;
args.root = root;
args.path = path;
SVN_ERR(svn_fs_base__retry_txn
(root->fs, txn_body_node_created_rev, &args, pool));
*revision = args.revision;
return SVN_NO_ERROR;
}
struct node_created_path_args {
const char **created_path;
svn_fs_root_t *root;
const char *path;
};
static svn_error_t *
txn_body_node_created_path(void *baton, trail_t *trail) {
struct node_created_path_args *args = baton;
dag_node_t *node;
SVN_ERR(get_dag(&node, args->root, args->path, trail, trail->pool));
*args->created_path = svn_fs_base__dag_get_created_path(node);
return SVN_NO_ERROR;
}
static svn_error_t *
base_node_created_path(const char **created_path,
svn_fs_root_t *root,
const char *path,
apr_pool_t *pool) {
struct node_created_path_args args;
args.created_path = created_path;
args.root = root;
args.path = path;
SVN_ERR(svn_fs_base__retry_txn
(root->fs, txn_body_node_created_path, &args, pool));
return SVN_NO_ERROR;
}
struct node_kind_args {
const svn_fs_id_t *id;
svn_node_kind_t kind;
};
static svn_error_t *
txn_body_node_kind(void *baton, trail_t *trail) {
struct node_kind_args *args = baton;
dag_node_t *node;
SVN_ERR(svn_fs_base__dag_get_node(&node, trail->fs, args->id,
trail, trail->pool));
args->kind = svn_fs_base__dag_node_kind(node);
return SVN_NO_ERROR;
}
static svn_error_t *
node_kind(svn_node_kind_t *kind_p,
svn_fs_root_t *root,
const char *path,
apr_pool_t *pool) {
struct node_kind_args args;
const svn_fs_id_t *node_id;
SVN_ERR(base_node_id(&node_id, root, path, pool));
args.id = node_id;
SVN_ERR(svn_fs_base__retry_txn(root->fs, txn_body_node_kind, &args, pool));
*kind_p = args.kind;
return SVN_NO_ERROR;
}
static svn_error_t *
base_check_path(svn_node_kind_t *kind_p,
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
struct node_prop_args {
svn_string_t **value_p;
svn_fs_root_t *root;
const char *path;
const char *propname;
};
static svn_error_t *
txn_body_node_prop(void *baton,
trail_t *trail) {
struct node_prop_args *args = baton;
dag_node_t *node;
apr_hash_t *proplist;
SVN_ERR(get_dag(&node, args->root, args->path, trail, trail->pool));
SVN_ERR(svn_fs_base__dag_get_proplist(&proplist, node,
trail, trail->pool));
*(args->value_p) = NULL;
if (proplist)
*(args->value_p) = apr_hash_get(proplist, args->propname,
APR_HASH_KEY_STRING);
return SVN_NO_ERROR;
}
static svn_error_t *
base_node_prop(svn_string_t **value_p,
svn_fs_root_t *root,
const char *path,
const char *propname,
apr_pool_t *pool) {
struct node_prop_args args;
svn_string_t *value;
args.value_p = &value;
args.root = root;
args.path = path;
args.propname = propname;
SVN_ERR(svn_fs_base__retry_txn(root->fs, txn_body_node_prop, &args, pool));
*value_p = value;
return SVN_NO_ERROR;
}
struct node_proplist_args {
apr_hash_t **table_p;
svn_fs_root_t *root;
const char *path;
};
static svn_error_t *
txn_body_node_proplist(void *baton, trail_t *trail) {
struct node_proplist_args *args = baton;
dag_node_t *node;
apr_hash_t *proplist;
SVN_ERR(get_dag(&node, args->root, args->path, trail, trail->pool));
SVN_ERR(svn_fs_base__dag_get_proplist(&proplist, node,
trail, trail->pool));
*args->table_p = proplist ? proplist : apr_hash_make(trail->pool);
return SVN_NO_ERROR;
}
static svn_error_t *
base_node_proplist(apr_hash_t **table_p,
svn_fs_root_t *root,
const char *path,
apr_pool_t *pool) {
apr_hash_t *table;
struct node_proplist_args args;
args.table_p = &table;
args.root = root;
args.path = path;
SVN_ERR(svn_fs_base__retry_txn(root->fs, txn_body_node_proplist, &args,
pool));
*table_p = table;
return SVN_NO_ERROR;
}
struct change_node_prop_args {
svn_fs_root_t *root;
const char *path;
const char *name;
const svn_string_t *value;
};
static svn_error_t *
txn_body_change_node_prop(void *baton,
trail_t *trail) {
struct change_node_prop_args *args = baton;
parent_path_t *parent_path;
apr_hash_t *proplist;
const char *txn_id = args->root->txn;
base_fs_data_t *bfd = trail->fs->fsap_data;
SVN_ERR(open_path(&parent_path, args->root, args->path, 0, txn_id,
trail, trail->pool));
if (args->root->txn_flags & SVN_FS_TXN_CHECK_LOCKS)
SVN_ERR(svn_fs_base__allow_locked_operation
(args->path, FALSE, trail, trail->pool));
SVN_ERR(make_path_mutable(args->root, parent_path, args->path,
trail, trail->pool));
SVN_ERR(svn_fs_base__dag_get_proplist(&proplist, parent_path->node,
trail, trail->pool));
if ((! proplist) && (! args->value))
return SVN_NO_ERROR;
if (! proplist)
proplist = apr_hash_make(trail->pool);
apr_hash_set(proplist, args->name, APR_HASH_KEY_STRING, args->value);
SVN_ERR(svn_fs_base__dag_set_proplist(parent_path->node, proplist,
txn_id, trail, trail->pool));
if ((bfd->format >= SVN_FS_BASE__MIN_MERGEINFO_FORMAT)
&& (strcmp(args->name, SVN_PROP_MERGEINFO) == 0)) {
svn_boolean_t had_mergeinfo, has_mergeinfo = args->value ? TRUE : FALSE;
SVN_ERR(svn_fs_base__dag_set_has_mergeinfo(parent_path->node,
has_mergeinfo,
&had_mergeinfo, txn_id,
trail, trail->pool));
if (parent_path->parent && ((! had_mergeinfo) != (! has_mergeinfo)))
SVN_ERR(adjust_parent_mergeinfo_counts(parent_path->parent,
has_mergeinfo ? 1 : -1,
txn_id, trail, trail->pool));
}
SVN_ERR(add_change(args->root->fs, txn_id,
args->path, svn_fs_base__dag_get_id(parent_path->node),
svn_fs_path_change_modify, FALSE, TRUE, trail,
trail->pool));
return SVN_NO_ERROR;
}
static svn_error_t *
base_change_node_prop(svn_fs_root_t *root,
const char *path,
const char *name,
const svn_string_t *value,
apr_pool_t *pool) {
struct change_node_prop_args args;
if (! root->is_txn_root)
return SVN_FS__NOT_TXN(root);
args.root = root;
args.path = path;
args.name = name;
args.value = value;
SVN_ERR(svn_fs_base__retry_txn(root->fs, txn_body_change_node_prop, &args,
pool));
return SVN_NO_ERROR;
}
struct things_changed_args {
svn_boolean_t *changed_p;
svn_fs_root_t *root1;
svn_fs_root_t *root2;
const char *path1;
const char *path2;
apr_pool_t *pool;
};
static svn_error_t *
txn_body_props_changed(void *baton, trail_t *trail) {
struct things_changed_args *args = baton;
dag_node_t *node1, *node2;
SVN_ERR(get_dag(&node1, args->root1, args->path1, trail, trail->pool));
SVN_ERR(get_dag(&node2, args->root2, args->path2, trail, trail->pool));
SVN_ERR(svn_fs_base__things_different(args->changed_p, NULL,
node1, node2, trail, trail->pool));
return SVN_NO_ERROR;
}
static svn_error_t *
base_props_changed(svn_boolean_t *changed_p,
svn_fs_root_t *root1,
const char *path1,
svn_fs_root_t *root2,
const char *path2,
apr_pool_t *pool) {
struct things_changed_args args;
if (root1->fs != root2->fs)
return svn_error_create
(SVN_ERR_FS_GENERAL, NULL,
_("Cannot compare property value between two different filesystems"));
args.root1 = root1;
args.root2 = root2;
args.path1 = path1;
args.path2 = path2;
args.changed_p = changed_p;
args.pool = pool;
SVN_ERR(svn_fs_base__retry_txn(root1->fs, txn_body_props_changed,
&args, pool));
return SVN_NO_ERROR;
}
struct dir_entries_args {
apr_hash_t **table_p;
svn_fs_root_t *root;
const char *path;
};
static svn_error_t *
txn_body_dir_entries(void *baton,
trail_t *trail) {
struct dir_entries_args *args = baton;
dag_node_t *node;
apr_hash_t *entries;
SVN_ERR(get_dag(&node, args->root, args->path, trail, trail->pool));
SVN_ERR(svn_fs_base__dag_dir_entries(&entries, node, trail, trail->pool));
*args->table_p = entries ? entries : apr_hash_make(trail->pool);
return SVN_NO_ERROR;
}
static svn_error_t *
base_dir_entries(apr_hash_t **table_p,
svn_fs_root_t *root,
const char *path,
apr_pool_t *pool) {
struct dir_entries_args args;
apr_pool_t *iterpool;
apr_hash_t *table;
svn_fs_t *fs = root->fs;
apr_hash_index_t *hi;
args.table_p = &table;
args.root = root;
args.path = path;
SVN_ERR(svn_fs_base__retry_txn(root->fs, txn_body_dir_entries, &args,
pool));
iterpool = svn_pool_create(pool);
for (hi = apr_hash_first(pool, table); hi; hi = apr_hash_next(hi)) {
svn_fs_dirent_t *entry;
struct node_kind_args nk_args;
void *val;
svn_pool_clear(iterpool);
apr_hash_this(hi, NULL, NULL, &val);
entry = val;
nk_args.id = entry->id;
SVN_ERR(svn_fs_base__retry_txn(fs, txn_body_node_kind, &nk_args,
iterpool));
entry->kind = nk_args.kind;
}
svn_pool_destroy(iterpool);
*table_p = table;
return SVN_NO_ERROR;
}
struct deltify_committed_args {
svn_fs_t *fs;
svn_revnum_t rev;
const char *txn_id;
};
struct txn_deltify_args {
const svn_fs_id_t *tgt_id;
const svn_fs_id_t *base_id;
svn_boolean_t is_dir;
};
static svn_error_t *
txn_body_txn_deltify(void *baton, trail_t *trail) {
struct txn_deltify_args *args = baton;
dag_node_t *tgt_node, *base_node;
SVN_ERR(svn_fs_base__dag_get_node(&tgt_node, trail->fs, args->tgt_id,
trail, trail->pool));
SVN_ERR(svn_fs_base__dag_get_node(&base_node, trail->fs, args->base_id,
trail, trail->pool));
SVN_ERR(svn_fs_base__dag_deltify(tgt_node, base_node, args->is_dir,
trail, trail->pool));
return SVN_NO_ERROR;
}
struct txn_pred_count_args {
const svn_fs_id_t *id;
int pred_count;
};
static svn_error_t *
txn_body_pred_count(void *baton, trail_t *trail) {
node_revision_t *noderev;
struct txn_pred_count_args *args = baton;
SVN_ERR(svn_fs_bdb__get_node_revision(&noderev, trail->fs,
args->id, trail, trail->pool));
args->pred_count = noderev->predecessor_count;
return SVN_NO_ERROR;
}
struct txn_pred_id_args {
const svn_fs_id_t *id;
const svn_fs_id_t *pred_id;
apr_pool_t *pool;
};
static svn_error_t *
txn_body_pred_id(void *baton, trail_t *trail) {
node_revision_t *nr;
struct txn_pred_id_args *args = baton;
SVN_ERR(svn_fs_bdb__get_node_revision(&nr, trail->fs, args->id,
trail, trail->pool));
if (nr->predecessor_id)
args->pred_id = svn_fs_base__id_copy(nr->predecessor_id, args->pool);
else
args->pred_id = NULL;
return SVN_NO_ERROR;
}
static svn_error_t *
deltify_mutable(svn_fs_t *fs,
svn_fs_root_t *root,
const char *path,
const svn_fs_id_t *node_id,
svn_node_kind_t kind,
const char *txn_id,
apr_pool_t *pool) {
const svn_fs_id_t *id = node_id;
apr_hash_t *entries = NULL;
struct txn_deltify_args td_args;
if (! node_id)
SVN_ERR(base_node_id(&id, root, path, pool));
if (strcmp(svn_fs_base__id_txn_id(id), txn_id))
return SVN_NO_ERROR;
if (kind == svn_node_unknown)
SVN_ERR(base_check_path(&kind, root, path, pool));
if (kind == svn_node_dir)
SVN_ERR(base_dir_entries(&entries, root, path, pool));
if (entries) {
apr_pool_t *subpool = svn_pool_create(pool);
apr_hash_index_t *hi;
for (hi = apr_hash_first(pool, entries); hi; hi = apr_hash_next(hi)) {
const void *key;
void *val;
svn_fs_dirent_t *entry;
svn_pool_clear(subpool);
apr_hash_this(hi, &key, NULL, &val);
entry = val;
SVN_ERR(deltify_mutable(fs, root,
svn_path_join(path, key, subpool),
entry->id, entry->kind, txn_id, subpool));
}
svn_pool_destroy(subpool);
}
{
int pred_count, nlevels, lev, count;
const svn_fs_id_t *pred_id;
struct txn_pred_count_args tpc_args;
apr_pool_t *subpools[2];
int active_subpool = 0;
tpc_args.id = id;
SVN_ERR(svn_fs_base__retry_txn(fs, txn_body_pred_count, &tpc_args,
pool));
pred_count = tpc_args.pred_count;
if (pred_count == 0)
return SVN_NO_ERROR;
nlevels = 1;
if (pred_count >= 32) {
while (pred_count % 2 == 0) {
pred_count /= 2;
nlevels++;
}
if (1 << (nlevels - 1) == pred_count)
nlevels--;
}
count = 0;
pred_id = id;
subpools[0] = svn_pool_create(pool);
subpools[1] = svn_pool_create(pool);
for (lev = 0; lev < nlevels; lev++) {
if (lev == 1)
continue;
while (count < (1 << lev)) {
struct txn_pred_id_args tpi_args;
active_subpool = !active_subpool;
svn_pool_clear(subpools[active_subpool]);
tpi_args.id = pred_id;
tpi_args.pool = subpools[active_subpool];
SVN_ERR(svn_fs_base__retry_txn(fs, txn_body_pred_id, &tpi_args,
subpools[active_subpool]));
pred_id = tpi_args.pred_id;
if (pred_id == NULL)
return svn_error_create
(SVN_ERR_FS_CORRUPT, 0,
_("Corrupt DB: faulty predecessor count"));
count++;
}
td_args.tgt_id = pred_id;
td_args.base_id = id;
td_args.is_dir = (kind == svn_node_dir);
SVN_ERR(svn_fs_base__retry_txn(fs, txn_body_txn_deltify, &td_args,
subpools[active_subpool]));
}
svn_pool_destroy(subpools[0]);
svn_pool_destroy(subpools[1]);
}
return SVN_NO_ERROR;
}
struct get_root_args {
svn_fs_root_t *root;
dag_node_t *node;
};
static svn_error_t *
txn_body_get_root(void *baton, trail_t *trail) {
struct get_root_args *args = baton;
SVN_ERR(get_dag(&(args->node), args->root, "", trail, trail->pool));
return SVN_NO_ERROR;
}
static svn_error_t *
update_ancestry(svn_fs_t *fs,
const svn_fs_id_t *source_id,
const svn_fs_id_t *target_id,
const char *txn_id,
const char *target_path,
int source_pred_count,
trail_t *trail,
apr_pool_t *pool) {
node_revision_t *noderev;
if (strcmp(svn_fs_base__id_txn_id(target_id), txn_id))
return svn_error_createf
(SVN_ERR_FS_NOT_MUTABLE, NULL,
_("Unexpected immutable node at '%s'"), target_path);
SVN_ERR(svn_fs_bdb__get_node_revision(&noderev, fs, target_id,
trail, pool));
noderev->predecessor_id = source_id;
noderev->predecessor_count = source_pred_count;
if (noderev->predecessor_count != -1)
noderev->predecessor_count++;
return svn_fs_bdb__put_node_revision(fs, target_id, noderev, trail, pool);
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
trail_t *trail,
apr_pool_t *pool) {
const svn_fs_id_t *source_id, *target_id, *ancestor_id;
apr_hash_t *s_entries, *t_entries, *a_entries;
apr_hash_index_t *hi;
apr_pool_t *iterpool;
svn_fs_t *fs;
int pred_count;
apr_int64_t mergeinfo_increment = 0;
base_fs_data_t *bfd = trail->fs->fsap_data;
fs = svn_fs_base__dag_get_fs(ancestor);
if ((fs != svn_fs_base__dag_get_fs(source))
|| (fs != svn_fs_base__dag_get_fs(target))) {
return svn_error_create
(SVN_ERR_FS_CORRUPT, NULL,
_("Bad merge; ancestor, source, and target not all in same fs"));
}
SVN_ERR(svn_fs__check_fs(fs, TRUE));
source_id = svn_fs_base__dag_get_id(source);
target_id = svn_fs_base__dag_get_id(target);
ancestor_id = svn_fs_base__dag_get_id(ancestor);
if (svn_fs_base__id_eq(ancestor_id, target_id)) {
svn_string_t *id_str = svn_fs_base__id_unparse(target_id, pool);
return svn_error_createf
(SVN_ERR_FS_GENERAL, NULL,
_("Bad merge; target '%s' has id '%s', same as ancestor"),
target_path, id_str->data);
}
svn_stringbuf_setempty(conflict_p);
if (svn_fs_base__id_eq(ancestor_id, source_id)
|| (svn_fs_base__id_eq(source_id, target_id)))
return SVN_NO_ERROR;
if ((svn_fs_base__dag_node_kind(source) != svn_node_dir)
|| (svn_fs_base__dag_node_kind(target) != svn_node_dir)
|| (svn_fs_base__dag_node_kind(ancestor) != svn_node_dir)) {
return conflict_err(conflict_p, target_path);
}
{
node_revision_t *tgt_nr, *anc_nr, *src_nr;
SVN_ERR(svn_fs_bdb__get_node_revision(&tgt_nr, fs, target_id,
trail, pool));
SVN_ERR(svn_fs_bdb__get_node_revision(&anc_nr, fs, ancestor_id,
trail, pool));
SVN_ERR(svn_fs_bdb__get_node_revision(&src_nr, fs, source_id,
trail, pool));
if (! svn_fs_base__same_keys(tgt_nr->prop_key, anc_nr->prop_key))
return conflict_err(conflict_p, target_path);
if (! svn_fs_base__same_keys(src_nr->prop_key, anc_nr->prop_key))
return conflict_err(conflict_p, target_path);
}
SVN_ERR(svn_fs_base__dag_dir_entries(&s_entries, source, trail, pool));
if (! s_entries)
s_entries = apr_hash_make(pool);
SVN_ERR(svn_fs_base__dag_dir_entries(&t_entries, target, trail, pool));
if (! t_entries)
t_entries = apr_hash_make(pool);
SVN_ERR(svn_fs_base__dag_dir_entries(&a_entries, ancestor, trail, pool));
if (! a_entries)
a_entries = apr_hash_make(pool);
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
if (s_entry && svn_fs_base__id_eq(a_entry->id, s_entry->id))
goto end;
else if (t_entry && svn_fs_base__id_eq(a_entry->id, t_entry->id)) {
dag_node_t *t_ent_node;
apr_int64_t mergeinfo_start;
SVN_ERR(svn_fs_base__dag_get_node(&t_ent_node, fs,
t_entry->id, trail, iterpool));
SVN_ERR(svn_fs_base__dag_get_mergeinfo_stats(NULL, &mergeinfo_start,
t_ent_node, trail,
iterpool));
mergeinfo_increment -= mergeinfo_start;
if (s_entry) {
dag_node_t *s_ent_node;
apr_int64_t mergeinfo_end;
SVN_ERR(svn_fs_base__dag_get_node(&s_ent_node, fs,
s_entry->id, trail,
iterpool));
SVN_ERR(svn_fs_base__dag_get_mergeinfo_stats(NULL,
&mergeinfo_end,
s_ent_node, trail,
iterpool));
mergeinfo_increment += mergeinfo_end;
SVN_ERR(svn_fs_base__dag_set_entry(target, key, s_entry->id,
txn_id, trail, iterpool));
} else {
SVN_ERR(svn_fs_base__dag_delete(target, key, txn_id,
trail, iterpool));
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
if (strcmp(svn_fs_base__id_node_id(s_entry->id),
svn_fs_base__id_node_id(a_entry->id)) != 0
|| strcmp(svn_fs_base__id_copy_id(s_entry->id),
svn_fs_base__id_copy_id(a_entry->id)) != 0
|| strcmp(svn_fs_base__id_node_id(t_entry->id),
svn_fs_base__id_node_id(a_entry->id)) != 0
|| strcmp(svn_fs_base__id_copy_id(t_entry->id),
svn_fs_base__id_copy_id(a_entry->id)) != 0)
return conflict_err(conflict_p,
svn_path_join(target_path,
a_entry->name,
iterpool));
SVN_ERR(svn_fs_base__dag_get_node(&s_ent_node, fs,
s_entry->id, trail, iterpool));
SVN_ERR(svn_fs_base__dag_get_node(&t_ent_node, fs,
t_entry->id, trail, iterpool));
SVN_ERR(svn_fs_base__dag_get_node(&a_ent_node, fs,
a_entry->id, trail, iterpool));
if ((svn_fs_base__dag_node_kind(s_ent_node) == svn_node_file)
|| (svn_fs_base__dag_node_kind(t_ent_node) == svn_node_file)
|| (svn_fs_base__dag_node_kind(a_ent_node) == svn_node_file))
return conflict_err(conflict_p,
svn_path_join(target_path,
a_entry->name,
iterpool));
new_tpath = svn_path_join(target_path, t_entry->name, iterpool);
SVN_ERR(merge(conflict_p, new_tpath,
t_ent_node, s_ent_node, a_ent_node,
txn_id, &sub_mergeinfo_increment, trail, iterpool));
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
apr_int64_t mergeinfo_s;
svn_pool_clear(iterpool);
apr_hash_this(hi, &key, &klen, &val);
s_entry = val;
t_entry = apr_hash_get(t_entries, key, klen);
if (t_entry)
return conflict_err(conflict_p,
svn_path_join(target_path,
t_entry->name,
iterpool));
SVN_ERR(svn_fs_base__dag_get_node(&s_ent_node, fs,
s_entry->id, trail, iterpool));
SVN_ERR(svn_fs_base__dag_get_mergeinfo_stats(NULL, &mergeinfo_s,
s_ent_node, trail,
iterpool));
mergeinfo_increment += mergeinfo_s;
SVN_ERR(svn_fs_base__dag_set_entry
(target, s_entry->name, s_entry->id, txn_id, trail, iterpool));
}
svn_pool_destroy(iterpool);
SVN_ERR(svn_fs_base__dag_get_predecessor_count(&pred_count, source,
trail, pool));
SVN_ERR(update_ancestry(fs, source_id, target_id, txn_id, target_path,
pred_count, trail, pool));
if (bfd->format >= SVN_FS_BASE__MIN_MERGEINFO_FORMAT) {
SVN_ERR(svn_fs_base__dag_adjust_mergeinfo_count(target,
mergeinfo_increment,
txn_id, trail, pool));
}
if (mergeinfo_increment_out)
*mergeinfo_increment_out = mergeinfo_increment;
return SVN_NO_ERROR;
}
struct merge_args {
dag_node_t *ancestor_node;
dag_node_t *source_node;
svn_fs_txn_t *txn;
svn_stringbuf_t *conflict;
};
static svn_error_t *
txn_body_merge(void *baton, trail_t *trail) {
struct merge_args *args = baton;
dag_node_t *source_node, *txn_root_node, *ancestor_node;
const svn_fs_id_t *source_id;
svn_fs_t *fs = args->txn->fs;
const char *txn_id = args->txn->id;
source_node = args->source_node;
ancestor_node = args->ancestor_node;
source_id = svn_fs_base__dag_get_id(source_node);
SVN_ERR(svn_fs_base__dag_txn_root(&txn_root_node, fs, txn_id,
trail, trail->pool));
if (ancestor_node == NULL) {
SVN_ERR(svn_fs_base__dag_txn_base_root(&ancestor_node, fs,
txn_id, trail, trail->pool));
}
if (svn_fs_base__id_eq(svn_fs_base__dag_get_id(ancestor_node),
svn_fs_base__dag_get_id(txn_root_node))) {
SVN_ERR(svn_fs_base__set_txn_base(fs, txn_id, source_id,
trail, trail->pool));
SVN_ERR(svn_fs_base__set_txn_root(fs, txn_id, source_id,
trail, trail->pool));
} else {
int pred_count;
SVN_ERR(merge(args->conflict, "/", txn_root_node, source_node,
ancestor_node, txn_id, NULL, trail, trail->pool));
SVN_ERR(svn_fs_base__dag_get_predecessor_count(&pred_count,
source_node, trail,
trail->pool));
SVN_ERR(update_ancestry(fs, source_id,
svn_fs_base__dag_get_id(txn_root_node),
txn_id, "/", pred_count, trail, trail->pool));
SVN_ERR(svn_fs_base__set_txn_base(fs, txn_id, source_id,
trail, trail->pool));
}
return SVN_NO_ERROR;
}
static svn_error_t *
verify_locks(const char *txn_name,
trail_t *trail,
apr_pool_t *pool) {
apr_pool_t *subpool = svn_pool_create(pool);
apr_hash_t *changes;
apr_hash_index_t *hi;
apr_array_header_t *changed_paths;
svn_stringbuf_t *last_recursed = NULL;
int i;
SVN_ERR(svn_fs_bdb__changes_fetch(&changes, trail->fs, txn_name,
trail, pool));
changed_paths = apr_array_make(pool, apr_hash_count(changes) + 1,
sizeof(const char *));
for (hi = apr_hash_first(pool, changes); hi; hi = apr_hash_next(hi)) {
const void *key;
apr_hash_this(hi, &key, NULL, NULL);
APR_ARRAY_PUSH(changed_paths, const char *) = key;
}
qsort(changed_paths->elts, changed_paths->nelts,
changed_paths->elt_size, svn_sort_compare_paths);
for (i = 0; i < changed_paths->nelts; i++) {
const char *path;
svn_fs_path_change_t *change;
svn_boolean_t recurse = TRUE;
svn_pool_clear(subpool);
path = APR_ARRAY_IDX(changed_paths, i, const char *);
if (last_recursed
&& svn_path_is_child(last_recursed->data, path, subpool))
continue;
change = apr_hash_get(changes, path, APR_HASH_KEY_STRING);
if (change->change_kind == svn_fs_path_change_modify)
recurse = FALSE;
SVN_ERR(svn_fs_base__allow_locked_operation(path, recurse,
trail, subpool));
if (recurse) {
if (! last_recursed)
last_recursed = svn_stringbuf_create(path, pool);
else
svn_stringbuf_set(last_recursed, path);
}
}
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
struct commit_args {
svn_fs_txn_t *txn;
svn_revnum_t new_rev;
};
static svn_error_t *
txn_body_commit(void *baton, trail_t *trail) {
struct commit_args *args = baton;
svn_fs_txn_t *txn = args->txn;
svn_fs_t *fs = txn->fs;
const char *txn_name = txn->id;
svn_revnum_t youngest_rev;
const svn_fs_id_t *y_rev_root_id;
dag_node_t *txn_base_root_node;
SVN_ERR(svn_fs_bdb__youngest_rev(&youngest_rev, fs, trail, trail->pool));
SVN_ERR(svn_fs_base__rev_get_root(&y_rev_root_id, fs, youngest_rev,
trail, trail->pool));
SVN_ERR(svn_fs_base__dag_txn_base_root(&txn_base_root_node, fs, txn_name,
trail, trail->pool));
if (! svn_fs_base__id_eq(y_rev_root_id,
svn_fs_base__dag_get_id(txn_base_root_node))) {
svn_string_t *id_str = svn_fs_base__id_unparse(y_rev_root_id,
trail->pool);
return svn_error_createf
(SVN_ERR_FS_TXN_OUT_OF_DATE, NULL,
_("Transaction '%s' out-of-date with respect to revision '%s'"),
txn_name, id_str->data);
}
SVN_ERR(verify_locks(txn_name, trail, trail->pool));
SVN_ERR(svn_fs_base__dag_commit_txn(&(args->new_rev), txn, trail,
trail->pool));
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_base__commit_txn(const char **conflict_p,
svn_revnum_t *new_rev,
svn_fs_txn_t *txn,
apr_pool_t *pool) {
svn_error_t *err;
svn_fs_t *fs = txn->fs;
apr_pool_t *subpool = svn_pool_create(pool);
*new_rev = SVN_INVALID_REVNUM;
if (conflict_p)
*conflict_p = NULL;
while (1729) {
struct get_root_args get_root_args;
struct merge_args merge_args;
struct commit_args commit_args;
svn_revnum_t youngish_rev;
svn_fs_root_t *youngish_root;
dag_node_t *youngish_root_node;
svn_pool_clear(subpool);
SVN_ERR(svn_fs_base__youngest_rev(&youngish_rev, fs, subpool));
SVN_ERR(svn_fs_base__revision_root(&youngish_root, fs, youngish_rev,
subpool));
get_root_args.root = youngish_root;
SVN_ERR(svn_fs_base__retry_txn(fs, txn_body_get_root,
&get_root_args, subpool));
youngish_root_node = get_root_args.node;
merge_args.ancestor_node = NULL;
merge_args.source_node = youngish_root_node;
merge_args.txn = txn;
merge_args.conflict = svn_stringbuf_create("", pool);
err = svn_fs_base__retry_txn(fs, txn_body_merge, &merge_args, subpool);
if (err) {
if ((err->apr_err == SVN_ERR_FS_CONFLICT) && conflict_p)
*conflict_p = merge_args.conflict->data;
return err;
}
commit_args.txn = txn;
err = svn_fs_base__retry_txn(fs, txn_body_commit, &commit_args,
subpool);
if (err && (err->apr_err == SVN_ERR_FS_TXN_OUT_OF_DATE)) {
svn_revnum_t youngest_rev;
svn_error_t *err2 = svn_fs_base__youngest_rev(&youngest_rev, fs,
subpool);
if (err2) {
svn_error_clear(err);
return err2;
} else if (youngest_rev == youngish_rev)
return err;
else
svn_error_clear(err);
} else if (err) {
return err;
} else {
*new_rev = commit_args.new_rev;
break;
}
}
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
static svn_error_t *
base_merge(const char **conflict_p,
svn_fs_root_t *source_root,
const char *source_path,
svn_fs_root_t *target_root,
const char *target_path,
svn_fs_root_t *ancestor_root,
const char *ancestor_path,
apr_pool_t *pool) {
dag_node_t *source, *ancestor;
struct get_root_args get_root_args;
struct merge_args merge_args;
svn_fs_txn_t *txn;
svn_error_t *err;
svn_fs_t *fs;
if (! target_root->is_txn_root)
return SVN_FS__NOT_TXN(target_root);
fs = ancestor_root->fs;
if ((source_root->fs != fs) || (target_root->fs != fs)) {
return svn_error_create
(SVN_ERR_FS_CORRUPT, NULL,
_("Bad merge; ancestor, source, and target not all in same fs"));
}
get_root_args.root = ancestor_root;
SVN_ERR(svn_fs_base__retry_txn(fs, txn_body_get_root, &get_root_args,
pool));
ancestor = get_root_args.node;
get_root_args.root = source_root;
SVN_ERR(svn_fs_base__retry_txn(fs, txn_body_get_root, &get_root_args,
pool));
source = get_root_args.node;
SVN_ERR(svn_fs_base__open_txn(&txn, fs, target_root->txn, pool));
merge_args.source_node = source;
merge_args.ancestor_node = ancestor;
merge_args.txn = txn;
merge_args.conflict = svn_stringbuf_create("", pool);
err = svn_fs_base__retry_txn(fs, txn_body_merge, &merge_args, pool);
if (err) {
if ((err->apr_err == SVN_ERR_FS_CONFLICT) && conflict_p)
*conflict_p = merge_args.conflict->data;
return err;
}
return SVN_NO_ERROR;
}
struct rev_get_txn_id_args {
const char **txn_id;
svn_revnum_t revision;
};
static svn_error_t *
txn_body_rev_get_txn_id(void *baton, trail_t *trail) {
struct rev_get_txn_id_args *args = baton;
return svn_fs_base__rev_get_txn_id(args->txn_id, trail->fs,
args->revision, trail, trail->pool);
}
svn_error_t *
svn_fs_base__deltify(svn_fs_t *fs,
svn_revnum_t revision,
apr_pool_t *pool) {
svn_fs_root_t *root;
const char *txn_id;
struct rev_get_txn_id_args args;
SVN_ERR(svn_fs_base__revision_root(&root, fs, revision, pool));
args.txn_id = &txn_id;
args.revision = revision;
SVN_ERR(svn_fs_base__retry_txn(fs, txn_body_rev_get_txn_id, &args, pool));
return deltify_mutable(fs, root, "/", NULL, svn_node_dir, txn_id, pool);
}
struct make_dir_args {
svn_fs_root_t *root;
const char *path;
};
static svn_error_t *
txn_body_make_dir(void *baton,
trail_t *trail) {
struct make_dir_args *args = baton;
svn_fs_root_t *root = args->root;
const char *path = args->path;
parent_path_t *parent_path;
dag_node_t *sub_dir;
const char *txn_id = root->txn;
SVN_ERR(open_path(&parent_path, root, path, open_path_last_optional,
txn_id, trail, trail->pool));
if (parent_path->node)
return SVN_FS__ALREADY_EXISTS(root, path);
if (args->root->txn_flags & SVN_FS_TXN_CHECK_LOCKS) {
SVN_ERR(svn_fs_base__allow_locked_operation(path, TRUE,
trail, trail->pool));
}
SVN_ERR(make_path_mutable(root, parent_path->parent, path,
trail, trail->pool));
SVN_ERR(svn_fs_base__dag_make_dir(&sub_dir,
parent_path->parent->node,
parent_path_path(parent_path->parent,
trail->pool),
parent_path->entry,
txn_id,
trail, trail->pool));
SVN_ERR(add_change(root->fs, txn_id, path,
svn_fs_base__dag_get_id(sub_dir),
svn_fs_path_change_add, FALSE, FALSE,
trail, trail->pool));
return SVN_NO_ERROR;
}
static svn_error_t *
base_make_dir(svn_fs_root_t *root,
const char *path,
apr_pool_t *pool) {
struct make_dir_args args;
if (! root->is_txn_root)
return SVN_FS__NOT_TXN(root);
args.root = root;
args.path = path;
return svn_fs_base__retry_txn(root->fs, txn_body_make_dir, &args, pool);
}
struct delete_args {
svn_fs_root_t *root;
const char *path;
};
static svn_error_t *
txn_body_delete(void *baton,
trail_t *trail) {
struct delete_args *args = baton;
svn_fs_root_t *root = args->root;
const char *path = args->path;
parent_path_t *parent_path;
const char *txn_id = root->txn;
base_fs_data_t *bfd = trail->fs->fsap_data;
if (! root->is_txn_root)
return SVN_FS__NOT_TXN(root);
SVN_ERR(open_path(&parent_path, root, path, 0, txn_id,
trail, trail->pool));
if (! parent_path->parent)
return svn_error_create(SVN_ERR_FS_ROOT_DIR, NULL,
_("The root directory cannot be deleted"));
if (root->txn_flags & SVN_FS_TXN_CHECK_LOCKS) {
SVN_ERR(svn_fs_base__allow_locked_operation(path, TRUE,
trail, trail->pool));
}
SVN_ERR(make_path_mutable(root, parent_path->parent, path,
trail, trail->pool));
if (bfd->format >= SVN_FS_BASE__MIN_MERGEINFO_FORMAT) {
apr_int64_t mergeinfo_count;
SVN_ERR(svn_fs_base__dag_get_mergeinfo_stats(NULL, &mergeinfo_count,
parent_path->node,
trail, trail->pool));
SVN_ERR(adjust_parent_mergeinfo_counts(parent_path->parent,
-mergeinfo_count, txn_id,
trail, trail->pool));
}
SVN_ERR(svn_fs_base__dag_delete(parent_path->parent->node,
parent_path->entry,
txn_id, trail, trail->pool));
SVN_ERR(add_change(root->fs, txn_id, path,
svn_fs_base__dag_get_id(parent_path->node),
svn_fs_path_change_delete, FALSE, FALSE, trail,
trail->pool));
return SVN_NO_ERROR;
}
static svn_error_t *
base_delete_node(svn_fs_root_t *root,
const char *path,
apr_pool_t *pool) {
struct delete_args args;
args.root = root;
args.path = path;
return svn_fs_base__retry_txn(root->fs, txn_body_delete, &args, pool);
}
struct copy_args {
svn_fs_root_t *from_root;
const char *from_path;
svn_fs_root_t *to_root;
const char *to_path;
svn_boolean_t preserve_history;
};
static svn_error_t *
txn_body_copy(void *baton,
trail_t *trail) {
struct copy_args *args = baton;
svn_fs_root_t *from_root = args->from_root;
const char *from_path = args->from_path;
svn_fs_root_t *to_root = args->to_root;
const char *to_path = args->to_path;
dag_node_t *from_node;
parent_path_t *to_parent_path;
const char *txn_id = to_root->txn;
SVN_ERR(get_dag(&from_node, from_root, from_path, trail, trail->pool));
SVN_ERR(open_path(&to_parent_path, to_root, to_path,
open_path_last_optional, txn_id, trail, trail->pool));
if (to_root->txn_flags & SVN_FS_TXN_CHECK_LOCKS) {
SVN_ERR(svn_fs_base__allow_locked_operation(to_path, TRUE,
trail, trail->pool));
}
if ((to_parent_path->node)
&& (svn_fs_base__id_compare(svn_fs_base__dag_get_id(from_node),
svn_fs_base__dag_get_id
(to_parent_path->node)) == 0))
return SVN_NO_ERROR;
if (! from_root->is_txn_root) {
svn_fs_path_change_kind_t kind;
dag_node_t *new_node;
apr_int64_t old_mergeinfo_count = 0, mergeinfo_count;
base_fs_data_t *bfd = trail->fs->fsap_data;
if (to_parent_path->node)
kind = svn_fs_path_change_replace;
else
kind = svn_fs_path_change_add;
SVN_ERR(make_path_mutable(to_root, to_parent_path->parent,
to_path, trail, trail->pool));
if (to_parent_path->node)
SVN_ERR(svn_fs_base__dag_get_mergeinfo_stats(NULL,
&old_mergeinfo_count,
to_parent_path->node,
trail, trail->pool));
SVN_ERR(svn_fs_base__dag_copy(to_parent_path->parent->node,
to_parent_path->entry,
from_node,
args->preserve_history,
from_root->rev,
from_path, txn_id, trail, trail->pool));
if (bfd->format >= SVN_FS_BASE__MIN_MERGEINFO_FORMAT) {
SVN_ERR(svn_fs_base__dag_get_mergeinfo_stats(NULL,
&mergeinfo_count,
from_node, trail,
trail->pool));
SVN_ERR(adjust_parent_mergeinfo_counts
(to_parent_path->parent,
mergeinfo_count - old_mergeinfo_count,
txn_id, trail, trail->pool));
}
SVN_ERR(get_dag(&new_node, to_root, to_path, trail, trail->pool));
SVN_ERR(add_change(to_root->fs, txn_id, to_path,
svn_fs_base__dag_get_id(new_node),
kind, FALSE, FALSE, trail, trail->pool));
} else {
abort();
}
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
struct copy_args args;
svn_boolean_t same_p;
SVN_ERR(fs_same_p(&same_p, from_root->fs, to_root->fs, pool));
if (! same_p)
return svn_error_createf
(SVN_ERR_UNSUPPORTED_FEATURE, NULL,
_("Cannot copy between two different filesystems ('%s' and '%s')"),
from_root->fs->path, to_root->fs->path);
if (! to_root->is_txn_root)
return SVN_FS__NOT_TXN(to_root);
if (from_root->is_txn_root)
return svn_error_create
(SVN_ERR_UNSUPPORTED_FEATURE, NULL,
_("Copy from mutable tree not currently supported"));
args.from_root = from_root;
args.from_path = from_path;
args.to_root = to_root;
args.to_path = to_path;
args.preserve_history = preserve_history;
return svn_fs_base__retry_txn(to_root->fs, txn_body_copy, &args, pool);
}
static svn_error_t *
base_copy(svn_fs_root_t *from_root,
const char *from_path,
svn_fs_root_t *to_root,
const char *to_path,
apr_pool_t *pool) {
return copy_helper(from_root, from_path, to_root, to_path, TRUE, pool);
}
static svn_error_t *
base_revision_link(svn_fs_root_t *from_root,
svn_fs_root_t *to_root,
const char *path,
apr_pool_t *pool) {
return copy_helper(from_root, path, to_root, path, FALSE, pool);
}
struct copied_from_args {
svn_fs_root_t *root;
const char *path;
svn_revnum_t result_rev;
const char *result_path;
apr_pool_t *pool;
};
static svn_error_t *
txn_body_copied_from(void *baton, trail_t *trail) {
struct copied_from_args *args = baton;
const svn_fs_id_t *node_id, *pred_id;
dag_node_t *node;
svn_fs_t *fs = args->root->fs;
args->result_path = NULL;
args->result_rev = SVN_INVALID_REVNUM;
SVN_ERR(get_dag(&node, args->root, args->path, trail, trail->pool));
node_id = svn_fs_base__dag_get_id(node);
SVN_ERR(svn_fs_base__dag_get_predecessor_id(&pred_id, node,
trail, trail->pool));
if (! pred_id)
return SVN_NO_ERROR;
if (svn_fs_base__key_compare(svn_fs_base__id_copy_id(node_id),
svn_fs_base__id_copy_id(pred_id)) != 0) {
copy_t *copy;
SVN_ERR(svn_fs_bdb__get_copy(&copy, fs,
svn_fs_base__id_copy_id(node_id),
trail, trail->pool));
if ((copy->kind == copy_kind_real)
&& svn_fs_base__id_eq(copy->dst_noderev_id, node_id)) {
args->result_path = copy->src_path;
SVN_ERR(svn_fs_base__txn_get_revision(&(args->result_rev), fs,
copy->src_txn_id,
trail, trail->pool));
}
}
return SVN_NO_ERROR;
}
static svn_error_t *
base_copied_from(svn_revnum_t *rev_p,
const char **path_p,
svn_fs_root_t *root,
const char *path,
apr_pool_t *pool) {
struct copied_from_args args;
args.root = root;
args.path = path;
args.pool = pool;
SVN_ERR(svn_fs_base__retry_txn(root->fs,
txn_body_copied_from, &args, pool));
*rev_p = args.result_rev;
*path_p = args.result_path;
return SVN_NO_ERROR;
}
struct make_file_args {
svn_fs_root_t *root;
const char *path;
};
static svn_error_t *
txn_body_make_file(void *baton,
trail_t *trail) {
struct make_file_args *args = baton;
svn_fs_root_t *root = args->root;
const char *path = args->path;
parent_path_t *parent_path;
dag_node_t *child;
const char *txn_id = root->txn;
SVN_ERR(open_path(&parent_path, root, path, open_path_last_optional,
txn_id, trail, trail->pool));
if (parent_path->node)
return SVN_FS__ALREADY_EXISTS(root, path);
if (args->root->txn_flags & SVN_FS_TXN_CHECK_LOCKS) {
SVN_ERR(svn_fs_base__allow_locked_operation(path, TRUE,
trail, trail->pool));
}
SVN_ERR(make_path_mutable(root, parent_path->parent, path,
trail, trail->pool));
SVN_ERR(svn_fs_base__dag_make_file(&child,
parent_path->parent->node,
parent_path_path(parent_path->parent,
trail->pool),
parent_path->entry,
txn_id,
trail, trail->pool));
SVN_ERR(add_change(root->fs, txn_id, path,
svn_fs_base__dag_get_id(child),
svn_fs_path_change_add, TRUE, FALSE,
trail, trail->pool));
return SVN_NO_ERROR;
}
static svn_error_t *
base_make_file(svn_fs_root_t *root,
const char *path,
apr_pool_t *pool) {
struct make_file_args args;
args.root = root;
args.path = path;
return svn_fs_base__retry_txn(root->fs, txn_body_make_file, &args, pool);
}
struct file_length_args {
svn_fs_root_t *root;
const char *path;
svn_filesize_t length;
};
static svn_error_t *
txn_body_file_length(void *baton,
trail_t *trail) {
struct file_length_args *args = baton;
dag_node_t *file;
SVN_ERR(get_dag(&file, args->root, args->path, trail, trail->pool));
return svn_fs_base__dag_file_length(&args->length, file,
trail, trail->pool);
}
static svn_error_t *
base_file_length(svn_filesize_t *length_p,
svn_fs_root_t *root,
const char *path,
apr_pool_t *pool) {
struct file_length_args args;
args.root = root;
args.path = path;
SVN_ERR(svn_fs_base__retry_txn(root->fs, txn_body_file_length, &args,
pool));
*length_p = args.length;
return SVN_NO_ERROR;
}
struct file_checksum_args {
svn_fs_root_t *root;
const char *path;
unsigned char *digest;
};
static svn_error_t *
txn_body_file_checksum(void *baton,
trail_t *trail) {
struct file_checksum_args *args = baton;
dag_node_t *file;
SVN_ERR(get_dag(&file, args->root, args->path, trail, trail->pool));
return svn_fs_base__dag_file_checksum(args->digest, file,
trail, trail->pool);
}
static svn_error_t *
base_file_md5_checksum(unsigned char digest[],
svn_fs_root_t *root,
const char *path,
apr_pool_t *pool) {
struct file_checksum_args args;
args.root = root;
args.path = path;
args.digest = digest;
SVN_ERR(svn_fs_base__retry_txn(root->fs, txn_body_file_checksum, &args,
pool));
return SVN_NO_ERROR;
}
typedef struct file_contents_baton_t {
svn_fs_root_t *root;
const char *path;
dag_node_t *node;
apr_pool_t *pool;
svn_stream_t *file_stream;
} file_contents_baton_t;
static svn_error_t *
txn_body_get_file_contents(void *baton, trail_t *trail) {
file_contents_baton_t *fb = (file_contents_baton_t *) baton;
SVN_ERR(get_dag(&(fb->node), fb->root, fb->path, trail, trail->pool));
SVN_ERR(svn_fs_base__dag_get_contents(&(fb->file_stream),
fb->node,
trail, fb->pool));
return SVN_NO_ERROR;
}
static svn_error_t *
base_file_contents(svn_stream_t **contents,
svn_fs_root_t *root,
const char *path,
apr_pool_t *pool) {
file_contents_baton_t *fb = apr_pcalloc(pool, sizeof(*fb));
fb->root = root;
fb->path = path;
fb->pool = pool;
SVN_ERR(svn_fs_base__retry_txn(root->fs, txn_body_get_file_contents,
fb, pool));
*contents = fb->file_stream;
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
txn_body_txdelta_finalize_edits(void *baton, trail_t *trail) {
txdelta_baton_t *tb = (txdelta_baton_t *) baton;
return svn_fs_base__dag_finalize_edits(tb->node,
tb->result_checksum,
tb->root->txn,
trail, trail->pool);
}
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
SVN_ERR(svn_fs_base__retry_txn(tb->root->fs,
txn_body_txdelta_finalize_edits, tb,
tb->pool));
}
return SVN_NO_ERROR;
}
static svn_error_t *
txn_body_apply_textdelta(void *baton, trail_t *trail) {
txdelta_baton_t *tb = (txdelta_baton_t *) baton;
parent_path_t *parent_path;
const char *txn_id = tb->root->txn;
SVN_ERR(open_path(&parent_path, tb->root, tb->path, 0, txn_id,
trail, trail->pool));
if (tb->root->txn_flags & SVN_FS_TXN_CHECK_LOCKS)
SVN_ERR(svn_fs_base__allow_locked_operation(tb->path, FALSE,
trail, trail->pool));
SVN_ERR(make_path_mutable(tb->root, parent_path, tb->path,
trail, trail->pool));
tb->node = parent_path->node;
if (tb->base_checksum) {
unsigned char digest[APR_MD5_DIGESTSIZE];
const char *hex;
SVN_ERR(svn_fs_base__dag_file_checksum(digest, tb->node,
trail, trail->pool));
hex = svn_md5_digest_to_cstring(digest, trail->pool);
if (hex && (strcmp(tb->base_checksum, hex) != 0))
return svn_error_createf
(SVN_ERR_CHECKSUM_MISMATCH,
NULL,
_("Base checksum mismatch on '%s':\n"
" expected: %s\n"
" actual: %s\n"),
tb->path, tb->base_checksum, hex);
}
SVN_ERR(svn_fs_base__dag_get_contents(&(tb->source_stream),
tb->node, trail, tb->pool));
SVN_ERR(svn_fs_base__dag_get_edit_stream(&(tb->target_stream), tb->node,
txn_id, trail, tb->pool));
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
svn_fs_base__dag_get_id(tb->node),
svn_fs_path_change_modify, TRUE, FALSE, trail,
trail->pool));
return SVN_NO_ERROR;
}
static svn_error_t *
base_apply_textdelta(svn_txdelta_window_handler_t *contents_p,
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
SVN_ERR(svn_fs_base__retry_txn(root->fs, txn_body_apply_textdelta, tb,
pool));
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
txn_body_fulltext_finalize_edits(void *baton, trail_t *trail) {
struct text_baton_t *tb = baton;
return svn_fs_base__dag_finalize_edits(tb->node,
tb->result_checksum,
tb->root->txn,
trail, trail->pool);
}
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
SVN_ERR(svn_fs_base__retry_txn(tb->root->fs,
txn_body_fulltext_finalize_edits, tb,
tb->pool));
return SVN_NO_ERROR;
}
static svn_error_t *
txn_body_apply_text(void *baton, trail_t *trail) {
struct text_baton_t *tb = baton;
parent_path_t *parent_path;
const char *txn_id = tb->root->txn;
SVN_ERR(open_path(&parent_path, tb->root, tb->path, 0, txn_id,
trail, trail->pool));
if (tb->root->txn_flags & SVN_FS_TXN_CHECK_LOCKS)
SVN_ERR(svn_fs_base__allow_locked_operation(tb->path, FALSE,
trail, trail->pool));
SVN_ERR(make_path_mutable(tb->root, parent_path, tb->path,
trail, trail->pool));
tb->node = parent_path->node;
SVN_ERR(svn_fs_base__dag_get_edit_stream(&(tb->file_stream), tb->node,
txn_id, trail, tb->pool));
tb->stream = svn_stream_create(tb, tb->pool);
svn_stream_set_write(tb->stream, text_stream_writer);
svn_stream_set_close(tb->stream, text_stream_closer);
SVN_ERR(add_change(tb->root->fs, txn_id, tb->path,
svn_fs_base__dag_get_id(tb->node),
svn_fs_path_change_modify, TRUE, FALSE, trail,
trail->pool));
return SVN_NO_ERROR;
}
static svn_error_t *
base_apply_text(svn_stream_t **contents_p,
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
SVN_ERR(svn_fs_base__retry_txn(root->fs, txn_body_apply_text, tb, pool));
*contents_p = tb->stream;
return SVN_NO_ERROR;
}
static svn_error_t *
txn_body_contents_changed(void *baton, trail_t *trail) {
struct things_changed_args *args = baton;
dag_node_t *node1, *node2;
SVN_ERR(get_dag(&node1, args->root1, args->path1, trail, trail->pool));
SVN_ERR(get_dag(&node2, args->root2, args->path2, trail, trail->pool));
SVN_ERR(svn_fs_base__things_different(NULL, args->changed_p,
node1, node2, trail, trail->pool));
return SVN_NO_ERROR;
}
static svn_error_t *
base_contents_changed(svn_boolean_t *changed_p,
svn_fs_root_t *root1,
const char *path1,
svn_fs_root_t *root2,
const char *path2,
apr_pool_t *pool) {
struct things_changed_args args;
if (root1->fs != root2->fs)
return svn_error_create
(SVN_ERR_FS_GENERAL, NULL,
_("Cannot compare file contents between two different filesystems"));
{
svn_node_kind_t kind;
SVN_ERR(base_check_path(&kind, root1, path1, pool));
if (kind != svn_node_file)
return svn_error_createf
(SVN_ERR_FS_GENERAL, NULL, _("'%s' is not a file"), path1);
SVN_ERR(base_check_path(&kind, root2, path2, pool));
if (kind != svn_node_file)
return svn_error_createf
(SVN_ERR_FS_GENERAL, NULL, _("'%s' is not a file"), path2);
}
args.root1 = root1;
args.root2 = root2;
args.path1 = path1;
args.path2 = path2;
args.changed_p = changed_p;
args.pool = pool;
SVN_ERR(svn_fs_base__retry_txn(root1->fs, txn_body_contents_changed,
&args, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
base_get_file_delta_stream(svn_txdelta_stream_t **stream_p,
svn_fs_root_t *source_root,
const char *source_path,
svn_fs_root_t *target_root,
const char *target_path,
apr_pool_t *pool) {
svn_stream_t *source, *target;
svn_txdelta_stream_t *delta_stream;
if (source_root && source_path)
SVN_ERR(base_file_contents(&source, source_root, source_path, pool));
else
source = svn_stream_empty(pool);
SVN_ERR(base_file_contents(&target, target_root, target_path, pool));
svn_txdelta(&delta_stream, source, target, pool);
*stream_p = delta_stream;
return SVN_NO_ERROR;
}
struct paths_changed_args {
apr_hash_t *changes;
svn_fs_root_t *root;
};
static svn_error_t *
txn_body_paths_changed(void *baton,
trail_t *trail) {
struct paths_changed_args *args = baton;
const char *txn_id;
svn_fs_t *fs = args->root->fs;
if (! args->root->is_txn_root)
SVN_ERR(svn_fs_base__rev_get_txn_id(&txn_id, fs, args->root->rev,
trail, trail->pool));
else
txn_id = args->root->txn;
return svn_fs_bdb__changes_fetch(&(args->changes), fs, txn_id,
trail, trail->pool);
}
static svn_error_t *
base_paths_changed(apr_hash_t **changed_paths_p,
svn_fs_root_t *root,
apr_pool_t *pool) {
struct paths_changed_args args;
args.root = root;
args.changes = NULL;
SVN_ERR(svn_fs_base__retry(root->fs, txn_body_paths_changed, &args, pool));
*changed_paths_p = args.changes;
return SVN_NO_ERROR;
}
typedef struct {
svn_fs_t *fs;
const char *path;
svn_revnum_t revision;
const char *path_hint;
svn_revnum_t rev_hint;
svn_boolean_t is_interesting;
} base_history_data_t;
static svn_fs_history_t *assemble_history(svn_fs_t *fs, const char *path,
svn_revnum_t revision,
svn_boolean_t is_interesting,
const char *path_hint,
svn_revnum_t rev_hint,
apr_pool_t *pool);
static svn_error_t *
base_node_history(svn_fs_history_t **history_p,
svn_fs_root_t *root,
const char *path,
apr_pool_t *pool) {
svn_node_kind_t kind;
if (root->is_txn_root)
return svn_error_create(SVN_ERR_FS_NOT_REVISION_ROOT, NULL, NULL);
SVN_ERR(base_check_path(&kind, root, path, pool));
if (kind == svn_node_none)
return SVN_FS__NOT_FOUND(root, path);
*history_p = assemble_history(root->fs,
svn_fs__canonicalize_abspath(path, pool),
root->rev, FALSE, NULL,
SVN_INVALID_REVNUM, pool);
return SVN_NO_ERROR;
}
static svn_error_t *
examine_copy_inheritance(const char **copy_id,
copy_t **copy,
svn_fs_t *fs,
parent_path_t *parent_path,
trail_t *trail,
apr_pool_t *pool) {
*copy_id = svn_fs_base__id_copy_id
(svn_fs_base__dag_get_id(parent_path->node));
*copy = NULL;
if (! parent_path->parent)
return SVN_NO_ERROR;
if (parent_path->copy_inherit == copy_id_inherit_self) {
if (((*copy_id)[0] == '0') && ((*copy_id)[1] == '\0'))
return SVN_NO_ERROR;
SVN_ERR(svn_fs_bdb__get_copy(copy, fs, *copy_id, trail, pool));
if ((*copy)->kind != copy_kind_soft)
return SVN_NO_ERROR;
}
return examine_copy_inheritance(copy_id, copy, fs,
parent_path->parent, trail, pool);
}
struct history_prev_args {
svn_fs_history_t **prev_history_p;
svn_fs_history_t *history;
svn_boolean_t cross_copies;
apr_pool_t *pool;
};
static svn_error_t *
txn_body_history_prev(void *baton, trail_t *trail) {
struct history_prev_args *args = baton;
svn_fs_history_t **prev_history = args->prev_history_p;
svn_fs_history_t *history = args->history;
base_history_data_t *bhd = history->fsap_data;
const char *commit_path, *src_path, *path = bhd->path;
svn_revnum_t commit_rev, src_rev, dst_rev, revision = bhd->revision;
apr_pool_t *retpool = args->pool;
svn_fs_t *fs = bhd->fs;
parent_path_t *parent_path;
dag_node_t *node;
svn_fs_root_t *root;
const svn_fs_id_t *node_id;
const char *end_copy_id = NULL;
struct revision_root_args rr_args;
svn_boolean_t reported = bhd->is_interesting;
const char *txn_id;
copy_t *copy = NULL;
svn_boolean_t retry = FALSE;
*prev_history = NULL;
if (bhd->path_hint && SVN_IS_VALID_REVNUM(bhd->rev_hint)) {
reported = FALSE;
if (! args->cross_copies)
return SVN_NO_ERROR;
path = bhd->path_hint;
revision = bhd->rev_hint;
}
rr_args.root_p = &root;
rr_args.rev = revision;
SVN_ERR(txn_body_revision_root(&rr_args, trail));
SVN_ERR(svn_fs_base__rev_get_txn_id(&txn_id, fs, revision, trail,
trail->pool));
SVN_ERR(open_path(&parent_path, root, path, 0, txn_id,
trail, trail->pool));
node = parent_path->node;
node_id = svn_fs_base__dag_get_id(node);
commit_path = svn_fs_base__dag_get_created_path(node);
SVN_ERR(svn_fs_base__dag_get_revision(&commit_rev, node,
trail, trail->pool));
if (revision == commit_rev) {
if (! reported) {
*prev_history = assemble_history(fs,
apr_pstrdup(retpool, commit_path),
commit_rev, TRUE, NULL,
SVN_INVALID_REVNUM, retpool);
return SVN_NO_ERROR;
} else {
const svn_fs_id_t *pred_id;
SVN_ERR(svn_fs_base__dag_get_predecessor_id(&pred_id, node,
trail, trail->pool));
if (! pred_id)
return SVN_NO_ERROR;
SVN_ERR(svn_fs_base__dag_get_node(&node, fs, pred_id,
trail, trail->pool));
node_id = svn_fs_base__dag_get_id(node);
commit_path = svn_fs_base__dag_get_created_path(node);
SVN_ERR(svn_fs_base__dag_get_revision(&commit_rev, node,
trail, trail->pool));
}
}
SVN_ERR(examine_copy_inheritance(&end_copy_id, &copy, fs,
parent_path, trail, trail->pool));
src_path = NULL;
src_rev = SVN_INVALID_REVNUM;
dst_rev = SVN_INVALID_REVNUM;
if (svn_fs_base__key_compare(svn_fs_base__id_copy_id(node_id),
end_copy_id) != 0) {
const char *remainder;
dag_node_t *dst_node;
const char *copy_dst;
if (! copy)
SVN_ERR(svn_fs_bdb__get_copy(&copy, fs, end_copy_id, trail,
trail->pool));
SVN_ERR(svn_fs_base__dag_get_node(&dst_node, fs,
copy->dst_noderev_id,
trail, trail->pool));
copy_dst = svn_fs_base__dag_get_created_path(dst_node);
if (strcmp(path, copy_dst) == 0)
remainder = "";
else
remainder = svn_path_is_child(copy_dst, path, trail->pool);
if (remainder) {
SVN_ERR(svn_fs_base__txn_get_revision
(&src_rev, fs, copy->src_txn_id, trail, trail->pool));
SVN_ERR(svn_fs_base__txn_get_revision
(&dst_rev, fs,
svn_fs_base__id_txn_id(copy->dst_noderev_id),
trail, trail->pool));
src_path = svn_path_join(copy->src_path, remainder,
trail->pool);
if (copy->kind == copy_kind_soft)
retry = TRUE;
}
}
if (src_path && SVN_IS_VALID_REVNUM(src_rev) && (src_rev >= commit_rev)) {
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
base_history_prev(svn_fs_history_t **prev_history_p,
svn_fs_history_t *history,
svn_boolean_t cross_copies,
apr_pool_t *pool) {
svn_fs_history_t *prev_history = NULL;
base_history_data_t *bhd = history->fsap_data;
svn_fs_t *fs = bhd->fs;
if (strcmp(bhd->path, "/") == 0) {
if (! bhd->is_interesting)
prev_history = assemble_history(fs, "/", bhd->revision,
1, NULL, SVN_INVALID_REVNUM, pool);
else if (bhd->revision > 0)
prev_history = assemble_history(fs, "/", bhd->revision - 1,
1, NULL, SVN_INVALID_REVNUM, pool);
} else {
struct history_prev_args args;
prev_history = history;
while (1) {
args.prev_history_p = &prev_history;
args.history = prev_history;
args.cross_copies = cross_copies;
args.pool = pool;
SVN_ERR(svn_fs_base__retry_txn(fs, txn_body_history_prev, &args,
pool));
if (! prev_history)
break;
bhd = prev_history->fsap_data;
if (bhd->is_interesting)
break;
}
}
*prev_history_p = prev_history;
return SVN_NO_ERROR;
}
static svn_error_t *
base_history_location(const char **path,
svn_revnum_t *revision,
svn_fs_history_t *history,
apr_pool_t *pool) {
base_history_data_t *bhd = history->fsap_data;
*path = apr_pstrdup(pool, bhd->path);
*revision = bhd->revision;
return SVN_NO_ERROR;
}
static history_vtable_t history_vtable = {
base_history_prev,
base_history_location
};
struct closest_copy_args {
svn_fs_root_t **root_p;
const char **path_p;
svn_fs_root_t *root;
const char *path;
apr_pool_t *pool;
};
static svn_error_t *
txn_body_closest_copy(void *baton, trail_t *trail) {
struct closest_copy_args *args = baton;
svn_fs_root_t *root = args->root;
const char *path = args->path;
svn_fs_t *fs = root->fs;
parent_path_t *parent_path;
const svn_fs_id_t *node_id;
const char *txn_id, *copy_id;
copy_t *copy = NULL;
svn_fs_root_t *copy_dst_root;
dag_node_t *path_node_in_copy_dst, *copy_dst_node, *copy_dst_root_node;
const char *copy_dst_path;
svn_revnum_t copy_dst_rev, created_rev;
svn_error_t *err;
*(args->path_p) = NULL;
*(args->root_p) = NULL;
if (root->is_txn_root)
txn_id = root->txn;
else
SVN_ERR(svn_fs_base__rev_get_txn_id(&txn_id, fs, root->rev,
trail, trail->pool));
SVN_ERR(open_path(&parent_path, root, path, 0, txn_id,
trail, trail->pool));
node_id = svn_fs_base__dag_get_id(parent_path->node);
SVN_ERR(examine_copy_inheritance(&copy_id, &copy, fs, parent_path,
trail, trail->pool));
if (((copy_id)[0] == '0') && ((copy_id)[1] == '\0'))
return SVN_NO_ERROR;
if (! copy)
SVN_ERR(svn_fs_bdb__get_copy(&copy, fs, copy_id, trail, trail->pool));
SVN_ERR(svn_fs_base__dag_get_node(&copy_dst_node, fs, copy->dst_noderev_id,
trail, trail->pool));
copy_dst_path = svn_fs_base__dag_get_created_path(copy_dst_node);
SVN_ERR(svn_fs_base__dag_get_revision(&copy_dst_rev, copy_dst_node,
trail, trail->pool));
SVN_ERR(svn_fs_base__dag_revision_root(&copy_dst_root_node, fs,
copy_dst_rev, trail, args->pool));
copy_dst_root = make_revision_root(fs, copy_dst_rev,
copy_dst_root_node, args->pool);
err = get_dag(&path_node_in_copy_dst, copy_dst_root, path,
trail, trail->pool);
if (err) {
if ((err->apr_err == SVN_ERR_FS_NOT_FOUND)
|| (err->apr_err == SVN_ERR_FS_NOT_DIRECTORY)) {
svn_error_clear(err);
return SVN_NO_ERROR;
}
return err;
}
if ((svn_fs_base__dag_node_kind(path_node_in_copy_dst) == svn_node_none)
|| (! (svn_fs_base__id_check_related
(node_id, svn_fs_base__dag_get_id(path_node_in_copy_dst))))) {
return SVN_NO_ERROR;
}
SVN_ERR(svn_fs_base__dag_get_revision(&created_rev, path_node_in_copy_dst,
trail, trail->pool));
if (created_rev == copy_dst_rev) {
const svn_fs_id_t *pred_id;
SVN_ERR(svn_fs_base__dag_get_predecessor_id(&pred_id,
path_node_in_copy_dst,
trail, trail->pool));
if (! pred_id)
return SVN_NO_ERROR;
}
*(args->path_p) = apr_pstrdup(args->pool, copy_dst_path);
*(args->root_p) = copy_dst_root;
return SVN_NO_ERROR;
}
static svn_error_t *
base_closest_copy(svn_fs_root_t **root_p,
const char **path_p,
svn_fs_root_t *root,
const char *path,
apr_pool_t *pool) {
struct closest_copy_args args;
svn_fs_t *fs = root->fs;
svn_fs_root_t *closest_root = NULL;
const char *closest_path = NULL;
args.root_p = &closest_root;
args.path_p = &closest_path;
args.root = root;
args.path = path;
args.pool = pool;
SVN_ERR(svn_fs_base__retry_txn(fs, txn_body_closest_copy, &args, pool));
*root_p = closest_root;
*path_p = closest_path;
return SVN_NO_ERROR;
}
static svn_fs_history_t *
assemble_history(svn_fs_t *fs,
const char *path,
svn_revnum_t revision,
svn_boolean_t is_interesting,
const char *path_hint,
svn_revnum_t rev_hint,
apr_pool_t *pool) {
svn_fs_history_t *history = apr_pcalloc(pool, sizeof(*history));
base_history_data_t *bhd = apr_pcalloc(pool, sizeof(*bhd));
bhd->path = path;
bhd->revision = revision;
bhd->is_interesting = is_interesting;
bhd->path_hint = path_hint;
bhd->rev_hint = rev_hint;
bhd->fs = fs;
history->vtable = &history_vtable;
history->fsap_data = bhd;
return history;
}
svn_error_t *
svn_fs_base__get_path_kind(svn_node_kind_t *kind,
const char *path,
trail_t *trail,
apr_pool_t *pool) {
svn_revnum_t head_rev;
svn_fs_root_t *root;
dag_node_t *root_dir, *path_node;
svn_error_t *err;
SVN_ERR(svn_fs_bdb__youngest_rev(&head_rev, trail->fs, trail, pool));
SVN_ERR(svn_fs_base__dag_revision_root(&root_dir, trail->fs, head_rev,
trail, pool));
root = make_revision_root(trail->fs, head_rev, root_dir, pool);
err = get_dag(&path_node, root, path, trail, pool);
if (err && (err->apr_err == SVN_ERR_FS_NOT_FOUND)) {
svn_error_clear(err);
*kind = svn_node_none;
return SVN_NO_ERROR;
} else if (err)
return err;
*kind = svn_fs_base__dag_node_kind(path_node);
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_base__get_path_created_rev(svn_revnum_t *rev,
const char *path,
trail_t *trail,
apr_pool_t *pool) {
svn_revnum_t head_rev, created_rev;
svn_fs_root_t *root;
dag_node_t *root_dir, *path_node;
svn_error_t *err;
SVN_ERR(svn_fs_bdb__youngest_rev(&head_rev, trail->fs, trail, pool));
SVN_ERR(svn_fs_base__dag_revision_root(&root_dir, trail->fs, head_rev,
trail, pool));
root = make_revision_root(trail->fs, head_rev, root_dir, pool);
err = get_dag(&path_node, root, path, trail, pool);
if (err && (err->apr_err == SVN_ERR_FS_NOT_FOUND)) {
svn_error_clear(err);
*rev = SVN_INVALID_REVNUM;
return SVN_NO_ERROR;
} else if (err)
return err;
SVN_ERR(svn_fs_base__dag_get_revision(&created_rev, path_node,
trail, pool));
*rev = created_rev;
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
SVN_ERR(base_closest_copy(&copy_root, &copy_path, root, path, pool));
if (! copy_root) {
*prev_rev = SVN_INVALID_REVNUM;
*prev_path = NULL;
return SVN_NO_ERROR;
}
SVN_ERR(base_copied_from(&copy_src_rev, &copy_src_path,
copy_root, copy_path, pool));
if (! strcmp(copy_path, path) == 0)
remainder = svn_path_is_child(copy_path, path, pool);
*prev_path = svn_path_join(copy_src_path, remainder, pool);
*prev_rev = copy_src_rev;
return SVN_NO_ERROR;
}
struct id_created_rev_args {
svn_revnum_t revision;
const svn_fs_id_t *id;
const char *path;
};
static svn_error_t *
txn_body_id_created_rev(void *baton, trail_t *trail) {
struct id_created_rev_args *args = baton;
dag_node_t *node;
SVN_ERR(svn_fs_base__dag_get_node(&node, trail->fs, args->id,
trail, trail->pool));
SVN_ERR(svn_fs_base__dag_get_revision(&(args->revision), node,
trail, trail->pool));
return SVN_NO_ERROR;
}
struct get_set_node_origin_args {
const svn_fs_id_t *origin_id;
const char *node_id;
};
static svn_error_t *
txn_body_get_node_origin(void *baton, trail_t *trail) {
struct get_set_node_origin_args *args = baton;
return svn_fs_bdb__get_node_origin(&(args->origin_id), trail->fs,
args->node_id, trail, trail->pool);
}
static svn_error_t *
txn_body_set_node_origin(void *baton, trail_t *trail) {
struct get_set_node_origin_args *args = baton;
return svn_fs_bdb__set_node_origin(trail->fs, args->node_id,
args->origin_id, trail, trail->pool);
}
static svn_error_t *
base_node_origin_rev(svn_revnum_t *revision,
svn_fs_root_t *root,
const char *path,
apr_pool_t *pool) {
svn_fs_t *fs = root->fs;
svn_error_t *err;
struct get_set_node_origin_args args;
const svn_fs_id_t *id, *origin_id;
struct id_created_rev_args icr_args;
SVN_ERR(svn_fs_base__test_required_feature_format
(fs, "node-origins", SVN_FS_BASE__MIN_NODE_ORIGINS_FORMAT));
SVN_ERR(base_node_id(&id, root, path, pool));
args.node_id = svn_fs_base__id_node_id(id);
err = svn_fs_base__retry_txn(root->fs, txn_body_get_node_origin,
&args, pool);
if (! err) {
origin_id = args.origin_id;
} else if (err->apr_err == SVN_ERR_FS_NO_SUCH_NODE_ORIGIN) {
svn_fs_root_t *curroot = root;
apr_pool_t *subpool = svn_pool_create(pool);
svn_stringbuf_t *lastpath =
svn_stringbuf_create(path, pool);
svn_revnum_t lastrev = SVN_INVALID_REVNUM;
const svn_fs_id_t *pred_id;
svn_error_clear(err);
err = SVN_NO_ERROR;
while (1) {
svn_revnum_t currev;
const char *curpath = lastpath->data;
if (SVN_IS_VALID_REVNUM(lastrev))
SVN_ERR(svn_fs_base__revision_root(&curroot, fs,
lastrev, subpool));
SVN_ERR(prev_location(&curpath, &currev, fs, curroot,
curpath, subpool));
if (! curpath)
break;
svn_stringbuf_set(lastpath, curpath);
}
SVN_ERR(base_node_id(&pred_id, curroot, lastpath->data, pool));
while (1) {
struct txn_pred_id_args pid_args;
svn_pool_clear(subpool);
pid_args.id = pred_id;
pid_args.pool = subpool;
SVN_ERR(svn_fs_base__retry_txn(fs, txn_body_pred_id,
&pid_args, subpool));
if (! pid_args.pred_id)
break;
pred_id = pid_args.pred_id;
}
args.origin_id = origin_id = svn_fs_base__id_copy(pred_id, pool);
SVN_ERR(svn_fs_base__retry_txn(root->fs, txn_body_set_node_origin,
&args, subpool));
svn_pool_destroy(subpool);
} else {
return err;
}
icr_args.id = origin_id;
SVN_ERR(svn_fs_base__retry_txn(root->fs, txn_body_id_created_rev,
&icr_args, pool));
*revision = icr_args.revision;
return SVN_NO_ERROR;
}
struct get_mergeinfo_data_and_entries_baton {
svn_mergeinfo_catalog_t result_catalog;
apr_hash_t *children_atop_mergeinfo_trees;
dag_node_t *node;
const char *node_path;
};
static svn_error_t *
txn_body_get_mergeinfo_data_and_entries(void *baton, trail_t *trail) {
struct get_mergeinfo_data_and_entries_baton *args = baton;
dag_node_t *node = args->node;
apr_hash_t *entries;
apr_hash_index_t *hi;
apr_pool_t *iterpool = svn_pool_create(trail->pool);
apr_pool_t *result_pool = apr_hash_pool_get(args->result_catalog);
apr_pool_t *children_pool =
apr_hash_pool_get(args->children_atop_mergeinfo_trees);
assert(svn_fs_base__dag_node_kind(node) == svn_node_dir);
SVN_ERR(svn_fs_base__dag_dir_entries(&entries, node, trail, trail->pool));
for (hi = apr_hash_first(NULL, entries); hi; hi = apr_hash_next(hi)) {
void *val;
svn_fs_dirent_t *dirent;
const svn_fs_id_t *child_id;
dag_node_t *child_node;
svn_boolean_t has_mergeinfo;
apr_int64_t kid_count;
svn_pool_clear(iterpool);
apr_hash_this(hi, NULL, NULL, &val);
dirent = val;
child_id = dirent->id;
SVN_ERR(svn_fs_base__dag_get_node(&child_node, trail->fs, child_id,
trail, iterpool));
SVN_ERR(svn_fs_base__dag_get_mergeinfo_stats(&has_mergeinfo, &kid_count,
child_node, trail,
iterpool));
if (has_mergeinfo) {
apr_hash_t *plist;
svn_mergeinfo_t child_mergeinfo;
svn_string_t *pval;
SVN_ERR(svn_fs_base__dag_get_proplist(&plist, child_node,
trail, iterpool));
pval = apr_hash_get(plist, SVN_PROP_MERGEINFO, APR_HASH_KEY_STRING);
if (! pval) {
svn_string_t *id_str = svn_fs_base__id_unparse(child_id,
iterpool);
return svn_error_createf(SVN_ERR_FS_CORRUPT, NULL,
_("Node-revision '%s' claims to have "
"mergeinfo but doesn't"),
id_str->data);
}
SVN_ERR(svn_mergeinfo_parse(&child_mergeinfo, pval->data,
result_pool));
apr_hash_set(args->result_catalog,
svn_path_join(args->node_path, dirent->name,
result_pool),
APR_HASH_KEY_STRING,
child_mergeinfo);
}
if ((kid_count - (has_mergeinfo ? 1 : 0)) > 0) {
if (svn_fs_base__dag_node_kind(child_node) != svn_node_dir) {
svn_string_t *id_str = svn_fs_base__id_unparse(child_id,
iterpool);
return svn_error_createf(SVN_ERR_FS_CORRUPT, NULL,
_("Node-revision '%s' claims to sit "
"atop a tree containing mergeinfo "
"but is not a directory"),
id_str->data);
}
apr_hash_set(args->children_atop_mergeinfo_trees,
apr_pstrdup(children_pool, dirent->name),
APR_HASH_KEY_STRING,
svn_fs_base__dag_dup(child_node, children_pool));
}
}
svn_pool_destroy(iterpool);
return SVN_NO_ERROR;
}
static svn_error_t *
crawl_directory_for_mergeinfo(svn_fs_t *fs,
dag_node_t *node,
const char *node_path,
svn_mergeinfo_catalog_t result_catalog,
apr_pool_t *pool) {
struct get_mergeinfo_data_and_entries_baton gmdae_args;
apr_hash_t *children_atop_mergeinfo_trees = apr_hash_make(pool);
apr_hash_index_t *hi;
apr_pool_t *iterpool;
gmdae_args.result_catalog = result_catalog;
gmdae_args.children_atop_mergeinfo_trees = children_atop_mergeinfo_trees;
gmdae_args.node = node;
gmdae_args.node_path = node_path;
SVN_ERR(svn_fs_base__retry_txn(fs, txn_body_get_mergeinfo_data_and_entries,
&gmdae_args, pool));
if (apr_hash_count(children_atop_mergeinfo_trees) == 0)
return SVN_NO_ERROR;
iterpool = svn_pool_create(pool);
for (hi = apr_hash_first(NULL, children_atop_mergeinfo_trees);
hi;
hi = apr_hash_next(hi)) {
const void *key;
void *val;
svn_pool_clear(iterpool);
apr_hash_this(hi, &key, NULL, &val);
crawl_directory_for_mergeinfo(fs, val,
svn_path_join(node_path, key, iterpool),
result_catalog, iterpool);
}
svn_pool_destroy(iterpool);
return SVN_NO_ERROR;
}
static svn_error_t *
append_to_merged_froms(svn_mergeinfo_t *output,
svn_mergeinfo_t input,
const char *rel_path,
apr_pool_t *pool) {
apr_hash_index_t *hi;
*output = apr_hash_make(pool);
for (hi = apr_hash_first(pool, input); hi; hi = apr_hash_next(hi)) {
const void *key;
void *val;
apr_hash_this(hi, &key, NULL, &val);
apr_hash_set(*output, svn_path_join(key, rel_path, pool),
APR_HASH_KEY_STRING, svn_rangelist_dup(val, pool));
}
return SVN_NO_ERROR;
}
struct get_mergeinfo_for_path_baton {
svn_mergeinfo_t *mergeinfo;
svn_fs_root_t *root;
const char *path;
svn_mergeinfo_inheritance_t inherit;
apr_pool_t *pool;
};
static svn_error_t *
txn_body_get_mergeinfo_for_path(void *baton, trail_t *trail) {
struct get_mergeinfo_for_path_baton *args = baton;
parent_path_t *parent_path, *nearest_ancestor;
apr_hash_t *proplist;
svn_string_t *mergeinfo_string;
apr_pool_t *iterpool;
dag_node_t *node = NULL;
*(args->mergeinfo) = NULL;
SVN_ERR(open_path(&parent_path, args->root, args->path, 0,
NULL, trail, trail->pool));
nearest_ancestor = parent_path;
if (args->inherit == svn_mergeinfo_nearest_ancestor) {
if (! parent_path->parent)
return SVN_NO_ERROR;
nearest_ancestor = parent_path->parent;
}
iterpool = svn_pool_create(trail->pool);
while (TRUE) {
svn_boolean_t has_mergeinfo;
apr_int64_t count;
svn_pool_clear(iterpool);
node = nearest_ancestor->node;
SVN_ERR(svn_fs_base__dag_get_mergeinfo_stats(&has_mergeinfo, &count,
node, trail, iterpool));
if (has_mergeinfo)
break;
if (args->inherit == svn_mergeinfo_explicit) {
svn_pool_destroy(iterpool);
return SVN_NO_ERROR;
}
nearest_ancestor = nearest_ancestor->parent;
if (! nearest_ancestor) {
svn_pool_destroy(iterpool);
return SVN_NO_ERROR;
}
}
svn_pool_destroy(iterpool);
SVN_ERR(svn_fs_base__dag_get_proplist(&proplist, node, trail, trail->pool));
mergeinfo_string = apr_hash_get(proplist, SVN_PROP_MERGEINFO,
APR_HASH_KEY_STRING);
if (! mergeinfo_string) {
svn_string_t *id_str =
svn_fs_base__id_unparse(svn_fs_base__dag_get_id(node), trail->pool);
return svn_error_createf(SVN_ERR_FS_CORRUPT, NULL,
_("Node-revision '%s' claims to have "
"mergeinfo but doesn't"), id_str->data);
}
if (nearest_ancestor == parent_path) {
SVN_ERR(svn_mergeinfo_parse(args->mergeinfo,
mergeinfo_string->data, args->pool));
} else {
svn_mergeinfo_t tmp_mergeinfo;
SVN_ERR(svn_mergeinfo_parse(&tmp_mergeinfo,
mergeinfo_string->data, trail->pool));
SVN_ERR(svn_mergeinfo_inheritable(&tmp_mergeinfo,
tmp_mergeinfo,
NULL, SVN_INVALID_REVNUM,
SVN_INVALID_REVNUM, trail->pool));
SVN_ERR(append_to_merged_froms(args->mergeinfo,
tmp_mergeinfo,
parent_path_relpath(parent_path,
nearest_ancestor,
trail->pool),
args->pool));
}
return SVN_NO_ERROR;
}
struct get_node_mergeinfo_stats_baton {
dag_node_t *node;
svn_boolean_t has_mergeinfo;
apr_int64_t child_mergeinfo_count;
svn_fs_root_t *root;
const char *path;
};
static svn_error_t *
txn_body_get_node_mergeinfo_stats(void *baton, trail_t *trail) {
struct get_node_mergeinfo_stats_baton *args = baton;
SVN_ERR(get_dag(&(args->node), args->root, args->path,
trail, trail->pool));
SVN_ERR(svn_fs_base__dag_get_mergeinfo_stats(&(args->has_mergeinfo),
&(args->child_mergeinfo_count),
args->node, trail,
trail->pool));
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
struct get_mergeinfo_for_path_baton gmfp_args;
const char *path = APR_ARRAY_IDX(paths, i, const char *);
svn_pool_clear(iterpool);
path = svn_fs__canonicalize_abspath(path, iterpool);
gmfp_args.mergeinfo = &path_mergeinfo;
gmfp_args.root = root;
gmfp_args.path = path;
gmfp_args.inherit = inherit;
gmfp_args.pool = pool;
SVN_ERR(svn_fs_base__retry_txn(root->fs,
txn_body_get_mergeinfo_for_path,
&gmfp_args, iterpool));
if (path_mergeinfo)
apr_hash_set(result_catalog, apr_pstrdup(pool, path),
APR_HASH_KEY_STRING,
path_mergeinfo);
if (include_descendants) {
svn_boolean_t do_crawl;
struct get_node_mergeinfo_stats_baton gnms_args;
gnms_args.root = root;
gnms_args.path = path;
SVN_ERR(svn_fs_base__retry_txn(root->fs,
txn_body_get_node_mergeinfo_stats,
&gnms_args, iterpool));
if (svn_fs_base__dag_node_kind(gnms_args.node) != svn_node_dir)
do_crawl = FALSE;
else
do_crawl = ((gnms_args.child_mergeinfo_count > 1)
|| ((gnms_args.child_mergeinfo_count == 1)
&& (! gnms_args.has_mergeinfo)));
if (do_crawl)
SVN_ERR(crawl_directory_for_mergeinfo(root->fs, gnms_args.node,
path, result_catalog,
iterpool));
}
}
svn_pool_destroy(iterpool);
*mergeinfo_catalog = result_catalog;
return SVN_NO_ERROR;
}
static svn_error_t *
base_get_mergeinfo(svn_mergeinfo_catalog_t *catalog,
svn_fs_root_t *root,
const apr_array_header_t *paths,
svn_mergeinfo_inheritance_t inherit,
svn_boolean_t include_descendants,
apr_pool_t *pool) {
SVN_ERR(svn_fs_base__test_required_feature_format
(root->fs, "mergeinfo", SVN_FS_BASE__MIN_MERGEINFO_FORMAT));
if (root->is_txn_root)
return svn_error_create(SVN_ERR_FS_NOT_REVISION_ROOT, NULL, NULL);
return get_mergeinfos_for_paths(root, catalog, paths,
inherit, include_descendants,
pool);
}
static root_vtable_t root_vtable = {
base_paths_changed,
base_check_path,
base_node_history,
base_node_id,
base_node_created_rev,
base_node_origin_rev,
base_node_created_path,
base_delete_node,
base_copied_from,
base_closest_copy,
base_node_prop,
base_node_proplist,
base_change_node_prop,
base_props_changed,
base_dir_entries,
base_make_dir,
base_copy,
base_revision_link,
base_file_length,
base_file_md5_checksum,
base_file_contents,
base_make_file,
base_apply_textdelta,
base_apply_text,
base_contents_changed,
base_get_file_delta_stream,
base_merge,
base_get_mergeinfo,
};
static svn_fs_root_t *
make_root(svn_fs_t *fs,
apr_pool_t *pool) {
apr_pool_t *subpool = svn_pool_create(pool);
svn_fs_root_t *root = apr_pcalloc(subpool, sizeof(*root));
base_root_data_t *brd = apr_palloc(subpool, sizeof(*brd));
root->fs = fs;
root->pool = subpool;
brd->node_cache = apr_hash_make(pool);
brd->node_cache_idx = 0;
root->vtable = &root_vtable;
root->fsap_data = brd;
return root;
}
static svn_fs_root_t *
make_revision_root(svn_fs_t *fs,
svn_revnum_t rev,
dag_node_t *root_dir,
apr_pool_t *pool) {
svn_fs_root_t *root = make_root(fs, pool);
base_root_data_t *brd = root->fsap_data;
root->is_txn_root = FALSE;
root->rev = rev;
brd->root_dir = root_dir;
return root;
}
static svn_fs_root_t *
make_txn_root(svn_fs_t *fs,
const char *txn,
svn_revnum_t base_rev,
apr_uint32_t flags,
apr_pool_t *pool) {
svn_fs_root_t *root = make_root(fs, pool);
root->is_txn_root = TRUE;
root->txn = apr_pstrdup(root->pool, txn);
root->txn_flags = flags;
root->rev = base_rev;
return root;
}
