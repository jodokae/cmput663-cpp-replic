#include "apr_strings.h"
#include "apr_portable.h"
#include "apr_buckets.h"
#include "ap_config.h"
#include "httpd.h"
#include "http_config.h"
#include "http_core.h"
#include "http_protocol.h"
#include "http_request.h"
#include "http_log.h"
#include "util_script.h"
#include "mod_core.h"
#include "apr_optional.h"
#include "apr_lib.h"
#include "mod_cgi.h"
#include "mpm_common.h"
#if defined(NETWARE)
module AP_MODULE_DECLARE_DATA netware_module;
typedef struct {
apr_table_t *file_type_handlers;
apr_table_t *file_handler_mode;
apr_table_t *extra_env_vars;
} netware_dir_config;
static void *create_netware_dir_config(apr_pool_t *p, char *dir) {
netware_dir_config *new = (netware_dir_config*) apr_palloc(p, sizeof(netware_dir_config));
new->file_type_handlers = apr_table_make(p, 10);
new->file_handler_mode = apr_table_make(p, 10);
new->extra_env_vars = apr_table_make(p, 10);
apr_table_setn(new->file_type_handlers, "NLM", "OS");
return new;
}
static void *merge_netware_dir_configs(apr_pool_t *p, void *basev, void *addv) {
netware_dir_config *base = (netware_dir_config *) basev;
netware_dir_config *add = (netware_dir_config *) addv;
netware_dir_config *new = (netware_dir_config *) apr_palloc(p, sizeof(netware_dir_config));
new->file_type_handlers = apr_table_overlay(p, add->file_type_handlers, base->file_type_handlers);
new->file_handler_mode = apr_table_overlay(p, add->file_handler_mode, base->file_handler_mode);
new->extra_env_vars = apr_table_overlay(p, add->extra_env_vars, base->extra_env_vars);
return new;
}
static const char *set_extension_map(cmd_parms *cmd, netware_dir_config *m,
char *CGIhdlr, char *ext, char *detach) {
int i, len;
if (*ext == '.')
++ext;
if (CGIhdlr != NULL) {
len = strlen(CGIhdlr);
for (i=0; i<len; i++) {
if (CGIhdlr[i] == '\\') {
CGIhdlr[i] = '/';
}
}
}
apr_table_set(m->file_type_handlers, ext, CGIhdlr);
if (detach) {
apr_table_set(m->file_handler_mode, ext, "y");
}
return NULL;
}
static apr_status_t ap_cgi_build_command(const char **cmd, const char ***argv,
request_rec *r, apr_pool_t *p,
cgi_exec_info_t *e_info) {
char *ext = NULL;
char *cmd_only, *ptr;
const char *new_cmd;
netware_dir_config *d;
const char *args = "";
d = (netware_dir_config *)ap_get_module_config(r->per_dir_config,
&netware_module);
if (e_info->process_cgi) {
*cmd = r->filename;
if (r->args && r->args[0] && !ap_strchr_c(r->args, '=')) {
args = r->args;
}
}
cmd_only = apr_pstrdup(p, *cmd);
e_info->cmd_type = APR_PROGRAM;
for (ptr = cmd_only; *ptr && (*ptr != ' '); ptr++);
*ptr = '\0';
ext = strrchr(apr_filepath_name_get(cmd_only), '.');
if (!ext) {
ext = "";
}
if (*ext == '.') {
++ext;
}
new_cmd = apr_table_get(d->file_type_handlers, ext);
e_info->detached = 1;
if (new_cmd == NULL) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(02135)
"Could not find a command associated with the %s extension", ext);
return APR_EBADF;
}
if (stricmp(new_cmd, "OS")) {
*cmd = apr_pstrcat (p, new_cmd, " ", cmd_only, NULL);
if (apr_table_get(d->file_handler_mode, ext)) {
e_info->addrspace = 1;
}
}
apr_tokenize_to_argv(*cmd, (char***)argv, p);
*cmd = ap_server_root_relative(p, *argv[0]);
return APR_SUCCESS;
}
static int
netware_pre_config(apr_pool_t *pconf, apr_pool_t *plog,
apr_pool_t *ptemp) {
ap_sys_privileges_handlers(1);
return OK;
}
static void register_hooks(apr_pool_t *p) {
APR_REGISTER_OPTIONAL_FN(ap_cgi_build_command);
ap_hook_pre_config(netware_pre_config,
NULL, NULL, APR_HOOK_FIRST);
}
static const command_rec netware_cmds[] = {
AP_INIT_TAKE23("CGIMapExtension", set_extension_map, NULL, OR_FILEINFO,
"Full path to the CGI NLM module followed by a file extension. If the "
"first parameter is set to \"OS\" then the following file extension is "
"treated as NLM. The optional parameter \"detach\" can be specified if "
"the NLM should be launched in its own address space."),
{ NULL }
};
AP_DECLARE_MODULE(netware) = {
STANDARD20_MODULE_STUFF,
create_netware_dir_config,
merge_netware_dir_configs,
NULL,
NULL,
netware_cmds,
register_hooks
};
#endif
