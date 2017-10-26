#if !defined(CONFLICTRESOLVERCALLBACK_H)
#define CONFLICTRESOLVERCALLBACK_H
#include <jni.h>
#include "svn_error.h"
#include "svn_wc.h"
class ConflictResolverCallback {
private:
jobject m_conflictResolver;
ConflictResolverCallback(jobject jconflictResolver);
public:
~ConflictResolverCallback();
static ConflictResolverCallback *
makeCConflictResolverCallback(jobject jconflictResolver);
static svn_error_t *
resolveConflict(svn_wc_conflict_result_t **result,
const svn_wc_conflict_description_t *desc,
void *baton,
apr_pool_t *pool);
protected:
svn_error_t * resolve(svn_wc_conflict_result_t **result,
const svn_wc_conflict_description_t *desc,
apr_pool_t *pool);
private:
static svn_wc_conflict_result_t * javaResultToC(jobject result,
apr_pool_t *pool);
static svn_wc_conflict_choice_t javaChoiceToC(jint choice);
};
#endif
