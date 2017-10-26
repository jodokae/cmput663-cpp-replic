#if !defined(STATUSCALLBACK_H)
#define STATUSCALLBACK_H
#include <jni.h>
#include "svn_client.h"
class StatusCallback {
public:
StatusCallback(jobject jcallback);
~StatusCallback();
static void callback(void *baton,
const char *path,
svn_wc_status2_t *status);
protected:
void doStatus(const char *path, svn_wc_status2_t *status);
private:
jobject m_callback;
jobject createJavaStatus(const char *path,
svn_wc_status2_t *status);
};
#endif
