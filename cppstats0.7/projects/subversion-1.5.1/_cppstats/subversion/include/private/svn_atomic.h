#if !defined(SVN_ATOMIC_H)
#define SVN_ATOMIC_H
#include <apr_version.h>
#include <apr_atomic.h>
#include "svn_error.h"
#include "private/svn_dep_compat.h"
#if defined(__cplusplus)
extern "C" {
#endif
#if APR_VERSION_AT_LEAST(1, 0, 0)
#define svn_atomic_t apr_uint32_t
#else
#define svn_atomic_t apr_atomic_t
#endif
#if APR_VERSION_AT_LEAST(1, 0, 0)
#define svn_atomic_read(mem) apr_atomic_read32((mem))
#else
#define svn_atomic_read(mem) apr_atomic_read((mem))
#endif
#if APR_VERSION_AT_LEAST(1, 0, 0)
#define svn_atomic_set(mem, val) apr_atomic_set32((mem), (val))
#else
#define svn_atomic_set(mem, val) apr_atomic_set((mem), (val))
#endif
#if APR_VERSION_AT_LEAST(1, 0, 0)
#define svn_atomic_inc(mem) apr_atomic_inc32(mem)
#else
#define svn_atomic_inc(mem) apr_atomic_inc(mem)
#endif
#if APR_VERSION_AT_LEAST(1, 0, 0)
#define svn_atomic_dec(mem) apr_atomic_dec32(mem)
#else
#define svn_atomic_dec(mem) apr_atomic_dec(mem)
#endif
#if APR_VERSION_AT_LEAST(1, 0, 0)
#define svn_atomic_cas(mem, with, cmp) apr_atomic_cas32((mem), (with), (cmp))
#else
#define svn_atomic_cas(mem, with, cmp) apr_atomic_cas((mem), (with), (cmp))
#endif
svn_error_t *
svn_atomic__init_once(volatile svn_atomic_t *global_status,
svn_error_t *(*init_func)(apr_pool_t*), apr_pool_t* pool);
#if defined(__cplusplus)
}
#endif
#endif
