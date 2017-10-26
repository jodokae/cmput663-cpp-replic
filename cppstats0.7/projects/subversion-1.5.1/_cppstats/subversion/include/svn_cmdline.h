#if !defined(SVN_CMDLINE_H)
#define SVN_CMDLINE_H
#if !defined(DOXYGEN_SHOULD_SKIP_THIS)
#define APR_WANT_STDIO
#endif
#include <apr_want.h>
#include <apr_getopt.h>
#include "svn_utf.h"
#include "svn_auth.h"
#include "svn_config.h"
#if defined(__cplusplus)
extern "C" {
#endif
int svn_cmdline_init(const char *progname, FILE *error_stream);
svn_error_t *svn_cmdline_cstring_from_utf8(const char **dest,
const char *src,
apr_pool_t *pool);
const char *svn_cmdline_cstring_from_utf8_fuzzy(const char *src,
apr_pool_t *pool);
svn_error_t * svn_cmdline_cstring_to_utf8(const char **dest,
const char *src,
apr_pool_t *pool);
svn_error_t *svn_cmdline_path_local_style_from_utf8(const char **dest,
const char *src,
apr_pool_t *pool);
svn_error_t *svn_cmdline_printf(apr_pool_t *pool,
const char *fmt,
...)
__attribute__((format(printf, 2, 3)));
svn_error_t *svn_cmdline_fprintf(FILE *stream,
apr_pool_t *pool,
const char *fmt,
...)
__attribute__((format(printf, 3, 4)));
svn_error_t *svn_cmdline_fputs(const char *string,
FILE *stream,
apr_pool_t *pool);
svn_error_t *svn_cmdline_fflush(FILE *stream);
const char *svn_cmdline_output_encoding(apr_pool_t *pool);
int svn_cmdline_handle_exit_error(svn_error_t *error,
apr_pool_t *pool,
const char *prefix);
typedef struct svn_cmdline_prompt_baton_t {
svn_cancel_func_t cancel_func;
void *cancel_baton;
} svn_cmdline_prompt_baton_t;
svn_error_t *
svn_cmdline_prompt_user2(const char **result,
const char *prompt_str,
svn_cmdline_prompt_baton_t *baton,
apr_pool_t *pool);
svn_error_t *
svn_cmdline_prompt_user(const char **result,
const char *prompt_str,
apr_pool_t *pool);
svn_error_t *
svn_cmdline_auth_simple_prompt(svn_auth_cred_simple_t **cred_p,
void *baton,
const char *realm,
const char *username,
svn_boolean_t may_save,
apr_pool_t *pool);
svn_error_t *
svn_cmdline_auth_username_prompt(svn_auth_cred_username_t **cred_p,
void *baton,
const char *realm,
svn_boolean_t may_save,
apr_pool_t *pool);
svn_error_t *
svn_cmdline_auth_ssl_server_trust_prompt
(svn_auth_cred_ssl_server_trust_t **cred_p,
void *baton,
const char *realm,
apr_uint32_t failures,
const svn_auth_ssl_server_cert_info_t *cert_info,
svn_boolean_t may_save,
apr_pool_t *pool);
svn_error_t *
svn_cmdline_auth_ssl_client_cert_prompt
(svn_auth_cred_ssl_client_cert_t **cred_p,
void *baton,
const char *realm,
svn_boolean_t may_save,
apr_pool_t *pool);
svn_error_t *
svn_cmdline_auth_ssl_client_cert_pw_prompt
(svn_auth_cred_ssl_client_cert_pw_t **cred_p,
void *baton,
const char *realm,
svn_boolean_t may_save,
apr_pool_t *pool);
svn_error_t *
svn_cmdline_setup_auth_baton(svn_auth_baton_t **ab,
svn_boolean_t non_interactive,
const char *username,
const char *password,
const char *config_dir,
svn_boolean_t no_auth_cache,
svn_config_t *cfg,
svn_cancel_func_t cancel_func,
void *cancel_baton,
apr_pool_t *pool);
svn_error_t *
svn_cmdline__getopt_init(apr_getopt_t **os,
int argc,
const char *argv[],
apr_pool_t *pool);
#if defined(__cplusplus)
}
#endif
#endif
