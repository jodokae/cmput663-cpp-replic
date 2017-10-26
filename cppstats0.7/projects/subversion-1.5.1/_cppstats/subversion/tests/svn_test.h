#if !defined(SVN_TEST_H)
#define SVN_TEST_H
#include <apr_pools.h>
#include "svn_delta.h"
#include "svn_path.h"
#include "svn_types.h"
#include "svn_error.h"
#include "svn_string.h"
#if defined(__cplusplus)
extern "C" {
#endif
typedef struct svn_test_opts_t {
const char *fs_type;
} svn_test_opts_t;
typedef svn_error_t* (*svn_test_driver_t)(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool);
enum svn_test_mode_t {
svn_test_pass,
svn_test_xfail,
svn_test_skip
};
struct svn_test_descriptor_t {
svn_test_driver_t func;
enum svn_test_mode_t mode;
};
extern struct svn_test_descriptor_t test_funcs[];
#define SVN_TEST_NULL {NULL, 0}
#define SVN_TEST_PASS(func) {func, svn_test_pass}
#define SVN_TEST_XFAIL(func) {func, svn_test_xfail}
#define SVN_TEST_XFAIL_COND(func, p){func, (p) ? svn_test_xfail : svn_test_pass}
#define SVN_TEST_SKIP(func, p) {func, ((p) ? svn_test_skip : svn_test_pass)}
apr_uint32_t svn_test_rand(apr_uint32_t *seed);
void svn_test_add_dir_cleanup(const char *path);
svn_error_t *svn_test_get_editor(const svn_delta_editor_t **editor,
void **edit_baton,
const char *editor_name,
svn_stream_t *out_stream,
int indentation,
svn_boolean_t verbose,
const char *path,
apr_pool_t *pool);
#if defined(__cplusplus)
}
#endif
#endif
