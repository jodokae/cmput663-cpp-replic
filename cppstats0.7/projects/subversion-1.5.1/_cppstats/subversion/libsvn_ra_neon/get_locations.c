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
#include "svn_ra.h"
#include "../libsvn_ra/ra_loader.h"
#include "svn_path.h"
#include "svn_xml.h"
#include "svn_dav.h"
#include "private/svn_dav_protocol.h"
#include "svn_private_config.h"
#include "ra_neon.h"
static const svn_ra_neon__xml_elm_t gloc_report_elements[] = {
{ SVN_XML_NAMESPACE, "get-locations-report", ELEM_get_locations_report, 0 },
{ SVN_XML_NAMESPACE, "location", ELEM_location, 0 },
{ NULL }
};
typedef struct {
svn_ra_neon__session_t *ras;
apr_hash_t *hash;
apr_pool_t *pool;
} get_locations_baton_t;
static svn_error_t *
gloc_start_element(int *elem, void *userdata, int parent_state,
const char *ns, const char *ln, const char **atts) {
get_locations_baton_t *baton = userdata;
const svn_ra_neon__xml_elm_t *elm;
elm = svn_ra_neon__lookup_xml_elem(gloc_report_elements, ns, ln);
if (!elm) {
*elem = NE_XML_DECLINE;
return SVN_NO_ERROR;
}
if (parent_state == ELEM_get_locations_report
&& elm->id == ELEM_location) {
svn_revnum_t rev = SVN_INVALID_REVNUM;
const char *path;
const char *r;
r = svn_xml_get_attr_value("rev", atts);
if (r)
rev = SVN_STR_TO_REV(r);
path = svn_xml_get_attr_value("path", atts);
if (SVN_IS_VALID_REVNUM(rev) && path)
apr_hash_set(baton->hash,
apr_pmemdup(baton->pool, &rev, sizeof(rev)),
sizeof(rev), apr_pstrdup(baton->pool, path));
else
return svn_error_create(SVN_ERR_RA_DAV_MALFORMED_DATA, NULL,
_("Expected a valid revnum and path"));
}
*elem = elm->id;
return SVN_NO_ERROR;
}
svn_error_t *
svn_ra_neon__get_locations(svn_ra_session_t *session,
apr_hash_t **locations,
const char *relative_path,
svn_revnum_t peg_revision,
apr_array_header_t *location_revisions,
apr_pool_t *pool) {
svn_ra_neon__session_t *ras = session->priv;
svn_stringbuf_t *request_body;
svn_error_t *err;
get_locations_baton_t request_baton;
const char *relative_path_quoted;
svn_string_t bc_url, bc_relative;
const char *final_bc_url;
int i;
int status_code = 0;
*locations = apr_hash_make(pool);
request_body = svn_stringbuf_create("", pool);
svn_stringbuf_appendcstr(request_body,
"<?xml version=\"1.0\" encoding=\"utf-8\"?>" DEBUG_CR
"<S:get-locations xmlns:S=\"" SVN_XML_NAMESPACE
"\" xmlns:D=\"DAV:\">" DEBUG_CR);
svn_stringbuf_appendcstr(request_body, "<S:path>");
relative_path_quoted = apr_xml_quote_string(pool, relative_path, 0);
svn_stringbuf_appendcstr(request_body, relative_path_quoted);
svn_stringbuf_appendcstr(request_body, "</S:path>" DEBUG_CR);
svn_stringbuf_appendcstr(request_body,
apr_psprintf(pool,
"<S:peg-revision>%ld"
"</S:peg-revision>" DEBUG_CR,
peg_revision));
for (i = 0; i < location_revisions->nelts; ++i) {
svn_revnum_t rev = APR_ARRAY_IDX(location_revisions, i, svn_revnum_t);
svn_stringbuf_appendcstr(request_body,
apr_psprintf(pool,
"<S:location-revision>%ld"
"</S:location-revision>" DEBUG_CR,
rev));
}
svn_stringbuf_appendcstr(request_body, "</S:get-locations>");
request_baton.ras = ras;
request_baton.hash = *locations;
request_baton.pool = pool;
SVN_ERR(svn_ra_neon__get_baseline_info(NULL, &bc_url, &bc_relative, NULL,
ras, ras->url->data,
peg_revision,
pool));
final_bc_url = svn_path_url_add_component(bc_url.data, bc_relative.data,
pool);
err = svn_ra_neon__parsed_request(ras, "REPORT", final_bc_url,
request_body->data, NULL, NULL,
gloc_start_element, NULL, NULL,
&request_baton, NULL, &status_code,
FALSE, pool);
if (status_code == 501)
return svn_error_create(SVN_ERR_RA_NOT_IMPLEMENTED, err,
_("'get-locations' REPORT not implemented"));
return err;
}