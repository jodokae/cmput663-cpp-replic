#if !defined(ENUM_MAPPER_H)
#define ENUM_MAPPER_H
#include <jni.h>
#include "svn_client.h"
#include "svn_wc.h"
#include "svn_types.h"
class EnumMapper {
public:
static jint mapCommitMessageStateFlags(apr_byte_t flags);
static jint mapNotifyState(svn_wc_notify_state_t state);
static jint mapNotifyAction(svn_wc_notify_action_t action);
static jint mapNodeKind(svn_node_kind_t nodeKind);
static jint mapNotifyLockState(svn_wc_notify_lock_state_t state);
static jint mapStatusKind(svn_wc_status_kind svnKind);
static jint mapScheduleKind(svn_wc_schedule_t schedule);
static jint mapConflictKind(svn_wc_conflict_kind_t kind);
static jint mapConflictAction(svn_wc_conflict_action_t action);
static jint mapConflictReason(svn_wc_conflict_reason_t reason);
};
#endif
