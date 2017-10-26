#if !defined(SVN_LIBSVN_FS_REPS_STRINGS_H)
#define SVN_LIBSVN_FS_REPS_STRINGS_H
#define APU_WANT_DB
#include <apu_want.h>
#include "svn_io.h"
#include "svn_fs.h"
#include "trail.h"
#if defined(__cplusplus)
extern "C" {
#endif
svn_error_t *svn_fs_base__get_mutable_rep(const char **new_rep_key,
const char *rep_key,
svn_fs_t *fs,
const char *txn_id,
trail_t *trail,
apr_pool_t *pool);
svn_error_t *svn_fs_base__delete_rep_if_mutable(svn_fs_t *fs,
const char *rep_key,
const char *txn_id,
trail_t *trail,
apr_pool_t *pool);
svn_error_t *svn_fs_base__rep_contents_size(svn_filesize_t *size_p,
svn_fs_t *fs,
const char *rep_key,
trail_t *trail,
apr_pool_t *pool);
svn_error_t *svn_fs_base__rep_contents_checksum(unsigned char digest[],
svn_fs_t *fs,
const char *rep_key,
trail_t *trail,
apr_pool_t *pool);
svn_error_t *svn_fs_base__rep_contents(svn_string_t *str,
svn_fs_t *fs,
const char *rep_key,
trail_t *trail,
apr_pool_t *pool);
svn_error_t *
svn_fs_base__rep_contents_read_stream(svn_stream_t **rs_p,
svn_fs_t *fs,
const char *rep_key,
svn_boolean_t use_trail_for_reads,
trail_t *trail,
apr_pool_t *pool);
svn_error_t *
svn_fs_base__rep_contents_write_stream(svn_stream_t **ws_p,
svn_fs_t *fs,
const char *rep_key,
const char *txn_id,
svn_boolean_t use_trail_for_writes,
trail_t *trail,
apr_pool_t *pool);
svn_error_t *svn_fs_base__rep_deltify(svn_fs_t *fs,
const char *target,
const char *source,
trail_t *trail,
apr_pool_t *pool);
#if defined(__cplusplus)
}
#endif
#endif
