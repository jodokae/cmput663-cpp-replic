#include <apr_pools.h>
#include <apr_strings.h>
#include <apr_xml.h>
#include <apr_md5.h>
#include <http_request.h>
#include <http_log.h>
#include <mod_dav.h>
#include "svn_pools.h"
#include "svn_repos.h"
#include "svn_fs.h"
#include "svn_md5.h"
#include "svn_base64.h"
#include "svn_xml.h"
#include "svn_path.h"
#include "svn_dav.h"
#include "svn_props.h"
#include "../dav_svn.h"
typedef struct {
const dav_resource *resource;
svn_fs_root_t *rev_root;
const char *anchor;
const char *target;
const char *dst_path;
apr_bucket_brigade *bb;
ap_filter_t *output;
apr_hash_t *pathmap;
svn_boolean_t resource_walk;
svn_boolean_t started_update;
svn_boolean_t send_all;
int svndiff_version;
} update_ctx_t;
typedef struct item_baton_t {
apr_pool_t *pool;
update_ctx_t *uc;
struct item_baton_t *parent;
const char *name;
const char *path;
const char *path2;
const char *path3;
const char *base_checksum;
const char *text_checksum;
svn_boolean_t text_changed;
svn_boolean_t added;
svn_boolean_t copyfrom;
apr_array_header_t *changed_props;
apr_array_header_t *removed_props;
const char *committed_rev;
const char *committed_date;
const char *last_author;
} item_baton_t;
#define DIR_OR_FILE(is_dir) ((is_dir) ? "directory" : "file")
static void
add_to_path_map(apr_hash_t *hash, const char *path, const char *linkpath) {
const char *norm_path = strcmp(path, "") ? path : "/";
const char *repos_path = linkpath ? linkpath : norm_path;
apr_hash_set(hash, path, APR_HASH_KEY_STRING, repos_path);
}
static const char *
get_from_path_map(apr_hash_t *hash, const char *path, apr_pool_t *pool) {
const char *repos_path;
svn_stringbuf_t *my_path;
if (! hash)
return apr_pstrdup(pool, path);
if ((repos_path = apr_hash_get(hash, path, APR_HASH_KEY_STRING))) {
return apr_pstrdup(pool, repos_path);
}
my_path = svn_stringbuf_create(path, pool);
do {
svn_path_remove_component(my_path);
if ((repos_path = apr_hash_get(hash, my_path->data, my_path->len))) {
return svn_path_join(repos_path, path + my_path->len + 1, pool);
}
} while (! svn_path_is_empty(my_path->data)
&& strcmp(my_path->data, "/") != 0);
return apr_pstrdup(pool, path);
}
static item_baton_t *
make_child_baton(item_baton_t *parent, const char *path, apr_pool_t *pool) {
item_baton_t *baton;
baton = apr_pcalloc(pool, sizeof(*baton));
baton->pool = pool;
baton->uc = parent->uc;
baton->name = svn_path_basename(path, pool);
baton->parent = parent;
baton->path = svn_path_join(parent->path, baton->name, pool);
baton->path2 = svn_path_join(parent->path2, baton->name, pool);
if ((*baton->uc->target) && (! parent->parent))
baton->path3 = svn_path_join(parent->path3, baton->uc->target, pool);
else
baton->path3 = svn_path_join(parent->path3, baton->name, pool);
return baton;
}
static const char *
get_real_fs_path(item_baton_t *baton, apr_pool_t *pool) {
const char *path = get_from_path_map(baton->uc->pathmap, baton->path, pool);
return strcmp(path, baton->path) ? path : baton->path2;
}
static svn_error_t *
send_vsn_url(item_baton_t *baton, apr_pool_t *pool) {
const char *href;
const char *path;
svn_revnum_t revision;
path = get_real_fs_path(baton, pool);
revision = dav_svn__get_safe_cr(baton->uc->rev_root, path, pool);
href = dav_svn__build_uri(baton->uc->resource->info->repos,
DAV_SVN__BUILD_URI_VERSION,
revision, path, 0 , pool);
return dav_svn__send_xml(baton->uc->bb, baton->uc->output,
"<D:checked-in><D:href>%s</D:href></D:checked-in>"
DEBUG_CR, apr_xml_quote_string(pool, href, 1));
}
static svn_error_t *
absent_helper(svn_boolean_t is_dir,
const char *path,
item_baton_t *parent,
apr_pool_t *pool) {
update_ctx_t *uc = parent->uc;
if (! uc->resource_walk) {
const char *elt = apr_psprintf(pool,
"<S:absent-%s name=\"%s\"/>" DEBUG_CR,
DIR_OR_FILE(is_dir),
apr_xml_quote_string
(pool,
svn_path_basename(path, pool),
1));
SVN_ERR(dav_svn__send_xml(uc->bb, uc->output, "%s", elt));
}
return SVN_NO_ERROR;
}
static svn_error_t *
upd_absent_directory(const char *path, void *parent_baton, apr_pool_t *pool) {
return absent_helper(TRUE, path, parent_baton, pool);
}
static svn_error_t *
upd_absent_file(const char *path, void *parent_baton, apr_pool_t *pool) {
return absent_helper(FALSE, path, parent_baton, pool);
}
static svn_error_t *
add_helper(svn_boolean_t is_dir,
const char *path,
item_baton_t *parent,
const char *copyfrom_path,
svn_revnum_t copyfrom_revision,
apr_pool_t *pool,
void **child_baton) {
item_baton_t *child;
update_ctx_t *uc = parent->uc;
const char *bc_url = NULL;
child = make_child_baton(parent, path, pool);
child->added = TRUE;
if (uc->resource_walk) {
SVN_ERR(dav_svn__send_xml(child->uc->bb, child->uc->output,
"<S:resource path=\"%s\">" DEBUG_CR,
apr_xml_quote_string(pool, child->path3, 1)));
} else {
const char *qname = apr_xml_quote_string(pool, child->name, 1);
const char *elt;
const char *real_path = get_real_fs_path(child, pool);
if (! is_dir) {
unsigned char digest[APR_MD5_DIGESTSIZE];
SVN_ERR(svn_fs_file_md5_checksum
(digest, uc->rev_root, real_path, pool));
child->text_checksum = svn_md5_digest_to_cstring(digest, pool);
} else {
svn_revnum_t revision;
revision = dav_svn__get_safe_cr(child->uc->rev_root, real_path,
pool);
bc_url = dav_svn__build_uri(child->uc->resource->info->repos,
DAV_SVN__BUILD_URI_BC,
revision, real_path,
0 , pool);
if (real_path && (! svn_path_is_empty(real_path)))
bc_url = svn_path_url_add_component(bc_url, real_path+1, pool);
bc_url = apr_xml_quote_string(pool, bc_url, 1);
}
if (copyfrom_path == NULL) {
if (bc_url)
elt = apr_psprintf(pool, "<S:add-%s name=\"%s\" "
"bc-url=\"%s\">" DEBUG_CR,
DIR_OR_FILE(is_dir), qname, bc_url);
else
elt = apr_psprintf(pool, "<S:add-%s name=\"%s\">" DEBUG_CR,
DIR_OR_FILE(is_dir), qname);
} else {
const char *qcopy = apr_xml_quote_string(pool, copyfrom_path, 1);
if (bc_url)
elt = apr_psprintf(pool, "<S:add-%s name=\"%s\" "
"copyfrom-path=\"%s\" copyfrom-rev=\"%ld\" "
"bc-url=\"%s\">" DEBUG_CR,
DIR_OR_FILE(is_dir),
qname, qcopy, copyfrom_revision,
bc_url);
else
elt = apr_psprintf(pool, "<S:add-%s name=\"%s\" "
"copyfrom-path=\"%s\""
" copyfrom-rev=\"%ld\">" DEBUG_CR,
DIR_OR_FILE(is_dir),
qname, qcopy, copyfrom_revision);
child->copyfrom = TRUE;
}
SVN_ERR(dav_svn__send_xml(child->uc->bb, child->uc->output, "%s", elt));
}
SVN_ERR(send_vsn_url(child, pool));
if (uc->resource_walk)
SVN_ERR(dav_svn__send_xml(child->uc->bb, child->uc->output,
"</S:resource>" DEBUG_CR));
*child_baton = child;
return SVN_NO_ERROR;
}
static svn_error_t *
open_helper(svn_boolean_t is_dir,
const char *path,
item_baton_t *parent,
svn_revnum_t base_revision,
apr_pool_t *pool,
void **child_baton) {
item_baton_t *child = make_child_baton(parent, path, pool);
const char *qname = apr_xml_quote_string(pool, child->name, 1);
SVN_ERR(dav_svn__send_xml(child->uc->bb, child->uc->output,
"<S:open-%s name=\"%s\""
" rev=\"%ld\">" DEBUG_CR,
DIR_OR_FILE(is_dir), qname, base_revision));
SVN_ERR(send_vsn_url(child, pool));
*child_baton = child;
return SVN_NO_ERROR;
}
static svn_error_t *
close_helper(svn_boolean_t is_dir, item_baton_t *baton) {
int i;
if (baton->uc->resource_walk)
return SVN_NO_ERROR;
if (baton->removed_props && (! baton->added || baton->copyfrom)) {
const char *qname;
for (i = 0; i < baton->removed_props->nelts; i++) {
qname = APR_ARRAY_IDX(baton->removed_props, i, const char *);
SVN_ERR(dav_svn__send_xml(baton->uc->bb, baton->uc->output,
"<S:remove-prop name=\"%s\"/>"
DEBUG_CR, qname));
}
}
if ((! baton->uc->send_all) && baton->changed_props && (! baton->added)) {
SVN_ERR(dav_svn__send_xml(baton->uc->bb, baton->uc->output,
"<S:fetch-props/>" DEBUG_CR));
}
SVN_ERR(dav_svn__send_xml(baton->uc->bb, baton->uc->output, "<S:prop>"));
if (baton->text_checksum) {
SVN_ERR(dav_svn__send_xml(baton->uc->bb, baton->uc->output,
"<V:md5-checksum>%s</V:md5-checksum>",
baton->text_checksum));
}
if (! baton->uc->send_all) {
if (baton->committed_rev)
SVN_ERR(dav_svn__send_xml(baton->uc->bb, baton->uc->output,
"<D:version-name>%s</D:version-name>",
baton->committed_rev));
if (baton->committed_date)
SVN_ERR(dav_svn__send_xml(baton->uc->bb, baton->uc->output,
"<D:creationdate>%s</D:creationdate>",
baton->committed_date));
if (baton->last_author)
SVN_ERR(dav_svn__send_xml(baton->uc->bb, baton->uc->output,
"<D:creator-displayname>%s"
"</D:creator-displayname>",
apr_xml_quote_string(baton->pool,
baton->last_author,
1)));
}
SVN_ERR(dav_svn__send_xml(baton->uc->bb, baton->uc->output, "</S:prop>\n"));
if (baton->added)
SVN_ERR(dav_svn__send_xml(baton->uc->bb, baton->uc->output,
"</S:add-%s>" DEBUG_CR,
DIR_OR_FILE(is_dir)));
else
SVN_ERR(dav_svn__send_xml(baton->uc->bb, baton->uc->output,
"</S:open-%s>" DEBUG_CR,
DIR_OR_FILE(is_dir)));
return SVN_NO_ERROR;
}
static svn_error_t *
maybe_start_update_report(update_ctx_t *uc) {
if ((! uc->resource_walk) && (! uc->started_update)) {
SVN_ERR(dav_svn__send_xml(uc->bb, uc->output,
DAV_XML_HEADER DEBUG_CR
"<S:update-report xmlns:S=\""
SVN_XML_NAMESPACE "\" "
"xmlns:V=\"" SVN_DAV_PROP_NS_DAV "\" "
"xmlns:D=\"DAV:\" %s>" DEBUG_CR,
uc->send_all ? "send-all=\"true\"" : ""));
uc->started_update = TRUE;
}
return SVN_NO_ERROR;
}
static svn_error_t *
upd_set_target_revision(void *edit_baton,
svn_revnum_t target_revision,
apr_pool_t *pool) {
update_ctx_t *uc = edit_baton;
SVN_ERR(maybe_start_update_report(uc));
if (! uc->resource_walk)
SVN_ERR(dav_svn__send_xml(uc->bb, uc->output,
"<S:target-revision rev=\"%ld"
"\"/>" DEBUG_CR, target_revision));
return SVN_NO_ERROR;
}
static svn_error_t *
upd_open_root(void *edit_baton,
svn_revnum_t base_revision,
apr_pool_t *pool,
void **root_baton) {
update_ctx_t *uc = edit_baton;
item_baton_t *b = apr_pcalloc(pool, sizeof(*b));
b->uc = uc;
b->pool = pool;
b->path = uc->anchor;
b->path2 = uc->dst_path;
b->path3 = "";
*root_baton = b;
SVN_ERR(maybe_start_update_report(uc));
if (uc->resource_walk) {
const char *qpath = apr_xml_quote_string(pool, b->path3, 1);
SVN_ERR(dav_svn__send_xml(uc->bb, uc->output,
"<S:resource path=\"%s\">" DEBUG_CR, qpath));
} else {
SVN_ERR(dav_svn__send_xml(uc->bb, uc->output,
"<S:open-directory rev=\"%ld\">"
DEBUG_CR, base_revision));
}
if (! *uc->target)
SVN_ERR(send_vsn_url(b, pool));
if (uc->resource_walk)
SVN_ERR(dav_svn__send_xml(uc->bb, uc->output,
"</S:resource>" DEBUG_CR));
return SVN_NO_ERROR;
}
static svn_error_t *
upd_delete_entry(const char *path,
svn_revnum_t revision,
void *parent_baton,
apr_pool_t *pool) {
item_baton_t *parent = parent_baton;
const char *qname = apr_xml_quote_string(pool,
svn_path_basename(path, pool), 1);
return dav_svn__send_xml(parent->uc->bb, parent->uc->output,
"<S:delete-entry name=\"%s\"/>" DEBUG_CR, qname);
}
static svn_error_t *
upd_add_directory(const char *path,
void *parent_baton,
const char *copyfrom_path,
svn_revnum_t copyfrom_revision,
apr_pool_t *pool,
void **child_baton) {
return add_helper(TRUE ,
path, parent_baton, copyfrom_path, copyfrom_revision, pool,
child_baton);
}
static svn_error_t *
upd_open_directory(const char *path,
void *parent_baton,
svn_revnum_t base_revision,
apr_pool_t *pool,
void **child_baton) {
return open_helper(TRUE ,
path, parent_baton, base_revision, pool, child_baton);
}
static svn_error_t *
upd_change_xxx_prop(void *baton,
const char *name,
const svn_string_t *value,
apr_pool_t *pool) {
item_baton_t *b = baton;
const char *qname;
if (b->uc->resource_walk)
return SVN_NO_ERROR;
qname = apr_xml_quote_string(b->pool, name, 1);
if (qname == name)
qname = apr_pstrdup(b->pool, name);
if (b->uc->send_all) {
if (value) {
const char *qval;
if (svn_xml_is_xml_safe(value->data, value->len)) {
svn_stringbuf_t *tmp = NULL;
svn_xml_escape_cdata_string(&tmp, value, pool);
qval = tmp->data;
SVN_ERR(dav_svn__send_xml(b->uc->bb, b->uc->output,
"<S:set-prop name=\"%s\">", qname));
} else {
qval = svn_base64_encode_string(value, pool)->data;
SVN_ERR(dav_svn__send_xml(b->uc->bb, b->uc->output,
"<S:set-prop name=\"%s\" "
"encoding=\"base64\">" DEBUG_CR,
qname));
}
SVN_ERR(dav_svn__send_xml(b->uc->bb, b->uc->output, "%s", qval));
SVN_ERR(dav_svn__send_xml(b->uc->bb, b->uc->output,
"</S:set-prop>" DEBUG_CR));
} else {
SVN_ERR(dav_svn__send_xml(b->uc->bb, b->uc->output,
"<S:remove-prop name=\"%s\"/>" DEBUG_CR,
qname));
}
} else {
#define NSLEN (sizeof(SVN_PROP_ENTRY_PREFIX) - 1)
if (! strncmp(name, SVN_PROP_ENTRY_PREFIX, NSLEN)) {
if (strcmp(name, SVN_PROP_ENTRY_COMMITTED_REV) == 0) {
b->committed_rev = value ?
apr_pstrdup(b->pool, value->data) : NULL;
} else if (strcmp(name, SVN_PROP_ENTRY_COMMITTED_DATE) == 0) {
b->committed_date = value ?
apr_pstrdup(b->pool, value->data) : NULL;
} else if (strcmp(name, SVN_PROP_ENTRY_LAST_AUTHOR) == 0) {
b->last_author = value ?
apr_pstrdup(b->pool, value->data) : NULL;
} else if ((strcmp(name, SVN_PROP_ENTRY_LOCK_TOKEN) == 0)
&& (! value)) {
if (! b->removed_props)
b->removed_props = apr_array_make(b->pool, 1, sizeof(name));
APR_ARRAY_PUSH(b->removed_props, const char *) = qname;
}
return SVN_NO_ERROR;
}
#undef NSLEN
if (value) {
if (! b->changed_props)
b->changed_props = apr_array_make(b->pool, 1, sizeof(name));
APR_ARRAY_PUSH(b->changed_props, const char *) = qname;
} else {
if (! b->removed_props)
b->removed_props = apr_array_make(b->pool, 1, sizeof(name));
APR_ARRAY_PUSH(b->removed_props, const char *) = qname;
}
}
return SVN_NO_ERROR;
}
static svn_error_t *
upd_close_directory(void *dir_baton, apr_pool_t *pool) {
return close_helper(TRUE , dir_baton);
}
static svn_error_t *
upd_add_file(const char *path,
void *parent_baton,
const char *copyfrom_path,
svn_revnum_t copyfrom_revision,
apr_pool_t *pool,
void **file_baton) {
return add_helper(FALSE ,
path, parent_baton, copyfrom_path, copyfrom_revision, pool,
file_baton);
}
static svn_error_t *
upd_open_file(const char *path,
void *parent_baton,
svn_revnum_t base_revision,
apr_pool_t *pool,
void **file_baton) {
return open_helper(FALSE ,
path, parent_baton, base_revision, pool, file_baton);
}
struct window_handler_baton {
svn_boolean_t seen_first_window;
update_ctx_t *uc;
svn_txdelta_window_handler_t handler;
void *handler_baton;
};
static svn_error_t *
window_handler(svn_txdelta_window_t *window, void *baton) {
struct window_handler_baton *wb = baton;
if (! wb->seen_first_window) {
wb->seen_first_window = TRUE;
SVN_ERR(dav_svn__send_xml(wb->uc->bb, wb->uc->output, "<S:txdelta>"));
}
SVN_ERR(wb->handler(window, wb->handler_baton));
if (window == NULL)
SVN_ERR(dav_svn__send_xml(wb->uc->bb, wb->uc->output, "</S:txdelta>"));
return SVN_NO_ERROR;
}
static svn_error_t *
dummy_window_handler(svn_txdelta_window_t *window, void *baton) {
return SVN_NO_ERROR;
}
static svn_error_t *
upd_apply_textdelta(void *file_baton,
const char *base_checksum,
apr_pool_t *pool,
svn_txdelta_window_handler_t *handler,
void **handler_baton) {
item_baton_t *file = file_baton;
struct window_handler_baton *wb;
svn_stream_t *base64_stream;
file->base_checksum = apr_pstrdup(file->pool, base_checksum);
file->text_changed = TRUE;
if (file->uc->resource_walk || (! file->uc->send_all)) {
*handler = dummy_window_handler;
*handler_baton = NULL;
return SVN_NO_ERROR;
}
wb = apr_palloc(file->pool, sizeof(*wb));
wb->seen_first_window = FALSE;
wb->uc = file->uc;
base64_stream = dav_svn__make_base64_output_stream(wb->uc->bb,
wb->uc->output,
file->pool);
svn_txdelta_to_svndiff2(&(wb->handler), &(wb->handler_baton),
base64_stream, file->uc->svndiff_version,
file->pool);
*handler = window_handler;
*handler_baton = wb;
return SVN_NO_ERROR;
}
static svn_error_t *
upd_close_file(void *file_baton, const char *text_checksum, apr_pool_t *pool) {
item_baton_t *file = file_baton;
file->text_checksum = text_checksum ?
apr_pstrdup(file->pool, text_checksum) : NULL;
if ((! file->uc->send_all) && (! file->added) && file->text_changed) {
const char *elt;
elt = apr_psprintf(pool, "<S:fetch-file%s%s%s/>" DEBUG_CR,
file->base_checksum ? " base-checksum=\"" : "",
file->base_checksum ? file->base_checksum : "",
file->base_checksum ? "\"" : "");
SVN_ERR(dav_svn__send_xml(file->uc->bb, file->uc->output, "%s", elt));
}
return close_helper(FALSE , file);
}
static svn_error_t *
upd_close_edit(void *edit_baton, apr_pool_t *pool) {
update_ctx_t *uc = edit_baton;
return maybe_start_update_report(uc);
}
static dav_error *
malformed_element_error(const char *tagname, apr_pool_t *pool) {
const char *errstr = apr_pstrcat(pool, "The request's '", tagname,
"' element is malformed; there "
"is a problem with the client.", NULL);
return dav_svn__new_error_tag(pool, HTTP_BAD_REQUEST, 0, errstr,
SVN_DAV_ERROR_NAMESPACE, SVN_DAV_ERROR_TAG);
}
dav_error *
dav_svn__update_report(const dav_resource *resource,
const apr_xml_doc *doc,
ap_filter_t *output) {
svn_delta_editor_t *editor;
apr_xml_elem *child;
void *rbaton = NULL;
update_ctx_t uc = { 0 };
svn_revnum_t revnum = SVN_INVALID_REVNUM;
svn_revnum_t from_revnum = SVN_INVALID_REVNUM;
int ns;
int entry_counter = 0;
svn_boolean_t entry_is_empty = FALSE;
svn_error_t *serr;
dav_error *derr = NULL;
apr_status_t apr_err;
const char *src_path = NULL;
const char *dst_path = NULL;
const dav_svn_repos *repos = resource->info->repos;
const char *target = "";
svn_boolean_t text_deltas = TRUE;
svn_depth_t requested_depth = svn_depth_unknown;
svn_boolean_t saw_depth = FALSE;
svn_boolean_t saw_recursive = FALSE;
svn_boolean_t resource_walk = FALSE;
svn_boolean_t ignore_ancestry = FALSE;
svn_boolean_t send_copyfrom_args = FALSE;
dav_svn__authz_read_baton arb;
apr_pool_t *subpool = svn_pool_create(resource->pool);
arb.r = resource->info->r;
arb.repos = repos;
if (resource->info->restype != DAV_SVN_RESTYPE_VCC) {
return dav_svn__new_error_tag(resource->pool, HTTP_CONFLICT, 0,
"This report can only be run against "
"a VCC.",
SVN_DAV_ERROR_NAMESPACE,
SVN_DAV_ERROR_TAG);
}
ns = dav_svn__find_ns(doc->namespaces, SVN_XML_NAMESPACE);
if (ns == -1) {
return dav_svn__new_error_tag(resource->pool, HTTP_BAD_REQUEST, 0,
"The request does not contain the 'svn:' "
"namespace, so it is not going to have an "
"svn:target-revision element. That element "
"is required.",
SVN_DAV_ERROR_NAMESPACE,
SVN_DAV_ERROR_TAG);
}
if (repos->bulk_updates) {
apr_xml_attr *this_attr;
for (this_attr = doc->root->attr; this_attr; this_attr = this_attr->next) {
if ((strcmp(this_attr->name, "send-all") == 0)
&& (strcmp(this_attr->value, "true") == 0)) {
uc.send_all = TRUE;
break;
}
}
}
for (child = doc->root->first_child; child != NULL; child = child->next) {
const char *cdata;
if (child->ns == ns && strcmp(child->name, "target-revision") == 0) {
cdata = dav_xml_get_cdata(child, resource->pool, 1);
if (! *cdata)
return malformed_element_error(child->name, resource->pool);
revnum = SVN_STR_TO_REV(cdata);
}
if (child->ns == ns && strcmp(child->name, "src-path") == 0) {
dav_svn__uri_info this_info;
cdata = dav_xml_get_cdata(child, resource->pool, 0);
if (! *cdata)
return malformed_element_error(child->name, resource->pool);
if ((derr = dav_svn__test_canonical(cdata, resource->pool)))
return derr;
if ((serr = dav_svn__simple_parse_uri(&this_info, resource,
cdata, resource->pool)))
return dav_svn__convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
"Could not parse 'src-path' URL.",
resource->pool);
src_path = this_info.repos_path;
}
if (child->ns == ns && strcmp(child->name, "dst-path") == 0) {
dav_svn__uri_info this_info;
cdata = dav_xml_get_cdata(child, resource->pool, 0);
if (! *cdata)
return malformed_element_error(child->name, resource->pool);
if ((derr = dav_svn__test_canonical(cdata, resource->pool)))
return derr;
if ((serr = dav_svn__simple_parse_uri(&this_info, resource,
cdata, resource->pool)))
return dav_svn__convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
"Could not parse 'dst-path' URL.",
resource->pool);
dst_path = this_info.repos_path;
}
if (child->ns == ns && strcmp(child->name, "update-target") == 0) {
cdata = dav_xml_get_cdata(child, resource->pool, 0);
if ((derr = dav_svn__test_canonical(cdata, resource->pool)))
return derr;
target = cdata;
}
if (child->ns == ns && strcmp(child->name, "depth") == 0) {
cdata = dav_xml_get_cdata(child, resource->pool, 1);
if (! *cdata)
return malformed_element_error(child->name, resource->pool);
requested_depth = svn_depth_from_word(cdata);
saw_depth = TRUE;
}
if ((child->ns == ns && strcmp(child->name, "recursive") == 0)
&& (! saw_depth)) {
cdata = dav_xml_get_cdata(child, resource->pool, 1);
if (! *cdata)
return malformed_element_error(child->name, resource->pool);
if (strcmp(cdata, "no") == 0)
requested_depth = svn_depth_files;
else
requested_depth = svn_depth_infinity;
saw_recursive = TRUE;
}
if (child->ns == ns && strcmp(child->name, "ignore-ancestry") == 0) {
cdata = dav_xml_get_cdata(child, resource->pool, 1);
if (! *cdata)
return malformed_element_error(child->name, resource->pool);
if (strcmp(cdata, "no") != 0)
ignore_ancestry = TRUE;
}
if (child->ns == ns && strcmp(child->name, "send-copyfrom-args") == 0) {
cdata = dav_xml_get_cdata(child, resource->pool, 1);
if (! *cdata)
return malformed_element_error(child->name, resource->pool);
if (strcmp(cdata, "no") != 0)
send_copyfrom_args = TRUE;
}
if (child->ns == ns && strcmp(child->name, "resource-walk") == 0) {
cdata = dav_xml_get_cdata(child, resource->pool, 1);
if (! *cdata)
return malformed_element_error(child->name, resource->pool);
if (strcmp(cdata, "no") != 0)
resource_walk = TRUE;
}
if (child->ns == ns && strcmp(child->name, "text-deltas") == 0) {
cdata = dav_xml_get_cdata(child, resource->pool, 1);
if (! *cdata)
return malformed_element_error(child->name, resource->pool);
if (strcmp(cdata, "no") == 0)
text_deltas = FALSE;
}
}
if (!saw_depth && !saw_recursive && (requested_depth == svn_depth_unknown))
requested_depth = svn_depth_infinity;
if (! src_path) {
return dav_svn__new_error_tag
(resource->pool, HTTP_BAD_REQUEST, 0,
"The request did not contain the '<src-path>' element.\n"
"This may indicate that your client is too old.",
SVN_DAV_ERROR_NAMESPACE,
SVN_DAV_ERROR_TAG);
}
if (revnum == SVN_INVALID_REVNUM) {
if ((serr = svn_fs_youngest_rev(&revnum, repos->fs, resource->pool)))
return dav_svn__convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
"Could not determine the youngest "
"revision for the update process.",
resource->pool);
}
uc.svndiff_version = resource->info->svndiff_version;
uc.resource = resource;
uc.output = output;
uc.anchor = src_path;
uc.target = target;
uc.bb = apr_brigade_create(resource->pool, output->c->bucket_alloc);
uc.pathmap = NULL;
if (dst_path) {
if (*target) {
uc.dst_path = svn_path_dirname(dst_path, resource->pool);
if (! uc.pathmap)
uc.pathmap = apr_hash_make(resource->pool);
add_to_path_map(uc.pathmap,
svn_path_join(src_path, target, resource->pool),
dst_path);
} else {
uc.dst_path = dst_path;
}
} else
uc.dst_path = uc.anchor;
if ((serr = svn_fs_revision_root(&uc.rev_root, repos->fs,
revnum, resource->pool))) {
return dav_svn__convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
"The revision root could not be created.",
resource->pool);
}
if (! uc.send_all)
text_deltas = FALSE;
editor = svn_delta_default_editor(resource->pool);
editor->set_target_revision = upd_set_target_revision;
editor->open_root = upd_open_root;
editor->delete_entry = upd_delete_entry;
editor->add_directory = upd_add_directory;
editor->open_directory = upd_open_directory;
editor->change_dir_prop = upd_change_xxx_prop;
editor->close_directory = upd_close_directory;
editor->absent_directory = upd_absent_directory;
editor->add_file = upd_add_file;
editor->open_file = upd_open_file;
editor->apply_textdelta = upd_apply_textdelta;
editor->change_file_prop = upd_change_xxx_prop;
editor->close_file = upd_close_file;
editor->absent_file = upd_absent_file;
editor->close_edit = upd_close_edit;
if ((serr = svn_repos_begin_report2(&rbaton, revnum,
repos->repos,
src_path, target,
dst_path,
text_deltas,
requested_depth,
ignore_ancestry,
send_copyfrom_args,
editor, &uc,
dav_svn__authz_read_func(&arb),
&arb,
resource->pool))) {
return dav_svn__convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
"The state report gatherer could not be "
"created.",
resource->pool);
}
for (child = doc->root->first_child; child != NULL; child = child->next)
if (child->ns == ns) {
svn_pool_clear(subpool);
if (strcmp(child->name, "entry") == 0) {
const char *path;
svn_revnum_t rev = SVN_INVALID_REVNUM;
svn_boolean_t saw_rev = FALSE;
const char *linkpath = NULL;
const char *locktoken = NULL;
svn_boolean_t start_empty = FALSE;
apr_xml_attr *this_attr = child->attr;
svn_depth_t depth = svn_depth_infinity;
entry_counter++;
while (this_attr) {
if (strcmp(this_attr->name, "rev") == 0) {
rev = SVN_STR_TO_REV(this_attr->value);
saw_rev = TRUE;
} else if (strcmp(this_attr->name, "depth") == 0)
depth = svn_depth_from_word(this_attr->value);
else if (strcmp(this_attr->name, "linkpath") == 0)
linkpath = this_attr->value;
else if (strcmp(this_attr->name, "start-empty") == 0)
start_empty = entry_is_empty = TRUE;
else if (strcmp(this_attr->name, "lock-token") == 0)
locktoken = this_attr->value;
this_attr = this_attr->next;
}
if (! saw_rev) {
serr = svn_error_create(SVN_ERR_XML_ATTRIB_NOT_FOUND,
NULL, "Missing XML attribute: rev");
derr = dav_svn__convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
"A failure occurred while "
"recording one of the items of "
"working copy state.",
resource->pool);
goto cleanup;
}
path = dav_xml_get_cdata(child, subpool, 0);
if (strcmp(path, "") == 0)
from_revnum = rev;
if (! linkpath)
serr = svn_repos_set_path3(rbaton, path, rev, depth,
start_empty, locktoken, subpool);
else
serr = svn_repos_link_path3(rbaton, path, linkpath, rev, depth,
start_empty, locktoken, subpool);
if (serr != NULL) {
derr = dav_svn__convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
"A failure occurred while "
"recording one of the items of "
"working copy state.",
resource->pool);
goto cleanup;
}
if (linkpath && (! dst_path)) {
const char *this_path;
if (! uc.pathmap)
uc.pathmap = apr_hash_make(resource->pool);
this_path = svn_path_join_many(apr_hash_pool_get(uc.pathmap),
src_path, target, path, NULL);
add_to_path_map(uc.pathmap, this_path, linkpath);
}
} else if (strcmp(child->name, "missing") == 0) {
const char *path = dav_xml_get_cdata(child, subpool, 0);
serr = svn_repos_delete_path(rbaton, path, subpool);
if (serr != NULL) {
derr = dav_svn__convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
"A failure occurred while "
"recording one of the (missing) "
"items of working copy state.",
resource->pool);
goto cleanup;
}
}
}
{
const char *action, *spath, *log_depth;
if (requested_depth == svn_depth_unknown)
log_depth = "";
else
log_depth = apr_pstrcat(resource->pool, " depth=",
svn_depth_to_word(requested_depth), NULL);
if (target)
spath = svn_path_join(src_path, target, resource->pool);
else
spath = src_path;
if (dst_path) {
if (uc.send_all)
action = apr_psprintf(resource->pool,
"switch %s %s@%ld%s",
svn_path_uri_encode(spath, resource->pool),
svn_path_uri_encode(dst_path, resource->pool),
revnum, log_depth);
else {
if (strcmp(spath, dst_path) == 0)
action = apr_psprintf(resource->pool,
"diff %s r%ld:%ld%s%s",
svn_path_uri_encode(spath, resource->pool),
from_revnum,
revnum, log_depth,
ignore_ancestry ? " ignore-ancestry" : "");
else
action = apr_psprintf(resource->pool,
"diff %s@%ld %s@%ld%s%s",
svn_path_uri_encode(spath, resource->pool),
from_revnum,
svn_path_uri_encode(dst_path,
resource->pool),
revnum, log_depth,
(ignore_ancestry
? " ignore-ancestry"
: ""));
}
}
else {
if (entry_counter == 1 && entry_is_empty)
action = apr_psprintf(resource->pool,
"checkout-or-export %s r%ld%s",
svn_path_uri_encode(spath, resource->pool),
revnum,
log_depth);
else {
if (text_deltas)
action = apr_psprintf(resource->pool,
"update %s r%ld%s%s",
svn_path_uri_encode(spath,
resource->pool),
revnum,
log_depth,
(send_copyfrom_args
? " send-copyfrom-args" : ""));
else
action = apr_psprintf(resource->pool,
"status %s r%ld%s",
svn_path_uri_encode(spath,
resource->pool),
revnum,
log_depth);
}
}
dav_svn__operational_log(resource->info, action);
}
serr = svn_repos_finish_report(rbaton, resource->pool);
if (serr) {
derr = dav_svn__convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
"A failure occurred while "
"driving the update report editor",
resource->pool);
goto cleanup;
}
rbaton = NULL;
if (dst_path && resource_walk) {
svn_node_kind_t dst_kind;
if ((serr = svn_fs_check_path(&dst_kind, uc.rev_root, dst_path,
resource->pool))) {
derr = dav_svn__convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
"Failed checking destination path kind",
resource->pool);
goto cleanup;
}
if (dst_kind != svn_node_dir)
resource_walk = FALSE;
}
if (dst_path && resource_walk) {
svn_fs_root_t *zero_root;
serr = svn_fs_revision_root(&zero_root, repos->fs, 0,
resource->pool);
if (serr) {
derr = dav_svn__convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
"Failed to find the revision root",
resource->pool);
goto cleanup;
}
serr = dav_svn__send_xml(uc.bb, uc.output,
"<S:resource-walk>" DEBUG_CR);
if (serr) {
derr = dav_svn__convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
"Unable to begin resource walk",
resource->pool);
goto cleanup;
}
uc.resource_walk = TRUE;
serr = svn_repos_dir_delta2(zero_root, "", target,
uc.rev_root, dst_path,
editor, &uc,
dav_svn__authz_read_func(&arb),
&arb, FALSE ,
requested_depth,
TRUE ,
FALSE ,
resource->pool);
if (serr) {
derr = dav_svn__convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
"Resource walk failed.",
resource->pool);
goto cleanup;
}
serr = dav_svn__send_xml(uc.bb, uc.output,
"</S:resource-walk>" DEBUG_CR);
if (serr) {
derr = dav_svn__convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
"Unable to complete resource walk.",
resource->pool);
goto cleanup;
}
}
if (uc.started_update) {
if ((serr = dav_svn__send_xml(uc.bb, uc.output,
"</S:update-report>" DEBUG_CR))) {
derr = dav_svn__convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
"Unable to complete update report.",
resource->pool);
goto cleanup;
}
}
cleanup:
if ((! derr) && ((apr_err = ap_fflush(output, uc.bb))))
derr = dav_svn__convert_err(svn_error_create(apr_err, 0, NULL),
HTTP_INTERNAL_SERVER_ERROR,
"Error flushing brigade.",
resource->pool);
if (derr) {
if (rbaton)
svn_error_clear(svn_repos_abort_report(rbaton, resource->pool));
return derr;
}
svn_pool_destroy(subpool);
return NULL;
}