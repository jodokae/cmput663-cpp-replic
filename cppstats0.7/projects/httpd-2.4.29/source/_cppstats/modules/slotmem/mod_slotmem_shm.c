#include "ap_slotmem.h"
#include "httpd.h"
#include "http_main.h"
#include "ap_mpm.h"
#define AP_SLOTMEM_IS_PREGRAB(t) (t->desc.type & AP_SLOTMEM_TYPE_PREGRAB)
#define AP_SLOTMEM_IS_PERSIST(t) (t->desc.type & AP_SLOTMEM_TYPE_PERSIST)
#define AP_SLOTMEM_IS_CLEARINUSE(t) (t->desc.type & AP_SLOTMEM_TYPE_CLEARINUSE)
typedef struct {
apr_size_t size;
unsigned int num;
ap_slotmem_type_t type;
} sharedslotdesc_t;
#define AP_SLOTMEM_OFFSET (APR_ALIGN_DEFAULT(sizeof(sharedslotdesc_t)))
#define AP_UNSIGNEDINT_OFFSET (APR_ALIGN_DEFAULT(sizeof(unsigned int)))
struct ap_slotmem_instance_t {
char *name;
char *pname;
int fbased;
void *shm;
void *base;
apr_pool_t *gpool;
char *inuse;
unsigned int *num_free;
void *persist;
sharedslotdesc_t desc;
struct ap_slotmem_instance_t *next;
};
static struct ap_slotmem_instance_t *globallistmem = NULL;
static apr_pool_t *gpool = NULL;
#define DEFAULT_SLOTMEM_PREFIX "slotmem-shm-"
#define DEFAULT_SLOTMEM_SUFFIX ".shm"
#define DEFAULT_SLOTMEM_PERSIST_SUFFIX ".persist"
#if !defined(SLOTMEM_UNLINK_SEMANTIC)
#if defined(WIN32) || defined(OS2)
#define SLOTMEM_UNLINK_SEMANTIC 0
#else
#define SLOTMEM_UNLINK_SEMANTIC 1
#endif
#endif
static int slotmem_filenames(apr_pool_t *pool,
const char *slotname,
const char **filename,
const char **persistname) {
const char *fname = NULL, *pname = NULL;
if (slotname && *slotname && strcasecmp(slotname, "none") != 0) {
if (slotname[0] != '/') {
#if !SLOTMEM_UNLINK_SEMANTIC
int generation = 0;
ap_mpm_query(AP_MPMQ_GENERATION, &generation);
fname = apr_psprintf(pool, "%s%s_%x%s", DEFAULT_SLOTMEM_PREFIX,
slotname, generation, DEFAULT_SLOTMEM_SUFFIX);
#else
fname = apr_pstrcat(pool, DEFAULT_SLOTMEM_PREFIX,
slotname, DEFAULT_SLOTMEM_SUFFIX,
NULL);
#endif
fname = ap_runtime_dir_relative(pool, fname);
} else {
fname = slotname;
}
if (persistname) {
#if !SLOTMEM_UNLINK_SEMANTIC
if (slotname[0] != '/') {
pname = apr_pstrcat(pool, DEFAULT_SLOTMEM_PREFIX,
slotname, DEFAULT_SLOTMEM_SUFFIX,
DEFAULT_SLOTMEM_PERSIST_SUFFIX,
NULL);
pname = ap_runtime_dir_relative(pool, pname);
} else
#endif
pname = apr_pstrcat(pool, fname,
DEFAULT_SLOTMEM_PERSIST_SUFFIX,
NULL);
}
}
*filename = fname;
if (persistname) {
*persistname = pname;
}
return (fname != NULL);
}
static void slotmem_clearinuse(ap_slotmem_instance_t *slot) {
unsigned int i;
char *inuse;
if (!slot) {
return;
}
inuse = slot->inuse;
for (i = 0; i < slot->desc.num; i++, inuse++) {
if (*inuse) {
*inuse = 0;
(*slot->num_free)++;
}
}
}
static void store_slotmem(ap_slotmem_instance_t *slotmem) {
apr_file_t *fp;
apr_status_t rv;
apr_size_t nbytes;
unsigned char digest[APR_MD5_DIGESTSIZE];
const char *storename = slotmem->pname;
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, ap_server_conf, APLOGNO(02334)
"storing %s", storename);
if (storename) {
rv = apr_file_open(&fp, storename, APR_CREATE | APR_READ | APR_WRITE,
APR_OS_DEFAULT, slotmem->gpool);
if (APR_STATUS_IS_EEXIST(rv)) {
apr_file_remove(storename, slotmem->gpool);
rv = apr_file_open(&fp, storename, APR_CREATE | APR_READ | APR_WRITE,
APR_OS_DEFAULT, slotmem->gpool);
}
if (rv != APR_SUCCESS) {
return;
}
if (AP_SLOTMEM_IS_CLEARINUSE(slotmem)) {
slotmem_clearinuse(slotmem);
}
nbytes = (slotmem->desc.size * slotmem->desc.num) +
(slotmem->desc.num * sizeof(char)) + AP_UNSIGNEDINT_OFFSET;
apr_md5(digest, slotmem->persist, nbytes);
rv = apr_file_write_full(fp, slotmem->persist, nbytes, NULL);
if (rv == APR_SUCCESS) {
rv = apr_file_write_full(fp, digest, APR_MD5_DIGESTSIZE, NULL);
}
apr_file_close(fp);
if (rv != APR_SUCCESS) {
apr_file_remove(storename, slotmem->gpool);
}
}
}
static apr_status_t restore_slotmem(void *ptr, const char *storename,
apr_size_t size, apr_pool_t *pool) {
apr_file_t *fp;
apr_size_t nbytes = size;
apr_status_t rv = APR_SUCCESS;
unsigned char digest[APR_MD5_DIGESTSIZE];
unsigned char digest2[APR_MD5_DIGESTSIZE];
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, ap_server_conf, APLOGNO(02335)
"restoring %s", storename);
if (storename) {
rv = apr_file_open(&fp, storename, APR_READ | APR_WRITE, APR_OS_DEFAULT,
pool);
if (rv == APR_SUCCESS) {
rv = apr_file_read(fp, ptr, &nbytes);
if ((rv == APR_SUCCESS || rv == APR_EOF) && nbytes == size) {
rv = APR_SUCCESS;
if (apr_file_eof(fp) != APR_EOF) {
apr_size_t ds = APR_MD5_DIGESTSIZE;
rv = apr_file_read(fp, digest, &ds);
if ((rv == APR_SUCCESS || rv == APR_EOF) &&
ds == APR_MD5_DIGESTSIZE) {
rv = APR_SUCCESS;
apr_md5(digest2, ptr, nbytes);
if (memcmp(digest, digest2, APR_MD5_DIGESTSIZE)) {
ap_log_error(APLOG_MARK, APLOG_ERR, 0, ap_server_conf,
APLOGNO(02551) "bad md5 match");
rv = APR_EGENERAL;
}
}
} else {
ap_log_error(APLOG_MARK, APLOG_NOTICE, 0, ap_server_conf,
APLOGNO(02552) "at EOF... bypassing md5 match check (old persist file?)");
}
} else if (nbytes != size) {
ap_log_error(APLOG_MARK, APLOG_ERR, 0, ap_server_conf,
APLOGNO(02553) "Expected %" APR_SIZE_T_FMT ": Read %" APR_SIZE_T_FMT,
size, nbytes);
rv = APR_EGENERAL;
}
apr_file_close(fp);
}
}
return rv;
}
static apr_status_t cleanup_slotmem(void *param) {
ap_slotmem_instance_t **mem = param;
if (*mem) {
ap_slotmem_instance_t *next = *mem;
while (next) {
if (AP_SLOTMEM_IS_PERSIST(next)) {
store_slotmem(next);
}
apr_shm_destroy((apr_shm_t *)next->shm);
if (next->fbased) {
apr_shm_remove(next->name, next->gpool);
apr_file_remove(next->name, next->gpool);
}
next = next->next;
}
}
globallistmem = NULL;
return APR_SUCCESS;
}
static apr_status_t slotmem_doall(ap_slotmem_instance_t *mem,
ap_slotmem_callback_fn_t *func,
void *data, apr_pool_t *pool) {
unsigned int i;
char *ptr;
char *inuse;
apr_status_t retval = APR_SUCCESS;
if (!mem) {
return APR_ENOSHMAVAIL;
}
ptr = (char *)mem->base;
inuse = mem->inuse;
for (i = 0; i < mem->desc.num; i++, inuse++) {
if (!AP_SLOTMEM_IS_PREGRAB(mem) ||
(AP_SLOTMEM_IS_PREGRAB(mem) && *inuse)) {
retval = func((void *) ptr, data, pool);
if (retval != APR_SUCCESS)
break;
}
ptr += mem->desc.size;
}
return retval;
}
static apr_status_t slotmem_create(ap_slotmem_instance_t **new,
const char *name, apr_size_t item_size,
unsigned int item_num,
ap_slotmem_type_t type, apr_pool_t *pool) {
int fbased = 1;
int restored = 0;
char *ptr;
sharedslotdesc_t desc;
ap_slotmem_instance_t *res;
ap_slotmem_instance_t *next = globallistmem;
const char *fname, *pname = NULL;
apr_shm_t *shm;
apr_size_t basesize = (item_size * item_num);
apr_size_t size = AP_SLOTMEM_OFFSET + AP_UNSIGNEDINT_OFFSET +
(item_num * sizeof(char)) + basesize;
int persist = (type & AP_SLOTMEM_TYPE_PERSIST) != 0;
apr_status_t rv;
if (gpool == NULL) {
return APR_ENOSHMAVAIL;
}
if (slotmem_filenames(pool, name, &fname, persist ? &pname : NULL)) {
if (next) {
for (;;) {
if (strcmp(next->name, fname) == 0) {
*new = next;
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, ap_server_conf, APLOGNO(02603)
"create found %s in global list", fname);
return APR_SUCCESS;
}
if (!next->next) {
break;
}
next = next->next;
}
}
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, ap_server_conf, APLOGNO(02602)
"create didn't find %s in global list", fname);
} else {
fbased = 0;
fname = "none";
}
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, ap_server_conf, APLOGNO(02300)
"create %s: %"APR_SIZE_T_FMT"/%u", fname, item_size,
item_num);
if (fbased) {
rv = apr_shm_attach(&shm, fname, gpool);
} else {
rv = APR_EINVAL;
}
if (rv == APR_SUCCESS) {
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, ap_server_conf, APLOGNO(02598)
"apr_shm_attach() succeeded");
if (apr_shm_size_get(shm) != size) {
apr_shm_detach(shm);
ap_log_error(APLOG_MARK, APLOG_ERR, 0, ap_server_conf, APLOGNO(02599)
"existing shared memory for %s could not be used (failed size check)",
fname);
return APR_EINVAL;
}
ptr = (char *)apr_shm_baseaddr_get(shm);
memcpy(&desc, ptr, sizeof(desc));
if (desc.size != item_size || desc.num != item_num) {
apr_shm_detach(shm);
ap_log_error(APLOG_MARK, APLOG_ERR, 0, ap_server_conf, APLOGNO(02600)
"existing shared memory for %s could not be used (failed contents check)",
fname);
return APR_EINVAL;
}
ptr += AP_SLOTMEM_OFFSET;
} else {
apr_size_t dsize = size - AP_SLOTMEM_OFFSET;
if (fbased) {
apr_shm_remove(fname, gpool);
rv = apr_shm_create(&shm, size, fname, gpool);
} else {
rv = apr_shm_create(&shm, size, NULL, gpool);
}
ap_log_error(APLOG_MARK, rv == APR_SUCCESS ? APLOG_DEBUG : APLOG_ERR,
rv, ap_server_conf, APLOGNO(02611)
"create: apr_shm_create(%s) %s",
fname ? fname : "",
rv == APR_SUCCESS ? "succeeded" : "failed");
if (rv != APR_SUCCESS) {
return rv;
}
ptr = (char *)apr_shm_baseaddr_get(shm);
desc.size = item_size;
desc.num = item_num;
desc.type = type;
memcpy(ptr, &desc, sizeof(desc));
ptr += AP_SLOTMEM_OFFSET;
memset(ptr, 0, dsize);
if (persist) {
rv = restore_slotmem(ptr, pname, dsize, pool);
if (rv == APR_SUCCESS) {
restored = 1;
} else {
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, ap_server_conf,
APLOGNO(02554) "could not restore %s", fname);
memset(ptr, 0, dsize);
}
}
}
res = (ap_slotmem_instance_t *) apr_pcalloc(gpool,
sizeof(ap_slotmem_instance_t));
res->name = apr_pstrdup(gpool, fname);
res->pname = apr_pstrdup(gpool, pname);
res->fbased = fbased;
res->shm = shm;
res->num_free = (unsigned int *)ptr;
if (!restored) {
*res->num_free = item_num;
}
res->persist = (void *)ptr;
ptr += AP_UNSIGNEDINT_OFFSET;
res->base = (void *)ptr;
res->desc = desc;
res->gpool = gpool;
res->next = NULL;
res->inuse = ptr + basesize;
if (globallistmem == NULL) {
globallistmem = res;
} else {
next->next = res;
}
*new = res;
return APR_SUCCESS;
}
static apr_status_t slotmem_attach(ap_slotmem_instance_t **new,
const char *name, apr_size_t *item_size,
unsigned int *item_num, apr_pool_t *pool) {
char *ptr;
ap_slotmem_instance_t *res;
ap_slotmem_instance_t *next = globallistmem;
sharedslotdesc_t desc;
const char *fname;
apr_shm_t *shm;
apr_status_t rv;
if (gpool == NULL) {
return APR_ENOSHMAVAIL;
}
if (!slotmem_filenames(pool, name, &fname, NULL)) {
return APR_ENOSHMAVAIL;
}
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, ap_server_conf, APLOGNO(02301)
"attach looking for %s", fname);
if (next) {
for (;;) {
if (strcmp(next->name, fname) == 0) {
*new = next;
*item_size = next->desc.size;
*item_num = next->desc.num;
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, ap_server_conf,
APLOGNO(02302)
"attach found %s: %"APR_SIZE_T_FMT"/%u", fname,
*item_size, *item_num);
return APR_SUCCESS;
}
if (!next->next) {
break;
}
next = next->next;
}
}
rv = apr_shm_attach(&shm, fname, gpool);
if (rv != APR_SUCCESS) {
return rv;
}
ptr = (char *)apr_shm_baseaddr_get(shm);
memcpy(&desc, ptr, sizeof(desc));
ptr += AP_SLOTMEM_OFFSET;
res = (ap_slotmem_instance_t *) apr_pcalloc(gpool,
sizeof(ap_slotmem_instance_t));
res->name = apr_pstrdup(gpool, fname);
res->fbased = 1;
res->shm = shm;
res->num_free = (unsigned int *)ptr;
res->persist = (void *)ptr;
ptr += AP_UNSIGNEDINT_OFFSET;
res->base = (void *)ptr;
res->desc = desc;
res->gpool = gpool;
res->inuse = ptr + (desc.size * desc.num);
res->next = NULL;
if (globallistmem == NULL) {
globallistmem = res;
} else {
next->next = res;
}
*new = res;
*item_size = desc.size;
*item_num = desc.num;
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, ap_server_conf,
APLOGNO(02303)
"attach found %s: %"APR_SIZE_T_FMT"/%u", fname,
*item_size, *item_num);
return APR_SUCCESS;
}
static apr_status_t slotmem_dptr(ap_slotmem_instance_t *slot,
unsigned int id, void **mem) {
char *ptr;
if (!slot) {
return APR_ENOSHMAVAIL;
}
if (id >= slot->desc.num) {
return APR_EINVAL;
}
ptr = (char *)slot->base + slot->desc.size * id;
if (!ptr) {
return APR_ENOSHMAVAIL;
}
*mem = (void *)ptr;
return APR_SUCCESS;
}
static apr_status_t slotmem_get(ap_slotmem_instance_t *slot, unsigned int id,
unsigned char *dest, apr_size_t dest_len) {
void *ptr;
char *inuse;
apr_status_t ret;
if (!slot) {
return APR_ENOSHMAVAIL;
}
inuse = slot->inuse + id;
if (id >= slot->desc.num) {
return APR_EINVAL;
}
if (AP_SLOTMEM_IS_PREGRAB(slot) && !*inuse) {
return APR_NOTFOUND;
}
ret = slotmem_dptr(slot, id, &ptr);
if (ret != APR_SUCCESS) {
return ret;
}
*inuse = 1;
memcpy(dest, ptr, dest_len);
return APR_SUCCESS;
}
static apr_status_t slotmem_put(ap_slotmem_instance_t *slot, unsigned int id,
unsigned char *src, apr_size_t src_len) {
void *ptr;
char *inuse;
apr_status_t ret;
if (!slot) {
return APR_ENOSHMAVAIL;
}
inuse = slot->inuse + id;
if (id >= slot->desc.num) {
return APR_EINVAL;
}
if (AP_SLOTMEM_IS_PREGRAB(slot) && !*inuse) {
return APR_NOTFOUND;
}
ret = slotmem_dptr(slot, id, &ptr);
if (ret != APR_SUCCESS) {
return ret;
}
*inuse=1;
memcpy(ptr, src, src_len);
return APR_SUCCESS;
}
static unsigned int slotmem_num_slots(ap_slotmem_instance_t *slot) {
return slot->desc.num;
}
static unsigned int slotmem_num_free_slots(ap_slotmem_instance_t *slot) {
if (AP_SLOTMEM_IS_PREGRAB(slot))
return *slot->num_free;
else {
unsigned int i, counter=0;
char *inuse = slot->inuse;
for (i=0; i<slot->desc.num; i++, inuse++) {
if (!*inuse)
counter++;
}
return counter;
}
}
static apr_size_t slotmem_slot_size(ap_slotmem_instance_t *slot) {
return slot->desc.size;
}
static apr_status_t slotmem_grab(ap_slotmem_instance_t *slot, unsigned int *id) {
unsigned int i;
char *inuse;
if (!slot) {
return APR_ENOSHMAVAIL;
}
inuse = slot->inuse;
for (i = 0; i < slot->desc.num; i++, inuse++) {
if (!*inuse) {
break;
}
}
if (i >= slot->desc.num) {
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, ap_server_conf, APLOGNO(02293)
"slotmem(%s) grab failed. Num %u/num_free %u",
slot->name, slotmem_num_slots(slot),
slotmem_num_free_slots(slot));
return APR_EINVAL;
}
*inuse = 1;
*id = i;
(*slot->num_free)--;
return APR_SUCCESS;
}
static apr_status_t slotmem_fgrab(ap_slotmem_instance_t *slot, unsigned int id) {
char *inuse;
if (!slot) {
return APR_ENOSHMAVAIL;
}
if (id >= slot->desc.num) {
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, ap_server_conf, APLOGNO(02397)
"slotmem(%s) fgrab failed. Num %u/num_free %u",
slot->name, slotmem_num_slots(slot),
slotmem_num_free_slots(slot));
return APR_EINVAL;
}
inuse = slot->inuse + id;
if (!*inuse) {
*inuse = 1;
(*slot->num_free)--;
}
return APR_SUCCESS;
}
static apr_status_t slotmem_release(ap_slotmem_instance_t *slot,
unsigned int id) {
char *inuse;
if (!slot) {
return APR_ENOSHMAVAIL;
}
inuse = slot->inuse;
if (id >= slot->desc.num || !inuse[id] ) {
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, ap_server_conf, APLOGNO(02294)
"slotmem(%s) release failed. Num %u/inuse[%u] %d",
slot->name, slotmem_num_slots(slot),
id, (int)inuse[id]);
if (id >= slot->desc.num) {
return APR_EINVAL;
} else {
return APR_NOTFOUND;
}
}
inuse[id] = 0;
(*slot->num_free)++;
return APR_SUCCESS;
}
static const ap_slotmem_provider_t storage = {
"sharedmem",
&slotmem_doall,
&slotmem_create,
&slotmem_attach,
&slotmem_dptr,
&slotmem_get,
&slotmem_put,
&slotmem_num_slots,
&slotmem_num_free_slots,
&slotmem_slot_size,
&slotmem_grab,
&slotmem_release,
&slotmem_fgrab
};
static const ap_slotmem_provider_t *slotmem_shm_getstorage(void) {
return (&storage);
}
static void slotmem_shm_initgpool(apr_pool_t *p) {
gpool = p;
}
static void slotmem_shm_initialize_cleanup(apr_pool_t *p) {
apr_pool_cleanup_register(p, &globallistmem, cleanup_slotmem,
apr_pool_cleanup_null);
}
static int post_config(apr_pool_t *p, apr_pool_t *plog, apr_pool_t *ptemp,
server_rec *s) {
slotmem_shm_initialize_cleanup(p);
return OK;
}
static int pre_config(apr_pool_t *p, apr_pool_t *plog,
apr_pool_t *ptemp) {
slotmem_shm_initgpool(p);
return OK;
}
static void ap_slotmem_shm_register_hook(apr_pool_t *p) {
const ap_slotmem_provider_t *storage = slotmem_shm_getstorage();
ap_register_provider(p, AP_SLOTMEM_PROVIDER_GROUP, "shm",
AP_SLOTMEM_PROVIDER_VERSION, storage);
ap_hook_post_config(post_config, NULL, NULL, APR_HOOK_LAST);
ap_hook_pre_config(pre_config, NULL, NULL, APR_HOOK_MIDDLE);
}
AP_DECLARE_MODULE(slotmem_shm) = {
STANDARD20_MODULE_STUFF,
NULL,
NULL,
NULL,
NULL,
NULL,
ap_slotmem_shm_register_hook
};