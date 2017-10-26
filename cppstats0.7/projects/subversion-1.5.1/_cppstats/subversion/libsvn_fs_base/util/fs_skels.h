#if !defined(SVN_LIBSVN_FS_FS_SKELS_H)
#define SVN_LIBSVN_FS_FS_SKELS_H
#define APU_WANT_DB
#include <apu_want.h>
#include <apr_pools.h>
#include <apr_hash.h>
#include "svn_fs.h"
#include "../fs.h"
#include "skel.h"
#if defined(__cplusplus)
extern "C" {
#endif
svn_error_t *
svn_fs_base__parse_proplist_skel(apr_hash_t **proplist_p,
skel_t *skel,
apr_pool_t *pool);
svn_error_t *
svn_fs_base__parse_revision_skel(revision_t **revision_p,
skel_t *skel,
apr_pool_t *pool);
svn_error_t *
svn_fs_base__parse_transaction_skel(transaction_t **transaction_p,
skel_t *skel,
apr_pool_t *pool);
svn_error_t *
svn_fs_base__parse_representation_skel(representation_t **rep_p,
skel_t *skel,
apr_pool_t *pool);
svn_error_t *
svn_fs_base__parse_node_revision_skel(node_revision_t **noderev_p,
skel_t *skel,
apr_pool_t *pool);
svn_error_t *
svn_fs_base__parse_copy_skel(copy_t **copy_p,
skel_t *skel,
apr_pool_t *pool);
svn_error_t *
svn_fs_base__parse_entries_skel(apr_hash_t **entries_p,
skel_t *skel,
apr_pool_t *pool);
svn_error_t *
svn_fs_base__parse_change_skel(change_t **change_p,
skel_t *skel,
apr_pool_t *pool);
svn_error_t *
svn_fs_base__parse_lock_skel(svn_lock_t **lock_p,
skel_t *skel,
apr_pool_t *pool);
svn_error_t *
svn_fs_base__unparse_proplist_skel(skel_t **skel_p,
apr_hash_t *proplist,
apr_pool_t *pool);
svn_error_t *
svn_fs_base__unparse_revision_skel(skel_t **skel_p,
const revision_t *revision,
apr_pool_t *pool);
svn_error_t *
svn_fs_base__unparse_transaction_skel(skel_t **skel_p,
const transaction_t *transaction,
apr_pool_t *pool);
svn_error_t *
svn_fs_base__unparse_representation_skel(skel_t **skel_p,
const representation_t *rep,
apr_pool_t *pool);
svn_error_t *
svn_fs_base__unparse_node_revision_skel(skel_t **skel_p,
const node_revision_t *noderev,
int format,
apr_pool_t *pool);
svn_error_t *
svn_fs_base__unparse_copy_skel(skel_t **skel_p,
const copy_t *copy,
apr_pool_t *pool);
svn_error_t *
svn_fs_base__unparse_entries_skel(skel_t **skel_p,
apr_hash_t *entries,
apr_pool_t *pool);
svn_error_t *
svn_fs_base__unparse_change_skel(skel_t **skel_p,
const change_t *change,
apr_pool_t *pool);
svn_error_t *
svn_fs_base__unparse_lock_skel(skel_t **skel_p,
const svn_lock_t *lock,
apr_pool_t *pool);
#if defined(__cplusplus)
}
#endif
#endif
