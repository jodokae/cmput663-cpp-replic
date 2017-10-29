#include "apr.h"
#include "apr_strings.h"
#include "apr_lib.h"
#define APR_WANT_STRFUNC
#include "apr_want.h"
#include "ap_config.h"
#include "httpd.h"
#include "http_config.h"
#include "http_log.h"
#include "http_request.h"
#include "http_protocol.h"
typedef struct {
int active;
int wildcards;
char *expiresdefault;
apr_table_t *expiresbytype;
} expires_dir_config;
#define DIR_CMD_PERMS OR_INDEXES
#define ACTIVE_ON 1
#define ACTIVE_OFF 0
#define ACTIVE_DONTCARE 2
module AP_MODULE_DECLARE_DATA expires_module;
static void *create_dir_expires_config(apr_pool_t *p, char *dummy) {
expires_dir_config *new =
(expires_dir_config *) apr_pcalloc(p, sizeof(expires_dir_config));
new->active = ACTIVE_DONTCARE;
new->wildcards = 0;
new->expiresdefault = NULL;
new->expiresbytype = apr_table_make(p, 4);
return (void *) new;
}
static const char *set_expiresactive(cmd_parms *cmd, void *in_dir_config, int arg) {
expires_dir_config *dir_config = in_dir_config;
dir_config->active = ACTIVE_ON;
if (arg == 0) {
dir_config->active = ACTIVE_OFF;
}
return NULL;
}
static char *check_code(apr_pool_t *p, const char *code, char **real_code) {
char *word;
char base = 'X';
int modifier = 0;
int num = 0;
int factor;
if ((code[0] == 'A') || (code[0] == 'M')) {
*real_code = (char *)code;
return NULL;
}
word = ap_getword_conf(p, &code);
if (!strncasecmp(word, "now", 1) ||
!strncasecmp(word, "access", 1)) {
base = 'A';
} else if (!strncasecmp(word, "modification", 1)) {
base = 'M';
} else {
return apr_pstrcat(p, "bad expires code, unrecognised <base> '",
word, "'", NULL);
}
word = ap_getword_conf(p, &code);
if (!strncasecmp(word, "plus", 1)) {
word = ap_getword_conf(p, &code);
}
while (word[0]) {
if (apr_isdigit(word[0])) {
num = atoi(word);
} else {
return apr_pstrcat(p, "bad expires code, numeric value expected <num> '",
word, "'", NULL);
}
word = ap_getword_conf(p, &code);
if (word[0] == '\0') {
return apr_pstrcat(p, "bad expires code, missing <type>", NULL);
}
if (!strncasecmp(word, "years", 1)) {
factor = 60 * 60 * 24 * 365;
} else if (!strncasecmp(word, "months", 2)) {
factor = 60 * 60 * 24 * 30;
} else if (!strncasecmp(word, "weeks", 1)) {
factor = 60 * 60 * 24 * 7;
} else if (!strncasecmp(word, "days", 1)) {
factor = 60 * 60 * 24;
} else if (!strncasecmp(word, "hours", 1)) {
factor = 60 * 60;
} else if (!strncasecmp(word, "minutes", 2)) {
factor = 60;
} else if (!strncasecmp(word, "seconds", 1)) {
factor = 1;
} else {
return apr_pstrcat(p, "bad expires code, unrecognised <type>",
"'", word, "'", NULL);
}
modifier = modifier + factor * num;
word = ap_getword_conf(p, &code);
}
*real_code = apr_psprintf(p, "%c%d", base, modifier);
return NULL;
}
static const char *set_expiresbytype(cmd_parms *cmd, void *in_dir_config,
const char *mime, const char *code) {
expires_dir_config *dir_config = in_dir_config;
char *response, *real_code;
const char *check;
check = ap_strrchr_c(mime, '/');
if (check == NULL) {
return "Invalid mimetype: should contain a slash";
}
if ((strlen(++check) == 1) && (*check == '*')) {
dir_config->wildcards = 1;
}
if ((response = check_code(cmd->pool, code, &real_code)) == NULL) {
apr_table_setn(dir_config->expiresbytype, mime, real_code);
return NULL;
}
return apr_pstrcat(cmd->pool,
"'ExpiresByType ", mime, " ", code, "': ", response, NULL);
}
static const char *set_expiresdefault(cmd_parms *cmd, void *in_dir_config,
const char *code) {
expires_dir_config * dir_config = in_dir_config;
char *response, *real_code;
if ((response = check_code(cmd->pool, code, &real_code)) == NULL) {
dir_config->expiresdefault = real_code;
return NULL;
}
return apr_pstrcat(cmd->pool,
"'ExpiresDefault ", code, "': ", response, NULL);
}
static const command_rec expires_cmds[] = {
AP_INIT_FLAG("ExpiresActive", set_expiresactive, NULL, DIR_CMD_PERMS,
"Limited to 'on' or 'off'"),
AP_INIT_TAKE2("ExpiresByType", set_expiresbytype, NULL, DIR_CMD_PERMS,
"a MIME type followed by an expiry date code"),
AP_INIT_TAKE1("ExpiresDefault", set_expiresdefault, NULL, DIR_CMD_PERMS,
"an expiry date code"),
{NULL}
};
static void *merge_expires_dir_configs(apr_pool_t *p, void *basev, void *addv) {
expires_dir_config *new = (expires_dir_config *) apr_pcalloc(p, sizeof(expires_dir_config));
expires_dir_config *base = (expires_dir_config *) basev;
expires_dir_config *add = (expires_dir_config *) addv;
if (add->active == ACTIVE_DONTCARE) {
new->active = base->active;
} else {
new->active = add->active;
}
if (add->expiresdefault != NULL) {
new->expiresdefault = add->expiresdefault;
} else {
new->expiresdefault = base->expiresdefault;
}
new->wildcards = add->wildcards;
new->expiresbytype = apr_table_overlay(p, add->expiresbytype,
base->expiresbytype);
return new;
}
static int set_expiration_fields(request_rec *r, const char *code,
apr_table_t *t) {
apr_time_t base;
apr_time_t additional;
apr_time_t expires;
int additional_sec;
char *timestr;
switch (code[0]) {
case 'M':
if (r->finfo.filetype == APR_NOFILE) {
return DECLINED;
}
base = r->finfo.mtime;
additional_sec = atoi(&code[1]);
additional = apr_time_from_sec(additional_sec);
break;
case 'A':
base = r->request_time;
additional_sec = atoi(&code[1]);
additional = apr_time_from_sec(additional_sec);
break;
default:
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(01500)
"internal error: bad expires code: %s", r->filename);
return HTTP_INTERNAL_SERVER_ERROR;
}
expires = base + additional;
if (expires < r->request_time) {
expires = r->request_time;
}
apr_table_mergen(t, "Cache-Control",
apr_psprintf(r->pool, "max-age=%" APR_TIME_T_FMT,
apr_time_sec(expires - r->request_time)));
timestr = apr_palloc(r->pool, APR_RFC822_DATE_LEN);
apr_rfc822_date(timestr, expires);
apr_table_setn(t, "Expires", timestr);
return OK;
}
static apr_status_t expires_filter(ap_filter_t *f,
apr_bucket_brigade *b) {
request_rec *r;
expires_dir_config *conf;
const char *expiry;
apr_table_t *t;
if (ap_is_HTTP_ERROR(f->r->status)) {
ap_remove_output_filter(f);
return ap_pass_brigade(f->next, b);
}
r = f->r;
conf = (expires_dir_config *) ap_get_module_config(r->per_dir_config,
&expires_module);
expiry = apr_table_get(r->err_headers_out, "Expires");
if (expiry != NULL) {
t = r->err_headers_out;
} else {
expiry = apr_table_get(r->headers_out, "Expires");
t = r->headers_out;
}
if (expiry == NULL) {
expiry = apr_table_get(conf->expiresbytype,
ap_field_noparam(r->pool, r->content_type));
if (expiry == NULL) {
int usedefault = 1;
if (conf->wildcards) {
char *checkmime;
char *spos;
checkmime = apr_pstrdup(r->pool, r->content_type);
spos = checkmime ? ap_strchr(checkmime, '/') : NULL;
if (spos != NULL) {
if (strlen(++spos) > 0) {
*spos++ = '*';
*spos = '\0';
} else {
checkmime = apr_pstrcat(r->pool, checkmime, "*", NULL);
}
expiry = apr_table_get(conf->expiresbytype, checkmime);
usedefault = (expiry == NULL);
}
}
if (usedefault) {
expiry = conf->expiresdefault;
}
}
if (expiry != NULL) {
set_expiration_fields(r, expiry, t);
}
}
ap_remove_output_filter(f);
return ap_pass_brigade(f->next, b);
}
static void expires_insert_filter(request_rec *r) {
expires_dir_config *conf;
if (ap_is_HTTP_ERROR(r->status)) {
return;
}
if (r->main != NULL) {
return;
}
conf = (expires_dir_config *) ap_get_module_config(r->per_dir_config,
&expires_module);
if (conf->active != ACTIVE_ON ||
(apr_is_empty_table(conf->expiresbytype) && !conf->expiresdefault)) {
return;
}
ap_add_output_filter("MOD_EXPIRES", NULL, r, r->connection);
}
static void register_hooks(apr_pool_t *p) {
ap_register_output_filter("MOD_EXPIRES", expires_filter, NULL,
AP_FTYPE_CONTENT_SET-2);
ap_hook_insert_error_filter(expires_insert_filter, NULL, NULL, APR_HOOK_MIDDLE);
ap_hook_insert_filter(expires_insert_filter, NULL, NULL, APR_HOOK_MIDDLE);
}
AP_DECLARE_MODULE(expires) = {
STANDARD20_MODULE_STUFF,
create_dir_expires_config,
merge_expires_dir_configs,
NULL,
NULL,
expires_cmds,
register_hooks
};
