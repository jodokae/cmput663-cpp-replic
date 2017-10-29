#if !defined(INFOCALLBACK_H)
#define INFOCALLBACK_H
#include <jni.h>
#include "svn_client.h"
struct info_entry;
class InfoCallback {
public:
InfoCallback(jobject jcallback);
~InfoCallback();
static svn_error_t *callback(void *baton,
const char *path,
const svn_info_t *info,
apr_pool_t *pool);
protected:
svn_error_t *singleInfo(const char *path,
const svn_info_t *info,
apr_pool_t *pool);
private:
jobject m_callback;
jobject createJavaInfo2(const char *path,
const svn_info_t *info,
apr_pool_t *pool);
};
#endif