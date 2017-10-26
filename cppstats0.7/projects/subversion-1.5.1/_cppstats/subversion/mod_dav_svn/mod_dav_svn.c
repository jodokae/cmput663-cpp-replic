#include <apr_strings.h>
#include <httpd.h>
#include <http_config.h>
#include <http_request.h>
#include <http_log.h>
#include <ap_provider.h>
#include <mod_dav.h>
#include "svn_version.h"
#include "svn_fs.h"
#include "svn_utf.h"
#include "svn_dso.h"
#include "mod_dav_svn.h"
#include "dav_svn.h"
#include "mod_authz_svn.h"
#define SVN_DEFAULT_SPECIAL_URI "!svn"
#define PATHAUTHZ_BYPASS_ARG "short_circuit"
typedef struct {
const char *special_uri;
} server_conf_t;
enum conf_flag {
CONF_FLAG_DEFAULT,
CONF_FLAG_ON,
CONF_FLAG_OFF
};
enum path_authz_conf {
CONF_PATHAUTHZ_DEFAULT,
CONF_PATHAUTHZ_ON,
CONF_PATHAUTHZ_OFF,
CONF_PATHAUTHZ_BYPASS
};
typedef struct {
const char *fs_path;
const char *repo_name;
const char *xslt_uri;
const char *fs_parent_path;
enum conf_flag autoversioning;
enum conf_flag bulk_updates;
enum path_authz_conf path_authz_method;
enum conf_flag list_parentpath;
const char *root_dir;
const char *master_uri;
const char *activities_db;
} dir_conf_t;
#define INHERIT_VALUE(parent, child, field) ((child)->field ? (child)->field : (parent)->field)
extern module AP_MODULE_DECLARE_DATA dav_svn_module;
static authz_svn__subreq_bypass_func_t pathauthz_bypass_func = NULL;
static int
init(apr_pool_t *p, apr_pool_t *plog, apr_pool_t *ptemp, server_rec *s) {
svn_error_t *serr;
ap_add_version_component(p, "SVN/" SVN_VER_NUMBER);
serr = svn_fs_initialize(p);
if (serr) {
ap_log_perror(APLOG_MARK, APLOG_ERR, serr->apr_err, p,
"mod_dav_svn: error calling svn_fs_initialize: '%s'",
serr->message ? serr->message : "(no more info)");
return HTTP_INTERNAL_SERVER_ERROR;
}
svn_utf_initialize(p);
return OK;
}
static int
init_dso(apr_pool_t *pconf, apr_pool_t *plog, apr_pool_t *ptemp) {
svn_dso_initialize();
return OK;
}
static void *
create_server_config(apr_pool_t *p, server_rec *s) {
return apr_pcalloc(p, sizeof(server_conf_t));
}
static void *
merge_server_config(apr_pool_t *p, void *base, void *overrides) {
server_conf_t *parent = base;
server_conf_t *child = overrides;
server_conf_t *newconf;
newconf = apr_pcalloc(p, sizeof(*newconf));
newconf->special_uri = INHERIT_VALUE(parent, child, special_uri);
return newconf;
}
static void *
create_dir_config(apr_pool_t *p, char *dir) {
dir_conf_t *conf = apr_pcalloc(p, sizeof(*conf));
conf->root_dir = dir;
conf->bulk_updates = CONF_FLAG_ON;
return conf;
}
static void *
merge_dir_config(apr_pool_t *p, void *base, void *overrides) {
dir_conf_t *parent = base;
dir_conf_t *child = overrides;
dir_conf_t *newconf;
newconf = apr_pcalloc(p, sizeof(*newconf));
newconf->fs_path = INHERIT_VALUE(parent, child, fs_path);
newconf->master_uri = INHERIT_VALUE(parent, child, master_uri);
newconf->activities_db = INHERIT_VALUE(parent, child, activities_db);
newconf->repo_name = INHERIT_VALUE(parent, child, repo_name);
newconf->xslt_uri = INHERIT_VALUE(parent, child, xslt_uri);
newconf->fs_parent_path = INHERIT_VALUE(parent, child, fs_parent_path);
newconf->autoversioning = INHERIT_VALUE(parent, child, autoversioning);
newconf->bulk_updates = INHERIT_VALUE(parent, child, bulk_updates);
newconf->path_authz_method = INHERIT_VALUE(parent, child, path_authz_method);
newconf->list_parentpath = INHERIT_VALUE(parent, child, list_parentpath);
newconf->root_dir = INHERIT_VALUE(child, parent, root_dir);
return newconf;
}
static const char *
SVNReposName_cmd(cmd_parms *cmd, void *config, const char *arg1) {
dir_conf_t *conf = config;
conf->repo_name = apr_pstrdup(cmd->pool, arg1);
return NULL;
}
static const char *
SVNMasterURI_cmd(cmd_parms *cmd, void *config, const char *arg1) {
dir_conf_t *conf = config;
conf->master_uri = apr_pstrdup(cmd->pool, arg1);
return NULL;
}
static const char *
SVNActivitiesDB_cmd(cmd_parms *cmd, void *config, const char *arg1) {
dir_conf_t *conf = config;
conf->activities_db = apr_pstrdup(cmd->pool, arg1);
return NULL;
}
static const char *
SVNIndexXSLT_cmd(cmd_parms *cmd, void *config, const char *arg1) {
dir_conf_t *conf = config;
conf->xslt_uri = apr_pstrdup(cmd->pool, arg1);
return NULL;
}
static const char *
SVNAutoversioning_cmd(cmd_parms *cmd, void *config, int arg) {
dir_conf_t *conf = config;
if (arg)
conf->autoversioning = CONF_FLAG_ON;
else
conf->autoversioning = CONF_FLAG_OFF;
return NULL;
}
static const char *
SVNAllowBulkUpdates_cmd(cmd_parms *cmd, void *config, int arg) {
dir_conf_t *conf = config;
if (arg)
conf->bulk_updates = CONF_FLAG_ON;
else
conf->bulk_updates = CONF_FLAG_OFF;
return NULL;
}
static const char *
SVNPathAuthz_cmd(cmd_parms *cmd, void *config, const char *arg1) {
dir_conf_t *conf = config;
if (apr_strnatcasecmp("off", arg1) == 0)
conf->path_authz_method = CONF_PATHAUTHZ_OFF;
else if (apr_strnatcasecmp(PATHAUTHZ_BYPASS_ARG,arg1) == 0) {
conf->path_authz_method = CONF_PATHAUTHZ_BYPASS;
if (pathauthz_bypass_func == NULL)
pathauthz_bypass_func=ap_lookup_provider(
AUTHZ_SVN__SUBREQ_BYPASS_PROV_GRP,
AUTHZ_SVN__SUBREQ_BYPASS_PROV_NAME,
AUTHZ_SVN__SUBREQ_BYPASS_PROV_VER);
} else
conf->path_authz_method = CONF_PATHAUTHZ_ON;
return NULL;
}
static const char *
SVNListParentPath_cmd(cmd_parms *cmd, void *config, int arg) {
dir_conf_t *conf = config;
if (arg)
conf->list_parentpath = CONF_FLAG_ON;
else
conf->list_parentpath = CONF_FLAG_OFF;
return NULL;
}
static const char *
SVNPath_cmd(cmd_parms *cmd, void *config, const char *arg1) {
dir_conf_t *conf = config;
if (conf->fs_parent_path != NULL)
return "SVNPath cannot be defined at same time as SVNParentPath.";
conf->fs_path = svn_path_internal_style(apr_pstrdup(cmd->pool, arg1),
cmd->pool);
return NULL;
}
static const char *
SVNParentPath_cmd(cmd_parms *cmd, void *config, const char *arg1) {
dir_conf_t *conf = config;
if (conf->fs_path != NULL)
return "SVNParentPath cannot be defined at same time as SVNPath.";
conf->fs_parent_path = svn_path_internal_style(apr_pstrdup(cmd->pool, arg1),
cmd->pool);
return NULL;
}
static const char *
SVNSpecialURI_cmd(cmd_parms *cmd, void *config, const char *arg1) {
server_conf_t *conf;
char *uri;
apr_size_t len;
uri = apr_pstrdup(cmd->pool, arg1);
ap_getparents(uri);
ap_no2slash(uri);
if (*uri == '/')
++uri;
len = strlen(uri);
if (len > 0 && uri[len - 1] == '/')
uri[--len] = '\0';
if (len == 0)
return "The special URI path must have at least one component.";
conf = ap_get_module_config(cmd->server->module_config,
&dav_svn_module);
conf->special_uri = uri;
return NULL;
}
const char *
dav_svn__get_fs_path(request_rec *r) {
dir_conf_t *conf;
conf = ap_get_module_config(r->per_dir_config, &dav_svn_module);
return conf->fs_path;
}
const char *
dav_svn__get_fs_parent_path(request_rec *r) {
dir_conf_t *conf;
conf = ap_get_module_config(r->per_dir_config, &dav_svn_module);
return conf->fs_parent_path;
}
AP_MODULE_DECLARE(dav_error *)
dav_svn_get_repos_path(request_rec *r,
const char *root_path,
const char **repos_path) {
const char *fs_path;
const char *fs_parent_path;
const char *repos_name;
const char *ignored_path_in_repos;
const char *ignored_cleaned_uri;
const char *ignored_relative;
int ignored_had_slash;
dav_error *derr;
fs_path = dav_svn__get_fs_path(r);
if (fs_path != NULL) {
*repos_path = fs_path;
return NULL;
}
fs_parent_path = dav_svn__get_fs_parent_path(r);
derr = dav_svn_split_uri(r, r->uri, root_path,
&ignored_cleaned_uri, &ignored_had_slash,
&repos_name,
&ignored_relative, &ignored_path_in_repos);
if (derr)
return derr;
*repos_path = svn_path_join(fs_parent_path, repos_name, r->pool);
return NULL;
}
const char *
dav_svn__get_repo_name(request_rec *r) {
dir_conf_t *conf;
conf = ap_get_module_config(r->per_dir_config, &dav_svn_module);
return conf->repo_name;
}
const char *
dav_svn__get_root_dir(request_rec *r) {
dir_conf_t *conf;
conf = ap_get_module_config(r->per_dir_config, &dav_svn_module);
return conf->root_dir;
}
const char *
dav_svn__get_master_uri(request_rec *r) {
dir_conf_t *conf;
conf = ap_get_module_config(r->per_dir_config, &dav_svn_module);
return conf->master_uri;
}
const char *
dav_svn__get_xslt_uri(request_rec *r) {
dir_conf_t *conf;
conf = ap_get_module_config(r->per_dir_config, &dav_svn_module);
return conf->xslt_uri;
}
const char *
dav_svn__get_special_uri(request_rec *r) {
server_conf_t *conf;
conf = ap_get_module_config(r->server->module_config,
&dav_svn_module);
return conf->special_uri ? conf->special_uri : SVN_DEFAULT_SPECIAL_URI;
}
svn_boolean_t
dav_svn__get_autoversioning_flag(request_rec *r) {
dir_conf_t *conf;
conf = ap_get_module_config(r->per_dir_config, &dav_svn_module);
return conf->autoversioning == CONF_FLAG_ON;
}
svn_boolean_t
dav_svn__get_bulk_updates_flag(request_rec *r) {
dir_conf_t *conf;
conf = ap_get_module_config(r->per_dir_config, &dav_svn_module);
return conf->bulk_updates == CONF_FLAG_ON;
}
svn_boolean_t
dav_svn__get_pathauthz_flag(request_rec *r) {
dir_conf_t *conf;
conf = ap_get_module_config(r->per_dir_config, &dav_svn_module);
return conf->path_authz_method != CONF_PATHAUTHZ_OFF;
}
authz_svn__subreq_bypass_func_t
dav_svn__get_pathauthz_bypass(request_rec *r) {
dir_conf_t *conf;
conf = ap_get_module_config(r->per_dir_config, &dav_svn_module);
if (conf->path_authz_method==CONF_PATHAUTHZ_BYPASS)
return pathauthz_bypass_func;
return NULL;
}
svn_boolean_t
dav_svn__get_list_parentpath_flag(request_rec *r) {
dir_conf_t *conf;
conf = ap_get_module_config(r->per_dir_config, &dav_svn_module);
return conf->list_parentpath == CONF_FLAG_ON;
}
const char *
dav_svn__get_activities_db(request_rec *r) {
dir_conf_t *conf;
conf = ap_get_module_config(r->per_dir_config, &dav_svn_module);
return conf->activities_db;
}
static void
merge_xml_filter_insert(request_rec *r) {
if ((r->method_number == M_MERGE)
|| (r->method_number == M_DELETE)) {
dir_conf_t *conf;
conf = ap_get_module_config(r->per_dir_config, &dav_svn_module);
if (conf->fs_path || conf->fs_parent_path) {
ap_add_input_filter("SVN-MERGE", NULL, r, r->connection);
}
}
}
typedef struct {
apr_bucket_brigade *bb;
apr_xml_parser *parser;
apr_pool_t *pool;
} merge_ctx_t;
static apr_status_t
merge_xml_in_filter(ap_filter_t *f,
apr_bucket_brigade *bb,
ap_input_mode_t mode,
apr_read_type_e block,
apr_off_t readbytes) {
apr_status_t rv;
request_rec *r = f->r;
merge_ctx_t *ctx = f->ctx;
apr_bucket *bucket;
int seen_eos = 0;
if ((r->method_number != M_MERGE)
&& (r->method_number != M_DELETE)) {
ap_remove_input_filter(f);
return ap_get_brigade(f->next, bb, mode, block, readbytes);
}
if (!ctx) {
f->ctx = ctx = apr_palloc(r->pool, sizeof(*ctx));
ctx->parser = apr_xml_parser_create(r->pool);
ctx->bb = apr_brigade_create(r->pool, r->connection->bucket_alloc);
apr_pool_create(&ctx->pool, r->pool);
}
rv = ap_get_brigade(f->next, ctx->bb, mode, block, readbytes);
if (rv != APR_SUCCESS)
return rv;
for (bucket = APR_BRIGADE_FIRST(ctx->bb);
bucket != APR_BRIGADE_SENTINEL(ctx->bb);
bucket = APR_BUCKET_NEXT(bucket)) {
const char *data;
apr_size_t len;
if (APR_BUCKET_IS_EOS(bucket)) {
seen_eos = 1;
break;
}
if (APR_BUCKET_IS_METADATA(bucket))
continue;
rv = apr_bucket_read(bucket, &data, &len, APR_BLOCK_READ);
if (rv != APR_SUCCESS)
return rv;
rv = apr_xml_parser_feed(ctx->parser, data, len);
if (rv != APR_SUCCESS) {
(void) apr_xml_parser_done(ctx->parser, NULL);
break;
}
}
APR_BRIGADE_CONCAT(bb, ctx->bb);
if (seen_eos) {
apr_xml_doc *pdoc;
ap_remove_input_filter(f);
rv = apr_xml_parser_done(ctx->parser, &pdoc);
if (rv == APR_SUCCESS) {
#if APR_CHARSET_EBCDIC
apr_xml_parser_convert_doc(r->pool, pdoc, ap_hdrs_from_ascii);
#endif
rv = apr_pool_userdata_set(pdoc, "svn-request-body",
NULL, r->pool);
if (rv != APR_SUCCESS)
return rv;
}
}
return APR_SUCCESS;
}
static const command_rec cmds[] = {
AP_INIT_TAKE1("SVNPath", SVNPath_cmd, NULL, ACCESS_CONF,
"specifies the location in the filesystem for a Subversion "
"repository's files."),
AP_INIT_TAKE1("SVNSpecialURI", SVNSpecialURI_cmd, NULL, RSRC_CONF,
"specify the URI component for special Subversion "
"resources"),
AP_INIT_TAKE1("SVNReposName", SVNReposName_cmd, NULL, ACCESS_CONF,
"specify the name of a Subversion repository"),
AP_INIT_TAKE1("SVNIndexXSLT", SVNIndexXSLT_cmd, NULL, ACCESS_CONF,
"specify the URI of an XSL transformation for "
"directory indexes"),
AP_INIT_TAKE1("SVNParentPath", SVNParentPath_cmd, NULL, ACCESS_CONF,
"specifies the location in the filesystem whose "
"subdirectories are assumed to be Subversion repositories."),
AP_INIT_FLAG("SVNAutoversioning", SVNAutoversioning_cmd, NULL,
ACCESS_CONF|RSRC_CONF, "turn on deltaV autoversioning."),
AP_INIT_TAKE1("SVNPathAuthz", SVNPathAuthz_cmd, NULL,
ACCESS_CONF|RSRC_CONF,
"control path-based authz by enabling subrequests(On,default), "
"disabling subrequests(Off), or"
"querying mod_authz_svn directly(" PATHAUTHZ_BYPASS_ARG ")"),
AP_INIT_FLAG("SVNListParentPath", SVNListParentPath_cmd, NULL,
ACCESS_CONF|RSRC_CONF, "allow GET of SVNParentPath."),
AP_INIT_TAKE1("SVNMasterURI", SVNMasterURI_cmd, NULL, ACCESS_CONF,
"specifies a URI to access a master Subversion repository"),
AP_INIT_TAKE1("SVNActivitiesDB", SVNActivitiesDB_cmd, NULL, ACCESS_CONF,
"specifies the location in the filesystem in which the "
"activities database(s) should be stored"),
AP_INIT_FLAG("SVNAllowBulkUpdates", SVNAllowBulkUpdates_cmd, NULL,
ACCESS_CONF|RSRC_CONF,
"enables support for bulk update-style requests (as opposed to "
"only skeletal reports that require additional per-file "
"downloads."),
{ NULL }
};
static dav_provider provider = {
&dav_svn__hooks_repository,
&dav_svn__hooks_propdb,
&dav_svn__hooks_locks,
&dav_svn__hooks_vsn,
NULL,
NULL
};
static void
register_hooks(apr_pool_t *pconf) {
ap_hook_pre_config(init_dso, NULL, NULL, APR_HOOK_REALLY_FIRST);
ap_hook_post_config(init, NULL, NULL, APR_HOOK_MIDDLE);
dav_register_provider(pconf, "svn", &provider);
ap_register_input_filter("SVN-MERGE", merge_xml_in_filter, NULL,
AP_FTYPE_RESOURCE);
ap_hook_insert_filter(merge_xml_filter_insert, NULL, NULL,
APR_HOOK_MIDDLE);
dav_hook_gather_propsets(dav_svn__gather_propsets, NULL, NULL,
APR_HOOK_MIDDLE);
dav_hook_find_liveprop(dav_svn__find_liveprop, NULL, NULL, APR_HOOK_MIDDLE);
dav_hook_insert_all_liveprops(dav_svn__insert_all_liveprops, NULL, NULL,
APR_HOOK_MIDDLE);
dav_register_liveprop_group(pconf, &dav_svn__liveprop_group);
ap_register_output_filter("LocationRewrite", dav_svn__location_header_filter,
NULL, AP_FTYPE_CONTENT_SET);
ap_register_output_filter("ReposRewrite", dav_svn__location_body_filter,
NULL, AP_FTYPE_CONTENT_SET);
ap_register_input_filter("IncomingRewrite", dav_svn__location_in_filter,
NULL, AP_FTYPE_CONTENT_SET);
ap_hook_fixups(dav_svn__proxy_merge_fixup, NULL, NULL, APR_HOOK_MIDDLE);
}
module AP_MODULE_DECLARE_DATA dav_svn_module = {
STANDARD20_MODULE_STUFF,
create_dir_config,
merge_dir_config,
create_server_config,
merge_server_config,
cmds,
register_hooks,
};
