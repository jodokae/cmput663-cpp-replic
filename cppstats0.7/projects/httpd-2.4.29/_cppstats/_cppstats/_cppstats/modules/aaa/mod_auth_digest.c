#include "apr_sha1.h"
#include "apr_base64.h"
#include "apr_lib.h"
#include "apr_time.h"
#include "apr_errno.h"
#include "apr_global_mutex.h"
#include "apr_strings.h"
#define APR_WANT_STRFUNC
#include "apr_want.h"
#include "ap_config.h"
#include "httpd.h"
#include "http_config.h"
#include "http_core.h"
#include "http_request.h"
#include "http_log.h"
#include "http_protocol.h"
#include "apr_uri.h"
#include "util_md5.h"
#include "util_mutex.h"
#include "apr_shm.h"
#include "apr_rmm.h"
#include "ap_provider.h"
#include "mod_auth.h"
#if APR_HAVE_UNISTD_H
#include <unistd.h>
#endif
typedef struct digest_config_struct {
const char *dir_name;
authn_provider_list *providers;
const char *realm;
apr_array_header_t *qop_list;
apr_sha1_ctx_t nonce_ctx;
apr_time_t nonce_lifetime;
const char *nonce_format;
int check_nc;
const char *algorithm;
char *uri_list;
const char *ha1;
} digest_config_rec;
#define DFLT_ALGORITHM "MD5"
#define DFLT_NONCE_LIFE apr_time_from_sec(300)
#define NEXTNONCE_DELTA apr_time_from_sec(30)
#define NONCE_TIME_LEN (((sizeof(apr_time_t)+2)/3)*4)
#define NONCE_HASH_LEN (2*APR_SHA1_DIGESTSIZE)
#define NONCE_LEN (int )(NONCE_TIME_LEN + NONCE_HASH_LEN)
#define SECRET_LEN 20
typedef struct hash_entry {
unsigned long key;
struct hash_entry *next;
unsigned long nonce_count;
char ha1[2*APR_MD5_DIGESTSIZE+1];
char last_nonce[NONCE_LEN+1];
} client_entry;
static struct hash_table {
client_entry **table;
unsigned long tbl_len;
unsigned long num_entries;
unsigned long num_created;
unsigned long num_removed;
unsigned long num_renewed;
} *client_list;
enum hdr_sts { NO_HEADER, NOT_DIGEST, INVALID, VALID };
typedef struct digest_header_struct {
const char *scheme;
const char *realm;
const char *username;
char *nonce;
const char *uri;
const char *method;
const char *digest;
const char *algorithm;
const char *cnonce;
const char *opaque;
unsigned long opaque_num;
const char *message_qop;
const char *nonce_count;
const char *raw_request_uri;
apr_uri_t *psd_request_uri;
apr_time_t nonce_time;
enum hdr_sts auth_hdr_sts;
int needed_auth;
client_entry *client;
} digest_header_rec;
typedef union time_union {
apr_time_t time;
unsigned char arr[sizeof(apr_time_t)];
} time_rec;
static unsigned char secret[SECRET_LEN];
static apr_shm_t *client_shm = NULL;
static apr_rmm_t *client_rmm = NULL;
static unsigned long *opaque_cntr;
static apr_time_t *otn_counter;
static apr_global_mutex_t *client_lock = NULL;
static apr_global_mutex_t *opaque_lock = NULL;
static const char *client_mutex_type = "authdigest-client";
static const char *opaque_mutex_type = "authdigest-opaque";
static const char *client_shm_filename;
#define DEF_SHMEM_SIZE 1000L
#define DEF_NUM_BUCKETS 15L
#define HASH_DEPTH 5
static apr_size_t shmem_size = DEF_SHMEM_SIZE;
static unsigned long num_buckets = DEF_NUM_BUCKETS;
module AP_MODULE_DECLARE_DATA auth_digest_module;
static apr_status_t cleanup_tables(void *not_used) {
ap_log_error(APLOG_MARK, APLOG_INFO, 0, NULL, APLOGNO(01756)
"cleaning up shared memory");
if (client_rmm) {
apr_rmm_destroy(client_rmm);
client_rmm = NULL;
}
if (client_shm) {
apr_shm_destroy(client_shm);
client_shm = NULL;
}
if (client_lock) {
apr_global_mutex_destroy(client_lock);
client_lock = NULL;
}
if (opaque_lock) {
apr_global_mutex_destroy(opaque_lock);
opaque_lock = NULL;
}
client_list = NULL;
return APR_SUCCESS;
}
static apr_status_t initialize_secret(server_rec *s) {
apr_status_t status;
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s, APLOGNO(01757)
"generating secret for digest authentication ...");
#if APR_HAS_RANDOM
status = apr_generate_random_bytes(secret, sizeof(secret));
#else
#error APR random number support is missing; you probably need to install the truerand library.
#endif
if (status != APR_SUCCESS) {
ap_log_error(APLOG_MARK, APLOG_CRIT, status, s, APLOGNO(01758)
"error generating secret");
return status;
}
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s, APLOGNO(01759) "done");
return APR_SUCCESS;
}
static void log_error_and_cleanup(char *msg, apr_status_t sts, server_rec *s) {
ap_log_error(APLOG_MARK, APLOG_ERR, sts, s, APLOGNO(01760)
"%s - all nonce-count checking, one-time nonces, and "
"MD5-sess algorithm disabled", msg);
cleanup_tables(NULL);
}
static void *rmm_malloc(apr_rmm_t *rmm, apr_size_t size) {
apr_rmm_off_t offset = apr_rmm_malloc(rmm, size);
if (!offset) {
return NULL;
}
return apr_rmm_addr_get(rmm, offset);
}
static apr_status_t rmm_free(apr_rmm_t *rmm, void *alloc) {
apr_rmm_off_t offset = apr_rmm_offset_get(rmm, alloc);
return apr_rmm_free(rmm, offset);
}
#if APR_HAS_SHARED_MEMORY
static int initialize_tables(server_rec *s, apr_pool_t *ctx) {
unsigned long idx;
apr_status_t sts;
client_shm_filename = ap_runtime_dir_relative(ctx, "authdigest_shm");
client_shm_filename = ap_append_pid(ctx, client_shm_filename, ".");
sts = apr_shm_create(&client_shm, shmem_size, NULL, ctx);
if (APR_STATUS_IS_ENOTIMPL(sts)) {
apr_shm_remove(client_shm_filename, ctx);
sts = apr_shm_create(&client_shm, shmem_size,
client_shm_filename, ctx);
}
if (APR_SUCCESS != sts) {
ap_log_error(APLOG_MARK, APLOG_ERR, sts, s, APLOGNO(01762)
"Failed to create shared memory segment on file %s",
client_shm_filename);
log_error_and_cleanup("failed to initialize shm", sts, s);
return HTTP_INTERNAL_SERVER_ERROR;
}
sts = apr_rmm_init(&client_rmm,
NULL,
apr_shm_baseaddr_get(client_shm),
shmem_size, ctx);
if (sts != APR_SUCCESS) {
log_error_and_cleanup("failed to initialize rmm", sts, s);
return !OK;
}
client_list = rmm_malloc(client_rmm, sizeof(*client_list) +
sizeof(client_entry *) * num_buckets);
if (!client_list) {
log_error_and_cleanup("failed to allocate shared memory", -1, s);
return !OK;
}
client_list->table = (client_entry**) (client_list + 1);
for (idx = 0; idx < num_buckets; idx++) {
client_list->table[idx] = NULL;
}
client_list->tbl_len = num_buckets;
client_list->num_entries = 0;
sts = ap_global_mutex_create(&client_lock, NULL, client_mutex_type, NULL,
s, ctx, 0);
if (sts != APR_SUCCESS) {
log_error_and_cleanup("failed to create lock (client_lock)", sts, s);
return !OK;
}
opaque_cntr = rmm_malloc(client_rmm, sizeof(*opaque_cntr));
if (opaque_cntr == NULL) {
log_error_and_cleanup("failed to allocate shared memory", -1, s);
return !OK;
}
*opaque_cntr = 1UL;
sts = ap_global_mutex_create(&opaque_lock, NULL, opaque_mutex_type, NULL,
s, ctx, 0);
if (sts != APR_SUCCESS) {
log_error_and_cleanup("failed to create lock (opaque_lock)", sts, s);
return !OK;
}
otn_counter = rmm_malloc(client_rmm, sizeof(*otn_counter));
if (otn_counter == NULL) {
log_error_and_cleanup("failed to allocate shared memory", -1, s);
return !OK;
}
*otn_counter = 0;
return OK;
}
#endif
static int pre_init(apr_pool_t *pconf, apr_pool_t *plog, apr_pool_t *ptemp) {
apr_status_t rv;
rv = ap_mutex_register(pconf, client_mutex_type, NULL, APR_LOCK_DEFAULT, 0);
if (rv == APR_SUCCESS) {
rv = ap_mutex_register(pconf, opaque_mutex_type, NULL, APR_LOCK_DEFAULT,
0);
}
if (rv != APR_SUCCESS) {
return rv;
}
return OK;
}
static int initialize_module(apr_pool_t *p, apr_pool_t *plog,
apr_pool_t *ptemp, server_rec *s) {
if (ap_state_query(AP_SQ_MAIN_STATE) == AP_SQ_MS_CREATE_PRE_CONFIG)
return OK;
if (initialize_secret(s) != APR_SUCCESS) {
return !OK;
}
#if APR_HAS_SHARED_MEMORY
if (initialize_tables(s, p) != OK) {
return !OK;
}
apr_pool_cleanup_register(p, NULL, cleanup_tables, apr_pool_cleanup_null);
#endif
return OK;
}
static void initialize_child(apr_pool_t *p, server_rec *s) {
apr_status_t sts;
if (!client_shm) {
return;
}
sts = apr_rmm_attach(&client_rmm,
NULL,
apr_shm_baseaddr_get(client_shm),
p);
if (sts != APR_SUCCESS) {
log_error_and_cleanup("failed to attach to rmm", sts, s);
return;
}
sts = apr_global_mutex_child_init(&client_lock,
apr_global_mutex_lockfile(client_lock),
p);
if (sts != APR_SUCCESS) {
log_error_and_cleanup("failed to create lock (client_lock)", sts, s);
return;
}
sts = apr_global_mutex_child_init(&opaque_lock,
apr_global_mutex_lockfile(opaque_lock),
p);
if (sts != APR_SUCCESS) {
log_error_and_cleanup("failed to create lock (opaque_lock)", sts, s);
return;
}
}
static void *create_digest_dir_config(apr_pool_t *p, char *dir) {
digest_config_rec *conf;
if (dir == NULL) {
return NULL;
}
conf = (digest_config_rec *) apr_pcalloc(p, sizeof(digest_config_rec));
if (conf) {
conf->qop_list = apr_array_make(p, 2, sizeof(char *));
conf->nonce_lifetime = DFLT_NONCE_LIFE;
conf->dir_name = apr_pstrdup(p, dir);
conf->algorithm = DFLT_ALGORITHM;
}
return conf;
}
static const char *set_realm(cmd_parms *cmd, void *config, const char *realm) {
digest_config_rec *conf = (digest_config_rec *) config;
conf->realm = realm;
apr_sha1_init(&conf->nonce_ctx);
apr_sha1_update_binary(&conf->nonce_ctx, secret, sizeof(secret));
apr_sha1_update_binary(&conf->nonce_ctx, (const unsigned char *) realm,
strlen(realm));
return DECLINE_CMD;
}
static const char *add_authn_provider(cmd_parms *cmd, void *config,
const char *arg) {
digest_config_rec *conf = (digest_config_rec*)config;
authn_provider_list *newp;
newp = apr_pcalloc(cmd->pool, sizeof(authn_provider_list));
newp->provider_name = arg;
newp->provider = ap_lookup_provider(AUTHN_PROVIDER_GROUP,
newp->provider_name,
AUTHN_PROVIDER_VERSION);
if (newp->provider == NULL) {
return apr_psprintf(cmd->pool,
"Unknown Authn provider: %s",
newp->provider_name);
}
if (!newp->provider->get_realm_hash) {
return apr_psprintf(cmd->pool,
"The '%s' Authn provider doesn't support "
"Digest Authentication", newp->provider_name);
}
if (!conf->providers) {
conf->providers = newp;
} else {
authn_provider_list *last = conf->providers;
while (last->next) {
last = last->next;
}
last->next = newp;
}
return NULL;
}
static const char *set_qop(cmd_parms *cmd, void *config, const char *op) {
digest_config_rec *conf = (digest_config_rec *) config;
if (!strcasecmp(op, "none")) {
apr_array_clear(conf->qop_list);
*(const char **)apr_array_push(conf->qop_list) = "none";
return NULL;
}
if (!strcasecmp(op, "auth-int")) {
return "AuthDigestQop auth-int is not implemented";
} else if (strcasecmp(op, "auth")) {
return apr_pstrcat(cmd->pool, "Unrecognized qop: ", op, NULL);
}
*(const char **)apr_array_push(conf->qop_list) = op;
return NULL;
}
static const char *set_nonce_lifetime(cmd_parms *cmd, void *config,
const char *t) {
char *endptr;
long lifetime;
lifetime = strtol(t, &endptr, 10);
if (endptr < (t+strlen(t)) && !apr_isspace(*endptr)) {
return apr_pstrcat(cmd->pool,
"Invalid time in AuthDigestNonceLifetime: ",
t, NULL);
}
((digest_config_rec *) config)->nonce_lifetime = apr_time_from_sec(lifetime);
return NULL;
}
static const char *set_nonce_format(cmd_parms *cmd, void *config,
const char *fmt) {
((digest_config_rec *) config)->nonce_format = fmt;
return "AuthDigestNonceFormat is not implemented (yet)";
}
static const char *set_nc_check(cmd_parms *cmd, void *config, int flag) {
#if !APR_HAS_SHARED_MEMORY
if (flag) {
return "AuthDigestNcCheck: ERROR: nonce-count checking "
"is not supported on platforms without shared-memory "
"support";
}
#endif
((digest_config_rec *) config)->check_nc = flag;
return NULL;
}
static const char *set_algorithm(cmd_parms *cmd, void *config, const char *alg) {
if (!strcasecmp(alg, "MD5-sess")) {
return "AuthDigestAlgorithm: ERROR: algorithm `MD5-sess' "
"is not fully implemented";
} else if (strcasecmp(alg, "MD5")) {
return apr_pstrcat(cmd->pool, "Invalid algorithm in AuthDigestAlgorithm: ", alg, NULL);
}
((digest_config_rec *) config)->algorithm = alg;
return NULL;
}
static const char *set_uri_list(cmd_parms *cmd, void *config, const char *uri) {
digest_config_rec *c = (digest_config_rec *) config;
if (c->uri_list) {
c->uri_list[strlen(c->uri_list)-1] = '\0';
c->uri_list = apr_pstrcat(cmd->pool, c->uri_list, " ", uri, "\"", NULL);
} else {
c->uri_list = apr_pstrcat(cmd->pool, ", domain=\"", uri, "\"", NULL);
}
return NULL;
}
static const char *set_shmem_size(cmd_parms *cmd, void *config,
const char *size_str) {
char *endptr;
long size, min;
size = strtol(size_str, &endptr, 10);
while (apr_isspace(*endptr)) endptr++;
if (*endptr == '\0' || *endptr == 'b' || *endptr == 'B') {
;
} else if (*endptr == 'k' || *endptr == 'K') {
size *= 1024;
} else if (*endptr == 'm' || *endptr == 'M') {
size *= 1048576;
} else {
return apr_pstrcat(cmd->pool, "Invalid size in AuthDigestShmemSize: ",
size_str, NULL);
}
min = sizeof(*client_list) + sizeof(client_entry*) + sizeof(client_entry);
if (size < min) {
return apr_psprintf(cmd->pool, "size in AuthDigestShmemSize too small: "
"%ld < %ld", size, min);
}
shmem_size = size;
num_buckets = (size - sizeof(*client_list)) /
(sizeof(client_entry*) + HASH_DEPTH * sizeof(client_entry));
if (num_buckets == 0) {
num_buckets = 1;
}
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, cmd->server, APLOGNO(01763)
"Set shmem-size: %" APR_SIZE_T_FMT ", num-buckets: %ld",
shmem_size, num_buckets);
return NULL;
}
static const command_rec digest_cmds[] = {
AP_INIT_TAKE1("AuthName", set_realm, NULL, OR_AUTHCFG,
"The authentication realm (e.g. \"Members Only\")"),
AP_INIT_ITERATE("AuthDigestProvider", add_authn_provider, NULL, OR_AUTHCFG,
"specify the auth providers for a directory or location"),
AP_INIT_ITERATE("AuthDigestQop", set_qop, NULL, OR_AUTHCFG,
"A list of quality-of-protection options"),
AP_INIT_TAKE1("AuthDigestNonceLifetime", set_nonce_lifetime, NULL, OR_AUTHCFG,
"Maximum lifetime of the server nonce (seconds)"),
AP_INIT_TAKE1("AuthDigestNonceFormat", set_nonce_format, NULL, OR_AUTHCFG,
"The format to use when generating the server nonce"),
AP_INIT_FLAG("AuthDigestNcCheck", set_nc_check, NULL, OR_AUTHCFG,
"Whether or not to check the nonce-count sent by the client"),
AP_INIT_TAKE1("AuthDigestAlgorithm", set_algorithm, NULL, OR_AUTHCFG,
"The algorithm used for the hash calculation"),
AP_INIT_ITERATE("AuthDigestDomain", set_uri_list, NULL, OR_AUTHCFG,
"A list of URI's which belong to the same protection space as the current URI"),
AP_INIT_TAKE1("AuthDigestShmemSize", set_shmem_size, NULL, RSRC_CONF,
"The amount of shared memory to allocate for keeping track of clients"),
{NULL}
};
static client_entry *get_client(unsigned long key, const request_rec *r) {
int bucket;
client_entry *entry, *prev = NULL;
if (!key || !client_shm) return NULL;
bucket = key % client_list->tbl_len;
entry = client_list->table[bucket];
apr_global_mutex_lock(client_lock);
while (entry && key != entry->key) {
prev = entry;
entry = entry->next;
}
if (entry && prev) {
prev->next = entry->next;
entry->next = client_list->table[bucket];
client_list->table[bucket] = entry;
}
apr_global_mutex_unlock(client_lock);
if (entry) {
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(01764)
"get_client(): client %lu found", key);
} else {
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(01765)
"get_client(): client %lu not found", key);
}
return entry;
}
static long gc(server_rec *s) {
client_entry *entry, *prev;
unsigned long num_removed = 0, idx;
for (idx = 0; idx < client_list->tbl_len; idx++) {
entry = client_list->table[idx];
prev = NULL;
if (!entry) {
continue;
}
while (entry->next) {
prev = entry;
entry = entry->next;
}
if (prev) {
prev->next = NULL;
} else {
client_list->table[idx] = NULL;
}
if (entry) {
apr_status_t err;
err = rmm_free(client_rmm, entry);
num_removed++;
if (err) {
ap_log_error(APLOG_MARK, APLOG_ERR, err, s, APLOGNO(10007)
"Failed to free auth_digest client allocation");
}
}
}
client_list->num_entries -= num_removed;
client_list->num_removed += num_removed;
return num_removed;
}
static client_entry *add_client(unsigned long key, client_entry *info,
server_rec *s) {
int bucket;
client_entry *entry;
if (!key || !client_shm) {
return NULL;
}
bucket = key % client_list->tbl_len;
apr_global_mutex_lock(client_lock);
entry = rmm_malloc(client_rmm, sizeof(client_entry));
if (!entry) {
long num_removed = gc(s);
ap_log_error(APLOG_MARK, APLOG_INFO, 0, s, APLOGNO(01766)
"gc'd %ld client entries. Total new clients: "
"%ld; Total removed clients: %ld; Total renewed clients: "
"%ld", num_removed,
client_list->num_created - client_list->num_renewed,
client_list->num_removed, client_list->num_renewed);
entry = rmm_malloc(client_rmm, sizeof(client_entry));
if (!entry) {
ap_log_error(APLOG_MARK, APLOG_ERR, 0, s, APLOGNO(01767)
"unable to allocate new auth_digest client");
apr_global_mutex_unlock(client_lock);
return NULL;
}
}
memcpy(entry, info, sizeof(client_entry));
entry->key = key;
entry->next = client_list->table[bucket];
client_list->table[bucket] = entry;
client_list->num_created++;
client_list->num_entries++;
apr_global_mutex_unlock(client_lock);
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s, APLOGNO(01768)
"allocated new client %lu", key);
return entry;
}
static int get_digest_rec(request_rec *r, digest_header_rec *resp) {
const char *auth_line;
apr_size_t l;
int vk = 0, vv = 0;
char *key, *value;
auth_line = apr_table_get(r->headers_in,
(PROXYREQ_PROXY == r->proxyreq)
? "Proxy-Authorization"
: "Authorization");
if (!auth_line) {
resp->auth_hdr_sts = NO_HEADER;
return !OK;
}
resp->scheme = ap_getword_white(r->pool, &auth_line);
if (strcasecmp(resp->scheme, "Digest")) {
resp->auth_hdr_sts = NOT_DIGEST;
return !OK;
}
l = strlen(auth_line);
key = apr_palloc(r->pool, l+1);
value = apr_palloc(r->pool, l+1);
while (auth_line[0] != '\0') {
while (apr_isspace(auth_line[0])) {
auth_line++;
}
vk = 0;
while (auth_line[0] != '=' && auth_line[0] != ','
&& auth_line[0] != '\0' && !apr_isspace(auth_line[0])) {
key[vk++] = *auth_line++;
}
key[vk] = '\0';
while (apr_isspace(auth_line[0])) {
auth_line++;
}
vv = 0;
if (auth_line[0] == '=') {
auth_line++;
while (apr_isspace(auth_line[0])) {
auth_line++;
}
if (auth_line[0] == '\"') {
auth_line++;
while (auth_line[0] != '\"' && auth_line[0] != '\0') {
if (auth_line[0] == '\\' && auth_line[1] != '\0') {
auth_line++;
}
value[vv++] = *auth_line++;
}
if (auth_line[0] != '\0') {
auth_line++;
}
} else {
while (auth_line[0] != ',' && auth_line[0] != '\0'
&& !apr_isspace(auth_line[0])) {
value[vv++] = *auth_line++;
}
}
}
value[vv] = '\0';
while (auth_line[0] != ',' && auth_line[0] != '\0') {
auth_line++;
}
if (auth_line[0] != '\0') {
auth_line++;
}
if (!strcasecmp(key, "username"))
resp->username = apr_pstrdup(r->pool, value);
else if (!strcasecmp(key, "realm"))
resp->realm = apr_pstrdup(r->pool, value);
else if (!strcasecmp(key, "nonce"))
resp->nonce = apr_pstrdup(r->pool, value);
else if (!strcasecmp(key, "uri"))
resp->uri = apr_pstrdup(r->pool, value);
else if (!strcasecmp(key, "response"))
resp->digest = apr_pstrdup(r->pool, value);
else if (!strcasecmp(key, "algorithm"))
resp->algorithm = apr_pstrdup(r->pool, value);
else if (!strcasecmp(key, "cnonce"))
resp->cnonce = apr_pstrdup(r->pool, value);
else if (!strcasecmp(key, "opaque"))
resp->opaque = apr_pstrdup(r->pool, value);
else if (!strcasecmp(key, "qop"))
resp->message_qop = apr_pstrdup(r->pool, value);
else if (!strcasecmp(key, "nc"))
resp->nonce_count = apr_pstrdup(r->pool, value);
}
if (!resp->username || !resp->realm || !resp->nonce || !resp->uri
|| !resp->digest
|| (resp->message_qop && (!resp->cnonce || !resp->nonce_count))) {
resp->auth_hdr_sts = INVALID;
return !OK;
}
if (resp->opaque) {
resp->opaque_num = (unsigned long) strtol(resp->opaque, NULL, 16);
}
resp->auth_hdr_sts = VALID;
return OK;
}
static int parse_hdr_and_update_nc(request_rec *r) {
digest_header_rec *resp;
int res;
if (!ap_is_initial_req(r)) {
return DECLINED;
}
resp = apr_pcalloc(r->pool, sizeof(digest_header_rec));
resp->raw_request_uri = r->unparsed_uri;
resp->psd_request_uri = &r->parsed_uri;
resp->needed_auth = 0;
resp->method = r->method;
ap_set_module_config(r->request_config, &auth_digest_module, resp);
res = get_digest_rec(r, resp);
resp->client = get_client(resp->opaque_num, r);
if (res == OK && resp->client) {
resp->client->nonce_count++;
}
return DECLINED;
}
static void gen_nonce_hash(char *hash, const char *timestr, const char *opaque,
const server_rec *server,
const digest_config_rec *conf) {
unsigned char sha1[APR_SHA1_DIGESTSIZE];
apr_sha1_ctx_t ctx;
memcpy(&ctx, &conf->nonce_ctx, sizeof(ctx));
apr_sha1_update_binary(&ctx, (const unsigned char *) timestr, strlen(timestr));
if (opaque) {
apr_sha1_update_binary(&ctx, (const unsigned char *) opaque,
strlen(opaque));
}
apr_sha1_final(sha1, &ctx);
ap_bin2hex(sha1, APR_SHA1_DIGESTSIZE, hash);
}
static const char *gen_nonce(apr_pool_t *p, apr_time_t now, const char *opaque,
const server_rec *server,
const digest_config_rec *conf) {
char *nonce = apr_palloc(p, NONCE_LEN+1);
time_rec t;
if (conf->nonce_lifetime != 0) {
t.time = now;
} else if (otn_counter) {
t.time = (*otn_counter)++;
} else {
t.time = 42;
}
apr_base64_encode_binary(nonce, t.arr, sizeof(t.arr));
gen_nonce_hash(nonce+NONCE_TIME_LEN, nonce, opaque, server, conf);
return nonce;
}
static client_entry *gen_client(const request_rec *r) {
unsigned long op;
client_entry new_entry = { 0, NULL, 0, "", "" }, *entry;
if (!opaque_cntr) {
return NULL;
}
apr_global_mutex_lock(opaque_lock);
op = (*opaque_cntr)++;
apr_global_mutex_unlock(opaque_lock);
if (!(entry = add_client(op, &new_entry, r->server))) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(01769)
"failed to allocate client entry - ignoring client");
return NULL;
}
return entry;
}
static const char *get_userpw_hash(const request_rec *r,
const digest_header_rec *resp,
const digest_config_rec *conf) {
return ap_md5(r->pool,
(unsigned char *) apr_pstrcat(r->pool, conf->ha1, ":", resp->nonce,
":", resp->cnonce, NULL));
}
static const char *get_session_HA1(const request_rec *r,
digest_header_rec *resp,
const digest_config_rec *conf,
int generate) {
const char *ha1 = NULL;
if (resp->opaque && resp->client && resp->client->ha1[0]) {
return resp->client->ha1;
} else if (!generate) {
return NULL;
}
if (!resp->client) {
resp->client = gen_client(r);
}
if (resp->client) {
ha1 = get_userpw_hash(r, resp, conf);
if (ha1) {
memcpy(resp->client->ha1, ha1, sizeof(resp->client->ha1));
}
}
return ha1;
}
static void clear_session(const digest_header_rec *resp) {
if (resp->client) {
resp->client->ha1[0] = '\0';
}
}
static const char *ltox(apr_pool_t *p, unsigned long num) {
if (num != 0) {
return apr_psprintf(p, "%lx", num);
} else {
return "";
}
}
static void note_digest_auth_failure(request_rec *r,
const digest_config_rec *conf,
digest_header_rec *resp, int stale) {
const char *qop, *opaque, *opaque_param, *domain, *nonce;
if (apr_is_empty_array(conf->qop_list)) {
qop = ", qop=\"auth\"";
} else if (!strcasecmp(*(const char **)(conf->qop_list->elts), "none")) {
qop = "";
} else {
qop = apr_pstrcat(r->pool, ", qop=\"",
apr_array_pstrcat(r->pool, conf->qop_list, ','),
"\"",
NULL);
}
if (resp->opaque == NULL) {
if ((conf->check_nc || conf->nonce_lifetime == 0
|| !strcasecmp(conf->algorithm, "MD5-sess"))
&& (resp->client = gen_client(r)) != NULL) {
opaque = ltox(r->pool, resp->client->key);
} else {
opaque = "";
}
} else if (resp->client == NULL) {
resp->client = gen_client(r);
if (resp->client != NULL) {
opaque = ltox(r->pool, resp->client->key);
stale = 1;
client_list->num_renewed++;
} else {
opaque = "";
}
} else {
opaque = resp->opaque;
resp->client->nonce_count = 0;
}
if (opaque[0]) {
opaque_param = apr_pstrcat(r->pool, ", opaque=\"", opaque, "\"", NULL);
} else {
opaque_param = NULL;
}
nonce = gen_nonce(r->pool, r->request_time, opaque, r->server, conf);
if (resp->client && conf->nonce_lifetime == 0) {
memcpy(resp->client->last_nonce, nonce, NONCE_LEN+1);
}
if (!strcasecmp(conf->algorithm, "MD5-sess")) {
clear_session(resp);
}
if (r->proxyreq || !conf->uri_list) {
domain = NULL;
} else {
domain = conf->uri_list;
}
apr_table_mergen(r->err_headers_out,
(PROXYREQ_PROXY == r->proxyreq)
? "Proxy-Authenticate" : "WWW-Authenticate",
apr_psprintf(r->pool, "Digest realm=\"%s\", "
"nonce=\"%s\", algorithm=%s%s%s%s%s",
ap_auth_name(r), nonce, conf->algorithm,
opaque_param ? opaque_param : "",
domain ? domain : "",
stale ? ", stale=true" : "", qop));
}
static int hook_note_digest_auth_failure(request_rec *r, const char *auth_type) {
request_rec *mainreq;
digest_header_rec *resp;
digest_config_rec *conf;
if (strcasecmp(auth_type, "Digest"))
return DECLINED;
mainreq = r;
while (mainreq->main != NULL) {
mainreq = mainreq->main;
}
while (mainreq->prev != NULL) {
mainreq = mainreq->prev;
}
resp = (digest_header_rec *) ap_get_module_config(mainreq->request_config,
&auth_digest_module);
resp->needed_auth = 1;
conf = (digest_config_rec *) ap_get_module_config(r->per_dir_config,
&auth_digest_module);
note_digest_auth_failure(r, conf, resp, 0);
return OK;
}
static authn_status get_hash(request_rec *r, const char *user,
digest_config_rec *conf) {
authn_status auth_result;
char *password;
authn_provider_list *current_provider;
current_provider = conf->providers;
do {
const authn_provider *provider;
if (!current_provider) {
provider = ap_lookup_provider(AUTHN_PROVIDER_GROUP,
AUTHN_DEFAULT_PROVIDER,
AUTHN_PROVIDER_VERSION);
if (!provider || !provider->get_realm_hash) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(01770)
"No Authn provider configured");
auth_result = AUTH_GENERAL_ERROR;
break;
}
apr_table_setn(r->notes, AUTHN_PROVIDER_NAME_NOTE, AUTHN_DEFAULT_PROVIDER);
} else {
provider = current_provider->provider;
apr_table_setn(r->notes, AUTHN_PROVIDER_NAME_NOTE, current_provider->provider_name);
}
auth_result = provider->get_realm_hash(r, user, conf->realm,
&password);
apr_table_unset(r->notes, AUTHN_PROVIDER_NAME_NOTE);
if (auth_result != AUTH_USER_NOT_FOUND) {
break;
}
if (!conf->providers) {
break;
}
current_provider = current_provider->next;
} while (current_provider);
if (auth_result == AUTH_USER_FOUND) {
conf->ha1 = password;
}
return auth_result;
}
static int check_nc(const request_rec *r, const digest_header_rec *resp,
const digest_config_rec *conf) {
unsigned long nc;
const char *snc = resp->nonce_count;
char *endptr;
if (conf->check_nc && !client_shm) {
ap_log_rerror(APLOG_MARK, APLOG_WARNING, 0, r, APLOGNO(01771)
"cannot check nonce count without shared memory");
return OK;
}
if (!conf->check_nc || !client_shm) {
return OK;
}
if (!apr_is_empty_array(conf->qop_list) &&
!strcasecmp(*(const char **)(conf->qop_list->elts), "none")) {
if (snc != NULL) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(01772)
"invalid nc %s received - no nonce count allowed when qop=none",
snc);
return !OK;
}
return OK;
}
nc = strtol(snc, &endptr, 16);
if (endptr < (snc+strlen(snc)) && !apr_isspace(*endptr)) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(01773)
"invalid nc %s received - not a number", snc);
return !OK;
}
if (!resp->client) {
return !OK;
}
if (nc != resp->client->nonce_count) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(01774)
"Warning, possible replay attack: nonce-count "
"check failed: %lu != %lu", nc,
resp->client->nonce_count);
return !OK;
}
return OK;
}
static int check_nonce(request_rec *r, digest_header_rec *resp,
const digest_config_rec *conf) {
apr_time_t dt;
time_rec nonce_time;
char tmp, hash[NONCE_HASH_LEN+1];
if (strlen(resp->nonce) != NONCE_LEN) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(01775)
"invalid nonce %s received - length is not %d",
resp->nonce, NONCE_LEN);
note_digest_auth_failure(r, conf, resp, 1);
return HTTP_UNAUTHORIZED;
}
tmp = resp->nonce[NONCE_TIME_LEN];
resp->nonce[NONCE_TIME_LEN] = '\0';
apr_base64_decode_binary(nonce_time.arr, resp->nonce);
gen_nonce_hash(hash, resp->nonce, resp->opaque, r->server, conf);
resp->nonce[NONCE_TIME_LEN] = tmp;
resp->nonce_time = nonce_time.time;
if (strcmp(hash, resp->nonce+NONCE_TIME_LEN)) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(01776)
"invalid nonce %s received - hash is not %s",
resp->nonce, hash);
note_digest_auth_failure(r, conf, resp, 1);
return HTTP_UNAUTHORIZED;
}
dt = r->request_time - nonce_time.time;
if (conf->nonce_lifetime > 0 && dt < 0) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(01777)
"invalid nonce %s received - user attempted "
"time travel", resp->nonce);
note_digest_auth_failure(r, conf, resp, 1);
return HTTP_UNAUTHORIZED;
}
if (conf->nonce_lifetime > 0) {
if (dt > conf->nonce_lifetime) {
ap_log_rerror(APLOG_MARK, APLOG_INFO, 0,r, APLOGNO(01778)
"user %s: nonce expired (%.2f seconds old "
"- max lifetime %.2f) - sending new nonce",
r->user, (double)apr_time_sec(dt),
(double)apr_time_sec(conf->nonce_lifetime));
note_digest_auth_failure(r, conf, resp, 1);
return HTTP_UNAUTHORIZED;
}
} else if (conf->nonce_lifetime == 0 && resp->client) {
if (memcmp(resp->client->last_nonce, resp->nonce, NONCE_LEN)) {
ap_log_rerror(APLOG_MARK, APLOG_INFO, 0, r, APLOGNO(01779)
"user %s: one-time-nonce mismatch - sending "
"new nonce", r->user);
note_digest_auth_failure(r, conf, resp, 1);
return HTTP_UNAUTHORIZED;
}
}
return OK;
}
static const char *old_digest(const request_rec *r,
const digest_header_rec *resp, const char *ha1) {
const char *ha2;
ha2 = ap_md5(r->pool, (unsigned char *)apr_pstrcat(r->pool, resp->method, ":",
resp->uri, NULL));
return ap_md5(r->pool,
(unsigned char *)apr_pstrcat(r->pool, ha1, ":", resp->nonce,
":", ha2, NULL));
}
static const char *new_digest(const request_rec *r,
digest_header_rec *resp,
const digest_config_rec *conf) {
const char *ha1, *ha2, *a2;
if (resp->algorithm && !strcasecmp(resp->algorithm, "MD5-sess")) {
ha1 = get_session_HA1(r, resp, conf, 1);
if (!ha1) {
return NULL;
}
} else {
ha1 = conf->ha1;
}
if (resp->message_qop && !strcasecmp(resp->message_qop, "auth-int")) {
a2 = apr_pstrcat(r->pool, resp->method, ":", resp->uri, ":",
ap_md5(r->pool, (const unsigned char*) ""), NULL);
} else {
a2 = apr_pstrcat(r->pool, resp->method, ":", resp->uri, NULL);
}
ha2 = ap_md5(r->pool, (const unsigned char *)a2);
return ap_md5(r->pool,
(unsigned char *)apr_pstrcat(r->pool, ha1, ":", resp->nonce,
":", resp->nonce_count, ":",
resp->cnonce, ":",
resp->message_qop, ":", ha2,
NULL));
}
static void copy_uri_components(apr_uri_t *dst,
apr_uri_t *src, request_rec *r) {
if (src->scheme && src->scheme[0] != '\0') {
dst->scheme = src->scheme;
} else {
dst->scheme = (char *) "http";
}
if (src->hostname && src->hostname[0] != '\0') {
dst->hostname = apr_pstrdup(r->pool, src->hostname);
ap_unescape_url(dst->hostname);
} else {
dst->hostname = (char *) ap_get_server_name(r);
}
if (src->port_str && src->port_str[0] != '\0') {
dst->port = src->port;
} else {
dst->port = ap_get_server_port(r);
}
if (src->path && src->path[0] != '\0') {
dst->path = apr_pstrdup(r->pool, src->path);
ap_unescape_url(dst->path);
} else {
dst->path = src->path;
}
if (src->query && src->query[0] != '\0') {
dst->query = apr_pstrdup(r->pool, src->query);
ap_unescape_url(dst->query);
} else {
dst->query = src->query;
}
dst->hostinfo = src->hostinfo;
}
static int authenticate_digest_user(request_rec *r) {
digest_config_rec *conf;
digest_header_rec *resp;
request_rec *mainreq;
const char *t;
int res;
authn_status return_code;
if (!(t = ap_auth_type(r)) || strcasecmp(t, "Digest")) {
return DECLINED;
}
if (!ap_auth_name(r)) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(01780)
"need AuthName: %s", r->uri);
return HTTP_INTERNAL_SERVER_ERROR;
}
mainreq = r;
while (mainreq->main != NULL) {
mainreq = mainreq->main;
}
while (mainreq->prev != NULL) {
mainreq = mainreq->prev;
}
resp = (digest_header_rec *) ap_get_module_config(mainreq->request_config,
&auth_digest_module);
resp->needed_auth = 1;
conf = (digest_config_rec *) ap_get_module_config(r->per_dir_config,
&auth_digest_module);
if (resp->auth_hdr_sts != VALID) {
if (resp->auth_hdr_sts == NOT_DIGEST) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(01781)
"client used wrong authentication scheme `%s': %s",
resp->scheme, r->uri);
} else if (resp->auth_hdr_sts == INVALID) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(01782)
"missing user, realm, nonce, uri, digest, "
"cnonce, or nonce_count in authorization header: %s",
r->uri);
}
note_digest_auth_failure(r, conf, resp, 0);
return HTTP_UNAUTHORIZED;
}
r->user = (char *) resp->username;
r->ap_auth_type = (char *) "Digest";
if (strcmp(resp->uri, resp->raw_request_uri)) {
apr_uri_t r_uri, d_uri;
copy_uri_components(&r_uri, resp->psd_request_uri, r);
if (apr_uri_parse(r->pool, resp->uri, &d_uri) != APR_SUCCESS) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(01783)
"invalid uri <%s> in Authorization header",
resp->uri);
return HTTP_BAD_REQUEST;
}
if (d_uri.hostname) {
ap_unescape_url(d_uri.hostname);
}
if (d_uri.path) {
ap_unescape_url(d_uri.path);
}
if (d_uri.query) {
ap_unescape_url(d_uri.query);
} else if (r_uri.query) {
if (apr_table_get(r->subprocess_env,
"AuthDigestEnableQueryStringHack")) {
ap_log_rerror(APLOG_MARK, APLOG_INFO, 0, r, APLOGNO(01784)
"applying AuthDigestEnableQueryStringHack "
"to uri <%s>", resp->raw_request_uri);
d_uri.query = r_uri.query;
}
}
if (r->method_number == M_CONNECT) {
if (!r_uri.hostinfo || strcmp(resp->uri, r_uri.hostinfo)) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(01785)
"uri mismatch - <%s> does not match "
"request-uri <%s>", resp->uri, r_uri.hostinfo);
return HTTP_BAD_REQUEST;
}
} else if (
(d_uri.hostname && d_uri.hostname[0] != '\0'
&& strcasecmp(d_uri.hostname, r_uri.hostname))
|| (d_uri.port_str && d_uri.port != r_uri.port)
|| (d_uri.hostname && d_uri.hostname[0] != '\0'
&& !d_uri.port_str && r_uri.port != ap_default_port(r))
|| (d_uri.path != r_uri.path
&& (!d_uri.path || !r_uri.path
|| strcmp(d_uri.path, r_uri.path))
&& !(d_uri.path && !r_uri.path && resp->psd_request_uri->hostname
&& d_uri.path[0] == '*' && d_uri.path[1] == '\0'))
|| (d_uri.query != r_uri.query
&& (!d_uri.query || !r_uri.query
|| strcmp(d_uri.query, r_uri.query)))
) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(01786)
"uri mismatch - <%s> does not match "
"request-uri <%s>", resp->uri, resp->raw_request_uri);
return HTTP_BAD_REQUEST;
}
}
if (resp->opaque && resp->opaque_num == 0) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(01787)
"received invalid opaque - got `%s'",
resp->opaque);
note_digest_auth_failure(r, conf, resp, 0);
return HTTP_UNAUTHORIZED;
}
if (!conf->realm) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(02533)
"realm mismatch - got `%s' but no realm specified",
resp->realm);
note_digest_auth_failure(r, conf, resp, 0);
return HTTP_UNAUTHORIZED;
}
if (!resp->realm || strcmp(resp->realm, conf->realm)) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(01788)
"realm mismatch - got `%s' but expected `%s'",
resp->realm, conf->realm);
note_digest_auth_failure(r, conf, resp, 0);
return HTTP_UNAUTHORIZED;
}
if (resp->algorithm != NULL
&& strcasecmp(resp->algorithm, "MD5")
&& strcasecmp(resp->algorithm, "MD5-sess")) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(01789)
"unknown algorithm `%s' received: %s",
resp->algorithm, r->uri);
note_digest_auth_failure(r, conf, resp, 0);
return HTTP_UNAUTHORIZED;
}
return_code = get_hash(r, r->user, conf);
if (return_code == AUTH_USER_NOT_FOUND) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(01790)
"user `%s' in realm `%s' not found: %s",
r->user, conf->realm, r->uri);
note_digest_auth_failure(r, conf, resp, 0);
return HTTP_UNAUTHORIZED;
} else if (return_code == AUTH_USER_FOUND) {
} else if (return_code == AUTH_DENIED) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(01791)
"user `%s' in realm `%s' denied by provider: %s",
r->user, conf->realm, r->uri);
note_digest_auth_failure(r, conf, resp, 0);
return HTTP_UNAUTHORIZED;
} else {
return HTTP_INTERNAL_SERVER_ERROR;
}
if (resp->message_qop == NULL) {
if (strcmp(resp->digest, old_digest(r, resp, conf->ha1))) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(01792)
"user %s: password mismatch: %s", r->user,
r->uri);
note_digest_auth_failure(r, conf, resp, 0);
return HTTP_UNAUTHORIZED;
}
} else {
const char *exp_digest;
int match = 0, idx;
const char **tmp = (const char **)(conf->qop_list->elts);
for (idx = 0; idx < conf->qop_list->nelts; idx++) {
if (!strcasecmp(*tmp, resp->message_qop)) {
match = 1;
break;
}
++tmp;
}
if (!match
&& !(apr_is_empty_array(conf->qop_list)
&& !strcasecmp(resp->message_qop, "auth"))) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(01793)
"invalid qop `%s' received: %s",
resp->message_qop, r->uri);
note_digest_auth_failure(r, conf, resp, 0);
return HTTP_UNAUTHORIZED;
}
exp_digest = new_digest(r, resp, conf);
if (!exp_digest) {
return HTTP_INTERNAL_SERVER_ERROR;
}
if (strcmp(resp->digest, exp_digest)) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(01794)
"user %s: password mismatch: %s", r->user,
r->uri);
note_digest_auth_failure(r, conf, resp, 0);
return HTTP_UNAUTHORIZED;
}
}
if (check_nc(r, resp, conf) != OK) {
note_digest_auth_failure(r, conf, resp, 0);
return HTTP_UNAUTHORIZED;
}
if ((res = check_nonce(r, resp, conf))) {
return res;
}
return OK;
}
static int add_auth_info(request_rec *r) {
const digest_config_rec *conf =
(digest_config_rec *) ap_get_module_config(r->per_dir_config,
&auth_digest_module);
digest_header_rec *resp =
(digest_header_rec *) ap_get_module_config(r->request_config,
&auth_digest_module);
const char *ai = NULL, *nextnonce = "";
if (resp == NULL || !resp->needed_auth || conf == NULL) {
return OK;
}
if (conf->nonce_lifetime > 0) {
if ((r->request_time - resp->nonce_time) > (conf->nonce_lifetime-NEXTNONCE_DELTA)) {
nextnonce = apr_pstrcat(r->pool, ", nextnonce=\"",
gen_nonce(r->pool, r->request_time,
resp->opaque, r->server, conf),
"\"", NULL);
if (resp->client)
resp->client->nonce_count = 0;
}
} else if (conf->nonce_lifetime == 0 && resp->client) {
const char *nonce = gen_nonce(r->pool, 0, resp->opaque, r->server,
conf);
nextnonce = apr_pstrcat(r->pool, ", nextnonce=\"", nonce, "\"", NULL);
memcpy(resp->client->last_nonce, nonce, NONCE_LEN+1);
}
if (!apr_is_empty_array(conf->qop_list) &&
!strcasecmp(*(const char **)(conf->qop_list->elts), "none")
&& resp->message_qop == NULL) {
ai = nextnonce;
} else {
const char *resp_dig, *ha1, *a2, *ha2;
if (resp->algorithm && !strcasecmp(resp->algorithm, "MD5-sess")) {
ha1 = get_session_HA1(r, resp, conf, 0);
if (!ha1) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(01795)
"internal error: couldn't find session "
"info for user %s", resp->username);
return !OK;
}
} else {
ha1 = conf->ha1;
}
if (resp->message_qop && !strcasecmp(resp->message_qop, "auth-int")) {
a2 = apr_pstrcat(r->pool, ":", resp->uri, ":",
ap_md5(r->pool,(const unsigned char *) ""), NULL);
} else {
a2 = apr_pstrcat(r->pool, ":", resp->uri, NULL);
}
ha2 = ap_md5(r->pool, (const unsigned char *)a2);
resp_dig = ap_md5(r->pool,
(unsigned char *)apr_pstrcat(r->pool, ha1, ":",
resp->nonce, ":",
resp->nonce_count, ":",
resp->cnonce, ":",
resp->message_qop ?
resp->message_qop : "",
":", ha2, NULL));
ai = apr_pstrcat(r->pool,
"rspauth=\"", resp_dig, "\"",
nextnonce,
resp->cnonce ? ", cnonce=\"" : "",
resp->cnonce
? ap_escape_quotes(r->pool, resp->cnonce)
: "",
resp->cnonce ? "\"" : "",
resp->nonce_count ? ", nc=" : "",
resp->nonce_count ? resp->nonce_count : "",
resp->message_qop ? ", qop=" : "",
resp->message_qop ? resp->message_qop : "",
NULL);
}
if (ai && ai[0]) {
apr_table_mergen(r->headers_out,
(PROXYREQ_PROXY == r->proxyreq)
? "Proxy-Authentication-Info"
: "Authentication-Info",
ai);
}
return OK;
}
static void register_hooks(apr_pool_t *p) {
static const char * const cfgPost[]= { "http_core.c", NULL };
static const char * const parsePre[]= { "mod_proxy.c", NULL };
ap_hook_pre_config(pre_init, NULL, NULL, APR_HOOK_MIDDLE);
ap_hook_post_config(initialize_module, NULL, cfgPost, APR_HOOK_MIDDLE);
ap_hook_child_init(initialize_child, NULL, NULL, APR_HOOK_MIDDLE);
ap_hook_post_read_request(parse_hdr_and_update_nc, parsePre, NULL, APR_HOOK_MIDDLE);
ap_hook_check_authn(authenticate_digest_user, NULL, NULL, APR_HOOK_MIDDLE,
AP_AUTH_INTERNAL_PER_CONF);
ap_hook_fixups(add_auth_info, NULL, NULL, APR_HOOK_MIDDLE);
ap_hook_note_auth_failure(hook_note_digest_auth_failure, NULL, NULL,
APR_HOOK_MIDDLE);
}
AP_DECLARE_MODULE(auth_digest) = {
STANDARD20_MODULE_STUFF,
create_digest_dir_config,
NULL,
NULL,
NULL,
digest_cmds,
register_hooks
};