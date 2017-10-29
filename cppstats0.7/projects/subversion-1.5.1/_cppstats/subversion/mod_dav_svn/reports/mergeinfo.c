#include <apr_pools.h>
#include <apr_strings.h>
#include <apr_xml.h>
#include <apr_md5.h>
#include <http_request.h>
#include <http_log.h>
#include <mod_dav.h>
#include "svn_pools.h"
#include "svn_repos.h"
#include "svn_xml.h"
#include "svn_path.h"
#include "svn_dav.h"
#include "private/svn_dav_protocol.h"
#include "private/svn_mergeinfo_private.h"
#include "../dav_svn.h"
dav_error *
dav_svn__get_mergeinfo_report(const dav_resource *resource,
const apr_xml_doc *doc,
ap_filter_t *output) {
svn_error_t *serr;
apr_status_t apr_err;
dav_error *derr = NULL;
apr_xml_elem *child;
svn_mergeinfo_catalog_t catalog;
svn_boolean_t include_descendants = FALSE;
dav_svn__authz_read_baton arb;
const dav_svn_repos *repos = resource->info->repos;
const char *action;
int ns;
apr_bucket_brigade *bb;
apr_hash_index_t *hi;
svn_boolean_t sent_anything = FALSE;
svn_revnum_t rev = SVN_INVALID_REVNUM;
svn_mergeinfo_inheritance_t inherit = svn_mergeinfo_explicit;
apr_array_header_t *paths
= apr_array_make(resource->pool, 0, sizeof(const char *));
svn_stringbuf_t *space_separated_paths =
svn_stringbuf_create("", resource->pool);
ns = dav_svn__find_ns(doc->namespaces, SVN_XML_NAMESPACE);
if (ns == -1) {
return dav_svn__new_error_tag(resource->pool, HTTP_BAD_REQUEST, 0,
"The request does not contain the 'svn:' "
"namespace, so it is not going to have "
"certain required elements.",
SVN_DAV_ERROR_NAMESPACE,
SVN_DAV_ERROR_TAG);
}
for (child = doc->root->first_child; child != NULL; child = child->next) {
if (child->ns != ns)
continue;
if (strcmp(child->name, SVN_DAV__REVISION) == 0)
rev = SVN_STR_TO_REV(dav_xml_get_cdata(child, resource->pool, 1));
else if (strcmp(child->name, SVN_DAV__INHERIT) == 0) {
inherit = svn_inheritance_from_word(
dav_xml_get_cdata(child, resource->pool, 1));
} else if (strcmp(child->name, SVN_DAV__PATH) == 0) {
const char *target;
const char *rel_path = dav_xml_get_cdata(child, resource->pool, 0);
if ((derr = dav_svn__test_canonical(rel_path, resource->pool)))
return derr;
target = svn_path_join(resource->info->repos_path, rel_path,
resource->pool);
(*((const char **)(apr_array_push(paths)))) = target;
if (space_separated_paths->len > 1)
svn_stringbuf_appendcstr(space_separated_paths, " ");
svn_stringbuf_appendcstr(space_separated_paths,
svn_path_uri_encode(target,
resource->pool));
} else if (strcmp(child->name, SVN_DAV__INCLUDE_DESCENDANTS) == 0) {
const char *word = dav_xml_get_cdata(child, resource->pool, 1);
if (strcmp(word, "yes") == 0)
include_descendants = TRUE;
}
}
arb.r = resource->info->r;
arb.repos = resource->info->repos;
bb = apr_brigade_create(resource->pool, output->c->bucket_alloc);
serr = svn_repos_fs_get_mergeinfo(&catalog, repos->repos, paths, rev,
inherit, include_descendants,
dav_svn__authz_read_func(&arb),
&arb, resource->pool);
if (serr) {
derr = dav_svn__convert_err(serr, HTTP_BAD_REQUEST, serr->message,
resource->pool);
goto cleanup;
}
serr = svn_mergeinfo__remove_prefix_from_catalog(&catalog, catalog,
resource->info->repos_path,
resource->pool);
if (serr) {
derr = dav_svn__convert_err(serr, HTTP_BAD_REQUEST, serr->message,
resource->pool);
goto cleanup;
}
sent_anything = TRUE;
serr = dav_svn__send_xml(bb, output,
DAV_XML_HEADER DEBUG_CR
"<S:" SVN_DAV__MERGEINFO_REPORT " "
"xmlns:S=\"" SVN_XML_NAMESPACE "\" "
"xmlns:D=\"DAV:\">" DEBUG_CR);
if (serr) {
derr = dav_svn__convert_err(serr, HTTP_BAD_REQUEST, serr->message,
resource->pool);
goto cleanup;
}
for (hi = apr_hash_first(resource->pool, catalog); hi;
hi = apr_hash_next(hi)) {
const void *key;
void *value;
const char *path;
svn_mergeinfo_t mergeinfo;
svn_string_t *mergeinfo_string;
const char itemformat[] = "<S:" SVN_DAV__MERGEINFO_ITEM ">"
DEBUG_CR
"<S:" SVN_DAV__MERGEINFO_PATH ">%s</S:" SVN_DAV__MERGEINFO_PATH ">"
DEBUG_CR
"<S:" SVN_DAV__MERGEINFO_INFO ">%s</S:" SVN_DAV__MERGEINFO_INFO ">"
DEBUG_CR
"</S:" SVN_DAV__MERGEINFO_ITEM ">";
apr_hash_this(hi, &key, NULL, &value);
path = key;
mergeinfo = value;
serr = svn_mergeinfo_to_string(&mergeinfo_string, mergeinfo,
resource->pool);
if (serr) {
derr = dav_svn__convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
"Error ending REPORT response.",
resource->pool);
goto cleanup;
}
serr = dav_svn__send_xml(bb, output, itemformat,
apr_xml_quote_string(resource->pool,
path, 0),
apr_xml_quote_string(resource->pool,
mergeinfo_string->data, 0));
if (serr) {
derr = dav_svn__convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
"Error ending REPORT response.",
resource->pool);
goto cleanup;
}
}
if ((serr = dav_svn__send_xml(bb, output,
"</S:" SVN_DAV__MERGEINFO_REPORT ">"
DEBUG_CR))) {
derr = dav_svn__convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
"Error ending REPORT response.",
resource->pool);
goto cleanup;
}
cleanup:
action = apr_psprintf(resource->pool, "get-mergeinfo (%s) %s%s",
space_separated_paths->data,
svn_inheritance_to_word(inherit),
include_descendants ? " include-descendants" : "");
dav_svn__operational_log(resource->info, action);
apr_err = 0;
if (sent_anything)
apr_err = ap_fflush(output, bb);
if (apr_err && !derr)
derr = dav_svn__convert_err(svn_error_create(apr_err, 0, NULL),
HTTP_INTERNAL_SERVER_ERROR,
"Error flushing brigade.",
resource->pool);
return derr;
}