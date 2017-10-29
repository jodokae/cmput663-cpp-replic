#include <string.h>
#include <apr_pools.h>
#include <apr_strings.h>
#include "svn_wc.h"
#include "svn_error.h"
#include "svn_string.h"
#include "svn_xml.h"
#include "svn_pools.h"
#include "svn_io.h"
#include "svn_path.h"
#include "svn_time.h"
#include "svn_iter.h"
#include "wc.h"
#include "log.h"
#include "props.h"
#include "adm_files.h"
#include "entries.h"
#include "lock.h"
#include "translate.h"
#include "questions.h"
#include "svn_private_config.h"
#define SVN_WC__LOG_MODIFY_ENTRY "modify-entry"
#define SVN_WC__LOG_DELETE_LOCK "delete-lock"
#define SVN_WC__LOG_DELETE_CHANGELIST "delete-changelist"
#define SVN_WC__LOG_DELETE_ENTRY "delete-entry"
#define SVN_WC__LOG_MV "mv"
#define SVN_WC__LOG_CP "cp"
#define SVN_WC__LOG_CP_AND_TRANSLATE "cp-and-translate"
#define SVN_WC__LOG_CP_AND_DETRANSLATE "cp-and-detranslate"
#define SVN_WC__LOG_RM "rm"
#define SVN_WC__LOG_APPEND "append"
#define SVN_WC__LOG_READONLY "readonly"
#define SVN_WC__LOG_MAYBE_READONLY "maybe-readonly"
#define SVN_WC__LOG_MAYBE_EXECUTABLE "maybe-executable"
#define SVN_WC__LOG_SET_TIMESTAMP "set-timestamp"
#define SVN_WC__LOG_COMMITTED "committed"
#define SVN_WC__LOG_MODIFY_WCPROP "modify-wcprop"
#define SVN_WC__LOG_MERGE "merge"
#define SVN_WC__LOG_UPGRADE_FORMAT "upgrade-format"
#define SVN_WC__LOG_ATTR_NAME "name"
#define SVN_WC__LOG_ATTR_DEST "dest"
#define SVN_WC__LOG_ATTR_REVISION "revision"
#define SVN_WC__LOG_ATTR_TIMESTAMP "timestamp"
#define SVN_WC__LOG_ATTR_PROPNAME "propname"
#define SVN_WC__LOG_ATTR_PROPVAL "propval"
#define SVN_WC__LOG_ATTR_ARG_1 "arg1"
#define SVN_WC__LOG_ATTR_ARG_2 "arg2"
#define SVN_WC__LOG_ATTR_ARG_3 "arg3"
#define SVN_WC__LOG_ATTR_ARG_4 "arg4"
#define SVN_WC__LOG_ATTR_ARG_5 "arg5"
#define SVN_WC__LOG_ATTR_FORMAT "format"
#define SVN_WC__LOG_ATTR_FORCE "force"
struct log_runner {
apr_pool_t *pool;
svn_xml_parser_t *parser;
svn_boolean_t entries_modified;
svn_boolean_t wcprops_modified;
svn_boolean_t rerun;
svn_wc_adm_access_t *adm_access;
const char *diff3_cmd;
int count;
};
static svn_error_t *
run_log_from_memory(svn_wc_adm_access_t *adm_access,
const char *buf,
apr_size_t buf_len,
svn_boolean_t rerun,
const char *diff3_cmd,
apr_pool_t *pool);
enum svn_wc__xfer_action {
svn_wc__xfer_cp,
svn_wc__xfer_mv,
svn_wc__xfer_append,
svn_wc__xfer_cp_and_translate,
svn_wc__xfer_cp_and_detranslate
};
static svn_error_t *
file_xfer_under_path(svn_wc_adm_access_t *adm_access,
const char *name,
const char *dest,
const char *versioned,
enum svn_wc__xfer_action action,
svn_boolean_t special_only,
svn_boolean_t rerun,
apr_pool_t *pool) {
svn_error_t *err;
const char *full_from_path, *full_dest_path, *full_versioned_path;
full_from_path = svn_path_join(svn_wc_adm_access_path(adm_access), name,
pool);
full_dest_path = svn_path_join(svn_wc_adm_access_path(adm_access), dest,
pool);
if (versioned)
full_versioned_path = svn_path_join(svn_wc_adm_access_path(adm_access),
versioned, pool);
else
full_versioned_path = NULL;
switch (action) {
case svn_wc__xfer_append:
err = svn_io_append_file(full_from_path, full_dest_path, pool);
if (err) {
if (! rerun || ! APR_STATUS_IS_ENOENT(err->apr_err))
return err;
svn_error_clear(err);
}
break;
case svn_wc__xfer_cp:
return svn_io_copy_file(full_from_path, full_dest_path, FALSE, pool);
case svn_wc__xfer_cp_and_translate: {
svn_subst_eol_style_t style;
const char *eol;
apr_hash_t *keywords;
svn_boolean_t special;
if (! full_versioned_path)
full_versioned_path = full_dest_path;
err = svn_wc__get_eol_style(&style, &eol, full_versioned_path,
adm_access, pool);
if (! err)
err = svn_wc__get_keywords(&keywords, full_versioned_path,
adm_access, NULL, pool);
if (! err)
err = svn_wc__get_special(&special, full_versioned_path, adm_access,
pool);
if (! err)
err = svn_subst_copy_and_translate3
(full_from_path, full_dest_path,
eol, TRUE,
keywords, TRUE,
special,
pool);
if (err) {
if (! rerun || ! APR_STATUS_IS_ENOENT(err->apr_err))
return err;
svn_error_clear(err);
}
SVN_ERR(svn_wc__maybe_set_read_only(NULL, full_dest_path,
adm_access, pool));
SVN_ERR(svn_wc__maybe_set_executable(NULL, full_dest_path,
adm_access, pool));
return SVN_NO_ERROR;
}
case svn_wc__xfer_cp_and_detranslate: {
const char *tmp_file;
SVN_ERR(svn_wc_translated_file2
(&tmp_file,
full_from_path,
versioned ? full_versioned_path : full_from_path, adm_access,
SVN_WC_TRANSLATE_TO_NF
| SVN_WC_TRANSLATE_FORCE_COPY,
pool));
SVN_ERR(svn_io_file_rename(tmp_file, full_dest_path, pool));
return SVN_NO_ERROR;
}
case svn_wc__xfer_mv:
err = svn_io_file_rename(full_from_path,
full_dest_path, pool);
if (err) {
if (! rerun || ! APR_STATUS_IS_ENOENT(err->apr_err))
return svn_error_quick_wrap(err, _("Can't move source to dest"));
svn_error_clear(err);
}
}
return SVN_NO_ERROR;
}
static svn_error_t *
install_committed_file(svn_boolean_t *overwrote_working,
svn_wc_adm_access_t *adm_access,
const char *name,
svn_boolean_t remove_executable,
svn_boolean_t remove_read_only,
apr_pool_t *pool) {
const char *filepath;
const char *tmp_text_base;
svn_node_kind_t kind;
svn_boolean_t same, did_set;
const char *tmp_wfile;
svn_boolean_t special;
*overwrote_working = FALSE;
filepath = svn_path_join(svn_wc_adm_access_path(adm_access), name, pool);
tmp_text_base = svn_wc__text_base_path(filepath, 1, pool);
SVN_ERR(svn_io_check_path(tmp_text_base, &kind, pool));
{
const char *tmp = (kind == svn_node_file) ? tmp_text_base : filepath;
SVN_ERR(svn_wc_translated_file2(&tmp_wfile,
tmp,
filepath, adm_access,
SVN_WC_TRANSLATE_FROM_NF,
pool));
SVN_ERR(svn_wc__get_special(&special, filepath, adm_access, pool));
if (! special && tmp != tmp_wfile)
SVN_ERR(svn_io_files_contents_same_p(&same, tmp_wfile,
filepath, pool));
else
same = TRUE;
}
if (! same) {
SVN_ERR(svn_io_file_rename(tmp_wfile, filepath, pool));
*overwrote_working = TRUE;
}
if (remove_executable) {
if (same)
SVN_ERR(svn_io_set_file_executable(filepath,
FALSE,
FALSE, pool));
*overwrote_working = TRUE;
} else {
SVN_ERR(svn_wc__maybe_set_executable(&did_set, filepath,
adm_access, pool));
if (did_set)
*overwrote_working = TRUE;
}
if (remove_read_only) {
if (same)
SVN_ERR(svn_io_set_file_read_write(filepath, FALSE, pool));
*overwrote_working = TRUE;
} else {
SVN_ERR(svn_wc__maybe_set_read_only(&did_set, filepath,
adm_access, pool));
if (did_set)
*overwrote_working = TRUE;
}
if (kind == svn_node_file)
SVN_ERR(svn_wc__sync_text_base(filepath, pool));
return SVN_NO_ERROR;
}
static apr_status_t
pick_error_code(struct log_runner *loggy) {
if (loggy->count <= 1)
return SVN_ERR_WC_BAD_ADM_LOG_START;
else
return SVN_ERR_WC_BAD_ADM_LOG;
}
#define SIGNAL_ERROR(loggy, err) svn_xml_signal_bailout (svn_error_createf(pick_error_code(loggy), err, _("In directory '%s'"), svn_path_local_style(svn_wc_adm_access_path (loggy->adm_access), loggy->pool)), loggy->parser)
static svn_error_t *
log_do_merge(struct log_runner *loggy,
const char *name,
const char **atts) {
const char *left, *right;
const char *left_label, *right_label, *target_label;
enum svn_wc_merge_outcome_t merge_outcome;
svn_stringbuf_t *log_accum = svn_stringbuf_create("", loggy->pool);
svn_error_t *err;
left = svn_xml_get_attr_value(SVN_WC__LOG_ATTR_ARG_1, atts);
if (! left)
return svn_error_createf(pick_error_code(loggy), NULL,
_("Missing 'left' attribute in '%s'"),
svn_path_local_style
(svn_wc_adm_access_path(loggy->adm_access),
loggy->pool));
right = svn_xml_get_attr_value(SVN_WC__LOG_ATTR_ARG_2, atts);
if (! right)
return svn_error_createf(pick_error_code(loggy), NULL,
_("Missing 'right' attribute in '%s'"),
svn_path_local_style
(svn_wc_adm_access_path(loggy->adm_access),
loggy->pool));
left_label = svn_xml_get_attr_value(SVN_WC__LOG_ATTR_ARG_3, atts);
right_label = svn_xml_get_attr_value(SVN_WC__LOG_ATTR_ARG_4, atts);
target_label = svn_xml_get_attr_value(SVN_WC__LOG_ATTR_ARG_5, atts);
left = svn_path_join(svn_wc_adm_access_path(loggy->adm_access), left,
loggy->pool);
right = svn_path_join(svn_wc_adm_access_path(loggy->adm_access), right,
loggy->pool);
name = svn_path_join(svn_wc_adm_access_path(loggy->adm_access), name,
loggy->pool);
err = svn_wc__merge_internal(&log_accum, &merge_outcome,
left, right, name, NULL, loggy->adm_access,
left_label, right_label, target_label,
FALSE, loggy->diff3_cmd, NULL, NULL,
NULL, NULL, loggy->pool);
if (err && loggy->rerun && APR_STATUS_IS_ENOENT(err->apr_err)) {
svn_error_clear(err);
return SVN_NO_ERROR;
} else if (err)
return err;
err = run_log_from_memory(loggy->adm_access,
log_accum->data, log_accum->len,
loggy->rerun, loggy->diff3_cmd, loggy->pool);
if (err && loggy->rerun && APR_STATUS_IS_ENOENT(err->apr_err)) {
svn_error_clear(err);
return SVN_NO_ERROR;
} else
return err;
}
static svn_error_t *
log_do_file_xfer(struct log_runner *loggy,
const char *name,
enum svn_wc__xfer_action action,
const char **atts) {
svn_error_t *err;
const char *dest = NULL;
const char *versioned;
svn_boolean_t special_only;
dest = svn_xml_get_attr_value(SVN_WC__LOG_ATTR_DEST, atts);
special_only =
svn_xml_get_attr_value(SVN_WC__LOG_ATTR_ARG_1, atts) != NULL;
versioned =
svn_xml_get_attr_value(SVN_WC__LOG_ATTR_ARG_2, atts);
if (! dest)
return svn_error_createf(pick_error_code(loggy), NULL,
_("Missing 'dest' attribute in '%s'"),
svn_path_local_style
(svn_wc_adm_access_path(loggy->adm_access),
loggy->pool));
err = file_xfer_under_path(loggy->adm_access, name, dest, versioned,
action, special_only, loggy->rerun, loggy->pool);
if (err)
SIGNAL_ERROR(loggy, err);
return SVN_NO_ERROR;
}
static svn_error_t *
log_do_file_readonly(struct log_runner *loggy,
const char *name) {
svn_error_t *err;
const char *full_path
= svn_path_join(svn_wc_adm_access_path(loggy->adm_access), name,
loggy->pool);
err = svn_io_set_file_read_only(full_path, FALSE, loggy->pool);
if (err && loggy->rerun && APR_STATUS_IS_ENOENT(err->apr_err)) {
svn_error_clear(err);
return SVN_NO_ERROR;
} else
return err;
}
static svn_error_t *
log_do_file_maybe_executable(struct log_runner *loggy,
const char *name) {
const char *full_path
= svn_path_join(svn_wc_adm_access_path(loggy->adm_access), name,
loggy->pool);
SVN_ERR(svn_wc__maybe_set_executable(NULL, full_path, loggy->adm_access,
loggy->pool));
return SVN_NO_ERROR;
}
static svn_error_t *
log_do_file_maybe_readonly(struct log_runner *loggy,
const char *name) {
const char *full_path
= svn_path_join(svn_wc_adm_access_path(loggy->adm_access), name,
loggy->pool);
SVN_ERR(svn_wc__maybe_set_read_only(NULL, full_path, loggy->adm_access,
loggy->pool));
return SVN_NO_ERROR;
}
static svn_error_t *
log_do_file_timestamp(struct log_runner *loggy,
const char *name,
const char **atts) {
apr_time_t timestamp;
svn_node_kind_t kind;
const char *full_path
= svn_path_join(svn_wc_adm_access_path(loggy->adm_access), name,
loggy->pool);
const char *timestamp_string
= svn_xml_get_attr_value(SVN_WC__LOG_ATTR_TIMESTAMP, atts);
svn_boolean_t is_special;
if (! timestamp_string)
return svn_error_createf(pick_error_code(loggy), NULL,
_("Missing 'timestamp' attribute in '%s'"),
svn_path_local_style
(svn_wc_adm_access_path(loggy->adm_access),
loggy->pool));
SVN_ERR(svn_io_check_special_path(full_path, &kind, &is_special,
loggy->pool));
if (! is_special) {
SVN_ERR(svn_time_from_cstring(&timestamp, timestamp_string,
loggy->pool));
SVN_ERR(svn_io_set_file_affected_time(timestamp, full_path,
loggy->pool));
}
return SVN_NO_ERROR;
}
static svn_error_t *
log_do_rm(struct log_runner *loggy, const char *name) {
const char *full_path
= svn_path_join(svn_wc_adm_access_path(loggy->adm_access),
name, loggy->pool);
svn_error_t *err =
svn_io_remove_file(full_path, loggy->pool);
if (err && APR_STATUS_IS_ENOENT(err->apr_err)) {
svn_error_clear(err);
return SVN_NO_ERROR;
} else
return err;
}
static svn_error_t *
log_do_modify_entry(struct log_runner *loggy,
const char *name,
const char **atts) {
svn_error_t *err;
apr_hash_t *ah = svn_xml_make_att_hash(atts, loggy->pool);
const char *tfile;
svn_wc_entry_t *entry;
apr_uint64_t modify_flags;
const char *valuestr;
if (loggy->rerun) {
const svn_wc_entry_t *existing;
const char *path
= svn_path_join(svn_wc_adm_access_path(loggy->adm_access), name,
loggy->pool);
SVN_ERR(svn_wc_entry(&existing, path, loggy->adm_access, TRUE,
loggy->pool));
if (! existing)
return SVN_NO_ERROR;
}
SVN_ERR(svn_wc__atts_to_entry(&entry, &modify_flags, ah, loggy->pool));
tfile = svn_path_join(svn_wc_adm_access_path(loggy->adm_access),
strcmp(name, SVN_WC_ENTRY_THIS_DIR) ? name : "",
loggy->pool);
valuestr = apr_hash_get(ah, SVN_WC__ENTRY_ATTR_TEXT_TIME,
APR_HASH_KEY_STRING);
if ((modify_flags & SVN_WC__ENTRY_MODIFY_TEXT_TIME)
&& (! strcmp(valuestr, SVN_WC__TIMESTAMP_WC))) {
apr_time_t text_time;
err = svn_io_file_affected_time(&text_time, tfile, loggy->pool);
if (err)
return svn_error_createf
(pick_error_code(loggy), err,
_("Error getting 'affected time' on '%s'"),
svn_path_local_style(tfile, loggy->pool));
entry->text_time = text_time;
}
valuestr = apr_hash_get(ah, SVN_WC__ENTRY_ATTR_PROP_TIME,
APR_HASH_KEY_STRING);
if ((modify_flags & SVN_WC__ENTRY_MODIFY_PROP_TIME)
&& (! strcmp(valuestr, SVN_WC__TIMESTAMP_WC))) {
apr_time_t prop_time;
SVN_ERR(svn_wc__props_last_modified(&prop_time,
tfile, svn_wc__props_working,
loggy->adm_access, loggy->pool));
entry->prop_time = prop_time;
}
valuestr = apr_hash_get(ah, SVN_WC__ENTRY_ATTR_WORKING_SIZE,
APR_HASH_KEY_STRING);
if ((modify_flags & SVN_WC__ENTRY_MODIFY_WORKING_SIZE)
&& (! strcmp(valuestr, SVN_WC__WORKING_SIZE_WC))) {
apr_finfo_t finfo;
const svn_wc_entry_t *tfile_entry;
err = svn_wc_entry(&tfile_entry, tfile, loggy->adm_access,
FALSE, loggy->pool);
if (err)
SIGNAL_ERROR(loggy, err);
if (! tfile_entry)
return SVN_NO_ERROR;
err = svn_io_stat(&finfo, tfile, APR_FINFO_MIN | APR_FINFO_LINK,
loggy->pool);
if (err && APR_STATUS_IS_ENOENT(err->apr_err)) {
svn_error_clear(err);
finfo.size = 0;
} else if (err)
return svn_error_createf
(pick_error_code(loggy), NULL,
_("Error getting file size on '%s'"),
svn_path_local_style(tfile, loggy->pool));
entry->working_size = finfo.size;
}
valuestr = apr_hash_get(ah, SVN_WC__LOG_ATTR_FORCE,
APR_HASH_KEY_STRING);
if (valuestr && strcmp(valuestr, "true") == 0)
modify_flags |= SVN_WC__ENTRY_MODIFY_FORCE;
err = svn_wc__entry_modify(loggy->adm_access, name,
entry, modify_flags, FALSE, loggy->pool);
if (err)
return svn_error_createf(pick_error_code(loggy), err,
_("Error modifying entry for '%s'"), name);
loggy->entries_modified = TRUE;
return SVN_NO_ERROR;
}
static svn_error_t *
log_do_delete_lock(struct log_runner *loggy,
const char *name) {
svn_error_t *err;
svn_wc_entry_t entry;
entry.lock_token = entry.lock_comment = entry.lock_owner = NULL;
entry.lock_creation_date = 0;
err = svn_wc__entry_modify(loggy->adm_access, name,
&entry,
SVN_WC__ENTRY_MODIFY_LOCK_TOKEN
| SVN_WC__ENTRY_MODIFY_LOCK_OWNER
| SVN_WC__ENTRY_MODIFY_LOCK_COMMENT
| SVN_WC__ENTRY_MODIFY_LOCK_CREATION_DATE,
FALSE, loggy->pool);
if (err)
return svn_error_createf(pick_error_code(loggy), err,
_("Error removing lock from entry for '%s'"),
name);
loggy->entries_modified = TRUE;
return SVN_NO_ERROR;
}
static svn_error_t *
log_do_delete_changelist(struct log_runner *loggy,
const char *name) {
svn_error_t *err;
svn_wc_entry_t entry;
entry.changelist = NULL;
err = svn_wc__entry_modify(loggy->adm_access, name,
&entry,
SVN_WC__ENTRY_MODIFY_CHANGELIST,
FALSE, loggy->pool);
if (err)
return svn_error_createf(pick_error_code(loggy), err,
_("Error removing changelist from entry '%s'"),
name);
loggy->entries_modified = TRUE;
return SVN_NO_ERROR;
}
static svn_error_t *
log_do_delete_entry(struct log_runner *loggy, const char *name) {
svn_wc_adm_access_t *adm_access;
const svn_wc_entry_t *entry;
svn_error_t *err = SVN_NO_ERROR;
const char *full_path
= svn_path_join(svn_wc_adm_access_path(loggy->adm_access), name,
loggy->pool);
SVN_ERR(svn_wc_adm_probe_retrieve(&adm_access, loggy->adm_access, full_path,
loggy->pool));
SVN_ERR(svn_wc_entry(&entry, full_path, adm_access, FALSE, loggy->pool));
if (! entry)
return SVN_NO_ERROR;
if (entry->kind == svn_node_dir) {
svn_wc_adm_access_t *ignored;
err = svn_wc_adm_retrieve(&ignored, adm_access, full_path, loggy->pool);
if (err) {
if (err->apr_err == SVN_ERR_WC_NOT_LOCKED) {
apr_hash_t *entries;
svn_error_clear(err);
err = SVN_NO_ERROR;
if (entry->schedule != svn_wc_schedule_add) {
SVN_ERR(svn_wc_entries_read(&entries, loggy->adm_access,
TRUE, loggy->pool));
svn_wc__entry_remove(entries, name);
SVN_ERR(svn_wc__entries_write(entries, loggy->adm_access,
loggy->pool));
}
} else {
return err;
}
} else {
SVN_ERR(svn_wc__adm_extend_lock_to_tree(adm_access, loggy->pool));
err = svn_wc_remove_from_revision_control(adm_access,
SVN_WC_ENTRY_THIS_DIR,
TRUE,
FALSE,
NULL, NULL,
loggy->pool);
}
} else if (entry->kind == svn_node_file) {
err = svn_wc_remove_from_revision_control(loggy->adm_access, name,
TRUE,
FALSE,
NULL, NULL,
loggy->pool);
}
if (err && err->apr_err == SVN_ERR_WC_LEFT_LOCAL_MOD) {
svn_error_clear(err);
return SVN_NO_ERROR;
} else {
return err;
}
}
static svn_error_t *
remove_deleted_entry(void *baton, const void *key,
apr_ssize_t klen, void *val, apr_pool_t *pool) {
struct log_runner *loggy = baton;
const char *base_name;
const char *pdir;
const svn_wc_entry_t *cur_entry = val;
svn_wc_adm_access_t *entry_access;
if (cur_entry->schedule != svn_wc_schedule_delete)
return SVN_NO_ERROR;
base_name = NULL;
if (cur_entry->kind == svn_node_file) {
pdir = svn_wc_adm_access_path(loggy->adm_access);
base_name = apr_pstrdup(pool, key);
entry_access = loggy->adm_access;
} else if (cur_entry->kind == svn_node_dir) {
pdir = svn_path_join(svn_wc_adm_access_path(loggy->adm_access),
key, pool);
base_name = SVN_WC_ENTRY_THIS_DIR;
SVN_ERR(svn_wc_adm_retrieve(&entry_access, loggy->adm_access,
pdir, pool));
}
if (base_name)
SVN_ERR(svn_wc_remove_from_revision_control
(entry_access, base_name, FALSE, FALSE,
NULL, NULL, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
log_do_committed(struct log_runner *loggy,
const char *name,
const char **atts) {
svn_error_t *err;
apr_pool_t *pool = loggy->pool;
int is_this_dir = (strcmp(name, SVN_WC_ENTRY_THIS_DIR) == 0);
const char *rev = svn_xml_get_attr_value(SVN_WC__LOG_ATTR_REVISION, atts);
svn_boolean_t wc_root, overwrote_working = FALSE, remove_executable = FALSE;
svn_boolean_t set_read_write = FALSE;
const char *full_path;
const char *pdir, *base_name;
apr_hash_t *entries;
const svn_wc_entry_t *orig_entry;
svn_wc_entry_t *entry;
apr_time_t text_time = 0;
svn_wc_adm_access_t *adm_access;
apr_finfo_t finfo;
svn_boolean_t prop_mods;
if (! is_this_dir)
full_path = svn_path_join(svn_wc_adm_access_path(loggy->adm_access),
name, pool);
else
full_path = apr_pstrdup(pool, svn_wc_adm_access_path(loggy->adm_access));
if (! rev)
return svn_error_createf(pick_error_code(loggy), NULL,
_("Missing 'revision' attribute for '%s'"),
name);
SVN_ERR(svn_wc_adm_probe_retrieve(&adm_access, loggy->adm_access, full_path,
pool));
SVN_ERR(svn_wc_entry(&orig_entry, full_path, adm_access, TRUE, pool));
if (loggy->rerun && (! orig_entry
|| (orig_entry->schedule == svn_wc_schedule_normal
&& orig_entry->deleted)))
return SVN_NO_ERROR;
if ((! orig_entry)
|| ((! is_this_dir) && (orig_entry->kind != svn_node_file)))
return svn_error_createf
(pick_error_code(loggy), NULL,
_("Log command for directory '%s' is mislocated"), name);
entry = svn_wc_entry_dup(orig_entry, pool);
if (entry->schedule == svn_wc_schedule_delete) {
svn_revnum_t new_rev = SVN_STR_TO_REV(rev);
if (is_this_dir) {
svn_wc_entry_t tmpentry;
tmpentry.revision = new_rev;
tmpentry.kind = svn_node_dir;
SVN_ERR(svn_wc__entry_modify
(loggy->adm_access, NULL, &tmpentry,
SVN_WC__ENTRY_MODIFY_REVISION | SVN_WC__ENTRY_MODIFY_KIND,
FALSE, pool));
loggy->entries_modified = TRUE;
err = svn_wc__make_killme(loggy->adm_access, entry->keep_local,
pool);
if (err) {
if (loggy->rerun && APR_STATUS_IS_EEXIST(err->apr_err))
svn_error_clear(err);
else
return err;
}
return SVN_NO_ERROR;
}
else {
const svn_wc_entry_t *parentry;
svn_wc_entry_t tmp_entry;
SVN_ERR(svn_wc_remove_from_revision_control(loggy->adm_access,
name, FALSE, FALSE,
NULL, NULL,
pool));
SVN_ERR(svn_wc_entry(&parentry,
svn_wc_adm_access_path(loggy->adm_access),
loggy->adm_access,
TRUE, pool));
if (new_rev > parentry->revision) {
tmp_entry.kind = svn_node_file;
tmp_entry.deleted = TRUE;
tmp_entry.revision = new_rev;
SVN_ERR(svn_wc__entry_modify
(loggy->adm_access, name, &tmp_entry,
SVN_WC__ENTRY_MODIFY_REVISION
| SVN_WC__ENTRY_MODIFY_KIND
| SVN_WC__ENTRY_MODIFY_DELETED,
FALSE, pool));
loggy->entries_modified = TRUE;
}
return SVN_NO_ERROR;
}
}
if ((entry->schedule == svn_wc_schedule_replace) && is_this_dir) {
SVN_ERR(svn_wc_entries_read(&entries, loggy->adm_access, TRUE, pool));
SVN_ERR(svn_iter_apr_hash(NULL, entries,
remove_deleted_entry, loggy, pool));
}
SVN_ERR(svn_wc__has_prop_mods(&prop_mods,
full_path, loggy->adm_access, pool));
if (prop_mods) {
if (entry->kind == svn_node_file) {
int i;
apr_array_header_t *propchanges;
SVN_ERR(svn_wc_get_prop_diffs(&propchanges, NULL,
full_path, loggy->adm_access, pool));
for (i = 0; i < propchanges->nelts; i++) {
svn_prop_t *propchange
= &APR_ARRAY_IDX(propchanges, i, svn_prop_t);
if ((! strcmp(propchange->name, SVN_PROP_EXECUTABLE))
&& (propchange->value == NULL))
remove_executable = TRUE;
else if ((! strcmp(propchange->name, SVN_PROP_NEEDS_LOCK))
&& (propchange->value == NULL))
set_read_write = TRUE;
}
}
SVN_ERR(svn_wc__working_props_committed(full_path, loggy->adm_access,
FALSE, pool));
}
if (entry->kind == svn_node_file) {
if ((err = install_committed_file
(&overwrote_working, loggy->adm_access, name,
remove_executable, set_read_write, pool)))
return svn_error_createf
(pick_error_code(loggy), err,
_("Error replacing text-base of '%s'"), name);
if ((err = svn_io_stat(&finfo, full_path,
APR_FINFO_MIN | APR_FINFO_LINK, pool)))
return svn_error_createf(pick_error_code(loggy), err,
_("Error getting 'affected time' of '%s'"),
svn_path_local_style(full_path, pool));
if (overwrote_working)
text_time = finfo.mtime;
else {
const char *basef;
svn_boolean_t modified = FALSE;
apr_finfo_t basef_finfo;
basef = svn_wc__text_base_path(full_path, 0, pool);
err = svn_io_stat(&basef_finfo, basef, APR_FINFO_MIN | APR_FINFO_LINK,
pool);
if (err)
return svn_error_createf
(pick_error_code(loggy), err,
_("Error getting 'affected time' for '%s'"),
svn_path_local_style(basef, pool));
else {
modified = finfo.size != basef_finfo.size;
if (finfo.mtime != basef_finfo.mtime && ! modified) {
err = svn_wc__versioned_file_modcheck(&modified, full_path,
loggy->adm_access,
basef, FALSE, pool);
if (err)
return svn_error_createf
(pick_error_code(loggy), err,
_("Error comparing '%s' and '%s'"),
svn_path_local_style(full_path, pool),
svn_path_local_style(basef, pool));
}
text_time = modified ? basef_finfo.mtime : finfo.mtime;
}
}
} else
finfo.size = 0;
entry->revision = SVN_STR_TO_REV(rev);
entry->kind = is_this_dir ? svn_node_dir : svn_node_file;
entry->schedule = svn_wc_schedule_normal;
entry->copied = FALSE;
entry->deleted = FALSE;
entry->text_time = text_time;
entry->conflict_old = NULL;
entry->conflict_new = NULL;
entry->conflict_wrk = NULL;
entry->prejfile = NULL;
entry->copyfrom_url = NULL;
entry->copyfrom_rev = SVN_INVALID_REVNUM;
entry->has_prop_mods = FALSE;
entry->working_size = finfo.size;
if ((err = svn_wc__entry_modify(loggy->adm_access, name, entry,
(SVN_WC__ENTRY_MODIFY_REVISION
| SVN_WC__ENTRY_MODIFY_SCHEDULE
| SVN_WC__ENTRY_MODIFY_COPIED
| SVN_WC__ENTRY_MODIFY_DELETED
| SVN_WC__ENTRY_MODIFY_COPYFROM_URL
| SVN_WC__ENTRY_MODIFY_COPYFROM_REV
| SVN_WC__ENTRY_MODIFY_CONFLICT_OLD
| SVN_WC__ENTRY_MODIFY_CONFLICT_NEW
| SVN_WC__ENTRY_MODIFY_CONFLICT_WRK
| SVN_WC__ENTRY_MODIFY_PREJFILE
| (text_time
? SVN_WC__ENTRY_MODIFY_TEXT_TIME
: 0)
| SVN_WC__ENTRY_MODIFY_HAS_PROP_MODS
| SVN_WC__ENTRY_MODIFY_WORKING_SIZE
| SVN_WC__ENTRY_MODIFY_FORCE),
FALSE, pool)))
return svn_error_createf
(pick_error_code(loggy), err,
_("Error modifying entry of '%s'"), name);
loggy->entries_modified = TRUE;
if (! is_this_dir)
return SVN_NO_ERROR;
SVN_ERR(svn_wc_is_wc_root(&wc_root,
svn_wc_adm_access_path(loggy->adm_access),
loggy->adm_access,
pool));
if (wc_root)
return SVN_NO_ERROR;
{
svn_wc_adm_access_t *paccess;
svn_boolean_t unassociated = FALSE;
svn_path_split(svn_wc_adm_access_path(loggy->adm_access), &pdir,
&base_name, pool);
err = svn_wc_adm_retrieve(&paccess, loggy->adm_access, pdir, pool);
if (err && (err->apr_err == SVN_ERR_WC_NOT_LOCKED)) {
svn_error_clear(err);
SVN_ERR(svn_wc_adm_open3(&paccess, NULL, pdir, TRUE, 0,
NULL, NULL, pool));
unassociated = TRUE;
} else if (err)
return err;
SVN_ERR(svn_wc_entries_read(&entries, paccess, FALSE, pool));
if (apr_hash_get(entries, base_name, APR_HASH_KEY_STRING)) {
if ((err = svn_wc__entry_modify(paccess, base_name, entry,
(SVN_WC__ENTRY_MODIFY_SCHEDULE
| SVN_WC__ENTRY_MODIFY_COPIED
| SVN_WC__ENTRY_MODIFY_DELETED
| SVN_WC__ENTRY_MODIFY_FORCE),
TRUE, pool)))
return svn_error_createf(pick_error_code(loggy), err,
_("Error modifying entry of '%s'"), name);
}
if (unassociated)
SVN_ERR(svn_wc_adm_close(paccess));
}
return SVN_NO_ERROR;
}
static svn_error_t *
log_do_modify_wcprop(struct log_runner *loggy,
const char *name,
const char **atts) {
svn_string_t value;
const char *propname, *propval, *path;
if (strcmp(name, SVN_WC_ENTRY_THIS_DIR) == 0)
path = svn_wc_adm_access_path(loggy->adm_access);
else
path = svn_path_join(svn_wc_adm_access_path(loggy->adm_access),
name, loggy->pool);
propname = svn_xml_get_attr_value(SVN_WC__LOG_ATTR_PROPNAME, atts);
propval = svn_xml_get_attr_value(SVN_WC__LOG_ATTR_PROPVAL, atts);
if (propval) {
value.data = propval;
value.len = strlen(propval);
}
SVN_ERR(svn_wc__wcprop_set(propname, propval ? &value : NULL,
path, loggy->adm_access, FALSE, loggy->pool));
loggy->wcprops_modified = TRUE;
return SVN_NO_ERROR;
}
static svn_error_t *
log_do_upgrade_format(struct log_runner *loggy,
const char **atts) {
const char *fmtstr = svn_xml_get_attr_value(SVN_WC__LOG_ATTR_FORMAT, atts);
int fmt;
const char *path = svn_wc__adm_path(svn_wc_adm_access_path(loggy->adm_access),
FALSE, loggy->pool,
SVN_WC__ADM_FORMAT, NULL);
if (! fmtstr || (fmt = atoi(fmtstr)) == 0)
return svn_error_create(pick_error_code(loggy), NULL,
_("Invalid 'format' attribute"));
SVN_ERR(svn_io_write_version_file(path, fmt, loggy->pool));
loggy->entries_modified = TRUE;
svn_wc__adm_set_wc_format(loggy->adm_access, fmt);
return SVN_NO_ERROR;
}
static void
start_handler(void *userData, const char *eltname, const char **atts) {
svn_error_t *err = SVN_NO_ERROR;
struct log_runner *loggy = userData;
const char *name = svn_xml_get_attr_value(SVN_WC__LOG_ATTR_NAME, atts);
svn_pool_clear(loggy->pool);
if (strcmp(eltname, "wc-log") == 0)
return;
else if (! name && strcmp(eltname, SVN_WC__LOG_UPGRADE_FORMAT) != 0) {
SIGNAL_ERROR
(loggy, svn_error_createf
(pick_error_code(loggy), NULL,
_("Log entry missing 'name' attribute (entry '%s' "
"for directory '%s')"),
eltname,
svn_path_local_style(svn_wc_adm_access_path(loggy->adm_access),
loggy->pool)));
return;
}
loggy->count += 1;
if (strcmp(eltname, SVN_WC__LOG_MODIFY_ENTRY) == 0) {
err = log_do_modify_entry(loggy, name, atts);
} else if (strcmp(eltname, SVN_WC__LOG_DELETE_LOCK) == 0) {
err = log_do_delete_lock(loggy, name);
} else if (strcmp(eltname, SVN_WC__LOG_DELETE_CHANGELIST) == 0) {
err = log_do_delete_changelist(loggy, name);
} else if (strcmp(eltname, SVN_WC__LOG_DELETE_ENTRY) == 0) {
err = log_do_delete_entry(loggy, name);
} else if (strcmp(eltname, SVN_WC__LOG_COMMITTED) == 0) {
err = log_do_committed(loggy, name, atts);
} else if (strcmp(eltname, SVN_WC__LOG_MODIFY_WCPROP) == 0) {
err = log_do_modify_wcprop(loggy, name, atts);
} else if (strcmp(eltname, SVN_WC__LOG_RM) == 0) {
err = log_do_rm(loggy, name);
} else if (strcmp(eltname, SVN_WC__LOG_MERGE) == 0) {
err = log_do_merge(loggy, name, atts);
} else if (strcmp(eltname, SVN_WC__LOG_MV) == 0) {
err = log_do_file_xfer(loggy, name, svn_wc__xfer_mv, atts);
} else if (strcmp(eltname, SVN_WC__LOG_CP) == 0) {
err = log_do_file_xfer(loggy, name, svn_wc__xfer_cp, atts);
} else if (strcmp(eltname, SVN_WC__LOG_CP_AND_TRANSLATE) == 0) {
err = log_do_file_xfer(loggy, name,svn_wc__xfer_cp_and_translate, atts);
} else if (strcmp(eltname, SVN_WC__LOG_CP_AND_DETRANSLATE) == 0) {
err = log_do_file_xfer(loggy, name,svn_wc__xfer_cp_and_detranslate, atts);
} else if (strcmp(eltname, SVN_WC__LOG_APPEND) == 0) {
err = log_do_file_xfer(loggy, name, svn_wc__xfer_append, atts);
} else if (strcmp(eltname, SVN_WC__LOG_READONLY) == 0) {
err = log_do_file_readonly(loggy, name);
} else if (strcmp(eltname, SVN_WC__LOG_MAYBE_READONLY) == 0) {
err = log_do_file_maybe_readonly(loggy, name);
} else if (strcmp(eltname, SVN_WC__LOG_MAYBE_EXECUTABLE) == 0) {
err = log_do_file_maybe_executable(loggy, name);
} else if (strcmp(eltname, SVN_WC__LOG_SET_TIMESTAMP) == 0) {
err = log_do_file_timestamp(loggy, name, atts);
} else if (strcmp(eltname, SVN_WC__LOG_UPGRADE_FORMAT) == 0) {
err = log_do_upgrade_format(loggy, atts);
} else {
SIGNAL_ERROR
(loggy, svn_error_createf
(pick_error_code(loggy), NULL,
_("Unrecognized logfile element '%s' in '%s'"),
eltname,
svn_path_local_style(svn_wc_adm_access_path(loggy->adm_access),
loggy->pool)));
return;
}
if (err)
SIGNAL_ERROR
(loggy, svn_error_createf
(pick_error_code(loggy), err,
_("Error processing command '%s' in '%s'"),
eltname,
svn_path_local_style(svn_wc_adm_access_path(loggy->adm_access),
loggy->pool)));
return;
}
static svn_error_t *
handle_killme(svn_wc_adm_access_t *adm_access,
svn_boolean_t adm_only,
svn_cancel_func_t cancel_func,
void *cancel_baton,
apr_pool_t *pool) {
const svn_wc_entry_t *thisdir_entry, *parent_entry;
svn_wc_entry_t tmp_entry;
svn_error_t *err;
SVN_ERR(svn_wc_entry(&thisdir_entry,
svn_wc_adm_access_path(adm_access), adm_access,
FALSE, pool));
err = svn_wc_remove_from_revision_control(adm_access,
SVN_WC_ENTRY_THIS_DIR,
!adm_only,
FALSE,
cancel_func, cancel_baton,
pool);
if (err && err->apr_err != SVN_ERR_WC_LEFT_LOCAL_MOD)
return err;
svn_error_clear(err);
{
const char *parent, *bname;
svn_wc_adm_access_t *parent_access;
svn_path_split(svn_wc_adm_access_path(adm_access), &parent, &bname, pool);
SVN_ERR(svn_wc_adm_retrieve(&parent_access, adm_access, parent, pool));
SVN_ERR(svn_wc_entry(&parent_entry, parent, parent_access, FALSE, pool));
if (thisdir_entry->revision > parent_entry->revision) {
tmp_entry.kind = svn_node_dir;
tmp_entry.deleted = TRUE;
tmp_entry.revision = thisdir_entry->revision;
SVN_ERR(svn_wc__entry_modify(parent_access, bname, &tmp_entry,
SVN_WC__ENTRY_MODIFY_REVISION
| SVN_WC__ENTRY_MODIFY_KIND
| SVN_WC__ENTRY_MODIFY_DELETED,
TRUE, pool));
}
}
return SVN_NO_ERROR;
}
const char *
svn_wc__logfile_path(int log_number,
apr_pool_t *pool) {
return apr_psprintf(pool, SVN_WC__ADM_LOG "%s",
(log_number == 0) ? ""
: apr_psprintf(pool, ".%d", log_number));
}
static svn_error_t *
run_log_from_memory(svn_wc_adm_access_t *adm_access,
const char *buf,
apr_size_t buf_len,
svn_boolean_t rerun,
const char *diff3_cmd,
apr_pool_t *pool) {
struct log_runner *loggy;
svn_xml_parser_t *parser;
const char *log_start
= "<wc-log xmlns=\"http://subversion.tigris.org/xmlns\">\n";
const char *log_end
= "</wc-log>\n";
loggy = apr_pcalloc(pool, sizeof(*loggy));
loggy->adm_access = adm_access;
loggy->pool = svn_pool_create(pool);
loggy->parser = svn_xml_make_parser(loggy, start_handler,
NULL, NULL, pool);
loggy->entries_modified = FALSE;
loggy->wcprops_modified = FALSE;
loggy->rerun = rerun;
loggy->diff3_cmd = diff3_cmd;
loggy->count = 0;
parser = loggy->parser;
SVN_ERR(svn_xml_parse(parser, log_start, strlen(log_start), 0));
SVN_ERR(svn_xml_parse(parser, buf, buf_len, 0));
SVN_ERR(svn_xml_parse(parser, log_end, strlen(log_end), 1));
return SVN_NO_ERROR;
}
static svn_error_t *
run_log(svn_wc_adm_access_t *adm_access,
svn_boolean_t rerun,
const char *diff3_cmd,
apr_pool_t *pool) {
svn_error_t *err, *err2;
svn_xml_parser_t *parser;
struct log_runner *loggy = apr_pcalloc(pool, sizeof(*loggy));
char *buf = apr_palloc(pool, SVN__STREAM_CHUNK_SIZE);
apr_size_t buf_len;
apr_file_t *f = NULL;
const char *logfile_path;
int log_number;
apr_pool_t *iterpool = svn_pool_create(pool);
svn_boolean_t killme, kill_adm_only;
const char *log_start
= "<wc-log xmlns=\"http://subversion.tigris.org/xmlns\">\n";
const char *log_end
= "</wc-log>\n";
#if defined(RERUN_LOG_FILES)
int rerun_counter = 2;
rerun:
#endif
parser = svn_xml_make_parser(loggy, start_handler, NULL, NULL, pool);
loggy->adm_access = adm_access;
loggy->pool = svn_pool_create(pool);
loggy->parser = parser;
loggy->entries_modified = FALSE;
loggy->wcprops_modified = FALSE;
loggy->rerun = rerun;
loggy->diff3_cmd = diff3_cmd;
loggy->count = 0;
SVN_ERR(svn_xml_parse(parser, log_start, strlen(log_start), 0));
for (log_number = 0; ; log_number++) {
svn_pool_clear(iterpool);
logfile_path = svn_wc__logfile_path(log_number, iterpool);
err = svn_wc__open_adm_file(&f, svn_wc_adm_access_path(adm_access),
logfile_path, APR_READ, iterpool);
if (err) {
if (APR_STATUS_IS_ENOENT(err->apr_err)) {
svn_error_clear(err);
break;
} else {
SVN_ERR_W(err, _("Couldn't open log"));
}
}
do {
buf_len = SVN__STREAM_CHUNK_SIZE;
err = svn_io_file_read(f, buf, &buf_len, iterpool);
if (err && !APR_STATUS_IS_EOF(err->apr_err))
return svn_error_createf
(err->apr_err, err,
_("Error reading administrative log file in '%s'"),
svn_path_local_style(svn_wc_adm_access_path(adm_access),
iterpool));
err2 = svn_xml_parse(parser, buf, buf_len, 0);
if (err2) {
svn_error_clear(err);
SVN_ERR(err2);
}
} while (! err);
svn_error_clear(err);
SVN_ERR(svn_io_file_close(f, iterpool));
}
SVN_ERR(svn_xml_parse(parser, log_end, strlen(log_end), 1));
svn_xml_free_parser(parser);
#if defined(RERUN_LOG_FILES)
rerun = TRUE;
if (--rerun_counter)
goto rerun;
#endif
if (loggy->entries_modified == TRUE) {
apr_hash_t *entries;
SVN_ERR(svn_wc_entries_read(&entries, loggy->adm_access, TRUE, pool));
SVN_ERR(svn_wc__entries_write(entries, loggy->adm_access, pool));
}
if (loggy->wcprops_modified)
SVN_ERR(svn_wc__props_flush(svn_wc_adm_access_path(adm_access),
svn_wc__props_wcprop, loggy->adm_access, pool));
SVN_ERR(svn_wc__check_killme(adm_access, &killme, &kill_adm_only, pool));
if (killme) {
SVN_ERR(handle_killme(adm_access, kill_adm_only, NULL, NULL, pool));
} else {
for (log_number--; log_number >= 0; log_number--) {
svn_pool_clear(iterpool);
logfile_path = svn_wc__logfile_path(log_number, iterpool);
SVN_ERR(svn_wc__remove_adm_file(svn_wc_adm_access_path(adm_access),
iterpool, logfile_path, NULL));
}
}
svn_pool_destroy(iterpool);
return SVN_NO_ERROR;
}
svn_error_t *
svn_wc__run_log(svn_wc_adm_access_t *adm_access,
const char *diff3_cmd,
apr_pool_t *pool) {
return run_log(adm_access, FALSE, diff3_cmd, pool);
}
svn_error_t *
svn_wc__rerun_log(svn_wc_adm_access_t *adm_access,
const char *diff3_cmd,
apr_pool_t *pool) {
return run_log(adm_access, TRUE, diff3_cmd, pool);
}
static svn_error_t *
loggy_move_copy_internal(svn_stringbuf_t **log_accum,
svn_boolean_t *dst_modified,
const char *move_copy_op,
svn_boolean_t special_only,
svn_wc_adm_access_t *adm_access,
const char *src_path, const char *dst_path,
svn_boolean_t remove_dst_if_no_src,
apr_pool_t *pool) {
svn_node_kind_t kind;
const char *full_src = svn_path_join(svn_wc_adm_access_path(adm_access),
src_path, pool);
const char *full_dst = svn_path_join(svn_wc_adm_access_path(adm_access),
dst_path, pool);
SVN_ERR(svn_io_check_path(full_src, &kind, pool));
if (dst_modified)
*dst_modified = FALSE;
if (kind != svn_node_none) {
svn_xml_make_open_tag(log_accum, pool,
svn_xml_self_closing,
move_copy_op,
SVN_WC__LOG_ATTR_NAME,
src_path,
SVN_WC__LOG_ATTR_DEST,
dst_path,
SVN_WC__LOG_ATTR_ARG_1,
special_only ? "true" : NULL,
NULL);
if (dst_modified)
*dst_modified = TRUE;
}
else if (kind == svn_node_none && remove_dst_if_no_src) {
SVN_ERR(svn_wc__loggy_remove(log_accum, adm_access, full_dst, pool));
if (dst_modified)
*dst_modified = TRUE;
}
return SVN_NO_ERROR;
}
static const char *
loggy_path(const char *path,
svn_wc_adm_access_t *adm_access) {
const char *adm_path = svn_wc_adm_access_path(adm_access);
const char *local_path = svn_path_is_child(adm_path, path, NULL);
if (! local_path && strcmp(path, adm_path) == 0)
local_path = SVN_WC_ENTRY_THIS_DIR;
return local_path;
}
svn_error_t *
svn_wc__loggy_append(svn_stringbuf_t **log_accum,
svn_wc_adm_access_t *adm_access,
const char *src, const char *dst,
apr_pool_t *pool) {
svn_xml_make_open_tag(log_accum,
pool,
svn_xml_self_closing,
SVN_WC__LOG_APPEND,
SVN_WC__LOG_ATTR_NAME,
loggy_path(src, adm_access),
SVN_WC__LOG_ATTR_DEST,
loggy_path(dst, adm_access),
NULL);
return SVN_NO_ERROR;
}
svn_error_t *
svn_wc__loggy_committed(svn_stringbuf_t **log_accum,
svn_wc_adm_access_t *adm_access,
const char *path, svn_revnum_t revnum,
apr_pool_t *pool) {
svn_xml_make_open_tag(log_accum, pool, svn_xml_self_closing,
SVN_WC__LOG_COMMITTED,
SVN_WC__LOG_ATTR_NAME, loggy_path(path, adm_access),
SVN_WC__LOG_ATTR_REVISION,
apr_psprintf(pool, "%ld", revnum),
NULL);
return SVN_NO_ERROR;
}
svn_error_t *
svn_wc__loggy_copy(svn_stringbuf_t **log_accum,
svn_boolean_t *dst_modified,
svn_wc_adm_access_t *adm_access,
svn_wc__copy_t copy_type,
const char *src_path, const char *dst_path,
svn_boolean_t remove_dst_if_no_src,
apr_pool_t *pool) {
static const char *copy_op[] = {
SVN_WC__LOG_CP,
SVN_WC__LOG_CP_AND_TRANSLATE,
SVN_WC__LOG_CP_AND_TRANSLATE,
SVN_WC__LOG_CP_AND_DETRANSLATE
};
return loggy_move_copy_internal
(log_accum, dst_modified,
copy_op[copy_type], copy_type == svn_wc__copy_translate_special_only,
adm_access,
loggy_path(src_path, adm_access),
loggy_path(dst_path, adm_access), remove_dst_if_no_src, pool);
}
svn_error_t *
svn_wc__loggy_translated_file(svn_stringbuf_t **log_accum,
svn_wc_adm_access_t *adm_access,
const char *dst,
const char *src,
const char *versioned,
apr_pool_t *pool) {
svn_xml_make_open_tag
(log_accum, pool, svn_xml_self_closing,
SVN_WC__LOG_CP_AND_TRANSLATE,
SVN_WC__LOG_ATTR_NAME, loggy_path(src, adm_access),
SVN_WC__LOG_ATTR_DEST, loggy_path(dst, adm_access),
SVN_WC__LOG_ATTR_ARG_2, loggy_path(versioned, adm_access),
NULL);
return SVN_NO_ERROR;
}
svn_error_t *
svn_wc__loggy_delete_entry(svn_stringbuf_t **log_accum,
svn_wc_adm_access_t *adm_access,
const char *path,
apr_pool_t *pool) {
svn_xml_make_open_tag(log_accum, pool, svn_xml_self_closing,
SVN_WC__LOG_DELETE_ENTRY,
SVN_WC__LOG_ATTR_NAME, loggy_path(path, adm_access),
NULL);
return SVN_NO_ERROR;
}
svn_error_t *
svn_wc__loggy_delete_lock(svn_stringbuf_t **log_accum,
svn_wc_adm_access_t *adm_access,
const char *path,
apr_pool_t *pool) {
svn_xml_make_open_tag(log_accum, pool, svn_xml_self_closing,
SVN_WC__LOG_DELETE_LOCK,
SVN_WC__LOG_ATTR_NAME, loggy_path(path, adm_access),
NULL);
return SVN_NO_ERROR;
}
svn_error_t *
svn_wc__loggy_delete_changelist(svn_stringbuf_t **log_accum,
svn_wc_adm_access_t *adm_access,
const char *path,
apr_pool_t *pool) {
svn_xml_make_open_tag(log_accum, pool, svn_xml_self_closing,
SVN_WC__LOG_DELETE_CHANGELIST,
SVN_WC__LOG_ATTR_NAME, loggy_path(path, adm_access),
NULL);
return SVN_NO_ERROR;
}
svn_error_t *
svn_wc__loggy_entry_modify(svn_stringbuf_t **log_accum,
svn_wc_adm_access_t *adm_access,
const char *name,
svn_wc_entry_t *entry,
apr_uint64_t modify_flags,
apr_pool_t *pool) {
apr_hash_t *prop_hash = apr_hash_make(pool);
static const char *kind_str[] = {
"none",
SVN_WC__ENTRIES_ATTR_FILE_STR,
SVN_WC__ENTRIES_ATTR_DIR_STR,
"unknown",
};
static const char *schedule_str[] = {
"",
SVN_WC__ENTRY_VALUE_ADD,
SVN_WC__ENTRY_VALUE_DELETE,
SVN_WC__ENTRY_VALUE_REPLACE,
};
if (! modify_flags)
return SVN_NO_ERROR;
#define ADD_ENTRY_ATTR(attr_flag, attr_name, value) if (modify_flags & (attr_flag)) apr_hash_set(prop_hash, (attr_name), APR_HASH_KEY_STRING, value)
ADD_ENTRY_ATTR(SVN_WC__ENTRY_MODIFY_REVISION,
SVN_WC__ENTRY_ATTR_REVISION,
apr_psprintf(pool, "%ld", entry->revision));
ADD_ENTRY_ATTR(SVN_WC__ENTRY_MODIFY_URL,
SVN_WC__ENTRY_ATTR_URL,
entry->url);
ADD_ENTRY_ATTR(SVN_WC__ENTRY_MODIFY_REPOS,
SVN_WC__ENTRY_ATTR_REPOS,
entry->repos);
ADD_ENTRY_ATTR(SVN_WC__ENTRY_MODIFY_UUID,
SVN_WC__ENTRY_ATTR_UUID,
entry->uuid);
ADD_ENTRY_ATTR(SVN_WC__ENTRY_MODIFY_KIND,
SVN_WC__ENTRY_ATTR_KIND,
kind_str[entry->kind]);
ADD_ENTRY_ATTR(SVN_WC__ENTRY_MODIFY_SCHEDULE,
SVN_WC__ENTRY_ATTR_SCHEDULE,
schedule_str[entry->schedule]);
ADD_ENTRY_ATTR(SVN_WC__ENTRY_MODIFY_COPIED,
SVN_WC__ENTRY_ATTR_COPIED,
entry->copied ? "true" : "false");
ADD_ENTRY_ATTR(SVN_WC__ENTRY_MODIFY_DELETED,
SVN_WC__ENTRY_ATTR_DELETED,
entry->deleted ? "true" : "false");
ADD_ENTRY_ATTR(SVN_WC__ENTRY_MODIFY_ABSENT,
SVN_WC__ENTRY_ATTR_ABSENT,
entry->absent ? "true" : "false");
ADD_ENTRY_ATTR(SVN_WC__ENTRY_MODIFY_INCOMPLETE,
SVN_WC__ENTRY_ATTR_INCOMPLETE,
entry->incomplete ? "true" : "false");
ADD_ENTRY_ATTR(SVN_WC__ENTRY_MODIFY_COPYFROM_URL,
SVN_WC__ENTRY_ATTR_COPYFROM_URL,
entry->copyfrom_url);
ADD_ENTRY_ATTR(SVN_WC__ENTRY_MODIFY_COPYFROM_REV,
SVN_WC__ENTRY_ATTR_COPYFROM_REV,
apr_psprintf(pool, "%ld", entry->copyfrom_rev));
ADD_ENTRY_ATTR(SVN_WC__ENTRY_MODIFY_CONFLICT_OLD,
SVN_WC__ENTRY_ATTR_CONFLICT_OLD,
entry->conflict_old ? entry->conflict_old : "");
ADD_ENTRY_ATTR(SVN_WC__ENTRY_MODIFY_CONFLICT_NEW,
SVN_WC__ENTRY_ATTR_CONFLICT_NEW,
entry->conflict_new ? entry->conflict_new : "");
ADD_ENTRY_ATTR(SVN_WC__ENTRY_MODIFY_CONFLICT_WRK,
SVN_WC__ENTRY_ATTR_CONFLICT_WRK,
entry->conflict_wrk ? entry->conflict_wrk : "");
ADD_ENTRY_ATTR(SVN_WC__ENTRY_MODIFY_PREJFILE,
SVN_WC__ENTRY_ATTR_PREJFILE,
entry->prejfile ? entry->prejfile : "");
ADD_ENTRY_ATTR(SVN_WC__ENTRY_MODIFY_TEXT_TIME,
SVN_WC__ENTRY_ATTR_TEXT_TIME,
svn_time_to_cstring(entry->text_time, pool));
ADD_ENTRY_ATTR(SVN_WC__ENTRY_MODIFY_PROP_TIME,
SVN_WC__ENTRY_ATTR_PROP_TIME,
svn_time_to_cstring(entry->prop_time, pool));
ADD_ENTRY_ATTR(SVN_WC__ENTRY_MODIFY_CHECKSUM,
SVN_WC__ENTRY_ATTR_CHECKSUM,
entry->checksum);
ADD_ENTRY_ATTR(SVN_WC__ENTRY_MODIFY_CMT_REV,
SVN_WC__ENTRY_ATTR_CMT_REV,
apr_psprintf(pool, "%ld", entry->cmt_rev));
ADD_ENTRY_ATTR(SVN_WC__ENTRY_MODIFY_CMT_DATE,
SVN_WC__ENTRY_ATTR_CMT_DATE,
svn_time_to_cstring(entry->cmt_date, pool));
ADD_ENTRY_ATTR(SVN_WC__ENTRY_MODIFY_CMT_AUTHOR,
SVN_WC__ENTRY_ATTR_CMT_AUTHOR,
entry->cmt_author);
ADD_ENTRY_ATTR(SVN_WC__ENTRY_MODIFY_LOCK_TOKEN,
SVN_WC__ENTRY_ATTR_LOCK_TOKEN,
entry->lock_token);
ADD_ENTRY_ATTR(SVN_WC__ENTRY_MODIFY_LOCK_OWNER,
SVN_WC__ENTRY_ATTR_LOCK_OWNER,
entry->lock_owner);
ADD_ENTRY_ATTR(SVN_WC__ENTRY_MODIFY_LOCK_COMMENT,
SVN_WC__ENTRY_ATTR_LOCK_COMMENT,
entry->lock_comment);
ADD_ENTRY_ATTR(SVN_WC__ENTRY_MODIFY_LOCK_CREATION_DATE,
SVN_WC__ENTRY_ATTR_LOCK_CREATION_DATE,
svn_time_to_cstring(entry->lock_creation_date, pool));
ADD_ENTRY_ATTR(SVN_WC__ENTRY_MODIFY_HAS_PROPS,
SVN_WC__ENTRY_ATTR_HAS_PROPS,
entry->has_props ? "true" : "false");
ADD_ENTRY_ATTR(SVN_WC__ENTRY_MODIFY_HAS_PROP_MODS,
SVN_WC__ENTRY_ATTR_HAS_PROP_MODS,
entry->has_prop_mods ? "true" : "false");
ADD_ENTRY_ATTR(SVN_WC__ENTRY_MODIFY_CACHABLE_PROPS,
SVN_WC__ENTRY_ATTR_CACHABLE_PROPS,
entry->cachable_props);
ADD_ENTRY_ATTR(SVN_WC__ENTRY_MODIFY_PRESENT_PROPS,
SVN_WC__ENTRY_ATTR_PRESENT_PROPS,
entry->present_props);
ADD_ENTRY_ATTR(SVN_WC__ENTRY_MODIFY_WORKING_SIZE,
SVN_WC__ENTRY_ATTR_WORKING_SIZE,
apr_psprintf(pool, "%" APR_OFF_T_FMT,
entry->working_size));
ADD_ENTRY_ATTR(SVN_WC__ENTRY_MODIFY_FORCE,
SVN_WC__LOG_ATTR_FORCE,
"true");
#undef ADD_ENTRY_ATTR
if (apr_hash_count(prop_hash) == 0)
return SVN_NO_ERROR;
apr_hash_set(prop_hash, SVN_WC__LOG_ATTR_NAME,
APR_HASH_KEY_STRING, loggy_path(name, adm_access));
svn_xml_make_open_tag_hash(log_accum, pool,
svn_xml_self_closing,
SVN_WC__LOG_MODIFY_ENTRY,
prop_hash);
return SVN_NO_ERROR;
}
svn_error_t *
svn_wc__loggy_modify_wcprop(svn_stringbuf_t **log_accum,
svn_wc_adm_access_t *adm_access,
const char *path,
const char *propname,
const char *propval,
apr_pool_t *pool) {
svn_xml_make_open_tag(log_accum, pool, svn_xml_self_closing,
SVN_WC__LOG_MODIFY_WCPROP,
SVN_WC__LOG_ATTR_NAME,
loggy_path(path, adm_access),
SVN_WC__LOG_ATTR_PROPNAME,
propname,
SVN_WC__LOG_ATTR_PROPVAL,
propval,
NULL);
return SVN_NO_ERROR;
}
svn_error_t *
svn_wc__loggy_move(svn_stringbuf_t **log_accum,
svn_boolean_t *dst_modified,
svn_wc_adm_access_t *adm_access,
const char *src_path, const char *dst_path,
svn_boolean_t remove_dst_if_no_src,
apr_pool_t *pool) {
return loggy_move_copy_internal(log_accum, dst_modified,
SVN_WC__LOG_MV, FALSE, adm_access,
loggy_path(src_path, adm_access),
loggy_path(dst_path, adm_access),
remove_dst_if_no_src, pool);
}
svn_error_t *
svn_wc__loggy_maybe_set_executable(svn_stringbuf_t **log_accum,
svn_wc_adm_access_t *adm_access,
const char *path,
apr_pool_t *pool) {
svn_xml_make_open_tag(log_accum,
pool,
svn_xml_self_closing,
SVN_WC__LOG_MAYBE_EXECUTABLE,
SVN_WC__LOG_ATTR_NAME, loggy_path(path, adm_access),
NULL);
return SVN_NO_ERROR;
}
svn_error_t *
svn_wc__loggy_maybe_set_readonly(svn_stringbuf_t **log_accum,
svn_wc_adm_access_t *adm_access,
const char *path,
apr_pool_t *pool) {
svn_xml_make_open_tag(log_accum,
pool,
svn_xml_self_closing,
SVN_WC__LOG_MAYBE_READONLY,
SVN_WC__LOG_ATTR_NAME,
loggy_path(path, adm_access),
NULL);
return SVN_NO_ERROR;
}
svn_error_t *
svn_wc__loggy_set_entry_timestamp_from_wc(svn_stringbuf_t **log_accum,
svn_wc_adm_access_t *adm_access,
const char *path,
const char *time_prop,
apr_pool_t *pool) {
svn_xml_make_open_tag(log_accum,
pool,
svn_xml_self_closing,
SVN_WC__LOG_MODIFY_ENTRY,
SVN_WC__LOG_ATTR_NAME,
loggy_path(path, adm_access),
time_prop,
SVN_WC__TIMESTAMP_WC,
NULL);
return SVN_NO_ERROR;
}
svn_error_t *
svn_wc__loggy_set_entry_working_size_from_wc(svn_stringbuf_t **log_accum,
svn_wc_adm_access_t *adm_access,
const char *path,
apr_pool_t *pool) {
svn_xml_make_open_tag(log_accum,
pool,
svn_xml_self_closing,
SVN_WC__LOG_MODIFY_ENTRY,
SVN_WC__LOG_ATTR_NAME,
loggy_path(path, adm_access),
SVN_WC__ENTRY_ATTR_WORKING_SIZE,
SVN_WC__TIMESTAMP_WC,
NULL);
return SVN_NO_ERROR;
}
svn_error_t *
svn_wc__loggy_set_readonly(svn_stringbuf_t **log_accum,
svn_wc_adm_access_t *adm_access,
const char *path,
apr_pool_t *pool) {
svn_xml_make_open_tag(log_accum,
pool,
svn_xml_self_closing,
SVN_WC__LOG_READONLY,
SVN_WC__LOG_ATTR_NAME,
loggy_path(path, adm_access),
NULL);
return SVN_NO_ERROR;
}
svn_error_t *
svn_wc__loggy_set_timestamp(svn_stringbuf_t **log_accum,
svn_wc_adm_access_t *adm_access,
const char *path,
const char *timestr,
apr_pool_t *pool) {
svn_xml_make_open_tag(log_accum,
pool,
svn_xml_self_closing,
SVN_WC__LOG_SET_TIMESTAMP,
SVN_WC__LOG_ATTR_NAME,
loggy_path(path, adm_access),
SVN_WC__LOG_ATTR_TIMESTAMP,
timestr,
NULL);
return SVN_NO_ERROR;
}
svn_error_t *
svn_wc__loggy_remove(svn_stringbuf_t **log_accum,
svn_wc_adm_access_t *adm_access,
const char *path,
apr_pool_t *pool) {
svn_xml_make_open_tag(log_accum, pool,
svn_xml_self_closing,
SVN_WC__LOG_RM,
SVN_WC__LOG_ATTR_NAME,
loggy_path(path, adm_access),
NULL);
return SVN_NO_ERROR;
}
svn_error_t *
svn_wc__loggy_upgrade_format(svn_stringbuf_t **log_accum,
svn_wc_adm_access_t *adm_access,
int format,
apr_pool_t *pool) {
svn_xml_make_open_tag(log_accum, pool,
svn_xml_self_closing,
SVN_WC__LOG_UPGRADE_FORMAT,
SVN_WC__LOG_ATTR_FORMAT,
apr_itoa(pool, format),
NULL);
return SVN_NO_ERROR;
}
svn_error_t *
svn_wc__write_log(svn_wc_adm_access_t *adm_access,
int log_number, svn_stringbuf_t *log_content,
apr_pool_t *pool) {
apr_file_t *log_file;
const char *logfile_name = svn_wc__logfile_path(log_number, pool);
const char *adm_path = svn_wc_adm_access_path(adm_access);
SVN_ERR(svn_wc__open_adm_file(&log_file, adm_path, logfile_name,
(APR_WRITE | APR_CREATE), pool));
SVN_ERR_W(svn_io_file_write_full(log_file, log_content->data,
log_content->len, NULL, pool),
apr_psprintf(pool, _("Error writing log for '%s'"),
svn_path_local_style(logfile_name, pool)));
SVN_ERR(svn_wc__close_adm_file(log_file, adm_path, logfile_name,
TRUE, pool));
return SVN_NO_ERROR;
}
svn_error_t *
svn_wc_cleanup(const char *path,
svn_wc_adm_access_t *optional_adm_access,
const char *diff3_cmd,
svn_cancel_func_t cancel_func,
void *cancel_baton,
apr_pool_t *pool) {
return svn_wc_cleanup2(path, diff3_cmd, cancel_func, cancel_baton, pool);
}
svn_error_t *
svn_wc_cleanup2(const char *path,
const char *diff3_cmd,
svn_cancel_func_t cancel_func,
void *cancel_baton,
apr_pool_t *pool) {
apr_hash_t *entries = NULL;
apr_hash_index_t *hi;
svn_node_kind_t kind;
svn_wc_adm_access_t *adm_access;
svn_boolean_t cleanup;
int wc_format_version;
apr_pool_t *subpool;
svn_boolean_t killme, kill_adm_only;
if (cancel_func)
SVN_ERR(cancel_func(cancel_baton));
SVN_ERR(svn_wc_check_wc(path, &wc_format_version, pool));
if (wc_format_version == 0)
return svn_error_createf
(SVN_ERR_WC_NOT_DIRECTORY, NULL,
_("'%s' is not a working copy directory"),
svn_path_local_style(path, pool));
SVN_ERR(svn_wc__adm_steal_write_lock(&adm_access, NULL, path, pool));
SVN_ERR(svn_wc_entries_read(&entries, adm_access, FALSE, pool));
subpool = svn_pool_create(pool);
for (hi = apr_hash_first(pool, entries); hi; hi = apr_hash_next(hi)) {
const void *key;
void *val;
const svn_wc_entry_t *entry;
const char *entry_path;
svn_pool_clear(subpool);
apr_hash_this(hi, &key, NULL, &val);
entry = val;
entry_path = svn_path_join(path, key, subpool);
if (entry->kind == svn_node_dir
&& strcmp(key, SVN_WC_ENTRY_THIS_DIR) != 0) {
SVN_ERR(svn_io_check_path(entry_path, &kind, subpool));
if (kind == svn_node_dir)
SVN_ERR(svn_wc_cleanup2(entry_path, diff3_cmd,
cancel_func, cancel_baton, subpool));
} else {
svn_boolean_t modified;
SVN_ERR(svn_wc_props_modified_p(&modified, entry_path,
adm_access, subpool));
if (entry->kind == svn_node_file)
SVN_ERR(svn_wc_text_modified_p(&modified, entry_path, FALSE,
adm_access, subpool));
}
}
svn_pool_destroy(subpool);
SVN_ERR(svn_wc__check_killme(adm_access, &killme, &kill_adm_only, pool));
if (killme) {
SVN_ERR(handle_killme(adm_access, kill_adm_only, cancel_func,
cancel_baton, pool));
} else {
SVN_ERR(svn_wc__adm_is_cleanup_required(&cleanup, adm_access, pool));
if (cleanup)
SVN_ERR(svn_wc__rerun_log(adm_access, diff3_cmd, pool));
}
if (svn_wc__adm_path_exists(path, 0, pool, NULL))
SVN_ERR(svn_wc__adm_cleanup_tmp_area(adm_access, pool));
SVN_ERR(svn_wc_adm_close(adm_access));
return SVN_NO_ERROR;
}