#if !defined(PATH_H)
#define PATH_H
#include <string>
#include <jni.h>
struct svn_error_t;
class Path {
private:
std::string m_path;
svn_error_t *m_error_occured;
void init(const char *pi_path);
public:
Path(const std::string &pi_path = "");
Path(const char *pi_path);
Path(const Path &pi_path);
Path &operator=(const Path&);
const std::string &path() const;
const char *c_str() const;
svn_error_t *error_occured() const;
static jboolean isValid(const char *path);
};
#endif