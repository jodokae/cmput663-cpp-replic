#include "../svn_test.h"
#include "../../libsvn_subr/utf_impl.h"
#include "svn_utf.h"
#include "svn_pools.h"
static apr_uint32_t diff_diff3_seed;
static apr_uint32_t
seed_val(void) {
static svn_boolean_t first = TRUE;
if (first) {
diff_diff3_seed = (apr_uint32_t) apr_time_now();
first = FALSE;
}
return diff_diff3_seed;
}
static apr_uint32_t
range_rand(apr_uint32_t min_val,
apr_uint32_t max_val) {
apr_uint64_t diff = max_val - min_val;
apr_uint64_t val = diff * svn_test_rand(&diff_diff3_seed);
val /= 0xffffffff;
return min_val + (apr_uint32_t) val;
}
static svn_error_t *
utf_validate(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
struct data {
svn_boolean_t valid;
char string[20];
} tests[] = {
{TRUE, {'a', 'b', '\0'}},
{FALSE, {'a', 'b', '\x80', '\0'}},
{FALSE, {'a', 'b', '\xC0', '\0'}},
{FALSE, {'a', 'b', '\xC0', '\x81', 'x', 'y', '\0'}},
{TRUE, {'a', 'b', '\xC5', '\x81', 'x', 'y', '\0'}},
{FALSE, {'a', 'b', '\xC5', '\xC0', 'x', 'y', '\0'}},
{FALSE, {'a', 'b', '\xE0', '\0'}},
{FALSE, {'a', 'b', '\xE0', 'x', 'y', '\0'}},
{FALSE, {'a', 'b', '\xE0', '\xA0', '\0'}},
{FALSE, {'a', 'b', '\xE0', '\xA0', 'x', 'y', '\0'}},
{TRUE, {'a', 'b', '\xE0', '\xA0', '\x81', 'x', 'y', '\0'}},
{FALSE, {'a', 'b', '\xE0', '\x9F', '\x81', 'x', 'y', '\0'}},
{FALSE, {'a', 'b', '\xE0', '\xCF', '\x81', 'x', 'y', '\0'}},
{FALSE, {'a', 'b', '\xE5', '\0'}},
{FALSE, {'a', 'b', '\xE5', 'x', 'y', '\0'}},
{FALSE, {'a', 'b', '\xE5', '\x81', '\0'}},
{FALSE, {'a', 'b', '\xE5', '\x81', 'x', 'y', '\0'}},
{TRUE, {'a', 'b', '\xE5', '\x81', '\x81', 'x', 'y', '\0'}},
{FALSE, {'a', 'b', '\xE5', '\xE1', '\x81', 'x', 'y', '\0'}},
{FALSE, {'a', 'b', '\xE5', '\x81', '\xE1', 'x', 'y', '\0'}},
{FALSE, {'a', 'b', '\xED', '\0'}},
{FALSE, {'a', 'b', '\xED', 'x', 'y', '\0'}},
{FALSE, {'a', 'b', '\xED', '\x81', '\0'}},
{FALSE, {'a', 'b', '\xED', '\x81', 'x', 'y', '\0'}},
{TRUE, {'a', 'b', '\xED', '\x81', '\x81', 'x', 'y', '\0'}},
{FALSE, {'a', 'b', '\xED', '\xA0', '\x81', 'x', 'y', '\0'}},
{FALSE, {'a', 'b', '\xED', '\x81', '\xC1', 'x', 'y', '\0'}},
{FALSE, {'a', 'b', '\xEE', '\0'}},
{FALSE, {'a', 'b', '\xEE', 'x', 'y', '\0'}},
{FALSE, {'a', 'b', '\xEE', '\x81', '\0'}},
{FALSE, {'a', 'b', '\xEE', '\x81', 'x', 'y', '\0'}},
{TRUE, {'a', 'b', '\xEE', '\x81', '\x81', 'x', 'y', '\0'}},
{TRUE, {'a', 'b', '\xEE', '\xA0', '\x81', 'x', 'y', '\0'}},
{FALSE, {'a', 'b', '\xEE', '\xC0', '\x81', 'x', 'y', '\0'}},
{FALSE, {'a', 'b', '\xEE', '\x81', '\xC1', 'x', 'y', '\0'}},
{FALSE, {'a', 'b', '\xF0', '\0'}},
{FALSE, {'a', 'b', '\xF0', 'x', 'y', '\0'}},
{FALSE, {'a', 'b', '\xF0', '\x91', '\0'}},
{FALSE, {'a', 'b', '\xF0', '\x91', 'x', 'y', '\0'}},
{FALSE, {'a', 'b', '\xF0', '\x91', '\x81', '\0'}},
{FALSE, {'a', 'b', '\xF0', '\x91', '\x81', 'x', 'y', '\0'}},
{TRUE, {'a', 'b', '\xF0', '\x91', '\x81', '\x81', 'x', 'y', '\0'}},
{FALSE, {'a', 'b', '\xF0', '\x81', '\x81', '\x81', 'x', 'y', '\0'}},
{FALSE, {'a', 'b', '\xF0', '\xC1', '\x81', '\x81', 'x', 'y', '\0'}},
{FALSE, {'a', 'b', '\xF0', '\x91', '\xC1', '\x81', 'x', 'y', '\0'}},
{FALSE, {'a', 'b', '\xF0', '\x91', '\x81', '\xC1', 'x', 'y', '\0'}},
{FALSE, {'a', 'b', '\xF2', 'x', 'y', '\0'}},
{FALSE, {'a', 'b', '\xF2', '\x91', 'x', 'y', '\0'}},
{FALSE, {'a', 'b', '\xF2', '\x91', '\x81', 'x', 'y', '\0'}},
{TRUE, {'a', 'b', '\xF2', '\x91', '\x81', '\x81', 'x', 'y', '\0'}},
{TRUE, {'a', 'b', '\xF2', '\x81', '\x81', '\x81', 'x', 'y', '\0'}},
{FALSE, {'a', 'b', '\xF2', '\xC1', '\x81', '\x81', 'x', 'y', '\0'}},
{FALSE, {'a', 'b', '\xF2', '\x91', '\xC1', '\x81', 'x', 'y', '\0'}},
{FALSE, {'a', 'b', '\xF2', '\x91', '\x81', '\xC1', 'x', 'y', '\0'}},
{FALSE, {'a', 'b', '\xF4', 'x', 'y', '\0'}},
{FALSE, {'a', 'b', '\xF4', '\x91', 'x', 'y', '\0'}},
{FALSE, {'a', 'b', '\xF4', '\x91', '\x81', 'x', 'y', '\0'}},
{FALSE, {'a', 'b', '\xF4', '\x91', '\x81', '\x81', 'x', 'y', '\0'}},
{TRUE, {'a', 'b', '\xF4', '\x81', '\x81', '\x81', 'x', 'y', '\0'}},
{FALSE, {'a', 'b', '\xF4', '\xC1', '\x81', '\x81', 'x', 'y', '\0'}},
{FALSE, {'a', 'b', '\xF4', '\x91', '\xC1', '\x81', 'x', 'y', '\0'}},
{FALSE, {'a', 'b', '\xF4', '\x91', '\x81', '\xC1', 'x', 'y', '\0'}},
{FALSE, {'a', 'b', '\xF5', 'x', 'y', '\0'}},
{FALSE, {'a', 'b', '\xF5', '\x81', 'x', 'y', '\0'}},
{
TRUE, {
'a', 'b', '\xF4', '\x81', '\x81', '\x81', 'x', 'y',
'a', 'b', '\xF2', '\x91', '\x81', '\x81', 'x', 'y', '\0'
}
},
{
FALSE, {
'a', 'b', '\xF4', '\x81', '\x81', '\x81', 'x', 'y',
'a', 'b', '\xF2', '\x91', '\x81', '\xC1', 'x', 'y', '\0'
}
},
{
FALSE, {
'a', 'b', '\xF4', '\x81', '\x81', '\x81', 'x', 'y',
'a', 'b', '\xF2', '\x91', '\x81', 'x', 'y', '\0'
}
},
{-1},
};
int i = 0;
*msg = "test is_valid/last_valid";
if (msg_only)
return SVN_NO_ERROR;
while (tests[i].valid != -1) {
const char *last = svn_utf__last_valid(tests[i].string,
strlen(tests[i].string));
apr_size_t len = strlen(tests[i].string);
if ((svn_utf__cstring_is_valid(tests[i].string) != tests[i].valid)
||
(svn_utf__is_valid(tests[i].string, len) != tests[i].valid))
return svn_error_createf
(SVN_ERR_TEST_FAILED, NULL, "is_valid test %d failed", i);
if (!svn_utf__is_valid(tests[i].string, last - tests[i].string)
||
(tests[i].valid && *last))
return svn_error_createf
(SVN_ERR_TEST_FAILED, NULL, "last_valid test %d failed", i);
++i;
}
return SVN_NO_ERROR;
}
static svn_error_t *
utf_validate2(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
int i;
*msg = apr_psprintf(pool,
"test last_valid/last_valid2 (seed:%u)", seed_val());
if (msg_only)
return SVN_NO_ERROR;
for (i = 0; i < 100000; ++i) {
unsigned int j;
char str[64];
apr_size_t len;
for (j = 0; j < sizeof(str) - 1; ++j)
str[j] = range_rand(0, 255);
str[sizeof(str) - 1] = 0;
len = strlen(str);
if (svn_utf__last_valid(str, len) != svn_utf__last_valid2(str, len)) {
svn_utf__last_valid(str, len);
svn_utf__last_valid2(str, len);
return svn_error_createf
(SVN_ERR_TEST_FAILED, NULL, "is_valid2 test %d failed", i);
}
}
return SVN_NO_ERROR;
}
static svn_error_t *
test_utf_cstring_to_utf8_ex2(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
apr_size_t i;
apr_pool_t *subpool = svn_pool_create(pool);
struct data {
const char *string;
const char *expected_result;
const char *from_page;
} tests[] = {
{"ascii text\n", "ascii text\n", "unexistant-page"},
{"Edelwei\xdf", "Edelwei\xc3\x9f", "ISO-8859-1"}
};
*msg = "test svn_utf_cstring_to_utf8_ex2";
if (msg_only)
return SVN_NO_ERROR;
for (i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
const char *dest;
svn_pool_clear(subpool);
SVN_ERR(svn_utf_cstring_to_utf8_ex2(&dest, tests[i].string,
tests[i].from_page, pool));
if (strcmp(dest, tests[i].expected_result)) {
return svn_error_createf
(SVN_ERR_TEST_FAILED, NULL,
"svn_utf_cstring_to_utf8_ex2 ('%s', '%s') returned ('%s') "
"instead of ('%s')",
tests[i].string, tests[i].from_page,
dest,
tests[i].expected_result);
}
}
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
static svn_error_t *
test_utf_cstring_from_utf8_ex2(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
apr_size_t i;
apr_pool_t *subpool = svn_pool_create(pool);
struct data {
const char *string;
const char *expected_result;
const char *to_page;
} tests[] = {
{"ascii text\n", "ascii text\n", "unexistant-page"},
{"Edelwei\xc3\x9f", "Edelwei\xdf", "ISO-8859-1"}
};
*msg = "test svn_utf_cstring_from_utf8_ex2";
if (msg_only)
return SVN_NO_ERROR;
for (i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
const char *dest;
svn_pool_clear(subpool);
SVN_ERR(svn_utf_cstring_from_utf8_ex2(&dest, tests[i].string,
tests[i].to_page, pool));
if (strcmp(dest, tests[i].expected_result)) {
return svn_error_createf
(SVN_ERR_TEST_FAILED, NULL,
"svn_utf_cstring_from_utf8_ex2 ('%s', '%s') returned ('%s') "
"instead of ('%s')",
tests[i].string, tests[i].to_page,
dest,
tests[i].expected_result);
}
}
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
struct svn_test_descriptor_t test_funcs[] = {
SVN_TEST_NULL,
SVN_TEST_PASS(utf_validate),
SVN_TEST_PASS(utf_validate2),
SVN_TEST_PASS(test_utf_cstring_to_utf8_ex2),
SVN_TEST_PASS(test_utf_cstring_from_utf8_ex2),
SVN_TEST_NULL
};