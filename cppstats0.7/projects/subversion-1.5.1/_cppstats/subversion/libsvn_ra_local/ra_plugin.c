#include "ra_local.h"
#include "svn_ra.h"
#include "svn_fs.h"
#include "svn_delta.h"
#include "svn_repos.h"
#include "svn_pools.h"
#include "svn_time.h"
#include "svn_props.h"
#include "svn_mergeinfo.h"
#include "svn_path.h"
#include "svn_private_config.h"
#include "../libsvn_ra/ra_loader.h"
#include "private/svn_mergeinfo_private.h"
#define APR_WANT_STRFUNC
#include <apr_want.h>
#include <assert.h>
static apr_status_t
cleanup_access(void *data) {
svn_error_t *serr;
svn_fs_t *fs = data;
serr = svn_fs_set_access(fs, NULL);
if (serr) {
apr_status_t apr_err = serr->apr_err;
svn_error_clear(serr);
return apr_err;
}
return APR_SUCCESS;
}
static svn_error_t *
get_username(svn_ra_session_t *session,
apr_pool_t *pool) {
svn_ra_local__session_baton_t *sess = session->priv;
svn_auth_iterstate_t *iterstate;
svn_fs_access_t *access_ctx;
if (! sess->username) {
if (sess->callbacks->auth_baton) {
void *creds;
svn_auth_cred_username_t *username_creds;
SVN_ERR(svn_auth_first_credentials(&creds, &iterstate,
SVN_AUTH_CRED_USERNAME,
sess->uuid,
sess->callbacks->auth_baton,
pool));
username_creds = creds;
if (username_creds && username_creds->username) {
sess->username = apr_pstrdup(session->pool,
username_creds->username);
svn_error_clear(svn_auth_save_credentials(iterstate, pool));
} else
sess->username = "";
} else
sess->username = "";
}
if (*sess->username) {
SVN_ERR(svn_fs_create_access(&access_ctx, sess->username,
pool));
SVN_ERR(svn_fs_set_access(sess->fs, access_ctx));
apr_pool_cleanup_register(pool, sess->fs, cleanup_access,
apr_pool_cleanup_null);
}
return SVN_NO_ERROR;
}
typedef struct reporter_baton_t {
svn_ra_local__session_baton_t *sess;
void *report_baton;
} reporter_baton_t;
static void *
make_reporter_baton(svn_ra_local__session_baton_t *sess,
void *report_baton,
apr_pool_t *pool) {
reporter_baton_t *rbaton = apr_palloc(pool, sizeof(*rbaton));
rbaton->sess = sess;
rbaton->report_baton = report_baton;
return rbaton;
}
static svn_error_t *
reporter_set_path(void *reporter_baton,
const char *path,
svn_revnum_t revision,
svn_depth_t depth,
svn_boolean_t start_empty,
const char *lock_token,
apr_pool_t *pool) {
reporter_baton_t *rbaton = reporter_baton;
return svn_repos_set_path3(rbaton->report_baton, path,
revision, depth, start_empty, lock_token, pool);
}
static svn_error_t *
reporter_delete_path(void *reporter_baton,
const char *path,
apr_pool_t *pool) {
reporter_baton_t *rbaton = reporter_baton;
return svn_repos_delete_path(rbaton->report_baton, path, pool);
}
static svn_error_t *
reporter_link_path(void *reporter_baton,
const char *path,
const char *url,
svn_revnum_t revision,
svn_depth_t depth,
svn_boolean_t start_empty,
const char *lock_token,
apr_pool_t *pool) {
reporter_baton_t *rbaton = reporter_baton;
const char *fs_path = NULL;
const char *repos_url_decoded;
int repos_url_len;
url = svn_path_uri_decode(url, pool);
repos_url_decoded = svn_path_uri_decode(rbaton->sess->repos_url, pool);
repos_url_len = strlen(repos_url_decoded);
if (strncmp(url, repos_url_decoded, repos_url_len) != 0)
return svn_error_createf(SVN_ERR_RA_ILLEGAL_URL, NULL,
_("'%s'\n"
"is not the same repository as\n"
"'%s'"), url, rbaton->sess->repos_url);
fs_path = url + repos_url_len;
return svn_repos_link_path3(rbaton->report_baton, path, fs_path, revision,
depth, start_empty, lock_token, pool);
}
static svn_error_t *
reporter_finish_report(void *reporter_baton,
apr_pool_t *pool) {
reporter_baton_t *rbaton = reporter_baton;
return svn_repos_finish_report(rbaton->report_baton, pool);
}
static svn_error_t *
reporter_abort_report(void *reporter_baton,
apr_pool_t *pool) {
reporter_baton_t *rbaton = reporter_baton;
return svn_repos_abort_report(rbaton->report_baton, pool);
}
static const svn_ra_reporter3_t ra_local_reporter = {
reporter_set_path,
reporter_delete_path,
reporter_link_path,
reporter_finish_report,
reporter_abort_report
};
static svn_error_t *
make_reporter(svn_ra_session_t *session,
const svn_ra_reporter3_t **reporter,
void **report_baton,
svn_revnum_t revision,
const char *target,
const char *other_url,
svn_boolean_t text_deltas,
svn_depth_t depth,
svn_boolean_t send_copyfrom_args,
svn_boolean_t ignore_ancestry,
const svn_delta_editor_t *editor,
void *edit_baton,
apr_pool_t *pool) {
svn_ra_local__session_baton_t *sess = session->priv;
void *rbaton;
int repos_url_len;
const char *other_fs_path = NULL;
const char *repos_url_decoded;
if (! SVN_IS_VALID_REVNUM(revision))
SVN_ERR(svn_fs_youngest_rev(&revision, sess->fs, pool));
if (other_url) {
other_url = svn_path_uri_decode(other_url, pool);
repos_url_decoded = svn_path_uri_decode(sess->repos_url, pool);
repos_url_len = strlen(repos_url_decoded);
if (strncmp(other_url, repos_url_decoded, repos_url_len) != 0)
return svn_error_createf
(SVN_ERR_RA_ILLEGAL_URL, NULL,
_("'%s'\n"
"is not the same repository as\n"
"'%s'"), other_url, sess->repos_url);
other_fs_path = other_url + repos_url_len;
}
*reporter = &ra_local_reporter;
SVN_ERR(get_username(session, pool));
if (sess->callbacks)
SVN_ERR(svn_delta_get_cancellation_editor(sess->callbacks->cancel_func,
sess->callback_baton,
editor,
edit_baton,
&editor,
&edit_baton,
pool));
SVN_ERR(svn_repos_begin_report2(&rbaton,
revision,
sess->repos,
sess->fs_path->data,
target,
other_fs_path,
text_deltas,
depth,
ignore_ancestry,
send_copyfrom_args,
editor,
edit_baton,
NULL,
NULL,
pool));
*report_baton = make_reporter_baton(sess, rbaton, pool);
return SVN_NO_ERROR;
}
struct deltify_etc_baton {
svn_fs_t *fs;
svn_repos_t *repos;
const char *fs_path;
apr_hash_t *lock_tokens;
apr_pool_t *pool;
svn_commit_callback2_t callback;
void *callback_baton;
};
static svn_error_t *
deltify_etc(const svn_commit_info_t *commit_info,
void *baton, apr_pool_t *pool) {
struct deltify_etc_baton *db = baton;
svn_error_t *err1, *err2;
apr_hash_index_t *hi;
apr_pool_t *iterpool;
err1 = (*db->callback)(commit_info, db->callback_baton, pool);
if (db->lock_tokens) {
iterpool = svn_pool_create(db->pool);
for (hi = apr_hash_first(db->pool, db->lock_tokens); hi;
hi = apr_hash_next(hi)) {
const void *rel_path;
void *val;
const char *abs_path, *token;
svn_pool_clear(iterpool);
apr_hash_this(hi, &rel_path, NULL, &val);
token = val;
abs_path = svn_path_join(db->fs_path, rel_path, iterpool);
svn_error_clear(svn_repos_fs_unlock(db->repos, abs_path, token,
FALSE, iterpool));
}
svn_pool_destroy(iterpool);
}
err2 = svn_fs_deltify_revision(db->fs, commit_info->revision, db->pool);
if (err1) {
svn_error_clear(err2);
return err1;
}
return err2;
}
#define RA_LOCAL_DESCRIPTION N_("Module for accessing a repository on local disk.")
static const char *
svn_ra_local__get_description(void) {
return _(RA_LOCAL_DESCRIPTION);
}
static const char * const *
svn_ra_local__get_schemes(apr_pool_t *pool) {
static const char *schemes[] = { "file", NULL };
return schemes;
}
static svn_error_t *
svn_ra_local__open(svn_ra_session_t *session,
const char *repos_URL,
const svn_ra_callbacks2_t *callbacks,
void *callback_baton,
apr_hash_t *config,
apr_pool_t *pool) {
svn_ra_local__session_baton_t *sess;
const char *fs_path;
sess = apr_pcalloc(pool, sizeof(*sess));
sess->callbacks = callbacks;
sess->callback_baton = callback_baton;
SVN_ERR_W(svn_ra_local__split_URL(&(sess->repos),
&(sess->repos_url),
&fs_path,
repos_URL,
session->pool),
_("Unable to open an ra_local session to URL"));
sess->fs_path = svn_stringbuf_create(fs_path, session->pool);
sess->fs = svn_repos_fs(sess->repos);
SVN_ERR(svn_fs_get_uuid(sess->fs, &sess->uuid, session->pool));
sess->username = NULL;
session->priv = sess;
return SVN_NO_ERROR;
}
static svn_error_t *
svn_ra_local__reparent(svn_ra_session_t *session,
const char *url,
apr_pool_t *pool) {
svn_ra_local__session_baton_t *sess = session->priv;
const char *relpath = "";
if (strcmp(url, sess->repos_url) != 0)
relpath = svn_path_is_child(sess->repos_url, url, pool);
if (! relpath)
return svn_error_createf
(SVN_ERR_RA_ILLEGAL_URL, NULL,
_("URL '%s' is not a child of the session's repository root "
"URL '%s'"), url, sess->repos_url);
relpath = apr_pstrcat(pool, "/", svn_path_uri_decode(relpath, pool), NULL);
svn_stringbuf_set(sess->fs_path, relpath);
return SVN_NO_ERROR;
}
static svn_error_t *
svn_ra_local__get_session_url(svn_ra_session_t *session,
const char **url,
apr_pool_t *pool) {
svn_ra_local__session_baton_t *sess = session->priv;
*url = svn_path_join(sess->repos_url,
svn_path_uri_encode(sess->fs_path->data + 1, pool),
pool);
return SVN_NO_ERROR;
}
static svn_error_t *
svn_ra_local__get_latest_revnum(svn_ra_session_t *session,
svn_revnum_t *latest_revnum,
apr_pool_t *pool) {
svn_ra_local__session_baton_t *sess = session->priv;
return svn_fs_youngest_rev(latest_revnum, sess->fs, pool);
}
static svn_error_t *
svn_ra_local__get_file_revs(svn_ra_session_t *session,
const char *path,
svn_revnum_t start,
svn_revnum_t end,
svn_boolean_t include_merged_revisions,
svn_file_rev_handler_t handler,
void *handler_baton,
apr_pool_t *pool) {
svn_ra_local__session_baton_t *sess = session->priv;
const char *abs_path = svn_path_join(sess->fs_path->data, path, pool);
return svn_repos_get_file_revs2(sess->repos, abs_path, start, end,
include_merged_revisions, NULL, NULL,
handler, handler_baton, pool);
}
static svn_error_t *
svn_ra_local__get_dated_revision(svn_ra_session_t *session,
svn_revnum_t *revision,
apr_time_t tm,
apr_pool_t *pool) {
svn_ra_local__session_baton_t *sess = session->priv;
return svn_repos_dated_revision(revision, sess->repos, tm, pool);
}
static svn_error_t *
svn_ra_local__change_rev_prop(svn_ra_session_t *session,
svn_revnum_t rev,
const char *name,
const svn_string_t *value,
apr_pool_t *pool) {
svn_ra_local__session_baton_t *sess = session->priv;
SVN_ERR(get_username(session, pool));
SVN_ERR(svn_repos_fs_change_rev_prop3(sess->repos, rev, sess->username,
name, value, TRUE, TRUE, NULL, NULL,
pool));
return SVN_NO_ERROR;
}
static svn_error_t *
svn_ra_local__get_uuid(svn_ra_session_t *session,
const char **uuid,
apr_pool_t *pool) {
svn_ra_local__session_baton_t *sess = session->priv;
*uuid = sess->uuid;
return SVN_NO_ERROR;
}
static svn_error_t *
svn_ra_local__get_repos_root(svn_ra_session_t *session,
const char **url,
apr_pool_t *pool) {
svn_ra_local__session_baton_t *sess = session->priv;
*url = sess->repos_url;
return SVN_NO_ERROR;
}
static svn_error_t *
svn_ra_local__rev_proplist(svn_ra_session_t *session,
svn_revnum_t rev,
apr_hash_t **props,
apr_pool_t *pool) {
svn_ra_local__session_baton_t *sess = session->priv;
return svn_repos_fs_revision_proplist(props, sess->repos, rev,
NULL, NULL, pool);
}
static svn_error_t *
svn_ra_local__rev_prop(svn_ra_session_t *session,
svn_revnum_t rev,
const char *name,
svn_string_t **value,
apr_pool_t *pool) {
svn_ra_local__session_baton_t *sess = session->priv;
return svn_repos_fs_revision_prop(value, sess->repos, rev, name,
NULL, NULL, pool);
}
static svn_error_t *
svn_ra_local__get_commit_editor(svn_ra_session_t *session,
const svn_delta_editor_t **editor,
void **edit_baton,
apr_hash_t *revprop_table,
svn_commit_callback2_t callback,
void *callback_baton,
apr_hash_t *lock_tokens,
svn_boolean_t keep_locks,
apr_pool_t *pool) {
svn_ra_local__session_baton_t *sess = session->priv;
struct deltify_etc_baton *db = apr_palloc(pool, sizeof(*db));
apr_hash_index_t *hi;
svn_fs_access_t *fs_access;
db->fs = sess->fs;
db->repos = sess->repos;
db->fs_path = sess->fs_path->data;
if (! keep_locks)
db->lock_tokens = lock_tokens;
else
db->lock_tokens = NULL;
db->pool = pool;
db->callback = callback;
db->callback_baton = callback_baton;
SVN_ERR(get_username(session, pool));
if (lock_tokens) {
SVN_ERR(svn_fs_get_access(&fs_access, sess->fs));
if (fs_access) {
for (hi = apr_hash_first(pool, lock_tokens); hi;
hi = apr_hash_next(hi)) {
void *val;
const char *token;
apr_hash_this(hi, NULL, NULL, &val);
token = val;
SVN_ERR(svn_fs_access_add_lock_token(fs_access, token));
}
}
}
revprop_table = apr_hash_copy(pool, revprop_table);
apr_hash_set(revprop_table, SVN_PROP_REVISION_AUTHOR, APR_HASH_KEY_STRING,
svn_string_create(sess->username, pool));
SVN_ERR(svn_repos_get_commit_editor5
(editor, edit_baton, sess->repos, NULL,
svn_path_uri_decode(sess->repos_url, pool), sess->fs_path->data,
revprop_table, deltify_etc, db, NULL, NULL, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
svn_ra_local__get_mergeinfo(svn_ra_session_t *session,
svn_mergeinfo_catalog_t *catalog,
const apr_array_header_t *paths,
svn_revnum_t revision,
svn_mergeinfo_inheritance_t inherit,
svn_boolean_t include_descendants,
apr_pool_t *pool) {
svn_ra_local__session_baton_t *sess = session->priv;
svn_mergeinfo_catalog_t tmp_catalog;
int i;
apr_array_header_t *abs_paths =
apr_array_make(pool, 0, sizeof(const char *));
for (i = 0; i < paths->nelts; i++) {
const char *relative_path = APR_ARRAY_IDX(paths, i, const char *);
APR_ARRAY_PUSH(abs_paths, const char *) =
svn_path_join(sess->fs_path->data, relative_path, pool);
}
SVN_ERR(svn_repos_fs_get_mergeinfo(&tmp_catalog, sess->repos, abs_paths,
revision, inherit, include_descendants,
NULL, NULL, pool));
if (apr_hash_count(tmp_catalog) > 0)
SVN_ERR(svn_mergeinfo__remove_prefix_from_catalog(catalog,
tmp_catalog,
sess->fs_path->data,
pool));
else
*catalog = NULL;
return SVN_NO_ERROR;
}
static svn_error_t *
svn_ra_local__do_update(svn_ra_session_t *session,
const svn_ra_reporter3_t **reporter,
void **report_baton,
svn_revnum_t update_revision,
const char *update_target,
svn_depth_t depth,
svn_boolean_t send_copyfrom_args,
const svn_delta_editor_t *update_editor,
void *update_baton,
apr_pool_t *pool) {
return make_reporter(session,
reporter,
report_baton,
update_revision,
update_target,
NULL,
TRUE,
depth,
send_copyfrom_args,
FALSE,
update_editor,
update_baton,
pool);
}
static svn_error_t *
svn_ra_local__do_switch(svn_ra_session_t *session,
const svn_ra_reporter3_t **reporter,
void **report_baton,
svn_revnum_t update_revision,
const char *update_target,
svn_depth_t depth,
const char *switch_url,
const svn_delta_editor_t *update_editor,
void *update_baton,
apr_pool_t *pool) {
return make_reporter(session,
reporter,
report_baton,
update_revision,
update_target,
switch_url,
TRUE,
depth,
FALSE,
TRUE,
update_editor,
update_baton,
pool);
}
static svn_error_t *
svn_ra_local__do_status(svn_ra_session_t *session,
const svn_ra_reporter3_t **reporter,
void **report_baton,
const char *status_target,
svn_revnum_t revision,
svn_depth_t depth,
const svn_delta_editor_t *status_editor,
void *status_baton,
apr_pool_t *pool) {
return make_reporter(session,
reporter,
report_baton,
revision,
status_target,
NULL,
FALSE,
depth,
FALSE,
FALSE,
status_editor,
status_baton,
pool);
}
static svn_error_t *
svn_ra_local__do_diff(svn_ra_session_t *session,
const svn_ra_reporter3_t **reporter,
void **report_baton,
svn_revnum_t update_revision,
const char *update_target,
svn_depth_t depth,
svn_boolean_t ignore_ancestry,
svn_boolean_t text_deltas,
const char *switch_url,
const svn_delta_editor_t *update_editor,
void *update_baton,
apr_pool_t *pool) {
return make_reporter(session,
reporter,
report_baton,
update_revision,
update_target,
switch_url,
text_deltas,
depth,
FALSE,
ignore_ancestry,
update_editor,
update_baton,
pool);
}
struct log_baton {
svn_ra_local__session_baton_t *sess;
svn_log_entry_receiver_t real_cb;
void *real_baton;
};
static svn_error_t *
cancellation_log_receiver(void *baton,
svn_log_entry_t *log_entry,
apr_pool_t *pool) {
struct log_baton *b = baton;
svn_ra_local__session_baton_t *sess = b->sess;
SVN_ERR((sess->callbacks->cancel_func)(sess->callback_baton));
return b->real_cb(b->real_baton, log_entry, pool);
}
static svn_error_t *
svn_ra_local__get_log(svn_ra_session_t *session,
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
apr_pool_t *pool) {
svn_ra_local__session_baton_t *sess = session->priv;
int i;
struct log_baton lb;
apr_array_header_t *abs_paths =
apr_array_make(pool, 0, sizeof(const char *));
if (paths) {
for (i = 0; i < paths->nelts; i++) {
const char *relative_path = APR_ARRAY_IDX(paths, i, const char *);
APR_ARRAY_PUSH(abs_paths, const char *) =
svn_path_join(sess->fs_path->data, relative_path, pool);
}
}
if (sess->callbacks &&
sess->callbacks->cancel_func) {
lb.real_cb = receiver;
lb.real_baton = receiver_baton;
lb.sess = sess;
receiver = cancellation_log_receiver;
receiver_baton = &lb;
}
return svn_repos_get_logs4(sess->repos,
abs_paths,
start,
end,
limit,
discover_changed_paths,
strict_node_history,
include_merged_revisions,
revprops,
NULL, NULL,
receiver,
receiver_baton,
pool);
}
static svn_error_t *
svn_ra_local__do_check_path(svn_ra_session_t *session,
const char *path,
svn_revnum_t revision,
svn_node_kind_t *kind,
apr_pool_t *pool) {
svn_ra_local__session_baton_t *sess = session->priv;
svn_fs_root_t *root;
const char *abs_path = svn_path_join(sess->fs_path->data, path, pool);
if (! SVN_IS_VALID_REVNUM(revision))
SVN_ERR(svn_fs_youngest_rev(&revision, sess->fs, pool));
SVN_ERR(svn_fs_revision_root(&root, sess->fs, revision, pool));
SVN_ERR(svn_fs_check_path(kind, root, abs_path, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
svn_ra_local__stat(svn_ra_session_t *session,
const char *path,
svn_revnum_t revision,
svn_dirent_t **dirent,
apr_pool_t *pool) {
svn_ra_local__session_baton_t *sess = session->priv;
svn_fs_root_t *root;
const char *abs_path = svn_path_join(sess->fs_path->data, path, pool);
if (! SVN_IS_VALID_REVNUM(revision))
SVN_ERR(svn_fs_youngest_rev(&revision, sess->fs, pool));
SVN_ERR(svn_fs_revision_root(&root, sess->fs, revision, pool));
SVN_ERR(svn_repos_stat(dirent, root, abs_path, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
get_node_props(apr_hash_t **props,
svn_ra_local__session_baton_t *sess,
svn_fs_root_t *root,
const char *path,
apr_pool_t *pool) {
svn_revnum_t cmt_rev;
const char *cmt_date, *cmt_author;
SVN_ERR(svn_fs_node_proplist(props, root, path, pool));
SVN_ERR(svn_repos_get_committed_info(&cmt_rev, &cmt_date,
&cmt_author, root, path, pool));
apr_hash_set(*props,
SVN_PROP_ENTRY_COMMITTED_REV,
APR_HASH_KEY_STRING,
svn_string_createf(pool, "%ld", cmt_rev));
apr_hash_set(*props,
SVN_PROP_ENTRY_COMMITTED_DATE,
APR_HASH_KEY_STRING,
cmt_date ? svn_string_create(cmt_date, pool) : NULL);
apr_hash_set(*props,
SVN_PROP_ENTRY_LAST_AUTHOR,
APR_HASH_KEY_STRING,
cmt_author ? svn_string_create(cmt_author, pool) : NULL);
apr_hash_set(*props,
SVN_PROP_ENTRY_UUID,
APR_HASH_KEY_STRING,
svn_string_create(sess->uuid, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
svn_ra_local__get_file(svn_ra_session_t *session,
const char *path,
svn_revnum_t revision,
svn_stream_t *stream,
svn_revnum_t *fetched_rev,
apr_hash_t **props,
apr_pool_t *pool) {
svn_fs_root_t *root;
svn_stream_t *contents;
svn_revnum_t youngest_rev;
svn_ra_local__session_baton_t *sess = session->priv;
const char *abs_path = svn_path_join(sess->fs_path->data, path, pool);
if (! SVN_IS_VALID_REVNUM(revision)) {
SVN_ERR(svn_fs_youngest_rev(&youngest_rev, sess->fs, pool));
SVN_ERR(svn_fs_revision_root(&root, sess->fs, youngest_rev, pool));
if (fetched_rev != NULL)
*fetched_rev = youngest_rev;
} else
SVN_ERR(svn_fs_revision_root(&root, sess->fs, revision, pool));
if (stream) {
SVN_ERR(svn_fs_file_contents(&contents, root, abs_path, pool));
SVN_ERR(svn_stream_copy2(contents, stream,
sess->callbacks
? sess->callbacks->cancel_func : NULL,
sess->callback_baton,
pool));
}
if (props)
SVN_ERR(get_node_props(props, sess, root, abs_path, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
svn_ra_local__get_dir(svn_ra_session_t *session,
apr_hash_t **dirents,
svn_revnum_t *fetched_rev,
apr_hash_t **props,
const char *path,
svn_revnum_t revision,
apr_uint32_t dirent_fields,
apr_pool_t *pool) {
svn_fs_root_t *root;
svn_revnum_t youngest_rev;
apr_hash_t *entries;
apr_hash_index_t *hi;
svn_ra_local__session_baton_t *sess = session->priv;
apr_pool_t *subpool;
const char *abs_path = svn_path_join(sess->fs_path->data, path, pool);
if (! SVN_IS_VALID_REVNUM(revision)) {
SVN_ERR(svn_fs_youngest_rev(&youngest_rev, sess->fs, pool));
SVN_ERR(svn_fs_revision_root(&root, sess->fs, youngest_rev, pool));
if (fetched_rev != NULL)
*fetched_rev = youngest_rev;
} else
SVN_ERR(svn_fs_revision_root(&root, sess->fs, revision, pool));
if (dirents) {
SVN_ERR(svn_fs_dir_entries(&entries, root, abs_path, pool));
*dirents = apr_hash_make(pool);
subpool = svn_pool_create(pool);
for (hi = apr_hash_first(pool, entries); hi; hi = apr_hash_next(hi)) {
const void *key;
void *val;
apr_hash_t *prophash;
const char *datestring, *entryname, *fullpath;
svn_fs_dirent_t *fs_entry;
svn_dirent_t *entry = apr_pcalloc(pool, sizeof(*entry));
svn_pool_clear(subpool);
apr_hash_this(hi, &key, NULL, &val);
entryname = (const char *) key;
fs_entry = (svn_fs_dirent_t *) val;
fullpath = svn_path_join(abs_path, entryname, subpool);
if (dirent_fields & SVN_DIRENT_KIND) {
entry->kind = fs_entry->kind;
}
if (dirent_fields & SVN_DIRENT_SIZE) {
if (entry->kind == svn_node_dir)
entry->size = 0;
else
SVN_ERR(svn_fs_file_length(&(entry->size), root,
fullpath, subpool));
}
if (dirent_fields & SVN_DIRENT_HAS_PROPS) {
SVN_ERR(svn_fs_node_proplist(&prophash, root, fullpath,
subpool));
entry->has_props = (apr_hash_count(prophash)) ? TRUE : FALSE;
}
if ((dirent_fields & SVN_DIRENT_TIME)
|| (dirent_fields & SVN_DIRENT_LAST_AUTHOR)
|| (dirent_fields & SVN_DIRENT_CREATED_REV)) {
SVN_ERR(svn_repos_get_committed_info(&(entry->created_rev),
&datestring,
&(entry->last_author),
root, fullpath, subpool));
if (datestring)
SVN_ERR(svn_time_from_cstring(&(entry->time), datestring,
pool));
if (entry->last_author)
entry->last_author = apr_pstrdup(pool, entry->last_author);
}
apr_hash_set(*dirents, entryname, APR_HASH_KEY_STRING, entry);
}
svn_pool_destroy(subpool);
}
if (props)
SVN_ERR(get_node_props(props, sess, root, abs_path, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
svn_ra_local__get_locations(svn_ra_session_t *session,
apr_hash_t **locations,
const char *path,
svn_revnum_t peg_revision,
apr_array_header_t *location_revisions,
apr_pool_t *pool) {
svn_ra_local__session_baton_t *sess = session->priv;
const char *abs_path = svn_path_join(sess->fs_path->data, path, pool);
return svn_repos_trace_node_locations(sess->fs, locations, abs_path,
peg_revision, location_revisions,
NULL, NULL, pool);
}
static svn_error_t *
svn_ra_local__get_location_segments(svn_ra_session_t *session,
const char *path,
svn_revnum_t peg_revision,
svn_revnum_t start_rev,
svn_revnum_t end_rev,
svn_location_segment_receiver_t receiver,
void *receiver_baton,
apr_pool_t *pool) {
svn_ra_local__session_baton_t *sess = session->priv;
const char *abs_path = svn_path_join(sess->fs_path->data, path, pool);
return svn_repos_node_location_segments(sess->repos, abs_path,
peg_revision, start_rev, end_rev,
receiver, receiver_baton,
NULL, NULL, pool);
}
static svn_error_t *
svn_ra_local__lock(svn_ra_session_t *session,
apr_hash_t *path_revs,
const char *comment,
svn_boolean_t force,
svn_ra_lock_callback_t lock_func,
void *lock_baton,
apr_pool_t *pool) {
svn_ra_local__session_baton_t *sess = session->priv;
apr_hash_index_t *hi;
apr_pool_t *iterpool = svn_pool_create(pool);
SVN_ERR(get_username(session, pool));
for (hi = apr_hash_first(pool, path_revs); hi; hi = apr_hash_next(hi)) {
svn_lock_t *lock;
const void *key;
const char *path;
void *val;
svn_revnum_t *revnum;
const char *abs_path;
svn_error_t *err, *callback_err = NULL;
svn_pool_clear(iterpool);
apr_hash_this(hi, &key, NULL, &val);
path = key;
revnum = val;
abs_path = svn_path_join(sess->fs_path->data, path, iterpool);
err = svn_repos_fs_lock(&lock, sess->repos, abs_path, NULL, comment,
FALSE ,
0 , *revnum, force,
iterpool);
if (err && !SVN_ERR_IS_LOCK_ERROR(err))
return err;
if (lock_func)
callback_err = lock_func(lock_baton, path, TRUE, err ? NULL : lock,
err, iterpool);
svn_error_clear(err);
if (callback_err)
return callback_err;
}
svn_pool_destroy(iterpool);
return SVN_NO_ERROR;
}
static svn_error_t *
svn_ra_local__unlock(svn_ra_session_t *session,
apr_hash_t *path_tokens,
svn_boolean_t force,
svn_ra_lock_callback_t lock_func,
void *lock_baton,
apr_pool_t *pool) {
svn_ra_local__session_baton_t *sess = session->priv;
apr_hash_index_t *hi;
apr_pool_t *iterpool = svn_pool_create(pool);
SVN_ERR(get_username(session, pool));
for (hi = apr_hash_first(pool, path_tokens); hi; hi = apr_hash_next(hi)) {
const void *key;
const char *path;
void *val;
const char *abs_path, *token;
svn_error_t *err, *callback_err = NULL;
svn_pool_clear(iterpool);
apr_hash_this(hi, &key, NULL, &val);
path = key;
if (strcmp(val, "") != 0)
token = val;
else
token = NULL;
abs_path = svn_path_join(sess->fs_path->data, path, iterpool);
err = svn_repos_fs_unlock(sess->repos, abs_path, token, force,
iterpool);
if (err && !SVN_ERR_IS_UNLOCK_ERROR(err))
return err;
if (lock_func)
callback_err = lock_func(lock_baton, path, FALSE, NULL, err, iterpool);
svn_error_clear(err);
if (callback_err)
return callback_err;
}
svn_pool_destroy(iterpool);
return SVN_NO_ERROR;
}
static svn_error_t *
svn_ra_local__get_lock(svn_ra_session_t *session,
svn_lock_t **lock,
const char *path,
apr_pool_t *pool) {
svn_ra_local__session_baton_t *sess = session->priv;
const char *abs_path = svn_path_join(sess->fs_path->data, path, pool);
return svn_fs_get_lock(lock, sess->fs, abs_path, pool);
}
static svn_error_t *
svn_ra_local__get_locks(svn_ra_session_t *session,
apr_hash_t **locks,
const char *path,
apr_pool_t *pool) {
svn_ra_local__session_baton_t *sess = session->priv;
const char *abs_path = svn_path_join(sess->fs_path->data, path, pool);
return svn_repos_fs_get_locks(locks, sess->repos, abs_path,
NULL, NULL, pool);
}
static svn_error_t *
svn_ra_local__replay(svn_ra_session_t *session,
svn_revnum_t revision,
svn_revnum_t low_water_mark,
svn_boolean_t send_deltas,
const svn_delta_editor_t *editor,
void *edit_baton,
apr_pool_t *pool) {
svn_ra_local__session_baton_t *sess = session->priv;
svn_fs_root_t *root;
SVN_ERR(svn_fs_revision_root(&root, svn_repos_fs(sess->repos),
revision, pool));
SVN_ERR(svn_repos_replay2(root, sess->fs_path->data, low_water_mark,
send_deltas, editor, edit_baton, NULL, NULL,
pool));
return SVN_NO_ERROR;
}
static svn_error_t *
svn_ra_local__replay_range(svn_ra_session_t *session,
svn_revnum_t start_revision,
svn_revnum_t end_revision,
svn_revnum_t low_water_mark,
svn_boolean_t send_deltas,
svn_ra_replay_revstart_callback_t revstart_func,
svn_ra_replay_revfinish_callback_t revfinish_func,
void *replay_baton,
apr_pool_t *pool) {
return svn_error_create(SVN_ERR_RA_NOT_IMPLEMENTED, NULL, NULL);
}
static svn_error_t *
svn_ra_local__has_capability(svn_ra_session_t *session,
svn_boolean_t *has,
const char *capability,
apr_pool_t *pool) {
svn_ra_local__session_baton_t *sess = session->priv;
if (strcmp(capability, SVN_RA_CAPABILITY_DEPTH) == 0
|| strcmp(capability, SVN_RA_CAPABILITY_LOG_REVPROPS) == 0
|| strcmp(capability, SVN_RA_CAPABILITY_PARTIAL_REPLAY) == 0
|| strcmp(capability, SVN_RA_CAPABILITY_COMMIT_REVPROPS) == 0) {
*has = TRUE;
} else if (strcmp(capability, SVN_RA_CAPABILITY_MERGEINFO) == 0) {
SVN_ERR(svn_repos_has_capability(sess->repos, has,
SVN_REPOS_CAPABILITY_MERGEINFO,
pool));
} else {
return svn_error_createf
(SVN_ERR_UNKNOWN_CAPABILITY, NULL,
_("Don't know anything about capability '%s'"), capability);
}
return SVN_NO_ERROR;
}
static const svn_version_t *
ra_local_version(void) {
SVN_VERSION_BODY;
}
static const svn_ra__vtable_t ra_local_vtable = {
ra_local_version,
svn_ra_local__get_description,
svn_ra_local__get_schemes,
svn_ra_local__open,
svn_ra_local__reparent,
svn_ra_local__get_session_url,
svn_ra_local__get_latest_revnum,
svn_ra_local__get_dated_revision,
svn_ra_local__change_rev_prop,
svn_ra_local__rev_proplist,
svn_ra_local__rev_prop,
svn_ra_local__get_commit_editor,
svn_ra_local__get_file,
svn_ra_local__get_dir,
svn_ra_local__get_mergeinfo,
svn_ra_local__do_update,
svn_ra_local__do_switch,
svn_ra_local__do_status,
svn_ra_local__do_diff,
svn_ra_local__get_log,
svn_ra_local__do_check_path,
svn_ra_local__stat,
svn_ra_local__get_uuid,
svn_ra_local__get_repos_root,
svn_ra_local__get_locations,
svn_ra_local__get_location_segments,
svn_ra_local__get_file_revs,
svn_ra_local__lock,
svn_ra_local__unlock,
svn_ra_local__get_lock,
svn_ra_local__get_locks,
svn_ra_local__replay,
svn_ra_local__has_capability,
svn_ra_local__replay_range
};
svn_error_t *
svn_ra_local__init(const svn_version_t *loader_version,
const svn_ra__vtable_t **vtable,
apr_pool_t *pool) {
static const svn_version_checklist_t checklist[] = {
{ "svn_subr", svn_subr_version },
{ "svn_delta", svn_delta_version },
{ "svn_repos", svn_repos_version },
{ "svn_fs", svn_fs_version },
{ NULL, NULL }
};
if (loader_version->major != SVN_VER_MAJOR)
return svn_error_createf(SVN_ERR_VERSION_MISMATCH, NULL,
_("Unsupported RA loader version (%d) for "
"ra_local"),
loader_version->major);
SVN_ERR(svn_ver_check_list(ra_local_version(), checklist));
#if !defined(SVN_LIBSVN_CLIENT_LINKS_RA_LOCAL)
SVN_ERR(svn_fs_initialize(pool));
#endif
*vtable = &ra_local_vtable;
return SVN_NO_ERROR;
}
#define NAME "ra_local"
#define DESCRIPTION RA_LOCAL_DESCRIPTION
#define VTBL ra_local_vtable
#define INITFUNC svn_ra_local__init
#define COMPAT_INITFUNC svn_ra_local_init
#include "../libsvn_ra/wrapper_template.h"