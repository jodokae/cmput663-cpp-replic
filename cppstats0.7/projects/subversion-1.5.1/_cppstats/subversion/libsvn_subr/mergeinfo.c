#include <assert.h>
#include <ctype.h>
#include "svn_path.h"
#include "svn_types.h"
#include "svn_ctype.h"
#include "svn_pools.h"
#include "svn_sorts.h"
#include "svn_error.h"
#include "svn_error_codes.h"
#include "svn_string.h"
#include "svn_mergeinfo.h"
#include "private/svn_mergeinfo_private.h"
#include "svn_private_config.h"
static svn_boolean_t
combine_ranges(svn_merge_range_t **output, svn_merge_range_t *in1,
svn_merge_range_t *in2,
svn_boolean_t consider_inheritance) {
if (in1->start <= in2->end && in2->start <= in1->end) {
if (!consider_inheritance
|| (consider_inheritance
&& ((in1->inheritable ? TRUE : FALSE)
== (in2->inheritable ? TRUE : FALSE)))) {
(*output)->start = MIN(in1->start, in2->start);
(*output)->end = MAX(in1->end, in2->end);
(*output)->inheritable =
(in1->inheritable || in2->inheritable) ? TRUE : FALSE;
return TRUE;
}
}
return FALSE;
}
static svn_error_t *
parse_pathname(const char **input, const char *end,
svn_stringbuf_t **pathname, apr_pool_t *pool) {
const char *curr = *input;
*pathname = svn_stringbuf_create("", pool);
while (curr < end && *curr != ':') {
svn_stringbuf_appendbytes(*pathname, curr, 1);
curr++;
}
if ((*pathname)->len == 0)
return svn_error_create(SVN_ERR_MERGEINFO_PARSE_ERROR, NULL,
_("No pathname preceeding ':'"));
*input = curr;
return SVN_NO_ERROR;
}
static APR_INLINE void
combine_with_lastrange(svn_merge_range_t** lastrange,
svn_merge_range_t *mrange, svn_boolean_t dup_mrange,
apr_array_header_t *revlist,
svn_boolean_t consider_inheritance,
apr_pool_t *pool) {
svn_merge_range_t *pushed_mrange_1 = NULL;
svn_merge_range_t *pushed_mrange_2 = NULL;
svn_boolean_t ranges_intersect = FALSE;
svn_boolean_t ranges_have_same_inheritance = FALSE;
if (*lastrange) {
if ((*lastrange)->start <= mrange->end
&& mrange->start <= (*lastrange)->end)
ranges_intersect = TRUE;
if ((*lastrange)->inheritable == mrange->inheritable)
ranges_have_same_inheritance = TRUE;
}
if (!(*lastrange)
|| (!ranges_intersect || (!ranges_have_same_inheritance
&& consider_inheritance)))
{
if (dup_mrange)
pushed_mrange_1 = svn_merge_range_dup(mrange, pool);
else
pushed_mrange_1 = mrange;
} else {
if (ranges_have_same_inheritance) {
(*lastrange)->start = MIN((*lastrange)->start, mrange->start);
(*lastrange)->end = MAX((*lastrange)->end, mrange->end);
(*lastrange)->inheritable =
((*lastrange)->inheritable || mrange->inheritable) ? TRUE : FALSE;
} else
{
svn_revnum_t tmp_revnum;
if ((*lastrange)->start == mrange->start) {
if ((*lastrange)->end == mrange->end) {
(*lastrange)->inheritable = TRUE;
} else if ((*lastrange)->end > mrange->end) {
if (!(*lastrange)->inheritable) {
tmp_revnum = (*lastrange)->end;
(*lastrange)->end = mrange->end;
(*lastrange)->inheritable = TRUE;
if (dup_mrange)
pushed_mrange_1 = svn_merge_range_dup(mrange, pool);
else
pushed_mrange_1 = mrange;
pushed_mrange_1->start = pushed_mrange_1->start;
pushed_mrange_1->end = tmp_revnum;
*lastrange = pushed_mrange_1;
}
} else {
if (mrange->inheritable) {
(*lastrange)->inheritable = TRUE;
(*lastrange)->end = mrange->end;
} else {
if (dup_mrange)
pushed_mrange_1 = svn_merge_range_dup(mrange, pool);
else
pushed_mrange_1 = mrange;
pushed_mrange_1->start = (*lastrange)->end;
}
}
}
else if ((*lastrange)->end == mrange->end) {
if ((*lastrange)->start < mrange->start) {
if (!(*lastrange)->inheritable) {
(*lastrange)->end = mrange->start;
if (dup_mrange)
pushed_mrange_1 = svn_merge_range_dup(mrange, pool);
else
pushed_mrange_1 = mrange;
*lastrange = pushed_mrange_1;
}
} else {
(*lastrange)->start = mrange->start;
(*lastrange)->end = mrange->end;
(*lastrange)->inheritable = mrange->inheritable;
if (dup_mrange)
pushed_mrange_1 = svn_merge_range_dup(mrange, pool);
else
pushed_mrange_1 = mrange;
pushed_mrange_1->start = (*lastrange)->end;
pushed_mrange_1->inheritable = TRUE;
}
} else {
if ((*lastrange)->start < mrange->start) {
if (!((*lastrange)->end > mrange->end
&& (*lastrange)->inheritable)) {
tmp_revnum = (*lastrange)->end;
if (!(*lastrange)->inheritable)
(*lastrange)->end = mrange->start;
else
mrange->start = (*lastrange)->end;
if (dup_mrange)
pushed_mrange_1 = svn_merge_range_dup(mrange, pool);
else
pushed_mrange_1 = mrange;
if (tmp_revnum > mrange->end) {
pushed_mrange_2 =
apr_palloc(pool, sizeof(*pushed_mrange_2));
pushed_mrange_2->start = mrange->end;
pushed_mrange_2->end = tmp_revnum;
pushed_mrange_2->inheritable =
(*lastrange)->inheritable;
}
mrange->inheritable = TRUE;
}
} else {
if ((*lastrange)->end < mrange->end) {
pushed_mrange_2->start = (*lastrange)->end;
pushed_mrange_2->end = mrange->end;
pushed_mrange_2->inheritable = mrange->inheritable;
tmp_revnum = (*lastrange)->start;
(*lastrange)->start = mrange->start;
(*lastrange)->end = tmp_revnum;
(*lastrange)->inheritable = mrange->inheritable;
mrange->start = tmp_revnum;
mrange->end = pushed_mrange_2->start;
mrange->inheritable = TRUE;
} else {
pushed_mrange_2->start = mrange->end;
pushed_mrange_2->end = (*lastrange)->end;
pushed_mrange_2->inheritable =
(*lastrange)->inheritable;
tmp_revnum = (*lastrange)->start;
(*lastrange)->start = mrange->start;
(*lastrange)->end = tmp_revnum;
(*lastrange)->inheritable = mrange->inheritable;
mrange->start = tmp_revnum;
mrange->end = pushed_mrange_2->start;
mrange->inheritable = TRUE;
}
}
}
}
}
if (pushed_mrange_1) {
APR_ARRAY_PUSH(revlist, svn_merge_range_t *) = pushed_mrange_1;
*lastrange = pushed_mrange_1;
}
if (pushed_mrange_2) {
APR_ARRAY_PUSH(revlist, svn_merge_range_t *) = pushed_mrange_2;
*lastrange = pushed_mrange_2;
}
}
static svn_error_t *
range_to_string(svn_string_t **result, svn_merge_range_t *range,
apr_pool_t *pool) {
if (range->start == range->end - 1)
*result = svn_string_createf(pool, "%ld%s", range->end,
range->inheritable
? "" : SVN_MERGEINFO_NONINHERITABLE_STR);
else
*result = svn_string_createf(pool, "%ld-%ld%s", range->start + 1,
range->end, range->inheritable
? "" : SVN_MERGEINFO_NONINHERITABLE_STR);
return SVN_NO_ERROR;
}
static svn_error_t *
combine_with_adjacent_lastrange(svn_merge_range_t **lastrange,
svn_merge_range_t *mrange,
svn_boolean_t dup_mrange,
apr_array_header_t *revlist,
apr_pool_t *pool) {
svn_merge_range_t *pushed_mrange = mrange;
if (*lastrange) {
svn_string_t *r1, *r2;
if ((*lastrange)->start <= mrange->end
&& mrange->start <= (*lastrange)->end) {
SVN_ERR(range_to_string(&r1, *lastrange, pool));
SVN_ERR(range_to_string(&r2, mrange, pool));
if (mrange->start < (*lastrange)->end) {
return svn_error_createf(SVN_ERR_MERGEINFO_PARSE_ERROR, NULL,
_("Parsing of overlapping revision "
"ranges '%s' and '%s' is not "
"supported"), r1->data, r2->data);
} else if ((*lastrange)->inheritable == mrange->inheritable) {
(*lastrange)->end = mrange->end;
return SVN_NO_ERROR;
}
} else if ((*lastrange)->start > mrange->start) {
SVN_ERR(range_to_string(&r1, *lastrange, pool));
SVN_ERR(range_to_string(&r2, mrange, pool));
return svn_error_createf(SVN_ERR_MERGEINFO_PARSE_ERROR, NULL,
_("Unable to parse unordered revision "
"ranges '%s' and '%s'"),
r1->data, r2->data);
}
}
if (dup_mrange)
pushed_mrange = svn_merge_range_dup(mrange, pool);
APR_ARRAY_PUSH(revlist, svn_merge_range_t *) = pushed_mrange;
*lastrange = pushed_mrange;
return SVN_NO_ERROR;
}
static svn_error_t *
parse_revlist(const char **input, const char *end,
apr_array_header_t *revlist, const char *pathname,
apr_pool_t *pool) {
const char *curr = *input;
svn_merge_range_t *lastrange = NULL;
while (curr < end && *curr != '\n' && isspace(*curr))
curr++;
if (*curr == '\n' || curr == end) {
*input = curr;
return svn_error_createf(SVN_ERR_MERGEINFO_PARSE_ERROR, NULL,
_("Mergeinfo for '%s' maps to an "
"empty revision range"), pathname);
}
while (curr < end && *curr != '\n') {
svn_merge_range_t *mrange = apr_pcalloc(pool, sizeof(*mrange));
svn_revnum_t firstrev;
SVN_ERR(svn_revnum_parse(&firstrev, curr, &curr));
if (*curr != '-' && *curr != '\n' && *curr != ',' && *curr != '*'
&& curr != end)
return svn_error_createf(SVN_ERR_MERGEINFO_PARSE_ERROR, NULL,
_("Invalid character '%c' found in revision "
"list"), *curr);
mrange->start = firstrev - 1;
mrange->end = firstrev;
mrange->inheritable = TRUE;
if (*curr == '-') {
svn_revnum_t secondrev;
curr++;
SVN_ERR(svn_revnum_parse(&secondrev, curr, &curr));
if (firstrev > secondrev)
return svn_error_createf(SVN_ERR_MERGEINFO_PARSE_ERROR, NULL,
_("Unable to parse reversed revision "
"range '%ld-%ld'"),
firstrev, secondrev);
else if (firstrev == secondrev)
return svn_error_createf(SVN_ERR_MERGEINFO_PARSE_ERROR, NULL,
_("Unable to parse revision range "
"'%ld-%ld' with same start and end "
"revisions"), firstrev, secondrev);
mrange->end = secondrev;
}
if (*curr == '\n' || curr == end) {
SVN_ERR(combine_with_adjacent_lastrange(&lastrange, mrange, FALSE,
revlist, pool));
*input = curr;
return SVN_NO_ERROR;
} else if (*curr == ',') {
SVN_ERR(combine_with_adjacent_lastrange(&lastrange, mrange, FALSE,
revlist, pool));
curr++;
} else if (*curr == '*') {
mrange->inheritable = FALSE;
curr++;
if (*curr == ',' || *curr == '\n' || curr == end) {
SVN_ERR(combine_with_adjacent_lastrange(&lastrange, mrange,
FALSE, revlist, pool));
if (*curr == ',') {
curr++;
} else {
*input = curr;
return SVN_NO_ERROR;
}
} else {
return svn_error_createf(SVN_ERR_MERGEINFO_PARSE_ERROR, NULL,
_("Invalid character '%c' found in "
"range list"), *curr);
}
} else {
return svn_error_createf(SVN_ERR_MERGEINFO_PARSE_ERROR, NULL,
_("Invalid character '%c' found in "
"range list"), *curr);
}
}
if (*curr != '\n')
return svn_error_create(SVN_ERR_MERGEINFO_PARSE_ERROR, NULL,
_("Range list parsing ended before hitting "
"newline"));
*input = curr;
return SVN_NO_ERROR;
}
static svn_error_t *
parse_revision_line(const char **input, const char *end, svn_mergeinfo_t hash,
apr_pool_t *pool) {
svn_stringbuf_t *pathname;
apr_array_header_t *revlist = apr_array_make(pool, 1,
sizeof(svn_merge_range_t *));
SVN_ERR(parse_pathname(input, end, &pathname, pool));
if (*(*input) != ':')
return svn_error_create(SVN_ERR_MERGEINFO_PARSE_ERROR, NULL,
_("Pathname not terminated by ':'"));
*input = *input + 1;
SVN_ERR(parse_revlist(input, end, revlist, pathname->data, pool));
if (*input != end && *(*input) != '\n')
return svn_error_createf(SVN_ERR_MERGEINFO_PARSE_ERROR, NULL,
_("Could not find end of line in range list line "
"in '%s'"), *input);
if (*input != end)
*input = *input + 1;
qsort(revlist->elts, revlist->nelts, revlist->elt_size,
svn_sort_compare_ranges);
apr_hash_set(hash, pathname->data, APR_HASH_KEY_STRING, revlist);
return SVN_NO_ERROR;
}
static svn_error_t *
parse_top(const char **input, const char *end, svn_mergeinfo_t hash,
apr_pool_t *pool) {
while (*input < end)
SVN_ERR(parse_revision_line(input, end, hash, pool));
return SVN_NO_ERROR;
}
svn_error_t *
svn_mergeinfo_parse(svn_mergeinfo_t *mergeinfo,
const char *input,
apr_pool_t *pool) {
svn_error_t *err;
*mergeinfo = apr_hash_make(pool);
err = parse_top(&input, input + strlen(input), *mergeinfo, pool);
if (err && err->apr_err != SVN_ERR_MERGEINFO_PARSE_ERROR)
err = svn_error_createf(SVN_ERR_MERGEINFO_PARSE_ERROR, err,
_("Could not parse mergeinfo string '%s'"),
input);
return err;
}
svn_error_t *
svn_rangelist_merge(apr_array_header_t **rangelist,
apr_array_header_t *changes,
apr_pool_t *pool) {
int i, j;
svn_merge_range_t *lastrange = NULL;
apr_array_header_t *output = apr_array_make(pool, 1,
sizeof(svn_merge_range_t *));
i = 0;
j = 0;
while (i < (*rangelist)->nelts && j < changes->nelts) {
svn_merge_range_t *elt1, *elt2;
int res;
elt1 = APR_ARRAY_IDX(*rangelist, i, svn_merge_range_t *);
elt2 = APR_ARRAY_IDX(changes, j, svn_merge_range_t *);
res = svn_sort_compare_ranges(&elt1, &elt2);
if (res == 0) {
if (elt1->inheritable || elt2->inheritable)
elt1->inheritable = TRUE;
combine_with_lastrange(&lastrange, elt1, TRUE, output,
FALSE, pool);
i++;
j++;
} else if (res < 0) {
combine_with_lastrange(&lastrange, elt1, TRUE, output,
FALSE, pool);
i++;
} else {
combine_with_lastrange(&lastrange, elt2, TRUE, output,
FALSE, pool);
j++;
}
}
assert (!(i < (*rangelist)->nelts && j < changes->nelts));
for (; i < (*rangelist)->nelts; i++) {
svn_merge_range_t *elt = APR_ARRAY_IDX(*rangelist, i,
svn_merge_range_t *);
combine_with_lastrange(&lastrange, elt, TRUE, output,
FALSE, pool);
}
for (; j < changes->nelts; j++) {
svn_merge_range_t *elt = APR_ARRAY_IDX(changes, j, svn_merge_range_t *);
combine_with_lastrange(&lastrange, elt, TRUE, output,
FALSE, pool);
}
*rangelist = output;
return SVN_NO_ERROR;
}
static svn_boolean_t
range_intersect(svn_merge_range_t *first, svn_merge_range_t *second,
svn_boolean_t consider_inheritance) {
return (first->start + 1 <= second->end)
&& (second->start + 1 <= first->end)
&& (!consider_inheritance
|| (!(first->inheritable) == !(second->inheritable)));
}
static svn_boolean_t
range_contains(svn_merge_range_t *first, svn_merge_range_t *second,
svn_boolean_t consider_inheritance) {
return (first->start <= second->start) && (second->end <= first->end)
&& (!consider_inheritance
|| (!(first->inheritable) == !(second->inheritable)));
}
static void
range_swap_endpoints(svn_merge_range_t *range) {
svn_revnum_t swap = range->start;
range->start = range->end;
range->end = swap;
}
svn_error_t *
svn_rangelist_reverse(apr_array_header_t *rangelist, apr_pool_t *pool) {
int i, swap_index;
svn_merge_range_t range;
for (i = 0; i < rangelist->nelts / 2; i++) {
swap_index = rangelist->nelts - i - 1;
range = *APR_ARRAY_IDX(rangelist, i, svn_merge_range_t *);
*APR_ARRAY_IDX(rangelist, i, svn_merge_range_t *) =
*APR_ARRAY_IDX(rangelist, swap_index, svn_merge_range_t *);
*APR_ARRAY_IDX(rangelist, swap_index, svn_merge_range_t *) = range;
range_swap_endpoints(APR_ARRAY_IDX(rangelist, swap_index,
svn_merge_range_t *));
range_swap_endpoints(APR_ARRAY_IDX(rangelist, i, svn_merge_range_t *));
}
if (rangelist->nelts % 2 == 1)
range_swap_endpoints(APR_ARRAY_IDX(rangelist, rangelist->nelts / 2,
svn_merge_range_t *));
return SVN_NO_ERROR;
}
static svn_error_t *
rangelist_intersect_or_remove(apr_array_header_t **output,
apr_array_header_t *eraser,
apr_array_header_t *whiteboard,
svn_boolean_t do_remove,
svn_boolean_t consider_inheritance,
apr_pool_t *pool) {
int i, j, lasti;
svn_merge_range_t *lastrange = NULL;
svn_merge_range_t wboardelt;
*output = apr_array_make(pool, 1, sizeof(svn_merge_range_t *));
i = 0;
j = 0;
lasti = -1;
while (i < whiteboard->nelts && j < eraser->nelts) {
svn_merge_range_t *elt1, *elt2;
elt2 = APR_ARRAY_IDX(eraser, j, svn_merge_range_t *);
if (i != lasti) {
wboardelt = *(APR_ARRAY_IDX(whiteboard, i, svn_merge_range_t *));
lasti = i;
}
elt1 = &wboardelt;
if (range_contains(elt2, elt1, consider_inheritance)) {
if (!do_remove)
combine_with_lastrange(&lastrange, elt1, TRUE, *output,
consider_inheritance, pool);
i++;
if (elt1->start == elt2->start && elt1->end == elt2->end)
j++;
} else if (range_intersect(elt2, elt1, consider_inheritance)) {
if (elt1->start < elt2->start) {
svn_merge_range_t tmp_range;
tmp_range.inheritable = elt1->inheritable;
if (do_remove) {
tmp_range.start = elt1->start;
tmp_range.end = elt2->start;
} else {
tmp_range.start = elt2->start;
tmp_range.end = MIN(elt1->end, elt2->end);
}
combine_with_lastrange(&lastrange, &tmp_range, TRUE,
*output, consider_inheritance, pool);
}
if (elt1->end > elt2->end) {
if (!do_remove) {
svn_merge_range_t tmp_range;
tmp_range.start = MAX(elt1->start, elt2->start);
tmp_range.end = elt2->end;
tmp_range.inheritable = elt1->inheritable;
combine_with_lastrange(&lastrange, &tmp_range, TRUE,
*output, consider_inheritance, pool);
}
wboardelt.start = elt2->end;
wboardelt.end = elt1->end;
} else
i++;
} else {
if (svn_sort_compare_ranges(&elt2, &elt1) < 0)
j++;
else {
if (do_remove && !(lastrange &&
combine_ranges(&lastrange, lastrange, elt1,
consider_inheritance))) {
lastrange = svn_merge_range_dup(elt1, pool);
APR_ARRAY_PUSH(*output, svn_merge_range_t *) = lastrange;
}
i++;
}
}
}
if (do_remove) {
if (i == lasti && i < whiteboard->nelts) {
combine_with_lastrange(&lastrange, &wboardelt, TRUE, *output,
consider_inheritance, pool);
i++;
}
for (; i < whiteboard->nelts; i++) {
svn_merge_range_t *elt = APR_ARRAY_IDX(whiteboard, i,
svn_merge_range_t *);
combine_with_lastrange(&lastrange, elt, TRUE, *output,
consider_inheritance, pool);
}
}
return SVN_NO_ERROR;
}
svn_error_t *
svn_rangelist_intersect(apr_array_header_t **output,
apr_array_header_t *rangelist1,
apr_array_header_t *rangelist2,
svn_boolean_t consider_inheritance,
apr_pool_t *pool) {
return rangelist_intersect_or_remove(output, rangelist1, rangelist2, FALSE,
consider_inheritance, pool);
}
svn_error_t *
svn_rangelist_remove(apr_array_header_t **output,
apr_array_header_t *eraser,
apr_array_header_t *whiteboard,
svn_boolean_t consider_inheritance,
apr_pool_t *pool) {
return rangelist_intersect_or_remove(output, eraser, whiteboard, TRUE,
consider_inheritance, pool);
}
svn_error_t *
svn_rangelist_diff(apr_array_header_t **deleted, apr_array_header_t **added,
apr_array_header_t *from, apr_array_header_t *to,
svn_boolean_t consider_inheritance,
apr_pool_t *pool) {
SVN_ERR(svn_rangelist_remove(deleted, to, from, consider_inheritance,
pool));
SVN_ERR(svn_rangelist_remove(added, from, to, consider_inheritance, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
walk_mergeinfo_hash_for_diff(svn_mergeinfo_t from, svn_mergeinfo_t to,
svn_mergeinfo_t deleted, svn_mergeinfo_t added,
svn_boolean_t consider_inheritance,
apr_pool_t *pool) {
apr_hash_index_t *hi;
const void *key;
void *val;
const char *path;
apr_array_header_t *from_rangelist, *to_rangelist;
for (hi = apr_hash_first(pool, from); hi; hi = apr_hash_next(hi)) {
apr_hash_this(hi, &key, NULL, &val);
path = key;
from_rangelist = val;
to_rangelist = apr_hash_get(to, path, APR_HASH_KEY_STRING);
if (to_rangelist) {
apr_array_header_t *deleted_rangelist, *added_rangelist;
svn_rangelist_diff(&deleted_rangelist, &added_rangelist,
from_rangelist, to_rangelist,
consider_inheritance, pool);
if (deleted && deleted_rangelist->nelts > 0)
apr_hash_set(deleted, apr_pstrdup(pool, path),
APR_HASH_KEY_STRING, deleted_rangelist);
if (added && added_rangelist->nelts > 0)
apr_hash_set(added, apr_pstrdup(pool, path),
APR_HASH_KEY_STRING, added_rangelist);
} else if (deleted)
apr_hash_set(deleted, apr_pstrdup(pool, path), APR_HASH_KEY_STRING,
svn_rangelist_dup(from_rangelist, pool));
}
if (!added)
return SVN_NO_ERROR;
for (hi = apr_hash_first(pool, to); hi; hi = apr_hash_next(hi)) {
apr_hash_this(hi, &key, NULL, &val);
path = key;
to_rangelist = val;
if (apr_hash_get(from, path, APR_HASH_KEY_STRING) == NULL)
apr_hash_set(added, apr_pstrdup(pool, path), APR_HASH_KEY_STRING,
svn_rangelist_dup(to_rangelist, pool));
}
return SVN_NO_ERROR;
}
svn_error_t *
svn_mergeinfo_diff(svn_mergeinfo_t *deleted, svn_mergeinfo_t *added,
svn_mergeinfo_t from, svn_mergeinfo_t to,
svn_boolean_t consider_inheritance,
apr_pool_t *pool) {
if (from && to == NULL) {
*deleted = svn_mergeinfo_dup(from, pool);
*added = apr_hash_make(pool);
} else if (from == NULL && to) {
*deleted = apr_hash_make(pool);
*added = svn_mergeinfo_dup(to, pool);
} else {
*deleted = apr_hash_make(pool);
*added = apr_hash_make(pool);
if (from && to) {
SVN_ERR(walk_mergeinfo_hash_for_diff(from, to, *deleted, *added,
consider_inheritance, pool));
}
}
return SVN_NO_ERROR;
}
svn_error_t *
svn_mergeinfo__equals(svn_boolean_t *is_equal,
svn_mergeinfo_t info1,
svn_mergeinfo_t info2,
svn_boolean_t consider_inheritance,
apr_pool_t *pool) {
if (apr_hash_count(info1) == apr_hash_count(info2)) {
svn_mergeinfo_t deleted, added;
SVN_ERR(svn_mergeinfo_diff(&deleted, &added, info1, info2,
consider_inheritance, pool));
*is_equal = apr_hash_count(deleted) == 0 && apr_hash_count(added) == 0;
} else {
*is_equal = FALSE;
}
return SVN_NO_ERROR;
}
svn_error_t *
svn_mergeinfo_merge(svn_mergeinfo_t mergeinfo, svn_mergeinfo_t changes,
apr_pool_t *pool) {
apr_array_header_t *sorted1, *sorted2;
int i, j;
sorted1 = svn_sort__hash(mergeinfo, svn_sort_compare_items_as_paths, pool);
sorted2 = svn_sort__hash(changes, svn_sort_compare_items_as_paths, pool);
i = 0;
j = 0;
while (i < sorted1->nelts && j < sorted2->nelts) {
svn_sort__item_t elt1, elt2;
int res;
elt1 = APR_ARRAY_IDX(sorted1, i, svn_sort__item_t);
elt2 = APR_ARRAY_IDX(sorted2, j, svn_sort__item_t);
res = svn_sort_compare_items_as_paths(&elt1, &elt2);
if (res == 0) {
apr_array_header_t *rl1, *rl2;
rl1 = elt1.value;
rl2 = elt2.value;
SVN_ERR(svn_rangelist_merge(&rl1, rl2,
pool));
apr_hash_set(mergeinfo, elt1.key, elt1.klen, rl1);
i++;
j++;
} else if (res < 0) {
i++;
} else {
apr_hash_set(mergeinfo, elt2.key, elt2.klen, elt2.value);
j++;
}
}
for (; j < sorted2->nelts; j++) {
svn_sort__item_t elt = APR_ARRAY_IDX(sorted2, j, svn_sort__item_t);
apr_hash_set(mergeinfo, elt.key, elt.klen, elt.value);
}
return SVN_NO_ERROR;
}
svn_error_t *
svn_mergeinfo_intersect(svn_mergeinfo_t *mergeinfo,
svn_mergeinfo_t mergeinfo1,
svn_mergeinfo_t mergeinfo2,
apr_pool_t *pool) {
apr_hash_index_t *hi;
*mergeinfo = apr_hash_make(pool);
for (hi = apr_hash_first(apr_hash_pool_get(mergeinfo1), mergeinfo1);
hi; hi = apr_hash_next(hi)) {
apr_array_header_t *rangelist;
const void *path;
void *val;
apr_hash_this(hi, &path, NULL, &val);
rangelist = apr_hash_get(mergeinfo2, path, APR_HASH_KEY_STRING);
if (rangelist) {
SVN_ERR(svn_rangelist_intersect(&rangelist,
(apr_array_header_t *) val,
rangelist, TRUE, pool));
if (rangelist->nelts > 0)
apr_hash_set(*mergeinfo, path, APR_HASH_KEY_STRING, rangelist);
}
}
return SVN_NO_ERROR;
}
svn_error_t *
svn_mergeinfo_remove(svn_mergeinfo_t *mergeinfo, svn_mergeinfo_t eraser,
svn_mergeinfo_t whiteboard, apr_pool_t *pool) {
*mergeinfo = apr_hash_make(pool);
SVN_ERR(walk_mergeinfo_hash_for_diff(whiteboard, eraser, *mergeinfo, NULL,
TRUE, pool));
return SVN_NO_ERROR;
}
svn_error_t *
svn_rangelist_to_string(svn_string_t **output,
const apr_array_header_t *rangelist,
apr_pool_t *pool) {
svn_stringbuf_t *buf = svn_stringbuf_create("", pool);
if (rangelist->nelts > 0) {
int i;
svn_merge_range_t *range;
svn_string_t *toappend;
for (i = 0; i < rangelist->nelts - 1; i++) {
range = APR_ARRAY_IDX(rangelist, i, svn_merge_range_t *);
SVN_ERR(range_to_string(&toappend, range, pool));
svn_stringbuf_appendcstr(buf, toappend->data);
svn_stringbuf_appendcstr(buf, ",");
}
range = APR_ARRAY_IDX(rangelist, i, svn_merge_range_t *);
SVN_ERR(range_to_string(&toappend, range, pool));
svn_stringbuf_appendcstr(buf, toappend->data);
}
*output = svn_string_create_from_buf(buf, pool);
return SVN_NO_ERROR;
}
static svn_error_t *
mergeinfo_to_stringbuf(svn_stringbuf_t **output, svn_mergeinfo_t input,
apr_pool_t *pool) {
*output = svn_stringbuf_create("", pool);
if (apr_hash_count(input) > 0) {
apr_array_header_t *sorted =
svn_sort__hash(input, svn_sort_compare_items_as_paths, pool);
int i;
for (i = 0; i < sorted->nelts; i++) {
svn_sort__item_t elt = APR_ARRAY_IDX(sorted, i, svn_sort__item_t);
svn_string_t *revlist;
SVN_ERR(svn_rangelist_to_string(&revlist, elt.value, pool));
svn_stringbuf_appendcstr(*output,
apr_psprintf(pool, "%s:%s",
(char *) elt.key,
revlist->data));
if (i < sorted->nelts - 1)
svn_stringbuf_appendcstr(*output, "\n");
}
}
return SVN_NO_ERROR;
}
svn_error_t *
svn_mergeinfo_to_string(svn_string_t **output, svn_mergeinfo_t input,
apr_pool_t *pool) {
if (apr_hash_count(input) > 0) {
svn_stringbuf_t *mergeinfo_buf;
SVN_ERR(mergeinfo_to_stringbuf(&mergeinfo_buf, input, pool));
*output = svn_string_create_from_buf(mergeinfo_buf, pool);
} else {
*output = svn_string_create("", pool);
}
return SVN_NO_ERROR;
}
svn_error_t*
svn_mergeinfo_sort(svn_mergeinfo_t input, apr_pool_t *pool) {
apr_hash_index_t *hi;
void *val;
for (hi = apr_hash_first(pool, input); hi; hi = apr_hash_next(hi)) {
apr_array_header_t *rl;
apr_hash_this(hi, NULL, NULL, &val);
rl = val;
qsort(rl->elts, rl->nelts, rl->elt_size, svn_sort_compare_ranges);
}
return SVN_NO_ERROR;
}
svn_mergeinfo_t
svn_mergeinfo_dup(svn_mergeinfo_t mergeinfo, apr_pool_t *pool) {
svn_mergeinfo_t new_mergeinfo = apr_hash_make(pool);
apr_hash_index_t *hi;
const void *path;
apr_ssize_t pathlen;
void *rangelist;
for (hi = apr_hash_first(pool, mergeinfo); hi; hi = apr_hash_next(hi)) {
apr_hash_this(hi, &path, &pathlen, &rangelist);
apr_hash_set(new_mergeinfo, apr_pstrmemdup(pool, path, pathlen), pathlen,
svn_rangelist_dup((apr_array_header_t *) rangelist, pool));
}
return new_mergeinfo;
}
svn_error_t *
svn_mergeinfo_inheritable(svn_mergeinfo_t *output,
svn_mergeinfo_t mergeinfo,
const char *path,
svn_revnum_t start,
svn_revnum_t end,
apr_pool_t *pool) {
apr_hash_index_t *hi;
const void *key;
apr_ssize_t keylen;
void *rangelist;
svn_mergeinfo_t inheritable_mergeinfo = apr_hash_make(pool);
for (hi = apr_hash_first(pool, mergeinfo); hi; hi = apr_hash_next(hi)) {
apr_array_header_t *inheritable_rangelist;
apr_hash_this(hi, &key, &keylen, &rangelist);
if (!path || svn_path_compare_paths(path, (const char *)key) == 0)
SVN_ERR(svn_rangelist_inheritable(&inheritable_rangelist,
(apr_array_header_t *) rangelist,
start, end, pool));
else
inheritable_rangelist =
svn_rangelist_dup((apr_array_header_t *)rangelist, pool);
apr_hash_set(inheritable_mergeinfo,
apr_pstrmemdup(pool, key, keylen), keylen,
inheritable_rangelist);
}
*output = inheritable_mergeinfo;
return SVN_NO_ERROR;
}
svn_error_t *
svn_rangelist_inheritable(apr_array_header_t **inheritable_rangelist,
apr_array_header_t *rangelist,
svn_revnum_t start,
svn_revnum_t end,
apr_pool_t *pool) {
*inheritable_rangelist = apr_array_make(pool, 1,
sizeof(svn_merge_range_t *));
if (rangelist->nelts) {
if (!SVN_IS_VALID_REVNUM(start)
|| !SVN_IS_VALID_REVNUM(end)
|| end < start) {
int i;
for (i = 0; i < rangelist->nelts; i++) {
svn_merge_range_t *range = APR_ARRAY_IDX(rangelist, i,
svn_merge_range_t *);
if (range->inheritable) {
svn_merge_range_t *inheritable_range =
apr_palloc(pool, sizeof(*inheritable_range));
inheritable_range->start = range->start;
inheritable_range->end = range->end;
inheritable_range->inheritable = TRUE;
APR_ARRAY_PUSH(*inheritable_rangelist,
svn_merge_range_t *) = range;
}
}
} else {
apr_array_header_t *ranges_inheritable =
apr_array_make(pool, 0, sizeof(svn_merge_range_t *));
svn_merge_range_t *range = apr_palloc(pool, sizeof(*range));
range->start = start;
range->end = end;
range->inheritable = FALSE;
APR_ARRAY_PUSH(ranges_inheritable, svn_merge_range_t *) = range;
if (rangelist->nelts)
SVN_ERR(svn_rangelist_remove(inheritable_rangelist,
ranges_inheritable,
rangelist,
TRUE,
pool));
}
}
return SVN_NO_ERROR;
}
svn_boolean_t
svn_mergeinfo__remove_empty_rangelists(svn_mergeinfo_t mergeinfo,
apr_pool_t *pool) {
apr_hash_index_t *hi;
svn_boolean_t removed_some_ranges = FALSE;
if (mergeinfo) {
for (hi = apr_hash_first(pool, mergeinfo); hi; hi = apr_hash_next(hi)) {
const void *key;
void *value;
const char *path;
apr_array_header_t *rangelist;
apr_hash_this(hi, &key, NULL, &value);
path = key;
rangelist = value;
if (rangelist->nelts == 0) {
apr_hash_set(mergeinfo, path, APR_HASH_KEY_STRING, NULL);
removed_some_ranges = TRUE;
}
}
}
return removed_some_ranges;
}
svn_error_t *
svn_mergeinfo__remove_prefix_from_catalog(svn_mergeinfo_catalog_t *out_catalog,
svn_mergeinfo_catalog_t in_catalog,
const char *prefix,
apr_pool_t *pool) {
apr_hash_index_t *hi;
int prefix_len = strlen(prefix);
*out_catalog = apr_hash_make(pool);
for (hi = apr_hash_first(pool, in_catalog); hi; hi = apr_hash_next(hi)) {
const void *key;
const char *original_path;
void *value;
apr_ssize_t klen;
apr_hash_this(hi, &key, &klen, &value);
original_path = key;
assert(klen >= prefix_len);
assert(strncmp(key, prefix, prefix_len) == 0);
apr_hash_set(*out_catalog, original_path + prefix_len, klen-prefix_len, value);
}
return SVN_NO_ERROR;
}
apr_array_header_t *
svn_rangelist_dup(apr_array_header_t *rangelist, apr_pool_t *pool) {
apr_array_header_t *new_rl = apr_array_make(pool, rangelist->nelts,
sizeof(svn_merge_range_t *));
int i;
for (i = 0; i < rangelist->nelts; i++) {
APR_ARRAY_PUSH(new_rl, svn_merge_range_t *) =
svn_merge_range_dup(APR_ARRAY_IDX(rangelist, i, svn_merge_range_t *),
pool);
}
return new_rl;
}
svn_merge_range_t *
svn_merge_range_dup(svn_merge_range_t *range, apr_pool_t *pool) {
svn_merge_range_t *new_range = apr_palloc(pool, sizeof(*new_range));
memcpy(new_range, range, sizeof(*new_range));
return new_range;
}
svn_boolean_t
svn_merge_range_contains_rev(svn_merge_range_t *range, svn_revnum_t rev) {
assert(SVN_IS_VALID_REVNUM(range->start));
assert(SVN_IS_VALID_REVNUM(range->end));
assert(range->start != range->end);
if (range->start < range->end)
return rev > range->start && rev <= range->end;
else
return rev > range->end && rev <= range->start;
}
