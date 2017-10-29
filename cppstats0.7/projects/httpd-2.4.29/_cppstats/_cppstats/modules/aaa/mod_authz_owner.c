#include "apr_strings.h"
#include "apr_file_info.h"
#include "apr_user.h"
#include "ap_config.h"
#include "ap_provider.h"
#include "httpd.h"
#include "http_config.h"
#include "http_core.h"
#include "http_log.h"
#include "http_protocol.h"
#include "http_request.h"
#include "mod_auth.h"
#include "mod_authz_owner.h"
static const command_rec authz_owner_cmds[] = {
{NULL}
};
module AP_MODULE_DECLARE_DATA authz_owner_module;
static authz_status fileowner_check_authorization(request_rec *r,
const char *require_args,
const void *parsed_require_args) {
char *reason = NULL;
apr_status_t status = 0;
#if !APR_HAS_USER
reason = "'Require file-owner' is not supported on this platform.";
ap_log_rerror(APLOG_MARK, APLOG_ERR, status, r, APLOGNO(01632)
"Authorization of user %s to access %s failed, reason: %s",
r->user, r->uri, reason ? reason : "unknown");
return AUTHZ_DENIED;
#else
char *owner = NULL;
apr_finfo_t finfo;
if (!r->user) {
return AUTHZ_DENIED_NO_USER;
}
if (!r->filename) {
reason = "no filename available";
ap_log_rerror(APLOG_MARK, APLOG_ERR, status, r, APLOGNO(01633)
"Authorization of user %s to access %s failed, reason: %s",
r->user, r->uri, reason ? reason : "unknown");
return AUTHZ_DENIED;
}
status = apr_stat(&finfo, r->filename, APR_FINFO_USER, r->pool);
if (status != APR_SUCCESS) {
reason = apr_pstrcat(r->pool, "could not stat file ",
r->filename, NULL);
ap_log_rerror(APLOG_MARK, APLOG_ERR, status, r, APLOGNO(01634)
"Authorization of user %s to access %s failed, reason: %s",
r->user, r->uri, reason ? reason : "unknown");
return AUTHZ_DENIED;
}
if (!(finfo.valid & APR_FINFO_USER)) {
reason = "no file owner information available";
ap_log_rerror(APLOG_MARK, APLOG_ERR, status, r, APLOGNO(01635)
"Authorization of user %s to access %s failed, reason: %s",
r->user, r->uri, reason ? reason : "unknown");
return AUTHZ_DENIED;
}
status = apr_uid_name_get(&owner, finfo.user, r->pool);
if (status != APR_SUCCESS || !owner) {
reason = "could not get name of file owner";
ap_log_rerror(APLOG_MARK, APLOG_ERR, status, r, APLOGNO(01636)
"Authorization of user %s to access %s failed, reason: %s",
r->user, r->uri, reason ? reason : "unknown");
return AUTHZ_DENIED;
}
if (strcmp(owner, r->user)) {
reason = apr_psprintf(r->pool, "file owner %s does not match.",
owner);
ap_log_rerror(APLOG_MARK, APLOG_ERR, status, r, APLOGNO(01637)
"Authorization of user %s to access %s failed, reason: %s",
r->user, r->uri, reason ? reason : "unknown");
return AUTHZ_DENIED;
}
return AUTHZ_GRANTED;
#endif
}
static char *authz_owner_get_file_group(request_rec *r) {
#if !APR_HAS_USER
return NULL;
#else
char *reason = NULL;
char *group = NULL;
apr_finfo_t finfo;
apr_status_t status = 0;
if (!r->filename) {
reason = "no filename available";
ap_log_rerror(APLOG_MARK, APLOG_ERR, status, r, APLOGNO(01638)
"Authorization of user %s to access %s failed, reason: %s",
r->user, r->uri, reason ? reason : "unknown");
return NULL;
}
status = apr_stat(&finfo, r->filename, APR_FINFO_GROUP, r->pool);
if (status != APR_SUCCESS) {
reason = apr_pstrcat(r->pool, "could not stat file ",
r->filename, NULL);
ap_log_rerror(APLOG_MARK, APLOG_ERR, status, r, APLOGNO(01639)
"Authorization of user %s to access %s failed, reason: %s",
r->user, r->uri, reason ? reason : "unknown");
return NULL;
}
if (!(finfo.valid & APR_FINFO_GROUP)) {
reason = "no file group information available";
ap_log_rerror(APLOG_MARK, APLOG_ERR, status, r, APLOGNO(01640)
"Authorization of user %s to access %s failed, reason: %s",
r->user, r->uri, reason ? reason : "unknown");
return NULL;
}
status = apr_gid_name_get(&group, finfo.group, r->pool);
if (status != APR_SUCCESS || !group) {
reason = "could not get name of file group";
ap_log_rerror(APLOG_MARK, APLOG_ERR, status, r, APLOGNO(01641)
"Authorization of user %s to access %s failed, reason: %s",
r->user, r->uri, reason ? reason : "unknown");
return NULL;
}
return group;
#endif
}
static const authz_provider authz_fileowner_provider = {
&fileowner_check_authorization,
NULL,
};
static void register_hooks(apr_pool_t *p) {
APR_REGISTER_OPTIONAL_FN(authz_owner_get_file_group);
ap_register_auth_provider(p, AUTHZ_PROVIDER_GROUP, "file-owner",
AUTHZ_PROVIDER_VERSION,
&authz_fileowner_provider,
AP_AUTH_INTERNAL_PER_CONF);
}
AP_DECLARE_MODULE(authz_owner) = {
STANDARD20_MODULE_STUFF,
NULL,
NULL,
NULL,
NULL,
authz_owner_cmds,
register_hooks
};