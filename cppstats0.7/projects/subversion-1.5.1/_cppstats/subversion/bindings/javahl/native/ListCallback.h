#if !defined(LISTCALLBACK_H)
#define LISTCALLBACK_H
#include <jni.h>
#include "svn_client.h"
class ListCallback {
public:
ListCallback(jobject jcallback);
~ListCallback();
static svn_error_t *callback(void *baton,
const char *path,
const svn_dirent_t *dirent,
const svn_lock_t *lock,
const char *abs_path,
apr_pool_t *pool);
protected:
svn_error_t *doList(const char *path,
const svn_dirent_t *dirent,
const svn_lock_t *lock,
const char *abs_path,
apr_pool_t *pool);
private:
jobject m_callback;
jobject createJavaDirEntry(const char *path,
const char *absPath,
const svn_dirent_t *dirent);
};
#endif
