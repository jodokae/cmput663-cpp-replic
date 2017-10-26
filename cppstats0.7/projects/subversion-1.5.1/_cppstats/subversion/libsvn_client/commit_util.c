#include <string.h>
#include <apr_pools.h>
#include <apr_hash.h>
#include "client.h"
#include "svn_path.h"
#include "svn_types.h"
#include "svn_pools.h"
#include "svn_props.h"
#include "svn_md5.h"
#include "svn_iter.h"
#include "svn_hash.h"
#include <assert.h>
#include <stdlib.h>
#include "svn_private_config.h"
#include "private/svn_wc_private.h"
static void
add_committable(apr_hash_t *committables,
const char *path,
svn_node_kind_t kind,
const char *url,
svn_revnum_t revision,
const char *copyfrom_url,
svn_revnum_t copyfrom_rev,
apr_byte_t state_flags) {
apr_pool_t *pool = apr_hash_pool_get(committables);
const char *repos_name = SVN_CLIENT__SINGLE_REPOS_NAME;
apr_array_header_t *array;
svn_client_commit_item3_t *new_item;
assert(path && url);
array = apr_hash_get(committables, repos_name, APR_HASH_KEY_STRING);
if (array == NULL) {
array = apr_array_make(pool, 1, sizeof(new_item));
apr_hash_set(committables, repos_name, APR_HASH_KEY_STRING, array);
}
svn_client_commit_item_create((const svn_client_commit_item3_t **) &new_item,
pool);
new_item->path = apr_pstrdup(pool, path);
new_item->kind = kind;
new_item->url = apr_pstrdup(pool, url);
new_item->revision = revision;
new_item->copyfrom_url = copyfrom_url
? apr_pstrdup(pool, copyfrom_url) : NULL;
new_item->copyfrom_rev = copyfrom_rev;
new_item->state_flags = state_flags;
new_item->incoming_prop_changes = apr_array_make(pool, 1,
sizeof(svn_prop_t *));
APR_ARRAY_PUSH(array, svn_client_commit_item3_t *) = new_item;
}
static svn_error_t *
check_prop_mods(svn_boolean_t *props_changed,
svn_boolean_t *eol_prop_changed,
const char *path,
svn_wc_adm_access_t *adm_access,
apr_pool_t *pool) {
apr_array_header_t *prop_mods;
int i;
*eol_prop_changed = *props_changed = FALSE;
SVN_ERR(svn_wc_props_modified_p(props_changed, path, adm_access, pool));
if (! *props_changed)
return SVN_NO_ERROR;
SVN_ERR(svn_wc_get_prop_diffs(&prop_mods, NULL, path, adm_access, pool));
for (i = 0; i < prop_mods->nelts; i++) {
svn_prop_t *prop_mod = &APR_ARRAY_IDX(prop_mods, i, svn_prop_t);
if (strcmp(prop_mod->name, SVN_PROP_EOL_STYLE) == 0)
*eol_prop_changed = TRUE;
}
return SVN_NO_ERROR;
}
static svn_client_commit_item3_t *
look_up_committable(apr_hash_t *committables,
const char *path,
apr_pool_t *pool) {
apr_hash_index_t *hi;
for (hi = apr_hash_first(pool, committables); hi; hi = apr_hash_next(hi)) {
void *val;
apr_array_header_t *these_committables;
int i;
apr_hash_this(hi, NULL, NULL, &val);
these_committables = val;
for (i = 0; i < these_committables->nelts; i++) {
svn_client_commit_item3_t *this_committable
= APR_ARRAY_IDX(these_committables, i,
svn_client_commit_item3_t *);
if (strcmp(this_committable->path, path) == 0)
return this_committable;
}
}
return NULL;
}
static svn_error_t *
add_lock_token(const char *path, const svn_wc_entry_t *entry,
void *walk_baton, apr_pool_t *pool) {
apr_hash_t *lock_tokens = walk_baton;
apr_pool_t *token_pool = apr_hash_pool_get(lock_tokens);
if (entry->url && entry->lock_token)
apr_hash_set(lock_tokens, apr_pstrdup(token_pool, entry->url),
APR_HASH_KEY_STRING,
apr_pstrdup(token_pool, entry->lock_token));
return SVN_NO_ERROR;
}
static svn_wc_entry_callbacks2_t add_tokens_callbacks = {
add_lock_token,
svn_client__default_walker_error_handler
};
static svn_error_t *
harvest_committables(apr_hash_t *committables,
apr_hash_t *lock_tokens,
const char *path,
svn_wc_adm_access_t *adm_access,
const char *url,
const char *copyfrom_url,
const svn_wc_entry_t *entry,
const svn_wc_entry_t *parent_entry,
svn_boolean_t adds_only,
svn_boolean_t copy_mode,
svn_depth_t depth,
svn_boolean_t just_locked,
apr_hash_t *changelists,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
apr_hash_t *entries = NULL;
svn_boolean_t text_mod = FALSE, prop_mod = FALSE;
apr_byte_t state_flags = 0;
svn_node_kind_t kind;
const char *p_path;
svn_boolean_t tc, pc;
const char *cf_url = NULL;
svn_revnum_t cf_rev = entry->copyfrom_rev;
const svn_string_t *propval;
svn_boolean_t is_special;
apr_pool_t *token_pool = (lock_tokens ? apr_hash_pool_get(lock_tokens)
: NULL);
if (look_up_committable(committables, path, pool))
return SVN_NO_ERROR;
assert(entry);
assert(url);
if (ctx->cancel_func)
SVN_ERR(ctx->cancel_func(ctx->cancel_baton));
p_path = svn_path_dirname(path, pool);
if ((entry->kind != svn_node_file) && (entry->kind != svn_node_dir))
return svn_error_createf
(SVN_ERR_NODE_UNKNOWN_KIND, NULL, _("Unknown entry kind for '%s'"),
svn_path_local_style(path, pool));
SVN_ERR(svn_io_check_special_path(path, &kind, &is_special, pool));
if ((kind != svn_node_file)
&& (kind != svn_node_dir)
&& (kind != svn_node_none)) {
return svn_error_createf
(SVN_ERR_NODE_UNKNOWN_KIND, NULL,
_("Unknown entry kind for '%s'"),
svn_path_local_style(path, pool));
}
SVN_ERR(svn_wc_prop_get(&propval, SVN_PROP_SPECIAL, path, adm_access,
pool));
if ((((! propval) && (is_special))
#if defined(HAVE_SYMLINK)
|| ((propval) && (! is_special))
#endif
) && (kind != svn_node_none)) {
return svn_error_createf
(SVN_ERR_NODE_UNEXPECTED_KIND, NULL,
_("Entry '%s' has unexpectedly changed special status"),
svn_path_local_style(path, pool));
}
if (entry->kind == svn_node_dir) {
svn_error_t *err;
const svn_wc_entry_t *e = NULL;
err = svn_wc_entries_read(&entries, adm_access, copy_mode, pool);
if (err) {
svn_error_clear(err);
entries = NULL;
}
if ((entries) && ((e = apr_hash_get(entries, SVN_WC_ENTRY_THIS_DIR,
APR_HASH_KEY_STRING)))) {
entry = e;
SVN_ERR(svn_wc_conflicted_p(&tc, &pc, path, entry, pool));
}
else {
SVN_ERR(svn_wc_conflicted_p(&tc, &pc, p_path, entry, pool));
}
}
else {
SVN_ERR(svn_wc_conflicted_p(&tc, &pc, p_path, entry, pool));
}
if (tc || pc) {
if (SVN_WC__CL_MATCH(changelists, entry))
return svn_error_createf(SVN_ERR_WC_FOUND_CONFLICT, NULL,
_("Aborting commit: '%s' remains in conflict"),
svn_path_local_style(path, pool));
}
if ((entry->url) && (! copy_mode))
url = entry->url;
if ((! adds_only)
&& ((entry->deleted && entry->schedule == svn_wc_schedule_normal)
|| (entry->schedule == svn_wc_schedule_delete)
|| (entry->schedule == svn_wc_schedule_replace))) {
state_flags |= SVN_CLIENT_COMMIT_ITEM_DELETE;
}
if ((entry->schedule == svn_wc_schedule_add)
|| (entry->schedule == svn_wc_schedule_replace)) {
state_flags |= SVN_CLIENT_COMMIT_ITEM_ADD;
if (entry->copyfrom_url) {
state_flags |= SVN_CLIENT_COMMIT_ITEM_IS_COPY;
cf_url = entry->copyfrom_url;
adds_only = FALSE;
} else {
adds_only = TRUE;
}
}
if ((entry->copied || copy_mode)
&& (! entry->deleted)
&& (entry->schedule == svn_wc_schedule_normal)) {
svn_revnum_t p_rev = entry->revision - 1;
svn_boolean_t wc_root = FALSE;
SVN_ERR(svn_wc_is_wc_root(&wc_root, path, adm_access, pool));
if (! wc_root) {
if (parent_entry)
p_rev = parent_entry->revision;
} else if (! copy_mode)
return svn_error_createf
(SVN_ERR_WC_CORRUPT, NULL,
_("Did not expect '%s' to be a working copy root"),
svn_path_local_style(path, pool));
if (entry->revision != p_rev) {
state_flags |= SVN_CLIENT_COMMIT_ITEM_ADD;
state_flags |= SVN_CLIENT_COMMIT_ITEM_IS_COPY;
adds_only = FALSE;
cf_rev = entry->revision;
if (copy_mode)
cf_url = entry->url;
else if (copyfrom_url)
cf_url = copyfrom_url;
else
return svn_error_createf
(SVN_ERR_BAD_URL, NULL,
_("Commit item '%s' has copy flag but no copyfrom URL"),
svn_path_local_style(path, pool));
}
}
if (state_flags & SVN_CLIENT_COMMIT_ITEM_ADD) {
svn_node_kind_t working_kind;
svn_boolean_t eol_prop_changed;
SVN_ERR(svn_io_check_path(path, &working_kind, pool));
if (working_kind == svn_node_none) {
return svn_error_createf
(SVN_ERR_WC_PATH_NOT_FOUND, NULL,
_("'%s' is scheduled for addition, but is missing"),
svn_path_local_style(path, pool));
}
SVN_ERR(check_prop_mods(&prop_mod, &eol_prop_changed, path,
adm_access, pool));
if (entry->kind == svn_node_file) {
if (state_flags & SVN_CLIENT_COMMIT_ITEM_IS_COPY)
SVN_ERR(svn_wc_text_modified_p(&text_mod, path,
eol_prop_changed,
adm_access, pool));
else
text_mod = TRUE;
}
}
else if (! (state_flags & SVN_CLIENT_COMMIT_ITEM_DELETE)) {
svn_boolean_t eol_prop_changed;
SVN_ERR(check_prop_mods(&prop_mod, &eol_prop_changed, path,
adm_access, pool));
if (entry->kind == svn_node_file)
SVN_ERR(svn_wc_text_modified_p(&text_mod, path, eol_prop_changed,
adm_access, pool));
}
if (text_mod)
state_flags |= SVN_CLIENT_COMMIT_ITEM_TEXT_MODS;
if (prop_mod)
state_flags |= SVN_CLIENT_COMMIT_ITEM_PROP_MODS;
if (entry->lock_token
&& (state_flags || just_locked))
state_flags |= SVN_CLIENT_COMMIT_ITEM_LOCK_TOKEN;
if (state_flags) {
if (SVN_WC__CL_MATCH(changelists, entry)) {
add_committable(committables, path, entry->kind, url,
entry->revision,
cf_url,
cf_rev,
state_flags);
if (lock_tokens && entry->lock_token)
apr_hash_set(lock_tokens, apr_pstrdup(token_pool, url),
APR_HASH_KEY_STRING,
apr_pstrdup(token_pool, entry->lock_token));
}
}
if (entries && (depth > svn_depth_empty)
&& ((! (state_flags & SVN_CLIENT_COMMIT_ITEM_DELETE))
|| (state_flags & SVN_CLIENT_COMMIT_ITEM_ADD))) {
apr_hash_index_t *hi;
const svn_wc_entry_t *this_entry;
apr_pool_t *loop_pool = svn_pool_create(pool);
for (hi = apr_hash_first(pool, entries);
hi;
hi = apr_hash_next(hi)) {
const void *key;
void *val;
const char *name;
const char *full_path;
const char *used_url = NULL;
const char *name_uri = NULL;
const char *this_cf_url = cf_url ? cf_url : copyfrom_url;
svn_wc_adm_access_t *dir_access = adm_access;
svn_pool_clear(loop_pool);
apr_hash_this(hi, &key, NULL, &val);
name = key;
if (! strcmp(name, SVN_WC_ENTRY_THIS_DIR))
continue;
this_entry = val;
name_uri = svn_path_uri_encode(name, loop_pool);
full_path = svn_path_join(path, name, loop_pool);
if (this_cf_url)
this_cf_url = svn_path_join(this_cf_url, name_uri, loop_pool);
if ((! this_entry->url) || (copy_mode))
used_url = svn_path_join(url, name_uri, loop_pool);
if (this_entry->kind == svn_node_dir) {
if (depth <= svn_depth_files) {
continue;
} else {
svn_error_t *lockerr;
lockerr = svn_wc_adm_retrieve(&dir_access, adm_access,
full_path, loop_pool);
if (lockerr) {
if (lockerr->apr_err == SVN_ERR_WC_NOT_LOCKED) {
svn_node_kind_t childkind;
svn_error_t *err = svn_io_check_path(full_path,
&childkind,
loop_pool);
if (! err
&& (childkind == svn_node_none)
&& (this_entry->schedule
== svn_wc_schedule_delete)) {
if (SVN_WC__CL_MATCH(changelists, entry)) {
add_committable(
committables, full_path,
this_entry->kind, used_url,
SVN_INVALID_REVNUM,
NULL,
SVN_INVALID_REVNUM,
SVN_CLIENT_COMMIT_ITEM_DELETE);
svn_error_clear(lockerr);
continue;
}
} else {
svn_error_clear(err);
return lockerr;
}
} else
return lockerr;
}
}
} else {
dir_access = adm_access;
}
{
svn_depth_t depth_below_here = depth;
if (depth == svn_depth_immediates
|| depth == svn_depth_files)
depth_below_here = svn_depth_empty;
SVN_ERR(harvest_committables
(committables, lock_tokens, full_path, dir_access,
used_url ? used_url : this_entry->url,
this_cf_url,
this_entry,
entry,
adds_only,
copy_mode,
depth_below_here,
just_locked,
changelists,
ctx,
loop_pool));
}
}
svn_pool_destroy(loop_pool);
}
if (lock_tokens && entry->kind == svn_node_dir
&& (state_flags & SVN_CLIENT_COMMIT_ITEM_DELETE)) {
SVN_ERR(svn_wc_walk_entries3(path, adm_access, &add_tokens_callbacks,
lock_tokens,
svn_depth_infinity, FALSE,
ctx->cancel_func, ctx->cancel_baton,
pool));
}
return SVN_NO_ERROR;
}
static svn_error_t *
validate_dangler(void *baton,
const void *key, apr_ssize_t klen, void *val,
apr_pool_t *pool) {
const char *dangling_parent = key;
const char *dangling_child = val;
if (! look_up_committable(baton, dangling_parent, pool)) {
return svn_error_createf
(SVN_ERR_ILLEGAL_TARGET, NULL,
_("'%s' is not under version control "
"and is not part of the commit, "
"yet its child '%s' is part of the commit"),
svn_path_local_style(dangling_parent, pool),
svn_path_local_style(dangling_child, pool));
}
return SVN_NO_ERROR;
}
svn_error_t *
svn_client__harvest_committables(apr_hash_t **committables,
apr_hash_t **lock_tokens,
svn_wc_adm_access_t *parent_dir,
apr_array_header_t *targets,
svn_depth_t depth,
svn_boolean_t just_locked,
const apr_array_header_t *changelists,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
int i = 0;
svn_wc_adm_access_t *dir_access;
apr_pool_t *subpool = svn_pool_create(pool);
apr_hash_t *changelist_hash = NULL;
apr_hash_t *danglers = apr_hash_make(pool);
*committables = apr_hash_make(pool);
*lock_tokens = apr_hash_make(pool);
if (changelists && changelists->nelts)
SVN_ERR(svn_hash_from_cstring_keys(&changelist_hash, changelists, pool));
do {
svn_wc_adm_access_t *adm_access;
const svn_wc_entry_t *entry;
const char *target;
svn_pool_clear(subpool);
target = svn_path_join_many(subpool,
svn_wc_adm_access_path(parent_dir),
targets->nelts
? APR_ARRAY_IDX(targets, i, const char *)
: NULL,
NULL);
SVN_ERR(svn_wc_adm_probe_retrieve(&adm_access, parent_dir,
target, subpool));
SVN_ERR(svn_wc__entry_versioned(&entry, target, adm_access, FALSE,
subpool));
if (! entry->url)
return svn_error_createf(SVN_ERR_WC_CORRUPT, NULL,
_("Entry for '%s' has no URL"),
svn_path_local_style(target, pool));
if ((entry->schedule == svn_wc_schedule_add)
|| (entry->schedule == svn_wc_schedule_replace)) {
const char *parent, *base_name;
svn_wc_adm_access_t *parent_access;
const svn_wc_entry_t *p_entry = NULL;
svn_error_t *err;
svn_path_split(target, &parent, &base_name, subpool);
err = svn_wc_adm_retrieve(&parent_access, parent_dir,
parent, subpool);
if (err && err->apr_err == SVN_ERR_WC_NOT_LOCKED) {
svn_error_clear(err);
SVN_ERR(svn_wc_adm_open3(&parent_access, NULL, parent,
FALSE, 0, ctx->cancel_func,
ctx->cancel_baton, subpool));
} else if (err) {
return err;
}
SVN_ERR(svn_wc_entry(&p_entry, parent, parent_access,
FALSE, subpool));
if (! p_entry)
return svn_error_createf
(SVN_ERR_WC_CORRUPT, NULL,
_("'%s' is scheduled for addition within unversioned parent"),
svn_path_local_style(target, pool));
if ((p_entry->schedule == svn_wc_schedule_add)
|| (p_entry->schedule == svn_wc_schedule_replace)) {
apr_hash_set(danglers, apr_pstrdup(pool, parent),
APR_HASH_KEY_STRING,
apr_pstrdup(pool, target));
}
}
if ((entry->copied) && (entry->schedule == svn_wc_schedule_normal))
return svn_error_createf
(SVN_ERR_ILLEGAL_TARGET, NULL,
_("Entry for '%s' is marked as 'copied' but is not itself scheduled"
"\nfor addition. Perhaps you're committing a target that is\n"
"inside an unversioned (or not-yet-versioned) directory?"),
svn_path_local_style(target, pool));
SVN_ERR(svn_wc_adm_retrieve(&dir_access, parent_dir,
(entry->kind == svn_node_dir
? target
: svn_path_dirname(target, subpool)),
subpool));
SVN_ERR(harvest_committables(*committables, *lock_tokens, target,
dir_access, entry->url, NULL,
entry, NULL, FALSE, FALSE, depth,
just_locked, changelist_hash,
ctx, subpool));
i++;
} while (i < targets->nelts);
SVN_ERR(svn_iter_apr_hash(NULL,
danglers, validate_dangler, *committables, pool));
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
struct copy_committables_baton {
svn_wc_adm_access_t *adm_access;
svn_client_ctx_t *ctx;
apr_hash_t *committables;
};
static svn_error_t *
harvest_copy_committables(void *baton, void *item, apr_pool_t *pool) {
struct copy_committables_baton *btn = baton;
const svn_wc_entry_t *entry;
svn_client__copy_pair_t *pair =
*(svn_client__copy_pair_t **)item;
svn_wc_adm_access_t *dir_access;
SVN_ERR(svn_wc__entry_versioned(&entry, pair->src, btn->adm_access, FALSE,
pool));
if (entry->kind == svn_node_dir)
SVN_ERR(svn_wc_adm_retrieve(&dir_access, btn->adm_access, pair->src, pool));
else
SVN_ERR(svn_wc_adm_retrieve(&dir_access, btn->adm_access,
svn_path_dirname(pair->src, pool),
pool));
SVN_ERR(harvest_committables(btn->committables, NULL, pair->src,
dir_access, pair->dst, entry->url, entry,
NULL, FALSE, TRUE, svn_depth_infinity,
FALSE, NULL, btn->ctx, pool));
return SVN_NO_ERROR;
}
svn_error_t *
svn_client__get_copy_committables(apr_hash_t **committables,
const apr_array_header_t *copy_pairs,
svn_wc_adm_access_t *adm_access,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
struct copy_committables_baton btn;
*committables = apr_hash_make(pool);
btn.adm_access = adm_access;
btn.ctx = ctx;
btn.committables = *committables;
SVN_ERR(svn_iter_apr_array(NULL, copy_pairs,
harvest_copy_committables, &btn, pool));
return SVN_NO_ERROR;
}
int svn_client__sort_commit_item_urls(const void *a, const void *b) {
const svn_client_commit_item3_t *item1
= *((const svn_client_commit_item3_t * const *) a);
const svn_client_commit_item3_t *item2
= *((const svn_client_commit_item3_t * const *) b);
return svn_path_compare_paths(item1->url, item2->url);
}
svn_error_t *
svn_client__condense_commit_items(const char **base_url,
apr_array_header_t *commit_items,
apr_pool_t *pool) {
apr_array_header_t *ci = commit_items;
const char *url;
svn_client_commit_item3_t *item, *last_item = NULL;
int i;
assert(ci && ci->nelts);
qsort(ci->elts, ci->nelts,
ci->elt_size, svn_client__sort_commit_item_urls);
for (i = 0; i < ci->nelts; i++) {
item = APR_ARRAY_IDX(ci, i, svn_client_commit_item3_t *);
url = item->url;
if ((last_item) && (strcmp(last_item->url, url) == 0))
return svn_error_createf
(SVN_ERR_CLIENT_DUPLICATE_COMMIT_URL, NULL,
_("Cannot commit both '%s' and '%s' as they refer to the same URL"),
svn_path_local_style(item->path, pool),
svn_path_local_style(last_item->path, pool));
if (i == 0)
*base_url = apr_pstrdup(pool, url);
else
*base_url = svn_path_get_longest_ancestor(*base_url, url, pool);
if ((strlen(*base_url) == strlen(url))
&& (! ((item->kind == svn_node_dir)
&& item->state_flags == SVN_CLIENT_COMMIT_ITEM_PROP_MODS)))
*base_url = svn_path_dirname(*base_url, pool);
last_item = item;
}
for (i = 0; i < ci->nelts; i++) {
svn_client_commit_item3_t *this_item
= APR_ARRAY_IDX(ci, i, svn_client_commit_item3_t *);
int url_len = strlen(this_item->url);
int base_url_len = strlen(*base_url);
if (url_len > base_url_len)
this_item->url = apr_pstrdup(pool, this_item->url + base_url_len + 1);
else
this_item->url = "";
}
#if defined(SVN_CLIENT_COMMIT_DEBUG)
fprintf(stderr, "COMMITTABLES: (base URL=%s)\n", *base_url);
fprintf(stderr, " FLAGS REV REL-URL (COPY-URL)\n");
for (i = 0; i < ci->nelts; i++) {
svn_client_commit_item3_t *this_item
= APR_ARRAY_IDX(ci, i, svn_client_commit_item3_t *);
char flags[6];
flags[0] = (this_item->state_flags & SVN_CLIENT_COMMIT_ITEM_ADD)
? 'a' : '-';
flags[1] = (this_item->state_flags & SVN_CLIENT_COMMIT_ITEM_DELETE)
? 'd' : '-';
flags[2] = (this_item->state_flags & SVN_CLIENT_COMMIT_ITEM_TEXT_MODS)
? 't' : '-';
flags[3] = (this_item->state_flags & SVN_CLIENT_COMMIT_ITEM_PROP_MODS)
? 'p' : '-';
flags[4] = (this_item->state_flags & SVN_CLIENT_COMMIT_ITEM_IS_COPY)
? 'c' : '-';
flags[5] = '\0';
fprintf(stderr, " %s %6ld '%s' (%s)\n",
flags,
this_item->revision,
this_item->url ? this_item->url : "",
this_item->copyfrom_url ? this_item->copyfrom_url : "none");
}
#endif
return SVN_NO_ERROR;
}
struct file_mod_t {
svn_client_commit_item3_t *item;
void *file_baton;
};
struct path_driver_cb_baton {
svn_wc_adm_access_t *adm_access;
const svn_delta_editor_t *editor;
void *edit_baton;
apr_hash_t *file_mods;
apr_hash_t *tempfiles;
const char *notify_path_prefix;
svn_client_ctx_t *ctx;
apr_hash_t *commit_items;
};
static svn_error_t *
do_item_commit(void **dir_baton,
void *parent_baton,
void *callback_baton,
const char *path,
apr_pool_t *pool) {
struct path_driver_cb_baton *cb_baton = callback_baton;
svn_client_commit_item3_t *item = apr_hash_get(cb_baton->commit_items,
path, APR_HASH_KEY_STRING);
svn_node_kind_t kind = item->kind;
void *file_baton = NULL;
const char *copyfrom_url = NULL;
apr_pool_t *file_pool = NULL;
svn_wc_adm_access_t *adm_access = cb_baton->adm_access;
const svn_delta_editor_t *editor = cb_baton->editor;
apr_hash_t *file_mods = cb_baton->file_mods;
const char *notify_path_prefix = cb_baton->notify_path_prefix;
svn_client_ctx_t *ctx = cb_baton->ctx;
*dir_baton = NULL;
if (item->copyfrom_url)
copyfrom_url = item->copyfrom_url;
if ((kind == svn_node_file)
&& (item->state_flags & SVN_CLIENT_COMMIT_ITEM_TEXT_MODS))
file_pool = apr_hash_pool_get(file_mods);
else
file_pool = pool;
if (ctx->cancel_func)
SVN_ERR(ctx->cancel_func(ctx->cancel_baton));
if (item->state_flags & SVN_CLIENT_COMMIT_ITEM_IS_COPY) {
if (! copyfrom_url)
return svn_error_createf
(SVN_ERR_BAD_URL, NULL,
_("Commit item '%s' has copy flag but no copyfrom URL"),
svn_path_local_style(path, pool));
if (! SVN_IS_VALID_REVNUM(item->copyfrom_rev))
return svn_error_createf
(SVN_ERR_CLIENT_BAD_REVISION, NULL,
_("Commit item '%s' has copy flag but an invalid revision"),
svn_path_local_style(path, pool));
}
if (ctx->notify_func2) {
const char *npath = NULL;
svn_wc_notify_t *notify;
if (notify_path_prefix) {
if (strcmp(notify_path_prefix, item->path))
npath = svn_path_is_child(notify_path_prefix, item->path, pool);
else
npath = ".";
}
if (! npath)
npath = item->path;
if ((item->state_flags & SVN_CLIENT_COMMIT_ITEM_DELETE)
&& (item->state_flags & SVN_CLIENT_COMMIT_ITEM_ADD)) {
notify = svn_wc_create_notify(npath, svn_wc_notify_commit_replaced,
pool);
} else if (item->state_flags & SVN_CLIENT_COMMIT_ITEM_DELETE) {
notify = svn_wc_create_notify(npath, svn_wc_notify_commit_deleted,
pool);
} else if (item->state_flags & SVN_CLIENT_COMMIT_ITEM_ADD) {
notify = svn_wc_create_notify(npath, svn_wc_notify_commit_added,
pool);
if (item->kind == svn_node_file) {
const svn_string_t *propval;
SVN_ERR(svn_wc_prop_get
(&propval, SVN_PROP_MIME_TYPE, item->path, adm_access,
pool));
if (propval)
notify->mime_type = propval->data;
}
} else if ((item->state_flags & SVN_CLIENT_COMMIT_ITEM_TEXT_MODS)
|| (item->state_flags & SVN_CLIENT_COMMIT_ITEM_PROP_MODS)) {
notify = svn_wc_create_notify(npath, svn_wc_notify_commit_modified,
pool);
if (item->state_flags & SVN_CLIENT_COMMIT_ITEM_TEXT_MODS)
notify->content_state = svn_wc_notify_state_changed;
else
notify->content_state = svn_wc_notify_state_unchanged;
if (item->state_flags & SVN_CLIENT_COMMIT_ITEM_PROP_MODS)
notify->prop_state = svn_wc_notify_state_changed;
else
notify->prop_state = svn_wc_notify_state_unchanged;
} else
notify = NULL;
if (notify) {
notify->kind = item->kind;
(*ctx->notify_func2)(ctx->notify_baton2, notify, pool);
}
}
if (item->state_flags & SVN_CLIENT_COMMIT_ITEM_DELETE) {
assert(parent_baton);
SVN_ERR(editor->delete_entry(path, item->revision,
parent_baton, pool));
}
if (item->state_flags & SVN_CLIENT_COMMIT_ITEM_ADD) {
if (kind == svn_node_file) {
assert(parent_baton);
SVN_ERR(editor->add_file
(path, parent_baton, copyfrom_url,
copyfrom_url ? item->copyfrom_rev : SVN_INVALID_REVNUM,
file_pool, &file_baton));
} else {
assert(parent_baton);
SVN_ERR(editor->add_directory
(path, parent_baton, copyfrom_url,
copyfrom_url ? item->copyfrom_rev : SVN_INVALID_REVNUM,
pool, dir_baton));
}
if (item->outgoing_prop_changes) {
svn_prop_t *prop;
apr_array_header_t *prop_changes = item->outgoing_prop_changes;
int ctr;
for (ctr = 0; ctr < prop_changes->nelts; ctr++) {
prop = APR_ARRAY_IDX(prop_changes, ctr, svn_prop_t *);
if (kind == svn_node_file) {
editor->change_file_prop(file_baton, prop->name,
prop->value, pool);
} else {
editor->change_dir_prop(*dir_baton, prop->name,
prop->value, pool);
}
}
}
}
if (item->state_flags & SVN_CLIENT_COMMIT_ITEM_PROP_MODS) {
const svn_wc_entry_t *tmp_entry;
if (kind == svn_node_file) {
if (! file_baton) {
assert(parent_baton);
SVN_ERR(editor->open_file(path, parent_baton,
item->revision,
file_pool, &file_baton));
}
} else {
if (! *dir_baton) {
if (! parent_baton) {
SVN_ERR(editor->open_root
(cb_baton->edit_baton, item->revision,
pool, dir_baton));
} else {
SVN_ERR(editor->open_directory
(path, parent_baton, item->revision,
pool, dir_baton));
}
}
}
SVN_ERR(svn_wc_entry(&tmp_entry, item->path, adm_access, TRUE, pool));
SVN_ERR(svn_wc_transmit_prop_deltas
(item->path, adm_access, tmp_entry, editor,
(kind == svn_node_dir) ? *dir_baton : file_baton, NULL, pool));
if (item->outgoing_prop_changes) {
svn_prop_t *prop;
int i;
for (i = 0; i < item->outgoing_prop_changes->nelts; i++) {
prop = APR_ARRAY_IDX(item->outgoing_prop_changes, i,
svn_prop_t *);
if (kind == svn_node_file) {
editor->change_file_prop(file_baton, prop->name,
prop->value, pool);
} else {
editor->change_dir_prop(*dir_baton, prop->name,
prop->value, pool);
}
}
}
}
if ((kind == svn_node_file)
&& (item->state_flags & SVN_CLIENT_COMMIT_ITEM_TEXT_MODS)) {
struct file_mod_t *mod = apr_palloc(file_pool, sizeof(*mod));
if (! file_baton) {
assert(parent_baton);
SVN_ERR(editor->open_file(path, parent_baton,
item->revision,
file_pool, &file_baton));
}
mod->item = item;
mod->file_baton = file_baton;
apr_hash_set(file_mods, item->url, APR_HASH_KEY_STRING, mod);
} else if (file_baton) {
SVN_ERR(editor->close_file(file_baton, NULL, file_pool));
}
return SVN_NO_ERROR;
}
#if defined(SVN_CLIENT_COMMIT_DEBUG)
static svn_error_t *get_test_editor(const svn_delta_editor_t **editor,
void **edit_baton,
const svn_delta_editor_t *real_editor,
void *real_eb,
const char *base_url,
apr_pool_t *pool);
#endif
svn_error_t *
svn_client__do_commit(const char *base_url,
apr_array_header_t *commit_items,
svn_wc_adm_access_t *adm_access,
const svn_delta_editor_t *editor,
void *edit_baton,
const char *notify_path_prefix,
apr_hash_t **tempfiles,
apr_hash_t **digests,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
apr_hash_t *file_mods = apr_hash_make(pool);
apr_hash_t *items_hash = apr_hash_make(pool);
apr_pool_t *subpool = svn_pool_create(pool);
apr_hash_index_t *hi;
int i;
struct path_driver_cb_baton cb_baton;
apr_array_header_t *paths =
apr_array_make(pool, commit_items->nelts, sizeof(const char *));
#if defined(SVN_CLIENT_COMMIT_DEBUG)
{
SVN_ERR(get_test_editor(&editor, &edit_baton,
editor, edit_baton,
base_url, pool));
}
#endif
if (tempfiles)
*tempfiles = apr_hash_make(pool);
if (digests)
*digests = apr_hash_make(pool);
for (i = 0; i < commit_items->nelts; i++) {
svn_client_commit_item3_t *item =
APR_ARRAY_IDX(commit_items, i, svn_client_commit_item3_t *);
const char *path = svn_path_uri_decode(item->url, pool);
apr_hash_set(items_hash, path, APR_HASH_KEY_STRING, item);
APR_ARRAY_PUSH(paths, const char *) = path;
}
cb_baton.adm_access = adm_access;
cb_baton.editor = editor;
cb_baton.edit_baton = edit_baton;
cb_baton.file_mods = file_mods;
cb_baton.tempfiles = tempfiles ? *tempfiles : NULL;
cb_baton.notify_path_prefix = notify_path_prefix;
cb_baton.ctx = ctx;
cb_baton.commit_items = items_hash;
SVN_ERR(svn_delta_path_driver(editor, edit_baton, SVN_INVALID_REVNUM,
paths, do_item_commit, &cb_baton, pool));
for (hi = apr_hash_first(pool, file_mods); hi; hi = apr_hash_next(hi)) {
struct file_mod_t *mod;
svn_client_commit_item3_t *item;
void *val;
void *file_baton;
const char *tempfile, *dir_path;
unsigned char digest[APR_MD5_DIGESTSIZE];
svn_boolean_t fulltext = FALSE;
svn_wc_adm_access_t *item_access;
svn_pool_clear(subpool);
apr_hash_this(hi, NULL, NULL, &val);
mod = val;
item = mod->item;
file_baton = mod->file_baton;
if (ctx->cancel_func)
SVN_ERR(ctx->cancel_func(ctx->cancel_baton));
if (ctx->notify_func2) {
svn_wc_notify_t *notify;
const char *npath = NULL;
if (notify_path_prefix) {
if (strcmp(notify_path_prefix, item->path) != 0)
npath = svn_path_is_child(notify_path_prefix, item->path,
subpool);
else
npath = ".";
}
if (! npath)
npath = item->path;
notify = svn_wc_create_notify(npath,
svn_wc_notify_commit_postfix_txdelta,
subpool);
notify->kind = svn_node_file;
(*ctx->notify_func2)(ctx->notify_baton2, notify, subpool);
}
if (item->state_flags & SVN_CLIENT_COMMIT_ITEM_ADD)
fulltext = TRUE;
dir_path = svn_path_dirname(item->path, subpool);
SVN_ERR(svn_wc_adm_retrieve(&item_access, adm_access, dir_path,
subpool));
SVN_ERR(svn_wc_transmit_text_deltas2(tempfiles ? &tempfile : NULL,
digest, item->path,
item_access, fulltext, editor,
file_baton, subpool));
if (tempfiles && tempfile && *tempfiles) {
tempfile = apr_pstrdup(apr_hash_pool_get(*tempfiles), tempfile);
apr_hash_set(*tempfiles, tempfile, APR_HASH_KEY_STRING, (void *)1);
}
if (digests) {
unsigned char *new_digest = apr_pmemdup(apr_hash_pool_get(*digests),
digest, APR_MD5_DIGESTSIZE);
apr_hash_set(*digests, item->path, APR_HASH_KEY_STRING, new_digest);
}
}
svn_pool_destroy(subpool);
SVN_ERR(editor->close_edit(edit_baton, pool));
return SVN_NO_ERROR;
}
struct commit_baton {
svn_commit_info_t **info;
apr_pool_t *pool;
};
svn_error_t *svn_client__commit_get_baton(void **baton,
svn_commit_info_t **info,
apr_pool_t *pool) {
struct commit_baton *cb = apr_pcalloc(pool, sizeof(*cb));
cb->info = info;
cb->pool = pool;
*baton = cb;
return SVN_NO_ERROR;
}
svn_error_t *svn_client__commit_callback(const svn_commit_info_t *commit_info,
void *baton,
apr_pool_t *pool) {
struct commit_baton *cb = baton;
*(cb->info) = svn_commit_info_dup(commit_info, cb->pool);
return SVN_NO_ERROR;
}
#if defined(SVN_CLIENT_COMMIT_DEBUG)
struct edit_baton {
const char *path;
const svn_delta_editor_t *real_editor;
void *real_eb;
};
struct item_baton {
struct edit_baton *eb;
void *real_baton;
const char *path;
};
static struct item_baton *
make_baton(struct edit_baton *eb,
void *real_baton,
const char *path,
apr_pool_t *pool) {
struct item_baton *new_baton = apr_pcalloc(pool, sizeof(*new_baton));
new_baton->eb = eb;
new_baton->real_baton = real_baton;
new_baton->path = apr_pstrdup(pool, path);
return new_baton;
}
static svn_error_t *
set_target_revision(void *edit_baton,
svn_revnum_t target_revision,
apr_pool_t *pool) {
struct edit_baton *eb = edit_baton;
return (*eb->real_editor->set_target_revision)(eb->real_eb,
target_revision,
pool);
}
static svn_error_t *
open_root(void *edit_baton,
svn_revnum_t base_revision,
apr_pool_t *dir_pool,
void **root_baton) {
struct edit_baton *eb = edit_baton;
struct item_baton *new_baton = make_baton(eb, NULL, eb->path, dir_pool);
fprintf(stderr, "TEST EDIT STARTED (base URL=%s)\n", eb->path);
*root_baton = new_baton;
return (*eb->real_editor->open_root)(eb->real_eb,
base_revision,
dir_pool,
&new_baton->real_baton);
}
static svn_error_t *
add_file(const char *path,
void *parent_baton,
const char *copyfrom_path,
svn_revnum_t copyfrom_revision,
apr_pool_t *pool,
void **baton) {
struct item_baton *db = parent_baton;
struct item_baton *new_baton = make_baton(db->eb, NULL, path, pool);
const char *copystuffs = "";
if (copyfrom_path && SVN_IS_VALID_REVNUM(copyfrom_revision))
copystuffs = apr_psprintf(pool,
" (copied from %s:%ld)",
copyfrom_path,
copyfrom_revision);
fprintf(stderr, " Adding : %s%s\n", path, copystuffs);
*baton = new_baton;
return (*db->eb->real_editor->add_file)(path, db->real_baton,
copyfrom_path, copyfrom_revision,
pool, &new_baton->real_baton);
}
static svn_error_t *
delete_entry(const char *path,
svn_revnum_t revision,
void *parent_baton,
apr_pool_t *pool) {
struct item_baton *db = parent_baton;
fprintf(stderr, " Deleting: %s\n", path);
return (*db->eb->real_editor->delete_entry)(path, revision,
db->real_baton, pool);
}
static svn_error_t *
open_file(const char *path,
void *parent_baton,
svn_revnum_t base_revision,
apr_pool_t *pool,
void **baton) {
struct item_baton *db = parent_baton;
struct item_baton *new_baton = make_baton(db->eb, NULL, path, pool);
fprintf(stderr, " Opening : %s\n", path);
*baton = new_baton;
return (*db->eb->real_editor->open_file)(path, db->real_baton,
base_revision, pool,
&new_baton->real_baton);
}
static svn_error_t *
close_file(void *baton, const char *text_checksum, apr_pool_t *pool) {
struct item_baton *fb = baton;
fprintf(stderr, " Closing : %s\n", fb->path);
return (*fb->eb->real_editor->close_file)(fb->real_baton,
text_checksum, pool);
}
static svn_error_t *
change_file_prop(void *file_baton,
const char *name,
const svn_string_t *value,
apr_pool_t *pool) {
struct item_baton *fb = file_baton;
fprintf(stderr, " PropSet (%s=%s)\n", name, value ? value->data : "");
return (*fb->eb->real_editor->change_file_prop)(fb->real_baton,
name, value, pool);
}
static svn_error_t *
apply_textdelta(void *file_baton,
const char *base_checksum,
apr_pool_t *pool,
svn_txdelta_window_handler_t *handler,
void **handler_baton) {
struct item_baton *fb = file_baton;
fprintf(stderr, " Transmitting text...\n");
return (*fb->eb->real_editor->apply_textdelta)(fb->real_baton,
base_checksum, pool,
handler, handler_baton);
}
static svn_error_t *
close_edit(void *edit_baton, apr_pool_t *pool) {
struct edit_baton *eb = edit_baton;
fprintf(stderr, "TEST EDIT COMPLETED\n");
return (*eb->real_editor->close_edit)(eb->real_eb, pool);
}
static svn_error_t *
add_directory(const char *path,
void *parent_baton,
const char *copyfrom_path,
svn_revnum_t copyfrom_revision,
apr_pool_t *pool,
void **baton) {
struct item_baton *db = parent_baton;
struct item_baton *new_baton = make_baton(db->eb, NULL, path, pool);
const char *copystuffs = "";
if (copyfrom_path && SVN_IS_VALID_REVNUM(copyfrom_revision))
copystuffs = apr_psprintf(pool,
" (copied from %s:%ld)",
copyfrom_path,
copyfrom_revision);
fprintf(stderr, " Adding : %s%s\n", path, copystuffs);
*baton = new_baton;
return (*db->eb->real_editor->add_directory)(path,
db->real_baton,
copyfrom_path,
copyfrom_revision,
pool,
&new_baton->real_baton);
}
static svn_error_t *
open_directory(const char *path,
void *parent_baton,
svn_revnum_t base_revision,
apr_pool_t *pool,
void **baton) {
struct item_baton *db = parent_baton;
struct item_baton *new_baton = make_baton(db->eb, NULL, path, pool);
fprintf(stderr, " Opening : %s\n", path);
*baton = new_baton;
return (*db->eb->real_editor->open_directory)(path, db->real_baton,
base_revision, pool,
&new_baton->real_baton);
}
static svn_error_t *
change_dir_prop(void *dir_baton,
const char *name,
const svn_string_t *value,
apr_pool_t *pool) {
struct item_baton *db = dir_baton;
fprintf(stderr, " PropSet (%s=%s)\n", name, value ? value->data : "");
return (*db->eb->real_editor->change_dir_prop)(db->real_baton,
name, value, pool);
}
static svn_error_t *
close_directory(void *baton, apr_pool_t *pool) {
struct item_baton *db = baton;
fprintf(stderr, " Closing : %s\n", db->path);
return (*db->eb->real_editor->close_directory)(db->real_baton, pool);
}
static svn_error_t *
abort_edit(void *edit_baton, apr_pool_t *pool) {
struct edit_baton *eb = edit_baton;
fprintf(stderr, "TEST EDIT ABORTED\n");
return (*eb->real_editor->abort_edit)(eb->real_eb, pool);
}
static svn_error_t *
get_test_editor(const svn_delta_editor_t **editor,
void **edit_baton,
const svn_delta_editor_t *real_editor,
void *real_eb,
const char *base_url,
apr_pool_t *pool) {
svn_delta_editor_t *ed = svn_delta_default_editor(pool);
struct edit_baton *eb = apr_pcalloc(pool, sizeof(*eb));
eb->path = apr_pstrdup(pool, base_url);
eb->real_editor = real_editor;
eb->real_eb = real_eb;
ed->set_target_revision = set_target_revision;
ed->open_root = open_root;
ed->add_directory = add_directory;
ed->open_directory = open_directory;
ed->close_directory = close_directory;
ed->add_file = add_file;
ed->open_file = open_file;
ed->close_file = close_file;
ed->delete_entry = delete_entry;
ed->apply_textdelta = apply_textdelta;
ed->change_dir_prop = change_dir_prop;
ed->change_file_prop = change_file_prop;
ed->close_edit = close_edit;
ed->abort_edit = abort_edit;
*editor = ed;
*edit_baton = eb;
return SVN_NO_ERROR;
}
#endif
svn_error_t *
svn_client__get_log_msg(const char **log_msg,
const char **tmp_file,
const apr_array_header_t *commit_items,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
if (ctx->log_msg_func3) {
return (*ctx->log_msg_func3)(log_msg, tmp_file, commit_items,
ctx->log_msg_baton3, pool);
} else if (ctx->log_msg_func2 || ctx->log_msg_func) {
svn_error_t *err;
apr_pool_t *subpool = svn_pool_create(pool);
apr_array_header_t *old_commit_items =
apr_array_make(subpool, commit_items->nelts, sizeof(void*));
int i;
for (i = 0; i < commit_items->nelts; i++) {
svn_client_commit_item3_t *item =
APR_ARRAY_IDX(commit_items, i, svn_client_commit_item3_t *);
if (ctx->log_msg_func2) {
svn_client_commit_item2_t *old_item =
apr_pcalloc(subpool, sizeof(*old_item));
old_item->path = item->path;
old_item->kind = item->kind;
old_item->url = item->url;
old_item->revision = item->revision;
old_item->copyfrom_url = item->copyfrom_url;
old_item->copyfrom_rev = item->copyfrom_rev;
old_item->state_flags = item->state_flags;
old_item->wcprop_changes = item->incoming_prop_changes;
APR_ARRAY_PUSH(old_commit_items, svn_client_commit_item2_t *) =
old_item;
} else {
svn_client_commit_item_t *old_item =
apr_pcalloc(subpool, sizeof(*old_item));
old_item->path = item->path;
old_item->kind = item->kind;
old_item->url = item->url;
old_item->revision = item->copyfrom_url ?
item->copyfrom_rev : item->revision;
old_item->copyfrom_url = item->copyfrom_url;
old_item->state_flags = item->state_flags;
old_item->wcprop_changes = item->incoming_prop_changes;
APR_ARRAY_PUSH(old_commit_items, svn_client_commit_item_t *) =
old_item;
}
}
if (ctx->log_msg_func2)
err = (*ctx->log_msg_func2)(log_msg, tmp_file, old_commit_items,
ctx->log_msg_baton2, pool);
else
err = (*ctx->log_msg_func)(log_msg, tmp_file, old_commit_items,
ctx->log_msg_baton, pool);
svn_pool_destroy(subpool);
return err;
} else {
*log_msg = "";
*tmp_file = NULL;
return SVN_NO_ERROR;
}
}
svn_error_t *
svn_client__ensure_revprop_table(apr_hash_t **revprop_table_out,
const apr_hash_t *revprop_table_in,
const char *log_msg,
svn_client_ctx_t *ctx,
apr_pool_t *pool) {
apr_hash_t *new_revprop_table;
if (revprop_table_in) {
if (svn_prop_has_svn_prop(revprop_table_in, pool))
return svn_error_create(SVN_ERR_CLIENT_PROPERTY_NAME, NULL,
_("Standard properties can't be set "
"explicitly as revision properties"));
new_revprop_table = apr_hash_copy(pool, revprop_table_in);
} else {
new_revprop_table = apr_hash_make(pool);
}
apr_hash_set(new_revprop_table, SVN_PROP_REVISION_LOG, APR_HASH_KEY_STRING,
svn_string_create(log_msg, pool));
*revprop_table_out = new_revprop_table;
return SVN_NO_ERROR;
}
