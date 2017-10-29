#if !defined(SVN_LIBSVN_FS_BASE_H)
#define SVN_LIBSVN_FS_BASE_H
#define APU_WANT_DB
#include <apu_want.h>
#include <apr_pools.h>
#include <apr_hash.h>
#include <apr_md5.h>
#include "svn_fs.h"
#include "bdb/env.h"
#if defined(__cplusplus)
extern "C" {
#endif
#define SVN_FS_BASE__FORMAT_NUMBER 3
#define SVN_FS_BASE__MIN_NODE_ORIGINS_FORMAT 3
#define SVN_FS_BASE__MIN_MERGEINFO_FORMAT 3
#define SVN_FS_BASE__MIN_SVNDIFF1_FORMAT 2
svn_error_t *
svn_fs_base__test_required_feature_format(svn_fs_t *fs,
const char *feature,
int requires);
typedef struct {
bdb_env_baton_t *bdb;
DB *changes;
DB *copies;
DB *nodes;
DB *representations;
DB *revisions;
DB *strings;
DB *transactions;
DB *uuids;
DB *locks;
DB *lock_tokens;
DB *node_origins;
svn_boolean_t in_txn_trail;
const char *uuid;
int format;
} base_fs_data_t;
typedef struct {
const char *txn_id;
} revision_t;
typedef enum {
transaction_kind_normal = 1,
transaction_kind_committed,
transaction_kind_dead
} transaction_kind_t;
typedef struct {
transaction_kind_t kind;
svn_revnum_t revision;
apr_hash_t *proplist;
const svn_fs_id_t *root_id;
const svn_fs_id_t *base_id;
apr_array_header_t *copies;
} transaction_t;
typedef struct {
svn_node_kind_t kind;
const svn_fs_id_t *predecessor_id;
int predecessor_count;
const char *prop_key;
const char *data_key;
const char *edit_key;
const char *created_path;
svn_boolean_t has_mergeinfo;
apr_int64_t mergeinfo_count;
} node_revision_t;
typedef enum {
rep_kind_fulltext = 1,
rep_kind_delta
} rep_kind_t;
typedef struct {
apr_byte_t version;
svn_filesize_t offset;
const char *string_key;
apr_size_t size;
const char *rep_key;
} rep_delta_chunk_t;
typedef struct {
rep_kind_t kind;
const char *txn_id;
unsigned char checksum[APR_MD5_DIGESTSIZE];
union {
struct {
const char *string_key;
} fulltext;
struct {
apr_array_header_t *chunks;
} delta;
} contents;
} representation_t;
typedef enum {
copy_kind_real = 1,
copy_kind_soft
} copy_kind_t;
typedef struct {
copy_kind_t kind;
const char *src_path;
const char *src_txn_id;
const svn_fs_id_t *dst_noderev_id;
} copy_t;
typedef struct {
const char *path;
const svn_fs_id_t *noderev_id;
svn_fs_path_change_kind_t kind;
svn_boolean_t text_mod;
svn_boolean_t prop_mod;
} change_t;
typedef struct {
apr_hash_t *entries;
const char *lock_token;
} lock_node_t;
#if defined(__cplusplus)
}
#endif
#endif
