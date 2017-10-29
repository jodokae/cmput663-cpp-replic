#include "apr.h"
#include "apr_strings.h"
#define APR_WANT_STDIO
#define APR_WANT_BYTEFUNC
#include "apr_want.h"
#include "mod_dav.h"
#include "http_log.h"
#include "http_request.h"
#define DAV_DISABLE_WRITABLE_PROPS 1
#define DAV_EMPTY_VALUE "\0"
struct dav_propdb {
apr_pool_t *p;
request_rec *r;
const dav_resource *resource;
int deferred;
dav_db *db;
apr_array_header_t *ns_xlate;
dav_namespace_map *mapping;
dav_lockdb *lockdb;
dav_buffer wb_lock;
request_rec *subreq;
const dav_hooks_db *db_hooks;
};
static const char * const dav_core_props[] = {
"getcontenttype",
"getcontentlanguage",
"lockdiscovery",
"supportedlock",
NULL
};
enum {
DAV_PROPID_CORE_getcontenttype = DAV_PROPID_CORE,
DAV_PROPID_CORE_getcontentlanguage,
DAV_PROPID_CORE_lockdiscovery,
DAV_PROPID_CORE_supportedlock,
DAV_PROPID_CORE_UNKNOWN
};
typedef struct dav_rollback_item {
dav_deadprop_rollback *deadprop;
dav_liveprop_rollback *liveprop;
} dav_rollback_item;
static int dav_find_liveprop_provider(dav_propdb *propdb,
const char *ns_uri,
const char *propname,
const dav_hooks_liveprop **provider) {
int propid;
*provider = NULL;
if (ns_uri == NULL) {
return DAV_PROPID_CORE_UNKNOWN;
}
propid = dav_run_find_liveprop(propdb->resource, ns_uri, propname,
provider);
if (propid != 0) {
return propid;
}
if (strcmp(ns_uri, "DAV:") == 0) {
const char * const *p = dav_core_props;
for (propid = DAV_PROPID_CORE; *p != NULL; ++p, ++propid)
if (strcmp(propname, *p) == 0) {
return propid;
}
}
return DAV_PROPID_CORE_UNKNOWN;
}
static void dav_find_liveprop(dav_propdb *propdb, apr_xml_elem *elem) {
const char *ns_uri;
dav_elem_private *priv = elem->priv;
const dav_hooks_liveprop *hooks;
if (elem->ns == APR_XML_NS_NONE)
ns_uri = NULL;
else if (elem->ns == APR_XML_NS_DAV_ID)
ns_uri = "DAV:";
else
ns_uri = APR_XML_GET_URI_ITEM(propdb->ns_xlate, elem->ns);
priv->propid = dav_find_liveprop_provider(propdb, ns_uri, elem->name,
&hooks);
if (priv->propid != DAV_PROPID_CORE_UNKNOWN) {
priv->provider = hooks;
}
}
static int dav_rw_liveprop(dav_propdb *propdb, dav_elem_private *priv) {
int propid = priv->propid;
if (priv->provider != NULL) {
return (*priv->provider->is_writable)(propdb->resource, propid);
}
if (propid == DAV_PROPID_CORE_lockdiscovery
#if DAV_DISABLE_WRITABLE_PROPS
|| propid == DAV_PROPID_CORE_getcontenttype
|| propid == DAV_PROPID_CORE_getcontentlanguage
#endif
|| propid == DAV_PROPID_CORE_supportedlock
) {
return 0;
}
if (propid == DAV_PROPID_CORE_getcontenttype
|| propid == DAV_PROPID_CORE_getcontentlanguage
|| propid == DAV_PROPID_CORE_UNKNOWN) {
return 1;
}
return 1;
}
static void dav_do_prop_subreq(dav_propdb *propdb) {
const char *e_uri = ap_escape_uri(propdb->resource->pool,
propdb->resource->uri);
propdb->subreq = ap_sub_req_lookup_uri(e_uri, propdb->r, NULL);
}
static dav_error * dav_insert_coreprop(dav_propdb *propdb,
int propid, const char *name,
dav_prop_insert what,
apr_text_header *phdr,
dav_prop_insert *inserted) {
const char *value = NULL;
dav_error *err;
*inserted = DAV_PROP_INSERT_NOTDEF;
if (propid == DAV_PROPID_CORE_UNKNOWN)
return NULL;
switch (propid) {
case DAV_PROPID_CORE_lockdiscovery:
if (propdb->lockdb != NULL) {
dav_lock *locks;
if ((err = dav_lock_query(propdb->lockdb, propdb->resource,
&locks)) != NULL) {
return dav_push_error(propdb->p, err->status, 0,
"DAV:lockdiscovery could not be "
"determined due to a problem fetching "
"the locks for this resource.",
err);
}
if (locks == NULL) {
value = "";
} else {
value = dav_lock_get_activelock(propdb->r, locks,
&propdb->wb_lock);
value = apr_pstrdup(propdb->p, propdb->wb_lock.buf);
}
}
break;
case DAV_PROPID_CORE_supportedlock:
if (propdb->lockdb != NULL) {
value = (*propdb->lockdb->hooks->get_supportedlock)(propdb->resource);
}
break;
case DAV_PROPID_CORE_getcontenttype:
if (propdb->subreq == NULL) {
dav_do_prop_subreq(propdb);
}
if (propdb->subreq->content_type != NULL) {
value = propdb->subreq->content_type;
}
break;
case DAV_PROPID_CORE_getcontentlanguage: {
const char *lang;
if (propdb->subreq == NULL) {
dav_do_prop_subreq(propdb);
}
if ((lang = apr_table_get(propdb->subreq->headers_out,
"Content-Language")) != NULL) {
value = lang;
}
break;
}
default:
break;
}
if (value != NULL) {
const char *s;
if (what == DAV_PROP_INSERT_SUPPORTED) {
s = apr_psprintf(propdb->p,
"<D:supported-live-property D:name=\"%s\"/>" DEBUG_CR,
name);
} else if (what == DAV_PROP_INSERT_VALUE && *value != '\0') {
s = apr_psprintf(propdb->p, "<D:%s>%s</D:%s>" DEBUG_CR,
name, value, name);
} else {
s = apr_psprintf(propdb->p, "<D:%s/>" DEBUG_CR, name);
}
apr_text_append(propdb->p, phdr, s);
*inserted = what;
}
return NULL;
}
static dav_error * dav_insert_liveprop(dav_propdb *propdb,
const apr_xml_elem *elem,
dav_prop_insert what,
apr_text_header *phdr,
dav_prop_insert *inserted) {
dav_elem_private *priv = elem->priv;
*inserted = DAV_PROP_INSERT_NOTDEF;
if (priv->provider == NULL) {
return dav_insert_coreprop(propdb, priv->propid, elem->name,
what, phdr, inserted);
}
*inserted = (*priv->provider->insert_prop)(propdb->resource, priv->propid,
what, phdr);
return NULL;
}
static void dav_output_prop_name(apr_pool_t *pool,
const dav_prop_name *name,
dav_xmlns_info *xi,
apr_text_header *phdr) {
const char *s;
if (*name->ns == '\0')
s = apr_psprintf(pool, "<%s/>" DEBUG_CR, name->name);
else {
const char *prefix = dav_xmlns_add_uri(xi, name->ns);
s = apr_psprintf(pool, "<%s:%s/>" DEBUG_CR, prefix, name->name);
}
apr_text_append(pool, phdr, s);
}
static void dav_insert_xmlns(apr_pool_t *p, const char *pre_prefix, long ns,
const char *ns_uri, apr_text_header *phdr) {
const char *s;
s = apr_psprintf(p, " xmlns:%s%ld=\"%s\"", pre_prefix, ns, ns_uri);
apr_text_append(p, phdr, s);
}
static dav_error *dav_really_open_db(dav_propdb *propdb, int ro) {
dav_error *err;
propdb->deferred = 0;
err = (*propdb->db_hooks->open)(propdb->p, propdb->resource, ro,
&propdb->db);
if (err != NULL) {
return dav_push_error(propdb->p, HTTP_INTERNAL_SERVER_ERROR,
DAV_ERR_PROP_OPENING,
"Could not open the property database.",
err);
}
return NULL;
}
DAV_DECLARE(dav_error *)dav_open_propdb(request_rec *r, dav_lockdb *lockdb,
const dav_resource *resource,
int ro,
apr_array_header_t * ns_xlate,
dav_propdb **p_propdb) {
dav_propdb *propdb = apr_pcalloc(r->pool, sizeof(*propdb));
*p_propdb = NULL;
#if DAV_DEBUG
if (resource->uri == NULL) {
return dav_new_error(r->pool, HTTP_INTERNAL_SERVER_ERROR, 0, 0,
"INTERNAL DESIGN ERROR: resource must define "
"its URI.");
}
#endif
propdb->r = r;
apr_pool_create(&propdb->p, r->pool);
propdb->resource = resource;
propdb->ns_xlate = ns_xlate;
propdb->db_hooks = DAV_GET_HOOKS_PROPDB(r);
propdb->lockdb = lockdb;
propdb->deferred = 1;
*p_propdb = propdb;
return NULL;
}
DAV_DECLARE(void) dav_close_propdb(dav_propdb *propdb) {
if (propdb->db != NULL) {
(*propdb->db_hooks->close)(propdb->db);
}
#if 0
apr_pool_destroy(propdb->p);
#endif
}
DAV_DECLARE(dav_get_props_result) dav_get_allprops(dav_propdb *propdb,
dav_prop_insert what) {
const dav_hooks_db *db_hooks = propdb->db_hooks;
apr_text_header hdr = { 0 };
apr_text_header hdr_ns = { 0 };
dav_get_props_result result = { 0 };
int found_contenttype = 0;
int found_contentlang = 0;
dav_prop_insert unused_inserted;
if (what != DAV_PROP_INSERT_SUPPORTED) {
if (propdb->deferred) {
(void) dav_really_open_db(propdb, 1 );
}
apr_text_append(propdb->p, &hdr,
"<D:propstat>" DEBUG_CR
"<D:prop>" DEBUG_CR);
if (propdb->db != NULL) {
dav_xmlns_info *xi = dav_xmlns_create(propdb->p);
dav_prop_name name;
dav_error *err;
(void) (*db_hooks->define_namespaces)(propdb->db, xi);
err = (*db_hooks->first_name)(propdb->db, &name);
while (!err && name.ns) {
if (*name.ns == 'D' && strcmp(name.ns, "DAV:") == 0
&& *name.name == 'g') {
if (strcmp(name.name, "getcontenttype") == 0) {
found_contenttype = 1;
} else if (strcmp(name.name, "getcontentlanguage") == 0) {
found_contentlang = 1;
}
}
if (what == DAV_PROP_INSERT_VALUE) {
int found;
if ((err = (*db_hooks->output_value)(propdb->db, &name,
xi, &hdr,
&found)) != NULL) {
goto next_key;
}
} else {
dav_output_prop_name(propdb->p, &name, xi, &hdr);
}
next_key:
err = (*db_hooks->next_name)(propdb->db, &name);
}
dav_xmlns_generate(xi, &hdr_ns);
}
dav_add_all_liveprop_xmlns(propdb->p, &hdr_ns);
}
dav_run_insert_all_liveprops(propdb->r, propdb->resource, what, &hdr);
(void)dav_insert_coreprop(propdb,
DAV_PROPID_CORE_supportedlock, "supportedlock",
what, &hdr, &unused_inserted);
(void)dav_insert_coreprop(propdb,
DAV_PROPID_CORE_lockdiscovery, "lockdiscovery",
what, &hdr, &unused_inserted);
if (!found_contenttype) {
(void)dav_insert_coreprop(propdb,
DAV_PROPID_CORE_getcontenttype,
"getcontenttype",
what, &hdr, &unused_inserted);
}
if (!found_contentlang) {
(void)dav_insert_coreprop(propdb,
DAV_PROPID_CORE_getcontentlanguage,
"getcontentlanguage",
what, &hdr, &unused_inserted);
}
if (what != DAV_PROP_INSERT_SUPPORTED) {
apr_text_append(propdb->p, &hdr,
"</D:prop>" DEBUG_CR
"<D:status>HTTP/1.1 200 OK</D:status>" DEBUG_CR
"</D:propstat>" DEBUG_CR);
}
result.propstats = hdr.first;
result.xmlns = hdr_ns.first;
return result;
}
DAV_DECLARE(dav_get_props_result) dav_get_props(dav_propdb *propdb,
apr_xml_doc *doc) {
const dav_hooks_db *db_hooks = propdb->db_hooks;
apr_xml_elem *elem = dav_find_child(doc->root, "prop");
apr_text_header hdr_good = { 0 };
apr_text_header hdr_bad = { 0 };
apr_text_header hdr_ns = { 0 };
int have_good = 0;
dav_get_props_result result = { 0 };
char *marks_liveprop;
dav_xmlns_info *xi;
int xi_filled = 0;
apr_text_append(propdb->p, &hdr_good,
"<D:propstat>" DEBUG_CR
"<D:prop>" DEBUG_CR);
marks_liveprop = apr_pcalloc(propdb->p, dav_get_liveprop_ns_count() + 1);
xi = dav_xmlns_create(propdb->p);
for (elem = elem->first_child; elem; elem = elem->next) {
dav_elem_private *priv;
dav_error *err;
dav_prop_insert inserted;
dav_prop_name name;
if (elem->priv == NULL) {
elem->priv = apr_pcalloc(propdb->p, sizeof(*priv));
}
priv = elem->priv;
if (priv->propid == 0)
dav_find_liveprop(propdb, elem);
if (priv->propid != DAV_PROPID_CORE_UNKNOWN) {
if ((err = dav_insert_liveprop(propdb, elem, DAV_PROP_INSERT_VALUE,
&hdr_good, &inserted)) != NULL) {
}
if (inserted == DAV_PROP_INSERT_VALUE) {
have_good = 1;
if (priv->provider != NULL) {
const char * const * scan_ns_uri;
for (scan_ns_uri = priv->provider->namespace_uris;
*scan_ns_uri != NULL;
++scan_ns_uri) {
long ns;
ns = dav_get_liveprop_ns_index(*scan_ns_uri);
if (marks_liveprop[ns])
continue;
marks_liveprop[ns] = 1;
dav_insert_xmlns(propdb->p, "lp", ns, *scan_ns_uri,
&hdr_ns);
}
}
continue;
} else if (inserted == DAV_PROP_INSERT_NOTDEF) {
}
#if DAV_DEBUG
else {
#if 0
return dav_new_error(propdb->p, HTTP_INTERNAL_SERVER_ERROR, 0,
0,
"INTERNAL DESIGN ERROR: insert_liveprop "
"did not insert what was asked for.");
#endif
}
#endif
}
if (propdb->deferred) {
(void) dav_really_open_db(propdb, 1 );
}
if (elem->ns == APR_XML_NS_NONE)
name.ns = "";
else
name.ns = APR_XML_GET_URI_ITEM(propdb->ns_xlate, elem->ns);
name.name = elem->name;
if (propdb->db != NULL) {
int found;
if ((err = (*db_hooks->output_value)(propdb->db, &name,
xi, &hdr_good,
&found)) != NULL) {
continue;
}
if (found) {
have_good = 1;
if (!xi_filled) {
(void) (*db_hooks->define_namespaces)(propdb->db, xi);
xi_filled = 1;
}
continue;
}
}
if (hdr_bad.first == NULL) {
apr_text_append(propdb->p, &hdr_bad,
"<D:propstat>" DEBUG_CR
"<D:prop>" DEBUG_CR);
}
dav_output_prop_name(propdb->p, &name, xi, &hdr_bad);
}
apr_text_append(propdb->p, &hdr_good,
"</D:prop>" DEBUG_CR
"<D:status>HTTP/1.1 200 OK</D:status>" DEBUG_CR
"</D:propstat>" DEBUG_CR);
result.propstats = hdr_good.first;
if (hdr_bad.first != NULL) {
apr_text_append(propdb->p, &hdr_bad,
"</D:prop>" DEBUG_CR
"<D:status>HTTP/1.1 404 Not Found</D:status>" DEBUG_CR
"</D:propstat>" DEBUG_CR);
if (!have_good) {
result.propstats = hdr_bad.first;
} else {
hdr_good.last->next = hdr_bad.first;
}
}
dav_xmlns_generate(xi, &hdr_ns);
result.xmlns = hdr_ns.first;
return result;
}
DAV_DECLARE(void) dav_get_liveprop_supported(dav_propdb *propdb,
const char *ns_uri,
const char *propname,
apr_text_header *body) {
int propid;
const dav_hooks_liveprop *hooks;
propid = dav_find_liveprop_provider(propdb, ns_uri, propname, &hooks);
if (propid != DAV_PROPID_CORE_UNKNOWN) {
if (hooks == NULL) {
dav_prop_insert unused_inserted;
dav_insert_coreprop(propdb, propid, propname,
DAV_PROP_INSERT_SUPPORTED, body, &unused_inserted);
} else {
(*hooks->insert_prop)(propdb->resource, propid,
DAV_PROP_INSERT_SUPPORTED, body);
}
}
}
DAV_DECLARE_NONSTD(void) dav_prop_validate(dav_prop_ctx *ctx) {
dav_propdb *propdb = ctx->propdb;
apr_xml_elem *prop = ctx->prop;
dav_elem_private *priv;
priv = ctx->prop->priv = apr_pcalloc(propdb->p, sizeof(*priv));
if (priv->propid == 0) {
dav_find_liveprop(propdb, prop);
ctx->is_liveprop = priv->provider != NULL;
}
if (!dav_rw_liveprop(propdb, priv)) {
ctx->err = dav_new_error(propdb->p, HTTP_CONFLICT,
DAV_ERR_PROP_READONLY, 0,
"Property is read-only.");
return;
}
if (ctx->is_liveprop) {
int defer_to_dead = 0;
ctx->err = (*priv->provider->patch_validate)(propdb->resource,
prop, ctx->operation,
&ctx->liveprop_ctx,
&defer_to_dead);
if (ctx->err != NULL || !defer_to_dead)
return;
ctx->is_liveprop = 0;
}
if (propdb->deferred
&& (ctx->err = dav_really_open_db(propdb, 0 )) != NULL) {
return;
}
if (propdb->db == NULL) {
ctx->err = dav_new_error(propdb->p, HTTP_INTERNAL_SERVER_ERROR,
DAV_ERR_PROP_NO_DATABASE, 0,
"Attempted to set/remove a property "
"without a valid, open, read/write "
"property database.");
return;
}
if (ctx->operation == DAV_PROP_OP_SET) {
(void) (*propdb->db_hooks->map_namespaces)(propdb->db,
propdb->ns_xlate,
&propdb->mapping);
} else if (ctx->operation == DAV_PROP_OP_DELETE) {
}
}
DAV_DECLARE_NONSTD(void) dav_prop_exec(dav_prop_ctx *ctx) {
dav_propdb *propdb = ctx->propdb;
dav_error *err = NULL;
dav_elem_private *priv = ctx->prop->priv;
ctx->rollback = apr_pcalloc(propdb->p, sizeof(*ctx->rollback));
if (ctx->is_liveprop) {
err = (*priv->provider->patch_exec)(propdb->resource,
ctx->prop, ctx->operation,
ctx->liveprop_ctx,
&ctx->rollback->liveprop);
} else {
dav_prop_name name;
if (ctx->prop->ns == APR_XML_NS_NONE)
name.ns = "";
else
name.ns = APR_XML_GET_URI_ITEM(propdb->ns_xlate, ctx->prop->ns);
name.name = ctx->prop->name;
if ((err = (*propdb->db_hooks
->get_rollback)(propdb->db, &name,
&ctx->rollback->deadprop)) != NULL)
goto error;
if (ctx->operation == DAV_PROP_OP_SET) {
err = (*propdb->db_hooks->store)(propdb->db, &name, ctx->prop,
propdb->mapping);
} else if (ctx->operation == DAV_PROP_OP_DELETE) {
(void) (*propdb->db_hooks->remove)(propdb->db, &name);
}
}
error:
if (err != NULL) {
ctx->err = dav_push_error(propdb->p, HTTP_INTERNAL_SERVER_ERROR,
DAV_ERR_PROP_EXEC,
"Could not execute PROPPATCH.", err);
}
}
DAV_DECLARE_NONSTD(void) dav_prop_commit(dav_prop_ctx *ctx) {
dav_elem_private *priv = ctx->prop->priv;
if (ctx->is_liveprop) {
(*priv->provider->patch_commit)(ctx->propdb->resource,
ctx->operation,
ctx->liveprop_ctx,
ctx->rollback->liveprop);
}
}
DAV_DECLARE_NONSTD(void) dav_prop_rollback(dav_prop_ctx *ctx) {
dav_error *err = NULL;
dav_elem_private *priv = ctx->prop->priv;
if (ctx->rollback == NULL)
return;
if (ctx->is_liveprop) {
err = (*priv->provider->patch_rollback)(ctx->propdb->resource,
ctx->operation,
ctx->liveprop_ctx,
ctx->rollback->liveprop);
} else {
err = (*ctx->propdb->db_hooks
->apply_rollback)(ctx->propdb->db, ctx->rollback->deadprop);
}
if (err != NULL) {
if (ctx->err == NULL)
ctx->err = err;
else {
dav_error *scan = err;
while (scan->prev != NULL)
scan = scan->prev;
scan->prev = ctx->err;
ctx->err = err;
}
}
}