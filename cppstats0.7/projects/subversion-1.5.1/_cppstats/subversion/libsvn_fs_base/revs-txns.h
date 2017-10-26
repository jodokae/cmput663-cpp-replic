#if !defined(SVN_LIBSVN_FS_REVS_TXNS_H)
#define SVN_LIBSVN_FS_REVS_TXNS_H
#define APU_WANT_DB
#include <apu_want.h>
#include "svn_fs.h"
#include "fs.h"
#include "trail.h"
#if defined(__cplusplus)
extern "C" {
#endif
svn_error_t *svn_fs_base__rev_get_root(const svn_fs_id_t **root_id_p,
svn_fs_t *fs,
svn_revnum_t rev,
trail_t *trail,
apr_pool_t *pool);
svn_error_t *svn_fs_base__rev_get_txn_id(const char **txn_id_p,
svn_fs_t *fs,
svn_revnum_t rev,
trail_t *trail,
apr_pool_t *pool);
svn_error_t *svn_fs_base__set_rev_prop(svn_fs_t *fs,
svn_revnum_t rev,
const char *name,
const svn_string_t *value,
trail_t *trail,
apr_pool_t *pool);
svn_error_t *svn_fs_base__txn_make_committed(svn_fs_t *fs,
const char *txn_name,
svn_revnum_t revision,
trail_t *trail,
apr_pool_t *pool);
svn_error_t *svn_fs_base__txn_get_revision(svn_revnum_t *revision,
svn_fs_t *fs,
const char *txn_name,
trail_t *trail,
apr_pool_t *pool);
svn_error_t *svn_fs_base__get_txn_ids(const svn_fs_id_t **root_id_p,
const svn_fs_id_t **base_root_id_p,
svn_fs_t *fs,
const char *txn_name,
trail_t *trail,
apr_pool_t *pool);
svn_error_t *svn_fs_base__set_txn_root(svn_fs_t *fs,
const char *txn_name,
const svn_fs_id_t *root_id,
trail_t *trail,
apr_pool_t *pool);
svn_error_t *svn_fs_base__add_txn_copy(svn_fs_t *fs,
const char *txn_name,
const char *copy_id,
trail_t *trail,
apr_pool_t *pool);
svn_error_t *svn_fs_base__set_txn_base(svn_fs_t *fs,
const char *txn_name,
const svn_fs_id_t *new_id,
trail_t *trail,
apr_pool_t *pool);
svn_error_t *svn_fs_base__set_txn_prop(svn_fs_t *fs,
const char *txn_name,
const char *name,
const svn_string_t *value,
trail_t *trail,
apr_pool_t *pool);
svn_error_t *svn_fs_base__youngest_rev(svn_revnum_t *youngest_p, svn_fs_t *fs,
apr_pool_t *pool);
svn_error_t *svn_fs_base__revision_prop(svn_string_t **value_p, svn_fs_t *fs,
svn_revnum_t rev,
const char *propname,
apr_pool_t *pool);
svn_error_t *svn_fs_base__revision_proplist(apr_hash_t **table_p,
svn_fs_t *fs,
svn_revnum_t rev,
apr_pool_t *pool);
svn_error_t *svn_fs_base__change_rev_prop(svn_fs_t *fs, svn_revnum_t rev,
const char *name,
const svn_string_t *value,
apr_pool_t *pool);
svn_error_t *svn_fs_base__begin_txn(svn_fs_txn_t **txn_p, svn_fs_t *fs,
svn_revnum_t rev, apr_uint32_t flags,
apr_pool_t *pool);
svn_error_t *svn_fs_base__open_txn(svn_fs_txn_t **txn, svn_fs_t *fs,
const char *name, apr_pool_t *pool);
svn_error_t *svn_fs_base__purge_txn(svn_fs_t *fs, const char *txn_id,
apr_pool_t *pool);
svn_error_t *svn_fs_base__list_transactions(apr_array_header_t **names_p,
svn_fs_t *fs, apr_pool_t *pool);
svn_error_t *svn_fs_base__abort_txn(svn_fs_txn_t *txn, apr_pool_t *pool);
svn_error_t *svn_fs_base__txn_prop(svn_string_t **value_p, svn_fs_txn_t *txn,
const char *propname, apr_pool_t *pool);
svn_error_t *svn_fs_base__txn_proplist(apr_hash_t **table_p,
svn_fs_txn_t *txn,
apr_pool_t *pool);
svn_error_t *svn_fs_base__txn_proplist_in_trail(apr_hash_t **table_p,
const char *txn_id,
trail_t *trail);
svn_error_t *svn_fs_base__change_txn_prop(svn_fs_txn_t *txn, const char *name,
const svn_string_t *value,
apr_pool_t *pool);
svn_error_t *svn_fs_base__change_txn_props(svn_fs_txn_t *txn,
apr_array_header_t *props,
apr_pool_t *pool);
#if defined(__cplusplus)
}
#endif
#endif
