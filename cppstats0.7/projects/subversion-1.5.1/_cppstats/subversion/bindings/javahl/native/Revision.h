#if !defined(REVISION_H)
#define REVISION_H
#include <jni.h>
#include "svn_opt.h"
class Revision {
private:
svn_opt_revision_t m_revision;
public:
static const svn_opt_revision_kind START;
static const svn_opt_revision_kind HEAD;
Revision(jobject jthis, bool headIfUnspecified = false,
bool oneIfUnspecified = false);
Revision(const svn_opt_revision_kind kind = svn_opt_revision_unspecified);
~Revision();
const svn_opt_revision_t *revision() const;
static jobject makeJRevision(svn_revnum_t rev);
};
#endif
