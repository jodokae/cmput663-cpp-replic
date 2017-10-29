#include "mod_proxy.h"
#include "apr_dbm.h"
module AP_MODULE_DECLARE_DATA proxy_express_module;
static int proxy_available = 0;
typedef struct {
const char *dbmfile;
const char *dbmtype;
int enabled;
} express_server_conf;
static const char *set_dbmfile(cmd_parms *cmd,
void *dconf,
const char *arg) {
express_server_conf *sconf;
sconf = ap_get_module_config(cmd->server->module_config, &proxy_express_module);
if ((sconf->dbmfile = ap_server_root_relative(cmd->pool, arg)) == NULL) {
return apr_pstrcat(cmd->pool, "ProxyExpressDBMFile: bad path to file: ",
arg, NULL);
}
return NULL;
}
static const char *set_dbmtype(cmd_parms *cmd,
void *dconf,
const char *arg) {
express_server_conf *sconf;
sconf = ap_get_module_config(cmd->server->module_config, &proxy_express_module);
sconf->dbmtype = arg;
return NULL;
}
static const char *set_enabled(cmd_parms *cmd,
void *dconf,
int flag) {
express_server_conf *sconf;
sconf = ap_get_module_config(cmd->server->module_config, &proxy_express_module);
sconf->enabled = flag;
return NULL;
}
static void *server_create(apr_pool_t *p, server_rec *s) {
express_server_conf *a;
a = (express_server_conf *)apr_pcalloc(p, sizeof(express_server_conf));
a->dbmfile = NULL;
a->dbmtype = "default";
a->enabled = 0;
return (void *)a;
}
static void *server_merge(apr_pool_t *p, void *basev, void *overridesv) {
express_server_conf *a, *base, *overrides;
a = (express_server_conf *)apr_pcalloc(p,
sizeof(express_server_conf));
base = (express_server_conf *)basev;
overrides = (express_server_conf *)overridesv;
a->dbmfile = (overrides->dbmfile) ? overrides->dbmfile : base->dbmfile;
a->dbmtype = (overrides->dbmtype) ? overrides->dbmtype : base->dbmtype;
a->enabled = (overrides->enabled) ? overrides->enabled : base->enabled;
return (void *)a;
}
static int post_config(apr_pool_t *p,
apr_pool_t *plog,
apr_pool_t *ptemp,
server_rec *s) {
proxy_available = (ap_find_linked_module("mod_proxy.c") != NULL);
return OK;
}
static int xlate_name(request_rec *r) {
int i;
const char *name;
char *backend = NULL;
apr_dbm_t *db;
apr_status_t rv;
apr_datum_t key, val;
struct proxy_alias *ralias;
proxy_dir_conf *dconf;
express_server_conf *sconf;
sconf = ap_get_module_config(r->server->module_config, &proxy_express_module);
dconf = ap_get_module_config(r->per_dir_config, &proxy_module);
if (!sconf->enabled) {
return DECLINED;
}
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(01001) "proxy_express: Enabled");
if (!sconf->dbmfile || (r->filename && strncmp(r->filename, "proxy:", 6) == 0)) {
return DECLINED;
}
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(01002)
"proxy_express: Opening DBM file: %s (%s)",
sconf->dbmfile, sconf->dbmtype);
rv = apr_dbm_open_ex(&db, sconf->dbmtype, sconf->dbmfile, APR_DBM_READONLY,
APR_OS_DEFAULT, r->pool);
if (rv != APR_SUCCESS) {
return DECLINED;
}
name = ap_get_server_name(r);
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(01003)
"proxy_express: looking for %s", name);
key.dptr = (char *)name;
key.dsize = strlen(key.dptr);
rv = apr_dbm_fetch(db, key, &val);
if (rv == APR_SUCCESS) {
backend = apr_pstrmemdup(r->pool, val.dptr, val.dsize);
}
apr_dbm_close(db);
if (rv != APR_SUCCESS || !backend) {
return DECLINED;
}
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(01004)
"proxy_express: found %s -> %s", name, backend);
r->filename = apr_pstrcat(r->pool, "proxy:", backend, r->uri, NULL);
r->handler = "proxy-server";
r->proxyreq = PROXYREQ_REVERSE;
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(01005)
"proxy_express: rewritten as: %s", r->filename);
ralias = (struct proxy_alias *)dconf->raliases->elts;
for (i = 0; i < dconf->raliases->nelts; i++, ralias++) {
if (strcasecmp(backend, ralias->real) == 0) {
ralias = NULL;
break;
}
}
if (!ralias) {
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(01006)
"proxy_express: adding PPR entry");
ralias = apr_array_push(dconf->raliases);
ralias->fake = "/";
ralias->real = apr_pstrdup(dconf->raliases->pool, backend);
ralias->flags = 0;
}
return OK;
}
static const command_rec command_table[] = {
AP_INIT_FLAG("ProxyExpressEnable", set_enabled, NULL, OR_FILEINFO,
"Enable the ProxyExpress functionality"),
AP_INIT_TAKE1("ProxyExpressDBMFile", set_dbmfile, NULL, OR_FILEINFO,
"Location of ProxyExpressDBMFile file"),
AP_INIT_TAKE1("ProxyExpressDBMType", set_dbmtype, NULL, OR_FILEINFO,
"Type of ProxyExpressDBMFile file"),
{ NULL }
};
static void register_hooks(apr_pool_t *p) {
ap_hook_post_config(post_config, NULL, NULL, APR_HOOK_LAST);
ap_hook_translate_name(xlate_name, NULL, NULL, APR_HOOK_FIRST);
}
AP_DECLARE_MODULE(proxy_express) = {
STANDARD20_MODULE_STUFF,
NULL,
NULL,
server_create,
server_merge,
command_table,
register_hooks
};
