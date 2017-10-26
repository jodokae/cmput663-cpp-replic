#if !defined(REVISION_RANGE_H)
#define REVISION_RANGE_H
#include <jni.h>
#include "svn_types.h"
class Pool;
class RevisionRange {
public:
RevisionRange(jobject jthis);
~RevisionRange();
const svn_opt_revision_range_t *toRange(Pool &pool) const;
static jobject makeJRevisionRange(svn_merge_range_t *range);
private:
jobject m_range;
};
#endif
