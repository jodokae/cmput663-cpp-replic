#include "apr_strings.h"
#include "apr_user.h"
#define APR_WANT_STRFUNC
#include "apr_want.h"
#if APR_HAVE_UNISTD_H
#include <unistd.h>
#endif
#include "ap_config.h"
#include "httpd.h"
#include "http_config.h"
#include "http_request.h"
#if !defined(WIN32) && !defined(OS2) && !defined(NETWARE)
#define HAVE_UNIX_SUEXEC
#endif
#if defined(HAVE_UNIX_SUEXEC)
#include "unixd.h"
#endif
#if !defined(DEFAULT_USER_DIR)
#define DEFAULT_USER_DIR NULL
#endif
#define O_DEFAULT 0
#define O_ENABLE 1
#define O_DISABLE 2
module AP_MODULE_DECLARE_DATA userdir_module;
typedef struct {
int globally_disabled;
const char *userdir;
apr_table_t *enabled_users;
apr_table_t *disabled_users;
} userdir_config;
static void *create_userdir_config(apr_pool_t *p, server_rec *s) {
userdir_config *newcfg = apr_pcalloc(p, sizeof(*newcfg));
newcfg->globally_disabled = O_DEFAULT;
newcfg->userdir = DEFAULT_USER_DIR;
newcfg->enabled_users = apr_table_make(p, 4);
newcfg->disabled_users = apr_table_make(p, 4);
return newcfg;
}
static void *merge_userdir_config(apr_pool_t *p, void *basev, void *overridesv) {
userdir_config *cfg = apr_pcalloc(p, sizeof(userdir_config));
userdir_config *base = basev, *overrides = overridesv;
cfg->globally_disabled = (overrides->globally_disabled != O_DEFAULT) ?
overrides->globally_disabled :
base->globally_disabled;
cfg->userdir = (overrides->userdir != DEFAULT_USER_DIR) ?
overrides->userdir : base->userdir;
cfg->enabled_users = overrides->enabled_users;
cfg->disabled_users = overrides->disabled_users;
return cfg;
}
static const char *set_user_dir(cmd_parms *cmd, void *dummy, const char *arg) {
userdir_config *s_cfg = ap_get_module_config(cmd->server->module_config,
&userdir_module);
char *username;
const char *usernames = arg;
char *kw = ap_getword_conf(cmd->temp_pool, &usernames);
apr_table_t *usertable;
if (*kw == '\0') {
return "UserDir requires an argument.";
}
if ((!strcasecmp(kw, "disable")) || (!strcasecmp(kw, "disabled"))) {
if (!*usernames) {
s_cfg->globally_disabled = O_DISABLE;
return NULL;
}
usertable = s_cfg->disabled_users;
} else if ((!strcasecmp(kw, "enable")) || (!strcasecmp(kw, "enabled"))) {
if (!*usernames) {
s_cfg->globally_disabled = O_ENABLE;
return NULL;
}
usertable = s_cfg->enabled_users;
} else {
s_cfg->userdir = arg;
return NULL;
}
while (*usernames) {
username = ap_getword_conf(cmd->pool, &usernames);
apr_table_setn(usertable, username, "1");
}
return NULL;
}
static const command_rec userdir_cmds[] = {
AP_INIT_RAW_ARGS("UserDir", set_user_dir, NULL, RSRC_CONF,
"the public subdirectory in users' home directories, or "
"'disabled', or 'disabled username username...', or "
"'enabled username username...'"),
{NULL}
};
static int translate_userdir(request_rec *r) {
ap_conf_vector_t *server_conf;
const userdir_config *s_cfg;
const char *userdirs;
const char *user, *dname;
char *redirect;
apr_finfo_t statbuf;
if (r->uri[0] != '/' || r->uri[1] != '~') {
return DECLINED;
}
server_conf = r->server->module_config;
s_cfg = ap_get_module_config(server_conf, &userdir_module);
userdirs = s_cfg->userdir;
if (userdirs == NULL) {
return DECLINED;
}
dname = r->uri + 2;
user = ap_getword(r->pool, &dname, '/');
if (dname[-1] == '/') {
--dname;
}
if (user[0] == '\0' ||
(user[1] == '.' && (user[2] == '\0' ||
(user[2] == '.' && user[3] == '\0')))) {
return DECLINED;
}
if (apr_table_get(s_cfg->disabled_users, user) != NULL) {
return DECLINED;
}
if (s_cfg->globally_disabled == O_DISABLE
&& apr_table_get(s_cfg->enabled_users, user) == NULL) {
return DECLINED;
}
while (*userdirs) {
const char *userdir = ap_getword_conf(r->pool, &userdirs);
char *filename = NULL, *prefix = NULL;
apr_status_t rv;
int is_absolute = ap_os_is_path_absolute(r->pool, userdir);
if (ap_strchr_c(userdir, '*'))
prefix = ap_getword(r->pool, &userdir, '*');
if (userdir[0] == '\0' || is_absolute) {
if (prefix) {
#if defined(HAVE_DRIVE_LETTERS)
if (strchr(prefix + 2, ':'))
#else
if (strchr(prefix, ':') && !is_absolute)
#endif
{
redirect = apr_pstrcat(r->pool, prefix, user, userdir,
dname, NULL);
apr_table_setn(r->headers_out, "Location", redirect);
return HTTP_MOVED_TEMPORARILY;
} else
filename = apr_pstrcat(r->pool, prefix, user, userdir,
NULL);
} else
filename = apr_pstrcat(r->pool, userdir, "/", user, NULL);
} else if (prefix && ap_strchr_c(prefix, ':')) {
redirect = apr_pstrcat(r->pool, prefix, user, dname, NULL);
apr_table_setn(r->headers_out, "Location", redirect);
return HTTP_MOVED_TEMPORARILY;
} else {
#if APR_HAS_USER
char *homedir;
if (apr_uid_homepath_get(&homedir, user, r->pool) == APR_SUCCESS) {
filename = apr_pstrcat(r->pool, homedir, "/", userdir, NULL);
}
#else
return DECLINED;
#endif
}
if (filename && (!*userdirs
|| ((rv = apr_stat(&statbuf, filename, APR_FINFO_MIN,
r->pool)) == APR_SUCCESS
|| rv == APR_INCOMPLETE))) {
r->filename = apr_pstrcat(r->pool, filename, dname, NULL);
ap_set_context_info(r, apr_pstrmemdup(r->pool, r->uri,
dname - r->uri),
filename);
if (*userdirs && dname[0] == 0)
r->finfo = statbuf;
apr_table_setn(r->notes, "mod_userdir_user", user);
return OK;
}
}
return DECLINED;
}
#if defined(HAVE_UNIX_SUEXEC)
static ap_unix_identity_t *get_suexec_id_doer(const request_rec *r) {
ap_unix_identity_t *ugid = NULL;
#if APR_HAS_USER
const char *username = apr_table_get(r->notes, "mod_userdir_user");
if (username == NULL) {
return NULL;
}
if ((ugid = apr_palloc(r->pool, sizeof(*ugid))) == NULL) {
return NULL;
}
if (apr_uid_get(&ugid->uid, &ugid->gid, username, r->pool) != APR_SUCCESS) {
return NULL;
}
ugid->userdir = 1;
#endif
return ugid;
}
#endif
static void register_hooks(apr_pool_t *p) {
static const char * const aszPre[]= { "mod_alias.c",NULL };
static const char * const aszSucc[]= { "mod_vhost_alias.c",NULL };
ap_hook_translate_name(translate_userdir,aszPre,aszSucc,APR_HOOK_MIDDLE);
#if defined(HAVE_UNIX_SUEXEC)
ap_hook_get_suexec_identity(get_suexec_id_doer,NULL,NULL,APR_HOOK_FIRST);
#endif
}
AP_DECLARE_MODULE(userdir) = {
STANDARD20_MODULE_STUFF,
NULL,
NULL,
create_userdir_config,
merge_userdir_config,
userdir_cmds,
register_hooks
};