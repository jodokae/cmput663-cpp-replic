#if !defined(SVN_LIBSVN_FS_KEY_GEN_H)
#define SVN_LIBSVN_FS_KEY_GEN_H
#include <apr.h>
#include "svn_types.h"
#if defined(__cplusplus)
extern "C" {
#endif
#define MAX_KEY_SIZE 200
void svn_fs_fs__next_key(const char *this, apr_size_t *len, char *next);
int svn_fs_fs__key_compare(const char *a, const char *b);
void svn_fs_fs__add_keys(const char *key1, const char *key2, char *result);
#if defined(__cplusplus)
}
#endif
#endif