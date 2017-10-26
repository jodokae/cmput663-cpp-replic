#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <apr.h>
#include "svn_pools.h"
#include "svn_string.h"
#include "../svn_test.h"
#include "../svn_test_fs.h"
#include "../../libsvn_fs_base/fs.h"
#include "../../libsvn_fs_base/util/skel.h"
static svn_error_t *
fail(apr_pool_t *pool, const char *fmt, ...) {
va_list ap;
char *msg;
va_start(ap, fmt);
msg = apr_pvsprintf(pool, fmt, ap);
va_end(ap);
return svn_error_create(SVN_ERR_TEST_FAILED, 0, msg);
}
static svn_stringbuf_t *
get_empty_string(apr_pool_t *pool) {
svn_pool_clear(pool);
return svn_stringbuf_ncreate(0, 0, pool);
}
static skel_t *
parse_str(svn_stringbuf_t *str, apr_pool_t *pool) {
return svn_fs_base__parse_skel(str->data, str->len, pool);
}
static skel_t *
parse_cstr(const char *str, apr_pool_t *pool) {
return svn_fs_base__parse_skel(str, strlen(str), pool);
}
enum char_type {
type_nothing = 0,
type_space = 1,
type_digit = 2,
type_paren = 3,
type_name = 4
};
static int skel_char_map_initialized;
static enum char_type skel_char_map[256];
static void
init_char_types(void) {
int i;
const char *c;
if (skel_char_map_initialized)
return;
for (i = 0; i < 256; i++)
skel_char_map[i] = type_nothing;
for (i = '0'; i <= '9'; i++)
skel_char_map[i] = type_digit;
for (c = "\t\n\f\r "; *c; c++)
skel_char_map[(unsigned char) *c] = type_space;
for (c = "()[]"; *c; c++)
skel_char_map[(unsigned char) *c] = type_paren;
for (i = 'A'; i <= 'Z'; i++)
skel_char_map[i] = type_name;
for (i = 'a'; i <= 'z'; i++)
skel_char_map[i] = type_name;
skel_char_map_initialized = 1;
}
static int
skel_is_space(char byte) {
init_char_types();
return skel_char_map[(unsigned char) byte] == type_space;
}
#if 0
static int
skel_is_digit(char byte) {
init_char_types();
return skel_char_map[(unsigned char) byte] == type_digit;
}
#endif
static int
skel_is_paren(char byte) {
init_char_types();
return skel_char_map[(unsigned char) byte] == type_paren;
}
static int
skel_is_name(char byte) {
init_char_types();
return skel_char_map[(unsigned char) byte] == type_name;
}
static int
check_atom(skel_t *skel, const char *data, apr_size_t len) {
return (skel
&& skel->is_atom
&& skel->len == len
&& ! memcmp(skel->data, data, len));
}
static void
put_implicit_length_byte(svn_stringbuf_t *str, char byte, char term) {
if (! skel_is_name(byte))
abort();
if (term != '\0'
&& ! skel_is_space(term)
&& ! skel_is_paren(term))
abort();
svn_stringbuf_appendbytes(str, &byte, 1);
if (term != '\0')
svn_stringbuf_appendbytes(str, &term, 1);
}
static int
check_implicit_length_byte(skel_t *skel, char byte) {
if (! skel_is_name(byte))
abort();
return check_atom(skel, &byte, 1);
}
static char *
gen_implicit_length_all_chars(apr_size_t *len_p) {
apr_size_t pos;
int i;
static char name[256];
pos = 0;
name[pos++] = 'x';
for (i = 0; i < 256; i++)
if (! skel_is_space( (apr_byte_t)i)
&& ! skel_is_paren( (apr_byte_t)i))
name[pos++] = i;
*len_p = pos;
return name;
}
static void
put_implicit_length_all_chars(svn_stringbuf_t *str, char term) {
apr_size_t len;
char *name = gen_implicit_length_all_chars(&len);
if (term != '\0'
&& ! skel_is_space(term)
&& ! skel_is_paren(term))
abort();
svn_stringbuf_appendbytes(str, name, len);
if (term != '\0')
svn_stringbuf_appendbytes(str, &term, 1);
}
static int
check_implicit_length_all_chars(skel_t *skel) {
apr_size_t len;
char *name = gen_implicit_length_all_chars(&len);
return check_atom(skel, name, len);
}
static svn_error_t *
parse_implicit_length(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
svn_stringbuf_t *str = get_empty_string(pool);
skel_t *skel;
*msg = "parse implicit-length atoms";
if (msg_only)
return SVN_NO_ERROR;
{
const char *c;
int i;
for (c = "\t\n\f\r ()[]"; *c; c++)
for (i = 0; i < 256; i++)
if (skel_is_name((apr_byte_t)i)) {
svn_stringbuf_setempty(str);
put_implicit_length_byte(str, (apr_byte_t)i, *c);
skel = parse_str(str, pool);
if (! check_implicit_length_byte(skel, (apr_byte_t)i))
return fail(pool, "single-byte implicit-length skel 0x%02x"
" with terminator 0x%02x",
i, c);
}
}
svn_stringbuf_setempty(str);
put_implicit_length_all_chars(str, '\0');
skel = parse_str(str, pool);
if (! check_implicit_length_all_chars(skel))
return fail(pool, "implicit-length skel containing all legal chars");
return SVN_NO_ERROR;
}
static void
put_explicit_length(svn_stringbuf_t *str, const char *data, apr_size_t len,
char sep) {
char *buf = malloc(len + 100);
apr_size_t length_len;
if (! skel_is_space(sep))
abort();
sprintf(buf, "%"APR_SIZE_T_FMT"%c", len, sep);
length_len = strlen(buf);
memcpy(buf + length_len, data, len);
svn_stringbuf_appendbytes(str, buf, length_len + len);
free(buf);
}
static int
check_explicit_length(skel_t *skel, const char *data, apr_size_t len) {
return check_atom(skel, data, len);
}
static svn_error_t *
try_explicit_length(const char *data, apr_size_t len, apr_size_t check_len,
apr_pool_t *pool) {
int i;
svn_stringbuf_t *str = get_empty_string(pool);
skel_t *skel;
for (i = 0; i < 256; i++)
if (skel_is_space( (apr_byte_t)i)) {
svn_stringbuf_setempty(str);
put_explicit_length(str, data, len, (apr_byte_t)i);
skel = parse_str(str, pool);
if (! check_explicit_length(skel, data, check_len))
return fail(pool, "failed to reparse explicit-length atom");
}
return SVN_NO_ERROR;
}
static svn_error_t *
parse_explicit_length(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
*msg = "parse explicit-length atoms";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(try_explicit_length("", 0, 0, pool));
{
int i;
for (i = 0; i < 256; i++) {
char buf[1];
buf[0] = i;
SVN_ERR(try_explicit_length(buf, 1, 1, pool));
}
}
{
int i;
char data[256];
for (i = 0; i < 256; i++)
data[i] = i;
SVN_ERR(try_explicit_length(data, 256, 256, pool));
}
return SVN_NO_ERROR;
}
static struct invalid_atoms {
int type;
apr_size_t len;
const char *data;
} invalid_atoms[] = { { 1, 1, "(" },
{ 1, 1, ")" },
{ 1, 1, "[" },
{ 1, 1, "]" },
{ 1, 1, " " },
{ 1, 13, "Hello, World!" },
{ 1, 8, "1mplicit" },
{ 2, 2, "1" },
{ 2, 1, "12" },
{ 7, 0, NULL }
};
static svn_error_t *
parse_invalid_atoms(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
struct invalid_atoms *ia = invalid_atoms;
*msg = "parse invalid atoms";
if (msg_only)
return SVN_NO_ERROR;
while (ia->type != 7) {
if (ia->type == 1) {
skel_t *skel = parse_cstr(ia->data, pool);
if (check_atom(skel, ia->data, ia->len))
return fail(pool,
"failed to detect parsing error in '%s'", ia->data);
} else {
svn_error_t *err = try_explicit_length(ia->data, ia->len,
strlen(ia->data), pool);
if (err == SVN_NO_ERROR)
return fail(pool, "got wrong length in explicit-length atom");
svn_error_clear(err);
}
ia++;
}
return SVN_NO_ERROR;
}
static void
put_list_start(svn_stringbuf_t *str, char space, int len) {
int i;
if (len > 0 && ! skel_is_space(space))
abort();
svn_stringbuf_appendcstr(str, "(");
for (i = 0; i < len; i++)
svn_stringbuf_appendbytes(str, &space, 1);
}
static void
put_list_end(svn_stringbuf_t *str, char space, int len) {
int i;
if (len > 0 && ! skel_is_space(space))
abort();
for (i = 0; i < len; i++)
svn_stringbuf_appendbytes(str, &space, 1);
svn_stringbuf_appendcstr(str, ")");
}
static int
check_list(skel_t *skel, int desired_len) {
int len;
skel_t *child;
if (! (skel
&& ! skel->is_atom))
return 0;
len = 0;
for (child = skel->children; child; child = child->next)
len++;
return len == desired_len;
}
static svn_error_t *
parse_list(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
*msg = "parse lists";
if (msg_only)
return SVN_NO_ERROR;
{
int list_len;
for (list_len = 0;
list_len < 30;
list_len < 4 ? list_len++ : (list_len *= 3)) {
int sep;
for (sep = 0; sep < 256; sep++)
if (skel_is_space( (apr_byte_t)sep)) {
int sep_count;
for (sep_count = 0;
sep_count < 30;
sep_count < 4 ? sep_count++ : (sep_count *= 3)) {
int atom_byte;
for (atom_byte = 0; atom_byte < 256; atom_byte++)
if (skel_is_name( (apr_byte_t)atom_byte)) {
int i;
svn_stringbuf_t *str = get_empty_string(pool);
skel_t *skel;
skel_t *child;
put_list_start(str, (apr_byte_t)sep, sep_count);
for (i = 0; i < list_len; i++)
put_implicit_length_byte(str,
(apr_byte_t)atom_byte,
(apr_byte_t)sep);
put_list_end(str, (apr_byte_t)sep, sep_count);
skel = parse_str(str, pool);
if (! check_list(skel, list_len))
return fail(pool, "couldn't parse list");
for (child = skel->children;
child;
child = child->next)
if (! check_implicit_length_byte
(child, (apr_byte_t)atom_byte))
return fail(pool,
"list was reparsed incorrectly");
}
{
int i;
svn_stringbuf_t *str = get_empty_string(pool);
skel_t *skel;
skel_t *child;
put_list_start(str, (apr_byte_t)sep, sep_count);
for (i = 0; i < list_len; i++)
put_implicit_length_all_chars(str, (apr_byte_t)sep);
put_list_end(str, (apr_byte_t)sep, sep_count);
skel = parse_str(str, pool);
if (! check_list(skel, list_len))
return fail(pool, "couldn't parse list");
for (child = skel->children;
child;
child = child->next)
if (! check_implicit_length_all_chars(child))
return fail(pool, "couldn't parse list");
}
for (atom_byte = 0; atom_byte < 256; atom_byte++) {
int i;
svn_stringbuf_t *str = get_empty_string(pool);
skel_t *skel;
skel_t *child;
char buf[1];
buf[0] = atom_byte;
put_list_start(str, (apr_byte_t)sep, sep_count);
for (i = 0; i < list_len; i++)
put_explicit_length(str, buf, 1, (apr_byte_t)sep);
put_list_end(str, (apr_byte_t)sep, sep_count);
skel = parse_str(str, pool);
if (! check_list(skel, list_len))
return fail(pool, "couldn't parse list");
for (child = skel->children;
child;
child = child->next)
if (! check_explicit_length(child, buf, 1))
return fail(pool, "list was reparsed incorrectly");
}
{
int i;
svn_stringbuf_t *str = get_empty_string(pool);
skel_t *skel;
skel_t *child;
char data[256];
for (i = 0; i < 256; i++)
data[i] = i;
put_list_start(str, (apr_byte_t)sep, sep_count);
for (i = 0; i < list_len; i++)
put_explicit_length(str, data, 256, (apr_byte_t)sep);
put_list_end(str, (apr_byte_t)sep, sep_count);
skel = parse_str(str, pool);
if (! check_list(skel, list_len))
return fail(pool, "couldn't parse list");
for (child = skel->children;
child;
child = child->next)
if (! check_explicit_length(child, data, 256))
return fail(pool, "list was re-parsed incorrectly");
}
}
}
}
}
{
int sep;
for (sep = 0; sep < 256; sep++)
if (skel_is_space( (apr_byte_t)sep)) {
int sep_count;
for (sep_count = 0;
sep_count < 100;
sep_count < 10 ? sep_count++ : (sep_count *= 3)) {
svn_stringbuf_t *str;
str = get_empty_string(pool);
put_list_start(str, (apr_byte_t)sep, sep_count);
if (parse_str(str, pool))
return fail(pool, "failed to detect syntax error");
str = get_empty_string(pool);
put_list_end(str, (apr_byte_t)sep, sep_count);
if (parse_str(str, pool))
return fail(pool, "failed to detect syntax error");
str = get_empty_string(pool);
put_list_start(str, (apr_byte_t)sep, sep_count);
svn_stringbuf_appendcstr(str, "100 ");
put_list_end(str, (apr_byte_t)sep, sep_count);
if (parse_str(str, pool))
return fail(pool, "failed to detect invalid element");
}
}
}
return SVN_NO_ERROR;
}
static skel_t *
build_atom(apr_size_t len, char *data, apr_pool_t *pool) {
char *copy = apr_palloc(pool, len);
skel_t *skel = apr_palloc(pool, sizeof(*skel));
memcpy(copy, data, len);
skel->is_atom = 1;
skel->len = len;
skel->data = copy;
return skel;
}
static skel_t *
empty(apr_pool_t *pool) {
skel_t *skel = apr_palloc(pool, sizeof(*skel));
skel->is_atom = 0;
skel->children = 0;
return skel;
}
static void
add(skel_t *element, skel_t *list) {
element->next = list->children;
list->children = element;
}
static int
skel_equal(skel_t *a, skel_t *b) {
if (a->is_atom != b->is_atom)
return 0;
if (a->is_atom)
return (a->len == b->len
&& ! memcmp(a->data, b->data, a->len));
else {
skel_t *a_child, *b_child;
for (a_child = a->children, b_child = b->children;
a_child && b_child;
a_child = a_child->next, b_child = b_child->next)
if (! skel_equal(a_child, b_child))
return 0;
if (a_child || b_child)
return 0;
}
return 1;
}
static svn_error_t *
unparse_implicit_length(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
*msg = "unparse implicit-length atoms";
if (msg_only)
return SVN_NO_ERROR;
{
int byte;
for (byte = 0; byte < 256; byte++)
if (skel_is_name( (apr_byte_t)byte)) {
svn_stringbuf_t *str = get_empty_string(pool);
char buf = (char)byte;
skel_t *skel = build_atom(1, &buf, pool);
str = svn_fs_base__unparse_skel(skel, pool);
if (! (str
&& str->len == 1
&& str->data[0] == (char)byte))
return fail(pool, "incorrectly unparsed single-byte "
"implicit-length atom");
}
}
return SVN_NO_ERROR;
}
static svn_error_t *
unparse_list(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
*msg = "unparse lists";
if (msg_only)
return SVN_NO_ERROR;
{
svn_stringbuf_t *str = get_empty_string(pool);
int byte;
skel_t *list = empty(pool);
skel_t *reparsed, *elt;
for (byte = 0; byte < 256; byte++)
if (skel_is_name( (apr_byte_t)byte)) {
char buf = byte;
add(build_atom(1, &buf, pool), list);
}
str = svn_fs_base__unparse_skel(list, pool);
reparsed = svn_fs_base__parse_skel(str->data, str->len, pool);
if (! reparsed || reparsed->is_atom)
return fail(pool, "result is syntactically misformed, or not a list");
if (! skel_equal(list, reparsed))
return fail(pool, "unparsing and parsing didn't preserve contents");
elt = reparsed->children;
for (byte = 255; byte >= 0; byte--)
if (skel_is_name( (apr_byte_t)byte)) {
if (! (elt
&& elt->is_atom
&& elt->len == 1
&& elt->data[0] == byte))
return fail(pool, "bad element");
if (elt->data < str->data
|| elt->data + elt->len > str->data + str->len)
return fail(pool, "bad element");
elt = elt->next;
}
if (elt)
return fail(pool, "list too long");
}
{
svn_stringbuf_t *str = get_empty_string(pool);
skel_t *top = empty(pool);
skel_t *reparsed;
int i;
for (i = 0; i < 10; i++) {
skel_t *middle = empty(pool);
int j;
for (j = 0; j < 10; j++) {
char buf[10];
apr_size_t k;
int val;
val = i * 10 + j;
for (k = 0; k < sizeof(buf); k++) {
buf[k] = val;
val += j;
}
add(build_atom(sizeof(buf), buf, pool), middle);
}
add(middle, top);
}
str = svn_fs_base__unparse_skel(top, pool);
reparsed = svn_fs_base__parse_skel(str->data, str->len, pool);
if (! skel_equal(top, reparsed))
return fail(pool, "failed to reparse list of lists");
}
return SVN_NO_ERROR;
}
struct svn_test_descriptor_t test_funcs[] = {
SVN_TEST_NULL,
SVN_TEST_PASS(parse_implicit_length),
SVN_TEST_PASS(parse_explicit_length),
SVN_TEST_PASS(parse_invalid_atoms),
SVN_TEST_PASS(parse_list),
SVN_TEST_PASS(unparse_implicit_length),
SVN_TEST_PASS(unparse_list),
SVN_TEST_NULL
};
