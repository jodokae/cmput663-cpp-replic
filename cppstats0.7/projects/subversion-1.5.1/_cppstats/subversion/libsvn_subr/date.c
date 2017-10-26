#include "svn_time.h"
#include "svn_error.h"
#include "svn_private_config.h"
enum rule_action {
ACCUM,
MICRO,
TZIND,
NOOP,
SKIPFROM,
SKIP,
ACCEPT
};
typedef struct {
char key;
const char *valid;
enum rule_action action;
int offset;
} rule;
typedef struct {
apr_time_exp_t base;
apr_int32_t offhours;
apr_int32_t offminutes;
} match_state;
#define DIGITS "0123456789"
static const rule
rules[] = {
{ 'Y', DIGITS, ACCUM, APR_OFFSETOF(match_state, base.tm_year) },
{ 'M', DIGITS, ACCUM, APR_OFFSETOF(match_state, base.tm_mon) },
{ 'D', DIGITS, ACCUM, APR_OFFSETOF(match_state, base.tm_mday) },
{ 'h', DIGITS, ACCUM, APR_OFFSETOF(match_state, base.tm_hour) },
{ 'm', DIGITS, ACCUM, APR_OFFSETOF(match_state, base.tm_min) },
{ 's', DIGITS, ACCUM, APR_OFFSETOF(match_state, base.tm_sec) },
{ 'u', DIGITS, MICRO, APR_OFFSETOF(match_state, base.tm_usec) },
{ 'O', DIGITS, ACCUM, APR_OFFSETOF(match_state, offhours) },
{ 'o', DIGITS, ACCUM, APR_OFFSETOF(match_state, offminutes) },
{ '+', "-+", TZIND, 0 },
{ 'Z', "Z", TZIND, 0 },
{ ':', ":", NOOP, 0 },
{ '-', "-", NOOP, 0 },
{ 'T', "T", NOOP, 0 },
{ ' ', " ", NOOP, 0 },
{ '.', ".,", NOOP, 0 },
{ '[', NULL, SKIPFROM, 0 },
{ ']', NULL, SKIP, 0 },
{ '\0', NULL, ACCEPT, 0 },
};
static const rule *
find_rule(char tchar) {
int i = sizeof(rules)/sizeof(rules[0]);
while (i--)
if (rules[i].key == tchar)
return &rules[i];
return NULL;
}
static svn_boolean_t
template_match(apr_time_exp_t *expt, svn_boolean_t *localtz,
const char *template, const char *value) {
int multiplier = 100000;
int tzind = 0;
match_state ms;
char *base = (char *)&ms;
memset(&ms, 0, sizeof(ms));
for (;;) {
const rule *match = find_rule(*template++);
char vchar = *value++;
apr_int32_t *place;
if (!match || (match->valid
&& (!vchar || !strchr(match->valid, vchar))))
return FALSE;
place = (apr_int32_t *)(base + match->offset);
switch (match->action) {
case ACCUM:
*place = *place * 10 + vchar - '0';
continue;
case MICRO:
*place += (vchar - '0') * multiplier;
multiplier /= 10;
continue;
case TZIND:
tzind = vchar;
continue;
case SKIP:
value--;
continue;
case NOOP:
continue;
case SKIPFROM:
if (!vchar)
break;
match = find_rule(*template);
if (!strchr(match->valid, vchar))
template = strchr(template, ']') + 1;
value--;
continue;
case ACCEPT:
if (vchar)
return FALSE;
break;
}
break;
}
if (ms.offhours > 23 || ms.offminutes > 59)
return FALSE;
switch (tzind) {
case '+':
ms.base.tm_gmtoff = ms.offhours * 3600 + ms.offminutes * 60;
break;
case '-':
ms.base.tm_gmtoff = -(ms.offhours * 3600 + ms.offminutes * 60);
break;
}
*expt = ms.base;
*localtz = (tzind == 0);
return TRUE;
}
static int
valid_days_by_month[] = {
31, 29, 31, 30,
31, 30, 31, 31,
30, 31, 30, 31
};
svn_error_t *
svn_parse_date(svn_boolean_t *matched, apr_time_t *result, const char *text,
apr_time_t now, apr_pool_t *pool) {
apr_time_exp_t expt, expnow;
apr_status_t apr_err;
svn_boolean_t localtz;
*matched = FALSE;
apr_err = apr_time_exp_lt(&expnow, now);
if (apr_err != APR_SUCCESS)
return svn_error_wrap_apr(apr_err, _("Can't manipulate current date"));
if (template_match(&expt, &localtz,
"YYYY-M[M]-D[D]",
text)
|| template_match(&expt, &localtz,
"YYYY-M[M]-D[D]Th[h]:mm[:ss[.u[u[u[u[u[u][Z]",
text)
|| template_match(&expt, &localtz,
"YYYY-M[M]-D[D]Th[h]:mm[:ss[.u[u[u[u[u[u]+OO[:oo]",
text)
|| template_match(&expt, &localtz,
"YYYYMMDD",
text)
|| template_match(&expt, &localtz,
"YYYYMMDDThhmm[ss[.u[u[u[u[u[u][Z]",
text)
|| template_match(&expt, &localtz,
"YYYYMMDDThhmm[ss[.u[u[u[u[u[u]+OO[oo]",
text)
|| template_match(&expt, &localtz,
"YYYY-M[M]-D[D] h[h]:mm[:ss[.u[u[u[u[u[u][ +OO[oo]",
text)
|| template_match(&expt, &localtz,
"YYYY-M[M]-D[D]Th[h]:mm[:ss[.u[u[u[u[u[u]+OO[oo]",
text)) {
expt.tm_year -= 1900;
expt.tm_mon -= 1;
} else if (template_match(&expt, &localtz,
"h[h]:mm[:ss[.u[u[u[u[u[u]",
text)) {
expt.tm_year = expnow.tm_year;
expt.tm_mon = expnow.tm_mon;
expt.tm_mday = expnow.tm_mday;
} else
return SVN_NO_ERROR;
if (expt.tm_mon < 0 || expt.tm_mon > 11
|| expt.tm_mday > valid_days_by_month[expt.tm_mon]
|| expt.tm_mday < 1
|| expt.tm_hour > 23
|| expt.tm_min > 59
|| expt.tm_sec > 60)
return SVN_NO_ERROR;
if (expt.tm_mon == 1
&& expt.tm_mday == 29
&& (expt.tm_year % 4 != 0
|| (expt.tm_year % 100 == 0 && expt.tm_year % 400 != 100)))
return SVN_NO_ERROR;
if (localtz) {
apr_time_t candidate;
apr_time_exp_t expthen;
expt.tm_gmtoff = expnow.tm_gmtoff;
apr_err = apr_time_exp_gmt_get(&candidate, &expt);
if (apr_err != APR_SUCCESS)
return svn_error_wrap_apr(apr_err,
_("Can't calculate requested date"));
apr_err = apr_time_exp_lt(&expthen, candidate);
if (apr_err != APR_SUCCESS)
return svn_error_wrap_apr(apr_err, _("Can't expand time"));
expt.tm_gmtoff = expthen.tm_gmtoff;
}
apr_err = apr_time_exp_gmt_get(result, &expt);
if (apr_err != APR_SUCCESS)
return svn_error_wrap_apr(apr_err, _("Can't calculate requested date"));
*matched = TRUE;
return SVN_NO_ERROR;
}
