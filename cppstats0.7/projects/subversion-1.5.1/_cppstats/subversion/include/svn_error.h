#if !defined(SVN_ERROR_H)
#define SVN_ERROR_H
#include <apr.h>
#include <apr_errno.h>
#include <apr_pools.h>
#if !defined(DOXYGEN_SHOULD_SKIP_THIS)
#define APR_WANT_STDIO
#endif
#include <apr_want.h>
#include "svn_types.h"
#if defined(__cplusplus)
extern "C" {
#endif
#define SVN_NO_ERROR 0
#include "svn_error_codes.h"
void svn_error__locate(const char *file, long line);
char *svn_strerror(apr_status_t statcode, char *buf, apr_size_t bufsize);
const char *svn_err_best_message(svn_error_t *err,
char *buf, apr_size_t bufsize);
svn_error_t *svn_error_create(apr_status_t apr_err,
svn_error_t *child,
const char *message);
#define svn_error_create (svn_error__locate(__FILE__,__LINE__), (svn_error_create))
svn_error_t *svn_error_createf(apr_status_t apr_err,
svn_error_t *child,
const char *fmt,
...)
__attribute__ ((format(printf, 3, 4)));
#define svn_error_createf (svn_error__locate(__FILE__,__LINE__), (svn_error_createf))
svn_error_t *svn_error_wrap_apr(apr_status_t status, const char *fmt, ...)
__attribute__((format(printf, 2, 3)));
#define svn_error_wrap_apr (svn_error__locate(__FILE__,__LINE__), (svn_error_wrap_apr))
svn_error_t *svn_error_quick_wrap(svn_error_t *child, const char *new_msg);
#define svn_error_quick_wrap (svn_error__locate(__FILE__,__LINE__), (svn_error_quick_wrap))
void svn_error_compose(svn_error_t *chain, svn_error_t *new_err);
svn_error_t *svn_error_root_cause(svn_error_t *err);
svn_error_t *svn_error_dup(svn_error_t *err);
void svn_error_clear(svn_error_t *error);
void svn_handle_error2(svn_error_t *error,
FILE *stream,
svn_boolean_t fatal,
const char *prefix);
void svn_handle_error(svn_error_t *error,
FILE *stream,
svn_boolean_t fatal);
void svn_handle_warning2(FILE *stream, svn_error_t *error, const char *prefix);
void svn_handle_warning(FILE *stream, svn_error_t *error);
#define SVN_ERR(expr) do { svn_error_t *svn_err__temp = (expr); if (svn_err__temp) return svn_err__temp; } while (0)
#define SVN_ERR_W(expr, wrap_msg) do { svn_error_t *svn_err__temp = (expr); if (svn_err__temp) return svn_error_quick_wrap(svn_err__temp, wrap_msg); } while (0)
#define SVN_INT_ERR(expr) do { svn_error_t *svn_err__temp = (expr); if (svn_err__temp) { svn_handle_error2(svn_err__temp, stderr, FALSE, "svn: "); svn_error_clear(svn_err__temp); return EXIT_FAILURE; } } while (0)
#define SVN_ERR_IS_LOCK_ERROR(err) (err->apr_err == SVN_ERR_FS_PATH_ALREADY_LOCKED || err->apr_err == SVN_ERR_FS_OUT_OF_DATE)
#define SVN_ERR_IS_UNLOCK_ERROR(err) (err->apr_err == SVN_ERR_FS_PATH_NOT_LOCKED || err->apr_err == SVN_ERR_FS_BAD_LOCK_TOKEN || err->apr_err == SVN_ERR_FS_LOCK_OWNER_MISMATCH || err->apr_err == SVN_ERR_FS_NO_SUCH_LOCK || err->apr_err == SVN_ERR_RA_NOT_LOCKED || err->apr_err == SVN_ERR_FS_LOCK_EXPIRED)
#if defined(__cplusplus)
}
#endif
#endif