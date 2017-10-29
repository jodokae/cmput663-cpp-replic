#include "apr.h"
#include "apr_strings.h"
#define APR_WANT_STRFUNC
#include "apr_want.h"
#if APR_HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#include "ap_config.h"
#include "httpd.h"
#include "http_config.h"
#include "util_script.h"
#include "http_log.h"
#include "http_request.h"
#include "http_protocol.h"
#include "apr_lib.h"
#define DIR_CMD_PERMS OR_INDEXES
#define DEFAULT_METADIR ".web"
#define DEFAULT_METASUFFIX ".meta"
#define DEFAULT_METAFILES 0
module AP_MODULE_DECLARE_DATA cern_meta_module;
typedef struct {
const char *metadir;
const char *metasuffix;
int metafiles;
} cern_meta_dir_config;
static void *create_cern_meta_dir_config(apr_pool_t *p, char *dummy) {
cern_meta_dir_config *new =
(cern_meta_dir_config *) apr_palloc(p, sizeof(cern_meta_dir_config));
new->metadir = NULL;
new->metasuffix = NULL;
new->metafiles = DEFAULT_METAFILES;
return new;
}
static void *merge_cern_meta_dir_configs(apr_pool_t *p, void *basev, void *addv) {
cern_meta_dir_config *base = (cern_meta_dir_config *) basev;
cern_meta_dir_config *add = (cern_meta_dir_config *) addv;
cern_meta_dir_config *new =
(cern_meta_dir_config *) apr_palloc(p, sizeof(cern_meta_dir_config));
new->metadir = add->metadir ? add->metadir : base->metadir;
new->metasuffix = add->metasuffix ? add->metasuffix : base->metasuffix;
new->metafiles = add->metafiles;
return new;
}
static const char *set_metadir(cmd_parms *parms, void *in_dconf, const char *arg) {
cern_meta_dir_config *dconf = in_dconf;
dconf->metadir = arg;
return NULL;
}
static const char *set_metasuffix(cmd_parms *parms, void *in_dconf, const char *arg) {
cern_meta_dir_config *dconf = in_dconf;
dconf->metasuffix = arg;
return NULL;
}
static const char *set_metafiles(cmd_parms *parms, void *in_dconf, int arg) {
cern_meta_dir_config *dconf = in_dconf;
dconf->metafiles = arg;
return NULL;
}
static const command_rec cern_meta_cmds[] = {
AP_INIT_FLAG("MetaFiles", set_metafiles, NULL, DIR_CMD_PERMS,
"Limited to 'on' or 'off'"),
AP_INIT_TAKE1("MetaDir", set_metadir, NULL, DIR_CMD_PERMS,
"the name of the directory containing meta files"),
AP_INIT_TAKE1("MetaSuffix", set_metasuffix, NULL, DIR_CMD_PERMS,
"the filename suffix for meta files"),
{NULL}
};
static int scan_meta_file(request_rec *r, apr_file_t *f) {
char w[MAX_STRING_LEN];
char *l;
int p;
apr_table_t *tmp_headers;
tmp_headers = apr_table_make(r->pool, 5);
while (apr_file_gets(w, MAX_STRING_LEN - 1, f) == APR_SUCCESS) {
p = strlen(w);
if (p > 0 && w[p - 1] == '\n') {
if (p > 1 && w[p - 2] == '\015')
w[p - 2] = '\0';
else
w[p - 1] = '\0';
}
if (w[0] == '\0') {
return OK;
}
if (!(l = strchr(w, ':'))) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(01560)
"malformed header in meta file: %s", r->filename);
return HTTP_INTERNAL_SERVER_ERROR;
}
*l++ = '\0';
while (apr_isspace(*l))
++l;
if (!strcasecmp(w, "Content-type")) {
char *tmp;
char *endp = l + strlen(l) - 1;
while (endp > l && apr_isspace(*endp))
*endp-- = '\0';
tmp = apr_pstrdup(r->pool, l);
ap_content_type_tolower(tmp);
ap_set_content_type(r, tmp);
} else if (!strcasecmp(w, "Status")) {
sscanf(l, "%d", &r->status);
r->status_line = apr_pstrdup(r->pool, l);
} else {
apr_table_set(tmp_headers, w, l);
}
}
apr_table_overlap(r->headers_out, tmp_headers, APR_OVERLAP_TABLES_SET);
return OK;
}
static int add_cern_meta_data(request_rec *r) {
char *metafilename;
char *leading_slash;
char *last_slash;
char *real_file;
char *scrap_book;
apr_file_t *f = NULL;
apr_status_t retcode;
cern_meta_dir_config *dconf;
int rv;
request_rec *rr;
dconf = ap_get_module_config(r->per_dir_config, &cern_meta_module);
if (!dconf->metafiles) {
return DECLINED;
}
if (r->finfo.filetype == APR_NOFILE) {
return DECLINED;
}
if (r->finfo.filetype == APR_DIR || r->uri[strlen(r->uri) - 1] == '/') {
return DECLINED;
}
scrap_book = apr_pstrdup(r->pool, r->filename);
leading_slash = strchr(scrap_book, '/');
last_slash = strrchr(scrap_book, '/');
if ((last_slash != NULL) && (last_slash != leading_slash)) {
real_file = last_slash;
real_file++;
*last_slash = '\0';
} else {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(01561)
"internal error in mod_cern_meta: %s", r->filename);
return DECLINED;
}
metafilename = apr_pstrcat(r->pool, scrap_book, "/",
dconf->metadir ? dconf->metadir : DEFAULT_METADIR,
"/", real_file,
dconf->metasuffix ? dconf->metasuffix : DEFAULT_METASUFFIX,
NULL);
rr = ap_sub_req_lookup_file(metafilename, r, NULL);
if (rr->status != HTTP_OK) {
ap_destroy_sub_req(rr);
return DECLINED;
}
ap_destroy_sub_req(rr);
retcode = apr_file_open(&f, metafilename, APR_READ, APR_OS_DEFAULT, r->pool);
if (retcode != APR_SUCCESS) {
if (APR_STATUS_IS_ENOENT(retcode)) {
return DECLINED;
}
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(01562)
"meta file permissions deny server access: %s", metafilename);
return HTTP_FORBIDDEN;
}
rv = scan_meta_file(r, f);
apr_file_close(f);
return rv;
}
static void register_hooks(apr_pool_t *p) {
ap_hook_fixups(add_cern_meta_data,NULL,NULL,APR_HOOK_MIDDLE);
}
AP_DECLARE_MODULE(cern_meta) = {
STANDARD20_MODULE_STUFF,
create_cern_meta_dir_config,
merge_cern_meta_dir_configs,
NULL,
NULL,
cern_meta_cmds,
register_hooks
};
