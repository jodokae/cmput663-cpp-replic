#if !defined(SVN_LIBSVN_RA_NEON_H)
#define SVN_LIBSVN_RA_NEON_H
#include <apr_pools.h>
#include <apr_tables.h>
#include <ne_request.h>
#include <ne_uri.h>
#include <ne_207.h>
#include <ne_props.h>
#include "svn_types.h"
#include "svn_string.h"
#include "svn_delta.h"
#include "svn_ra.h"
#include "svn_dav.h"
#include "private/svn_dav_protocol.h"
#include "svn_private_config.h"
#if defined(__cplusplus)
extern "C" {
#endif
#define SVN_RA_NEON__XML_DECLINE NE_XML_DECLINE
#define SVN_RA_NEON__XML_INVALID NE_XML_ABORT
#define SVN_RA_NEON__XML_CDATA (1<<1)
#define SVN_RA_NEON__XML_COLLECT ((1<<2) | SVN_RA_NEON__XML_CDATA)
typedef int svn_ra_neon__xml_elmid;
typedef struct {
const char *nspace;
const char *name;
svn_ra_neon__xml_elmid id;
unsigned int flags;
} svn_ra_neon__xml_elm_t;
typedef struct {
apr_pool_t *pool;
svn_stringbuf_t *url;
ne_uri root;
const char *repos_root;
ne_session *ne_sess;
ne_session *ne_sess2;
svn_boolean_t main_session_busy;
const svn_ra_callbacks2_t *callbacks;
void *callback_baton;
svn_auth_iterstate_t *auth_iterstate;
const char *auth_username;
svn_auth_iterstate_t *p11pin_iterstate;
svn_boolean_t compression;
const char *vcc;
const char *uuid;
svn_ra_progress_notify_func_t progress_func;
void *progress_baton;
apr_hash_t *capabilities;
} svn_ra_neon__session_t;
typedef struct {
ne_request *ne_req;
ne_session *ne_sess;
svn_ra_neon__session_t *sess;
const char *method;
const char *url;
int rv;
int code;
const char *code_desc;
svn_error_t *err;
svn_boolean_t marshalled_error;
apr_pool_t *pool;
apr_pool_t *iterpool;
} svn_ra_neon__request_t;
#define SVN_RA_NEON__REQ_ERR(req, new_err) do { svn_error_t *svn_err__tmp = (new_err); if ((req)->err && !(req)->marshalled_error) svn_error_clear(svn_err__tmp); else if (svn_err__tmp) { svn_error_clear((req)->err); (req)->err = svn_err__tmp; (req)->marshalled_error = FALSE; } } while (0)
svn_ra_neon__request_t *
svn_ra_neon__request_create(svn_ra_neon__session_t *sess,
const char *method, const char *url,
apr_pool_t *pool);
typedef svn_error_t *(*svn_ra_neon__block_reader)(void *baton,
const char *data,
size_t len);
void
svn_ra_neon__add_response_body_reader(svn_ra_neon__request_t *req,
ne_accept_response accpt,
svn_ra_neon__block_reader reader,
void *userdata);
#define svn_ra_neon__request_destroy(req) svn_pool_destroy((req)->pool)
#if defined(SVN_DEBUG)
#define DEBUG_CR "\n"
#else
#define DEBUG_CR ""
#endif
svn_error_t *svn_ra_neon__get_latest_revnum(svn_ra_session_t *session,
svn_revnum_t *latest_revnum,
apr_pool_t *pool);
svn_error_t *svn_ra_neon__get_dated_revision(svn_ra_session_t *session,
svn_revnum_t *revision,
apr_time_t timestamp,
apr_pool_t *pool);
svn_error_t *svn_ra_neon__change_rev_prop(svn_ra_session_t *session,
svn_revnum_t rev,
const char *name,
const svn_string_t *value,
apr_pool_t *pool);
svn_error_t *svn_ra_neon__rev_proplist(svn_ra_session_t *session,
svn_revnum_t rev,
apr_hash_t **props,
apr_pool_t *pool);
svn_error_t *svn_ra_neon__rev_prop(svn_ra_session_t *session,
svn_revnum_t rev,
const char *name,
svn_string_t **value,
apr_pool_t *pool);
svn_error_t * svn_ra_neon__get_commit_editor(svn_ra_session_t *session,
const svn_delta_editor_t **editor,
void **edit_baton,
apr_hash_t *revprop_table,
svn_commit_callback2_t callback,
void *callback_baton,
apr_hash_t *lock_tokens,
svn_boolean_t keep_locks,
apr_pool_t *pool);
svn_error_t * svn_ra_neon__get_file(svn_ra_session_t *session,
const char *path,
svn_revnum_t revision,
svn_stream_t *stream,
svn_revnum_t *fetched_rev,
apr_hash_t **props,
apr_pool_t *pool);
svn_error_t *svn_ra_neon__get_dir(svn_ra_session_t *session,
apr_hash_t **dirents,
svn_revnum_t *fetched_rev,
apr_hash_t **props,
const char *path,
svn_revnum_t revision,
apr_uint32_t dirent_fields,
apr_pool_t *pool);
svn_error_t * svn_ra_neon__abort_commit(void *session_baton,
void *edit_baton);
svn_error_t * svn_ra_neon__get_mergeinfo(svn_ra_session_t *session,
apr_hash_t **mergeinfo,
const apr_array_header_t *paths,
svn_revnum_t revision,
svn_mergeinfo_inheritance_t inherit,
svn_boolean_t include_descendants,
apr_pool_t *pool);
svn_error_t * svn_ra_neon__do_update(svn_ra_session_t *session,
const svn_ra_reporter3_t **reporter,
void **report_baton,
svn_revnum_t revision_to_update_to,
const char *update_target,
svn_depth_t depth,
svn_boolean_t send_copyfrom_args,
const svn_delta_editor_t *wc_update,
void *wc_update_baton,
apr_pool_t *pool);
svn_error_t * svn_ra_neon__do_status(svn_ra_session_t *session,
const svn_ra_reporter3_t **reporter,
void **report_baton,
const char *status_target,
svn_revnum_t revision,
svn_depth_t depth,
const svn_delta_editor_t *wc_status,
void *wc_status_baton,
apr_pool_t *pool);
svn_error_t * svn_ra_neon__do_switch(svn_ra_session_t *session,
const svn_ra_reporter3_t **reporter,
void **report_baton,
svn_revnum_t revision_to_update_to,
const char *update_target,
svn_depth_t depth,
const char *switch_url,
const svn_delta_editor_t *wc_update,
void *wc_update_baton,
apr_pool_t *pool);
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
apr_pool_t *pool);
svn_error_t * svn_ra_neon__get_log(svn_ra_session_t *session,
const apr_array_header_t *paths,
svn_revnum_t start,
svn_revnum_t end,
int limit,
svn_boolean_t discover_changed_paths,
svn_boolean_t strict_node_history,
svn_boolean_t include_merged_revisions,
const apr_array_header_t *revprops,
svn_log_entry_receiver_t receiver,
void *receiver_baton,
apr_pool_t *pool);
svn_error_t *svn_ra_neon__do_check_path(svn_ra_session_t *session,
const char *path,
svn_revnum_t revision,
svn_node_kind_t *kind,
apr_pool_t *pool);
svn_error_t *svn_ra_neon__do_stat(svn_ra_session_t *session,
const char *path,
svn_revnum_t revision,
svn_dirent_t **dirent,
apr_pool_t *pool);
svn_error_t *svn_ra_neon__get_file_revs(svn_ra_session_t *session,
const char *path,
svn_revnum_t start,
svn_revnum_t end,
svn_boolean_t include_merged_revisions,
svn_file_rev_handler_t handler,
void *handler_baton,
apr_pool_t *pool);
#define SVN_RA_NEON__LP_NAMESPACE SVN_PROP_WC_PREFIX "ra_dav:"
#define SVN_RA_NEON__LP_ACTIVITY_COLL SVN_RA_NEON__LP_NAMESPACE "activity-url"
#define SVN_RA_NEON__LP_VSN_URL SVN_RA_NEON__LP_NAMESPACE "version-url"
#define SVN_RA_NEON__PROP_BASELINE_COLLECTION "DAV:baseline-collection"
#define SVN_RA_NEON__PROP_CHECKED_IN "DAV:checked-in"
#define SVN_RA_NEON__PROP_VCC "DAV:version-controlled-configuration"
#define SVN_RA_NEON__PROP_VERSION_NAME "DAV:" SVN_DAV__VERSION_NAME
#define SVN_RA_NEON__PROP_CREATIONDATE "DAV:creationdate"
#define SVN_RA_NEON__PROP_CREATOR_DISPLAYNAME "DAV:creator-displayname"
#define SVN_RA_NEON__PROP_GETCONTENTLENGTH "DAV:getcontentlength"
#define SVN_RA_NEON__PROP_BASELINE_RELPATH SVN_DAV_PROP_NS_DAV "baseline-relative-path"
#define SVN_RA_NEON__PROP_MD5_CHECKSUM SVN_DAV_PROP_NS_DAV "md5-checksum"
#define SVN_RA_NEON__PROP_REPOSITORY_UUID SVN_DAV_PROP_NS_DAV "repository-uuid"
#define SVN_RA_NEON__PROP_DEADPROP_COUNT SVN_DAV_PROP_NS_DAV "deadprop-count"
typedef struct {
const char *url;
int is_collection;
apr_hash_t *propset;
int href_parent;
apr_pool_t *pool;
} svn_ra_neon__resource_t;
svn_error_t * svn_ra_neon__get_props(apr_hash_t **results,
svn_ra_neon__session_t *sess,
const char *url,
int depth,
const char *label,
const ne_propname *which_props,
apr_pool_t *pool);
svn_error_t * svn_ra_neon__get_props_resource(svn_ra_neon__resource_t **rsrc,
svn_ra_neon__session_t *sess,
const char *url,
const char *label,
const ne_propname *which_props,
apr_pool_t *pool);
svn_error_t * svn_ra_neon__get_starting_props(svn_ra_neon__resource_t **rsrc,
svn_ra_neon__session_t *sess,
const char *url,
const char *label,
apr_pool_t *pool);
svn_error_t *
svn_ra_neon__search_for_starting_props(svn_ra_neon__resource_t **rsrc,
const char **missing_path,
svn_ra_neon__session_t *sess,
const char *url,
apr_pool_t *pool);
svn_error_t * svn_ra_neon__get_one_prop(const svn_string_t **propval,
svn_ra_neon__session_t *sess,
const char *url,
const char *label,
const ne_propname *propname,
apr_pool_t *pool);
svn_error_t *svn_ra_neon__get_baseline_info(svn_boolean_t *is_dir,
svn_string_t *bc_url,
svn_string_t *bc_relative,
svn_revnum_t *latest_rev,
svn_ra_neon__session_t *sess,
const char *url,
svn_revnum_t revision,
apr_pool_t *pool);
svn_error_t *svn_ra_neon__get_baseline_props(svn_string_t *bc_relative,
svn_ra_neon__resource_t **bln_rsrc,
svn_ra_neon__session_t *sess,
const char *url,
svn_revnum_t revision,
const ne_propname *which_props,
apr_pool_t *pool);
svn_error_t *svn_ra_neon__get_vcc(const char **vcc,
svn_ra_neon__session_t *sess,
const char *url,
apr_pool_t *pool);
svn_error_t *svn_ra_neon__do_proppatch(svn_ra_neon__session_t *ras,
const char *url,
apr_hash_t *prop_changes,
apr_array_header_t *prop_deletes,
apr_hash_t *extra_headers,
apr_pool_t *pool);
extern const ne_propname svn_ra_neon__vcc_prop;
extern const ne_propname svn_ra_neon__checked_in_prop;
svn_error_t * svn_ra_neon__get_activity_collection
(const svn_string_t **activity_coll,
svn_ra_neon__session_t *ras,
const char *url,
apr_pool_t *pool);
svn_error_t *svn_ra_neon__set_neon_body_provider(svn_ra_neon__request_t *req,
apr_file_t *body_file);
#define SVN_RA_NEON__DEPTH_ZERO 0
#define SVN_RA_NEON__DEPTH_ONE 1
#define SVN_RA_NEON__DEPTH_INFINITE -1
void
svn_ra_neon__add_depth_header(apr_hash_t *extra_headers, int depth);
const svn_ra_neon__xml_elm_t *
svn_ra_neon__lookup_xml_elem(const svn_ra_neon__xml_elm_t *table,
const char *nspace,
const char *name);
svn_error_t *
svn_ra_neon__xml_collect_cdata(void *baton, int state,
const char *cdata, size_t len);
typedef svn_error_t * (*svn_ra_neon__startelm_cb_t)(int *elem,
void *baton,
int parent,
const char *nspace,
const char *name,
const char **atts);
typedef svn_error_t * (*svn_ra_neon__cdata_cb_t)(void *baton,
int state,
const char *cdata,
size_t len);
typedef svn_error_t * (*svn_ra_neon__endelm_cb_t)(void *baton,
int state,
const char *nspace,
const char *name);
ne_xml_parser *
svn_ra_neon__xml_parser_create(svn_ra_neon__request_t *req,
ne_accept_response accpt,
svn_ra_neon__startelm_cb_t startelm_cb,
svn_ra_neon__cdata_cb_t cdata_cb,
svn_ra_neon__endelm_cb_t endelm_cb,
void *baton);
svn_error_t *
svn_ra_neon__parsed_request(svn_ra_neon__session_t *sess,
const char *method,
const char *url,
const char *body,
apr_file_t *body_file,
void set_parser(ne_xml_parser *parser,
void *baton),
svn_ra_neon__startelm_cb_t startelm_cb,
svn_ra_neon__cdata_cb_t cdata_cb,
svn_ra_neon__endelm_cb_t endelm_cb,
void *baton,
apr_hash_t *extra_headers,
int *status_code,
svn_boolean_t spool_response,
apr_pool_t *pool);
enum {
ELEM_unknown = 1,
ELEM_root = NE_XML_STATEROOT,
ELEM_UNUSED = 100,
ELEM_207_first = ELEM_UNUSED,
ELEM_multistatus = ELEM_207_first,
ELEM_response = ELEM_207_first + 1,
ELEM_responsedescription = ELEM_207_first + 2,
ELEM_href = ELEM_207_first + 3,
ELEM_propstat = ELEM_207_first + 4,
ELEM_prop = ELEM_207_first + 5,
ELEM_status = ELEM_207_first + 6,
ELEM_207_UNUSED = ELEM_UNUSED + 100,
ELEM_PROPS_UNUSED = ELEM_207_UNUSED + 100,
ELEM_activity_coll_set = ELEM_207_UNUSED,
ELEM_baseline,
ELEM_baseline_coll,
ELEM_checked_in,
ELEM_collection,
ELEM_comment,
ELEM_revprop,
ELEM_creationdate,
ELEM_creator_displayname,
ELEM_ignored_set,
ELEM_merge_response,
ELEM_merged_set,
ELEM_options_response,
ELEM_set_prop,
ELEM_remove_prop,
ELEM_resourcetype,
ELEM_get_content_length,
ELEM_updated_set,
ELEM_vcc,
ELEM_version_name,
ELEM_post_commit_err,
ELEM_error,
ELEM_absent_directory,
ELEM_absent_file,
ELEM_add_directory,
ELEM_add_file,
ELEM_baseline_relpath,
ELEM_md5_checksum,
ELEM_deleted_path,
ELEM_replaced_path,
ELEM_added_path,
ELEM_modified_path,
ELEM_delete_entry,
ELEM_fetch_file,
ELEM_fetch_props,
ELEM_txdelta,
ELEM_log_date,
ELEM_log_item,
ELEM_log_report,
ELEM_open_directory,
ELEM_open_file,
ELEM_target_revision,
ELEM_update_report,
ELEM_resource_walk,
ELEM_resource,
ELEM_SVN_prop,
ELEM_dated_rev_report,
ELEM_name_version_name,
ELEM_name_creationdate,
ELEM_name_creator_displayname,
ELEM_svn_error,
ELEM_human_readable,
ELEM_repository_uuid,
ELEM_get_locations_report,
ELEM_location,
ELEM_get_location_segments_report,
ELEM_location_segment,
ELEM_file_revs_report,
ELEM_file_rev,
ELEM_rev_prop,
ELEM_get_locks_report,
ELEM_lock,
ELEM_lock_path,
ELEM_lock_token,
ELEM_lock_owner,
ELEM_lock_comment,
ELEM_lock_creationdate,
ELEM_lock_expirationdate,
ELEM_lock_discovery,
ELEM_lock_activelock,
ELEM_lock_type,
ELEM_lock_scope,
ELEM_lock_depth,
ELEM_lock_timeout,
ELEM_editor_report,
ELEM_open_root,
ELEM_apply_textdelta,
ELEM_change_file_prop,
ELEM_change_dir_prop,
ELEM_close_file,
ELEM_close_directory,
ELEM_deadprop_count,
ELEM_mergeinfo_report,
ELEM_mergeinfo_item,
ELEM_mergeinfo_path,
ELEM_mergeinfo_info,
ELEM_has_children,
ELEM_merged_revision
};
svn_error_t * svn_ra_neon__merge_activity(svn_revnum_t *new_rev,
const char **committed_date,
const char **committed_author,
const char **post_commit_err,
svn_ra_neon__session_t *ras,
const char *repos_url,
const char *activity_url,
apr_hash_t *valid_targets,
apr_hash_t *lock_tokens,
svn_boolean_t keep_locks,
svn_boolean_t disable_merge_response,
apr_pool_t *pool);
#define MAKE_BUFFER(p) svn_stringbuf_ncreate("", 0, (p))
svn_error_t *
svn_ra_neon__copy_href(svn_stringbuf_t *dst, const char *src,
apr_pool_t *pool);
svn_error_t *
svn_ra_neon__maybe_store_auth_info(svn_ra_neon__session_t *ras,
apr_pool_t *pool);
svn_error_t *
svn_ra_neon__maybe_store_auth_info_after_result(svn_error_t *err,
svn_ra_neon__session_t *ras,
apr_pool_t *pool);
#define UNEXPECTED_ELEMENT(ns, elem) (ns ? svn_error_createf(SVN_ERR_RA_DAV_MALFORMED_DATA, NULL, _("Got unexpected element %s:%s"), ns, elem) : svn_error_createf(SVN_ERR_RA_DAV_MALFORMED_DATA, NULL, _("Got unexpected element %s"), elem))
#define MISSING_ATTR(ns, elem, attr) (ns ? svn_error_createf(SVN_ERR_RA_DAV_MALFORMED_DATA, NULL, _("Missing attribute '%s' on element %s:%s"), attr, ns, elem) : svn_error_createf(SVN_ERR_RA_DAV_MALFORMED_DATA, NULL, _("Missing attribute '%s' on element %s"), attr, elem))
svn_error_t *
svn_ra_neon__request_dispatch(int *code_p,
svn_ra_neon__request_t *request,
apr_hash_t *extra_headers,
const char *body,
int okay_1,
int okay_2,
apr_pool_t *pool);
svn_error_t *
svn_ra_neon__simple_request(int *code,
svn_ra_neon__session_t *ras,
const char *method,
const char *url,
apr_hash_t *extra_headers,
const char *body,
int okay_1, int okay_2, apr_pool_t *pool);
#define svn_ra_neon__set_header(hash, hdr, val) apr_hash_set((hash), (hdr), APR_HASH_KEY_STRING, (val))
svn_error_t *
svn_ra_neon__copy(svn_ra_neon__session_t *ras,
svn_boolean_t overwrite,
int depth,
const char *src,
const char *dst,
apr_pool_t *pool);
const char *
svn_ra_neon__request_get_location(svn_ra_neon__request_t *request,
apr_pool_t *pool);
svn_error_t *
svn_ra_neon__get_locations(svn_ra_session_t *session,
apr_hash_t **locations,
const char *path,
svn_revnum_t peg_revision,
apr_array_header_t *location_revisions,
apr_pool_t *pool);
svn_error_t *
svn_ra_neon__get_location_segments(svn_ra_session_t *session,
const char *path,
svn_revnum_t peg_revision,
svn_revnum_t start_rev,
svn_revnum_t end_rev,
svn_location_segment_receiver_t receiver,
void *receiver_baton,
apr_pool_t *pool);
svn_error_t *
svn_ra_neon__get_locks(svn_ra_session_t *session,
apr_hash_t **locks,
const char *path,
apr_pool_t *pool);
svn_error_t *
svn_ra_neon__lock(svn_ra_session_t *session,
apr_hash_t *path_revs,
const char *comment,
svn_boolean_t force,
svn_ra_lock_callback_t lock_func,
void *lock_baton,
apr_pool_t *pool);
svn_error_t *
svn_ra_neon__unlock(svn_ra_session_t *session,
apr_hash_t *path_tokens,
svn_boolean_t force,
svn_ra_lock_callback_t lock_func,
void *lock_baton,
apr_pool_t *pool);
svn_error_t *
svn_ra_neon__get_lock_internal(svn_ra_neon__session_t *session,
svn_lock_t **lock,
const char *path,
apr_pool_t *pool);
svn_error_t *
svn_ra_neon__get_lock(svn_ra_session_t *session,
svn_lock_t **lock,
const char *path,
apr_pool_t *pool);
svn_error_t *
svn_ra_neon__replay(svn_ra_session_t *session,
svn_revnum_t revision,
svn_revnum_t low_water_mark,
svn_boolean_t send_deltas,
const svn_delta_editor_t *editor,
void *edit_baton,
apr_pool_t *pool);
svn_error_t *
svn_ra_neon__replay_range(svn_ra_session_t *session,
svn_revnum_t start_revision,
svn_revnum_t end_revision,
svn_revnum_t low_water_mark,
svn_boolean_t send_deltas,
svn_ra_replay_revstart_callback_t revstart_func,
svn_ra_replay_revfinish_callback_t revfinish_func,
void *replay_baton,
apr_pool_t *pool);
svn_error_t *
svn_ra_neon__has_capability(svn_ra_session_t *session,
svn_boolean_t *has,
const char *capability,
apr_pool_t *pool);
svn_error_t *
svn_ra_neon__assemble_locktoken_body(svn_stringbuf_t **body,
apr_hash_t *lock_tokens,
apr_pool_t *pool);
#if defined(__cplusplus)
}
#endif
#endif
