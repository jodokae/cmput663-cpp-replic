#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <apr_pools.h>
#include <apr_general.h>
#include <apr_lib.h>
#include "svn_cmdline.h"
#include "svn_opt.h"
#include "svn_pools.h"
#include "svn_error.h"
#include "svn_test.h"
#include "svn_io.h"
#include "svn_path.h"
#include "svn_private_config.h"
int test_argc;
const char **test_argv;
static int verbose_mode = 0;
static int quiet_mode = 0;
static int cleanup_mode = 0;
enum {
cleanup_opt = SVN_OPT_FIRST_LONGOPT_ID,
fstype_opt,
list_opt,
verbose_opt,
quiet_opt
};
static const apr_getopt_option_t cl_options[] = {
{
"cleanup", cleanup_opt, 0,
N_("remove test directories after success")
},
{
"fs-type", fstype_opt, 1,
N_("specify a filesystem backend type ARG")
},
{
"list", list_opt, 0,
N_("lists all the tests with their short description")
},
{
"verbose", verbose_opt, 0,
N_("print extra information")
},
{
"quiet", quiet_opt, 0,
N_("print only unexpected results")
},
{0, 0, 0, 0}
};
static int skip_cleanup = 0;
static apr_pool_t *cleanup_pool = 0;
static apr_status_t
cleanup_rmtree(void *data) {
if (!skip_cleanup) {
apr_pool_t *pool = svn_pool_create(NULL);
const char *path = data;
svn_error_t *err = svn_io_remove_dir2(path, FALSE, NULL, NULL, pool);
if (verbose_mode) {
if (err)
printf("FAILED CLEANUP: %s\n", path);
else
printf("CLEANUP: %s\n", path);
}
svn_pool_destroy(pool);
}
return APR_SUCCESS;
}
void
svn_test_add_dir_cleanup(const char *path) {
if (cleanup_mode) {
const char *abspath;
svn_error_t *err = svn_path_get_absolute(&abspath, path, cleanup_pool);
if (!err)
apr_pool_cleanup_register(cleanup_pool, abspath, cleanup_rmtree,
apr_pool_cleanup_null);
else if (verbose_mode)
printf("FAILED ABSPATH: %s\n", path);
}
}
apr_uint32_t
svn_test_rand(apr_uint32_t *seed) {
*seed = (*seed * 1103515245UL + 12345UL) & 0xffffffffUL;
return *seed;
}
static int
get_array_size(void) {
int i;
for (i = 1; test_funcs[i].func; i++) {
}
return (i - 1);
}
static int
do_test_num(const char *progname,
int test_num,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
svn_test_driver_t func;
svn_boolean_t skip, xfail;
svn_error_t *err;
int array_size = get_array_size();
int test_failed = 0;
const char *msg = 0;
if ((test_num > array_size) || (test_num <= 0)) {
printf("FAIL: %s: THERE IS NO TEST NUMBER %2d\n", progname, test_num);
return (skip_cleanup = 1);
} else {
func = test_funcs[test_num].func;
skip = (test_funcs[test_num].mode == svn_test_skip);
xfail = (test_funcs[test_num].mode == svn_test_xfail);
}
err = func(&msg, msg_only || skip, opts, pool);
test_failed = ((err != SVN_NO_ERROR) != (xfail != 0));
if (err) {
svn_handle_error2(err, stdout, FALSE, "svn_tests: ");
svn_error_clear(err);
}
if (msg_only) {
printf(" %2d %-5s %s\n",
test_num,
(xfail ? "XFAIL" : (skip ? "SKIP" : "")),
msg ? msg : "(test did not provide name)");
} else if ((! quiet_mode) || test_failed) {
printf("%s %s %d: %s\n",
(err
? (xfail ? "XFAIL:" : "FAIL: ")
: (xfail ? "XPASS:" : (skip ? "SKIP: " : "PASS: "))),
progname,
test_num,
msg ? msg : "(test did not provide name)");
}
if (msg) {
int len = strlen(msg);
if (len > 50)
printf("WARNING: Test docstring exceeds 50 characters\n");
if (msg[len - 1] == '.')
printf("WARNING: Test docstring ends in a period (.)\n");
if (apr_isupper(msg[0]))
printf("WARNING: Test docstring is capitalized\n");
}
skip_cleanup = test_failed;
return test_failed;
}
int
main(int argc, const char *argv[]) {
const char *prog_name;
int test_num;
int i;
int got_error = 0;
apr_pool_t *pool, *test_pool;
int ran_a_test = 0;
int list_mode = 0;
int opt_id;
apr_status_t apr_err;
apr_getopt_t *os;
svn_error_t *err;
char errmsg[200];
int array_size = get_array_size();
svn_test_opts_t opts = { NULL };
opts.fs_type = DEFAULT_FS_TYPE;
if (apr_initialize() != APR_SUCCESS) {
printf("apr_initialize() failed.\n");
exit(1);
}
pool = svn_pool_create(NULL);
test_argc = argc;
test_argv = argv;
err = svn_cmdline__getopt_init(&os, argc, argv, pool);
prog_name = strrchr(argv[0], '/');
if (prog_name)
prog_name++;
else {
prog_name = strrchr(argv[0], '\\');
if (prog_name)
prog_name++;
else
prog_name = argv[0];
}
if (err)
return svn_cmdline_handle_exit_error(err, pool, prog_name);
while (1) {
const char *opt_arg;
apr_err = apr_getopt_long(os, cl_options, &opt_id, &opt_arg);
if (APR_STATUS_IS_EOF(apr_err))
break;
else if (apr_err && (apr_err != APR_BADCH)) {
fprintf(stderr,"apr_getopt_long failed : [%d] %s\n",
apr_err, apr_strerror(apr_err, errmsg, sizeof(errmsg)));
exit(1);
}
switch (opt_id) {
case cleanup_opt:
cleanup_mode = 1;
break;
case fstype_opt:
opts.fs_type = apr_pstrdup(pool, opt_arg);
break;
case list_opt:
list_mode = 1;
break;
case verbose_opt:
verbose_mode = 1;
break;
case quiet_opt:
quiet_mode = 1;
break;
}
}
if (quiet_mode && verbose_mode) {
fprintf(stderr, "FAIL: --verbose and --quiet are mutually exclusive\n");
exit(1);
}
cleanup_pool = svn_pool_create(pool);
test_pool = svn_pool_create(pool);
if (argc >= 2) {
if (! strcmp(argv[1], "list") || list_mode) {
ran_a_test = 1;
printf("Test #Mode Test Description\n"
"------ ----- ----------------\n");
for (i = 1; i <= array_size; i++) {
if (do_test_num(prog_name, i, TRUE, &opts, test_pool))
got_error = 1;
svn_pool_clear(test_pool);
svn_pool_clear(cleanup_pool);
}
} else {
for (i = 1; i < argc; i++) {
if (apr_isdigit(argv[i][0])) {
ran_a_test = 1;
test_num = atoi(argv[i]);
if (do_test_num(prog_name, test_num, FALSE, &opts, test_pool))
got_error = 1;
svn_pool_clear(test_pool);
svn_pool_clear(cleanup_pool);
}
}
}
}
if (! ran_a_test) {
for (i = 1; i <= array_size; i++) {
if (do_test_num(prog_name, i, FALSE, &opts, test_pool))
got_error = 1;
svn_pool_clear(test_pool);
svn_pool_clear(cleanup_pool);
}
}
svn_pool_destroy(pool);
apr_terminate();
exit(got_error);
return got_error;
}