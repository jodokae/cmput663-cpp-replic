#if !defined(SVN_DSO_H)
#define SVN_DSO_H
#include <apr_dso.h>
#include "svn_error.h"
#if defined(__cplusplus)
extern "C" {
#endif
void svn_dso_initialize(void);
#if APR_HAS_DSO
svn_error_t *svn_dso_load(apr_dso_handle_t **dso, const char *libname);
#endif
#if defined(__cplusplus)
}
#endif
#endif
