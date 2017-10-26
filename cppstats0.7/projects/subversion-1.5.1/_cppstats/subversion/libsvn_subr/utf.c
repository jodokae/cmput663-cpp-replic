#include <string.h>
#include <assert.h>
#include <apr_strings.h>
#include <apr_lib.h>
#include <apr_xlate.h>
#include "svn_string.h"
#include "svn_error.h"
#include "svn_pools.h"
#include "svn_ctype.h"
#include "svn_utf.h"
#include "utf_impl.h"
#include "svn_private_config.h"
#include "win32_xlate.h"
#define SVN_UTF_NTOU_XLATE_HANDLE "svn-utf-ntou-xlate-handle"
#define SVN_UTF_UTON_XLATE_HANDLE "svn-utf-uton-xlate-handle"
#if !defined(AS400)
#define SVN_APR_UTF8_CHARSET "UTF-8"
#else
#define SVN_APR_UTF8_CHARSET (const char*)1208
#endif
#if APR_HAS_THREADS
static apr_thread_mutex_t *xlate_handle_mutex = NULL;
#endif
typedef struct xlate_handle_node_t {
apr_xlate_t *handle;
svn_boolean_t valid;
const char *frompage, *topage;
struct xlate_handle_node_t *next;
} xlate_handle_node_t;
static apr_hash_t *xlate_handle_hash = NULL;
static apr_status_t
xlate_cleanup(void *arg) {
#if APR_HAS_THREADS
apr_thread_mutex_destroy(xlate_handle_mutex);
xlate_handle_mutex = NULL;
#endif
xlate_handle_hash = NULL;
return APR_SUCCESS;
}
static apr_status_t
xlate_handle_node_cleanup(void *arg) {
xlate_handle_node_t *node = arg;
node->valid = FALSE;
return APR_SUCCESS;
}
void
svn_utf_initialize(apr_pool_t *pool) {
apr_pool_t *subpool;
#if APR_HAS_THREADS
apr_thread_mutex_t *mutex;
#endif
if (!xlate_handle_hash) {
subpool = svn_pool_create(pool);
#if APR_HAS_THREADS
if (apr_thread_mutex_create(&mutex, APR_THREAD_MUTEX_DEFAULT, subpool)
== APR_SUCCESS)
xlate_handle_mutex = mutex;
else
return;
#endif
xlate_handle_hash = apr_hash_make(subpool);
apr_pool_cleanup_register(subpool, NULL, xlate_cleanup,
apr_pool_cleanup_null);
}
}
static const char*
get_xlate_key(const char *topage,
const char *frompage,
apr_pool_t *pool) {
#if !defined(AS400)
if (frompage == SVN_APR_LOCALE_CHARSET)
frompage = "APR_LOCALE_CHARSET";
else if (frompage == SVN_APR_DEFAULT_CHARSET)
frompage = "APR_DEFAULT_CHARSET";
if (topage == SVN_APR_LOCALE_CHARSET)
topage = "APR_LOCALE_CHARSET";
else if (topage == SVN_APR_DEFAULT_CHARSET)
topage = "APR_DEFAULT_CHARSET";
return apr_pstrcat(pool, "svn-utf-", frompage, "to", topage,
"-xlate-handle", NULL);
#else
return apr_psprintf(pool, "svn-utf-%dto%d-xlate-handle", (int)frompage,
(int)topage);
#endif
}
static svn_error_t *
get_xlate_handle_node(xlate_handle_node_t **ret,
const char *topage, const char *frompage,
const char *userdata_key, apr_pool_t *pool) {
xlate_handle_node_t **old_node_p;
xlate_handle_node_t *old_node = NULL;
apr_status_t apr_err;
apr_xlate_t *handle;
svn_error_t *err = NULL;
if (userdata_key) {
if (xlate_handle_hash) {
#if APR_HAS_THREADS
apr_err = apr_thread_mutex_lock(xlate_handle_mutex);
if (apr_err != APR_SUCCESS)
return svn_error_create(apr_err, NULL,
_("Can't lock charset translation mutex"));
#endif
old_node_p = apr_hash_get(xlate_handle_hash, userdata_key,
APR_HASH_KEY_STRING);
if (old_node_p)
old_node = *old_node_p;
if (old_node) {
if (old_node->valid) {
*old_node_p = old_node->next;
old_node->next = NULL;
#if APR_HAS_THREADS
apr_err = apr_thread_mutex_unlock(xlate_handle_mutex);
if (apr_err != APR_SUCCESS)
return svn_error_create(apr_err, NULL,
_("Can't unlock charset "
"translation mutex"));
#endif
*ret = old_node;
return SVN_NO_ERROR;
}
}
} else {
void *p;
apr_pool_userdata_get(&p, userdata_key, pool);
old_node = p;
if (old_node && old_node->valid) {
*ret = old_node;
return SVN_NO_ERROR;
}
}
}
#if !defined(AS400)
assert(frompage != SVN_APR_DEFAULT_CHARSET
&& topage != SVN_APR_DEFAULT_CHARSET
&& (frompage != SVN_APR_LOCALE_CHARSET
|| topage != SVN_APR_LOCALE_CHARSET));
#endif
if (userdata_key && xlate_handle_hash)
pool = apr_hash_pool_get(xlate_handle_hash);
#if defined( WIN32)
apr_err = svn_subr__win32_xlate_open((win32_xlate_t **)&handle, topage,
frompage, pool);
#elif defined(AS400)
apr_err = apr_xlate_open(&handle, (int)topage, (int)frompage, pool);
#else
apr_err = apr_xlate_open(&handle, topage, frompage, pool);
#endif
if (APR_STATUS_IS_EINVAL(apr_err) || APR_STATUS_IS_ENOTIMPL(apr_err))
handle = NULL;
else if (apr_err != APR_SUCCESS) {
const char *errstr;
#if !defined(AS400)
if (frompage == SVN_APR_LOCALE_CHARSET)
errstr = apr_psprintf(pool,
_("Can't create a character converter from "
"native encoding to '%s'"), topage);
else if (topage == SVN_APR_LOCALE_CHARSET)
errstr = apr_psprintf(pool,
_("Can't create a character converter from "
"'%s' to native encoding"), frompage);
else
errstr = apr_psprintf(pool,
_("Can't create a character converter from "
"'%s' to '%s'"), frompage, topage);
#else
errstr = apr_psprintf(pool,
_("Can't create a character converter from "
"'%i' to '%i'"), frompage, topage);
#endif
err = svn_error_create(apr_err, NULL, errstr);
goto cleanup;
}
*ret = apr_palloc(pool, sizeof(xlate_handle_node_t));
(*ret)->handle = handle;
(*ret)->valid = TRUE;
(*ret)->frompage = ((frompage != SVN_APR_LOCALE_CHARSET)
? apr_pstrdup(pool, frompage) : frompage);
(*ret)->topage = ((topage != SVN_APR_LOCALE_CHARSET)
? apr_pstrdup(pool, topage) : topage);
(*ret)->next = NULL;
if (handle)
apr_pool_cleanup_register(pool, *ret, xlate_handle_node_cleanup,
apr_pool_cleanup_null);
cleanup:
#if APR_HAS_THREADS
if (userdata_key && xlate_handle_hash) {
apr_status_t unlock_err = apr_thread_mutex_unlock(xlate_handle_mutex);
if (unlock_err != APR_SUCCESS)
return svn_error_create(unlock_err, NULL,
_("Can't unlock charset translation mutex"));
}
#endif
return err;
}
static void
put_xlate_handle_node(xlate_handle_node_t *node,
const char *userdata_key,
apr_pool_t *pool) {
assert(node->next == NULL);
if (!userdata_key)
return;
if (xlate_handle_hash) {
xlate_handle_node_t **node_p;
#if APR_HAS_THREADS
if (apr_thread_mutex_lock(xlate_handle_mutex) != APR_SUCCESS)
abort();
#endif
node_p = apr_hash_get(xlate_handle_hash, userdata_key,
APR_HASH_KEY_STRING);
if (node_p == NULL) {
userdata_key = apr_pstrdup(apr_hash_pool_get(xlate_handle_hash),
userdata_key);
node_p = apr_palloc(apr_hash_pool_get(xlate_handle_hash),
sizeof(*node_p));
*node_p = NULL;
apr_hash_set(xlate_handle_hash, userdata_key,
APR_HASH_KEY_STRING, node_p);
}
node->next = *node_p;
*node_p = node;
#if APR_HAS_THREADS
if (apr_thread_mutex_unlock(xlate_handle_mutex) != APR_SUCCESS)
abort();
#endif
} else {
apr_pool_userdata_set(node, userdata_key, apr_pool_cleanup_null, pool);
}
}
static svn_error_t *
get_ntou_xlate_handle_node(xlate_handle_node_t **ret, apr_pool_t *pool) {
return get_xlate_handle_node(ret, SVN_APR_UTF8_CHARSET,
SVN_APR_LOCALE_CHARSET,
SVN_UTF_NTOU_XLATE_HANDLE, pool);
}
static svn_error_t *
get_uton_xlate_handle_node(xlate_handle_node_t **ret, apr_pool_t *pool) {
return get_xlate_handle_node(ret, SVN_APR_LOCALE_CHARSET,
SVN_APR_UTF8_CHARSET,
SVN_UTF_UTON_XLATE_HANDLE, pool);
}
static const char *
fuzzy_escape(const char *src, apr_size_t len, apr_pool_t *pool) {
const char *src_orig = src, *src_end = src + len;
apr_size_t new_len = 0;
char *new;
const char *new_orig;
while (src < src_end) {
if (! svn_ctype_isascii(*src) || *src == '\0')
new_len += 5;
else
new_len += 1;
src++;
}
new = apr_palloc(pool, new_len + 1);
new_orig = new;
while (src_orig < src_end) {
if (! svn_ctype_isascii(*src_orig) || src_orig == '\0') {
sprintf(new, "?\\%03u", (unsigned char) *src_orig);
new += 5;
} else {
*new = *src_orig;
new += 1;
}
src_orig++;
}
*new = '\0';
return new_orig;
}
static svn_error_t *
convert_to_stringbuf(xlate_handle_node_t *node,
const char *src_data,
apr_size_t src_length,
svn_stringbuf_t **dest,
apr_pool_t *pool) {
#if defined(WIN32)
apr_status_t apr_err;
apr_err = svn_subr__win32_xlate_to_stringbuf((win32_xlate_t *) node->handle,
src_data, src_length,
dest, pool);
#else
apr_size_t buflen = src_length * 2;
apr_status_t apr_err;
apr_size_t srclen = src_length;
apr_size_t destlen = buflen;
char *destbuf;
*dest = svn_stringbuf_create("", pool);
destbuf = (*dest)->data;
if (src_length == 0)
return SVN_NO_ERROR;
do {
svn_stringbuf_ensure(*dest, buflen + 1);
destlen = buflen - (*dest)->len;
apr_err = apr_xlate_conv_buffer(node->handle,
src_data + (src_length - srclen),
&srclen,
(*dest)->data + (*dest)->len,
&destlen);
(*dest)->len += ((buflen - (*dest)->len) - destlen);
buflen += srclen * 3;
} while (apr_err == APR_SUCCESS && srclen != 0);
#endif
if (apr_err) {
const char *errstr;
svn_error_t *err;
#if !defined(AS400)
if (node->frompage == SVN_APR_LOCALE_CHARSET)
errstr = apr_psprintf
(pool, _("Can't convert string from native encoding to '%s':"),
node->topage);
else if (node->topage == SVN_APR_LOCALE_CHARSET)
errstr = apr_psprintf
(pool, _("Can't convert string from '%s' to native encoding:"),
node->frompage);
else
errstr = apr_psprintf
(pool, _("Can't convert string from '%s' to '%s':"),
node->frompage, node->topage);
#else
errstr = apr_psprintf
(pool, _("Can't convert string from CCSID '%i' to CCSID '%i'"),
node->frompage, node->topage);
#endif
err = svn_error_create(apr_err, NULL, fuzzy_escape(src_data,
src_length, pool));
return svn_error_create(apr_err, err, errstr);
}
(*dest)->data[(*dest)->len] = '\0';
return SVN_NO_ERROR;
}
static svn_error_t *
check_non_ascii(const char *data, apr_size_t len, apr_pool_t *pool) {
const char *data_start = data;
for (; len > 0; --len, data++) {
if ((! apr_isascii(*data))
|| ((! apr_isspace(*data))
&& apr_iscntrl(*data))) {
if (data - data_start) {
const char *error_data
= apr_pstrndup(pool, data_start, (data - data_start));
return svn_error_createf
(APR_EINVAL, NULL,
_("Safe data '%s' was followed by non-ASCII byte %d: "
"unable to convert to/from UTF-8"),
error_data, *((const unsigned char *) data));
} else {
return svn_error_createf
(APR_EINVAL, NULL,
_("Non-ASCII character (code %d) detected, "
"and unable to convert to/from UTF-8"),
*((const unsigned char *) data));
}
}
}
return SVN_NO_ERROR;
}
static svn_error_t *
invalid_utf8(const char *data, apr_size_t len, apr_pool_t *pool) {
const char *last = svn_utf__last_valid(data, len);
const char *valid_txt = "", *invalid_txt = "";
int i, valid, invalid;
valid = last - data;
if (valid > 24)
valid = 24;
for (i = 0; i < valid; ++i)
valid_txt = apr_pstrcat(pool, valid_txt,
apr_psprintf(pool, " %02x",
(unsigned char)last[i-valid]), NULL);
invalid = data + len - last;
if (invalid > 4)
invalid = 4;
for (i = 0; i < invalid; ++i)
invalid_txt = apr_pstrcat(pool, invalid_txt,
apr_psprintf(pool, " %02x",
(unsigned char)last[i]), NULL);
return svn_error_createf(APR_EINVAL, NULL,
_("Valid UTF-8 data\n(hex:%s)\n"
"followed by invalid UTF-8 sequence\n(hex:%s)"),
valid_txt, invalid_txt);
}
static svn_error_t *
check_utf8(const char *data, apr_size_t len, apr_pool_t *pool) {
if (! svn_utf__is_valid(data, len))
return invalid_utf8(data, len, pool);
return SVN_NO_ERROR;
}
static svn_error_t *
check_cstring_utf8(const char *data, apr_pool_t *pool) {
if (! svn_utf__cstring_is_valid(data))
return invalid_utf8(data, strlen(data), pool);
return SVN_NO_ERROR;
}
svn_error_t *
svn_utf_stringbuf_to_utf8(svn_stringbuf_t **dest,
const svn_stringbuf_t *src,
apr_pool_t *pool) {
xlate_handle_node_t *node;
svn_error_t *err;
SVN_ERR(get_ntou_xlate_handle_node(&node, pool));
if (node->handle) {
err = convert_to_stringbuf(node, src->data, src->len, dest, pool);
if (! err)
err = check_utf8((*dest)->data, (*dest)->len, pool);
} else {
err = check_non_ascii(src->data, src->len, pool);
if (! err)
*dest = svn_stringbuf_dup(src, pool);
}
put_xlate_handle_node(node, SVN_UTF_NTOU_XLATE_HANDLE, pool);
return err;
}
svn_error_t *
svn_utf_string_to_utf8(const svn_string_t **dest,
const svn_string_t *src,
apr_pool_t *pool) {
svn_stringbuf_t *destbuf;
xlate_handle_node_t *node;
svn_error_t *err;
SVN_ERR(get_ntou_xlate_handle_node(&node, pool));
if (node->handle) {
err = convert_to_stringbuf(node, src->data, src->len, &destbuf, pool);
if (! err)
err = check_utf8(destbuf->data, destbuf->len, pool);
if (! err)
*dest = svn_string_create_from_buf(destbuf, pool);
} else {
err = check_non_ascii(src->data, src->len, pool);
if (! err)
*dest = svn_string_dup(src, pool);
}
put_xlate_handle_node(node, SVN_UTF_NTOU_XLATE_HANDLE, pool);
return err;
}
static svn_error_t *
convert_cstring(const char **dest,
const char *src,
xlate_handle_node_t *node,
apr_pool_t *pool) {
if (node->handle) {
svn_stringbuf_t *destbuf;
SVN_ERR(convert_to_stringbuf(node, src, strlen(src),
&destbuf, pool));
*dest = destbuf->data;
} else {
apr_size_t len = strlen(src);
SVN_ERR(check_non_ascii(src, len, pool));
*dest = apr_pstrmemdup(pool, src, len);
}
return SVN_NO_ERROR;
}
svn_error_t *
svn_utf_cstring_to_utf8(const char **dest,
const char *src,
apr_pool_t *pool) {
xlate_handle_node_t *node;
svn_error_t *err;
SVN_ERR(get_ntou_xlate_handle_node(&node, pool));
err = convert_cstring(dest, src, node, pool);
put_xlate_handle_node(node, SVN_UTF_NTOU_XLATE_HANDLE, pool);
SVN_ERR(err);
SVN_ERR(check_cstring_utf8(*dest, pool));
return SVN_NO_ERROR;
}
svn_error_t *
svn_utf_cstring_to_utf8_ex2(const char **dest,
const char *src,
const char *frompage,
apr_pool_t *pool) {
xlate_handle_node_t *node;
svn_error_t *err;
const char *convset_key = get_xlate_key(SVN_APR_UTF8_CHARSET, frompage,
pool);
SVN_ERR(get_xlate_handle_node(&node, SVN_APR_UTF8_CHARSET, frompage,
convset_key, pool));
err = convert_cstring(dest, src, node, pool);
put_xlate_handle_node(node, convset_key, pool);
SVN_ERR(err);
SVN_ERR(check_cstring_utf8(*dest, pool));
return SVN_NO_ERROR;
}
svn_error_t *
svn_utf_cstring_to_utf8_ex(const char **dest,
const char *src,
const char *frompage,
const char *convset_key,
apr_pool_t *pool) {
return svn_utf_cstring_to_utf8_ex2(dest, src, frompage, pool);
}
svn_error_t *
svn_utf_stringbuf_from_utf8(svn_stringbuf_t **dest,
const svn_stringbuf_t *src,
apr_pool_t *pool) {
xlate_handle_node_t *node;
svn_error_t *err;
SVN_ERR(get_uton_xlate_handle_node(&node, pool));
if (node->handle) {
err = check_utf8(src->data, src->len, pool);
if (! err)
err = convert_to_stringbuf(node, src->data, src->len, dest, pool);
} else {
err = check_non_ascii(src->data, src->len, pool);
if (! err)
*dest = svn_stringbuf_dup(src, pool);
}
put_xlate_handle_node(node, SVN_UTF_UTON_XLATE_HANDLE, pool);
return err;
}
svn_error_t *
svn_utf_string_from_utf8(const svn_string_t **dest,
const svn_string_t *src,
apr_pool_t *pool) {
svn_stringbuf_t *dbuf;
xlate_handle_node_t *node;
svn_error_t *err;
SVN_ERR(get_uton_xlate_handle_node(&node, pool));
if (node->handle) {
err = check_utf8(src->data, src->len, pool);
if (! err)
err = convert_to_stringbuf(node, src->data, src->len,
&dbuf, pool);
if (! err)
*dest = svn_string_create_from_buf(dbuf, pool);
} else {
err = check_non_ascii(src->data, src->len, pool);
if (! err)
*dest = svn_string_dup(src, pool);
}
put_xlate_handle_node(node, SVN_UTF_UTON_XLATE_HANDLE, pool);
return err;
}
svn_error_t *
svn_utf_cstring_from_utf8(const char **dest,
const char *src,
apr_pool_t *pool) {
xlate_handle_node_t *node;
svn_error_t *err;
SVN_ERR(check_utf8(src, strlen(src), pool));
SVN_ERR(get_uton_xlate_handle_node(&node, pool));
err = convert_cstring(dest, src, node, pool);
put_xlate_handle_node(node, SVN_UTF_UTON_XLATE_HANDLE, pool);
return err;
}
svn_error_t *
svn_utf_cstring_from_utf8_ex2(const char **dest,
const char *src,
const char *topage,
apr_pool_t *pool) {
xlate_handle_node_t *node;
svn_error_t *err;
const char *convset_key = get_xlate_key(topage, SVN_APR_UTF8_CHARSET,
pool);
SVN_ERR(check_utf8(src, strlen(src), pool));
SVN_ERR(get_xlate_handle_node(&node, topage, SVN_APR_UTF8_CHARSET,
convset_key, pool));
err = convert_cstring(dest, src, node, pool);
put_xlate_handle_node(node, convset_key, pool);
return err;
}
svn_error_t *
svn_utf_cstring_from_utf8_ex(const char **dest,
const char *src,
const char *topage,
const char *convset_key,
apr_pool_t *pool) {
return svn_utf_cstring_from_utf8_ex2(dest, src, topage, pool);
}
const char *
svn_utf__cstring_from_utf8_fuzzy(const char *src,
apr_pool_t *pool,
svn_error_t *(*convert_from_utf8)
(const char **, const char *, apr_pool_t *)) {
const char *escaped, *converted;
svn_error_t *err;
escaped = fuzzy_escape(src, strlen(src), pool);
err = convert_from_utf8(((const char **) &converted), escaped, pool);
if (err) {
svn_error_clear(err);
return escaped;
} else
return converted;
}
const char *
svn_utf_cstring_from_utf8_fuzzy(const char *src,
apr_pool_t *pool) {
return svn_utf__cstring_from_utf8_fuzzy(src, pool,
svn_utf_cstring_from_utf8);
}
svn_error_t *
svn_utf_cstring_from_utf8_stringbuf(const char **dest,
const svn_stringbuf_t *src,
apr_pool_t *pool) {
svn_stringbuf_t *destbuf;
SVN_ERR(svn_utf_stringbuf_from_utf8(&destbuf, src, pool));
*dest = destbuf->data;
return SVN_NO_ERROR;
}
svn_error_t *
svn_utf_cstring_from_utf8_string(const char **dest,
const svn_string_t *src,
apr_pool_t *pool) {
svn_stringbuf_t *dbuf;
xlate_handle_node_t *node;
svn_error_t *err;
SVN_ERR(get_uton_xlate_handle_node(&node, pool));
if (node->handle) {
err = check_utf8(src->data, src->len, pool);
if (! err)
err = convert_to_stringbuf(node, src->data, src->len,
&dbuf, pool);
if (! err)
*dest = dbuf->data;
} else {
err = check_non_ascii(src->data, src->len, pool);
if (! err)
*dest = apr_pstrmemdup(pool, src->data, src->len);
}
put_xlate_handle_node(node, SVN_UTF_UTON_XLATE_HANDLE, pool);
return err;
}
