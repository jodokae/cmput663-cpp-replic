#if !defined(NOTIFY_H)
#define NOTIFY_H
#include <jni.h>
#include "svn_wc.h"
class Notify {
private:
jobject m_notify;
Notify(jobject p_notify);
public:
static Notify *makeCNotify(jobject notify);
~Notify();
static void notify(void *baton,
const char *path,
svn_wc_notify_action_t action,
svn_node_kind_t kind,
const char *mime_type,
svn_wc_notify_state_t content_state,
svn_wc_notify_state_t prop_state,
svn_revnum_t revision);
void onNotify(const char *path,
svn_wc_notify_action_t action,
svn_node_kind_t kind,
const char *mime_type,
svn_wc_notify_state_t content_state,
svn_wc_notify_state_t prop_state,
svn_revnum_t revision);
};
#endif
