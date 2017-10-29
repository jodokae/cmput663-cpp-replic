#if !defined(JNISTRINGHOLDER_H)
#define JNISTRINGHOLDER_H
#include <jni.h>
#include <apr_pools.h>
class JNIStringHolder {
public:
JNIStringHolder(jstring jtext);
~JNIStringHolder();
operator const char *() {
return m_str;
}
const char *pstrdup(apr_pool_t *pool);
protected:
const char *m_str;
JNIEnv *m_env;
jstring m_jtext;
};
#endif
