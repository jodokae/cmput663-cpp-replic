#include <apr_uuid.h>
#include <apr_time.h>
#include <httpd.h>
#include <http_log.h>
#include <mod_dav.h>
#include "svn_fs.h"
#include "svn_repos.h"
#include "svn_dav.h"
#include "svn_time.h"
#include "svn_pools.h"
#include "dav_svn.h"
struct dav_lockdb_private {
svn_boolean_t lock_steal;
svn_boolean_t lock_break;
svn_boolean_t keep_locks;
svn_revnum_t working_revnum;
request_rec *r;
};
static void
svn_lock_to_dav_lock(dav_lock **dlock,
const svn_lock_t *slock,
svn_boolean_t hide_auth_user,
svn_boolean_t exists_p,
apr_pool_t *pool) {
dav_lock *lock = apr_pcalloc(pool, sizeof(*lock));
dav_locktoken *token = apr_pcalloc(pool, sizeof(*token));
lock->rectype = DAV_LOCKREC_DIRECT;
lock->scope = DAV_LOCKSCOPE_EXCLUSIVE;
lock->type = DAV_LOCKTYPE_WRITE;
lock->depth = 0;
lock->is_locknull = exists_p;
token->uuid_str = apr_pstrdup(pool, slock->token);
lock->locktoken = token;
if (slock->comment) {
if (! slock->is_dav_comment) {
lock->owner = apr_pstrcat(pool,
"<D:owner xmlns:D=\"DAV:\">",
apr_xml_quote_string(pool,
slock->comment, 1),
"</D:owner>", NULL);
} else {
lock->owner = apr_pstrdup(pool, slock->comment);
}
} else
lock->owner = NULL;
if (! hide_auth_user)
lock->auth_user = apr_pstrdup(pool, slock->owner);
if (slock->expiration_date)
lock->timeout = (time_t) (slock->expiration_date / APR_USEC_PER_SEC);
else
lock->timeout = DAV_TIMEOUT_INFINITE;
*dlock = lock;
}
static dav_error *
unescape_xml(const char **output,
const char *input,
apr_pool_t *pool) {
apr_xml_parser *xml_parser = apr_xml_parser_create(pool);
apr_xml_doc *xml_doc;
apr_status_t apr_err;
const char *xml_input = apr_pstrcat
(pool, "<?xml version=\"1.0\" encoding=\"utf-8\"?>", input, NULL);
apr_err = apr_xml_parser_feed(xml_parser, xml_input, strlen(xml_input));
if (!apr_err)
apr_err = apr_xml_parser_done(xml_parser, &xml_doc);
if (apr_err) {
char errbuf[1024];
(void)apr_xml_parser_geterror(xml_parser, errbuf, sizeof(errbuf));
return dav_new_error(pool, HTTP_INTERNAL_SERVER_ERROR,
DAV_ERR_LOCK_SAVE_LOCK, errbuf);
}
apr_xml_to_text(pool, xml_doc->root, APR_XML_X2T_INNER,
xml_doc->namespaces, NULL, output, NULL);
return SVN_NO_ERROR;
}
static dav_error *
dav_lock_to_svn_lock(svn_lock_t **slock,
const dav_lock *dlock,
const char *path,
dav_lockdb_private *info,
svn_boolean_t is_svn_client,
apr_pool_t *pool) {
svn_lock_t *lock;
if (dlock->type != DAV_LOCKTYPE_WRITE)
return dav_new_error(pool, HTTP_BAD_REQUEST,
DAV_ERR_LOCK_SAVE_LOCK,
"Only 'write' locks are supported.");
if (dlock->scope != DAV_LOCKSCOPE_EXCLUSIVE)
return dav_new_error(pool, HTTP_BAD_REQUEST,
DAV_ERR_LOCK_SAVE_LOCK,
"Only exclusive locks are supported.");
lock = svn_lock_create(pool);
lock->path = apr_pstrdup(pool, path);
lock->token = apr_pstrdup(pool, dlock->locktoken->uuid_str);
lock->creation_date = apr_time_now();
if (dlock->auth_user)
lock->owner = apr_pstrdup(pool, dlock->auth_user);
if (dlock->owner) {
if (is_svn_client) {
dav_error *derr;
lock->is_dav_comment = 0;
derr = unescape_xml(&(lock->comment), dlock->owner, pool);
if (derr)
return derr;
} else {
lock->comment = apr_pstrdup(pool, dlock->owner);
lock->is_dav_comment = 1;
}
}
if (dlock->timeout == DAV_TIMEOUT_INFINITE)
lock->expiration_date = 0;
else
lock->expiration_date = ((apr_time_t)dlock->timeout) * APR_USEC_PER_SEC;
*slock = lock;
return 0;
}
static const char *
get_supportedlock(const dav_resource *resource) {
static const char supported[] = DEBUG_CR
"<D:lockentry>" DEBUG_CR
"<D:lockscope><D:exclusive/></D:lockscope>" DEBUG_CR
"<D:locktype><D:write/></D:locktype>" DEBUG_CR
"</D:lockentry>" DEBUG_CR;
if (resource->collection)
return NULL;
else
return supported;
}
static dav_error *
parse_locktoken(apr_pool_t *pool,
const char *char_token,
dav_locktoken **locktoken_p) {
dav_locktoken *token = apr_pcalloc(pool, sizeof(*token));
token->uuid_str = apr_pstrdup(pool, char_token);
*locktoken_p = token;
return 0;
}
static const char *
format_locktoken(apr_pool_t *p, const dav_locktoken *locktoken) {
return apr_pstrdup(p, locktoken->uuid_str);
}
static int
compare_locktoken(const dav_locktoken *lt1, const dav_locktoken *lt2) {
return strcmp(lt1->uuid_str, lt2->uuid_str);
}
static dav_error *
open_lockdb(request_rec *r, int ro, int force, dav_lockdb **lockdb) {
const char *svn_client_options, *version_name;
dav_lockdb *db = apr_pcalloc(r->pool, sizeof(*db));
dav_lockdb_private *info = apr_pcalloc(r->pool, sizeof(*info));
info->r = r;
svn_client_options = apr_table_get(r->headers_in, SVN_DAV_OPTIONS_HEADER);
if (svn_client_options) {
if (ap_strstr_c(svn_client_options, SVN_DAV_OPTION_LOCK_BREAK))
info->lock_break = TRUE;
if (ap_strstr_c(svn_client_options, SVN_DAV_OPTION_LOCK_STEAL))
info->lock_steal = TRUE;
if (ap_strstr_c(svn_client_options, SVN_DAV_OPTION_KEEP_LOCKS))
info->keep_locks = TRUE;
}
version_name = apr_table_get(r->headers_in, SVN_DAV_VERSION_NAME_HEADER);
info->working_revnum = version_name ?
SVN_STR_TO_REV(version_name): SVN_INVALID_REVNUM;
db->hooks = &dav_svn__hooks_locks;
db->ro = ro;
db->info = info;
*lockdb = db;
return 0;
}
static void
close_lockdb(dav_lockdb *lockdb) {
return;
}
static dav_error *
remove_locknull_state(dav_lockdb *lockdb, const dav_resource *resource) {
return 0;
}
static dav_error *
create_lock(dav_lockdb *lockdb, const dav_resource *resource, dav_lock **lock) {
svn_error_t *serr;
dav_locktoken *token = apr_pcalloc(resource->pool, sizeof(*token));
dav_lock *dlock = apr_pcalloc(resource->pool, sizeof(*dlock));
dlock->rectype = DAV_LOCKREC_DIRECT;
dlock->is_locknull = resource->exists;
dlock->scope = DAV_LOCKSCOPE_UNKNOWN;
dlock->type = DAV_LOCKTYPE_UNKNOWN;
dlock->depth = 0;
serr = svn_fs_generate_lock_token(&(token->uuid_str),
resource->info->repos->fs,
resource->pool);
if (serr)
return dav_svn__convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
"Failed to generate a lock token.",
resource->pool);
dlock->locktoken = token;
*lock = dlock;
return 0;
}
static dav_error *
get_locks(dav_lockdb *lockdb,
const dav_resource *resource,
int calltype,
dav_lock **locks) {
dav_lockdb_private *info = lockdb->info;
svn_error_t *serr;
svn_lock_t *slock;
dav_lock *lock = NULL;
if (! resource->info->repos_path) {
*locks = NULL;
return 0;
}
if (info->r->method_number == M_LOCK) {
*locks = NULL;
return 0;
}
if (! dav_svn__allow_read(resource, SVN_INVALID_REVNUM, resource->pool))
return dav_new_error(resource->pool, HTTP_FORBIDDEN,
DAV_ERR_LOCK_SAVE_LOCK,
"Path is not accessible.");
serr = svn_fs_get_lock(&slock,
resource->info->repos->fs,
resource->info->repos_path,
resource->pool);
if (serr)
return dav_svn__convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
"Failed to check path for a lock.",
resource->pool);
if (slock != NULL) {
svn_lock_to_dav_lock(&lock, slock, info->lock_break,
resource->exists, resource->pool);
apr_table_setn(info->r->headers_out, SVN_DAV_CREATIONDATE_HEADER,
svn_time_to_cstring(slock->creation_date,
resource->pool));
apr_table_setn(info->r->headers_out, SVN_DAV_LOCK_OWNER_HEADER,
slock->owner);
}
*locks = lock;
return 0;
}
static dav_error *
find_lock(dav_lockdb *lockdb,
const dav_resource *resource,
const dav_locktoken *locktoken,
int partial_ok,
dav_lock **lock) {
dav_lockdb_private *info = lockdb->info;
svn_error_t *serr;
svn_lock_t *slock;
dav_lock *dlock = NULL;
if (! dav_svn__allow_read(resource, SVN_INVALID_REVNUM, resource->pool))
return dav_new_error(resource->pool, HTTP_FORBIDDEN,
DAV_ERR_LOCK_SAVE_LOCK,
"Path is not accessible.");
serr = svn_fs_get_lock(&slock,
resource->info->repos->fs,
resource->info->repos_path,
resource->pool);
if (serr)
return dav_svn__convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
"Failed to look up lock by path.",
resource->pool);
if (slock != NULL) {
if (strcmp(locktoken->uuid_str, slock->token) != 0)
return dav_new_error(resource->pool, HTTP_BAD_REQUEST,
DAV_ERR_LOCK_SAVE_LOCK,
"Incoming token doesn't match existing lock.");
svn_lock_to_dav_lock(&dlock, slock, FALSE,
resource->exists, resource->pool);
apr_table_setn(info->r->headers_out, SVN_DAV_CREATIONDATE_HEADER,
svn_time_to_cstring(slock->creation_date,
resource->pool));
apr_table_setn(info->r->headers_out, SVN_DAV_LOCK_OWNER_HEADER,
slock->owner);
}
*lock = dlock;
return 0;
}
static dav_error *
has_locks(dav_lockdb *lockdb, const dav_resource *resource, int *locks_present) {
dav_lockdb_private *info = lockdb->info;
svn_error_t *serr;
svn_lock_t *slock;
if (! resource->info->repos_path) {
*locks_present = 0;
return 0;
}
if (info->r->method_number == M_LOCK) {
*locks_present = 0;
return 0;
}
if (! dav_svn__allow_read(resource, SVN_INVALID_REVNUM, resource->pool))
return dav_new_error(resource->pool, HTTP_FORBIDDEN,
DAV_ERR_LOCK_SAVE_LOCK,
"Path is not accessible.");
serr = svn_fs_get_lock(&slock,
resource->info->repos->fs,
resource->info->repos_path,
resource->pool);
if (serr)
return dav_svn__convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
"Failed to check path for a lock.",
resource->pool);
*locks_present = slock ? 1 : 0;
return 0;
}
static dav_error *
append_locks(dav_lockdb *lockdb,
const dav_resource *resource,
int make_indirect,
const dav_lock *lock) {
dav_lockdb_private *info = lockdb->info;
svn_lock_t *slock;
svn_error_t *serr;
dav_error *derr;
if (! dav_svn__allow_read(resource, SVN_INVALID_REVNUM, resource->pool))
return dav_new_error(resource->pool, HTTP_FORBIDDEN,
DAV_ERR_LOCK_SAVE_LOCK,
"Path is not accessible.");
if (lock->next)
return dav_new_error(resource->pool, HTTP_BAD_REQUEST,
DAV_ERR_LOCK_SAVE_LOCK,
"Tried to attach multiple locks to a resource.");
if (! resource->exists) {
svn_revnum_t rev, new_rev;
svn_fs_txn_t *txn;
svn_fs_root_t *txn_root;
const char *conflict_msg;
dav_svn_repos *repos = resource->info->repos;
if (resource->info->repos->is_svn_client)
return dav_new_error(resource->pool, HTTP_METHOD_NOT_ALLOWED,
DAV_ERR_LOCK_SAVE_LOCK,
"Subversion clients may not lock "
"nonexistent paths.");
else if (! resource->info->repos->autoversioning)
return dav_new_error(resource->pool, HTTP_METHOD_NOT_ALLOWED,
DAV_ERR_LOCK_SAVE_LOCK,
"Attempted to lock non-existent path;"
" turn on autoversioning first.");
if ((serr = svn_fs_youngest_rev(&rev, repos->fs, resource->pool)))
return dav_svn__convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
"Could not determine youngest revision",
resource->pool);
if ((serr = svn_repos_fs_begin_txn_for_commit(&txn, repos->repos, rev,
repos->username, NULL,
resource->pool)))
return dav_svn__convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
"Could not begin a transaction",
resource->pool);
if ((serr = svn_fs_txn_root(&txn_root, txn, resource->pool)))
return dav_svn__convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
"Could not begin a transaction",
resource->pool);
if ((serr = svn_fs_make_file(txn_root, resource->info->repos_path,
resource->pool)))
return dav_svn__convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
"Could not create empty file.",
resource->pool);
if ((serr = dav_svn__attach_auto_revprops(txn,
resource->info->repos_path,
resource->pool)))
return dav_svn__convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
"Could not create empty file.",
resource->pool);
if ((serr = svn_repos_fs_commit_txn(&conflict_msg, repos->repos,
&new_rev, txn, resource->pool))) {
svn_error_clear(svn_fs_abort_txn(txn, resource->pool));
return dav_svn__convert_err(serr, HTTP_CONFLICT,
apr_psprintf(resource->pool,
"Conflict when committing "
"'%s'.", conflict_msg),
resource->pool);
}
}
derr = dav_lock_to_svn_lock(&slock, lock, resource->info->repos_path,
info, resource->info->repos->is_svn_client,
resource->pool);
if (derr)
return derr;
serr = svn_repos_fs_lock(&slock,
resource->info->repos->repos,
slock->path,
slock->token,
slock->comment,
slock->is_dav_comment,
slock->expiration_date,
info->working_revnum,
info->lock_steal,
resource->pool);
if (serr && serr->apr_err == SVN_ERR_FS_NO_USER) {
svn_error_clear(serr);
return dav_new_error(resource->pool, HTTP_UNAUTHORIZED,
DAV_ERR_LOCK_SAVE_LOCK,
"Anonymous lock creation is not allowed.");
} else if (serr)
return dav_svn__convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
"Failed to create new lock.",
resource->pool);
apr_table_setn(info->r->headers_out, SVN_DAV_CREATIONDATE_HEADER,
svn_time_to_cstring(slock->creation_date, resource->pool));
apr_table_setn(info->r->headers_out, SVN_DAV_LOCK_OWNER_HEADER,
slock->owner);
dav_svn__operational_log(resource->info,
apr_psprintf(resource->info->r->pool,
"lock (%s)%s",
svn_path_uri_encode(slock->path,
resource->info->r->pool),
info->lock_steal ? " steal" : ""));
return 0;
}
static dav_error *
remove_lock(dav_lockdb *lockdb,
const dav_resource *resource,
const dav_locktoken *locktoken) {
dav_lockdb_private *info = lockdb->info;
svn_error_t *serr;
svn_lock_t *slock;
const char *token = NULL;
if (! resource->info->repos_path)
return 0;
if (info->keep_locks)
return 0;
if (! dav_svn__allow_read(resource, SVN_INVALID_REVNUM, resource->pool))
return dav_new_error(resource->pool, HTTP_FORBIDDEN,
DAV_ERR_LOCK_SAVE_LOCK,
"Path is not accessible.");
if (locktoken == NULL) {
serr = svn_fs_get_lock(&slock,
resource->info->repos->fs,
resource->info->repos_path,
resource->pool);
if (serr)
return dav_svn__convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
"Failed to check path for a lock.",
resource->pool);
if (slock)
token = slock->token;
} else {
token = locktoken->uuid_str;
}
if (token) {
serr = svn_repos_fs_unlock(resource->info->repos->repos,
resource->info->repos_path,
token,
info->lock_break,
resource->pool);
if (serr && serr->apr_err == SVN_ERR_FS_NO_USER) {
svn_error_clear(serr);
return dav_new_error(resource->pool, HTTP_UNAUTHORIZED,
DAV_ERR_LOCK_SAVE_LOCK,
"Anonymous lock removal is not allowed.");
} else if (serr)
return dav_svn__convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
"Failed to remove a lock.",
resource->pool);
dav_svn__operational_log(resource->info,
apr_psprintf(resource->info->r->pool,
"unlock (%s)%s",
svn_path_uri_encode(resource->info->repos_path,
resource->info->r->pool),
info->lock_break ? " break" : ""));
}
return 0;
}
static dav_error *
refresh_locks(dav_lockdb *lockdb,
const dav_resource *resource,
const dav_locktoken_list *ltl,
time_t new_time,
dav_lock **locks) {
dav_locktoken *token = ltl->locktoken;
svn_error_t *serr;
svn_lock_t *slock;
dav_lock *dlock;
if (! dav_svn__allow_read(resource, SVN_INVALID_REVNUM, resource->pool))
return dav_new_error(resource->pool, HTTP_FORBIDDEN,
DAV_ERR_LOCK_SAVE_LOCK,
"Path is not accessible.");
serr = svn_fs_get_lock(&slock,
resource->info->repos->fs,
resource->info->repos_path,
resource->pool);
if (serr)
return dav_svn__convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
"Token doesn't point to a lock.",
resource->pool);
if ((! slock)
|| (strcmp(token->uuid_str, slock->token) != 0))
return dav_new_error(resource->pool, HTTP_UNAUTHORIZED,
DAV_ERR_LOCK_SAVE_LOCK,
"Lock refresh request doesn't match existing lock.");
serr = svn_repos_fs_lock(&slock,
resource->info->repos->repos,
slock->path,
slock->token,
slock->comment,
slock->is_dav_comment,
(new_time == DAV_TIMEOUT_INFINITE)
? 0 : (apr_time_t)new_time * APR_USEC_PER_SEC,
SVN_INVALID_REVNUM,
TRUE,
resource->pool);
if (serr && serr->apr_err == SVN_ERR_FS_NO_USER) {
svn_error_clear(serr);
return dav_new_error(resource->pool, HTTP_UNAUTHORIZED,
DAV_ERR_LOCK_SAVE_LOCK,
"Anonymous lock refreshing is not allowed.");
} else if (serr)
return dav_svn__convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
"Failed to refresh existing lock.",
resource->pool);
svn_lock_to_dav_lock(&dlock, slock, FALSE, resource->exists, resource->pool);
*locks = dlock;
return 0;
}
const dav_hooks_locks dav_svn__hooks_locks = {
get_supportedlock,
parse_locktoken,
format_locktoken,
compare_locktoken,
open_lockdb,
close_lockdb,
remove_locknull_state,
create_lock,
get_locks,
find_lock,
has_locks,
append_locks,
remove_lock,
refresh_locks,
NULL,
NULL
};
