#define APR_WANT_STRFUNC
#include <apr_want.h>
#include <apr_strings.h>
#include <apr_pools.h>
#include "client.h"
#include "svn_pools.h"
#include "svn_client.h"
#include "svn_compat.h"
#include "svn_error.h"
#include "svn_path.h"
#include "svn_sorts.h"
#include "svn_private_config.h"
#include "private/svn_wc_private.h"
static svn_error_t *
revnum_receiver(void *baton,
svn_log_entry_t *log_entry,
apr_pool_t *pool) {
if (SVN_IS_VALID_REVNUM(log_entry->revision))
*((svn_revnum_t *) baton) = log_entry->revision;
return SVN_NO_ERROR;
}
svn_error_t *
svn_client__oldest_rev_at_path(svn_revnum_t *oldest_rev,
svn_ra_session_t *ra_session,
const char *rel_path,
svn_revnum_t rev,
apr_pool_t *pool) {
apr_array_header_t *rel_paths = apr_array_make(pool, 1, sizeof(rel_path));
apr_array_header_t *revprops = apr_array_make(pool, 0, sizeof(char *));
*oldest_rev = SVN_INVALID_REVNUM;
APR_ARRAY_PUSH(rel_paths, const char *) = rel_path;
return svn_ra_get_log2(ra_session, rel_paths, 1, rev, 1, FALSE, TRUE,
FALSE, revprops, revnum_receiver, oldest_rev, pool);
}
typedef struct {
const char *target_path;
const char *path;
svn_revnum_t rev;
apr_pool_t *pool;
} copyfrom_info_t;
static svn_error_t *
copyfrom_info_receiver(void *baton,
svn_log_entry_t *log_entry,
apr_pool_t *pool) {
copyfrom_info_t *copyfrom_info = baton;
if (copyfrom_info->path)
return SVN_NO_ERROR;
if (log_entry->changed_paths) {
int i;
const char *path;
svn_log_changed_path_t *changed_path;
apr_array_header_t *sorted_changed_paths =
svn_sort__hash(log_entry->changed_paths,
svn_sort_compare_items_as_paths, pool);
for (i = (sorted_changed_paths->nelts -1) ; i >= 0 ; i--) {
svn_sort__item_t *item = &APR_ARRAY_IDX(sorted_changed_paths, i,
svn_sort__item_t);
path = item->key;
changed_path = item->value;
if (changed_path->copyfrom_path &&
SVN_IS_VALID_REVNUM(changed_path->copyfrom_rev) &&
svn_path_is_ancestor(path, copyfrom_info->target_path)) {
if (strcmp(path, copyfrom_info->target_path) == 0) {
copyfrom_info->path =
apr_pstrdup(copyfrom_info->pool,
changed_path->copyfrom_path);
} else {
copyfrom_info->path =
apr_pstrcat(copyfrom_info->pool,
changed_path->copyfrom_path,
copyfrom_info->target_path +
strlen(path), NULL);
}
copyfrom_info->rev = changed_path->copyfrom_rev;
break;
}
}
}
return SVN_NO_ERROR;
}
svn_error_t *
svn_client__get_copy_source(const char *path_or_url,
const svn_opt_revision_t *revision,
const char **copyfrom_path,
svn_revnum_t *copyfrom_rev,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
svn_error_t *err;
copyfrom_info_t copyfrom_info = { NULL, NULL, SVN_INVALID_REVNUM, pool };
apr_array_header_t *targets = apr_array_make(pool, 1, sizeof(path_or_url));
apr_pool_t *sesspool = svn_pool_create(pool);
svn_ra_session_t *ra_session;
svn_revnum_t at_rev;
const char *at_url;
SVN_ERR(svn_client__ra_session_from_path(&ra_session, &at_rev, &at_url,
path_or_url, NULL,
revision, revision,
ctx, sesspool));
SVN_ERR(svn_client__path_relative_to_root(&copyfrom_info.target_path,
path_or_url, NULL, TRUE,
ra_session, NULL, pool));
APR_ARRAY_PUSH(targets, const char *) = "";
err = svn_ra_get_log2(ra_session, targets, at_rev, 1, 0, TRUE,
TRUE, FALSE,
apr_array_make(pool, 0, sizeof(const char *)),
copyfrom_info_receiver, &copyfrom_info, pool);
svn_pool_destroy(sesspool);
if (err) {
if (err->apr_err == SVN_ERR_FS_NOT_FOUND ||
err->apr_err == SVN_ERR_RA_DAV_REQUEST_FAILED) {
svn_error_clear(err);
err = SVN_NO_ERROR;
*copyfrom_path = NULL;
*copyfrom_rev = SVN_INVALID_REVNUM;
}
return err;
}
*copyfrom_path = copyfrom_info.path;
*copyfrom_rev = copyfrom_info.rev;
return SVN_NO_ERROR;
}
typedef struct {
svn_client_ctx_t *ctx;
svn_ra_session_t *ra_session;
const apr_array_header_t *revprops;
svn_log_entry_receiver_t receiver;
void *baton;
} pre_15_receiver_baton_t;
static svn_error_t *
pre_15_receiver(void *baton, svn_log_entry_t *log_entry, apr_pool_t *pool) {
pre_15_receiver_baton_t *rb = baton;
if (log_entry->revision == SVN_INVALID_REVNUM)
return rb->receiver(rb->baton, log_entry, pool);
if (rb->revprops) {
int i;
svn_boolean_t want_author, want_date, want_log;
want_author = want_date = want_log = FALSE;
for (i = 0; i < rb->revprops->nelts; i++) {
const char *name = APR_ARRAY_IDX(rb->revprops, i, const char *);
svn_string_t *value;
if (strcmp(name, SVN_PROP_REVISION_AUTHOR) == 0) {
want_author = TRUE;
continue;
}
if (strcmp(name, SVN_PROP_REVISION_DATE) == 0) {
want_date = TRUE;
continue;
}
if (strcmp(name, SVN_PROP_REVISION_LOG) == 0) {
want_log = TRUE;
continue;
}
SVN_ERR(svn_ra_rev_prop(rb->ra_session, log_entry->revision,
name, &value, pool));
if (log_entry->revprops == NULL)
log_entry->revprops = apr_hash_make(pool);
apr_hash_set(log_entry->revprops, (const void *)name,
APR_HASH_KEY_STRING, (const void *)value);
}
if (log_entry->revprops) {
if (!want_author)
apr_hash_set(log_entry->revprops, SVN_PROP_REVISION_AUTHOR,
APR_HASH_KEY_STRING, NULL);
if (!want_date)
apr_hash_set(log_entry->revprops, SVN_PROP_REVISION_DATE,
APR_HASH_KEY_STRING, NULL);
if (!want_log)
apr_hash_set(log_entry->revprops, SVN_PROP_REVISION_LOG,
APR_HASH_KEY_STRING, NULL);
}
} else {
SVN_ERR(svn_ra_rev_proplist(rb->ra_session, log_entry->revision,
&log_entry->revprops, pool));
}
return rb->receiver(rb->baton, log_entry, pool);
}
svn_error_t *
svn_client_log4(const apr_array_header_t *targets,
const svn_opt_revision_t *peg_revision,
const svn_opt_revision_t *start,
const svn_opt_revision_t *end,
int limit,
svn_boolean_t discover_changed_paths,
svn_boolean_t strict_node_history,
svn_boolean_t include_merged_revisions,
const apr_array_header_t *revprops,
svn_log_entry_receiver_t real_receiver,
void *real_receiver_baton,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
svn_ra_session_t *ra_session;
const char *url_or_path;
const char *actual_url;
apr_array_header_t *condensed_targets;
svn_revnum_t ignored_revnum;
svn_opt_revision_t session_opt_rev;
const char *ra_target;
if ((start->kind == svn_opt_revision_unspecified)
|| (end->kind == svn_opt_revision_unspecified)) {
return svn_error_create
(SVN_ERR_CLIENT_BAD_REVISION, NULL,
_("Missing required revision specification"));
}
url_or_path = APR_ARRAY_IDX(targets, 0, const char *);
if (svn_path_is_url(url_or_path)) {
if (peg_revision->kind == svn_opt_revision_base
|| peg_revision->kind == svn_opt_revision_committed
|| peg_revision->kind == svn_opt_revision_previous)
return svn_error_create
(SVN_ERR_CLIENT_BAD_REVISION, NULL,
_("Revision type requires a working copy path, not a URL"));
condensed_targets = apr_array_make(pool, 1, sizeof(const char *));
if (targets->nelts > 1) {
int i;
for (i = 1; i < targets->nelts; i++)
APR_ARRAY_PUSH(condensed_targets, const char *) =
APR_ARRAY_IDX(targets, i, const char *);
} else {
APR_ARRAY_PUSH(condensed_targets, const char *) = "";
}
} else {
svn_wc_adm_access_t *adm_access;
apr_array_header_t *target_urls;
apr_array_header_t *real_targets;
apr_pool_t *iterpool;
int i;
if (targets->nelts > 1)
return svn_error_create(SVN_ERR_UNSUPPORTED_FEATURE, NULL,
_("When specifying working copy paths, only "
"one target may be given"));
target_urls = apr_array_make(pool, 1, sizeof(const char *));
real_targets = apr_array_make(pool, 1, sizeof(const char *));
iterpool = svn_pool_create(pool);
for (i = 0; i < targets->nelts; i++) {
const svn_wc_entry_t *entry;
const char *URL;
const char *target = APR_ARRAY_IDX(targets, i, const char *);
svn_pool_clear(iterpool);
SVN_ERR(svn_wc_adm_probe_open3(&adm_access, NULL, target,
FALSE, 0, ctx->cancel_func,
ctx->cancel_baton, iterpool));
SVN_ERR(svn_wc__entry_versioned(&entry, target, adm_access, FALSE,
iterpool));
if (! entry->url)
return svn_error_createf
(SVN_ERR_ENTRY_MISSING_URL, NULL,
_("Entry '%s' has no URL"),
svn_path_local_style(target, pool));
URL = apr_pstrdup(pool, entry->url);
SVN_ERR(svn_wc_adm_close(adm_access));
APR_ARRAY_PUSH(target_urls, const char *) = URL;
APR_ARRAY_PUSH(real_targets, const char *) = target;
}
svn_pool_destroy(iterpool);
if (target_urls->nelts == 0)
return SVN_NO_ERROR;
SVN_ERR(svn_path_condense_targets(&url_or_path, &condensed_targets,
target_urls, TRUE, pool));
if (condensed_targets->nelts == 0)
APR_ARRAY_PUSH(condensed_targets, const char *) = "";
targets = real_targets;
}
if (start->kind == svn_opt_revision_number &&
end->kind == svn_opt_revision_number)
session_opt_rev = (start->value.number > end->value.number ?
*start : *end);
else if (start->kind == svn_opt_revision_date &&
end->kind == svn_opt_revision_date)
session_opt_rev = (start->value.date > end->value.date ? *start : *end);
else
session_opt_rev.kind = svn_opt_revision_unspecified;
{
if (peg_revision->kind == svn_opt_revision_base
|| peg_revision->kind == svn_opt_revision_committed
|| peg_revision->kind == svn_opt_revision_previous
|| peg_revision->kind == svn_opt_revision_working)
SVN_ERR(svn_path_condense_targets(&ra_target, NULL, targets, TRUE, pool));
else
ra_target = url_or_path;
SVN_ERR(svn_client__ra_session_from_path(&ra_session, &ignored_revnum,
&actual_url, ra_target, NULL,
peg_revision, &session_opt_rev,
ctx, pool));
}
{
svn_revnum_t start_revnum, end_revnum, youngest_rev = SVN_INVALID_REVNUM;
const char *path = APR_ARRAY_IDX(targets, 0, const char *);
svn_boolean_t has_log_revprops;
SVN_ERR(svn_client__get_revision_number
(&start_revnum, &youngest_rev, ra_session, start, path, pool));
SVN_ERR(svn_client__get_revision_number
(&end_revnum, &youngest_rev, ra_session, end, path, pool));
SVN_ERR(svn_ra_has_capability(ra_session, &has_log_revprops,
SVN_RA_CAPABILITY_LOG_REVPROPS, pool));
if (has_log_revprops)
return svn_ra_get_log2(ra_session,
condensed_targets,
start_revnum,
end_revnum,
limit,
discover_changed_paths,
strict_node_history,
include_merged_revisions,
revprops,
real_receiver,
real_receiver_baton,
pool);
else {
pre_15_receiver_baton_t rb;
rb.ctx = ctx;
SVN_ERR(svn_client_open_ra_session(&rb.ra_session, actual_url,
ctx, pool));
rb.revprops = revprops;
rb.receiver = real_receiver;
rb.baton = real_receiver_baton;
SVN_ERR(svn_client__ra_session_from_path(&ra_session,
&ignored_revnum,
&actual_url, ra_target, NULL,
peg_revision,
&session_opt_rev,
ctx, pool));
return svn_ra_get_log2(ra_session,
condensed_targets,
start_revnum,
end_revnum,
limit,
discover_changed_paths,
strict_node_history,
include_merged_revisions,
svn_compat_log_revprops_in(pool),
pre_15_receiver,
&rb,
pool);
}
}
}
svn_error_t *
svn_client_log3(const apr_array_header_t *targets,
const svn_opt_revision_t *peg_revision,
const svn_opt_revision_t *start,
const svn_opt_revision_t *end,
int limit,
svn_boolean_t discover_changed_paths,
svn_boolean_t strict_node_history,
svn_log_message_receiver_t receiver,
void *receiver_baton,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
svn_log_entry_receiver_t receiver2;
void *receiver2_baton;
svn_compat_wrap_log_receiver(&receiver2, &receiver2_baton,
receiver, receiver_baton,
pool);
return svn_client_log4(targets, peg_revision, start, end, limit,
discover_changed_paths, strict_node_history, FALSE,
svn_compat_log_revprops_in(pool),
receiver2, receiver2_baton, ctx, pool);
}
svn_error_t *
svn_client_log2(const apr_array_header_t *targets,
const svn_opt_revision_t *start,
const svn_opt_revision_t *end,
int limit,
svn_boolean_t discover_changed_paths,
svn_boolean_t strict_node_history,
svn_log_message_receiver_t receiver,
void *receiver_baton,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
svn_opt_revision_t peg_revision;
peg_revision.kind = svn_opt_revision_unspecified;
return svn_client_log3(targets, &peg_revision, start, end, limit,
discover_changed_paths, strict_node_history,
receiver, receiver_baton, ctx, pool);
}
svn_error_t *
svn_client_log(const apr_array_header_t *targets,
const svn_opt_revision_t *start,
const svn_opt_revision_t *end,
svn_boolean_t discover_changed_paths,
svn_boolean_t strict_node_history,
svn_log_message_receiver_t receiver,
void *receiver_baton,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
svn_error_t *err = SVN_NO_ERROR;
err = svn_client_log2(targets, start, end, 0, discover_changed_paths,
strict_node_history, receiver, receiver_baton, ctx,
pool);
if (err && (err->apr_err == SVN_ERR_FS_NO_SUCH_REVISION)
&& (start->kind == svn_opt_revision_head)
&& ((end->kind == svn_opt_revision_number)
&& (end->value.number == 1))) {
svn_error_clear(err);
err = SVN_NO_ERROR;
SVN_ERR(receiver(receiver_baton,
NULL, 0, "", "", _("No commits in repository"),
pool));
}
return err;
}
