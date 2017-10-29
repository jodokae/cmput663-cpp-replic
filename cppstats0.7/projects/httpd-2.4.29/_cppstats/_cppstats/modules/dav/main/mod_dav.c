#include "apr_strings.h"
#include "apr_lib.h"
#define APR_WANT_STRFUNC
#include "apr_want.h"
#include "httpd.h"
#include "http_config.h"
#include "http_core.h"
#include "http_log.h"
#include "http_main.h"
#include "http_protocol.h"
#include "http_request.h"
#include "util_script.h"
#include "mod_dav.h"
#include "ap_provider.h"
#define DAV_DEFAULT_PROVIDER "filesystem"
#define DAV_HANDLER_NAME "dav-handler"
APLOG_USE_MODULE(dav);
enum {
DAV_ENABLED_UNSET = 0,
DAV_ENABLED_OFF,
DAV_ENABLED_ON
};
typedef struct {
const char *provider_name;
const dav_provider *provider;
const char *dir;
int locktimeout;
int allow_depthinfinity;
} dav_dir_conf;
typedef struct {
int unused;
} dav_server_conf;
#define DAV_INHERIT_VALUE(parent, child, field) ((child)->field ? (child)->field : (parent)->field)
extern module DAV_DECLARE_DATA dav_module;
enum {
DAV_M_BIND = 0,
DAV_M_SEARCH,
DAV_M_LAST
};
static int dav_methods[DAV_M_LAST];
static int dav_init_handler(apr_pool_t *p, apr_pool_t *plog, apr_pool_t *ptemp,
server_rec *s) {
dav_methods[DAV_M_BIND] = ap_method_register(p, "BIND");
dav_methods[DAV_M_SEARCH] = ap_method_register(p, "SEARCH");
return OK;
}
static void *dav_create_server_config(apr_pool_t *p, server_rec *s) {
dav_server_conf *newconf;
newconf = (dav_server_conf *)apr_pcalloc(p, sizeof(*newconf));
return newconf;
}
static void *dav_merge_server_config(apr_pool_t *p, void *base, void *overrides) {
#if 0
dav_server_conf *child = overrides;
#endif
dav_server_conf *newconf;
newconf = (dav_server_conf *)apr_pcalloc(p, sizeof(*newconf));
return newconf;
}
static void *dav_create_dir_config(apr_pool_t *p, char *dir) {
dav_dir_conf *conf;
conf = (dav_dir_conf *)apr_pcalloc(p, sizeof(*conf));
if (dir != NULL) {
char *d;
apr_size_t l;
l = strlen(dir);
d = apr_pstrmemdup(p, dir, l);
if (l > 1 && d[l - 1] == '/')
d[l - 1] = '\0';
conf->dir = d;
}
return conf;
}
static void *dav_merge_dir_config(apr_pool_t *p, void *base, void *overrides) {
dav_dir_conf *parent = base;
dav_dir_conf *child = overrides;
dav_dir_conf *newconf = (dav_dir_conf *)apr_pcalloc(p, sizeof(*newconf));
newconf->provider_name = DAV_INHERIT_VALUE(parent, child, provider_name);
newconf->provider = DAV_INHERIT_VALUE(parent, child, provider);
if (parent->provider_name != NULL) {
if (child->provider_name == NULL) {
ap_log_error(APLOG_MARK, APLOG_ERR, 0, NULL, APLOGNO(00578)
"\"DAV Off\" cannot be used to turn off a subtree "
"of a DAV-enabled location.");
} else if (strcasecmp(child->provider_name,
parent->provider_name) != 0) {
ap_log_error(APLOG_MARK, APLOG_ERR, 0, NULL, APLOGNO(00579)
"A subtree cannot specify a different DAV provider "
"than its parent.");
}
}
newconf->locktimeout = DAV_INHERIT_VALUE(parent, child, locktimeout);
newconf->dir = DAV_INHERIT_VALUE(parent, child, dir);
newconf->allow_depthinfinity = DAV_INHERIT_VALUE(parent, child,
allow_depthinfinity);
return newconf;
}
DAV_DECLARE(const char *) dav_get_provider_name(request_rec *r) {
dav_dir_conf *conf = ap_get_module_config(r->per_dir_config, &dav_module);
return conf ? conf->provider_name : NULL;
}
static const dav_provider *dav_get_provider(request_rec *r) {
dav_dir_conf *conf;
conf = ap_get_module_config(r->per_dir_config, &dav_module);
return conf->provider;
}
DAV_DECLARE(const dav_hooks_locks *) dav_get_lock_hooks(request_rec *r) {
return dav_get_provider(r)->locks;
}
DAV_DECLARE(const dav_hooks_propdb *) dav_get_propdb_hooks(request_rec *r) {
return dav_get_provider(r)->propdb;
}
DAV_DECLARE(const dav_hooks_vsn *) dav_get_vsn_hooks(request_rec *r) {
return dav_get_provider(r)->vsn;
}
DAV_DECLARE(const dav_hooks_binding *) dav_get_binding_hooks(request_rec *r) {
return dav_get_provider(r)->binding;
}
DAV_DECLARE(const dav_hooks_search *) dav_get_search_hooks(request_rec *r) {
return dav_get_provider(r)->search;
}
static const char *dav_cmd_dav(cmd_parms *cmd, void *config, const char *arg1) {
dav_dir_conf *conf = (dav_dir_conf *)config;
if (strcasecmp(arg1, "on") == 0) {
conf->provider_name = DAV_DEFAULT_PROVIDER;
} else if (strcasecmp(arg1, "off") == 0) {
conf->provider_name = NULL;
conf->provider = NULL;
} else {
conf->provider_name = arg1;
}
if (conf->provider_name != NULL) {
conf->provider = dav_lookup_provider(conf->provider_name);
if (conf->provider == NULL) {
return apr_psprintf(cmd->pool,
"Unknown DAV provider: %s",
conf->provider_name);
}
}
return NULL;
}
static const char *dav_cmd_davdepthinfinity(cmd_parms *cmd, void *config,
int arg) {
dav_dir_conf *conf = (dav_dir_conf *)config;
if (arg)
conf->allow_depthinfinity = DAV_ENABLED_ON;
else
conf->allow_depthinfinity = DAV_ENABLED_OFF;
return NULL;
}
static const char *dav_cmd_davmintimeout(cmd_parms *cmd, void *config,
const char *arg1) {
dav_dir_conf *conf = (dav_dir_conf *)config;
conf->locktimeout = atoi(arg1);
if (conf->locktimeout < 0)
return "DAVMinTimeout requires a non-negative integer.";
return NULL;
}
static int dav_error_response(request_rec *r, int status, const char *body) {
r->status = status;
r->status_line = ap_get_status_line(status);
ap_set_content_type(r, "text/html; charset=ISO-8859-1");
ap_rvputs(r,
DAV_RESPONSE_BODY_1,
r->status_line,
DAV_RESPONSE_BODY_2,
&r->status_line[4],
DAV_RESPONSE_BODY_3,
body,
DAV_RESPONSE_BODY_4,
ap_psignature("<hr />\n", r),
DAV_RESPONSE_BODY_5,
NULL);
return DONE;
}
static int dav_error_response_tag(request_rec *r,
dav_error *err) {
r->status = err->status;
ap_set_content_type(r, DAV_XML_CONTENT_TYPE);
ap_rputs(DAV_XML_HEADER DEBUG_CR
"<D:error xmlns:D=\"DAV:\"", r);
if (err->desc != NULL) {
ap_rputs(" xmlns:m=\"http://apache.org/dav/xmlns\"", r);
}
if (err->childtags) {
if (err->namespace != NULL) {
ap_rprintf(r,
" xmlns:C=\"%s\">" DEBUG_CR
"<C:%s>%s</C:%s>" DEBUG_CR,
err->namespace,
err->tagname, err->childtags, err->tagname);
} else {
ap_rprintf(r,
">" DEBUG_CR
"<D:%s>%s</D:%s>" DEBUG_CR,
err->tagname, err->childtags, err->tagname);
}
} else {
if (err->namespace != NULL) {
ap_rprintf(r,
" xmlns:C=\"%s\">" DEBUG_CR
"<C:%s/>" DEBUG_CR,
err->namespace, err->tagname);
} else {
ap_rprintf(r,
">" DEBUG_CR
"<D:%s/>" DEBUG_CR, err->tagname);
}
}
if (err->desc != NULL) {
ap_rprintf(r,
"<m:human-readable errcode=\"%d\">" DEBUG_CR
"%s" DEBUG_CR
"</m:human-readable>" DEBUG_CR,
err->error_id,
apr_xml_quote_string(r->pool, err->desc, 0));
}
ap_rputs("</D:error>" DEBUG_CR, r);
return DONE;
}
static const char *dav_xml_escape_uri(apr_pool_t *p, const char *uri) {
const char *e_uri = ap_escape_uri(p, uri);
if (ap_strchr_c(e_uri, '&') == NULL)
return e_uri;
return apr_xml_quote_string(p, e_uri, 0);
}
DAV_DECLARE(void) dav_send_one_response(dav_response *response,
apr_bucket_brigade *bb,
request_rec *r,
apr_pool_t *pool) {
apr_text *t = NULL;
if (response->propresult.xmlns == NULL) {
ap_fputs(r->output_filters, bb, "<D:response>");
} else {
ap_fputs(r->output_filters, bb, "<D:response");
for (t = response->propresult.xmlns; t; t = t->next) {
ap_fputs(r->output_filters, bb, t->text);
}
ap_fputc(r->output_filters, bb, '>');
}
ap_fputstrs(r->output_filters, bb,
DEBUG_CR "<D:href>",
dav_xml_escape_uri(pool, response->href),
"</D:href>" DEBUG_CR,
NULL);
if (response->propresult.propstats == NULL) {
ap_fputstrs(r->output_filters, bb,
"<D:status>HTTP/1.1 ",
ap_get_status_line(response->status),
"</D:status>" DEBUG_CR,
NULL);
} else {
for (t = response->propresult.propstats; t; t = t->next) {
ap_fputs(r->output_filters, bb, t->text);
}
}
if (response->desc != NULL) {
ap_fputstrs(r->output_filters, bb,
"<D:responsedescription>",
response->desc,
"</D:responsedescription>" DEBUG_CR,
NULL);
}
ap_fputs(r->output_filters, bb, "</D:response>" DEBUG_CR);
}
DAV_DECLARE(void) dav_begin_multistatus(apr_bucket_brigade *bb,
request_rec *r, int status,
apr_array_header_t *namespaces) {
r->status = status;
ap_set_content_type(r, DAV_XML_CONTENT_TYPE);
ap_fputs(r->output_filters, bb, DAV_XML_HEADER DEBUG_CR
"<D:multistatus xmlns:D=\"DAV:\"");
if (namespaces != NULL) {
int i;
for (i = namespaces->nelts; i--; ) {
ap_fprintf(r->output_filters, bb, " xmlns:ns%d=\"%s\"", i,
APR_XML_GET_URI_ITEM(namespaces, i));
}
}
ap_fputs(r->output_filters, bb, ">" DEBUG_CR);
}
DAV_DECLARE(apr_status_t) dav_finish_multistatus(request_rec *r,
apr_bucket_brigade *bb) {
apr_bucket *b;
ap_fputs(r->output_filters, bb, "</D:multistatus>" DEBUG_CR);
b = apr_bucket_eos_create(r->connection->bucket_alloc);
APR_BRIGADE_INSERT_TAIL(bb, b);
return ap_pass_brigade(r->output_filters, bb);
}
DAV_DECLARE(void) dav_send_multistatus(request_rec *r, int status,
dav_response *first,
apr_array_header_t *namespaces) {
apr_pool_t *subpool;
apr_bucket_brigade *bb = apr_brigade_create(r->pool,
r->connection->bucket_alloc);
dav_begin_multistatus(bb, r, status, namespaces);
apr_pool_create(&subpool, r->pool);
for (; first != NULL; first = first->next) {
apr_pool_clear(subpool);
dav_send_one_response(first, bb, r, subpool);
}
apr_pool_destroy(subpool);
dav_finish_multistatus(r, bb);
}
static void dav_log_err(request_rec *r, dav_error *err, int level) {
dav_error *errscan;
for (errscan = err; errscan != NULL; errscan = errscan->prev) {
if (errscan->desc == NULL)
continue;
ap_log_rerror(APLOG_MARK, level, errscan->aprerr, r, "%s [%d, #%d]",
errscan->desc, errscan->status, errscan->error_id);
}
}
DAV_DECLARE(int) dav_handle_err(request_rec *r, dav_error *err,
dav_response *response) {
dav_log_err(r, err, APLOG_ERR);
if (!ap_is_HTTP_VALID_RESPONSE(err->status)) {
return AP_FILTER_ERROR;
}
if (response == NULL) {
dav_error *stackerr = err;
apr_table_setn(r->notes, "verbose-error-to", "*");
while (stackerr != NULL && stackerr->tagname == NULL)
stackerr = stackerr->prev;
if (stackerr != NULL && stackerr->tagname != NULL)
return dav_error_response_tag(r, stackerr);
return err->status;
}
dav_send_multistatus(r, err->status, response, NULL);
return DONE;
}
static int dav_created(request_rec *r, const char *locn, const char *what,
int replaced) {
const char *body;
if (locn == NULL) {
locn = r->unparsed_uri;
}
if (replaced) {
return HTTP_NO_CONTENT;
}
apr_table_setn(r->headers_out, "Location", ap_construct_url(r->pool, locn, r));
body = apr_psprintf(r->pool, "%s %s has been created.",
what, ap_escape_html(r->pool, locn));
return dav_error_response(r, HTTP_CREATED, body);
}
DAV_DECLARE(int) dav_get_depth(request_rec *r, int def_depth) {
const char *depth = apr_table_get(r->headers_in, "Depth");
if (depth == NULL) {
return def_depth;
}
if (strcasecmp(depth, "infinity") == 0) {
return DAV_INFINITY;
} else if (strcmp(depth, "0") == 0) {
return 0;
} else if (strcmp(depth, "1") == 0) {
return 1;
}
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(00580)
"An invalid Depth header was specified.");
return -1;
}
static int dav_get_overwrite(request_rec *r) {
const char *overwrite = apr_table_get(r->headers_in, "Overwrite");
if (overwrite == NULL) {
return 1;
}
if ((*overwrite == 'F' || *overwrite == 'f') && overwrite[1] == '\0') {
return 0;
}
if ((*overwrite == 'T' || *overwrite == 't') && overwrite[1] == '\0') {
return 1;
}
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(00581)
"An invalid Overwrite header was specified.");
return -1;
}
static dav_error *dav_get_resource(request_rec *r, int label_allowed,
int use_checked_in, dav_resource **res_p) {
dav_dir_conf *conf;
const char *label = NULL;
dav_error *err;
if (label_allowed) {
label = apr_table_get(r->headers_in, "label");
}
conf = ap_get_module_config(r->per_dir_config, &dav_module);
if (conf->provider == NULL) {
return dav_new_error(r->pool, HTTP_METHOD_NOT_ALLOWED, 0, 0,
apr_psprintf(r->pool,
"DAV not enabled for %s",
ap_escape_html(r->pool, r->uri)));
}
err = (*conf->provider->repos->get_resource)(r, conf->dir,
label, use_checked_in,
res_p);
if (err != NULL) {
err = dav_push_error(r->pool, err->status, 0,
"Could not fetch resource information.", err);
return err;
}
if (*res_p == NULL) {
return dav_new_error(r->pool, HTTP_NOT_FOUND, 0, 0,
apr_psprintf(r->pool,
"The provider did not define a "
"resource for %s.",
ap_escape_html(r->pool, r->uri)));
}
dav_add_vary_header(r, r, *res_p);
return NULL;
}
static dav_error * dav_open_lockdb(request_rec *r, int ro, dav_lockdb **lockdb) {
const dav_hooks_locks *hooks = DAV_GET_HOOKS_LOCKS(r);
if (hooks == NULL) {
*lockdb = NULL;
return NULL;
}
return (*hooks->open_lockdb)(r, ro, 0, lockdb);
}
static int dav_parse_range(request_rec *r,
apr_off_t *range_start, apr_off_t *range_end) {
const char *range_c;
char *range;
char *dash;
char *slash;
char *errp;
range_c = apr_table_get(r->headers_in, "content-range");
if (range_c == NULL)
return 0;
range = apr_pstrdup(r->pool, range_c);
if (strncasecmp(range, "bytes ", 6) != 0
|| (dash = ap_strchr(range, '-')) == NULL
|| (slash = ap_strchr(range, '/')) == NULL) {
return -1;
}
*dash++ = *slash++ = '\0';
if (apr_strtoff(range_start, range + 6, &errp, 10)
|| *errp || *range_start < 0) {
return -1;
}
if (apr_strtoff(range_end, dash, &errp, 10)
|| *errp || *range_end < 0 || *range_end < *range_start) {
return -1;
}
if (*slash != '*') {
apr_off_t dummy;
if (apr_strtoff(&dummy, slash, &errp, 10)
|| *errp || dummy <= *range_end) {
return -1;
}
}
return 1;
}
static int dav_method_get(request_rec *r) {
dav_resource *resource;
dav_error *err;
int status;
err = dav_get_resource(r, 1 , 0 ,
&resource);
if (err != NULL)
return dav_handle_err(r, err, NULL);
if (!resource->exists) {
return HTTP_NOT_FOUND;
}
if ((err = (*resource->hooks->set_headers)(r, resource)) != NULL) {
err = dav_push_error(r->pool, err->status, 0,
"Unable to set up HTTP headers.",
err);
return dav_handle_err(r, err, NULL);
}
status = ap_meets_conditions(r);
if (status) {
return status;
}
if (r->header_only) {
return DONE;
}
if ((err = (*resource->hooks->deliver)(resource,
r->output_filters)) != NULL) {
err = dav_push_error(r->pool, err->status, 0,
"Unable to deliver content.",
err);
return dav_handle_err(r, err, NULL);
}
return DONE;
}
static int dav_method_post(request_rec *r) {
dav_resource *resource;
dav_error *err;
err = dav_get_resource(r, 0 , 0 ,
&resource);
if (err != NULL)
return dav_handle_err(r, err, NULL);
if ((err = dav_validate_request(r, resource, 0, NULL, NULL,
DAV_VALIDATE_RESOURCE, NULL)) != NULL) {
return dav_handle_err(r, err, NULL);
}
return DECLINED;
}
static int dav_method_put(request_rec *r) {
dav_resource *resource;
int resource_state;
dav_auto_version_info av_info;
const dav_hooks_locks *locks_hooks = DAV_GET_HOOKS_LOCKS(r);
const char *body;
dav_error *err;
dav_error *err2;
dav_stream_mode mode;
dav_stream *stream;
dav_response *multi_response;
int has_range;
apr_off_t range_start;
apr_off_t range_end;
err = dav_get_resource(r, 0 , 0 ,
&resource);
if (err != NULL)
return dav_handle_err(r, err, NULL);
if (resource->type != DAV_RESOURCE_TYPE_REGULAR
&& resource->type != DAV_RESOURCE_TYPE_WORKING) {
body = apr_psprintf(r->pool,
"Cannot create resource %s with PUT.",
ap_escape_html(r->pool, r->uri));
return dav_error_response(r, HTTP_CONFLICT, body);
}
if (resource->collection) {
return dav_error_response(r, HTTP_CONFLICT,
"Cannot PUT to a collection.");
}
resource_state = dav_get_resource_state(r, resource);
if ((err = dav_validate_request(r, resource, 0, NULL, &multi_response,
resource_state == DAV_RESOURCE_NULL ?
DAV_VALIDATE_PARENT :
DAV_VALIDATE_RESOURCE, NULL)) != NULL) {
return dav_handle_err(r, err, multi_response);
}
has_range = dav_parse_range(r, &range_start, &range_end);
if (has_range < 0) {
body = apr_psprintf(r->pool,
"Malformed Content-Range header for PUT %s.",
ap_escape_html(r->pool, r->uri));
return dav_error_response(r, HTTP_BAD_REQUEST, body);
} else if (has_range) {
mode = DAV_MODE_WRITE_SEEKABLE;
} else {
mode = DAV_MODE_WRITE_TRUNC;
}
if ((err = dav_auto_checkout(r, resource,
0 ,
&av_info)) != NULL) {
return dav_handle_err(r, err, NULL);
}
if ((err = (*resource->hooks->open_stream)(resource, mode,
&stream)) != NULL) {
err = dav_push_error(r->pool, HTTP_FORBIDDEN, 0,
apr_psprintf(r->pool,
"Unable to PUT new contents for %s.",
ap_escape_html(r->pool, r->uri)),
err);
}
if (err == NULL && has_range) {
err = (*resource->hooks->seek_stream)(stream, range_start);
}
if (err == NULL) {
apr_bucket_brigade *bb;
apr_bucket *b;
int seen_eos = 0;
bb = apr_brigade_create(r->pool, r->connection->bucket_alloc);
do {
apr_status_t rc;
rc = ap_get_brigade(r->input_filters, bb, AP_MODE_READBYTES,
APR_BLOCK_READ, DAV_READ_BLOCKSIZE);
if (rc != APR_SUCCESS) {
int http_err;
char *msg = ap_escape_html(r->pool, r->uri);
http_err = ap_map_http_request_error(rc, HTTP_BAD_REQUEST);
msg = apr_psprintf(r->pool, "An error occurred while reading "
"the request body (URI: %s)",
msg);
err = dav_new_error(r->pool, http_err, 0, rc, msg);
break;
}
for (b = APR_BRIGADE_FIRST(bb);
b != APR_BRIGADE_SENTINEL(bb);
b = APR_BUCKET_NEXT(b)) {
const char *data;
apr_size_t len;
if (APR_BUCKET_IS_EOS(b)) {
seen_eos = 1;
break;
}
if (APR_BUCKET_IS_METADATA(b)) {
continue;
}
if (err == NULL) {
rc = apr_bucket_read(b, &data, &len, APR_BLOCK_READ);
if (rc != APR_SUCCESS) {
err = dav_new_error(r->pool, HTTP_BAD_REQUEST, 0, rc,
apr_psprintf(r->pool,
"An error occurred while"
" reading the request body"
" from the bucket (URI: %s)",
ap_escape_html(r->pool, r->uri)));
break;
}
err = (*resource->hooks->write_stream)(stream, data, len);
}
}
apr_brigade_cleanup(bb);
} while (!seen_eos);
apr_brigade_destroy(bb);
err2 = (*resource->hooks->close_stream)(stream,
err == NULL );
err = dav_join_error(err, err2);
}
if (err == NULL) {
resource->exists = 1;
}
err2 = dav_auto_checkin(r, resource, err != NULL ,
0 , &av_info);
if (err != NULL) {
err = dav_join_error(err, err2);
return dav_handle_err(r, err, NULL);
}
if (err2 != NULL) {
err2 = dav_push_error(r->pool, err2->status, 0,
"The PUT was successful, but there "
"was a problem automatically checking in "
"the resource or its parent collection.",
err2);
dav_log_err(r, err2, APLOG_WARNING);
}
if (locks_hooks != NULL) {
dav_lockdb *lockdb;
if ((err = (*locks_hooks->open_lockdb)(r, 0, 0, &lockdb)) != NULL) {
err = dav_push_error(r->pool, err->status, 0,
"The file was PUT successfully, but there "
"was a problem opening the lock database "
"which prevents inheriting locks from the "
"parent resources.",
err);
return dav_handle_err(r, err, NULL);
}
err = dav_notify_created(r, lockdb, resource, resource_state, 0);
(*locks_hooks->close_lockdb)(lockdb);
if (err != NULL) {
err = dav_push_error(r->pool, err->status, 0,
"The file was PUT successfully, but there "
"was a problem updating its lock "
"information.",
err);
return dav_handle_err(r, err, NULL);
}
}
return dav_created(r, NULL, "Resource", resource_state == DAV_RESOURCE_EXISTS);
}
static void dav_stream_response(dav_walk_resource *wres,
int status,
dav_get_props_result *propstats,
apr_pool_t *pool) {
dav_response resp = { 0 };
dav_walker_ctx *ctx = wres->walk_ctx;
resp.href = wres->resource->uri;
resp.status = status;
if (propstats) {
resp.propresult = *propstats;
}
dav_send_one_response(&resp, ctx->bb, ctx->r, pool);
}
DAV_DECLARE(void) dav_add_response(dav_walk_resource *wres,
int status, dav_get_props_result *propstats) {
dav_response *resp;
resp = apr_pcalloc(wres->pool, sizeof(*resp));
resp->href = apr_pstrdup(wres->pool, wres->resource->uri);
resp->status = status;
if (propstats) {
resp->propresult = *propstats;
}
resp->next = wres->response;
wres->response = resp;
}
static int dav_method_delete(request_rec *r) {
dav_resource *resource;
dav_auto_version_info av_info;
dav_error *err;
dav_error *err2;
dav_response *multi_response;
int result;
int depth;
if ((result = ap_discard_request_body(r)) != OK) {
return result;
}
err = dav_get_resource(r, 0 , 0 ,
&resource);
if (err != NULL)
return dav_handle_err(r, err, NULL);
if (!resource->exists) {
return HTTP_NOT_FOUND;
}
depth = dav_get_depth(r, DAV_INFINITY);
if (resource->collection && depth != DAV_INFINITY) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(00582)
"Depth must be \"infinity\" for DELETE of a collection.");
return HTTP_BAD_REQUEST;
}
if (!resource->collection && depth == 1) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(00583)
"Depth of \"1\" is not allowed for DELETE.");
return HTTP_BAD_REQUEST;
}
if ((err = dav_validate_request(r, resource, depth, NULL,
&multi_response,
DAV_VALIDATE_PARENT
| DAV_VALIDATE_USE_424, NULL)) != NULL) {
err = dav_push_error(r->pool, err->status, 0,
apr_psprintf(r->pool,
"Could not DELETE %s due to a failed "
"precondition (e.g. locks).",
ap_escape_html(r->pool, r->uri)),
err);
return dav_handle_err(r, err, multi_response);
}
if ((result = dav_unlock(r, resource, NULL)) != OK) {
return result;
}
if ((err = dav_auto_checkout(r, resource, 1 ,
&av_info)) != NULL) {
return dav_handle_err(r, err, NULL);
}
err = (*resource->hooks->remove_resource)(resource, &multi_response);
err2 = dav_auto_checkin(r, NULL, err != NULL ,
0 , &av_info);
if (err != NULL) {
err = dav_push_error(r->pool, err->status, 0,
apr_psprintf(r->pool,
"Could not DELETE %s.",
ap_escape_html(r->pool, r->uri)),
err);
return dav_handle_err(r, err, multi_response);
}
if (err2 != NULL) {
err = dav_push_error(r->pool, err2->status, 0,
"The DELETE was successful, but there "
"was a problem automatically checking in "
"the parent collection.",
err2);
dav_log_err(r, err, APLOG_WARNING);
}
return HTTP_NO_CONTENT;
}
static dav_error *dav_gen_supported_methods(request_rec *r,
const apr_xml_elem *elem,
const apr_table_t *methods,
apr_text_header *body) {
const apr_array_header_t *arr;
const apr_table_entry_t *elts;
apr_xml_elem *child;
apr_xml_attr *attr;
char *s;
int i;
apr_text_append(r->pool, body, "<D:supported-method-set>" DEBUG_CR);
if (elem->first_child == NULL) {
arr = apr_table_elts(methods);
elts = (const apr_table_entry_t *)arr->elts;
for (i = 0; i < arr->nelts; ++i) {
if (elts[i].key == NULL)
continue;
s = apr_psprintf(r->pool,
"<D:supported-method D:name=\"%s\"/>"
DEBUG_CR,
elts[i].key);
apr_text_append(r->pool, body, s);
}
} else {
for (child = elem->first_child; child != NULL; child = child->next) {
if (child->ns == APR_XML_NS_DAV_ID
&& strcmp(child->name, "supported-method") == 0) {
const char *name = NULL;
for (attr = child->attr; attr != NULL; attr = attr->next) {
if (attr->ns == APR_XML_NS_DAV_ID
&& strcmp(attr->name, "name") == 0)
name = attr->value;
}
if (name == NULL) {
return dav_new_error(r->pool, HTTP_BAD_REQUEST, 0, 0,
"A DAV:supported-method element "
"does not have a \"name\" attribute");
}
if (apr_table_get(methods, name) != NULL) {
s = apr_psprintf(r->pool,
"<D:supported-method D:name=\"%s\"/>"
DEBUG_CR,
name);
apr_text_append(r->pool, body, s);
}
}
}
}
apr_text_append(r->pool, body, "</D:supported-method-set>" DEBUG_CR);
return NULL;
}
static dav_error *dav_gen_supported_live_props(request_rec *r,
const dav_resource *resource,
const apr_xml_elem *elem,
apr_text_header *body) {
dav_lockdb *lockdb;
dav_propdb *propdb;
apr_xml_elem *child;
apr_xml_attr *attr;
dav_error *err;
if ((err = dav_open_lockdb(r, 0, &lockdb)) != NULL) {
return dav_push_error(r->pool, err->status, 0,
"The lock database could not be opened, "
"preventing the reporting of supported lock "
"properties.",
err);
}
if ((err = dav_open_propdb(r, lockdb, resource, 1, NULL,
&propdb)) != NULL) {
if (lockdb != NULL)
(*lockdb->hooks->close_lockdb)(lockdb);
return dav_push_error(r->pool, err->status, 0,
"The property database could not be opened, "
"preventing report of supported properties.",
err);
}
apr_text_append(r->pool, body, "<D:supported-live-property-set>" DEBUG_CR);
if (elem->first_child == NULL) {
dav_get_props_result props = dav_get_allprops(propdb, DAV_PROP_INSERT_SUPPORTED);
body->last->next = props.propstats;
while (body->last->next != NULL)
body->last = body->last->next;
} else {
for (child = elem->first_child; child != NULL; child = child->next) {
if (child->ns == APR_XML_NS_DAV_ID
&& strcmp(child->name, "supported-live-property") == 0) {
const char *name = NULL;
const char *nmspace = NULL;
for (attr = child->attr; attr != NULL; attr = attr->next) {
if (attr->ns == APR_XML_NS_DAV_ID) {
if (strcmp(attr->name, "name") == 0)
name = attr->value;
else if (strcmp(attr->name, "namespace") == 0)
nmspace = attr->value;
}
}
if (name == NULL) {
err = dav_new_error(r->pool, HTTP_BAD_REQUEST, 0, 0,
"A DAV:supported-live-property "
"element does not have a \"name\" "
"attribute");
break;
}
if (nmspace == NULL)
nmspace = "DAV:";
dav_get_liveprop_supported(propdb, nmspace, name, body);
}
}
}
apr_text_append(r->pool, body, "</D:supported-live-property-set>" DEBUG_CR);
dav_close_propdb(propdb);
if (lockdb != NULL)
(*lockdb->hooks->close_lockdb)(lockdb);
return err;
}
static dav_error *dav_gen_supported_reports(request_rec *r,
const dav_resource *resource,
const apr_xml_elem *elem,
const dav_hooks_vsn *vsn_hooks,
apr_text_header *body) {
apr_xml_elem *child;
apr_xml_attr *attr;
dav_error *err;
char *s;
apr_text_append(r->pool, body, "<D:supported-report-set>" DEBUG_CR);
if (vsn_hooks != NULL) {
const dav_report_elem *reports;
const dav_report_elem *rp;
if ((err = (*vsn_hooks->avail_reports)(resource, &reports)) != NULL) {
return dav_push_error(r->pool, err->status, 0,
"DAV:supported-report-set could not be "
"determined due to a problem fetching the "
"available reports for this resource.",
err);
}
if (reports != NULL) {
if (elem->first_child == NULL) {
for (rp = reports; rp->nmspace != NULL; ++rp) {
s = apr_psprintf(r->pool,
"<D:supported-report D:name=\"%s\" "
"D:namespace=\"%s\"/>" DEBUG_CR,
rp->name, rp->nmspace);
apr_text_append(r->pool, body, s);
}
} else {
for (child = elem->first_child; child != NULL; child = child->next) {
if (child->ns == APR_XML_NS_DAV_ID
&& strcmp(child->name, "supported-report") == 0) {
const char *name = NULL;
const char *nmspace = NULL;
for (attr = child->attr; attr != NULL; attr = attr->next) {
if (attr->ns == APR_XML_NS_DAV_ID) {
if (strcmp(attr->name, "name") == 0)
name = attr->value;
else if (strcmp(attr->name, "namespace") == 0)
nmspace = attr->value;
}
}
if (name == NULL) {
return dav_new_error(r->pool, HTTP_BAD_REQUEST, 0, 0,
"A DAV:supported-report element "
"does not have a \"name\" attribute");
}
if (nmspace == NULL)
nmspace = "DAV:";
for (rp = reports; rp->nmspace != NULL; ++rp) {
if (strcmp(name, rp->name) == 0
&& strcmp(nmspace, rp->nmspace) == 0) {
s = apr_psprintf(r->pool,
"<D:supported-report "
"D:name=\"%s\" "
"D:namespace=\"%s\"/>"
DEBUG_CR,
rp->name, rp->nmspace);
apr_text_append(r->pool, body, s);
break;
}
}
}
}
}
}
}
apr_text_append(r->pool, body, "</D:supported-report-set>" DEBUG_CR);
return NULL;
}
static int dav_method_search(request_rec *r) {
const dav_hooks_search *search_hooks = DAV_GET_HOOKS_SEARCH(r);
dav_resource *resource;
dav_error *err;
dav_response *multi_status;
if (search_hooks == NULL)
return DECLINED;
err = dav_get_resource(r, 1 , 0 ,
&resource);
if (err != NULL)
return dav_handle_err(r, err, NULL);
if (!resource->exists) {
return HTTP_NOT_FOUND;
}
if ((err = (*resource->hooks->set_headers)(r, resource)) != NULL) {
err = dav_push_error(r->pool, err->status, 0,
"Unable to set up HTTP headers.",
err);
return dav_handle_err(r, err, NULL);
}
if (r->header_only) {
return DONE;
}
if ((err = (*search_hooks->search_resource)(r, &multi_status)) != NULL) {
return dav_handle_err(r, err, NULL);
}
dav_send_multistatus(r, HTTP_MULTI_STATUS, multi_status, NULL);
return DONE;
}
static int dav_method_options(request_rec *r) {
const dav_hooks_locks *locks_hooks = DAV_GET_HOOKS_LOCKS(r);
const dav_hooks_vsn *vsn_hooks = DAV_GET_HOOKS_VSN(r);
const dav_hooks_binding *binding_hooks = DAV_GET_HOOKS_BINDING(r);
const dav_hooks_search *search_hooks = DAV_GET_HOOKS_SEARCH(r);
dav_resource *resource;
const char *dav_level;
char *allow;
char *s;
const apr_array_header_t *arr;
const apr_table_entry_t *elts;
apr_table_t *methods = apr_table_make(r->pool, 12);
apr_text_header vsn_options = { 0 };
apr_text_header body = { 0 };
apr_text *t;
int text_size;
int result;
int i;
apr_array_header_t *uri_ary;
apr_xml_doc *doc;
const apr_xml_elem *elem;
dav_error *err;
apr_array_header_t *extensions;
ap_list_provider_names_t *entry;
err = dav_get_resource(r, 0 , 0 ,
&resource);
if (err != NULL)
return dav_handle_err(r, err, NULL);
if ((result = ap_xml_parse_input(r, &doc)) != OK) {
return result;
}
if (doc && !dav_validate_root(doc, "options")) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(00584)
"The \"options\" element was not found.");
return HTTP_BAD_REQUEST;
}
dav_level = "1";
if (locks_hooks != NULL) {
dav_level = "1,2";
}
if (binding_hooks != NULL)
dav_level = apr_pstrcat(r->pool, dav_level, ",bindings", NULL);
extensions = ap_list_provider_names(r->pool, DAV_OPTIONS_EXTENSION_GROUP, "0");
entry = (ap_list_provider_names_t *)extensions->elts;
for (i = 0; i < extensions->nelts; i++, entry++) {
const dav_options_provider *options =
dav_get_options_providers(entry->provider_name);
if (options && options->dav_header) {
apr_text_header hoptions = { 0 };
options->dav_header(r, resource, &hoptions);
for (t = hoptions.first; t && t->text; t = t->next)
dav_level = apr_pstrcat(r->pool, dav_level, ",", t->text, NULL);
}
}
apr_table_setn(r->headers_out, "DAV", dav_level);
if (vsn_hooks != NULL) {
(*vsn_hooks->get_vsn_options)(r->pool, &vsn_options);
for (t = vsn_options.first; t != NULL; t = t->next)
apr_table_addn(r->headers_out, "DAV", t->text);
}
uri_ary = apr_array_make(r->pool, 5, sizeof(const char *));
dav_run_gather_propsets(uri_ary);
for (i = 0; i < uri_ary->nelts; ++i) {
if (((char **)uri_ary->elts)[i] != NULL)
apr_table_addn(r->headers_out, "DAV", ((char **)uri_ary->elts)[i]);
}
apr_table_setn(r->headers_out, "MS-Author-Via", "DAV");
apr_table_addn(methods, "OPTIONS", "");
switch (dav_get_resource_state(r, resource)) {
case DAV_RESOURCE_EXISTS:
apr_table_addn(methods, "GET", "");
apr_table_addn(methods, "HEAD", "");
apr_table_addn(methods, "POST", "");
apr_table_addn(methods, "DELETE", "");
apr_table_addn(methods, "TRACE", "");
apr_table_addn(methods, "PROPFIND", "");
apr_table_addn(methods, "PROPPATCH", "");
apr_table_addn(methods, "COPY", "");
apr_table_addn(methods, "MOVE", "");
if (!resource->collection)
apr_table_addn(methods, "PUT", "");
if (locks_hooks != NULL) {
apr_table_addn(methods, "LOCK", "");
apr_table_addn(methods, "UNLOCK", "");
}
break;
case DAV_RESOURCE_LOCK_NULL:
apr_table_addn(methods, "MKCOL", "");
apr_table_addn(methods, "PROPFIND", "");
apr_table_addn(methods, "PUT", "");
if (locks_hooks != NULL) {
apr_table_addn(methods, "LOCK", "");
apr_table_addn(methods, "UNLOCK", "");
}
break;
case DAV_RESOURCE_NULL:
apr_table_addn(methods, "MKCOL", "");
apr_table_addn(methods, "PUT", "");
if (locks_hooks != NULL)
apr_table_addn(methods, "LOCK", "");
break;
default:
break;
}
if (vsn_hooks != NULL) {
if (!resource->exists) {
if ((*vsn_hooks->versionable)(resource))
apr_table_addn(methods, "VERSION-CONTROL", "");
if (vsn_hooks->can_be_workspace != NULL
&& (*vsn_hooks->can_be_workspace)(resource))
apr_table_addn(methods, "MKWORKSPACE", "");
if (vsn_hooks->can_be_activity != NULL
&& (*vsn_hooks->can_be_activity)(resource))
apr_table_addn(methods, "MKACTIVITY", "");
} else if (!resource->versioned) {
if ((*vsn_hooks->versionable)(resource))
apr_table_addn(methods, "VERSION-CONTROL", "");
} else if (resource->working) {
apr_table_addn(methods, "CHECKIN", "");
apr_table_addn(methods, "UNCHECKOUT", "");
} else if (vsn_hooks->add_label != NULL) {
apr_table_addn(methods, "CHECKOUT", "");
apr_table_addn(methods, "LABEL", "");
} else {
apr_table_addn(methods, "CHECKOUT", "");
}
}
if (binding_hooks != NULL
&& (*binding_hooks->is_bindable)(resource)) {
apr_table_addn(methods, "BIND", "");
}
if (search_hooks != NULL) {
apr_table_addn(methods, "SEARCH", "");
}
extensions = ap_list_provider_names(r->pool, DAV_OPTIONS_EXTENSION_GROUP, "0");
entry = (ap_list_provider_names_t *)extensions->elts;
for (i = 0; i < extensions->nelts; i++, entry++) {
const dav_options_provider *options =
dav_get_options_providers(entry->provider_name);
if (options && options->dav_method) {
apr_text_header hoptions = { 0 };
options->dav_method(r, resource, &hoptions);
for (t = hoptions.first; t && t->text; t = t->next)
apr_table_addn(methods, t->text, "");
}
}
arr = apr_table_elts(methods);
elts = (const apr_table_entry_t *)arr->elts;
text_size = 0;
for (i = 0; i < arr->nelts; ++i) {
if (elts[i].key == NULL)
continue;
text_size += strlen(elts[i].key) + 1;
}
s = allow = apr_palloc(r->pool, text_size);
for (i = 0; i < arr->nelts; ++i) {
if (elts[i].key == NULL)
continue;
if (s != allow)
*s++ = ',';
strcpy(s, elts[i].key);
s += strlen(s);
}
apr_table_setn(r->headers_out, "Allow", allow);
if (search_hooks != NULL
&& *search_hooks->set_option_head != NULL) {
if ((err = (*search_hooks->set_option_head)(r)) != NULL) {
return dav_handle_err(r, err, NULL);
}
}
if (doc == NULL) {
ap_set_content_length(r, 0);
return DONE;
}
for (elem = doc->root->first_child; elem != NULL; elem = elem->next) {
int core_option = 0;
dav_error *err = NULL;
if (elem->ns == APR_XML_NS_DAV_ID) {
if (strcmp(elem->name, "supported-method-set") == 0) {
err = dav_gen_supported_methods(r, elem, methods, &body);
core_option = 1;
} else if (strcmp(elem->name, "supported-live-property-set") == 0) {
err = dav_gen_supported_live_props(r, resource, elem, &body);
core_option = 1;
} else if (strcmp(elem->name, "supported-report-set") == 0) {
err = dav_gen_supported_reports(r, resource, elem, vsn_hooks, &body);
core_option = 1;
}
}
if (err != NULL)
return dav_handle_err(r, err, NULL);
if (!core_option && vsn_hooks != NULL) {
if ((err = (*vsn_hooks->get_option)(resource, elem, &body))
!= NULL) {
return dav_handle_err(r, err, NULL);
}
}
}
r->status = HTTP_OK;
ap_set_content_type(r, DAV_XML_CONTENT_TYPE);
ap_rputs(DAV_XML_HEADER DEBUG_CR
"<D:options-response xmlns:D=\"DAV:\">" DEBUG_CR, r);
for (t = body.first; t != NULL; t = t->next)
ap_rputs(t->text, r);
ap_rputs("</D:options-response>" DEBUG_CR, r);
return DONE;
}
static void dav_cache_badprops(dav_walker_ctx *ctx) {
const apr_xml_elem *elem;
apr_text_header hdr = { 0 };
if (ctx->propstat_404 != NULL) {
return;
}
apr_text_append(ctx->w.pool, &hdr,
"<D:propstat>" DEBUG_CR
"<D:prop>" DEBUG_CR);
elem = dav_find_child(ctx->doc->root, "prop");
for (elem = elem->first_child; elem; elem = elem->next) {
apr_text_append(ctx->w.pool, &hdr,
apr_xml_empty_elem(ctx->w.pool, elem));
}
apr_text_append(ctx->w.pool, &hdr,
"</D:prop>" DEBUG_CR
"<D:status>HTTP/1.1 404 Not Found</D:status>" DEBUG_CR
"</D:propstat>" DEBUG_CR);
ctx->propstat_404 = hdr.first;
}
static dav_error * dav_propfind_walker(dav_walk_resource *wres, int calltype) {
dav_walker_ctx *ctx = wres->walk_ctx;
dav_error *err;
dav_propdb *propdb;
dav_get_props_result propstats = { 0 };
err = dav_open_propdb(ctx->r, ctx->w.lockdb, wres->resource, 1,
ctx->doc ? ctx->doc->namespaces : NULL, &propdb);
if (err != NULL) {
if (ctx->propfind_type == DAV_PROPFIND_IS_PROP) {
dav_get_props_result badprops = { 0 };
dav_cache_badprops(ctx);
badprops.propstats = ctx->propstat_404;
dav_stream_response(wres, 0, &badprops, ctx->scratchpool);
} else {
dav_stream_response(wres, HTTP_OK, NULL, ctx->scratchpool);
}
apr_pool_clear(ctx->scratchpool);
return NULL;
}
if (ctx->propfind_type == DAV_PROPFIND_IS_PROP) {
propstats = dav_get_props(propdb, ctx->doc);
} else {
dav_prop_insert what = ctx->propfind_type == DAV_PROPFIND_IS_ALLPROP
? DAV_PROP_INSERT_VALUE
: DAV_PROP_INSERT_NAME;
propstats = dav_get_allprops(propdb, what);
}
dav_close_propdb(propdb);
dav_stream_response(wres, 0, &propstats, ctx->scratchpool);
apr_pool_clear(ctx->scratchpool);
return NULL;
}
static int dav_method_propfind(request_rec *r) {
dav_resource *resource;
int depth;
dav_error *err;
int result;
apr_xml_doc *doc;
dav_walker_ctx ctx = { { 0 } };
dav_response *multi_status;
err = dav_get_resource(r, 1 , 0 ,
&resource);
if (err != NULL)
return dav_handle_err(r, err, NULL);
if (dav_get_resource_state(r, resource) == DAV_RESOURCE_NULL) {
return HTTP_NOT_FOUND;
}
if ((depth = dav_get_depth(r, DAV_INFINITY)) < 0) {
return HTTP_BAD_REQUEST;
}
if (depth == DAV_INFINITY && resource->collection) {
dav_dir_conf *conf;
conf = (dav_dir_conf *)ap_get_module_config(r->per_dir_config,
&dav_module);
if (conf->allow_depthinfinity != DAV_ENABLED_ON) {
return dav_error_response(r, HTTP_FORBIDDEN,
apr_psprintf(r->pool,
"PROPFIND requests with a "
"Depth of \"infinity\" are "
"not allowed for %s.",
ap_escape_html(r->pool,
r->uri)));
}
}
if ((result = ap_xml_parse_input(r, &doc)) != OK) {
return result;
}
if (doc && !dav_validate_root(doc, "propfind")) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(00585)
"The \"propfind\" element was not found.");
return HTTP_BAD_REQUEST;
}
if (doc == NULL || dav_find_child(doc->root, "allprop") != NULL) {
ctx.propfind_type = DAV_PROPFIND_IS_ALLPROP;
} else if (dav_find_child(doc->root, "propname") != NULL) {
ctx.propfind_type = DAV_PROPFIND_IS_PROPNAME;
} else if (dav_find_child(doc->root, "prop") != NULL) {
ctx.propfind_type = DAV_PROPFIND_IS_PROP;
} else {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(00586)
"The \"propfind\" element does not contain one of "
"the required child elements (the specific command).");
return HTTP_BAD_REQUEST;
}
ctx.w.walk_type = DAV_WALKTYPE_NORMAL | DAV_WALKTYPE_AUTH;
ctx.w.func = dav_propfind_walker;
ctx.w.walk_ctx = &ctx;
ctx.w.pool = r->pool;
ctx.w.root = resource;
ctx.doc = doc;
ctx.r = r;
ctx.bb = apr_brigade_create(r->pool, r->connection->bucket_alloc);
apr_pool_create(&ctx.scratchpool, r->pool);
if ((err = dav_open_lockdb(r, 0, &ctx.w.lockdb)) != NULL) {
err = dav_push_error(r->pool, err->status, 0,
"The lock database could not be opened, "
"preventing access to the various lock "
"properties for the PROPFIND.",
err);
return dav_handle_err(r, err, NULL);
}
if (ctx.w.lockdb != NULL) {
ctx.w.walk_type |= DAV_WALKTYPE_LOCKNULL;
}
dav_begin_multistatus(ctx.bb, r, HTTP_MULTI_STATUS,
doc ? doc->namespaces : NULL);
err = (*resource->hooks->walk)(&ctx.w, depth, &multi_status);
if (ctx.w.lockdb != NULL) {
(*ctx.w.lockdb->hooks->close_lockdb)(ctx.w.lockdb);
}
if (err != NULL) {
err = dav_push_error(r->pool, err->status, 0,
"Provider encountered an error while streaming"
" a multistatus PROPFIND response.", err);
dav_log_err(r, err, APLOG_ERR);
r->connection->aborted = 1;
return DONE;
}
dav_finish_multistatus(r, ctx.bb);
return DONE;
}
DAV_DECLARE(apr_text *) dav_failed_proppatch(apr_pool_t *p,
apr_array_header_t *prop_ctx) {
apr_text_header hdr = { 0 };
int i = prop_ctx->nelts;
dav_prop_ctx *ctx = (dav_prop_ctx *)prop_ctx->elts;
dav_error *err424_set = NULL;
dav_error *err424_delete = NULL;
const char *s;
for ( ; i-- > 0; ++ctx ) {
apr_text_append(p, &hdr,
"<D:propstat>" DEBUG_CR
"<D:prop>");
apr_text_append(p, &hdr, apr_xml_empty_elem(p, ctx->prop));
apr_text_append(p, &hdr, "</D:prop>" DEBUG_CR);
if (ctx->err == NULL) {
if (ctx->operation == DAV_PROP_OP_SET) {
if (err424_set == NULL)
err424_set = dav_new_error(p, HTTP_FAILED_DEPENDENCY, 0, 0,
"Attempted DAV:set operation "
"could not be completed due "
"to other errors.");
ctx->err = err424_set;
} else if (ctx->operation == DAV_PROP_OP_DELETE) {
if (err424_delete == NULL)
err424_delete = dav_new_error(p, HTTP_FAILED_DEPENDENCY, 0, 0,
"Attempted DAV:remove "
"operation could not be "
"completed due to other "
"errors.");
ctx->err = err424_delete;
}
}
s = apr_psprintf(p,
"<D:status>"
"HTTP/1.1 %d (status)"
"</D:status>" DEBUG_CR,
ctx->err->status);
apr_text_append(p, &hdr, s);
if (ctx->err->desc != NULL) {
apr_text_append(p, &hdr, "<D:responsedescription>" DEBUG_CR);
apr_text_append(p, &hdr, ctx->err->desc);
apr_text_append(p, &hdr, "</D:responsedescription>" DEBUG_CR);
}
apr_text_append(p, &hdr, "</D:propstat>" DEBUG_CR);
}
return hdr.first;
}
DAV_DECLARE(apr_text *) dav_success_proppatch(apr_pool_t *p,
apr_array_header_t *prop_ctx) {
apr_text_header hdr = { 0 };
int i = prop_ctx->nelts;
dav_prop_ctx *ctx = (dav_prop_ctx *)prop_ctx->elts;
apr_text_append(p, &hdr,
"<D:propstat>" DEBUG_CR
"<D:prop>" DEBUG_CR);
for ( ; i-- > 0; ++ctx ) {
apr_text_append(p, &hdr, apr_xml_empty_elem(p, ctx->prop));
}
apr_text_append(p, &hdr,
"</D:prop>" DEBUG_CR
"<D:status>HTTP/1.1 200 OK</D:status>" DEBUG_CR
"</D:propstat>" DEBUG_CR);
return hdr.first;
}
static void dav_prop_log_errors(dav_prop_ctx *ctx) {
dav_log_err(ctx->r, ctx->err, APLOG_ERR);
}
static int dav_process_ctx_list(void (*func)(dav_prop_ctx *ctx),
apr_array_header_t *ctx_list, int stop_on_error,
int reverse) {
int i = ctx_list->nelts;
dav_prop_ctx *ctx = (dav_prop_ctx *)ctx_list->elts;
if (reverse)
ctx += i;
while (i--) {
if (reverse)
--ctx;
(*func)(ctx);
if (stop_on_error && DAV_PROP_CTX_HAS_ERR(*ctx)) {
return 1;
}
if (!reverse)
++ctx;
}
return 0;
}
static int dav_method_proppatch(request_rec *r) {
dav_error *err;
dav_resource *resource;
int result;
apr_xml_doc *doc;
apr_xml_elem *child;
dav_propdb *propdb;
int failure = 0;
dav_response resp = { 0 };
apr_text *propstat_text;
apr_array_header_t *ctx_list;
dav_prop_ctx *ctx;
dav_auto_version_info av_info;
err = dav_get_resource(r, 0 , 0 ,
&resource);
if (err != NULL)
return dav_handle_err(r, err, NULL);
if (!resource->exists) {
return HTTP_NOT_FOUND;
}
if ((result = ap_xml_parse_input(r, &doc)) != OK) {
return result;
}
if (doc == NULL || !dav_validate_root(doc, "propertyupdate")) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(00587)
"The request body does not contain "
"a \"propertyupdate\" element.");
return HTTP_BAD_REQUEST;
}
if ((err = dav_validate_request(r, resource, 0, NULL, NULL,
DAV_VALIDATE_RESOURCE, NULL)) != NULL) {
return dav_handle_err(r, err, NULL);
}
if ((err = dav_auto_checkout(r, resource,
0 ,
&av_info)) != NULL) {
return dav_handle_err(r, err, NULL);
}
if ((err = dav_open_propdb(r, NULL, resource, 0, doc->namespaces,
&propdb)) != NULL) {
dav_auto_checkin(r, resource, 1 , 0 , &av_info);
err = dav_push_error(r->pool, HTTP_INTERNAL_SERVER_ERROR, 0,
apr_psprintf(r->pool,
"Could not open the property "
"database for %s.",
ap_escape_html(r->pool, r->uri)),
err);
return dav_handle_err(r, err, NULL);
}
ctx_list = apr_array_make(r->pool, 10, sizeof(dav_prop_ctx));
for (child = doc->root->first_child; child; child = child->next) {
int is_remove;
apr_xml_elem *prop_group;
apr_xml_elem *one_prop;
if (child->ns != APR_XML_NS_DAV_ID
|| (!(is_remove = (strcmp(child->name, "remove") == 0))
&& strcmp(child->name, "set") != 0)) {
continue;
}
if ((prop_group = dav_find_child(child, "prop")) == NULL) {
dav_close_propdb(propdb);
dav_auto_checkin(r, resource, 1 , 0 , &av_info);
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(00588)
"A \"prop\" element is missing inside "
"the propertyupdate command.");
return HTTP_BAD_REQUEST;
}
for (one_prop = prop_group->first_child; one_prop;
one_prop = one_prop->next) {
ctx = (dav_prop_ctx *)apr_array_push(ctx_list);
ctx->propdb = propdb;
ctx->operation = is_remove ? DAV_PROP_OP_DELETE : DAV_PROP_OP_SET;
ctx->prop = one_prop;
ctx->r = r;
dav_prop_validate(ctx);
if ( DAV_PROP_CTX_HAS_ERR(*ctx) ) {
failure = 1;
}
}
}
if (!failure && dav_process_ctx_list(dav_prop_exec, ctx_list, 1, 0)) {
failure = 1;
}
if (failure) {
(void)dav_process_ctx_list(dav_prop_rollback, ctx_list, 0, 1);
propstat_text = dav_failed_proppatch(r->pool, ctx_list);
} else {
(void)dav_process_ctx_list(dav_prop_commit, ctx_list, 0, 0);
propstat_text = dav_success_proppatch(r->pool, ctx_list);
}
dav_close_propdb(propdb);
dav_auto_checkin(r, resource, failure, 0 , &av_info);
(void)dav_process_ctx_list(dav_prop_log_errors, ctx_list, 0, 0);
resp.href = resource->uri;
resp.propresult.propstats = propstat_text;
dav_send_multistatus(r, HTTP_MULTI_STATUS, &resp, doc->namespaces);
return DONE;
}
static int process_mkcol_body(request_rec *r) {
const char *tenc = apr_table_get(r->headers_in, "Transfer-Encoding");
const char *lenp = apr_table_get(r->headers_in, "Content-Length");
r->read_body = REQUEST_NO_BODY;
r->read_chunked = 0;
r->remaining = 0;
if (tenc) {
if (strcasecmp(tenc, "chunked")) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(00589)
"Unknown Transfer-Encoding %s", tenc);
return HTTP_NOT_IMPLEMENTED;
}
r->read_chunked = 1;
} else if (lenp) {
const char *pos = lenp;
while (apr_isdigit(*pos) || apr_isspace(*pos)) {
++pos;
}
if (*pos != '\0') {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(00590)
"Invalid Content-Length %s", lenp);
return HTTP_BAD_REQUEST;
}
r->remaining = apr_atoi64(lenp);
}
if (r->read_chunked || r->remaining > 0) {
return HTTP_UNSUPPORTED_MEDIA_TYPE;
}
return ap_discard_request_body(r);
}
static int dav_method_mkcol(request_rec *r) {
dav_resource *resource;
int resource_state;
dav_auto_version_info av_info;
const dav_hooks_locks *locks_hooks = DAV_GET_HOOKS_LOCKS(r);
dav_error *err;
dav_error *err2;
int result;
dav_response *multi_status;
if ((result = process_mkcol_body(r)) != OK) {
return result;
}
err = dav_get_resource(r, 0 , 0 ,
&resource);
if (err != NULL)
return dav_handle_err(r, err, NULL);
if (resource->exists) {
return HTTP_METHOD_NOT_ALLOWED;
}
resource_state = dav_get_resource_state(r, resource);
if ((err = dav_validate_request(r, resource, 0, NULL, &multi_status,
resource_state == DAV_RESOURCE_NULL ?
DAV_VALIDATE_PARENT :
DAV_VALIDATE_RESOURCE, NULL)) != NULL) {
return dav_handle_err(r, err, multi_status);
}
if ((err = dav_auto_checkout(r, resource, 1 ,
&av_info)) != NULL) {
return dav_handle_err(r, err, NULL);
}
resource->collection = 1;
err = (*resource->hooks->create_collection)(resource);
err2 = dav_auto_checkin(r, NULL, err != NULL ,
0 , &av_info);
if (err != NULL) {
return dav_handle_err(r, err, NULL);
}
if (err2 != NULL) {
err = dav_push_error(r->pool, err2->status, 0,
"The MKCOL was successful, but there "
"was a problem automatically checking in "
"the parent collection.",
err2);
dav_log_err(r, err, APLOG_WARNING);
}
if (locks_hooks != NULL) {
dav_lockdb *lockdb;
if ((err = (*locks_hooks->open_lockdb)(r, 0, 0, &lockdb)) != NULL) {
err = dav_push_error(r->pool, err->status, 0,
"The MKCOL was successful, but there "
"was a problem opening the lock database "
"which prevents inheriting locks from the "
"parent resources.",
err);
return dav_handle_err(r, err, NULL);
}
err = dav_notify_created(r, lockdb, resource, resource_state, 0);
(*locks_hooks->close_lockdb)(lockdb);
if (err != NULL) {
err = dav_push_error(r->pool, err->status, 0,
"The MKCOL was successful, but there "
"was a problem updating its lock "
"information.",
err);
return dav_handle_err(r, err, NULL);
}
}
return dav_created(r, NULL, "Collection", 0);
}
static int dav_method_copymove(request_rec *r, int is_move) {
dav_resource *resource;
dav_resource *resnew;
dav_auto_version_info src_av_info = { 0 };
dav_auto_version_info dst_av_info = { 0 };
const char *body;
const char *dest;
dav_error *err;
dav_error *err2;
dav_error *err3;
dav_response *multi_response;
dav_lookup_result lookup;
int is_dir;
int overwrite;
int depth;
int result;
dav_lockdb *lockdb;
int replace_dest;
int resnew_state;
err = dav_get_resource(r, !is_move ,
0 , &resource);
if (err != NULL)
return dav_handle_err(r, err, NULL);
if (!resource->exists) {
return HTTP_NOT_FOUND;
}
if (resource->type != DAV_RESOURCE_TYPE_REGULAR) {
body = apr_psprintf(r->pool,
"Cannot COPY/MOVE resource %s.",
ap_escape_html(r->pool, r->uri));
return dav_error_response(r, HTTP_METHOD_NOT_ALLOWED, body);
}
dest = apr_table_get(r->headers_in, "Destination");
if (dest == NULL) {
const char *nscp_host = apr_table_get(r->headers_in, "Host");
const char *nscp_path = apr_table_get(r->headers_in, "New-uri");
if (nscp_host != NULL && nscp_path != NULL)
dest = apr_psprintf(r->pool, "http://%s%s", nscp_host, nscp_path);
}
if (dest == NULL) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(00591)
"The request is missing a Destination header.");
return HTTP_BAD_REQUEST;
}
lookup = dav_lookup_uri(dest, r, 1 );
if (lookup.rnew == NULL) {
if (lookup.err.status == HTTP_BAD_REQUEST) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(00592)
"%s", lookup.err.desc);
return HTTP_BAD_REQUEST;
}
return dav_error_response(r, lookup.err.status, lookup.err.desc);
}
if (lookup.rnew->status != HTTP_OK) {
const char *auth = apr_table_get(lookup.rnew->err_headers_out,
"WWW-Authenticate");
if (lookup.rnew->status == HTTP_UNAUTHORIZED && auth != NULL) {
apr_table_setn(r->err_headers_out, "WWW-Authenticate",
apr_pstrdup(r->pool, auth));
}
return dav_error_response(r, lookup.rnew->status,
"Destination URI had an error.");
}
err = dav_get_resource(lookup.rnew, 0 ,
0 , &resnew);
if (err != NULL)
return dav_handle_err(r, err, NULL);
if (resource->hooks != resnew->hooks) {
return dav_error_response(r, HTTP_BAD_GATEWAY,
"Destination URI is handled by a "
"different repository than the source URI. "
"MOVE or COPY between repositories is "
"not possible.");
}
if ((overwrite = dav_get_overwrite(r)) < 0) {
return HTTP_BAD_REQUEST;
}
if (resnew->exists && !overwrite) {
return dav_error_response(r, HTTP_PRECONDITION_FAILED,
"Destination is not empty and "
"Overwrite is not \"T\"");
}
if ((*resource->hooks->is_same_resource)(resource, resnew)) {
return dav_error_response(r, HTTP_FORBIDDEN,
"Source and Destination URIs are the same.");
}
is_dir = resource->collection;
if ((depth = dav_get_depth(r, DAV_INFINITY)) < 0) {
return HTTP_BAD_REQUEST;
}
if (depth == 1) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(00593)
"Depth must be \"0\" or \"infinity\" for COPY or MOVE.");
return HTTP_BAD_REQUEST;
}
if (is_move && is_dir && depth != DAV_INFINITY) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(00594)
"Depth must be \"infinity\" when moving a collection.");
return HTTP_BAD_REQUEST;
}
if ((err = dav_validate_request(r, resource, depth, NULL,
&multi_response,
(is_move ? DAV_VALIDATE_PARENT
: DAV_VALIDATE_RESOURCE
| DAV_VALIDATE_NO_MODIFY)
| DAV_VALIDATE_USE_424,
NULL)) != NULL) {
err = dav_push_error(r->pool, err->status, 0,
apr_psprintf(r->pool,
"Could not %s %s due to a failed "
"precondition on the source "
"(e.g. locks).",
is_move ? "MOVE" : "COPY",
ap_escape_html(r->pool, r->uri)),
err);
return dav_handle_err(r, err, multi_response);
}
if ((err = dav_validate_request(lookup.rnew, resnew, DAV_INFINITY, NULL,
&multi_response,
DAV_VALIDATE_PARENT
| DAV_VALIDATE_USE_424, NULL)) != NULL) {
err = dav_push_error(r->pool, err->status, 0,
apr_psprintf(r->pool,
"Could not MOVE/COPY %s due to a "
"failed precondition on the "
"destination (e.g. locks).",
ap_escape_html(r->pool, r->uri)),
err);
return dav_handle_err(r, err, multi_response);
}
if (is_dir
&& depth == DAV_INFINITY
&& (*resource->hooks->is_parent_resource)(resource, resnew)) {
return dav_error_response(r, HTTP_FORBIDDEN,
"Source collection contains the "
"Destination.");
}
if (is_dir
&& (*resnew->hooks->is_parent_resource)(resnew, resource)) {
return dav_error_response(r, HTTP_FORBIDDEN,
"Destination collection contains the Source "
"and Overwrite has been specified.");
}
if ((result = ap_discard_request_body(r)) != OK) {
return result;
}
if ((err = dav_open_lockdb(r, 0, &lockdb)) != NULL) {
return dav_handle_err(r, err, NULL);
}
if (is_move && lockdb != NULL) {
(void)dav_unlock(r, resource, NULL);
}
if (is_move) {
if ((err = dav_auto_checkout(r, resource, 1 ,
&src_av_info)) != NULL) {
if (lockdb != NULL)
(*lockdb->hooks->close_lockdb)(lockdb);
return dav_handle_err(r, err, NULL);
}
}
resnew_state = dav_get_resource_state(lookup.rnew, resnew);
if (!resnew->exists)
replace_dest = 0;
else if (is_move || !resource->versioned)
replace_dest = 1;
else if (resource->type != resnew->type)
replace_dest = 1;
else if ((resource->collection == 0) != (resnew->collection == 0))
replace_dest = 1;
else
replace_dest = 0;
if (!resnew->exists || replace_dest) {
if ((err = dav_auto_checkout(r, resnew, 1 ,
&dst_av_info)) != NULL) {
if (is_move) {
(void)dav_auto_checkin(r, NULL, 1 ,
0 , &src_av_info);
}
if (lockdb != NULL)
(*lockdb->hooks->close_lockdb)(lockdb);
return dav_handle_err(r, err, NULL);
}
}
if (src_av_info.parent_resource != NULL
&& dst_av_info.parent_resource != NULL
&& (*src_av_info.parent_resource->hooks->is_same_resource)
(src_av_info.parent_resource, dst_av_info.parent_resource)) {
dst_av_info.parent_resource = src_av_info.parent_resource;
}
if (replace_dest)
err = (*resnew->hooks->remove_resource)(resnew, &multi_response);
if (err == NULL) {
if (is_move)
err = (*resource->hooks->move_resource)(resource, resnew,
&multi_response);
else
err = (*resource->hooks->copy_resource)(resource, resnew, depth,
&multi_response);
}
err2 = dav_auto_checkin(r, NULL, err != NULL ,
0 , &dst_av_info);
if (is_move) {
err3 = dav_auto_checkin(r, NULL, err != NULL ,
0 , &src_av_info);
} else
err3 = NULL;
if (err != NULL) {
if (lockdb != NULL)
(*lockdb->hooks->close_lockdb)(lockdb);
err = dav_push_error(r->pool, err->status, 0,
apr_psprintf(r->pool,
"Could not MOVE/COPY %s.",
ap_escape_html(r->pool, r->uri)),
err);
return dav_handle_err(r, err, multi_response);
}
if (err2 != NULL) {
err = dav_push_error(r->pool, err2->status, 0,
"The MOVE/COPY was successful, but there was a "
"problem automatically checking in the "
"source parent collection.",
err2);
dav_log_err(r, err, APLOG_WARNING);
}
if (err3 != NULL) {
err = dav_push_error(r->pool, err3->status, 0,
"The MOVE/COPY was successful, but there was a "
"problem automatically checking in the "
"destination or its parent collection.",
err3);
dav_log_err(r, err, APLOG_WARNING);
}
if (lockdb != NULL) {
err = dav_notify_created(r, lockdb, resnew, resnew_state, depth);
(*lockdb->hooks->close_lockdb)(lockdb);
if (err != NULL) {
err = dav_push_error(r->pool, err->status, 0,
"The MOVE/COPY was successful, but there "
"was a problem updating the lock "
"information.",
err);
return dav_handle_err(r, err, NULL);
}
}
return dav_created(r, lookup.rnew->unparsed_uri, "Destination",
resnew_state == DAV_RESOURCE_EXISTS);
}
static int dav_method_lock(request_rec *r) {
dav_error *err;
dav_resource *resource;
dav_resource *parent;
const dav_hooks_locks *locks_hooks;
int result;
int depth;
int new_lock_request = 0;
apr_xml_doc *doc;
dav_lock *lock;
dav_response *multi_response = NULL;
dav_lockdb *lockdb;
int resource_state;
locks_hooks = DAV_GET_HOOKS_LOCKS(r);
if (locks_hooks == NULL)
return DECLINED;
if ((result = ap_xml_parse_input(r, &doc)) != OK)
return result;
depth = dav_get_depth(r, DAV_INFINITY);
if (depth != 0 && depth != DAV_INFINITY) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(00595)
"Depth must be 0 or \"infinity\" for LOCK.");
return HTTP_BAD_REQUEST;
}
err = dav_get_resource(r, 0 , 0 ,
&resource);
if (err != NULL)
return dav_handle_err(r, err, NULL);
if ((err = resource->hooks->get_parent_resource(resource, &parent)) != NULL) {
return dav_handle_err(r, err, NULL);
}
if (parent && (!parent->exists || parent->collection != 1)) {
err = dav_new_error(r->pool, HTTP_CONFLICT, 0, 0,
apr_psprintf(r->pool,
"The parent resource of %s does not "
"exist or is not a collection.",
ap_escape_html(r->pool, r->uri)));
return dav_handle_err(r, err, NULL);
}
if ((err = (*locks_hooks->open_lockdb)(r, 0, 0, &lockdb)) != NULL) {
return dav_handle_err(r, err, NULL);
}
if (doc != NULL) {
if ((err = dav_lock_parse_lockinfo(r, resource, lockdb, doc,
&lock)) != NULL) {
goto error;
}
new_lock_request = 1;
lock->auth_user = apr_pstrdup(r->pool, r->user);
}
resource_state = dav_get_resource_state(r, resource);
if ((err = dav_validate_request(r, resource, depth, NULL, &multi_response,
(resource_state == DAV_RESOURCE_NULL
? DAV_VALIDATE_PARENT
: DAV_VALIDATE_RESOURCE)
| (new_lock_request ? lock->scope : 0)
| DAV_VALIDATE_ADD_LD,
lockdb)) != OK) {
err = dav_push_error(r->pool, err->status, 0,
apr_psprintf(r->pool,
"Could not LOCK %s due to a failed "
"precondition (e.g. other locks).",
ap_escape_html(r->pool, r->uri)),
err);
goto error;
}
if (new_lock_request == 0) {
dav_locktoken_list *ltl;
if ((err = dav_get_locktoken_list(r, &ltl)) != NULL) {
err = dav_push_error(r->pool, err->status, 0,
apr_psprintf(r->pool,
"The lock refresh for %s failed "
"because no lock tokens were "
"specified in an \"If:\" "
"header.",
ap_escape_html(r->pool, r->uri)),
err);
goto error;
}
if ((err = (*locks_hooks->refresh_locks)(lockdb, resource, ltl,
dav_get_timeout(r),
&lock)) != NULL) {
goto error;
}
} else {
char *locktoken_txt;
dav_dir_conf *conf;
conf = (dav_dir_conf *)ap_get_module_config(r->per_dir_config,
&dav_module);
if (lock->timeout != DAV_TIMEOUT_INFINITE
&& lock->timeout < time(NULL) + conf->locktimeout)
lock->timeout = time(NULL) + conf->locktimeout;
err = dav_add_lock(r, resource, lockdb, lock, &multi_response);
if (err != NULL) {
goto error;
}
locktoken_txt = apr_pstrcat(r->pool, "<",
(*locks_hooks->format_locktoken)(r->pool,
lock->locktoken),
">", NULL);
apr_table_setn(r->headers_out, "Lock-Token", locktoken_txt);
}
(*locks_hooks->close_lockdb)(lockdb);
r->status = HTTP_OK;
ap_set_content_type(r, DAV_XML_CONTENT_TYPE);
ap_rputs(DAV_XML_HEADER DEBUG_CR "<D:prop xmlns:D=\"DAV:\">" DEBUG_CR, r);
if (lock == NULL)
ap_rputs("<D:lockdiscovery/>" DEBUG_CR, r);
else {
ap_rprintf(r,
"<D:lockdiscovery>" DEBUG_CR
"%s" DEBUG_CR
"</D:lockdiscovery>" DEBUG_CR,
dav_lock_get_activelock(r, lock, NULL));
}
ap_rputs("</D:prop>", r);
return DONE;
error:
(*locks_hooks->close_lockdb)(lockdb);
return dav_handle_err(r, err, multi_response);
}
static int dav_method_unlock(request_rec *r) {
dav_error *err;
dav_resource *resource;
const dav_hooks_locks *locks_hooks;
int result;
const char *const_locktoken_txt;
char *locktoken_txt;
dav_locktoken *locktoken = NULL;
int resource_state;
dav_response *multi_response;
locks_hooks = DAV_GET_HOOKS_LOCKS(r);
if (locks_hooks == NULL)
return DECLINED;
if ((const_locktoken_txt = apr_table_get(r->headers_in,
"Lock-Token")) == NULL) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(00596)
"Unlock failed (%s): "
"No Lock-Token specified in header", r->filename);
return HTTP_BAD_REQUEST;
}
locktoken_txt = apr_pstrdup(r->pool, const_locktoken_txt);
if (locktoken_txt[0] != '<') {
return HTTP_BAD_REQUEST;
}
locktoken_txt++;
if (locktoken_txt[strlen(locktoken_txt) - 1] != '>') {
return HTTP_BAD_REQUEST;
}
locktoken_txt[strlen(locktoken_txt) - 1] = '\0';
if ((err = (*locks_hooks->parse_locktoken)(r->pool, locktoken_txt,
&locktoken)) != NULL) {
err = dav_push_error(r->pool, HTTP_BAD_REQUEST, 0,
apr_psprintf(r->pool,
"The UNLOCK on %s failed -- an "
"invalid lock token was specified "
"in the \"If:\" header.",
ap_escape_html(r->pool, r->uri)),
err);
return dav_handle_err(r, err, NULL);
}
err = dav_get_resource(r, 0 , 0 ,
&resource);
if (err != NULL)
return dav_handle_err(r, err, NULL);
resource_state = dav_get_resource_state(r, resource);
if ((err = dav_validate_request(r, resource, 0, locktoken,
&multi_response,
resource_state == DAV_RESOURCE_LOCK_NULL
? DAV_VALIDATE_PARENT
: DAV_VALIDATE_RESOURCE, NULL)) != NULL) {
return dav_handle_err(r, err, multi_response);
}
if ((result = dav_unlock(r, resource, locktoken)) != OK) {
return result;
}
return HTTP_NO_CONTENT;
}
static int dav_method_vsn_control(request_rec *r) {
dav_resource *resource;
int resource_state;
dav_auto_version_info av_info;
const dav_hooks_locks *locks_hooks = DAV_GET_HOOKS_LOCKS(r);
const dav_hooks_vsn *vsn_hooks = DAV_GET_HOOKS_VSN(r);
dav_error *err;
apr_xml_doc *doc;
const char *target = NULL;
int result;
if (vsn_hooks == NULL)
return DECLINED;
err = dav_get_resource(r, 0 , 0 ,
&resource);
if (err != NULL)
return dav_handle_err(r, err, NULL);
resource_state = dav_get_resource_state(r, resource);
if ((result = ap_xml_parse_input(r, &doc)) != OK) {
return result;
}
if (doc != NULL) {
const apr_xml_elem *child;
apr_size_t tsize;
if (!dav_validate_root(doc, "version-control")) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(00597)
"The request body does not contain "
"a \"version-control\" element.");
return HTTP_BAD_REQUEST;
}
if ((child = dav_find_child(doc->root, "version")) == NULL) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(00598)
"The \"version-control\" element does not contain "
"a \"version\" element.");
return HTTP_BAD_REQUEST;
}
if ((child = dav_find_child(child, "href")) == NULL) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(00599)
"The \"version\" element does not contain "
"an \"href\" element.");
return HTTP_BAD_REQUEST;
}
apr_xml_to_text(r->pool, child, APR_XML_X2T_INNER, NULL, NULL,
&target, &tsize);
if (tsize == 0) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(00600)
"An \"href\" element does not contain a URI.");
return HTTP_BAD_REQUEST;
}
}
if (!resource->exists && target == NULL) {
err = dav_new_error(r->pool, HTTP_CONFLICT, 0, 0,
"<DAV:initial-version-required/>");
return dav_handle_err(r, err, NULL);
} else if (resource->exists) {
if (target != NULL) {
err = dav_new_error(r->pool, HTTP_CONFLICT, 0, 0,
"<DAV:cannot-add-to-existing-history/>");
return dav_handle_err(r, err, NULL);
}
if (resource->type != DAV_RESOURCE_TYPE_REGULAR
|| (!resource->versioned && !(vsn_hooks->versionable)(resource))) {
err = dav_new_error(r->pool, HTTP_CONFLICT, 0, 0,
"<DAV:must-be-versionable/>");
return dav_handle_err(r, err, NULL);
}
if (resource->versioned) {
apr_table_setn(r->headers_out, "Cache-Control", "no-cache");
ap_set_content_length(r, 0);
return DONE;
}
}
if ((err = dav_validate_request(r, resource, 0, NULL, NULL,
resource_state == DAV_RESOURCE_NULL ?
DAV_VALIDATE_PARENT :
DAV_VALIDATE_RESOURCE, NULL)) != NULL) {
return dav_handle_err(r, err, NULL);
}
if ((err = dav_auto_checkout(r, resource, 1 ,
&av_info)) != NULL) {
return dav_handle_err(r, err, NULL);
}
if ((err = (*vsn_hooks->vsn_control)(resource, target)) != NULL) {
dav_auto_checkin(r, resource, 1 , 0 , &av_info);
err = dav_push_error(r->pool, HTTP_CONFLICT, 0,
apr_psprintf(r->pool,
"Could not VERSION-CONTROL resource %s.",
ap_escape_html(r->pool, r->uri)),
err);
return dav_handle_err(r, err, NULL);
}
err = dav_auto_checkin(r, resource, 0 , 0 , &av_info);
if (err != NULL) {
err = dav_push_error(r->pool, err->status, 0,
"The VERSION-CONTROL was successful, but there "
"was a problem automatically checking in "
"the parent collection.",
err);
dav_log_err(r, err, APLOG_WARNING);
}
if (locks_hooks != NULL
&& (*locks_hooks->get_supportedlock)(resource) != NULL) {
dav_lockdb *lockdb;
if ((err = (*locks_hooks->open_lockdb)(r, 0, 0, &lockdb)) != NULL) {
err = dav_push_error(r->pool, err->status, 0,
"The VERSION-CONTROL was successful, but there "
"was a problem opening the lock database "
"which prevents inheriting locks from the "
"parent resources.",
err);
return dav_handle_err(r, err, NULL);
}
err = dav_notify_created(r, lockdb, resource, resource_state, 0);
(*locks_hooks->close_lockdb)(lockdb);
if (err != NULL) {
err = dav_push_error(r->pool, err->status, 0,
"The VERSION-CONTROL was successful, but there "
"was a problem updating its lock "
"information.",
err);
return dav_handle_err(r, err, NULL);
}
}
apr_table_setn(r->headers_out, "Cache-Control", "no-cache");
return dav_created(r, resource->uri, "Version selector", 0 );
}
static int dav_method_checkout(request_rec *r) {
dav_resource *resource;
dav_resource *working_resource;
const dav_hooks_vsn *vsn_hooks = DAV_GET_HOOKS_VSN(r);
dav_error *err;
int result;
apr_xml_doc *doc;
int apply_to_vsn = 0;
int is_unreserved = 0;
int is_fork_ok = 0;
int create_activity = 0;
apr_array_header_t *activities = NULL;
if (vsn_hooks == NULL)
return DECLINED;
if ((result = ap_xml_parse_input(r, &doc)) != OK)
return result;
if (doc != NULL) {
const apr_xml_elem *aset;
if (!dav_validate_root(doc, "checkout")) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(00601)
"The request body, if present, must be a "
"DAV:checkout element.");
return HTTP_BAD_REQUEST;
}
if (dav_find_child(doc->root, "apply-to-version") != NULL) {
if (apr_table_get(r->headers_in, "label") != NULL) {
return dav_error_response(r, HTTP_CONFLICT,
"DAV:apply-to-version cannot be "
"used in conjunction with a "
"Label header.");
}
apply_to_vsn = 1;
}
is_unreserved = dav_find_child(doc->root, "unreserved") != NULL;
is_fork_ok = dav_find_child(doc->root, "fork-ok") != NULL;
if ((aset = dav_find_child(doc->root, "activity-set")) != NULL) {
if (dav_find_child(aset, "new") != NULL) {
create_activity = 1;
} else {
const apr_xml_elem *child = aset->first_child;
activities = apr_array_make(r->pool, 1, sizeof(const char *));
for (; child != NULL; child = child->next) {
if (child->ns == APR_XML_NS_DAV_ID
&& strcmp(child->name, "href") == 0) {
const char *href;
href = dav_xml_get_cdata(child, r->pool,
1 );
*(const char **)apr_array_push(activities) = href;
}
}
if (activities->nelts == 0) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(00602)
"Within the DAV:activity-set element, the "
"DAV:new element must be used, or at least "
"one DAV:href must be specified.");
return HTTP_BAD_REQUEST;
}
}
}
}
err = dav_get_resource(r, 1 , apply_to_vsn, &resource);
if (err != NULL)
return dav_handle_err(r, err, NULL);
if (!resource->exists) {
return HTTP_NOT_FOUND;
}
if (resource->type != DAV_RESOURCE_TYPE_REGULAR
&& resource->type != DAV_RESOURCE_TYPE_VERSION) {
return dav_error_response(r, HTTP_CONFLICT,
"Cannot checkout this type of resource.");
}
if (!resource->versioned) {
return dav_error_response(r, HTTP_CONFLICT,
"Cannot checkout unversioned resource.");
}
if (resource->working) {
return dav_error_response(r, HTTP_CONFLICT,
"The resource is already checked out to the workspace.");
}
if ((err = (*vsn_hooks->checkout)(resource, 0 ,
is_unreserved, is_fork_ok,
create_activity, activities,
&working_resource)) != NULL) {
err = dav_push_error(r->pool, HTTP_CONFLICT, 0,
apr_psprintf(r->pool,
"Could not CHECKOUT resource %s.",
ap_escape_html(r->pool, r->uri)),
err);
return dav_handle_err(r, err, NULL);
}
apr_table_setn(r->headers_out, "Cache-Control", "no-cache");
if (working_resource == NULL) {
ap_set_content_length(r, 0);
return DONE;
}
return dav_created(r, working_resource->uri, "Checked-out resource", 0);
}
static int dav_method_uncheckout(request_rec *r) {
dav_resource *resource;
const dav_hooks_vsn *vsn_hooks = DAV_GET_HOOKS_VSN(r);
dav_error *err;
int result;
if (vsn_hooks == NULL)
return DECLINED;
if ((result = ap_discard_request_body(r)) != OK) {
return result;
}
err = dav_get_resource(r, 0 , 0 ,
&resource);
if (err != NULL)
return dav_handle_err(r, err, NULL);
if (!resource->exists) {
return HTTP_NOT_FOUND;
}
if (resource->type != DAV_RESOURCE_TYPE_REGULAR) {
return dav_error_response(r, HTTP_CONFLICT,
"Cannot uncheckout this type of resource.");
}
if (!resource->versioned) {
return dav_error_response(r, HTTP_CONFLICT,
"Cannot uncheckout unversioned resource.");
}
if (!resource->working) {
return dav_error_response(r, HTTP_CONFLICT,
"The resource is not checked out to the workspace.");
}
if ((err = (*vsn_hooks->uncheckout)(resource)) != NULL) {
err = dav_push_error(r->pool, HTTP_CONFLICT, 0,
apr_psprintf(r->pool,
"Could not UNCHECKOUT resource %s.",
ap_escape_html(r->pool, r->uri)),
err);
return dav_handle_err(r, err, NULL);
}
ap_set_content_length(r, 0);
return DONE;
}
static int dav_method_checkin(request_rec *r) {
dav_resource *resource;
dav_resource *new_version;
const dav_hooks_vsn *vsn_hooks = DAV_GET_HOOKS_VSN(r);
dav_error *err;
int result;
apr_xml_doc *doc;
int keep_checked_out = 0;
if (vsn_hooks == NULL)
return DECLINED;
if ((result = ap_xml_parse_input(r, &doc)) != OK)
return result;
if (doc != NULL) {
if (!dav_validate_root(doc, "checkin")) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(00603)
"The request body, if present, must be a "
"DAV:checkin element.");
return HTTP_BAD_REQUEST;
}
keep_checked_out = dav_find_child(doc->root, "keep-checked-out") != NULL;
}
err = dav_get_resource(r, 0 , 0 ,
&resource);
if (err != NULL)
return dav_handle_err(r, err, NULL);
if (!resource->exists) {
return HTTP_NOT_FOUND;
}
if (resource->type != DAV_RESOURCE_TYPE_REGULAR) {
return dav_error_response(r, HTTP_CONFLICT,
"Cannot checkin this type of resource.");
}
if (!resource->versioned) {
return dav_error_response(r, HTTP_CONFLICT,
"Cannot checkin unversioned resource.");
}
if (!resource->working) {
return dav_error_response(r, HTTP_CONFLICT,
"The resource is not checked out.");
}
if ((err = (*vsn_hooks->checkin)(resource, keep_checked_out, &new_version))
!= NULL) {
err = dav_push_error(r->pool, HTTP_CONFLICT, 0,
apr_psprintf(r->pool,
"Could not CHECKIN resource %s.",
ap_escape_html(r->pool, r->uri)),
err);
return dav_handle_err(r, err, NULL);
}
return dav_created(r, new_version->uri, "Version", 0);
}
static int dav_method_update(request_rec *r) {
dav_resource *resource;
dav_resource *version = NULL;
const dav_hooks_vsn *vsn_hooks = DAV_GET_HOOKS_VSN(r);
apr_xml_doc *doc;
apr_xml_elem *child;
int is_label = 0;
int depth;
int result;
apr_size_t tsize;
const char *target;
dav_response *multi_response;
dav_error *err;
dav_lookup_result lookup;
if (vsn_hooks == NULL || vsn_hooks->update == NULL)
return DECLINED;
if ((depth = dav_get_depth(r, 0)) < 0) {
return HTTP_BAD_REQUEST;
}
if ((result = ap_xml_parse_input(r, &doc)) != OK) {
return result;
}
if (doc == NULL || !dav_validate_root(doc, "update")) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(00604)
"The request body does not contain "
"an \"update\" element.");
return HTTP_BAD_REQUEST;
}
if ((child = dav_find_child(doc->root, "label-name")) != NULL)
is_label = 1;
else if ((child = dav_find_child(doc->root, "version")) != NULL) {
if ((child = dav_find_child(child, "href")) == NULL) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(00605)
"The version element does not contain "
"an \"href\" element.");
return HTTP_BAD_REQUEST;
}
} else {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(00606)
"The \"update\" element does not contain "
"a \"label-name\" or \"version\" element.");
return HTTP_BAD_REQUEST;
}
if (!is_label && depth != 0) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(00607)
"Depth must be zero for UPDATE with a version");
return HTTP_BAD_REQUEST;
}
apr_xml_to_text(r->pool, child, APR_XML_X2T_INNER, NULL, NULL,
&target, &tsize);
if (tsize == 0) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(00608)
"A \"label-name\" or \"href\" element does not contain "
"any content.");
return HTTP_BAD_REQUEST;
}
err = dav_get_resource(r, 0 , 0 ,
&resource);
if (err != NULL)
return dav_handle_err(r, err, NULL);
if (!resource->exists) {
return HTTP_NOT_FOUND;
}
if (resource->type != DAV_RESOURCE_TYPE_REGULAR
|| !resource->versioned || resource->working) {
return dav_error_response(r, HTTP_CONFLICT,
"<DAV:must-be-checked-in-version-controlled-resource>");
}
if (!is_label) {
lookup = dav_lookup_uri(target, r, 0 );
if (lookup.rnew == NULL) {
if (lookup.err.status == HTTP_BAD_REQUEST) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(00609)
"%s", lookup.err.desc);
return HTTP_BAD_REQUEST;
}
return dav_error_response(r, lookup.err.status, lookup.err.desc);
}
if (lookup.rnew->status != HTTP_OK) {
return dav_error_response(r, lookup.rnew->status,
"Version URI had an error.");
}
err = dav_get_resource(lookup.rnew, 0 ,
0 , &version);
if (err != NULL)
return dav_handle_err(r, err, NULL);
target = NULL;
}
err = (*vsn_hooks->update)(resource, version, target, depth, &multi_response);
if (err != NULL) {
err = dav_push_error(r->pool, err->status, 0,
apr_psprintf(r->pool,
"Could not UPDATE %s.",
ap_escape_html(r->pool, r->uri)),
err);
return dav_handle_err(r, err, multi_response);
}
apr_table_setn(r->headers_out, "Cache-Control", "no-cache");
ap_set_content_length(r, 0);
return DONE;
}
typedef struct dav_label_walker_ctx {
dav_walk_params w;
const char *label;
int label_op;
#define DAV_LABEL_ADD 1
#define DAV_LABEL_SET 2
#define DAV_LABEL_REMOVE 3
const dav_hooks_vsn *vsn_hooks;
} dav_label_walker_ctx;
static dav_error * dav_label_walker(dav_walk_resource *wres, int calltype) {
dav_label_walker_ctx *ctx = wres->walk_ctx;
dav_error *err = NULL;
if (wres->resource->type != DAV_RESOURCE_TYPE_VERSION &&
(wres->resource->type != DAV_RESOURCE_TYPE_REGULAR
|| !wres->resource->versioned)) {
err = dav_new_error(ctx->w.pool, HTTP_CONFLICT, 0, 0,
"<DAV:must-be-version-or-version-selector/>");
} else if (wres->resource->working) {
err = dav_new_error(ctx->w.pool, HTTP_CONFLICT, 0, 0,
"<DAV:must-not-be-checked-out/>");
} else {
if (ctx->label_op == DAV_LABEL_REMOVE)
err = (*ctx->vsn_hooks->remove_label)(wres->resource, ctx->label);
else
err = (*ctx->vsn_hooks->add_label)(wres->resource, ctx->label,
ctx->label_op == DAV_LABEL_SET);
}
if (err != NULL) {
dav_add_response(wres, err->status, NULL);
wres->response->desc = err->desc;
}
return NULL;
}
static int dav_method_label(request_rec *r) {
dav_resource *resource;
const dav_hooks_vsn *vsn_hooks = DAV_GET_HOOKS_VSN(r);
apr_xml_doc *doc;
apr_xml_elem *child;
int depth;
int result;
apr_size_t tsize;
dav_error *err;
dav_label_walker_ctx ctx = { { 0 } };
dav_response *multi_status;
if (vsn_hooks == NULL || vsn_hooks->add_label == NULL)
return DECLINED;
err = dav_get_resource(r, 1 , 0 ,
&resource);
if (err != NULL)
return dav_handle_err(r, err, NULL);
if (!resource->exists) {
return HTTP_NOT_FOUND;
}
if ((depth = dav_get_depth(r, 0)) < 0) {
return HTTP_BAD_REQUEST;
}
if ((result = ap_xml_parse_input(r, &doc)) != OK) {
return result;
}
if (doc == NULL || !dav_validate_root(doc, "label")) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(00610)
"The request body does not contain "
"a \"label\" element.");
return HTTP_BAD_REQUEST;
}
if ((child = dav_find_child(doc->root, "add")) != NULL) {
ctx.label_op = DAV_LABEL_ADD;
} else if ((child = dav_find_child(doc->root, "set")) != NULL) {
ctx.label_op = DAV_LABEL_SET;
} else if ((child = dav_find_child(doc->root, "remove")) != NULL) {
ctx.label_op = DAV_LABEL_REMOVE;
} else {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(00611)
"The \"label\" element does not contain "
"an \"add\", \"set\", or \"remove\" element.");
return HTTP_BAD_REQUEST;
}
if ((child = dav_find_child(child, "label-name")) == NULL) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(00612)
"The label command element does not contain "
"a \"label-name\" element.");
return HTTP_BAD_REQUEST;
}
apr_xml_to_text(r->pool, child, APR_XML_X2T_INNER, NULL, NULL,
&ctx.label, &tsize);
if (tsize == 0) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(00613)
"A \"label-name\" element does not contain "
"a label name.");
return HTTP_BAD_REQUEST;
}
ctx.w.walk_type = DAV_WALKTYPE_NORMAL;
ctx.w.func = dav_label_walker;
ctx.w.walk_ctx = &ctx;
ctx.w.pool = r->pool;
ctx.w.root = resource;
ctx.vsn_hooks = vsn_hooks;
err = (*resource->hooks->walk)(&ctx.w, depth, &multi_status);
if (err != NULL) {
err = dav_push_error(r->pool, err->status, 0,
"The LABEL operation was terminated prematurely.",
err);
return dav_handle_err(r, err, multi_status);
}
if (multi_status != NULL) {
if (depth == 0) {
err = dav_new_error(r->pool, multi_status->status, 0, 0,
multi_status->desc);
multi_status = NULL;
} else {
err = dav_new_error(r->pool, HTTP_MULTI_STATUS, 0, 0,
"Errors occurred during the LABEL operation.");
}
return dav_handle_err(r, err, multi_status);
}
apr_table_setn(r->headers_out, "Cache-Control", "no-cache");
ap_set_content_length(r, 0);
return DONE;
}
static int dav_method_report(request_rec *r) {
dav_resource *resource;
const dav_hooks_vsn *vsn_hooks = DAV_GET_HOOKS_VSN(r);
int result;
int label_allowed;
apr_xml_doc *doc;
dav_error *err;
if (vsn_hooks == NULL)
return DECLINED;
if ((result = ap_xml_parse_input(r, &doc)) != OK)
return result;
if (doc == NULL) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(00614)
"The request body must specify a report.");
return HTTP_BAD_REQUEST;
}
label_allowed = (*vsn_hooks->report_label_header_allowed)(doc);
err = dav_get_resource(r, label_allowed, 0 ,
&resource);
if (err != NULL)
return dav_handle_err(r, err, NULL);
if (!resource->exists) {
return HTTP_NOT_FOUND;
}
r->status = HTTP_OK;
ap_set_content_type(r, DAV_XML_CONTENT_TYPE);
if ((err = (*vsn_hooks->deliver_report)(r, resource, doc,
r->output_filters)) != NULL) {
if (! r->sent_bodyct)
return dav_handle_err(r, err, NULL);
err = dav_push_error(r->pool, err->status, 0,
"Provider encountered an error while streaming"
" a REPORT response.", err);
dav_log_err(r, err, APLOG_ERR);
r->connection->aborted = 1;
return DONE;
}
return DONE;
}
static int dav_method_make_workspace(request_rec *r) {
dav_resource *resource;
const dav_hooks_vsn *vsn_hooks = DAV_GET_HOOKS_VSN(r);
dav_error *err;
apr_xml_doc *doc;
int result;
if (vsn_hooks == NULL || vsn_hooks->make_workspace == NULL)
return DECLINED;
err = dav_get_resource(r, 0 , 0 ,
&resource);
if (err != NULL)
return dav_handle_err(r, err, NULL);
if ((result = ap_xml_parse_input(r, &doc)) != OK) {
return result;
}
if (doc == NULL
|| !dav_validate_root(doc, "mkworkspace")) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(00615)
"The request body does not contain "
"a \"mkworkspace\" element.");
return HTTP_BAD_REQUEST;
}
if (resource->exists) {
err = dav_new_error(r->pool, HTTP_CONFLICT, 0, 0,
"<DAV:resource-must-be-null/>");
return dav_handle_err(r, err, NULL);
}
if ((err = (*vsn_hooks->make_workspace)(resource, doc)) != NULL) {
err = dav_push_error(r->pool, err->status, 0,
apr_psprintf(r->pool,
"Could not create workspace %s.",
ap_escape_html(r->pool, r->uri)),
err);
return dav_handle_err(r, err, NULL);
}
apr_table_setn(r->headers_out, "Cache-Control", "no-cache");
return dav_created(r, resource->uri, "Workspace", 0 );
}
static int dav_method_make_activity(request_rec *r) {
dav_resource *resource;
const dav_hooks_vsn *vsn_hooks = DAV_GET_HOOKS_VSN(r);
dav_error *err;
int result;
if (vsn_hooks == NULL || vsn_hooks->make_activity == NULL)
return DECLINED;
err = dav_get_resource(r, 0 , 0 ,
&resource);
if (err != NULL)
return dav_handle_err(r, err, NULL);
if ((result = ap_discard_request_body(r)) != OK) {
return result;
}
if (resource->exists) {
err = dav_new_error(r->pool, HTTP_CONFLICT, 0, 0,
"<DAV:resource-must-be-null/>");
return dav_handle_err(r, err, NULL);
}
if (vsn_hooks->can_be_activity != NULL
&& !(*vsn_hooks->can_be_activity)(resource)) {
err = dav_new_error(r->pool, HTTP_FORBIDDEN, 0, 0,
"<DAV:activity-location-ok/>");
return dav_handle_err(r, err, NULL);
}
if ((err = (*vsn_hooks->make_activity)(resource)) != NULL) {
err = dav_push_error(r->pool, err->status, 0,
apr_psprintf(r->pool,
"Could not create activity %s.",
ap_escape_html(r->pool, r->uri)),
err);
return dav_handle_err(r, err, NULL);
}
apr_table_setn(r->headers_out, "Cache-Control", "no-cache");
return dav_created(r, resource->uri, "Activity", 0 );
}
static int dav_method_baseline_control(request_rec *r) {
return HTTP_METHOD_NOT_ALLOWED;
}
static int dav_method_merge(request_rec *r) {
dav_resource *resource;
dav_resource *source_resource;
const dav_hooks_vsn *vsn_hooks = DAV_GET_HOOKS_VSN(r);
dav_error *err;
int result;
apr_xml_doc *doc;
apr_xml_elem *source_elem;
apr_xml_elem *href_elem;
apr_xml_elem *prop_elem;
const char *source;
int no_auto_merge;
int no_checkout;
dav_lookup_result lookup;
if (vsn_hooks == NULL)
return DECLINED;
if ((result = ap_xml_parse_input(r, &doc)) != OK)
return result;
if (doc == NULL || !dav_validate_root(doc, "merge")) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(00616)
"The request body must be present and must be a "
"DAV:merge element.");
return HTTP_BAD_REQUEST;
}
if ((source_elem = dav_find_child(doc->root, "source")) == NULL) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(00617)
"The DAV:merge element must contain a DAV:source "
"element.");
return HTTP_BAD_REQUEST;
}
if ((href_elem = dav_find_child(source_elem, "href")) == NULL) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(00618)
"The DAV:source element must contain a DAV:href "
"element.");
return HTTP_BAD_REQUEST;
}
source = dav_xml_get_cdata(href_elem, r->pool, 1 );
lookup = dav_lookup_uri(source, r, 0 );
if (lookup.rnew == NULL) {
if (lookup.err.status == HTTP_BAD_REQUEST) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(00619)
"%s", lookup.err.desc);
return HTTP_BAD_REQUEST;
}
return dav_error_response(r, lookup.err.status, lookup.err.desc);
}
if (lookup.rnew->status != HTTP_OK) {
return dav_error_response(r, lookup.rnew->status,
"Merge source URI had an error.");
}
err = dav_get_resource(lookup.rnew, 0 ,
0 , &source_resource);
if (err != NULL)
return dav_handle_err(r, err, NULL);
no_auto_merge = dav_find_child(doc->root, "no-auto-merge") != NULL;
no_checkout = dav_find_child(doc->root, "no-checkout") != NULL;
prop_elem = dav_find_child(doc->root, "prop");
err = dav_get_resource(r, 0 , 0 ,
&resource);
if (err != NULL)
return dav_handle_err(r, err, NULL);
if (!resource->exists) {
return HTTP_NOT_FOUND;
}
apr_table_setn(r->headers_out, "Cache-Control", "no-cache");
r->status = HTTP_OK;
ap_set_content_type(r, "text/xml");
if ((err = (*vsn_hooks->merge)(resource, source_resource,
no_auto_merge, no_checkout,
prop_elem,
r->output_filters)) != NULL) {
err = dav_push_error(r->pool, err->status, 0,
apr_psprintf(r->pool,
"Could not MERGE resource \"%s\" "
"into \"%s\".",
ap_escape_html(r->pool, source),
ap_escape_html(r->pool, r->uri)),
err);
return dav_handle_err(r, err, NULL);
}
return DONE;
}
static int dav_method_bind(request_rec *r) {
dav_resource *resource;
dav_resource *binding;
dav_auto_version_info av_info;
const dav_hooks_binding *binding_hooks = DAV_GET_HOOKS_BINDING(r);
const char *dest;
dav_error *err;
dav_error *err2;
dav_response *multi_response = NULL;
dav_lookup_result lookup;
int overwrite;
if (binding_hooks == NULL)
return DECLINED;
err = dav_get_resource(r, 0 , 0 ,
&resource);
if (err != NULL)
return dav_handle_err(r, err, NULL);
if (!resource->exists) {
return HTTP_NOT_FOUND;
}
dest = apr_table_get(r->headers_in, "Destination");
if (dest == NULL) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(00620)
"The request is missing a Destination header.");
return HTTP_BAD_REQUEST;
}
lookup = dav_lookup_uri(dest, r, 0 );
if (lookup.rnew == NULL) {
if (lookup.err.status == HTTP_BAD_REQUEST) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(00621)
"%s", lookup.err.desc);
return HTTP_BAD_REQUEST;
} else if (lookup.err.status == HTTP_BAD_GATEWAY) {
return dav_error_response(r, HTTP_FORBIDDEN,
"Cross server bindings are not "
"allowed by this server.");
}
return dav_error_response(r, lookup.err.status, lookup.err.desc);
}
if (lookup.rnew->status != HTTP_OK) {
return dav_error_response(r, lookup.rnew->status,
"Destination URI had an error.");
}
err = dav_get_resource(lookup.rnew, 0 ,
0 , &binding);
if (err != NULL)
return dav_handle_err(r, err, NULL);
if (resource->hooks != binding->hooks) {
return dav_error_response(r, HTTP_BAD_GATEWAY,
"Destination URI is handled by a "
"different repository than the source URI. "
"BIND between repositories is not possible.");
}
if ((overwrite = dav_get_overwrite(r)) < 0) {
return HTTP_BAD_REQUEST;
}
if (binding->exists && !overwrite) {
return dav_error_response(r, HTTP_PRECONDITION_FAILED,
"Destination is not empty and "
"Overwrite is not \"T\"");
}
if ((*resource->hooks->is_same_resource)(resource, binding)) {
return dav_error_response(r, HTTP_FORBIDDEN,
"Source and Destination URIs are the same.");
}
if ((err = dav_validate_request(lookup.rnew, binding, DAV_INFINITY, NULL,
&multi_response,
DAV_VALIDATE_PARENT
| DAV_VALIDATE_USE_424, NULL)) != NULL) {
err = dav_push_error(r->pool, err->status, 0,
apr_psprintf(r->pool,
"Could not BIND %s due to a "
"failed precondition on the "
"destination (e.g. locks).",
ap_escape_html(r->pool, r->uri)),
err);
return dav_handle_err(r, err, multi_response);
}
if (resource->collection
&& (*resource->hooks->is_parent_resource)(resource, binding)) {
return dav_error_response(r, HTTP_FORBIDDEN,
"Source collection contains the Destination.");
}
if (resource->collection
&& (*resource->hooks->is_parent_resource)(binding, resource)) {
return dav_error_response(r, HTTP_FORBIDDEN,
"Destination collection contains the Source and "
"Overwrite has been specified.");
}
if ((err = dav_auto_checkout(r, binding, 1 ,
&av_info)) != NULL) {
return dav_handle_err(r, err, NULL);
}
if (binding->exists)
err = (*resource->hooks->remove_resource)(binding, &multi_response);
if (err == NULL) {
err = (*binding_hooks->bind_resource)(resource, binding);
}
err2 = dav_auto_checkin(r, NULL,
err != NULL ,
0 , &av_info);
if (err != NULL) {
err = dav_push_error(r->pool, err->status, 0,
apr_psprintf(r->pool,
"Could not BIND %s.",
ap_escape_html(r->pool, r->uri)),
err);
return dav_handle_err(r, err, multi_response);
}
if (err2 != NULL) {
err = dav_push_error(r->pool, err2->status, 0,
"The BIND was successful, but there was a "
"problem automatically checking in the "
"source parent collection.",
err2);
dav_log_err(r, err, APLOG_WARNING);
}
return dav_created(r, lookup.rnew->unparsed_uri, "Binding", 0);
}
static int dav_handler(request_rec *r) {
if (strcmp(r->handler, DAV_HANDLER_NAME) != 0)
return DECLINED;
if (r->parsed_uri.fragment != NULL) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(00622)
"buggy client used un-escaped hash in Request-URI");
return dav_error_response(r, HTTP_BAD_REQUEST,
"The request was invalid: the URI included "
"an un-escaped hash character");
}
r->allowed = 0
| (AP_METHOD_BIT << M_GET)
| (AP_METHOD_BIT << M_PUT)
| (AP_METHOD_BIT << M_DELETE)
| (AP_METHOD_BIT << M_OPTIONS)
| (AP_METHOD_BIT << M_INVALID);
r->allowed |= 0
| (AP_METHOD_BIT << M_COPY)
| (AP_METHOD_BIT << M_LOCK)
| (AP_METHOD_BIT << M_UNLOCK)
| (AP_METHOD_BIT << M_MKCOL)
| (AP_METHOD_BIT << M_MOVE)
| (AP_METHOD_BIT << M_PROPFIND)
| (AP_METHOD_BIT << M_PROPPATCH);
r->allowed |= 0
| (AP_METHOD_BIT << M_POST);
if (r->method_number == M_GET) {
return dav_method_get(r);
}
if (r->method_number == M_PUT) {
return dav_method_put(r);
}
if (r->method_number == M_POST) {
return dav_method_post(r);
}
if (r->method_number == M_DELETE) {
return dav_method_delete(r);
}
if (r->method_number == M_OPTIONS) {
return dav_method_options(r);
}
if (r->method_number == M_PROPFIND) {
return dav_method_propfind(r);
}
if (r->method_number == M_PROPPATCH) {
return dav_method_proppatch(r);
}
if (r->method_number == M_MKCOL) {
return dav_method_mkcol(r);
}
if (r->method_number == M_COPY) {
return dav_method_copymove(r, DAV_DO_COPY);
}
if (r->method_number == M_MOVE) {
return dav_method_copymove(r, DAV_DO_MOVE);
}
if (r->method_number == M_LOCK) {
return dav_method_lock(r);
}
if (r->method_number == M_UNLOCK) {
return dav_method_unlock(r);
}
if (r->method_number == M_VERSION_CONTROL) {
return dav_method_vsn_control(r);
}
if (r->method_number == M_CHECKOUT) {
return dav_method_checkout(r);
}
if (r->method_number == M_UNCHECKOUT) {
return dav_method_uncheckout(r);
}
if (r->method_number == M_CHECKIN) {
return dav_method_checkin(r);
}
if (r->method_number == M_UPDATE) {
return dav_method_update(r);
}
if (r->method_number == M_LABEL) {
return dav_method_label(r);
}
if (r->method_number == M_REPORT) {
return dav_method_report(r);
}
if (r->method_number == M_MKWORKSPACE) {
return dav_method_make_workspace(r);
}
if (r->method_number == M_MKACTIVITY) {
return dav_method_make_activity(r);
}
if (r->method_number == M_BASELINE_CONTROL) {
return dav_method_baseline_control(r);
}
if (r->method_number == M_MERGE) {
return dav_method_merge(r);
}
if (r->method_number == dav_methods[DAV_M_BIND]) {
return dav_method_bind(r);
}
if (r->method_number == dav_methods[DAV_M_SEARCH]) {
return dav_method_search(r);
}
return DECLINED;
}
static int dav_fixups(request_rec *r) {
dav_dir_conf *conf;
if (r->assbackwards && !r->main) {
return DECLINED;
}
conf = (dav_dir_conf *)ap_get_module_config(r->per_dir_config,
&dav_module);
if (conf->provider == NULL) {
return DECLINED;
}
if (r->method_number == M_GET) {
if (!conf->provider->repos->handle_get) {
return DECLINED;
}
}
if (r->method_number != M_POST) {
r->handler = DAV_HANDLER_NAME;
return OK;
}
return DECLINED;
}
static void register_hooks(apr_pool_t *p) {
ap_hook_handler(dav_handler, NULL, NULL, APR_HOOK_MIDDLE);
ap_hook_post_config(dav_init_handler, NULL, NULL, APR_HOOK_MIDDLE);
ap_hook_fixups(dav_fixups, NULL, NULL, APR_HOOK_MIDDLE);
dav_hook_find_liveprop(dav_core_find_liveprop, NULL, NULL, APR_HOOK_LAST);
dav_hook_insert_all_liveprops(dav_core_insert_all_liveprops,
NULL, NULL, APR_HOOK_MIDDLE);
dav_core_register_uris(p);
}
static const command_rec dav_cmds[] = {
AP_INIT_TAKE1("DAV", dav_cmd_dav, NULL, ACCESS_CONF,
"specify the DAV provider for a directory or location"),
AP_INIT_TAKE1("DAVMinTimeout", dav_cmd_davmintimeout, NULL,
ACCESS_CONF|RSRC_CONF,
"specify minimum allowed timeout"),
AP_INIT_FLAG("DAVDepthInfinity", dav_cmd_davdepthinfinity, NULL,
ACCESS_CONF|RSRC_CONF,
"allow Depth infinity PROPFIND requests"),
{ NULL }
};
module DAV_DECLARE_DATA dav_module = {
STANDARD20_MODULE_STUFF,
dav_create_dir_config,
dav_merge_dir_config,
dav_create_server_config,
dav_merge_server_config,
dav_cmds,
register_hooks,
};
APR_HOOK_STRUCT(
APR_HOOK_LINK(gather_propsets)
APR_HOOK_LINK(find_liveprop)
APR_HOOK_LINK(insert_all_liveprops)
)
APR_IMPLEMENT_EXTERNAL_HOOK_VOID(dav, DAV, gather_propsets,
(apr_array_header_t *uris),
(uris))
APR_IMPLEMENT_EXTERNAL_HOOK_RUN_FIRST(dav, DAV, int, find_liveprop,
(const dav_resource *resource,
const char *ns_uri, const char *name,
const dav_hooks_liveprop **hooks),
(resource, ns_uri, name, hooks), 0)
APR_IMPLEMENT_EXTERNAL_HOOK_VOID(dav, DAV, insert_all_liveprops,
(request_rec *r, const dav_resource *resource,
dav_prop_insert what, apr_text_header *phdr),
(r, resource, what, phdr))
