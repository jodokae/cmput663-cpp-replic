#include <apr_pools.h>
#include <apr_strings.h>
#include "svn_types.h"
#include "svn_props.h"
#include "svn_string.h"
svn_commit_info_t *
svn_create_commit_info(apr_pool_t *pool) {
svn_commit_info_t *commit_info
= apr_pcalloc(pool, sizeof(*commit_info));
commit_info->revision = SVN_INVALID_REVNUM;
return commit_info;
}
svn_commit_info_t *
svn_commit_info_dup(const svn_commit_info_t *src_commit_info,
apr_pool_t *pool) {
svn_commit_info_t *dst_commit_info = svn_create_commit_info(pool);
dst_commit_info->date = src_commit_info->date
? apr_pstrdup(pool, src_commit_info->date) : NULL;
dst_commit_info->author = src_commit_info->author
? apr_pstrdup(pool, src_commit_info->author) : NULL;
dst_commit_info->revision = src_commit_info->revision;
dst_commit_info->post_commit_err = src_commit_info->post_commit_err
? apr_pstrdup(pool, src_commit_info->post_commit_err) : NULL;
return dst_commit_info;
}
svn_log_changed_path_t *
svn_log_changed_path_dup(const svn_log_changed_path_t *changed_path,
apr_pool_t *pool) {
svn_log_changed_path_t *new_changed_path
= apr_palloc(pool, sizeof(*new_changed_path));
*new_changed_path = *changed_path;
if (new_changed_path->copyfrom_path)
new_changed_path->copyfrom_path =
apr_pstrdup(pool, new_changed_path->copyfrom_path);
return new_changed_path;
}
static void
svn_prop__members_dup(svn_prop_t *prop, apr_pool_t *pool) {
if (prop->name)
prop->name = apr_pstrdup(pool, prop->name);
if (prop->value)
prop->value = svn_string_dup(prop->value, pool);
}
svn_prop_t *
svn_prop_dup(const svn_prop_t *prop, apr_pool_t *pool) {
svn_prop_t *new_prop = apr_palloc(pool, sizeof(*new_prop));
*new_prop = *prop;
svn_prop__members_dup(new_prop, pool);
return new_prop;
}
apr_array_header_t *
svn_prop_array_dup(const apr_array_header_t *array, apr_pool_t *pool) {
int i;
apr_array_header_t *new_array = apr_array_copy(pool, array);
for (i = 0; i < new_array->nelts; ++i) {
svn_prop_t *elt = &APR_ARRAY_IDX(new_array, i, svn_prop_t);
svn_prop__members_dup(elt, pool);
}
return new_array;
}
apr_array_header_t *
svn_prop_hash_to_array(apr_hash_t *hash, apr_pool_t *pool) {
apr_hash_index_t *hi;
apr_array_header_t *array = apr_array_make(pool, apr_hash_count(hash),
sizeof(svn_prop_t));
for (hi = apr_hash_first(pool, hash); hi; hi = apr_hash_next(hi)) {
const void *key;
void *val;
svn_prop_t prop;
apr_hash_this(hi, &key, NULL, &val);
prop.name = key;
prop.value = val;
APR_ARRAY_PUSH(array, svn_prop_t) = prop;
}
return array;
}
svn_dirent_t *
svn_dirent_dup(const svn_dirent_t *dirent,
apr_pool_t *pool) {
svn_dirent_t *new_dirent = apr_palloc(pool, sizeof(*new_dirent));
*new_dirent = *dirent;
new_dirent->last_author = apr_pstrdup(pool, dirent->last_author);
return new_dirent;
}
svn_log_entry_t *
svn_log_entry_create(apr_pool_t *pool) {
svn_log_entry_t *log_entry = apr_pcalloc(pool, sizeof(*log_entry));
return log_entry;
}
svn_location_segment_t *
svn_location_segment_dup(svn_location_segment_t *segment,
apr_pool_t *pool) {
svn_location_segment_t *new_segment =
apr_pcalloc(pool, sizeof(*new_segment));
*new_segment = *segment;
if (segment->path)
new_segment->path = apr_pstrdup(pool, segment->path);
return new_segment;
}
