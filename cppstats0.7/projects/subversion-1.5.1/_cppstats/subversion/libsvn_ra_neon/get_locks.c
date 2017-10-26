#define APR_WANT_STRFUNC
#include <apr_want.h>
#include <apr_pools.h>
#include <apr_tables.h>
#include <apr_strings.h>
#include <apr_xml.h>
#include <ne_basic.h>
#include "svn_error.h"
#include "svn_pools.h"
#include "svn_base64.h"
#include "svn_ra.h"
#include "../libsvn_ra/ra_loader.h"
#include "svn_path.h"
#include "svn_xml.h"
#include "svn_dav.h"
#include "svn_time.h"
#include "private/svn_dav_protocol.h"
#include "svn_private_config.h"
#include "ra_neon.h"
static const svn_ra_neon__xml_elm_t getlocks_report_elements[] = {
{ SVN_XML_NAMESPACE, "get-locks-report", ELEM_get_locks_report, 0 },
{ SVN_XML_NAMESPACE, "lock", ELEM_lock, 0},
{ SVN_XML_NAMESPACE, "path", ELEM_lock_path, SVN_RA_NEON__XML_CDATA },
{ SVN_XML_NAMESPACE, "token", ELEM_lock_token, SVN_RA_NEON__XML_CDATA },
{ SVN_XML_NAMESPACE, "owner", ELEM_lock_owner, SVN_RA_NEON__XML_CDATA },
{ SVN_XML_NAMESPACE, "comment", ELEM_lock_comment, SVN_RA_NEON__XML_CDATA },
{
SVN_XML_NAMESPACE, SVN_DAV__CREATIONDATE,
ELEM_lock_creationdate, SVN_RA_NEON__XML_CDATA
},
{
SVN_XML_NAMESPACE, "expirationdate",
ELEM_lock_expirationdate, SVN_RA_NEON__XML_CDATA
},
{ NULL }
};
typedef struct {
svn_lock_t *current_lock;
svn_stringbuf_t *cdata_accum;
const char *encoding;
apr_hash_t *lock_hash;
apr_pool_t *scratchpool;
apr_pool_t *pool;
} get_locks_baton_t;
static svn_error_t *
getlocks_start_element(int *elem, void *userdata, int parent_state,
const char *ns, const char *ln, const char **atts) {
get_locks_baton_t *baton = userdata;
const svn_ra_neon__xml_elm_t *elm;
elm = svn_ra_neon__lookup_xml_elem(getlocks_report_elements, ns, ln);
if (!elm) {
*elem = NE_XML_DECLINE;
return SVN_NO_ERROR;
}
if (elm->id == ELEM_lock) {
if (parent_state != ELEM_get_locks_report)
return UNEXPECTED_ELEMENT(ns, ln);
else
baton->current_lock = svn_lock_create(baton->pool);
}
else if (elm->id == ELEM_lock_path
|| elm->id == ELEM_lock_token
|| elm->id == ELEM_lock_owner
|| elm->id == ELEM_lock_comment
|| elm->id == ELEM_lock_creationdate
|| elm->id == ELEM_lock_expirationdate) {
const char *encoding;
if (parent_state != ELEM_lock)
return UNEXPECTED_ELEMENT(ns, ln);
encoding = svn_xml_get_attr_value("encoding", atts);
if (encoding)
baton->encoding = apr_pstrdup(baton->scratchpool, encoding);
}
*elem = elm->id;
return SVN_NO_ERROR;
}
static svn_error_t *
getlocks_cdata_handler(void *userdata, int state,
const char *cdata, size_t len) {
get_locks_baton_t *baton = userdata;
switch(state) {
case ELEM_lock_path:
case ELEM_lock_token:
case ELEM_lock_owner:
case ELEM_lock_comment:
case ELEM_lock_creationdate:
case ELEM_lock_expirationdate:
svn_stringbuf_appendbytes(baton->cdata_accum, cdata, len);
break;
}
return SVN_NO_ERROR;
}
static svn_error_t *
getlocks_end_element(void *userdata, int state,
const char *ns, const char *ln) {
get_locks_baton_t *baton = userdata;
const svn_ra_neon__xml_elm_t *elm;
elm = svn_ra_neon__lookup_xml_elem(getlocks_report_elements, ns, ln);
if (elm == NULL)
return SVN_NO_ERROR;
switch (elm->id) {
case ELEM_lock:
if ((! baton->current_lock->path)
|| (! baton->current_lock->token)
|| (! baton->current_lock->owner)
|| (! baton->current_lock->creation_date))
SVN_ERR(svn_error_create(SVN_ERR_RA_DAV_MALFORMED_DATA, NULL,
_("Incomplete lock data returned")));
apr_hash_set(baton->lock_hash, baton->current_lock->path,
APR_HASH_KEY_STRING, baton->current_lock);
break;
case ELEM_lock_path:
baton->current_lock->path = apr_pstrmemdup(baton->pool,
baton->cdata_accum->data,
baton->cdata_accum->len);
svn_stringbuf_setempty(baton->cdata_accum);
svn_pool_clear(baton->scratchpool);
break;
case ELEM_lock_token:
baton->current_lock->token = apr_pstrmemdup(baton->pool,
baton->cdata_accum->data,
baton->cdata_accum->len);
svn_stringbuf_setempty(baton->cdata_accum);
svn_pool_clear(baton->scratchpool);
break;
case ELEM_lock_creationdate:
SVN_ERR(svn_time_from_cstring(&(baton->current_lock->creation_date),
baton->cdata_accum->data,
baton->scratchpool));
svn_stringbuf_setempty(baton->cdata_accum);
svn_pool_clear(baton->scratchpool);
break;
case ELEM_lock_expirationdate:
SVN_ERR(svn_time_from_cstring(&(baton->current_lock->expiration_date),
baton->cdata_accum->data,
baton->scratchpool));
svn_stringbuf_setempty(baton->cdata_accum);
svn_pool_clear(baton->scratchpool);
break;
case ELEM_lock_owner:
case ELEM_lock_comment: {
const char *final_val;
if (baton->encoding) {
if (strcmp(baton->encoding, "base64") == 0) {
svn_string_t *encoded_val;
const svn_string_t *decoded_val;
encoded_val = svn_string_create_from_buf(baton->cdata_accum,
baton->scratchpool);
decoded_val = svn_base64_decode_string(encoded_val,
baton->scratchpool);
final_val = decoded_val->data;
} else
return svn_error_createf(SVN_ERR_RA_DAV_MALFORMED_DATA,
NULL,
_("Got unrecognized encoding '%s'"),
baton->encoding);
baton->encoding = NULL;
} else {
final_val = baton->cdata_accum->data;
}
if (elm->id == ELEM_lock_owner)
baton->current_lock->owner = apr_pstrdup(baton->pool, final_val);
if (elm->id == ELEM_lock_comment)
baton->current_lock->comment = apr_pstrdup(baton->pool, final_val);
svn_stringbuf_setempty(baton->cdata_accum);
svn_pool_clear(baton->scratchpool);
break;
}
default:
break;
}
return SVN_NO_ERROR;
}
svn_error_t *
svn_ra_neon__get_locks(svn_ra_session_t *session,
apr_hash_t **locks,
const char *path,
apr_pool_t *pool) {
svn_ra_neon__session_t *ras = session->priv;
const char *body, *url;
svn_error_t *err;
int status_code = 0;
get_locks_baton_t baton;
baton.lock_hash = apr_hash_make(pool);
baton.pool = pool;
baton.scratchpool = svn_pool_create(pool);
baton.current_lock = NULL;
baton.encoding = NULL;
baton.cdata_accum = svn_stringbuf_create("", pool);
body = apr_psprintf(pool,
"<?xml version=\"1.0\" encoding=\"utf-8\"?>"
"<S:get-locks-report xmlns:S=\"" SVN_XML_NAMESPACE "\" "
"xmlns:D=\"DAV:\">"
"</S:get-locks-report>");
url = svn_path_url_add_component(ras->url->data, path, pool);
err = svn_ra_neon__parsed_request(ras, "REPORT", url,
body, NULL, NULL,
getlocks_start_element,
getlocks_cdata_handler,
getlocks_end_element,
&baton,
NULL,
&status_code,
FALSE,
pool);
svn_pool_destroy(baton.scratchpool);
if (err && err->apr_err == SVN_ERR_RA_DAV_PATH_NOT_FOUND) {
svn_error_clear(err);
*locks = baton.lock_hash;
return SVN_NO_ERROR;
}
err = svn_ra_neon__maybe_store_auth_info_after_result(err, ras, pool);
if (status_code == 501)
return svn_error_create(SVN_ERR_RA_NOT_IMPLEMENTED, err,
_("Server does not support locking features"));
if (err && err->apr_err == SVN_ERR_UNSUPPORTED_FEATURE)
return svn_error_create(SVN_ERR_RA_NOT_IMPLEMENTED, err,
_("Server does not support locking features"));
else if (err)
return err;
*locks = baton.lock_hash;
return SVN_NO_ERROR;
}
