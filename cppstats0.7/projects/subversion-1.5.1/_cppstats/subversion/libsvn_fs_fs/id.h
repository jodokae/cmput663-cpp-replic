#if !defined(SVN_LIBSVN_FS_FS_ID_H)
#define SVN_LIBSVN_FS_FS_ID_H
#include "svn_fs.h"
#if defined(__cplusplus)
extern "C" {
#endif
const char *svn_fs_fs__id_node_id(const svn_fs_id_t *id);
const char *svn_fs_fs__id_copy_id(const svn_fs_id_t *id);
const char *svn_fs_fs__id_txn_id(const svn_fs_id_t *id);
svn_revnum_t svn_fs_fs__id_rev(const svn_fs_id_t *id);
apr_off_t svn_fs_fs__id_offset(const svn_fs_id_t *id);
svn_string_t *svn_fs_fs__id_unparse(const svn_fs_id_t *id,
apr_pool_t *pool);
svn_boolean_t svn_fs_fs__id_eq(const svn_fs_id_t *a,
const svn_fs_id_t *b);
svn_boolean_t svn_fs_fs__id_check_related(const svn_fs_id_t *a,
const svn_fs_id_t *b);
int svn_fs_fs__id_compare(const svn_fs_id_t *a,
const svn_fs_id_t *b);
svn_fs_id_t *svn_fs_fs__id_txn_create(const char *node_id,
const char *copy_id,
const char *txn_id,
apr_pool_t *pool);
svn_fs_id_t *svn_fs_fs__id_rev_create(const char *node_id,
const char *copy_id,
svn_revnum_t rev,
apr_off_t offset,
apr_pool_t *pool);
svn_fs_id_t *svn_fs_fs__id_copy(const svn_fs_id_t *id,
apr_pool_t *pool);
svn_fs_id_t *svn_fs_fs__id_parse(const char *data,
apr_size_t len,
apr_pool_t *pool);
#if defined(__cplusplus)
}
#endif
#endif