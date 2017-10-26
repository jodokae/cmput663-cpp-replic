#if !defined(SVN_LIBSVN_WC_LOG_H)
#define SVN_LIBSVN_WC_LOG_H
#include <apr_pools.h>
#include "svn_types.h"
#include "svn_error.h"
#include "svn_wc.h"
#if defined(__cplusplus)
extern "C" {
#endif
const char *svn_wc__logfile_path(int log_number,
apr_pool_t *pool);
svn_error_t *
svn_wc__loggy_append(svn_stringbuf_t **log_accum,
svn_wc_adm_access_t *adm_access,
const char *src, const char *dst,
apr_pool_t *pool);
svn_error_t *
svn_wc__loggy_committed(svn_stringbuf_t **log_accum,
svn_wc_adm_access_t *adm_access,
const char *path, svn_revnum_t revnum,
apr_pool_t *pool);
typedef enum svn_wc__copy_t {
svn_wc__copy_normal = 0,
svn_wc__copy_translate,
svn_wc__copy_translate_special_only,
svn_wc__copy_detranslate
} svn_wc__copy_t;
svn_error_t *
svn_wc__loggy_copy(svn_stringbuf_t **log_accum,
svn_boolean_t *dst_modified,
svn_wc_adm_access_t *adm_access,
svn_wc__copy_t copy_type,
const char *src_path, const char *dst_path,
svn_boolean_t remove_dst_if_no_src,
apr_pool_t *pool);
svn_error_t *
svn_wc__loggy_translated_file(svn_stringbuf_t **log_accum,
svn_wc_adm_access_t *adm_access,
const char *dst,
const char *src,
const char *versioned,
apr_pool_t *pool);
svn_error_t *
svn_wc__loggy_delete_entry(svn_stringbuf_t **log_accum,
svn_wc_adm_access_t *adm_access,
const char *path,
apr_pool_t *pool);
svn_error_t *
svn_wc__loggy_delete_lock(svn_stringbuf_t **log_accum,
svn_wc_adm_access_t *adm_access,
const char *path,
apr_pool_t *pool);
svn_error_t *
svn_wc__loggy_delete_changelist(svn_stringbuf_t **log_accum,
svn_wc_adm_access_t *adm_access,
const char *path,
apr_pool_t *pool);
svn_error_t *
svn_wc__loggy_entry_modify(svn_stringbuf_t **log_accum,
svn_wc_adm_access_t *adm_access,
const char *name,
svn_wc_entry_t *entry,
apr_uint64_t modify_flags,
apr_pool_t *pool);
svn_error_t *
svn_wc__loggy_modify_wcprop(svn_stringbuf_t **log_accum,
svn_wc_adm_access_t *adm_access,
const char *path,
const char *propname,
const char *propval,
apr_pool_t *pool);
svn_error_t *
svn_wc__loggy_move(svn_stringbuf_t **log_accum,
svn_boolean_t *dst_modified,
svn_wc_adm_access_t *adm_access,
const char *src_path, const char *dst_path,
svn_boolean_t remove_dst_if_no_src,
apr_pool_t *pool);
svn_error_t *
svn_wc__loggy_maybe_set_executable(svn_stringbuf_t **log_accum,
svn_wc_adm_access_t *adm_access,
const char *path,
apr_pool_t *pool);
svn_error_t *
svn_wc__loggy_maybe_set_readonly(svn_stringbuf_t **log_accum,
svn_wc_adm_access_t *adm_access,
const char *path,
apr_pool_t *pool);
svn_error_t *
svn_wc__loggy_set_entry_timestamp_from_wc(svn_stringbuf_t **log_accum,
svn_wc_adm_access_t *adm_access,
const char *path,
const char *time_prop,
apr_pool_t *pool);
svn_error_t *
svn_wc__loggy_set_entry_working_size_from_wc(svn_stringbuf_t **log_accum,
svn_wc_adm_access_t *adm_access,
const char *path,
apr_pool_t *pool);
svn_error_t *
svn_wc__loggy_set_readonly(svn_stringbuf_t **log_accum,
svn_wc_adm_access_t *adm_access,
const char *path,
apr_pool_t *pool);
svn_error_t *
svn_wc__loggy_set_timestamp(svn_stringbuf_t **log_accum,
svn_wc_adm_access_t *adm_access,
const char *path,
const char *timestr,
apr_pool_t *pool);
svn_error_t *
svn_wc__loggy_remove(svn_stringbuf_t **log_accum,
svn_wc_adm_access_t *adm_access,
const char *path,
apr_pool_t *pool);
svn_error_t *
svn_wc__loggy_upgrade_format(svn_stringbuf_t **log_accum,
svn_wc_adm_access_t *adm_access,
int format,
apr_pool_t *pool);
svn_error_t *
svn_wc__write_log(svn_wc_adm_access_t *adm_access,
int log_number, svn_stringbuf_t *log_content,
apr_pool_t *pool);
svn_error_t *svn_wc__run_log(svn_wc_adm_access_t *adm_access,
const char *diff3_cmd,
apr_pool_t *pool);
svn_error_t *svn_wc__rerun_log(svn_wc_adm_access_t *adm_access,
const char *diff3_cmd,
apr_pool_t *pool);
#if defined(__cplusplus)
}
#endif
#endif
