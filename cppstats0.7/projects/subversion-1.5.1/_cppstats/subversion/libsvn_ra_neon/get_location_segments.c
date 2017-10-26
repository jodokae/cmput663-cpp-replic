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
static const svn_ra_neon__xml_elm_t gls_report_elements[] = {
{
SVN_XML_NAMESPACE, "get-location-segments-report",
ELEM_get_location_segments_report, 0
},
{ SVN_XML_NAMESPACE, "location-segment", ELEM_location_segment, 0 },
{ NULL }
};
typedef struct {
svn_location_segment_receiver_t receiver;
void *receiver_baton;
apr_pool_t *subpool;
} get_location_segments_baton_t;
static svn_error_t *
gls_start_element(int *elem, void *userdata, int parent_state,
const char *ns, const char *ln, const char **atts) {
get_location_segments_baton_t *baton = userdata;
const svn_ra_neon__xml_elm_t *elm;
if (! ((elm = svn_ra_neon__lookup_xml_elem(gls_report_elements, ns, ln)))) {
*elem = NE_XML_DECLINE;
return SVN_NO_ERROR;
}
if (parent_state == ELEM_get_location_segments_report
&& elm->id == ELEM_location_segment) {
const char *rev_str;
svn_revnum_t range_start = SVN_INVALID_REVNUM;
svn_revnum_t range_end = SVN_INVALID_REVNUM;
const char *path = NULL;
path = svn_xml_get_attr_value("path", atts);
rev_str = svn_xml_get_attr_value("range-start", atts);
if (rev_str)
range_start = SVN_STR_TO_REV(rev_str);
rev_str = svn_xml_get_attr_value("range-end", atts);
if (rev_str)
range_end = SVN_STR_TO_REV(rev_str);
if (SVN_IS_VALID_REVNUM(range_start) && SVN_IS_VALID_REVNUM(range_end)) {
svn_location_segment_t *segment = apr_pcalloc(baton->subpool,
sizeof(*segment));
segment->path = path;
segment->range_start = range_start;
segment->range_end = range_end;
SVN_ERR(baton->receiver(segment,
baton->receiver_baton,
baton->subpool));
svn_pool_clear(baton->subpool);
} else {
return svn_error_create(SVN_ERR_RA_DAV_MALFORMED_DATA, NULL,
_("Expected valid revision range"));
}
}
*elem = elm->id;
return SVN_NO_ERROR;
}
svn_error_t *
svn_ra_neon__get_location_segments(svn_ra_session_t *session,
const char *path,
svn_revnum_t peg_revision,
svn_revnum_t start_rev,
svn_revnum_t end_rev,
svn_location_segment_receiver_t receiver,
void *receiver_baton,
apr_pool_t *pool)
{
svn_ra_neon__session_t *ras = session->priv;
svn_stringbuf_t *request_body;
svn_error_t *err;
get_location_segments_baton_t request_baton;
svn_string_t bc_url, bc_relative;
const char *bc;
int status_code = 0;
apr_pool_t *subpool = svn_pool_create(pool);
request_body = svn_stringbuf_create("", subpool);
svn_stringbuf_appendcstr(request_body,
"<?xml version=\"1.0\" encoding=\"utf-8\"?>"
DEBUG_CR "<S:get-location-segments xmlns:S=\""
SVN_XML_NAMESPACE "\" xmlns:D=\"DAV:\">" DEBUG_CR);
svn_stringbuf_appendcstr(request_body, "<S:path>");
svn_stringbuf_appendcstr(request_body, apr_xml_quote_string(subpool, path, 0));
svn_stringbuf_appendcstr(request_body, "</S:path>" DEBUG_CR);
if (SVN_IS_VALID_REVNUM(peg_revision))
svn_stringbuf_appendcstr
(request_body, apr_psprintf(subpool,
"<S:peg-revision>%ld</S:peg-revision>"
DEBUG_CR, peg_revision));
if (SVN_IS_VALID_REVNUM(start_rev))
svn_stringbuf_appendcstr
(request_body, apr_psprintf(subpool,
"<S:start-revision>%ld</S:start-revision>"
DEBUG_CR, start_rev));
if (SVN_IS_VALID_REVNUM(end_rev))
svn_stringbuf_appendcstr
(request_body, apr_psprintf(subpool,
"<S:end-revision>%ld</S:end-revision>"
DEBUG_CR, end_rev));
svn_stringbuf_appendcstr(request_body, "</S:get-location-segments>");
request_baton.receiver = receiver;
request_baton.receiver_baton = receiver_baton;
request_baton.subpool = svn_pool_create(subpool);
SVN_ERR(svn_ra_neon__get_baseline_info(NULL, &bc_url, &bc_relative, NULL,
ras, ras->url->data,
peg_revision, subpool));
bc = svn_path_url_add_component(bc_url.data, bc_relative.data, subpool);
err = svn_ra_neon__parsed_request(ras, "REPORT", bc,
request_body->data, NULL, NULL,
gls_start_element, NULL, NULL,
&request_baton, NULL, &status_code,
FALSE, subpool);
svn_pool_destroy(request_baton.subpool);
svn_pool_destroy(subpool);
if (status_code == 501)
return svn_error_create(SVN_ERR_RA_NOT_IMPLEMENTED, err,
_("'get-location-segments' REPORT "
"not implemented"));
return err;
}
