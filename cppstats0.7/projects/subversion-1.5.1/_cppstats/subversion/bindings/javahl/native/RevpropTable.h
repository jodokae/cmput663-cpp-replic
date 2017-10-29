#if !defined(REVPROPTABLE_H)
#define REVPROPTABLE_H
#include <jni.h>
struct apr_hash_t;
struct svn_error_t;
class Pool;
#include "Path.h"
#include <map>
#include <string>
class RevpropTable {
private:
std::map<std::string, std::string> m_revprops;
jobject m_revpropTable;
public:
RevpropTable(jobject jrevpropTable);
~RevpropTable();
const apr_hash_t *hash(const Pool &pool);
};
#endif
