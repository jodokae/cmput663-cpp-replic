#if !defined(SVN_LIBSVN_FS_SKEL_H)
#define SVN_LIBSVN_FS_SKEL_H
#include <apr_pools.h>
#include "svn_string.h"
#if defined(__cplusplus)
extern "C" {
#endif
struct skel_t {
svn_boolean_t is_atom;
const char *data;
apr_size_t len;
struct skel_t *children;
struct skel_t *next;
};
typedef struct skel_t skel_t;
skel_t *svn_fs_base__parse_skel(const char *data, apr_size_t len,
apr_pool_t *pool);
skel_t *svn_fs_base__str_atom(const char *str, apr_pool_t *pool);
skel_t *svn_fs_base__mem_atom(const void *addr, apr_size_t len,
apr_pool_t *pool);
skel_t *svn_fs_base__make_empty_list(apr_pool_t *pool);
void svn_fs_base__prepend(skel_t *skel, skel_t *list);
void svn_fs_base__append(skel_t *skel, skel_t *list);
svn_stringbuf_t *svn_fs_base__unparse_skel(skel_t *skel, apr_pool_t *pool);
svn_boolean_t svn_fs_base__matches_atom(skel_t *skel, const char *str);
svn_boolean_t svn_fs_base__atom_matches_string(skel_t *skel,
const svn_string_t *str);
int svn_fs_base__list_length(skel_t *skel);
svn_boolean_t svn_fs_base__skels_are_equal(skel_t *skel1, skel_t *skel2);
skel_t *svn_fs_base__copy_skel(skel_t *skel, apr_pool_t *pool);
#if defined(__cplusplus)
}
#endif
#endif
