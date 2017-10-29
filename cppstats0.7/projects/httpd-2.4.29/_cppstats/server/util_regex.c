#include "apr.h"
#include "apr_lib.h"
#include "apr_pools.h"
#include "apr_strings.h"
#include "ap_config.h"
#include "ap_regex.h"
#include "httpd.h"
static apr_status_t rxplus_cleanup(void *preg) {
ap_regfree((ap_regex_t *) preg);
return APR_SUCCESS;
}
AP_DECLARE(ap_rxplus_t*) ap_rxplus_compile(apr_pool_t *pool,
const char *pattern) {
const char *endp = 0;
const char *str = pattern;
const char *rxstr;
ap_rxplus_t *ret = apr_pcalloc(pool, sizeof(ap_rxplus_t));
char delim = 0;
enum { SUBSTITUTE = 's', MATCH = 'm'} action = MATCH;
if (!apr_isalnum(pattern[0])) {
delim = *str++;
} else if (pattern[0] == 's' && !apr_isalnum(pattern[1])) {
action = SUBSTITUTE;
delim = pattern[1];
str += 2;
} else if (pattern[0] == 'm' && !apr_isalnum(pattern[1])) {
delim = pattern[1];
str += 2;
}
if (delim) {
endp = ap_strchr_c(str, delim);
}
if (!endp) {
if (ap_regcomp(&ret->rx, pattern, 0) == 0) {
apr_pool_cleanup_register(pool, &ret->rx, rxplus_cleanup,
apr_pool_cleanup_null);
return ret;
} else {
return NULL;
}
}
rxstr = apr_pstrmemdup(pool, str, endp-str);
if (action == SUBSTITUTE) {
str = endp+1;
if (!*str || (endp = ap_strchr_c(str, delim), !endp)) {
return NULL;
}
ret->subs = apr_pstrmemdup(pool, str, endp-str);
}
while (*++endp) {
switch (*endp) {
case 'i':
ret->flags |= AP_REG_ICASE;
break;
case 'm':
ret->flags |= AP_REG_NEWLINE;
break;
case 'n':
ret->flags |= AP_REG_NOMEM;
break;
case 'g':
ret->flags |= AP_REG_MULTI;
break;
case 's':
ret->flags |= AP_REG_DOTALL;
break;
case '^':
ret->flags |= AP_REG_NOTBOL;
break;
case '$':
ret->flags |= AP_REG_NOTEOL;
break;
default:
break;
}
}
if (ap_regcomp(&ret->rx, rxstr, ret->flags) == 0) {
apr_pool_cleanup_register(pool, &ret->rx, rxplus_cleanup,
apr_pool_cleanup_null);
} else {
return NULL;
}
if (!(ret->flags & AP_REG_NOMEM)) {
ret->nmatch = 1;
while (*rxstr) {
switch (*rxstr++) {
case '\\':
if (*rxstr != 0) {
++rxstr;
}
break;
case '(':
++ret->nmatch;
break;
default:
break;
}
}
ret->pmatch = apr_palloc(pool, ret->nmatch*sizeof(ap_regmatch_t));
}
return ret;
}
AP_DECLARE(int) ap_rxplus_exec(apr_pool_t *pool, ap_rxplus_t *rx,
const char *pattern, char **newpattern) {
int ret = 1;
int startl, oldl, newl, diffsz;
const char *remainder;
char *subs;
if (ap_regexec(&rx->rx, pattern, rx->nmatch, rx->pmatch, rx->flags) != 0) {
rx->match = NULL;
return 0;
}
rx->match = pattern;
if (rx->subs) {
*newpattern = ap_pregsub(pool, rx->subs, pattern,
rx->nmatch, rx->pmatch);
if (!*newpattern) {
return 0;
}
startl = rx->pmatch[0].rm_so;
oldl = rx->pmatch[0].rm_eo - startl;
newl = strlen(*newpattern);
diffsz = newl - oldl;
remainder = pattern + startl + oldl;
if (rx->flags & AP_REG_MULTI) {
ret += ap_rxplus_exec(pool, rx, remainder, &subs);
if (ret > 1) {
diffsz += strlen(subs) - strlen(remainder);
remainder = subs;
}
}
subs = apr_palloc(pool, strlen(pattern) + 1 + diffsz);
memcpy(subs, pattern, startl);
memcpy(subs+startl, *newpattern, newl);
strcpy(subs+startl+newl, remainder);
*newpattern = subs;
}
return ret;
}
#if defined(DOXYGEN)
AP_DECLARE(int) ap_rxplus_nmatch(ap_rxplus_t *rx) {
return (rx->match != NULL) ? rx->nmatch : 0;
}
#endif
AP_DECLARE(void) ap_rxplus_match(ap_rxplus_t *rx, int n, int *len,
const char **match) {
if (n >= 0 && n < ap_rxplus_nmatch(rx)) {
*match = rx->match + rx->pmatch[n].rm_so;
*len = rx->pmatch[n].rm_eo - rx->pmatch[n].rm_so;
} else {
*len = -1;
*match = NULL;
}
}
AP_DECLARE(char*) ap_rxplus_pmatch(apr_pool_t *pool, ap_rxplus_t *rx, int n) {
int len;
const char *match;
ap_rxplus_match(rx, n, &len, &match);
return apr_pstrndup(pool, match, len);
}
