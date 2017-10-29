#if !defined(_MOD_DAV_H_)
#define _MOD_DAV_H_
#include "apr_hooks.h"
#include "apr_hash.h"
#include "apr_dbm.h"
#include "apr_tables.h"
#include "httpd.h"
#include "util_filter.h"
#include "util_xml.h"
#include <limits.h>
#include <time.h>
#if defined(__cplusplus)
extern "C" {
#endif
#define DAV_VERSION AP_SERVER_BASEREVISION
#define DAV_XML_HEADER "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
#define DAV_XML_CONTENT_TYPE "text/xml; charset=\"utf-8\""
#define DAV_READ_BLOCKSIZE 2048
#define DAV_RESPONSE_BODY_1 "<!DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML 2.0//EN\">\n<html><head>\n<title>"
#define DAV_RESPONSE_BODY_2 "</title>\n</head><body>\n<h1>"
#define DAV_RESPONSE_BODY_3 "</h1>\n<p>"
#define DAV_RESPONSE_BODY_4 "</p>\n"
#define DAV_RESPONSE_BODY_5 "</body></html>\n"
#define DAV_DO_COPY 0
#define DAV_DO_MOVE 1
#if 1
#define DAV_DEBUG 1
#define DEBUG_CR "\n"
#define DBG0(f) ap_log_error(APLOG_MARK, APLOG_ERR, 0, NULL, (f))
#define DBG1(f,a1) ap_log_error(APLOG_MARK, APLOG_ERR, 0, NULL, f, a1)
#define DBG2(f,a1,a2) ap_log_error(APLOG_MARK, APLOG_ERR, 0, NULL, f, a1, a2)
#define DBG3(f,a1,a2,a3) ap_log_error(APLOG_MARK, APLOG_ERR, 0, NULL, f, a1, a2, a3)
#else
#undef DAV_DEBUG
#define DEBUG_CR ""
#endif
#define DAV_INFINITY INT_MAX
#if !defined(WIN32)
#define DAV_DECLARE(type) type
#define DAV_DECLARE_NONSTD(type) type
#define DAV_DECLARE_DATA
#elif defined(DAV_DECLARE_STATIC)
#define DAV_DECLARE(type) type __stdcall
#define DAV_DECLARE_NONSTD(type) type
#define DAV_DECLARE_DATA
#elif defined(DAV_DECLARE_EXPORT)
#define DAV_DECLARE(type) __declspec(dllexport) type __stdcall
#define DAV_DECLARE_NONSTD(type) __declspec(dllexport) type
#define DAV_DECLARE_DATA __declspec(dllexport)
#else
#define DAV_DECLARE(type) __declspec(dllimport) type __stdcall
#define DAV_DECLARE_NONSTD(type) __declspec(dllimport) type
#define DAV_DECLARE_DATA __declspec(dllimport)
#endif
typedef struct dav_error {
int status;
int error_id;
const char *desc;
apr_status_t aprerr;
const char *namespace;
const char *tagname;
struct dav_error *prev;
const char *childtags;
} dav_error;
DAV_DECLARE(dav_error*) dav_new_error(apr_pool_t *p, int status,
int error_id, apr_status_t aprerr,
const char *desc);
DAV_DECLARE(dav_error*) dav_new_error_tag(apr_pool_t *p, int status,
int error_id, apr_status_t aprerr,
const char *desc,
const char *namespace,
const char *tagname);
DAV_DECLARE(dav_error*) dav_push_error(apr_pool_t *p, int status, int error_id,
const char *desc, dav_error *prev);
DAV_DECLARE(dav_error*) dav_join_error(dav_error* dest, dav_error* src);
typedef struct dav_response dav_response;
DAV_DECLARE(int) dav_handle_err(request_rec *r, dav_error *err,
dav_response *response);
#define DAV_ERR_IF_PARSE 100
#define DAV_ERR_IF_MULTIPLE_NOT 101
#define DAV_ERR_IF_UNK_CHAR 102
#define DAV_ERR_IF_ABSENT 103
#define DAV_ERR_IF_TAGGED 104
#define DAV_ERR_IF_UNCLOSED_PAREN 105
#define DAV_ERR_PROP_BAD_MAJOR 200
#define DAV_ERR_PROP_READONLY 201
#define DAV_ERR_PROP_NO_DATABASE 202
#define DAV_ERR_PROP_NOT_FOUND 203
#define DAV_ERR_PROP_BAD_LOCKDB 204
#define DAV_ERR_PROP_OPENING 205
#define DAV_ERR_PROP_EXEC 206
#define DAV_ERR_LOCK_OPENDB 400
#define DAV_ERR_LOCK_NO_DB 401
#define DAV_ERR_LOCK_CORRUPT_DB 402
#define DAV_ERR_LOCK_UNK_STATE_TOKEN 403
#define DAV_ERR_LOCK_PARSE_TOKEN 404
#define DAV_ERR_LOCK_SAVE_LOCK 405
typedef struct dav_hooks_propdb dav_hooks_propdb;
typedef struct dav_hooks_locks dav_hooks_locks;
typedef struct dav_hooks_vsn dav_hooks_vsn;
typedef struct dav_hooks_repository dav_hooks_repository;
typedef struct dav_hooks_liveprop dav_hooks_liveprop;
typedef struct dav_hooks_binding dav_hooks_binding;
typedef struct dav_hooks_search dav_hooks_search;
typedef dav_hooks_propdb dav_hooks_db;
typedef enum {
DAV_RESOURCE_TYPE_UNKNOWN,
DAV_RESOURCE_TYPE_REGULAR,
DAV_RESOURCE_TYPE_VERSION,
DAV_RESOURCE_TYPE_HISTORY,
DAV_RESOURCE_TYPE_WORKING,
DAV_RESOURCE_TYPE_WORKSPACE,
DAV_RESOURCE_TYPE_ACTIVITY,
DAV_RESOURCE_TYPE_PRIVATE
} dav_resource_type;
typedef struct dav_resource_private dav_resource_private;
typedef struct dav_resource {
dav_resource_type type;
int exists;
int collection;
int versioned;
int baselined;
int working;
const char *uri;
dav_resource_private *info;
const dav_hooks_repository *hooks;
apr_pool_t *pool;
} dav_resource;
typedef struct dav_locktoken dav_locktoken;
typedef struct {
apr_size_t alloc_len;
apr_size_t cur_len;
char *buf;
} dav_buffer;
#define DAV_BUFFER_MINSIZE 256
#define DAV_BUFFER_PAD 64
DAV_DECLARE(void) dav_set_bufsize(apr_pool_t *p, dav_buffer *pbuf,
apr_size_t size);
DAV_DECLARE(void) dav_buffer_init(apr_pool_t *p, dav_buffer *pbuf,
const char *str);
DAV_DECLARE(void) dav_check_bufsize(apr_pool_t *p, dav_buffer *pbuf,
apr_size_t extra_needed);
DAV_DECLARE(void) dav_buffer_append(apr_pool_t *p, dav_buffer *pbuf,
const char *str);
DAV_DECLARE(void) dav_buffer_place(apr_pool_t *p, dav_buffer *pbuf,
const char *str);
DAV_DECLARE(void) dav_buffer_place_mem(apr_pool_t *p, dav_buffer *pbuf,
const void *mem, apr_size_t amt,
apr_size_t pad);
typedef struct {
apr_text * propstats;
apr_text * xmlns;
} dav_get_props_result;
struct dav_response {
const char *href;
const char *desc;
dav_get_props_result propresult;
int status;
struct dav_response *next;
};
typedef struct {
request_rec *rnew;
dav_error err;
} dav_lookup_result;
DAV_DECLARE(dav_lookup_result) dav_lookup_uri(const char *uri, request_rec *r,
int must_be_absolute);
typedef enum {
DAV_PROP_INSERT_NOTDEF,
DAV_PROP_INSERT_NOTSUPP,
DAV_PROP_INSERT_NAME,
DAV_PROP_INSERT_VALUE,
DAV_PROP_INSERT_SUPPORTED
} dav_prop_insert;
#define DAV_STYLE_ISO8601 1
#define DAV_STYLE_RFC822 2
#define DAV_TIMEBUF_SIZE 30
DAV_DECLARE(void) dav_send_one_response(dav_response *response,
apr_bucket_brigade *bb,
request_rec *r,
apr_pool_t *pool);
DAV_DECLARE(void) dav_begin_multistatus(apr_bucket_brigade *bb,
request_rec *r, int status,
apr_array_header_t *namespaces);
DAV_DECLARE(apr_status_t) dav_finish_multistatus(request_rec *r,
apr_bucket_brigade *bb);
DAV_DECLARE(void) dav_send_multistatus(request_rec *r, int status,
dav_response *first,
apr_array_header_t *namespaces);
DAV_DECLARE(apr_text *) dav_failed_proppatch(apr_pool_t *p,
apr_array_header_t *prop_ctx);
DAV_DECLARE(apr_text *) dav_success_proppatch(apr_pool_t *p,
apr_array_header_t *prop_ctx);
DAV_DECLARE(int) dav_get_depth(request_rec *r, int def_depth);
DAV_DECLARE(int) dav_validate_root(const apr_xml_doc *doc,
const char *tagname);
DAV_DECLARE(apr_xml_elem *) dav_find_child(const apr_xml_elem *elem,
const char *tagname);
DAV_DECLARE(const char *) dav_xml_get_cdata(const apr_xml_elem *elem, apr_pool_t *pool,
int strip_white);
typedef struct {
apr_pool_t *pool;
apr_hash_t *uri_prefix;
apr_hash_t *prefix_uri;
int count;
} dav_xmlns_info;
DAV_DECLARE(dav_xmlns_info *) dav_xmlns_create(apr_pool_t *pool);
DAV_DECLARE(void) dav_xmlns_add(dav_xmlns_info *xi,
const char *prefix, const char *uri);
DAV_DECLARE(const char *) dav_xmlns_add_uri(dav_xmlns_info *xi,
const char *uri);
DAV_DECLARE(const char *) dav_xmlns_get_uri(dav_xmlns_info *xi,
const char *prefix);
DAV_DECLARE(const char *) dav_xmlns_get_prefix(dav_xmlns_info *xi,
const char *uri);
DAV_DECLARE(void) dav_xmlns_generate(dav_xmlns_info *xi,
apr_text_header *phdr);
typedef struct {
const dav_hooks_repository *repos;
const dav_hooks_propdb *propdb;
const dav_hooks_locks *locks;
const dav_hooks_vsn *vsn;
const dav_hooks_binding *binding;
const dav_hooks_search *search;
void *ctx;
} dav_provider;
APR_DECLARE_EXTERNAL_HOOK(dav, DAV, void, gather_propsets,
(apr_array_header_t *uris))
APR_DECLARE_EXTERNAL_HOOK(dav, DAV, int, find_liveprop,
(const dav_resource *resource,
const char *ns_uri, const char *name,
const dav_hooks_liveprop **hooks))
APR_DECLARE_EXTERNAL_HOOK(dav, DAV, void, insert_all_liveprops,
(request_rec *r, const dav_resource *resource,
dav_prop_insert what, apr_text_header *phdr))
DAV_DECLARE(const dav_hooks_locks *) dav_get_lock_hooks(request_rec *r);
DAV_DECLARE(const dav_hooks_propdb *) dav_get_propdb_hooks(request_rec *r);
DAV_DECLARE(const dav_hooks_vsn *) dav_get_vsn_hooks(request_rec *r);
DAV_DECLARE(const dav_hooks_binding *) dav_get_binding_hooks(request_rec *r);
DAV_DECLARE(const dav_hooks_search *) dav_get_search_hooks(request_rec *r);
DAV_DECLARE(void) dav_register_provider(apr_pool_t *p, const char *name,
const dav_provider *hooks);
DAV_DECLARE(const dav_provider *) dav_lookup_provider(const char *name);
DAV_DECLARE(const char *) dav_get_provider_name(request_rec *r);
#define DAV_GET_HOOKS_PROPDB(r) dav_get_propdb_hooks(r)
#define DAV_GET_HOOKS_LOCKS(r) dav_get_lock_hooks(r)
#define DAV_GET_HOOKS_VSN(r) dav_get_vsn_hooks(r)
#define DAV_GET_HOOKS_BINDING(r) dav_get_binding_hooks(r)
#define DAV_GET_HOOKS_SEARCH(r) dav_get_search_hooks(r)
typedef enum {
dav_if_etag,
dav_if_opaquelock,
dav_if_unknown
} dav_if_state_type;
typedef struct dav_if_state_list {
dav_if_state_type type;
int condition;
#define DAV_IF_COND_NORMAL 0
#define DAV_IF_COND_NOT 1
const char *etag;
dav_locktoken *locktoken;
struct dav_if_state_list *next;
} dav_if_state_list;
typedef struct dav_if_header {
const char *uri;
apr_size_t uri_len;
struct dav_if_state_list *state;
struct dav_if_header *next;
int dummy_header;
} dav_if_header;
typedef struct dav_locktoken_list {
dav_locktoken *locktoken;
struct dav_locktoken_list *next;
} dav_locktoken_list;
DAV_DECLARE(dav_error *) dav_get_locktoken_list(request_rec *r,
dav_locktoken_list **ltl);
typedef struct dav_liveprop_rollback dav_liveprop_rollback;
struct dav_hooks_liveprop {
dav_prop_insert (*insert_prop)(const dav_resource *resource,
int propid, dav_prop_insert what,
apr_text_header *phdr);
int (*is_writable)(const dav_resource *resource, int propid);
const char * const * namespace_uris;
dav_error * (*patch_validate)(const dav_resource *resource,
const apr_xml_elem *elem,
int operation,
void **context,
int *defer_to_dead);
dav_error * (*patch_exec)(const dav_resource *resource,
const apr_xml_elem *elem,
int operation,
void *context,
dav_liveprop_rollback **rollback_ctx);
void (*patch_commit)(const dav_resource *resource,
int operation,
void *context,
dav_liveprop_rollback *rollback_ctx);
dav_error * (*patch_rollback)(const dav_resource *resource,
int operation,
void *context,
dav_liveprop_rollback *rollback_ctx);
void *ctx;
};
typedef struct {
int ns;
const char *name;
int propid;
int is_writable;
} dav_liveprop_spec;
typedef struct {
const dav_liveprop_spec *specs;
const char * const *namespace_uris;
const dav_hooks_liveprop *hooks;
} dav_liveprop_group;
DAV_DECLARE(int) dav_do_find_liveprop(const char *ns_uri, const char *name,
const dav_liveprop_group *group,
const dav_hooks_liveprop **hooks);
DAV_DECLARE(long) dav_get_liveprop_info(int propid,
const dav_liveprop_group *group,
const dav_liveprop_spec **info);
DAV_DECLARE(void) dav_register_liveprop_group(apr_pool_t *pool,
const dav_liveprop_group *group);
DAV_DECLARE(long) dav_get_liveprop_ns_index(const char *uri);
DAV_DECLARE(long) dav_get_liveprop_ns_count(void);
DAV_DECLARE(void) dav_add_all_liveprop_xmlns(apr_pool_t *p,
apr_text_header *phdr);
DAV_DECLARE_NONSTD(int) dav_core_find_liveprop(
const dav_resource *resource,
const char *ns_uri,
const char *name,
const dav_hooks_liveprop **hooks);
DAV_DECLARE_NONSTD(void) dav_core_insert_all_liveprops(
request_rec *r,
const dav_resource *resource,
dav_prop_insert what,
apr_text_header *phdr);
DAV_DECLARE_NONSTD(void) dav_core_register_uris(apr_pool_t *p);
enum {
DAV_PROPID_BEGIN = 20000,
DAV_PROPID_creationdate,
DAV_PROPID_displayname,
DAV_PROPID_getcontentlanguage,
DAV_PROPID_getcontentlength,
DAV_PROPID_getcontenttype,
DAV_PROPID_getetag,
DAV_PROPID_getlastmodified,
DAV_PROPID_lockdiscovery,
DAV_PROPID_resourcetype,
DAV_PROPID_source,
DAV_PROPID_supportedlock,
DAV_PROPID_activity_checkout_set,
DAV_PROPID_activity_set,
DAV_PROPID_activity_version_set,
DAV_PROPID_auto_merge_set,
DAV_PROPID_auto_version,
DAV_PROPID_baseline_collection,
DAV_PROPID_baseline_controlled_collection,
DAV_PROPID_baseline_controlled_collection_set,
DAV_PROPID_checked_in,
DAV_PROPID_checked_out,
DAV_PROPID_checkin_fork,
DAV_PROPID_checkout_fork,
DAV_PROPID_checkout_set,
DAV_PROPID_comment,
DAV_PROPID_creator_displayname,
DAV_PROPID_current_activity_set,
DAV_PROPID_current_workspace_set,
DAV_PROPID_default_variant,
DAV_PROPID_eclipsed_set,
DAV_PROPID_label_name_set,
DAV_PROPID_merge_set,
DAV_PROPID_precursor_set,
DAV_PROPID_predecessor_set,
DAV_PROPID_root_version,
DAV_PROPID_subactivity_set,
DAV_PROPID_subbaseline_set,
DAV_PROPID_successor_set,
DAV_PROPID_supported_method_set,
DAV_PROPID_supported_live_property_set,
DAV_PROPID_supported_report_set,
DAV_PROPID_unreserved,
DAV_PROPID_variant_set,
DAV_PROPID_version_controlled_binding_set,
DAV_PROPID_version_controlled_configuration,
DAV_PROPID_version_history,
DAV_PROPID_version_name,
DAV_PROPID_workspace,
DAV_PROPID_workspace_checkout_set,
DAV_PROPID_END
};
#define DAV_PROPID_CORE 10000
#define DAV_PROPID_FS 10100
#define DAV_PROPID_TEST1 10300
#define DAV_PROPID_TEST2 10400
#define DAV_PROPID_TEST3 10500
typedef struct dav_db dav_db;
typedef struct dav_namespace_map dav_namespace_map;
typedef struct dav_deadprop_rollback dav_deadprop_rollback;
typedef struct {
const char *ns;
const char *name;
} dav_prop_name;
struct dav_hooks_propdb {
dav_error * (*open)(apr_pool_t *p, const dav_resource *resource, int ro,
dav_db **pdb);
void (*close)(dav_db *db);
dav_error * (*define_namespaces)(dav_db *db, dav_xmlns_info *xi);
dav_error * (*output_value)(dav_db *db, const dav_prop_name *name,
dav_xmlns_info *xi,
apr_text_header *phdr, int *found);
dav_error * (*map_namespaces)(dav_db *db,
const apr_array_header_t *namespaces,
dav_namespace_map **mapping);
dav_error * (*store)(dav_db *db, const dav_prop_name *name,
const apr_xml_elem *elem,
dav_namespace_map *mapping);
dav_error * (*remove)(dav_db *db, const dav_prop_name *name);
int (*exists)(dav_db *db, const dav_prop_name *name);
dav_error * (*first_name)(dav_db *db, dav_prop_name *pname);
dav_error * (*next_name)(dav_db *db, dav_prop_name *pname);
dav_error * (*get_rollback)(dav_db *db, const dav_prop_name *name,
dav_deadprop_rollback **prollback);
dav_error * (*apply_rollback)(dav_db *db,
dav_deadprop_rollback *rollback);
void *ctx;
};
#define DAV_TIMEOUT_INFINITE 0
DAV_DECLARE(time_t) dav_get_timeout(request_rec *r);
typedef struct dav_lockdb_private dav_lockdb_private;
typedef struct dav_lock_private dav_lock_private;
typedef struct {
const dav_hooks_locks *hooks;
int ro;
dav_lockdb_private *info;
} dav_lockdb;
typedef enum {
DAV_LOCKSCOPE_UNKNOWN,
DAV_LOCKSCOPE_EXCLUSIVE,
DAV_LOCKSCOPE_SHARED
} dav_lock_scope;
typedef enum {
DAV_LOCKTYPE_UNKNOWN,
DAV_LOCKTYPE_WRITE
} dav_lock_type;
typedef enum {
DAV_LOCKREC_DIRECT,
DAV_LOCKREC_INDIRECT,
DAV_LOCKREC_INDIRECT_PARTIAL
} dav_lock_rectype;
typedef struct dav_lock {
dav_lock_rectype rectype;
int is_locknull;
dav_lock_scope scope;
dav_lock_type type;
int depth;
time_t timeout;
const dav_locktoken *locktoken;
const char *owner;
const char *auth_user;
dav_lock_private *info;
struct dav_lock *next;
} dav_lock;
DAV_DECLARE(const char *)dav_lock_get_activelock(request_rec *r,
dav_lock *locks,
dav_buffer *pbuf);
DAV_DECLARE(dav_error *) dav_lock_parse_lockinfo(request_rec *r,
const dav_resource *resrouce,
dav_lockdb *lockdb,
const apr_xml_doc *doc,
dav_lock **lock_request);
DAV_DECLARE(int) dav_unlock(request_rec *r,
const dav_resource *resource,
const dav_locktoken *locktoken);
DAV_DECLARE(dav_error *) dav_add_lock(request_rec *r,
const dav_resource *resource,
dav_lockdb *lockdb, dav_lock *request,
dav_response **response);
DAV_DECLARE(dav_error *) dav_notify_created(request_rec *r,
dav_lockdb *lockdb,
const dav_resource *resource,
int resource_state,
int depth);
DAV_DECLARE(dav_error*) dav_lock_query(dav_lockdb *lockdb,
const dav_resource *resource,
dav_lock **locks);
DAV_DECLARE(dav_error *) dav_validate_request(request_rec *r,
dav_resource *resource,
int depth,
dav_locktoken *locktoken,
dav_response **response,
int flags,
dav_lockdb *lockdb);
#define DAV_VALIDATE_RESOURCE 0x0010
#define DAV_VALIDATE_PARENT 0x0020
#define DAV_VALIDATE_ADD_LD 0x0040
#define DAV_VALIDATE_USE_424 0x0080
#define DAV_VALIDATE_IS_PARENT 0x0100
#define DAV_VALIDATE_NO_MODIFY 0x0200
DAV_DECLARE(int) dav_get_resource_state(request_rec *r,
const dav_resource *resource);
struct dav_hooks_locks {
const char * (*get_supportedlock)(
const dav_resource *resource
);
dav_error * (*parse_locktoken)(
apr_pool_t *p,
const char *char_token,
dav_locktoken **locktoken_p
);
const char * (*format_locktoken)(
apr_pool_t *p,
const dav_locktoken *locktoken
);
int (*compare_locktoken)(
const dav_locktoken *lt1,
const dav_locktoken *lt2
);
dav_error * (*open_lockdb)(
request_rec *r,
int ro,
int force,
dav_lockdb **lockdb
);
void (*close_lockdb)(
dav_lockdb *lockdb
);
dav_error * (*remove_locknull_state)(
dav_lockdb *lockdb,
const dav_resource *resource
);
dav_error * (*create_lock)(dav_lockdb *lockdb,
const dav_resource *resource,
dav_lock **lock);
dav_error * (*get_locks)(dav_lockdb *lockdb,
const dav_resource *resource,
int calltype,
dav_lock **locks);
#define DAV_GETLOCKS_RESOLVED 0
#define DAV_GETLOCKS_PARTIAL 1
#define DAV_GETLOCKS_COMPLETE 2
dav_error * (*find_lock)(dav_lockdb *lockdb,
const dav_resource *resource,
const dav_locktoken *locktoken,
int partial_ok,
dav_lock **lock);
dav_error * (*has_locks)(dav_lockdb *lockdb,
const dav_resource *resource,
int *locks_present);
dav_error * (*append_locks)(dav_lockdb *lockdb,
const dav_resource *resource,
int make_indirect,
const dav_lock *lock);
dav_error * (*remove_lock)(dav_lockdb *lockdb,
const dav_resource *resource,
const dav_locktoken *locktoken);
dav_error * (*refresh_locks)(dav_lockdb *lockdb,
const dav_resource *resource,
const dav_locktoken_list *ltl,
time_t new_time,
dav_lock **locks);
dav_error * (*lookup_resource)(dav_lockdb *lockdb,
const dav_locktoken *locktoken,
const dav_resource *start_resource,
const dav_resource **resource);
void *ctx;
};
#define DAV_RESOURCE_LOCK_NULL 10
#define DAV_RESOURCE_NULL 11
#define DAV_RESOURCE_EXISTS 12
#define DAV_RESOURCE_ERROR 13
typedef struct dav_propdb dav_propdb;
DAV_DECLARE(dav_error *) dav_open_propdb(
request_rec *r,
dav_lockdb *lockdb,
const dav_resource *resource,
int ro,
apr_array_header_t *ns_xlate,
dav_propdb **propdb);
DAV_DECLARE(void) dav_close_propdb(dav_propdb *db);
DAV_DECLARE(dav_get_props_result) dav_get_props(
dav_propdb *db,
apr_xml_doc *doc);
DAV_DECLARE(dav_get_props_result) dav_get_allprops(
dav_propdb *db,
dav_prop_insert what);
DAV_DECLARE(void) dav_get_liveprop_supported(
dav_propdb *propdb,
const char *ns_uri,
const char *propname,
apr_text_header *body);
typedef struct dav_prop_ctx {
dav_propdb *propdb;
apr_xml_elem *prop;
int operation;
#define DAV_PROP_OP_SET 1
#define DAV_PROP_OP_DELETE 2
int is_liveprop;
void *liveprop_ctx;
struct dav_rollback_item *rollback;
dav_error *err;
request_rec *r;
} dav_prop_ctx;
DAV_DECLARE_NONSTD(void) dav_prop_validate(dav_prop_ctx *ctx);
DAV_DECLARE_NONSTD(void) dav_prop_exec(dav_prop_ctx *ctx);
DAV_DECLARE_NONSTD(void) dav_prop_commit(dav_prop_ctx *ctx);
DAV_DECLARE_NONSTD(void) dav_prop_rollback(dav_prop_ctx *ctx);
#define DAV_PROP_CTX_HAS_ERR(dpc) ((dpc).err && (dpc).err->status >= 300)
enum {
DAV_CALLTYPE_MEMBER = 1,
DAV_CALLTYPE_COLLECTION,
DAV_CALLTYPE_LOCKNULL
};
typedef struct {
void *walk_ctx;
apr_pool_t *pool;
const dav_resource *resource;
dav_response *response;
} dav_walk_resource;
typedef struct {
int walk_type;
#define DAV_WALKTYPE_AUTH 0x0001
#define DAV_WALKTYPE_NORMAL 0x0002
#define DAV_WALKTYPE_LOCKNULL 0x0004
dav_error * (*func)(dav_walk_resource *wres, int calltype);
void *walk_ctx;
apr_pool_t *pool;
const dav_resource *root;
dav_lockdb *lockdb;
} dav_walk_params;
typedef struct dav_walker_ctx {
dav_walk_params w;
apr_bucket_brigade *bb;
apr_pool_t *scratchpool;
request_rec *r;
apr_xml_doc *doc;
int propfind_type;
#define DAV_PROPFIND_IS_ALLPROP 1
#define DAV_PROPFIND_IS_PROPNAME 2
#define DAV_PROPFIND_IS_PROP 3
apr_text *propstat_404;
const dav_if_header *if_header;
const dav_locktoken *locktoken;
const dav_lock *lock;
int skip_root;
int flags;
dav_buffer work_buf;
} dav_walker_ctx;
DAV_DECLARE(void) dav_add_response(dav_walk_resource *wres,
int status,
dav_get_props_result *propstats);
typedef struct dav_stream dav_stream;
typedef enum {
DAV_MODE_WRITE_TRUNC,
DAV_MODE_WRITE_SEEKABLE
} dav_stream_mode;
struct dav_hooks_repository {
int handle_get;
dav_error * (*get_resource)(
request_rec *r,
const char *root_dir,
const char *label,
int use_checked_in,
dav_resource **resource
);
dav_error * (*get_parent_resource)(
const dav_resource *resource,
dav_resource **parent_resource
);
int (*is_same_resource)(
const dav_resource *res1,
const dav_resource *res2
);
int (*is_parent_resource)(
const dav_resource *res1,
const dav_resource *res2
);
dav_error * (*open_stream)(const dav_resource *resource,
dav_stream_mode mode,
dav_stream **stream);
dav_error * (*close_stream)(dav_stream *stream, int commit);
dav_error * (*write_stream)(dav_stream *stream,
const void *buf, apr_size_t bufsize);
dav_error * (*seek_stream)(dav_stream *stream, apr_off_t abs_position);
dav_error * (*set_headers)(request_rec *r,
const dav_resource *resource);
dav_error * (*deliver)(const dav_resource *resource,
ap_filter_t *output);
dav_error * (*create_collection)(
dav_resource *resource
);
dav_error * (*copy_resource)(
const dav_resource *src,
dav_resource *dst,
int depth,
dav_response **response
);
dav_error * (*move_resource)(
dav_resource *src,
dav_resource *dst,
dav_response **response
);
dav_error * (*remove_resource)(
dav_resource *resource,
dav_response **response
);
dav_error * (*walk)(const dav_walk_params *params, int depth,
dav_response **response);
const char * (*getetag)(const dav_resource *resource);
void *ctx;
request_rec * (*get_request_rec)(const dav_resource *resource);
const char * (*get_pathname)(const dav_resource *resource);
};
DAV_DECLARE(void) dav_add_vary_header(request_rec *in_req,
request_rec *out_req,
const dav_resource *resource);
typedef enum {
DAV_AUTO_VERSION_NEVER,
DAV_AUTO_VERSION_ALWAYS,
DAV_AUTO_VERSION_LOCKED
} dav_auto_version;
typedef struct {
int resource_versioned;
int resource_checkedout;
int parent_checkedout;
dav_resource *parent_resource;
} dav_auto_version_info;
DAV_DECLARE(dav_error *) dav_auto_checkout(
request_rec *r,
dav_resource *resource,
int parent_only,
dav_auto_version_info *av_info);
DAV_DECLARE(dav_error *) dav_auto_checkin(
request_rec *r,
dav_resource *resource,
int undo,
int unlock,
dav_auto_version_info *av_info);
typedef struct {
const char *nmspace;
const char *name;
} dav_report_elem;
struct dav_hooks_vsn {
void (*get_vsn_options)(apr_pool_t *p, apr_text_header *phdr);
dav_error * (*get_option)(const dav_resource *resource,
const apr_xml_elem *elem,
apr_text_header *option);
int (*versionable)(const dav_resource *resource);
dav_auto_version (*auto_versionable)(const dav_resource *resource);
dav_error * (*vsn_control)(dav_resource *resource,
const char *target);
dav_error * (*checkout)(dav_resource *resource,
int auto_checkout,
int is_unreserved, int is_fork_ok,
int create_activity,
apr_array_header_t *activities,
dav_resource **working_resource);
dav_error * (*uncheckout)(dav_resource *resource);
dav_error * (*checkin)(dav_resource *resource,
int keep_checked_out,
dav_resource **version_resource);
dav_error * (*avail_reports)(const dav_resource *resource,
const dav_report_elem **reports);
int (*report_label_header_allowed)(const apr_xml_doc *doc);
dav_error * (*deliver_report)(request_rec *r,
const dav_resource *resource,
const apr_xml_doc *doc,
ap_filter_t *output);
dav_error * (*update)(const dav_resource *resource,
const dav_resource *version,
const char *label,
int depth,
dav_response **response);
dav_error * (*add_label)(const dav_resource *resource,
const char *label,
int replace);
dav_error * (*remove_label)(const dav_resource *resource,
const char *label);
int (*can_be_workspace)(const dav_resource *resource);
dav_error * (*make_workspace)(dav_resource *resource,
apr_xml_doc *doc);
int (*can_be_activity)(const dav_resource *resource);
dav_error * (*make_activity)(dav_resource *resource);
dav_error * (*merge)(dav_resource *target, dav_resource *source,
int no_auto_merge, int no_checkout,
apr_xml_elem *prop_elem,
ap_filter_t *output);
void *ctx;
};
struct dav_hooks_binding {
int (*is_bindable)(const dav_resource *resource);
dav_error * (*bind_resource)(const dav_resource *resource,
dav_resource *binding);
void *ctx;
};
struct dav_hooks_search {
dav_error * (*set_option_head)(request_rec *r);
dav_error * (*search_resource)(request_rec *r,
dav_response **response);
void *ctx;
};
typedef struct {
int propid;
const dav_hooks_liveprop *provider;
} dav_elem_private;
#define DAV_OPTIONS_EXTENSION_GROUP "dav_options"
typedef struct dav_options_provider {
dav_error* (*dav_header)(request_rec *r,
const dav_resource *resource,
apr_text_header *phdr);
dav_error* (*dav_method)(request_rec *r,
const dav_resource *resource,
apr_text_header *phdr);
void *ctx;
} dav_options_provider;
extern DAV_DECLARE(const dav_options_provider *) dav_get_options_providers(const char *name);
extern DAV_DECLARE(void) dav_options_provider_register(apr_pool_t *p,
const char *name,
const dav_options_provider *provider);
typedef struct dav_resource_type_provider {
int (*get_resource_type)(const dav_resource *resource,
const char **name,
const char **uri);
} dav_resource_type_provider;
#define DAV_RESOURCE_TYPE_GROUP "dav_resource_type"
DAV_DECLARE(void) dav_resource_type_provider_register(apr_pool_t *p,
const char *name,
const dav_resource_type_provider *provider);
DAV_DECLARE(const dav_resource_type_provider *) dav_get_resource_type_providers(const char *name);
#if defined(__cplusplus)
}
#endif
#endif
