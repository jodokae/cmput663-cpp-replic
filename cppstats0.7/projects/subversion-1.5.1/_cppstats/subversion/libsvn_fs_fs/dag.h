#if !defined(SVN_LIBSVN_FS_DAG_H)
#define SVN_LIBSVN_FS_DAG_H
#include "svn_fs.h"
#include "svn_delta.h"
#include "fs.h"
#if defined(__cplusplus)
extern "C" {
#endif
svn_error_t *
svn_fs_fs__dag_get_node(dag_node_t **node,
svn_fs_t *fs,
const svn_fs_id_t *id,
apr_pool_t *pool);
dag_node_t *svn_fs_fs__dag_dup(dag_node_t *node,
apr_pool_t *pool);
svn_fs_t *svn_fs_fs__dag_get_fs(dag_node_t *node);
svn_error_t *svn_fs_fs__dag_get_revision(svn_revnum_t *rev,
dag_node_t *node,
apr_pool_t *pool);
const svn_fs_id_t *svn_fs_fs__dag_get_id(dag_node_t *node);
const char *svn_fs_fs__dag_get_created_path(dag_node_t *node);
svn_error_t *svn_fs_fs__dag_get_predecessor_id(const svn_fs_id_t **id_p,
dag_node_t *node,
apr_pool_t *pool);
svn_error_t *svn_fs_fs__dag_get_predecessor_count(int *count,
dag_node_t *node,
apr_pool_t *pool);
svn_error_t *svn_fs_fs__dag_get_mergeinfo_count(apr_int64_t *count,
dag_node_t *node,
apr_pool_t *pool);
svn_error_t *
svn_fs_fs__dag_has_descendants_with_mergeinfo(svn_boolean_t *do_they,
dag_node_t *node,
apr_pool_t *pool);
svn_error_t *
svn_fs_fs__dag_has_mergeinfo(svn_boolean_t *has_mergeinfo,
dag_node_t *node,
apr_pool_t *pool);
svn_boolean_t svn_fs_fs__dag_check_mutable(dag_node_t *node);
svn_node_kind_t svn_fs_fs__dag_node_kind(dag_node_t *node);
svn_error_t *svn_fs_fs__dag_get_proplist(apr_hash_t **proplist_p,
dag_node_t *node,
apr_pool_t *pool);
svn_error_t *svn_fs_fs__dag_set_proplist(dag_node_t *node,
apr_hash_t *proplist,
apr_pool_t *pool);
svn_error_t *svn_fs_fs__dag_increment_mergeinfo_count(dag_node_t *node,
apr_int64_t increment,
apr_pool_t *pool);
svn_error_t *svn_fs_fs__dag_set_has_mergeinfo(dag_node_t *node,
svn_boolean_t has_mergeinfo,
apr_pool_t *pool);
svn_error_t *svn_fs_fs__dag_revision_root(dag_node_t **node_p,
svn_fs_t *fs,
svn_revnum_t rev,
apr_pool_t *pool);
svn_error_t *svn_fs_fs__dag_txn_root(dag_node_t **node_p,
svn_fs_t *fs,
const char *txn_id,
apr_pool_t *pool);
svn_error_t *svn_fs_fs__dag_txn_base_root(dag_node_t **node_p,
svn_fs_t *fs,
const char *txn_id,
apr_pool_t *pool);
svn_error_t *svn_fs_fs__dag_clone_root(dag_node_t **root_p,
svn_fs_t *fs,
const char *txn_id,
apr_pool_t *pool);
svn_error_t *svn_fs_fs__dag_open(dag_node_t **child_p,
dag_node_t *parent,
const char *name,
apr_pool_t *pool);
svn_error_t *svn_fs_fs__dag_dir_entries(apr_hash_t **entries_p,
dag_node_t *node,
apr_pool_t *pool,
apr_pool_t *node_pool);
svn_error_t *svn_fs_fs__dag_set_entry(dag_node_t *node,
const char *entry_name,
const svn_fs_id_t *id,
svn_node_kind_t kind,
const char *txn_id,
apr_pool_t *pool);
svn_error_t *svn_fs_fs__dag_clone_child(dag_node_t **child_p,
dag_node_t *parent,
const char *parent_path,
const char *name,
const char *copy_id,
const char *txn_id,
svn_boolean_t is_parent_copyroot,
apr_pool_t *pool);
svn_error_t *svn_fs_fs__dag_delete(dag_node_t *parent,
const char *name,
const char *txn_id,
apr_pool_t *pool);
svn_error_t *svn_fs_fs__dag_remove_node(svn_fs_t *fs,
const svn_fs_id_t *id,
apr_pool_t *pool);
svn_error_t *svn_fs_fs__dag_delete_if_mutable(svn_fs_t *fs,
const svn_fs_id_t *id,
apr_pool_t *pool);
svn_error_t *svn_fs_fs__dag_make_dir(dag_node_t **child_p,
dag_node_t *parent,
const char *parent_path,
const char *name,
const char *txn_id,
apr_pool_t *pool);
svn_error_t *svn_fs_fs__dag_get_contents(svn_stream_t **contents,
dag_node_t *file,
apr_pool_t *pool);
svn_error_t *
svn_fs_fs__dag_get_file_delta_stream(svn_txdelta_stream_t **stream_p,
dag_node_t *source,
dag_node_t *target,
apr_pool_t *pool);
svn_error_t *svn_fs_fs__dag_get_edit_stream(svn_stream_t **contents,
dag_node_t *file,
apr_pool_t *pool);
svn_error_t *svn_fs_fs__dag_finalize_edits(dag_node_t *file,
const char *checksum,
apr_pool_t *pool);
svn_error_t *svn_fs_fs__dag_file_length(svn_filesize_t *length,
dag_node_t *file,
apr_pool_t *pool);
svn_error_t *
svn_fs_fs__dag_file_checksum(unsigned char digest[],
dag_node_t *file,
apr_pool_t *pool);
svn_error_t *svn_fs_fs__dag_make_file(dag_node_t **child_p,
dag_node_t *parent,
const char *parent_path,
const char *name,
const char *txn_id,
apr_pool_t *pool);
svn_error_t *svn_fs_fs__dag_copy(dag_node_t *to_node,
const char *entry,
dag_node_t *from_node,
svn_boolean_t preserve_history,
svn_revnum_t from_rev,
const char *from_path,
const char *txn_id,
apr_pool_t *pool);
svn_error_t *svn_fs_fs__dag_things_different(svn_boolean_t *props_changed,
svn_boolean_t *contents_changed,
dag_node_t *node1,
dag_node_t *node2,
apr_pool_t *pool);
svn_error_t *svn_fs_fs__dag_get_copyroot(svn_revnum_t *rev,
const char **path,
dag_node_t *node,
apr_pool_t *pool);
svn_error_t *svn_fs_fs__dag_get_copyfrom_rev(svn_revnum_t *rev,
dag_node_t *node,
apr_pool_t *pool);
svn_error_t *svn_fs_fs__dag_get_copyfrom_path(const char **path,
dag_node_t *node,
apr_pool_t *pool);
#if defined(__cplusplus)
}
#endif
#endif
