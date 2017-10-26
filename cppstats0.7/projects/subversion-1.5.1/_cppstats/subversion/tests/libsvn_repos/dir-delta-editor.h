#if !defined(SVN_TEST__DIR_DELTA_EDITOR_H)
#define SVN_TEST__DIR_DELTA_EDITOR_H
#include <stdio.h>
#include <apr_pools.h>
#include "svn_types.h"
#include "svn_error.h"
#include "svn_delta.h"
#if defined(__cplusplus)
extern "C" {
#endif
svn_error_t *
dir_delta_get_editor(const svn_delta_editor_t **editor,
void **edit_baton,
svn_fs_t *fs,
svn_fs_root_t *txn_root,
const char *path,
apr_pool_t *pool);
#if defined(__cplusplus)
}
#endif
#endif
