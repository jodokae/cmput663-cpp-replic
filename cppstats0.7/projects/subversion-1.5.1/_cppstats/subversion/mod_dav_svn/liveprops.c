#include <apr_tables.h>
#include <apr_md5.h>
#include <httpd.h>
#include <http_core.h>
#include <util_xml.h>
#include <mod_dav.h>
#include "svn_pools.h"
#include "svn_time.h"
#include "svn_dav.h"
#include "svn_md5.h"
#include "svn_props.h"
#include "private/svn_dav_protocol.h"
#include "dav_svn.h"
static const char * const namespace_uris[] = {
"DAV:",
SVN_DAV_PROP_NS_DAV,
NULL
};
enum {
NAMESPACE_URI_DAV,
NAMESPACE_URI
};
#define SVN_RO_DAV_PROP(name) { NAMESPACE_URI_DAV, #name, DAV_PROPID_##name, 0 }
#define SVN_RW_DAV_PROP(name) { NAMESPACE_URI_DAV, #name, DAV_PROPID_##name, 1 }
#define SVN_RO_DAV_PROP2(sym,name) { NAMESPACE_URI_DAV, #name, DAV_PROPID_##sym, 0 }
#define SVN_RW_DAV_PROP2(sym,name) { NAMESPACE_URI_DAV, #name, DAV_PROPID_##sym, 1 }
#define SVN_RO_SVN_PROP(sym,name) { NAMESPACE_URI, #name, SVN_PROPID_##sym, 0 }
#define SVN_RW_SVN_PROP(sym,name) { NAMESPACE_URI, #name, SVN_PROPID_##sym, 1 }
enum {
SVN_PROPID_baseline_relative_path = 1,
SVN_PROPID_md5_checksum,
SVN_PROPID_repository_uuid,
SVN_PROPID_deadprop_count
};
static const dav_liveprop_spec props[] = {
#if 0
SVN_RO_DAV_PROP(getcontentlanguage),
#endif
SVN_RO_DAV_PROP(getcontentlength),
SVN_RO_DAV_PROP(getcontenttype),
SVN_RO_DAV_PROP(getetag),
SVN_RO_DAV_PROP(creationdate),
SVN_RO_DAV_PROP(getlastmodified),
SVN_RO_DAV_PROP2(baseline_collection, baseline-collection),
SVN_RO_DAV_PROP2(checked_in, checked-in),
SVN_RO_DAV_PROP2(version_controlled_configuration,
version-controlled-configuration),
SVN_RO_DAV_PROP2(version_name, version-name),
SVN_RO_DAV_PROP2(creator_displayname, creator-displayname),
SVN_RO_DAV_PROP2(auto_version, auto-version),
SVN_RO_SVN_PROP(baseline_relative_path, baseline-relative-path),
SVN_RO_SVN_PROP(md5_checksum, md5-checksum),
SVN_RO_SVN_PROP(repository_uuid, repository-uuid),
SVN_RO_SVN_PROP(deadprop_count, deadprop-count),
{ 0 }
};
static svn_error_t *
get_path_revprop(svn_string_t **propval,
const dav_resource *resource,
svn_revnum_t committed_rev,
const char *propname,
apr_pool_t *pool) {
*propval = NULL;
if (! dav_svn__allow_read(resource, committed_rev, pool))
return SVN_NO_ERROR;
return svn_repos_fs_revision_prop(propval,
resource->info->repos->repos,
committed_rev,
propname,
NULL, NULL, pool);
}
enum time_format {
time_format_iso8601,
time_format_rfc1123
};
static int
get_last_modified_time(const char **datestring,
apr_time_t *timeval,
const dav_resource *resource,
enum time_format format,
apr_pool_t *pool) {
svn_revnum_t committed_rev = SVN_INVALID_REVNUM;
svn_string_t *committed_date = NULL;
svn_error_t *serr;
apr_time_t timeval_tmp;
if ((datestring == NULL) && (timeval == NULL))
return 0;
if (resource->baselined && resource->type == DAV_RESOURCE_TYPE_VERSION) {
committed_rev = resource->info->root.rev;
} else if (resource->type == DAV_RESOURCE_TYPE_REGULAR
|| resource->type == DAV_RESOURCE_TYPE_WORKING
|| resource->type == DAV_RESOURCE_TYPE_VERSION) {
serr = svn_fs_node_created_rev(&committed_rev,
resource->info->root.root,
resource->info->repos_path, pool);
if (serr != NULL) {
svn_error_clear(serr);
return 1;
}
} else {
return 1;
}
serr = get_path_revprop(&committed_date,
resource,
committed_rev,
SVN_PROP_REVISION_DATE,
pool);
if (serr) {
svn_error_clear(serr);
return 1;
}
if (committed_date == NULL)
return 1;
serr = svn_time_from_cstring(&timeval_tmp, committed_date->data, pool);
if (serr != NULL) {
svn_error_clear(serr);
return 1;
}
if (timeval)
memcpy(timeval, &timeval_tmp, sizeof(*timeval));
if (! datestring)
return 0;
if (format == time_format_iso8601) {
*datestring = committed_date->data;
} else if (format == time_format_rfc1123) {
apr_time_exp_t tms;
apr_status_t status;
status = apr_time_exp_gmt(&tms, timeval_tmp);
if (status != APR_SUCCESS)
return 1;
*datestring = apr_psprintf(pool, "%s, %.2d %s %d %.2d:%.2d:%.2d GMT",
apr_day_snames[tms.tm_wday],
tms.tm_mday, apr_month_snames[tms.tm_mon],
tms.tm_year + 1900,
tms.tm_hour, tms.tm_min, tms.tm_sec);
} else {
return 1;
}
return 0;
}
static dav_prop_insert
insert_prop(const dav_resource *resource,
int propid,
dav_prop_insert what,
apr_text_header *phdr) {
const char *value = NULL;
const char *s;
apr_pool_t *response_pool = resource->pool;
apr_pool_t *p = resource->info->pool;
const dav_liveprop_spec *info;
int global_ns;
svn_error_t *serr;
if ((! resource->exists)
&& (propid != DAV_PROPID_version_controlled_configuration)
&& (propid != SVN_PROPID_baseline_relative_path))
return DAV_PROP_INSERT_NOTSUPP;
switch (propid) {
case DAV_PROPID_getlastmodified:
case DAV_PROPID_creationdate: {
const char *datestring;
apr_time_t timeval;
enum time_format format;
if (resource->type == DAV_RESOURCE_TYPE_PRIVATE
&& resource->info->restype == DAV_SVN_RESTYPE_VCC) {
return DAV_PROP_INSERT_NOTSUPP;
}
if (propid == DAV_PROPID_creationdate) {
format = time_format_iso8601;
} else {
format = time_format_rfc1123;
}
if (0 != get_last_modified_time(&datestring, &timeval,
resource, format, p)) {
return DAV_PROP_INSERT_NOTDEF;
}
value = apr_xml_quote_string(p, datestring, 1);
break;
}
case DAV_PROPID_creator_displayname: {
svn_revnum_t committed_rev = SVN_INVALID_REVNUM;
svn_string_t *last_author = NULL;
if (resource->type == DAV_RESOURCE_TYPE_PRIVATE
&& resource->info->restype == DAV_SVN_RESTYPE_VCC) {
return DAV_PROP_INSERT_NOTSUPP;
}
if (resource->baselined && resource->type == DAV_RESOURCE_TYPE_VERSION) {
committed_rev = resource->info->root.rev;
} else if (resource->type == DAV_RESOURCE_TYPE_REGULAR
|| resource->type == DAV_RESOURCE_TYPE_WORKING
|| resource->type == DAV_RESOURCE_TYPE_VERSION) {
serr = svn_fs_node_created_rev(&committed_rev,
resource->info->root.root,
resource->info->repos_path, p);
if (serr != NULL) {
svn_error_clear(serr);
value = "###error###";
break;
}
} else {
return DAV_PROP_INSERT_NOTSUPP;
}
serr = get_path_revprop(&last_author,
resource,
committed_rev,
SVN_PROP_REVISION_AUTHOR,
p);
if (serr) {
svn_error_clear(serr);
value = "###error###";
break;
}
if (last_author == NULL)
return DAV_PROP_INSERT_NOTDEF;
value = apr_xml_quote_string(p, last_author->data, 1);
break;
}
case DAV_PROPID_getcontentlanguage:
return DAV_PROP_INSERT_NOTSUPP;
break;
case DAV_PROPID_getcontentlength: {
svn_filesize_t len = 0;
if (resource->collection || resource->baselined)
return DAV_PROP_INSERT_NOTSUPP;
serr = svn_fs_file_length(&len, resource->info->root.root,
resource->info->repos_path, p);
if (serr != NULL) {
svn_error_clear(serr);
value = "0";
break;
}
value = apr_psprintf(p, "%" SVN_FILESIZE_T_FMT, len);
break;
}
case DAV_PROPID_getcontenttype: {
svn_string_t *pval;
const char *mime_type = NULL;
if (resource->baselined && resource->type == DAV_RESOURCE_TYPE_VERSION)
return DAV_PROP_INSERT_NOTSUPP;
if (resource->type == DAV_RESOURCE_TYPE_PRIVATE
&& resource->info->restype == DAV_SVN_RESTYPE_VCC) {
return DAV_PROP_INSERT_NOTSUPP;
}
if (resource->collection) {
if (resource->info->repos->xslt_uri)
mime_type = "text/xml";
else
mime_type = "text/html; charset=UTF-8";
} else {
if ((serr = svn_fs_node_prop(&pval, resource->info->root.root,
resource->info->repos_path,
SVN_PROP_MIME_TYPE, p))) {
svn_error_clear(serr);
pval = NULL;
}
if (pval)
mime_type = pval->data;
else if ((! resource->info->repos->is_svn_client)
&& resource->info->r->content_type)
mime_type = resource->info->r->content_type;
else
mime_type = ap_default_type(resource->info->r);
if ((serr = svn_mime_type_validate(mime_type, p))) {
svn_error_clear(serr);
return DAV_PROP_INSERT_NOTDEF;
}
}
value = mime_type;
break;
}
case DAV_PROPID_getetag:
if (resource->type == DAV_RESOURCE_TYPE_PRIVATE
&& resource->info->restype == DAV_SVN_RESTYPE_VCC) {
return DAV_PROP_INSERT_NOTSUPP;
}
value = dav_svn__getetag(resource, p);
break;
case DAV_PROPID_auto_version:
if (resource->info->repos->autoversioning)
value = "DAV:checkout-checkin";
else
return DAV_PROP_INSERT_NOTDEF;
break;
case DAV_PROPID_baseline_collection:
if (resource->type != DAV_RESOURCE_TYPE_VERSION || !resource->baselined)
return DAV_PROP_INSERT_NOTSUPP;
value = dav_svn__build_uri(resource->info->repos, DAV_SVN__BUILD_URI_BC,
resource->info->root.rev, NULL,
1 , p);
break;
case DAV_PROPID_checked_in:
if (resource->type == DAV_RESOURCE_TYPE_PRIVATE
&& resource->info->restype == DAV_SVN_RESTYPE_VCC) {
svn_revnum_t revnum;
serr = svn_fs_youngest_rev(&revnum, resource->info->repos->fs, p);
if (serr != NULL) {
svn_error_clear(serr);
value = "###error###";
break;
}
s = dav_svn__build_uri(resource->info->repos,
DAV_SVN__BUILD_URI_BASELINE,
revnum, NULL, 0 , p);
value = apr_psprintf(p, "<D:href>%s</D:href>",
apr_xml_quote_string(p, s, 1));
} else if (resource->type != DAV_RESOURCE_TYPE_REGULAR) {
return DAV_PROP_INSERT_NOTSUPP;
} else {
svn_revnum_t rev_to_use =
dav_svn__get_safe_cr(resource->info->root.root,
resource->info->repos_path, p);
s = dav_svn__build_uri(resource->info->repos,
DAV_SVN__BUILD_URI_VERSION,
rev_to_use, resource->info->repos_path,
0 , p);
value = apr_psprintf(p, "<D:href>%s</D:href>",
apr_xml_quote_string(p, s, 1));
}
break;
case DAV_PROPID_version_controlled_configuration:
if (resource->type != DAV_RESOURCE_TYPE_REGULAR)
return DAV_PROP_INSERT_NOTSUPP;
value = dav_svn__build_uri(resource->info->repos, DAV_SVN__BUILD_URI_VCC,
SVN_IGNORED_REVNUM, NULL,
1 , p);
break;
case DAV_PROPID_version_name:
if ((resource->type != DAV_RESOURCE_TYPE_VERSION)
&& (! resource->versioned))
return DAV_PROP_INSERT_NOTSUPP;
if (resource->type == DAV_RESOURCE_TYPE_PRIVATE
&& resource->info->restype == DAV_SVN_RESTYPE_VCC) {
return DAV_PROP_INSERT_NOTSUPP;
}
if (resource->baselined) {
value = apr_psprintf(p, "%ld",
resource->info->root.rev);
} else {
svn_revnum_t committed_rev = SVN_INVALID_REVNUM;
serr = svn_fs_node_created_rev(&committed_rev,
resource->info->root.root,
resource->info->repos_path, p);
if (serr != NULL) {
svn_error_clear(serr);
value = "###error###";
break;
}
s = apr_psprintf(p, "%ld", committed_rev);
value = apr_xml_quote_string(p, s, 1);
}
break;
case SVN_PROPID_baseline_relative_path:
if (resource->type != DAV_RESOURCE_TYPE_REGULAR)
return DAV_PROP_INSERT_NOTSUPP;
s = resource->info->repos_path + 1;
value = apr_xml_quote_string(p, s, 1);
break;
case SVN_PROPID_md5_checksum:
if ((! resource->collection)
&& (! resource->baselined)
&& (resource->type == DAV_RESOURCE_TYPE_REGULAR
|| resource->type == DAV_RESOURCE_TYPE_WORKING
|| resource->type == DAV_RESOURCE_TYPE_VERSION)) {
unsigned char digest[APR_MD5_DIGESTSIZE];
serr = svn_fs_file_md5_checksum(digest,
resource->info->root.root,
resource->info->repos_path, p);
if (serr != NULL) {
svn_error_clear(serr);
value = "###error###";
break;
}
value = svn_md5_digest_to_cstring(digest, p);
if (! value)
return DAV_PROP_INSERT_NOTSUPP;
} else
return DAV_PROP_INSERT_NOTSUPP;
break;
case SVN_PROPID_repository_uuid:
serr = svn_fs_get_uuid(resource->info->repos->fs, &value, p);
if (serr != NULL) {
svn_error_clear(serr);
value = "###error###";
break;
}
break;
case SVN_PROPID_deadprop_count: {
unsigned int propcount;
apr_hash_t *proplist;
if (resource->type != DAV_RESOURCE_TYPE_REGULAR)
return DAV_PROP_INSERT_NOTSUPP;
serr = svn_fs_node_proplist(&proplist,
resource->info->root.root,
resource->info->repos_path, p);
if (serr != NULL) {
svn_error_clear(serr);
value = "###error###";
break;
}
propcount = apr_hash_count(proplist);
value = apr_psprintf(p, "%u", propcount);
break;
}
default:
return DAV_PROP_INSERT_NOTDEF;
}
global_ns = dav_get_liveprop_info(propid, &dav_svn__liveprop_group, &info);
if (what == DAV_PROP_INSERT_NAME
|| (what == DAV_PROP_INSERT_VALUE && *value == '\0')) {
s = apr_psprintf(response_pool, "<lp%d:%s/>" DEBUG_CR, global_ns,
info->name);
} else if (what == DAV_PROP_INSERT_VALUE) {
s = apr_psprintf(response_pool, "<lp%d:%s>%s</lp%d:%s>" DEBUG_CR,
global_ns, info->name, value, global_ns, info->name);
} else {
s = apr_psprintf(response_pool,
"<D:supported-live-property D:name=\"%s\" "
"D:namespace=\"%s\"/>" DEBUG_CR,
info->name, namespace_uris[info->ns]);
}
apr_text_append(response_pool, phdr, s);
return what;
}
static int
is_writable(const dav_resource *resource, int propid) {
const dav_liveprop_spec *info;
(void) dav_get_liveprop_info(propid, &dav_svn__liveprop_group, &info);
return info->is_writable;
}
static dav_error *
patch_validate(const dav_resource *resource,
const apr_xml_elem *elem,
int operation,
void **context,
int *defer_to_dead) {
return NULL;
}
static dav_error *
patch_exec(const dav_resource *resource,
const apr_xml_elem *elem,
int operation,
void *context,
dav_liveprop_rollback **rollback_ctx) {
return NULL;
}
static void
patch_commit(const dav_resource *resource,
int operation,
void *context,
dav_liveprop_rollback *rollback_ctx) {
}
static dav_error *
patch_rollback(const dav_resource *resource,
int operation,
void *context,
dav_liveprop_rollback *rollback_ctx) {
return NULL;
}
static const dav_hooks_liveprop hooks_liveprop = {
insert_prop,
is_writable,
namespace_uris,
patch_validate,
patch_exec,
patch_commit,
patch_rollback,
};
const dav_liveprop_group dav_svn__liveprop_group = {
props,
namespace_uris,
&hooks_liveprop
};
void
dav_svn__gather_propsets(apr_array_header_t *uris) {
#if 0
*(const char **)apr_array_push(uris) =
"<http://subversion.tigris.org/dav/propset/svn/1>";
#endif
}
int
dav_svn__find_liveprop(const dav_resource *resource,
const char *ns_uri,
const char *name,
const dav_hooks_liveprop **hooks) {
if (resource->hooks != &dav_svn__hooks_repository)
return 0;
return dav_do_find_liveprop(ns_uri, name, &dav_svn__liveprop_group, hooks);
}
void
dav_svn__insert_all_liveprops(request_rec *r,
const dav_resource *resource,
dav_prop_insert what,
apr_text_header *phdr) {
const dav_liveprop_spec *spec;
apr_pool_t *pool;
apr_pool_t *subpool;
if (resource->hooks != &dav_svn__hooks_repository)
return;
if (!resource->exists) {
return;
}
pool = resource->info->pool;
subpool = svn_pool_create(pool);
resource->info->pool = subpool;
for (spec = props; spec->name != NULL; ++spec) {
svn_pool_clear(subpool);
(void) insert_prop(resource, spec->propid, what, phdr);
}
resource->info->pool = pool;
svn_pool_destroy(subpool);
}
