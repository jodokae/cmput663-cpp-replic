#if !defined(SVN_LIBSVN_FS_KEY_GEN_H)
#define SVN_LIBSVN_FS_KEY_GEN_H
#include <apr.h>
#include "svn_types.h"
#if defined(__cplusplus)
extern "C" {
#endif
#define MAX_KEY_SIZE 200
#define NEXT_KEY_KEY "next-key"
apr_size_t svn_fs_base__getsize(const char *data, apr_size_t len,
const char **endptr, apr_size_t max);
int svn_fs_base__putsize(char *data, apr_size_t len, apr_size_t value);
void svn_fs_base__next_key(const char *this, apr_size_t *len, char *next);
int svn_fs_base__key_compare(const char *a, const char *b);
svn_boolean_t svn_fs_base__same_keys(const char *a, const char *b);
#if defined(__cplusplus)
}
#endif
#endif
