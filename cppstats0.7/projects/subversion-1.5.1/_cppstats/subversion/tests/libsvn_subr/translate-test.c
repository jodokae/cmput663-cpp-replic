#include <stdio.h>
#include <string.h>
#include <apr_general.h>
#include <apr_file_io.h>
#include "svn_pools.h"
#include "svn_subst.h"
#include "../svn_test.h"
const char *lines[] = {
"Line 1: fairly boring subst test data... blah blah",
"Line 2: fairly boring subst test data... blah blah.",
"Line 3: Valid $LastChangedRevision$, started unexpanded.",
"Line 4: fairly boring subst test data... blah blah.",
"Line 5: Valid $Rev$, started unexpanded.",
"Line 6: fairly boring subst test data... blah blah.",
"Line 7: fairly boring subst test data... blah blah.",
"Line 8: Valid $LastChangedBy$, started unexpanded.",
"Line 9: Valid $Author$, started unexpanded.",
"Line 10: fairly boring subst test data... blah blah.",
"Line 11: fairly boring subst test data... blah blah.",
"Line 12: Valid $LastChangedDate$, started unexpanded.",
"Line 13: Valid $Date$, started unexpanded.",
"Line 14: fairly boring subst test data... blah blah.",
"Line 15: fairly boring subst test data... blah blah.",
"Line 16: Valid $HeadURL$, started unexpanded.",
"Line 17: Valid $URL$, started unexpanded.",
"Line 18: fairly boring subst test data... blah blah.",
"Line 19: Invalid expanded keyword spanning two lines: $Author: ",
"Line 20: jrandom$ remainder of invalid keyword spanning two lines.",
"Line 21: fairly boring subst test data... blah blah.",
"Line 22: an unknown keyword $LastChangedSocks$.",
"Line 23: fairly boring subst test data... blah blah.",
"Line 24: keyword in a keyword: $Author: $Date$ $",
"Line 25: fairly boring subst test data... blah blah.",
"Line 26: Emptily expanded keyword $Rev: $.",
"Line 27: fairly boring subst test data... blah blah.",
"Line 28: fairly boring subst test data... blah blah.",
"Line 29: Valid $LastChangedRevision: 1729 $, started expanded.",
"Line 30: Valid $Rev: 1729 $, started expanded.",
"Line 31: fairly boring subst test data... blah blah.",
"Line 32: fairly boring subst test data... blah blah.",
"Line 33: Valid $LastChangedDate: 2002-01-01 $, started expanded.",
"Line 34: Valid $Date: 2002-01-01 $, started expanded.",
"Line 35: fairly boring subst test data... blah blah.",
"Line 36: fairly boring subst test data... blah blah.",
"Line 37: Valid $LastChangedBy: jrandom $, started expanded.",
"Line 38: Valid $Author: jrandom $, started expanded.",
"Line 39: fairly boring subst test data... blah blah.",
"Line 40: fairly boring subst test data... blah blah.",
"Line 41: Valid $HeadURL: http://tomato/mauve $, started expanded.",
"Line 42: Valid $URL: http://tomato/mauve $, started expanded.",
"Line 43: fairly boring subst test data... blah blah.",
"Line 44: fairly boring subst test data... blah blah.",
"Line 45: Invalid $LastChangedRevisionWithSuffix$, started unexpanded.",
"Line 46: Empty $Author:$, started expanded.",
"Line 47: fairly boring subst test data... blah blah.",
"Line 48: Two keywords back to back: $Author$$Rev$.",
"Line 49: One keyword, one not, back to back: $Author$Rev$.",
"Line 50: a series of dollar signs $$$$$$$$$$$$$$$$$$$$$$$$$$$$.",
"Line 51: same, but with embedded keyword $$$$$$$$Date$$$$$$$$$$.",
"Line 52: same, with expanded, empty keyword $$$$$$Date: $$$$$$.",
"Line 53: $This is a lengthy line designed to test a bug that was "
"reported about keyword expansion. The problem was that a line "
"had more than SVN_KEYWORD_MAX_LEN (255 at the time) characters "
"after an initial dollar sign, which triggered a buglet in our "
"svn_subst_copy_and_translate() function and resulted in, in some cases "
"a SEGFAULT, and in others a filthy corrupt commit. ",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"",
"$Author$Rev$.",
".$veR$Author$",
"$",
"$$",
"Line 74: Valid $Author: jran$dom $, started expanded.",
"Line 75: Valid $URL: http://tomato/mau$ve $, started expanded.",
"$ "
" "
" "
" $$",
"$ "
" "
" "
" $$",
"$ "
" "
" "
" $$",
"Line 79: Valid $Author: aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
"aaaaaaaaaaaaa$aaaaaaaaaaaaaaaaaaaaaa$aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa$aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa $, started expanded.",
"Line 80: Valid $Author: aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa$aaaaaaaaaaaaaaa"
"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa$aaaaaaaaaaaaaaaaaa"
"aaaaaaa$aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa$aaaaaaa $, started "
"expanded.",
"Line 81: Valid $Author: aaaaaaaaaaaaaaaaaaaa$aaaaaaaaaaaaaaaaaaaaaaaaaaa"
"aaaaaaaaaaaaa$aaaaaaaaaaaaaaaaaaaa$$aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa$$aaaaaaaaaaaaaaaaaaaaaaaaa"
"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa$$aaaaaa$$$ $, started "
"expanded.",
"Line 82: Valid $Author: aaaaaaaaaaa$$aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
"aaaaaaaaaaaaaaaaaaaaaaaaaa$$$$aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa$$aaaaaaaaaaaaaaaaaaaaaaaaaaa"
"aaaaaaaaaaaaaa$$aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa$$$ $, started "
"expanded.",
"Line 83: Invalid $Author: aaaaaaaaaaa$$aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
"aaaaaaaaaaaaaaaaaaaaaaaaaa$$$$aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa$$aaaaaaaaaaaaaaaaaaaaaaaaaaa"
"aaaaaaaaaaaaaa$$aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa$$$ $, started "
"expanded.",
"Line 84: Invalid $Author: aaaaaaaaaaa$$aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
"aaaaaaaaaaaaaaaaaaaaaaaaaa$$$$aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa$$aaaaaaaaaaaaaaaaaaaaaaaaaaa"
"aaaaaaaaaaaaaa$$aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa$$$ $, started "
"expanded.",
"Line 85: end of subst test data."
};
static const char *
random_eol_marker(void) {
static int seeded = 0;
const char *eol_markers[] = { "\n", "\r\n" };
if (! seeded) {
srand(1729);
seeded = 1;
}
return eol_markers[rand()
% ((sizeof(eol_markers)) / (sizeof(*eol_markers)))];
}
static svn_error_t *
create_file(const char *fname, const char *eol_str, apr_pool_t *pool) {
apr_status_t apr_err;
apr_file_t *f;
apr_size_t i, j;
apr_err = apr_file_open(&f, fname,
(APR_WRITE | APR_CREATE | APR_EXCL | APR_BINARY),
APR_OS_DEFAULT, pool);
if (apr_err)
return svn_error_create(apr_err, NULL, fname);
for (i = 0; i < (sizeof(lines) / sizeof(*lines)); i++) {
const char *this_eol_str = eol_str ? eol_str : random_eol_marker();
apr_err = apr_file_printf(f, lines[i]);
for (j = 0; this_eol_str[j]; j++) {
apr_err = apr_file_putc(this_eol_str[j], f);
if (apr_err)
return svn_error_create(apr_err, NULL, fname);
}
}
apr_err = apr_file_close(f);
if (apr_err)
return svn_error_create(apr_err, NULL, fname);
return SVN_NO_ERROR;
}
static svn_error_t *
remove_file(const char *fname, apr_pool_t *pool) {
apr_status_t apr_err;
apr_finfo_t finfo;
if (apr_stat(&finfo, fname, APR_FINFO_TYPE, pool) == APR_SUCCESS) {
if (finfo.filetype == APR_REG) {
apr_err = apr_file_remove(fname, pool);
if (apr_err)
return svn_error_create(apr_err, NULL, fname);
} else
return svn_error_createf(SVN_ERR_TEST_FAILED, NULL,
"non-file '%s' is in the way", fname);
}
return SVN_NO_ERROR;
}
static svn_error_t *
substitute_and_verify(const char *test_name,
const char *src_eol,
const char *dst_eol,
svn_boolean_t repair,
const char *rev,
const char *date,
const char *author,
const char *url,
svn_boolean_t expand,
apr_pool_t *pool) {
svn_error_t *err;
svn_stringbuf_t *contents;
apr_hash_t *keywords = apr_hash_make(pool);
apr_size_t idx = 0;
apr_size_t i;
const char *expect[(sizeof(lines) / sizeof(*lines))];
const char *src_fname = apr_pstrcat(pool, test_name, ".src", NULL);
const char *dst_fname = apr_pstrcat(pool, test_name, ".dst", NULL);
svn_string_t *val;
apr_pool_t *subpool = svn_pool_create(pool);
SVN_ERR(remove_file(src_fname, pool));
SVN_ERR(remove_file(dst_fname, pool));
SVN_ERR(create_file(src_fname, src_eol, pool));
if (rev) {
val = svn_string_create(rev, pool);
apr_hash_set(keywords, SVN_KEYWORD_REVISION_LONG,
APR_HASH_KEY_STRING, val);
apr_hash_set(keywords, SVN_KEYWORD_REVISION_MEDIUM,
APR_HASH_KEY_STRING, val);
apr_hash_set(keywords, SVN_KEYWORD_REVISION_SHORT,
APR_HASH_KEY_STRING, val);
}
if (date) {
val = svn_string_create(date, pool);
apr_hash_set(keywords, SVN_KEYWORD_DATE_LONG,
APR_HASH_KEY_STRING, val);
apr_hash_set(keywords, SVN_KEYWORD_DATE_SHORT,
APR_HASH_KEY_STRING, val);
}
if (author) {
val = svn_string_create(author, pool);
apr_hash_set(keywords, SVN_KEYWORD_AUTHOR_LONG,
APR_HASH_KEY_STRING, val);
apr_hash_set(keywords, SVN_KEYWORD_AUTHOR_SHORT,
APR_HASH_KEY_STRING, val);
}
if (url) {
val = svn_string_create(url, pool);
apr_hash_set(keywords, SVN_KEYWORD_URL_LONG,
APR_HASH_KEY_STRING, val);
apr_hash_set(keywords, SVN_KEYWORD_URL_SHORT,
APR_HASH_KEY_STRING, val);
}
err = svn_subst_copy_and_translate3(src_fname, dst_fname, dst_eol, repair,
keywords, expand, FALSE, subpool);
svn_pool_destroy(subpool);
if ((! src_eol) && dst_eol && (! repair)) {
if (! err) {
return svn_error_createf
(SVN_ERR_TEST_FAILED, NULL,
"translation of '%s' should have failed, but didn't", src_fname);
} else if (err->apr_err != SVN_ERR_IO_INCONSISTENT_EOL) {
return svn_error_createf
(SVN_ERR_TEST_FAILED, err,
"translation of '%s' should fail, but not with this error",
src_fname);
} else {
svn_error_clear(err);
SVN_ERR(remove_file(src_fname, pool));
return SVN_NO_ERROR;
}
} else if (err)
return err;
for (i = 0; i < (sizeof(expect) / sizeof(*expect)); i++)
expect[i] = lines[i];
if (rev) {
if (expand) {
expect[3 - 1] =
apr_pstrcat(pool, "Line 3: ",
"Valid $LastChangedRevision: ",
rev,
" $, started unexpanded.",
NULL);
expect[5 - 1] =
apr_pstrcat(pool, "Line 5: ",
"Valid $Rev: ", rev, " $, started unexpanded.",
NULL);
expect[26 - 1] =
apr_pstrcat(pool, "Line 26: ",
"Emptily expanded keyword $Rev: ", rev," $.",
NULL);
expect[29 - 1] =
apr_pstrcat(pool, "Line 29: ",
"Valid $LastChangedRevision: ",
rev,
" $, started expanded.",
NULL);
expect[30 - 1] =
apr_pstrcat(pool, "Line 30: ",
"Valid $Rev: ",
rev,
" $, started expanded.",
NULL);
} else {
expect[26 - 1] =
"Line 26: Emptily expanded keyword $Rev$.";
expect[29 - 1] =
"Line 29: Valid $LastChangedRevision$, started expanded.";
expect[30 - 1] =
"Line 30: Valid $Rev$, started expanded.";
}
}
if (date) {
if (expand) {
expect[12 - 1] =
apr_pstrcat(pool, "Line 12: ",
"Valid $LastChangedDate: ",
date,
" $, started unexpanded.",
NULL);
expect[13 - 1] =
apr_pstrcat(pool, "Line 13: ",
"Valid $Date: ", date, " $, started unexpanded.",
NULL);
expect[33 - 1] =
apr_pstrcat(pool, "Line 33: ",
"Valid $LastChangedDate: ",
date,
" $, started expanded.",
NULL);
expect[34 - 1] =
apr_pstrcat(pool, "Line 34: ",
"Valid $Date: ", date, " $, started expanded.",
NULL);
expect[51 - 1] =
apr_pstrcat(pool, "Line 51: ",
"same, but with embedded keyword ",
"$$$$$$$$Date: ", date, " $$$$$$$$$$.",
NULL);
expect[52 - 1] =
apr_pstrcat(pool, "Line 52: ",
"same, with expanded, empty keyword ",
"$$$$$$Date: ", date, " $$$$$$.",
NULL);
} else {
expect[33 - 1] =
"Line 33: Valid $LastChangedDate$, started expanded.";
expect[34 - 1] =
"Line 34: Valid $Date$, started expanded.";
expect[51 - 1] =
"Line 51: same, but with embedded keyword $$$$$$$$Date$$$$$$$$$$.";
expect[52 - 1] =
"Line 52: same, with expanded, empty keyword $$$$$$Date$$$$$$.";
}
}
if (author) {
if (expand) {
expect[8 - 1] =
apr_pstrcat(pool, "Line 8: ",
"Valid $LastChangedBy: ",
author,
" $, started unexpanded.",
NULL);
expect[9 - 1] =
apr_pstrcat(pool, "Line 9: ",
"Valid $Author: ", author, " $, started unexpanded.",
NULL);
expect[37 - 1] =
apr_pstrcat(pool, "Line 37: ",
"Valid $LastChangedBy: ", author,
" $, started expanded.", NULL);
expect[38 - 1] =
apr_pstrcat(pool, "Line 38: ",
"Valid $Author: ", author, " $, started expanded.",
NULL);
expect[46 - 1] =
apr_pstrcat(pool, "Line 46: ",
"Empty $Author: ", author, " $, started expanded.",
NULL);
expect[71 - 1] =
apr_pstrcat(pool, ".$veR$Author: ", author, " $", NULL);
expect[74 - 1] =
apr_pstrcat(pool, "Line 74: ",
"Valid $Author: ", author, " $, started expanded.",
NULL);
expect[79 - 1] =
apr_pstrcat(pool, "Line 79: ",
"Valid $Author: ", author, " $, started expanded.",
NULL);
expect[80 - 1] =
apr_pstrcat(pool, "Line 80: ",
"Valid $Author: ", author, " $, started expanded.",
NULL);
expect[81 - 1] =
apr_pstrcat(pool, "Line 81: ",
"Valid $Author: ", author, " $, started expanded.",
NULL);
expect[82 - 1] =
apr_pstrcat(pool, "Line 82: ",
"Valid $Author: ", author, " $, started expanded.",
NULL);
} else {
expect[37 - 1] =
"Line 37: Valid $LastChangedBy$, started expanded.";
expect[38 - 1] =
"Line 38: Valid $Author$, started expanded.";
expect[46 - 1] =
"Line 46: Empty $Author$, started expanded.";
expect[74 - 1] =
"Line 74: Valid $Author$, started expanded.";
expect[79 - 1] =
"Line 79: Valid $Author$, started expanded.";
expect[80 - 1] =
"Line 80: Valid $Author$, started expanded.";
expect[81 - 1] =
"Line 81: Valid $Author$, started expanded.";
expect[82 - 1] =
"Line 82: Valid $Author$, started expanded.";
}
}
if (url) {
if (expand) {
expect[16 - 1] =
apr_pstrcat(pool, "Line 16: ",
"Valid $HeadURL: ", url, " $, started unexpanded.",
NULL);
expect[17 - 1] =
apr_pstrcat(pool, "Line 17: ",
"Valid $URL: ", url, " $, started unexpanded.",
NULL);
expect[41 - 1] =
apr_pstrcat(pool, "Line 41: ",
"Valid $HeadURL: ", url, " $, started expanded.",
NULL);
expect[42 - 1] =
apr_pstrcat(pool, "Line 42: ",
"Valid $URL: ", url, " $, started expanded.",
NULL);
expect[75 - 1] =
apr_pstrcat(pool, "Line 75: ",
"Valid $URL: ", url, " $, started expanded.",
NULL);
} else {
expect[41 - 1] =
"Line 41: Valid $HeadURL$, started expanded.";
expect[42 - 1] =
"Line 42: Valid $URL$, started expanded.";
expect[75 - 1] =
"Line 75: Valid $URL$, started expanded.";
}
}
if (rev && author) {
if (expand) {
expect[48 - 1] =
apr_pstrcat(pool, "Line 48: ",
"Two keywords back to back: "
"$Author: ", author, " $"
"$Rev: ", rev, " $.",
NULL);
expect[49 - 1] =
apr_pstrcat(pool, "Line 49: ",
"One keyword, one not, back to back: "
"$Author: ", author, " $Rev$.",
NULL);
expect[70 - 1] =
apr_pstrcat(pool, "$Author: ", author, " $Rev$.", NULL);
}
} else if (rev && (! author)) {
if (expand) {
expect[48 - 1] =
apr_pstrcat(pool, "Line 48: ",
"Two keywords back to back: "
"$Author$$Rev: ", rev, " $.",
NULL);
expect[49 - 1] =
apr_pstrcat(pool, "Line 49: ",
"One keyword, one not, back to back: "
"$Author$Rev: ", rev, " $.",
NULL);
expect[70 - 1] =
apr_pstrcat(pool, "$Author$Rev: ", rev, " $.", NULL);
}
} else if ((! rev) && author) {
if (expand) {
expect[48 - 1] =
apr_pstrcat(pool, "Line 48: ",
"Two keywords back to back: "
"$Author: ", author, " $$Rev$.",
NULL);
expect[49 - 1] =
apr_pstrcat(pool, "Line 49: ",
"One keyword, one not, back to back: "
"$Author: ", author, " $Rev$.",
NULL);
expect[70 - 1] =
apr_pstrcat(pool, "$Author: ", author, " $Rev$.", NULL);
}
}
if (date && author) {
if (expand) {
expect[24 - 1] =
apr_pstrcat(pool, "Line 24: ",
"keyword in a keyword: $Author: ",
author,
" $Date$ $",
NULL);
} else {
expect[24 - 1] =
apr_pstrcat(pool, "Line 24: ",
"keyword in a keyword: $Author$Date$ $",
NULL);
}
} else if (date && (! author)) {
if (expand) {
expect[24 - 1] =
apr_pstrcat(pool, "Line 24: ",
"keyword in a keyword: $Author: $Date: ",
date,
" $ $",
NULL);
}
} else if ((! date) && author) {
if (expand) {
expect[24 - 1] =
apr_pstrcat(pool, "Line 24: ",
"keyword in a keyword: $Author: ",
author,
" $Date$ $",
NULL);
} else {
expect[24 - 1] =
apr_pstrcat(pool, "Line 24: ",
"keyword in a keyword: $Author$Date$ $",
NULL);
}
}
SVN_ERR(svn_stringbuf_from_file(&contents, dst_fname, pool));
for (i = 0; i < (sizeof(expect) / sizeof(*expect)); i++) {
if (contents->len < idx)
return svn_error_createf
(SVN_ERR_MALFORMED_FILE, NULL,
"'%s' has short contents at line %" APR_SIZE_T_FMT,
dst_fname, i + 1);
if (strncmp(contents->data + idx, expect[i], strlen(expect[i])) != 0)
return svn_error_createf
(SVN_ERR_MALFORMED_FILE, NULL,
"'%s' has wrong contents at line %" APR_SIZE_T_FMT,
dst_fname, i + 1);
idx += strlen(expect[i]);
if (dst_eol) {
if (strncmp(contents->data + idx, dst_eol, strlen(dst_eol)) != 0)
return svn_error_createf
(SVN_ERR_IO_UNKNOWN_EOL, NULL,
"'%s' has wrong eol style at line %" APR_SIZE_T_FMT, dst_fname,
i + 1);
else
idx += strlen(dst_eol);
} else {
while ((*(contents->data + idx) == '\r')
|| (*(contents->data + idx) == '\n'))
idx++;
}
}
SVN_ERR(remove_file(src_fname, pool));
SVN_ERR(remove_file(dst_fname, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
noop(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
*msg = "no conversions";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(substitute_and_verify
("noop", NULL, NULL, 0, NULL, NULL, NULL, NULL, 1, pool));
SVN_ERR(substitute_and_verify
("noop", "\r", NULL, 0, NULL, NULL, NULL, NULL, 1, pool));
SVN_ERR(substitute_and_verify
("noop", "\n", NULL, 0, NULL, NULL, NULL, NULL, 1, pool));
SVN_ERR(substitute_and_verify
("noop", "\r\n", NULL, 0, NULL, NULL, NULL, NULL, 1, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
crlf_to_crlf(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
*msg = "convert CRLF to CRLF";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(substitute_and_verify
("crlf_to_crlf", "\r\n", "\r\n", 0,
NULL, NULL, NULL, NULL, 1, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
lf_to_crlf(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
*msg = "convert LF to CRLF";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(substitute_and_verify
("lf_to_crlf", "\n", "\r\n", 0, NULL, NULL, NULL, NULL, 1, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
cr_to_crlf(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
*msg = "convert CR to CRLF";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(substitute_and_verify
("cr_to_crlf", "\r", "\r\n", 0, NULL, NULL, NULL, NULL, 1, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
mixed_to_crlf(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
*msg = "convert mixed line endings to CRLF";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(substitute_and_verify
("mixed_to_crlf", NULL, "\r\n", 1,
NULL, NULL, NULL, NULL, 1, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
lf_to_lf(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
*msg = "convert LF to LF";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(substitute_and_verify
("lf_to_lf", "\n", "\n", 0, NULL, NULL, NULL, NULL, 1, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
crlf_to_lf(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
*msg = "convert CRLF to LF";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(substitute_and_verify
("crlf_to_lf", "\r\n", "\n", 0, NULL, NULL, NULL, NULL, 1, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
cr_to_lf(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
*msg = "convert CR to LF";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(substitute_and_verify
("cr_to_lf", "\r", "\n", 0, NULL, NULL, NULL, NULL, 1, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
mixed_to_lf(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
*msg = "convert mixed line endings to LF";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(substitute_and_verify
("cr_to_lf", NULL, "\n", 1, NULL, NULL, NULL, NULL, 1, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
crlf_to_cr(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
*msg = "convert CRLF to CR";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(substitute_and_verify
("crlf_to_cr", "\r\n", "\r", 0, NULL, NULL, NULL, NULL, 1, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
lf_to_cr(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
*msg = "convert LF to CR";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(substitute_and_verify
("lf_to_cr", "\n", "\r", 0, NULL, NULL, NULL, NULL, 1, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
cr_to_cr(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
*msg = "convert CR to CR";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(substitute_and_verify
("cr_to_cr", "\r", "\r", 0, NULL, NULL, NULL, NULL, 1, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
mixed_to_cr(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
*msg = "convert mixed line endings to CR";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(substitute_and_verify
("mixed_to_cr", NULL, "\r", 1, NULL, NULL, NULL, NULL, 1, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
mixed_no_repair(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
*msg = "keep mixed line endings without repair flag";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(substitute_and_verify
("mixed_no_repair", NULL, "\n", 0,
NULL, NULL, NULL, NULL, 1, pool));
SVN_ERR(substitute_and_verify
("mixed_no_repair", NULL, "\r\n", 0,
NULL, NULL, NULL, NULL, 1, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
expand_author(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
*msg = "expand author";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(substitute_and_verify
("author", "\n", NULL, 0, NULL, NULL, "jrandom", NULL, 1, pool));
SVN_ERR(substitute_and_verify
("author", "\r\n", NULL, 0, NULL, NULL, "jrandom", NULL, 1, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
expand_date(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
*msg = "expand date";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(substitute_and_verify
("date", "\n", NULL, 0,
NULL, "Wed Jan 9 07:49:05 2002", NULL, NULL, 1, pool));
SVN_ERR(substitute_and_verify
("date", "\r\n", NULL, 0,
NULL, "Wed Jan 9 07:49:05 2002", NULL, NULL, 1, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
expand_author_date(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
*msg = "expand author and date";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(substitute_and_verify
("author_date", "\n", NULL, 0,
NULL, "Wed Jan 9 07:49:05 2002", "jrandom", NULL, 1, pool));
SVN_ERR(substitute_and_verify
("author_date", "\r\n", NULL, 0,
NULL, "Wed Jan 9 07:49:05 2002", "jrandom", NULL, 1, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
expand_author_rev(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
*msg = "expand author and rev";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(substitute_and_verify
("author_rev", "\n", NULL, 0,
"1729", NULL, "jrandom", NULL, 1, pool));
SVN_ERR(substitute_and_verify
("author_rev", "\r\n", NULL, 0,
"1729", NULL, "jrandom", NULL, 1, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
expand_rev(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
*msg = "expand rev";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(substitute_and_verify
("rev", "\n", NULL, 0,
"1729", NULL, NULL, NULL, 1, pool));
SVN_ERR(substitute_and_verify
("rev", "\r\n", NULL, 0,
"1729", NULL, NULL, NULL, 1, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
expand_rev_url(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
*msg = "expand rev and url";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(substitute_and_verify
("rev_url", "\n", NULL, 0,
"1729", NULL, NULL, "http://subversion.tigris.org", 1, pool));
SVN_ERR(substitute_and_verify
("rev_url", "\r\n", NULL, 0,
"1729", NULL, NULL, "http://subversion.tigris.org", 1, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
expand_author_date_rev_url(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
*msg = "expand author, date, rev, and url";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(substitute_and_verify
("author_date_rev_url", "\n", NULL, 0,
"1729",
"Wed Jan 9 07:49:05 2002",
"jrandom",
"http://subversion.tigris.org",
1, pool));
SVN_ERR(substitute_and_verify
("author_date_rev_url", "\r\n", NULL, 0,
"1729",
"Wed Jan 9 07:49:05 2002",
"jrandom",
"http://subversion.tigris.org",
1, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
lf_to_crlf_expand_author(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
*msg = "lf_to_crlf; expand author";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(substitute_and_verify
("lf_to_crlf_author", "\n", "\r\n", 0,
NULL, NULL, "jrandom", NULL, 1, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
mixed_to_lf_expand_author_date(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
*msg = "mixed_to_lf; expand author and date";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(substitute_and_verify
("mixed_to_lf_author_date", NULL, "\n", 1,
NULL, "Wed Jan 9 07:49:05 2002", "jrandom", NULL, 1, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
crlf_to_cr_expand_author_rev(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
*msg = "crlf_to_cr; expand author and rev";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(substitute_and_verify
("crlf_to_cr_author_rev", "\r\n", "\r", 0,
"1729", NULL, "jrandom", NULL, 1, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
cr_to_crlf_expand_rev(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
*msg = "cr_to_crlf; expand rev";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(substitute_and_verify
("cr_to_crlf_rev", "\r", "\r\n", 0,
"1729", NULL, NULL, NULL, 1, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
cr_to_crlf_expand_rev_url(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
*msg = "cr_to_crlf; expand rev and url";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(substitute_and_verify
("cr_to_crlf_rev_url", "\r", "\r\n", 0,
"1729", NULL, NULL, "http://subversion.tigris.org", 1, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
mixed_to_crlf_expand_author_date_rev_url(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
*msg = "mixed_to_crlf; expand author, date, rev, and url";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(substitute_and_verify
("mixed_to_crlf_author_date_rev_url", NULL, "\r\n", 1,
"1729",
"Wed Jan 9 07:49:05 2002",
"jrandom",
"http://subversion.tigris.org",
1,
pool));
return SVN_NO_ERROR;
}
static svn_error_t *
unexpand_author(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
*msg = "unexpand author";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(substitute_and_verify
("author", "\n", NULL, 0, NULL, NULL, "jrandom", NULL, 0, pool));
SVN_ERR(substitute_and_verify
("author", "\r\n", NULL, 0, NULL, NULL, "jrandom", NULL, 0, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
unexpand_date(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
*msg = "unexpand date";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(substitute_and_verify
("date", "\n", NULL, 0,
NULL, "Wed Jan 9 07:49:05 2002", NULL, NULL, 0, pool));
SVN_ERR(substitute_and_verify
("date", "\r\n", NULL, 0,
NULL, "Wed Jan 9 07:49:05 2002", NULL, NULL, 0, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
unexpand_author_date(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
*msg = "unexpand author and date";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(substitute_and_verify
("author_date", "\n", NULL, 0,
NULL, "Wed Jan 9 07:49:05 2002", "jrandom", NULL, 0, pool));
SVN_ERR(substitute_and_verify
("author_date", "\r\n", NULL, 0,
NULL, "Wed Jan 9 07:49:05 2002", "jrandom", NULL, 0, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
unexpand_author_rev(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
*msg = "unexpand author and rev";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(substitute_and_verify
("author_rev", "\n", NULL, 0,
"1729", NULL, "jrandom", NULL, 0, pool));
SVN_ERR(substitute_and_verify
("author_rev", "\r\n", NULL, 0,
"1729", NULL, "jrandom", NULL, 0, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
unexpand_rev(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
*msg = "unexpand rev";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(substitute_and_verify
("rev", "\n", NULL, 0,
"1729", NULL, NULL, NULL, 0, pool));
SVN_ERR(substitute_and_verify
("rev", "\r\n", NULL, 0,
"1729", NULL, NULL, NULL, 0, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
unexpand_rev_url(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
*msg = "unexpand rev and url";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(substitute_and_verify
("rev_url", "\n", NULL, 0,
"1729", NULL, NULL, "http://subversion.tigris.org", 0, pool));
SVN_ERR(substitute_and_verify
("rev_url", "\r\n", NULL, 0,
"1729", NULL, NULL, "http://subversion.tigris.org", 0, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
unexpand_author_date_rev_url(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
*msg = "unexpand author, date, rev, and url";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(substitute_and_verify
("author_date_rev_url", "\n", NULL, 0,
"1729",
"Wed Jan 9 07:49:05 2002",
"jrandom",
"http://subversion.tigris.org",
1, pool));
SVN_ERR(substitute_and_verify
("author_date_rev_url", "\r\n", NULL, 0,
"1729",
"Wed Jan 9 07:49:05 2002",
"jrandom",
"http://subversion.tigris.org",
1, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
lf_to_crlf_unexpand_author(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
*msg = "lf_to_crlf; unexpand author";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(substitute_and_verify
("lf_to_crlf_author", "\n", "\r\n", 0,
NULL, NULL, "jrandom", NULL, 0, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
mixed_to_lf_unexpand_author_date(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
*msg = "mixed_to_lf; unexpand author and date";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(substitute_and_verify
("mixed_to_lf_author_date", NULL, "\n", 1,
NULL, "Wed Jan 9 07:49:05 2002", "jrandom", NULL, 0, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
crlf_to_cr_unexpand_author_rev(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
*msg = "crlf_to_cr; unexpand author and rev";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(substitute_and_verify
("crlf_to_cr_author_rev", "\r\n", "\r", 0,
"1729", NULL, "jrandom", NULL, 0, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
cr_to_crlf_unexpand_rev(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
*msg = "cr_to_crlf; unexpand rev";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(substitute_and_verify
("cr_to_crlf_rev", "\r", "\r\n", 0,
"1729", NULL, NULL, NULL, 0, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
cr_to_crlf_unexpand_rev_url(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
*msg = "cr_to_crlf; unexpand rev and url";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(substitute_and_verify
("cr_to_crlf_rev_url", "\r", "\r\n", 0,
"1729", NULL, NULL, "http://subversion.tigris.org", 0, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
mixed_to_crlf_unexpand_author_date_rev_url(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
*msg = "mixed_to_crlf; unexpand author, date, rev, url";
if (msg_only)
return SVN_NO_ERROR;
SVN_ERR(substitute_and_verify
("mixed_to_crlf_author_date_rev_url", NULL, "\r\n", 1,
"1729",
"Wed Jan 9 07:49:05 2002",
"jrandom",
"http://subversion.tigris.org",
0,
pool));
return SVN_NO_ERROR;
}
struct svn_test_descriptor_t test_funcs[] = {
SVN_TEST_NULL,
SVN_TEST_PASS(noop),
SVN_TEST_PASS(crlf_to_crlf),
SVN_TEST_PASS(lf_to_crlf),
SVN_TEST_PASS(cr_to_crlf),
SVN_TEST_PASS(mixed_to_crlf),
SVN_TEST_PASS(lf_to_lf),
SVN_TEST_PASS(crlf_to_lf),
SVN_TEST_PASS(cr_to_lf),
SVN_TEST_PASS(mixed_to_lf),
SVN_TEST_PASS(crlf_to_cr),
SVN_TEST_PASS(lf_to_cr),
SVN_TEST_PASS(cr_to_cr),
SVN_TEST_PASS(mixed_to_cr),
SVN_TEST_PASS(mixed_no_repair),
SVN_TEST_PASS(expand_author),
SVN_TEST_PASS(expand_date),
SVN_TEST_PASS(expand_author_date),
SVN_TEST_PASS(expand_author_rev),
SVN_TEST_PASS(expand_rev),
SVN_TEST_PASS(expand_rev_url),
SVN_TEST_PASS(expand_author_date_rev_url),
SVN_TEST_PASS(lf_to_crlf_expand_author),
SVN_TEST_PASS(mixed_to_lf_expand_author_date),
SVN_TEST_PASS(crlf_to_cr_expand_author_rev),
SVN_TEST_PASS(cr_to_crlf_expand_rev),
SVN_TEST_PASS(cr_to_crlf_expand_rev_url),
SVN_TEST_PASS(mixed_to_crlf_expand_author_date_rev_url),
SVN_TEST_PASS(unexpand_author),
SVN_TEST_PASS(unexpand_date),
SVN_TEST_PASS(unexpand_author_date),
SVN_TEST_PASS(unexpand_author_rev),
SVN_TEST_PASS(unexpand_rev),
SVN_TEST_PASS(unexpand_rev_url),
SVN_TEST_PASS(unexpand_author_date_rev_url),
SVN_TEST_PASS(lf_to_crlf_unexpand_author),
SVN_TEST_PASS(mixed_to_lf_unexpand_author_date),
SVN_TEST_PASS(crlf_to_cr_unexpand_author_rev),
SVN_TEST_PASS(cr_to_crlf_unexpand_rev),
SVN_TEST_PASS(cr_to_crlf_unexpand_rev_url),
SVN_TEST_PASS(mixed_to_crlf_unexpand_author_date_rev_url),
SVN_TEST_NULL
};
