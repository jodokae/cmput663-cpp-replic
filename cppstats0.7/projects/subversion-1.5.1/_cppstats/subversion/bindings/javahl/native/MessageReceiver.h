#if !defined(MESSAGERECEIVER_H)
#define MESSAGERECEIVER_H
#include <jni.h>
class MessageReceiver {
jobject m_jthis;
public:
MessageReceiver(jobject jthis);
~MessageReceiver();
void receiveMessage(const char *message);
};
#endif
