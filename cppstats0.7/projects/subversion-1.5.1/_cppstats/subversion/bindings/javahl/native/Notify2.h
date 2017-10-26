#if !defined(NOTIFY2_H)
#define NOTIFY2_H
#include <jni.h>
#include "svn_wc.h"
class Notify2 {
private:
jobject m_notify;
Notify2(jobject p_notify);
public:
static Notify2 *makeCNotify(jobject notify);
~Notify2();
static void notify(void *baton,
const svn_wc_notify_t *notify,
apr_pool_t *pool);
void onNotify(const svn_wc_notify_t *notify,
apr_pool_t *pool);
};
#endif
