#include "svn_pools.h"
#include "svn_error.h"
#include "svn_path.h"
svn_error_t *
svn_path_condense_targets(const char **pcommon,
apr_array_header_t **pcondensed_targets,
const apr_array_header_t *targets,
svn_boolean_t remove_redundancies,
apr_pool_t *pool) {
int i, j, num_condensed = targets->nelts;
svn_boolean_t *removed;
apr_array_header_t *abs_targets;
int basedir_len;
if (targets->nelts <= 0) {
*pcommon = NULL;
if (pcondensed_targets)
*pcondensed_targets = NULL;
return SVN_NO_ERROR;
}
SVN_ERR(svn_path_get_absolute(pcommon,
APR_ARRAY_IDX(targets, 0, const char *),
pool));
if (targets->nelts == 1) {
if (pcondensed_targets)
*pcondensed_targets = apr_array_make(pool, 0, sizeof(const char *));
return SVN_NO_ERROR;
}
removed = apr_pcalloc(pool, (targets->nelts * sizeof(svn_boolean_t)));
abs_targets = apr_array_make(pool, targets->nelts, sizeof(const char *));
APR_ARRAY_PUSH(abs_targets, const char *) = *pcommon;
for (i = 1; i < targets->nelts; ++i) {
const char *rel = APR_ARRAY_IDX(targets, i, const char *);
const char *absolute;
SVN_ERR(svn_path_get_absolute(&absolute, rel, pool));
APR_ARRAY_PUSH(abs_targets, const char *) = absolute;
*pcommon = svn_path_get_longest_ancestor(*pcommon, absolute, pool);
}
if (pcondensed_targets != NULL) {
if (remove_redundancies) {
for (i = 0; i < abs_targets->nelts; ++i) {
if (removed[i])
continue;
for (j = i + 1; j < abs_targets->nelts; ++j) {
const char *abs_targets_i;
const char *abs_targets_j;
const char *ancestor;
if (removed[j])
continue;
abs_targets_i = APR_ARRAY_IDX(abs_targets, i, const char *);
abs_targets_j = APR_ARRAY_IDX(abs_targets, j, const char *);
ancestor = svn_path_get_longest_ancestor
(abs_targets_i, abs_targets_j, pool);
if (*ancestor == '\0')
continue;
if (strcmp(ancestor, abs_targets_i) == 0) {
removed[j] = TRUE;
num_condensed--;
} else if (strcmp(ancestor, abs_targets_j) == 0) {
removed[i] = TRUE;
num_condensed--;
}
}
}
for (i = 0; i < abs_targets->nelts; ++i) {
const char *abs_targets_i = APR_ARRAY_IDX(abs_targets, i,
const char *);
if ((strcmp(abs_targets_i, *pcommon) == 0) && (! removed[i])) {
removed[i] = TRUE;
num_condensed--;
}
}
}
basedir_len = strlen(*pcommon);
*pcondensed_targets = apr_array_make(pool, num_condensed,
sizeof(const char *));
for (i = 0; i < abs_targets->nelts; ++i) {
const char *rel_item = APR_ARRAY_IDX(abs_targets, i, const char *);
if (removed[i])
continue;
if (basedir_len > 0) {
rel_item += basedir_len;
if (rel_item[0] &&
! svn_dirent_is_root(*pcommon, basedir_len))
rel_item++;
}
APR_ARRAY_PUSH(*pcondensed_targets, const char *)
= apr_pstrdup(pool, rel_item);
}
}
return SVN_NO_ERROR;
}
svn_error_t *
svn_path_remove_redundancies(apr_array_header_t **pcondensed_targets,
const apr_array_header_t *targets,
apr_pool_t *pool) {
apr_pool_t *temp_pool;
apr_array_header_t *abs_targets;
apr_array_header_t *rel_targets;
int i;
if ((targets->nelts <= 0) || (! pcondensed_targets)) {
if (pcondensed_targets)
*pcondensed_targets = NULL;
return SVN_NO_ERROR;
}
temp_pool = svn_pool_create(pool);
abs_targets = apr_array_make(temp_pool, targets->nelts,
sizeof(const char *));
rel_targets = apr_array_make(pool, targets->nelts,
sizeof(const char *));
for (i = 0; i < targets->nelts; i++) {
const char *rel_path = APR_ARRAY_IDX(targets, i, const char *);
const char *abs_path;
int j;
svn_boolean_t keep_me;
SVN_ERR(svn_path_get_absolute(&abs_path, rel_path, temp_pool));
keep_me = TRUE;
for (j = 0; j < abs_targets->nelts; j++) {
const char *keeper = APR_ARRAY_IDX(abs_targets, j, const char *);
if (strcmp(keeper, abs_path) == 0) {
keep_me = FALSE;
break;
}
if (svn_path_is_child(keeper, abs_path, temp_pool)) {
keep_me = FALSE;
break;
}
}
if (keep_me) {
APR_ARRAY_PUSH(abs_targets, const char *) = abs_path;
APR_ARRAY_PUSH(rel_targets, const char *) = rel_path;
}
}
svn_pool_destroy(temp_pool);
*pcondensed_targets = rel_targets;
return SVN_NO_ERROR;
}
