#define APR_WANT_BYTEFUNC
#include "apr_want.h"
#include "apr_general.h"
#include "apr_network_io.h"
#include "httpd.h"
#include "http_config.h"
#include "http_log.h"
#include "http_protocol.h"
#define ROOT_SIZE 10
typedef struct {
unsigned int stamp;
char root[ROOT_SIZE];
unsigned short counter;
unsigned int thread_index;
} unique_id_rec;
static unique_id_rec cur_unique_id;
#define UNIQUE_ID_REC_MAX 4
static unsigned short unique_id_rec_offset[UNIQUE_ID_REC_MAX],
unique_id_rec_size[UNIQUE_ID_REC_MAX],
unique_id_rec_total_size,
unique_id_rec_size_uu;
static int unique_id_global_init(apr_pool_t *p, apr_pool_t *plog, apr_pool_t *ptemp, server_rec *main_server) {
unique_id_rec_offset[0] = APR_OFFSETOF(unique_id_rec, stamp);
unique_id_rec_size[0] = sizeof(cur_unique_id.stamp);
unique_id_rec_offset[1] = APR_OFFSETOF(unique_id_rec, root);
unique_id_rec_size[1] = sizeof(cur_unique_id.root);
unique_id_rec_offset[2] = APR_OFFSETOF(unique_id_rec, counter);
unique_id_rec_size[2] = sizeof(cur_unique_id.counter);
unique_id_rec_offset[3] = APR_OFFSETOF(unique_id_rec, thread_index);
unique_id_rec_size[3] = sizeof(cur_unique_id.thread_index);
unique_id_rec_total_size = unique_id_rec_size[0] + unique_id_rec_size[1] +
unique_id_rec_size[2] + unique_id_rec_size[3];
unique_id_rec_size_uu = (unique_id_rec_total_size*8+5)/6;
return OK;
}
static void unique_id_child_init(apr_pool_t *p, server_rec *s) {
ap_random_insecure_bytes(&cur_unique_id.root,
sizeof(cur_unique_id.root));
ap_random_insecure_bytes(&cur_unique_id.counter,
sizeof(cur_unique_id.counter));
}
static const char uuencoder[64] = {
'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '@', '-',
};
static const char *gen_unique_id(const request_rec *r) {
char *str;
unique_id_rec new_unique_id;
struct {
unique_id_rec foo;
unsigned char pad[2];
} paddedbuf;
unsigned char *x,*y;
unsigned short counter;
int i,j,k;
memcpy(&new_unique_id.root, &cur_unique_id.root, ROOT_SIZE);
new_unique_id.counter = cur_unique_id.counter;
new_unique_id.stamp = htonl((unsigned int)apr_time_sec(r->request_time));
new_unique_id.thread_index = htonl((unsigned int)r->connection->id);
x = (unsigned char *) &paddedbuf;
k = 0;
for (i = 0; i < UNIQUE_ID_REC_MAX; i++) {
y = ((unsigned char *) &new_unique_id) + unique_id_rec_offset[i];
for (j = 0; j < unique_id_rec_size[i]; j++, k++) {
x[k] = y[j];
}
}
x[k++] = '\0';
x[k++] = '\0';
str = (char *)apr_palloc(r->pool, unique_id_rec_size_uu + 1);
k = 0;
for (i = 0; i < unique_id_rec_total_size; i += 3) {
y = x + i;
str[k++] = uuencoder[y[0] >> 2];
str[k++] = uuencoder[((y[0] & 0x03) << 4) | ((y[1] & 0xf0) >> 4)];
if (k == unique_id_rec_size_uu) break;
str[k++] = uuencoder[((y[1] & 0x0f) << 2) | ((y[2] & 0xc0) >> 6)];
if (k == unique_id_rec_size_uu) break;
str[k++] = uuencoder[y[2] & 0x3f];
}
str[k++] = '\0';
counter = ntohs(new_unique_id.counter) + 1;
cur_unique_id.counter = htons(counter);
return str;
}
static int generate_log_id(const conn_rec *c, const request_rec *r,
const char **id) {
if (r == NULL)
return DECLINED;
*id = apr_table_get(r->subprocess_env, "UNIQUE_ID");
if (!*id)
*id = gen_unique_id(r);
return OK;
}
static int set_unique_id(request_rec *r) {
const char *id = NULL;
if (r->prev) {
id = apr_table_get(r->subprocess_env, "REDIRECT_UNIQUE_ID");
}
if (!id) {
id = r->log_id;
}
if (!id) {
id = gen_unique_id(r);
}
apr_table_setn(r->subprocess_env, "UNIQUE_ID", id);
return DECLINED;
}
static void register_hooks(apr_pool_t *p) {
ap_hook_post_config(unique_id_global_init, NULL, NULL, APR_HOOK_MIDDLE);
ap_hook_child_init(unique_id_child_init, NULL, NULL, APR_HOOK_MIDDLE);
ap_hook_post_read_request(set_unique_id, NULL, NULL, APR_HOOK_MIDDLE);
ap_hook_generate_log_id(generate_log_id, NULL, NULL, APR_HOOK_MIDDLE);
}
AP_DECLARE_MODULE(unique_id) = {
STANDARD20_MODULE_STUFF,
NULL,
NULL,
NULL,
NULL,
NULL,
register_hooks
};
