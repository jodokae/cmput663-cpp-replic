#if !defined(SVN_LIBSVN_WC_H)
#define SVN_LIBSVN_WC_H
#include <apr_pools.h>
#include <apr_hash.h>
#include "svn_types.h"
#include "svn_error.h"
#include "svn_wc.h"
#if defined(__cplusplus)
extern "C" {
#endif
#define SVN_WC__TMP_EXT ".tmp"
#define SVN_WC__PROP_REJ_EXT ".prej"
#define SVN_WC__BASE_EXT ".svn-base"
#define SVN_WC__WORK_EXT ".svn-work"
#define SVN_WC__REVERT_EXT ".svn-revert"
#define SVN_WC__VERSION 9
#define SVN_WC__NO_PROPCACHING_VERSION 5
#define SVN_WC__XML_ENTRIES_VERSION 6
#define SVN_WC__WCPROPS_MANY_FILES_VERSION 7
struct svn_wc_traversal_info_t {
apr_pool_t *pool;
apr_hash_t *externals_old;
apr_hash_t *externals_new;
apr_hash_t *depths;
};
#define SVN_WC__TIMESTAMP_WC "working"
#define SVN_WC__WORKING_SIZE_WC "working"
#define SVN_WC__ADM_FORMAT "format"
#define SVN_WC__ADM_ENTRIES "entries"
#define SVN_WC__ADM_LOCK "lock"
#define SVN_WC__ADM_TMP "tmp"
#define SVN_WC__ADM_TEXT_BASE "text-base"
#define SVN_WC__ADM_PROPS "props"
#define SVN_WC__ADM_PROP_BASE "prop-base"
#define SVN_WC__ADM_DIR_PROPS "dir-props"
#define SVN_WC__ADM_DIR_PROP_BASE "dir-prop-base"
#define SVN_WC__ADM_DIR_PROP_REVERT "dir-prop-revert"
#define SVN_WC__ADM_WCPROPS "wcprops"
#define SVN_WC__ADM_DIR_WCPROPS "dir-wcprops"
#define SVN_WC__ADM_ALL_WCPROPS "all-wcprops"
#define SVN_WC__ADM_LOG "log"
#define SVN_WC__ADM_KILLME "KILLME"
#define SVN_WC__ADM_README "README.txt"
#define SVN_WC__ADM_EMPTY_FILE "empty-file"
#define SVN_WC__THIS_DIR_PREJ "dir_conflicts"
#define SVN_WC__KILL_ADM_ONLY "adm-only"
#define SVN_WC__CACHABLE_PROPS SVN_PROP_SPECIAL " " SVN_PROP_EXTERNALS " " SVN_PROP_NEEDS_LOCK
svn_error_t *svn_wc__ensure_directory(const char *path, apr_pool_t *pool);
typedef struct svn_wc__compat_notify_baton_t {
svn_wc_notify_func_t func;
void *baton;
} svn_wc__compat_notify_baton_t;
void svn_wc__compat_call_notify_func(void *baton,
const svn_wc_notify_t *notify,
apr_pool_t *pool);
svn_error_t *
svn_wc__text_modified_internal_p(svn_boolean_t *modified_p,
const char *filename,
svn_boolean_t force_comparison,
svn_wc_adm_access_t *adm_access,
svn_boolean_t compare_textbases,
apr_pool_t *pool);
svn_error_t *
svn_wc__merge_internal(svn_stringbuf_t **log_accum,
enum svn_wc_merge_outcome_t *merge_outcome,
const char *left,
const char *right,
const char *merge_target,
const char *copyfrom_text,
svn_wc_adm_access_t *adm_access,
const char *left_label,
const char *right_label,
const char *target_label,
svn_boolean_t dry_run,
const char *diff3_cmd,
const apr_array_header_t *merge_options,
const apr_array_header_t *prop_diff,
svn_wc_conflict_resolver_func_t conflict_func,
void *conflict_baton,
apr_pool_t *pool);
svn_error_t *
svn_wc__walker_default_error_handler(const char *path,
svn_error_t *err,
void *walk_baton,
apr_pool_t *pool);
svn_error_t *
svn_wc__ambient_depth_filter_editor(const svn_delta_editor_t **editor,
void **edit_baton,
const svn_delta_editor_t *wrapped_editor,
void *wrapped_edit_baton,
const char *anchor,
const char *target,
svn_wc_adm_access_t *adm_access,
apr_pool_t *pool);
#if defined(__cplusplus)
}
#endif
#endif