#if !defined(SVN_PROPS_H)
#define SVN_PROPS_H
#include <apr_pools.h>
#include <apr_tables.h>
#include "svn_string.h"
#if defined(__cplusplus)
extern "C" {
#endif
typedef struct svn_prop_t {
const char *name;
const svn_string_t *value;
} svn_prop_t;
svn_prop_t *svn_prop_dup(const svn_prop_t *prop, apr_pool_t *pool);
apr_array_header_t *
svn_prop_array_dup(const apr_array_header_t *array, apr_pool_t *pool);
apr_array_header_t *
svn_prop_hash_to_array(apr_hash_t *hash, apr_pool_t *pool);
typedef enum svn_prop_kind {
svn_prop_entry_kind,
svn_prop_wc_kind,
svn_prop_regular_kind
} svn_prop_kind_t;
svn_prop_kind_t svn_property_kind(int *prefix_len,
const char *prop_name);
svn_boolean_t svn_prop_is_svn_prop(const char *prop_name);
svn_boolean_t svn_prop_has_svn_prop(const apr_hash_t *props,
apr_pool_t *pool);
svn_boolean_t svn_prop_is_boolean(const char *prop_name);
svn_boolean_t svn_prop_needs_translation(const char *prop_name);
svn_error_t *svn_categorize_props(const apr_array_header_t *proplist,
apr_array_header_t **entry_props,
apr_array_header_t **wc_props,
apr_array_header_t **regular_props,
apr_pool_t *pool);
svn_error_t *svn_prop_diffs(apr_array_header_t **propdiffs,
apr_hash_t *target_props,
apr_hash_t *source_props,
apr_pool_t *pool);
svn_boolean_t svn_prop_name_is_valid(const char *prop_name);
#define SVN_PROP_PREFIX "svn:"
#define SVN_PROP_BOOLEAN_TRUE "*"
#define SVN_PROP_MIME_TYPE SVN_PROP_PREFIX "mime-type"
#define SVN_PROP_IGNORE SVN_PROP_PREFIX "ignore"
#define SVN_PROP_EOL_STYLE SVN_PROP_PREFIX "eol-style"
#define SVN_PROP_KEYWORDS SVN_PROP_PREFIX "keywords"
#define SVN_PROP_EXECUTABLE SVN_PROP_PREFIX "executable"
#define SVN_PROP_EXECUTABLE_VALUE SVN_PROP_BOOLEAN_TRUE
#define SVN_PROP_NEEDS_LOCK SVN_PROP_PREFIX "needs-lock"
#define SVN_PROP_NEEDS_LOCK_VALUE SVN_PROP_BOOLEAN_TRUE
#define SVN_PROP_SPECIAL SVN_PROP_PREFIX "special"
#define SVN_PROP_SPECIAL_VALUE SVN_PROP_BOOLEAN_TRUE
#define SVN_PROP_EXTERNALS SVN_PROP_PREFIX "externals"
#define SVN_PROP_MERGEINFO SVN_PROP_PREFIX "mergeinfo"
#define SVN_PROP_WC_PREFIX SVN_PROP_PREFIX "wc:"
#define SVN_PROP_ENTRY_PREFIX SVN_PROP_PREFIX "entry:"
#define SVN_PROP_ENTRY_COMMITTED_REV SVN_PROP_ENTRY_PREFIX "committed-rev"
#define SVN_PROP_ENTRY_COMMITTED_DATE SVN_PROP_ENTRY_PREFIX "committed-date"
#define SVN_PROP_ENTRY_LAST_AUTHOR SVN_PROP_ENTRY_PREFIX "last-author"
#define SVN_PROP_ENTRY_UUID SVN_PROP_ENTRY_PREFIX "uuid"
#define SVN_PROP_ENTRY_LOCK_TOKEN SVN_PROP_ENTRY_PREFIX "lock-token"
#define SVN_PROP_CUSTOM_PREFIX SVN_PROP_PREFIX "custom:"
#define SVN_PROP_REVISION_AUTHOR SVN_PROP_PREFIX "author"
#define SVN_PROP_REVISION_LOG SVN_PROP_PREFIX "log"
#define SVN_PROP_REVISION_DATE SVN_PROP_PREFIX "date"
#define SVN_PROP_REVISION_ORIG_DATE SVN_PROP_PREFIX "original-date"
#define SVN_PROP_REVISION_AUTOVERSIONED SVN_PROP_PREFIX "autoversioned"
#define SVNSYNC_PROP_PREFIX SVN_PROP_PREFIX "sync-"
#define SVNSYNC_PROP_LOCK SVNSYNC_PROP_PREFIX "lock"
#define SVNSYNC_PROP_FROM_URL SVNSYNC_PROP_PREFIX "from-url"
#define SVNSYNC_PROP_FROM_UUID SVNSYNC_PROP_PREFIX "from-uuid"
#define SVNSYNC_PROP_LAST_MERGED_REV SVNSYNC_PROP_PREFIX "last-merged-rev"
#define SVNSYNC_PROP_CURRENTLY_COPYING SVNSYNC_PROP_PREFIX "currently-copying"
#define SVN_PROP_REVISION_ALL_PROPS SVN_PROP_REVISION_AUTHOR, SVN_PROP_REVISION_LOG, SVN_PROP_REVISION_DATE, SVN_PROP_REVISION_AUTOVERSIONED, SVN_PROP_REVISION_ORIG_DATE, SVNSYNC_PROP_LOCK, SVNSYNC_PROP_FROM_URL, SVNSYNC_PROP_FROM_UUID, SVNSYNC_PROP_LAST_MERGED_REV, SVNSYNC_PROP_CURRENTLY_COPYING,
#if defined(__cplusplus)
}
#endif
#endif
