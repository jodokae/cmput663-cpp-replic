#if !defined(SVN_LIBSVN_FS_DBT_H)
#define SVN_LIBSVN_FS_DBT_H
#include <apr_pools.h>
#define APU_WANT_DB
#include <apu_want.h>
#include "svn_fs.h"
#include "../util/skel.h"
#if defined(__cplusplus)
extern "C" {
#endif
DBT *svn_fs_base__clear_dbt(DBT *dbt);
DBT *svn_fs_base__nodata_dbt(DBT *dbt);
DBT *svn_fs_base__set_dbt(DBT *dbt, const void *data, u_int32_t size);
DBT *svn_fs_base__result_dbt(DBT *dbt);
DBT *svn_fs_base__track_dbt(DBT *dbt, apr_pool_t *pool);
DBT *svn_fs_base__recno_dbt(DBT *dbt, db_recno_t *recno);
int svn_fs_base__compare_dbt(const DBT *a, const DBT *b);
DBT *svn_fs_base__id_to_dbt(DBT *dbt, const svn_fs_id_t *id,
apr_pool_t *pool);
DBT *svn_fs_base__skel_to_dbt(DBT *dbt, skel_t *skel, apr_pool_t *pool);
DBT *svn_fs_base__str_to_dbt(DBT *dbt, const char *str);
#if defined(__cplusplus)
}
#endif
#endif
