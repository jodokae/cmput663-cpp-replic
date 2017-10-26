#include "svn_wc.h"
#include "svn_error.h"
#include "svn_pools.h"
#include "svn_path.h"
#include "wc.h"
#include "entries.h"
#include "lock.h"
#include "props.h"
#include "svn_private_config.h"
static svn_error_t *
relocate_entry(svn_wc_adm_access_t *adm_access,
const svn_wc_entry_t *entry,
const char *from,
const char *to,
svn_wc_relocation_validator3_t validator,
void *validator_baton,
svn_boolean_t do_sync,
apr_pool_t *pool) {
svn_wc_entry_t entry2;
apr_uint64_t flags = 0;
apr_size_t from_len = strlen(from);
if (entry->url && ! strncmp(entry->url, from, from_len)) {
entry2.url = apr_pstrcat(pool, to, entry->url + from_len, NULL);
if (entry->uuid)
SVN_ERR(validator(validator_baton, entry->uuid, entry2.url, NULL,
pool));
flags |= SVN_WC__ENTRY_MODIFY_URL;
}
if (entry->repos && (flags & SVN_WC__ENTRY_MODIFY_URL)) {
apr_size_t repos_len = strlen(entry->repos);
if (from_len >= repos_len) {
apr_size_t to_len = strlen(to);
apr_size_t fs_path_len = from_len - repos_len;
if (to_len < fs_path_len
|| strncmp(from + repos_len, to + (to_len - fs_path_len),
fs_path_len) != 0)
return svn_error_create(SVN_ERR_WC_INVALID_RELOCATION, NULL,
_("Relocate can only change the "
"repository part of an URL"));
from_len = repos_len;
to = apr_pstrndup(pool, to, to_len - fs_path_len);
}
if (strncmp(from, entry->repos, from_len) == 0) {
entry2.repos = apr_pstrcat(pool, to, entry->repos + from_len, NULL);
flags |= SVN_WC__ENTRY_MODIFY_REPOS;
SVN_ERR(validator(validator_baton, entry->uuid, entry2.url,
entry2.repos, pool));
}
}
if (entry->copyfrom_url && ! strncmp(entry->copyfrom_url, from, from_len)) {
entry2.copyfrom_url = apr_pstrcat(pool, to,
entry->copyfrom_url + from_len, NULL);
if (entry->uuid)
SVN_ERR(validator(validator_baton, entry->uuid,
entry2.copyfrom_url, NULL, pool));
flags |= SVN_WC__ENTRY_MODIFY_COPYFROM_URL;
}
if (flags)
SVN_ERR(svn_wc__entry_modify(adm_access, entry->name,
&entry2, flags, do_sync, pool));
return SVN_NO_ERROR;
}
svn_error_t *
svn_wc_relocate3(const char *path,
svn_wc_adm_access_t *adm_access,
const char *from,
const char *to,
svn_boolean_t recurse,
svn_wc_relocation_validator3_t validator,
void *validator_baton,
apr_pool_t *pool) {
apr_hash_t *entries;
apr_hash_index_t *hi;
const svn_wc_entry_t *entry;
apr_pool_t *subpool;
SVN_ERR(svn_wc_entry(&entry, path, adm_access, TRUE, pool));
if (! entry)
return svn_error_create(SVN_ERR_ENTRY_NOT_FOUND, NULL, NULL);
if (entry->kind == svn_node_file) {
SVN_ERR(relocate_entry(adm_access, entry, from, to,
validator, validator_baton, TRUE ,
pool));
return SVN_NO_ERROR;
}
SVN_ERR(svn_wc_entries_read(&entries, adm_access, TRUE, pool));
entry = apr_hash_get(entries, SVN_WC_ENTRY_THIS_DIR, APR_HASH_KEY_STRING);
SVN_ERR(relocate_entry(adm_access, entry, from, to,
validator, validator_baton, FALSE, pool));
subpool = svn_pool_create(pool);
for (hi = apr_hash_first(pool, entries); hi; hi = apr_hash_next(hi)) {
const void *key;
void *val;
apr_hash_this(hi, &key, NULL, &val);
entry = val;
if (strcmp(key, SVN_WC_ENTRY_THIS_DIR) == 0)
continue;
svn_pool_clear(subpool);
if (recurse && (entry->kind == svn_node_dir)
&& (! entry->deleted || (entry->schedule == svn_wc_schedule_add))
&& ! entry->absent) {
svn_wc_adm_access_t *subdir_access;
const char *subdir = svn_path_join(path, key, subpool);
if (svn_wc__adm_missing(adm_access, subdir))
continue;
SVN_ERR(svn_wc_adm_retrieve(&subdir_access, adm_access,
subdir, subpool));
SVN_ERR(svn_wc_relocate3(subdir, subdir_access, from, to,
recurse, validator,
validator_baton, subpool));
}
SVN_ERR(relocate_entry(adm_access, entry, from, to,
validator, validator_baton, FALSE, subpool));
}
svn_pool_destroy(subpool);
SVN_ERR(svn_wc__props_delete(path, svn_wc__props_wcprop, adm_access, pool));
SVN_ERR(svn_wc__entries_write(entries, adm_access, pool));
return SVN_NO_ERROR;
}
struct compat2_baton {
svn_wc_relocation_validator2_t validator;
void *baton;
};
struct compat_baton {
svn_wc_relocation_validator_t validator;
void *baton;
};
static svn_error_t *
compat2_validator(void *baton,
const char *uuid,
const char *url,
const char *root_url,
apr_pool_t *pool) {
struct compat2_baton *cb = baton;
return cb->validator(cb->baton, uuid,
(root_url ? root_url : url), (root_url ? TRUE : FALSE),
pool);
}
static svn_error_t *
compat_validator(void *baton,
const char *uuid,
const char *url,
const char *root_url,
apr_pool_t *pool) {
struct compat_baton *cb = baton;
if (uuid)
return cb->validator(cb->baton, uuid, url);
return SVN_NO_ERROR;
}
svn_error_t *
svn_wc_relocate2(const char *path,
svn_wc_adm_access_t *adm_access,
const char *from,
const char *to,
svn_boolean_t recurse,
svn_wc_relocation_validator2_t validator,
void *validator_baton,
apr_pool_t *pool) {
struct compat2_baton cb;
cb.validator = validator;
cb.baton = validator_baton;
return svn_wc_relocate3(path, adm_access, from, to, recurse,
compat2_validator, &cb, pool);
}
svn_error_t *
svn_wc_relocate(const char *path,
svn_wc_adm_access_t *adm_access,
const char *from,
const char *to,
svn_boolean_t recurse,
svn_wc_relocation_validator_t validator,
void *validator_baton,
apr_pool_t *pool) {
struct compat_baton cb;
cb.validator = validator;
cb.baton = validator_baton;
return svn_wc_relocate3(path, adm_access, from, to, recurse,
compat_validator, &cb, pool);
}
