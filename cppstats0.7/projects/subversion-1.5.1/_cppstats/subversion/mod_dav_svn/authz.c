#include <http_request.h>
#include <http_log.h>
#include "svn_pools.h"
#include "svn_path.h"
#include "mod_authz_svn.h"
#include "dav_svn.h"
static svn_boolean_t
allow_read(request_rec *r,
const dav_svn_repos *repos,
const char *path,
svn_revnum_t rev,
apr_pool_t *pool) {
const char *uri;
request_rec *subreq;
enum dav_svn__build_what uri_type;
svn_boolean_t allowed = FALSE;
authz_svn__subreq_bypass_func_t allow_read_bypass = NULL;
if (! dav_svn__get_pathauthz_flag(r)) {
return TRUE;
}
allow_read_bypass = dav_svn__get_pathauthz_bypass(r);
if (allow_read_bypass != NULL) {
if (allow_read_bypass(r,path, repos->repo_name) == OK)
return TRUE;
else
return FALSE;
}
if (SVN_IS_VALID_REVNUM(rev))
uri_type = DAV_SVN__BUILD_URI_VERSION;
else
uri_type = DAV_SVN__BUILD_URI_PUBLIC;
uri = dav_svn__build_uri(repos, uri_type, rev, path, FALSE, pool);
subreq = ap_sub_req_method_uri("GET", uri, r, r->output_filters);
if (subreq) {
if (subreq->status == HTTP_OK)
allowed = TRUE;
ap_destroy_sub_req(subreq);
}
return allowed;
}
static svn_error_t *
authz_read(svn_boolean_t *allowed,
svn_fs_root_t *root,
const char *path,
void *baton,
apr_pool_t *pool) {
dav_svn__authz_read_baton *arb = baton;
svn_revnum_t rev = SVN_INVALID_REVNUM;
const char *revpath = NULL;
if (svn_fs_is_txn_root(root)) {
svn_stringbuf_t *path_s = svn_stringbuf_create(path, pool);
const char *lopped_path = "";
while (! (svn_path_is_empty(path_s->data)
|| ((path_s->len == 1) && (path_s->data[0] == '/')))) {
SVN_ERR(svn_fs_copied_from(&rev, &revpath, root,
path_s->data, pool));
if (SVN_IS_VALID_REVNUM(rev) && revpath) {
revpath = svn_path_join(revpath, lopped_path, pool);
break;
}
lopped_path = svn_path_join(svn_path_basename
(path_s->data, pool), lopped_path, pool);
svn_path_remove_component(path_s);
}
if ((rev == SVN_INVALID_REVNUM) && (revpath == NULL)) {
rev = svn_fs_txn_root_base_revision(root);
revpath = path;
}
} else {
rev = svn_fs_revision_root_revision(root);
revpath = path;
}
*allowed = allow_read(arb->r, arb->repos, revpath, rev, pool);
return SVN_NO_ERROR;
}
svn_repos_authz_func_t
dav_svn__authz_read_func(dav_svn__authz_read_baton *baton) {
if (! dav_svn__get_pathauthz_flag(baton->r))
return NULL;
return authz_read;
}
svn_boolean_t
dav_svn__allow_read(const dav_resource *resource,
svn_revnum_t rev,
apr_pool_t *pool) {
return allow_read(resource->info->r, resource->info->repos,
resource->info->repos_path, rev, pool);
}