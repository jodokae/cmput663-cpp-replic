#if !defined(SVN_LIBSVN_FS_FS_H)
#define SVN_LIBSVN_FS_FS_H
#include <apr_pools.h>
#include <apr_hash.h>
#include <apr_md5.h>
#include <apr_thread_mutex.h>
#include <apr_network_io.h>
#include "svn_fs.h"
#include "private/svn_fs_private.h"
#if defined(__cplusplus)
extern "C" {
#endif
#define PATH_FORMAT "format"
#define PATH_UUID "uuid"
#define PATH_CURRENT "current"
#define PATH_LOCK_FILE "write-lock"
#define PATH_REVS_DIR "revs"
#define PATH_REVPROPS_DIR "revprops"
#define PATH_TXNS_DIR "transactions"
#define PATH_NODE_ORIGINS_DIR "node-origins"
#define PATH_TXN_PROTOS_DIR "txn-protorevs"
#define PATH_TXN_CURRENT "txn-current"
#define PATH_TXN_CURRENT_LOCK "txn-current-lock"
#define PATH_LOCKS_DIR "locks"
#define PATH_CHANGES "changes"
#define PATH_TXN_PROPS "props"
#define PATH_NEXT_IDS "next-ids"
#define PATH_PREFIX_NODE "node."
#define PATH_EXT_TXN ".txn"
#define PATH_EXT_CHILDREN ".children"
#define PATH_EXT_PROPS ".props"
#define PATH_EXT_REV ".rev"
#define PATH_EXT_REV_LOCK ".rev-lock"
#define PATH_REV "rev"
#define PATH_REV_LOCK "rev-lock"
#define SVN_FS_FS__FORMAT_NUMBER 3
#define SVN_FS_FS__MIN_SVNDIFF1_FORMAT 2
#define SVN_FS_FS__MIN_TXN_CURRENT_FORMAT 3
#define SVN_FS_FS__MIN_LAYOUT_FORMAT_OPTION_FORMAT 3
#define SVN_FS_FS__MIN_PROTOREVS_DIR_FORMAT 3
#define SVN_FS_FS__MIN_NO_GLOBAL_IDS_FORMAT 3
#define SVN_FS_FS__MIN_MERGEINFO_FORMAT 3
#define NUM_DIR_CACHE_ENTRIES 128
#define DIR_CACHE_ENTRIES_MASK(x) ((x) & (NUM_DIR_CACHE_ENTRIES - 1))
#define NUM_RRI_CACHE_ENTRIES 4096
struct fs_fs_shared_txn_data_t;
typedef struct fs_fs_shared_txn_data_t {
struct fs_fs_shared_txn_data_t *next;
char txn_id[SVN_FS__TXN_MAX_LEN+1];
svn_boolean_t being_written;
apr_pool_t *pool;
} fs_fs_shared_txn_data_t;
typedef struct {
fs_fs_shared_txn_data_t *txns;
fs_fs_shared_txn_data_t *free_txn;
#if APR_HAS_THREADS
apr_thread_mutex_t *txn_list_lock;
apr_thread_mutex_t *fs_write_lock;
apr_thread_mutex_t *txn_current_lock;
#endif
apr_pool_t *common_pool;
} fs_fs_shared_data_t;
typedef struct dag_node_t dag_node_t;
typedef struct dag_node_cache_t {
const char *key;
dag_node_t *node;
struct dag_node_cache_t *prev;
struct dag_node_cache_t *next;
apr_pool_t *pool;
} dag_node_cache_t;
typedef struct {
svn_fs_id_t *dir_cache_id[NUM_DIR_CACHE_ENTRIES];
apr_hash_t *dir_cache[NUM_DIR_CACHE_ENTRIES];
apr_pool_t *dir_cache_pool[NUM_DIR_CACHE_ENTRIES];
int format;
int max_files_per_dir;
const char *uuid;
svn_revnum_t youngest_rev_cache;
apr_hash_t *rev_root_id_cache;
apr_pool_t *rev_root_id_cache_pool;
dag_node_cache_t rev_node_list;
apr_hash_t *rev_node_cache;
fs_fs_shared_data_t *shared;
} fs_fs_data_t;
typedef struct {
apr_hash_t *proplist;
const svn_fs_id_t *root_id;
const svn_fs_id_t *base_id;
apr_array_header_t *copies;
} transaction_t;
typedef struct {
unsigned char checksum[APR_MD5_DIGESTSIZE];
svn_revnum_t revision;
apr_off_t offset;
svn_filesize_t size;
svn_filesize_t expanded_size;
const char *txn_id;
} representation_t;
typedef struct {
svn_node_kind_t kind;
const svn_fs_id_t *id;
const svn_fs_id_t *predecessor_id;
const char *copyfrom_path;
svn_revnum_t copyfrom_rev;
svn_revnum_t copyroot_rev;
const char *copyroot_path;
int predecessor_count;
representation_t *prop_rep;
representation_t *data_rep;
const char *created_path;
svn_boolean_t is_fresh_txn_root;
apr_int64_t mergeinfo_count;
svn_boolean_t has_mergeinfo;
} node_revision_t;
typedef struct {
const char *path;
const svn_fs_id_t *noderev_id;
svn_fs_path_change_kind_t kind;
svn_boolean_t text_mod;
svn_boolean_t prop_mod;
svn_revnum_t copyfrom_rev;
const char * copyfrom_path;
} change_t;
#if defined(__cplusplus)
}
#endif
#endif
