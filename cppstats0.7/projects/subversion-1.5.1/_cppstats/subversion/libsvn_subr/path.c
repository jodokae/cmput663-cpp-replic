#include <string.h>
#include <assert.h>
#include <apr_file_info.h>
#include <apr_lib.h>
#include "svn_string.h"
#include "svn_path.h"
#include "svn_private_config.h"
#include "svn_utf.h"
#include "svn_io.h"
#include "svn_ctype.h"
#define SVN_EMPTY_PATH ""
#define SVN_PATH_IS_EMPTY(s) ((s)[0] == '\0')
#define SVN_PATH_IS_PLATFORM_EMPTY(s,n) ((n) == 1 && (s)[0] == '.')
const char *
svn_path_internal_style(const char *path, apr_pool_t *pool) {
if ('/' != SVN_PATH_LOCAL_SEPARATOR) {
char *p = apr_pstrdup(pool, path);
path = p;
for (; *p != '\0'; ++p)
if (*p == SVN_PATH_LOCAL_SEPARATOR)
*p = '/';
}
return svn_path_canonicalize(path, pool);
}
const char *
svn_path_local_style(const char *path, apr_pool_t *pool) {
path = svn_path_canonicalize(path, pool);
if (SVN_PATH_IS_EMPTY(path))
return ".";
if (svn_path_is_url(path))
return apr_pstrdup(pool, path);
if ('/' != SVN_PATH_LOCAL_SEPARATOR) {
char *p = apr_pstrdup(pool, path);
path = p;
for (; *p != '\0'; ++p)
if (*p == '/')
*p = SVN_PATH_LOCAL_SEPARATOR;
}
return path;
}
#if !defined(NDEBUG)
static svn_boolean_t
is_canonical(const char *path,
apr_size_t len) {
return (! SVN_PATH_IS_PLATFORM_EMPTY(path, len)
&& (svn_dirent_is_root(path, len) ||
(len <= 1 || path[len-1] != '/')));
}
#endif
char *svn_path_join(const char *base,
const char *component,
apr_pool_t *pool) {
apr_size_t blen = strlen(base);
apr_size_t clen = strlen(component);
char *path;
assert(is_canonical(base, blen));
assert(is_canonical(component, clen));
if (*component == '/')
return apr_pmemdup(pool, component, clen + 1);
if (SVN_PATH_IS_EMPTY(base))
return apr_pmemdup(pool, component, clen + 1);
if (SVN_PATH_IS_EMPTY(component))
return apr_pmemdup(pool, base, blen + 1);
if (blen == 1 && base[0] == '/')
blen = 0;
path = apr_palloc(pool, blen + 1 + clen + 1);
memcpy(path, base, blen);
path[blen] = '/';
memcpy(path + blen + 1, component, clen + 1);
return path;
}
char *svn_path_join_many(apr_pool_t *pool, const char *base, ...) {
#define MAX_SAVED_LENGTHS 10
apr_size_t saved_lengths[MAX_SAVED_LENGTHS];
apr_size_t total_len;
int nargs;
va_list va;
const char *s;
apr_size_t len;
char *path;
char *p;
svn_boolean_t base_is_empty = FALSE, base_is_root = FALSE;
int base_arg = 0;
total_len = strlen(base);
assert(is_canonical(base, total_len));
if (total_len == 1 && *base == '/')
base_is_root = TRUE;
else if (SVN_PATH_IS_EMPTY(base)) {
total_len = sizeof(SVN_EMPTY_PATH) - 1;
base_is_empty = TRUE;
}
saved_lengths[0] = total_len;
nargs = 0;
va_start(va, base);
while ((s = va_arg(va, const char *)) != NULL) {
len = strlen(s);
assert(is_canonical(s, len));
if (SVN_PATH_IS_EMPTY(s))
continue;
if (nargs++ < MAX_SAVED_LENGTHS)
saved_lengths[nargs] = len;
if (*s == '/') {
total_len = len;
base_arg = nargs;
base_is_root = len == 1;
base_is_empty = FALSE;
} else if (nargs == base_arg
|| (nargs == base_arg + 1 && base_is_root)
|| base_is_empty) {
if (base_is_empty) {
base_is_empty = FALSE;
total_len = 0;
}
total_len += len;
} else {
total_len += 1 + len;
}
}
va_end(va);
if (base_is_root && total_len == 1)
return apr_pmemdup(pool, "/", 2);
path = p = apr_palloc(pool, total_len + 1);
if (base_arg == 0 && ! (SVN_PATH_IS_EMPTY(base) && ! base_is_empty)) {
if (SVN_PATH_IS_EMPTY(base))
memcpy(p, SVN_EMPTY_PATH, len = saved_lengths[0]);
else
memcpy(p, base, len = saved_lengths[0]);
p += len;
}
nargs = 0;
va_start(va, base);
while ((s = va_arg(va, const char *)) != NULL) {
if (SVN_PATH_IS_EMPTY(s))
continue;
if (++nargs < base_arg)
continue;
if (nargs < MAX_SAVED_LENGTHS)
len = saved_lengths[nargs];
else
len = strlen(s);
if (p != path && p[-1] != '/')
*p++ = '/';
memcpy(p, s, len);
p += len;
}
va_end(va);
*p = '\0';
assert((apr_size_t)(p - path) == total_len);
return path;
}
apr_size_t
svn_path_component_count(const char *path) {
apr_size_t count = 0;
assert(is_canonical(path, strlen(path)));
while (*path) {
const char *start;
while (*path == '/')
++path;
start = path;
while (*path && *path != '/')
++path;
if (path != start)
++count;
}
return count;
}
static apr_size_t
previous_segment(const char *path,
apr_size_t len) {
if (len == 0)
return 0;
while (len > 0 && path[--len] != '/')
;
if (len == 0 && path[0] == '/')
return 1;
else
return len;
}
void
svn_path_add_component(svn_stringbuf_t *path,
const char *component) {
apr_size_t len = strlen(component);
assert(is_canonical(path->data, path->len));
assert(is_canonical(component, len));
if ((! SVN_PATH_IS_EMPTY(path->data))
&& (! ((path->len == 1) && (*(path->data) == '/')))) {
char dirsep = '/';
svn_stringbuf_appendbytes(path, &dirsep, sizeof(dirsep));
}
svn_stringbuf_appendbytes(path, component, len);
}
void
svn_path_remove_component(svn_stringbuf_t *path) {
assert(is_canonical(path->data, path->len));
path->len = previous_segment(path->data, path->len);
path->data[path->len] = '\0';
}
void
svn_path_remove_components(svn_stringbuf_t *path, apr_size_t n) {
while (n > 0) {
svn_path_remove_component(path);
n--;
}
}
char *
svn_path_dirname(const char *path, apr_pool_t *pool) {
apr_size_t len = strlen(path);
assert(is_canonical(path, len));
return apr_pstrmemdup(pool, path, previous_segment(path, len));
}
char *
svn_path_basename(const char *path, apr_pool_t *pool) {
apr_size_t len = strlen(path);
apr_size_t start;
assert(is_canonical(path, len));
if (len == 1 && path[0] == '/')
start = 0;
else {
start = len;
while (start > 0 && path[start - 1] != '/')
--start;
}
return apr_pstrmemdup(pool, path + start, len - start);
}
void
svn_path_split(const char *path,
const char **dirpath,
const char **base_name,
apr_pool_t *pool) {
assert(dirpath != base_name);
if (dirpath)
*dirpath = svn_path_dirname(path, pool);
if (base_name)
*base_name = svn_path_basename(path, pool);
}
int
svn_path_is_empty(const char *path) {
if (SVN_PATH_IS_EMPTY(path))
return 1;
return 0;
}
svn_boolean_t
svn_dirent_is_root(const char *dirent, apr_size_t len) {
if (len == 1 && dirent[0] == '/')
return TRUE;
#if defined(WIN32) || defined(__CYGWIN__)
if ((len == 2 || len == 3) &&
(dirent[1] == ':') &&
((dirent[0] >= 'A' && dirent[0] <= 'Z') ||
(dirent[0] >= 'a' && dirent[0] <= 'z')) &&
(len == 2 || (dirent[2] == '/' && len == 3)))
return TRUE;
if (len >= 2 && dirent[0] == '/' && dirent[1] == '/'
&& dirent[len - 1] != '/') {
int segments = 0;
int i;
for (i = len; i >= 2; i--) {
if (dirent[i] == '/') {
segments ++;
if (segments > 1)
return FALSE;
}
}
return (segments <= 1);
}
#endif
return FALSE;
}
int
svn_path_compare_paths(const char *path1,
const char *path2) {
apr_size_t path1_len = strlen(path1);
apr_size_t path2_len = strlen(path2);
apr_size_t min_len = ((path1_len < path2_len) ? path1_len : path2_len);
apr_size_t i = 0;
assert(is_canonical(path1, path1_len));
assert(is_canonical(path2, path2_len));
while (i < min_len && path1[i] == path2[i])
++i;
if ((path1_len == path2_len) && (i >= min_len))
return 0;
if ((path1[i] == '/') && (path2[i] == 0))
return 1;
if ((path2[i] == '/') && (path1[i] == 0))
return -1;
if (path1[i] == '/')
return -1;
if (path2[i] == '/')
return 1;
return (unsigned char)(path1[i]) < (unsigned char)(path2[i]) ? -1 : 1;
}
static apr_size_t
get_path_ancestor_length(const char *path1,
const char *path2,
apr_pool_t *pool) {
apr_size_t path1_len, path2_len;
apr_size_t i = 0;
apr_size_t last_dirsep = 0;
path1_len = strlen(path1);
path2_len = strlen(path2);
if (SVN_PATH_IS_EMPTY(path1) || SVN_PATH_IS_EMPTY(path2))
return 0;
while (path1[i] == path2[i]) {
if (path1[i] == '/')
last_dirsep = i;
i++;
if ((i == path1_len) || (i == path2_len))
break;
}
if (i == 1 && path1[0] == '/' && path2[0] == '/')
return 1;
if (((i == path1_len) && (path2[i] == '/'))
|| ((i == path2_len) && (path1[i] == '/'))
|| ((i == path1_len) && (i == path2_len)))
return i;
else if (last_dirsep == 0 && path1[0] == '/' && path2[0] == '/')
return 1;
return last_dirsep;
}
char *
svn_path_get_longest_ancestor(const char *path1,
const char *path2,
apr_pool_t *pool) {
svn_boolean_t path1_is_url, path2_is_url;
path1_is_url = svn_path_is_url(path1);
path2_is_url = svn_path_is_url(path2);
if (path1_is_url && path2_is_url) {
apr_size_t path_ancestor_len;
apr_size_t i = 0;
while (1) {
if (path1[i] != path2[i])
return apr_pmemdup(pool, SVN_EMPTY_PATH,
sizeof(SVN_EMPTY_PATH));
if (path1[i] == ':')
break;
assert((path1[i] != '\0') && (path2[i] != '\0'));
i++;
}
i += 3;
path_ancestor_len = get_path_ancestor_length(path1 + i, path2 + i,
pool);
if (path_ancestor_len == 0 ||
(path_ancestor_len == 1 && (path1 + i)[0] == '/'))
return apr_pmemdup(pool, SVN_EMPTY_PATH, sizeof(SVN_EMPTY_PATH));
else
return apr_pstrndup(pool, path1, path_ancestor_len + i);
}
else if ((! path1_is_url) && (! path2_is_url)) {
return apr_pstrndup(pool, path1,
get_path_ancestor_length(path1, path2, pool));
}
else {
return apr_pmemdup(pool, SVN_EMPTY_PATH, sizeof(SVN_EMPTY_PATH));
}
}
const char *
svn_path_is_child(const char *path1,
const char *path2,
apr_pool_t *pool) {
apr_size_t i;
if (SVN_PATH_IS_EMPTY(path1)) {
if (SVN_PATH_IS_EMPTY(path2)
|| path2[0] == '/')
return NULL;
else
return pool ? apr_pstrdup(pool, path2) : path2;
}
for (i = 0; path1[i] && path2[i]; i++)
if (path1[i] != path2[i])
return NULL;
if (path1[i] == '\0' && path2[i]) {
if (path2[i] == '/')
return pool ? apr_pstrdup(pool, path2 + i + 1) : path2 + i + 1;
else if (i == 1 && path1[0] == '/')
return pool ? apr_pstrdup(pool, path2 + 1) : path2 + 1;
}
return NULL;
}
svn_boolean_t
svn_path_is_ancestor(const char *path1, const char *path2) {
apr_size_t path1_len = strlen(path1);
if (SVN_PATH_IS_EMPTY(path1))
return *path2 != '/';
if (strncmp(path1, path2, path1_len) == 0)
return path1[path1_len - 1] == '/'
|| (path2[path1_len] == '/' || path2[path1_len] == '\0');
return FALSE;
}
apr_array_header_t *
svn_path_decompose(const char *path,
apr_pool_t *pool) {
apr_size_t i, oldi;
apr_array_header_t *components =
apr_array_make(pool, 1, sizeof(const char *));
if (SVN_PATH_IS_EMPTY(path))
return components;
i = oldi = 0;
if (path[i] == '/') {
char dirsep = '/';
APR_ARRAY_PUSH(components, const char *)
= apr_pstrmemdup(pool, &dirsep, sizeof(dirsep));
i++;
oldi++;
if (path[i] == '\0')
return components;
}
do {
if ((path[i] == '/') || (path[i] == '\0')) {
if (SVN_PATH_IS_PLATFORM_EMPTY(path + oldi, i - oldi))
APR_ARRAY_PUSH(components, const char *) = SVN_EMPTY_PATH;
else
APR_ARRAY_PUSH(components, const char *)
= apr_pstrmemdup(pool, path + oldi, i - oldi);
i++;
oldi = i;
continue;
}
i++;
} while (path[i-1]);
return components;
}
const char *
svn_path_compose(const apr_array_header_t *components,
apr_pool_t *pool) {
apr_size_t *lengths = apr_palloc(pool, components->nelts*sizeof(*lengths));
apr_size_t max_length = components->nelts;
char *path;
char *p;
int i;
for (i = 0; i < components->nelts; ++i) {
apr_size_t l = strlen(APR_ARRAY_IDX(components, i, const char *));
lengths[i] = l;
max_length += l;
}
path = apr_palloc(pool, max_length + 1);
p = path;
for (i = 0; i < components->nelts; ++i) {
if (i > 1 ||
(i == 1 && strcmp("/", APR_ARRAY_IDX(components,
0,
const char *)) != 0)) {
*p++ = '/';
}
memcpy(p, APR_ARRAY_IDX(components, i, const char *), lengths[i]);
p += lengths[i];
}
*p = '\0';
return path;
}
svn_boolean_t
svn_path_is_single_path_component(const char *name) {
if (SVN_PATH_IS_EMPTY(name)
|| (name[0] == '.' && name[1] == '.' && name[2] == '\0'))
return FALSE;
if (strchr(name, '/') != NULL)
return FALSE;
return TRUE;
}
svn_boolean_t
svn_path_is_backpath_present(const char *path) {
int len = strlen(path);
if (! strcmp(path, ".."))
return TRUE;
if (! strncmp(path, "../", 3))
return TRUE;
if (strstr(path, "/../") != NULL)
return TRUE;
if (len >= 3
&& (! strncmp(path + len - 3, "/..", 3)))
return TRUE;
return FALSE;
}
static const char *
skip_uri_scheme(const char *path) {
apr_size_t j;
for (j = 0; path[j] && path[j] != ':'; ++j)
if (path[j] == '/')
return NULL;
if (j > 0 && path[j] == ':' && path[j+1] == '/' && path[j+2] == '/')
return path + j + 3;
return NULL;
}
svn_boolean_t
svn_path_is_url(const char *path) {
return skip_uri_scheme(path) ? TRUE : FALSE;
}
static const char uri_char_validity[256] = {
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 1, 0, 0, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 0, 0,
1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1,
0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};
svn_boolean_t
svn_path_is_uri_safe(const char *path) {
apr_size_t i;
path = skip_uri_scheme(path);
if (! path)
return FALSE;
path = strchr(path, '/');
if (path == NULL)
return TRUE;
for (i = 0; path[i]; i++) {
if (path[i] == '%') {
if (apr_isxdigit(path[i + 1]) && apr_isxdigit(path[i + 2])) {
i += 2;
continue;
}
return FALSE;
} else if (! uri_char_validity[((unsigned char)path[i])]) {
return FALSE;
}
}
return TRUE;
}
static const char *
uri_escape(const char *path, const char table[], apr_pool_t *pool) {
svn_stringbuf_t *retstr;
apr_size_t i, copied = 0;
int c;
retstr = svn_stringbuf_create("", pool);
for (i = 0; path[i]; i++) {
c = (unsigned char)path[i];
if (table[c])
continue;
if (i - copied)
svn_stringbuf_appendbytes(retstr, path + copied,
i - copied);
svn_stringbuf_ensure(retstr, retstr->len + 4);
sprintf(retstr->data + retstr->len, "%%%02X", (unsigned char)c);
retstr->len += 3;
copied = i + 1;
}
if (retstr->len == 0)
return path;
if (i - copied)
svn_stringbuf_appendbytes(retstr, path + copied, i - copied);
return retstr->data;
}
const char *
svn_path_uri_encode(const char *path, apr_pool_t *pool) {
const char *ret;
ret = uri_escape(path, uri_char_validity, pool);
if (ret == path)
return apr_pstrdup(pool, path);
else
return ret;
}
static const char iri_escape_chars[256] = {
1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};
const char *
svn_path_uri_from_iri(const char *iri, apr_pool_t *pool) {
return uri_escape(iri, iri_escape_chars, pool);
}
static const char uri_autoescape_chars[256] = {
1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
0, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1,
1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1,
0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1,
1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
};
const char *
svn_path_uri_autoescape(const char *uri, apr_pool_t *pool) {
return uri_escape(uri, uri_autoescape_chars, pool);
}
const char *
svn_path_uri_decode(const char *path, apr_pool_t *pool) {
svn_stringbuf_t *retstr;
apr_size_t i;
svn_boolean_t query_start = FALSE;
retstr = svn_stringbuf_create("", pool);
svn_stringbuf_ensure(retstr, strlen(path) + 1);
retstr->len = 0;
for (i = 0; path[i]; i++) {
char c = path[i];
if (c == '?') {
query_start = TRUE;
} else if (c == '+' && query_start) {
c = ' ';
} else if (c == '%' && apr_isxdigit(path[i + 1])
&& apr_isxdigit(path[i+2])) {
char digitz[3];
digitz[0] = path[++i];
digitz[1] = path[++i];
digitz[2] = '\0';
c = (char)(strtol(digitz, NULL, 16));
}
retstr->data[retstr->len++] = c;
}
retstr->data[retstr->len] = 0;
return retstr->data;
}
const char *
svn_path_url_add_component(const char *url,
const char *component,
apr_pool_t *pool) {
url = svn_path_canonicalize(url, pool);
return svn_path_join(url, svn_path_uri_encode(component, pool), pool);
}
svn_error_t *
svn_path_get_absolute(const char **pabsolute,
const char *relative,
apr_pool_t *pool) {
char *buffer;
apr_status_t apr_err;
const char *path_apr;
if (svn_path_is_url(relative)) {
*pabsolute = apr_pstrdup(pool, relative);
return SVN_NO_ERROR;
}
SVN_ERR(svn_path_cstring_from_utf8(&path_apr, relative, pool));
apr_err = apr_filepath_merge(&buffer, NULL,
path_apr,
APR_FILEPATH_NOTRELATIVE
| APR_FILEPATH_TRUENAME,
pool);
if (apr_err)
return svn_error_createf(SVN_ERR_BAD_FILENAME, NULL,
_("Couldn't determine absolute path of '%s'"),
svn_path_local_style(relative, pool));
SVN_ERR(svn_path_cstring_to_utf8(pabsolute, buffer, pool));
*pabsolute = svn_path_canonicalize(*pabsolute, pool);
return SVN_NO_ERROR;
}
svn_error_t *
svn_path_split_if_file(const char *path,
const char **pdirectory,
const char **pfile,
apr_pool_t *pool) {
apr_finfo_t finfo;
svn_error_t *err;
err = svn_io_stat(&finfo, path, APR_FINFO_TYPE, pool);
if (err && ! APR_STATUS_IS_ENOENT(err->apr_err))
return err;
if (err || finfo.filetype == APR_REG) {
svn_error_clear(err);
svn_path_split(path, pdirectory, pfile, pool);
} else if (finfo.filetype == APR_DIR) {
*pdirectory = path;
*pfile = SVN_EMPTY_PATH;
} else {
return svn_error_createf(SVN_ERR_BAD_FILENAME, NULL,
_("'%s' is neither a file nor a directory name"),
svn_path_local_style(path, pool));
}
return SVN_NO_ERROR;
}
const char *
svn_path_canonicalize(const char *path, apr_pool_t *pool) {
char *canon, *dst;
const char *src;
apr_size_t seglen;
apr_size_t canon_segments = 0;
svn_boolean_t uri;
dst = canon = apr_pcalloc(pool, strlen(path) + 1);
src = skip_uri_scheme(path);
if (src) {
uri = TRUE;
memcpy(dst, path, src - path);
dst += (src - path);
} else {
uri = FALSE;
src = path;
}
if (*src == '/') {
*(dst++) = *(src++);
#if defined(WIN32) || defined(__CYGWIN__)
if (!uri && *src == '/')
*(dst++) = *(src++);
#endif
}
while (*src) {
const char *next = src;
while (*next && (*next != '/'))
++next;
seglen = next - src;
if (seglen == 0 || (seglen == 1 && src[0] == '.')) {
} else {
if (*next)
seglen++;
memcpy(dst, src, seglen);
dst += seglen;
canon_segments++;
}
src = next;
if (*src)
src++;
}
if ((canon_segments > 0 || uri) && *(dst - 1) == '/')
dst--;
*dst = '\0';
#if defined(WIN32) || defined(__CYGWIN__)
if (canon_segments < 2 && canon[0] == '/' && canon[1] == '/')
return canon + 1;
#endif
return canon;
}
svn_boolean_t
svn_path_is_canonical(const char *path, apr_pool_t *pool) {
return (strcmp(path, svn_path_canonicalize(path, pool)) == 0);
}
static svn_error_t *
get_path_encoding(svn_boolean_t *path_is_utf8, apr_pool_t *pool) {
apr_status_t apr_err;
int encoding_style;
apr_err = apr_filepath_encoding(&encoding_style, pool);
if (apr_err)
return svn_error_wrap_apr(apr_err,
_("Can't determine the native path encoding"));
*path_is_utf8 = (encoding_style == APR_FILEPATH_ENCODING_UTF8);
return SVN_NO_ERROR;
}
svn_error_t *
svn_path_cstring_from_utf8(const char **path_apr,
const char *path_utf8,
apr_pool_t *pool) {
svn_boolean_t path_is_utf8;
SVN_ERR(get_path_encoding(&path_is_utf8, pool));
if (path_is_utf8) {
*path_apr = apr_pstrdup(pool, path_utf8);
return SVN_NO_ERROR;
} else
return svn_utf_cstring_from_utf8(path_apr, path_utf8, pool);
}
svn_error_t *
svn_path_cstring_to_utf8(const char **path_utf8,
const char *path_apr,
apr_pool_t *pool) {
svn_boolean_t path_is_utf8;
SVN_ERR(get_path_encoding(&path_is_utf8, pool));
if (path_is_utf8) {
*path_utf8 = apr_pstrdup(pool, path_apr);
return SVN_NO_ERROR;
} else
return svn_utf_cstring_to_utf8(path_utf8, path_apr, pool);
}
static const char *
illegal_path_escape(const char *path, apr_pool_t *pool) {
svn_stringbuf_t *retstr;
apr_size_t i, copied = 0;
int c;
retstr = svn_stringbuf_create("", pool);
for (i = 0; path[i]; i++) {
c = (unsigned char)path[i];
if (! svn_ctype_iscntrl(c))
continue;
if (i - copied)
svn_stringbuf_appendbytes(retstr, path + copied,
i - copied);
svn_stringbuf_ensure(retstr, retstr->len + 4);
sprintf(retstr->data + retstr->len, "\\%03o", (unsigned char)c);
retstr->len += 4;
copied = i + 1;
}
if (retstr->len == 0)
return path;
if (i - copied)
svn_stringbuf_appendbytes(retstr, path + copied, i - copied);
return retstr->data;
}
svn_error_t *
svn_path_check_valid(const char *path, apr_pool_t *pool) {
const char *c;
for (c = path; *c; c++) {
if (svn_ctype_iscntrl(*c)) {
return svn_error_createf
(SVN_ERR_FS_PATH_SYNTAX, NULL,
_("Invalid control character '0x%02x' in path '%s'"),
*c,
illegal_path_escape(svn_path_local_style(path, pool), pool));
}
}
return SVN_NO_ERROR;
}
void
svn_path_splitext(const char **path_root,
const char **path_ext,
const char *path,
apr_pool_t *pool) {
const char *last_dot, *last_slash;
if (! (path_root || path_ext))
return;
last_dot = strrchr(path, '.');
if (last_dot && (last_dot + 1 != '\0')) {
last_slash = strrchr(path, '/');
if ((last_slash && (last_dot > (last_slash + 1)))
|| ((! last_slash) && (last_dot > path))) {
if (path_root)
*path_root = apr_pstrmemdup(pool, path,
(last_dot - path + 1) * sizeof(*path));
if (path_ext)
*path_ext = apr_pstrdup(pool, last_dot + 1);
return;
}
}
if (path_root)
*path_root = apr_pstrdup(pool, path);
if (path_ext)
*path_ext = "";
}