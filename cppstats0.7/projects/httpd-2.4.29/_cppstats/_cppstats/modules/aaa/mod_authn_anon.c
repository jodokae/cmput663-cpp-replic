#include "apr_strings.h"
#define APR_WANT_STRFUNC
#include "apr_want.h"
#include "ap_provider.h"
#include "httpd.h"
#include "http_config.h"
#include "http_core.h"
#include "http_log.h"
#include "http_request.h"
#include "http_protocol.h"
#include "mod_auth.h"
typedef struct anon_auth_user {
const char *user;
struct anon_auth_user *next;
} anon_auth_user;
typedef struct {
anon_auth_user *users;
int nouserid;
int logemail;
int verifyemail;
int mustemail;
int anyuserid;
} authn_anon_config_rec;
static void *create_authn_anon_dir_config(apr_pool_t *p, char *d) {
authn_anon_config_rec *conf = apr_palloc(p, sizeof(*conf));
conf->users = NULL;
conf->nouserid = 0;
conf->anyuserid = 0;
conf->logemail = 1;
conf->verifyemail = 0;
conf->mustemail = 1;
return conf;
}
static const char *anon_set_string_slots(cmd_parms *cmd,
void *my_config, const char *arg) {
authn_anon_config_rec *conf = my_config;
anon_auth_user *first;
if (!*arg) {
return "Anonymous string cannot be empty, use Anonymous_NoUserId";
}
if (!conf->anyuserid) {
if (!strcmp(arg, "*")) {
conf->anyuserid = 1;
} else {
first = conf->users;
conf->users = apr_palloc(cmd->pool, sizeof(*conf->users));
conf->users->user = arg;
conf->users->next = first;
}
}
return NULL;
}
static const command_rec authn_anon_cmds[] = {
AP_INIT_ITERATE("Anonymous", anon_set_string_slots, NULL, OR_AUTHCFG,
"a space-separated list of user IDs"),
AP_INIT_FLAG("Anonymous_MustGiveEmail", ap_set_flag_slot,
(void *)APR_OFFSETOF(authn_anon_config_rec, mustemail),
OR_AUTHCFG, "Limited to 'on' or 'off'"),
AP_INIT_FLAG("Anonymous_NoUserId", ap_set_flag_slot,
(void *)APR_OFFSETOF(authn_anon_config_rec, nouserid),
OR_AUTHCFG, "Limited to 'on' or 'off'"),
AP_INIT_FLAG("Anonymous_VerifyEmail", ap_set_flag_slot,
(void *)APR_OFFSETOF(authn_anon_config_rec, verifyemail),
OR_AUTHCFG, "Limited to 'on' or 'off'"),
AP_INIT_FLAG("Anonymous_LogEmail", ap_set_flag_slot,
(void *)APR_OFFSETOF(authn_anon_config_rec, logemail),
OR_AUTHCFG, "Limited to 'on' or 'off'"),
{NULL}
};
module AP_MODULE_DECLARE_DATA authn_anon_module;
static authn_status check_anonymous(request_rec *r, const char *user,
const char *sent_pw) {
authn_anon_config_rec *conf = ap_get_module_config(r->per_dir_config,
&authn_anon_module);
authn_status res = AUTH_USER_NOT_FOUND;
if (!conf->users && !conf->anyuserid) {
return AUTH_USER_NOT_FOUND;
}
if (!*user) {
if (conf->nouserid) {
res = AUTH_USER_FOUND;
}
} else if (conf->anyuserid) {
res = AUTH_USER_FOUND;
} else {
anon_auth_user *p = conf->users;
while (p) {
if (!strcasecmp(user, p->user)) {
res = AUTH_USER_FOUND;
break;
}
p = p->next;
}
}
if ( (res == AUTH_USER_FOUND)
&& (!conf->mustemail || *sent_pw)
&& ( !conf->verifyemail
|| (ap_strchr_c(sent_pw, '@') && ap_strchr_c(sent_pw, '.')))) {
if (conf->logemail && ap_is_initial_req(r)) {
ap_log_rerror(APLOG_MARK, APLOG_INFO, APR_SUCCESS, r, APLOGNO(01672)
"Anonymous: Passwd <%s> Accepted",
sent_pw ? sent_pw : "\'none\'");
}
return AUTH_GRANTED;
}
return (res == AUTH_USER_NOT_FOUND ? res : AUTH_DENIED);
}
static const authn_provider authn_anon_provider = {
&check_anonymous,
NULL
};
static void register_hooks(apr_pool_t *p) {
ap_register_auth_provider(p, AUTHN_PROVIDER_GROUP, "anon",
AUTHN_PROVIDER_VERSION,
&authn_anon_provider, AP_AUTH_INTERNAL_PER_CONF);
}
AP_DECLARE_MODULE(authn_anon) = {
STANDARD20_MODULE_STUFF,
create_authn_anon_dir_config,
NULL,
NULL,
NULL,
authn_anon_cmds,
register_hooks
};