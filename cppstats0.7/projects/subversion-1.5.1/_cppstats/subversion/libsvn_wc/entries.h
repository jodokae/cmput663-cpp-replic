#if !defined(SVN_LIBSVN_WC_ENTRIES_H)
#define SVN_LIBSVN_WC_ENTRIES_H
#include <apr_pools.h>
#include "svn_types.h"
#if defined(__cplusplus)
extern "C" {
#endif
#define SVN_WC__ENTRIES_TOPLEVEL "wc-entries"
#define SVN_WC__ENTRIES_ENTRY "entry"
#define SVN_WC__ENTRIES_ATTR_FILE_STR "file"
#define SVN_WC__ENTRIES_ATTR_DIR_STR "dir"
#define SVN_WC__ENTRY_ATTR_NAME "name"
#define SVN_WC__ENTRY_ATTR_REVISION "revision"
#define SVN_WC__ENTRY_ATTR_URL "url"
#define SVN_WC__ENTRY_ATTR_REPOS "repos"
#define SVN_WC__ENTRY_ATTR_KIND "kind"
#define SVN_WC__ENTRY_ATTR_TEXT_TIME "text-time"
#define SVN_WC__ENTRY_ATTR_PROP_TIME "prop-time"
#define SVN_WC__ENTRY_ATTR_CHECKSUM "checksum"
#define SVN_WC__ENTRY_ATTR_SCHEDULE "schedule"
#define SVN_WC__ENTRY_ATTR_COPIED "copied"
#define SVN_WC__ENTRY_ATTR_DELETED "deleted"
#define SVN_WC__ENTRY_ATTR_ABSENT "absent"
#define SVN_WC__ENTRY_ATTR_COPYFROM_URL "copyfrom-url"
#define SVN_WC__ENTRY_ATTR_COPYFROM_REV "copyfrom-rev"
#define SVN_WC__ENTRY_ATTR_CONFLICT_OLD "conflict-old"
#define SVN_WC__ENTRY_ATTR_CONFLICT_NEW "conflict-new"
#define SVN_WC__ENTRY_ATTR_CONFLICT_WRK "conflict-wrk"
#define SVN_WC__ENTRY_ATTR_PREJFILE "prop-reject-file"
#define SVN_WC__ENTRY_ATTR_CMT_REV "committed-rev"
#define SVN_WC__ENTRY_ATTR_CMT_DATE "committed-date"
#define SVN_WC__ENTRY_ATTR_CMT_AUTHOR "last-author"
#define SVN_WC__ENTRY_ATTR_UUID "uuid"
#define SVN_WC__ENTRY_ATTR_INCOMPLETE "incomplete"
#define SVN_WC__ENTRY_ATTR_LOCK_TOKEN "lock-token"
#define SVN_WC__ENTRY_ATTR_LOCK_OWNER "lock-owner"
#define SVN_WC__ENTRY_ATTR_LOCK_COMMENT "lock-comment"
#define SVN_WC__ENTRY_ATTR_LOCK_CREATION_DATE "lock-creation-date"
#define SVN_WC__ENTRY_ATTR_HAS_PROPS "has-props"
#define SVN_WC__ENTRY_ATTR_HAS_PROP_MODS "has-prop-mods"
#define SVN_WC__ENTRY_ATTR_CACHABLE_PROPS "cachable-props"
#define SVN_WC__ENTRY_ATTR_PRESENT_PROPS "present-props"
#define SVN_WC__ENTRY_ATTR_CHANGELIST "changelist"
#define SVN_WC__ENTRY_ATTR_KEEP_LOCAL "keep-local"
#define SVN_WC__ENTRY_ATTR_WORKING_SIZE "working-size"
#define SVN_WC__ENTRY_VALUE_ADD "add"
#define SVN_WC__ENTRY_VALUE_DELETE "delete"
#define SVN_WC__ENTRY_VALUE_REPLACE "replace"
svn_error_t *svn_wc__entries_init(const char *path,
const char *uuid,
const char *url,
const char *repos,
svn_revnum_t initial_rev,
svn_depth_t depth,
apr_pool_t *pool);
svn_error_t *svn_wc__entries_write(apr_hash_t *entries,
svn_wc_adm_access_t *adm_access,
apr_pool_t *pool);
svn_error_t *svn_wc__atts_to_entry(svn_wc_entry_t **new_entry,
apr_uint64_t *modify_flags,
apr_hash_t *atts,
apr_pool_t *pool);
#define SVN_WC__ENTRY_MODIFY_REVISION APR_INT64_C(0x0000000000000001)
#define SVN_WC__ENTRY_MODIFY_URL APR_INT64_C(0x0000000000000002)
#define SVN_WC__ENTRY_MODIFY_REPOS APR_INT64_C(0x0000000000000004)
#define SVN_WC__ENTRY_MODIFY_KIND APR_INT64_C(0x0000000000000008)
#define SVN_WC__ENTRY_MODIFY_TEXT_TIME APR_INT64_C(0x0000000000000010)
#define SVN_WC__ENTRY_MODIFY_PROP_TIME APR_INT64_C(0x0000000000000020)
#define SVN_WC__ENTRY_MODIFY_CHECKSUM APR_INT64_C(0x0000000000000040)
#define SVN_WC__ENTRY_MODIFY_SCHEDULE APR_INT64_C(0x0000000000000080)
#define SVN_WC__ENTRY_MODIFY_COPIED APR_INT64_C(0x0000000000000100)
#define SVN_WC__ENTRY_MODIFY_DELETED APR_INT64_C(0x0000000000000200)
#define SVN_WC__ENTRY_MODIFY_COPYFROM_URL APR_INT64_C(0x0000000000000400)
#define SVN_WC__ENTRY_MODIFY_COPYFROM_REV APR_INT64_C(0x0000000000000800)
#define SVN_WC__ENTRY_MODIFY_CONFLICT_OLD APR_INT64_C(0x0000000000001000)
#define SVN_WC__ENTRY_MODIFY_CONFLICT_NEW APR_INT64_C(0x0000000000002000)
#define SVN_WC__ENTRY_MODIFY_CONFLICT_WRK APR_INT64_C(0x0000000000004000)
#define SVN_WC__ENTRY_MODIFY_PREJFILE APR_INT64_C(0x0000000000008000)
#define SVN_WC__ENTRY_MODIFY_CMT_REV APR_INT64_C(0x0000000000010000)
#define SVN_WC__ENTRY_MODIFY_CMT_DATE APR_INT64_C(0x0000000000020000)
#define SVN_WC__ENTRY_MODIFY_CMT_AUTHOR APR_INT64_C(0x0000000000040000)
#define SVN_WC__ENTRY_MODIFY_UUID APR_INT64_C(0x0000000000080000)
#define SVN_WC__ENTRY_MODIFY_INCOMPLETE APR_INT64_C(0x0000000000100000)
#define SVN_WC__ENTRY_MODIFY_ABSENT APR_INT64_C(0x0000000000200000)
#define SVN_WC__ENTRY_MODIFY_LOCK_TOKEN APR_INT64_C(0x0000000000400000)
#define SVN_WC__ENTRY_MODIFY_LOCK_OWNER APR_INT64_C(0x0000000000800000)
#define SVN_WC__ENTRY_MODIFY_LOCK_COMMENT APR_INT64_C(0x0000000001000000)
#define SVN_WC__ENTRY_MODIFY_LOCK_CREATION_DATE APR_INT64_C(0x0000000002000000)
#define SVN_WC__ENTRY_MODIFY_HAS_PROPS APR_INT64_C(0x0000000004000000)
#define SVN_WC__ENTRY_MODIFY_HAS_PROP_MODS APR_INT64_C(0x0000000008000000)
#define SVN_WC__ENTRY_MODIFY_CACHABLE_PROPS APR_INT64_C(0x0000000010000000)
#define SVN_WC__ENTRY_MODIFY_PRESENT_PROPS APR_INT64_C(0x0000000020000000)
#define SVN_WC__ENTRY_MODIFY_CHANGELIST APR_INT64_C(0x0000000040000000)
#define SVN_WC__ENTRY_MODIFY_KEEP_LOCAL APR_INT64_C(0x0000000080000000)
#define SVN_WC__ENTRY_MODIFY_WORKING_SIZE APR_INT64_C(0x0000000100000000)
#define SVN_WC__ENTRY_MODIFY_FORCE APR_INT64_C(0x4000000000000000)
svn_error_t *svn_wc__entry_modify(svn_wc_adm_access_t *adm_access,
const char *name,
svn_wc_entry_t *entry,
apr_uint64_t modify_flags,
svn_boolean_t do_sync,
apr_pool_t *pool);
void svn_wc__entry_remove(apr_hash_t *entries, const char *name);
svn_error_t *
svn_wc__tweak_entry(apr_hash_t *entries,
const char *name,
const char *new_url,
const char *repos,
svn_revnum_t new_rev,
svn_boolean_t allow_removal,
svn_boolean_t *write_required,
apr_pool_t *pool);
#if defined(__cplusplus)
}
#endif
#endif
