#if !defined(JNICRITICALSECTION_H)
#define JNICRITICALSECTION_H
class JNIMutex;
class JNICriticalSection {
public:
JNICriticalSection(JNIMutex &mutex);
~JNICriticalSection();
private:
JNIMutex *m_mutex;
};
#endif
