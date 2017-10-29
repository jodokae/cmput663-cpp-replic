#include <apr.h>
#include <apr_general.h>
#include <apr_file_io.h>
#include "svn_pools.h"
#include "svn_diff.h"
#include "svn_io.h"
static svn_error_t *
do_diff4(svn_stream_t *ostream,
const char *original,
const char *modified,
const char *latest,
const char *ancestor,
apr_pool_t *pool) {
svn_diff_t *diff;
SVN_ERR(svn_diff_file_diff4(&diff,
original, modified, latest, ancestor,
pool));
SVN_ERR(svn_diff_file_output_merge(ostream, diff,
original, modified, latest,
NULL, NULL, NULL, NULL,
FALSE,
FALSE,
pool));
return NULL;
}
int main(int argc, char *argv[]) {
apr_pool_t *pool;
svn_stream_t *ostream;
int rc = 0;
svn_error_t *svn_err;
apr_initialize();
pool = svn_pool_create(NULL);
svn_err = svn_stream_for_stdout(&ostream, pool);
if (svn_err) {
svn_handle_error2(svn_err, stdout, FALSE, "diff4-test: ");
rc = 2;
} else if (argc == 5) {
svn_err = do_diff4(ostream,
argv[2], argv[1], argv[3], argv[4],
pool);
if (svn_err != NULL) {
svn_handle_error2(svn_err, stdout, FALSE, "diff4-test: ");
rc = 2;
}
} else {
svn_error_clear(svn_stream_printf
(ostream, pool, "Usage: %s <mine> <older> <yours> <ancestor>\n",
argv[0]));
rc = 2;
}
apr_terminate();
return rc;
}
