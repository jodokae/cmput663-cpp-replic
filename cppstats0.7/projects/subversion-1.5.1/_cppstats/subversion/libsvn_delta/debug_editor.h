#if !defined(SVN_DEBUG_EDITOR_H)
#define SVN_DEBUG_EDITOR_H
#include "svn_delta.h"
#if defined(__cplusplus)
extern "C" {
#endif
svn_error_t *
svn_delta__get_debug_editor(const svn_delta_editor_t **editor,
void **edit_baton,
const svn_delta_editor_t *wrapped_editor,
void *wrapped_baton,
apr_pool_t *pool);
#if defined(__cplusplus)
}
#endif
#endif