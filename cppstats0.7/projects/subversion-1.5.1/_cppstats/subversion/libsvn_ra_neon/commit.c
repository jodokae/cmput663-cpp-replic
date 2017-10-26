#include <apr_pools.h>
#include <apr_hash.h>
#include <apr_uuid.h>
#define APR_WANT_STDIO
#define APR_WANT_STRFUNC
#include <apr_want.h>
#include <assert.h>
#include "svn_pools.h"
#include "svn_error.h"
#include "svn_delta.h"
#include "svn_io.h"
#include "svn_ra.h"
#include "../libsvn_ra/ra_loader.h"
#include "svn_path.h"
#include "svn_xml.h"
#include "svn_dav.h"
#include "svn_props.h"
#include "svn_private_config.h"
#include "ra_neon.h"
typedef struct {
svn_revnum_t revision;
const char *url;
const char *vsn_url;
const char *wr_url;
const char *local_path;
const char *name;
apr_pool_t *pool;
} version_rsrc_t;
typedef struct {
svn_ra_neon__session_t *ras;
const char *activity_url;
apr_hash_t *valid_targets;
svn_ra_get_wc_prop_func_t get_func;
svn_ra_push_wc_prop_func_t push_func;
void *cb_baton;
svn_boolean_t disable_merge_response;
const char *user;
svn_commit_callback2_t callback;
void *callback_baton;
apr_hash_t *tokens;
svn_boolean_t keep_locks;
} commit_ctx_t;
typedef struct {
apr_file_t *tmpfile;
svn_stringbuf_t *fname;
const char *base_checksum;
apr_off_t progress;
svn_ra_neon__session_t *ras;
apr_pool_t *pool;
} put_baton_t;
typedef struct {
commit_ctx_t *cc;
version_rsrc_t *rsrc;
apr_hash_t *prop_changes;
apr_array_header_t *prop_deletes;
svn_boolean_t created;
svn_boolean_t copied;
apr_pool_t *pool;
put_baton_t *put_baton;
const char *token;
} resource_baton_t;
static const ne_propname fetch_props[] = {
{ "DAV:", "checked-in" },
{ NULL }
};
static const ne_propname log_message_prop = { SVN_DAV_PROP_NS_SVN, "log" };
static version_rsrc_t * dup_resource(version_rsrc_t *base, apr_pool_t *pool) {
version_rsrc_t *rsrc = apr_pcalloc(pool, sizeof(*rsrc));
rsrc->pool = pool;
rsrc->revision = base->revision;
rsrc->url = base->url ?
apr_pstrdup(pool, base->url) : NULL;
rsrc->vsn_url = base->vsn_url ?
apr_pstrdup(pool, base->vsn_url) : NULL;
rsrc->wr_url = base->wr_url ?
apr_pstrdup(pool, base->wr_url) : NULL;
rsrc->local_path = base->local_path ?
apr_pstrdup(pool, base->local_path) : NULL;
return rsrc;
}
static svn_error_t * delete_activity(void *edit_baton,
apr_pool_t *pool) {
commit_ctx_t *cc = edit_baton;
return svn_ra_neon__simple_request(NULL, cc->ras, "DELETE",
cc->activity_url, NULL, NULL,
204 ,
404 , pool);
}
static svn_error_t * get_version_url(commit_ctx_t *cc,
const version_rsrc_t *parent,
version_rsrc_t *rsrc,
svn_boolean_t force,
apr_pool_t *pool) {
svn_ra_neon__resource_t *propres;
const char *url;
const svn_string_t *url_str;
if (!force) {
if (cc->get_func != NULL) {
const svn_string_t *vsn_url_value;
SVN_ERR((*cc->get_func)(cc->cb_baton,
rsrc->local_path,
SVN_RA_NEON__LP_VSN_URL,
&vsn_url_value,
pool));
if (vsn_url_value != NULL) {
rsrc->vsn_url = apr_pstrdup(rsrc->pool, vsn_url_value->data);
return SVN_NO_ERROR;
}
}
if (parent && parent->vsn_url && parent->revision == rsrc->revision) {
rsrc->vsn_url = svn_path_url_add_component(parent->vsn_url,
rsrc->name,
rsrc->pool);
return SVN_NO_ERROR;
}
}
if (rsrc->revision == SVN_INVALID_REVNUM) {
url = rsrc->url;
} else {
svn_string_t bc_url;
svn_string_t bc_relative;
SVN_ERR(svn_ra_neon__get_baseline_info(NULL,
&bc_url, &bc_relative, NULL,
cc->ras,
rsrc->url,
rsrc->revision,
pool));
url = svn_path_url_add_component(bc_url.data, bc_relative.data, pool);
}
SVN_ERR(svn_ra_neon__get_props_resource(&propres, cc->ras, url,
NULL, fetch_props, pool));
url_str = apr_hash_get(propres->propset,
SVN_RA_NEON__PROP_CHECKED_IN,
APR_HASH_KEY_STRING);
if (url_str == NULL) {
return svn_error_create(APR_EGENERAL, NULL,
_("Could not fetch the Version Resource URL "
"(needed during an import or when it is "
"missing from the local, cached props)"));
}
rsrc->vsn_url = apr_pstrdup(rsrc->pool, url_str->data);
if (cc->push_func != NULL) {
SVN_ERR((*cc->push_func)(cc->cb_baton,
rsrc->local_path,
SVN_RA_NEON__LP_VSN_URL,
url_str,
pool));
}
return SVN_NO_ERROR;
}
static svn_error_t * get_activity_collection(commit_ctx_t *cc,
const svn_string_t **collection,
svn_boolean_t force,
apr_pool_t *pool) {
if (!force && cc->get_func != NULL) {
SVN_ERR((*cc->get_func)(cc->cb_baton,
"",
SVN_RA_NEON__LP_ACTIVITY_COLL,
collection,
pool));
if (*collection != NULL) {
return SVN_NO_ERROR;
}
}
SVN_ERR(svn_ra_neon__get_activity_collection(collection,
cc->ras,
cc->ras->root.path,
pool));
if (cc->push_func != NULL) {
SVN_ERR((*cc->push_func)(cc->cb_baton,
"",
SVN_RA_NEON__LP_ACTIVITY_COLL,
*collection,
pool));
}
return SVN_NO_ERROR;
}
static svn_error_t * create_activity(commit_ctx_t *cc,
apr_pool_t *pool) {
const svn_string_t * activity_collection;
const char *uuid_buf = svn_uuid_generate(pool);
int code;
const char *url;
SVN_ERR(get_activity_collection(cc, &activity_collection, FALSE, pool));
url = svn_path_url_add_component(activity_collection->data,
uuid_buf, pool);
SVN_ERR(svn_ra_neon__simple_request(&code, cc->ras,
"MKACTIVITY", url, NULL, NULL,
201 ,
404 , pool));
if (code == 404) {
SVN_ERR(get_activity_collection(cc, &activity_collection, TRUE, pool));
url = svn_path_url_add_component(activity_collection->data,
uuid_buf, pool);
SVN_ERR(svn_ra_neon__simple_request(&code, cc->ras,
"MKACTIVITY", url, NULL, NULL,
201, 0, pool));
}
cc->activity_url = url;
return SVN_NO_ERROR;
}
static svn_error_t * add_child(version_rsrc_t **child,
commit_ctx_t *cc,
const version_rsrc_t *parent,
const char *name,
int created,
svn_revnum_t revision,
apr_pool_t *pool) {
version_rsrc_t *rsrc;
rsrc = apr_pcalloc(pool, sizeof(*rsrc));
rsrc->pool = pool;
rsrc->revision = revision;
rsrc->name = name;
rsrc->url = svn_path_url_add_component(parent->url, name, pool);
rsrc->local_path = svn_path_join(parent->local_path, name, pool);
if (created || (parent->vsn_url == NULL))
rsrc->wr_url = svn_path_url_add_component(parent->wr_url, name, pool);
else
SVN_ERR(get_version_url(cc, parent, rsrc, FALSE, pool));
*child = rsrc;
return SVN_NO_ERROR;
}
static svn_error_t * do_checkout(commit_ctx_t *cc,
const char *vsn_url,
svn_boolean_t allow_404,
const char *token,
int *code,
const char **locn,
apr_pool_t *pool) {
svn_ra_neon__request_t *request;
const char *body;
apr_hash_t *extra_headers = NULL;
svn_error_t *err = SVN_NO_ERROR;
request =
svn_ra_neon__request_create(cc->ras, "CHECKOUT", vsn_url, pool);
body = apr_psprintf(request->pool,
"<?xml version=\"1.0\" encoding=\"utf-8\"?>"
"<D:checkout xmlns:D=\"DAV:\">"
"<D:activity-set>"
"<D:href>%s</D:href>"
"</D:activity-set></D:checkout>", cc->activity_url);
if (token) {
extra_headers = apr_hash_make(request->pool);
svn_ra_neon__set_header(extra_headers, "If",
apr_psprintf(request->pool, "(<%s>)", token));
}
err = svn_ra_neon__request_dispatch(code, request, extra_headers, body,
201 ,
allow_404 ? 404 : 0,
pool);
if (err)
goto cleanup;
if (allow_404 && *code == 404 && request->err) {
svn_error_clear(request->err);
request->err = SVN_NO_ERROR;
}
*locn = svn_ra_neon__request_get_location(request, pool);
cleanup:
svn_ra_neon__request_destroy(request);
return err;
}
static svn_error_t * checkout_resource(commit_ctx_t *cc,
version_rsrc_t *rsrc,
svn_boolean_t allow_404,
const char *token,
apr_pool_t *pool) {
int code;
const char *locn = NULL;
ne_uri parse;
svn_error_t *err;
if (rsrc->wr_url != NULL) {
return NULL;
}
err = do_checkout(cc, rsrc->vsn_url, allow_404, token, &code, &locn, pool);
if (err == NULL && allow_404 && code == 404) {
locn = NULL;
SVN_ERR(get_version_url(cc, NULL, rsrc, TRUE, pool));
err = do_checkout(cc, rsrc->vsn_url, FALSE, token, &code, &locn, pool);
}
if (err) {
if (err->apr_err == SVN_ERR_FS_CONFLICT)
return svn_error_createf
(err->apr_err, err,
_("File or directory '%s' is out of date; try updating"),
svn_path_local_style(rsrc->local_path, pool));
return err;
}
if (locn == NULL)
return svn_error_create(SVN_ERR_RA_DAV_REQUEST_FAILED, NULL,
_("The CHECKOUT response did not contain a "
"'Location:' header"));
if (ne_uri_parse(locn, &parse) != 0) {
ne_uri_free(&parse);
return svn_error_createf(SVN_ERR_RA_DAV_MALFORMED_DATA, NULL,
_("Unable to parse URL '%s'"), locn);
}
rsrc->wr_url = apr_pstrdup(rsrc->pool, parse.path);
ne_uri_free(&parse);
return SVN_NO_ERROR;
}
static void record_prop_change(apr_pool_t *pool,
resource_baton_t *r,
const char *name,
const svn_string_t *value) {
name = apr_pstrdup(pool, name);
if (value) {
if (r->prop_changes == NULL)
r->prop_changes = apr_hash_make(pool);
apr_hash_set(r->prop_changes, name, APR_HASH_KEY_STRING,
svn_string_dup(value, pool));
} else {
if (r->prop_deletes == NULL)
r->prop_deletes = apr_array_make(pool, 5, sizeof(char *));
APR_ARRAY_PUSH(r->prop_deletes, const char *) = name;
}
}
static svn_error_t * do_proppatch(svn_ra_neon__session_t *ras,
const version_rsrc_t *rsrc,
resource_baton_t *rb,
apr_pool_t *pool) {
const char *url = rsrc->wr_url;
apr_hash_t *extra_headers = NULL;
if (rb->token) {
const char *token_header_val;
token_header_val = apr_psprintf(pool, "(<%s>)", rb->token);
extra_headers = apr_hash_make(pool);
apr_hash_set(extra_headers, "If", APR_HASH_KEY_STRING,
token_header_val);
}
return svn_ra_neon__do_proppatch(ras, url, rb->prop_changes,
rb->prop_deletes, extra_headers, pool);
}
static void
add_valid_target(commit_ctx_t *cc,
const char *path,
enum svn_recurse_kind kind) {
apr_hash_t *hash = cc->valid_targets;
svn_string_t *path_str = svn_string_create(path, apr_hash_pool_get(hash));
apr_hash_set(hash, path_str->data, path_str->len, (void*)kind);
}
static svn_error_t * commit_open_root(void *edit_baton,
svn_revnum_t base_revision,
apr_pool_t *dir_pool,
void **root_baton) {
commit_ctx_t *cc = edit_baton;
resource_baton_t *root;
version_rsrc_t *rsrc;
rsrc = apr_pcalloc(dir_pool, sizeof(*rsrc));
rsrc->pool = dir_pool;
rsrc->revision = SVN_INVALID_REVNUM;
rsrc->url = cc->ras->root.path;
rsrc->local_path = "";
SVN_ERR(get_version_url(cc, NULL, rsrc, FALSE, dir_pool));
root = apr_pcalloc(dir_pool, sizeof(*root));
root->pool = dir_pool;
root->cc = cc;
root->rsrc = rsrc;
root->created = FALSE;
*root_baton = root;
return SVN_NO_ERROR;
}
static apr_hash_t *get_child_tokens(apr_hash_t *lock_tokens,
const char *dir,
apr_pool_t *pool) {
apr_hash_index_t *hi;
apr_hash_t *tokens = apr_hash_make(pool);
apr_pool_t *subpool = svn_pool_create(pool);
for (hi = apr_hash_first(pool, lock_tokens); hi; hi = apr_hash_next(hi)) {
const void *key;
apr_ssize_t klen;
void *val;
svn_pool_clear(subpool);
apr_hash_this(hi, &key, &klen, &val);
if (svn_path_is_child(dir, key, subpool))
apr_hash_set(tokens, key, klen, val);
}
svn_pool_destroy(subpool);
return tokens;
}
static svn_error_t * commit_delete_entry(const char *path,
svn_revnum_t revision,
void *parent_baton,
apr_pool_t *pool) {
resource_baton_t *parent = parent_baton;
const char *name = svn_path_basename(path, pool);
apr_hash_t *extra_headers = NULL;
const char *child;
int code;
svn_error_t *serr;
if (SVN_IS_VALID_REVNUM(revision)) {
const char *revstr = apr_psprintf(pool, "%ld", revision);
if (! extra_headers)
extra_headers = apr_hash_make(pool);
apr_hash_set(extra_headers, SVN_DAV_VERSION_NAME_HEADER,
APR_HASH_KEY_STRING, revstr);
}
SVN_ERR(checkout_resource(parent->cc, parent->rsrc, TRUE, NULL, pool));
child = svn_path_url_add_component(parent->rsrc->wr_url, name, pool);
if (parent->cc->tokens) {
const char *token =
apr_hash_get(parent->cc->tokens, path, APR_HASH_KEY_STRING);
if (token) {
const char *token_header_val;
const char *token_uri;
token_uri = svn_path_url_add_component(parent->cc->ras->url->data,
path, pool);
token_header_val = apr_psprintf(pool, "<%s> (<%s>)",
token_uri, token);
extra_headers = apr_hash_make(pool);
apr_hash_set(extra_headers, "If", APR_HASH_KEY_STRING,
token_header_val);
}
}
if (parent->cc->keep_locks) {
if (! extra_headers)
extra_headers = apr_hash_make(pool);
apr_hash_set(extra_headers, SVN_DAV_OPTIONS_HEADER,
APR_HASH_KEY_STRING, SVN_DAV_OPTION_KEEP_LOCKS);
}
serr = svn_ra_neon__simple_request(&code, parent->cc->ras,
"DELETE", child,
extra_headers, NULL,
204 ,
404 , pool);
if (serr && ((serr->apr_err == SVN_ERR_FS_BAD_LOCK_TOKEN)
|| (serr->apr_err == SVN_ERR_FS_NO_LOCK_TOKEN)
|| (serr->apr_err == SVN_ERR_FS_LOCK_OWNER_MISMATCH)
|| (serr->apr_err == SVN_ERR_FS_PATH_ALREADY_LOCKED))) {
apr_hash_t *child_tokens = NULL;
svn_ra_neon__request_t *request;
const char *body;
const char *token;
svn_stringbuf_t *locks_list;
svn_error_t *err = SVN_NO_ERROR;
if (parent->cc->tokens)
child_tokens = get_child_tokens(parent->cc->tokens, path, pool);
if ((! child_tokens)
|| (apr_hash_count(child_tokens) == 0))
return serr;
else
svn_error_clear(serr);
if ((token = apr_hash_get(parent->cc->tokens, path,
APR_HASH_KEY_STRING)))
apr_hash_set(child_tokens, path, APR_HASH_KEY_STRING, token);
request =
svn_ra_neon__request_create(parent->cc->ras, "DELETE", child, pool);
err = svn_ra_neon__assemble_locktoken_body(&locks_list,
child_tokens, request->pool);
if (err)
goto cleanup;
body = apr_psprintf(request->pool,
"<?xml version=\"1.0\" encoding=\"utf-8\"?> %s",
locks_list->data);
err = svn_ra_neon__request_dispatch(&code, request, NULL, body,
204 ,
404 ,
pool);
cleanup:
svn_ra_neon__request_destroy(request);
SVN_ERR(err);
} else if (serr)
return serr;
add_valid_target(parent->cc, path, svn_nonrecursive);
return SVN_NO_ERROR;
}
static svn_error_t * commit_add_dir(const char *path,
void *parent_baton,
const char *copyfrom_path,
svn_revnum_t copyfrom_revision,
apr_pool_t *dir_pool,
void **child_baton) {
resource_baton_t *parent = parent_baton;
resource_baton_t *child;
int code;
const char *name = svn_path_basename(path, dir_pool);
apr_pool_t *workpool = svn_pool_create(dir_pool);
version_rsrc_t *rsrc = NULL;
SVN_ERR(checkout_resource(parent->cc, parent->rsrc, TRUE, NULL, dir_pool));
child = apr_pcalloc(dir_pool, sizeof(*child));
child->pool = dir_pool;
child->cc = parent->cc;
child->created = TRUE;
SVN_ERR(add_child(&rsrc, parent->cc, parent->rsrc,
name, 1, SVN_INVALID_REVNUM, workpool));
child->rsrc = dup_resource(rsrc, dir_pool);
if (! copyfrom_path) {
SVN_ERR(svn_ra_neon__simple_request(&code, parent->cc->ras, "MKCOL",
child->rsrc->wr_url, NULL, NULL,
201 , 0, workpool));
} else {
svn_string_t bc_url, bc_relative;
const char *copy_src;
SVN_ERR(svn_ra_neon__get_baseline_info(NULL,
&bc_url, &bc_relative, NULL,
parent->cc->ras,
copyfrom_path,
copyfrom_revision,
workpool));
copy_src = svn_path_url_add_component(bc_url.data,
bc_relative.data,
workpool);
SVN_ERR(svn_ra_neon__copy(parent->cc->ras,
1,
SVN_RA_NEON__DEPTH_INFINITE,
copy_src,
child->rsrc->wr_url,
workpool));
child->copied = TRUE;
}
add_valid_target(parent->cc, path,
copyfrom_path ? svn_recursive : svn_nonrecursive);
svn_pool_destroy(workpool);
*child_baton = child;
return SVN_NO_ERROR;
}
static svn_error_t * commit_open_dir(const char *path,
void *parent_baton,
svn_revnum_t base_revision,
apr_pool_t *dir_pool,
void **child_baton) {
resource_baton_t *parent = parent_baton;
resource_baton_t *child = apr_pcalloc(dir_pool, sizeof(*child));
const char *name = svn_path_basename(path, dir_pool);
apr_pool_t *workpool = svn_pool_create(dir_pool);
version_rsrc_t *rsrc = NULL;
child->pool = dir_pool;
child->cc = parent->cc;
child->created = FALSE;
SVN_ERR(add_child(&rsrc, parent->cc, parent->rsrc,
name, 0, base_revision, workpool));
child->rsrc = dup_resource(rsrc, dir_pool);
svn_pool_destroy(workpool);
*child_baton = child;
return SVN_NO_ERROR;
}
static svn_error_t * commit_change_dir_prop(void *dir_baton,
const char *name,
const svn_string_t *value,
apr_pool_t *pool) {
resource_baton_t *dir = dir_baton;
record_prop_change(dir->pool, dir, name, value);
SVN_ERR(checkout_resource(dir->cc, dir->rsrc, TRUE, NULL, pool));
add_valid_target(dir->cc, dir->rsrc->local_path, svn_nonrecursive);
return SVN_NO_ERROR;
}
static svn_error_t * commit_close_dir(void *dir_baton,
apr_pool_t *pool) {
resource_baton_t *dir = dir_baton;
SVN_ERR(do_proppatch(dir->cc->ras, dir->rsrc, dir, pool));
return SVN_NO_ERROR;
}
static svn_error_t * commit_add_file(const char *path,
void *parent_baton,
const char *copyfrom_path,
svn_revnum_t copyfrom_revision,
apr_pool_t *file_pool,
void **file_baton) {
resource_baton_t *parent = parent_baton;
resource_baton_t *file;
const char *name = svn_path_basename(path, file_pool);
apr_pool_t *workpool = svn_pool_create(file_pool);
version_rsrc_t *rsrc = NULL;
SVN_ERR(checkout_resource(parent->cc, parent->rsrc, TRUE, NULL, workpool));
file = apr_pcalloc(file_pool, sizeof(*file));
file->pool = file_pool;
file->cc = parent->cc;
file->created = TRUE;
SVN_ERR(add_child(&rsrc, parent->cc, parent->rsrc,
name, 1, SVN_INVALID_REVNUM, workpool));
file->rsrc = dup_resource(rsrc, file_pool);
if (parent->cc->tokens)
file->token = apr_hash_get(parent->cc->tokens, path, APR_HASH_KEY_STRING);
if ((! parent->created)
&& (! apr_hash_get(file->cc->valid_targets, path, APR_HASH_KEY_STRING))) {
svn_ra_neon__resource_t *res;
svn_error_t *err = svn_ra_neon__get_starting_props(&res,
file->cc->ras,
file->rsrc->wr_url,
NULL, workpool);
if (!err) {
return svn_error_createf(SVN_ERR_RA_DAV_ALREADY_EXISTS, NULL,
_("File '%s' already exists"),
file->rsrc->url);
} else if (err->apr_err == SVN_ERR_RA_DAV_PATH_NOT_FOUND) {
svn_error_clear(err);
} else {
return err;
}
}
if (! copyfrom_path) {
} else {
svn_string_t bc_url, bc_relative;
const char *copy_src;
SVN_ERR(svn_ra_neon__get_baseline_info(NULL,
&bc_url, &bc_relative, NULL,
parent->cc->ras,
copyfrom_path,
copyfrom_revision,
workpool));
copy_src = svn_path_url_add_component(bc_url.data,
bc_relative.data,
workpool);
SVN_ERR(svn_ra_neon__copy(parent->cc->ras,
1,
SVN_RA_NEON__DEPTH_ZERO,
copy_src,
file->rsrc->wr_url,
workpool));
file->copied = TRUE;
}
add_valid_target(parent->cc, path, svn_nonrecursive);
svn_pool_destroy(workpool);
*file_baton = file;
return SVN_NO_ERROR;
}
static svn_error_t * commit_open_file(const char *path,
void *parent_baton,
svn_revnum_t base_revision,
apr_pool_t *file_pool,
void **file_baton) {
resource_baton_t *parent = parent_baton;
resource_baton_t *file;
const char *name = svn_path_basename(path, file_pool);
apr_pool_t *workpool = svn_pool_create(file_pool);
version_rsrc_t *rsrc = NULL;
file = apr_pcalloc(file_pool, sizeof(*file));
file->pool = file_pool;
file->cc = parent->cc;
file->created = FALSE;
SVN_ERR(add_child(&rsrc, parent->cc, parent->rsrc,
name, 0, base_revision, workpool));
file->rsrc = dup_resource(rsrc, file_pool);
if (parent->cc->tokens)
file->token = apr_hash_get(parent->cc->tokens, path, APR_HASH_KEY_STRING);
SVN_ERR(checkout_resource(parent->cc, file->rsrc, TRUE,
file->token, workpool));
svn_pool_destroy(workpool);
*file_baton = file;
return SVN_NO_ERROR;
}
static svn_error_t * commit_stream_write(void *baton,
const char *data,
apr_size_t *len) {
put_baton_t *pb = baton;
svn_ra_neon__session_t *ras = pb->ras;
apr_status_t status;
if (ras->callbacks && ras->callbacks->cancel_func)
SVN_ERR(ras->callbacks->cancel_func(ras->callback_baton));
status = apr_file_write_full(pb->tmpfile, data, *len, NULL);
if (status)
return svn_error_wrap_apr(status,
_("Could not write svndiff to temp file"));
if (ras->progress_func) {
pb->progress += *len;
ras->progress_func(pb->progress, -1, ras->progress_baton, pb->pool);
}
return SVN_NO_ERROR;
}
static svn_error_t *
commit_apply_txdelta(void *file_baton,
const char *base_checksum,
apr_pool_t *pool,
svn_txdelta_window_handler_t *handler,
void **handler_baton) {
resource_baton_t *file = file_baton;
put_baton_t *baton;
svn_stream_t *stream;
baton = apr_pcalloc(file->pool, sizeof(*baton));
baton->ras = file->cc->ras;
baton->pool = file->pool;
file->put_baton = baton;
if (base_checksum)
baton->base_checksum = apr_pstrdup(file->pool, base_checksum);
else
baton->base_checksum = NULL;
SVN_ERR(file->cc->ras->callbacks->open_tmp_file
(&baton->tmpfile,
file->cc->ras->callback_baton,
file->pool));
stream = svn_stream_create(baton, pool);
svn_stream_set_write(stream, commit_stream_write);
svn_txdelta_to_svndiff(stream, pool, handler, handler_baton);
add_valid_target(file->cc, file->rsrc->local_path, svn_nonrecursive);
return SVN_NO_ERROR;
}
static svn_error_t * commit_change_file_prop(void *file_baton,
const char *name,
const svn_string_t *value,
apr_pool_t *pool) {
resource_baton_t *file = file_baton;
record_prop_change(file->pool, file, name, value);
SVN_ERR(checkout_resource(file->cc, file->rsrc, TRUE, file->token, pool));
add_valid_target(file->cc, file->rsrc->local_path, svn_nonrecursive);
return SVN_NO_ERROR;
}
static svn_error_t * commit_close_file(void *file_baton,
const char *text_checksum,
apr_pool_t *pool) {
resource_baton_t *file = file_baton;
commit_ctx_t *cc = file->cc;
if ((! file->put_baton) && file->created && (! file->copied)) {
file->put_baton = apr_pcalloc(file->pool, sizeof(*(file->put_baton)));
}
if (file->put_baton) {
put_baton_t *pb = file->put_baton;
const char *url = file->rsrc->wr_url;
apr_hash_t *extra_headers;
svn_ra_neon__request_t *request;
svn_error_t *err = SVN_NO_ERROR;
request = svn_ra_neon__request_create(cc->ras, "PUT", url, pool);
extra_headers = apr_hash_make(request->pool);
if (file->token)
svn_ra_neon__set_header
(extra_headers, "If",
apr_psprintf(pool, "<%s> (<%s>)",
svn_path_url_add_component(cc->ras->url->data,
file->rsrc->url,
request->pool),
file->token));
if (pb->base_checksum)
svn_ra_neon__set_header(extra_headers,
SVN_DAV_BASE_FULLTEXT_MD5_HEADER,
pb->base_checksum);
if (text_checksum)
svn_ra_neon__set_header(extra_headers,
SVN_DAV_RESULT_FULLTEXT_MD5_HEADER,
text_checksum);
if (pb->tmpfile) {
svn_ra_neon__set_header(extra_headers, "Content-Type",
SVN_SVNDIFF_MIME_TYPE);
err = svn_ra_neon__set_neon_body_provider(request, pb->tmpfile);
if (err)
goto cleanup;
} else {
ne_set_request_body_buffer(request->ne_req, "", 0);
}
err = svn_ra_neon__request_dispatch(NULL, request, extra_headers, NULL,
201 ,
204 ,
pool);
cleanup:
svn_ra_neon__request_destroy(request);
SVN_ERR(err);
if (pb->tmpfile) {
(void) apr_file_close(pb->tmpfile);
}
}
SVN_ERR(do_proppatch(cc->ras, file->rsrc, file, pool));
return SVN_NO_ERROR;
}
static svn_error_t * commit_close_edit(void *edit_baton,
apr_pool_t *pool) {
commit_ctx_t *cc = edit_baton;
svn_commit_info_t *commit_info = svn_create_commit_info(pool);
SVN_ERR(svn_ra_neon__merge_activity(&(commit_info->revision),
&(commit_info->date),
&(commit_info->author),
&(commit_info->post_commit_err),
cc->ras,
cc->ras->root.path,
cc->activity_url,
cc->valid_targets,
cc->tokens,
cc->keep_locks,
cc->disable_merge_response,
pool));
SVN_ERR(delete_activity(edit_baton, pool));
SVN_ERR(svn_ra_neon__maybe_store_auth_info(cc->ras, pool));
if (commit_info->revision != SVN_INVALID_REVNUM)
SVN_ERR(cc->callback(commit_info, cc->callback_baton, pool));
return SVN_NO_ERROR;
}
static svn_error_t * commit_abort_edit(void *edit_baton,
apr_pool_t *pool) {
return delete_activity(edit_baton, pool);
}
static svn_error_t * apply_revprops(commit_ctx_t *cc,
apr_hash_t *revprop_table,
apr_pool_t *pool) {
const char *vcc;
const svn_string_t *baseline_url;
version_rsrc_t baseline_rsrc = { SVN_INVALID_REVNUM };
svn_error_t *err = NULL;
int retry_count = 5;
SVN_ERR(svn_ra_neon__get_vcc(&vcc, cc->ras, cc->ras->root.path, pool));
do {
svn_error_clear(err);
SVN_ERR(svn_ra_neon__get_one_prop(&baseline_url, cc->ras,
vcc, NULL,
&svn_ra_neon__checked_in_prop, pool));
baseline_rsrc.pool = pool;
baseline_rsrc.vsn_url = baseline_url->data;
err = checkout_resource(cc, &baseline_rsrc, FALSE, NULL, pool);
if (err && err->apr_err != SVN_ERR_APMOD_BAD_BASELINE)
return err;
} while (err && (--retry_count > 0));
if (err)
return err;
return svn_ra_neon__do_proppatch(cc->ras, baseline_rsrc.wr_url, revprop_table,
NULL, NULL, pool);
}
svn_error_t * svn_ra_neon__get_commit_editor(svn_ra_session_t *session,
const svn_delta_editor_t **editor,
void **edit_baton,
apr_hash_t *revprop_table,
svn_commit_callback2_t callback,
void *callback_baton,
apr_hash_t *lock_tokens,
svn_boolean_t keep_locks,
apr_pool_t *pool) {
svn_ra_neon__session_t *ras = session->priv;
svn_delta_editor_t *commit_editor;
commit_ctx_t *cc;
svn_error_t *err;
cc = apr_pcalloc(pool, sizeof(*cc));
cc->ras = ras;
cc->valid_targets = apr_hash_make(pool);
cc->get_func = ras->callbacks->get_wc_prop;
cc->push_func = ras->callbacks->push_wc_prop;
cc->cb_baton = ras->callback_baton;
cc->callback = callback;
cc->callback_baton = callback_baton;
cc->tokens = lock_tokens;
cc->keep_locks = keep_locks;
if (ras->callbacks->push_wc_prop == NULL)
cc->disable_merge_response = TRUE;
SVN_ERR(create_activity(cc, pool));
err = apply_revprops(cc, revprop_table, pool);
if (err) {
svn_error_clear(commit_abort_edit(cc, pool));
return err;
}
commit_editor = svn_delta_default_editor(pool);
commit_editor->open_root = commit_open_root;
commit_editor->delete_entry = commit_delete_entry;
commit_editor->add_directory = commit_add_dir;
commit_editor->open_directory = commit_open_dir;
commit_editor->change_dir_prop = commit_change_dir_prop;
commit_editor->close_directory = commit_close_dir;
commit_editor->add_file = commit_add_file;
commit_editor->open_file = commit_open_file;
commit_editor->apply_textdelta = commit_apply_txdelta;
commit_editor->change_file_prop = commit_change_file_prop;
commit_editor->close_file = commit_close_file;
commit_editor->close_edit = commit_close_edit;
commit_editor->abort_edit = commit_abort_edit;
*editor = commit_editor;
*edit_baton = cc;
return SVN_NO_ERROR;
}
