#if !defined(SVN_VERSION_H)
#define SVN_VERSION_H
#if !defined(APR_STRINGIFY)
#include <apr_general.h>
#endif
#include "svn_types.h"
#if defined(__cplusplus)
extern "C" {
#endif
#define SVN_VER_MAJOR 1
#define SVN_VER_MINOR 5
#define SVN_VER_PATCH 1
#define SVN_VER_MICRO SVN_VER_PATCH
#define SVN_VER_LIBRARY SVN_VER_MAJOR
#define SVN_VER_TAG " (r32289)"
#define SVN_VER_NUMTAG ""
#define SVN_VER_REVISION 32289
#define SVN_VER_NUM APR_STRINGIFY(SVN_VER_MAJOR) "." APR_STRINGIFY(SVN_VER_MINOR) "." APR_STRINGIFY(SVN_VER_PATCH)
#define SVN_VER_NUMBER SVN_VER_NUM SVN_VER_NUMTAG
#define SVN_VERSION SVN_VER_NUM SVN_VER_TAG
typedef struct svn_version_t {
int major;
int minor;
int patch;
const char *tag;
} svn_version_t;
#define SVN_VERSION_DEFINE(name) static const svn_version_t name = { SVN_VER_MAJOR, SVN_VER_MINOR, SVN_VER_PATCH, SVN_VER_NUMTAG }
#define SVN_VERSION_BODY SVN_VERSION_DEFINE(versioninfo); return &versioninfo
svn_boolean_t svn_ver_compatible(const svn_version_t *my_version,
const svn_version_t *lib_version);
svn_boolean_t svn_ver_equal(const svn_version_t *my_version,
const svn_version_t *lib_version);
typedef struct svn_version_checklist_t {
const char *label;
const svn_version_t *(*version_query)(void);
} svn_version_checklist_t;
svn_error_t *svn_ver_check_list(const svn_version_t *my_version,
const svn_version_checklist_t *checklist);
const svn_version_t *svn_subr_version(void);
#if defined(__cplusplus)
}
#endif
#endif
