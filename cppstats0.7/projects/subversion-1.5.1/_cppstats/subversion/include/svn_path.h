#if !defined(SVN_PATH_H)
#define SVN_PATH_H
#include <apr_pools.h>
#include <apr_tables.h>
#include "svn_string.h"
#include "svn_error.h"
#if defined(__cplusplus)
extern "C" {
#endif
const char *svn_path_internal_style(const char *path, apr_pool_t *pool);
const char *svn_path_local_style(const char *path, apr_pool_t *pool);
char *svn_path_join(const char *base,
const char *component,
apr_pool_t *pool);
char *svn_path_join_many(apr_pool_t *pool, const char *base, ...);
char *svn_path_basename(const char *path, apr_pool_t *pool);
char *svn_path_dirname(const char *path, apr_pool_t *pool);
void svn_path_splitext(const char **path_root, const char **path_ext,
const char *path, apr_pool_t *pool);
apr_size_t
svn_path_component_count(const char *path);
void svn_path_add_component(svn_stringbuf_t *path,
const char *component);
void svn_path_remove_component(svn_stringbuf_t *path);
void svn_path_remove_components(svn_stringbuf_t *path, apr_size_t n);
void svn_path_split(const char *path,
const char **dirpath,
const char **base_name,
apr_pool_t *pool);
int svn_path_is_empty(const char *path);
svn_boolean_t svn_dirent_is_root(const char *dirent, apr_size_t len);
const char *svn_path_canonicalize(const char *path, apr_pool_t *pool);
svn_boolean_t svn_path_is_canonical(const char *path, apr_pool_t *pool);
int svn_path_compare_paths(const char *path1, const char *path2);
char *svn_path_get_longest_ancestor(const char *path1,
const char *path2,
apr_pool_t *pool);
svn_error_t *
svn_path_get_absolute(const char **pabsolute,
const char *relative,
apr_pool_t *pool);
svn_error_t *
svn_path_split_if_file(const char *path,
const char **pdirectory,
const char **pfile,
apr_pool_t *pool);
svn_error_t *
svn_path_condense_targets(const char **pcommon,
apr_array_header_t **pcondensed_targets,
const apr_array_header_t *targets,
svn_boolean_t remove_redundancies,
apr_pool_t *pool);
svn_error_t *
svn_path_remove_redundancies(apr_array_header_t **pcondensed_targets,
const apr_array_header_t *targets,
apr_pool_t *pool);
apr_array_header_t *svn_path_decompose(const char *path,
apr_pool_t *pool);
const char *svn_path_compose(const apr_array_header_t *components,
apr_pool_t *pool);
svn_boolean_t svn_path_is_single_path_component(const char *name);
svn_boolean_t svn_path_is_backpath_present(const char *path);
const char *svn_path_is_child(const char *path1,
const char *path2,
apr_pool_t *pool);
svn_boolean_t
svn_path_is_ancestor(const char *path1, const char *path2);
svn_error_t *svn_path_check_valid(const char *path, apr_pool_t *pool);
svn_boolean_t svn_path_is_url(const char *path);
svn_boolean_t svn_path_is_uri_safe(const char *path);
const char *svn_path_uri_encode(const char *path, apr_pool_t *pool);
const char *svn_path_uri_decode(const char *path, apr_pool_t *pool);
const char *svn_path_url_add_component(const char *url,
const char *component,
apr_pool_t *pool);
const char *svn_path_uri_from_iri(const char *iri,
apr_pool_t *pool);
const char *svn_path_uri_autoescape(const char *uri,
apr_pool_t *pool);
svn_error_t *svn_path_cstring_from_utf8(const char **path_apr,
const char *path_utf8,
apr_pool_t *pool);
svn_error_t *svn_path_cstring_to_utf8(const char **path_utf8,
const char *path_apr,
apr_pool_t *pool);
#if defined(__cplusplus)
}
#endif
#endif