#if !defined(SVN_LIBSVN_FS_DAG_H)
#define SVN_LIBSVN_FS_DAG_H
#include "svn_fs.h"
#include "trail.h"
#if defined(__cplusplus)
extern "C" {
#endif
svn_error_t *svn_fs_base__dag_init_fs(svn_fs_t *fs);
typedef struct dag_node_t dag_node_t;
svn_error_t *svn_fs_base__dag_get_node(dag_node_t **node,
svn_fs_t *fs,
const svn_fs_id_t *id,
trail_t *trail,
apr_pool_t *pool);
dag_node_t *svn_fs_base__dag_dup(dag_node_t *node,
apr_pool_t *pool);
svn_fs_t *svn_fs_base__dag_get_fs(dag_node_t *node);
svn_error_t *svn_fs_base__dag_get_revision(svn_revnum_t *rev,
dag_node_t *node,
trail_t *trail,
apr_pool_t *pool);
const svn_fs_id_t *svn_fs_base__dag_get_id(dag_node_t *node);
const char *svn_fs_base__dag_get_created_path(dag_node_t *node);
svn_error_t *svn_fs_base__dag_get_predecessor_id(const svn_fs_id_t **id_p,
dag_node_t *node,
trail_t *trail,
apr_pool_t *pool);
svn_error_t *svn_fs_base__dag_get_predecessor_count(int *count,
dag_node_t *node,
trail_t *trail,
apr_pool_t *pool);
svn_boolean_t svn_fs_base__dag_check_mutable(dag_node_t *node,
const char *txn_id);
svn_node_kind_t svn_fs_base__dag_node_kind(dag_node_t *node);
svn_error_t *svn_fs_base__dag_get_proplist(apr_hash_t **proplist_p,
dag_node_t *node,
trail_t *trail,
apr_pool_t *pool);
svn_error_t *svn_fs_base__dag_set_proplist(dag_node_t *node,
apr_hash_t *proplist,
const char *txn_id,
trail_t *trail,
apr_pool_t *pool);
svn_error_t *svn_fs_base__dag_get_mergeinfo_stats(svn_boolean_t *has_mergeinfo,
apr_int64_t *count,
dag_node_t *node,
trail_t *trail,
apr_pool_t *pool);
svn_error_t *svn_fs_base__dag_set_has_mergeinfo(dag_node_t *node,
svn_boolean_t has_mergeinfo,
svn_boolean_t *had_mergeinfo,
const char *txn_id,
trail_t *trail,
apr_pool_t *pool);
svn_error_t *svn_fs_base__dag_adjust_mergeinfo_count(dag_node_t *node,
apr_int64_t count_delta,
const char *txn_id,
trail_t *trail,
apr_pool_t *pool);
svn_error_t *svn_fs_base__dag_revision_root(dag_node_t **node_p,
svn_fs_t *fs,
svn_revnum_t rev,
trail_t *trail,
apr_pool_t *pool);
svn_error_t *svn_fs_base__dag_txn_root(dag_node_t **node_p,
svn_fs_t *fs,
const char *txn_id,
trail_t *trail,
apr_pool_t *pool);
svn_error_t *svn_fs_base__dag_txn_base_root(dag_node_t **node_p,
svn_fs_t *fs,
const char *txn_id,
trail_t *trail,
apr_pool_t *pool);
svn_error_t *svn_fs_base__dag_clone_root(dag_node_t **root_p,
svn_fs_t *fs,
const char *txn_id,
trail_t *trail,
apr_pool_t *pool);
svn_error_t *svn_fs_base__dag_commit_txn(svn_revnum_t *new_rev,
svn_fs_txn_t *txn,
trail_t *trail,
apr_pool_t *pool);
svn_error_t *svn_fs_base__dag_open(dag_node_t **child_p,
dag_node_t *parent,
const char *name,
trail_t *trail,
apr_pool_t *pool);
svn_error_t *svn_fs_base__dag_dir_entries(apr_hash_t **entries_p,
dag_node_t *node,
trail_t *trail,
apr_pool_t *pool);
svn_error_t *svn_fs_base__dag_set_entry(dag_node_t *node,
const char *entry_name,
const svn_fs_id_t *id,
const char *txn_id,
trail_t *trail,
apr_pool_t *pool);
svn_error_t *svn_fs_base__dag_clone_child(dag_node_t **child_p,
dag_node_t *parent,
const char *parent_path,
const char *name,
const char *copy_id,
const char *txn_id,
trail_t *trail,
apr_pool_t *pool);
svn_error_t *svn_fs_base__dag_delete(dag_node_t *parent,
const char *name,
const char *txn_id,
trail_t *trail,
apr_pool_t *pool);
svn_error_t *svn_fs_base__dag_remove_node(svn_fs_t *fs,
const svn_fs_id_t *id,
const char *txn_id,
trail_t *trail,
apr_pool_t *pool);
svn_error_t *svn_fs_base__dag_delete_if_mutable(svn_fs_t *fs,
const svn_fs_id_t *id,
const char *txn_id,
trail_t *trail,
apr_pool_t *pool);
svn_error_t *svn_fs_base__dag_make_dir(dag_node_t **child_p,
dag_node_t *parent,
const char *parent_path,
const char *name,
const char *txn_id,
trail_t *trail,
apr_pool_t *pool);
svn_error_t *svn_fs_base__dag_get_contents(svn_stream_t **contents,
dag_node_t *file,
trail_t *trail,
apr_pool_t *pool);
svn_error_t *svn_fs_base__dag_get_edit_stream(svn_stream_t **contents,
dag_node_t *file,
const char *txn_id,
trail_t *trail,
apr_pool_t *pool);
svn_error_t *svn_fs_base__dag_finalize_edits(dag_node_t *file,
const char *checksum,
const char *txn_id,
trail_t *trail,
apr_pool_t *pool);
svn_error_t *svn_fs_base__dag_file_length(svn_filesize_t *length,
dag_node_t *file,
trail_t *trail,
apr_pool_t *pool);
svn_error_t *
svn_fs_base__dag_file_checksum(unsigned char digest[],
dag_node_t *file,
trail_t *trail,
apr_pool_t *pool);
svn_error_t *svn_fs_base__dag_make_file(dag_node_t **child_p,
dag_node_t *parent,
const char *parent_path,
const char *name,
const char *txn_id,
trail_t *trail,
apr_pool_t *pool);
svn_error_t *svn_fs_base__dag_copy(dag_node_t *to_node,
const char *entry,
dag_node_t *from_node,
svn_boolean_t preserve_history,
svn_revnum_t from_rev,
const char *from_path,
const char *txn_id,
trail_t *trail,
apr_pool_t *pool);
svn_error_t *svn_fs_base__dag_deltify(dag_node_t *target,
dag_node_t *source,
svn_boolean_t props_only,
trail_t *trail,
apr_pool_t *pool);
svn_error_t *svn_fs_base__things_different(svn_boolean_t *props_changed,
svn_boolean_t *contents_changed,
dag_node_t *node1,
dag_node_t *node2,
trail_t *trail,
apr_pool_t *pool);
#if defined(__cplusplus)
}
#endif
#endif
