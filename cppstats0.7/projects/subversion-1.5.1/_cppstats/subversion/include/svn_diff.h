#if !defined(SVN_DIFF_H)
#define SVN_DIFF_H
#include <apr.h>
#include <apr_pools.h>
#include <apr_file_io.h>
#include "svn_types.h"
#include "svn_error.h"
#include "svn_io.h"
#include "svn_version.h"
#if defined(__cplusplus)
extern "C" {
#endif
const svn_version_t *svn_diff_version(void);
typedef struct svn_diff_t svn_diff_t;
typedef enum svn_diff_datasource_e {
svn_diff_datasource_original,
svn_diff_datasource_modified,
svn_diff_datasource_latest,
svn_diff_datasource_ancestor
} svn_diff_datasource_e;
typedef struct svn_diff_fns_t {
svn_error_t *(*datasource_open)(void *diff_baton,
svn_diff_datasource_e datasource);
svn_error_t *(*datasource_close)(void *diff_baton,
svn_diff_datasource_e datasource);
svn_error_t *(*datasource_get_next_token)(apr_uint32_t *hash, void **token,
void *diff_baton,
svn_diff_datasource_e datasource);
svn_error_t *(*token_compare)(void *diff_baton,
void *ltoken,
void *rtoken,
int *compare);
void (*token_discard)(void *diff_baton,
void *token);
void (*token_discard_all)(void *diff_baton);
} svn_diff_fns_t;
svn_error_t *svn_diff_diff(svn_diff_t **diff,
void *diff_baton,
const svn_diff_fns_t *diff_fns,
apr_pool_t *pool);
svn_error_t *svn_diff_diff3(svn_diff_t **diff,
void *diff_baton,
const svn_diff_fns_t *diff_fns,
apr_pool_t *pool);
svn_error_t *svn_diff_diff4(svn_diff_t **diff,
void *diff_baton,
const svn_diff_fns_t *diff_fns,
apr_pool_t *pool);
svn_boolean_t
svn_diff_contains_conflicts(svn_diff_t *diff);
svn_boolean_t
svn_diff_contains_diffs(svn_diff_t *diff);
typedef struct svn_diff_output_fns_t {
svn_error_t *(*output_common)(void *output_baton,
apr_off_t original_start,
apr_off_t original_length,
apr_off_t modified_start,
apr_off_t modified_length,
apr_off_t latest_start,
apr_off_t latest_length);
svn_error_t *(*output_diff_modified)(void *output_baton,
apr_off_t original_start,
apr_off_t original_length,
apr_off_t modified_start,
apr_off_t modified_length,
apr_off_t latest_start,
apr_off_t latest_length);
svn_error_t *(*output_diff_latest)(void *output_baton,
apr_off_t original_start,
apr_off_t original_length,
apr_off_t modified_start,
apr_off_t modified_length,
apr_off_t latest_start,
apr_off_t latest_length);
svn_error_t *(*output_diff_common)(void *output_baton,
apr_off_t original_start,
apr_off_t original_length,
apr_off_t modified_start,
apr_off_t modified_length,
apr_off_t latest_start,
apr_off_t latest_length);
svn_error_t *(*output_conflict)(void *output_baton,
apr_off_t original_start,
apr_off_t original_length,
apr_off_t modified_start,
apr_off_t modified_length,
apr_off_t latest_start,
apr_off_t latest_length,
svn_diff_t *resolved_diff);
} svn_diff_output_fns_t;
svn_error_t *
svn_diff_output(svn_diff_t *diff,
void *output_baton,
const svn_diff_output_fns_t *output_fns);
typedef enum svn_diff_file_ignore_space_t {
svn_diff_file_ignore_space_none,
svn_diff_file_ignore_space_change,
svn_diff_file_ignore_space_all
} svn_diff_file_ignore_space_t;
typedef struct svn_diff_file_options_t {
svn_diff_file_ignore_space_t ignore_space;
svn_boolean_t ignore_eol_style;
svn_boolean_t show_c_function;
} svn_diff_file_options_t;
svn_diff_file_options_t *
svn_diff_file_options_create(apr_pool_t *pool);
svn_error_t *
svn_diff_file_options_parse(svn_diff_file_options_t *options,
const apr_array_header_t *args,
apr_pool_t *pool);
svn_error_t *
svn_diff_file_diff_2(svn_diff_t **diff,
const char *original,
const char *modified,
const svn_diff_file_options_t *options,
apr_pool_t *pool);
svn_error_t *
svn_diff_file_diff(svn_diff_t **diff,
const char *original,
const char *modified,
apr_pool_t *pool);
svn_error_t *
svn_diff_file_diff3_2(svn_diff_t **diff,
const char *original,
const char *modified,
const char *latest,
const svn_diff_file_options_t *options,
apr_pool_t *pool);
svn_error_t *
svn_diff_file_diff3(svn_diff_t **diff,
const char *original,
const char *modified,
const char *latest,
apr_pool_t *pool);
svn_error_t *
svn_diff_file_diff4_2(svn_diff_t **diff,
const char *original,
const char *modified,
const char *latest,
const char *ancestor,
const svn_diff_file_options_t *options,
apr_pool_t *pool);
svn_error_t *
svn_diff_file_diff4(svn_diff_t **diff,
const char *original,
const char *modified,
const char *latest,
const char *ancestor,
apr_pool_t *pool);
svn_error_t *
svn_diff_file_output_unified3(svn_stream_t *output_stream,
svn_diff_t *diff,
const char *original_path,
const char *modified_path,
const char *original_header,
const char *modified_header,
const char *header_encoding,
const char *relative_to_dir,
svn_boolean_t show_c_function,
apr_pool_t *pool);
svn_error_t *
svn_diff_file_output_unified2(svn_stream_t *output_stream,
svn_diff_t *diff,
const char *original_path,
const char *modified_path,
const char *original_header,
const char *modified_header,
const char *header_encoding,
apr_pool_t *pool);
svn_error_t *
svn_diff_file_output_unified(svn_stream_t *output_stream,
svn_diff_t *diff,
const char *original_path,
const char *modified_path,
const char *original_header,
const char *modified_header,
apr_pool_t *pool);
svn_error_t *
svn_diff_file_output_merge(svn_stream_t *output_stream,
svn_diff_t *diff,
const char *original_path,
const char *modified_path,
const char *latest_path,
const char *conflict_original,
const char *conflict_modified,
const char *conflict_latest,
const char *conflict_separator,
svn_boolean_t display_original_in_conflict,
svn_boolean_t display_resolved_conflicts,
apr_pool_t *pool);
svn_error_t *
svn_diff_mem_string_diff(svn_diff_t **diff,
const svn_string_t *original,
const svn_string_t *modified,
const svn_diff_file_options_t *options,
apr_pool_t *pool);
svn_error_t *
svn_diff_mem_string_diff3(svn_diff_t **diff,
const svn_string_t *original,
const svn_string_t *modified,
const svn_string_t *latest,
const svn_diff_file_options_t *options,
apr_pool_t *pool);
svn_error_t *
svn_diff_mem_string_diff4(svn_diff_t **diff,
const svn_string_t *original,
const svn_string_t *modified,
const svn_string_t *latest,
const svn_string_t *ancestor,
const svn_diff_file_options_t *options,
apr_pool_t *pool);
svn_error_t *
svn_diff_mem_string_output_unified(svn_stream_t *output_stream,
svn_diff_t *diff,
const char *original_header,
const char *modified_header,
const char *header_encoding,
const svn_string_t *original,
const svn_string_t *modified,
apr_pool_t *pool);
svn_error_t *
svn_diff_mem_string_output_merge(svn_stream_t *output_stream,
svn_diff_t *diff,
const svn_string_t *original,
const svn_string_t *modified,
const svn_string_t *latest,
const char *conflict_original,
const char *conflict_modified,
const char *conflict_latest,
const char *conflict_separator,
svn_boolean_t display_original_in_conflict,
svn_boolean_t display_resolved_conflicts,
apr_pool_t *pool);
#if defined(__cplusplus)
}
#endif
#endif
