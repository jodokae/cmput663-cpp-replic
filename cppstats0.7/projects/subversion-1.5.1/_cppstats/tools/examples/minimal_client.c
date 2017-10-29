#include "svn_client.h"
#include "svn_cmdline.h"
#include "svn_pools.h"
#include "svn_config.h"
#include "svn_fs.h"
static svn_error_t *
prompt_and_read_line(const char *prompt,
char *buffer,
size_t max) {
int len;
printf("%s: ", prompt);
if (fgets(buffer, max, stdin) == NULL)
return svn_error_create(0, NULL, "error reading stdin");
len = strlen(buffer);
if (len > 0 && buffer[len-1] == '\n')
buffer[len-1] = 0;
return SVN_NO_ERROR;
}
static svn_error_t *
my_simple_prompt_callback (svn_auth_cred_simple_t **cred,
void *baton,
const char *realm,
const char *username,
svn_boolean_t may_save,
apr_pool_t *pool) {
svn_auth_cred_simple_t *ret = apr_pcalloc (pool, sizeof (*ret));
char answerbuf[100];
if (realm) {
printf ("Authentication realm: %s\n", realm);
}
if (username)
ret->username = apr_pstrdup (pool, username);
else {
SVN_ERR (prompt_and_read_line("Username", answerbuf, sizeof(answerbuf)));
ret->username = apr_pstrdup (pool, answerbuf);
}
SVN_ERR (prompt_and_read_line("Password", answerbuf, sizeof(answerbuf)));
ret->password = apr_pstrdup (pool, answerbuf);
*cred = ret;
return SVN_NO_ERROR;
}
static svn_error_t *
my_username_prompt_callback (svn_auth_cred_username_t **cred,
void *baton,
const char *realm,
svn_boolean_t may_save,
apr_pool_t *pool) {
svn_auth_cred_username_t *ret = apr_pcalloc (pool, sizeof (*ret));
char answerbuf[100];
if (realm) {
printf ("Authentication realm: %s\n", realm);
}
SVN_ERR (prompt_and_read_line("Username", answerbuf, sizeof(answerbuf)));
ret->username = apr_pstrdup (pool, answerbuf);
*cred = ret;
return SVN_NO_ERROR;
}
int
main (int argc, const char **argv) {
apr_pool_t *pool;
svn_error_t *err;
svn_opt_revision_t revision;
apr_hash_t *dirents;
apr_hash_index_t *hi;
svn_client_ctx_t *ctx;
const char *URL;
if (argc <= 1) {
printf ("Usage: %s URL\n", argv[0]);
return EXIT_FAILURE;
} else
URL = argv[1];
if (svn_cmdline_init ("minimal_client", stderr) != EXIT_SUCCESS)
return EXIT_FAILURE;
pool = svn_pool_create (NULL);
err = svn_fs_initialize (pool);
if (err) {
svn_handle_error2 (err, stderr, FALSE, "minimal_client: ");
return EXIT_FAILURE;
}
err = svn_config_ensure (NULL, pool);
if (err) {
svn_handle_error2 (err, stderr, FALSE, "minimal_client: ");
return EXIT_FAILURE;
}
{
if ((err = svn_client_create_context (&ctx, pool))) {
svn_handle_error2 (err, stderr, FALSE, "minimal_client: ");
return EXIT_FAILURE;
}
if ((err = svn_config_get_config (&(ctx->config), NULL, pool))) {
svn_handle_error2 (err, stderr, FALSE, "minimal_client: ");
return EXIT_FAILURE;
}
#if defined(WIN32)
if (getenv ("SVN_ASP_DOT_NET_HACK")) {
err = svn_wc_set_adm_dir ("_svn", pool);
if (err) {
svn_handle_error2 (err, stderr, FALSE, "minimal_client: ");
return EXIT_FAILURE;
}
}
#endif
{
svn_auth_provider_object_t *provider;
apr_array_header_t *providers
= apr_array_make (pool, 4, sizeof (svn_auth_provider_object_t *));
svn_auth_get_simple_prompt_provider (&provider,
my_simple_prompt_callback,
NULL,
2, pool);
APR_ARRAY_PUSH (providers, svn_auth_provider_object_t *) = provider;
svn_auth_get_username_prompt_provider (&provider,
my_username_prompt_callback,
NULL,
2, pool);
APR_ARRAY_PUSH (providers, svn_auth_provider_object_t *) = provider;
svn_auth_open (&ctx->auth_baton, providers, pool);
}
}
revision.kind = svn_opt_revision_head;
err = svn_client_ls (&dirents,
URL, &revision,
FALSE,
ctx, pool);
if (err) {
svn_handle_error2 (err, stderr, FALSE, "minimal_client: ");
return EXIT_FAILURE;
}
for (hi = apr_hash_first (pool, dirents); hi; hi = apr_hash_next (hi)) {
const char *entryname;
svn_dirent_t *val;
apr_hash_this (hi, (void *) &entryname, NULL, (void *) &val);
printf (" %s\n", entryname);
}
return EXIT_SUCCESS;
}
