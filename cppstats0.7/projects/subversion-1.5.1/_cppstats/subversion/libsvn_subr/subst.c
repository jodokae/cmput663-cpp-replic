#define APR_WANT_STRFUNC
#include <apr_want.h>
#include <stdlib.h>
#include <assert.h>
#include <apr_pools.h>
#include <apr_tables.h>
#include <apr_file_io.h>
#include <apr_strings.h>
#include "svn_cmdline.h"
#include "svn_types.h"
#include "svn_string.h"
#include "svn_time.h"
#include "svn_path.h"
#include "svn_error.h"
#include "svn_utf.h"
#include "svn_io.h"
#include "svn_subst.h"
#include "svn_pools.h"
#include "svn_private_config.h"
#define SVN_SUBST__DEFAULT_EOL_STR "\n"
#define SVN_SUBST__SPECIAL_LINK_STR "link"
void
svn_subst_eol_style_from_value(svn_subst_eol_style_t *style,
const char **eol,
const char *value) {
if (value == NULL) {
*eol = NULL;
if (style)
*style = svn_subst_eol_style_none;
} else if (! strcmp("native", value)) {
*eol = APR_EOL_STR;
if (style)
*style = svn_subst_eol_style_native;
} else if (! strcmp("LF", value)) {
*eol = "\n";
if (style)
*style = svn_subst_eol_style_fixed;
} else if (! strcmp("CR", value)) {
*eol = "\r";
if (style)
*style = svn_subst_eol_style_fixed;
} else if (! strcmp("CRLF", value)) {
*eol = "\r\n";
if (style)
*style = svn_subst_eol_style_fixed;
} else {
*eol = NULL;
if (style)
*style = svn_subst_eol_style_unknown;
}
}
svn_boolean_t
svn_subst_translation_required(svn_subst_eol_style_t style,
const char *eol,
apr_hash_t *keywords,
svn_boolean_t special,
svn_boolean_t force_eol_check) {
return (special || keywords
|| (style != svn_subst_eol_style_none && force_eol_check)
|| (style == svn_subst_eol_style_native &&
strcmp(APR_EOL_STR, SVN_SUBST__DEFAULT_EOL_STR) != 0)
|| (style == svn_subst_eol_style_fixed &&
strcmp(APR_EOL_STR, eol) != 0));
}
svn_error_t *
svn_subst_translate_to_normal_form(const char *src,
const char *dst,
svn_subst_eol_style_t eol_style,
const char *eol_str,
svn_boolean_t always_repair_eols,
apr_hash_t *keywords,
svn_boolean_t special,
apr_pool_t *pool) {
if (eol_style == svn_subst_eol_style_native)
eol_str = SVN_SUBST__DEFAULT_EOL_STR;
else if (! (eol_style == svn_subst_eol_style_fixed
|| eol_style == svn_subst_eol_style_none))
return svn_error_create(SVN_ERR_IO_UNKNOWN_EOL, NULL, NULL);
return svn_subst_copy_and_translate3(src, dst, eol_str,
eol_style == svn_subst_eol_style_fixed
|| always_repair_eols,
keywords,
FALSE ,
special,
pool);
}
svn_error_t *
svn_subst_stream_translated_to_normal_form(svn_stream_t **stream,
svn_stream_t *source,
svn_subst_eol_style_t eol_style,
const char *eol_str,
svn_boolean_t always_repair_eols,
apr_hash_t *keywords,
apr_pool_t *pool) {
if (eol_style == svn_subst_eol_style_native)
eol_str = SVN_SUBST__DEFAULT_EOL_STR;
else if (! (eol_style == svn_subst_eol_style_fixed
|| eol_style == svn_subst_eol_style_none))
return svn_error_create(SVN_ERR_IO_UNKNOWN_EOL, NULL, NULL);
*stream = svn_subst_stream_translated(source, eol_str,
eol_style == svn_subst_eol_style_fixed
|| always_repair_eols,
keywords, FALSE, pool);
return SVN_NO_ERROR;
}
static svn_string_t *
keyword_printf(const char *fmt,
const char *rev,
const char *url,
apr_time_t date,
const char *author,
apr_pool_t *pool) {
svn_stringbuf_t *value = svn_stringbuf_ncreate("", 0, pool);
const char *cur;
int n;
for (;;) {
cur = fmt;
while (*cur != '\0' && *cur != '%')
cur++;
if ((n = cur - fmt) > 0)
svn_stringbuf_appendbytes(value, fmt, n);
if (*cur == '\0')
break;
switch (cur[1]) {
case 'a':
if (author)
svn_stringbuf_appendcstr(value, author);
break;
case 'b':
if (url) {
const char *base_name
= svn_path_uri_decode(svn_path_basename(url, pool), pool);
svn_stringbuf_appendcstr(value, base_name);
}
break;
case 'd':
if (date) {
apr_time_exp_t exploded_time;
const char *human;
apr_time_exp_gmt(&exploded_time, date);
human = apr_psprintf(pool, "%04d-%02d-%02d %02d:%02d:%02dZ",
exploded_time.tm_year + 1900,
exploded_time.tm_mon + 1,
exploded_time.tm_mday,
exploded_time.tm_hour,
exploded_time.tm_min,
exploded_time.tm_sec);
svn_stringbuf_appendcstr(value, human);
}
break;
case 'D':
if (date)
svn_stringbuf_appendcstr(value,
svn_time_to_human_cstring(date, pool));
break;
case 'r':
if (rev)
svn_stringbuf_appendcstr(value, rev);
break;
case 'u':
if (url)
svn_stringbuf_appendcstr(value, url);
break;
case '%':
svn_stringbuf_appendbytes(value, cur, 1);
break;
case '\0':
svn_stringbuf_appendbytes(value, cur, 1);
cur--;
break;
default:
svn_stringbuf_appendbytes(value, cur, 2);
break;
}
fmt = cur + 2;
}
return svn_string_create_from_buf(value, pool);
}
static apr_hash_t *
kwstruct_to_kwhash(const svn_subst_keywords_t *kwstruct,
apr_pool_t *pool) {
apr_hash_t *kwhash;
if (kwstruct == NULL)
return NULL;
kwhash = apr_hash_make(pool);
if (kwstruct->revision) {
apr_hash_set(kwhash, SVN_KEYWORD_REVISION_LONG,
APR_HASH_KEY_STRING, kwstruct->revision);
apr_hash_set(kwhash, SVN_KEYWORD_REVISION_MEDIUM,
APR_HASH_KEY_STRING, kwstruct->revision);
apr_hash_set(kwhash, SVN_KEYWORD_REVISION_SHORT,
APR_HASH_KEY_STRING, kwstruct->revision);
}
if (kwstruct->date) {
apr_hash_set(kwhash, SVN_KEYWORD_DATE_LONG,
APR_HASH_KEY_STRING, kwstruct->date);
apr_hash_set(kwhash, SVN_KEYWORD_DATE_SHORT,
APR_HASH_KEY_STRING, kwstruct->date);
}
if (kwstruct->author) {
apr_hash_set(kwhash, SVN_KEYWORD_AUTHOR_LONG,
APR_HASH_KEY_STRING, kwstruct->author);
apr_hash_set(kwhash, SVN_KEYWORD_AUTHOR_SHORT,
APR_HASH_KEY_STRING, kwstruct->author);
}
if (kwstruct->url) {
apr_hash_set(kwhash, SVN_KEYWORD_URL_LONG,
APR_HASH_KEY_STRING, kwstruct->url);
apr_hash_set(kwhash, SVN_KEYWORD_URL_SHORT,
APR_HASH_KEY_STRING, kwstruct->url);
}
if (kwstruct->id) {
apr_hash_set(kwhash, SVN_KEYWORD_ID,
APR_HASH_KEY_STRING, kwstruct->id);
}
return kwhash;
}
svn_error_t *
svn_subst_build_keywords(svn_subst_keywords_t *kw,
const char *keywords_val,
const char *rev,
const char *url,
apr_time_t date,
const char *author,
apr_pool_t *pool) {
apr_hash_t *kwhash;
const svn_string_t *val;
SVN_ERR(svn_subst_build_keywords2(&kwhash, keywords_val, rev,
url, date, author, pool));
val = apr_hash_get(kwhash, SVN_KEYWORD_REVISION_LONG, APR_HASH_KEY_STRING);
if (val)
kw->revision = val;
val = apr_hash_get(kwhash, SVN_KEYWORD_DATE_LONG, APR_HASH_KEY_STRING);
if (val)
kw->date = val;
val = apr_hash_get(kwhash, SVN_KEYWORD_AUTHOR_LONG, APR_HASH_KEY_STRING);
if (val)
kw->author = val;
val = apr_hash_get(kwhash, SVN_KEYWORD_URL_LONG, APR_HASH_KEY_STRING);
if (val)
kw->url = val;
val = apr_hash_get(kwhash, SVN_KEYWORD_ID, APR_HASH_KEY_STRING);
if (val)
kw->id = val;
return SVN_NO_ERROR;
}
svn_error_t *
svn_subst_build_keywords2(apr_hash_t **kw,
const char *keywords_val,
const char *rev,
const char *url,
apr_time_t date,
const char *author,
apr_pool_t *pool) {
apr_array_header_t *keyword_tokens;
int i;
*kw = apr_hash_make(pool);
keyword_tokens = svn_cstring_split(keywords_val, " \t\v\n\b\r\f",
TRUE , pool);
for (i = 0; i < keyword_tokens->nelts; ++i) {
const char *keyword = APR_ARRAY_IDX(keyword_tokens, i, const char *);
if ((! strcmp(keyword, SVN_KEYWORD_REVISION_LONG))
|| (! strcmp(keyword, SVN_KEYWORD_REVISION_MEDIUM))
|| (! svn_cstring_casecmp(keyword, SVN_KEYWORD_REVISION_SHORT))) {
svn_string_t *revision_val;
revision_val = keyword_printf("%r", rev, url, date, author, pool);
apr_hash_set(*kw, SVN_KEYWORD_REVISION_LONG,
APR_HASH_KEY_STRING, revision_val);
apr_hash_set(*kw, SVN_KEYWORD_REVISION_MEDIUM,
APR_HASH_KEY_STRING, revision_val);
apr_hash_set(*kw, SVN_KEYWORD_REVISION_SHORT,
APR_HASH_KEY_STRING, revision_val);
} else if ((! strcmp(keyword, SVN_KEYWORD_DATE_LONG))
|| (! svn_cstring_casecmp(keyword, SVN_KEYWORD_DATE_SHORT))) {
svn_string_t *date_val;
date_val = keyword_printf("%D", rev, url, date, author, pool);
apr_hash_set(*kw, SVN_KEYWORD_DATE_LONG,
APR_HASH_KEY_STRING, date_val);
apr_hash_set(*kw, SVN_KEYWORD_DATE_SHORT,
APR_HASH_KEY_STRING, date_val);
} else if ((! strcmp(keyword, SVN_KEYWORD_AUTHOR_LONG))
|| (! svn_cstring_casecmp(keyword, SVN_KEYWORD_AUTHOR_SHORT))) {
svn_string_t *author_val;
author_val = keyword_printf("%a", rev, url, date, author, pool);
apr_hash_set(*kw, SVN_KEYWORD_AUTHOR_LONG,
APR_HASH_KEY_STRING, author_val);
apr_hash_set(*kw, SVN_KEYWORD_AUTHOR_SHORT,
APR_HASH_KEY_STRING, author_val);
} else if ((! strcmp(keyword, SVN_KEYWORD_URL_LONG))
|| (! svn_cstring_casecmp(keyword, SVN_KEYWORD_URL_SHORT))) {
svn_string_t *url_val;
url_val = keyword_printf("%u", rev, url, date, author, pool);
apr_hash_set(*kw, SVN_KEYWORD_URL_LONG,
APR_HASH_KEY_STRING, url_val);
apr_hash_set(*kw, SVN_KEYWORD_URL_SHORT,
APR_HASH_KEY_STRING, url_val);
} else if ((! svn_cstring_casecmp(keyword, SVN_KEYWORD_ID))) {
svn_string_t *id_val;
id_val = keyword_printf("%b %r %d %a", rev, url, date, author,
pool);
apr_hash_set(*kw, SVN_KEYWORD_ID,
APR_HASH_KEY_STRING, id_val);
}
}
return SVN_NO_ERROR;
}
static svn_error_t *
translate_write(svn_stream_t *stream,
const void *buf,
apr_size_t len) {
apr_size_t wrote = len;
svn_error_t *write_err = svn_stream_write(stream, buf, &wrote);
if ((write_err) || (len != wrote))
return write_err;
return SVN_NO_ERROR;
}
static svn_boolean_t
translate_keyword_subst(char *buf,
apr_size_t *len,
const char *keyword,
apr_size_t keyword_len,
const svn_string_t *value) {
char *buf_ptr;
assert(*len <= SVN_KEYWORD_MAX_LEN);
assert((buf[0] == '$') && (buf[*len - 1] == '$'));
if (*len < keyword_len + 2)
return FALSE;
if (strncmp(buf + 1, keyword, keyword_len))
return FALSE;
buf_ptr = buf + 1 + keyword_len;
if ((buf_ptr[0] == ':')
&& (buf_ptr[1] == ':')
&& (buf_ptr[2] == ' ')
&& ((buf[*len - 2] == ' ')
|| (buf[*len - 2] == '#'))
&& ((6 + keyword_len) < *len)) {
apr_size_t max_value_len = *len - (6 + keyword_len);
if (! value) {
buf_ptr += 2;
while (*buf_ptr != '$')
*(buf_ptr++) = ' ';
} else {
if (value->len <= max_value_len) {
strncpy(buf_ptr + 3, value->data, value->len);
buf_ptr += 3 + value->len;
while (*buf_ptr != '$')
*(buf_ptr++) = ' ';
} else {
strncpy(buf_ptr + 3, value->data, max_value_len);
buf[*len - 2] = '#';
buf[*len - 1] = '$';
}
}
return TRUE;
}
else if (buf_ptr[0] == '$') {
if (value) {
buf_ptr[0] = ':';
buf_ptr[1] = ' ';
if (value->len) {
apr_size_t vallen = value->len;
if (vallen > (SVN_KEYWORD_MAX_LEN - 5 - keyword_len))
vallen = SVN_KEYWORD_MAX_LEN - 5 - keyword_len;
strncpy(buf_ptr + 2, value->data, vallen);
buf_ptr[2 + vallen] = ' ';
buf_ptr[2 + vallen + 1] = '$';
*len = 5 + keyword_len + vallen;
} else {
buf_ptr[2] = '$';
*len = 4 + keyword_len;
}
} else {
}
return TRUE;
}
else if (((*len >= 4 + keyword_len )
&& (buf_ptr[0] == ':')
&& (buf_ptr[1] == ' ')
&& (buf[*len - 2] == ' '))
|| ((*len >= 3 + keyword_len )
&& (buf_ptr[0] == ':')
&& (buf_ptr[1] == '$'))) {
if (! value) {
buf_ptr[0] = '$';
*len = 2 + keyword_len;
} else {
buf_ptr[0] = ':';
buf_ptr[1] = ' ';
if (value->len) {
apr_size_t vallen = value->len;
if (vallen > (SVN_KEYWORD_MAX_LEN - 5))
vallen = SVN_KEYWORD_MAX_LEN - 5;
strncpy(buf_ptr + 2, value->data, vallen);
buf_ptr[2 + vallen] = ' ';
buf_ptr[2 + vallen + 1] = '$';
*len = 5 + keyword_len + vallen;
} else {
buf_ptr[2] = '$';
*len = 4 + keyword_len;
}
}
return TRUE;
}
return FALSE;
}
static svn_boolean_t
match_keyword(char *buf,
apr_size_t len,
char *keyword_name,
apr_hash_t *keywords) {
apr_size_t i;
if (! keywords)
return FALSE;
for (i = 0; i < len - 2 && buf[i + 1] != ':'; i++)
keyword_name[i] = buf[i + 1];
keyword_name[i] = '\0';
return apr_hash_get(keywords, keyword_name, APR_HASH_KEY_STRING) != NULL;
}
static svn_boolean_t
translate_keyword(char *buf,
apr_size_t *len,
const char *keyword_name,
svn_boolean_t expand,
apr_hash_t *keywords) {
const svn_string_t *value;
assert(*len <= SVN_KEYWORD_MAX_LEN);
assert((buf[0] == '$') && (buf[*len - 1] == '$'));
if (! keywords)
return FALSE;
value = apr_hash_get(keywords, keyword_name, APR_HASH_KEY_STRING);
if (value) {
return translate_keyword_subst(buf, len,
keyword_name, strlen(keyword_name),
expand ? value : NULL);
}
return FALSE;
}
static svn_error_t *
translate_newline(const char *eol_str,
apr_size_t eol_str_len,
char *src_format,
apr_size_t *src_format_len,
char *newline_buf,
apr_size_t newline_len,
svn_stream_t *dst,
svn_boolean_t repair) {
if (*src_format_len) {
if ((! repair) &&
((*src_format_len != newline_len) ||
(strncmp(src_format, newline_buf, newline_len))))
return svn_error_create(SVN_ERR_IO_INCONSISTENT_EOL, NULL, NULL);
} else {
strncpy(src_format, newline_buf, newline_len);
*src_format_len = newline_len;
}
return translate_write(dst, eol_str, eol_str_len);
}
svn_boolean_t
svn_subst_keywords_differ(const svn_subst_keywords_t *a,
const svn_subst_keywords_t *b,
svn_boolean_t compare_values) {
if (((a == NULL) && (b == NULL))
|| ((a == NULL)
&& (b->revision == NULL)
&& (b->date == NULL)
&& (b->author == NULL)
&& (b->url == NULL))
|| ((b == NULL) && (a->revision == NULL)
&& (a->date == NULL)
&& (a->author == NULL)
&& (a->url == NULL))
|| ((a != NULL) && (b != NULL)
&& (b->revision == NULL)
&& (b->date == NULL)
&& (b->author == NULL)
&& (b->url == NULL)
&& (a->revision == NULL)
&& (a->date == NULL)
&& (a->author == NULL)
&& (a->url == NULL))) {
return FALSE;
} else if ((a == NULL) || (b == NULL))
return TRUE;
if ((! a->revision) != (! b->revision))
return TRUE;
else if ((compare_values && (a->revision != NULL))
&& (strcmp(a->revision->data, b->revision->data) != 0))
return TRUE;
if ((! a->date) != (! b->date))
return TRUE;
else if ((compare_values && (a->date != NULL))
&& (strcmp(a->date->data, b->date->data) != 0))
return TRUE;
if ((! a->author) != (! b->author))
return TRUE;
else if ((compare_values && (a->author != NULL))
&& (strcmp(a->author->data, b->author->data) != 0))
return TRUE;
if ((! a->url) != (! b->url))
return TRUE;
else if ((compare_values && (a->url != NULL))
&& (strcmp(a->url->data, b->url->data) != 0))
return TRUE;
return FALSE;
}
svn_boolean_t
svn_subst_keywords_differ2(apr_hash_t *a,
apr_hash_t *b,
svn_boolean_t compare_values,
apr_pool_t *pool) {
apr_hash_index_t *hi;
unsigned int a_count, b_count;
a_count = (a == NULL) ? 0 : apr_hash_count(a);
b_count = (b == NULL) ? 0 : apr_hash_count(b);
if (a_count != b_count)
return TRUE;
if (a_count == 0)
return FALSE;
for (hi = apr_hash_first(pool, a); hi; hi = apr_hash_next(hi)) {
const void *key;
apr_ssize_t klen;
void *void_a_val;
svn_string_t *a_val, *b_val;
apr_hash_this(hi, &key, &klen, &void_a_val);
a_val = void_a_val;
b_val = apr_hash_get(b, key, klen);
if (!b_val || (compare_values && !svn_string_compare(a_val, b_val)))
return TRUE;
}
return FALSE;
}
svn_error_t *
svn_subst_translate_stream2(svn_stream_t *s,
svn_stream_t *d,
const char *eol_str,
svn_boolean_t repair,
const svn_subst_keywords_t *keywords,
svn_boolean_t expand,
apr_pool_t *pool) {
apr_hash_t *kh = kwstruct_to_kwhash(keywords, pool);
return svn_subst_translate_stream3(s, d, eol_str, repair, kh, expand, pool);
}
struct translation_baton {
const char *eol_str;
svn_boolean_t repair;
apr_hash_t *keywords;
svn_boolean_t expand;
const char *interesting;
apr_size_t eol_str_len;
char newline_buf[2];
apr_size_t newline_off;
char keyword_buf[SVN_KEYWORD_MAX_LEN];
apr_size_t keyword_off;
char src_format[2];
apr_size_t src_format_len;
};
static struct translation_baton *
create_translation_baton(const char *eol_str,
svn_boolean_t repair,
apr_hash_t *keywords,
svn_boolean_t expand,
apr_pool_t *pool) {
struct translation_baton *b = apr_palloc(pool, sizeof(*b));
if (keywords && (apr_hash_count(keywords) == 0))
keywords = NULL;
b->eol_str = eol_str;
b->eol_str_len = eol_str ? strlen(eol_str) : 0;
b->repair = repair;
b->keywords = keywords;
b->expand = expand;
b->interesting = (eol_str && keywords) ? "$\r\n" : eol_str ? "\r\n" : "$";
b->newline_off = 0;
b->keyword_off = 0;
b->src_format_len = 0;
return b;
}
static svn_error_t *
translate_chunk(svn_stream_t *dst,
struct translation_baton *b,
const char *buf,
apr_size_t buflen,
apr_pool_t *pool) {
const char *p;
apr_size_t len;
if (buf) {
const char *end = buf + buflen;
const char *interesting = b->interesting;
apr_size_t next_sign_off = 0;
for (p = buf; p < end;) {
if (b->newline_off) {
if (*p == '\n')
b->newline_buf[b->newline_off++] = *p++;
SVN_ERR(translate_newline(b->eol_str, b->eol_str_len,
b->src_format,
&b->src_format_len, b->newline_buf,
b->newline_off, dst, b->repair));
b->newline_off = 0;
} else if (b->keyword_off && *p == '$') {
svn_boolean_t keyword_matches;
char keyword_name[SVN_KEYWORD_MAX_LEN + 1];
b->keyword_buf[b->keyword_off++] = *p++;
keyword_matches = match_keyword(b->keyword_buf, b->keyword_off,
keyword_name, b->keywords);
if (keyword_matches == FALSE) {
p--;
b->keyword_off--;
}
if (keyword_matches == FALSE ||
translate_keyword(b->keyword_buf, &b->keyword_off,
keyword_name, b->expand, b->keywords) ||
b->keyword_off >= SVN_KEYWORD_MAX_LEN) {
SVN_ERR(translate_write(dst, b->keyword_buf, b->keyword_off));
next_sign_off = 0;
b->keyword_off = 0;
} else {
if (next_sign_off == 0)
next_sign_off = b->keyword_off - 1;
continue;
}
} else if (b->keyword_off == SVN_KEYWORD_MAX_LEN - 1
|| (b->keyword_off && (*p == '\r' || *p == '\n'))) {
if (next_sign_off > 0) {
p -= (b->keyword_off - next_sign_off);
b->keyword_off = next_sign_off;
next_sign_off = 0;
}
SVN_ERR(translate_write(dst, b->keyword_buf, b->keyword_off));
b->keyword_off = 0;
} else if (b->keyword_off) {
b->keyword_buf[b->keyword_off++] = *p++;
continue;
}
len = 0;
while ((p + len) < end
&& (! p[len] || ! strchr(interesting, p[len])))
len++;
if (len)
SVN_ERR(translate_write(dst, p, len));
p += len;
if (p < end) {
switch (*p) {
case '$':
b->keyword_buf[b->keyword_off++] = *p++;
break;
case '\r':
b->newline_buf[b->newline_off++] = *p++;
break;
case '\n':
b->newline_buf[b->newline_off++] = *p++;
SVN_ERR(translate_newline(b->eol_str, b->eol_str_len,
b->src_format,
&b->src_format_len,
b->newline_buf,
b->newline_off, dst, b->repair));
b->newline_off = 0;
break;
}
}
}
} else {
if (b->newline_off) {
SVN_ERR(translate_newline(b->eol_str, b->eol_str_len,
b->src_format, &b->src_format_len,
b->newline_buf, b->newline_off,
dst, b->repair));
b->newline_off = 0;
}
if (b->keyword_off) {
SVN_ERR(translate_write(dst, b->keyword_buf, b->keyword_off));
b->keyword_off = 0;
}
}
return SVN_NO_ERROR;
}
struct translated_stream_baton {
svn_stream_t *stream;
struct translation_baton *in_baton, *out_baton;
svn_boolean_t written;
svn_stringbuf_t *readbuf;
apr_size_t readbuf_off;
char *buf;
apr_pool_t *pool;
apr_pool_t *iterpool;
};
static svn_error_t *
translated_stream_read(void *baton,
char *buffer,
apr_size_t *len) {
struct translated_stream_baton *b = baton;
apr_size_t readlen = SVN__STREAM_CHUNK_SIZE;
apr_size_t unsatisfied = *len;
apr_size_t off = 0;
apr_pool_t *iterpool;
iterpool = b->iterpool;
while (readlen == SVN__STREAM_CHUNK_SIZE && unsatisfied > 0) {
apr_size_t to_copy;
apr_size_t buffer_remainder;
svn_pool_clear(iterpool);
if (! (b->readbuf_off < b->readbuf->len)) {
svn_stream_t *buf_stream;
svn_stringbuf_setempty(b->readbuf);
b->readbuf_off = 0;
SVN_ERR(svn_stream_read(b->stream, b->buf, &readlen));
buf_stream = svn_stream_from_stringbuf(b->readbuf, iterpool);
SVN_ERR(translate_chunk(buf_stream, b->in_baton, b->buf,
readlen, iterpool));
if (readlen != SVN__STREAM_CHUNK_SIZE)
SVN_ERR(translate_chunk(buf_stream, b->in_baton, NULL, 0,
iterpool));
SVN_ERR(svn_stream_close(buf_stream));
}
buffer_remainder = b->readbuf->len - b->readbuf_off;
to_copy = (buffer_remainder > unsatisfied)
? unsatisfied : buffer_remainder;
memcpy(buffer + off, b->readbuf->data + b->readbuf_off, to_copy);
off += to_copy;
b->readbuf_off += to_copy;
unsatisfied -= to_copy;
}
*len -= unsatisfied;
return SVN_NO_ERROR;
}
static svn_error_t *
translated_stream_write(void *baton,
const char *buffer,
apr_size_t *len) {
struct translated_stream_baton *b = baton;
svn_pool_clear(b->iterpool);
b->written = TRUE;
SVN_ERR(translate_chunk(b->stream, b->out_baton, buffer, *len,
b->iterpool));
return SVN_NO_ERROR;
}
static svn_error_t *
translated_stream_close(void *baton) {
struct translated_stream_baton *b = baton;
if (b->written)
SVN_ERR(translate_chunk(b->stream, b->out_baton, NULL, 0, b->iterpool));
SVN_ERR(svn_stream_close(b->stream));
svn_pool_destroy(b->pool);
return SVN_NO_ERROR;
}
static svn_error_t *
detranslated_stream_special(svn_stream_t **translated_stream_p,
const char *src,
apr_pool_t *pool) {
apr_finfo_t finfo;
apr_file_t *s;
svn_string_t *buf;
svn_stringbuf_t *strbuf;
SVN_ERR(svn_io_stat(&finfo, src, APR_FINFO_MIN | APR_FINFO_LINK, pool));
switch (finfo.filetype) {
case APR_REG:
SVN_ERR(svn_io_file_open(&s, src, APR_READ | APR_BUFFERED,
APR_OS_DEFAULT, pool));
*translated_stream_p = svn_stream_from_aprfile2(s, FALSE, pool);
break;
case APR_LNK:
SVN_ERR(svn_io_read_link(&buf, src, pool));
strbuf = svn_stringbuf_createf(pool, "link %s", buf->data);
*translated_stream_p = svn_stream_from_stringbuf(strbuf, pool);
break;
default:
abort();
}
return SVN_NO_ERROR;
}
svn_error_t *
svn_subst_stream_detranslated(svn_stream_t **stream_p,
const char *src,
svn_subst_eol_style_t eol_style,
const char *eol_str,
svn_boolean_t always_repair_eols,
apr_hash_t *keywords,
svn_boolean_t special,
apr_pool_t *pool) {
apr_file_t *file_h;
svn_stream_t *src_stream;
if (special)
return detranslated_stream_special(stream_p, src, pool);
if (eol_style == svn_subst_eol_style_native)
eol_str = SVN_SUBST__DEFAULT_EOL_STR;
else if (! (eol_style == svn_subst_eol_style_fixed
|| eol_style == svn_subst_eol_style_none))
return svn_error_create(SVN_ERR_IO_UNKNOWN_EOL, NULL, NULL);
SVN_ERR(svn_io_file_open(&file_h, src, APR_READ,
APR_OS_DEFAULT, pool));
src_stream = svn_stream_from_aprfile2(file_h, FALSE, pool);
*stream_p = svn_subst_stream_translated(
src_stream, eol_str,
eol_style == svn_subst_eol_style_fixed || always_repair_eols,
keywords, FALSE, pool);
return SVN_NO_ERROR;
}
svn_stream_t *
svn_subst_stream_translated(svn_stream_t *stream,
const char *eol_str,
svn_boolean_t repair,
apr_hash_t *keywords,
svn_boolean_t expand,
apr_pool_t *pool) {
apr_pool_t *baton_pool = svn_pool_create(pool);
struct translated_stream_baton *baton
= apr_palloc(baton_pool, sizeof(*baton));
svn_stream_t *s = svn_stream_create(baton, baton_pool);
if (eol_str)
eol_str = apr_pstrdup(baton_pool, eol_str);
if (keywords) {
if (apr_hash_count(keywords) == 0)
keywords = NULL;
else {
apr_hash_t *copy = apr_hash_make(baton_pool);
apr_hash_index_t *hi;
for (hi = apr_hash_first(pool, keywords);
hi; hi = apr_hash_next(hi)) {
const void *key;
void *val;
apr_hash_this(hi, &key, NULL, &val);
apr_hash_set(copy, apr_pstrdup(baton_pool, key),
APR_HASH_KEY_STRING,
svn_string_dup(val, baton_pool));
}
keywords = copy;
}
}
baton->stream = stream;
baton->in_baton
= create_translation_baton(eol_str, repair, keywords, expand, baton_pool);
baton->out_baton
= create_translation_baton(eol_str, repair, keywords, expand, baton_pool);
baton->written = FALSE;
baton->readbuf = svn_stringbuf_create("", baton_pool);
baton->readbuf_off = 0;
baton->iterpool = svn_pool_create(baton_pool);
baton->pool = baton_pool;
baton->buf = apr_palloc(baton->pool, SVN__STREAM_CHUNK_SIZE + 1);
svn_stream_set_read(s, translated_stream_read);
svn_stream_set_write(s, translated_stream_write);
svn_stream_set_close(s, translated_stream_close);
return s;
}
svn_error_t *
svn_subst_translate_stream3(svn_stream_t *s,
svn_stream_t *d,
const char *eol_str,
svn_boolean_t repair,
apr_hash_t *keywords,
svn_boolean_t expand,
apr_pool_t *pool) {
apr_pool_t *subpool = svn_pool_create(pool);
apr_pool_t *iterpool = svn_pool_create(subpool);
struct translation_baton *baton;
apr_size_t readlen = SVN__STREAM_CHUNK_SIZE;
char *buf = apr_palloc(subpool, SVN__STREAM_CHUNK_SIZE);
assert(eol_str || keywords);
baton = create_translation_baton(eol_str, repair, keywords, expand, pool);
while (readlen == SVN__STREAM_CHUNK_SIZE) {
svn_pool_clear(iterpool);
SVN_ERR(svn_stream_read(s, buf, &readlen));
SVN_ERR(translate_chunk(d, baton, buf, readlen, iterpool));
}
SVN_ERR(translate_chunk(d, baton, NULL, 0, iterpool));
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
svn_error_t *
svn_subst_translate_stream(svn_stream_t *s,
svn_stream_t *d,
const char *eol_str,
svn_boolean_t repair,
const svn_subst_keywords_t *keywords,
svn_boolean_t expand) {
apr_pool_t *pool = svn_pool_create(NULL);
svn_error_t *err = svn_subst_translate_stream2(s, d, eol_str, repair,
keywords, expand, pool);
svn_pool_destroy(pool);
return err;
}
svn_error_t *
svn_subst_translate_cstring(const char *src,
const char **dst,
const char *eol_str,
svn_boolean_t repair,
const svn_subst_keywords_t *keywords,
svn_boolean_t expand,
apr_pool_t *pool) {
apr_hash_t *kh = kwstruct_to_kwhash(keywords, pool);
return svn_subst_translate_cstring2(src, dst, eol_str, repair,
kh, expand, pool);
}
svn_error_t *
svn_subst_translate_cstring2(const char *src,
const char **dst,
const char *eol_str,
svn_boolean_t repair,
apr_hash_t *keywords,
svn_boolean_t expand,
apr_pool_t *pool) {
svn_stringbuf_t *src_stringbuf, *dst_stringbuf;
svn_stream_t *src_stream, *dst_stream;
svn_error_t *err;
src_stringbuf = svn_stringbuf_create(src, pool);
if (! (eol_str || (keywords && (apr_hash_count(keywords) > 0)))) {
dst_stringbuf = svn_stringbuf_dup(src_stringbuf, pool);
goto all_good;
}
src_stream = svn_stream_from_stringbuf(src_stringbuf, pool);
dst_stringbuf = svn_stringbuf_create("", pool);
dst_stream = svn_stream_from_stringbuf(dst_stringbuf, pool);
err = svn_subst_translate_stream3(src_stream, dst_stream,
eol_str, repair, keywords, expand, pool);
if (err) {
svn_error_clear(svn_stream_close(src_stream));
svn_error_clear(svn_stream_close(dst_stream));
return err;
}
SVN_ERR(svn_stream_close(src_stream));
SVN_ERR(svn_stream_close(dst_stream));
all_good:
*dst = dst_stringbuf->data;
return SVN_NO_ERROR;
}
svn_error_t *
svn_subst_copy_and_translate(const char *src,
const char *dst,
const char *eol_str,
svn_boolean_t repair,
const svn_subst_keywords_t *keywords,
svn_boolean_t expand,
apr_pool_t *pool) {
return svn_subst_copy_and_translate2(src, dst, eol_str, repair, keywords,
expand, FALSE, pool);
}
static svn_error_t *
detranslate_special_file_to_stream(svn_stream_t **src_stream,
const char *src,
apr_pool_t *pool) {
apr_finfo_t finfo;
apr_file_t *s;
svn_string_t *buf;
SVN_ERR(svn_io_stat(&finfo, src, APR_FINFO_MIN | APR_FINFO_LINK, pool));
switch (finfo.filetype) {
case APR_REG:
SVN_ERR(svn_io_file_open(&s, src, APR_READ | APR_BUFFERED,
APR_OS_DEFAULT, pool));
*src_stream = svn_stream_from_aprfile(s, pool);
break;
case APR_LNK:
*src_stream = svn_stream_from_stringbuf(svn_stringbuf_create ("", pool),
pool);
SVN_ERR(svn_io_read_link(&buf, src, pool));
SVN_ERR(svn_stream_printf(*src_stream, pool, "link %s",
buf->data));
break;
default:
abort ();
}
return SVN_NO_ERROR;
}
static svn_error_t *
detranslate_special_file(const char *src, const char *dst, apr_pool_t *pool) {
const char *dst_tmp;
apr_file_t *d;
svn_stream_t *src_stream, *dst_stream;
SVN_ERR(svn_io_open_unique_file2(&d, &dst_tmp, dst,
".tmp", svn_io_file_del_none, pool));
dst_stream = svn_stream_from_aprfile2(d, FALSE, pool);
SVN_ERR(detranslate_special_file_to_stream(&src_stream, src, pool));
SVN_ERR(svn_stream_copy(src_stream, dst_stream, pool));
SVN_ERR(svn_stream_close(dst_stream));
SVN_ERR(svn_io_file_rename(dst_tmp, dst, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
create_special_file_from_stringbuf(svn_stringbuf_t *src, const char *dst,
apr_pool_t *pool) {
char *identifier, *remainder;
const char *dst_tmp;
svn_boolean_t create_using_internal_representation = FALSE;
identifier = src->data;
for (remainder = identifier; *remainder; remainder++) {
if (*remainder == ' ') {
remainder++;
break;
}
}
if (! strncmp(identifier, SVN_SUBST__SPECIAL_LINK_STR " ",
strlen(SVN_SUBST__SPECIAL_LINK_STR " "))) {
svn_error_t *err = svn_io_create_unique_link(&dst_tmp, dst, remainder,
".tmp", pool);
if (err) {
if (err->apr_err == SVN_ERR_UNSUPPORTED_FEATURE) {
svn_error_clear(err);
create_using_internal_representation = TRUE;
} else
return err;
}
} else {
create_using_internal_representation = TRUE;
}
if (create_using_internal_representation) {
apr_file_t *dst_tmp_file;
apr_size_t written;
SVN_ERR(svn_io_open_unique_file2(&dst_tmp_file, &dst_tmp,
dst, ".tmp", svn_io_file_del_none,
pool));
SVN_ERR(svn_io_file_write_full(dst_tmp_file, src->data, src->len,
&written, pool));
SVN_ERR(svn_io_file_close(dst_tmp_file, pool));
}
return svn_io_file_rename(dst_tmp, dst, pool);
}
static svn_error_t *
create_special_file(const char *src, const char *dst, apr_pool_t *pool) {
svn_stringbuf_t *contents;
svn_node_kind_t kind;
svn_boolean_t is_special;
svn_stream_t *source;
SVN_ERR(svn_io_check_special_path(src, &kind, &is_special, pool));
if (is_special) {
svn_boolean_t eof;
SVN_ERR(detranslate_special_file_to_stream(&source, src, pool));
SVN_ERR(svn_stream_readline(source, &contents, "\n", &eof, pool));
} else
SVN_ERR(svn_stringbuf_from_file(&contents, src, pool));
return create_special_file_from_stringbuf(contents, dst, pool);
}
svn_error_t *
svn_subst_copy_and_translate2(const char *src,
const char *dst,
const char *eol_str,
svn_boolean_t repair,
const svn_subst_keywords_t *keywords,
svn_boolean_t expand,
svn_boolean_t special,
apr_pool_t *pool) {
apr_hash_t *kh = kwstruct_to_kwhash(keywords, pool);
return svn_subst_copy_and_translate3(src, dst, eol_str,
repair, kh, expand, special,
pool);
}
svn_error_t *
svn_subst_copy_and_translate3(const char *src,
const char *dst,
const char *eol_str,
svn_boolean_t repair,
apr_hash_t *keywords,
svn_boolean_t expand,
svn_boolean_t special,
apr_pool_t *pool) {
const char *dst_tmp = NULL;
svn_stream_t *src_stream, *dst_stream;
apr_file_t *s = NULL, *d = NULL;
svn_error_t *err;
svn_node_kind_t kind;
svn_boolean_t path_special;
SVN_ERR(svn_io_check_special_path(src, &kind, &path_special, pool));
if (special || path_special) {
if (expand)
SVN_ERR(create_special_file(src, dst, pool));
else
SVN_ERR(detranslate_special_file(src, dst, pool));
return SVN_NO_ERROR;
}
if (! (eol_str || (keywords && (apr_hash_count(keywords) > 0))))
return svn_io_copy_file(src, dst, FALSE, pool);
SVN_ERR(svn_io_file_open(&s, src, APR_READ | APR_BUFFERED,
APR_OS_DEFAULT, pool));
SVN_ERR(svn_io_open_unique_file2(&d, &dst_tmp, dst,
".tmp", svn_io_file_del_on_pool_cleanup,
pool));
src_stream = svn_stream_from_aprfile(s, pool);
dst_stream = svn_stream_from_aprfile(d, pool);
err = svn_subst_translate_stream3(src_stream, dst_stream, eol_str,
repair, keywords, expand, pool);
if (err) {
if (err->apr_err == SVN_ERR_IO_INCONSISTENT_EOL)
return svn_error_createf
(SVN_ERR_IO_INCONSISTENT_EOL, err,
_("File '%s' has inconsistent newlines"),
svn_path_local_style(src, pool));
else
return err;
}
SVN_ERR(svn_stream_close(src_stream));
SVN_ERR(svn_stream_close(dst_stream));
SVN_ERR(svn_io_file_close(s, pool));
SVN_ERR(svn_io_file_close(d, pool));
SVN_ERR(svn_io_file_rename(dst_tmp, dst, pool));
return SVN_NO_ERROR;
}
struct special_stream_baton {
svn_stream_t *read_stream;
svn_stringbuf_t *write_content;
svn_stream_t *write_stream;
const char *path;
apr_pool_t *pool;
};
static svn_error_t *
read_handler_special(void *baton, char *buffer, apr_size_t *len) {
struct special_stream_baton *btn = baton;
if (btn->read_stream)
return svn_stream_read(btn->read_stream, buffer, len);
else
return svn_error_createf(APR_ENOENT, NULL,
"Can't read special file: File '%s' not found",
svn_path_local_style (btn->path, btn->pool));
}
static svn_error_t *
write_handler_special(void *baton, const char *buffer, apr_size_t *len) {
struct special_stream_baton *btn = baton;
return svn_stream_write(btn->write_stream, buffer, len);
}
static svn_error_t *
close_handler_special (void *baton) {
struct special_stream_baton *btn = baton;
if (btn->write_content->len) {
SVN_ERR(create_special_file_from_stringbuf(btn->write_content,
btn->path,
btn->pool));
}
return SVN_NO_ERROR;
}
svn_error_t *
svn_subst_stream_from_specialfile(svn_stream_t **stream,
const char *path,
apr_pool_t *pool) {
struct special_stream_baton *baton = apr_palloc(pool, sizeof(*baton));
svn_error_t *err;
baton->pool = pool;
baton->path = apr_pstrdup(pool, path);
err = detranslate_special_file_to_stream(&baton->read_stream, path, pool);
if (err && APR_STATUS_IS_ENOENT(err->apr_err)) {
svn_error_clear(err);
baton->read_stream = NULL;
}
baton->write_content = svn_stringbuf_create("", pool);
baton->write_stream = svn_stream_from_stringbuf(baton->write_content, pool);
*stream = svn_stream_create(baton, pool);
svn_stream_set_read(*stream, read_handler_special);
svn_stream_set_write(*stream, write_handler_special);
svn_stream_set_close(*stream, close_handler_special);
return SVN_NO_ERROR;
}
svn_error_t *
svn_subst_translate_string(svn_string_t **new_value,
const svn_string_t *value,
const char *encoding,
apr_pool_t *pool) {
const char *val_utf8;
const char *val_utf8_lf;
if (value == NULL) {
*new_value = NULL;
return SVN_NO_ERROR;
}
if (encoding) {
SVN_ERR(svn_utf_cstring_to_utf8_ex2(&val_utf8, value->data,
encoding, pool));
} else {
SVN_ERR(svn_utf_cstring_to_utf8(&val_utf8, value->data, pool));
}
SVN_ERR(svn_subst_translate_cstring2(val_utf8,
&val_utf8_lf,
"\n",
FALSE,
NULL,
FALSE,
pool));
*new_value = svn_string_create(val_utf8_lf, pool);
return SVN_NO_ERROR;
}
svn_error_t *
svn_subst_detranslate_string(svn_string_t **new_value,
const svn_string_t *value,
svn_boolean_t for_output,
apr_pool_t *pool) {
svn_error_t *err;
const char *val_neol;
const char *val_nlocale_neol;
if (value == NULL) {
*new_value = NULL;
return SVN_NO_ERROR;
}
SVN_ERR(svn_subst_translate_cstring2(value->data,
&val_neol,
APR_EOL_STR,
FALSE,
NULL,
FALSE,
pool));
if (for_output) {
err = svn_cmdline_cstring_from_utf8(&val_nlocale_neol, val_neol, pool);
if (err && (APR_STATUS_IS_EINVAL(err->apr_err))) {
val_nlocale_neol =
svn_cmdline_cstring_from_utf8_fuzzy(val_neol, pool);
svn_error_clear(err);
} else if (err)
return err;
} else {
err = svn_utf_cstring_from_utf8(&val_nlocale_neol, val_neol, pool);
if (err && (APR_STATUS_IS_EINVAL(err->apr_err))) {
val_nlocale_neol = svn_utf_cstring_from_utf8_fuzzy(val_neol, pool);
svn_error_clear(err);
} else if (err)
return err;
}
*new_value = svn_string_create(val_nlocale_neol, pool);
return SVN_NO_ERROR;
}
