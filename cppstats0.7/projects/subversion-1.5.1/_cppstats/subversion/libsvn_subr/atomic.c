#include <apr_time.h>
#include "private/svn_atomic.h"
#define SVN_ATOMIC_UNINITIALIZED 0
#define SVN_ATOMIC_START_INIT 1
#define SVN_ATOMIC_INIT_FAILED 2
#define SVN_ATOMIC_INITIALIZED 3
svn_error_t*
svn_atomic__init_once(volatile svn_atomic_t *global_status,
svn_error_t *(*init_func)(apr_pool_t*), apr_pool_t* pool) {
svn_atomic_t status = svn_atomic_cas(global_status,
SVN_ATOMIC_START_INIT,
SVN_ATOMIC_UNINITIALIZED);
if (status == SVN_ATOMIC_UNINITIALIZED) {
svn_error_t *err = init_func(pool);
if (err) {
#if APR_HAS_THREADS
svn_atomic_cas(global_status,
SVN_ATOMIC_INIT_FAILED,
SVN_ATOMIC_START_INIT);
#endif
return err;
}
svn_atomic_cas(global_status,
SVN_ATOMIC_INITIALIZED,
SVN_ATOMIC_START_INIT);
}
#if APR_HAS_THREADS
else while (status != SVN_ATOMIC_INITIALIZED) {
if (status == SVN_ATOMIC_INIT_FAILED)
return svn_error_create(SVN_ERR_FS_GENERAL, NULL,
"Couldn't perform atomic initialization");
apr_sleep(APR_USEC_PER_SEC / 1000);
status = svn_atomic_cas(global_status,
SVN_ATOMIC_UNINITIALIZED,
SVN_ATOMIC_UNINITIALIZED);
}
#endif
return SVN_NO_ERROR;
}
