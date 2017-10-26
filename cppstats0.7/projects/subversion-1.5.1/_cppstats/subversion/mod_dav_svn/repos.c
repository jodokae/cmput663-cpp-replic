#define APR_WANT_STRFUNC
#include <apr_want.h>
#include <apr_strings.h>
#include <apr_hash.h>
#include <apr_lib.h>
#include <httpd.h>
#include <http_request.h>
#include <http_protocol.h>
#include <http_log.h>
#include <http_core.h>
#include <mod_dav.h>
#include "svn_types.h"
#include "svn_pools.h"
#include "svn_error.h"
#include "svn_time.h"
#include "svn_fs.h"
#include "svn_repos.h"
#include "svn_dav.h"
#include "svn_sorts.h"
#include "svn_version.h"
#include "svn_props.h"
#include "mod_dav_svn.h"
#include "svn_ra.h"
#include "dav_svn.h"
#define DEFAULT_ACTIVITY_DB "dav/activities.d"
struct dav_stream {
const dav_resource *res;
svn_stream_t *rstream;
svn_stream_t *wstream;
svn_txdelta_window_handler_t delta_handler;
void *delta_baton;
};
typedef struct {
dav_resource res;
dav_resource_private priv;
} dav_resource_combined;
static dav_error *
fs_check_path(svn_node_kind_t *kind,
svn_fs_root_t *root,
const char *path,
apr_pool_t *pool) {
svn_error_t *serr;
svn_node_kind_t my_kind;
serr = svn_fs_check_path(&my_kind, root, path, pool);
if (serr && serr->apr_err == SVN_ERR_FS_NOT_DIRECTORY) {
svn_error_clear(serr);
*kind = svn_node_none;
return NULL;
} else if (serr) {
return dav_svn__convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
apr_psprintf(pool, "Error checking kind of "
"path '%s' in repository",
path),
pool);
}
*kind = my_kind;
return NULL;
}
static int
parse_version_uri(dav_resource_combined *comb,
const char *path,
const char *label,
int use_checked_in) {
const char *slash;
const char *created_rev_str;
comb->res.type = DAV_RESOURCE_TYPE_VERSION;
comb->res.versioned = TRUE;
slash = ap_strchr_c(path, '/');
if (slash == NULL) {
created_rev_str = apr_pstrndup(comb->res.pool, path, strlen(path));
comb->priv.root.rev = SVN_STR_TO_REV(created_rev_str);
comb->priv.repos_path = "/";
} else if (slash == path) {
return TRUE;
} else {
apr_size_t len = slash - path;
created_rev_str = apr_pstrndup(comb->res.pool, path, len);
comb->priv.root.rev = SVN_STR_TO_REV(created_rev_str);
comb->priv.repos_path = slash;
}
if (comb->priv.root.rev == SVN_INVALID_REVNUM)
return TRUE;
return FALSE;
}
static int
parse_history_uri(dav_resource_combined *comb,
const char *path,
const char *label,
int use_checked_in) {
comb->res.type = DAV_RESOURCE_TYPE_HISTORY;
comb->priv.repos_path = path;
return FALSE;
}
static int
parse_working_uri(dav_resource_combined *comb,
const char *path,
const char *label,
int use_checked_in) {
const char *slash;
comb->res.type = DAV_RESOURCE_TYPE_WORKING;
comb->res.working = TRUE;
comb->res.versioned = TRUE;
slash = ap_strchr_c(path, '/');
if (slash == path)
return TRUE;
if (slash == NULL) {
comb->priv.root.activity_id = apr_pstrdup(comb->res.pool, path);
comb->priv.repos_path = "/";
} else {
comb->priv.root.activity_id = apr_pstrndup(comb->res.pool, path,
slash - path);
comb->priv.repos_path = slash;
}
return FALSE;
}
static int
parse_activity_uri(dav_resource_combined *comb,
const char *path,
const char *label,
int use_checked_in) {
comb->res.type = DAV_RESOURCE_TYPE_ACTIVITY;
comb->priv.root.activity_id = path;
return FALSE;
}
static int
parse_vcc_uri(dav_resource_combined *comb,
const char *path,
const char *label,
int use_checked_in) {
if (strcmp(path, DAV_SVN__DEFAULT_VCC_NAME) != 0)
return TRUE;
if (label == NULL && !use_checked_in) {
comb->res.type = DAV_RESOURCE_TYPE_PRIVATE;
comb->priv.restype = DAV_SVN_RESTYPE_VCC;
comb->res.exists = TRUE;
comb->res.versioned = TRUE;
comb->res.baselined = TRUE;
} else {
int revnum;
if (label != NULL) {
revnum = SVN_STR_TO_REV(label);
if (!SVN_IS_VALID_REVNUM(revnum))
return TRUE;
} else {
revnum = SVN_INVALID_REVNUM;
}
comb->res.type = DAV_RESOURCE_TYPE_VERSION;
comb->res.versioned = TRUE;
comb->res.baselined = TRUE;
comb->priv.root.rev = revnum;
}
return FALSE;
}
static int
parse_baseline_coll_uri(dav_resource_combined *comb,
const char *path,
const char *label,
int use_checked_in) {
const char *slash;
int revnum;
slash = ap_strchr_c(path, '/');
if (slash == NULL)
slash = "/";
else if (slash == path)
return TRUE;
revnum = SVN_STR_TO_REV(path);
if (!SVN_IS_VALID_REVNUM(revnum))
return TRUE;
comb->res.type = DAV_RESOURCE_TYPE_REGULAR;
comb->res.versioned = TRUE;
comb->priv.root.rev = revnum;
comb->priv.repos_path = slash;
return FALSE;
}
static int
parse_baseline_uri(dav_resource_combined *comb,
const char *path,
const char *label,
int use_checked_in) {
int revnum;
revnum = SVN_STR_TO_REV(path);
if (!SVN_IS_VALID_REVNUM(revnum))
return TRUE;
comb->res.type = DAV_RESOURCE_TYPE_VERSION;
comb->res.versioned = TRUE;
comb->res.baselined = TRUE;
comb->priv.root.rev = revnum;
return FALSE;
}
static int
parse_wrk_baseline_uri(dav_resource_combined *comb,
const char *path,
const char *label,
int use_checked_in) {
const char *slash;
comb->res.type = DAV_RESOURCE_TYPE_WORKING;
comb->res.working = TRUE;
comb->res.versioned = TRUE;
comb->res.baselined = TRUE;
if ((slash = ap_strchr_c(path, '/')) == NULL
|| slash == path
|| slash[1] == '\0')
return TRUE;
comb->priv.root.activity_id = apr_pstrndup(comb->res.pool, path,
slash - path);
comb->priv.root.rev = SVN_STR_TO_REV(slash + 1);
return FALSE;
}
static const struct special_defn {
const char *name;
int (*parse)(dav_resource_combined *comb, const char *path,
const char *label, int use_checked_in);
int numcomponents;
int has_repos_path;
enum dav_svn_private_restype restype;
} special_subdirs[] = {
{ "ver", parse_version_uri, 1, TRUE, DAV_SVN_RESTYPE_VER_COLLECTION },
{ "his", parse_history_uri, 0, FALSE, DAV_SVN_RESTYPE_HIS_COLLECTION },
{ "wrk", parse_working_uri, 1, TRUE, DAV_SVN_RESTYPE_WRK_COLLECTION },
{ "act", parse_activity_uri, 1, FALSE, DAV_SVN_RESTYPE_ACT_COLLECTION },
{ "vcc", parse_vcc_uri, 1, FALSE, DAV_SVN_RESTYPE_VCC_COLLECTION },
{ "bc", parse_baseline_coll_uri, 1, TRUE, DAV_SVN_RESTYPE_BC_COLLECTION },
{ "bln", parse_baseline_uri, 1, FALSE, DAV_SVN_RESTYPE_BLN_COLLECTION },
{ "wbl", parse_wrk_baseline_uri, 2, FALSE, DAV_SVN_RESTYPE_WBL_COLLECTION },
{ NULL }
};
static int
parse_uri(dav_resource_combined *comb,
const char *uri,
const char *label,
int use_checked_in) {
const char *special_uri = comb->priv.repos->special_uri;
apr_size_t len1;
apr_size_t len2;
char ch;
len1 = strlen(uri);
len2 = strlen(special_uri);
if (len1 > len2
&& ((ch = uri[len2]) == '/' || ch == '\0')
&& memcmp(uri, special_uri, len2) == 0) {
if (ch == '\0') {
comb->res.type = DAV_RESOURCE_TYPE_PRIVATE;
comb->priv.restype = DAV_SVN_RESTYPE_ROOT_COLLECTION;
} else {
const struct special_defn *defn;
uri += len2 + 1;
len1 -= len2 + 1;
for (defn = special_subdirs ; defn->name != NULL; ++defn) {
apr_size_t len3 = strlen(defn->name);
if (len1 >= len3 && memcmp(uri, defn->name, len3) == 0) {
if (uri[len3] == '\0') {
comb->res.type = DAV_RESOURCE_TYPE_PRIVATE;
comb->priv.restype = defn->restype;
} else if (uri[len3] == '/') {
if ((*defn->parse)(comb, uri + len3 + 1, label,
use_checked_in))
return TRUE;
} else {
return TRUE;
}
break;
}
}
if (defn->name == NULL)
return TRUE;
}
} else {
comb->res.type = DAV_RESOURCE_TYPE_REGULAR;
comb->res.versioned = TRUE;
comb->priv.repos_path = comb->priv.uri_path->data;
}
return FALSE;
}
static dav_error *
prep_regular(dav_resource_combined *comb) {
apr_pool_t *pool = comb->res.pool;
dav_svn_repos *repos = comb->priv.repos;
svn_error_t *serr;
dav_error *derr;
svn_node_kind_t kind;
if (comb->priv.root.rev == SVN_INVALID_REVNUM) {
serr = svn_fs_youngest_rev(&comb->priv.root.rev, repos->fs, pool);
if (serr != NULL) {
return dav_svn__convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
"Could not determine the proper "
"revision to access",
pool);
}
}
serr = svn_fs_revision_root(&comb->priv.root.root, repos->fs,
comb->priv.root.rev, pool);
if (serr != NULL) {
return dav_svn__convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
"Could not open the root of the "
"repository",
pool);
}
derr = fs_check_path(&kind, comb->priv.root.root,
comb->priv.repos_path, pool);
if (derr != NULL)
return derr;
comb->res.exists = (kind == svn_node_none) ? FALSE : TRUE;
comb->res.collection = (kind == svn_node_dir) ? TRUE : FALSE;
if (! comb->res.exists)
comb->priv.r->path_info = (char *) "";
return NULL;
}
static dav_error *
prep_version(dav_resource_combined *comb) {
svn_error_t *serr;
apr_pool_t *pool = comb->res.pool;
if (!SVN_IS_VALID_REVNUM(comb->priv.root.rev)) {
serr = svn_fs_youngest_rev(&comb->priv.root.rev,
comb->priv.repos->fs,
pool);
if (serr != NULL) {
return dav_svn__convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
"Could not fetch 'youngest' revision "
"to enable accessing the latest "
"baseline resource.",
pool);
}
}
if (!comb->priv.root.root) {
serr = svn_fs_revision_root(&comb->priv.root.root,
comb->priv.repos->fs,
comb->priv.root.rev,
pool);
if (serr != NULL) {
return dav_svn__convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
"Could not open a revision root.",
pool);
}
}
comb->res.exists = TRUE;
comb->res.uri = dav_svn__build_uri(comb->priv.repos,
DAV_SVN__BUILD_URI_BASELINE,
comb->priv.root.rev, NULL,
0 ,
pool);
return NULL;
}
static dav_error *
prep_history(dav_resource_combined *comb) {
return NULL;
}
static dav_error *
prep_working(dav_resource_combined *comb) {
const char *txn_name = dav_svn__get_txn(comb->priv.repos,
comb->priv.root.activity_id);
apr_pool_t *pool = comb->res.pool;
svn_error_t *serr;
dav_error *derr;
svn_node_kind_t kind;
if (txn_name == NULL) {
return dav_new_error(pool, HTTP_BAD_REQUEST, 0,
"An unknown activity was specified in the URL. "
"This is generally caused by a problem in the "
"client software.");
}
comb->priv.root.txn_name = txn_name;
serr = svn_fs_open_txn(&comb->priv.root.txn, comb->priv.repos->fs, txn_name,
pool);
if (serr != NULL) {
if (serr->apr_err == SVN_ERR_FS_NO_SUCH_TRANSACTION) {
svn_error_clear(serr);
return dav_new_error(pool, HTTP_INTERNAL_SERVER_ERROR, 0,
"An activity was specified and found, but the "
"corresponding SVN FS transaction was not "
"found.");
}
return dav_svn__convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
"Could not open the SVN FS transaction "
"corresponding to the specified activity.",
pool);
}
if (comb->res.baselined) {
comb->res.exists = TRUE;
return NULL;
}
if (comb->priv.repos->username) {
svn_string_t *current_author;
svn_string_t request_author;
serr = svn_fs_txn_prop(&current_author, comb->priv.root.txn,
SVN_PROP_REVISION_AUTHOR, pool);
if (serr != NULL) {
return dav_svn__convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
"Failed to retrieve author of the SVN FS transaction "
"corresponding to the specified activity.",
pool);
}
request_author.data = comb->priv.repos->username;
request_author.len = strlen(request_author.data);
if (!current_author) {
serr = svn_fs_change_txn_prop(comb->priv.root.txn,
SVN_PROP_REVISION_AUTHOR, &request_author, pool);
if (serr != NULL) {
return dav_svn__convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
"Failed to set the author of the SVN FS transaction "
"corresponding to the specified activity.",
pool);
}
} else if (!svn_string_compare(current_author, &request_author)) {
return dav_new_error(pool, HTTP_NOT_IMPLEMENTED, 0,
"Multi-author commits not supported.");
}
}
serr = svn_fs_txn_root(&comb->priv.root.root, comb->priv.root.txn, pool);
if (serr != NULL) {
return dav_svn__convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
"Could not open the (transaction) root of "
"the repository",
pool);
}
derr = fs_check_path(&kind, comb->priv.root.root,
comb->priv.repos_path, pool);
if (derr != NULL)
return derr;
comb->res.exists = (kind == svn_node_none) ? FALSE : TRUE;
comb->res.collection = (kind == svn_node_dir) ? TRUE : FALSE;
return NULL;
}
static dav_error *
prep_activity(dav_resource_combined *comb) {
const char *txn_name = dav_svn__get_txn(comb->priv.repos,
comb->priv.root.activity_id);
comb->priv.root.txn_name = txn_name;
comb->res.exists = txn_name != NULL;
return NULL;
}
static dav_error *
prep_private(dav_resource_combined *comb) {
if (comb->priv.restype == DAV_SVN_RESTYPE_VCC) {
}
return NULL;
}
static const struct res_type_handler {
dav_resource_type type;
dav_error * (*prep)(dav_resource_combined *comb);
} res_type_handlers[] = {
{ DAV_RESOURCE_TYPE_REGULAR, prep_regular },
{ DAV_RESOURCE_TYPE_VERSION, prep_version },
{ DAV_RESOURCE_TYPE_HISTORY, prep_history },
{ DAV_RESOURCE_TYPE_WORKING, prep_working },
{ DAV_RESOURCE_TYPE_ACTIVITY, prep_activity },
{ DAV_RESOURCE_TYPE_PRIVATE, prep_private },
{ 0, NULL }
};
static dav_error *
prep_resource(dav_resource_combined *comb) {
const struct res_type_handler *scan;
for (scan = res_type_handlers; scan->prep != NULL; ++scan) {
if (comb->res.type == scan->type)
return (*scan->prep)(comb);
}
return dav_new_error(comb->res.pool, HTTP_INTERNAL_SERVER_ERROR, 0,
"DESIGN FAILURE: unknown resource type");
}
static dav_resource *
create_private_resource(const dav_resource *base,
enum dav_svn_private_restype restype) {
dav_resource_combined *comb;
svn_stringbuf_t *path;
const struct special_defn *defn;
for (defn = special_subdirs; defn->name != NULL; ++defn)
if (defn->restype == restype)
break;
path = svn_stringbuf_createf(base->pool, "/%s/%s",
base->info->repos->special_uri, defn->name);
comb = apr_pcalloc(base->pool, sizeof(*comb));
comb->res.type = DAV_RESOURCE_TYPE_PRIVATE;
comb->res.exists = TRUE;
comb->res.collection = TRUE;
comb->res.uri = apr_pstrcat(base->pool, base->info->repos->root_path,
path->data, NULL);
comb->res.info = &comb->priv;
comb->res.hooks = &dav_svn__hooks_repository;
comb->res.pool = base->pool;
comb->priv.uri_path = path;
comb->priv.repos = base->info->repos;
comb->priv.root.rev = SVN_INVALID_REVNUM;
return &comb->res;
}
static void log_warning(void *baton, svn_error_t *err) {
request_rec *r = baton;
ap_log_rerror(APLOG_MARK, APLOG_ERR, APR_EGENERAL, r, "%s", err->message);
}
AP_MODULE_DECLARE(dav_error *)
dav_svn_split_uri(request_rec *r,
const char *uri_to_split,
const char *root_path,
const char **cleaned_uri,
int *trailing_slash,
const char **repos_name,
const char **relative_path,
const char **repos_path) {
apr_size_t len1;
int had_slash;
const char *fs_path;
const char *fs_parent_path;
const char *relative;
char *uri;
fs_path = dav_svn__get_fs_path(r);
fs_parent_path = dav_svn__get_fs_parent_path(r);
if ((fs_path == NULL) && (fs_parent_path == NULL)) {
return dav_new_error(r->pool, HTTP_INTERNAL_SERVER_ERROR,
SVN_ERR_APMOD_MISSING_PATH_TO_FS,
"The server is misconfigured: "
"either an SVNPath or SVNParentPath "
"directive is required to specify the location "
"of this resource's repository.");
}
uri = apr_pstrdup(r->pool, uri_to_split);
ap_no2slash(uri);
len1 = strlen(uri);
had_slash = (len1 > 0 && uri[len1 - 1] == '/');
if (len1 > 1 && had_slash)
uri[len1 - 1] = '\0';
if (had_slash)
*trailing_slash = TRUE;
else
*trailing_slash = FALSE;
*cleaned_uri = apr_pstrdup(r->pool, uri);
relative = ap_stripprefix(uri, root_path);
if (*relative == '\0')
relative = "/";
else if (*relative != '/')
--relative;
if (fs_path != NULL) {
*repos_name = svn_path_basename(root_path, r->pool);
}
else {
const char *magic_component, *magic_end;
if (relative[1] == '\0') {
return dav_new_error(r->pool, HTTP_FORBIDDEN,
SVN_ERR_APMOD_MALFORMED_URI,
"The URI does not contain the name "
"of a repository.");
}
magic_end = ap_strchr_c(relative + 1, '/');
if (!magic_end) {
magic_component = relative + 1;
relative = "/";
} else {
magic_component = apr_pstrndup(r->pool, relative + 1,
magic_end - relative - 1);
relative = magic_end;
}
*repos_name = magic_component;
}
*relative_path = apr_pstrdup(r->pool, relative);
relative++;
{
const char *special_uri = dav_svn__get_special_uri(r);
apr_size_t len2;
char ch;
len1 = strlen(relative);
len2 = strlen(special_uri);
if (len1 > len2
&& ((ch = relative[len2]) == '/' || ch == '\0')
&& memcmp(relative, special_uri, len2) == 0) {
if (ch == '\0') {
return dav_new_error(r->pool, HTTP_INTERNAL_SERVER_ERROR,
SVN_ERR_APMOD_MALFORMED_URI,
"Nothing follows the svn special_uri.");
} else {
const struct special_defn *defn;
relative += len2 + 1;
len1 -= len2 + 1;
for (defn = special_subdirs ; defn->name != NULL; ++defn) {
apr_size_t len3 = strlen(defn->name);
if (len1 >= len3 && memcmp(relative, defn->name, len3) == 0) {
if (relative[len3] == '\0') {
if (defn->numcomponents == 0)
*repos_path = NULL;
else
return
dav_new_error(r->pool, HTTP_INTERNAL_SERVER_ERROR,
SVN_ERR_APMOD_MALFORMED_URI,
"Missing info after special_uri.");
} else if (relative[len3] == '/') {
int j;
const char *end = NULL, *start = relative + len3 + 1;
for (j = 0; j < defn->numcomponents; j++) {
end = ap_strchr_c(start, '/');
if (! end)
break;
start = end + 1;
}
if (! end) {
if (j != (defn->numcomponents - 1))
return
dav_new_error(r->pool,
HTTP_INTERNAL_SERVER_ERROR,
SVN_ERR_APMOD_MALFORMED_URI,
"Not enough components"
" after special_uri.");
if (! defn->has_repos_path)
*repos_path = NULL;
else
*repos_path = "/";
} else {
*repos_path = apr_pstrdup(r->pool, start);
}
} else {
return
dav_new_error(r->pool, HTTP_INTERNAL_SERVER_ERROR,
SVN_ERR_APMOD_MALFORMED_URI,
"Unknown data after special_uri.");
}
break;
}
}
if (defn->name == NULL)
return
dav_new_error(r->pool, HTTP_INTERNAL_SERVER_ERROR,
SVN_ERR_APMOD_MALFORMED_URI,
"Couldn't match subdir after special_uri.");
}
} else {
*repos_path = apr_pstrdup(r->pool, relative);
}
}
return NULL;
}
struct cleanup_fs_access_baton {
svn_fs_t *fs;
apr_pool_t *pool;
};
static apr_status_t
cleanup_fs_access(void *data) {
svn_error_t *serr;
struct cleanup_fs_access_baton *baton = data;
serr = svn_fs_set_access(baton->fs, NULL);
if (serr) {
ap_log_perror(APLOG_MARK, APLOG_ERR, serr->apr_err, baton->pool,
"cleanup_fs_access: error clearing fs access context");
svn_error_clear(serr);
}
return APR_SUCCESS;
}
static dav_error *
get_parentpath_resource(request_rec *r,
const char *root_path,
dav_resource **resource) {
const char *new_uri;
dav_svn_root *droot = apr_pcalloc(r->pool, sizeof(*droot));
dav_svn_repos *repos = apr_pcalloc(r->pool, sizeof(*repos));
dav_resource_combined *comb = apr_pcalloc(r->pool, sizeof(*comb));
apr_size_t len = strlen(r->uri);
comb->res.exists = TRUE;
comb->res.collection = TRUE;
comb->res.uri = apr_pstrdup(r->pool, r->uri);
comb->res.info = &comb->priv;
comb->res.hooks = &dav_svn__hooks_repository;
comb->res.pool = r->pool;
comb->res.type = DAV_RESOURCE_TYPE_PRIVATE;
comb->priv.restype = DAV_SVN_RESTYPE_PARENTPATH_COLLECTION;
comb->priv.r = r;
comb->priv.repos_path = "Collection of Repositories";
comb->priv.root = *droot;
droot->rev = SVN_INVALID_REVNUM;
comb->priv.repos = repos;
repos->pool = r->pool;
repos->xslt_uri = dav_svn__get_xslt_uri(r);
repos->autoversioning = dav_svn__get_autoversioning_flag(r);
repos->bulk_updates = dav_svn__get_bulk_updates_flag(r);
repos->base_url = ap_construct_url(r->pool, "", r);
repos->special_uri = dav_svn__get_special_uri(r);
repos->username = r->user;
repos->capabilities = apr_hash_make(repos->pool);
if (r->uri[len-1] != '/') {
new_uri = apr_pstrcat(r->pool, ap_escape_uri(r->pool, r->uri),
"/", NULL);
apr_table_setn(r->headers_out, "Location",
ap_construct_url(r->pool, new_uri, r));
return dav_new_error(r->pool, HTTP_MOVED_PERMANENTLY, 0,
"Requests for a collection must have a "
"trailing slash on the URI.");
}
*resource = &comb->res;
return NULL;
}
typedef struct accept_rec {
char *name;
float quality;
} accept_rec;
static const char *get_entry(apr_pool_t *p, accept_rec *result,
const char *accept_line) {
result->quality = 1.0f;
result->name = ap_get_token(p, &accept_line, 0);
ap_str_tolower(result->name);
while (*accept_line == ';') {
char *parm;
char *cp;
char *end;
++accept_line;
parm = ap_get_token(p, &accept_line, 1);
for (cp = parm; (*cp && !apr_isspace(*cp) && *cp != '='); ++cp) {
*cp = apr_tolower(*cp);
}
if (!*cp) {
continue;
}
*cp++ = '\0';
while (*cp && (apr_isspace(*cp) || *cp == '=')) {
++cp;
}
if (*cp == '"') {
++cp;
for (end = cp;
(*end && *end != '\n' && *end != '\r' && *end != '\"');
end++);
} else {
for (end = cp; (*end && !apr_isspace(*end)); end++);
}
if (*end) {
*end = '\0';
}
ap_str_tolower(cp);
if (parm[0] == 'q'
&& (parm[1] == '\0' || (parm[1] == 's' && parm[2] == '\0'))) {
result->quality = (float) atof(cp);
}
}
if (*accept_line == ',') {
++accept_line;
}
return accept_line;
}
static apr_array_header_t *do_header_line(apr_pool_t *p,
const char *accept_line) {
apr_array_header_t *accept_recs;
if (!accept_line)
return NULL;
accept_recs = apr_array_make(p, 10, sizeof(accept_rec));
while (*accept_line) {
accept_rec *prefs = (accept_rec *) apr_array_push(accept_recs);
accept_line = get_entry(p, prefs, accept_line);
}
return accept_recs;
}
static int sort_encoding_pref(const void *accept_rec1, const void *accept_rec2) {
float diff = ((const accept_rec *) accept_rec1)->quality -
((const accept_rec *) accept_rec2)->quality;
return (diff == 0 ? 0 : (diff > 0 ? -1 : 1));
}
static void
negotiate_encoding_prefs(request_rec *r, int *svndiff_version) {
int i;
const apr_array_header_t *encoding_prefs;
encoding_prefs = do_header_line(r->pool,
apr_table_get(r->headers_in,
"Accept-Encoding"));
if (!encoding_prefs || apr_is_empty_array(encoding_prefs)) {
*svndiff_version = 0;
return;
}
*svndiff_version = 0;
qsort(encoding_prefs->elts, (size_t) encoding_prefs->nelts,
sizeof(accept_rec), sort_encoding_pref);
for (i = 0; i < encoding_prefs->nelts; i++) {
struct accept_rec rec = APR_ARRAY_IDX(encoding_prefs, i,
struct accept_rec);
if (strcmp(rec.name, "svndiff1") == 0) {
*svndiff_version = 1;
break;
} else if (strcmp(rec.name, "svndiff") == 0) {
*svndiff_version = 0;
break;
}
}
}
static const char *capability_yes = "yes";
static const char *capability_no = "no";
static apr_array_header_t *
capabilities_as_list(apr_hash_t *capabilities, apr_pool_t *pool) {
apr_array_header_t *list = apr_array_make(pool, apr_hash_count(capabilities),
sizeof(char *));
apr_hash_index_t *hi;
for (hi = apr_hash_first(pool, capabilities); hi; hi = apr_hash_next(hi)) {
const void *key;
void *val;
apr_hash_this(hi, &key, NULL, &val);
if (strcmp((const char *) val, "yes") == 0)
APR_ARRAY_PUSH(list, const char *) = key;
}
return list;
}
static dav_error *
get_resource(request_rec *r,
const char *root_path,
const char *label,
int use_checked_in,
dav_resource **resource) {
const char *fs_path;
const char *repo_name;
const char *xslt_uri;
const char *fs_parent_path;
dav_resource_combined *comb;
dav_svn_repos *repos;
const char *cleaned_uri;
const char *repos_name;
const char *relative;
const char *repos_path;
const char *repos_key;
const char *version_name;
svn_error_t *serr;
dav_error *err;
int had_slash;
dav_locktoken_list *ltl;
struct cleanup_fs_access_baton *cleanup_baton;
void *userdata;
repo_name = dav_svn__get_repo_name(r);
xslt_uri = dav_svn__get_xslt_uri(r);
fs_parent_path = dav_svn__get_fs_parent_path(r);
if (fs_parent_path && dav_svn__get_list_parentpath_flag(r)) {
char *uri = apr_pstrdup(r->pool, r->uri);
char *parentpath = apr_pstrdup(r->pool, root_path);
apr_size_t uri_len = strlen(uri);
apr_size_t parentpath_len = strlen(parentpath);
if (uri[uri_len-1] == '/')
uri[uri_len-1] = '\0';
if (parentpath[parentpath_len-1] == '/')
parentpath[parentpath_len-1] = '\0';
if (strcmp(parentpath, uri) == 0) {
err = get_parentpath_resource(r, root_path, resource);
if (err)
return err;
return NULL;
}
}
err = dav_svn_split_uri(r, r->uri, root_path,
&cleaned_uri, &had_slash,
&repos_name, &relative, &repos_path);
if (err)
return err;
fs_path = dav_svn__get_fs_path(r);
if (fs_parent_path != NULL) {
root_path = svn_path_join(root_path, repos_name, r->pool);
fs_path = svn_path_join(fs_parent_path, repos_name, r->pool);
}
comb = apr_pcalloc(r->pool, sizeof(*comb));
comb->res.info = &comb->priv;
comb->res.hooks = &dav_svn__hooks_repository;
comb->res.pool = r->pool;
comb->res.uri = cleaned_uri;
comb->priv.r = r;
{
const char *ct = apr_table_get(r->headers_in, "content-type");
comb->priv.is_svndiff =
ct != NULL
&& strcmp(ct, SVN_SVNDIFF_MIME_TYPE) == 0;
}
negotiate_encoding_prefs(r, &comb->priv.svndiff_version);
comb->priv.delta_base = apr_table_get(r->headers_in,
SVN_DAV_DELTA_BASE_HEADER);
comb->priv.svn_client_options = apr_table_get(r->headers_in,
SVN_DAV_OPTIONS_HEADER);
version_name = apr_table_get(r->headers_in, SVN_DAV_VERSION_NAME_HEADER);
comb->priv.version_name
= version_name ? SVN_STR_TO_REV(version_name): SVN_INVALID_REVNUM;
comb->priv.base_checksum =
apr_table_get(r->headers_in, SVN_DAV_BASE_FULLTEXT_MD5_HEADER);
comb->priv.result_checksum =
apr_table_get(r->headers_in, SVN_DAV_RESULT_FULLTEXT_MD5_HEADER);
comb->priv.uri_path = svn_stringbuf_create(relative, r->pool);
comb->priv.root.rev = SVN_INVALID_REVNUM;
repos = apr_pcalloc(r->pool, sizeof(*repos));
repos->pool = r->pool;
comb->priv.repos = repos;
repos->root_path = svn_path_uri_encode(root_path, r->pool);
repos->fs_path = fs_path;
repos->repo_name = repo_name;
repos->repo_basename = repos_name;
repos->xslt_uri = xslt_uri;
repos->autoversioning = dav_svn__get_autoversioning_flag(r);
repos->bulk_updates = dav_svn__get_bulk_updates_flag(r);
repos->activities_db = dav_svn__get_activities_db(r);
if (repos->activities_db == NULL)
repos->activities_db = svn_path_join(repos->fs_path,
DEFAULT_ACTIVITY_DB,
r->pool);
else if (fs_parent_path != NULL)
repos->activities_db = svn_path_join(repos->activities_db,
svn_path_basename(repos->fs_path,
r->pool),
r->pool);
repos->base_url = ap_construct_url(r->pool, "", r);
repos->special_uri = dav_svn__get_special_uri(r);
repos->username = r->user;
repos->capabilities = apr_hash_make(repos->pool);
{
const char *val = apr_table_get(r->headers_in, "User-Agent");
if (val && (ap_strstr_c(val, "SVN/") == val)) {
repos->is_svn_client = TRUE;
apr_hash_set(repos->capabilities, SVN_RA_CAPABILITY_MERGEINFO,
APR_HASH_KEY_STRING, capability_no);
val = apr_table_get(r->headers_in, "DAV");
if (val) {
apr_array_header_t *vals
= svn_cstring_split(val, ",", TRUE, r->pool);
if (svn_cstring_match_glob_list(SVN_DAV_NS_DAV_SVN_MERGEINFO,
vals)) {
apr_hash_set(repos->capabilities, SVN_RA_CAPABILITY_MERGEINFO,
APR_HASH_KEY_STRING, capability_yes);
}
}
}
}
repos_key = apr_pstrcat(r->pool, "mod_dav_svn:", fs_path, NULL);
apr_pool_userdata_get(&userdata, repos_key, r->connection->pool);
repos->repos = userdata;
if (repos->repos == NULL) {
serr = svn_repos_open(&(repos->repos), fs_path, r->connection->pool);
if (serr != NULL) {
return dav_svn__sanitize_error(serr, "Could not open the requested "
"SVN filesystem",
HTTP_INTERNAL_SERVER_ERROR, r);
}
apr_pool_userdata_set(repos->repos, repos_key,
NULL, r->connection->pool);
serr = svn_repos_remember_client_capabilities
(repos->repos, capabilities_as_list(repos->capabilities,
r->connection->pool));
if (serr != NULL) {
return dav_svn__sanitize_error(serr,
"Error storing client capabilities "
"in repos object",
HTTP_INTERNAL_SERVER_ERROR, r);
}
}
repos->fs = svn_repos_fs(repos->repos);
svn_fs_set_warning_func(repos->fs, log_warning, r);
if (r->user) {
svn_fs_access_t *access_ctx;
cleanup_baton = apr_pcalloc(r->pool, sizeof(*cleanup_baton));
cleanup_baton->pool = r->pool;
cleanup_baton->fs = repos->fs;
apr_pool_cleanup_register(r->pool, cleanup_baton, cleanup_fs_access,
apr_pool_cleanup_null);
serr = svn_fs_create_access(&access_ctx, r->user, r->pool);
if (serr) {
return dav_svn__sanitize_error(serr,
"Could not create fs access context",
HTTP_INTERNAL_SERVER_ERROR, r);
}
serr = svn_fs_set_access(repos->fs, access_ctx);
if (serr) {
return dav_svn__sanitize_error(serr, "Could not attach access "
"context to fs",
HTTP_INTERNAL_SERVER_ERROR, r);
}
}
err = dav_get_locktoken_list(r, &ltl);
if (err && (err->error_id != DAV_ERR_IF_ABSENT))
return err;
if (ltl) {
svn_fs_access_t *access_ctx;
dav_locktoken_list *list = ltl;
serr = svn_fs_get_access(&access_ctx, repos->fs);
if (serr) {
return dav_svn__sanitize_error(serr, "Lock token is in request, "
"but no user name",
HTTP_BAD_REQUEST, r);
}
do {
serr = svn_fs_access_add_lock_token(access_ctx,
list->locktoken->uuid_str);
if (serr)
return dav_svn__convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
"Error pushing token into filesystem.",
r->pool);
list = list->next;
} while (list);
}
if (parse_uri(comb, relative + 1, label, use_checked_in))
goto malformed_URI;
#if defined(SVN_DEBUG)
if (comb->res.type == DAV_RESOURCE_TYPE_UNKNOWN) {
DBG0("DESIGN FAILURE: should not be UNKNOWN at this point");
*resource = NULL;
return NULL;
}
#endif
if ((err = prep_resource(comb)) != NULL)
return err;
if (comb->res.collection && comb->res.type == DAV_RESOURCE_TYPE_REGULAR
&& !had_slash && r->method_number == M_GET) {
const char *new_path = apr_pstrcat(r->pool,
ap_escape_uri(r->pool, r->uri),
"/",
NULL);
apr_table_setn(r->headers_out, "Location",
ap_construct_url(r->pool, new_path, r));
return dav_new_error(r->pool, HTTP_MOVED_PERMANENTLY, 0,
"Requests for a collection must have a "
"trailing slash on the URI.");
}
*resource = &comb->res;
return NULL;
malformed_URI:
return dav_new_error(r->pool, HTTP_INTERNAL_SERVER_ERROR,
SVN_ERR_APMOD_MALFORMED_URI,
"The URI indicated a resource within Subversion's "
"special resource area, but does not exist. This is "
"generally caused by a problem in the client "
"software.");
}
static const char *
get_parent_path(const char *path, apr_pool_t *pool) {
apr_size_t len;
const char *parentpath, *base_name;
char *tmp = apr_pstrdup(pool, path);
len = strlen(tmp);
if (len > 0) {
if (tmp[len-1] == '/')
tmp[len-1] = '\0';
svn_path_split(tmp, &parentpath, &base_name, pool);
return parentpath;
}
return path;
}
static dav_error *
get_parent_resource(const dav_resource *resource,
dav_resource **parent_resource) {
dav_resource *parent;
dav_resource_private *parentinfo;
svn_stringbuf_t *path = resource->info->uri_path;
if (path->len == 1 && *path->data == '/') {
*parent_resource = NULL;
return NULL;
}
switch (resource->type) {
case DAV_RESOURCE_TYPE_REGULAR:
parent = apr_pcalloc(resource->pool, sizeof(*parent));
parentinfo = apr_pcalloc(resource->pool, sizeof(*parentinfo));
parent->type = DAV_RESOURCE_TYPE_REGULAR;
parent->exists = 1;
parent->collection = 1;
parent->versioned = 1;
parent->hooks = resource->hooks;
parent->pool = resource->pool;
parent->uri = get_parent_path(resource->uri, resource->pool);
parent->info = parentinfo;
parentinfo->pool = resource->info->pool;
parentinfo->uri_path =
svn_stringbuf_create(get_parent_path(resource->info->uri_path->data,
resource->pool), resource->pool);
parentinfo->repos = resource->info->repos;
parentinfo->root = resource->info->root;
parentinfo->r = resource->info->r;
parentinfo->svn_client_options = resource->info->svn_client_options;
parentinfo->repos_path = get_parent_path(resource->info->repos_path,
resource->pool);
*parent_resource = parent;
break;
case DAV_RESOURCE_TYPE_WORKING:
*parent_resource =
create_private_resource(resource, DAV_SVN_RESTYPE_WRK_COLLECTION);
break;
case DAV_RESOURCE_TYPE_ACTIVITY:
*parent_resource =
create_private_resource(resource, DAV_SVN_RESTYPE_ACT_COLLECTION);
break;
default:
return dav_new_error(resource->pool, HTTP_INTERNAL_SERVER_ERROR, 0,
apr_psprintf(resource->pool,
"get_parent_resource was called for "
"%s (type %d)",
resource->uri, resource->type));
break;
}
return NULL;
}
static int
is_our_resource(const dav_resource *res1, const dav_resource *res2) {
if (res1->hooks != res2->hooks
|| strcmp(res1->info->repos->fs_path, res2->info->repos->fs_path) != 0) {
return 0;
}
if (res1->info->repos != res2->info->repos) {
res2->info->repos = res1->info->repos;
if (res2->info->root.txn_name) {
svn_error_clear(svn_fs_open_txn(&(res2->info->root.txn),
res2->info->repos->fs,
res2->info->root.txn_name,
res2->info->repos->pool));
svn_error_clear(svn_fs_txn_root(&(res2->info->root.root),
res2->info->root.txn,
res2->info->repos->pool));
} else if (res2->info->root.rev) {
svn_error_clear(svn_fs_revision_root(&(res2->info->root.root),
res2->info->repos->fs,
res2->info->root.rev,
res2->info->repos->pool));
}
}
return 1;
}
static int
is_same_resource(const dav_resource *res1, const dav_resource *res2) {
if (!is_our_resource(res1, res2))
return 0;
return svn_stringbuf_compare(res1->info->uri_path, res2->info->uri_path);
}
static int
is_parent_resource(const dav_resource *res1, const dav_resource *res2) {
apr_size_t len1 = strlen(res1->info->uri_path->data);
apr_size_t len2;
if (!is_our_resource(res1, res2))
return 0;
len2 = strlen(res2->info->uri_path->data);
return (len2 > len1
&& memcmp(res1->info->uri_path->data, res2->info->uri_path->data,
len1) == 0
&& res2->info->uri_path->data[len1] == '/');
}
#if 0
static dav_error *
resource_kind(request_rec *r,
const char *uri,
const char *root_path,
svn_node_kind_t *kind) {
dav_error *derr;
svn_error_t *serr;
dav_resource *resource;
svn_revnum_t base_rev;
svn_fs_root_t *base_rev_root;
char *saved_uri;
saved_uri = r->uri;
r->uri = apr_pstrdup(r->pool, uri);
derr = get_resource(r, root_path,
"ignored_label", 1,
&resource);
r->uri = saved_uri;
if (derr)
return derr;
if (resource->type == DAV_RESOURCE_TYPE_REGULAR) {
if (! resource->exists)
*kind = svn_node_none;
else
*kind = resource->collection ? svn_node_dir : svn_node_file;
}
else if (resource->type == DAV_RESOURCE_TYPE_VERSION) {
if (resource->baselined)
*kind = svn_node_unknown;
else {
derr = fs_check_path(kind, resource->info->root.root,
resource->info->repos_path, r->pool);
if (derr != NULL)
return derr;
}
}
else if (resource->type == DAV_RESOURCE_TYPE_WORKING) {
if (resource->baselined)
*kind = svn_node_unknown;
else {
base_rev = svn_fs_txn_base_revision(resource->info->root.txn);
serr = svn_fs_revision_root(&base_rev_root,
resource->info->repos->fs,
base_rev, r->pool);
if (serr)
return dav_svn__convert_err
(serr, HTTP_INTERNAL_SERVER_ERROR,
apr_psprintf(r->pool,
"Could not open root of revision %ld",
base_rev),
r->pool);
derr = fs_check_path(kind, base_rev_root,
resource->info->repos_path, r->pool);
if (derr != NULL)
return derr;
}
}
else
*kind = svn_node_unknown;
return NULL;
}
#endif
static dav_error *
open_stream(const dav_resource *resource,
dav_stream_mode mode,
dav_stream **stream) {
svn_node_kind_t kind;
dav_error *derr;
svn_error_t *serr;
if (mode == DAV_MODE_WRITE_TRUNC || mode == DAV_MODE_WRITE_SEEKABLE) {
if (resource->type != DAV_RESOURCE_TYPE_WORKING) {
return dav_new_error(resource->pool, HTTP_METHOD_NOT_ALLOWED, 0,
"Resource body changes may only be made to "
"working resources [at this time].");
}
}
#if 1
if (mode == DAV_MODE_WRITE_SEEKABLE) {
return dav_new_error(resource->pool, HTTP_NOT_IMPLEMENTED, 0,
"Resource body writes cannot use ranges "
"[at this time].");
}
#endif
*stream = apr_pcalloc(resource->pool, sizeof(**stream));
(*stream)->res = resource;
derr = fs_check_path(&kind, resource->info->root.root,
resource->info->repos_path, resource->pool);
if (derr != NULL)
return derr;
if (kind == svn_node_none) {
serr = svn_fs_make_file(resource->info->root.root,
resource->info->repos_path,
resource->pool);
if (serr != NULL) {
return dav_svn__convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
"Could not create file within the "
"repository.",
resource->pool);
}
}
if (resource->info->auto_checked_out
&& resource->info->r->content_type) {
svn_string_t *mime_type;
serr = svn_fs_node_prop(&mime_type,
resource->info->root.root,
resource->info->repos_path,
SVN_PROP_MIME_TYPE,
resource->pool);
if (serr != NULL) {
return dav_svn__convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
"Error fetching mime-type property.",
resource->pool);
}
if (!mime_type) {
serr = svn_fs_change_node_prop(resource->info->root.root,
resource->info->repos_path,
SVN_PROP_MIME_TYPE,
svn_string_create
(resource->info->r->content_type,
resource->pool),
resource->pool);
if (serr != NULL) {
return dav_svn__convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
"Could not set mime-type property.",
resource->pool);
}
}
}
serr = svn_fs_apply_textdelta(&(*stream)->delta_handler,
&(*stream)->delta_baton,
resource->info->root.root,
resource->info->repos_path,
resource->info->base_checksum,
resource->info->result_checksum,
resource->pool);
if (serr != NULL) {
return dav_svn__convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
"Could not prepare to write the file",
resource->pool);
}
if (resource->info->is_svndiff) {
(*stream)->wstream =
svn_txdelta_parse_svndiff((*stream)->delta_handler,
(*stream)->delta_baton,
TRUE,
resource->pool);
}
return NULL;
}
static dav_error *
close_stream(dav_stream *stream, int commit) {
svn_error_t *serr;
apr_pool_t *pool = stream->res->pool;
if (stream->rstream != NULL) {
serr = svn_stream_close(stream->rstream);
if (serr)
return dav_svn__convert_err
(serr, HTTP_INTERNAL_SERVER_ERROR,
"mod_dav_svn close_stream: error closing read stream",
pool);
}
if (stream->wstream != NULL) {
serr = svn_stream_close(stream->wstream);
if (serr)
return dav_svn__convert_err
(serr, HTTP_INTERNAL_SERVER_ERROR,
"mod_dav_svn close_stream: error closing write stream",
pool);
} else if (stream->delta_handler != NULL) {
serr = (*stream->delta_handler)(NULL, stream->delta_baton);
if (serr)
return dav_svn__convert_err
(serr, HTTP_INTERNAL_SERVER_ERROR,
"mod_dav_svn close_stream: error sending final (null) delta window",
pool);
}
return NULL;
}
static dav_error *
write_stream(dav_stream *stream, const void *buf, apr_size_t bufsize) {
svn_error_t *serr;
apr_pool_t *pool = stream->res->pool;
if (stream->wstream != NULL) {
serr = svn_stream_write(stream->wstream, buf, &bufsize);
} else {
svn_txdelta_window_t window = { 0 };
svn_txdelta_op_t op;
svn_string_t data;
data.data = buf;
data.len = bufsize;
op.action_code = svn_txdelta_new;
op.offset = 0;
op.length = bufsize;
window.tview_len = bufsize;
window.num_ops = 1;
window.ops = &op;
window.new_data = &data;
serr = (*stream->delta_handler)(&window, stream->delta_baton);
}
if (serr) {
return dav_svn__convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
"could not write the file contents",
pool);
}
return NULL;
}
static dav_error *
seek_stream(dav_stream *stream, apr_off_t abs_position) {
return dav_new_error(stream->res->pool, HTTP_NOT_IMPLEMENTED, 0,
"Resource body read/write cannot use ranges "
"(at this time)");
}
#define RESOURCE_LACKS_ETAG_POTENTIAL(resource) (!resource->exists || (resource->type != DAV_RESOURCE_TYPE_REGULAR && resource->type != DAV_RESOURCE_TYPE_VERSION) || (resource->type == DAV_RESOURCE_TYPE_VERSION && resource->baselined))
static apr_time_t
get_last_modified(const dav_resource *resource) {
apr_time_t last_modified;
svn_error_t *serr;
svn_revnum_t created_rev;
svn_string_t *date_time;
if (RESOURCE_LACKS_ETAG_POTENTIAL(resource))
return -1;
if ((serr = svn_fs_node_created_rev(&created_rev, resource->info->root.root,
resource->info->repos_path,
resource->pool))) {
svn_error_clear(serr);
return -1;
}
if ((serr = svn_fs_revision_prop(&date_time, resource->info->repos->fs,
created_rev, "svn:date", resource->pool))) {
svn_error_clear(serr);
return -1;
}
if (date_time == NULL || date_time->data == NULL)
return -1;
if ((serr = svn_time_from_cstring(&last_modified, date_time->data,
resource->pool))) {
svn_error_clear(serr);
return -1;
}
return last_modified;
}
const char *
dav_svn__getetag(const dav_resource *resource, apr_pool_t *pool) {
svn_error_t *serr;
svn_revnum_t created_rev;
if (RESOURCE_LACKS_ETAG_POTENTIAL(resource))
return "";
if ((serr = svn_fs_node_created_rev(&created_rev, resource->info->root.root,
resource->info->repos_path,
pool))) {
svn_error_clear(serr);
return "";
}
return apr_psprintf(pool, "%s\"%ld/%s\"",
resource->collection ? "W/" : "",
created_rev,
apr_xml_quote_string(pool,
resource->info->repos_path, 1));
}
static const char *
getetag_pathetic(const dav_resource *resource) {
return dav_svn__getetag(resource, resource->pool);
}
static dav_error *
set_headers(request_rec *r, const dav_resource *resource) {
svn_error_t *serr;
svn_filesize_t length;
const char *mimetype = NULL;
apr_time_t last_modified;
if (!resource->exists)
return NULL;
last_modified = get_last_modified(resource);
if (last_modified != -1) {
ap_update_mtime(r, last_modified);
ap_set_last_modified(r);
}
apr_table_setn(r->headers_out, "ETag",
dav_svn__getetag(resource, resource->pool));
#if 0
if (resource->type == DAV_RESOURCE_TYPE_VERSION)
apr_table_setn(r->headers_out, "Cache-Control", "max-age=604800");
#endif
apr_table_setn(r->headers_out, "Accept-Ranges", "bytes");
if (resource->collection) {
if (resource->info->repos->xslt_uri)
mimetype = "text/xml";
else
mimetype = "text/html; charset=UTF-8";
} else if (resource->info->delta_base != NULL) {
dav_svn__uri_info info;
serr = dav_svn__simple_parse_uri(&info, resource,
resource->info->delta_base,
resource->pool);
if ((serr == NULL) && (info.rev != SVN_INVALID_REVNUM)) {
mimetype = SVN_SVNDIFF_MIME_TYPE;
}
svn_error_clear(serr);
}
if ((mimetype == NULL)
&& ((resource->type == DAV_RESOURCE_TYPE_VERSION)
|| (resource->type == DAV_RESOURCE_TYPE_REGULAR))
&& (resource->info->repos_path != NULL)) {
svn_string_t *value;
serr = svn_fs_node_prop(&value,
resource->info->root.root,
resource->info->repos_path,
SVN_PROP_MIME_TYPE,
resource->pool);
if (serr != NULL)
return dav_svn__convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
"could not fetch the resource's MIME type",
resource->pool);
if (value)
mimetype = value->data;
else if ((! resource->info->repos->is_svn_client)
&& r->content_type)
mimetype = r->content_type;
else
mimetype = ap_default_type(r);
serr = svn_mime_type_validate(mimetype, resource->pool);
if (serr) {
svn_error_clear(serr);
mimetype = "application/octet-stream";
}
serr = svn_fs_file_length(&length,
resource->info->root.root,
resource->info->repos_path,
resource->pool);
if (serr != NULL) {
return dav_svn__convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
"could not fetch the resource length",
resource->pool);
}
ap_set_content_length(r, (apr_off_t) length);
}
ap_set_content_type(r, mimetype);
return NULL;
}
typedef struct {
ap_filter_t *output;
apr_pool_t *pool;
} diff_ctx_t;
static svn_error_t *
write_to_filter(void *baton, const char *buffer, apr_size_t *len) {
diff_ctx_t *dc = baton;
apr_bucket_brigade *bb;
apr_bucket *bkt;
apr_status_t status;
bb = apr_brigade_create(dc->pool, dc->output->c->bucket_alloc);
bkt = apr_bucket_transient_create(buffer, *len, dc->output->c->bucket_alloc);
APR_BRIGADE_INSERT_TAIL(bb, bkt);
if ((status = ap_pass_brigade(dc->output, bb)) != APR_SUCCESS) {
return svn_error_create(status, NULL,
"Could not write data to filter");
}
return SVN_NO_ERROR;
}
static svn_error_t *
close_filter(void *baton) {
diff_ctx_t *dc = baton;
apr_bucket_brigade *bb;
apr_bucket *bkt;
apr_status_t status;
bb = apr_brigade_create(dc->pool, dc->output->c->bucket_alloc);
bkt = apr_bucket_eos_create(dc->output->c->bucket_alloc);
APR_BRIGADE_INSERT_TAIL(bb, bkt);
if ((status = ap_pass_brigade(dc->output, bb)) != APR_SUCCESS)
return svn_error_create(status, NULL, "Could not write EOS to filter");
return SVN_NO_ERROR;
}
static dav_error *
deliver(const dav_resource *resource, ap_filter_t *output) {
svn_error_t *serr;
apr_bucket_brigade *bb;
apr_bucket *bkt;
apr_status_t status;
if (resource->type != DAV_RESOURCE_TYPE_REGULAR
&& resource->type != DAV_RESOURCE_TYPE_VERSION
&& resource->type != DAV_RESOURCE_TYPE_WORKING
&& resource->info->restype != DAV_SVN_RESTYPE_PARENTPATH_COLLECTION) {
return dav_new_error(resource->pool, HTTP_CONFLICT, 0,
"Cannot GET this type of resource.");
}
if (resource->collection) {
const int gen_html = !resource->info->repos->xslt_uri;
apr_hash_t *entries;
apr_pool_t *entry_pool;
apr_array_header_t *sorted;
int i;
static const char xml_index_dtd[] =
"<!DOCTYPE svn [\n"
" <!ELEMENT svn (index)>\n"
" <!ATTLIST svn version CDATA #REQUIRED\n"
" href CDATA #REQUIRED>\n"
" <!ELEMENT index (updir?, (file | dir)*)>\n"
" <!ATTLIST index name CDATA #IMPLIED\n"
" path CDATA #IMPLIED\n"
" rev CDATA #IMPLIED\n"
" base CDATA #IMPLIED>\n"
" <!ELEMENT updir EMPTY>\n"
" <!ELEMENT file EMPTY>\n"
" <!ATTLIST file name CDATA #REQUIRED\n"
" href CDATA #REQUIRED>\n"
" <!ELEMENT dir EMPTY>\n"
" <!ATTLIST dir name CDATA #REQUIRED\n"
" href CDATA #REQUIRED>\n"
"]>\n";
if (resource->info->restype == DAV_SVN_RESTYPE_PARENTPATH_COLLECTION) {
apr_hash_index_t *hi;
apr_hash_t *dirents;
const char *fs_parent_path =
dav_svn__get_fs_parent_path(resource->info->r);
serr = svn_io_get_dirents2(&dirents, fs_parent_path, resource->pool);
if (serr != NULL)
return dav_svn__convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
"couldn't fetch dirents of SVNParentPath",
resource->pool);
entries = apr_hash_make(resource->pool);
for (hi = apr_hash_first(resource->pool, dirents);
hi; hi = apr_hash_next(hi)) {
const void *key;
void *val;
svn_io_dirent_t *dirent;
svn_fs_dirent_t *ent = apr_pcalloc(resource->pool, sizeof(*ent));
apr_hash_this(hi, &key, NULL, &val);
dirent = val;
if (dirent->kind != svn_node_dir)
continue;
ent->name = key;
ent->id = NULL;
ent->kind = dirent->kind;
apr_hash_set(entries, key, APR_HASH_KEY_STRING, ent);
}
} else {
serr = svn_fs_dir_entries(&entries, resource->info->root.root,
resource->info->repos_path, resource->pool);
if (serr != NULL)
return dav_svn__convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
"could not fetch directory entries",
resource->pool);
}
bb = apr_brigade_create(resource->pool, output->c->bucket_alloc);
if (gen_html) {
const char *title;
if (resource->info->repos_path == NULL)
title = "unknown location";
else
title = resource->info->repos_path;
if (resource->info->restype != DAV_SVN_RESTYPE_PARENTPATH_COLLECTION) {
if (SVN_IS_VALID_REVNUM(resource->info->root.rev))
title = apr_psprintf(resource->pool,
"Revision %ld: %s",
resource->info->root.rev, title);
if (resource->info->repos->repo_basename)
title = apr_psprintf(resource->pool, "%s - %s",
resource->info->repos->repo_basename,
title);
if (resource->info->repos->repo_name)
title = apr_psprintf(resource->pool, "%s: %s",
resource->info->repos->repo_name,
title);
}
ap_fprintf(output, bb, "<html><head><title>%s</title></head>\n"
"<body>\n <h2>%s</h2>\n <ul>\n", title, title);
} else {
const char *name = resource->info->repos->repo_name;
const char *href = resource->info->repos_path;
const char *base = resource->info->repos->repo_basename;
ap_fputs(output, bb, "<?xml version=\"1.0\"?>\n");
ap_fprintf(output, bb,
"<?xml-stylesheet type=\"text/xsl\" href=\"%s\"?>\n",
resource->info->repos->xslt_uri);
ap_fputs(output, bb, xml_index_dtd);
ap_fputs(output, bb,
"<svn version=\"" SVN_VERSION "\"\n"
" href=\"http://subversion.tigris.org/\">\n");
ap_fputs(output, bb, " <index");
if (name)
ap_fprintf(output, bb, " name=\"%s\"",
apr_xml_quote_string(resource->pool, name, 1));
if (SVN_IS_VALID_REVNUM(resource->info->root.rev))
ap_fprintf(output, bb, " rev=\"%ld\"",
resource->info->root.rev);
if (href)
ap_fprintf(output, bb, " path=\"%s\"",
apr_xml_quote_string(resource->pool,
href,
1));
if (base)
ap_fprintf(output, bb, " base=\"%s\"", base);
ap_fputs(output, bb, ">\n");
}
if ((resource->info->repos_path && resource->info->repos_path[1] != '\0')
&& (resource->info->restype != DAV_SVN_RESTYPE_PARENTPATH_COLLECTION)) {
if (gen_html)
ap_fprintf(output, bb, " <li><a href=\"../\">..</a></li>\n");
else
ap_fprintf(output, bb, " <updir />\n");
}
sorted = svn_sort__hash(entries, svn_sort_compare_items_as_paths,
resource->pool);
entry_pool = svn_pool_create(resource->pool);
for (i = 0; i < sorted->nelts; ++i) {
const svn_sort__item_t *item = &APR_ARRAY_IDX(sorted, i,
const svn_sort__item_t);
const svn_fs_dirent_t *entry = item->value;
const char *name = item->key;
const char *href = name;
svn_boolean_t is_dir = (entry->kind == svn_node_dir);
svn_pool_clear(entry_pool);
if (is_dir)
href = apr_pstrcat(entry_pool, href, "/", NULL);
if (gen_html)
name = href;
name = apr_xml_quote_string(entry_pool, name, !gen_html);
href = ap_os_escape_path(entry_pool, href, 0);
href = apr_xml_quote_string(entry_pool, href, 1);
if (gen_html) {
ap_fprintf(output, bb,
" <li><a href=\"%s\">%s</a></li>\n",
href, name);
} else {
const char *const tag = (is_dir ? "dir" : "file");
ap_fprintf(output, bb,
" <%s name=\"%s\" href=\"%s\" />\n",
tag, name, href);
}
}
svn_pool_destroy(entry_pool);
if (gen_html)
ap_fputs(output, bb,
" </ul>\n <hr noshade><em>Powered by "
"<a href=\"http://subversion.tigris.org/\">Subversion</a> "
"version " SVN_VERSION "."
"</em>\n</body></html>");
else
ap_fputs(output, bb, " </index>\n</svn>\n");
bkt = apr_bucket_eos_create(output->c->bucket_alloc);
APR_BRIGADE_INSERT_TAIL(bb, bkt);
if ((status = ap_pass_brigade(output, bb)) != APR_SUCCESS)
return dav_new_error(resource->pool, HTTP_INTERNAL_SERVER_ERROR, 0,
"Could not write EOS to filter.");
return NULL;
}
if (resource->info->delta_base != NULL) {
dav_svn__uri_info info;
svn_fs_root_t *root;
svn_boolean_t is_file;
svn_txdelta_stream_t *txd_stream;
svn_stream_t *o_stream;
svn_txdelta_window_handler_t handler;
void * h_baton;
diff_ctx_t dc = { 0 };
serr = dav_svn__simple_parse_uri(&info, resource,
resource->info->delta_base,
resource->pool);
if ((serr == NULL) && (info.rev != SVN_INVALID_REVNUM)) {
serr = svn_fs_revision_root(&root, resource->info->repos->fs,
info.rev, resource->pool);
if (serr != NULL)
return dav_svn__convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
"could not open a root for the base",
resource->pool);
serr = svn_fs_is_file(&is_file, root, info.repos_path,
resource->pool);
if (serr != NULL)
return dav_svn__convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
"could not determine if the base "
"is really a file",
resource->pool);
if (!is_file)
return dav_new_error(resource->pool, HTTP_BAD_REQUEST, 0,
"the delta base does not refer to a file");
serr = svn_fs_get_file_delta_stream(&txd_stream,
root, info.repos_path,
resource->info->root.root,
resource->info->repos_path,
resource->pool);
if (serr != NULL)
return dav_svn__convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
"could not prepare to read a delta",
resource->pool);
dc.output = output;
dc.pool = resource->pool;
o_stream = svn_stream_create(&dc, resource->pool);
svn_stream_set_write(o_stream, write_to_filter);
svn_stream_set_close(o_stream, close_filter);
svn_txdelta_to_svndiff2(&handler, &h_baton,
o_stream, resource->info->svndiff_version,
resource->pool);
serr = svn_txdelta_send_txstream(txd_stream, handler, h_baton,
resource->pool);
if (serr != NULL)
return dav_svn__convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
"could not deliver the txdelta stream",
resource->pool);
return NULL;
} else {
svn_error_clear(serr);
}
}
{
svn_stream_t *stream;
char *block;
serr = svn_fs_file_contents(&stream,
resource->info->root.root,
resource->info->repos_path,
resource->pool);
if (serr != NULL) {
return dav_svn__convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
"could not prepare to read the file",
resource->pool);
}
block = apr_palloc(resource->pool, SVN__STREAM_CHUNK_SIZE);
while (1) {
apr_size_t bufsize = SVN__STREAM_CHUNK_SIZE;
serr = svn_stream_read(stream, block, &bufsize);
if (serr != NULL) {
return dav_svn__convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
"could not read the file contents",
resource->pool);
}
if (bufsize == 0)
break;
bb = apr_brigade_create(resource->pool, output->c->bucket_alloc);
bkt = apr_bucket_transient_create(block, bufsize,
output->c->bucket_alloc);
APR_BRIGADE_INSERT_TAIL(bb, bkt);
if ((status = ap_pass_brigade(output, bb)) != APR_SUCCESS) {
return dav_new_error(resource->pool, HTTP_INTERNAL_SERVER_ERROR, 0,
"Could not write data to filter.");
}
}
bb = apr_brigade_create(resource->pool, output->c->bucket_alloc);
bkt = apr_bucket_eos_create(output->c->bucket_alloc);
APR_BRIGADE_INSERT_TAIL(bb, bkt);
if ((status = ap_pass_brigade(output, bb)) != APR_SUCCESS) {
return dav_new_error(resource->pool, HTTP_INTERNAL_SERVER_ERROR, 0,
"Could not write EOS to filter.");
}
return NULL;
}
}
static dav_error *
create_collection(dav_resource *resource) {
svn_error_t *serr;
dav_error *err;
if (resource->type != DAV_RESOURCE_TYPE_WORKING
&& resource->type != DAV_RESOURCE_TYPE_REGULAR) {
return dav_new_error(resource->pool, HTTP_METHOD_NOT_ALLOWED, 0,
"Collections can only be created within a working "
"or regular collection [at this time].");
}
if (resource->type == DAV_RESOURCE_TYPE_REGULAR
&& ! (resource->info->repos->autoversioning))
return dav_new_error(resource->pool, HTTP_METHOD_NOT_ALLOWED, 0,
"MKCOL called on regular resource, but "
"autoversioning is not active.");
if (resource->type == DAV_RESOURCE_TYPE_REGULAR) {
err = dav_svn__checkout(resource,
1 ,
0, 0, 0, NULL, NULL);
if (err)
return err;
}
if ((serr = svn_fs_make_dir(resource->info->root.root,
resource->info->repos_path,
resource->pool)) != NULL) {
return dav_svn__convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
"Could not create the collection.",
resource->pool);
}
if (resource->info->auto_checked_out) {
err = dav_svn__checkin(resource, 0, NULL);
if (err)
return err;
}
return NULL;
}
static dav_error *
copy_resource(const dav_resource *src,
dav_resource *dst,
int depth,
dav_response **response) {
svn_error_t *serr;
dav_error *err;
const char *src_repos_path, *dst_repos_path;
if (dst->baselined && dst->type == DAV_RESOURCE_TYPE_VERSION)
return dav_new_error(src->pool, HTTP_PRECONDITION_FAILED, 0,
"Illegal: COPY Destination is a baseline.");
if (dst->type == DAV_RESOURCE_TYPE_REGULAR
&& !(dst->info->repos->autoversioning))
return dav_new_error(dst->pool, HTTP_METHOD_NOT_ALLOWED, 0,
"COPY called on regular resource, but "
"autoversioning is not active.");
if (dst->type == DAV_RESOURCE_TYPE_REGULAR) {
err = dav_svn__checkout(dst,
1 ,
0, 0, 0, NULL, NULL);
if (err)
return err;
}
serr = svn_path_get_absolute(&src_repos_path,
svn_repos_path(src->info->repos->repos,
src->pool),
src->pool);
if (!serr)
serr = svn_path_get_absolute(&dst_repos_path,
svn_repos_path(dst->info->repos->repos,
dst->pool),
dst->pool);
if (!serr) {
if (strcmp(src_repos_path, dst_repos_path) != 0)
return dav_svn__new_error_tag
(dst->pool, HTTP_INTERNAL_SERVER_ERROR, 0,
"Copy source and destination are in different repositories.",
SVN_DAV_ERROR_NAMESPACE, SVN_DAV_ERROR_TAG);
serr = svn_fs_copy(src->info->root.root,
src->info->repos_path,
dst->info->root.root,
dst->info->repos_path,
src->pool);
}
if (serr)
return dav_svn__convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
"Unable to make a filesystem copy.",
dst->pool);
if (dst->info->auto_checked_out) {
err = dav_svn__checkin(dst, 0, NULL);
if (err)
return err;
}
return NULL;
}
static dav_error *
remove_resource(dav_resource *resource, dav_response **response) {
svn_error_t *serr;
dav_error *err;
apr_hash_t *locks;
if (resource->type != DAV_RESOURCE_TYPE_WORKING
&& resource->type != DAV_RESOURCE_TYPE_REGULAR
&& resource->type != DAV_RESOURCE_TYPE_ACTIVITY)
return dav_new_error(resource->pool, HTTP_METHOD_NOT_ALLOWED, 0,
"DELETE called on invalid resource type.");
if (resource->type == DAV_RESOURCE_TYPE_REGULAR
&& ! (resource->info->repos->autoversioning))
return dav_new_error(resource->pool, HTTP_METHOD_NOT_ALLOWED, 0,
"DELETE called on regular resource, but "
"autoversioning is not active.");
if (resource->type == DAV_RESOURCE_TYPE_ACTIVITY) {
return dav_svn__delete_activity(resource->info->repos,
resource->info->root.activity_id);
}
if (resource->type == DAV_RESOURCE_TYPE_REGULAR) {
err = dav_svn__checkout(resource,
1 ,
0, 0, 0, NULL, NULL);
if (err)
return err;
}
if (SVN_IS_VALID_REVNUM(resource->info->version_name)) {
svn_revnum_t created_rev;
serr = svn_fs_node_created_rev(&created_rev,
resource->info->root.root,
resource->info->repos_path,
resource->pool);
if (serr)
return dav_svn__convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
"Could not get created rev of resource",
resource->pool);
if (resource->info->version_name < created_rev) {
serr = svn_error_createf(SVN_ERR_RA_OUT_OF_DATE, NULL,
"Item '%s' is out of date",
resource->info->repos_path);
return dav_svn__convert_err(serr, HTTP_CONFLICT,
"Can't DELETE out-of-date resource",
resource->pool);
}
}
err = dav_svn__build_lock_hash(&locks, resource->info->r,
resource->info->repos_path, resource->pool);
if (err != NULL)
return err;
if (apr_hash_count(locks)) {
err = dav_svn__push_locks(resource, locks, resource->pool);
if (err != NULL)
return err;
}
if ((serr = svn_fs_delete(resource->info->root.root,
resource->info->repos_path,
resource->pool)) != NULL) {
return dav_svn__convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
"Could not delete the resource",
resource->pool);
}
if (resource->info->auto_checked_out) {
err = dav_svn__checkin(resource, 0, NULL);
if (err)
return err;
}
return NULL;
}
static dav_error *
move_resource(dav_resource *src,
dav_resource *dst,
dav_response **response) {
svn_error_t *serr;
dav_error *err;
if (src->type != DAV_RESOURCE_TYPE_REGULAR
|| dst->type != DAV_RESOURCE_TYPE_REGULAR
|| !(src->info->repos->autoversioning))
return dav_new_error(dst->pool, HTTP_METHOD_NOT_ALLOWED, 0,
"MOVE only allowed on two public URIs, and "
"autoversioning must be active.");
err = dav_svn__checkout(dst,
1 ,
0, 0, 0, NULL, NULL);
if (err)
return err;
serr = svn_fs_copy(src->info->root.root,
src->info->repos_path,
dst->info->root.root,
dst->info->repos_path,
src->pool);
if (serr)
return dav_svn__convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
"Unable to make a filesystem copy.",
dst->pool);
if ((serr = svn_fs_delete(dst->info->root.root,
src->info->repos_path,
dst->pool)) != NULL)
return dav_svn__convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
"Could not delete the src resource.",
dst->pool);
err = dav_svn__checkin(dst, 0, NULL);
if (err)
return err;
return NULL;
}
typedef struct {
const dav_walk_params *params;
dav_walk_resource wres;
dav_resource res;
dav_resource_private info;
svn_stringbuf_t *uri;
svn_stringbuf_t *repos_path;
} walker_ctx_t;
static dav_error *
do_walk(walker_ctx_t *ctx, int depth) {
const dav_walk_params *params = ctx->params;
int isdir = ctx->res.collection;
dav_error *err;
svn_error_t *serr;
apr_hash_index_t *hi;
apr_size_t path_len;
apr_size_t uri_len;
apr_size_t repos_len;
apr_hash_t *children;
svn_pool_clear(ctx->info.pool);
err = (*params->func)(&ctx->wres,
isdir ? DAV_CALLTYPE_COLLECTION : DAV_CALLTYPE_MEMBER);
if (err != NULL)
return err;
if (depth == 0 || !isdir)
return NULL;
if (params->root->type == DAV_RESOURCE_TYPE_WORKING)
return NULL;
if (params->root->type != DAV_RESOURCE_TYPE_REGULAR) {
return dav_new_error(params->pool, HTTP_METHOD_NOT_ALLOWED, 0,
"Walking the resource hierarchy can only be done "
"on 'regular' resources [at this time].");
}
if (ctx->info.uri_path->data[ctx->info.uri_path->len - 1] != '/')
svn_stringbuf_appendcstr(ctx->info.uri_path, "/");
if (ctx->repos_path->data[ctx->repos_path->len - 1] != '/')
svn_stringbuf_appendcstr(ctx->repos_path, "/");
ctx->info.repos_path = ctx->repos_path->data;
ctx->res.exists = TRUE;
ctx->res.collection = FALSE;
path_len = ctx->info.uri_path->len;
uri_len = ctx->uri->len;
repos_len = ctx->repos_path->len;
dav_svn__operational_log(&ctx->info,
apr_psprintf(params->pool,
"get-dir %s r%ld text",
svn_path_uri_encode(ctx->info.repos_path,
params->pool),
ctx->info.root.rev));
serr = svn_fs_dir_entries(&children, ctx->info.root.root,
ctx->info.repos_path, params->pool);
if (serr != NULL)
return dav_svn__convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
"could not fetch collection members",
params->pool);
for (hi = apr_hash_first(params->pool, children); hi; hi = apr_hash_next(hi)) {
const void *key;
apr_ssize_t klen;
void *val;
svn_fs_dirent_t *dirent;
apr_hash_this(hi, &key, &klen, &val);
dirent = val;
if (params->walk_type & DAV_WALKTYPE_AUTH) {
}
svn_stringbuf_appendbytes(ctx->info.uri_path, key, klen);
svn_stringbuf_appendbytes(ctx->uri, key, klen);
svn_stringbuf_appendbytes(ctx->repos_path, key, klen);
ctx->res.uri = ctx->uri->data;
ctx->info.repos_path = ctx->repos_path->data;
if (dirent->kind == svn_node_file) {
err = (*params->func)(&ctx->wres, DAV_CALLTYPE_MEMBER);
if (err != NULL)
return err;
} else {
ctx->res.collection = TRUE;
svn_stringbuf_appendcstr(ctx->uri, "/");
ctx->res.uri = ctx->uri->data;
err = do_walk(ctx, depth - 1);
if (err != NULL)
return err;
ctx->res.collection = FALSE;
}
ctx->info.uri_path->len = path_len;
ctx->uri->len = uri_len;
ctx->repos_path->len = repos_len;
}
return NULL;
}
static dav_error *
walk(const dav_walk_params *params, int depth, dav_response **response) {
walker_ctx_t ctx = { 0 };
dav_error *err;
ctx.params = params;
ctx.wres.walk_ctx = params->walk_ctx;
ctx.wres.pool = params->pool;
ctx.wres.resource = &ctx.res;
ctx.res = *params->root;
ctx.info = *ctx.res.info;
ctx.res.info = &ctx.info;
ctx.res.pool = params->pool;
ctx.info.uri_path = svn_stringbuf_dup(ctx.info.uri_path, params->pool);
ctx.uri = svn_stringbuf_create(params->root->uri, params->pool);
if (ctx.info.repos_path == NULL)
ctx.repos_path = NULL;
else
ctx.repos_path = svn_stringbuf_create(ctx.info.repos_path, params->pool);
if (ctx.res.collection && ctx.uri->data[ctx.uri->len - 1] != '/') {
svn_stringbuf_appendcstr(ctx.uri, "/");
}
ctx.res.uri = ctx.uri->data;
if (ctx.repos_path != NULL)
ctx.info.repos_path = ctx.repos_path->data;
ctx.info.pool = svn_pool_create(params->pool);
err = do_walk(&ctx, depth);
*response = ctx.wres.response;
return err;
}
dav_resource *
dav_svn__create_working_resource(dav_resource *base,
const char *activity_id,
const char *txn_name,
int tweak_in_place) {
const char *path;
dav_resource *res;
if (base->baselined)
path = apr_psprintf(base->pool,
"/%s/wbl/%s/%ld",
base->info->repos->special_uri,
activity_id, base->info->root.rev);
else
path = apr_psprintf(base->pool, "/%s/wrk/%s%s",
base->info->repos->special_uri,
activity_id, base->info->repos_path);
path = svn_path_uri_encode(path, base->pool);
if (tweak_in_place)
res = base;
else {
res = apr_pcalloc(base->pool, sizeof(*res));
res->info = apr_pcalloc(base->pool, sizeof(*res->info));
}
res->type = DAV_RESOURCE_TYPE_WORKING;
res->exists = TRUE;
res->versioned = TRUE;
res->working = TRUE;
res->baselined = base->baselined;
res->uri = apr_pstrcat(base->pool, base->info->repos->root_path,
path, NULL);
res->hooks = &dav_svn__hooks_repository;
res->pool = base->pool;
res->info->uri_path = svn_stringbuf_create(path, base->pool);
res->info->repos = base->info->repos;
res->info->repos_path = base->info->repos_path;
res->info->root.rev = base->info->root.rev;
res->info->root.activity_id = activity_id;
res->info->root.txn_name = txn_name;
if (tweak_in_place)
return NULL;
else
return res;
}
dav_error *
dav_svn__working_to_regular_resource(dav_resource *resource) {
dav_resource_private *priv = resource->info;
dav_svn_repos *repos = priv->repos;
const char *path;
svn_error_t *serr;
resource->type = DAV_RESOURCE_TYPE_REGULAR;
resource->working = FALSE;
if (priv->root.rev == SVN_INVALID_REVNUM) {
serr = svn_fs_youngest_rev(&priv->root.rev, repos->fs, resource->pool);
if (serr != NULL)
return dav_svn__convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
"Could not determine youngest rev.",
resource->pool);
path = apr_psprintf(resource->pool, "%s", priv->repos_path);
} else {
path = dav_svn__build_uri(repos, DAV_SVN__BUILD_URI_BC,
priv->root.rev, priv->repos_path,
0, resource->pool);
}
path = svn_path_uri_encode(path, resource->pool);
priv->uri_path = svn_stringbuf_create(path, resource->pool);
serr = svn_fs_revision_root(&priv->root.root, repos->fs,
priv->root.rev, resource->pool);
if (serr != NULL)
return dav_svn__convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
"Could not open revision root.",
resource->pool);
return NULL;
}
dav_error *
dav_svn__create_version_resource(dav_resource **version_res,
const char *uri,
apr_pool_t *pool) {
int result;
dav_error *err;
dav_resource_combined *comb = apr_pcalloc(pool, sizeof(*comb));
result = parse_version_uri(comb, uri, NULL, 0);
if (result != 0)
return dav_new_error(pool, HTTP_INTERNAL_SERVER_ERROR, 0,
"Could not parse version resource uri.");
err = prep_version(comb);
if (err)
return err;
*version_res = &comb->res;
return NULL;
}
const dav_hooks_repository dav_svn__hooks_repository = {
1,
get_resource,
get_parent_resource,
is_same_resource,
is_parent_resource,
open_stream,
close_stream,
write_stream,
seek_stream,
set_headers,
deliver,
create_collection,
copy_resource,
move_resource,
remove_resource,
walk,
getetag_pathetic
};
