#if !defined(SVN_STRING_H)
#define SVN_STRING_H
#include <apr.h>
#include <apr_tables.h>
#include <apr_pools.h>
#include <apr_strings.h>
#include "svn_types.h"
#if defined(__cplusplus)
extern "C" {
#endif
typedef struct svn_string_t {
const char *data;
apr_size_t len;
} svn_string_t;
typedef struct svn_stringbuf_t {
apr_pool_t *pool;
char *data;
apr_size_t len;
apr_size_t blocksize;
} svn_stringbuf_t;
svn_string_t *svn_string_create(const char *cstring,
apr_pool_t *pool);
svn_string_t *svn_string_ncreate(const char *bytes,
apr_size_t size,
apr_pool_t *pool);
svn_string_t *svn_string_create_from_buf(const svn_stringbuf_t *strbuf,
apr_pool_t *pool);
svn_string_t *svn_string_createf(apr_pool_t *pool,
const char *fmt,
...)
__attribute__((format(printf, 2, 3)));
svn_string_t *svn_string_createv(apr_pool_t *pool,
const char *fmt,
va_list ap)
__attribute__((format(printf, 2, 0)));
svn_boolean_t svn_string_isempty(const svn_string_t *str);
svn_string_t *svn_string_dup(const svn_string_t *original_string,
apr_pool_t *pool);
svn_boolean_t svn_string_compare(const svn_string_t *str1,
const svn_string_t *str2);
apr_size_t svn_string_first_non_whitespace(const svn_string_t *str);
apr_size_t svn_string_find_char_backward(const svn_string_t *str, char ch);
svn_stringbuf_t *svn_stringbuf_create(const char *cstring,
apr_pool_t *pool);
svn_stringbuf_t *svn_stringbuf_ncreate(const char *bytes,
apr_size_t size,
apr_pool_t *pool);
svn_stringbuf_t *svn_stringbuf_create_from_string(const svn_string_t *str,
apr_pool_t *pool);
svn_stringbuf_t *svn_stringbuf_createf(apr_pool_t *pool,
const char *fmt,
...)
__attribute__((format(printf, 2, 3)));
svn_stringbuf_t *svn_stringbuf_createv(apr_pool_t *pool,
const char *fmt,
va_list ap)
__attribute__((format(printf, 2, 0)));
void svn_stringbuf_ensure(svn_stringbuf_t *str,
apr_size_t minimum_size);
void svn_stringbuf_set(svn_stringbuf_t *str, const char *value);
void svn_stringbuf_setempty(svn_stringbuf_t *str);
svn_boolean_t svn_stringbuf_isempty(const svn_stringbuf_t *str);
void svn_stringbuf_chop(svn_stringbuf_t *str, apr_size_t nbytes);
void svn_stringbuf_fillchar(svn_stringbuf_t *str, unsigned char c);
void svn_stringbuf_appendbytes(svn_stringbuf_t *targetstr,
const char *bytes,
apr_size_t count);
void svn_stringbuf_appendstr(svn_stringbuf_t *targetstr,
const svn_stringbuf_t *appendstr);
void svn_stringbuf_appendcstr(svn_stringbuf_t *targetstr,
const char *cstr);
svn_stringbuf_t *svn_stringbuf_dup(const svn_stringbuf_t *original_string,
apr_pool_t *pool);
svn_boolean_t svn_stringbuf_compare(const svn_stringbuf_t *str1,
const svn_stringbuf_t *str2);
apr_size_t svn_stringbuf_first_non_whitespace(const svn_stringbuf_t *str);
void svn_stringbuf_strip_whitespace(svn_stringbuf_t *str);
apr_size_t svn_stringbuf_find_char_backward(const svn_stringbuf_t *str,
char ch);
svn_boolean_t svn_string_compare_stringbuf(const svn_string_t *str1,
const svn_stringbuf_t *str2);
apr_array_header_t *svn_cstring_split(const char *input,
const char *sep_chars,
svn_boolean_t chop_whitespace,
apr_pool_t *pool);
void svn_cstring_split_append(apr_array_header_t *array,
const char *input,
const char *sep_chars,
svn_boolean_t chop_whitespace,
apr_pool_t *pool);
svn_boolean_t svn_cstring_match_glob_list(const char *str,
apr_array_header_t *list);
int svn_cstring_count_newlines(const char *msg);
char *
svn_cstring_join(apr_array_header_t *strings,
const char *separator,
apr_pool_t *pool);
int svn_cstring_casecmp(const char *str1, const char *str2);
#if defined(__cplusplus)
}
#endif
#endif
