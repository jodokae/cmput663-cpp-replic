#if !defined(COPY_SOURCES_H)
#define COPY_SOURCES_H
#include <jni.h>
#include <apr_tables.h>
class Pool;
class CopySources {
public:
CopySources(jobjectArray copySources);
~CopySources();
apr_array_header_t *array(Pool &pool);
static jobject makeJCopySource(const char *path, svn_revnum_t rev,
Pool &pool);
private:
jobjectArray m_copySources;
};
#endif
