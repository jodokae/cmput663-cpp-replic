#include <apr_pools.h>
#include <assert.h>
#include "svn_error.h"
#include "svn_pools.h"
#include "svn_sorts.h"
#include "svn_path.h"
#include "svn_ra.h"
#include "svn_io.h"
#include "svn_compat.h"
#include "ra_loader.h"
#include "svn_private_config.h"
static int
compare_revisions(const void *a, const void *b) {
svn_revnum_t a_rev = *(const svn_revnum_t *)a;
svn_revnum_t b_rev = *(const svn_revnum_t *)b;
if (a_rev == b_rev)
return 0;
return a_rev < b_rev ? -1 : 1;
}
static svn_error_t *
prev_log_path(const char **prev_path_p,
char *action_p,
svn_revnum_t *copyfrom_rev_p,
apr_hash_t *changed_paths,
const char *path,
svn_node_kind_t kind,
svn_revnum_t revision,
apr_pool_t *pool) {
svn_log_changed_path_t *change;
const char *prev_path = NULL;
assert(path);
if (action_p)
*action_p = 'M';
if (copyfrom_rev_p)
*copyfrom_rev_p = SVN_INVALID_REVNUM;
if (changed_paths) {
change = apr_hash_get(changed_paths, path, APR_HASH_KEY_STRING);
if (change) {
if (change->action != 'A' && change->action != 'R') {
prev_path = path;
} else {
if (change->copyfrom_path)
prev_path = apr_pstrdup(pool, change->copyfrom_path);
else
prev_path = NULL;
*prev_path_p = prev_path;
if (action_p)
*action_p = change->action;
if (copyfrom_rev_p)
*copyfrom_rev_p = change->copyfrom_rev;
return SVN_NO_ERROR;
}
}
if (apr_hash_count(changed_paths)) {
int i;
apr_array_header_t *paths;
paths = svn_sort__hash(changed_paths,
svn_sort_compare_items_as_paths, pool);
for (i = paths->nelts; i > 0; i--) {
svn_sort__item_t item = APR_ARRAY_IDX(paths,
i - 1, svn_sort__item_t);
const char *ch_path = item.key;
int len = strlen(ch_path);
if (! ((strncmp(ch_path, path, len) == 0) && (path[len] == '/')))
continue;
change = apr_hash_get(changed_paths, ch_path, len);
if (change->copyfrom_path) {
if (action_p)
*action_p = change->action;
if (copyfrom_rev_p)
*copyfrom_rev_p = change->copyfrom_rev;
prev_path = svn_path_join(change->copyfrom_path,
path + len + 1, pool);
break;
}
}
}
}
if (! prev_path) {
if (kind == svn_node_dir)
prev_path = apr_pstrdup(pool, path);
else
return svn_error_createf(SVN_ERR_CLIENT_UNRELATED_RESOURCES, NULL,
_("Missing changed-path information for "
"'%s' in revision %ld"),
svn_path_local_style(path, pool), revision);
}
*prev_path_p = prev_path;
return SVN_NO_ERROR;
}
struct log_receiver_baton {
svn_node_kind_t kind;
const char *last_path;
svn_revnum_t peg_revision;
apr_array_header_t *location_revisions;
const char *peg_path;
apr_hash_t *locations;
apr_pool_t *pool;
};
static svn_error_t *
log_receiver(void *baton,
svn_log_entry_t *log_entry,
apr_pool_t *pool) {
struct log_receiver_baton *lrb = baton;
apr_pool_t *hash_pool = apr_hash_pool_get(lrb->locations);
const char *current_path = lrb->last_path;
const char *prev_path;
if (! log_entry->changed_paths)
return SVN_NO_ERROR;
if (! current_path)
return SVN_NO_ERROR;
if ((! lrb->peg_path) && (log_entry->revision <= lrb->peg_revision))
lrb->peg_path = apr_pstrdup(lrb->pool, current_path);
while (lrb->location_revisions->nelts) {
svn_revnum_t next = APR_ARRAY_IDX(lrb->location_revisions,
lrb->location_revisions->nelts - 1,
svn_revnum_t);
if (log_entry->revision <= next) {
apr_hash_set(lrb->locations,
apr_pmemdup(hash_pool, &next, sizeof(next)),
sizeof(next),
apr_pstrdup(hash_pool, current_path));
apr_array_pop(lrb->location_revisions);
} else
break;
}
SVN_ERR(prev_log_path(&prev_path, NULL, NULL, log_entry->changed_paths,
current_path, lrb->kind, log_entry->revision, pool));
if (! prev_path)
lrb->last_path = NULL;
else if (strcmp(prev_path, current_path) != 0)
lrb->last_path = apr_pstrdup(lrb->pool, prev_path);
return SVN_NO_ERROR;
}
svn_error_t *
svn_ra__locations_from_log(svn_ra_session_t *session,
apr_hash_t **locations_p,
const char *path,
svn_revnum_t peg_revision,
apr_array_header_t *location_revisions,
apr_pool_t *pool) {
apr_hash_t *locations = apr_hash_make(pool);
struct log_receiver_baton lrb = { 0 };
apr_array_header_t *targets;
svn_revnum_t youngest_requested, oldest_requested, youngest, oldest;
svn_node_kind_t kind;
const char *root_url, *url, *rel_path;
SVN_ERR(svn_ra_get_repos_root2(session, &root_url, pool));
SVN_ERR(svn_ra_get_session_url(session, &url, pool));
url = svn_path_join(url, path, pool);
rel_path = svn_path_uri_decode(url + strlen(root_url), pool);
SVN_ERR(svn_ra_check_path(session, path, peg_revision, &kind, pool));
if (kind == svn_node_none)
return svn_error_createf(SVN_ERR_FS_NOT_FOUND, NULL,
_("Path '%s' doesn't exist in revision %ld"),
rel_path, peg_revision);
if (! location_revisions->nelts) {
*locations_p = locations;
return SVN_NO_ERROR;
}
qsort(location_revisions->elts, location_revisions->nelts,
location_revisions->elt_size, compare_revisions);
oldest_requested = APR_ARRAY_IDX(location_revisions, 0, svn_revnum_t);
youngest_requested = APR_ARRAY_IDX(location_revisions,
location_revisions->nelts - 1,
svn_revnum_t);
youngest = peg_revision;
youngest = (oldest_requested > youngest) ? oldest_requested : youngest;
youngest = (youngest_requested > youngest) ? youngest_requested : youngest;
oldest = peg_revision;
oldest = (oldest_requested < oldest) ? oldest_requested : oldest;
oldest = (youngest_requested < oldest) ? youngest_requested : oldest;
lrb.kind = kind;
lrb.last_path = rel_path;
lrb.location_revisions = apr_array_copy(pool, location_revisions);
lrb.peg_revision = peg_revision;
lrb.peg_path = NULL;
lrb.locations = locations;
lrb.pool = pool;
targets = apr_array_make(pool, 1, sizeof(const char *));
APR_ARRAY_PUSH(targets, const char *) = path;
SVN_ERR(svn_ra_get_log2(session, targets, youngest, oldest, 0,
TRUE, FALSE, FALSE,
apr_array_make(pool, 0, sizeof(const char *)),
log_receiver, &lrb, pool));
if (! lrb.peg_path)
lrb.peg_path = lrb.last_path;
if (lrb.last_path) {
int i;
for (i = 0; i < location_revisions->nelts; i++) {
svn_revnum_t rev = APR_ARRAY_IDX(location_revisions, i,
svn_revnum_t);
if (! apr_hash_get(locations, &rev, sizeof(rev)))
apr_hash_set(locations, apr_pmemdup(pool, &rev, sizeof(rev)),
sizeof(rev), apr_pstrdup(pool, lrb.last_path));
}
}
if (! lrb.peg_path)
return svn_error_createf
(APR_EGENERAL, NULL,
_("Unable to find repository location for '%s' in revision %ld"),
rel_path, peg_revision);
if (strcmp(rel_path, lrb.peg_path) != 0)
return svn_error_createf
(SVN_ERR_CLIENT_UNRELATED_RESOURCES, NULL,
_("'%s' in revision %ld is an unrelated object"),
rel_path, youngest);
*locations_p = locations;
return SVN_NO_ERROR;
}
struct gls_log_receiver_baton {
svn_node_kind_t kind;
svn_boolean_t done;
const char *last_path;
svn_revnum_t start_rev;
svn_revnum_t range_end;
svn_location_segment_receiver_t receiver;
void *receiver_baton;
apr_pool_t *pool;
};
static svn_error_t *
maybe_crop_and_send_segment(const char *path,
svn_revnum_t start_rev,
svn_revnum_t range_start,
svn_revnum_t range_end,
svn_location_segment_receiver_t receiver,
void *receiver_baton,
apr_pool_t *pool) {
svn_location_segment_t *segment = apr_pcalloc(pool, sizeof(*segment));
segment->path = path ? ((*path == '/') ? path + 1 : path) : NULL;
segment->range_start = range_start;
segment->range_end = range_end;
if (segment->range_start <= start_rev) {
if (segment->range_end > start_rev)
segment->range_end = start_rev;
return receiver(segment, receiver_baton, pool);
}
return SVN_NO_ERROR;
}
static svn_error_t *
gls_log_receiver(void *baton,
svn_log_entry_t *log_entry,
apr_pool_t *pool) {
struct gls_log_receiver_baton *lrb = baton;
const char *current_path = lrb->last_path;
const char *prev_path;
svn_revnum_t copyfrom_rev;
if (lrb->done)
return SVN_NO_ERROR;
SVN_ERR(prev_log_path(&prev_path, NULL, &copyfrom_rev,
log_entry->changed_paths, current_path,
lrb->kind, log_entry->revision, pool));
if (! prev_path) {
lrb->done = TRUE;
return maybe_crop_and_send_segment(current_path, lrb->start_rev,
log_entry->revision, lrb->range_end,
lrb->receiver, lrb->receiver_baton,
pool);
}
if (SVN_IS_VALID_REVNUM(copyfrom_rev)) {
SVN_ERR(maybe_crop_and_send_segment(current_path, lrb->start_rev,
log_entry->revision, lrb->range_end,
lrb->receiver, lrb->receiver_baton,
pool));
lrb->range_end = log_entry->revision - 1;
if (log_entry->revision - copyfrom_rev > 1) {
SVN_ERR(maybe_crop_and_send_segment(NULL, lrb->start_rev,
copyfrom_rev + 1, lrb->range_end,
lrb->receiver,
lrb->receiver_baton, pool));
lrb->range_end = copyfrom_rev;
}
lrb->last_path = apr_pstrdup(lrb->pool, prev_path);
}
return SVN_NO_ERROR;
}
svn_error_t *
svn_ra__location_segments_from_log(svn_ra_session_t *session,
const char *path,
svn_revnum_t peg_revision,
svn_revnum_t start_rev,
svn_revnum_t end_rev,
svn_location_segment_receiver_t receiver,
void *receiver_baton,
apr_pool_t *pool) {
struct gls_log_receiver_baton lrb = { 0 };
apr_array_header_t *targets;
svn_node_kind_t kind;
svn_revnum_t youngest_rev = SVN_INVALID_REVNUM;
const char *root_url, *url, *rel_path;
SVN_ERR(svn_ra_get_repos_root2(session, &root_url, pool));
SVN_ERR(svn_ra_get_session_url(session, &url, pool));
url = svn_path_join(url, path, pool);
rel_path = svn_path_uri_decode(url + strlen(root_url), pool);
if (! SVN_IS_VALID_REVNUM(peg_revision)) {
SVN_ERR(svn_ra_get_latest_revnum(session, &youngest_rev, pool));
peg_revision = youngest_rev;
}
if (! SVN_IS_VALID_REVNUM(start_rev)) {
if (SVN_IS_VALID_REVNUM(youngest_rev))
start_rev = youngest_rev;
else
SVN_ERR(svn_ra_get_latest_revnum(session, &start_rev, pool));
}
if (! SVN_IS_VALID_REVNUM(end_rev)) {
end_rev = 0;
}
assert((peg_revision >= start_rev) && (start_rev >= end_rev));
SVN_ERR(svn_ra_check_path(session, path, peg_revision, &kind, pool));
if (kind == svn_node_none)
return svn_error_createf(SVN_ERR_FS_NOT_FOUND, NULL,
_("Path '%s' doesn't exist in revision %ld"),
rel_path, start_rev);
lrb.kind = kind;
lrb.last_path = rel_path;
lrb.done = FALSE;
lrb.start_rev = start_rev;
lrb.range_end = start_rev;
lrb.receiver = receiver;
lrb.receiver_baton = receiver_baton;
lrb.pool = pool;
targets = apr_array_make(pool, 1, sizeof(const char *));
APR_ARRAY_PUSH(targets, const char *) = path;
SVN_ERR(svn_ra_get_log2(session, targets, peg_revision, end_rev, 0,
TRUE, FALSE, FALSE,
apr_array_make(pool, 0, sizeof(const char *)),
gls_log_receiver, &lrb, pool));
if (! lrb.done)
SVN_ERR(maybe_crop_and_send_segment(lrb.last_path, start_rev,
end_rev, lrb.range_end,
receiver, receiver_baton, pool));
return SVN_NO_ERROR;
}
struct rev {
svn_revnum_t revision;
const char *path;
apr_hash_t *props;
struct rev *next;
};
struct fr_log_message_baton {
const char *path;
struct rev *eldest;
char action;
svn_revnum_t copyrev;
apr_pool_t *pool;
};
static svn_error_t *
fr_log_message_receiver(void *baton,
svn_log_entry_t *log_entry,
apr_pool_t *pool) {
struct fr_log_message_baton *lmb = baton;
struct rev *rev;
apr_hash_index_t *hi;
rev = apr_palloc(lmb->pool, sizeof(*rev));
rev->revision = log_entry->revision;
rev->path = lmb->path;
rev->next = lmb->eldest;
lmb->eldest = rev;
rev->props = apr_hash_make(lmb->pool);
for (hi = apr_hash_first(pool, log_entry->revprops); hi;
hi = apr_hash_next(hi)) {
void *val;
const void *key;
apr_hash_this(hi, &key, NULL, &val);
apr_hash_set(rev->props, apr_pstrdup(lmb->pool, key), APR_HASH_KEY_STRING,
svn_string_dup(val, lmb->pool));
}
return prev_log_path(&lmb->path, &lmb->action,
&lmb->copyrev, log_entry->changed_paths,
lmb->path, svn_node_file, log_entry->revision,
lmb->pool);
}
svn_error_t *
svn_ra__file_revs_from_log(svn_ra_session_t *ra_session,
const char *path,
svn_revnum_t start,
svn_revnum_t end,
svn_file_rev_handler_t handler,
void *handler_baton,
apr_pool_t *pool) {
svn_node_kind_t kind;
const char *repos_url;
const char *session_url;
const char *tmp;
char *repos_abs_path;
apr_array_header_t *condensed_targets;
struct fr_log_message_baton lmb;
struct rev *rev;
apr_hash_t *last_props;
const char *last_path;
svn_stream_t *last_stream;
apr_pool_t *currpool, *lastpool;
SVN_ERR(svn_ra_get_repos_root2(ra_session, &repos_url, pool));
SVN_ERR(svn_ra_get_session_url(ra_session, &session_url, pool));
tmp = svn_path_is_child(repos_url, session_url, pool);
repos_abs_path = apr_palloc(pool, strlen(tmp) + 1);
repos_abs_path[0] = '/';
memcpy(repos_abs_path + 1, tmp, strlen(tmp));
SVN_ERR(svn_ra_check_path(ra_session, "", end, &kind, pool));
if (kind == svn_node_dir)
return svn_error_createf(SVN_ERR_FS_NOT_FILE, NULL,
_("'%s' is not a file"), repos_abs_path);
condensed_targets = apr_array_make(pool, 1, sizeof(const char *));
APR_ARRAY_PUSH(condensed_targets, const char *) = "";
lmb.path = svn_path_uri_decode(repos_abs_path, pool);
lmb.eldest = NULL;
lmb.pool = pool;
SVN_ERR(svn_ra_get_log2(ra_session,
condensed_targets,
end, start, 0,
TRUE, FALSE, FALSE,
NULL, fr_log_message_receiver, &lmb,
pool));
SVN_ERR(svn_ra_reparent(ra_session, repos_url, pool));
currpool = svn_pool_create(pool);
lastpool = svn_pool_create(pool);
last_props = apr_hash_make(lastpool);
last_path = NULL;
last_stream = svn_stream_empty(lastpool);
for (rev = lmb.eldest; rev; rev = rev->next) {
const char *temp_path;
const char *temp_dir;
apr_pool_t *tmppool;
apr_hash_t *props;
apr_file_t *file;
svn_stream_t *stream;
apr_array_header_t *prop_diffs;
svn_txdelta_stream_t *delta_stream;
svn_txdelta_window_handler_t delta_handler = NULL;
void *delta_baton = NULL;
svn_pool_clear(currpool);
SVN_ERR(svn_io_temp_dir(&temp_dir, currpool));
SVN_ERR(svn_io_open_unique_file2
(&file, &temp_path,
svn_path_join(temp_dir, "tmp", currpool), ".tmp",
svn_io_file_del_on_pool_cleanup, currpool));
stream = svn_stream_from_aprfile(file, currpool);
SVN_ERR(svn_ra_get_file(ra_session, rev->path + 1, rev->revision,
stream, NULL, &props, currpool));
SVN_ERR(svn_stream_close(stream));
SVN_ERR(svn_io_file_close(file, currpool));
SVN_ERR(svn_io_file_open(&file, temp_path, APR_READ, APR_OS_DEFAULT,
currpool));
stream = svn_stream_from_aprfile2(file, FALSE, currpool);
SVN_ERR(svn_prop_diffs(&prop_diffs, props, last_props, lastpool));
SVN_ERR(handler(handler_baton, rev->path, rev->revision, rev->props,
FALSE,
&delta_handler, &delta_baton, prop_diffs, lastpool));
if (delta_handler) {
svn_txdelta(&delta_stream, last_stream, stream, lastpool);
SVN_ERR(svn_txdelta_send_txstream(delta_stream, delta_handler,
delta_baton, lastpool));
}
tmppool = currpool;
currpool = lastpool;
lastpool = tmppool;
svn_stream_close(last_stream);
last_stream = stream;
last_props = props;
}
svn_stream_close(last_stream);
svn_pool_destroy(currpool);
svn_pool_destroy(lastpool);
return svn_ra_reparent(ra_session, session_url, pool);
}
