#if !defined(SVN_LIBSVN_FS_TXN_TABLE_H)
#define SVN_LIBSVN_FS_TXN_TABLE_H
#include "svn_fs.h"
#include "svn_error.h"
#include "../trail.h"
#include "../fs.h"
#if defined(__cplusplus)
extern "C" {
#endif
int svn_fs_bdb__open_transactions_table(DB **transactions_p,
DB_ENV *env,
svn_boolean_t create);
svn_error_t *svn_fs_bdb__create_txn(const char **txn_name_p,
svn_fs_t *fs,
const svn_fs_id_t *root_id,
trail_t *trail,
apr_pool_t *pool);
svn_error_t *svn_fs_bdb__delete_txn(svn_fs_t *fs,
const char *txn_name,
trail_t *trail,
apr_pool_t *pool);
svn_error_t *svn_fs_bdb__get_txn(transaction_t **txn_p,
svn_fs_t *fs,
const char *txn_name,
trail_t *trail,
apr_pool_t *pool);
svn_error_t *svn_fs_bdb__put_txn(svn_fs_t *fs,
const transaction_t *txn,
const char *txn_name,
trail_t *trail,
apr_pool_t *pool);
svn_error_t *svn_fs_bdb__get_txn_list(apr_array_header_t **names_p,
svn_fs_t *fs,
trail_t *trail,
apr_pool_t *pool);
#if defined(__cplusplus)
}
#endif
#endif
