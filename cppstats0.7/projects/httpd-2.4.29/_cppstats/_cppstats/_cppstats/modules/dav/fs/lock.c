#include "apr.h"
#include "apr_strings.h"
#include "apr_file_io.h"
#include "apr_uuid.h"
#define APR_WANT_MEMFUNC
#include "apr_want.h"
#include "httpd.h"
#include "http_log.h"
#include "mod_dav.h"
#include "repos.h"
#define DAV_TRUE 1
#define DAV_FALSE 0
#define DAV_CREATE_LIST 23
#define DAV_APPEND_LIST 24
#define DAV_LOCK_DIRECT 1
#define DAV_LOCK_INDIRECT 2
#define DAV_TYPE_FNAME 11
static dav_error * dav_fs_remove_locknull_member(apr_pool_t *p,
const char *filename,
dav_buffer *pbuf);
struct dav_locktoken {
apr_uuid_t uuid;
};
#define dav_compare_locktoken(plt1, plt2) memcmp(&(plt1)->uuid, &(plt2)->uuid, sizeof((plt1)->uuid))
typedef struct dav_lock_discovery_fixed {
char scope;
char type;
int depth;
time_t timeout;
} dav_lock_discovery_fixed;
typedef struct dav_lock_discovery {
struct dav_lock_discovery_fixed f;
dav_locktoken *locktoken;
const char *owner;
const char *auth_user;
struct dav_lock_discovery *next;
} dav_lock_discovery;
typedef struct dav_lock_indirect {
dav_locktoken *locktoken;
apr_datum_t key;
struct dav_lock_indirect *next;
time_t timeout;
} dav_lock_indirect;
#define dav_size_direct(a) ( 1 + sizeof(dav_lock_discovery_fixed) + sizeof(apr_uuid_t) + ((a)->owner ? strlen((a)->owner) : 0) + ((a)->auth_user ? strlen((a)->auth_user) : 0) + 2)
#define dav_size_indirect(a) (1 + sizeof(apr_uuid_t) + sizeof(time_t) + sizeof((a)->key.dsize) + (a)->key.dsize)
struct dav_lockdb_private {
request_rec *r;
apr_pool_t *pool;
const char *lockdb_path;
int opened;
dav_db *db;
};
typedef struct {
dav_lockdb pub;
dav_lockdb_private priv;
} dav_lockdb_combined;
struct dav_lock_private {
apr_datum_t key;
};
typedef struct {
dav_lock pub;
dav_lock_private priv;
dav_locktoken token;
} dav_lock_combined;
extern const dav_hooks_locks dav_hooks_locks_fs;
static dav_lock *dav_fs_alloc_lock(dav_lockdb *lockdb, apr_datum_t key,
const dav_locktoken *locktoken) {
dav_lock_combined *comb;
comb = apr_pcalloc(lockdb->info->pool, sizeof(*comb));
comb->pub.rectype = DAV_LOCKREC_DIRECT;
comb->pub.info = &comb->priv;
comb->priv.key = key;
if (locktoken == NULL) {
comb->pub.locktoken = &comb->token;
apr_uuid_get(&comb->token.uuid);
} else {
comb->pub.locktoken = locktoken;
}
return &comb->pub;
}
static dav_error * dav_fs_parse_locktoken(
apr_pool_t *p,
const char *char_token,
dav_locktoken **locktoken_p) {
dav_locktoken *locktoken;
if (ap_strstr_c(char_token, "opaquelocktoken:") != char_token) {
return dav_new_error(p,
HTTP_BAD_REQUEST, DAV_ERR_LOCK_UNK_STATE_TOKEN, 0,
"The lock token uses an unknown State-token "
"format and could not be parsed.");
}
char_token += 16;
locktoken = apr_pcalloc(p, sizeof(*locktoken));
if (apr_uuid_parse(&locktoken->uuid, char_token)) {
return dav_new_error(p, HTTP_BAD_REQUEST, DAV_ERR_LOCK_PARSE_TOKEN, 0,
"The opaquelocktoken has an incorrect format "
"and could not be parsed.");
}
*locktoken_p = locktoken;
return NULL;
}
static const char *dav_fs_format_locktoken(
apr_pool_t *p,
const dav_locktoken *locktoken) {
char buf[APR_UUID_FORMATTED_LENGTH + 1];
apr_uuid_format(buf, &locktoken->uuid);
return apr_pstrcat(p, "opaquelocktoken:", buf, NULL);
}
static int dav_fs_compare_locktoken(
const dav_locktoken *lt1,
const dav_locktoken *lt2) {
return dav_compare_locktoken(lt1, lt2);
}
static dav_error * dav_fs_really_open_lockdb(dav_lockdb *lockdb) {
dav_error *err;
if (lockdb->info->opened)
return NULL;
err = dav_dbm_open_direct(lockdb->info->pool,
lockdb->info->lockdb_path,
lockdb->ro,
&lockdb->info->db);
if (err != NULL) {
return dav_push_error(lockdb->info->pool,
HTTP_INTERNAL_SERVER_ERROR,
DAV_ERR_LOCK_OPENDB,
"Could not open the lock database.",
err);
}
lockdb->info->opened = 1;
return NULL;
}
static dav_error * dav_fs_open_lockdb(request_rec *r, int ro, int force,
dav_lockdb **lockdb) {
dav_lockdb_combined *comb;
comb = apr_pcalloc(r->pool, sizeof(*comb));
comb->pub.hooks = &dav_hooks_locks_fs;
comb->pub.ro = ro;
comb->pub.info = &comb->priv;
comb->priv.r = r;
comb->priv.pool = r->pool;
comb->priv.lockdb_path = dav_get_lockdb_path(r);
if (comb->priv.lockdb_path == NULL) {
return dav_new_error(r->pool, HTTP_INTERNAL_SERVER_ERROR,
DAV_ERR_LOCK_NO_DB, 0,
"A lock database was not specified with the "
"DAVLockDB directive. One must be specified "
"to use the locking functionality.");
}
*lockdb = &comb->pub;
if (force) {
return dav_fs_really_open_lockdb(*lockdb);
}
return NULL;
}
static void dav_fs_close_lockdb(dav_lockdb *lockdb) {
if (lockdb->info->db != NULL)
dav_dbm_close(lockdb->info->db);
}
static apr_datum_t dav_fs_build_key(apr_pool_t *p,
const dav_resource *resource) {
const char *pathname = dav_fs_pathname(resource);
apr_datum_t key;
key.dsize = strlen(pathname) + 2;
key.dptr = apr_palloc(p, key.dsize);
*key.dptr = DAV_TYPE_FNAME;
memcpy(key.dptr + 1, pathname, key.dsize - 1);
if (key.dptr[key.dsize - 2] == '/')
key.dptr[--key.dsize - 1] = '\0';
return key;
}
static int dav_fs_lock_expired(time_t expires) {
return expires != DAV_TIMEOUT_INFINITE && time(NULL) >= expires;
}
static dav_error * dav_fs_save_lock_record(dav_lockdb *lockdb, apr_datum_t key,
dav_lock_discovery *direct,
dav_lock_indirect *indirect) {
dav_error *err;
apr_datum_t val = { 0 };
char *ptr;
dav_lock_discovery *dp = direct;
dav_lock_indirect *ip = indirect;
#if DAV_DEBUG
if (lockdb->ro) {
return dav_new_error(lockdb->info->pool,
HTTP_INTERNAL_SERVER_ERROR, 0, 0,
"INTERNAL DESIGN ERROR: the lockdb was opened "
"readonly, but an attempt to save locks was "
"performed.");
}
#endif
if ((err = dav_fs_really_open_lockdb(lockdb)) != NULL) {
return err;
}
if (dp == NULL && ip == NULL) {
(void) dav_dbm_delete(lockdb->info->db, key);
return NULL;
}
while(dp) {
val.dsize += dav_size_direct(dp);
dp = dp->next;
}
while(ip) {
val.dsize += dav_size_indirect(ip);
ip = ip->next;
}
ptr = val.dptr = apr_pcalloc(lockdb->info->pool, val.dsize);
dp = direct;
ip = indirect;
while(dp) {
*ptr++ = DAV_LOCK_DIRECT;
memcpy(ptr, dp, sizeof(dp->f));
ptr += sizeof(dp->f);
memcpy(ptr, dp->locktoken, sizeof(*dp->locktoken));
ptr += sizeof(*dp->locktoken);
if (dp->owner == NULL) {
*ptr++ = '\0';
} else {
memcpy(ptr, dp->owner, strlen(dp->owner) + 1);
ptr += strlen(dp->owner) + 1;
}
if (dp->auth_user == NULL) {
*ptr++ = '\0';
} else {
memcpy(ptr, dp->auth_user, strlen(dp->auth_user) + 1);
ptr += strlen(dp->auth_user) + 1;
}
dp = dp->next;
}
while(ip) {
*ptr++ = DAV_LOCK_INDIRECT;
memcpy(ptr, ip->locktoken, sizeof(*ip->locktoken));
ptr += sizeof(*ip->locktoken);
memcpy(ptr, &ip->timeout, sizeof(ip->timeout));
ptr += sizeof(ip->timeout);
memcpy(ptr, &ip->key.dsize, sizeof(ip->key.dsize));
ptr += sizeof(ip->key.dsize);
memcpy(ptr, ip->key.dptr, ip->key.dsize);
ptr += ip->key.dsize;
ip = ip->next;
}
if ((err = dav_dbm_store(lockdb->info->db, key, val)) != NULL) {
return dav_push_error(lockdb->info->pool,
HTTP_INTERNAL_SERVER_ERROR,
DAV_ERR_LOCK_SAVE_LOCK,
"Could not save lock information.",
err);
}
return NULL;
}
static dav_error * dav_fs_load_lock_record(dav_lockdb *lockdb, apr_datum_t key,
int add_method,
dav_lock_discovery **direct,
dav_lock_indirect **indirect) {
apr_pool_t *p = lockdb->info->pool;
dav_error *err;
apr_size_t offset = 0;
int need_save = DAV_FALSE;
apr_datum_t val = { 0 };
dav_lock_discovery *dp;
dav_lock_indirect *ip;
dav_buffer buf = { 0 };
if (add_method != DAV_APPEND_LIST) {
*direct = NULL;
*indirect = NULL;
}
if ((err = dav_fs_really_open_lockdb(lockdb)) != NULL) {
return err;
}
if (lockdb->info->db == NULL)
return NULL;
if ((err = dav_dbm_fetch(lockdb->info->db, key, &val)) != NULL)
return err;
if (!val.dsize)
return NULL;
while (offset < val.dsize) {
switch (*(val.dptr + offset++)) {
case DAV_LOCK_DIRECT:
dp = apr_pcalloc(p, sizeof(*dp));
memcpy(dp, val.dptr + offset, sizeof(dp->f));
offset += sizeof(dp->f);
dp->locktoken = apr_pmemdup(p, val.dptr + offset, sizeof(*dp->locktoken));
offset += sizeof(*dp->locktoken);
if (*(val.dptr + offset) == '\0') {
++offset;
} else {
dp->owner = apr_pstrdup(p, val.dptr + offset);
offset += strlen(dp->owner) + 1;
}
if (*(val.dptr + offset) == '\0') {
++offset;
} else {
dp->auth_user = apr_pstrdup(p, val.dptr + offset);
offset += strlen(dp->auth_user) + 1;
}
if (!dav_fs_lock_expired(dp->f.timeout)) {
dp->next = *direct;
*direct = dp;
} else {
need_save = DAV_TRUE;
if (*key.dptr == DAV_TYPE_FNAME) {
const char *fname = key.dptr + 1;
apr_finfo_t finfo;
apr_status_t rv;
rv = apr_stat(&finfo, fname, APR_FINFO_MIN | APR_FINFO_LINK, p);
if (rv != APR_SUCCESS && rv != APR_INCOMPLETE) {
if ((err = dav_fs_remove_locknull_member(p, fname, &buf)) != NULL) {
return err;
}
}
}
}
break;
case DAV_LOCK_INDIRECT:
ip = apr_pcalloc(p, sizeof(*ip));
ip->locktoken = apr_pmemdup(p, val.dptr + offset, sizeof(*ip->locktoken));
offset += sizeof(*ip->locktoken);
memcpy(&ip->timeout, val.dptr + offset, sizeof(ip->timeout));
offset += sizeof(ip->timeout);
memcpy(&ip->key.dsize, val.dptr + offset, sizeof(ip->key.dsize));
offset += sizeof(ip->key.dsize);
ip->key.dptr = apr_pmemdup(p, val.dptr + offset, ip->key.dsize);
offset += ip->key.dsize;
if (!dav_fs_lock_expired(ip->timeout)) {
ip->next = *indirect;
*indirect = ip;
} else {
need_save = DAV_TRUE;
}
break;
default:
dav_dbm_freedatum(lockdb->info->db, val);
--offset;
return dav_new_error(p,
HTTP_INTERNAL_SERVER_ERROR,
DAV_ERR_LOCK_CORRUPT_DB, 0,
apr_psprintf(p,
"The lock database was found to "
"be corrupt. offset %"
APR_SIZE_T_FMT ", c=%02x",
offset, val.dptr[offset]));
}
}
dav_dbm_freedatum(lockdb->info->db, val);
if (need_save == DAV_TRUE) {
return dav_fs_save_lock_record(lockdb, key, *direct, *indirect);
}
return NULL;
}
static dav_error * dav_fs_resolve(dav_lockdb *lockdb,
dav_lock_indirect *indirect,
dav_lock_discovery **direct,
dav_lock_discovery **ref_dp,
dav_lock_indirect **ref_ip) {
dav_error *err;
dav_lock_discovery *dir;
dav_lock_indirect *ind;
if ((err = dav_fs_load_lock_record(lockdb, indirect->key,
DAV_CREATE_LIST,
&dir, &ind)) != NULL) {
return err;
}
if (ref_dp != NULL) {
*ref_dp = dir;
*ref_ip = ind;
}
for (; dir != NULL; dir = dir->next) {
if (!dav_compare_locktoken(indirect->locktoken, dir->locktoken)) {
*direct = dir;
return NULL;
}
}
return dav_new_error(lockdb->info->pool,
HTTP_INTERNAL_SERVER_ERROR,
DAV_ERR_LOCK_CORRUPT_DB, 0,
"The lock database was found to be corrupt. "
"An indirect lock's direct lock could not "
"be found.");
}
static const char *dav_fs_get_supportedlock(const dav_resource *resource) {
static const char supported[] = DEBUG_CR
"<D:lockentry>" DEBUG_CR
"<D:lockscope><D:exclusive/></D:lockscope>" DEBUG_CR
"<D:locktype><D:write/></D:locktype>" DEBUG_CR
"</D:lockentry>" DEBUG_CR
"<D:lockentry>" DEBUG_CR
"<D:lockscope><D:shared/></D:lockscope>" DEBUG_CR
"<D:locktype><D:write/></D:locktype>" DEBUG_CR
"</D:lockentry>" DEBUG_CR;
return supported;
}
static dav_error * dav_fs_load_locknull_list(apr_pool_t *p, const char *dirpath,
dav_buffer *pbuf) {
apr_finfo_t finfo;
apr_file_t *file = NULL;
dav_error *err = NULL;
apr_size_t amt;
apr_status_t rv;
dav_buffer_init(p, pbuf, dirpath);
if (pbuf->buf[pbuf->cur_len - 1] == '/')
pbuf->buf[--pbuf->cur_len] = '\0';
dav_buffer_place(p, pbuf, "/" DAV_FS_STATE_DIR "/" DAV_FS_LOCK_NULL_FILE);
pbuf->cur_len = 0;
if (apr_file_open(&file, pbuf->buf, APR_READ | APR_BINARY, APR_OS_DEFAULT,
p) != APR_SUCCESS) {
return NULL;
}
rv = apr_file_info_get(&finfo, APR_FINFO_SIZE, file);
if (rv != APR_SUCCESS) {
err = dav_new_error(p, HTTP_INTERNAL_SERVER_ERROR, 0, rv,
apr_psprintf(p,
"Opened but could not stat file %s",
pbuf->buf));
goto loaderror;
}
if (finfo.size != (apr_size_t)finfo.size) {
err = dav_new_error(p, HTTP_INTERNAL_SERVER_ERROR, 0, 0,
apr_psprintf(p,
"Opened but rejected huge file %s",
pbuf->buf));
goto loaderror;
}
amt = (apr_size_t)finfo.size;
dav_set_bufsize(p, pbuf, amt);
if ((rv = apr_file_read(file, pbuf->buf, &amt)) != APR_SUCCESS
|| amt != finfo.size) {
err = dav_new_error(p, HTTP_INTERNAL_SERVER_ERROR, 0, rv,
apr_psprintf(p,
"Failure reading locknull file "
"for %s", dirpath));
pbuf->cur_len = 0;
goto loaderror;
}
loaderror:
apr_file_close(file);
return err;
}
static dav_error * dav_fs_save_locknull_list(apr_pool_t *p, const char *dirpath,
dav_buffer *pbuf) {
const char *pathname;
apr_file_t *file = NULL;
dav_error *err = NULL;
apr_size_t amt;
apr_status_t rv;
if (pbuf->buf == NULL)
return NULL;
dav_fs_ensure_state_dir(p, dirpath);
pathname = apr_pstrcat(p,
dirpath,
dirpath[strlen(dirpath) - 1] == '/' ? "" : "/",
DAV_FS_STATE_DIR "/" DAV_FS_LOCK_NULL_FILE,
NULL);
if (pbuf->cur_len == 0) {
if ((rv = apr_file_remove(pathname, p)) != APR_SUCCESS) {
return dav_new_error(p, HTTP_INTERNAL_SERVER_ERROR, 0, rv,
apr_psprintf(p,
"Error removing %s", pathname));
}
return NULL;
}
if ((rv = apr_file_open(&file, pathname,
APR_WRITE | APR_CREATE | APR_TRUNCATE | APR_BINARY,
APR_OS_DEFAULT, p)) != APR_SUCCESS) {
return dav_new_error(p, HTTP_INTERNAL_SERVER_ERROR, 0, rv,
apr_psprintf(p,
"Error opening %s for writing",
pathname));
}
amt = pbuf->cur_len;
if ((rv = apr_file_write_full(file, pbuf->buf, amt, &amt)) != APR_SUCCESS
|| amt != pbuf->cur_len) {
err = dav_new_error(p, HTTP_INTERNAL_SERVER_ERROR, 0, rv,
apr_psprintf(p,
"Error writing %" APR_SIZE_T_FMT
" bytes to %s",
pbuf->cur_len, pathname));
}
apr_file_close(file);
return err;
}
static dav_error * dav_fs_remove_locknull_member(apr_pool_t *p,
const char *filename,
dav_buffer *pbuf) {
dav_error *err;
apr_size_t len;
apr_size_t scanlen;
char *scan;
const char *scanend;
char *dirpath = apr_pstrdup(p, filename);
char *fname = strrchr(dirpath, '/');
int dirty = 0;
if (fname != NULL)
*fname++ = '\0';
else
fname = dirpath;
len = strlen(fname) + 1;
if ((err = dav_fs_load_locknull_list(p, dirpath, pbuf)) != NULL) {
return err;
}
for (scan = pbuf->buf, scanend = scan + pbuf->cur_len;
scan < scanend;
scan += scanlen) {
scanlen = strlen(scan) + 1;
if (len == scanlen && memcmp(fname, scan, scanlen) == 0) {
pbuf->cur_len -= scanlen;
memmove(scan, scan + scanlen, scanend - (scan + scanlen));
dirty = 1;
break;
}
}
if (dirty) {
if ((err = dav_fs_save_locknull_list(p, dirpath, pbuf)) != NULL) {
return err;
}
}
return NULL;
}
dav_error * dav_fs_get_locknull_members(
const dav_resource *resource,
dav_buffer *pbuf) {
const char *dirpath;
(void) dav_fs_dir_file_name(resource, &dirpath, NULL);
return dav_fs_load_locknull_list(dav_fs_pool(resource), dirpath, pbuf);
}
static dav_error * dav_fs_add_locknull_state(
dav_lockdb *lockdb,
const dav_resource *resource) {
dav_buffer buf = { 0 };
apr_pool_t *p = lockdb->info->pool;
const char *dirpath;
const char *fname;
dav_error *err;
(void) dav_fs_dir_file_name(resource, &dirpath, &fname);
if ((err = dav_fs_load_locknull_list(p, dirpath, &buf)) != NULL) {
return dav_push_error(p, HTTP_INTERNAL_SERVER_ERROR, 0,
"Could not load .locknull file.", err);
}
dav_buffer_append(p, &buf, fname);
buf.cur_len++;
if ((err = dav_fs_save_locknull_list(p, dirpath, &buf)) != NULL) {
return dav_push_error(p, HTTP_INTERNAL_SERVER_ERROR, 0,
"Could not save .locknull file.", err);
}
return NULL;
}
static dav_error * dav_fs_remove_locknull_state(
dav_lockdb *lockdb,
const dav_resource *resource) {
dav_buffer buf = { 0 };
dav_error *err;
apr_pool_t *p = lockdb->info->pool;
const char *pathname = dav_fs_pathname(resource);
if ((err = dav_fs_remove_locknull_member(p, pathname, &buf)) != NULL) {
return err;
}
return NULL;
}
static dav_error * dav_fs_create_lock(dav_lockdb *lockdb,
const dav_resource *resource,
dav_lock **lock) {
apr_datum_t key;
key = dav_fs_build_key(lockdb->info->pool, resource);
*lock = dav_fs_alloc_lock(lockdb,
key,
NULL);
(*lock)->is_locknull = !resource->exists;
return NULL;
}
static dav_error * dav_fs_get_locks(dav_lockdb *lockdb,
const dav_resource *resource,
int calltype,
dav_lock **locks) {
apr_pool_t *p = lockdb->info->pool;
apr_datum_t key;
dav_error *err;
dav_lock *lock = NULL;
dav_lock *newlock;
dav_lock_discovery *dp;
dav_lock_indirect *ip;
#if DAV_DEBUG
if (calltype == DAV_GETLOCKS_COMPLETE) {
return dav_new_error(lockdb->info->pool,
HTTP_INTERNAL_SERVER_ERROR, 0, 0,
"INTERNAL DESIGN ERROR: DAV_GETLOCKS_COMPLETE "
"is not yet supported");
}
#endif
key = dav_fs_build_key(p, resource);
if ((err = dav_fs_load_lock_record(lockdb, key, DAV_CREATE_LIST,
&dp, &ip)) != NULL) {
return err;
}
for (; dp != NULL; dp = dp->next) {
newlock = dav_fs_alloc_lock(lockdb, key, dp->locktoken);
newlock->is_locknull = !resource->exists;
newlock->scope = dp->f.scope;
newlock->type = dp->f.type;
newlock->depth = dp->f.depth;
newlock->timeout = dp->f.timeout;
newlock->owner = dp->owner;
newlock->auth_user = dp->auth_user;
newlock->next = lock;
lock = newlock;
}
for (; ip != NULL; ip = ip->next) {
newlock = dav_fs_alloc_lock(lockdb, ip->key, ip->locktoken);
newlock->is_locknull = !resource->exists;
if (calltype == DAV_GETLOCKS_RESOLVED) {
if ((err = dav_fs_resolve(lockdb, ip, &dp, NULL, NULL)) != NULL) {
return err;
}
newlock->scope = dp->f.scope;
newlock->type = dp->f.type;
newlock->depth = dp->f.depth;
newlock->timeout = dp->f.timeout;
newlock->owner = dp->owner;
newlock->auth_user = dp->auth_user;
} else {
newlock->rectype = DAV_LOCKREC_INDIRECT_PARTIAL;
}
newlock->next = lock;
lock = newlock;
}
*locks = lock;
return NULL;
}
static dav_error * dav_fs_find_lock(dav_lockdb *lockdb,
const dav_resource *resource,
const dav_locktoken *locktoken,
int partial_ok,
dav_lock **lock) {
dav_error *err;
apr_datum_t key;
dav_lock_discovery *dp;
dav_lock_indirect *ip;
*lock = NULL;
key = dav_fs_build_key(lockdb->info->pool, resource);
if ((err = dav_fs_load_lock_record(lockdb, key, DAV_CREATE_LIST,
&dp, &ip)) != NULL) {
return err;
}
for (; dp != NULL; dp = dp->next) {
if (!dav_compare_locktoken(locktoken, dp->locktoken)) {
*lock = dav_fs_alloc_lock(lockdb, key, locktoken);
(*lock)->is_locknull = !resource->exists;
(*lock)->scope = dp->f.scope;
(*lock)->type = dp->f.type;
(*lock)->depth = dp->f.depth;
(*lock)->timeout = dp->f.timeout;
(*lock)->owner = dp->owner;
(*lock)->auth_user = dp->auth_user;
return NULL;
}
}
for (; ip != NULL; ip = ip->next) {
if (!dav_compare_locktoken(locktoken, ip->locktoken)) {
*lock = dav_fs_alloc_lock(lockdb, ip->key, locktoken);
(*lock)->is_locknull = !resource->exists;
if (partial_ok) {
(*lock)->rectype = DAV_LOCKREC_INDIRECT_PARTIAL;
} else {
(*lock)->rectype = DAV_LOCKREC_INDIRECT;
if ((err = dav_fs_resolve(lockdb, ip, &dp,
NULL, NULL)) != NULL) {
return err;
}
(*lock)->scope = dp->f.scope;
(*lock)->type = dp->f.type;
(*lock)->depth = dp->f.depth;
(*lock)->timeout = dp->f.timeout;
(*lock)->owner = dp->owner;
(*lock)->auth_user = dp->auth_user;
}
return NULL;
}
}
return NULL;
}
static dav_error * dav_fs_has_locks(dav_lockdb *lockdb,
const dav_resource *resource,
int *locks_present) {
dav_error *err;
apr_datum_t key;
*locks_present = 0;
if ((err = dav_fs_really_open_lockdb(lockdb)) != NULL) {
return err;
}
if (lockdb->info->db == NULL)
return NULL;
key = dav_fs_build_key(lockdb->info->pool, resource);
*locks_present = dav_dbm_exists(lockdb->info->db, key);
return NULL;
}
static dav_error * dav_fs_append_locks(dav_lockdb *lockdb,
const dav_resource *resource,
int make_indirect,
const dav_lock *lock) {
apr_pool_t *p = lockdb->info->pool;
dav_error *err;
dav_lock_indirect *ip;
dav_lock_discovery *dp;
apr_datum_t key;
key = dav_fs_build_key(lockdb->info->pool, resource);
if ((err = dav_fs_load_lock_record(lockdb, key, 0, &dp, &ip)) != NULL) {
return err;
}
if (make_indirect) {
for (; lock != NULL; lock = lock->next) {
dav_lock_indirect *newi = apr_pcalloc(p, sizeof(*newi));
newi->locktoken = (dav_locktoken *)lock->locktoken;
newi->timeout = lock->timeout;
newi->key = lock->info->key;
newi->next = ip;
ip = newi;
}
} else {
for (; lock != NULL; lock = lock->next) {
if (lock->rectype == DAV_LOCKREC_DIRECT) {
dav_lock_discovery *newd = apr_pcalloc(p, sizeof(*newd));
newd->f.scope = lock->scope;
newd->f.type = lock->type;
newd->f.depth = lock->depth;
newd->f.timeout = lock->timeout;
newd->locktoken = (dav_locktoken *)lock->locktoken;
newd->owner = lock->owner;
newd->auth_user = lock->auth_user;
newd->next = dp;
dp = newd;
} else {
dav_lock_indirect *newi = apr_pcalloc(p, sizeof(*newi));
newi->locktoken = (dav_locktoken *)lock->locktoken;
newi->key = lock->info->key;
newi->next = ip;
ip = newi;
}
}
}
if ((err = dav_fs_save_lock_record(lockdb, key, dp, ip)) != NULL) {
return err;
}
if (!resource->exists
&& (err = dav_fs_add_locknull_state(lockdb, resource)) != NULL) {
return err;
}
return NULL;
}
static dav_error * dav_fs_remove_lock(dav_lockdb *lockdb,
const dav_resource *resource,
const dav_locktoken *locktoken) {
dav_error *err;
dav_buffer buf = { 0 };
dav_lock_discovery *dh = NULL;
dav_lock_indirect *ih = NULL;
apr_datum_t key;
key = dav_fs_build_key(lockdb->info->pool, resource);
if (locktoken != NULL) {
dav_lock_discovery *dp;
dav_lock_discovery *dprev = NULL;
dav_lock_indirect *ip;
dav_lock_indirect *iprev = NULL;
if ((err = dav_fs_load_lock_record(lockdb, key, DAV_CREATE_LIST,
&dh, &ih)) != NULL) {
return err;
}
for (dp = dh; dp != NULL; dp = dp->next) {
if (dav_compare_locktoken(locktoken, dp->locktoken) == 0) {
if (dprev)
dprev->next = dp->next;
else
dh = dh->next;
}
dprev = dp;
}
for (ip = ih; ip != NULL; ip = ip->next) {
if (dav_compare_locktoken(locktoken, ip->locktoken) == 0) {
if (iprev)
iprev->next = ip->next;
else
ih = ih->next;
}
iprev = ip;
}
}
if ((err = dav_fs_save_lock_record(lockdb, key, dh, ih)) != NULL) {
return err;
}
if (!resource->exists && dh == NULL && ih == NULL
&& (err = dav_fs_remove_locknull_member(lockdb->info->pool,
dav_fs_pathname(resource),
&buf)) != NULL) {
return err;
}
return NULL;
}
static int dav_fs_do_refresh(dav_lock_discovery *dp,
const dav_locktoken_list *ltl,
time_t new_time) {
int dirty = 0;
for (; ltl != NULL; ltl = ltl->next) {
if (dav_compare_locktoken(dp->locktoken, ltl->locktoken) == 0) {
dp->f.timeout = new_time;
dirty = 1;
break;
}
}
return dirty;
}
static dav_error * dav_fs_refresh_locks(dav_lockdb *lockdb,
const dav_resource *resource,
const dav_locktoken_list *ltl,
time_t new_time,
dav_lock **locks) {
dav_error *err;
apr_datum_t key;
dav_lock_discovery *dp;
dav_lock_discovery *dp_scan;
dav_lock_indirect *ip;
int dirty = 0;
dav_lock *newlock;
*locks = NULL;
key = dav_fs_build_key(lockdb->info->pool, resource);
if ((err = dav_fs_load_lock_record(lockdb, key, DAV_CREATE_LIST,
&dp, &ip)) != NULL) {
return err;
}
for (dp_scan = dp; dp_scan != NULL; dp_scan = dp_scan->next) {
if (dav_fs_do_refresh(dp_scan, ltl, new_time)) {
newlock = dav_fs_alloc_lock(lockdb, key, dp_scan->locktoken);
newlock->is_locknull = !resource->exists;
newlock->scope = dp_scan->f.scope;
newlock->type = dp_scan->f.type;
newlock->depth = dp_scan->f.depth;
newlock->timeout = dp_scan->f.timeout;
newlock->owner = dp_scan->owner;
newlock->auth_user = dp_scan->auth_user;
newlock->next = *locks;
*locks = newlock;
dirty = 1;
}
}
if (dirty
&& (err = dav_fs_save_lock_record(lockdb, key, dp, ip)) != NULL) {
return err;
}
for (; ip != NULL; ip = ip->next) {
dav_lock_discovery *ref_dp;
dav_lock_indirect *ref_ip;
if ((err = dav_fs_resolve(lockdb, ip, &dp_scan,
&ref_dp, &ref_ip)) != NULL) {
return err;
}
if (dav_fs_do_refresh(dp_scan, ltl, new_time)) {
newlock = dav_fs_alloc_lock(lockdb, ip->key, dp_scan->locktoken);
newlock->is_locknull = !resource->exists;
newlock->scope = dp_scan->f.scope;
newlock->type = dp_scan->f.type;
newlock->depth = dp_scan->f.depth;
newlock->timeout = dp_scan->f.timeout;
newlock->owner = dp_scan->owner;
newlock->auth_user = dp_scan->auth_user;
newlock->next = *locks;
*locks = newlock;
if ((err = dav_fs_save_lock_record(lockdb, ip->key, ref_dp,
ref_ip)) != NULL) {
return err;
}
}
}
return NULL;
}
const dav_hooks_locks dav_hooks_locks_fs = {
dav_fs_get_supportedlock,
dav_fs_parse_locktoken,
dav_fs_format_locktoken,
dav_fs_compare_locktoken,
dav_fs_open_lockdb,
dav_fs_close_lockdb,
dav_fs_remove_locknull_state,
dav_fs_create_lock,
dav_fs_get_locks,
dav_fs_find_lock,
dav_fs_has_locks,
dav_fs_append_locks,
dav_fs_remove_lock,
dav_fs_refresh_locks,
NULL,
NULL
};
