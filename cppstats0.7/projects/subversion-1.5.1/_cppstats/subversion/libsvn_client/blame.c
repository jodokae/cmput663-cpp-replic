#include <apr_pools.h>
#include "client.h"
#include "svn_client.h"
#include "svn_subst.h"
#include "svn_string.h"
#include "svn_error.h"
#include "svn_diff.h"
#include "svn_pools.h"
#include "svn_path.h"
#include "svn_props.h"
#include "svn_sorts.h"
#include "svn_private_config.h"
#include <assert.h>
struct rev {
svn_revnum_t revision;
const char *author;
const char *date;
const char *path;
};
struct blame {
struct rev *rev;
apr_off_t start;
struct blame *next;
};
struct blame_chain {
struct blame *blame;
struct blame *avail;
struct apr_pool_t *pool;
};
struct diff_baton {
struct blame_chain *chain;
struct rev *rev;
};
struct file_rev_baton {
svn_revnum_t start_rev, end_rev;
const char *target;
svn_client_ctx_t *ctx;
const svn_diff_file_options_t *diff_options;
svn_boolean_t ignore_mime_type;
const char *last_filename;
struct rev *rev;
struct blame_chain *chain;
const char *tmp_path;
apr_pool_t *mainpool;
apr_pool_t *lastpool;
apr_pool_t *currpool;
svn_boolean_t include_merged_revisions;
svn_boolean_t merged_revision;
struct blame_chain *merged_chain;
const char *last_original_filename;
apr_pool_t *filepool;
apr_pool_t *prevfilepool;
};
struct delta_baton {
svn_txdelta_window_handler_t wrapped_handler;
void *wrapped_baton;
struct file_rev_baton *file_rev_baton;
apr_file_t *source_file;
apr_file_t *file;
const char *filename;
};
static struct blame *
blame_create(struct blame_chain *chain,
struct rev *rev,
apr_off_t start) {
struct blame *blame;
if (chain->avail) {
blame = chain->avail;
chain->avail = blame->next;
} else
blame = apr_palloc(chain->pool, sizeof(*blame));
blame->rev = rev;
blame->start = start;
blame->next = NULL;
return blame;
}
static void
blame_destroy(struct blame_chain *chain,
struct blame *blame) {
blame->next = chain->avail;
chain->avail = blame;
}
static struct blame *
blame_find(struct blame *blame, apr_off_t off) {
struct blame *prev = NULL;
while (blame) {
if (blame->start > off) break;
prev = blame;
blame = blame->next;
}
return prev;
}
static void
blame_adjust(struct blame *blame, apr_off_t adjust) {
while (blame) {
blame->start += adjust;
blame = blame->next;
}
}
static svn_error_t *
blame_delete_range(struct blame_chain *chain,
apr_off_t start,
apr_off_t length) {
struct blame *first = blame_find(chain->blame, start);
struct blame *last = blame_find(chain->blame, start + length);
struct blame *tail = last->next;
if (first != last) {
struct blame *walk = first->next;
while (walk != last) {
struct blame *next = walk->next;
blame_destroy(chain, walk);
walk = next;
}
first->next = last;
last->start = start;
if (first->start == start) {
*first = *last;
blame_destroy(chain, last);
last = first;
}
}
if (tail && tail->start == last->start + length) {
*last = *tail;
blame_destroy(chain, tail);
tail = last->next;
}
blame_adjust(tail, -length);
return SVN_NO_ERROR;
}
static svn_error_t *
blame_insert_range(struct blame_chain *chain,
struct rev *rev,
apr_off_t start,
apr_off_t length) {
struct blame *head = chain->blame;
struct blame *point = blame_find(head, start);
struct blame *insert;
if (point->start == start) {
insert = blame_create(chain, point->rev, point->start + length);
point->rev = rev;
insert->next = point->next;
point->next = insert;
} else {
struct blame *middle;
middle = blame_create(chain, rev, start);
insert = blame_create(chain, point->rev, start + length);
middle->next = insert;
insert->next = point->next;
point->next = middle;
}
blame_adjust(insert->next, length);
return SVN_NO_ERROR;
}
static svn_error_t *
output_diff_modified(void *baton,
apr_off_t original_start,
apr_off_t original_length,
apr_off_t modified_start,
apr_off_t modified_length,
apr_off_t latest_start,
apr_off_t latest_length) {
struct diff_baton *db = baton;
if (original_length)
SVN_ERR(blame_delete_range(db->chain, modified_start, original_length));
if (modified_length)
SVN_ERR(blame_insert_range(db->chain, db->rev, modified_start,
modified_length));
return SVN_NO_ERROR;
}
static const svn_diff_output_fns_t output_fns = {
NULL,
output_diff_modified
};
static svn_error_t *
add_file_blame(const char *last_file,
const char *cur_file,
struct blame_chain *chain,
struct rev *rev,
const svn_diff_file_options_t *diff_options,
apr_pool_t *pool) {
if (!last_file) {
assert(chain->blame == NULL);
chain->blame = blame_create(chain, rev, 0);
} else {
svn_diff_t *diff;
struct diff_baton diff_baton;
diff_baton.chain = chain;
diff_baton.rev = rev;
SVN_ERR(svn_diff_file_diff_2(&diff, last_file, cur_file,
diff_options, pool));
SVN_ERR(svn_diff_output(diff, &diff_baton, &output_fns));
}
return SVN_NO_ERROR;
}
static svn_error_t *
window_handler(svn_txdelta_window_t *window, void *baton) {
struct delta_baton *dbaton = baton;
struct file_rev_baton *frb = dbaton->file_rev_baton;
struct blame_chain *chain;
SVN_ERR(dbaton->wrapped_handler(window, dbaton->wrapped_baton));
if (window)
return SVN_NO_ERROR;
if (dbaton->source_file)
SVN_ERR(svn_io_file_close(dbaton->source_file, frb->currpool));
SVN_ERR(svn_io_file_close(dbaton->file, frb->currpool));
if (frb->include_merged_revisions)
chain = frb->merged_chain;
else
chain = frb->chain;
SVN_ERR(add_file_blame(frb->last_filename,
dbaton->filename, chain, frb->rev,
frb->diff_options, frb->currpool));
if (frb->include_merged_revisions && ! frb->merged_revision) {
apr_pool_t *tmppool;
SVN_ERR(add_file_blame(frb->last_original_filename,
dbaton->filename, frb->chain, frb->rev,
frb->diff_options, frb->currpool));
svn_pool_clear(frb->prevfilepool);
tmppool = frb->filepool;
frb->filepool = frb->prevfilepool;
frb->prevfilepool = tmppool;
frb->last_original_filename = apr_pstrdup(frb->filepool,
dbaton->filename);
}
frb->last_filename = dbaton->filename;
{
apr_pool_t *tmp_pool = frb->lastpool;
frb->lastpool = frb->currpool;
frb->currpool = tmp_pool;
}
return SVN_NO_ERROR;
}
static svn_error_t *
check_mimetype(apr_array_header_t *prop_diffs, const char *target,
apr_pool_t *pool) {
int i;
for (i = 0; i < prop_diffs->nelts; ++i) {
const svn_prop_t *prop = &APR_ARRAY_IDX(prop_diffs, i, svn_prop_t);
if (strcmp(prop->name, SVN_PROP_MIME_TYPE) == 0
&& prop->value
&& svn_mime_type_is_binary(prop->value->data))
return svn_error_createf
(SVN_ERR_CLIENT_IS_BINARY_FILE, 0,
_("Cannot calculate blame information for binary file '%s'"),
svn_path_local_style(target, pool));
}
return SVN_NO_ERROR;
}
static svn_error_t *
file_rev_handler(void *baton, const char *path, svn_revnum_t revnum,
apr_hash_t *rev_props,
svn_boolean_t merged_revision,
svn_txdelta_window_handler_t *content_delta_handler,
void **content_delta_baton,
apr_array_header_t *prop_diffs,
apr_pool_t *pool) {
struct file_rev_baton *frb = baton;
svn_stream_t *last_stream;
svn_stream_t *cur_stream;
struct delta_baton *delta_baton;
apr_pool_t *filepool;
svn_pool_clear(frb->currpool);
if (! frb->ignore_mime_type)
SVN_ERR(check_mimetype(prop_diffs, frb->target, frb->currpool));
if (frb->ctx->notify_func2) {
svn_wc_notify_t *notify
= svn_wc_create_notify(path, svn_wc_notify_blame_revision, pool);
notify->kind = svn_node_none;
notify->content_state = notify->prop_state
= svn_wc_notify_state_inapplicable;
notify->lock_state = svn_wc_notify_lock_state_inapplicable;
notify->revision = revnum;
frb->ctx->notify_func2(frb->ctx->notify_baton2, notify, pool);
}
if (frb->ctx->cancel_func)
SVN_ERR(frb->ctx->cancel_func(frb->ctx->cancel_baton));
if (!content_delta_handler)
return SVN_NO_ERROR;
frb->merged_revision = merged_revision;
delta_baton = apr_palloc(frb->currpool, sizeof(*delta_baton));
if (frb->last_filename)
SVN_ERR(svn_io_file_open(&delta_baton->source_file, frb->last_filename,
APR_READ, APR_OS_DEFAULT, frb->currpool));
else
delta_baton->source_file = NULL;
last_stream = svn_stream_from_aprfile(delta_baton->source_file, pool);
if (frb->include_merged_revisions && !frb->merged_revision)
filepool = frb->filepool;
else
filepool = frb->currpool;
SVN_ERR(svn_io_open_unique_file2(&delta_baton->file,
&delta_baton->filename,
frb->tmp_path,
".tmp", svn_io_file_del_on_pool_cleanup,
filepool));
cur_stream = svn_stream_from_aprfile(delta_baton->file, frb->currpool);
svn_txdelta_apply(last_stream, cur_stream, NULL, NULL,
frb->currpool,
&delta_baton->wrapped_handler,
&delta_baton->wrapped_baton);
delta_baton->file_rev_baton = frb;
*content_delta_handler = window_handler;
*content_delta_baton = delta_baton;
frb->rev = apr_palloc(frb->mainpool, sizeof(struct rev));
if (revnum < frb->start_rev) {
assert(frb->last_filename == NULL);
frb->rev->revision = SVN_INVALID_REVNUM;
frb->rev->author = NULL;
frb->rev->date = NULL;
} else {
svn_string_t *str;
assert(revnum <= frb->end_rev);
frb->rev->revision = revnum;
if ((str = apr_hash_get(rev_props, SVN_PROP_REVISION_AUTHOR,
sizeof(SVN_PROP_REVISION_AUTHOR) - 1)))
frb->rev->author = apr_pstrdup(frb->mainpool, str->data);
else
frb->rev->author = NULL;
if ((str = apr_hash_get(rev_props, SVN_PROP_REVISION_DATE,
sizeof(SVN_PROP_REVISION_DATE) - 1)))
frb->rev->date = apr_pstrdup(frb->mainpool, str->data);
else
frb->rev->date = NULL;
}
if (frb->include_merged_revisions)
frb->rev->path = apr_pstrdup(frb->mainpool, path);
return SVN_NO_ERROR;
}
static void
normalize_blames(struct blame_chain *chain,
struct blame_chain *chain_merged,
apr_pool_t *pool) {
struct blame *walk, *walk_merged;
for (walk = chain->blame, walk_merged = chain_merged->blame;
walk->next && walk_merged->next;
walk = walk->next, walk_merged = walk_merged->next) {
assert(walk->start == walk_merged->start);
if (walk->next->start < walk_merged->next->start) {
struct blame *tmp = blame_create(chain_merged, walk_merged->next->rev,
walk->next->start);
tmp->next = walk_merged->next->next;
walk_merged->next = tmp;
}
if (walk->next->start > walk_merged->next->start) {
struct blame *tmp = blame_create(chain, walk->next->rev,
walk_merged->next->start);
tmp->next = walk->next->next;
walk->next = tmp;
}
}
if (walk->next == NULL && walk_merged->next == NULL)
return;
if (walk_merged->next == NULL) {
while (walk->next != NULL) {
struct blame *tmp = blame_create(chain_merged, walk_merged->rev,
walk->next->start);
walk_merged->next = tmp;
walk_merged = walk_merged->next;
walk = walk->next;
}
}
if (walk->next == NULL) {
while (walk_merged->next != NULL) {
struct blame *tmp = blame_create(chain, walk->rev,
walk_merged->next->start);
walk->next = tmp;
walk = walk->next;
walk_merged = walk_merged->next;
}
}
}
svn_error_t *
svn_client_blame4(const char *target,
const svn_opt_revision_t *peg_revision,
const svn_opt_revision_t *start,
const svn_opt_revision_t *end,
const svn_diff_file_options_t *diff_options,
svn_boolean_t ignore_mime_type,
svn_boolean_t include_merged_revisions,
svn_client_blame_receiver2_t receiver,
void *receiver_baton,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
struct file_rev_baton frb;
svn_ra_session_t *ra_session;
const char *url;
svn_revnum_t start_revnum, end_revnum;
struct blame *walk, *walk_merged = NULL;
apr_file_t *file;
apr_pool_t *iterpool;
svn_stream_t *stream;
if (start->kind == svn_opt_revision_unspecified
|| end->kind == svn_opt_revision_unspecified)
return svn_error_create
(SVN_ERR_CLIENT_BAD_REVISION, NULL, NULL);
else if (start->kind == svn_opt_revision_working
|| end->kind == svn_opt_revision_working)
return svn_error_create
(SVN_ERR_UNSUPPORTED_FEATURE, NULL,
_("blame of the WORKING revision is not supported"));
SVN_ERR(svn_client__ra_session_from_path(&ra_session, &end_revnum,
&url, target, NULL,
peg_revision, end,
ctx, pool));
SVN_ERR(svn_client__get_revision_number(&start_revnum, NULL, ra_session,
start, target, pool));
if (end_revnum < start_revnum)
return svn_error_create
(SVN_ERR_CLIENT_BAD_REVISION, NULL,
_("Start revision must precede end revision"));
frb.start_rev = start_revnum;
frb.end_rev = end_revnum;
frb.target = target;
frb.ctx = ctx;
frb.diff_options = diff_options;
frb.ignore_mime_type = ignore_mime_type;
frb.include_merged_revisions = include_merged_revisions;
frb.last_filename = NULL;
frb.last_original_filename = NULL;
frb.chain = apr_palloc(pool, sizeof(*frb.chain));
frb.chain->blame = NULL;
frb.chain->avail = NULL;
frb.chain->pool = pool;
if (include_merged_revisions) {
frb.merged_chain = apr_palloc(pool, sizeof(*frb.merged_chain));
frb.merged_chain->blame = NULL;
frb.merged_chain->avail = NULL;
frb.merged_chain->pool = pool;
}
SVN_ERR(svn_io_temp_dir(&frb.tmp_path, pool));
frb.tmp_path = svn_path_join(frb.tmp_path, "tmp", pool),
frb.mainpool = pool;
frb.lastpool = svn_pool_create(pool);
frb.currpool = svn_pool_create(pool);
if (include_merged_revisions) {
frb.filepool = svn_pool_create(pool);
frb.prevfilepool = svn_pool_create(pool);
}
SVN_ERR(svn_ra_get_file_revs2(ra_session, "",
start_revnum - (start_revnum > 0 ? 1 : 0),
end_revnum, include_merged_revisions,
file_rev_handler, &frb, pool));
assert(frb.last_filename != NULL);
iterpool = svn_pool_create(pool);
SVN_ERR(svn_io_file_open(&file, frb.last_filename, APR_READ | APR_BUFFERED,
APR_OS_DEFAULT, pool));
stream = svn_subst_stream_translated(svn_stream_from_aprfile(file, pool),
"\n", TRUE, NULL, FALSE, pool);
if (include_merged_revisions) {
if (!frb.chain->blame)
frb.chain->blame = blame_create(frb.chain, frb.rev, 0);
normalize_blames(frb.chain, frb.merged_chain, pool);
walk_merged = frb.merged_chain->blame;
}
for (walk = frb.chain->blame; walk; walk = walk->next) {
apr_off_t line_no;
svn_revnum_t merged_rev;
const char *merged_author, *merged_date, *merged_path;
if (walk_merged) {
merged_rev = walk_merged->rev->revision;
merged_author = walk_merged->rev->author;
merged_date = walk_merged->rev->date;
merged_path = walk_merged->rev->path;
} else {
merged_rev = SVN_INVALID_REVNUM;
merged_author = NULL;
merged_date = NULL;
merged_path = NULL;
}
for (line_no = walk->start;
!walk->next || line_no < walk->next->start;
++line_no) {
svn_boolean_t eof;
svn_stringbuf_t *sb;
svn_pool_clear(iterpool);
SVN_ERR(svn_stream_readline(stream, &sb, "\n", &eof, iterpool));
if (ctx->cancel_func)
SVN_ERR(ctx->cancel_func(ctx->cancel_baton));
if (!eof || sb->len)
SVN_ERR(receiver(receiver_baton, line_no, walk->rev->revision,
walk->rev->author, walk->rev->date,
merged_rev, merged_author, merged_date,
merged_path, sb->data, iterpool));
if (eof) break;
}
if (walk_merged)
walk_merged = walk_merged->next;
}
SVN_ERR(svn_stream_close(stream));
SVN_ERR(svn_io_file_close(file, pool));
svn_pool_destroy(frb.lastpool);
svn_pool_destroy(frb.currpool);
if (include_merged_revisions) {
svn_pool_destroy(frb.filepool);
svn_pool_destroy(frb.prevfilepool);
}
svn_pool_destroy(iterpool);
return SVN_NO_ERROR;
}
struct blame_receiver_wrapper_baton {
void *baton;
svn_client_blame_receiver_t receiver;
};
static svn_error_t *
blame_wrapper_receiver(void *baton,
apr_int64_t line_no,
svn_revnum_t revision,
const char *author,
const char *date,
svn_revnum_t merged_revision,
const char *merged_author,
const char *merged_date,
const char *merged_path,
const char *line,
apr_pool_t *pool) {
struct blame_receiver_wrapper_baton *brwb = baton;
if (brwb->receiver)
return brwb->receiver(brwb->baton,
line_no, revision, author, date, line, pool);
return SVN_NO_ERROR;
}
static void
wrap_blame_receiver(svn_client_blame_receiver2_t *receiver2,
void **receiver2_baton,
svn_client_blame_receiver_t receiver,
void *receiver_baton,
apr_pool_t *pool) {
struct blame_receiver_wrapper_baton *brwb = apr_palloc(pool, sizeof(*brwb));
brwb->baton = receiver_baton;
brwb->receiver = receiver;
*receiver2_baton = brwb;
*receiver2 = blame_wrapper_receiver;
}
svn_error_t *
svn_client_blame3(const char *target,
const svn_opt_revision_t *peg_revision,
const svn_opt_revision_t *start,
const svn_opt_revision_t *end,
const svn_diff_file_options_t *diff_options,
svn_boolean_t ignore_mime_type,
svn_client_blame_receiver_t receiver,
void *receiver_baton,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
svn_client_blame_receiver2_t receiver2;
void *receiver2_baton;
wrap_blame_receiver(&receiver2, &receiver2_baton, receiver, receiver_baton,
pool);
return svn_client_blame4(target, peg_revision, start, end, diff_options,
ignore_mime_type, FALSE, receiver2, receiver2_baton,
ctx, pool);
}
struct wrapped_receiver_baton_s {
svn_client_blame_receiver_t orig_receiver;
void *orig_baton;
};
static svn_error_t *
wrapped_receiver(void *baton,
apr_int64_t line_no,
svn_revnum_t revision,
const char *author,
const char *date,
const char *line,
apr_pool_t *pool) {
struct wrapped_receiver_baton_s *b = baton;
svn_stringbuf_t *expanded_line = svn_stringbuf_create(line, pool);
svn_stringbuf_appendbytes(expanded_line, "\r", 1);
return b->orig_receiver(b->orig_baton, line_no, revision, author,
date, expanded_line->data, pool);
}
static void
wrap_pre_blame3_receiver(svn_client_blame_receiver_t *receiver,
void **receiver_baton,
apr_pool_t *pool) {
if (strlen(APR_EOL_STR) > 1) {
struct wrapped_receiver_baton_s *b = apr_palloc(pool,sizeof(*b));
b->orig_receiver = *receiver;
b->orig_baton = *receiver_baton;
*receiver_baton = b;
*receiver = wrapped_receiver;
}
}
svn_error_t *
svn_client_blame2(const char *target,
const svn_opt_revision_t *peg_revision,
const svn_opt_revision_t *start,
const svn_opt_revision_t *end,
svn_client_blame_receiver_t receiver,
void *receiver_baton,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
wrap_pre_blame3_receiver(&receiver, &receiver_baton, pool);
return svn_client_blame3(target, peg_revision, start, end,
svn_diff_file_options_create(pool), FALSE,
receiver, receiver_baton, ctx, pool);
}
svn_error_t *
svn_client_blame(const char *target,
const svn_opt_revision_t *start,
const svn_opt_revision_t *end,
svn_client_blame_receiver_t receiver,
void *receiver_baton,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
wrap_pre_blame3_receiver(&receiver, &receiver_baton, pool);
return svn_client_blame2(target, end, start, end,
receiver, receiver_baton, ctx, pool);
}
