#if !defined(SVN_LIBSVN_WC_PROPS_H)
#define SVN_LIBSVN_WC_PROPS_H
#include <apr_pools.h>
#include "svn_types.h"
#include "svn_string.h"
#include "svn_props.h"
#if defined(__cplusplus)
extern "C" {
#endif
typedef enum svn_wc__props_kind_t {
svn_wc__props_base = 0,
svn_wc__props_revert,
svn_wc__props_wcprop,
svn_wc__props_working
} svn_wc__props_kind_t;
svn_error_t *svn_wc__has_props(svn_boolean_t *has_props,
const char *path,
svn_wc_adm_access_t *adm_access,
apr_pool_t *pool);
svn_error_t *svn_wc__merge_props(svn_wc_notify_state_t *state,
svn_wc_adm_access_t *adm_access,
const char *path,
apr_hash_t *server_baseprops,
apr_hash_t *base_props,
apr_hash_t *working_props,
const apr_array_header_t *propchanges,
svn_boolean_t base_merge,
svn_boolean_t dry_run,
svn_wc_conflict_resolver_func_t conflict_func,
void *conflict_baton,
apr_pool_t *pool,
svn_stringbuf_t **entry_accum);
svn_error_t *
svn_wc__wcprop_list(apr_hash_t **wcprops,
const char *entryname,
svn_wc_adm_access_t *adm_access,
apr_pool_t *pool);
svn_error_t *svn_wc__wcprop_set(const char *name,
const svn_string_t *value,
const char *path,
svn_wc_adm_access_t *adm_access,
svn_boolean_t force_write,
apr_pool_t *pool);
svn_boolean_t svn_wc__has_special_property(apr_hash_t *props);
svn_boolean_t svn_wc__has_magic_property(const apr_array_header_t *properties);
svn_error_t *svn_wc__install_props(svn_stringbuf_t **log_accum,
svn_wc_adm_access_t *adm_access,
const char *path,
apr_hash_t *base_props,
apr_hash_t *props,
svn_boolean_t write_base_props,
apr_pool_t *pool);
svn_error_t *
svn_wc__loggy_revert_props_create(svn_stringbuf_t **log_accum,
const char *path,
svn_wc_adm_access_t *adm_access,
svn_boolean_t destroy_baseprops,
apr_pool_t *pool);
svn_error_t *
svn_wc__loggy_revert_props_restore(svn_stringbuf_t **log_accum,
const char *path,
svn_wc_adm_access_t *adm_access,
apr_pool_t *pool);
svn_error_t *
svn_wc__loggy_props_delete(svn_stringbuf_t **log_accum,
const char *path,
svn_wc__props_kind_t props_kind,
svn_wc_adm_access_t *adm_access,
apr_pool_t *pool);
svn_error_t *
svn_wc__props_delete(const char *path,
svn_wc__props_kind_t props_kind,
svn_wc_adm_access_t *adm_access,
apr_pool_t *pool);
svn_error_t *
svn_wc__props_flush(const char *path,
svn_wc__props_kind_t props_kind,
svn_wc_adm_access_t *adm_access,
apr_pool_t *pool);
svn_error_t *
svn_wc__working_props_committed(const char *path,
svn_wc_adm_access_t *adm_access,
svn_boolean_t sync_entries,
apr_pool_t *pool);
svn_error_t *
svn_wc__props_last_modified(apr_time_t *mod_time,
const char *path,
svn_wc__props_kind_t props_kind,
svn_wc_adm_access_t *adm_access,
apr_pool_t *pool);
svn_error_t *
svn_wc__has_prop_mods(svn_boolean_t *prop_mods,
const char *path,
svn_wc_adm_access_t *adm_access,
apr_pool_t *pool);
svn_error_t *
svn_wc__load_props(apr_hash_t **base_props_p,
apr_hash_t **props_p,
apr_hash_t **revert_props_p,
svn_wc_adm_access_t *adm_access,
const char *path,
apr_pool_t *pool);
#if defined(__cplusplus)
}
#endif
#endif