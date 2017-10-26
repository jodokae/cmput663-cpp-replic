#if !defined(TARGETS_H)
#define TARGETS_H
#include <jni.h>
struct apr_array_header_t;
struct svn_error_t;
class Pool;
#include "Path.h"
#include <vector>
class Targets {
private:
std::vector<Path> m_targets;
jobjectArray m_targetArray;
svn_error_t *m_error_occured;
bool m_doesNotContainsPath;
public:
Targets(jobjectArray jtargets);
Targets(const char *path);
void add(const char *path);
~Targets();
const apr_array_header_t *array(const Pool &pool);
svn_error_t *error_occured();
void setDoesNotContainsPath();
};
#endif
