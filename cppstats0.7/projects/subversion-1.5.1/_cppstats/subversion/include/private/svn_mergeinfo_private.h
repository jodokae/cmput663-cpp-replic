#if !defined(SVN_MERGEINFO_PRIVATE_H)
#define SVN_MERGEINFO_PRIVATE_H
#if defined(__cplusplus)
extern "C" {
#endif
svn_error_t *
svn_mergeinfo__equals(svn_boolean_t *is_equal,
svn_mergeinfo_t info1,
svn_mergeinfo_t info2,
svn_boolean_t consider_inheritance,
apr_pool_t *pool);
svn_boolean_t
svn_mergeinfo__remove_empty_rangelists(svn_mergeinfo_t mergeinfo,
apr_pool_t *pool);
svn_error_t *
svn_mergeinfo__remove_prefix_from_catalog(svn_mergeinfo_catalog_t *out_catalog,
svn_mergeinfo_catalog_t in_catalog,
const char *prefix,
apr_pool_t *pool);
#if defined(__cplusplus)
}
#endif
#endif
