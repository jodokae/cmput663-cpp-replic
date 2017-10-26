#if !defined(SVN_UTF_H)
#define SVN_UTF_H
#include <apr_xlate.h>
#include "svn_error.h"
#include "svn_string.h"
#if defined(__cplusplus)
extern "C" {
#endif
#if !defined(AS400)
#define SVN_APR_LOCALE_CHARSET APR_LOCALE_CHARSET
#define SVN_APR_DEFAULT_CHARSET APR_DEFAULT_CHARSET
#else
#define SVN_APR_LOCALE_CHARSET (const char*)APR_LOCALE_CHARSET
#define SVN_APR_DEFAULT_CHARSET (const char*)APR_DEFAULT_CHARSET
#endif
void svn_utf_initialize(apr_pool_t *pool);
svn_error_t *svn_utf_stringbuf_to_utf8(svn_stringbuf_t **dest,
const svn_stringbuf_t *src,
apr_pool_t *pool);
svn_error_t *svn_utf_string_to_utf8(const svn_string_t **dest,
const svn_string_t *src,
apr_pool_t *pool);
svn_error_t *svn_utf_cstring_to_utf8(const char **dest,
const char *src,
apr_pool_t *pool);
svn_error_t *svn_utf_cstring_to_utf8_ex2(const char **dest,
const char *src,
const char *frompage,
apr_pool_t *pool);
svn_error_t *svn_utf_cstring_to_utf8_ex(const char **dest,
const char *src,
const char *frompage,
const char *convset_key,
apr_pool_t *pool);
svn_error_t *svn_utf_stringbuf_from_utf8(svn_stringbuf_t **dest,
const svn_stringbuf_t *src,
apr_pool_t *pool);
svn_error_t *svn_utf_string_from_utf8(const svn_string_t **dest,
const svn_string_t *src,
apr_pool_t *pool);
svn_error_t *svn_utf_cstring_from_utf8(const char **dest,
const char *src,
apr_pool_t *pool);
svn_error_t *svn_utf_cstring_from_utf8_ex2(const char **dest,
const char *src,
const char *topage,
apr_pool_t *pool);
svn_error_t *svn_utf_cstring_from_utf8_ex(const char **dest,
const char *src,
const char *topage,
const char *convset_key,
apr_pool_t *pool);
const char *svn_utf_cstring_from_utf8_fuzzy(const char *src,
apr_pool_t *pool);
svn_error_t *svn_utf_cstring_from_utf8_stringbuf(const char **dest,
const svn_stringbuf_t *src,
apr_pool_t *pool);
svn_error_t *svn_utf_cstring_from_utf8_string(const char **dest,
const svn_string_t *src,
apr_pool_t *pool);
#if defined(__cplusplus)
}
#endif
#endif
