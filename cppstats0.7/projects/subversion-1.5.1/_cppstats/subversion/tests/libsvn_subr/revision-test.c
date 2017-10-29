#include "svn_types.h"
#include "../svn_test.h"
static svn_error_t *
test_revnum_parse(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
const char **t;
const char *failure_tests[] = {
"",
"abc",
"-456",
NULL
};
const char *success_tests[] = {
"0",
"12345",
"12345ABC",
NULL
};
*msg = "test svn_revnum_parse";
for (t=success_tests; *t; ++t) {
svn_revnum_t rev = -123;
const char *endptr;
SVN_ERR(svn_revnum_parse(&rev, *t, NULL));
SVN_ERR(svn_revnum_parse(&rev, *t, &endptr));
if (-123 == rev)
return svn_error_createf
(SVN_ERR_TEST_FAILED,
NULL,
"svn_revnum_parse('%s') should change the revision for "
"a good string",
*t);
if (endptr == *t)
return svn_error_createf
(SVN_ERR_TEST_FAILED,
NULL,
"End pointer for svn_revnum_parse('%s') should not "
"point to the start of the string",
*t);
}
for (t=failure_tests; *t; ++t) {
svn_revnum_t rev = -123;
const char *endptr;
svn_error_t *err = svn_revnum_parse(&rev, *t, NULL);
svn_error_clear(err);
err = svn_revnum_parse(&rev, *t, &endptr);
if (! err)
return svn_error_createf
(SVN_ERR_TEST_FAILED, NULL,
"svn_revnum_parse('%s') succeeded when it should "
"have failed",
*t);
svn_error_clear(err);
if (-123 != rev)
return svn_error_createf
(SVN_ERR_TEST_FAILED,
NULL,
"svn_revnum_parse('%s') should not change the revision "
"for a bad string",
*t);
if (endptr != *t)
return svn_error_createf
(SVN_ERR_TEST_FAILED,
NULL,
"End pointer for svn_revnum_parse('%s') does not "
"point to the start of the string",
*t);
}
return SVN_NO_ERROR;
}
struct svn_test_descriptor_t test_funcs[] = {
SVN_TEST_NULL,
SVN_TEST_PASS(test_revnum_parse),
SVN_TEST_NULL
};