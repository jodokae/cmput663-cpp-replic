#include "svn_path.h"
#include "svn_types.h"
#include "svn_error.h"
#include "svn_error_codes.h"
#include "svn_fs.h"
#include "svn_repos.h"
#include "svn_pools.h"
#include "svn_md5.h"
#include "svn_props.h"
#include "repos.h"
#include "svn_private_config.h"
#define NUM_CACHED_SOURCE_ROOTS 4
typedef struct path_info_t {
const char *path;
const char *link_path;
svn_revnum_t rev;
svn_depth_t depth;
svn_boolean_t start_empty;
const char *lock_token;
apr_pool_t *pool;
} path_info_t;
typedef struct report_baton_t {
svn_repos_t *repos;
const char *fs_base;
const char *s_operand;
svn_revnum_t t_rev;
const char *t_path;
svn_boolean_t text_deltas;
svn_depth_t requested_depth;
svn_boolean_t ignore_ancestry;
svn_boolean_t send_copyfrom_args;
svn_boolean_t is_switch;
const svn_delta_editor_t *editor;
void *edit_baton;
svn_repos_authz_func_t authz_read_func;
void *authz_read_baton;
apr_file_t *tempfile;
path_info_t *lookahead;
svn_fs_root_t *t_root;
svn_fs_root_t *s_roots[NUM_CACHED_SOURCE_ROOTS];
apr_pool_t *pool;
} report_baton_t;
typedef svn_error_t *proplist_change_fn_t(report_baton_t *b, void *object,
const char *name,
const svn_string_t *value,
apr_pool_t *pool);
static svn_error_t *delta_dirs(report_baton_t *b, svn_revnum_t s_rev,
const char *s_path, const char *t_path,
void *dir_baton, const char *e_path,
svn_boolean_t start_empty,
svn_depth_t wc_depth,
svn_depth_t requested_depth,
apr_pool_t *pool);
static svn_error_t *
read_number(apr_uint64_t *num, apr_file_t *temp, apr_pool_t *pool) {
char c;
*num = 0;
while (1) {
SVN_ERR(svn_io_file_getc(&c, temp, pool));
if (c == ':')
break;
*num = *num * 10 + (c - '0');
}
return SVN_NO_ERROR;
}
static svn_error_t *
read_string(const char **str, apr_file_t *temp, apr_pool_t *pool) {
apr_uint64_t len;
char *buf;
SVN_ERR(read_number(&len, temp, pool));
if (len + 1 < len) {
return svn_error_createf(SVN_ERR_REPOS_BAD_REVISION_REPORT, NULL,
apr_psprintf(pool,
_("Invalid length (%%%s) when "
"about to read a string"),
APR_UINT64_T_FMT),
len);
}
buf = apr_palloc(pool, len + 1);
SVN_ERR(svn_io_file_read_full(temp, buf, len, NULL, pool));
buf[len] = 0;
*str = buf;
return SVN_NO_ERROR;
}
static svn_error_t *
read_rev(svn_revnum_t *rev, apr_file_t *temp, apr_pool_t *pool) {
char c;
apr_uint64_t num;
SVN_ERR(svn_io_file_getc(&c, temp, pool));
if (c == '+') {
SVN_ERR(read_number(&num, temp, pool));
*rev = (svn_revnum_t) num;
} else
*rev = SVN_INVALID_REVNUM;
return SVN_NO_ERROR;
}
static svn_error_t *
read_depth(svn_depth_t *depth, apr_file_t *temp, const char *path,
apr_pool_t *pool) {
char c;
SVN_ERR(svn_io_file_getc(&c, temp, pool));
switch (c) {
case 'X':
*depth = svn_depth_exclude;
break;
case 'E':
*depth = svn_depth_empty;
break;
case 'F':
*depth = svn_depth_files;
break;
case 'M':
*depth = svn_depth_immediates;
break;
default:
return svn_error_createf(SVN_ERR_REPOS_BAD_REVISION_REPORT, NULL,
_("Invalid depth (%c) for path '%s'"), c, path);
}
return SVN_NO_ERROR;
}
static svn_error_t *
read_path_info(path_info_t **pi, apr_file_t *temp, apr_pool_t *pool) {
char c;
SVN_ERR(svn_io_file_getc(&c, temp, pool));
if (c == '-') {
*pi = NULL;
return SVN_NO_ERROR;
}
*pi = apr_palloc(pool, sizeof(**pi));
SVN_ERR(read_string(&(*pi)->path, temp, pool));
SVN_ERR(svn_io_file_getc(&c, temp, pool));
if (c == '+')
SVN_ERR(read_string(&(*pi)->link_path, temp, pool));
else
(*pi)->link_path = NULL;
SVN_ERR(read_rev(&(*pi)->rev, temp, pool));
SVN_ERR(svn_io_file_getc(&c, temp, pool));
if (c == '+')
SVN_ERR(read_depth(&((*pi)->depth), temp, (*pi)->path, pool));
else
(*pi)->depth = svn_depth_infinity;
SVN_ERR(svn_io_file_getc(&c, temp, pool));
(*pi)->start_empty = (c == '+');
SVN_ERR(svn_io_file_getc(&c, temp, pool));
if (c == '+')
SVN_ERR(read_string(&(*pi)->lock_token, temp, pool));
else
(*pi)->lock_token = NULL;
(*pi)->pool = pool;
return SVN_NO_ERROR;
}
static svn_boolean_t
relevant(path_info_t *pi, const char *prefix, apr_size_t plen) {
return (pi && strncmp(pi->path, prefix, plen) == 0 &&
(!*prefix || pi->path[plen] == '/'));
}
static svn_error_t *
fetch_path_info(report_baton_t *b, const char **entry, path_info_t **info,
const char *prefix, apr_pool_t *pool) {
apr_size_t plen = strlen(prefix);
const char *relpath, *sep;
apr_pool_t *subpool;
if (!relevant(b->lookahead, prefix, plen)) {
*entry = NULL;
*info = NULL;
} else {
relpath = b->lookahead->path + (*prefix ? plen + 1 : 0);
sep = strchr(relpath, '/');
if (sep) {
*entry = apr_pstrmemdup(pool, relpath, sep - relpath);
*info = NULL;
} else {
*entry = relpath;
*info = b->lookahead;
subpool = svn_pool_create(b->pool);
SVN_ERR(read_path_info(&b->lookahead, b->tempfile, subpool));
}
}
return SVN_NO_ERROR;
}
static svn_error_t *
skip_path_info(report_baton_t *b, const char *prefix) {
apr_size_t plen = strlen(prefix);
apr_pool_t *subpool;
while (relevant(b->lookahead, prefix, plen)) {
svn_pool_destroy(b->lookahead->pool);
subpool = svn_pool_create(b->pool);
SVN_ERR(read_path_info(&b->lookahead, b->tempfile, subpool));
}
return SVN_NO_ERROR;
}
static svn_boolean_t
any_path_info(report_baton_t *b, const char *prefix) {
return relevant(b->lookahead, prefix, strlen(prefix));
}
static svn_error_t *
get_source_root(report_baton_t *b, svn_fs_root_t **s_root, svn_revnum_t rev) {
int i;
svn_fs_root_t *root, *prev = NULL;
for (i = 0; i < NUM_CACHED_SOURCE_ROOTS; i++) {
root = b->s_roots[i];
b->s_roots[i] = prev;
if (root && svn_fs_revision_root_revision(root) == rev)
break;
prev = root;
}
if (i == NUM_CACHED_SOURCE_ROOTS) {
if (prev)
svn_fs_close_root(prev);
SVN_ERR(svn_fs_revision_root(&root, b->repos->fs, rev, b->pool));
}
b->s_roots[0] = root;
*s_root = root;
return SVN_NO_ERROR;
}
static svn_error_t *
change_dir_prop(report_baton_t *b, void *dir_baton, const char *name,
const svn_string_t *value, apr_pool_t *pool) {
return b->editor->change_dir_prop(dir_baton, name, value, pool);
}
static svn_error_t *
change_file_prop(report_baton_t *b, void *file_baton, const char *name,
const svn_string_t *value, apr_pool_t *pool) {
return b->editor->change_file_prop(file_baton, name, value, pool);
}
static svn_error_t *
delta_proplists(report_baton_t *b, svn_revnum_t s_rev, const char *s_path,
const char *t_path, const char *lock_token,
proplist_change_fn_t *change_fn,
void *object, apr_pool_t *pool) {
svn_fs_root_t *s_root;
apr_hash_t *s_props, *t_props, *r_props;
apr_array_header_t *prop_diffs;
int i;
svn_revnum_t crev;
const char *uuid;
svn_string_t *cr_str, *cdate, *last_author;
svn_boolean_t changed;
const svn_prop_t *pc;
svn_lock_t *lock;
SVN_ERR(svn_fs_node_created_rev(&crev, b->t_root, t_path, pool));
if (SVN_IS_VALID_REVNUM(crev)) {
cr_str = svn_string_createf(pool, "%ld", crev);
SVN_ERR(change_fn(b, object,
SVN_PROP_ENTRY_COMMITTED_REV, cr_str, pool));
SVN_ERR(svn_fs_revision_proplist(&r_props, b->repos->fs, crev, pool));
cdate = apr_hash_get(r_props, SVN_PROP_REVISION_DATE,
APR_HASH_KEY_STRING);
if (cdate || s_path)
SVN_ERR(change_fn(b, object, SVN_PROP_ENTRY_COMMITTED_DATE,
cdate, pool));
last_author = apr_hash_get(r_props, SVN_PROP_REVISION_AUTHOR,
APR_HASH_KEY_STRING);
if (last_author || s_path)
SVN_ERR(change_fn(b, object, SVN_PROP_ENTRY_LAST_AUTHOR,
last_author, pool));
SVN_ERR(svn_fs_get_uuid(b->repos->fs, &uuid, pool));
SVN_ERR(change_fn(b, object, SVN_PROP_ENTRY_UUID,
svn_string_create(uuid, pool), pool));
}
if (lock_token) {
SVN_ERR(svn_fs_get_lock(&lock, b->repos->fs, t_path, pool));
if (! lock || strcmp(lock_token, lock->token) != 0)
SVN_ERR(change_fn(b, object, SVN_PROP_ENTRY_LOCK_TOKEN,
NULL, pool));
}
if (s_path) {
SVN_ERR(get_source_root(b, &s_root, s_rev));
SVN_ERR(svn_fs_props_changed(&changed, b->t_root, t_path, s_root,
s_path, pool));
if (! changed)
return SVN_NO_ERROR;
SVN_ERR(svn_fs_node_proplist(&s_props, s_root, s_path, pool));
} else
s_props = apr_hash_make(pool);
SVN_ERR(svn_fs_node_proplist(&t_props, b->t_root, t_path, pool));
SVN_ERR(svn_prop_diffs(&prop_diffs, t_props, s_props, pool));
for (i = 0; i < prop_diffs->nelts; i++) {
pc = &APR_ARRAY_IDX(prop_diffs, i, svn_prop_t);
SVN_ERR(change_fn(b, object, pc->name, pc->value, pool));
}
return SVN_NO_ERROR;
}
static svn_error_t *
delta_files(report_baton_t *b, void *file_baton, svn_revnum_t s_rev,
const char *s_path, const char *t_path, const char *lock_token,
apr_pool_t *pool) {
svn_boolean_t changed;
svn_fs_root_t *s_root = NULL;
svn_txdelta_stream_t *dstream = NULL;
unsigned char s_digest[APR_MD5_DIGESTSIZE];
const char *s_hex_digest = NULL;
svn_txdelta_window_handler_t dhandler;
void *dbaton;
SVN_ERR(delta_proplists(b, s_rev, s_path, t_path, lock_token,
change_file_prop, file_baton, pool));
if (s_path) {
SVN_ERR(get_source_root(b, &s_root, s_rev));
if (b->ignore_ancestry)
SVN_ERR(svn_repos__compare_files(&changed, b->t_root, t_path,
s_root, s_path, pool));
else
SVN_ERR(svn_fs_contents_changed(&changed, b->t_root, t_path, s_root,
s_path, pool));
if (!changed)
return SVN_NO_ERROR;
SVN_ERR(svn_fs_file_md5_checksum(s_digest, s_root, s_path, pool));
s_hex_digest = svn_md5_digest_to_cstring(s_digest, pool);
}
SVN_ERR(b->editor->apply_textdelta(file_baton, s_hex_digest, pool,
&dhandler, &dbaton));
if (b->text_deltas) {
SVN_ERR(svn_fs_get_file_delta_stream(&dstream, s_root, s_path,
b->t_root, t_path, pool));
return svn_txdelta_send_txstream(dstream, dhandler, dbaton, pool);
} else
return dhandler(NULL, dbaton);
}
static svn_error_t *
check_auth(report_baton_t *b, svn_boolean_t *allowed, const char *path,
apr_pool_t *pool) {
if (b->authz_read_func)
return b->authz_read_func(allowed, b->t_root, path,
b->authz_read_baton, pool);
*allowed = TRUE;
return SVN_NO_ERROR;
}
static svn_error_t *
fake_dirent(const svn_fs_dirent_t **entry, svn_fs_root_t *root,
const char *path, apr_pool_t *pool) {
svn_node_kind_t kind;
svn_fs_dirent_t *ent;
SVN_ERR(svn_fs_check_path(&kind, root, path, pool));
if (kind == svn_node_none)
*entry = NULL;
else {
ent = apr_palloc(pool, sizeof(**entry));
ent->name = svn_path_basename(path, pool);
SVN_ERR(svn_fs_node_id(&ent->id, root, path, pool));
ent->kind = kind;
*entry = ent;
}
return SVN_NO_ERROR;
}
static svn_boolean_t
is_depth_upgrade(svn_depth_t wc_depth,
svn_depth_t requested_depth,
svn_node_kind_t kind) {
if (requested_depth == svn_depth_unknown
|| requested_depth <= wc_depth
|| wc_depth == svn_depth_immediates)
return FALSE;
if (kind == svn_node_file
&& wc_depth == svn_depth_files)
return FALSE;
if (kind == svn_node_dir
&& wc_depth == svn_depth_empty
&& requested_depth == svn_depth_files)
return FALSE;
return TRUE;
}
static svn_error_t *
add_file_smartly(report_baton_t *b,
const char *path,
void *parent_baton,
const char *o_path,
void **new_file_baton,
const char **copyfrom_path,
svn_revnum_t *copyfrom_rev,
apr_pool_t *pool) {
svn_fs_t *fs = svn_repos_fs(b->repos);
svn_fs_root_t *closest_copy_root = NULL;
const char *closest_copy_path = NULL;
*copyfrom_path = NULL;
*copyfrom_rev = SVN_INVALID_REVNUM;
if (b->send_copyfrom_args) {
if (*o_path != '/')
o_path = apr_pstrcat(pool, "/", o_path, NULL);
SVN_ERR(svn_fs_closest_copy(&closest_copy_root, &closest_copy_path,
b->t_root, o_path, pool));
if (closest_copy_root != NULL) {
if (strcmp(closest_copy_path, o_path) == 0) {
SVN_ERR(svn_fs_copied_from(copyfrom_rev, copyfrom_path,
closest_copy_root, closest_copy_path,
pool));
if (b->authz_read_func) {
svn_boolean_t allowed;
svn_fs_root_t *copyfrom_root;
SVN_ERR(svn_fs_revision_root(&copyfrom_root, fs,
*copyfrom_rev, pool));
SVN_ERR(b->authz_read_func(&allowed, copyfrom_root,
*copyfrom_path, b->authz_read_baton,
pool));
if (! allowed) {
*copyfrom_path = NULL;
*copyfrom_rev = SVN_INVALID_REVNUM;
}
}
}
}
}
SVN_ERR(b->editor->add_file(path, parent_baton,
*copyfrom_path, *copyfrom_rev,
pool, new_file_baton));
return SVN_NO_ERROR;
}
static svn_error_t *
update_entry(report_baton_t *b, svn_revnum_t s_rev, const char *s_path,
const svn_fs_dirent_t *s_entry, const char *t_path,
const svn_fs_dirent_t *t_entry, void *dir_baton,
const char *e_path, path_info_t *info, svn_depth_t wc_depth,
svn_depth_t requested_depth, apr_pool_t *pool) {
svn_fs_root_t *s_root;
svn_boolean_t allowed, related;
void *new_baton;
unsigned char digest[APR_MD5_DIGESTSIZE];
const char *hex_digest;
int distance;
if (info && info->link_path && !b->is_switch) {
t_path = info->link_path;
SVN_ERR(fake_dirent(&t_entry, b->t_root, t_path, pool));
}
if (info && !SVN_IS_VALID_REVNUM(info->rev)) {
s_path = NULL;
s_entry = NULL;
} else if (info && s_path) {
s_path = (info->link_path) ? info->link_path : s_path;
s_rev = info->rev;
SVN_ERR(get_source_root(b, &s_root, s_rev));
SVN_ERR(fake_dirent(&s_entry, s_root, s_path, pool));
}
if (s_path && !s_entry)
return svn_error_createf(SVN_ERR_FS_NOT_FOUND, NULL,
_("Working copy path '%s' does not exist in "
"repository"), e_path);
related = FALSE;
if (s_entry && t_entry && s_entry->kind == t_entry->kind) {
distance = svn_fs_compare_ids(s_entry->id, t_entry->id);
if (distance == 0 && !any_path_info(b, e_path)
&& (!info || (!info->start_empty && !info->lock_token))
&& (requested_depth <= wc_depth || t_entry->kind == svn_node_file))
return SVN_NO_ERROR;
else if (distance != -1 || b->ignore_ancestry)
related = TRUE;
}
if (s_entry && !related) {
svn_revnum_t deleted_rev;
SVN_ERR(svn_repos_deleted_rev(svn_fs_root_fs(b->t_root), t_path,
s_rev, b->t_rev, &deleted_rev,
pool));
SVN_ERR(b->editor->delete_entry(e_path, deleted_rev, dir_baton,
pool));
s_path = NULL;
}
if (!t_entry)
return skip_path_info(b, e_path);
SVN_ERR(check_auth(b, &allowed, t_path, pool));
if (!allowed) {
if (t_entry->kind == svn_node_dir)
SVN_ERR(b->editor->absent_directory(e_path, dir_baton, pool));
else
SVN_ERR(b->editor->absent_file(e_path, dir_baton, pool));
return skip_path_info(b, e_path);
}
if (t_entry->kind == svn_node_dir) {
if (related)
SVN_ERR(b->editor->open_directory(e_path, dir_baton, s_rev, pool,
&new_baton));
else
SVN_ERR(b->editor->add_directory(e_path, dir_baton, NULL,
SVN_INVALID_REVNUM, pool,
&new_baton));
SVN_ERR(delta_dirs(b, s_rev, s_path, t_path, new_baton, e_path,
info ? info->start_empty : FALSE,
wc_depth, requested_depth, pool));
return b->editor->close_directory(new_baton, pool);
} else {
if (related) {
SVN_ERR(b->editor->open_file(e_path, dir_baton, s_rev, pool,
&new_baton));
SVN_ERR(delta_files(b, new_baton, s_rev, s_path, t_path,
info ? info->lock_token : NULL, pool));
} else {
svn_revnum_t copyfrom_rev = SVN_INVALID_REVNUM;
const char *copyfrom_path = NULL;
SVN_ERR(add_file_smartly(b, e_path, dir_baton, t_path, &new_baton,
&copyfrom_path, &copyfrom_rev, pool));
if (! copyfrom_path)
SVN_ERR(delta_files(b, new_baton, s_rev, s_path, t_path,
info ? info->lock_token : NULL, pool));
else
SVN_ERR(delta_files(b, new_baton, copyfrom_rev, copyfrom_path,
t_path, info ? info->lock_token : NULL, pool));
}
SVN_ERR(svn_fs_file_md5_checksum(digest, b->t_root, t_path, pool));
hex_digest = svn_md5_digest_to_cstring(digest, pool);
return b->editor->close_file(new_baton, hex_digest, pool);
}
}
#define DEPTH_BELOW_HERE(depth) ((depth) == svn_depth_immediates) ? svn_depth_empty : (depth)
static svn_error_t *
delta_dirs(report_baton_t *b, svn_revnum_t s_rev, const char *s_path,
const char *t_path, void *dir_baton, const char *e_path,
svn_boolean_t start_empty, svn_depth_t wc_depth,
svn_depth_t requested_depth, apr_pool_t *pool) {
svn_fs_root_t *s_root;
apr_hash_t *s_entries = NULL, *t_entries;
apr_hash_index_t *hi;
apr_pool_t *subpool;
const svn_fs_dirent_t *s_entry, *t_entry;
void *val;
const char *name, *s_fullpath, *t_fullpath, *e_fullpath;
path_info_t *info;
SVN_ERR(delta_proplists(b, s_rev, start_empty ? NULL : s_path, t_path,
NULL, change_dir_prop, dir_baton, pool));
if (requested_depth > svn_depth_empty
|| requested_depth == svn_depth_unknown) {
if (s_path && !start_empty) {
SVN_ERR(get_source_root(b, &s_root, s_rev));
SVN_ERR(svn_fs_dir_entries(&s_entries, s_root, s_path, pool));
}
SVN_ERR(svn_fs_dir_entries(&t_entries, b->t_root, t_path, pool));
subpool = svn_pool_create(pool);
while (1) {
svn_pool_clear(subpool);
SVN_ERR(fetch_path_info(b, &name, &info, e_path, subpool));
if (!name)
break;
if (info
&& !SVN_IS_VALID_REVNUM(info->rev)
&& info->depth != svn_depth_exclude) {
if (s_entries)
apr_hash_set(s_entries, name, APR_HASH_KEY_STRING, NULL);
continue;
}
e_fullpath = svn_path_join(e_path, name, subpool);
t_fullpath = svn_path_join(t_path, name, subpool);
t_entry = apr_hash_get(t_entries, name, APR_HASH_KEY_STRING);
s_fullpath = s_path ? svn_path_join(s_path, name, subpool) : NULL;
s_entry = s_entries ?
apr_hash_get(s_entries, name, APR_HASH_KEY_STRING) : NULL;
if ((! info || info->depth != svn_depth_exclude)
&& (requested_depth != svn_depth_files
|| ((! t_entry || t_entry->kind != svn_node_dir)
&& (! s_entry || s_entry->kind != svn_node_dir))))
SVN_ERR(update_entry(b, s_rev, s_fullpath, s_entry, t_fullpath,
t_entry, dir_baton, e_fullpath, info,
info ? info->depth
: DEPTH_BELOW_HERE(wc_depth),
DEPTH_BELOW_HERE(requested_depth), subpool));
apr_hash_set(t_entries, name, APR_HASH_KEY_STRING, NULL);
if (s_entries)
apr_hash_set(s_entries, name, APR_HASH_KEY_STRING, NULL);
if (info)
svn_pool_destroy(info->pool);
}
if (s_entries) {
for (hi = apr_hash_first(pool, s_entries);
hi;
hi = apr_hash_next(hi)) {
svn_pool_clear(subpool);
apr_hash_this(hi, NULL, NULL, &val);
s_entry = val;
if (apr_hash_get(t_entries, s_entry->name,
APR_HASH_KEY_STRING) == NULL) {
svn_revnum_t deleted_rev;
if (s_entry->kind == svn_node_file
&& wc_depth < svn_depth_files)
continue;
if (s_entry->kind == svn_node_dir
&& (wc_depth < svn_depth_immediates
|| requested_depth == svn_depth_files))
continue;
e_fullpath = svn_path_join(e_path, s_entry->name, subpool);
SVN_ERR(svn_repos_deleted_rev(svn_fs_root_fs(b->t_root),
svn_path_join(t_path,
s_entry->name,
subpool),
s_rev, b->t_rev,
&deleted_rev, subpool));
SVN_ERR(b->editor->delete_entry(e_fullpath,
deleted_rev,
dir_baton, subpool));
}
}
}
for (hi = apr_hash_first(pool, t_entries); hi; hi = apr_hash_next(hi)) {
svn_pool_clear(subpool);
apr_hash_this(hi, NULL, NULL, &val);
t_entry = val;
if (is_depth_upgrade(wc_depth, requested_depth, t_entry->kind)) {
s_entry = NULL;
s_fullpath = NULL;
} else {
if (t_entry->kind == svn_node_file
&& requested_depth == svn_depth_unknown
&& wc_depth < svn_depth_files)
continue;
if (t_entry->kind == svn_node_dir
&& (wc_depth < svn_depth_immediates
|| requested_depth == svn_depth_files))
continue;
s_entry = s_entries ?
apr_hash_get(s_entries, t_entry->name, APR_HASH_KEY_STRING)
: NULL;
s_fullpath = s_entry ?
svn_path_join(s_path, t_entry->name, subpool) : NULL;
}
e_fullpath = svn_path_join(e_path, t_entry->name, subpool);
t_fullpath = svn_path_join(t_path, t_entry->name, subpool);
SVN_ERR(update_entry(b, s_rev, s_fullpath, s_entry, t_fullpath,
t_entry, dir_baton, e_fullpath, NULL,
DEPTH_BELOW_HERE(wc_depth),
DEPTH_BELOW_HERE(requested_depth),
subpool));
}
svn_pool_destroy(subpool);
}
return SVN_NO_ERROR;
}
static svn_error_t *
drive(report_baton_t *b, svn_revnum_t s_rev, path_info_t *info,
apr_pool_t *pool) {
const char *t_anchor, *s_fullpath;
svn_boolean_t allowed, info_is_set_path;
svn_fs_root_t *s_root;
const svn_fs_dirent_t *s_entry, *t_entry;
void *root_baton;
t_anchor = *b->s_operand ? svn_path_dirname(b->t_path, pool) : b->t_path;
SVN_ERR(check_auth(b, &allowed, t_anchor, pool));
if (!allowed)
return svn_error_create
(SVN_ERR_AUTHZ_ROOT_UNREADABLE, NULL,
_("Not authorized to open root of edit operation"));
SVN_ERR(b->editor->set_target_revision(b->edit_baton, b->t_rev, pool));
s_fullpath = svn_path_join(b->fs_base, b->s_operand, pool);
SVN_ERR(get_source_root(b, &s_root, s_rev));
SVN_ERR(fake_dirent(&s_entry, s_root, s_fullpath, pool));
SVN_ERR(fake_dirent(&t_entry, b->t_root, b->t_path, pool));
info_is_set_path = (SVN_IS_VALID_REVNUM(info->rev) && !info->link_path);
if (info_is_set_path && !s_entry)
s_fullpath = NULL;
if (!*b->s_operand && !(t_entry))
return svn_error_create(SVN_ERR_FS_PATH_SYNTAX, NULL,
_("Target path does not exist"));
else if (!*b->s_operand && (!s_entry || s_entry->kind != svn_node_dir
|| t_entry->kind != svn_node_dir))
return svn_error_create(SVN_ERR_FS_PATH_SYNTAX, NULL,
_("Cannot replace a directory from within"));
SVN_ERR(b->editor->open_root(b->edit_baton, s_rev, pool, &root_baton));
if (!*b->s_operand)
SVN_ERR(delta_dirs(b, s_rev, s_fullpath, b->t_path, root_baton,
"", info->start_empty, info->depth, b->requested_depth,
pool));
else
SVN_ERR(update_entry(b, s_rev, s_fullpath, s_entry, b->t_path,
t_entry, root_baton, b->s_operand, info,
info->depth, b->requested_depth, pool));
SVN_ERR(b->editor->close_directory(root_baton, pool));
SVN_ERR(b->editor->close_edit(b->edit_baton, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
finish_report(report_baton_t *b, apr_pool_t *pool) {
apr_off_t offset;
path_info_t *info;
apr_pool_t *subpool;
svn_revnum_t s_rev;
int i;
b->pool = pool;
SVN_ERR(svn_io_file_write_full(b->tempfile, "-", 1, NULL, pool));
offset = 0;
SVN_ERR(svn_io_file_seek(b->tempfile, APR_SET, &offset, pool));
SVN_ERR(read_path_info(&info, b->tempfile, pool));
if (!info || strcmp(info->path, b->s_operand) != 0
|| info->link_path || !SVN_IS_VALID_REVNUM(info->rev))
return svn_error_create(SVN_ERR_REPOS_BAD_REVISION_REPORT, NULL,
_("Invalid report for top level of working copy"));
s_rev = info->rev;
subpool = svn_pool_create(pool);
SVN_ERR(read_path_info(&b->lookahead, b->tempfile, subpool));
if (b->lookahead && strcmp(b->lookahead->path, b->s_operand) == 0) {
if (!*b->s_operand)
return svn_error_create(SVN_ERR_REPOS_BAD_REVISION_REPORT, NULL,
_("Two top-level reports with no target"));
if (! SVN_IS_VALID_REVNUM(b->lookahead->rev)) {
b->lookahead->depth = info->depth;
}
info = b->lookahead;
SVN_ERR(read_path_info(&b->lookahead, b->tempfile, subpool));
}
SVN_ERR(svn_fs_revision_root(&b->t_root, b->repos->fs, b->t_rev, pool));
for (i = 0; i < NUM_CACHED_SOURCE_ROOTS; i++)
b->s_roots[i] = NULL;
return drive(b, s_rev, info, pool);
}
static svn_error_t *
write_path_info(report_baton_t *b, const char *path, const char *lpath,
svn_revnum_t rev, svn_depth_t depth,
svn_boolean_t start_empty,
const char *lock_token, apr_pool_t *pool) {
const char *lrep, *rrep, *drep, *ltrep, *rep;
path = svn_path_join(b->s_operand, path, pool);
lrep = lpath ? apr_psprintf(pool, "+%" APR_SIZE_T_FMT ":%s",
strlen(lpath), lpath) : "-";
rrep = (SVN_IS_VALID_REVNUM(rev)) ?
apr_psprintf(pool, "+%ld:", rev) : "-";
if (depth == svn_depth_exclude)
drep = "+X";
else if (depth == svn_depth_empty)
drep = "+E";
else if (depth == svn_depth_files)
drep = "+F";
else if (depth == svn_depth_immediates)
drep = "+M";
else if (depth == svn_depth_infinity)
drep = "-";
else
return svn_error_createf(SVN_ERR_REPOS_BAD_ARGS, NULL,
_("Unsupported report depth '%s'"),
svn_depth_to_word(depth));
ltrep = lock_token ? apr_psprintf(pool, "+%" APR_SIZE_T_FMT ":%s",
strlen(lock_token), lock_token) : "-";
rep = apr_psprintf(pool, "+%" APR_SIZE_T_FMT ":%s%s%s%s%c%s",
strlen(path), path, lrep, rrep, drep,
start_empty ? '+' : '-', ltrep);
return svn_io_file_write_full(b->tempfile, rep, strlen(rep), NULL, pool);
}
svn_error_t *
svn_repos_set_path3(void *baton, const char *path, svn_revnum_t rev,
svn_depth_t depth, svn_boolean_t start_empty,
const char *lock_token, apr_pool_t *pool) {
return write_path_info(baton, path, NULL, rev, depth, start_empty,
lock_token, pool);
}
svn_error_t *
svn_repos_set_path2(void *baton, const char *path, svn_revnum_t rev,
svn_boolean_t start_empty, const char *lock_token,
apr_pool_t *pool) {
return svn_repos_set_path3(baton, path, rev, svn_depth_infinity,
start_empty, lock_token, pool);
}
svn_error_t *
svn_repos_set_path(void *baton, const char *path, svn_revnum_t rev,
svn_boolean_t start_empty, apr_pool_t *pool) {
return svn_repos_set_path2(baton, path, rev, start_empty, NULL, pool);
}
svn_error_t *
svn_repos_link_path3(void *baton, const char *path, const char *link_path,
svn_revnum_t rev, svn_depth_t depth,
svn_boolean_t start_empty,
const char *lock_token, apr_pool_t *pool) {
if (depth == svn_depth_exclude)
return svn_error_create(SVN_ERR_REPOS_BAD_ARGS, NULL,
_("Depth 'exclude' not supported for link"));
return write_path_info(baton, path, link_path, rev, depth,
start_empty, lock_token, pool);
}
svn_error_t *
svn_repos_link_path2(void *baton, const char *path, const char *link_path,
svn_revnum_t rev, svn_boolean_t start_empty,
const char *lock_token, apr_pool_t *pool) {
return svn_repos_link_path3(baton, path, link_path, rev, svn_depth_infinity,
start_empty, lock_token, pool);
}
svn_error_t *
svn_repos_link_path(void *baton, const char *path, const char *link_path,
svn_revnum_t rev, svn_boolean_t start_empty,
apr_pool_t *pool) {
return svn_repos_link_path2(baton, path, link_path, rev, start_empty,
NULL, pool);
}
svn_error_t *
svn_repos_delete_path(void *baton, const char *path, apr_pool_t *pool) {
return write_path_info(baton, path, NULL, SVN_INVALID_REVNUM,
svn_depth_infinity, FALSE, NULL, pool);
}
svn_error_t *
svn_repos_finish_report(void *baton, apr_pool_t *pool) {
report_baton_t *b = baton;
svn_error_t *finish_err, *close_err;
finish_err = finish_report(b, pool);
close_err = svn_io_file_close(b->tempfile, pool);
if (finish_err)
svn_error_clear(close_err);
return finish_err ? finish_err : close_err;
}
svn_error_t *
svn_repos_abort_report(void *baton, apr_pool_t *pool) {
report_baton_t *b = baton;
return svn_io_file_close(b->tempfile, pool);
}
svn_error_t *
svn_repos_begin_report2(void **report_baton,
svn_revnum_t revnum,
svn_repos_t *repos,
const char *fs_base,
const char *s_operand,
const char *switch_path,
svn_boolean_t text_deltas,
svn_depth_t depth,
svn_boolean_t ignore_ancestry,
svn_boolean_t send_copyfrom_args,
const svn_delta_editor_t *editor,
void *edit_baton,
svn_repos_authz_func_t authz_read_func,
void *authz_read_baton,
apr_pool_t *pool) {
report_baton_t *b;
const char *tempdir;
if (depth == svn_depth_exclude)
return svn_error_create(SVN_ERR_REPOS_BAD_ARGS, NULL,
_("Request depth 'exclude' not supported"));
b = apr_palloc(pool, sizeof(*b));
b->repos = repos;
b->fs_base = apr_pstrdup(pool, fs_base);
b->s_operand = apr_pstrdup(pool, s_operand);
b->t_rev = revnum;
b->t_path = switch_path ? switch_path
: svn_path_join(fs_base, s_operand, pool);
b->text_deltas = text_deltas;
b->requested_depth = depth;
b->ignore_ancestry = ignore_ancestry;
b->send_copyfrom_args = send_copyfrom_args;
b->is_switch = (switch_path != NULL);
b->editor = editor;
b->edit_baton = edit_baton;
b->authz_read_func = authz_read_func;
b->authz_read_baton = authz_read_baton;
SVN_ERR(svn_io_temp_dir(&tempdir, pool));
SVN_ERR(svn_io_open_unique_file2(&b->tempfile, NULL,
apr_psprintf(pool, "%s/report", tempdir),
".tmp", svn_io_file_del_on_close, pool));
*report_baton = b;
return SVN_NO_ERROR;
}
svn_error_t *
svn_repos_begin_report(void **report_baton,
svn_revnum_t revnum,
const char *username,
svn_repos_t *repos,
const char *fs_base,
const char *s_operand,
const char *switch_path,
svn_boolean_t text_deltas,
svn_boolean_t recurse,
svn_boolean_t ignore_ancestry,
const svn_delta_editor_t *editor,
void *edit_baton,
svn_repos_authz_func_t authz_read_func,
void *authz_read_baton,
apr_pool_t *pool) {
return svn_repos_begin_report2(report_baton,
revnum,
repos,
fs_base,
s_operand,
switch_path,
text_deltas,
SVN_DEPTH_INFINITY_OR_FILES(recurse),
ignore_ancestry,
FALSE,
editor,
edit_baton,
authz_read_func,
authz_read_baton,
pool);
}
