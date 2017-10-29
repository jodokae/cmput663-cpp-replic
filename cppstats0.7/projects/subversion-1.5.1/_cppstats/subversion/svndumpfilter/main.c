#include <stdlib.h>
#include <apr_file_io.h>
#include "svn_private_config.h"
#include "svn_cmdline.h"
#include "svn_error.h"
#include "svn_string.h"
#include "svn_opt.h"
#include "svn_utf.h"
#include "svn_path.h"
#include "svn_hash.h"
#include "svn_repos.h"
#include "svn_fs.h"
#include "svn_pools.h"
#include "svn_sorts.h"
#include "svn_props.h"
#include "svn_mergeinfo.h"
#include "private/svn_mergeinfo_private.h"
static svn_error_t *
create_stdio_stream(svn_stream_t **stream,
APR_DECLARE(apr_status_t) open_fn(apr_file_t **,
apr_pool_t *),
apr_pool_t *pool) {
apr_file_t *stdio_file;
apr_status_t apr_err = open_fn(&stdio_file, pool);
if (apr_err)
return svn_error_wrap_apr(apr_err, _("Can't open stdio file"));
*stream = svn_stream_from_aprfile(stdio_file, pool);
return SVN_NO_ERROR;
}
static void
write_prop_to_stringbuf(svn_stringbuf_t **strbuf,
const char *name,
const svn_string_t *value) {
int bytes_used, namelen;
char buf[SVN_KEYLINE_MAXLEN];
namelen = strlen(name);
svn_stringbuf_appendbytes(*strbuf, "K ", 2);
bytes_used = sprintf(buf, "%d", namelen);
svn_stringbuf_appendbytes(*strbuf, buf, bytes_used);
svn_stringbuf_appendbytes(*strbuf, "\n", 1);
svn_stringbuf_appendbytes(*strbuf, name, namelen);
svn_stringbuf_appendbytes(*strbuf, "\n", 1);
svn_stringbuf_appendbytes(*strbuf, "V ", 2);
bytes_used = sprintf(buf, "%" APR_SIZE_T_FMT, value->len);
svn_stringbuf_appendbytes(*strbuf, buf, bytes_used);
svn_stringbuf_appendbytes(*strbuf, "\n", 1);
svn_stringbuf_appendbytes(*strbuf, value->data, value->len);
svn_stringbuf_appendbytes(*strbuf, "\n", 1);
}
static svn_boolean_t
ary_prefix_match(apr_array_header_t *pfxlist, const char *path) {
int i, pfx_len, path_len = strlen(path);
const char *pfx;
for (i = 0; i < pfxlist->nelts; i++) {
pfx = APR_ARRAY_IDX(pfxlist, i, const char *);
pfx_len = strlen(pfx);
if (path_len < pfx_len)
continue;
if (strncmp(path, pfx, pfx_len) == 0
&& (path[pfx_len] == '\0' || path[pfx_len] == '/'))
return TRUE;
}
return FALSE;
}
static APR_INLINE svn_boolean_t
skip_path(const char *path, apr_array_header_t *prefixes,
svn_boolean_t do_exclude) {
return (ary_prefix_match(prefixes, path) ? do_exclude : !do_exclude);
}
struct revmap_t {
svn_revnum_t rev;
svn_boolean_t was_dropped;
};
struct parse_baton_t {
svn_boolean_t do_exclude;
svn_boolean_t quiet;
svn_boolean_t drop_empty_revs;
svn_boolean_t do_renumber_revs;
svn_boolean_t preserve_revprops;
svn_boolean_t skip_missing_merge_sources;
apr_array_header_t *prefixes;
svn_stream_t *in_stream;
svn_stream_t *out_stream;
apr_int32_t rev_drop_count;
apr_hash_t *dropped_nodes;
apr_hash_t *renumber_history;
svn_revnum_t last_live_revision;
};
struct revision_baton_t {
struct parse_baton_t *pb;
svn_boolean_t has_nodes;
svn_boolean_t has_props;
svn_boolean_t had_dropped_nodes;
svn_boolean_t writing_begun;
svn_revnum_t rev_orig;
svn_revnum_t rev_actual;
svn_stringbuf_t *header;
apr_hash_t *props;
};
struct node_baton_t {
struct revision_baton_t *rb;
svn_boolean_t do_skip;
svn_boolean_t has_props;
svn_boolean_t has_text;
svn_boolean_t writing_begun;
svn_filesize_t tcl;
svn_stringbuf_t *header;
svn_stringbuf_t *props;
};
static svn_error_t *
new_revision_record(void **revision_baton,
apr_hash_t *headers,
void *parse_baton,
apr_pool_t *pool) {
struct revision_baton_t *rb;
apr_hash_index_t *hi;
const void *key;
void *val;
svn_stream_t *header_stream;
*revision_baton = apr_palloc(pool, sizeof(struct revision_baton_t));
rb = *revision_baton;
rb->pb = parse_baton;
rb->has_nodes = FALSE;
rb->has_props = FALSE;
rb->had_dropped_nodes = FALSE;
rb->writing_begun = FALSE;
rb->header = svn_stringbuf_create("", pool);
rb->props = apr_hash_make(pool);
header_stream = svn_stream_from_stringbuf(rb->header, pool);
val = apr_hash_get(headers, SVN_REPOS_DUMPFILE_REVISION_NUMBER,
APR_HASH_KEY_STRING);
rb->rev_orig = SVN_STR_TO_REV(val);
if (rb->pb->do_renumber_revs)
rb->rev_actual = rb->rev_orig - rb->pb->rev_drop_count;
else
rb->rev_actual = rb->rev_orig;
SVN_ERR(svn_stream_printf(header_stream, pool,
SVN_REPOS_DUMPFILE_REVISION_NUMBER ": %ld\n",
rb->rev_actual));
for (hi = apr_hash_first(pool, headers); hi; hi = apr_hash_next(hi)) {
apr_hash_this(hi, &key, NULL, &val);
if ((!strcmp(key, SVN_REPOS_DUMPFILE_CONTENT_LENGTH))
|| (!strcmp(key, SVN_REPOS_DUMPFILE_PROP_CONTENT_LENGTH))
|| (!strcmp(key, SVN_REPOS_DUMPFILE_REVISION_NUMBER)))
continue;
SVN_ERR(svn_stream_printf(header_stream, pool, "%s: %s\n",
(const char *)key,
(const char *)val));
}
SVN_ERR(svn_stream_close(header_stream));
return SVN_NO_ERROR;
}
static svn_error_t *
output_revision(struct revision_baton_t *rb) {
int bytes_used;
char buf[SVN_KEYLINE_MAXLEN];
apr_hash_index_t *hi;
apr_pool_t *hash_pool = apr_hash_pool_get(rb->props);
svn_stringbuf_t *props = svn_stringbuf_create("", hash_pool);
apr_pool_t *subpool = svn_pool_create(hash_pool);
rb->writing_begun = TRUE;
if ((! rb->pb->preserve_revprops)
&& (! rb->has_nodes)
&& rb->had_dropped_nodes
&& (! rb->pb->drop_empty_revs)) {
apr_hash_t *old_props = rb->props;
rb->has_props = TRUE;
rb->props = apr_hash_make(hash_pool);
apr_hash_set(rb->props, SVN_PROP_REVISION_DATE, APR_HASH_KEY_STRING,
apr_hash_get(old_props, SVN_PROP_REVISION_DATE,
APR_HASH_KEY_STRING));
apr_hash_set(rb->props, SVN_PROP_REVISION_LOG, APR_HASH_KEY_STRING,
svn_string_create(_("This is an empty revision for "
"padding."), hash_pool));
}
if (rb->has_props) {
for (hi = apr_hash_first(subpool, rb->props);
hi;
hi = apr_hash_next(hi)) {
const void *key;
void *val;
apr_hash_this(hi, &key, NULL, &val);
write_prop_to_stringbuf(&props, key, val);
}
svn_stringbuf_appendcstr(props, "PROPS-END\n");
svn_stringbuf_appendcstr(rb->header,
SVN_REPOS_DUMPFILE_PROP_CONTENT_LENGTH);
bytes_used = sprintf(buf, ": %" APR_SIZE_T_FMT, props->len);
svn_stringbuf_appendbytes(rb->header, buf, bytes_used);
svn_stringbuf_appendbytes(rb->header, "\n", 1);
}
svn_stringbuf_appendcstr(rb->header, SVN_REPOS_DUMPFILE_CONTENT_LENGTH);
bytes_used = sprintf(buf, ": %" APR_SIZE_T_FMT, props->len);
svn_stringbuf_appendbytes(rb->header, buf, bytes_used);
svn_stringbuf_appendbytes(rb->header, "\n", 1);
svn_stringbuf_appendbytes(rb->header, "\n", 1);
svn_stringbuf_appendbytes(props, "\n", 1);
if (rb->has_nodes
|| (! rb->pb->drop_empty_revs)
|| (! rb->had_dropped_nodes)) {
SVN_ERR(svn_stream_write(rb->pb->out_stream,
rb->header->data, &(rb->header->len)));
SVN_ERR(svn_stream_write(rb->pb->out_stream,
props->data, &(props->len)));
if (rb->pb->do_renumber_revs) {
svn_revnum_t *rr_key;
struct revmap_t *rr_val;
apr_pool_t *rr_pool = apr_hash_pool_get(rb->pb->renumber_history);
rr_key = apr_palloc(rr_pool, sizeof(*rr_key));
rr_val = apr_palloc(rr_pool, sizeof(*rr_val));
*rr_key = rb->rev_orig;
rr_val->rev = rb->rev_actual;
rr_val->was_dropped = FALSE;
apr_hash_set(rb->pb->renumber_history, rr_key,
sizeof(*rr_key), rr_val);
rb->pb->last_live_revision = rb->rev_actual;
}
if (! rb->pb->quiet)
SVN_ERR(svn_cmdline_fprintf(stderr, subpool,
_("Revision %ld committed as %ld.\n"),
rb->rev_orig, rb->rev_actual));
} else {
rb->pb->rev_drop_count++;
if (rb->pb->do_renumber_revs) {
svn_revnum_t *rr_key;
struct revmap_t *rr_val;
apr_pool_t *rr_pool = apr_hash_pool_get(rb->pb->renumber_history);
rr_key = apr_palloc(rr_pool, sizeof(*rr_key));
rr_val = apr_palloc(rr_pool, sizeof(*rr_val));
*rr_key = rb->rev_orig;
rr_val->rev = rb->pb->last_live_revision;
rr_val->was_dropped = TRUE;
apr_hash_set(rb->pb->renumber_history, rr_key,
sizeof(*rr_key), rr_val);
}
if (! rb->pb->quiet)
SVN_ERR(svn_cmdline_fprintf(stderr, subpool,
_("Revision %ld skipped.\n"),
rb->rev_orig));
}
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
static svn_error_t *
uuid_record(const char *uuid, void *parse_baton, apr_pool_t *pool) {
struct parse_baton_t *pb = parse_baton;
SVN_ERR(svn_stream_printf(pb->out_stream, pool,
SVN_REPOS_DUMPFILE_UUID ": %s\n\n", uuid));
return SVN_NO_ERROR;
}
static svn_error_t *
new_node_record(void **node_baton,
apr_hash_t *headers,
void *rev_baton,
apr_pool_t *pool) {
struct parse_baton_t *pb;
struct node_baton_t *nb;
char *node_path, *copyfrom_path;
apr_hash_index_t *hi;
const void *key;
void *val;
const char *tcl;
*node_baton = apr_palloc(pool, sizeof(struct node_baton_t));
nb = *node_baton;
nb->rb = rev_baton;
pb = nb->rb->pb;
node_path = apr_hash_get(headers, SVN_REPOS_DUMPFILE_NODE_PATH,
APR_HASH_KEY_STRING);
copyfrom_path = apr_hash_get(headers,
SVN_REPOS_DUMPFILE_NODE_COPYFROM_PATH,
APR_HASH_KEY_STRING);
node_path = svn_path_join("/", node_path, pool);
if (copyfrom_path)
copyfrom_path = svn_path_join("/", copyfrom_path, pool);
nb->do_skip = skip_path(node_path, pb->prefixes, pb->do_exclude);
if (nb->do_skip) {
apr_hash_set(pb->dropped_nodes,
apr_pstrdup(apr_hash_pool_get(pb->dropped_nodes),
node_path),
APR_HASH_KEY_STRING, (void *)1);
nb->rb->had_dropped_nodes = TRUE;
} else {
tcl = apr_hash_get(headers, SVN_REPOS_DUMPFILE_TEXT_CONTENT_LENGTH,
APR_HASH_KEY_STRING);
if (copyfrom_path &&
skip_path(copyfrom_path, pb->prefixes, pb->do_exclude)) {
const char *kind;
kind = apr_hash_get(headers, SVN_REPOS_DUMPFILE_NODE_KIND,
APR_HASH_KEY_STRING);
if (tcl && (strcmp(kind, "file") == 0)) {
apr_hash_set(headers, SVN_REPOS_DUMPFILE_NODE_COPYFROM_PATH,
APR_HASH_KEY_STRING, NULL);
apr_hash_set(headers, SVN_REPOS_DUMPFILE_NODE_COPYFROM_REV,
APR_HASH_KEY_STRING, NULL);
copyfrom_path = NULL;
}
else {
return svn_error_createf
(SVN_ERR_INCOMPLETE_DATA, 0,
_("Invalid copy source path '%s'"), copyfrom_path);
}
}
nb->has_props = FALSE;
nb->has_text = FALSE;
nb->writing_begun = FALSE;
nb->tcl = tcl ? svn__atoui64(tcl) : 0;
nb->header = svn_stringbuf_create("", pool);
nb->props = svn_stringbuf_create("", pool);
nb->rb->has_nodes = TRUE;
if (! nb->rb->writing_begun)
SVN_ERR(output_revision(nb->rb));
for (hi = apr_hash_first(pool, headers); hi; hi = apr_hash_next(hi)) {
apr_hash_this(hi, (const void **) &key, NULL, &val);
if ((!strcmp(key, SVN_REPOS_DUMPFILE_CONTENT_LENGTH))
|| (!strcmp(key, SVN_REPOS_DUMPFILE_PROP_CONTENT_LENGTH))
|| (!strcmp(key, SVN_REPOS_DUMPFILE_TEXT_CONTENT_LENGTH)))
continue;
if (pb->do_renumber_revs
&& (!strcmp(key, SVN_REPOS_DUMPFILE_NODE_COPYFROM_REV))) {
svn_revnum_t cf_orig_rev;
struct revmap_t *cf_renum_val;
cf_orig_rev = SVN_STR_TO_REV(val);
cf_renum_val = apr_hash_get(pb->renumber_history,
&cf_orig_rev,
sizeof(svn_revnum_t));
if (! (cf_renum_val && SVN_IS_VALID_REVNUM(cf_renum_val->rev)))
return svn_error_createf
(SVN_ERR_NODE_UNEXPECTED_KIND, NULL,
_("No valid copyfrom revision in filtered stream"));
SVN_ERR(svn_stream_printf
(nb->rb->pb->out_stream, pool,
SVN_REPOS_DUMPFILE_NODE_COPYFROM_REV ": %ld\n",
cf_renum_val->rev));
continue;
}
SVN_ERR(svn_stream_printf(nb->rb->pb->out_stream,
pool, "%s: %s\n",
(const char *)key,
(const char *)val));
}
}
return SVN_NO_ERROR;
}
static svn_error_t *
output_node(struct node_baton_t *nb) {
int bytes_used;
char buf[SVN_KEYLINE_MAXLEN];
nb->writing_begun = TRUE;
if (nb->has_props)
svn_stringbuf_appendcstr(nb->props, "PROPS-END\n");
if (nb->has_props) {
svn_stringbuf_appendcstr(nb->header,
SVN_REPOS_DUMPFILE_PROP_CONTENT_LENGTH);
bytes_used = sprintf(buf, ": %" APR_SIZE_T_FMT, nb->props->len);
svn_stringbuf_appendbytes(nb->header, buf, bytes_used);
svn_stringbuf_appendbytes(nb->header, "\n", 1);
}
if (nb->has_text) {
svn_stringbuf_appendcstr(nb->header,
SVN_REPOS_DUMPFILE_TEXT_CONTENT_LENGTH);
bytes_used = sprintf(buf, ": %" SVN_FILESIZE_T_FMT, nb->tcl);
svn_stringbuf_appendbytes(nb->header, buf, bytes_used);
svn_stringbuf_appendbytes(nb->header, "\n", 1);
}
svn_stringbuf_appendcstr(nb->header, SVN_REPOS_DUMPFILE_CONTENT_LENGTH);
bytes_used = sprintf(buf, ": %" SVN_FILESIZE_T_FMT,
(svn_filesize_t) (nb->props->len + nb->tcl));
svn_stringbuf_appendbytes(nb->header, buf, bytes_used);
svn_stringbuf_appendbytes(nb->header, "\n", 1);
svn_stringbuf_appendbytes(nb->header, "\n", 1);
SVN_ERR(svn_stream_write(nb->rb->pb->out_stream,
nb->header->data , &(nb->header->len)));
SVN_ERR(svn_stream_write(nb->rb->pb->out_stream,
nb->props->data , &(nb->props->len)));
return SVN_NO_ERROR;
}
static svn_error_t *
adjust_mergeinfo(svn_string_t **final_val, const svn_string_t *initial_val,
struct revision_baton_t *rb, apr_pool_t *pool) {
apr_hash_t *mergeinfo;
apr_hash_t *final_mergeinfo = apr_hash_make(pool);
apr_hash_index_t *hi;
apr_pool_t *subpool = svn_pool_create(pool);
SVN_ERR(svn_mergeinfo_parse(&mergeinfo, initial_val->data, subpool));
for (hi = apr_hash_first(NULL, mergeinfo); hi; hi = apr_hash_next(hi)) {
const char *merge_source;
apr_array_header_t *rangelist;
struct parse_baton_t *pb = rb->pb;
int i;
const void *key;
void *val;
apr_hash_this(hi, &key, NULL, &val);
merge_source = (const char *) key;
rangelist = (apr_array_header_t *) val;
if (skip_path(merge_source, pb->prefixes, pb->do_exclude)) {
if (pb->skip_missing_merge_sources)
continue;
else
return svn_error_createf(SVN_ERR_INCOMPLETE_DATA, 0,
_("Missing merge source path '%s'; try "
"with --skip-missing-merge-sources"),
merge_source);
}
if (pb->do_renumber_revs) {
for (i = 0; i < rangelist->nelts; i++) {
struct revmap_t *revmap_start;
struct revmap_t *revmap_end;
svn_merge_range_t *range = APR_ARRAY_IDX(rangelist, i,
svn_merge_range_t *);
revmap_start = apr_hash_get(pb->renumber_history,
&range->start, sizeof(svn_revnum_t));
if (! (revmap_start && SVN_IS_VALID_REVNUM(revmap_start->rev)))
return svn_error_createf
(SVN_ERR_NODE_UNEXPECTED_KIND, NULL,
_("No valid revision range 'start' in filtered stream"));
revmap_end = apr_hash_get(pb->renumber_history,
&range->end, sizeof(svn_revnum_t));
if (! (revmap_end && SVN_IS_VALID_REVNUM(revmap_end->rev)))
return svn_error_createf
(SVN_ERR_NODE_UNEXPECTED_KIND, NULL,
_("No valid revision range 'end' in filtered stream"));
range->start = revmap_start->rev;
range->end = revmap_end->rev;
}
}
apr_hash_set(final_mergeinfo, merge_source,
APR_HASH_KEY_STRING, rangelist);
}
SVN_ERR(svn_mergeinfo_sort(final_mergeinfo, subpool));
SVN_ERR(svn_mergeinfo_to_string(final_val, final_mergeinfo, pool));
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
static svn_error_t *
set_revision_property(void *revision_baton,
const char *name,
const svn_string_t *value) {
struct revision_baton_t *rb = revision_baton;
apr_pool_t *hash_pool = apr_hash_pool_get(rb->props);
rb->has_props = TRUE;
apr_hash_set(rb->props, apr_pstrdup(hash_pool, name),
APR_HASH_KEY_STRING, svn_string_dup(value, hash_pool));
return SVN_NO_ERROR;
}
static svn_error_t *
set_node_property(void *node_baton,
const char *name,
const svn_string_t *value) {
struct node_baton_t *nb = node_baton;
struct revision_baton_t *rb = nb->rb;
if (nb->do_skip)
return SVN_NO_ERROR;
if (!nb->has_props)
return svn_error_create(SVN_ERR_UNSUPPORTED_FEATURE, NULL,
_("Delta property block detected - "
"not supported by svndumpfilter"));
if (strcmp(name, SVN_PROP_MERGEINFO) == 0) {
svn_string_t *filtered_mergeinfo;
apr_pool_t *pool = apr_hash_pool_get(rb->props);
SVN_ERR(adjust_mergeinfo(&filtered_mergeinfo, value, rb, pool));
value = filtered_mergeinfo;
}
write_prop_to_stringbuf(&(nb->props), name, value);
return SVN_NO_ERROR;
}
static svn_error_t *
remove_node_props(void *node_baton) {
struct node_baton_t *nb = node_baton;
nb->has_props = TRUE;
return SVN_NO_ERROR;
}
static svn_error_t *
set_fulltext(svn_stream_t **stream, void *node_baton) {
struct node_baton_t *nb = node_baton;
if (!nb->do_skip) {
nb->has_text = TRUE;
if (! nb->writing_begun)
SVN_ERR(output_node(nb));
*stream = nb->rb->pb->out_stream;
}
return SVN_NO_ERROR;
}
static svn_error_t *
close_node(void *node_baton) {
struct node_baton_t *nb = node_baton;
apr_size_t len = 2;
if (nb->do_skip)
return SVN_NO_ERROR;
if (! nb->writing_begun)
SVN_ERR(output_node(nb));
SVN_ERR(svn_stream_write(nb->rb->pb->out_stream, "\n\n", &len));
return SVN_NO_ERROR;
}
static svn_error_t *
close_revision(void *revision_baton) {
struct revision_baton_t *rb = revision_baton;
if (! rb->writing_begun)
return output_revision(rb);
else
return SVN_NO_ERROR;
}
svn_repos_parser_fns2_t filtering_vtable = {
new_revision_record,
uuid_record,
new_node_record,
set_revision_property,
set_node_property,
NULL,
remove_node_props,
set_fulltext,
NULL,
close_node,
close_revision
};
static svn_opt_subcommand_t
subcommand_help,
subcommand_exclude,
subcommand_include;
enum {
svndumpfilter__drop_empty_revs = SVN_OPT_FIRST_LONGOPT_ID,
svndumpfilter__renumber_revs,
svndumpfilter__preserve_revprops,
svndumpfilter__skip_missing_merge_sources,
svndumpfilter__quiet,
svndumpfilter__version
};
static const apr_getopt_option_t options_table[] = {
{
"help", 'h', 0,
N_("show help on a subcommand")
},
{
NULL, '?', 0,
N_("show help on a subcommand")
},
{
"version", svndumpfilter__version, 0,
N_("show program version information")
},
{
"quiet", svndumpfilter__quiet, 0,
N_("Do not display filtering statistics.")
},
{
"drop-empty-revs", svndumpfilter__drop_empty_revs, 0,
N_("Remove revisions emptied by filtering.")
},
{
"renumber-revs", svndumpfilter__renumber_revs, 0,
N_("Renumber revisions left after filtering.")
},
{
"skip-missing-merge-sources",
svndumpfilter__skip_missing_merge_sources, 0,
N_("Skip missing merge sources.")
},
{
"preserve-revprops", svndumpfilter__preserve_revprops, 0,
N_("Don't filter revision properties.")
},
{NULL}
};
static const svn_opt_subcommand_desc_t cmd_table[] = {
{
"exclude", subcommand_exclude, {0},
N_("Filter out nodes with given prefixes from dumpstream.\n"
"usage: svndumpfilter exclude PATH_PREFIX...\n"),
{
svndumpfilter__drop_empty_revs, svndumpfilter__renumber_revs,
svndumpfilter__skip_missing_merge_sources,
svndumpfilter__preserve_revprops, svndumpfilter__quiet
}
},
{
"include", subcommand_include, {0},
N_("Filter out nodes without given prefixes from dumpstream.\n"
"usage: svndumpfilter include PATH_PREFIX...\n"),
{
svndumpfilter__drop_empty_revs, svndumpfilter__renumber_revs,
svndumpfilter__skip_missing_merge_sources,
svndumpfilter__preserve_revprops, svndumpfilter__quiet
}
},
{
"help", subcommand_help, {"?", "h"},
N_("Describe the usage of this program or its subcommands.\n"
"usage: svndumpfilter help [SUBCOMMAND...]\n"),
{0}
},
{ NULL, NULL, {0}, NULL, {0} }
};
struct svndumpfilter_opt_state {
svn_opt_revision_t start_revision;
svn_opt_revision_t end_revision;
svn_boolean_t quiet;
svn_boolean_t version;
svn_boolean_t drop_empty_revs;
svn_boolean_t help;
svn_boolean_t renumber_revs;
svn_boolean_t preserve_revprops;
svn_boolean_t skip_missing_merge_sources;
apr_array_header_t *prefixes;
};
static svn_error_t *
parse_baton_initialize(struct parse_baton_t **pb,
struct svndumpfilter_opt_state *opt_state,
svn_boolean_t do_exclude,
apr_pool_t *pool) {
struct parse_baton_t *baton = apr_palloc(pool, sizeof(*baton));
SVN_ERR(create_stdio_stream(&(baton->in_stream),
apr_file_open_stdin, pool));
SVN_ERR(create_stdio_stream(&(baton->out_stream),
apr_file_open_stdout, pool));
baton->do_exclude = do_exclude;
baton->do_renumber_revs = opt_state->renumber_revs;
baton->drop_empty_revs = opt_state->drop_empty_revs;
baton->preserve_revprops = opt_state->preserve_revprops;
baton->quiet = opt_state->quiet;
baton->prefixes = opt_state->prefixes;
baton->skip_missing_merge_sources = opt_state->skip_missing_merge_sources;
baton->rev_drop_count = 0;
baton->dropped_nodes = apr_hash_make(pool);
baton->renumber_history = apr_hash_make(pool);
baton->last_live_revision = SVN_INVALID_REVNUM;
SVN_ERR(svn_stream_printf(baton->out_stream, pool,
SVN_REPOS_DUMPFILE_MAGIC_HEADER ": %d\n\n",
2));
*pb = baton;
return SVN_NO_ERROR;
}
static svn_error_t *
subcommand_help(apr_getopt_t *os, void *baton, apr_pool_t *pool) {
struct svndumpfilter_opt_state *opt_state = baton;
const char *header =
_("general usage: svndumpfilter SUBCOMMAND [ARGS & OPTIONS ...]\n"
"Type 'svndumpfilter help <subcommand>' for help on a "
"specific subcommand.\n"
"Type 'svndumpfilter --version' to see the program version.\n"
"\n"
"Available subcommands:\n");
SVN_ERR(svn_opt_print_help(os, "svndumpfilter",
opt_state ? opt_state->version : FALSE,
FALSE, NULL,
header, cmd_table, options_table, NULL,
pool));
return SVN_NO_ERROR;
}
static svn_error_t *
check_lib_versions(void) {
static const svn_version_checklist_t checklist[] = {
{ "svn_subr", svn_subr_version },
{ "svn_repos", svn_repos_version },
{ "svn_delta", svn_delta_version },
{ NULL, NULL }
};
SVN_VERSION_DEFINE(my_version);
return svn_ver_check_list(&my_version, checklist);
}
static svn_error_t *
do_filter(apr_getopt_t *os,
void *baton,
svn_boolean_t do_exclude,
apr_pool_t *pool) {
struct svndumpfilter_opt_state *opt_state = baton;
struct parse_baton_t *pb;
apr_hash_index_t *hi;
apr_array_header_t *keys;
const void *key;
int i, num_keys;
if (! opt_state->quiet) {
apr_pool_t *subpool = svn_pool_create(pool);
SVN_ERR(svn_cmdline_fprintf(stderr, subpool,
do_exclude
? opt_state->drop_empty_revs
? _("Excluding (and dropping empty "
"revisions for) prefixes:\n")
: _("Excluding prefixes:\n")
: opt_state->drop_empty_revs
? _("Including (and dropping empty "
"revisions for) prefixes:\n")
: _("Including prefixes:\n")));
for (i = 0; i < opt_state->prefixes->nelts; i++) {
svn_pool_clear(subpool);
SVN_ERR(svn_cmdline_fprintf
(stderr, subpool, " '%s'\n",
APR_ARRAY_IDX(opt_state->prefixes, i, const char *)));
}
SVN_ERR(svn_cmdline_fputs("\n", stderr, subpool));
svn_pool_destroy(subpool);
}
SVN_ERR(parse_baton_initialize(&pb, opt_state, do_exclude, pool));
SVN_ERR(svn_repos_parse_dumpstream2(pb->in_stream, &filtering_vtable, pb,
NULL, NULL, pool));
if (opt_state->quiet)
return SVN_NO_ERROR;
SVN_ERR(svn_cmdline_fputs("\n", stderr, pool));
if (pb->rev_drop_count)
SVN_ERR(svn_cmdline_fprintf(stderr, pool,
_("Dropped %d revision(s).\n\n"),
pb->rev_drop_count));
if (pb->do_renumber_revs) {
apr_pool_t *subpool = svn_pool_create(pool);
SVN_ERR(svn_cmdline_fputs(_("Revisions renumbered as follows:\n"),
stderr, subpool));
num_keys = apr_hash_count(pb->renumber_history);
keys = apr_array_make(pool, num_keys + 1, sizeof(svn_revnum_t));
for (hi = apr_hash_first(pool, pb->renumber_history);
hi;
hi = apr_hash_next(hi)) {
apr_hash_this(hi, &key, NULL, NULL);
APR_ARRAY_PUSH(keys, svn_revnum_t) = *((const svn_revnum_t *) key);
}
qsort(keys->elts, keys->nelts,
keys->elt_size, svn_sort_compare_revisions);
for (i = 0; i < keys->nelts; i++) {
svn_revnum_t this_key;
struct revmap_t *this_val;
svn_pool_clear(subpool);
this_key = APR_ARRAY_IDX(keys, i, svn_revnum_t);
this_val = apr_hash_get(pb->renumber_history, &this_key,
sizeof(this_key));
if (this_val->was_dropped)
SVN_ERR(svn_cmdline_fprintf(stderr, subpool,
_(" %ld => (dropped)\n"),
this_key));
else
SVN_ERR(svn_cmdline_fprintf(stderr, subpool,
" %ld => %ld\n",
this_key, this_val->rev));
}
SVN_ERR(svn_cmdline_fputs("\n", stderr, subpool));
svn_pool_destroy(subpool);
}
if (apr_hash_count(pb->dropped_nodes)) {
apr_pool_t *subpool = svn_pool_create(pool);
SVN_ERR(svn_cmdline_fprintf(stderr, subpool,
_("Dropped %d node(s):\n"),
apr_hash_count(pb->dropped_nodes)));
num_keys = apr_hash_count(pb->dropped_nodes);
keys = apr_array_make(pool, num_keys + 1, sizeof(const char *));
for (hi = apr_hash_first(pool, pb->dropped_nodes);
hi;
hi = apr_hash_next(hi)) {
apr_hash_this(hi, &key, NULL, NULL);
APR_ARRAY_PUSH(keys, const char *) = key;
}
qsort(keys->elts, keys->nelts, keys->elt_size, svn_sort_compare_paths);
for (i = 0; i < keys->nelts; i++) {
svn_pool_clear(subpool);
SVN_ERR(svn_cmdline_fprintf
(stderr, subpool, " '%s'\n",
(const char *)APR_ARRAY_IDX(keys, i, const char *)));
}
SVN_ERR(svn_cmdline_fputs("\n", stderr, subpool));
svn_pool_destroy(subpool);
}
return SVN_NO_ERROR;
}
static svn_error_t *
subcommand_exclude(apr_getopt_t *os, void *baton, apr_pool_t *pool) {
return do_filter(os, baton, TRUE, pool);
}
static svn_error_t *
subcommand_include(apr_getopt_t *os, void *baton, apr_pool_t *pool) {
return do_filter(os, baton, FALSE, pool);
}
int
main(int argc, const char *argv[]) {
svn_error_t *err;
apr_status_t apr_err;
apr_allocator_t *allocator;
apr_pool_t *pool;
const svn_opt_subcommand_desc_t *subcommand = NULL;
struct svndumpfilter_opt_state opt_state;
apr_getopt_t *os;
int opt_id;
apr_array_header_t *received_opts;
int i;
if (svn_cmdline_init("svndumpfilter", stderr) != EXIT_SUCCESS)
return EXIT_FAILURE;
if (apr_allocator_create(&allocator))
return EXIT_FAILURE;
apr_allocator_max_free_set(allocator, SVN_ALLOCATOR_RECOMMENDED_MAX_FREE);
pool = svn_pool_create_ex(NULL, allocator);
apr_allocator_owner_set(allocator, pool);
err = check_lib_versions();
if (err)
return svn_cmdline_handle_exit_error(err, pool, "svndumpfilter: ");
received_opts = apr_array_make(pool, SVN_OPT_MAX_OPTIONS, sizeof(int));
err = svn_fs_initialize(pool);
if (err)
return svn_cmdline_handle_exit_error(err, pool, "svndumpfilter: ");
if (argc <= 1) {
subcommand_help(NULL, NULL, pool);
svn_pool_destroy(pool);
return EXIT_FAILURE;
}
memset(&opt_state, 0, sizeof(opt_state));
opt_state.start_revision.kind = svn_opt_revision_unspecified;
opt_state.end_revision.kind = svn_opt_revision_unspecified;
err = svn_cmdline__getopt_init(&os, argc, argv, pool);
if (err)
return svn_cmdline_handle_exit_error(err, pool, "svndumpfilter: ");
os->interleave = 1;
while (1) {
const char *opt_arg;
apr_err = apr_getopt_long(os, options_table, &opt_id, &opt_arg);
if (APR_STATUS_IS_EOF(apr_err))
break;
else if (apr_err) {
subcommand_help(NULL, NULL, pool);
svn_pool_destroy(pool);
return EXIT_FAILURE;
}
APR_ARRAY_PUSH(received_opts, int) = opt_id;
switch (opt_id) {
case 'h':
case '?':
opt_state.help = TRUE;
break;
case svndumpfilter__version:
opt_state.version = TRUE;
case svndumpfilter__quiet:
opt_state.quiet = TRUE;
break;
case svndumpfilter__drop_empty_revs:
opt_state.drop_empty_revs = TRUE;
break;
case svndumpfilter__renumber_revs:
opt_state.renumber_revs = TRUE;
break;
case svndumpfilter__preserve_revprops:
opt_state.preserve_revprops = TRUE;
break;
case svndumpfilter__skip_missing_merge_sources:
opt_state.skip_missing_merge_sources = TRUE;
break;
default: {
subcommand_help(NULL, NULL, pool);
svn_pool_destroy(pool);
return EXIT_FAILURE;
}
}
}
if (opt_state.help)
subcommand = svn_opt_get_canonical_subcommand(cmd_table, "help");
if (subcommand == NULL) {
if (os->ind >= os->argc) {
if (opt_state.version) {
static const svn_opt_subcommand_desc_t pseudo_cmd = {
"--version", subcommand_help, {0}, "",
{
svndumpfilter__version,
}
};
subcommand = &pseudo_cmd;
} else {
svn_error_clear(svn_cmdline_fprintf
(stderr, pool,
_("Subcommand argument required\n")));
subcommand_help(NULL, NULL, pool);
svn_pool_destroy(pool);
return EXIT_FAILURE;
}
} else {
const char *first_arg = os->argv[os->ind++];
subcommand = svn_opt_get_canonical_subcommand(cmd_table, first_arg);
if (subcommand == NULL) {
const char* first_arg_utf8;
if ((err = svn_utf_cstring_to_utf8(&first_arg_utf8, first_arg,
pool)))
return svn_cmdline_handle_exit_error(err, pool,
"svndumpfilter: ");
svn_error_clear(svn_cmdline_fprintf(stderr, pool,
_("Unknown command: '%s'\n"),
first_arg_utf8));
subcommand_help(NULL, NULL, pool);
svn_pool_destroy(pool);
return EXIT_FAILURE;
}
}
}
if (subcommand->cmd_func != subcommand_help) {
if (os->ind >= os->argc) {
svn_error_clear(svn_cmdline_fprintf
(stderr, pool,
_("\nError: no prefixes supplied.\n")));
svn_pool_destroy(pool);
return EXIT_FAILURE;
}
opt_state.prefixes = apr_array_make(pool, os->argc - os->ind,
sizeof(const char *));
for (i = os->ind ; i< os->argc; i++) {
const char *prefix;
SVN_INT_ERR(svn_utf_cstring_to_utf8(&prefix, os->argv[i], pool));
prefix = svn_path_internal_style(prefix, pool);
prefix = svn_path_join("/", prefix, pool);
APR_ARRAY_PUSH(opt_state.prefixes, const char *) = prefix;
}
}
for (i = 0; i < received_opts->nelts; i++) {
opt_id = APR_ARRAY_IDX(received_opts, i, int);
if (opt_id == 'h' || opt_id == '?')
continue;
if (! svn_opt_subcommand_takes_option(subcommand, opt_id)) {
const char *optstr;
const apr_getopt_option_t *badopt =
svn_opt_get_option_from_code(opt_id, options_table);
svn_opt_format_option(&optstr, badopt, FALSE, pool);
if (subcommand->name[0] == '-')
subcommand_help(NULL, NULL, pool);
else
svn_error_clear(svn_cmdline_fprintf
(stderr, pool,
_("Subcommand '%s' doesn't accept option '%s'\n"
"Type 'svndumpfilter help %s' for usage.\n"),
subcommand->name, optstr, subcommand->name));
svn_pool_destroy(pool);
return EXIT_FAILURE;
}
}
err = (*subcommand->cmd_func)(os, &opt_state, pool);
if (err) {
if (err->apr_err == SVN_ERR_CL_INSUFFICIENT_ARGS
|| err->apr_err == SVN_ERR_CL_ARG_PARSING_ERROR) {
err = svn_error_quick_wrap(err,
_("Try 'svndumpfilter help' for more "
"info"));
}
return svn_cmdline_handle_exit_error(err, pool, "svndumpfilter: ");
} else {
svn_pool_destroy(pool);
SVN_INT_ERR(svn_cmdline_fflush(stdout));
return EXIT_SUCCESS;
}
}