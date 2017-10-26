#if !defined(COMMITMESSAGE_H)
#define COMMITMESSAGE_H
#include <jni.h>
struct apr_array_header_t;
class CommitMessage {
public:
~CommitMessage();
jstring getCommitMessage(const apr_array_header_t *commit_items);
static CommitMessage *makeCCommitMessage(jobject jcommitMessage);
private:
jobject m_jcommitMessage;
CommitMessage(jobject jcommitMessage);
};
#endif
