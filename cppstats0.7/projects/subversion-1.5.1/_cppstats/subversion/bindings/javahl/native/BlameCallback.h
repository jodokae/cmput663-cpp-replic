#if !defined(BLAMECALLBACK_H)
#define BLAMECALLBACK_H
#include <jni.h>
#include "svn_client.h"
class BlameCallback {
public:
BlameCallback(jobject jcallback);
~BlameCallback();
static svn_error_t *callback(void *baton,
apr_int64_t line_no,
svn_revnum_t revision,
const char *author,
const char *date,
svn_revnum_t merged_revision,
const char *merged_author,
const char *merged_date,
const char *merged_path,
const char *line,
apr_pool_t *pool);
protected:
svn_error_t *singleLine(svn_revnum_t revision, const char *author,
const char *date, svn_revnum_t mergedRevision,
const char *mergedAuthor, const char *mergedDate,
const char *mergedPath, const char *line,
apr_pool_t *pool);
private:
jobject m_callback;
};
#endif
