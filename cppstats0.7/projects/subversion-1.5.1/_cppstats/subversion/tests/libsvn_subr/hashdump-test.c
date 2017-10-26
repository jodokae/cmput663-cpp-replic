#include <stdio.h>
#include <stdlib.h>
#include <apr_pools.h>
#include <apr_hash.h>
#include <apr_file_io.h>
#include "svn_types.h"
#include "svn_string.h"
#include "svn_error.h"
#include "svn_hash.h"
#include "../svn_test.h"
apr_hash_t *proplist, *new_proplist;
const char *review =
"A forthright entrance, yet coquettish on the tongue, its deceptively\n"
"fruity exterior hides the warm mahagony undercurrent that is the\n"
"hallmark of Chateau Fraisant-Pitre. Connoisseurs of the region will\n"
"be pleased to note the familiar, subtle hints of mulberries and\n"
"carburator fluid. Its confident finish is marred only by a barely\n"
"detectable suggestion of rancid squid ink.";
static svn_error_t *
test1(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
svn_error_t *result;
svn_stringbuf_t *key;
apr_file_t *f;
*msg = "write a hash to a file";
if (msg_only)
return SVN_NO_ERROR;
proplist = apr_hash_make(pool);
key = svn_stringbuf_create("color", pool);
apr_hash_set(proplist, key->data, key->len,
svn_string_create("red", pool));
key = svn_stringbuf_create("wine review", pool);
apr_hash_set(proplist, key->data, key->len,
svn_string_create(review, pool));
key = svn_stringbuf_create("price", pool);
apr_hash_set(proplist, key->data, key->len,
svn_string_create("US $6.50", pool));
key = svn_stringbuf_create("twice-used property name", pool);
apr_hash_set(proplist, key->data, key->len,
svn_string_create("This is the FIRST value.", pool));
apr_hash_set(proplist, key->data, key->len,
svn_string_create("This is the SECOND value.", pool));
apr_file_open(&f, "hashdump.out",
(APR_WRITE | APR_CREATE),
APR_OS_DEFAULT, pool);
result = svn_hash_write2(proplist, svn_stream_from_aprfile(f, pool),
SVN_HASH_TERMINATOR, pool);
apr_file_close(f);
return result;
}
static svn_error_t *
test2(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
svn_error_t *result;
apr_file_t *f;
*msg = "read a file into a hash";
if (msg_only)
return SVN_NO_ERROR;
new_proplist = apr_hash_make(pool);
apr_file_open(&f, "hashdump.out", APR_READ, APR_OS_DEFAULT, pool);
result = svn_hash_read2(new_proplist, svn_stream_from_aprfile(f, pool),
SVN_HASH_TERMINATOR, pool);
apr_file_close(f);
return result;
}
static svn_error_t *
test3(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
apr_hash_index_t *this;
svn_error_t *err;
int found_discrepancy = 0;
const char *ignored;
*msg = "write hash out, read back in, compare";
if (msg_only)
return SVN_NO_ERROR;
err = test1(&ignored, FALSE, opts, pool);
if (err)
return err;
err = test2(&ignored, FALSE, opts, pool);
if (err)
return err;
for (this = apr_hash_first(pool, proplist);
this;
this = apr_hash_next(this)) {
const void *key;
apr_ssize_t keylen;
void *val;
svn_string_t *orig_str, *new_str;
apr_hash_this(this, &key, &keylen, &val);
orig_str = val;
new_str = apr_hash_get(new_proplist, key, keylen);
if (new_str == NULL)
found_discrepancy = 1;
else if (! svn_string_compare(orig_str, new_str))
found_discrepancy = 1;
}
if (found_discrepancy)
return svn_error_createf(SVN_ERR_TEST_FAILED, 0,
"found discrepancy reading back hash table");
return SVN_NO_ERROR;
}
struct svn_test_descriptor_t test_funcs[] = {
SVN_TEST_NULL,
SVN_TEST_PASS(test1),
SVN_TEST_PASS(test2),
SVN_TEST_PASS(test3),
SVN_TEST_NULL
};
