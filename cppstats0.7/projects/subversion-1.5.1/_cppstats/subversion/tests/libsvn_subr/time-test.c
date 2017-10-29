#include <stdio.h>
#include <string.h>
#include <apr_general.h>
#include "svn_time.h"
#include "../svn_test.h"
apr_time_t test_timestamp = APR_TIME_C(1021316450966679);
const char *test_timestring =
"2002-05-13T19:00:50.966679Z";
const char *test_old_timestring =
"Mon 13 May 2002 22:00:50.966679 (day 133, dst 1, gmt_off 010800)";
static svn_error_t *
test_time_to_cstring(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
const char *timestring;
*msg = "test svn_time_to_cstring";
if (msg_only)
return SVN_NO_ERROR;
timestring = svn_time_to_cstring(test_timestamp,pool);
if (strcmp(timestring,test_timestring) != 0) {
return svn_error_createf
(SVN_ERR_TEST_FAILED, NULL,
"svn_time_to_cstring (%" APR_TIME_T_FMT
") returned date string '%s' instead of '%s'",
test_timestamp,timestring,test_timestring);
}
return SVN_NO_ERROR;
}
static svn_error_t *
test_time_from_cstring(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
apr_time_t timestamp;
*msg = "test svn_time_from_cstring";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(svn_time_from_cstring(&timestamp, test_timestring, pool));
if (timestamp != test_timestamp) {
return svn_error_createf
(SVN_ERR_TEST_FAILED, NULL,
"svn_time_from_cstring (%s) returned time '%" APR_TIME_T_FMT
"' instead of '%" APR_TIME_T_FMT "'",
test_timestring,timestamp,test_timestamp);
}
return SVN_NO_ERROR;
}
static const char *failure_old_tests[] = {
"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
" 3 Oct 2000 HH:MM:SS.UUU (day 277, dst 1, gmt_off -18000)",
"Tue 3 AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
" 2000 HH:MM:SS.UUU (day 277, dst 1, gmt_off -18000)",
NULL
};
static svn_error_t *
test_time_from_cstring_old(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
apr_time_t timestamp;
const char **ft;
*msg = "test svn_time_from_cstring (old format)";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(svn_time_from_cstring(&timestamp, test_old_timestring, pool));
if (timestamp != test_timestamp) {
return svn_error_createf
(SVN_ERR_TEST_FAILED, NULL,
"svn_time_from_cstring (%s) returned time '%" APR_TIME_T_FMT
"' instead of '%" APR_TIME_T_FMT "'",
test_old_timestring,timestamp,test_timestamp);
}
for (ft = failure_old_tests; *ft; ft++) {
svn_error_t *err = svn_time_from_cstring(&timestamp, *ft, pool);
if (! err)
return svn_error_createf
(SVN_ERR_TEST_FAILED, NULL,
"svn_time_from_cstring (%s) succeeded when it should have failed",
*ft);
svn_error_clear(err);
}
return SVN_NO_ERROR;
}
static svn_error_t *
test_time_invariant(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
apr_time_t current_timestamp = apr_time_now();
const char *timestring;
apr_time_t timestamp;
*msg = "test svn_time_[to/from]_cstring() invariant";
if (msg_only)
return SVN_NO_ERROR;
timestring = svn_time_to_cstring(current_timestamp, pool);
SVN_ERR(svn_time_from_cstring(&timestamp, timestring, pool));
if (timestamp != current_timestamp) {
return svn_error_createf
(SVN_ERR_TEST_FAILED, NULL,
"svn_time_from_cstring ( svn_time_to_cstring (n) ) returned time '%"
APR_TIME_T_FMT
"' instead of '%" APR_TIME_T_FMT "'",
timestamp,current_timestamp);
}
return SVN_NO_ERROR;
}
struct date_test {
const char *str;
apr_int32_t year;
apr_int32_t mon;
apr_int32_t mday;
apr_int32_t hour;
apr_int32_t min;
apr_int32_t sec;
apr_int32_t usec;
};
static struct date_test localtz_tests[] = {
{ "2013-01-25", 2013, 1, 25, 0, 0, 0, 0 },
{ "2013-1-25", 2013, 1, 25, 0, 0, 0, 0 },
{ "2013-01-2", 2013, 1, 2, 0, 0, 0, 0 },
{ "2015-04-26T00:01:59.652655", 2015, 4, 26, 0, 1, 59, 652655 },
{ "2034-07-20T17:03:36.11379", 2034, 7, 20, 17, 3, 36, 113790 },
{ "1975-10-29T17:23:56.3859", 1975, 10, 29, 17, 23, 56, 385900 },
{ "2024-06-08T13:06:14.897", 2024, 6, 8, 13, 6, 14, 897000 },
{ "2000-01-10T05:11:11.65", 2000, 1, 10, 5, 11, 11, 650000 },
{ "2017-01-28T07:21:13.2", 2017, 1, 28, 7, 21, 13, 200000 },
{ "2013-05-18T13:52:49", 2013, 5, 18, 13, 52, 49, 0 },
{ "2020-05-14T15:28", 2020, 5, 14, 15, 28, 0, 0 },
{ "2032-05-14T7:28", 2032, 5, 14, 7, 28, 0, 0 },
{ "2020-5-14T15:28", 2020, 5, 14, 15, 28, 0, 0 },
{ "2020-05-1T15:28", 2020, 5, 1, 15, 28, 0, 0 },
{ "20100226", 2010, 2, 26, 0, 0, 0, 0 },
{ "19860214T050745.6", 1986, 2, 14, 5, 7, 45, 600000 },
{ "20230219T0045", 2023, 2, 19, 0, 45, 0, 0 },
{ "1975-07-11 06:31:49.749504", 1975, 7, 11, 6, 31, 49, 749504 },
{ "2037-05-06 00:08", 2037, 5, 6, 0, 8, 0, 0 },
{ "2037-5-6 7:01", 2037, 5, 6, 7, 1, 0, 0 },
{ "1976-02-29", 1976, 02, 29, 0, 0, 0, 0 },
{ "2000-02-29", 2000, 02, 29, 0, 0, 0, 0 },
{ NULL }
};
static struct date_test gmt_tests[] = {
{ "1979-05-05T15:16:04.39Z", 1979, 5, 5, 15, 16, 4, 390000 },
{ "2012-03-25T12:14Z", 2012, 3, 25, 12, 14, 0, 0 },
{ "1991-09-13T20:13:01.12779+02:26", 1991, 9, 13, 17, 47, 1, 127790 },
{ "1971-07-20T06:11-10", 1971, 7, 20, 16, 11, 0, 0 },
{ "20010808T105155.527Z", 2001, 8, 8, 10, 51, 55, 527000 },
{ "19781204T1322Z", 1978, 12, 4, 13, 22, 0, 0 },
{ "20030930T001647.8008-0230", 2003, 9, 30, 2, 46, 47, 800800 },
{ "19810526T1705+22", 1981, 5, 25, 19, 5, 0, 0 },
{ "2024-06-02 11:30:36 +08", 2024, 6, 2, 3, 30, 36, 0 },
{ "1994-10-07 08:08 -1457", 1994, 10, 07, 23, 5, 0, 0 },
{ NULL }
};
static struct date_test daytime_tests[] = {
{ "00:54:15.46", 0, 0, 0, 0, 54, 15, 460000 },
{ "18:21", 0, 0, 0, 18, 21, 0, 0 },
{ NULL }
};
static const char *failure_tests[] = {
"2000-00-02",
"2000-13-02",
"2000-01-32",
"2000-01-00",
"1999-02-29",
"2000-01-01 24:00:00",
"2000-01-01 00:60:00",
"2000-01-01 00:00:61",
"2000-01-01+24:00",
"2000-01-01-02:60",
"2000-01-01Z",
"2000-01-01+01:00",
"20000101Z",
"20000101-0100",
"2000-01-01T10",
"20000101T10",
"2000-01-01 10",
NULL
};
static svn_error_t *
compare_results(struct date_test *dt,
apr_time_exp_t *expt) {
if (expt->tm_year + 1900 != dt->year || expt->tm_mon + 1 != dt->mon
|| expt->tm_mday != dt->mday || expt->tm_hour != dt->hour
|| expt->tm_min != dt->min || expt->tm_sec != dt->sec
|| expt->tm_usec != dt->usec)
return svn_error_createf
(SVN_ERR_TEST_FAILED, NULL, "Comparison failed for '%s'", dt->str);
return SVN_NO_ERROR;
}
static svn_error_t *
test_parse_date(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
apr_time_t now, result;
apr_time_exp_t nowexp, expt;
svn_boolean_t matched;
struct date_test *dt;
const char **ft;
*msg = "test svn_parse_date";
if (msg_only)
return SVN_NO_ERROR;
now = apr_time_now();
if (apr_time_exp_lt(&nowexp, now) != APR_SUCCESS)
return svn_error_create(SVN_ERR_TEST_FAILED, NULL, "Can't expand time");
for (dt = localtz_tests; dt->str; dt++) {
SVN_ERR(svn_parse_date(&matched, &result, dt->str, now, pool));
if (!matched)
return svn_error_createf
(SVN_ERR_TEST_FAILED, NULL, "Match failed for '%s'", dt->str);
if (apr_time_exp_lt(&expt, result) != APR_SUCCESS)
return svn_error_createf
(SVN_ERR_TEST_FAILED, NULL, "Expand failed for '%s'", dt->str);
SVN_ERR(compare_results(dt, &expt));
}
for (dt = gmt_tests; dt->str; dt++) {
SVN_ERR(svn_parse_date(&matched, &result, dt->str, now, pool));
if (!matched)
return svn_error_createf
(SVN_ERR_TEST_FAILED, NULL, "Match failed for '%s'", dt->str);
if (apr_time_exp_gmt(&expt, result) != APR_SUCCESS)
return svn_error_createf
(SVN_ERR_TEST_FAILED, NULL, "Expand failed for '%s'", dt->str);
SVN_ERR(compare_results(dt, &expt));
}
for (dt = daytime_tests; dt->str; dt++) {
SVN_ERR(svn_parse_date(&matched, &result, dt->str, now, pool));
if (!matched)
return svn_error_createf
(SVN_ERR_TEST_FAILED, NULL, "Match failed for '%s'", dt->str);
if (apr_time_exp_lt(&expt, result) != APR_SUCCESS)
return svn_error_createf
(SVN_ERR_TEST_FAILED, NULL, "Expand failed for '%s'", dt->str);
dt->year = nowexp.tm_year + 1900;
dt->mon = nowexp.tm_mon + 1;
dt->mday = nowexp.tm_mday;
SVN_ERR(compare_results(dt, &expt));
}
for (ft = failure_tests; *ft; ft++) {
SVN_ERR(svn_parse_date(&matched, &result, *ft, now, pool));
if (matched)
return svn_error_createf
(SVN_ERR_TEST_FAILED, NULL, "Match succeeded for '%s'", *ft);
}
return SVN_NO_ERROR;
}
struct svn_test_descriptor_t test_funcs[] = {
SVN_TEST_NULL,
SVN_TEST_PASS(test_time_to_cstring),
SVN_TEST_PASS(test_time_from_cstring),
SVN_TEST_PASS(test_time_from_cstring_old),
SVN_TEST_PASS(test_time_invariant),
SVN_TEST_PASS(test_parse_date),
SVN_TEST_NULL
};