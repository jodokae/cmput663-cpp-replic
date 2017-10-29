#include <apr.h>
#include <apr_general.h>
#include "svn_error.h"
#include "svn_diff.h"
#include "svn_types.h"
#include "svn_ctype.h"
#include "diff.h"
#define ADLER_MOD_BASE 65521
#define ADLER_MOD_BLOCK_SIZE 5552
apr_uint32_t
svn_diff__adler32(apr_uint32_t checksum, const char *data, apr_size_t len) {
const unsigned char *input = (const unsigned char *)data;
apr_uint32_t s1 = checksum & 0xFFFF;
apr_uint32_t s2 = checksum >> 16;
apr_uint32_t b;
apr_size_t blocks = len / ADLER_MOD_BLOCK_SIZE;
len %= ADLER_MOD_BLOCK_SIZE;
while (blocks--) {
int count = ADLER_MOD_BLOCK_SIZE;
while (count--) {
b = *input++;
s1 += b;
s2 += s1;
}
s1 %= ADLER_MOD_BASE;
s2 %= ADLER_MOD_BASE;
}
while (len--) {
b = *input++;
s1 += b;
s2 += s1;
}
return ((s2 % ADLER_MOD_BASE) << 16) | (s1 % ADLER_MOD_BASE);
}
svn_boolean_t
svn_diff_contains_conflicts(svn_diff_t *diff) {
while (diff != NULL) {
if (diff->type == svn_diff__type_conflict) {
return TRUE;
}
diff = diff->next;
}
return FALSE;
}
svn_boolean_t
svn_diff_contains_diffs(svn_diff_t *diff) {
while (diff != NULL) {
if (diff->type != svn_diff__type_common) {
return TRUE;
}
diff = diff->next;
}
return FALSE;
}
svn_error_t *
svn_diff_output(svn_diff_t *diff,
void *output_baton,
const svn_diff_output_fns_t *vtable) {
svn_error_t *(*output_fn)(void *,
apr_off_t, apr_off_t,
apr_off_t, apr_off_t,
apr_off_t, apr_off_t);
while (diff != NULL) {
switch (diff->type) {
case svn_diff__type_common:
output_fn = vtable->output_common;
break;
case svn_diff__type_diff_common:
output_fn = vtable->output_diff_common;
break;
case svn_diff__type_diff_modified:
output_fn = vtable->output_diff_modified;
break;
case svn_diff__type_diff_latest:
output_fn = vtable->output_diff_latest;
break;
case svn_diff__type_conflict:
output_fn = NULL;
if (vtable->output_conflict != NULL) {
SVN_ERR(vtable->output_conflict(output_baton,
diff->original_start, diff->original_length,
diff->modified_start, diff->modified_length,
diff->latest_start, diff->latest_length,
diff->resolved_diff));
}
break;
default:
output_fn = NULL;
break;
}
if (output_fn != NULL) {
SVN_ERR(output_fn(output_baton,
diff->original_start, diff->original_length,
diff->modified_start, diff->modified_length,
diff->latest_start, diff->latest_length));
}
diff = diff->next;
}
return SVN_NO_ERROR;
}
void
svn_diff__normalize_buffer(char **tgt,
apr_off_t *lengthp,
svn_diff__normalize_state_t *statep,
const char *buf,
const svn_diff_file_options_t *opts) {
const char *curp, *endp;
svn_diff__normalize_state_t state = *statep;
const char *start = buf;
apr_size_t include_len = 0;
svn_boolean_t last_skipped = FALSE;
char *tgt_newend = *tgt;
if (! opts->ignore_space && ! opts->ignore_eol_style) {
*tgt = (char *)buf;
return;
}
#define SKIP do { if (start == curp) ++start; last_skipped = TRUE; } while (0)
#define INCLUDE do { if (last_skipped) COPY_INCLUDED_SECTION; ++include_len; last_skipped = FALSE; } while (0)
#define COPY_INCLUDED_SECTION do { if (include_len > 0) { memmove(tgt_newend, start, include_len); tgt_newend += include_len; include_len = 0; } start = curp; } while (0)
#define INCLUDE_AS(x) do { if (*curp == (x)) INCLUDE; else { INSERT((x)); SKIP; } } while (0)
#define INSERT(x) do { COPY_INCLUDED_SECTION; *tgt_newend++ = (x); } while (0)
for (curp = buf, endp = buf + *lengthp; curp != endp; ++curp) {
switch (*curp) {
case '\r':
if (opts->ignore_eol_style)
INCLUDE_AS('\n');
else
INCLUDE;
state = svn_diff__normalize_state_cr;
break;
case '\n':
if (state == svn_diff__normalize_state_cr
&& opts->ignore_eol_style)
SKIP;
else
INCLUDE;
state = svn_diff__normalize_state_normal;
break;
default:
if (svn_ctype_isspace(*curp)
&& opts->ignore_space != svn_diff_file_ignore_space_none) {
if (state != svn_diff__normalize_state_whitespace
&& opts->ignore_space
== svn_diff_file_ignore_space_change)
INCLUDE_AS(' ');
else
SKIP;
state = svn_diff__normalize_state_whitespace;
} else {
INCLUDE;
state = svn_diff__normalize_state_normal;
}
}
}
if (*tgt == tgt_newend) {
*tgt = (char *)start;
*lengthp = include_len;
} else {
COPY_INCLUDED_SECTION;
*lengthp = tgt_newend - *tgt;
}
*statep = state;
#undef SKIP
#undef INCLUDE
#undef INCLUDE_AS
#undef INSERT
#undef COPY_INCLUDED_SECTION
}
const svn_version_t *
svn_diff_version(void) {
SVN_VERSION_BODY;
}
