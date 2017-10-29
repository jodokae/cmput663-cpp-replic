#include "httpd.h"
#include "http_config.h"
#include "http_core.h"
#include "http_log.h"
#include "http_request.h"
#include "apr_strings.h"
#include "unixd.h"
#include "mpm_common.h"
#include "mod_suexec.h"
module AP_MODULE_DECLARE_DATA suexec_module;
static void *mkconfig(apr_pool_t *p) {
suexec_config_t *cfg = apr_palloc(p, sizeof(suexec_config_t));
cfg->active = 0;
return cfg;
}
static void *create_mconfig_for_server(apr_pool_t *p, server_rec *s) {
return mkconfig(p);
}
static void *create_mconfig_for_directory(apr_pool_t *p, char *dir) {
return mkconfig(p);
}
static const char *set_suexec_ugid(cmd_parms *cmd, void *mconfig,
const char *uid, const char *gid) {
suexec_config_t *cfg = (suexec_config_t *) mconfig;
const char *err = ap_check_cmd_context(cmd, NOT_IN_DIR_LOC_FILE);
if (err != NULL) {
return err;
}
if (!ap_unixd_config.suexec_enabled) {
return apr_pstrcat(cmd->pool, "SuexecUserGroup configured, but "
"suEXEC is disabled: ",
ap_unixd_config.suexec_disabled_reason, NULL);
}
cfg->ugid.uid = ap_uname2id(uid);
cfg->ugid.gid = ap_gname2id(gid);
cfg->ugid.userdir = 0;
cfg->active = 1;
return NULL;
}
static ap_unix_identity_t *get_suexec_id_doer(const request_rec *r) {
suexec_config_t *cfg =
(suexec_config_t *) ap_get_module_config(r->per_dir_config, &suexec_module);
return cfg->active ? &cfg->ugid : NULL;
}
#define SUEXEC_POST_CONFIG_USERDATA "suexec_post_config_userdata"
static int suexec_post_config(apr_pool_t *p, apr_pool_t *plog,
apr_pool_t *ptemp, server_rec *s) {
void *reported;
apr_pool_userdata_get(&reported, SUEXEC_POST_CONFIG_USERDATA,
s->process->pool);
if ((reported == NULL) && ap_unixd_config.suexec_enabled) {
ap_log_error(APLOG_MARK, APLOG_NOTICE, 0, s, APLOGNO(01232)
"suEXEC mechanism enabled (wrapper: %s)", SUEXEC_BIN);
apr_pool_userdata_set((void *)1, SUEXEC_POST_CONFIG_USERDATA,
apr_pool_cleanup_null, s->process->pool);
}
return OK;
}
#undef SUEXEC_POST_CONFIG_USERDATA
static const command_rec suexec_cmds[] = {
AP_INIT_TAKE2("SuexecUserGroup", set_suexec_ugid, NULL, RSRC_CONF,
"User and group for spawned processes"),
{ NULL }
};
static void suexec_hooks(apr_pool_t *p) {
ap_hook_get_suexec_identity(get_suexec_id_doer,NULL,NULL,APR_HOOK_MIDDLE);
ap_hook_post_config(suexec_post_config,NULL,NULL,APR_HOOK_MIDDLE);
}
AP_DECLARE_MODULE(suexec) = {
STANDARD20_MODULE_STUFF,
create_mconfig_for_directory,
NULL,
create_mconfig_for_server,
NULL,
suexec_cmds,
suexec_hooks
};