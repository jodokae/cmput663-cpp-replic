#if !defined(LIBSVN_FS_FS_H)
#define LIBSVN_FS_FS_H
#include "svn_version.h"
#include "svn_fs.h"
#if defined(__cplusplus)
extern "C" {
#endif
typedef struct fs_library_vtable_t {
const svn_version_t *(*get_version)(void);
svn_error_t *(*create)(svn_fs_t *fs, const char *path, apr_pool_t *pool,
apr_pool_t *common_pool);
svn_error_t *(*open_fs)(svn_fs_t *fs, const char *path, apr_pool_t *pool,
apr_pool_t *common_pool);
svn_error_t *(*open_fs_for_recovery)(svn_fs_t *fs, const char *path,
apr_pool_t *pool,
apr_pool_t *common_pool);
svn_error_t *(*upgrade_fs)(svn_fs_t *fs, const char *path, apr_pool_t *pool,
apr_pool_t *common_pool);
svn_error_t *(*delete_fs)(const char *path, apr_pool_t *pool);
svn_error_t *(*hotcopy)(const char *src_path, const char *dest_path,
svn_boolean_t clean, apr_pool_t *pool);
const char *(*get_description)(void);
svn_error_t *(*recover)(svn_fs_t *fs,
svn_cancel_func_t cancel_func, void *cancel_baton,
apr_pool_t *pool);
svn_error_t *(*bdb_logfiles)(apr_array_header_t **logfiles,
const char *path, svn_boolean_t only_unused,
apr_pool_t *pool);
svn_fs_id_t *(*parse_id)(const char *data, apr_size_t len,
apr_pool_t *pool);
} fs_library_vtable_t;
typedef svn_error_t *(*fs_init_func_t)(const svn_version_t *loader_version,
fs_library_vtable_t **vtable,
apr_pool_t* common_pool);
svn_error_t *svn_fs_base__init(const svn_version_t *loader_version,
fs_library_vtable_t **vtable,
apr_pool_t* common_pool);
svn_error_t *svn_fs_fs__init(const svn_version_t *loader_version,
fs_library_vtable_t **vtable,
apr_pool_t* common_pool);
typedef struct fs_vtable_t {
svn_error_t *(*youngest_rev)(svn_revnum_t *youngest_p, svn_fs_t *fs,
apr_pool_t *pool);
svn_error_t *(*revision_prop)(svn_string_t **value_p, svn_fs_t *fs,
svn_revnum_t rev, const char *propname,
apr_pool_t *pool);
svn_error_t *(*revision_proplist)(apr_hash_t **table_p, svn_fs_t *fs,
svn_revnum_t rev, apr_pool_t *pool);
svn_error_t *(*change_rev_prop)(svn_fs_t *fs, svn_revnum_t rev,
const char *name,
const svn_string_t *value,
apr_pool_t *pool);
svn_error_t *(*get_uuid)(svn_fs_t *fs, const char **uuid, apr_pool_t *pool);
svn_error_t *(*set_uuid)(svn_fs_t *fs, const char *uuid, apr_pool_t *pool);
svn_error_t *(*revision_root)(svn_fs_root_t **root_p, svn_fs_t *fs,
svn_revnum_t rev, apr_pool_t *pool);
svn_error_t *(*begin_txn)(svn_fs_txn_t **txn_p, svn_fs_t *fs,
svn_revnum_t rev, apr_uint32_t flags,
apr_pool_t *pool);
svn_error_t *(*open_txn)(svn_fs_txn_t **txn, svn_fs_t *fs,
const char *name, apr_pool_t *pool);
svn_error_t *(*purge_txn)(svn_fs_t *fs, const char *txn_id,
apr_pool_t *pool);
svn_error_t *(*list_transactions)(apr_array_header_t **names_p,
svn_fs_t *fs, apr_pool_t *pool);
svn_error_t *(*deltify)(svn_fs_t *fs, svn_revnum_t rev, apr_pool_t *pool);
svn_error_t *(*lock)(svn_lock_t **lock, svn_fs_t *fs,
const char *path, const char *token,
const char *comment, svn_boolean_t is_dav_comment,
apr_time_t expiration_date,
svn_revnum_t current_rev, svn_boolean_t steal_lock,
apr_pool_t *pool);
svn_error_t *(*generate_lock_token)(const char **token, svn_fs_t *fs,
apr_pool_t *pool);
svn_error_t *(*unlock)(svn_fs_t *fs, const char *path, const char *token,
svn_boolean_t break_lock, apr_pool_t *pool);
svn_error_t *(*get_lock)(svn_lock_t **lock, svn_fs_t *fs,
const char *path, apr_pool_t *pool);
svn_error_t *(*get_locks)(svn_fs_t *fs, const char *path,
svn_fs_get_locks_callback_t get_locks_func,
void *get_locks_baton,
apr_pool_t *pool);
svn_error_t *(*bdb_set_errcall)(svn_fs_t *fs,
void (*handler)(const char *errpfx,
char *msg));
} fs_vtable_t;
typedef struct txn_vtable_t {
svn_error_t *(*commit)(const char **conflict_p, svn_revnum_t *new_rev,
svn_fs_txn_t *txn, apr_pool_t *pool);
svn_error_t *(*abort)(svn_fs_txn_t *txn, apr_pool_t *pool);
svn_error_t *(*get_prop)(svn_string_t **value_p, svn_fs_txn_t *txn,
const char *propname, apr_pool_t *pool);
svn_error_t *(*get_proplist)(apr_hash_t **table_p, svn_fs_txn_t *txn,
apr_pool_t *pool);
svn_error_t *(*change_prop)(svn_fs_txn_t *txn, const char *name,
const svn_string_t *value, apr_pool_t *pool);
svn_error_t *(*root)(svn_fs_root_t **root_p, svn_fs_txn_t *txn,
apr_pool_t *pool);
svn_error_t *(*change_props)(svn_fs_txn_t *txn, apr_array_header_t *props,
apr_pool_t *pool);
} txn_vtable_t;
typedef struct root_vtable_t {
svn_error_t *(*paths_changed)(apr_hash_t **changed_paths_p,
svn_fs_root_t *root,
apr_pool_t *pool);
svn_error_t *(*check_path)(svn_node_kind_t *kind_p, svn_fs_root_t *root,
const char *path, apr_pool_t *pool);
svn_error_t *(*node_history)(svn_fs_history_t **history_p,
svn_fs_root_t *root, const char *path,
apr_pool_t *pool);
svn_error_t *(*node_id)(const svn_fs_id_t **id_p, svn_fs_root_t *root,
const char *path, apr_pool_t *pool);
svn_error_t *(*node_created_rev)(svn_revnum_t *revision,
svn_fs_root_t *root, const char *path,
apr_pool_t *pool);
svn_error_t *(*node_origin_rev)(svn_revnum_t *revision,
svn_fs_root_t *root, const char *path,
apr_pool_t *pool);
svn_error_t *(*node_created_path)(const char **created_path,
svn_fs_root_t *root, const char *path,
apr_pool_t *pool);
svn_error_t *(*delete_node)(svn_fs_root_t *root, const char *path,
apr_pool_t *pool);
svn_error_t *(*copied_from)(svn_revnum_t *rev_p, const char **path_p,
svn_fs_root_t *root, const char *path,
apr_pool_t *pool);
svn_error_t *(*closest_copy)(svn_fs_root_t **root_p, const char **path_p,
svn_fs_root_t *root, const char *path,
apr_pool_t *pool);
svn_error_t *(*node_prop)(svn_string_t **value_p, svn_fs_root_t *root,
const char *path, const char *propname,
apr_pool_t *pool);
svn_error_t *(*node_proplist)(apr_hash_t **table_p, svn_fs_root_t *root,
const char *path, apr_pool_t *pool);
svn_error_t *(*change_node_prop)(svn_fs_root_t *root, const char *path,
const char *name,
const svn_string_t *value,
apr_pool_t *pool);
svn_error_t *(*props_changed)(int *changed_p, svn_fs_root_t *root1,
const char *path1, svn_fs_root_t *root2,
const char *path2, apr_pool_t *pool);
svn_error_t *(*dir_entries)(apr_hash_t **entries_p, svn_fs_root_t *root,
const char *path, apr_pool_t *pool);
svn_error_t *(*make_dir)(svn_fs_root_t *root, const char *path,
apr_pool_t *pool);
svn_error_t *(*copy)(svn_fs_root_t *from_root, const char *from_path,
svn_fs_root_t *to_root, const char *to_path,
apr_pool_t *pool);
svn_error_t *(*revision_link)(svn_fs_root_t *from_root,
svn_fs_root_t *to_root,
const char *path,
apr_pool_t *pool);
svn_error_t *(*file_length)(svn_filesize_t *length_p, svn_fs_root_t *root,
const char *path, apr_pool_t *pool);
svn_error_t *(*file_md5_checksum)(unsigned char digest[],
svn_fs_root_t *root,
const char *path, apr_pool_t *pool);
svn_error_t *(*file_contents)(svn_stream_t **contents,
svn_fs_root_t *root, const char *path,
apr_pool_t *pool);
svn_error_t *(*make_file)(svn_fs_root_t *root, const char *path,
apr_pool_t *pool);
svn_error_t *(*apply_textdelta)(svn_txdelta_window_handler_t *contents_p,
void **contents_baton_p,
svn_fs_root_t *root, const char *path,
const char *base_checksum,
const char *result_checksum,
apr_pool_t *pool);
svn_error_t *(*apply_text)(svn_stream_t **contents_p, svn_fs_root_t *root,
const char *path, const char *result_checksum,
apr_pool_t *pool);
svn_error_t *(*contents_changed)(int *changed_p, svn_fs_root_t *root1,
const char *path1, svn_fs_root_t *root2,
const char *path2, apr_pool_t *pool);
svn_error_t *(*get_file_delta_stream)(svn_txdelta_stream_t **stream_p,
svn_fs_root_t *source_root,
const char *source_path,
svn_fs_root_t *target_root,
const char *target_path,
apr_pool_t *pool);
svn_error_t *(*merge)(const char **conflict_p,
svn_fs_root_t *source_root,
const char *source_path,
svn_fs_root_t *target_root,
const char *target_path,
svn_fs_root_t *ancestor_root,
const char *ancestor_path,
apr_pool_t *pool);
svn_error_t *(*get_mergeinfo)(svn_mergeinfo_catalog_t *catalog,
svn_fs_root_t *root,
const apr_array_header_t *paths,
svn_mergeinfo_inheritance_t inherit,
svn_boolean_t include_descendants,
apr_pool_t *pool);
} root_vtable_t;
typedef struct history_vtable_t {
svn_error_t *(*prev)(svn_fs_history_t **prev_history_p,
svn_fs_history_t *history, svn_boolean_t cross_copies,
apr_pool_t *pool);
svn_error_t *(*location)(const char **path, svn_revnum_t *revision,
svn_fs_history_t *history, apr_pool_t *pool);
} history_vtable_t;
typedef struct id_vtable_t {
svn_string_t *(*unparse)(const svn_fs_id_t *id, apr_pool_t *pool);
int (*compare)(const svn_fs_id_t *a, const svn_fs_id_t *b);
} id_vtable_t;
#define SVN_FS__PROP_TXN_CHECK_LOCKS SVN_PROP_PREFIX "check-locks"
#define SVN_FS__PROP_TXN_CHECK_OOD SVN_PROP_PREFIX "check-ood"
struct svn_fs_t {
apr_pool_t *pool;
char *path;
svn_fs_warning_callback_t warning;
void *warning_baton;
apr_hash_t *config;
svn_fs_access_t *access_ctx;
fs_vtable_t *vtable;
void *fsap_data;
};
struct svn_fs_txn_t {
svn_fs_t *fs;
svn_revnum_t base_rev;
const char *id;
txn_vtable_t *vtable;
void *fsap_data;
};
struct svn_fs_root_t {
apr_pool_t *pool;
svn_fs_t *fs;
svn_boolean_t is_txn_root;
const char *txn;
apr_uint32_t txn_flags;
svn_revnum_t rev;
root_vtable_t *vtable;
void *fsap_data;
};
struct svn_fs_history_t {
history_vtable_t *vtable;
void *fsap_data;
};
struct svn_fs_id_t {
id_vtable_t *vtable;
void *fsap_data;
};
struct svn_fs_access_t {
const char *username;
apr_hash_t *lock_tokens;
};
#if defined(__cplusplus)
}
#endif
#endif