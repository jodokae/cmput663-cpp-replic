#include <apr_pools.h>
#include <apr_tables.h>
#include <apr_strings.h>
#define APR_WANT_STRFUNC
#include <apr_want.h>
#include "svn_error.h"
#include "svn_path.h"
#include "svn_dav.h"
#include "svn_base64.h"
#include "svn_xml.h"
#include "svn_time.h"
#include "svn_pools.h"
#include "svn_props.h"
#include "../libsvn_ra/ra_loader.h"
#include "private/svn_dav_protocol.h"
#include "svn_private_config.h"
#include "ra_neon.h"
const ne_propname svn_ra_neon__vcc_prop = {
"DAV:", "version-controlled-configuration"
};
const ne_propname svn_ra_neon__checked_in_prop = {
"DAV:", "checked-in"
};
static const ne_propname starting_props[] = {
{ "DAV:", "version-controlled-configuration" },
{ "DAV:", "resourcetype" },
{ SVN_DAV_PROP_NS_DAV, "baseline-relative-path" },
{ SVN_DAV_PROP_NS_DAV, "repository-uuid"},
{ NULL }
};
static const ne_propname baseline_props[] = {
{ "DAV:", "baseline-collection" },
{ "DAV:", SVN_DAV__VERSION_NAME },
{ NULL }
};
typedef struct {
svn_ra_neon__xml_elmid id;
const char *name;
int is_property;
} elem_defn;
static const elem_defn elem_definitions[] = {
{ ELEM_multistatus, "DAV:multistatus", 0 },
{ ELEM_response, "DAV:response", 0 },
{ ELEM_href, "DAV:href", SVN_RA_NEON__XML_CDATA },
{ ELEM_propstat, "DAV:propstat", 0 },
{ ELEM_prop, "DAV:prop", 0 },
{ ELEM_status, "DAV:status", SVN_RA_NEON__XML_CDATA },
{ ELEM_baseline, "DAV:baseline", SVN_RA_NEON__XML_CDATA },
{ ELEM_collection, "DAV:collection", SVN_RA_NEON__XML_CDATA },
{ ELEM_resourcetype, "DAV:resourcetype", 0 },
{ ELEM_baseline_coll, SVN_RA_NEON__PROP_BASELINE_COLLECTION, 0 },
{ ELEM_checked_in, SVN_RA_NEON__PROP_CHECKED_IN, 0 },
{ ELEM_vcc, SVN_RA_NEON__PROP_VCC, 0 },
{ ELEM_version_name, SVN_RA_NEON__PROP_VERSION_NAME, 1 },
{ ELEM_get_content_length, SVN_RA_NEON__PROP_GETCONTENTLENGTH, 1 },
{ ELEM_creationdate, SVN_RA_NEON__PROP_CREATIONDATE, 1 },
{ ELEM_creator_displayname, SVN_RA_NEON__PROP_CREATOR_DISPLAYNAME, 1 },
{ ELEM_baseline_relpath, SVN_RA_NEON__PROP_BASELINE_RELPATH, 1 },
{ ELEM_md5_checksum, SVN_RA_NEON__PROP_MD5_CHECKSUM, 1 },
{ ELEM_repository_uuid, SVN_RA_NEON__PROP_REPOSITORY_UUID, 1 },
{ ELEM_deadprop_count, SVN_RA_NEON__PROP_DEADPROP_COUNT, 1 },
{ 0 }
};
static const svn_ra_neon__xml_elm_t propfind_elements[] = {
{ "DAV:", "multistatus", ELEM_multistatus, 0 },
{ "DAV:", "response", ELEM_response, 0 },
{ "DAV:", "href", ELEM_href, SVN_RA_NEON__XML_CDATA },
{ "DAV:", "propstat", ELEM_propstat, 0 },
{ "DAV:", "prop", ELEM_prop, 0 },
{ "DAV:", "status", ELEM_status, SVN_RA_NEON__XML_CDATA },
{ "DAV:", "baseline", ELEM_baseline, SVN_RA_NEON__XML_CDATA },
{ "DAV:", "baseline-collection", ELEM_baseline_coll, SVN_RA_NEON__XML_CDATA },
{ "DAV:", "checked-in", ELEM_checked_in, 0 },
{ "DAV:", "collection", ELEM_collection, SVN_RA_NEON__XML_CDATA },
{ "DAV:", "resourcetype", ELEM_resourcetype, 0 },
{ "DAV:", "version-controlled-configuration", ELEM_vcc, 0 },
{ "DAV:", SVN_DAV__VERSION_NAME, ELEM_version_name, SVN_RA_NEON__XML_CDATA },
{
"DAV:", "getcontentlength", ELEM_get_content_length,
SVN_RA_NEON__XML_CDATA
},
{ "DAV:", SVN_DAV__CREATIONDATE, ELEM_creationdate, SVN_RA_NEON__XML_CDATA },
{
"DAV:", "creator-displayname", ELEM_creator_displayname,
SVN_RA_NEON__XML_CDATA
},
{
SVN_DAV_PROP_NS_DAV, "baseline-relative-path", ELEM_baseline_relpath,
SVN_RA_NEON__XML_CDATA
},
{
SVN_DAV_PROP_NS_DAV, "md5-checksum", ELEM_md5_checksum,
SVN_RA_NEON__XML_CDATA
},
{
SVN_DAV_PROP_NS_DAV, "repository-uuid", ELEM_repository_uuid,
SVN_RA_NEON__XML_CDATA
},
{
SVN_DAV_PROP_NS_DAV, "deadprop-count", ELEM_deadprop_count,
SVN_RA_NEON__XML_CDATA
},
{ "", "", ELEM_unknown, SVN_RA_NEON__XML_COLLECT },
{ NULL }
};
typedef struct propfind_ctx_t {
svn_stringbuf_t *cdata;
apr_hash_t *props;
svn_ra_neon__resource_t *rsrc;
const char *encoding;
int status;
apr_hash_t *propbuffer;
svn_ra_neon__xml_elmid last_open_id;
ne_xml_parser *parser;
apr_pool_t *pool;
} propfind_ctx_t;
static const elem_defn *defn_from_id(svn_ra_neon__xml_elmid id) {
const elem_defn *defn;
for (defn = elem_definitions; defn->name != NULL; ++defn) {
if (id == defn->id)
return defn;
}
return NULL;
}
static svn_error_t *
assign_rsrc_url(svn_ra_neon__resource_t *rsrc,
const char *url, apr_pool_t *pool) {
char *url_path;
apr_size_t len;
ne_uri parsed_url;
if (ne_uri_parse(url, &parsed_url) != 0) {
ne_uri_free(&parsed_url);
return svn_error_createf(SVN_ERR_RA_DAV_MALFORMED_DATA, NULL,
_("Unable to parse URL '%s'"), url);
}
url_path = apr_pstrdup(pool, parsed_url.path);
ne_uri_free(&parsed_url);
len = strlen(url_path);
if (len > 1 && url_path[len - 1] == '/')
url_path[len - 1] = '\0';
rsrc->url = url_path;
return SVN_NO_ERROR;
}
static int validate_element(svn_ra_neon__xml_elmid parent,
svn_ra_neon__xml_elmid child) {
switch (parent) {
case ELEM_root:
if (child == ELEM_multistatus)
return child;
else
return SVN_RA_NEON__XML_INVALID;
case ELEM_multistatus:
if (child == ELEM_response)
return child;
else
return SVN_RA_NEON__XML_DECLINE;
case ELEM_response:
if ((child == ELEM_href) || (child == ELEM_propstat))
return child;
else
return SVN_RA_NEON__XML_DECLINE;
case ELEM_propstat:
if ((child == ELEM_prop) || (child == ELEM_status))
return child;
else
return SVN_RA_NEON__XML_DECLINE;
case ELEM_prop:
return child;
case ELEM_baseline_coll:
case ELEM_checked_in:
case ELEM_vcc:
if (child == ELEM_href)
return child;
else
return SVN_RA_NEON__XML_DECLINE;
case ELEM_resourcetype:
if ((child == ELEM_collection) || (child == ELEM_baseline))
return child;
else
return SVN_RA_NEON__XML_DECLINE;
default:
return SVN_RA_NEON__XML_DECLINE;
}
}
static svn_error_t *
start_element(int *elem, void *baton, int parent,
const char *nspace, const char *name, const char **atts) {
propfind_ctx_t *pc = baton;
const svn_ra_neon__xml_elm_t *elm
= svn_ra_neon__lookup_xml_elem(propfind_elements, nspace, name);
*elem = elm ? validate_element(parent, elm->id) : SVN_RA_NEON__XML_DECLINE;
if (*elem < 1)
return SVN_NO_ERROR;
svn_stringbuf_setempty(pc->cdata);
*elem = elm ? elm->id : ELEM_unknown;
switch (*elem) {
case ELEM_response:
if (pc->rsrc)
return svn_error_create(SVN_ERR_XML_MALFORMED, NULL, NULL);
pc->rsrc = apr_pcalloc(pc->pool, sizeof(*(pc->rsrc)));
pc->rsrc->pool = pc->pool;
pc->rsrc->propset = apr_hash_make(pc->pool);
pc->status = 0;
break;
case ELEM_propstat:
pc->status = 0;
break;
case ELEM_href:
pc->rsrc->href_parent = pc->last_open_id;
break;
case ELEM_collection:
pc->rsrc->is_collection = 1;
break;
case ELEM_unknown:
pc->encoding = ne_xml_get_attr(pc->parser, atts, SVN_DAV_PROP_NS_DAV,
"encoding");
if (pc->encoding)
pc->encoding = apr_pstrdup(pc->pool, pc->encoding);
break;
default:
break;
}
pc->last_open_id = *elem;
return SVN_NO_ERROR;
}
static svn_error_t * end_element(void *baton, int state,
const char *nspace, const char *name) {
propfind_ctx_t *pc = baton;
svn_ra_neon__resource_t *rsrc = pc->rsrc;
const svn_string_t *value = NULL;
const elem_defn *parent_defn;
const elem_defn *defn;
ne_status status;
const char *cdata = pc->cdata->data;
switch (state) {
case ELEM_response:
if (!pc->rsrc->url)
return svn_error_create(SVN_ERR_XML_MALFORMED, NULL, NULL);
apr_hash_set(pc->props, pc->rsrc->url, APR_HASH_KEY_STRING, pc->rsrc);
pc->rsrc = NULL;
return SVN_NO_ERROR;
case ELEM_propstat:
if (pc->status) {
apr_hash_index_t *hi = apr_hash_first(pc->pool, pc->propbuffer);
for (; hi; hi = apr_hash_next(hi)) {
const void *key;
apr_ssize_t klen;
void *val;
apr_hash_this(hi, &key, &klen, &val);
if (pc->status == 200)
apr_hash_set(rsrc->propset, key, klen, val);
apr_hash_set(pc->propbuffer, key, klen, NULL);
}
} else if (! pc->status) {
return svn_error_create(SVN_ERR_XML_MALFORMED, NULL, NULL);
}
return SVN_NO_ERROR;
case ELEM_status:
if (ne_parse_statusline(cdata, &status))
return svn_error_create(SVN_ERR_XML_MALFORMED, NULL, NULL);
free(status.reason_phrase);
pc->status = status.code;
return SVN_NO_ERROR;
case ELEM_href:
if (rsrc->href_parent == ELEM_response)
return assign_rsrc_url(pc->rsrc, cdata, pc->pool);
parent_defn = defn_from_id(rsrc->href_parent);
if (!parent_defn)
return SVN_NO_ERROR;
name = parent_defn->name;
value = svn_string_create(cdata, pc->pool);
break;
default:
if (state == ELEM_unknown) {
name = apr_pstrcat(pc->pool, nspace, name, NULL);
} else {
defn = defn_from_id(state);
if (! (defn && defn->is_property))
return SVN_NO_ERROR;
name = defn->name;
}
if (pc->encoding == NULL) {
value = svn_string_create(cdata, pc->pool);
break;
}
if (strcmp(pc->encoding, "base64") != 0)
return svn_error_create(SVN_ERR_XML_MALFORMED, NULL, NULL);
{
svn_string_t in;
in.data = cdata;
in.len = strlen(cdata);
value = svn_base64_decode_string(&in, pc->pool);
}
pc->encoding = NULL;
}
apr_hash_set(pc->propbuffer, name, APR_HASH_KEY_STRING, value);
return SVN_NO_ERROR;
}
static void set_parser(ne_xml_parser *parser,
void *baton) {
propfind_ctx_t *pc = baton;
pc->parser = parser;
}
svn_error_t * svn_ra_neon__get_props(apr_hash_t **results,
svn_ra_neon__session_t *sess,
const char *url,
int depth,
const char *label,
const ne_propname *which_props,
apr_pool_t *pool) {
propfind_ctx_t pc;
svn_stringbuf_t *body;
apr_hash_t *extra_headers = apr_hash_make(pool);
svn_ra_neon__add_depth_header(extra_headers, depth);
if (label != NULL)
apr_hash_set(extra_headers, "Label", 5, label);
body = svn_stringbuf_create
("<?xml version=\"1.0\" encoding=\"utf-8\"?>" DEBUG_CR
"<propfind xmlns=\"DAV:\">" DEBUG_CR, pool);
if (which_props) {
int n;
apr_pool_t *iterpool = svn_pool_create(pool);
svn_stringbuf_appendcstr(body, "<prop>" DEBUG_CR);
for (n = 0; which_props[n].name != NULL; n++) {
svn_pool_clear(iterpool);
svn_stringbuf_appendcstr
(body, apr_pstrcat(iterpool, "<", which_props[n].name, " xmlns=\"",
which_props[n].nspace, "\"/>" DEBUG_CR, NULL));
}
svn_stringbuf_appendcstr(body, "</prop></propfind>" DEBUG_CR);
svn_pool_destroy(iterpool);
} else {
svn_stringbuf_appendcstr(body, "<allprop/></propfind>" DEBUG_CR);
}
memset(&pc, 0, sizeof(pc));
pc.pool = pool;
pc.propbuffer = apr_hash_make(pool);
pc.props = apr_hash_make(pool);
pc.cdata = svn_stringbuf_create("", pool);
SVN_ERR(svn_ra_neon__parsed_request(sess, "PROPFIND", url,
body->data, 0,
set_parser,
start_element,
svn_ra_neon__xml_collect_cdata,
end_element,
&pc, extra_headers, NULL, FALSE, pool));
*results = pc.props;
return SVN_NO_ERROR;
}
svn_error_t * svn_ra_neon__get_props_resource(svn_ra_neon__resource_t **rsrc,
svn_ra_neon__session_t *sess,
const char *url,
const char *label,
const ne_propname *which_props,
apr_pool_t *pool) {
apr_hash_t *props;
char * url_path = apr_pstrdup(pool, url);
int len = strlen(url);
if (len > 1 && url[len - 1] == '/')
url_path[len - 1] = '\0';
SVN_ERR(svn_ra_neon__get_props(&props, sess, url_path, SVN_RA_NEON__DEPTH_ZERO,
label, which_props, pool));
if (1 || label != NULL) {
apr_hash_index_t *hi = apr_hash_first(pool, props);
if (hi) {
void *ent;
apr_hash_this(hi, NULL, NULL, &ent);
*rsrc = ent;
} else
*rsrc = NULL;
} else {
*rsrc = apr_hash_get(props, url_path, APR_HASH_KEY_STRING);
}
if (*rsrc == NULL) {
return svn_error_createf(APR_EGENERAL, NULL,
_("Failed to find label '%s' for URL '%s'"),
label ? label : "NULL", url_path);
}
return SVN_NO_ERROR;
}
svn_error_t * svn_ra_neon__get_one_prop(const svn_string_t **propval,
svn_ra_neon__session_t *sess,
const char *url,
const char *label,
const ne_propname *propname,
apr_pool_t *pool) {
svn_ra_neon__resource_t *rsrc;
ne_propname props[2] = { { 0 } };
const char *name;
const svn_string_t *value;
props[0] = *propname;
SVN_ERR(svn_ra_neon__get_props_resource(&rsrc, sess, url, label, props,
pool));
name = apr_pstrcat(pool, propname->nspace, propname->name, NULL);
value = apr_hash_get(rsrc->propset, name, APR_HASH_KEY_STRING);
if (value == NULL) {
return svn_error_createf(SVN_ERR_RA_DAV_PROPS_NOT_FOUND, NULL,
_("'%s' was not present on the resource"),
name);
}
*propval = value;
return SVN_NO_ERROR;
}
svn_error_t * svn_ra_neon__get_starting_props(svn_ra_neon__resource_t **rsrc,
svn_ra_neon__session_t *sess,
const char *url,
const char *label,
apr_pool_t *pool) {
svn_string_t *propval;
SVN_ERR(svn_ra_neon__get_props_resource(rsrc, sess, url, label,
starting_props, pool));
if (! sess->vcc) {
propval = apr_hash_get((*rsrc)->propset,
SVN_RA_NEON__PROP_VCC,
APR_HASH_KEY_STRING);
if (propval)
sess->vcc = apr_pstrdup(sess->pool, propval->data);
}
if (! sess->uuid) {
propval = apr_hash_get((*rsrc)->propset,
SVN_RA_NEON__PROP_REPOSITORY_UUID,
APR_HASH_KEY_STRING);
if (propval)
sess->uuid = apr_pstrdup(sess->pool, propval->data);
}
return SVN_NO_ERROR;
}
svn_error_t *
svn_ra_neon__search_for_starting_props(svn_ra_neon__resource_t **rsrc,
const char **missing_path,
svn_ra_neon__session_t *sess,
const char *url,
apr_pool_t *pool) {
svn_error_t *err = SVN_NO_ERROR;
apr_size_t len;
svn_stringbuf_t *path_s;
ne_uri parsed_url;
svn_stringbuf_t *lopped_path =
svn_stringbuf_create(url, pool);
apr_pool_t *iterpool = svn_pool_create(pool);
ne_uri_parse(url, &parsed_url);
if (parsed_url.path == NULL) {
ne_uri_free(&parsed_url);
return svn_error_createf(SVN_ERR_RA_ILLEGAL_URL, NULL,
_("Neon was unable to parse URL '%s'"), url);
}
svn_stringbuf_setempty(lopped_path);
path_s = svn_stringbuf_create(parsed_url.path, pool);
ne_uri_free(&parsed_url);
while (! svn_path_is_empty(path_s->data)) {
svn_pool_clear(iterpool);
err = svn_ra_neon__get_starting_props(rsrc, sess, path_s->data,
NULL, iterpool);
if (! err)
break;
if (err->apr_err != SVN_ERR_RA_DAV_PATH_NOT_FOUND)
return err;
svn_stringbuf_set(lopped_path,
svn_path_join(svn_path_basename(path_s->data, iterpool),
lopped_path->data, iterpool));
len = path_s->len;
svn_path_remove_component(path_s);
if (path_s->len == len)
return svn_error_quick_wrap
(err, _("The path was not part of a repository"));
svn_error_clear(err);
}
if (svn_path_is_empty(path_s->data))
return svn_error_createf(SVN_ERR_RA_ILLEGAL_URL, NULL,
_("No part of path '%s' was found in "
"repository HEAD"), parsed_url.path);
{
apr_hash_index_t *hi;
svn_ra_neon__resource_t *tmp = apr_pcalloc(pool, sizeof(*tmp));
tmp->url = apr_pstrdup(pool, (*rsrc)->url);
tmp->is_collection = (*rsrc)->is_collection;
tmp->pool = pool;
tmp->propset = apr_hash_make(pool);
for (hi = apr_hash_first(iterpool, (*rsrc)->propset);
hi; hi = apr_hash_next(hi)) {
const void *key;
void *val;
apr_hash_this(hi, &key, NULL, &val);
apr_hash_set(tmp->propset, apr_pstrdup(pool, key), APR_HASH_KEY_STRING,
svn_string_dup(val, pool));
}
*rsrc = tmp;
}
*missing_path = lopped_path->data;
svn_pool_destroy(iterpool);
return SVN_NO_ERROR;
}
svn_error_t *svn_ra_neon__get_vcc(const char **vcc,
svn_ra_neon__session_t *sess,
const char *url,
apr_pool_t *pool) {
svn_ra_neon__resource_t *rsrc;
const char *lopped_path;
if (sess->vcc) {
*vcc = sess->vcc;
return SVN_NO_ERROR;
}
SVN_ERR(svn_ra_neon__search_for_starting_props(&rsrc, &lopped_path,
sess, url, pool));
if (! sess->vcc) {
return svn_error_create(APR_EGENERAL, NULL,
_("The VCC property was not found on the "
"resource"));
}
*vcc = sess->vcc;
return SVN_NO_ERROR;
}
svn_error_t *svn_ra_neon__get_baseline_props(svn_string_t *bc_relative,
svn_ra_neon__resource_t **bln_rsrc,
svn_ra_neon__session_t *sess,
const char *url,
svn_revnum_t revision,
const ne_propname *which_props,
apr_pool_t *pool) {
svn_ra_neon__resource_t *rsrc;
const char *vcc;
const svn_string_t *relative_path;
const char *my_bc_relative;
const char *lopped_path;
SVN_ERR(svn_ra_neon__search_for_starting_props(&rsrc, &lopped_path,
sess, url, pool));
SVN_ERR(svn_ra_neon__get_vcc(&vcc, sess, url, pool));
if (vcc == NULL) {
return svn_error_create(APR_EGENERAL, NULL,
_("The VCC property was not found on the "
"resource"));
}
relative_path = apr_hash_get(rsrc->propset,
SVN_RA_NEON__PROP_BASELINE_RELPATH,
APR_HASH_KEY_STRING);
if (relative_path == NULL) {
return svn_error_create(APR_EGENERAL, NULL,
_("The relative-path property was not "
"found on the resource"));
}
my_bc_relative = svn_path_join(relative_path->data,
svn_path_uri_decode(lopped_path, pool),
pool);
if (bc_relative) {
bc_relative->data = my_bc_relative;
bc_relative->len = strlen(my_bc_relative);
}
if (revision == SVN_INVALID_REVNUM) {
const svn_string_t *baseline;
SVN_ERR(svn_ra_neon__get_one_prop(&baseline, sess, vcc, NULL,
&svn_ra_neon__checked_in_prop, pool));
SVN_ERR(svn_ra_neon__get_props_resource(&rsrc, sess,
baseline->data, NULL,
which_props, pool));
} else {
char label[20];
apr_snprintf(label, sizeof(label), "%ld", revision);
SVN_ERR(svn_ra_neon__get_props_resource(&rsrc, sess, vcc, label,
which_props, pool));
}
*bln_rsrc = rsrc;
return SVN_NO_ERROR;
}
svn_error_t *svn_ra_neon__get_baseline_info(svn_boolean_t *is_dir,
svn_string_t *bc_url,
svn_string_t *bc_relative,
svn_revnum_t *latest_rev,
svn_ra_neon__session_t *sess,
const char *url,
svn_revnum_t revision,
apr_pool_t *pool) {
svn_ra_neon__resource_t *baseline_rsrc, *rsrc;
const svn_string_t *my_bc_url;
svn_string_t my_bc_rel;
SVN_ERR(svn_ra_neon__get_baseline_props(&my_bc_rel,
&baseline_rsrc,
sess,
url,
revision,
baseline_props,
pool));
my_bc_url = apr_hash_get(baseline_rsrc->propset,
SVN_RA_NEON__PROP_BASELINE_COLLECTION,
APR_HASH_KEY_STRING);
if (my_bc_url == NULL) {
return svn_error_create(APR_EGENERAL, NULL,
_("'DAV:baseline-collection' was not present "
"on the baseline resource"));
}
if (bc_url)
*bc_url = *my_bc_url;
if (latest_rev != NULL) {
const svn_string_t *vsn_name= apr_hash_get(baseline_rsrc->propset,
SVN_RA_NEON__PROP_VERSION_NAME,
APR_HASH_KEY_STRING);
if (vsn_name == NULL) {
return svn_error_createf(APR_EGENERAL, NULL,
_("'%s' was not present on the baseline "
"resource"),
"DAV:" SVN_DAV__VERSION_NAME);
}
*latest_rev = SVN_STR_TO_REV(vsn_name->data);
}
if (is_dir != NULL) {
const char *full_bc_url = svn_path_url_add_component(my_bc_url->data,
my_bc_rel.data,
pool);
SVN_ERR(svn_ra_neon__get_starting_props(&rsrc, sess, full_bc_url,
NULL, pool));
*is_dir = rsrc->is_collection;
}
if (bc_relative)
*bc_relative = my_bc_rel;
return SVN_NO_ERROR;
}
static void
append_setprop(svn_stringbuf_t *body,
const char *name,
const svn_string_t *value,
apr_pool_t *pool) {
const char *encoding = "";
const char *xml_safe;
const char *xml_tag_name;
#define NSLEN (sizeof(SVN_PROP_PREFIX) - 1)
if (strncmp(name, SVN_PROP_PREFIX, NSLEN) == 0) {
xml_tag_name = apr_pstrcat(pool, "S:", name + NSLEN, NULL);
}
#undef NSLEN
else {
xml_tag_name = apr_pstrcat(pool, "C:", name, NULL);
}
if (! value) {
svn_stringbuf_appendcstr(body,
apr_psprintf(pool, "<%s />", xml_tag_name));
return;
}
if (svn_xml_is_xml_safe(value->data, value->len)) {
svn_stringbuf_t *xml_esc = NULL;
svn_xml_escape_cdata_string(&xml_esc, value, pool);
xml_safe = xml_esc->data;
} else {
const svn_string_t *base64ed = svn_base64_encode_string(value, pool);
encoding = " V:encoding=\"base64\"";
xml_safe = base64ed->data;
}
svn_stringbuf_appendcstr(body,
apr_psprintf(pool,"<%s %s>%s</%s>",
xml_tag_name, encoding,
xml_safe, xml_tag_name));
return;
}
svn_error_t *
svn_ra_neon__do_proppatch(svn_ra_neon__session_t *ras,
const char *url,
apr_hash_t *prop_changes,
apr_array_header_t *prop_deletes,
apr_hash_t *extra_headers,
apr_pool_t *pool) {
svn_error_t *err;
svn_stringbuf_t *body;
apr_pool_t *subpool = svn_pool_create(pool);
if ((prop_changes == NULL || (! apr_hash_count(prop_changes)))
&& (prop_deletes == NULL || prop_deletes->nelts == 0))
return SVN_NO_ERROR;
body = svn_stringbuf_create
("<?xml version=\"1.0\" encoding=\"utf-8\" ?>" DEBUG_CR
"<D:propertyupdate xmlns:D=\"DAV:\" xmlns:V=\""
SVN_DAV_PROP_NS_DAV "\" xmlns:C=\""
SVN_DAV_PROP_NS_CUSTOM "\" xmlns:S=\""
SVN_DAV_PROP_NS_SVN "\">" DEBUG_CR, pool);
if (prop_changes) {
apr_hash_index_t *hi;
svn_stringbuf_appendcstr(body, "<D:set><D:prop>");
for (hi = apr_hash_first(pool, prop_changes); hi; hi = apr_hash_next(hi)) {
const void *key;
void *val;
svn_pool_clear(subpool);
apr_hash_this(hi, &key, NULL, &val);
append_setprop(body, key, val, subpool);
}
svn_stringbuf_appendcstr(body, "</D:prop></D:set>");
}
if (prop_deletes) {
int n;
svn_stringbuf_appendcstr(body, "<D:remove><D:prop>");
for (n = 0; n < prop_deletes->nelts; n++) {
const char *name = APR_ARRAY_IDX(prop_deletes, n, const char *);
svn_pool_clear(subpool);
append_setprop(body, name, NULL, subpool);
}
svn_stringbuf_appendcstr(body, "</D:prop></D:remove>");
}
svn_pool_destroy(subpool);
svn_stringbuf_appendcstr(body, "</D:propertyupdate>");
if (! extra_headers)
extra_headers = apr_hash_make(pool);
apr_hash_set(extra_headers, "Content-Type", APR_HASH_KEY_STRING,
"text/xml; charset=UTF-8");
err = svn_ra_neon__simple_request(NULL, ras, "PROPPATCH", url,
extra_headers, body->data,
200, 207, pool);
if (err)
return svn_error_create
(SVN_ERR_RA_DAV_PROPPATCH_FAILED, err,
_("At least one property change failed; repository is unchanged"));
return SVN_NO_ERROR;
}
svn_error_t *
svn_ra_neon__do_check_path(svn_ra_session_t *session,
const char *path,
svn_revnum_t revision,
svn_node_kind_t *kind,
apr_pool_t *pool) {
svn_ra_neon__session_t *ras = session->priv;
const char *url = ras->url->data;
svn_error_t *err;
svn_boolean_t is_dir;
if (path)
url = svn_path_url_add_component(url, path, pool);
err = svn_ra_neon__get_baseline_info(&is_dir, NULL, NULL, NULL,
ras, url, revision, pool);
if (err == SVN_NO_ERROR) {
if (is_dir)
*kind = svn_node_dir;
else
*kind = svn_node_file;
} else if (err->apr_err == SVN_ERR_RA_DAV_PATH_NOT_FOUND) {
svn_error_clear(err);
*kind = svn_node_none;
return SVN_NO_ERROR;
}
return err;
}
svn_error_t *
svn_ra_neon__do_stat(svn_ra_session_t *session,
const char *path,
svn_revnum_t revision,
svn_dirent_t **dirent,
apr_pool_t *pool) {
svn_ra_neon__session_t *ras = session->priv;
const char *url = ras->url->data;
const char *final_url;
apr_hash_t *resources;
apr_hash_index_t *hi;
svn_error_t *err;
if (path)
url = svn_path_url_add_component(url, path, pool);
if (! SVN_IS_VALID_REVNUM(revision)) {
final_url = url;
} else {
svn_string_t bc_url, bc_relative;
err = svn_ra_neon__get_baseline_info(NULL, &bc_url, &bc_relative,
NULL, ras,
url, revision, pool);
if (err) {
if (err->apr_err == SVN_ERR_RA_DAV_PATH_NOT_FOUND) {
svn_error_clear(err);
*dirent = NULL;
return SVN_NO_ERROR;
} else
return err;
}
final_url = svn_path_url_add_component(bc_url.data, bc_relative.data,
pool);
}
err = svn_ra_neon__get_props(&resources, ras, final_url,
SVN_RA_NEON__DEPTH_ZERO,
NULL, NULL , pool);
if (err) {
if (err->apr_err == SVN_ERR_RA_DAV_PATH_NOT_FOUND) {
svn_error_clear(err);
*dirent = NULL;
return SVN_NO_ERROR;
} else
return err;
}
for (hi = apr_hash_first(pool, resources); hi; hi = apr_hash_next(hi)) {
void *val;
svn_ra_neon__resource_t *resource;
const svn_string_t *propval;
apr_hash_index_t *h;
svn_dirent_t *entry;
apr_hash_this(hi, NULL, NULL, &val);
resource = val;
entry = apr_pcalloc(pool, sizeof(*entry));
entry->kind = resource->is_collection ? svn_node_dir : svn_node_file;
if (entry->kind == svn_node_file) {
propval = apr_hash_get(resource->propset,
SVN_RA_NEON__PROP_GETCONTENTLENGTH,
APR_HASH_KEY_STRING);
if (propval)
entry->size = svn__atoui64(propval->data);
}
for (h = apr_hash_first(pool, resource->propset);
h; h = apr_hash_next(h)) {
const void *kkey;
apr_hash_this(h, &kkey, NULL, NULL);
if (strncmp((const char *)kkey, SVN_DAV_PROP_NS_CUSTOM,
sizeof(SVN_DAV_PROP_NS_CUSTOM) - 1) == 0)
entry->has_props = TRUE;
else if (strncmp((const char *)kkey, SVN_DAV_PROP_NS_SVN,
sizeof(SVN_DAV_PROP_NS_SVN) - 1) == 0)
entry->has_props = TRUE;
}
propval = apr_hash_get(resource->propset,
SVN_RA_NEON__PROP_VERSION_NAME,
APR_HASH_KEY_STRING);
if (propval != NULL)
entry->created_rev = SVN_STR_TO_REV(propval->data);
propval = apr_hash_get(resource->propset,
SVN_RA_NEON__PROP_CREATIONDATE,
APR_HASH_KEY_STRING);
if (propval != NULL)
SVN_ERR(svn_time_from_cstring(&(entry->time),
propval->data, pool));
propval = apr_hash_get(resource->propset,
SVN_RA_NEON__PROP_CREATOR_DISPLAYNAME,
APR_HASH_KEY_STRING);
if (propval != NULL)
entry->last_author = propval->data;
*dirent = entry;
}
return SVN_NO_ERROR;
}
