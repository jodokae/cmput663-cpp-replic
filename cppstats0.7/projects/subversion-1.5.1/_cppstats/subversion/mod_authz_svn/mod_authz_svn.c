#include <httpd.h>
#include <http_config.h>
#include <http_core.h>
#include <http_request.h>
#include <http_protocol.h>
#include <http_log.h>
#include <ap_config.h>
#include <ap_provider.h>
#include <apr_uri.h>
#include <mod_dav.h>
#include "mod_dav_svn.h"
#include "mod_authz_svn.h"
#include "svn_path.h"
#include "svn_config.h"
#include "svn_string.h"
#include "svn_repos.h"
extern module AP_MODULE_DECLARE_DATA authz_svn_module;
typedef struct {
int authoritative;
int anonymous;
int no_auth_when_anon_ok;
const char *base_path;
const char *access_file;
} authz_svn_config_rec;
static void *create_authz_svn_dir_config(apr_pool_t *p, char *d) {
authz_svn_config_rec *conf = apr_pcalloc(p, sizeof(*conf));
conf->base_path = d;
conf->authoritative = 1;
conf->anonymous = 1;
return conf;
}
static const command_rec authz_svn_cmds[] = {
AP_INIT_FLAG("AuthzSVNAuthoritative", ap_set_flag_slot,
(void *)APR_OFFSETOF(authz_svn_config_rec, authoritative),
OR_AUTHCFG,
"Set to 'Off' to allow access control to be passed along to "
"lower modules. (default is On.)"),
AP_INIT_TAKE1("AuthzSVNAccessFile", ap_set_file_slot,
(void *)APR_OFFSETOF(authz_svn_config_rec, access_file),
OR_AUTHCFG,
"Text file containing permissions of repository paths."),
AP_INIT_FLAG("AuthzSVNAnonymous", ap_set_flag_slot,
(void *)APR_OFFSETOF(authz_svn_config_rec, anonymous),
OR_AUTHCFG,
"Set to 'Off' to disable two special-case behaviours of "
"this module: (1) interaction with the 'Satisfy Any' "
"directive, and (2) enforcement of the authorization "
"policy even when no 'Require' directives are present. "
"(default is On.)"),
AP_INIT_FLAG("AuthzSVNNoAuthWhenAnonymousAllowed", ap_set_flag_slot,
(void *)APR_OFFSETOF(authz_svn_config_rec,
no_auth_when_anon_ok),
OR_AUTHCFG,
"Set to 'On' to suppress authentication and authorization "
"for requests which anonymous users are allowed to perform. "
"(default is Off.)"),
{ NULL }
};
static svn_authz_t *get_access_conf(request_rec *r,
authz_svn_config_rec *conf) {
const char *cache_key = NULL;
void *user_data = NULL;
svn_authz_t *access_conf = NULL;
svn_error_t *svn_err;
char errbuf[256];
cache_key = apr_pstrcat(r->pool, "mod_authz_svn:",
conf->access_file, NULL);
apr_pool_userdata_get(&user_data, cache_key, r->connection->pool);
access_conf = user_data;
if (access_conf == NULL) {
svn_err = svn_repos_authz_read(&access_conf, conf->access_file,
TRUE, r->connection->pool);
if (svn_err) {
ap_log_rerror(APLOG_MARK, APLOG_ERR,
((svn_err->apr_err >= APR_OS_START_USERERR &&
svn_err->apr_err < APR_OS_START_CANONERR) ?
0 : svn_err->apr_err),
r, "Failed to load the AuthzSVNAccessFile: %s",
svn_err_best_message(svn_err,
errbuf, sizeof(errbuf)));
svn_error_clear(svn_err);
access_conf = NULL;
} else {
apr_pool_userdata_set(access_conf, cache_key,
NULL, r->connection->pool);
}
}
return access_conf;
}
static int req_check_access(request_rec *r,
authz_svn_config_rec *conf,
const char **repos_path_ref,
const char **dest_repos_path_ref) {
const char *dest_uri;
apr_uri_t parsed_dest_uri;
const char *cleaned_uri;
int trailing_slash;
const char *repos_name;
const char *dest_repos_name;
const char *relative_path;
const char *repos_path;
const char *dest_repos_path = NULL;
dav_error *dav_err;
svn_repos_authz_access_t authz_svn_type = svn_authz_none;
svn_boolean_t authz_access_granted = FALSE;
svn_authz_t *access_conf = NULL;
svn_error_t *svn_err;
char errbuf[256];
switch (r->method_number) {
case M_COPY:
authz_svn_type |= svn_authz_recursive;
case M_OPTIONS:
case M_GET:
case M_PROPFIND:
case M_REPORT:
authz_svn_type |= svn_authz_read;
break;
case M_MOVE:
case M_DELETE:
authz_svn_type |= svn_authz_recursive;
case M_MKCOL:
case M_PUT:
case M_PROPPATCH:
case M_CHECKOUT:
case M_MERGE:
case M_MKACTIVITY:
case M_LOCK:
case M_UNLOCK:
authz_svn_type |= svn_authz_write;
break;
default:
authz_svn_type |= svn_authz_write | svn_authz_recursive;
break;
}
dav_err = dav_svn_split_uri(r,
r->uri,
conf->base_path,
&cleaned_uri,
&trailing_slash,
&repos_name,
&relative_path,
&repos_path);
if (dav_err) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r,
"%s [%d, #%d]",
dav_err->desc, dav_err->status, dav_err->error_id);
return (dav_err->status != OK && dav_err->status != DECLINED) ?
dav_err->status : HTTP_INTERNAL_SERVER_ERROR;
}
if (r->method_number == M_MERGE) {
repos_path = NULL;
}
if (repos_path)
repos_path = svn_path_join("/", repos_path, r->pool);
*repos_path_ref = apr_pstrcat(r->pool, repos_name, ":", repos_path, NULL);
if (r->method_number == M_MOVE || r->method_number == M_COPY) {
dest_uri = apr_table_get(r->headers_in, "Destination");
if (!dest_uri)
return DECLINED;
apr_uri_parse(r->pool, dest_uri, &parsed_dest_uri);
ap_unescape_url(parsed_dest_uri.path);
dest_uri = parsed_dest_uri.path;
if (strncmp(dest_uri, conf->base_path, strlen(conf->base_path))) {
return HTTP_BAD_REQUEST;
}
dav_err = dav_svn_split_uri(r,
dest_uri,
conf->base_path,
&cleaned_uri,
&trailing_slash,
&dest_repos_name,
&relative_path,
&dest_repos_path);
if (dav_err) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r,
"%s [%d, #%d]",
dav_err->desc, dav_err->status, dav_err->error_id);
return (dav_err->status != OK && dav_err->status != DECLINED) ?
dav_err->status : HTTP_INTERNAL_SERVER_ERROR;
}
if (dest_repos_path)
dest_repos_path = svn_path_join("/", dest_repos_path, r->pool);
*dest_repos_path_ref = apr_pstrcat(r->pool, dest_repos_name, ":",
dest_repos_path, NULL);
}
access_conf = get_access_conf(r,conf);
if (access_conf == NULL) {
return DECLINED;
}
if (repos_path
|| (!repos_path && (authz_svn_type & svn_authz_write))) {
svn_err = svn_repos_authz_check_access(access_conf, repos_name,
repos_path, r->user,
authz_svn_type,
&authz_access_granted,
r->pool);
if (svn_err) {
ap_log_rerror(APLOG_MARK, APLOG_ERR,
((svn_err->apr_err >= APR_OS_START_USERERR &&
svn_err->apr_err < APR_OS_START_CANONERR) ?
0 : svn_err->apr_err),
r, "Failed to perform access control: %s",
svn_err_best_message(svn_err, errbuf, sizeof(errbuf)));
svn_error_clear(svn_err);
return DECLINED;
}
if (!authz_access_granted)
return DECLINED;
}
if (r->method_number != M_MOVE
&& r->method_number != M_COPY) {
return OK;
}
if (repos_path) {
svn_err = svn_repos_authz_check_access(access_conf,
dest_repos_name,
dest_repos_path,
r->user,
svn_authz_write
|svn_authz_recursive,
&authz_access_granted,
r->pool);
if (svn_err) {
ap_log_rerror(APLOG_MARK, APLOG_ERR,
((svn_err->apr_err >= APR_OS_START_USERERR &&
svn_err->apr_err < APR_OS_START_CANONERR) ?
0 : svn_err->apr_err),
r, "Failed to perform access control: %s",
svn_err_best_message(svn_err, errbuf, sizeof(errbuf)));
svn_error_clear(svn_err);
return DECLINED;
}
if (!authz_access_granted)
return DECLINED;
}
return OK;
}
static void log_access_verdict(const char *file, int line,
const request_rec *r,
int allowed,
const char *repos_path,
const char *dest_repos_path) {
int level = allowed ? APLOG_INFO : APLOG_ERR;
const char *verdict = allowed ? "granted" : "denied";
if (r->user) {
if (dest_repos_path) {
ap_log_rerror(file, line, level, 0, r,
"Access %s: '%s' %s %s %s", verdict, r->user,
r->method, repos_path, dest_repos_path);
} else {
ap_log_rerror(file, line, level, 0, r,
"Access %s: '%s' %s %s", verdict, r->user,
r->method, repos_path);
}
} else {
if (dest_repos_path) {
ap_log_rerror(file, line, level, 0, r,
"Access %s: - %s %s %s", verdict,
r->method, repos_path, dest_repos_path);
} else {
ap_log_rerror(file, line, level, 0, r,
"Access %s: - %s %s", verdict,
r->method, repos_path);
}
}
}
static int subreq_bypass(request_rec *r,
const char *repos_path,
const char *repos_name) {
svn_error_t *svn_err = NULL;
svn_authz_t *access_conf = NULL;
authz_svn_config_rec *conf = NULL;
svn_boolean_t authz_access_granted = FALSE;
char errbuf[256];
conf = ap_get_module_config(r->per_dir_config,
&authz_svn_module);
if (!conf->anonymous || !conf->access_file) {
log_access_verdict(APLOG_MARK, r, 0, repos_path, NULL);
return HTTP_FORBIDDEN;
}
access_conf = get_access_conf(r,conf);
if (access_conf == NULL)
return HTTP_FORBIDDEN;
if (repos_path) {
svn_err = svn_repos_authz_check_access(access_conf, repos_name,
repos_path, r->user,
svn_authz_none|svn_authz_read,
&authz_access_granted,
r->pool);
if (svn_err) {
ap_log_rerror(APLOG_MARK, APLOG_ERR,
((svn_err->apr_err >= APR_OS_START_USERERR &&
svn_err->apr_err < APR_OS_START_CANONERR) ?
0 : svn_err->apr_err),
r, "Failed to perform access control: %s",
svn_err_best_message(svn_err, errbuf, sizeof(errbuf)));
svn_error_clear(svn_err);
return HTTP_FORBIDDEN;
}
if (!authz_access_granted) {
log_access_verdict(APLOG_MARK, r, 0, repos_path, NULL);
return HTTP_FORBIDDEN;
}
}
log_access_verdict(APLOG_MARK, r, 1, repos_path, NULL);
return OK;
}
static int access_checker(request_rec *r) {
authz_svn_config_rec *conf = ap_get_module_config(r->per_dir_config,
&authz_svn_module);
const char *repos_path;
const char *dest_repos_path = NULL;
int status;
if (!conf->anonymous || !conf->access_file)
return DECLINED;
if (ap_some_auth_required(r)) {
if (ap_satisfies(r) != SATISFY_ANY)
return DECLINED;
if (apr_table_get(r->headers_in,
(PROXYREQ_PROXY == r->proxyreq)
? "Proxy-Authorization" : "Authorization")) {
return HTTP_FORBIDDEN;
}
}
status = req_check_access(r, conf, &repos_path, &dest_repos_path);
if (status == DECLINED) {
if (!conf->authoritative)
return DECLINED;
if (!ap_some_auth_required(r)) {
log_access_verdict(APLOG_MARK, r, 0, repos_path, dest_repos_path);
}
return HTTP_FORBIDDEN;
}
if (status != OK)
return status;
log_access_verdict(APLOG_MARK, r, 1, repos_path, dest_repos_path);
return OK;
}
static int check_user_id(request_rec *r) {
authz_svn_config_rec *conf = ap_get_module_config(r->per_dir_config,
&authz_svn_module);
const char *repos_path;
const char *dest_repos_path = NULL;
int status;
if (!conf->access_file || !conf->no_auth_when_anon_ok || r->user)
return DECLINED;
status = req_check_access(r, conf, &repos_path, &dest_repos_path);
if (status == OK) {
apr_table_setn(r->notes, "authz_svn-anon-ok", (const char*)1);
log_access_verdict(APLOG_MARK, r, 1, repos_path, dest_repos_path);
return OK;
}
return status;
}
static int auth_checker(request_rec *r) {
authz_svn_config_rec *conf = ap_get_module_config(r->per_dir_config,
&authz_svn_module);
const char *repos_path;
const char *dest_repos_path = NULL;
int status;
if (!conf->access_file)
return DECLINED;
if (!r->user && apr_table_get(r->notes, "authz_svn-anon-ok")) {
return OK;
}
status = req_check_access(r, conf, &repos_path, &dest_repos_path);
if (status == DECLINED) {
if (conf->authoritative) {
log_access_verdict(APLOG_MARK, r, 0, repos_path, dest_repos_path);
ap_note_auth_failure(r);
return HTTP_FORBIDDEN;
}
return DECLINED;
}
if (status != OK)
return status;
log_access_verdict(APLOG_MARK, r, 1, repos_path, dest_repos_path);
return OK;
}
static void register_hooks(apr_pool_t *p) {
static const char * const mod_ssl[] = { "mod_ssl.c", NULL };
ap_hook_access_checker(access_checker, NULL, NULL, APR_HOOK_LAST);
ap_hook_check_user_id(check_user_id, mod_ssl, NULL, APR_HOOK_FIRST);
ap_hook_auth_checker(auth_checker, NULL, NULL, APR_HOOK_FIRST);
ap_register_provider(p,
AUTHZ_SVN__SUBREQ_BYPASS_PROV_GRP,
AUTHZ_SVN__SUBREQ_BYPASS_PROV_NAME,
AUTHZ_SVN__SUBREQ_BYPASS_PROV_VER,
(void*)subreq_bypass);
}
module AP_MODULE_DECLARE_DATA authz_svn_module = {
STANDARD20_MODULE_STUFF,
create_authz_svn_dir_config,
NULL,
NULL,
NULL,
authz_svn_cmds,
register_hooks
};
