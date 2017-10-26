#if !defined(INPUTER_H)
#define INPUTER_H
#include <jni.h>
#include "svn_io.h"
#include "Pool.h"
class Inputer {
private:
jobject m_jthis;
static svn_error_t *read(void *baton, char *buffer, apr_size_t *len);
static svn_error_t *close(void *baton);
public:
Inputer(jobject jthis);
~Inputer();
svn_stream_t *getStream(const Pool &pool);
};
#endif
