#include "apr_strings.h"
#include "apr_lib.h"
#include "ap_config.h"
#include "httpd.h"
#include "http_config.h"
#include "http_core.h"
#include "http_request.h"
#include "http_protocol.h"
#include "http_log.h"
#include "http_main.h"
#include "util_script.h"
#include "mod_rewrite.h"
module AP_MODULE_DECLARE_DATA dir_module;
typedef enum {
MODDIR_OFF = 0,
MODDIR_ON,
MODDIR_UNSET
} moddir_cfg;
#define REDIRECT_OFF 0
#define REDIRECT_UNSET 1
typedef struct dir_config_struct {
apr_array_header_t *index_names;
moddir_cfg do_slash;
moddir_cfg checkhandler;
int redirect_index;
const char *dflt;
} dir_config_rec;
#define DIR_CMD_PERMS OR_INDEXES
static const char *add_index(cmd_parms *cmd, void *dummy, const char *arg) {
dir_config_rec *d = dummy;
const char *t, *w;
int count = 0;
if (!d->index_names) {
d->index_names = apr_array_make(cmd->pool, 2, sizeof(char *));
}
t = arg;
while ((w = ap_getword_conf(cmd->pool, &t)) && w[0]) {
if (count == 0 && !strcasecmp(w, "disabled")) {
const char *tt = t;
const char *ww = ap_getword_conf(cmd->temp_pool, &tt);
if (ww[0] == '\0') {
apr_array_clear(d->index_names);
break;
}
}
*(const char **)apr_array_push(d->index_names) = w;
count++;
}
return NULL;
}
static const char *configure_slash(cmd_parms *cmd, void *d_, int arg) {
dir_config_rec *d = d_;
d->do_slash = arg ? MODDIR_ON : MODDIR_OFF;
return NULL;
}
static const char *configure_checkhandler(cmd_parms *cmd, void *d_, int arg) {
dir_config_rec *d = d_;
d->checkhandler = arg ? MODDIR_ON : MODDIR_OFF;
return NULL;
}
static const char *configure_redirect(cmd_parms *cmd, void *d_, const char *arg1) {
dir_config_rec *d = d_;
int status;
if (!strcasecmp(arg1, "ON"))
status = HTTP_MOVED_TEMPORARILY;
else if (!strcasecmp(arg1, "OFF"))
status = REDIRECT_OFF;
else if (!strcasecmp(arg1, "permanent"))
status = HTTP_MOVED_PERMANENTLY;
else if (!strcasecmp(arg1, "temp"))
status = HTTP_MOVED_TEMPORARILY;
else if (!strcasecmp(arg1, "seeother"))
status = HTTP_SEE_OTHER;
else if (apr_isdigit(*arg1)) {
status = atoi(arg1);
if (!ap_is_HTTP_REDIRECT(status)) {
return "DirectoryIndexRedirect only accepts values between 300 and 399";
}
} else {
return "DirectoryIndexRedirect ON|OFF|permanent|temp|seeother|3xx";
}
d->redirect_index = status;
return NULL;
}
static const command_rec dir_cmds[] = {
AP_INIT_TAKE1("FallbackResource", ap_set_string_slot,
(void*)APR_OFFSETOF(dir_config_rec, dflt),
DIR_CMD_PERMS, "Set a default handler"),
AP_INIT_RAW_ARGS("DirectoryIndex", add_index, NULL, DIR_CMD_PERMS,
"a list of file names"),
AP_INIT_FLAG("DirectorySlash", configure_slash, NULL, DIR_CMD_PERMS,
"On or Off"),
AP_INIT_FLAG("DirectoryCheckHandler", configure_checkhandler, NULL, DIR_CMD_PERMS,
"On or Off"),
AP_INIT_TAKE1("DirectoryIndexRedirect", configure_redirect,
NULL, DIR_CMD_PERMS, "On, Off, or a 3xx status code."),
{NULL}
};
static void *create_dir_config(apr_pool_t *p, char *dummy) {
dir_config_rec *new = apr_pcalloc(p, sizeof(dir_config_rec));
new->index_names = NULL;
new->do_slash = MODDIR_UNSET;
new->checkhandler = MODDIR_UNSET;
new->redirect_index = REDIRECT_UNSET;
return (void *) new;
}
static void *merge_dir_configs(apr_pool_t *p, void *basev, void *addv) {
dir_config_rec *new = apr_pcalloc(p, sizeof(dir_config_rec));
dir_config_rec *base = (dir_config_rec *)basev;
dir_config_rec *add = (dir_config_rec *)addv;
new->index_names = add->index_names ? add->index_names : base->index_names;
new->do_slash =
(add->do_slash == MODDIR_UNSET) ? base->do_slash : add->do_slash;
new->checkhandler =
(add->checkhandler == MODDIR_UNSET) ? base->checkhandler : add->checkhandler;
new->redirect_index=
(add->redirect_index == REDIRECT_UNSET) ? base->redirect_index : add->redirect_index;
new->dflt = add->dflt ? add->dflt : base->dflt;
return new;
}
static int fixup_dflt(request_rec *r) {
dir_config_rec *d = ap_get_module_config(r->per_dir_config, &dir_module);
const char *name_ptr;
request_rec *rr;
int error_notfound = 0;
name_ptr = d->dflt;
if ((name_ptr == NULL) || !(strcasecmp(name_ptr,"disabled"))) {
return DECLINED;
}
if (r->args != NULL) {
name_ptr = apr_pstrcat(r->pool, name_ptr, "?", r->args, NULL);
}
rr = ap_sub_req_lookup_uri(name_ptr, r, r->output_filters);
if (rr->status == HTTP_OK
&& ( (rr->handler && !strcmp(rr->handler, "proxy-server"))
|| rr->finfo.filetype == APR_REG)) {
ap_internal_fast_redirect(rr, r);
return OK;
} else if (ap_is_HTTP_REDIRECT(rr->status)) {
apr_pool_join(r->pool, rr->pool);
r->notes = apr_table_overlay(r->pool, r->notes, rr->notes);
r->headers_out = apr_table_overlay(r->pool, r->headers_out,
rr->headers_out);
r->err_headers_out = apr_table_overlay(r->pool, r->err_headers_out,
rr->err_headers_out);
error_notfound = rr->status;
} else if (rr->status && rr->status != HTTP_NOT_FOUND
&& rr->status != HTTP_OK) {
error_notfound = rr->status;
}
ap_destroy_sub_req(rr);
if (error_notfound) {
return error_notfound;
}
return DECLINED;
}
static int fixup_dir(request_rec *r) {
dir_config_rec *d;
char *dummy_ptr[1];
char **names_ptr;
int num_names;
int error_notfound = 0;
if (!r->handler) {
r->handler = DIR_MAGIC_TYPE;
}
if (r->path_info && *r->path_info) {
return DECLINED;
}
d = (dir_config_rec *)ap_get_module_config(r->per_dir_config,
&dir_module);
if (r->uri[0] == '\0' || r->uri[strlen(r->uri) - 1] != '/') {
char *ifile;
if (!d->do_slash) {
return DECLINED;
}
if ((r->method_number != M_GET)
&& apr_table_get(r->subprocess_env, "redirect-carefully")) {
return DECLINED;
}
if (r->args != NULL) {
ifile = apr_pstrcat(r->pool, ap_escape_uri(r->pool, r->uri),
"/?", r->args, NULL);
} else {
ifile = apr_pstrcat(r->pool, ap_escape_uri(r->pool, r->uri),
"/", NULL);
}
apr_table_setn(r->headers_out, "Location",
ap_construct_url(r->pool, ifile, r));
return HTTP_MOVED_PERMANENTLY;
}
if (!strcmp(r->handler, REWRITE_REDIRECT_HANDLER_NAME)) {
if (!strcmp(r->content_type, DIR_MAGIC_TYPE)) {
r->content_type = NULL;
}
return DECLINED;
}
if (d->checkhandler == MODDIR_ON && strcmp(r->handler, DIR_MAGIC_TYPE)) {
if (!strcmp(r->content_type, DIR_MAGIC_TYPE)) {
r->content_type = NULL;
}
return DECLINED;
}
if (d->index_names) {
names_ptr = (char **)d->index_names->elts;
num_names = d->index_names->nelts;
} else {
dummy_ptr[0] = AP_DEFAULT_INDEX;
names_ptr = dummy_ptr;
num_names = 1;
}
for (; num_names; ++names_ptr, --num_names) {
char *name_ptr = *names_ptr;
request_rec *rr;
if (r->args != NULL) {
name_ptr = apr_pstrcat(r->pool, name_ptr, "?", r->args, NULL);
}
rr = ap_sub_req_lookup_uri(name_ptr, r, r->output_filters);
if (rr->status == HTTP_OK
&& ( (rr->handler && !strcmp(rr->handler, "proxy-server"))
|| rr->finfo.filetype == APR_REG)) {
if (ap_is_HTTP_REDIRECT(d->redirect_index)) {
apr_table_setn(r->headers_out, "Location", ap_construct_url(r->pool, rr->uri, r));
return d->redirect_index;
}
ap_internal_fast_redirect(rr, r);
return OK;
}
if (ap_is_HTTP_REDIRECT(rr->status)
|| (rr->status == HTTP_NOT_ACCEPTABLE && num_names == 1)
|| (rr->status == HTTP_UNAUTHORIZED && num_names == 1)) {
apr_pool_join(r->pool, rr->pool);
error_notfound = rr->status;
r->notes = apr_table_overlay(r->pool, r->notes, rr->notes);
r->headers_out = apr_table_overlay(r->pool, r->headers_out,
rr->headers_out);
r->err_headers_out = apr_table_overlay(r->pool, r->err_headers_out,
rr->err_headers_out);
return error_notfound;
}
if (rr->status && rr->status != HTTP_NOT_FOUND
&& rr->status != HTTP_OK) {
error_notfound = rr->status;
}
ap_destroy_sub_req(rr);
}
if (error_notfound) {
return error_notfound;
}
apr_table_setn(r->notes, "dir-index-names",
d->index_names ?
apr_array_pstrcat(r->pool, d->index_names, ',') :
AP_DEFAULT_INDEX);
return DECLINED;
}
static int dir_fixups(request_rec *r) {
if (r->finfo.filetype == APR_DIR) {
return fixup_dir(r);
} else if ((r->finfo.filetype == APR_NOFILE) && (r->handler == NULL)) {
return fixup_dflt(r);
}
return DECLINED;
}
static void register_hooks(apr_pool_t *p) {
ap_hook_fixups(dir_fixups,NULL,NULL,APR_HOOK_LAST);
}
AP_DECLARE_MODULE(dir) = {
STANDARD20_MODULE_STUFF,
create_dir_config,
merge_dir_configs,
NULL,
NULL,
dir_cmds,
register_hooks
};