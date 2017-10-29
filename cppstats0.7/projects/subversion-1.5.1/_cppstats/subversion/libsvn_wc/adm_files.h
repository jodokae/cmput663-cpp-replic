#if !defined(SVN_LIBSVN_WC_ADM_FILES_H)
#define SVN_LIBSVN_WC_ADM_FILES_H
#include <apr_pools.h>
#include "svn_types.h"
#include "props.h"
#if defined(__cplusplus)
extern "C" {
#endif
const char * svn_wc__adm_path(const char *path,
svn_boolean_t tmp,
apr_pool_t *pool,
...);
svn_boolean_t svn_wc__adm_path_exists(const char *path,
svn_boolean_t tmp,
apr_pool_t *pool,
...);
svn_error_t *svn_wc__make_adm_thing(svn_wc_adm_access_t *adm_access,
const char *thing,
svn_node_kind_t type,
apr_fileperms_t perms,
svn_boolean_t tmp,
apr_pool_t *pool);
svn_error_t *svn_wc__make_killme(svn_wc_adm_access_t *adm_access,
svn_boolean_t adm_only,
apr_pool_t *pool);
svn_error_t *svn_wc__check_killme(svn_wc_adm_access_t *adm_access,
svn_boolean_t *exists,
svn_boolean_t *kill_adm_only,
apr_pool_t *pool);
svn_error_t *
svn_wc__sync_text_base(const char *path, apr_pool_t *pool);
const char *svn_wc__text_base_path(const char *path,
svn_boolean_t tmp,
apr_pool_t *pool);
const char *
svn_wc__text_revert_path(const char *path,
svn_boolean_t tmp,
apr_pool_t *pool);
svn_error_t *svn_wc__prop_path(const char **prop_path,
const char *path,
svn_node_kind_t node_kind,
svn_wc__props_kind_t props_kind,
svn_boolean_t tmp,
apr_pool_t *pool);
svn_error_t *svn_wc__open_adm_file(apr_file_t **handle,
const char *path,
const char *fname,
apr_int32_t flags,
apr_pool_t *pool);
svn_error_t *svn_wc__close_adm_file(apr_file_t *fp,
const char *path,
const char *fname,
int sync,
apr_pool_t *pool);
svn_error_t *svn_wc__remove_adm_file(const char *path,
apr_pool_t *pool,
...);
svn_error_t *svn_wc__open_text_base(apr_file_t **handle,
const char *file,
apr_int32_t flags,
apr_pool_t *pool);
svn_error_t *svn_wc__open_revert_base(apr_file_t **handle,
const char *file,
apr_int32_t flags,
apr_pool_t *pool);
svn_error_t *svn_wc__close_text_base(apr_file_t *fp,
const char *file,
int sync,
apr_pool_t *pool);
svn_error_t *svn_wc__close_revert_base(apr_file_t *fp,
const char *file,
int sync,
apr_pool_t *pool);
svn_error_t *svn_wc__open_props(apr_file_t **handle,
const char *path,
svn_node_kind_t kind,
apr_int32_t flags,
svn_boolean_t base,
svn_boolean_t wcprops,
apr_pool_t *pool);
svn_error_t *svn_wc__close_props(apr_file_t *fp,
const char *path,
svn_node_kind_t kind,
svn_boolean_t base,
svn_boolean_t wcprops,
int sync,
apr_pool_t *pool);
svn_error_t *svn_wc__adm_destroy(svn_wc_adm_access_t *adm_access,
apr_pool_t *pool);
svn_error_t *svn_wc__adm_cleanup_tmp_area(svn_wc_adm_access_t *adm_access,
apr_pool_t *pool);
#if defined(__cplusplus)
}
#endif
#endif