#if !defined(JNIUTIL_H)
#define JNIUTIL_H
#include <list>
struct apr_pool_t;
struct svn_error;
class JNIMutex;
class SVNBase;
class Pool;
#include <jni.h>
#include <fstream>
#include <apr_time.h>
#include <string>
struct svn_error_t;
#define JAVA_PACKAGE "org/tigris/subversion/javahl"
class JNIUtil {
public:
static svn_error_t *preprocessPath(const char *&path, apr_pool_t *pool);
static void throwNativeException(const char *exceptionClassName,
const char *msg,
const char *source = NULL,
int aprErr = -1);
static void throwNullPointerException(const char *message);
static jbyteArray makeJByteArray(const signed char *data, int length);
static void setRequestPool(Pool *pool);
static Pool *getRequestPool();
static jobject createDate(apr_time_t time);
static void logMessage(const char *message);
static int getLogLevel();
static char *getFormatBuffer();
static void initLogFile(int level, jstring path);
static jstring makeJString(const char *txt);
static bool isJavaExceptionThrown();
static JNIEnv *getEnv();
static void setEnv(JNIEnv *);
static bool isExceptionThrown();
static void handleAPRError(int error, const char *op);
static void enqueueForDeletion(SVNBase *object);
static void putFinalizedClient(SVNBase *cl);
static const char *thrownExceptionToCString();
static void handleSVNError(svn_error_t *err);
static jstring makeSVNErrorMessage(svn_error_t *err);
static void raiseThrowable(const char *name, const char *message);
static void throwError(const char *message) {
raiseThrowable(JAVA_PACKAGE"/JNIError", message);
}
static apr_pool_t *getPool();
static bool JNIGlobalInit(JNIEnv *env);
static bool JNIInit(JNIEnv *env);
static JNIMutex *getGlobalPoolMutex();
enum { formatBufferSize = 2048 };
enum { noLog, errorLog, exceptionLog, entryLog } LogLevel;
private:
static void assembleErrorMessage(svn_error_t *err, int depth,
apr_status_t parent_apr_err,
std::string &buffer);
static void setExceptionThrown(bool flag = true);
static int g_logLevel;
static apr_pool_t *g_pool;
static std::list<SVNBase*> g_finalizedObjects;
static JNIMutex *g_finalizedObjectsMutex;
static JNIMutex *g_logMutex;
static bool g_initException;
static bool g_inInit;
static JNIEnv *g_initEnv;
static char g_initFormatBuffer[formatBufferSize];
static std::ofstream g_logStream;
static JNIMutex *g_globalPoolMutext;
};
#define SVN_JNI_NULL_PTR_EX(expr, str, ret_val) if (expr == NULL) { JNIUtil::throwNullPointerException(str); return ret_val ; }
#define SVN_JNI_ERR(expr, ret_val) do { svn_error_t *svn_jni_err__temp = (expr); if (svn_jni_err__temp != SVN_NO_ERROR) { JNIUtil::handleSVNError(svn_jni_err__temp); return ret_val ; } } while (0)
#endif
