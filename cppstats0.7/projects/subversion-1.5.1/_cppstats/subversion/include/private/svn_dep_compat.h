#if !defined(SVN_DEP_COMPAT_H)
#define SVN_DEP_COMPAT_H
#include <apr_version.h>
#if defined(__cplusplus)
extern "C" {
#endif
#if !defined(APR_VERSION_AT_LEAST)
#define APR_VERSION_AT_LEAST(major,minor,patch) (((major) < APR_MAJOR_VERSION) || ((major) == APR_MAJOR_VERSION && (minor) < APR_MINOR_VERSION) || ((major) == APR_MAJOR_VERSION && (minor) == APR_MINOR_VERSION && (patch) <= APR_PATCH_VERSION))
#endif
#if !defined(SERF_VERSION_AT_LEAST)
#define SERF_VERSION_AT_LEAST(major,minor,patch) (((major) < SERF_MAJOR_VERSION) || ((major) == SERF_MAJOR_VERSION && (minor) < SERF_MINOR_VERSION) || ((major) == SERF_MAJOR_VERSION && (minor) == SERF_MINOR_VERSION && (patch) <= SERF_PATCH_VERSION))
#endif
#if defined(__cplusplus)
}
#endif
#endif
