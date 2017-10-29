#include "apr_strings.h"
#include "apr_lib.h"
#define APR_WANT_STRFUNC
#include "apr_want.h"
#include "ap_config.h"
#include "httpd.h"
#include "http_core.h"
#include "http_config.h"
#include "http_request.h"
#include "http_log.h"
#include "ap_expr.h"
typedef struct {
const char *real;
const char *fake;
char *handler;
ap_regex_t *regexp;
int redir_status;
} alias_entry;
typedef struct {
apr_array_header_t *aliases;
apr_array_header_t *redirects;
} alias_server_conf;
typedef struct {
unsigned int alias_set:1;
unsigned int redirect_set:1;
apr_array_header_t *redirects;
const ap_expr_info_t *alias;
char *handler;
const ap_expr_info_t *redirect;
int redirect_status;
} alias_dir_conf;
module AP_MODULE_DECLARE_DATA alias_module;
static char magic_error_value;
#define PREGSUB_ERROR (&magic_error_value)
static void *create_alias_config(apr_pool_t *p, server_rec *s) {
alias_server_conf *a =
(alias_server_conf *) apr_pcalloc(p, sizeof(alias_server_conf));
a->aliases = apr_array_make(p, 20, sizeof(alias_entry));
a->redirects = apr_array_make(p, 20, sizeof(alias_entry));
return a;
}
static void *create_alias_dir_config(apr_pool_t *p, char *d) {
alias_dir_conf *a =
(alias_dir_conf *) apr_pcalloc(p, sizeof(alias_dir_conf));
a->redirects = apr_array_make(p, 2, sizeof(alias_entry));
return a;
}
static void *merge_alias_config(apr_pool_t *p, void *basev, void *overridesv) {
alias_server_conf *a =
(alias_server_conf *) apr_pcalloc(p, sizeof(alias_server_conf));
alias_server_conf *base = (alias_server_conf *) basev;
alias_server_conf *overrides = (alias_server_conf *) overridesv;
a->aliases = apr_array_append(p, overrides->aliases, base->aliases);
a->redirects = apr_array_append(p, overrides->redirects, base->redirects);
return a;
}
static void *merge_alias_dir_config(apr_pool_t *p, void *basev, void *overridesv) {
alias_dir_conf *a =
(alias_dir_conf *) apr_pcalloc(p, sizeof(alias_dir_conf));
alias_dir_conf *base = (alias_dir_conf *) basev;
alias_dir_conf *overrides = (alias_dir_conf *) overridesv;
a->redirects = apr_array_append(p, overrides->redirects, base->redirects);
a->alias = (overrides->alias_set == 0) ? base->alias : overrides->alias;
a->handler = (overrides->alias_set == 0) ? base->handler : overrides->handler;
a->alias_set = overrides->alias_set || base->alias_set;
a->redirect = (overrides->redirect_set == 0) ? base->redirect : overrides->redirect;
a->redirect_status = (overrides->redirect_set == 0) ? base->redirect_status : overrides->redirect_status;
a->redirect_set = overrides->redirect_set || base->redirect_set;
return a;
}
static int alias_matches(const char *uri, const char *alias_fakename);
static const char *add_alias_internal(cmd_parms *cmd, void *dummy,
const char *fake, const char *real,
int use_regex) {
server_rec *s = cmd->server;
alias_server_conf *conf = ap_get_module_config(s->module_config,
&alias_module);
alias_entry *new = apr_array_push(conf->aliases);
alias_entry *entries = (alias_entry *)conf->aliases->elts;
int i;
const char *err = ap_check_cmd_context(cmd, NOT_IN_DIR_LOC_FILE);
if (err != NULL) {
return err;
}
if (use_regex) {
new->regexp = ap_pregcomp(cmd->pool, fake, AP_REG_EXTENDED);
if (new->regexp == NULL)
return "Regular expression could not be compiled.";
new->real = real;
} else {
new->real = real;
}
new->fake = fake;
new->handler = cmd->info;
if (!use_regex) {
for (i = 0; i < conf->aliases->nelts - 1; ++i) {
alias_entry *alias = &entries[i];
if ( (!alias->regexp && alias_matches(fake, alias->fake) > 0)
|| (alias->regexp && !ap_regexec(alias->regexp, fake, 0, NULL, 0))) {
ap_log_error(APLOG_MARK, APLOG_WARNING, 0, cmd->server, APLOGNO(00671)
"The %s directive in %s at line %d will probably "
"never match because it overlaps an earlier "
"%sAlias%s.",
cmd->cmd->name, cmd->directive->filename,
cmd->directive->line_num,
alias->handler ? "Script" : "",
alias->regexp ? "Match" : "");
break;
}
}
}
return NULL;
}
static const char *add_alias(cmd_parms *cmd, void *dummy, const char *fake,
const char *real) {
if (real) {
return add_alias_internal(cmd, dummy, fake, real, 0);
} else {
alias_dir_conf *dirconf = (alias_dir_conf *) dummy;
const char *err = ap_check_cmd_context(cmd, NOT_IN_DIRECTORY|NOT_IN_FILES);
if (err != NULL) {
return err;
}
if (!cmd->path) {
return "Alias must have two arguments when used globally";
}
dirconf->alias =
ap_expr_parse_cmd(cmd, fake, AP_EXPR_FLAG_STRING_RESULT,
&err, NULL);
if (err) {
return apr_pstrcat(cmd->temp_pool,
"Cannot parse alias expression '", fake, "': ", err,
NULL);
}
dirconf->handler = cmd->info;
dirconf->alias_set = 1;
return NULL;
}
}
static const char *add_alias_regex(cmd_parms *cmd, void *dummy,
const char *fake, const char *real) {
return add_alias_internal(cmd, dummy, fake, real, 1);
}
static const char *add_redirect_internal(cmd_parms *cmd,
alias_dir_conf *dirconf,
const char *arg1, const char *arg2,
const char *arg3, int use_regex) {
alias_entry *new;
server_rec *s = cmd->server;
alias_server_conf *serverconf = ap_get_module_config(s->module_config,
&alias_module);
int status = (int) (long) cmd->info;
int grokarg1 = 1;
ap_regex_t *regex = NULL;
const char *fake = arg2;
const char *url = arg3;
if (!strcasecmp(arg1, "permanent"))
status = HTTP_MOVED_PERMANENTLY;
else if (!strcasecmp(arg1, "temp"))
status = HTTP_MOVED_TEMPORARILY;
else if (!strcasecmp(arg1, "seeother"))
status = HTTP_SEE_OTHER;
else if (!strcasecmp(arg1, "gone")) {
status = HTTP_GONE;
grokarg1 = -1;
} else if (apr_isdigit(*arg1)) {
status = atoi(arg1);
if (!ap_is_HTTP_REDIRECT(status)) {
grokarg1 = -1;
}
} else {
grokarg1 = 0;
}
if (arg3 && !grokarg1)
return "Redirect: invalid first argument (of three)";
if (!cmd->path) {
;
} else if ((grokarg1 > 0 && arg2 && !arg3) || (!grokarg1 && !arg2)) {
const char *expr_err = NULL;
url = grokarg1 ? arg2 : arg1;
dirconf->redirect =
ap_expr_parse_cmd(cmd, url, AP_EXPR_FLAG_STRING_RESULT,
&expr_err, NULL);
if (expr_err) {
return apr_pstrcat(cmd->temp_pool,
"Cannot parse redirect expression '", url, "': ", expr_err,
NULL);
}
dirconf->redirect_status = status;
dirconf->redirect_set = 1;
return NULL;
} else if (grokarg1 < 0 && !arg2) {
dirconf->redirect_status = status;
dirconf->redirect_set = 1;
return NULL;
}
if (!arg3 && !grokarg1) {
fake = arg1;
url = arg2;
}
if (use_regex) {
regex = ap_pregcomp(cmd->pool, fake, AP_REG_EXTENDED);
if (regex == NULL)
return "Regular expression could not be compiled.";
}
if (ap_is_HTTP_REDIRECT(status)) {
if (!url)
return "URL to redirect to is missing";
if (!use_regex && !ap_is_url(url) && (url[0] != '/'))
return "Redirect to non-URL";
} else {
if (url)
return "Redirect URL not valid for this status";
}
if (cmd->path)
new = apr_array_push(dirconf->redirects);
else
new = apr_array_push(serverconf->redirects);
new->fake = fake;
new->real = url;
new->regexp = regex;
new->redir_status = status;
return NULL;
}
static const char *add_redirect(cmd_parms *cmd, void *dirconf,
const char *arg1, const char *arg2,
const char *arg3) {
return add_redirect_internal(cmd, dirconf, arg1, arg2, arg3, 0);
}
static const char *add_redirect2(cmd_parms *cmd, void *dirconf,
const char *arg1, const char *arg2) {
return add_redirect_internal(cmd, dirconf, arg1, arg2, NULL, 0);
}
static const char *add_redirect_regex(cmd_parms *cmd, void *dirconf,
const char *arg1, const char *arg2,
const char *arg3) {
return add_redirect_internal(cmd, dirconf, arg1, arg2, arg3, 1);
}
static const command_rec alias_cmds[] = {
AP_INIT_TAKE12("Alias", add_alias, NULL, RSRC_CONF | ACCESS_CONF,
"a fakename and a realname, or a realname in a Location"),
AP_INIT_TAKE12("ScriptAlias", add_alias, "cgi-script", RSRC_CONF | ACCESS_CONF,
"a fakename and a realname, or a realname in a Location"),
AP_INIT_TAKE123("Redirect", add_redirect, (void *) HTTP_MOVED_TEMPORARILY,
OR_FILEINFO,
"an optional status, then document to be redirected and "
"destination URL"),
AP_INIT_TAKE2("AliasMatch", add_alias_regex, NULL, RSRC_CONF,
"a regular expression and a filename"),
AP_INIT_TAKE2("ScriptAliasMatch", add_alias_regex, "cgi-script", RSRC_CONF,
"a regular expression and a filename"),
AP_INIT_TAKE23("RedirectMatch", add_redirect_regex,
(void *) HTTP_MOVED_TEMPORARILY, OR_FILEINFO,
"an optional status, then a regular expression and "
"destination URL"),
AP_INIT_TAKE2("RedirectTemp", add_redirect2,
(void *) HTTP_MOVED_TEMPORARILY, OR_FILEINFO,
"a document to be redirected, then the destination URL"),
AP_INIT_TAKE2("RedirectPermanent", add_redirect2,
(void *) HTTP_MOVED_PERMANENTLY, OR_FILEINFO,
"a document to be redirected, then the destination URL"),
{NULL}
};
static int alias_matches(const char *uri, const char *alias_fakename) {
const char *aliasp = alias_fakename, *urip = uri;
while (*aliasp) {
if (*aliasp == '/') {
if (*urip != '/')
return 0;
do {
++aliasp;
} while (*aliasp == '/');
do {
++urip;
} while (*urip == '/');
} else {
if (*urip++ != *aliasp++)
return 0;
}
}
if (aliasp[-1] != '/' && *urip != '\0' && *urip != '/')
return 0;
return urip - uri;
}
static char *try_alias(request_rec *r) {
alias_dir_conf *dirconf =
(alias_dir_conf *) ap_get_module_config(r->per_dir_config, &alias_module);
if (dirconf->alias) {
const char *err = NULL;
char *found = apr_pstrdup(r->pool,
ap_expr_str_exec(r, dirconf->alias, &err));
if (err) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(02825)
"Can't evaluate alias expression: %s", err);
return PREGSUB_ERROR;
}
if (dirconf->handler) {
r->handler = dirconf->handler;
apr_table_setn(r->notes, "alias-forced-type", r->handler);
}
found = ap_server_root_relative(r->pool, found);
return found;
}
return NULL;
}
static char *try_redirect(request_rec *r, int *status) {
alias_dir_conf *dirconf =
(alias_dir_conf *) ap_get_module_config(r->per_dir_config, &alias_module);
if (dirconf->redirect_set) {
apr_uri_t uri;
const char *err = NULL;
char *found = "";
if (dirconf->redirect) {
found = apr_pstrdup(r->pool,
ap_expr_str_exec(r, dirconf->redirect, &err));
if (err) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(02826)
"Can't evaluate redirect expression: %s", err);
return PREGSUB_ERROR;
}
apr_uri_parse(r->pool, found, &uri);
found = apr_uri_unparse(r->pool, &uri, APR_URI_UNP_OMITQUERY);
found = ap_escape_uri(r->pool, found);
if (uri.query) {
found = apr_pstrcat(r->pool, found, "?", uri.query, NULL);
}
if (uri.fragment) {
found = apr_pstrcat(r->pool, found, "#", uri.fragment, NULL);
}
}
*status = dirconf->redirect_status;
return found;
}
return NULL;
}
static char *try_alias_list(request_rec *r, apr_array_header_t *aliases,
int is_redir, int *status) {
alias_entry *entries = (alias_entry *) aliases->elts;
ap_regmatch_t regm[AP_MAX_REG_MATCH];
char *found = NULL;
int i;
for (i = 0; i < aliases->nelts; ++i) {
alias_entry *alias = &entries[i];
int l;
if (alias->regexp) {
if (!ap_regexec(alias->regexp, r->uri, AP_MAX_REG_MATCH, regm, 0)) {
if (alias->real) {
found = ap_pregsub(r->pool, alias->real, r->uri,
AP_MAX_REG_MATCH, regm);
if (found) {
if (is_redir) {
apr_uri_t uri;
apr_uri_parse(r->pool, found, &uri);
found = apr_uri_unparse(r->pool, &uri,
APR_URI_UNP_OMITQUERY);
found = ap_escape_uri(r->pool, found);
if (uri.query) {
found = apr_pstrcat(r->pool, found, "?",
uri.query, NULL);
}
if (uri.fragment) {
found = apr_pstrcat(r->pool, found, "#",
uri.fragment, NULL);
}
}
} else {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(00672)
"Regex substitution in '%s' failed. "
"Replacement too long?", alias->real);
return PREGSUB_ERROR;
}
} else {
found = "";
}
}
} else {
l = alias_matches(r->uri, alias->fake);
if (l > 0) {
ap_set_context_info(r, alias->fake, alias->real);
if (is_redir) {
char *escurl;
escurl = ap_os_escape_path(r->pool, r->uri + l, 1);
found = apr_pstrcat(r->pool, alias->real, escurl, NULL);
} else
found = apr_pstrcat(r->pool, alias->real, r->uri + l, NULL);
}
}
if (found) {
if (alias->handler) {
r->handler = alias->handler;
apr_table_setn(r->notes, "alias-forced-type", r->handler);
}
if (!is_redir) {
found = ap_server_root_relative(r->pool, found);
}
if (found) {
*status = alias->redir_status;
}
return found;
}
}
return NULL;
}
static int translate_alias_redir(request_rec *r) {
ap_conf_vector_t *sconf = r->server->module_config;
alias_server_conf *serverconf = ap_get_module_config(sconf, &alias_module);
char *ret;
int status;
if (r->uri[0] != '/' && r->uri[0] != '\0') {
return DECLINED;
}
if ((ret = try_redirect(r, &status)) != NULL
|| (ret = try_alias_list(r, serverconf->redirects, 1, &status))
!= NULL) {
if (ret == PREGSUB_ERROR)
return HTTP_INTERNAL_SERVER_ERROR;
if (ap_is_HTTP_REDIRECT(status)) {
if (ret[0] == '/') {
char *orig_target = ret;
ret = ap_construct_url(r->pool, ret, r);
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(00673)
"incomplete redirection target of '%s' for "
"URI '%s' modified to '%s'",
orig_target, r->uri, ret);
}
if (!ap_is_url(ret)) {
status = HTTP_INTERNAL_SERVER_ERROR;
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(00674)
"cannot redirect '%s' to '%s'; "
"target is not a valid absoluteURI or abs_path",
r->uri, ret);
} else {
if (r->args && !ap_strchr(ret, '?')) {
ret = apr_pstrcat(r->pool, ret, "?", r->args, NULL);
}
apr_table_setn(r->headers_out, "Location", ret);
}
}
return status;
}
if ((ret = try_alias(r)) != NULL
|| (ret = try_alias_list(r, serverconf->aliases, 0, &status))
!= NULL) {
r->filename = ret;
return OK;
}
return DECLINED;
}
static int fixup_redir(request_rec *r) {
void *dconf = r->per_dir_config;
alias_dir_conf *dirconf =
(alias_dir_conf *) ap_get_module_config(dconf, &alias_module);
char *ret;
int status;
if ((ret = try_redirect(r, &status)) != NULL
|| (ret = try_alias_list(r, dirconf->redirects, 1, &status))
!= NULL) {
if (ret == PREGSUB_ERROR)
return HTTP_INTERNAL_SERVER_ERROR;
if (ap_is_HTTP_REDIRECT(status)) {
if (ret[0] == '/') {
char *orig_target = ret;
ret = ap_construct_url(r->pool, ret, r);
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(00675)
"incomplete redirection target of '%s' for "
"URI '%s' modified to '%s'",
orig_target, r->uri, ret);
}
if (!ap_is_url(ret)) {
status = HTTP_INTERNAL_SERVER_ERROR;
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(00676)
"cannot redirect '%s' to '%s'; "
"target is not a valid absoluteURI or abs_path",
r->uri, ret);
} else {
if (r->args && !ap_strchr(ret, '?')) {
ret = apr_pstrcat(r->pool, ret, "?", r->args, NULL);
}
apr_table_setn(r->headers_out, "Location", ret);
}
}
return status;
}
return DECLINED;
}
static void register_hooks(apr_pool_t *p) {
static const char * const aszSucc[]= { "mod_userdir.c",
"mod_vhost_alias.c",NULL
};
ap_hook_translate_name(translate_alias_redir,NULL,aszSucc,APR_HOOK_MIDDLE);
ap_hook_fixups(fixup_redir,NULL,NULL,APR_HOOK_MIDDLE);
}
AP_DECLARE_MODULE(alias) = {
STANDARD20_MODULE_STUFF,
create_alias_dir_config,
merge_alias_dir_config,
create_alias_config,
merge_alias_config,
alias_cmds,
register_hooks
};
