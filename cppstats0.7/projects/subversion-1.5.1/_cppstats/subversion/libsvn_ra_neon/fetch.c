#include <assert.h>
#include <stdlib.h>
#define APR_WANT_STRFUNC
#include <apr_want.h>
#include <apr_pools.h>
#include <apr_tables.h>
#include <apr_strings.h>
#include <apr_md5.h>
#include <apr_xml.h>
#include <ne_basic.h>
#include "svn_error.h"
#include "svn_pools.h"
#include "svn_delta.h"
#include "svn_io.h"
#include "svn_md5.h"
#include "svn_base64.h"
#include "svn_ra.h"
#include "../libsvn_ra/ra_loader.h"
#include "svn_path.h"
#include "svn_xml.h"
#include "svn_dav.h"
#include "svn_time.h"
#include "svn_props.h"
#include "private/svn_dav_protocol.h"
#include "svn_private_config.h"
#include "ra_neon.h"
typedef struct {
svn_ra_neon__resource_t *rsrc;
void *parent_baton;
} subdir_t;
typedef struct {
apr_pool_t *pool;
svn_txdelta_window_handler_t handler;
void *handler_baton;
svn_stream_t *stream;
} file_read_ctx_t;
typedef struct {
svn_boolean_t do_checksum;
apr_md5_ctx_t md5_context;
svn_stream_t *stream;
} file_write_ctx_t;
typedef struct {
svn_ra_neon__request_t *req;
int checked_type;
void *subctx;
} custom_get_ctx_t;
#define POP_SUBDIR(sds) (APR_ARRAY_IDX((sds), --(sds)->nelts, subdir_t *))
#define PUSH_SUBDIR(sds,s) (APR_ARRAY_PUSH((sds), subdir_t *) = (s))
typedef svn_error_t * (*prop_setter_t)(void *baton,
const char *name,
const svn_string_t *value,
apr_pool_t *pool);
typedef struct {
void *baton;
svn_boolean_t fetch_props;
const char *vsn_url;
svn_stringbuf_t *pathbuf;
apr_hash_t *children;
apr_pool_t *pool;
} dir_item_t;
typedef struct {
svn_ra_neon__session_t *ras;
apr_file_t *tmpfile;
apr_pool_t *pool;
apr_pool_t *scratch_pool;
svn_boolean_t fetch_content;
svn_boolean_t fetch_props;
const svn_delta_editor_t *editor;
void *edit_baton;
apr_array_header_t *dirs;
#define TOP_DIR(rb) (APR_ARRAY_IDX((rb)->dirs, (rb)->dirs->nelts - 1, dir_item_t))
#define DIR_DEPTH(rb) ((rb)->dirs->nelts)
#define PUSH_BATON(rb,b) (APR_ARRAY_PUSH((rb)->dirs, void *) = (b))
void *file_baton;
apr_pool_t *file_pool;
const char *result_checksum;
svn_stringbuf_t *namestr;
svn_stringbuf_t *cpathstr;
svn_stringbuf_t *href;
svn_stringbuf_t *encoding;
svn_txdelta_window_handler_t whandler;
void *whandler_baton;
svn_stream_t *svndiff_decoder;
svn_stream_t *base64_decoder;
svn_stringbuf_t *cdata_accum;
svn_boolean_t in_resource;
svn_stringbuf_t *current_wcprop_path;
svn_boolean_t is_switch;
const char *target;
svn_boolean_t send_copyfrom_args;
svn_boolean_t spool_response;
svn_boolean_t receiving_all;
apr_hash_t *lock_tokens;
} report_baton_t;
static const svn_ra_neon__xml_elm_t report_elements[] = {
{ SVN_XML_NAMESPACE, "update-report", ELEM_update_report, 0 },
{ SVN_XML_NAMESPACE, "resource-walk", ELEM_resource_walk, 0 },
{ SVN_XML_NAMESPACE, "resource", ELEM_resource, 0 },
{ SVN_XML_NAMESPACE, "target-revision", ELEM_target_revision, 0 },
{ SVN_XML_NAMESPACE, "open-directory", ELEM_open_directory, 0 },
{ SVN_XML_NAMESPACE, "add-directory", ELEM_add_directory, 0 },
{ SVN_XML_NAMESPACE, "absent-directory", ELEM_absent_directory, 0 },
{ SVN_XML_NAMESPACE, "open-file", ELEM_open_file, 0 },
{ SVN_XML_NAMESPACE, "add-file", ELEM_add_file, 0 },
{ SVN_XML_NAMESPACE, "txdelta", ELEM_txdelta, 0 },
{ SVN_XML_NAMESPACE, "absent-file", ELEM_absent_file, 0 },
{ SVN_XML_NAMESPACE, "delete-entry", ELEM_delete_entry, 0 },
{ SVN_XML_NAMESPACE, "fetch-props", ELEM_fetch_props, 0 },
{ SVN_XML_NAMESPACE, "set-prop", ELEM_set_prop, 0 },
{ SVN_XML_NAMESPACE, "remove-prop", ELEM_remove_prop, 0 },
{ SVN_XML_NAMESPACE, "fetch-file", ELEM_fetch_file, 0 },
{ SVN_XML_NAMESPACE, "prop", ELEM_SVN_prop, 0 },
{
SVN_DAV_PROP_NS_DAV, "repository-uuid",
ELEM_repository_uuid, SVN_RA_NEON__XML_CDATA
},
{
SVN_DAV_PROP_NS_DAV, "md5-checksum", ELEM_md5_checksum,
SVN_RA_NEON__XML_CDATA
},
{ "DAV:", "version-name", ELEM_version_name, SVN_RA_NEON__XML_CDATA },
{ "DAV:", SVN_DAV__CREATIONDATE, ELEM_creationdate, SVN_RA_NEON__XML_CDATA },
{
"DAV:", "creator-displayname", ELEM_creator_displayname,
SVN_RA_NEON__XML_CDATA
},
{ "DAV:", "checked-in", ELEM_checked_in, 0 },
{ "DAV:", "href", ELEM_href, SVN_RA_NEON__XML_CDATA },
{ NULL }
};
static svn_error_t *simple_store_vsn_url(const char *vsn_url,
void *baton,
prop_setter_t setter,
apr_pool_t *pool) {
SVN_ERR_W((*setter)(baton, SVN_RA_NEON__LP_VSN_URL,
svn_string_create(vsn_url, pool), pool),
_("Could not save the URL of the version resource"));
return NULL;
}
static svn_error_t *get_delta_base(const char **delta_base,
const char *relpath,
svn_ra_get_wc_prop_func_t get_wc_prop,
void *cb_baton,
apr_pool_t *pool) {
const svn_string_t *value;
if (relpath == NULL || get_wc_prop == NULL) {
*delta_base = NULL;
return SVN_NO_ERROR;
}
SVN_ERR((*get_wc_prop)(cb_baton, relpath, SVN_RA_NEON__LP_VSN_URL,
&value, pool));
*delta_base = value ? value->data : NULL;
return SVN_NO_ERROR;
}
static svn_error_t *set_special_wc_prop(const char *key,
const svn_string_t *val,
prop_setter_t setter,
void *baton,
apr_pool_t *pool) {
const char *name = NULL;
if (strcmp(key, SVN_RA_NEON__PROP_VERSION_NAME) == 0)
name = SVN_PROP_ENTRY_COMMITTED_REV;
else if (strcmp(key, SVN_RA_NEON__PROP_CREATIONDATE) == 0)
name = SVN_PROP_ENTRY_COMMITTED_DATE;
else if (strcmp(key, SVN_RA_NEON__PROP_CREATOR_DISPLAYNAME) == 0)
name = SVN_PROP_ENTRY_LAST_AUTHOR;
else if (strcmp(key, SVN_RA_NEON__PROP_REPOSITORY_UUID) == 0)
name = SVN_PROP_ENTRY_UUID;
if (name)
SVN_ERR((*setter)(baton, name, val, pool));
return SVN_NO_ERROR;
}
static svn_error_t *add_props(apr_hash_t *props,
prop_setter_t setter,
void *baton,
apr_pool_t *pool) {
apr_hash_index_t *hi;
for (hi = apr_hash_first(pool, props); hi; hi = apr_hash_next(hi)) {
const void *vkey;
void *vval;
const char *key;
const svn_string_t *val;
apr_hash_this(hi, &vkey, NULL, &vval);
key = vkey;
val = vval;
#define NSLEN (sizeof(SVN_DAV_PROP_NS_CUSTOM) - 1)
if (strncmp(key, SVN_DAV_PROP_NS_CUSTOM, NSLEN) == 0) {
SVN_ERR((*setter)(baton, key + NSLEN, val, pool));
continue;
}
#undef NSLEN
#define NSLEN (sizeof(SVN_DAV_PROP_NS_SVN) - 1)
if (strncmp(key, SVN_DAV_PROP_NS_SVN, NSLEN) == 0) {
SVN_ERR((*setter)(baton, apr_pstrcat(pool, SVN_PROP_PREFIX,
key + NSLEN, NULL),
val, pool));
}
#undef NSLEN
else {
SVN_ERR(set_special_wc_prop(key, val, setter, baton, pool));
}
}
return SVN_NO_ERROR;
}
static svn_error_t *custom_get_request(svn_ra_neon__session_t *ras,
const char *url,
const char *relpath,
svn_ra_neon__block_reader reader,
void *subctx,
svn_ra_get_wc_prop_func_t get_wc_prop,
void *cb_baton,
svn_boolean_t use_base,
apr_pool_t *pool) {
custom_get_ctx_t cgc = { 0 };
const char *delta_base;
svn_ra_neon__request_t *request;
svn_error_t *err;
if (use_base) {
SVN_ERR(get_delta_base(&delta_base, relpath,
get_wc_prop, cb_baton, pool));
} else {
delta_base = NULL;
}
request = svn_ra_neon__request_create(ras, "GET", url, pool);
if (delta_base) {
ne_add_request_header(request->ne_req,
SVN_DAV_DELTA_BASE_HEADER, delta_base);
}
svn_ra_neon__add_response_body_reader(request, ne_accept_2xx, reader, &cgc);
cgc.req = request;
cgc.subctx = subctx;
err = svn_ra_neon__request_dispatch(NULL, request, NULL, NULL,
200 ,
226 ,
pool);
svn_ra_neon__request_destroy(request);
return err;
}
static svn_error_t *
fetch_file_reader(void *userdata, const char *buf, size_t len) {
custom_get_ctx_t *cgc = userdata;
file_read_ctx_t *frc = cgc->subctx;
if (len == 0) {
return 0;
}
if (!cgc->checked_type) {
ne_content_type ctype = { 0 };
int rv = ne_get_content_type(cgc->req->ne_req, &ctype);
if (rv != 0)
return
svn_error_create(SVN_ERR_RA_DAV_RESPONSE_HEADER_BADNESS, NULL,
_("Could not get content-type from response"));
if (!strcmp(ctype.type, "application")
&& !strcmp(ctype.subtype, "vnd.svn-svndiff")) {
frc->stream = svn_txdelta_parse_svndiff(frc->handler,
frc->handler_baton,
TRUE,
frc->pool);
}
if (ctype.value)
free(ctype.value);
cgc->checked_type = 1;
}
if (frc->stream == NULL) {
svn_txdelta_window_t window = { 0 };
svn_txdelta_op_t op;
svn_string_t data;
data.data = buf;
data.len = len;
op.action_code = svn_txdelta_new;
op.offset = 0;
op.length = len;
window.tview_len = len;
window.num_ops = 1;
window.ops = &op;
window.new_data = &data;
SVN_RA_NEON__REQ_ERR
(cgc->req,
(*frc->handler)(&window, frc->handler_baton));
} else {
apr_size_t written = len;
SVN_ERR(svn_stream_write(frc->stream, buf, &written));
#if 0
if (written != len && cgc->err == NULL)
cgc->err = svn_error_createf(SVN_ERR_INCOMPLETE_DATA, NULL,
"Unable to completely write the svndiff "
"data to the parser stream "
"(wrote " APR_SIZE_T_FMT " "
"of " APR_SIZE_T_FMT " bytes)",
written, len);
#endif
}
return 0;
}
static svn_error_t *simple_fetch_file(svn_ra_neon__session_t *ras,
const char *url,
const char *relpath,
svn_boolean_t text_deltas,
void *file_baton,
const char *base_checksum,
const svn_delta_editor_t *editor,
svn_ra_get_wc_prop_func_t get_wc_prop,
void *cb_baton,
apr_pool_t *pool) {
file_read_ctx_t frc = { 0 };
SVN_ERR_W((*editor->apply_textdelta)(file_baton,
base_checksum,
pool,
&frc.handler,
&frc.handler_baton),
_("Could not save file"));
if (! text_deltas) {
SVN_ERR((*frc.handler)(NULL, frc.handler_baton));
return SVN_NO_ERROR;
}
frc.pool = pool;
SVN_ERR(custom_get_request(ras, url, relpath,
fetch_file_reader, &frc,
get_wc_prop, cb_baton,
TRUE, pool));
SVN_ERR((*frc.handler)(NULL, frc.handler_baton));
return SVN_NO_ERROR;
}
static svn_error_t *
get_file_reader(void *userdata, const char *buf, size_t len) {
custom_get_ctx_t *cgc = userdata;
file_write_ctx_t *fwc = cgc->subctx;
svn_stream_t *stream = fwc->stream;
if (fwc->do_checksum)
apr_md5_update(&(fwc->md5_context), buf, len);
SVN_ERR(svn_stream_write(stream, buf, &len));
return SVN_NO_ERROR;
}
static svn_error_t *
add_prop_to_hash(void *baton,
const char *name,
const svn_string_t *value,
apr_pool_t *pool) {
apr_hash_t *ht = (apr_hash_t *) baton;
apr_hash_set(ht, name, APR_HASH_KEY_STRING, value);
return SVN_NO_ERROR;
}
static svn_error_t *
filter_props(apr_hash_t *props,
svn_ra_neon__resource_t *rsrc,
svn_boolean_t add_entry_props,
apr_pool_t *pool) {
apr_hash_index_t *hi;
for (hi = apr_hash_first(pool, rsrc->propset); hi; hi = apr_hash_next(hi)) {
const void *key;
const char *name;
void *val;
const svn_string_t *value;
apr_hash_this(hi, &key, NULL, &val);
name = key;
value = svn_string_dup(val, pool);
#define NSLEN (sizeof(SVN_DAV_PROP_NS_CUSTOM) - 1)
if (strncmp(name, SVN_DAV_PROP_NS_CUSTOM, NSLEN) == 0) {
apr_hash_set(props, name + NSLEN, APR_HASH_KEY_STRING, value);
continue;
}
#undef NSLEN
#define NSLEN (sizeof(SVN_DAV_PROP_NS_SVN) - 1)
if (strncmp(name, SVN_DAV_PROP_NS_SVN, NSLEN) == 0) {
apr_hash_set(props,
apr_pstrcat(pool, SVN_PROP_PREFIX, name + NSLEN, NULL),
APR_HASH_KEY_STRING,
value);
continue;
}
#undef NSLEN
else if (strcmp(name, SVN_RA_NEON__PROP_CHECKED_IN) == 0) {
apr_hash_set(props, SVN_RA_NEON__LP_VSN_URL,
APR_HASH_KEY_STRING, value);
} else {
if (add_entry_props)
SVN_ERR(set_special_wc_prop(name, value, add_prop_to_hash,
props, pool));
}
}
return SVN_NO_ERROR;
}
svn_error_t *svn_ra_neon__get_file(svn_ra_session_t *session,
const char *path,
svn_revnum_t revision,
svn_stream_t *stream,
svn_revnum_t *fetched_rev,
apr_hash_t **props,
apr_pool_t *pool) {
svn_ra_neon__resource_t *rsrc;
const char *final_url;
svn_ra_neon__session_t *ras = session->priv;
const char *url = svn_path_url_add_component(ras->url->data, path, pool);
if ((! SVN_IS_VALID_REVNUM(revision)) && (fetched_rev == NULL))
final_url = url;
else {
svn_revnum_t got_rev;
svn_string_t bc_url, bc_relative;
SVN_ERR(svn_ra_neon__get_baseline_info(NULL,
&bc_url, &bc_relative,
&got_rev,
ras,
url, revision,
pool));
final_url = svn_path_url_add_component(bc_url.data,
bc_relative.data,
pool);
if (fetched_rev != NULL)
*fetched_rev = got_rev;
}
if (stream) {
svn_error_t *err;
const svn_string_t *expected_checksum = NULL;
file_write_ctx_t fwc;
ne_propname md5_propname = { SVN_DAV_PROP_NS_DAV, "md5-checksum" };
unsigned char digest[APR_MD5_DIGESTSIZE];
const char *hex_digest;
err = svn_ra_neon__get_one_prop(&expected_checksum,
ras,
final_url,
NULL,
&md5_propname,
pool);
if ((err && (err->apr_err == SVN_ERR_RA_DAV_PROPS_NOT_FOUND))
|| (expected_checksum && (*expected_checksum->data == '\0'))) {
fwc.do_checksum = FALSE;
svn_error_clear(err);
} else if (err)
return err;
else
fwc.do_checksum = TRUE;
fwc.stream = stream;
if (fwc.do_checksum)
apr_md5_init(&(fwc.md5_context));
SVN_ERR(custom_get_request(ras, final_url, path,
get_file_reader, &fwc,
ras->callbacks->get_wc_prop,
ras->callback_baton,
FALSE, pool));
if (fwc.do_checksum) {
apr_md5_final(digest, &(fwc.md5_context));
hex_digest = svn_md5_digest_to_cstring_display(digest, pool);
if (strcmp(hex_digest, expected_checksum->data) != 0)
return svn_error_createf
(SVN_ERR_CHECKSUM_MISMATCH, NULL,
_("Checksum mismatch for '%s':\n"
" expected checksum: %s\n"
" actual checksum: %s\n"),
path, expected_checksum->data, hex_digest);
}
}
if (props) {
SVN_ERR(svn_ra_neon__get_props_resource(&rsrc, ras, final_url,
NULL, NULL ,
pool));
*props = apr_hash_make(pool);
SVN_ERR(filter_props(*props, rsrc, TRUE, pool));
}
return SVN_NO_ERROR;
}
static const ne_propname deadprop_count_support_props[] = {
{ SVN_DAV_PROP_NS_DAV, "deadprop-count" },
{ NULL }
};
svn_error_t *svn_ra_neon__get_dir(svn_ra_session_t *session,
apr_hash_t **dirents,
svn_revnum_t *fetched_rev,
apr_hash_t **props,
const char *path,
svn_revnum_t revision,
apr_uint32_t dirent_fields,
apr_pool_t *pool) {
svn_ra_neon__resource_t *rsrc;
apr_hash_index_t *hi;
apr_hash_t *resources;
const char *final_url;
apr_size_t final_url_n_components;
svn_boolean_t supports_deadprop_count;
svn_ra_neon__session_t *ras = session->priv;
const char *url = svn_path_url_add_component(ras->url->data, path, pool);
if ((! SVN_IS_VALID_REVNUM(revision)) && (fetched_rev == NULL))
final_url = url;
else {
svn_revnum_t got_rev;
svn_string_t bc_url, bc_relative;
SVN_ERR(svn_ra_neon__get_baseline_info(NULL,
&bc_url, &bc_relative,
&got_rev,
ras,
url, revision,
pool));
final_url = svn_path_url_add_component(bc_url.data,
bc_relative.data,
pool);
if (fetched_rev != NULL)
*fetched_rev = got_rev;
}
{
const svn_string_t *deadprop_count;
SVN_ERR(svn_ra_neon__get_props_resource(&rsrc, ras,
final_url, NULL,
deadprop_count_support_props,
pool));
deadprop_count = apr_hash_get(rsrc->propset,
SVN_RA_NEON__PROP_DEADPROP_COUNT,
APR_HASH_KEY_STRING);
supports_deadprop_count = (deadprop_count != NULL);
}
if (dirents) {
ne_propname *which_props;
if ((SVN_DIRENT_HAS_PROPS & dirent_fields) == 0
|| supports_deadprop_count) {
int num_props = 1;
if (dirent_fields & SVN_DIRENT_KIND)
++num_props;
if (dirent_fields & SVN_DIRENT_SIZE)
++num_props;
if (dirent_fields & SVN_DIRENT_HAS_PROPS)
++num_props;
if (dirent_fields & SVN_DIRENT_CREATED_REV)
++num_props;
if (dirent_fields & SVN_DIRENT_TIME)
++num_props;
if (dirent_fields & SVN_DIRENT_LAST_AUTHOR)
++num_props;
which_props = apr_pcalloc(pool, num_props * sizeof(ne_propname));
--num_props;
which_props[num_props].nspace = NULL;
which_props[num_props--].name = NULL;
if (dirent_fields & SVN_DIRENT_KIND) {
which_props[num_props].nspace = "DAV:";
which_props[num_props--].name = "resourcetype";
}
if (dirent_fields & SVN_DIRENT_SIZE) {
which_props[num_props].nspace = "DAV:";
which_props[num_props--].name = "getcontentlength";
}
if (dirent_fields & SVN_DIRENT_HAS_PROPS) {
which_props[num_props].nspace = SVN_DAV_PROP_NS_DAV;
which_props[num_props--].name = "deadprop-count";
}
if (dirent_fields & SVN_DIRENT_CREATED_REV) {
which_props[num_props].nspace = "DAV:";
which_props[num_props--].name = "version-name";
}
if (dirent_fields & SVN_DIRENT_TIME) {
which_props[num_props].nspace = "DAV:";
which_props[num_props--].name = SVN_DAV__CREATIONDATE;
}
if (dirent_fields & SVN_DIRENT_LAST_AUTHOR) {
which_props[num_props].nspace = "DAV:";
which_props[num_props--].name = "creator-displayname";
}
assert(num_props == -1);
} else {
which_props = NULL;
}
SVN_ERR(svn_ra_neon__get_props(&resources, ras,
final_url, SVN_RA_NEON__DEPTH_ONE,
NULL, which_props, pool));
final_url_n_components = svn_path_component_count(final_url);
*dirents = apr_hash_make(pool);
for (hi = apr_hash_first(pool, resources);
hi;
hi = apr_hash_next(hi)) {
const void *key;
void *val;
const char *childname;
svn_ra_neon__resource_t *resource;
const svn_string_t *propval;
apr_hash_index_t *h;
svn_dirent_t *entry;
apr_hash_this(hi, &key, NULL, &val);
childname = key;
resource = val;
if (svn_path_component_count(childname) == final_url_n_components)
continue;
entry = apr_pcalloc(pool, sizeof(*entry));
if (dirent_fields & SVN_DIRENT_KIND) {
entry->kind = resource->is_collection ? svn_node_dir
: svn_node_file;
}
if (dirent_fields & SVN_DIRENT_SIZE) {
propval = apr_hash_get(resource->propset,
SVN_RA_NEON__PROP_GETCONTENTLENGTH,
APR_HASH_KEY_STRING);
if (propval == NULL)
entry->size = 0;
else
entry->size = svn__atoui64(propval->data);
}
if (dirent_fields & SVN_DIRENT_HAS_PROPS) {
if (supports_deadprop_count) {
propval = apr_hash_get(resource->propset,
SVN_RA_NEON__PROP_DEADPROP_COUNT,
APR_HASH_KEY_STRING);
if (propval == NULL) {
return svn_error_create(SVN_ERR_INCOMPLETE_DATA, NULL,
_("Server response missing the "
"expected deadprop-count "
"property"));
} else {
apr_int64_t prop_count = svn__atoui64(propval->data);
entry->has_props = (prop_count > 0);
}
} else {
for (h = apr_hash_first(pool, resource->propset);
h; h = apr_hash_next(h)) {
const void *kkey;
apr_hash_this(h, &kkey, NULL, NULL);
if (strncmp((const char *) kkey, SVN_DAV_PROP_NS_CUSTOM,
sizeof(SVN_DAV_PROP_NS_CUSTOM) - 1) == 0
|| strncmp((const char *) kkey, SVN_DAV_PROP_NS_SVN,
sizeof(SVN_DAV_PROP_NS_SVN) - 1) == 0)
entry->has_props = TRUE;
}
}
}
if (dirent_fields & SVN_DIRENT_CREATED_REV) {
propval = apr_hash_get(resource->propset,
SVN_RA_NEON__PROP_VERSION_NAME,
APR_HASH_KEY_STRING);
if (propval != NULL)
entry->created_rev = SVN_STR_TO_REV(propval->data);
}
if (dirent_fields & SVN_DIRENT_TIME) {
propval = apr_hash_get(resource->propset,
SVN_RA_NEON__PROP_CREATIONDATE,
APR_HASH_KEY_STRING);
if (propval != NULL)
SVN_ERR(svn_time_from_cstring(&(entry->time),
propval->data, pool));
}
if (dirent_fields & SVN_DIRENT_LAST_AUTHOR) {
propval = apr_hash_get(resource->propset,
SVN_RA_NEON__PROP_CREATOR_DISPLAYNAME,
APR_HASH_KEY_STRING);
if (propval != NULL)
entry->last_author = propval->data;
}
apr_hash_set(*dirents,
svn_path_uri_decode(svn_path_basename(childname, pool),
pool),
APR_HASH_KEY_STRING, entry);
}
}
if (props) {
SVN_ERR(svn_ra_neon__get_props_resource(&rsrc, ras, final_url,
NULL, NULL ,
pool));
*props = apr_hash_make(pool);
SVN_ERR(filter_props(*props, rsrc, TRUE, pool));
}
return SVN_NO_ERROR;
}
svn_error_t *svn_ra_neon__get_latest_revnum(svn_ra_session_t *session,
svn_revnum_t *latest_revnum,
apr_pool_t *pool) {
svn_ra_neon__session_t *ras = session->priv;
SVN_ERR(svn_ra_neon__get_baseline_info(NULL, NULL, NULL, latest_revnum,
ras, ras->root.path,
SVN_INVALID_REVNUM, pool));
SVN_ERR(svn_ra_neon__maybe_store_auth_info(ras, pool));
return NULL;
}
svn_error_t *svn_ra_neon__change_rev_prop(svn_ra_session_t *session,
svn_revnum_t rev,
const char *name,
const svn_string_t *value,
apr_pool_t *pool) {
svn_ra_neon__session_t *ras = session->priv;
svn_ra_neon__resource_t *baseline;
svn_error_t *err;
apr_hash_t *prop_changes = NULL;
apr_array_header_t *prop_deletes = NULL;
static const ne_propname wanted_props[] = {
{ "DAV:", "auto-version" },
{ NULL }
};
SVN_ERR(svn_ra_neon__get_baseline_props(NULL, &baseline,
ras,
ras->url->data,
rev,
wanted_props,
pool));
if (value) {
prop_changes = apr_hash_make(pool);
apr_hash_set(prop_changes, name, APR_HASH_KEY_STRING, value);
} else {
prop_deletes = apr_array_make(pool, 1, sizeof(const char *));
APR_ARRAY_PUSH(prop_deletes, const char *) = name;
}
err = svn_ra_neon__do_proppatch(ras, baseline->url, prop_changes,
prop_deletes, NULL, pool);
if (err)
return
svn_error_create
(SVN_ERR_RA_DAV_REQUEST_FAILED, err,
_("DAV request failed; it's possible that the repository's "
"pre-revprop-change hook either failed or is non-existent"));
return SVN_NO_ERROR;
}
svn_error_t *svn_ra_neon__rev_proplist(svn_ra_session_t *session,
svn_revnum_t rev,
apr_hash_t **props,
apr_pool_t *pool) {
svn_ra_neon__session_t *ras = session->priv;
svn_ra_neon__resource_t *baseline;
*props = apr_hash_make(pool);
SVN_ERR(svn_ra_neon__get_baseline_props(NULL, &baseline,
ras,
ras->url->data,
rev,
NULL,
pool));
SVN_ERR(filter_props(*props, baseline, FALSE, pool));
return SVN_NO_ERROR;
}
svn_error_t *svn_ra_neon__rev_prop(svn_ra_session_t *session,
svn_revnum_t rev,
const char *name,
svn_string_t **value,
apr_pool_t *pool) {
apr_hash_t *props;
SVN_ERR(svn_ra_neon__rev_proplist(session, rev, &props, pool));
*value = apr_hash_get(props, name, APR_HASH_KEY_STRING);
return SVN_NO_ERROR;
}
static int validate_element(svn_ra_neon__xml_elmid parent,
svn_ra_neon__xml_elmid child) {
switch (parent) {
case ELEM_root:
if (child == ELEM_update_report)
return child;
else
return SVN_RA_NEON__XML_INVALID;
case ELEM_update_report:
if (child == ELEM_target_revision
|| child == ELEM_open_directory
|| child == ELEM_resource_walk)
return child;
else
return SVN_RA_NEON__XML_INVALID;
case ELEM_resource_walk:
if (child == ELEM_resource)
return child;
else
return SVN_RA_NEON__XML_INVALID;
case ELEM_resource:
if (child == ELEM_checked_in)
return child;
else
return SVN_RA_NEON__XML_INVALID;
case ELEM_open_directory:
if (child == ELEM_absent_directory
|| child == ELEM_open_directory
|| child == ELEM_add_directory
|| child == ELEM_absent_file
|| child == ELEM_open_file
|| child == ELEM_add_file
|| child == ELEM_fetch_props
|| child == ELEM_set_prop
|| child == ELEM_remove_prop
|| child == ELEM_delete_entry
|| child == ELEM_SVN_prop
|| child == ELEM_checked_in)
return child;
else
return SVN_RA_NEON__XML_INVALID;
case ELEM_add_directory:
if (child == ELEM_absent_directory
|| child == ELEM_add_directory
|| child == ELEM_absent_file
|| child == ELEM_add_file
|| child == ELEM_remove_prop
|| child == ELEM_set_prop
|| child == ELEM_SVN_prop
|| child == ELEM_checked_in)
return child;
else
return SVN_RA_NEON__XML_INVALID;
case ELEM_open_file:
if (child == ELEM_checked_in
|| child == ELEM_fetch_file
|| child == ELEM_SVN_prop
|| child == ELEM_txdelta
|| child == ELEM_fetch_props
|| child == ELEM_set_prop
|| child == ELEM_remove_prop)
return child;
else
return SVN_RA_NEON__XML_INVALID;
case ELEM_add_file:
if (child == ELEM_checked_in
|| child == ELEM_txdelta
|| child == ELEM_set_prop
|| child == ELEM_remove_prop
|| child == ELEM_SVN_prop)
return child;
else
return SVN_RA_NEON__XML_INVALID;
case ELEM_checked_in:
if (child == ELEM_href)
return child;
else
return SVN_RA_NEON__XML_INVALID;
case ELEM_set_prop:
return child;
case ELEM_SVN_prop:
return child;
default:
return SVN_RA_NEON__XML_DECLINE;
}
}
static void push_dir(report_baton_t *rb,
void *baton,
svn_stringbuf_t *pathbuf,
apr_pool_t *pool) {
dir_item_t *di = apr_array_push(rb->dirs);
memset(di, 0, sizeof(*di));
di->baton = baton;
di->pathbuf = pathbuf;
di->pool = pool;
}
static svn_error_t *
start_element(int *elem, void *userdata, int parent, const char *nspace,
const char *elt_name, const char **atts) {
report_baton_t *rb = userdata;
const char *att;
svn_revnum_t base;
const char *name;
const char *bc_url;
svn_stringbuf_t *cpath = NULL;
svn_revnum_t crev = SVN_INVALID_REVNUM;
dir_item_t *parent_dir;
void *new_dir_baton;
svn_stringbuf_t *pathbuf;
apr_pool_t *subpool;
const char *base_checksum = NULL;
const svn_ra_neon__xml_elm_t *elm;
elm = svn_ra_neon__lookup_xml_elem(report_elements, nspace, elt_name);
*elem = elm ? validate_element(parent, elm->id) : SVN_RA_NEON__XML_DECLINE;
if (*elem < 1)
return SVN_NO_ERROR;
switch (elm->id) {
case ELEM_update_report:
att = svn_xml_get_attr_value("send-all", atts);
if (att && (strcmp(att, "true") == 0))
rb->receiving_all = TRUE;
break;
case ELEM_target_revision:
att = svn_xml_get_attr_value("rev", atts);
if (att == NULL)
return svn_error_createf(SVN_ERR_RA_DAV_MALFORMED_DATA, NULL,
_("Missing rev attr in target-revision"
" element"));
SVN_ERR((*rb->editor->set_target_revision)(rb->edit_baton,
SVN_STR_TO_REV(att),
rb->pool));
break;
case ELEM_absent_directory:
name = svn_xml_get_attr_value("name", atts);
if (name == NULL)
return svn_error_createf(SVN_ERR_RA_DAV_MALFORMED_DATA, NULL,
_("Missing name attr in absent-directory"
" element"));
parent_dir = &TOP_DIR(rb);
pathbuf = svn_stringbuf_dup(parent_dir->pathbuf, parent_dir->pool);
svn_path_add_component(pathbuf, name);
SVN_ERR((*rb->editor->absent_directory)(pathbuf->data,
parent_dir->baton,
parent_dir->pool));
break;
case ELEM_absent_file:
name = svn_xml_get_attr_value("name", atts);
if (name == NULL)
return svn_error_createf(SVN_ERR_RA_DAV_MALFORMED_DATA, NULL,
_("Missing name attr in absent-file"
" element"));
parent_dir = &TOP_DIR(rb);
pathbuf = svn_stringbuf_dup(parent_dir->pathbuf, parent_dir->pool);
svn_path_add_component(pathbuf, name);
SVN_ERR((*rb->editor->absent_file)(pathbuf->data,
parent_dir->baton,
parent_dir->pool));
break;
case ELEM_resource:
att = svn_xml_get_attr_value("path", atts);
if (att == NULL)
return svn_error_createf(SVN_ERR_RA_DAV_MALFORMED_DATA, NULL,
_("Missing path attr in resource element"));
svn_stringbuf_set(rb->current_wcprop_path, att);
rb->in_resource = TRUE;
break;
case ELEM_open_directory:
att = svn_xml_get_attr_value("rev", atts);
if (att == NULL)
return svn_error_createf(SVN_ERR_RA_DAV_MALFORMED_DATA, NULL,
_("Missing rev attr in open-directory"
" element"));
base = SVN_STR_TO_REV(att);
if (DIR_DEPTH(rb) == 0) {
pathbuf = svn_stringbuf_create("", rb->pool);
if (rb->is_switch && rb->ras->callbacks->invalidate_wc_props) {
SVN_ERR(rb->ras->callbacks->invalidate_wc_props
(rb->ras->callback_baton, rb->target,
SVN_RA_NEON__LP_VSN_URL, rb->pool));
}
subpool = svn_pool_create(rb->pool);
SVN_ERR((*rb->editor->open_root)(rb->edit_baton, base,
subpool, &new_dir_baton));
push_dir(rb, new_dir_baton, pathbuf, subpool);
} else {
name = svn_xml_get_attr_value("name", atts);
if (name == NULL)
return svn_error_createf(SVN_ERR_RA_DAV_MALFORMED_DATA, NULL,
_("Missing name attr in open-directory"
" element"));
svn_stringbuf_set(rb->namestr, name);
parent_dir = &TOP_DIR(rb);
subpool = svn_pool_create(parent_dir->pool);
pathbuf = svn_stringbuf_dup(parent_dir->pathbuf, subpool);
svn_path_add_component(pathbuf, rb->namestr->data);
SVN_ERR((*rb->editor->open_directory)(pathbuf->data,
parent_dir->baton, base,
subpool,
&new_dir_baton));
push_dir(rb, new_dir_baton, pathbuf, subpool);
}
TOP_DIR(rb).fetch_props = FALSE;
break;
case ELEM_add_directory:
name = svn_xml_get_attr_value("name", atts);
if (name == NULL)
return svn_error_createf(SVN_ERR_RA_DAV_MALFORMED_DATA, NULL,
_("Missing name attr in add-directory"
" element"));
svn_stringbuf_set(rb->namestr, name);
att = svn_xml_get_attr_value("copyfrom-path", atts);
if (att != NULL) {
cpath = rb->cpathstr;
svn_stringbuf_set(cpath, att);
att = svn_xml_get_attr_value("copyfrom-rev", atts);
if (att == NULL)
return svn_error_createf(SVN_ERR_RA_DAV_MALFORMED_DATA, NULL,
_("Missing copyfrom-rev attr in"
" add-directory element"));
crev = SVN_STR_TO_REV(att);
}
parent_dir = &TOP_DIR(rb);
subpool = svn_pool_create(parent_dir->pool);
pathbuf = svn_stringbuf_dup(parent_dir->pathbuf, subpool);
svn_path_add_component(pathbuf, rb->namestr->data);
SVN_ERR((*rb->editor->add_directory)(pathbuf->data, parent_dir->baton,
cpath ? cpath->data : NULL,
crev, subpool,
&new_dir_baton));
push_dir(rb, new_dir_baton, pathbuf, subpool);
TOP_DIR(rb).fetch_props = TRUE;
bc_url = svn_xml_get_attr_value("bc-url", atts);
if ((! rb->receiving_all) && bc_url) {
apr_hash_t *bc_children;
SVN_ERR(svn_ra_neon__get_props(&bc_children,
rb->ras,
bc_url,
SVN_RA_NEON__DEPTH_ONE,
NULL, NULL ,
TOP_DIR(rb).pool));
if (bc_children) {
apr_hash_index_t *hi;
TOP_DIR(rb).children = apr_hash_make(TOP_DIR(rb).pool);
for (hi = apr_hash_first(TOP_DIR(rb).pool, bc_children);
hi; hi = apr_hash_next(hi)) {
void *val;
svn_ra_neon__resource_t *rsrc;
const svn_string_t *vc_url;
apr_hash_this(hi, NULL, NULL, &val);
rsrc = val;
vc_url = apr_hash_get(rsrc->propset,
SVN_RA_NEON__PROP_CHECKED_IN,
APR_HASH_KEY_STRING);
if (vc_url)
apr_hash_set(TOP_DIR(rb).children,
vc_url->data, vc_url->len,
rsrc->propset);
}
}
}
break;
case ELEM_open_file:
att = svn_xml_get_attr_value("rev", atts);
if (att == NULL)
return svn_error_createf(SVN_ERR_RA_DAV_MALFORMED_DATA, NULL,
_("Missing rev attr in open-file"
" element"));
base = SVN_STR_TO_REV(att);
name = svn_xml_get_attr_value("name", atts);
if (name == NULL)
return svn_error_createf(SVN_ERR_RA_DAV_MALFORMED_DATA, NULL,
_("Missing name attr in open-file"
" element"));
svn_stringbuf_set(rb->namestr, name);
parent_dir = &TOP_DIR(rb);
rb->file_pool = svn_pool_create(parent_dir->pool);
rb->result_checksum = NULL;
svn_path_add_component(parent_dir->pathbuf, rb->namestr->data);
SVN_ERR((*rb->editor->open_file)(parent_dir->pathbuf->data,
parent_dir->baton, base,
rb->file_pool,
&rb->file_baton));
rb->fetch_props = FALSE;
break;
case ELEM_add_file:
name = svn_xml_get_attr_value("name", atts);
if (name == NULL)
return svn_error_createf(SVN_ERR_RA_DAV_MALFORMED_DATA, NULL,
_("Missing name attr in add-file"
" element"));
svn_stringbuf_set(rb->namestr, name);
att = svn_xml_get_attr_value("copyfrom-path", atts);
if (att != NULL) {
cpath = rb->cpathstr;
svn_stringbuf_set(cpath, att);
att = svn_xml_get_attr_value("copyfrom-rev", atts);
if (att == NULL)
return svn_error_createf(SVN_ERR_RA_DAV_MALFORMED_DATA, NULL,
_("Missing copyfrom-rev attr in add-file"
" element"));
crev = SVN_STR_TO_REV(att);
}
parent_dir = &TOP_DIR(rb);
rb->file_pool = svn_pool_create(parent_dir->pool);
rb->result_checksum = NULL;
svn_path_add_component(parent_dir->pathbuf, rb->namestr->data);
SVN_ERR((*rb->editor->add_file)(parent_dir->pathbuf->data,
parent_dir->baton,
cpath ? cpath->data : NULL,
crev, rb->file_pool,
&rb->file_baton));
rb->fetch_props = TRUE;
break;
case ELEM_txdelta:
if (! rb->receiving_all)
break;
SVN_ERR((*rb->editor->apply_textdelta)(rb->file_baton,
NULL,
rb->file_pool,
&(rb->whandler),
&(rb->whandler_baton)));
rb->svndiff_decoder = svn_txdelta_parse_svndiff(rb->whandler,
rb->whandler_baton,
TRUE, rb->file_pool);
rb->base64_decoder = svn_base64_decode(rb->svndiff_decoder,
rb->file_pool);
break;
case ELEM_set_prop: {
const char *encoding = svn_xml_get_attr_value("encoding", atts);
name = svn_xml_get_attr_value("name", atts);
if (name == NULL)
return svn_error_createf(SVN_ERR_RA_DAV_MALFORMED_DATA, NULL,
_("Missing name attr in set-prop element"));
svn_stringbuf_set(rb->namestr, name);
if (encoding)
svn_stringbuf_set(rb->encoding, encoding);
else
svn_stringbuf_setempty(rb->encoding);
}
break;
case ELEM_remove_prop:
name = svn_xml_get_attr_value("name", atts);
if (name == NULL)
return svn_error_createf(SVN_ERR_RA_DAV_MALFORMED_DATA, NULL,
_("Missing name attr in remove-prop element"));
svn_stringbuf_set(rb->namestr, name);
if (rb->file_baton == NULL)
SVN_ERR(rb->editor->change_dir_prop(TOP_DIR(rb).baton,
rb->namestr->data,
NULL, TOP_DIR(rb).pool));
else
SVN_ERR(rb->editor->change_file_prop(rb->file_baton, rb->namestr->data,
NULL, rb->file_pool));
break;
case ELEM_fetch_props:
if (!rb->fetch_content) {
svn_stringbuf_set(rb->namestr, SVN_PROP_PREFIX "BOGOSITY");
if (rb->file_baton == NULL)
SVN_ERR(rb->editor->change_dir_prop(TOP_DIR(rb).baton,
rb->namestr->data,
NULL, TOP_DIR(rb).pool));
else
SVN_ERR(rb->editor->change_file_prop(rb->file_baton,
rb->namestr->data,
NULL, rb->file_pool));
} else {
if (rb->file_baton == NULL)
TOP_DIR(rb).fetch_props = TRUE;
else
rb->fetch_props = TRUE;
}
break;
case ELEM_fetch_file:
base_checksum = svn_xml_get_attr_value("base-checksum", atts);
rb->result_checksum = NULL;
if (! rb->receiving_all) {
SVN_ERR(simple_fetch_file(rb->ras,
rb->href->data,
TOP_DIR(rb).pathbuf->data,
rb->fetch_content,
rb->file_baton,
base_checksum,
rb->editor,
rb->ras->callbacks->get_wc_prop,
rb->ras->callback_baton,
rb->file_pool));
}
break;
case ELEM_delete_entry:
name = svn_xml_get_attr_value("name", atts);
if (name == NULL)
return svn_error_createf(SVN_ERR_RA_DAV_MALFORMED_DATA, NULL,
_("Missing name attr in delete-entry"
" element"));
svn_stringbuf_set(rb->namestr, name);
parent_dir = &TOP_DIR(rb);
subpool = svn_pool_create(parent_dir->pool);
pathbuf = svn_stringbuf_dup(parent_dir->pathbuf, subpool);
svn_path_add_component(pathbuf, rb->namestr->data);
SVN_ERR((*rb->editor->delete_entry)(pathbuf->data,
SVN_INVALID_REVNUM,
TOP_DIR(rb).baton,
subpool));
svn_pool_destroy(subpool);
break;
default:
break;
}
*elem = elm->id;
return SVN_NO_ERROR;
}
static svn_error_t *
add_node_props(report_baton_t *rb, apr_pool_t *pool) {
svn_ra_neon__resource_t *rsrc = NULL;
apr_hash_t *props = NULL;
if (rb->receiving_all)
return SVN_NO_ERROR;
if (!rb->fetch_content)
return SVN_NO_ERROR;
if (rb->file_baton) {
const char *lock_token = apr_hash_get(rb->lock_tokens,
TOP_DIR(rb).pathbuf->data,
TOP_DIR(rb).pathbuf->len);
if (lock_token) {
svn_lock_t *lock;
SVN_ERR(svn_ra_neon__get_lock_internal(rb->ras, &lock,
TOP_DIR(rb).pathbuf->data,
pool));
if (! (lock
&& lock->token
&& (strcmp(lock->token, lock_token) == 0)))
SVN_ERR(rb->editor->change_file_prop(rb->file_baton,
SVN_PROP_ENTRY_LOCK_TOKEN,
NULL, pool));
}
if (! rb->fetch_props)
return SVN_NO_ERROR;
if ( ! ((TOP_DIR(rb).children)
&& (props = apr_hash_get(TOP_DIR(rb).children, rb->href->data,
APR_HASH_KEY_STRING))) ) {
SVN_ERR(svn_ra_neon__get_props_resource(&rsrc,
rb->ras,
rb->href->data,
NULL,
NULL,
pool));
props = rsrc->propset;
}
SVN_ERR(add_props(props,
rb->editor->change_file_prop,
rb->file_baton,
pool));
} else {
if (! TOP_DIR(rb).fetch_props)
return SVN_NO_ERROR;
if ( ! ((TOP_DIR(rb).children)
&& (props = apr_hash_get(TOP_DIR(rb).children,
TOP_DIR(rb).vsn_url,
APR_HASH_KEY_STRING))) ) {
SVN_ERR(svn_ra_neon__get_props_resource(&rsrc,
rb->ras,
TOP_DIR(rb).vsn_url,
NULL,
NULL,
pool));
props = rsrc->propset;
}
SVN_ERR(add_props(props,
rb->editor->change_dir_prop,
TOP_DIR(rb).baton,
pool));
}
return SVN_NO_ERROR;
}
static svn_error_t *
cdata_handler(void *userdata, int state, const char *cdata, size_t len) {
report_baton_t *rb = userdata;
switch(state) {
case ELEM_href:
case ELEM_set_prop:
case ELEM_md5_checksum:
case ELEM_version_name:
case ELEM_creationdate:
case ELEM_creator_displayname:
svn_stringbuf_appendbytes(rb->cdata_accum, cdata, len);
break;
case ELEM_txdelta: {
apr_size_t nlen = len;
if (! rb->receiving_all)
break;
SVN_ERR(svn_stream_write(rb->base64_decoder, cdata, &nlen));
if (nlen != len) {
return svn_error_createf(SVN_ERR_STREAM_UNEXPECTED_EOF, NULL,
_("Error writing to '%s': unexpected EOF"),
svn_path_local_style(rb->namestr->data,
rb->pool));
}
}
break;
}
return 0;
}
static svn_error_t *
end_element(void *userdata, int state,
const char *nspace, const char *elt_name) {
report_baton_t *rb = userdata;
const svn_delta_editor_t *editor = rb->editor;
const svn_ra_neon__xml_elm_t *elm;
elm = svn_ra_neon__lookup_xml_elem(report_elements, nspace, elt_name);
if (elm == NULL)
return SVN_NO_ERROR;
switch (elm->id) {
case ELEM_resource:
rb->in_resource = FALSE;
break;
case ELEM_update_report:
SVN_ERR((*rb->editor->close_edit)(rb->edit_baton, rb->pool));
rb->edit_baton = NULL;
break;
case ELEM_add_directory:
case ELEM_open_directory:
if (! ((DIR_DEPTH(rb) == 1) && *rb->target))
SVN_ERR(add_node_props(rb, TOP_DIR(rb).pool));
SVN_ERR((*rb->editor->close_directory)(TOP_DIR(rb).baton,
TOP_DIR(rb).pool));
svn_pool_destroy(TOP_DIR(rb).pool);
apr_array_pop(rb->dirs);
break;
case ELEM_add_file:
if (! rb->receiving_all) {
SVN_ERR(simple_fetch_file(rb->ras,
rb->href->data,
TOP_DIR(rb).pathbuf->data,
rb->fetch_content,
rb->file_baton,
NULL,
rb->editor,
rb->ras->callbacks->get_wc_prop,
rb->ras->callback_baton,
rb->file_pool));
SVN_ERR(add_node_props(rb, rb->file_pool));
}
SVN_ERR((*rb->editor->close_file)(rb->file_baton,
rb->result_checksum,
rb->file_pool));
rb->file_baton = NULL;
svn_path_remove_component(TOP_DIR(rb).pathbuf);
svn_pool_destroy(rb->file_pool);
rb->file_pool = NULL;
break;
case ELEM_txdelta:
if (! rb->receiving_all)
break;
SVN_ERR(svn_stream_close(rb->base64_decoder));
rb->whandler = NULL;
rb->whandler_baton = NULL;
rb->svndiff_decoder = NULL;
rb->base64_decoder = NULL;
break;
case ELEM_open_file:
SVN_ERR(add_node_props(rb, rb->file_pool));
SVN_ERR((*rb->editor->close_file)(rb->file_baton,
rb->result_checksum,
rb->file_pool));
rb->file_baton = NULL;
svn_path_remove_component(TOP_DIR(rb).pathbuf);
svn_pool_destroy(rb->file_pool);
rb->file_pool = NULL;
break;
case ELEM_set_prop: {
svn_string_t decoded_value;
const svn_string_t *decoded_value_p;
apr_pool_t *pool;
if (rb->file_baton)
pool = rb->file_pool;
else
pool = TOP_DIR(rb).pool;
decoded_value.data = rb->cdata_accum->data;
decoded_value.len = rb->cdata_accum->len;
if (svn_stringbuf_isempty(rb->encoding)) {
decoded_value_p = &decoded_value;
} else if (strcmp(rb->encoding->data, "base64") == 0) {
decoded_value_p = svn_base64_decode_string(&decoded_value, pool);
svn_stringbuf_setempty(rb->encoding);
} else {
SVN_ERR(svn_error_createf(SVN_ERR_XML_UNKNOWN_ENCODING, NULL,
_("Unknown XML encoding: '%s'"),
rb->encoding->data));
abort();
}
if (rb->file_baton) {
SVN_ERR(rb->editor->change_file_prop(rb->file_baton,
rb->namestr->data,
decoded_value_p, pool));
} else {
SVN_ERR(rb->editor->change_dir_prop(TOP_DIR(rb).baton,
rb->namestr->data,
decoded_value_p, pool));
}
}
svn_stringbuf_setempty(rb->cdata_accum);
break;
case ELEM_href:
if (rb->fetch_content)
SVN_ERR(svn_ra_neon__copy_href(rb->href, rb->cdata_accum->data,
rb->scratch_pool));
svn_stringbuf_setempty(rb->cdata_accum);
if (!rb->fetch_content)
break;
if (rb->in_resource) {
svn_string_t href_val;
href_val.data = rb->href->data;
href_val.len = rb->href->len;
if (rb->ras->callbacks->set_wc_prop != NULL)
SVN_ERR(rb->ras->callbacks->set_wc_prop
(rb->ras->callback_baton,
rb->current_wcprop_path->data,
SVN_RA_NEON__LP_VSN_URL,
&href_val,
rb->scratch_pool));
svn_pool_clear(rb->scratch_pool);
}
else if (rb->file_baton == NULL) {
if (! ((DIR_DEPTH(rb) == 1) && *rb->target)) {
SVN_ERR(simple_store_vsn_url(rb->href->data, TOP_DIR(rb).baton,
rb->editor->change_dir_prop,
TOP_DIR(rb).pool));
TOP_DIR(rb).vsn_url = apr_pmemdup(TOP_DIR(rb).pool,
rb->href->data,
rb->href->len + 1);
}
} else {
SVN_ERR(simple_store_vsn_url(rb->href->data, rb->file_baton,
rb->editor->change_file_prop,
rb->file_pool));
}
break;
case ELEM_md5_checksum:
if (rb->file_baton) {
rb->result_checksum = apr_pstrdup(rb->file_pool,
rb->cdata_accum->data);
}
svn_stringbuf_setempty(rb->cdata_accum);
break;
case ELEM_version_name:
case ELEM_creationdate:
case ELEM_creator_displayname: {
apr_pool_t *pool =
rb->file_baton ? rb->file_pool : TOP_DIR(rb).pool;
prop_setter_t setter =
rb->file_baton ? editor->change_file_prop : editor->change_dir_prop;
const char *name = apr_pstrcat(pool, elm->nspace, elm->name, NULL);
void *baton = rb->file_baton ? rb->file_baton : TOP_DIR(rb).baton;
svn_string_t valstr;
valstr.data = rb->cdata_accum->data;
valstr.len = rb->cdata_accum->len;
SVN_ERR(set_special_wc_prop(name, &valstr, setter, baton, pool));
svn_stringbuf_setempty(rb->cdata_accum);
}
break;
default:
break;
}
return SVN_NO_ERROR;
}
static svn_error_t * reporter_set_path(void *report_baton,
const char *path,
svn_revnum_t revision,
svn_depth_t depth,
svn_boolean_t start_empty,
const char *lock_token,
apr_pool_t *pool) {
report_baton_t *rb = report_baton;
const char *entry;
svn_stringbuf_t *qpath = NULL;
const char *tokenstring = "";
const char *depthstring = apr_psprintf(pool, "depth=\"%s\"",
svn_depth_to_word(depth));
if (lock_token) {
tokenstring = apr_psprintf(pool, "lock-token=\"%s\"", lock_token);
apr_hash_set(rb->lock_tokens,
apr_pstrdup(apr_hash_pool_get(rb->lock_tokens), path),
APR_HASH_KEY_STRING,
apr_pstrdup(apr_hash_pool_get(rb->lock_tokens), lock_token));
}
svn_xml_escape_cdata_cstring(&qpath, path, pool);
if (start_empty)
entry = apr_psprintf(pool,
"<S:entry rev=\"%ld\" %s %s"
" start-empty=\"true\">%s</S:entry>" DEBUG_CR,
revision, depthstring, tokenstring, qpath->data);
else
entry = apr_psprintf(pool,
"<S:entry rev=\"%ld\" %s %s>"
"%s</S:entry>" DEBUG_CR,
revision, depthstring, tokenstring, qpath->data);
return svn_io_file_write_full(rb->tmpfile, entry, strlen(entry), NULL, pool);
}
static svn_error_t * reporter_link_path(void *report_baton,
const char *path,
const char *url,
svn_revnum_t revision,
svn_depth_t depth,
svn_boolean_t start_empty,
const char *lock_token,
apr_pool_t *pool) {
report_baton_t *rb = report_baton;
const char *entry;
svn_stringbuf_t *qpath = NULL, *qlinkpath = NULL;
svn_string_t bc_relative;
const char *tokenstring = "";
const char *depthstring = apr_psprintf(pool, "depth=\"%s\"",
svn_depth_to_word(depth));
if (lock_token) {
tokenstring = apr_psprintf(pool, "lock-token=\"%s\"", lock_token);
apr_hash_set(rb->lock_tokens,
apr_pstrdup(apr_hash_pool_get(rb->lock_tokens), path),
APR_HASH_KEY_STRING,
apr_pstrdup(apr_hash_pool_get(rb->lock_tokens), lock_token));
}
SVN_ERR(svn_ra_neon__get_baseline_info(NULL, NULL, &bc_relative, NULL,
rb->ras,
url, revision,
pool));
svn_xml_escape_cdata_cstring(&qpath, path, pool);
svn_xml_escape_attr_cstring(&qlinkpath, bc_relative.data, pool);
if (start_empty)
entry = apr_psprintf(pool,
"<S:entry rev=\"%ld\" %s %s"
" linkpath=\"/%s\" start-empty=\"true\""
">%s</S:entry>" DEBUG_CR,
revision, depthstring, tokenstring,
qlinkpath->data, qpath->data);
else
entry = apr_psprintf(pool,
"<S:entry rev=\"%ld\" %s %s"
" linkpath=\"/%s\">%s</S:entry>" DEBUG_CR,
revision, depthstring, tokenstring,
qlinkpath->data, qpath->data);
return svn_io_file_write_full(rb->tmpfile, entry, strlen(entry), NULL, pool);
}
static svn_error_t * reporter_delete_path(void *report_baton,
const char *path,
apr_pool_t *pool) {
report_baton_t *rb = report_baton;
const char *s;
svn_stringbuf_t *qpath = NULL;
svn_xml_escape_cdata_cstring(&qpath, path, pool);
s = apr_psprintf(pool,
"<S:missing>%s</S:missing>" DEBUG_CR,
qpath->data);
return svn_io_file_write_full(rb->tmpfile, s, strlen(s), NULL, pool);
}
static svn_error_t * reporter_abort_report(void *report_baton,
apr_pool_t *pool) {
report_baton_t *rb = report_baton;
(void) apr_file_close(rb->tmpfile);
return SVN_NO_ERROR;
}
static svn_error_t * reporter_finish_report(void *report_baton,
apr_pool_t *pool) {
report_baton_t *rb = report_baton;
svn_error_t *err;
const char *vcc;
apr_hash_t *request_headers = apr_hash_make(pool);
apr_hash_set(request_headers, "Accept-Encoding", APR_HASH_KEY_STRING,
"svndiff1;q=0.9,svndiff;q=0.8");
#define SVN_RA_NEON__REPORT_TAIL "</S:update-report>" DEBUG_CR
SVN_ERR(svn_io_file_write_full(rb->tmpfile,
SVN_RA_NEON__REPORT_TAIL,
sizeof(SVN_RA_NEON__REPORT_TAIL) - 1,
NULL, pool));
#undef SVN_RA_NEON__REPORT_TAIL
rb->dirs = apr_array_make(rb->pool, 5, sizeof(dir_item_t));
rb->namestr = MAKE_BUFFER(rb->pool);
rb->cpathstr = MAKE_BUFFER(rb->pool);
rb->encoding = MAKE_BUFFER(rb->pool);
rb->href = MAKE_BUFFER(rb->pool);
if ((err = svn_ra_neon__get_vcc(&vcc, rb->ras,
rb->ras->url->data, pool))) {
(void) apr_file_close(rb->tmpfile);
return err;
}
err = svn_ra_neon__parsed_request(rb->ras, "REPORT", vcc,
NULL, rb->tmpfile, NULL,
start_element,
cdata_handler,
end_element,
rb,
request_headers, NULL,
rb->spool_response, pool);
(void) apr_file_close(rb->tmpfile);
SVN_ERR(err);
if (rb->edit_baton) {
return svn_error_createf
(SVN_ERR_RA_DAV_REQUEST_FAILED, NULL,
_("REPORT response handling failed to complete the editor drive"));
}
SVN_ERR(svn_ra_neon__maybe_store_auth_info(rb->ras, pool));
return SVN_NO_ERROR;
}
static const svn_ra_reporter3_t ra_neon_reporter = {
reporter_set_path,
reporter_delete_path,
reporter_link_path,
reporter_finish_report,
reporter_abort_report
};
static svn_error_t *
make_reporter(svn_ra_session_t *session,
const svn_ra_reporter3_t **reporter,
void **report_baton,
svn_revnum_t revision,
const char *target,
const char *dst_path,
svn_depth_t depth,
svn_boolean_t send_copyfrom_args,
svn_boolean_t ignore_ancestry,
svn_boolean_t resource_walk,
const svn_delta_editor_t *editor,
void *edit_baton,
svn_boolean_t fetch_content,
svn_boolean_t send_all,
svn_boolean_t spool_response,
apr_pool_t *pool) {
svn_ra_neon__session_t *ras = session->priv;
report_baton_t *rb;
const char *s;
svn_stringbuf_t *xml_s;
const svn_delta_editor_t *filter_editor;
void *filter_baton;
svn_boolean_t has_target = *target ? TRUE : FALSE;
svn_boolean_t server_supports_depth;
SVN_ERR(svn_ra_neon__has_capability(session, &server_supports_depth,
SVN_RA_CAPABILITY_DEPTH, pool));
if ((depth != svn_depth_files)
&& (depth != svn_depth_infinity)
&& ! server_supports_depth) {
SVN_ERR(svn_delta_depth_filter_editor(&filter_editor,
&filter_baton,
editor,
edit_baton,
depth,
has_target,
pool));
editor = filter_editor;
edit_baton = filter_baton;
}
rb = apr_pcalloc(pool, sizeof(*rb));
rb->ras = ras;
rb->pool = pool;
rb->scratch_pool = svn_pool_create(pool);
rb->editor = editor;
rb->edit_baton = edit_baton;
rb->fetch_content = fetch_content;
rb->in_resource = FALSE;
rb->current_wcprop_path = svn_stringbuf_create("", pool);
rb->is_switch = dst_path ? TRUE : FALSE;
rb->target = target;
rb->receiving_all = FALSE;
rb->spool_response = spool_response;
rb->whandler = NULL;
rb->whandler_baton = NULL;
rb->svndiff_decoder = NULL;
rb->base64_decoder = NULL;
rb->cdata_accum = svn_stringbuf_create("", pool);
rb->send_copyfrom_args = send_copyfrom_args;
rb->lock_tokens = apr_hash_make(pool);
SVN_ERR(ras->callbacks->open_tmp_file(&rb->tmpfile, ras->callback_baton,
pool));
s = apr_psprintf(pool, "<S:update-report send-all=\"%s\" xmlns:S=\""
SVN_XML_NAMESPACE "\">" DEBUG_CR,
send_all ? "true" : "false");
SVN_ERR(svn_io_file_write_full(rb->tmpfile, s, strlen(s), NULL, pool));
xml_s = NULL;
svn_xml_escape_cdata_cstring(&xml_s, ras->url->data, pool);
s = apr_psprintf(pool, "<S:src-path>%s</S:src-path>" DEBUG_CR, xml_s->data);
SVN_ERR(svn_io_file_write_full(rb->tmpfile, s, strlen(s), NULL, pool));
if (SVN_IS_VALID_REVNUM(revision)) {
s = apr_psprintf(pool,
"<S:target-revision>%ld</S:target-revision>" DEBUG_CR,
revision);
SVN_ERR(svn_io_file_write_full(rb->tmpfile, s, strlen(s), NULL, pool));
}
if (*target) {
xml_s = NULL;
svn_xml_escape_cdata_cstring(&xml_s, target, pool);
s = apr_psprintf(pool, "<S:update-target>%s</S:update-target>" DEBUG_CR,
xml_s->data);
SVN_ERR(svn_io_file_write_full(rb->tmpfile, s, strlen(s), NULL, pool));
}
if (dst_path) {
xml_s = NULL;
svn_xml_escape_cdata_cstring(&xml_s, dst_path, pool);
s = apr_psprintf(pool, "<S:dst-path>%s</S:dst-path>" DEBUG_CR,
xml_s->data);
SVN_ERR(svn_io_file_write_full(rb->tmpfile, s, strlen(s), NULL, pool));
}
if (depth == svn_depth_files || depth == svn_depth_empty) {
const char *data = "<S:recursive>no</S:recursive>" DEBUG_CR;
SVN_ERR(svn_io_file_write_full(rb->tmpfile, data, strlen(data),
NULL, pool));
}
{
s = apr_psprintf(pool, "<S:depth>%s</S:depth>" DEBUG_CR,
svn_depth_to_word(depth));
SVN_ERR(svn_io_file_write_full(rb->tmpfile, s, strlen(s), NULL, pool));
}
if (ignore_ancestry) {
const char *data = "<S:ignore-ancestry>yes</S:ignore-ancestry>" DEBUG_CR;
SVN_ERR(svn_io_file_write_full(rb->tmpfile, data, strlen(data),
NULL, pool));
}
if (send_copyfrom_args) {
const char *data =
"<S:send-copyfrom-args>yes</S:send-copyfrom-args>" DEBUG_CR;
SVN_ERR(svn_io_file_write_full(rb->tmpfile, data, strlen(data),
NULL, pool));
}
if (resource_walk) {
const char *data = "<S:resource-walk>yes</S:resource-walk>" DEBUG_CR;
SVN_ERR(svn_io_file_write_full(rb->tmpfile, data, strlen(data),
NULL, pool));
}
if (send_all && (! fetch_content)) {
const char *data = "<S:text-deltas>no</S:text-deltas>" DEBUG_CR;
SVN_ERR(svn_io_file_write_full(rb->tmpfile, data, strlen(data),
NULL, pool));
}
*reporter = &ra_neon_reporter;
*report_baton = rb;
return SVN_NO_ERROR;
}
svn_error_t * svn_ra_neon__do_update(svn_ra_session_t *session,
const svn_ra_reporter3_t **reporter,
void **report_baton,
svn_revnum_t revision_to_update_to,
const char *update_target,
svn_depth_t depth,
svn_boolean_t send_copyfrom_args,
const svn_delta_editor_t *wc_update,
void *wc_update_baton,
apr_pool_t *pool) {
return make_reporter(session,
reporter,
report_baton,
revision_to_update_to,
update_target,
NULL,
depth,
send_copyfrom_args,
FALSE,
FALSE,
wc_update,
wc_update_baton,
TRUE,
TRUE,
FALSE,
pool);
}
svn_error_t * svn_ra_neon__do_status(svn_ra_session_t *session,
const svn_ra_reporter3_t **reporter,
void **report_baton,
const char *status_target,
svn_revnum_t revision,
svn_depth_t depth,
const svn_delta_editor_t *wc_status,
void *wc_status_baton,
apr_pool_t *pool) {
return make_reporter(session,
reporter,
report_baton,
revision,
status_target,
NULL,
depth,
FALSE,
FALSE,
FALSE,
wc_status,
wc_status_baton,
FALSE,
TRUE,
FALSE,
pool);
}
svn_error_t * svn_ra_neon__do_switch(svn_ra_session_t *session,
const svn_ra_reporter3_t **reporter,
void **report_baton,
svn_revnum_t revision_to_update_to,
const char *update_target,
svn_depth_t depth,
const char *switch_url,
const svn_delta_editor_t *wc_update,
void *wc_update_baton,
apr_pool_t *pool) {
return make_reporter(session,
reporter,
report_baton,
revision_to_update_to,
update_target,
switch_url,
depth,
FALSE,
TRUE,
FALSE,
wc_update,
wc_update_baton,
TRUE,
TRUE,
FALSE,
pool);
}
svn_error_t * svn_ra_neon__do_diff(svn_ra_session_t *session,
const svn_ra_reporter3_t **reporter,
void **report_baton,
svn_revnum_t revision,
const char *diff_target,
svn_depth_t depth,
svn_boolean_t ignore_ancestry,
svn_boolean_t text_deltas,
const char *versus_url,
const svn_delta_editor_t *wc_diff,
void *wc_diff_baton,
apr_pool_t *pool) {
return make_reporter(session,
reporter,
report_baton,
revision,
diff_target,
versus_url,
depth,
FALSE,
ignore_ancestry,
FALSE,
wc_diff,
wc_diff_baton,
text_deltas,
FALSE,
TRUE,
pool);
}
