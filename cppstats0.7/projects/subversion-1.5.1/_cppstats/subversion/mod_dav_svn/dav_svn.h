#if !defined(DAV_SVN_H)
#define DAV_SVN_H
#include <apr_tables.h>
#include <apr_xml.h>
#include <httpd.h>
#include <http_log.h>
#include <mod_dav.h>
#include "svn_error.h"
#include "svn_fs.h"
#include "svn_repos.h"
#include "svn_path.h"
#include "svn_xml.h"
#include "private/svn_dav_protocol.h"
#include "mod_authz_svn.h"
#if defined(__cplusplus)
extern "C" {
#endif
#define DAV_SVN__DEFAULT_VCC_NAME "default"
#define DAV_SVN__AUTOVERSIONING_ACTIVITY "svn-autoversioning-activity"
typedef struct {
apr_pool_t *pool;
const char *root_path;
const char *base_url;
const char *special_uri;
const char *fs_path;
const char *repo_name;
const char *repo_basename;
const char *xslt_uri;
svn_boolean_t autoversioning;
svn_boolean_t bulk_updates;
svn_repos_t *repos;
svn_fs_t *fs;
const char *username;
svn_boolean_t is_svn_client;
apr_hash_t *capabilities;
const char *activities_db;
} dav_svn_repos;
enum dav_svn_private_restype {
DAV_SVN_RESTYPE_UNSET,
DAV_SVN_RESTYPE_ROOT_COLLECTION,
DAV_SVN_RESTYPE_VER_COLLECTION,
DAV_SVN_RESTYPE_HIS_COLLECTION,
DAV_SVN_RESTYPE_WRK_COLLECTION,
DAV_SVN_RESTYPE_ACT_COLLECTION,
DAV_SVN_RESTYPE_VCC_COLLECTION,
DAV_SVN_RESTYPE_BC_COLLECTION,
DAV_SVN_RESTYPE_BLN_COLLECTION,
DAV_SVN_RESTYPE_WBL_COLLECTION,
DAV_SVN_RESTYPE_VCC,
DAV_SVN_RESTYPE_PARENTPATH_COLLECTION
};
typedef struct {
svn_fs_root_t *root;
svn_revnum_t rev;
const char *activity_id;
const char *txn_name;
svn_fs_txn_t *txn;
} dav_svn_root;
struct dav_resource_private {
svn_stringbuf_t *uri_path;
const char *repos_path;
dav_svn_repos *repos;
dav_svn_root root;
enum dav_svn_private_restype restype;
request_rec *r;
int is_svndiff;
const char *delta_base;
int svndiff_version;
const char *svn_client_options;
svn_revnum_t version_name;
const char *base_checksum;
const char *result_checksum;
svn_boolean_t auto_checked_out;
apr_pool_t *pool;
};
struct dav_locktoken {
const char *uuid_str;
};
const char *dav_svn__get_fs_path(request_rec *r);
const char *dav_svn__get_fs_parent_path(request_rec *r);
svn_boolean_t dav_svn__get_autoversioning_flag(request_rec *r);
svn_boolean_t dav_svn__get_bulk_updates_flag(request_rec *r);
svn_boolean_t dav_svn__get_pathauthz_flag(request_rec *r);
authz_svn__subreq_bypass_func_t dav_svn__get_pathauthz_bypass(request_rec *r);
svn_boolean_t dav_svn__get_list_parentpath_flag(request_rec *r);
const char *dav_svn__get_special_uri(request_rec *r);
const char *dav_svn__get_repo_name(request_rec *r);
const char *dav_svn__get_xslt_uri(request_rec *r);
const char * dav_svn__get_master_uri(request_rec *r);
const char * dav_svn__get_activities_db(request_rec *r);
const char * dav_svn__get_root_dir(request_rec *r);
const char *
dav_svn__get_txn(const dav_svn_repos *repos, const char *activity_id);
dav_error *
dav_svn__delete_activity(const dav_svn_repos *repos, const char *activity_id);
dav_error *
dav_svn__store_activity(const dav_svn_repos *repos,
const char *activity_id,
const char *txn_name);
dav_error *
dav_svn__create_activity(const dav_svn_repos *repos,
const char **ptxn_name,
apr_pool_t *pool);
const char *
dav_svn__getetag(const dav_resource *resource, apr_pool_t *pool);
dav_resource *
dav_svn__create_working_resource(dav_resource *base,
const char *activity_id,
const char *txn_name,
int tweak_in_place);
dav_error *
dav_svn__working_to_regular_resource(dav_resource *resource);
dav_error *
dav_svn__create_version_resource(dav_resource **version_res,
const char *uri,
apr_pool_t *pool);
extern const dav_hooks_repository dav_svn__hooks_repository;
extern const dav_hooks_propdb dav_svn__hooks_propdb;
extern const dav_hooks_locks dav_svn__hooks_locks;
svn_error_t *
dav_svn__attach_auto_revprops(svn_fs_txn_t *txn,
const char *fs_path,
apr_pool_t *pool);
dav_error *
dav_svn__checkout(dav_resource *resource,
int auto_checkout,
int is_unreserved,
int is_fork_ok,
int create_activity,
apr_array_header_t *activities,
dav_resource **working_resource);
dav_error *
dav_svn__checkin(dav_resource *resource,
int keep_checked_out,
dav_resource **version_resource);
dav_error *
dav_svn__build_lock_hash(apr_hash_t **locks,
request_rec *r,
const char *path_prefix,
apr_pool_t *pool);
dav_error *
dav_svn__push_locks(dav_resource *resource,
apr_hash_t *locks,
apr_pool_t *pool);
extern const dav_hooks_vsn dav_svn__hooks_vsn;
extern const dav_liveprop_group dav_svn__liveprop_group;
void dav_svn__gather_propsets(apr_array_header_t *uris);
int
dav_svn__find_liveprop(const dav_resource *resource,
const char *ns_uri,
const char *name,
const dav_hooks_liveprop **hooks);
void
dav_svn__insert_all_liveprops(request_rec *r,
const dav_resource *resource,
dav_prop_insert what,
apr_text_header *phdr);
dav_error *
dav_svn__merge_response(ap_filter_t *output,
const dav_svn_repos *repos,
svn_revnum_t new_rev,
char *post_commit_err,
apr_xml_elem *prop_elem,
svn_boolean_t disable_merge_response,
apr_pool_t *pool);
static const dav_report_elem dav_svn__reports_list[] = {
{ SVN_XML_NAMESPACE, "update-report" },
{ SVN_XML_NAMESPACE, "log-report" },
{ SVN_XML_NAMESPACE, "dated-rev-report" },
{ SVN_XML_NAMESPACE, "get-locations" },
{ SVN_XML_NAMESPACE, "get-location-segments" },
{ SVN_XML_NAMESPACE, "file-revs-report" },
{ SVN_XML_NAMESPACE, "get-locks-report" },
{ SVN_XML_NAMESPACE, "replay-report" },
{ SVN_XML_NAMESPACE, SVN_DAV__MERGEINFO_REPORT },
{ NULL, NULL },
};
dav_error *
dav_svn__update_report(const dav_resource *resource,
const apr_xml_doc *doc,
ap_filter_t *output);
dav_error *
dav_svn__log_report(const dav_resource *resource,
const apr_xml_doc *doc,
ap_filter_t *output);
dav_error *
dav_svn__dated_rev_report(const dav_resource *resource,
const apr_xml_doc *doc,
ap_filter_t *output);
dav_error *
dav_svn__get_locations_report(const dav_resource *resource,
const apr_xml_doc *doc,
ap_filter_t *output);
dav_error *
dav_svn__get_location_segments_report(const dav_resource *resource,
const apr_xml_doc *doc,
ap_filter_t *output);
dav_error *
dav_svn__file_revs_report(const dav_resource *resource,
const apr_xml_doc *doc,
ap_filter_t *output);
dav_error *
dav_svn__replay_report(const dav_resource *resource,
const apr_xml_doc *doc,
ap_filter_t *output);
dav_error *
dav_svn__get_mergeinfo_report(const dav_resource *resource,
const apr_xml_doc *doc,
ap_filter_t *output);
dav_error *
dav_svn__get_locks_report(const dav_resource *resource,
const apr_xml_doc *doc,
ap_filter_t *output);
typedef struct {
request_rec *r;
const dav_svn_repos *repos;
} dav_svn__authz_read_baton;
svn_boolean_t
dav_svn__allow_read(const dav_resource *resource,
svn_revnum_t rev,
apr_pool_t *pool);
svn_repos_authz_func_t
dav_svn__authz_read_func(dav_svn__authz_read_baton *baton);
dav_error *
dav_svn__new_error_tag(apr_pool_t *pool,
int status,
int errno_id,
const char *desc,
const char *namespace,
const char *tagname);
dav_error *
dav_svn__convert_err(svn_error_t *serr,
int status,
const char *message,
apr_pool_t *pool);
svn_revnum_t
dav_svn__get_safe_cr(svn_fs_root_t *root, const char *path, apr_pool_t *pool);
enum dav_svn__build_what {
DAV_SVN__BUILD_URI_ACT_COLLECTION,
DAV_SVN__BUILD_URI_BASELINE,
DAV_SVN__BUILD_URI_BC,
DAV_SVN__BUILD_URI_PUBLIC,
DAV_SVN__BUILD_URI_VERSION,
DAV_SVN__BUILD_URI_VCC
};
const char *
dav_svn__build_uri(const dav_svn_repos *repos,
enum dav_svn__build_what what,
svn_revnum_t revision,
const char *path,
int add_href,
apr_pool_t *pool);
typedef struct {
svn_revnum_t rev;
const char *repos_path;
const char *activity_id;
} dav_svn__uri_info;
svn_error_t *
dav_svn__simple_parse_uri(dav_svn__uri_info *info,
const dav_resource *relative,
const char *uri,
apr_pool_t *pool);
int dav_svn__find_ns(apr_array_header_t *namespaces, const char *uri);
svn_error_t *
dav_svn__send_xml(apr_bucket_brigade *bb,
ap_filter_t *output,
const char *fmt,
...)
__attribute__((format(printf, 3, 4)));
dav_error *dav_svn__test_canonical(const char *path, apr_pool_t *pool);
dav_error *
dav_svn__sanitize_error(svn_error_t *serr,
const char *new_msg,
int http_status,
request_rec *r);
svn_stream_t *
dav_svn__make_base64_output_stream(apr_bucket_brigade *bb,
ap_filter_t *output,
apr_pool_t *pool);
void
dav_svn__operational_log(struct dav_resource_private *info, const char *line);
int dav_svn__proxy_merge_fixup(request_rec *r);
apr_status_t dav_svn__location_in_filter(ap_filter_t *f,
apr_bucket_brigade *bb,
ap_input_mode_t mode,
apr_read_type_e block,
apr_off_t readbytes);
apr_status_t dav_svn__location_header_filter(ap_filter_t *f,
apr_bucket_brigade *bb);
apr_status_t dav_svn__location_body_filter(ap_filter_t *f,
apr_bucket_brigade *bb);
#if defined(__cplusplus)
}
#endif
#endif
