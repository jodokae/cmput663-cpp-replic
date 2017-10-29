#include "apr.h"
#include "apr_strings.h"
#include "ap_hooks.h"
#include "apr_lib.h"
#define APR_WANT_STRFUNC
#include "apr_want.h"
#include "httpd.h"
#include "http_config.h"
#include "http_core.h"
#include "http_request.h"
module AP_MODULE_DECLARE_DATA vhost_alias_module;
typedef enum {
VHOST_ALIAS_UNSET, VHOST_ALIAS_NONE, VHOST_ALIAS_NAME, VHOST_ALIAS_IP
} mva_mode_e;
typedef struct mva_sconf_t {
const char *doc_root;
const char *cgi_root;
mva_mode_e doc_root_mode;
mva_mode_e cgi_root_mode;
} mva_sconf_t;
static void *mva_create_server_config(apr_pool_t *p, server_rec *s) {
mva_sconf_t *conf;
conf = (mva_sconf_t *) apr_pcalloc(p, sizeof(mva_sconf_t));
conf->doc_root = NULL;
conf->cgi_root = NULL;
conf->doc_root_mode = VHOST_ALIAS_UNSET;
conf->cgi_root_mode = VHOST_ALIAS_UNSET;
return conf;
}
static void *mva_merge_server_config(apr_pool_t *p, void *parentv, void *childv) {
mva_sconf_t *parent = (mva_sconf_t *) parentv;
mva_sconf_t *child = (mva_sconf_t *) childv;
mva_sconf_t *conf;
conf = (mva_sconf_t *) apr_pcalloc(p, sizeof(*conf));
if (child->doc_root_mode == VHOST_ALIAS_UNSET) {
conf->doc_root_mode = parent->doc_root_mode;
conf->doc_root = parent->doc_root;
} else {
conf->doc_root_mode = child->doc_root_mode;
conf->doc_root = child->doc_root;
}
if (child->cgi_root_mode == VHOST_ALIAS_UNSET) {
conf->cgi_root_mode = parent->cgi_root_mode;
conf->cgi_root = parent->cgi_root;
} else {
conf->cgi_root_mode = child->cgi_root_mode;
conf->cgi_root = child->cgi_root;
}
return conf;
}
static int vhost_alias_set_doc_root_ip,
vhost_alias_set_cgi_root_ip,
vhost_alias_set_doc_root_name,
vhost_alias_set_cgi_root_name;
static const char *vhost_alias_set(cmd_parms *cmd, void *dummy, const char *map) {
mva_sconf_t *conf;
mva_mode_e mode, *pmode;
const char **pmap;
const char *p;
conf = (mva_sconf_t *) ap_get_module_config(cmd->server->module_config,
&vhost_alias_module);
if (&vhost_alias_set_doc_root_ip == cmd->info) {
mode = VHOST_ALIAS_IP;
pmap = &conf->doc_root;
pmode = &conf->doc_root_mode;
} else if (&vhost_alias_set_cgi_root_ip == cmd->info) {
mode = VHOST_ALIAS_IP;
pmap = &conf->cgi_root;
pmode = &conf->cgi_root_mode;
} else if (&vhost_alias_set_doc_root_name == cmd->info) {
mode = VHOST_ALIAS_NAME;
pmap = &conf->doc_root;
pmode = &conf->doc_root_mode;
} else if (&vhost_alias_set_cgi_root_name == cmd->info) {
mode = VHOST_ALIAS_NAME;
pmap = &conf->cgi_root;
pmode = &conf->cgi_root_mode;
} else {
return "INTERNAL ERROR: unknown command info";
}
if (!ap_os_is_path_absolute(cmd->pool, map)) {
if (strcasecmp(map, "none")) {
return "format string must be an absolute path, or 'none'";
}
*pmap = NULL;
*pmode = VHOST_ALIAS_NONE;
return NULL;
}
p = map;
while (*p != '\0') {
if (*p++ != '%') {
continue;
}
if (*p == 'p' || *p == '%') {
++p;
continue;
}
if (*p == '-') {
++p;
}
if (apr_isdigit(*p)) {
++p;
} else {
return "syntax error in format string";
}
if (*p == '+') {
++p;
}
if (*p != '.') {
continue;
}
++p;
if (*p == '-') {
++p;
}
if (apr_isdigit(*p)) {
++p;
} else {
return "syntax error in format string";
}
if (*p == '+') {
++p;
}
}
*pmap = map;
*pmode = mode;
return NULL;
}
static const command_rec mva_commands[] = {
AP_INIT_TAKE1("VirtualScriptAlias", vhost_alias_set,
&vhost_alias_set_cgi_root_name, RSRC_CONF,
"how to create a ScriptAlias based on the host"),
AP_INIT_TAKE1("VirtualDocumentRoot", vhost_alias_set,
&vhost_alias_set_doc_root_name, RSRC_CONF,
"how to create the DocumentRoot based on the host"),
AP_INIT_TAKE1("VirtualScriptAliasIP", vhost_alias_set,
&vhost_alias_set_cgi_root_ip, RSRC_CONF,
"how to create a ScriptAlias based on the host"),
AP_INIT_TAKE1("VirtualDocumentRootIP", vhost_alias_set,
&vhost_alias_set_doc_root_ip, RSRC_CONF,
"how to create the DocumentRoot based on the host"),
{ NULL }
};
static APR_INLINE void vhost_alias_checkspace(request_rec *r, char *buf,
char **pdest, int size) {
if (*pdest + size > buf + HUGE_STRING_LEN) {
**pdest = '\0';
if (r->filename) {
r->filename = apr_pstrcat(r->pool, r->filename, buf, NULL);
} else {
r->filename = apr_pstrdup(r->pool, buf);
}
*pdest = buf;
}
}
static void vhost_alias_interpolate(request_rec *r, const char *name,
const char *map, const char *uri) {
enum { MAXDOTS = 19 };
const char *dots[MAXDOTS+1];
int ndots;
char buf[HUGE_STRING_LEN];
char *dest;
const char *docroot;
int N, M, Np, Mp, Nd, Md;
const char *start, *end;
const char *p;
ndots = 0;
dots[ndots++] = name-1;
for (p = name; *p; ++p) {
if (*p == '.' && ndots < MAXDOTS) {
dots[ndots++] = p;
}
}
dots[ndots] = p;
r->filename = NULL;
dest = buf;
while (*map) {
if (*map != '%') {
vhost_alias_checkspace(r, buf, &dest, 1);
*dest++ = *map++;
continue;
}
++map;
if (*map == '%') {
++map;
vhost_alias_checkspace(r, buf, &dest, 1);
*dest++ = '%';
continue;
}
if (*map == 'p') {
++map;
vhost_alias_checkspace(r, buf, &dest, 7);
dest += apr_snprintf(dest, 7, "%d", ap_get_server_port(r));
continue;
}
M = 0;
Np = Mp = 0;
Nd = Md = 0;
if (*map == '-') ++map, Nd = 1;
N = *map++ - '0';
if (*map == '+') ++map, Np = 1;
if (*map == '.') {
++map;
if (*map == '-') {
++map, Md = 1;
}
M = *map++ - '0';
if (*map == '+') {
++map, Mp = 1;
}
}
start = dots[0]+1;
end = dots[ndots];
if (N != 0) {
if (N > ndots) {
start = "_";
end = start+1;
} else if (!Nd) {
start = dots[N-1]+1;
if (!Np) {
end = dots[N];
}
} else {
if (!Np) {
start = dots[ndots-N]+1;
}
end = dots[ndots-N+1];
}
}
if (M != 0) {
if (M > end - start) {
start = "_";
end = start+1;
} else if (!Md) {
start = start+M-1;
if (!Mp) {
end = start+1;
}
} else {
if (!Mp) {
start = end-M;
}
end = end-M+1;
}
}
vhost_alias_checkspace(r, buf, &dest, end - start);
for (p = start; p < end; ++p) {
*dest++ = apr_tolower(*p);
}
}
if (dest - buf > 0 && dest[-1] == '/') {
--dest;
}
*dest = '\0';
if (r->filename)
docroot = apr_pstrcat(r->pool, r->filename, buf, NULL);
else
docroot = apr_pstrdup(r->pool, buf);
r->filename = apr_pstrcat(r->pool, docroot, uri, NULL);
ap_set_context_info(r, NULL, docroot);
ap_set_document_root(r, docroot);
}
static int mva_translate(request_rec *r) {
mva_sconf_t *conf;
const char *name, *map, *uri;
mva_mode_e mode;
const char *cgi;
conf = (mva_sconf_t *) ap_get_module_config(r->server->module_config,
&vhost_alias_module);
cgi = NULL;
if (conf->cgi_root) {
cgi = strstr(r->uri, "cgi-bin/");
if (cgi && (cgi != r->uri + strspn(r->uri, "/"))) {
cgi = NULL;
}
}
if (cgi) {
mode = conf->cgi_root_mode;
map = conf->cgi_root;
uri = cgi + strlen("cgi-bin");
} else if (r->uri[0] == '/') {
mode = conf->doc_root_mode;
map = conf->doc_root;
uri = r->uri;
} else {
return DECLINED;
}
if (mode == VHOST_ALIAS_NAME) {
name = ap_get_server_name(r);
} else if (mode == VHOST_ALIAS_IP) {
name = r->connection->local_ip;
} else {
return DECLINED;
}
r->canonical_filename = "";
vhost_alias_interpolate(r, name, map, uri);
if (cgi) {
r->handler = "cgi-script";
apr_table_setn(r->notes, "alias-forced-type", r->handler);
ap_set_context_info(r, "/cgi-bin", NULL);
}
return OK;
}
static void register_hooks(apr_pool_t *p) {
static const char * const aszPre[]= { "mod_alias.c","mod_userdir.c",NULL };
ap_hook_translate_name(mva_translate, aszPre, NULL, APR_HOOK_MIDDLE);
}
AP_DECLARE_MODULE(vhost_alias) = {
STANDARD20_MODULE_STUFF,
NULL,
NULL,
mva_create_server_config,
mva_merge_server_config,
mva_commands,
register_hooks
};