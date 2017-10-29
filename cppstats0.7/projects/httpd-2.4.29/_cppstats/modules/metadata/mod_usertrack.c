#include "apr.h"
#include "apr_lib.h"
#include "apr_strings.h"
#define APR_WANT_STRFUNC
#include "apr_want.h"
#include "httpd.h"
#include "http_config.h"
#include "http_core.h"
#include "http_request.h"
#include "http_log.h"
module AP_MODULE_DECLARE_DATA usertrack_module;
typedef struct {
int always;
int expires;
} cookie_log_state;
typedef enum {
CT_UNSET,
CT_NETSCAPE,
CT_COOKIE,
CT_COOKIE2
} cookie_type_e;
typedef struct {
int enabled;
cookie_type_e style;
const char *cookie_name;
const char *cookie_domain;
char *regexp_string;
ap_regex_t *regexp;
} cookie_dir_rec;
#define COOKIE_NAME "Apache"
static void make_cookie(request_rec *r) {
cookie_log_state *cls = ap_get_module_config(r->server->module_config,
&usertrack_module);
char cookiebuf[2 * (sizeof(apr_uint64_t) + sizeof(int)) + 2];
unsigned int random;
apr_time_t now = r->request_time ? r->request_time : apr_time_now();
char *new_cookie;
cookie_dir_rec *dcfg;
ap_random_insecure_bytes(&random, sizeof(random));
apr_snprintf(cookiebuf, sizeof(cookiebuf), "%x.%" APR_UINT64_T_HEX_FMT,
random, (apr_uint64_t)now);
dcfg = ap_get_module_config(r->per_dir_config, &usertrack_module);
if (cls->expires) {
new_cookie = apr_psprintf(r->pool, "%s=%s; path=/",
dcfg->cookie_name, cookiebuf);
if ((dcfg->style == CT_UNSET) || (dcfg->style == CT_NETSCAPE)) {
apr_time_exp_t tms;
apr_time_exp_gmt(&tms, r->request_time
+ apr_time_from_sec(cls->expires));
new_cookie = apr_psprintf(r->pool,
"%s; expires=%s, "
"%.2d-%s-%.2d %.2d:%.2d:%.2d GMT",
new_cookie, apr_day_snames[tms.tm_wday],
tms.tm_mday,
apr_month_snames[tms.tm_mon],
tms.tm_year % 100,
tms.tm_hour, tms.tm_min, tms.tm_sec);
} else {
new_cookie = apr_psprintf(r->pool, "%s; max-age=%d",
new_cookie, cls->expires);
}
} else {
new_cookie = apr_psprintf(r->pool, "%s=%s; path=/",
dcfg->cookie_name, cookiebuf);
}
if (dcfg->cookie_domain != NULL) {
new_cookie = apr_pstrcat(r->pool, new_cookie, "; domain=",
dcfg->cookie_domain,
(dcfg->style == CT_COOKIE2
? "; version=1"
: ""),
NULL);
}
apr_table_addn(r->err_headers_out,
(dcfg->style == CT_COOKIE2 ? "Set-Cookie2" : "Set-Cookie"),
new_cookie);
apr_table_setn(r->notes, "cookie", apr_pstrdup(r->pool, cookiebuf));
}
#define NUM_SUBS 3
static void set_and_comp_regexp(cookie_dir_rec *dcfg,
apr_pool_t *p,
const char *cookie_name) {
int danger_chars = 0;
const char *sp = cookie_name;
while (*sp) {
if (!apr_isalnum(*sp)) {
++danger_chars;
}
++sp;
}
if (danger_chars) {
char *cp;
cp = apr_palloc(p, sp - cookie_name + danger_chars + 1);
sp = cookie_name;
cookie_name = cp;
while (*sp) {
if (!apr_isalnum(*sp)) {
*cp++ = '\\';
}
*cp++ = *sp++;
}
*cp = '\0';
}
dcfg->regexp_string = apr_pstrcat(p, "^",
cookie_name,
"=([^;,]+)|[;,][ \t]*",
cookie_name,
"=([^;,]+)", NULL);
dcfg->regexp = ap_pregcomp(p, dcfg->regexp_string, AP_REG_EXTENDED);
ap_assert(dcfg->regexp != NULL);
}
static int spot_cookie(request_rec *r) {
cookie_dir_rec *dcfg = ap_get_module_config(r->per_dir_config,
&usertrack_module);
const char *cookie_header;
ap_regmatch_t regm[NUM_SUBS];
if (!dcfg->enabled || r->main) {
return DECLINED;
}
if ((cookie_header = apr_table_get(r->headers_in, "Cookie"))) {
if (!ap_regexec(dcfg->regexp, cookie_header, NUM_SUBS, regm, 0)) {
char *cookieval = NULL;
int err = 0;
if (regm[1].rm_so != -1) {
cookieval = ap_pregsub(r->pool, "$1", cookie_header,
NUM_SUBS, regm);
if (cookieval == NULL)
err = 1;
}
if (regm[2].rm_so != -1) {
cookieval = ap_pregsub(r->pool, "$2", cookie_header,
NUM_SUBS, regm);
if (cookieval == NULL)
err = 1;
}
if (err) {
ap_log_rerror(APLOG_MARK, APLOG_CRIT, 0, r, APLOGNO(01499)
"Failed to extract cookie value (out of mem?)");
return HTTP_INTERNAL_SERVER_ERROR;
}
apr_table_setn(r->notes, "cookie", cookieval);
return DECLINED;
}
}
make_cookie(r);
return OK;
}
static void *make_cookie_log_state(apr_pool_t *p, server_rec *s) {
cookie_log_state *cls =
(cookie_log_state *) apr_palloc(p, sizeof(cookie_log_state));
cls->expires = 0;
return (void *) cls;
}
static void *make_cookie_dir(apr_pool_t *p, char *d) {
cookie_dir_rec *dcfg;
dcfg = (cookie_dir_rec *) apr_pcalloc(p, sizeof(cookie_dir_rec));
dcfg->cookie_name = COOKIE_NAME;
dcfg->cookie_domain = NULL;
dcfg->style = CT_UNSET;
dcfg->enabled = 0;
set_and_comp_regexp(dcfg, p, COOKIE_NAME);
return dcfg;
}
static const char *set_cookie_enable(cmd_parms *cmd, void *mconfig, int arg) {
cookie_dir_rec *dcfg = mconfig;
dcfg->enabled = arg;
return NULL;
}
static const char *set_cookie_exp(cmd_parms *parms, void *dummy,
const char *arg) {
cookie_log_state *cls;
time_t factor, modifier = 0;
time_t num = 0;
char *word;
cls = ap_get_module_config(parms->server->module_config,
&usertrack_module);
if (apr_isdigit(arg[0]) && apr_isdigit(arg[strlen(arg) - 1])) {
cls->expires = atol(arg);
return NULL;
}
word = ap_getword_conf(parms->temp_pool, &arg);
if (!strncasecmp(word, "plus", 1)) {
word = ap_getword_conf(parms->temp_pool, &arg);
};
while (word[0]) {
if (apr_isdigit(word[0]))
num = atoi(word);
else
return "bad expires code, numeric value expected.";
word = ap_getword_conf(parms->temp_pool, &arg);
if (!word[0])
return "bad expires code, missing <type>";
if (!strncasecmp(word, "years", 1))
factor = 60 * 60 * 24 * 365;
else if (!strncasecmp(word, "months", 2))
factor = 60 * 60 * 24 * 30;
else if (!strncasecmp(word, "weeks", 1))
factor = 60 * 60 * 24 * 7;
else if (!strncasecmp(word, "days", 1))
factor = 60 * 60 * 24;
else if (!strncasecmp(word, "hours", 1))
factor = 60 * 60;
else if (!strncasecmp(word, "minutes", 2))
factor = 60;
else if (!strncasecmp(word, "seconds", 1))
factor = 1;
else
return "bad expires code, unrecognized type";
modifier = modifier + factor * num;
word = ap_getword_conf(parms->temp_pool, &arg);
}
cls->expires = modifier;
return NULL;
}
static const char *set_cookie_name(cmd_parms *cmd, void *mconfig,
const char *name) {
cookie_dir_rec *dcfg = (cookie_dir_rec *) mconfig;
dcfg->cookie_name = name;
set_and_comp_regexp(dcfg, cmd->pool, name);
if (dcfg->regexp == NULL) {
return "Regular expression could not be compiled.";
}
if (dcfg->regexp->re_nsub + 1 != NUM_SUBS) {
return apr_pstrcat(cmd->pool, "Invalid cookie name \"",
name, "\"", NULL);
}
return NULL;
}
static const char *set_cookie_domain(cmd_parms *cmd, void *mconfig,
const char *name) {
cookie_dir_rec *dcfg;
dcfg = (cookie_dir_rec *) mconfig;
if (!name[0]) {
return "CookieDomain values may not be null";
}
if (name[0] != '.') {
return "CookieDomain values must begin with a dot";
}
if (ap_strchr_c(&name[1], '.') == NULL) {
return "CookieDomain values must contain at least one embedded dot";
}
dcfg->cookie_domain = name;
return NULL;
}
static const char *set_cookie_style(cmd_parms *cmd, void *mconfig,
const char *name) {
cookie_dir_rec *dcfg;
dcfg = (cookie_dir_rec *) mconfig;
if (strcasecmp(name, "Netscape") == 0) {
dcfg->style = CT_NETSCAPE;
} else if ((strcasecmp(name, "Cookie") == 0)
|| (strcasecmp(name, "RFC2109") == 0)) {
dcfg->style = CT_COOKIE;
} else if ((strcasecmp(name, "Cookie2") == 0)
|| (strcasecmp(name, "RFC2965") == 0)) {
dcfg->style = CT_COOKIE2;
} else {
return apr_psprintf(cmd->pool, "Invalid %s keyword: '%s'",
cmd->cmd->name, name);
}
return NULL;
}
static const command_rec cookie_log_cmds[] = {
AP_INIT_TAKE1("CookieExpires", set_cookie_exp, NULL, OR_FILEINFO,
"an expiry date code"),
AP_INIT_TAKE1("CookieDomain", set_cookie_domain, NULL, OR_FILEINFO,
"domain to which this cookie applies"),
AP_INIT_TAKE1("CookieStyle", set_cookie_style, NULL, OR_FILEINFO,
"'Netscape', 'Cookie' (RFC2109), or 'Cookie2' (RFC2965)"),
AP_INIT_FLAG("CookieTracking", set_cookie_enable, NULL, OR_FILEINFO,
"whether or not to enable cookies"),
AP_INIT_TAKE1("CookieName", set_cookie_name, NULL, OR_FILEINFO,
"name of the tracking cookie"),
{NULL}
};
static void register_hooks(apr_pool_t *p) {
ap_hook_fixups(spot_cookie,NULL,NULL,APR_HOOK_REALLY_FIRST);
}
AP_DECLARE_MODULE(usertrack) = {
STANDARD20_MODULE_STUFF,
make_cookie_dir,
NULL,
make_cookie_log_state,
NULL,
cookie_log_cmds,
register_hooks
};
