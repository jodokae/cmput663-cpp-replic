#if !defined(CHANGELISTCALLBACK_H)
#define CHANGELISTCALLBACK_H
#include <jni.h>
#include "svn_client.h"
class ChangelistCallback {
public:
ChangelistCallback(jobject jcallback);
~ChangelistCallback();
static svn_error_t *callback(void *baton,
const char *path,
const char *changelist,
apr_pool_t *pool);
protected:
void doChangelist(const char *path, const char *changelist, apr_pool_t *pool);
private:
jobject m_callback;
};
#endif
