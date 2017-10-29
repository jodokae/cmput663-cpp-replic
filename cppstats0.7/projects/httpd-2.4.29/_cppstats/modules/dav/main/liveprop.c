#include "apr_pools.h"
#include "apr_hash.h"
#include "apr_errno.h"
#include "apr_strings.h"
#include "util_xml.h"
#include "mod_dav.h"
static apr_hash_t *dav_liveprop_uris = NULL;
static long dav_liveprop_count = 0;
static apr_status_t dav_cleanup_liveprops(void *ctx) {
dav_liveprop_uris = NULL;
dav_liveprop_count = 0;
return APR_SUCCESS;
}
static void dav_register_liveprop_namespace(apr_pool_t *p, const char *uri) {
long value;
if (dav_liveprop_uris == NULL) {
dav_liveprop_uris = apr_hash_make(p);
apr_pool_cleanup_register(p, NULL, dav_cleanup_liveprops, apr_pool_cleanup_null);
}
value = (long)apr_hash_get(dav_liveprop_uris, uri, APR_HASH_KEY_STRING);
if (value != 0) {
return;
}
apr_hash_set(dav_liveprop_uris, uri, APR_HASH_KEY_STRING,
(void *)++dav_liveprop_count);
}
DAV_DECLARE(long) dav_get_liveprop_ns_index(const char *uri) {
return (long)apr_hash_get(dav_liveprop_uris, uri, APR_HASH_KEY_STRING);
}
DAV_DECLARE(long) dav_get_liveprop_ns_count(void) {
return dav_liveprop_count;
}
DAV_DECLARE(void) dav_add_all_liveprop_xmlns(apr_pool_t *p,
apr_text_header *phdr) {
apr_hash_index_t *idx = apr_hash_first(p, dav_liveprop_uris);
for ( ; idx != NULL; idx = apr_hash_next(idx) ) {
const void *key;
void *val;
const char *s;
apr_hash_this(idx, &key, NULL, &val);
s = apr_psprintf(p, " xmlns:lp%ld=\"%s\"", (long)val, (const char *)key);
apr_text_append(p, phdr, s);
}
}
DAV_DECLARE(int) dav_do_find_liveprop(const char *ns_uri, const char *name,
const dav_liveprop_group *group,
const dav_hooks_liveprop **hooks) {
const char * const *uris = group->namespace_uris;
const dav_liveprop_spec *scan;
int ns;
for (ns = 0; uris[ns] != NULL; ++ns)
if (strcmp(ns_uri, uris[ns]) == 0)
break;
if (uris[ns] == NULL) {
return 0;
}
for (scan = group->specs; scan->name != NULL; ++scan)
if (ns == scan->ns && strcmp(name, scan->name) == 0) {
*hooks = group->hooks;
return scan->propid;
}
return 0;
}
DAV_DECLARE(long) dav_get_liveprop_info(int propid,
const dav_liveprop_group *group,
const dav_liveprop_spec **info) {
const dav_liveprop_spec *scan;
for (scan = group->specs; scan->name != NULL; ++scan) {
if (scan->propid == propid) {
*info = scan;
return dav_get_liveprop_ns_index(group->namespace_uris[scan->ns]);
}
}
*info = NULL;
return 0;
}
DAV_DECLARE(void) dav_register_liveprop_group(apr_pool_t *p,
const dav_liveprop_group *group) {
const char * const * uris = group->namespace_uris;
for ( ; *uris != NULL; ++uris) {
dav_register_liveprop_namespace(p, *uris);
}
}
