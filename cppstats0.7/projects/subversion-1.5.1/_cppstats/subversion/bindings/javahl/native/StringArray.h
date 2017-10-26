#if !defined(STRINGARRAY_H)
#define STRINGARRAY_H
#include <jni.h>
struct apr_array_header_t;
struct svn_error_t;
class Pool;
#include "Path.h"
#include <vector>
#include <string>
class StringArray {
private:
std::vector<std::string> m_strings;
jobjectArray m_stringArray;
public:
StringArray(jobjectArray jstrings);
~StringArray();
const apr_array_header_t *array(const Pool &pool);
};
#endif
