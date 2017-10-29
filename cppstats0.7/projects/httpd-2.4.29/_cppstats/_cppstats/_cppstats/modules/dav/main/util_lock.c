#include "apr.h"
#include "apr_strings.h"
#include "mod_dav.h"
#include "http_log.h"
#include "http_config.h"
#include "http_protocol.h"
#include "http_core.h"
APLOG_USE_MODULE(dav);
DAV_DECLARE(const char *) dav_lock_get_activelock(request_rec *r,
dav_lock *lock,
dav_buffer *pbuf) {
dav_lock *lock_scan;
const dav_hooks_locks *hooks = DAV_GET_HOOKS_LOCKS(r);
int count = 0;
dav_buffer work_buf = { 0 };
apr_pool_t *p = r->pool;
if (lock == NULL || hooks == NULL) {
return "";
}
for (lock_scan = lock; lock_scan != NULL; lock_scan = lock_scan->next)
count++;
if (pbuf == NULL)
pbuf = &work_buf;
pbuf->cur_len = 0;
dav_check_bufsize(p, pbuf, count * 300);
for (; lock != NULL; lock = lock->next) {
char tmp[100];
#if DAV_DEBUG
if (lock->rectype == DAV_LOCKREC_INDIRECT_PARTIAL) {
dav_buffer_append(p, pbuf,
"DESIGN ERROR: attempted to product an "
"activelock element from a partial, indirect "
"lock record. Creating an XML parsing error "
"to ease detection of this situation: <");
}
#endif
dav_buffer_append(p, pbuf, "<D:activelock>" DEBUG_CR "<D:locktype>");
switch (lock->type) {
case DAV_LOCKTYPE_WRITE:
dav_buffer_append(p, pbuf, "<D:write/>");
break;
default:
break;
}
dav_buffer_append(p, pbuf, "</D:locktype>" DEBUG_CR "<D:lockscope>");
switch (lock->scope) {
case DAV_LOCKSCOPE_EXCLUSIVE:
dav_buffer_append(p, pbuf, "<D:exclusive/>");
break;
case DAV_LOCKSCOPE_SHARED:
dav_buffer_append(p, pbuf, "<D:shared/>");
break;
default:
break;
}
dav_buffer_append(p, pbuf, "</D:lockscope>" DEBUG_CR);
apr_snprintf(tmp, sizeof(tmp), "<D:depth>%s</D:depth>" DEBUG_CR,
lock->depth == DAV_INFINITY ? "infinity" : "0");
dav_buffer_append(p, pbuf, tmp);
if (lock->owner) {
dav_buffer_append(p, pbuf, lock->owner);
}
dav_buffer_append(p, pbuf, "<D:timeout>");
if (lock->timeout == DAV_TIMEOUT_INFINITE) {
dav_buffer_append(p, pbuf, "Infinite");
} else {
time_t now = time(NULL);
if (now >= lock->timeout) {
dav_buffer_append(p, pbuf, "Second-0");
} else {
apr_snprintf(tmp, sizeof(tmp), "Second-%lu", (long unsigned int)(lock->timeout - now));
dav_buffer_append(p, pbuf, tmp);
}
}
dav_buffer_append(p, pbuf,
"</D:timeout>" DEBUG_CR
"<D:locktoken>" DEBUG_CR
"<D:href>");
dav_buffer_append(p, pbuf,
(*hooks->format_locktoken)(p, lock->locktoken));
dav_buffer_append(p, pbuf,
"</D:href>" DEBUG_CR
"</D:locktoken>" DEBUG_CR
"</D:activelock>" DEBUG_CR);
}
return pbuf->buf;
}
DAV_DECLARE(dav_error *) dav_lock_parse_lockinfo(request_rec *r,
const dav_resource *resource,
dav_lockdb *lockdb,
const apr_xml_doc *doc,
dav_lock **lock_request) {
apr_pool_t *p = r->pool;
dav_error *err;
apr_xml_elem *child;
dav_lock *lock;
if (!dav_validate_root(doc, "lockinfo")) {
return dav_new_error(p, HTTP_BAD_REQUEST, 0, 0,
"The request body contains an unexpected "
"XML root element.");
}
if ((err = (*lockdb->hooks->create_lock)(lockdb, resource,
&lock)) != NULL) {
return dav_push_error(p, err->status, 0,
"Could not parse the lockinfo due to an "
"internal problem creating a lock structure.",
err);
}
lock->depth = dav_get_depth(r, DAV_INFINITY);
if (lock->depth == -1) {
return dav_new_error(p, HTTP_BAD_REQUEST, 0, 0,
"An invalid Depth header was specified.");
}
lock->timeout = dav_get_timeout(r);
for (child = doc->root->first_child; child; child = child->next) {
if (strcmp(child->name, "locktype") == 0
&& child->first_child
&& lock->type == DAV_LOCKTYPE_UNKNOWN) {
if (strcmp(child->first_child->name, "write") == 0) {
lock->type = DAV_LOCKTYPE_WRITE;
continue;
}
}
if (strcmp(child->name, "lockscope") == 0
&& child->first_child
&& lock->scope == DAV_LOCKSCOPE_UNKNOWN) {
if (strcmp(child->first_child->name, "exclusive") == 0)
lock->scope = DAV_LOCKSCOPE_EXCLUSIVE;
else if (strcmp(child->first_child->name, "shared") == 0)
lock->scope = DAV_LOCKSCOPE_SHARED;
if (lock->scope != DAV_LOCKSCOPE_UNKNOWN)
continue;
}
if (strcmp(child->name, "owner") == 0 && lock->owner == NULL) {
const char *text;
apr_xml_quote_elem(p, child);
apr_xml_to_text(p, child, APR_XML_X2T_FULL_NS_LANG, doc->namespaces,
NULL, &text, NULL);
lock->owner = text;
continue;
}
return dav_new_error(p, HTTP_PRECONDITION_FAILED, 0, 0,
apr_psprintf(p,
"The server cannot satisfy the "
"LOCK request due to an unknown XML "
"element (\"%s\") within the "
"DAV:lockinfo element.",
child->name));
}
*lock_request = lock;
return NULL;
}
static dav_error * dav_lock_walker(dav_walk_resource *wres, int calltype) {
dav_walker_ctx *ctx = wres->walk_ctx;
dav_error *err;
if ((*wres->resource->hooks->is_same_resource)(wres->resource,
ctx->w.root))
return NULL;
if ((err = (*ctx->w.lockdb->hooks->append_locks)(ctx->w.lockdb,
wres->resource, 1,
ctx->lock)) != NULL) {
if (ap_is_HTTP_SERVER_ERROR(err->status)) {
return err;
}
dav_add_response(wres, err->status, NULL);
}
return NULL;
}
DAV_DECLARE(dav_error *) dav_add_lock(request_rec *r,
const dav_resource *resource,
dav_lockdb *lockdb, dav_lock *lock,
dav_response **response) {
dav_error *err;
int depth = lock->depth;
*response = NULL;
if (!resource->collection) {
depth = 0;
}
if ((err = (*lockdb->hooks->append_locks)(lockdb, resource, 0,
lock)) != NULL) {
return err;
}
if (depth > 0) {
dav_walker_ctx ctx = { { 0 } };
dav_response *multi_status;
ctx.w.walk_type = DAV_WALKTYPE_NORMAL | DAV_WALKTYPE_AUTH;
ctx.w.func = dav_lock_walker;
ctx.w.walk_ctx = &ctx;
ctx.w.pool = r->pool;
ctx.w.root = resource;
ctx.w.lockdb = lockdb;
ctx.r = r;
ctx.lock = lock;
err = (*resource->hooks->walk)(&ctx.w, DAV_INFINITY, &multi_status);
if (err != NULL) {
return err;
}
if (multi_status != NULL) {
*response = multi_status;
return dav_new_error(r->pool, HTTP_MULTI_STATUS, 0, 0,
"Error(s) occurred on resources during the "
"addition of a depth lock.");
}
}
return NULL;
}
DAV_DECLARE(dav_error*) dav_lock_query(dav_lockdb *lockdb,
const dav_resource *resource,
dav_lock **locks) {
if (lockdb == NULL) {
*locks = NULL;
return NULL;
}
return (*lockdb->hooks->get_locks)(lockdb, resource,
DAV_GETLOCKS_RESOLVED,
locks);
}
static dav_error * dav_unlock_walker(dav_walk_resource *wres, int calltype) {
dav_walker_ctx *ctx = wres->walk_ctx;
dav_error *err;
if (wres->resource->working) {
if ((err = dav_auto_checkin(ctx->r, (dav_resource *) wres->resource,
0 , 1 , NULL))
!= NULL) {
return err;
}
}
if ((err = (*ctx->w.lockdb->hooks->remove_lock)(ctx->w.lockdb,
wres->resource,
ctx->locktoken)) != NULL) {
return err;
}
return NULL;
}
static dav_error * dav_get_direct_resource(apr_pool_t *p,
dav_lockdb *lockdb,
const dav_locktoken *locktoken,
const dav_resource *resource,
const dav_resource **direct_resource) {
if (lockdb->hooks->lookup_resource != NULL) {
return (*lockdb->hooks->lookup_resource)(lockdb, locktoken,
resource, direct_resource);
}
*direct_resource = NULL;
while (resource != NULL) {
dav_error *err;
dav_lock *lock;
dav_resource *parent;
if ((err = (*lockdb->hooks->find_lock)(lockdb, resource, locktoken,
1, &lock)) != NULL) {
return err;
}
if (lock == NULL) {
return dav_new_error(p, HTTP_BAD_REQUEST, 0, 0,
"The specified locktoken does not correspond "
"to an existing lock on this resource.");
}
if (lock->rectype == DAV_LOCKREC_DIRECT) {
*direct_resource = resource;
return NULL;
}
if ((err = (*resource->hooks->get_parent_resource)(resource,
&parent)) != NULL) {
return err;
}
resource = parent;
}
return dav_new_error(p, HTTP_INTERNAL_SERVER_ERROR, 0, 0,
"The lock database is corrupt. A direct lock could "
"not be found for the corresponding indirect lock "
"on this resource.");
}
DAV_DECLARE(int) dav_unlock(request_rec *r, const dav_resource *resource,
const dav_locktoken *locktoken) {
int result;
dav_lockdb *lockdb;
const dav_resource *lock_resource = resource;
const dav_hooks_locks *hooks = DAV_GET_HOOKS_LOCKS(r);
const dav_hooks_repository *repos_hooks = resource->hooks;
dav_walker_ctx ctx = { { 0 } };
dav_response *multi_status;
dav_error *err;
if (hooks == NULL) {
return OK;
}
if ((err = (*hooks->open_lockdb)(r, 0, 1, &lockdb)) != NULL) {
return HTTP_INTERNAL_SERVER_ERROR;
}
if (locktoken != NULL
&& (err = dav_get_direct_resource(r->pool, lockdb,
locktoken, resource,
&lock_resource)) != NULL) {
return err->status;
}
ctx.w.walk_type = DAV_WALKTYPE_NORMAL | DAV_WALKTYPE_LOCKNULL;
ctx.w.func = dav_unlock_walker;
ctx.w.walk_ctx = &ctx;
ctx.w.pool = r->pool;
ctx.w.root = lock_resource;
ctx.w.lockdb = lockdb;
ctx.r = r;
ctx.locktoken = locktoken;
err = (*repos_hooks->walk)(&ctx.w, DAV_INFINITY, &multi_status);
result = err == NULL ? OK : err->status;
(*hooks->close_lockdb)(lockdb);
return result;
}
static dav_error * dav_inherit_walker(dav_walk_resource *wres, int calltype) {
dav_walker_ctx *ctx = wres->walk_ctx;
if (ctx->skip_root
&& (*wres->resource->hooks->is_same_resource)(wres->resource,
ctx->w.root)) {
return NULL;
}
return (*ctx->w.lockdb->hooks->append_locks)(ctx->w.lockdb,
wres->resource, 1,
ctx->lock);
}
static dav_error * dav_inherit_locks(request_rec *r, dav_lockdb *lockdb,
const dav_resource *resource,
int use_parent) {
dav_error *err;
const dav_resource *which_resource;
dav_lock *locks;
dav_lock *scan;
dav_lock *prev;
dav_walker_ctx ctx = { { 0 } };
const dav_hooks_repository *repos_hooks = resource->hooks;
dav_response *multi_status;
if (use_parent) {
dav_resource *parent;
if ((err = (*repos_hooks->get_parent_resource)(resource,
&parent)) != NULL) {
return err;
}
if (parent == NULL) {
return dav_new_error(r->pool, HTTP_INTERNAL_SERVER_ERROR, 0, 0,
"Could not fetch parent resource. Unable to "
"inherit locks from the parent and apply "
"them to this resource.");
}
which_resource = parent;
} else {
which_resource = resource;
}
if ((err = (*lockdb->hooks->get_locks)(lockdb, which_resource,
DAV_GETLOCKS_PARTIAL,
&locks)) != NULL) {
return err;
}
if (locks == NULL) {
return NULL;
}
for (scan = locks, prev = NULL;
scan != NULL;
prev = scan, scan = scan->next) {
if (scan->rectype == DAV_LOCKREC_DIRECT
&& scan->depth != DAV_INFINITY) {
if (prev == NULL)
locks = scan->next;
else
prev->next = scan->next;
}
}
ctx.w.walk_type = DAV_WALKTYPE_NORMAL | DAV_WALKTYPE_LOCKNULL;
ctx.w.func = dav_inherit_walker;
ctx.w.walk_ctx = &ctx;
ctx.w.pool = r->pool;
ctx.w.root = resource;
ctx.w.lockdb = lockdb;
ctx.r = r;
ctx.lock = locks;
ctx.skip_root = !use_parent;
return (*repos_hooks->walk)(&ctx.w, DAV_INFINITY, &multi_status);
}
DAV_DECLARE(int) dav_get_resource_state(request_rec *r,
const dav_resource *resource) {
const dav_hooks_locks *hooks = DAV_GET_HOOKS_LOCKS(r);
if (resource->exists)
return DAV_RESOURCE_EXISTS;
if (hooks != NULL) {
dav_error *err;
dav_lockdb *lockdb;
int locks_present;
if (r->path_info != NULL && *r->path_info != '\0') {
return DAV_RESOURCE_NULL;
}
if ((err = (*hooks->open_lockdb)(r, 1, 1, &lockdb)) == NULL) {
err = (*hooks->has_locks)(lockdb, resource, &locks_present);
(*hooks->close_lockdb)(lockdb);
}
if (err != NULL) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(00623)
"Failed to query lock-null status for %s",
r->filename);
return DAV_RESOURCE_ERROR;
}
if (locks_present)
return DAV_RESOURCE_LOCK_NULL;
}
return DAV_RESOURCE_NULL;
}
DAV_DECLARE(dav_error *) dav_notify_created(request_rec *r,
dav_lockdb *lockdb,
const dav_resource *resource,
int resource_state,
int depth) {
dav_error *err;
if (resource_state == DAV_RESOURCE_LOCK_NULL) {
(void)(*lockdb->hooks->remove_locknull_state)(lockdb, resource);
if (depth > 0 &&
(err = dav_inherit_locks(r, lockdb, resource, 0)) != NULL) {
return err;
}
} else if (resource_state == DAV_RESOURCE_NULL) {
if ((err = dav_inherit_locks(r, lockdb, resource, 1)) != NULL) {
err = dav_push_error(r->pool, err->status, 0,
"The resource was created successfully, but "
"there was a problem inheriting locks from "
"the parent resource.",
err);
return err;
}
}
return NULL;
}