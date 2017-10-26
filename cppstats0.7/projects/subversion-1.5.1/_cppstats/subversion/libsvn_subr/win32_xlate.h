#if !defined(SVN_LIBSVN_SUBR_WIN32_XLATE_H)
#define SVN_LIBSVN_SUBR_WIN32_XLATE_H
#if defined(WIN32)
typedef struct win32_xlate_t win32_xlate_t;
apr_status_t svn_subr__win32_xlate_open(win32_xlate_t **xlate_p,
const char *topage,
const char *frompage,
apr_pool_t *pool);
apr_status_t svn_subr__win32_xlate_to_stringbuf(win32_xlate_t *handle,
const char *src_data,
apr_size_t src_length,
svn_stringbuf_t **dest,
apr_pool_t *pool);
#endif
#endif
