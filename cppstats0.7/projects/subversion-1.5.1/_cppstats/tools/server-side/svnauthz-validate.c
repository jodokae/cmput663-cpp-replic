#include "svn_pools.h"
#include "svn_repos.h"
#include "svn_cmdline.h"
int
main(int argc, const char **argv) {
apr_pool_t *pool;
svn_error_t *err;
svn_authz_t *authz;
const char *authz_file;
if (argc <= 1) {
printf("Usage: %s PATH \n\n", argv[0]);
printf("Loads the authz file at PATH and validates its syntax. \n"
"Returns:\n"
" 0 when syntax is OK.\n"
" 1 when syntax is invalid.\n"
" 2 operational error\n");
return 2;
}
authz_file = argv[1];
if (svn_cmdline_init(argv[0], stderr) != EXIT_SUCCESS)
return 2;
pool = svn_pool_create(NULL);
err = svn_repos_authz_read(&authz, authz_file, TRUE, pool);
svn_pool_destroy(pool);
if (err) {
svn_handle_error2(err, stderr, FALSE, "svnauthz-validate: ");
return 1;
} else {
return 0;
}
}