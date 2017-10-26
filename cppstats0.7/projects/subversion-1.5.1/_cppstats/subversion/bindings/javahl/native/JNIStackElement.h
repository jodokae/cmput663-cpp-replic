#if !defined(JNISTACKELEMENT_H)
#define JNISTACKELEMENT_H
#include <jni.h>
#include "JNIUtil.h"
#define JNIEntry(c,m) JNIStackElement se(env, #c, #m, jthis);
#define JNIEntryStatic(c,m) JNIStackElement se(env, #c, #m, jclazz);
class JNIStackElement {
public:
JNIStackElement(JNIEnv *env, const char *clazz,
const char *method, jobject jthis);
virtual ~JNIStackElement();
private:
const char *m_method;
const char *m_clazz;
char m_objectID[JNIUtil::formatBufferSize];
};
#endif
