#include <apr_tables.h>
#include <apr_uuid.h>
#include <httpd.h>
#include <http_log.h>
#include <mod_dav.h>
#include "svn_fs.h"
#include "svn_xml.h"
#include "svn_repos.h"
#include "svn_dav.h"
#include "svn_time.h"
#include "svn_pools.h"
#include "svn_props.h"
#include "svn_dav.h"
#include "svn_base64.h"
#include "private/svn_dav_protocol.h"
#include "../dav_svn.h"
dav_error *
dav_svn__dated_rev_report(const dav_resource *resource,
const apr_xml_doc *doc,
ap_filter_t *output) {
apr_xml_elem *child;
int ns;
apr_time_t tm = (apr_time_t) -1;
svn_revnum_t rev;
apr_bucket_brigade *bb;
svn_error_t *err;
apr_status_t apr_err;
dav_error *derr = NULL;
ns = dav_svn__find_ns(doc->namespaces, "DAV:");
if (ns != -1) {
for (child = doc->root->first_child; child != NULL; child = child->next) {
if (child->ns != ns ||
strcmp(child->name, SVN_DAV__CREATIONDATE) != 0)
continue;
svn_error_clear
(svn_time_from_cstring(&tm, dav_xml_get_cdata(child,
resource->pool, 1),
resource->pool));
}
}
if (tm == (apr_time_t) -1) {
return dav_new_error(resource->pool, HTTP_BAD_REQUEST, 0,
"The request does not contain a valid "
"'DAV:" SVN_DAV__CREATIONDATE "' element.");
}
if ((err = svn_repos_dated_revision(&rev, resource->info->repos->repos, tm,
resource->pool)) != SVN_NO_ERROR) {
svn_error_clear(err);
return dav_new_error(resource->pool, HTTP_INTERNAL_SERVER_ERROR, 0,
"Could not access revision times.");
}
bb = apr_brigade_create(resource->pool, output->c->bucket_alloc);
apr_err = ap_fprintf(output, bb,
DAV_XML_HEADER DEBUG_CR
"<S:dated-rev-report xmlns:S=\"" SVN_XML_NAMESPACE "\" "
"xmlns:D=\"DAV:\">" DEBUG_CR
"<D:" SVN_DAV__VERSION_NAME ">%ld</D:"
SVN_DAV__VERSION_NAME ">""</S:dated-rev-report>", rev);
if (apr_err)
derr = dav_svn__convert_err(svn_error_create(apr_err, 0, NULL),
HTTP_INTERNAL_SERVER_ERROR,
"Error writing REPORT response.",
resource->pool);
if (((apr_err = ap_fflush(output, bb))) && (! derr))
derr = dav_svn__convert_err(svn_error_create(apr_err, 0, NULL),
HTTP_INTERNAL_SERVER_ERROR,
"Error flushing brigade.",
resource->pool);
return derr;
}
