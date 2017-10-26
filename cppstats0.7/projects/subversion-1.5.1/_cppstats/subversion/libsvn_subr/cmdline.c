#include <stdlib.h>
#include <stdio.h>
#include <locale.h>
#if !defined(WIN32)
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif
#include <apr_errno.h>
#include <apr_general.h>
#include <apr_atomic.h>
#include <apr_strings.h>
#include <apr_pools.h>
#include "svn_cmdline.h"
#include "svn_dso.h"
#include "svn_path.h"
#include "svn_pools.h"
#include "svn_error.h"
#include "svn_nls.h"
#include "svn_auth.h"
#include "utf_impl.h"
#include "svn_private_config.h"
#include "win32_crashrpt.h"
static const char *input_encoding = NULL;
static const char *output_encoding = NULL;
int
svn_cmdline_init(const char *progname, FILE *error_stream) {
apr_status_t status;
apr_pool_t *pool;
#if !defined(WIN32)
{
struct stat st;
#if defined(AS400_UTF8)
#pragma convert(0)
#endif
if ((fstat(0, &st) == -1 && open("/dev/null", O_RDONLY) == -1) ||
(fstat(1, &st) == -1 && open("/dev/null", O_WRONLY) == -1) ||
(fstat(2, &st) == -1 && open("/dev/null", O_WRONLY) == -1))
#if defined(AS400_UTF8)
#pragma convert(1208)
#endif
{
if (error_stream)
fprintf(error_stream, "%s: error: cannot open '/dev/null'\n",
progname);
return EXIT_FAILURE;
}
}
#endif
if (error_stream)
setvbuf(error_stream, NULL, _IONBF, 0);
#if !defined(WIN32)
setvbuf(stdout, NULL, _IOLBF, 0);
#endif
#if defined(WIN32)
#if _MSC_VER < 1400
{
static char input_encoding_buffer[16];
static char output_encoding_buffer[16];
apr_snprintf(input_encoding_buffer, sizeof input_encoding_buffer,
"CP%u", (unsigned) GetConsoleCP());
input_encoding = input_encoding_buffer;
apr_snprintf(output_encoding_buffer, sizeof output_encoding_buffer,
"CP%u", (unsigned) GetConsoleOutputCP());
output_encoding = output_encoding_buffer;
}
#endif
#if defined(SVN_USE_WIN32_CRASHHANDLER)
SetUnhandledExceptionFilter(svn__unhandled_exception_filter);
#endif
#endif
if (!setlocale(LC_ALL, "")
&& !setlocale(LC_CTYPE, "")) {
if (error_stream) {
const char *env_vars[] = { "LC_ALL", "LC_CTYPE", "LANG", NULL };
const char **env_var = &env_vars[0], *env_val = NULL;
while (*env_var) {
env_val = getenv(*env_var);
if (env_val && env_val[0])
break;
++env_var;
}
if (!*env_var) {
--env_var;
env_val = "not set";
}
fprintf(error_stream,
"%s: warning: cannot set LC_CTYPE locale\n"
"%s: warning: environment variable %s is %s\n"
"%s: warning: please check that your locale name is correct\n",
progname, progname, *env_var, env_val, progname);
}
}
status = apr_initialize();
if (status) {
if (error_stream) {
char buf[1024];
apr_strerror(status, buf, sizeof(buf) - 1);
fprintf(error_stream,
"%s: error: cannot initialize APR: %s\n",
progname, buf);
}
return EXIT_FAILURE;
}
svn_dso_initialize();
if (0 > atexit(apr_terminate)) {
if (error_stream)
fprintf(error_stream,
"%s: error: atexit registration failed\n",
progname);
return EXIT_FAILURE;
}
pool = svn_pool_create(NULL);
svn_utf_initialize(pool);
{
svn_error_t *err = svn_nls_init();
if (err) {
if (error_stream && err->message)
fprintf(error_stream, "%s", err->message);
svn_error_clear(err);
return EXIT_FAILURE;
}
}
return EXIT_SUCCESS;
}
svn_error_t *
svn_cmdline_cstring_from_utf8(const char **dest,
const char *src,
apr_pool_t *pool) {
if (output_encoding == NULL)
return svn_utf_cstring_from_utf8(dest, src, pool);
else
return svn_utf_cstring_from_utf8_ex2(dest, src, output_encoding, pool);
}
const char *
svn_cmdline_cstring_from_utf8_fuzzy(const char *src,
apr_pool_t *pool) {
return svn_utf__cstring_from_utf8_fuzzy(src, pool,
svn_cmdline_cstring_from_utf8);
}
svn_error_t *
svn_cmdline_cstring_to_utf8(const char **dest,
const char *src,
apr_pool_t *pool) {
if (input_encoding == NULL)
return svn_utf_cstring_to_utf8(dest, src, pool);
else
return svn_utf_cstring_to_utf8_ex2(dest, src, input_encoding, pool);
}
svn_error_t *
svn_cmdline_path_local_style_from_utf8(const char **dest,
const char *src,
apr_pool_t *pool) {
return svn_cmdline_cstring_from_utf8(dest,
svn_path_local_style(src, pool),
pool);
}
svn_error_t *
svn_cmdline_printf(apr_pool_t *pool, const char *fmt, ...) {
const char *message;
va_list ap;
va_start(ap, fmt);
message = apr_pvsprintf(pool, fmt, ap);
va_end(ap);
return svn_cmdline_fputs(message, stdout, pool);
}
svn_error_t *
svn_cmdline_fprintf(FILE *stream, apr_pool_t *pool, const char *fmt, ...) {
const char *message;
va_list ap;
va_start(ap, fmt);
message = apr_pvsprintf(pool, fmt, ap);
va_end(ap);
return svn_cmdline_fputs(message, stream, pool);
}
svn_error_t *
svn_cmdline_fputs(const char *string, FILE* stream, apr_pool_t *pool) {
svn_error_t *err;
const char *out;
err = svn_cmdline_cstring_from_utf8(&out, string, pool);
if (err) {
svn_error_clear(err);
out = svn_cmdline_cstring_from_utf8_fuzzy(string, pool);
}
errno = 0;
if (fputs(out, stream) == EOF) {
if (errno)
return svn_error_wrap_apr(errno, _("Write error"));
else
return svn_error_create
(SVN_ERR_IO_WRITE_ERROR, NULL, NULL);
}
return SVN_NO_ERROR;
}
svn_error_t *
svn_cmdline_fflush(FILE *stream) {
errno = 0;
if (fflush(stream) == EOF) {
if (errno)
return svn_error_wrap_apr(errno, _("Write error"));
else
return svn_error_create(SVN_ERR_IO_WRITE_ERROR, NULL, NULL);
}
return SVN_NO_ERROR;
}
const char *svn_cmdline_output_encoding(apr_pool_t *pool) {
if (output_encoding)
return apr_pstrdup(pool, output_encoding);
else
return SVN_APR_LOCALE_CHARSET;
}
int
svn_cmdline_handle_exit_error(svn_error_t *err,
apr_pool_t *pool,
const char *prefix) {
svn_handle_error2(err, stderr, FALSE, prefix);
svn_error_clear(err);
if (pool)
svn_pool_destroy(pool);
return EXIT_FAILURE;
}
svn_error_t *
svn_cmdline_setup_auth_baton(svn_auth_baton_t **ab,
svn_boolean_t non_interactive,
const char *auth_username,
const char *auth_password,
const char *config_dir,
svn_boolean_t no_auth_cache,
svn_config_t *cfg,
svn_cancel_func_t cancel_func,
void *cancel_baton,
apr_pool_t *pool) {
svn_boolean_t store_password_val = TRUE;
svn_auth_provider_object_t *provider;
apr_array_header_t *providers
= apr_array_make(pool, 12, sizeof(svn_auth_provider_object_t *));
#if defined(WIN32) && !defined(__MINGW32__)
svn_auth_get_windows_simple_provider(&provider, pool);
APR_ARRAY_PUSH(providers, svn_auth_provider_object_t *) = provider;
#endif
#if defined(SVN_HAVE_KEYCHAIN_SERVICES)
svn_auth_get_keychain_simple_provider(&provider, pool);
APR_ARRAY_PUSH(providers, svn_auth_provider_object_t *) = provider;
#endif
svn_auth_get_simple_provider(&provider, pool);
APR_ARRAY_PUSH(providers, svn_auth_provider_object_t *) = provider;
svn_auth_get_username_provider(&provider, pool);
APR_ARRAY_PUSH(providers, svn_auth_provider_object_t *) = provider;
#if defined(WIN32) && !defined(__MINGW32__)
svn_auth_get_windows_ssl_server_trust_provider(&provider, pool);
APR_ARRAY_PUSH(providers, svn_auth_provider_object_t *) = provider;
#endif
svn_auth_get_ssl_server_trust_file_provider(&provider, pool);
APR_ARRAY_PUSH(providers, svn_auth_provider_object_t *) = provider;
svn_auth_get_ssl_client_cert_file_provider(&provider, pool);
APR_ARRAY_PUSH(providers, svn_auth_provider_object_t *) = provider;
svn_auth_get_ssl_client_cert_pw_file_provider(&provider, pool);
APR_ARRAY_PUSH(providers, svn_auth_provider_object_t *) = provider;
if (non_interactive == FALSE) {
svn_cmdline_prompt_baton_t *pb = NULL;
if (cancel_func) {
pb = apr_palloc(pool, sizeof(*pb));
pb->cancel_func = cancel_func;
pb->cancel_baton = cancel_baton;
}
svn_auth_get_simple_prompt_provider(&provider,
svn_cmdline_auth_simple_prompt,
pb,
2,
pool);
APR_ARRAY_PUSH(providers, svn_auth_provider_object_t *) = provider;
svn_auth_get_username_prompt_provider
(&provider, svn_cmdline_auth_username_prompt, pb,
2, pool);
APR_ARRAY_PUSH(providers, svn_auth_provider_object_t *) = provider;
svn_auth_get_ssl_server_trust_prompt_provider
(&provider, svn_cmdline_auth_ssl_server_trust_prompt, pb, pool);
APR_ARRAY_PUSH(providers, svn_auth_provider_object_t *) = provider;
svn_auth_get_ssl_client_cert_prompt_provider
(&provider, svn_cmdline_auth_ssl_client_cert_prompt, pb, 2, pool);
APR_ARRAY_PUSH(providers, svn_auth_provider_object_t *) = provider;
svn_auth_get_ssl_client_cert_pw_prompt_provider
(&provider, svn_cmdline_auth_ssl_client_cert_pw_prompt, pb, 2, pool);
APR_ARRAY_PUSH(providers, svn_auth_provider_object_t *) = provider;
}
svn_auth_open(ab, providers, pool);
if (auth_username)
svn_auth_set_parameter(*ab, SVN_AUTH_PARAM_DEFAULT_USERNAME,
auth_username);
if (auth_password)
svn_auth_set_parameter(*ab, SVN_AUTH_PARAM_DEFAULT_PASSWORD,
auth_password);
if (non_interactive)
svn_auth_set_parameter(*ab, SVN_AUTH_PARAM_NON_INTERACTIVE, "");
if (config_dir)
svn_auth_set_parameter(*ab, SVN_AUTH_PARAM_CONFIG_DIR,
config_dir);
SVN_ERR(svn_config_get_bool(cfg, &store_password_val,
SVN_CONFIG_SECTION_AUTH,
SVN_CONFIG_OPTION_STORE_PASSWORDS,
TRUE));
if (! store_password_val)
svn_auth_set_parameter(*ab, SVN_AUTH_PARAM_DONT_STORE_PASSWORDS, "");
SVN_ERR(svn_config_get_bool(cfg, &store_password_val,
SVN_CONFIG_SECTION_AUTH,
SVN_CONFIG_OPTION_STORE_AUTH_CREDS,
TRUE));
if (no_auth_cache || ! store_password_val)
svn_auth_set_parameter(*ab, SVN_AUTH_PARAM_NO_AUTH_CACHE, "");
return SVN_NO_ERROR;
}
svn_error_t *
svn_cmdline__getopt_init(apr_getopt_t **os,
int argc,
const char *argv[],
apr_pool_t *pool) {
apr_status_t apr_err;
#if defined(AS400_UTF8)
int i;
for (i = 0; i < argc; ++i) {
char *arg_utf8;
SVN_ERR(svn_utf_cstring_to_utf8_ex2(&arg_utf8, argv[i],
(const char *)0, pool));
argv[i] = arg_utf8;
}
#endif
apr_err = apr_getopt_init(os, pool, argc, argv);
if (apr_err)
return svn_error_wrap_apr(apr_err,
_("Error initializing command line arguments"));
return SVN_NO_ERROR;
}
