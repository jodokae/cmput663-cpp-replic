#if !defined(APU_LDAP_CACHE_H)
#define APU_LDAP_CACHE_H
#if APR_HAS_LDAP
#include "util_ldap.h"
typedef struct util_cache_node_t {
void *payload;
apr_time_t add_time;
struct util_cache_node_t *next;
} util_cache_node_t;
typedef struct util_ald_cache util_ald_cache_t;
struct util_ald_cache {
unsigned long size;
unsigned long maxentries;
unsigned long numentries;
unsigned long fullmark;
apr_time_t marktime;
unsigned long (*hash)(void *);
int (*compare)(void *, void *);
void * (*copy)(util_ald_cache_t *cache, void *);
void (*free)(util_ald_cache_t *cache, void *);
void (*display)(request_rec *r, util_ald_cache_t *cache, void *);
util_cache_node_t **nodes;
unsigned long numpurges;
double avg_purgetime;
apr_time_t last_purge;
unsigned long npurged;
unsigned long fetches;
unsigned long hits;
unsigned long inserts;
unsigned long removes;
#if APR_HAS_SHARED_MEMORY
apr_shm_t *shm_addr;
apr_rmm_t *rmm_addr;
#endif
};
#if !defined(WIN32)
#define ALD_MM_FILE_MODE ( S_IRUSR|S_IWUSR )
#else
#define ALD_MM_FILE_MODE ( _S_IREAD|_S_IWRITE )
#endif
typedef struct util_url_node_t {
const char *url;
util_ald_cache_t *search_cache;
util_ald_cache_t *compare_cache;
util_ald_cache_t *dn_compare_cache;
} util_url_node_t;
typedef struct util_compare_subgroup_t {
const char **subgroupDNs;
int len;
} util_compare_subgroup_t;
typedef struct util_search_node_t {
const char *username;
const char *dn;
const char *bindpw;
apr_time_t lastbind;
const char **vals;
int numvals;
} util_search_node_t;
typedef struct util_compare_node_t {
const char *dn;
const char *attrib;
const char *value;
apr_time_t lastcompare;
int result;
int sgl_processed;
struct util_compare_subgroup_t *subgroupList;
} util_compare_node_t;
typedef struct util_dn_compare_node_t {
const char *reqdn;
const char *dn;
} util_dn_compare_node_t;
unsigned long util_ldap_url_node_hash(void *n);
int util_ldap_url_node_compare(void *a, void *b);
void *util_ldap_url_node_copy(util_ald_cache_t *cache, void *c);
void util_ldap_url_node_free(util_ald_cache_t *cache, void *n);
void util_ldap_url_node_display(request_rec *r, util_ald_cache_t *cache, void *n);
unsigned long util_ldap_search_node_hash(void *n);
int util_ldap_search_node_compare(void *a, void *b);
void *util_ldap_search_node_copy(util_ald_cache_t *cache, void *c);
void util_ldap_search_node_free(util_ald_cache_t *cache, void *n);
void util_ldap_search_node_display(request_rec *r, util_ald_cache_t *cache, void *n);
unsigned long util_ldap_compare_node_hash(void *n);
int util_ldap_compare_node_compare(void *a, void *b);
void *util_ldap_compare_node_copy(util_ald_cache_t *cache, void *c);
void util_ldap_compare_node_free(util_ald_cache_t *cache, void *n);
void util_ldap_compare_node_display(request_rec *r, util_ald_cache_t *cache, void *n);
unsigned long util_ldap_dn_compare_node_hash(void *n);
int util_ldap_dn_compare_node_compare(void *a, void *b);
void *util_ldap_dn_compare_node_copy(util_ald_cache_t *cache, void *c);
void util_ldap_dn_compare_node_free(util_ald_cache_t *cache, void *n);
void util_ldap_dn_compare_node_display(request_rec *r, util_ald_cache_t *cache, void *n);
void util_ald_free(util_ald_cache_t *cache, const void *ptr);
void *util_ald_alloc(util_ald_cache_t *cache, unsigned long size);
const char *util_ald_strdup(util_ald_cache_t *cache, const char *s);
util_compare_subgroup_t *util_ald_sgl_dup(util_ald_cache_t *cache, util_compare_subgroup_t *sgl);
void util_ald_sgl_free(util_ald_cache_t *cache, util_compare_subgroup_t **sgl);
unsigned long util_ald_hash_string(int nstr, ...);
void util_ald_cache_purge(util_ald_cache_t *cache);
util_url_node_t *util_ald_create_caches(util_ldap_state_t *s, const char *url);
util_ald_cache_t *util_ald_create_cache(util_ldap_state_t *st,
long cache_size,
unsigned long (*hashfunc)(void *),
int (*comparefunc)(void *, void *),
void * (*copyfunc)(util_ald_cache_t *cache, void *),
void (*freefunc)(util_ald_cache_t *cache, void *),
void (*displayfunc)(request_rec *r, util_ald_cache_t *cache, void *));
void util_ald_destroy_cache(util_ald_cache_t *cache);
void *util_ald_cache_fetch(util_ald_cache_t *cache, void *payload);
void *util_ald_cache_insert(util_ald_cache_t *cache, void *payload);
void util_ald_cache_remove(util_ald_cache_t *cache, void *payload);
char *util_ald_cache_display_stats(request_rec *r, util_ald_cache_t *cache, char *name, char *id);
#endif
#endif
