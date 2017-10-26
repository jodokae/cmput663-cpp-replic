#if !defined(POOL_H)
#define POOL_H
#include "svn_pools.h"
class Pool {
public:
Pool();
~Pool();
apr_pool_t *pool() const;
void clear() const;
private:
apr_pool_t *m_pool;
Pool(Pool &that);
Pool &operator= (Pool &that);
};
APR_INLINE
apr_pool_t *Pool::pool () const {
return m_pool;
}
APR_INLINE
void Pool::clear() const {
svn_pool_clear(m_pool);
}
#endif
