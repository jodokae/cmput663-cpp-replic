#include "httpd.h"
#include "http_log.h"
#include "http_request.h"
#include "http_protocol.h"
#include "http_config.h"
#include "mod_status.h"
#include "apr.h"
#include "apr_strings.h"
#include "apr_time.h"
#include "apr_shm.h"
#define APR_WANT_STRFUNC
#include "apr_want.h"
#include "apr_general.h"
#if APR_HAVE_LIMITS_H
#include <limits.h>
#endif
#include "ap_socache.h"
#define SHMCB_MAX_SIZE (UINT_MAX<APR_SIZE_MAX ? UINT_MAX : APR_SIZE_MAX)
#define DEFAULT_SHMCB_PREFIX "socache-shmcb-"
#define DEFAULT_SHMCB_SUFFIX ".cache"
#define ALIGNED_HEADER_SIZE APR_ALIGN_DEFAULT(sizeof(SHMCBHeader))
#define ALIGNED_SUBCACHE_SIZE APR_ALIGN_DEFAULT(sizeof(SHMCBSubcache))
#define ALIGNED_INDEX_SIZE APR_ALIGN_DEFAULT(sizeof(SHMCBIndex))
typedef struct {
unsigned long stat_stores;
unsigned long stat_replaced;
unsigned long stat_expiries;
unsigned long stat_scrolled;
unsigned long stat_retrieves_hit;
unsigned long stat_retrieves_miss;
unsigned long stat_removes_hit;
unsigned long stat_removes_miss;
unsigned int subcache_num;
unsigned int index_num;
unsigned int subcache_size;
unsigned int subcache_data_offset;
unsigned int subcache_data_size;
} SHMCBHeader;
typedef struct {
unsigned int idx_pos, idx_used;
unsigned int data_pos, data_used;
} SHMCBSubcache;
typedef struct {
apr_time_t expires;
unsigned int data_pos;
unsigned int data_used;
unsigned int id_len;
unsigned char removed;
} SHMCBIndex;
struct ap_socache_instance_t {
const char *data_file;
apr_size_t shm_size;
apr_shm_t *shm;
SHMCBHeader *header;
};
#define SHMCB_SUBCACHE(pHeader, num) (SHMCBSubcache *)(((unsigned char *)(pHeader)) + ALIGNED_HEADER_SIZE + (num) * ((pHeader)->subcache_size))
#define SHMCB_MASK(pHeader, id) SHMCB_SUBCACHE((pHeader), *(id) & ((pHeader)->subcache_num - 1))
#define SHMCB_MASK_DBG(pHeader, id) *(id), (*(id) & ((pHeader)->subcache_num - 1))
#define SHMCB_INDEX(pSubcache, num) (SHMCBIndex *)(((unsigned char *)pSubcache) + ALIGNED_SUBCACHE_SIZE + (num) * ALIGNED_INDEX_SIZE)
#define SHMCB_DATA(pHeader, pSubcache) ((unsigned char *)(pSubcache) + (pHeader)->subcache_data_offset)
#define SHMCB_CYCLIC_INCREMENT(val,inc,mod) (((val) + (inc)) % (mod))
#define SHMCB_CYCLIC_SPACE(val1,val2,mod) ((val2) >= (val1) ? ((val2) - (val1)) : ((val2) + (mod) - (val1)))
static void shmcb_cyclic_ntoc_memcpy(unsigned int buf_size, unsigned char *data,
unsigned int dest_offset, const unsigned char *src,
unsigned int src_len) {
if (dest_offset + src_len < buf_size)
memcpy(data + dest_offset, src, src_len);
else {
memcpy(data + dest_offset, src, buf_size - dest_offset);
memcpy(data, src + buf_size - dest_offset,
src_len + dest_offset - buf_size);
}
}
static void shmcb_cyclic_cton_memcpy(unsigned int buf_size, unsigned char *dest,
const unsigned char *data, unsigned int src_offset,
unsigned int src_len) {
if (src_offset + src_len < buf_size)
memcpy(dest, data + src_offset, src_len);
else {
memcpy(dest, data + src_offset, buf_size - src_offset);
memcpy(dest + buf_size - src_offset, data,
src_len + src_offset - buf_size);
}
}
static int shmcb_cyclic_memcmp(unsigned int buf_size, unsigned char *data,
unsigned int dest_offset,
const unsigned char *src,
unsigned int src_len) {
if (dest_offset + src_len < buf_size)
return memcmp(data + dest_offset, src, src_len);
else {
int diff;
diff = memcmp(data + dest_offset, src, buf_size - dest_offset);
if (diff) {
return diff;
}
return memcmp(data, src + buf_size - dest_offset,
src_len + dest_offset - buf_size);
}
}
static void shmcb_subcache_expire(server_rec *, SHMCBHeader *, SHMCBSubcache *,
apr_time_t);
static int shmcb_subcache_store(server_rec *s, SHMCBHeader *header,
SHMCBSubcache *subcache,
unsigned char *data, unsigned int data_len,
const unsigned char *id, unsigned int id_len,
apr_time_t expiry);
static int shmcb_subcache_retrieve(server_rec *, SHMCBHeader *, SHMCBSubcache *,
const unsigned char *id, unsigned int idlen,
unsigned char *data, unsigned int *datalen);
static int shmcb_subcache_remove(server_rec *, SHMCBHeader *, SHMCBSubcache *,
const unsigned char *, unsigned int);
static apr_status_t shmcb_subcache_iterate(ap_socache_instance_t *instance,
server_rec *s,
void *userctx,
SHMCBHeader *header,
SHMCBSubcache *subcache,
ap_socache_iterator_t *iterator,
unsigned char **buf,
apr_size_t *buf_len,
apr_pool_t *pool,
apr_time_t now);
static const char *socache_shmcb_create(ap_socache_instance_t **context,
const char *arg,
apr_pool_t *tmp, apr_pool_t *p) {
ap_socache_instance_t *ctx;
char *path, *cp, *cp2;
*context = ctx = apr_pcalloc(p, sizeof *ctx);
ctx->shm_size = 1024*512;
if (!arg || *arg == '\0') {
return NULL;
}
ctx->data_file = path = ap_server_root_relative(p, arg);
cp = strrchr(path, '(');
cp2 = path + strlen(path) - 1;
if (cp) {
char *endptr;
if (*cp2 != ')') {
return "Invalid argument: no closing parenthesis or cache size "
"missing after pathname with parenthesis";
}
*cp++ = '\0';
*cp2 = '\0';
ctx->shm_size = strtol(cp, &endptr, 10);
if (endptr != cp2) {
return "Invalid argument: cache size not numerical";
}
if (ctx->shm_size < 8192) {
return "Invalid argument: size has to be >= 8192 bytes";
}
if (ctx->shm_size >= SHMCB_MAX_SIZE) {
return apr_psprintf(tmp, "Invalid argument: size has "
"to be < %" APR_SIZE_T_FMT " bytes on this platform",
SHMCB_MAX_SIZE);
}
} else if (cp2 >= path && *cp2 == ')') {
return "Invalid argument: no opening parenthesis";
}
return NULL;
}
static apr_status_t socache_shmcb_init(ap_socache_instance_t *ctx,
const char *namespace,
const struct ap_socache_hints *hints,
server_rec *s, apr_pool_t *p) {
void *shm_segment;
apr_size_t shm_segsize;
apr_status_t rv;
SHMCBHeader *header;
unsigned int num_subcache, num_idx, loop;
apr_size_t avg_obj_size, avg_id_len;
if (ctx->data_file == NULL) {
const char *path = apr_pstrcat(p, DEFAULT_SHMCB_PREFIX, namespace,
DEFAULT_SHMCB_SUFFIX, NULL);
ctx->data_file = ap_runtime_dir_relative(p, path);
}
rv = apr_shm_create(&ctx->shm, ctx->shm_size, NULL, p);
if (APR_STATUS_IS_ENOTIMPL(rv)) {
if (ctx->data_file == NULL) {
ap_log_error(APLOG_MARK, APLOG_ERR, 0, s, APLOGNO(00818)
"Could not use default path '%s' for shmcb socache",
ctx->data_file);
return APR_EINVAL;
}
apr_shm_remove(ctx->data_file, p);
rv = apr_shm_create(&ctx->shm, ctx->shm_size, ctx->data_file, p);
}
if (rv != APR_SUCCESS) {
ap_log_error(APLOG_MARK, APLOG_ERR, rv, s, APLOGNO(00819)
"Could not allocate shared memory segment for shmcb "
"socache");
return rv;
}
shm_segment = apr_shm_baseaddr_get(ctx->shm);
shm_segsize = apr_shm_size_get(ctx->shm);
if (shm_segsize < (5 * ALIGNED_HEADER_SIZE)) {
ap_log_error(APLOG_MARK, APLOG_ERR, 0, s, APLOGNO(00820)
"shared memory segment too small");
return APR_ENOSPC;
}
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s, APLOGNO(00821)
"shmcb_init allocated %" APR_SIZE_T_FMT
" bytes of shared memory",
shm_segsize);
shm_segsize -= ALIGNED_HEADER_SIZE;
avg_obj_size = hints && hints->avg_obj_size ? hints->avg_obj_size : 150;
avg_id_len = hints && hints->avg_id_len ? hints->avg_id_len : 30;
num_idx = (shm_segsize) / (avg_obj_size + avg_id_len);
num_subcache = 256;
while ((num_idx / num_subcache) < (2 * num_subcache))
num_subcache /= 2;
num_idx /= num_subcache;
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s, APLOGNO(00822)
"for %" APR_SIZE_T_FMT " bytes (%" APR_SIZE_T_FMT
" including header), recommending %u subcaches, "
"%u indexes each", shm_segsize,
shm_segsize + ALIGNED_HEADER_SIZE,
num_subcache, num_idx);
if (num_idx < 5) {
ap_log_error(APLOG_MARK, APLOG_ERR, 0, s, APLOGNO(00823)
"shared memory segment too small");
return APR_ENOSPC;
}
ctx->header = header = shm_segment;
header->stat_stores = 0;
header->stat_replaced = 0;
header->stat_expiries = 0;
header->stat_scrolled = 0;
header->stat_retrieves_hit = 0;
header->stat_retrieves_miss = 0;
header->stat_removes_hit = 0;
header->stat_removes_miss = 0;
header->subcache_num = num_subcache;
header->subcache_size = (size_t)(shm_segsize / num_subcache);
if (header->subcache_size != APR_ALIGN_DEFAULT(header->subcache_size)) {
header->subcache_size = APR_ALIGN_DEFAULT(header->subcache_size) -
APR_ALIGN_DEFAULT(1);
}
header->subcache_data_offset = ALIGNED_SUBCACHE_SIZE +
num_idx * ALIGNED_INDEX_SIZE;
header->subcache_data_size = header->subcache_size -
header->subcache_data_offset;
header->index_num = num_idx;
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s, APLOGNO(00824)
"shmcb_init_memory choices follow");
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s, APLOGNO(00825)
"subcache_num = %u", header->subcache_num);
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s, APLOGNO(00826)
"subcache_size = %u", header->subcache_size);
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s, APLOGNO(00827)
"subcache_data_offset = %u", header->subcache_data_offset);
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s, APLOGNO(00828)
"subcache_data_size = %u", header->subcache_data_size);
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s, APLOGNO(00829)
"index_num = %u", header->index_num);
for (loop = 0; loop < header->subcache_num; loop++) {
SHMCBSubcache *subcache = SHMCB_SUBCACHE(header, loop);
subcache->idx_pos = subcache->idx_used = 0;
subcache->data_pos = subcache->data_used = 0;
}
ap_log_error(APLOG_MARK, APLOG_INFO, 0, s, APLOGNO(00830)
"Shared memory socache initialised");
return APR_SUCCESS;
}
static void socache_shmcb_destroy(ap_socache_instance_t *ctx, server_rec *s) {
if (ctx && ctx->shm) {
apr_shm_destroy(ctx->shm);
ctx->shm = NULL;
}
}
static apr_status_t socache_shmcb_store(ap_socache_instance_t *ctx,
server_rec *s, const unsigned char *id,
unsigned int idlen, apr_time_t expiry,
unsigned char *encoded,
unsigned int len_encoded,
apr_pool_t *p) {
SHMCBHeader *header = ctx->header;
SHMCBSubcache *subcache = SHMCB_MASK(header, id);
int tryreplace;
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s, APLOGNO(00831)
"socache_shmcb_store (0x%02x -> subcache %d)",
SHMCB_MASK_DBG(header, id));
if (idlen < 4) {
ap_log_error(APLOG_MARK, APLOG_ERR, 0, s, APLOGNO(00832) "unusably short id provided "
"(%u bytes)", idlen);
return APR_EINVAL;
}
tryreplace = shmcb_subcache_remove(s, header, subcache, id, idlen);
if (shmcb_subcache_store(s, header, subcache, encoded,
len_encoded, id, idlen, expiry)) {
ap_log_error(APLOG_MARK, APLOG_ERR, 0, s, APLOGNO(00833)
"can't store an socache entry!");
return APR_ENOSPC;
}
if (tryreplace == 0) {
header->stat_replaced++;
} else {
header->stat_stores++;
}
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s, APLOGNO(00834)
"leaving socache_shmcb_store successfully");
return APR_SUCCESS;
}
static apr_status_t socache_shmcb_retrieve(ap_socache_instance_t *ctx,
server_rec *s,
const unsigned char *id, unsigned int idlen,
unsigned char *dest, unsigned int *destlen,
apr_pool_t *p) {
SHMCBHeader *header = ctx->header;
SHMCBSubcache *subcache = SHMCB_MASK(header, id);
int rv;
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s, APLOGNO(00835)
"socache_shmcb_retrieve (0x%02x -> subcache %d)",
SHMCB_MASK_DBG(header, id));
rv = shmcb_subcache_retrieve(s, header, subcache, id, idlen,
dest, destlen);
if (rv == 0)
header->stat_retrieves_hit++;
else
header->stat_retrieves_miss++;
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s, APLOGNO(00836)
"leaving socache_shmcb_retrieve successfully");
return rv == 0 ? APR_SUCCESS : APR_NOTFOUND;
}
static apr_status_t socache_shmcb_remove(ap_socache_instance_t *ctx,
server_rec *s, const unsigned char *id,
unsigned int idlen, apr_pool_t *p) {
SHMCBHeader *header = ctx->header;
SHMCBSubcache *subcache = SHMCB_MASK(header, id);
apr_status_t rv;
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s, APLOGNO(00837)
"socache_shmcb_remove (0x%02x -> subcache %d)",
SHMCB_MASK_DBG(header, id));
if (idlen < 4) {
ap_log_error(APLOG_MARK, APLOG_ERR, 0, s, APLOGNO(00838) "unusably short id provided "
"(%u bytes)", idlen);
return APR_EINVAL;
}
if (shmcb_subcache_remove(s, header, subcache, id, idlen) == 0) {
header->stat_removes_hit++;
rv = APR_SUCCESS;
} else {
header->stat_removes_miss++;
rv = APR_NOTFOUND;
}
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s, APLOGNO(00839)
"leaving socache_shmcb_remove successfully");
return rv;
}
static void socache_shmcb_status(ap_socache_instance_t *ctx,
request_rec *r, int flags) {
server_rec *s = r->server;
SHMCBHeader *header = ctx->header;
unsigned int loop, total = 0, cache_total = 0, non_empty_subcaches = 0;
apr_time_t idx_expiry, min_expiry = 0, max_expiry = 0;
apr_time_t now = apr_time_now();
double expiry_total = 0;
int index_pct, cache_pct;
AP_DEBUG_ASSERT(header->subcache_num > 0);
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(00840) "inside shmcb_status");
for (loop = 0; loop < header->subcache_num; loop++) {
SHMCBSubcache *subcache = SHMCB_SUBCACHE(header, loop);
shmcb_subcache_expire(s, header, subcache, now);
total += subcache->idx_used;
cache_total += subcache->data_used;
if (subcache->idx_used) {
SHMCBIndex *idx = SHMCB_INDEX(subcache, subcache->idx_pos);
non_empty_subcaches++;
idx_expiry = idx->expires;
expiry_total += (double)idx_expiry;
max_expiry = ((idx_expiry > max_expiry) ? idx_expiry : max_expiry);
if (!min_expiry)
min_expiry = idx_expiry;
else
min_expiry = ((idx_expiry < min_expiry) ? idx_expiry : min_expiry);
}
}
index_pct = (100 * total) / (header->index_num *
header->subcache_num);
cache_pct = (100 * cache_total) / (header->subcache_data_size *
header->subcache_num);
if (!(flags & AP_STATUS_SHORT)) {
ap_rprintf(r, "cache type: <b>SHMCB</b>, shared memory: <b>%" APR_SIZE_T_FMT "</b> "
"bytes, current entries: <b>%d</b><br>",
ctx->shm_size, total);
ap_rprintf(r, "subcaches: <b>%d</b>, indexes per subcache: <b>%d</b><br>",
header->subcache_num, header->index_num);
if (non_empty_subcaches) {
apr_time_t average_expiry = (apr_time_t)(expiry_total / (double)non_empty_subcaches);
ap_rprintf(r, "time left on oldest entries' objects: ");
if (now < average_expiry)
ap_rprintf(r, "avg: <b>%d</b> seconds, (range: %d...%d)<br>",
(int)apr_time_sec(average_expiry - now),
(int)apr_time_sec(min_expiry - now),
(int)apr_time_sec(max_expiry - now));
else
ap_rprintf(r, "expiry_threshold: <b>Calculation error!</b><br>");
}
ap_rprintf(r, "index usage: <b>%d%%</b>, cache usage: <b>%d%%</b><br>",
index_pct, cache_pct);
ap_rprintf(r, "total entries stored since starting: <b>%lu</b><br>",
header->stat_stores);
ap_rprintf(r, "total entries replaced since starting: <b>%lu</b><br>",
header->stat_replaced);
ap_rprintf(r, "total entries expired since starting: <b>%lu</b><br>",
header->stat_expiries);
ap_rprintf(r, "total (pre-expiry) entries scrolled out of the cache: "
"<b>%lu</b><br>", header->stat_scrolled);
ap_rprintf(r, "total retrieves since starting: <b>%lu</b> hit, "
"<b>%lu</b> miss<br>", header->stat_retrieves_hit,
header->stat_retrieves_miss);
ap_rprintf(r, "total removes since starting: <b>%lu</b> hit, "
"<b>%lu</b> miss<br>", header->stat_removes_hit,
header->stat_removes_miss);
} else {
ap_rputs("CacheType: SHMCB\n", r);
ap_rprintf(r, "CacheSharedMemory: %" APR_SIZE_T_FMT "\n",
ctx->shm_size);
ap_rprintf(r, "CacheCurrentEntries: %d\n", total);
ap_rprintf(r, "CacheSubcaches: %d\n", header->subcache_num);
ap_rprintf(r, "CacheIndexesPerSubcaches: %d\n", header->index_num);
if (non_empty_subcaches) {
apr_time_t average_expiry = (apr_time_t)(expiry_total / (double)non_empty_subcaches);
if (now < average_expiry) {
ap_rprintf(r, "CacheTimeLeftOldestAvg: %d\n", (int)apr_time_sec(average_expiry - now));
ap_rprintf(r, "CacheTimeLeftOldestMin: %d\n", (int)apr_time_sec(min_expiry - now));
ap_rprintf(r, "CacheTimeLeftOldestMax: %d\n", (int)apr_time_sec(max_expiry - now));
}
}
ap_rprintf(r, "CacheIndexUsage: %d%%\n", index_pct);
ap_rprintf(r, "CacheUsage: %d%%\n", cache_pct);
ap_rprintf(r, "CacheStoreCount: %lu\n", header->stat_stores);
ap_rprintf(r, "CacheReplaceCount: %lu\n", header->stat_replaced);
ap_rprintf(r, "CacheExpireCount: %lu\n", header->stat_expiries);
ap_rprintf(r, "CacheDiscardCount: %lu\n", header->stat_scrolled);
ap_rprintf(r, "CacheRetrieveHitCount: %lu\n", header->stat_retrieves_hit);
ap_rprintf(r, "CacheRetrieveMissCount: %lu\n", header->stat_retrieves_miss);
ap_rprintf(r, "CacheRemoveHitCount: %lu\n", header->stat_removes_hit);
ap_rprintf(r, "CacheRemoveMissCount: %lu\n", header->stat_removes_miss);
}
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(00841) "leaving shmcb_status");
}
static apr_status_t socache_shmcb_iterate(ap_socache_instance_t *instance,
server_rec *s, void *userctx,
ap_socache_iterator_t *iterator,
apr_pool_t *pool) {
SHMCBHeader *header = instance->header;
unsigned int loop;
apr_time_t now = apr_time_now();
apr_status_t rv = APR_SUCCESS;
apr_size_t buflen = 0;
unsigned char *buf = NULL;
for (loop = 0; loop < header->subcache_num && rv == APR_SUCCESS; loop++) {
SHMCBSubcache *subcache = SHMCB_SUBCACHE(header, loop);
rv = shmcb_subcache_iterate(instance, s, userctx, header, subcache,
iterator, &buf, &buflen, pool, now);
}
return rv;
}
static void shmcb_subcache_expire(server_rec *s, SHMCBHeader *header,
SHMCBSubcache *subcache, apr_time_t now) {
unsigned int loop = 0, freed = 0, expired = 0;
unsigned int new_idx_pos = subcache->idx_pos;
SHMCBIndex *idx = NULL;
while (loop < subcache->idx_used) {
idx = SHMCB_INDEX(subcache, new_idx_pos);
if (idx->removed)
freed++;
else if (idx->expires <= now)
expired++;
else
break;
loop++;
new_idx_pos = SHMCB_CYCLIC_INCREMENT(new_idx_pos, 1, header->index_num);
}
if (!loop)
return;
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s, APLOGNO(00842)
"expiring %u and reclaiming %u removed socache entries",
expired, freed);
if (loop == subcache->idx_used) {
subcache->idx_used = 0;
subcache->data_used = 0;
} else {
unsigned int diff = SHMCB_CYCLIC_SPACE(subcache->data_pos,
idx->data_pos,
header->subcache_data_size);
subcache->idx_used -= loop;
subcache->idx_pos = new_idx_pos;
subcache->data_used -= diff;
subcache->data_pos = idx->data_pos;
}
header->stat_expiries += expired;
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s, APLOGNO(00843)
"we now have %u socache entries", subcache->idx_used);
}
static int shmcb_subcache_store(server_rec *s, SHMCBHeader *header,
SHMCBSubcache *subcache,
unsigned char *data, unsigned int data_len,
const unsigned char *id, unsigned int id_len,
apr_time_t expiry) {
unsigned int data_offset, new_idx, id_offset;
SHMCBIndex *idx;
unsigned int total_len = id_len + data_len;
if (total_len > header->subcache_data_size) {
ap_log_error(APLOG_MARK, APLOG_ERR, 0, s, APLOGNO(00844)
"inserting socache entry larger (%d) than subcache data area (%d)",
total_len, header->subcache_data_size);
return -1;
}
shmcb_subcache_expire(s, header, subcache, apr_time_now());
if (header->subcache_data_size - subcache->data_used < total_len
|| subcache->idx_used == header->index_num) {
unsigned int loop = 0;
idx = SHMCB_INDEX(subcache, subcache->idx_pos);
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s, APLOGNO(00845)
"about to force-expire, subcache: idx_used=%d, "
"data_used=%d", subcache->idx_used, subcache->data_used);
do {
SHMCBIndex *idx2;
subcache->idx_pos = SHMCB_CYCLIC_INCREMENT(subcache->idx_pos, 1,
header->index_num);
subcache->idx_used--;
if (!subcache->idx_used) {
subcache->data_used = 0;
break;
}
idx2 = SHMCB_INDEX(subcache, subcache->idx_pos);
subcache->data_used -= SHMCB_CYCLIC_SPACE(idx->data_pos, idx2->data_pos,
header->subcache_data_size);
subcache->data_pos = idx2->data_pos;
header->stat_scrolled++;
idx = idx2;
loop++;
} while (header->subcache_data_size - subcache->data_used < total_len);
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s, APLOGNO(00846)
"finished force-expire, subcache: idx_used=%d, "
"data_used=%d", subcache->idx_used, subcache->data_used);
}
id_offset = SHMCB_CYCLIC_INCREMENT(subcache->data_pos, subcache->data_used,
header->subcache_data_size);
shmcb_cyclic_ntoc_memcpy(header->subcache_data_size,
SHMCB_DATA(header, subcache), id_offset,
id, id_len);
subcache->data_used += id_len;
data_offset = SHMCB_CYCLIC_INCREMENT(subcache->data_pos, subcache->data_used,
header->subcache_data_size);
shmcb_cyclic_ntoc_memcpy(header->subcache_data_size,
SHMCB_DATA(header, subcache), data_offset,
data, data_len);
subcache->data_used += data_len;
new_idx = SHMCB_CYCLIC_INCREMENT(subcache->idx_pos, subcache->idx_used,
header->index_num);
idx = SHMCB_INDEX(subcache, new_idx);
idx->expires = expiry;
idx->data_pos = id_offset;
idx->data_used = total_len;
idx->id_len = id_len;
idx->removed = 0;
subcache->idx_used++;
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s, APLOGNO(00847)
"insert happened at idx=%d, data=(%u:%u)", new_idx,
id_offset, data_offset);
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s, APLOGNO(00848)
"finished insert, subcache: idx_pos/idx_used=%d/%d, "
"data_pos/data_used=%d/%d",
subcache->idx_pos, subcache->idx_used,
subcache->data_pos, subcache->data_used);
return 0;
}
static int shmcb_subcache_retrieve(server_rec *s, SHMCBHeader *header,
SHMCBSubcache *subcache,
const unsigned char *id, unsigned int idlen,
unsigned char *dest, unsigned int *destlen) {
unsigned int pos;
unsigned int loop = 0;
apr_time_t now = apr_time_now();
pos = subcache->idx_pos;
while (loop < subcache->idx_used) {
SHMCBIndex *idx = SHMCB_INDEX(subcache, pos);
if (!idx->removed
&& idx->id_len == idlen
&& (idx->data_used - idx->id_len) <= *destlen
&& shmcb_cyclic_memcmp(header->subcache_data_size,
SHMCB_DATA(header, subcache),
idx->data_pos, id, idx->id_len) == 0) {
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s, APLOGNO(00849)
"match at idx=%d, data=%d", pos, idx->data_pos);
if (idx->expires > now) {
unsigned int data_offset;
data_offset = SHMCB_CYCLIC_INCREMENT(idx->data_pos,
idx->id_len,
header->subcache_data_size);
*destlen = idx->data_used - idx->id_len;
shmcb_cyclic_cton_memcpy(header->subcache_data_size,
dest, SHMCB_DATA(header, subcache),
data_offset, *destlen);
return 0;
} else {
idx->removed = 1;
header->stat_expiries++;
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s, APLOGNO(00850)
"shmcb_subcache_retrieve discarding expired entry");
return -1;
}
}
loop++;
pos = SHMCB_CYCLIC_INCREMENT(pos, 1, header->index_num);
}
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s, APLOGNO(00851)
"shmcb_subcache_retrieve found no match");
return -1;
}
static int shmcb_subcache_remove(server_rec *s, SHMCBHeader *header,
SHMCBSubcache *subcache,
const unsigned char *id,
unsigned int idlen) {
unsigned int pos;
unsigned int loop = 0;
pos = subcache->idx_pos;
while (loop < subcache->idx_used) {
SHMCBIndex *idx = SHMCB_INDEX(subcache, pos);
if (!idx->removed && idx->id_len == idlen
&& shmcb_cyclic_memcmp(header->subcache_data_size,
SHMCB_DATA(header, subcache),
idx->data_pos, id, idx->id_len) == 0) {
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s, APLOGNO(00852)
"possible match at idx=%d, data=%d", pos, idx->data_pos);
idx->removed = 1;
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s, APLOGNO(00853)
"shmcb_subcache_remove removing matching entry");
return 0;
}
loop++;
pos = SHMCB_CYCLIC_INCREMENT(pos, 1, header->index_num);
}
return -1;
}
static apr_status_t shmcb_subcache_iterate(ap_socache_instance_t *instance,
server_rec *s,
void *userctx,
SHMCBHeader *header,
SHMCBSubcache *subcache,
ap_socache_iterator_t *iterator,
unsigned char **buf,
apr_size_t *buf_len,
apr_pool_t *pool,
apr_time_t now) {
unsigned int pos;
unsigned int loop = 0;
apr_status_t rv;
pos = subcache->idx_pos;
while (loop < subcache->idx_used) {
SHMCBIndex *idx = SHMCB_INDEX(subcache, pos);
if (!idx->removed) {
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s, APLOGNO(00854)
"iterating idx=%d, data=%d", pos, idx->data_pos);
if (idx->expires > now) {
unsigned char *id = *buf;
unsigned char *dest;
unsigned int data_offset, dest_len;
apr_size_t buf_req;
data_offset = SHMCB_CYCLIC_INCREMENT(idx->data_pos,
idx->id_len,
header->subcache_data_size);
dest_len = idx->data_used - idx->id_len;
buf_req = APR_ALIGN_DEFAULT(idx->id_len + 1)
+ APR_ALIGN_DEFAULT(dest_len + 1);
if (buf_req > *buf_len) {
*buf_len = buf_req + APR_ALIGN_DEFAULT(buf_req / 2);
*buf = apr_palloc(pool, *buf_len);
id = *buf;
}
dest = *buf + APR_ALIGN_DEFAULT(idx->id_len + 1);
shmcb_cyclic_cton_memcpy(header->subcache_data_size, id,
SHMCB_DATA(header, subcache),
idx->data_pos, idx->id_len);
id[idx->id_len] = '\0';
shmcb_cyclic_cton_memcpy(header->subcache_data_size, dest,
SHMCB_DATA(header, subcache),
data_offset, dest_len);
dest[dest_len] = '\0';
rv = iterator(instance, s, userctx, id, idx->id_len,
dest, dest_len, pool);
ap_log_error(APLOG_MARK, APLOG_DEBUG, rv, s, APLOGNO(00855)
"shmcb entry iterated");
if (rv != APR_SUCCESS)
return rv;
} else {
idx->removed = 1;
header->stat_expiries++;
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s, APLOGNO(00856)
"shmcb_subcache_iterate discarding expired entry");
}
}
loop++;
pos = SHMCB_CYCLIC_INCREMENT(pos, 1, header->index_num);
}
return APR_SUCCESS;
}
static const ap_socache_provider_t socache_shmcb = {
"shmcb",
AP_SOCACHE_FLAG_NOTMPSAFE,
socache_shmcb_create,
socache_shmcb_init,
socache_shmcb_destroy,
socache_shmcb_store,
socache_shmcb_retrieve,
socache_shmcb_remove,
socache_shmcb_status,
socache_shmcb_iterate
};
static void register_hooks(apr_pool_t *p) {
ap_register_provider(p, AP_SOCACHE_PROVIDER_GROUP, "shmcb",
AP_SOCACHE_PROVIDER_VERSION,
&socache_shmcb);
ap_register_provider(p, AP_SOCACHE_PROVIDER_GROUP,
AP_SOCACHE_DEFAULT_PROVIDER,
AP_SOCACHE_PROVIDER_VERSION,
&socache_shmcb);
}
AP_DECLARE_MODULE(socache_shmcb) = {
STANDARD20_MODULE_STUFF,
NULL, NULL, NULL, NULL, NULL,
register_hooks
};
