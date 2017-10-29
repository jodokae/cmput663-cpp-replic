#if !defined(SVN_LIBSVN_FS_STRINGS_TABLE_H)
#define SVN_LIBSVN_FS_STRINGS_TABLE_H
#define APU_WANT_DB
#include <apu_want.h>
#include "svn_io.h"
#include "svn_fs.h"
#include "../trail.h"
#if defined(__cplusplus)
extern "C" {
#endif
int svn_fs_bdb__open_strings_table(DB **strings_p,
DB_ENV *env,
svn_boolean_t create);
svn_error_t *svn_fs_bdb__string_read(svn_fs_t *fs,
const char *key,
char *buf,
svn_filesize_t offset,
apr_size_t *len,
trail_t *trail,
apr_pool_t *pool);
svn_error_t *svn_fs_bdb__string_size(svn_filesize_t *size,
svn_fs_t *fs,
const char *key,
trail_t *trail,
apr_pool_t *pool);
svn_error_t *svn_fs_bdb__string_append(svn_fs_t *fs,
const char **key,
apr_size_t len,
const char *buf,
trail_t *trail,
apr_pool_t *pool);
svn_error_t *svn_fs_bdb__string_clear(svn_fs_t *fs,
const char *key,
trail_t *trail,
apr_pool_t *pool);
svn_error_t *svn_fs_bdb__string_delete(svn_fs_t *fs,
const char *key,
trail_t *trail,
apr_pool_t *pool);
svn_error_t *svn_fs_bdb__string_copy(svn_fs_t *fs,
const char **new_key,
const char *key,
trail_t *trail,
apr_pool_t *pool);
#if defined(__cplusplus)
}
#endif
#endif