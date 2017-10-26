#if !defined(SVN_LIBSVN_FS__FS_FS_H)
#define SVN_LIBSVN_FS__FS_FS_H
#include "fs.h"
svn_error_t *svn_fs_fs__open(svn_fs_t *fs,
const char *path,
apr_pool_t *pool);
svn_error_t *svn_fs_fs__upgrade(svn_fs_t *fs,
apr_pool_t *pool);
svn_error_t *svn_fs_fs__hotcopy(const char *src_path,
const char *dst_path,
apr_pool_t *pool);
svn_error_t *svn_fs_fs__recover(svn_fs_t *fs,
svn_cancel_func_t cancel_func,
void *cancel_baton,
apr_pool_t *pool);
svn_error_t *svn_fs_fs__get_node_revision(node_revision_t **noderev_p,
svn_fs_t *fs,
const svn_fs_id_t *id,
apr_pool_t *pool);
svn_error_t *svn_fs_fs__put_node_revision(svn_fs_t *fs,
const svn_fs_id_t *id,
node_revision_t *noderev,
svn_boolean_t fresh_txn_root,
apr_pool_t *pool);
svn_error_t *svn_fs_fs__youngest_rev(svn_revnum_t *youngest,
svn_fs_t *fs,
apr_pool_t *pool);
svn_error_t *svn_fs_fs__rev_get_root(svn_fs_id_t **root_id,
svn_fs_t *fs,
svn_revnum_t rev,
apr_pool_t *pool);
svn_error_t *svn_fs_fs__rep_contents_dir(apr_hash_t **entries,
svn_fs_t *fs,
node_revision_t *noderev,
apr_pool_t *pool);
svn_error_t *svn_fs_fs__get_contents(svn_stream_t **contents,
svn_fs_t *fs,
node_revision_t *noderev,
apr_pool_t *pool);
svn_error_t *svn_fs_fs__get_file_delta_stream(svn_txdelta_stream_t **stream_p,
svn_fs_t *fs,
node_revision_t *source,
node_revision_t *target,
apr_pool_t *pool);
svn_error_t *svn_fs_fs__get_proplist(apr_hash_t **proplist,
svn_fs_t *fs,
node_revision_t *noderev,
apr_pool_t *pool);
svn_error_t *svn_fs_fs__set_revision_proplist(svn_fs_t *fs,
svn_revnum_t rev,
apr_hash_t *proplist,
apr_pool_t *pool);
svn_error_t *svn_fs_fs__revision_proplist(apr_hash_t **proplist,
svn_fs_t *fs,
svn_revnum_t rev,
apr_pool_t *pool);
svn_error_t *svn_fs_fs__file_length(svn_filesize_t *length,
node_revision_t *noderev,
apr_pool_t *pool);
svn_boolean_t svn_fs_fs__noderev_same_rep_key(representation_t *a,
representation_t *b);
representation_t *svn_fs_fs__rep_copy(representation_t *rep,
apr_pool_t *pool);
svn_error_t *svn_fs_fs__file_checksum(unsigned char digest[],
node_revision_t *noderev,
apr_pool_t *pool);
svn_error_t *svn_fs_fs__paths_changed(apr_hash_t **changed_paths_p,
svn_fs_t *fs,
svn_revnum_t rev,
apr_hash_t *copyfrom_cache,
apr_pool_t *pool);
svn_error_t *svn_fs_fs__create_txn(svn_fs_txn_t **txn_p,
svn_fs_t *fs,
svn_revnum_t rev,
apr_pool_t *pool);
svn_error_t *svn_fs_fs__change_txn_prop(svn_fs_txn_t *txn,
const char *name,
const svn_string_t *value,
apr_pool_t *pool);
svn_error_t *svn_fs_fs__change_txn_props(svn_fs_txn_t *txn,
apr_array_header_t *props,
apr_pool_t *pool);
svn_boolean_t svn_fs_fs__fs_supports_mergeinfo(svn_fs_t *fs);
svn_error_t *svn_fs_fs__get_txn(transaction_t **txn_p,
svn_fs_t *fs,
const char *txn_id,
apr_pool_t *pool);
svn_error_t *svn_fs_fs__abort_txn(svn_fs_txn_t *txn, apr_pool_t *pool);
svn_error_t *svn_fs_fs__create_node(const svn_fs_id_t **id_p,
svn_fs_t *fs,
node_revision_t *noderev,
const char *copy_id,
const char *txn_id,
apr_pool_t *pool);
svn_error_t *svn_fs_fs__purge_txn(svn_fs_t *fs,
const char *txn_id,
apr_pool_t *pool);
svn_error_t *svn_fs_fs__set_entry(svn_fs_t *fs,
const char *txn_id,
node_revision_t *parent_noderev,
const char *name,
const svn_fs_id_t *id,
svn_node_kind_t kind,
apr_pool_t *pool);
svn_error_t *svn_fs_fs__add_change(svn_fs_t *fs,
const char *txn_id,
const char *path,
const svn_fs_id_t *id,
svn_fs_path_change_kind_t change_kind,
svn_boolean_t text_mod,
svn_boolean_t prop_mod,
svn_revnum_t copyfrom_rev,
const char *copyfrom_path,
apr_pool_t *pool);
svn_error_t *svn_fs_fs__set_contents(svn_stream_t **stream,
svn_fs_t *fs,
node_revision_t *noderev,
apr_pool_t *pool);
svn_error_t *svn_fs_fs__create_successor(const svn_fs_id_t **new_id_p,
svn_fs_t *fs,
const svn_fs_id_t *old_idp,
node_revision_t *new_noderev,
const char *copy_id,
const char *txn_id,
apr_pool_t *pool);
svn_error_t *svn_fs_fs__set_proplist(svn_fs_t *fs,
node_revision_t *noderev,
apr_hash_t *proplist,
apr_pool_t *pool);
svn_error_t *svn_fs_fs__commit(svn_revnum_t *new_rev_p,
svn_fs_t *fs,
svn_fs_txn_t *txn,
apr_pool_t *pool);
svn_error_t *svn_fs_fs__reserve_copy_id(const char **copy_id,
svn_fs_t *fs,
const char *txn_id,
apr_pool_t *pool);
svn_error_t *svn_fs_fs__create(svn_fs_t *fs,
const char *path,
apr_pool_t *pool);
svn_error_t *svn_fs_fs__get_uuid(svn_fs_t *fs,
const char **uuid,
apr_pool_t *pool);
svn_error_t *svn_fs_fs__set_uuid(svn_fs_t *fs,
const char *uuid,
apr_pool_t *pool);
svn_error_t *svn_fs_fs__list_transactions(apr_array_header_t **names_p,
svn_fs_t *fs,
apr_pool_t *pool);
svn_error_t *svn_fs_fs__open_txn(svn_fs_txn_t **txn_p,
svn_fs_t *fs,
const char *name,
apr_pool_t *pool);
svn_error_t *svn_fs_fs__txn_proplist(apr_hash_t **proplist,
svn_fs_txn_t *txn,
apr_pool_t *pool);
svn_error_t *svn_fs_fs__delete_node_revision(svn_fs_t *fs,
const svn_fs_id_t *id,
apr_pool_t *pool);
svn_error_t *svn_fs_fs__txn_changes_fetch(apr_hash_t **changes,
svn_fs_t *fs,
const char *txn_id,
apr_hash_t *copyfrom_cache,
apr_pool_t *pool);
svn_error_t *svn_fs_fs__move_into_place(const char *old_filename,
const char *new_filename,
const char *perms_reference,
apr_pool_t *pool);
svn_error_t *svn_fs_fs__dup_perms(const char *filename,
const char *perms_reference,
apr_pool_t *pool);
const char *svn_fs_fs__path_rev(svn_fs_t *fs,
svn_revnum_t rev,
apr_pool_t *pool);
const char *
svn_fs_fs__path_current(svn_fs_t *fs, apr_pool_t *pool);
svn_error_t *
svn_fs_fs__with_write_lock(svn_fs_t *fs,
svn_error_t *(*body)(void *baton,
apr_pool_t *pool),
void *baton,
apr_pool_t *pool);
svn_error_t *svn_fs_fs__revision_prop(svn_string_t **value_p, svn_fs_t *fs,
svn_revnum_t rev,
const char *propname,
apr_pool_t *pool);
svn_error_t *svn_fs_fs__change_rev_prop(svn_fs_t *fs, svn_revnum_t rev,
const char *name,
const svn_string_t *value,
apr_pool_t *pool);
svn_error_t *svn_fs_fs__get_txn_ids(const svn_fs_id_t **root_id_p,
const svn_fs_id_t **base_root_id_p,
svn_fs_t *fs,
const char *txn_name,
apr_pool_t *pool);
svn_error_t *svn_fs_fs__begin_txn(svn_fs_txn_t **txn_p, svn_fs_t *fs,
svn_revnum_t rev, apr_uint32_t flags,
apr_pool_t *pool);
svn_error_t *svn_fs_fs__txn_prop(svn_string_t **value_p, svn_fs_txn_t *txn,
const char *propname, apr_pool_t *pool);
svn_error_t *svn_fs_fs__ensure_dir_exists(const char *path,
svn_fs_t *fs,
apr_pool_t *pool);
svn_error_t *
svn_fs_fs__set_node_origin(svn_fs_t *fs,
const char *node_id,
const svn_fs_id_t *node_rev_id,
apr_pool_t *pool);
svn_error_t *
svn_fs_fs__get_node_origin(const svn_fs_id_t **origin_id,
svn_fs_t *fs,
const char *node_id,
apr_pool_t *pool);
#endif
