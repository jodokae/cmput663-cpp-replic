#include <apr_pools.h>
#include <apr_buckets.h>
#include <apr_xml.h>
#include <apr_hash.h>
#include <httpd.h>
#include <util_filter.h>
#include "svn_pools.h"
#include "svn_fs.h"
#include "svn_props.h"
#include "svn_xml.h"
#include "dav_svn.h"
static svn_error_t *
send_response(const dav_svn_repos *repos,
svn_fs_root_t *root,
const char *path,
svn_boolean_t is_dir,
ap_filter_t *output,
apr_bucket_brigade *bb,
apr_pool_t *pool) {
const char *href;
const char *vsn_url;
apr_status_t status;
svn_revnum_t rev_to_use;
href = dav_svn__build_uri(repos, DAV_SVN__BUILD_URI_PUBLIC,
SVN_IGNORED_REVNUM, path, 0 , pool);
rev_to_use = dav_svn__get_safe_cr(root, path, pool);
vsn_url = dav_svn__build_uri(repos, DAV_SVN__BUILD_URI_VERSION,
rev_to_use, path, 0 , pool);
status = ap_fputstrs(output, bb,
"<D:response>" DEBUG_CR
"<D:href>",
apr_xml_quote_string(pool, href, 1),
"</D:href>" DEBUG_CR
"<D:propstat><D:prop>" DEBUG_CR,
is_dir
? "<D:resourcetype><D:collection/></D:resourcetype>"
: "<D:resourcetype/>",
DEBUG_CR,
"<D:checked-in><D:href>",
apr_xml_quote_string(pool, vsn_url, 1),
"</D:href></D:checked-in>" DEBUG_CR
"</D:prop>" DEBUG_CR
"<D:status>HTTP/1.1 200 OK</D:status>" DEBUG_CR
"</D:propstat>" DEBUG_CR
"</D:response>" DEBUG_CR,
NULL);
if (status != APR_SUCCESS)
return svn_error_wrap_apr(status, "Can't write response to output");
return SVN_NO_ERROR;
}
static svn_error_t *
do_resources(const dav_svn_repos *repos,
svn_fs_root_t *root,
svn_revnum_t revision,
ap_filter_t *output,
apr_bucket_brigade *bb,
apr_pool_t *pool) {
apr_hash_t *changes;
apr_hash_t *sent = apr_hash_make(pool);
apr_hash_index_t *hi;
apr_pool_t *subpool = svn_pool_create(pool);
SVN_ERR(svn_fs_paths_changed(&changes, root, pool));
for (hi = apr_hash_first(pool, changes); hi; hi = apr_hash_next(hi)) {
const void *key;
void *val;
const char *path;
svn_fs_path_change_t *change;
svn_boolean_t send_self;
svn_boolean_t send_parent;
svn_pool_clear(subpool);
apr_hash_this(hi, &key, NULL, &val);
path = key;
change = val;
switch (change->change_kind) {
case svn_fs_path_change_delete:
send_self = FALSE;
send_parent = TRUE;
break;
case svn_fs_path_change_add:
case svn_fs_path_change_replace:
send_self = TRUE;
send_parent = TRUE;
break;
case svn_fs_path_change_modify:
default:
send_self = TRUE;
send_parent = FALSE;
break;
}
if (send_self) {
if (! apr_hash_get(sent, path, APR_HASH_KEY_STRING)) {
svn_node_kind_t kind;
SVN_ERR(svn_fs_check_path(&kind, root, path, subpool));
SVN_ERR(send_response(repos, root, path,
kind == svn_node_dir ? TRUE : FALSE,
output, bb, subpool));
apr_hash_set(sent, path, APR_HASH_KEY_STRING, (void *)1);
}
}
if (send_parent) {
const char *parent = svn_path_dirname(path, pool);
if (! apr_hash_get(sent, parent, APR_HASH_KEY_STRING)) {
SVN_ERR(send_response(repos, root, parent,
TRUE, output, bb, subpool));
apr_hash_set(sent, parent, APR_HASH_KEY_STRING, (void *)1);
}
}
}
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
dav_error *
dav_svn__merge_response(ap_filter_t *output,
const dav_svn_repos *repos,
svn_revnum_t new_rev,
char *post_commit_err,
apr_xml_elem *prop_elem,
svn_boolean_t disable_merge_response,
apr_pool_t *pool) {
apr_bucket_brigade *bb;
svn_fs_root_t *root;
svn_error_t *serr;
const char *vcc;
const char *rev;
svn_string_t *creationdate, *creator_displayname;
const char *post_commit_err_elem = NULL,
*post_commit_header_info = NULL;
serr = svn_fs_revision_root(&root, repos->fs, new_rev, pool);
if (serr != NULL) {
return dav_svn__convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
"Could not open the FS root for the "
"revision just committed.",
repos->pool);
}
bb = apr_brigade_create(pool, output->c->bucket_alloc);
vcc = dav_svn__build_uri(repos, DAV_SVN__BUILD_URI_VCC, SVN_IGNORED_REVNUM,
NULL, 0 , pool);
rev = apr_psprintf(pool, "%ld", new_rev);
if (post_commit_err) {
post_commit_header_info = apr_psprintf(pool,
" xmlns:S=\"%s\"",
SVN_XML_NAMESPACE);
post_commit_err_elem = apr_psprintf(pool,
"<S:post-commit-err>%s"
"</S:post-commit-err>",
post_commit_err);
} else {
post_commit_header_info = "" ;
post_commit_err_elem = "" ;
}
serr = svn_fs_revision_prop(&creationdate, repos->fs, new_rev,
SVN_PROP_REVISION_DATE, pool);
if (serr != NULL) {
return dav_svn__convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
"Could not get date of newest revision",
repos->pool);
}
serr = svn_fs_revision_prop(&creator_displayname, repos->fs, new_rev,
SVN_PROP_REVISION_AUTHOR, pool);
if (serr != NULL) {
return dav_svn__convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
"Could not get author of newest revision",
repos->pool);
}
(void) ap_fputstrs(output, bb,
DAV_XML_HEADER DEBUG_CR
"<D:merge-response xmlns:D=\"DAV:\"",
post_commit_header_info,
">" DEBUG_CR
"<D:updated-set>" DEBUG_CR
"<D:response>" DEBUG_CR
"<D:href>",
apr_xml_quote_string(pool, vcc, 1),
"</D:href>" DEBUG_CR
"<D:propstat><D:prop>" DEBUG_CR
"<D:resourcetype><D:baseline/></D:resourcetype>" DEBUG_CR,
post_commit_err_elem, DEBUG_CR
"<D:version-name>", rev, "</D:version-name>" DEBUG_CR,
NULL);
if (creationdate) {
(void) ap_fputstrs(output, bb,
"<D:creationdate>",
apr_xml_quote_string(pool, creationdate->data, 1),
"</D:creationdate>" DEBUG_CR,
NULL);
}
if (creator_displayname) {
(void) ap_fputstrs(output, bb,
"<D:creator-displayname>",
apr_xml_quote_string(pool,
creator_displayname->data, 1),
"</D:creator-displayname>" DEBUG_CR,
NULL);
}
(void) ap_fputstrs(output, bb,
"</D:prop>" DEBUG_CR
"<D:status>HTTP/1.1 200 OK</D:status>" DEBUG_CR
"</D:propstat>" DEBUG_CR
"</D:response>" DEBUG_CR,
NULL);
if (! disable_merge_response) {
serr = do_resources(repos, root, new_rev, output, bb, pool);
if (serr != NULL) {
return dav_svn__convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
"Error constructing resource list.",
repos->pool);
}
}
(void) ap_fputs(output, bb,
"</D:updated-set>" DEBUG_CR
"</D:merge-response>" DEBUG_CR);
(void) ap_pass_brigade(output, bb);
return SVN_NO_ERROR;
}