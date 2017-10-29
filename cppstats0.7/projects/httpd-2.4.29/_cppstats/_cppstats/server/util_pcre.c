#include "httpd.h"
#include "apr_strings.h"
#include "apr_tables.h"
#include "pcre.h"
#define APR_WANT_STRFUNC
#include "apr_want.h"
#if !defined(POSIX_MALLOC_THRESHOLD)
#define POSIX_MALLOC_THRESHOLD (10)
#endif
static const char *const pstring[] = {
"",
"internal error",
"failed to get memory",
"bad argument",
"match failed"
};
AP_DECLARE(apr_size_t) ap_regerror(int errcode, const ap_regex_t *preg,
char *errbuf, apr_size_t errbuf_size) {
const char *message, *addmessage;
apr_size_t length, addlength;
message = (errcode >= (int)(sizeof(pstring) / sizeof(char *))) ?
"unknown error code" : pstring[errcode];
length = strlen(message) + 1;
addmessage = " at offset ";
addlength = (preg != NULL && (int)preg->re_erroffset != -1) ?
strlen(addmessage) + 6 : 0;
if (errbuf_size > 0) {
if (addlength > 0 && errbuf_size >= length + addlength)
apr_snprintf(errbuf, errbuf_size, "%s%s%-6d", message, addmessage,
(int)preg->re_erroffset);
else
apr_cpystrn(errbuf, message, errbuf_size);
}
return length + addlength;
}
AP_DECLARE(void) ap_regfree(ap_regex_t *preg) {
(pcre_free)(preg->re_pcre);
}
AP_DECLARE(int) ap_regcomp(ap_regex_t * preg, const char *pattern, int cflags) {
const char *errorptr;
int erroffset;
int errcode = 0;
int options = PCRE_DUPNAMES;
if ((cflags & AP_REG_ICASE) != 0)
options |= PCRE_CASELESS;
if ((cflags & AP_REG_NEWLINE) != 0)
options |= PCRE_MULTILINE;
if ((cflags & AP_REG_DOTALL) != 0)
options |= PCRE_DOTALL;
preg->re_pcre =
pcre_compile2(pattern, options, &errcode, &errorptr, &erroffset, NULL);
preg->re_erroffset = erroffset;
if (preg->re_pcre == NULL) {
if (errcode == 21)
return AP_REG_ESPACE;
return AP_REG_INVARG;
}
pcre_fullinfo((const pcre *)preg->re_pcre, NULL,
PCRE_INFO_CAPTURECOUNT, &(preg->re_nsub));
return 0;
}
AP_DECLARE(int) ap_regexec(const ap_regex_t *preg, const char *string,
apr_size_t nmatch, ap_regmatch_t *pmatch,
int eflags) {
return ap_regexec_len(preg, string, strlen(string), nmatch, pmatch,
eflags);
}
AP_DECLARE(int) ap_regexec_len(const ap_regex_t *preg, const char *buff,
apr_size_t len, apr_size_t nmatch,
ap_regmatch_t *pmatch, int eflags) {
int rc;
int options = 0;
int *ovector = NULL;
int small_ovector[POSIX_MALLOC_THRESHOLD * 3];
int allocated_ovector = 0;
if ((eflags & AP_REG_NOTBOL) != 0)
options |= PCRE_NOTBOL;
if ((eflags & AP_REG_NOTEOL) != 0)
options |= PCRE_NOTEOL;
((ap_regex_t *)preg)->re_erroffset = (apr_size_t)(-1);
if (nmatch > 0) {
if (nmatch <= POSIX_MALLOC_THRESHOLD) {
ovector = &(small_ovector[0]);
} else {
ovector = (int *)malloc(sizeof(int) * nmatch * 3);
if (ovector == NULL)
return AP_REG_ESPACE;
allocated_ovector = 1;
}
}
rc = pcre_exec((const pcre *)preg->re_pcre, NULL, buff, (int)len,
0, options, ovector, nmatch * 3);
if (rc == 0)
rc = nmatch;
if (rc >= 0) {
apr_size_t i;
for (i = 0; i < (apr_size_t)rc; i++) {
pmatch[i].rm_so = ovector[i * 2];
pmatch[i].rm_eo = ovector[i * 2 + 1];
}
if (allocated_ovector)
free(ovector);
for (; i < nmatch; i++)
pmatch[i].rm_so = pmatch[i].rm_eo = -1;
return 0;
} else {
if (allocated_ovector)
free(ovector);
switch (rc) {
case PCRE_ERROR_NOMATCH:
return AP_REG_NOMATCH;
case PCRE_ERROR_NULL:
return AP_REG_INVARG;
case PCRE_ERROR_BADOPTION:
return AP_REG_INVARG;
case PCRE_ERROR_BADMAGIC:
return AP_REG_INVARG;
case PCRE_ERROR_UNKNOWN_NODE:
return AP_REG_ASSERT;
case PCRE_ERROR_NOMEMORY:
return AP_REG_ESPACE;
#if defined(PCRE_ERROR_MATCHLIMIT)
case PCRE_ERROR_MATCHLIMIT:
return AP_REG_ESPACE;
#endif
#if defined(PCRE_ERROR_BADUTF8)
case PCRE_ERROR_BADUTF8:
return AP_REG_INVARG;
#endif
#if defined(PCRE_ERROR_BADUTF8_OFFSET)
case PCRE_ERROR_BADUTF8_OFFSET:
return AP_REG_INVARG;
#endif
default:
return AP_REG_ASSERT;
}
}
}
AP_DECLARE(int) ap_regname(const ap_regex_t *preg,
apr_array_header_t *names, const char *prefix,
int upper) {
int namecount;
int nameentrysize;
int i;
char *nametable;
pcre_fullinfo((const pcre *)preg->re_pcre, NULL,
PCRE_INFO_NAMECOUNT, &namecount);
pcre_fullinfo((const pcre *)preg->re_pcre, NULL,
PCRE_INFO_NAMEENTRYSIZE, &nameentrysize);
pcre_fullinfo((const pcre *)preg->re_pcre, NULL,
PCRE_INFO_NAMETABLE, &nametable);
for (i = 0; i < namecount; i++) {
const char *offset = nametable + i * nameentrysize;
int capture = ((offset[0] << 8) + offset[1]);
while (names->nelts <= capture) {
apr_array_push(names);
}
if (upper || prefix) {
char *name = ((char **) names->elts)[capture] =
prefix ? apr_pstrcat(names->pool, prefix, offset + 2,
NULL) :
apr_pstrdup(names->pool, offset + 2);
if (upper) {
ap_str_toupper(name);
}
} else {
((const char **)names->elts)[capture] = offset + 2;
}
}
return namecount;
}
