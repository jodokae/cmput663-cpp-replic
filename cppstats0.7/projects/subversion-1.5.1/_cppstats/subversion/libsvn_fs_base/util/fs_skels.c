#include <string.h>
#include "svn_error.h"
#include "svn_string.h"
#include "svn_types.h"
#include "svn_time.h"
#include "fs_skels.h"
#include "skel.h"
#include "../id.h"
static svn_error_t *
skel_err(const char *skel_type) {
return svn_error_createf(SVN_ERR_FS_MALFORMED_SKEL, NULL,
"Malformed%s%s skeleton",
skel_type ? " " : "",
skel_type ? skel_type : "");
}
static svn_boolean_t
is_valid_checksum_skel(skel_t *skel) {
if (svn_fs_base__list_length(skel) != 2)
return FALSE;
if (svn_fs_base__matches_atom(skel->children, "md5")
&& skel->children->next->is_atom)
return TRUE;
return FALSE;
}
static svn_boolean_t
is_valid_proplist_skel(skel_t *skel) {
int len = svn_fs_base__list_length(skel);
if ((len >= 0) && (len & 1) == 0) {
skel_t *elt;
for (elt = skel->children; elt; elt = elt->next)
if (! elt->is_atom)
return FALSE;
return TRUE;
}
return FALSE;
}
static svn_boolean_t
is_valid_revision_skel(skel_t *skel) {
int len = svn_fs_base__list_length(skel);
if ((len == 2)
&& svn_fs_base__matches_atom(skel->children, "revision")
&& skel->children->next->is_atom)
return TRUE;
return FALSE;
}
static svn_boolean_t
is_valid_transaction_skel(skel_t *skel, transaction_kind_t *kind) {
int len = svn_fs_base__list_length(skel);
if (len != 5)
return FALSE;
if (svn_fs_base__matches_atom(skel->children, "transaction"))
*kind = transaction_kind_normal;
else if (svn_fs_base__matches_atom(skel->children, "committed"))
*kind = transaction_kind_committed;
else if (svn_fs_base__matches_atom(skel->children, "dead"))
*kind = transaction_kind_dead;
else
return FALSE;
if (skel->children->next->is_atom
&& skel->children->next->next->is_atom
&& (! skel->children->next->next->next->is_atom)
&& (! skel->children->next->next->next->next->is_atom))
return TRUE;
return FALSE;
}
static svn_boolean_t
is_valid_rep_delta_chunk_skel(skel_t *skel) {
int len;
skel_t *window;
skel_t *diff;
if ((svn_fs_base__list_length(skel) != 2)
|| (! skel->children->is_atom))
return FALSE;
window = skel->children->next;
len = svn_fs_base__list_length(window);
if ((len < 3) || (len > 4))
return FALSE;
if (! ((! window->children->is_atom)
&& (window->children->next->is_atom)
&& (window->children->next->next->is_atom)))
return FALSE;
if ((len == 4)
&& (! window->children->next->next->next->is_atom))
return FALSE;
diff = window->children;
if ((svn_fs_base__list_length(diff) == 3)
&& (svn_fs_base__matches_atom(diff->children, "svndiff"))
&& ((svn_fs_base__matches_atom(diff->children->next, "0"))
|| (svn_fs_base__matches_atom(diff->children->next, "1")))
&& (diff->children->next->next->is_atom))
return TRUE;
return FALSE;
}
static svn_boolean_t
is_valid_representation_skel(skel_t *skel) {
int len = svn_fs_base__list_length(skel);
skel_t *header;
int header_len;
if (len < 2)
return FALSE;
header = skel->children;
header_len = svn_fs_base__list_length(header);
if (! (((header_len == 2)
&& (header->children->is_atom)
&& (header->children->next->is_atom))
|| ((header_len == 3)
&& (header->children->is_atom)
&& (header->children->next->is_atom)
&& (is_valid_checksum_skel(header->children->next->next)))))
return FALSE;
if ((len == 2)
&& (svn_fs_base__matches_atom(header->children, "fulltext")))
return TRUE;
if ((len >= 2)
&& (svn_fs_base__matches_atom(header->children, "delta"))) {
skel_t *chunk = skel->children->next;
while (chunk) {
if (! is_valid_rep_delta_chunk_skel(chunk))
return FALSE;
chunk = chunk->next;
}
return TRUE;
}
return FALSE;
}
static svn_boolean_t
is_valid_node_revision_header_skel(skel_t *skel, skel_t **kind_p) {
int len = svn_fs_base__list_length(skel);
if (len < 2)
return FALSE;
*kind_p = skel->children;
if (! ((len == 2) || (len == 3) || (len == 4) || (len == 6)))
return FALSE;
if ((len > 4)
&& (! (skel->children->next->next->next->next->is_atom
&& skel->children->next->next->next->next->next->is_atom)))
return FALSE;
if ((len > 3)
&& (! skel->children->next->next->next->is_atom))
return FALSE;
if ((len > 2)
&& (! skel->children->next->next->is_atom))
return FALSE;
if (! (skel->children->is_atom
&& skel->children->next->is_atom
&& (skel->children->next->data[0] == '/')))
return FALSE;
return TRUE;
}
static svn_boolean_t
is_valid_node_revision_skel(skel_t *skel) {
int len = svn_fs_base__list_length(skel);
if (len >= 1) {
skel_t *header = skel->children;
skel_t *kind;
if (is_valid_node_revision_header_skel(header, &kind)) {
if (svn_fs_base__matches_atom(kind, "dir")
&& len == 3
&& header->next->is_atom
&& header->next->next->is_atom)
return TRUE;
if (svn_fs_base__matches_atom(kind, "file")
&& ((len == 3) || (len == 4))
&& header->next->is_atom
&& header->next->next->is_atom) {
if ((len == 4) && (! header->next->next->next->is_atom))
return FALSE;
return TRUE;
}
}
}
return FALSE;
}
static svn_boolean_t
is_valid_copy_skel(skel_t *skel) {
return (((svn_fs_base__list_length(skel) == 4)
&& (svn_fs_base__matches_atom(skel->children, "copy")
|| svn_fs_base__matches_atom(skel->children, "soft-copy"))
&& skel->children->next->is_atom
&& skel->children->next->next->is_atom
&& skel->children->next->next->next->is_atom) ? TRUE : FALSE);
}
static svn_boolean_t
is_valid_change_skel(skel_t *skel, svn_fs_path_change_kind_t *kind) {
if ((svn_fs_base__list_length(skel) == 6)
&& svn_fs_base__matches_atom(skel->children, "change")
&& skel->children->next->is_atom
&& skel->children->next->next->is_atom
&& skel->children->next->next->next->is_atom
&& skel->children->next->next->next->next->is_atom
&& skel->children->next->next->next->next->next->is_atom) {
skel_t *kind_skel = skel->children->next->next->next;
if (svn_fs_base__matches_atom(kind_skel, "reset")) {
if (kind)
*kind = svn_fs_path_change_reset;
return TRUE;
}
if (svn_fs_base__matches_atom(kind_skel, "add")) {
if (kind)
*kind = svn_fs_path_change_add;
return TRUE;
}
if (svn_fs_base__matches_atom(kind_skel, "delete")) {
if (kind)
*kind = svn_fs_path_change_delete;
return TRUE;
}
if (svn_fs_base__matches_atom(kind_skel, "replace")) {
if (kind)
*kind = svn_fs_path_change_replace;
return TRUE;
}
if (svn_fs_base__matches_atom(kind_skel, "modify")) {
if (kind)
*kind = svn_fs_path_change_modify;
return TRUE;
}
}
return FALSE;
}
static svn_boolean_t
is_valid_lock_skel(skel_t *skel) {
if ((svn_fs_base__list_length(skel) == 8)
&& svn_fs_base__matches_atom(skel->children, "lock")
&& skel->children->next->is_atom
&& skel->children->next->next->is_atom
&& skel->children->next->next->next->is_atom
&& skel->children->next->next->next->next->is_atom
&& skel->children->next->next->next->next->next->is_atom
&& skel->children->next->next->next->next->next->next->is_atom
&& skel->children->next->next->next->next->next->next->next->is_atom)
return TRUE;
return FALSE;
}
svn_error_t *
svn_fs_base__parse_proplist_skel(apr_hash_t **proplist_p,
skel_t *skel,
apr_pool_t *pool) {
apr_hash_t *proplist = NULL;
skel_t *elt;
if (! is_valid_proplist_skel(skel))
return skel_err("proplist");
if (skel->children)
proplist = apr_hash_make(pool);
for (elt = skel->children; elt; elt = elt->next->next) {
svn_string_t *value = svn_string_ncreate(elt->next->data,
elt->next->len, pool);
apr_hash_set(proplist,
apr_pstrmemdup(pool, elt->data, elt->len),
elt->len,
value);
}
*proplist_p = proplist;
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_base__parse_revision_skel(revision_t **revision_p,
skel_t *skel,
apr_pool_t *pool) {
revision_t *revision;
if (! is_valid_revision_skel(skel))
return skel_err("revision");
revision = apr_pcalloc(pool, sizeof(*revision));
revision->txn_id = apr_pstrmemdup(pool, skel->children->next->data,
skel->children->next->len);
*revision_p = revision;
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_base__parse_transaction_skel(transaction_t **transaction_p,
skel_t *skel,
apr_pool_t *pool) {
transaction_t *transaction;
transaction_kind_t kind;
skel_t *root_id, *base_id_or_rev, *proplist, *copies;
int len;
if (! is_valid_transaction_skel(skel, &kind))
return skel_err("transaction");
root_id = skel->children->next;
base_id_or_rev = skel->children->next->next;
proplist = skel->children->next->next->next;
copies = skel->children->next->next->next->next;
transaction = apr_pcalloc(pool, sizeof(*transaction));
transaction->kind = kind;
if (kind == transaction_kind_committed) {
transaction->base_id = NULL;
transaction->revision = atoi(apr_pstrmemdup(pool, base_id_or_rev->data,
base_id_or_rev->len));
if (! SVN_IS_VALID_REVNUM(transaction->revision))
return skel_err("transaction");
} else {
transaction->revision = SVN_INVALID_REVNUM;
transaction->base_id = svn_fs_base__id_parse(base_id_or_rev->data,
base_id_or_rev->len, pool);
}
transaction->root_id = svn_fs_base__id_parse(root_id->data,
root_id->len, pool);
SVN_ERR(svn_fs_base__parse_proplist_skel(&(transaction->proplist),
proplist, pool));
if ((len = svn_fs_base__list_length(copies))) {
const char *copy_id;
apr_array_header_t *txncopies;
skel_t *cpy = copies->children;
txncopies = apr_array_make(pool, len, sizeof(copy_id));
while (cpy) {
copy_id = apr_pstrmemdup(pool, cpy->data, cpy->len);
APR_ARRAY_PUSH(txncopies, const char *) = copy_id;
cpy = cpy->next;
}
transaction->copies = txncopies;
}
*transaction_p = transaction;
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_base__parse_representation_skel(representation_t **rep_p,
skel_t *skel,
apr_pool_t *pool) {
representation_t *rep;
skel_t *header_skel;
if (! is_valid_representation_skel(skel))
return skel_err("representation");
header_skel = skel->children;
rep = apr_pcalloc(pool, sizeof(*rep));
if (svn_fs_base__matches_atom(header_skel->children, "fulltext"))
rep->kind = rep_kind_fulltext;
else
rep->kind = rep_kind_delta;
rep->txn_id = apr_pstrmemdup(pool, header_skel->children->next->data,
header_skel->children->next->len);
if (header_skel->children->next->next) {
memcpy(rep->checksum,
header_skel->children->next->next->children->next->data,
APR_MD5_DIGESTSIZE);
} else {
memset(rep->checksum, 0, APR_MD5_DIGESTSIZE);
}
if (rep->kind == rep_kind_fulltext) {
rep->contents.fulltext.string_key
= apr_pstrmemdup(pool,
skel->children->next->data,
skel->children->next->len);
} else {
skel_t *chunk_skel = skel->children->next;
rep_delta_chunk_t *chunk;
apr_array_header_t *chunks;
chunks = apr_array_make(pool, svn_fs_base__list_length(skel) - 1,
sizeof(chunk));
while (chunk_skel) {
skel_t *window_skel = chunk_skel->children->next;
skel_t *diff_skel = window_skel->children;
chunk = apr_palloc(pool, sizeof(*chunk));
chunk->version
= (apr_byte_t)atoi(apr_pstrmemdup
(pool,
diff_skel->children->next->data,
diff_skel->children->next->len));
chunk->string_key
= apr_pstrmemdup(pool,
diff_skel->children->next->next->data,
diff_skel->children->next->next->len);
chunk->size
= atoi(apr_pstrmemdup(pool,
window_skel->children->next->data,
window_skel->children->next->len));
chunk->rep_key
= apr_pstrmemdup(pool,
window_skel->children->next->next->data,
window_skel->children->next->next->len);
chunk->offset =
svn__atoui64(apr_pstrmemdup(pool,
chunk_skel->children->data,
chunk_skel->children->len));
APR_ARRAY_PUSH(chunks, rep_delta_chunk_t *) = chunk;
chunk_skel = chunk_skel->next;
}
rep->contents.delta.chunks = chunks;
}
*rep_p = rep;
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_base__parse_node_revision_skel(node_revision_t **noderev_p,
skel_t *skel,
apr_pool_t *pool) {
node_revision_t *noderev;
skel_t *header_skel, *cur_skel;
if (! is_valid_node_revision_skel(skel))
return skel_err("node-revision");
header_skel = skel->children;
noderev = apr_pcalloc(pool, sizeof(*noderev));
if (svn_fs_base__matches_atom(header_skel->children, "dir"))
noderev->kind = svn_node_dir;
else
noderev->kind = svn_node_file;
noderev->created_path = apr_pstrmemdup(pool,
header_skel->children->next->data,
header_skel->children->next->len);
if (header_skel->children->next->next) {
cur_skel = header_skel->children->next->next;
if (cur_skel->len)
noderev->predecessor_id = svn_fs_base__id_parse(cur_skel->data,
cur_skel->len, pool);
noderev->predecessor_count = -1;
if (cur_skel->next) {
cur_skel = cur_skel->next;
if (cur_skel->len)
noderev->predecessor_count = atoi(apr_pstrmemdup(pool,
cur_skel->data,
cur_skel->len));
if (cur_skel->next) {
cur_skel = cur_skel->next;
noderev->has_mergeinfo = atoi(apr_pstrmemdup(pool,
cur_skel->data,
cur_skel->len))
? TRUE : FALSE;
noderev->mergeinfo_count =
apr_atoi64(apr_pstrmemdup(pool,
cur_skel->next->data,
cur_skel->next->len));
}
}
}
if (skel->children->next->len)
noderev->prop_key = apr_pstrmemdup(pool, skel->children->next->data,
skel->children->next->len);
if (skel->children->next->next->len)
noderev->data_key = apr_pstrmemdup(pool, skel->children->next->next->data,
skel->children->next->next->len);
if ((noderev->kind == svn_node_file)
&& skel->children->next->next->next
&& skel->children->next->next->next->len)
noderev->edit_key
= apr_pstrmemdup(pool, skel->children->next->next->next->data,
skel->children->next->next->next->len);
*noderev_p = noderev;
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_base__parse_copy_skel(copy_t **copy_p,
skel_t *skel,
apr_pool_t *pool) {
copy_t *copy;
if (! is_valid_copy_skel(skel))
return skel_err("copy");
copy = apr_pcalloc(pool, sizeof(*copy));
if (svn_fs_base__matches_atom(skel->children, "soft-copy"))
copy->kind = copy_kind_soft;
else
copy->kind = copy_kind_real;
copy->src_path = apr_pstrmemdup(pool,
skel->children->next->data,
skel->children->next->len);
copy->src_txn_id = apr_pstrmemdup(pool,
skel->children->next->next->data,
skel->children->next->next->len);
copy->dst_noderev_id
= svn_fs_base__id_parse(skel->children->next->next->next->data,
skel->children->next->next->next->len, pool);
*copy_p = copy;
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_base__parse_entries_skel(apr_hash_t **entries_p,
skel_t *skel,
apr_pool_t *pool) {
apr_hash_t *entries = NULL;
int len = svn_fs_base__list_length(skel);
skel_t *elt;
if (! (len >= 0))
return skel_err("entries");
if (len > 0) {
entries = apr_hash_make(pool);
for (elt = skel->children; elt; elt = elt->next) {
const char *name;
svn_fs_id_t *id;
if (svn_fs_base__list_length(elt) != 2)
return skel_err("entries");
name = apr_pstrmemdup(pool, elt->children->data,
elt->children->len);
id = svn_fs_base__id_parse(elt->children->next->data,
elt->children->next->len, pool);
apr_hash_set(entries, name, elt->children->len, id);
}
}
*entries_p = entries;
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_base__parse_change_skel(change_t **change_p,
skel_t *skel,
apr_pool_t *pool) {
change_t *change;
svn_fs_path_change_kind_t kind;
if (! is_valid_change_skel(skel, &kind))
return skel_err("change");
change = apr_pcalloc(pool, sizeof(*change));
change->path = apr_pstrmemdup(pool, skel->children->next->data,
skel->children->next->len);
if (skel->children->next->next->len)
change->noderev_id = svn_fs_base__id_parse
(skel->children->next->next->data, skel->children->next->next->len,
pool);
change->kind = kind;
if (skel->children->next->next->next->next->len)
change->text_mod = TRUE;
if (skel->children->next->next->next->next->next->len)
change->prop_mod = TRUE;
*change_p = change;
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_base__parse_lock_skel(svn_lock_t **lock_p,
skel_t *skel,
apr_pool_t *pool) {
svn_lock_t *lock;
const char *timestr;
if (! is_valid_lock_skel(skel))
return skel_err("lock");
lock = apr_pcalloc(pool, sizeof(*lock));
lock->path = apr_pstrmemdup(pool, skel->children->next->data,
skel->children->next->len);
lock->token = apr_pstrmemdup(pool,
skel->children->next->next->data,
skel->children->next->next->len);
lock->owner = apr_pstrmemdup(pool,
skel->children->next->next->next->data,
skel->children->next->next->next->len);
if (skel->children->next->next->next->next->len)
lock->comment =
apr_pstrmemdup(pool,
skel->children->next->next->next->next->data,
skel->children->next->next->next->next->len);
if (svn_fs_base__matches_atom
(skel->children->next->next->next->next->next, "1"))
lock->is_dav_comment = TRUE;
else
lock->is_dav_comment = FALSE;
timestr = apr_pstrmemdup
(pool,
skel->children->next->next->next->next->next->next->data,
skel->children->next->next->next->next->next->next->len);
SVN_ERR(svn_time_from_cstring(&(lock->creation_date),
timestr, pool));
if (skel->children->next->next->next->next->next->next->next->len) {
timestr =
apr_pstrmemdup
(pool,
skel->children->next->next->next->next->next->next->next->data,
skel->children->next->next->next->next->next->next->next->len);
SVN_ERR(svn_time_from_cstring(&(lock->expiration_date),
timestr, pool));
}
*lock_p = lock;
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_base__unparse_proplist_skel(skel_t **skel_p,
apr_hash_t *proplist,
apr_pool_t *pool) {
skel_t *skel = svn_fs_base__make_empty_list(pool);
apr_hash_index_t *hi;
if (proplist) {
for (hi = apr_hash_first(pool, proplist); hi; hi = apr_hash_next(hi)) {
const void *key;
void *val;
apr_ssize_t klen;
svn_string_t *value;
apr_hash_this(hi, &key, &klen, &val);
value = val;
svn_fs_base__prepend(svn_fs_base__mem_atom(value->data,
value->len, pool),
skel);
svn_fs_base__prepend(svn_fs_base__mem_atom(key, klen, pool), skel);
}
}
if (! is_valid_proplist_skel(skel))
return skel_err("proplist");
*skel_p = skel;
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_base__unparse_revision_skel(skel_t **skel_p,
const revision_t *revision,
apr_pool_t *pool) {
skel_t *skel;
skel = svn_fs_base__make_empty_list(pool);
svn_fs_base__prepend(svn_fs_base__str_atom(revision->txn_id, pool), skel);
svn_fs_base__prepend(svn_fs_base__str_atom("revision", pool), skel);
if (! is_valid_revision_skel(skel))
return skel_err("revision");
*skel_p = skel;
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_base__unparse_transaction_skel(skel_t **skel_p,
const transaction_t *transaction,
apr_pool_t *pool) {
skel_t *skel;
skel_t *proplist_skel, *copies_skel, *header_skel;
svn_string_t *id_str;
transaction_kind_t kind;
skel = svn_fs_base__make_empty_list(pool);
switch (transaction->kind) {
case transaction_kind_committed:
header_skel = svn_fs_base__str_atom("committed", pool);
if ((transaction->base_id)
|| (! SVN_IS_VALID_REVNUM(transaction->revision)))
return skel_err("transaction");
break;
case transaction_kind_dead:
header_skel = svn_fs_base__str_atom("dead", pool);
if ((! transaction->base_id)
|| (SVN_IS_VALID_REVNUM(transaction->revision)))
return skel_err("transaction");
break;
case transaction_kind_normal:
header_skel = svn_fs_base__str_atom("transaction", pool);
if ((! transaction->base_id)
|| (SVN_IS_VALID_REVNUM(transaction->revision)))
return skel_err("transaction");
break;
default:
return skel_err("transaction");
}
copies_skel = svn_fs_base__make_empty_list(pool);
if (transaction->copies && transaction->copies->nelts) {
int i;
for (i = transaction->copies->nelts - 1; i >= 0; i--) {
svn_fs_base__prepend(svn_fs_base__str_atom
(APR_ARRAY_IDX(transaction->copies, i,
const char *), pool),
copies_skel);
}
}
svn_fs_base__prepend(copies_skel, skel);
SVN_ERR(svn_fs_base__unparse_proplist_skel(&proplist_skel,
transaction->proplist, pool));
svn_fs_base__prepend(proplist_skel, skel);
if (transaction->kind == transaction_kind_committed) {
svn_fs_base__prepend(svn_fs_base__str_atom
(apr_psprintf(pool, "%ld",
transaction->revision), pool),
skel);
} else {
id_str = svn_fs_base__id_unparse(transaction->base_id, pool);
svn_fs_base__prepend(svn_fs_base__mem_atom(id_str->data, id_str->len,
pool), skel);
}
id_str = svn_fs_base__id_unparse(transaction->root_id, pool);
svn_fs_base__prepend(svn_fs_base__mem_atom(id_str->data, id_str->len,
pool), skel);
svn_fs_base__prepend(header_skel, skel);
if (! is_valid_transaction_skel(skel, &kind))
return skel_err("transaction");
if (kind != transaction->kind)
return skel_err("transaction");
*skel_p = skel;
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_base__unparse_representation_skel(skel_t **skel_p,
const representation_t *rep,
apr_pool_t *pool) {
skel_t *skel = svn_fs_base__make_empty_list(pool);
skel_t *header_skel = svn_fs_base__make_empty_list(pool);
{
skel_t *checksum_skel = svn_fs_base__make_empty_list(pool);
svn_fs_base__prepend(svn_fs_base__mem_atom
(rep->checksum, APR_MD5_DIGESTSIZE, pool),
checksum_skel);
svn_fs_base__prepend(svn_fs_base__str_atom("md5", pool), checksum_skel);
svn_fs_base__prepend(checksum_skel, header_skel);
}
if (rep->txn_id)
svn_fs_base__prepend(svn_fs_base__str_atom(rep->txn_id, pool),
header_skel);
else
svn_fs_base__prepend(svn_fs_base__mem_atom(NULL, 0, pool), header_skel);
if (rep->kind == rep_kind_fulltext) {
if ((! rep->contents.fulltext.string_key)
|| (! *rep->contents.fulltext.string_key))
svn_fs_base__prepend(svn_fs_base__mem_atom(NULL, 0, pool), skel);
else
svn_fs_base__prepend(svn_fs_base__str_atom
(rep->contents.fulltext.string_key, pool), skel);
svn_fs_base__prepend(svn_fs_base__str_atom("fulltext", pool),
header_skel);
svn_fs_base__prepend(header_skel, skel);
} else if (rep->kind == rep_kind_delta) {
int i;
apr_array_header_t *chunks = rep->contents.delta.chunks;
for (i = chunks->nelts; i > 0; i--) {
skel_t *window_skel = svn_fs_base__make_empty_list(pool);
skel_t *chunk_skel = svn_fs_base__make_empty_list(pool);
skel_t *diff_skel = svn_fs_base__make_empty_list(pool);
const char *size_str, *offset_str, *version_str;
rep_delta_chunk_t *chunk = APR_ARRAY_IDX(chunks, i - 1,
rep_delta_chunk_t *);
offset_str = apr_psprintf(pool, "%" SVN_FILESIZE_T_FMT,
chunk->offset);
size_str = apr_psprintf(pool, "%" APR_SIZE_T_FMT, chunk->size);
version_str = apr_psprintf(pool, "%d", chunk->version);
if ((! chunk->string_key) || (! *chunk->string_key))
svn_fs_base__prepend(svn_fs_base__mem_atom(NULL, 0, pool),
diff_skel);
else
svn_fs_base__prepend(svn_fs_base__str_atom(chunk->string_key,
pool), diff_skel);
svn_fs_base__prepend(svn_fs_base__str_atom(version_str, pool),
diff_skel);
svn_fs_base__prepend(svn_fs_base__str_atom("svndiff", pool),
diff_skel);
if ((! chunk->rep_key) || (! *(chunk->rep_key)))
svn_fs_base__prepend(svn_fs_base__mem_atom(NULL, 0, pool),
window_skel);
else
svn_fs_base__prepend(svn_fs_base__str_atom(chunk->rep_key,
pool),
window_skel);
svn_fs_base__prepend(svn_fs_base__str_atom(size_str, pool),
window_skel);
svn_fs_base__prepend(diff_skel, window_skel);
svn_fs_base__prepend(window_skel, chunk_skel);
svn_fs_base__prepend(svn_fs_base__str_atom(offset_str, pool),
chunk_skel);
svn_fs_base__prepend(chunk_skel, skel);
}
svn_fs_base__prepend(svn_fs_base__str_atom("delta", pool),
header_skel);
svn_fs_base__prepend(header_skel, skel);
} else
abort();
if (! is_valid_representation_skel(skel))
return skel_err("representation");
*skel_p = skel;
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_base__unparse_node_revision_skel(skel_t **skel_p,
const node_revision_t *noderev,
int format,
apr_pool_t *pool) {
skel_t *skel;
skel_t *header_skel;
const char *num_str;
skel = svn_fs_base__make_empty_list(pool);
header_skel = svn_fs_base__make_empty_list(pool);
if (format >= SVN_FS_BASE__MIN_MERGEINFO_FORMAT) {
num_str = apr_psprintf(pool, "%" APR_INT64_T_FMT,
noderev->mergeinfo_count);
svn_fs_base__prepend(svn_fs_base__str_atom(num_str, pool), header_skel);
svn_fs_base__prepend(svn_fs_base__mem_atom(noderev->has_mergeinfo
? "1" : "0",
1, pool), header_skel);
if (noderev->predecessor_count == -1) {
svn_fs_base__prepend(svn_fs_base__mem_atom(NULL, 0, pool),
header_skel);
}
}
if (noderev->predecessor_count != -1) {
const char *count_str = apr_psprintf(pool, "%d",
noderev->predecessor_count);
svn_fs_base__prepend(svn_fs_base__str_atom(count_str, pool),
header_skel);
}
if (noderev->predecessor_id) {
svn_string_t *id_str = svn_fs_base__id_unparse(noderev->predecessor_id,
pool);
svn_fs_base__prepend(svn_fs_base__mem_atom(id_str->data, id_str->len,
pool),
header_skel);
} else {
svn_fs_base__prepend(svn_fs_base__mem_atom(NULL, 0, pool), header_skel);
}
svn_fs_base__prepend(svn_fs_base__str_atom(noderev->created_path, pool),
header_skel);
if (noderev->kind == svn_node_file)
svn_fs_base__prepend(svn_fs_base__str_atom("file", pool), header_skel);
else if (noderev->kind == svn_node_dir)
svn_fs_base__prepend(svn_fs_base__str_atom("dir", pool), header_skel);
else
abort();
if ((noderev->edit_key) && (*noderev->edit_key))
svn_fs_base__prepend(svn_fs_base__str_atom(noderev->edit_key, pool),
skel);
if ((noderev->data_key) && (*noderev->data_key))
svn_fs_base__prepend(svn_fs_base__str_atom(noderev->data_key, pool),
skel);
else
svn_fs_base__prepend(svn_fs_base__mem_atom(NULL, 0, pool), skel);
if ((noderev->prop_key) && (*noderev->prop_key))
svn_fs_base__prepend(svn_fs_base__str_atom(noderev->prop_key, pool),
skel);
else
svn_fs_base__prepend(svn_fs_base__mem_atom(NULL, 0, pool), skel);
svn_fs_base__prepend(header_skel, skel);
if (! is_valid_node_revision_skel(skel))
return skel_err("node-revision");
*skel_p = skel;
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_base__unparse_copy_skel(skel_t **skel_p,
const copy_t *copy,
apr_pool_t *pool) {
skel_t *skel;
svn_string_t *tmp_str;
skel = svn_fs_base__make_empty_list(pool);
tmp_str = svn_fs_base__id_unparse(copy->dst_noderev_id, pool);
svn_fs_base__prepend(svn_fs_base__mem_atom(tmp_str->data, tmp_str->len,
pool), skel);
if ((copy->src_txn_id) && (*copy->src_txn_id))
svn_fs_base__prepend(svn_fs_base__str_atom(copy->src_txn_id, pool),
skel);
else
svn_fs_base__prepend(svn_fs_base__mem_atom(NULL, 0, pool), skel);
if ((copy->src_path) && (*copy->src_path))
svn_fs_base__prepend(svn_fs_base__str_atom(copy->src_path, pool), skel);
else
svn_fs_base__prepend(svn_fs_base__mem_atom(NULL, 0, pool), skel);
if (copy->kind == copy_kind_real)
svn_fs_base__prepend(svn_fs_base__str_atom("copy", pool), skel);
else
svn_fs_base__prepend(svn_fs_base__str_atom("soft-copy", pool), skel);
if (! is_valid_copy_skel(skel))
return skel_err("copy");
*skel_p = skel;
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_base__unparse_entries_skel(skel_t **skel_p,
apr_hash_t *entries,
apr_pool_t *pool) {
skel_t *skel = svn_fs_base__make_empty_list(pool);
apr_hash_index_t *hi;
if (entries) {
for (hi = apr_hash_first(pool, entries); hi; hi = apr_hash_next(hi)) {
const void *key;
void *val;
apr_ssize_t klen;
svn_fs_id_t *value;
svn_string_t *id_str;
skel_t *entry_skel = svn_fs_base__make_empty_list(pool);
apr_hash_this(hi, &key, &klen, &val);
value = val;
id_str = svn_fs_base__id_unparse(value, pool);
svn_fs_base__prepend(svn_fs_base__mem_atom(id_str->data,
id_str->len, pool),
entry_skel);
svn_fs_base__prepend(svn_fs_base__mem_atom(key, klen, pool),
entry_skel);
svn_fs_base__prepend(entry_skel, skel);
}
}
*skel_p = skel;
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_base__unparse_change_skel(skel_t **skel_p,
const change_t *change,
apr_pool_t *pool) {
skel_t *skel;
svn_string_t *tmp_str;
svn_fs_path_change_kind_t kind;
skel = svn_fs_base__make_empty_list(pool);
if (change->prop_mod)
svn_fs_base__prepend(svn_fs_base__str_atom("1", pool), skel);
else
svn_fs_base__prepend(svn_fs_base__mem_atom(NULL, 0, pool), skel);
if (change->text_mod)
svn_fs_base__prepend(svn_fs_base__str_atom("1", pool), skel);
else
svn_fs_base__prepend(svn_fs_base__mem_atom(NULL, 0, pool), skel);
switch (change->kind) {
case svn_fs_path_change_reset:
svn_fs_base__prepend(svn_fs_base__str_atom("reset", pool), skel);
break;
case svn_fs_path_change_add:
svn_fs_base__prepend(svn_fs_base__str_atom("add", pool), skel);
break;
case svn_fs_path_change_delete:
svn_fs_base__prepend(svn_fs_base__str_atom("delete", pool), skel);
break;
case svn_fs_path_change_replace:
svn_fs_base__prepend(svn_fs_base__str_atom("replace", pool), skel);
break;
case svn_fs_path_change_modify:
default:
svn_fs_base__prepend(svn_fs_base__str_atom("modify", pool), skel);
break;
}
if (change->noderev_id) {
tmp_str = svn_fs_base__id_unparse(change->noderev_id, pool);
svn_fs_base__prepend(svn_fs_base__mem_atom(tmp_str->data,
tmp_str->len, pool),
skel);
} else {
svn_fs_base__prepend(svn_fs_base__mem_atom(NULL, 0, pool), skel);
}
svn_fs_base__prepend(svn_fs_base__str_atom(change->path, pool), skel);
svn_fs_base__prepend(svn_fs_base__str_atom("change", pool), skel);
if (! is_valid_change_skel(skel, &kind))
return skel_err("change");
if (kind != change->kind)
return skel_err("change");
*skel_p = skel;
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_base__unparse_lock_skel(skel_t **skel_p,
const svn_lock_t *lock,
apr_pool_t *pool) {
skel_t *skel;
skel = svn_fs_base__make_empty_list(pool);
if (lock->expiration_date)
svn_fs_base__prepend
(svn_fs_base__str_atom
(svn_time_to_cstring(lock->expiration_date, pool), pool), skel);
else
svn_fs_base__prepend(svn_fs_base__mem_atom(NULL, 0, pool), skel);
svn_fs_base__prepend
(svn_fs_base__str_atom
(svn_time_to_cstring(lock->creation_date, pool), pool), skel);
if (lock->is_dav_comment)
svn_fs_base__prepend(svn_fs_base__str_atom("1", pool), skel);
else
svn_fs_base__prepend(svn_fs_base__str_atom("0", pool), skel);
if (lock->comment)
svn_fs_base__prepend(svn_fs_base__str_atom(lock->comment, pool), skel);
else
svn_fs_base__prepend(svn_fs_base__mem_atom(NULL, 0, pool), skel);
svn_fs_base__prepend(svn_fs_base__str_atom(lock->owner, pool), skel);
svn_fs_base__prepend(svn_fs_base__str_atom(lock->token, pool), skel);
svn_fs_base__prepend(svn_fs_base__str_atom(lock->path, pool), skel);
svn_fs_base__prepend(svn_fs_base__str_atom("lock", pool), skel);
if (! is_valid_lock_skel(skel))
return skel_err("lock");
*skel_p = skel;
return SVN_NO_ERROR;
}
