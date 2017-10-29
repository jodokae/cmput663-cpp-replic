#if !defined(SVN_CONFIG_H)
#define SVN_CONFIG_H
#include <apr_pools.h>
#include "svn_types.h"
#include "svn_error.h"
#if defined(__cplusplus)
extern "C" {
#endif
typedef struct svn_config_t svn_config_t;
#define SVN_CONFIG_CATEGORY_SERVERS "servers"
#define SVN_CONFIG_SECTION_GROUPS "groups"
#define SVN_CONFIG_SECTION_GLOBAL "global"
#define SVN_CONFIG_OPTION_HTTP_PROXY_HOST "http-proxy-host"
#define SVN_CONFIG_OPTION_HTTP_PROXY_PORT "http-proxy-port"
#define SVN_CONFIG_OPTION_HTTP_PROXY_USERNAME "http-proxy-username"
#define SVN_CONFIG_OPTION_HTTP_PROXY_PASSWORD "http-proxy-password"
#define SVN_CONFIG_OPTION_HTTP_PROXY_EXCEPTIONS "http-proxy-exceptions"
#define SVN_CONFIG_OPTION_HTTP_TIMEOUT "http-timeout"
#define SVN_CONFIG_OPTION_HTTP_COMPRESSION "http-compression"
#define SVN_CONFIG_OPTION_NEON_DEBUG_MASK "neon-debug-mask"
#define SVN_CONFIG_OPTION_HTTP_AUTH_TYPES "http-auth-types"
#define SVN_CONFIG_OPTION_SSL_AUTHORITY_FILES "ssl-authority-files"
#define SVN_CONFIG_OPTION_SSL_TRUST_DEFAULT_CA "ssl-trust-default-ca"
#define SVN_CONFIG_OPTION_SSL_CLIENT_CERT_FILE "ssl-client-cert-file"
#define SVN_CONFIG_OPTION_SSL_CLIENT_CERT_PASSWORD "ssl-client-cert-password"
#define SVN_CONFIG_OPTION_SSL_PKCS11_PROVIDER "ssl-pkcs11-provider"
#define SVN_CONFIG_OPTION_HTTP_LIBRARY "http-library"
#define SVN_CONFIG_CATEGORY_CONFIG "config"
#define SVN_CONFIG_SECTION_AUTH "auth"
#define SVN_CONFIG_OPTION_STORE_PASSWORDS "store-passwords"
#define SVN_CONFIG_OPTION_STORE_AUTH_CREDS "store-auth-creds"
#define SVN_CONFIG_SECTION_HELPERS "helpers"
#define SVN_CONFIG_OPTION_EDITOR_CMD "editor-cmd"
#define SVN_CONFIG_OPTION_DIFF_CMD "diff-cmd"
#define SVN_CONFIG_OPTION_DIFF3_CMD "diff3-cmd"
#define SVN_CONFIG_OPTION_DIFF3_HAS_PROGRAM_ARG "diff3-has-program-arg"
#define SVN_CONFIG_OPTION_MERGE_TOOL_CMD "merge-tool-cmd"
#define SVN_CONFIG_SECTION_MISCELLANY "miscellany"
#define SVN_CONFIG_OPTION_GLOBAL_IGNORES "global-ignores"
#define SVN_CONFIG_OPTION_LOG_ENCODING "log-encoding"
#define SVN_CONFIG_OPTION_USE_COMMIT_TIMES "use-commit-times"
#define SVN_CONFIG_OPTION_TEMPLATE_ROOT "template-root"
#define SVN_CONFIG_OPTION_ENABLE_AUTO_PROPS "enable-auto-props"
#define SVN_CONFIG_OPTION_NO_UNLOCK "no-unlock"
#define SVN_CONFIG_OPTION_MIMETYPES_FILE "mime-types-file"
#define SVN_CONFIG_OPTION_PRESERVED_CF_EXTS "preserved-conflict-file-exts"
#define SVN_CONFIG_OPTION_INTERACTIVE_CONFLICTS "interactive-conflicts"
#define SVN_CONFIG_SECTION_TUNNELS "tunnels"
#define SVN_CONFIG_SECTION_AUTO_PROPS "auto-props"
#define SVN_CONFIG_SECTION_GENERAL "general"
#define SVN_CONFIG_OPTION_ANON_ACCESS "anon-access"
#define SVN_CONFIG_OPTION_AUTH_ACCESS "auth-access"
#define SVN_CONFIG_OPTION_PASSWORD_DB "password-db"
#define SVN_CONFIG_OPTION_REALM "realm"
#define SVN_CONFIG_OPTION_AUTHZ_DB "authz-db"
#define SVN_CONFIG_SECTION_SASL "sasl"
#define SVN_CONFIG_OPTION_USE_SASL "use-sasl"
#define SVN_CONFIG_OPTION_MIN_SSF "min-encryption"
#define SVN_CONFIG_OPTION_MAX_SSF "max-encryption"
#define SVN_CONFIG_SECTION_USERS "users"
#define SVN_CONFIG__DEFAULT_GLOBAL_IGNORES_LINE_1 "*.o *.lo *.la *.al .libs *.so *.so.[0-9]* *.a *.pyc *.pyo"
#define SVN_CONFIG__DEFAULT_GLOBAL_IGNORES_LINE_2 "*.rej *~ #*#.#* .*.swp .DS_Store"
#define SVN_CONFIG_DEFAULT_GLOBAL_IGNORES SVN_CONFIG__DEFAULT_GLOBAL_IGNORES_LINE_1 " " SVN_CONFIG__DEFAULT_GLOBAL_IGNORES_LINE_2
#define SVN_CONFIG_TRUE "TRUE"
#define SVN_CONFIG_FALSE "FALSE"
svn_error_t *svn_config_get_config(apr_hash_t **cfg_hash,
const char *config_dir,
apr_pool_t *pool);
svn_error_t *svn_config_read(svn_config_t **cfgp,
const char *file,
svn_boolean_t must_exist,
apr_pool_t *pool);
svn_error_t *svn_config_merge(svn_config_t *cfg,
const char *file,
svn_boolean_t must_exist);
void svn_config_get(svn_config_t *cfg, const char **valuep,
const char *section, const char *option,
const char *default_value);
void svn_config_set(svn_config_t *cfg,
const char *section, const char *option,
const char *value);
svn_error_t *svn_config_get_bool(svn_config_t *cfg, svn_boolean_t *valuep,
const char *section, const char *option,
svn_boolean_t default_value);
void svn_config_set_bool(svn_config_t *cfg,
const char *section, const char *option,
svn_boolean_t value);
typedef svn_boolean_t (*svn_config_section_enumerator_t)(const char *name,
void *baton);
int svn_config_enumerate_sections(svn_config_t *cfg,
svn_config_section_enumerator_t callback,
void *baton);
typedef svn_boolean_t (*svn_config_section_enumerator2_t)(const char *name,
void *baton,
apr_pool_t *pool);
int svn_config_enumerate_sections2(svn_config_t *cfg,
svn_config_section_enumerator2_t callback,
void *baton, apr_pool_t *pool);
typedef svn_boolean_t (*svn_config_enumerator_t)(const char *name,
const char *value,
void *baton);
int svn_config_enumerate(svn_config_t *cfg, const char *section,
svn_config_enumerator_t callback, void *baton);
typedef svn_boolean_t (*svn_config_enumerator2_t)(const char *name,
const char *value,
void *baton,
apr_pool_t *pool);
int svn_config_enumerate2(svn_config_t *cfg, const char *section,
svn_config_enumerator2_t callback, void *baton,
apr_pool_t *pool);
svn_boolean_t svn_config_has_section(svn_config_t *cfg, const char *section);
const char *svn_config_find_group(svn_config_t *cfg, const char *key,
const char *master_section,
apr_pool_t *pool);
const char *svn_config_get_server_setting(svn_config_t *cfg,
const char* server_group,
const char* option_name,
const char* default_value);
svn_error_t *svn_config_get_server_setting_int(svn_config_t *cfg,
const char *server_group,
const char *option_name,
apr_int64_t default_value,
apr_int64_t *result_value,
apr_pool_t *pool);
svn_error_t *svn_config_ensure(const char *config_dir, apr_pool_t *pool);
#define SVN_CONFIG_REALMSTRING_KEY "svn:realmstring"
svn_error_t * svn_config_read_auth_data(apr_hash_t **hash,
const char *cred_kind,
const char *realmstring,
const char *config_dir,
apr_pool_t *pool);
svn_error_t * svn_config_write_auth_data(apr_hash_t *hash,
const char *cred_kind,
const char *realmstring,
const char *config_dir,
apr_pool_t *pool);
#if defined(__cplusplus)
}
#endif
#endif
