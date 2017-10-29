#include "apr_strings.h"
#include "apr_lib.h"
#include "apr_base64.h"
#define APR_WANT_STRFUNC
#include "apr_want.h"
#include "ap_config.h"
#include "httpd.h"
#include "http_config.h"
#include "http_core.h"
#include "http_log.h"
#include "http_protocol.h"
#include "http_request.h"
#include "ap_provider.h"
#include "util_md5.h"
#include "ap_expr.h"
#include "mod_auth.h"
#include "mod_session.h"
#include "mod_request.h"
#define FORM_LOGIN_HANDLER "form-login-handler"
#define FORM_LOGOUT_HANDLER "form-logout-handler"
#define FORM_REDIRECT_HANDLER "form-redirect-handler"
#define MOD_AUTH_FORM_HASH "site"
static int (*ap_session_load_fn) (request_rec * r, session_rec ** z) = NULL;
static apr_status_t (*ap_session_get_fn)(request_rec * r, session_rec * z,
const char *key, const char **value) = NULL;
static apr_status_t (*ap_session_set_fn)(request_rec * r, session_rec * z,
const char *key, const char *value) = NULL;
static void (*ap_request_insert_filter_fn) (request_rec * r) = NULL;
static void (*ap_request_remove_filter_fn) (request_rec * r) = NULL;
typedef struct {
authn_provider_list *providers;
char *dir;
int authoritative;
int authoritative_set;
const char *site;
int site_set;
const char *username;
int username_set;
const char *password;
int password_set;
apr_size_t form_size;
int form_size_set;
int fakebasicauth;
int fakebasicauth_set;
const char *location;
int location_set;
const char *method;
int method_set;
const char *mimetype;
int mimetype_set;
const char *body;
int body_set;
int disable_no_store;
int disable_no_store_set;
ap_expr_info_t *loginsuccess;
int loginsuccess_set;
ap_expr_info_t *loginrequired;
int loginrequired_set;
ap_expr_info_t *logout;
int logout_set;
} auth_form_config_rec;
static void *create_auth_form_dir_config(apr_pool_t * p, char *d) {
auth_form_config_rec *conf = apr_pcalloc(p, sizeof(*conf));
conf->dir = d;
conf->authoritative = 1;
conf->form_size = HUGE_STRING_LEN;
conf->username = "httpd_username";
conf->password = "httpd_password";
conf->location = "httpd_location";
conf->method = "httpd_method";
conf->mimetype = "httpd_mimetype";
conf->body = "httpd_body";
return conf;
}
static void *merge_auth_form_dir_config(apr_pool_t * p, void *basev, void *addv) {
auth_form_config_rec *new = (auth_form_config_rec *) apr_pcalloc(p, sizeof(auth_form_config_rec));
auth_form_config_rec *add = (auth_form_config_rec *) addv;
auth_form_config_rec *base = (auth_form_config_rec *) basev;
new->providers = !add->providers ? base->providers : add->providers;
new->authoritative = (add->authoritative_set == 0) ? base->authoritative : add->authoritative;
new->authoritative_set = add->authoritative_set || base->authoritative_set;
new->site = (add->site_set == 0) ? base->site : add->site;
new->site_set = add->site_set || base->site_set;
new->username = (add->username_set == 0) ? base->username : add->username;
new->username_set = add->username_set || base->username_set;
new->password = (add->password_set == 0) ? base->password : add->password;
new->password_set = add->password_set || base->password_set;
new->location = (add->location_set == 0) ? base->location : add->location;
new->location_set = add->location_set || base->location_set;
new->form_size = (add->form_size_set == 0) ? base->form_size : add->form_size;
new->form_size_set = add->form_size_set || base->form_size_set;
new->fakebasicauth = (add->fakebasicauth_set == 0) ? base->fakebasicauth : add->fakebasicauth;
new->fakebasicauth_set = add->fakebasicauth_set || base->fakebasicauth_set;
new->method = (add->method_set == 0) ? base->method : add->method;
new->method_set = add->method_set || base->method_set;
new->mimetype = (add->mimetype_set == 0) ? base->mimetype : add->mimetype;
new->mimetype_set = add->mimetype_set || base->mimetype_set;
new->body = (add->body_set == 0) ? base->body : add->body;
new->body_set = add->body_set || base->body_set;
new->disable_no_store = (add->disable_no_store_set == 0) ? base->disable_no_store : add->disable_no_store;
new->disable_no_store_set = add->disable_no_store_set || base->disable_no_store_set;
new->loginsuccess = (add->loginsuccess_set == 0) ? base->loginsuccess : add->loginsuccess;
new->loginsuccess_set = add->loginsuccess_set || base->loginsuccess_set;
new->loginrequired = (add->loginrequired_set == 0) ? base->loginrequired : add->loginrequired;
new->loginrequired_set = add->loginrequired_set || base->loginrequired_set;
new->logout = (add->logout_set == 0) ? base->logout : add->logout;
new->logout_set = add->logout_set || base->logout_set;
return new;
}
static const char *add_authn_provider(cmd_parms * cmd, void *config,
const char *arg) {
auth_form_config_rec *conf = (auth_form_config_rec *) config;
authn_provider_list *newp;
newp = apr_pcalloc(cmd->pool, sizeof(authn_provider_list));
newp->provider_name = arg;
newp->provider = ap_lookup_provider(AUTHN_PROVIDER_GROUP,
newp->provider_name,
AUTHN_PROVIDER_VERSION);
if (newp->provider == NULL) {
return apr_psprintf(cmd->pool,
"Unknown Authn provider: %s",
newp->provider_name);
}
if (!newp->provider->check_password) {
return apr_psprintf(cmd->pool,
"The '%s' Authn provider doesn't support "
"Form Authentication", newp->provider_name);
}
if (!conf->providers) {
conf->providers = newp;
} else {
authn_provider_list *last = conf->providers;
while (last->next) {
last = last->next;
}
last->next = newp;
}
return NULL;
}
static const char *check_string(cmd_parms * cmd, const char *string) {
if (!string || !*string || ap_strchr_c(string, '=') || ap_strchr_c(string, '&')) {
return apr_pstrcat(cmd->pool, cmd->directive->directive,
" cannot be empty, or contain '=' or '&'.",
NULL);
}
return NULL;
}
static const char *set_cookie_form_location(cmd_parms * cmd, void *config, const char *location) {
auth_form_config_rec *conf = (auth_form_config_rec *) config;
conf->location = location;
conf->location_set = 1;
return check_string(cmd, location);
}
static const char *set_cookie_form_username(cmd_parms * cmd, void *config, const char *username) {
auth_form_config_rec *conf = (auth_form_config_rec *) config;
conf->username = username;
conf->username_set = 1;
return check_string(cmd, username);
}
static const char *set_cookie_form_password(cmd_parms * cmd, void *config, const char *password) {
auth_form_config_rec *conf = (auth_form_config_rec *) config;
conf->password = password;
conf->password_set = 1;
return check_string(cmd, password);
}
static const char *set_cookie_form_method(cmd_parms * cmd, void *config, const char *method) {
auth_form_config_rec *conf = (auth_form_config_rec *) config;
conf->method = method;
conf->method_set = 1;
return check_string(cmd, method);
}
static const char *set_cookie_form_mimetype(cmd_parms * cmd, void *config, const char *mimetype) {
auth_form_config_rec *conf = (auth_form_config_rec *) config;
conf->mimetype = mimetype;
conf->mimetype_set = 1;
return check_string(cmd, mimetype);
}
static const char *set_cookie_form_body(cmd_parms * cmd, void *config, const char *body) {
auth_form_config_rec *conf = (auth_form_config_rec *) config;
conf->body = body;
conf->body_set = 1;
return check_string(cmd, body);
}
static const char *set_cookie_form_size(cmd_parms * cmd, void *config,
const char *arg) {
auth_form_config_rec *conf = config;
apr_off_t size;
if (APR_SUCCESS != apr_strtoff(&size, arg, NULL, 10)
|| size < 0 || size > APR_SIZE_MAX) {
return "AuthCookieFormSize must be a size in bytes, or zero.";
}
conf->form_size = (apr_size_t)size;
conf->form_size_set = 1;
return NULL;
}
static const char *set_login_required_location(cmd_parms * cmd, void *config, const char *loginrequired) {
auth_form_config_rec *conf = (auth_form_config_rec *) config;
const char *err;
conf->loginrequired = ap_expr_parse_cmd(cmd, loginrequired, AP_EXPR_FLAG_STRING_RESULT,
&err, NULL);
if (err) {
return apr_psprintf(cmd->pool,
"Could not parse login required expression '%s': %s",
loginrequired, err);
}
conf->loginrequired_set = 1;
return NULL;
}
static const char *set_login_success_location(cmd_parms * cmd, void *config, const char *loginsuccess) {
auth_form_config_rec *conf = (auth_form_config_rec *) config;
const char *err;
conf->loginsuccess = ap_expr_parse_cmd(cmd, loginsuccess, AP_EXPR_FLAG_STRING_RESULT,
&err, NULL);
if (err) {
return apr_psprintf(cmd->pool,
"Could not parse login success expression '%s': %s",
loginsuccess, err);
}
conf->loginsuccess_set = 1;
return NULL;
}
static const char *set_logout_location(cmd_parms * cmd, void *config, const char *logout) {
auth_form_config_rec *conf = (auth_form_config_rec *) config;
const char *err;
conf->logout = ap_expr_parse_cmd(cmd, logout, AP_EXPR_FLAG_STRING_RESULT,
&err, NULL);
if (err) {
return apr_psprintf(cmd->pool,
"Could not parse logout required expression '%s': %s",
logout, err);
}
conf->logout_set = 1;
return NULL;
}
static const char *set_site_passphrase(cmd_parms * cmd, void *config, const char *site) {
auth_form_config_rec *conf = (auth_form_config_rec *) config;
conf->site = site;
conf->site_set = 1;
return NULL;
}
static const char *set_authoritative(cmd_parms * cmd, void *config, int flag) {
auth_form_config_rec *conf = (auth_form_config_rec *) config;
conf->authoritative = flag;
conf->authoritative_set = 1;
return NULL;
}
static const char *set_fake_basic_auth(cmd_parms * cmd, void *config, int flag) {
auth_form_config_rec *conf = (auth_form_config_rec *) config;
conf->fakebasicauth = flag;
conf->fakebasicauth_set = 1;
return NULL;
}
static const char *set_disable_no_store(cmd_parms * cmd, void *config, int flag) {
auth_form_config_rec *conf = (auth_form_config_rec *) config;
conf->disable_no_store = flag;
conf->disable_no_store_set = 1;
return NULL;
}
static const command_rec auth_form_cmds[] = {
AP_INIT_ITERATE("AuthFormProvider", add_authn_provider, NULL, OR_AUTHCFG,
"specify the auth providers for a directory or location"),
AP_INIT_TAKE1("AuthFormUsername", set_cookie_form_username, NULL, OR_AUTHCFG,
"The field of the login form carrying the username"),
AP_INIT_TAKE1("AuthFormPassword", set_cookie_form_password, NULL, OR_AUTHCFG,
"The field of the login form carrying the password"),
AP_INIT_TAKE1("AuthFormLocation", set_cookie_form_location, NULL, OR_AUTHCFG,
"The field of the login form carrying the URL to redirect on "
"successful login."),
AP_INIT_TAKE1("AuthFormMethod", set_cookie_form_method, NULL, OR_AUTHCFG,
"The field of the login form carrying the original request method."),
AP_INIT_TAKE1("AuthFormMimetype", set_cookie_form_mimetype, NULL, OR_AUTHCFG,
"The field of the login form carrying the original request mimetype."),
AP_INIT_TAKE1("AuthFormBody", set_cookie_form_body, NULL, OR_AUTHCFG,
"The field of the login form carrying the urlencoded original request "
"body."),
AP_INIT_TAKE1("AuthFormSize", set_cookie_form_size, NULL, ACCESS_CONF,
"Maximum size of body parsed by the form parser"),
AP_INIT_TAKE1("AuthFormLoginRequiredLocation", set_login_required_location,
NULL, OR_AUTHCFG,
"If set, redirect the browser to this URL rather than "
"return 401 Not Authorized."),
AP_INIT_TAKE1("AuthFormLoginSuccessLocation", set_login_success_location,
NULL, OR_AUTHCFG,
"If set, redirect the browser to this URL when a login "
"processed by the login handler is successful."),
AP_INIT_TAKE1("AuthFormLogoutLocation", set_logout_location,
NULL, OR_AUTHCFG,
"The URL of the logout successful page. An attempt to access an "
"URL handled by the handler " FORM_LOGOUT_HANDLER " will result "
"in an redirect to this page after logout."),
AP_INIT_TAKE1("AuthFormSitePassphrase", set_site_passphrase,
NULL, OR_AUTHCFG,
"If set, use this passphrase to determine whether the user should "
"be authenticated. Bypasses the user authentication check on "
"every website hit, and is useful for high traffic sites."),
AP_INIT_FLAG("AuthFormAuthoritative", set_authoritative,
NULL, OR_AUTHCFG,
"Set to 'Off' to allow access control to be passed along to "
"lower modules if the UserID is not known to this module"),
AP_INIT_FLAG("AuthFormFakeBasicAuth", set_fake_basic_auth,
NULL, OR_AUTHCFG,
"Set to 'On' to pass through authentication to the rest of the "
"server as a basic authentication header."),
AP_INIT_FLAG("AuthFormDisableNoStore", set_disable_no_store,
NULL, OR_AUTHCFG,
"Set to 'on' to stop the sending of a Cache-Control no-store header with "
"the login screen. This allows the browser to cache the credentials, but "
"at the risk of it being possible for the login form to be resubmitted "
"and revealed to the backend server through XSS. Use at own risk."),
{NULL}
};
module AP_MODULE_DECLARE_DATA auth_form_module;
static void note_cookie_auth_failure(request_rec * r) {
auth_form_config_rec *conf = ap_get_module_config(r->per_dir_config,
&auth_form_module);
if (conf->location && ap_strchr_c(conf->location, ':')) {
apr_table_setn(r->err_headers_out, "Location", conf->location);
}
}
static int hook_note_cookie_auth_failure(request_rec * r,
const char *auth_type) {
if (strcasecmp(auth_type, "form"))
return DECLINED;
note_cookie_auth_failure(r);
return OK;
}
static void set_notes_auth(request_rec * r,
const char *user, const char *pw,
const char *method, const char *mimetype) {
apr_table_t *notes = NULL;
const char *authname;
while (r->main) {
r = r->main;
}
while (r->prev) {
r = r->prev;
}
notes = r->notes;
authname = ap_auth_name(r);
if (user) {
apr_table_setn(notes, apr_pstrcat(r->pool, authname, "-user", NULL), user);
}
if (pw) {
apr_table_setn(notes, apr_pstrcat(r->pool, authname, "-pw", NULL), pw);
}
if (method) {
apr_table_setn(notes, apr_pstrcat(r->pool, authname, "-method", NULL), method);
}
if (mimetype) {
apr_table_setn(notes, apr_pstrcat(r->pool, authname, "-mimetype", NULL), mimetype);
}
}
static void get_notes_auth(request_rec *r,
const char **user, const char **pw,
const char **method, const char **mimetype) {
const char *authname;
request_rec *m = r;
while (m->main) {
m = m->main;
}
while (m->prev) {
m = m->prev;
}
authname = ap_auth_name(m);
if (user) {
*user = (char *) apr_table_get(m->notes, apr_pstrcat(m->pool, authname, "-user", NULL));
}
if (pw) {
*pw = (char *) apr_table_get(m->notes, apr_pstrcat(m->pool, authname, "-pw", NULL));
}
if (method) {
*method = (char *) apr_table_get(m->notes, apr_pstrcat(m->pool, authname, "-method", NULL));
}
if (mimetype) {
*mimetype = (char *) apr_table_get(m->notes, apr_pstrcat(m->pool, authname, "-mimetype", NULL));
}
if (user && *user) {
r->user = (char *) *user;
}
ap_log_rerror(APLOG_MARK, APLOG_TRACE1, 0, r,
"from notes: user: %s, pw: %s, method: %s, mimetype: %s",
user ? *user : "<null>", pw ? *pw : "<null>",
method ? *method : "<null>", mimetype ? *mimetype : "<null>");
}
static apr_status_t set_session_auth(request_rec * r,
const char *user, const char *pw, const char *site) {
const char *hash = NULL;
const char *authname = ap_auth_name(r);
session_rec *z = NULL;
if (site) {
hash = ap_md5(r->pool,
(unsigned char *) apr_pstrcat(r->pool, user, ":", site, NULL));
}
ap_session_load_fn(r, &z);
ap_session_set_fn(r, z, apr_pstrcat(r->pool, authname, "-" MOD_SESSION_USER, NULL), user);
ap_session_set_fn(r, z, apr_pstrcat(r->pool, authname, "-" MOD_SESSION_PW, NULL), pw);
ap_session_set_fn(r, z, apr_pstrcat(r->pool, authname, "-" MOD_AUTH_FORM_HASH, NULL), hash);
return APR_SUCCESS;
}
static apr_status_t get_session_auth(request_rec * r,
const char **user, const char **pw, const char **hash) {
const char *authname = ap_auth_name(r);
session_rec *z = NULL;
ap_session_load_fn(r, &z);
if (user) {
ap_session_get_fn(r, z, apr_pstrcat(r->pool, authname, "-" MOD_SESSION_USER, NULL), user);
}
if (pw) {
ap_session_get_fn(r, z, apr_pstrcat(r->pool, authname, "-" MOD_SESSION_PW, NULL), pw);
}
if (hash) {
ap_session_get_fn(r, z, apr_pstrcat(r->pool, authname, "-" MOD_AUTH_FORM_HASH, NULL), hash);
}
if (user && *user) {
r->user = (char *) *user;
}
ap_log_rerror(APLOG_MARK, APLOG_TRACE1, 0, r,
"from session: " MOD_SESSION_USER ": %s, " MOD_SESSION_PW
": %s, " MOD_AUTH_FORM_HASH ": %s",
user ? *user : "<null>", pw ? *pw : "<null>",
hash ? *hash : "<null>");
return APR_SUCCESS;
}
static int get_form_auth(request_rec * r,
const char *username,
const char *password,
const char *location,
const char *method,
const char *mimetype,
const char *body,
const char **sent_user,
const char **sent_pw,
const char **sent_loc,
const char **sent_method,
const char **sent_mimetype,
apr_bucket_brigade **sent_body,
auth_form_config_rec * conf) {
apr_array_header_t *pairs = NULL;
apr_off_t len;
apr_size_t size;
int res;
char *buffer;
get_notes_auth(r, sent_user, sent_pw, sent_method, sent_mimetype);
if (*sent_user && *sent_pw) {
return OK;
}
res = ap_parse_form_data(r, NULL, &pairs, -1, conf->form_size);
if (res != OK) {
return res;
}
while (pairs && !apr_is_empty_array(pairs)) {
ap_form_pair_t *pair = (ap_form_pair_t *) apr_array_pop(pairs);
if (username && !strcmp(pair->name, username) && sent_user) {
apr_brigade_length(pair->value, 1, &len);
size = (apr_size_t) len;
buffer = apr_palloc(r->pool, size + 1);
apr_brigade_flatten(pair->value, buffer, &size);
buffer[len] = 0;
*sent_user = buffer;
} else if (password && !strcmp(pair->name, password) && sent_pw) {
apr_brigade_length(pair->value, 1, &len);
size = (apr_size_t) len;
buffer = apr_palloc(r->pool, size + 1);
apr_brigade_flatten(pair->value, buffer, &size);
buffer[len] = 0;
*sent_pw = buffer;
} else if (location && !strcmp(pair->name, location) && sent_loc) {
apr_brigade_length(pair->value, 1, &len);
size = (apr_size_t) len;
buffer = apr_palloc(r->pool, size + 1);
apr_brigade_flatten(pair->value, buffer, &size);
buffer[len] = 0;
*sent_loc = buffer;
} else if (method && !strcmp(pair->name, method) && sent_method) {
apr_brigade_length(pair->value, 1, &len);
size = (apr_size_t) len;
buffer = apr_palloc(r->pool, size + 1);
apr_brigade_flatten(pair->value, buffer, &size);
buffer[len] = 0;
*sent_method = buffer;
} else if (mimetype && !strcmp(pair->name, mimetype) && sent_mimetype) {
apr_brigade_length(pair->value, 1, &len);
size = (apr_size_t) len;
buffer = apr_palloc(r->pool, size + 1);
apr_brigade_flatten(pair->value, buffer, &size);
buffer[len] = 0;
*sent_mimetype = buffer;
} else if (body && !strcmp(pair->name, body) && sent_body) {
*sent_body = pair->value;
}
}
ap_log_rerror(APLOG_MARK, APLOG_TRACE1, 0, r,
"from form: user: %s, pw: %s, method: %s, mimetype: %s, location: %s",
sent_user ? *sent_user : "<null>", sent_pw ? *sent_pw : "<null>",
sent_method ? *sent_method : "<null>",
sent_mimetype ? *sent_mimetype : "<null>",
sent_loc ? *sent_loc : "<null>");
if (sent_user && *sent_user) {
r->user = (char *) *sent_user;
}
if (!sent_user || !*sent_user) {
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(02982)
"form parsed, but username field '%s' was missing or empty, unauthorized",
username);
return HTTP_UNAUTHORIZED;
}
if (!sent_pw || !*sent_pw) {
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(02983)
"form parsed, but password field '%s' was missing or empty, unauthorized",
password);
return HTTP_UNAUTHORIZED;
}
set_notes_auth(r, *sent_user, *sent_pw, sent_method ? *sent_method : NULL,
sent_mimetype ? *sent_mimetype : NULL);
return OK;
}
static int check_site(request_rec * r, const char *site, const char *sent_user, const char *sent_hash) {
if (site && sent_user && sent_hash) {
const char *hash = ap_md5(r->pool,
(unsigned char *) apr_pstrcat(r->pool, sent_user, ":", site, NULL));
if (!strcmp(sent_hash, hash)) {
return OK;
} else {
return AUTH_USER_NOT_FOUND;
}
}
return DECLINED;
}
static int check_authn(request_rec * r, const char *sent_user, const char *sent_pw) {
authn_status auth_result;
authn_provider_list *current_provider;
auth_form_config_rec *conf = ap_get_module_config(r->per_dir_config,
&auth_form_module);
current_provider = conf->providers;
do {
const authn_provider *provider;
if (!current_provider) {
provider = ap_lookup_provider(AUTHN_PROVIDER_GROUP,
AUTHN_DEFAULT_PROVIDER,
AUTHN_PROVIDER_VERSION);
if (!provider || !provider->check_password) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(01806)
"no authn provider configured");
auth_result = AUTH_GENERAL_ERROR;
break;
}
apr_table_setn(r->notes, AUTHN_PROVIDER_NAME_NOTE, AUTHN_DEFAULT_PROVIDER);
} else {
provider = current_provider->provider;
apr_table_setn(r->notes, AUTHN_PROVIDER_NAME_NOTE, current_provider->provider_name);
}
if (!sent_user || !sent_pw) {
auth_result = AUTH_USER_NOT_FOUND;
break;
}
auth_result = provider->check_password(r, sent_user, sent_pw);
apr_table_unset(r->notes, AUTHN_PROVIDER_NAME_NOTE);
if (auth_result != AUTH_USER_NOT_FOUND) {
break;
}
if (!conf->providers) {
break;
}
current_provider = current_provider->next;
} while (current_provider);
if (auth_result != AUTH_GRANTED) {
int return_code;
if (!(conf->authoritative) && auth_result != AUTH_DENIED) {
return DECLINED;
}
switch (auth_result) {
case AUTH_DENIED:
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(01807)
"user '%s': authentication failure for \"%s\": "
"password Mismatch",
sent_user, r->uri);
return_code = HTTP_UNAUTHORIZED;
break;
case AUTH_USER_NOT_FOUND:
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(01808)
"user '%s' not found: %s", sent_user, r->uri);
return_code = HTTP_UNAUTHORIZED;
break;
case AUTH_GENERAL_ERROR:
default:
return_code = HTTP_INTERNAL_SERVER_ERROR;
break;
}
if (return_code == HTTP_UNAUTHORIZED) {
note_cookie_auth_failure(r);
}
return return_code;
}
return OK;
}
static void fake_basic_authentication(request_rec *r, auth_form_config_rec *conf,
const char *user, const char *pw) {
if (conf->fakebasicauth) {
char *basic = apr_pstrcat(r->pool, user, ":", pw, NULL);
apr_size_t size = (apr_size_t) strlen(basic);
char *base64 = apr_palloc(r->pool,
apr_base64_encode_len(size + 1) * sizeof(char));
apr_base64_encode(base64, basic, size);
apr_table_setn(r->headers_in, "Authorization",
apr_pstrcat(r->pool, "Basic ", base64, NULL));
}
}
static int authenticate_form_authn(request_rec * r) {
auth_form_config_rec *conf = ap_get_module_config(r->per_dir_config,
&auth_form_module);
const char *sent_user = NULL, *sent_pw = NULL, *sent_hash = NULL;
const char *sent_loc = NULL, *sent_method = "GET", *sent_mimetype = NULL;
const char *current_auth = NULL;
const char *err;
apr_status_t res;
int rv = HTTP_UNAUTHORIZED;
current_auth = ap_auth_type(r);
if (!current_auth || strcasecmp(current_auth, "form")) {
return DECLINED;
}
if (PROXYREQ_PROXY == r->proxyreq) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(01809)
"form auth cannot be used for proxy "
"requests due to XSS risk, access denied: %s", r->uri);
return HTTP_INTERNAL_SERVER_ERROR;
}
if (!ap_auth_name(r)) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(01810)
"need AuthName: %s", r->uri);
return HTTP_INTERNAL_SERVER_ERROR;
}
r->ap_auth_type = (char *) current_auth;
get_notes_auth(r, &sent_user, &sent_pw, &sent_method, &sent_mimetype);
if (!sent_user || !sent_pw || !*sent_user || !*sent_pw) {
res = get_session_auth(r, &sent_user, &sent_pw, &sent_hash);
} else {
res = APR_SUCCESS;
}
if (APR_SUCCESS == res && sent_user && sent_hash && sent_pw) {
rv = check_site(r, conf->site, sent_user, sent_hash);
if (OK == rv) {
fake_basic_authentication(r, conf, sent_user, sent_pw);
return OK;
}
}
if (APR_SUCCESS == res && sent_user && sent_pw) {
rv = check_authn(r, sent_user, sent_pw);
if (OK == rv) {
fake_basic_authentication(r, conf, sent_user, sent_pw);
return OK;
}
}
if (HTTP_UNAUTHORIZED == rv && r->method_number == M_POST && ap_is_initial_req(r)) {
request_rec *rr;
apr_bucket_brigade *sent_body = NULL;
rr = ap_sub_req_lookup_uri(r->uri, r, r->input_filters);
rr->headers_in = r->headers_in;
ap_run_insert_filter(rr);
rv = get_form_auth(rr, conf->username, conf->password, conf->location,
conf->method, conf->mimetype, conf->body,
&sent_user, &sent_pw, &sent_loc, &sent_method,
&sent_mimetype, &sent_body, conf);
r->user = apr_pstrdup(r->pool, rr->user);
if (sent_body && sent_mimetype) {
apr_table_set(r->headers_in, "Content-Type", sent_mimetype);
r->kept_body = sent_body;
} else {
r->kept_body = apr_brigade_create(r->pool, r->connection->bucket_alloc);
}
ap_request_insert_filter_fn(r);
if (sent_method && strcmp(r->method, sent_method)) {
r->handler = FORM_REDIRECT_HANDLER;
}
if (OK == rv) {
rv = check_authn(r, sent_user, sent_pw);
if (OK == rv) {
fake_basic_authentication(r, conf, sent_user, sent_pw);
set_session_auth(r, sent_user, sent_pw, conf->site);
if (sent_loc) {
apr_table_set(r->headers_out, "Location", sent_loc);
return HTTP_MOVED_TEMPORARILY;
}
if (conf->loginsuccess) {
const char *loginsuccess = ap_expr_str_exec(r,
conf->loginsuccess, &err);
if (!err) {
apr_table_set(r->headers_out, "Location", loginsuccess);
return HTTP_MOVED_TEMPORARILY;
} else {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(02339)
"Can't evaluate login success expression: %s", err);
return HTTP_INTERNAL_SERVER_ERROR;
}
}
}
}
}
if (HTTP_UNAUTHORIZED == rv && conf->loginrequired) {
const char *loginrequired = ap_expr_str_exec(r,
conf->loginrequired, &err);
if (!err) {
apr_table_set(r->headers_out, "Location", loginrequired);
return HTTP_MOVED_TEMPORARILY;
} else {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(02340)
"Can't evaluate login required expression: %s", err);
return HTTP_INTERNAL_SERVER_ERROR;
}
}
if (sent_loc) {
apr_table_set(r->headers_out, "Location", sent_loc);
rv = HTTP_MOVED_TEMPORARILY;
}
if (HTTP_UNAUTHORIZED == rv && !conf->disable_no_store) {
apr_table_addn(r->headers_out, "Cache-Control", "no-store");
apr_table_addn(r->err_headers_out, "Cache-Control", "no-store");
}
return rv;
}
static int authenticate_form_login_handler(request_rec * r) {
auth_form_config_rec *conf;
const char *err;
const char *sent_user = NULL, *sent_pw = NULL, *sent_loc = NULL;
int rv;
if (strcmp(r->handler, FORM_LOGIN_HANDLER)) {
return DECLINED;
}
if (r->method_number != M_POST) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(01811)
"the " FORM_LOGIN_HANDLER " only supports the POST method for %s",
r->uri);
return HTTP_METHOD_NOT_ALLOWED;
}
conf = ap_get_module_config(r->per_dir_config, &auth_form_module);
rv = get_form_auth(r, conf->username, conf->password, conf->location,
NULL, NULL, NULL,
&sent_user, &sent_pw, &sent_loc,
NULL, NULL, NULL, conf);
if (OK == rv) {
rv = check_authn(r, sent_user, sent_pw);
if (OK == rv) {
set_session_auth(r, sent_user, sent_pw, conf->site);
if (sent_loc) {
apr_table_set(r->headers_out, "Location", sent_loc);
return HTTP_MOVED_TEMPORARILY;
}
if (conf->loginsuccess) {
const char *loginsuccess = ap_expr_str_exec(r,
conf->loginsuccess, &err);
if (!err) {
apr_table_set(r->headers_out, "Location", loginsuccess);
return HTTP_MOVED_TEMPORARILY;
} else {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(02341)
"Can't evaluate login success expression: %s", err);
return HTTP_INTERNAL_SERVER_ERROR;
}
}
return HTTP_OK;
}
}
if (HTTP_UNAUTHORIZED == rv && conf->loginrequired) {
const char *loginrequired = ap_expr_str_exec(r,
conf->loginrequired, &err);
if (!err) {
apr_table_set(r->headers_out, "Location", loginrequired);
return HTTP_MOVED_TEMPORARILY;
} else {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(02342)
"Can't evaluate login required expression: %s", err);
return HTTP_INTERNAL_SERVER_ERROR;
}
}
return rv;
}
static int authenticate_form_logout_handler(request_rec * r) {
auth_form_config_rec *conf;
const char *err;
if (strcmp(r->handler, FORM_LOGOUT_HANDLER)) {
return DECLINED;
}
conf = ap_get_module_config(r->per_dir_config, &auth_form_module);
set_session_auth(r, NULL, NULL, NULL);
apr_table_addn(r->headers_out, "Cache-Control", "no-store");
apr_table_addn(r->err_headers_out, "Cache-Control", "no-store");
if (conf->logout) {
const char *logout = ap_expr_str_exec(r,
conf->logout, &err);
if (!err) {
apr_table_addn(r->headers_out, "Location", logout);
return HTTP_TEMPORARY_REDIRECT;
} else {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(02343)
"Can't evaluate logout expression: %s", err);
return HTTP_INTERNAL_SERVER_ERROR;
}
}
return HTTP_OK;
}
static int authenticate_form_redirect_handler(request_rec * r) {
request_rec *rr = NULL;
const char *sent_method = NULL, *sent_mimetype = NULL;
if (strcmp(r->handler, FORM_REDIRECT_HANDLER)) {
return DECLINED;
}
get_notes_auth(r, NULL, NULL, &sent_method, &sent_mimetype);
if (r->kept_body && sent_method && sent_mimetype) {
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(01812)
"internal redirect to method '%s' and body mimetype '%s' for the "
"uri: %s", sent_method, sent_mimetype, r->uri);
rr = ap_sub_req_method_uri(sent_method, r->uri, r, r->output_filters);
r->status = ap_run_sub_req(rr);
} else {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(01813)
"internal redirect requested but one or all of method, mimetype or "
"body are NULL: %s", r->uri);
return HTTP_INTERNAL_SERVER_ERROR;
}
return r->status == HTTP_OK || r->status == OK ? OK : r->status;
}
static int authenticate_form_post_config(apr_pool_t *pconf, apr_pool_t *plog,
apr_pool_t *ptemp, server_rec *s) {
if (!ap_session_load_fn || !ap_session_get_fn || !ap_session_set_fn) {
ap_session_load_fn = APR_RETRIEVE_OPTIONAL_FN(ap_session_load);
ap_session_get_fn = APR_RETRIEVE_OPTIONAL_FN(ap_session_get);
ap_session_set_fn = APR_RETRIEVE_OPTIONAL_FN(ap_session_set);
if (!ap_session_load_fn || !ap_session_get_fn || !ap_session_set_fn) {
ap_log_error(APLOG_MARK, APLOG_CRIT, 0, NULL, APLOGNO(02617)
"You must load mod_session to enable the mod_auth_form "
"functions");
return !OK;
}
}
if (!ap_request_insert_filter_fn || !ap_request_remove_filter_fn) {
ap_request_insert_filter_fn = APR_RETRIEVE_OPTIONAL_FN(ap_request_insert_filter);
ap_request_remove_filter_fn = APR_RETRIEVE_OPTIONAL_FN(ap_request_remove_filter);
if (!ap_request_insert_filter_fn || !ap_request_remove_filter_fn) {
ap_log_error(APLOG_MARK, APLOG_CRIT, 0, NULL, APLOGNO(02618)
"You must load mod_request to enable the mod_auth_form "
"functions");
return !OK;
}
}
return OK;
}
static void register_hooks(apr_pool_t * p) {
ap_hook_post_config(authenticate_form_post_config,NULL,NULL,APR_HOOK_MIDDLE);
#if AP_MODULE_MAGIC_AT_LEAST(20080403,1)
ap_hook_check_authn(authenticate_form_authn, NULL, NULL, APR_HOOK_MIDDLE,
AP_AUTH_INTERNAL_PER_CONF);
#else
ap_hook_check_user_id(authenticate_form_authn, NULL, NULL, APR_HOOK_MIDDLE);
#endif
ap_hook_handler(authenticate_form_login_handler, NULL, NULL, APR_HOOK_MIDDLE);
ap_hook_handler(authenticate_form_logout_handler, NULL, NULL, APR_HOOK_MIDDLE);
ap_hook_handler(authenticate_form_redirect_handler, NULL, NULL, APR_HOOK_MIDDLE);
ap_hook_note_auth_failure(hook_note_cookie_auth_failure, NULL, NULL,
APR_HOOK_MIDDLE);
}
AP_DECLARE_MODULE(auth_form) = {
STANDARD20_MODULE_STUFF,
create_auth_form_dir_config,
merge_auth_form_dir_config,
NULL,
NULL,
auth_form_cmds,
register_hooks
};