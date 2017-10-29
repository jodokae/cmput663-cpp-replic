#if !defined(SVN_FS_H)
#define SVN_FS_H
#include <apr_pools.h>
#include <apr_hash.h>
#include <apr_tables.h>
#include "svn_types.h"
#include "svn_error.h"
#include "svn_delta.h"
#include "svn_io.h"
#include "svn_mergeinfo.h"
#if defined(__cplusplus)
extern "C" {
#endif
const svn_version_t *svn_fs_version(void);
typedef struct svn_fs_t svn_fs_t;
#define SVN_FS_CONFIG_BDB_TXN_NOSYNC "bdb-txn-nosync"
#define SVN_FS_CONFIG_BDB_LOG_AUTOREMOVE "bdb-log-autoremove"
#define SVN_FS_CONFIG_FS_TYPE "fs-type"
#define SVN_FS_TYPE_BDB "bdb"
#define SVN_FS_TYPE_FSFS "fsfs"
#define SVN_FS_CONFIG_PRE_1_4_COMPATIBLE "pre-1.4-compatible"
#define SVN_FS_CONFIG_PRE_1_5_COMPATIBLE "pre-1.5-compatible"
svn_error_t *svn_fs_initialize(apr_pool_t *pool);
typedef void (*svn_fs_warning_callback_t)(void *baton, svn_error_t *err);
void
svn_fs_set_warning_func(svn_fs_t *fs,
svn_fs_warning_callback_t warning,
void *warning_baton);
svn_error_t *
svn_fs_create(svn_fs_t **fs_p,
const char *path,
apr_hash_t *fs_config,
apr_pool_t *pool);
svn_error_t *
svn_fs_open(svn_fs_t **fs_p,
const char *path,
apr_hash_t *fs_config,
apr_pool_t *pool);
svn_error_t *
svn_fs_upgrade(const char *path,
apr_pool_t *pool);
svn_error_t *
svn_fs_type(const char **fs_type,
const char *path,
apr_pool_t *pool);
const char *svn_fs_path(svn_fs_t *fs, apr_pool_t *pool);
svn_error_t *svn_fs_delete_fs(const char *path, apr_pool_t *pool);
svn_error_t *
svn_fs_hotcopy(const char *src_path,
const char *dest_path,
svn_boolean_t clean,
apr_pool_t *pool);
svn_error_t *
svn_fs_recover(const char *path,
svn_cancel_func_t cancel_func,
void *cancel_baton,
apr_pool_t *pool);
svn_error_t *
svn_fs_set_berkeley_errcall(svn_fs_t *fs,
void (*handler)(const char *errpfx,
char *msg));
svn_error_t *
svn_fs_berkeley_logfiles(apr_array_header_t **logfiles,
const char *path,
svn_boolean_t only_unused,
apr_pool_t *pool);
svn_fs_t *svn_fs_new(apr_hash_t *fs_config, apr_pool_t *pool);
svn_error_t *svn_fs_create_berkeley(svn_fs_t *fs, const char *path);
svn_error_t *svn_fs_open_berkeley(svn_fs_t *fs, const char *path);
const char *svn_fs_berkeley_path(svn_fs_t *fs, apr_pool_t *pool);
svn_error_t *svn_fs_delete_berkeley(const char *path, apr_pool_t *pool);
svn_error_t *
svn_fs_hotcopy_berkeley(const char *src_path,
const char *dest_path,
svn_boolean_t clean_logs,
apr_pool_t *pool);
svn_error_t *
svn_fs_berkeley_recover(const char *path,
apr_pool_t *pool);
typedef struct svn_fs_access_t svn_fs_access_t;
svn_error_t *
svn_fs_create_access(svn_fs_access_t **access_ctx,
const char *username,
apr_pool_t *pool);
svn_error_t *
svn_fs_set_access(svn_fs_t *fs,
svn_fs_access_t *access_ctx);
svn_error_t *
svn_fs_get_access(svn_fs_access_t **access_ctx,
svn_fs_t *fs);
svn_error_t *
svn_fs_access_get_username(const char **username,
svn_fs_access_t *access_ctx);
svn_error_t *
svn_fs_access_add_lock_token(svn_fs_access_t *access_ctx,
const char *token);
typedef struct svn_fs_id_t svn_fs_id_t;
int svn_fs_compare_ids(const svn_fs_id_t *a, const svn_fs_id_t *b);
svn_boolean_t
svn_fs_check_related(const svn_fs_id_t *id1,
const svn_fs_id_t *id2);
svn_fs_id_t *
svn_fs_parse_id(const char *data,
apr_size_t len,
apr_pool_t *pool);
svn_string_t *
svn_fs_unparse_id(const svn_fs_id_t *id,
apr_pool_t *pool);
typedef struct svn_fs_txn_t svn_fs_txn_t;
#define SVN_FS_TXN_CHECK_OOD 0x00001
#define SVN_FS_TXN_CHECK_LOCKS 0x00002
svn_error_t *
svn_fs_begin_txn2(svn_fs_txn_t **txn_p,
svn_fs_t *fs,
svn_revnum_t rev,
apr_uint32_t flags,
apr_pool_t *pool);
svn_error_t *
svn_fs_begin_txn(svn_fs_txn_t **txn_p,
svn_fs_t *fs,
svn_revnum_t rev,
apr_pool_t *pool);
svn_error_t *
svn_fs_commit_txn(const char **conflict_p,
svn_revnum_t *new_rev,
svn_fs_txn_t *txn,
apr_pool_t *pool);
svn_error_t *
svn_fs_abort_txn(svn_fs_txn_t *txn,
apr_pool_t *pool);
svn_error_t *
svn_fs_purge_txn(svn_fs_t *fs,
const char *txn_id,
apr_pool_t *pool);
svn_error_t *
svn_fs_txn_name(const char **name_p,
svn_fs_txn_t *txn,
apr_pool_t *pool);
svn_revnum_t svn_fs_txn_base_revision(svn_fs_txn_t *txn);
svn_error_t *
svn_fs_open_txn(svn_fs_txn_t **txn,
svn_fs_t *fs,
const char *name,
apr_pool_t *pool);
svn_error_t *
svn_fs_list_transactions(apr_array_header_t **names_p,
svn_fs_t *fs,
apr_pool_t *pool);
svn_error_t *
svn_fs_txn_prop(svn_string_t **value_p,
svn_fs_txn_t *txn,
const char *propname,
apr_pool_t *pool);
svn_error_t *
svn_fs_txn_proplist(apr_hash_t **table_p,
svn_fs_txn_t *txn,
apr_pool_t *pool);
svn_error_t *
svn_fs_change_txn_prop(svn_fs_txn_t *txn,
const char *name,
const svn_string_t *value,
apr_pool_t *pool);
svn_error_t *
svn_fs_change_txn_props(svn_fs_txn_t *txn,
apr_array_header_t *props,
apr_pool_t *pool);
typedef struct svn_fs_root_t svn_fs_root_t;
svn_error_t *
svn_fs_revision_root(svn_fs_root_t **root_p,
svn_fs_t *fs,
svn_revnum_t rev,
apr_pool_t *pool);
svn_error_t *
svn_fs_txn_root(svn_fs_root_t **root_p,
svn_fs_txn_t *txn,
apr_pool_t *pool);
void svn_fs_close_root(svn_fs_root_t *root);
svn_fs_t *svn_fs_root_fs(svn_fs_root_t *root);
svn_boolean_t svn_fs_is_txn_root(svn_fs_root_t *root);
svn_boolean_t svn_fs_is_revision_root(svn_fs_root_t *root);
const char *
svn_fs_txn_root_name(svn_fs_root_t *root,
apr_pool_t *pool);
svn_revnum_t svn_fs_txn_root_base_revision(svn_fs_root_t *root);
svn_revnum_t svn_fs_revision_root_revision(svn_fs_root_t *root);
typedef enum {
svn_fs_path_change_modify = 0,
svn_fs_path_change_add,
svn_fs_path_change_delete,
svn_fs_path_change_replace,
svn_fs_path_change_reset
} svn_fs_path_change_kind_t;
typedef struct svn_fs_path_change_t {
const svn_fs_id_t *node_rev_id;
svn_fs_path_change_kind_t change_kind;
svn_boolean_t text_mod;
svn_boolean_t prop_mod;
} svn_fs_path_change_t;
svn_error_t *
svn_fs_paths_changed(apr_hash_t **changed_paths_p,
svn_fs_root_t *root,
apr_pool_t *pool);
svn_error_t *
svn_fs_check_path(svn_node_kind_t *kind_p,
svn_fs_root_t *root,
const char *path,
apr_pool_t *pool);
typedef struct svn_fs_history_t svn_fs_history_t;
svn_error_t *
svn_fs_node_history(svn_fs_history_t **history_p,
svn_fs_root_t *root,
const char *path,
apr_pool_t *pool);
svn_error_t *
svn_fs_history_prev(svn_fs_history_t **prev_history_p,
svn_fs_history_t *history,
svn_boolean_t cross_copies,
apr_pool_t *pool);
svn_error_t *
svn_fs_history_location(const char **path,
svn_revnum_t *revision,
svn_fs_history_t *history,
apr_pool_t *pool);
svn_error_t *
svn_fs_is_dir(svn_boolean_t *is_dir,
svn_fs_root_t *root,
const char *path,
apr_pool_t *pool);
svn_error_t *
svn_fs_is_file(svn_boolean_t *is_file,
svn_fs_root_t *root,
const char *path,
apr_pool_t *pool);
svn_error_t *
svn_fs_node_id(const svn_fs_id_t **id_p,
svn_fs_root_t *root,
const char *path,
apr_pool_t *pool);
svn_error_t *
svn_fs_node_created_rev(svn_revnum_t *revision,
svn_fs_root_t *root,
const char *path,
apr_pool_t *pool);
svn_error_t *
svn_fs_node_origin_rev(svn_revnum_t *revision,
svn_fs_root_t *root,
const char *path,
apr_pool_t *pool);
svn_error_t *
svn_fs_node_created_path(const char **created_path,
svn_fs_root_t *root,
const char *path,
apr_pool_t *pool);
svn_error_t *
svn_fs_node_prop(svn_string_t **value_p,
svn_fs_root_t *root,
const char *path,
const char *propname,
apr_pool_t *pool);
svn_error_t *
svn_fs_node_proplist(apr_hash_t **table_p,
svn_fs_root_t *root,
const char *path,
apr_pool_t *pool);
svn_error_t *
svn_fs_change_node_prop(svn_fs_root_t *root,
const char *path,
const char *name,
const svn_string_t *value,
apr_pool_t *pool);
svn_error_t *
svn_fs_props_changed(svn_boolean_t *changed_p,
svn_fs_root_t *root1,
const char *path1,
svn_fs_root_t *root2,
const char *path2,
apr_pool_t *pool);
svn_error_t *
svn_fs_copied_from(svn_revnum_t *rev_p,
const char **path_p,
svn_fs_root_t *root,
const char *path,
apr_pool_t *pool);
svn_error_t *
svn_fs_closest_copy(svn_fs_root_t **root_p,
const char **path_p,
svn_fs_root_t *root,
const char *path,
apr_pool_t *pool);
svn_error_t *
svn_fs_get_mergeinfo(svn_mergeinfo_catalog_t *catalog,
svn_fs_root_t *root,
const apr_array_header_t *paths,
svn_mergeinfo_inheritance_t inherit,
svn_boolean_t include_descendants,
apr_pool_t *pool);
svn_error_t *
svn_fs_merge(const char **conflict_p,
svn_fs_root_t *source_root,
const char *source_path,
svn_fs_root_t *target_root,
const char *target_path,
svn_fs_root_t *ancestor_root,
const char *ancestor_path,
apr_pool_t *pool);
typedef struct svn_fs_dirent_t {
const char *name;
const svn_fs_id_t *id;
svn_node_kind_t kind;
} svn_fs_dirent_t;
svn_error_t *
svn_fs_dir_entries(apr_hash_t **entries_p,
svn_fs_root_t *root,
const char *path,
apr_pool_t *pool);
svn_error_t *
svn_fs_make_dir(svn_fs_root_t *root,
const char *path,
apr_pool_t *pool);
svn_error_t *
svn_fs_delete(svn_fs_root_t *root,
const char *path,
apr_pool_t *pool);
svn_error_t *
svn_fs_copy(svn_fs_root_t *from_root,
const char *from_path,
svn_fs_root_t *to_root,
const char *to_path,
apr_pool_t *pool);
svn_error_t *
svn_fs_revision_link(svn_fs_root_t *from_root,
svn_fs_root_t *to_root,
const char *path,
apr_pool_t *pool);
svn_error_t *
svn_fs_file_length(svn_filesize_t *length_p,
svn_fs_root_t *root,
const char *path,
apr_pool_t *pool);
svn_error_t *
svn_fs_file_md5_checksum(unsigned char digest[],
svn_fs_root_t *root,
const char *path,
apr_pool_t *pool);
svn_error_t *
svn_fs_file_contents(svn_stream_t **contents,
svn_fs_root_t *root,
const char *path,
apr_pool_t *pool);
svn_error_t *
svn_fs_make_file(svn_fs_root_t *root,
const char *path,
apr_pool_t *pool);
svn_error_t *
svn_fs_apply_textdelta(svn_txdelta_window_handler_t *contents_p,
void **contents_baton_p,
svn_fs_root_t *root,
const char *path,
const char *base_checksum,
const char *result_checksum,
apr_pool_t *pool);
svn_error_t *
svn_fs_apply_text(svn_stream_t **contents_p,
svn_fs_root_t *root,
const char *path,
const char *result_checksum,
apr_pool_t *pool);
svn_error_t *
svn_fs_contents_changed(svn_boolean_t *changed_p,
svn_fs_root_t *root1,
const char *path1,
svn_fs_root_t *root2,
const char *path2,
apr_pool_t *pool);
svn_error_t *
svn_fs_youngest_rev(svn_revnum_t *youngest_p,
svn_fs_t *fs,
apr_pool_t *pool);
svn_error_t *
svn_fs_deltify_revision(svn_fs_t *fs,
svn_revnum_t revision,
apr_pool_t *pool);
svn_error_t *
svn_fs_revision_prop(svn_string_t **value_p,
svn_fs_t *fs,
svn_revnum_t rev,
const char *propname,
apr_pool_t *pool);
svn_error_t *
svn_fs_revision_proplist(apr_hash_t **table_p,
svn_fs_t *fs,
svn_revnum_t rev,
apr_pool_t *pool);
svn_error_t *
svn_fs_change_rev_prop(svn_fs_t *fs,
svn_revnum_t rev,
const char *name,
const svn_string_t *value,
apr_pool_t *pool);
svn_error_t *
svn_fs_get_file_delta_stream(svn_txdelta_stream_t **stream_p,
svn_fs_root_t *source_root,
const char *source_path,
svn_fs_root_t *target_root,
const char *target_path,
apr_pool_t *pool);
svn_error_t *
svn_fs_get_uuid(svn_fs_t *fs,
const char **uuid,
apr_pool_t *pool);
svn_error_t *
svn_fs_set_uuid(svn_fs_t *fs,
const char *uuid,
apr_pool_t *pool);
svn_error_t *
svn_fs_lock(svn_lock_t **lock,
svn_fs_t *fs,
const char *path,
const char *token,
const char *comment,
svn_boolean_t is_dav_comment,
apr_time_t expiration_date,
svn_revnum_t current_rev,
svn_boolean_t steal_lock,
apr_pool_t *pool);
svn_error_t *
svn_fs_generate_lock_token(const char **token,
svn_fs_t *fs,
apr_pool_t *pool);
svn_error_t *
svn_fs_unlock(svn_fs_t *fs,
const char *path,
const char *token,
svn_boolean_t break_lock,
apr_pool_t *pool);
svn_error_t *
svn_fs_get_lock(svn_lock_t **lock,
svn_fs_t *fs,
const char *path,
apr_pool_t *pool);
typedef svn_error_t *(*svn_fs_get_locks_callback_t)(void *baton,
svn_lock_t *lock,
apr_pool_t *pool);
svn_error_t *
svn_fs_get_locks(svn_fs_t *fs,
const char *path,
svn_fs_get_locks_callback_t get_locks_func,
void *get_locks_baton,
apr_pool_t *pool);
svn_error_t *
svn_fs_print_modules(svn_stringbuf_t *output,
apr_pool_t *pool);
#if defined(__cplusplus)
}
#endif
#endif