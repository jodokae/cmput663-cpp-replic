#if !defined(PROPLISTCALLBACK_H)
#define PROPLISTCALLBACK_H
#include <jni.h>
#include "svn_client.h"
class ProplistCallback {
public:
ProplistCallback(jobject jcallback);
~ProplistCallback();
static svn_error_t *callback(void *baton,
const char *path,
apr_hash_t *prop_hash,
apr_pool_t *pool);
static jobject makeMapFromHash(apr_hash_t *prop_hash, apr_pool_t *pool);
protected:
svn_error_t *singlePath(const char *path,
apr_hash_t *prop_hash,
apr_pool_t *pool);
private:
jobject m_callback;
};
#endif
