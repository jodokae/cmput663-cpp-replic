#include <apr_hash.h>
#include <httpd.h>
#include <mod_dav.h>
#include "svn_xml.h"
#include "svn_pools.h"
#include "svn_dav.h"
#include "svn_base64.h"
#include "svn_props.h"
#include "dav_svn.h"
struct dav_db {
const dav_resource *resource;
apr_pool_t *p;
apr_hash_t *props;
apr_hash_index_t *hi;
svn_stringbuf_t *work;
svn_repos_authz_func_t authz_read_func;
void *authz_read_baton;
};
struct dav_deadprop_rollback {
dav_prop_name name;
svn_string_t value;
};
static const char *
get_repos_path(struct dav_resource_private *info) {
return info->repos_path;
}
static void
get_repos_propname(dav_db *db,
const dav_prop_name *name,
const char **repos_propname) {
if (strcmp(name->ns, SVN_DAV_PROP_NS_SVN) == 0) {
svn_stringbuf_set(db->work, SVN_PROP_PREFIX);
svn_stringbuf_appendcstr(db->work, name->name);
*repos_propname = db->work->data;
} else if (strcmp(name->ns, SVN_DAV_PROP_NS_CUSTOM) == 0) {
*repos_propname = name->name;
} else {
*repos_propname = NULL;
}
}
static dav_error *
get_value(dav_db *db, const dav_prop_name *name, svn_string_t **pvalue) {
const char *propname;
svn_error_t *serr;
get_repos_propname(db, name, &propname);
if (propname == NULL) {
*pvalue = NULL;
return NULL;
}
if (db->resource->baselined)
if (db->resource->type == DAV_RESOURCE_TYPE_WORKING)
serr = svn_fs_txn_prop(pvalue, db->resource->info->root.txn,
propname, db->p);
else
serr = svn_repos_fs_revision_prop(pvalue,
db->resource->info-> repos->repos,
db->resource->info->root.rev, propname,
db->authz_read_func,
db->authz_read_baton, db->p);
else
serr = svn_fs_node_prop(pvalue, db->resource->info->root.root,
get_repos_path(db->resource->info),
propname, db->p);
if (serr != NULL)
return dav_svn__convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
"could not fetch a property",
db->resource->pool);
return NULL;
}
static dav_error *
save_value(dav_db *db, const dav_prop_name *name, const svn_string_t *value) {
const char *propname;
svn_error_t *serr;
get_repos_propname(db, name, &propname);
if (propname == NULL) {
if (db->resource->info->repos->autoversioning)
propname = name->name;
else
return dav_new_error(db->p, HTTP_CONFLICT, 0,
"Properties may only be defined in the "
SVN_DAV_PROP_NS_SVN " and " SVN_DAV_PROP_NS_CUSTOM
" namespaces.");
}
if (db->resource->baselined)
if (db->resource->working)
serr = svn_repos_fs_change_txn_prop(db->resource->info->root.txn,
propname, value, db->resource->pool);
else {
serr = svn_repos_fs_change_rev_prop3
(db->resource->info->repos->repos,
db->resource->info->root.rev,
db->resource->info->repos->username,
propname, value, TRUE, TRUE,
db->authz_read_func,
db->authz_read_baton,
db->resource->pool);
dav_svn__operational_log(db->resource->info,
apr_psprintf(db->resource->pool,
"change-rev-prop r%ld %s",
db->resource->info->root.rev,
svn_path_uri_encode(propname,
db->resource->pool)));
}
else
serr = svn_repos_fs_change_node_prop(db->resource->info->root.root,
get_repos_path(db->resource->info),
propname, value, db->resource->pool);
if (serr != NULL)
return dav_svn__convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
NULL,
db->resource->pool);
db->props = NULL;
return NULL;
}
static dav_error *
db_open(apr_pool_t *p,
const dav_resource *resource,
int ro,
dav_db **pdb) {
dav_db *db;
dav_svn__authz_read_baton *arb;
if (resource->type == DAV_RESOURCE_TYPE_HISTORY
|| resource->type == DAV_RESOURCE_TYPE_ACTIVITY
|| resource->type == DAV_RESOURCE_TYPE_PRIVATE) {
*pdb = NULL;
return NULL;
}
if (!ro && resource->type != DAV_RESOURCE_TYPE_WORKING) {
if (! (resource->baselined
&& resource->type == DAV_RESOURCE_TYPE_VERSION))
return dav_new_error(p, HTTP_CONFLICT, 0,
"Properties may only be changed on working "
"resources.");
}
db = apr_pcalloc(p, sizeof(*db));
db->resource = resource;
db->p = svn_pool_create(p);
db->work = svn_stringbuf_ncreate("", 0, db->p);
arb = apr_pcalloc(p, sizeof(*arb));
arb->r = resource->info->r;
arb->repos = resource->info->repos;
db->authz_read_baton = arb;
db->authz_read_func = dav_svn__authz_read_func(arb);
*pdb = db;
return NULL;
}
static void
db_close(dav_db *db) {
svn_pool_destroy(db->p);
}
static dav_error *
db_define_namespaces(dav_db *db, dav_xmlns_info *xi) {
dav_xmlns_add(xi, "S", SVN_DAV_PROP_NS_SVN);
dav_xmlns_add(xi, "C", SVN_DAV_PROP_NS_CUSTOM);
dav_xmlns_add(xi, "V", SVN_DAV_PROP_NS_DAV);
return NULL;
}
static dav_error *
db_output_value(dav_db *db,
const dav_prop_name *name,
dav_xmlns_info *xi,
apr_text_header *phdr,
int *found) {
const char *prefix;
const char *s;
svn_string_t *propval;
dav_error *err;
apr_pool_t *pool = db->resource->pool;
if ((err = get_value(db, name, &propval)) != NULL)
return err;
*found = (propval != NULL);
if (propval == NULL)
return NULL;
if (strcmp(name->ns, SVN_DAV_PROP_NS_CUSTOM) == 0)
prefix = "C:";
else
prefix = "S:";
if (propval->len == 0) {
s = apr_psprintf(pool, "<%s%s/>" DEBUG_CR, prefix, name->name);
apr_text_append(pool, phdr, s);
} else {
const char *xml_safe;
const char *encoding = "";
if (! svn_xml_is_xml_safe(propval->data, propval->len)) {
const svn_string_t *enc_propval
= svn_base64_encode_string(propval, pool);
xml_safe = enc_propval->data;
encoding = apr_pstrcat(pool, " V:encoding=\"base64\"", NULL);
} else {
svn_stringbuf_t *xmlval = NULL;
svn_xml_escape_cdata_string(&xmlval, propval, pool);
xml_safe = xmlval->data;
}
s = apr_psprintf(pool, "<%s%s%s>", prefix, name->name, encoding);
apr_text_append(pool, phdr, s);
apr_text_append(pool, phdr, xml_safe);
s = apr_psprintf(pool, "</%s%s>" DEBUG_CR, prefix, name->name);
apr_text_append(pool, phdr, s);
}
return NULL;
}
static dav_error *
db_map_namespaces(dav_db *db,
const apr_array_header_t *namespaces,
dav_namespace_map **mapping) {
return NULL;
}
static dav_error *
db_store(dav_db *db,
const dav_prop_name *name,
const apr_xml_elem *elem,
dav_namespace_map *mapping) {
const svn_string_t *propval;
apr_pool_t *pool = db->p;
apr_xml_attr *attr = elem->attr;
propval = svn_string_create
(dav_xml_get_cdata(elem, pool, 0 ), pool);
while (attr) {
if (strcmp(attr->name, "encoding") == 0) {
const char *enc_type = attr->value;
if (enc_type && (strcmp(enc_type, "base64") == 0))
propval = svn_base64_decode_string(propval, pool);
else
return dav_new_error(pool, HTTP_INTERNAL_SERVER_ERROR, 0,
"Unknown property encoding");
break;
}
attr = attr->next;
}
return save_value(db, name, propval);
}
static dav_error *
db_remove(dav_db *db, const dav_prop_name *name) {
svn_error_t *serr;
const char *propname;
get_repos_propname(db, name, &propname);
if (propname == NULL)
return NULL;
if (db->resource->baselined)
if (db->resource->working)
serr = svn_repos_fs_change_txn_prop(db->resource->info->root.txn,
propname, NULL, db->resource->pool);
else
serr = svn_repos_fs_change_rev_prop3(db->resource->info->repos->repos,
db->resource->info->root.rev,
db->resource->info->repos->username,
propname, NULL, TRUE, TRUE,
db->authz_read_func,
db->authz_read_baton,
db->resource->pool);
else
serr = svn_repos_fs_change_node_prop(db->resource->info->root.root,
get_repos_path(db->resource->info),
propname, NULL, db->resource->pool);
if (serr != NULL)
return dav_svn__convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
"could not remove a property",
db->resource->pool);
db->props = NULL;
return NULL;
}
static int
db_exists(dav_db *db, const dav_prop_name *name) {
const char *propname;
svn_string_t *propval;
svn_error_t *serr;
int retval;
get_repos_propname(db, name, &propname);
if (propname == NULL)
return 0;
if (db->resource->baselined)
if (db->resource->type == DAV_RESOURCE_TYPE_WORKING)
serr = svn_fs_txn_prop(&propval, db->resource->info->root.txn,
propname, db->p);
else
serr = svn_repos_fs_revision_prop(&propval,
db->resource->info->repos->repos,
db->resource->info->root.rev,
propname,
db->authz_read_func,
db->authz_read_baton, db->p);
else
serr = svn_fs_node_prop(&propval, db->resource->info->root.root,
get_repos_path(db->resource->info),
propname, db->p);
retval = (serr == NULL && propval != NULL);
svn_error_clear(serr);
return retval;
}
static void get_name(dav_db *db, dav_prop_name *pname) {
if (db->hi == NULL) {
pname->ns = pname->name = NULL;
} else {
const void *name;
apr_hash_this(db->hi, &name, NULL, NULL);
#define PREFIX_LEN (sizeof(SVN_PROP_PREFIX) - 1)
if (strncmp(name, SVN_PROP_PREFIX, PREFIX_LEN) == 0)
#undef PREFIX_LEN
{
pname->ns = SVN_DAV_PROP_NS_SVN;
pname->name = (const char *)name + 4;
} else {
pname->ns = SVN_DAV_PROP_NS_CUSTOM;
pname->name = name;
}
}
}
static dav_error *
db_first_name(dav_db *db, dav_prop_name *pname) {
char *action = NULL;
if (db->props == NULL) {
svn_error_t *serr;
if (db->resource->baselined) {
if (db->resource->type == DAV_RESOURCE_TYPE_WORKING)
serr = svn_fs_txn_proplist(&db->props,
db->resource->info->root.txn,
db->p);
else {
action = apr_psprintf(db->resource->pool, "rev-proplist r%ld",
db->resource->info->root.rev);
serr = svn_repos_fs_revision_proplist
(&db->props,
db->resource->info->repos->repos,
db->resource->info->root.rev,
db->authz_read_func,
db->authz_read_baton,
db->p);
}
} else {
svn_node_kind_t kind;
serr = svn_fs_node_proplist(&db->props,
db->resource->info->root.root,
get_repos_path(db->resource->info),
db->p);
if (! serr)
serr = svn_fs_check_path(&kind, db->resource->info->root.root,
get_repos_path(db->resource->info),
db->p);
if (! serr)
action = apr_psprintf(db->resource->pool, "get-%s %s r%ld props",
(kind == svn_node_dir ? "dir" : "file"),
svn_path_uri_encode(db->resource->info->repos_path,
db->resource->pool),
db->resource->info->root.rev);
}
if (serr != NULL)
return dav_svn__convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
"could not begin sequencing through "
"properties",
db->resource->pool);
}
db->hi = apr_hash_first(db->p, db->props);
get_name(db, pname);
if (action != NULL)
dav_svn__operational_log(db->resource->info, action);
return NULL;
}
static dav_error *
db_next_name(dav_db *db, dav_prop_name *pname) {
if (db->hi != NULL)
db->hi = apr_hash_next(db->hi);
get_name(db, pname);
return NULL;
}
static dav_error *
db_get_rollback(dav_db *db,
const dav_prop_name *name,
dav_deadprop_rollback **prollback) {
dav_error *err;
dav_deadprop_rollback *ddp;
svn_string_t *propval;
if ((err = get_value(db, name, &propval)) != NULL)
return err;
ddp = apr_palloc(db->p, sizeof(*ddp));
ddp->name = *name;
ddp->value.data = propval ? propval->data : NULL;
ddp->value.len = propval ? propval->len : 0;
*prollback = ddp;
return NULL;
}
static dav_error *
db_apply_rollback(dav_db *db, dav_deadprop_rollback *rollback) {
if (rollback->value.data == NULL) {
return db_remove(db, &rollback->name);
}
return save_value(db, &rollback->name, &rollback->value);
}
const dav_hooks_propdb dav_svn__hooks_propdb = {
db_open,
db_close,
db_define_namespaces,
db_output_value,
db_map_namespaces,
db_store,
db_remove,
db_exists,
db_first_name,
db_next_name,
db_get_rollback,
db_apply_rollback,
};