#include "apr.h"
#include "apr_dso.h"
#include "apr_strings.h"
#include "apr_errno.h"
#include "ap_config.h"
#include "httpd.h"
#include "http_config.h"
#include "http_log.h"
#include "http_core.h"
#include "mod_so.h"
module AP_MODULE_DECLARE_DATA so_module;
typedef struct so_server_conf {
apr_array_header_t *loaded_modules;
} so_server_conf;
static void *so_sconf_create(apr_pool_t *p, server_rec *s) {
so_server_conf *soc;
soc = (so_server_conf *)apr_pcalloc(p, sizeof(so_server_conf));
soc->loaded_modules = apr_array_make(p, DYNAMIC_MODULE_LIMIT,
sizeof(ap_module_symbol_t));
return (void *)soc;
}
#if !defined(NO_DLOPEN)
static apr_status_t unload_module(void *data) {
ap_module_symbol_t *modi = (ap_module_symbol_t*)data;
if (modi->modp == NULL)
return APR_SUCCESS;
ap_remove_loaded_module(modi->modp);
modi->modp = NULL;
modi->name = NULL;
return APR_SUCCESS;
}
static const char *dso_load(cmd_parms *cmd, apr_dso_handle_t **modhandlep,
const char *filename, const char **used_filename) {
int retry = 0;
const char *fullname = ap_server_root_relative(cmd->temp_pool, filename);
char my_error[256];
if (filename != NULL && ap_strchr_c(filename, '/') == NULL) {
retry = 1;
}
if (fullname == NULL && !retry) {
return apr_psprintf(cmd->temp_pool, "Invalid %s path %s",
cmd->cmd->name, filename);
}
*used_filename = fullname;
if (apr_dso_load(modhandlep, fullname, cmd->pool) == APR_SUCCESS) {
return NULL;
}
if (retry) {
*used_filename = filename;
if (apr_dso_load(modhandlep, filename, cmd->pool) == APR_SUCCESS)
return NULL;
}
return apr_pstrcat(cmd->temp_pool, "Cannot load ", filename,
" into server: ",
apr_dso_error(*modhandlep, my_error, sizeof(my_error)),
NULL);
}
static const char *load_module(cmd_parms *cmd, void *dummy,
const char *modname, const char *filename) {
apr_dso_handle_t *modhandle;
apr_dso_handle_sym_t modsym;
module *modp;
const char *module_file;
so_server_conf *sconf;
ap_module_symbol_t *modi;
ap_module_symbol_t *modie;
int i;
const char *error;
*(ap_directive_t **)dummy = NULL;
sconf = (so_server_conf *)ap_get_module_config(cmd->server->module_config,
&so_module);
modie = (ap_module_symbol_t *)sconf->loaded_modules->elts;
for (i = 0; i < sconf->loaded_modules->nelts; i++) {
modi = &modie[i];
if (modi->name != NULL && strcmp(modi->name, modname) == 0) {
ap_log_perror(APLOG_MARK, APLOG_WARNING, 0, cmd->pool, APLOGNO(01574)
"module %s is already loaded, skipping",
modname);
return NULL;
}
}
for (i = 0; ap_preloaded_modules[i]; i++) {
const char *preload_name;
apr_size_t preload_len;
apr_size_t thismod_len;
modp = ap_preloaded_modules[i];
if (memcmp(modp->name, "mod_", 4)) {
continue;
}
preload_name = modp->name + strlen("mod_");
preload_len = strlen(preload_name) - 2;
if (strlen(modname) <= strlen("_module")) {
continue;
}
thismod_len = strlen(modname) - strlen("_module");
if (strcmp(modname + thismod_len, "_module")) {
continue;
}
if (thismod_len != preload_len) {
continue;
}
if (!memcmp(modname, preload_name, preload_len)) {
return apr_pstrcat(cmd->pool, "module ", modname,
" is built-in and can't be loaded",
NULL);
}
}
modi = apr_array_push(sconf->loaded_modules);
modi->name = modname;
error = dso_load(cmd, &modhandle, filename, &module_file);
if (error)
return error;
ap_log_perror(APLOG_MARK, APLOG_DEBUG, 0, cmd->pool, APLOGNO(01575)
"loaded module %s from %s", modname, module_file);
if (apr_dso_sym(&modsym, modhandle, modname) != APR_SUCCESS) {
char my_error[256];
return apr_pstrcat(cmd->pool, "Can't locate API module structure `",
modname, "' in file ", module_file, ": ",
apr_dso_error(modhandle, my_error, sizeof(my_error)),
NULL);
}
modp = (module*) modsym;
modp->dynamic_load_handle = (apr_dso_handle_t *)modhandle;
modi->modp = modp;
if (modp->magic != MODULE_MAGIC_COOKIE) {
return apr_psprintf(cmd->pool, "API module structure '%s' in file %s "
"is garbled - expected signature %08lx but saw "
"%08lx - perhaps this is not an Apache module DSO, "
"or was compiled for a different Apache version?",
modname, module_file,
MODULE_MAGIC_COOKIE, modp->magic);
}
error = ap_add_loaded_module(modp, cmd->pool, modname);
if (error) {
return error;
}
apr_pool_cleanup_register(cmd->pool, modi, unload_module, apr_pool_cleanup_null);
ap_single_module_configure(cmd->pool, cmd->server, modp);
return NULL;
}
static const char *load_file(cmd_parms *cmd, void *dummy, const char *filename) {
apr_dso_handle_t *handle;
const char *used_file, *error;
error = dso_load(cmd, &handle, filename, &used_file);
if (error)
return error;
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, NULL, APLOGNO(01576)
"loaded file %s", used_file);
return NULL;
}
static module *ap_find_loaded_module_symbol(server_rec *s, const char *modname) {
so_server_conf *sconf;
ap_module_symbol_t *modi;
ap_module_symbol_t *modie;
int i;
sconf = (so_server_conf *)ap_get_module_config(s->module_config,
&so_module);
modie = (ap_module_symbol_t *)sconf->loaded_modules->elts;
for (i = 0; i < sconf->loaded_modules->nelts; i++) {
modi = &modie[i];
if (modi->name != NULL && strcmp(modi->name, modname) == 0) {
return modi->modp;
}
}
return NULL;
}
static void dump_loaded_modules(apr_pool_t *p, server_rec *s) {
ap_module_symbol_t *modie;
ap_module_symbol_t *modi;
so_server_conf *sconf;
int i;
apr_file_t *out = NULL;
if (!ap_exists_config_define("DUMP_MODULES")) {
return;
}
apr_file_open_stdout(&out, p);
apr_file_printf(out, "Loaded Modules:\n");
sconf = (so_server_conf *)ap_get_module_config(s->module_config,
&so_module);
for (i = 0; ; i++) {
modi = &ap_prelinked_module_symbols[i];
if (modi->name != NULL) {
apr_file_printf(out, " %s (static)\n", modi->name);
} else {
break;
}
}
modie = (ap_module_symbol_t *)sconf->loaded_modules->elts;
for (i = 0; i < sconf->loaded_modules->nelts; i++) {
modi = &modie[i];
if (modi->name != NULL) {
apr_file_printf(out, " %s (shared)\n", modi->name);
}
}
}
#else
static const char *load_file(cmd_parms *cmd, void *dummy, const char *filename) {
ap_log_perror(APLOG_MARK, APLOG_STARTUP, 0, cmd->pool, APLOGNO(01577)
"WARNING: LoadFile not supported on this platform");
return NULL;
}
static const char *load_module(cmd_parms *cmd, void *dummy,
const char *modname, const char *filename) {
ap_log_perror(APLOG_MARK, APLOG_STARTUP, 0, cmd->pool, APLOGNO(01578)
"WARNING: LoadModule not supported on this platform");
return NULL;
}
#endif
static void register_hooks(apr_pool_t *p) {
#if !defined(NO_DLOPEN)
APR_REGISTER_OPTIONAL_FN(ap_find_loaded_module_symbol);
ap_hook_test_config(dump_loaded_modules, NULL, NULL, APR_HOOK_MIDDLE);
#endif
}
static const command_rec so_cmds[] = {
AP_INIT_TAKE2("LoadModule", load_module, NULL, RSRC_CONF | EXEC_ON_READ,
"a module name and the name of a shared object file to load it from"),
AP_INIT_ITERATE("LoadFile", load_file, NULL, RSRC_CONF | EXEC_ON_READ,
"shared object file or library to load into the server at runtime"),
{ NULL }
};
AP_DECLARE_MODULE(so) = {
STANDARD20_MODULE_STUFF,
NULL,
NULL,
so_sconf_create,
NULL,
so_cmds,
register_hooks
};