#include "httpd.h"
#include "http_config.h"
#include "apr_strings.h"
#include "mod_dav.h"
#include "repos.h"
typedef struct {
const char *lockdb_path;
} dav_fs_server_conf;
extern module AP_MODULE_DECLARE_DATA dav_fs_module;
const char *dav_get_lockdb_path(const request_rec *r) {
dav_fs_server_conf *conf;
conf = ap_get_module_config(r->server->module_config, &dav_fs_module);
return conf->lockdb_path;
}
static void *dav_fs_create_server_config(apr_pool_t *p, server_rec *s) {
return apr_pcalloc(p, sizeof(dav_fs_server_conf));
}
static void *dav_fs_merge_server_config(apr_pool_t *p,
void *base, void *overrides) {
dav_fs_server_conf *parent = base;
dav_fs_server_conf *child = overrides;
dav_fs_server_conf *newconf;
newconf = apr_pcalloc(p, sizeof(*newconf));
newconf->lockdb_path =
child->lockdb_path ? child->lockdb_path : parent->lockdb_path;
return newconf;
}
static const char *dav_fs_cmd_davlockdb(cmd_parms *cmd, void *config,
const char *arg1) {
dav_fs_server_conf *conf;
conf = ap_get_module_config(cmd->server->module_config,
&dav_fs_module);
conf->lockdb_path = ap_server_root_relative(cmd->pool, arg1);
if (!conf->lockdb_path) {
return apr_pstrcat(cmd->pool, "Invalid DAVLockDB path ",
arg1, NULL);
}
return NULL;
}
static const command_rec dav_fs_cmds[] = {
AP_INIT_TAKE1("DAVLockDB", dav_fs_cmd_davlockdb, NULL, RSRC_CONF,
"specify a lock database"),
{ NULL }
};
static void register_hooks(apr_pool_t *p) {
dav_hook_gather_propsets(dav_fs_gather_propsets, NULL, NULL,
APR_HOOK_MIDDLE);
dav_hook_find_liveprop(dav_fs_find_liveprop, NULL, NULL, APR_HOOK_MIDDLE);
dav_hook_insert_all_liveprops(dav_fs_insert_all_liveprops, NULL, NULL,
APR_HOOK_MIDDLE);
dav_fs_register(p);
}
AP_DECLARE_MODULE(dav_fs) = {
STANDARD20_MODULE_STUFF,
NULL,
NULL,
dav_fs_create_server_config,
dav_fs_merge_server_config,
dav_fs_cmds,
register_hooks,
};