#if !defined(JNITHREADDATA_H)
#define JNITHREADDATA_H
#include <jni.h>
#include "JNIUtil.h"
struct apr_threadkey_t;
class Pool;
class JNIThreadData {
public:
static void del(void *);
static JNIThreadData *getThreadData();
static bool initThreadData();
static void pushNewThreadData();
static void popThreadData();
JNIThreadData();
~JNIThreadData();
JNIEnv *m_env;
bool m_exceptionThrown;
char m_formatBuffer[JNIUtil::formatBufferSize];
Pool *m_requestPool;
private:
JNIThreadData *m_previous;
static apr_threadkey_t *g_key;
};
#endif