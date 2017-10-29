#include <apr_thread_mutex.h>
#include <apr_hash.h>
#include "svn_dso.h"
#include "svn_pools.h"
#include "svn_private_config.h"
#if APR_HAS_THREADS
static apr_thread_mutex_t *dso_mutex;
#endif
static apr_pool_t *dso_pool;
static apr_hash_t *dso_cache;
static int not_there_sentinel;
#define NOT_THERE ((void *) &not_there_sentinel)
void
svn_dso_initialize() {
if (dso_pool)
return;
dso_pool = svn_pool_create(NULL);
#if APR_HAS_THREADS
apr_thread_mutex_create(&dso_mutex, APR_THREAD_MUTEX_DEFAULT, dso_pool);
#endif
dso_cache = apr_hash_make(dso_pool);
}
#if APR_HAS_DSO
svn_error_t *
svn_dso_load(apr_dso_handle_t **dso, const char *fname) {
apr_status_t status;
if (! dso_pool)
svn_dso_initialize();
#if APR_HAS_THREADS
status = apr_thread_mutex_lock(dso_mutex);
if (status)
return svn_error_wrap_apr(status, _("Can't grab DSO mutex"));
#endif
*dso = apr_hash_get(dso_cache, fname, APR_HASH_KEY_STRING);
if (*dso == NOT_THERE) {
*dso = NULL;
#if APR_HAS_THREADS
status = apr_thread_mutex_unlock(dso_mutex);
if (status)
return svn_error_wrap_apr(status, _("Can't ungrab DSO mutex"));
#endif
return SVN_NO_ERROR;
}
if (! *dso) {
status = apr_dso_load(dso, fname, dso_pool);
if (status) {
*dso = NULL;
apr_hash_set(dso_cache,
apr_pstrdup(dso_pool, fname),
APR_HASH_KEY_STRING,
NOT_THERE);
#if APR_HAS_THREADS
status = apr_thread_mutex_unlock(dso_mutex);
if (status)
return svn_error_wrap_apr(status, _("Can't ungrab DSO mutex"));
#endif
return SVN_NO_ERROR;
}
apr_hash_set(dso_cache,
apr_pstrdup(dso_pool, fname),
APR_HASH_KEY_STRING,
*dso);
}
#if APR_HAS_THREADS
status = apr_thread_mutex_unlock(dso_mutex);
if (status)
return svn_error_wrap_apr(status, _("Can't ungrab DSO mutex"));
#endif
return SVN_NO_ERROR;
}
#endif