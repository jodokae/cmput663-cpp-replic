#include <assert.h>
#include <apr_pools.h>
#include <apr_strings.h>
#include "svn_types.h"
#include "svn_delta.h"
#include "svn_pools.h"
#include "svn_path.h"
#include "svn_sorts.h"
typedef struct dir_stack_t {
void *dir_baton;
apr_pool_t *pool;
} dir_stack_t;
static svn_error_t *
open_dir(apr_array_header_t *db_stack,
const svn_delta_editor_t *editor,
const char *path,
svn_revnum_t revision,
apr_pool_t *pool) {
void *parent_db, *db;
dir_stack_t *item;
apr_pool_t *subpool;
assert(db_stack && db_stack->nelts);
item = APR_ARRAY_IDX(db_stack, db_stack->nelts - 1, void *);
parent_db = item->dir_baton;
subpool = svn_pool_create(pool);
SVN_ERR(editor->open_directory(path, parent_db, revision, subpool, &db));
item = apr_pcalloc(subpool, sizeof(*item));
item->dir_baton = db;
item->pool = subpool;
APR_ARRAY_PUSH(db_stack, dir_stack_t *) = item;
return SVN_NO_ERROR;
}
static svn_error_t *
pop_stack(apr_array_header_t *db_stack,
const svn_delta_editor_t *editor) {
dir_stack_t *item;
assert(db_stack && db_stack->nelts);
item = APR_ARRAY_IDX(db_stack, db_stack->nelts - 1, dir_stack_t *);
(void) apr_array_pop(db_stack);
SVN_ERR(editor->close_directory(item->dir_baton, item->pool));
svn_pool_destroy(item->pool);
return SVN_NO_ERROR;
}
static int
count_components(const char *path) {
int count = 1;
const char *instance = path;
if ((strlen(path) == 1) && (path[0] == '/'))
return 0;
do {
instance++;
instance = strchr(instance, '/');
if (instance)
count++;
} while (instance);
return count;
}
svn_error_t *
svn_delta_path_driver(const svn_delta_editor_t *editor,
void *edit_baton,
svn_revnum_t revision,
apr_array_header_t *paths,
svn_delta_path_driver_cb_func_t callback_func,
void *callback_baton,
apr_pool_t *pool) {
apr_array_header_t *db_stack = apr_array_make(pool, 4, sizeof(void *));
const char *last_path = NULL;
int i = 0;
void *parent_db = NULL, *db = NULL;
const char *path;
apr_pool_t *subpool, *iterpool;
dir_stack_t *item;
if (! paths->nelts)
return SVN_NO_ERROR;
subpool = svn_pool_create(pool);
iterpool = svn_pool_create(pool);
item = apr_pcalloc(subpool, sizeof(*item));
qsort(paths->elts, paths->nelts, paths->elt_size, svn_sort_compare_paths);
path = APR_ARRAY_IDX(paths, 0, const char *);
if (svn_path_is_empty(path)) {
SVN_ERR(callback_func(&db, NULL, callback_baton, path, subpool));
last_path = path;
i++;
} else {
SVN_ERR(editor->open_root(edit_baton, revision, subpool, &db));
}
item->pool = subpool;
item->dir_baton = db;
APR_ARRAY_PUSH(db_stack, void *) = item;
for (; i < paths->nelts; i++) {
const char *pdir, *bname;
const char *common = "";
size_t common_len;
svn_pool_clear(iterpool);
path = APR_ARRAY_IDX(paths, i, const char *);
if (i > 0)
common = svn_path_get_longest_ancestor(last_path, path, iterpool);
common_len = strlen(common);
if ((i > 0) && (strlen(last_path) > common_len)) {
const char *rel = last_path + (common_len ? (common_len + 1) : 0);
int count = count_components(rel);
while (count--) {
SVN_ERR(pop_stack(db_stack, editor));
}
}
svn_path_split(path, &pdir, &bname, iterpool);
if (strlen(pdir) > common_len) {
const char *piece = pdir + common_len + 1;
while (1) {
const char *rel = pdir;
piece = strchr(piece, '/');
if (piece)
rel = apr_pstrmemdup(iterpool, pdir, piece - pdir);
SVN_ERR(open_dir(db_stack, editor, rel, revision, pool));
if (piece)
piece++;
else
break;
}
}
item = APR_ARRAY_IDX(db_stack, db_stack->nelts - 1, void *);
parent_db = item->dir_baton;
subpool = svn_pool_create(pool);
SVN_ERR(callback_func(&db, parent_db, callback_baton, path, subpool));
if (db) {
item = apr_pcalloc(subpool, sizeof(*item));
item->dir_baton = db;
item->pool = subpool;
APR_ARRAY_PUSH(db_stack, void *) = item;
} else {
svn_pool_destroy(subpool);
}
if (db)
last_path = path;
else
last_path = apr_pstrdup(pool, pdir);
}
svn_pool_destroy(iterpool);
while (db_stack->nelts) {
SVN_ERR(pop_stack(db_stack, editor));
}
return SVN_NO_ERROR;
}
