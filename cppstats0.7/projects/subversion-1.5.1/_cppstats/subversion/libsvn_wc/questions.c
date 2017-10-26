#include <string.h>
#include <apr_pools.h>
#include <apr_file_io.h>
#include <apr_file_info.h>
#include <apr_time.h>
#include "svn_pools.h"
#include "svn_types.h"
#include "svn_string.h"
#include "svn_error.h"
#include "svn_path.h"
#include "svn_time.h"
#include "svn_io.h"
#include "svn_props.h"
#include "wc.h"
#include "adm_files.h"
#include "questions.h"
#include "entries.h"
#include "props.h"
#include "translate.h"
#include "svn_md5.h"
#include <apr_md5.h>
#include "svn_private_config.h"
#include "private/svn_wc_private.h"
svn_error_t *
svn_wc_check_wc(const char *path,
int *wc_format,
apr_pool_t *pool) {
svn_error_t *err = SVN_NO_ERROR;
const char *format_file_path
= svn_wc__adm_path(path, FALSE, pool, SVN_WC__ADM_ENTRIES, NULL);
err = svn_io_read_version_file(wc_format, format_file_path, pool);
if (err && err->apr_err == SVN_ERR_BAD_VERSION_FILE_FORMAT) {
svn_error_clear(err);
format_file_path
= svn_wc__adm_path(path, FALSE, pool, SVN_WC__ADM_FORMAT, NULL);
err = svn_io_read_version_file(wc_format, format_file_path, pool);
}
if (err && (APR_STATUS_IS_ENOENT(err->apr_err)
|| APR_STATUS_IS_ENOTDIR(err->apr_err))) {
svn_node_kind_t kind;
svn_error_clear(err);
SVN_ERR(svn_io_check_path(path, &kind, pool));
if (kind == svn_node_none) {
return svn_error_createf
(APR_ENOENT, NULL, _("'%s' does not exist"),
svn_path_local_style(path, pool));
}
*wc_format = 0;
} else if (err)
return err;
else {
SVN_ERR(svn_wc__check_format(*wc_format, path, pool));
}
return SVN_NO_ERROR;
}
svn_error_t *
svn_wc__check_format(int wc_format, const char *path, apr_pool_t *pool) {
if (wc_format < 2) {
return svn_error_createf
(SVN_ERR_WC_UNSUPPORTED_FORMAT, NULL,
_("Working copy format of '%s' is too old (%d); "
"please check out your working copy again"),
svn_path_local_style(path, pool), wc_format);
} else if (wc_format > SVN_WC__VERSION) {
return svn_error_createf
(SVN_ERR_WC_UNSUPPORTED_FORMAT, NULL,
_("This client is too old to work with working copy '%s'. You need\n"
"to get a newer Subversion client, or to downgrade this working "
"copy.\n"
"See "
"http://subversion.tigris.org/faq.html#working-copy-format-change\n"
"for details."
),
svn_path_local_style(path, pool));
}
return SVN_NO_ERROR;
}
svn_error_t *
svn_wc__timestamps_equal_p(svn_boolean_t *equal_p,
const char *path,
svn_wc_adm_access_t *adm_access,
enum svn_wc__timestamp_kind timestamp_kind,
apr_pool_t *pool) {
apr_time_t wfile_time, entrytime = 0;
const svn_wc_entry_t *entry;
SVN_ERR(svn_wc__entry_versioned(&entry, path, adm_access, FALSE, pool));
if (timestamp_kind == svn_wc__text_time) {
SVN_ERR(svn_io_file_affected_time(&wfile_time, path, pool));
entrytime = entry->text_time;
}
else if (timestamp_kind == svn_wc__prop_time) {
SVN_ERR(svn_wc__props_last_modified(&wfile_time,
path, svn_wc__props_working,
adm_access, pool));
entrytime = entry->prop_time;
}
if (! entrytime) {
*equal_p = FALSE;
return SVN_NO_ERROR;
}
{
}
if (wfile_time == entrytime)
*equal_p = TRUE;
else
*equal_p = FALSE;
return SVN_NO_ERROR;
}
static svn_error_t *
compare_and_verify(svn_boolean_t *modified_p,
const char *versioned_file,
svn_wc_adm_access_t *adm_access,
const char *base_file,
svn_boolean_t compare_textbases,
svn_boolean_t verify_checksum,
apr_pool_t *pool) {
svn_boolean_t same;
svn_subst_eol_style_t eol_style;
const char *eol_str;
apr_hash_t *keywords;
svn_boolean_t special;
svn_boolean_t need_translation;
SVN_ERR(svn_wc__get_eol_style(&eol_style, &eol_str, versioned_file,
adm_access, pool));
SVN_ERR(svn_wc__get_keywords(&keywords, versioned_file,
adm_access, NULL, pool));
SVN_ERR(svn_wc__get_special(&special, versioned_file, adm_access, pool));
need_translation = svn_subst_translation_required(eol_style, eol_str,
keywords, special, TRUE);
compare_textbases |= special;
if (verify_checksum || need_translation) {
const unsigned char *digest;
apr_file_t *v_file_h, *b_file_h;
svn_stream_t *v_stream, *b_stream;
const svn_wc_entry_t *entry;
SVN_ERR(svn_io_file_open(&b_file_h, base_file, APR_READ,
APR_OS_DEFAULT, pool));
b_stream = svn_stream_from_aprfile2(b_file_h, FALSE, pool);
if (verify_checksum) {
SVN_ERR(svn_wc__entry_versioned(&entry, versioned_file, adm_access,
TRUE, pool));
if (entry->checksum)
b_stream = svn_stream_checksummed(b_stream, &digest, NULL, TRUE,
pool);
}
if (compare_textbases && need_translation) {
SVN_ERR(svn_subst_stream_detranslated(&v_stream,
versioned_file,
eol_style,
eol_str, TRUE,
keywords, special,
pool));
} else {
SVN_ERR(svn_io_file_open(&v_file_h, versioned_file, APR_READ,
APR_OS_DEFAULT, pool));
v_stream = svn_stream_from_aprfile2(v_file_h, FALSE, pool);
if (need_translation) {
b_stream = svn_subst_stream_translated(b_stream, eol_str,
FALSE, keywords, TRUE,
pool);
}
}
SVN_ERR(svn_stream_contents_same(&same, b_stream, v_stream, pool));
SVN_ERR(svn_stream_close(v_stream));
SVN_ERR(svn_stream_close(b_stream));
if (verify_checksum && entry->checksum) {
const char *checksum;
checksum = svn_md5_digest_to_cstring_display(digest, pool);
if (strcmp(checksum, entry->checksum) != 0) {
return svn_error_createf
(SVN_ERR_WC_CORRUPT_TEXT_BASE, NULL,
_("Checksum mismatch indicates corrupt text base: '%s'\n"
" expected: %s\n"
" actual: %s\n"),
svn_path_local_style(base_file, pool),
entry->checksum,
checksum);
}
}
} else {
SVN_ERR(svn_io_files_contents_same_p(&same, base_file, versioned_file,
pool));
}
*modified_p = (! same);
return SVN_NO_ERROR;
}
svn_error_t *
svn_wc__versioned_file_modcheck(svn_boolean_t *modified_p,
const char *versioned_file,
svn_wc_adm_access_t *adm_access,
const char *base_file,
svn_boolean_t compare_textbases,
apr_pool_t *pool) {
return compare_and_verify(modified_p, versioned_file, adm_access,
base_file, compare_textbases, FALSE, pool);
}
svn_error_t *
svn_wc__text_modified_internal_p(svn_boolean_t *modified_p,
const char *filename,
svn_boolean_t force_comparison,
svn_wc_adm_access_t *adm_access,
svn_boolean_t compare_textbases,
apr_pool_t *pool) {
const char *textbase_filename;
svn_node_kind_t kind;
svn_error_t *err;
apr_finfo_t finfo;
err = svn_io_stat(&finfo, filename,
APR_FINFO_SIZE | APR_FINFO_MTIME | APR_FINFO_TYPE
| APR_FINFO_LINK, pool);
if ((err && APR_STATUS_IS_ENOENT(err->apr_err))
|| (!err && !(finfo.filetype == APR_REG ||
finfo.filetype == APR_LNK))) {
svn_error_clear(err);
*modified_p = FALSE;
return SVN_NO_ERROR;
} else if (err)
return err;
if (! force_comparison) {
const svn_wc_entry_t *entry;
err = svn_wc_entry(&entry, filename, adm_access, FALSE, pool);
if (err) {
svn_error_clear(err);
goto compare_them;
}
if (! entry)
goto compare_them;
if (entry->working_size != SVN_WC_ENTRY_WORKING_SIZE_UNKNOWN
&& finfo.size != entry->working_size)
goto compare_them;
if (entry->text_time != finfo.mtime)
goto compare_them;
*modified_p = FALSE;
return SVN_NO_ERROR;
}
compare_them:
textbase_filename = svn_wc__text_base_path(filename, FALSE, pool);
{
apr_pool_t *subpool = svn_pool_create(pool);
err = compare_and_verify(modified_p,
filename,
adm_access,
textbase_filename,
compare_textbases,
force_comparison,
subpool);
if (err) {
svn_error_t *err2;
err2 = svn_io_check_path(textbase_filename, &kind, pool);
if (! err2 && kind != svn_node_file) {
svn_error_clear(err);
*modified_p = TRUE;
return SVN_NO_ERROR;
}
svn_error_clear(err);
return err2;
}
svn_pool_destroy(subpool);
}
if (! *modified_p && svn_wc_adm_locked(adm_access)) {
svn_wc_entry_t tmp;
tmp.working_size = finfo.size;
tmp.text_time = finfo.mtime;
SVN_ERR(svn_wc__entry_modify(adm_access,
svn_path_basename(filename, pool),
&tmp,
SVN_WC__ENTRY_MODIFY_TEXT_TIME
| SVN_WC__ENTRY_MODIFY_WORKING_SIZE,
TRUE, pool));
}
return SVN_NO_ERROR;
}
svn_error_t *
svn_wc_text_modified_p(svn_boolean_t *modified_p,
const char *filename,
svn_boolean_t force_comparison,
svn_wc_adm_access_t *adm_access,
apr_pool_t *pool) {
return svn_wc__text_modified_internal_p(modified_p, filename,
force_comparison, adm_access,
TRUE, pool);
}
svn_error_t *
svn_wc_conflicted_p(svn_boolean_t *text_conflicted_p,
svn_boolean_t *prop_conflicted_p,
const char *dir_path,
const svn_wc_entry_t *entry,
apr_pool_t *pool) {
const char *path;
svn_node_kind_t kind;
apr_pool_t *subpool = svn_pool_create(pool);
*text_conflicted_p = FALSE;
*prop_conflicted_p = FALSE;
if (entry->conflict_old) {
path = svn_path_join(dir_path, entry->conflict_old, subpool);
SVN_ERR(svn_io_check_path(path, &kind, subpool));
if (kind == svn_node_file)
*text_conflicted_p = TRUE;
}
if ((! *text_conflicted_p) && (entry->conflict_new)) {
path = svn_path_join(dir_path, entry->conflict_new, subpool);
SVN_ERR(svn_io_check_path(path, &kind, subpool));
if (kind == svn_node_file)
*text_conflicted_p = TRUE;
}
if ((! *text_conflicted_p) && (entry->conflict_wrk)) {
path = svn_path_join(dir_path, entry->conflict_wrk, subpool);
SVN_ERR(svn_io_check_path(path, &kind, subpool));
if (kind == svn_node_file)
*text_conflicted_p = TRUE;
}
if (entry->prejfile) {
path = svn_path_join(dir_path, entry->prejfile, subpool);
SVN_ERR(svn_io_check_path(path, &kind, subpool));
if (kind == svn_node_file)
*prop_conflicted_p = TRUE;
}
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
svn_error_t *
svn_wc_has_binary_prop(svn_boolean_t *has_binary_prop,
const char *path,
svn_wc_adm_access_t *adm_access,
apr_pool_t *pool) {
const svn_string_t *value;
apr_pool_t *subpool = svn_pool_create(pool);
SVN_ERR(svn_wc_prop_get(&value, SVN_PROP_MIME_TYPE, path, adm_access,
subpool));
if (value && (svn_mime_type_is_binary(value->data)))
*has_binary_prop = TRUE;
else
*has_binary_prop = FALSE;
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
