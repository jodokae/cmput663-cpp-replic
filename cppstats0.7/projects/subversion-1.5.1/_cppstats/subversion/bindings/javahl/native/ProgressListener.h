#if !defined(PROGRESSLISTENER_H)
#define PROGRESSLISTENER_H
#include <jni.h>
#include "svn_wc.h"
class ProgressListener {
private:
jobject m_progressListener;
ProgressListener(jobject jprogressListener);
public:
~ProgressListener();
static ProgressListener *makeCProgressListener(jobject jprogressListener);
static void progress(apr_off_t nbrBytes,
apr_off_t total,
void *baton,
apr_pool_t *pool);
protected:
void onProgress(apr_off_t progress,
apr_off_t total,
apr_pool_t *pool);
};
#endif