#include <stdio.h>
#include <string.h>
#include <apr_hash.h>
#include <apr_tables.h>
#include "svn_pools.h"
#include "svn_types.h"
#include "svn_mergeinfo.h"
#include "private/svn_mergeinfo_private.h"
#include "../svn_test.h"
static svn_error_t *
fail(apr_pool_t *pool, const char *fmt, ...) {
va_list ap;
char *msg;
va_start(ap, fmt);
msg = apr_pvsprintf(pool, fmt, ap);
va_end(ap);
return svn_error_create(SVN_ERR_TEST_FAILED, 0, msg);
}
#define MAX_NBR_RANGES 3
static svn_error_t *
verify_mergeinfo_parse(const char *input,
const char *expected_path,
const svn_merge_range_t *expected_ranges,
apr_pool_t *pool) {
svn_error_t *err;
apr_hash_t *path_to_merge_ranges;
apr_hash_index_t *hi;
err = svn_mergeinfo_parse(&path_to_merge_ranges, input, pool);
if (err || apr_hash_count(path_to_merge_ranges) != 1)
return svn_error_createf(SVN_ERR_TEST_FAILED, err,
"svn_mergeinfo_parse (%s) failed unexpectedly",
input);
for (hi = apr_hash_first(pool, path_to_merge_ranges); hi;
hi = apr_hash_next(hi)) {
const void *path;
void *val;
apr_array_header_t *ranges;
svn_merge_range_t *range;
int j;
apr_hash_this(hi, &path, NULL, &val);
ranges = val;
if (strcmp((const char *) path, expected_path) != 0)
return fail(pool, "svn_mergeinfo_parse (%s) failed to parse the "
"correct path (%s)", input, expected_path);
for (j = 0; j < ranges->nelts; j++) {
range = APR_ARRAY_IDX(ranges, j, svn_merge_range_t *);
if (range->start != expected_ranges[j].start
|| range->end != expected_ranges[j].end
|| range->inheritable != expected_ranges[j].inheritable)
return svn_error_createf(SVN_ERR_TEST_FAILED, NULL,
"svn_mergeinfo_parse (%s) failed to "
"parse the correct range",
input);
}
if (j < MAX_NBR_RANGES - 1
&& !expected_ranges[j].end == 0)
return svn_error_createf(SVN_ERR_TEST_FAILED, NULL,
"svn_mergeinfo_parse (%s) failed to "
"produce the expected number of ranges",
input);
}
return SVN_NO_ERROR;
}
static apr_hash_t *info1, *info2;
#define NBR_MERGEINFO_VALS 5
static const char * const mergeinfo_vals[NBR_MERGEINFO_VALS] = {
"/trunk:1",
"/trunk/foo:1-6",
"/trunk: 5,7-9,10,11,13,14",
"/trunk: 3-10,11*,13,14",
"/branch: 1,2-18*,33*"
};
static const char * const mergeinfo_paths[NBR_MERGEINFO_VALS] = {
"/trunk",
"/trunk/foo",
"/trunk",
"/trunk",
"/branch"
};
static svn_merge_range_t mergeinfo_ranges[NBR_MERGEINFO_VALS][MAX_NBR_RANGES] = {
{ {0, 1, TRUE} },
{ {0, 6, TRUE} },
{ {4, 5, TRUE}, { 6, 11, TRUE }, {12, 14, TRUE } },
{ {2, 10, TRUE}, {10, 11, FALSE}, {12, 14, TRUE } },
{ {0, 1, TRUE}, { 1, 18, FALSE}, {32, 33, FALSE} }
};
static svn_error_t *
test_parse_single_line_mergeinfo(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
int i;
*msg = "parse single line mergeinfo";
if (msg_only)
return SVN_NO_ERROR;
for (i = 0; i < NBR_MERGEINFO_VALS; i++)
SVN_ERR(verify_mergeinfo_parse(mergeinfo_vals[i], mergeinfo_paths[i],
mergeinfo_ranges[i], pool));
return SVN_NO_ERROR;
}
static const char *single_mergeinfo = "/trunk: 5,7-9,10,11,13,14";
static svn_error_t *
test_mergeinfo_dup(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
apr_hash_t *orig_mergeinfo, *copied_mergeinfo;
apr_pool_t *subpool;
apr_array_header_t *rangelist;
*msg = "copy a mergeinfo data structure";
if (msg_only)
return SVN_NO_ERROR;
subpool = svn_pool_create(pool);
orig_mergeinfo = apr_hash_make(subpool);
copied_mergeinfo = svn_mergeinfo_dup(orig_mergeinfo, subpool);
if (apr_hash_count(copied_mergeinfo) != 0)
return fail(pool, "Copied mergeinfo should be empty");
SVN_ERR(svn_mergeinfo_parse(&orig_mergeinfo, single_mergeinfo, subpool));
copied_mergeinfo = svn_mergeinfo_dup(orig_mergeinfo, pool);
svn_pool_destroy(subpool);
if (apr_hash_count(copied_mergeinfo) != 1)
return fail(pool, "Copied mergeinfo should contain one merge source");
rangelist = apr_hash_get(copied_mergeinfo, "/trunk", APR_HASH_KEY_STRING);
if (! rangelist)
return fail(pool, "Expected copied mergeinfo; got nothing");
if (rangelist->nelts != 3)
return fail(pool, "Copied mergeinfo should contain 3 revision ranges, "
"rather than the %d it contains", rangelist->nelts);
return SVN_NO_ERROR;
}
static svn_error_t *
test_parse_combine_rangeinfo(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
apr_array_header_t *result;
svn_merge_range_t *resultrange;
*msg = "parse single line mergeinfo and combine ranges";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(svn_mergeinfo_parse(&info1, single_mergeinfo, pool));
if (apr_hash_count(info1) != 1)
return fail(pool, "Wrong number of paths in parsed mergeinfo");
result = apr_hash_get(info1, "/trunk", APR_HASH_KEY_STRING);
if (!result)
return fail(pool, "Missing path in parsed mergeinfo");
if (result->nelts != 3)
return fail(pool, "Parsing failed to combine ranges");
resultrange = APR_ARRAY_IDX(result, 0, svn_merge_range_t *);
if (resultrange->start != 4 || resultrange->end != 5)
return fail(pool, "Range combining produced wrong result");
resultrange = APR_ARRAY_IDX(result, 1, svn_merge_range_t *);
if (resultrange->start != 6 || resultrange->end != 11)
return fail(pool, "Range combining produced wrong result");
resultrange = APR_ARRAY_IDX(result, 2, svn_merge_range_t *);
if (resultrange->start != 12 || resultrange->end != 14)
return fail(pool, "Range combining produced wrong result");
return SVN_NO_ERROR;
}
#define NBR_BROKEN_MERGEINFO_VALS 38
static const char * const broken_mergeinfo_vals[NBR_BROKEN_MERGEINFO_VALS] = {
"/missing-revs",
"/trunk: 5,7-9,10,11,13,14,",
"/trunk 5,7-9,10,11,13,14",
"/trunk:5 7--9 10 11 13 14",
"/trunk:3-6,15,18,9,22",
"/trunk:5,3",
"/trunk:3-6*,15*,18*,9,22*",
"/trunk:5,3*",
"/trunk:5-9*,9",
"/trunk:5,5-9*",
"/trunk:5-9,9*",
"/trunk:5*,5-9",
"/trunk:4,4*",
"/trunk:4*,4",
"/trunk:3-7*,4-23",
"/trunk:3-7,4-23*",
"/trunk:5-9*,9*",
"/trunk:5*,5-9*",
"/trunk:5-9,9",
"/trunk:5,5-9",
"/trunk:4,4",
"/trunk:4*,4*",
"/trunk:3-7,4-23",
"/trunk:3-7*,4-23*",
"/trunk:22-20",
"/trunk:22-20*",
"/trunk:3,7-12,22-20,25",
"/trunk:3,7,22-20*,25-30",
"/trunk:22-22",
"/trunk:22-22*",
"/trunk:3,7-12,20-20,25",
"/trunk:3,7,20-20*,25-30",
"/trunk:",
"/trunk:2-9\n/branch:",
":1-3",
"trunk:a-3",
"branch:3-four",
"trunk:yadayadayada"
};
static svn_error_t *
test_parse_broken_mergeinfo(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
int i;
svn_error_t *err;
*msg = "parse broken single line mergeinfo";
if (msg_only)
return SVN_NO_ERROR;
for (i = 0; i < NBR_BROKEN_MERGEINFO_VALS; i++) {
err = svn_mergeinfo_parse(&info1, broken_mergeinfo_vals[i], pool);
if (err == SVN_NO_ERROR) {
return fail(pool, "svn_mergeinfo_parse (%s) failed to detect an error",
broken_mergeinfo_vals[i]);
} else if (err->apr_err != SVN_ERR_MERGEINFO_PARSE_ERROR) {
svn_error_clear(err);
return fail(pool, "svn_mergeinfo_parse (%s) returned some error other"
" than SVN_ERR_MERGEINFO_PARSE_ERROR",
broken_mergeinfo_vals[i]);
} else {
svn_error_clear(err);
}
}
return SVN_NO_ERROR;
}
static const char *mergeinfo1 = "/trunk: 3,5,7-9,10,11,13,14\n/fred:8-10";
#define NBR_RANGELIST_DELTAS 4
static char *
range_to_string(svn_merge_range_t *range,
apr_pool_t *pool) {
if (range->start == range->end - 1)
return apr_psprintf(pool, "%ld%s", range->end,
range->inheritable
? "" : SVN_MERGEINFO_NONINHERITABLE_STR);
else
return apr_psprintf(pool, "%ld-%ld%s", range->start + 1,
range->end, range->inheritable
? "" : SVN_MERGEINFO_NONINHERITABLE_STR);
}
static svn_error_t *
verify_ranges_match(apr_array_header_t *actual_rangelist,
svn_merge_range_t *expected_ranges, int nbr_expected,
const char *func_verified, const char *type,
apr_pool_t *pool) {
int i;
if (actual_rangelist->nelts != nbr_expected)
return fail(pool, "%s should report %d range %ss, but found %d",
func_verified, nbr_expected, type, actual_rangelist->nelts);
for (i = 0; i < actual_rangelist->nelts; i++) {
svn_merge_range_t *range = APR_ARRAY_IDX(actual_rangelist, i,
svn_merge_range_t *);
if (range->start != expected_ranges[i].start
|| range->end != expected_ranges[i].end
|| range->inheritable != expected_ranges[i].inheritable)
return fail(pool, "%s should report range %s, but found %s",
func_verified,
range_to_string(&expected_ranges[i], pool),
range_to_string(range, pool));
}
return SVN_NO_ERROR;
}
static svn_error_t *
verify_mergeinfo_deltas(apr_hash_t *deltas, svn_merge_range_t *expected_deltas,
const char *func_verified, const char *type,
apr_pool_t *pool) {
apr_array_header_t *rangelist;
if (apr_hash_count(deltas) != 1)
return fail(pool, "%s should report 1 path %s, but found %d",
func_verified, type, apr_hash_count(deltas));
rangelist = apr_hash_get(deltas, "/trunk", APR_HASH_KEY_STRING);
if (rangelist == NULL)
return fail(pool, "%s failed to produce a rangelist for /trunk",
func_verified);
return verify_ranges_match(rangelist, expected_deltas, NBR_RANGELIST_DELTAS,
func_verified, type, pool);
}
static svn_error_t *
test_diff_mergeinfo(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
apr_hash_t *deleted, *added, *from, *to;
svn_merge_range_t expected_rangelist_deletions[NBR_RANGELIST_DELTAS] =
{ {6, 7, TRUE}, {8, 9, TRUE}, {10, 11, TRUE}, {32, 34, TRUE} };
svn_merge_range_t expected_rangelist_additions[NBR_RANGELIST_DELTAS] =
{ {1, 2, TRUE}, {4, 6, TRUE}, {12, 16, TRUE}, {29, 30, TRUE} };
*msg = "diff of mergeinfo";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(svn_mergeinfo_parse(&from, "/trunk: 1,3-4,7,9,11-12,31-34", pool));
SVN_ERR(svn_mergeinfo_parse(&to, "/trunk: 1-6,12-16,30-32", pool));
SVN_ERR(svn_mergeinfo_diff(&deleted, &added, from, to,
FALSE, pool));
SVN_ERR(verify_mergeinfo_deltas(deleted, expected_rangelist_deletions,
"svn_mergeinfo_diff", "deletion", pool));
SVN_ERR(verify_mergeinfo_deltas(added, expected_rangelist_additions,
"svn_mergeinfo_diff", "addition", pool));
return SVN_NO_ERROR;
}
static svn_error_t *
test_rangelist_reverse(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
apr_array_header_t *rangelist;
svn_merge_range_t expected_rangelist[3] =
{ {10, 9, TRUE}, {7, 4, TRUE}, {3, 2, TRUE} };
*msg = "reversal of rangelist";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(svn_mergeinfo_parse(&info1, "/trunk: 3,5-7,10", pool));
rangelist = apr_hash_get(info1, "/trunk", APR_HASH_KEY_STRING);
SVN_ERR(svn_rangelist_reverse(rangelist, pool));
return verify_ranges_match(rangelist, expected_rangelist, 3,
"svn_rangelist_reverse", "reversal", pool);
}
static svn_error_t *
test_rangelist_intersect(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
apr_array_header_t *rangelist1, *rangelist2, *intersection;
svn_merge_range_t expected_intersection[] = {
{0, 1, TRUE}, {2, 4, TRUE}, {11, 12, TRUE}, {30, 32, TRUE},
{39, 42, TRUE}
};
*msg = "intersection of rangelists";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(svn_mergeinfo_parse(&info1, "/trunk: 1-6,12-16,30-32,40-42", pool));
SVN_ERR(svn_mergeinfo_parse(&info2, "/trunk: 1,3-4,7,9,11-12,31-34,38-44",
pool));
rangelist1 = apr_hash_get(info1, "/trunk", APR_HASH_KEY_STRING);
rangelist2 = apr_hash_get(info2, "/trunk", APR_HASH_KEY_STRING);
SVN_ERR(svn_rangelist_intersect(&intersection, rangelist1, rangelist2,
TRUE, pool));
return verify_ranges_match(intersection, expected_intersection, 5,
"svn_rangelist_intersect", "intersect", pool);
}
static svn_error_t *
test_mergeinfo_intersect(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
svn_merge_range_t expected_intersection[3] =
{ {0, 1, TRUE}, {2, 4, TRUE}, {11, 12, TRUE} };
apr_array_header_t *rangelist;
apr_hash_t *intersection;
*msg = "intersection of mergeinfo";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(svn_mergeinfo_parse(&info1, "/trunk: 1-6,12-16\n/foo: 31", pool));
SVN_ERR(svn_mergeinfo_parse(&info2, "/trunk: 1,3-4,7,9,11-12", pool));
SVN_ERR(svn_mergeinfo_intersect(&intersection, info1, info2, pool));
if (apr_hash_count(intersection) != 1)
return fail(pool, "Unexpected number of rangelists in mergeinfo "
"intersection: Expected %d, found %d", 1,
apr_hash_count(intersection));
rangelist = apr_hash_get(intersection, "/trunk", APR_HASH_KEY_STRING);
return verify_ranges_match(rangelist, expected_intersection, 3,
"svn_rangelist_intersect", "intersect", pool);
}
static svn_error_t *
test_merge_mergeinfo(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
int i;
#define NBR_MERGEINFO_MERGES 12
#define MAX_NBR_MERGEINFO_PATHS 4
#define MAX_NBR_MERGEINFO_RANGES 10
struct mergeinfo_merge_path_range {
const char *path;
svn_merge_range_t expected_rngs[MAX_NBR_MERGEINFO_RANGES];
};
struct mergeinfo_merge_test_data {
const char *mergeinfo1;
const char *mergeinfo2;
int expected_paths;
struct mergeinfo_merge_path_range path_rngs[MAX_NBR_MERGEINFO_PATHS];
};
static struct mergeinfo_merge_test_data mergeinfo[NBR_MERGEINFO_MERGES] = {
{
"/trunk: 5-10",
"/trunk: 6", 1,
{ {"/trunk", { {4, 10, TRUE} } } }
},
{
"/trunk: 5-10*",
"/trunk: 6*", 1,
{ {"/trunk", { {4, 10, FALSE} } } }
},
{
"/trunk: 5-10",
"/trunk: 6*", 1,
{ {"/trunk", { {4, 10, TRUE} } } }
},
{
"/trunk: 5-10*",
"/trunk: 6", 1,
{ {"/trunk", { {4, 5, FALSE}, {5, 6, TRUE}, {6, 10, FALSE} } } }
},
{
"/trunk: 1,3,5-11,13",
"/trunk: 2,4,12,14-22", 1,
{ {"/trunk", { {0, 22, TRUE} } } }
},
{
"/trunk: 1*,3*,5-11*,13*",
"/trunk: 2*,4*,12*,14-22*", 1,
{ {"/trunk", { {0, 22, FALSE} } } }
},
{
"/trunk: 1*,3*,5-11*,13*",
"/trunk: 2,4,12,14-22", 1,
{ {
"/trunk", { { 0, 1, FALSE}, { 1, 2, TRUE},
{ 2, 3, FALSE}, { 3, 4, TRUE},
{ 4, 11, FALSE}, {11, 12, TRUE},
{12, 13, FALSE}, {13, 22, TRUE}
}
}
}
},
{
"/trunk: 1,3,5-11,13",
"/trunk: 2*,4*,12*,14-22*", 1,
{ {
"/trunk", { { 0, 1, TRUE}, { 1, 2, FALSE},
{ 2, 3, TRUE}, { 3, 4, FALSE},
{ 4, 11, TRUE}, {11, 12, FALSE},
{12, 13, TRUE}, {13, 22, FALSE}
}
}
}
},
{
"/trunk: 3,5,7-9,10,11,13,14\n/fred:8-10",
"/trunk: 1-4,6\n/fred:9-12", 2,
{ {"/trunk", { {0, 11, TRUE}, {12, 14, TRUE} } },
{"/fred", { {7, 12, TRUE} } }
}
},
{
"/trunk: 3*,5*,7-9*,10*,11*,13*,14*\n/fred:8-10*",
"/trunk: 1-4*,6*\n/fred:9-12*", 2,
{ {"/trunk", { {0, 11, FALSE}, {12, 14, FALSE} } },
{"/fred", { {7, 12, FALSE} } }
}
},
{
"/trunk: 3,5*,7-9,10,11*,13,14\n/fred:8-10",
"/trunk: 1-4,6\n/fred:9-12*", 2,
{ {
"/trunk", { { 0, 4, TRUE }, { 4, 5, FALSE}, {5, 10, TRUE},
{10, 11, FALSE}, {12, 14, TRUE }
}
},
{"/fred", { { 7, 10, TRUE }, {10, 12, FALSE} } }
}
},
{
"/trunk: 3,5-9*\n/betty: 2-4",
"/fred: 1-18\n/barney: 1,3-43", 4,
{ {"/trunk", { {2, 3, TRUE}, {4, 9, FALSE} } },
{"/betty", { {1, 4, TRUE} } },
{"/barney", { {0, 1, TRUE}, {2, 43, TRUE} } },
{"/fred", { {0, 18, TRUE} } }
}
}
};
*msg = "merging of mergeinfo hashs";
if (msg_only)
return SVN_NO_ERROR;
for (i = 0; i < NBR_MERGEINFO_MERGES; i++) {
int j;
SVN_ERR(svn_mergeinfo_parse(&info1, mergeinfo[i].mergeinfo1, pool));
SVN_ERR(svn_mergeinfo_parse(&info2, mergeinfo[i].mergeinfo2, pool));
SVN_ERR(svn_mergeinfo_merge(info1, info2, pool));
if (mergeinfo[i].expected_paths != apr_hash_count(info1))
return fail(pool, "Wrong number of paths in merged mergeinfo");
for (j = 0; j < mergeinfo[i].expected_paths; j++) {
int k;
apr_array_header_t *rangelist =
apr_hash_get(info1, mergeinfo[i].path_rngs[j].path,
APR_HASH_KEY_STRING);
if (!rangelist)
return fail(pool, "Missing path '%s' in merged mergeinfo",
mergeinfo[i].path_rngs->path);
for (k = 0; k < rangelist->nelts; k++) {
svn_merge_range_t *ranges =
APR_ARRAY_IDX(rangelist, k, svn_merge_range_t *);
if (ranges->start
!= mergeinfo[i].path_rngs[j].expected_rngs[k].start
|| ranges->end
!= mergeinfo[i].path_rngs[j].expected_rngs[k].end
|| ranges->inheritable
!= mergeinfo[i].path_rngs[j].expected_rngs[k].inheritable)
return fail(
pool,
"Range'%i-%i%s' not found in merged mergeinfo",
mergeinfo[i].path_rngs->expected_rngs[k].start,
mergeinfo[i].path_rngs->expected_rngs[k].end,
mergeinfo[i].path_rngs->expected_rngs[k].inheritable
? "" : "*");
}
if (k < MAX_NBR_MERGEINFO_RANGES
&& mergeinfo[i].path_rngs[j].expected_rngs[k].start != 0)
return fail(pool,
"Not all expected ranges found in merged mergeinfo");
}
}
return SVN_NO_ERROR;
}
static svn_error_t *
test_remove_rangelist(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
int i, j;
svn_error_t *err, *child_err;
apr_array_header_t *output, *eraser, *whiteboard;
struct rangelist_remove_test_data {
const char *whiteboard;
const char *eraser;
int expected_ranges_consider_inheritance;
svn_merge_range_t expected_removed_consider_inheritance[10];
int expected_ranges_ignore_inheritance;
svn_merge_range_t expected_removed_ignore_inheritance[10];
};
#define SIZE_OF_RANGE_REMOVE_TEST_ARRAY 15
struct rangelist_remove_test_data test_data[SIZE_OF_RANGE_REMOVE_TEST_ARRAY] = {
{
"/A: 1-44", "/A: 5", 2, { {0, 4, TRUE }, {5, 44, TRUE }},
2, { {0, 4, TRUE }, {5, 44, TRUE }}
},
{
"/A: 1-44*", "/A: 5", 1, { {0, 44, FALSE} },
2, { {0, 4, FALSE}, {5, 44, FALSE}}
},
{
"/A: 1-44", "/A: 5*", 1, { {0, 44, TRUE } },
2, { {0, 4, TRUE }, {5, 44, TRUE }}
},
{
"/A: 1-44*", "/A: 5*", 2, { {0, 4, FALSE}, {5, 44, FALSE}},
2, { {0, 4, FALSE}, {5, 44, FALSE}}
},
{
"/A: 2-9,14-19", "/A: 12", 2, { {1, 9, TRUE }, {13, 19, TRUE }},
2, { {1, 9, TRUE }, {13, 19, TRUE }}
},
{
"/A: 2-9*,14-19*", "/A: 12", 2, { {1, 9, FALSE}, {13, 19, FALSE}},
2, { {1, 9, FALSE}, {13, 19, FALSE}}
},
{
"/A: 2-9,14-19", "/A: 12*", 2, { {1, 9, TRUE }, {13, 19, TRUE }},
2, { {1, 9, TRUE }, {13, 19, TRUE }}
},
{
"/A: 2-9*,14-19*", "/A: 12*", 2, { {1, 9, FALSE}, {13, 19, FALSE}},
2, { {1, 9, FALSE}, {13, 19, FALSE}}
},
{
"/A: 1,9-17", "/A: 12-20", 2, { {0, 1, TRUE }, {8, 11, TRUE }},
2, { {0, 1, TRUE }, {8, 11, TRUE }}
},
{
"/A: 1,9-17*", "/A: 12-20", 2, { {0, 1, TRUE }, {8, 17, FALSE}},
2, { {0, 1, TRUE }, {8, 11, FALSE}}
},
{
"/A: 1,9-17", "/A: 12-20*", 2, { {0, 1, TRUE }, {8, 17, TRUE }},
2, { {0, 1, TRUE }, {8, 11, TRUE }}
},
{
"/A: 1,9-17*", "/A: 12-20*", 2, { {0, 1, TRUE }, {8, 11, FALSE}},
2, { {0, 1, TRUE }, {8, 11, FALSE}}
},
{
"", "", 0, { {0, 0, FALSE}},
0, { {0, 0, FALSE}}
},
{
"", "/A: 5-8,10-100", 0, { {0, 0, FALSE}},
0, { {0, 0, FALSE}}
},
{
"/A: 5-8,10-100", "", 2, { {4, 8, TRUE }, {9, 100, TRUE }},
2, { {4, 8, TRUE }, {9, 100, TRUE }}
}
};
*msg = "remove rangelists";
err = child_err = SVN_NO_ERROR;
for (j = 0; j < 2; j++) {
for (i = 0; i < SIZE_OF_RANGE_REMOVE_TEST_ARRAY; i++) {
int expected_nbr_ranges;
svn_merge_range_t *expected_ranges;
SVN_ERR(svn_mergeinfo_parse(&info1, (test_data[i]).eraser, pool));
SVN_ERR(svn_mergeinfo_parse(&info2, (test_data[i]).whiteboard, pool));
eraser = apr_hash_get(info1, "/A", APR_HASH_KEY_STRING);
whiteboard = apr_hash_get(info2, "/A", APR_HASH_KEY_STRING);
if (eraser == NULL)
eraser = apr_array_make(pool, 0, sizeof(*eraser));
if (whiteboard == NULL)
whiteboard = apr_array_make(pool, 0, sizeof(*whiteboard));
if (j == 0) {
expected_nbr_ranges = (test_data[i]).expected_ranges_consider_inheritance;
expected_ranges = (test_data[i]).expected_removed_consider_inheritance;
} else {
expected_nbr_ranges = (test_data[i]).expected_ranges_ignore_inheritance;
expected_ranges = (test_data[i]).expected_removed_ignore_inheritance;
}
SVN_ERR(svn_rangelist_remove(&output, eraser, whiteboard,
j == 0 ? TRUE : FALSE,
pool));
child_err = verify_ranges_match(output, expected_ranges,
expected_nbr_ranges,
apr_psprintf(pool,
"svn_rangelist_remove "
"case %i", i),
"remove", pool);
if (child_err) {
if (err)
svn_error_compose(err, child_err);
else
err = child_err;
}
}
}
return err;
}
#define RANDOM_REV_ARRAY_LENGTH 100
static apr_uint32_t random_rev_array_seed;
static void
randomly_fill_rev_array(svn_boolean_t *revs) {
int i;
for (i = 0; i < RANDOM_REV_ARRAY_LENGTH; i++) {
apr_uint32_t next = svn_test_rand(&random_rev_array_seed);
revs[i] = (next < 0x40000000) ? 0 : 1;
}
}
static svn_error_t *
rev_array_to_rangelist(apr_array_header_t **rangelist,
svn_boolean_t *revs,
apr_pool_t *pool) {
svn_stringbuf_t *buf = svn_stringbuf_create("/trunk: ", pool);
svn_boolean_t first = TRUE;
apr_hash_t *mergeinfo;
int i;
for (i = 0; i < RANDOM_REV_ARRAY_LENGTH; i++) {
if (revs[i]) {
if (first)
first = FALSE;
else
svn_stringbuf_appendcstr(buf, ",");
svn_stringbuf_appendcstr(buf, apr_psprintf(pool, "%d", i));
}
}
SVN_ERR(svn_mergeinfo_parse(&mergeinfo, buf->data, pool));
*rangelist = apr_hash_get(mergeinfo, "/trunk", APR_HASH_KEY_STRING);
return SVN_NO_ERROR;
}
static svn_error_t *
test_rangelist_remove_randomly(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
int i;
apr_pool_t *iterpool;
*msg = "test rangelist remove with random data";
if (msg_only)
return SVN_NO_ERROR;
random_rev_array_seed = (apr_uint32_t) apr_time_now();
iterpool = svn_pool_create(pool);
for (i = 0; i < 20; i++) {
svn_boolean_t first_revs[RANDOM_REV_ARRAY_LENGTH],
second_revs[RANDOM_REV_ARRAY_LENGTH],
expected_revs[RANDOM_REV_ARRAY_LENGTH];
apr_array_header_t *first_rangelist, *second_rangelist,
*expected_rangelist, *actual_rangelist;
svn_merge_range_t expected_range_array[RANDOM_REV_ARRAY_LENGTH];
int j;
svn_pool_clear(iterpool);
randomly_fill_rev_array(first_revs);
randomly_fill_rev_array(second_revs);
for (j = 0; j < RANDOM_REV_ARRAY_LENGTH; j++)
expected_revs[j] = second_revs[j] && !first_revs[j];
SVN_ERR(rev_array_to_rangelist(&first_rangelist, first_revs, iterpool));
SVN_ERR(rev_array_to_rangelist(&second_rangelist, second_revs, iterpool));
SVN_ERR(rev_array_to_rangelist(&expected_rangelist, expected_revs,
iterpool));
for (j = 0; j < expected_rangelist->nelts; j++) {
expected_range_array[j] = *(APR_ARRAY_IDX(expected_rangelist, j,
svn_merge_range_t *));
}
SVN_ERR(svn_rangelist_remove(&actual_rangelist, first_rangelist,
second_rangelist, TRUE, iterpool));
SVN_ERR(verify_ranges_match(actual_rangelist,
expected_range_array,
expected_rangelist->nelts,
"svn_rangelist_remove random call",
"remove", iterpool));
}
svn_pool_destroy(iterpool);
return SVN_NO_ERROR;
}
static svn_error_t *
test_rangelist_intersect_randomly(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
int i;
apr_pool_t *iterpool;
*msg = "test rangelist intersect with random data";
if (msg_only)
return SVN_NO_ERROR;
random_rev_array_seed = (apr_uint32_t) apr_time_now();
iterpool = svn_pool_create(pool);
for (i = 0; i < 20; i++) {
svn_boolean_t first_revs[RANDOM_REV_ARRAY_LENGTH],
second_revs[RANDOM_REV_ARRAY_LENGTH],
expected_revs[RANDOM_REV_ARRAY_LENGTH];
apr_array_header_t *first_rangelist, *second_rangelist,
*expected_rangelist, *actual_rangelist;
svn_merge_range_t expected_range_array[RANDOM_REV_ARRAY_LENGTH];
int j;
svn_pool_clear(iterpool);
randomly_fill_rev_array(first_revs);
randomly_fill_rev_array(second_revs);
for (j = 0; j < RANDOM_REV_ARRAY_LENGTH; j++)
expected_revs[j] = second_revs[j] && first_revs[j];
SVN_ERR(rev_array_to_rangelist(&first_rangelist, first_revs, iterpool));
SVN_ERR(rev_array_to_rangelist(&second_rangelist, second_revs, iterpool));
SVN_ERR(rev_array_to_rangelist(&expected_rangelist, expected_revs,
iterpool));
for (j = 0; j < expected_rangelist->nelts; j++) {
expected_range_array[j] = *(APR_ARRAY_IDX(expected_rangelist, j,
svn_merge_range_t *));
}
SVN_ERR(svn_rangelist_intersect(&actual_rangelist, first_rangelist,
second_rangelist, TRUE, iterpool));
SVN_ERR(verify_ranges_match(actual_rangelist,
expected_range_array,
expected_rangelist->nelts,
"svn_rangelist_intersect random call",
"intersect", iterpool));
}
svn_pool_destroy(iterpool);
return SVN_NO_ERROR;
}
static svn_error_t *
test_remove_mergeinfo(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
apr_hash_t *output, *whiteboard, *eraser;
svn_merge_range_t expected_rangelist_remainder[NBR_RANGELIST_DELTAS] =
{ {6, 7, TRUE}, {8, 9, TRUE}, {10, 11, TRUE}, {32, 34, TRUE} };
*msg = "remove of mergeinfo";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(svn_mergeinfo_parse(&whiteboard,
"/trunk: 1,3-4,7,9,11-12,31-34", pool));
SVN_ERR(svn_mergeinfo_parse(&eraser, "/trunk: 1-6,12-16,30-32", pool));
SVN_ERR(svn_mergeinfo_remove(&output, eraser, whiteboard, pool));
return verify_mergeinfo_deltas(output, expected_rangelist_remainder,
"svn_mergeinfo_remove", "leftover", pool);
}
#undef NBR_RANGELIST_DELTAS
static svn_error_t *
test_rangelist_to_string(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
apr_array_header_t *result;
svn_string_t *output;
svn_string_t *expected = svn_string_create("3,5,7-11,13-14", pool);
*msg = "turning rangelist back into a string";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(svn_mergeinfo_parse(&info1, mergeinfo1, pool));
result = apr_hash_get(info1, "/trunk", APR_HASH_KEY_STRING);
if (!result)
return fail(pool, "Missing path in parsed mergeinfo");
SVN_ERR(svn_rangelist_to_string(&output, result, pool));
if (svn_string_compare(expected, output) != TRUE)
return fail(pool, "Rangelist string not what we expected");
return SVN_NO_ERROR;
}
static svn_error_t *
test_mergeinfo_to_string(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
svn_string_t *output;
svn_string_t *expected;
expected = svn_string_create("/fred:8-10\n/trunk:3,5,7-11,13-14", pool);
*msg = "turning mergeinfo back into a string";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(svn_mergeinfo_parse(&info1, mergeinfo1, pool));
SVN_ERR(svn_mergeinfo_to_string(&output, info1, pool));
if (svn_string_compare(expected, output) != TRUE)
return fail(pool, "Mergeinfo string not what we expected");
return SVN_NO_ERROR;
}
static svn_error_t *
test_rangelist_merge(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
int i;
svn_error_t *err, *child_err;
apr_array_header_t *rangelist1, *rangelist2;
struct rangelist_merge_test_data {
const char *mergeinfo1;
const char *mergeinfo2;
int expected_ranges;
svn_merge_range_t expected_merge[6];
};
#define SIZE_OF_RANGE_MERGE_TEST_ARRAY 59
struct rangelist_merge_test_data test_data[SIZE_OF_RANGE_MERGE_TEST_ARRAY] = {
{"/A: 1-44", "/A: 70-101", 2, {{ 0, 44, TRUE }, {69, 101, TRUE }}},
{"/A: 1-44*", "/A: 70-101", 2, {{ 0, 44, FALSE}, {69, 101, TRUE }}},
{"/A: 1-44", "/A: 70-101*", 2, {{ 0, 44, TRUE }, {69, 101, FALSE}}},
{"/A: 1-44*", "/A: 70-101*", 2, {{ 0, 44, FALSE}, {69, 101, FALSE}}},
{"/A: 70-101", "/A: 1-44", 2, {{ 0, 44, TRUE }, {69, 101, TRUE }}},
{"/A: 70-101*", "/A: 1-44", 2, {{ 0, 44, TRUE }, {69, 101, FALSE}}},
{"/A: 70-101", "/A: 1-44*", 2, {{ 0, 44, FALSE}, {69, 101, TRUE }}},
{"/A: 70-101*", "/A: 1-44*", 2, {{ 0, 44, FALSE}, {69, 101, FALSE}}},
{"/A: 4-20", "/A: 4-20", 1, {{3, 20, TRUE }}},
{"/A: 4-20*", "/A: 4-20", 1, {{3, 20, TRUE }}},
{"/A: 4-20", "/A: 4-20*", 1, {{3, 20, TRUE }}},
{"/A: 4-20*", "/A: 4-20*", 1, {{3, 20, FALSE}}},
{"/A: 6-17", "/A: 6-12", 1, {{5, 17, TRUE}}},
{"/A: 6-17*", "/A: 6-12", 2, {{5, 12, TRUE }, {12, 17, FALSE}}},
{"/A: 6-17", "/A: 6-12*", 1, {{5, 17, TRUE }}},
{"/A: 6-17*", "/A: 6-12*", 1, {{5, 17, FALSE}}},
{"/A: 6-12", "/A: 6-17", 1, {{5, 17, TRUE }}},
{"/A: 6-12*", "/A: 6-17", 1, {{5, 17, TRUE }}},
{"/A: 6-12", "/A: 6-17*", 2, {{5, 12, TRUE }, {12, 17, FALSE}}},
{"/A: 6-12*", "/A: 6-17*", 1, {{5, 17, FALSE}}},
{"/A: 5-77", "/A: 44-77", 1, {{4, 77, TRUE }}},
{"/A: 5-77*", "/A: 44-77", 2, {{4, 43, FALSE}, {43, 77, TRUE}}},
{"/A: 5-77", "/A: 44-77*", 1, {{4, 77, TRUE }}},
{"/A: 5-77*", "/A: 44-77*", 1, {{4, 77, FALSE}}},
{"/A: 44-77", "/A: 5-77", 1, {{4, 77, TRUE }}},
{"/A: 44-77*", "/A: 5-77", 1, {{4, 77, TRUE }}},
{"/A: 44-77", "/A: 5-77*", 2, {{4, 43, FALSE}, {43, 77, TRUE}}},
{"/A: 44-77*", "/A: 5-77*", 1, {{4, 77, FALSE}}},
{"/A: 12-24", "/A: 20-23", 1, {{11, 24, TRUE }}},
{
"/A: 12-24*", "/A: 20-23", 3, {{11, 19, FALSE}, {19, 23, TRUE },
{23, 24, FALSE}
}
},
{"/A: 12-24", "/A: 20-23*", 1, {{11, 24, TRUE }}},
{"/A: 12-24*", "/A: 20-23*", 1, {{11, 24, FALSE}}},
{"/A: 20-23", "/A: 12-24", 1, {{11, 24, TRUE }}},
{"/A: 20-23*", "/A: 12-24", 1, {{11, 24, TRUE }}},
{
"/A: 20-23", "/A: 12-24*", 3, {{11, 19, FALSE}, {19, 23, TRUE },
{23, 24, FALSE}
}
},
{"/A: 20-23*", "/A: 12-24*", 1, {{11, 24, FALSE}}},
{"/A: 50-73", "/A: 60-99", 1, {{49, 99, TRUE }}},
{"/A: 50-73*", "/A: 60-99", 2, {{49, 59, FALSE}, {59, 99, TRUE }}},
{"/A: 50-73", "/A: 60-99*", 2, {{49, 73, TRUE }, {73, 99, FALSE}}},
{"/A: 50-73*", "/A: 60-99*", 1, {{49, 99, FALSE}}},
{"/A: 60-99", "/A: 50-73", 1, {{49, 99, TRUE }}},
{"/A: 60-99*", "/A: 50-73", 2, {{49, 73, TRUE }, {73, 99, FALSE}}},
{"/A: 60-99", "/A: 50-73*", 2, {{49, 59, FALSE}, {59, 99, TRUE }}},
{"/A: 60-99*", "/A: 50-73*", 1, {{49, 99, FALSE}}},
{"/A: 1-5,7,12-13", "/A: 2-17", 1, {{0, 17, TRUE }}},
{"/A: 1-5*,7*,12-13*", "/A: 2-17*", 1, {{0, 17, FALSE}}},
{
"/A: 1-5,7,12-13", "/A: 2-17*", 6,
{ {0, 5, TRUE }, { 5, 6, FALSE}, { 6, 7, TRUE },
{7, 11, FALSE}, {11, 13, TRUE }, {13, 17, FALSE}
}
},
{
"/A: 1-5*,7*,12-13*", "/A: 2-17", 2,
{{0, 1, FALSE}, {1, 17, TRUE }}
},
{"/A: 2-17", "/A: 1-5,7,12-13", 1, {{0, 17, TRUE }}},
{"/A: 2-17*", "/A: 1-5*,7*,12-13*", 1, {{0, 17, FALSE}}},
{
"/A: 2-17*", "/A: 1-5,7,12-13", 6,
{ {0, 5, TRUE }, { 5, 6, FALSE}, { 6, 7, TRUE },
{7, 11, FALSE}, {11, 13, TRUE }, {13, 17, FALSE}
}
},
{
"/A: 2-17", "/A: 1-5*,7*,12-13*", 2,
{{0, 1, FALSE}, {1, 17, TRUE}}
},
{"/A: 1-44,45,46,47-50", "", 1, {{ 0, 50, TRUE }}},
{"/A: 1,2,3,4,5,6,7,8", "", 1, {{ 0, 8, TRUE }}},
{
"/A: 6-10,12-13,14,15,16-22", "", 2,
{{ 5, 10, TRUE }, { 11, 22, TRUE }}
},
{"", "/A: 1-44,45,46,47-50", 1, {{ 0, 50, TRUE }}},
{"", "/A: 1,2,3,4,5,6,7,8", 1, {{ 0, 8, TRUE }}},
{
"", "/A: 6-10,12-13,14,15,16-22", 2,
{{ 5, 10, TRUE }, { 11, 22, TRUE }}
},
{"", "", 0, {{0, 0, FALSE}}}
};
*msg = "merge of rangelists";
if (msg_only)
return SVN_NO_ERROR;
err = child_err = SVN_NO_ERROR;
for (i = 0; i < SIZE_OF_RANGE_MERGE_TEST_ARRAY; i++) {
SVN_ERR(svn_mergeinfo_parse(&info1, (test_data[i]).mergeinfo1, pool));
SVN_ERR(svn_mergeinfo_parse(&info2, (test_data[i]).mergeinfo2, pool));
rangelist1 = apr_hash_get(info1, "/A", APR_HASH_KEY_STRING);
rangelist2 = apr_hash_get(info2, "/A", APR_HASH_KEY_STRING);
if (rangelist1 == NULL)
rangelist1 = apr_array_make(pool, 0, sizeof(svn_merge_range_t *));
if (rangelist2 == NULL)
rangelist2 = apr_array_make(pool, 0, sizeof(svn_merge_range_t *));
SVN_ERR(svn_rangelist_merge(&rangelist1, rangelist2, pool));
child_err = verify_ranges_match(rangelist1,
(test_data[i]).expected_merge,
(test_data[i]).expected_ranges,
apr_psprintf(pool,
"svn_rangelist_merge "
"case %i", i),
"merge", pool);
if (child_err) {
if (err)
svn_error_compose(err, child_err);
else
err = child_err;
}
}
return err;
}
static svn_error_t *
test_rangelist_diff(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
int i;
svn_error_t *err, *child_err;
apr_array_header_t *from, *to, *added, *deleted;
struct rangelist_diff_test_data {
const char *from;
const char *to;
int expected_add_ranges;
svn_merge_range_t expected_adds[10];
int expected_del_ranges;
svn_merge_range_t expected_dels[10];
int expected_add_ranges_ignore_inheritance;
svn_merge_range_t expected_adds_ignore_inheritance[10];
int expected_del_ranges_ignore_inheritance;
svn_merge_range_t expected_dels_ignore_inheritance[10];
};
#define SIZE_OF_RANGE_DIFF_TEST_ARRAY 16
struct rangelist_diff_test_data test_data[SIZE_OF_RANGE_DIFF_TEST_ARRAY] = {
{
"/A: 1", "/A: 3",
1, { { 2, 3, TRUE } },
1, { { 0, 1, TRUE } },
1, { { 2, 3, TRUE } },
1, { { 0, 1, TRUE } }
},
{
"/A: 1", "/A: 1,3",
1, { { 2, 3, TRUE } },
0, { { 0, 0, FALSE } },
1, { { 2, 3, TRUE } },
0, { { 0, 0, FALSE } }
},
{
"/A: 1,3", "/A: 1",
0, { { 0, 0, FALSE } },
1, { { 2, 3, TRUE } },
0, { { 0, 0, FALSE } },
1, { { 2, 3, TRUE } }
},
{
"/A: 1,3", "/A: 1,3",
0, { { 0, 0, FALSE } },
0, { { 0, 0, FALSE } },
0, { { 0, 0, FALSE } },
0, { { 0, 0, FALSE } }
},
{
"/A: 1,3*", "/A: 1,3*",
0, { { 0, 0, FALSE } },
0, { { 0, 0, FALSE } },
0, { { 0, 0, FALSE } },
0, { { 0, 0, FALSE } }
},
{
"/A: 1,5-8", "/A: 1,6,10-12",
1, { { 9, 12, TRUE } },
2, { { 4, 5, TRUE }, { 6, 8, TRUE } },
1, { { 9, 12, TRUE } },
2, { { 4, 5, TRUE }, { 6, 8, TRUE } }
},
{
"/A: 6*", "/A: 6",
1, { { 5, 6, TRUE } },
1, { { 5, 6, FALSE } },
0, { { 0, 0, FALSE } },
0, { { 0, 0, FALSE } }
},
{
"/A: 6", "/A: 6*",
1, { { 5, 6, FALSE } },
1, { { 5, 6, TRUE } },
0, { { 0, 0, FALSE } },
0, { { 0, 0, FALSE } }
},
{
"/A: 6*", "/A: 6",
1, { { 5, 6, TRUE } },
1, { { 5, 6, FALSE } },
0, { { 0, 0, FALSE } },
0, { { 0, 0, FALSE } }
},
{
"/A: 1,5-8", "/A: 1,6*,10-12",
2, { { 5, 6, FALSE }, { 9, 12, TRUE } },
1, { { 4, 8, TRUE } },
1, { { 9, 12, TRUE } },
2, { { 4, 5, TRUE }, { 6, 8, TRUE } }
},
{
"/A: 1,5-8*", "/A: 1,6,10-12",
2, { { 5, 6, TRUE }, { 9, 12, TRUE } },
1, { { 4, 8, FALSE } },
1, { { 9, 12, TRUE } },
2, { { 4, 5, FALSE }, { 6, 8, FALSE } }
},
{
"/A: 3-9", "",
0, { { 0, 0, FALSE } },
1, { { 2, 9, TRUE } },
0, { { 0, 0, FALSE } },
1, { { 2, 9, TRUE } }
},
{
"/A: 3-9*", "",
0, { { 0, 0, FALSE } },
1, { { 2, 9, FALSE } },
0, { { 0, 0, FALSE } },
1, { { 2, 9, FALSE } }
},
{
"", "/A: 3-9",
1, { { 2, 9, TRUE } },
0, { { 0, 0, FALSE } },
1, { { 2, 9, TRUE } },
0, { { 0, 0, FALSE } }
},
{
"", "/A: 3-9*",
1, { { 2, 9, FALSE } },
0, { { 0, 0, FALSE } },
1, { { 2, 9, FALSE } },
0, { { 0, 0, FALSE } }
},
{
"", "",
0, { { 0, 0, FALSE } },
0, { { 0, 0, FALSE } },
0, { { 0, 0, FALSE } },
0, { { 0, 0, FALSE } }
},
};
*msg = "diff of rangelists";
if (msg_only)
return SVN_NO_ERROR;
err = child_err = SVN_NO_ERROR;
for (i = 0; i < SIZE_OF_RANGE_DIFF_TEST_ARRAY; i++) {
SVN_ERR(svn_mergeinfo_parse(&info1, (test_data[i]).to, pool));
SVN_ERR(svn_mergeinfo_parse(&info2, (test_data[i]).from, pool));
to = apr_hash_get(info1, "/A", APR_HASH_KEY_STRING);
from = apr_hash_get(info2, "/A", APR_HASH_KEY_STRING);
if (to == NULL)
to = apr_array_make(pool, 0, sizeof(*to));
if (from == NULL)
from = apr_array_make(pool, 0, sizeof(*from));
SVN_ERR(svn_rangelist_diff(&deleted, &added, from, to, TRUE, pool));
child_err = verify_ranges_match(added,
(test_data[i]).expected_adds,
(test_data[i]).expected_add_ranges,
apr_psprintf(pool,
"svn_rangelist_diff"
"case %i", i),
"diff", pool);
if (!child_err)
child_err = verify_ranges_match(deleted,
(test_data[i]).expected_dels,
(test_data[i]).expected_del_ranges,
apr_psprintf(pool,
"svn_rangelist_diff"
"case %i", i),
"diff", pool);
if (!child_err) {
SVN_ERR(svn_rangelist_diff(&deleted, &added, from, to, FALSE,
pool));
child_err = verify_ranges_match(
added,
(test_data[i]).expected_adds_ignore_inheritance,
(test_data[i]).expected_add_ranges_ignore_inheritance,
apr_psprintf(pool, "svn_rangelist_diff case %i", i),
"diff", pool);
if (!child_err)
child_err = verify_ranges_match(
deleted,
(test_data[i]).expected_dels_ignore_inheritance,
(test_data[i]).expected_del_ranges_ignore_inheritance,
apr_psprintf(pool, "svn_rangelist_diff case %i", i),
"diff", pool);
}
if (child_err) {
if (err)
svn_error_compose(err, child_err);
else
err = child_err;
}
}
return err;
}
struct svn_test_descriptor_t test_funcs[] = {
SVN_TEST_NULL,
SVN_TEST_PASS(test_parse_single_line_mergeinfo),
SVN_TEST_PASS(test_mergeinfo_dup),
SVN_TEST_PASS(test_parse_combine_rangeinfo),
SVN_TEST_PASS(test_parse_broken_mergeinfo),
SVN_TEST_PASS(test_remove_rangelist),
SVN_TEST_PASS(test_rangelist_remove_randomly),
SVN_TEST_PASS(test_remove_mergeinfo),
SVN_TEST_PASS(test_rangelist_reverse),
SVN_TEST_PASS(test_rangelist_intersect),
SVN_TEST_PASS(test_rangelist_intersect_randomly),
SVN_TEST_PASS(test_diff_mergeinfo),
SVN_TEST_PASS(test_merge_mergeinfo),
SVN_TEST_PASS(test_mergeinfo_intersect),
SVN_TEST_PASS(test_rangelist_to_string),
SVN_TEST_PASS(test_mergeinfo_to_string),
SVN_TEST_PASS(test_rangelist_merge),
SVN_TEST_PASS(test_rangelist_diff),
SVN_TEST_NULL
};