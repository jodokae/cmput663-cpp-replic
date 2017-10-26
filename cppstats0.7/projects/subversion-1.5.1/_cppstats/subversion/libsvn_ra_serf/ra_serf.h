#if !defined(SVN_LIBSVN_RA_SERF_RA_SERF_H)
#define SVN_LIBSVN_RA_SERF_RA_SERF_H
#include <serf.h>
#include <expat.h>
#include <apr_uri.h>
#include "svn_types.h"
#include "svn_string.h"
#include "svn_pools.h"
#include "svn_ra.h"
#include "svn_delta.h"
#include "svn_version.h"
#include "svn_dav.h"
#include "private/svn_dav_protocol.h"
#define UNUSED_CTX(x) ((void)(x))
#define USER_AGENT "SVN/" SVN_VERSION " serf/" APR_STRINGIFY(SERF_MAJOR_VERSION) "." APR_STRINGIFY(SERF_MINOR_VERSION) "." APR_STRINGIFY(SERF_PATCH_VERSION)
#if defined(WIN32)
#if SERF_VERSION_AT_LEAST(0, 1, 3)
#define SVN_RA_SERF_SSPI_ENABLED
#endif
#endif
typedef struct svn_ra_serf__session_t svn_ra_serf__session_t;
typedef struct svn_ra_serf__auth_protocol_t svn_ra_serf__auth_protocol_t;
#if defined(SVN_RA_SERF_SSPI_ENABLED)
typedef struct serf_sspi_context_t serf_sspi_context_t;
#endif
typedef struct {
serf_connection_t *conn;
serf_bucket_alloc_t *bkt_alloc;
const char *hostinfo;
apr_sockaddr_t *address;
svn_boolean_t using_ssl;
svn_boolean_t using_compression;
int last_status_code;
const char *auth_header;
char *auth_value;
serf_ssl_context_t *ssl_context;
svn_auth_iterstate_t *ssl_client_auth_state;
svn_auth_iterstate_t *ssl_client_pw_auth_state;
svn_ra_serf__session_t *session;
#if defined(SVN_RA_SERF_SSPI_ENABLED)
serf_sspi_context_t *sspi_context;
#endif
const char *proxy_auth_header;
char *proxy_auth_value;
const char *useragent;
} svn_ra_serf__connection_t;
struct svn_ra_serf__session_t {
apr_pool_t *pool;
serf_context_t *context;
serf_bucket_alloc_t *bkt_alloc;
svn_boolean_t using_ssl;
svn_boolean_t using_compression;
svn_ra_serf__connection_t **conns;
int num_conns;
int cur_conn;
apr_uri_t repos_url;
const char *repos_url_str;
apr_uri_t repos_root;
const char *repos_root_str;
const char *vcc_url;
apr_hash_t *cached_props;
const char *realm;
const char *auth_header;
char *auth_value;
svn_auth_iterstate_t *auth_state;
int auth_attempts;
const svn_ra_callbacks2_t *wc_callbacks;
void *wc_callback_baton;
svn_ra_progress_notify_func_t wc_progress_func;
void *wc_progress_baton;
svn_error_t *pending_error;
const svn_ra_serf__auth_protocol_t *auth_protocol;
apr_hash_t *capabilities;
int using_proxy;
const char *proxy_auth_header;
char *proxy_auth_value;
const svn_ra_serf__auth_protocol_t *proxy_auth_protocol;
const char *proxy_username;
const char *proxy_password;
int proxy_auth_attempts;
svn_boolean_t trust_default_ca;
const char *ssl_authorities;
};
typedef struct {
const char *namespace;
const char *name;
} svn_ra_serf__dav_props_t;
typedef struct ns_t {
const char *namespace;
const char *url;
struct ns_t *next;
} svn_ra_serf__ns_t;
typedef struct ra_serf_list_t {
void *data;
struct ra_serf_list_t *next;
} svn_ra_serf__list_t;
static const svn_ra_serf__dav_props_t base_props[] = {
{ "DAV:", "version-controlled-configuration" },
{ "DAV:", "resourcetype" },
{ SVN_DAV_PROP_NS_DAV, "baseline-relative-path" },
{ SVN_DAV_PROP_NS_DAV, "repository-uuid" },
{ NULL }
};
static const svn_ra_serf__dav_props_t checked_in_props[] = {
{ "DAV:", "checked-in" },
{ NULL }
};
static const svn_ra_serf__dav_props_t baseline_props[] = {
{ "DAV:", "baseline-collection" },
{ "DAV:", SVN_DAV__VERSION_NAME },
{ NULL }
};
static const svn_ra_serf__dav_props_t all_props[] = {
{ "DAV:", "allprop" },
{ NULL }
};
static const svn_ra_serf__dav_props_t vcc_props[] = {
{ "DAV:", "version-controlled-configuration" },
{ NULL }
};
static const svn_ra_serf__dav_props_t check_path_props[] = {
{ "DAV:", "resourcetype" },
{ NULL }
};
static const svn_ra_serf__dav_props_t uuid_props[] = {
{ SVN_DAV_PROP_NS_DAV, "repository-uuid" },
{ NULL }
};
static const svn_ra_serf__dav_props_t repos_root_props[] = {
{ SVN_DAV_PROP_NS_DAV, "baseline-relative-path" },
{ NULL }
};
static const svn_ra_serf__dav_props_t href_props[] = {
{ "DAV:", "href" },
{ NULL }
};
#define SVN_RA_SERF__WC_NAMESPACE SVN_PROP_WC_PREFIX "ra_dav:"
#define SVN_RA_SERF__WC_ACTIVITY_URL SVN_RA_SERF__WC_NAMESPACE "activity-url"
#define SVN_RA_SERF__WC_CHECKED_IN_URL SVN_RA_SERF__WC_NAMESPACE "version-url"
serf_bucket_t *
svn_ra_serf__conn_setup(apr_socket_t *sock,
void *baton,
apr_pool_t *pool);
serf_bucket_t*
svn_ra_serf__accept_response(serf_request_t *request,
serf_bucket_t *stream,
void *acceptor_baton,
apr_pool_t *pool);
void
svn_ra_serf__conn_closed(serf_connection_t *conn,
void *closed_baton,
apr_status_t why,
apr_pool_t *pool);
apr_status_t
svn_ra_serf__is_conn_closing(serf_bucket_t *response);
apr_status_t
svn_ra_serf__cleanup_serf_session(void *data);
apr_status_t
svn_ra_serf__handle_client_cert(void *data,
const char **cert_path);
apr_status_t
svn_ra_serf__handle_client_cert_pw(void *data,
const char *cert_path,
const char **password);
void
svn_ra_serf__setup_serf_req(serf_request_t *request,
serf_bucket_t **req_bkt, serf_bucket_t **hdrs_bkt,
svn_ra_serf__connection_t *conn,
const char *method, const char *url,
serf_bucket_t *body_bkt, const char *content_type);
svn_error_t *
svn_ra_serf__context_run_wait(svn_boolean_t *done,
svn_ra_serf__session_t *sess,
apr_pool_t *pool);
typedef serf_bucket_t*
(*svn_ra_serf__request_body_delegate_t)(void *baton,
serf_bucket_alloc_t *alloc,
apr_pool_t *pool);
typedef apr_status_t
(*svn_ra_serf__request_header_delegate_t)(serf_bucket_t *headers,
void *baton,
apr_pool_t *pool);
typedef apr_status_t
(*svn_ra_serf__response_error_t)(serf_request_t *request,
serf_bucket_t *response,
int status_code,
void *baton);
typedef struct {
const char *method;
const char *path;
serf_bucket_t *body_buckets;
const char *body_type;
serf_response_handler_t response_handler;
void *response_baton;
svn_ra_serf__response_error_t response_error;
void *response_error_baton;
serf_request_setup_t delegate;
void *delegate_baton;
svn_ra_serf__request_header_delegate_t header_delegate;
void *header_delegate_baton;
svn_ra_serf__request_body_delegate_t body_delegate;
void *body_delegate_baton;
svn_ra_serf__connection_t *conn;
svn_ra_serf__session_t *session;
} svn_ra_serf__handler_t;
serf_request_t*
svn_ra_serf__request_create(svn_ra_serf__handler_t *handler);
serf_request_t*
svn_ra_serf__priority_request_create(svn_ra_serf__handler_t *handler);
typedef struct svn_ra_serf__xml_state_t {
int current_state;
void *private;
apr_pool_t *pool;
svn_ra_serf__ns_t *ns_list;
struct svn_ra_serf__xml_state_t *prev;
} svn_ra_serf__xml_state_t;
typedef struct svn_ra_serf__xml_parser_t svn_ra_serf__xml_parser_t;
typedef svn_error_t *
(*svn_ra_serf__xml_start_element_t)(svn_ra_serf__xml_parser_t *parser,
void *baton,
svn_ra_serf__dav_props_t name,
const char **attrs);
typedef svn_error_t *
(*svn_ra_serf__xml_end_element_t)(svn_ra_serf__xml_parser_t *parser,
void *baton,
svn_ra_serf__dav_props_t name);
typedef svn_error_t *
(*svn_ra_serf__xml_cdata_chunk_handler_t)(svn_ra_serf__xml_parser_t *parser,
void *baton,
const char *data,
apr_size_t len);
struct svn_ra_serf__xml_parser_t {
apr_pool_t *pool;
void *user_data;
svn_ra_serf__xml_start_element_t start;
svn_ra_serf__xml_end_element_t end;
svn_ra_serf__xml_cdata_chunk_handler_t cdata;
XML_Parser xmlp;
svn_ra_serf__xml_state_t *state;
svn_ra_serf__xml_state_t *free_state;
int *status_code;
svn_boolean_t *done;
svn_ra_serf__list_t **done_list;
svn_ra_serf__list_t *done_item;
svn_boolean_t ignore_errors;
svn_error_t *error;
};
typedef struct {
svn_error_t *error;
svn_boolean_t init;
svn_boolean_t has_xml_response;
svn_boolean_t done;
svn_boolean_t in_error;
svn_boolean_t collect_cdata;
svn_stringbuf_t *cdata;
svn_ra_serf__xml_parser_t parser;
} svn_ra_serf__server_error_t;
typedef struct {
int status;
const char *reason;
svn_boolean_t done;
svn_ra_serf__server_error_t server_error;
} svn_ra_serf__simple_request_context_t;
apr_status_t
svn_ra_serf__handle_status_only(serf_request_t *request,
serf_bucket_t *response,
void *baton,
apr_pool_t *pool);
apr_status_t
svn_ra_serf__handle_discard_body(serf_request_t *request,
serf_bucket_t *response,
void *baton,
apr_pool_t *pool);
svn_error_t *
svn_ra_serf__handle_server_error(serf_request_t *request,
serf_bucket_t *response,
apr_pool_t *pool);
apr_status_t
svn_ra_serf__handle_multistatus_only(serf_request_t *request,
serf_bucket_t *response,
void *baton,
apr_pool_t *pool);
apr_status_t
svn_ra_serf__handle_xml_parser(serf_request_t *request,
serf_bucket_t *response,
void *handler_baton,
apr_pool_t *pool);
void
svn_ra_serf__xml_push_state(svn_ra_serf__xml_parser_t *parser,
int state);
void
svn_ra_serf__xml_pop_state(svn_ra_serf__xml_parser_t *parser);
void
svn_ra_serf__add_tag_buckets(serf_bucket_t *agg_bucket,
const char *tag,
const char *value,
serf_bucket_alloc_t *bkt_alloc);
void
svn_ra_serf__define_ns(svn_ra_serf__ns_t **ns_list,
const char **attrs,
apr_pool_t *pool);
svn_ra_serf__dav_props_t
svn_ra_serf__expand_ns(svn_ra_serf__ns_t *ns_list,
const char *name);
void
svn_ra_serf__expand_string(const char **cur, apr_size_t *cur_len,
const char *new, apr_size_t new_len,
apr_pool_t *pool);
typedef struct svn_ra_serf__propfind_context_t svn_ra_serf__propfind_context_t;
svn_boolean_t
svn_ra_serf__propfind_is_done(svn_ra_serf__propfind_context_t *ctx);
int
svn_ra_serf__propfind_status_code(svn_ra_serf__propfind_context_t *ctx);
serf_bucket_t *
svn_ra_serf__bucket_propfind_create(svn_ra_serf__connection_t *conn,
const char *path,
const char *label,
const char *depth,
const svn_ra_serf__dav_props_t *find_props,
serf_bucket_alloc_t *allocator);
svn_error_t *
svn_ra_serf__deliver_props(svn_ra_serf__propfind_context_t **prop_ctx,
apr_hash_t *prop_vals,
svn_ra_serf__session_t *sess,
svn_ra_serf__connection_t *conn,
const char *url,
svn_revnum_t rev,
const char *depth,
const svn_ra_serf__dav_props_t *lookup_props,
svn_boolean_t cache_props,
svn_ra_serf__list_t **done_list,
apr_pool_t *pool);
svn_error_t *
svn_ra_serf__wait_for_props(svn_ra_serf__propfind_context_t *prop_ctx,
svn_ra_serf__session_t *sess,
apr_pool_t *pool);
svn_error_t *
svn_ra_serf__search_for_base_props(apr_hash_t *props,
const char **remaining_path,
const char **missing_path,
svn_ra_serf__session_t *session,
svn_ra_serf__connection_t *conn,
const char *url,
apr_pool_t *pool);
svn_error_t *
svn_ra_serf__retrieve_props(apr_hash_t *prop_vals,
svn_ra_serf__session_t *sess,
svn_ra_serf__connection_t *conn,
const char *url,
svn_revnum_t rev,
const char *depth,
const svn_ra_serf__dav_props_t *props,
apr_pool_t *pool);
void
svn_ra_serf__set_ver_prop(apr_hash_t *props,
const char *path, svn_revnum_t rev,
const char *ns, const char *name,
const svn_string_t *val, apr_pool_t *pool);
typedef svn_error_t *
(*svn_ra_serf__walker_visitor_t)(void *baton,
const char *ns, apr_ssize_t ns_len,
const char *name, apr_ssize_t name_len,
const svn_string_t *val,
apr_pool_t *pool);
void
svn_ra_serf__walk_all_props(apr_hash_t *props,
const char *name,
svn_revnum_t rev,
svn_ra_serf__walker_visitor_t walker,
void *baton,
apr_pool_t *pool);
typedef svn_error_t *
(*svn_ra_serf__path_rev_walker_t)(void *baton,
const char *path, apr_ssize_t path_len,
const char *ns, apr_ssize_t ns_len,
const char *name, apr_ssize_t name_len,
const svn_string_t *val,
apr_pool_t *pool);
void
svn_ra_serf__walk_all_paths(apr_hash_t *props,
svn_revnum_t rev,
svn_ra_serf__path_rev_walker_t walker,
void *baton,
apr_pool_t *pool);
typedef svn_error_t * (*svn_ra_serf__prop_set_t)(void *baton,
const char *name,
const svn_string_t *value,
apr_pool_t *pool);
svn_error_t *
svn_ra_serf__set_baton_props(svn_ra_serf__prop_set_t setprop, void *baton,
const char *ns, apr_ssize_t ns_len,
const char *name, apr_ssize_t name_len,
const svn_string_t *val,
apr_pool_t *pool);
svn_error_t *
svn_ra_serf__set_flat_props(void *baton,
const char *ns, apr_ssize_t ns_len,
const char *name, apr_ssize_t name_len,
const svn_string_t *val,
apr_pool_t *pool);
svn_error_t *
svn_ra_serf__set_bare_props(void *baton,
const char *ns, apr_ssize_t ns_len,
const char *name, apr_ssize_t name_len,
const svn_string_t *val,
apr_pool_t *pool);
const svn_string_t *
svn_ra_serf__get_ver_prop_string(apr_hash_t *props,
const char *path, svn_revnum_t rev,
const char *ns, const char *name);
const char *
svn_ra_serf__get_ver_prop(apr_hash_t *props,
const char *path, svn_revnum_t rev,
const char *ns, const char *name);
const char *
svn_ra_serf__get_prop(apr_hash_t *props,
const char *path,
const char *ns,
const char *name);
void
svn_ra_serf__set_rev_prop(apr_hash_t *props,
const char *path, svn_revnum_t rev,
const char *ns, const char *name,
const svn_string_t *val, apr_pool_t *pool);
void
svn_ra_serf__set_prop(apr_hash_t *props, const char *path,
const char *ns, const char *name,
const svn_string_t *val, apr_pool_t *pool);
typedef struct svn_ra_serf__merge_context_t svn_ra_serf__merge_context_t;
svn_boolean_t*
svn_ra_serf__merge_get_done_ptr(svn_ra_serf__merge_context_t *ctx);
svn_commit_info_t*
svn_ra_serf__merge_get_commit_info(svn_ra_serf__merge_context_t *ctx);
int
svn_ra_serf__merge_get_status(svn_ra_serf__merge_context_t *ctx);
void
svn_ra_serf__merge_lock_token_list(apr_hash_t *lock_tokens,
const char *parent,
serf_bucket_t *body,
serf_bucket_alloc_t *alloc,
apr_pool_t *pool);
svn_error_t *
svn_ra_serf__merge_create_req(svn_ra_serf__merge_context_t **merge_ctx,
svn_ra_serf__session_t *session,
svn_ra_serf__connection_t *conn,
const char *path,
const char *activity_url,
apr_size_t activity_url_len,
apr_hash_t *lock_tokens,
svn_boolean_t keep_locks,
apr_pool_t *pool);
typedef struct svn_ra_serf__options_context_t svn_ra_serf__options_context_t;
svn_boolean_t*
svn_ra_serf__get_options_done_ptr(svn_ra_serf__options_context_t *ctx);
const char *
svn_ra_serf__options_get_activity_collection(svn_ra_serf__options_context_t *ctx);
svn_error_t *
svn_ra_serf__get_options_error(svn_ra_serf__options_context_t *ctx);
svn_error_t *
svn_ra_serf__get_options_parser_error(svn_ra_serf__options_context_t *ctx);
svn_error_t *
svn_ra_serf__create_options_req(svn_ra_serf__options_context_t **opt_ctx,
svn_ra_serf__session_t *session,
svn_ra_serf__connection_t *conn,
const char *path,
apr_pool_t *pool);
svn_error_t *
svn_ra_serf__discover_root(const char **vcc_url,
const char **rel_path,
svn_ra_serf__session_t *session,
svn_ra_serf__connection_t *conn,
const char *orig_path,
apr_pool_t *pool);
svn_error_t *
svn_ra_serf__get_baseline_info(const char **bc_url,
const char **bc_relative,
svn_ra_serf__session_t *session,
const char *url,
svn_revnum_t revision,
apr_pool_t *pool);
svn_error_t *
svn_ra_serf__get_log(svn_ra_session_t *session,
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
svn_error_t *
svn_ra_serf__get_locations(svn_ra_session_t *session,
apr_hash_t **locations,
const char *path,
svn_revnum_t peg_revision,
apr_array_header_t *location_revisions,
apr_pool_t *pool);
svn_error_t *
svn_ra_serf__get_location_segments(svn_ra_session_t *session,
const char *path,
svn_revnum_t peg_revision,
svn_revnum_t start_rev,
svn_revnum_t end_rev,
svn_location_segment_receiver_t receiver,
void *receiver_baton,
apr_pool_t *pool);
svn_error_t *
svn_ra_serf__do_diff(svn_ra_session_t *session,
const svn_ra_reporter3_t **reporter,
void **report_baton,
svn_revnum_t revision,
const char *diff_target,
svn_depth_t depth,
svn_boolean_t ignore_ancestry,
svn_boolean_t text_deltas,
const char *versus_url,
const svn_delta_editor_t *diff_editor,
void *diff_baton,
apr_pool_t *pool);
svn_error_t *
svn_ra_serf__do_status(svn_ra_session_t *ra_session,
const svn_ra_reporter3_t **reporter,
void **report_baton,
const char *status_target,
svn_revnum_t revision,
svn_depth_t depth,
const svn_delta_editor_t *status_editor,
void *status_baton,
apr_pool_t *pool);
svn_error_t *
svn_ra_serf__do_update(svn_ra_session_t *ra_session,
const svn_ra_reporter3_t **reporter,
void **report_baton,
svn_revnum_t revision_to_update_to,
const char *update_target,
svn_depth_t depth,
svn_boolean_t send_copyfrom_args,
const svn_delta_editor_t *update_editor,
void *update_baton,
apr_pool_t *pool);
svn_error_t *
svn_ra_serf__do_switch(svn_ra_session_t *ra_session,
const svn_ra_reporter3_t **reporter,
void **report_baton,
svn_revnum_t revision_to_switch_to,
const char *switch_target,
svn_depth_t depth,
const char *switch_url,
const svn_delta_editor_t *switch_editor,
void *switch_baton,
apr_pool_t *pool);
svn_error_t *
svn_ra_serf__get_file_revs(svn_ra_session_t *session,
const char *path,
svn_revnum_t start,
svn_revnum_t end,
svn_boolean_t include_merged_revisions,
svn_file_rev_handler_t handler,
void *handler_baton,
apr_pool_t *pool);
svn_error_t *
svn_ra_serf__get_dated_revision(svn_ra_session_t *session,
svn_revnum_t *revision,
apr_time_t tm,
apr_pool_t *pool);
svn_error_t *
svn_ra_serf__get_commit_editor(svn_ra_session_t *session,
const svn_delta_editor_t **editor,
void **edit_baton,
apr_hash_t *revprop_table,
svn_commit_callback2_t callback,
void *callback_baton,
apr_hash_t *lock_tokens,
svn_boolean_t keep_locks,
apr_pool_t *pool);
svn_error_t *
svn_ra_serf__get_file(svn_ra_session_t *session,
const char *path,
svn_revnum_t revision,
svn_stream_t *stream,
svn_revnum_t *fetched_rev,
apr_hash_t **props,
apr_pool_t *pool);
svn_error_t *
svn_ra_serf__change_rev_prop(svn_ra_session_t *session,
svn_revnum_t rev,
const char *name,
const svn_string_t *value,
apr_pool_t *pool);
svn_error_t *
svn_ra_serf__replay(svn_ra_session_t *ra_session,
svn_revnum_t revision,
svn_revnum_t low_water_mark,
svn_boolean_t text_deltas,
const svn_delta_editor_t *editor,
void *edit_baton,
apr_pool_t *pool);
svn_error_t *
svn_ra_serf__replay_range(svn_ra_session_t *ra_session,
svn_revnum_t start_revision,
svn_revnum_t end_revision,
svn_revnum_t low_water_mark,
svn_boolean_t send_deltas,
svn_ra_replay_revstart_callback_t revstart_func,
svn_ra_replay_revfinish_callback_t revfinish_func,
void *replay_baton,
apr_pool_t *pool);
svn_error_t *
svn_ra_serf__lock(svn_ra_session_t *ra_session,
apr_hash_t *path_revs,
const char *comment,
svn_boolean_t force,
svn_ra_lock_callback_t lock_func,
void *lock_baton,
apr_pool_t *pool);
svn_error_t *
svn_ra_serf__unlock(svn_ra_session_t *ra_session,
apr_hash_t *path_tokens,
svn_boolean_t force,
svn_ra_lock_callback_t lock_func,
void *lock_baton,
apr_pool_t *pool);
svn_error_t *
svn_ra_serf__get_lock(svn_ra_session_t *ra_session,
svn_lock_t **lock,
const char *path,
apr_pool_t *pool);
svn_error_t *
svn_ra_serf__get_locks(svn_ra_session_t *ra_session,
apr_hash_t **locks,
const char *path,
apr_pool_t *pool);
svn_error_t * svn_ra_serf__get_mergeinfo(svn_ra_session_t *ra_session,
apr_hash_t **mergeinfo,
const apr_array_header_t *paths,
svn_revnum_t revision,
svn_mergeinfo_inheritance_t inherit,
svn_boolean_t include_descendants,
apr_pool_t *pool);
svn_error_t *
svn_ra_serf__has_capability(svn_ra_session_t *ra_session,
svn_boolean_t *has,
const char *capability,
apr_pool_t *pool);
typedef svn_error_t *
(*svn_serf__auth_handler_func_t)(svn_ra_serf__session_t *session,
svn_ra_serf__connection_t *conn,
serf_request_t *request,
serf_bucket_t *response,
char *auth_hdr,
char *auth_attr,
apr_pool_t *pool);
typedef svn_error_t *
(*svn_serf__init_conn_func_t)(svn_ra_serf__session_t *session,
svn_ra_serf__connection_t *conn,
apr_pool_t *pool);
typedef svn_error_t *
(*svn_serf__setup_request_func_t)(svn_ra_serf__connection_t *conn,
serf_bucket_t *hdrs_bkt);
struct svn_ra_serf__auth_protocol_t {
int code;
const char *auth_name;
svn_serf__init_conn_func_t init_conn_func;
svn_serf__auth_handler_func_t handle_func;
svn_serf__setup_request_func_t setup_request_func;
};
svn_error_t *
svn_ra_serf__handle_auth(int code,
svn_ra_serf__session_t *session,
svn_ra_serf__connection_t *conn,
serf_request_t *request,
serf_bucket_t *response,
apr_pool_t *pool);
void
svn_ra_serf__encode_auth_header(const char * protocol,
char **header,
const char * data,
apr_size_t data_len,
apr_pool_t *pool);
#endif
