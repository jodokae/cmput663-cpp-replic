#include <string.h>
#include <assert.h>
#include <apr_strings.h>
#include "svn_xml.h"
#include "svn_error.h"
#include "svn_types.h"
#include "svn_time.h"
#include "svn_pools.h"
#include "svn_path.h"
#include "svn_ctype.h"
#include "wc.h"
#include "adm_files.h"
#include "adm_ops.h"
#include "entries.h"
#include "lock.h"
#include "svn_private_config.h"
#include "private/svn_wc_private.h"
static svn_wc_entry_t *
alloc_entry(apr_pool_t *pool) {
svn_wc_entry_t *entry = apr_pcalloc(pool, sizeof(*entry));
entry->revision = SVN_INVALID_REVNUM;
entry->copyfrom_rev = SVN_INVALID_REVNUM;
entry->cmt_rev = SVN_INVALID_REVNUM;
entry->kind = svn_node_none;
entry->working_size = SVN_WC_ENTRY_WORKING_SIZE_UNKNOWN;
entry->depth = svn_depth_infinity;
return entry;
}
static svn_error_t *
do_bool_attr(svn_boolean_t *entry_flag,
apr_uint64_t *modify_flags, apr_uint64_t modify_flag,
apr_hash_t *atts, const char *attr_name,
const char *entry_name) {
const char *str = apr_hash_get(atts, attr_name, APR_HASH_KEY_STRING);
*entry_flag = FALSE;
if (str) {
if (strcmp(str, "true") == 0)
*entry_flag = TRUE;
else if (strcmp(str, "false") == 0 || strcmp(str, "") == 0)
*entry_flag = FALSE;
else
return svn_error_createf
(SVN_ERR_ENTRY_ATTRIBUTE_INVALID, NULL,
_("Entry '%s' has invalid '%s' value"),
(entry_name ? entry_name : SVN_WC_ENTRY_THIS_DIR), attr_name);
*modify_flags |= modify_flag;
}
return SVN_NO_ERROR;
}
static svn_error_t *
read_escaped(char *result, char **buf, const char *end) {
apr_uint64_t val;
char digits[3];
if (end - *buf < 3 || **buf != 'x' || ! svn_ctype_isxdigit((*buf)[1])
|| ! svn_ctype_isxdigit((*buf)[2]))
return svn_error_create(SVN_ERR_WC_CORRUPT, NULL,
_("Invalid escape sequence"));
(*buf)++;
digits[0] = *((*buf)++);
digits[1] = *((*buf)++);
digits[2] = 0;
if ((val = apr_strtoi64(digits, NULL, 16)) == 0)
return svn_error_create(SVN_ERR_WC_CORRUPT, NULL,
_("Invalid escaped character"));
*result = (char) val;
return SVN_NO_ERROR;
}
static svn_error_t *
read_str(const char **result,
char **buf, const char *end,
apr_pool_t *pool) {
svn_stringbuf_t *s = NULL;
const char *start;
if (*buf == end)
return svn_error_create(SVN_ERR_WC_CORRUPT, NULL,
_("Unexpected end of entry"));
if (**buf == '\n') {
*result = NULL;
(*buf)++;
return SVN_NO_ERROR;
}
start = *buf;
while (*buf != end && **buf != '\n') {
if (**buf == '\\') {
char c;
if (! s)
s = svn_stringbuf_ncreate(start, *buf - start, pool);
else
svn_stringbuf_appendbytes(s, start, *buf - start);
(*buf)++;
SVN_ERR(read_escaped(&c, buf, end));
svn_stringbuf_appendbytes(s, &c, 1);
start = *buf;
} else
(*buf)++;
}
if (*buf == end)
return svn_error_create(SVN_ERR_WC_CORRUPT, NULL,
_("Unexpected end of entry"));
if (s) {
svn_stringbuf_appendbytes(s, start, *buf - start);
*result = s->data;
} else
*result = apr_pstrndup(pool, start, *buf - start);
(*buf)++;
return SVN_NO_ERROR;
}
static svn_error_t *
read_path(const char **result,
char **buf, const char *end,
apr_pool_t *pool) {
SVN_ERR(read_str(result, buf, end, pool));
if (*result && **result && (! svn_path_is_canonical(*result, pool)))
return svn_error_createf(SVN_ERR_WC_CORRUPT, NULL,
_("Entry contains non-canonical path '%s'"),
*result);
return SVN_NO_ERROR;
}
static svn_error_t *
read_val(const char **result,
char **buf, const char *end) {
const char *start = *buf;
if (*buf == end)
return svn_error_create(SVN_ERR_WC_CORRUPT, NULL,
_("Unexpected end of entry"));
if (**buf == '\n') {
(*buf)++;
*result = NULL;
return SVN_NO_ERROR;
}
while (*buf != end && **buf != '\n')
(*buf)++;
if (*buf == end)
return svn_error_create(SVN_ERR_WC_CORRUPT, NULL,
_("Unexpected end of entry"));
**buf = '\0';
*result = start;
(*buf)++;
return SVN_NO_ERROR;
}
static svn_error_t *
read_bool(svn_boolean_t *result, const char *field_name,
char **buf, const char *end) {
const char *val;
SVN_ERR(read_val(&val, buf, end));
if (val) {
if (strcmp(val, field_name) != 0)
return svn_error_createf(SVN_ERR_WC_CORRUPT, NULL,
_("Invalid value for field '%s'"),
field_name);
*result = TRUE;
} else
*result = FALSE;
return SVN_NO_ERROR;
}
static svn_error_t *
read_revnum(svn_revnum_t *result,
char **buf,
const char *end,
apr_pool_t *pool) {
const char *val;
SVN_ERR(read_val(&val, buf, end));
if (val)
*result = SVN_STR_TO_REV(val);
else
*result = SVN_INVALID_REVNUM;
return SVN_NO_ERROR;
}
static svn_error_t *
read_time(apr_time_t *result,
char **buf, const char *end,
apr_pool_t *pool) {
const char *val;
SVN_ERR(read_val(&val, buf, end));
if (val)
SVN_ERR(svn_time_from_cstring(result, val, pool));
else
*result = 0;
return SVN_NO_ERROR;
}
static svn_error_t *
read_entry(svn_wc_entry_t **new_entry,
char **buf, const char *end,
apr_pool_t *pool) {
svn_wc_entry_t *entry = alloc_entry(pool);
const char *name;
#define MAYBE_DONE if (**buf == '\f') goto done
SVN_ERR(read_path(&name, buf, end, pool));
entry->name = name ? name : SVN_WC_ENTRY_THIS_DIR;
{
const char *kindstr;
SVN_ERR(read_val(&kindstr, buf, end));
if (kindstr) {
if (! strcmp(kindstr, SVN_WC__ENTRIES_ATTR_FILE_STR))
entry->kind = svn_node_file;
else if (! strcmp(kindstr, SVN_WC__ENTRIES_ATTR_DIR_STR))
entry->kind = svn_node_dir;
else
return svn_error_createf
(SVN_ERR_NODE_UNKNOWN_KIND, NULL,
_("Entry '%s' has invalid node kind"),
(name ? name : SVN_WC_ENTRY_THIS_DIR));
} else
entry->kind = svn_node_none;
}
MAYBE_DONE;
SVN_ERR(read_revnum(&entry->revision, buf, end, pool));
MAYBE_DONE;
SVN_ERR(read_path(&entry->url, buf, end, pool));
MAYBE_DONE;
SVN_ERR(read_path(&entry->repos, buf, end, pool));
if (entry->repos && entry->url
&& ! svn_path_is_ancestor(entry->repos, entry->url))
return svn_error_createf(SVN_ERR_WC_CORRUPT, NULL,
_("Entry for '%s' has invalid repository "
"root"),
name ? name : SVN_WC_ENTRY_THIS_DIR);
MAYBE_DONE;
{
const char *schedulestr;
SVN_ERR(read_val(&schedulestr, buf, end));
entry->schedule = svn_wc_schedule_normal;
if (schedulestr) {
if (! strcmp(schedulestr, SVN_WC__ENTRY_VALUE_ADD))
entry->schedule = svn_wc_schedule_add;
else if (! strcmp(schedulestr, SVN_WC__ENTRY_VALUE_DELETE))
entry->schedule = svn_wc_schedule_delete;
else if (! strcmp(schedulestr, SVN_WC__ENTRY_VALUE_REPLACE))
entry->schedule = svn_wc_schedule_replace;
else
return svn_error_createf
(SVN_ERR_ENTRY_ATTRIBUTE_INVALID, NULL,
_("Entry '%s' has invalid '%s' value"),
(name ? name : SVN_WC_ENTRY_THIS_DIR),
SVN_WC__ENTRY_ATTR_SCHEDULE);
}
}
MAYBE_DONE;
SVN_ERR(read_time(&entry->text_time, buf, end, pool));
MAYBE_DONE;
SVN_ERR(read_str(&entry->checksum, buf, end, pool));
MAYBE_DONE;
SVN_ERR(read_time(&entry->cmt_date, buf, end, pool));
MAYBE_DONE;
SVN_ERR(read_revnum(&entry->cmt_rev, buf, end, pool));
MAYBE_DONE;
SVN_ERR(read_str(&entry->cmt_author, buf, end, pool));
MAYBE_DONE;
SVN_ERR(read_bool(&entry->has_props, SVN_WC__ENTRY_ATTR_HAS_PROPS,
buf, end));
MAYBE_DONE;
SVN_ERR(read_bool(&entry->has_prop_mods, SVN_WC__ENTRY_ATTR_HAS_PROP_MODS,
buf, end));
MAYBE_DONE;
SVN_ERR(read_val(&entry->cachable_props, buf, end));
if (entry->cachable_props)
entry->cachable_props = apr_pstrdup(pool, entry->cachable_props);
MAYBE_DONE;
SVN_ERR(read_val(&entry->present_props, buf, end));
if (entry->present_props)
entry->present_props = apr_pstrdup(pool, entry->present_props);
MAYBE_DONE;
{
SVN_ERR(read_path(&entry->prejfile, buf, end, pool));
MAYBE_DONE;
SVN_ERR(read_path(&entry->conflict_old, buf, end, pool));
MAYBE_DONE;
SVN_ERR(read_path(&entry->conflict_new, buf, end, pool));
MAYBE_DONE;
SVN_ERR(read_path(&entry->conflict_wrk, buf, end, pool));
MAYBE_DONE;
}
SVN_ERR(read_bool(&entry->copied, SVN_WC__ENTRY_ATTR_COPIED, buf, end));
MAYBE_DONE;
SVN_ERR(read_path(&entry->copyfrom_url, buf, end, pool));
MAYBE_DONE;
SVN_ERR(read_revnum(&entry->copyfrom_rev, buf, end, pool));
MAYBE_DONE;
SVN_ERR(read_bool(&entry->deleted, SVN_WC__ENTRY_ATTR_DELETED, buf, end));
MAYBE_DONE;
SVN_ERR(read_bool(&entry->absent, SVN_WC__ENTRY_ATTR_ABSENT, buf, end));
MAYBE_DONE;
SVN_ERR(read_bool(&entry->incomplete, SVN_WC__ENTRY_ATTR_INCOMPLETE,
buf, end));
MAYBE_DONE;
SVN_ERR(read_str(&entry->uuid, buf, end, pool));
MAYBE_DONE;
SVN_ERR(read_str(&entry->lock_token, buf, end, pool));
MAYBE_DONE;
SVN_ERR(read_str(&entry->lock_owner, buf, end, pool));
MAYBE_DONE;
SVN_ERR(read_str(&entry->lock_comment, buf, end, pool));
MAYBE_DONE;
SVN_ERR(read_time(&entry->lock_creation_date, buf, end, pool));
MAYBE_DONE;
SVN_ERR(read_str(&entry->changelist, buf, end, pool));
MAYBE_DONE;
SVN_ERR(read_bool(&entry->keep_local, SVN_WC__ENTRY_ATTR_KEEP_LOCAL,
buf, end));
MAYBE_DONE;
{
const char *val;
SVN_ERR(read_val(&val, buf, end));
if (val)
entry->working_size = (apr_off_t)apr_strtoi64(val, NULL, 0);
}
MAYBE_DONE;
{
const char *result;
SVN_ERR(read_val(&result, buf, end));
if (result)
entry->depth = svn_depth_from_word(result);
else
entry->depth = svn_depth_infinity;
}
MAYBE_DONE;
done:
*new_entry = entry;
return SVN_NO_ERROR;
}
svn_error_t *
svn_wc__atts_to_entry(svn_wc_entry_t **new_entry,
apr_uint64_t *modify_flags,
apr_hash_t *atts,
apr_pool_t *pool) {
svn_wc_entry_t *entry = alloc_entry(pool);
const char *name;
*modify_flags = 0;
name = apr_hash_get(atts, SVN_WC__ENTRY_ATTR_NAME, APR_HASH_KEY_STRING);
entry->name = name ? apr_pstrdup(pool, name) : SVN_WC_ENTRY_THIS_DIR;
{
const char *revision_str
= apr_hash_get(atts, SVN_WC__ENTRY_ATTR_REVISION, APR_HASH_KEY_STRING);
if (revision_str) {
entry->revision = SVN_STR_TO_REV(revision_str);
*modify_flags |= SVN_WC__ENTRY_MODIFY_REVISION;
} else
entry->revision = SVN_INVALID_REVNUM;
}
{
entry->url
= apr_hash_get(atts, SVN_WC__ENTRY_ATTR_URL, APR_HASH_KEY_STRING);
if (entry->url) {
*modify_flags |= SVN_WC__ENTRY_MODIFY_URL;
entry->url = apr_pstrdup(pool, entry->url);
}
}
{
entry->repos = apr_hash_get(atts, SVN_WC__ENTRY_ATTR_REPOS,
APR_HASH_KEY_STRING);
if (entry->repos) {
if (entry->url && ! svn_path_is_ancestor(entry->repos, entry->url))
return svn_error_createf(SVN_ERR_WC_CORRUPT, NULL,
_("Entry for '%s' has invalid repository "
"root"),
name ? name : SVN_WC_ENTRY_THIS_DIR);
*modify_flags |= SVN_WC__ENTRY_MODIFY_REPOS;
entry->repos = apr_pstrdup(pool, entry->repos);
}
}
{
const char *kindstr
= apr_hash_get(atts, SVN_WC__ENTRY_ATTR_KIND, APR_HASH_KEY_STRING);
entry->kind = svn_node_none;
if (kindstr) {
if (! strcmp(kindstr, SVN_WC__ENTRIES_ATTR_FILE_STR))
entry->kind = svn_node_file;
else if (! strcmp(kindstr, SVN_WC__ENTRIES_ATTR_DIR_STR))
entry->kind = svn_node_dir;
else
return svn_error_createf
(SVN_ERR_NODE_UNKNOWN_KIND, NULL,
_("Entry '%s' has invalid node kind"),
(name ? name : SVN_WC_ENTRY_THIS_DIR));
*modify_flags |= SVN_WC__ENTRY_MODIFY_KIND;
}
}
{
const char *schedulestr
= apr_hash_get(atts, SVN_WC__ENTRY_ATTR_SCHEDULE, APR_HASH_KEY_STRING);
entry->schedule = svn_wc_schedule_normal;
if (schedulestr) {
if (! strcmp(schedulestr, SVN_WC__ENTRY_VALUE_ADD))
entry->schedule = svn_wc_schedule_add;
else if (! strcmp(schedulestr, SVN_WC__ENTRY_VALUE_DELETE))
entry->schedule = svn_wc_schedule_delete;
else if (! strcmp(schedulestr, SVN_WC__ENTRY_VALUE_REPLACE))
entry->schedule = svn_wc_schedule_replace;
else if (! strcmp(schedulestr, ""))
entry->schedule = svn_wc_schedule_normal;
else
return svn_error_createf
(SVN_ERR_ENTRY_ATTRIBUTE_INVALID, NULL,
_("Entry '%s' has invalid '%s' value"),
(name ? name : SVN_WC_ENTRY_THIS_DIR),
SVN_WC__ENTRY_ATTR_SCHEDULE);
*modify_flags |= SVN_WC__ENTRY_MODIFY_SCHEDULE;
}
}
{
if ((entry->prejfile
= apr_hash_get(atts, SVN_WC__ENTRY_ATTR_PREJFILE,
APR_HASH_KEY_STRING))) {
*modify_flags |= SVN_WC__ENTRY_MODIFY_PREJFILE;
entry->prejfile = *(entry->prejfile)
? apr_pstrdup(pool, entry->prejfile) : NULL;
}
if ((entry->conflict_old
= apr_hash_get(atts, SVN_WC__ENTRY_ATTR_CONFLICT_OLD,
APR_HASH_KEY_STRING))) {
*modify_flags |= SVN_WC__ENTRY_MODIFY_CONFLICT_OLD;
entry->conflict_old =
*(entry->conflict_old)
? apr_pstrdup(pool, entry->conflict_old) : NULL;
}
if ((entry->conflict_new
= apr_hash_get(atts, SVN_WC__ENTRY_ATTR_CONFLICT_NEW,
APR_HASH_KEY_STRING))) {
*modify_flags |= SVN_WC__ENTRY_MODIFY_CONFLICT_NEW;
entry->conflict_new =
*(entry->conflict_new)
? apr_pstrdup(pool, entry->conflict_new) : NULL;
}
if ((entry->conflict_wrk
= apr_hash_get(atts, SVN_WC__ENTRY_ATTR_CONFLICT_WRK,
APR_HASH_KEY_STRING))) {
*modify_flags |= SVN_WC__ENTRY_MODIFY_CONFLICT_WRK;
entry->conflict_wrk =
*(entry->conflict_wrk)
? apr_pstrdup(pool, entry->conflict_wrk) : NULL;
}
}
SVN_ERR(do_bool_attr(&entry->copied,
modify_flags, SVN_WC__ENTRY_MODIFY_COPIED,
atts, SVN_WC__ENTRY_ATTR_COPIED, name));
{
const char *revstr;
entry->copyfrom_url = apr_hash_get(atts, SVN_WC__ENTRY_ATTR_COPYFROM_URL,
APR_HASH_KEY_STRING);
if (entry->copyfrom_url) {
*modify_flags |= SVN_WC__ENTRY_MODIFY_COPYFROM_URL;
entry->copyfrom_url = apr_pstrdup(pool, entry->copyfrom_url);
}
revstr = apr_hash_get(atts, SVN_WC__ENTRY_ATTR_COPYFROM_REV,
APR_HASH_KEY_STRING);
if (revstr) {
entry->copyfrom_rev = SVN_STR_TO_REV(revstr);
*modify_flags |= SVN_WC__ENTRY_MODIFY_COPYFROM_REV;
}
}
SVN_ERR(do_bool_attr(&entry->deleted,
modify_flags, SVN_WC__ENTRY_MODIFY_DELETED,
atts, SVN_WC__ENTRY_ATTR_DELETED, name));
SVN_ERR(do_bool_attr(&entry->absent,
modify_flags, SVN_WC__ENTRY_MODIFY_ABSENT,
atts, SVN_WC__ENTRY_ATTR_ABSENT, name));
SVN_ERR(do_bool_attr(&entry->incomplete,
modify_flags, SVN_WC__ENTRY_MODIFY_INCOMPLETE,
atts, SVN_WC__ENTRY_ATTR_INCOMPLETE, name));
SVN_ERR(do_bool_attr(&entry->keep_local,
modify_flags, SVN_WC__ENTRY_MODIFY_KEEP_LOCAL,
atts, SVN_WC__ENTRY_ATTR_KEEP_LOCAL, name));
{
const char *text_timestr, *prop_timestr;
text_timestr = apr_hash_get(atts, SVN_WC__ENTRY_ATTR_TEXT_TIME,
APR_HASH_KEY_STRING);
if (text_timestr) {
if (! strcmp(text_timestr, SVN_WC__TIMESTAMP_WC)) {
} else
SVN_ERR(svn_time_from_cstring(&entry->text_time, text_timestr,
pool));
*modify_flags |= SVN_WC__ENTRY_MODIFY_TEXT_TIME;
}
prop_timestr = apr_hash_get(atts, SVN_WC__ENTRY_ATTR_PROP_TIME,
APR_HASH_KEY_STRING);
if (prop_timestr) {
if (! strcmp(prop_timestr, SVN_WC__TIMESTAMP_WC)) {
} else
SVN_ERR(svn_time_from_cstring(&entry->prop_time, prop_timestr,
pool));
*modify_flags |= SVN_WC__ENTRY_MODIFY_PROP_TIME;
}
}
{
entry->checksum = apr_hash_get(atts, SVN_WC__ENTRY_ATTR_CHECKSUM,
APR_HASH_KEY_STRING);
if (entry->checksum) {
*modify_flags |= SVN_WC__ENTRY_MODIFY_CHECKSUM;
entry->checksum = apr_pstrdup(pool, entry->checksum);
}
}
{
entry->uuid = apr_hash_get(atts, SVN_WC__ENTRY_ATTR_UUID,
APR_HASH_KEY_STRING);
if (entry->uuid) {
*modify_flags |= SVN_WC__ENTRY_MODIFY_UUID;
entry->uuid = apr_pstrdup(pool, entry->uuid);
}
}
{
const char *cmt_datestr, *cmt_revstr;
cmt_datestr = apr_hash_get(atts, SVN_WC__ENTRY_ATTR_CMT_DATE,
APR_HASH_KEY_STRING);
if (cmt_datestr) {
SVN_ERR(svn_time_from_cstring(&entry->cmt_date, cmt_datestr, pool));
*modify_flags |= SVN_WC__ENTRY_MODIFY_CMT_DATE;
} else
entry->cmt_date = 0;
cmt_revstr = apr_hash_get(atts, SVN_WC__ENTRY_ATTR_CMT_REV,
APR_HASH_KEY_STRING);
if (cmt_revstr) {
entry->cmt_rev = SVN_STR_TO_REV(cmt_revstr);
*modify_flags |= SVN_WC__ENTRY_MODIFY_CMT_REV;
} else
entry->cmt_rev = SVN_INVALID_REVNUM;
entry->cmt_author = apr_hash_get(atts, SVN_WC__ENTRY_ATTR_CMT_AUTHOR,
APR_HASH_KEY_STRING);
if (entry->cmt_author) {
*modify_flags |= SVN_WC__ENTRY_MODIFY_CMT_AUTHOR;
entry->cmt_author = apr_pstrdup(pool, entry->cmt_author);
}
}
entry->lock_token = apr_hash_get(atts, SVN_WC__ENTRY_ATTR_LOCK_TOKEN,
APR_HASH_KEY_STRING);
if (entry->lock_token) {
*modify_flags |= SVN_WC__ENTRY_MODIFY_LOCK_TOKEN;
entry->lock_token = apr_pstrdup(pool, entry->lock_token);
}
entry->lock_owner = apr_hash_get(atts, SVN_WC__ENTRY_ATTR_LOCK_OWNER,
APR_HASH_KEY_STRING);
if (entry->lock_owner) {
*modify_flags |= SVN_WC__ENTRY_MODIFY_LOCK_OWNER;
entry->lock_owner = apr_pstrdup(pool, entry->lock_owner);
}
entry->lock_comment = apr_hash_get(atts, SVN_WC__ENTRY_ATTR_LOCK_COMMENT,
APR_HASH_KEY_STRING);
if (entry->lock_comment) {
*modify_flags |= SVN_WC__ENTRY_MODIFY_LOCK_COMMENT;
entry->lock_comment = apr_pstrdup(pool, entry->lock_comment);
}
{
const char *cdate_str =
apr_hash_get(atts, SVN_WC__ENTRY_ATTR_LOCK_CREATION_DATE,
APR_HASH_KEY_STRING);
if (cdate_str) {
SVN_ERR(svn_time_from_cstring(&entry->lock_creation_date,
cdate_str, pool));
*modify_flags |= SVN_WC__ENTRY_MODIFY_LOCK_CREATION_DATE;
}
}
entry->changelist = apr_hash_get(atts, SVN_WC__ENTRY_ATTR_CHANGELIST,
APR_HASH_KEY_STRING);
if (entry->changelist) {
*modify_flags |= SVN_WC__ENTRY_MODIFY_CHANGELIST;
entry->changelist = apr_pstrdup(pool, entry->changelist);
}
SVN_ERR(do_bool_attr(&entry->has_props,
modify_flags, SVN_WC__ENTRY_MODIFY_HAS_PROPS,
atts, SVN_WC__ENTRY_ATTR_HAS_PROPS, name));
{
const char *has_prop_mods_str
= apr_hash_get(atts, SVN_WC__ENTRY_ATTR_HAS_PROP_MODS,
APR_HASH_KEY_STRING);
if (has_prop_mods_str) {
if (strcmp(has_prop_mods_str, "true") == 0)
entry->has_prop_mods = TRUE;
else if (strcmp(has_prop_mods_str, "false") != 0)
return svn_error_createf
(SVN_ERR_ENTRY_ATTRIBUTE_INVALID, NULL,
_("Entry '%s' has invalid '%s' value"),
(name ? name : SVN_WC_ENTRY_THIS_DIR),
SVN_WC__ENTRY_ATTR_HAS_PROP_MODS);
*modify_flags |= SVN_WC__ENTRY_MODIFY_HAS_PROP_MODS;
}
}
entry->cachable_props = apr_hash_get(atts,
SVN_WC__ENTRY_ATTR_CACHABLE_PROPS,
APR_HASH_KEY_STRING);
if (entry->cachable_props) {
*modify_flags |= SVN_WC__ENTRY_MODIFY_CACHABLE_PROPS;
entry->cachable_props = apr_pstrdup(pool, entry->cachable_props);
}
entry->present_props = apr_hash_get(atts,
SVN_WC__ENTRY_ATTR_PRESENT_PROPS,
APR_HASH_KEY_STRING);
if (entry->present_props) {
*modify_flags |= SVN_WC__ENTRY_MODIFY_PRESENT_PROPS;
entry->present_props = apr_pstrdup(pool, entry->present_props);
}
{
const char *val
= apr_hash_get(atts,
SVN_WC__ENTRY_ATTR_WORKING_SIZE,
APR_HASH_KEY_STRING);
if (val) {
if (! strcmp(val, SVN_WC__WORKING_SIZE_WC)) {
} else
entry->working_size = (apr_off_t)apr_strtoi64(val, NULL, 0);
*modify_flags |= SVN_WC__ENTRY_MODIFY_WORKING_SIZE;
}
}
*new_entry = entry;
return SVN_NO_ERROR;
}
struct entries_accumulator {
apr_hash_t *entries;
svn_xml_parser_t *parser;
svn_boolean_t show_hidden;
apr_pool_t *pool;
apr_pool_t *scratch_pool;
};
static void
handle_start_tag(void *userData, const char *tagname, const char **atts) {
struct entries_accumulator *accum = userData;
apr_hash_t *attributes;
svn_wc_entry_t *entry;
svn_error_t *err;
apr_uint64_t modify_flags = 0;
if (strcmp(tagname, SVN_WC__ENTRIES_ENTRY))
return;
svn_pool_clear(accum->scratch_pool);
attributes = svn_xml_make_att_hash(atts, accum->scratch_pool);
err = svn_wc__atts_to_entry(&entry, &modify_flags, attributes, accum->pool);
if (err) {
svn_xml_signal_bailout(err, accum->parser);
return;
}
if ((entry->deleted || entry->absent)
&& (entry->schedule != svn_wc_schedule_add)
&& (entry->schedule != svn_wc_schedule_replace)
&& (! accum->show_hidden))
;
else
apr_hash_set(accum->entries, entry->name, APR_HASH_KEY_STRING, entry);
}
static svn_error_t *
parse_entries_xml(svn_wc_adm_access_t *adm_access,
apr_hash_t *entries,
svn_boolean_t show_hidden,
const char *buf,
apr_size_t size,
apr_pool_t *pool) {
svn_xml_parser_t *svn_parser;
struct entries_accumulator accum;
accum.entries = entries;
accum.show_hidden = show_hidden;
accum.pool = svn_wc_adm_access_pool(adm_access);
accum.scratch_pool = svn_pool_create(pool);
svn_parser = svn_xml_make_parser(&accum,
handle_start_tag,
NULL,
NULL,
pool);
accum.parser = svn_parser;
SVN_ERR_W(svn_xml_parse(svn_parser, buf, size, TRUE),
apr_psprintf(pool,
_("XML parser failed in '%s'"),
svn_path_local_style
(svn_wc_adm_access_path(adm_access), pool)));
svn_pool_destroy(accum.scratch_pool);
svn_xml_free_parser(svn_parser);
return SVN_NO_ERROR;
}
static void
take_from_entry(svn_wc_entry_t *src, svn_wc_entry_t *dst, apr_pool_t *pool) {
if ((dst->revision == SVN_INVALID_REVNUM) && (dst->kind != svn_node_dir))
dst->revision = src->revision;
if (! dst->url)
dst->url = svn_path_url_add_component(src->url, dst->name, pool);
if (! dst->repos)
dst->repos = src->repos;
if ((! dst->uuid)
&& (! ((dst->schedule == svn_wc_schedule_add)
|| (dst->schedule == svn_wc_schedule_replace)))) {
dst->uuid = src->uuid;
}
if (! dst->cachable_props)
dst->cachable_props = src->cachable_props;
}
static svn_error_t *
resolve_to_defaults(apr_hash_t *entries,
apr_pool_t *pool) {
apr_hash_index_t *hi;
svn_wc_entry_t *default_entry
= apr_hash_get(entries, SVN_WC_ENTRY_THIS_DIR, APR_HASH_KEY_STRING);
if (! default_entry)
return svn_error_create(SVN_ERR_ENTRY_NOT_FOUND,
NULL,
_("Missing default entry"));
if (default_entry->revision == SVN_INVALID_REVNUM)
return svn_error_create(SVN_ERR_ENTRY_MISSING_REVISION,
NULL,
_("Default entry has no revision number"));
if (! default_entry->url)
return svn_error_create(SVN_ERR_ENTRY_MISSING_URL,
NULL,
_("Default entry is missing URL"));
for (hi = apr_hash_first(pool, entries); hi; hi = apr_hash_next(hi)) {
void *val;
svn_wc_entry_t *this_entry;
apr_hash_this(hi, NULL, NULL, &val);
this_entry = val;
if (this_entry == default_entry)
continue;
if (this_entry->kind == svn_node_dir)
continue;
if (this_entry->kind == svn_node_file)
take_from_entry(default_entry, this_entry, pool);
}
return SVN_NO_ERROR;
}
static svn_error_t *
read_entries(svn_wc_adm_access_t *adm_access,
svn_boolean_t show_hidden,
apr_pool_t *pool) {
const char *path = svn_wc_adm_access_path(adm_access);
apr_file_t *infile = NULL;
svn_stringbuf_t *buf = svn_stringbuf_create("", pool);
apr_hash_t *entries = apr_hash_make(svn_wc_adm_access_pool(adm_access));
char *curp, *endp;
svn_wc_entry_t *entry;
int entryno;
SVN_ERR(svn_wc__open_adm_file(&infile, path,
SVN_WC__ADM_ENTRIES, APR_READ, pool));
SVN_ERR(svn_stringbuf_from_aprfile(&buf, infile, pool));
curp = buf->data;
endp = buf->data + buf->len;
if (curp != endp && !svn_ctype_isdigit(*curp))
SVN_ERR(parse_entries_xml(adm_access, entries, show_hidden,
buf->data, buf->len, pool));
else {
curp = memchr(curp, '\n', buf->len);
if (! curp)
return svn_error_createf(SVN_ERR_WC_CORRUPT, NULL,
_("Invalid version line in entries file "
"of '%s'"),
svn_path_local_style(path, pool));
++curp;
entryno = 1;
while (curp != endp) {
svn_error_t *err = read_entry(&entry, &curp, endp,
svn_wc_adm_access_pool(adm_access));
if (! err) {
curp = memchr(curp, '\f', endp - curp);
if (! curp)
err = svn_error_create(SVN_ERR_WC_CORRUPT, NULL,
_("Missing entry terminator"));
if (! err && (curp == endp || *(++curp) != '\n'))
err = svn_error_create(SVN_ERR_WC_CORRUPT, NULL,
_("Invalid entry terminator"));
}
if (err)
return svn_error_createf(err->apr_err, err,
_("Error at entry %d in entries file for "
"'%s':"),
entryno, svn_path_local_style(path, pool));
++curp;
++entryno;
if ((entry->deleted || entry->absent)
&& (entry->schedule != svn_wc_schedule_add)
&& (entry->schedule != svn_wc_schedule_replace)
&& (! show_hidden))
;
else
apr_hash_set(entries, entry->name, APR_HASH_KEY_STRING, entry);
}
}
SVN_ERR(svn_wc__close_adm_file(infile, svn_wc_adm_access_path(adm_access),
SVN_WC__ADM_ENTRIES, 0, pool));
SVN_ERR(resolve_to_defaults(entries, svn_wc_adm_access_pool(adm_access)));
svn_wc__adm_access_set_entries(adm_access, show_hidden, entries);
return SVN_NO_ERROR;
}
svn_error_t *
svn_wc_entry(const svn_wc_entry_t **entry,
const char *path,
svn_wc_adm_access_t *adm_access,
svn_boolean_t show_hidden,
apr_pool_t *pool) {
const char *entry_name;
svn_wc_adm_access_t *dir_access;
SVN_ERR(svn_wc__adm_retrieve_internal(&dir_access, adm_access, path, pool));
if (! dir_access) {
const char *dir_path, *base_name;
svn_path_split(path, &dir_path, &base_name, pool);
SVN_ERR(svn_wc__adm_retrieve_internal(&dir_access, adm_access, dir_path,
pool));
entry_name = base_name;
} else
entry_name = SVN_WC_ENTRY_THIS_DIR;
if (dir_access) {
apr_hash_t *entries;
SVN_ERR(svn_wc_entries_read(&entries, dir_access, show_hidden, pool));
*entry = apr_hash_get(entries, entry_name, APR_HASH_KEY_STRING);
} else
*entry = NULL;
return SVN_NO_ERROR;
}
svn_error_t *
svn_wc__entry_versioned_internal(const svn_wc_entry_t **entry,
const char *path,
svn_wc_adm_access_t *adm_access,
svn_boolean_t show_hidden,
const char *caller_filename,
int caller_lineno,
apr_pool_t *pool) {
SVN_ERR(svn_wc_entry(entry, path, adm_access, show_hidden, pool));
if (! *entry) {
svn_error_t *err
= svn_error_createf(SVN_ERR_ENTRY_NOT_FOUND, NULL,
_("'%s' is not under version control"),
svn_path_local_style(path, pool));
err->file = caller_filename;
err->line = caller_lineno;
return err;
}
return SVN_NO_ERROR;
}
#if 0
static svn_error_t *
check_entries(apr_hash_t *entries,
const char *path,
apr_pool_t *pool) {
svn_wc_entry_t *default_entry;
apr_hash_index_t *hi;
default_entry = apr_hash_get(entries,
SVN_WC_ENTRY_THIS_DIR,
APR_HASH_KEY_STRING);
if (! default_entry)
return svn_error_createf
(SVN_ERR_WC_CORRUPT, NULL,
_("Corrupt working copy: '%s' has no default entry"),
svn_path_local_style(path, pool));
switch (default_entry->schedule) {
case svn_wc_schedule_normal:
case svn_wc_schedule_add:
case svn_wc_schedule_delete:
case svn_wc_schedule_replace:
break;
default:
return svn_error_createf
(SVN_ERR_WC_CORRUPT, NULL,
_("Corrupt working copy: directory '%s' has an invalid schedule"),
svn_path_local_style(path, pool));
}
for (hi = apr_hash_first(pool, entries); hi; hi = apr_hash_next(hi)) {
const void *key;
const char *name;
void *val;
svn_wc_entry_t *this_entry;
apr_hash_this(hi, &key, NULL, &val);
this_entry = val;
name = key;
if (! strcmp(name, SVN_WC_ENTRY_THIS_DIR ))
continue;
switch (this_entry->schedule) {
case svn_wc_schedule_normal:
case svn_wc_schedule_add:
case svn_wc_schedule_delete:
case svn_wc_schedule_replace:
break;
default:
return svn_error_createf
(SVN_ERR_WC_CORRUPT, NULL,
_("Corrupt working copy: "
"'%s' in directory '%s' has an invalid schedule"),
name, svn_path_local_style(path, pool));
}
if ((default_entry->schedule == svn_wc_schedule_add)
&& (this_entry->schedule != svn_wc_schedule_add))
return svn_error_createf
(SVN_ERR_WC_CORRUPT, NULL,
_("Corrupt working copy: '%s' in directory '%s' (which is "
"scheduled for addition) is not itself scheduled for addition"),
name, svn_path_local_style(path, pool));
if ((default_entry->schedule == svn_wc_schedule_delete)
&& (this_entry->schedule != svn_wc_schedule_delete))
return svn_error_createf
(SVN_ERR_WC_CORRUPT, NULL,
_("Corrupt working copy: '%s' in directory '%s' (which is "
"scheduled for deletion) is not itself scheduled for deletion"),
name, svn_path_local_style(path, pool));
if ((default_entry->schedule == svn_wc_schedule_replace)
&& (this_entry->schedule == svn_wc_schedule_normal))
return svn_error_createf
(SVN_ERR_WC_CORRUPT, NULL,
_("Corrupt working copy: '%s' in directory '%s' (which is "
"scheduled for replacement) has an invalid schedule"),
name, svn_path_local_style(path, pool));
}
return SVN_NO_ERROR;
}
#endif
svn_error_t *
svn_wc_entries_read(apr_hash_t **entries,
svn_wc_adm_access_t *adm_access,
svn_boolean_t show_hidden,
apr_pool_t *pool) {
apr_hash_t *new_entries;
new_entries = svn_wc__adm_access_entries(adm_access, show_hidden, pool);
if (! new_entries) {
SVN_ERR(read_entries(adm_access, TRUE, pool));
new_entries = svn_wc__adm_access_entries(adm_access, show_hidden, pool);
}
*entries = new_entries;
return SVN_NO_ERROR;
}
static void
write_str(svn_stringbuf_t *buf, const char *str, apr_pool_t *pool) {
const char *start = str;
if (str) {
while (*str) {
if (svn_ctype_iscntrl(*str) || *str == '\\') {
svn_stringbuf_appendbytes(buf, start, str - start);
svn_stringbuf_appendcstr(buf,
apr_psprintf(pool, "\\x%02x", *str));
start = str + 1;
}
++str;
}
svn_stringbuf_appendbytes(buf, start, str - start);
}
svn_stringbuf_appendbytes(buf, "\n", 1);
}
static void
write_val(svn_stringbuf_t *buf, const char *val, apr_size_t len) {
if (val)
svn_stringbuf_appendbytes(buf, val, len);
svn_stringbuf_appendbytes(buf, "\n", 1);
}
static void
write_bool(svn_stringbuf_t *buf, const char *field_name, svn_boolean_t val) {
write_val(buf, val ? field_name : NULL, val ? strlen(field_name) : 0);
}
static void
write_revnum(svn_stringbuf_t *buf, svn_revnum_t revnum, apr_pool_t *pool) {
if (SVN_IS_VALID_REVNUM(revnum))
svn_stringbuf_appendcstr(buf, apr_ltoa(pool, revnum));
svn_stringbuf_appendbytes(buf, "\n", 1);
}
static void
write_time(svn_stringbuf_t *buf, apr_time_t val, apr_pool_t *pool) {
if (val)
svn_stringbuf_appendcstr(buf, svn_time_to_cstring(val, pool));
svn_stringbuf_appendbytes(buf, "\n", 1);
}
static void
write_entry(svn_stringbuf_t *buf,
svn_wc_entry_t *entry,
const char *name,
svn_wc_entry_t *this_dir,
apr_pool_t *pool) {
const char *valuestr;
svn_revnum_t valuerev;
svn_boolean_t is_this_dir = strcmp(name, SVN_WC_ENTRY_THIS_DIR) == 0;
svn_boolean_t is_subdir = ! is_this_dir && (entry->kind == svn_node_dir);
assert(name);
write_str(buf, name, pool);
switch (entry->kind) {
case svn_node_dir:
write_val(buf, SVN_WC__ENTRIES_ATTR_DIR_STR,
sizeof(SVN_WC__ENTRIES_ATTR_DIR_STR) - 1);
break;
case svn_node_none:
write_val(buf, NULL, 0);
break;
case svn_node_file:
case svn_node_unknown:
default:
write_val(buf, SVN_WC__ENTRIES_ATTR_FILE_STR,
sizeof(SVN_WC__ENTRIES_ATTR_FILE_STR) - 1);
break;
}
if (is_this_dir || (! is_subdir && entry->revision != this_dir->revision))
valuerev = entry->revision;
else
valuerev = SVN_INVALID_REVNUM;
write_revnum(buf, valuerev, pool);
if (is_this_dir ||
(! is_subdir && strcmp(svn_path_url_add_component(this_dir->url, name,
pool),
entry->url) != 0))
valuestr = entry->url;
else
valuestr = NULL;
write_str(buf, valuestr, pool);
if (! is_subdir
&& (is_this_dir
|| (this_dir->repos == NULL
|| (entry->repos
&& strcmp(this_dir->repos, entry->repos) != 0))))
valuestr = entry->repos;
else
valuestr = NULL;
write_str(buf, valuestr, pool);
switch (entry->schedule) {
case svn_wc_schedule_add:
write_val(buf, SVN_WC__ENTRY_VALUE_ADD,
sizeof(SVN_WC__ENTRY_VALUE_ADD) - 1);
break;
case svn_wc_schedule_delete:
write_val(buf, SVN_WC__ENTRY_VALUE_DELETE,
sizeof(SVN_WC__ENTRY_VALUE_DELETE) - 1);
break;
case svn_wc_schedule_replace:
write_val(buf, SVN_WC__ENTRY_VALUE_REPLACE,
sizeof(SVN_WC__ENTRY_VALUE_REPLACE) - 1);
break;
case svn_wc_schedule_normal:
default:
write_val(buf, NULL, 0);
break;
}
write_time(buf, entry->text_time, pool);
write_val(buf, entry->checksum,
entry->checksum ? strlen(entry->checksum) : 0);
write_time(buf, entry->cmt_date, pool);
write_revnum(buf, entry->cmt_rev, pool);
write_str(buf, entry->cmt_author, pool);
write_bool(buf, SVN_WC__ENTRY_ATTR_HAS_PROPS, entry->has_props);
write_bool(buf, SVN_WC__ENTRY_ATTR_HAS_PROP_MODS, entry->has_prop_mods);
if (is_this_dir
|| ! this_dir->cachable_props || ! entry->cachable_props
|| strcmp(this_dir->cachable_props, entry->cachable_props) != 0)
valuestr = entry->cachable_props;
else
valuestr = NULL;
write_val(buf, valuestr, valuestr ? strlen(valuestr) : 0);
write_val(buf, entry->present_props,
entry->present_props ? strlen(entry->present_props) : 0);
write_str(buf, entry->prejfile, pool);
write_str(buf, entry->conflict_old, pool);
write_str(buf, entry->conflict_new, pool);
write_str(buf, entry->conflict_wrk, pool);
write_bool(buf, SVN_WC__ENTRY_ATTR_COPIED, entry->copied);
write_str(buf, entry->copyfrom_url, pool);
write_revnum(buf, entry->copyfrom_rev, pool);
write_bool(buf, SVN_WC__ENTRY_ATTR_DELETED, entry->deleted);
write_bool(buf, SVN_WC__ENTRY_ATTR_ABSENT, entry->absent);
write_bool(buf, SVN_WC__ENTRY_ATTR_INCOMPLETE, entry->incomplete);
if (is_this_dir || ! this_dir->uuid || ! entry->uuid
|| strcmp(this_dir->uuid, entry->uuid) != 0)
valuestr = entry->uuid;
else
valuestr = NULL;
write_val(buf, valuestr, valuestr ? strlen(valuestr) : 0);
write_str(buf, entry->lock_token, pool);
write_str(buf, entry->lock_owner, pool);
write_str(buf, entry->lock_comment, pool);
write_time(buf, entry->lock_creation_date, pool);
write_str(buf, entry->changelist, pool);
write_bool(buf, SVN_WC__ENTRY_ATTR_KEEP_LOCAL, entry->keep_local);
{
const char *val
= (entry->working_size != SVN_WC_ENTRY_WORKING_SIZE_UNKNOWN)
? apr_off_t_toa(pool, entry->working_size) : "";
write_val(buf, val, strlen(val));
}
if (is_subdir || entry->depth == svn_depth_infinity) {
write_val(buf, NULL, 0);
} else {
const char *val = svn_depth_to_word(entry->depth);
write_val(buf, val, strlen(val));
}
while (buf->len > 1 && buf->data[buf->len - 2] == '\n')
buf->len--;
svn_stringbuf_appendbytes(buf, "\f\n", 2);
}
static void
write_entry_xml(svn_stringbuf_t **output,
svn_wc_entry_t *entry,
const char *name,
svn_wc_entry_t *this_dir,
apr_pool_t *pool) {
apr_hash_t *atts = apr_hash_make(pool);
const char *valuestr;
assert(name);
apr_hash_set(atts, SVN_WC__ENTRY_ATTR_NAME, APR_HASH_KEY_STRING,
entry->name);
if (SVN_IS_VALID_REVNUM(entry->revision))
apr_hash_set(atts, SVN_WC__ENTRY_ATTR_REVISION, APR_HASH_KEY_STRING,
apr_psprintf(pool, "%ld", entry->revision));
if (entry->url)
apr_hash_set(atts, SVN_WC__ENTRY_ATTR_URL, APR_HASH_KEY_STRING,
entry->url);
if (entry->repos)
apr_hash_set(atts, SVN_WC__ENTRY_ATTR_REPOS, APR_HASH_KEY_STRING,
entry->repos);
switch (entry->kind) {
case svn_node_dir:
valuestr = SVN_WC__ENTRIES_ATTR_DIR_STR;
break;
case svn_node_none:
valuestr = NULL;
break;
case svn_node_file:
case svn_node_unknown:
default:
valuestr = SVN_WC__ENTRIES_ATTR_FILE_STR;
break;
}
apr_hash_set(atts, SVN_WC__ENTRY_ATTR_KIND, APR_HASH_KEY_STRING, valuestr);
switch (entry->schedule) {
case svn_wc_schedule_add:
valuestr = SVN_WC__ENTRY_VALUE_ADD;
break;
case svn_wc_schedule_delete:
valuestr = SVN_WC__ENTRY_VALUE_DELETE;
break;
case svn_wc_schedule_replace:
valuestr = SVN_WC__ENTRY_VALUE_REPLACE;
break;
case svn_wc_schedule_normal:
default:
valuestr = NULL;
break;
}
apr_hash_set(atts, SVN_WC__ENTRY_ATTR_SCHEDULE, APR_HASH_KEY_STRING,
valuestr);
if (entry->conflict_old)
apr_hash_set(atts, SVN_WC__ENTRY_ATTR_CONFLICT_OLD, APR_HASH_KEY_STRING,
entry->conflict_old);
if (entry->conflict_new)
apr_hash_set(atts, SVN_WC__ENTRY_ATTR_CONFLICT_NEW, APR_HASH_KEY_STRING,
entry->conflict_new);
if (entry->conflict_wrk)
apr_hash_set(atts, SVN_WC__ENTRY_ATTR_CONFLICT_WRK, APR_HASH_KEY_STRING,
entry->conflict_wrk);
if (entry->prejfile)
apr_hash_set(atts, SVN_WC__ENTRY_ATTR_PREJFILE, APR_HASH_KEY_STRING,
entry->prejfile);
apr_hash_set(atts, SVN_WC__ENTRY_ATTR_COPIED, APR_HASH_KEY_STRING,
(entry->copied ? "true" : NULL));
if (SVN_IS_VALID_REVNUM(entry->copyfrom_rev))
apr_hash_set(atts, SVN_WC__ENTRY_ATTR_COPYFROM_REV, APR_HASH_KEY_STRING,
apr_psprintf(pool, "%ld",
entry->copyfrom_rev));
if (entry->copyfrom_url)
apr_hash_set(atts, SVN_WC__ENTRY_ATTR_COPYFROM_URL, APR_HASH_KEY_STRING,
entry->copyfrom_url);
apr_hash_set(atts, SVN_WC__ENTRY_ATTR_DELETED, APR_HASH_KEY_STRING,
(entry->deleted ? "true" : NULL));
apr_hash_set(atts, SVN_WC__ENTRY_ATTR_ABSENT, APR_HASH_KEY_STRING,
(entry->absent ? "true" : NULL));
apr_hash_set(atts, SVN_WC__ENTRY_ATTR_INCOMPLETE, APR_HASH_KEY_STRING,
(entry->incomplete ? "true" : NULL));
if (entry->text_time) {
apr_hash_set(atts, SVN_WC__ENTRY_ATTR_TEXT_TIME, APR_HASH_KEY_STRING,
svn_time_to_cstring(entry->text_time, pool));
}
if (entry->prop_time) {
apr_hash_set(atts, SVN_WC__ENTRY_ATTR_PROP_TIME, APR_HASH_KEY_STRING,
svn_time_to_cstring(entry->prop_time, pool));
}
if (entry->checksum)
apr_hash_set(atts, SVN_WC__ENTRY_ATTR_CHECKSUM, APR_HASH_KEY_STRING,
entry->checksum);
if (SVN_IS_VALID_REVNUM(entry->cmt_rev))
apr_hash_set(atts, SVN_WC__ENTRY_ATTR_CMT_REV, APR_HASH_KEY_STRING,
apr_psprintf(pool, "%ld", entry->cmt_rev));
if (entry->cmt_author)
apr_hash_set(atts, SVN_WC__ENTRY_ATTR_CMT_AUTHOR, APR_HASH_KEY_STRING,
entry->cmt_author);
if (entry->uuid)
apr_hash_set(atts, SVN_WC__ENTRY_ATTR_UUID, APR_HASH_KEY_STRING,
entry->uuid);
if (entry->cmt_date) {
apr_hash_set(atts, SVN_WC__ENTRY_ATTR_CMT_DATE, APR_HASH_KEY_STRING,
svn_time_to_cstring(entry->cmt_date, pool));
}
if (entry->lock_token)
apr_hash_set(atts, SVN_WC__ENTRY_ATTR_LOCK_TOKEN, APR_HASH_KEY_STRING,
entry->lock_token);
if (entry->lock_owner)
apr_hash_set(atts, SVN_WC__ENTRY_ATTR_LOCK_OWNER, APR_HASH_KEY_STRING,
entry->lock_owner);
if (entry->lock_comment)
apr_hash_set(atts, SVN_WC__ENTRY_ATTR_LOCK_COMMENT, APR_HASH_KEY_STRING,
entry->lock_comment);
if (entry->lock_creation_date)
apr_hash_set(atts, SVN_WC__ENTRY_ATTR_LOCK_CREATION_DATE,
APR_HASH_KEY_STRING,
svn_time_to_cstring(entry->lock_creation_date, pool));
apr_hash_set(atts, SVN_WC__ENTRY_ATTR_HAS_PROPS, APR_HASH_KEY_STRING,
(entry->has_props ? "true" : NULL));
if (entry->has_prop_mods)
apr_hash_set(atts, SVN_WC__ENTRY_ATTR_HAS_PROP_MODS,
APR_HASH_KEY_STRING, "true");
if (entry->cachable_props && *entry->cachable_props)
apr_hash_set(atts, SVN_WC__ENTRY_ATTR_CACHABLE_PROPS,
APR_HASH_KEY_STRING, entry->cachable_props);
if (entry->present_props
&& *entry->present_props)
apr_hash_set(atts, SVN_WC__ENTRY_ATTR_PRESENT_PROPS,
APR_HASH_KEY_STRING, entry->present_props);
if (strcmp(name, SVN_WC_ENTRY_THIS_DIR)) {
if (! strcmp(name, ".")) {
abort();
}
if (entry->kind == svn_node_dir) {
apr_hash_set(atts, SVN_WC__ENTRY_ATTR_REVISION, APR_HASH_KEY_STRING,
NULL);
apr_hash_set(atts, SVN_WC__ENTRY_ATTR_URL, APR_HASH_KEY_STRING,
NULL);
apr_hash_set(atts, SVN_WC__ENTRY_ATTR_REPOS, APR_HASH_KEY_STRING,
NULL);
apr_hash_set(atts, SVN_WC__ENTRY_ATTR_UUID, APR_HASH_KEY_STRING,
NULL);
} else {
if (entry->revision == this_dir->revision)
apr_hash_set(atts, SVN_WC__ENTRY_ATTR_REVISION,
APR_HASH_KEY_STRING, NULL);
if (entry->uuid && this_dir->uuid) {
if (strcmp(entry->uuid, this_dir->uuid) == 0)
apr_hash_set(atts, SVN_WC__ENTRY_ATTR_UUID,
APR_HASH_KEY_STRING, NULL);
}
if (entry->url) {
if (strcmp(entry->url,
svn_path_url_add_component(this_dir->url,
name, pool)) == 0)
apr_hash_set(atts, SVN_WC__ENTRY_ATTR_URL,
APR_HASH_KEY_STRING, NULL);
}
if (entry->repos && this_dir->repos
&& strcmp(entry->repos, this_dir->repos) == 0)
apr_hash_set(atts, SVN_WC__ENTRY_ATTR_REPOS, APR_HASH_KEY_STRING,
NULL);
if (entry->cachable_props && this_dir->cachable_props
&& strcmp(entry->cachable_props, this_dir->cachable_props) == 0)
apr_hash_set(atts, SVN_WC__ENTRY_ATTR_CACHABLE_PROPS,
APR_HASH_KEY_STRING, NULL);
}
}
svn_xml_make_open_tag_hash(output,
pool,
svn_xml_self_closing,
SVN_WC__ENTRIES_ENTRY,
atts);
}
static void
write_entries_xml(svn_stringbuf_t **output,
apr_hash_t *entries,
svn_wc_entry_t *this_dir,
apr_pool_t *pool) {
apr_hash_index_t *hi;
apr_pool_t *subpool = svn_pool_create(pool);
svn_xml_make_header(output, pool);
svn_xml_make_open_tag(output, pool, svn_xml_normal,
SVN_WC__ENTRIES_TOPLEVEL,
"xmlns",
SVN_XML_NAMESPACE,
NULL);
write_entry_xml(output, this_dir, SVN_WC_ENTRY_THIS_DIR, this_dir, pool);
for (hi = apr_hash_first(pool, entries); hi; hi = apr_hash_next(hi)) {
const void *key;
void *val;
svn_wc_entry_t *this_entry;
svn_pool_clear(subpool);
apr_hash_this(hi, &key, NULL, &val);
this_entry = val;
if (! strcmp(key, SVN_WC_ENTRY_THIS_DIR ))
continue;
write_entry_xml(output, this_entry, key, this_dir, subpool);
}
svn_xml_make_close_tag(output, pool, SVN_WC__ENTRIES_TOPLEVEL);
svn_pool_destroy(subpool);
}
svn_error_t *
svn_wc__entries_write(apr_hash_t *entries,
svn_wc_adm_access_t *adm_access,
apr_pool_t *pool) {
svn_error_t *err = SVN_NO_ERROR;
svn_stringbuf_t *bigstr = NULL;
apr_file_t *outfile = NULL;
apr_hash_index_t *hi;
svn_wc_entry_t *this_dir;
SVN_ERR(svn_wc__adm_write_check(adm_access));
this_dir = apr_hash_get(entries, SVN_WC_ENTRY_THIS_DIR,
APR_HASH_KEY_STRING);
if (! this_dir)
return svn_error_createf(SVN_ERR_ENTRY_NOT_FOUND, NULL,
_("No default entry in directory '%s'"),
svn_path_local_style
(svn_wc_adm_access_path(adm_access), pool));
SVN_ERR(svn_wc__open_adm_file(&outfile,
svn_wc_adm_access_path(adm_access),
SVN_WC__ADM_ENTRIES,
(APR_WRITE | APR_CREATE),
pool));
if (svn_wc__adm_wc_format(adm_access) > SVN_WC__XML_ENTRIES_VERSION) {
apr_pool_t *subpool = svn_pool_create(pool);
bigstr = svn_stringbuf_createf(pool, "%d\n",
svn_wc__adm_wc_format(adm_access));
write_entry(bigstr, this_dir, SVN_WC_ENTRY_THIS_DIR, this_dir, pool);
for (hi = apr_hash_first(pool, entries); hi; hi = apr_hash_next(hi)) {
const void *key;
void *val;
svn_wc_entry_t *this_entry;
svn_pool_clear(subpool);
apr_hash_this(hi, &key, NULL, &val);
this_entry = val;
if (! strcmp(key, SVN_WC_ENTRY_THIS_DIR ))
continue;
write_entry(bigstr, this_entry, key, this_dir, subpool);
}
svn_pool_destroy(subpool);
} else
write_entries_xml(&bigstr, entries, this_dir, pool);
SVN_ERR_W(svn_io_file_write_full(outfile, bigstr->data,
bigstr->len, NULL, pool),
apr_psprintf(pool,
_("Error writing to '%s'"),
svn_path_local_style
(svn_wc_adm_access_path(adm_access), pool)));
err = svn_wc__close_adm_file(outfile,
svn_wc_adm_access_path(adm_access),
SVN_WC__ADM_ENTRIES, 1, pool);
svn_wc__adm_access_set_entries(adm_access, TRUE, entries);
svn_wc__adm_access_set_entries(adm_access, FALSE, NULL);
return err;
}
static void
fold_entry(apr_hash_t *entries,
const char *name,
apr_uint64_t modify_flags,
svn_wc_entry_t *entry,
apr_pool_t *pool) {
svn_wc_entry_t *cur_entry
= apr_hash_get(entries, name, APR_HASH_KEY_STRING);
assert(name != NULL);
if (! cur_entry)
cur_entry = alloc_entry(pool);
if (! cur_entry->name)
cur_entry->name = apr_pstrdup(pool, name);
if (modify_flags & SVN_WC__ENTRY_MODIFY_REVISION)
cur_entry->revision = entry->revision;
if (modify_flags & SVN_WC__ENTRY_MODIFY_URL)
cur_entry->url = entry->url ? apr_pstrdup(pool, entry->url) : NULL;
if (modify_flags & SVN_WC__ENTRY_MODIFY_REPOS)
cur_entry->repos = entry->repos ? apr_pstrdup(pool, entry->repos) : NULL;
if (modify_flags & SVN_WC__ENTRY_MODIFY_KIND)
cur_entry->kind = entry->kind;
if (modify_flags & SVN_WC__ENTRY_MODIFY_SCHEDULE)
cur_entry->schedule = entry->schedule;
if (modify_flags & SVN_WC__ENTRY_MODIFY_CHECKSUM)
cur_entry->checksum = entry->checksum
? apr_pstrdup(pool, entry->checksum)
: NULL;
if (modify_flags & SVN_WC__ENTRY_MODIFY_COPIED)
cur_entry->copied = entry->copied;
if (modify_flags & SVN_WC__ENTRY_MODIFY_COPYFROM_URL)
cur_entry->copyfrom_url = entry->copyfrom_url
? apr_pstrdup(pool, entry->copyfrom_url)
: NULL;
if (modify_flags & SVN_WC__ENTRY_MODIFY_COPYFROM_REV)
cur_entry->copyfrom_rev = entry->copyfrom_rev;
if (modify_flags & SVN_WC__ENTRY_MODIFY_DELETED)
cur_entry->deleted = entry->deleted;
if (modify_flags & SVN_WC__ENTRY_MODIFY_ABSENT)
cur_entry->absent = entry->absent;
if (modify_flags & SVN_WC__ENTRY_MODIFY_INCOMPLETE)
cur_entry->incomplete = entry->incomplete;
if (modify_flags & SVN_WC__ENTRY_MODIFY_TEXT_TIME)
cur_entry->text_time = entry->text_time;
if (modify_flags & SVN_WC__ENTRY_MODIFY_PROP_TIME)
cur_entry->prop_time = entry->prop_time;
if (modify_flags & SVN_WC__ENTRY_MODIFY_CONFLICT_OLD)
cur_entry->conflict_old = entry->conflict_old
? apr_pstrdup(pool, entry->conflict_old)
: NULL;
if (modify_flags & SVN_WC__ENTRY_MODIFY_CONFLICT_NEW)
cur_entry->conflict_new = entry->conflict_new
? apr_pstrdup(pool, entry->conflict_new)
: NULL;
if (modify_flags & SVN_WC__ENTRY_MODIFY_CONFLICT_WRK)
cur_entry->conflict_wrk = entry->conflict_wrk
? apr_pstrdup(pool, entry->conflict_wrk)
: NULL;
if (modify_flags & SVN_WC__ENTRY_MODIFY_PREJFILE)
cur_entry->prejfile = entry->prejfile
? apr_pstrdup(pool, entry->prejfile)
: NULL;
if (modify_flags & SVN_WC__ENTRY_MODIFY_CMT_REV)
cur_entry->cmt_rev = entry->cmt_rev;
if (modify_flags & SVN_WC__ENTRY_MODIFY_CMT_DATE)
cur_entry->cmt_date = entry->cmt_date;
if (modify_flags & SVN_WC__ENTRY_MODIFY_CMT_AUTHOR)
cur_entry->cmt_author = entry->cmt_author
? apr_pstrdup(pool, entry->cmt_author)
: NULL;
if (modify_flags & SVN_WC__ENTRY_MODIFY_UUID)
cur_entry->uuid = entry->uuid
? apr_pstrdup(pool, entry->uuid)
: NULL;
if (modify_flags & SVN_WC__ENTRY_MODIFY_LOCK_TOKEN)
cur_entry->lock_token = (entry->lock_token
? apr_pstrdup(pool, entry->lock_token)
: NULL);
if (modify_flags & SVN_WC__ENTRY_MODIFY_LOCK_OWNER)
cur_entry->lock_owner = (entry->lock_owner
? apr_pstrdup(pool, entry->lock_owner)
: NULL);
if (modify_flags & SVN_WC__ENTRY_MODIFY_LOCK_COMMENT)
cur_entry->lock_comment = (entry->lock_comment
? apr_pstrdup(pool, entry->lock_comment)
: NULL);
if (modify_flags & SVN_WC__ENTRY_MODIFY_LOCK_CREATION_DATE)
cur_entry->lock_creation_date = entry->lock_creation_date;
if (modify_flags & SVN_WC__ENTRY_MODIFY_CHANGELIST)
cur_entry->changelist = (entry->changelist
? apr_pstrdup(pool, entry->changelist)
: NULL);
if (modify_flags & SVN_WC__ENTRY_MODIFY_HAS_PROPS)
cur_entry->has_props = entry->has_props;
if (modify_flags & SVN_WC__ENTRY_MODIFY_HAS_PROP_MODS)
cur_entry->has_prop_mods = entry->has_prop_mods;
if (modify_flags & SVN_WC__ENTRY_MODIFY_CACHABLE_PROPS)
cur_entry->cachable_props = (entry->cachable_props
? apr_pstrdup(pool, entry->cachable_props)
: NULL);
if (modify_flags & SVN_WC__ENTRY_MODIFY_PRESENT_PROPS)
cur_entry->present_props = (entry->present_props
? apr_pstrdup(pool, entry->present_props)
: NULL);
if (modify_flags & SVN_WC__ENTRY_MODIFY_KEEP_LOCAL)
cur_entry->keep_local = entry->keep_local;
if (cur_entry->kind != svn_node_dir) {
svn_wc_entry_t *default_entry
= apr_hash_get(entries, SVN_WC_ENTRY_THIS_DIR, APR_HASH_KEY_STRING);
if (default_entry)
take_from_entry(default_entry, cur_entry, pool);
}
if (modify_flags & SVN_WC__ENTRY_MODIFY_SCHEDULE
&& entry->schedule == svn_wc_schedule_delete) {
cur_entry->copied = FALSE;
cur_entry->copyfrom_rev = SVN_INVALID_REVNUM;
cur_entry->copyfrom_url = NULL;
}
if (modify_flags & SVN_WC__ENTRY_MODIFY_WORKING_SIZE)
cur_entry->working_size = entry->working_size;
if (modify_flags & SVN_WC__ENTRY_MODIFY_SCHEDULE
&& entry->schedule != svn_wc_schedule_delete) {
cur_entry->keep_local = FALSE;
}
apr_hash_set(entries, cur_entry->name, APR_HASH_KEY_STRING, cur_entry);
}
void
svn_wc__entry_remove(apr_hash_t *entries, const char *name) {
apr_hash_set(entries, name, APR_HASH_KEY_STRING, NULL);
}
static svn_error_t *
fold_scheduling(apr_hash_t *entries,
const char *name,
apr_uint64_t *modify_flags,
svn_wc_schedule_t *schedule,
apr_pool_t *pool) {
svn_wc_entry_t *entry, *this_dir_entry;
if (! (*modify_flags & SVN_WC__ENTRY_MODIFY_SCHEDULE))
return SVN_NO_ERROR;
entry = apr_hash_get(entries, name, APR_HASH_KEY_STRING);
if (*modify_flags & SVN_WC__ENTRY_MODIFY_FORCE) {
switch (*schedule) {
case svn_wc_schedule_add:
case svn_wc_schedule_delete:
case svn_wc_schedule_replace:
case svn_wc_schedule_normal:
return SVN_NO_ERROR;
default:
return svn_error_create(SVN_ERR_WC_SCHEDULE_CONFLICT, NULL, NULL);
}
}
if (! entry) {
if (*schedule == svn_wc_schedule_add)
return SVN_NO_ERROR;
else
return
svn_error_createf(SVN_ERR_WC_SCHEDULE_CONFLICT, NULL,
_("'%s' is not under version control"),
name);
}
this_dir_entry = apr_hash_get(entries, SVN_WC_ENTRY_THIS_DIR,
APR_HASH_KEY_STRING);
if ((entry != this_dir_entry)
&& (this_dir_entry->schedule == svn_wc_schedule_delete)) {
if (*schedule == svn_wc_schedule_add)
return
svn_error_createf(SVN_ERR_WC_SCHEDULE_CONFLICT, NULL,
_("Can't add '%s' to deleted directory; "
"try undeleting its parent directory first"),
name);
if (*schedule == svn_wc_schedule_replace)
return
svn_error_createf(SVN_ERR_WC_SCHEDULE_CONFLICT, NULL,
_("Can't replace '%s' in deleted directory; "
"try undeleting its parent directory first"),
name);
}
if (entry->absent && (*schedule == svn_wc_schedule_add)) {
return svn_error_createf
(SVN_ERR_WC_SCHEDULE_CONFLICT, NULL,
_("'%s' is marked as absent, so it cannot be scheduled for addition"),
name);
}
switch (entry->schedule) {
case svn_wc_schedule_normal:
switch (*schedule) {
case svn_wc_schedule_normal:
*modify_flags &= ~SVN_WC__ENTRY_MODIFY_SCHEDULE;
return SVN_NO_ERROR;
case svn_wc_schedule_delete:
case svn_wc_schedule_replace:
return SVN_NO_ERROR;
case svn_wc_schedule_add:
if (! entry->deleted)
return
svn_error_createf
(SVN_ERR_WC_SCHEDULE_CONFLICT, NULL,
_("Entry '%s' is already under version control"), name);
}
break;
case svn_wc_schedule_add:
switch (*schedule) {
case svn_wc_schedule_normal:
case svn_wc_schedule_add:
case svn_wc_schedule_replace:
*modify_flags &= ~SVN_WC__ENTRY_MODIFY_SCHEDULE;
return SVN_NO_ERROR;
case svn_wc_schedule_delete:
assert(entry != this_dir_entry);
if (! entry->deleted)
apr_hash_set(entries, name, APR_HASH_KEY_STRING, NULL);
else
*schedule = svn_wc_schedule_normal;
return SVN_NO_ERROR;
}
break;
case svn_wc_schedule_delete:
switch (*schedule) {
case svn_wc_schedule_normal:
return SVN_NO_ERROR;
case svn_wc_schedule_delete:
*modify_flags &= ~SVN_WC__ENTRY_MODIFY_SCHEDULE;
return SVN_NO_ERROR;
case svn_wc_schedule_add:
*schedule = svn_wc_schedule_replace;
return SVN_NO_ERROR;
case svn_wc_schedule_replace:
return SVN_NO_ERROR;
}
break;
case svn_wc_schedule_replace:
switch (*schedule) {
case svn_wc_schedule_normal:
return SVN_NO_ERROR;
case svn_wc_schedule_add:
case svn_wc_schedule_replace:
*modify_flags &= ~SVN_WC__ENTRY_MODIFY_SCHEDULE;
return SVN_NO_ERROR;
case svn_wc_schedule_delete:
*schedule = svn_wc_schedule_delete;
return SVN_NO_ERROR;
}
break;
default:
return
svn_error_createf
(SVN_ERR_WC_SCHEDULE_CONFLICT, NULL,
_("Entry '%s' has illegal schedule"), name);
}
return SVN_NO_ERROR;
}
svn_error_t *
svn_wc__entry_modify(svn_wc_adm_access_t *adm_access,
const char *name,
svn_wc_entry_t *entry,
apr_uint64_t modify_flags,
svn_boolean_t do_sync,
apr_pool_t *pool) {
apr_hash_t *entries, *entries_nohidden;
svn_boolean_t entry_was_deleted_p = FALSE;
assert(entry);
SVN_ERR(svn_wc_entries_read(&entries, adm_access, TRUE, pool));
SVN_ERR(svn_wc_entries_read(&entries_nohidden, adm_access, FALSE, pool));
if (name == NULL)
name = SVN_WC_ENTRY_THIS_DIR;
if (modify_flags & SVN_WC__ENTRY_MODIFY_SCHEDULE) {
svn_wc_entry_t *entry_before, *entry_after;
apr_uint64_t orig_modify_flags = modify_flags;
svn_wc_schedule_t orig_schedule = entry->schedule;
entry_before = apr_hash_get(entries, name, APR_HASH_KEY_STRING);
SVN_ERR(fold_scheduling(entries, name, &modify_flags,
&entry->schedule, pool));
if (entries != entries_nohidden) {
SVN_ERR(fold_scheduling(entries_nohidden, name, &orig_modify_flags,
&orig_schedule, pool));
assert(orig_modify_flags == modify_flags);
assert(orig_schedule == entry->schedule);
}
entry_after = apr_hash_get(entries, name, APR_HASH_KEY_STRING);
if (entry_before && (! entry_after))
entry_was_deleted_p = TRUE;
}
if (! entry_was_deleted_p) {
fold_entry(entries, name, modify_flags, entry,
svn_wc_adm_access_pool(adm_access));
if (entries != entries_nohidden)
fold_entry(entries_nohidden, name, modify_flags, entry,
svn_wc_adm_access_pool(adm_access));
}
if (do_sync)
SVN_ERR(svn_wc__entries_write(entries, adm_access, pool));
return SVN_NO_ERROR;
}
svn_wc_entry_t *
svn_wc_entry_dup(const svn_wc_entry_t *entry, apr_pool_t *pool) {
svn_wc_entry_t *dupentry = apr_palloc(pool, sizeof(*dupentry));
*dupentry = *entry;
if (entry->name)
dupentry->name = apr_pstrdup(pool, entry->name);
if (entry->url)
dupentry->url = apr_pstrdup(pool, entry->url);
if (entry->repos)
dupentry->repos = apr_pstrdup(pool, entry->repos);
if (entry->uuid)
dupentry->uuid = apr_pstrdup(pool, entry->uuid);
if (entry->copyfrom_url)
dupentry->copyfrom_url = apr_pstrdup(pool, entry->copyfrom_url);
if (entry->conflict_old)
dupentry->conflict_old = apr_pstrdup(pool, entry->conflict_old);
if (entry->conflict_new)
dupentry->conflict_new = apr_pstrdup(pool, entry->conflict_new);
if (entry->conflict_wrk)
dupentry->conflict_wrk = apr_pstrdup(pool, entry->conflict_wrk);
if (entry->prejfile)
dupentry->prejfile = apr_pstrdup(pool, entry->prejfile);
if (entry->checksum)
dupentry->checksum = apr_pstrdup(pool, entry->checksum);
if (entry->cmt_author)
dupentry->cmt_author = apr_pstrdup(pool, entry->cmt_author);
if (entry->lock_token)
dupentry->lock_token = apr_pstrdup(pool, entry->lock_token);
if (entry->lock_owner)
dupentry->lock_owner = apr_pstrdup(pool, entry->lock_owner);
if (entry->lock_comment)
dupentry->lock_comment = apr_pstrdup(pool, entry->lock_comment);
if (entry->changelist)
dupentry->changelist = apr_pstrdup(pool, entry->changelist);
if (entry->cachable_props)
dupentry->cachable_props = apr_pstrdup(pool, entry->cachable_props);
if (entry->present_props)
dupentry->present_props = apr_pstrdup(pool, entry->present_props);
return dupentry;
}
svn_error_t *
svn_wc__tweak_entry(apr_hash_t *entries,
const char *name,
const char *new_url,
const char *repos,
svn_revnum_t new_rev,
svn_boolean_t allow_removal,
svn_boolean_t *write_required,
apr_pool_t *pool) {
svn_wc_entry_t *entry;
entry = apr_hash_get(entries, name, APR_HASH_KEY_STRING);
if (! entry)
return svn_error_createf(SVN_ERR_ENTRY_NOT_FOUND, NULL,
_("No such entry: '%s'"), name);
if (new_url != NULL
&& (! entry->url || strcmp(new_url, entry->url))) {
*write_required = TRUE;
entry->url = apr_pstrdup(pool, new_url);
}
if (repos != NULL
&& (! entry->repos || strcmp(repos, entry->repos))
&& entry->url
&& svn_path_is_ancestor(repos, entry->url)) {
svn_boolean_t set_repos = TRUE;
if (strcmp(entry->name, SVN_WC_ENTRY_THIS_DIR) == 0) {
apr_hash_index_t *hi;
for (hi = apr_hash_first(pool, entries); hi;
hi = apr_hash_next(hi)) {
void *value;
const svn_wc_entry_t *child_entry;
apr_hash_this(hi, NULL, NULL, &value);
child_entry = value;
if (! child_entry->repos && child_entry->url
&& ! svn_path_is_ancestor(repos, child_entry->url)) {
set_repos = FALSE;
break;
}
}
}
if (set_repos) {
*write_required = TRUE;
entry->repos = apr_pstrdup(pool, repos);
}
}
if ((SVN_IS_VALID_REVNUM(new_rev))
&& (entry->schedule != svn_wc_schedule_add)
&& (entry->schedule != svn_wc_schedule_replace)
&& (entry->copied != TRUE)
&& (entry->revision != new_rev)) {
*write_required = TRUE;
entry->revision = new_rev;
}
if (allow_removal
&& (entry->deleted || (entry->absent && entry->revision != new_rev))) {
*write_required = TRUE;
apr_hash_set(entries, name, APR_HASH_KEY_STRING, NULL);
}
return SVN_NO_ERROR;
}
svn_error_t *
svn_wc__entries_init(const char *path,
const char *uuid,
const char *url,
const char *repos,
svn_revnum_t initial_rev,
svn_depth_t depth,
apr_pool_t *pool) {
apr_file_t *f = NULL;
svn_stringbuf_t *accum = svn_stringbuf_createf(pool, "%d\n",
SVN_WC__VERSION);
svn_wc_entry_t *entry = alloc_entry(pool);
assert(! repos || svn_path_is_ancestor(repos, url));
assert(depth == svn_depth_empty
|| depth == svn_depth_files
|| depth == svn_depth_immediates
|| depth == svn_depth_infinity);
SVN_ERR(svn_wc__open_adm_file(&f, path, SVN_WC__ADM_ENTRIES,
(APR_WRITE | APR_CREATE | APR_EXCL), pool));
entry->kind = svn_node_dir;
entry->url = url;
entry->revision = initial_rev;
entry->uuid = uuid;
entry->repos = repos;
entry->depth = depth;
if (initial_rev > 0)
entry->incomplete = TRUE;
entry->cachable_props = SVN_WC__CACHABLE_PROPS;
write_entry(accum, entry, SVN_WC_ENTRY_THIS_DIR, entry, pool);
SVN_ERR_W(svn_io_file_write_full(f, accum->data, accum->len, NULL, pool),
apr_psprintf(pool,
_("Error writing entries file for '%s'"),
svn_path_local_style(path, pool)));
SVN_ERR(svn_wc__close_adm_file(f, path, SVN_WC__ADM_ENTRIES, 1, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
walker_helper(const char *dirpath,
svn_wc_adm_access_t *adm_access,
const svn_wc_entry_callbacks2_t *walk_callbacks,
void *walk_baton,
svn_depth_t depth,
svn_boolean_t show_hidden,
svn_cancel_func_t cancel_func,
void *cancel_baton,
apr_pool_t *pool) {
apr_pool_t *subpool = svn_pool_create(pool);
apr_hash_t *entries;
apr_hash_index_t *hi;
svn_wc_entry_t *dot_entry;
SVN_ERR(walk_callbacks->handle_error
(dirpath, svn_wc_entries_read(&entries, adm_access, show_hidden,
pool), walk_baton, pool));
dot_entry = apr_hash_get(entries, SVN_WC_ENTRY_THIS_DIR,
APR_HASH_KEY_STRING);
if (! dot_entry)
return walk_callbacks->handle_error
(dirpath, svn_error_createf(SVN_ERR_ENTRY_NOT_FOUND, NULL,
_("Directory '%s' has no THIS_DIR entry"),
svn_path_local_style(dirpath, pool)),
walk_baton, pool);
SVN_ERR(walk_callbacks->handle_error
(dirpath,
walk_callbacks->found_entry(dirpath, dot_entry, walk_baton, pool),
walk_baton, pool));
if (depth == svn_depth_empty)
return SVN_NO_ERROR;
for (hi = apr_hash_first(pool, entries); hi; hi = apr_hash_next(hi)) {
const void *key;
void *val;
const svn_wc_entry_t *current_entry;
const char *entrypath;
svn_pool_clear(subpool);
if (cancel_func)
SVN_ERR(cancel_func(cancel_baton));
apr_hash_this(hi, &key, NULL, &val);
current_entry = val;
if (strcmp(current_entry->name, SVN_WC_ENTRY_THIS_DIR) == 0)
continue;
entrypath = svn_path_join(dirpath, key, subpool);
if (current_entry->kind == svn_node_file
|| depth >= svn_depth_immediates) {
SVN_ERR(walk_callbacks->handle_error
(entrypath,
walk_callbacks->found_entry(entrypath, current_entry,
walk_baton, subpool),
walk_baton, pool));
}
if (current_entry->kind == svn_node_dir
&& depth >= svn_depth_immediates) {
svn_wc_adm_access_t *entry_access;
svn_depth_t depth_below_here = depth;
if (depth == svn_depth_immediates)
depth_below_here = svn_depth_empty;
SVN_ERR(walk_callbacks->handle_error
(entrypath,
svn_wc_adm_retrieve(&entry_access, adm_access, entrypath,
subpool),
walk_baton, pool));
if (entry_access)
SVN_ERR(walker_helper(entrypath, entry_access,
walk_callbacks, walk_baton,
depth_below_here, show_hidden,
cancel_func, cancel_baton,
subpool));
}
}
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
svn_error_t *
svn_wc_walk_entries(const char *path,
svn_wc_adm_access_t *adm_access,
const svn_wc_entry_callbacks_t *walk_callbacks,
void *walk_baton,
svn_boolean_t show_hidden,
apr_pool_t *pool) {
return svn_wc_walk_entries2(path, adm_access, walk_callbacks,
walk_baton, show_hidden, NULL, NULL,
pool);
}
svn_error_t *
svn_wc__walker_default_error_handler(const char *path,
svn_error_t *err,
void *walk_baton,
apr_pool_t *pool) {
return err;
}
svn_error_t *
svn_wc_walk_entries2(const char *path,
svn_wc_adm_access_t *adm_access,
const svn_wc_entry_callbacks_t *walk_callbacks,
void *walk_baton,
svn_boolean_t show_hidden,
svn_cancel_func_t cancel_func,
void *cancel_baton,
apr_pool_t *pool) {
svn_wc_entry_callbacks2_t walk_cb2 = { walk_callbacks->found_entry,
svn_wc__walker_default_error_handler
};
return svn_wc_walk_entries3(path, adm_access,
&walk_cb2, walk_baton, svn_depth_infinity,
show_hidden, cancel_func, cancel_baton, pool);
}
svn_error_t *
svn_wc_walk_entries3(const char *path,
svn_wc_adm_access_t *adm_access,
const svn_wc_entry_callbacks2_t *walk_callbacks,
void *walk_baton,
svn_depth_t depth,
svn_boolean_t show_hidden,
svn_cancel_func_t cancel_func,
void *cancel_baton,
apr_pool_t *pool) {
const svn_wc_entry_t *entry;
SVN_ERR(svn_wc_entry(&entry, path, adm_access, show_hidden, pool));
if (! entry)
return walk_callbacks->handle_error
(path, svn_error_createf(SVN_ERR_UNVERSIONED_RESOURCE, NULL,
_("'%s' is not under version control"),
svn_path_local_style(path, pool)),
walk_baton, pool);
if (entry->kind == svn_node_file)
return walk_callbacks->handle_error
(path, walk_callbacks->found_entry(path, entry, walk_baton, pool),
walk_baton, pool);
else if (entry->kind == svn_node_dir)
return walker_helper(path, adm_access, walk_callbacks, walk_baton,
depth, show_hidden, cancel_func, cancel_baton, pool);
else
return walk_callbacks->handle_error
(path, svn_error_createf(SVN_ERR_NODE_UNKNOWN_KIND, NULL,
_("'%s' has an unrecognized node kind"),
svn_path_local_style(path, pool)),
walk_baton, pool);
}
svn_error_t *
svn_wc_mark_missing_deleted(const char *path,
svn_wc_adm_access_t *parent,
apr_pool_t *pool) {
svn_node_kind_t pkind;
SVN_ERR(svn_io_check_path(path, &pkind, pool));
if (pkind == svn_node_none) {
const char *parent_path, *bname;
svn_wc_adm_access_t *adm_access;
svn_wc_entry_t newent;
newent.deleted = TRUE;
newent.schedule = svn_wc_schedule_normal;
svn_path_split(path, &parent_path, &bname, pool);
SVN_ERR(svn_wc_adm_retrieve(&adm_access, parent, parent_path, pool));
SVN_ERR(svn_wc__entry_modify(adm_access, bname, &newent,
(SVN_WC__ENTRY_MODIFY_DELETED
| SVN_WC__ENTRY_MODIFY_SCHEDULE
| SVN_WC__ENTRY_MODIFY_FORCE),
TRUE, pool));
return SVN_NO_ERROR;
} else
return svn_error_createf(SVN_ERR_WC_PATH_FOUND, NULL,
_("Unexpectedly found '%s': "
"path is marked 'missing'"),
svn_path_local_style(path, pool));
}
