#include "apr.h"
#include "apr_lib.h"
#include "apr_strings.h"
#include "apr_buckets.h"
#include "apr_hash.h"
#define APR_WANT_STRFUNC
#include "apr_want.h"
#include "httpd.h"
#include "http_config.h"
#include "http_request.h"
#include "http_log.h"
#include "util_filter.h"
#include "http_protocol.h"
#include "ap_expr.h"
#include "mod_ssl.h"
static apr_hash_t *format_tag_hash;
typedef enum {
hdr_add = 'a',
hdr_set = 's',
hdr_append = 'm',
hdr_merge = 'g',
hdr_unset = 'u',
hdr_echo = 'e',
hdr_edit = 'r',
hdr_edit_r = 'R',
hdr_setifempty = 'i',
hdr_note = 'n'
} hdr_actions;
static char hdr_in = '0';
static char hdr_out_onsuccess = '1';
static char hdr_out_always = '2';
typedef const char *format_tag_fn(request_rec *r, char *a);
typedef struct {
format_tag_fn *func;
char *arg;
} format_tag;
static const char* condition_early = "early";
typedef struct {
hdr_actions action;
const char *header;
apr_array_header_t *ta;
ap_regex_t *regex;
const char *condition_var;
const char *subs;
ap_expr_info_t *expr;
ap_expr_info_t *expr_out;
} header_entry;
typedef struct {
request_rec *r;
header_entry *hdr;
} echo_do;
typedef struct {
request_rec *r;
header_entry *hdr;
apr_table_t *t;
} edit_do;
typedef struct {
apr_array_header_t *fixup_in;
apr_array_header_t *fixup_out;
apr_array_header_t *fixup_err;
} headers_conf;
module AP_MODULE_DECLARE_DATA headers_module;
static APR_OPTIONAL_FN_TYPE(ssl_var_lookup) *header_ssl_lookup = NULL;
static const char *constant_item(request_rec *r, char *stuff) {
return stuff;
}
static const char *header_request_duration(request_rec *r, char *a) {
return apr_psprintf(r->pool, "D=%" APR_TIME_T_FMT,
(apr_time_now() - r->request_time));
}
static const char *header_request_time(request_rec *r, char *a) {
return apr_psprintf(r->pool, "t=%" APR_TIME_T_FMT, r->request_time);
}
static const char *unwrap_header(apr_pool_t *p, const char *hdr) {
if (ap_strchr_c(hdr, APR_ASCII_LF) || ap_strchr_c(hdr, APR_ASCII_CR)) {
char *ptr;
hdr = ptr = apr_pstrdup(p, hdr);
do {
if (*ptr == APR_ASCII_LF || *ptr == APR_ASCII_CR)
*ptr = APR_ASCII_BLANK;
} while (*ptr++);
}
return hdr;
}
static const char *header_request_env_var(request_rec *r, char *a) {
const char *s = apr_table_get(r->subprocess_env,a);
if (s)
return unwrap_header(r->pool, s);
else
return "(null)";
}
static const char *header_request_ssl_var(request_rec *r, char *name) {
if (header_ssl_lookup) {
const char *val = header_ssl_lookup(r->pool, r->server,
r->connection, r, name);
if (val && val[0])
return unwrap_header(r->pool, val);
else
return "(null)";
} else {
return "(null)";
}
}
static const char *header_request_loadavg(request_rec *r, char *a) {
ap_loadavg_t t;
ap_get_loadavg(&t);
return apr_psprintf(r->pool, "l=%.2f/%.2f/%.2f", t.loadavg,
t.loadavg5, t.loadavg15);
}
static const char *header_request_idle(request_rec *r, char *a) {
ap_sload_t t;
ap_get_sload(&t);
return apr_psprintf(r->pool, "i=%d", t.idle);
}
static const char *header_request_busy(request_rec *r, char *a) {
ap_sload_t t;
ap_get_sload(&t);
return apr_psprintf(r->pool, "b=%d", t.busy);
}
static void *create_headers_dir_config(apr_pool_t *p, char *d) {
headers_conf *conf = apr_pcalloc(p, sizeof(*conf));
conf->fixup_in = apr_array_make(p, 2, sizeof(header_entry));
conf->fixup_out = apr_array_make(p, 2, sizeof(header_entry));
conf->fixup_err = apr_array_make(p, 2, sizeof(header_entry));
return conf;
}
static void *merge_headers_config(apr_pool_t *p, void *basev, void *overridesv) {
headers_conf *newconf = apr_pcalloc(p, sizeof(*newconf));
headers_conf *base = basev;
headers_conf *overrides = overridesv;
newconf->fixup_in = apr_array_append(p, base->fixup_in,
overrides->fixup_in);
newconf->fixup_out = apr_array_append(p, base->fixup_out,
overrides->fixup_out);
newconf->fixup_err = apr_array_append(p, base->fixup_err,
overrides->fixup_err);
return newconf;
}
static char *parse_misc_string(apr_pool_t *p, format_tag *tag, const char **sa) {
const char *s;
char *d;
tag->func = constant_item;
s = *sa;
while (*s && *s != '%') {
s++;
}
tag->arg = apr_palloc(p, s - *sa + 1);
d = tag->arg;
s = *sa;
while (*s && *s != '%') {
if (*s != '\\') {
*d++ = *s++;
} else {
s++;
switch (*s) {
case '\\':
*d++ = '\\';
s++;
break;
case 'r':
*d++ = '\r';
s++;
break;
case 'n':
*d++ = '\n';
s++;
break;
case 't':
*d++ = '\t';
s++;
break;
default:
*d++ = '\\';
break;
}
}
}
*d = '\0';
*sa = s;
return NULL;
}
static char *parse_format_tag(apr_pool_t *p, format_tag *tag, const char **sa) {
const char *s = *sa;
const char * (*tag_handler)(request_rec *,char *);
if (*s != '%') {
return parse_misc_string(p, tag, sa);
}
s++;
if ((*s == '%') || (*s == '\0')) {
tag->func = constant_item;
tag->arg = "%";
if (*s)
s++;
*sa = s;
return NULL;
}
tag->arg = "\0";
if (*s == '{') {
++s;
tag->arg = ap_getword(p,&s,'}');
}
tag_handler = (const char * (*)(request_rec *,char *))apr_hash_get(format_tag_hash, s++, 1);
if (!tag_handler) {
char dummy[2];
dummy[0] = s[-1];
dummy[1] = '\0';
return apr_pstrcat(p, "Unrecognized header format %", dummy, NULL);
}
tag->func = tag_handler;
*sa = s;
return NULL;
}
static char *parse_format_string(cmd_parms *cmd, header_entry *hdr, const char *s) {
apr_pool_t *p = cmd->pool;
char *res;
if (hdr->action == hdr_unset || hdr->action == hdr_echo) {
return NULL;
} else if (hdr->action == hdr_edit || hdr->action == hdr_edit_r ) {
s = hdr->subs;
}
if (!strncmp(s, "expr=", 5)) {
const char *err;
hdr->expr_out = ap_expr_parse_cmd(cmd, s+5,
AP_EXPR_FLAG_STRING_RESULT,
&err, NULL);
if (err) {
return apr_pstrcat(cmd->pool,
"Can't parse value expression : ", err, NULL);
}
return NULL;
}
hdr->ta = apr_array_make(p, 10, sizeof(format_tag));
while (*s) {
if ((res = parse_format_tag(p, (format_tag *) apr_array_push(hdr->ta), &s))) {
return res;
}
}
return NULL;
}
static APR_INLINE const char *header_inout_cmd(cmd_parms *cmd,
void *indirconf,
const char *action,
const char *hdr,
const char *value,
const char *subs,
const char *envclause) {
headers_conf *dirconf = indirconf;
const char *condition_var = NULL;
const char *colon;
header_entry *new;
ap_expr_info_t *expr = NULL;
apr_array_header_t *fixup = (cmd->info == &hdr_in)
? dirconf->fixup_in : (cmd->info == &hdr_out_always)
? dirconf->fixup_err
: dirconf->fixup_out;
new = (header_entry *) apr_array_push(fixup);
if (!strcasecmp(action, "set"))
new->action = hdr_set;
else if (!strcasecmp(action, "setifempty"))
new->action = hdr_setifempty;
else if (!strcasecmp(action, "add"))
new->action = hdr_add;
else if (!strcasecmp(action, "append"))
new->action = hdr_append;
else if (!strcasecmp(action, "merge"))
new->action = hdr_merge;
else if (!strcasecmp(action, "unset"))
new->action = hdr_unset;
else if (!strcasecmp(action, "echo"))
new->action = hdr_echo;
else if (!strcasecmp(action, "edit"))
new->action = hdr_edit;
else if (!strcasecmp(action, "edit*"))
new->action = hdr_edit_r;
else if (!strcasecmp(action, "note"))
new->action = hdr_note;
else
return "first argument must be 'add', 'set', 'setifempty', 'append', 'merge', "
"'unset', 'echo', 'note', 'edit', or 'edit*'.";
if (new->action == hdr_edit || new->action == hdr_edit_r) {
if (subs == NULL) {
return "Header edit requires a match and a substitution";
}
new->regex = ap_pregcomp(cmd->pool, value, AP_REG_EXTENDED);
if (new->regex == NULL) {
return "Header edit regex could not be compiled";
}
new->subs = subs;
} else {
if (envclause != NULL) {
return "Too many arguments to directive";
}
envclause = subs;
}
if (new->action == hdr_unset) {
if (value) {
if (envclause) {
return "header unset takes two arguments";
}
envclause = value;
value = NULL;
}
} else if (new->action == hdr_echo) {
ap_regex_t *regex;
if (value) {
if (envclause) {
return "Header echo takes two arguments";
}
envclause = value;
value = NULL;
}
if (cmd->info != &hdr_out_onsuccess && cmd->info != &hdr_out_always)
return "Header echo only valid on Header "
"directives";
else {
regex = ap_pregcomp(cmd->pool, hdr, AP_REG_EXTENDED | AP_REG_NOSUB);
if (regex == NULL) {
return "Header echo regex could not be compiled";
}
}
new->regex = regex;
} else if (!value)
return "Header requires three arguments";
if (envclause != NULL) {
if (strcasecmp(envclause, "early") == 0) {
condition_var = condition_early;
} else if (strncasecmp(envclause, "env=", 4) == 0) {
if ((envclause[4] == '\0')
|| ((envclause[4] == '!') && (envclause[5] == '\0'))) {
return "error: missing environment variable name. "
"envclause should be in the form env=envar ";
}
condition_var = envclause + 4;
} else if (strncasecmp(envclause, "expr=", 5) == 0) {
const char *err = NULL;
expr = ap_expr_parse_cmd(cmd, envclause + 5, 0, &err, NULL);
if (err) {
return apr_pstrcat(cmd->pool,
"Can't parse envclause/expression: ", err,
NULL);
}
} else {
return apr_pstrcat(cmd->pool, "Unknown parameter: ", envclause,
NULL);
}
}
if ((colon = ap_strchr_c(hdr, ':'))) {
hdr = apr_pstrmemdup(cmd->pool, hdr, colon-hdr);
}
new->header = hdr;
new->condition_var = condition_var;
new->expr = expr;
return parse_format_string(cmd, new, value);
}
static const char *header_cmd(cmd_parms *cmd, void *indirconf,
const char *args) {
const char *action;
const char *hdr;
const char *val;
const char *envclause;
const char *subs;
action = ap_getword_conf(cmd->temp_pool, &args);
if (cmd->info == &hdr_out_onsuccess) {
if (!strcasecmp(action, "always")) {
cmd->info = &hdr_out_always;
action = ap_getword_conf(cmd->temp_pool, &args);
} else if (!strcasecmp(action, "onsuccess")) {
action = ap_getword_conf(cmd->temp_pool, &args);
}
}
hdr = ap_getword_conf(cmd->pool, &args);
val = *args ? ap_getword_conf(cmd->pool, &args) : NULL;
subs = *args ? ap_getword_conf(cmd->pool, &args) : NULL;
envclause = *args ? ap_getword_conf(cmd->pool, &args) : NULL;
if (*args) {
return apr_pstrcat(cmd->pool, cmd->cmd->name,
" has too many arguments", NULL);
}
return header_inout_cmd(cmd, indirconf, action, hdr, val, subs, envclause);
}
static char* process_tags(header_entry *hdr, request_rec *r) {
int i;
const char *s;
char *str = NULL;
format_tag *tag = NULL;
if (hdr->expr_out) {
const char *err;
const char *val;
val = ap_expr_str_exec(r, hdr->expr_out, &err);
if (err) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(02557)
"Can't evaluate value expression: %s", err);
return "";
}
return apr_pstrdup(r->pool, val);
}
tag = (format_tag*) hdr->ta->elts;
for (i = 0; i < hdr->ta->nelts; i++) {
s = tag[i].func(r, tag[i].arg);
if (str == NULL)
str = apr_pstrdup(r->pool, s);
else
str = apr_pstrcat(r->pool, str, s, NULL);
}
return str ? str : "";
}
static const char *process_regexp(header_entry *hdr, const char *value,
request_rec *r) {
ap_regmatch_t pmatch[AP_MAX_REG_MATCH];
const char *subs;
const char *remainder;
char *ret;
int diffsz;
if (ap_regexec(hdr->regex, value, AP_MAX_REG_MATCH, pmatch, 0)) {
return value;
}
subs = ap_pregsub(r->pool, process_tags(hdr, r), value, AP_MAX_REG_MATCH, pmatch);
if (subs == NULL)
return NULL;
diffsz = strlen(subs) - (pmatch[0].rm_eo - pmatch[0].rm_so);
if (hdr->action == hdr_edit) {
remainder = value + pmatch[0].rm_eo;
} else {
remainder = process_regexp(hdr, value + pmatch[0].rm_eo, r);
if (remainder == NULL)
return NULL;
diffsz += strlen(remainder) - strlen(value + pmatch[0].rm_eo);
}
ret = apr_palloc(r->pool, strlen(value) + 1 + diffsz);
memcpy(ret, value, pmatch[0].rm_so);
strcpy(ret + pmatch[0].rm_so, subs);
strcat(ret, remainder);
return ret;
}
static int echo_header(void *v, const char *key, const char *val) {
edit_do *ed = v;
if (!ap_regexec(ed->hdr->regex, key, 0, NULL, 0)) {
apr_table_add(ed->r->headers_out, key, val);
}
return 1;
}
static int edit_header(void *v, const char *key, const char *val) {
edit_do *ed = (edit_do *)v;
const char *repl = process_regexp(ed->hdr, val, ed->r);
if (repl == NULL)
return 0;
apr_table_addn(ed->t, key, repl);
return 1;
}
static int add_them_all(void *v, const char *key, const char *val) {
apr_table_t *headers = (apr_table_t *)v;
apr_table_addn(headers, key, val);
return 1;
}
static int do_headers_fixup(request_rec *r, apr_table_t *headers,
apr_array_header_t *fixup, int early) {
echo_do v;
int i;
const char *val;
for (i = 0; i < fixup->nelts; ++i) {
header_entry *hdr = &((header_entry *) (fixup->elts))[i];
const char *envar = hdr->condition_var;
if (!early && (envar == condition_early)) {
continue;
} else if (early && (envar != condition_early)) {
continue;
} else if (hdr->expr != NULL) {
const char *err = NULL;
int eval = ap_expr_exec(r, hdr->expr, &err);
if (err) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(01501)
"Failed to evaluate expression (%s) - ignoring",
err);
} else if (!eval) {
continue;
}
} else if (envar && !early) {
if (*envar != '!') {
if (apr_table_get(r->subprocess_env, envar) == NULL)
continue;
} else {
if (apr_table_get(r->subprocess_env, &envar[1]) != NULL)
continue;
}
}
switch (hdr->action) {
case hdr_add:
apr_table_addn(headers, hdr->header, process_tags(hdr, r));
break;
case hdr_append:
apr_table_mergen(headers, hdr->header, process_tags(hdr, r));
break;
case hdr_merge:
val = apr_table_get(headers, hdr->header);
if (val == NULL) {
apr_table_addn(headers, hdr->header, process_tags(hdr, r));
} else {
char *new_val = process_tags(hdr, r);
apr_size_t new_val_len = strlen(new_val);
int tok_found = 0;
while (*val) {
const char *tok_start;
while (apr_isspace(*val))
++val;
tok_start = val;
while (*val && *val != ',') {
if (*val++ == '"')
while (*val)
if (*val++ == '"')
break;
}
if (new_val_len == (apr_size_t)(val - tok_start)
&& !strncmp(tok_start, new_val, new_val_len)) {
tok_found = 1;
break;
}
if (*val)
++val;
}
if (!tok_found) {
apr_table_mergen(headers, hdr->header, new_val);
}
}
break;
case hdr_set:
if (!strcasecmp(hdr->header, "Content-Type")) {
ap_set_content_type(r, process_tags(hdr, r));
}
apr_table_setn(headers, hdr->header, process_tags(hdr, r));
break;
case hdr_setifempty:
if (NULL == apr_table_get(headers, hdr->header)) {
if (!strcasecmp(hdr->header, "Content-Type")) {
ap_set_content_type(r, process_tags(hdr, r));
}
apr_table_setn(headers, hdr->header, process_tags(hdr, r));
}
break;
case hdr_unset:
apr_table_unset(headers, hdr->header);
break;
case hdr_echo:
v.r = r;
v.hdr = hdr;
apr_table_do(echo_header, &v, r->headers_in, NULL);
break;
case hdr_edit:
case hdr_edit_r:
if (!strcasecmp(hdr->header, "Content-Type") && r->content_type) {
const char *repl = process_regexp(hdr, r->content_type, r);
if (repl == NULL)
return 0;
ap_set_content_type(r, repl);
}
if (apr_table_get(headers, hdr->header)) {
edit_do ed;
ed.r = r;
ed.hdr = hdr;
ed.t = apr_table_make(r->pool, 5);
if (!apr_table_do(edit_header, (void *) &ed, headers,
hdr->header, NULL))
return 0;
apr_table_unset(headers, hdr->header);
apr_table_do(add_them_all, (void *) headers, ed.t, NULL);
}
break;
case hdr_note:
apr_table_setn(r->notes, process_tags(hdr, r), apr_table_get(headers, hdr->header));
break;
}
}
return 1;
}
static void ap_headers_insert_output_filter(request_rec *r) {
headers_conf *dirconf = ap_get_module_config(r->per_dir_config,
&headers_module);
if (dirconf->fixup_out->nelts || dirconf->fixup_err->nelts) {
ap_add_output_filter("FIXUP_HEADERS_OUT", NULL, r, r->connection);
}
}
static void ap_headers_insert_error_filter(request_rec *r) {
headers_conf *dirconf = ap_get_module_config(r->per_dir_config,
&headers_module);
if (dirconf->fixup_err->nelts) {
ap_add_output_filter("FIXUP_HEADERS_ERR", NULL, r, r->connection);
}
}
static apr_status_t ap_headers_output_filter(ap_filter_t *f,
apr_bucket_brigade *in) {
headers_conf *dirconf = ap_get_module_config(f->r->per_dir_config,
&headers_module);
ap_log_error(APLOG_MARK, APLOG_TRACE2, 0, f->r->server, APLOGNO(01502)
"headers: ap_headers_output_filter()");
do_headers_fixup(f->r, f->r->err_headers_out, dirconf->fixup_err, 0);
do_headers_fixup(f->r, f->r->headers_out, dirconf->fixup_out, 0);
ap_remove_output_filter(f);
return ap_pass_brigade(f->next,in);
}
static apr_status_t ap_headers_error_filter(ap_filter_t *f,
apr_bucket_brigade *in) {
headers_conf *dirconf;
dirconf = ap_get_module_config(f->r->per_dir_config,
&headers_module);
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, f->r->server, APLOGNO(01503)
"headers: ap_headers_error_filter()");
do_headers_fixup(f->r, f->r->err_headers_out, dirconf->fixup_err, 0);
ap_remove_output_filter(f);
return ap_pass_brigade(f->next, in);
}
static apr_status_t ap_headers_fixup(request_rec *r) {
headers_conf *dirconf = ap_get_module_config(r->per_dir_config,
&headers_module);
if (dirconf->fixup_in->nelts) {
do_headers_fixup(r, r->headers_in, dirconf->fixup_in, 0);
}
return DECLINED;
}
static apr_status_t ap_headers_early(request_rec *r) {
headers_conf *dirconf = ap_get_module_config(r->per_dir_config,
&headers_module);
if (dirconf->fixup_in->nelts) {
if (!do_headers_fixup(r, r->headers_in, dirconf->fixup_in, 1))
goto err;
}
if (dirconf->fixup_err->nelts) {
if (!do_headers_fixup(r, r->err_headers_out, dirconf->fixup_err, 1))
goto err;
}
if (dirconf->fixup_out->nelts) {
if (!do_headers_fixup(r, r->headers_out, dirconf->fixup_out, 1))
goto err;
}
return DECLINED;
err:
ap_log_rerror(APLOG_MARK, APLOG_CRIT, 0, r, APLOGNO(01504)
"Regular expression replacement failed (replacement too long?)");
return HTTP_INTERNAL_SERVER_ERROR;
}
static const command_rec headers_cmds[] = {
AP_INIT_RAW_ARGS("Header", header_cmd, &hdr_out_onsuccess, OR_FILEINFO,
"an optional condition, an action, header and value "
"followed by optional env clause"),
AP_INIT_RAW_ARGS("RequestHeader", header_cmd, &hdr_in, OR_FILEINFO,
"an action, header and value followed by optional env "
"clause"),
{NULL}
};
static void register_format_tag_handler(const char *tag,
format_tag_fn *tag_handler) {
apr_hash_set(format_tag_hash, tag, 1, tag_handler);
}
static int header_pre_config(apr_pool_t *p, apr_pool_t *plog, apr_pool_t *ptemp) {
format_tag_hash = apr_hash_make(p);
register_format_tag_handler("D", header_request_duration);
register_format_tag_handler("t", header_request_time);
register_format_tag_handler("e", header_request_env_var);
register_format_tag_handler("s", header_request_ssl_var);
register_format_tag_handler("l", header_request_loadavg);
register_format_tag_handler("i", header_request_idle);
register_format_tag_handler("b", header_request_busy);
return OK;
}
static int header_post_config(apr_pool_t *pconf, apr_pool_t *plog,
apr_pool_t *ptemp, server_rec *s) {
header_ssl_lookup = APR_RETRIEVE_OPTIONAL_FN(ssl_var_lookup);
return OK;
}
static void register_hooks(apr_pool_t *p) {
ap_register_output_filter("FIXUP_HEADERS_OUT", ap_headers_output_filter,
NULL, AP_FTYPE_CONTENT_SET);
ap_register_output_filter("FIXUP_HEADERS_ERR", ap_headers_error_filter,
NULL, AP_FTYPE_CONTENT_SET);
ap_hook_pre_config(header_pre_config,NULL,NULL,APR_HOOK_MIDDLE);
ap_hook_post_config(header_post_config,NULL,NULL,APR_HOOK_MIDDLE);
ap_hook_insert_filter(ap_headers_insert_output_filter, NULL, NULL, APR_HOOK_LAST);
ap_hook_insert_error_filter(ap_headers_insert_error_filter,
NULL, NULL, APR_HOOK_LAST);
ap_hook_fixups(ap_headers_fixup, NULL, NULL, APR_HOOK_LAST);
ap_hook_post_read_request(ap_headers_early, NULL, NULL, APR_HOOK_FIRST);
}
AP_DECLARE_MODULE(headers) = {
STANDARD20_MODULE_STUFF,
create_headers_dir_config,
merge_headers_config,
NULL,
NULL,
headers_cmds,
register_hooks
};