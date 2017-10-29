#if !defined(SVN_TEST__FS_HELPERS_H)
#define SVN_TEST__FS_HELPERS_H
#include <apr_pools.h>
#include "svn_error.h"
#include "svn_fs.h"
#include "svn_repos.h"
#include "svn_delta.h"
#include "svn_test.h"
#if defined(__cplusplus)
extern "C" {
#endif
svn_error_t *
svn_test__fs_new(svn_fs_t **fs_p, apr_pool_t *pool);
svn_error_t *
svn_test__create_fs(svn_fs_t **fs_p,
const char *name,
const char *fs_type,
apr_pool_t *pool);
svn_error_t *
svn_test__create_repos(svn_repos_t **repos_p,
const char *name,
const char *fs_type,
apr_pool_t *pool);
svn_error_t *
svn_test__stream_to_string(svn_stringbuf_t **string,
svn_stream_t *stream,
apr_pool_t *pool);
svn_error_t *
svn_test__set_file_contents(svn_fs_root_t *root,
const char *path,
const char *contents,
apr_pool_t *pool);
svn_error_t *
svn_test__get_file_contents(svn_fs_root_t *root,
const char *path,
svn_stringbuf_t **str,
apr_pool_t *pool);
typedef struct svn_test__tree_entry_t {
const char *path;
const char *contents;
}
svn_test__tree_entry_t;
typedef struct svn_test__tree_t {
svn_test__tree_entry_t *entries;
int num_entries;
}
svn_test__tree_t;
svn_error_t *
svn_test__validate_tree(svn_fs_root_t *root,
svn_test__tree_entry_t *entries,
int num_entries,
apr_pool_t *pool);
typedef struct svn_test__txn_script_command_t {
int cmd;
const char *path;
const char *param1;
}
svn_test__txn_script_command_t;
svn_error_t *
svn_test__txn_script_exec(svn_fs_root_t *txn_root,
svn_test__txn_script_command_t *script,
int num_edits,
apr_pool_t *pool);
svn_error_t *
svn_test__check_greek_tree(svn_fs_root_t *root,
apr_pool_t *pool);
svn_error_t *
svn_test__create_greek_tree(svn_fs_root_t *txn_root,
apr_pool_t *pool);
#if defined(__cplusplus)
}
#endif
#endif