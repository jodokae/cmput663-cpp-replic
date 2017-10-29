#if !defined(SVN_COMPAT_H)
#define SVN_COMPAT_H
#include <apr_pools.h>
#include <apr_hash.h>
#include <apr_tables.h>
#include "svn_types.h"
#if defined(__cplusplus)
extern "C" {
#endif
void svn_compat_wrap_commit_callback(svn_commit_callback2_t *callback2,
void **callback2_baton,
svn_commit_callback_t callback,
void *callback_baton,
apr_pool_t *pool);
void
svn_compat_log_revprops_clear(apr_hash_t *revprops);
apr_array_header_t *
svn_compat_log_revprops_in(apr_pool_t *pool);
void
svn_compat_log_revprops_out(const char **author, const char **date,
const char **message, apr_hash_t *revprops);
void svn_compat_wrap_log_receiver(svn_log_entry_receiver_t *receiver2,
void **receiver2_baton,
svn_log_message_receiver_t receiver,
void *receiver_baton,
apr_pool_t *pool);
#if defined(__cplusplus)
}
#endif
#endif