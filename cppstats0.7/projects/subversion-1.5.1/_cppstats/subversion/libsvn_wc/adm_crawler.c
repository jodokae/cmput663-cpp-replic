#include <string.h>
#include <apr_pools.h>
#include <apr_file_io.h>
#include <apr_hash.h>
#include <apr_md5.h>
#include <assert.h>
#include "svn_types.h"
#include "svn_pools.h"
#include "svn_wc.h"
#include "svn_io.h"
#include "svn_md5.h"
#include "svn_base64.h"
#include "svn_delta.h"
#include "svn_path.h"
#include "private/svn_wc_private.h"
#include "wc.h"
#include "adm_files.h"
#include "props.h"
#include "translate.h"
#include "entries.h"
#include "lock.h"
#include "svn_private_config.h"
static svn_error_t *
restore_file(const char *file_path,
svn_wc_adm_access_t *adm_access,
svn_boolean_t use_commit_times,
apr_pool_t *pool) {
const char *tmp_file, *text_base_path;
svn_wc_entry_t newentry;
const char *bname;
svn_boolean_t special;
text_base_path = svn_wc__text_base_path(file_path, FALSE, pool);
bname = svn_path_basename(file_path, pool);
SVN_ERR(svn_wc_translated_file2(&tmp_file,
text_base_path, file_path, adm_access,
SVN_WC_TRANSLATE_FROM_NF
| SVN_WC_TRANSLATE_FORCE_COPY, pool));
SVN_ERR(svn_io_file_rename(tmp_file, file_path, pool));
SVN_ERR(svn_wc__maybe_set_read_only(NULL, file_path, adm_access, pool));
SVN_ERR(svn_wc__maybe_set_executable(NULL, file_path, adm_access, pool));
SVN_ERR(svn_wc_resolved_conflict3(file_path, adm_access, TRUE, FALSE,
svn_depth_empty,
svn_wc_conflict_choose_merged,
NULL, NULL, NULL, NULL, pool));
if (use_commit_times) {
SVN_ERR(svn_wc__get_special(&special, file_path, adm_access, pool));
}
if (use_commit_times && (! special)) {
const svn_wc_entry_t *entry;
SVN_ERR(svn_wc_entry(&entry, file_path, adm_access, FALSE, pool));
assert(entry != NULL);
SVN_ERR(svn_io_set_file_affected_time(entry->cmt_date,
file_path, pool));
newentry.text_time = entry->cmt_date;
} else {
SVN_ERR(svn_io_file_affected_time(&newentry.text_time,
file_path, pool));
}
SVN_ERR(svn_wc__entry_modify(adm_access, bname,
&newentry, SVN_WC__ENTRY_MODIFY_TEXT_TIME,
TRUE , pool));
return SVN_NO_ERROR;
}
static svn_error_t *
report_revisions_and_depths(svn_wc_adm_access_t *adm_access,
const char *dir_path,
svn_revnum_t dir_rev,
const svn_ra_reporter3_t *reporter,
void *report_baton,
svn_wc_notify_func2_t notify_func,
void *notify_baton,
svn_boolean_t restore_files,
svn_depth_t depth,
svn_boolean_t depth_compatibility_trick,
svn_boolean_t report_everything,
svn_boolean_t use_commit_times,
svn_wc_traversal_info_t *traversal_info,
apr_pool_t *pool) {
apr_hash_t *entries, *dirents;
apr_hash_index_t *hi;
apr_pool_t *subpool = svn_pool_create(pool), *iterpool;
const svn_wc_entry_t *dot_entry;
const char *this_url, *this_path, *full_path, *this_full_path;
svn_wc_adm_access_t *dir_access;
svn_wc_notify_t *notify;
full_path = svn_path_join(svn_wc_adm_access_path(adm_access),
dir_path, subpool);
SVN_ERR(svn_wc_adm_retrieve(&dir_access, adm_access, full_path, subpool));
SVN_ERR(svn_wc_entries_read(&entries, dir_access, TRUE, subpool));
SVN_ERR(svn_io_get_dir_filenames(&dirents, full_path, subpool));
dot_entry = apr_hash_get(entries, SVN_WC_ENTRY_THIS_DIR,
APR_HASH_KEY_STRING);
if (traversal_info) {
const svn_string_t *val;
SVN_ERR(svn_wc_prop_get(&val, SVN_PROP_EXTERNALS, full_path, adm_access,
subpool));
if (val) {
apr_pool_t *dup_pool = traversal_info->pool;
const char *dup_path = apr_pstrdup(dup_pool, full_path);
const char *dup_val = apr_pstrmemdup(dup_pool, val->data, val->len);
apr_hash_set(traversal_info->externals_old,
dup_path, APR_HASH_KEY_STRING, dup_val);
apr_hash_set(traversal_info->externals_new,
dup_path, APR_HASH_KEY_STRING, dup_val);
apr_hash_set(traversal_info->depths,
dup_path, APR_HASH_KEY_STRING,
svn_depth_to_word(dot_entry->depth));
}
}
iterpool = svn_pool_create(subpool);
for (hi = apr_hash_first(subpool, entries); hi; hi = apr_hash_next(hi)) {
const void *key;
apr_ssize_t klen;
void *val;
const svn_wc_entry_t *current_entry;
svn_io_dirent_t *dirent;
svn_node_kind_t dirent_kind;
svn_boolean_t missing = FALSE;
svn_pool_clear(iterpool);
apr_hash_this(hi, &key, &klen, &val);
current_entry = val;
if (! strcmp(key, SVN_WC_ENTRY_THIS_DIR))
continue;
this_url = svn_path_url_add_component(dot_entry->url, key, iterpool);
this_path = svn_path_join(dir_path, key, iterpool);
this_full_path = svn_path_join(full_path, key, iterpool);
if (current_entry->deleted || current_entry->absent) {
if (! report_everything)
SVN_ERR(reporter->delete_path(report_baton, this_path, iterpool));
continue;
}
dirent = apr_hash_get(dirents, key, klen);
if (! dirent) {
SVN_ERR(svn_io_check_path(this_full_path, &dirent_kind,
iterpool));
if (dirent_kind == svn_node_none)
missing = TRUE;
}
if (current_entry->schedule == svn_wc_schedule_add)
continue;
if (current_entry->kind == svn_node_file) {
if (missing
&& restore_files
&& (current_entry->schedule != svn_wc_schedule_delete)
&& (current_entry->schedule != svn_wc_schedule_replace)) {
SVN_ERR(restore_file(this_full_path, dir_access,
use_commit_times, iterpool));
if (notify_func != NULL) {
notify = svn_wc_create_notify(this_full_path,
svn_wc_notify_restore,
iterpool);
notify->kind = svn_node_file;
(*notify_func)(notify_baton, notify, iterpool);
}
}
if (report_everything) {
if (strcmp(current_entry->url, this_url) != 0)
SVN_ERR(reporter->link_path(report_baton, this_path,
current_entry->url,
current_entry->revision,
current_entry->depth,
FALSE, current_entry->lock_token,
iterpool));
else
SVN_ERR(reporter->set_path(report_baton, this_path,
current_entry->revision,
current_entry->depth,
FALSE, current_entry->lock_token,
iterpool));
}
else if ((current_entry->schedule != svn_wc_schedule_add)
&& (current_entry->schedule != svn_wc_schedule_replace)
&& (strcmp(current_entry->url, this_url) != 0))
SVN_ERR(reporter->link_path(report_baton,
this_path,
current_entry->url,
current_entry->revision,
current_entry->depth,
FALSE,
current_entry->lock_token,
iterpool));
else if (current_entry->revision != dir_rev
|| current_entry->lock_token
|| dot_entry->depth == svn_depth_empty)
SVN_ERR(reporter->set_path(report_baton,
this_path,
current_entry->revision,
current_entry->depth,
FALSE,
current_entry->lock_token,
iterpool));
}
else if (current_entry->kind == svn_node_dir
&& (depth > svn_depth_files
|| depth == svn_depth_unknown)) {
svn_wc_adm_access_t *subdir_access;
const svn_wc_entry_t *subdir_entry;
svn_boolean_t start_empty;
if (missing) {
if (! report_everything)
SVN_ERR(reporter->delete_path(report_baton, this_path,
iterpool));
continue;
}
if (svn_wc__adm_missing(adm_access, this_full_path))
continue;
SVN_ERR(svn_wc_adm_retrieve(&subdir_access, adm_access,
this_full_path, iterpool));
SVN_ERR(svn_wc_entry(&subdir_entry, this_full_path, subdir_access,
TRUE, iterpool));
start_empty = subdir_entry->incomplete;
if (depth_compatibility_trick
&& subdir_entry->depth <= svn_depth_files
&& depth > subdir_entry->depth) {
start_empty = TRUE;
}
if (report_everything) {
if (strcmp(subdir_entry->url, this_url) != 0)
SVN_ERR(reporter->link_path(report_baton, this_path,
subdir_entry->url,
subdir_entry->revision,
subdir_entry->depth,
start_empty,
subdir_entry->lock_token,
iterpool));
else
SVN_ERR(reporter->set_path(report_baton, this_path,
subdir_entry->revision,
subdir_entry->depth,
start_empty,
subdir_entry->lock_token,
iterpool));
}
else if (strcmp(subdir_entry->url, this_url) != 0)
SVN_ERR(reporter->link_path(report_baton,
this_path,
subdir_entry->url,
subdir_entry->revision,
subdir_entry->depth,
start_empty,
subdir_entry->lock_token,
iterpool));
else if (subdir_entry->revision != dir_rev
|| subdir_entry->lock_token
|| subdir_entry->incomplete
|| dot_entry->depth == svn_depth_empty
|| dot_entry->depth == svn_depth_files
|| (dot_entry->depth == svn_depth_immediates
&& subdir_entry->depth != svn_depth_empty))
SVN_ERR(reporter->set_path(report_baton,
this_path,
subdir_entry->revision,
subdir_entry->depth,
start_empty,
subdir_entry->lock_token,
iterpool));
if (SVN_DEPTH_IS_RECURSIVE(depth))
SVN_ERR(report_revisions_and_depths(adm_access, this_path,
subdir_entry->revision,
reporter, report_baton,
notify_func, notify_baton,
restore_files, depth,
depth_compatibility_trick,
start_empty,
use_commit_times,
traversal_info,
iterpool));
}
}
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
svn_error_t *
svn_wc_crawl_revisions3(const char *path,
svn_wc_adm_access_t *adm_access,
const svn_ra_reporter3_t *reporter,
void *report_baton,
svn_boolean_t restore_files,
svn_depth_t depth,
svn_boolean_t depth_compatibility_trick,
svn_boolean_t use_commit_times,
svn_wc_notify_func2_t notify_func,
void *notify_baton,
svn_wc_traversal_info_t *traversal_info,
apr_pool_t *pool) {
svn_error_t *fserr, *err = SVN_NO_ERROR;
const svn_wc_entry_t *entry;
svn_revnum_t base_rev = SVN_INVALID_REVNUM;
svn_boolean_t missing = FALSE;
const svn_wc_entry_t *parent_entry = NULL;
svn_wc_notify_t *notify;
svn_boolean_t start_empty;
SVN_ERR(svn_wc_entry(&entry, path, adm_access, FALSE, pool));
if ((! entry) || ((entry->schedule == svn_wc_schedule_add)
&& (entry->kind == svn_node_dir))) {
SVN_ERR(svn_wc__entry_versioned(&parent_entry,
svn_path_dirname(path, pool),
adm_access, FALSE, pool));
base_rev = parent_entry->revision;
if (depth == svn_depth_unknown)
depth = svn_depth_infinity;
SVN_ERR(reporter->set_path(report_baton, "", base_rev, depth,
entry ? entry->incomplete : TRUE,
NULL, pool));
SVN_ERR(reporter->delete_path(report_baton, "", pool));
SVN_ERR(reporter->finish_report(report_baton, pool));
return SVN_NO_ERROR;
}
base_rev = entry->revision;
start_empty = entry->incomplete;
if (depth_compatibility_trick
&& entry->depth <= svn_depth_immediates
&& depth > entry->depth) {
start_empty = TRUE;
}
if (base_rev == SVN_INVALID_REVNUM) {
const char *dirname = svn_path_dirname(path, pool);
SVN_ERR(svn_wc__entry_versioned(&parent_entry, dirname, adm_access,
FALSE, pool));
base_rev = parent_entry->revision;
}
SVN_ERR(reporter->set_path(report_baton, "", base_rev, entry->depth,
start_empty, NULL, pool));
if (entry->schedule != svn_wc_schedule_delete) {
apr_finfo_t info;
err = svn_io_stat(&info, path, APR_FINFO_MIN, pool);
if (err) {
if (APR_STATUS_IS_ENOENT(err->apr_err))
missing = TRUE;
svn_error_clear(err);
err = NULL;
}
}
if (entry->kind == svn_node_dir) {
if (missing) {
err = reporter->delete_path(report_baton, "", pool);
if (err)
goto abort_report;
} else if (depth != svn_depth_empty) {
err = report_revisions_and_depths(adm_access,
"",
base_rev,
reporter, report_baton,
notify_func, notify_baton,
restore_files, depth,
depth_compatibility_trick,
start_empty,
use_commit_times,
traversal_info,
pool);
if (err)
goto abort_report;
}
}
else if (entry->kind == svn_node_file) {
const char *pdir, *bname;
if (missing && restore_files) {
err = restore_file(path, adm_access, use_commit_times, pool);
if (err)
goto abort_report;
if (notify_func != NULL) {
notify = svn_wc_create_notify(path, svn_wc_notify_restore,
pool);
notify->kind = svn_node_file;
(*notify_func)(notify_baton, notify, pool);
}
}
svn_path_split(path, &pdir, &bname, pool);
if (! parent_entry) {
err = svn_wc_entry(&parent_entry, pdir, adm_access, FALSE, pool);
if (err)
goto abort_report;
}
if (parent_entry
&& parent_entry->url
&& entry->url
&& strcmp(entry->url,
svn_path_url_add_component(parent_entry->url,
bname, pool))) {
err = reporter->link_path(report_baton,
"",
entry->url,
entry->revision,
entry->depth,
FALSE,
entry->lock_token,
pool);
if (err)
goto abort_report;
} else if (entry->revision != base_rev || entry->lock_token) {
err = reporter->set_path(report_baton, "", base_rev, entry->depth,
FALSE,
entry->lock_token, pool);
if (err)
goto abort_report;
}
}
return reporter->finish_report(report_baton, pool);
abort_report:
if ((fserr = reporter->abort_report(report_baton, pool))) {
fserr = svn_error_quick_wrap(fserr, _("Error aborting report"));
svn_error_compose(err, fserr);
}
return err;
}
struct wrap_3to2_report_baton {
const svn_ra_reporter2_t *reporter;
void *baton;
};
static svn_error_t *wrap_3to2_set_path(void *report_baton,
const char *path,
svn_revnum_t revision,
svn_depth_t depth,
svn_boolean_t start_empty,
const char *lock_token,
apr_pool_t *pool) {
struct wrap_3to2_report_baton *wrb = report_baton;
return wrb->reporter->set_path(wrb->baton, path, revision, start_empty,
lock_token, pool);
}
static svn_error_t *wrap_3to2_delete_path(void *report_baton,
const char *path,
apr_pool_t *pool) {
struct wrap_3to2_report_baton *wrb = report_baton;
return wrb->reporter->delete_path(wrb->baton, path, pool);
}
static svn_error_t *wrap_3to2_link_path(void *report_baton,
const char *path,
const char *url,
svn_revnum_t revision,
svn_depth_t depth,
svn_boolean_t start_empty,
const char *lock_token,
apr_pool_t *pool) {
struct wrap_3to2_report_baton *wrb = report_baton;
return wrb->reporter->link_path(wrb->baton, path, url, revision,
start_empty, lock_token, pool);
}
static svn_error_t *wrap_3to2_finish_report(void *report_baton,
apr_pool_t *pool) {
struct wrap_3to2_report_baton *wrb = report_baton;
return wrb->reporter->finish_report(wrb->baton, pool);
}
static svn_error_t *wrap_3to2_abort_report(void *report_baton,
apr_pool_t *pool) {
struct wrap_3to2_report_baton *wrb = report_baton;
return wrb->reporter->abort_report(wrb->baton, pool);
}
static const svn_ra_reporter3_t wrap_3to2_reporter = {
wrap_3to2_set_path,
wrap_3to2_delete_path,
wrap_3to2_link_path,
wrap_3to2_finish_report,
wrap_3to2_abort_report
};
svn_error_t *
svn_wc_crawl_revisions2(const char *path,
svn_wc_adm_access_t *adm_access,
const svn_ra_reporter2_t *reporter,
void *report_baton,
svn_boolean_t restore_files,
svn_boolean_t recurse,
svn_boolean_t use_commit_times,
svn_wc_notify_func2_t notify_func,
void *notify_baton,
svn_wc_traversal_info_t *traversal_info,
apr_pool_t *pool) {
struct wrap_3to2_report_baton wrb;
wrb.reporter = reporter;
wrb.baton = report_baton;
return svn_wc_crawl_revisions3(path,
adm_access,
&wrap_3to2_reporter, &wrb,
restore_files,
SVN_DEPTH_INFINITY_OR_FILES(recurse),
FALSE,
use_commit_times,
notify_func,
notify_baton,
traversal_info,
pool);
}
struct wrap_2to1_report_baton {
const svn_ra_reporter_t *reporter;
void *baton;
};
static svn_error_t *wrap_2to1_set_path(void *report_baton,
const char *path,
svn_revnum_t revision,
svn_boolean_t start_empty,
const char *lock_token,
apr_pool_t *pool) {
struct wrap_2to1_report_baton *wrb = report_baton;
return wrb->reporter->set_path(wrb->baton, path, revision, start_empty,
pool);
}
static svn_error_t *wrap_2to1_delete_path(void *report_baton,
const char *path,
apr_pool_t *pool) {
struct wrap_2to1_report_baton *wrb = report_baton;
return wrb->reporter->delete_path(wrb->baton, path, pool);
}
static svn_error_t *wrap_2to1_link_path(void *report_baton,
const char *path,
const char *url,
svn_revnum_t revision,
svn_boolean_t start_empty,
const char *lock_token,
apr_pool_t *pool) {
struct wrap_2to1_report_baton *wrb = report_baton;
return wrb->reporter->link_path(wrb->baton, path, url, revision,
start_empty, pool);
}
static svn_error_t *wrap_2to1_finish_report(void *report_baton,
apr_pool_t *pool) {
struct wrap_2to1_report_baton *wrb = report_baton;
return wrb->reporter->finish_report(wrb->baton, pool);
}
static svn_error_t *wrap_2to1_abort_report(void *report_baton,
apr_pool_t *pool) {
struct wrap_2to1_report_baton *wrb = report_baton;
return wrb->reporter->abort_report(wrb->baton, pool);
}
static const svn_ra_reporter2_t wrap_2to1_reporter = {
wrap_2to1_set_path,
wrap_2to1_delete_path,
wrap_2to1_link_path,
wrap_2to1_finish_report,
wrap_2to1_abort_report
};
svn_error_t *
svn_wc_crawl_revisions(const char *path,
svn_wc_adm_access_t *adm_access,
const svn_ra_reporter_t *reporter,
void *report_baton,
svn_boolean_t restore_files,
svn_boolean_t recurse,
svn_boolean_t use_commit_times,
svn_wc_notify_func_t notify_func,
void *notify_baton,
svn_wc_traversal_info_t *traversal_info,
apr_pool_t *pool) {
struct wrap_2to1_report_baton wrb;
svn_wc__compat_notify_baton_t nb;
wrb.reporter = reporter;
wrb.baton = report_baton;
nb.func = notify_func;
nb.baton = notify_baton;
return svn_wc_crawl_revisions2(path, adm_access, &wrap_2to1_reporter, &wrb,
restore_files, recurse, use_commit_times,
svn_wc__compat_call_notify_func, &nb,
traversal_info,
pool);
}
struct copying_stream_baton {
svn_stream_t *source;
svn_stream_t *target;
};
static svn_error_t *
read_handler_copy(void *baton, char *buffer, apr_size_t *len) {
struct copying_stream_baton *btn = baton;
SVN_ERR(svn_stream_read(btn->source, buffer, len));
return svn_stream_write(btn->target, buffer, len);
}
static svn_error_t *
close_handler_copy(void *baton) {
struct copying_stream_baton *btn = baton;
SVN_ERR(svn_stream_close(btn->target));
return svn_stream_close(btn->source);
}
static svn_stream_t *
copying_stream(svn_stream_t *source,
svn_stream_t *target,
apr_pool_t *pool) {
struct copying_stream_baton *baton;
svn_stream_t *stream;
baton = apr_palloc(pool, sizeof (*baton));
baton->source = source;
baton->target = target;
stream = svn_stream_create(baton, pool);
svn_stream_set_read(stream, read_handler_copy);
svn_stream_set_close(stream, close_handler_copy);
return stream;
}
svn_error_t *
svn_wc_transmit_text_deltas2(const char **tempfile,
unsigned char digest[],
const char *path,
svn_wc_adm_access_t *adm_access,
svn_boolean_t fulltext,
const svn_delta_editor_t *editor,
void *file_baton,
apr_pool_t *pool) {
const char *tmp_base;
svn_txdelta_window_handler_t handler;
void *wh_baton;
svn_txdelta_stream_t *txdelta_stream;
apr_file_t *basefile = NULL;
apr_file_t *tempbasefile;
const char *base_digest_hex = NULL;
const unsigned char *base_digest = NULL;
const unsigned char *local_digest = NULL;
svn_error_t *err;
const svn_wc_entry_t *ent;
svn_stream_t *base_stream;
svn_stream_t *local_stream;
apr_time_t wf_time;
SVN_ERR(svn_wc_entry(&ent, path, adm_access, FALSE, pool));
SVN_ERR(svn_io_file_affected_time(&wf_time, path, pool));
SVN_ERR(svn_wc_translated_stream(&local_stream, path, path,
adm_access, SVN_WC_TRANSLATE_TO_NF, pool));
tmp_base = svn_wc__text_base_path(path, TRUE, pool);
if (tempfile) {
*tempfile = tmp_base;
SVN_ERR(svn_io_file_open(&tempbasefile, tmp_base,
APR_WRITE | APR_CREATE, APR_OS_DEFAULT, pool));
local_stream
= copying_stream(local_stream,
svn_stream_from_aprfile2(tempbasefile, FALSE, pool),
pool);
}
if (! fulltext) {
if (! ent->checksum) {
unsigned char tmp_digest[APR_MD5_DIGESTSIZE];
const char *tb = svn_wc__text_base_path (path, FALSE, pool);
SVN_ERR (svn_io_file_checksum (tmp_digest, tb, pool));
base_digest_hex = svn_md5_digest_to_cstring_display(tmp_digest, pool);
} else
base_digest_hex = ent->checksum;
SVN_ERR(svn_wc__open_text_base(&basefile, path, APR_READ, pool));
}
SVN_ERR(editor->apply_textdelta
(file_baton, base_digest_hex, pool, &handler, &wh_baton));
base_stream = svn_stream_from_aprfile2(basefile, TRUE, pool);
if (! fulltext)
base_stream
= svn_stream_checksummed(base_stream, &base_digest, NULL, TRUE, pool);
svn_txdelta(&txdelta_stream, base_stream, local_stream, pool);
err = svn_txdelta_send_txstream(txdelta_stream, handler, wh_baton, pool);
if (err) {
svn_error_clear(svn_stream_close(base_stream));
svn_error_clear(svn_stream_close(local_stream));
} else {
SVN_ERR(svn_stream_close(base_stream));
SVN_ERR(svn_stream_close(local_stream));
}
if (! fulltext && ent->checksum && base_digest) {
base_digest_hex = svn_md5_digest_to_cstring_display(base_digest, pool);
if (strcmp(base_digest_hex, ent->checksum) != 0) {
svn_error_clear(err);
svn_error_clear(svn_io_remove_file(tmp_base, pool));
return svn_error_createf
(SVN_ERR_WC_CORRUPT_TEXT_BASE, NULL,
_("Checksum mismatch for '%s'; "
"expected: '%s', actual: '%s'"),
svn_path_local_style(svn_wc__text_base_path(path, FALSE, pool),
pool),
ent->checksum, base_digest_hex);
}
}
SVN_ERR_W(err, apr_psprintf(pool,
_("While preparing '%s' for commit"),
svn_path_local_style(path, pool)));
if (basefile)
SVN_ERR(svn_wc__close_text_base(basefile, path, 0, pool));
local_digest = svn_txdelta_md5_digest(txdelta_stream);
if (digest)
memcpy(digest, local_digest, APR_MD5_DIGESTSIZE);
return editor->close_file
(file_baton, svn_md5_digest_to_cstring(local_digest, pool), pool);
}
svn_error_t *
svn_wc_transmit_text_deltas(const char *path,
svn_wc_adm_access_t *adm_access,
svn_boolean_t fulltext,
const svn_delta_editor_t *editor,
void *file_baton,
const char **tempfile,
apr_pool_t *pool) {
return svn_wc_transmit_text_deltas2(tempfile, NULL, path, adm_access,
fulltext, editor, file_baton, pool);
}
svn_error_t *
svn_wc_transmit_prop_deltas(const char *path,
svn_wc_adm_access_t *adm_access,
const svn_wc_entry_t *entry,
const svn_delta_editor_t *editor,
void *baton,
const char **tempfile,
apr_pool_t *pool) {
int i;
apr_array_header_t *propmods;
if (tempfile)
*tempfile = NULL;
SVN_ERR(svn_wc_get_prop_diffs(&propmods, NULL,
path, adm_access, pool));
for (i = 0; i < propmods->nelts; i++) {
const svn_prop_t *p = &APR_ARRAY_IDX(propmods, i, svn_prop_t);
if (entry->kind == svn_node_file)
SVN_ERR(editor->change_file_prop(baton, p->name, p->value, pool));
else
SVN_ERR(editor->change_dir_prop(baton, p->name, p->value, pool));
}
return SVN_NO_ERROR;
}