#include "httpd.h"
#include "http_config.h"
#include "http_core.h"
#include "http_log.h"
#include "http_main.h"
#include "http_protocol.h"
#include "http_request.h"
#include "util_script.h"
#include "http_connection.h"
#if defined(HAVE_UNIX_SUEXEC)
#include "unixd.h"
#endif
#include "scoreboard.h"
#include "mpm_common.h"
#include "apr_strings.h"
#include <stdio.h>
typedef struct x_cfg {
int cmode;
#define CONFIG_MODE_SERVER 1
#define CONFIG_MODE_DIRECTORY 2
#define CONFIG_MODE_COMBO 3
int local;
int congenital;
char *trace;
char *loc;
} x_cfg;
static const char *trace = NULL;
module AP_MODULE_DECLARE_DATA example_hooks_module;
static x_cfg *our_dconfig(const request_rec *r) {
return (x_cfg *) ap_get_module_config(r->per_dir_config, &example_hooks_module);
}
#if 0
static x_cfg *our_sconfig(const server_rec *s) {
return (x_cfg *) ap_get_module_config(s->module_config, &example_hooks_module);
}
static x_cfg *our_rconfig(const request_rec *r) {
return (x_cfg *) ap_get_module_config(r->request_config, &example_hooks_module);
}
#endif
static x_cfg *our_cconfig(const conn_rec *c) {
return (x_cfg *) ap_get_module_config(c->conn_config, &example_hooks_module);
}
#if !defined(EXAMPLE_LOG_EACH)
#define EXAMPLE_LOG_EACH 0
#endif
#if EXAMPLE_LOG_EACH
static void example_log_each(apr_pool_t *p, server_rec *s, const char *note) {
if (s != NULL) {
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s, APLOGNO(02991)
"mod_example_hooks: %s", note);
} else {
apr_file_t *out = NULL;
apr_file_open_stderr(&out, p);
apr_file_printf(out, "mod_example_hooks traced in non-loggable "
"context: %s\n", note);
}
}
#endif
static void trace_startup(apr_pool_t *p, server_rec *s, x_cfg *mconfig,
const char *note) {
const char *sofar;
char *where, *addon;
#if EXAMPLE_LOG_EACH
example_log_each(p, s, note);
#endif
where = (mconfig != NULL) ? mconfig->loc : "nowhere";
where = (where != NULL) ? where : "";
addon = apr_pstrcat(p,
" <li>\n"
" <dl>\n"
" <dt><samp>", note, "</samp></dt>\n"
" <dd><samp>[", where, "]</samp></dd>\n"
" </dl>\n"
" </li>\n",
NULL);
sofar = (trace == NULL) ? "" : trace;
trace = apr_pstrcat(p, sofar, addon, NULL);
}
#define TRACE_NOTE "example-hooks-trace"
static void trace_request(const request_rec *r, const char *note) {
const char *trace_copy, *sofar;
char *addon, *where;
x_cfg *cfg;
#if EXAMPLE_LOG_EACH
example_log_each(r->pool, r->server, note);
#endif
if ((sofar = apr_table_get(r->notes, TRACE_NOTE)) == NULL) {
sofar = "";
}
cfg = our_dconfig(r);
where = (cfg != NULL) ? cfg->loc : "nowhere";
where = (where != NULL) ? where : "";
addon = apr_pstrcat(r->pool,
" <li>\n"
" <dl>\n"
" <dt><samp>", note, "</samp></dt>\n"
" <dd><samp>[", where, "]</samp></dd>\n"
" </dl>\n"
" </li>\n",
NULL);
trace_copy = apr_pstrcat(r->pool, sofar, addon, NULL);
apr_table_set(r->notes, TRACE_NOTE, trace_copy);
}
#define CONN_NOTE "example-hooks-connection"
static void trace_connection(conn_rec *c, const char *note) {
const char *trace_copy, *sofar;
char *addon, *where;
void *data;
x_cfg *cfg;
#if EXAMPLE_LOG_EACH
example_log_each(c->pool, c->base_server, note);
#endif
cfg = our_cconfig(c);
where = (cfg != NULL) ? cfg->loc : "nowhere";
where = (where != NULL) ? where : "";
addon = apr_pstrcat(c->pool,
" <li>\n"
" <dl>\n"
" <dt><samp>", note, "</samp></dt>\n"
" <dd><samp>[", where, "]</samp></dd>\n"
" </dl>\n"
" </li>\n",
NULL);
apr_pool_userdata_get(&data, CONN_NOTE, c->pool);
sofar = (data == NULL) ? "" : (const char *) data;
trace_copy = apr_pstrcat(c->pool, sofar, addon, NULL);
apr_pool_userdata_set((const void *) trace_copy, CONN_NOTE,
NULL, c->pool);
}
static void trace_nocontext(apr_pool_t *p, const char *file, int line,
const char *note) {
#if defined(EXAMPLE_LOG_EACH)
ap_log_perror(file, line, APLOG_MODULE_INDEX, APLOG_NOTICE, 0, p, "%s", note);
#endif
}
static const char *cmd_example(cmd_parms *cmd, void *mconfig) {
x_cfg *cfg = (x_cfg *) mconfig;
cfg->local = 1;
trace_startup(cmd->pool, cmd->server, cfg, "cmd_example()");
return NULL;
}
static void *x_create_dir_config(apr_pool_t *p, char *dirspec) {
x_cfg *cfg;
char *dname = dirspec;
char *note;
cfg = (x_cfg *) apr_pcalloc(p, sizeof(x_cfg));
cfg->local = 0;
cfg->congenital = 0;
cfg->cmode = CONFIG_MODE_DIRECTORY;
dname = (dname != NULL) ? dname : "";
cfg->loc = apr_pstrcat(p, "DIR(", dname, ")", NULL);
note = apr_psprintf(p, "x_create_dir_config(p == %pp, dirspec == %s)",
(void*) p, dirspec);
trace_startup(p, NULL, cfg, note);
return (void *) cfg;
}
static void *x_merge_dir_config(apr_pool_t *p, void *parent_conf,
void *newloc_conf) {
x_cfg *merged_config = (x_cfg *) apr_pcalloc(p, sizeof(x_cfg));
x_cfg *pconf = (x_cfg *) parent_conf;
x_cfg *nconf = (x_cfg *) newloc_conf;
char *note;
merged_config->local = nconf->local;
merged_config->loc = apr_pstrdup(p, nconf->loc);
merged_config->congenital = (pconf->congenital | pconf->local);
merged_config->cmode =
(pconf->cmode == nconf->cmode) ? pconf->cmode : CONFIG_MODE_COMBO;
note = apr_psprintf(p, "x_merge_dir_config(p == %pp, parent_conf == "
"%pp, newloc_conf == %pp)", (void*) p,
(void*) parent_conf, (void*) newloc_conf);
trace_startup(p, NULL, merged_config, note);
return (void *) merged_config;
}
static void *x_create_server_config(apr_pool_t *p, server_rec *s) {
x_cfg *cfg;
char *sname = s->server_hostname;
cfg = (x_cfg *) apr_pcalloc(p, sizeof(x_cfg));
cfg->local = 0;
cfg->congenital = 0;
cfg->cmode = CONFIG_MODE_SERVER;
sname = (sname != NULL) ? sname : "";
cfg->loc = apr_pstrcat(p, "SVR(", sname, ")", NULL);
trace_startup(p, s, cfg, "x_create_server_config()");
return (void *) cfg;
}
static void *x_merge_server_config(apr_pool_t *p, void *server1_conf,
void *server2_conf) {
x_cfg *merged_config = (x_cfg *) apr_pcalloc(p, sizeof(x_cfg));
x_cfg *s1conf = (x_cfg *) server1_conf;
x_cfg *s2conf = (x_cfg *) server2_conf;
char *note;
merged_config->cmode =
(s1conf->cmode == s2conf->cmode) ? s1conf->cmode : CONFIG_MODE_COMBO;
merged_config->local = s2conf->local;
merged_config->congenital = (s1conf->congenital | s1conf->local);
merged_config->loc = apr_pstrdup(p, s2conf->loc);
note = apr_pstrcat(p, "x_merge_server_config(\"", s1conf->loc, "\",\"",
s2conf->loc, "\")", NULL);
trace_startup(p, NULL, merged_config, note);
return (void *) merged_config;
}
static int x_pre_config(apr_pool_t *pconf, apr_pool_t *plog,
apr_pool_t *ptemp) {
trace_startup(ptemp, NULL, NULL, "x_pre_config()");
return OK;
}
static int x_check_config(apr_pool_t *pconf, apr_pool_t *plog,
apr_pool_t *ptemp, server_rec *s) {
trace_startup(ptemp, s, NULL, "x_check_config()");
return OK;
}
static void x_test_config(apr_pool_t *pconf, server_rec *s) {
apr_file_t *out = NULL;
apr_file_open_stderr(&out, pconf);
apr_file_printf(out, "Example module configuration test routine\n");
trace_startup(pconf, s, NULL, "x_test_config()");
}
static int x_open_logs(apr_pool_t *pconf, apr_pool_t *plog,
apr_pool_t *ptemp, server_rec *s) {
trace_startup(ptemp, s, NULL, "x_open_logs()");
return OK;
}
static int x_post_config(apr_pool_t *pconf, apr_pool_t *plog,
apr_pool_t *ptemp, server_rec *s) {
trace_startup(ptemp, s, NULL, "x_post_config()");
return OK;
}
static apr_status_t x_child_exit(void *data) {
char *note;
server_rec *s = data;
char *sname = s->server_hostname;
sname = (sname != NULL) ? sname : "";
note = apr_pstrcat(s->process->pool, "x_child_exit(", sname, ")", NULL);
trace_startup(s->process->pool, s, NULL, note);
return APR_SUCCESS;
}
static void x_child_init(apr_pool_t *p, server_rec *s) {
char *note;
char *sname = s->server_hostname;
sname = (sname != NULL) ? sname : "";
note = apr_pstrcat(p, "x_child_init(", sname, ")", NULL);
trace_startup(p, s, NULL, note);
apr_pool_cleanup_register(p, s, x_child_exit, x_child_exit);
}
static const char *x_http_scheme(const request_rec *r) {
trace_request(r, "x_http_scheme()");
return NULL;
}
static apr_port_t x_default_port(const request_rec *r) {
trace_request(r, "x_default_port()");
return 0;
}
static void x_insert_filter(request_rec *r) {
trace_request(r, "x_insert_filter()");
}
static void x_insert_error_filter(request_rec *r) {
trace_request(r, "x_insert_error_filter()");
}
static int x_handler(request_rec *r) {
x_cfg *dcfg;
char *note;
void *conn_data;
apr_status_t status;
dcfg = our_dconfig(r);
note = apr_pstrcat(r->pool, "x_handler(), handler is \"",
r->handler, "\"", NULL);
trace_request(r, note);
if (strcmp(r->handler, "example-hooks-handler")) {
return DECLINED;
}
ap_set_content_type(r, "text/html");
if (r->header_only) {
return OK;
}
ap_rputs(DOCTYPE_HTML_3_2, r);
ap_rputs("<HTML>\n", r);
ap_rputs(" <HEAD>\n", r);
ap_rputs(" <TITLE>mod_example_hooks Module Content-Handler Output\n", r);
ap_rputs(" </TITLE>\n", r);
ap_rputs(" </HEAD>\n", r);
ap_rputs(" <BODY>\n", r);
ap_rputs(" <H1><SAMP>mod_example_hooks</SAMP> Module Content-Handler Output\n", r);
ap_rputs(" </H1>\n", r);
ap_rputs(" <P>\n", r);
ap_rprintf(r, " Apache HTTP Server version: \"%s\"\n",
ap_get_server_banner());
ap_rputs(" <BR>\n", r);
ap_rprintf(r, " Server built: \"%s\"\n", ap_get_server_built());
ap_rputs(" </P>\n", r);
ap_rputs(" <P>\n", r);
ap_rputs(" The format for the callback trace is:\n", r);
ap_rputs(" </P>\n", r);
ap_rputs(" <DL>\n", r);
ap_rputs(" <DT><EM>n</EM>.<SAMP>&lt;routine-name&gt;", r);
ap_rputs("(&lt;routine-data&gt;)</SAMP>\n", r);
ap_rputs(" </DT>\n", r);
ap_rputs(" <DD><SAMP>[&lt;applies-to&gt;]</SAMP>\n", r);
ap_rputs(" </DD>\n", r);
ap_rputs(" </DL>\n", r);
ap_rputs(" <P>\n", r);
ap_rputs(" The <SAMP>&lt;routine-data&gt;</SAMP> is supplied by\n", r);
ap_rputs(" the routine when it requests the trace,\n", r);
ap_rputs(" and the <SAMP>&lt;applies-to&gt;</SAMP> is extracted\n", r);
ap_rputs(" from the configuration record at the time of the trace.\n", r);
ap_rputs(" <STRONG>SVR()</STRONG> indicates a server environment\n", r);
ap_rputs(" (blank means the main or default server, otherwise it's\n", r);
ap_rputs(" the name of the VirtualHost); <STRONG>DIR()</STRONG>\n", r);
ap_rputs(" indicates a location in the URL or filesystem\n", r);
ap_rputs(" namespace.\n", r);
ap_rputs(" </P>\n", r);
ap_rprintf(r, " <H2>Startup callbacks so far:</H2>\n <OL>\n%s </OL>\n",
trace);
ap_rputs(" <H2>Connection-specific callbacks so far:</H2>\n", r);
status = apr_pool_userdata_get(&conn_data, CONN_NOTE,
r->connection->pool);
if ((status == APR_SUCCESS) && conn_data) {
ap_rprintf(r, " <OL>\n%s </OL>\n", (char *) conn_data);
} else {
ap_rputs(" <P>No connection-specific callback information was "
"retrieved.</P>\n", r);
}
ap_rputs(" <H2>Request-specific callbacks so far:</H2>\n", r);
ap_rprintf(r, " <OL>\n%s </OL>\n", apr_table_get(r->notes, TRACE_NOTE));
ap_rputs(" <H2>Environment for <EM>this</EM> call:</H2>\n", r);
ap_rputs(" <UL>\n", r);
ap_rprintf(r, " <LI>Applies-to: <SAMP>%s</SAMP>\n </LI>\n", dcfg->loc);
ap_rprintf(r, " <LI>\"Example\" directive declared here: %s\n </LI>\n",
(dcfg->local ? "YES" : "NO"));
ap_rprintf(r, " <LI>\"Example\" inherited: %s\n </LI>\n",
(dcfg->congenital ? "YES" : "NO"));
ap_rputs(" </UL>\n", r);
ap_rputs(" </BODY>\n", r);
ap_rputs("</HTML>\n", r);
return OK;
}
static int x_quick_handler(request_rec *r, int lookup_uri) {
trace_request(r, "x_quick_handler()");
return DECLINED;
}
static int x_pre_connection(conn_rec *c, void *csd) {
char *note;
note = apr_psprintf(c->pool, "x_pre_connection(c = %pp, p = %pp)",
(void*) c, (void*) c->pool);
trace_connection(c, note);
return OK;
}
static int x_process_connection(conn_rec *c) {
trace_connection(c, "x_process_connection()");
return DECLINED;
}
static void x_pre_read_request(request_rec *r, conn_rec *c) {
trace_request(r, "x_pre_read_request()");
}
static int x_post_read_request(request_rec *r) {
trace_request(r, "x_post_read_request()");
return DECLINED;
}
static int x_translate_name(request_rec *r) {
trace_request(r, "x_translate_name()");
return DECLINED;
}
static int x_map_to_storage(request_rec *r) {
trace_request(r, "x_map_to_storage()");
return DECLINED;
}
static int x_header_parser(request_rec *r) {
trace_request(r, "x_header_parser()");
return DECLINED;
}
static int x_check_access(request_rec *r) {
trace_request(r, "x_check_access()");
return DECLINED;
}
static int x_check_authn(request_rec *r) {
trace_request(r, "x_check_authn()");
return DECLINED;
}
static int x_check_authz(request_rec *r) {
trace_request(r, "x_check_authz()");
return DECLINED;
}
static int x_type_checker(request_rec *r) {
trace_request(r, "x_type_checker()");
return DECLINED;
}
static int x_fixups(request_rec *r) {
trace_request(r, "x_fixups()");
return DECLINED;
}
static int x_log_transaction(request_rec *r) {
trace_request(r, "x_log_transaction()");
return DECLINED;
}
#if defined(HAVE_UNIX_SUEXEC)
static ap_unix_identity_t *x_get_suexec_identity(const request_rec *r) {
trace_request(r, "x_get_suexec_identity()");
return NULL;
}
#endif
static conn_rec *x_create_connection(apr_pool_t *p, server_rec *server,
apr_socket_t *csd, long conn_id,
void *sbh, apr_bucket_alloc_t *alloc) {
trace_nocontext(p, __FILE__, __LINE__, "x_create_connection()");
return NULL;
}
static int x_get_mgmt_items(apr_pool_t *p, const char *val, apr_hash_t *ht) {
trace_nocontext(p, __FILE__, __LINE__, "x_check_config()");
return DECLINED;
}
static int x_create_request(request_rec *r) {
trace_nocontext( r->pool, __FILE__, __LINE__, "x_create_request()");
return DECLINED;
}
static int x_pre_mpm(apr_pool_t *p, ap_scoreboard_e sb_type) {
trace_nocontext(p, __FILE__, __LINE__, "x_pre_mpm()");
return DECLINED;
}
static int x_monitor(apr_pool_t *p, server_rec *s) {
trace_nocontext(p, __FILE__, __LINE__, "x_monitor()");
return DECLINED;
}
static void x_register_hooks(apr_pool_t *p) {
ap_hook_pre_config(x_pre_config, NULL, NULL, APR_HOOK_MIDDLE);
ap_hook_check_config(x_check_config, NULL, NULL, APR_HOOK_MIDDLE);
ap_hook_test_config(x_test_config, NULL, NULL, APR_HOOK_MIDDLE);
ap_hook_open_logs(x_open_logs, NULL, NULL, APR_HOOK_MIDDLE);
ap_hook_post_config(x_post_config, NULL, NULL, APR_HOOK_MIDDLE);
ap_hook_child_init(x_child_init, NULL, NULL, APR_HOOK_MIDDLE);
ap_hook_handler(x_handler, NULL, NULL, APR_HOOK_MIDDLE);
ap_hook_quick_handler(x_quick_handler, NULL, NULL, APR_HOOK_MIDDLE);
ap_hook_pre_connection(x_pre_connection, NULL, NULL, APR_HOOK_MIDDLE);
ap_hook_process_connection(x_process_connection, NULL, NULL, APR_HOOK_MIDDLE);
ap_hook_pre_read_request(x_pre_read_request, NULL, NULL,
APR_HOOK_MIDDLE);
ap_hook_post_read_request(x_post_read_request, NULL, NULL,
APR_HOOK_MIDDLE);
ap_hook_log_transaction(x_log_transaction, NULL, NULL, APR_HOOK_MIDDLE);
ap_hook_http_scheme(x_http_scheme, NULL, NULL, APR_HOOK_MIDDLE);
ap_hook_default_port(x_default_port, NULL, NULL, APR_HOOK_MIDDLE);
ap_hook_translate_name(x_translate_name, NULL, NULL, APR_HOOK_MIDDLE);
ap_hook_map_to_storage(x_map_to_storage, NULL,NULL, APR_HOOK_MIDDLE);
ap_hook_header_parser(x_header_parser, NULL, NULL, APR_HOOK_MIDDLE);
ap_hook_fixups(x_fixups, NULL, NULL, APR_HOOK_MIDDLE);
ap_hook_type_checker(x_type_checker, NULL, NULL, APR_HOOK_MIDDLE);
ap_hook_check_access(x_check_access, NULL, NULL, APR_HOOK_MIDDLE,
AP_AUTH_INTERNAL_PER_CONF);
ap_hook_check_authn(x_check_authn, NULL, NULL, APR_HOOK_MIDDLE,
AP_AUTH_INTERNAL_PER_CONF);
ap_hook_check_authz(x_check_authz, NULL, NULL, APR_HOOK_MIDDLE,
AP_AUTH_INTERNAL_PER_CONF);
ap_hook_insert_filter(x_insert_filter, NULL, NULL, APR_HOOK_MIDDLE);
ap_hook_insert_error_filter(x_insert_error_filter, NULL, NULL, APR_HOOK_MIDDLE);
#if defined(HAVE_UNIX_SUEXEC)
ap_hook_get_suexec_identity(x_get_suexec_identity, NULL, NULL, APR_HOOK_MIDDLE);
#endif
ap_hook_create_connection(x_create_connection, NULL, NULL, APR_HOOK_MIDDLE);
ap_hook_get_mgmt_items(x_get_mgmt_items, NULL, NULL, APR_HOOK_MIDDLE);
ap_hook_create_request(x_create_request, NULL, NULL, APR_HOOK_MIDDLE);
ap_hook_pre_mpm(x_pre_mpm, NULL, NULL, APR_HOOK_MIDDLE);
ap_hook_monitor(x_monitor, NULL, NULL, APR_HOOK_MIDDLE);
}
static const command_rec x_cmds[] = {
AP_INIT_NO_ARGS(
"Example",
cmd_example,
NULL,
OR_OPTIONS,
"Example directive - no arguments"
),
{NULL}
};
AP_DECLARE_MODULE(example_hooks) = {
STANDARD20_MODULE_STUFF,
x_create_dir_config,
x_merge_dir_config,
x_create_server_config,
x_merge_server_config,
x_cmds,
x_register_hooks,
};