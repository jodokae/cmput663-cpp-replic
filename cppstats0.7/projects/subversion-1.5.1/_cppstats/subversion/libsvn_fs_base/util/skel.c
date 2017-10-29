#include <string.h>
#include "svn_string.h"
#include "skel.h"
#include "../key-gen.h"
enum char_type {
type_nothing = 0,
type_space = 1,
type_digit = 2,
type_paren = 3,
type_name = 4
};
static const enum char_type skel_char_type[256] = {
0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 1, 1, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
1, 0, 0, 0, 0, 0, 0, 0, 3, 3, 0, 0, 0, 0, 0, 0,
2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0,
0, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 3, 0, 3, 0, 0,
0, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};
static skel_t *parse(const char *data, apr_size_t len,
apr_pool_t *pool);
static skel_t *list(const char *data, apr_size_t len,
apr_pool_t *pool);
static skel_t *implicit_atom(const char *data, apr_size_t len,
apr_pool_t *pool);
static skel_t *explicit_atom(const char *data, apr_size_t len,
apr_pool_t *pool);
skel_t *
svn_fs_base__parse_skel(const char *data,
apr_size_t len,
apr_pool_t *pool) {
return parse(data, len, pool);
}
static skel_t *
parse(const char *data,
apr_size_t len,
apr_pool_t *pool) {
char c;
if (len <= 0)
return 0;
c = *data;
if (c == '(')
return list(data, len, pool);
if (skel_char_type[(unsigned char) c] == type_name)
return implicit_atom(data, len, pool);
else
return explicit_atom(data, len, pool);
}
static skel_t *
list(const char *data,
apr_size_t len,
apr_pool_t *pool) {
const char *end = data + len;
const char *list_start;
if (data >= end || *data != '(')
return 0;
list_start = data;
data++;
{
skel_t *children = 0;
skel_t **tail = &children;
for (;;) {
skel_t *element;
while (data < end
&& skel_char_type[(unsigned char) *data] == type_space)
data++;
if (data >= end)
return 0;
if (*data == ')') {
data++;
break;
}
element = parse(data, end - data, pool);
if (! element)
return 0;
element->next = 0;
*tail = element;
tail = &element->next;
data = element->data + element->len;
}
{
skel_t *s = apr_pcalloc(pool, sizeof(*s));
s->is_atom = FALSE;
s->data = list_start;
s->len = data - list_start;
s->children = children;
return s;
}
}
}
static skel_t *
implicit_atom(const char *data,
apr_size_t len,
apr_pool_t *pool) {
const char *start = data;
const char *end = data + len;
skel_t *s;
if (data >= end || skel_char_type[(unsigned char) *data] != type_name)
return 0;
while (++data < end
&& skel_char_type[(unsigned char) *data] != type_space
&& skel_char_type[(unsigned char) *data] != type_paren)
;
s = apr_pcalloc(pool, sizeof(*s));
s->is_atom = TRUE;
s->data = start;
s->len = data - start;
return s;
}
static skel_t *
explicit_atom(const char *data,
apr_size_t len,
apr_pool_t *pool) {
const char *end = data + len;
const char *next;
apr_size_t size;
skel_t *s;
size = svn_fs_base__getsize(data, end - data, &next, end - data);
data = next;
if (! data)
return 0;
if (data >= end || skel_char_type[(unsigned char) *data] != type_space)
return 0;
data++;
if (data + size > end)
return 0;
s = apr_pcalloc(pool, sizeof(*s));
s->is_atom = TRUE;
s->data = data;
s->len = size;
return s;
}
static apr_size_t estimate_unparsed_size(skel_t *);
static svn_stringbuf_t *unparse(skel_t *, svn_stringbuf_t *, apr_pool_t *);
svn_stringbuf_t *
svn_fs_base__unparse_skel(skel_t *skel, apr_pool_t *pool) {
svn_stringbuf_t *str;
str = apr_palloc(pool, sizeof(*str));
str->pool = pool;
str->blocksize = estimate_unparsed_size(skel) + 200;
str->data = apr_palloc(pool, str->blocksize);
str->len = 0;
return unparse(skel, str, pool);
}
static apr_size_t
estimate_unparsed_size(skel_t *skel) {
if (skel->is_atom) {
if (skel->len < 100)
return skel->len + 3;
else
return skel->len + 30;
} else {
int total_len;
skel_t *child;
total_len = 2;
for (child = skel->children; child; child = child->next)
total_len += estimate_unparsed_size(child) + 1;
return total_len;
}
}
static svn_boolean_t
use_implicit(skel_t *skel) {
if (skel->len == 0
|| skel->len >= 100)
return FALSE;
if (skel_char_type[(unsigned char) skel->data[0]] != type_name)
return FALSE;
{
apr_size_t i;
for (i = 1; i < skel->len; i++)
if (skel_char_type[(unsigned char) skel->data[i]] == type_space
|| skel_char_type[(unsigned char) skel->data[i]] == type_paren)
return FALSE;
}
return TRUE;
}
static svn_stringbuf_t *
unparse(skel_t *skel, svn_stringbuf_t *str, apr_pool_t *pool) {
if (skel->is_atom) {
if (use_implicit(skel))
svn_stringbuf_appendbytes(str, skel->data, skel->len);
else {
char buf[200];
int length_len;
length_len = svn_fs_base__putsize(buf, sizeof(buf), skel->len);
if (! length_len)
abort();
svn_stringbuf_ensure(str, str->len + length_len + 1 + skel->len);
svn_stringbuf_appendbytes(str, buf, length_len);
str->data[str->len++] = ' ';
svn_stringbuf_appendbytes(str, skel->data, skel->len);
}
} else {
skel_t *child;
svn_stringbuf_ensure(str, str->len + 1);
str->data[str->len++] = '(';
for (child = skel->children; child; child = child->next) {
unparse(child, str, pool);
if (child->next) {
svn_stringbuf_ensure(str, str->len + 1);
str->data[str->len++] = ' ';
}
}
svn_stringbuf_appendbytes(str, ")", 1);
}
return str;
}
skel_t *
svn_fs_base__str_atom(const char *str, apr_pool_t *pool) {
skel_t *skel = apr_pcalloc(pool, sizeof(*skel));
skel->is_atom = TRUE;
skel->data = str;
skel->len = strlen(str);
return skel;
}
skel_t *
svn_fs_base__mem_atom(const void *addr,
apr_size_t len,
apr_pool_t *pool) {
skel_t *skel = apr_pcalloc(pool, sizeof(*skel));
skel->is_atom = TRUE;
skel->data = addr;
skel->len = len;
return skel;
}
skel_t *
svn_fs_base__make_empty_list(apr_pool_t *pool) {
skel_t *skel = apr_pcalloc(pool, sizeof(*skel));
return skel;
}
void
svn_fs_base__prepend(skel_t *skel, skel_t *list_skel) {
if (list_skel->is_atom)
abort();
skel->next = list_skel->children;
list_skel->children = skel;
}
void
svn_fs_base__append(skel_t *skel, skel_t *list_skel) {
if (list_skel->is_atom)
abort();
if (! list_skel->children) {
list_skel->children = skel;
} else {
skel_t *tmp = list_skel->children;
while (tmp->next) {
tmp = tmp->next;
}
tmp->next = skel;
}
}
svn_boolean_t
svn_fs_base__matches_atom(skel_t *skel, const char *str) {
if (skel && skel->is_atom) {
apr_size_t len = strlen(str);
return ((skel->len == len
&& ! memcmp(skel->data, str, len)) ? TRUE : FALSE);
}
return FALSE;
}
int
svn_fs_base__atom_matches_string(skel_t *skel, const svn_string_t *str) {
if (skel && skel->is_atom) {
return ((skel->len == str->len
&& ! memcmp(skel->data, str->data, skel->len)) ? TRUE : FALSE);
}
return FALSE;
}
int
svn_fs_base__list_length(skel_t *skel) {
int len = 0;
skel_t *child;
if ((! skel) || skel->is_atom)
return -1;
for (child = skel->children; child; child = child->next)
len++;
return len;
}
svn_boolean_t
svn_fs_base__skels_are_equal(skel_t *skel1, skel_t *skel2) {
if (skel1 == skel2)
return TRUE;
if (skel1->is_atom && skel2->is_atom) {
if ((skel1->len == skel2->len)
&& (! strncmp(skel1->data, skel2->data, skel1->len)))
return TRUE;
else
return FALSE;
} else if (((! skel1->is_atom) && (! skel2->is_atom))
&& ((svn_fs_base__list_length(skel1))
== (svn_fs_base__list_length(skel2)))) {
int len = svn_fs_base__list_length(skel1);
int i;
for (i = 0; i < len; i++)
if (! svn_fs_base__skels_are_equal((skel1->children) + i,
(skel2->children) + i))
return FALSE;
return TRUE;
} else
return FALSE;
}
skel_t *
svn_fs_base__copy_skel(skel_t *skel, apr_pool_t *pool) {
skel_t *copy = apr_pcalloc(pool, sizeof(*copy));
if (skel->is_atom) {
apr_size_t len = skel->len;
char *s = apr_palloc(pool, len);
memcpy(s, skel->data, len);
copy->is_atom = TRUE;
copy->data = s;
copy->len = len;
} else {
skel_t *skel_child, **copy_child_ptr;
copy->is_atom = FALSE;
copy->data = 0;
copy->len = 0;
copy_child_ptr = &copy->children;
for (skel_child = skel->children;
skel_child;
skel_child = skel_child->next) {
*copy_child_ptr = svn_fs_base__copy_skel(skel_child, pool);
copy_child_ptr = &(*copy_child_ptr)->next;
}
*copy_child_ptr = 0;
}
return copy;
}
