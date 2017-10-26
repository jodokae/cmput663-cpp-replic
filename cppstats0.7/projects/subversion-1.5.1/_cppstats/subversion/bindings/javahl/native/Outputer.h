#if !defined(OUTPUTER_H)
#define OUTPUTER_H
#include <jni.h>
#include "svn_io.h"
#include "Pool.h"
class Outputer {
jobject m_jthis;
static svn_error_t *write(void *baton,
const char *buffer, apr_size_t *len);
static svn_error_t *close(void *baton);
public:
Outputer(jobject jthis);
~Outputer();
svn_stream_t *getStream(const Pool &pool);
};
#endif
