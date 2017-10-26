#if !defined(LOGMESSAGECALLBACK_H)
#define LOGMESSAGECALLBACK_H
#include <jni.h>
#include "svn_client.h"
class LogMessageCallback {
public:
LogMessageCallback(jobject jcallback);
~LogMessageCallback();
static svn_error_t *callback(void *baton,
svn_log_entry_t *log_entry,
apr_pool_t *pool);
protected:
svn_error_t *singleMessage(svn_log_entry_t *log_entry, apr_pool_t *pool);
private:
jobject m_callback;
};
#endif
