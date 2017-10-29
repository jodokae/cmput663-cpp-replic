#if !defined(SVN_TYPES_H)
#define SVN_TYPES_H
#include <stdlib.h>
#include <apr.h>
#include <apr_pools.h>
#include <apr_hash.h>
#include <apr_tables.h>
#include <apr_time.h>
#if defined(__cplusplus)
extern "C" {
#endif
typedef struct svn_error_t {
apr_status_t apr_err;
const char *message;
struct svn_error_t *child;
apr_pool_t *pool;
const char *file;
long line;
} svn_error_t;
#if !defined(APR_ARRAY_IDX)
#define APR_ARRAY_IDX(ary,i,type) (((type *)(ary)->elts)[i])
#endif
#if !defined(APR_ARRAY_PUSH)
#define APR_ARRAY_PUSH(ary,type) (*((type *)apr_array_push(ary)))
#endif
typedef enum {
svn_node_none,
svn_node_file,
svn_node_dir,
svn_node_unknown
} svn_node_kind_t;
typedef long int svn_revnum_t;
#define SVN_IS_VALID_REVNUM(n) ((n) >= 0)
#define SVN_INVALID_REVNUM ((svn_revnum_t) -1)
#define SVN_IGNORED_REVNUM ((svn_revnum_t) -1)
#define SVN_STR_TO_REV(str) ((svn_revnum_t) atol(str))
svn_error_t *
svn_revnum_parse(svn_revnum_t *rev,
const char *str,
const char **endptr);
#define SVN_REVNUM_T_FMT "ld"
typedef apr_int64_t svn_filesize_t;
#define SVN_INVALID_FILESIZE ((svn_filesize_t) -1)
#define SVN_FILESIZE_T_FMT APR_INT64_T_FMT
#if !defined(DOXYGEN_SHOULD_SKIP_THIS)
#define svn__atoui64(X) ((apr_uint64_t) apr_atoi64(X))
#endif
typedef int svn_boolean_t;
#if !defined(TRUE)
#define TRUE 1
#endif
#if !defined(FALSE)
#define FALSE 0
#endif
enum svn_recurse_kind {
svn_nonrecursive = 1,
svn_recursive
};
typedef enum {
svn_depth_unknown = -2,
svn_depth_exclude = -1,
svn_depth_empty = 0,
svn_depth_files = 1,
svn_depth_immediates = 2,
svn_depth_infinity = 3
} svn_depth_t;
const char *
svn_depth_to_word(svn_depth_t depth);
svn_depth_t
svn_depth_from_word(const char *word);
#define SVN_DEPTH_INFINITY_OR_FILES(recurse) ((recurse) ? svn_depth_infinity : svn_depth_files)
#define SVN_DEPTH_INFINITY_OR_IMMEDIATES(recurse) ((recurse) ? svn_depth_infinity : svn_depth_immediates)
#define SVN_DEPTH_INFINITY_OR_EMPTY(recurse) ((recurse) ? svn_depth_infinity : svn_depth_empty)
#define SVN_DEPTH_IS_RECURSIVE(depth) (((depth) == svn_depth_infinity || (depth) == svn_depth_unknown) ? TRUE : FALSE)
#define SVN_DIRENT_KIND 0x00001
#define SVN_DIRENT_SIZE 0x00002
#define SVN_DIRENT_HAS_PROPS 0x00004
#define SVN_DIRENT_CREATED_REV 0x00008
#define SVN_DIRENT_TIME 0x00010
#define SVN_DIRENT_LAST_AUTHOR 0x00020
#define SVN_DIRENT_ALL ~((apr_uint32_t ) 0)
typedef struct svn_dirent_t {
svn_node_kind_t kind;
svn_filesize_t size;
svn_boolean_t has_props;
svn_revnum_t created_rev;
apr_time_t time;
const char *last_author;
} svn_dirent_t;
svn_dirent_t *svn_dirent_dup(const svn_dirent_t *dirent,
apr_pool_t *pool);
#define SVN_KEYWORD_MAX_LEN 255
#define SVN_KEYWORD_REVISION_LONG "LastChangedRevision"
#define SVN_KEYWORD_REVISION_SHORT "Rev"
#define SVN_KEYWORD_REVISION_MEDIUM "Revision"
#define SVN_KEYWORD_DATE_LONG "LastChangedDate"
#define SVN_KEYWORD_DATE_SHORT "Date"
#define SVN_KEYWORD_AUTHOR_LONG "LastChangedBy"
#define SVN_KEYWORD_AUTHOR_SHORT "Author"
#define SVN_KEYWORD_URL_LONG "HeadURL"
#define SVN_KEYWORD_URL_SHORT "URL"
#define SVN_KEYWORD_ID "Id"
typedef struct svn_commit_info_t {
svn_revnum_t revision;
const char *date;
const char *author;
const char *post_commit_err;
} svn_commit_info_t;
svn_commit_info_t *
svn_create_commit_info(apr_pool_t *pool);
svn_commit_info_t *
svn_commit_info_dup(const svn_commit_info_t *src_commit_info,
apr_pool_t *pool);
typedef struct svn_log_changed_path_t {
char action;
const char *copyfrom_path;
svn_revnum_t copyfrom_rev;
} svn_log_changed_path_t;
svn_log_changed_path_t *
svn_log_changed_path_dup(const svn_log_changed_path_t *changed_path,
apr_pool_t *pool);
typedef struct svn_log_entry_t {
apr_hash_t *changed_paths;
svn_revnum_t revision;
apr_hash_t *revprops;
svn_boolean_t has_children;
} svn_log_entry_t;
svn_log_entry_t *
svn_log_entry_create(apr_pool_t *pool);
typedef svn_error_t *(*svn_log_entry_receiver_t)
(void *baton,
svn_log_entry_t *log_entry,
apr_pool_t *pool);
typedef svn_error_t *(*svn_log_message_receiver_t)
(void *baton,
apr_hash_t *changed_paths,
svn_revnum_t revision,
const char *author,
const char *date,
const char *message,
apr_pool_t *pool);
typedef svn_error_t *(*svn_commit_callback2_t)
(const svn_commit_info_t *commit_info,
void *baton,
apr_pool_t *pool);
typedef svn_error_t *(*svn_commit_callback_t)
(svn_revnum_t new_revision,
const char *date,
const char *author,
void *baton);
#define SVN_STREAM_CHUNK_SIZE 102400
#if !defined(DOXYGEN_SHOULD_SKIP_THIS)
#define SVN__STREAM_CHUNK_SIZE 16384
#endif
#define SVN_MAX_OBJECT_SIZE (((apr_size_t) -1) / 2)
svn_error_t *svn_mime_type_validate(const char *mime_type,
apr_pool_t *pool);
svn_boolean_t svn_mime_type_is_binary(const char *mime_type);
typedef svn_error_t *(*svn_cancel_func_t)(void *cancel_baton);
typedef struct svn_lock_t {
const char *path;
const char *token;
const char *owner;
const char *comment;
svn_boolean_t is_dav_comment;
apr_time_t creation_date;
apr_time_t expiration_date;
} svn_lock_t;
svn_lock_t *
svn_lock_create(apr_pool_t *pool);
svn_lock_t *
svn_lock_dup(const svn_lock_t *lock, apr_pool_t *pool);
const char *
svn_uuid_generate(apr_pool_t *pool);
typedef struct svn_merge_range_t {
svn_revnum_t start;
svn_revnum_t end;
svn_boolean_t inheritable;
} svn_merge_range_t;
svn_merge_range_t *
svn_merge_range_dup(svn_merge_range_t *range, apr_pool_t *pool);
svn_boolean_t
svn_merge_range_contains_rev(svn_merge_range_t *range, svn_revnum_t rev);
typedef struct svn_location_segment_t {
svn_revnum_t range_start;
svn_revnum_t range_end;
const char *path;
} svn_location_segment_t;
typedef svn_error_t *(*svn_location_segment_receiver_t)
(svn_location_segment_t *segment,
void *baton,
apr_pool_t *pool);
svn_location_segment_t *
svn_location_segment_dup(svn_location_segment_t *segment,
apr_pool_t *pool);
#if defined(__cplusplus)
}
#endif
#endif