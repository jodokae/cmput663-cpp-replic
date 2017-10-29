#if !defined(SVN_LIBSVN_SUBR_CONFIG_IMPL_H)
#define SVN_LIBSVN_SUBR_CONFIG_IMPL_H
#define APR_WANT_STDIO
#include <apr_want.h>
#include <apr_hash.h>
#include "svn_types.h"
#include "svn_string.h"
#include "svn_config.h"
#include "svn_private_config.h"
#if defined(__cplusplus)
extern "C" {
#endif
struct svn_config_t {
apr_hash_t *sections;
apr_pool_t *pool;
apr_pool_t *x_pool;
svn_boolean_t x_values;
svn_stringbuf_t *tmp_key;
svn_stringbuf_t *tmp_value;
};
svn_error_t *svn_config__parse_file(svn_config_t *cfg,
const char *file,
svn_boolean_t must_exist,
apr_pool_t *pool);
#define SVN_CONFIG__DEFAULT_SECTION "DEFAULT"
#if defined(WIN32)
svn_error_t *svn_config__win_config_path(const char **folder,
int system_path,
apr_pool_t *pool);
svn_error_t *svn_config__parse_registry(svn_config_t *cfg,
const char *file,
svn_boolean_t must_exist,
apr_pool_t *pool);
#define SVN_REGISTRY_PREFIX "REGISTRY:"
#define SVN_REGISTRY_PREFIX_LEN ((sizeof(SVN_REGISTRY_PREFIX)) - 1)
#define SVN_REGISTRY_HKLM "HKLM\\"
#define SVN_REGISTRY_HKLM_LEN ((sizeof(SVN_REGISTRY_HKLM)) - 1)
#define SVN_REGISTRY_HKCU "HKCU\\"
#define SVN_REGISTRY_HKCU_LEN ((sizeof(SVN_REGISTRY_HKCU)) - 1)
#define SVN_REGISTRY_PATH "Software\\Tigris.org\\Subversion\\"
#define SVN_REGISTRY_PATH_LEN ((sizeof(SVN_REGISTRY_PATH)) - 1)
#define SVN_REGISTRY_SYS_CONFIG_PATH SVN_REGISTRY_PREFIX SVN_REGISTRY_HKLM SVN_REGISTRY_PATH
#define SVN_REGISTRY_USR_CONFIG_PATH SVN_REGISTRY_PREFIX SVN_REGISTRY_HKCU SVN_REGISTRY_PATH
#endif
#if defined(WIN32)
#define SVN_CONFIG__SUBDIRECTORY "Subversion"
#else
#define SVN_CONFIG__SYS_DIRECTORY "/etc/subversion"
#define SVN_CONFIG__USR_DIRECTORY ".subversion"
#endif
#define SVN_CONFIG__USR_README_FILE "README.txt"
#define SVN_CONFIG__AUTH_SUBDIR "auth"
svn_error_t *
svn_config__sys_config_path(const char **path_p,
const char *fname,
apr_pool_t *pool);
svn_error_t *
svn_config__user_config_path(const char *config_dir,
const char **path_p,
const char *fname,
apr_pool_t *pool);
svn_error_t *
svn_config__open_file(FILE **pfile,
const char *filename,
const char *mode,
apr_pool_t *pool);
typedef svn_boolean_t(*svn_config__section_enumerator_t)
(const char *name, void *baton);
int svn_config__enumerate_sections(svn_config_t *cfg,
svn_config__section_enumerator_t callback,
void *baton);
#if defined(__cplusplus)
}
#endif
#endif