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
#include "dav_svn.h"
svn_error_t *
dav_svn__attach_auto_revprops(svn_fs_txn_t *txn,
const char *fs_path,
apr_pool_t *pool) {
const char *logmsg;
svn_string_t *logval;
svn_error_t *serr;
logmsg = apr_psprintf(pool,
"Autoversioning commit: a non-deltaV client made "
"a change to\n%s", fs_path);
logval = svn_string_create(logmsg, pool);
if ((serr = svn_repos_fs_change_txn_prop(txn, SVN_PROP_REVISION_LOG, logval,
pool)))
return serr;
if ((serr = svn_repos_fs_change_txn_prop(txn,
SVN_PROP_REVISION_AUTOVERSIONED,
svn_string_create("*", pool),
pool)))
return serr;
return SVN_NO_ERROR;
}
static dav_error *
set_auto_revprops(dav_resource *resource) {
svn_error_t *serr;
if (! (resource->type == DAV_RESOURCE_TYPE_WORKING
&& resource->info->auto_checked_out))
return dav_new_error(resource->pool, HTTP_INTERNAL_SERVER_ERROR, 0,
"Set_auto_revprops called on invalid resource.");
if ((serr = dav_svn__attach_auto_revprops(resource->info->root.txn,
resource->info->repos_path,
resource->pool)))
return dav_svn__convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
"Error setting a revision property "
" on auto-checked-out resource's txn. ",
resource->pool);
return NULL;
}
static dav_error *
open_txn(svn_fs_txn_t **ptxn,
svn_fs_t *fs,
const char *txn_name,
apr_pool_t *pool) {
svn_error_t *serr;
serr = svn_fs_open_txn(ptxn, fs, txn_name, pool);
if (serr != NULL) {
if (serr->apr_err == SVN_ERR_FS_NO_SUCH_TRANSACTION) {
return dav_svn__convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
"The transaction specified by the "
"activity does not exist",
pool);
}
return dav_svn__convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
"There was a problem opening the "
"transaction specified by this "
"activity.",
pool);
}
return NULL;
}
static void
get_vsn_options(apr_pool_t *p, apr_text_header *phdr) {
apr_text_append(p, phdr,
"version-control,checkout,working-resource");
apr_text_append(p, phdr,
"merge,baseline,activity,version-controlled-collection");
apr_text_append(p, phdr, SVN_DAV_NS_DAV_SVN_MERGEINFO);
apr_text_append(p, phdr, SVN_DAV_NS_DAV_SVN_DEPTH);
apr_text_append(p, phdr, SVN_DAV_NS_DAV_SVN_LOG_REVPROPS);
apr_text_append(p, phdr, SVN_DAV_NS_DAV_SVN_PARTIAL_REPLAY);
}
static dav_error *
get_option(const dav_resource *resource,
const apr_xml_elem *elem,
apr_text_header *option) {
if (elem->ns == APR_XML_NS_DAV_ID) {
if (strcmp(elem->name, "activity-collection-set") == 0) {
apr_text_append(resource->pool, option,
"<D:activity-collection-set>");
apr_text_append(resource->pool, option,
dav_svn__build_uri(resource->info->repos,
DAV_SVN__BUILD_URI_ACT_COLLECTION,
SVN_INVALID_REVNUM, NULL,
1 ,
resource->pool));
apr_text_append(resource->pool, option,
"</D:activity-collection-set>");
}
}
return NULL;
}
static int
versionable(const dav_resource *resource) {
return 0;
}
static dav_auto_version
auto_versionable(const dav_resource *resource) {
if (resource->type == DAV_RESOURCE_TYPE_VERSION
&& resource->baselined)
return DAV_AUTO_VERSION_ALWAYS;
if (resource->info->repos->autoversioning) {
if (resource->type == DAV_RESOURCE_TYPE_REGULAR)
return DAV_AUTO_VERSION_ALWAYS;
if (resource->type == DAV_RESOURCE_TYPE_WORKING
&& resource->info->auto_checked_out)
return DAV_AUTO_VERSION_ALWAYS;
}
return DAV_AUTO_VERSION_NEVER;
}
static dav_error *
vsn_control(dav_resource *resource, const char *target) {
if (resource->exists)
return dav_new_error(resource->pool, HTTP_BAD_REQUEST, 0,
"vsn_control called on already-versioned resource.");
if (target != NULL)
return dav_svn__new_error_tag(resource->pool, HTTP_NOT_IMPLEMENTED,
SVN_ERR_UNSUPPORTED_FEATURE,
"vsn_control called with non-null target.",
SVN_DAV_ERROR_NAMESPACE,
SVN_DAV_ERROR_TAG);
return NULL;
}
dav_error *
dav_svn__checkout(dav_resource *resource,
int auto_checkout,
int is_unreserved,
int is_fork_ok,
int create_activity,
apr_array_header_t *activities,
dav_resource **working_resource) {
const char *txn_name;
svn_error_t *serr;
apr_status_t apr_err;
dav_error *derr;
dav_svn__uri_info parse;
if (auto_checkout) {
dav_resource *res;
const char *uuid_buf;
void *data;
const char *shared_activity, *shared_txn_name = NULL;
if ((resource->type == DAV_RESOURCE_TYPE_VERSION)
&& resource->baselined)
return NULL;
if (resource->type != DAV_RESOURCE_TYPE_REGULAR)
return dav_svn__new_error_tag(resource->pool, HTTP_METHOD_NOT_ALLOWED,
SVN_ERR_UNSUPPORTED_FEATURE,
"auto-checkout attempted on non-regular "
"version-controlled resource.",
SVN_DAV_ERROR_NAMESPACE,
SVN_DAV_ERROR_TAG);
if (resource->baselined)
return dav_svn__new_error_tag(resource->pool, HTTP_METHOD_NOT_ALLOWED,
SVN_ERR_UNSUPPORTED_FEATURE,
"auto-checkout attempted on baseline "
"collection, which is not supported.",
SVN_DAV_ERROR_NAMESPACE,
SVN_DAV_ERROR_TAG);
apr_err = apr_pool_userdata_get(&data,
DAV_SVN__AUTOVERSIONING_ACTIVITY,
resource->info->r->pool);
if (apr_err)
return dav_svn__convert_err(svn_error_create(apr_err, 0, NULL),
HTTP_INTERNAL_SERVER_ERROR,
"Error fetching pool userdata.",
resource->pool);
shared_activity = data;
if (! shared_activity) {
uuid_buf = svn_uuid_generate(resource->info->r->pool);
shared_activity = apr_pstrdup(resource->info->r->pool, uuid_buf);
derr = dav_svn__create_activity(resource->info->repos,
&shared_txn_name,
resource->info->r->pool);
if (derr) return derr;
derr = dav_svn__store_activity(resource->info->repos,
shared_activity, shared_txn_name);
if (derr) return derr;
apr_err = apr_pool_userdata_set(shared_activity,
DAV_SVN__AUTOVERSIONING_ACTIVITY,
NULL, resource->info->r->pool);
if (apr_err)
return dav_svn__convert_err(svn_error_create(apr_err, 0, NULL),
HTTP_INTERNAL_SERVER_ERROR,
"Error setting pool userdata.",
resource->pool);
}
if (! shared_txn_name) {
shared_txn_name = dav_svn__get_txn(resource->info->repos,
shared_activity);
if (! shared_txn_name)
return dav_new_error(resource->pool, HTTP_INTERNAL_SERVER_ERROR, 0,
"Cannot look up a txn_name by activity");
}
res = dav_svn__create_working_resource(resource,
shared_activity, shared_txn_name,
TRUE );
resource->info->auto_checked_out = TRUE;
derr = open_txn(&resource->info->root.txn, resource->info->repos->fs,
resource->info->root.txn_name, resource->pool);
if (derr) return derr;
serr = svn_fs_txn_root(&resource->info->root.root,
resource->info->root.txn, resource->pool);
if (serr != NULL)
return dav_svn__convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
"Could not open a (transaction) root "
"in the repository",
resource->pool);
return NULL;
}
if (resource->type != DAV_RESOURCE_TYPE_VERSION) {
return dav_svn__new_error_tag(resource->pool, HTTP_METHOD_NOT_ALLOWED,
SVN_ERR_UNSUPPORTED_FEATURE,
"CHECKOUT can only be performed on a "
"version resource [at this time].",
SVN_DAV_ERROR_NAMESPACE,
SVN_DAV_ERROR_TAG);
}
if (create_activity) {
return dav_svn__new_error_tag(resource->pool, HTTP_NOT_IMPLEMENTED,
SVN_ERR_UNSUPPORTED_FEATURE,
"CHECKOUT can not create an activity at "
"this time. Use MKACTIVITY first.",
SVN_DAV_ERROR_NAMESPACE,
SVN_DAV_ERROR_TAG);
}
if (is_unreserved) {
return dav_svn__new_error_tag(resource->pool, HTTP_NOT_IMPLEMENTED,
SVN_ERR_UNSUPPORTED_FEATURE,
"Unreserved checkouts are not yet "
"available. A version history may not be "
"checked out more than once, into a "
"specific activity.",
SVN_DAV_ERROR_NAMESPACE,
SVN_DAV_ERROR_TAG);
}
if (activities == NULL) {
return dav_svn__new_error_tag(resource->pool, HTTP_CONFLICT,
SVN_ERR_INCOMPLETE_DATA,
"An activity must be provided for "
"checkout.",
SVN_DAV_ERROR_NAMESPACE,
SVN_DAV_ERROR_TAG);
}
if (activities->nelts != 1) {
return dav_svn__new_error_tag(resource->pool, HTTP_CONFLICT,
SVN_ERR_INCORRECT_PARAMS,
"Only one activity may be specified within "
"the CHECKOUT.",
SVN_DAV_ERROR_NAMESPACE,
SVN_DAV_ERROR_TAG);
}
serr = dav_svn__simple_parse_uri(&parse, resource,
APR_ARRAY_IDX(activities, 0, const char *),
resource->pool);
if (serr != NULL) {
return dav_svn__convert_err(serr, HTTP_CONFLICT,
"The activity href could not be parsed "
"properly.",
resource->pool);
}
if (parse.activity_id == NULL) {
return dav_svn__new_error_tag(resource->pool, HTTP_CONFLICT,
SVN_ERR_INCORRECT_PARAMS,
"The provided href is not an activity URI.",
SVN_DAV_ERROR_NAMESPACE,
SVN_DAV_ERROR_TAG);
}
if ((txn_name = dav_svn__get_txn(resource->info->repos,
parse.activity_id)) == NULL) {
return dav_svn__new_error_tag(resource->pool, HTTP_CONFLICT,
SVN_ERR_APMOD_ACTIVITY_NOT_FOUND,
"The specified activity does not exist.",
SVN_DAV_ERROR_NAMESPACE,
SVN_DAV_ERROR_TAG);
}
if (resource->baselined || resource->info->root.rev == SVN_INVALID_REVNUM) {
svn_revnum_t youngest;
serr = svn_fs_youngest_rev(&youngest, resource->info->repos->fs,
resource->pool);
if (serr != NULL) {
return dav_svn__convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
"Could not determine the youngest "
"revision for verification against "
"the baseline being checked out.",
resource->pool);
}
if (resource->info->root.rev != youngest) {
return dav_svn__new_error_tag(resource->pool, HTTP_CONFLICT,
SVN_ERR_APMOD_BAD_BASELINE,
"The specified baseline is not the "
"latest baseline, so it may not be "
"checked out.",
SVN_DAV_ERROR_NAMESPACE,
SVN_DAV_ERROR_TAG);
}
} else {
svn_fs_txn_t *txn;
svn_fs_root_t *txn_root;
svn_revnum_t txn_created_rev;
dav_error *err;
if ((err = open_txn(&txn, resource->info->repos->fs, txn_name,
resource->pool)) != NULL)
return err;
serr = svn_fs_txn_root(&txn_root, txn, resource->pool);
if (serr != NULL) {
return dav_svn__convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
"Could not open the transaction tree.",
resource->pool);
}
serr = svn_fs_node_created_rev(&txn_created_rev,
txn_root, resource->info->repos_path,
resource->pool);
if (serr != NULL) {
return dav_svn__convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
"Could not get created-rev of "
"transaction node.",
resource->pool);
}
if (SVN_IS_VALID_REVNUM( txn_created_rev )) {
if (resource->info->root.rev < txn_created_rev) {
return dav_svn__new_error_tag
(resource->pool, HTTP_CONFLICT, SVN_ERR_FS_CONFLICT,
"resource out of date; try updating",
SVN_DAV_ERROR_NAMESPACE,
SVN_DAV_ERROR_TAG);
} else if (resource->info->root.rev > txn_created_rev) {
const svn_fs_id_t *url_noderev_id, *txn_noderev_id;
if ((serr = svn_fs_node_id(&txn_noderev_id, txn_root,
resource->info->repos_path,
resource->pool))) {
err = dav_svn__new_error_tag
(resource->pool, HTTP_CONFLICT, serr->apr_err,
"Unable to fetch the node revision id of the version "
"resource within the transaction.",
SVN_DAV_ERROR_NAMESPACE,
SVN_DAV_ERROR_TAG);
svn_error_clear(serr);
return err;
}
if ((serr = svn_fs_node_id(&url_noderev_id,
resource->info->root.root,
resource->info->repos_path,
resource->pool))) {
err = dav_svn__new_error_tag
(resource->pool, HTTP_CONFLICT, serr->apr_err,
"Unable to fetch the node revision id of the version "
"resource within the revision.",
SVN_DAV_ERROR_NAMESPACE,
SVN_DAV_ERROR_TAG);
svn_error_clear(serr);
return err;
}
if (svn_fs_compare_ids(url_noderev_id, txn_noderev_id) != 0) {
return dav_svn__new_error_tag
(resource->pool, HTTP_CONFLICT, SVN_ERR_FS_CONFLICT,
"version resource newer than txn (restart the commit)",
SVN_DAV_ERROR_NAMESPACE,
SVN_DAV_ERROR_TAG);
}
}
}
}
*working_resource = dav_svn__create_working_resource(resource,
parse.activity_id,
txn_name,
FALSE);
return NULL;
}
static dav_error *
uncheckout(dav_resource *resource) {
if (resource->type != DAV_RESOURCE_TYPE_WORKING)
return dav_svn__new_error_tag(resource->pool, HTTP_INTERNAL_SERVER_ERROR,
SVN_ERR_UNSUPPORTED_FEATURE,
"UNCHECKOUT called on non-working resource.",
SVN_DAV_ERROR_NAMESPACE,
SVN_DAV_ERROR_TAG);
if (resource->info->root.txn)
svn_error_clear(svn_fs_abort_txn(resource->info->root.txn,
resource->pool));
if (resource->info->root.activity_id) {
dav_svn__delete_activity(resource->info->repos,
resource->info->root.activity_id);
apr_pool_userdata_set(NULL, DAV_SVN__AUTOVERSIONING_ACTIVITY,
NULL, resource->info->r->pool);
}
resource->info->root.txn_name = NULL;
resource->info->root.txn = NULL;
resource->info->auto_checked_out = FALSE;
return dav_svn__working_to_regular_resource(resource);
}
struct cleanup_deltify_baton {
const char *repos_path;
svn_revnum_t revision;
apr_pool_t *pool;
};
static apr_status_t
cleanup_deltify(void *data) {
struct cleanup_deltify_baton *cdb = data;
svn_repos_t *repos;
svn_error_t *err;
apr_pool_t *subpool = svn_pool_create(cdb->pool);
err = svn_repos_open(&repos, cdb->repos_path, subpool);
if (err) {
ap_log_perror(APLOG_MARK, APLOG_ERR, err->apr_err, cdb->pool,
"cleanup_deltify: error opening repository '%s'",
cdb->repos_path);
svn_error_clear(err);
goto cleanup;
}
err = svn_fs_deltify_revision(svn_repos_fs(repos),
cdb->revision, subpool);
if (err) {
ap_log_perror(APLOG_MARK, APLOG_ERR, err->apr_err, cdb->pool,
"cleanup_deltify: error deltifying against revision %ld"
" in repository '%s'",
cdb->revision, cdb->repos_path);
svn_error_clear(err);
}
cleanup:
svn_pool_destroy(subpool);
return APR_SUCCESS;
}
static void
register_deltification_cleanup(svn_repos_t *repos,
svn_revnum_t revision,
apr_pool_t *pool) {
struct cleanup_deltify_baton *cdb = apr_palloc(pool, sizeof(*cdb));
cdb->repos_path = svn_repos_path(repos, pool);
cdb->revision = revision;
cdb->pool = pool;
apr_pool_cleanup_register(pool, cdb, cleanup_deltify, apr_pool_cleanup_null);
}
dav_error *
dav_svn__checkin(dav_resource *resource,
int keep_checked_out,
dav_resource **version_resource) {
svn_error_t *serr;
dav_error *err;
apr_status_t apr_err;
const char *uri;
const char *shared_activity;
void *data;
if (resource->type != DAV_RESOURCE_TYPE_WORKING)
return dav_svn__new_error_tag(resource->pool, HTTP_INTERNAL_SERVER_ERROR,
SVN_ERR_UNSUPPORTED_FEATURE,
"CHECKIN called on non-working resource.",
SVN_DAV_ERROR_NAMESPACE,
SVN_DAV_ERROR_TAG);
apr_err = apr_pool_userdata_get(&data,
DAV_SVN__AUTOVERSIONING_ACTIVITY,
resource->info->r->pool);
if (apr_err)
return dav_svn__convert_err(svn_error_create(apr_err, 0, NULL),
HTTP_INTERNAL_SERVER_ERROR,
"Error fetching pool userdata.",
resource->pool);
shared_activity = data;
if (shared_activity
&& (strcmp(shared_activity, resource->info->root.activity_id) == 0)) {
const char *shared_txn_name;
const char *conflict_msg;
svn_revnum_t new_rev;
shared_txn_name = dav_svn__get_txn(resource->info->repos,
shared_activity);
if (! shared_txn_name)
return dav_new_error(resource->pool, HTTP_INTERNAL_SERVER_ERROR, 0,
"Cannot look up a txn_name by activity");
if (resource->info->root.txn_name
&& (strcmp(shared_txn_name, resource->info->root.txn_name) != 0))
return dav_new_error(resource->pool, HTTP_INTERNAL_SERVER_ERROR, 0,
"Internal txn_name doesn't match"
" autoversioning transaction.");
if (! resource->info->root.txn)
return dav_new_error(resource->pool, HTTP_INTERNAL_SERVER_ERROR, 0,
"Autoversioning txn isn't open "
"when it should be.");
err = set_auto_revprops(resource);
if (err)
return err;
serr = svn_repos_fs_commit_txn(&conflict_msg,
resource->info->repos->repos,
&new_rev,
resource->info->root.txn,
resource->pool);
if (serr != NULL) {
const char *msg;
svn_error_clear(svn_fs_abort_txn(resource->info->root.txn,
resource->pool));
if (serr->apr_err == SVN_ERR_FS_CONFLICT) {
msg = apr_psprintf(resource->pool,
"A conflict occurred during the CHECKIN "
"processing. The problem occurred with "
"the \"%s\" resource.",
conflict_msg);
} else
msg = "An error occurred while committing the transaction.";
dav_svn__delete_activity(resource->info->repos, shared_activity);
apr_pool_userdata_set(NULL, DAV_SVN__AUTOVERSIONING_ACTIVITY,
NULL, resource->info->r->pool);
return dav_svn__convert_err(serr, HTTP_CONFLICT, msg,
resource->pool);
}
dav_svn__delete_activity(resource->info->repos, shared_activity);
apr_pool_userdata_set(NULL, DAV_SVN__AUTOVERSIONING_ACTIVITY,
NULL, resource->info->r->pool);
register_deltification_cleanup(resource->info->repos->repos,
new_rev,
resource->info->r->connection->pool);
if (version_resource) {
uri = dav_svn__build_uri(resource->info->repos,
DAV_SVN__BUILD_URI_VERSION,
new_rev, resource->info->repos_path,
0, resource->pool);
err = dav_svn__create_version_resource(version_resource, uri,
resource->pool);
if (err)
return err;
}
}
resource->info->root.txn_name = NULL;
resource->info->root.txn = NULL;
if (! keep_checked_out) {
resource->info->auto_checked_out = FALSE;
return dav_svn__working_to_regular_resource(resource);
}
return NULL;
}
static dav_error *
avail_reports(const dav_resource *resource, const dav_report_elem **reports) {
if (resource->type != DAV_RESOURCE_TYPE_REGULAR) {
*reports = NULL;
return NULL;
}
*reports = dav_svn__reports_list;
return NULL;
}
static int
report_label_header_allowed(const apr_xml_doc *doc) {
return 0;
}
static dav_error *
deliver_report(request_rec *r,
const dav_resource *resource,
const apr_xml_doc *doc,
ap_filter_t *output) {
int ns = dav_svn__find_ns(doc->namespaces, SVN_XML_NAMESPACE);
if (doc->root->ns == ns) {
if (strcmp(doc->root->name, "update-report") == 0) {
return dav_svn__update_report(resource, doc, output);
} else if (strcmp(doc->root->name, "log-report") == 0) {
return dav_svn__log_report(resource, doc, output);
} else if (strcmp(doc->root->name, "dated-rev-report") == 0) {
return dav_svn__dated_rev_report(resource, doc, output);
} else if (strcmp(doc->root->name, "get-locations") == 0) {
return dav_svn__get_locations_report(resource, doc, output);
} else if (strcmp(doc->root->name, "get-location-segments") == 0) {
return dav_svn__get_location_segments_report(resource, doc, output);
} else if (strcmp(doc->root->name, "file-revs-report") == 0) {
return dav_svn__file_revs_report(resource, doc, output);
} else if (strcmp(doc->root->name, "get-locks-report") == 0) {
return dav_svn__get_locks_report(resource, doc, output);
} else if (strcmp(doc->root->name, "replay-report") == 0) {
return dav_svn__replay_report(resource, doc, output);
} else if (strcmp(doc->root->name, SVN_DAV__MERGEINFO_REPORT) == 0) {
return dav_svn__get_mergeinfo_report(resource, doc, output);
}
}
return dav_svn__new_error_tag(resource->pool, HTTP_NOT_IMPLEMENTED,
SVN_ERR_UNSUPPORTED_FEATURE,
"The requested report is unknown.",
SVN_DAV_ERROR_NAMESPACE,
SVN_DAV_ERROR_TAG);
}
static int
can_be_activity(const dav_resource *resource) {
return (resource->info->auto_checked_out == TRUE ||
(resource->type == DAV_RESOURCE_TYPE_ACTIVITY &&
!resource->exists));
}
static dav_error *
make_activity(dav_resource *resource) {
const char *activity_id = resource->info->root.activity_id;
const char *txn_name;
dav_error *err;
if (! can_be_activity(resource))
return dav_svn__new_error_tag(resource->pool, HTTP_FORBIDDEN,
SVN_ERR_APMOD_MALFORMED_URI,
"Activities cannot be created at that "
"location; query the "
"DAV:activity-collection-set property.",
SVN_DAV_ERROR_NAMESPACE,
SVN_DAV_ERROR_TAG);
err = dav_svn__create_activity(resource->info->repos, &txn_name,
resource->pool);
if (err != NULL)
return err;
err = dav_svn__store_activity(resource->info->repos, activity_id, txn_name);
if (err != NULL)
return err;
resource->info->root.txn_name = txn_name;
resource->exists = 1;
return NULL;
}
dav_error *
dav_svn__build_lock_hash(apr_hash_t **locks,
request_rec *r,
const char *path_prefix,
apr_pool_t *pool) {
apr_status_t apr_err;
dav_error *derr;
void *data = NULL;
apr_xml_doc *doc = NULL;
apr_xml_elem *child, *lockchild;
int ns;
apr_hash_t *hash = apr_hash_make(pool);
apr_err = apr_pool_userdata_get(&data, "svn-request-body", r->pool);
if (apr_err)
return dav_svn__convert_err(svn_error_create(apr_err, 0, NULL),
HTTP_INTERNAL_SERVER_ERROR,
"Error fetching pool userdata.",
pool);
doc = data;
if (! doc) {
*locks = hash;
return SVN_NO_ERROR;
}
ns = dav_svn__find_ns(doc->namespaces, SVN_XML_NAMESPACE);
if (ns == -1) {
*locks = hash;
return SVN_NO_ERROR;
}
if ((doc->root->ns == ns)
&& (strcmp(doc->root->name, "lock-token-list") == 0)) {
child = doc->root;
} else {
for (child = doc->root->first_child; child != NULL; child = child->next) {
if (child->ns != ns)
continue;
if (strcmp(child->name, "lock-token-list") == 0)
break;
}
}
if (! child) {
*locks = hash;
return SVN_NO_ERROR;
}
for (lockchild = child->first_child; lockchild != NULL;
lockchild = lockchild->next) {
const char *lockpath = NULL, *locktoken = NULL;
apr_xml_elem *lfchild;
if (strcmp(lockchild->name, "lock") != 0)
continue;
for (lfchild = lockchild->first_child; lfchild != NULL;
lfchild = lfchild->next) {
if (strcmp(lfchild->name, "lock-path") == 0) {
const char *cdata = dav_xml_get_cdata(lfchild, pool, 0);
if ((derr = dav_svn__test_canonical(cdata, pool)))
return derr;
lockpath = svn_path_join(path_prefix, cdata, pool);
if (lockpath && locktoken) {
apr_hash_set(hash, lockpath, APR_HASH_KEY_STRING, locktoken);
lockpath = NULL;
locktoken = NULL;
}
} else if (strcmp(lfchild->name, "lock-token") == 0) {
locktoken = dav_xml_get_cdata(lfchild, pool, 1);
if (lockpath && *locktoken) {
apr_hash_set(hash, lockpath, APR_HASH_KEY_STRING, locktoken);
lockpath = NULL;
locktoken = NULL;
}
}
}
}
*locks = hash;
return SVN_NO_ERROR;
}
dav_error *
dav_svn__push_locks(dav_resource *resource,
apr_hash_t *locks,
apr_pool_t *pool) {
svn_fs_access_t *fsaccess;
apr_hash_index_t *hi;
svn_error_t *serr;
serr = svn_fs_get_access(&fsaccess, resource->info->repos->fs);
if (serr) {
return dav_svn__sanitize_error(serr, "Lock token(s) in request, but "
"missing an user name", HTTP_BAD_REQUEST,
resource->info->r);
}
for (hi = apr_hash_first(pool, locks); hi; hi = apr_hash_next(hi)) {
const char *token;
void *val;
apr_hash_this(hi, NULL, NULL, &val);
token = val;
serr = svn_fs_access_add_lock_token(fsaccess, token);
if (serr)
return dav_svn__convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
"Error pushing token into filesystem.",
pool);
}
return NULL;
}
static svn_error_t *
release_locks(apr_hash_t *locks,
svn_repos_t *repos,
request_rec *r,
apr_pool_t *pool) {
apr_hash_index_t *hi;
const void *key;
void *val;
apr_pool_t *subpool = svn_pool_create(pool);
svn_error_t *err;
for (hi = apr_hash_first(pool, locks); hi; hi = apr_hash_next(hi)) {
svn_pool_clear(subpool);
apr_hash_this(hi, &key, NULL, &val);
err = svn_repos_fs_unlock(repos, key, val, FALSE, subpool);
if (err)
ap_log_rerror(APLOG_MARK, APLOG_ERR, err->apr_err, r,
"%s", err->message);
svn_error_clear(err);
}
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
static dav_error *
merge(dav_resource *target,
dav_resource *source,
int no_auto_merge,
int no_checkout,
apr_xml_elem *prop_elem,
ap_filter_t *output) {
apr_pool_t *pool;
dav_error *err;
svn_fs_txn_t *txn;
const char *conflict;
svn_error_t *serr;
char *post_commit_err = NULL;
svn_revnum_t new_rev;
apr_hash_t *locks;
svn_boolean_t disable_merge_response = FALSE;
pool = target->pool;
if (source->type != DAV_RESOURCE_TYPE_ACTIVITY) {
return dav_svn__new_error_tag(pool, HTTP_METHOD_NOT_ALLOWED,
SVN_ERR_INCORRECT_PARAMS,
"MERGE can only be performed using an "
"activity as the source [at this time].",
SVN_DAV_ERROR_NAMESPACE,
SVN_DAV_ERROR_TAG);
}
err = dav_svn__build_lock_hash(&locks, target->info->r,
target->info->repos_path,
pool);
if (err != NULL)
return err;
if (apr_hash_count(locks)) {
err = dav_svn__push_locks(source, locks, pool);
if (err != NULL)
return err;
}
if ((err = open_txn(&txn, source->info->repos->fs,
source->info->root.txn_name, pool)) != NULL)
return err;
serr = svn_repos_fs_commit_txn(&conflict, source->info->repos->repos,
&new_rev, txn, pool);
if (serr && (serr->apr_err != SVN_ERR_REPOS_POST_COMMIT_HOOK_FAILED)) {
const char *msg;
svn_error_clear(svn_fs_abort_txn(txn, pool));
if (serr->apr_err == SVN_ERR_FS_CONFLICT) {
msg = apr_psprintf(pool,
"A conflict occurred during the MERGE "
"processing. The problem occurred with the "
"\"%s\" resource.",
conflict);
} else
msg = "An error occurred while committing the transaction.";
return dav_svn__convert_err(serr, HTTP_CONFLICT, msg, pool);
} else if (serr) {
if (serr->child && serr->child->message)
post_commit_err = apr_pstrdup(pool, serr->child->message);
svn_error_clear(serr);
}
register_deltification_cleanup(source->info->repos->repos, new_rev,
source->info->r->connection->pool);
dav_svn__operational_log(target->info,
apr_psprintf(target->info->r->pool,
"commit r%ld",
new_rev));
err = dav_svn__store_activity(source->info->repos,
source->info->root.activity_id, "");
if (err != NULL)
return err;
if (source->info->svn_client_options != NULL) {
if ((NULL != (ap_strstr_c(source->info->svn_client_options,
SVN_DAV_OPTION_RELEASE_LOCKS)))
&& apr_hash_count(locks)) {
serr = release_locks(locks, source->info->repos->repos,
source->info->r, pool);
if (serr != NULL)
return dav_svn__convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
"Error releasing locks", pool);
}
if (NULL != (ap_strstr_c(source->info->svn_client_options,
SVN_DAV_OPTION_NO_MERGE_RESPONSE)))
disable_merge_response = TRUE;
}
return dav_svn__merge_response(output, source->info->repos, new_rev,
post_commit_err, prop_elem,
disable_merge_response, pool);
}
const dav_hooks_vsn dav_svn__hooks_vsn = {
get_vsn_options,
get_option,
versionable,
auto_versionable,
vsn_control,
dav_svn__checkout,
uncheckout,
dav_svn__checkin,
avail_reports,
report_label_header_allowed,
deliver_report,
NULL,
NULL,
NULL,
NULL,
NULL,
can_be_activity,
make_activity,
merge,
};
