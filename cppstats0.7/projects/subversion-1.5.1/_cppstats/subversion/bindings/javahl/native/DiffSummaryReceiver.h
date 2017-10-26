#if !defined(DIFFSUMMARYRECEIVER_H)
#define DIFFSUMMARYRECEIVER_H
#include <jni.h>
#include "svn_client.h"
class DiffSummaryReceiver {
public:
DiffSummaryReceiver(jobject jreceiver);
~DiffSummaryReceiver();
static svn_error_t *summarize(const svn_client_diff_summarize_t *diff,
void *baton,
apr_pool_t *pool);
protected:
svn_error_t *onSummary(const svn_client_diff_summarize_t *diff,
apr_pool_t *pool);
private:
jobject m_receiver;
};
#endif
