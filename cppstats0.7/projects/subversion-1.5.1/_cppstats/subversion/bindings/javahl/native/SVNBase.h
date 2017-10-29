#if !defined(SVNBASE_H)
#define SVNBASE_H
#include <jni.h>
class SVNBase {
public:
SVNBase();
virtual ~SVNBase();
jlong getCppAddr();
virtual void dispose(jobject jthis) = 0;
void finalize();
protected:
static jlong findCppAddrForJObject(jobject jthis, jfieldID *fid,
const char *className);
void dispose(jobject jthis, jfieldID *fid, const char *className);
private:
static void findCppAddrFieldID(jfieldID *fid, const char *className,
JNIEnv *env);
};
#endif